/*	$NetBSD: kern_todr.c,v 1.39 2015/04/13 16:36:54 riastradh Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah Hdr: clock.c 1.18 91/01/21
 *
 *	@(#)clock.c	8.1 (Berkeley) 6/10/93
 */

#include "opt_todr.h"

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_todr.c,v 1.39 2015/04/13 16:36:54 riastradh Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/timetc.h>
#include <sys/intr.h>
#include <sys/rndsource.h>

#include <dev/clock_subr.h>	/* hmm.. this should probably move to sys */

static todr_chip_handle_t todr_handle = NULL;

/*
 * Attach the clock device to todr_handle.
 */
void
todr_attach(todr_chip_handle_t todr)
{

	if (todr_handle) {
		printf("todr_attach: TOD already configured\n");
		return;
	}
	todr_handle = todr;
}

static bool timeset = false;

/*
 * Set up the system's time, given a `reasonable' time value.
 */
void
inittodr(time_t base)
{
	bool badbase = false;
	bool waszero = (base == 0);
	bool goodtime = false;
	bool badrtc = false;
	int s;
	struct timespec ts;
	struct timeval tv;

	rnd_add_data(NULL, &base, sizeof(base), 0);

	if (base < 5 * SECS_PER_COMMON_YEAR) {
		struct clock_ymdhms basedate;

		/*
		 * If base is 0, assume filesystem time is just unknown
		 * instead of preposterous. Don't bark.
		 */
		if (base != 0)
			printf("WARNING: preposterous time in file system\n");
		/* not going to use it anyway, if the chip is readable */
		basedate.dt_year = 2010;
		basedate.dt_mon = 1;
		basedate.dt_day = 1;
		basedate.dt_hour = 12;
		basedate.dt_min = 0;
		basedate.dt_sec = 0;
		base = clock_ymdhms_to_secs(&basedate);
		badbase = true;
	}

	/*
	 * Some ports need to be supplied base in order to fabricate a time_t.
	 */
	if (todr_handle)
		todr_handle->base_time = base;

	if ((todr_handle == NULL) ||
	    (todr_gettime(todr_handle, &tv) != 0) ||
	    (tv.tv_sec < (25 * SECS_PER_COMMON_YEAR))) {

		if (todr_handle != NULL)
			printf("WARNING: preposterous TOD clock time\n");
		else
			printf("WARNING: no TOD clock present\n");
		badrtc = true;
	} else {
		time_t deltat = tv.tv_sec - base;

		if (deltat < 0)
			deltat = -deltat;

		if (!badbase && deltat >= 2 * SECS_PER_DAY) {
			
			if (tv.tv_sec < base) {
				/*
				 * The clock should never go backwards
				 * relative to filesystem time.  If it
				 * does by more than the threshold,
				 * believe the filesystem.
				 */
				printf("WARNING: clock lost %" PRId64 " days\n",
				    deltat / SECS_PER_DAY);
				badrtc = true;
			} else {
				aprint_verbose("WARNING: clock gained %" PRId64
				    " days\n", deltat / SECS_PER_DAY);
				goodtime = true;
			}
		} else {
			goodtime = true;
		}

		rnd_add_data(NULL, &tv, sizeof(tv), 0);
	}

	/* if the rtc time is bad, use the filesystem time */
	if (badrtc) {
		if (badbase) {
			printf("WARNING: using default initial time\n");
		} else {
			printf("WARNING: using filesystem time\n");
		}
		tv.tv_sec = base;
		tv.tv_usec = 0;
	}

	timeset = true;

	ts.tv_sec = tv.tv_sec;
	ts.tv_nsec = tv.tv_usec * 1000;
	s = splclock();
	tc_setclock(&ts);
	splx(s);

	if (waszero || goodtime)
		return;

	printf("WARNING: CHECK AND RESET THE DATE!\n");
}

/*
 * Reset the TODR based on the time value; used when the TODR
 * has a preposterous value and also when the time is reset
 * by the stime system call.  Also called when the TODR goes past
 * TODRZERO + 100*(SECS_PER_COMMON_YEAR+2*SECS_PER_DAY)
 * (e.g. on Jan 2 just after midnight) to wrap the TODR around.
 */
void
resettodr(void)
{
	struct timeval tv;

	/*
	 * We might have been called by boot() due to a crash early
	 * on.  Don't reset the clock chip if we don't know what time
	 * it is.
	 */
	if (!timeset)
		return;

	getmicrotime(&tv);

	if (tv.tv_sec == 0)
		return;

	if (todr_handle)
		if (todr_settime(todr_handle, &tv) != 0)
			printf("Cannot set TOD clock time\n");
}

#ifdef	TODR_DEBUG
static void
todr_debug(const char *prefix, int rv, struct clock_ymdhms *dt,
    struct timeval *tvp)
{
	struct timeval tv_val;
	struct clock_ymdhms dt_val;

	if (dt == NULL) {
		clock_secs_to_ymdhms(tvp->tv_sec, &dt_val);
		dt = &dt_val;
	}
	if (tvp == NULL) {
		tvp = &tv_val;
		tvp->tv_sec = clock_ymdhms_to_secs(dt);
		tvp->tv_usec = 0;
	}
	printf("%s: rv = %d\n", prefix, rv);
	printf("%s: rtc_offset = %d\n", prefix, rtc_offset);
	printf("%s: %4u/%02u/%02u %02u:%02u:%02u, (wday %d) (epoch %u.%06u)\n",
	    prefix,
	    (unsigned)dt->dt_year, dt->dt_mon, dt->dt_day,
	    dt->dt_hour, dt->dt_min, dt->dt_sec,
	    dt->dt_wday, (unsigned)tvp->tv_sec, (unsigned)tvp->tv_usec);
}
#else	/* !TODR_DEBUG */
#define	todr_debug(prefix, rv, dt, tvp)
#endif	/* TODR_DEBUG */


int
todr_gettime(todr_chip_handle_t tch, struct timeval *tvp)
{
	struct clock_ymdhms	dt;
	int			rv;

	if (tch->todr_gettime) {
		rv = tch->todr_gettime(tch, tvp);
		/*
		 * Some unconverted ports have their own references to
		 * rtc_offset.   A converted port must not do that.
		 */
		if (rv == 0)
			tvp->tv_sec += rtc_offset * 60;
		todr_debug("TODR-GET-SECS", rv, NULL, tvp);
		return rv;
	} else if (tch->todr_gettime_ymdhms) {
		rv = tch->todr_gettime_ymdhms(tch, &dt);
		todr_debug("TODR-GET-YMDHMS", rv, &dt, NULL);
		if (rv)
			return rv;

		/*
		 * Simple sanity checks.  Note that this includes a
		 * value for clocks that can return a leap second.
		 * Note that we don't support double leap seconds,
		 * since this was apparently an error/misunderstanding
		 * on the part of the ISO C committee, and can never
		 * actually occur.  If your clock issues us a double
		 * leap second, it must be broken.  Ultimately, you'd
		 * have to be trying to read time at precisely that
		 * instant to even notice, so even broken clocks will
		 * work the vast majority of the time.  In such a case
		 * it is recommended correction be applied in the
		 * clock driver.
		 */
		if (dt.dt_mon < 1 || dt.dt_mon > 12 ||
		    dt.dt_day < 1 || dt.dt_day > 31 ||
		    dt.dt_hour > 23 || dt.dt_min > 59 || dt.dt_sec > 60) {
			return EINVAL;
		}
		tvp->tv_sec = clock_ymdhms_to_secs(&dt) + rtc_offset * 60;
		tvp->tv_usec = 0;
		return tvp->tv_sec < 0 ? EINVAL : 0;
	}

	return ENXIO;
}

int
todr_settime(todr_chip_handle_t tch, struct timeval *tvp)
{
	struct clock_ymdhms	dt;
	int			rv;

	if (tch->todr_settime) {
		/* See comments above in gettime why this is ifdef'd */
		struct timeval	copy = *tvp;
		copy.tv_sec -= rtc_offset * 60;
		rv = tch->todr_settime(tch, &copy);
		todr_debug("TODR-SET-SECS", rv, NULL, tvp);
		return rv;
	} else if (tch->todr_settime_ymdhms) {
		time_t	sec = tvp->tv_sec - rtc_offset * 60;
		if (tvp->tv_usec >= 500000)
			sec++;
		clock_secs_to_ymdhms(sec, &dt);
		rv = tch->todr_settime_ymdhms(tch, &dt);
		todr_debug("TODR-SET-YMDHMS", rv, &dt, NULL);
		return rv;
	} else {
		return ENXIO;
	}
}
