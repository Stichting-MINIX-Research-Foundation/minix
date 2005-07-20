/* PM watchdog timer management. 
 */

#include "pm.h"

#define VERBOSE 0

#include <timers.h>
#include <minix/syslib.h>
#include <minix/com.h>

PRIVATE timer_t *pm_timers = NULL;

PUBLIC void pm_set_timer(timer_t *tp, int ticks, tmr_func_t watchdog, int arg)
{
	int r;
	clock_t now, prev_time = 0, next_time;

	if((r = getuptime(&now)) != OK)
		panic(__FILE__, "PM couldn't get uptime from system task.", NO_NUM);

	/* Set timer argument. */
	tmr_arg(tp)->ta_int = arg;

	prev_time = tmrs_settimer(&pm_timers, tp, now+ticks, watchdog, &next_time);

	/* reschedule our synchronous alarm if necessary */
	if(! prev_time || prev_time > next_time) {
		if(sys_syncalrm(SELF, next_time, 1) != OK)
			panic(__FILE__, "PM set timer couldn't set synchronous alarm.", NO_NUM);
#if VERBOSE
		else
			printf("timers: after setting, set synalarm to %d -> %d\n", prev_time, next_time);
#endif
	}

	return;
}

PUBLIC void pm_expire_timers(clock_t now)
{
	clock_t next_time;
	tmrs_exptimers(&pm_timers, now, &next_time);
	if(next_time > 0) {
		if(sys_syncalrm(SELF, next_time, 1) != OK)
			panic(__FILE__, "PM expire timer couldn't set synchronous alarm.", NO_NUM);
#if VERBOSE
		else
			printf("timers: after expiry, set synalarm to %d\n", next_time);
#endif
	}
#if VERBOSE
	else printf("after expiry, no new timer set\n");
#endif
}

PUBLIC void pm_cancel_timer(timer_t *tp)
{
	clock_t next_time, prev_time;
	prev_time = tmrs_clrtimer(&pm_timers, tp, &next_time);

	/* if the earliest timer has been removed, we have to set
	 * the synalarm to the next timer, or cancel the synalarm
	 * altogether if th last time has been cancelled (next_time
	 * will be 0 then).
	 */
	if(prev_time < next_time || ! next_time) {
		if(sys_syncalrm(SELF, next_time, 1) != OK)
			panic(__FILE__, "PM expire timer couldn't set synchronous alarm.", NO_NUM);
#if VERBOSE
		printf("timers: after cancelling, set synalarm to %d -> %d\n", prev_time, next_time);
#endif
	}
#if VERBOSE
	else printf("timers: after cancelling no new timer\n");
#endif
}
