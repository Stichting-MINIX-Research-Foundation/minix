/* The system call implemented in this file:
 *   m_type:	SYS_UMAP
 *
 * The parameters for this system call are:
 *    m5_i1:	CP_SRC_PROC_NR	(process number)	
 *    m5_c1:	CP_SRC_SPACE	(segment where address is: T, D, or S)
 *    m5_l1:	CP_SRC_ADDR	(virtual address)	
 *    m5_l2:	CP_DST_ADDR	(returns physical address)	
 *    m5_l3:	CP_NR_BYTES	(size of datastructure) 	
 */

#include "../kernel.h"
#include "../system.h"

/*==========================================================================*
 *				do_umap					    *
 *==========================================================================*/
PUBLIC int do_umap(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
/* Same as umap_local(), for non-kernel processes. */
  int proc_nr = (int) m_ptr->CP_SRC_PROC_NR;
  if (proc_nr == SELF) proc_nr = m_ptr->m_source;
  if (! isokprocn(proc_nr)) return(EINVAL);

  m_ptr->CP_DST_ADDR = umap_local(proc_addr(proc_nr),
                           (int) m_ptr->CP_SRC_SPACE,
                           (vir_bytes) m_ptr->CP_SRC_ADDR,
                           (vir_bytes) m_ptr->CP_NR_BYTES);
  return(OK);
}


