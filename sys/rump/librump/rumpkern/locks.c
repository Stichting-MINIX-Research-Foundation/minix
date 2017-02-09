/*	$NetBSD: locks.c,v 1.71 2015/09/30 02:45:33 ozaki-r Exp $	*/

/*
 * Copyright (c) 2007-2011 Antti Kantee.  All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: locks.c,v 1.71 2015/09/30 02:45:33 ozaki-r Exp $");

#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>

#include <rump/rumpuser.h>

#include "rump_private.h"

#ifdef LOCKDEBUG
const int rump_lockdebug = 1;
#else
const int rump_lockdebug = 0;
#endif

/*
 * Simple lockdebug.  If it's compiled in, it's always active.
 * Currently available only for mtx/rwlock.
 */
#ifdef LOCKDEBUG
#include <sys/lockdebug.h>

static lockops_t mutex_lockops = {
	"mutex",
	LOCKOPS_SLEEP,
	NULL
};
static lockops_t rw_lockops = {
	"rwlock",
	LOCKOPS_SLEEP,
	NULL
};

#define ALLOCK(lock, ops)		\
    lockdebug_alloc(lock, ops, (uintptr_t)__builtin_return_address(0))
#define FREELOCK(lock)			\
    lockdebug_free(lock)
#define WANTLOCK(lock, shar)	\
    lockdebug_wantlock(lock, (uintptr_t)__builtin_return_address(0), shar)
#define LOCKED(lock, shar)		\
    lockdebug_locked(lock, NULL, (uintptr_t)__builtin_return_address(0), shar)
#define UNLOCKED(lock, shar)		\
    lockdebug_unlocked(lock, (uintptr_t)__builtin_return_address(0), shar)
#define BARRIER(lock, slp)		\
    lockdebug_barrier(lock, slp)
#else
#define ALLOCK(a, b)
#define FREELOCK(a)
#define WANTLOCK(a, b)
#define LOCKED(a, b)
#define UNLOCKED(a, b)
#define BARRIER(a, b)
#endif

/*
 * We map locks to pthread routines.  The difference between kernel
 * and rumpuser routines is that while the kernel uses static
 * storage, rumpuser allocates the object from the heap.  This
 * indirection is necessary because we don't know the size of
 * pthread objects here.  It is also beneficial, since we can
 * be easily compatible with the kernel ABI because all kernel
 * objects regardless of machine architecture are always at least
 * the size of a pointer.  The downside, of course, is a performance
 * penalty.
 */

#define RUMPMTX(mtx) (*(struct rumpuser_mtx **)(mtx))

void
mutex_init(kmutex_t *mtx, kmutex_type_t type, int ipl)
{
	int ruflags = RUMPUSER_MTX_KMUTEX;
	int isspin;

	CTASSERT(sizeof(kmutex_t) >= sizeof(void *));

	/*
	 * Try to figure out if the caller wanted a spin mutex or
	 * not with this easy set of conditionals.  The difference
	 * between a spin mutex and an adaptive mutex for a rump
	 * kernel is that the hypervisor does not relinquish the
	 * rump kernel CPU context for a spin mutex.  The
	 * hypervisor itself may block even when "spinning".
	 */
	if (type == MUTEX_SPIN) {
		isspin = 1;
	} else if (ipl == IPL_NONE || ipl == IPL_SOFTCLOCK ||
	    ipl == IPL_SOFTBIO || ipl == IPL_SOFTNET ||
	    ipl == IPL_SOFTSERIAL) {
		isspin = 0;
	} else {
		isspin = 1;
	}

	if (isspin)
		ruflags |= RUMPUSER_MTX_SPIN;
	rumpuser_mutex_init((struct rumpuser_mtx **)mtx, ruflags);
	ALLOCK(mtx, &mutex_lockops);
}

void
mutex_destroy(kmutex_t *mtx)
{

	FREELOCK(mtx);
	rumpuser_mutex_destroy(RUMPMTX(mtx));
}

void
mutex_enter(kmutex_t *mtx)
{

	WANTLOCK(mtx, 0);
	BARRIER(mtx, 1);
	rumpuser_mutex_enter(RUMPMTX(mtx));
	LOCKED(mtx, false);
}

void
mutex_spin_enter(kmutex_t *mtx)
{

	WANTLOCK(mtx, 0);
	BARRIER(mtx, 1);
	rumpuser_mutex_enter_nowrap(RUMPMTX(mtx));
	LOCKED(mtx, false);
}

int
mutex_tryenter(kmutex_t *mtx)
{
	int error;

	error = rumpuser_mutex_tryenter(RUMPMTX(mtx));
	if (error == 0) {
		WANTLOCK(mtx, 0);
		LOCKED(mtx, false);
	}
	return error == 0;
}

void
mutex_exit(kmutex_t *mtx)
{

	UNLOCKED(mtx, false);
	rumpuser_mutex_exit(RUMPMTX(mtx));
}
__strong_alias(mutex_spin_exit,mutex_exit);

int
mutex_owned(kmutex_t *mtx)
{

	return mutex_owner(mtx) == curlwp;
}

struct lwp *
mutex_owner(kmutex_t *mtx)
{
	struct lwp *l;

	rumpuser_mutex_owner(RUMPMTX(mtx), &l);
	return l;
}

#define RUMPRW(rw) (*(struct rumpuser_rw **)(rw))

/* reader/writer locks */

static enum rumprwlock
krw2rumprw(const krw_t op)
{

	switch (op) {
	case RW_READER:
		return RUMPUSER_RW_READER;
	case RW_WRITER:
		return RUMPUSER_RW_WRITER;
	default:
		panic("unknown rwlock type");
	}
}

void
rw_init(krwlock_t *rw)
{

	CTASSERT(sizeof(krwlock_t) >= sizeof(void *));

	rumpuser_rw_init((struct rumpuser_rw **)rw);
	ALLOCK(rw, &rw_lockops);
}

void
rw_destroy(krwlock_t *rw)
{

	FREELOCK(rw);
	rumpuser_rw_destroy(RUMPRW(rw));
}

void
rw_enter(krwlock_t *rw, const krw_t op)
{

	WANTLOCK(rw, op == RW_READER);
	BARRIER(rw, 1);
	rumpuser_rw_enter(krw2rumprw(op), RUMPRW(rw));
	LOCKED(rw, op == RW_READER);
}

int
rw_tryenter(krwlock_t *rw, const krw_t op)
{
	int error;

	error = rumpuser_rw_tryenter(krw2rumprw(op), RUMPRW(rw));
	if (error == 0) {
		WANTLOCK(rw, op == RW_READER);
		LOCKED(rw, op == RW_READER);
	}
	return error == 0;
}

void
rw_exit(krwlock_t *rw)
{

#ifdef LOCKDEBUG
	bool shared = !rw_write_held(rw);

	if (shared)
		KASSERT(rw_read_held(rw));
	UNLOCKED(rw, shared);
#endif
	rumpuser_rw_exit(RUMPRW(rw));
}

int
rw_tryupgrade(krwlock_t *rw)
{
	int rv;

	rv = rumpuser_rw_tryupgrade(RUMPRW(rw));
	if (rv == 0) {
		UNLOCKED(rw, 1);
		WANTLOCK(rw, 0);
		LOCKED(rw, 0);
	}
	return rv == 0;
}

void
rw_downgrade(krwlock_t *rw)
{

	rumpuser_rw_downgrade(RUMPRW(rw));
	UNLOCKED(rw, 0);
	WANTLOCK(rw, 1);
	LOCKED(rw, 1);
}

int
rw_read_held(krwlock_t *rw)
{
	int rv;

	rumpuser_rw_held(RUMPUSER_RW_READER, RUMPRW(rw), &rv);
	return rv;
}

int
rw_write_held(krwlock_t *rw)
{
	int rv;

	rumpuser_rw_held(RUMPUSER_RW_WRITER, RUMPRW(rw), &rv);
	return rv;
}

int
rw_lock_held(krwlock_t *rw)
{

	return rw_read_held(rw) || rw_write_held(rw);
}

/* curriculum vitaes */

#define RUMPCV(cv) (*(struct rumpuser_cv **)(cv))

void
cv_init(kcondvar_t *cv, const char *msg)
{

	CTASSERT(sizeof(kcondvar_t) >= sizeof(void *));

	rumpuser_cv_init((struct rumpuser_cv **)cv);
}

void
cv_destroy(kcondvar_t *cv)
{

	rumpuser_cv_destroy(RUMPCV(cv));
}

static int
docvwait(kcondvar_t *cv, kmutex_t *mtx, struct timespec *ts)
{
	struct lwp *l = curlwp;
	int rv;

	if (__predict_false(l->l_flag & LW_RUMP_QEXIT)) {
		/*
		 * yield() here, someone might want the cpu
		 * to set a condition.  otherwise we'll just
		 * loop forever.
		 */
		yield();
		return EINTR;
	}

	UNLOCKED(mtx, false);

	l->l_private = cv;
	rv = 0;
	if (ts) {
		if (rumpuser_cv_timedwait(RUMPCV(cv), RUMPMTX(mtx),
		    ts->tv_sec, ts->tv_nsec))
			rv = EWOULDBLOCK;
	} else {
		rumpuser_cv_wait(RUMPCV(cv), RUMPMTX(mtx));
	}

	LOCKED(mtx, false);

	/*
	 * Check for QEXIT.  if so, we need to wait here until we
	 * are allowed to exit.
	 */
	if (__predict_false(l->l_flag & LW_RUMP_QEXIT)) {
		struct proc *p = l->l_proc;

		mutex_exit(mtx); /* drop and retake later */

		mutex_enter(p->p_lock);
		while ((p->p_sflag & PS_RUMP_LWPEXIT) == 0) {
			/* avoid recursion */
			rumpuser_cv_wait(RUMPCV(&p->p_waitcv),
			    RUMPMTX(p->p_lock));
		}
		KASSERT(p->p_sflag & PS_RUMP_LWPEXIT);
		mutex_exit(p->p_lock);

		/* ok, we can exit and remove "reference" to l->private */

		mutex_enter(mtx);
		rv = EINTR;
	}
	l->l_private = NULL;

	return rv;
}

void
cv_wait(kcondvar_t *cv, kmutex_t *mtx)
{

	if (__predict_false(rump_threads == 0))
		panic("cv_wait without threads");
	(void) docvwait(cv, mtx, NULL);
}

int
cv_wait_sig(kcondvar_t *cv, kmutex_t *mtx)
{

	if (__predict_false(rump_threads == 0))
		panic("cv_wait without threads");
	return docvwait(cv, mtx, NULL);
}

int
cv_timedwait(kcondvar_t *cv, kmutex_t *mtx, int ticks)
{
	struct timespec ts;
	extern int hz;
	int rv;

	if (ticks == 0) {
		rv = cv_wait_sig(cv, mtx);
	} else {
		ts.tv_sec = ticks / hz;
		ts.tv_nsec = (ticks % hz) * (1000000000/hz);
		rv = docvwait(cv, mtx, &ts);
	}

	return rv;
}
__strong_alias(cv_timedwait_sig,cv_timedwait);

void
cv_signal(kcondvar_t *cv)
{

	rumpuser_cv_signal(RUMPCV(cv));
}

void
cv_broadcast(kcondvar_t *cv)
{

	rumpuser_cv_broadcast(RUMPCV(cv));
}

bool
cv_has_waiters(kcondvar_t *cv)
{
	int rv;

	rumpuser_cv_has_waiters(RUMPCV(cv), &rv);
	return rv != 0;
}

/* this is not much of an attempt, but ... */
bool
cv_is_valid(kcondvar_t *cv)
{

	return RUMPCV(cv) != NULL;
}
