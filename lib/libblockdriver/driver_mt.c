/* This file contains the multithreaded driver interface.
 *
 * Changes:
 *   Aug 27, 2011   created (A. Welzel)
 *
 * The entry points into this file are:
 *   blockdriver_mt_task:	the main message loop of the driver
 *   blockdriver_mt_terminate:	break out of the main message loop
 *   blockdriver_mt_sleep:	put the current thread to sleep
 *   blockdriver_mt_wakeup:	wake up a sleeping thread
 *   blockdriver_mt_set_workers:set the number of worker threads
 */

#include <minix/blockdriver_mt.h>
#include <minix/mthread.h>
#include <assert.h>

#include "const.h"
#include "driver.h"
#include "mq.h"

/* A thread ID is composed of a device ID and a per-device worker thread ID.
 * All thread IDs must be in the range 0..(MAX_THREADS-1) inclusive.
 */
#define MAKE_TID(did, wid)	((did) * MAX_WORKERS + (wid))
#define TID_DEVICE(tid)		((tid) / MAX_WORKERS)
#define TID_WORKER(tid)		((tid) % MAX_WORKERS)

typedef int worker_id_t;

typedef enum {
  STATE_DEAD,
  STATE_RUNNING,
  STATE_BUSY,
  STATE_EXITED
} worker_state;

/* Structure with information about a worker thread. */
typedef struct {
  device_id_t device_id;
  worker_id_t worker_id;
  worker_state state;
  mthread_thread_t mthread;
  mthread_event_t sleep_event;
} worker_t;

/* Structure with information about a device. */
typedef struct {
  device_id_t id;
  unsigned int workers;
  worker_t worker[MAX_WORKERS];
  mthread_event_t queue_event;
  mthread_rwlock_t barrier;
} device_t;

static struct blockdriver *bdtab;
static int running = FALSE;

static mthread_key_t worker_key;

static device_t device[MAX_DEVICES];

static worker_t *exited[MAX_THREADS];
static int num_exited = 0;

/*===========================================================================*
 *				enqueue					     *
 *===========================================================================*/
static void enqueue(device_t *dp, const message *m_src, int ipc_status)
{
/* Enqueue a message into the device's queue, and signal the event.
 * Must be called from the master thread.
 */

  if (!mq_enqueue(dp->id, m_src, ipc_status))
	panic("blockdriver_mt: enqueue failed (message queue full)");

  mthread_event_fire(&dp->queue_event);
}

/*===========================================================================*
 *				try_dequeue				     *
 *===========================================================================*/
static int try_dequeue(device_t *dp, message *m_dst, int *ipc_status)
{
/* See if a message can be dequeued from the current worker thread's device
 * queue. If so, dequeue the message and return TRUE. If not, return FALSE.
 * Must be called from a worker thread. Does not block.
 */

  return mq_dequeue(dp->id, m_dst, ipc_status);
}

/*===========================================================================*
 *				dequeue					     *
 *===========================================================================*/
static int dequeue(device_t *dp, worker_t *wp, message *m_dst,
  int *ipc_status)
{
/* Dequeue a message from the current worker thread's device queue. Block the
 * current thread if necessary. Must be called from a worker thread. Either
 * succeeds with a message (TRUE) or indicates that the thread should be
 * terminated (FALSE).
 */

  do {
	mthread_event_wait(&dp->queue_event);

	/* If we were woken up as a result of terminate or set_workers, break
	 * out of the loop and terminate the thread.
	 */
	if (!running || wp->worker_id >= dp->workers)
		return FALSE;
  } while (!try_dequeue(dp, m_dst, ipc_status));

  return TRUE;
}

/*===========================================================================*
 *				is_transfer_req				     *
 *===========================================================================*/
static int is_transfer_req(int type)
{
/* Return whether the given block device request is a transfer request.
 */

  switch (type) {
  case BDEV_READ:
  case BDEV_WRITE:
  case BDEV_GATHER:
  case BDEV_SCATTER:
	return TRUE;

  default:
	return FALSE;
  }
}

/*===========================================================================*
 *				worker_thread				     *
 *===========================================================================*/
static void *worker_thread(void *param)
{
/* The worker thread loop. Set up the thread-specific reference to itself and
 * start looping. The loop consists of blocking dequeing and handling messages.
 * After handling a message, the thread might have been stopped, so we check
 * for this condition and exit if so.
 */
  worker_t *wp;
  device_t *dp;
  thread_id_t tid;
  message m;
  int ipc_status, r;

  wp = (worker_t *) param;
  assert(wp != NULL);
  dp = &device[wp->device_id];
  tid = MAKE_TID(wp->device_id, wp->worker_id);

  if (mthread_setspecific(worker_key, wp))
	panic("blockdriver_mt: could not save local thread pointer");

  while (running && wp->worker_id < dp->workers) {

	/* See if a new message is available right away. */
	if (!try_dequeue(dp, &m, &ipc_status)) {

		/* If not, block waiting for a new message or a thread
		 * termination event.
		 */
		if (!dequeue(dp, wp, &m, &ipc_status))
			break;
	}

	/* Even if the thread was stopped before, a new message resumes it. */
	wp->state = STATE_BUSY;

	/* If the request is a transfer request, we acquire the read barrier
	 * lock. Otherwise, we acquire the write lock.
	 */
	if (is_transfer_req(m.m_type))
		mthread_rwlock_rdlock(&dp->barrier);
	else
		mthread_rwlock_wrlock(&dp->barrier);

	/* Handle the request and send a reply. */
	blockdriver_process_on_thread(bdtab, &m, ipc_status, tid);

	/* Switch the thread back to running state, and unlock the barrier. */
	wp->state = STATE_RUNNING;
	mthread_rwlock_unlock(&dp->barrier);
  }

  /* Clean up and terminate this thread. */
  if (mthread_setspecific(worker_key, NULL))
	panic("blockdriver_mt: could not delete local thread pointer");

  wp->state = STATE_EXITED;

  exited[num_exited++] = wp;

  return NULL;
}

/*===========================================================================*
 *				master_create_worker			     *
 *===========================================================================*/
static void master_create_worker(worker_t *wp, worker_id_t worker_id,
  device_id_t device_id)
{
/* Start a new worker thread.
 */
  mthread_attr_t attr;
  int r;

  wp->device_id = device_id;
  wp->worker_id = worker_id;
  wp->state = STATE_RUNNING;

  /* Initialize synchronization primitives. */
  mthread_event_init(&wp->sleep_event);

  r = mthread_attr_init(&attr);
  if (r != 0)
	panic("blockdriver_mt: could not initialize attributes (%d)", r);

  r = mthread_attr_setstacksize(&attr, STACK_SIZE);
  if (r != 0)
	panic("blockdriver_mt: could not set stack size (%d)", r);

  r = mthread_create(&wp->mthread, &attr, worker_thread, (void *) wp);
  if (r != 0)
	panic("blockdriver_mt: could not start thread %d (%d)", worker_id, r);

  mthread_attr_destroy(&attr);
}

/*===========================================================================*
 *				master_destroy_worker			     *
 *===========================================================================*/
static void master_destroy_worker(worker_t *wp)
{
/* Clean up resources used by an exited worker thread.
 */

  assert(wp != NULL);
  assert(wp->state == STATE_EXITED);

  /* Join the thread. */
  if (mthread_join(wp->mthread, NULL))
	panic("blockdriver_mt: could not join thread %d", wp->worker_id);

  /* Destroy resources. */
  mthread_event_destroy(&wp->sleep_event);

  wp->state = STATE_DEAD;
}

/*===========================================================================*
 *				master_handle_exits			     *
 *===========================================================================*/
static void master_handle_exits(void)
{
/* Destroy the remains of all exited threads.
 */
  int i;

  for (i = 0; i < num_exited; i++)
	master_destroy_worker(exited[i]);

  num_exited = 0;
}

/*===========================================================================*
 *				master_handle_message			     *
 *===========================================================================*/
static void master_handle_message(message *m_ptr, int ipc_status)
{
/* For real request messages, query the device ID, start a thread if none is
 * free and the maximum number of threads for that device has not yet been
 * reached, and enqueue the message in the devices's message queue. All other
 * messages are handled immediately from the main thread.
 */
  device_id_t id;
  worker_t *wp;
  device_t *dp;
  int r, wid;

  /* If this is not a block driver request, we cannot get the minor device
   * associated with it, and thus we can not tell which thread should process
   * it either. In that case, the master thread has to handle it instead.
   */
  if (is_ipc_notify(ipc_status) || !IS_BDEV_RQ(m_ptr->m_type)) {
	/* Process as 'other' message. */
	blockdriver_process_on_thread(bdtab, m_ptr, ipc_status, MAIN_THREAD);

	return;
  }

  /* Query the device ID. Upon failure, send the error code to the caller. */
  r = (*bdtab->bdr_device)(m_ptr->m_lbdev_lblockdriver_msg.minor, &id);
  if (r != OK) {
	blockdriver_reply(m_ptr, ipc_status, r);

	return;
  }

  /* Look up the device control block. */
  assert(id >= 0 && id < MAX_DEVICES);
  dp = &device[id];

  /* Find the first non-busy worker thread. */
  for (wid = 0; wid < dp->workers; wid++)
	if (dp->worker[wid].state != STATE_BUSY)
		break;

  /* If the worker thread is dead, start a thread now, unless we have already
   * reached the maximum number of threads.
   */
  if (wid < dp->workers) {
	wp = &dp->worker[wid];

	assert(wp->state != STATE_EXITED);

	/* If the non-busy thread has not yet been created, create one now. */
	if (wp->state == STATE_DEAD)
		master_create_worker(wp, wid, dp->id);
  }

  /* Enqueue the message at the device queue. */
  enqueue(dp, m_ptr, ipc_status);
}

/*===========================================================================*
 *				master_init				     *
 *===========================================================================*/
static void master_init(struct blockdriver *bdp)
{
/* Initialize the state of the master thread.
 */
  int i, j;

  assert(bdp != NULL);
  assert(bdp->bdr_device != NULL);

  bdtab = bdp;

  /* Initialize device-specific data structures. */
  for (i = 0; i < MAX_DEVICES; i++) {
	device[i].id = i;
	device[i].workers = 1;
	mthread_event_init(&device[i].queue_event);
	mthread_rwlock_init(&device[i].barrier);

	for (j = 0; j < MAX_WORKERS; j++)
		device[i].worker[j].state = STATE_DEAD;
  }

  /* Initialize a per-thread key, where each worker thread stores its own
   * reference to the worker structure.
   */
  if (mthread_key_create(&worker_key, NULL))
	panic("blockdriver_mt: error initializing worker key");
}

/*===========================================================================*
 *				blockdriver_mt_get_tid			     *
 *===========================================================================*/
thread_id_t blockdriver_mt_get_tid(void)
{
/* Return back the ID of this thread.
 */
  worker_t *wp;

  wp = (worker_t *) mthread_getspecific(worker_key);

  if (wp == NULL)
	panic("blockdriver_mt: master thread cannot query thread ID\n");

  return MAKE_TID(wp->device_id, wp->worker_id);
}

/*===========================================================================*
 *				blockdriver_mt_receive			     *
 *===========================================================================*/
static void blockdriver_mt_receive(message *m_ptr, int *ipc_status)
{
/* Receive a message.
 */
  int r;

  r = sef_receive_status(ANY, m_ptr, ipc_status);

  if (r != OK)
	panic("blockdriver_mt: sef_receive_status() returned %d", r);
}

/*===========================================================================*
 *				blockdriver_mt_task			     *
 *===========================================================================*/
void blockdriver_mt_task(struct blockdriver *driver_tab)
{
/* The multithreaded driver task.
 */
  int ipc_status, i;
  message mess;

  /* Initialize first if necessary. */
  if (!running) {
	master_init(driver_tab);

	running = TRUE;
  }

  /* The main message loop. */
  while (running) {
	/* Receive a message. */
	blockdriver_mt_receive(&mess, &ipc_status);

	/* Dispatch the message. */
	master_handle_message(&mess, ipc_status);

	/* Let other threads run. */
	mthread_yield_all();

	/* Clean up any exited threads. */
	if (num_exited > 0)
		master_handle_exits();
  }

  /* Free up resources. */
  for (i = 0; i < MAX_DEVICES; i++)
	mthread_event_destroy(&device[i].queue_event);
}

/*===========================================================================*
 *				blockdriver_mt_terminate		     *
 *===========================================================================*/
void blockdriver_mt_terminate(void)
{
/* Instruct libblockdriver to shut down.
 */

  running = FALSE;
}

/*===========================================================================*
 *				blockdriver_mt_sleep			     *
 *===========================================================================*/
void blockdriver_mt_sleep(void)
{
/* Let the current thread sleep until it gets woken up by the master thread.
 */
  worker_t *wp;

  wp = (worker_t *) mthread_getspecific(worker_key);

  if (wp == NULL)
	panic("blockdriver_mt: master thread cannot sleep");

  mthread_event_wait(&wp->sleep_event);
}

/*===========================================================================*
 *				blockdriver_mt_wakeup			     *
 *===========================================================================*/
void blockdriver_mt_wakeup(thread_id_t id)
{
/* Wake up a sleeping worker thread from the master thread.
 */
  worker_t *wp;
  device_id_t device_id;
  worker_id_t worker_id;

  device_id = TID_DEVICE(id);
  worker_id = TID_WORKER(id);

  assert(device_id >= 0 && device_id < MAX_DEVICES);
  assert(worker_id >= 0 && worker_id < MAX_WORKERS);

  wp = &device[device_id].worker[worker_id];

  assert(wp->state == STATE_RUNNING || wp->state == STATE_BUSY);

  mthread_event_fire(&wp->sleep_event);
}

/*===========================================================================*
 *				blockdriver_mt_set_workers		     *
 *===========================================================================*/
void blockdriver_mt_set_workers(device_id_t id, int workers)
{
/* Set the number of worker threads for the given device.
 */
  device_t *dp;

  assert(id >= 0 && id < MAX_DEVICES);

  if (workers > MAX_WORKERS)
	workers = MAX_WORKERS;

  dp = &device[id];

  /* If we are cleaning up, wake up all threads waiting on a queue event. */
  if (workers == 1 && dp->workers > workers)
	mthread_event_fire_all(&dp->queue_event);

  dp->workers = workers;
}
