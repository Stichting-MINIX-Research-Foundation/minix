#include "syslib.h"

/*===========================================================================*
 *                                sys_umap				     *
 *===========================================================================*/
int sys_umap(endpoint_t proc_ep, int seg, vir_bytes vir_addr, vir_bytes bytes, phys_bytes *phys_addr)
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

