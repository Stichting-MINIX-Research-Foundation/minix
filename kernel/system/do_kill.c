/* The kernel call that is implemented in this file:
 *   m_type:	SYS_KILL
 *
 * The parameters for this kernel call are:
 *     m2_i1:	SIG_PROC  	# process to signal/ pending		
 *     m2_i2:	SIG_NUMBER	# signal number to send to process
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
/* Handle sys_kill(). Cause a signal to be sent to a process. The PM is the
 * central server where all signals are processed and handler policies can
 * be registered. Any request, except for PM requests, is added to the map
 * of pending signals and the PM is informed about the new signal.
 * Since system servers cannot use normal POSIX signal handlers (because they
 * are usually blocked on a RECEIVE), they can request the PM to transform 
 * signals into messages. This is done by the PM with a call to sys_kill(). 
 */
  proc_nr_t proc_nr = m_ptr->SIG_PROC;
  int sig_nr = m_ptr->SIG_NUMBER;

  if (! isokprocn(proc_nr) || sig_nr > _NSIG) return(EINVAL);
  if (iskerneln(proc_nr)) return(EPERM);

  if (m_ptr->m_source == PM_PROC_NR) {
      /* Directly send signal notification to a system process. */
      if (! (priv(proc_addr(proc_nr))->s_flags & SYS_PROC)) return(EPERM);
      send_sig(proc_nr, sig_nr);
  } else {
      /* Set pending signal to be processed by the PM. */
      cause_sig(proc_nr, sig_nr);
  }
  return(OK);
}

#endif /* USE_KILL */

