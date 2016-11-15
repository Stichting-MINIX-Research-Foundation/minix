/* $NetBSD: titemp.c,v 1.1 2015/05/12 20:54:08 jmcneill Exp $ */

/*-
 * Copyright (c) 2015 Jared D. McNeill <jmcneill@invisible.ca>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: titemp.c,v 1.1 2015/05/12 20:54:08 jmcneill Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/kmem.h>

#include <dev/i2c/i2cvar.h>

#include <dev/sysmon/sysmonvar.h>

#define TITEMP_LTEMP_HI_REG		0x00
#define TITEMP_RTEMP_HI_REG		0x01
#define TITEMP_STATUS_REG		0x02
#define TITEMP_CONFIG_REG		0x03
#define TITEMP_CONVRATE_REG		0x04
#define TITEMP_LTEMP_HLIMIT_HI_REG	0x05
#define TITEMP_LTEMP_LLIMIT_HI_REG	0x06
#define TITEMP_RTEMP_HLIMIT_HI_REG	0x07
#define TITEMP_RTEMP_LLIMIT_HI_REG	0x08
#define TITEMP_RTEMP_LO_REG		0x10
#define TITEMP_RTEMP_OFF_HI_REG		0x11
#define TITEMP_RTEMP_OFF_LO_REG		0x12
#define TITEMP_RTEMP_HLIMIT_LO_REG	0x13
#define TITEMP_RTEMP_LLIMIT_LO_REG	0x14
#define TITEMP_LTEMP_LO_REG		0x15
#define TITEMP_RTEMP_THERM_LIMIT_REG	0x19
#define TITEMP_LTEMP_THERM_LIMIT_REG	0x20
#define TITEMP_THERM_HYST_REG		0x21
#define TITEMP_CONAL_REG		0x22
#define TITEMP_NC_REG			0x23
#define TITEMP_DF_REG			0x24
#define TITEMP_MFID_REG			0xfe

#define TITEMP_MFID_TMP451		0x55

struct titemp_softc {
	device_t	sc_dev;
	i2c_tag_t	sc_i2c;
	i2c_addr_t	sc_addr;

	struct sysmon_envsys *sc_sme;
	envsys_data_t	sc_sensor_ltemp;
	envsys_data_t	sc_sensor_rtemp;
};

static int	titemp_match(device_t, cfdata_t, void *);
static void	titemp_attach(device_t, device_t, void *);

static void	titemp_sensors_refresh(struct sysmon_envsys *, envsys_data_t *);
static int	titemp_read(struct titemp_softc *, uint8_t, uint8_t *);

CFATTACH_DECL_NEW(titemp, sizeof(struct titemp_softc),
    titemp_match, titemp_attach, NULL, NULL);

static int
titemp_match(device_t parent, cfdata_t match, void *aux)
{
	struct i2c_attach_args *ia = aux;
	uint8_t mfid;
	int error;

	if (iic_acquire_bus(ia->ia_tag, I2C_F_POLL) != 0)
		return 0;
	error = iic_smbus_read_byte(ia->ia_tag, ia->ia_addr,
	    TITEMP_MFID_REG, &mfid, I2C_F_POLL);
	iic_release_bus(ia->ia_tag, I2C_F_POLL);

	if (error || mfid != TITEMP_MFID_TMP451)
		return 0;

	return 1;
}

static void
titemp_attach(device_t parent, device_t self, void *aux)
{
	struct titemp_softc *sc = device_private(self);
	struct i2c_attach_args *ia = aux;

	sc->sc_dev = self;
	sc->sc_i2c = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	aprint_naive("\n");
	aprint_normal(": TMP451\n");

	sc->sc_sme = sysmon_envsys_create();
	sc->sc_sme->sme_name = device_xname(self);
	sc->sc_sme->sme_cookie = sc;
	sc->sc_sme->sme_refresh = titemp_sensors_refresh;

	sc->sc_sensor_ltemp.units = ENVSYS_STEMP;
	sc->sc_sensor_ltemp.state = ENVSYS_SINVALID;
	snprintf(sc->sc_sensor_ltemp.desc, sizeof(sc->sc_sensor_ltemp.desc),
	    "local temperature");
	sysmon_envsys_sensor_attach(sc->sc_sme, &sc->sc_sensor_ltemp);

	sc->sc_sensor_rtemp.units = ENVSYS_STEMP;
	sc->sc_sensor_rtemp.state = ENVSYS_SINVALID;
	snprintf(sc->sc_sensor_rtemp.desc, sizeof(sc->sc_sensor_rtemp.desc),
	    "remote temperature");
	sysmon_envsys_sensor_attach(sc->sc_sme, &sc->sc_sensor_rtemp);

	sysmon_envsys_register(sc->sc_sme);
}

static void
titemp_sensors_refresh(struct sysmon_envsys *sme, envsys_data_t *edata)
{
	struct titemp_softc *sc = sme->sme_cookie;
	uint8_t reg_hi, reg_lo, temp[2];
	int error;

	if (edata == &sc->sc_sensor_ltemp) {
		reg_hi = TITEMP_LTEMP_HI_REG;
		reg_lo = TITEMP_LTEMP_LO_REG;
	} else if (edata == &sc->sc_sensor_rtemp) {
		reg_hi = TITEMP_RTEMP_HI_REG;
		reg_lo = TITEMP_RTEMP_LO_REG;
	} else {
		edata->state = ENVSYS_SINVALID;
		return;
	}
		
	iic_acquire_bus(sc->sc_i2c, 0);
	if ((error = titemp_read(sc, reg_hi, &temp[0])) != 0)
		goto done;
	if ((error = titemp_read(sc, reg_lo, &temp[1])) != 0)
		goto done;
done:
	iic_release_bus(sc->sc_i2c, 0);

	if (error) {
		edata->state = ENVSYS_SINVALID;
	} else {
		edata->value_cur =
		    ((uint64_t)temp[0] * 1000000) +
		    ((uint64_t)temp[1] * 62500) +
		    + 273150000;
		edata->state = ENVSYS_SVALID;
	}
}

static int
titemp_read(struct titemp_softc *sc, uint8_t reg, uint8_t *val)
{
	return iic_smbus_read_byte(sc->sc_i2c, sc->sc_addr, reg, val, 0);
}
