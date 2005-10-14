/* This file provides a catch-all handler for unused kernel calls. A kernel 
 * call may be unused when it is not defined or when it is disabled in the
 * kernel's configuration.
 */
#include "../system.h"

/*===========================================================================*
 *			          do_unused				     *
 *===========================================================================*/
PUBLIC int do_unused(m)
message *m;				/* pointer to request message */
{
  kprintf("SYSTEM: got unused request %d from %d", m->m_type, m->m_source);
  return(EBADREQUEST);			/* illegal message type */
}

