/* The kernel call implemented in this file:
 *   m_type:	SYS_ABORT
 *
 * The parameters for this kernel call are:
 *    m1_i1:	ABRT_HOW 	(how to abort, possibly fetch monitor params)	
 */

#include "kernel/system.h"
#include <unistd.h>

#if USE_ABORT

/*===========================================================================*
 *				do_abort				     *
 *===========================================================================*/
int do_abort(struct proc * caller, message * m_ptr)
{
/* Handle sys_abort. MINIX is unable to continue. This can originate e.g.
 * in the PM (normal abort) or TTY (after CTRL-ALT-DEL).
 */
  int how = m_ptr->ABRT_HOW;

  /* Now prepare to shutdown MINIX. */
  prepare_shutdown(how);
  return(OK);				/* pro-forma (really EDISASTER) */
}

#endif /* USE_ABORT */

