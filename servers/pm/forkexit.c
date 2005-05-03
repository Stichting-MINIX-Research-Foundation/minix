/* This file deals with creating processes (via FORK) and deleting them (via
 * EXIT/WAIT).  When a process forks, a new slot in the 'mproc' table is
 * allocated for it, and a copy of the parent's core image is made for the
 * child.  Then the kernel and file system are informed.  A process is removed
 * from the 'mproc' table when two events have occurred: (1) it has exited or
 * been killed by a signal, and (2) the parent has done a WAIT.  If the process
 * exits first, it continues to occupy a slot until the parent does a WAIT.
 *
 * The entry points into this file are:
 *   do_fork:	 perform the FORK system call
 *   do_mm_exit: perform the EXIT system call (by calling mm_exit())
 *   mm_exit:	 actually do the exiting
 *   do_wait:	 perform the WAITPID or WAIT system call
 */


#include "mm.h"
#include <sys/wait.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/utils.h>
#include <signal.h>
#include "mproc.h"
#include "param.h"

#define LAST_FEW            2	/* last few slots reserved for superuser */

PRIVATE pid_t next_pid = INIT_PID+1;	/* next pid to be assigned */

FORWARD _PROTOTYPE (void cleanup, (register struct mproc *child) );

/*===========================================================================*
 *				do_fork					     *
 *===========================================================================*/
PUBLIC int do_fork()
{
/* The process pointed to by 'mp' has forked.  Create a child process. */

  register struct mproc *rmp;	/* pointer to parent */
  register struct mproc *rmc;	/* pointer to child */
  int i, child_nr, t;
  phys_clicks prog_clicks, child_base;
  phys_bytes prog_bytes, parent_abs, child_abs;	/* Intel only */

 /* If tables might fill up during FORK, don't even start since recovery half
  * way through is such a nuisance.
  */
  rmp = mp;
  if ((procs_in_use == NR_PROCS) || 
  		(procs_in_use >= NR_PROCS-LAST_FEW && rmp->mp_effuid != 0))
  {
  	printf("MM: proc table full!\n");
  	return(EAGAIN);
  }

  /* Determine how much memory to allocate.  Only the data and stack need to
   * be copied, because the text segment is either shared or of zero length.
   */
  prog_clicks = (phys_clicks) rmp->mp_seg[S].mem_len;
  prog_clicks += (rmp->mp_seg[S].mem_vir - rmp->mp_seg[D].mem_vir);
  prog_bytes = (phys_bytes) prog_clicks << CLICK_SHIFT;
  if ( (child_base = alloc_mem(prog_clicks)) == NO_MEM) return(ENOMEM);

  /* Create a copy of the parent's core image for the child. */
  child_abs = (phys_bytes) child_base << CLICK_SHIFT;
  parent_abs = (phys_bytes) rmp->mp_seg[D].mem_phys << CLICK_SHIFT;
  i = sys_abscopy(parent_abs, child_abs, prog_bytes);
  if (i < 0) panic("do_fork can't copy", i);

  /* Find a slot in 'mproc' for the child process.  A slot must exist. */
  for (rmc = &mproc[0]; rmc < &mproc[NR_PROCS]; rmc++)
	if ( (rmc->mp_flags & IN_USE) == 0) break;

  /* Set up the child and its memory map; copy its 'mproc' slot from parent. */
  child_nr = (int)(rmc - mproc);	/* slot number of the child */
  procs_in_use++;
  *rmc = *rmp;			/* copy parent's process slot to child's */

  rmc->mp_parent = who;			/* record child's parent */
  rmc->mp_flags &= (IN_USE|SEPARATE);	/* inherit only these flags */

  /* A separate I&D child keeps the parents text segment.  The data and stack
   * segments must refer to the new copy.
   */
  if (!(rmc->mp_flags & SEPARATE)) rmc->mp_seg[T].mem_phys = child_base;
  rmc->mp_seg[D].mem_phys = child_base;
  rmc->mp_seg[S].mem_phys = rmc->mp_seg[D].mem_phys + 
			(rmp->mp_seg[S].mem_vir - rmp->mp_seg[D].mem_vir);
  rmc->mp_exitstatus = 0;
  rmc->mp_sigstatus = 0;

  /* Find a free pid for the child and put it in the table. */
  do {
	t = 0;			/* 't' = 0 means pid still free */
	next_pid = (next_pid < 30000 ? next_pid + 1 : INIT_PID + 1);
	for (rmp = &mproc[0]; rmp < &mproc[NR_PROCS]; rmp++)
		if (rmp->mp_pid == next_pid || rmp->mp_procgrp == next_pid) {
			t = 1;
			break;
		}
	rmc->mp_pid = next_pid;	/* assign pid to child */
  } while (t);

  /* Tell kernel and file system about the (now successful) FORK. */
  sys_fork(who, child_nr, rmc->mp_pid);
  tell_fs(FORK, who, child_nr, rmc->mp_pid);

  /* Report child's memory map to kernel. */
  sys_newmap(child_nr, rmc->mp_seg);

  /* Reply to child to wake it up. */
  setreply(child_nr, 0);
  return(next_pid);		 /* child's pid */
}


/*===========================================================================*
 *				do_mm_exit				     *
 *===========================================================================*/
PUBLIC int do_mm_exit()
{
/* Perform the exit(status) system call. The real work is done by mm_exit(),
 * which is also called when a process is killed by a signal.
 */

  mm_exit(mp, m_in.status);
  return(SUSPEND);		/* can't communicate from beyond the grave */
}


/*===========================================================================*
 *				mm_exit					     *
 *===========================================================================*/
PUBLIC void mm_exit(rmp, exit_status)
register struct mproc *rmp;	/* pointer to the process to be terminated */
int exit_status;		/* the process' exit status (for parent) */
{
/* A process is done.  Release most of the process' possessions.  If its
 * parent is waiting, release the rest, else keep the process slot and
 * become a zombie.
 */

  register int proc_nr;
  int parent_waiting, right_child;
  pid_t pidarg, procgrp;
  struct mproc *p_mp;

  proc_nr = (int) (rmp - mproc);	/* get process slot number */

  /* Remember a session leader's process group. */
  procgrp = (rmp->mp_pid == mp->mp_procgrp) ? mp->mp_procgrp : 0;

  /* If the exited process has a timer pending, kill it. */
  if (rmp->mp_flags & ALARM_ON) set_alarm(proc_nr, (unsigned) 0);

  /* Tell the kernel and FS that the process is no longer runnable. */
  tell_fs(EXIT, proc_nr, 0, 0);  /* file system can free the proc slot */
  sys_xit(rmp->mp_parent, proc_nr);

  /* Release the memory occupied by the child. */
  if (find_share(rmp, rmp->mp_ino, rmp->mp_dev, rmp->mp_ctime) == NULL) {
	/* No other process shares the text segment, so free it. */
	free_mem(rmp->mp_seg[T].mem_phys, rmp->mp_seg[T].mem_len);
  }
  /* Free the data and stack segments. */
  free_mem(rmp->mp_seg[D].mem_phys,
      rmp->mp_seg[S].mem_vir + rmp->mp_seg[S].mem_len - rmp->mp_seg[D].mem_vir);

  /* The process slot can only be freed if the parent has done a WAIT. */
  rmp->mp_exitstatus = (char) exit_status;

  p_mp = &mproc[rmp->mp_parent];	/* process' parent */
  pidarg = p_mp->mp_wpid;		/* who's being waited for? */
  parent_waiting = p_mp->mp_flags & WAITING;

  right_child =				/* child meets one of the 3 tests? */
	(pidarg == -1 || pidarg == rmp->mp_pid || -pidarg == rmp->mp_procgrp);

  if (parent_waiting && right_child) {
	cleanup(rmp);			/* tell parent and release child slot */
  } else {
	rmp->mp_flags = IN_USE|ZOMBIE;	/* parent not waiting, zombify child */
	sig_proc(p_mp, SIGCHLD);	/* send parent a "child died" signal */
  }

  /* If the process has children, disinherit them.  INIT is the new parent. */
  for (rmp = &mproc[0]; rmp < &mproc[NR_PROCS]; rmp++) {
	if (rmp->mp_flags & IN_USE && rmp->mp_parent == proc_nr) {
		/* 'rmp' now points to a child to be disinherited. */
		rmp->mp_parent = INIT_PROC_NR;
		parent_waiting = mproc[INIT_PROC_NR].mp_flags & WAITING;
		if (parent_waiting && (rmp->mp_flags & ZOMBIE)) cleanup(rmp);
	}
  }

  /* Send a hangup to the process' process group if it was a session leader. */
  if (procgrp != 0) check_sig(-procgrp, SIGHUP);
}


/*===========================================================================*
 *				do_waitpid				     *
 *===========================================================================*/
PUBLIC int do_waitpid()
{
/* A process wants to wait for a child to terminate. If one is already waiting,
 * go clean it up and let this WAIT call terminate.  Otherwise, really wait.
 * Both WAIT and WAITPID are handled by this code.
 */

  register struct mproc *rp;
  int pidarg, options, children;

  /* A process calling WAIT never gets a reply in the usual way at the end
   * of the main loop (unless WNOHANG is set or no qualifying child exists).
   * If a child has already exited, the routine cleanup() sends the reply
   * to awaken the caller.
   */

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
	if ( (rp->mp_flags & IN_USE) && rp->mp_parent == who) {
		/* The value of pidarg determines which children qualify. */
		if (pidarg  > 0 && pidarg != rp->mp_pid) continue;
		if (pidarg < -1 && -pidarg != rp->mp_procgrp) continue;

		children++;		/* this child is acceptable */
		if (rp->mp_flags & ZOMBIE) {
			/* This child meets the pid test and has exited. */
			cleanup(rp);	/* this child has already exited */
			return(SUSPEND);
		}
		if ((rp->mp_flags & STOPPED) && rp->mp_sigstatus) {
			/* This child meets the pid test and is being traced.*/
			mp->mp_reply.reply_res2 = 0177|(rp->mp_sigstatus << 8);
			rp->mp_sigstatus = 0;
			return(rp->mp_pid);
		}
	}
  }

  /* No qualifying child has exited.  Wait for one, unless none exists. */
  if (children > 0) {
	/* At least 1 child meets the pid test exists, but has not exited. */
	if (options & WNOHANG) return(0);    /* parent does not want to wait */
	mp->mp_flags |= WAITING;	     /* parent wants to wait */
	mp->mp_wpid = (pid_t) pidarg;	     /* save pid for later */
	return(SUSPEND);		     /* do not reply, let it wait */
  } else {
	/* No child even meets the pid test.  Return error immediately. */
	return(ECHILD);			     /* no - parent has no children */
  }
}


/*===========================================================================*
 *				cleanup					     *
 *===========================================================================*/
PRIVATE void cleanup(child)
register struct mproc *child;	/* tells which process is exiting */
{
/* Finish off the exit of a process.  The process has exited or been killed
 * by a signal, and its parent is waiting.
 */

  struct mproc *parent = &mproc[child->mp_parent];
  int exitstatus;

  /* Wake up the parent. */
  exitstatus = (child->mp_exitstatus << 8) | (child->mp_sigstatus & 0377);
  parent->mp_reply.reply_res2 = exitstatus;
  setreply(child->mp_parent, child->mp_pid);
  parent->mp_flags &= ~WAITING;		/* parent no longer waiting */

  /* Release the process table entry. */
  child->mp_flags = 0;
  procs_in_use--;
}
