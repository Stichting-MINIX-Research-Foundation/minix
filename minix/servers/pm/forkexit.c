/* This file deals with creating processes (via FORK) and deleting them (via
 * EXIT/WAIT4).  When a process forks, a new slot in the 'mproc' table is
 * allocated for it, and a copy of the parent's core image is made for the
 * child.  Then the kernel and file system are informed.  A process is removed
 * from the 'mproc' table when two events have occurred: (1) it has exited or
 * been killed by a signal, and (2) the parent has done a WAIT4.  If the
 * process exits first, it continues to occupy a slot until the parent does a
 * WAIT4.
 *
 * The entry points into this file are:
 *   do_fork:		perform the FORK system call
 *   do_srv_fork:	special FORK, used by RS to create sys services
 *   do_exit:		perform the EXIT system call (by calling exit_proc())
 *   exit_proc:		actually do the exiting, and tell VFS about it
 *   exit_restart:	continue exiting a process after VFS has replied
 *   do_wait4:		perform the WAIT4 system call
 *   wait_test:		check whether a parent is waiting for a child
 */

#include "pm.h"
#include <sys/wait.h>
#include <assert.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/sched.h>
#include <minix/vm.h>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <signal.h>
#include "mproc.h"

#define LAST_FEW            2	/* last few slots reserved for superuser */

static void zombify(struct mproc *rmp);
static void check_parent(struct mproc *child, int try_cleanup);
static int tell_parent(struct mproc *child, vir_bytes addr);
static void tell_tracer(struct mproc *child);
static void tracer_died(struct mproc *child);
static void cleanup(register struct mproc *rmp);

/*===========================================================================*
 *				do_fork					     *
 *===========================================================================*/
int
do_fork(void)
{
/* The process pointed to by 'mp' has forked.  Create a child process. */
  register struct mproc *rmp;	/* pointer to parent */
  register struct mproc *rmc;	/* pointer to child */
  pid_t new_pid;
  static unsigned int next_child = 0;
  int i, n = 0, s;
  endpoint_t child_ep;
  message m;

 /* If tables might fill up during FORK, don't even start since recovery half
  * way through is such a nuisance.
  */
  rmp = mp;
  if ((procs_in_use == NR_PROCS) ||
  		(procs_in_use >= NR_PROCS-LAST_FEW && rmp->mp_effuid != 0))
  {
  	printf("PM: warning, process table is full!\n");
  	return(EAGAIN);
  }

  /* Find a slot in 'mproc' for the child process.  A slot must exist. */
  do {
        next_child = (next_child+1) % NR_PROCS;
	n++;
  } while((mproc[next_child].mp_flags & IN_USE) && n <= NR_PROCS);
  if(n > NR_PROCS)
	panic("do_fork can't find child slot");
  if(next_child >= NR_PROCS || (mproc[next_child].mp_flags & IN_USE))
	panic("do_fork finds wrong child slot: %d", next_child);

  /* Memory part of the forking. */
  if((s=vm_fork(rmp->mp_endpoint, next_child, &child_ep)) != OK) {
	return s;
  }

  /* PM may not fail fork after call to vm_fork(), as VM calls sys_fork(). */

  rmc = &mproc[next_child];
  /* Set up the child and its memory map; copy its 'mproc' slot from parent. */
  procs_in_use++;
  *rmc = *rmp;			/* copy parent's process slot to child's */
  rmc->mp_sigact = mpsigact[next_child];	/* restore mp_sigact ptr */
  memcpy(rmc->mp_sigact, rmp->mp_sigact, sizeof(mpsigact[next_child]));
  rmc->mp_parent = who_p;			/* record child's parent */
  if (!(rmc->mp_trace_flags & TO_TRACEFORK)) {
	rmc->mp_tracer = NO_TRACER;		/* no tracer attached */
	rmc->mp_trace_flags = 0;
	(void) sigemptyset(&rmc->mp_sigtrace);
  }

  /* Some system servers like to call regular fork, such as RS spawning
   * recovery scripts; in this case PM will take care of their scheduling
   * because RS cannot do so for non-system processes */
  if (rmc->mp_flags & PRIV_PROC) {
	assert(rmc->mp_scheduler == NONE);
	rmc->mp_scheduler = SCHED_PROC_NR;
  }

  /* Inherit only these flags. In normal fork(), PRIV_PROC is not inherited. */
  rmc->mp_flags &= (IN_USE|DELAY_CALL|TAINTED);
  rmc->mp_child_utime = 0;		/* reset administration */
  rmc->mp_child_stime = 0;		/* reset administration */
  rmc->mp_exitstatus = 0;
  rmc->mp_sigstatus = 0;
  rmc->mp_endpoint = child_ep;		/* passed back by VM */
  for (i = 0; i < NR_ITIMERS; i++)
	rmc->mp_interval[i] = 0;	/* reset timer intervals */
  rmc->mp_started = getticks();		/* remember start time, for ps(1) */

  assert(rmc->mp_eventsub == NO_EVENTSUB);

  /* Find a free pid for the child and put it in the table. */
  new_pid = get_free_pid();
  rmc->mp_pid = new_pid;	/* assign pid to child */

  memset(&m, 0, sizeof(m));
  m.m_type = VFS_PM_FORK;
  m.VFS_PM_ENDPT = rmc->mp_endpoint;
  m.VFS_PM_PENDPT = rmp->mp_endpoint;
  m.VFS_PM_CPID = rmc->mp_pid;
  m.VFS_PM_REUID = -1;	/* Not used by VFS_PM_FORK */
  m.VFS_PM_REGID = -1;	/* Not used by VFS_PM_FORK */

  tell_vfs(rmc, &m);

  /* Tell the tracer, if any, about the new child */
  if (rmc->mp_tracer != NO_TRACER)
	sig_proc(rmc, SIGSTOP, TRUE /*trace*/, FALSE /* ksig */);

  /* Do not reply until VFS is ready to process the fork
  * request
  */
  return SUSPEND;
}

/*===========================================================================*
 *				do_srv_fork				     *
 *===========================================================================*/
int
do_srv_fork(void)
{
/* The process pointed to by 'mp' has forked.  Create a child process. */
  register struct mproc *rmp;	/* pointer to parent */
  register struct mproc *rmc;	/* pointer to child */
  int s;
  pid_t new_pid;
  static unsigned int next_child = 0;
  int i, n = 0;
  endpoint_t child_ep;
  message m;

  /* Only RS is allowed to use srv_fork. */
  if (mp->mp_endpoint != RS_PROC_NR)
	return EPERM;

 /* If tables might fill up during FORK, don't even start since recovery half
  * way through is such a nuisance.
  */
  rmp = mp;
  if ((procs_in_use == NR_PROCS) ||
  		(procs_in_use >= NR_PROCS-LAST_FEW && rmp->mp_effuid != 0))
  {
  	printf("PM: warning, process table is full!\n");
  	return(EAGAIN);
  }

  /* Find a slot in 'mproc' for the child process.  A slot must exist. */
  do {
        next_child = (next_child+1) % NR_PROCS;
	n++;
  } while((mproc[next_child].mp_flags & IN_USE) && n <= NR_PROCS);
  if(n > NR_PROCS)
	panic("do_fork can't find child slot");
  if(next_child >= NR_PROCS || (mproc[next_child].mp_flags & IN_USE))
	panic("do_fork finds wrong child slot: %d", next_child);

  if((s=vm_fork(rmp->mp_endpoint, next_child, &child_ep)) != OK) {
	return s;
  }

  rmc = &mproc[next_child];
  /* Set up the child and its memory map; copy its 'mproc' slot from parent. */
  procs_in_use++;
  *rmc = *rmp;			/* copy parent's process slot to child's */
  rmc->mp_sigact = mpsigact[next_child];	/* restore mp_sigact ptr */
  memcpy(rmc->mp_sigact, rmp->mp_sigact, sizeof(mpsigact[next_child]));
  rmc->mp_parent = who_p;			/* record child's parent */
  if (!(rmc->mp_trace_flags & TO_TRACEFORK)) {
	rmc->mp_tracer = NO_TRACER;		/* no tracer attached */
	rmc->mp_trace_flags = 0;
	(void) sigemptyset(&rmc->mp_sigtrace);
  }
  /* inherit only these flags */
  rmc->mp_flags &= (IN_USE|PRIV_PROC|DELAY_CALL);
  rmc->mp_child_utime = 0;		/* reset administration */
  rmc->mp_child_stime = 0;		/* reset administration */
  rmc->mp_exitstatus = 0;
  rmc->mp_sigstatus = 0;
  rmc->mp_endpoint = child_ep;		/* passed back by VM */
  rmc->mp_realuid = m_in.m_lsys_pm_srv_fork.uid;
  rmc->mp_effuid = m_in.m_lsys_pm_srv_fork.uid;
  rmc->mp_svuid = m_in.m_lsys_pm_srv_fork.uid;
  rmc->mp_realgid = m_in.m_lsys_pm_srv_fork.gid;
  rmc->mp_effgid = m_in.m_lsys_pm_srv_fork.gid;
  rmc->mp_svgid = m_in.m_lsys_pm_srv_fork.gid;
  for (i = 0; i < NR_ITIMERS; i++)
	rmc->mp_interval[i] = 0;	/* reset timer intervals */
  rmc->mp_started = getticks();		/* remember start time, for ps(1) */

  assert(rmc->mp_eventsub == NO_EVENTSUB);

  /* Find a free pid for the child and put it in the table. */
  new_pid = get_free_pid();
  rmc->mp_pid = new_pid;	/* assign pid to child */

  memset(&m, 0, sizeof(m));
  m.m_type = VFS_PM_SRV_FORK;
  m.VFS_PM_ENDPT = rmc->mp_endpoint;
  m.VFS_PM_PENDPT = rmp->mp_endpoint;
  m.VFS_PM_CPID = rmc->mp_pid;
  m.VFS_PM_REUID = m_in.m_lsys_pm_srv_fork.uid;
  m.VFS_PM_REGID = m_in.m_lsys_pm_srv_fork.gid;

  tell_vfs(rmc, &m);

  /* Tell the tracer, if any, about the new child */
  if (rmc->mp_tracer != NO_TRACER)
	sig_proc(rmc, SIGSTOP, TRUE /*trace*/, FALSE /* ksig */);

  /* Wakeup the newly created process */
  reply(rmc-mproc, OK);

  return rmc->mp_pid;
}

/*===========================================================================*
 *				do_exit					     *
 *===========================================================================*/
int
do_exit(void)
{
 /* Perform the exit(status) system call. The real work is done by exit_proc(),
  * which is also called when a process is killed by a signal. System processes
  * do not use PM's exit() to terminate. If they try to, we warn the user
  * and send a SIGKILL signal to the system process.
  */
  if(mp->mp_flags & PRIV_PROC) {
      printf("PM: system process %d (%s) tries to exit(), sending SIGKILL\n",
          mp->mp_endpoint, mp->mp_name);
      sys_kill(mp->mp_endpoint, SIGKILL);
  }
  else {
      exit_proc(mp, m_in.m_lc_pm_exit.status, FALSE /*dump_core*/);
  }
  return(SUSPEND);		/* can't communicate from beyond the grave */
}

/*===========================================================================*
 *				exit_proc				     *
 *===========================================================================*/
void
exit_proc(
	register struct mproc *rmp,	/* pointer to the process to be terminated */
	int exit_status,		/* the process' exit status (for parent) */
	int dump_core			/* flag indicating whether to dump core */
)
{
/* A process is done.  Release most of the process' possessions.  If its
 * parent is waiting, release the rest, else keep the process slot and
 * become a zombie.
 */
  register int proc_nr, proc_nr_e;
  int r;
  pid_t procgrp;
  clock_t user_time, sys_time;
  message m;

  /* Do not create core files for set uid execution */
  if (dump_core && rmp->mp_realuid != rmp->mp_effuid)
	dump_core = FALSE;

  /* System processes are destroyed before informing VFS, meaning that VFS can
   * not get their CPU state, so we can't generate a coredump for them either.
   */
  if (dump_core && (rmp->mp_flags & PRIV_PROC))
	dump_core = FALSE;

  proc_nr = (int) (rmp - mproc);	/* get process slot number */
  proc_nr_e = rmp->mp_endpoint;

  /* Remember a session leader's process group. */
  procgrp = (rmp->mp_pid == mp->mp_procgrp) ? mp->mp_procgrp : 0;

  /* If the exited process has a timer pending, kill it. */
  if (rmp->mp_flags & ALARM_ON) set_alarm(rmp, (clock_t) 0);

  /* Do accounting: fetch usage times and save with dead child process.
   * POSIX forbids accumulation at parent until child has been waited for.
   */
  if((r=sys_times(proc_nr_e, &user_time, &sys_time, NULL, NULL)) != OK)
  	panic("exit_proc: sys_times failed: %d", r);
  rmp->mp_child_utime += user_time;		/* add user time */
  rmp->mp_child_stime += sys_time;		/* add system time */

  /* Tell the kernel the process is no longer runnable to prevent it from
   * being scheduled in between the following steps. Then tell VFS that it
   * the process has exited and finally, clean up the process at the kernel.
   * This order is important so that VFS can tell drivers to cancel requests
   * such as copying to/ from the exiting process, before it is gone.
   */
  /* If the process is not yet stopped, we force a stop here. This means that
   * the process may still have a delay call pending. For this reason, the main
   * message loop discards requests from exiting processes.
   *
   * TODO: make the kernel discard delayed calls upon forced stops for exits,
   * so that no service needs to deal with this.  Right now it appears that the
   * only thing preventing problems with other services is the fact that
   * regular messages are prioritized over asynchronous messages.
   */
  if (!(rmp->mp_flags & PROC_STOPPED)) {
	if ((r = sys_stop(proc_nr_e)) != OK)		/* stop the process */
		panic("sys_stop failed: %d", r);
	rmp->mp_flags |= PROC_STOPPED;
  }

  if((r=vm_willexit(proc_nr_e)) != OK) {
	panic("exit_proc: vm_willexit failed: %d", r);
  }

  if (proc_nr_e == INIT_PROC_NR)
  {
	printf("PM: INIT died with exit status %d; showing stacktrace\n", exit_status);
	sys_diagctl_stacktrace(proc_nr_e);
	return;
  }
  if (proc_nr_e == VFS_PROC_NR)
  {
	panic("exit_proc: VFS died: %d", r);
  }

  /* Tell VFS, and after that any matching process event subscribers, about the
   * exiting process.
   */
  memset(&m, 0, sizeof(m));
  m.m_type = dump_core ? VFS_PM_DUMPCORE : VFS_PM_EXIT;
  m.VFS_PM_ENDPT = rmp->mp_endpoint;

  if (dump_core) {
	m.VFS_PM_TERM_SIG = rmp->mp_sigstatus;
	m.VFS_PM_PATH = rmp->mp_name;
  }

  tell_vfs(rmp, &m);

  if (rmp->mp_flags & PRIV_PROC)
  {
	/* Destroy system processes without waiting for VFS. This is
	 * needed because the system process might be a block device
	 * driver that VFS is blocked waiting on.
	 */
	if((r= sys_clear(rmp->mp_endpoint)) != OK)
		panic("exit_proc: sys_clear failed: %d", r);
  }

  /* Clean up most of the flags describing the process's state before the exit,
   * and mark it as exiting.
   */
  rmp->mp_flags &= (IN_USE|VFS_CALL|PRIV_PROC|TRACE_EXIT|PROC_STOPPED);
  rmp->mp_flags |= EXITING;

  /* Keep the process around until VFS is finished with it. */

  rmp->mp_exitstatus = (char) exit_status;

  /* For normal exits, try to notify the parent as soon as possible.
   * For core dumps, notify the parent only once the core dump has been made.
   */
  if (!dump_core)
	zombify(rmp);

  /* If the process has children, disinherit them.  INIT is the new parent. */
  for (rmp = &mproc[0]; rmp < &mproc[NR_PROCS]; rmp++) {
	if (!(rmp->mp_flags & IN_USE)) continue;
	if (rmp->mp_tracer == proc_nr) {
		/* This child's tracer died. Do something sensible. */
		tracer_died(rmp);
	}
	if (rmp->mp_parent == proc_nr) {
		/* 'rmp' now points to a child to be disinherited. */
		rmp->mp_parent = INIT_PROC_NR;

		/* If the process is making a VFS call, remember that we set
		 * a new parent. This prevents FORK from replying to the wrong
		 * parent upon completion.
		 */
		if (rmp->mp_flags & VFS_CALL)
			rmp->mp_flags |= NEW_PARENT;

		/* Notify new parent. */
		if (rmp->mp_flags & ZOMBIE)
			check_parent(rmp, TRUE /*try_cleanup*/);
	}
  }

  /* Send a hangup to the process' process group if it was a session leader. */
  if (procgrp != 0) check_sig(-procgrp, SIGHUP, FALSE /* ksig */);
}

/*===========================================================================*
 *				exit_restart				     *
 *===========================================================================*/
void exit_restart(struct mproc *rmp)
{
/* VFS replied to our exit or coredump request. Perform the second half of the
 * exit code.
 */
  int r;

  if((r = sched_stop(rmp->mp_scheduler, rmp->mp_endpoint)) != OK) {
 	/* If the scheduler refuses to give up scheduling, there is
	 * little we can do, except report it. This may cause problems
	 * later on, if this scheduler is asked to schedule another proc
	 * that has an endpoint->schedproc mapping identical to the proc
	 * we just tried to stop scheduling.
	*/
	printf("PM: The scheduler did not want to give up "
		"scheduling %s, ret=%d.\n", rmp->mp_name, r);
  }

  /* sched_stop is either called when the process is exiting or it is
   * being moved between schedulers. If it is being moved between
   * schedulers, we need to set the mp_scheduler to NONE so that PM
   * doesn't forward messages to the process' scheduler while being moved
   * (such as sched_nice). */
  rmp->mp_scheduler = NONE;

  /* For core dumps, now is the right time to try to contact the parent. */
  if (!(rmp->mp_flags & (TRACE_ZOMBIE | ZOMBIE | TOLD_PARENT)))
	zombify(rmp);

  if (!(rmp->mp_flags & PRIV_PROC))
  {
	/* destroy the (user) process */
	if((r=sys_clear(rmp->mp_endpoint)) != OK)
		panic("exit_restart: sys_clear failed: %d", r);
  }

  /* Release the memory occupied by the child. */
  if((r=vm_exit(rmp->mp_endpoint)) != OK) {
  	panic("exit_restart: vm_exit failed: %d", r);
  }

  if (rmp->mp_flags & TRACE_EXIT)
  {
	/* Wake up the tracer, completing the ptrace(T_EXIT) call */
	mproc[rmp->mp_tracer].mp_reply.m_pm_lc_ptrace.data = 0;
	reply(rmp->mp_tracer, OK);
  }

  /* Clean up if the parent has collected the exit status */
  if (rmp->mp_flags & TOLD_PARENT)
	cleanup(rmp);
}

/*===========================================================================*
 *				do_wait4				     *
 *===========================================================================*/
int
do_wait4(void)
{
/* A process wants to wait for a child to terminate. If a child is already
 * waiting, go clean it up and let this WAIT4 call terminate.  Otherwise,
 * really wait.
 * A process calling WAIT4 never gets a reply in the usual way at the end
 * of the main loop (unless WNOHANG is set or no qualifying child exists).
 * If a child has already exited, the routine tell_parent() sends the reply
 * to awaken the caller.
 */
  register struct mproc *rp;
  vir_bytes addr;
  int i, pidarg, options, children, waited_for;

  /* Set internal variables. */
  pidarg  = m_in.m_lc_pm_wait4.pid;		/* 1st param */
  options = m_in.m_lc_pm_wait4.options;		/* 3rd param */
  addr    = m_in.m_lc_pm_wait4.addr;		/* 4th param */
  if (pidarg == 0) pidarg = -mp->mp_procgrp;	/* pidarg < 0 ==> proc grp */

  /* Is there a child waiting to be collected? At this point, pidarg != 0:
   *	pidarg  >  0 means pidarg is pid of a specific process to wait for
   *	pidarg == -1 means wait for any child
   *	pidarg  < -1 means wait for any child whose process group = -pidarg
   */
  children = 0;
  for (rp = &mproc[0]; rp < &mproc[NR_PROCS]; rp++) {
	if ((rp->mp_flags & (IN_USE | TOLD_PARENT)) != IN_USE) continue;
	if (rp->mp_parent != who_p && rp->mp_tracer != who_p) continue;
	if (rp->mp_parent != who_p && (rp->mp_flags & ZOMBIE)) continue;

	/* The value of pidarg determines which children qualify. */
	if (pidarg  > 0 && pidarg != rp->mp_pid) continue;
	if (pidarg < -1 && -pidarg != rp->mp_procgrp) continue;

	children++;			/* this child is acceptable */

	if (rp->mp_tracer == who_p) {
		if (rp->mp_flags & TRACE_ZOMBIE) {
			/* Traced child meets the pid test and has exited. */
			tell_tracer(rp);
			check_parent(rp, TRUE /*try_cleanup*/);
			return(SUSPEND);
		}
		if (rp->mp_flags & TRACE_STOPPED) {
			/* This child meets the pid test and is being traced.
			 * Deliver a signal to the tracer, if any.
			 */
			for (i = 1; i < _NSIG; i++) {
				if (sigismember(&rp->mp_sigtrace, i)) {
					/* TODO: rusage support */

					sigdelset(&rp->mp_sigtrace, i);

					mp->mp_reply.m_pm_lc_wait4.status =
					    W_STOPCODE(i);
					return(rp->mp_pid);
				}
			}
		}
	}

	if (rp->mp_parent == who_p) {
		if (rp->mp_flags & ZOMBIE) {
			/* This child meets the pid test and has exited. */
			waited_for = tell_parent(rp, addr);

			if (waited_for &&
			    !(rp->mp_flags & (VFS_CALL | EVENT_CALL)))
				cleanup(rp);
			return(SUSPEND);
		}
	}
  }

  /* No qualifying child has exited.  Wait for one, unless none exists. */
  if (children > 0) {
	/* At least 1 child meets the pid test exists, but has not exited. */
	if (options & WNOHANG) {
		return(0);    /* parent does not want to wait */
	}
	mp->mp_flags |= WAITING;	     /* parent wants to wait */
	mp->mp_wpid = (pid_t) pidarg;	     /* save pid for later */
	mp->mp_waddr = addr;		     /* save rusage addr for later */
	return(SUSPEND);		     /* do not reply, let it wait */
  } else {
	/* No child even meets the pid test.  Return error immediately. */
	return(ECHILD);			     /* no - parent has no children */
  }
}

/*===========================================================================*
 *				wait_test				     *
 *===========================================================================*/
int
wait_test(
	struct mproc *rmp,			/* process that may be waiting */
	struct mproc *child			/* process that may be waited for */
)
{
/* See if a parent or tracer process is waiting for a child process.
 * A tracer is considered to be a pseudo-parent.
 */
  int parent_waiting, right_child;
  pid_t pidarg;

  pidarg = rmp->mp_wpid;		/* who's being waited for? */
  parent_waiting = rmp->mp_flags & WAITING;
  right_child =				/* child meets one of the 3 tests? */
  	(pidarg == -1 || pidarg == child->mp_pid ||
  	 -pidarg == child->mp_procgrp);

  return (parent_waiting && right_child);
}

/*===========================================================================*
 *				zombify					     *
 *===========================================================================*/
static void
zombify(struct mproc *rmp)
{
/* Zombify a process. First check if the exiting process is traced by a process
 * other than its parent; if so, the tracer must be notified about the exit
 * first. Once that is done, the real parent may be notified about the exit of
 * its child.
 */
  struct mproc *t_mp;

  if (rmp->mp_flags & (TRACE_ZOMBIE | ZOMBIE))
	panic("zombify: process was already a zombie");

  /* See if we have to notify a tracer process first. */
  if (rmp->mp_tracer != NO_TRACER && rmp->mp_tracer != rmp->mp_parent) {
	rmp->mp_flags |= TRACE_ZOMBIE;

	t_mp = &mproc[rmp->mp_tracer];

	/* Do not bother sending SIGCHLD signals to tracers. */
	if (!wait_test(t_mp, rmp))
		return;

	tell_tracer(rmp);
  }
  else {
	rmp->mp_flags |= ZOMBIE;
  }

  /* No tracer, or tracer is parent, or tracer has now been notified. */
  check_parent(rmp, FALSE /*try_cleanup*/);
}

/*===========================================================================*
 *				check_parent				     *
 *===========================================================================*/
static void
check_parent(
	struct mproc *child,			/* tells which process is exiting */
	int try_cleanup			/* clean up the child when done? */
)
{
/* We would like to inform the parent of an exiting child about the child's
 * death. If the parent is waiting for the child, tell it immediately;
 * otherwise, send it a SIGCHLD signal.
 *
 * Note that we may call this function twice on a single child; first with
 * its original parent, later (if the parent died) with INIT as its parent.
 */
  struct mproc *p_mp;

  p_mp = &mproc[child->mp_parent];

  if (p_mp->mp_flags & EXITING) {
	/* This may trigger if the child of a dead parent dies. The child will
	 * be assigned to INIT and rechecked shortly after. Do nothing.
	 */
  }
  else if (wait_test(p_mp, child)) {
	if (!tell_parent(child, p_mp->mp_waddr))
		try_cleanup = FALSE; /* child is still there */

	/* The 'try_cleanup' flag merely saves us from having to be really
	 * careful with statement ordering in exit_proc() and exit_restart().
	 */
	if (try_cleanup && !(child->mp_flags & (VFS_CALL | EVENT_CALL)))
		cleanup(child);
  }
  else {
	/* Parent is not waiting. */
	sig_proc(p_mp, SIGCHLD, TRUE /*trace*/, FALSE /* ksig */);
  }
}

/*===========================================================================*
 *				tell_parent				     *
 *===========================================================================*/
static int tell_parent(struct mproc *child, vir_bytes addr)
{
/* Tell the parent of the given process that it has terminated, by satisfying
 * the parent's ongoing wait4() call.  If the parent has requested the child
 * tree's resource usage, copy that information out first.  The copy may fail;
 * in that case, the parent's wait4() call will return with an error, but the
 * child will remain a zombie.  Return TRUE if the child is cleaned up, or
 * FALSE if the child is still a zombie.
 */
  struct rusage r_usage;
  int mp_parent;
  struct mproc *parent;
  int r;

  mp_parent= child->mp_parent;
  if (mp_parent <= 0)
	panic("tell_parent: bad value in mp_parent: %d", mp_parent);
  if(!(child->mp_flags & ZOMBIE))
  	panic("tell_parent: child not a zombie");
  if(child->mp_flags & TOLD_PARENT)
	panic("tell_parent: telling parent again");
  parent = &mproc[mp_parent];

  /* See if we need to report resource usage to the parent. */
  if (addr) {
	/* We report only user and system times for now. TODO: support other
	 * fields, although this is tricky since the child process is already
	 * gone as far as the kernel and other services are concerned..
	 */
	memset(&r_usage, 0, sizeof(r_usage));
	set_rusage_times(&r_usage, child->mp_child_utime,
	    child->mp_child_stime);

	if ((r = sys_datacopy(SELF, (vir_bytes)&r_usage, parent->mp_endpoint,
	    addr, sizeof(r_usage))) != OK) {
		reply(child->mp_parent, r);

		return FALSE; /* copy error - the child is still there */
	}
  }

  /* Wake up the parent by sending the reply message. */
  parent->mp_reply.m_pm_lc_wait4.status =
	W_EXITCODE(child->mp_exitstatus, child->mp_sigstatus);
  reply(child->mp_parent, child->mp_pid);
  parent->mp_flags &= ~WAITING;		/* parent no longer waiting */
  child->mp_flags &= ~ZOMBIE;		/* child no longer a zombie */
  child->mp_flags |= TOLD_PARENT;	/* avoid informing parent twice */

  /* Now that the child has been waited for, accumulate the times of the
   * terminated child process at the parent.
   */
  parent->mp_child_utime += child->mp_child_utime;
  parent->mp_child_stime += child->mp_child_stime;

  return TRUE; /* child has been waited for */
}

/*===========================================================================*
 *				tell_tracer				     *
 *===========================================================================*/
static void
tell_tracer(
	struct mproc *child			/* tells which process is exiting */
)
{
  int mp_tracer;
  struct mproc *tracer;

  mp_tracer = child->mp_tracer;
  if (mp_tracer <= 0)
	panic("tell_tracer: bad value in mp_tracer: %d", mp_tracer);
  if(!(child->mp_flags & TRACE_ZOMBIE))
  	panic("tell_tracer: child not a zombie");
  tracer = &mproc[mp_tracer];

  /* TODO: rusage support */

  tracer->mp_reply.m_pm_lc_wait4.status =
	W_EXITCODE(child->mp_exitstatus, (child->mp_sigstatus & 0377));
  reply(child->mp_tracer, child->mp_pid);
  tracer->mp_flags &= ~WAITING;		/* tracer no longer waiting */
  child->mp_flags &= ~TRACE_ZOMBIE;	/* child no longer zombie to tracer */
  child->mp_flags |= ZOMBIE;		/* child is now zombie to parent */
}

/*===========================================================================*
 *				tracer_died				     *
 *===========================================================================*/
static void
tracer_died(
	struct mproc *child			/* process being traced */
)
{
/* The process that was tracing the given child, has died for some reason.
 * This is really the tracer's fault, but we can't let INIT deal with this.
 */

  child->mp_tracer = NO_TRACER;
  child->mp_flags &= ~TRACE_EXIT;

  /* If the tracer died while the child was running or stopped, we have no
   * idea what state the child is in. Avoid a trainwreck, by killing the child.
   * Note that this may cause cascading exits.
   */
  if (!(child->mp_flags & EXITING)) {
	sig_proc(child, SIGKILL, TRUE /*trace*/, FALSE /* ksig */);

	return;
  }

  /* If the tracer died while the child was telling it about its own death,
   * forget about the tracer and notify the real parent instead.
   */
  if (child->mp_flags & TRACE_ZOMBIE) {
	child->mp_flags &= ~TRACE_ZOMBIE;
	child->mp_flags |= ZOMBIE;

	check_parent(child, TRUE /*try_cleanup*/);
  }
}

/*===========================================================================*
 *				cleanup					     *
 *===========================================================================*/
static void
cleanup(
	register struct mproc *rmp	/* tells which process is exiting */
)
{
  /* Release the process table entry and reinitialize some field. */
  rmp->mp_pid = 0;
  rmp->mp_flags = 0;
  rmp->mp_child_utime = 0;
  rmp->mp_child_stime = 0;
  procs_in_use--;
}

