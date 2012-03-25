/* Watchdog timer management. These functions in this file provide a
 * convenient interface to the timers library that manages a list of
 * watchdog timers. All details of scheduling an alarm at the CLOCK task
 * are hidden behind this interface.
 *
 * The entry points into this file are:
 *   init_timer:     initialize a timer structure
 *   set_timer:      reset and existing or set a new watchdog timer
 *   cancel_timer:   remove a timer from the list of timers
 *   expire_timers:  check for expired timers and run watchdog functions
 *
 */

#include "syslib.h"
#include <timers.h>
#include <minix/sysutil.h>

static timer_t *timers = NULL;
static int expiring = 0;

/*===========================================================================*
 *                              init_timer                                   *
 *===========================================================================*/
void init_timer(timer_t *tp)
{
        tmr_inittimer(tp);
}

/*===========================================================================*
 *                              set_timer                                    *
 *===========================================================================*/
void set_timer(timer_t *tp, int ticks, tmr_func_t watchdog, int arg)
{
        int r;
        clock_t now, prev_time = 0, next_time;

        if ((r = getuptime(&now)) != OK)
                panic("set_timer: couldn't get uptime");

        /* Set timer argument and add timer to the list. */
        tmr_arg(tp)->ta_int = arg;
        prev_time = tmrs_settimer(&timers, tp, now+ticks, watchdog, &next_time);

        /* Reschedule our synchronous alarm if necessary. */
        if (expiring == 0 && (! prev_time || prev_time > next_time)) {
                if (sys_setalarm(next_time, 1) != OK)
                        panic("set_timer: couldn't set alarm");
        }
}

/*===========================================================================*
 *                              cancel_timer                                 *
 *===========================================================================*/
void cancel_timer(timer_t *tp)
{
        clock_t next_time, prev_time;
        prev_time = tmrs_clrtimer(&timers, tp, &next_time);

        /* If the earliest timer has been removed, we have to set the alarm to
         * the next timer, or cancel the alarm altogether if the last timer
         * has been cancelled (next_time will be 0 then).
         */
        if (expiring == 0 && (prev_time < next_time || ! next_time)) {
                if (sys_setalarm(next_time, 1) != OK)
                        panic("cancel_timer: couldn't set alarm");
        }
}

/*===========================================================================*
 *                              expire_timers                                *
 *===========================================================================*/
void expire_timers(clock_t now)
{
        clock_t next_time;

        /* Check for expired timers. Use a global variable to indicate that
         * watchdog functions are called, so that sys_setalarm() isn't called
         * more often than necessary when set_timer or cancel_timer are called
         * from these watchdog functions. */
        expiring = 1;
        tmrs_exptimers(&timers, now, &next_time);
        expiring = 0;

        /* Reschedule an alarm if necessary. */
        if (next_time > 0) {
                if (sys_setalarm(next_time, 1) != OK)
                        panic("expire_timers: couldn't set alarm");
        }
}
