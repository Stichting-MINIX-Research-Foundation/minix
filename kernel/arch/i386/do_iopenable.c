/* The system call implemented in this file:
 *   m_type:	SYS_IOPENABLE
 *
 * The parameters for this system call are:
 *    m2_i2:	IO_ENDPT	(process to give I/O Protection Level bits)
 *
 * Author:
 *    Jorrit N. Herder <jnherder@cs.vu.nl>
 */

#include "kernel/system.h"
#include "kernel/kernel.h"
#include <minix/endpoint.h>

#include "proto.h"

/*===========================================================================*
 *			        do_iopenable				     *
 *===========================================================================*/
PUBLIC int do_iopenable(struct proc * caller, message * m_ptr)
{
  int proc_nr;

#if 1 /* ENABLE_USERPRIV && ENABLE_USERIOPL */
  if (m_ptr->IO_ENDPT == SELF) {
	proc_nr = _ENDPOINT_P(caller->p_endpoint);
  } else if(!isokendpt(m_ptr->IO_ENDPT, &proc_nr))
	return(EINVAL);
  enable_iop(proc_addr(proc_nr));
  return(OK);
#else
  return(EPERM);
#endif
}


