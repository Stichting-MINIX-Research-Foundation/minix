#include "syslib.h"

/*===========================================================================*
 *                                sys_umap				     *
 *===========================================================================*/
PUBLIC int sys_umap(proc_nr, seg, vir_addr, bytes, phys_addr)
int proc_nr; 				/* process number to do umap for */
int seg;				/* T, D, or S segment */
vir_bytes vir_addr;			/* address in bytes with segment*/
vir_bytes bytes;			/* number of bytes to be copied */
phys_bytes *phys_addr;			/* placeholder for result */
{
    message m;
    int result;

    m.CP_SRC_PROC_NR = proc_nr;
    m.CP_SRC_SPACE = seg;
    m.CP_SRC_ADDR = vir_addr;
    m.CP_NR_BYTES = bytes;

    result = _taskcall(SYSTASK, SYS_UMAP, &m);
    *phys_addr = m.CP_DST_ADDR;
    return(result);
}

