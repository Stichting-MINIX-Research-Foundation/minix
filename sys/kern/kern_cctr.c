/*	$NetBSD: kern_cctr.c,v 1.9 2009/01/03 03:31:23 yamt Exp $	*/

/*-
 * Copyright (c) 2006, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * re-implementation of TSC for MP systems merging cc_microtime and
 * TSC for timecounters by Frank Kardel
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

/* basic calibration ideas are (kern_microtime.c): */
/******************************************************************************
 *                                                                            *
 * Copyright (c) David L. Mills 1993, 1994                                    *
 *                                                                            *
 * Permission to use, copy, modify, and distribute this software and its      *
 * documentation for any purpose and without fee is hereby granted, provided  *
 * that the above copyright notice appears in all copies and that both the    *
 * copyright notice and this permission notice appear in supporting           *
 * documentation, and that the name University of Delaware not be used in     *
 * advertising or publicity pertaining to distribution of the software        *
 * without specific, written prior permission.  The University of Delaware    *
 * makes no representations about the suitability this software for any       *
 * purpose.  It is provided "as is" without express or implied warranty.      *
 *                                                                            *
 ******************************************************************************/

/* reminiscents from older version of this file are: */
/*-
 * Copyright (c) 1998-2003 Poul-Henning Kamp
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
/* __FBSDID("$FreeBSD: src/sys/i386/i386/tsc.c,v 1.204 2003/10/21 18:28:34 silby Exp $"); */
__KERNEL_RCSID(0, "$NetBSD: kern_cctr.c,v 1.9 2009/01/03 03:31:23 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/timetc.h>
#include <sys/kernel.h>
#include <sys/power.h>
#include <sys/cpu.h>
#include <machine/cpu_counter.h>

/* XXX make cc_timecounter.tc_frequency settable by sysctl() */

static timecounter_pps_t cc_calibrate;

void cc_calibrate_cpu(struct cpu_info *);

static int64_t cc_cal_val;  /* last calibrate time stamp */

static struct timecounter cc_timecounter = {
	.tc_get_timecount	= cc_get_timecount,
	.tc_poll_pps		= cc_calibrate,
	.tc_counter_mask	= ~0u,
	.tc_frequency		= 0,
	.tc_name		= "unkown cycle counter",
	/*
	 * don't pick cycle counter automatically
	 * if frequency changes might affect cycle counter
	 */
	.tc_quality		= -100000,

	.tc_priv		= NULL,
	.tc_next		= NULL
};

/*
 * initialize cycle counter based timecounter
 */
struct timecounter *
cc_init(timecounter_get_t getcc, uint64_t freq, const char *name, int quality)
{

	if (getcc != NULL)
		cc_timecounter.tc_get_timecount = getcc;

	cc_timecounter.tc_frequency = freq;
	cc_timecounter.tc_name = name;
	cc_timecounter.tc_quality = quality;
	tc_init(&cc_timecounter);

	return &cc_timecounter;
}

/*
 * pick up tick count scaled to reference tick count
 */
u_int
cc_get_timecount(struct timecounter *tc)
{
	struct cpu_info *ci;
	int64_t rcc, cc, ncsw;
	u_int gen;

 retry:
 	ncsw = curlwp->l_ncsw;
 	__insn_barrier();
	ci = curcpu();
	if (ci->ci_cc.cc_denom == 0) {
		/*
		 * This is our first time here on this CPU.  Just
		 * start with reasonable initial values.
		 */
	        ci->ci_cc.cc_cc    = cpu_counter32();
		ci->ci_cc.cc_val   = 0;
		if (ci->ci_cc.cc_gen == 0)
			ci->ci_cc.cc_gen++;

		ci->ci_cc.cc_denom = cpu_frequency(ci);
		if (ci->ci_cc.cc_denom == 0)
			ci->ci_cc.cc_denom = cc_timecounter.tc_frequency;
		ci->ci_cc.cc_delta = ci->ci_cc.cc_denom;
	}

	/*
	 * read counter and re-read when the re-calibration
	 * strikes inbetween
	 */
	do {
		/* pick up current generation number */
		gen = ci->ci_cc.cc_gen;

		/* determine local delta ticks */
		cc = cpu_counter32() - ci->ci_cc.cc_cc;
		if (cc < 0)
			cc += 0x100000000LL;

		/* scale to primary */
		rcc = (cc * ci->ci_cc.cc_delta) / ci->ci_cc.cc_denom
		    + ci->ci_cc.cc_val;
	} while (gen == 0 || gen != ci->ci_cc.cc_gen);
 	__insn_barrier();
 	if (ncsw != curlwp->l_ncsw) {
 		/* Was preempted */ 
 		goto retry;
	}

	return rcc;
}

/*
 * called once per clock tick via the pps callback
 * for the calibration of the TSC counters.
 * it is called only for the PRIMARY cpu. all
 * other cpus are called via a broadcast IPI
 * calibration interval is 1 second - we call
 * the calibration code only every hz calls
 */
static void
cc_calibrate(struct timecounter *tc)
{
	static int calls;
	struct cpu_info *ci;

	KASSERT(kpreempt_disabled());

	 /*
	  * XXX: for high interrupt frequency
	  * support: ++calls < hz / tc_tick
	  */
	if (++calls < hz)
		return;

	calls = 0;
	ci = curcpu();
	/* pick up reference ticks */
	cc_cal_val = cpu_counter32();

#if defined(MULTIPROCESSOR)
	cc_calibrate_mp(ci);
#endif
	cc_calibrate_cpu(ci);
}

/*
 * This routine is called about once per second directly by the master
 * processor and via an interprocessor interrupt for other processors.
 * It determines the CC frequency of each processor relative to the
 * master clock and the time this determination is made.  These values
 * are used by cc_get_timecount() to interpolate the ticks between
 * timer interrupts.  Note that we assume the kernel variables have
 * been zeroed early in life.
 */
void
cc_calibrate_cpu(struct cpu_info *ci)
{
	u_int   gen;
	int64_t val;
	int64_t delta, denom;
	int s;
#ifdef TIMECOUNTER_DEBUG
	int64_t factor, old_factor;
#endif
	val = cc_cal_val;

	s = splhigh();
	/* create next generation number */
	gen = ci->ci_cc.cc_gen;
	gen++;
	if (gen == 0)
		gen++;

	/* update in progress */
	ci->ci_cc.cc_gen = 0;

	denom = ci->ci_cc.cc_cc;
	ci->ci_cc.cc_cc = cpu_counter32();

	if (ci->ci_cc.cc_denom == 0) {
		/*
		 * This is our first time here on this CPU.  Just
		 * start with reasonable initial values.
		 */
		ci->ci_cc.cc_val = val;
		ci->ci_cc.cc_denom = cpu_frequency(ci);
		if (ci->ci_cc.cc_denom == 0)
			ci->ci_cc.cc_denom = cc_timecounter.tc_frequency;
		ci->ci_cc.cc_delta = ci->ci_cc.cc_denom;
		ci->ci_cc.cc_gen = gen;
		splx(s);
		return;
	}

#ifdef TIMECOUNTER_DEBUG
	old_factor = (ci->ci_cc.cc_delta * 1000 ) / ci->ci_cc.cc_denom;
#endif

	/* local ticks per period */
	denom = ci->ci_cc.cc_cc - denom;
	if (denom < 0)
		denom += 0x100000000LL;

	ci->ci_cc.cc_denom = denom;

	/* reference ticks per period */
	delta = val - ci->ci_cc.cc_val;
	if (delta < 0)
		delta += 0x100000000LL;

	ci->ci_cc.cc_val = val;
	ci->ci_cc.cc_delta = delta;
	
	/* publish new generation number */
	ci->ci_cc.cc_gen = gen;
	splx(s);

#ifdef TIMECOUNTER_DEBUG
	factor = (delta * 1000) / denom - old_factor;
	if (factor < 0)
		factor = -factor;

	if (factor > old_factor / 10)
		printf("cc_calibrate_cpu[%u]: 10%% exceeded - delta %"
		    PRId64 ", denom %" PRId64 ", factor %" PRId64
		    ", old factor %" PRId64"\n", ci->ci_index,
		    delta, denom, (delta * 1000) / denom, old_factor);
#endif /* TIMECOUNTER_DEBUG */
}
