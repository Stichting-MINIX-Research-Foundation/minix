/*
 * Watchdog timer management. These functions in this file provide a
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
#include <minix/timers.h>
#include <minix/sysutil.h>

static minix_timer_t *timers = NULL;
static int expiring = FALSE;

/*
 * Initialize the timer 'tp'.
 */
void
init_timer(minix_timer_t * tp)
{

	tmr_inittimer(tp);
}

/*
 * Set the timer 'tp' to trigger 'ticks' clock ticks in the future.  When it
 * triggers, call function 'watchdog' with argument 'arg'.  The given timer
 * object must have been initialized with init_timer(3) already.  The given
 * number of ticks must be between 0 and TMRDIFF_MAX inclusive.  A ticks value
 * of zero will cause the alarm to trigger on the next clock tick.  If the
 * timer was already set, it will be canceled first.
 */
void
set_timer(minix_timer_t *tp, clock_t ticks, tmr_func_t watchdog, int arg)
{
	clock_t prev_time, next_time;
	int r, had_timers;

	if (ticks > TMRDIFF_MAX)
		panic("set_timer: ticks value too large: %u", (int)ticks);

	/* Add the timer to the list. */
	had_timers = tmrs_settimer(&timers, tp, getticks() + ticks, watchdog,
	    arg, &prev_time, &next_time);

	/* Reschedule our synchronous alarm if necessary. */
	if (!expiring && (!had_timers || next_time != prev_time)) {
		if ((r = sys_setalarm(next_time, TRUE /*abs_time*/)) != OK)
			panic("set_timer: couldn't set alarm: %d", r);
        }
}

/*
 * Cancel the timer 'tp'.  The timer object must have been initialized with
 * init_timer(3) first.  If the timer was not set before, the call is a no-op.
 */
void
cancel_timer(minix_timer_t * tp)
{
	clock_t next_time, prev_time;
	int r, have_timers;

	if (!tmr_is_set(tp))
		return;

	have_timers = tmrs_clrtimer(&timers, tp, &prev_time, &next_time);

	/*
	 * If the earliest timer has been removed, we have to set the alarm to
	 * the next timer, or cancel the alarm altogether if the last timer
	 * has been canceled.
	 */
        if (!expiring) {
		if (!have_timers)
			r = sys_setalarm(0, FALSE /*abs_time*/);
		else if (prev_time != next_time)
			r = sys_setalarm(next_time, TRUE /*abs_time*/);
		else
			r = OK;

		if (r != OK)
                        panic("cancel_timer: couldn't set alarm: %d", r);
        }
}

/*
 * Expire all timers that were set to expire before/at the given current time.
 */
void
expire_timers(clock_t now)
{
        clock_t next_time;
	int r, have_timers;

	/*
	 * Check for expired timers. Use a global variable to indicate that
	 * watchdog functions are called, so that sys_setalarm() isn't called
	 * more often than necessary when set_timer or cancel_timer are called
	 * from these watchdog functions.
	 */
	expiring = TRUE;
	have_timers = tmrs_exptimers(&timers, now, &next_time);
	expiring = FALSE;

	/* Reschedule an alarm if necessary. */
	if (have_timers) {
		if ((r = sys_setalarm(next_time, TRUE /*abs_time*/)) != OK)
			panic("expire_timers: couldn't set alarm: %d", r);
	}
}
