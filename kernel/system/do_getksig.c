/* The kernel call that is implemented in this file:
 *   m_type:	SYS_GETKSIG
 *
 * The parameters for this kernel call are:
 *     m2_i1:	SIG_ENDPT  	# process with pending signals
 *     m2_l1:	SIG_MAP		# bit map with pending signals
 */

#include "../system.h"
#include <signal.h>
#include <minix/endpoint.h>

#if USE_GETKSIG

/*===========================================================================*
 *			      do_getksig				     *
 *===========================================================================*/
PUBLIC int do_getksig(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* PM is ready to accept signals and repeatedly does a kernel call to get 
 * one. Find a process with pending signals. If no signals are available, 
 * return NONE in the process number field.
 * It is not sufficient to ready the process when PM is informed, because 
 * PM can block waiting for FS to do a core dump.
 */
  register struct proc *rp;

  /* Find the next process with pending signals. */
  for (rp = BEG_USER_ADDR; rp < END_PROC_ADDR; rp++) {
      if (RTS_ISSET(rp, RTS_SIGNALED)) {
	  /* store signaled process' endpoint */
          m_ptr->SIG_ENDPT = rp->p_endpoint;
          m_ptr->SIG_MAP = rp->p_pending;	/* pending signals map */
          sigemptyset(&rp->p_pending); 		/* ball is in PM's court */
	  RTS_LOCK_UNSET(rp, RTS_SIGNALED);		/* blocked by SIG_PENDING */
          return(OK);
      }
  }

  /* No process with pending signals was found. */
  m_ptr->SIG_ENDPT = NONE; 
  return(OK);
}
#endif /* USE_GETKSIG */

