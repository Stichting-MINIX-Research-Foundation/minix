/* The kernel call implemented in this file:
 *   m_type:	SYS_SEGCTL
 *
 * The parameters for this kernel call are:
 *    m4_l3:	SEG_PHYS	(physical base address)
 *    m4_l4:	SEG_SIZE	(size of segment)
 *    m4_l1:	SEG_SELECT	(return segment selector here)
 *    m4_l2:	SEG_OFFSET	(return offset within segment here)
 *    m4_l5:	SEG_INDEX	(return index into remote memory map here)
 */
#include "kernel/system.h"

#if USE_SEGCTL

/*===========================================================================*
 *			        do_segctl				     *
 *===========================================================================*/
PUBLIC int do_segctl(struct proc * caller, message * m_ptr)
{
/* Return a segment selector and offset that can be used to reach a physical
 * address, for use by a driver doing memory I/O in the A0000 - DFFFF range.
 */
  u32_t selector;
  vir_bytes offset;
  int i, index;
  phys_bytes phys = (phys_bytes) m_ptr->SEG_PHYS;
  vir_bytes size = (vir_bytes) m_ptr->SEG_SIZE;
  int result;

  /* First check if there is a slot available for this segment. */
  index = -1;
  for (i=0; i < NR_REMOTE_SEGS; i++) {
      if (! caller->p_priv->s_farmem[i].in_use) {
          index = i; 
          caller->p_priv->s_farmem[i].in_use = TRUE;
          caller->p_priv->s_farmem[i].mem_phys = phys;
          caller->p_priv->s_farmem[i].mem_len = size;
          break;
      }
  }
  if (index < 0) return(ENOSPC);

       offset = alloc_remote_segment(&selector, &caller->p_seg,
		i, phys, size, USER_PRIVILEGE);
       result = OK;          

  /* Request successfully done. Now return the result. */
  m_ptr->SEG_INDEX = index | REMOTE_SEG;
  m_ptr->SEG_SELECT = selector;
  m_ptr->SEG_OFFSET = offset;
  return(result);
}

#endif /* USE_SEGCTL */

