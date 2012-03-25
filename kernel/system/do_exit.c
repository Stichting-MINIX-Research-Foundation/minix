/* The kernel call implemented in this file:
 *   m_type:	SYS_EXIT
 */

#include "kernel/system.h"

#include <signal.h>

#if USE_EXIT

/*===========================================================================*
 *				 do_exit				     *
 *===========================================================================*/
int do_exit(struct proc * caller, message * m_ptr)
{
/* Handle sys_exit. A system process has requested to exit. Generate a
 * self-termination signal.
 */
  int sig_nr = SIGABRT;

  cause_sig(caller->p_nr, sig_nr);      /* send a signal to the caller */

  return(EDONTREPLY);			/* don't reply */
}

#endif /* USE_EXIT */

