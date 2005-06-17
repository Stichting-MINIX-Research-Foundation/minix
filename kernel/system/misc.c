#include "../kernel.h"
#include "../system.h"
#include <unistd.h>
#include <minix/config.h>


/*===========================================================================*
 *			          do_unused				     *
 *===========================================================================*/
PUBLIC int do_unused(m)
message *m;				/* pointer to request message */
{
  kprintf("SYS task got illegal request from %d.", m->m_source);
  return(EBADREQUEST);		/* illegal message type */
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
  int how = m_ptr->ABRT_HOW;
  
  if (how == RBT_MONITOR) {
      /* The monitor is to run user specified instructions. */
      int proc_nr = m_ptr->ABRT_MON_PROC;
      int length = m_ptr->ABRT_MON_LEN + 1;
      vir_bytes src_vir = (vir_bytes) m_ptr->ABRT_MON_ADDR;
      phys_bytes src_phys = numap_local(proc_nr, src_vir, length);

      /* Validate length and address of shutdown code before copying. */
      if (length > kinfo.params_size || src_phys == 0) 
	  kprintf("Warning, skipping shutdown code\n", NO_NUM);
      else
          phys_copy(src_phys, kinfo.params_base, (phys_bytes) length);
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
        /* fall through */
    }
    case GET_PROCTAB: {
    	length = sizeof(struct proc) * (NR_PROCS + NR_TASKS);
    	src_phys = vir2phys(proc);
        break;
    }
    case GET_PROC: {
    	nr = (m_ptr->I_KEY_LEN == SELF) ? m_ptr->m_source : m_ptr->I_KEY_LEN;
    	if (! isokprocn(nr)) return(EINVAL);	/* validate request */
    	length = sizeof(struct proc);
    	src_phys = vir2phys(proc_addr(nr));
        break;
    }
    case GET_MONPARAMS: {
    	src_phys = kinfo.params_base;		/* already is a physical */
    	length = kinfo.params_size;
    	break;
    }
    case GET_RANDOMNESS: {		
        struct randomness copy = krandom;	/* copy to keep counters */
  	krandom.r_next = krandom.r_size = 0;	/* invalidate random data */
    	length = sizeof(struct randomness);
    	src_phys = vir2phys(&copy);
    	break;
    }
    case GET_KMESSAGES: {
        length = sizeof(struct kmessages);
        src_phys = vir2phys(&kmess);
        break;
    }
#if ENABLE_LOCK_TIMING
    case GET_LOCKTIMING: {
	length = sizeof(timingdata);
	src_phys = vir2phys(timingdata);
	break;
    }
#endif
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


