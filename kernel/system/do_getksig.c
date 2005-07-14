/* The system call that is implemented in this file:
 *     SYS_SIGCTL	# signal handling functionality 
 *
 * The parameters and types for this system call are:
 *     SIG_REQUEST 	# request to perform			(long)
 *     SIG_PROC  	# process to signal/ pending		(int)
 *     SIG_CTXT_PTR 	# pointer to sigcontext structure	(pointer)	
 *     SIG_FLAGS    	# flags for S_SIGRETURN call		(int)	
 *     SIG_MAP		# bit map with pending signals		(long)	
 *     SIG_NUMBER	# signal number to send to process	(int)	
 *
 * Supported request types are in the parameter SIG_REQUEST:
 *     S_GETSIG		# get a pending kernel signal
 *     S_ENDSIG		# signal has been processed 
 *     S_SENDSIG	# deliver a POSIX-style signal 
 *     S_SIGRETURN	# return from a POSIX-style signal 
 *     S_KILL		# send a signal to a process 
 */

#include "../system.h"
#include <signal.h>
#include <sys/sigcontext.h>

#if USE_GETKSIG

/*===========================================================================*
 *			      do_getksig				     *
 *===========================================================================*/
PUBLIC int do_getksig(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* PM is ready to accept signals and repeatedly does a system call to get 
 * one. Find a process with pending signals. If no signals are available, 
 * return NONE in the process number field.
 */
  register struct proc *rp;

  /* Find the next process with pending signals. */
  for (rp = BEG_USER_ADDR; rp < END_PROC_ADDR; rp++) {
      if (rp->p_rts_flags & SIGNALED) {
          m_ptr->SIG_PROC = rp->p_nr;
          m_ptr->SIG_MAP = rp->p_pending;
          sigemptyset(&rp->p_pending); 	/* ball is in PM's court */
          rp->p_rts_flags &= ~SIGNALED;	/* blocked by SIG_PENDING */
          return(OK);
      }
  }

  /* No process with pending signals was found. */
  m_ptr->SIG_PROC = NONE; 
  return(OK);
}
#endif /* USE_GETKSIG */


