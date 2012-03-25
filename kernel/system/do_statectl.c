/* The kernel call implemented in this file:
 *   m_type:	SYS_STATECTL
 *
 * The parameters for this kernel call are:
 *    m2_i2:	CTL_REQUEST	(state control request)
 */

#include "kernel/system.h"

#if USE_STATECTL

/*===========================================================================*
 *			          do_statectl				     *
 *===========================================================================*/
int do_statectl(struct proc * caller, message * m_ptr)
{
/* Handle sys_statectl(). A process has issued a state control request. */

  switch(m_ptr->CTL_REQUEST)
  {
  case SYS_STATE_CLEAR_IPC_REFS:
	/* Clear IPC references for all the processes communicating
	 * with the caller.
	 */
	clear_ipc_refs(caller, EDEADSRCDST);
	return(OK);
  default:
	printf("do_statectl: bad request %d\n", m_ptr->CTL_REQUEST);
	return EINVAL;
  }
}

#endif /* USE_STATECTL */
