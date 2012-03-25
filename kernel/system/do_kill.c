/* The kernel call that is implemented in this file:
 *   m_type:	SYS_KILL
 *
 * The parameters for this kernel call are:
 *     m2_i1:	SIG_ENDPT  	# process to signal/ pending		
 *     m2_i2:	SIG_NUMBER	# signal number to send to process
 */

#include "kernel/system.h"
#include <signal.h>

#if USE_KILL

/*===========================================================================*
 *			          do_kill				     *
 *===========================================================================*/
int do_kill(struct proc * caller, message * m_ptr)
{
/* Handle sys_kill(). Cause a signal to be sent to a process. Any request
 * is added to the map of pending signals and the signal manager
 * associated to the process is informed about the new signal. The signal
 * is then delivered using POSIX signal handlers for user processes, or
 * translated into an IPC message for system services.
 */
  proc_nr_t proc_nr, proc_nr_e;
  int sig_nr = m_ptr->SIG_NUMBER;

  proc_nr_e= (proc_nr_t) m_ptr->SIG_ENDPT;

  if (!isokendpt(proc_nr_e, &proc_nr)) return(EINVAL);
  if (sig_nr >= _NSIG) return(EINVAL);
  if (iskerneln(proc_nr)) return(EPERM);

  /* Set pending signal to be processed by the signal manager. */
  cause_sig(proc_nr, sig_nr);

  return(OK);
}

#endif /* USE_KILL */

