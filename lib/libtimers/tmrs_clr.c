#include "timers.h"

/*===========================================================================*
 *				tmrs_clrtimer				     *
 *===========================================================================*/
clock_t tmrs_clrtimer(tmrs, tp, next_time)
timer_t **tmrs;				/* pointer to timers queue */
timer_t *tp;				/* timer to be removed */
clock_t *next_time;
{
/* Deactivate a timer and remove it from the timers queue. 
 */
  timer_t **atp;
  struct proc *p;
  clock_t prev_time;

  if(*tmrs)
  	prev_time = (*tmrs)->tmr_exp_time;
  else
  	prev_time = 0;

  tp->tmr_exp_time = TMR_NEVER;

  for (atp = tmrs; *atp != NULL; atp = &(*atp)->tmr_next) {
	if (*atp == tp) {
		*atp = tp->tmr_next;
		break;
	}
  }

  if(next_time) {
  	if(*tmrs)
  		*next_time = (*tmrs)->tmr_exp_time;
  	else	
  		*next_time = 0;
  }

  return prev_time;
}

