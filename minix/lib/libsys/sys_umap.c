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

    m.m_lsys_krn_sys_umap.src_endpt = proc_ep;
    m.m_lsys_krn_sys_umap.segment = seg;
    m.m_lsys_krn_sys_umap.src_addr = vir_addr;
    m.m_lsys_krn_sys_umap.nr_bytes = bytes;

    result = _kernel_call(SYS_UMAP, &m);
    *phys_addr = m.m_krn_lsys_sys_umap.dst_addr;
    return(result);
}

