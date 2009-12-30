/* The kernel call implemented in this file:
 *   m_type:	SYS_STIME
 *
 * The parameters for this kernel call are:
 *    m4_l3:	T_BOOTTIME
 */

#include "../system.h"

#include <minix/endpoint.h>

/*===========================================================================*
 *				do_stime				     *
 *===========================================================================*/
PUBLIC int do_stime(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
  boottime= m_ptr->T_BOOTTIME;
  return(OK);
}
