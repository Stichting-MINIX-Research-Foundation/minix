/* This file handles the process manager's part of debugging, using the 
 * ptrace system call. Most of the commands are passed on to the system
 * task for completion.
 *
 * The debugging commands available are:
 * T_STOP	stop the process 
 * T_OK		enable tracing by parent for this process
 * T_GETINS	return value from instruction space 
 * T_GETDATA	return value from data space 
 * T_GETUSER	return value from user process table
 * T_SETINS	set value in instruction space
 * T_SETDATA	set value in data space
 * T_SETUSER	set value in user process table 
 * T_RESUME	resume execution 
 * T_EXIT	exit
 * T_STEP	set trace bit 
 * 
 * The T_OK and T_EXIT commands are handled here, and the T_RESUME and
 * T_STEP commands are partially handled here and completed by the system
 * task. The rest are handled entirely by the system task. 
 */

#include "pm.h"
#include <minix/com.h>
#include <sys/ptrace.h>
#include <signal.h>
#include "mproc.h"
#include "param.h"

#define NIL_MPROC	((struct mproc *) 0)

FORWARD _PROTOTYPE( struct mproc *find_proc, (pid_t lpid) );

/*===========================================================================*
 *				do_trace  				     *
 *===========================================================================*/
PUBLIC int do_trace()
{
  register struct mproc *child;

  /* the T_OK call is made by the child fork of the debugger before it execs  
   * the process to be traced
   */
  if (m_in.request == T_OK) {	/* enable tracing by parent for this proc */
	mp->mp_flags |= TRACED;
	mp->mp_reply.reply_trace = 0;
	return(OK);
  }
  if ((child=find_proc(m_in.pid))==NIL_MPROC || !(child->mp_flags & STOPPED)) {
	return(ESRCH);
  }
  /* all the other calls are made by the parent fork of the debugger to 
   * control execution of the child
   */
  switch (m_in.request) {
  case T_EXIT:		/* exit */
	pm_exit(child, (int) m_in.data);
	mp->mp_reply.reply_trace = 0;
	return(OK);
  case T_RESUME: 
  case T_STEP: 		/* resume execution */
	if (m_in.data < 0 || m_in.data > _NSIG) return(EIO);
	if (m_in.data > 0) {		/* issue signal */
		child->mp_flags &= ~TRACED;  /* so signal is not diverted */
		sig_proc(child, (int) m_in.data);
		child->mp_flags |= TRACED;
	}
	child->mp_flags &= ~STOPPED;
  	break;
  }
  if (sys_trace(m_in.request,(int)(child-mproc),m_in.taddr,&m_in.data) != OK)
	return(-errno);
  mp->mp_reply.reply_trace = m_in.data;
  return(OK);
}

/*===========================================================================*
 *				find_proc  				     *
 *===========================================================================*/
PRIVATE struct mproc *find_proc(lpid)
pid_t lpid;
{
  register struct mproc *rmp;

  for (rmp = &mproc[0]; rmp < &mproc[NR_PROCS]; rmp++)
	if (rmp->mp_flags & IN_USE && rmp->mp_pid == lpid) return(rmp);
  return(NIL_MPROC);
}

/*===========================================================================*
 *				stop_proc  				     *
 *===========================================================================*/
PUBLIC void stop_proc(rmp, signo)
register struct mproc *rmp;
int signo;
{
/* A traced process got a signal so stop it. */

  register struct mproc *rpmp = mproc + rmp->mp_parent;

  if (sys_trace(-1, (int) (rmp - mproc), 0L, (long *) 0) != OK) return;
  rmp->mp_flags |= STOPPED;
  if (rpmp->mp_flags & WAITING) {
	rpmp->mp_flags &= ~WAITING;	/* parent is no longer waiting */
	rpmp->mp_reply.reply_res2 = 0177 | (signo << 8);
	setreply(rmp->mp_parent, rmp->mp_pid);
  } else {
	rmp->mp_sigstatus = signo;
  }
  return;
}
