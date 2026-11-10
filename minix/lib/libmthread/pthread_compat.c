#define _MTHREADIFY_PTHREADS
#include <minix/mthread.h>
#include "global.h"
#include "proto.h"

/* WARNING:
 * The following works under the hypothesis that we have only green threads,
 * which implies that we have no preemption, unless explicit yield or possible
 * calls done to mthread functions.
 *
 * This has impact on the fact we do not maintain a table of currently being
 * initialized mutexes or condition variables, to prevent double initialization
 * and/or TOCTU problems. TOCTU could appear between the test against the
 * initializer value, and the actual initialization, which could lead to double
 * initialization of the same mutex AND get two threads at the same time in the
 * critical section as they both hold a (different) mutex.
 *
 * Besides the static initializers, a zero-filled (NULL) mutex or condition
 * variable is also initialized on first use: ported software allocates
 * them with calloc()/memset() and relies on that, as with glibc.
 */

#include <stdint.h>

/* Static initializer, or zero-filled memory: initialize on first use. */
#define MUTEX_LAZY(m)	(*(m) == PTHREAD_MUTEX_INITIALIZER || *(m) == NULL)
#define COND_LAZY(c)	(*(c) == PTHREAD_COND_INITIALIZER || *(c) == NULL)


/*===========================================================================*
 *				pthread_mutex_init			     *
 *===========================================================================*/
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *mattr)
{
	mthread_mutexattr_t a;

	/* The attribute is a pointer-sized value: pass a copy (no cast-qual). */
	if (mattr == NULL)
		return mthread_mutex_init(mutex, NULL);
	a = *mattr;
	return mthread_mutex_init(mutex, &a);
}

/*===========================================================================*
 *				pthread_mutex_destroy			     *
 *===========================================================================*/
int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	if (MUTEX_LAZY(mutex)) {
		*mutex = NULL;
		return 0;
	}

	return mthread_mutex_destroy(mutex);
}

/*===========================================================================*
 *				pthread_mutex_lock			     *
 *===========================================================================*/
int pthread_mutex_lock(pthread_mutex_t *mutex)
{
	if (MUTEX_LAZY(mutex)) {
		mthread_mutex_init(mutex, NULL);	
	}

	return mthread_mutex_lock(mutex);
}

/*===========================================================================*
 *				pthread_mutex_trylock			     *
 *===========================================================================*/
int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	if (MUTEX_LAZY(mutex)) {
		mthread_mutex_init(mutex, NULL);	
	}

	return mthread_mutex_trylock(mutex);
}

/*===========================================================================*
 *				pthread_mutex_unlock			     *
 *===========================================================================*/
int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	if (MUTEX_LAZY(mutex)) {
		mthread_mutex_init(mutex, NULL);	
	}

	return mthread_mutex_unlock(mutex);
}

/*===========================================================================*
 *				pthread_cond_init			     *
 *===========================================================================*/
int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *cattr)
{
	mthread_condattr_t a;

	if (cattr == NULL)
		return mthread_cond_init(cond, NULL);
	a = *cattr;
	return mthread_cond_init(cond, &a);
}

/*===========================================================================*
 *				pthread_cond_broadcast			     *
 *===========================================================================*/
int pthread_cond_broadcast(pthread_cond_t *cond)
{
	if (COND_LAZY(cond)) {
		mthread_cond_init(cond, NULL);
	}

	return mthread_cond_broadcast(cond);
}

/*===========================================================================*
 *				pthread_cond_destroy			     *
 *===========================================================================*/
int pthread_cond_destroy(pthread_cond_t *cond)
{
	if (COND_LAZY(cond)) {
		*cond = NULL;
		return 0;
	}

	return mthread_cond_destroy(cond);
}

/*===========================================================================*
 *				pthread_cond_signal			     *
 *===========================================================================*/
int pthread_cond_signal(pthread_cond_t *cond)
{
	if (COND_LAZY(cond)) {
		mthread_cond_init(cond, NULL);
	}

	return mthread_cond_signal(cond);
}

/*===========================================================================*
 *				pthread_cond_wait			     *
 *===========================================================================*/
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
	if (COND_LAZY(cond)) {
		mthread_cond_init(cond, NULL);
	}

	if (MUTEX_LAZY(mutex)) {
		mthread_mutex_init(mutex, NULL);
	}

	return mthread_cond_wait(cond, mutex);
}

/*===========================================================================*
 *				pthread_condattr_setclock		     *
 *===========================================================================*/
int pthread_condattr_setclock(pthread_condattr_t *attr, clockid_t clock_id)
{
	/* The attribute is a pointer-sized value: NULL for the default clock
	 * (CLOCK_REALTIME), otherwise the clock id + 1. */
	if (attr == NULL)
		return EINVAL;
	switch (clock_id) {
	case CLOCK_REALTIME:
		*attr = NULL;
		return 0;
	case CLOCK_MONOTONIC:
		*attr = (pthread_condattr_t) ((uintptr_t) clock_id + 1);
		return 0;
	default:
		return EINVAL;
	}
}

/*===========================================================================*
 *				pthread_condattr_getclock		     *
 *===========================================================================*/
int pthread_condattr_getclock(const pthread_condattr_t *attr,
	clockid_t *clock_id)
{
	if (attr == NULL || clock_id == NULL)
		return EINVAL;
	*clock_id = (*attr == NULL) ? CLOCK_REALTIME :
	    (clockid_t) ((uintptr_t) *attr - 1);
	return 0;
}

/*===========================================================================*
 *				pthread_rwlock_init			     *
 *===========================================================================*/
int pthread_rwlock_init(pthread_rwlock_t *rwlock,
	const pthread_rwlockattr_t *UNUSED(attr))
{
	return mthread_rwlock_init(rwlock);
}

#if !defined(__weak_alias)
#error __weak_alias is required to compile the pthread compat library
#endif

/*===========================================================================*
 *			pthread_mutexattr_* (type only)			     *
 *===========================================================================*/
int pthread_mutexattr_init(pthread_mutexattr_t *mattr)
{
	if (mattr == NULL)
		return EINVAL;
	*mattr = (pthread_mutexattr_t) (intptr_t) PTHREAD_MUTEX_DEFAULT;
	return 0;
}

int pthread_mutexattr_destroy(pthread_mutexattr_t *mattr)
{
	return mattr == NULL ? EINVAL : 0;
}

int pthread_mutexattr_settype(pthread_mutexattr_t *mattr, int type)
{
	if (mattr == NULL || (type != PTHREAD_MUTEX_NORMAL &&
	    type != PTHREAD_MUTEX_RECURSIVE && type != PTHREAD_MUTEX_ERRORCHECK))
		return EINVAL;
	*mattr = (pthread_mutexattr_t) (intptr_t) type;
	return 0;
}

int pthread_mutexattr_gettype(const pthread_mutexattr_t *mattr, int *type)
{
	if (mattr == NULL || type == NULL)
		return EINVAL;
	*type = (int) (intptr_t) *mattr;
	return 0;
}

/*===========================================================================*
 *			Thread IDs: pthread_t vs. mthread_thread_t	     *
 *===========================================================================*/
/* mthread numbers the main thread MAIN_THREAD (-1) and created threads from
 * 0 up.  Portable code - libstdc++'s std::thread above all - takes a
 * pthread_t of 0 to mean "no thread", so the first thread created would
 * look non-joinable.  The pthread API therefore hands out mthread IDs offset
 * by PTHREAD_ID_BIAS: the main thread is 1, created threads are 2 and up, and
 * 0 is never a valid pthread_t.
 */
#define PTHREAD_ID_BIAS	2
#define TO_PTHREAD(t)	((pthread_t) ((t) + PTHREAD_ID_BIAS))
#define TO_MTHREAD(t)	((mthread_thread_t) ((t) - PTHREAD_ID_BIAS))

int pthread_create(pthread_t *thread, const pthread_attr_t *tattr,
	void *(*proc)(void *), void *arg)
{
	mthread_thread_t t;
	mthread_attr_t a;
	int r;

	if (tattr != NULL)
		a = *tattr;
	if ((r = mthread_create(&t, tattr != NULL ? &a : NULL, proc, arg)) == 0 &&
	    thread != NULL)
		*thread = TO_PTHREAD(t);
	return r;
}

int pthread_detach(pthread_t thread)
{
	return mthread_detach(TO_MTHREAD(thread));
}

int pthread_equal(pthread_t l, pthread_t r)
{
	return l == r;
}

int pthread_join(pthread_t thread, void **value)
{
	return mthread_join(TO_MTHREAD(thread), value);
}

pthread_t pthread_self(void)
{
	return TO_PTHREAD(mthread_self());
}

/*===========================================================================*
 *				__libc_thr_yield			     *
 *===========================================================================*/
/* <sched.h> renames sched_yield() to __libc_thr_yield() (NetBSD).  libc has
 * a weak stub for programs without threads; this strong definition makes
 * sched_yield() - and so std::this_thread::yield() - switch threads.
 */
int __libc_thr_yield(void);

int __libc_thr_yield(void)
{
	return mthread_yield();
}
