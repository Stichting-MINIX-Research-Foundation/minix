/* This file deals with creating processes (via FORK) and deleting them (via
 * EXIT/WAIT).  When a process forks, a new slot in the 'mproc' table is
 * allocated for it, and a copy of the parent's core image is made for the
 * child.  Then the kernel and file system are informed.  A process is removed
 * from the 'mproc' table when two events have occurred: (1) it has exited or
 * been killed by a signal, and (2) the parent has done a WAIT.  If the process
 * exits first, it continues to occupy a slot until the parent does a WAIT.
 *
 * The entry points into this file are:
 *   do_fork:		perform the FORK system call
 *   do_fork_nb:	special nonblocking version of FORK, for RS
 *   do_exit:		perform the EXIT system call (by calling exit_proc())
 *   exit_proc:		actually do the exiting, and tell FS about it
 *   exit_restart:	continue exiting a process after FS has replied
 *   do_waitpid:	perform the WAITPID or WAIT system call
 *   wait_test:		check whether a parent is waiting for a child
 */

#include "pm.h"
#include <sys/wait.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/vm.h>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <signal.h>
#include "mproc.h"
#include "param.h"

#define LAST_FEW            2	/* last few slots reserved for superuser */

FORWARD _PROTOTYPE (void zombify, (struct mproc *rmp) );
FORWARD _PROTOTYPE (void check_parent, (struct mproc *child,
	int try_cleanup) );
FORWARD _PROTOTYPE (void tell_parent, (struct mproc *child) );
FORWARD _PROTOTYPE (void tell_tracer, (struct mproc *child) );
FORWARD _PROTOTYPE (void tracer_died, (struct mproc *child) );
FORWARD _PROTOTYPE (void cleanup, (register struct mproc *rmp) );

/*===========================================================================*
 *				do_fork					     *
 *===========================================================================*/
PUBLIC int do_fork()
{
/* The process pointed to by 'mp' has forked.  Create a child process. */
  register struct mproc *rmp;	/* pointer to parent */
  register struct mproc *rmc;	/* pointer to child */
  pid_t new_pid;
  static int next_child;
  int i, n = 0, r, s;
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
	panic(__FILE__,"do_fork can't find child slot", NO_NUM);
  if(next_child < 0 || next_child >= NR_PROCS
 || (mproc[next_child].mp_flags & IN_USE))
	panic(__FILE__,"do_fork finds wrong child slot", next_child);

  /* Memory part of the forking. */
  if((s=vm_fork(rmp->mp_endpoint, next_child, &child_ep)) != OK) {
	printf("PM: vm_fork failed: %d\n", s);
	return s;
  }

  /* PM may not fail fork after call to vm_fork(), as VM calls sys_fork(). */

  rmc = &mproc[next_child];
  /* Set up the child and its memory map; copy its 'mproc' slot from parent. */
  procs_in_use++;
  *rmc = *rmp;			/* copy parent's process slot to child's */
  rmc->mp_parent = who_p;			/* record child's parent */
  if (!(rmc->mp_trace_flags & TO_TRACEFORK)) {
	rmc->mp_tracer = NO_TRACER;		/* no tracer attached */
	rmc->mp_trace_flags = 0;
	sigemptyset(&rmc->mp_sigtrace);
  }
  /* inherit only these flags */
  rmc->mp_flags &= (IN_USE|PRIV_PROC|DELAY_CALL);
  rmc->mp_child_utime = 0;		/* reset administration */
  rmc->mp_child_stime = 0;		/* reset administration */
  rmc->mp_exitstatus = 0;
  rmc->mp_sigstatus = 0;
  rmc->mp_endpoint = child_ep;		/* passed back by VM */
  for (i = 0; i < NR_ITIMERS; i++)
	rmc->mp_interval[i] = 0;	/* reset timer intervals */

  /* Find a free pid for the child and put it in the table. */
  new_pid = get_free_pid();
  rmc->mp_pid = new_pid;	/* assign pid to child */

  m.m_type = PM_FORK;
  m.PM_PROC = rmc->mp_endpoint;
  m.PM_PPROC = rmp->mp_endpoint;
  m.PM_CPID = rmc->mp_pid;

  tell_fs(rmc, &m);

  /* Tell the tracer, if any, about the new child */
  if (rmc->mp_tracer != NO_TRACER)
	sig_proc(rmc, SIGSTOP, TRUE /*trace*/);

  /* Do not reply until FS is ready to process the fork
  * request
  */
  return SUSPEND;
}

/*===========================================================================*
 *				do_fork_nb				     *
 *===========================================================================*/
PUBLIC int do_fork_nb()
{
/* The process pointed to by 'mp' has forked.  Create a child process. */
  register struct mproc *rmp;	/* pointer to parent */
  register struct mproc *rmc;	/* pointer to child */
  int s;
  pid_t new_pid;
  static int next_child;
  int i, n = 0, r;
  endpoint_t child_ep;
  message m;

  /* Only system processes are allowed to use fork_nb */
  if (!(mp->mp_flags & PRIV_PROC))
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
	panic(__FILE__,"do_fork can't find child slot", NO_NUM);
  if(next_child < 0 || next_child >= NR_PROCS
 || (mproc[next_child].mp_flags & IN_USE))
	panic(__FILE__,"do_fork finds wrong child slot", next_child);

  if((s=vm_fork(rmp->mp_endpoint, next_child, &child_ep)) != OK) {
	printf("PM: vm_fork failed: %d\n", s);
	return s;
  }

  rmc = &mproc[next_child];
  /* Set up the child and its memory map; copy its 'mproc' slot from parent. */
  procs_in_use++;
  *rmc = *rmp;			/* copy parent's process slot to child's */
  rmc->mp_parent = who_p;			/* record child's parent */
  if (!(rmc->mp_trace_flags & TO_TRACEFORK)) {
	rmc->mp_tracer = NO_TRACER;		/* no tracer attached */
	rmc->mp_trace_flags = 0;
	sigemptyset(&rmc->mp_sigtrace);
  }
  /* inherit only these flags */
  rmc->mp_flags &= (IN_USE|PRIV_PROC|DELAY_CALL);
  rmc->mp_child_utime = 0;		/* reset administration */
  rmc->mp_child_stime = 0;		/* reset administration */
  rmc->mp_exitstatus = 0;
  rmc->mp_sigstatus = 0;
  rmc->mp_endpoint = child_ep;		/* passed back by VM */
  for (i = 0; i < NR_ITIMERS; i++)
	rmc->mp_interval[i] = 0;	/* reset timer intervals */

  /* Find a free pid for the child and put it in the table. */
  new_pid = get_free_pid();
  rmc->mp_pid = new_pid;	/* assign pid to child */

  m.m_type = PM_FORK_NB;
  m.PM_PROC = rmc->mp_endpoint;
  m.PM_PPROC = rmp->mp_endpoint;
  m.PM_CPID = rmc->mp_pid;

  tell_fs(rmc, &m);

  /* Tell the tracer, if any, about the new child */
  if (rmc->mp_tracer != NO_TRACER)
	sig_proc(rmc, SIGSTOP, TRUE /*trace*/);

  /* Wakeup the newly created process */
  setreply(rmc-mproc, OK);

  return rmc->mp_pid;
}

/*===========================================================================*
 *				do_exit					     *
 *===========================================================================*/
PUBLIC int do_exit()
{
/* Perform the exit(status) system call. The real work is done by exit_proc(),
 * which is also called when a process is killed by a signal.
 */
  exit_proc(mp, m_in.status, FALSE /*dump_core*/);
  return(SUSPEND);		/* can't communicate from beyond the grave */
}

/*===========================================================================*
 *				exit_proc				     *
 *===========================================================================*/
PUBLIC void exit_proc(rmp, exit_status, dump_core)
register struct mproc *rmp;	/* pointer to the process to be terminated */
int exit_status;		/* the process' exit status (for parent) */
int dump_core;			/* flag indicating whether to dump core */
{
/* A process is done.  Release most of the process' possessions.  If its
 * parent is waiting, release the rest, else keep the process slot and
 * become a zombie.
 */
  register int proc_nr, proc_nr_e;
  int parent_waiting, r;
  pid_t procgrp;
  struct mproc *p_mp;
  clock_t user_time, sys_time;
  message m;

  /* Do not create core files for set uid execution */
  if (dump_core && rmp->mp_realuid != rmp->mp_effuid)
	dump_core = FALSE;

  /* System processes are destroyed before informing FS, meaning that FS can
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

  /* Do accounting: fetch usage times and accumulate at parent. */
  if((r=sys_times(proc_nr_e, &user_time, &sys_time, NULL, NULL)) != OK)
  	panic(__FILE__,"exit_proc: sys_times failed", r);

  p_mp = &mproc[rmp->mp_parent];			/* process' parent */
  p_mp->mp_child_utime += user_time + rmp->mp_child_utime; /* add user time */
  p_mp->mp_child_stime += sys_time + rmp->mp_child_stime; /* add system time */

  /* Tell the kernel the process is no longer runnable to prevent it from 
   * being scheduled in between the following steps. Then tell FS that it 
   * the process has exited and finally, clean up the process at the kernel.
   * This order is important so that FS can tell drivers to cancel requests
   * such as copying to/ from the exiting process, before it is gone.
   */
  if ((r = sys_stop(proc_nr_e)) != OK)		/* stop the process */
  	panic(__FILE__, "sys_stop failed", r);

  if((r=vm_willexit(proc_nr_e)) != OK) {
	panic(__FILE__, "exit_proc: vm_willexit failed", r);
  }
  vm_notify_sig_wrapper(rmp->mp_endpoint);

  if (proc_nr_e == INIT_PROC_NR)
  {
	printf("PM: INIT died\n");
	return;
  }
  if (proc_nr_e == FS_PROC_NR)
  {
	panic(__FILE__, "exit_proc: FS died", r);
  }

  /* Tell FS about the exiting process. */
  m.m_type = dump_core ? PM_DUMPCORE : PM_EXIT;
  m.PM_PROC = rmp->mp_endpoint;

  tell_fs(rmp, &m);

  if (rmp->mp_flags & PRIV_PROC)
  {
	/* Destroy system processes without waiting for FS. This is
	 * needed because the system process might be a block device
	 * driver that FS is blocked waiting on.
	 */
	if((r= sys_exit(rmp->mp_endpoint)) != OK)
		panic(__FILE__, "exit_proc: sys_exit failed", r);
  }

  /* Clean up most of the flags describing the process's state before the exit,
   * and mark it as exiting.
   */
  rmp->mp_flags &= (IN_USE|FS_CALL|PRIV_PROC|TRACE_EXIT);
  rmp->mp_flags |= EXITING;

  /* Keep the process around until FS is finished with it. */
  
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

		/* Notify new parent. */
		if (rmp->mp_flags & ZOMBIE)
			check_parent(rmp, TRUE /*try_cleanup*/);
	}
  }

  /* Send a hangup to the process' process group if it was a session leader. */
  if (procgrp != 0) check_sig(-procgrp, SIGHUP);
}

/*===========================================================================*
 *				exit_restart				     *
 *===========================================================================*/
PUBLIC void exit_restart(rmp, dump_core)
struct mproc *rmp;		/* pointer to the process being terminated */
int dump_core;			/* flag indicating whether to dump core */
{
/* FS replied to our exit or coredump request. Perform the second half of the
 * exit code.
 */
  int r;

  /* For core dumps, now is the right time to try to contact the parent. */
  if (dump_core)
	zombify(rmp);

  if (!(rmp->mp_flags & PRIV_PROC))
  {
	/* destroy the (user) process */
	if((r=sys_exit(rmp->mp_endpoint)) != OK)
		panic(__FILE__, "exit_restart: sys_exit failed", r);
  }

  /* Release the memory occupied by the child. */
  if((r=vm_exit(rmp->mp_endpoint)) != OK) {
  	panic(__FILE__, "exit_restart: vm_exit failed", r);
  }

  if (rmp->mp_flags & TRACE_EXIT)
  {
	/* Wake up the tracer, completing the ptrace(T_EXIT) call */
	mproc[rmp->mp_tracer].mp_reply.reply_trace = 0;
	setreply(rmp->mp_tracer, OK);
  }

  /* Clean up if the parent has collected the exit status */
  if (rmp->mp_flags & TOLD_PARENT)
	cleanup(rmp);
}

/*===========================================================================*
 *				do_waitpid				     *
 *===========================================================================*/
PUBLIC int do_waitpid()
{
/* A process wants to wait for a child to terminate. If a child is already 
 * waiting, go clean it up and let this WAIT call terminate.  Otherwise, 
 * really wait. 
 * A process calling WAIT never gets a reply in the usual way at the end
 * of the main loop (unless WNOHANG is set or no qualifying child exists).
 * If a child has already exited, the routine tell_parent() sends the reply
 * to awaken the caller.
 * Both WAIT and WAITPID are handled by this code.
 */
  register struct mproc *rp;
  int i, pidarg, options, children;

  /* Set internal variables, depending on whether this is WAIT or WAITPID. */
  pidarg  = (call_nr == WAIT ? -1 : m_in.pid);	   /* 1st param of waitpid */
  options = (call_nr == WAIT ?  0 : m_in.sig_nr);  /* 3rd param of waitpid */
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
		if (rp->mp_flags & STOPPED) {
			/* This child meets the pid test and is being traced.
			 * Deliver a signal to the tracer, if any.
			 */
			for (i = 1; i < _NSIG; i++) {
				if (sigismember(&rp->mp_sigtrace, i)) {
					sigdelset(&rp->mp_sigtrace, i);

					mp->mp_reply.reply_res2 =
						0177 | (i << 8);
					return(rp->mp_pid);
				}
			}
		}
	}

	if (rp->mp_parent == who_p) {
		if (rp->mp_flags & ZOMBIE) {
			/* This child meets the pid test and has exited. */
			tell_parent(rp); /* this child has already exited */
			if (!(rp->mp_flags & FS_CALL))
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
	return(SUSPEND);		     /* do not reply, let it wait */
  } else {
	/* No child even meets the pid test.  Return error immediately. */
	return(ECHILD);			     /* no - parent has no children */
  }
}

/*===========================================================================*
 *				wait_test				     *
 *===========================================================================*/
PUBLIC int wait_test(rmp, child)
struct mproc *rmp;			/* process that may be waiting */
struct mproc *child;			/* process that may be waited for */
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
PRIVATE void zombify(rmp)
struct mproc *rmp;
{
/* Zombify a process. First check if the exiting process is traced by a process
 * other than its parent; if so, the tracer must be notified about the exit
 * first. Once that is done, the real parent may be notified about the exit of
 * its child.
 */
  struct mproc *t_mp;

  if (rmp->mp_flags & (TRACE_ZOMBIE | ZOMBIE))
	panic(__FILE__, "zombify: process was already a zombie", NO_NUM);

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
PRIVATE void check_parent(child, try_cleanup)
struct mproc *child;			/* tells which process is exiting */
int try_cleanup;			/* clean up the child when done? */
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
	tell_parent(child);

	/* The 'try_cleanup' flag merely saves us from having to be really
	 * careful with statement ordering in exit_proc() and exit_restart().
	 */
	if (try_cleanup && !(child->mp_flags & FS_CALL))
		cleanup(child);
  }
  else {
	/* Parent is not waiting. */
	sig_proc(p_mp, SIGCHLD, TRUE /*trace*/);
  }
}

/*===========================================================================*
 *				tell_parent				     *
 *===========================================================================*/
PRIVATE void tell_parent(child)
register struct mproc *child;	/* tells which process is exiting */
{
  int exitstatus, mp_parent;
  struct mproc *parent;

  mp_parent= child->mp_parent;
  if (mp_parent <= 0)
	panic(__FILE__, "tell_parent: bad value in mp_parent", mp_parent);
  if(!(child->mp_flags & ZOMBIE))
  	panic(__FILE__, "tell_parent: child not a zombie", NO_NUM);
  if(child->mp_flags & TOLD_PARENT)
	panic(__FILE__, "tell_parent: telling parent again", NO_NUM);
  parent = &mproc[mp_parent];

  /* Wake up the parent by sending the reply message. */
  exitstatus = (child->mp_exitstatus << 8) | (child->mp_sigstatus & 0377);
  parent->mp_reply.reply_res2 = exitstatus;
  setreply(child->mp_parent, child->mp_pid);
  parent->mp_flags &= ~WAITING;		/* parent no longer waiting */
  child->mp_flags &= ~ZOMBIE;		/* child no longer a zombie */
  child->mp_flags |= TOLD_PARENT;	/* avoid informing parent twice */
}

/*===========================================================================*
 *				tell_tracer				     *
 *===========================================================================*/
PRIVATE void tell_tracer(child)
struct mproc *child;			/* tells which process is exiting */
{
  int exitstatus, mp_tracer;
  struct mproc *tracer;

  mp_tracer = child->mp_tracer;
  if (mp_tracer <= 0)
	panic(__FILE__, "tell_tracer: bad value in mp_tracer", mp_tracer);
  if(!(child->mp_flags & TRACE_ZOMBIE))
  	panic(__FILE__, "tell_tracer: child not a zombie", NO_NUM);
  tracer = &mproc[mp_tracer];

  exitstatus = (child->mp_exitstatus << 8) | (child->mp_sigstatus & 0377);
  tracer->mp_reply.reply_res2 = exitstatus;
  setreply(child->mp_tracer, child->mp_pid);
  tracer->mp_flags &= ~WAITING;		/* tracer no longer waiting */
  child->mp_flags &= ~TRACE_ZOMBIE;	/* child no longer zombie to tracer */
  child->mp_flags |= ZOMBIE;		/* child is now zombie to parent */
}

/*===========================================================================*
 *				tracer_died				     *
 *===========================================================================*/
PRIVATE void tracer_died(child)
struct mproc *child;			/* process being traced */
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
	sig_proc(child, SIGKILL, TRUE /*trace*/);

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
PRIVATE void cleanup(rmp)
register struct mproc *rmp;	/* tells which process is exiting */
{
  /* Release the process table entry and reinitialize some field. */
  rmp->mp_pid = 0;
  rmp->mp_flags = 0;
  rmp->mp_child_utime = 0;
  rmp->mp_child_stime = 0;
  procs_in_use--;
}

PUBLIC void _exit(int code)
{
	sys_exit(SELF);
}

PUBLIC void __exit(int code)
{
	sys_exit(SELF);
}
