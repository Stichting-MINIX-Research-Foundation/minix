#include <minix/timers.h>

/*
 * Deactivate a timer and remove it from the timers queue.  'tmrs' is a pointer
 * to the timers queue.  'tp' is a pointer to the timer to be removed, which
 * generally should be on the queue (but this is not a requirement, and the
 * kernel abuses this).  If 'prev_time' is non-NULL, it is filled with the
 * previous timer head time, which always exists since at least 'tp' is on it.
 * The function returns TRUE if there is still at least one timer on the queue
 * after this function is done, in which case 'next_time' (if non-NULL) is
 * filled with the absolute expiry time of the new head timer.
 */
int
tmrs_clrtimer(minix_timer_t ** tmrs, minix_timer_t * tp, clock_t * prev_time,
	clock_t * next_time)
{
	minix_timer_t **atp;
	int r;

	if (*tmrs != NULL) {
		if (prev_time != NULL)
			*prev_time = (*tmrs)->tmr_exp_time;
		r = TRUE;
	} else
		r = FALSE;

	tp->tmr_func = NULL;	/* clear the timer object */

	for (atp = tmrs; *atp != NULL; atp = &(*atp)->tmr_next) {
		if (*atp == tp) {
			*atp = tp->tmr_next;
			break;
		}
	}

	if (next_time != NULL) {
		if (*tmrs != NULL)
			*next_time = (*tmrs)->tmr_exp_time;
		else
			*next_time = 0;
	}

	return r;
}
