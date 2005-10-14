/* The kernel call implemented in this file:
 *   m_type:	SYS_UMAP
 *
 * The parameters for this kernel call are:
 *    m5_i1:	CP_SRC_PROC_NR	(process number)	
 *    m5_c1:	CP_SRC_SPACE	(segment where address is: T, D, or S)
 *    m5_l1:	CP_SRC_ADDR	(virtual address)	
 *    m5_l2:	CP_DST_ADDR	(returns physical address)	
 *    m5_l3:	CP_NR_BYTES	(size of datastructure) 	
 */

#include "../system.h"

#if USE_UMAP

/*==========================================================================*
 *				do_umap					    *
 *==========================================================================*/
PUBLIC int do_umap(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
/* Map virtual address to physical, for non-kernel processes. */
  int seg_type = m_ptr->CP_SRC_SPACE & SEGMENT_TYPE;
  int seg_index = m_ptr->CP_SRC_SPACE & SEGMENT_INDEX;
  vir_bytes offset = m_ptr->CP_SRC_ADDR;
  int count = m_ptr->CP_NR_BYTES;
  int proc_nr = (int) m_ptr->CP_SRC_PROC_NR;
  phys_bytes phys_addr;

  /* Verify process number. */
  if (proc_nr == SELF) proc_nr = m_ptr->m_source;
  if (! isokprocn(proc_nr)) return(EINVAL);

  /* See which mapping should be made. */
  switch(seg_type) {
  case LOCAL_SEG:
      phys_addr = umap_local(proc_addr(proc_nr), seg_index, offset, count); 
      break;
  case REMOTE_SEG:
      phys_addr = umap_remote(proc_addr(proc_nr), seg_index, offset, count); 
      break;
  case BIOS_SEG:
      phys_addr = umap_bios(proc_addr(proc_nr), offset, count); 
      break;
  default:
      return(EINVAL);
  }
  m_ptr->CP_DST_ADDR = phys_addr;
  return (phys_addr == 0) ? EFAULT: OK;
}

#endif /* USE_UMAP */
