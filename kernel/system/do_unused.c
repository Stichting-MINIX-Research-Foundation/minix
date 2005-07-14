
#include "../system.h"

/*===========================================================================*
 *			          do_unused				     *
 *===========================================================================*/
PUBLIC int do_unused(m)
message *m;				/* pointer to request message */
{
  kprintf("SYSTEM got unused request %d", m->m_type);
  kprintf("from %d.\n", m->m_source);
  return(EBADREQUEST);		/* illegal message type */
}


