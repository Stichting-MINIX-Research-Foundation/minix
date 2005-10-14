/* The kernel call implemented in this file:
 *   m_type:	SYS_PRIVCTL
 *
 * The parameters for this kernel call are:
 *    m1_i1:	PR_PROC_NR 	(process number of caller)	
 */

#include "../system.h"
#include "../ipc.h"
#include <signal.h>

#if USE_PRIVCTL

#define FILLED_MASK	(~0)

/*===========================================================================*
 *				do_privctl				     *
 *===========================================================================*/
PUBLIC int do_privctl(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* Handle sys_privctl(). Update a process' privileges. If the process is not
 * yet a system process, make sure it gets its own privilege structure.
 */
  register struct proc *caller_ptr;
  register struct proc *rp;
  register struct priv *sp;
  int proc_nr;
  int priv_id;
  int old_flags;
  int i;

  /* Check whether caller is allowed to make this call. Privileged proceses 
   * can only update the privileges of processes that are inhibited from 
   * running by the NO_PRIV flag. This flag is set when a privileged process
   * forks. 
   */
  caller_ptr = proc_addr(m_ptr->m_source);
  if (! (priv(caller_ptr)->s_flags & SYS_PROC)) return(EPERM); 
  proc_nr = m_ptr->PR_PROC_NR;
  if (! isokprocn(proc_nr)) return(EINVAL);
  rp = proc_addr(proc_nr);
  if (! (rp->p_rts_flags & NO_PRIV)) return(EPERM);

  /* Make sure this process has its own privileges structure. This may fail, 
   * since there are only a limited number of system processes. Then copy the
   * privileges from the caller and restore some defaults.
   */
  if ((i=get_priv(rp, SYS_PROC)) != OK) return(i);
  priv_id = priv(rp)->s_id;			/* backup privilege id */
  *priv(rp) = *priv(caller_ptr);		/* copy from caller */
  priv(rp)->s_id = priv_id;			/* restore privilege id */
  priv(rp)->s_proc_nr = proc_nr;		/* reassociate process nr */

  for (i=0; i< BITMAP_CHUNKS(NR_SYS_PROCS); i++)	/* remove pending: */
      priv(rp)->s_notify_pending.chunk[i] = 0;		/* - notifications */
  priv(rp)->s_int_pending = 0;				/* - interrupts */
  sigemptyset(&priv(rp)->s_sig_pending);		/* - signals */

  /* Now update the process' privileges as requested. */
  rp->p_priv->s_trap_mask = FILLED_MASK;
  for (i=0; i<BITMAP_CHUNKS(NR_SYS_PROCS); i++) {
	rp->p_priv->s_ipc_to.chunk[i] = FILLED_MASK;
  }
  unset_sys_bit(rp->p_priv->s_ipc_to, USER_PRIV_ID);

  /* All process that this process can send to must be able to reply. 
   * Therefore, their send masks should be updated as well. 
   */
  for (i=0; i<NR_SYS_PROCS; i++) {
      if (get_sys_bit(rp->p_priv->s_ipc_to, i)) {
          set_sys_bit(priv_addr(i)->s_ipc_to, priv_id(rp));
      }
  }

  /* Done. Privileges have been set. Allow process to run again. */
  old_flags = rp->p_rts_flags;		/* save value of the flags */
  rp->p_rts_flags &= ~NO_PRIV; 		
  if (old_flags != 0 && rp->p_rts_flags == 0) lock_enqueue(rp);
  return(OK);
}

#endif /* USE_PRIVCTL */

