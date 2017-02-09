/* $NetBSD: act8846.c,v 1.3 2015/01/02 21:55:31 jmcneill Exp $ */

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

//#define ACT_DEBUG

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: act8846.c,v 1.3 2015/01/02 21:55:31 jmcneill Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/kmem.h>

#include <dev/i2c/i2cvar.h>
#include <dev/i2c/act8846.h>

#define ACT_BATTVOL_STATUS_REG		0x00
#define ACT_THERMAL_CTRL_REG		0x01
#define ACT_DCDC1_BASE_REG		0x10
#define ACT_DCDC2_BASE_REG		0x20
#define ACT_DCDC3_BASE_REG		0x30
#define ACT_DCDC4_BASE_REG		0x40
#define ACT_LDO1_BASE_REG		0x50
#define ACT_LDO2_BASE_REG		0x58
#define ACT_LDO3_BASE_REG		0x60
#define ACT_LDO4_BASE_REG		0x68
#define ACT_LDO5_BASE_REG		0x70
#define ACT_LDO6_BASE_REG		0x80
#define ACT_LDO7_BASE_REG		0x90
#define ACT_LDO8_BASE_REG		0xa0
#define ACT_LDO9_BASE_REG		0xb0

#define ACT_VSET0_OFFSET		0
#define ACT_VSET1_OFFSET		1
#define ACT_DCDC_CTRL_OFFSET		2
#define ACT_LDO_CTRL_OFFSET		1

#define ACT_VSET_VSET			__BITS(5,0)

#define ACT_DCDC_CTRL_ON		__BIT(7)

#define ACT_LDO_CTRL_ON			__BIT(7)

enum act8846_ctrl_type {
	ACT_CTRL_DCDC,
	ACT_CTRL_LDO,
};

#define ACT_VOLTAGE_MIN			600
#define ACT_VOLTAGE_MAX			3900

struct act8846_ctrl {
	device_t	c_dev;

	const char *	c_name;
	u_int		c_min;
	u_int		c_max;
	uint8_t		c_base;
	enum act8846_ctrl_type c_type;
};

#define ACT_CTRL(name, base, type)				\
	{ .c_name = (name),					\
	  .c_min = ACT_VOLTAGE_MIN, .c_max = ACT_VOLTAGE_MAX,	\
	  .c_base = ACT_ ## base ## _BASE_REG, .c_type = (type) }

#define ACT_DCDC(name, base)	ACT_CTRL(name, base, ACT_CTRL_DCDC)
#define ACT_LDO(name, base)	ACT_CTRL(name, base, ACT_CTRL_LDO)

static const struct act8846_ctrl act8846_ctrls[] = {
	ACT_DCDC("DCDC1", DCDC1),	/* VCC_DDR */
	ACT_DCDC("DCDC2", DCDC2),	/* VDD_LOG */
	ACT_DCDC("DCDC3", DCDC3),	/* VDD_ARM */
	ACT_DCDC("DCDC4", DCDC4),	/* VCC_IO */
	ACT_LDO("LDO1", LDO1),		/* VDD_10 */
	ACT_LDO("LDO2", LDO2),		/* VCC_25 */
	ACT_LDO("LDO3", LDO3),		/* VCC18_CIF */
	ACT_LDO("LDO4", LDO4),		/* VCCA_33 */
	ACT_LDO("LDO5", LDO5),		/* VCC_TOUCH */
	ACT_LDO("LDO6", LDO6),		/* VCC33 */
	ACT_LDO("LDO7", LDO7),		/* VCC18_IO */
	ACT_LDO("LDO8", LDO8),		/* VCC28_CIF */
#if 0
	ACT_LDO("LDO9", LDO9),		/* VDD_RTC (Always-ON) */
#endif
};

/* From datasheet, Table 5: REGx/VSET[] Output Voltage Setting */
static const u_int act8846_vset[] = {
	600, 625, 650, 675, 700, 725, 750, 775,
	800, 825, 850, 875, 900, 925, 950, 975,
	1000, 1025, 1050, 1075, 1100, 1125, 1150, 1175,
	1200, 1250, 1300, 1350, 1400, 1450, 1500, 1550,
	1600, 1650, 1700, 1750, 1800, 1850, 1900, 1950,
	2000, 2050, 2100, 2150, 2200, 2250, 2300, 2350,
	2400, 2500, 2600, 2700, 2800, 2900, 3000, 3100,
	3200, 3300, 3400, 3500, 3600, 3700, 3800, 3900
};

struct act8846_softc {
	device_t	sc_dev;
	i2c_tag_t	sc_i2c;
	i2c_addr_t	sc_addr;

	u_int		sc_nctrl;
	struct act8846_ctrl *sc_ctrl;
};

static int	act8846_match(device_t, cfdata_t, void *);
static void	act8846_attach(device_t, device_t, void *);

static int	act8846_read(struct act8846_softc *, uint8_t, uint8_t *);
static int	act8846_write(struct act8846_softc *, uint8_t, uint8_t);

static void	act8846_print(struct act8846_ctrl *c);

CFATTACH_DECL_NEW(act8846pm, sizeof(struct act8846_softc),
    act8846_match, act8846_attach, NULL, NULL);

static int
act8846_match(device_t parent, cfdata_t match, void *aux)
{
	return 1;
}

static void
act8846_attach(device_t parent, device_t self, void *aux)
{
	struct act8846_softc *sc = device_private(self);
	struct i2c_attach_args *ia = aux;
	u_int n;

	sc->sc_dev = self;
	sc->sc_i2c = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	aprint_naive("\n");
	aprint_normal("\n");

	sc->sc_nctrl = __arraycount(act8846_ctrls);
	sc->sc_ctrl = kmem_alloc(sizeof(act8846_ctrls), KM_SLEEP);
	memcpy(sc->sc_ctrl, act8846_ctrls, sizeof(act8846_ctrls));
	for (n = 0; n < sc->sc_nctrl; n++) {
		sc->sc_ctrl[n].c_dev = self;
	}

	for (n = 0; n < sc->sc_nctrl; n++) {
		act8846_print(&sc->sc_ctrl[n]);
	}
}

static int
act8846_read(struct act8846_softc *sc, uint8_t reg, uint8_t *val)
{
	return iic_smbus_read_byte(sc->sc_i2c, sc->sc_addr, reg, val,
	    cold ? I2C_F_POLL : 0);
}

static int
act8846_write(struct act8846_softc *sc, uint8_t reg, uint8_t val)
{
	return iic_smbus_write_byte(sc->sc_i2c, sc->sc_addr, reg, val,
	    cold ? I2C_F_POLL : 0);
}

static void
act8846_print(struct act8846_ctrl *c)
{
	struct act8846_softc *sc = device_private(c->c_dev);
	u_int voltage;
	bool enabled;

	device_printf(sc->sc_dev, "%s:", c->c_name);
	if (act8846_get_voltage(c, &voltage)) {
		printf(" [??? V]");
	} else {
		printf(" [%d.%03dV]", voltage / 1000,
		    voltage % 1000);
	}
	if (act8846_is_enabled(c, &enabled)) {
		printf(" [unknown state]");
	} else {
		printf(" [%s]", enabled ? "ON" : "OFF");
	}
	printf("\n");
}

struct act8846_ctrl *
act8846_lookup(device_t dev, const char *name)
{
	struct act8846_softc *sc = device_private(dev);
	struct act8846_ctrl *c;
	u_int n;

	for (n = 0; n < sc->sc_nctrl; n++) {
		c = &sc->sc_ctrl[n];
		if (strcmp(c->c_name, name) == 0) {
			return c;
		}
	}

	return NULL;
}

int
act8846_set_voltage(struct act8846_ctrl *c, u_int min, u_int max)
{
	struct act8846_softc *sc = device_private(c->c_dev);
	uint8_t val;
	int error, n;

	if (min < c->c_min || min > c->c_max || (min % 25) != 0)
		return EINVAL;

	for (n = 0; n < __arraycount(act8846_vset); n++) {
		if (min >= act8846_vset[n] && max <= act8846_vset[n]) {
			break;
		}
	}
	if (n == __arraycount(act8846_vset))
		return EINVAL;

	val = __SHIFTIN(n, ACT_VSET_VSET);

	iic_acquire_bus(sc->sc_i2c, 0);
	error = act8846_write(sc, c->c_base + ACT_VSET0_OFFSET, val);
	iic_release_bus(sc->sc_i2c, 0);
#ifdef ACT_DEBUG
	if (error == 0)
		act8846_print(c);
#endif
	return error;
}

int
act8846_get_voltage(struct act8846_ctrl *c, u_int *pvol)
{
	struct act8846_softc *sc = device_private(c->c_dev);
	uint8_t val;
	int error;

	iic_acquire_bus(sc->sc_i2c, 0);
	error = act8846_read(sc, c->c_base + ACT_VSET0_OFFSET, &val);
	iic_release_bus(sc->sc_i2c, 0);
	if (error)
		return error;

	*pvol = act8846_vset[__SHIFTOUT(val, ACT_VSET_VSET)];

	return 0;
}

int
act8846_is_enabled(struct act8846_ctrl *c, bool *penabled)
{
	struct act8846_softc *sc = device_private(c->c_dev);
	uint8_t val, regoff, regmask;
	int error;

	if (c->c_type == ACT_CTRL_DCDC) {
		regoff = ACT_DCDC_CTRL_OFFSET;
		regmask = ACT_DCDC_CTRL_ON;
	} else {
		regoff = ACT_LDO_CTRL_OFFSET;
		regmask = ACT_LDO_CTRL_ON;
	}

	iic_acquire_bus(sc->sc_i2c, 0);
	error = act8846_read(sc, c->c_base + regoff, &val);
	iic_release_bus(sc->sc_i2c, 0);
	if (error)
		return error;

	*penabled = !!(val & regmask);
	return 0;
}

int
act8846_enable(struct act8846_ctrl *c)
{
	struct act8846_softc *sc = device_private(c->c_dev);
	uint8_t val, regoff, regmask;
	int error;

	if (c->c_type == ACT_CTRL_DCDC) {
		regoff = ACT_DCDC_CTRL_OFFSET;
		regmask = ACT_DCDC_CTRL_ON;
	} else {
		regoff = ACT_LDO_CTRL_OFFSET;
		regmask = ACT_LDO_CTRL_ON;
	}

	iic_acquire_bus(sc->sc_i2c, 0);
	if ((error = act8846_read(sc, c->c_base + regoff, &val)) != 0)
		goto done;
	val |= regmask;
	error = act8846_write(sc, c->c_base + regoff, val);
done:
	iic_release_bus(sc->sc_i2c, 0);
#ifdef ACT_DEBUG
	if (error == 0)
		act8846_print(c);
#endif

	return error;
}

int
act8846_disable(struct act8846_ctrl *c)
{
	struct act8846_softc *sc = device_private(c->c_dev);
	uint8_t val, regoff, regmask;
	int error;

	if (c->c_type == ACT_CTRL_DCDC) {
		regoff = ACT_DCDC_CTRL_OFFSET;
		regmask = ACT_DCDC_CTRL_ON;
	} else {
		regoff = ACT_LDO_CTRL_OFFSET;
		regmask = ACT_LDO_CTRL_ON;
	}

	iic_acquire_bus(sc->sc_i2c, 0);
	if ((error = act8846_read(sc, c->c_base + regoff, &val)) != 0)
		goto done;
	val &= ~regmask;
	error = act8846_write(sc, c->c_base + regoff, val);
done:
	iic_release_bus(sc->sc_i2c, 0);
#ifdef ACT_DEBUG
	if (error == 0)
		act8846_print(c);
#endif

	return error;
}
