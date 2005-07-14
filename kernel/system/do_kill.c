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

#if USE_KILL

/*===========================================================================*
 *			          do_kill				     *
 *===========================================================================*/
PUBLIC int do_kill(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* Handle sys_kill(). Cause a signal to be sent to a process via PM.
 * Note that this has nothing to do with the kill (2) system call, this
 * is how the FS (and possibly other servers) get access to cause_sig. 
 */
  cause_sig(m_ptr->SIG_PROC, m_ptr->SIG_NUMBER);
  return(OK);
}

#endif /* USE_KILL */

