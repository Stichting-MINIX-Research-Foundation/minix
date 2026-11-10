/* mthread_cond_timedwait() and pthread_cond_timedwait(): kept apart from
 * condition.c because they need clock_gettime() and nanosleep(), which the
 * system services linking libmthread (VFS) do not have.
 */
#define _MTHREADIFY_PTHREADS
#include <minix/mthread.h>
#include <time.h>
#include "global.h"
#include "proto.h"

/* As in condition.c (non-MTHREAD_STRICT) and pthread_compat.c. */
#define mthread_cond_valid(c)	((*c)->mc_magic == MTHREAD_INIT_MAGIC)
#define MUTEX_LAZY(m)	(*(m) == PTHREAD_MUTEX_INITIALIZER || *(m) == NULL)
#define COND_LAZY(c)	(*(c) == PTHREAD_COND_INITIALIZER || *(c) == NULL)

/*===========================================================================*
 *				mthread_cond_timedwait			     *
 *===========================================================================*/
int mthread_cond_timedwait(mthread_cond_t *cond, mthread_mutex_t *mutex,
	const struct timespec *abstime)
{
/* Wait for a condition to be signaled, or until 'abstime' on the condition's
 * clock has passed (ETIMEDOUT).  Threads are cooperative, so the waiter does
 * not block: it stays runnable and lets the other threads run until one of
 * them signals it (mthread_cond_wake() sets m_signaled) or the time is up.
 * When no other thread is runnable, none can signal it before the deadline,
 * so it sleeps (the whole process) until then.
 */
  mthread_tcb_t *tcb;
  struct __mthread_cond *c;
  struct __mthread_mutex *m;
  struct timespec now, left;
  unsigned int count;
  int r;

  if (cond == NULL || mutex == NULL || abstime == NULL ||
      abstime->tv_nsec < 0 || abstime->tv_nsec >= 1000000000L)
	return(EINVAL);

  c = (struct __mthread_cond *) *cond;
  m = (struct __mthread_mutex *) *mutex;

  if (!mthread_cond_valid(cond) || !mthread_mutex_valid(mutex))
	return(EINVAL);

  c->mc_mutex = m;
  count = m->mm_count;	/* release a recursive mutex completely */
  m->mm_count = 0;
  if (mthread_mutex_unlock(mutex) != 0) {
	m->mm_count = count;
	return(EPERM);
  }

  tcb = mthread_find_tcb(current_thread);
  tcb->m_cond = c;
  tcb->m_signaled = 0;
  tcb->m_timedwait = 1;

  r = 0;
  while (!tcb->m_signaled) {
	if (clock_gettime(c->mc_clock, &now) != 0) {
		r = EINVAL;
		break;
	}
	if (now.tv_sec > abstime->tv_sec || (now.tv_sec == abstime->tv_sec &&
	    now.tv_nsec >= abstime->tv_nsec)) {
		r = ETIMEDOUT;
		break;
	}
	if (mthread_yield() != 0) {
		/* Nothing else can run: sleep until the deadline. */
		left.tv_sec = abstime->tv_sec - now.tv_sec;
		left.tv_nsec = abstime->tv_nsec - now.tv_nsec;
		if (left.tv_nsec < 0) {
			left.tv_sec--;
			left.tv_nsec += 1000000000L;
		}
		(void) nanosleep(&left, NULL);
	}
  }

  tcb->m_timedwait = 0;
  tcb->m_signaled = 0;
  c->mc_mutex = NULL;
  tcb->m_cond = NULL;
  if (mthread_mutex_lock(mutex) != 0)
	return(EINVAL);
  m->mm_count = count;

  return(r);
}

/*===========================================================================*
 *				pthread_cond_timedwait			     *
 *===========================================================================*/
int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
	const struct timespec *abstime)
{
	if (COND_LAZY(cond)) {
		mthread_cond_init(cond, NULL);
	}

	if (MUTEX_LAZY(mutex)) {
		mthread_mutex_init(mutex, NULL);
	}

	return mthread_cond_timedwait(cond, mutex, abstime);
}

