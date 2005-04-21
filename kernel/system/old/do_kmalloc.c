/* The system call implemented in this file:
 *   m_type:	SYS_KMALLOC
 *
 * The parameters for this system call are:
 *    m4_l2:	MEM_CHUNK_SIZE	(request a buffer of this size)
 *    m4_l1:	MEM_CHUNK_BASE 	(return physical address on success)	
 *
 * Author:
 *    Jorrit N. Herder <jnherder@cs.vu.nl>
 */

#include "../kernel.h"
#include "../system.h"

/*===========================================================================*
 *			        do_kmalloc				     *
 *===========================================================================*/
PUBLIC int do_kmalloc(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
/* Request a (DMA) buffer to be allocated in one of the memory chunks. */
  phys_clicks tot_clicks;
  struct memory *memp;
  
  tot_clicks = (m_ptr->MEM_CHUNK_SIZE + CLICK_SIZE-1) >> CLICK_SHIFT;
  memp = &mem[NR_MEMS];
  while ((--memp)->size < tot_clicks) {
      if (memp == mem) {
          return(ENOMEM);
      }
  }
  memp->size -= tot_clicks;
  m_ptr->MEM_CHUNK_BASE = (memp->base + memp->size) << CLICK_SHIFT; 
  return(OK);
}

