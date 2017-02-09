/*	$NetBSD: kern_sleepq.c,v 1.50 2014/09/05 05:57:21 matt Exp $	*/

/*-
 * Copyright (c) 2006, 2007, 2008, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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
 * Sleep queue implementation, used by turnstiles and general sleep/wakeup
 * interfaces.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_sleepq.c,v 1.50 2014/09/05 05:57:21 matt Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/cpu.h>
#include <sys/intr.h>
#include <sys/pool.h>
#include <sys/proc.h> 
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/systm.h>
#include <sys/sleepq.h>
#include <sys/ktrace.h>

/*
 * for sleepq_abort:
 * During autoconfiguration or after a panic, a sleep will simply lower the
 * priority briefly to allow interrupts, then return.  The priority to be
 * used (IPL_SAFEPRI) is machine-dependent, thus this value is initialized and
 * maintained in the machine-dependent layers.  This priority will typically
 * be 0, or the lowest priority that is safe for use on the interrupt stack;
 * it can be made higher to block network software interrupts after panics.
 */
#ifndef	IPL_SAFEPRI
#define	IPL_SAFEPRI	0
#endif

static int	sleepq_sigtoerror(lwp_t *, int);

/* General purpose sleep table, used by mtsleep() and condition variables. */
sleeptab_t	sleeptab	__cacheline_aligned;

/*
 * sleeptab_init:
 *
 *	Initialize a sleep table.
 */
void
sleeptab_init(sleeptab_t *st)
{
	sleepq_t *sq;
	int i;

	for (i = 0; i < SLEEPTAB_HASH_SIZE; i++) {
		sq = &st->st_queues[i].st_queue;
		st->st_queues[i].st_mutex =
		    mutex_obj_alloc(MUTEX_DEFAULT, IPL_SCHED);
		sleepq_init(sq);
	}
}

/*
 * sleepq_init:
 *
 *	Prepare a sleep queue for use.
 */
void
sleepq_init(sleepq_t *sq)
{

	TAILQ_INIT(sq);
}

/*
 * sleepq_remove:
 *
 *	Remove an LWP from a sleep queue and wake it up.
 */
void
sleepq_remove(sleepq_t *sq, lwp_t *l)
{
	struct schedstate_percpu *spc;
	struct cpu_info *ci;

	KASSERT(lwp_locked(l, NULL));

	TAILQ_REMOVE(sq, l, l_sleepchain);
	l->l_syncobj = &sched_syncobj;
	l->l_wchan = NULL;
	l->l_sleepq = NULL;
	l->l_flag &= ~LW_SINTR;

	ci = l->l_cpu;
	spc = &ci->ci_schedstate;

	/*
	 * If not sleeping, the LWP must have been suspended.  Let whoever
	 * holds it stopped set it running again.
	 */
	if (l->l_stat != LSSLEEP) {
		KASSERT(l->l_stat == LSSTOP || l->l_stat == LSSUSPENDED);
		lwp_setlock(l, spc->spc_lwplock);
		return;
	}

	/*
	 * If the LWP is still on the CPU, mark it as LSONPROC.  It may be
	 * about to call mi_switch(), in which case it will yield.
	 */
	if ((l->l_pflag & LP_RUNNING) != 0) {
		l->l_stat = LSONPROC;
		l->l_slptime = 0;
		lwp_setlock(l, spc->spc_lwplock);
		return;
	}

	/* Update sleep time delta, call the wake-up handler of scheduler */
	l->l_slpticksum += (hardclock_ticks - l->l_slpticks);
	sched_wakeup(l);

	/* Look for a CPU to wake up */
	l->l_cpu = sched_takecpu(l);
	ci = l->l_cpu;
	spc = &ci->ci_schedstate;

	/*
	 * Set it running.
	 */
	spc_lock(ci);
	lwp_setlock(l, spc->spc_mutex);
	sched_setrunnable(l);
	l->l_stat = LSRUN;
	l->l_slptime = 0;
	sched_enqueue(l, false);
	spc_unlock(ci);
}

/*
 * sleepq_insert:
 *
 *	Insert an LWP into the sleep queue, optionally sorting by priority.
 */
static void
sleepq_insert(sleepq_t *sq, lwp_t *l, syncobj_t *sobj)
{

	if ((sobj->sobj_flag & SOBJ_SLEEPQ_SORTED) != 0) {
		lwp_t *l2;
		const int pri = lwp_eprio(l);

		TAILQ_FOREACH(l2, sq, l_sleepchain) {
			if (lwp_eprio(l2) < pri) {
				TAILQ_INSERT_BEFORE(l2, l, l_sleepchain);
				return;
			}
		}
	}

	if ((sobj->sobj_flag & SOBJ_SLEEPQ_LIFO) != 0)
		TAILQ_INSERT_HEAD(sq, l, l_sleepchain);
	else
		TAILQ_INSERT_TAIL(sq, l, l_sleepchain);
}

/*
 * sleepq_enqueue:
 *
 *	Enter an LWP into the sleep queue and prepare for sleep.  The sleep
 *	queue must already be locked, and any interlock (such as the kernel
 *	lock) must have be released (see sleeptab_lookup(), sleepq_enter()).
 */
void
sleepq_enqueue(sleepq_t *sq, wchan_t wchan, const char *wmesg, syncobj_t *sobj)
{
	lwp_t *l = curlwp;

	KASSERT(lwp_locked(l, NULL));
	KASSERT(l->l_stat == LSONPROC);
	KASSERT(l->l_wchan == NULL && l->l_sleepq == NULL);

	l->l_syncobj = sobj;
	l->l_wchan = wchan;
	l->l_sleepq = sq;
	l->l_wmesg = wmesg;
	l->l_slptime = 0;
	l->l_stat = LSSLEEP;
	l->l_sleeperr = 0;

	sleepq_insert(sq, l, sobj);

	/* Save the time when thread has slept */
	l->l_slpticks = hardclock_ticks;
	sched_slept(l);
}

/*
 * sleepq_block:
 *
 *	After any intermediate step such as releasing an interlock, switch.
 * 	sleepq_block() may return early under exceptional conditions, for
 * 	example if the LWP's containing process is exiting.
 *
 *	timo is a timeout in ticks.  timo = 0 specifies an infinite timeout.
 */
int
sleepq_block(int timo, bool catch_p)
{
	int error = 0, sig;
	struct proc *p;
	lwp_t *l = curlwp;
	bool early = false;
	int biglocks = l->l_biglocks;

	ktrcsw(1, 0);

	/*
	 * If sleeping interruptably, check for pending signals, exits or
	 * core dump events.
	 */
	if (catch_p) {
		l->l_flag |= LW_SINTR;
		if ((l->l_flag & (LW_CANCELLED|LW_WEXIT|LW_WCORE)) != 0) {
			l->l_flag &= ~LW_CANCELLED;
			error = EINTR;
			early = true;
		} else if ((l->l_flag & LW_PENDSIG) != 0 && sigispending(l, 0))
			early = true;
	}

	if (early) {
		/* lwp_unsleep() will release the lock */
		lwp_unsleep(l, true);
	} else {
		if (timo) {
			callout_schedule(&l->l_timeout_ch, timo);
		}
		mi_switch(l);

		/* The LWP and sleep queue are now unlocked. */
		if (timo) {
			/*
			 * Even if the callout appears to have fired, we need to
			 * stop it in order to synchronise with other CPUs.
			 */
			if (callout_halt(&l->l_timeout_ch, NULL))
				error = EWOULDBLOCK;
		}
	}

	if (catch_p && error == 0) {
		p = l->l_proc;
		if ((l->l_flag & (LW_CANCELLED | LW_WEXIT | LW_WCORE)) != 0)
			error = EINTR;
		else if ((l->l_flag & LW_PENDSIG) != 0) {
			/*
			 * Acquiring p_lock may cause us to recurse
			 * through the sleep path and back into this
			 * routine, but is safe because LWPs sleeping
			 * on locks are non-interruptable.  We will
			 * not recurse again.
			 */
			mutex_enter(p->p_lock);
			if (((sig = sigispending(l, 0)) != 0 &&
			    (sigprop[sig] & SA_STOP) == 0) ||
			    (sig = issignal(l)) != 0)
				error = sleepq_sigtoerror(l, sig);
			mutex_exit(p->p_lock);
		}
	}

	ktrcsw(0, 0);
	if (__predict_false(biglocks != 0)) {
		KERNEL_LOCK(biglocks, NULL);
	}
	return error;
}

/*
 * sleepq_wake:
 *
 *	Wake zero or more LWPs blocked on a single wait channel.
 */
void
sleepq_wake(sleepq_t *sq, wchan_t wchan, u_int expected, kmutex_t *mp)
{
	lwp_t *l, *next;

	KASSERT(mutex_owned(mp));

	for (l = TAILQ_FIRST(sq); l != NULL; l = next) {
		KASSERT(l->l_sleepq == sq);
		KASSERT(l->l_mutex == mp);
		next = TAILQ_NEXT(l, l_sleepchain);
		if (l->l_wchan != wchan)
			continue;
		sleepq_remove(sq, l);
		if (--expected == 0)
			break;
	}

	mutex_spin_exit(mp);
}

/*
 * sleepq_unsleep:
 *
 *	Remove an LWP from its sleep queue and set it runnable again. 
 *	sleepq_unsleep() is called with the LWP's mutex held, and will
 *	always release it.
 */
void
sleepq_unsleep(lwp_t *l, bool cleanup)
{
	sleepq_t *sq = l->l_sleepq;
	kmutex_t *mp = l->l_mutex;

	KASSERT(lwp_locked(l, mp));
	KASSERT(l->l_wchan != NULL);

	sleepq_remove(sq, l);
	if (cleanup) {
		mutex_spin_exit(mp);
	}
}

/*
 * sleepq_timeout:
 *
 *	Entered via the callout(9) subsystem to time out an LWP that is on a
 *	sleep queue.
 */
void
sleepq_timeout(void *arg)
{
	lwp_t *l = arg;

	/*
	 * Lock the LWP.  Assuming it's still on the sleep queue, its
	 * current mutex will also be the sleep queue mutex.
	 */
	lwp_lock(l);

	if (l->l_wchan == NULL) {
		/* Somebody beat us to it. */
		lwp_unlock(l);
		return;
	}

	lwp_unsleep(l, true);
}

/*
 * sleepq_sigtoerror:
 *
 *	Given a signal number, interpret and return an error code.
 */
static int
sleepq_sigtoerror(lwp_t *l, int sig)
{
	struct proc *p = l->l_proc;
	int error;

	KASSERT(mutex_owned(p->p_lock));

	/*
	 * If this sleep was canceled, don't let the syscall restart.
	 */
	if ((SIGACTION(p, sig).sa_flags & SA_RESTART) == 0)
		error = EINTR;
	else
		error = ERESTART;

	return error;
}

/*
 * sleepq_abort:
 *
 *	After a panic or during autoconfiguration, lower the interrupt
 *	priority level to give pending interrupts a chance to run, and
 *	then return.  Called if sleepq_dontsleep() returns non-zero, and
 *	always returns zero.
 */
int
sleepq_abort(kmutex_t *mtx, int unlock)
{ 
	int s;

	s = splhigh();
	splx(IPL_SAFEPRI);
	splx(s);
	if (mtx != NULL && unlock != 0)
		mutex_exit(mtx);

	return 0;
}

/*
 * sleepq_reinsert:
 *
 *	Move the possition of the lwp in the sleep queue after a possible
 *	change of the lwp's effective priority.
 */
static void
sleepq_reinsert(sleepq_t *sq, lwp_t *l)
{

	KASSERT(l->l_sleepq == sq);
	if ((l->l_syncobj->sobj_flag & SOBJ_SLEEPQ_SORTED) == 0) {
		return;
	}

	/*
	 * Don't let the sleep queue become empty, even briefly.
	 * cv_signal() and cv_broadcast() inspect it without the
	 * sleep queue lock held and need to see a non-empty queue
	 * head if there are waiters.
	 */
	if (TAILQ_FIRST(sq) == l && TAILQ_NEXT(l, l_sleepchain) == NULL) {
		return;
	}
	TAILQ_REMOVE(sq, l, l_sleepchain);
	sleepq_insert(sq, l, l->l_syncobj);
}

/*
 * sleepq_changepri:
 *
 *	Adjust the priority of an LWP residing on a sleepq.
 */
void
sleepq_changepri(lwp_t *l, pri_t pri)
{
	sleepq_t *sq = l->l_sleepq;

	KASSERT(lwp_locked(l, NULL));

	l->l_priority = pri;
	sleepq_reinsert(sq, l);
}

/*
 * sleepq_changepri:
 *
 *	Adjust the lended priority of an LWP residing on a sleepq.
 */
void
sleepq_lendpri(lwp_t *l, pri_t pri)
{
	sleepq_t *sq = l->l_sleepq;

	KASSERT(lwp_locked(l, NULL));

	l->l_inheritedprio = pri;
	sleepq_reinsert(sq, l);
}
