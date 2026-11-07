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
int pthread_mutex_init(pthread_mutex_t *mutex, pthread_mutexattr_t *mattr)
{
	return mthread_mutex_init(mutex, mattr);
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
int pthread_cond_init(pthread_cond_t *cond, pthread_condattr_t *cattr)
{
	return mthread_cond_init(cond, cattr);
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
 *				pthread_rwlock_init			     *
 *===========================================================================*/
int pthread_rwlock_init(pthread_rwlock_t *rwlock, pthread_rwlockattr_t *UNUSED(attr))
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
