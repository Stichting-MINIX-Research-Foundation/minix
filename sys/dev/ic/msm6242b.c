/*      $NetBSD: msm6242b.c,v 1.3 2013/12/04 07:48:59 rkujawa Exp $ */

/*-
 * Copyright (c) 2012 The NetBSD Foundation, Inc.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: msm6242b.c,v 1.3 2013/12/04 07:48:59 rkujawa Exp $");

/* 
 * Driver for OKI MSM6242B Real Time Clock. Somewhat based on an ancient, amiga
 * specifc a2kbbc driver (which was turned into frontend to this driver).
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/bus.h>

#include <dev/clock_subr.h>

#include <dev/ic/msm6242bvar.h>
#include <dev/ic/msm6242breg.h>

/* #define MSM6242B_DEBUG 1 */

static int msm6242b_gettime_ymdhms(todr_chip_handle_t, struct clock_ymdhms *); 
int msm6242b_settime_ymdhms(todr_chip_handle_t, struct clock_ymdhms *);

bool msm6242b_hold(struct msm6242b_softc *sc);
void msm6242b_free(struct msm6242b_softc *sc);
static uint8_t msm6242b_read(struct msm6242b_softc *, uint8_t);
static void msm6242b_write(struct msm6242b_softc *, uint8_t, uint8_t);
static void msm6242b_set(struct msm6242b_softc *, uint8_t, uint8_t);
static void msm6242b_unset(struct msm6242b_softc *, uint8_t, uint8_t);

void
msm6242b_attach(struct msm6242b_softc *sc)
{
	struct clock_ymdhms dt;

	todr_chip_handle_t handle;
	aprint_normal(": OKI MSM6242B\n");

	handle = &sc->sc_handle;
	handle->cookie = sc;
	handle->todr_gettime = NULL;
	handle->todr_settime = NULL;
	handle->todr_gettime_ymdhms = msm6242b_gettime_ymdhms;
	handle->todr_settime_ymdhms = msm6242b_settime_ymdhms;
	handle->todr_setwen = NULL;

	if (msm6242b_gettime_ymdhms(handle, &dt) != 0) {
		aprint_error_dev(sc->sc_dev, "RTC does not work correctly\n");
		return;
	}

#ifdef MSM6242B_DEBUG
	aprint_normal_dev(sc->sc_dev, "the time is %d %d %d %d %d %d\n",
	    dt.dt_year, dt.dt_mon, dt.dt_day, dt.dt_hour, dt.dt_min, dt.dt_sec);
#endif /* MSM6242B_DEBUG */
	todr_attach(handle);
}

static int
msm6242b_gettime_ymdhms(todr_chip_handle_t handle, struct clock_ymdhms *dt)
{
	struct msm6242b_softc *sc;
	
	sc = handle->cookie;
	/* XXX: splsched(); */

	if(!msm6242b_hold(sc))
		return (ENXIO);

	dt->dt_sec = msm6242b_read(sc, MSM6242B_10SECOND) * 10 +
	    msm6242b_read(sc, MSM6242B_1SECOND);
	dt->dt_min = msm6242b_read(sc, MSM6242B_10MINUTE) * 10 +
	    msm6242b_read(sc, MSM6242B_1MINUTE);
	dt->dt_hour = (msm6242b_read(sc, MSM6242B_10HOUR_PMAM) &
	    MSM6242B_10HOUR_MASK) * 10 + msm6242b_read(sc, MSM6242B_1HOUR);
	dt->dt_day = msm6242b_read(sc, MSM6242B_10DAY) * 10 +
	    msm6242b_read(sc, MSM6242B_1DAY);
	dt->dt_mon = msm6242b_read(sc, MSM6242B_10MONTH) * 10 +
	    msm6242b_read(sc, MSM6242B_1MONTH);
	dt->dt_year = msm6242b_read(sc, MSM6242B_10YEAR) * 10 +
	    msm6242b_read(sc, MSM6242B_1YEAR);
	dt->dt_wday = msm6242b_read(sc, MSM6242B_WEEK);

#ifdef MSM6242B_DEBUG
	aprint_normal_dev(sc->sc_dev, "the time is %d %d %d %d %d %d\n",
	    dt->dt_year, dt->dt_mon, dt->dt_day, dt->dt_hour, dt->dt_min, dt->dt_sec);
#endif /* MSM6242B_DEBUG */

	/* handle 12h mode */
	if ((msm6242b_read(sc, MSM6242B_CONTROL_F) & 
	    MSM6242B_CONTROL_F_24H) == 0) {
		if ((msm6242b_read(sc, MSM6242B_10HOUR_PMAM) & 
		    MSM6242B_PMAM_BIT) == 0 && dt->dt_hour == 12)
			dt->dt_hour = 0;
		else if ((msm6242b_read(sc, MSM6242B_10HOUR_PMAM) & 
		    MSM6242B_PMAM_BIT) && dt->dt_hour != 12);
			dt->dt_hour += 12;
	}

	msm6242b_free(sc);

	dt->dt_year += MSM6242B_BASE_YEAR;
	if (dt->dt_year < POSIX_BASE_YEAR)
		dt->dt_year += 100;

	if ((dt->dt_hour > 23) ||
	    (dt->dt_day  > 31) ||
	    (dt->dt_mon  > 12) ||
	    (dt->dt_year > 2036))
		return (EINVAL);

	return 0; 
}

bool
msm6242b_hold(struct msm6242b_softc *sc)
{
	int try;

#define TRY_MAX 10
	for (try = 0; try < TRY_MAX; try++) {
		msm6242b_set(sc, MSM6242B_CONTROL_D, MSM6242B_CONTROL_D_HOLD);
		if (msm6242b_read(sc, MSM6242B_CONTROL_D) 
		    & MSM6242B_CONTROL_D_BUSY) {
#ifdef MSM6242B_DEBUG
			aprint_normal_dev(sc->sc_dev, "gotta idle\n");
#endif /* MSM6242B_DEBUG */
			msm6242b_unset(sc, MSM6242B_CONTROL_D, 
			    MSM6242B_CONTROL_D_HOLD);
			delay(70);
		} else {
#ifdef MSM6242B_DEBUG
			aprint_normal_dev(sc->sc_dev, "not busy\n");
#endif /* MSM6242B_DEBUG */
			break;
		}
	}		

	if (try == TRY_MAX) {
		aprint_error_dev(sc->sc_dev, "can't hold the chip\n");
		return false;
	}
	return true;
}

void
msm6242b_free(struct msm6242b_softc *sc)
{
	msm6242b_unset(sc, MSM6242B_CONTROL_D, MSM6242B_CONTROL_D_HOLD);
}

static uint8_t
msm6242b_read(struct msm6242b_softc *sc, uint8_t reg)
{
	uint8_t r;
	r = bus_space_read_1(sc->sc_iot, sc->sc_ioh, reg) & MSM6242B_MASK;
	return r;
}

static void
msm6242b_write(struct msm6242b_softc *sc, uint8_t reg, uint8_t val)
{
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, reg, val);
}

static void
msm6242b_set(struct msm6242b_softc *sc, uint8_t reg, uint8_t bits)
{
	uint8_t v;
	v = msm6242b_read(sc, reg) | bits;
	msm6242b_write(sc, reg, v);	
}

static void
msm6242b_unset(struct msm6242b_softc *sc, uint8_t reg, uint8_t bits)
{
	uint8_t v;
	v = msm6242b_read(sc, reg) & ~bits;
	msm6242b_write(sc, reg, v);
}

int 
msm6242b_settime_ymdhms(todr_chip_handle_t handle, struct clock_ymdhms *dt) 
{
	struct msm6242b_softc *sc;
	int ampm;
	/* XXX: splsched(); */

	sc = handle->cookie;

	if(!msm6242b_hold(sc))
		return (ENXIO);

	ampm = 0;
	if ((msm6242b_read(sc, MSM6242B_CONTROL_F) & 
	    MSM6242B_CONTROL_F_24H) == 0) {
		if (dt->dt_hour >= 12) { 
			ampm = MSM6242B_CONTROL_F_24H;
			if (dt->dt_hour != 12)
				dt->dt_hour -= 12;
		} else if (dt->dt_hour == 0) { 
			dt->dt_hour = 12;
		}
	}

	msm6242b_write(sc, MSM6242B_10HOUR_PMAM, (dt->dt_hour / 10) | ampm);
	msm6242b_write(sc, MSM6242B_1HOUR, dt->dt_hour % 10);
	msm6242b_write(sc, MSM6242B_10SECOND, dt->dt_sec / 10);
	msm6242b_write(sc, MSM6242B_1SECOND, dt->dt_sec % 10);
	msm6242b_write(sc, MSM6242B_10MINUTE, dt->dt_min / 10);
	msm6242b_write(sc, MSM6242B_1MINUTE, dt->dt_min % 10);
	msm6242b_write(sc, MSM6242B_10DAY, dt->dt_day / 10);
	msm6242b_write(sc, MSM6242B_1DAY, dt->dt_day % 10);
	msm6242b_write(sc, MSM6242B_10MONTH, dt->dt_mon / 10);
	msm6242b_write(sc, MSM6242B_1MONTH, dt->dt_mon % 10);
	msm6242b_write(sc, MSM6242B_10YEAR, (dt->dt_year / 10) % 10);
	msm6242b_write(sc, MSM6242B_1YEAR, dt->dt_year % 10);
	msm6242b_write(sc, MSM6242B_WEEK, dt->dt_wday);

	msm6242b_free(sc);

	return 0;
}

