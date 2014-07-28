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
#include "kernel/ipc.h"
#include <signal.h>
#include <string.h>
#include <minix/endpoint.h>

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
  struct priv priv;
  int irq;

  /* Check whether caller is allowed to make this call. Privileged processes 
   * can only update the privileges of processes that are inhibited from 
   * running by the RTS_NO_PRIV flag. This flag is set when a privileged process
   * forks. 
   */
  if (! (priv(caller)->s_flags & SYS_PROC)) return(EPERM);
  if(m_ptr->m_lsys_krn_sys_privctl.endpt == SELF) okendpt(caller->p_endpoint,
	&proc_nr);
  else if(!isokendpt(m_ptr->m_lsys_krn_sys_privctl.endpt, &proc_nr))
	return(EINVAL);
  rp = proc_addr(proc_nr);

  switch(m_ptr->m_lsys_krn_sys_privctl.request)
  {
  case SYS_PRIV_ALLOW:
	/* Allow process to run. Make sure its privilege structure has already
	 * been set.
	 */
	if (!RTS_ISSET(rp, RTS_NO_PRIV) || priv(rp)->s_proc_nr == NONE) {
		return(EPERM);
	}
	RTS_UNSET(rp, RTS_NO_PRIV);
	return(OK);

  case SYS_PRIV_YIELD:
	/* Allow process to run and suspend the caller. */
	if (!RTS_ISSET(rp, RTS_NO_PRIV) || priv(rp)->s_proc_nr == NONE) {
		return(EPERM);
	}
	RTS_SET(caller, RTS_NO_PRIV);
	RTS_UNSET(rp, RTS_NO_PRIV);
	return(OK);

  case SYS_PRIV_DISALLOW:
	/* Disallow process from running. */
	if (RTS_ISSET(rp, RTS_NO_PRIV)) return(EPERM);
	RTS_SET(rp, RTS_NO_PRIV);
	return(OK);

  case SYS_PRIV_SET_SYS:
	/* Set a privilege structure of a blocked system process. */
	if (! RTS_ISSET(rp, RTS_NO_PRIV)) return(EPERM);

	/* Check whether a static or dynamic privilege id must be allocated. */
	priv_id = NULL_PRIV_ID;
	if (m_ptr->m_lsys_krn_sys_privctl.arg_ptr)
	{
		/* Copy privilege structure from caller */
		if((r=data_copy(caller->p_endpoint,
			m_ptr->m_lsys_krn_sys_privctl.arg_ptr, KERNEL,
			(vir_bytes) &priv, sizeof(priv))) != OK)
			return r;

		/* See if the caller wants to assign a static privilege id. */
		if(!(priv.s_flags & DYN_PRIV_ID)) {
			priv_id = priv.s_id;
		}
	}

	/* Make sure this process has its own privileges structure. This may
	 * fail, since there are only a limited number of system processes.
	 * Then copy privileges from the caller and restore some defaults.
	 */
	if ((i=get_priv(rp, priv_id)) != OK)
	{
		printf("do_privctl: unable to allocate priv_id %d: %d\n",
			priv_id, i);
		return(i);
	}
	priv_id = priv(rp)->s_id;		/* backup privilege id */
	*priv(rp) = *priv(caller);		/* copy from caller */
	priv(rp)->s_id = priv_id;		/* restore privilege id */
	priv(rp)->s_proc_nr = proc_nr;		/* reassociate process nr */

	for (i=0; i< NR_SYS_CHUNKS; i++)		/* remove pending: */
	      priv(rp)->s_notify_pending.chunk[i] = 0;	/* - notifications */
	priv(rp)->s_int_pending = 0;			/* - interrupts */
	(void) sigemptyset(&priv(rp)->s_sig_pending);	/* - signals */
	reset_kernel_timer(&priv(rp)->s_alarm_timer);	/* - alarm */
	priv(rp)->s_asyntab= -1;			/* - asynsends */
	priv(rp)->s_asynsize= 0;
	priv(rp)->s_diag_sig = FALSE;		/* no request for diag sigs */

	/* Set defaults for privilege bitmaps. */
	priv(rp)->s_flags= DSRV_F;           /* privilege flags */
	priv(rp)->s_trap_mask= DSRV_T;       /* allowed traps */
	memset(&map, 0, sizeof(map));
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
	 * no IRQs, no grant table
	 */
	priv(rp)->s_nr_io_range= 0;
	priv(rp)->s_nr_mem_range= 0;
	priv(rp)->s_nr_irq= 0;
	priv(rp)->s_grant_table= 0;
	priv(rp)->s_grant_entries= 0;

	/* Override defaults if the caller has supplied a privilege structure. */
	if (m_ptr->m_lsys_krn_sys_privctl.arg_ptr)
	{
		if((r = update_priv(rp, &priv)) != OK) {
			return r;
		} 
	}

	return(OK);

  case SYS_PRIV_SET_USER:
	/* Set a privilege structure of a blocked user process. */
	if (!RTS_ISSET(rp, RTS_NO_PRIV)) return(EPERM);

	/* Link the process to the privilege structure of the root user
	 * process all the user processes share.
	 */
	priv(rp) = priv_addr(USER_PRIV_ID);

	return(OK);

  case SYS_PRIV_ADD_IO:
	if (RTS_ISSET(rp, RTS_NO_PRIV))
		return(EPERM);

	/* Only system processes get I/O resources? */
	if (!(priv(rp)->s_flags & SYS_PROC))
		return EPERM;

#if 0 /* XXX -- do we need a call for this? */
	if (strcmp(rp->p_name, "fxp") == 0 ||
		strcmp(rp->p_name, "rtl8139") == 0)
	{
		printf("setting ipc_stats_target to %d\n", rp->p_endpoint);
		ipc_stats_target= rp->p_endpoint;
	}
#endif

	/* Get the I/O range */
	data_copy(caller->p_endpoint, m_ptr->m_lsys_krn_sys_privctl.arg_ptr,
		KERNEL, (vir_bytes) &io_range, sizeof(io_range));
	priv(rp)->s_flags |= CHECK_IO_PORT;	/* Check I/O accesses */

	for (i = 0; i < priv(rp)->s_nr_io_range; i++) {
		if (priv(rp)->s_io_tab[i].ior_base == io_range.ior_base &&
			priv(rp)->s_io_tab[i].ior_limit == io_range.ior_limit)
			return OK;
	}

	i= priv(rp)->s_nr_io_range;
	if (i >= NR_IO_RANGE) {
		printf("do_privctl: %d already has %d i/o ranges.\n",
			rp->p_endpoint, i);
		return ENOMEM;
	}

	priv(rp)->s_io_tab[i].ior_base= io_range.ior_base;
	priv(rp)->s_io_tab[i].ior_limit= io_range.ior_limit;
	priv(rp)->s_nr_io_range++;

	return OK;

  case SYS_PRIV_ADD_MEM:
	if (RTS_ISSET(rp, RTS_NO_PRIV))
		return(EPERM);

	/* Only system processes get memory resources? */
	if (!(priv(rp)->s_flags & SYS_PROC))
		return EPERM;

	/* Get the memory range */
	if((r=data_copy(caller->p_endpoint,
		m_ptr->m_lsys_krn_sys_privctl.arg_ptr, KERNEL,
		(vir_bytes) &mem_range, sizeof(mem_range))) != OK)
		return r;
	priv(rp)->s_flags |= CHECK_MEM;	/* Check memory mappings */

	/* When restarting a driver, check if it already has the permission */
	for (i = 0; i < priv(rp)->s_nr_mem_range; i++) {
		if (priv(rp)->s_mem_tab[i].mr_base == mem_range.mr_base &&
			priv(rp)->s_mem_tab[i].mr_limit == mem_range.mr_limit)
			return OK;
	}

	i= priv(rp)->s_nr_mem_range;
	if (i >= NR_MEM_RANGE) {
		printf("do_privctl: %d already has %d mem ranges.\n",
			rp->p_endpoint, i);
		return ENOMEM;
	}

	priv(rp)->s_mem_tab[i].mr_base= mem_range.mr_base;
	priv(rp)->s_mem_tab[i].mr_limit= mem_range.mr_limit;
	priv(rp)->s_nr_mem_range++;

	return OK;

  case SYS_PRIV_ADD_IRQ:
	if (RTS_ISSET(rp, RTS_NO_PRIV))
		return(EPERM);

	/* Only system processes get IRQs? */
	if (!(priv(rp)->s_flags & SYS_PROC))
		return EPERM;

	data_copy(caller->p_endpoint, m_ptr->m_lsys_krn_sys_privctl.arg_ptr,
		KERNEL, (vir_bytes) &irq, sizeof(irq));
	priv(rp)->s_flags |= CHECK_IRQ;	/* Check IRQs */

	/* When restarting a driver, check if it already has the permission */
	for (i = 0; i < priv(rp)->s_nr_irq; i++) {
		if (priv(rp)->s_irq_tab[i] == irq)
			return OK;
	}

	i= priv(rp)->s_nr_irq;
	if (i >= NR_IRQ) {
		printf("do_privctl: %d already has %d irq's.\n",
			rp->p_endpoint, i);
		return ENOMEM;
	}
	priv(rp)->s_irq_tab[i]= irq;
	priv(rp)->s_nr_irq++;

	return OK;
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
		return EPERM;
	if(!(sp = priv(rp)))
		return EPERM;
	if (!(sp->s_flags & SYS_PROC))
		return EPERM;
	for(i = 0; i < sp->s_nr_mem_range; i++) {
		if(addr >= sp->s_mem_tab[i].mr_base &&
		   limit <= sp->s_mem_tab[i].mr_limit)
			return OK;
	}
	return EPERM;
  }

  case SYS_PRIV_UPDATE_SYS:
	/* Update the privilege structure of a system process. */
	if(!m_ptr->m_lsys_krn_sys_privctl.arg_ptr) return EINVAL;

	/* Copy privilege structure from caller */
	if((r=data_copy(caller->p_endpoint,
		m_ptr->m_lsys_krn_sys_privctl.arg_ptr, KERNEL,
		(vir_bytes) &priv, sizeof(priv))) != OK)
		return r;

	/* Override settings in existing privilege structure. */
	if((r = update_priv(rp, &priv)) != OK) {
		return r;
	}

	return(OK);

  default:
	printf("do_privctl: bad request %d\n",
		m_ptr->m_lsys_krn_sys_privctl.request);
	return EINVAL;
  }
}

/*===========================================================================*
 *				update_priv				     *
 *===========================================================================*/
static int update_priv(struct proc *rp, struct priv *priv)
{
/* Update the privilege structure of a given process. */

  int i;

  /* Copy s_flags and signal managers. */
  priv(rp)->s_flags = priv->s_flags;
  priv(rp)->s_sig_mgr = priv->s_sig_mgr;
  priv(rp)->s_bak_sig_mgr = priv->s_bak_sig_mgr;

  /* Copy IRQs. */
  if(priv->s_flags & CHECK_IRQ) {
  	if (priv->s_nr_irq < 0 || priv->s_nr_irq > NR_IRQ)
  		return EINVAL;
  	priv(rp)->s_nr_irq= priv->s_nr_irq;
  	for (i= 0; i<priv->s_nr_irq; i++)
  	{
  		priv(rp)->s_irq_tab[i]= priv->s_irq_tab[i];
#if PRIV_DEBUG
  		printf("do_privctl: adding IRQ %d for %d\n",
  			priv(rp)->s_irq_tab[i], rp->p_endpoint);
#endif
  	}
  }

  /* Copy I/O ranges. */
  if(priv->s_flags & CHECK_IO_PORT) {
  	if (priv->s_nr_io_range < 0 || priv->s_nr_io_range > NR_IO_RANGE)
  		return EINVAL;
  	priv(rp)->s_nr_io_range= priv->s_nr_io_range;
  	for (i= 0; i<priv->s_nr_io_range; i++)
  	{
  		priv(rp)->s_io_tab[i]= priv->s_io_tab[i];
#if PRIV_DEBUG
  		printf("do_privctl: adding I/O range [%x..%x] for %d\n",
  			priv(rp)->s_io_tab[i].ior_base,
  			priv(rp)->s_io_tab[i].ior_limit,
  			rp->p_endpoint);
#endif
  	}
  }

  /* Copy memory ranges. */
  if(priv->s_flags & CHECK_MEM) {
  	if (priv->s_nr_mem_range < 0 || priv->s_nr_mem_range > NR_MEM_RANGE)
  		return EINVAL;
  	priv(rp)->s_nr_mem_range= priv->s_nr_mem_range;
  	for (i= 0; i<priv->s_nr_mem_range; i++)
  	{
  		priv(rp)->s_mem_tab[i]= priv->s_mem_tab[i];
#if PRIV_DEBUG
  		printf("do_privctl: adding mem range [%x..%x] for %d\n",
  			priv(rp)->s_mem_tab[i].mr_base,
  			priv(rp)->s_mem_tab[i].mr_limit,
  			rp->p_endpoint);
#endif
  	}
  }

  /* Copy trap mask. */
  priv(rp)->s_trap_mask = priv->s_trap_mask;

  /* Copy target mask. */
#if PRIV_DEBUG
  printf("do_privctl: Setting ipc target mask for %d:");
  for (i=0; i < NR_SYS_PROCS; i += BITCHUNK_BITS) {
  	printf(" %08x", get_sys_bits(priv->s_ipc_to, i));
  }
  printf("\n");
#endif

  fill_sendto_mask(rp, &priv->s_ipc_to);

#if PRIV_DEBUG
  printf("do_privctl: Set ipc target mask for %d:");
  for (i=0; i < NR_SYS_PROCS; i += BITCHUNK_BITS) {
  	printf(" %08x", get_sys_bits(priv(rp)->s_ipc_to, i));
  }
  printf("\n");
#endif

  /* Copy kernel call mask. */
  memcpy(priv(rp)->s_k_call_mask, priv->s_k_call_mask,
  	sizeof(priv(rp)->s_k_call_mask));

  return OK;
}

#endif /* USE_PRIVCTL */

