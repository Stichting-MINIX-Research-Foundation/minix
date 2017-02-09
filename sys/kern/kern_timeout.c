/*	$NetBSD: kern_timeout.c,v 1.50 2015/02/09 20:46:55 christos Exp $	*/

/*-
 * Copyright (c) 2003, 2006, 2007, 2008, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe, and by Andrew Doran.
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
 * Copyright (c) 2001 Thomas Nordin <nordin@openbsd.org>
 * Copyright (c) 2000-2001 Artur Grabowski <art@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_timeout.c,v 1.50 2015/02/09 20:46:55 christos Exp $");

/*
 * Timeouts are kept in a hierarchical timing wheel.  The c_time is the
 * value of c_cpu->cc_ticks when the timeout should be called.  There are
 * four levels with 256 buckets each. See 'Scheme 7' in "Hashed and
 * Hierarchical Timing Wheels: Efficient Data Structures for Implementing
 * a Timer Facility" by George Varghese and Tony Lauck.
 *
 * Some of the "math" in here is a bit tricky.  We have to beware of
 * wrapping ints.
 *
 * We use the fact that any element added to the queue must be added with
 * a positive time.  That means that any element `to' on the queue cannot
 * be scheduled to timeout further in time than INT_MAX, but c->c_time can
 * be positive or negative so comparing it with anything is dangerous. 
 * The only way we can use the c->c_time value in any predictable way is
 * when we calculate how far in the future `to' will timeout - "c->c_time
 * - c->c_cpu->cc_ticks".  The result will always be positive for future
 * timeouts and 0 or negative for due timeouts.
 */

#define	_CALLOUT_PRIVATE

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/callout.h>
#include <sys/lwp.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sleepq.h>
#include <sys/syncobj.h>
#include <sys/evcnt.h>
#include <sys/intr.h>
#include <sys/cpu.h>
#include <sys/kmem.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_interface.h>
#include <ddb/db_access.h>
#include <ddb/db_cpu.h>
#include <ddb/db_sym.h>
#include <ddb/db_output.h>
#endif

#define BUCKETS		1024
#define WHEELSIZE	256
#define WHEELMASK	255
#define WHEELBITS	8

#define MASKWHEEL(wheel, time) (((time) >> ((wheel)*WHEELBITS)) & WHEELMASK)

#define BUCKET(cc, rel, abs)						\
    (((rel) <= (1 << (2*WHEELBITS)))					\
    	? ((rel) <= (1 << WHEELBITS))					\
            ? &(cc)->cc_wheel[MASKWHEEL(0, (abs))]			\
            : &(cc)->cc_wheel[MASKWHEEL(1, (abs)) + WHEELSIZE]		\
        : ((rel) <= (1 << (3*WHEELBITS)))				\
            ? &(cc)->cc_wheel[MASKWHEEL(2, (abs)) + 2*WHEELSIZE]	\
            : &(cc)->cc_wheel[MASKWHEEL(3, (abs)) + 3*WHEELSIZE])

#define MOVEBUCKET(cc, wheel, time)					\
    CIRCQ_APPEND(&(cc)->cc_todo,					\
        &(cc)->cc_wheel[MASKWHEEL((wheel), (time)) + (wheel)*WHEELSIZE])

/*
 * Circular queue definitions.
 */

#define CIRCQ_INIT(list)						\
do {									\
        (list)->cq_next_l = (list);					\
        (list)->cq_prev_l = (list);					\
} while (/*CONSTCOND*/0)

#define CIRCQ_INSERT(elem, list)					\
do {									\
        (elem)->cq_prev_e = (list)->cq_prev_e;				\
        (elem)->cq_next_l = (list);					\
        (list)->cq_prev_l->cq_next_l = (elem);				\
        (list)->cq_prev_l = (elem);					\
} while (/*CONSTCOND*/0)

#define CIRCQ_APPEND(fst, snd)						\
do {									\
        if (!CIRCQ_EMPTY(snd)) {					\
                (fst)->cq_prev_l->cq_next_l = (snd)->cq_next_l;		\
                (snd)->cq_next_l->cq_prev_l = (fst)->cq_prev_l;		\
                (snd)->cq_prev_l->cq_next_l = (fst);			\
                (fst)->cq_prev_l = (snd)->cq_prev_l;			\
                CIRCQ_INIT(snd);					\
        }								\
} while (/*CONSTCOND*/0)

#define CIRCQ_REMOVE(elem)						\
do {									\
        (elem)->cq_next_l->cq_prev_e = (elem)->cq_prev_e;		\
        (elem)->cq_prev_l->cq_next_e = (elem)->cq_next_e;		\
} while (/*CONSTCOND*/0)

#define CIRCQ_FIRST(list)	((list)->cq_next_e)
#define CIRCQ_NEXT(elem)	((elem)->cq_next_e)
#define CIRCQ_LAST(elem,list)	((elem)->cq_next_l == (list))
#define CIRCQ_EMPTY(list)	((list)->cq_next_l == (list))

struct callout_cpu {
	kmutex_t	*cc_lock;
	sleepq_t	cc_sleepq;
	u_int		cc_nwait;
	u_int		cc_ticks;
	lwp_t		*cc_lwp;
	callout_impl_t	*cc_active;
	callout_impl_t	*cc_cancel;
	struct evcnt	cc_ev_late;
	struct evcnt	cc_ev_block;
	struct callout_circq cc_todo;		/* Worklist */
	struct callout_circq cc_wheel[BUCKETS];	/* Queues of timeouts */
	char		cc_name1[12];
	char		cc_name2[12];
};

#ifndef CRASH

static void	callout_softclock(void *);
static struct callout_cpu callout_cpu0;
static void *callout_sih;

static inline kmutex_t *
callout_lock(callout_impl_t *c)
{
	struct callout_cpu *cc;
	kmutex_t *lock;

	for (;;) {
		cc = c->c_cpu;
		lock = cc->cc_lock;
		mutex_spin_enter(lock);
		if (__predict_true(cc == c->c_cpu))
			return lock;
		mutex_spin_exit(lock);
	}
}

/*
 * callout_startup:
 *
 *	Initialize the callout facility, called at system startup time.
 *	Do just enough to allow callouts to be safely registered.
 */
void
callout_startup(void)
{
	struct callout_cpu *cc;
	int b;

	KASSERT(curcpu()->ci_data.cpu_callout == NULL);

	cc = &callout_cpu0;
	cc->cc_lock = mutex_obj_alloc(MUTEX_DEFAULT, IPL_SCHED);
	CIRCQ_INIT(&cc->cc_todo);
	for (b = 0; b < BUCKETS; b++)
		CIRCQ_INIT(&cc->cc_wheel[b]);
	curcpu()->ci_data.cpu_callout = cc;
}

/*
 * callout_init_cpu:
 *
 *	Per-CPU initialization.
 */
CTASSERT(sizeof(callout_impl_t) <= sizeof(callout_t));

void
callout_init_cpu(struct cpu_info *ci)
{
	struct callout_cpu *cc;
	int b;

	if ((cc = ci->ci_data.cpu_callout) == NULL) {
		cc = kmem_zalloc(sizeof(*cc), KM_SLEEP);
		if (cc == NULL)
			panic("callout_init_cpu (1)");
		cc->cc_lock = mutex_obj_alloc(MUTEX_DEFAULT, IPL_SCHED);
		CIRCQ_INIT(&cc->cc_todo);
		for (b = 0; b < BUCKETS; b++)
			CIRCQ_INIT(&cc->cc_wheel[b]);
	} else {
		/* Boot CPU, one time only. */
		callout_sih = softint_establish(SOFTINT_CLOCK | SOFTINT_MPSAFE,
		    callout_softclock, NULL);
		if (callout_sih == NULL)
			panic("callout_init_cpu (2)");
	}

	sleepq_init(&cc->cc_sleepq);

	snprintf(cc->cc_name1, sizeof(cc->cc_name1), "late/%u",
	    cpu_index(ci));
	evcnt_attach_dynamic(&cc->cc_ev_late, EVCNT_TYPE_MISC,
	    NULL, "callout", cc->cc_name1);

	snprintf(cc->cc_name2, sizeof(cc->cc_name2), "wait/%u",
	    cpu_index(ci));
	evcnt_attach_dynamic(&cc->cc_ev_block, EVCNT_TYPE_MISC,
	    NULL, "callout", cc->cc_name2);

	ci->ci_data.cpu_callout = cc;
}

/*
 * callout_init:
 *
 *	Initialize a callout structure.  This must be quick, so we fill
 *	only the minimum number of fields.
 */
void
callout_init(callout_t *cs, u_int flags)
{
	callout_impl_t *c = (callout_impl_t *)cs;
	struct callout_cpu *cc;

	KASSERT((flags & ~CALLOUT_FLAGMASK) == 0);

	cc = curcpu()->ci_data.cpu_callout;
	c->c_func = NULL;
	c->c_magic = CALLOUT_MAGIC;
	if (__predict_true((flags & CALLOUT_MPSAFE) != 0 && cc != NULL)) {
		c->c_flags = flags;
		c->c_cpu = cc;
		return;
	}
	c->c_flags = flags | CALLOUT_BOUND;
	c->c_cpu = &callout_cpu0;
}

/*
 * callout_destroy:
 *
 *	Destroy a callout structure.  The callout must be stopped.
 */
void
callout_destroy(callout_t *cs)
{
	callout_impl_t *c = (callout_impl_t *)cs;

	/*
	 * It's not necessary to lock in order to see the correct value
	 * of c->c_flags.  If the callout could potentially have been
	 * running, the current thread should have stopped it.
	 */
	KASSERTMSG((c->c_flags & CALLOUT_PENDING) == 0,
	    "callout %p: c_func (%p) c_flags (%#x) destroyed from %p",
	    c, c->c_func, c->c_flags, __builtin_return_address(0));
	KASSERT(c->c_cpu->cc_lwp == curlwp || c->c_cpu->cc_active != c);
	KASSERTMSG(c->c_magic == CALLOUT_MAGIC,
	    "callout %p: c_magic (%#x) != CALLOUT_MAGIC (%#x)",
	    c, c->c_magic, CALLOUT_MAGIC);
	c->c_magic = 0;
}

/*
 * callout_schedule_locked:
 *
 *	Schedule a callout to run.  The function and argument must
 *	already be set in the callout structure.  Must be called with
 *	callout_lock.
 */
static void
callout_schedule_locked(callout_impl_t *c, kmutex_t *lock, int to_ticks)
{
	struct callout_cpu *cc, *occ;
	int old_time;

	KASSERT(to_ticks >= 0);
	KASSERT(c->c_func != NULL);

	/* Initialize the time here, it won't change. */
	occ = c->c_cpu;
	c->c_flags &= ~(CALLOUT_FIRED | CALLOUT_INVOKING);

	/*
	 * If this timeout is already scheduled and now is moved
	 * earlier, reschedule it now.  Otherwise leave it in place
	 * and let it be rescheduled later.
	 */
	if ((c->c_flags & CALLOUT_PENDING) != 0) {
		/* Leave on existing CPU. */
		old_time = c->c_time;
		c->c_time = to_ticks + occ->cc_ticks;
		if (c->c_time - old_time < 0) {
			CIRCQ_REMOVE(&c->c_list);
			CIRCQ_INSERT(&c->c_list, &occ->cc_todo);
		}
		mutex_spin_exit(lock);
		return;
	}

	cc = curcpu()->ci_data.cpu_callout;
	if ((c->c_flags & CALLOUT_BOUND) != 0 || cc == occ ||
	    !mutex_tryenter(cc->cc_lock)) {
		/* Leave on existing CPU. */
		c->c_time = to_ticks + occ->cc_ticks;
		c->c_flags |= CALLOUT_PENDING;
		CIRCQ_INSERT(&c->c_list, &occ->cc_todo);
	} else {
		/* Move to this CPU. */
		c->c_cpu = cc;
		c->c_time = to_ticks + cc->cc_ticks;
		c->c_flags |= CALLOUT_PENDING;
		CIRCQ_INSERT(&c->c_list, &cc->cc_todo);
		mutex_spin_exit(cc->cc_lock);
	}
	mutex_spin_exit(lock);
}

/*
 * callout_reset:
 *
 *	Reset a callout structure with a new function and argument, and
 *	schedule it to run.
 */
void
callout_reset(callout_t *cs, int to_ticks, void (*func)(void *), void *arg)
{
	callout_impl_t *c = (callout_impl_t *)cs;
	kmutex_t *lock;

	KASSERT(c->c_magic == CALLOUT_MAGIC);
	KASSERT(func != NULL);

	lock = callout_lock(c);
	c->c_func = func;
	c->c_arg = arg;
	callout_schedule_locked(c, lock, to_ticks);
}

/*
 * callout_schedule:
 *
 *	Schedule a callout to run.  The function and argument must
 *	already be set in the callout structure.
 */
void
callout_schedule(callout_t *cs, int to_ticks)
{
	callout_impl_t *c = (callout_impl_t *)cs;
	kmutex_t *lock;

	KASSERT(c->c_magic == CALLOUT_MAGIC);

	lock = callout_lock(c);
	callout_schedule_locked(c, lock, to_ticks);
}

/*
 * callout_stop:
 *
 *	Try to cancel a pending callout.  It may be too late: the callout
 *	could be running on another CPU.  If called from interrupt context,
 *	the callout could already be in progress at a lower priority.
 */
bool
callout_stop(callout_t *cs)
{
	callout_impl_t *c = (callout_impl_t *)cs;
	struct callout_cpu *cc;
	kmutex_t *lock;
	bool expired;

	KASSERT(c->c_magic == CALLOUT_MAGIC);

	lock = callout_lock(c);

	if ((c->c_flags & CALLOUT_PENDING) != 0)
		CIRCQ_REMOVE(&c->c_list);
	expired = ((c->c_flags & CALLOUT_FIRED) != 0);
	c->c_flags &= ~(CALLOUT_PENDING|CALLOUT_FIRED);

	cc = c->c_cpu;
	if (cc->cc_active == c) {
		/*
		 * This is for non-MPSAFE callouts only.  To synchronize
		 * effectively we must be called with kernel_lock held.
		 * It's also taken in callout_softclock.
		 */
		cc->cc_cancel = c;
	}

	mutex_spin_exit(lock);

	return expired;
}

/*
 * callout_halt:
 *
 *	Cancel a pending callout.  If in-flight, block until it completes.
 *	May not be called from a hard interrupt handler.  If the callout
 * 	can take locks, the caller of callout_halt() must not hold any of
 *	those locks, otherwise the two could deadlock.  If 'interlock' is
 *	non-NULL and we must wait for the callout to complete, it will be
 *	released and re-acquired before returning.
 */
bool
callout_halt(callout_t *cs, void *interlock)
{
	callout_impl_t *c = (callout_impl_t *)cs;
	struct callout_cpu *cc;
	struct lwp *l;
	kmutex_t *lock, *relock;
	bool expired;

	KASSERT(c->c_magic == CALLOUT_MAGIC);
	KASSERT(!cpu_intr_p());

	lock = callout_lock(c);
	relock = NULL;

	expired = ((c->c_flags & CALLOUT_FIRED) != 0);
	if ((c->c_flags & CALLOUT_PENDING) != 0)
		CIRCQ_REMOVE(&c->c_list);
	c->c_flags &= ~(CALLOUT_PENDING|CALLOUT_FIRED);

	l = curlwp;
	for (;;) {
		cc = c->c_cpu;
		if (__predict_true(cc->cc_active != c || cc->cc_lwp == l))
			break;
		if (interlock != NULL) {
			/*
			 * Avoid potential scheduler lock order problems by
			 * dropping the interlock without the callout lock
			 * held.
			 */
			mutex_spin_exit(lock);
			mutex_exit(interlock);
			relock = interlock;
			interlock = NULL;
		} else {
			/* XXX Better to do priority inheritance. */
			KASSERT(l->l_wchan == NULL);
			cc->cc_nwait++;
			cc->cc_ev_block.ev_count++;
			l->l_kpriority = true;
			sleepq_enter(&cc->cc_sleepq, l, cc->cc_lock);
			sleepq_enqueue(&cc->cc_sleepq, cc, "callout",
			    &sleep_syncobj);
			sleepq_block(0, false);
		}
		lock = callout_lock(c);
	}

	mutex_spin_exit(lock);
	if (__predict_false(relock != NULL))
		mutex_enter(relock);

	return expired;
}

#ifdef notyet
/*
 * callout_bind:
 *
 *	Bind a callout so that it will only execute on one CPU.
 *	The callout must be stopped, and must be MPSAFE.
 *
 *	XXX Disabled for now until it is decided how to handle
 *	offlined CPUs.  We may want weak+strong binding.
 */
void
callout_bind(callout_t *cs, struct cpu_info *ci)
{
	callout_impl_t *c = (callout_impl_t *)cs;
	struct callout_cpu *cc;
	kmutex_t *lock;

	KASSERT((c->c_flags & CALLOUT_PENDING) == 0);
	KASSERT(c->c_cpu->cc_active != c);
	KASSERT(c->c_magic == CALLOUT_MAGIC);
	KASSERT((c->c_flags & CALLOUT_MPSAFE) != 0);

	lock = callout_lock(c);
	cc = ci->ci_data.cpu_callout;
	c->c_flags |= CALLOUT_BOUND;
	if (c->c_cpu != cc) {
		/*
		 * Assigning c_cpu effectively unlocks the callout
		 * structure, as we don't hold the new CPU's lock.
		 * Issue memory barrier to prevent accesses being
		 * reordered.
		 */
		membar_exit();
		c->c_cpu = cc;
	}
	mutex_spin_exit(lock);
}
#endif

void
callout_setfunc(callout_t *cs, void (*func)(void *), void *arg)
{
	callout_impl_t *c = (callout_impl_t *)cs;
	kmutex_t *lock;

	KASSERT(c->c_magic == CALLOUT_MAGIC);
	KASSERT(func != NULL);

	lock = callout_lock(c);
	c->c_func = func;
	c->c_arg = arg;
	mutex_spin_exit(lock);
}

bool
callout_expired(callout_t *cs)
{
	callout_impl_t *c = (callout_impl_t *)cs;
	kmutex_t *lock;
	bool rv;

	KASSERT(c->c_magic == CALLOUT_MAGIC);

	lock = callout_lock(c);
	rv = ((c->c_flags & CALLOUT_FIRED) != 0);
	mutex_spin_exit(lock);

	return rv;
}

bool
callout_active(callout_t *cs)
{
	callout_impl_t *c = (callout_impl_t *)cs;
	kmutex_t *lock;
	bool rv;

	KASSERT(c->c_magic == CALLOUT_MAGIC);

	lock = callout_lock(c);
	rv = ((c->c_flags & (CALLOUT_PENDING|CALLOUT_FIRED)) != 0);
	mutex_spin_exit(lock);

	return rv;
}

bool
callout_pending(callout_t *cs)
{
	callout_impl_t *c = (callout_impl_t *)cs;
	kmutex_t *lock;
	bool rv;

	KASSERT(c->c_magic == CALLOUT_MAGIC);

	lock = callout_lock(c);
	rv = ((c->c_flags & CALLOUT_PENDING) != 0);
	mutex_spin_exit(lock);

	return rv;
}

bool
callout_invoking(callout_t *cs)
{
	callout_impl_t *c = (callout_impl_t *)cs;
	kmutex_t *lock;
	bool rv;

	KASSERT(c->c_magic == CALLOUT_MAGIC);

	lock = callout_lock(c);
	rv = ((c->c_flags & CALLOUT_INVOKING) != 0);
	mutex_spin_exit(lock);

	return rv;
}

void
callout_ack(callout_t *cs)
{
	callout_impl_t *c = (callout_impl_t *)cs;
	kmutex_t *lock;

	KASSERT(c->c_magic == CALLOUT_MAGIC);

	lock = callout_lock(c);
	c->c_flags &= ~CALLOUT_INVOKING;
	mutex_spin_exit(lock);
}

/*
 * callout_hardclock:
 *
 *	Called from hardclock() once every tick.  We schedule a soft
 *	interrupt if there is work to be done.
 */
void
callout_hardclock(void)
{
	struct callout_cpu *cc;
	int needsoftclock, ticks;

	cc = curcpu()->ci_data.cpu_callout;
	mutex_spin_enter(cc->cc_lock);

	ticks = ++cc->cc_ticks;

	MOVEBUCKET(cc, 0, ticks);
	if (MASKWHEEL(0, ticks) == 0) {
		MOVEBUCKET(cc, 1, ticks);
		if (MASKWHEEL(1, ticks) == 0) {
			MOVEBUCKET(cc, 2, ticks);
			if (MASKWHEEL(2, ticks) == 0)
				MOVEBUCKET(cc, 3, ticks);
		}
	}

	needsoftclock = !CIRCQ_EMPTY(&cc->cc_todo);
	mutex_spin_exit(cc->cc_lock);

	if (needsoftclock)
		softint_schedule(callout_sih);
}

/*
 * callout_softclock:
 *
 *	Soft interrupt handler, scheduled above if there is work to
 * 	be done.  Callouts are made in soft interrupt context.
 */
static void
callout_softclock(void *v)
{
	callout_impl_t *c;
	struct callout_cpu *cc;
	void (*func)(void *);
	void *arg;
	int mpsafe, count, ticks, delta;
	lwp_t *l;

	l = curlwp;
	KASSERT(l->l_cpu == curcpu());
	cc = l->l_cpu->ci_data.cpu_callout;

	mutex_spin_enter(cc->cc_lock);
	cc->cc_lwp = l;
	while (!CIRCQ_EMPTY(&cc->cc_todo)) {
		c = CIRCQ_FIRST(&cc->cc_todo);
		KASSERT(c->c_magic == CALLOUT_MAGIC);
		KASSERT(c->c_func != NULL);
		KASSERT(c->c_cpu == cc);
		KASSERT((c->c_flags & CALLOUT_PENDING) != 0);
		KASSERT((c->c_flags & CALLOUT_FIRED) == 0);
		CIRCQ_REMOVE(&c->c_list);

		/* If due run it, otherwise insert it into the right bucket. */
		ticks = cc->cc_ticks;
		delta = c->c_time - ticks;
		if (delta > 0) {
			CIRCQ_INSERT(&c->c_list, BUCKET(cc, delta, c->c_time));
			continue;
		}
		if (delta < 0)
			cc->cc_ev_late.ev_count++;

		c->c_flags = (c->c_flags & ~CALLOUT_PENDING) |
		    (CALLOUT_FIRED | CALLOUT_INVOKING);
		mpsafe = (c->c_flags & CALLOUT_MPSAFE);
		func = c->c_func;
		arg = c->c_arg;
		cc->cc_active = c;

		mutex_spin_exit(cc->cc_lock);
		KASSERT(func != NULL);
		if (__predict_false(!mpsafe)) {
			KERNEL_LOCK(1, NULL);
			(*func)(arg);
			KERNEL_UNLOCK_ONE(NULL);
		} else
			(*func)(arg);
		mutex_spin_enter(cc->cc_lock);

		/*
		 * We can't touch 'c' here because it might be
		 * freed already.  If LWPs waiting for callout
		 * to complete, awaken them.
		 */
		cc->cc_active = NULL;
		if ((count = cc->cc_nwait) != 0) {
			cc->cc_nwait = 0;
			/* sleepq_wake() drops the lock. */
			sleepq_wake(&cc->cc_sleepq, cc, count, cc->cc_lock);
			mutex_spin_enter(cc->cc_lock);
		}
	}
	cc->cc_lwp = NULL;
	mutex_spin_exit(cc->cc_lock);
}
#endif

#ifdef DDB
static void
db_show_callout_bucket(struct callout_cpu *cc, struct callout_circq *bucket)
{
	callout_impl_t *c, ci;
	db_expr_t offset;
	const char *name;
	static char question[] = "?";
	struct callout_circq bi;
	struct callout_cpu cci;
	int b;

	db_read_bytes((db_addr_t)cc, sizeof(cci), (char *)&cci);
	cc = &cci;

	db_read_bytes((db_addr_t)bucket, sizeof(bi), (char *)&bi);

	if (CIRCQ_EMPTY(&bi))
		return;

	for (c = CIRCQ_FIRST(&bi); /*nothing*/; c = CIRCQ_NEXT(&c->c_list)) {
		db_read_bytes((db_addr_t)c, sizeof(ci), (char *)&ci);
		c = &ci;
		db_find_sym_and_offset((db_addr_t)(intptr_t)c->c_func, &name,
		    &offset);
		name = name ? name : question;
		b = (bucket - cc->cc_wheel);
		if (b < 0)
			b = -WHEELSIZE;
		db_printf("%9d %2d/%-4d %16lx  %s\n",
		    c->c_time - cc->cc_ticks, b / WHEELSIZE, b,
		    (u_long)c->c_arg, name);
		if (CIRCQ_LAST(&c->c_list, bucket))
			break;
	}
}

void
db_show_callout(db_expr_t addr, bool haddr, db_expr_t count, const char *modif)
{
	struct callout_cpu *cc;
	struct cpu_info *ci;
	int b;

#ifndef CRASH
	db_printf("hardclock_ticks now: %d\n", hardclock_ticks);
#endif
	db_printf("    ticks  wheel               arg  func\n");

	/*
	 * Don't lock the callwheel; all the other CPUs are paused
	 * anyhow, and we might be called in a circumstance where
	 * some other CPU was paused while holding the lock.
	 */
	for (ci = db_cpu_first(); ci != NULL; ci = db_cpu_next(ci)) {
		db_read_bytes((db_addr_t)&ci->ci_data.cpu_callout,
		    sizeof(cc), (char *)&cc);
		db_show_callout_bucket(cc, &cc->cc_todo);
	}
	for (b = 0; b < BUCKETS; b++) {
		for (ci = db_cpu_first(); ci != NULL; ci = db_cpu_next(ci)) {
			db_read_bytes((db_addr_t)&ci->ci_data.cpu_callout,
			    sizeof(cc), (char *)&cc);
			db_show_callout_bucket(cc, &cc->cc_wheel[b]);
		}
	}
}
#endif /* DDB */
