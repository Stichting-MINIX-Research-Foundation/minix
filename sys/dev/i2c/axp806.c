/* $NetBSD: axp806.c,v 1.1 2014/12/07 00:33:26 jmcneill Exp $ */

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

#define AXP_DEBUG

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: axp806.c,v 1.1 2014/12/07 00:33:26 jmcneill Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/kmem.h>

#include <dev/i2c/i2cvar.h>
#include <dev/i2c/axp806.h>

#define AXP_STARTUP_SOURCE_REG	0x00
#define AXP_IC_TYPE_REG		0x03
#define AXP_DATA0_REG		0x04
#define AXP_DATA1_REG		0x05
#define AXP_DATA2_REG		0x06
#define AXP_DATA3_REG		0x07
#define AXP_OUT_CTRL1_REG	0x10
#define AXP_OUT_CTRL2_REG	0x11
#define AXP_DCDCA_CTRL_REG	0x12
#define AXP_DCDCB_CTRL_REG	0x13
#define AXP_DCDCC_CTRL_REG	0x14
#define AXP_DCDCD_CTRL_REG	0x15
#define AXP_DCDCE_CTRL_REG	0x16
#define AXP_ALDO1_CTRL_REG	0x17
#define AXP_ALDO2_CTRL_REG	0x18
#define AXP_ALDO3_CTRL_REG	0x19
#define AXP_DCDC_MODE_CTRL1_REG	0x1a
#define AXP_DCDC_MODE_CTRL2_REG	0x1b
#define AXP_DCDC_FREQ_REG	0x1c
#define AXP_OUTPUT_MON_CTRL_REG	0x1d
#define AXP_IRQ_PWROK_REG	0x1f
#define AXP_BLDO1_CTRL_REG	0x20
#define AXP_BLDO2_CTRL_REG	0x21
#define AXP_BLDO3_CTRL_REG	0x22
#define AXP_BLDO4_CTRL_REG	0x23
#define AXP_CLDO1_CTRL_REG	0x24
#define AXP_CLDO2_CTRL_REG	0x25
#define AXP_CLDO3_CTRL_REG	0x26
#define AXP_POWER_WAKE_CTRL_REG	0x31
#define AXP_POWER_DISABLE_REG	0x32
#define AXP_WAKEUP_PINFUNC_REG	0x35
#define AXP_POK_SETTING_REG	0x36
#define AXP_MODE_SEL_REG	0x3e
#define AXP_SPECIAL_CTRL_REG	0x3f
#define AXP_IRQ_ENABLE1_REG	0x40
#define AXP_IRQ_ENABLE2_REG	0x41
#define AXP_IRQ_STATUS1_REG	0x48
#define AXP_IRQ_STATUS2_REG	0x49
#define AXP_VREF_TEMP_WARN_REG	0xf3
#define AXP_SI_ADDR_EXT_REG	0xfe
#define AXP_REG_ADDR_EXT_REG	0xff

struct axp806_ctrl {
	device_t	c_dev;

	const char *	c_name;
	u_int		c_min;
	u_int		c_max;
	u_int		c_step1;
	u_int		c_step1cnt;
	u_int		c_step2;
	u_int		c_step2cnt;

	uint8_t		c_enable_reg;
	uint8_t		c_enable_mask;

	uint8_t		c_voltage_reg;
	uint8_t		c_voltage_mask;
};

#define AXP_CTRL(name, min, max, step, ereg, emask, vreg, vmask)	\
	{ .c_name = (name), .c_min = (min), .c_max = (max),		\
	  .c_step1 = (step), .c_step1cnt = (((max) - (min)) / (step)) + 1, \
	  .c_step2 = 0, .c_step2cnt = 0,				\
	  .c_enable_reg = AXP_##ereg##_REG, .c_enable_mask = (emask),	\
	  .c_voltage_reg = AXP_##vreg##_REG, .c_voltage_mask = (vmask) }

#define AXP_CTRL2(name, min, max, step1, step1cnt, step2, step2cnt, ereg, emask, vreg, vmask) \
	{ .c_name = (name), .c_min = (min), .c_max = (max),		\
	  .c_step1 = (step1), .c_step1cnt = (step1cnt),			\
	  .c_step2 = (step2), .c_step2cnt = (step2cnt),			\
	  .c_enable_reg = AXP_##ereg##_REG, .c_enable_mask = (emask),	\
	  .c_voltage_reg = AXP_##vreg##_REG, .c_voltage_mask = (vmask) }

static const struct axp806_ctrl axp806_ctrls[] = {
	AXP_CTRL2("DCDCA", 600, 1520, 10, 51, 20, 21,
		OUT_CTRL1, __BIT(0), DCDCA_CTRL, __BITS(6,0)),
	AXP_CTRL("DCDCB", 1000, 2550, 50,
		OUT_CTRL1, __BIT(1), DCDCB_CTRL, __BITS(4,0)),
	AXP_CTRL2("DCDCC", 600, 1520, 10, 51, 20, 21,
		OUT_CTRL1, __BIT(2), DCDCC_CTRL, __BITS(6,0)),
	AXP_CTRL2("DCDCD", 600, 3300, 20, 46, 100, 18,
		OUT_CTRL1, __BIT(3), DCDCD_CTRL, __BITS(5,0)),
	AXP_CTRL("DCDCE", 1100, 3400, 100,
		OUT_CTRL1, __BIT(4), DCDCE_CTRL, __BITS(4,0)),
	AXP_CTRL("ALDO1", 700, 3300, 100,
		OUT_CTRL1, __BIT(5), ALDO1_CTRL, __BITS(4,0)),
	AXP_CTRL("ALDO2", 700, 3400, 100,
		OUT_CTRL1, __BIT(6), ALDO2_CTRL, __BITS(4,0)),
	AXP_CTRL("ALDO3", 700, 3300, 100,
		OUT_CTRL1, __BIT(7), ALDO3_CTRL, __BITS(4,0)),
	AXP_CTRL("BLDO1", 700, 1900, 100,
		OUT_CTRL2, __BIT(0), BLDO1_CTRL, __BITS(3,0)),
	AXP_CTRL("BLDO2", 700, 1900, 100,
		OUT_CTRL2, __BIT(1), BLDO2_CTRL, __BITS(3,0)),
	AXP_CTRL("BLDO3", 700, 1900, 100,
		OUT_CTRL2, __BIT(2), BLDO3_CTRL, __BITS(3,0)),
	AXP_CTRL("BLDO4", 700, 1900, 100,
		OUT_CTRL2, __BIT(3), BLDO4_CTRL, __BITS(3,0)),
	AXP_CTRL("CLDO1", 700, 3300, 100, 
		OUT_CTRL2, __BIT(4), CLDO1_CTRL, __BITS(4,0)),
	AXP_CTRL2("CLDO2", 700, 4200, 100, 28, 200, 4,
		OUT_CTRL2, __BIT(5), CLDO2_CTRL, __BITS(4,0)),
	AXP_CTRL("CLDO3", 700, 3300, 100, 
		OUT_CTRL2, __BIT(6), CLDO3_CTRL, __BITS(4,0)),
};

struct axp806_softc {
	device_t	sc_dev;
	i2c_tag_t	sc_i2c;
	i2c_addr_t	sc_addr;

	u_int		sc_nctrl;
	struct axp806_ctrl *sc_ctrl;
};

static int	axp806_match(device_t, cfdata_t, void *);
static void	axp806_attach(device_t, device_t, void *);

static int	axp806_read(struct axp806_softc *, uint8_t, uint8_t *);
static int	axp806_write(struct axp806_softc *, uint8_t, uint8_t);

static void	axp806_print(struct axp806_ctrl *c);

CFATTACH_DECL_NEW(axp806pm, sizeof(struct axp806_softc),
    axp806_match, axp806_attach, NULL, NULL);

static int
axp806_match(device_t parent, cfdata_t match, void *aux)
{
	return 1;
}

static void
axp806_attach(device_t parent, device_t self, void *aux)
{
	struct axp806_softc *sc = device_private(self);
	struct i2c_attach_args *ia = aux;
	u_int n;

	sc->sc_dev = self;
	sc->sc_i2c = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	aprint_naive("\n");
	aprint_normal("\n");

	sc->sc_nctrl = __arraycount(axp806_ctrls);
	sc->sc_ctrl = kmem_alloc(sizeof(axp806_ctrls), KM_SLEEP);
	memcpy(sc->sc_ctrl, axp806_ctrls, sizeof(axp806_ctrls));
	for (n = 0; n < sc->sc_nctrl; n++) {
		sc->sc_ctrl[n].c_dev = self;
	}

#ifdef AXP_DEBUG
	for (n = 0; n < sc->sc_nctrl; n++) {
		axp806_print(&sc->sc_ctrl[n]);
	}
#endif
}

static int
axp806_read(struct axp806_softc *sc, uint8_t reg, uint8_t *val)
{
	return iic_smbus_read_byte(sc->sc_i2c, sc->sc_addr, reg, val,
	    cold ? I2C_F_POLL : 0);
}

static int
axp806_write(struct axp806_softc *sc, uint8_t reg, uint8_t val)
{
	return iic_smbus_write_byte(sc->sc_i2c, sc->sc_addr, reg, val,
	    cold ? I2C_F_POLL : 0);
}

static void
axp806_print(struct axp806_ctrl *c)
{
	struct axp806_softc *sc = device_private(c->c_dev);
	u_int voltage;
	bool enabled;

	device_printf(sc->sc_dev, "%s:", c->c_name);
	if (c->c_voltage_reg) {
		if (axp806_get_voltage(c, &voltage)) {
			printf(" [??? V]");
		} else {
			printf(" [%d.%03dV]", voltage / 1000,
			    voltage % 1000);
		}
	}
	if (c->c_enable_reg) {
		if (axp806_is_enabled(c, &enabled)) {
			printf(" [unknown state]");
		} else {
			printf(" [%s]", enabled ? "ON" : "OFF");
		}
	}
	printf("\n");
}

struct axp806_ctrl *
axp806_lookup(device_t dev, const char *name)
{
	struct axp806_softc *sc = device_private(dev);
	struct axp806_ctrl *c;
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
axp806_set_voltage(struct axp806_ctrl *c, u_int min, u_int max)
{
	struct axp806_softc *sc = device_private(c->c_dev);
	u_int vol, reg_val;
	int nstep, error;
	uint8_t val;

	if (!c->c_voltage_mask)
		return EINVAL;

	if (min < c->c_min || min > c->c_max)
		return EINVAL;

	reg_val = 0;
	nstep = 1;
	vol = c->c_min;

	for (nstep = 0; nstep < c->c_step1cnt && vol < min; nstep++) {
		++reg_val;
		vol += c->c_step1;
	}
	for (nstep = 0; nstep < c->c_step2cnt && vol < min; nstep++) {
		++reg_val;
		vol += c->c_step2;
	}

	if (vol > max)
		return EINVAL;

	iic_acquire_bus(sc->sc_i2c, 0);
	if ((error = axp806_read(sc, c->c_voltage_reg, &val)) != 0)
		goto done;
	val &= ~c->c_voltage_mask;
	val |= __SHIFTIN(reg_val, c->c_voltage_mask);
	error = axp806_write(sc, c->c_voltage_reg, val);

done:
	iic_release_bus(sc->sc_i2c, 0);
#ifdef AXP_DEBUG
	if (error == 0)
		axp806_print(c);
#endif
	return error;
}

int
axp806_get_voltage(struct axp806_ctrl *c, u_int *pvol)
{
	struct axp806_softc *sc = device_private(c->c_dev);
	int reg_val, error;
	uint8_t val;

	if (!c->c_voltage_mask)
		return EINVAL;

	iic_acquire_bus(sc->sc_i2c, 0);
	error = axp806_read(sc, c->c_voltage_reg, &val);
	iic_release_bus(sc->sc_i2c, 0);
	if (error)
		return error;

	reg_val = __SHIFTOUT(val, c->c_voltage_mask);
	if (reg_val < c->c_step1cnt) {
		*pvol = c->c_min + reg_val * c->c_step1;
	} else {
		*pvol = c->c_min + (c->c_step1cnt * c->c_step1) +
		    ((reg_val - c->c_step1cnt) * c->c_step2);
	}

	return 0;
}

int
axp806_is_enabled(struct axp806_ctrl *c, bool *penabled)
{
	struct axp806_softc *sc = device_private(c->c_dev);
	uint8_t val;
	int error;

	if (!c->c_enable_mask)
		return EINVAL;

	iic_acquire_bus(sc->sc_i2c, 0);
	error = axp806_read(sc, c->c_enable_reg, &val);
	iic_release_bus(sc->sc_i2c, 0);
	if (error)
		return error;

	*penabled = !!(val & c->c_enable_mask);
	return 0;
}

int
axp806_enable(struct axp806_ctrl *c)
{
	struct axp806_softc *sc = device_private(c->c_dev);
	uint8_t val;
	int error;

	if (!c->c_enable_mask)
		return EINVAL;

	iic_acquire_bus(sc->sc_i2c, 0);
	if ((error = axp806_read(sc, c->c_enable_reg, &val)) != 0)
		goto done;
	val |= c->c_enable_mask;
	error = axp806_write(sc, c->c_enable_reg, val);
done:
	iic_release_bus(sc->sc_i2c, 0);
#ifdef AXP_DEBUG
	if (error == 0)
		axp806_print(c);
#endif

	return error;
}

int
axp806_disable(struct axp806_ctrl *c)
{
	struct axp806_softc *sc = device_private(c->c_dev);
	uint8_t val;
	int error;

	if (!c->c_enable_mask)
		return EINVAL;

	iic_acquire_bus(sc->sc_i2c, 0);
	if ((error = axp806_read(sc, c->c_enable_reg, &val)) != 0)
		goto done;
	val &= ~c->c_enable_mask;
	error = axp806_write(sc, c->c_enable_reg, val);
done:
	iic_release_bus(sc->sc_i2c, 0);
#ifdef AXP_DEBUG
	if (error == 0)
		axp806_print(c);
#endif

	return error;
}
