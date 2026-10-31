#include "syslib.h"

/*===========================================================================*
 *                                sys_endksig				     *
 *===========================================================================*/
int sys_endksig(endpoint_t proc_ep)
{
    message m;
    int result;

    m.m_sigcalls.endpt = proc_ep;
    result = _kernel_call(SYS_ENDKSIG, &m);
    return(result);
}

