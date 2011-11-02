/* This file contains a simple thread event implementation.
 */

#include <minix/mthread.h>
#include <minix/sysutil.h>

#include "event.h"

/*===========================================================================*
 *				driver_event_init			     *
 *===========================================================================*/
PUBLIC void driver_event_init(event_t *event)
{
/* Initialize an event object.
 */
  int r;

  if ((r = mthread_mutex_init(&event->mutex, NULL)) != 0)
	panic("libdriver: error initializing mutex (%d)", r);
  if ((r = mthread_cond_init(&event->cond, NULL)) != 0)
	panic("libdriver: error initializing condvar (%d)", r);
}

/*===========================================================================*
 *				driver_event_destroy			     *
 *===========================================================================*/
PUBLIC void driver_event_destroy(event_t *event)
{
/* Destroy an event object.
 */
  int r;

  if ((r = mthread_cond_destroy(&event->cond)) != 0)
	panic("libdriver: error destroying condvar (%d)", r);
  if ((r = mthread_mutex_destroy(&event->mutex)) != 0)
	panic("libdriver: error destroying mutex (%d)", r);
}

/*===========================================================================*
 *				driver_event_wait			     *
 *===========================================================================*/
PUBLIC void driver_event_wait(event_t *event)
{
/* Wait for an event, blocking the current thread in the process.
 */
  int r;

  if ((r = mthread_mutex_lock(&event->mutex)) != 0)
	panic("libdriver: error locking mutex (%d)", r);
  if ((r = mthread_cond_wait(&event->cond, &event->mutex)) != 0)
	panic("libdriver: error waiting for condvar (%d)", r);
  if ((r = mthread_mutex_unlock(&event->mutex)) != 0)
	panic("libdriver: error unlocking mutex (%d)", r);
}

/*===========================================================================*
 *				driver_event_fire			     *
 *===========================================================================*/
PUBLIC void driver_event_fire(event_t *event)
{
/* Fire an event, waking up any thread blocked on it without scheduling it.
 */
  int r;

  if ((r = mthread_mutex_lock(&event->mutex)) != 0)
	panic("libdriver: error locking mutex (%d)", r);
  if ((r = mthread_cond_signal(&event->cond)) != 0)
	panic("libdriver: error signaling condvar (%d)", r);
  if ((r = mthread_mutex_unlock(&event->mutex)) != 0)
	panic("libdriver: error unlocking mutex (%d)", r);
}
