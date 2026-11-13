#include "syslib.h"

/*===========================================================================*
 *                                sys_getksig				     *
 *===========================================================================*/
int sys_getksig(endpoint_t *proc_ep, sigset_t *k_sig_map)
{
    message m;
    int result;

    result = _kernel_call(SYS_GETKSIG, &m);
    *proc_ep = m.m_sigcalls.endpt;
    *k_sig_map = m.m_sigcalls.map;
    return(result);
}

