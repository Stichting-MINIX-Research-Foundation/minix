/* This file contains the main program of MINIX as well as its shutdown code.
 * The routine main() initializes the system and starts the ball rolling by
 * setting up the process table, interrupt vectors, and scheduling each task 
 * to run to initialize itself.
 * The routine shutdown() does the opposite and brings down MINIX. 
 *
 * The entries into this file are:
 *   main:	    	MINIX main program
 *   prepare_shutdown:	prepare to take MINIX down
 *
 * Changes:
 *   Nov 24, 2004   simplified main() with system image  (Jorrit N. Herder)
 *   Aug 20, 2004   new prepare_shutdown() and shutdown()  (Jorrit N. Herder)
 */
#include "kernel.h"
#include <signal.h>
#include <unistd.h>
#include <a.out.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include "proc.h"
#include "sendmask.h"

/* Prototype declarations for PRIVATE functions. */
FORWARD _PROTOTYPE( void announce, (void));	
FORWARD _PROTOTYPE( void shutdown, (timer_t *tp));


/*===========================================================================*
 *                                   main                                    *
 *===========================================================================*/
PUBLIC void main()
{
/* Start the ball rolling. */
  register struct proc *rp;
  register int i;
  int hdrindex;			/* index to array of a.out headers */
  phys_clicks text_base;
  vir_clicks text_clicks;
  vir_clicks data_clicks;
  reg_t ktsb;			/* kernel task stack base */
  struct system_image *ip;	/* boot image pointer */
  struct exec e_hdr;		/* for a copy of an a.out header */

  /* Initialize the interrupt controller. */
  intr_init(1);

  /* Clear the process table. Anounce each slot as empty and set up mappings 
   * for proc_addr() and proc_nr() macros.
   */
  for (rp = BEG_PROC_ADDR, i = -NR_TASKS; rp < END_PROC_ADDR; ++rp, ++i) {
  	rp->p_rts_flags = SLOT_FREE;		/* initialize free slot */
	rp->p_nr = i;				/* proc number from ptr */
        (pproc_addr + NR_TASKS)[i] = rp;        /* proc ptr from number */
  }

  /* Set up proc table entries for tasks and servers.  The stacks of the
   * kernel tasks are initialized to an array in data space.  The stacks
   * of the servers have been added to the data segment by the monitor, so
   * the stack pointer is set to the end of the data segment.  All the
   * processes are in low memory on the 8086.  On the 386 only the kernel
   * is in low memory, the rest is loaded in extended memory.
   */

  /* Task stacks. */
  ktsb = (reg_t) t_stack;

  for (i=0; i < IMAGE_SIZE; ++i) {
	ip = &image[i];				/* t's task attributes */
	rp = proc_addr(ip->proc_nr);		/* t's process slot */
	kstrncpy(rp->p_name, ip->proc_name, P_NAME_LEN);  /* set name */
	rp->p_name[P_NAME_LEN-1] = '\0';	/* just for safety */
	rp->p_flags = ip->flags;		/* process flags */
	rp->p_max_priority = ip->priority;	/* max scheduling priority */
	rp->p_priority = ip->priority;		/* current priority */
	rp->p_quantum_size = ip->quantum;	/* quantum size in ticks */
	rp->p_sched_ticks = ip->quantum;	/* current credit */
	rp->p_full_quantums = QUANTUMS(ip->priority);  /* quantums left */
	rp->p_call_mask = ip->call_mask;	/* allowed system calls */
	rp->p_sendmask = ip->sendmask;		/* sendmask protection */
	if (i-NR_TASKS < 0) {			/* part of the kernel? */ 
		if (ip->stksize > 0) {		/* HARDWARE stack size is 0 */
			rp->p_stguard = (reg_t *) ktsb;
			*rp->p_stguard = STACK_GUARD;
		}
		ktsb += ip->stksize;	/* point to high end of stack */
		rp->p_reg.sp = ktsb;	/* this task's initial stack ptr */
		text_base = kinfo.code_base >> CLICK_SHIFT;
					/* processes that are in the kernel */
		hdrindex = 0;		/* all use the first a.out header */
	} else {
		hdrindex = 1 + i-NR_TASKS;	/* drivers, servers, INIT follow */
	}

	/* The bootstrap loader created an array of the a.out headers at
	 * absolute address 'aout'. Get one element to e_hdr.
	 */
	phys_copy(aout + hdrindex * A_MINHDR, vir2phys(&e_hdr),
							(phys_bytes) A_MINHDR);
	/* Convert addresses to clicks and build process memory map */
	text_base = e_hdr.a_syms >> CLICK_SHIFT;
	text_clicks = (e_hdr.a_text + CLICK_SIZE-1) >> CLICK_SHIFT;
	if (!(e_hdr.a_flags & A_SEP)) text_clicks = 0;	/* Common I&D */
	data_clicks = (e_hdr.a_total + CLICK_SIZE-1) >> CLICK_SHIFT;
	rp->p_memmap[T].mem_phys = text_base;
	rp->p_memmap[T].mem_len  = text_clicks;
	rp->p_memmap[D].mem_phys = text_base + text_clicks;
	rp->p_memmap[D].mem_len  = data_clicks;
	rp->p_memmap[S].mem_phys = text_base + text_clicks + data_clicks;
	rp->p_memmap[S].mem_vir  = data_clicks;	/* empty - stack is in data */

	/* Set initial register values.  The processor status word for tasks 
	 * is different from that of other processes because tasks can
	 * access I/O; this is not allowed to less-privileged processes 
	 */
	rp->p_reg.pc = (reg_t) ip->initial_pc;
	rp->p_reg.psw = (iskernelp(rp)) ? INIT_TASK_PSW : INIT_PSW;

	/* Initialize the server stack pointer. Take it down one word
	 * to give crtso.s something to use as "argc".
	 */
	if (i-NR_TASKS >= 0) {
		rp->p_reg.sp = (rp->p_memmap[S].mem_vir +
				rp->p_memmap[S].mem_len) << CLICK_SHIFT;
		rp->p_reg.sp -= sizeof(reg_t);
	}
	
	/* Set ready. The HARDWARE task is never ready. */
#if ENABLE_K_DEBUGGING
	rp->p_ready = 0;
#endif
	if (rp->p_nr != HARDWARE) lock_ready(rp);	
	rp->p_rts_flags = 0;

	/* Code and data segments must be allocated in protected mode. */
	alloc_segments(rp);
  }

#if ENABLE_BOOTDEV
  /* Expect an image of the boot device to be loaded into memory as well. 
   * The boot device is the last module that is loaded into memory, and, 
   * for example, can contain the root FS (useful for embedded systems). 
   */
  hdrindex ++;
  phys_copy(aout + hdrindex * A_MINHDR,vir2phys(&e_hdr),(phys_bytes) A_MINHDR);
  if (e_hdr.a_flags & A_IMG) {
  	kinfo.bootdev_base = e_hdr.a_syms; 
  	kinfo.bootdev_size = e_hdr.a_data; 
  }
#endif

  /* MINIX is now ready. All boot image processes are on the ready queue.
   * Return to the assembly code to start running the current process. 
   */
  bill_ptr = proc_addr(IDLE);		/* it has to point somewhere */
  announce();				/* print MINIX startup banner */
  restart();
}



/*==========================================================================*
 *				announce				    *
 *==========================================================================*/
PRIVATE void announce(void)
{
  /* Display the MINIX startup banner. */
  kprintf("MINIX %s.  Copyright 2001 Prentice-Hall, Inc.\n", 
      karg(OS_RELEASE "." OS_VERSION));

#if (CHIP == INTEL)
  /* Real mode, or 16/32-bit protected mode? */
  kprintf("Executing in %s mode\n\n",
      machine.protected ? karg("32-bit protected") : karg("real"));
#endif
}


/*==========================================================================*
 *			       prepare_shutdown				    *
 *==========================================================================*/
PUBLIC void prepare_shutdown(how)
int how;				/* reason to shut down */
{
/* This function prepares to shutdown MINIX. It uses a global flag to make 
 * sure it is only executed once. Unless a CPU exception occurred, the 
 */
  static timer_t shutdown_timer; 	/* timer for watchdog function */ 
  message m;

  /* Show debugging dumps on panics. Make sure that the TTY task is still 
   * available to handle them. This is done with help of a non-blocking send. 
   * We rely on TTY to call sys_abort() when it is done with the dumps.
   */
  if (how == RBT_PANIC) {
      m.m_type = PANIC_DUMPS;
      if (nb_send(TTY, &m) == OK)	/* don't block if TTY isn't ready */
          return;			/* await sys_abort() from TTY */
  }

  /* The TTY expects two HARD_STOP notifications. One to switch to the 
   * primary console for stop sequence output, and one to actually exit.
   */
  m.NOTIFY_TYPE = HARD_STOP;
  lock_notify(TTY, &m);

  /* Allow processes to be scheduled to clean up, unless a CPU exception 
   * occurred. This is done by setting a timer. The timer argument passes
   * the shutdown status.
   */
  tmr_arg(&shutdown_timer)->ta_int = how;	/* pass how in timer */
  if (kernel_exception) {			/* set in exception() */
      kprintf("\nAn exception occured; skipping stop sequence.\n", NO_NUM);
      shutdown(&shutdown_timer);		/* TTY isn't scheduled */
  } else {
      kprintf("\nNotifying system services about MINIX shutdown.\n", NO_NUM); 
      set_timer(&shutdown_timer, get_uptime(), shutdown);
  }
}


/*==========================================================================*
 *				   shutdown 				    *
 *==========================================================================*/
PRIVATE void shutdown(tp)
timer_t *tp;
{
/* This function is called from prepare_shutdown or stop_sequence to bring 
 * down MINIX. How to shutdown is in the argument: RBT_HALT (return to the
 * monitor), RBT_MONITOR (execute given code), RBT_RESET (hard reset). 
 */
  int how = tmr_arg(tp)->ta_int;
  u16_t magic; 

  /* Now mask all interrupts, including the clock, and stop the clock. */
  outb(INT_CTLMASK, ~0); 
  clock_stop();

  if (mon_return && how != RBT_RESET) {
	/* Reinitialize the interrupt controllers to the BIOS defaults. */
	intr_init(0);
	outb(INT_CTLMASK, 0);
	outb(INT2_CTLMASK, 0);

	/* Return to the boot monitor. Set the program if not already done. */
	if (how != RBT_MONITOR) phys_copy(vir2phys(""), kinfo.params_base, 1); 
	level0(monitor);
  }

  /* Reset the system by jumping to the reset address (real mode), or by
   * forcing a processor shutdown (protected mode). First stop the BIOS 
   * memory test by setting a soft reset flag. 
   */
  magic = STOP_MEM_CHECK;
  phys_copy(vir2phys(&magic), SOFT_RESET_FLAG_ADDR, SOFT_RESET_FLAG_SIZE);
  level0(reset);
}

