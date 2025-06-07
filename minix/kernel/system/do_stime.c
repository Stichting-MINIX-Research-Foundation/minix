/* The kernel call implemented in this file:
 *   m_type:	SYS_STIME
 *
 * The parameters for this kernel call are:
 *   m_lsys_krn_sys_stime.boot_time
 */

#include "kernel/system.h"

#include <minix/endpoint.h> // Kept

// Added kernel headers
#include <minix/kernel_types.h> // For k_time_t
#include <klib/include/kprintf.h>
#include <klib/include/kstring.h>
#include <klib/include/kmemory.h>


/*===========================================================================*
 *				do_stime				     *
 *===========================================================================*/
int do_stime(struct proc * caller, message * m_ptr)
{
  // Assuming m_ptr->m_lsys_krn_sys_stime.boot_time is compatible with k_time_t
  // or will be after message struct refactoring.
  // set_boottime itself now expects k_time_t.
  set_boottime(m_ptr->m_lsys_krn_sys_stime.boot_time);
  return(OK);
}
