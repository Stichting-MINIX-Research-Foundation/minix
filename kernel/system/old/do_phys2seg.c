/* The system call implemented in this file:
 *   m_type:	SYS_PHYS2SEG
 *
 * The parameters for this system call are:
 *    m4_l1:	SEG_SELECT	(return segment selector here)
 *    m4_l2:	SEG_OFFSET	(return offset within segment here)
 *    m4_l3:	SEG_PHYS	(physical address to convert)
 *    m4_l4:	SEG_SIZE	(size of segment)
 *
 * Author:
 *    Jorrit N. Herder <jnherder@cs.vu.nl>
 */

#include "../kernel.h"
#include "../system.h"
#include "../protect.h"


/*===========================================================================*
 *			        do_phys2seg				     *
 *===========================================================================*/
PUBLIC int do_phys2seg(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
/* Return a segment selector and offset that can be used to reach a physical
 * address, for use by a driver doing memory I/O in the A0000 - DFFFF range.
 */
  u16_t selector;
  vir_bytes offset;
  register struct proc *rp;
  phys_bytes phys = (phys_bytes) m_ptr->SEG_PHYS;
  vir_bytes size = (vir_bytes) m_ptr->SEG_SIZE;
  int result;

#if 0
  kprintf("FLAT DS SELECTOR used\n", NO_ARG);
  selector = FLAT_DS_SELECTOR;
  offset = phys;
#else
  kprintf("Using Experimental LDT selector for video memory\n", NO_ARG);

  if (!protected_mode) {
      selector = phys / HCLICK_SIZE;
      offset = phys % HCLICK_SIZE;
      result = OK;
  } else {
      /* Check if the segment size can be recorded in bytes, that is, check
       * if descriptor's limit field can delimited the allowed memory region
       * precisely. This works up to 1MB. If the size is larger, 4K pages
       * instead of bytes are used.
       */
      if (size < BYTE_GRAN_MAX) {
          rp = proc_addr(m_ptr->m_source);
          init_dataseg(&rp->p_ldt[EXTRA_LDT_INDEX], phys, size, 
          	USER_PRIVILEGE);
          selector = (EXTRA_LDT_INDEX * 0x08) | (1 * 0x04) | USER_PRIVILEGE;
          offset = 0;
          result = OK;			
      } else {
#if ENABLE_USERPRIV && ENABLE_LOOSELDT
          rp = proc_addr(m_ptr->m_source);
          init_dataseg(&rp->p_ldt[EXTRA_LDT_INDEX], phys & ~0xFFFF, 0, 
          	USER_PRIVILEGE);
          selector = (EXTRA_LDT_INDEX * 0x08) | (1 * 0x04) | USER_PRIVILEGE;
          offset = phys & 0xFFFF;
          result = OK;
#else
      	  result = E2BIG;		/* allow settings only */
#endif
      }
  }
#endif

#if 0
	kprintf("do_phys2seg: proc %d", m_ptr->m_source);
	kprintf(" phys %u", phys);
	kprintf(" size %u", size);
	kprintf(" sel %u", selector);
	kprintf(" off %u\n", offset);
#endif
  m_ptr->SEG_SELECT = selector;
  m_ptr->SEG_OFFSET = offset;
  return(result);
}


