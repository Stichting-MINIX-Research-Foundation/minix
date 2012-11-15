#include "sysutil.h"
#include <timers.h>

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

    m.ALRM_EXP_TIME = ticks;		/* request message after ticks */
    m.ALRM_ABS_TIME = 0;		/* ticks are relative to now */
    s = _kernel_call(SYS_SETALARM, &m);
    if (s != OK) return(s);

    sef_receive(CLOCK,&m_alarm);		/* await synchronous alarm */

    /* Check if we must reschedule the current alarm. */
    if (m.ALRM_TIME_LEFT > 0 && m.ALRM_TIME_LEFT != TMR_NEVER) {
    	m.ALRM_EXP_TIME = m.ALRM_TIME_LEFT - ticks;
    	if (m.ALRM_EXP_TIME <= 0) 
    		m.ALRM_EXP_TIME = 1;
    	s = _kernel_call(SYS_SETALARM, &m);
    }

    return(s);
}





