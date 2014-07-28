#include "syslib.h"

/*===========================================================================*
 *                                sys_umap_remote			     *
 *===========================================================================*/
int sys_umap_remote(proc_ep, grantee, seg, vir_addr, bytes, phys_addr)
endpoint_t proc_ep;			/* process number to do umap for */
endpoint_t grantee;			/* process nr to check as grantee */
int seg;				/* T, D, or S segment */
vir_bytes vir_addr;			/* address in bytes with segment*/
vir_bytes bytes;			/* number of bytes to be copied */
phys_bytes *phys_addr;			/* placeholder for result */
{
    message m;
    int result;

    /* Note about the grantee parameter:
     * - Is ignored for non-grant umap calls, but should be SELF to
     *   pass the sanity check in that case;
     * - May be SELF to get the same behaviour as sys_umap, namely that the
     *   caller must be the grantee;
     * - In all other cases, should be a valid endpoint (neither ANY nor NONE).
     */

    m.m_lsys_krn_sys_umap.src_endpt = proc_ep;
    m.m_lsys_krn_sys_umap.dst_endpt = grantee;
    m.m_lsys_krn_sys_umap.segment = seg;
    m.m_lsys_krn_sys_umap.src_addr = vir_addr;
    m.m_lsys_krn_sys_umap.nr_bytes = bytes;

    result = _kernel_call(SYS_UMAP_REMOTE, &m);
    *phys_addr = m.m_krn_lsys_sys_umap.dst_addr;
    return(result);
}

