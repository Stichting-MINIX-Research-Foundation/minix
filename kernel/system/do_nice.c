/* The kernel call implemented in this file:
 *   m_type:	SYS_NICE
 *
 * The parameters for this kernel call are:
 *    m1_i1:	PR_PROC_NR	process number to change priority
 *    m1_i2:	PR_PRIORITY	the new priority
 */

#include "../system.h"
#include <minix/type.h>
#include <sys/resource.h>

#if USE_NICE

/*===========================================================================*
 *				  do_nice				     *
 *===========================================================================*/
PUBLIC int do_nice(message *m_ptr)
{
  int proc_nr, pri, new_q ;
  register struct proc *rp;

  /* Extract the message parameters and do sanity checking. */
  proc_nr = m_ptr->PR_PROC_NR;
  if (! isokprocn(proc_nr)) return(EINVAL);
  if (iskerneln(proc_nr)) return(EPERM);
  pri = m_ptr->PR_PRIORITY;
  if (pri < PRIO_MIN || pri > PRIO_MAX) return(EINVAL);

  /* The priority is currently between PRIO_MIN and PRIO_MAX. We have to
   * scale this between MIN_USER_Q and MAX_USER_Q.
   */
  new_q = MAX_USER_Q + (pri-PRIO_MIN) * (MIN_USER_Q-MAX_USER_Q+1) / 
      (PRIO_MAX-PRIO_MIN+1);
  if (new_q < MAX_USER_Q) new_q = MAX_USER_Q;	/* shouldn't happen */
  if (new_q > MIN_USER_Q) new_q = MIN_USER_Q;	/* shouldn't happen */

  /* Make sure the process is not running while changing its priority; the
   * max_priority is the base priority. Put the process back in its new
   * queue if it is runnable.
   */
  rp = proc_addr(proc_nr);
  lock_dequeue(rp);
  rp->p_max_priority = rp->p_priority = new_q;
  if (! rp->p_rts_flags) lock_enqueue(rp);

  return(OK);
}

#endif /* USE_NICE */

