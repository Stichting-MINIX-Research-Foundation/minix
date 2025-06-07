/* The kernel call implemented in this file:
 *   m_type:	SYS_SETTIME
 *
 * The parameters for this kernel call are:
 *   m_lsys_krn_sys_settime.now
 *   m_lsys_krn_sys_settime.clock_id
 *   m_lsys_krn_sys_settime.sec
 *   m_lsys_krn_sys_settime.nsec
 */

#include "kernel/system.h"
#include <minix/endpoint.h> // Kept
// #include <time.h> // Replaced

// Added kernel headers
#include <minix/kernel_types.h> // For k_clock_t, k_time_t, k_errno_t, CLOCK_REALTIME (if moved)
#include <klib/include/kprintf.h>
#include <klib/include/kstring.h>
#include <klib/include/kmemory.h>


/*===========================================================================*
 *				do_settime				     *
 *===========================================================================*/
int do_settime(struct proc * caller, message * m_ptr)
{
  k_clock_t newclock; // MODIFIED clock_t
  int32_t ticks; // int32_t might be undefined
  k_time_t boottime, timediff, timediff_ticks; // MODIFIED time_t

  /* only realtime can change */
  if (m_ptr->m_lsys_krn_sys_settime.clock_id != CLOCK_REALTIME) // CLOCK_REALTIME might be undefined
	return EINVAL; // EINVAL might be undefined

  /* user just wants to adjtime() */
  if (m_ptr->m_lsys_krn_sys_settime.now == 0) {
	/* convert delta value from seconds and nseconds to ticks */
	ticks = (m_ptr->m_lsys_krn_sys_settime.sec * system_hz) +
		(m_ptr->m_lsys_krn_sys_settime.nsec/(1000000000/system_hz));
	set_adjtime_delta(ticks);
	return(OK);
  } /* else user wants to set the time */

  boottime = get_boottime();

  timediff = m_ptr->m_lsys_krn_sys_settime.sec - boottime;
  timediff_ticks = timediff * system_hz;

  /* prevent a negative value for realtime */
  // LONG_MIN, LONG_MAX might be undefined (from limits.h)
  if (m_ptr->m_lsys_krn_sys_settime.sec <= boottime ||
      timediff_ticks < (-0x7FFFFFFFEL-1) / 2 /* LONG_MIN/2 placeholder */ ||
      timediff_ticks > 0x7FFFFFFFEL / 2 /* LONG_MAX/2 placeholder */) {
  	/* boottime was likely wrong, try to correct it. */
	set_boottime(m_ptr->m_lsys_krn_sys_settime.sec);
	set_realtime(1);
	return(OK);
  }

  /* calculate the new value of realtime in ticks */
  newclock = timediff_ticks +
      (m_ptr->m_lsys_krn_sys_settime.nsec/(1000000000/system_hz));

  set_realtime(newclock);

  return(OK);
}
