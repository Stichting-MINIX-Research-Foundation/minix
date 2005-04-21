/* The system call implemented in this file:
 *   m_type:	SYS_ENDSIG
 *
 * The parameters for this system call are:
 *    m2_i1:	SIG_PROC		(process that was signaled)
 */

#include "../kernel.h"
#include "../system.h"
INIT_ASSERT

/*===========================================================================*
 *			      do_endsig					     *
 *===========================================================================*/
PUBLIC int do_endsig(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
/* Finish up after a KSIG-type signal, caused by a SYS_KILL message or a call
 * to cause_sig by a task
 */

  register struct proc *rp;

  rp = proc_addr(m_ptr->SIG_PROC);
  if (isemptyp(rp)) return(EINVAL);		/* process already dead? */
  assert(isuserp(rp));

  /* MM has finished one KSIG. */
  if (rp->p_pendcount != 0 && --rp->p_pendcount == 0
      && (rp->p_flags &= ~SIG_PENDING) == 0)
	lock_ready(rp);
  return(OK);
}

