/* The kernel call implemented in this file:
 *   m_type:	SYS_SETTIME
 *
 * The parameters for this kernel call are:
 *    m4_l2:	T_SETTIME_NOW
 *    m4_l3:	T_CLOCK_ID
 *    m4_l4:	T_TIME_SEC
 *    m4_l5:	T_TIME_NSEC
 */

#include "kernel/system.h"
#include <minix/endpoint.h>
#include <time.h>

/*===========================================================================*
 *				do_settime				     *
 *===========================================================================*/
int do_settime(struct proc * caller, message * m_ptr)
{
  clock_t newclock, ticks;
  time_t timediff;

  if (m_ptr->T_CLOCK_ID != CLOCK_REALTIME) /* only realtime can change */
	return EINVAL;

  if (m_ptr->T_SETTIME_NOW == 0) { /* user just wants to adjtime() */
	/* convert delta value from seconds and nseconds to ticks */
	ticks = (m_ptr->T_TIME_SEC * system_hz) +
				(m_ptr->T_TIME_NSEC/(1000000000/system_hz));
	set_adjtime_delta(ticks);
	return(OK);
  } /* else user wants to set the time */

  /* prevent a negative value for realtime */
  if (m_ptr->T_TIME_SEC <= boottime) {
  	/* boottime was likely wrong, try to correct it. */
	boottime = m_ptr->T_TIME_SEC;
	set_realtime(1);
	return(OK);
  }

  /* calculate the new value of realtime in ticks */
  timediff = m_ptr->T_TIME_SEC - boottime;
  newclock = (timediff*system_hz) + (m_ptr->T_TIME_NSEC/(1000000000/system_hz));

  set_realtime(newclock);

  return(OK);
}
