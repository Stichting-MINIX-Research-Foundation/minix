/* This file contains the multithreaded device independent driver interface.
 *
 * Changes:
 *   Aug 27, 2011   created (A. Welzel)
 *
 * The entry points into this file are:
 *   driver_mt_task:		the main message loop of the driver
 *   driver_mt_terminate:	break out of the main message loop
 *   driver_mt_sleep:		put the current thread to sleep
 *   driver_mt_wakeup:		wake up a sleeping thread
 *   driver_mt_stop:		put up the current thread for termination
 */

#include <minix/driver_mt.h>
#include <minix/mthread.h>
#include <assert.h>

#include "driver.h"
#include "mq.h"
#include "event.h"

typedef enum {
  STATE_DEAD,
  STATE_RUNNING,
  STATE_STOPPING,
  STATE_EXITED
} worker_state;

/* Structure to handle running worker threads. */
typedef struct {
  thread_id_t id;
  worker_state state;
  mthread_thread_t mthread;
  event_t queue_event;
  event_t sleep_event;
} worker_t;

PRIVATE struct driver *driver_cb;
PRIVATE int driver_mt_type;
PRIVATE int driver_mt_running = FALSE;

PRIVATE mthread_key_t worker_key;

PRIVATE worker_t worker[DRIVER_MT_MAX_WORKERS];

PRIVATE worker_t *exited[DRIVER_MT_MAX_WORKERS];
PRIVATE int num_exited = 0;

/*===========================================================================*
 *				enqueue					     *
 *===========================================================================*/
PRIVATE void enqueue(worker_t *wp, const message *m_src, int ipc_status)
{
/* Enqueue a message into a worker thread's queue, and signal the thread.
 * Must be called from the master thread.
 */

  assert(wp->state == STATE_RUNNING || wp->state == STATE_STOPPING);

  if (!driver_mq_enqueue(wp->id, m_src, ipc_status))
	panic("driver_mt: enqueue failed (message queue full)");

  driver_event_fire(&wp->queue_event);
}

/*===========================================================================*
 *				try_dequeue				     *
 *===========================================================================*/
PRIVATE int try_dequeue(worker_t *wp, message *m_dst, int *ipc_status)
{
/* See if a message can be dequeued from the current worker thread's queue. If
 * so, dequeue the message and return TRUE. If not, return FALSE. Must be
 * called from a worker thread. Does not block.
 */

  return driver_mq_dequeue(wp->id, m_dst, ipc_status);
}

/*===========================================================================*
 *				dequeue					     *
 *===========================================================================*/
PRIVATE void dequeue(worker_t *wp, message *m_dst, int *ipc_status)
{
/* Dequeue a message from the current worker thread's queue. Block the current
 * thread if necessary. Must be called from a worker thread. Always successful.
 */

  while (!try_dequeue(wp, m_dst, ipc_status))
	driver_event_wait(&wp->queue_event);
}

/*===========================================================================*
 *				worker_thread				     *
 *===========================================================================*/
PRIVATE void *worker_thread(void *param)
{
/* The worker thread loop. Set up the thread-specific reference to itself and
 * start looping. The loop consists of blocking dequeing and handling messages.
 * After handling a message, the thread might have been stopped, so we check
 * for this condition and exit if so.
 */
  worker_t *wp;
  message m;
  int ipc_status, r;
 
  wp = (worker_t *) param;
  assert(wp != NULL);

  if (mthread_setspecific(worker_key, wp))
	panic("driver_mt: could not save local thread pointer");

  while (driver_mt_running) {
	/* See if a new message is available right away. */
	if (!try_dequeue(wp, &m, &ipc_status)) {
		/* If not, and this thread should be stopped, stop now. */
		if (wp->state == STATE_STOPPING)
			break;

		/* Otherwise, block waiting for a new message. */
		dequeue(wp, &m, &ipc_status);

		if (!driver_mt_running)
			break;
	}

	/* Even if the thread was stopped before, a new message resumes it. */
	wp->state = STATE_RUNNING;

	/* Handle the request, and send a reply. */
	r = driver_handle_request(driver_cb, &m);

	driver_reply(driver_mt_type, &m, ipc_status, r);
  }

  /* Clean up and terminate this thread. */
  if (mthread_setspecific(worker_key, NULL))
	panic("driver_mt: could not delete local thread pointer");

  wp->state = STATE_EXITED;

  exited[num_exited++] = wp;

  return NULL;
}

/*===========================================================================*
 *				master_create_worker			     *
 *===========================================================================*/
PRIVATE void master_create_worker(worker_t *wp, thread_id_t id)
{
/* Start a new worker thread.
 */
  int r;

  wp->id = id;
  wp->state = STATE_RUNNING;

  /* Initialize synchronization primitives. */
  driver_event_init(&wp->queue_event);
  driver_event_init(&wp->sleep_event);

  r = mthread_create(&wp->mthread, NULL /*attr*/, worker_thread, (void *) wp);

  if (r != 0)
	panic("driver_mt: could not start thread %d (%d)", id, r);
}

/*===========================================================================*
 *				master_destroy_worker			     *
 *===========================================================================*/
PRIVATE void master_destroy_worker(worker_t *wp)
{
/* Clean up resources used by an exited worker thread.
 */
  message m;
  int ipc_status;

  assert(wp != NULL);
  assert(wp->state == STATE_EXITED);
  assert(!driver_mq_dequeue(wp->id, &m, &ipc_status));

  /* Join the thread. */
  if (mthread_join(wp->mthread, NULL))
	panic("driver_mt: could not join thread %d", wp->id);

  /* Destroy resources. */
  driver_event_destroy(&wp->sleep_event);
  driver_event_destroy(&wp->queue_event);

  wp->state = STATE_DEAD;
}

/*===========================================================================*
 *				master_handle_exits			     *
 *===========================================================================*/
PRIVATE void master_handle_exits(void)
{
/* Destroy the remains of all exited threads.
 */
  int i;

  for (i = 0; i < num_exited; i++)
	master_destroy_worker(exited[i]);

  num_exited = 0;
}

/*===========================================================================*
 *				master_handle_request			     *
 *===========================================================================*/
PRIVATE void master_handle_request(message *m_ptr, int ipc_status)
{
/* For real request messages, query the thread ID, start a thread if one with
 * that ID is not already running, and enqueue the message in the thread's
 * message queue.
 */
  thread_id_t thread_id;
  worker_t *wp;
  int r;

  /* If this is not a request that has a minor device associated with it, we
   * can not tell which thread should process it either. In that case, the
   * master thread has to handle it instead.
   */
  if (!IS_DEV_MINOR_RQ(m_ptr->m_type)) {
	if (driver_cb->dr_other)
		r = (*driver_cb->dr_other)(driver_cb, m_ptr);
	else
		r = EINVAL;

	driver_reply(driver_mt_type, m_ptr, ipc_status, r);

	return;
  }

  /* Query the thread ID. Upon failure, send the error code to the caller. */
  r = driver_cb->dr_thread(m_ptr->DEVICE, &thread_id);

  if (r != OK) {
	driver_reply(driver_mt_type, m_ptr, ipc_status, r);

	return;
  }

  /* Start the thread if it is not already running. */
  assert(thread_id >= 0 && thread_id < DRIVER_MT_MAX_WORKERS);

  wp = &worker[thread_id];

  assert(wp->state != STATE_EXITED);

  if (wp->state == STATE_DEAD)
	master_create_worker(wp, thread_id);

  /* Enqueue the message for the thread, and possibly wake it up. */
  enqueue(wp, m_ptr, ipc_status);
}

/*===========================================================================*
 *				master_init				     *
 *===========================================================================*/
PRIVATE void master_init(struct driver *dp, int type)
{
/* Initialize the state of the master thread.
 */
  int i;

  assert(dp != NULL);
  assert(dp->dr_thread != NULL);

  mthread_init();

  driver_mt_type = type;
  driver_cb = dp;

  for (i = 0; i < DRIVER_MT_MAX_WORKERS; i++)
	worker[i].state = STATE_DEAD;

  /* Initialize a per-thread key, where each worker thread stores its own
   * reference to the worker structure.
   */
  if (mthread_key_create(&worker_key, NULL))
	panic("driver_mt: error initializing worker key");
}

/*===========================================================================*
 *				driver_mt_receive			     *
 *===========================================================================*/
PRIVATE void driver_mt_receive(message *m_ptr, int *ipc_status)
{
/* Receive a message.
 */
  int r;

  r = sef_receive_status(ANY, m_ptr, ipc_status);

  if (r != OK)
	panic("driver_mt: sef_receive_status() returned %d", r);
}

/*===========================================================================*
 *				driver_mt_task				     *
 *===========================================================================*/
PUBLIC void driver_mt_task(struct driver *driver_p, int driver_type)
{
/* The multithreaded driver task.
 */
  int ipc_status;
  message mess;

  /* Initialize first if necessary. */
  if (!driver_mt_running) {
	master_init(driver_p, driver_type);

	driver_mt_running = TRUE;
  }

  /* The main message loop. */
  while (driver_mt_running) {
	/* Receive a message. */
	driver_mt_receive(&mess, &ipc_status);

	/* Dispatch the message. */
	if (is_ipc_notify(ipc_status))
		driver_handle_notify(driver_cb, &mess);
	else
		master_handle_request(&mess, ipc_status);

	/* Let other threads run. */
	mthread_yield_all();

	/* Clean up any exited threads. */
	if (num_exited > 0)
		master_handle_exits();
  }
}

/*===========================================================================*
 *				driver_mt_terminate			     *
 *===========================================================================*/
PUBLIC void driver_mt_terminate(void)
{
/* Instruct libdriver to shut down.
 */

  driver_mt_running = FALSE;
}

/*===========================================================================*
 *				driver_mt_sleep				     *
 *===========================================================================*/
PUBLIC void driver_mt_sleep(void)
{
/* Let the current thread sleep until it gets woken up by the master thread.
 */
  worker_t *wp;

  wp = (worker_t *) mthread_getspecific(worker_key);

  if (wp == NULL)
	panic("driver_mt: master thread cannot sleep");

  driver_event_wait(&wp->sleep_event);
}

/*===========================================================================*
 *				driver_mt_wakeup			     *
 *===========================================================================*/
PUBLIC void driver_mt_wakeup(thread_id_t id)
{
/* Wake up a sleeping worker thread from the master thread.
 */
  worker_t *wp;

  assert(id >= 0 && id < DRIVER_MT_MAX_WORKERS);

  wp = &worker[id];

  assert(wp->state == STATE_RUNNING || wp->state == STATE_STOPPING);

  driver_event_fire(&wp->sleep_event);
}

/*===========================================================================*
 *				driver_mt_stop				     *
 *===========================================================================*/
PUBLIC void driver_mt_stop(void)
{
/* Put up the current worker thread for termination. Once the current dispatch
 * call has finished, and there are no more messages in the thread's message
 * queue, the thread will be terminated. Any messages in the queue will undo
 * the effect of this call.
 */
  worker_t *wp;
	
  wp = (worker_t *) mthread_getspecific(worker_key);

  assert(wp != NULL);
  assert(wp->state == STATE_RUNNING || wp->state == STATE_STOPPING);

  wp->state = STATE_STOPPING;
}
