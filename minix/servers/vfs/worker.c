#include "fs.h"
#include <assert.h>

static void worker_get_work(void);
static void *worker_main(void *arg);
static void worker_sleep(void);
static void worker_wake(struct worker_thread *worker);
static mthread_attr_t tattr;

#ifdef MKCOVERAGE
# define TH_STACKSIZE (40 * 1024)
#else
# define TH_STACKSIZE (28 * 1024)
#endif

#define ASSERTW(w) assert((w) >= &workers[0] && (w) < &workers[NR_WTHREADS])

/*===========================================================================*
 *				worker_init				     *
 *===========================================================================*/
void worker_init(void)
{
/* Initialize worker thread */
  struct worker_thread *wp;
  int i;

  if (mthread_attr_init(&tattr) != 0)
	panic("failed to initialize attribute");
  if (mthread_attr_setstacksize(&tattr, TH_STACKSIZE) != 0)
	panic("couldn't set default thread stack size");
  if (mthread_attr_setdetachstate(&tattr, MTHREAD_CREATE_DETACHED) != 0)
	panic("couldn't set default thread detach state");
  pending = 0;

  for (i = 0; i < NR_WTHREADS; i++) {
	wp = &workers[i];

	wp->w_fp = NULL;		/* Mark not in use */
	wp->w_next = NULL;
	wp->w_task = NONE;
	if (mutex_init(&wp->w_event_mutex, NULL) != 0)
		panic("failed to initialize mutex");
	if (cond_init(&wp->w_event, NULL) != 0)
		panic("failed to initialize conditional variable");
	if (mthread_create(&wp->w_tid, &tattr, worker_main, (void *) wp) != 0)
		panic("unable to start thread");
  }

  /* Let all threads get ready to accept work. */
  yield_all();
}

/*===========================================================================*
 *				worker_get_work				     *
 *===========================================================================*/
static void worker_get_work(void)
{
/* Find new work to do. Work can be 'queued', 'pending', or absent. In the
 * latter case wait for new work to come in.
 */
  struct fproc *rfp;

  /* Do we have queued work to do? */
  if (pending > 0) {
	/* Find pending work */
	for (rfp = &fproc[0]; rfp < &fproc[NR_PROCS]; rfp++) {
		if (rfp->fp_flags & FP_PENDING) {
			self->w_fp = rfp;
			rfp->fp_worker = self;
			rfp->fp_flags &= ~FP_PENDING; /* No longer pending */
			pending--;
			assert(pending >= 0);
			return;
		}
	}
	panic("Pending work inconsistency");
  }

  /* Wait for work to come to us */
  worker_sleep();
}

/*===========================================================================*
 *				worker_available			     *
 *===========================================================================*/
int worker_available(void)
{
  int busy, i;

  busy = 0;
  for (i = 0; i < NR_WTHREADS; i++) {
	if (workers[i].w_fp != NULL)
		busy++;
  }

  return(NR_WTHREADS - busy);
}

/*===========================================================================*
 *				worker_main				     *
 *===========================================================================*/
static void *worker_main(void *arg)
{
/* Worker thread main loop */

  self = (struct worker_thread *) arg;
  ASSERTW(self);

  while(TRUE) {
	worker_get_work();

	fp = self->w_fp;
	assert(fp->fp_worker == self);

	/* Lock the process. */
	lock_proc(fp);

	/* The following two blocks could be run in a loop until both the
	 * conditions are no longer met, but it is currently impossible that
	 * more normal work is present after postponed PM work has been done.
	 */

	/* Perform normal work, if any. */
	if (fp->fp_func != NULL) {
		self->w_m_in = fp->fp_msg;
		err_code = OK;

		fp->fp_func();

		fp->fp_func = NULL;	/* deliberately unset AFTER the call */
	}

	/* Perform postponed PM work, if any. */
	if (fp->fp_flags & FP_PM_WORK) {
		self->w_m_in = fp->fp_pm_msg;

		service_pm_postponed();

		fp->fp_flags &= ~FP_PM_WORK;
	}

	/* Perform cleanup actions. */
	thread_cleanup();

	unlock_proc(fp);

	fp->fp_worker = NULL;
	self->w_fp = NULL;
  }

  return(NULL);	/* Unreachable */
}

/*===========================================================================*
 *				worker_can_start			     *
 *===========================================================================*/
int worker_can_start(struct fproc *rfp)
{
/* Return whether normal (non-PM) work can be started for the given process.
 * This function is used to serialize invocation of "special" procedures, and
 * not entirely safe for other cases, as explained in the comments below.
 */
  int is_pending, is_active, has_normal_work, has_pm_work;

  is_pending = (rfp->fp_flags & FP_PENDING);
  is_active = (rfp->fp_worker != NULL);
  has_normal_work = (rfp->fp_func != NULL);
  has_pm_work = (rfp->fp_flags & FP_PM_WORK);

  /* If there is no work scheduled for the process, we can start work. */
  if (!is_pending && !is_active) return TRUE;

  /* If there is already normal work scheduled for the process, we cannot add
   * more, since we support only one normal job per process.
   */
  if (has_normal_work) return FALSE;

  /* If this process has pending PM work but no normal work, we can add the
   * normal work for execution before the worker will start.
   */
  if (is_pending) return TRUE;

  /* However, if a worker is active for PM work, we cannot add normal work
   * either, because the work will not be considered. For this reason, we can
   * not use this function for processes that can possibly get postponed PM
   * work. It is still safe for core system processes, though.
   */
  return FALSE;
}

/*===========================================================================*
 *				worker_try_activate			     *
 *===========================================================================*/
static void worker_try_activate(struct fproc *rfp, int use_spare)
{
/* See if we can wake up a thread to do the work scheduled for the given
 * process. If not, mark the process as having pending work for later.
 */
  int i, available, needed;
  struct worker_thread *worker;

  /* Use the last available thread only if requested. Otherwise, leave at least
   * one spare thread for deadlock resolution.
   */
  needed = use_spare ? 1 : 2;

  worker = NULL;
  for (i = available = 0; i < NR_WTHREADS; i++) {
	if (workers[i].w_fp == NULL) {
		if (worker == NULL)
			worker = &workers[i];
		if (++available >= needed)
			break;
	}
  }

  if (available >= needed) {
	assert(worker != NULL);
	rfp->fp_worker = worker;
	worker->w_fp = rfp;
	worker_wake(worker);
  } else {
	rfp->fp_flags |= FP_PENDING;
	pending++;
  }
}

/*===========================================================================*
 *				worker_start				     *
 *===========================================================================*/
void worker_start(struct fproc *rfp, void (*func)(void), message *m_ptr,
	int use_spare)
{
/* Schedule work to be done by a worker thread. The work is bound to the given
 * process. If a function pointer is given, the work is considered normal work,
 * and the function will be called to handle it. If the function pointer is
 * NULL, the work is considered postponed PM work, and service_pm_postponed
 * will be called to handle it. The input message will be a copy of the given
 * message. Optionally, the last spare (deadlock-resolving) thread may be used
 * to execute the work immediately.
 */
  int is_pm_work, is_pending, is_active, has_normal_work, has_pm_work;

  assert(rfp != NULL);

  is_pm_work = (func == NULL);
  is_pending = (rfp->fp_flags & FP_PENDING);
  is_active = (rfp->fp_worker != NULL);
  has_normal_work = (rfp->fp_func != NULL);
  has_pm_work = (rfp->fp_flags & FP_PM_WORK);

  /* Sanity checks. If any of these trigger, someone messed up badly! */
  if (is_pending || is_active) {
	if (is_pending && is_active)
		panic("work cannot be both pending and active");

	/* The process cannot make more than one call at once. */
	if (!is_pm_work && has_normal_work)
		panic("process has two calls (%x, %x)",
			rfp->fp_msg.m_type, m_ptr->m_type);

	/* PM will not send more than one job per process to us at once. */
	if (is_pm_work && has_pm_work)
		panic("got two calls from PM (%x, %x)",
			rfp->fp_pm_msg.m_type, m_ptr->m_type);

	/* Despite PM's sys_delay_stop() system, it is possible that normal
	 * work (in particular, do_pending_pipe) arrives after postponed PM
	 * work has been scheduled for execution, so we don't check for that.
	 */
#if 0
	printf("VFS: adding %s work to %s thread\n",
		is_pm_work ? "PM" : "normal",
		is_pending ? "pending" : "active");
#endif
  } else {
	/* Some cleanup step forgotten somewhere? */
	if (has_normal_work || has_pm_work)
		panic("worker administration error");
  }

  /* Save the work to be performed. */
  if (!is_pm_work) {
	rfp->fp_msg = *m_ptr;
	rfp->fp_func = func;
  } else {
	rfp->fp_pm_msg = *m_ptr;
	rfp->fp_flags |= FP_PM_WORK;
  }

  /* If we have not only added to existing work, go look for a free thread.
   * Note that we won't be using the spare thread for normal work if there is
   * already PM work pending, but that situation will never occur in practice.
   */
  if (!is_pending && !is_active)
	worker_try_activate(rfp, use_spare);
}

/*===========================================================================*
 *				worker_sleep				     *
 *===========================================================================*/
static void worker_sleep(void)
{
  struct worker_thread *worker = self;
  ASSERTW(worker);
  if (mutex_lock(&worker->w_event_mutex) != 0)
	panic("unable to lock event mutex");
  if (cond_wait(&worker->w_event, &worker->w_event_mutex) != 0)
	panic("could not wait on conditional variable");
  if (mutex_unlock(&worker->w_event_mutex) != 0)
	panic("unable to unlock event mutex");
  self = worker;
}

/*===========================================================================*
 *				worker_wake				     *
 *===========================================================================*/
static void worker_wake(struct worker_thread *worker)
{
/* Signal a worker to wake up */
  ASSERTW(worker);
  if (mutex_lock(&worker->w_event_mutex) != 0)
	panic("unable to lock event mutex");
  if (cond_signal(&worker->w_event) != 0)
	panic("unable to signal conditional variable");
  if (mutex_unlock(&worker->w_event_mutex) != 0)
	panic("unable to unlock event mutex");
}

/*===========================================================================*
 *				worker_suspend				     *
 *===========================================================================*/
struct worker_thread *worker_suspend(void)
{
/* Suspend the current thread, saving certain thread variables. Return a
 * pointer to the thread's worker structure for later resumption.
 */

  ASSERTW(self);
  assert(fp != NULL);
  assert(self->w_fp == fp);
  assert(fp->fp_worker == self);

  self->w_err_code = err_code;

  return self;
}

/*===========================================================================*
 *				worker_resume				     *
 *===========================================================================*/
void worker_resume(struct worker_thread *org_self)
{
/* Resume the current thread after suspension, restoring thread variables. */

  ASSERTW(org_self);

  self = org_self;

  fp = self->w_fp;
  assert(fp != NULL);

  err_code = self->w_err_code;
}

/*===========================================================================*
 *				worker_wait				     *
 *===========================================================================*/
void worker_wait(void)
{
/* Put the current thread to sleep until woken up by the main thread. */

  (void) worker_suspend(); /* worker_sleep already saves and restores 'self' */

  worker_sleep();

  /* We continue here after waking up */
  worker_resume(self);
  assert(self->w_next == NULL);
}

/*===========================================================================*
 *				worker_signal				     *
 *===========================================================================*/
void worker_signal(struct worker_thread *worker)
{
  ASSERTW(worker);		/* Make sure we have a valid thread */
  worker_wake(worker);
}

/*===========================================================================*
 *				worker_stop				     *
 *===========================================================================*/
void worker_stop(struct worker_thread *worker)
{
  ASSERTW(worker);		/* Make sure we have a valid thread */
  if (worker->w_task != NONE) {
	/* This thread is communicating with a driver or file server */
	if (worker->w_drv_sendrec != NULL) {			/* Driver */
		worker->w_drv_sendrec->m_type = EIO;
	} else if (worker->w_sendrec != NULL) {		/* FS */
		worker->w_sendrec->m_type = EIO;
	} else {
		panic("reply storage consistency error");	/* Oh dear */
	}
  } else {
	/* This shouldn't happen at all... */
	printf("VFS: stopping worker not blocked on any task?\n");
	util_stacktrace();
  }
  worker_wake(worker);
}

/*===========================================================================*
 *				worker_stop_by_endpt			     *
 *===========================================================================*/
void worker_stop_by_endpt(endpoint_t proc_e)
{
  struct worker_thread *worker;
  int i;

  if (proc_e == NONE) return;

  for (i = 0; i < NR_WTHREADS; i++) {
	worker = &workers[i];
	if (worker->w_fp != NULL && worker->w_task == proc_e)
		worker_stop(worker);
  }
}

/*===========================================================================*
 *				worker_get				     *
 *===========================================================================*/
struct worker_thread *worker_get(thread_t worker_tid)
{
  int i;

  for (i = 0; i < NR_WTHREADS; i++)
	if (workers[i].w_tid == worker_tid)
		return(&workers[i]);

  return(NULL);
}

/*===========================================================================*
 *				worker_set_proc				     *
 *===========================================================================*/
void worker_set_proc(struct fproc *rfp)
{
/* Perform an incredibly ugly action that completely violates the threading
 * model: change the current working thread's process context to another
 * process. The caller is expected to hold the lock to both the calling and the
 * target process, and neither process is expected to continue regular
 * operation when done. This code is here *only* and *strictly* for the reboot
 * code, and *must not* be used for anything else.
 */

  if (fp == rfp) return;

  if (rfp->fp_worker != NULL)
	panic("worker_set_proc: target process not idle");

  fp->fp_worker = NULL;

  fp = rfp;

  self->w_fp = rfp;
  fp->fp_worker = self;
}
