/* The kernel call implemented in this file:
 *   m_type:	SYS_PRIVCTL
 *
 * The parameters for this kernel call are:
 *    m1_i1:	PR_ENDPT 	(process number of caller)	
 */

#include "../system.h"
#include "../ipc.h"
#include <signal.h>
#include <string.h>

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
  int i, r;
  struct io_range io_range;
  struct mem_range mem_range;
  struct priv priv;

  /* Check whether caller is allowed to make this call. Privileged proceses 
   * can only update the privileges of processes that are inhibited from 
   * running by the NO_PRIV flag. This flag is set when a privileged process
   * forks. 
   */
  caller_ptr = proc_addr(who_p);
  if (! (priv(caller_ptr)->s_flags & SYS_PROC)) return(EPERM); 
  if(m_ptr->PR_ENDPT == SELF) proc_nr = who_p;
  else if(!isokendpt(m_ptr->PR_ENDPT, &proc_nr)) return(EINVAL);
  rp = proc_addr(proc_nr);

  switch(m_ptr->CTL_REQUEST)
  {
  case SYS_PRIV_INIT:
	if (! RTS_ISSET(rp, NO_PRIV)) return(EPERM);

	/* Make sure this process has its own privileges structure. This may
	 * fail, since there are only a limited number of system processes.
	 * Then copy the privileges from the caller and restore some defaults.
	 */
	if ((i=get_priv(rp, SYS_PROC)) != OK)
	{
		kprintf("do_privctl: out of priv structures\n");
		return(i);
	}
	priv_id = priv(rp)->s_id;		/* backup privilege id */
	*priv(rp) = *priv(caller_ptr);		/* copy from caller */
	priv(rp)->s_id = priv_id;		/* restore privilege id */
	priv(rp)->s_proc_nr = proc_nr;		/* reassociate process nr */

	for (i=0; i< BITMAP_CHUNKS(NR_SYS_PROCS); i++)	/* remove pending: */
	      priv(rp)->s_notify_pending.chunk[i] = 0;	/* - notifications */
	priv(rp)->s_int_pending = 0;			/* - interrupts */
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

	for (i=0; i<BITMAP_CHUNKS(NR_SYS_PROCS); i++) {
		rp->p_priv->s_ipc_sendrec.chunk[i] = FILLED_MASK;
	}
	unset_sys_bit(rp->p_priv->s_ipc_sendrec, USER_PRIV_ID);

	/* No I/O resources, no memory resources, no IRQs, no grant table */
	priv(rp)->s_nr_io_range= 0;
	priv(rp)->s_nr_mem_range= 0;
	priv(rp)->s_nr_irq= 0;
	priv(rp)->s_grant_table= 0;
	priv(rp)->s_grant_entries= 0;

	if (m_ptr->CTL_ARG_PTR)
	{
		/* Copy privilege structure from caller */
		if((r=data_copy(who_e, (vir_bytes) m_ptr->CTL_ARG_PTR,
			SYSTEM, (vir_bytes) &priv, sizeof(priv))) != OK)
			return r;

		/* Copy the call mask */
		for (i= 0; i<CALL_MASK_SIZE; i++)
			priv(rp)->s_k_call_mask[i]= priv.s_k_call_mask[i];

		/* Copy IRQs */
		if (priv.s_nr_irq < 0 || priv.s_nr_irq > NR_IRQ)
			return EINVAL;
		priv(rp)->s_nr_irq= priv.s_nr_irq;
		for (i= 0; i<priv.s_nr_irq; i++)
		{
			priv(rp)->s_irq_tab[i]= priv.s_irq_tab[i];
#if 0
			kprintf("do_privctl: adding IRQ %d for %d\n",
				priv(rp)->s_irq_tab[i], rp->p_endpoint);
#endif
		}

		priv(rp)->s_flags |= CHECK_IRQ;	/* Check requests for IRQs */

		/* Copy I/O ranges */
		if (priv.s_nr_io_range < 0 || priv.s_nr_io_range > NR_IO_RANGE)
			return EINVAL;
		priv(rp)->s_nr_io_range= priv.s_nr_io_range;
		for (i= 0; i<priv.s_nr_io_range; i++)
		{
			priv(rp)->s_io_tab[i]= priv.s_io_tab[i];
#if 0
			kprintf("do_privctl: adding I/O range [%x..%x] for %d\n",
				priv(rp)->s_io_tab[i].ior_base,
				priv(rp)->s_io_tab[i].ior_limit,
				rp->p_endpoint);
#endif
		}

		/* Check requests for IRQs */
		priv(rp)->s_flags |= CHECK_IO_PORT;

		memcpy(priv(rp)->s_k_call_mask, priv.s_k_call_mask,
			sizeof(priv(rp)->s_k_call_mask));
		memcpy(&priv(rp)->s_ipc_to, &priv.s_ipc_to,
			sizeof(priv(rp)->s_ipc_to));
		memcpy(&priv(rp)->s_ipc_sendrec, &priv.s_ipc_sendrec,
			sizeof(priv(rp)->s_ipc_sendrec));
	}

	/* Done. Privileges have been set. Allow process to run again. */
	RTS_LOCK_UNSET(rp, NO_PRIV);
	return(OK);
  case SYS_PRIV_USER:
	/* Make this process an ordinary user process. */
	if (!RTS_ISSET(rp, NO_PRIV)) return(EPERM);
	if ((i=get_priv(rp, 0)) != OK) return(i);
	RTS_LOCK_UNSET(rp, NO_PRIV);
	return(OK);

  case SYS_PRIV_ADD_IO:
	if (RTS_ISSET(rp, NO_PRIV))
		return(EPERM);

	/* Only system processes get I/O resources? */
	if (!(priv(rp)->s_flags & SYS_PROC))
		return EPERM;

#if 0 /* XXX -- do we need a call for this? */
	if (strcmp(rp->p_name, "fxp") == 0 ||
		strcmp(rp->p_name, "rtl8139") == 0)
	{
		kprintf("setting ipc_stats_target to %d\n", rp->p_endpoint);
		ipc_stats_target= rp->p_endpoint;
	}
#endif

	/* Get the I/O range */
	data_copy(who_e, (vir_bytes) m_ptr->CTL_ARG_PTR,
		SYSTEM, (vir_bytes) &io_range, sizeof(io_range));
	priv(rp)->s_flags |= CHECK_IO_PORT;	/* Check I/O accesses */
	i= priv(rp)->s_nr_io_range;
	if (i >= NR_IO_RANGE)
		return ENOMEM;

	priv(rp)->s_io_tab[i].ior_base= io_range.ior_base;
	priv(rp)->s_io_tab[i].ior_limit= io_range.ior_limit;
	priv(rp)->s_nr_io_range++;

	return OK;

  case SYS_PRIV_ADD_MEM:
	if (RTS_ISSET(rp, NO_PRIV))
		return(EPERM);

	/* Only system processes get memory resources? */
	if (!(priv(rp)->s_flags & SYS_PROC))
		return EPERM;

	/* Get the memory range */
	if((r=data_copy(who_e, (vir_bytes) m_ptr->CTL_ARG_PTR,
		SYSTEM, (vir_bytes) &mem_range, sizeof(mem_range))) != OK)
		return r;
	priv(rp)->s_flags |= CHECK_MEM;	/* Check I/O accesses */
	i= priv(rp)->s_nr_mem_range;
	if (i >= NR_MEM_RANGE)
		return ENOMEM;

#if 0
	priv(rp)->s_mem_tab[i].mr_base= mem_range.mr_base;
	priv(rp)->s_mem_tab[i].mr_limit= mem_range.mr_limit;
	priv(rp)->s_nr_mem_range++;
#endif

	return OK;

  case SYS_PRIV_ADD_IRQ:
	if (RTS_ISSET(rp, NO_PRIV))
		return(EPERM);

	/* Only system processes get IRQs? */
	if (!(priv(rp)->s_flags & SYS_PROC))
		return EPERM;

	priv(rp)->s_flags |= CHECK_IRQ;	/* Check IRQs */

	i= priv(rp)->s_nr_irq;
	if (i >= NR_IRQ)
		return ENOMEM;
	priv(rp)->s_irq_tab[i]= m_ptr->CTL_MM_PRIV;
	priv(rp)->s_nr_irq++;

	return OK;
  default:
	kprintf("do_privctl: bad request %d\n", m_ptr->CTL_REQUEST);
	return EINVAL;
  }
}

#endif /* USE_PRIVCTL */

