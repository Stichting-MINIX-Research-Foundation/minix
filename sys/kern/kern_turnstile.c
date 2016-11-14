/*	$NetBSD: kern_turnstile.c,v 1.32 2012/06/15 13:51:40 yamt Exp $	*/

/*-
 * Copyright (c) 2002, 2006, 2007, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe and Andrew Doran.
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
 * Turnstiles are described in detail in:
 *
 *	Solaris Internals: Core Kernel Architecture, Jim Mauro and
 *	    Richard McDougall.
 *
 * Turnstiles are kept in a hash table.  There are likely to be many more
 * synchronisation objects than there are threads.  Since a thread can block
 * on only one lock at a time, we only need one turnstile per thread, and
 * so they are allocated at thread creation time.
 *
 * When a thread decides it needs to block on a lock, it looks up the
 * active turnstile for that lock.  If no active turnstile exists, then
 * the process lends its turnstile to the lock.  If there is already an
 * active turnstile for the lock, the thread places its turnstile on a
 * list of free turnstiles, and references the active one instead.
 *
 * The act of looking up the turnstile acquires an interlock on the sleep
 * queue.  If a thread decides it doesn't need to block after all, then this
 * interlock must be released by explicitly aborting the turnstile
 * operation.
 *
 * When a thread is awakened, it needs to get its turnstile back.  If there
 * are still other threads waiting in the active turnstile, the thread
 * grabs a free turnstile off the free list.  Otherwise, it can take back
 * the active turnstile from the lock (thus deactivating the turnstile).
 *
 * Turnstiles are the place to do priority inheritence.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_turnstile.c,v 1.32 2012/06/15 13:51:40 yamt Exp $");

#include <sys/param.h>
#include <sys/lockdebug.h>
#include <sys/pool.h>
#include <sys/proc.h> 
#include <sys/sleepq.h>
#include <sys/systm.h>

#define	TS_HASH_SIZE	64
#define	TS_HASH_MASK	(TS_HASH_SIZE - 1)
#define	TS_HASH(obj)	(((uintptr_t)(obj) >> 3) & TS_HASH_MASK)

static tschain_t	turnstile_tab[TS_HASH_SIZE]	__cacheline_aligned;
pool_cache_t		turnstile_cache			__read_mostly;

static int		turnstile_ctor(void *, void *, int);

extern turnstile_t	turnstile0;

/*
 * turnstile_init:
 *
 *	Initialize the turnstile mechanism.
 */
void
turnstile_init(void)
{
	tschain_t *tc;
	int i;

	for (i = 0; i < TS_HASH_SIZE; i++) {
		tc = &turnstile_tab[i];
		LIST_INIT(&tc->tc_chain);
		tc->tc_mutex = mutex_obj_alloc(MUTEX_DEFAULT, IPL_SCHED);
	}

	turnstile_cache = pool_cache_init(sizeof(turnstile_t), 0, 0, 0,
	    "tstilepl", NULL, IPL_NONE, turnstile_ctor, NULL, NULL);
	KASSERT(turnstile_cache != NULL);

	(void)turnstile_ctor(NULL, &turnstile0, 0);
}

/*
 * turnstile_ctor:
 *
 *	Constructor for turnstiles.
 */
static int
turnstile_ctor(void *arg, void *obj, int flags)
{
	turnstile_t *ts = obj;

	memset(ts, 0, sizeof(*ts));
	sleepq_init(&ts->ts_sleepq[TS_READER_Q]);
	sleepq_init(&ts->ts_sleepq[TS_WRITER_Q]);
	return (0);
}

/*
 * turnstile_remove:
 *
 *	Remove an LWP from a turnstile sleep queue and wake it.
 */
static inline void
turnstile_remove(turnstile_t *ts, lwp_t *l, int q)
{
	turnstile_t *nts;

	KASSERT(l->l_ts == ts);

	/*
	 * This process is no longer using the active turnstile.
	 * Find an inactive one on the free list to give to it.
	 */
	if ((nts = ts->ts_free) != NULL) {
		KASSERT(TS_ALL_WAITERS(ts) > 1);
		l->l_ts = nts;
		ts->ts_free = nts->ts_free;
		nts->ts_free = NULL;
	} else {
		/*
		 * If the free list is empty, this is the last
		 * waiter.
		 */
		KASSERT(TS_ALL_WAITERS(ts) == 1);
		LIST_REMOVE(ts, ts_chain);
	}

	ts->ts_waiters[q]--;
	sleepq_remove(&ts->ts_sleepq[q], l);
}

/*
 * turnstile_lookup:
 *
 *	Look up the turnstile for the specified lock.  This acquires and
 *	holds the turnstile chain lock (sleep queue interlock).
 */
turnstile_t *
turnstile_lookup(wchan_t obj)
{
	turnstile_t *ts;
	tschain_t *tc;

	tc = &turnstile_tab[TS_HASH(obj)];
	mutex_spin_enter(tc->tc_mutex);

	LIST_FOREACH(ts, &tc->tc_chain, ts_chain)
		if (ts->ts_obj == obj)
			return (ts);

	/*
	 * No turnstile yet for this lock.  No problem, turnstile_block()
	 * handles this by fetching the turnstile from the blocking thread.
	 */
	return (NULL);
}

/*
 * turnstile_exit:
 *
 *	Abort a turnstile operation.
 */
void
turnstile_exit(wchan_t obj)
{
	tschain_t *tc;

	tc = &turnstile_tab[TS_HASH(obj)];
	mutex_spin_exit(tc->tc_mutex);
}

/*
 * turnstile_lendpri:
 *
 *	Lend our priority to lwps on the blocking chain.
 *
 *	If the current owner of the lock (l->l_wchan, set by sleepq_enqueue)
 *	has a priority lower than ours (lwp_eprio(l)), lend our priority to
 *	him to avoid priority inversions.
 */

static void
turnstile_lendpri(lwp_t *cur)
{
	lwp_t * l = cur;
	pri_t prio;

	/*
	 * NOTE: if you get a panic in this code block, it is likely that
	 * a lock has been destroyed or corrupted while still in use.  Try
	 * compiling a kernel with LOCKDEBUG to pinpoint the problem.
	 */

	LOCKDEBUG_BARRIER(l->l_mutex, 1);
	KASSERT(l == curlwp);
	prio = lwp_eprio(l);
	for (;;) {
		lwp_t *owner;
		turnstile_t *ts;
		bool dolock;

		if (l->l_wchan == NULL)
			break;

		/*
		 * Ask syncobj the owner of the lock.
		 */
		owner = (*l->l_syncobj->sobj_owner)(l->l_wchan);
		if (owner == NULL)
			break;

		/*
		 * The owner may have changed as we have dropped the tc lock.
		 */
		if (cur == owner) {
			/*
			 * We own the lock: stop here, sleepq_block()
			 * should wake up immediatly.
			 */
			break;
		}
		/*
		 * Acquire owner->l_mutex if we don't have it yet.
		 * Because we already have another LWP lock (l->l_mutex) held,
		 * we need to play a try lock dance to avoid deadlock.
		 */
		dolock = l->l_mutex != owner->l_mutex;
		if (l == owner || (dolock && !lwp_trylock(owner))) {
			/*
			 * The owner was changed behind us or trylock failed.
			 * Restart from curlwp.
			 *
			 * Note that there may be a livelock here:
			 * the owner may try grabing cur's lock (which is the
			 * tc lock) while we're trying to grab the owner's lock.
			 */
			lwp_unlock(l);
			l = cur;
			lwp_lock(l);
			prio = lwp_eprio(l);
			continue;
		}
		/*
		 * If the owner's priority is already higher than ours,
		 * there's nothing to do anymore.
		 */
		if (prio <= lwp_eprio(owner)) {
			if (dolock)
				lwp_unlock(owner);
			break;
		}
		/*
		 * Lend our priority to the 'owner' LWP.
		 *
		 * Update lenders info for turnstile_unlendpri.
		 */
		ts = l->l_ts;
		KASSERT(ts->ts_inheritor == owner || ts->ts_inheritor == NULL);
		if (ts->ts_inheritor == NULL) {
			ts->ts_inheritor = owner;
			ts->ts_eprio = prio;
			SLIST_INSERT_HEAD(&owner->l_pi_lenders, ts, ts_pichain);
			lwp_lendpri(owner, prio);
		} else if (prio > ts->ts_eprio) {
			ts->ts_eprio = prio;
			lwp_lendpri(owner, prio);
		}
		if (dolock)
			lwp_unlock(l);
		LOCKDEBUG_BARRIER(owner->l_mutex, 1);
		l = owner;
	}
	LOCKDEBUG_BARRIER(l->l_mutex, 1);
	if (cur->l_mutex != l->l_mutex) {
		lwp_unlock(l);
		lwp_lock(cur);
	}
	LOCKDEBUG_BARRIER(cur->l_mutex, 1);
}

/*
 * turnstile_unlendpri: undo turnstile_lendpri
 */

static void
turnstile_unlendpri(turnstile_t *ts)
{
	lwp_t * const l = curlwp;
	turnstile_t *iter;
	turnstile_t *next;
	turnstile_t *prev = NULL;
	pri_t prio;
	bool dolock;

	KASSERT(ts->ts_inheritor != NULL);
	ts->ts_inheritor = NULL;
	dolock = l->l_mutex == l->l_cpu->ci_schedstate.spc_lwplock;
	if (dolock) {
		lwp_lock(l);
	}

	/*
	 * the following loop does two things.
	 *
	 * - remove ts from the list.
	 *
	 * - from the rest of the list, find the highest priority.
	 */

	prio = -1;
	KASSERT(!SLIST_EMPTY(&l->l_pi_lenders));
	for (iter = SLIST_FIRST(&l->l_pi_lenders);
	    iter != NULL; iter = next) {
		KASSERT(lwp_eprio(l) >= ts->ts_eprio);
		next = SLIST_NEXT(iter, ts_pichain);
		if (iter == ts) {
			if (prev == NULL) {
				SLIST_REMOVE_HEAD(&l->l_pi_lenders,
				    ts_pichain);
			} else {
				SLIST_REMOVE_AFTER(prev, ts_pichain);
			}
		} else if (prio < iter->ts_eprio) {
			prio = iter->ts_eprio;
		}
		prev = iter;
	}

	lwp_lendpri(l, prio);

	if (dolock) {
		lwp_unlock(l);
	}
}

/*
 * turnstile_block:
 *
 *	 Enter an object into the turnstile chain and prepare the current
 *	 LWP for sleep.
 */
void
turnstile_block(turnstile_t *ts, int q, wchan_t obj, syncobj_t *sobj)
{
	lwp_t * const l = curlwp; /* cached curlwp */
	turnstile_t *ots;
	tschain_t *tc;
	sleepq_t *sq;
	pri_t obase;

	tc = &turnstile_tab[TS_HASH(obj)];

	KASSERT(q == TS_READER_Q || q == TS_WRITER_Q);
	KASSERT(mutex_owned(tc->tc_mutex));
	KASSERT(l != NULL && l->l_ts != NULL);

	if (ts == NULL) {
		/*
		 * We are the first thread to wait for this object;
		 * lend our turnstile to it.
		 */
		ts = l->l_ts;
		KASSERT(TS_ALL_WAITERS(ts) == 0);
		KASSERT(TAILQ_EMPTY(&ts->ts_sleepq[TS_READER_Q]) &&
			TAILQ_EMPTY(&ts->ts_sleepq[TS_WRITER_Q]));
		ts->ts_obj = obj;
		ts->ts_inheritor = NULL;
		LIST_INSERT_HEAD(&tc->tc_chain, ts, ts_chain);
	} else {
		/*
		 * Object already has a turnstile.  Put our turnstile
		 * onto the free list, and reference the existing
		 * turnstile instead.
		 */
		ots = l->l_ts;
		KASSERT(ots->ts_free == NULL);
		ots->ts_free = ts->ts_free;
		ts->ts_free = ots;
		l->l_ts = ts;

		KASSERT(ts->ts_obj == obj);
		KASSERT(TS_ALL_WAITERS(ts) != 0);
		KASSERT(!TAILQ_EMPTY(&ts->ts_sleepq[TS_READER_Q]) ||
			!TAILQ_EMPTY(&ts->ts_sleepq[TS_WRITER_Q]));
	}

	sq = &ts->ts_sleepq[q];
	ts->ts_waiters[q]++;
	sleepq_enter(sq, l, tc->tc_mutex);
	LOCKDEBUG_BARRIER(tc->tc_mutex, 1);
	l->l_kpriority = true;
	obase = l->l_kpribase;
	if (obase < PRI_KTHREAD)
		l->l_kpribase = PRI_KTHREAD;
	sleepq_enqueue(sq, obj, "tstile", sobj);

	/*
	 * Disable preemption across this entire block, as we may drop
	 * scheduler locks (allowing preemption), and would prefer not
	 * to be interrupted while in a state of flux.
	 */
	KPREEMPT_DISABLE(l);
	KASSERT(tc->tc_mutex == l->l_mutex);
	turnstile_lendpri(l);
	sleepq_block(0, false);
	l->l_kpribase = obase;
	KPREEMPT_ENABLE(l);
}

/*
 * turnstile_wakeup:
 *
 *	Wake up the specified number of threads that are blocked
 *	in a turnstile.
 */
void
turnstile_wakeup(turnstile_t *ts, int q, int count, lwp_t *nl)
{
	sleepq_t *sq;
	tschain_t *tc;
	lwp_t *l;

	tc = &turnstile_tab[TS_HASH(ts->ts_obj)];
	sq = &ts->ts_sleepq[q];

	KASSERT(q == TS_READER_Q || q == TS_WRITER_Q);
	KASSERT(count > 0 && count <= TS_WAITERS(ts, q));
	KASSERT(mutex_owned(tc->tc_mutex));
	KASSERT(ts->ts_inheritor == curlwp || ts->ts_inheritor == NULL);

	/*
	 * restore inherited priority if necessary.
	 */

	if (ts->ts_inheritor != NULL) {
		turnstile_unlendpri(ts);
	}

	if (nl != NULL) {
#if defined(DEBUG) || defined(LOCKDEBUG)
		TAILQ_FOREACH(l, sq, l_sleepchain) {
			if (l == nl)
				break;
		}
		if (l == NULL)
			panic("turnstile_wakeup: nl not on sleepq");
#endif
		turnstile_remove(ts, nl, q);
	} else {
		while (count-- > 0) {
			l = TAILQ_FIRST(sq);
			KASSERT(l != NULL);
			turnstile_remove(ts, l, q);
		}
	}
	mutex_spin_exit(tc->tc_mutex);
}

/*
 * turnstile_unsleep:
 *
 *	Remove an LWP from the turnstile.  This is called when the LWP has
 *	not been awoken normally but instead interrupted: for example, if it
 *	has received a signal.  It's not a valid action for turnstiles,
 *	since LWPs blocking on a turnstile are not interruptable.
 */
void
turnstile_unsleep(lwp_t *l, bool cleanup)
{

	lwp_unlock(l);
	panic("turnstile_unsleep");
}

/*
 * turnstile_changepri:
 *
 *	Adjust the priority of an LWP residing on a turnstile.
 */
void
turnstile_changepri(lwp_t *l, pri_t pri)
{

	/* XXX priority inheritance */
	sleepq_changepri(l, pri);
}

#if defined(LOCKDEBUG)
/*
 * turnstile_print:
 *
 *	Given the address of a lock object, print the contents of a
 *	turnstile.
 */
void
turnstile_print(volatile void *obj, void (*pr)(const char *, ...))
{
	turnstile_t *ts;
	tschain_t *tc;
	sleepq_t *rsq, *wsq;
	lwp_t *l;

	tc = &turnstile_tab[TS_HASH(obj)];

	LIST_FOREACH(ts, &tc->tc_chain, ts_chain)
		if (ts->ts_obj == obj)
			break;

	(*pr)("Turnstile chain at %p.\n", tc);
	if (ts == NULL) {
		(*pr)("=> No active turnstile for this lock.\n");
		return;
	}

	rsq = &ts->ts_sleepq[TS_READER_Q];
	wsq = &ts->ts_sleepq[TS_WRITER_Q];

	(*pr)("=> Turnstile at %p (wrq=%p, rdq=%p).\n", ts, rsq, wsq);

	(*pr)("=> %d waiting readers:", TS_WAITERS(ts, TS_READER_Q));
	TAILQ_FOREACH(l, rsq, l_sleepchain) {
		(*pr)(" %p", l);
	}
	(*pr)("\n");

	(*pr)("=> %d waiting writers:", TS_WAITERS(ts, TS_WRITER_Q));
	TAILQ_FOREACH(l, wsq, l_sleepchain) {
		(*pr)(" %p", l);
	}
	(*pr)("\n");
}
#endif	/* LOCKDEBUG */
