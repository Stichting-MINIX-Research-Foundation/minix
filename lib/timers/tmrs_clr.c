#include "timers.h"

/*===========================================================================*
 *				tmrs_clrtimer				     *
 *===========================================================================*/
void tmrs_clrtimer(tmrs, tp)
timer_t **tmrs;				/* pointer to timers queue */
timer_t *tp;				/* timer to be removed */
{
/* Deactivate a timer and remove it from the timers queue. 
 */
  timer_t **atp;
  struct proc *p;

  tp->tmr_exp_time = TMR_NEVER;

  for (atp = tmrs; *atp != NULL; atp = &(*atp)->tmr_next) {
	if (*atp == tp) {
		*atp = tp->tmr_next;
		return;
	}
  }
}

