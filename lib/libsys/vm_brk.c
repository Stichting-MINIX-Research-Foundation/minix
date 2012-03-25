
#include "syslib.h"

#include <minix/vm.h>

/*===========================================================================*
 *                                vm_brk				     *
 *===========================================================================*/
int vm_brk(endpoint_t ep, char *addr)
{
    message m;

    m.VMB_ENDPOINT = ep;
    m.VMB_ADDR = (void *) addr;

    return _taskcall(VM_PROC_NR, VM_BRK, &m);
}

