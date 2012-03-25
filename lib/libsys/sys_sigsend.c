#include "syslib.h"

/*===========================================================================*
 *                                sys_sigsend				     *
 *===========================================================================*/
int sys_sigsend(proc_ep, sig_ctxt)
endpoint_t proc_ep;			/* for which process */
struct sigmsg *sig_ctxt;		/* POSIX style handling */
{
    message m;
    int result;

    m.SIG_ENDPT = proc_ep;
    m.SIG_CTXT_PTR = (char *) sig_ctxt;
    result = _kernel_call(SYS_SIGSEND, &m);
    return(result);
}

