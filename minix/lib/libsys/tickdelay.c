#include "sysutil.h"
#include <minix/timers.h>

/*===========================================================================*
 *                               tickdelay			    	     *
 *===========================================================================*/
int tickdelay(clock_t ticks)
{
/* This function uses the synchronous alarm to delay for a while. This works
 * even if a previous synchronous alarm was scheduled, because the remaining
 * ticks of the previous alarm are returned so that it can be rescheduled.
 * Note however that a long tick delay (longer than the remaining time of the
 * previous) alarm will also delay the previous alarm.
 */
    clock_t time_left, uptime;
    message m;
    int r, status;

    if (ticks <= 0) return OK;		/* check for robustness */

    /* Set the new alarm while getting the time left on the previous alarm. */
    if ((r = sys_setalarm2(ticks, FALSE, &time_left, &uptime)) != OK)
	return r;

    /* Await synchronous alarm.  Since an alarm notification may already have
     * been dispatched by the time that we set the new alarm, we keep going
     * until we actually receive an alarm with a timestamp no earlier than the
     * alarm time we expect.
     */
    while ((r = ipc_receive(CLOCK, &m, &status)) == OK) {
	if (m.m_type == NOTIFY_MESSAGE &&
	    m.m_notify.timestamp >= uptime + ticks)
		break;
    }

    /* Check if we must reschedule the previous alarm. */
    if (time_left != TMR_NEVER) {
	if (time_left > ticks)
		time_left -= ticks;
	else
		time_left = 1; /* force an alarm */

	/* There's no point in returning an error from here.. */
	(void)sys_setalarm(time_left, FALSE);
    }

    return r;
}
