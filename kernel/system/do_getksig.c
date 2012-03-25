/* The kernel call that is implemented in this file:
 *   m_type:	SYS_GETKSIG
 *
 * The parameters for this kernel call are:
 *     m2_i1:	SIG_ENDPT  	# process with pending signals
 *     m2_l1:	SIG_MAP		# bit map with pending signals
 */

#include "kernel/system.h"
#include <signal.h>
#include <minix/endpoint.h>

#if USE_GETKSIG

/*===========================================================================*
 *			      do_getksig				     *
 *===========================================================================*/
int do_getksig(struct proc * caller, message * m_ptr)
{
/* The signal manager is ready to accept signals and repeatedly does a kernel
 * call to get one. Find a process with pending signals. If no signals are
 * available, return NONE in the process number field.
 */
  register struct proc *rp;

  /* Find the next process with pending signals. */
  for (rp = BEG_USER_ADDR; rp < END_PROC_ADDR; rp++) {
      if (RTS_ISSET(rp, RTS_SIGNALED)) {
          if (caller->p_endpoint != priv(rp)->s_sig_mgr) continue;
	  /* store signaled process' endpoint */
          m_ptr->SIG_ENDPT = rp->p_endpoint;
          m_ptr->SIG_MAP = rp->p_pending;	/* pending signals map */
          (void) sigemptyset(&rp->p_pending); 	/* clear map in the kernel */
	  RTS_UNSET(rp, RTS_SIGNALED);		/* blocked by SIG_PENDING */
          return(OK);
      }
  }

  /* No process with pending signals was found. */
  m_ptr->SIG_ENDPT = NONE; 
  return(OK);
}
#endif /* USE_GETKSIG */

