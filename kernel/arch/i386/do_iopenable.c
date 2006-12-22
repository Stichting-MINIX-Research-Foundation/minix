/* The system call implemented in this file:
 *   m_type:	SYS_IOPENABLE
 *
 * The parameters for this system call are:
 *    m2_i2:	IO_ENDPT	(process to give I/O Protection Level bits)
 *
 * Author:
 *    Jorrit N. Herder <jnherder@cs.vu.nl>
 */

#include "../../system.h"
#include "../../kernel.h"

#include "proto.h"

/*===========================================================================*
 *			        do_iopenable				     *
 *===========================================================================*/
PUBLIC int do_iopenable(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
  int proc_nr;

#if 1 /* ENABLE_USERPRIV && ENABLE_USERIOPL */
  if (m_ptr->IO_ENDPT == SELF) {
	proc_nr = who_p;
  } else if(!isokendpt(m_ptr->IO_ENDPT, &proc_nr))
	return(EINVAL);
  enable_iop(proc_addr(proc_nr));
  return(OK);
#else
  return(EPERM);
#endif
}


