#include <minix/timers.h>

/*
 * Use the current time to check the timers queue list for expired timers.
 * Run the watchdog functions for all expired timers and deactivate them.
 * The caller is responsible for scheduling a new alarm if needed.
 */
int
tmrs_exptimers(minix_timer_t ** tmrs, clock_t now, clock_t * new_head)
{
	minix_timer_t *tp;
	tmr_func_t func;

	while ((tp = *tmrs) != NULL && tmr_has_expired(tp, now)) {
		*tmrs = tp->tmr_next;

		func = tp->tmr_func;
		tp->tmr_func = NULL;

		(*func)(tp->tmr_arg);
	}

	if (*tmrs != NULL) {
		if (new_head != NULL)
			*new_head = (*tmrs)->tmr_exp_time;
		return TRUE;
	} else
		return FALSE;
}
