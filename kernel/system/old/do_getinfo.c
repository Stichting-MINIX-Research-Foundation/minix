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


#include "../kernel.h"
#include "../system.h"


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
  int proc_nr;

  /* First get the process number and verify it. */
  proc_nr = (m_ptr->I_PROC_NR == SELF) ? m_ptr->m_source : m_ptr->I_PROC_NR;
  if (! isokprocn(proc_nr)) {
  	printf("Invalid process number: %d from %d\n", proc_nr, m_ptr->m_source); 
  	return(EINVAL);
  }

  /* Set source address and length based on request type. */      
  switch (m_ptr->I_REQUEST) {	
    case GET_KENVIRON: {
  	struct kenviron kenv;
  	extern int end;
    	kenv.pc_at = pc_at; 		kenv.ps_mca = ps_mca;
    	kenv.processor = processor; 	kenv.protected = protected_mode;
    	kenv.ega = ega; 		kenv.vga = vga;
        kenv.proc_addr = (vir_bytes) proc;
        kenv.kmem_start = vir2phys(0);
        kenv.kmem_end = vir2phys(&end);
    	length = sizeof(struct kenviron);
    	src_phys = vir2phys(&kenv);
    	break;
    }
    case GET_IMAGE: {
    	length = sizeof(struct system_image) * IMAGE_SIZE;
    	src_phys = vir2phys(image);
        break;
    }
    case GET_IRQTAB: {
    	length = sizeof(struct irqtab) * NR_IRQ_VECTORS;
    	src_phys = vir2phys(irqtab);
        break;
    }
    case GET_MEMCHUNKS: {
        length = sizeof(struct memory) * NR_MEMS;
        src_phys = vir2phys(mem);
        break;
    }
    case GET_SCHEDINFO: {
        /* This is slightly complicated because we need several variables
         * at once, otherwise the scheduling information may be incorrect.
         */
        length = sizeof(struct proc *) * NR_SCHED_QUEUES;
        src_phys = vir2phys(rdy_head);
        dst_phys = numap_local(m_ptr->m_source, (vir_bytes) m_ptr->I_KEY_PTR,
        	 length); 
        if (src_phys == 0 || dst_phys == 0) return(EFAULT);
        phys_copy(src_phys, dst_phys, length);
        /* Fall through to also get a copy of the process table. */
    }
    case GET_PROCTAB: {
    	length = sizeof(struct proc) * (NR_PROCS + NR_TASKS);
    	src_phys = vir2phys(proc);
        break;
    }
    case GET_PROC: {
    	if (! isokprocn(m_ptr->I_KEY_LEN)) return(EINVAL);
    	length = sizeof(struct proc);
    	src_phys = vir2phys(proc_addr(m_ptr->I_KEY_LEN));
        break;
    }
    case GET_MONPARAMS: {
    	src_phys = mon_params;	/* already is a physical address! */
    	length = mon_parmsize;
    	break;
    }
    case GET_KENV: {		/* get one string by name */
  	char key[32];		/* boot variable key provided by caller */
  	char *val;		/* pointer to actual boot variable value */
    	if (m_ptr->I_KEY_LEN > sizeof(key)) return(EINVAL);
    	if (vir_copy(proc_nr, (vir_bytes) m_ptr->I_KEY_PTR,
    	    SYSTASK, (vir_bytes) key, m_ptr->I_KEY_LEN) != OK) return(EFAULT);
    	if ((val=getkenv(key)) == NULL) return(ESRCH);
        length = strlen(val) + 1;
        src_phys = vir2phys(val);
        break;
    }
    case GET_PROCNR: {
        length = sizeof(int);
        if (m_ptr->I_KEY_LEN == 0) {		/* get own process nr */
            src_phys = vir2phys(&proc_nr);	
        } else {				/* lookup nr by name */
  	    int proc_found = FALSE;
  	    struct proc *pp;
  	    char key[8];	/* storage for process name to lookup */
    	    if (m_ptr->I_KEY_LEN > sizeof(key)) return(EINVAL);
    	    if (vir_copy(proc_nr, (vir_bytes) m_ptr->I_KEY_PTR, SYSTASK,
    	        (vir_bytes) key, m_ptr->I_KEY_LEN) != OK) return(EFAULT);
  	    for (pp= BEG_PROC_ADDR; pp<END_PROC_ADDR; pp++) {
		if (strncmp(pp->p_name, key, m_ptr->I_KEY_LEN) == 0) {
			src_phys = vir2phys(&(pp->p_nr));
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
  dst_phys = numap_local(proc_nr, (vir_bytes) m_ptr->I_VAL_PTR, length); 
  if (src_phys == 0 || dst_phys == 0) return(EFAULT);
  phys_copy(src_phys, dst_phys, length);
  return(OK);
}


