/*	$NetBSD: kern_clock.c,v 1.134 2015/04/22 16:46:58 pooka Exp $	*/

/*-
 * Copyright (c) 2000, 2004, 2006, 2007, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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

/*-
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)kern_clock.c	8.5 (Berkeley) 1/21/94
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_clock.c,v 1.134 2015/04/22 16:46:58 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_dtrace.h"
#include "opt_perfctrs.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/sysctl.h>
#include <sys/timex.h>
#include <sys/sched.h>
#include <sys/time.h>
#include <sys/timetc.h>
#include <sys/cpu.h>
#include <sys/atomic.h>

#ifdef GPROF
#include <sys/gmon.h>
#endif

#ifdef KDTRACE_HOOKS
#include <sys/dtrace_bsd.h>
#include <sys/cpu.h>

cyclic_clock_func_t	cyclic_clock_func[MAXCPUS];
#endif

static int sysctl_kern_clockrate(SYSCTLFN_PROTO);

/*
 * Clock handling routines.
 *
 * This code is written to operate with two timers that run independently of
 * each other.  The main clock, running hz times per second, is used to keep
 * track of real time.  The second timer handles kernel and user profiling,
 * and does resource use estimation.  If the second timer is programmable,
 * it is randomized to avoid aliasing between the two clocks.  For example,
 * the randomization prevents an adversary from always giving up the CPU
 * just before its quantum expires.  Otherwise, it would never accumulate
 * CPU ticks.  The mean frequency of the second timer is stathz.
 *
 * If no second timer exists, stathz will be zero; in this case we drive
 * profiling and statistics off the main clock.  This WILL NOT be accurate;
 * do not do it unless absolutely necessary.
 *
 * The statistics clock may (or may not) be run at a higher rate while
 * profiling.  This profile clock runs at profhz.  We require that profhz
 * be an integral multiple of stathz.
 *
 * If the statistics clock is running fast, it must be divided by the ratio
 * profhz/stathz for statistics.  (For profiling, every tick counts.)
 */

int	stathz;
int	profhz;
int	profsrc;
int	schedhz;
int	profprocs;
int	hardclock_ticks;
static int hardscheddiv; /* hard => sched divider (used if schedhz == 0) */
static int psdiv;			/* prof => stat divider */
int	psratio;			/* ratio: prof / stat */

static u_int get_intr_timecount(struct timecounter *);

static struct timecounter intr_timecounter = {
	get_intr_timecount,	/* get_timecount */
	0,			/* no poll_pps */
	~0u,			/* counter_mask */
	0,		        /* frequency */
	"clockinterrupt",	/* name */
	0,			/* quality - minimum implementation level for a clock */
	NULL,			/* prev */
	NULL,			/* next */
};

static u_int
get_intr_timecount(struct timecounter *tc)
{

	return (u_int)hardclock_ticks;
}

/*
 * Initialize clock frequencies and start both clocks running.
 */
void
initclocks(void)
{
	static struct sysctllog *clog;
	int i;

	/*
	 * Set divisors to 1 (normal case) and let the machine-specific
	 * code do its bit.
	 */
	psdiv = 1;
	/*
	 * provide minimum default time counter
	 * will only run at interrupt resolution
	 */
	intr_timecounter.tc_frequency = hz;
	tc_init(&intr_timecounter);
	cpu_initclocks();

	/*
	 * Compute profhz and stathz, fix profhz if needed.
	 */
	i = stathz ? stathz : hz;
	if (profhz == 0)
		profhz = i;
	psratio = profhz / i;
	if (schedhz == 0) {
		/* 16Hz is best */
		hardscheddiv = hz / 16;
		if (hardscheddiv <= 0)
			panic("hardscheddiv");
	}

	sysctl_createv(&clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRUCT, "clockrate",
		       SYSCTL_DESCR("Kernel clock rates"),
		       sysctl_kern_clockrate, 0, NULL,
		       sizeof(struct clockinfo),
		       CTL_KERN, KERN_CLOCKRATE, CTL_EOL);
	sysctl_createv(&clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_INT, "hardclock_ticks",
		       SYSCTL_DESCR("Number of hardclock ticks"),
		       NULL, 0, &hardclock_ticks, sizeof(hardclock_ticks),
		       CTL_KERN, KERN_HARDCLOCK_TICKS, CTL_EOL);
}

/*
 * The real-time timer, interrupting hz times per second.
 */
void
hardclock(struct clockframe *frame)
{
	struct lwp *l;
	struct cpu_info *ci;

	ci = curcpu();
	l = ci->ci_data.cpu_onproc;

	timer_tick(l, CLKF_USERMODE(frame));

	/*
	 * If no separate statistics clock is available, run it from here.
	 */
	if (stathz == 0)
		statclock(frame);
	/*
	 * If no separate schedclock is provided, call it here
	 * at about 16 Hz.
	 */
	if (schedhz == 0) {
		if ((int)(--ci->ci_schedstate.spc_schedticks) <= 0) {
			schedclock(l);
			ci->ci_schedstate.spc_schedticks = hardscheddiv;
		}
	}
	if ((--ci->ci_schedstate.spc_ticks) <= 0)
		sched_tick(ci);

	if (CPU_IS_PRIMARY(ci)) {
		hardclock_ticks++;
		tc_ticktock();
	}

	/*
	 * Update real-time timeout queue.
	 */
	callout_hardclock();

#ifdef KDTRACE_HOOKS
	cyclic_clock_func_t func = cyclic_clock_func[cpu_index(ci)];
	if (func) {
		(*func)((struct clockframe *)frame);
	}
#endif
}

/*
 * Start profiling on a process.
 *
 * Kernel profiling passes proc0 which never exits and hence
 * keeps the profile clock running constantly.
 */
void
startprofclock(struct proc *p)
{

	KASSERT(mutex_owned(&p->p_stmutex));

	if ((p->p_stflag & PST_PROFIL) == 0) {
		p->p_stflag |= PST_PROFIL;
		/*
		 * This is only necessary if using the clock as the
		 * profiling source.
		 */
		if (++profprocs == 1 && stathz != 0)
			psdiv = psratio;
	}
}

/*
 * Stop profiling on a process.
 */
void
stopprofclock(struct proc *p)
{

	KASSERT(mutex_owned(&p->p_stmutex));

	if (p->p_stflag & PST_PROFIL) {
		p->p_stflag &= ~PST_PROFIL;
		/*
		 * This is only necessary if using the clock as the
		 * profiling source.
		 */
		if (--profprocs == 0 && stathz != 0)
			psdiv = 1;
	}
}

#if defined(PERFCTRS)
/*
 * Independent profiling "tick" in case we're using a separate
 * clock or profiling event source.  Currently, that's just
 * performance counters--hence the wrapper.
 */
void
proftick(struct clockframe *frame)
{
#ifdef GPROF
        struct gmonparam *g;
        intptr_t i;
#endif
	struct lwp *l;
	struct proc *p;

	l = curcpu()->ci_data.cpu_onproc;
	p = (l ? l->l_proc : NULL);
	if (CLKF_USERMODE(frame)) {
		mutex_spin_enter(&p->p_stmutex);
		if (p->p_stflag & PST_PROFIL)
			addupc_intr(l, CLKF_PC(frame));
		mutex_spin_exit(&p->p_stmutex);
	} else {
#ifdef GPROF
		g = &_gmonparam;
		if (g->state == GMON_PROF_ON) {
			i = CLKF_PC(frame) - g->lowpc;
			if (i < g->textsize) {
				i /= HISTFRACTION * sizeof(*g->kcount);
				g->kcount[i]++;
			}
		}
#endif
#ifdef LWP_PC
		if (p != NULL && (p->p_stflag & PST_PROFIL) != 0)
			addupc_intr(l, LWP_PC(l));
#endif
	}
}
#endif

void
schedclock(struct lwp *l)
{
	if ((l->l_flag & LW_IDLE) != 0)
		return;

	sched_schedclock(l);
}

/*
 * Statistics clock.  Grab profile sample, and if divider reaches 0,
 * do process and kernel statistics.
 */
void
statclock(struct clockframe *frame)
{
#ifdef GPROF
	struct gmonparam *g;
	intptr_t i;
#endif
	struct cpu_info *ci = curcpu();
	struct schedstate_percpu *spc = &ci->ci_schedstate;
	struct proc *p;
	struct lwp *l;

	/*
	 * Notice changes in divisor frequency, and adjust clock
	 * frequency accordingly.
	 */
	if (spc->spc_psdiv != psdiv) {
		spc->spc_psdiv = psdiv;
		spc->spc_pscnt = psdiv;
		if (psdiv == 1) {
			setstatclockrate(stathz);
		} else {
			setstatclockrate(profhz);
		}
	}
	l = ci->ci_data.cpu_onproc;
	if ((l->l_flag & LW_IDLE) != 0) {
		/*
		 * don't account idle lwps as swapper.
		 */
		p = NULL;
	} else {
		p = l->l_proc;
		mutex_spin_enter(&p->p_stmutex);
	}

	if (CLKF_USERMODE(frame)) {
		if ((p->p_stflag & PST_PROFIL) && profsrc == PROFSRC_CLOCK)
			addupc_intr(l, CLKF_PC(frame));
		if (--spc->spc_pscnt > 0) {
			mutex_spin_exit(&p->p_stmutex);
			return;
		}

		/*
		 * Came from user mode; CPU was in user state.
		 * If this process is being profiled record the tick.
		 */
		p->p_uticks++;
		if (p->p_nice > NZERO)
			spc->spc_cp_time[CP_NICE]++;
		else
			spc->spc_cp_time[CP_USER]++;
	} else {
#ifdef GPROF
		/*
		 * Kernel statistics are just like addupc_intr, only easier.
		 */
		g = &_gmonparam;
		if (profsrc == PROFSRC_CLOCK && g->state == GMON_PROF_ON) {
			i = CLKF_PC(frame) - g->lowpc;
			if (i < g->textsize) {
				i /= HISTFRACTION * sizeof(*g->kcount);
				g->kcount[i]++;
			}
		}
#endif
#ifdef LWP_PC
		if (p != NULL && profsrc == PROFSRC_CLOCK &&
		    (p->p_stflag & PST_PROFIL)) {
			addupc_intr(l, LWP_PC(l));
		}
#endif
		if (--spc->spc_pscnt > 0) {
			if (p != NULL)
				mutex_spin_exit(&p->p_stmutex);
			return;
		}
		/*
		 * Came from kernel mode, so we were:
		 * - handling an interrupt,
		 * - doing syscall or trap work on behalf of the current
		 *   user process, or
		 * - spinning in the idle loop.
		 * Whichever it is, charge the time as appropriate.
		 * Note that we charge interrupts to the current process,
		 * regardless of whether they are ``for'' that process,
		 * so that we know how much of its real time was spent
		 * in ``non-process'' (i.e., interrupt) work.
		 */
		if (CLKF_INTR(frame) || (curlwp->l_pflag & LP_INTR) != 0) {
			if (p != NULL) {
				p->p_iticks++;
			}
			spc->spc_cp_time[CP_INTR]++;
		} else if (p != NULL) {
			p->p_sticks++;
			spc->spc_cp_time[CP_SYS]++;
		} else {
			spc->spc_cp_time[CP_IDLE]++;
		}
	}
	spc->spc_pscnt = psdiv;

	if (p != NULL) {
		atomic_inc_uint(&l->l_cpticks);
		mutex_spin_exit(&p->p_stmutex);
	}
}

/*
 * sysctl helper routine for kern.clockrate. Assembles a struct on
 * the fly to be returned to the caller.
 */
static int
sysctl_kern_clockrate(SYSCTLFN_ARGS)
{
	struct clockinfo clkinfo;
	struct sysctlnode node;

	clkinfo.tick = tick;
	clkinfo.tickadj = tickadj;
	clkinfo.hz = hz;
	clkinfo.profhz = profhz;
	clkinfo.stathz = stathz ? stathz : hz;

	node = *rnode;
	node.sysctl_data = &clkinfo;
	return (sysctl_lookup(SYSCTLFN_CALL(&node)));
}
