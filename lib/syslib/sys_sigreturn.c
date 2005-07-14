#include "syslib.h"

/*===========================================================================*
 *                                sys_sigreturn				     *
 *===========================================================================*/
PUBLIC int sys_sigreturn(proc_nr, sig_ctxt, flags)
int proc_nr;				/* for which process */
struct sigmsg *sig_ctxt;		/* POSIX style handling */
int flags;				/* flags for POSIX handling */
{
    message m;
    int result;

    m.SIG_PROC = proc_nr;
    m.SIG_CTXT_PTR = (char *) sig_ctxt;
    m.SIG_FLAGS = flags;
    result = _taskcall(SYSTASK, SYS_SIGRETURN, &m);
    return(result);
}

