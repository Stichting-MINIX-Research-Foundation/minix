#include "syslib.h"

/*===========================================================================*
 *                                sys_sigreturn				     *
 *===========================================================================*/
int sys_sigreturn(endpoint_t proc_ep, struct sigmsg *sig_ctxt)
{
    message m;
    int result;

    m.m_sigcalls.endpt = proc_ep;
    m.m_sigcalls.sigctx = sig_ctxt;
    result = _kernel_call(SYS_SIGRETURN, &m);
    return(result);
}

