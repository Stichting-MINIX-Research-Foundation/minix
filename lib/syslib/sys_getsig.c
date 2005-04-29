#include "syslib.h"

/*===========================================================================*
 *                                sys_getsig				     *
 *===========================================================================*/
PUBLIC int sys_getsig(k_proc_nr, k_sig_map)
int *k_proc_nr;				/* return process number here */
sigset_t *k_sig_map;			/* return signal map here */
{
    message m;
    int result;

    result = _taskcall(SYSTASK, SYS_GETSIG, &m);
    *k_proc_nr = m.SIG_PROC;
    *k_sig_map = (sigset_t) m.SIG_MAP;
    return(result);
}

