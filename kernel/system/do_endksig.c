/* The kernel call that is implemented in this file:
 *   m_type:	SYS_ENDKSIG
 *
 * The parameters for this kernel call are:
 *     m2_i1:	SIG_ENDPT  	# process for which PM is done
 */

#include "../system.h"
#include <signal.h>

#if USE_ENDKSIG 

/*===========================================================================*
 *			      do_endksig				     *
 *===========================================================================*/
PUBLIC int do_endksig(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* Finish up after a kernel type signal, caused by a SYS_KILL message or a 
 * call to cause_sig by a task. This is called by the PM after processing a
 * signal it got with SYS_GETKSIG.
 */
  register struct proc *rp;
  int proc_nr;

  /* Get process pointer and verify that it had signals pending. If the 
   * process is already dead its flags will be reset. 
   */
  if(!isokendpt(m_ptr->SIG_ENDPT, &proc_nr))
	return EINVAL;

  rp = proc_addr(proc_nr);
  if (!RTS_ISSET(rp, RTS_SIG_PENDING)) return(EINVAL);

  /* PM has finished one kernel signal. Perhaps process is ready now? */
  if (!RTS_ISSET(rp, RTS_SIGNALED)) 		/* new signal arrived */
	RTS_LOCK_UNSET(rp, RTS_SIG_PENDING);	/* remove pending flag */
  return(OK);
}

#endif /* USE_ENDKSIG */

