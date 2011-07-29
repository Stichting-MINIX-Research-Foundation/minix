/* This file contains the main program of MINIX as well as its shutdown code.
 * The routine main() initializes the system and starts the ball rolling by
 * setting up the process table, interrupt vectors, and scheduling each task 
 * to run to initialize itself.
 * The routine shutdown() does the opposite and brings down MINIX. 
 *
 * The entries into this file are:
 *   main:	    	MINIX main program
 *   prepare_shutdown:	prepare to take MINIX down
 */
#include "kernel.h"
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <a.out.h>
#include <minix/com.h>
#include <minix/endpoint.h>
#include <minix/u64.h>
#include "proc.h"
#include "debug.h"
#include "clock.h"
#include "hw_intr.h"
#include "arch_proto.h"

#ifdef CONFIG_SMP
#include "smp.h"
#endif
#ifdef USE_WATCHDOG
#include "watchdog.h"
#endif
#include "spinlock.h"

/* dummy for linking */
char *** _penviron;

/* Prototype declarations for PRIVATE functions. */
FORWARD _PROTOTYPE( void announce, (void));	

PUBLIC void bsp_finish_booting(void)
{
  int i;
#if SPROFILE
  sprofiling = 0;      /* we're not profiling until instructed to */
#endif /* SPROFILE */
  cprof_procs_no = 0;  /* init nr of hash table slots used */

  cpu_identify();

  vm_running = 0;
  krandom.random_sources = RANDOM_SOURCES;
  krandom.random_elements = RANDOM_ELEMENTS;

  /* MINIX is now ready. All boot image processes are on the ready queue.
   * Return to the assembly code to start running the current process. 
   */
  
  /* it should point somewhere */
  get_cpulocal_var(bill_ptr) = get_cpulocal_var_ptr(idle_proc);
  get_cpulocal_var(proc_ptr) = get_cpulocal_var_ptr(idle_proc);
  announce();				/* print MINIX startup banner */

  /*
   * we have access to the cpu local run queue, only now schedule the processes.
   * We ignore the slots for the former kernel tasks
   */
  for (i=0; i < NR_BOOT_PROCS - NR_TASKS; i++) {
	RTS_UNSET(proc_addr(i), RTS_PROC_STOP);
  }
  /*
   * enable timer interrupts and clock task on the boot CPU
   */
  if (boot_cpu_init_timer(system_hz)) {
	  panic("FATAL : failed to initialize timer interrupts, "
			  "cannot continue without any clock source!");
  }

  fpu_init();

/* Warnings for sanity checks that take time. These warnings are printed
 * so it's a clear warning no full release should be done with them
 * enabled.
 */
#if DEBUG_SCHED_CHECK
  FIXME("DEBUG_SCHED_CHECK enabled");
#endif
#if DEBUG_VMASSERT
  FIXME("DEBUG_VMASSERT enabled");
#endif
#if DEBUG_PROC_CHECK
  FIXME("PROC check enabled");
#endif

  DEBUGEXTRA(("cycles_accounting_init()... "));
  cycles_accounting_init();
  DEBUGEXTRA(("done\n"));

#ifdef CONFIG_SMP
  cpu_set_flag(bsp_cpu_id, CPU_IS_READY);
  machine.processors_count = ncpus;
  machine.bsp_id = bsp_cpu_id;
#else
  machine.processors_count = 1;
  machine.bsp_id = 0;
#endif
  
  switch_to_user();
  NOT_REACHABLE;
}

/*===========================================================================*
 *				main                                         *
 *===========================================================================*/
PUBLIC int main(void)
{
/* Start the ball rolling. */
  struct boot_image *ip;	/* boot image pointer */
  register struct proc *rp;	/* process pointer */
  register int i, j;
  size_t argsz;			/* size of arguments passed to crtso on stack */
#if !defined(__ELF__)
  vir_clicks text_clicks, data_clicks, st_clicks;
  phys_clicks text_base;
  int hdrindex;			/* index to array of a.out headers */
  struct exec e_hdr;		/* for a copy of an a.out header */
#endif

  BKL_LOCK();
   /* Global value to test segment sanity. */
   magictest = MAGICTEST;
 
   DEBUGEXTRA(("main()\n"));

   proc_init();

  /* Set up proc table entries for processes in boot image.  The stacks
   * of the servers have been added to the data segment by the monitor, so
   * the stack pointer is set to the end of the data segment.
   */

  for (i=0; i < NR_BOOT_PROCS; ++i) {
	int schedulable_proc;
	proc_nr_t proc_nr;
	int ipc_to_m, kcalls;
	sys_map_t map;

	ip = &image[i];				/* process' attributes */
	DEBUGEXTRA(("initializing %s... ", ip->proc_name));
	rp = proc_addr(ip->proc_nr);		/* get process pointer */
	ip->endpoint = rp->p_endpoint;		/* ipc endpoint */
	make_zero64(rp->p_cpu_time_left);
	strncpy(rp->p_name, ip->proc_name, P_NAME_LEN); /* set process name */
	
	reset_proc_accounting(rp);

	/* See if this process is immediately schedulable.
	 * In that case, set its privileges now and allow it to run.
	 * Only kernel tasks and the root system process get to run immediately.
	 * All the other system processes are inhibited from running by the
	 * RTS_NO_PRIV flag. They can only be scheduled once the root system
	 * process has set their privileges.
	 */
	proc_nr = proc_nr(rp);
	schedulable_proc = (iskerneln(proc_nr) || isrootsysn(proc_nr));
	if(schedulable_proc) {
	    /* Assign privilege structure. Force a static privilege id. */
            (void) get_priv(rp, static_priv_id(proc_nr));

            /* Priviliges for kernel tasks. */
            if(iskerneln(proc_nr)) {
                /* Privilege flags. */
                priv(rp)->s_flags = (proc_nr == IDLE ? IDL_F : TSK_F);
                /* Allowed traps. */
                priv(rp)->s_trap_mask = (proc_nr == CLOCK 
                    || proc_nr == SYSTEM  ? CSK_T : TSK_T);
                ipc_to_m = TSK_M;                  /* allowed targets */
                kcalls = TSK_KC;                   /* allowed kernel calls */
            }
            /* Priviliges for the root system process. */
            else if(isrootsysn(proc_nr)) {
                priv(rp)->s_flags= RSYS_F;        /* privilege flags */
                priv(rp)->s_trap_mask= SRV_T;     /* allowed traps */
                ipc_to_m = SRV_M;                 /* allowed targets */
                kcalls = SRV_KC;                  /* allowed kernel calls */
                priv(rp)->s_sig_mgr = SRV_SM;     /* signal manager */
                rp->p_priority = SRV_Q;	          /* priority queue */
                rp->p_quantum_size_ms = SRV_QT;   /* quantum size */
            }
            /* Priviliges for ordinary process. */
            else {
		NOT_REACHABLE;
            }

            /* Fill in target mask. */
            memset(&map, 0, sizeof(map));

            if (ipc_to_m == ALL_M) {
                for(j = 0; j < NR_SYS_PROCS; j++)
                    set_sys_bit(map, j);
            }

            fill_sendto_mask(rp, &map);

            /* Fill in kernel call mask. */
            for(j = 0; j < SYS_CALL_MASK_SIZE; j++) {
                priv(rp)->s_k_call_mask[j] = (kcalls == NO_C ? 0 : (~0));
            }
	}
	else {
	    /* Don't let the process run for now. */
            RTS_SET(rp, RTS_NO_PRIV | RTS_NO_QUANTUM);
	}
#if defined(__ELF__)
	rp->p_memmap[T].mem_vir  = ABS2CLICK(ip->memmap.text_vaddr);
	rp->p_memmap[T].mem_phys = ABS2CLICK(ip->memmap.text_paddr);
	rp->p_memmap[T].mem_len  = ABS2CLICK(ip->memmap.text_bytes);
	rp->p_memmap[D].mem_vir  = ABS2CLICK(ip->memmap.data_vaddr);
	rp->p_memmap[D].mem_phys = ABS2CLICK(ip->memmap.data_paddr);
	rp->p_memmap[D].mem_len  = ABS2CLICK(ip->memmap.data_bytes);
	rp->p_memmap[S].mem_phys = ABS2CLICK(ip->memmap.data_paddr +
					     ip->memmap.data_bytes +
					     ip->memmap.stack_bytes);
	rp->p_memmap[S].mem_vir  = ABS2CLICK(ip->memmap.data_vaddr +
					     ip->memmap.data_bytes +
					     ip->memmap.stack_bytes);
	rp->p_memmap[S].mem_len  = 0;
#else
	if (iskerneln(proc_nr)) {		/* part of the kernel? */ 
		hdrindex = 0;		/* all use the first a.out header */
	} else {
		hdrindex = 1 + i-NR_TASKS;	/* system/user processes */
	}

	/* Architecture-specific way to find out aout header of this
	 * boot process.
	 */
	arch_get_aout_headers(hdrindex, &e_hdr);

	/* Convert addresses to clicks and build process memory map */
	text_base = e_hdr.a_syms >> CLICK_SHIFT;
	text_clicks = (vir_clicks) (CLICK_CEIL(e_hdr.a_text) >> CLICK_SHIFT);
	data_clicks = (vir_clicks) (CLICK_CEIL(e_hdr.a_data
		+ e_hdr.a_bss) >> CLICK_SHIFT);
	st_clicks = (vir_clicks) (CLICK_CEIL(e_hdr.a_total) >> CLICK_SHIFT);
	if (!(e_hdr.a_flags & A_SEP))
	{
		data_clicks = (vir_clicks) (CLICK_CEIL(e_hdr.a_text +
			e_hdr.a_data + e_hdr.a_bss) >> CLICK_SHIFT);
		text_clicks = 0;	   /* common I&D */
	}
	rp->p_memmap[T].mem_phys = text_base;
	rp->p_memmap[T].mem_len  = text_clicks;
	rp->p_memmap[D].mem_phys = text_base + text_clicks;
	rp->p_memmap[D].mem_len  = data_clicks;
	rp->p_memmap[S].mem_phys = text_base + text_clicks + st_clicks;
	rp->p_memmap[S].mem_vir  = st_clicks;
	rp->p_memmap[S].mem_len  = 0;
#endif

	/* Set initial register values.  The processor status word for tasks 
	 * is different from that of other processes because tasks can
	 * access I/O; this is not allowed to less-privileged processes 
	 */
	rp->p_reg.pc = ip->memmap.entry;
	rp->p_reg.psw = (iskerneln(proc_nr)) ? INIT_TASK_PSW : INIT_PSW;

	/* Initialize the server stack pointer. Take it down three words
	 * to give crtso.s something to use as "argc", "argv" and "envp".
	 */
	if (isusern(proc_nr)) {		/* user-space process? */ 
		rp->p_reg.sp = (rp->p_memmap[S].mem_vir +
				rp->p_memmap[S].mem_len) << CLICK_SHIFT;
		argsz = 3 * sizeof(reg_t);
		rp->p_reg.sp -= argsz;
		phys_memset(rp->p_reg.sp - 
			(rp->p_memmap[S].mem_vir << CLICK_SHIFT) +
			(rp->p_memmap[S].mem_phys << CLICK_SHIFT), 
			0, argsz);
	}

	/* scheduling functions depend on proc_ptr pointing somewhere. */
	if(!get_cpulocal_var(proc_ptr))
		get_cpulocal_var(proc_ptr) = rp;

	/* If this process has its own page table, VM will set the
	 * PT up and manage it. VM will signal the kernel when it has
	 * done this; until then, don't let it run.
	 */
	if(ip->flags & PROC_FULLVM)
		rp->p_rts_flags |= RTS_VMINHIBIT;

	rp->p_rts_flags |= RTS_PROC_STOP;
	rp->p_rts_flags &= ~RTS_SLOT_FREE;
	alloc_segments(rp);
	DEBUGEXTRA(("done\n"));
  }

#define IPCNAME(n) { \
	assert((n) >= 0 && (n) <= IPCNO_HIGHEST); \
	assert(!ipc_call_names[n]);	\
	ipc_call_names[n] = #n; \
}

  IPCNAME(SEND);
  IPCNAME(RECEIVE);
  IPCNAME(SENDREC);
  IPCNAME(NOTIFY);
  IPCNAME(SENDNB);
  IPCNAME(SENDA);

  /* Architecture-dependent initialization. */
  DEBUGEXTRA(("arch_init()... "));
  arch_init();
  DEBUGEXTRA(("done\n"));

  /* System and processes initialization */
  DEBUGEXTRA(("system_init()... "));
  system_init();
  DEBUGEXTRA(("done\n"));

#ifdef CONFIG_SMP
  if (config_no_apic) {
	  BOOT_VERBOSE(printf("APIC disabled, disables SMP, using legacy PIC\n"));
	  smp_single_cpu_fallback();
  } else if (config_no_smp) {
	  BOOT_VERBOSE(printf("SMP disabled, using legacy PIC\n"));
	  smp_single_cpu_fallback();
  } else {
	  smp_init();
	  /*
	   * if smp_init() returns it means that it failed and we try to finish
	   * single CPU booting
	   */
	  bsp_finish_booting();
  }
#else
  /* 
   * if configured for a single CPU, we are already on the kernel stack which we
   * are going to use everytime we execute kernel code. We finish booting and we
   * never return here
   */
  bsp_finish_booting();
#endif

  NOT_REACHABLE;
  return 1;
}

/*===========================================================================*
 *				announce				     *
 *===========================================================================*/
PRIVATE void announce(void)
{
  /* Display the MINIX startup banner. */
  printf("\nMINIX %s.%s. "
#ifdef _VCS_REVISION
	"(" _VCS_REVISION ")\n"
#endif
      "Copyright 2010, Vrije Universiteit, Amsterdam, The Netherlands\n",
      OS_RELEASE, OS_VERSION);
  printf("MINIX is open source software, see http://www.minix3.org\n");
}

/*===========================================================================*
 *				prepare_shutdown			     *
 *===========================================================================*/
PUBLIC void prepare_shutdown(const int how)
{
/* This function prepares to shutdown MINIX. */
  static timer_t shutdown_timer;

  /* Continue after 1 second, to give processes a chance to get scheduled to 
   * do shutdown work.  Set a watchog timer to call shutdown(). The timer 
   * argument passes the shutdown status. 
   */
  printf("MINIX will now be shut down ...\n");
  tmr_arg(&shutdown_timer)->ta_int = how;
  set_timer(&shutdown_timer, get_uptime() + system_hz, minix_shutdown);
}

/*===========================================================================*
 *				shutdown 				     *
 *===========================================================================*/
PUBLIC void minix_shutdown(timer_t *tp)
{
/* This function is called from prepare_shutdown or stop_sequence to bring 
 * down MINIX. How to shutdown is in the argument: RBT_HALT (return to the
 * monitor), RBT_MONITOR (execute given code), RBT_RESET (hard reset). 
 */
#ifdef CONFIG_SMP
  /* 
   * FIXME
   *
   * we will need to stop timers on all cpus if SMP is enabled and put them in
   * such a state that we can perform the whole boot process once restarted from
   * monitor again
   */
  if (ncpus > 1)
	  smp_shutdown_aps();
#endif
  hw_intr_disable_all();
  stop_local_timer();
  intr_init(INTS_ORIG, 0);
  arch_shutdown(tp ? tmr_arg(tp)->ta_int : RBT_PANIC);
}

