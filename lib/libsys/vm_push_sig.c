
#include "syslib.h"

#include <minix/vm.h>

/*===========================================================================*
 *                                vm_push_sig			     *
 *===========================================================================*/
PUBLIC int vm_push_sig(endpoint_t ep, vir_bytes *old_sp)
{
    message m;
    int result;

    m.VMPS_ENDPOINT = ep;
    result = _taskcall(VM_PROC_NR, VM_PUSH_SIG, &m);
    *old_sp = (vir_bytes)  m.VMPS_OLD_SP;

    return result;
}

