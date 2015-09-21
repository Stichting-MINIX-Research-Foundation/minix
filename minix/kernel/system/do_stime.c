/* The kernel call implemented in this file:
 *   m_type:	SYS_STIME
 *
 * The parameters for this kernel call are:
 *   m_lsys_krn_sys_stime.boot_time
 */

#include "kernel/system.h"

#include <minix/endpoint.h>

/*===========================================================================*
 *				do_stime				     *
 *===========================================================================*/
int do_stime(struct proc * caller, message * m_ptr)
{
  set_boottime(m_ptr->m_lsys_krn_sys_stime.boot_time);
  return(OK);
}
