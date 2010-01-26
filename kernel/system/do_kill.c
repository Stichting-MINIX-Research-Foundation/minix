/* The kernel call that is implemented in this file:
 *   m_type:	SYS_KILL
 *
 * The parameters for this kernel call are:
 *     m2_i1:	SIG_ENDPT  	# process to signal/ pending		
 *     m2_i2:	SIG_NUMBER	# signal number to send to process
 */

#include "../system.h"
#include <signal.h>

#if USE_KILL

/*===========================================================================*
 *			          do_kill				     *
 *===========================================================================*/
PUBLIC int do_kill(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* Handle sys_kill(). Cause a signal to be sent to a process. The PM is the
 * central server where all signals are processed and handler policies can
 * be registered. Any request, except for PM requests, is added to the map
 * of pending signals and the PM is informed about the new signal.
 * Since system servers cannot use normal POSIX signal handlers (because they
 * are usually blocked on a RECEIVE), they can request the PM to transform 
 * signals into messages. This is done by the PM with a call to sys_kill(). 
 */
  proc_nr_t proc_nr, proc_nr_e;
  int sig_nr = m_ptr->SIG_NUMBER;

  proc_nr_e= (proc_nr_t) m_ptr->SIG_ENDPT;

  if (!isokendpt(proc_nr_e, &proc_nr)) return(EINVAL);
  if (sig_nr >= _NSIG) return(EINVAL);
  if (iskerneln(proc_nr)) return(EPERM);

  /* Set pending signal to be processed by the PM. */
  cause_sig(proc_nr, sig_nr);
  if (sig_nr == SIGKILL)
	clear_endpoint(proc_addr(proc_nr));
  return(OK);
}

#endif /* USE_KILL */

