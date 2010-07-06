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

#include <string.h>
#include <minix/endpoint.h>

#include "kernel/system.h"


#if USE_GETINFO

/*===========================================================================*
 *			        do_getinfo				     *
 *===========================================================================*/
PUBLIC int do_getinfo(struct proc * caller, message * m_ptr)
{
/* Request system information to be copied to caller's address space. This
 * call simply copies entire data structures to the caller.
 */
  size_t length;
  vir_bytes src_vir; 
  int nr_e, nr, r;
  int wipe_rnd_bin = -1;
  struct exec e_hdr;

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
        length = sizeof(system_hz);
        src_vir = (vir_bytes) &system_hz;
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
		caller->p_endpoint : m_ptr->I_VAL_LEN2_E;
	if(!isokendpt(nr_e, &nr)) return EINVAL; /* validate request */
        length = sizeof(struct proc);
        src_vir = (vir_bytes) proc_addr(nr);
        break;
    }
    case GET_PRIV: {
        nr_e = (m_ptr->I_VAL_LEN2_E == SELF) ?
            caller->p_endpoint : m_ptr->I_VAL_LEN2_E;
        if(!isokendpt(nr_e, &nr)) return EINVAL; /* validate request */
        length = sizeof(struct priv);
        src_vir = (vir_bytes) priv_addr(nr_to_id(nr));
        break;
    }
    case GET_WHOAMI: {
	int len;
	/* GET_WHOAMI uses m3 and only uses the message contents for info. */
	m_ptr->GIWHO_EP = caller->p_endpoint;
	len = MIN(sizeof(m_ptr->GIWHO_NAME), sizeof(caller->p_name))-1;
	strncpy(m_ptr->GIWHO_NAME, caller->p_name, len);
	m_ptr->GIWHO_NAME[len] = '\0';
	m_ptr->GIWHO_PRIVFLAGS = priv(caller)->s_flags;
	return OK;
    }
    case GET_MONPARAMS: {
        src_vir = (vir_bytes) params_buffer;
	length = sizeof(params_buffer);
        break;
    }
    case GET_RANDOMNESS: {		
        static struct k_randomness copy;	/* copy to keep counters */
	int i;

        copy = krandom;
        for (i= 0; i<RANDOM_SOURCES; i++) {
  		krandom.bin[i].r_size = 0;	/* invalidate random data */
  		krandom.bin[i].r_next = 0;
	}
    	length = sizeof(copy);
    	src_vir = (vir_bytes) &copy;
    	break;
    }
    case GET_RANDOMNESS_BIN: {		
	int bin = m_ptr->I_VAL_LEN2_E;

	if(bin < 0 || bin >= RANDOM_SOURCES) {
		printf("SYSTEM: GET_RANDOMNESS_BIN: %d out of range\n", bin);
		return EINVAL;
	}

	if(krandom.bin[bin].r_size < RANDOM_ELEMENTS)
		return ENOENT;

    	length = sizeof(krandom.bin[bin]);
    	src_vir = (vir_bytes) &krandom.bin[bin];

	wipe_rnd_bin = bin;

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
    case GET_IDLETSC: {
	struct proc * idl;

	idl = proc_addr(IDLE);
        length = sizeof(idl->p_cycles);
        src_vir = (vir_bytes) &idl->p_cycles;
        break;
    }
    case GET_AOUTHEADER: {
        int hdrindex, index = m_ptr->I_VAL_LEN2_E;
        if(index < 0 || index >= NR_BOOT_PROCS) {
            return EINVAL;
        }
        if (iskerneln(_ENDPOINT_P(image[index].endpoint))) { 
            hdrindex = 0;
        } else {
            hdrindex = 1 + index-NR_TASKS;
        }
        arch_get_aout_headers(hdrindex, &e_hdr);
        length = sizeof(e_hdr);
        src_vir = (vir_bytes) &e_hdr;
        break;
    }

    default:
	printf("do_getinfo: invalid request %d\n", m_ptr->I_REQUEST);
        return(EINVAL);
  }

  /* Try to make the actual copy for the requested data. */
  if (m_ptr->I_VAL_LEN > 0 && length > m_ptr->I_VAL_LEN) return (E2BIG);
  r = data_copy_vmcheck(caller, KERNEL, src_vir, caller->p_endpoint,
	(vir_bytes) m_ptr->I_VAL_PTR, length);

  if(r != OK) return r;

	if(wipe_rnd_bin >= 0 && wipe_rnd_bin < RANDOM_SOURCES) {
		krandom.bin[wipe_rnd_bin].r_size = 0;
		krandom.bin[wipe_rnd_bin].r_next = 0;
	}

  return(OK);
}

#endif /* USE_GETINFO */

