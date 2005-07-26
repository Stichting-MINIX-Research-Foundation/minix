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
  int i;

  /* Extract message parameters. */
  proc_nr = m_ptr->CTL_PROC_NR;
  if (proc_nr == SELF) proc_nr = m_ptr->m_source;
  if (! isokprocn(proc_nr)) return(EINVAL);

  rp = proc_addr(proc_nr);

  /* Make sure this process has its own privileges structure. */
  if (! (priv(rp)->s_flags & SYS_PROC)) 
      get_priv(rp, SYS_PROC);

  /* Now update the process' privileges as requested. */
  rp->p_priv->s_call_mask = FILLED_MASK;
  for (i=0; i<BITMAP_CHUNKS(NR_SYS_PROCS); i++) {
	rp->p_priv->s_send_mask.chunk[i] = FILLED_MASK;
  }
  unset_sys_bit(rp->p_priv->s_send_mask, USER_PRIV_ID);

  /* All process that this process can send to must be able to reply. 
   * Therefore, their send masks should be updated as well. 
   */
  for (i=0; i<NR_SYS_PROCS; i++) {
      if (get_sys_bit(rp->p_priv->s_send_mask, i)) {
          set_sys_bit(priv_addr(i)->s_send_mask, priv_id(rp));
      }
  }
  return(OK);
}

#endif /* USE_PRIVCTL */

