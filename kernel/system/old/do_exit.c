/* The system call implemented in this file:
 *   m_type:	SYS_EXIT
 *
 * The parameters for this system call are:
 *    m1_i1:	EXIT_STATUS	(exit status, 0 if normal exit)
 *
 * Author:
 *    Jorrit N. Herder <jnherder@cs.vu.nl>
 */

#include "../kernel.h"
#include "../system.h"

/*===========================================================================*
 * 				   do_exit				     *
 *===========================================================================*/
PUBLIC int do_exit(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* Handle sys_exit. A server or driver wants to exit. This may happen
 * on a panic, but also is done when MINIX is shutdown.
 */
  register struct proc *rp;
  int proc_nr = m_ptr->m_source;	/* can only exit own process */

  if (m_ptr->EXIT_STATUS != 0) {
      kprintf("WARNING: system process %d exited with an error.\n", proc_nr );
  }

  /* Now call the routine to clean up of the process table slot. This cancels
   * outstanding timers, possibly removes the process from the message queues,
   * and reset important process table fields.
   */
  clear_proc(proc_nr);

  /* If the shutdown sequence is active, see if it was awaiting the shutdown
   * of this system service. If so, directly continue the stop sequence. 
   */
  if (shutting_down && shutdown_process == proc_addr(proc_nr)) {
      stop_sequence(&shutdown_timer);
  }
  return(EDONTREPLY);			/* no reply is sent */
}


