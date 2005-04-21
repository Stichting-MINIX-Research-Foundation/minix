/* The system call implemented in this file:
 *   m_type:	SYS_FORK
 *
 * The parameters for this system call are:
 *    m1_i1:	PR_PROC_NR	(child's process table slot)	
 *    m1_i2:	PR_PPROC_NR	(parent, process that forked)	
 *    m1_i3:	PR_PID	 	(child pid received from MM)
 */

#include "../kernel.h"
#include "../system.h"
#include <signal.h>
#include "../sendmask.h"
INIT_ASSERT

/*===========================================================================*
 *				do_fork					     *
 *===========================================================================*/
PUBLIC int do_fork(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
/* Handle sys_fork().  PR_PPROC_NR has forked.  The child is PR_PROC_NR. */

#if (CHIP == INTEL)
  reg_t old_ldt_sel;
#endif
  register struct proc *rpc;
  struct proc *rpp;

  rpp = proc_addr(m_ptr->PR_PPROC_NR);
  assert(isuserp(rpp));
  rpc = proc_addr(m_ptr->PR_PROC_NR);
  assert(isemptyp(rpc));

  /* Copy parent 'proc' struct to child. */
#if (CHIP == INTEL)
  old_ldt_sel = rpc->p_ldt_sel;	/* stop this being obliterated by copy */
#endif

  *rpc = *rpp;					/* copy 'proc' struct */

#if (CHIP == INTEL)
  rpc->p_ldt_sel = old_ldt_sel;
#endif
  rpc->p_nr = m_ptr->PR_PROC_NR;	/* this was obliterated by copy */

  rpc->p_flags |= NO_MAP;	/* inhibit the process from running */

  rpc->p_flags &= ~(PENDING | SIG_PENDING | P_STOP);

  /* Only 1 in group should have PENDING, child does not inherit trace status*/
  sigemptyset(&rpc->p_pending);
  rpc->p_pendcount = 0;
  rpc->p_reg.retreg = 0;	/* child sees pid = 0 to know it is child */

  rpc->user_time = 0;		/* set all the accounting times to 0 */
  rpc->sys_time = 0;
  rpc->child_utime = 0;
  rpc->child_stime = 0;

  return(OK);
}


