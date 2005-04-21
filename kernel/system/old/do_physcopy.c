/* The system call implemented in this file:
 *   m_type:	SYS_PHYSCOPY
 *
 * The parameters for this system call are:
 *    m5_l1:	CP_SRC_ADDR	(physical source address)	
 *    m5_l2:	CP_DST_ADDR	(physical destination address)	
 *    m5_l3:	CP_NR_BYTES	(number of bytes to copy)
 * 
 * Author:
 *    Jorrit N. Herder <jnherder@cs.vu.nl>
 */

#include "../kernel.h"
#include "../system.h"

/*===========================================================================*
 *				do_physcopy					     *
 *===========================================================================*/
PUBLIC int do_physcopy(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
/* Handle sys_physcopy().  Copy data by using physical addressing. */

  phys_bytes src_phys, dst_phys, bytes;

  /* Dismember the command message. */
  src_phys = (phys_bytes) m_ptr->CP_SRC_ADDR;
  dst_phys = (phys_bytes) m_ptr->CP_DST_ADDR;
  bytes = (phys_bytes) m_ptr->CP_NR_BYTES;

  /* Do some checks and copy the data. */
  if (src_phys == 0 || dst_phys == 0)  return(EFAULT);
  phys_copy(src_phys, dst_phys, bytes);
  return(OK);
}


