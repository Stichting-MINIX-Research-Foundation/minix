/* The system call implemented in this file:
 *   m_type:	SYS_EXIT
 *
 * The parameters for this system call are:
 *    m1_i1:	PR_PROC_NR		(slot number of exiting process)
 */

#include "../system.h"

#if USE_EXIT

/*===========================================================================*
 *				do_exit					     *
 *===========================================================================*/
PUBLIC int do_exit(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* Handle sys_exit. A user process has exited or a system process requests 
 * to exit. Only the PM can request other process slots to be cleared.
 * The routine to clean up a process table slot cancels outstanding timers, 
 * possibly removes the process from the message queues, and resets certain 
 * process table fields to the default values.
 */
  int exit_proc_nr;				

  /* Determine what process exited. User processes are handled here. */
  if (PM_PROC_NR == m_ptr->m_source) {
      exit_proc_nr = m_ptr->PR_PROC_NR;		/* get exiting process */
      if (exit_proc_nr != SELF) { 		/* PM tries to exit self */
          if (! isokprocn(exit_proc_nr)) return(EINVAL);
          clear_proc(exit_proc_nr);		/* exit a user process */
          return(OK);				/* report back to PM */
      }
  } 

  /* The PM or some other system process requested to be exited. */
  clear_proc(m_ptr->m_source);
  return(EDONTREPLY);
}
#endif /* USE_EXIT */


