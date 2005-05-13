/* This file contains the main program of MINIX as well as its shutdown code.
 * The routine main() initializes the system and starts the ball rolling by
 * setting up the process table, interrupt vectors, and scheduling each task 
 * to run to initialize itself.
 * The routine prepare_shutdown() tries to cleanly shuts down MINIX by running
 * the stop_sequence() to notify all system services and allowing them some 
 * time to finalize. In case of an exception(), the stop sequence is skipped. 
 *
 * The entries into this file are:
 *   main:	    	MINIX main program
 *   prepare_shutdown:	prepare to take MINIX down
 *   stop_sequence: 	take down all system services
 *
 * Changes:
 *   Nov 24, 2004   simplified main() with system image  (Jorrit N. Herder)
 *   Oct 21, 2004   moved copyright to announce()  (Jorrit N. Herder) 
 *   Sep 04, 2004   created stop_sequence() to cleanup  (Jorrit N. Herder)
 *   Aug 20, 2004   split wreboot() and shutdown()  (Jorrit N. Herder)
 *   Jun 15, 2004   moved wreboot() to this file  (Jorrit N. Herder)
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
FORWARD _PROTOTYPE( void shutdown, (int how));

#define STOP_TICKS	(5*HZ)			/* time allowed to stop */

/*===========================================================================*
 *                                   main                                    *
 *===========================================================================*/
PUBLIC void main()
{
/* Start the ball rolling. */

  register struct proc *rp;
  register int i;
  int hdrindex;			/* index to array of a.out headers */
  phys_clicks text_base, bootdev_base;
  vir_clicks text_clicks, bootdev_clicks;
  vir_clicks data_clicks;
  reg_t ktsb;			/* kernel task stack base */
  struct memory *memp;
  struct system_image *ttp;
  struct exec e_hdr;		/* for a copy of an a.out header */

  /* Initialize the interrupt controller. */
  intr_init(1);

  /* Clear the process table. Anounce each slot as empty and
   * set up mappings for proc_addr() and proc_number() macros.
   */
  for (rp = BEG_PROC_ADDR, i = -NR_TASKS; rp < END_PROC_ADDR; ++rp, ++i) {
  	rp->p_type = P_NONE;			/* isemptyp() tests on this */
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
	ttp = &image[i];			/* t's task attributes */
	rp = proc_addr(ttp->proc_nr);		/* t's process slot */
	kstrncpy(rp->p_name, ttp->proc_name, PROC_NAME_LEN);	 /* set name */
	rp->p_type = ttp->type;			/* type of process */
	rp->p_priority = ttp->priority;		/* scheduling priority */
	rp->p_sendmask = ttp->sendmask;		/* sendmask protection */
	if (i-NR_TASKS < 0) {			/* part of the kernel? */ 
		if (ttp->stksize > 0) {		/* HARDWARE stack size is 0 */
			rp->p_stguard = (reg_t *) ktsb;
			*rp->p_stguard = STACK_GUARD;
		}
		ktsb += ttp->stksize;	/* point to high end of stack */
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

	/* Remove server memory from the free memory list. The boot monitor
	 * promises to put processes at the start of memory chunks. The 
	 * tasks all use same base address, so only the first task changes
	 * the memory lists. The servers and init have their own memory
	 * spaces and their memory will be removed from the list. 
	 */
	for (memp = mem; memp < &mem[NR_MEMS]; memp++) {
		if (memp->base == text_base) {
			memp->base += text_clicks + data_clicks;
			memp->size -= text_clicks + data_clicks;
		}
	}

	/* Set initial register values.  The processor status word for tasks 
	 * is different from that of other processes because tasks can
	 * access I/O; this is not allowed to less-privileged processes 
	 */
	rp->p_reg.pc = (reg_t) ttp->initial_pc;
	rp->p_reg.psw = (isidlep(rp)||istaskp(rp)) ? INIT_TASK_PSW : INIT_PSW;

	/* Initialize the server stack pointer. Take it down one word
	 * to give crtso.s something to use as "argc".
	 */
	if (i-NR_TASKS >= 0) {
		rp->p_reg.sp = (rp->p_memmap[S].mem_vir +
				rp->p_memmap[S].mem_len) << CLICK_SHIFT;
		rp->p_reg.sp -= sizeof(reg_t);
	}
	
	/* Set ready. The HARDWARE task is never ready. */
	if (rp->p_nr != HARDWARE) lock_ready(rp);	
	rp->p_flags = 0;

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

  	/* Remove from free list, to prevent being overwritten. */
	bootdev_base = e_hdr.a_syms >> CLICK_SHIFT;
	bootdev_clicks = (e_hdr.a_total + CLICK_SIZE-1) >> CLICK_SHIFT;
	for (memp = mem; memp < &mem[NR_MEMS]; memp++) {
		if (memp->base == bootdev_base) {
			memp->base += bootdev_clicks;
			memp->size -= bootdev_clicks;
		}
	}
  }
#endif

  /* This actually is not needed, because ready() already set 'proc_ptr.' */
  lock_pick_proc();
  bill_ptr = proc_addr(IDLE);		/* it has to point somewhere */

  /* MINIX is now ready. Display the startup banner to the user and return 
   * to the assembly code to start running the current process. 
   */
  announce();
  restart();
}



/*==========================================================================*
 *				announce				    *
 *==========================================================================*/
PRIVATE void announce(void)
{
  /* Display the MINIX startup banner. */
  kprintf("MINIX %s.  Copyright 2001 Prentice-Hall, Inc.\n", 
      karg(kinfo.version));

#if (CHIP == INTEL)
  /* Real mode, or 16/32-bit protected mode? */
  kprintf("Executing in %s mode\n\n",
      machine.protected ? karg("32-bit protected") : karg("real"));
#endif

  /* Check if boot device was loaded with the kernel. */
  if (kinfo.bootdev_base > 0)
      kprintf("Image of /dev/boot loaded. Size: %u KB.\n", kinfo.bootdev_size);
}


/*==========================================================================*
 *			       prepare_shutdown				    *
 *==========================================================================*/
PUBLIC void prepare_shutdown(how)
int how;		/* 0 = halt, 1 = reboot, 2 = panic!, ... */
{
/* This function prepares to shutdown MINIX. It uses a global flag to make 
 * sure it is only executed once. Unless a CPU exception occurred, the 
 * stop_sequence() is started. 
 */
  if (shutting_down)
  	return;

  /* Show debugging dumps on panics. Make sure that the TTY task is still 
   * available to handle them. This is done with help of a non-blocking send. 
   * We rely on TTY to call sys_abort() when it is done with the dumps.
   */
  if (how == RBT_PANIC) {
      message m;
      m.m_type = PANIC_DUMPS;
      if (nb_send(TTY, &m) == OK)	/* don't block if TTY isn't ready */
          return;			/* await sys_abort() from TTY */
  }

  /* The TTY expects two HARD_STOP notifications. One to switch to the 
   * primary console for stop sequence output, and one to actually exit.
   */
  notify(TTY, HARD_STOP);		/* let TTY switch to console 0 */

  /* Run the stop sequence. The timer argument passes the shutdown status.
   * The stop sequence is skipped for fatal CPU exceptions.
   */
  shutting_down = TRUE;				/* flag for sys_exit() */
  tmr_arg(&shutdown_timer)->ta_int = how;	/* pass how in timer */
  if (skip_stop_sequence) {			/* set in exception() */
      kprintf("\nAn exception occured; skipping stop sequence.\n", NO_ARG);
      shutdown(how);		/* TTY isn't scheduled */
  } else {
      kprintf("\nNotifying system services about MINIX shutdown.\n", NO_ARG); 
      kprintf("Known bug: hitting a key before done will hang the monitor.\n", NO_ARG); 
      stop_sequence(&shutdown_timer);
  }
}

/*==========================================================================*
 *			        stop_sequence 				    *
 *==========================================================================*/
PUBLIC void stop_sequence(tp)
timer_t *tp;
{
/* Try to cleanly stop all system services before shutting down. For each 
 * process type, all processes are notified and given STOP_TICKS to cleanly
 * shutdown. The notification order is servers, drivers, tasks. The variable 
 * 'shutdown_process' is set globally to indicate the process next to stop 
 * so that the stop sequence can directly continue if it has exited. Only if 
 * stop sequence has finished, MINIX is brought down. 
 */
  static int level = P_SERVER;		/* start at the highest level */
  static struct proc *p = NIL_PROC;	/* next process to stop */
  static char *types[] = {"task","system","driver","server","user"}; 

  /* See if the last process' shutdown was successful. Else, force exit. */
  if (p != NIL_PROC) { 
      kprintf("[%s]\n", isalivep(p) ? karg("FAILED") : karg("OK"));
      if (isalivep(p))
          clear_proc(p->p_nr);		/* now force process to exit */ 
  }

  /* Find the next process that must be stopped. Continue where last search
   * ended or start at begin. Possibly go to next level while searching. If
   * the last level is completely searched, shutdown MINIX. Processes are
   * stopped in the order of dependencies, that is, from the highest level to
   * the lowest level so that, for example, the file system can still rely on 
   * device drivers to cleanly shutdown.  
   */ 
  if (p == NIL_PROC) p = BEG_PROC_ADDR; 
  while (TRUE) {
      if (isalivep(p) && p->p_type == level) {	/* found a process */
      	int w;
          kprintf("- Stopping %s ", karg(p->p_name));
          kprintf("%s ... ", karg(types[p->p_type]));
          shutdown_process = p;		/* directly continue if exited */
          notify(proc_number(p), HARD_STOP);
          set_timer(tp, get_uptime()+STOP_TICKS, stop_sequence);
          return;			/* allow the process to shut down */ 
      } 
      p++;				/* proceed to next process */
      if (p >= END_PROC_ADDR) {		/* proceed to next level */
      	  p = BEG_PROC_ADDR;		
       	  level = level - 1;		
          if (level == P_TASK) {	/* done; tasks must remain alive */
          	shutdown(tmr_arg(tp)->ta_int);
          	/* no return */
		return;
          }
      }
  }
}

/*==========================================================================*
 *				   shutdown 				    *
 *==========================================================================*/
PRIVATE void shutdown(int how)
{
/* This function is called from prepare_shutdown or stop_sequence to bring 
 * down MINIX. How to shutdown is in the argument: RBT_REBOOT, RBT_HALT, 
 * RBT_RESET. 
 */
  static u16_t magic = STOP_MEM_CHECK;

  /* Now mask all interrupts, including the clock, and stop the clock. */
  outb(INT_CTLMASK, ~0); 
  clock_stop();

  if (mon_return && how != RBT_RESET) {
	/* Reinitialize the interrupt controllers to the BIOS defaults. */
	intr_init(0);
	outb(INT_CTLMASK, 0);
	outb(INT2_CTLMASK, 0);

	/* Return to the boot monitor. Set the program for the boot monitor.
	 * For RBT_MONITOR, the MM has provided the program.
	 */
	if (how == RBT_HALT) {
		phys_copy(vir2phys("delay;"), kinfo.params_base, 7); 
	} else if (how == RBT_REBOOT) {
		phys_copy(vir2phys("delay;boot"), kinfo.params_base, 11);
	}
	level0(monitor);
  }

  /* Stop BIOS memory test. */
  phys_copy(vir2phys(&magic), SOFT_RESET_FLAG_ADDR, SOFT_RESET_FLAG_SIZE);

  /* Reset the system by jumping to the reset address (real mode), or by
   * forcing a processor shutdown (protected mode).
   */
  level0(reset);
}

