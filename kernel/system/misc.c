#include "../kernel.h"
#include "../system.h"
#include <unistd.h>
INIT_ASSERT


/*===========================================================================*
 *			          do_unused				     *
 *===========================================================================*/
PUBLIC int do_unused(m)
message *m;				/* pointer to request message */
{
  kprintf("SYS task got illegal request from %d.", m->m_source);
  return(EBADREQUEST);		/* illegal message type */
}


/*===========================================================================*
 *			          do_random				     *
 *===========================================================================*/
PUBLIC int do_random(m)
message *m;				/* pointer to request message */
{
  return(ENOSYS);			/* no yet implemented */
}


/* The system call implemented in this file:
 *   m_type:	SYS_ABORT
 *
 * The parameters for this system call are:
 *    m1_i1:	ABRT_HOW 	(how to abort, possibly fetch monitor params)	
 *    m1_i2:	ABRT_MON_PROC 	(proc nr to get monitor params from)	
 *    m1_i3:	ABRT_MON_LEN	(length of monitor params)
 *    m1_p1:	ABRT_MON_ADDR 	(virtual address of params)	
 */

/*===========================================================================*
 *				do_abort				     *
 *===========================================================================*/
PUBLIC int do_abort(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* Handle sys_abort. MINIX is unable to continue. This can originate in the
 * PM (normal abort or panic) or FS (panic), or TTY (a CTRL-ALT-DEL or ESC
 * after debugging dumps).
 */
  register struct proc *rp;
  phys_bytes src_phys;
  vir_bytes len;
  int how = m_ptr->ABRT_HOW;
  
  rp = proc_addr(m_ptr->m_source);

  if (how == RBT_MONITOR) {
	/* The monitor is to run user specified instructions. */
	len = m_ptr->ABRT_MON_LEN + 1;
	assert(len <= kinfo.params_size);
	src_phys = numap_local(m_ptr->ABRT_MON_PROC, 
		(vir_bytes) m_ptr->ABRT_MON_ADDR, len);
	assert(src_phys != 0);
	phys_copy(src_phys, kinfo.params_base, (phys_bytes) len);
  }
  prepare_shutdown(how);
  return(OK);				/* pro-forma (really EDISASTER) */
}

/* The system call implemented in this file:
 *   m_type:	SYS_GETINFO
 *
 * The parameters for this system call are:
 *    m1_i3:	I_REQUEST	(what info to get)	
 *    m1_i4:	I_PROC_NR	(process to store value at)	
 *    m1_p1:	I_VAL_PTR 	(where to put it)	
 *    m1_i1:	I_VAL_LEN 	(maximum length expected, optional)	
 *    m1_p2:	I_KEY_PTR	(environment variable key)	
 *    m1_i2:	I_KEY_LEN	(lenght of environment variable key)	
 * 
 * Author:
 *    Jorrit N. Herder <jnherder@cs.vu.nl>
 */

/*===========================================================================*
 *			        do_getinfo				     *
 *===========================================================================*/
PUBLIC int do_getinfo(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
/* Request system information to be copied to caller's address space. */
  size_t length;
  phys_bytes src_phys; 
  phys_bytes dst_phys; 
  int proc_nr, nr;

  /* Set source address and length based on request type. */      
  switch (m_ptr->I_REQUEST) {	
    case GET_MACHINE: {
    	length = sizeof(struct machine);
    	src_phys = vir2phys(&machine);
    	break;
    }
    case GET_KINFO: {
    	length = sizeof(struct kinfo);
    	src_phys = vir2phys(&kinfo);
    	break;
    }
    case GET_IMAGE: {
    	length = sizeof(struct system_image) * IMAGE_SIZE;
    	src_phys = vir2phys(image);
        break;
    }
    case GET_IRQHOOKS: {
    	length = sizeof(struct irq_hook) * NR_IRQ_HOOKS;
    	src_phys = vir2phys(irq_hooks);
        break;
    }
    case GET_MEMCHUNKS: {
        length = sizeof(struct memory) * NR_MEMS;
        src_phys = vir2phys(mem);
        break;
    }
    case GET_SCHEDINFO: {
        /* This is slightly complicated because we need two data structures
         * at once, otherwise the scheduling information may be incorrect.
         * Copy the queue heads and fall through to copy the process table. 
         */
        length = sizeof(struct proc *) * NR_SCHED_QUEUES;
        src_phys = vir2phys(rdy_head);
        dst_phys = numap_local(m_ptr->m_source, (vir_bytes) m_ptr->I_KEY_PTR,
        	 length); 
        if (src_phys == 0 || dst_phys == 0) return(EFAULT);
        phys_copy(src_phys, dst_phys, length);
    }
    case GET_PROCTAB: {
    	length = sizeof(struct proc) * (NR_PROCS + NR_TASKS);
    	src_phys = vir2phys(proc);
        break;
    }
    case GET_PROC: {
    	nr = (m_ptr->I_KEY_LEN == SELF) ? m_ptr->m_source : m_ptr->I_KEY_LEN;
    	if (! isokprocn(nr)) return(EINVAL);
    	length = sizeof(struct proc);
    	src_phys = vir2phys(proc_addr(nr));
        break;
    }
    case GET_MONPARAMS: {
    	src_phys = kinfo.params_base;	/* already is a physical address! */
    	length = kinfo.params_size;
    	break;
    }
    case GET_PROCNR: {
        if (m_ptr->I_KEY_LEN == 0) {		/* get own process nr */
	/* GET_PROCNR functionality will be moved to the Process Manager! */
        kprintf("GET_PROCNR (own) from %d\n", m_ptr->m_source);
            src_phys = vir2phys(&proc_nr);	
            length = sizeof(int);
        } else {				/* lookup nr by name */
  	    int proc_found = FALSE;
  	    struct proc *pp;
	    struct vir_addr vsrc, vdst;
  	    char key[8];	/* storage for process name to lookup */
	/* GET_PROCNR functionality will be moved to the Process Manager! */
        kprintf("GET_PROCNR (by name) from %d\n", m_ptr->m_source);
  proc_nr = m_ptr->m_source;	/* only caller can request copy */
    	    if (m_ptr->I_KEY_LEN > sizeof(key)) return(EINVAL);
	    vsrc.proc_nr = proc_nr; vsrc.segment = D; vsrc.offset = (vir_bytes) m_ptr->I_KEY_PTR;
	    vdst.proc_nr = SYSTASK, vdst.segment = D; vdst.offset = (vir_bytes) key;
	    if (virtual_copy(&vsrc, &vdst, m_ptr->I_KEY_LEN) != OK) return(EFAULT);
#if DEAD_CODE
    	    if (vir_copy(proc_nr, (vir_bytes) m_ptr->I_KEY_PTR, SYSTASK,
    	        (vir_bytes) key, m_ptr->I_KEY_LEN) != OK) return(EFAULT);
#endif
  	    for (pp=BEG_PROC_ADDR; pp<END_PROC_ADDR; pp++) {
		if (kstrncmp(pp->p_name, key, m_ptr->I_KEY_LEN) == 0) {
			src_phys = vir2phys(&(pp->p_nr));
            		length = sizeof(int);
			proc_found = TRUE;
			break;
		}
	    }
	    if (! proc_found) return(ESRCH);
        }
        break;
    }
    case GET_KMESSAGES: {
        length = sizeof(struct kmessages);
        src_phys = vir2phys(&kmess);
        break;
    }
    default:
        return(EINVAL);
  }

  /* Try to make the actual copy for the requested data. */
  if (m_ptr->I_VAL_LEN > 0 && length > m_ptr->I_VAL_LEN) return (E2BIG);
  proc_nr = m_ptr->m_source;	/* only caller can request copy */
  dst_phys = numap_local(proc_nr, (vir_bytes) m_ptr->I_VAL_PTR, length); 
  if (src_phys == 0 || dst_phys == 0) return(EFAULT);
  phys_copy(src_phys, dst_phys, length);
  return(OK);
}


