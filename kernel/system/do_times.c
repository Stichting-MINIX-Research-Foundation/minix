/* The kernel call implemented in this file:
 *   m_type:	SYS_TIMES
 *
 * The parameters for this kernel call are:
 *    m4_l1:	T_ENDPT		(get info for this process)	
 *    m4_l1:	T_USER_TIME		(return values ...)	
 *    m4_l2:	T_SYSTEM_TIME	
 *    m4_l3:	T_BOOTTIME
 *    m4_l5:	T_BOOT_TICKS	
 */

#include "kernel/system.h"

#include <minix/endpoint.h>

#if USE_TIMES

/*===========================================================================*
 *				do_times				     *
 *===========================================================================*/
int do_times(struct proc * caller, message * m_ptr)
{
/* Handle sys_times().  Retrieve the accounting information. */
  register const struct proc *rp;
  int proc_nr;
  endpoint_t e_proc_nr;

  /* Insert the times needed by the SYS_TIMES kernel call in the message. 
   * The clock's interrupt handler may run to update the user or system time
   * while in this code, but that cannot do any harm.
   */
  e_proc_nr = (m_ptr->T_ENDPT == SELF) ? caller->p_endpoint : m_ptr->T_ENDPT;
  if(e_proc_nr != NONE && isokendpt(e_proc_nr, &proc_nr)) {
      rp = proc_addr(proc_nr);
      m_ptr->T_USER_TIME   = rp->p_user_time;
      m_ptr->T_SYSTEM_TIME = rp->p_sys_time;
  }
  m_ptr->T_BOOT_TICKS = get_monotonic();  
  m_ptr->T_REAL_TICKS = get_realtime();
  m_ptr->T_BOOTTIME = boottime;  
  return(OK);
}

#endif /* USE_TIMES */

