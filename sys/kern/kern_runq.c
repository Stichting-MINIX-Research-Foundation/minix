/*	$NetBSD: kern_runq.c,v 1.44 2015/10/07 00:32:34 christos Exp $	*/

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_runq.c,v 1.44 2015/10/07 00:32:34 christos Exp $");

#include "opt_dtrace.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bitops.h>
#include <sys/cpu.h>
#include <sys/idle.h>
#include <sys/intr.h>
#include <sys/kmem.h>
#include <sys/lwp.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/syscallargs.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/evcnt.h>

/*
 * Priority related definitions.
 */
#define	PRI_TS_COUNT	(NPRI_USER)
#define	PRI_RT_COUNT	(PRI_COUNT - PRI_TS_COUNT)
#define	PRI_HTS_RANGE	(PRI_TS_COUNT / 10)

#define	PRI_HIGHEST_TS	(MAXPRI_USER)

/*
 * Bits per map.
 */
#define	BITMAP_BITS	(32)
#define	BITMAP_SHIFT	(5)
#define	BITMAP_MSB	(0x80000000U)
#define	BITMAP_MASK	(BITMAP_BITS - 1)

/*
 * Structures, runqueue.
 */

const int	schedppq = 1;

typedef struct {
	TAILQ_HEAD(, lwp) q_head;
} queue_t;

typedef struct {
	/* Bitmap */
	uint32_t	r_bitmap[PRI_COUNT >> BITMAP_SHIFT];
	/* Counters */
	u_int		r_count;	/* Count of the threads */
	u_int		r_avgcount;	/* Average count of threads */
	u_int		r_mcount;	/* Count of migratable threads */
	/* Runqueues */
	queue_t		r_rt_queue[PRI_RT_COUNT];
	queue_t		r_ts_queue[PRI_TS_COUNT];
	/* Event counters */
	struct evcnt	r_ev_pull;
	struct evcnt	r_ev_push;
	struct evcnt	r_ev_stay;
	struct evcnt	r_ev_localize;
} runqueue_t;

static void *	sched_getrq(runqueue_t *, const pri_t);
#ifdef MULTIPROCESSOR
static lwp_t *	sched_catchlwp(struct cpu_info *);
static void	sched_balance(void *);
#endif

/*
 * Preemption control.
 */
int		sched_upreempt_pri = 0;
#ifdef __HAVE_PREEMPTION
# ifdef DEBUG
int		sched_kpreempt_pri = 0;
# else
int		sched_kpreempt_pri = PRI_USER_RT;
# endif
#else
int		sched_kpreempt_pri = 1000;
#endif

/*
 * Migration and balancing.
 */
static u_int	cacheht_time;		/* Cache hotness time */
static u_int	min_catch;		/* Minimal LWP count for catching */
static u_int	balance_period;		/* Balance period */
static struct cpu_info *worker_ci;	/* Victim CPU */
#ifdef MULTIPROCESSOR
static struct callout balance_ch;	/* Callout of balancer */
#endif

#ifdef KDTRACE_HOOKS
struct lwp *curthread;
#endif

void
runq_init(void)
{

	/* Balancing */
	worker_ci = curcpu();
	cacheht_time = mstohz(3);		/*   ~3 ms */
	balance_period = mstohz(300);		/* ~300 ms */

	/* Minimal count of LWPs for catching */
	min_catch = 1;

	/* Initialize balancing callout and run it */
#ifdef MULTIPROCESSOR
	callout_init(&balance_ch, CALLOUT_MPSAFE);
	callout_setfunc(&balance_ch, sched_balance, NULL);
	callout_schedule(&balance_ch, balance_period);
#endif
}

void
sched_cpuattach(struct cpu_info *ci)
{
	runqueue_t *ci_rq;
	void *rq_ptr;
	u_int i, size;

	if (ci->ci_schedstate.spc_lwplock == NULL) {
		ci->ci_schedstate.spc_lwplock =
		    mutex_obj_alloc(MUTEX_DEFAULT, IPL_SCHED);
	}
	if (ci == lwp0.l_cpu) {
		/* Initialize the scheduler structure of the primary LWP */
		lwp0.l_mutex = ci->ci_schedstate.spc_lwplock;
	}
	if (ci->ci_schedstate.spc_mutex != NULL) {
		/* Already initialized. */
		return;
	}

	/* Allocate the run queue */
	size = roundup2(sizeof(runqueue_t), coherency_unit) + coherency_unit;
	rq_ptr = kmem_zalloc(size, KM_SLEEP);
	if (rq_ptr == NULL) {
		panic("sched_cpuattach: could not allocate the runqueue");
	}
	ci_rq = (void *)(roundup2((uintptr_t)(rq_ptr), coherency_unit));

	/* Initialize run queues */
	ci->ci_schedstate.spc_mutex =
	    mutex_obj_alloc(MUTEX_DEFAULT, IPL_SCHED);
	for (i = 0; i < PRI_RT_COUNT; i++)
		TAILQ_INIT(&ci_rq->r_rt_queue[i].q_head);
	for (i = 0; i < PRI_TS_COUNT; i++)
		TAILQ_INIT(&ci_rq->r_ts_queue[i].q_head);

	ci->ci_schedstate.spc_sched_info = ci_rq;

	evcnt_attach_dynamic(&ci_rq->r_ev_pull, EVCNT_TYPE_MISC, NULL,
	   cpu_name(ci), "runqueue pull");
	evcnt_attach_dynamic(&ci_rq->r_ev_push, EVCNT_TYPE_MISC, NULL,
	   cpu_name(ci), "runqueue push");
	evcnt_attach_dynamic(&ci_rq->r_ev_stay, EVCNT_TYPE_MISC, NULL,
	   cpu_name(ci), "runqueue stay");
	evcnt_attach_dynamic(&ci_rq->r_ev_localize, EVCNT_TYPE_MISC, NULL,
	   cpu_name(ci), "runqueue localize");
}

/*
 * Control of the runqueue.
 */

static inline void *
sched_getrq(runqueue_t *ci_rq, const pri_t prio)
{

	KASSERT(prio < PRI_COUNT);
	return (prio <= PRI_HIGHEST_TS) ?
	    &ci_rq->r_ts_queue[prio].q_head :
	    &ci_rq->r_rt_queue[prio - PRI_HIGHEST_TS - 1].q_head;
}

void
sched_enqueue(struct lwp *l, bool swtch)
{
	runqueue_t *ci_rq;
	struct schedstate_percpu *spc;
	TAILQ_HEAD(, lwp) *q_head;
	const pri_t eprio = lwp_eprio(l);
	struct cpu_info *ci;
	int type;

	ci = l->l_cpu;
	spc = &ci->ci_schedstate;
	ci_rq = spc->spc_sched_info;
	KASSERT(lwp_locked(l, l->l_cpu->ci_schedstate.spc_mutex));

	/* Update the last run time on switch */
	if (__predict_true(swtch == true))
		l->l_rticksum += (hardclock_ticks - l->l_rticks);
	else if (l->l_rticks == 0)
		l->l_rticks = hardclock_ticks;

	/* Enqueue the thread */
	q_head = sched_getrq(ci_rq, eprio);
	if (TAILQ_EMPTY(q_head)) {
		u_int i;
		uint32_t q;

		/* Mark bit */
		i = eprio >> BITMAP_SHIFT;
		q = BITMAP_MSB >> (eprio & BITMAP_MASK);
		KASSERT((ci_rq->r_bitmap[i] & q) == 0);
		ci_rq->r_bitmap[i] |= q;
	}
	TAILQ_INSERT_TAIL(q_head, l, l_runq);
	ci_rq->r_count++;
	if ((l->l_pflag & LP_BOUND) == 0)
		ci_rq->r_mcount++;

	/*
	 * Update the value of highest priority in the runqueue,
	 * if priority of this thread is higher.
	 */
	if (eprio > spc->spc_maxpriority)
		spc->spc_maxpriority = eprio;

	sched_newts(l);

	/*
	 * Wake the chosen CPU or cause a preemption if the newly
	 * enqueued thread has higher priority.  Don't cause a 
	 * preemption if the thread is yielding (swtch).
	 */
	if (!swtch && eprio > spc->spc_curpriority) {
		if (eprio >= sched_kpreempt_pri)
			type = RESCHED_KPREEMPT;
		else if (eprio >= sched_upreempt_pri)
			type = RESCHED_IMMED;
		else
			type = RESCHED_LAZY;
		cpu_need_resched(ci, type);
	}
}

void
sched_dequeue(struct lwp *l)
{
	runqueue_t *ci_rq;
	TAILQ_HEAD(, lwp) *q_head;
	struct schedstate_percpu *spc;
	const pri_t eprio = lwp_eprio(l);

	spc = & l->l_cpu->ci_schedstate;
	ci_rq = spc->spc_sched_info;
	KASSERT(lwp_locked(l, spc->spc_mutex));

	KASSERT(eprio <= spc->spc_maxpriority); 
	KASSERT(ci_rq->r_bitmap[eprio >> BITMAP_SHIFT] != 0);
	KASSERT(ci_rq->r_count > 0);

	if (spc->spc_migrating == l)
		spc->spc_migrating = NULL;

	ci_rq->r_count--;
	if ((l->l_pflag & LP_BOUND) == 0)
		ci_rq->r_mcount--;

	q_head = sched_getrq(ci_rq, eprio);
	TAILQ_REMOVE(q_head, l, l_runq);
	if (TAILQ_EMPTY(q_head)) {
		u_int i;
		uint32_t q;

		/* Unmark bit */
		i = eprio >> BITMAP_SHIFT;
		q = BITMAP_MSB >> (eprio & BITMAP_MASK);
		KASSERT((ci_rq->r_bitmap[i] & q) != 0);
		ci_rq->r_bitmap[i] &= ~q;

		/*
		 * Update the value of highest priority in the runqueue, in a
		 * case it was a last thread in the queue of highest priority.
		 */
		if (eprio != spc->spc_maxpriority)
			return;

		do {
			if (ci_rq->r_bitmap[i] != 0) {
				q = ffs(ci_rq->r_bitmap[i]);
				spc->spc_maxpriority =
				    (i << BITMAP_SHIFT) + (BITMAP_BITS - q);
				return;
			}
		} while (i--);

		/* If not found - set the lowest value */
		spc->spc_maxpriority = 0;
	}
}

/*
 * Migration and balancing.
 */

#ifdef MULTIPROCESSOR

/* Estimate if LWP is cache-hot */
static inline bool
lwp_cache_hot(const struct lwp *l)
{

	if (__predict_false(l->l_slptime || l->l_rticks == 0))
		return false;

	return (hardclock_ticks - l->l_rticks <= cacheht_time);
}

/* Check if LWP can migrate to the chosen CPU */
static inline bool
sched_migratable(const struct lwp *l, struct cpu_info *ci)
{
	const struct schedstate_percpu *spc = &ci->ci_schedstate;
	KASSERT(lwp_locked(__UNCONST(l), NULL));

	/* Is CPU offline? */
	if (__predict_false(spc->spc_flags & SPCF_OFFLINE))
		return false;

	/* Is affinity set? */
	if (__predict_false(l->l_affinity))
		return kcpuset_isset(l->l_affinity, cpu_index(ci));

	/* Is there a processor-set? */
	return (spc->spc_psid == l->l_psid);
}

/*
 * Estimate the migration of LWP to the other CPU.
 * Take and return the CPU, if migration is needed.
 */
struct cpu_info *
sched_takecpu(struct lwp *l)
{
	struct cpu_info *ci, *tci, *pivot, *next;
	struct schedstate_percpu *spc;
	runqueue_t *ci_rq, *ici_rq;
	pri_t eprio, lpri, pri;

	KASSERT(lwp_locked(l, NULL));

	/* If thread is strictly bound, do not estimate other CPUs */
	ci = l->l_cpu;
	if (l->l_pflag & LP_BOUND)
		return ci;

	spc = &ci->ci_schedstate;
	ci_rq = spc->spc_sched_info;

	/* Make sure that thread is in appropriate processor-set */
	if (__predict_true(spc->spc_psid == l->l_psid)) {
		/* If CPU of this thread is idling - run there */
		if (ci_rq->r_count == 0) {
			ci_rq->r_ev_stay.ev_count++;
			return ci;
		}
		/* Stay if thread is cache-hot */
		eprio = lwp_eprio(l);
		if (__predict_true(l->l_stat != LSIDL) &&
		    lwp_cache_hot(l) && eprio >= spc->spc_curpriority) {
			ci_rq->r_ev_stay.ev_count++;
			return ci;
		}
	} else {
		eprio = lwp_eprio(l);
	}

	/* Run on current CPU if priority of thread is higher */
	ci = curcpu();
	spc = &ci->ci_schedstate;
	if (eprio > spc->spc_curpriority && sched_migratable(l, ci)) {
		ci_rq = spc->spc_sched_info;
		ci_rq->r_ev_localize.ev_count++;
		return ci;
	}

	/*
	 * Look for the CPU with the lowest priority thread.  In case of
	 * equal priority, choose the CPU with the fewest of threads.
	 */
	pivot = l->l_cpu;
	ci = pivot;
	tci = pivot;
	lpri = PRI_COUNT;
	do {
		if ((next = cpu_lookup(cpu_index(ci) + 1)) == NULL) {
			/* Reached the end, start from the beginning. */
			next = cpu_lookup(0);
		}
		spc = &ci->ci_schedstate;
		ici_rq = spc->spc_sched_info;
		pri = MAX(spc->spc_curpriority, spc->spc_maxpriority);
		if (pri > lpri)
			continue;

		if (pri == lpri && ci_rq->r_count < ici_rq->r_count)
			continue;

		if (!sched_migratable(l, ci))
			continue;

		lpri = pri;
		tci = ci;
		ci_rq = ici_rq;
	} while (ci = next, ci != pivot);

	ci_rq = tci->ci_schedstate.spc_sched_info;
	ci_rq->r_ev_push.ev_count++;

	return tci;
}

/*
 * Tries to catch an LWP from the runqueue of other CPU.
 */
static struct lwp *
sched_catchlwp(struct cpu_info *ci)
{
	struct cpu_info *curci = curcpu();
	struct schedstate_percpu *spc, *curspc;
	TAILQ_HEAD(, lwp) *q_head;
	runqueue_t *ci_rq;
	struct lwp *l;

	curspc = &curci->ci_schedstate;
	spc = &ci->ci_schedstate;
	KASSERT(curspc->spc_psid == spc->spc_psid);

	ci_rq = spc->spc_sched_info;
	if (ci_rq->r_mcount < min_catch) {
		spc_unlock(ci);
		return NULL;
	}

	/* Take the highest priority thread */
	q_head = sched_getrq(ci_rq, spc->spc_maxpriority);
	l = TAILQ_FIRST(q_head);

	for (;;) {
		/* Check the first and next result from the queue */
		if (l == NULL) {
			break;
		}
		KASSERTMSG(l->l_stat == LSRUN, "%s l %p (%s) l_stat %d",
		    ci->ci_data.cpu_name,
		    l, (l->l_name ? l->l_name : l->l_proc->p_comm), l->l_stat);

		/* Look for threads, whose are allowed to migrate */
		if ((l->l_pflag & LP_BOUND) || lwp_cache_hot(l) ||
		    !sched_migratable(l, curci)) {
			l = TAILQ_NEXT(l, l_runq);
			continue;
		}

		/* Grab the thread, and move to the local run queue */
		sched_dequeue(l);

		/*
		 * If LWP is still context switching, we may need to
		 * spin-wait before changing its CPU.
		 */
		if (__predict_false(l->l_ctxswtch != 0)) {
			u_int count;
			count = SPINLOCK_BACKOFF_MIN;
			while (l->l_ctxswtch)
				SPINLOCK_BACKOFF(count);
		}
		l->l_cpu = curci;
		ci_rq->r_ev_pull.ev_count++;
		lwp_unlock_to(l, curspc->spc_mutex);
		sched_enqueue(l, false);
		return l;
	}
	spc_unlock(ci);

	return l;
}

/*
 * Periodical calculations for balancing.
 */
static void
sched_balance(void *nocallout)
{
	struct cpu_info *ci, *hci;
	runqueue_t *ci_rq;
	CPU_INFO_ITERATOR cii;
	u_int highest;

	hci = curcpu();
	highest = 0;

	/* Make lockless countings */
	for (CPU_INFO_FOREACH(cii, ci)) {
		ci_rq = ci->ci_schedstate.spc_sched_info;

		/* Average count of the threads */
		ci_rq->r_avgcount = (ci_rq->r_avgcount + ci_rq->r_mcount) >> 1;

		/* Look for CPU with the highest average */
		if (ci_rq->r_avgcount > highest) {
			hci = ci;
			highest = ci_rq->r_avgcount;
		}
	}

	/* Update the worker */
	worker_ci = hci;

	if (nocallout == NULL)
		callout_schedule(&balance_ch, balance_period);
}

/*
 * Called from each CPU's idle loop.
 */
void
sched_idle(void)
{
	struct cpu_info *ci = curcpu(), *tci = NULL;
	struct schedstate_percpu *spc, *tspc;
	runqueue_t *ci_rq;
	bool dlock = false;

	/* Check if there is a migrating LWP */
	spc = &ci->ci_schedstate;
	if (spc->spc_migrating == NULL)
		goto no_migration;

	spc_lock(ci);
	for (;;) {
		struct lwp *l;

		l = spc->spc_migrating;
		if (l == NULL)
			break;

		/*
		 * If second attempt, and target CPU has changed,
		 * drop the old lock.
		 */
		if (dlock == true && tci != l->l_target_cpu) {
			KASSERT(tci != NULL);
			spc_unlock(tci);
			dlock = false;
		}

		/*
		 * Nothing to do if destination has changed to the
		 * local CPU, or migration was done by other CPU.
		 */
		tci = l->l_target_cpu;
		if (tci == NULL || tci == ci) {
			spc->spc_migrating = NULL;
			l->l_target_cpu = NULL;
			break;
		}
		tspc = &tci->ci_schedstate;

		/*
		 * Double-lock the runqueues.
		 * We do that only once.
		 */
		if (dlock == false) {
			dlock = true;
			if (ci < tci) {
				spc_lock(tci);
			} else if (!mutex_tryenter(tspc->spc_mutex)) {
				spc_unlock(ci);
				spc_lock(tci);
				spc_lock(ci);
				/* Check the situation again.. */
				continue;
			}
		}

		/* Migrate the thread */
		KASSERT(l->l_stat == LSRUN);
		spc->spc_migrating = NULL;
		l->l_target_cpu = NULL;
		sched_dequeue(l);
		l->l_cpu = tci;
		lwp_setlock(l, tspc->spc_mutex);
		sched_enqueue(l, false);
		break;
	}
	if (dlock == true) {
		KASSERT(tci != NULL);
		spc_unlock(tci);
	}
	spc_unlock(ci);

no_migration:
	ci_rq = spc->spc_sched_info;
	if ((spc->spc_flags & SPCF_OFFLINE) != 0 || ci_rq->r_count != 0) {
		return;
	}

	/* Reset the counter, and call the balancer */
	ci_rq->r_avgcount = 0;
	sched_balance(ci);
	tci = worker_ci;
	tspc = &tci->ci_schedstate;
	if (ci == tci || spc->spc_psid != tspc->spc_psid)
		return;
	spc_dlock(ci, tci);
	(void)sched_catchlwp(tci);
	spc_unlock(ci);
}

#else

/*
 * stubs for !MULTIPROCESSOR
 */

struct cpu_info *
sched_takecpu(struct lwp *l)
{

	return l->l_cpu;
}

void
sched_idle(void)
{

}
#endif	/* MULTIPROCESSOR */

/*
 * Scheduling statistics and balancing.
 */
void
sched_lwp_stats(struct lwp *l)
{
	int batch;

	KASSERT(lwp_locked(l, NULL));

	/* Update sleep time */
	if (l->l_stat == LSSLEEP || l->l_stat == LSSTOP ||
	    l->l_stat == LSSUSPENDED)
		l->l_slptime++;

	/*
	 * Set that thread is more CPU-bound, if sum of run time exceeds the
	 * sum of sleep time.  Check if thread is CPU-bound a first time.
	 */
	batch = (l->l_rticksum > l->l_slpticksum);
	if (batch != 0) {
		if ((l->l_flag & LW_BATCH) == 0)
			batch = 0;
		l->l_flag |= LW_BATCH;
	} else
		l->l_flag &= ~LW_BATCH;

	/*
	 * If thread is CPU-bound and never sleeps, it would occupy the CPU.
	 * In such case reset the value of last sleep, and check it later, if
	 * it is still zero - perform the migration, unmark the batch flag.
	 */
	if (batch && (l->l_slptime + l->l_slpticksum) == 0) {
		if (l->l_slpticks == 0) {
			if (l->l_target_cpu == NULL &&
			    (l->l_stat == LSRUN || l->l_stat == LSONPROC)) {
				struct cpu_info *ci = sched_takecpu(l);
				l->l_target_cpu = (ci != l->l_cpu) ? ci : NULL;
			}
			l->l_flag &= ~LW_BATCH;
		} else {
			l->l_slpticks = 0;
		}
	}

	/* Reset the time sums */
	l->l_slpticksum = 0;
	l->l_rticksum = 0;

	/* Scheduler-specific hook */
	sched_pstats_hook(l, batch);
#ifdef KDTRACE_HOOKS
	curthread = l;
#endif
}

/*
 * Scheduler mill.
 */
struct lwp *
sched_nextlwp(void)
{
	struct cpu_info *ci = curcpu();
	struct schedstate_percpu *spc;
	TAILQ_HEAD(, lwp) *q_head;
	runqueue_t *ci_rq;
	struct lwp *l;

	/* Return to idle LWP if there is a migrating thread */
	spc = &ci->ci_schedstate;
	if (__predict_false(spc->spc_migrating != NULL))
		return NULL;
	ci_rq = spc->spc_sched_info;

#ifdef MULTIPROCESSOR
	/* If runqueue is empty, try to catch some thread from other CPU */
	if (__predict_false(ci_rq->r_count == 0)) {
		struct schedstate_percpu *cspc;
		struct cpu_info *cci;

		/* Offline CPUs should not perform this, however */
		if (__predict_false(spc->spc_flags & SPCF_OFFLINE))
			return NULL;

		/* Reset the counter, and call the balancer */
		ci_rq->r_avgcount = 0;
		sched_balance(ci);
		cci = worker_ci;
		cspc = &cci->ci_schedstate;
		if (ci == cci || spc->spc_psid != cspc->spc_psid ||
		    !mutex_tryenter(cci->ci_schedstate.spc_mutex))
			return NULL;
		return sched_catchlwp(cci);
	}
#else
	if (__predict_false(ci_rq->r_count == 0))
		return NULL;
#endif

	/* Take the highest priority thread */
	KASSERT(ci_rq->r_bitmap[spc->spc_maxpriority >> BITMAP_SHIFT]);
	q_head = sched_getrq(ci_rq, spc->spc_maxpriority);
	l = TAILQ_FIRST(q_head);
	KASSERT(l != NULL);

	sched_oncpu(l);
	l->l_rticks = hardclock_ticks;

	return l;
}

/*
 * sched_curcpu_runnable_p: return if curcpu() should exit the idle loop.
 */

bool
sched_curcpu_runnable_p(void)
{
	const struct cpu_info *ci;
	const struct schedstate_percpu *spc;
	const runqueue_t *ci_rq;
	bool rv;

	kpreempt_disable();
	ci = curcpu();
	spc = &ci->ci_schedstate;
	ci_rq = spc->spc_sched_info;

#ifndef __HAVE_FAST_SOFTINTS
	if (ci->ci_data.cpu_softints) {
		kpreempt_enable();
		return true;
	}
#endif

	rv = (ci_rq->r_count != 0) ? true : false;
	kpreempt_enable();

	return rv;
}

/*
 * Sysctl nodes and initialization.
 */

SYSCTL_SETUP(sysctl_sched_setup, "sysctl sched setup")
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

	sysctl_createv(clog, 0, &node, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "cacheht_time",
		SYSCTL_DESCR("Cache hotness time (in ticks)"),
		NULL, 0, &cacheht_time, 0,
		CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &node, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "balance_period",
		SYSCTL_DESCR("Balance period (in ticks)"),
		NULL, 0, &balance_period, 0,
		CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &node, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "min_catch",
		SYSCTL_DESCR("Minimal count of threads for catching"),
		NULL, 0, &min_catch, 0,
		CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &node, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "timesoftints",
		SYSCTL_DESCR("Track CPU time for soft interrupts"),
		NULL, 0, &softint_timing, 0,
		CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &node, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "kpreempt_pri",
		SYSCTL_DESCR("Minimum priority to trigger kernel preemption"),
		NULL, 0, &sched_kpreempt_pri, 0,
		CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &node, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "upreempt_pri",
		SYSCTL_DESCR("Minimum priority to trigger user preemption"),
		NULL, 0, &sched_upreempt_pri, 0,
		CTL_CREATE, CTL_EOL);
}

/*
 * Debugging.
 */

#ifdef DDB

void
sched_print_runqueue(void (*pr)(const char *, ...))
{
	runqueue_t *ci_rq;
	struct cpu_info *ci, *tci;
	struct schedstate_percpu *spc;
	struct lwp *l;
	struct proc *p;
	CPU_INFO_ITERATOR cii;

	for (CPU_INFO_FOREACH(cii, ci)) {
		int i;

		spc = &ci->ci_schedstate;
		ci_rq = spc->spc_sched_info;

		(*pr)("Run-queue (CPU = %u):\n", ci->ci_index);
		(*pr)(" pid.lid = %d.%d, r_count = %u, r_avgcount = %u, "
		    "maxpri = %d, mlwp = %p\n",
#ifdef MULTIPROCESSOR
		    ci->ci_curlwp->l_proc->p_pid, ci->ci_curlwp->l_lid,
#else
		    curlwp->l_proc->p_pid, curlwp->l_lid,
#endif
		    ci_rq->r_count, ci_rq->r_avgcount, spc->spc_maxpriority,
		    spc->spc_migrating);
		i = (PRI_COUNT >> BITMAP_SHIFT) - 1;
		do {
			uint32_t q;
			q = ci_rq->r_bitmap[i];
			(*pr)(" bitmap[%d] => [ %d (0x%x) ]\n", i, ffs(q), q);
		} while (i--);
	}

	(*pr)("   %5s %4s %4s %10s %3s %18s %4s %4s %s\n",
	    "LID", "PRI", "EPRI", "FL", "ST", "LWP", "CPU", "TCI", "LRTICKS");

	PROCLIST_FOREACH(p, &allproc) {
		(*pr)(" /- %d (%s)\n", (int)p->p_pid, p->p_comm);
		LIST_FOREACH(l, &p->p_lwps, l_sibling) {
			ci = l->l_cpu;
			tci = l->l_target_cpu;
			(*pr)(" | %5d %4u %4u 0x%8.8x %3s %18p %4u %4d %u\n",
			    (int)l->l_lid, l->l_priority, lwp_eprio(l),
			    l->l_flag, l->l_stat == LSRUN ? "RQ" :
			    (l->l_stat == LSSLEEP ? "SQ" : "-"),
			    l, ci->ci_index, (tci ? tci->ci_index : -1),
			    (u_int)(hardclock_ticks - l->l_rticks));
		}
	}
}

#endif
