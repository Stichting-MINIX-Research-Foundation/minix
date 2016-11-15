/* $NetBSD: axp20x.c,v 1.2 2014/09/09 23:39:16 jmcneill Exp $ */

/*-
 * Copyright (c) 2014 Jared D. McNeill <jmcneill@invisible.ca>
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
__KERNEL_RCSID(0, "$NetBSD: axp20x.c,v 1.2 2014/09/09 23:39:16 jmcneill Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/kmem.h>

#include <dev/i2c/i2cvar.h>

#include <dev/sysmon/sysmonvar.h>

#define AXP_TEMP_MON_REG	0x5e	/* 2 bytes */

struct axp20x_softc {
	device_t	sc_dev;
	i2c_tag_t	sc_i2c;
	i2c_addr_t	sc_addr;

	struct sysmon_envsys *sc_sme;
	envsys_data_t	sc_sensor_temp;
};

static int	axp20x_match(device_t, cfdata_t, void *);
static void	axp20x_attach(device_t, device_t, void *);

static void	axp20x_sensors_refresh(struct sysmon_envsys *, envsys_data_t *);
static int	axp20x_read(struct axp20x_softc *, uint8_t, uint8_t *, size_t);

CFATTACH_DECL_NEW(axp20x, sizeof(struct axp20x_softc),
    axp20x_match, axp20x_attach, NULL, NULL);

static int
axp20x_match(device_t parent, cfdata_t match, void *aux)
{
	return 1;
}

static void
axp20x_attach(device_t parent, device_t self, void *aux)
{
	struct axp20x_softc *sc = device_private(self);
	struct i2c_attach_args *ia = aux;

	sc->sc_dev = self;
	sc->sc_i2c = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	aprint_naive("\n");
	aprint_normal("\n");

	sc->sc_sme = sysmon_envsys_create();
	sc->sc_sme->sme_name = device_xname(self);
	sc->sc_sme->sme_cookie = sc;
	sc->sc_sme->sme_refresh = axp20x_sensors_refresh;

	sc->sc_sensor_temp.units = ENVSYS_STEMP;
	sc->sc_sensor_temp.state = ENVSYS_SINVALID;
	sc->sc_sensor_temp.flags = ENVSYS_FHAS_ENTROPY;
	snprintf(sc->sc_sensor_temp.desc, sizeof(sc->sc_sensor_temp.desc),
	    "internal temperature");
	sysmon_envsys_sensor_attach(sc->sc_sme, &sc->sc_sensor_temp);

	sysmon_envsys_register(sc->sc_sme);
}

static void
axp20x_sensors_refresh(struct sysmon_envsys *sme, envsys_data_t *edata)
{
	struct axp20x_softc *sc = sme->sme_cookie;
	uint8_t buf[2];
	int error;

	iic_acquire_bus(sc->sc_i2c, 0);
	error = axp20x_read(sc, AXP_TEMP_MON_REG, buf, sizeof(buf));
	iic_release_bus(sc->sc_i2c, 0);

	if (error) {
		edata->state = ENVSYS_SINVALID;
	} else {
		/* between -144.7C and 264.8C, step +0.1C */
		edata->value_cur = (((buf[0] << 4) | (buf[1] & 0xf)) - 1447)
				   * 100000 + 273150000;
		edata->state = ENVSYS_SVALID;
	}
}

static int
axp20x_read(struct axp20x_softc *sc, uint8_t reg, uint8_t *val, size_t len)
{
	return iic_smbus_block_read(sc->sc_i2c, sc->sc_addr,
	    reg, val, len, 0);
}
