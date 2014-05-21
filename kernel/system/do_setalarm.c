/* The kernel call implemented in this file:
 *   m_type:	SYS_SETALARM 
 *
 * The parameters for this kernel call are:
 *    m_lsys_krn_sys_setalarm.exp_time		(alarm's expiration time)
 *    m_lsys_krn_sys_setalarm.abs_time		(expiration time is absolute?)
 *    m_lsys_krn_sys_setalarm.time_left		(return seconds left of previous)
 */

#include "kernel/system.h"

#include <minix/endpoint.h>
#include <assert.h>

#if USE_SETALARM

static void cause_alarm(minix_timer_t *tp);

/*===========================================================================*
 *				do_setalarm				     *
 *===========================================================================*/
int do_setalarm(struct proc * caller, message * m_ptr)
{
/* A process requests a synchronous alarm, or wants to cancel its alarm. */
  long exp_time;		/* expiration time for this alarm */
  int use_abs_time;		/* use absolute or relative time */
  minix_timer_t *tp;		/* the process' timer structure */
  clock_t uptime;		/* placeholder for current uptime */

  /* Extract shared parameters from the request message. */
  exp_time = m_ptr->m_lsys_krn_sys_setalarm.exp_time;		/* alarm's expiration time */
  use_abs_time = m_ptr->m_lsys_krn_sys_setalarm.abs_time;	/* flag for absolute time */
  if (! (priv(caller)->s_flags & SYS_PROC)) return(EPERM);

  /* Get the timer structure and set the parameters for this alarm. */
  tp = &(priv(caller)->s_alarm_timer);
  tmr_arg(tp)->ta_int = caller->p_endpoint;
  tp->tmr_func = cause_alarm; 

  /* Return the ticks left on the previous alarm. */
  uptime = get_monotonic(); 
  if ((tp->tmr_exp_time != TMR_NEVER) && (uptime < tp->tmr_exp_time) ) {
      m_ptr->m_lsys_krn_sys_setalarm.time_left = (tp->tmr_exp_time - uptime);
  } else {
      m_ptr->m_lsys_krn_sys_setalarm.time_left = 0;
  }

  /* Finally, (re)set the timer depending on the expiration time. */
  if (exp_time == 0) {
      reset_kernel_timer(tp);
  } else {
      tp->tmr_exp_time = (use_abs_time) ? exp_time : exp_time + get_monotonic();
      set_kernel_timer(tp, tp->tmr_exp_time, tp->tmr_func);
  }
  return(OK);
}

/*===========================================================================*
 *				cause_alarm				     *
 *===========================================================================*/
static void cause_alarm(minix_timer_t *tp)
{
/* Routine called if a timer goes off and the process requested a synchronous
 * alarm. The process number is stored in timer argument 'ta_int'. Notify that
 * process with a notification message from CLOCK.
 */
  endpoint_t proc_nr_e = tmr_arg(tp)->ta_int;	/* get process number */
  mini_notify(proc_addr(CLOCK), proc_nr_e);	/* notify process */
}

#endif /* USE_SETALARM */
