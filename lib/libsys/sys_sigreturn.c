#include "syslib.h"

/*===========================================================================*
 *                                sys_sigreturn				     *
 *===========================================================================*/
int sys_sigreturn(proc_ep, sig_ctxt)
endpoint_t proc_ep;			/* for which process */
struct sigmsg *sig_ctxt;		/* POSIX style handling */
{
    message m;
    int result;

    m.SYS_SIG_ENDPT = proc_ep;
    m.SYS_SIG_CTXT_PTR = (char *) sig_ctxt;
    result = _kernel_call(SYS_SIGRETURN, &m);
    return(result);
}

