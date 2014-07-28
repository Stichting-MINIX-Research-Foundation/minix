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
#include <minix/const.h>
#include <minix/endpoint.h>
#include <minix/safecopies.h>
#include <minix/debug.h>
#include <minix/vfsif.h>
#include "file.h"
#include "scratchpad.h"
#include "vmnt.h"
#include "vnode.h"

#if ENABLE_SYSCALL_STATS
EXTERN unsigned long calls_stats[NR_VFS_CALLS];
#endif

/* Thread related prototypes */
static void do_reply(struct worker_thread *wp);
static void do_work(void);
static void do_init_root(void);
static void handle_work(void (*func)(void));
static void reply(message *m_out, endpoint_t whom, int result);

static void get_work(void);
static void service_pm(void);
static int unblock(struct fproc *rfp);

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init_fresh(int type, sef_init_info_t *info);
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
  struct worker_thread *wp;

  /* SEF local startup. */
  sef_local_startup();

  printf("Started VFS: %d worker thread(s)\n", NR_WTHREADS);

  if (OK != (sys_getkinfo(&kinfo)))
	panic("couldn't get kernel kinfo");

  /* This is the main loop that gets work, processes it, and sends replies. */
  while (TRUE) {
	yield_all();	/* let other threads run */
	self = NULL;
	send_work();
	get_work();

	transid = TRNS_GET_ID(m_in.m_type);
	if (IS_VFS_FS_TRANSID(transid)) {
		wp = worker_get((thread_t) transid - VFS_TRANSID);
		if (wp == NULL || wp->w_fp == NULL) {
			printf("VFS: spurious message %d from endpoint %d\n",
				m_in.m_type, m_in.m_source);
			continue;
		}
		m_in.m_type = TRNS_DEL_ID(m_in.m_type);
		do_reply(wp);
		continue;
	} else if (who_e == PM_PROC_NR) { /* Calls from PM */
		/* Special control messages from PM */
		service_pm();
		continue;
	} else if (is_notify(call_nr)) {
		/* A task ipc_notify()ed us */
		switch (who_e) {
		case DS_PROC_NR:
			/* Start a thread to handle DS events, if no thread
			 * is pending or active for it already. DS is not
			 * supposed to issue calls to VFS or be the subject of
			 * postponed PM requests, so this should be no problem.
			 */
			if (worker_can_start(fp))
				handle_work(ds_event);
			break;
		case KERNEL:
			mthread_stacktraces();
			break;
		case CLOCK:
			/* Timer expired. Used only for select(). Check it. */
			expire_timers(m_in.m_notify.timestamp);
			break;
		default:
			printf("VFS: ignoring notification from %d\n", who_e);
		}
		continue;
	} else if (who_p < 0) { /* i.e., message comes from a task */
		/* We're going to ignore this message. Tasks should
		 * send ipc_notify()s only.
		 */
		 printf("VFS: ignoring message from %d (%d)\n", who_e, call_nr);
		 continue;
	}

	if (IS_BDEV_RS(call_nr)) {
		/* We've got results for a block device request. */
		bdev_reply();
	} else if (IS_CDEV_RS(call_nr)) {
		/* We've got results for a character device request. */
		cdev_reply();
	} else {
		/* Normal syscall. This spawns a new thread. */
		handle_work(do_work);
	}
  }
  return(OK);				/* shouldn't come here */
}

/*===========================================================================*
 *			       handle_work				     *
 *===========================================================================*/
static void handle_work(void (*func)(void))
{
/* Handle asynchronous device replies and new system calls. If the originating
 * endpoint is an FS endpoint, take extra care not to get in deadlock. */
  struct vmnt *vmp = NULL;
  endpoint_t proc_e;
  int use_spare = FALSE;

  proc_e = m_in.m_source;

  if (fp->fp_flags & FP_SRV_PROC) {
	vmp = find_vmnt(proc_e);
	if (vmp != NULL) {
		/* A callback from an FS endpoint. Can do only one at once. */
		if (vmp->m_flags & VMNT_CALLBACK) {
			replycode(proc_e, EAGAIN);
			return;
		}
		/* Already trying to resolve a deadlock? Can't handle more. */
		if (worker_available() == 0) {
			replycode(proc_e, EAGAIN);
			return;
		}
		/* A thread is available. Set callback flag. */
		vmp->m_flags |= VMNT_CALLBACK;
		if (vmp->m_flags & VMNT_MOUNTING) {
			vmp->m_flags |= VMNT_FORCEROOTBSF;
		}
	}

	/* Use the spare thread to handle this request if needed. */
	use_spare = TRUE;
  }

  worker_start(fp, func, &m_in, use_spare);
}


/*===========================================================================*
 *			       do_reply				             *
 *===========================================================================*/
static void do_reply(struct worker_thread *wp)
{
  struct vmnt *vmp = NULL;

  if(who_e != VM_PROC_NR && (vmp = find_vmnt(who_e)) == NULL)
	panic("Couldn't find vmnt for endpoint %d", who_e);

  if (wp->w_task != who_e) {
	printf("VFS: tid %d: expected %d to reply, not %d\n",
		wp->w_tid, wp->w_task, who_e);
  }
  *wp->w_sendrec = m_in;
  wp->w_task = NONE;
  if(vmp) vmp->m_comm.c_cur_reqs--; /* We've got our reply, make room for others */
  worker_signal(wp); /* Continue this thread */
}

/*===========================================================================*
 *			       do_pending_pipe				     *
 *===========================================================================*/
static void do_pending_pipe(void)
{
  int r, op;
  struct filp *f;
  tll_access_t locktype;

  f = scratch(fp).file.filp;
  assert(f != NULL);
  scratch(fp).file.filp = NULL;

  locktype = (job_call_nr == VFS_READ) ? VNODE_READ : VNODE_WRITE;
  op = (job_call_nr == VFS_READ) ? READING : WRITING;
  lock_filp(f, locktype);

  r = rw_pipe(op, who_e, f, scratch(fp).io.io_buffer, scratch(fp).io.io_nbytes);

  if (r != SUSPEND)  /* Do we have results to report? */
	replycode(fp->fp_endpoint, r);

  unlock_filp(f);
}

/*===========================================================================*
 *			       do_work					     *
 *===========================================================================*/
static void do_work(void)
{
  unsigned int call_index;
  int error;

  if (fp->fp_pid == PID_FREE) {
	/* Process vanished before we were able to handle request.
	 * Replying has no use. Just drop it.
	 */
	return;
  }

  memset(&job_m_out, 0, sizeof(job_m_out));

  /* At this point we assume that we're dealing with a call that has been
   * made specifically to VFS. Typically it will be a POSIX call from a
   * normal process, but we also handle a few calls made by drivers such
   * such as UDS and VND through here. Call the internal function that
   * does the work.
   */
  if (IS_VFS_CALL(job_call_nr)) {
	call_index = (unsigned int) (job_call_nr - VFS_BASE);

	if (call_index < NR_VFS_CALLS && call_vec[call_index] != NULL) {
#if ENABLE_SYSCALL_STATS
		calls_stats[call_index]++;
#endif
		error = (*call_vec[call_index])();
	} else
		error = ENOSYS;
  } else
	error = ENOSYS;

  /* Copy the results back to the user and send reply. */
  if (error != SUSPEND) reply(&job_m_out, fp->fp_endpoint, error);
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

	if (mess.m_type != VFS_PM_INIT)
		panic("unexpected message from PM: %d", mess.m_type);

	if (NONE == mess.VFS_PM_ENDPT) break;

	rfp = &fproc[mess.VFS_PM_SLOT];
	rfp->fp_flags = FP_NOFLAGS;
	rfp->fp_pid = mess.VFS_PM_PID;
	rfp->fp_endpoint = mess.VFS_PM_ENDPT;
	rfp->fp_grant = GRANT_INVALID;
	rfp->fp_blocked_on = FP_BLOCKED_ON_NONE;
	rfp->fp_realuid = (uid_t) SYS_UID;
	rfp->fp_effuid = (uid_t) SYS_UID;
	rfp->fp_realgid = (gid_t) SYS_GID;
	rfp->fp_effgid = (gid_t) SYS_GID;
	rfp->fp_umask = ~0;
  } while (TRUE);			/* continue until process NONE */
  mess.m_type = OK;			/* tell PM that we succeeded */
  s = ipc_send(PM_PROC_NR, &mess);		/* send synchronization message */

  system_hz = sys_hz();

  /* Subscribe to block and character driver events. */
  s = ds_subscribe("drv\\.[bc]..\\..*", DSF_INITIAL | DSF_OVERWRITE);
  if (s != OK) panic("VFS: can't subscribe to driver events (%d)", s);

  /* Initialize worker threads */
  worker_init();

  /* Initialize global locks */
  if (mthread_mutex_init(&bsf_lock, NULL) != 0)
	panic("VFS: couldn't initialize block special file lock");

  init_dmap();			/* Initialize device table. */

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

  /* Initialize locks and initial values for all processes. */
  for (rfp = &fproc[0]; rfp < &fproc[NR_PROCS]; rfp++) {
	if (mutex_init(&rfp->fp_lock, NULL) != 0)
		panic("unable to initialize fproc lock");
	rfp->fp_worker = NULL;
#if LOCK_DEBUG
	rfp->fp_vp_rdlocks = 0;
	rfp->fp_vmnt_rdlocks = 0;
#endif

	/* Initialize process directories. mount_fs will set them to the
	 * correct values.
	 */
	for (i = 0; i < OPEN_MAX; i++)
		rfp->fp_filp[i] = NULL;
	rfp->fp_rd = NULL;
	rfp->fp_wd = NULL;
  }

  init_vnodes();		/* init vnodes */
  init_vmnts();			/* init vmnt structures */
  init_select();		/* init select() structures */
  init_filps();			/* Init filp structures */
  mount_pfs();			/* mount Pipe File Server */

  /* Mount initial ramdisk as file system root. */
  receive_from = MFS_PROC_NR;
  worker_start(fproc_addr(VFS_PROC_NR), do_init_root, &mess /*unused*/,
	FALSE /*use_spare*/);

  return(OK);
}

/*===========================================================================*
 *			       do_init_root				     *
 *===========================================================================*/
static void do_init_root(void)
{
  int r;
  char *mount_type = "mfs"; /* FIXME: use boot image process name instead */
  char *mount_label = "fs_imgrd"; /* FIXME: obtain this from RS */

  r = mount_fs(DEV_IMGRD, "bootramdisk", "/", MFS_PROC_NR, 0, mount_type,
	mount_label);
  if (r != OK)
	panic("Failed to initialize root");
  receive_from = ANY;
}

/*===========================================================================*
 *				lock_proc				     *
 *===========================================================================*/
void lock_proc(struct fproc *rfp)
{
  int r;
  struct worker_thread *org_self;

  r = mutex_trylock(&rfp->fp_lock);
  if (r == 0) return;

  org_self = worker_suspend();

  if ((r = mutex_lock(&rfp->fp_lock)) != 0)
	panic("unable to lock fproc lock: %d", r);

  worker_resume(org_self);
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
void thread_cleanup(void)
{
/* Perform cleanup actions for a worker thread. */

#if LOCK_DEBUG
  check_filp_locks_by_me();
  check_vnode_locks_by_me(fp);
  check_vmnt_locks_by_me(fp);
#endif

  if (fp->fp_flags & FP_SRV_PROC) {
	struct vmnt *vmp;

	if ((vmp = find_vmnt(fp->fp_endpoint)) != NULL) {
		vmp->m_flags &= ~VMNT_CALLBACK;
	}
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

	if (m_in.m_type == EDEADSRCDST) {
		printf("VFS: failed ipc_sendrec\n");
		return;	/* Failed 'ipc_sendrec' */
	}

	/* Negative who_p is never used to access the fproc array. Negative
	 * numbers (kernel tasks) are treated in a special way.
	 */
	if (fp && fp->fp_endpoint == NONE) {
		printf("VFS: ignoring request from %d: NONE endpoint %d (%d)\n",
			m_in.m_source, who_p, m_in.m_type);
		continue;
	}

	/* Internal consistency check; our mental image of process numbers and
	 * endpoints must match with how the rest of the system thinks of them.
	 */
	if (fp && fp->fp_endpoint != who_e) {
		if (fproc[who_p].fp_endpoint == NONE)
			printf("slot unknown even\n");

		panic("VFS: receive endpoint inconsistent (source %d, who_p "
			"%d, stored ep %d, who_e %d).\n", m_in.m_source, who_p,
			fproc[who_p].fp_endpoint, who_e);
	}

	return;
  }
}

/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
static void reply(message *m_out, endpoint_t whom, int result)
{
/* Send a reply to a user process.  If the send fails, just ignore it. */
  int r;

  m_out->m_type = result;
  r = ipc_sendnb(whom, m_out);
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
  message m_out;

  memset(&m_out, 0, sizeof(m_out));

  reply(&m_out, whom, result);
}

/*===========================================================================*
 *				service_pm_postponed			     *
 *===========================================================================*/
void service_pm_postponed(void)
{
  int r, term_signal;
  vir_bytes core_path;
  vir_bytes exec_path, stack_frame, pc, newsp, ps_str;
  size_t exec_path_len, stack_frame_len;
  endpoint_t proc_e;
  message m_out;

  memset(&m_out, 0, sizeof(m_out));

  switch(job_call_nr) {
  case VFS_PM_EXEC:
	proc_e = job_m_in.VFS_PM_ENDPT;
	exec_path = (vir_bytes) job_m_in.VFS_PM_PATH;
	exec_path_len = (size_t) job_m_in.VFS_PM_PATH_LEN;
	stack_frame = (vir_bytes) job_m_in.VFS_PM_FRAME;
	stack_frame_len = (size_t) job_m_in.VFS_PM_FRAME_LEN;
	ps_str = (vir_bytes) job_m_in.VFS_PM_PS_STR;

	assert(proc_e == fp->fp_endpoint);

	r = pm_exec(exec_path, exec_path_len, stack_frame, stack_frame_len,
		&pc, &newsp, &ps_str);

	/* Reply status to PM */
	m_out.m_type = VFS_PM_EXEC_REPLY;
	m_out.VFS_PM_ENDPT = proc_e;
	m_out.VFS_PM_PC = (void *) pc;
	m_out.VFS_PM_STATUS = r;
	m_out.VFS_PM_NEWSP = (void *) newsp;
	m_out.VFS_PM_NEWPS_STR = ps_str;

	break;

  case VFS_PM_EXIT:
	proc_e = job_m_in.VFS_PM_ENDPT;

	assert(proc_e == fp->fp_endpoint);

	pm_exit();

	/* Reply dummy status to PM for synchronization */
	m_out.m_type = VFS_PM_EXIT_REPLY;
	m_out.VFS_PM_ENDPT = proc_e;

	break;

  case VFS_PM_DUMPCORE:
	proc_e = job_m_in.VFS_PM_ENDPT;
	term_signal = job_m_in.VFS_PM_TERM_SIG;
	core_path = (vir_bytes) job_m_in.VFS_PM_PATH;

	assert(proc_e == fp->fp_endpoint);

	r = pm_dumpcore(term_signal, core_path);

	/* Reply status to PM */
	m_out.m_type = VFS_PM_CORE_REPLY;
	m_out.VFS_PM_ENDPT = proc_e;
	m_out.VFS_PM_STATUS = r;

	break;

  case VFS_PM_UNPAUSE:
	proc_e = job_m_in.VFS_PM_ENDPT;

	assert(proc_e == fp->fp_endpoint);

	unpause();

	m_out.m_type = VFS_PM_UNPAUSE_REPLY;
	m_out.VFS_PM_ENDPT = proc_e;

	break;

  default:
	panic("Unhandled postponed PM call %d", job_m_in.m_type);
  }

  r = ipc_send(PM_PROC_NR, &m_out);
  if (r != OK)
	panic("service_pm_postponed: ipc_send failed: %d", r);
}

/*===========================================================================*
 *				service_pm				     *
 *===========================================================================*/
static void service_pm(void)
{
/* Process a request from PM. This function is called from the main thread, and
 * may therefore not block. Any requests that may require blocking the calling
 * thread must be executed in a separate thread. Aside from VFS_PM_REBOOT, all
 * requests from PM involve another, target process: for example, PM tells VFS
 * that a process is performing a setuid() call. For some requests however,
 * that other process may not be idle, and in that case VFS must serialize the
 * PM request handling with any operation is it handling for that target
 * process. As it happens, the requests that may require blocking are also the
 * ones where the target process may not be idle. For both these reasons, such
 * requests are run in worker threads associated to the target process.
 */
  struct fproc *rfp;
  int r, slot;
  message m_out;

  memset(&m_out, 0, sizeof(m_out));

  switch (call_nr) {
  case VFS_PM_SETUID:
	{
		endpoint_t proc_e;
		uid_t euid, ruid;

		proc_e = m_in.VFS_PM_ENDPT;
		euid = m_in.VFS_PM_EID;
		ruid = m_in.VFS_PM_RID;

		pm_setuid(proc_e, euid, ruid);

		m_out.m_type = VFS_PM_SETUID_REPLY;
		m_out.VFS_PM_ENDPT = proc_e;
	}
	break;

  case VFS_PM_SETGID:
	{
		endpoint_t proc_e;
		gid_t egid, rgid;

		proc_e = m_in.VFS_PM_ENDPT;
		egid = m_in.VFS_PM_EID;
		rgid = m_in.VFS_PM_RID;

		pm_setgid(proc_e, egid, rgid);

		m_out.m_type = VFS_PM_SETGID_REPLY;
		m_out.VFS_PM_ENDPT = proc_e;
	}
	break;

  case VFS_PM_SETSID:
	{
		endpoint_t proc_e;

		proc_e = m_in.VFS_PM_ENDPT;
		pm_setsid(proc_e);

		m_out.m_type = VFS_PM_SETSID_REPLY;
		m_out.VFS_PM_ENDPT = proc_e;
	}
	break;

  case VFS_PM_EXEC:
  case VFS_PM_EXIT:
  case VFS_PM_DUMPCORE:
  case VFS_PM_UNPAUSE:
	{
		endpoint_t proc_e = m_in.VFS_PM_ENDPT;

		if(isokendpt(proc_e, &slot) != OK) {
			printf("VFS: proc ep %d not ok\n", proc_e);
			return;
		}

		rfp = &fproc[slot];

		/* PM requests on behalf of a proc are handled after the
		 * system call that might be in progress for that proc has
		 * finished. If the proc is not busy, we start a new thread.
		 */
		worker_start(rfp, NULL, &m_in, FALSE /*use_spare*/);

		return;
	}
  case VFS_PM_FORK:
  case VFS_PM_SRV_FORK:
	{
		endpoint_t pproc_e, proc_e;
		pid_t child_pid;
		uid_t reuid;
		gid_t regid;

		pproc_e = m_in.VFS_PM_PENDPT;
		proc_e = m_in.VFS_PM_ENDPT;
		child_pid = m_in.VFS_PM_CPID;
		reuid = m_in.VFS_PM_REUID;
		regid = m_in.VFS_PM_REGID;

		pm_fork(pproc_e, proc_e, child_pid);
		m_out.m_type = VFS_PM_FORK_REPLY;

		if (call_nr == VFS_PM_SRV_FORK) {
			m_out.m_type = VFS_PM_SRV_FORK_REPLY;
			pm_setuid(proc_e, reuid, reuid);
			pm_setgid(proc_e, regid, regid);
		}

		m_out.VFS_PM_ENDPT = proc_e;
	}
	break;
  case VFS_PM_SETGROUPS:
	{
		endpoint_t proc_e;
		int group_no;
		gid_t *group_addr;

		proc_e = m_in.VFS_PM_ENDPT;
		group_no = m_in.VFS_PM_GROUP_NO;
		group_addr = (gid_t *) m_in.VFS_PM_GROUP_ADDR;

		pm_setgroups(proc_e, group_no, group_addr);

		m_out.m_type = VFS_PM_SETGROUPS_REPLY;
		m_out.VFS_PM_ENDPT = proc_e;
	}
	break;

  case VFS_PM_REBOOT:
	/* Reboot requests are not considered postponed PM work and are instead
	 * handled from a separate worker thread that is associated with PM's
	 * process. PM makes no regular VFS calls, and thus, from VFS's
	 * perspective, PM is always idle. Therefore, we can safely do this.
	 * We do assume that PM sends us only one VFS_PM_REBOOT message at
	 * once, or ever for that matter. :)
	 */
	worker_start(fproc_addr(PM_PROC_NR), pm_reboot, &m_in,
		FALSE /*use_spare*/);

	return;

    default:
	printf("VFS: don't know how to handle PM request %d\n", call_nr);

	return;
  }

  r = ipc_send(PM_PROC_NR, &m_out);
  if (r != OK)
	panic("service_pm: ipc_send failed: %d", r);
}


/*===========================================================================*
 *				unblock					     *
 *===========================================================================*/
static int unblock(rfp)
struct fproc *rfp;
{
/* Unblock a process that was previously blocked on a pipe or a lock.  This is
 * done by reconstructing the original request and continuing/repeating it.
 * This function returns TRUE when it has restored a request for execution, and
 * FALSE if the caller should continue looking for work to do.
 */
  int blocked_on;

  blocked_on = rfp->fp_blocked_on;

  /* Reconstruct the original request from the saved data. */
  memset(&m_in, 0, sizeof(m_in));
  m_in.m_source = rfp->fp_endpoint;
  m_in.m_type = rfp->fp_block_callnr;
  switch (m_in.m_type) {
  case VFS_READ:
  case VFS_WRITE:
	assert(blocked_on == FP_BLOCKED_ON_PIPE);
	m_in.m_lc_vfs_readwrite.fd = scratch(rfp).file.fd_nr;
	m_in.m_lc_vfs_readwrite.buf = scratch(rfp).io.io_buffer;
	m_in.m_lc_vfs_readwrite.len = scratch(rfp).io.io_nbytes;
	break;
  case VFS_FCNTL:
	assert(blocked_on == FP_BLOCKED_ON_LOCK);
	m_in.m_lc_vfs_fcntl.fd = scratch(rfp).file.fd_nr;
	m_in.m_lc_vfs_fcntl.cmd = scratch(rfp).io.io_nbytes;
	m_in.m_lc_vfs_fcntl.arg_ptr = scratch(rfp).io.io_buffer;
	assert(m_in.m_lc_vfs_fcntl.cmd == F_SETLKW);
	break;
  default:
	panic("unblocking call %d blocked on %d ??", m_in.m_type, blocked_on);
  }

  rfp->fp_blocked_on = FP_BLOCKED_ON_NONE;	/* no longer blocked */
  rfp->fp_flags &= ~FP_REVIVED;
  reviving--;
  assert(reviving >= 0);

  /* This should not be device I/O. If it is, it'll 'leak' grants. */
  assert(!GRANT_VALID(rfp->fp_grant));

  /* Pending pipe reads/writes cannot be repeated as is, and thus require a
   * special resumption procedure.
   */
  if (blocked_on == FP_BLOCKED_ON_PIPE) {
	worker_start(rfp, do_pending_pipe, &m_in, FALSE /*use_spare*/);
	return(FALSE);	/* Retrieve more work */
  }

  /* A lock request. Repeat the original request as though it just came in. */
  fp = rfp;
  return(TRUE);	/* We've unblocked a process */
}
