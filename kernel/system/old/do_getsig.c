/* The system call implemented in this file:
 *   m_type:	SYS_GETSIG
 *
 * The parameters for this system call are:
 *    m2_i1:	SIG_PROC	(return proc nr or NONE here)
 *    m2_l1:	SIG_MAP		(return signal map here)
 */

#include "../kernel.h"
#include "../system.h"
#include <signal.h>



/*===========================================================================*
 *				do_getsig				     *
 *===========================================================================*/
PUBLIC int do_getsig(m_ptr)
message *m_ptr;			/* pointer to the request message */
{
/* MM is ready to accept signals and repeatedly does a system call to get one
 * Find a process with pending signals. If no more signals are available, 
 * return NONE in the process number field.
 */
  register struct proc *rp;

  /* Only the MM is allowed to request pending signals. */
  if (m_ptr->m_source != MM_PROC_NR)
  	return(EPERM);

  /* Find the next process with pending signals. */
  for (rp = BEG_SERV_ADDR; rp < END_PROC_ADDR; rp++) {
    if (rp->p_flags & PENDING) {
	m_ptr->SIG_PROC = proc_number(rp);
	m_ptr->SIG_MAP = rp->p_pending;
	sigemptyset(&rp->p_pending); 	/* the ball is now in MM's court */
	rp->p_flags &= ~PENDING;	/* remains inhibited by SIG_PENDING */
	return(OK);
    }
  }

  /* No process with pending signals was found. */
  m_ptr->SIG_PROC = NONE; 
  return(OK);
}

