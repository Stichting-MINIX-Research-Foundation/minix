/*	$NetBSD: mcp980x.c,v 1.5 2013/10/28 11:24:08 rkujawa Exp $ */

/*-
 * Copyright (c) 2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Radoslaw Kujawa.
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

/*
 * Microchip MCP9800/1/2/3 2-Wire High-Accuracy Temperature Sensor driver.
 *
 * TODO: better error checking, particurarly in user settable limits.
 *
 * Note: MCP9805 is different and is supported by the sdtemp(4) driver.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mcp980x.c,v 1.5 2013/10/28 11:24:08 rkujawa Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/mutex.h>
#include <sys/endian.h>
#include <sys/sysctl.h>

#include <sys/bus.h>
#include <dev/i2c/i2cvar.h>

#include <dev/sysmon/sysmonvar.h>

#include <dev/i2c/mcp980xreg.h>

struct mcp980x_softc {
	device_t		sc_dev;

	i2c_tag_t		sc_tag;
	i2c_addr_t		sc_addr;

	int			sc_res;
	int			sc_hyst;
	int			sc_limit;

	/* envsys(4) stuff */
	struct sysmon_envsys	*sc_sme;
	envsys_data_t		sc_sensor;
	kmutex_t		sc_lock; 
};


static int mcp980x_match(device_t, cfdata_t, void *);
static void mcp980x_attach(device_t, device_t, void *);

static uint8_t mcp980x_reg_read_1(struct mcp980x_softc *, uint8_t);
static uint16_t mcp980x_reg_read_2(struct mcp980x_softc *, uint8_t);
static void mcp980x_reg_write_1(struct mcp980x_softc *, uint8_t, uint8_t);

static uint8_t mcp980x_resolution_get(struct mcp980x_softc *);
static void mcp980x_resolution_set(struct mcp980x_softc *, uint8_t);

static int8_t mcp980x_hysteresis_get(struct mcp980x_softc *);
static void mcp980x_hysteresis_set(struct mcp980x_softc *, int8_t);
static int8_t mcp980x_templimit_get(struct mcp980x_softc *);
static void mcp980x_templimit_set(struct mcp980x_softc *, int8_t);

static int8_t mcp980x_s8b_get(struct mcp980x_softc *, uint8_t);
static void mcp980x_s8b_set(struct mcp980x_softc *, uint8_t, int8_t);

static uint32_t mcp980x_temperature(struct mcp980x_softc *);

static void mcp980x_envsys_register(struct mcp980x_softc *);
static void mcp980x_envsys_refresh(struct sysmon_envsys *, envsys_data_t *);

static void mcp980x_setup_sysctl(struct mcp980x_softc *);
static int sysctl_mcp980x_res(SYSCTLFN_ARGS);
static int sysctl_mcp980x_hysteresis(SYSCTLFN_ARGS);
static int sysctl_mcp980x_templimit(SYSCTLFN_ARGS);

CFATTACH_DECL_NEW(mcp980x, sizeof (struct mcp980x_softc),
    mcp980x_match, mcp980x_attach, NULL, NULL);

static int
mcp980x_match(device_t parent, cfdata_t cf, void *aux)
{
	/*
	 * No sane way to probe? Perhaps at least try to match constant part
	 * of the I2Caddress.
	 */

	return 1;
}

static void
mcp980x_attach(device_t parent, device_t self, void *aux)
{
	struct mcp980x_softc *sc = device_private(self);
	struct i2c_attach_args *ia = aux;

	sc->sc_dev = self;
	sc->sc_addr = ia->ia_addr;
	sc->sc_tag = ia->ia_tag;

	aprint_normal(": Microchip MCP980x Temperature Sensor\n");

	sc->sc_res = MCP980X_CONFIG_ADC_RES_12BIT;
	mcp980x_resolution_set(sc, sc->sc_res);

	sc->sc_hyst = mcp980x_hysteresis_get(sc);
	sc->sc_limit = mcp980x_templimit_get(sc);

	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_NONE);

	mcp980x_setup_sysctl(sc);
	mcp980x_envsys_register(sc);
}

static uint16_t
mcp980x_reg_read_2(struct mcp980x_softc *sc, uint8_t reg)
{
	uint16_t rv;

	if (iic_acquire_bus(sc->sc_tag, I2C_F_POLL) != 0) {
		aprint_error_dev(sc->sc_dev, "cannot acquire bus for read\n");
		return 0;
	}

	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP, sc->sc_addr, &reg,
	    1, &rv, 2, I2C_F_POLL)) {
		aprint_error_dev(sc->sc_dev, "cannot execute operation\n");
		iic_release_bus(sc->sc_tag, I2C_F_POLL);
		return 0;
	}
	iic_release_bus(sc->sc_tag, I2C_F_POLL);

	return be16toh(rv);
}

static uint8_t
mcp980x_reg_read_1(struct mcp980x_softc *sc, uint8_t reg)
{
	uint8_t rv;

	if (iic_acquire_bus(sc->sc_tag, I2C_F_POLL) != 0) {
		aprint_error_dev(sc->sc_dev, "cannot acquire bus for read\n");
		return 0;
	}

	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP, sc->sc_addr, &reg,
	    1, &rv, 1, I2C_F_POLL)) {
		aprint_error_dev(sc->sc_dev, "cannot execute operation\n");
		iic_release_bus(sc->sc_tag, I2C_F_POLL);
		return 0;
	}
	iic_release_bus(sc->sc_tag, I2C_F_POLL);

	return rv;
}

static void
mcp980x_reg_write_2(struct mcp980x_softc *sc, uint8_t reg, uint16_t val)
{
	uint16_t beval;

	beval = htobe16(val);

        if (iic_acquire_bus(sc->sc_tag, I2C_F_POLL) != 0) {
		aprint_error_dev(sc->sc_dev, "cannot acquire bus for write\n");
		return;
	}

        if (iic_exec(sc->sc_tag, I2C_OP_WRITE_WITH_STOP, sc->sc_addr, &reg,
	    1, &beval, 2, I2C_F_POLL)) {
		aprint_error_dev(sc->sc_dev, "cannot execute operation\n");
        }

	iic_release_bus(sc->sc_tag, I2C_F_POLL);

}

static void
mcp980x_reg_write_1(struct mcp980x_softc *sc, uint8_t reg, uint8_t val)
{
        if (iic_acquire_bus(sc->sc_tag, I2C_F_POLL) != 0) {
		aprint_error_dev(sc->sc_dev, "cannot acquire bus for write\n");
		return;
	}

        if (iic_exec(sc->sc_tag, I2C_OP_WRITE_WITH_STOP, sc->sc_addr, &reg,
	    1, &val, 1, I2C_F_POLL)) {
		aprint_error_dev(sc->sc_dev, "cannot execute operation\n");
        }

	iic_release_bus(sc->sc_tag, I2C_F_POLL);

}

static int8_t
mcp980x_templimit_get(struct mcp980x_softc *sc)
{
	return mcp980x_s8b_get(sc, MCP980X_TEMP_LIMIT);
}

static void
mcp980x_templimit_set(struct mcp980x_softc *sc, int8_t val)
{
	mcp980x_s8b_set(sc, MCP980X_TEMP_LIMIT, val);
}

static int8_t
mcp980x_hysteresis_get(struct mcp980x_softc *sc)
{
	return mcp980x_s8b_get(sc, MCP980X_TEMP_HYSTERESIS);
}

static void
mcp980x_hysteresis_set(struct mcp980x_softc *sc, int8_t val)
{
	mcp980x_s8b_set(sc, MCP980X_TEMP_HYSTERESIS, val);
}

static int8_t
mcp980x_s8b_get(struct mcp980x_softc *sc, uint8_t reg) 
{
	return mcp980x_reg_read_2(sc, reg) >> MCP980X_TEMP_HYSTLIMIT_INT_SHIFT;
}

static void
mcp980x_s8b_set(struct mcp980x_softc *sc, uint8_t reg, int8_t val)
{
	mcp980x_reg_write_2(sc, reg, val << MCP980X_TEMP_HYSTLIMIT_INT_SHIFT);
}

static uint8_t 
mcp980x_resolution_get(struct mcp980x_softc *sc)
{
	uint8_t cfg, res;

	cfg = mcp980x_reg_read_1(sc, MCP980X_CONFIG);
	res = (cfg & MCP980X_CONFIG_ADC_RES) >> 
	    MCP980X_CONFIG_ADC_RES_SHIFT;

	return res;
}

static void
mcp980x_resolution_set(struct mcp980x_softc *sc, uint8_t res)
{
	uint8_t cfg;

	/* read config register but discard resolution bits */
	cfg = mcp980x_reg_read_1(sc, MCP980X_CONFIG) & ~MCP980X_CONFIG_ADC_RES;
	/* set resolution bits to new value */
	cfg |= res << MCP980X_CONFIG_ADC_RES_SHIFT;

	mcp980x_reg_write_1(sc, MCP980X_CONFIG, cfg);
}

/* Get temperature in microKelvins. */
static uint32_t
mcp980x_temperature(struct mcp980x_softc *sc)
{
	uint16_t raw;
	uint32_t rv, uk, basedegc;

	raw = mcp980x_reg_read_2(sc, MCP980X_AMBIENT_TEMP);

	basedegc = (raw & MCP980X_AMBIENT_TEMP_DEGREES) >> 
	    MCP980X_AMBIENT_TEMP_DEGREES_SHIFT;

	uk = 1000000 * basedegc;

	if (raw & MCP980X_AMBIENT_TEMP_05DEGREE)
		uk += 500000;
	if (raw & MCP980X_AMBIENT_TEMP_025DEGREE)
		uk += 250000;
	if (raw & MCP980X_AMBIENT_TEMP_0125DEGREE)
		uk += 125000;
	if (raw & MCP980X_AMBIENT_TEMP_00625DEGREE)
		uk += 62500;

	if (raw & MCP980X_AMBIENT_TEMP_SIGN)
		rv = 273150000U - uk;
	else
		rv = 273150000U + uk;

	return rv;	
}

static void
mcp980x_envsys_register(struct mcp980x_softc *sc)
{
	sc->sc_sme = sysmon_envsys_create();

	strlcpy(sc->sc_sensor.desc, "Ambient temp",
	    sizeof(sc->sc_sensor.desc));
	sc->sc_sensor.units = ENVSYS_STEMP;
	sc->sc_sensor.state = ENVSYS_SINVALID;

	if (sysmon_envsys_sensor_attach(sc->sc_sme, &sc->sc_sensor)) {
		aprint_error_dev(sc->sc_dev,
		    "error attaching sensor\n");
		return;
	}

	sc->sc_sme->sme_name = device_xname(sc->sc_dev);
	sc->sc_sme->sme_cookie = sc;
	sc->sc_sme->sme_refresh = mcp980x_envsys_refresh;

	if (sysmon_envsys_register(sc->sc_sme)) {
		aprint_error_dev(sc->sc_dev, "unable to register in sysmon\n");
		sysmon_envsys_destroy(sc->sc_sme);
	}
}

static void
mcp980x_envsys_refresh(struct sysmon_envsys *sme, envsys_data_t *edata)
{
	struct mcp980x_softc *sc = sme->sme_cookie;

	mutex_enter(&sc->sc_lock);

	edata->value_cur = mcp980x_temperature(sc);
	edata->state = ENVSYS_SVALID;

	mutex_exit(&sc->sc_lock);
}

static void
mcp980x_setup_sysctl(struct mcp980x_softc *sc)
{
	const struct sysctlnode *me = NULL, *node = NULL;
 
	sysctl_createv(NULL, 0, NULL, &me,
	    CTLFLAG_READWRITE,
	    CTLTYPE_NODE, device_xname(sc->sc_dev), NULL,
	    NULL, 0, NULL, 0,
	    CTL_MACHDEP, CTL_CREATE, CTL_EOL);

	sysctl_createv(NULL, 0, NULL, &node,
	    CTLFLAG_READWRITE | CTLFLAG_OWNDESC,
	    CTLTYPE_INT, "res", "Resolution",
	    sysctl_mcp980x_res, 1, (void *)sc, 0,
	    CTL_MACHDEP, me->sysctl_num, CTL_CREATE, CTL_EOL);
	
	sysctl_createv(NULL, 0, NULL, &node,
	    CTLFLAG_READWRITE | CTLFLAG_OWNDESC,
	    CTLTYPE_INT, "hysteresis", "Temperature hysteresis",
	    sysctl_mcp980x_hysteresis, 1, (void *)sc, 0,
	    CTL_MACHDEP, me->sysctl_num, CTL_CREATE, CTL_EOL);

	sysctl_createv(NULL, 0, NULL, &node,
	    CTLFLAG_READWRITE | CTLFLAG_OWNDESC,
	    CTLTYPE_INT, "templimit", "Temperature limit",
	    sysctl_mcp980x_templimit, 1, (void *)sc, 0,
	    CTL_MACHDEP, me->sysctl_num, CTL_CREATE, CTL_EOL);
}


SYSCTL_SETUP(sysctl_mcp980x_setup, "sysctl mcp980x subtree setup")
{
	sysctl_createv(NULL, 0, NULL, NULL, CTLFLAG_PERMANENT,
	    CTLTYPE_NODE, "machdep", NULL, NULL, 0, NULL, 0,
	    CTL_MACHDEP, CTL_EOL);
}


static int
sysctl_mcp980x_res(SYSCTLFN_ARGS)
{
	struct sysctlnode node = *rnode;
	struct mcp980x_softc *sc = node.sysctl_data;
	int newres, err;

	node.sysctl_data = &sc->sc_res;
	if ((err = (sysctl_lookup(SYSCTLFN_CALL(&node)))) != 0) 
		return err;

	if (newp) {
		newres = *(int *)node.sysctl_data;
		if (newres > MCP980X_CONFIG_ADC_RES_12BIT)
			return EINVAL;
		sc->sc_res = (uint8_t) newres;
		mcp980x_resolution_set(sc, sc->sc_res);
		return 0;
	} else {
		sc->sc_res = mcp980x_resolution_get(sc);
		node.sysctl_size = 4;
	}

	return err;
}

static int
sysctl_mcp980x_hysteresis(SYSCTLFN_ARGS)
{
	struct sysctlnode node = *rnode;
	struct mcp980x_softc *sc = node.sysctl_data;
	int newhyst, err;

	node.sysctl_data = &sc->sc_hyst;
	if ((err = (sysctl_lookup(SYSCTLFN_CALL(&node)))) != 0) 
		return err;

	if (newp) {
		newhyst = *(int *)node.sysctl_data;
		sc->sc_hyst = newhyst;
		mcp980x_hysteresis_set(sc, sc->sc_hyst);
		return 0;
	} else {
		sc->sc_hyst = mcp980x_hysteresis_get(sc);
		node.sysctl_size = 4;
	}

	return err;
}

static int
sysctl_mcp980x_templimit(SYSCTLFN_ARGS)
{
	struct sysctlnode node = *rnode;
	struct mcp980x_softc *sc = node.sysctl_data;
	int newlimit, err;

	node.sysctl_data = &sc->sc_limit;
	if ((err = (sysctl_lookup(SYSCTLFN_CALL(&node)))) != 0) 
		return err;

	if (newp) {
		newlimit = *(int *)node.sysctl_data;
		sc->sc_limit = newlimit;
		mcp980x_templimit_set(sc, sc->sc_limit);
		return 0;
	} else {
		sc->sc_limit = mcp980x_templimit_get(sc);
		node.sysctl_size = 4;
	}

	return err;
}

