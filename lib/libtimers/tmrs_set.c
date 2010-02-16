#include "timers.h"

/*===========================================================================*
 *				tmrs_settimer				     *
 *===========================================================================*/
clock_t tmrs_settimer(tmrs, tp, exp_time, watchdog, new_head)
timer_t **tmrs;				/* pointer to timers queue */
timer_t *tp;				/* the timer to be added */
clock_t exp_time;			/* its expiration time */
tmr_func_t watchdog;			/* watchdog function to be run */
clock_t *new_head;			/* new earliest timer, if non NULL */
{
/* Activate a timer to run function 'fp' at time 'exp_time'. If the timer is
 * already in use it is first removed from the timers queue. Then, it is put
 * in the list of active timers with the first to expire in front.
 * The caller responsible for scheduling a new alarm for the timer if needed. 
 */
  timer_t **atp;
  clock_t old_head = 0;

  if(*tmrs)
  	old_head = (*tmrs)->tmr_exp_time;

  /* Set the timer's variables. */
  (void) tmrs_clrtimer(tmrs, tp, NULL);
  tp->tmr_exp_time = exp_time;
  tp->tmr_func = watchdog;

  /* Add the timer to the active timers. The next timer due is in front. */
  for (atp = tmrs; *atp != NULL; atp = &(*atp)->tmr_next) {
	if (exp_time < (*atp)->tmr_exp_time) break;
  }
  tp->tmr_next = *atp;
  *atp = tp;
  if(new_head)
  	(*new_head) = (*tmrs)->tmr_exp_time;
  return old_head;
}

