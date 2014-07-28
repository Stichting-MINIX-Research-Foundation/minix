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
 */


/*===========================================================================*
 *				pthread_mutex_init			     *
 *===========================================================================*/
int pthread_mutex_init(pthread_mutex_t *mutex, pthread_mutexattr_t *mattr)
{
	return mthread_mutex_init(mutex, mattr);
}

/*===========================================================================*
 *				pthread_mutex_destroy			     *
 *===========================================================================*/
int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	if (PTHREAD_MUTEX_INITIALIZER == *mutex) {
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
	if (PTHREAD_MUTEX_INITIALIZER == *mutex) {
		mthread_mutex_init(mutex, NULL);	
	}

	return mthread_mutex_lock(mutex);
}

/*===========================================================================*
 *				pthread_mutex_trylock			     *
 *===========================================================================*/
int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	if (PTHREAD_MUTEX_INITIALIZER == *mutex) {
		mthread_mutex_init(mutex, NULL);	
	}

	return pthread_mutex_trylock(mutex);
}

/*===========================================================================*
 *				pthread_mutex_unlock			     *
 *===========================================================================*/
int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	if (PTHREAD_MUTEX_INITIALIZER == *mutex) {
		mthread_mutex_init(mutex, NULL);	
	}

	return mthread_mutex_unlock(mutex);
}

/*===========================================================================*
 *				pthread_cond_init			     *
 *===========================================================================*/
int pthread_cond_init(pthread_cond_t *cond, pthread_condattr_t *cattr)
{
	return mthread_cond_init(cond, cattr);
}

/*===========================================================================*
 *				pthread_cond_broadcast			     *
 *===========================================================================*/
int pthread_cond_broadcast(pthread_cond_t *cond)
{
	if (PTHREAD_COND_INITIALIZER == *cond) {
		mthread_cond_init(cond, NULL);
	}

	return mthread_cond_broadcast(cond);
}

/*===========================================================================*
 *				pthread_cond_destroy			     *
 *===========================================================================*/
int pthread_cond_destroy(pthread_cond_t *cond)
{
	if (PTHREAD_COND_INITIALIZER == *cond) {
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
	if (PTHREAD_COND_INITIALIZER == *cond) {
		mthread_cond_init(cond, NULL);
	}

	return mthread_cond_signal(cond);
}

/*===========================================================================*
 *				pthread_cond_wait			     *
 *===========================================================================*/
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
	if (PTHREAD_COND_INITIALIZER == *cond) {
		mthread_cond_init(cond, NULL);
	}

	return mthread_cond_wait(cond, mutex);
}

/*===========================================================================*
 *				pthread_rwlock_init			     *
 *===========================================================================*/
int pthread_rwlock_init(pthread_rwlock_t *rwlock, pthread_rwlockattr_t *UNUSED(attr))
{
	return mthread_rwlock_init(rwlock);
}

#if !defined(__weak_alias)
#error __weak_alias is required to compile the pthread compat library
#endif

