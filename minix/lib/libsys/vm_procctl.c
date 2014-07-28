
#include "syslib.h"

#include <minix/vm.h>
#include <string.h>

/*===========================================================================*
 *                                vm_procctl				     *
 *===========================================================================*/
static int vm_procctl(endpoint_t ep, int param,
	vir_bytes m1, vir_bytes len, int flags)
{
    message m;
    int result;

    memset(&m, 0, sizeof(m));

    m.VMPCTL_WHO = ep;
    m.VMPCTL_PARAM = param;
    m.VMPCTL_M1 = m1;
    m.VMPCTL_LEN = len;
    m.VMPCTL_FLAGS = flags;

    result = _taskcall(VM_PROC_NR, VM_PROCCTL, &m);
    return(result);
}

int vm_procctl_clear(endpoint_t ep)
{
	return vm_procctl(ep, VMPPARAM_CLEAR, 0, 0, 0);
}

int vm_procctl_handlemem(endpoint_t ep, vir_bytes m1, vir_bytes len,
	int writeflag)
{
	return vm_procctl(ep, VMPPARAM_HANDLEMEM, m1, len, writeflag);
}

