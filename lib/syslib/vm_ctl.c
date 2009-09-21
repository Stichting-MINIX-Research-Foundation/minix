
#include "syslib.h"

#include <minix/vm.h>

/*===========================================================================*
 *                                vm_umap				     *
 *===========================================================================*/
PUBLIC int vm_ctl(int what, int param)
{
    message m;
    int result;

    m.VCTL_WHAT = what;
    m.VCTL_PARAM = param;
    return _taskcall(VM_PROC_NR, VM_CTL, &m);
}

