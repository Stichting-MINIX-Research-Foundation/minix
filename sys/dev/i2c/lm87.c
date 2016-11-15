/*	$NetBSD: lm87.c,v 1.5 2015/09/27 13:02:21 phx Exp $	*/
/*	$OpenBSD: lm87.c,v 1.20 2008/11/10 05:19:48 cnst Exp $	*/

/*
 * Copyright (c) 2005 Mark Kettenis
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
__KERNEL_RCSID(0, "$NetBSD: lm87.c,v 1.5 2015/09/27 13:02:21 phx Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <dev/sysmon/sysmonvar.h>

#include <dev/i2c/i2cvar.h>

/* LM87 registers */
#define LM87_2_5V	0x20
#define LM87_VCCP1	0x21
#define LM87_VCC	0x22
#define LM87_5V		0x23
#define LM87_12V	0x24
#define LM87_VCCP2	0x25
#define LM87_EXT_TEMP	0x26
#define LM87_INT_TEMP	0x27
#define LM87_FAN1	0x28
#define LM87_FAN2	0x29
#define LM87_COMPANY_ID	0x3e
#define LM87_REVISION	0x3f
#define LM87_CONFIG1	0x40
#define  LM87_CONFIG1_START	0x01
#define  LM87_CONFIG1_INTCLR	0x08
#define LM87_CHANNEL	0x16
#define  LM87_CHANNEL_AIN1	0x01
#define  LM87_CHANNEL_AIN2	0x02
#define LM87_FANDIV	0x47

struct lmenv_id {
	u_int8_t id, family;
	const char *name;
};

static const struct lmenv_id lmenv_ids[] = {
	{ 0x01, 81, "LM81" },
	{ 0x02, 87, "LM87" },	/* LM87 or LM87CIMT */
	{ 0x23, 81, "ADM9240" },
	{ 0xda, 81, "DSL780" },
	{ 0x00, 0, NULL }
};

/* Sensors */
#define LMENV_2_5V		0
#define LMENV_VCCP1		1
#define LMENV_VCC		2
#define LMENV_5V		3
#define LMENV_12V		4
#define LMENV_VCCP2		5
#define LMENV_EXT_TEMP		6
#define LMENV_INT_TEMP		7
#define LMENV_FAN1		8
#define LMENV_FAN2		9
#define LMENV_NUM_SENSORS	10

struct lmenv_softc {
	i2c_tag_t sc_tag;
	i2c_addr_t sc_addr;

	int	sc_fan1_div, sc_fan2_div;
	int	sc_family;

	struct sysmon_envsys *sc_sme;
	envsys_data_t sc_sensor[LMENV_NUM_SENSORS];
};

int	lmenv_match(device_t, cfdata_t, void *);
void	lmenv_attach(device_t, device_t, void *);

void	lmenv_refresh(struct sysmon_envsys *, envsys_data_t *);

CFATTACH_DECL_NEW(lmenv, sizeof(struct lmenv_softc),
	lmenv_match, lmenv_attach, NULL, NULL);

static const char * lmenv_compats[] = {
	"lm87",
	"lm87cimt",
	"adm9240",
	"lm81",
	"ds1780",
	NULL
};

int
lmenv_match(device_t parent, cfdata_t match, void *aux)
{
	struct i2c_attach_args *ia = aux;
	u_int8_t cmd, val;
	int error, i;

	if (ia->ia_name == NULL) {
		/*
		 * Indirect config - not much we can do!
		 * Check typical addresses and read the Company ID register
		 */
		if ((ia->ia_addr < 0x2c) || (ia->ia_addr > 0x2f))
			return 0;

		cmd = LM87_COMPANY_ID;
		iic_acquire_bus(ia->ia_tag, 0);
		error = iic_exec(ia->ia_tag, I2C_OP_READ_WITH_STOP, ia->ia_addr,
		    &cmd, 1, &val, 1, I2C_F_POLL);
		iic_release_bus(ia->ia_tag, 0);

		if (error)
			return 0;

		for (i = 0; lmenv_ids[i].id != 0; i++)
			if (lmenv_ids[i].id == val)
				return 1;
	} else {
		/*
		 * Direct config - match via the list of compatible
		 * hardware or simply match the device name.
		 */
		if (ia->ia_ncompat > 0) {
			if (iic_compat_match(ia, lmenv_compats))
				return 1;
		} else {
			if (strcmp(ia->ia_name, "lmenv") == 0)
				return 1;
		}
	}

	return 0;
}

void
lmenv_attach(device_t parent, device_t self, void *aux)
{
	struct lmenv_softc *sc = device_private(self);
	struct i2c_attach_args *ia = aux;
	u_int8_t cmd, data, data2, channel;
	int i;

	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	iic_acquire_bus(sc->sc_tag, 0);

	cmd = LM87_COMPANY_ID;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0)) {
		iic_release_bus(sc->sc_tag, 0);
		printf(": cannot read ID register\n");
		return;
	}
	for (i = 0; lmenv_ids[i].id != 0; i++)
		if (lmenv_ids[i].id == data)
			break;

	cmd = LM87_REVISION;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &data2, sizeof data, 0)) {
		iic_release_bus(sc->sc_tag, 0);
		printf(": cannot read revision register\n");
		return;
	}
	printf(": %s rev %x\n", lmenv_ids[i].name, data2);
	sc->sc_family = lmenv_ids[i].family;

	cmd = LM87_FANDIV;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0)) {
		iic_release_bus(sc->sc_tag, 0);
		printf(", cannot read Fan Divisor register\n");
		return;
	}
	sc->sc_fan1_div = 1 << ((data >> 4) & 0x03);
	sc->sc_fan2_div = 1 << ((data >> 6) & 0x03);

	if (sc->sc_family == 87) {
		cmd = LM87_CHANNEL;
		if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
		    sc->sc_addr, &cmd, sizeof cmd, &channel,
		    sizeof channel, 0)) {
			iic_release_bus(sc->sc_tag, 0);
			printf(", cannot read Channel register\n");
			return;
		}
	} else
		channel = 0;

	cmd = LM87_CONFIG1;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0)) {
		iic_release_bus(sc->sc_tag, 0);
		printf(", cannot read Configuration Register 1\n");
		return;
	}

	/*
	 * if chip is not running, try to start it.
	 * if it is stalled doing an interrupt, unstall it
	 */
	data2 = (data | LM87_CONFIG1_START);
	data2 = data2 & ~LM87_CONFIG1_INTCLR;

	if (data != data2) {
		if (iic_exec(sc->sc_tag, I2C_OP_WRITE_WITH_STOP,
		    sc->sc_addr, &cmd, sizeof cmd, &data2, sizeof data2, 0)) {
			iic_release_bus(sc->sc_tag, 0);
			printf(", cannot write Configuration Register 1\n");
			return;
		}
	}
	iic_release_bus(sc->sc_tag, 0);

	/* Initialize sensor data. */
	sc->sc_sensor[LMENV_2_5V].state = ENVSYS_SINVALID;
	sc->sc_sensor[LMENV_2_5V].units = ENVSYS_SVOLTS_DC;
	strlcpy(sc->sc_sensor[LMENV_2_5V].desc, "+2.5Vin",
	    sizeof(sc->sc_sensor[LMENV_2_5V].desc));

	sc->sc_sensor[LMENV_VCCP1].state = ENVSYS_SINVALID;
	sc->sc_sensor[LMENV_VCCP1].units = ENVSYS_SVOLTS_DC;
	strlcpy(sc->sc_sensor[LMENV_VCCP1].desc, "Vccp1",
	    sizeof(sc->sc_sensor[LMENV_VCCP1].desc));

	sc->sc_sensor[LMENV_VCC].state = ENVSYS_SINVALID;
	sc->sc_sensor[LMENV_VCC].units = ENVSYS_SVOLTS_DC;
	strlcpy(sc->sc_sensor[LMENV_VCC].desc, "+Vcc",
	    sizeof(sc->sc_sensor[LMENV_VCC].desc));

	sc->sc_sensor[LMENV_5V].state = ENVSYS_SINVALID;
	sc->sc_sensor[LMENV_5V].units = ENVSYS_SVOLTS_DC;
	strlcpy(sc->sc_sensor[LMENV_5V].desc, "+5Vin/Vcc",
	    sizeof(sc->sc_sensor[LMENV_5V].desc));

	sc->sc_sensor[LMENV_12V].state = ENVSYS_SINVALID;
	sc->sc_sensor[LMENV_12V].units = ENVSYS_SVOLTS_DC;
	strlcpy(sc->sc_sensor[LMENV_12V].desc, "+12Vin",
	    sizeof(sc->sc_sensor[LMENV_12V].desc));

	sc->sc_sensor[LMENV_VCCP2].state = ENVSYS_SINVALID;
	sc->sc_sensor[LMENV_VCCP2].units = ENVSYS_SVOLTS_DC;
	strlcpy(sc->sc_sensor[LMENV_VCCP2].desc, "Vccp2",
	    sizeof(sc->sc_sensor[LMENV_VCCP2].desc));

	sc->sc_sensor[LMENV_EXT_TEMP].state = ENVSYS_SINVALID;
	sc->sc_sensor[LMENV_EXT_TEMP].units = ENVSYS_STEMP;
	strlcpy(sc->sc_sensor[LMENV_EXT_TEMP].desc, "External",
	    sizeof(sc->sc_sensor[LMENV_EXT_TEMP].desc));
	if (sc->sc_family == 81)
		sc->sc_sensor[LMENV_EXT_TEMP].state = ENVSYS_SINVALID;

	sc->sc_sensor[LMENV_INT_TEMP].state = ENVSYS_SINVALID;
	sc->sc_sensor[LMENV_INT_TEMP].units = ENVSYS_STEMP;
	strlcpy(sc->sc_sensor[LMENV_INT_TEMP].desc, "Internal",
	    sizeof(sc->sc_sensor[LMENV_INT_TEMP].desc));

	sc->sc_sensor[LMENV_FAN1].state = ENVSYS_SINVALID;
	if (channel & LM87_CHANNEL_AIN1) {
		sc->sc_sensor[LMENV_FAN1].units = ENVSYS_SVOLTS_DC;
		strlcpy(sc->sc_sensor[LMENV_FAN1].desc, "AIN1",
		    sizeof(sc->sc_sensor[LMENV_FAN1].desc));
	} else {
		sc->sc_sensor[LMENV_FAN1].units = ENVSYS_SFANRPM;
		strlcpy(sc->sc_sensor[LMENV_FAN1].desc, "FAN1",
		    sizeof(sc->sc_sensor[LMENV_FAN1].desc));
	}

	sc->sc_sensor[LMENV_FAN2].state = ENVSYS_SINVALID;
	if (channel & LM87_CHANNEL_AIN2) {
		sc->sc_sensor[LMENV_FAN2].units = ENVSYS_SVOLTS_DC;
		strlcpy(sc->sc_sensor[LMENV_FAN2].desc, "AIN2",
		    sizeof(sc->sc_sensor[LMENV_FAN2].desc));
	} else {
		sc->sc_sensor[LMENV_FAN2].units = ENVSYS_SFANRPM;
		strlcpy(sc->sc_sensor[LMENV_FAN2].desc, "FAN2",
		    sizeof(sc->sc_sensor[LMENV_FAN2].desc));
	}

	sc->sc_sme = sysmon_envsys_create();
	for (i = 0; i < LMENV_NUM_SENSORS; i++)
		if (sysmon_envsys_sensor_attach(sc->sc_sme,
		    &sc->sc_sensor[i])) {
			sysmon_envsys_destroy(sc->sc_sme);
			aprint_error_dev(self,
			    "unable to attach sensor %d at sysmon\n", i);
			return;
		}
        sc->sc_sme->sme_name = device_xname(self);
        sc->sc_sme->sme_cookie = sc;
        sc->sc_sme->sme_refresh = lmenv_refresh;
	if (sysmon_envsys_register(sc->sc_sme)) {
		aprint_error_dev(self,
		    "unable to register with sysmon\n");
		sysmon_envsys_destroy(sc->sc_sme);
		return;
	}
}

void
lmenv_refresh(struct sysmon_envsys *sme, envsys_data_t *edata)
{
	struct lmenv_softc *sc = sme->sme_cookie;
	u_int8_t cmd, data;
	u_int tmp;

	iic_acquire_bus(sc->sc_tag, 0);

	cmd = LM87_2_5V + edata->sensor;
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sc->sc_addr, &cmd, sizeof cmd, &data, sizeof data, 0)) {
		edata->state = ENVSYS_SINVALID;
		return;
	}

	switch (edata->sensor) {
	case LMENV_2_5V:
		edata->value_cur = 2500000 * data / 192;
		edata->state = ENVSYS_SVALID;
		break;
	case LMENV_5V:
		edata->value_cur = 5000000 * data / 192;
		edata->state = ENVSYS_SVALID;
		break;
	case LMENV_12V:
		edata->value_cur = 12000000 * data / 192;
		edata->state = ENVSYS_SVALID;
		break;
	case LMENV_VCCP1:
	case LMENV_VCCP2:
		edata->value_cur = 2700000 * data / 192;
		edata->state = ENVSYS_SVALID;
		break;
	case LMENV_VCC:
		edata->value_cur = 3300000 * data / 192;
		edata->state = ENVSYS_SVALID;
		break;
	case LMENV_EXT_TEMP:
		if (sc->sc_family == 81) {
			edata->state = ENVSYS_SINVALID;
			break;		/* missing on LM81 */
		}
		/* FALLTHROUGH */
	case LMENV_INT_TEMP:
		if (data == 0x80)
			edata->state = ENVSYS_SINVALID;
		else {
			edata->value_cur = (int8_t)data * 1000000 + 273150000;
			edata->state = ENVSYS_SVALID;
		}
		break;
	case LMENV_FAN1:
		if (edata->units == ENVSYS_SVOLTS_DC) {
			edata->value_cur = 1870000 * data / 192;
			edata->state = ENVSYS_SVALID;
			break;
		}
		if (data == 0xff) {
			edata->state = ENVSYS_SINVALID;
			break;
		}
		tmp = data * sc->sc_fan1_div;
		if (tmp == 0)
			edata->state = ENVSYS_SINVALID;
		else {
			edata->value_cur = 1350000 / tmp;
			edata->state = ENVSYS_SVALID;
		}
		break;
	case LMENV_FAN2:
		if (edata->units == ENVSYS_SVOLTS_DC) {
			edata->value_cur = 1870000 * data / 192;
			edata->state = ENVSYS_SVALID;
			break;
		}
		if (data == 0xff) {
			edata->state = ENVSYS_SINVALID;
			break;
		}
		tmp = data * sc->sc_fan2_div;
		if (tmp == 0)
			edata->state = ENVSYS_SINVALID;
		else {
			edata->value_cur = 1350000 / tmp;
			edata->state = ENVSYS_SVALID;
		}
		break;
	default:
		edata->state = ENVSYS_SINVALID;
		break;
	}

	iic_release_bus(sc->sc_tag, 0);
}
