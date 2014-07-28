#include <minix/mthread.h>
#include "global.h"

/*===========================================================================*
 *				mthread_event_init			     *
 *===========================================================================*/
int mthread_event_init(event)
mthread_event_t *event; /* The event to be initialized */
{
/* Initialize an event object.
 */
  int r;

  if (!event)
	return EINVAL;

  r = mthread_mutex_init(&event->mutex, NULL);
  if (r != 0)
	return r;

  r = mthread_cond_init(&event->cond, NULL);
  if (r != 0) 
	mthread_mutex_destroy(&event->mutex);

  return r;
}


/*===========================================================================*
 *				mthread_event_destroy			     *
 *===========================================================================*/
int mthread_event_destroy(event)
mthread_event_t *event; /* The event to be destroyed */
{
/* Destroy an event object.
 */
  int r;

  if (!event)
	return EINVAL;

  r = mthread_cond_destroy(&event->cond);
  if (r != 0)
	return r;

  return mthread_mutex_destroy(&event->mutex);
}

/*===========================================================================*
 *				mthread_event_wait			     *
 *===========================================================================*/
int mthread_event_wait(event)
mthread_event_t *event; /* The event to be waited on */
{
/* Wait for an event, blocking the current thread in the process.
 */
  int r;

  if (!event)
	return EINVAL;

  r = mthread_mutex_lock(&event->mutex);
  if (r != 0)
	return r;

  r = mthread_cond_wait(&event->cond, &event->mutex);
  if (r != 0) {
	mthread_mutex_unlock(&event->mutex);
	return r;
  }

  return mthread_mutex_unlock(&event->mutex);
}

/*===========================================================================*
 *				mthread_event_fire			     *
 *===========================================================================*/
int mthread_event_fire(event)
mthread_event_t *event; /* The event to be fired */
{
/* Fire an event, waking up any thread blocked on it.
*/
  int r;

  if (!event)
	return EINVAL;

  r = mthread_mutex_lock(&event->mutex);
  if (r != 0)
	return r;

  r = mthread_cond_signal(&event->cond);
  if (r != 0) {
	mthread_mutex_unlock(&event->mutex);
	return r;
  }

  return mthread_mutex_unlock(&event->mutex);
}


/*===========================================================================*
 *				mthread_event_fire_all			     *
 *===========================================================================*/
int mthread_event_fire_all(event)
mthread_event_t *event; /* The event to be fired */
{
/* Fire an event, waking up any thread blocked on it.
*/
  int r;

  if (!event)
	return EINVAL;

  r = mthread_mutex_lock(&event->mutex);
  if (r != 0)
	return r;

  r = mthread_cond_broadcast(&event->cond);
  if (r != 0) {
	mthread_mutex_unlock(&event->mutex);
	return r;
  }

  return mthread_mutex_unlock(&event->mutex);
}

/* pthread compatibility layer. */
__weak_alias(pthread_event_destroy, mthread_event_destroy)
__weak_alias(pthread_event_init, mthread_event_init)
__weak_alias(pthread_event_wait, mthread_event_wait)
__weak_alias(pthread_event_fire, mthread_event_fire)
__weak_alias(pthread_event_fire_all, mthread_event_fire_all)

