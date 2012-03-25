
#include "syslib.h"

#include <minix/vm.h>

/*===========================================================================*
 *                                vm_exit				     *
 *===========================================================================*/
int vm_exit(endpoint_t ep)
{
    message m;
    int result;

    m.VME_ENDPOINT = ep;

    result = _taskcall(VM_PROC_NR, VM_EXIT, &m);
    return(result);
}


/*===========================================================================*
 *                                vm_willexit				     *
 *===========================================================================*/
int vm_willexit(endpoint_t ep)
{
    message m;
    int result;

    m.VMWE_ENDPOINT = ep;

    result = _taskcall(VM_PROC_NR, VM_WILLEXIT, &m);
    return(result);
}

