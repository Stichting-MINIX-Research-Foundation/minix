
#include "syslib.h"

#include <minix/vm.h>

/*===========================================================================*
 *                                vm_umap				     *
 *===========================================================================*/
int vm_umap(int seg, vir_bytes offset, vir_bytes len, phys_bytes *addr)
{
    message m;
    int result;

    m.VMU_SEG = seg;
    m.VMU_OFFSET = (char *) offset;
    m.VMU_LENGTH = (char *) len;
    result = _taskcall(VM_PROC_NR, VM_UMAP, &m);
    *addr = (phys_bytes) m.VMU_RETADDR;

    return result;
}

