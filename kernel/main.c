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
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <a.out.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/endpoint.h>
#include "proc.h"

/* Prototype declarations for PRIVATE functions. */
FORWARD _PROTOTYPE( void announce, (void));	

/*===========================================================================*
 *				main                                         *
 *===========================================================================*/
PUBLIC void main()
{
/* Start the ball rolling. */
  struct boot_image *ip;	/* boot image pointer */
  register struct proc *rp;	/* process pointer */
  register struct priv *sp;	/* privilege structure pointer */
  register int i, j, s;
  int hdrindex;			/* index to array of a.out headers */
  phys_clicks text_base;
  vir_clicks text_clicks, data_clicks, st_clicks;
  reg_t ktsb;			/* kernel task stack base */
  struct exec e_hdr;		/* for a copy of an a.out header */

  /* Clear the process table. Anounce each slot as empty and set up mappings 
   * for proc_addr() and proc_nr() macros. Do the same for the table with 
   * privilege structures for the system processes. 
   */
  for (rp = BEG_PROC_ADDR, i = -NR_TASKS; rp < END_PROC_ADDR; ++rp, ++i) {
  	rp->p_rts_flags = SLOT_FREE;		/* initialize free slot */
	rp->p_nr = i;				/* proc number from ptr */
	rp->p_endpoint = _ENDPOINT(0, rp->p_nr); /* generation no. 0 */
        (pproc_addr + NR_TASKS)[i] = rp;        /* proc ptr from number */
  }
  for (sp = BEG_PRIV_ADDR, i = 0; sp < END_PRIV_ADDR; ++sp, ++i) {
	sp->s_proc_nr = NONE;			/* initialize as free */
	sp->s_id = i;				/* priv structure index */
	ppriv_addr[i] = sp;			/* priv ptr from number */
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
	int ci;
	bitchunk_t fv;
	ip = &image[i];				/* process' attributes */
	rp = proc_addr(ip->proc_nr);		/* get process pointer */
	ip->endpoint = rp->p_endpoint;		/* ipc endpoint */
	rp->p_max_priority = ip->priority;	/* max scheduling priority */
	rp->p_priority = ip->priority;		/* current priority */
	rp->p_quantum_size = ip->quantum;	/* quantum size in ticks */
	rp->p_ticks_left = ip->quantum;		/* current credit */
	strncpy(rp->p_name, ip->proc_name, P_NAME_LEN); /* set process name */
	(void) get_priv(rp, (ip->flags & SYS_PROC));    /* assign structure */
	priv(rp)->s_flags = ip->flags;			/* process flags */
	priv(rp)->s_trap_mask = ip->trap_mask;		/* allowed traps */

	/* Initialize call mask bitmap from unordered set.
	 * A single SYS_ALL_CALLS is a special case - it
	 * means all calls are allowed.
	 */
	if(ip->nr_k_calls == 1 && ip->k_calls[0] == SYS_ALL_CALLS)
		fv = ~0;		/* fill call mask */
	else
		fv = 0;			/* clear call mask */

	for(ci = 0; ci < CALL_MASK_SIZE; ci++) 	/* fill or clear call mask */
		priv(rp)->s_k_call_mask[ci] = fv;
	if(!fv)			/* not all full? enter calls bit by bit */
		for(ci = 0; ci < ip->nr_k_calls; ci++)
			SET_BIT(priv(rp)->s_k_call_mask,
				ip->k_calls[ci]-KERNEL_CALL);

	priv(rp)->s_ipc_to.chunk[0] = ip->ipc_to;	/* restrict targets */

	for (j=0; j<BITMAP_CHUNKS(NR_SYS_PROCS); j++) {
		rp->p_priv->s_ipc_sendrec.chunk[j] = ~0L;
	}
	unset_sys_bit(rp->p_priv->s_ipc_sendrec, USER_PRIV_ID);

	if (iskerneln(proc_nr(rp))) {		/* part of the kernel? */ 
		if (ip->stksize > 0) {		/* HARDWARE stack size is 0 */
			rp->p_priv->s_stack_guard = (reg_t *) ktsb;
			*rp->p_priv->s_stack_guard = STACK_GUARD;
		}
		ktsb += ip->stksize;	/* point to high end of stack */
		rp->p_reg.sp = ktsb;	/* this task's initial stack ptr */
		hdrindex = 0;		/* all use the first a.out header */
	} else {
		hdrindex = 1 + i-NR_TASKS;	/* servers, drivers, INIT */
	}

	/* Architecture-specific way to find out aout header of this
	 * boot process.
	 */
	arch_get_aout_headers(hdrindex, &e_hdr);

	/* Convert addresses to clicks and build process memory map */
	text_base = e_hdr.a_syms >> CLICK_SHIFT;
	text_clicks = (e_hdr.a_text + CLICK_SIZE-1) >> CLICK_SHIFT;
	data_clicks = (e_hdr.a_data+e_hdr.a_bss + CLICK_SIZE-1) >> CLICK_SHIFT;
	st_clicks= (e_hdr.a_total + CLICK_SIZE-1) >> CLICK_SHIFT;
	if (!(e_hdr.a_flags & A_SEP))
	{
		data_clicks= (e_hdr.a_text+e_hdr.a_data+e_hdr.a_bss +
			CLICK_SIZE-1) >> CLICK_SHIFT;
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
	rp->p_reg.pc = (reg_t) ip->initial_pc;
	rp->p_reg.psw = (iskernelp(rp)) ? INIT_TASK_PSW : INIT_PSW;

	/* Initialize the server stack pointer. Take it down one word
	 * to give crtso.s something to use as "argc".
	 */
	if (isusern(proc_nr(rp))) {		/* user-space process? */ 
		rp->p_reg.sp = (rp->p_memmap[S].mem_vir +
				rp->p_memmap[S].mem_len) << CLICK_SHIFT;
		rp->p_reg.sp -= sizeof(reg_t);
	}
	
	/* Set ready. The HARDWARE task is never ready. */
	if (rp->p_nr == HARDWARE) RTS_SET(rp, NO_PRIORITY);
	RTS_UNSET(rp, SLOT_FREE); /* remove SLOT_FREE and schedule */

	/* Code and data segments must be allocated in protected mode. */
	alloc_segments(rp);
  }

#if SPROFILE
  sprofiling = 0;      /* we're not profiling until instructed to */
#endif /* SPROFILE */
#if CPROFILE
  cprof_procs_no = 0;  /* init nr of hash table slots used */
#endif /* CPROFILE */

  vm_running = 0;

  /* MINIX is now ready. All boot image processes are on the ready queue.
   * Return to the assembly code to start running the current process. 
   */
  bill_ptr = proc_addr(IDLE);		/* it has to point somewhere */
  announce();				/* print MINIX startup banner */
  restart();
}

/*===========================================================================*
 *				announce				     *
 *===========================================================================*/
PRIVATE void announce(void)
{
  /* Display the MINIX startup banner. */
  kprintf("\nMINIX %s.%s. "
#ifdef _SVN_REVISION
	"(" _SVN_REVISION ")\n"
#endif
      "Copyright 2006, Vrije Universiteit, Amsterdam, The Netherlands\n",
      OS_RELEASE, OS_VERSION);
}

/*===========================================================================*
 *				prepare_shutdown			     *
 *===========================================================================*/
PUBLIC void prepare_shutdown(how)
int how;
{
/* This function prepares to shutdown MINIX. */
  static timer_t shutdown_timer;
  register struct proc *rp; 
  message m;

  /* Continue after 1 second, to give processes a chance to get scheduled to 
   * do shutdown work.  Set a watchog timer to call shutdown(). The timer 
   * argument passes the shutdown status. 
   */
  kprintf("MINIX will now be shut down ...\n");
  tmr_arg(&shutdown_timer)->ta_int = how;
  set_timer(&shutdown_timer, get_uptime() + 5*HZ, minix_shutdown);
}

/*===========================================================================*
 *				shutdown 				     *
 *===========================================================================*/
PUBLIC void minix_shutdown(tp)
timer_t *tp;
{
/* This function is called from prepare_shutdown or stop_sequence to bring 
 * down MINIX. How to shutdown is in the argument: RBT_HALT (return to the
 * monitor), RBT_MONITOR (execute given code), RBT_RESET (hard reset). 
 */
  intr_init(INTS_ORIG);
  clock_stop();
  arch_shutdown(tp ? tmr_arg(tp)->ta_int : RBT_PANIC);
}

