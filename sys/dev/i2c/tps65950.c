/* $NetBSD: tps65950.c,v 1.5 2014/11/20 16:34:26 christos Exp $ */

/*-
 * Copyright (c) 2012 Jared D. McNeill <jmcneill@invisible.ca>
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

/*
 * TI TPS65950 OMAP Power Management and System Companion Device
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: tps65950.c,v 1.5 2014/11/20 16:34:26 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/kmem.h>
#include <sys/sysctl.h>
#include <sys/wdog.h>

#include <dev/i2c/i2cvar.h>

#include <dev/clock_subr.h>
#include <dev/sysmon/sysmonvar.h>

/* Default watchdog period, in seconds */
#ifndef TPS65950_WDOG_DEFAULT_PERIOD
#define TPS65950_WDOG_DEFAULT_PERIOD	30
#endif

/* I2C Bus Addressing */
#define	TPS65950_ADDR_ID1		0x48	/* GP */
#define TPS65950_ADDR_ID2		0x49	/* GP */
#define TPS65950_ADDR_ID3		0x4a	/* GP */
#define TPS65950_ADDR_ID4		0x4b	/* GP */
#define TPS65950_ADDR_ID5		0x12	/* SmartReflex */

/* ID2 */
#define TPS65950_ID2_IDCODE_7_0		0x85
#define TPS65950_ID2_IDCODE_15_8	0x86
#define TPS65950_ID2_IDCODE_23_16	0x87
#define TPS65950_ID2_IDCODE_31_24	0x88
#define TPS65950_ID2_UNLOCK_TEST_REG	0x97
#define TPS65950_ID2_UNLOCK_TEST_REG_MAGIC 0x49

/* ID3 */
#define TPS65950_LED_BASE		0xee
#define	TPS65950_ID3_REG_LED		(TPS65950_LED_BASE + 0)
#define	TPS65950_ID3_REG_LED_LEDAON	__BIT(0)
#define	TPS65950_ID3_REG_LED_LEDBON	__BIT(1)
#define TPS65950_ID3_REG_LED_LEDAPWM	__BIT(4)
#define TPS65950_ID3_REG_LED_LEDBPWM	__BIT(5)

/* ID4 */
#define TPS65950_PM_RECEIVER_BASE	0x5b
#define TPS65950_ID4_REG_WATCHDOG_CFG	(TPS65950_PM_RECEIVER_BASE + 3)
#define TPS65950_RTC_BASE		0x1c
#define TPS65950_ID4_REG_SECONDS_REG	(TPS65950_RTC_BASE + 0)
#define TPS65950_ID4_REG_MINUTES_REG	(TPS65950_RTC_BASE + 1)
#define TPS65950_ID4_REG_HOURS_REG	(TPS65950_RTC_BASE + 2)
#define TPS65950_ID4_REG_DAYS_REG	(TPS65950_RTC_BASE + 3)
#define TPS65950_ID4_REG_MONTHS_REG	(TPS65950_RTC_BASE + 4)
#define TPS65950_ID4_REG_YEARS_REG	(TPS65950_RTC_BASE + 5)
#define TPS65950_ID4_REG_WEEKS_REG	(TPS65950_RTC_BASE + 6)
#define TPS65950_ID4_REG_RTC_CTRL_REG	(TPS65950_RTC_BASE + 13)
#define TPS65950_ID4_REG_RTC_CTRL_REG_GET_TIME __BIT(6)
#define TPS65950_ID4_REG_RTC_CTRL_REG_STOP_RTC __BIT(1)

struct tps65950_softc {
	device_t	sc_dev;
	i2c_tag_t	sc_i2c;
	i2c_addr_t	sc_addr;

	struct sysctllog *sc_sysctllog;
	struct sysmon_wdog sc_smw;
	struct todr_chip_handle sc_todr;
};

static int	tps65950_match(device_t, cfdata_t, void *);
static void	tps65950_attach(device_t, device_t, void *);

static int	tps65950_read_1(struct tps65950_softc *, uint8_t, uint8_t *);
static int	tps65950_write_1(struct tps65950_softc *, uint8_t, uint8_t);

static void	tps65950_sysctl_attach(struct tps65950_softc *);

static void	tps65950_rtc_attach(struct tps65950_softc *);
static int	tps65950_rtc_enable(struct tps65950_softc *, bool);
static int	tps65950_rtc_gettime(todr_chip_handle_t, struct clock_ymdhms *);
static int	tps65950_rtc_settime(todr_chip_handle_t, struct clock_ymdhms *);

static void	tps65950_wdog_attach(struct tps65950_softc *);
static int	tps65950_wdog_setmode(struct sysmon_wdog *);
static int	tps65950_wdog_tickle(struct sysmon_wdog *);

CFATTACH_DECL_NEW(tps65950pm, sizeof(struct tps65950_softc),
    tps65950_match, tps65950_attach, NULL, NULL);

static int
tps65950_match(device_t parent, cfdata_t match, void *aux)
{
	struct i2c_attach_args *ia = aux;

	switch (ia->ia_addr) {
	case TPS65950_ADDR_ID1:
	case TPS65950_ADDR_ID2:
	case TPS65950_ADDR_ID3:
	case TPS65950_ADDR_ID4:
	case TPS65950_ADDR_ID5:
		return 1;
	default:
		return 0;
	}
}

static void
tps65950_attach(device_t parent, device_t self, void *aux)
{
	struct tps65950_softc *sc = device_private(self);
	struct i2c_attach_args *ia = aux;
	uint8_t buf[4];
	uint32_t idcode;

	sc->sc_dev = self;
	sc->sc_i2c = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	aprint_naive("\n");

	switch (sc->sc_addr) {
	case TPS65950_ADDR_ID2:
		memset(buf, 0, sizeof(buf));
		iic_acquire_bus(sc->sc_i2c, 0);
		tps65950_write_1(sc, TPS65950_ID2_UNLOCK_TEST_REG,
		    TPS65950_ID2_UNLOCK_TEST_REG_MAGIC);
		tps65950_read_1(sc, TPS65950_ID2_IDCODE_7_0, &buf[0]);
		tps65950_read_1(sc, TPS65950_ID2_IDCODE_15_8, &buf[1]);
		tps65950_read_1(sc, TPS65950_ID2_IDCODE_23_16, &buf[2]);
		tps65950_read_1(sc, TPS65950_ID2_IDCODE_31_24, &buf[3]);
		iic_release_bus(sc->sc_i2c, 0);
		idcode = (buf[0] << 0) | (buf[1] << 8) |
			 (buf[2] << 16) | (buf[3] << 24);
		aprint_normal(": IDCODE %08X\n", idcode);
		break;
	case TPS65950_ADDR_ID3:
		aprint_normal(": LED\n");
		tps65950_sysctl_attach(sc);
		break;
	case TPS65950_ADDR_ID4:
		aprint_normal(": RTC, WATCHDOG\n");
		tps65950_rtc_attach(sc);
		tps65950_wdog_attach(sc);
		break;
	default:
		aprint_normal("\n");
		break;
	}
}

static int
tps65950_read_1(struct tps65950_softc *sc, uint8_t reg, uint8_t *val)
{
	return iic_exec(sc->sc_i2c, I2C_OP_READ_WITH_STOP, sc->sc_addr,
	    &reg, sizeof(reg), val, sizeof(*val), 0);
}

static int
tps65950_write_1(struct tps65950_softc *sc, uint8_t reg, uint8_t val)
{
	uint8_t data[2] = { reg, val };
	return iic_exec(sc->sc_i2c, I2C_OP_WRITE_WITH_STOP, sc->sc_addr,
	    NULL, 0, data, sizeof(data), 0);
}

static int
tps65950_sysctl_leda(SYSCTLFN_ARGS)
{
	struct tps65950_softc *sc;
	struct sysctlnode node;
	uint8_t val;
	u_int leda;
	int error;

	node = *rnode;
	sc = node.sysctl_data;
	iic_acquire_bus(sc->sc_i2c, 0);
	error = tps65950_read_1(sc, TPS65950_ID3_REG_LED, &val);
	iic_release_bus(sc->sc_i2c, 0);
	if (error)
		return error;
	leda = (val & TPS65950_ID3_REG_LED_LEDAON) ? 1 : 0;
	node.sysctl_data = &leda;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	if (leda)
		val |= (TPS65950_ID3_REG_LED_LEDAON|TPS65950_ID3_REG_LED_LEDAPWM);
	else
		val &= ~(TPS65950_ID3_REG_LED_LEDAON|TPS65950_ID3_REG_LED_LEDAPWM);

	
	iic_acquire_bus(sc->sc_i2c, 0);
	error = tps65950_write_1(sc, TPS65950_ID3_REG_LED, val);
	iic_release_bus(sc->sc_i2c, 0);

	return error;
}

static int
tps65950_sysctl_ledb(SYSCTLFN_ARGS)
{
	struct tps65950_softc *sc;
	struct sysctlnode node;
	uint8_t val;
	u_int ledb;
	int error;

	node = *rnode;
	sc = node.sysctl_data;
	iic_acquire_bus(sc->sc_i2c, 0);
	error = tps65950_read_1(sc, TPS65950_ID3_REG_LED, &val);
	iic_release_bus(sc->sc_i2c, 0);
	if (error)
		return error;
	ledb = (val & TPS65950_ID3_REG_LED_LEDBON) ? 1 : 0;
	node.sysctl_data = &ledb;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	if (ledb)
		val |= (TPS65950_ID3_REG_LED_LEDBON|TPS65950_ID3_REG_LED_LEDBPWM);
	else
		val &= ~(TPS65950_ID3_REG_LED_LEDBON|TPS65950_ID3_REG_LED_LEDBPWM);

	iic_acquire_bus(sc->sc_i2c, 0);
	error = tps65950_write_1(sc, TPS65950_ID3_REG_LED, val);
	iic_release_bus(sc->sc_i2c, 0);

	return error;
}

static void
tps65950_sysctl_attach(struct tps65950_softc *sc)
{
	struct sysctllog **log = &sc->sc_sysctllog;
	const struct sysctlnode *rnode, *cnode;
	int error;

	error = sysctl_createv(log, 0, NULL, &rnode, CTLFLAG_PERMANENT,
	    CTLTYPE_NODE, "tps65950", SYSCTL_DESCR("tps65950 control"),
	    NULL, 0, NULL, 0, CTL_HW, CTL_CREATE, CTL_EOL);
	if (error)
		return;

	error = sysctl_createv(log, 0, &rnode, &cnode,
	    CTLFLAG_PERMANENT|CTLFLAG_READWRITE, CTLTYPE_INT, "leda",
	    SYSCTL_DESCR("LEDA enable"), tps65950_sysctl_leda, 0,
	    (void *)sc, 0, CTL_CREATE, CTL_EOL);
	if (error)
		return;

	error = sysctl_createv(log, 0, &rnode, &cnode,
	    CTLFLAG_PERMANENT|CTLFLAG_READWRITE, CTLTYPE_INT, "ledb",
	    SYSCTL_DESCR("LEDB enable"), tps65950_sysctl_ledb, 0,
	    (void *)sc, 0, CTL_CREATE, CTL_EOL);
	if (error)
		return;
}

static void
tps65950_rtc_attach(struct tps65950_softc *sc)
{
	sc->sc_todr.todr_gettime_ymdhms = tps65950_rtc_gettime;
	sc->sc_todr.todr_settime_ymdhms = tps65950_rtc_settime;
	sc->sc_todr.cookie = sc;
	todr_attach(&sc->sc_todr);

	iic_acquire_bus(sc->sc_i2c, 0);
	tps65950_rtc_enable(sc, true);
	iic_release_bus(sc->sc_i2c, 0);
}

static int
tps65950_rtc_enable(struct tps65950_softc *sc, bool enable)
{
	uint8_t rtc_ctrl;
	int error;

	error = tps65950_read_1(sc, TPS65950_ID4_REG_RTC_CTRL_REG, &rtc_ctrl);
	if (error)
		return error;

	if (enable) {
		rtc_ctrl |= TPS65950_ID4_REG_RTC_CTRL_REG_STOP_RTC;
	} else {
		rtc_ctrl &= ~TPS65950_ID4_REG_RTC_CTRL_REG_STOP_RTC;
	}

	return tps65950_write_1(sc, TPS65950_ID4_REG_RTC_CTRL_REG, rtc_ctrl);
}

#define RTC_READ(reg, var)						 \
	do {								 \
		tps65950_write_1(sc, TPS65950_ID4_REG_RTC_CTRL_REG,	 \
		    TPS65950_ID4_REG_RTC_CTRL_REG_GET_TIME);		 \
		if ((error = tps65950_read_1(sc, (reg), &(var))) != 0) { \
			iic_release_bus(sc->sc_i2c, 0);			 \
			return error;					 \
		}							 \
	} while (0)

#define RTC_WRITE(reg, val)						 \
	do {								 \
		if ((error = tps65950_write_1(sc, (reg), (val))) != 0) { \
			iic_release_bus(sc->sc_i2c, 0);			 \
			return error;					 \
		}							 \
	} while (0)

static int
tps65950_rtc_gettime(todr_chip_handle_t tch, struct clock_ymdhms *dt)
{
	struct tps65950_softc *sc = tch->cookie;
	uint8_t seconds_reg, minutes_reg, hours_reg,
		days_reg, months_reg, years_reg, weeks_reg;
	int error = 0;

	iic_acquire_bus(sc->sc_i2c, 0);
	RTC_READ(TPS65950_ID4_REG_SECONDS_REG, seconds_reg);
	RTC_READ(TPS65950_ID4_REG_MINUTES_REG, minutes_reg);
	RTC_READ(TPS65950_ID4_REG_HOURS_REG, hours_reg);
	RTC_READ(TPS65950_ID4_REG_DAYS_REG, days_reg);
	RTC_READ(TPS65950_ID4_REG_MONTHS_REG, months_reg);
	RTC_READ(TPS65950_ID4_REG_YEARS_REG, years_reg);
	RTC_READ(TPS65950_ID4_REG_WEEKS_REG, weeks_reg);
	iic_release_bus(sc->sc_i2c, 0);

	dt->dt_sec = bcdtobin(seconds_reg);
	dt->dt_min = bcdtobin(minutes_reg);
	dt->dt_hour = bcdtobin(hours_reg);
	dt->dt_day = bcdtobin(days_reg);
	dt->dt_mon = bcdtobin(months_reg);
	dt->dt_year = bcdtobin(years_reg) + 2000;
	dt->dt_wday = bcdtobin(weeks_reg);

	return 0;
}

static int
tps65950_rtc_settime(todr_chip_handle_t tch, struct clock_ymdhms *dt)
{
	struct tps65950_softc *sc = tch->cookie;
	int error = 0;

	iic_acquire_bus(sc->sc_i2c, 0);
	tps65950_rtc_enable(sc, false);
	RTC_WRITE(TPS65950_ID4_REG_SECONDS_REG, bintobcd(dt->dt_sec));
	RTC_WRITE(TPS65950_ID4_REG_MINUTES_REG, bintobcd(dt->dt_min));
	RTC_WRITE(TPS65950_ID4_REG_HOURS_REG, bintobcd(dt->dt_hour));
	RTC_WRITE(TPS65950_ID4_REG_DAYS_REG, bintobcd(dt->dt_day));
	RTC_WRITE(TPS65950_ID4_REG_MONTHS_REG, bintobcd(dt->dt_mon));
	RTC_WRITE(TPS65950_ID4_REG_YEARS_REG, bintobcd(dt->dt_year % 100));
	RTC_WRITE(TPS65950_ID4_REG_WEEKS_REG, bintobcd(dt->dt_wday));
	tps65950_rtc_enable(sc, true);
	iic_release_bus(sc->sc_i2c, 0);

	return error;
}

static void
tps65950_wdog_attach(struct tps65950_softc *sc)
{
	sc->sc_smw.smw_name = device_xname(sc->sc_dev);
	sc->sc_smw.smw_cookie = sc;
	sc->sc_smw.smw_setmode = tps65950_wdog_setmode;
	sc->sc_smw.smw_tickle = tps65950_wdog_tickle;
	sc->sc_smw.smw_period = TPS65950_WDOG_DEFAULT_PERIOD;

	if (sysmon_wdog_register(&sc->sc_smw) != 0)
		aprint_error_dev(sc->sc_dev, "couldn't register watchdog\n");

	iic_acquire_bus(sc->sc_i2c, 0);
	tps65950_write_1(sc, TPS65950_ID4_REG_WATCHDOG_CFG, 0);
	iic_release_bus(sc->sc_i2c, 0);
}

static int
tps65950_wdog_setmode(struct sysmon_wdog *smw)
{
	struct tps65950_softc *sc = smw->smw_cookie;
	int error;

	if ((smw->smw_mode & WDOG_MODE_MASK) == WDOG_MODE_DISARMED) {
		iic_acquire_bus(sc->sc_i2c, 0);
		error = tps65950_write_1(sc, TPS65950_ID4_REG_WATCHDOG_CFG, 0);
		iic_release_bus(sc->sc_i2c, 0);
	} else {
		if (smw->smw_period == WDOG_PERIOD_DEFAULT) {
			smw->smw_period = TPS65950_WDOG_DEFAULT_PERIOD;
		}
		if (smw->smw_period > 30) {
			error = EINVAL;
		} else {
			error = tps65950_wdog_tickle(smw);
		}
	}
	return error;
}

static int
tps65950_wdog_tickle(struct sysmon_wdog *smw)
{
	struct tps65950_softc *sc = smw->smw_cookie;
	int error;

	iic_acquire_bus(sc->sc_i2c, 0);
	tps65950_write_1(sc, TPS65950_ID4_REG_WATCHDOG_CFG, 0);
	error = tps65950_write_1(sc, TPS65950_ID4_REG_WATCHDOG_CFG,
	    smw->smw_period + 1);
	iic_release_bus(sc->sc_i2c, 0);

	return error;
}
