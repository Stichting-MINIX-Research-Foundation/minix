/* The system call implemented in this file:
 *   m_type:	SYS_IOPENABLE
 *
 * The parameters for this system call are:
 *    m2_i2:	PROC_NR		(process to give I/O Protection Level bits)
 *
 * Author:
 *    Jorrit N. Herder <jnherder@cs.vu.nl>
 */

#include "../kernel.h"
#include "../system.h"

/*===========================================================================*
 *			        do_iopenable				     *
 *===========================================================================*/
PUBLIC int do_iopenable(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
#if ENABLE_USERPRIV && ENABLE_USERIOPL
  enable_iop(proc_addr(m_ptr->PROC_NR)); 
  return(OK);
#else
  return(EPERM);
#endif
}


