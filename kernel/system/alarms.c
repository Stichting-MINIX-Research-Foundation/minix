/* The system call implemented in this file:
 *   m_type:	CLK_SIGNALRM, CLK_SYNCALRM, CLK_FLAGALRM
 *
 * The parameters for this system call are:
 *    m2_i1:	ALRM_PROC_NR		(set alarm for this process)	
 *    m2_l1:	ALRM_EXP_TIME		(alarm's expiration time)
 *    m2_i2:	ALRM_ABS_TIME		(expiration time is absolute?)
 *    m2_l1:	ALRM_SEC_LEFT		(return seconds left of previous)
 *    m2_p1:	ALRM_FLAG_PTR		(virtual addr of alarm flag)	
 *
 * Changes:
 *    Aug 25, 2004   fully rewritten to unite all alarms  (Jorrit N. Herder)  
 *    May 02, 2004   added new timeout flag alarm  (Jorrit N. Herder)
 */

#include "../kernel.h"
#include "../system.h"
#include <signal.h>

FORWARD _PROTOTYPE( void cause_syncalrm, (timer_t *tp) );
FORWARD _PROTOTYPE( void cause_flagalrm, (timer_t *tp) );
FORWARD _PROTOTYPE( void cause_signalrm, (timer_t *tp) );

/*===========================================================================*
 *				do_setalarm				     *
 *===========================================================================*/
PUBLIC int do_setalarm(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* A process requests an alarm, or wants to cancel its alarm. This function
 * is shared used for all of SYS_SIGNALRM, SYS_SYNCALRM, and SYS_FLAGALRM.
 */
  int proc_nr;			/* which process wants the alarm */
  long exp_time;		/* expiration time for this alarm */
  int use_abs_time;		/* use absolute or relative time */
  timer_t *tp;			/* the process' timer structure */
  clock_t uptime;		/* placeholder for current uptime */

  /* Extract shared parameters from the request message. */
  proc_nr = m_ptr->ALRM_PROC_NR;	/* process to interrupt later */
  if (SELF == proc_nr) proc_nr = m_ptr->m_source;
  if (! isokprocn(proc_nr)) return(EINVAL);
  exp_time = m_ptr->ALRM_EXP_TIME;	/* alarm's expiration time */
  use_abs_time = m_ptr->ALRM_ABS_TIME;	/* flag for absolute time */

  /* Get the timer structure and set the parameters for this alarm. */
  switch (m_ptr->m_type) {
    case SYS_SYNCALRM:			/* notify with SYN_ALARM message */
  	tp = &(proc_addr(proc_nr)->p_syncalrm);	
     	tmr_arg(tp)->ta_int = proc_nr;	
        tp->tmr_func = cause_syncalrm;
        break;
    case SYS_SIGNALRM:			/* send process a SIGALRM signal */
  	tp = &(proc_addr(proc_nr)->p_signalrm);	
     	tmr_arg(tp)->ta_int = proc_nr;
        tp->tmr_func = cause_signalrm;
        break;
    case SYS_FLAGALRM:			/* set caller's timeout flag to 1 */    
  	tp = &(proc_addr(proc_nr)->p_flagalrm);	
        tmr_arg(tp)->ta_long = 		
            numap_local(proc_nr,(vir_bytes) m_ptr->ALRM_FLAG_PTR,sizeof(int));
        if (! tmr_arg(tp)->ta_long) return(EFAULT);
      	tp->tmr_func = cause_flagalrm; 	
        break;
    default:				/* invalid alarm type */
        return(EINVAL);
  }

  /* Return the ticks left on the previous alarm. */
  uptime = get_uptime();  
  if ((tp->tmr_exp_time == TMR_NEVER) || (tp->tmr_exp_time < uptime) ) {
      m_ptr->ALRM_TIME_LEFT = 0;
  } else {
      m_ptr->ALRM_TIME_LEFT = (tp->tmr_exp_time - uptime);
  }

  /* Finally, (re)set the timer depending on 'exp_time'. */
  if (exp_time == 0) {
    reset_timer(tp);
  } else {
    tp->tmr_exp_time = (use_abs_time) ? exp_time : exp_time + get_uptime();
    set_timer(tp, tp->tmr_exp_time, tp->tmr_func);
  }
  return(OK);
}


/*===========================================================================*
 *				cause_signalrm				     *
 *===========================================================================*/
PRIVATE void cause_signalrm(tp)
timer_t *tp;
{
/* Routine called if a timer goes off for a process that requested an SIGALRM
 * signal using the alarm(2) system call. The timer argument 'ta_int' contains
 * the process number of the process to signal.
 */
  cause_sig(tmr_arg(tp)->ta_int, SIGALRM);
}

/*===========================================================================*
 *				cause_flagalrm				     *
 *===========================================================================*/
PRIVATE void cause_flagalrm(tp)
timer_t *tp;
{
/* Routine called if a timer goes off for a process that requested a timeout 
 * flag to be set when the alarm expires. The timer argument 'ta_long' gives
 * the physical address of the timeout flag. No validity check was done when 
 * setting the alarm, so check for 0 here. 
 */
  int timeout = 1;
  phys_bytes timeout_flag = (phys_bytes) tmr_arg(tp)->ta_long;
  phys_copy(vir2phys(&timeout), tmr_arg(tp)->ta_long, sizeof(int));
}

/*===========================================================================*
 *				cause_syncalrm				     *
 *===========================================================================*/
PRIVATE void cause_syncalrm(tp)
timer_t *tp;
{
/* Routine called if a timer goes off and the process requested a synchronous
 * alarm. The process number is stored in timer argument 'ta_int'. Notify that
 * process given with a SYN_ALARM message.
 */
  notify(tmr_arg(tp)->ta_int, SYN_ALARM);
}


