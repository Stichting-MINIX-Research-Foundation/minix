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
 *    - save offset to initial argc (for ps)
 *
 * The entry points into this file are:
 *   do_exec:	 perform the EXEC system call
 *   do_exec_newmem: allocate new memory map for a process that tries to exec
 *   do_execrestart: finish the special exec call for RS
 *   exec_restart: finish a regular exec call
 *   find_share: find a process whose text segment can be shared
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
#include <sys/ptrace.h>
#include "mproc.h"
#include "param.h"

#define ESCRIPT	(-2000)	/* Returned by read_header for a #! script. */
#define PTRSIZE	sizeof(char *) /* Size of pointers in argv[] and envp[]. */

/*===========================================================================*
 *				do_exec					     *
 *===========================================================================*/
PUBLIC int do_exec()
{
	message m;
	int r;

	/* Forward call to FS */
	m.m_type = PM_EXEC;
	m.PM_PROC = mp->mp_endpoint;
	m.PM_PATH = m_in.exec_name;
	m.PM_PATH_LEN = m_in.exec_len;
	m.PM_FRAME = m_in.stack_ptr;
	m.PM_FRAME_LEN = m_in.stack_bytes;

	tell_fs(mp, &m);

	/* Do not reply */
	return SUSPEND;
}


/*===========================================================================*
 *				do_exec_newmem				     *
 *===========================================================================*/
PUBLIC int do_exec_newmem()
{
	int proc_e, proc_n, allow_setuid;
	char *ptr;
	struct mproc *rmp;
	struct exec_newmem args;
	int r, flags;
	char *stack_top;

	if (who_e != FS_PROC_NR && who_e != RS_PROC_NR)
		return EPERM;

	proc_e= m_in.EXC_NM_PROC;
	if (pm_isokendpt(proc_e, &proc_n) != OK)
	{
		panic(__FILE__, "do_exec_newmem: got bad endpoint",
			proc_e);
	}
	rmp= &mproc[proc_n];
	ptr= m_in.EXC_NM_PTR;
	r= sys_datacopy(who_e, (vir_bytes)ptr,
		SELF, (vir_bytes)&args, sizeof(args));
	if (r != OK)
		panic(__FILE__, "do_exec_newmem: sys_datacopy failed", r);

	if((r=vm_exec_newmem(proc_e, &args, sizeof(args), &stack_top, &flags)) == OK) {
		allow_setuid= 0;                /* Do not allow setuid execution */  

		if (rmp->mp_tracer == NO_TRACER) {
			/* Okay, setuid execution is allowed */
			allow_setuid= 1;
			rmp->mp_effuid = args.new_uid;
			rmp->mp_effgid = args.new_gid;
		}

		/* System will save command line for debugging, ps(1) output, etc. */
		strncpy(rmp->mp_name, args.progname, PROC_NAME_LEN-1);
		rmp->mp_name[PROC_NAME_LEN-1] = '\0';

		/* Save offset to initial argc (for ps) */
		rmp->mp_procargs = (vir_bytes) stack_top - args.args_bytes;

		/* Kill process if something goes wrong after this point. */
		rmp->mp_flags |= PARTIAL_EXEC;

		mp->mp_reply.reply_res2= (vir_bytes) stack_top;
		mp->mp_reply.reply_res3= flags;
		if (allow_setuid)
			mp->mp_reply.reply_res3 |= EXC_NM_RF_ALLOW_SETUID;
	} else {
		printf("PM: newmem failed for %s\n", args.progname);
	}
	return r;
}

/*===========================================================================*
 *				do_execrestart				     *
 *===========================================================================*/
PUBLIC int do_execrestart()
{
	int proc_e, proc_n, result;
	struct mproc *rmp;

	if (who_e != RS_PROC_NR)
		return EPERM;

	proc_e= m_in.EXC_RS_PROC;
	if (pm_isokendpt(proc_e, &proc_n) != OK)
	{
		panic(__FILE__, "do_execrestart: got bad endpoint",
			proc_e);
	}
	rmp= &mproc[proc_n];
	result= m_in.EXC_RS_RESULT;

	exec_restart(rmp, result);

	return OK;
}


/*===========================================================================*
 *				exec_restart				     *
 *===========================================================================*/
PUBLIC void exec_restart(rmp, result)
struct mproc *rmp;
int result;
{
	int r, sn;
	vir_bytes pc;
	char *new_sp;

	if (result != OK)
	{
		if (rmp->mp_flags & PARTIAL_EXEC)
		{
			/* Use SIGILL signal that something went wrong */
			rmp->mp_sigstatus = SIGILL;
			exit_proc(rmp, 0, FALSE /*dump_core*/);
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
	if (rmp->mp_tracer != NO_TRACER && !(rmp->mp_trace_flags & TO_NOEXEC))
	{
		sn = (rmp->mp_trace_flags & TO_ALTEXEC) ? SIGSTOP : SIGTRAP;

		check_sig(rmp->mp_pid, sn);
	}

	new_sp= (char *)rmp->mp_procargs;
	pc= 0;	/* for now */
	r= sys_exec(rmp->mp_endpoint, new_sp, rmp->mp_name, pc);
	if (r != OK) panic(__FILE__, "sys_exec failed", r);
}

