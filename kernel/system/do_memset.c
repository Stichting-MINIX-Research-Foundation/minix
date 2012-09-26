/* The kernel call implemented in this file:
 *   m_type:	SYS_MEMSET
 *
 * The parameters for this kernel call are:
 *    m2_p1:	MEM_PTR		(virtual address)	
 *    m2_l1:	MEM_COUNT	(returns physical address)	
 *    m2_l2:	MEM_PATTERN	(pattern byte to be written) 	
 */

#include "kernel/system.h"

#if USE_MEMSET

/*===========================================================================*
 *				do_memset				     *
 *===========================================================================*/
int do_memset(struct proc * caller, message * m_ptr)
{
/* Handle sys_memset(). This writes a pattern into the specified memory. */
  vm_memset(caller, m_ptr->MEM_PROCESS, (phys_bytes) m_ptr->MEM_PTR,
	m_ptr->MEM_PATTERN, (phys_bytes) m_ptr->MEM_COUNT);
  return(OK);
}

#endif /* USE_MEMSET */

