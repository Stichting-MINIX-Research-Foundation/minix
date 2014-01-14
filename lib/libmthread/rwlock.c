#include <minix/mthread.h>
#include "global.h"

/*===========================================================================*
 *				mthread_rwlock_init			     *
 *===========================================================================*/
int mthread_rwlock_init(rwlock)
mthread_rwlock_t *rwlock; /* The rwlock to be initialized */
{
  /* Initialize a readers/writer lock. */
  int r;

  if (!rwlock)
	return EINVAL;

  rwlock->writer = NO_THREAD;
  rwlock->readers = 0;

  r = mthread_mutex_init(&rwlock->queue, NULL);
  if (r != 0)
	return r;

  r = mthread_event_init(&rwlock->drain);
  if (r != 0)
	mthread_mutex_destroy(&rwlock->queue);

  return r;
}

/*===========================================================================*
 *				mthread_rwlock_destroy			     *
 *===========================================================================*/
int mthread_rwlock_destroy(rwlock)
mthread_rwlock_t *rwlock; /* The rwlock to be destroyed */
{
  /* Destroy a readers/writer lock. */
  int r;

  if (!rwlock)
	return EINVAL;

  assert(rwlock->writer == NO_THREAD);
  assert(rwlock->readers == 0);

  r = mthread_event_destroy(&rwlock->drain);
  if (r != 0)
	return r;

  return mthread_mutex_destroy(&rwlock->queue);
}

/*===========================================================================*
 *				mthread_rwlock_rdlock			     *
 *===========================================================================*/
int mthread_rwlock_rdlock(rwlock)
mthread_rwlock_t *rwlock; /* The rwlock to be read locked */
{
  /* Acquire a reader lock. */
  int r;

  if (!rwlock)
	return EINVAL;

  r = mthread_mutex_lock(&rwlock->queue);
  if (r != 0)
	return r;

  r = mthread_mutex_unlock(&rwlock->queue);
  if (r != 0)
	return r;

  rwlock->readers++;

  return 0;
}

/*===========================================================================*
 *				mthread_rwlock_wrlock			     *
 *===========================================================================*/
int mthread_rwlock_wrlock(rwlock)
mthread_rwlock_t *rwlock; /* The rwlock to be write locked */
{
  /* Acquire a writer lock. */
  int r;

  if (!rwlock)
	  return EINVAL;

  r = mthread_mutex_lock(&rwlock->queue);
  if (r != 0)
	return r;

  rwlock->writer = current_thread;

  if (rwlock->readers > 0)
	r = mthread_event_wait(&rwlock->drain);

  if (r == 0)
	assert(rwlock->readers == 0);

  return r;
}

/*===========================================================================*
 *				mthread_rwlock_unlock				*
 *===========================================================================*/
int mthread_rwlock_unlock(rwlock)
mthread_rwlock_t *rwlock; /* The rwlock to be unlocked */
{
  /* Release a lock. */
  int r;

  r = 0;
  if (!rwlock)
	  return EINVAL;

  if (rwlock->writer == current_thread) {
	rwlock->writer = NO_THREAD;
	r = mthread_mutex_unlock(&rwlock->queue);
  } else {
	assert(rwlock->readers > 0);

	rwlock->readers--;

	if (rwlock->readers == 0 && rwlock->writer != NO_THREAD)
		r = mthread_event_fire(&rwlock->drain);
  }

  return r;
}

/* pthread compatibility layer. */
__weak_alias(pthread_rwlock_destroy, mthread_rwlock_destroy)
__weak_alias(pthread_rwlock_rdlock, mthread_rwlock_rdlock)
__weak_alias(pthread_rwlock_wrlock, mthread_rwlock_wrlock)
__weak_alias(pthread_rwlock_unlock, mthread_rwlock_unlock)

