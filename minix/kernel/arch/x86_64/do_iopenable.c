/* The system call implemented in this file:
 *   m_type:	SYS_IOPENABLE
 *
 * The parameters for this system call are:
 *   m_lsys_krn_sys_iopenable.endpt	(process to give I/O Protection Level bits)
 *
 * Author:
 *    Jorrit N. Herder <jnherder@cs.vu.nl>
 */

#include "kernel/system.h"
#include <minix/endpoint.h>

#include "arch_proto.h"

/*===========================================================================*
 *			        do_iopenable				     *
 *===========================================================================*/
int do_iopenable(struct proc * caller, message * m_ptr)
{
  int proc_nr;

#if 1 /* ENABLE_USERPRIV && ENABLE_USERIOPL */
  if (m_ptr->m_lsys_krn_sys_iopenable.endpt == SELF) {
	okendpt(caller->p_endpoint, &proc_nr);
  } else if(!isokendpt(m_ptr->m_lsys_krn_sys_iopenable.endpt, &proc_nr))
	return(EINVAL);
  enable_iop(proc_addr(proc_nr));
  return(OK);
#else
  return(EPERM);
#endif
}


