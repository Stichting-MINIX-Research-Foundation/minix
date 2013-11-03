
#include "syslib.h"

#include <string.h>
#include <minix/vm.h>

/*===========================================================================*
 *                                vm_notify_sig				     *
 *===========================================================================*/
int vm_notify_sig(endpoint_t ep, endpoint_t ipc_ep)
{
    message m;
    int result;

    memset(&m, 0, sizeof(m));
    m.VM_NOTIFY_SIG_ENDPOINT = ep;
    m.VM_NOTIFY_SIG_IPC = ipc_ep;

    result = _taskcall(VM_PROC_NR, VM_NOTIFY_SIG, &m);
    return(result);
}

