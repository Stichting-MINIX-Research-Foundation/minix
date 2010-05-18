/* SCHED watchdog timer management, based on servers/pm/timers.c.
 *
 * The entry points into this file are:
 *   sched_set_timer:      reset and existing or set a new watchdog timer
 *   sched_expire_timers:  check for expired timers and run watchdog functions
 *
 */

#include "sched.h"

#include <timers.h>
#include <minix/syslib.h>
#include <minix/com.h>

PRIVATE timer_t *sched_timers = NULL;
PRIVATE int sched_expiring = 0;

/*===========================================================================*
 *				pm_set_timer				     *
 *===========================================================================*/
PUBLIC void sched_set_timer(timer_t *tp, int ticks, tmr_func_t watchdog, int arg)
{
	int r;
	clock_t now, prev_time = 0, next_time;

	if ((r = getuptime(&now)) != OK)
		panic("SCHED couldn't get uptime");

	/* Set timer argument and add timer to the list. */
	tmr_arg(tp)->ta_int = arg;
	prev_time = tmrs_settimer(&sched_timers,tp,now+ticks,watchdog,&next_time);

	/* Reschedule our synchronous alarm if necessary. */
	if (sched_expiring == 0 && (! prev_time || prev_time > next_time)) {
		if (sys_setalarm(next_time, 1) != OK)
			panic("SCHED set timer couldn't set alarm");
	}

	return;
}

/*===========================================================================*
 *				sched_expire_timers			     *
 *===========================================================================*/
PUBLIC void sched_expire_timers(clock_t now)
{
	clock_t next_time;

	/* Check for expired timers. Use a global variable to indicate that
	 * watchdog functions are called, so that sys_setalarm() isn't called
	 * more often than necessary when sched_set_timer is
	 * called from these watchdog functions. */
	sched_expiring = 1;
	tmrs_exptimers(&sched_timers, now, &next_time);
	sched_expiring = 0;

	/* Reschedule an alarm if necessary. */
	if (next_time > 0) {
		if (sys_setalarm(next_time, 1) != OK)
			panic("SCHED expire timer couldn't set alarm");
	}
}
