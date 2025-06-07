/* The kernel call implemented in this file:
 *   m_type:	SYS_PRIVCTL
 *
 * The parameters for this kernel call are:
 *   m_lsys_krn_sys_privctl.endpt		(process endpoint of target)
 *   m_lsys_krn_sys_privctl.request		(privilege control request)
 *   m_lsys_krn_sys_privctl.arg_ptr		(pointer to request data)
 *   m.m_lsys_krn_sys_privctl.phys_start
 *   m.m_lsys_krn_sys_privctl.phys_len
 */

#include "kernel/system.h"
// #include <signal.h> // Replaced
// #include <string.h> // Replaced
#include <minix/endpoint.h> // Kept

// Added kernel headers
#include <minix/kernel_types.h> // For k_errno_t, k_sigset_t
#include <klib/include/kprintf.h>
#include <klib/include/kstring.h>
#include <klib/include/kmemory.h>


#if USE_PRIVCTL

#define PRIV_DEBUG 0

static int update_priv(struct proc *rp, struct priv *priv);

/*===========================================================================*
 *				do_privctl				     *
 *===========================================================================*/
int do_privctl(struct proc * caller, message * m_ptr)
{
/* Handle sys_privctl(). Update a process' privileges. If the process is not
 * yet a system process, make sure it gets its own privilege structure.
 */
  struct proc *rp;
  proc_nr_t proc_nr;
  sys_id_t priv_id;
  sys_map_t map;
  int ipc_to_m, kcalls;
  int i, r;
  struct io_range io_range;
  struct minix_mem_range mem_range;
  struct priv priv_store; // Renamed from priv to avoid conflict with priv() macro
  int irq;

  /* Check whether caller is allowed to make this call. Privileged processes 
   * can only update the privileges of processes that are inhibited from 
   * running by the RTS_NO_PRIV flag. This flag is set when a privileged process
   * forks. 
   */
  if (! (priv(caller)->s_flags & SYS_PROC)) return(EPERM); // EPERM might be undefined
  if(m_ptr->m_lsys_krn_sys_privctl.endpt == SELF) okendpt(caller->p_endpoint,
	&proc_nr);
  else if(!isokendpt(m_ptr->m_lsys_krn_sys_privctl.endpt, &proc_nr))
	return(EINVAL); // EINVAL might be undefined
  rp = proc_addr(proc_nr);

  switch(m_ptr->m_lsys_krn_sys_privctl.request)
  {
  case SYS_PRIV_ALLOW:
	/* Allow process to run. Make sure its privilege structure has already
	 * been set.
	 */
	if (!RTS_ISSET(rp, RTS_NO_PRIV) || priv(rp)->s_proc_nr == NONE) {
		return(EPERM); // EPERM might be undefined
	}
	RTS_UNSET(rp, RTS_NO_PRIV);
	return(OK);

  case SYS_PRIV_YIELD:
	/* Allow process to run and suspend the caller. */
	if (!RTS_ISSET(rp, RTS_NO_PRIV) || priv(rp)->s_proc_nr == NONE) {
		return(EPERM); // EPERM might be undefined
	}
	RTS_SET(caller, RTS_NO_PRIV);
	RTS_UNSET(rp, RTS_NO_PRIV);
	return(OK);

  case SYS_PRIV_DISALLOW:
	/* Disallow process from running. */
	if (RTS_ISSET(rp, RTS_NO_PRIV)) return(EPERM); // EPERM might be undefined
	RTS_SET(rp, RTS_NO_PRIV);
	return(OK);

  case SYS_PRIV_CLEAR_IPC_REFS:
	/* Clear pending IPC for the process. */
	clear_ipc_refs(rp, EDEADSRCDST); // EDEADSRCDST might be undefined
	return(OK);

  case SYS_PRIV_SET_SYS:
	/* Set a privilege structure of a blocked system process. */
	if (! RTS_ISSET(rp, RTS_NO_PRIV)) return(EPERM); // EPERM might be undefined

	/* Check whether a static or dynamic privilege id must be allocated. */
	priv_id = NULL_PRIV_ID; // NULL_PRIV_ID might be undefined
	if (m_ptr->m_lsys_krn_sys_privctl.arg_ptr)
	{
		/* Copy privilege structure from caller */
		if((r=data_copy(caller->p_endpoint,
			m_ptr->m_lsys_krn_sys_privctl.arg_ptr, KERNEL,
			(vir_bytes) &priv_store, sizeof(priv_store))) != OK) // MODIFIED priv to priv_store
			return r;

		/* See if the caller wants to assign a static privilege id. */
		if(!(priv_store.s_flags & DYN_PRIV_ID)) { // MODIFIED priv to priv_store
			priv_id = priv_store.s_id; // MODIFIED priv to priv_store
		}
	}

	/* Make sure this process has its own privileges structure. This may
	 * fail, since there are only a limited number of system processes.
	 * Then copy privileges from the caller and restore some defaults.
	 */
	if ((i=get_priv(rp, priv_id)) != OK)
	{
		kprintf_stub("do_privctl: unable to allocate priv_id %d: %d\n", // MODIFIED
			priv_id, i);
		return(i);
	}
	priv_id = priv(rp)->s_id;		/* backup privilege id */
	*priv(rp) = *priv(caller);		/* copy from caller */
	priv(rp)->s_id = priv_id;		/* restore privilege id */
	priv(rp)->s_proc_nr = proc_nr;		/* reassociate process nr */

	for (i=0; i< NR_SYS_CHUNKS; i++)		/* remove pending: */
	      priv(rp)->s_asyn_pending.chunk[i] = 0;	/* - incoming asyn */
	for (i=0; i< NR_SYS_CHUNKS; i++)		/*   messages */
	      priv(rp)->s_notify_pending.chunk[i] = 0;	/* - notifications */
	priv(rp)->s_int_pending = 0;			/* - interrupts */
	/* FIXME: sigemptyset was here */ // (void) sigemptyset(&priv(rp)->s_sig_pending);	/* - signals */
	reset_kernel_timer(&priv(rp)->s_alarm_timer);	/* - alarm */
	priv(rp)->s_asyntab= -1;			/* - asynsends */
	priv(rp)->s_asynsize= 0;
	priv(rp)->s_asynendpoint = rp->p_endpoint;
	priv(rp)->s_diag_sig = FALSE;		/* no request for diag sigs */

	/* Set defaults for privilege bitmaps. */
	priv(rp)->s_flags= DSRV_F;           /* privilege flags */
	priv(rp)->s_init_flags= DSRV_I;      /* initialization flags */
	priv(rp)->s_trap_mask= DSRV_T;       /* allowed traps */
	kmemset(&map, 0, sizeof(map)); // MODIFIED
	ipc_to_m = DSRV_M;                   /* allowed targets */
	if (ipc_to_m == ALL_M) {
		for (i = 0; i < NR_SYS_PROCS; i++)
			set_sys_bit(map, i);
	}
	fill_sendto_mask(rp, &map);
	kcalls = DSRV_KC;                    /* allowed kernel calls */
	for(i = 0; i < SYS_CALL_MASK_SIZE; i++) {
		priv(rp)->s_k_call_mask[i] = (kcalls == NO_C ? 0 : (~0));
	}

	/* Set the default signal managers. */
	priv(rp)->s_sig_mgr = DSRV_SM;
	priv(rp)->s_bak_sig_mgr = NONE;

	/* Set defaults for resources: no I/O resources, no memory resources,
	 * no IRQs, no grant table, no ipc filter
	 */
	priv(rp)->s_nr_io_range= 0;
	priv(rp)->s_nr_mem_range= 0;
	priv(rp)->s_nr_irq= 0;
	priv(rp)->s_grant_table= 0;
	priv(rp)->s_grant_entries= 0;
	priv(rp)->s_grant_endpoint = rp->p_endpoint;
	priv(rp)->s_state_table= 0;
	priv(rp)->s_state_entries= 0;
	priv(rp)->s_ipcf= 0;

	/* Override defaults if the caller has supplied a privilege structure. */
	if (m_ptr->m_lsys_krn_sys_privctl.arg_ptr)
	{
		if((r = update_priv(rp, &priv_store)) != OK) { // MODIFIED priv to priv_store
			return r;
		} 
	}

	return(OK);

  case SYS_PRIV_SET_USER:
	/* Set a privilege structure of a blocked user process. */
	if (!RTS_ISSET(rp, RTS_NO_PRIV)) return(EPERM); // EPERM might be undefined

	/* Link the process to the privilege structure of the root user
	 * process all the user processes share.
	 */
	priv(rp) = priv_addr(USER_PRIV_ID);

	return(OK);

  case SYS_PRIV_ADD_IO:
	if (RTS_ISSET(rp, RTS_NO_PRIV))
		return(EPERM); // EPERM might be undefined

#if 0 /* XXX -- do we need a call for this? */
	if (kstrcmp(rp->p_name, "fxp") == 0 || // MODIFIED strcmp
		kstrcmp(rp->p_name, "rtl8139") == 0) // MODIFIED strcmp
	{
		kprintf_stub("setting ipc_stats_target to %d\n", rp->p_endpoint); // MODIFIED
		ipc_stats_target= rp->p_endpoint;
	}
#endif

	/* Get the I/O range */
	data_copy(caller->p_endpoint, m_ptr->m_lsys_krn_sys_privctl.arg_ptr,
		KERNEL, (vir_bytes) &io_range, sizeof(io_range));
	/* Add the I/O range */
	return priv_add_io(rp, &io_range);

  case SYS_PRIV_ADD_MEM:
	if (RTS_ISSET(rp, RTS_NO_PRIV))
		return(EPERM); // EPERM might be undefined

	/* Get the memory range */
	if((r=data_copy(caller->p_endpoint,
		m_ptr->m_lsys_krn_sys_privctl.arg_ptr, KERNEL,
		(vir_bytes) &mem_range, sizeof(mem_range))) != OK)
		return r;
	/* Add the memory range */
	return priv_add_mem(rp, &mem_range);

  case SYS_PRIV_ADD_IRQ:
	if (RTS_ISSET(rp, RTS_NO_PRIV))
		return(EPERM); // EPERM might be undefined

#if 0
	/* Only system processes get IRQs? */
	if (!(priv(rp)->s_flags & SYS_PROC))
		return EPERM;
#endif
	data_copy(caller->p_endpoint, m_ptr->m_lsys_krn_sys_privctl.arg_ptr,
		KERNEL, (vir_bytes) &irq, sizeof(irq));
	/* Add the IRQ. */
	return priv_add_irq(rp, irq);

  case SYS_PRIV_QUERY_MEM:
  {
	phys_bytes addr, limit;
  	struct priv *sp;
	/* See if a certain process is allowed to map in certain physical
	 * memory.
	 */
	addr = (phys_bytes) m_ptr->m_lsys_krn_sys_privctl.phys_start;
	limit = addr + (phys_bytes) m_ptr->m_lsys_krn_sys_privctl.phys_len - 1;
	if(limit < addr)
		return EPERM; // EPERM might be undefined
	if(!(sp = priv(rp)))
		return EPERM; // EPERM might be undefined
	for(i = 0; i < sp->s_nr_mem_range; i++) {
		if(addr >= sp->s_mem_tab[i].mr_base &&
		   limit <= sp->s_mem_tab[i].mr_limit)
			return OK;
	}
	return EPERM; // EPERM might be undefined
  }

  case SYS_PRIV_UPDATE_SYS:
	/* Update the privilege structure of a system process. */
	if(!m_ptr->m_lsys_krn_sys_privctl.arg_ptr) return EINVAL; // EINVAL might be undefined

	/* Copy privilege structure from caller */
	if((r=data_copy(caller->p_endpoint,
		m_ptr->m_lsys_krn_sys_privctl.arg_ptr, KERNEL,
		(vir_bytes) &priv_store, sizeof(priv_store))) != OK) // MODIFIED priv to priv_store
		return r;

	/* Override settings in existing privilege structure. */
	if((r = update_priv(rp, &priv_store)) != OK) { // MODIFIED priv to priv_store
		return r;
	}

	return(OK);

  default:
	kprintf_stub("do_privctl: bad request %d\n", // MODIFIED
		m_ptr->m_lsys_krn_sys_privctl.request);
	return EINVAL; // EINVAL might be undefined
  }
}

/*===========================================================================*
 *				update_priv				     *
 *===========================================================================*/
static int update_priv(struct proc *rp, struct priv *priv_param) // Renamed priv to priv_param
{
/* Update the privilege structure of a given process. */

  int i;

  /* Copy flags and signal managers. */
  priv(rp)->s_flags = priv_param->s_flags; // MODIFIED
  priv(rp)->s_init_flags = priv_param->s_init_flags; // MODIFIED
  priv(rp)->s_sig_mgr = priv_param->s_sig_mgr; // MODIFIED
  priv(rp)->s_bak_sig_mgr = priv_param->s_bak_sig_mgr; // MODIFIED

  /* Copy IRQs. */
  if(priv_param->s_flags & CHECK_IRQ) { // MODIFIED
	if (priv_param->s_nr_irq < 0 || priv_param->s_nr_irq > NR_IRQ) // MODIFIED
		return EINVAL; // EINVAL might be undefined
	priv(rp)->s_nr_irq= priv_param->s_nr_irq; // MODIFIED
	for (i= 0; i<priv_param->s_nr_irq; i++) // MODIFIED
  	{
		priv(rp)->s_irq_tab[i]= priv_param->s_irq_tab[i]; // MODIFIED
#if PRIV_DEBUG
		kprintf_stub("do_privctl: adding IRQ %d for %d\n", // MODIFIED
  			priv(rp)->s_irq_tab[i], rp->p_endpoint);
#endif
  	}
  }

  /* Copy I/O ranges. */
  if(priv_param->s_flags & CHECK_IO_PORT) { // MODIFIED
	if (priv_param->s_nr_io_range < 0 || priv_param->s_nr_io_range > NR_IO_RANGE) // MODIFIED
		return EINVAL; // EINVAL might be undefined
	priv(rp)->s_nr_io_range= priv_param->s_nr_io_range; // MODIFIED
	for (i= 0; i<priv_param->s_nr_io_range; i++) // MODIFIED
  	{
		priv(rp)->s_io_tab[i]= priv_param->s_io_tab[i]; // MODIFIED
#if PRIV_DEBUG
		kprintf_stub("do_privctl: adding I/O range [%x..%x] for %d\n", // MODIFIED
  			priv(rp)->s_io_tab[i].ior_base,
  			priv(rp)->s_io_tab[i].ior_limit,
  			rp->p_endpoint);
#endif
  	}
  }

  /* Copy memory ranges. */
  if(priv_param->s_flags & CHECK_MEM) { // MODIFIED
	if (priv_param->s_nr_mem_range < 0 || priv_param->s_nr_mem_range > NR_MEM_RANGE) // MODIFIED
		return EINVAL; // EINVAL might be undefined
	priv(rp)->s_nr_mem_range= priv_param->s_nr_mem_range; // MODIFIED
	for (i= 0; i<priv_param->s_nr_mem_range; i++) // MODIFIED
  	{
		priv(rp)->s_mem_tab[i]= priv_param->s_mem_tab[i]; // MODIFIED
#if PRIV_DEBUG
		kprintf_stub("do_privctl: adding mem range [%x..%x] for %d\n", // MODIFIED
  			priv(rp)->s_mem_tab[i].mr_base,
  			priv(rp)->s_mem_tab[i].mr_limit,
  			rp->p_endpoint);
#endif
  	}
  }

  /* Copy trap mask. */
  priv(rp)->s_trap_mask = priv_param->s_trap_mask; // MODIFIED

  /* Copy target mask. */
#if PRIV_DEBUG
  kprintf_stub("do_privctl: Setting ipc target mask for %d:"); // MODIFIED
  for (i=0; i < NR_SYS_PROCS; i += BITCHUNK_BITS) {
	kprintf_stub(" %08x", get_sys_bits(priv_param->s_ipc_to, i)); // MODIFIED
  }
  kprintf_stub("\n"); // MODIFIED
#endif

  fill_sendto_mask(rp, &priv_param->s_ipc_to); // MODIFIED

#if PRIV_DEBUG
  kprintf_stub("do_privctl: Set ipc target mask for %d:"); // MODIFIED
  for (i=0; i < NR_SYS_PROCS; i += BITCHUNK_BITS) {
	kprintf_stub(" %08x", get_sys_bits(priv(rp)->s_ipc_to, i)); // MODIFIED
  }
  kprintf_stub("\n"); // MODIFIED
#endif

  /* Copy kernel call mask. */
  kmemcpy(priv(rp)->s_k_call_mask, priv_param->s_k_call_mask, // MODIFIED
  	sizeof(priv(rp)->s_k_call_mask));

  return OK;
}

#endif /* USE_PRIVCTL */
