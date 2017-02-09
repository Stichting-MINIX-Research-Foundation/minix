/*	$NetBSD: locks_up.c,v 1.9 2013/05/06 16:28:17 pooka Exp $	*/

/*
 * Copyright (c) 2010 Antti Kantee.  All Rights Reserved.
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

/*
 * Virtual uniprocessor rump kernel version of locks.  Since the entire
 * kernel is running on only one CPU in the system, there is no need 
 * to perform slow cache-coherent MP locking operations.  This speeds
 * up things quite dramatically and is a good example of that two
 * disjoint kernels running simultaneously in an MP system can be
 * massively faster than one with fine-grained locking.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: locks_up.c,v 1.9 2013/05/06 16:28:17 pooka Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>

#include <rump/rumpuser.h>

#include "rump_private.h"

struct upmtx {
	struct lwp *upm_owner;
	int upm_wanted;
	struct rumpuser_cv *upm_rucv;
};
#define UPMTX(mtx) struct upmtx *upm = *(struct upmtx **)mtx

static inline void
checkncpu(void)
{

	if (__predict_false(ncpu != 1))
		panic("UP lock implementation requires RUMP_NCPU == 1");
}

void
mutex_init(kmutex_t *mtx, kmutex_type_t type, int ipl)
{
	struct upmtx *upm;

	CTASSERT(sizeof(kmutex_t) >= sizeof(void *));
	checkncpu();

	/*
	 * In uniprocessor locking we don't need to differentiate
	 * between spin mutexes and adaptive ones.  We could
	 * replace mutex_enter() with a NOP for spin mutexes, but
	 * not bothering with that for now.
	 */

	/*
	 * XXX: pool_cache would be nice, but not easily possible,
	 * as pool cache init wants to call mutex_init() ...
	 */
	upm = rump_hypermalloc(sizeof(*upm), 0, true, "mutex_init");
	memset(upm, 0, sizeof(*upm));
	rumpuser_cv_init(&upm->upm_rucv);
	memcpy(mtx, &upm, sizeof(void *));
}

void
mutex_destroy(kmutex_t *mtx)
{
	UPMTX(mtx);

	KASSERT(upm->upm_owner == NULL);
	KASSERT(upm->upm_wanted == 0);
	rumpuser_cv_destroy(upm->upm_rucv);
	rump_hyperfree(upm, sizeof(*upm));
}

void
mutex_enter(kmutex_t *mtx)
{
	UPMTX(mtx);

	/* fastpath? */
	if (mutex_tryenter(mtx))
		return;

	/*
	 * No?  bummer, do it the slow and painful way then.
	 */
	upm->upm_wanted++;
	while (!mutex_tryenter(mtx)) {
		rump_schedlock_cv_wait(upm->upm_rucv);
	}
	upm->upm_wanted--;

	KASSERT(upm->upm_wanted >= 0);
}

void
mutex_spin_enter(kmutex_t *mtx)
{

	mutex_enter(mtx);
}

int
mutex_tryenter(kmutex_t *mtx)
{
	UPMTX(mtx);

	if (upm->upm_owner)
		return 0;

	upm->upm_owner = curlwp;
	return 1;
}

void
mutex_exit(kmutex_t *mtx)
{
	UPMTX(mtx);

	if (upm->upm_wanted) {
		rumpuser_cv_signal(upm->upm_rucv); /* CPU is our interlock */
	}
	upm->upm_owner = NULL;
}

void
mutex_spin_exit(kmutex_t *mtx)
{

	mutex_exit(mtx);
}

int
mutex_owned(kmutex_t *mtx)
{
	UPMTX(mtx);

	return upm->upm_owner == curlwp;
}

struct lwp *
mutex_owner(kmutex_t *mtx)
{
	UPMTX(mtx);

	return upm->upm_owner;
}

struct uprw {
	struct lwp *uprw_owner;
	int uprw_readers;
	uint16_t uprw_rwant;
	uint16_t uprw_wwant;
	struct rumpuser_cv *uprw_rucv_reader;
	struct rumpuser_cv *uprw_rucv_writer;
};

#define UPRW(rw) struct uprw *uprw = *(struct uprw **)rw

/* reader/writer locks */

void
rw_init(krwlock_t *rw)
{
	struct uprw *uprw;

	CTASSERT(sizeof(krwlock_t) >= sizeof(void *));
	checkncpu();

	uprw = rump_hypermalloc(sizeof(*uprw), 0, true, "rwinit");
	memset(uprw, 0, sizeof(*uprw));
	rumpuser_cv_init(&uprw->uprw_rucv_reader);
	rumpuser_cv_init(&uprw->uprw_rucv_writer);
	memcpy(rw, &uprw, sizeof(void *));
}

void
rw_destroy(krwlock_t *rw)
{
	UPRW(rw);

	rumpuser_cv_destroy(uprw->uprw_rucv_reader);
	rumpuser_cv_destroy(uprw->uprw_rucv_writer);
	rump_hyperfree(uprw, sizeof(*uprw));
}

/* take rwlock.  prefer writers over readers (see rw_tryenter and rw_exit) */
void
rw_enter(krwlock_t *rw, const krw_t op)
{
	UPRW(rw);
	struct rumpuser_cv *rucv;
	uint16_t *wp;

	if (rw_tryenter(rw, op))
		return;

	/* lagpath */
	if (op == RW_READER) {
		rucv = uprw->uprw_rucv_reader;
		wp = &uprw->uprw_rwant;
	} else {
		rucv = uprw->uprw_rucv_writer;
		wp = &uprw->uprw_wwant;
	}

	(*wp)++;
	while (!rw_tryenter(rw, op)) {
		rump_schedlock_cv_wait(rucv);
	}
	(*wp)--;
}

int
rw_tryenter(krwlock_t *rw, const krw_t op)
{
	UPRW(rw);

	switch (op) {
	case RW_READER:
		if (uprw->uprw_owner == NULL && uprw->uprw_wwant == 0) {
			uprw->uprw_readers++;
			return 1;
		}
		break;
	case RW_WRITER:
		if (uprw->uprw_owner == NULL && uprw->uprw_readers == 0) {
			uprw->uprw_owner = curlwp;
			return 1;
		}
		break;
	}

	return 0;
}

void
rw_exit(krwlock_t *rw)
{
	UPRW(rw);

	if (uprw->uprw_readers > 0) {
		uprw->uprw_readers--;
	} else {
		KASSERT(uprw->uprw_owner == curlwp);
		uprw->uprw_owner = NULL;
	}

	if (uprw->uprw_wwant) {
		rumpuser_cv_signal(uprw->uprw_rucv_writer);
	} else if (uprw->uprw_rwant) {
		rumpuser_cv_signal(uprw->uprw_rucv_reader);
	}
}

int
rw_tryupgrade(krwlock_t *rw)
{
	UPRW(rw);

	if (uprw->uprw_readers == 1 && uprw->uprw_owner == NULL) {
		uprw->uprw_readers = 0;
		uprw->uprw_owner = curlwp;
		return 1;
	} else {
		return 0;
	}
}

int
rw_write_held(krwlock_t *rw)
{
	UPRW(rw);

	return uprw->uprw_owner == curlwp;
}

int
rw_read_held(krwlock_t *rw)
{
	UPRW(rw);

	return uprw->uprw_readers > 0;
}

int
rw_lock_held(krwlock_t *rw)
{
	UPRW(rw);

	return uprw->uprw_owner || uprw->uprw_readers;
}


/*
 * Condvars are almost the same as in the MP case except that we
 * use the scheduler mutex as the pthread interlock instead of the
 * mutex associated with the condvar.
 */

#define RUMPCV(cv) (*(struct rumpuser_cv **)(cv))

void
cv_init(kcondvar_t *cv, const char *msg)
{

	CTASSERT(sizeof(kcondvar_t) >= sizeof(void *));
	checkncpu();

	rumpuser_cv_init((struct rumpuser_cv **)cv);
}

void
cv_destroy(kcondvar_t *cv)
{

	rumpuser_cv_destroy(RUMPCV(cv));
}

void
cv_wait(kcondvar_t *cv, kmutex_t *mtx)
{
#ifdef DIAGNOSTIC
	UPMTX(mtx);
	KASSERT(upm->upm_owner == curlwp);

	if (rump_threads == 0)
		panic("cv_wait without threads");
#endif

	/*
	 * NOTE: we must atomically release the *CPU* here, i.e.
	 * nothing between mutex_exit and entering rumpuser condwait
	 * may preempt us from the virtual CPU.
	 */
	mutex_exit(mtx);
	rump_schedlock_cv_wait(RUMPCV(cv));
	mutex_enter(mtx);
}

int
cv_wait_sig(kcondvar_t *cv, kmutex_t *mtx)
{

	cv_wait(cv, mtx);
	return 0;
}

int
cv_timedwait(kcondvar_t *cv, kmutex_t *mtx, int ticks)
{
	struct timespec ts;

#ifdef DIAGNOSTIC
	UPMTX(mtx);
	KASSERT(upm->upm_owner == curlwp);
#endif

	ts.tv_sec = ticks / hz;
	ts.tv_nsec = (ticks % hz) * (1000000000/hz);

	if (ticks == 0) {
		cv_wait(cv, mtx);
		return 0;
	} else {
		int rv;
		mutex_exit(mtx);
		rv = rump_schedlock_cv_timedwait(RUMPCV(cv), &ts);
		mutex_enter(mtx);
		if (rv)
			return EWOULDBLOCK;
		else
			return 0;
	}
}

int
cv_timedwait_sig(kcondvar_t *cv, kmutex_t *mtx, int ticks)
{

	return cv_timedwait(cv, mtx, ticks);
}

void
cv_signal(kcondvar_t *cv)
{

	/* CPU == interlock */
	rumpuser_cv_signal(RUMPCV(cv));
}

void
cv_broadcast(kcondvar_t *cv)
{

	/* CPU == interlock */
	rumpuser_cv_broadcast(RUMPCV(cv));
}

bool
cv_has_waiters(kcondvar_t *cv)
{
	int n;

	rumpuser_cv_has_waiters(RUMPCV(cv), &n);

	return n > 0;
}

/* this is not much of an attempt, but ... */
bool
cv_is_valid(kcondvar_t *cv)
{

	return RUMPCV(cv) != NULL;
}
