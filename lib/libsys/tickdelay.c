#include "sysutil.h"
#include <minix/timers.h>

/*===========================================================================*
 *                               tickdelay			    	     *
 *===========================================================================*/
int tickdelay(clock_t ticks)
{
/* This function uses the synchronous alarm to delay for a while. This works
 * even if a previous synchronous alarm was scheduled, because the remaining
 * tick of the previous alarm are returned so that it can be rescheduled.
 * Note however that a long tick_delay (longer than the remaining time of the
 * previous) alarm will also delay the previous alarm.
 */
    message m, m_alarm;
    int s;

    if (ticks <= 0) return OK;		/* check for robustness */

    m.m_lsys_krn_sys_setalarm.exp_time = ticks;	/* request message after ticks */
    m.m_lsys_krn_sys_setalarm.abs_time = 0;	/* ticks are relative to now */
    s = _kernel_call(SYS_SETALARM, &m);
    if (s != OK) return(s);

    sef_receive(CLOCK,&m_alarm);		/* await synchronous alarm */

    /* Check if we must reschedule the current alarm. */
    if (m.m_lsys_krn_sys_setalarm.time_left > 0 &&
		m.m_lsys_krn_sys_setalarm.time_left != TMR_NEVER) {

	m.m_lsys_krn_sys_setalarm.exp_time =
		m.m_lsys_krn_sys_setalarm.time_left - ticks;

	if (m.m_lsys_krn_sys_setalarm.exp_time <= 0)
		m.m_lsys_krn_sys_setalarm.exp_time = 1;
    	s = _kernel_call(SYS_SETALARM, &m);
    }

    return(s);
}
