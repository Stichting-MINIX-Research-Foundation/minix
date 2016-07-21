#include <minix/timers.h>

/*
 * Activate a timer to run function 'watchdog' at absolute time 'exp_time', as
 * part of timers queue 'tmrs'. If the timer is already in use, it is first
 * removed from the timers queue.  Then, it is put in the list of active timers
 * with the first to expire in front.  The caller responsible for scheduling a
 * new alarm for the timer if needed.  To that end, the function returns three
 * values: its return value (TRUE or FALSE) indicates whether there was an old
 * head timer; if TRUE, 'old_head' (if non-NULL) is filled with the absolute
 * expiry time of the old head timer.  If 'new_head' is non-NULL, it is filled
 * with the absolute expiry time of the new head timer.
 */
int
tmrs_settimer(minix_timer_t ** tmrs, minix_timer_t * tp, clock_t exp_time,
	tmr_func_t watchdog, int arg, clock_t * old_head, clock_t * new_head)
{
	minix_timer_t **atp;
	int r;

	if (*tmrs != NULL) {
		if (old_head != NULL)
			*old_head = (*tmrs)->tmr_exp_time;
		r = TRUE;
	} else
		r = FALSE;

	/* Set the timer's variables. */
	if (tmr_is_set(tp))
		(void)tmrs_clrtimer(tmrs, tp, NULL, NULL);
	tp->tmr_exp_time = exp_time;
	tp->tmr_func = watchdog;	/* set the timer object */
	tp->tmr_arg = arg;

	/*
	 * Add the timer to the active timers. The next timer due is in front.
	 */
	for (atp = tmrs; *atp != NULL; atp = &(*atp)->tmr_next) {
		if (tmr_is_first(exp_time, (*atp)->tmr_exp_time))
			break;
	}
	tp->tmr_next = *atp;
	*atp = tp;

	if (new_head != NULL)
		*new_head = (*tmrs)->tmr_exp_time;
	return r;
}
