/* The kernel call implemented in this file:
 *   m_type:	SYS_NICE
 *
 * The parameters for this kernel call are:
 *    m1_i1:	PR_ENDPT   	process number to change priority
 *    m1_i2:	PR_PRIORITY	the new priority
 */

#include "../system.h"
#include <sys/resource.h>

#if USE_NICE

/*===========================================================================*
 *				  do_nice				     *
 *===========================================================================*/
PUBLIC int do_nice(struct proc * caller, message * m_ptr)
{
/* Change process priority or stop the process. */
  int proc_nr, pri, new_q ;
  register struct proc *rp;

  /* Extract the message parameters and do sanity checking. */
  if(!isokendpt(m_ptr->PR_ENDPT, &proc_nr)) return EINVAL;
  if (iskerneln(proc_nr)) return(EPERM);
  pri = m_ptr->PR_PRIORITY;
  rp = proc_addr(proc_nr);

  /* The value passed in is currently between PRIO_MIN and PRIO_MAX. 
   * We have to scale this between MIN_USER_Q and MAX_USER_Q to match 
   * the kernel's scheduling queues.
   */
  if (pri < PRIO_MIN || pri > PRIO_MAX) return(EINVAL);

  new_q = MAX_USER_Q + (pri-PRIO_MIN) * (MIN_USER_Q-MAX_USER_Q+1) / 
      (PRIO_MAX-PRIO_MIN+1);
  if (new_q < MAX_USER_Q) new_q = MAX_USER_Q;	/* shouldn't happen */
  if (new_q > MIN_USER_Q) new_q = MIN_USER_Q;	/* shouldn't happen */

  /* Dequeue the process and put it in its new queue if it is runnable. */
  RTS_SET(rp, RTS_SYS_LOCK);
  rp->p_max_priority = rp->p_priority = new_q;
  RTS_UNSET(rp, RTS_SYS_LOCK);

  return(OK);
}

#endif /* USE_NICE */

