/* The system call implemented in this file:
 *   m_type:	SYS_GETSP
 *
 * The parameters for this system call are:
 *    m1_i1:	PR_PROC_NR	(process to get stack pointer of)
 *    m1_p1:	PR_STACK_PTR	(return stack pointer here)
 *
 * Author:
 *    Jorrit N. Herder <jnherder@cs.vu.nl>
 */

#include "../kernel.h"
#include "../system.h"
INIT_ASSERT

/*===========================================================================*
 *				do_getsp				     *
 *===========================================================================*/
PUBLIC int do_getsp(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
/* Handle sys_getsp().  MM wants to know what sp is. */

  register struct proc *rp;

  rp = proc_addr(m_ptr->PR_PROC_NR);
  assert(isuserp(rp)); 
  m_ptr->PR_STACK_PTR = (char *) rp->p_reg.sp;	/* return sp here (bad type) */
  return(OK);
}


