/* This file handles the EXEC system call.  It performs the work as follows:
 *    - see if the permissions allow the file to be executed
 *    - read the header and extract the sizes
 *    - fetch the initial args and environment from the user space
 *    - allocate the memory for the new process
 *    - copy the initial stack from PM to the process
 *    - read in the text and data segments and copy to the process
 *    - take care of setuid and setgid bits
 *    - fix up 'mproc' table
 *    - tell kernel about EXEC
 *    - save offset to initial argc (for procfs)
 *
 * The entry points into this file are:
 *   do_exec:	 perform the EXEC system call
 *   do_newexec: handle PM part of exec call after VFS
 *   do_execrestart: finish the special exec call for RS
 *   exec_restart: finish a regular exec call
 */

#include "pm.h"
#include <sys/stat.h>
#include <minix/callnr.h>
#include <minix/endpoint.h>
#include <minix/com.h>
#include <minix/vm.h>
#include <a.out.h>
#include <signal.h>
#include <string.h>
#include <libexec.h>
#include <sys/ptrace.h>
#include "mproc.h"
#include "param.h"

#define ESCRIPT	(-2000)	/* Returned by read_header for a #! script. */
#define PTRSIZE	sizeof(char *) /* Size of pointers in argv[] and envp[]. */

/*===========================================================================*
 *				do_exec					     *
 *===========================================================================*/
int do_exec()
{
	message m;

	/* Forward call to VFS */
	m.m_type = PM_EXEC;
	m.PM_PROC = mp->mp_endpoint;
	m.PM_PATH = m_in.exec_name;
	m.PM_PATH_LEN = m_in.exec_len;
	m.PM_FRAME = m_in.frame_ptr;
	m.PM_FRAME_LEN = m_in.msg_frame_len;
	m.PM_EXECFLAGS = m_in.PMEXEC_FLAGS;

	tell_vfs(mp, &m);

	/* Do not reply */
	return SUSPEND;
}


/*===========================================================================*
 *				do_newexec				     *
 *===========================================================================*/
int do_newexec()
{
	int proc_e, proc_n, allow_setuid;
	char *ptr;
	struct mproc *rmp;
	struct exec_info args;
	int r, flags = 0;

	if (who_e != VFS_PROC_NR && who_e != RS_PROC_NR)
		return EPERM;

	proc_e= m_in.EXC_NM_PROC;
	if (pm_isokendpt(proc_e, &proc_n) != OK) {
		panic("do_newexec: got bad endpoint: %d", proc_e);
	}
	rmp= &mproc[proc_n];
	ptr= m_in.EXC_NM_PTR;
	r= sys_datacopy(who_e, (vir_bytes)ptr,
		SELF, (vir_bytes)&args, sizeof(args));
	if (r != OK)
		panic("do_newexec: sys_datacopy failed: %d", r);

	allow_setuid = 0;	/* Do not allow setuid execution */
	rmp->mp_flags &= ~TAINTED;	/* By default not tainted */

	if (rmp->mp_tracer == NO_TRACER) {
		/* Okay, setuid execution is allowed */
		allow_setuid = 1;
	}

	if (allow_setuid && args.allow_setuid) {
		rmp->mp_effuid = args.new_uid;
		rmp->mp_effgid = args.new_gid;
	}

	/* A process is considered 'tainted' when it's executing with
	 * setuid or setgid bit set, or when the real{u,g}id doesn't
	 * match the eff{u,g}id, respectively. */
	if (allow_setuid && args.allow_setuid) {
		/* Program has setuid and/or setgid bits set */
		rmp->mp_flags |= TAINTED;
	} else if (rmp->mp_effuid != rmp->mp_realuid ||
		   rmp->mp_effgid != rmp->mp_realgid) {
		rmp->mp_flags |= TAINTED;
	}

	/* System will save command line for debugging, ps(1) output, etc. */
	strncpy(rmp->mp_name, args.progname, PROC_NAME_LEN-1);
	rmp->mp_name[PROC_NAME_LEN-1] = '\0';

	/* Save offset to initial argc (for procfs) */
	rmp->mp_frame_addr = (vir_bytes) args.stack_high - args.frame_len;
	rmp->mp_frame_len = args.frame_len;

	/* Kill process if something goes wrong after this point. */
	rmp->mp_flags |= PARTIAL_EXEC;

	mp->mp_reply.reply_res2= (vir_bytes) rmp->mp_frame_addr;
	mp->mp_reply.reply_res3= flags;
	if (allow_setuid && args.allow_setuid)
		mp->mp_reply.reply_res3 |= EXC_NM_RF_ALLOW_SETUID;

	return r;
}

/*===========================================================================*
 *				do_execrestart				     *
 *===========================================================================*/
int do_execrestart()
{
	int proc_e, proc_n, result;
	struct mproc *rmp;
	vir_bytes pc;

	if (who_e != RS_PROC_NR)
		return EPERM;

	proc_e= m_in.EXC_RS_PROC;
	if (pm_isokendpt(proc_e, &proc_n) != OK) {
		panic("do_execrestart: got bad endpoint: %d", proc_e);
	}
	rmp= &mproc[proc_n];
	result= m_in.EXC_RS_RESULT;
	pc= (vir_bytes)m_in.EXC_RS_PC;

	exec_restart(rmp, result, pc, rmp->mp_frame_addr);

	return OK;
}


/*===========================================================================*
 *				exec_restart				     *
 *===========================================================================*/
void exec_restart(rmp, result, pc, vfs_newsp)
struct mproc *rmp;
int result;
vir_bytes pc;
vir_bytes vfs_newsp;
{
	int r, sn;

	if (result != OK)
	{
		if (rmp->mp_flags & PARTIAL_EXEC)
		{
			/* Use SIGKILL to signal that something went wrong */
			sys_kill(rmp->mp_endpoint, SIGKILL);
			return;
		}
		setreply(rmp-mproc, result);
		return;
	}

	rmp->mp_flags &= ~PARTIAL_EXEC;

	/* Fix 'mproc' fields, tell kernel that exec is done, reset caught
	 * sigs.
	 */
	for (sn = 1; sn < _NSIG; sn++) {
		if (sigismember(&rmp->mp_catch, sn)) {
			sigdelset(&rmp->mp_catch, sn);
			rmp->mp_sigact[sn].sa_handler = SIG_DFL;
			sigemptyset(&rmp->mp_sigact[sn].sa_mask);
		}
	}

	/* Cause a signal if this process is traced.
	 * Do this before making the process runnable again!
	 */
#if USE_TRACE
	if (rmp->mp_tracer != NO_TRACER && !(rmp->mp_trace_flags & TO_NOEXEC))
	{
		sn = (rmp->mp_trace_flags & TO_ALTEXEC) ? SIGSTOP : SIGTRAP;

		check_sig(rmp->mp_pid, sn, FALSE /* ksig */);
	}
#endif /* USE_TRACE */

	/* Call kernel to exec with SP and PC set by VFS. */
	r= sys_exec(rmp->mp_endpoint, (char *) vfs_newsp, rmp->mp_name, pc);
	if (r != OK) panic("sys_exec failed: %d", r);
}

