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
 * T_SYSCALL	trace system call
 * T_ATTACH	attach to an existing process
 * T_DETACH	detach from a traced process
 * T_SETOPT	set trace options
 * T_GETRANGE	get range of values
 * T_SETRANGE	set range of values
 * 
 * The T_OK, T_ATTACH, T_EXIT, and T_SETOPT commands are handled here, and the
 * T_RESUME, T_STEP, T_SYSCALL, and T_DETACH commands are partially handled
 * here and completed by the system task. The rest are handled entirely by the
 * system task. 
 */

#include "pm.h"
#include <minix/com.h>
#include <sys/ptrace.h>
#include <signal.h>
#include "mproc.h"
#include "param.h"

/*===========================================================================*
 *				do_trace  				     *
 *===========================================================================*/
int do_trace()
{
  register struct mproc *child;
  struct ptrace_range pr;
  int i, r, req;
  message m;

  req = m_in.request;

  /* The T_OK call is made by the child fork of the debugger before it execs
   * the process to be traced. The T_ATTACH call is made by the debugger itself
   * to attach to an existing process.
   */
  switch (req) {
  case T_OK:		/* enable tracing by parent for this proc */
	if (mp->mp_tracer != NO_TRACER) return(EBUSY);

	mp->mp_tracer = mp->mp_parent;
	mp->mp_reply.reply_trace = 0;
	return(OK);

  case T_ATTACH:	/* attach to an existing process */
	if ((child = find_proc(m_in.pid)) == NULL) return(ESRCH);
	if (child->mp_flags & EXITING) return(ESRCH);

	/* For non-root processes, user and group ID must match. */
	if (mp->mp_effuid != SUPER_USER &&
		(mp->mp_effuid != child->mp_effuid ||
		 mp->mp_effgid != child->mp_effgid ||
		 child->mp_effuid != child->mp_realuid ||
		 child->mp_effgid != child->mp_realgid)) return(EPERM);

	/* Only root may trace system servers. */
	if (mp->mp_effuid != SUPER_USER && (child->mp_flags & PRIV_PROC))
		return(EPERM);

	/* System servers may not trace anyone. They can use sys_trace(). */
	if (mp->mp_flags & PRIV_PROC) return(EPERM);

	/* Can't trace self, PM or VM. */
	if (child == mp || child->mp_endpoint == PM_PROC_NR ||
		child->mp_endpoint == VM_PROC_NR) return(EPERM);

	/* Can't trace a process that is already being traced. */
	if (child->mp_tracer != NO_TRACER) return(EBUSY);

	child->mp_tracer = who_p;
	child->mp_trace_flags = TO_NOEXEC;

	sig_proc(child, SIGSTOP, TRUE /*trace*/, FALSE /* ksig */);

	mp->mp_reply.reply_trace = 0;
	return(OK);

  case T_DUMPCORE:
	if ((child = find_proc(m_in.pid)) == NULL) return(ESRCH);

	/* Allow dumpcore only if traced! */
	if (child->mp_tracer != who_p) return(EPERM);

	/* Tell VFS to dump the core. */
	m.m_type = PM_DUMPCORE;
	m.PM_PROC = mp->mp_endpoint;
	m.PM_TRACED_PROC = child->mp_endpoint;
	/* Note that m.PM_PROC != m.PM_TRACED_PROC
	 * (we use this to differentiate between a VFS core dump reply for a
	 * an exiting process and the one for a traced process) */

	m.PM_TERM_SIG = child->mp_sigstatus;
	m.PM_PATH = child->mp_name;

	tell_vfs(mp, &m);

	return(SUSPEND); /* Suspend the process until we receive reply from VFS */

  case T_STOP:		/* stop the process */
	/* This call is not exposed to user programs, because its effect can be
	 * achieved better by sending the traced process a signal with kill(2).
	 */
	return(EINVAL);

  case T_READB_INS:	/* special hack for reading text segments */
	if (mp->mp_effuid != SUPER_USER) return(EPERM);
	if ((child = find_proc(m_in.pid)) == NULL) return(ESRCH);
	if (child->mp_flags & EXITING) return(ESRCH);

	r = sys_trace(req, child->mp_endpoint, m_in.PMTRACE_ADDR, &m_in.data);
	if (r != OK) return(r);

	mp->mp_reply.reply_trace = m_in.data;
	return(OK);

  case T_WRITEB_INS:	/* special hack for patching text segments */
	if (mp->mp_effuid != SUPER_USER) return(EPERM);
	if ((child = find_proc(m_in.pid)) == NULL) return(ESRCH);
	if (child->mp_flags & EXITING) return(ESRCH);

#if 0
	/* Should check for shared text */

	/* Make sure the text segment is not used as a source for shared
	 * text.
	 */
	child->mp_ino = 0;
	child->mp_dev = 0;
	child->mp_ctime = 0;
#endif

	r = sys_trace(req, child->mp_endpoint, m_in.PMTRACE_ADDR, &m_in.data);
	if (r != OK) return(r);

	mp->mp_reply.reply_trace = m_in.data;
	return(OK);
  }

  /* All the other calls are made by the tracing process to control execution
   * of the child. For all these calls, the child must be stopped.
   */
  if ((child = find_proc(m_in.pid)) == NULL) return(ESRCH);
  if (child->mp_flags & EXITING) return(ESRCH);
  if (child->mp_tracer != who_p) return(ESRCH);
  if (!(child->mp_flags & STOPPED)) return(EBUSY);

  switch (req) {
  case T_EXIT:		/* exit */
	child->mp_flags |= TRACE_EXIT;

	/* Defer the exit if the traced process has an VFS call pending. */
	if (child->mp_flags & VFS_CALL)
		child->mp_exitstatus = (int) m_in.data;	/* save for later */
	else
		exit_proc(child, (int) m_in.data, FALSE /*dump_core*/);

	/* Do not reply to the caller until VFS has processed the exit
	 * request.
	 */
	return(SUSPEND);

  case T_SETOPT:	/* set trace options */
	child->mp_trace_flags = m_in.data;

	mp->mp_reply.reply_trace = 0;
	return(OK);

  case T_GETRANGE:
  case T_SETRANGE:	/* get/set range of values */
	r = sys_datacopy(who_e, (vir_bytes) m_in.PMTRACE_ADDR,
			SELF, (vir_bytes) &pr, (phys_bytes) sizeof(pr));
	if (r != OK) return(r);

	if (pr.pr_space != TS_INS && pr.pr_space != TS_DATA) return(EINVAL);
	if (pr.pr_size == 0 || pr.pr_size > LONG_MAX) return(EINVAL);

	if (req == T_GETRANGE)
		r = sys_vircopy(child->mp_endpoint, (vir_bytes) pr.pr_addr,
			who_e, (vir_bytes) pr.pr_ptr,
			(phys_bytes) pr.pr_size);
	else
		r = sys_vircopy(who_e, (vir_bytes) pr.pr_ptr,
			child->mp_endpoint, (vir_bytes) pr.pr_addr,
			(phys_bytes) pr.pr_size);

	if (r != OK) return(r);

	mp->mp_reply.reply_trace = 0;
	return(OK);

  case T_DETACH:	/* detach from traced process */
	if (m_in.data < 0 || m_in.data >= _NSIG) return(EINVAL);

	child->mp_tracer = NO_TRACER;

	/* Let all tracer-pending signals through the filter. */
	for (i = 1; i < _NSIG; i++) {
		if (sigismember(&child->mp_sigtrace, i)) {
			sigdelset(&child->mp_sigtrace, i);
			check_sig(child->mp_pid, i, FALSE /* ksig */);
		}
	}

	if (m_in.data > 0) {		/* issue signal */
		sig_proc(child, (int) m_in.data, TRUE /*trace*/, 
			FALSE /* ksig */);
	}

	/* Resume the child as if nothing ever happened. */ 
	child->mp_flags &= ~STOPPED;
	child->mp_trace_flags = 0;

	check_pending(child);

	break;

  case T_RESUME: 
  case T_STEP:
  case T_SYSCALL:	/* resume execution */
	if (m_in.data < 0 || m_in.data >= _NSIG) return(EINVAL);

	if (m_in.data > 0) {		/* issue signal */
		sig_proc(child, (int) m_in.data, FALSE /*trace*/,
			FALSE /* ksig */);
	}

	/* If there are any other signals waiting to be delivered,
	 * feign a successful resumption.
	 */
	for (i = 1; i < _NSIG; i++) {
		if (sigismember(&child->mp_sigtrace, i)) {
			mp->mp_reply.reply_trace = 0;
			return(OK);
		}
	}

	child->mp_flags &= ~STOPPED;

	check_pending(child);

	break;
  }
  r = sys_trace(req, child->mp_endpoint, m_in.PMTRACE_ADDR, &m_in.data);
  if (r != OK) return(r);

  mp->mp_reply.reply_trace = m_in.data;
  return(OK);
}

/*===========================================================================*
 *				stop_proc				     *
 *===========================================================================*/
void stop_proc(rmp, signo)
register struct mproc *rmp;
int signo;
{
/* A traced process got a signal so stop it. */

  register struct mproc *rpmp = mproc + rmp->mp_tracer;
  int r;

  r = sys_trace(T_STOP, rmp->mp_endpoint, 0L, (long *) 0);
  if (r != OK) panic("sys_trace failed: %d", r);
 
  rmp->mp_flags |= STOPPED;
  if (wait_test(rpmp, rmp)) {
	sigdelset(&rmp->mp_sigtrace, signo);

	rpmp->mp_flags &= ~WAITING;	/* parent is no longer waiting */
	rpmp->mp_reply.reply_res2 = 0177 | (signo << 8);
	setreply(rmp->mp_tracer, rmp->mp_pid);
  }
}
