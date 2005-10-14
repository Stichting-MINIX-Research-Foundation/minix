/* The kernel call that is implemented in this file:
 *   m_type:	SYS_ENDKSIG
 *
 * The parameters for this kernel call are:
 *     m2_i1:	SIG_PROC  	# process for which PM is done
 */

#include "../system.h"
#include <signal.h>
#include <sys/sigcontext.h>

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

  /* Get process pointer and verify that it had signals pending. If the 
   * process is already dead its flags will be reset. 
   */
  rp = proc_addr(m_ptr->SIG_PROC);
  if (! (rp->p_rts_flags & SIG_PENDING)) return(EINVAL);

  /* PM has finished one kernel signal. Perhaps process is ready now? */
  if (! (rp->p_rts_flags & SIGNALED)) 		/* new signal arrived */
     if ((rp->p_rts_flags &= ~SIG_PENDING)==0)	/* remove pending flag */
         lock_enqueue(rp);			/* ready if no flags */
  return(OK);
}

#endif /* USE_ENDKSIG */

