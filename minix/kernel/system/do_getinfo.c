/* The kernel call implemented in this file:
 *   m_type:	SYS_GETINFO
 *
 * The parameters for this kernel call are:
 *   m_lsys_krn_sys_getinfo.request	(what info to get)
 *   m_lsys_krn_sys_getinfo.val_ptr 	(where to put it)
 *   m_lsys_krn_sys_getinfo.val_len 	(maximum length expected, optional)
 *   m_lsys_krn_sys_getinfo.val_ptr2	(second, optional pointer)
 *   m_lsys_krn_sys_getinfo.val_len2_e	(second length or process nr)
 *
 * Upon return of the GETWHOAMI request the following parameters are used:
 *   m_krn_lsys_sys_getwhoami.endpt	(the caller endpoint)
 *   m_krn_lsys_sys_getwhoami.privflags	(the caller priviledes)
 *   m_krn_lsys_sys_getwhoami.initflags (the caller initflags)
 *   m_krn_lsys_sys_getwhoami.name	(the caller process name)
 *
 */

// #include <string.h> // Replaced

#include "kernel/system.h"

// Added kernel headers
#include <minix/kernel_types.h>
#include <klib/include/kprintf.h>
#include <klib/include/kstring.h>
#include <klib/include/kmemory.h>


#if USE_GETINFO

#include <minix/u64.h>      // Kept
// #include <sys/resource.h> // Removed

/*===========================================================================*
 *			        update_idle_time			     *
 *===========================================================================*/
static void update_idle_time(void)
{
	int i;
	struct proc * idl = proc_addr(IDLE);

	idl->p_cycles = make64(0, 0);

	for (i = 0; i < CONFIG_MAX_CPUS ; i++) {
		idl->p_cycles += get_cpu_var(i, idle_proc).p_cycles;
	}
}

/*===========================================================================*
 *			        do_getinfo				     *
 *===========================================================================*/
int do_getinfo(struct proc * caller, message * m_ptr)
{
/* Request system information to be copied to caller's address space. This
 * call simply copies entire data structures to the caller.
 */
  k_size_t length; // MODIFIED size_t
  vir_bytes src_vir; 
  int nr_e, nr, r;
  int wipe_rnd_bin = -1;
  struct proc *p;
  struct rusage r_usage; // This struct might be undefined now

  /* Set source address and length based on request type. */
  switch (m_ptr->m_lsys_krn_sys_getinfo.request) {
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
    case GET_CPUINFO: {
        length = sizeof(cpu_info);
        src_vir = (vir_bytes) &cpu_info;
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
	update_idle_time();
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
        nr_e = (m_ptr->m_lsys_krn_sys_getinfo.val_len2_e == SELF) ?
		caller->p_endpoint : m_ptr->m_lsys_krn_sys_getinfo.val_len2_e;
	if(!isokendpt(nr_e, &nr)) return EINVAL; /* validate request */ // EINVAL might be undefined
        length = sizeof(struct proc);
        src_vir = (vir_bytes) proc_addr(nr);
        break;
    }
    case GET_PRIV: {
        nr_e = (m_ptr->m_lsys_krn_sys_getinfo.val_len2_e == SELF) ?
            caller->p_endpoint : m_ptr->m_lsys_krn_sys_getinfo.val_len2_e;
        if(!isokendpt(nr_e, &nr)) return EINVAL; /* validate request */ // EINVAL might be undefined
        length = sizeof(struct priv);
        src_vir = (vir_bytes) priv_addr(nr_to_id(nr));
        break;
    }
    case GET_REGS: {
        nr_e = (m_ptr->m_lsys_krn_sys_getinfo.val_len2_e == SELF) ?
            caller->p_endpoint : m_ptr->m_lsys_krn_sys_getinfo.val_len2_e;
        if(!isokendpt(nr_e, &nr)) return EINVAL; /* validate request */ // EINVAL might be undefined
        p = proc_addr(nr);
        length = sizeof(p->p_reg);
        src_vir = (vir_bytes) &p->p_reg;
        break;
    }
    case GET_WHOAMI: {
	int len;
	m_ptr->m_krn_lsys_sys_getwhoami.endpt = caller->p_endpoint;
	len = MIN(sizeof(m_ptr->m_krn_lsys_sys_getwhoami.name),
		sizeof(caller->p_name))-1;
	// MODIFIED strncpy to kstrlcpy with FIXME
	(void)kstrlcpy(m_ptr->m_krn_lsys_sys_getwhoami.name, caller->p_name, len); /* FIXME: strncpy(dst,src,n) replaced. Validate 'n' is buffer size for kstrlcpy, not just copy length. Semantics differ. */
	m_ptr->m_krn_lsys_sys_getwhoami.name[len] = '\0';
	m_ptr->m_krn_lsys_sys_getwhoami.privflags = priv(caller)->s_flags;
        m_ptr->m_krn_lsys_sys_getwhoami.initflags = priv(caller)->s_init_flags;
	return OK;
    }
    case GET_MONPARAMS: {
        src_vir = (vir_bytes) kinfo.param_buf;
	length = sizeof(kinfo.param_buf);
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
	int bin = m_ptr->m_lsys_krn_sys_getinfo.val_len2_e;

	if(bin < 0 || bin >= RANDOM_SOURCES) {
		kprintf_stub("SYSTEM: GET_RANDOMNESS_BIN: %d out of range\n", bin); // MODIFIED
		return EINVAL; // EINVAL might be undefined
	}

	if(krandom.bin[bin].r_size < RANDOM_ELEMENTS)
		return ENOENT; // ENOENT might be undefined

    	length = sizeof(krandom.bin[bin]);
    	src_vir = (vir_bytes) &krandom.bin[bin];

	wipe_rnd_bin = bin;

    	break;
    }
    case GET_IRQACTIDS: {
        length = sizeof(irq_actids);
        src_vir = (vir_bytes) irq_actids;
        break;
    }
    case GET_IDLETSC: {
	struct proc * idl;
	update_idle_time();
	idl = proc_addr(IDLE);
        length = sizeof(idl->p_cycles);
        src_vir = (vir_bytes) &idl->p_cycles;
        break;
    }
    case GET_CPUTICKS: {
	uint64_t ticks[MINIX_CPUSTATES]; // uint64_t might be undefined
	unsigned int cpu;
	cpu = (unsigned int)m_ptr->m_lsys_krn_sys_getinfo.val_len2_e;
	if (cpu >= CONFIG_MAX_CPUS)
		return EINVAL; // EINVAL might be undefined
	get_cpu_ticks(cpu, ticks);
	length = sizeof(ticks);
	src_vir = (vir_bytes)ticks;
	break;
    }
    default:
	kprintf_stub("do_getinfo: invalid request %d\n", // MODIFIED
		m_ptr->m_lsys_krn_sys_getinfo.request);
        return(EINVAL); // EINVAL might be undefined
  }

  /* Try to make the actual copy for the requested data. */
  if (m_ptr->m_lsys_krn_sys_getinfo.val_len > 0 &&
	length > (k_size_t)m_ptr->m_lsys_krn_sys_getinfo.val_len) // MODIFIED size_t cast
	return (E2BIG); // E2BIG might be undefined

  r = data_copy_vmcheck(caller, KERNEL, src_vir, caller->p_endpoint,
	m_ptr->m_lsys_krn_sys_getinfo.val_ptr, length);

  if(r != OK) return r;

	if(wipe_rnd_bin >= 0 && wipe_rnd_bin < RANDOM_SOURCES) {
		krandom.bin[wipe_rnd_bin].r_size = 0;
		krandom.bin[wipe_rnd_bin].r_next = 0;
	}

  return(OK);
}

#endif /* USE_GETINFO */
