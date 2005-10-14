/* The kernel call implemented in this file:
 *   m_type:	SYS_FORK
 *
 * The parameters for this kernel call are:
 *    m1_i1:	PR_PROC_NR	(child's process table slot)	
 *    m1_i2:	PR_PPROC_NR	(parent, process that forked)	
 */

#include "../system.h"
#include <signal.h>
#if (CHIP == INTEL)
#include "../protect.h"
#endif

#if USE_FORK

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
  register struct proc *rpc;		/* child process pointer */
  struct proc *rpp;			/* parent process pointer */
  int i;

  rpp = proc_addr(m_ptr->PR_PPROC_NR);
  rpc = proc_addr(m_ptr->PR_PROC_NR);
  if (isemptyp(rpp) || ! isemptyp(rpc)) return(EINVAL);

  /* Copy parent 'proc' struct to child. And reinitialize some fields. */
#if (CHIP == INTEL)
  old_ldt_sel = rpc->p_ldt_sel;		/* backup local descriptors */
  *rpc = *rpp;				/* copy 'proc' struct */
  rpc->p_ldt_sel = old_ldt_sel;		/* restore descriptors */
#else
  *rpc = *rpp;				/* copy 'proc' struct */
#endif
  rpc->p_nr = m_ptr->PR_PROC_NR;	/* this was obliterated by copy */

  /* Only one in group should have SIGNALED, child doesn't inherit tracing. */
  rpc->p_rts_flags |= NO_MAP;		/* inhibit process from running */
  rpc->p_rts_flags &= ~(SIGNALED | SIG_PENDING | P_STOP);
  sigemptyset(&rpc->p_pending);

  rpc->p_reg.retreg = 0;	/* child sees pid = 0 to know it is child */
  rpc->p_user_time = 0;		/* set all the accounting times to 0 */
  rpc->p_sys_time = 0;

  /* Parent and child have to share the quantum that the forked process had,
   * so that queued processes do not have to wait longer because of the fork.
   * If the time left is odd, the child gets an extra tick.
   */
  rpc->p_ticks_left = (rpc->p_ticks_left + 1) / 2;
  rpp->p_ticks_left =  rpp->p_ticks_left / 2;	

  /* If the parent is a privileged process, take away the privileges from the 
   * child process and inhibit it from running by setting the NO_PRIV flag.
   * The caller should explicitely set the new privileges before executing.
   */
  if (priv(rpp)->s_flags & SYS_PROC) {
      rpc->p_priv = priv_addr(USER_PRIV_ID);
      rpc->p_rts_flags |= NO_PRIV;
  }
  return(OK);
}

#endif /* USE_FORK */

