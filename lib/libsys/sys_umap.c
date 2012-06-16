#include "syslib.h"

/*===========================================================================*
 *                                sys_umap				     *
 *===========================================================================*/
int sys_umap(proc_ep, seg, vir_addr, bytes, phys_addr)
endpoint_t proc_ep;			/* process number to do umap for */
int seg;				/* T, D, or S segment */
vir_bytes vir_addr;			/* address in bytes with segment*/
vir_bytes bytes;			/* number of bytes to be copied */
phys_bytes *phys_addr;			/* placeholder for result */
{
    message m;
    int result;

    m.CP_SRC_ENDPT = proc_ep;
    m.UMAP_SEG = seg;
    m.CP_SRC_ADDR = vir_addr;
    m.CP_NR_BYTES = bytes;

    result = _kernel_call(SYS_UMAP, &m);
    *phys_addr = m.CP_DST_ADDR;
    return(result);
}

