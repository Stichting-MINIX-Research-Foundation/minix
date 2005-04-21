#include "timers.h"

/*===========================================================================*
 *				tmrs_exptimers				     *
 *===========================================================================*/
void tmrs_exptimers(tmrs, now)
timer_t **tmrs;				/* pointer to timers queue */
clock_t now;				/* current time */
{
/* Use the current time to check the timers queue list for expired timers. 
 * Run the watchdog functions for all expired timers and deactivate them.
 * The caller is responsible for scheduling a new alarm if needed.
 */
  timer_t *tp;

  while ((tp = *tmrs) != NULL && tp->tmr_exp_time <= now) {
	*tmrs = tp->tmr_next;
	tp->tmr_exp_time = TMR_NEVER;
	(*tp->tmr_func)(tp);
  }
}


