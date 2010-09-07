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

/* Prototype declarations for PRIVATE functions. */
FORWARD _PROTOTYPE( void announce, (void));	

/*===========================================================================*
 *				main                                         *
 *===========================================================================*/
PUBLIC int main(void)
{
/* Start the ball rolling. */
  struct boot_image *ip;	/* boot image pointer */
  register struct proc *rp;	/* process pointer */
  register struct priv *sp;	/* privilege structure pointer */
  register int i, j;
  int hdrindex;			/* index to array of a.out headers */
  phys_clicks text_base;
  vir_clicks text_clicks, data_clicks, st_clicks;
  reg_t ktsb;			/* kernel task stack base */
  struct exec e_hdr;		/* for a copy of an a.out header */
  size_t argsz;			/* size of arguments passed to crtso on stack */

   /* Global value to test segment sanity. */
   magictest = MAGICTEST;
 
   DEBUGEXTRA(("main()\n"));

  /* Clear the process table. Anounce each slot as empty and set up mappings 
   * for proc_addr() and proc_nr() macros. Do the same for the table with 
   * privilege structures for the system processes. 
   */
  for (rp = BEG_PROC_ADDR, i = -NR_TASKS; rp < END_PROC_ADDR; ++rp, ++i) {
  	rp->p_rts_flags = RTS_SLOT_FREE;		/* initialize free slot */
	rp->p_magic = PMAGIC;
	rp->p_nr = i;				/* proc number from ptr */
	rp->p_endpoint = _ENDPOINT(0, rp->p_nr); /* generation no. 0 */
	rp->p_scheduler = NULL;			/* no user space scheduler */
	rp->p_priority = 0;		        /* no priority */
	rp->p_quantum_size_ms = 0;	        /* no quantum size */
  }
  for (sp = BEG_PRIV_ADDR, i = 0; sp < END_PRIV_ADDR; ++sp, ++i) {
	sp->s_proc_nr = NONE;			/* initialize as free */
	sp->s_id = (sys_id_t) i;		/* priv structure index */
	ppriv_addr[i] = sp;			/* priv ptr from number */
	sp->s_sig_mgr = NONE;			/* clear signal managers */
	sp->s_bak_sig_mgr = NONE;
  }

  /* Set up proc table entries for processes in boot image.  The stacks of the
   * kernel tasks are initialized to an array in data space.  The stacks
   * of the servers have been added to the data segment by the monitor, so
   * the stack pointer is set to the end of the data segment.  All the
   * processes are in low memory on the 8086.  On the 386 only the kernel
   * is in low memory, the rest is loaded in extended memory.
   */

  /* Task stacks. */
  ktsb = (reg_t) t_stack;

  for (i=0; i < NR_BOOT_PROCS; ++i) {
	int schedulable_proc;
	proc_nr_t proc_nr;
	int ipc_to_m, kcalls;

	ip = &image[i];				/* process' attributes */
	DEBUGEXTRA(("initializing %s... ", ip->proc_name));
	rp = proc_addr(ip->proc_nr);		/* get process pointer */
	ip->endpoint = rp->p_endpoint;		/* ipc endpoint */
	make_zero64(rp->p_cpu_time_left);
	strncpy(rp->p_name, ip->proc_name, P_NAME_LEN); /* set process name */

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
            fill_sendto_mask(rp, ipc_to_m);

            /* Fill in kernel call mask. */
            for(j = 0; j < SYS_CALL_MASK_SIZE; j++) {
                priv(rp)->s_k_call_mask[j] = (kcalls == NO_C ? 0 : (~0));
            }
	}
	else {
	    /* Don't let the process run for now. */
            RTS_SET(rp, RTS_NO_PRIV | RTS_NO_QUANTUM);
	}

	if (iskerneln(proc_nr)) {		/* part of the kernel? */ 
		if (ip->stksize > 0) {		/* HARDWARE stack size is 0 */
			rp->p_priv->s_stack_guard = (reg_t *) ktsb;
			*rp->p_priv->s_stack_guard = STACK_GUARD;
		}
		ktsb += ip->stksize;	/* point to high end of stack */
		rp->p_reg.sp = ktsb;	/* this task's initial stack ptr */
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

	/* Set initial register values.  The processor status word for tasks 
	 * is different from that of other processes because tasks can
	 * access I/O; this is not allowed to less-privileged processes 
	 */
	rp->p_reg.pc = 0; /* we cannot start anything else */
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
	if(!proc_ptr) proc_ptr = rp;

	/* If this process has its own page table, VM will set the
	 * PT up and manage it. VM will signal the kernel when it has
	 * done this; until then, don't let it run.
	 */
	if(ip->flags & PROC_FULLVM)
		RTS_SET(rp, RTS_VMINHIBIT);

	/* None of the kernel tasks run */
	if (rp->p_nr < 0) RTS_SET(rp, RTS_PROC_STOP);
	RTS_UNSET(rp, RTS_SLOT_FREE); /* remove RTS_SLOT_FREE and schedule */
	alloc_segments(rp);
	DEBUGEXTRA(("done\n"));
  }

  /* Architecture-dependent initialization. */
  DEBUGEXTRA(("arch_init()... "));
  arch_init();
  DEBUGEXTRA(("done\n"));

  /* System and processes initialization */
  DEBUGEXTRA(("system_init()... "));
  system_init();
  DEBUGEXTRA(("done\n"));

#if SPROFILE
  sprofiling = 0;      /* we're not profiling until instructed to */
#endif /* SPROFILE */
  cprof_procs_no = 0;  /* init nr of hash table slots used */

  vm_running = 0;
  krandom.random_sources = RANDOM_SOURCES;
  krandom.random_elements = RANDOM_ELEMENTS;

  /* MINIX is now ready. All boot image processes are on the ready queue.
   * Return to the assembly code to start running the current process. 
   */
  bill_ptr = proc_addr(IDLE);	/* it has to point somewhere */
  announce();				/* print MINIX startup banner */

  /*
   * enable timer interrupts and clock task on the boot CPU
   */

  if (boot_cpu_init_timer(system_hz)) {
	  panic( "FATAL : failed to initialize timer interrupts; "
		"cannot continue without any clock source!");
  }

/* Warnings for sanity checks that take time. These warnings are printed
 * so it's a clear warning no full release should be done with them
 * enabled.
 */
#if DEBUG_PROC_CHECK
  FIXME("PROC check enabled");
#endif

  DEBUGEXTRA(("cycles_accounting_init()... "));
  cycles_accounting_init();
  DEBUGEXTRA(("done\n"));

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

  assert(runqueues_ok());

  switch_to_user();
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
#ifdef _SVN_REVISION
	"(" _SVN_REVISION ")\n"
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
  arch_stop_local_timer();
  hw_intr_disable_all();
  intr_init(INTS_ORIG, 0);
  arch_shutdown(tp ? tmr_arg(tp)->ta_int : RBT_PANIC);
}

