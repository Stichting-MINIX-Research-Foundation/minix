#include "syslib.h"

/*===========================================================================*
 *                                sys_endksig				     *
 *===========================================================================*/
PUBLIC int sys_endksig(proc_ep)
endpoint_t proc_ep;				/* process number */
{
    message m;
    int result;

    m.SIG_ENDPT = proc_ep;
    result = _taskcall(SYSTASK, SYS_ENDKSIG, &m);
    return(result);
}

