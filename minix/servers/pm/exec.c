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
#include <signal.h>
#include <libexec.h>
#include <sys/ptrace.h>
#include "mproc.h"

#define ESCRIPT	(-2000)	/* Returned by read_header for a #! script. */
#define PTRSIZE	sizeof(char *) /* Size of pointers in argv[] and envp[]. */

/*===========================================================================*
 *				do_exec					     *
 *===========================================================================*/
int
do_exec(void)
{
	message m;

	/* Forward call to VFS */
	memset(&m, 0, sizeof(m));
	m.m_type = VFS_PM_EXEC;
	m.VFS_PM_ENDPT = mp->mp_endpoint;
	m.VFS_PM_PATH = (void *)m_in.m_lc_pm_exec.name;
	m.VFS_PM_PATH_LEN = m_in.m_lc_pm_exec.namelen;
	m.VFS_PM_FRAME = (void *)m_in.m_lc_pm_exec.frame;
	m.VFS_PM_FRAME_LEN = m_in.m_lc_pm_exec.framelen;
	m.VFS_PM_PS_STR = m_in.m_lc_pm_exec.ps_str;

	tell_vfs(mp, &m);

	/* Do not reply */
	return SUSPEND;
}


/*===========================================================================*
 *				do_newexec				     *
 *===========================================================================*/
int do_newexec(void)
{
	int proc_e, proc_n, allow_setuid;
	vir_bytes ptr;
	struct mproc *rmp;
	struct exec_info args;
	int r;

	if (who_e != VFS_PROC_NR && who_e != RS_PROC_NR)
		return EPERM;

	proc_e= m_in.m_lexec_pm_exec_new.endpt;
	if (pm_isokendpt(proc_e, &proc_n) != OK) {
		panic("do_newexec: got bad endpoint: %d", proc_e);
	}
	rmp= &mproc[proc_n];
	ptr= m_in.m_lexec_pm_exec_new.ptr;
	r= sys_datacopy(who_e, ptr, SELF, (vir_bytes)&args, sizeof(args));
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

	/* Always update the saved user and group ID at this point. */
	rmp->mp_svuid = rmp->mp_effuid;
	rmp->mp_svgid = rmp->mp_effgid;

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

	mp->mp_reply.m_pm_lexec_exec_new.suid = (allow_setuid && args.allow_setuid);

	return r;
}

/*===========================================================================*
 *				do_execrestart				     *
 *===========================================================================*/
int do_execrestart(void)
{
	int proc_e, proc_n, result;
	struct mproc *rmp;
	vir_bytes pc, ps_str;

	if (who_e != RS_PROC_NR)
		return EPERM;

	proc_e = m_in.m_rs_pm_exec_restart.endpt;
	if (pm_isokendpt(proc_e, &proc_n) != OK) {
		panic("do_execrestart: got bad endpoint: %d", proc_e);
	}
	rmp = &mproc[proc_n];
	result = m_in.m_rs_pm_exec_restart.result;
	pc = m_in.m_rs_pm_exec_restart.pc;
	ps_str = m_in.m_rs_pm_exec_restart.ps_str;

	exec_restart(rmp, result, pc, rmp->mp_frame_addr, ps_str);

	return OK;
}

/*===========================================================================*
 *				exec_restart				     *
 *===========================================================================*/
void exec_restart(struct mproc *rmp, int result, vir_bytes pc, vir_bytes sp,
       vir_bytes ps_str)
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
		reply(rmp-mproc, result);
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
	if (rmp->mp_tracer != NO_TRACER && !(rmp->mp_trace_flags & TO_NOEXEC))
	{
		sn = (rmp->mp_trace_flags & TO_ALTEXEC) ? SIGSTOP : SIGTRAP;

		check_sig(rmp->mp_pid, sn, FALSE /* ksig */);
	}

	/* Call kernel to exec with SP and PC set by VFS. */
	r = sys_exec(rmp->mp_endpoint, sp, (vir_bytes)rmp->mp_name, pc, ps_str);
	if (r != OK) panic("sys_exec failed: %d", r);
}

