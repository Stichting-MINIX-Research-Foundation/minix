/*	$NetBSD: sched_m2.c,v 1.32 2014/06/24 10:08:45 maxv Exp $	*/

/*
 * Copyright (c) 2007, 2008 Mindaugas Rasiukevicius <rmind at NetBSD org>
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

/*
 * TODO:
 *  - Implementation of fair share queue;
 *  - Support for NUMA;
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sched_m2.c,v 1.32 2014/06/24 10:08:45 maxv Exp $");

#include <sys/param.h>

#include <sys/cpu.h>
#include <sys/callout.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/lwp.h>
#include <sys/mutex.h>
#include <sys/pool.h>
#include <sys/proc.h>
#include <sys/pset.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/syscallargs.h>
#include <sys/sysctl.h>
#include <sys/types.h>

/*
 * Priority related definitions.
 */
#define	PRI_TS_COUNT	(NPRI_USER)
#define	PRI_RT_COUNT	(PRI_COUNT - PRI_TS_COUNT)
#define	PRI_HTS_RANGE	(PRI_TS_COUNT / 10)

#define	PRI_HIGHEST_TS	(MAXPRI_USER)

/*
 * Time-slices and priorities.
 */
static u_int	min_ts;			/* Minimal time-slice */
static u_int	max_ts;			/* Maximal time-slice */
static u_int	rt_ts;			/* Real-time time-slice */
static u_int	ts_map[PRI_COUNT];	/* Map of time-slices */
static pri_t	high_pri[PRI_COUNT];	/* Map for priority increase */

static void	sched_precalcts(void);

/*
 * Initialization and setup.
 */

void
sched_rqinit(void)
{
	if (hz < 100) {
		panic("sched_rqinit: value of HZ is too low\n");
	}

	/* Default timing ranges */
	min_ts = mstohz(20);			/*  ~20 ms */
	max_ts = mstohz(150);			/* ~150 ms */
	rt_ts = mstohz(100);			/* ~100 ms */
	sched_precalcts();

#ifdef notdef
	/* Need to set the name etc. This does not belong here */
	/* Attach the primary CPU here */
	sched_cpuattach(curcpu());
#endif

	sched_lwp_fork(NULL, &lwp0);
	sched_newts(&lwp0);
}

/* Pre-calculate the time-slices for the priorities */
static void
sched_precalcts(void)
{
	pri_t p;

	/* Time-sharing range */
	for (p = 0; p <= PRI_HIGHEST_TS; p++) {
		ts_map[p] = max_ts -
		    (p * 100 / (PRI_TS_COUNT - 1) * (max_ts - min_ts) / 100);
		high_pri[p] = (PRI_HIGHEST_TS - PRI_HTS_RANGE) +
		    ((p * PRI_HTS_RANGE) / (PRI_TS_COUNT - 1));
	}

	/* Real-time range */
	for (p = (PRI_HIGHEST_TS + 1); p < PRI_COUNT; p++) {
		ts_map[p] = rt_ts;
		high_pri[p] = p;
	}
}

/*
 * Hooks.
 */

void
sched_proc_fork(struct proc *parent, struct proc *child)
{
	struct lwp *l;

	LIST_FOREACH(l, &child->p_lwps, l_sibling) {
		lwp_lock(l);
		sched_newts(l);
		lwp_unlock(l);
	}
}

void
sched_proc_exit(struct proc *child, struct proc *parent)
{

}

void
sched_lwp_fork(struct lwp *l1, struct lwp *l2)
{

}

void
sched_lwp_collect(struct lwp *l)
{

}

void
sched_setrunnable(struct lwp *l)
{

}

void
sched_schedclock(struct lwp *l)
{

}

/*
 * Priorities and time-slice.
 */

void
sched_nice(struct proc *p, int prio)
{
	struct lwp *l;
	int n;

	KASSERT(mutex_owned(p->p_lock));

	p->p_nice = prio;
	n = (prio - NZERO) >> 2;
	if (n == 0)
		return;

	LIST_FOREACH(l, &p->p_lwps, l_sibling) {
		lwp_lock(l);
		if (l->l_class == SCHED_OTHER) {
			pri_t pri = l->l_priority - n;
			pri = (n < 0) ? min(pri, PRI_HIGHEST_TS) : imax(pri, 0);
			lwp_changepri(l, pri);
		}
		lwp_unlock(l);
	}
}

/* Recalculate the time-slice */
void
sched_newts(struct lwp *l)
{

	l->l_sched.timeslice = ts_map[lwp_eprio(l)];
}

void
sched_slept(struct lwp *l)
{

	/*
	 * If thread is in time-sharing queue and batch flag is not marked,
	 * increase the priority, and run with the lower time-quantum.
	 */
	if (l->l_priority < PRI_HIGHEST_TS && (l->l_flag & LW_BATCH) == 0) {
		struct proc *p = l->l_proc;

		KASSERT(l->l_class == SCHED_OTHER);
		if (__predict_false(p->p_nice < NZERO)) {
			const int n = max((NZERO - p->p_nice) >> 2, 1);
			l->l_priority = min(l->l_priority + n, PRI_HIGHEST_TS);
		} else {
			l->l_priority++;
		}
	}
}

void
sched_wakeup(struct lwp *l)
{

	/* If thread was sleeping a second or more - set a high priority */
	if (l->l_slptime >= 1)
		l->l_priority = high_pri[l->l_priority];
}

void
sched_pstats_hook(struct lwp *l, int batch)
{
	pri_t prio;

	/*
	 * Estimate threads on time-sharing queue only, however,
	 * exclude the highest priority for performance purposes.
	 */
	KASSERT(lwp_locked(l, NULL));
	if (l->l_priority >= PRI_HIGHEST_TS)
		return;
	KASSERT(l->l_class == SCHED_OTHER);

	/* If it is CPU-bound not a first time - decrease the priority */
	prio = l->l_priority;
	if (batch && prio != 0)
		prio--;

	/* If thread was not ran a second or more - set a high priority */
	if (l->l_stat == LSRUN) {
		if (l->l_rticks && (hardclock_ticks - l->l_rticks >= hz))
			prio = high_pri[prio];
		/* Re-enqueue the thread if priority has changed */
		if (prio != l->l_priority)
			lwp_changepri(l, prio);
	} else {
		/* In other states, change the priority directly */
		l->l_priority = prio;
	}
}

void
sched_oncpu(lwp_t *l)
{
	struct schedstate_percpu *spc = &l->l_cpu->ci_schedstate;

	/* Update the counters */
	KASSERT(l->l_sched.timeslice >= min_ts);
	KASSERT(l->l_sched.timeslice <= max_ts);
	spc->spc_ticks = l->l_sched.timeslice;
}

/*
 * Time-driven events.
 */

/*
 * Called once per time-quantum.  This routine is CPU-local and runs at
 * IPL_SCHED, thus the locking is not needed.
 */
void
sched_tick(struct cpu_info *ci)
{
	struct schedstate_percpu *spc = &ci->ci_schedstate;
	struct lwp *l = curlwp;
	struct proc *p;

	if (__predict_false(CURCPU_IDLE_P()))
		return;

	switch (l->l_class) {
	case SCHED_FIFO:
		/*
		 * Update the time-quantum, and continue running,
		 * if thread runs on FIFO real-time policy.
		 */
		KASSERT(l->l_priority > PRI_HIGHEST_TS);
		spc->spc_ticks = l->l_sched.timeslice;
		return;
	case SCHED_OTHER:
		/*
		 * If thread is in time-sharing queue, decrease the priority,
		 * and run with a higher time-quantum.
		 */
		KASSERT(l->l_priority <= PRI_HIGHEST_TS);
		if (l->l_priority == 0)
			break;

		p = l->l_proc;
		if (__predict_false(p->p_nice > NZERO)) {
			const int n = max((p->p_nice - NZERO) >> 2, 1);
			l->l_priority = imax(l->l_priority - n, 0);
		} else
			l->l_priority--;
		break;
	}

	/*
	 * If there are higher priority threads or threads in the same queue,
	 * mark that thread should yield, otherwise, continue running.
	 */
	if (lwp_eprio(l) <= spc->spc_maxpriority || l->l_target_cpu) {
		spc->spc_flags |= SPCF_SHOULDYIELD;
		cpu_need_resched(ci, 0);
	} else
		spc->spc_ticks = l->l_sched.timeslice;
}

/*
 * Sysctl nodes and initialization.
 */

static int
sysctl_sched_rtts(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	int rttsms = hztoms(rt_ts);

	node = *rnode;
	node.sysctl_data = &rttsms;
	return sysctl_lookup(SYSCTLFN_CALL(&node));
}

static int
sysctl_sched_mints(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	struct cpu_info *ci;
	int error, newsize;
	CPU_INFO_ITERATOR cii;

	node = *rnode;
	node.sysctl_data = &newsize;

	newsize = hztoms(min_ts);
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	newsize = mstohz(newsize);
	if (newsize < 1 || newsize > hz || newsize >= max_ts)
		return EINVAL;

	/* It is safe to do this in such order */
	for (CPU_INFO_FOREACH(cii, ci))
		spc_lock(ci);

	min_ts = newsize;
	sched_precalcts();

	for (CPU_INFO_FOREACH(cii, ci))
		spc_unlock(ci);

	return 0;
}

static int
sysctl_sched_maxts(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	struct cpu_info *ci;
	int error, newsize;
	CPU_INFO_ITERATOR cii;

	node = *rnode;
	node.sysctl_data = &newsize;

	newsize = hztoms(max_ts);
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	newsize = mstohz(newsize);
	if (newsize < 10 || newsize > hz || newsize <= min_ts)
		return EINVAL;

	/* It is safe to do this in such order */
	for (CPU_INFO_FOREACH(cii, ci))
		spc_lock(ci);

	max_ts = newsize;
	sched_precalcts();

	for (CPU_INFO_FOREACH(cii, ci))
		spc_unlock(ci);

	return 0;
}

SYSCTL_SETUP(sysctl_sched_m2_setup, "sysctl sched setup")
{
	const struct sysctlnode *node = NULL;

	sysctl_createv(clog, 0, NULL, &node,
		CTLFLAG_PERMANENT,
		CTLTYPE_NODE, "sched",
		SYSCTL_DESCR("Scheduler options"),
		NULL, 0, NULL, 0,
		CTL_KERN, CTL_CREATE, CTL_EOL);

	if (node == NULL)
		return;

	sysctl_createv(NULL, 0, &node, NULL,
		CTLFLAG_PERMANENT,
		CTLTYPE_STRING, "name", NULL,
		NULL, 0, __UNCONST("M2"), 0,
		CTL_CREATE, CTL_EOL);
	sysctl_createv(NULL, 0, &node, NULL,
		CTLFLAG_PERMANENT,
		CTLTYPE_INT, "rtts",
		SYSCTL_DESCR("Round-robin time quantum (in milliseconds)"),
		sysctl_sched_rtts, 0, NULL, 0,
		CTL_CREATE, CTL_EOL);
	sysctl_createv(NULL, 0, &node, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "maxts",
		SYSCTL_DESCR("Maximal time quantum (in milliseconds)"),
		sysctl_sched_maxts, 0, &max_ts, 0,
		CTL_CREATE, CTL_EOL);
	sysctl_createv(NULL, 0, &node, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "mints",
		SYSCTL_DESCR("Minimal time quantum (in milliseconds)"),
		sysctl_sched_mints, 0, &min_ts, 0,
		CTL_CREATE, CTL_EOL);
}
