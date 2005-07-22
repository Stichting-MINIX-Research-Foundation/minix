/* The system call implemented in this file:
 *   m_type:	SYS_PRIVCTL
 *
 * The parameters for this system call are:
 *    m2_i1:	CTL_PROC_NR 	(process number of caller)	
 */

#include "../system.h"
#include "../ipc.h"

#if USE_PRIVCTL

/*===========================================================================*
 *				do_privctl				     *
 *===========================================================================*/
PUBLIC int do_privctl(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* Handle sys_privctl(). Update a process' privileges. If the process is not
 * yet a system process, make sure it gets its own privilege structure.
 */
  register struct proc *rp;
  register struct priv *sp;
  int proc_nr;

  /* Extract message parameters. */
  proc_nr = m_ptr->CTL_PROC_NR;
  if (proc_nr == SELF) proc_nr = m_ptr->m_source;
  if (! isokprocn(proc_nr)) return(EINVAL);

  rp = proc_addr(proc_nr);

  /* Make sure this process has its own privileges structure. */
  if (! (priv(rp)->s_flags & SYS_PROC)) 
      set_priv(rp, SYS_PROC);

  /* Now update the process' privileges as requested. */
  rp->p_priv->s_call_mask = SYSTEM_CALL_MASK;
  return(OK);
}

#endif /* USE_PRIVCTL */

