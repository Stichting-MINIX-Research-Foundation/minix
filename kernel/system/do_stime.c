/* The kernel call implemented in this file:
 *   m_type:	SYS_STIME
 *
 * The parameters for this kernel call are:
 *    m4_l3:	T_BOOTTIME
 */

#include "kernel/system.h"

#include <minix/endpoint.h>

/*===========================================================================*
 *				do_stime				     *
 *===========================================================================*/
int do_stime(struct proc * caller, message * m_ptr)
{
  boottime= m_ptr->T_BOOTTIME;
  return(OK);
}
