/*
 * a loop that gets messages requesting work, carries out the work, and sends
 * replies.
 *
 * The entry points into this file are:
 *   main:	main program of the Virtual File System
 *   reply:	send a reply to a process after the requested work is done
 *
 */

#include "fs.h"
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/ioc_memory.h>
#include <sys/svrctl.h>
#include <sys/select.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/keymap.h>
#include <minix/const.h>
#include <minix/endpoint.h>
#include <minix/safecopies.h>
#include <minix/debug.h>
#include <minix/vfsif.h>
#include "file.h"
#include "dmap.h"
#include "fproc.h"
#include "scratchpad.h"
#include "vmnt.h"
#include "vnode.h"
#include "job.h"
#include "param.h"

#if ENABLE_SYSCALL_STATS
EXTERN unsigned long calls_stats[NCALLS];
#endif

/* Thread related prototypes */
FORWARD _PROTOTYPE( void thread_cleanup_f, (struct fproc *rfp, char *f,
					    int l)			);
#define thread_cleanup(x) thread_cleanup_f(x, __FILE__, __LINE__)
FORWARD _PROTOTYPE( void *do_async_dev_result, (void *arg)		);
FORWARD _PROTOTYPE( void *do_control_msgs, (void *arg)			);
FORWARD _PROTOTYPE( void *do_fs_reply, (struct job *job)			);
FORWARD _PROTOTYPE( void *do_work, (void *arg)				);
FORWARD _PROTOTYPE( void *do_pm, (void *arg)				);
FORWARD _PROTOTYPE( void *do_init_root, (void *arg)			);
FORWARD _PROTOTYPE( void handle_work, (void *(*func)(void *arg))		);

FORWARD _PROTOTYPE( void get_work, (void)				);
FORWARD _PROTOTYPE( void lock_pm, (void)				);
FORWARD _PROTOTYPE( void unlock_pm, (void)				);
FORWARD _PROTOTYPE( void service_pm, (void)				);
FORWARD _PROTOTYPE( void service_pm_postponed, (void)				);
FORWARD _PROTOTYPE( int unblock, (struct fproc *rfp)			);

/* SEF functions and variables. */
FORWARD _PROTOTYPE( void sef_local_startup, (void) );
FORWARD _PROTOTYPE( int sef_cb_init_fresh, (int type, sef_init_info_t *info) );
PRIVATE mutex_t pm_lock;
PRIVATE endpoint_t receive_from;

/*===========================================================================*
 *				main					     *
 *===========================================================================*/
PUBLIC int main(void)
{
/* This is the main program of the file system.  The main loop consists of
 * three major activities: getting new work, processing the work, and sending
 * the reply.  This loop never terminates as long as the file system runs.
 */
  int transid, req;
  struct job *job;

  /* SEF local startup. */
  sef_local_startup();

  printf("Started VFS: %d worker thread(s)\n", NR_WTHREADS);

  /* This is the main loop that gets work, processes it, and sends replies. */
  while (TRUE) {
	yield_all();	/* let other threads run */
	send_work();
	get_work();

	transid = TRNS_GET_ID(m_in.m_type);
	req = TRNS_DEL_ID(m_in.m_type);
	job = worker_getjob( (thread_t) transid - VFS_TRANSID);

	/* Transaction encoding changes original m_type value; restore. */
	if (job == NULL)
		m_in.m_type = transid;
	else
		m_in.m_type = req;

	if (job != NULL) {
		do_fs_reply(job);
		continue;
	} else if (who_e == PM_PROC_NR) { /* Calls from PM */
		/* Special control messages from PM */
		sys_worker_start(do_pm);
		continue;
	} else if (is_notify(call_nr)) {
		/* A task notify()ed us */
		sys_worker_start(do_control_msgs);
		continue;
	} else if (who_p < 0) { /* i.e., message comes from a task */
		/* We're going to ignore this message. Tasks should
		 * send notify()s only.
		 */
		 printf("VFS: ignoring message from %d (%d)\n", who_e, call_nr);
		 continue;
	}

	/* At this point we either have results from an asynchronous device
	 * or a new system call. In both cases a new worker thread has to be
	 * started and there might not be one available from the pool. This is
	 * not a problem (requests/replies are simply queued), except when
	 * they're from an FS endpoint, because these can cause a deadlock.
	 * handle_work() takes care of the details. */
	if (IS_DEV_RS(call_nr)) {
		/* We've got results for a device request */
		handle_work(do_async_dev_result);
		continue;
	} else {
		/* Normal syscall. */
		handle_work(do_work);
	}
  }
  return(OK);				/* shouldn't come here */
}

/*===========================================================================*
 *			       handle_work				     *
 *===========================================================================*/
PRIVATE void handle_work(void *(*func)(void *arg))
{
/* Handle asynchronous device replies and new system calls. If the originating
 * endpoint is an FS endpoint, take extra care not to get in deadlock. */
  struct vmnt *vmp = NULL;

  if ((vmp = find_vmnt(who_e)) != NULL) {
	/* A call back or dev result from an FS endpoint */

	/* Set call back flag. We assume that an FS does only one call back
	 * at a time */
	vmp->m_flags |= VMNT_CALLBACK;

	/* When an FS point has to make a call back in order to mount, force
	 * its device to a "none device" so block reads/writes will be handled
	 * by ROOT_FS_E.
	 */
	if (vmp->m_flags & VMNT_MOUNTING)
		vmp->m_flags |= VMNT_FORCEROOTBSF;

	if (worker_available() == 0) {
		/* No worker threads available to handle call */
		if (deadlock_resolving) {
			/* Already trying to resolve a deadlock, can't
			 * handle more, sorry */
			vmp->m_flags &= ~VMNT_CALLBACK;
			reply(who_e, EAGAIN);
			return;
		}
		deadlock_resolving = 1;
		dl_worker_start(func);
		return;
	}
  }

  worker_start(func);
}

/*===========================================================================*
 *			       do_async_dev_result				     *
 *===========================================================================*/
PRIVATE void *do_async_dev_result(void *arg)
{
  endpoint_t endpt;
  struct job my_job;

  my_job = *((struct job *) arg);
  fp = my_job.j_fp;
  m_in = my_job.j_m_in;

  /* An asynchronous character driver has results for us */
  if (call_nr == DEV_REVIVE) {
	endpt = m_in.REP_ENDPT;
	if (endpt == VFS_PROC_NR)
		endpt = find_suspended_ep(m_in.m_source, m_in.REP_IO_GRANT);

	if (endpt == NONE) {
		printf("VFS: proc with grant %d from %d not found\n",
			m_in.REP_IO_GRANT, m_in.m_source);
	} else if (m_in.REP_STATUS == SUSPEND) {
		printf("VFS: got SUSPEND on DEV_REVIVE: not reviving proc\n");
	} else
		revive(endpt, m_in.REP_STATUS);
  }
  else if (call_nr == DEV_OPEN_REPL) open_reply();
  else if (call_nr == DEV_REOPEN_REPL) reopen_reply();
  else if (call_nr == DEV_CLOSE_REPL) close_reply();
  else if (call_nr == DEV_SEL_REPL1)
	select_reply1(m_in.m_source, m_in.DEV_MINOR, m_in.DEV_SEL_OPS);
  else if (call_nr == DEV_SEL_REPL2)
	select_reply2(m_in.m_source, m_in.DEV_MINOR, m_in.DEV_SEL_OPS);

  if (deadlock_resolving) {
	if (fp != NULL && fp->fp_wtid == dl_worker.w_tid)
		deadlock_resolving = 0;
  }

  if (fp != NULL && (fp->fp_flags & FP_SYS_PROC)) {
	struct vmnt *vmp;

	if ((vmp = find_vmnt(fp->fp_endpoint)) != NULL)
		vmp->m_flags &= ~VMNT_CALLBACK;
  }

  thread_cleanup(NULL);
  return(NULL);
}

/*===========================================================================*
 *			       do_control_msgs				     *
 *===========================================================================*/
PRIVATE void *do_control_msgs(void *arg)
{
  struct job my_job;

  my_job = *((struct job *) arg);
  fp = my_job.j_fp;
  m_in = my_job.j_m_in;

  /* Check for special control messages. */
  if (who_e == CLOCK) {
	/* Alarm timer expired. Used only for select(). Check it. */
	expire_timers(m_in.NOTIFY_TIMESTAMP);
  } else if (who_e == DS_PROC_NR) {
	/* DS notifies us of an event. */
	ds_event();
  } else {
	/* Device notifies us of an event. */
	dev_status(&m_in);
  }

  thread_cleanup(NULL);
  return(NULL);
}

/*===========================================================================*
 *			       do_fs_reply				     *
 *===========================================================================*/
PRIVATE void *do_fs_reply(struct job *job)
{
  struct vmnt *vmp;
  struct fproc *rfp;

  if ((vmp = find_vmnt(who_e)) == NULL)
	panic("Couldn't find vmnt for endpoint %d", who_e);

  rfp = job->j_fp;

  if (rfp == NULL || rfp->fp_endpoint == NONE) {
	printf("VFS: spurious reply from %d\n", who_e);
	return(NULL);
  }

  if (rfp->fp_task != who_e)
	printf("VFS: expected %d to reply, not %d\n", rfp->fp_task, who_e);
  *rfp->fp_sendrec = m_in;
  rfp->fp_task = NONE;
  vmp->m_comm.c_cur_reqs--;	/* We've got our reply, make room for others */
  worker_signal(worker_get(rfp->fp_wtid));/* Continue this worker thread */
  return(NULL);
}

/*===========================================================================*
 *				lock_pm					     *
 *===========================================================================*/
PRIVATE void lock_pm(void)
{
  message org_m_in;
  struct fproc *org_fp;
  struct worker_thread *org_self;

  /* First try to get it right off the bat */
  if (mutex_trylock(&pm_lock) == 0)
	return;

  org_m_in = m_in;
  org_fp = fp;
  org_self = self;

  if (mutex_lock(&pm_lock) != 0)
	panic("Could not obtain lock on pm\n");

  m_in = org_m_in;
  fp = org_fp;
  self = org_self;
}

/*===========================================================================*
 *				unlock_pm				     *
 *===========================================================================*/
PRIVATE void unlock_pm(void)
{
  if (mutex_unlock(&pm_lock) != 0)
	panic("Could not release lock on pm");
}

/*===========================================================================*
 *			       do_pm					     *
 *===========================================================================*/
PRIVATE void *do_pm(void *arg)
{
  struct job my_job;
  struct fproc *rfp;

  my_job = *((struct job *) arg);
  rfp = fp = my_job.j_fp;
  m_in = my_job.j_m_in;

  lock_pm();
  service_pm();
  unlock_pm();

  thread_cleanup(NULL);
  return(NULL);
}

/*===========================================================================*
 *			       do_pending_pipe					     *
 *===========================================================================*/
PRIVATE void *do_pending_pipe(void *arg)
{
  int r, op;
  struct job my_job;
  struct filp *f;
  tll_access_t locktype;

  my_job = *((struct job *) arg);
  fp = my_job.j_fp;
  m_in = my_job.j_m_in;

  lock_proc(fp, 1 /* force lock */);

  f = scratch(fp).file.filp;
  assert(f != NULL);
  scratch(fp).file.filp = NULL;

  locktype = (call_nr == READ) ? VNODE_READ : VNODE_WRITE;
  op = (call_nr == READ) ? READING : WRITING;
  lock_filp(f, locktype);

  r = rw_pipe(op, who_e, f, scratch(fp).io.io_buffer, scratch(fp).io.io_nbytes);

  if (r != SUSPEND)  /* Do we have results to report? */
	reply(who_e, r);

  unlock_filp(f);

  thread_cleanup(fp);
  return(NULL);
}

/*===========================================================================*
 *			       do_dummy					     *
 *===========================================================================*/
PUBLIC void *do_dummy(void *arg)
{
  struct job my_job;
  int r;

  my_job = *((struct job *) arg);
  fp = my_job.j_fp;
  m_in = my_job.j_m_in;

  if ((r = mutex_trylock(&fp->fp_lock)) == 0) {
	thread_cleanup(fp);
  } else {
	/* Proc is busy, let that worker thread carry out the work */
	thread_cleanup(NULL);
  }
  return(NULL);
}

/*===========================================================================*
 *			       do_work					     *
 *===========================================================================*/
PRIVATE void *do_work(void *arg)
{
  int error;
  struct job my_job;

  my_job = *((struct job *) arg);
  fp = my_job.j_fp;
  m_in = my_job.j_m_in;

  lock_proc(fp, 0); /* This proc is busy */

  if (call_nr == MAPDRIVER) {
	error = do_mapdriver();
  } else if (call_nr == COMMON_GETSYSINFO) {
	error = do_getsysinfo();
  } else if (IS_PFS_VFS_RQ(call_nr)) {
	if (who_e != PFS_PROC_NR) {
		printf("VFS: only PFS is allowed to make nested VFS calls\n");
		error = ENOSYS;
	} else if (call_nr <= PFS_BASE || call_nr >= PFS_BASE + PFS_NREQS) {
		error = ENOSYS;
	} else {
		call_nr -= PFS_BASE;
		error = (*pfs_call_vec[call_nr])();
	}
  } else {
	/* We're dealing with a POSIX system call from a normal
	 * process. Call the internal function that does the work.
	 */
	if (call_nr < 0 || call_nr >= NCALLS) {
		error = ENOSYS;
	} else if (fp->fp_pid == PID_FREE) {
		/* Process vanished before we were able to handle request.
		 * Replying has no use. Just drop it. */
		error = SUSPEND;
	} else {
#if ENABLE_SYSCALL_STATS
		calls_stats[call_nr]++;
#endif
		error = (*call_vec[call_nr])();
	}
  }

  /* Copy the results back to the user and send reply. */
  if (error != SUSPEND) {

	if ((fp->fp_flags & FP_SYS_PROC)) {
		struct vmnt *vmp;

		if ((vmp = find_vmnt(fp->fp_endpoint)) != NULL)
			vmp->m_flags &= ~VMNT_CALLBACK;
	}

	if (deadlock_resolving) {
		if (fp->fp_wtid == dl_worker.w_tid)
			deadlock_resolving = 0;
	}
	reply(who_e, error);
  }

  thread_cleanup(fp);
  return(NULL);
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
PRIVATE void sef_local_startup()
{
  /* Register init callbacks. */
  sef_setcb_init_fresh(sef_cb_init_fresh);
  sef_setcb_init_restart(sef_cb_init_fail);

  /* No live update support for now. */

  /* Let SEF perform startup. */
  sef_startup();
}

/*===========================================================================*
 *				sef_cb_init_fresh			     *
 *===========================================================================*/
PRIVATE int sef_cb_init_fresh(int UNUSED(type), sef_init_info_t *info)
{
/* Initialize the virtual file server. */
  int s, i;
  struct fproc *rfp;
  message mess;
  struct rprocpub rprocpub[NR_BOOT_PROCS];

  force_sync = 0;
  receive_from = ANY;

  /* Initialize proc endpoints to NONE */
  for (rfp = &fproc[0]; rfp < &fproc[NR_PROCS]; rfp++) {
	rfp->fp_endpoint = NONE;
	rfp->fp_pid = PID_FREE;
  }

  /* Initialize the process table with help of the process manager messages.
   * Expect one message for each system process with its slot number and pid.
   * When no more processes follow, the magic process number NONE is sent.
   * Then, stop and synchronize with the PM.
   */
  do {
	if ((s = sef_receive(PM_PROC_NR, &mess)) != OK)
		panic("VFS: couldn't receive from PM: %d", s);

	if (mess.m_type != PM_INIT)
		panic("unexpected message from PM: %d", mess.m_type);

	if (NONE == mess.PM_PROC) break;

	rfp = &fproc[mess.PM_SLOT];
	rfp->fp_flags = FP_NOFLAGS;
	rfp->fp_pid = mess.PM_PID;
	rfp->fp_endpoint = mess.PM_PROC;
	rfp->fp_grant = GRANT_INVALID;
	rfp->fp_blocked_on = FP_BLOCKED_ON_NONE;
	rfp->fp_realuid = (uid_t) SYS_UID;
	rfp->fp_effuid = (uid_t) SYS_UID;
	rfp->fp_realgid = (gid_t) SYS_GID;
	rfp->fp_effgid = (gid_t) SYS_GID;
	rfp->fp_umask = ~0;
  } while (TRUE);			/* continue until process NONE */
  mess.m_type = OK;			/* tell PM that we succeeded */
  s = send(PM_PROC_NR, &mess);		/* send synchronization message */

  /* All process table entries have been set. Continue with initialization. */
  fp = &fproc[_ENDPOINT_P(VFS_PROC_NR)];/* During init all communication with
					 * FSes is on behalf of myself */
  init_dmap();			/* Initialize device table. */
  system_hz = sys_hz();

  /* Map all the services in the boot image. */
  if ((s = sys_safecopyfrom(RS_PROC_NR, info->rproctab_gid, 0,
			    (vir_bytes) rprocpub, sizeof(rprocpub), S)) != OK){
	panic("sys_safecopyfrom failed: %d", s);
  }
  for (i = 0; i < NR_BOOT_PROCS; i++) {
	if (rprocpub[i].in_use) {
		if ((s = map_service(&rprocpub[i])) != OK) {
			panic("VFS: unable to map service: %d", s);
		}
	}
  }

  /* Subscribe to block and character driver events. */
  s = ds_subscribe("drv\\.[bc]..\\..*", DSF_INITIAL | DSF_OVERWRITE);
  if (s != OK) panic("VFS: can't subscribe to driver events (%d)", s);

#if DO_SANITYCHECKS
  FIXME("VFS: DO_SANITYCHECKS is on");
#endif

  /* Initialize worker threads */
  for (i = 0; i < NR_WTHREADS; i++)  {
	worker_init(&workers[i]);
  }
  worker_init(&sys_worker); /* exclusive system worker thread */
  worker_init(&dl_worker); /* exclusive worker thread to resolve deadlocks */

  /* Initialize global locks */
  if (mthread_mutex_init(&pm_lock, NULL) != 0)
	panic("VFS: couldn't initialize pm lock mutex");
  if (mthread_mutex_init(&exec_lock, NULL) != 0)
	panic("VFS: couldn't initialize exec lock");
  if (mthread_mutex_init(&bsf_lock, NULL) != 0)
	panic("VFS: couldn't initialize block special file lock");

  /* Initialize event resources for boot procs and locks for all procs */
  for (rfp = &fproc[0]; rfp < &fproc[NR_PROCS]; rfp++) {
	if (mutex_init(&rfp->fp_lock, NULL) != 0)
		panic("unable to initialize fproc lock");
#if LOCK_DEBUG
	rfp->fp_vp_rdlocks = 0;
	rfp->fp_vmnt_rdlocks = 0;
#endif
  }

  init_vnodes();		/* init vnodes */
  init_vmnts();			/* init vmnt structures */
  init_select();		/* init select() structures */
  init_filps();			/* Init filp structures */
  mount_pfs();			/* mount Pipe File Server */
  worker_start(do_init_root);	/* mount initial ramdisk as file system root */
  yield();			/* force do_init_root to start */

  return(OK);
}

/*===========================================================================*
 *			       do_init_root				     *
 *===========================================================================*/
PRIVATE void *do_init_root(void *arg)
{
  struct fproc *rfp;
  struct job my_job;
  int r;
  char *mount_label = "fs_imgrd"; /* FIXME: obtain this from RS */

  my_job = *((struct job *) arg);
  fp = my_job.j_fp;

  lock_proc(fp, 1 /* force lock */); /* This proc is busy */
  lock_pm();

  /* Initialize process directories. mount_fs will set them to the correct
   * values */
  for (rfp = &fproc[0]; rfp < &fproc[NR_PROCS]; rfp++) {
	FD_ZERO(&(rfp->fp_filp_inuse));
	rfp->fp_rd = NULL;
	rfp->fp_wd = NULL;
  }

  receive_from = MFS_PROC_NR;
  if ((r = mount_fs(DEV_IMGRD, "/", MFS_PROC_NR, 0, mount_label)) != OK)
	panic("Failed to initialize root");
  receive_from = ANY;

  unlock_pm();
  thread_cleanup(fp);
  return(NULL);
}

/*===========================================================================*
 *				lock_proc				     *
 *===========================================================================*/
PUBLIC void lock_proc(struct fproc *rfp, int force_lock)
{
  int r;
  message org_m_in;
  struct fproc *org_fp;
  struct worker_thread *org_self;

  r = mutex_trylock(&rfp->fp_lock);

  /* Were we supposed to obtain this lock immediately? */
  if (force_lock) {
	assert(r == 0);
	return;
  }

  if (r == 0) return;

  org_m_in = m_in;
  org_fp = fp;
  org_self = self;
  if ((r = mutex_lock(&rfp->fp_lock)) != 0)
	panic("unable to lock fproc lock: %d", r);
  m_in = org_m_in;
  fp = org_fp;
  self = org_self;
}

/*===========================================================================*
 *				unlock_proc				     *
 *===========================================================================*/
PUBLIC void unlock_proc(struct fproc *rfp)
{
  int r;

  if ((r = mutex_unlock(&rfp->fp_lock)) != 0)
	panic("Failed to unlock: %d", r);
}

/*===========================================================================*
 *				thread_cleanup				     *
 *===========================================================================*/
PRIVATE void thread_cleanup_f(struct fproc *rfp, char *f, int l)
{
/* Clean up worker thread. Skip parts if this thread is not associated
 * with a particular process (i.e., rfp is NULL) */

  assert(mthread_self() != -1);

#if LOCK_DEBUG
  if (rfp != NULL) {
	check_filp_locks_by_me();
	check_vnode_locks_by_me(rfp);
	check_vmnt_locks_by_me(rfp);
  }
#endif

  if (rfp != NULL && rfp->fp_flags & FP_PM_PENDING) {	/* Postponed PM call */
	m_in = rfp->fp_job.j_m_in;
	rfp->fp_flags &= ~FP_PM_PENDING;
	service_pm_postponed();
  }

#if LOCK_DEBUG
  if (rfp != NULL) {
	check_filp_locks_by_me();
	check_vnode_locks_by_me(rfp);
	check_vmnt_locks_by_me(rfp);
  }
#endif

  if (rfp != NULL) {
	rfp->fp_flags &= ~FP_DROP_WORK;
	unlock_proc(rfp);
  }

#if 0
  mthread_exit(NULL);
#endif
}

/*===========================================================================*
 *				get_work				     *
 *===========================================================================*/
PRIVATE void get_work()
{
  /* Normally wait for new input.  However, if 'reviving' is
   * nonzero, a suspended process must be awakened.
   */
  int r, found_one, proc_p;
  register struct fproc *rp;

  while (reviving != 0) {
	found_one = FALSE;

	/* Find a suspended process. */
	for (rp = &fproc[0]; rp < &fproc[NR_PROCS]; rp++)
		if (rp->fp_pid != PID_FREE && (rp->fp_flags & FP_REVIVED)) {
			found_one = TRUE; /* Found a suspended process */
			if (unblock(rp))
				return;	/* So main loop can process job */
			send_work();
		}

	if (!found_one)	/* Consistency error */
		panic("VFS: get_work couldn't revive anyone");
  }

  for(;;) {
	/* Normal case.  No one to revive. Get a useful request. */
	if ((r = sef_receive(receive_from, &m_in)) != OK) {
		panic("VFS: sef_receive error: %d", r);
	}

	proc_p = _ENDPOINT_P(m_in.m_source);
	if (proc_p < 0) fp = NULL;
	else fp = &fproc[proc_p];

	if (m_in.m_type == EDEADSRCDST) return;	/* Failed 'sendrec' */

	/* Negative who_p is never used to access the fproc array. Negative
	 * numbers (kernel tasks) are treated in a special way.
	 */
	if (who_p >= (int)(sizeof(fproc) / sizeof(struct fproc)))
		panic("receive process out of range: %d", who_p);
	if (who_p >= 0 && fproc[who_p].fp_endpoint == NONE) {
		printf("VFS: ignoring request from %d, endpointless slot %d (%d)\n",
			m_in.m_source, who_p, m_in.m_type);
		continue;
	}

	/* Internal consistency check; our mental image of process numbers and
	 * endpoints must match with how the rest of the system thinks of them.
	 */
	if (who_p >= 0 && fproc[who_p].fp_endpoint != who_e) {
		if (fproc[who_p].fp_endpoint == NONE)
			printf("slot unknown even\n");

		printf("VFS: receive endpoint inconsistent (source %d, who_p "
			"%d, stored ep %d, who_e %d).\n", m_in.m_source, who_p,
			fproc[who_p].fp_endpoint, who_e);
		panic("VFS: inconsistent endpoint ");
	}

	return;
  }
}


/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
PUBLIC void reply(whom, result)
int whom;			/* process to reply to */
int result;			/* result of the call (usually OK or error #) */
{
/* Send a reply to a user process.  If the send fails, just ignore it. */
  int r;

  m_out.reply_type = result;
  r = sendnb(whom, &m_out);
  if (r != OK) {
	printf("VFS: couldn't send reply %d to %d: %d\n", result, whom, r);
  }
}

/*===========================================================================*
 *				service_pm_postponed			     *
 *===========================================================================*/
PRIVATE void service_pm_postponed(void)
{
  int r;
  vir_bytes pc;

  switch(call_nr) {
    case PM_EXEC:
	r = pm_exec(m_in.PM_PROC, m_in.PM_PATH, m_in.PM_PATH_LEN,
		    m_in.PM_FRAME, m_in.PM_FRAME_LEN, &pc);

	/* Reply status to PM */
	m_out.m_type = PM_EXEC_REPLY;
	m_out.PM_PROC = m_in.PM_PROC;
	m_out.PM_PC = (void*)pc;
	m_out.PM_STATUS = r;

	break;

    case PM_EXIT:
	pm_exit(m_in.PM_PROC);

	/* Reply dummy status to PM for synchronization */
	m_out.m_type = PM_EXIT_REPLY;
	m_out.PM_PROC = m_in.PM_PROC;

	break;

    case PM_DUMPCORE:
	/* Copy parameters first. m_in gets overwritten when creating core
	 * file.
	 */

	r = pm_dumpcore(m_in.PM_PROC, m_in.PM_TERM_SIG,
			(vir_bytes) m_in.PM_PATH);

	/* Reply status to PM */
	m_out.m_type = PM_CORE_REPLY;
	m_out.PM_PROC = m_in.PM_PROC;
	m_out.PM_TRACED_PROC = m_in.PM_TRACED_PROC;
	m_out.PM_STATUS = r;

	break;

    default:
	panic("Unhandled postponed PM call %d", m_in.m_type);
  }

  r = send(PM_PROC_NR, &m_out);
  if (r != OK)
	panic("service_pm_postponed: send failed: %d", r);
}

/*===========================================================================*
 *				service_pm				     *
 *===========================================================================*/
PRIVATE void service_pm()
{
  int r, slot;

  switch (call_nr) {
    case PM_SETUID:
	pm_setuid(m_in.PM_PROC, m_in.PM_EID, m_in.PM_RID);

	m_out.m_type = PM_SETUID_REPLY;
	m_out.PM_PROC = m_in.PM_PROC;

	break;

    case PM_SETGID:
	pm_setgid(m_in.PM_PROC, m_in.PM_EID, m_in.PM_RID);

	m_out.m_type = PM_SETGID_REPLY;
	m_out.PM_PROC = m_in.PM_PROC;

	break;

    case PM_SETSID:
	pm_setsid(m_in.PM_PROC);

	m_out.m_type = PM_SETSID_REPLY;
	m_out.PM_PROC = m_in.PM_PROC;

	break;

    case PM_EXEC:
    case PM_EXIT:
    case PM_DUMPCORE:
	okendpt(m_in.PM_PROC, &slot);
	fp = &fproc[slot];

	if (fp->fp_flags & FP_PENDING) {
		/* This process has a request pending, but PM wants it gone.
		 * Forget about the pending request and satisfy PM's request
		 * instead. Note that a pending request AND an EXEC request
		 * are mutually exclusive. Also, PM should send only one
		 * request/process at a time.
		 */
		 assert(fp->fp_job.j_m_in.m_source != PM_PROC_NR);
	}

        /* PM requests on behalf of a proc are handled after the system call
         * that might be in progress for that proc has finished. If the proc
         * is not busy, we start a dummy call */
	if (!(fp->fp_flags & FP_PENDING) && mutex_trylock(&fp->fp_lock) == 0) {
		mutex_unlock(&fp->fp_lock);
		worker_start(do_dummy);
		fp->fp_flags |= FP_DROP_WORK;
        }

	fp->fp_job.j_m_in = m_in;
	fp->fp_flags |= FP_PM_PENDING;

	return;

    case PM_FORK:
    case PM_SRV_FORK:
	pm_fork(m_in.PM_PPROC, m_in.PM_PROC, m_in.PM_CPID);
	m_out.m_type = PM_FORK_REPLY;

	if (call_nr == PM_SRV_FORK) {
		m_out.m_type = PM_SRV_FORK_REPLY;
		pm_setuid(m_in.PM_PROC, m_in.PM_REUID, m_in.PM_REUID);
		pm_setgid(m_in.PM_PROC, m_in.PM_REGID, m_in.PM_REGID);
	}

	m_out.PM_PROC = m_in.PM_PROC;

	break;
    case PM_SETGROUPS:
	pm_setgroups(m_in.PM_PROC, m_in.PM_GROUP_NO,
			(gid_t *) m_in.PM_GROUP_ADDR);

	m_out.m_type = PM_SETGROUPS_REPLY;
	m_out.PM_PROC = m_in.PM_PROC;

	break;

    case PM_UNPAUSE:
	unpause(m_in.PM_PROC);

	m_out.m_type = PM_UNPAUSE_REPLY;
	m_out.PM_PROC = m_in.PM_PROC;

	break;

    case PM_REBOOT:
	pm_reboot();

	/* Reply dummy status to PM for synchronization */
	m_out.m_type = PM_REBOOT_REPLY;

	break;

    default:
	printf("VFS: don't know how to handle PM request %d\n", call_nr);

	return;
  }

  r = send(PM_PROC_NR, &m_out);
  if (r != OK)
	panic("service_pm: send failed: %d", r);

}


/*===========================================================================*
 *				unblock					     *
 *===========================================================================*/
PRIVATE int unblock(rfp)
struct fproc *rfp;
{
  int blocked_on;

  fp = rfp;
  blocked_on = rfp->fp_blocked_on;
  m_in.m_type = rfp->fp_block_callnr;
  m_in.fd = scratch(fp).file.fd_nr;
  m_in.buffer = scratch(fp).io.io_buffer;
  m_in.nbytes = scratch(fp).io.io_nbytes;

  rfp->fp_blocked_on = FP_BLOCKED_ON_NONE;	/* no longer blocked */
  rfp->fp_flags &= ~FP_REVIVED;
  reviving--;
  assert(reviving >= 0);

  /* This should be a pipe I/O, not a device I/O. If it is, it'll 'leak'
   * grants.
   */
  assert(!GRANT_VALID(rfp->fp_grant));

  /* Pending pipe reads/writes can be handled directly */
  if (blocked_on == FP_BLOCKED_ON_PIPE) {
	worker_start(do_pending_pipe);
	yield();	/* Give thread a chance to run */
	return(0);	/* Retrieve more work */
  }

  return(1);	/* We've unblocked a process */
}
