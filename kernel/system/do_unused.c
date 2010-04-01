/* This file provides a catch-all handler for unused kernel calls. A kernel 
 * call may be unused when it is not defined or when it is disabled in the
 * kernel's configuration.
 */
#include "kernel/system.h"

/*===========================================================================*
 *			          do_unused				     *
 *===========================================================================*/
PUBLIC int do_unused(struct proc * caller, message * m_ptr)
{
  printf("SYSTEM: got unused request %d from %d\n",
		  m_ptr->m_type, m_ptr->m_source);
  return(EBADREQUEST);			/* illegal message type */
}

