/*	$NetBSD: rumpuser_pth.c,v 1.45 2015/09/18 10:56:25 pooka Exp $	*/

/*
 * Copyright (c) 2007-2010 Antti Kantee.  All Rights Reserved.
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

#include "rumpuser_port.h"

#if !defined(lint)
__RCSID("$NetBSD: rumpuser_pth.c,v 1.45 2015/09/18 10:56:25 pooka Exp $");
#endif /* !lint */

#include <sys/queue.h>

#if defined(HAVE_SYS_ATOMIC_H)
#include <sys/atomic.h>
#endif

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include <rump/rumpuser.h>

#include "rumpuser_int.h"

int
rumpuser_thread_create(void *(*f)(void *), void *arg, const char *thrname,
	int joinable, int priority, int cpuidx, void **ptcookie)
{
	pthread_t ptid;
	pthread_t *ptidp;
	pthread_attr_t pattr;
	int rv, i;

	if ((rv = pthread_attr_init(&pattr)) != 0)
		return rv;

	if (joinable) {
		NOFAIL(ptidp = malloc(sizeof(*ptidp)));
		pthread_attr_setdetachstate(&pattr, PTHREAD_CREATE_JOINABLE);
	} else {
		ptidp = &ptid;
		pthread_attr_setdetachstate(&pattr, PTHREAD_CREATE_DETACHED);
	}

	for (i = 0; i < 10; i++) {
		const struct timespec ts = {0, 10*1000*1000};

		rv = pthread_create(ptidp, &pattr, f, arg);
		if (rv != EAGAIN)
			break;
		nanosleep(&ts, NULL);
	}

#if defined(HAVE_PTHREAD_SETNAME3)
	if (rv == 0 && thrname) {
		pthread_setname_np(*ptidp, thrname, NULL);
	}
#elif defined(HAVE_PTHREAD_SETNAME2)
	if (rv == 0 && thrname) {
		pthread_setname_np(*ptidp, thrname);
	}
#endif

	if (joinable) {
		assert(ptcookie);
		*ptcookie = ptidp;
	}

	pthread_attr_destroy(&pattr);

	ET(rv);
}

__dead void
rumpuser_thread_exit(void)
{

	/*
	 * FIXXXME: with glibc on ARM pthread_exit() aborts because
	 * it fails to unwind the stack.  In the typical case, only
	 * the mountroothook thread will exit and even that's
	 * conditional on vfs being present.
	 */
#if (defined(__ARMEL__) || defined(__ARMEB__)) && defined(__GLIBC__)
	for (;;)
		pause();
#endif

	pthread_exit(NULL);
}

int
rumpuser_thread_join(void *ptcookie)
{
	pthread_t *pt = ptcookie;
	int rv;

	KLOCK_WRAP((rv = pthread_join(*pt, NULL)));
	if (rv == 0)
		free(pt);

	ET(rv);
}

struct rumpuser_mtx {
	pthread_mutex_t pthmtx;
	struct lwp *owner;
	int flags;
};

void
rumpuser_mutex_init(struct rumpuser_mtx **mtxp, int flags)
{
	struct rumpuser_mtx *mtx;
	pthread_mutexattr_t att;
	size_t allocsz;

	allocsz = (sizeof(*mtx)+RUMPUSER_LOCKALIGN) & ~(RUMPUSER_LOCKALIGN-1);
	NOFAIL(mtx = aligned_alloc(RUMPUSER_LOCKALIGN, allocsz));

	pthread_mutexattr_init(&att);
	pthread_mutexattr_settype(&att, PTHREAD_MUTEX_ERRORCHECK);
	NOFAIL_ERRNO(pthread_mutex_init(&mtx->pthmtx, &att));
	pthread_mutexattr_destroy(&att);

	mtx->owner = NULL;
	assert(flags != 0);
	mtx->flags = flags;

	*mtxp = mtx;
}

static void
mtxenter(struct rumpuser_mtx *mtx)
{

	if (!(mtx->flags & RUMPUSER_MTX_KMUTEX))
		return;

	assert(mtx->owner == NULL);
	mtx->owner = rumpuser_curlwp();
}

static void
mtxexit(struct rumpuser_mtx *mtx)
{

	if (!(mtx->flags & RUMPUSER_MTX_KMUTEX))
		return;

	assert(mtx->owner != NULL);
	mtx->owner = NULL;
}

void
rumpuser_mutex_enter(struct rumpuser_mtx *mtx)
{

	if (mtx->flags & RUMPUSER_MTX_SPIN) {
		rumpuser_mutex_enter_nowrap(mtx);
		return;
	}

	assert(mtx->flags & RUMPUSER_MTX_KMUTEX);
	if (pthread_mutex_trylock(&mtx->pthmtx) != 0)
		KLOCK_WRAP(NOFAIL_ERRNO(pthread_mutex_lock(&mtx->pthmtx)));
	mtxenter(mtx);
}

void
rumpuser_mutex_enter_nowrap(struct rumpuser_mtx *mtx)
{

	assert(mtx->flags & RUMPUSER_MTX_SPIN);
	NOFAIL_ERRNO(pthread_mutex_lock(&mtx->pthmtx));
	mtxenter(mtx);
}

int
rumpuser_mutex_tryenter(struct rumpuser_mtx *mtx)
{
	int rv;

	rv = pthread_mutex_trylock(&mtx->pthmtx);
	if (rv == 0) {
		mtxenter(mtx);
	}

	ET(rv);
}

void
rumpuser_mutex_exit(struct rumpuser_mtx *mtx)
{

	mtxexit(mtx);
	NOFAIL_ERRNO(pthread_mutex_unlock(&mtx->pthmtx));
}

void
rumpuser_mutex_destroy(struct rumpuser_mtx *mtx)
{

	NOFAIL_ERRNO(pthread_mutex_destroy(&mtx->pthmtx));
	free(mtx);
}

void
rumpuser_mutex_owner(struct rumpuser_mtx *mtx, struct lwp **lp)
{

	if (__predict_false(!(mtx->flags & RUMPUSER_MTX_KMUTEX))) {
		printf("panic: rumpuser_mutex_held unsupported on non-kmtx\n");
		abort();
	}

	*lp = mtx->owner;
}

/*
 * rwlocks.  these are mostly simple, except that NetBSD wants to
 * support something called downgrade, which means we need to swap
 * our exclusive lock for a shared lock.  to accommodate this,
 * we need to check *after* acquiring a lock in case someone was
 * downgrading it.  if so, we couldn't actually have it and maybe
 * need to retry later.
 */

struct rumpuser_rw {
	pthread_rwlock_t pthrw;
#if !defined(__APPLE__) && !defined(__ANDROID__)
	char pad[64 - sizeof(pthread_rwlock_t)];
	pthread_spinlock_t spin;
#endif
	unsigned int readers;
	struct lwp *writer;
	int downgrade; /* someone is downgrading (hopefully lock holder ;) */
};

static int
rw_amwriter(struct rumpuser_rw *rw)
{

	return rw->writer == rumpuser_curlwp() && rw->readers == (unsigned)-1;
}

static int
rw_nreaders(struct rumpuser_rw *rw)
{
	unsigned nreaders = rw->readers;

	return nreaders != (unsigned)-1 ? nreaders : 0;
}

static int
rw_setwriter(struct rumpuser_rw *rw, int retry)
{

	/*
	 * Don't need the spinlock here, we already have an
	 * exclusive lock and "downgrade" is stable until complete.
	 */
	if (rw->downgrade) {
		pthread_rwlock_unlock(&rw->pthrw);
		if (retry) {
			struct timespec ts;

			/* portable yield, essentially */
			ts.tv_sec = 0;
			ts.tv_nsec = 1;
			KLOCK_WRAP(nanosleep(&ts, NULL));
		}
		return EBUSY;
	}
	assert(rw->readers == 0);
	rw->writer = rumpuser_curlwp();
	rw->readers = (unsigned)-1;
	return 0;
}

static void
rw_clearwriter(struct rumpuser_rw *rw)
{

	assert(rw_amwriter(rw));
	rw->readers = 0;
	rw->writer = NULL;
}

static inline void
rw_readup(struct rumpuser_rw *rw)
{

#if defined(__NetBSD__) || defined(__APPLE__) || defined(__ANDROID__)
	atomic_inc_uint(&rw->readers);
#else
	pthread_spin_lock(&rw->spin);
	++rw->readers;
	pthread_spin_unlock(&rw->spin);
#endif
}

static inline void
rw_readdown(struct rumpuser_rw *rw)
{

#if defined(__NetBSD__) || defined(__APPLE__) || defined(__ANDROID__)
	atomic_dec_uint(&rw->readers);
#else
	pthread_spin_lock(&rw->spin);
	assert(rw->readers > 0);
	--rw->readers;
	pthread_spin_unlock(&rw->spin);
#endif
}

void
rumpuser_rw_init(struct rumpuser_rw **rwp)
{
	struct rumpuser_rw *rw;
	size_t allocsz;

	allocsz = (sizeof(*rw)+RUMPUSER_LOCKALIGN) & ~(RUMPUSER_LOCKALIGN-1);

	NOFAIL(rw = aligned_alloc(RUMPUSER_LOCKALIGN, allocsz));
	NOFAIL_ERRNO(pthread_rwlock_init(&rw->pthrw, NULL));
#if !defined(__APPLE__) && !defined(__ANDROID__)
	NOFAIL_ERRNO(pthread_spin_init(&rw->spin, PTHREAD_PROCESS_PRIVATE));
#endif
	rw->readers = 0;
	rw->writer = NULL;
	rw->downgrade = 0;

	*rwp = rw;
}

void
rumpuser_rw_enter(int enum_rumprwlock, struct rumpuser_rw *rw)
{
	enum rumprwlock lk = enum_rumprwlock;

	switch (lk) {
	case RUMPUSER_RW_WRITER:
		do {
			if (pthread_rwlock_trywrlock(&rw->pthrw) != 0)
				KLOCK_WRAP(NOFAIL_ERRNO(
				    pthread_rwlock_wrlock(&rw->pthrw)));
		} while (rw_setwriter(rw, 1) != 0);
		break;
	case RUMPUSER_RW_READER:
		if (pthread_rwlock_tryrdlock(&rw->pthrw) != 0)
			KLOCK_WRAP(NOFAIL_ERRNO(
			    pthread_rwlock_rdlock(&rw->pthrw)));
		rw_readup(rw);
		break;
	}
}

int
rumpuser_rw_tryenter(int enum_rumprwlock, struct rumpuser_rw *rw)
{
	enum rumprwlock lk = enum_rumprwlock;
	int rv;

	switch (lk) {
	case RUMPUSER_RW_WRITER:
		rv = pthread_rwlock_trywrlock(&rw->pthrw);
		if (rv == 0)
			rv = rw_setwriter(rw, 0);
		break;
	case RUMPUSER_RW_READER:
		rv = pthread_rwlock_tryrdlock(&rw->pthrw);
		if (rv == 0)
			rw_readup(rw);
		break;
	default:
		rv = EINVAL;
		break;
	}

	ET(rv);
}

int
rumpuser_rw_tryupgrade(struct rumpuser_rw *rw)
{

	/*
	 * Not supported by pthreads.  Since the caller needs to
	 * back off anyway to avoid deadlock, always failing
	 * is correct.
	 */
	ET(EBUSY);
}

/*
 * convert from exclusive to shared lock without allowing anyone to
 * obtain an exclusive lock in between.  actually, might allow
 * someone to obtain the lock, we just don't allow that thread to
 * return from the hypercall with it.
 */
void
rumpuser_rw_downgrade(struct rumpuser_rw *rw)
{

	assert(rw->downgrade == 0);
	rw->downgrade = 1;
	rumpuser_rw_exit(rw);
	/*
	 * though the competition can't get out of the hypervisor, it
	 * might have rescheduled itself after we released the lock.
	 * so need a wrap here.
	 */
	KLOCK_WRAP(NOFAIL_ERRNO(pthread_rwlock_rdlock(&rw->pthrw)));
	rw->downgrade = 0;
	rw_readup(rw);
}

void
rumpuser_rw_exit(struct rumpuser_rw *rw)
{

	if (rw_nreaders(rw))
		rw_readdown(rw);
	else
		rw_clearwriter(rw);
	NOFAIL_ERRNO(pthread_rwlock_unlock(&rw->pthrw));
}

void
rumpuser_rw_destroy(struct rumpuser_rw *rw)
{

	NOFAIL_ERRNO(pthread_rwlock_destroy(&rw->pthrw));
#if !defined(__APPLE__) && ! defined(__ANDROID__)
	NOFAIL_ERRNO(pthread_spin_destroy(&rw->spin));
#endif
	free(rw);
}

void
rumpuser_rw_held(int enum_rumprwlock, struct rumpuser_rw *rw, int *rv)
{
	enum rumprwlock lk = enum_rumprwlock;

	switch (lk) {
	case RUMPUSER_RW_WRITER:
		*rv = rw_amwriter(rw);
		break;
	case RUMPUSER_RW_READER:
		*rv = rw_nreaders(rw);
		break;
	}
}

/*
 * condvar
 */

struct rumpuser_cv {
	pthread_cond_t pthcv;
	int nwaiters;
};

void
rumpuser_cv_init(struct rumpuser_cv **cv)
{

	NOFAIL(*cv = malloc(sizeof(struct rumpuser_cv)));
	NOFAIL_ERRNO(pthread_cond_init(&((*cv)->pthcv), NULL));
	(*cv)->nwaiters = 0;
}

void
rumpuser_cv_destroy(struct rumpuser_cv *cv)
{

	NOFAIL_ERRNO(pthread_cond_destroy(&cv->pthcv));
	free(cv);
}

static void
cv_unschedule(struct rumpuser_mtx *mtx, int *nlocks)
{

	rumpkern_unsched(nlocks, mtx);
	mtxexit(mtx);
}

static void
cv_reschedule(struct rumpuser_mtx *mtx, int nlocks)
{

	/*
	 * If the cv interlock is a spin mutex, we must first release
	 * the mutex that was reacquired by pthread_cond_wait(),
	 * acquire the CPU context and only then relock the mutex.
	 * This is to preserve resource allocation order so that
	 * we don't deadlock.  Non-spinning mutexes don't have this
	 * problem since they don't use a hold-and-wait approach
	 * to acquiring the mutex wrt the rump kernel CPU context.
	 *
	 * The more optimal solution would be to rework rumpkern_sched()
	 * so that it's possible to tell the scheduler
	 * "if you need to block, drop this lock first", but I'm not
	 * going poking there without some numbers on how often this
	 * path is taken for spin mutexes.
	 */
	if ((mtx->flags & (RUMPUSER_MTX_SPIN | RUMPUSER_MTX_KMUTEX)) ==
	    (RUMPUSER_MTX_SPIN | RUMPUSER_MTX_KMUTEX)) {
		NOFAIL_ERRNO(pthread_mutex_unlock(&mtx->pthmtx));
		rumpkern_sched(nlocks, mtx);
		rumpuser_mutex_enter_nowrap(mtx);
	} else {
		mtxenter(mtx);
		rumpkern_sched(nlocks, mtx);
	}
}

void
rumpuser_cv_wait(struct rumpuser_cv *cv, struct rumpuser_mtx *mtx)
{
	int nlocks;

	cv->nwaiters++;
	cv_unschedule(mtx, &nlocks);
	NOFAIL_ERRNO(pthread_cond_wait(&cv->pthcv, &mtx->pthmtx));
	cv_reschedule(mtx, nlocks);
	cv->nwaiters--;
}

void
rumpuser_cv_wait_nowrap(struct rumpuser_cv *cv, struct rumpuser_mtx *mtx)
{

	cv->nwaiters++;
	mtxexit(mtx);
	NOFAIL_ERRNO(pthread_cond_wait(&cv->pthcv, &mtx->pthmtx));
	mtxenter(mtx);
	cv->nwaiters--;
}

int
rumpuser_cv_timedwait(struct rumpuser_cv *cv, struct rumpuser_mtx *mtx,
	int64_t sec, int64_t nsec)
{
	struct timespec ts;
	int rv, nlocks;

	/*
	 * Get clock already here, just in case we will be put to sleep
	 * after releasing the kernel context.
	 *
	 * The condition variables should use CLOCK_MONOTONIC, but since
	 * that's not available everywhere, leave it for another day.
	 */
	clock_gettime(CLOCK_REALTIME, &ts);

	cv->nwaiters++;
	cv_unschedule(mtx, &nlocks);

	ts.tv_sec += sec;
	ts.tv_nsec += nsec;
	if (ts.tv_nsec >= 1000*1000*1000) {
		ts.tv_sec++;
		ts.tv_nsec -= 1000*1000*1000;
	}
	rv = pthread_cond_timedwait(&cv->pthcv, &mtx->pthmtx, &ts);

	cv_reschedule(mtx, nlocks);
	cv->nwaiters--;

	ET(rv);
}

void
rumpuser_cv_signal(struct rumpuser_cv *cv)
{

	NOFAIL_ERRNO(pthread_cond_signal(&cv->pthcv));
}

void
rumpuser_cv_broadcast(struct rumpuser_cv *cv)
{

	NOFAIL_ERRNO(pthread_cond_broadcast(&cv->pthcv));
}

void
rumpuser_cv_has_waiters(struct rumpuser_cv *cv, int *nwaiters)
{

	*nwaiters = cv->nwaiters;
}

/*
 * curlwp
 */

static pthread_key_t curlwpkey;

/*
 * the if0'd curlwp implementation is not used by this hypervisor,
 * but serves as test code to check that the intended usage works.
 */
#if 0
struct rumpuser_lwp {
	struct lwp *l;
	LIST_ENTRY(rumpuser_lwp) l_entries;
};
static LIST_HEAD(, rumpuser_lwp) lwps = LIST_HEAD_INITIALIZER(lwps);
static pthread_mutex_t lwplock = PTHREAD_MUTEX_INITIALIZER;

void
rumpuser_curlwpop(enum rumplwpop op, struct lwp *l)
{
	struct rumpuser_lwp *rl, *rliter;

	switch (op) {
	case RUMPUSER_LWP_CREATE:
		rl = malloc(sizeof(*rl));
		rl->l = l;
		pthread_mutex_lock(&lwplock);
		LIST_FOREACH(rliter, &lwps, l_entries) {
			if (rliter->l == l) {
				fprintf(stderr, "LWP_CREATE: %p exists\n", l);
				abort();
			}
		}
		LIST_INSERT_HEAD(&lwps, rl, l_entries);
		pthread_mutex_unlock(&lwplock);
		break;
	case RUMPUSER_LWP_DESTROY:
		pthread_mutex_lock(&lwplock);
		LIST_FOREACH(rl, &lwps, l_entries) {
			if (rl->l == l)
				break;
		}
		if (!rl) {
			fprintf(stderr, "LWP_DESTROY: %p does not exist\n", l);
			abort();
		}
		LIST_REMOVE(rl, l_entries);
		pthread_mutex_unlock(&lwplock);
		free(rl);
		break;
	case RUMPUSER_LWP_SET:
		assert(pthread_getspecific(curlwpkey) == NULL && l != NULL);

		pthread_mutex_lock(&lwplock);
		LIST_FOREACH(rl, &lwps, l_entries) {
			if (rl->l == l)
				break;
		}
		if (!rl) {
			fprintf(stderr,
			    "LWP_SET: %p does not exist\n", l);
			abort();
		}
		pthread_mutex_unlock(&lwplock);

		pthread_setspecific(curlwpkey, rl);
		break;
	case RUMPUSER_LWP_CLEAR:
		assert(((struct rumpuser_lwp *)
		    pthread_getspecific(curlwpkey))->l == l);
		pthread_setspecific(curlwpkey, NULL);
		break;
	}
}

struct lwp *
rumpuser_curlwp(void)
{
	struct rumpuser_lwp *rl;

	rl = pthread_getspecific(curlwpkey);
	return rl ? rl->l : NULL;
}

#else

void
rumpuser_curlwpop(int enum_rumplwpop, struct lwp *l)
{
	enum rumplwpop op = enum_rumplwpop;

	switch (op) {
	case RUMPUSER_LWP_CREATE:
		break;
	case RUMPUSER_LWP_DESTROY:
		break;
	case RUMPUSER_LWP_SET:
		assert(pthread_getspecific(curlwpkey) == NULL);
		pthread_setspecific(curlwpkey, l);
		break;
	case RUMPUSER_LWP_CLEAR:
		assert(pthread_getspecific(curlwpkey) == l);
		pthread_setspecific(curlwpkey, NULL);
		break;
	}
}

struct lwp *
rumpuser_curlwp(void)
{

	return pthread_getspecific(curlwpkey);
}
#endif


void
rumpuser__thrinit(void)
{
	pthread_key_create(&curlwpkey, NULL);
}
