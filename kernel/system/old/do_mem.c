/* The system call implemented in this file:
 *   m_type:	SYS_MEM
 *
 * The parameters for this system call are:
 *    m4_l1:	MEM_CHUNK_BASE 	(memory base)	
 *    m4_l2:	MEM_CHUNK_SIZE  (memory size)	
 *    m4_l3:	MEM_TOT_SIZE 	(total memory)
 */

#include "../kernel.h"
#include "../system.h"

/*===========================================================================*
 *				do_mem					     *
 *===========================================================================*/
PUBLIC int do_mem(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
/* Return the base and size of the next chunk of memory. */

  struct memory *memp;

  for (memp = mem; memp < &mem[NR_MEMS]; ++memp) {
	m_ptr->MEM_CHUNK_BASE = memp->base;
	m_ptr->MEM_CHUNK_SIZE = memp->size;
	m_ptr->MEM_TOT_SIZE = tot_mem_size;
	memp->size = 0;
	if (m_ptr->MEM_CHUNK_SIZE != 0) break;	/* found a chunk */
  }
  return(OK);
}


