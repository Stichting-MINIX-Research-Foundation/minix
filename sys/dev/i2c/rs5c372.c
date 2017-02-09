/*	$NetBSD: rs5c372.c,v 1.14 2014/11/20 16:34:26 christos Exp $	*/

/*-
 * Copyright (C) 2005 NONAKA Kimihiro <nonaka@netbsd.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rs5c372.c,v 1.14 2014/11/20 16:34:26 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <sys/conf.h>
#include <sys/event.h>

#include <dev/clock_subr.h>

#include <dev/i2c/i2cvar.h>
#include <dev/i2c/rs5c372reg.h>

struct rs5c372rtc_softc {
	device_t sc_dev;
	i2c_tag_t sc_tag;
	int sc_address;
	struct todr_chip_handle sc_todr;
};

static int rs5c372rtc_match(device_t, cfdata_t, void *);
static void rs5c372rtc_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(rs5c372rtc, sizeof(struct rs5c372rtc_softc),
    rs5c372rtc_match, rs5c372rtc_attach, NULL, NULL);

static void rs5c372rtc_reg_write(struct rs5c372rtc_softc *, int, uint8_t);
static int rs5c372rtc_clock_read(struct rs5c372rtc_softc *, struct clock_ymdhms *);
static int rs5c372rtc_clock_write(struct rs5c372rtc_softc *, struct clock_ymdhms *);
static int rs5c372rtc_gettime_ymdhms(todr_chip_handle_t, struct clock_ymdhms *);
static int rs5c372rtc_settime_ymdhms(todr_chip_handle_t, struct clock_ymdhms *);

static int
rs5c372rtc_match(device_t parent, cfdata_t cf, void *arg)
{
	struct i2c_attach_args *ia = arg;

	if (ia->ia_name) {
		/* direct config - check name */
		if (strcmp(ia->ia_name, "rs5c372rtc") == 0)
			return 1;
	} else {
		/* indirect config - check typical address */
		if (ia->ia_addr == RS5C372_ADDR)
			return 1;
	}
	return 0;
}

static void
rs5c372rtc_attach(device_t parent, device_t self, void *arg)
{
	struct rs5c372rtc_softc *sc = device_private(self);
	struct i2c_attach_args *ia = arg;

	aprint_naive(": Real-time Clock\n");
	aprint_normal(": RICOH RS5C372[AB] Real-time Clock\n");

	sc->sc_tag = ia->ia_tag;
	sc->sc_address = ia->ia_addr;
	sc->sc_dev = self;
	sc->sc_todr.cookie = sc;
	sc->sc_todr.todr_gettime_ymdhms = rs5c372rtc_gettime_ymdhms;
	sc->sc_todr.todr_settime_ymdhms = rs5c372rtc_settime_ymdhms;
	sc->sc_todr.todr_setwen = NULL;

	todr_attach(&sc->sc_todr);

	/* Initialize RTC */
	rs5c372rtc_reg_write(sc, RS5C372_CONTROL2, RS5C372_CONTROL2_24HRS);
	rs5c372rtc_reg_write(sc, RS5C372_CONTROL1, 0);
}

static int
rs5c372rtc_gettime_ymdhms(todr_chip_handle_t ch, struct clock_ymdhms *dt)
{
	struct rs5c372rtc_softc *sc = ch->cookie;

	if (rs5c372rtc_clock_read(sc, dt) == 0)
		return (-1);

	return (0);
}

static int
rs5c372rtc_settime_ymdhms(todr_chip_handle_t ch, struct clock_ymdhms *dt)
{
	struct rs5c372rtc_softc *sc = ch->cookie;

	if (rs5c372rtc_clock_write(sc, dt) == 0)
		return (-1);

	return (0);
}

static void
rs5c372rtc_reg_write(struct rs5c372rtc_softc *sc, int reg, uint8_t val)
{
	uint8_t cmdbuf[2];

	if (iic_acquire_bus(sc->sc_tag, I2C_F_POLL)) {
		aprint_error_dev(sc->sc_dev,
		    "rs5c372rtc_reg_write: failed to acquire I2C bus\n");
		return;
	}

	reg &= 0xf;
	cmdbuf[0] = (reg << 4);
	cmdbuf[1] = val;
	if (iic_exec(sc->sc_tag, I2C_OP_WRITE_WITH_STOP, sc->sc_address,
	             cmdbuf, 1, &cmdbuf[1], 1, I2C_F_POLL)) {
		iic_release_bus(sc->sc_tag, I2C_F_POLL);
		aprint_error_dev(sc->sc_dev,
		    "rs5c372rtc_reg_write: failed to write reg%d\n", reg);
		return;
	}

	iic_release_bus(sc->sc_tag, I2C_F_POLL);
}

static int
rs5c372rtc_clock_read(struct rs5c372rtc_softc *sc, struct clock_ymdhms *dt)
{
	uint8_t bcd[RS5C372_NRTC_REGS];
	uint8_t cmdbuf[1];

	if (iic_acquire_bus(sc->sc_tag, I2C_F_POLL)) {
		aprint_error_dev(sc->sc_dev,
		    "rs5c372rtc_clock_read: failed to acquire I2C bus\n");
		return (0);
	}

	cmdbuf[0] = (RS5C372_SECONDS << 4);
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP, sc->sc_address,
	             cmdbuf, 1, bcd, RS5C372_NRTC_REGS, I2C_F_POLL)) {
		iic_release_bus(sc->sc_tag, I2C_F_POLL);
		aprint_error_dev(sc->sc_dev,
		    "rs5c372rtc_clock_read: failed to read rtc\n");
		return (0);
	}

	iic_release_bus(sc->sc_tag, I2C_F_POLL);

	/*
	 * Convert the RS5C372's register values into something useable
	 */
	dt->dt_sec = bcdtobin(bcd[RS5C372_SECONDS] & RS5C372_SECONDS_MASK);
	dt->dt_min = bcdtobin(bcd[RS5C372_MINUTES] & RS5C372_MINUTES_MASK);
	dt->dt_hour = bcdtobin(bcd[RS5C372_HOURS] & RS5C372_HOURS_24MASK);
	dt->dt_day = bcdtobin(bcd[RS5C372_DATE] & RS5C372_DATE_MASK);
	dt->dt_mon = bcdtobin(bcd[RS5C372_MONTH] & RS5C372_MONTH_MASK);
	dt->dt_year = bcdtobin(bcd[RS5C372_YEAR]) + 2000;

	return (1);
}

static int
rs5c372rtc_clock_write(struct rs5c372rtc_softc *sc, struct clock_ymdhms *dt)
{
	uint8_t bcd[RS5C372_NRTC_REGS];
	uint8_t cmdbuf[1];

	/*
	 * Convert our time representation into something the RS5C372
	 * can understand.
	 */
	bcd[RS5C372_SECONDS] = bintobcd(dt->dt_sec);
	bcd[RS5C372_MINUTES] = bintobcd(dt->dt_min);
	bcd[RS5C372_HOURS] = bintobcd(dt->dt_hour);
	bcd[RS5C372_DATE] = bintobcd(dt->dt_day);
	bcd[RS5C372_DAY] = bintobcd(dt->dt_wday);
	bcd[RS5C372_MONTH] = bintobcd(dt->dt_mon);
	bcd[RS5C372_YEAR] = bintobcd(dt->dt_year % 100);

	if (iic_acquire_bus(sc->sc_tag, I2C_F_POLL)) {
		aprint_error_dev(sc->sc_dev, "rs5c372rtc_clock_write: failed to "
		    "acquire I2C bus\n");
		return (0);
	}

	cmdbuf[0] = (RS5C372_SECONDS << 4);
	if (iic_exec(sc->sc_tag, I2C_OP_WRITE_WITH_STOP, sc->sc_address,
	             cmdbuf, 1, bcd, RS5C372_NRTC_REGS, I2C_F_POLL)) {
		iic_release_bus(sc->sc_tag, I2C_F_POLL);
		aprint_error_dev(sc->sc_dev,
		    "rs5c372rtc_clock_write: failed to write rtc\n");
		return (0);
	}

	iic_release_bus(sc->sc_tag, I2C_F_POLL);

	return (1);
}
