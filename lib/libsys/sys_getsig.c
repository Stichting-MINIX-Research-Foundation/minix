#include "syslib.h"

/*===========================================================================*
 *                                sys_getksig				     *
 *===========================================================================*/
int sys_getksig(proc_ep, k_sig_map)
endpoint_t *proc_ep;			/* return process number here */
sigset_t *k_sig_map;			/* return signal map here */
{
    message m;
    int result;

    result = _kernel_call(SYS_GETKSIG, &m);
    *proc_ep = m.SYS_SIG_ENDPT;
    *k_sig_map = m.SYS_SIG_MAP;
    return(result);
}

