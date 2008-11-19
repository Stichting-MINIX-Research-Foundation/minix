/* The kernel call implemented in this file:
 *   m_type:	SYS_GETINFO
 *
 * The parameters for this kernel call are:
 *    m1_i3:	I_REQUEST	(what info to get)	
 *    m1_p1:	I_VAL_PTR 	(where to put it)	
 *    m1_i1:	I_VAL_LEN 	(maximum length expected, optional)	
 *    m1_p2:	I_VAL_PTR2	(second, optional pointer)	
 *    m1_i2:	I_VAL_LEN2_E	(second length or process nr)	
 */

#include "../system.h"
#include "../vm.h"


#if USE_GETINFO

/*===========================================================================*
 *			        do_getinfo				     *
 *===========================================================================*/
PUBLIC int do_getinfo(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
/* Request system information to be copied to caller's address space. This
 * call simply copies entire data structures to the caller.
 */
  size_t length;
  vir_bytes src_vir; 
  int proc_nr, nr_e, nr, hz;
  struct proc *caller;
  phys_bytes ph;

  caller = proc_addr(who_p);

  /* Set source address and length based on request type. */
  switch (m_ptr->I_REQUEST) {
    case GET_MACHINE: {
        length = sizeof(struct machine);
        src_vir = (vir_bytes) &machine;
        break;
    }
    case GET_KINFO: {
        length = sizeof(struct kinfo);
        src_vir = (vir_bytes) &kinfo;
        break;
    }
    case GET_LOADINFO: {
        length = sizeof(struct loadinfo);
        src_vir = (vir_bytes) &kloadinfo;
        break;
    }
    case GET_HZ: {
        length = sizeof(hz);
        src_vir = (vir_bytes) &hz;
	hz = HZ;
        break;
    }
    case GET_IMAGE: {
        length = sizeof(struct boot_image) * NR_BOOT_PROCS;
        src_vir = (vir_bytes) image;
        break;
    }
    case GET_IRQHOOKS: {
        length = sizeof(struct irq_hook) * NR_IRQ_HOOKS;
        src_vir = (vir_bytes) irq_hooks;
        break;
    }
    case GET_SCHEDINFO: {
        /* This is slightly complicated because we need two data structures
         * at once, otherwise the scheduling information may be incorrect.
         * Copy the queue heads and fall through to copy the process table. 
         */
	if((ph=umap_local(caller, D, (vir_bytes) m_ptr->I_VAL_PTR2,length)) == 0)
		return EFAULT;
        length = sizeof(struct proc *) * NR_SCHED_QUEUES;
  	CHECKRANGE_OR_SUSPEND(proc_addr(who_p), ph, length, 1);
	data_copy(SYSTEM, (vir_bytes) rdy_head,
		who_e, (vir_bytes) m_ptr->I_VAL_PTR2, length);
        /* fall through to GET_PROCTAB */
    }
    case GET_PROCTAB: {
        length = sizeof(struct proc) * (NR_PROCS + NR_TASKS);
        src_vir = (vir_bytes) proc;
        break;
    }
    case GET_PRIVTAB: {
        length = sizeof(struct priv) * (NR_SYS_PROCS);
        src_vir = (vir_bytes) priv;
        break;
    }
    case GET_PROC: {
        nr_e = (m_ptr->I_VAL_LEN2_E == SELF) ?
		who_e : m_ptr->I_VAL_LEN2_E;
	if(!isokendpt(nr_e, &nr)) return EINVAL; /* validate request */
        length = sizeof(struct proc);
        src_vir = (vir_bytes) proc_addr(nr);
        break;
    }
    case GET_WHOAMI: {
	int len;
	/* GET_WHOAMI uses m3 and only uses the message contents for info. */
	m_ptr->GIWHO_EP = who_e;
	len = MIN(sizeof(m_ptr->GIWHO_NAME), sizeof(caller->p_name))-1;
	strncpy(m_ptr->GIWHO_NAME, caller->p_name, len);
	m_ptr->GIWHO_NAME[len] = '\0';
	return OK;
    }
    case GET_MONPARAMS: {
        src_vir = (vir_bytes) params_buffer;
	length = sizeof(params_buffer);
        break;
    }
    case GET_RANDOMNESS: {		
        static struct randomness copy;		/* copy to keep counters */
	int i;

        copy = krandom;
        for (i= 0; i<RANDOM_SOURCES; i++) {
  		krandom.bin[i].r_size = 0;	/* invalidate random data */
  		krandom.bin[i].r_next = 0;
	}
    	length = sizeof(struct randomness);
    	src_vir = (vir_bytes) &copy;
    	break;
    }
    case GET_KMESSAGES: {
        length = sizeof(struct kmessages);
        src_vir = (vir_bytes) &kmess;
        break;
    }
#if DEBUG_TIME_LOCKS
    case GET_LOCKTIMING: {
    length = sizeof(timingdata);
    src_vir = (vir_bytes) timingdata;
    break;
    }
#endif

    case GET_IRQACTIDS: {
        length = sizeof(irq_actids);
        src_vir = (vir_bytes) irq_actids;
        break;
    }

    case GET_PRIVID:
	if (!isokendpt(m_ptr->I_VAL_LEN2_E, &proc_nr)) 
		return EINVAL;
	return proc_addr(proc_nr)->p_priv->s_id;

    default:
	kprintf("do_getinfo: invalid request %d\n", m_ptr->I_REQUEST);
        return(EINVAL);
  }

  /* Try to make the actual copy for the requested data. */
  if (m_ptr->I_VAL_LEN > 0 && length > m_ptr->I_VAL_LEN) return (E2BIG);
  if((ph=umap_local(caller, D, (vir_bytes) m_ptr->I_VAL_PTR,length)) == 0)
	return EFAULT;
  CHECKRANGE_OR_SUSPEND(caller, ph, length, 1);
  data_copy(SYSTEM, src_vir, who_e, (vir_bytes) m_ptr->I_VAL_PTR, length);
  return(OK);
}

#endif /* USE_GETINFO */

