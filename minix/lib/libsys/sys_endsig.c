#include "syslib.h"

/*===========================================================================*
 *                                sys_endksig				     *
 *===========================================================================*/
int sys_endksig(proc_ep)
endpoint_t proc_ep;				/* process number */
{
    message m;
    int result;

    m.m_sigcalls.endpt = proc_ep;
    result = _kernel_call(SYS_ENDKSIG, &m);
    return(result);
}

