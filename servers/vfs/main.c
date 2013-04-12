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
static void *do_async_dev_result(void *arg);
static void *do_control_msgs(void *arg);
static void *do_dev_event(void *arg);
static void *do_fs_reply(struct job *job);
static void *do_work(void *arg);
static void *do_pm(void *arg);
static void *do_init_root(void *arg);
static void handle_work(void *(*func)(void *arg));

static void get_work(void);
static void lock_pm(void);
static void unlock_pm(void);
static void service_pm(void);
static void service_pm_postponed(void);
static int unblock(struct fproc *rfp);

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init_fresh(int type, sef_init_info_t *info);
static mutex_t pm_lock;
static endpoint_t receive_from;

/*===========================================================================*
 *				main					     *
 *===========================================================================*/
int main(void)
{
/* This is the main program of the file system.  The main loop consists of
 * three major activities: getting new work, processing the work, and sending
 * the reply.  This loop never terminates as long as the file system runs.
 */
  int transid;
  struct job *job;

  /* SEF local startup. */
  sef_local_startup();

  printf("Started VFS: %d worker thread(s)\n", NR_WTHREADS);

  if (OK != (sys_getkinfo(&kinfo)))
	panic("couldn't get kernel kinfo");

  /* This is the main loop that gets work, processes it, and sends replies. */
  while (TRUE) {
	yield_all();	/* let other threads run */
	self = NULL;
	job = NULL;
	send_work();
	get_work();

	transid = TRNS_GET_ID(m_in.m_type);
	if (IS_VFS_FS_TRANSID(transid)) {
		job = worker_getjob( (thread_t) transid - VFS_TRANSID);
		if (job == NULL) {
			printf("VFS: spurious message %d from endpoint %d\n",
				m_in.m_type, m_in.m_source);
			continue;
		}
		m_in.m_type = TRNS_DEL_ID(m_in.m_type);
	}

	if (job != NULL) {
		do_fs_reply(job);
		continue;
	} else if (who_e == PM_PROC_NR) { /* Calls from PM */
		/* Special control messages from PM */
		sys_worker_start(do_pm);
		continue;
	} else if (is_notify(call_nr)) {
		/* A task notify()ed us */
		if (who_e == DS_PROC_NR)
			handle_work(ds_event);
		else if (who_e == KERNEL)
			mthread_stacktraces();
		else if (fp != NULL && (fp->fp_flags & FP_SRV_PROC))
			handle_work(do_dev_event);
		else
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
	if (IS_DRV_REPLY(call_nr)) {
		/* We've got results for a device request */

		struct dmap *dp;

		dp = get_dmap(who_e);
		if (dp != NULL) {
			if (dev_style_asyn(dp->dmap_style)) {
				handle_work(do_async_dev_result);

			} else {
				if (dp->dmap_servicing == NONE) {
					printf("Got spurious dev reply from %d",
					who_e);
				} else {
					dev_reply(dp);
				}
			}
			continue;
		}
		printf("VFS: ignoring dev reply from unknown driver %d\n",
			who_e);
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
static void handle_work(void *(*func)(void *arg))
{
/* Handle asynchronous device replies and new system calls. If the originating
 * endpoint is an FS endpoint, take extra care not to get in deadlock. */
  struct vmnt *vmp = NULL;
  endpoint_t proc_e;

  proc_e = m_in.m_source;

  if (fp->fp_flags & FP_SRV_PROC) {
	vmp = find_vmnt(proc_e);
	if (vmp != NULL) {
		/* A call back or dev result from an FS
		 * endpoint. Set call back flag. Can do only
		 * one call back at a time.
		 */
		if (vmp->m_flags & VMNT_CALLBACK) {
			replycode(proc_e, EAGAIN);
			return;
		}
		vmp->m_flags |= VMNT_CALLBACK;
		if (vmp->m_flags & VMNT_MOUNTING) {
			vmp->m_flags |= VMNT_FORCEROOTBSF;
		}
	}

	if (worker_available() == 0) {
		if (!deadlock_resolving) {
			deadlock_resolving = 1;
			dl_worker_start(func);
			return;
		}

		if (vmp != NULL) {
			/* Already trying to resolve a deadlock, can't
			 * handle more, sorry */

			replycode(proc_e, EAGAIN);
			return;
		}
	}
  }

  worker_start(func);
}

/*===========================================================================*
 *			       do_async_dev_result			     *
 *===========================================================================*/
static void *do_async_dev_result(void *arg)
{
  endpoint_t endpt;
  struct job my_job;

  my_job = *((struct job *) arg);
  fp = my_job.j_fp;

  /* An asynchronous character driver has results for us */
  if (job_call_nr == DEV_REVIVE) {
	endpt = job_m_in.REP_ENDPT;
	if (endpt == VFS_PROC_NR)
		endpt = find_suspended_ep(job_m_in.m_source,
					  job_m_in.REP_IO_GRANT);

	if (endpt == NONE) {
		printf("VFS: proc with grant %d from %d not found\n",
			job_m_in.REP_IO_GRANT, job_m_in.m_source);
	} else if (job_m_in.REP_STATUS == SUSPEND) {
		printf("VFS: got SUSPEND on DEV_REVIVE: not reviving proc\n");
	} else
		revive(endpt, job_m_in.REP_STATUS);
  }
  else if (job_call_nr == DEV_OPEN_REPL) open_reply();
  else if (job_call_nr == DEV_REOPEN_REPL) reopen_reply();
  else if (job_call_nr == DEV_CLOSE_REPL) close_reply();
  else if (job_call_nr == DEV_SEL_REPL1)
	select_reply1(job_m_in.m_source, job_m_in.DEV_MINOR,
		      job_m_in.DEV_SEL_OPS);
  else if (job_call_nr == DEV_SEL_REPL2)
	select_reply2(job_m_in.m_source, job_m_in.DEV_MINOR,
		      job_m_in.DEV_SEL_OPS);

  thread_cleanup(fp);
  return(NULL);
}

/*===========================================================================*
 *			       do_control_msgs				     *
 *===========================================================================*/
static void *do_control_msgs(void *arg)
{
  struct job my_job;

  my_job = *((struct job *) arg);
  fp = my_job.j_fp;

  /* Check for special control messages. */
  if (job_m_in.m_source == CLOCK) {
	/* Alarm timer expired. Used only for select(). Check it. */
	expire_timers(job_m_in.NOTIFY_TIMESTAMP);
  }

  thread_cleanup(NULL);
  return(NULL);
}

/*===========================================================================*
 *			       do_dev_event				     *
 *===========================================================================*/
static void *do_dev_event(void *arg)
{
/* Device notifies us of an event. */
  struct job my_job;

  my_job = *((struct job *) arg);
  fp = my_job.j_fp;

  dev_status(job_m_in.m_source);

  thread_cleanup(fp);
  return(NULL);
}

/*===========================================================================*
 *			       do_fs_reply				     *
 *===========================================================================*/
static void *do_fs_reply(struct job *job)
{
  struct vmnt *vmp;
  struct worker_thread *wp;

  if ((vmp = find_vmnt(who_e)) == NULL)
	panic("Couldn't find vmnt for endpoint %d", who_e);

  wp = worker_get(job->j_fp->fp_wtid);

  if (wp == NULL) {
	printf("VFS: spurious reply from %d\n", who_e);
	return(NULL);
  }

  if (wp->w_task != who_e) {
	printf("VFS: expected %d to reply, not %d\n", wp->w_task, who_e);
	return(NULL);
  }
  *wp->w_fs_sendrec = m_in;
  wp->w_task = NONE;
  vmp->m_comm.c_cur_reqs--; /* We've got our reply, make room for others */
  worker_signal(wp); /* Continue this thread */
  return(NULL);
}

/*===========================================================================*
 *				lock_pm					     *
 *===========================================================================*/
static void lock_pm(void)
{
  struct fproc *org_fp;
  struct worker_thread *org_self;

  /* First try to get it right off the bat */
  if (mutex_trylock(&pm_lock) == 0)
	return;

  org_fp = fp;
  org_self = self;

  if (mutex_lock(&pm_lock) != 0)
	panic("Could not obtain lock on pm\n");

  fp = org_fp;
  self = org_self;
}

/*===========================================================================*
 *				unlock_pm				     *
 *===========================================================================*/
static void unlock_pm(void)
{
  if (mutex_unlock(&pm_lock) != 0)
	panic("Could not release lock on pm");
}

/*===========================================================================*
 *			       do_pm					     *
 *===========================================================================*/
static void *do_pm(void *arg __unused)
{
  lock_pm();
  service_pm();
  unlock_pm();

  thread_cleanup(NULL);
  return(NULL);
}

/*===========================================================================*
 *			       do_pending_pipe				     *
 *===========================================================================*/
static void *do_pending_pipe(void *arg)
{
  int r, op;
  struct job my_job;
  struct filp *f;
  tll_access_t locktype;

  my_job = *((struct job *) arg);
  fp = my_job.j_fp;

  lock_proc(fp, 1 /* force lock */);

  f = scratch(fp).file.filp;
  assert(f != NULL);
  scratch(fp).file.filp = NULL;

  locktype = (job_call_nr == READ) ? VNODE_READ : VNODE_WRITE;
  op = (job_call_nr == READ) ? READING : WRITING;
  lock_filp(f, locktype);

  r = rw_pipe(op, who_e, f, scratch(fp).io.io_buffer, scratch(fp).io.io_nbytes);

  if (r != SUSPEND)  /* Do we have results to report? */
	replycode(fp->fp_endpoint, r);

  unlock_filp(f);
  thread_cleanup(fp);
  unlock_proc(fp);
  return(NULL);
}

/*===========================================================================*
 *			       do_dummy					     *
 *===========================================================================*/
void *do_dummy(void *arg)
{
  struct job my_job;
  int r;

  my_job = *((struct job *) arg);
  fp = my_job.j_fp;

  if ((r = mutex_trylock(&fp->fp_lock)) == 0) {
	thread_cleanup(fp);
	unlock_proc(fp);
  } else {
	/* Proc is busy, let that worker thread carry out the work */
	thread_cleanup(NULL);
  }
  return(NULL);
}

/*===========================================================================*
 *			       do_work					     *
 *===========================================================================*/
static void *do_work(void *arg)
{
  int error;
  struct job my_job;
  message m_out;

  memset(&m_out, 0, sizeof(m_out));

  my_job = *((struct job *) arg);
  fp = my_job.j_fp;

  lock_proc(fp, 0); /* This proc is busy */

  if (job_call_nr == MAPDRIVER) {
	error = do_mapdriver();
  } else if (job_call_nr == COMMON_GETSYSINFO) {
	error = do_getsysinfo();
  } else if (IS_PFS_VFS_RQ(job_call_nr)) {
	if (who_e != PFS_PROC_NR) {
		printf("VFS: only PFS is allowed to make nested VFS calls\n");
		error = ENOSYS;
	} else if (job_call_nr <= PFS_BASE ||
		   job_call_nr >= PFS_BASE + PFS_NREQS) {
		error = ENOSYS;
	} else {
		job_call_nr -= PFS_BASE;
		error = (*pfs_call_vec[job_call_nr])(&m_out);
	}
  } else {
	/* We're dealing with a POSIX system call from a normal
	 * process. Call the internal function that does the work.
	 */
	if (job_call_nr < 0 || job_call_nr >= NCALLS) {
		error = ENOSYS;
	} else if (fp->fp_pid == PID_FREE) {
		/* Process vanished before we were able to handle request.
		 * Replying has no use. Just drop it. */
		error = SUSPEND;
	} else {
#if ENABLE_SYSCALL_STATS
		calls_stats[job_call_nr]++;
#endif
		error = (*call_vec[job_call_nr])(&m_out);
	}
  }

  /* Copy the results back to the user and send reply. */
  if (error != SUSPEND) reply(&m_out, fp->fp_endpoint, error);

  thread_cleanup(fp);
  unlock_proc(fp);
  return(NULL);
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
static void sef_local_startup()
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
static int sef_cb_init_fresh(int UNUSED(type), sef_init_info_t *info)
{
/* Initialize the virtual file server. */
  int s, i;
  struct fproc *rfp;
  message mess;
  struct rprocpub rprocpub[NR_BOOT_PROCS];

  force_sync = 0;
  receive_from = ANY;
  self = NULL;
  verbose = 0;

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
			    (vir_bytes) rprocpub, sizeof(rprocpub))) != OK){
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

  init_dmap_locks();		/* init dmap locks */
  init_vnodes();		/* init vnodes */
  init_vmnts();			/* init vmnt structures */
  init_select();		/* init select() structures */
  init_filps();			/* Init filp structures */
  mount_pfs();			/* mount Pipe File Server */
  worker_start(do_init_root);	/* mount initial ramdisk as file system root */
  yield();			/* force do_init_root to start */
  self = NULL;

  return(OK);
}

/*===========================================================================*
 *			       do_init_root				     *
 *===========================================================================*/
static void *do_init_root(void *arg)
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
  r = mount_fs(DEV_IMGRD, "bootramdisk", "/", MFS_PROC_NR, 0, mount_label);
  if (r != OK)
	panic("Failed to initialize root");
  receive_from = ANY;

  unlock_pm();
  thread_cleanup(fp);
  unlock_proc(fp);
  return(NULL);
}

/*===========================================================================*
 *				lock_proc				     *
 *===========================================================================*/
void lock_proc(struct fproc *rfp, int force_lock)
{
  int r;
  struct fproc *org_fp;
  struct worker_thread *org_self;

  r = mutex_trylock(&rfp->fp_lock);

  /* Were we supposed to obtain this lock immediately? */
  if (force_lock) {
	assert(r == 0);
	return;
  }

  if (r == 0) return;

  org_fp = fp;
  org_self = self;

  if ((r = mutex_lock(&rfp->fp_lock)) != 0)
	panic("unable to lock fproc lock: %d", r);

  fp = org_fp;
  self = org_self;
}

/*===========================================================================*
 *				unlock_proc				     *
 *===========================================================================*/
void unlock_proc(struct fproc *rfp)
{
  int r;

  if ((r = mutex_unlock(&rfp->fp_lock)) != 0)
	panic("Failed to unlock: %d", r);
}

/*===========================================================================*
 *				thread_cleanup				     *
 *===========================================================================*/
void thread_cleanup(struct fproc *rfp)
{
/* Clean up worker thread. Skip parts if this thread is not associated
 * with a particular process (i.e., rfp is NULL) */

#if LOCK_DEBUG
  if (rfp != NULL) {
	check_filp_locks_by_me();
	check_vnode_locks_by_me(rfp);
	check_vmnt_locks_by_me(rfp);
  }
#endif

  if (rfp != NULL && rfp->fp_flags & FP_PM_PENDING) {	/* Postponed PM call */
	job_m_in = rfp->fp_job.j_m_in;
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
	if (rfp->fp_flags & FP_SRV_PROC) {
		struct vmnt *vmp;

		if ((vmp = find_vmnt(rfp->fp_endpoint)) != NULL) {
			vmp->m_flags &= ~VMNT_CALLBACK;
		}
	}
  }

  if (deadlock_resolving) {
	if (self->w_tid == dl_worker.w_tid)
		deadlock_resolving = 0;
  }
}

/*===========================================================================*
 *				get_work				     *
 *===========================================================================*/
static void get_work()
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
	if (proc_p < 0 || proc_p >= NR_PROCS) fp = NULL;
	else fp = &fproc[proc_p];

	if (m_in.m_type == EDEADSRCDST) return;	/* Failed 'sendrec' */

	/* Negative who_p is never used to access the fproc array. Negative
	 * numbers (kernel tasks) are treated in a special way.
	 */
	if (who_p >= (int)(sizeof(fproc) / sizeof(struct fproc)))
		panic("receive process out of range: %d", who_p);
	if (who_p >= 0 && fproc[who_p].fp_endpoint == NONE) {
		printf("VFS: ignoring request from %d: NONE endpoint %d (%d)\n",
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
void reply(message *m_out, endpoint_t whom, int result)
{
/* Send a reply to a user process.  If the send fails, just ignore it. */
  int r;

  m_out->reply_type = result;
  r = sendnb(whom, m_out);
  if (r != OK) {
	printf("VFS: %d couldn't send reply %d to %d: %d\n", mthread_self(),
		result, whom, r);
	util_stacktrace();
  }
}

/*===========================================================================*
 *				replycode				     *
 *===========================================================================*/
void replycode(endpoint_t whom, int result)
{
/* Send a reply to a user process.  If the send fails, just ignore it. */
  int r;
  message m_out;

  memset(&m_out, 0, sizeof(m_out));

  m_out.reply_type = result;
  r = sendnb(whom, &m_out);
  if (r != OK) {
	printf("VFS: %d couldn't send reply %d to %d: %d\n", mthread_self(),
		result, whom, r);
	util_stacktrace();
  }
}

/*===========================================================================*
 *				service_pm_postponed			     *
 *===========================================================================*/
static void service_pm_postponed(void)
{
  int r;
  vir_bytes pc, newsp;
  message m_out;

  memset(&m_out, 0, sizeof(m_out));

  switch(job_call_nr) {
    case PM_EXEC:
	{
		endpoint_t proc_e;
		vir_bytes exec_path, stack_frame;
		size_t exec_path_len, stack_frame_len;

		proc_e = job_m_in.PM_PROC;
		exec_path = (vir_bytes) job_m_in.PM_PATH;
		exec_path_len = (size_t) job_m_in.PM_PATH_LEN;
		stack_frame = (vir_bytes) job_m_in.PM_FRAME;
		stack_frame_len = (size_t) job_m_in.PM_FRAME_LEN;

		r = pm_exec(proc_e, exec_path, exec_path_len, stack_frame,
			    stack_frame_len, &pc, &newsp, job_m_in.PM_EXECFLAGS);

		/* Reply status to PM */
		m_out.m_type = PM_EXEC_REPLY;
		m_out.PM_PROC = proc_e;
		m_out.PM_PC = (void*) pc;
		m_out.PM_STATUS = r;
		m_out.PM_NEWSP = (void *) newsp;
	}
	break;

    case PM_EXIT:
	{
		endpoint_t proc_e;
		proc_e = job_m_in.PM_PROC;

		pm_exit(proc_e);

		/* Reply dummy status to PM for synchronization */
		m_out.m_type = PM_EXIT_REPLY;
		m_out.PM_PROC = proc_e;
	}
	break;

    case PM_DUMPCORE:
	{
		endpoint_t proc_e, traced_proc_e;
		int term_signal;
		vir_bytes core_path;

		proc_e = job_m_in.PM_PROC;
		traced_proc_e = job_m_in.PM_TRACED_PROC;
		if(job_m_in.PM_PROC != job_m_in.PM_TRACED_PROC) {
			/* dumpcore request */
			term_signal = 0;
		} else {
			/* dumpcore on exit */
			term_signal = job_m_in.PM_TERM_SIG;
		}
		core_path = (vir_bytes) job_m_in.PM_PATH;

		r = pm_dumpcore(proc_e, term_signal, core_path);

		/* Reply status to PM */
		m_out.m_type = PM_CORE_REPLY;
		m_out.PM_PROC = proc_e;
		m_out.PM_TRACED_PROC = traced_proc_e;
		m_out.PM_STATUS = r;
	}
	break;

    default:
	panic("Unhandled postponed PM call %d", job_m_in.m_type);
  }

  r = send(PM_PROC_NR, &m_out);
  if (r != OK)
	panic("service_pm_postponed: send failed: %d", r);
}

/*===========================================================================*
 *				service_pm				     *
 *===========================================================================*/
static void service_pm()
{
  int r, slot;
  message m_out;

  memset(&m_out, 0, sizeof(m_out));

  switch (job_call_nr) {
    case PM_SETUID:
	{
		endpoint_t proc_e;
		uid_t euid, ruid;

		proc_e = job_m_in.PM_PROC;
		euid = job_m_in.PM_EID;
		ruid = job_m_in.PM_RID;

		pm_setuid(proc_e, euid, ruid);

		m_out.m_type = PM_SETUID_REPLY;
		m_out.PM_PROC = proc_e;
	}
	break;

    case PM_SETGID:
	{
		endpoint_t proc_e;
		gid_t egid, rgid;

		proc_e = job_m_in.PM_PROC;
		egid = job_m_in.PM_EID;
		rgid = job_m_in.PM_RID;

		pm_setgid(proc_e, egid, rgid);

		m_out.m_type = PM_SETGID_REPLY;
		m_out.PM_PROC = proc_e;
	}
	break;

    case PM_SETSID:
	{
		endpoint_t proc_e;

		proc_e = job_m_in.PM_PROC;
		pm_setsid(proc_e);

		m_out.m_type = PM_SETSID_REPLY;
		m_out.PM_PROC = proc_e;
	}
	break;

    case PM_EXEC:
    case PM_EXIT:
    case PM_DUMPCORE:
	{
		endpoint_t proc_e = job_m_in.PM_PROC;

		if(isokendpt(proc_e, &slot) != OK) {
			printf("VFS: proc ep %d not ok\n", proc_e);
			return;
		}

		fp = &fproc[slot];

		if (fp->fp_flags & FP_PENDING) {
			/* This process has a request pending, but PM wants it
			 * gone. Forget about the pending request and satisfy
			 * PM's request instead. Note that a pending request
			 * AND an EXEC request are mutually exclusive. Also, PM
			 * should send only one request/process at a time.
			 */
			 assert(fp->fp_job.j_m_in.m_source != PM_PROC_NR);
		}

		/* PM requests on behalf of a proc are handled after the
		 * system call that might be in progress for that proc has
		 * finished. If the proc is not busy, we start a dummy call.
		 */
		if (!(fp->fp_flags & FP_PENDING) &&
					mutex_trylock(&fp->fp_lock) == 0) {
			mutex_unlock(&fp->fp_lock);
			worker_start(do_dummy);
			fp->fp_flags |= FP_DROP_WORK;
		}

		fp->fp_job.j_m_in = job_m_in;
		fp->fp_flags |= FP_PM_PENDING;

		return;
	}
    case PM_FORK:
    case PM_SRV_FORK:
	{
		endpoint_t pproc_e, proc_e;
		pid_t child_pid;
		uid_t reuid;
		gid_t regid;

		pproc_e = job_m_in.PM_PPROC;
		proc_e = job_m_in.PM_PROC;
		child_pid = job_m_in.PM_CPID;
		reuid = job_m_in.PM_REUID;
		regid = job_m_in.PM_REGID;

		pm_fork(pproc_e, proc_e, child_pid);
		m_out.m_type = PM_FORK_REPLY;

		if (job_call_nr == PM_SRV_FORK) {
			m_out.m_type = PM_SRV_FORK_REPLY;
			pm_setuid(proc_e, reuid, reuid);
			pm_setgid(proc_e, regid, regid);
		}

		m_out.PM_PROC = proc_e;
	}
	break;
    case PM_SETGROUPS:
	{
		endpoint_t proc_e;
		int group_no;
		gid_t *group_addr;

		proc_e = job_m_in.PM_PROC;
		group_no = job_m_in.PM_GROUP_NO;
		group_addr = (gid_t *) job_m_in.PM_GROUP_ADDR;

		pm_setgroups(proc_e, group_no, group_addr);

		m_out.m_type = PM_SETGROUPS_REPLY;
		m_out.PM_PROC = proc_e;
	}
	break;

    case PM_UNPAUSE:
	{
		endpoint_t proc_e;

		proc_e = job_m_in.PM_PROC;

		unpause(proc_e);

		m_out.m_type = PM_UNPAUSE_REPLY;
		m_out.PM_PROC = proc_e;
	}
	break;

    case PM_REBOOT:
	pm_reboot();

	/* Reply dummy status to PM for synchronization */
	m_out.m_type = PM_REBOOT_REPLY;

	break;

    default:
	printf("VFS: don't know how to handle PM request %d\n", job_call_nr);

	return;
  }

  r = send(PM_PROC_NR, &m_out);
  if (r != OK)
	panic("service_pm: send failed: %d", r);

}


/*===========================================================================*
 *				unblock					     *
 *===========================================================================*/
static int unblock(rfp)
struct fproc *rfp;
{
  int blocked_on;

  fp = rfp;
  blocked_on = rfp->fp_blocked_on;
  m_in.m_source = rfp->fp_endpoint;
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
	self = NULL;
	return(0);	/* Retrieve more work */
  }

  return(1);	/* We've unblocked a process */
}
