#include "syslib.h"

/*===========================================================================*
 *                                sys_endksig				     *
 *===========================================================================*/
PUBLIC int sys_endksig(proc_nr)
int proc_nr;				/* process number */
{
    message m;
    int result;

    m.SIG_PROC = proc_nr;
    result = _taskcall(SYSTASK, SYS_ENDKSIG, &m);
    return(result);
}

