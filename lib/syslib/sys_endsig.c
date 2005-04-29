#include "syslib.h"

/*===========================================================================*
 *                                sys_endsig				     *
 *===========================================================================*/
PUBLIC int sys_endsig(proc_nr)
int proc_nr;				/* process number */
{
    message m;
    int result;

    m.SIG_PROC = proc_nr;
    result = _taskcall(SYSTASK, SYS_ENDSIG, &m);
    return(result);
}

