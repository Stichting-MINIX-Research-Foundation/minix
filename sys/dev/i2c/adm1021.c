/*	$NetBSD: adm1021.c,v 1.9 2015/09/27 13:02:21 phx Exp $ */
/*	$OpenBSD: adm1021.c,v 1.27 2007/06/24 05:34:35 dlg Exp $	*/

/*
 * Copyright (c) 2005 Theo de Raadt
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: adm1021.c,v 1.9 2015/09/27 13:02:21 phx Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <dev/sysmon/sysmonvar.h>

#include <dev/i2c/i2cvar.h>


/* ADM 1021 registers */
#define ADM1021_INT_TEMP	0x00
#define ADM1021_EXT_TEMP	0x01
#define ADM1021_STATUS		0x02
#define ADM1021_STATUS_INVAL	0x7f
#define ADM1021_STATUS_NOEXT	0x40
#define ADM1021_CONFIG_READ	0x03
#define ADM1021_CONFIG_WRITE	0x09
#define ADM1021_CONFIG_RUN	0x40
#define ADM1021_COMPANY		0xfe	/* contains 0x41 */
#define ADM1021_DIE_REVISION	0xff

/* Sensors */
#define ADMTEMP_INT		0
#define ADMTEMP_EXT		1
#define ADMTEMP_NUM_SENSORS	2

struct admtemp_softc {
	i2c_tag_t	sc_tag;
	i2c_addr_t	sc_addr;

	int		sc_noexternal;
	struct sysmon_envsys *sc_sme;
	envsys_data_t sc_sensor[ADMTEMP_NUM_SENSORS];
};

int	admtemp_match(device_t, cfdata_t, void *);
void	admtemp_attach(device_t, device_t, void *);
void	admtemp_refresh(struct sysmon_envsys *, envsys_data_t *);

CFATTACH_DECL_NEW(admtemp, sizeof(struct admtemp_softc),
	admtemp_match, admtemp_attach, NULL, NULL);

static const char * admtemp_compats[] = {
	"i2c-max1617",
	NULL
};

int
admtemp_match(device_t parent, cfdata_t match, void *aux)
{
	struct i2c_attach_args *ia = aux;

	if (ia->ia_name == NULL) {
		/*
		 * Indirect config - not much we can do!
		 * Check typical addresses.
		 */
		if (((ia->ia_addr >= 0x18) && (ia->ia_addr <= 0x1a)) ||
		    ((ia->ia_addr >= 0x29) && (ia->ia_addr <= 0x2b)) ||
		    ((ia->ia_addr >= 0x4c) && (ia->ia_addr <= 0x4e)))
			return (1);
	} else {
		/*
		 * Direct config - match via the list of compatible
		 * hardware or simply match the device name.
		 */
		if (ia->ia_ncompat > 0) {
			if (iic_compat_match(ia, admtemp_compats))
				return 1;
		} else {
			if (strcmp(ia->ia_name, "admtemp") == 0)
				return 1;
		}
	}

	return 0;
}


void
admtemp_attach(device_t parent, device_t self, void *aux)
{
	struct admtemp_softc *sc = device_private(self);
	struct i2c_attach_args *ia = aux;
	u_int8_t cmd, data, stat;

	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	aprint_normal(": ADM1021 or compatible environmental sensor\n");
	aprint_naive(": Environmental sensor\n");

	iic_acquire_bus(sc->sc_tag, 0);
	cmd = ADM1021_CONFIG_READ;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0)) {
		iic_release_bus(sc->sc_tag, 0);
		aprint_error_dev(self, "cannot get control register\n");
		return;
	}
	if (data & ADM1021_CONFIG_RUN) {
		cmd = ADM1021_STATUS;
		if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
		    sc->sc_addr, &cmd, sizeof cmd, &stat, sizeof stat, 0)) {
			iic_release_bus(sc->sc_tag, 0);
			aprint_error_dev(self,
			    "cannot read status register\n");
			return;
		}
		if ((stat & ADM1021_STATUS_INVAL) == ADM1021_STATUS_INVAL) {
			if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
			    sc->sc_addr, &cmd, sizeof cmd, &stat, sizeof stat,
			    0)) {
				iic_release_bus(sc->sc_tag, 0);
				aprint_error_dev(self,
				    "cannot read status register\n");
				return;
			}
		}

		/* means external is dead */
		if ((stat & ADM1021_STATUS_INVAL) != ADM1021_STATUS_INVAL &&
		    (stat & ADM1021_STATUS_NOEXT))
			sc->sc_noexternal = 1;

		data &= ~ADM1021_CONFIG_RUN;
		cmd = ADM1021_CONFIG_WRITE;
		if (iic_exec(sc->sc_tag, I2C_OP_WRITE_WITH_STOP,
		    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0)) {
			iic_release_bus(sc->sc_tag, 0);
			aprint_error_dev(self,
			    "cannot set control register\n");
			return;
		}
	}
	iic_release_bus(sc->sc_tag, 0);

	/* Initialize sensor data. */
	sc->sc_sensor[ADMTEMP_INT].state = ENVSYS_SINVALID;
	sc->sc_sensor[ADMTEMP_INT].units = ENVSYS_STEMP;
	sc->sc_sensor[ADMTEMP_EXT].state = ENVSYS_SINVALID;
	sc->sc_sensor[ADMTEMP_EXT].units = ENVSYS_STEMP;
	sc->sc_sensor[ADMTEMP_INT].state = ENVSYS_SINVALID;
	sc->sc_sensor[ADMTEMP_EXT].state = ENVSYS_SINVALID;
	strlcpy(sc->sc_sensor[ADMTEMP_INT].desc, "internal",sizeof("internal"));
	strlcpy(sc->sc_sensor[ADMTEMP_EXT].desc, "external",sizeof("external"));
	sc->sc_sme = sysmon_envsys_create();
	if (sysmon_envsys_sensor_attach(
	    sc->sc_sme, &sc->sc_sensor[ADMTEMP_INT])) {
		sysmon_envsys_destroy(sc->sc_sme);
		aprint_error_dev(self,
		    "unable to attach internal at sysmon\n");
		return;
	}
	if (sc->sc_noexternal == 0 &&
	    sysmon_envsys_sensor_attach(
	    sc->sc_sme, &sc->sc_sensor[ADMTEMP_EXT])) {
		sysmon_envsys_destroy(sc->sc_sme);
		aprint_error_dev(self,
		    "unable to attach external at sysmon\n");
		return;
	}
        sc->sc_sme->sme_name = device_xname(self);
        sc->sc_sme->sme_cookie = sc;
        sc->sc_sme->sme_refresh = admtemp_refresh;
	if (sysmon_envsys_register(sc->sc_sme)) {
		aprint_error_dev(self,
		    "unable to register with sysmon\n");
		sysmon_envsys_destroy(sc->sc_sme);
		return;
	}
}


void
admtemp_refresh(struct sysmon_envsys *sme, envsys_data_t *edata)
{
	struct admtemp_softc *sc = sme->sme_cookie;
	u_int8_t cmd;
	int8_t sdata;

	iic_acquire_bus(sc->sc_tag, 0);

	if (edata->sensor == ADMTEMP_INT)
		cmd = ADM1021_INT_TEMP;
	else
		cmd = ADM1021_EXT_TEMP;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP, sc->sc_addr,
	    &cmd, sizeof cmd, &sdata,  sizeof sdata, 0) == 0) {
		if (sdata == ADM1021_STATUS_INVAL) {
			edata->state = ENVSYS_SINVALID;
		} else {
			edata->value_cur = 273150000 + 1000000 * sdata;
			edata->state = ENVSYS_SVALID;
		}
	}

	iic_release_bus(sc->sc_tag, 0);
}
