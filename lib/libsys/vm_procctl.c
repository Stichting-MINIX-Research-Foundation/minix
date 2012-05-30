
#include "syslib.h"

#include <minix/vm.h>
#include <string.h>

/*===========================================================================*
 *                                vm_exit				     *
 *===========================================================================*/
int vm_procctl(endpoint_t ep, int param)
{
    message m;
    int result;

    memset(&m, 0, sizeof(m));

    m.VMPCTL_WHO = ep;
    m.VMPCTL_PARAM = param;

    result = _taskcall(VM_PROC_NR, VM_PROCCTL, &m);
    return(result);
}

