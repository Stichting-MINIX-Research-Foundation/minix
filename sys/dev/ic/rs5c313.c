/*	$NetBSD: rs5c313.c,v 1.10 2014/11/20 16:34:26 christos Exp $	*/

/*-
 * Copyright (c) 2006 The NetBSD Foundation, Inc.
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
__KERNEL_RCSID(0, "$NetBSD: rs5c313.c,v 1.10 2014/11/20 16:34:26 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>

#include <dev/clock_subr.h>

#include <dev/ic/rs5c313reg.h>
#include <dev/ic/rs5c313var.h>


/* todr(9) methods */
static int rs5c313_todr_gettime_ymdhms(todr_chip_handle_t, struct clock_ymdhms *);
static int rs5c313_todr_settime_ymdhms(todr_chip_handle_t, struct clock_ymdhms *);

/* sugar for chip access */
#define rtc_begin(sc)		((*sc->sc_ops->rs5c313_op_begin)(sc))
#define rtc_ce(sc, onoff)	((*sc->sc_ops->rs5c313_op_ce)(sc, onoff))
#define rtc_clk(sc, onoff)	((*sc->sc_ops->rs5c313_op_clk)(sc, onoff))
#define rtc_dir(sc, output)	((*sc->sc_ops->rs5c313_op_dir)(sc, output))
#define rtc_di(sc)		((*sc->sc_ops->rs5c313_op_read)(sc))
#define rtc_do(sc, bit)		((*sc->sc_ops->rs5c313_op_write)(sc, bit))

static int rs5c313_init(struct rs5c313_softc *);
static int rs5c313_read_reg(struct rs5c313_softc *, int);
static void rs5c313_write_reg(struct rs5c313_softc *, int, int);


void
rs5c313_attach(struct rs5c313_softc *sc)
{
	device_t self = sc->sc_dev;
	const char *model;

	switch (sc->sc_model) {
	case MODEL_5C313:
		model = "5C313";
		sc->sc_ctrl[0] = CTRL_24H;
		sc->sc_ctrl[1] = CTRL2_NTEST;
		break;

	case MODEL_5C316:
		model = "5C316";
		sc->sc_ctrl[0] = 0;
		sc->sc_ctrl[1] = CTRL2_24H|CTRL2_NTEST;
		break;
	
	default:
		aprint_error("unknown model (%d)\n", sc->sc_model);
		return;
	}

	aprint_naive("\n");
	aprint_normal(": RICOH %s real time clock\n", model);

	sc->sc_todr.cookie = sc;
	sc->sc_todr.todr_gettime_ymdhms = rs5c313_todr_gettime_ymdhms;
	sc->sc_todr.todr_settime_ymdhms = rs5c313_todr_settime_ymdhms;

	if (rs5c313_init(sc) != 0) {
		aprint_error_dev(self, "init failed\n");
		return;
	}

	todr_attach(&sc->sc_todr);
}


static int
rs5c313_init(struct rs5c313_softc *sc)
{
	device_t self = sc->sc_dev;
	int status = 0;
	int retry;

	rtc_ce(sc, 0);

	rtc_begin(sc);
	rtc_ce(sc, 1);

	if ((rs5c313_read_reg(sc, RS5C313_CTRL) & CTRL_XSTP) == 0) {
		sc->sc_valid = 1;
		goto done;
	}

	sc->sc_valid = 0;
	aprint_error_dev(self, "time not valid\n");

	rs5c313_write_reg(sc, RS5C313_TINT, 0);
	rs5c313_write_reg(sc, RS5C313_CTRL, (sc->sc_ctrl[0] | CTRL_ADJ));

	for (retry = 1000; retry > 0; --retry) {
		if (rs5c313_read_reg(sc, RS5C313_CTRL) & CTRL_BSY)
			delay(1);
		else
			break;
	}
	if (retry == 0) {
		status = EIO;
		goto done;
	}

	rs5c313_write_reg(sc, RS5C313_CTRL, sc->sc_ctrl[0]);
	rs5c313_write_reg(sc, RS5C313_CTRL2, sc->sc_ctrl[1]);

  done:
	rtc_ce(sc, 0);
	return status;
}


static int
rs5c313_todr_gettime_ymdhms(todr_chip_handle_t todr, struct clock_ymdhms *dt)
{
	struct rs5c313_softc *sc = todr->cookie;
	int retry;
	int s;

	/*
	 * If chip had invalid data on init, don't bother reading
	 * bogus values, let todr(9) cope.
	 */
	if (sc->sc_valid == 0)
		return EIO;

	s = splhigh();

	rtc_begin(sc);
	for (retry = 10; retry > 0; --retry) {
		rtc_ce(sc, 1);

		rs5c313_write_reg(sc, RS5C313_CTRL, sc->sc_ctrl[0]);
		if ((rs5c313_read_reg(sc, RS5C313_CTRL) & CTRL_BSY) == 0)
			break;

		rtc_ce(sc, 0);
		delay(1);
	}
	if (retry == 0) {
		splx(s);
		return EIO;
	}

#define RTCGET(x, y)							\
	do {								\
		int ones = rs5c313_read_reg(sc, RS5C313_ ## y ## 1);	\
		int tens = rs5c313_read_reg(sc, RS5C313_ ## y ## 10);	\
		dt->dt_ ## x = tens * 10 + ones;			\
	} while (/* CONSTCOND */0)

	RTCGET(sec, SEC);
	RTCGET(min, MIN);
	RTCGET(hour, HOUR);
	RTCGET(day, DAY);
	RTCGET(mon, MON);
	RTCGET(year, YEAR);
#undef	RTCGET
	dt->dt_wday = rs5c313_read_reg(sc, RS5C313_WDAY);

	rtc_ce(sc, 0);
	splx(s);

	dt->dt_year = (dt->dt_year % 100) + 1900;
	if (dt->dt_year < POSIX_BASE_YEAR) {
		dt->dt_year += 100;
	}

	return 0;
}


static int
rs5c313_todr_settime_ymdhms(todr_chip_handle_t todr, struct clock_ymdhms *dt)
{
	struct rs5c313_softc *sc = todr->cookie;
	int retry;
	int t;
	int s;

	s = splhigh();

	rtc_begin(sc);
	for (retry = 10; retry > 0; --retry) {
		rtc_ce(sc, 1);

		rs5c313_write_reg(sc, RS5C313_CTRL, sc->sc_ctrl[0]);
		if ((rs5c313_read_reg(sc, RS5C313_CTRL) & CTRL_BSY) == 0)
			break;

		rtc_ce(sc, 0);
		delay(1);
	}

	if (retry == 0) {
		splx(s);
		return EIO;
	}

#define	RTCSET(x, y)							     \
	do {								     \
		t = bintobcd(dt->dt_ ## y) & 0xff;				     \
		rs5c313_write_reg(sc, RS5C313_ ## x ## 1, t & 0x0f);	     \
		rs5c313_write_reg(sc, RS5C313_ ## x ## 10, (t >> 4) & 0x0f); \
	} while (/* CONSTCOND */0)

	RTCSET(SEC, sec);
	RTCSET(MIN, min);
	RTCSET(HOUR, hour);
	RTCSET(DAY, day);
	RTCSET(MON, mon);

#undef	RTCSET

	t = dt->dt_year % 100;
	t = bintobcd(t);
	rs5c313_write_reg(sc, RS5C313_YEAR1, t & 0x0f);
	rs5c313_write_reg(sc, RS5C313_YEAR10, (t >> 4) & 0x0f);

	rs5c313_write_reg(sc, RS5C313_WDAY, dt->dt_wday);

	rtc_ce(sc, 0);
	splx(s);

	sc->sc_valid = 1;
	return 0;
}


static int
rs5c313_read_reg(struct rs5c313_softc *sc, int addr)
{
	int data;

	/* output */
	rtc_dir(sc, 1);

	/* control */
	rtc_do(sc, 1);		/* ignored */
	rtc_do(sc, 1);		/* R/#W = 1(READ) */
	rtc_do(sc, 1);		/* AD = 1 */
	rtc_do(sc, 0);		/* DT = 0 */

	/* address */
	rtc_do(sc, addr & 0x8);	/* A3 */
	rtc_do(sc, addr & 0x4);	/* A2 */
	rtc_do(sc, addr & 0x2);	/* A1 */
	rtc_do(sc, addr & 0x1);	/* A0 */

	/* input */
	rtc_dir(sc, 0);

	/* ignore */
	(void)rtc_di(sc);
	(void)rtc_di(sc);
	(void)rtc_di(sc);
	(void)rtc_di(sc);

	/* data */
	data = rtc_di(sc);	/* D3 */
	data <<= 1;
	data |= rtc_di(sc);	/* D2 */
	data <<= 1;
	data |= rtc_di(sc);	/* D1 */
	data <<= 1;
	data |= rtc_di(sc);	/* D0 */

	return data;
}


static void
rs5c313_write_reg(struct rs5c313_softc *sc, int addr, int data)
{

	/* output */
	rtc_dir(sc, 1);

	/* control */
	rtc_do(sc, 1);		/* ignored */
	rtc_do(sc, 0);		/* R/#W = 0 (WRITE) */
	rtc_do(sc, 1);		/* AD = 1 */
	rtc_do(sc, 0);		/* DT = 0 */

	/* address */
	rtc_do(sc, addr & 0x8);	/* A3 */
	rtc_do(sc, addr & 0x4);	/* A2 */
	rtc_do(sc, addr & 0x2);	/* A1 */
	rtc_do(sc, addr & 0x1);	/* A0 */

	/* control */
	rtc_do(sc, 1);		/* ignored */
	rtc_do(sc, 0);		/* R/#W = 0(WRITE) */
	rtc_do(sc, 0);		/* AD = 0 */
	rtc_do(sc, 1);		/* DT = 1 */

	/* data */
	rtc_do(sc, data & 0x8);	/* D3 */
	rtc_do(sc, data & 0x4);	/* D2 */
	rtc_do(sc, data & 0x2);	/* D1 */
	rtc_do(sc, data & 0x1);	/* D0 */
}
