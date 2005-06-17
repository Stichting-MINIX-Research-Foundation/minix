#include "timers.h"

/*===========================================================================*
 *				tmrs_clrtimer				     *
 *===========================================================================*/
clock_t tmrs_clrtimer(tmrs, tp, new_head)
timer_t **tmrs;				/* pointer to timers queue */
timer_t *tp;				/* timer to be removed */
clock_t *new_head;
{
/* Deactivate a timer and remove it from the timers queue. 
 */
  timer_t **atp;
  struct proc *p;
  clock_t old_head = 0;

  if(*tmrs)
  	old_head = (*tmrs)->tmr_exp_time;
  else
  	old_head = 0;

  tp->tmr_exp_time = TMR_NEVER;

  for (atp = tmrs; *atp != NULL; atp = &(*atp)->tmr_next) {
	if (*atp == tp) {
		*atp = tp->tmr_next;
		break;
	}
  }

  if(new_head) {
  	if(*tmrs)
  		*new_head = (*tmrs)->tmr_exp_time;
  	else	
  		*new_head = 0;
  }

  return old_head;
}

