/* The kernel call implemented in this file:
 *   m_type:	SYS_MAPDMA
 *
 * The parameters for this kernel call are:
 *    m5_l1:	CP_SRC_ADDR	(virtual address)	
 *    m5_l3:	CP_NR_BYTES	(size of datastructure) 	
 */

#include "../system.h"

/*==========================================================================*
 *				do_mapdma				    *
 *==========================================================================*/
PUBLIC int do_mapdma(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
	int r;
	endpoint_t proc_e;
	int proc_p;
	vir_bytes base, size;
	phys_bytes phys_base;
	struct proc *proc;
	message m;

	proc_e = m_ptr->CP_SRC_ENDPT;
	base= m_ptr->CP_SRC_ADDR;
	size= m_ptr->CP_NR_BYTES;

	if (!isokendpt(proc_e, &proc_p))
		return(EINVAL);

	proc = proc_addr(proc_p);

        phys_base= umap_virtual(proc, D, base, size);
        if (!phys_base)
        {
                kprintf("do_mapdma: umap_virtual failed\n");
		return EFAULT;
	}

	m_ptr->CP_DST_ADDR = phys_base;
	return OK;
}
