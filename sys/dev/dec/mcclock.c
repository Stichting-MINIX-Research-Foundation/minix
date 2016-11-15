/* $NetBSD: mcclock.c,v 1.28 2014/11/17 02:15:49 christos Exp $ */

/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mcclock.c,v 1.28 2014/11/17 02:15:49 christos Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <dev/clock_subr.h>

#include <dev/dec/clockvar.h>
#include <dev/dec/mcclockvar.h>
#include <dev/ic/mc146818reg.h>

/*
 * XXX default rate is machine-dependent.
 */
#ifdef __alpha__
#define MC_DFEAULTHZ	1024
#endif
#ifdef pmax
#define MC_DEFAULTHZ	256
#endif


void	mcclock_init(device_t);
int	mcclock_get(todr_chip_handle_t, struct timeval *);
int	mcclock_set(todr_chip_handle_t, struct timeval *);

const struct clockfns mcclock_clockfns = {
	mcclock_init, 
};

#define	mc146818_write(sc, reg, datum)					\
	    (*(sc)->sc_busfns->mc_bf_write)(sc, reg, datum)
#define	mc146818_read(sc, reg)						\
	    (*(sc)->sc_busfns->mc_bf_read)(sc, reg)

void
mcclock_attach(struct mcclock_softc *sc, const struct mcclock_busfns *busfns)
{

	printf(": mc146818 or compatible");

	sc->sc_busfns = busfns;

	/* Turn interrupts off, just in case. */
	mc146818_write(sc, MC_REGB, MC_REGB_BINARY | MC_REGB_24HR);

	clockattach(sc->sc_dev, &mcclock_clockfns);

	sc->sc_todr.todr_gettime = mcclock_get;
	sc->sc_todr.todr_settime = mcclock_set;
	sc->sc_todr.cookie = sc;
	todr_attach(&sc->sc_todr);
}

void
mcclock_init(device_t dev)
{
	struct mcclock_softc *sc = device_private(dev);
	int rate;

again:
	switch (hz) {
	case 32:
		rate = MC_BASE_32_KHz | MC_RATE_32_Hz;
		break;
	case 64:
		rate = MC_BASE_32_KHz | MC_RATE_64_Hz;
		break;
	case 128:
		rate = MC_BASE_32_KHz | MC_RATE_128_Hz;
		break;
	case 256:
		rate = MC_BASE_32_KHz | MC_RATE_256_Hz;
		break;
	case 512:
		rate = MC_BASE_32_KHz | MC_RATE_512_Hz;
		break;
	case 1024:
		rate = MC_BASE_32_KHz | MC_RATE_1024_Hz;
		break;
	case 2048:
		rate = MC_BASE_32_KHz | MC_RATE_2048_Hz;
		break;
	case 4096:
		rate = MC_BASE_32_KHz | MC_RATE_4096_Hz;
		break;
	case 8192:
		rate = MC_BASE_32_KHz | MC_RATE_8192_Hz;
		break;
	case 16384:
		rate = MC_BASE_4_MHz | MC_RATE_1;
		break;
	case 32768:
		rate = MC_BASE_4_MHz | MC_RATE_2;
		break;
	default:
		printf("%s: Cannot get %d Hz clock; using %d Hz\n",
		    device_xname(dev), hz, MC_DEFAULTHZ);
		hz = MC_DEFAULTHZ;
		goto again;
	}
	mc146818_write(sc, MC_REGA, rate);
	mc146818_write(sc, MC_REGB,
	    MC_REGB_PIE | MC_REGB_SQWE | MC_REGB_BINARY | MC_REGB_24HR);
}

/*
 * Experiments (and  passing years) show that Decstation PROMS
 * assume the kernel uses the clock chip as a time-of-year clock.
 * The PROM assumes the clock is always set to 1972 or 1973, and contains
 * time-of-year in seconds.   The PROM checks the clock at boot time,
 * and if it's outside that range, sets it to 1972-01-01.
 *
 * XXX should be at the mc146818 layer?
*/

/*
 * Get the time of day, based on the clock's value and/or the base value.
 */
int
mcclock_get(todr_chip_handle_t tch, struct timeval *tvp)
{
	struct mcclock_softc *sc = tch->cookie;
	uint32_t yearsecs;
	mc_todregs regs;
	int s;
	struct clock_ymdhms dt;

	s = splclock();
	MC146818_GETTOD(sc, &regs)
	splx(s);

	dt.dt_sec = regs[MC_SEC];
	dt.dt_min = regs[MC_MIN];
	dt.dt_hour = regs[MC_HOUR];
	dt.dt_day = regs[MC_DOM];
	dt.dt_mon = regs[MC_MONTH];
	dt.dt_year = 1972;

	yearsecs = clock_ymdhms_to_secs(&dt) - (72 - 70) * SECS_PER_COMMON_YEAR;

	/*
	 * Take the actual year from the filesystem if possible;
	 * allow for 2 days of clock loss and 363 days of clock gain.
	 */
	dt.dt_year = 1972; /* or MINYEAR or base/SECS_PER_COMMON_YEAR+1970... */
	dt.dt_mon = 1;
	dt.dt_day = 1;
	dt.dt_hour = 0;
	dt.dt_min = 0;
	dt.dt_sec = 0;
	for(;;) {
		tvp->tv_sec = yearsecs + clock_ymdhms_to_secs(&dt);
		if (tvp->tv_sec > tch->base_time - 2 * SECS_PER_DAY)
			break;
		dt.dt_year++;
	}

	tvp->tv_usec = 0;
	return 0;
}

/*
 * Reset the TODR based on the time value.
 */
int
mcclock_set(todr_chip_handle_t tch, struct timeval *tvp)
{
	struct mcclock_softc *sc = tch->cookie;
	struct clock_ymdhms dt;
	uint32_t yearsecs;
	mc_todregs regs;
	int s;

	/*
	 * calculate seconds relative to this year
	 */
	clock_secs_to_ymdhms(tvp->tv_sec, &dt); /* get the year */
	dt.dt_mon = 1;
	dt.dt_day = 1;
	dt.dt_hour = 0;
	dt.dt_min = 0;
	dt.dt_sec = 0;
	yearsecs = tvp->tv_sec - clock_ymdhms_to_secs(&dt);

#define first72 ((72 - 70) * SECS_PER_COMMON_YEAR)
	clock_secs_to_ymdhms(first72 + yearsecs, &dt);

#ifdef DEBUG
	if (dt.dt_year != 1972)
		printf("resettodr: botch (%d, %" PRId64 ")\n",
		    yearsecs, time_second);
#endif

	s = splclock();
	MC146818_GETTOD(sc, &regs);
	splx(s);

	regs[MC_SEC] = dt.dt_sec;
	regs[MC_MIN] = dt.dt_min;
	regs[MC_HOUR] = dt.dt_hour;
	regs[MC_DOW] = dt.dt_wday;
	regs[MC_DOM] = dt.dt_day;
	regs[MC_MONTH] = dt.dt_mon;
	regs[MC_YEAR] = dt.dt_year - 1900;	/* rt clock wants 2 digits */

	s = splclock();
	MC146818_PUTTOD(sc, &regs);
	splx(s);

	return 0;
}
