#include "fs.h"
#include "glo.h"
#include "fproc.h"
#include "threads.h"
#include "job.h"
#include <assert.h>

static void append_job(struct job *job, void *(*func)(void *arg));
static void get_work(struct worker_thread *worker);
static void *worker_main(void *arg);
static void worker_sleep(struct worker_thread *worker);
static void worker_wake(struct worker_thread *worker);
static int worker_waiting_for(struct worker_thread *worker, endpoint_t
	proc_e);
static int init = 0;
static mthread_attr_t tattr;

#ifdef MKCOVERAGE
# define TH_STACKSIZE (10 * 1024)
#else
# define TH_STACKSIZE (7 * 1024)
#endif

#define ASSERTW(w) assert((w) == &sys_worker || (w) == &dl_worker || \
		   ((w) >= &workers[0] && (w) < &workers[NR_WTHREADS]));

/*===========================================================================*
 *				worker_init				     *
 *===========================================================================*/
void worker_init(struct worker_thread *wp)
{
/* Initialize worker thread */
  if (!init) {
	threads_init();
	if (mthread_attr_init(&tattr) != 0)
		panic("failed to initialize attribute");
	if (mthread_attr_setstacksize(&tattr, TH_STACKSIZE) != 0)
		panic("couldn't set default thread stack size");
	if (mthread_attr_setdetachstate(&tattr, MTHREAD_CREATE_DETACHED) != 0)
		panic("couldn't set default thread detach state");
	invalid_thread_id = mthread_self(); /* Assuming we're the main thread*/
	pending = 0;
	init = 1;
  }

  ASSERTW(wp);

  wp->w_job.j_func = NULL;		/* Mark not in use */
  wp->w_next = NULL;
  if (mutex_init(&wp->w_event_mutex, NULL) != 0)
	panic("failed to initialize mutex");
  if (cond_init(&wp->w_event, NULL) != 0)
	panic("failed to initialize conditional variable");
  if (mthread_create(&wp->w_tid, &tattr, worker_main, (void *) wp) != 0)
	panic("unable to start thread");
  yield();
}

/*===========================================================================*
 *				get_work				     *
 *===========================================================================*/
static void get_work(struct worker_thread *worker)
{
/* Find new work to do. Work can be 'queued', 'pending', or absent. In the
 * latter case wait for new work to come in. */

  struct job *new_job;
  struct fproc *rfp;

  ASSERTW(worker);
  self = worker;

  /* Do we have queued work to do? */
  if ((new_job = worker->w_job.j_next) != NULL) {
	worker->w_job = *new_job;
	free(new_job);
	return;
  } else if (worker != &sys_worker && worker != &dl_worker && pending > 0) {
	/* Find pending work */
	for (rfp = &fproc[0]; rfp < &fproc[NR_PROCS]; rfp++) {
		if (rfp->fp_flags & FP_PENDING) {
			worker->w_job = rfp->fp_job;
			rfp->fp_job.j_func = NULL;
			rfp->fp_flags &= ~FP_PENDING; /* No longer pending */
			pending--;
			assert(pending >= 0);
			return;
		}
	}
	panic("Pending work inconsistency");
  }

  /* Wait for work to come to us */
  worker_sleep(worker);
}

/*===========================================================================*
 *				worker_available				     *
 *===========================================================================*/
int worker_available(void)
{
  int busy, i;

  busy = 0;
  for (i = 0; i < NR_WTHREADS; i++) {
	if (workers[i].w_job.j_func != NULL)
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
  struct worker_thread *me;

  me = (struct worker_thread *) arg;
  ASSERTW(me);

  while(TRUE) {
	get_work(me);

	/* Register ourselves in fproc table if possible */
	if (me->w_job.j_fp != NULL) {
		me->w_job.j_fp->fp_wtid = me->w_tid;
	}

	/* Carry out work */
	me->w_job.j_func(&me->w_job);

	/* Deregister if possible */
	if (me->w_job.j_fp != NULL) {
		me->w_job.j_fp->fp_wtid = invalid_thread_id;
	}

	/* Mark ourselves as done */
	me->w_job.j_func = NULL;
	me->w_job.j_fp = NULL;
  }

  return(NULL);	/* Unreachable */
}

/*===========================================================================*
 *				dl_worker_start				     *
 *===========================================================================*/
void dl_worker_start(void *(*func)(void *arg))
{
/* Start the deadlock resolving worker. This worker is reserved to run in case
 * all other workers are busy and we have to have an additional worker to come
 * to the rescue. */
  assert(dl_worker.w_job.j_func == NULL);

  if (dl_worker.w_job.j_func == NULL) {
	dl_worker.w_job.j_fp = fp;
	dl_worker.w_job.j_m_in = m_in;
	dl_worker.w_job.j_func = func;
	worker_wake(&dl_worker);
  }
}

/*===========================================================================*
 *				sys_worker_start			     *
 *===========================================================================*/
void sys_worker_start(void *(*func)(void *arg))
{
/* Carry out work for the system (i.e., kernel or PM). If this thread is idle
 * do it right away, else create new job and append it to the queue. */

  if (sys_worker.w_job.j_func == NULL) {
	sys_worker.w_job.j_fp = fp;
	sys_worker.w_job.j_m_in = m_in;
	sys_worker.w_job.j_func = func;
	worker_wake(&sys_worker);
  } else {
	append_job(&sys_worker.w_job, func);
  }
}

/*===========================================================================*
 *				append_job				     *
 *===========================================================================*/
static void append_job(struct job *job, void *(*func)(void *arg))
{
/* Append a job */

  struct job *new_job, *tail;

  /* Create new job */
  new_job = calloc(1, sizeof(struct job));
  assert(new_job != NULL);
  new_job->j_fp = fp;
  new_job->j_m_in = m_in;
  new_job->j_func = func;
  new_job->j_next = NULL;
  new_job->j_err_code = OK;

  /* Append to queue */
  tail = job;
  while (tail->j_next != NULL) tail = tail->j_next;
  tail->j_next = new_job;
}

/*===========================================================================*
 *				worker_start				     *
 *===========================================================================*/
void worker_start(void *(*func)(void *arg))
{
/* Find an available worker or wait for one */
  int i;
  struct worker_thread *worker;

  if (fp->fp_flags & FP_DROP_WORK) {
	return;	/* This process is not allowed to accept new work */
  }

  worker = NULL;
  for (i = 0; i < NR_WTHREADS; i++) {
	if (workers[i].w_job.j_func == NULL) {
		worker = &workers[i];
		break;
	}
  }

  if (worker != NULL) {
	worker->w_job.j_fp = fp;
	worker->w_job.j_m_in = m_in;
	worker->w_job.j_func = func;
	worker->w_job.j_next = NULL;
	worker->w_job.j_err_code = OK;
	worker_wake(worker);
	return;
  }

  /* No worker threads available, let's wait for one to finish. */
  /* If this process already has a job scheduled, forget about this new
   * job;
   *  - the new job is do_dummy and we have already scheduled an actual job
   *  - the new job is an actual job and we have already scheduled do_dummy in
   *    order to exit this proc, so doing the new job is pointless. */
  if (fp->fp_job.j_func == NULL) {
	assert(!(fp->fp_flags & FP_PENDING));
	fp->fp_job.j_fp = fp;
	fp->fp_job.j_m_in = m_in;
	fp->fp_job.j_func = func;
	fp->fp_job.j_next = NULL;
	fp->fp_job.j_err_code = OK;
	fp->fp_flags |= FP_PENDING;
	pending++;
  }
}

/*===========================================================================*
 *				worker_sleep				     *
 *===========================================================================*/
static void worker_sleep(struct worker_thread *worker)
{
  ASSERTW(worker);
  assert(self == worker);
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
 *				worker_wait				     *
 *===========================================================================*/
void worker_wait(void)
{
  self->w_job.j_err_code = err_code;
  worker_sleep(self);
  /* We continue here after waking up */
  fp = self->w_job.j_fp;	/* Restore global data */
  err_code = self->w_job.j_err_code;
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
	} else if (worker->w_fs_sendrec != NULL) {		/* FS */
		worker->w_fs_sendrec->m_type = EIO;
	} else {
		panic("reply storage consistency error");	/* Oh dear */
	}
  } else {
	worker->w_job.j_m_in.m_type = EIO;
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

  if (worker_waiting_for(&sys_worker, proc_e)) worker_stop(&sys_worker);
  if (worker_waiting_for(&dl_worker, proc_e)) worker_stop(&dl_worker);

  for (i = 0; i < NR_WTHREADS; i++) {
	worker = &workers[i];
	if (worker_waiting_for(worker, proc_e))
		worker_stop(worker);
  }
}

/*===========================================================================*
 *				worker_get				     *
 *===========================================================================*/
struct worker_thread *worker_get(thread_t worker_tid)
{
  int i;
  struct worker_thread *worker;

  worker = NULL;
  if (worker_tid == sys_worker.w_tid)
	worker = &sys_worker;
  else if (worker_tid == dl_worker.w_tid)
	worker = &dl_worker;
  else {
	for (i = 0; i < NR_WTHREADS; i++) {
		if (workers[i].w_tid == worker_tid) {
			worker = &workers[i];
			break;
		}
	}
  }

  return(worker);
}

/*===========================================================================*
 *				worker_getjob				     *
 *===========================================================================*/
struct job *worker_getjob(thread_t worker_tid)
{
  struct worker_thread *worker;

  if ((worker = worker_get(worker_tid)) != NULL)
	return(&worker->w_job);

  return(NULL);
}

/*===========================================================================*
 *				worker_waiting_for			     *
 *===========================================================================*/
static int worker_waiting_for(struct worker_thread *worker, endpoint_t proc_e)
{
  ASSERTW(worker);		/* Make sure we have a valid thread */

  if (worker->w_job.j_func != NULL) {
	if (worker->w_task != NONE)
		return(worker->w_task == proc_e);
	else if (worker->w_job.j_fp != NULL) {
		return(worker->w_job.j_fp->fp_task == proc_e);
	}
  }

  return(0);
}
