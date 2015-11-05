/* The kernel call implemented in this file:
 *   m_type:	SYS_VTIMER
 *
 * The parameters for this kernel call are:
 *    m2_i1:	VT_WHICH		(the timer: VT_VIRTUAL or VT_PROF)
 *    m2_i2:	VT_SET			(whether to set, or just retrieve)
 *    m2_l1:	VT_VALUE		(new/old expiration time, in ticks)
 *    m2_l2:	VT_ENDPT		(process to which the timer belongs)
 */

#include "kernel/system.h"

#include <signal.h>
#include <minix/endpoint.h>

#if USE_VTIMER

/*===========================================================================*
 *				do_vtimer				     *
 *===========================================================================*/
int do_vtimer(struct proc * caller, message * m_ptr)
{
/* Set and/or retrieve the value of one of a process' virtual timers. */
  struct proc *rp;		/* pointer to process the timer belongs to */
  register int pt_flag;		/* the misc on/off flag for the req.d timer */
  register clock_t *pt_left;	/* pointer to the process' ticks-left field */ 
  clock_t old_value;		/* the previous number of ticks left */
  int proc_nr, proc_nr_e;

  /* The requesting process must be privileged. */
  if (! (priv(caller)->s_flags & SYS_PROC)) return(EPERM);

  if (m_ptr->VT_WHICH != VT_VIRTUAL && m_ptr->VT_WHICH != VT_PROF)
      return(EINVAL);

  /* The target process must be valid. */
  proc_nr_e = (m_ptr->VT_ENDPT == SELF) ? caller->p_endpoint : m_ptr->VT_ENDPT;
  if (!isokendpt(proc_nr_e, &proc_nr)) return(EINVAL);
  rp = proc_addr(proc_nr);

  /* Determine which flag and which field in the proc structure we want to
   * retrieve and/or modify. This saves us having to differentiate between
   * VT_VIRTUAL and VT_PROF multiple times below.
   */
  if (m_ptr->VT_WHICH == VT_VIRTUAL) {
      pt_flag = MF_VIRT_TIMER;
      pt_left = &rp->p_virt_left;
  } else { /* VT_PROF */
      pt_flag = MF_PROF_TIMER;
      pt_left = &rp->p_prof_left;
  }

  /* Retrieve the old value. */
  if (rp->p_misc_flags & pt_flag) {
      old_value = *pt_left;
  } else {
      old_value = 0;
  }

  if (m_ptr->VT_SET) {
      rp->p_misc_flags &= ~pt_flag;	/* disable virtual timer */

      if (m_ptr->VT_VALUE > 0) {
          *pt_left = m_ptr->VT_VALUE;	/* set new timer value */
          rp->p_misc_flags |= pt_flag;	/* (re)enable virtual timer */
      } else {
          *pt_left = 0;			/* clear timer value */
      }
  }

  m_ptr->VT_VALUE = old_value;

  return(OK);
}

#endif /* USE_VTIMER */

/*===========================================================================*
 *				vtimer_check				     *
 *===========================================================================*/
void vtimer_check(struct proc * rp)
{
  /* This is called from the clock task, so we can be interrupted by the clock
   * interrupt, but not by the system task. Therefore we only have to protect
   * against interference from the clock handler. We can safely perform the
   * following actions without locking as well though, as the clock handler
   * never alters p_misc_flags, and only decreases p_virt_left/p_prof_left.
   */

  /* Check if the virtual timer expired. If so, send a SIGVTALRM signal. */
  if ((rp->p_misc_flags & MF_VIRT_TIMER) && rp->p_virt_left == 0) {
      rp->p_misc_flags &= ~MF_VIRT_TIMER;
      rp->p_virt_left = 0;
      cause_sig(rp->p_nr, SIGVTALRM);
  }

  /* Check if the profile timer expired. If so, send a SIGPROF signal. */
  if ((rp->p_misc_flags & MF_PROF_TIMER) && rp->p_prof_left == 0) {
      rp->p_misc_flags &= ~MF_PROF_TIMER;
      rp->p_prof_left = 0;
      cause_sig(rp->p_nr, SIGPROF);
  }
}
