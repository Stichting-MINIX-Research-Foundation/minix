/* This task handles the interface between the kernel and user-level servers.
 * System services can be accessed by doing a system call. System calls are 
 * transformed into request messages, which are handled by this task. By 
 * convention, a sys_call() is transformed in a SYS_CALL request message that
 * is handled in a function named do_call(). 
 *
 * A private call vector is used to map all system calls to the functions that
 * handle them. The actual handler functions are contained in separate files
 * to keep this file clean. The call vector is used in the system task's main
 * loop to handle all incoming requests.  
 *
 * In addition to the main sys_task() entry point, which starts the main loop,
 * there are several other minor entry points:
 *   cause_sig:		take action to cause a signal to occur
 *   clear_proc:	clean up a process in the process table, e.g. on exit
 *   umap_local:	map virtual address in LOCAL_SEG to physical 
 *   umap_remote:	map virtual address in REMOTE_SEG to physical 
 *   umap_bios:		map virtual address in BIOS_SEG to physical 
 *   numap_local:	umap_local D segment from proc nr instead of pointer
 *   virtual_copy:	copy bytes from one virtual address to another 
 *   get_randomness:	accumulate randomness in a buffer
 *   generic_handler:	interrupt handler for user-level device drivers
 *
 * Changes:
 *   Apr 25, 2005   made mapping of call vector explicit  (Jorrit N. Herder)
 *   Oct 29, 2004   new clear_proc() function  (Jorrit N. Herder)
 *   Oct 17, 2004   generic handler and IRQ policies  (Jorrit N. Herder)
 *   Oct 10, 2004   dispatch system calls from call vector  (Jorrit N. Herder)
 *   Sep 30, 2004   source code documentation updated  (Jorrit N. Herder)
 *   Sep 10, 2004   system call functions in library  (Jorrit N. Herder)
 *   2004 to 2005   various new syscalls (see syslib.h)  (Jorrit N. Herder)
 */

#include "kernel.h"
#include "system.h"
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/sigcontext.h>
#include <sys/svrctl.h>
#include <minix/callnr.h>
#include "sendmask.h"
#if (CHIP == INTEL)
#include <ibm/memory.h>
#include "protect.h"
#endif

/* Declaration of the call vector that defines the mapping of system calls 
 * to handler functions. The vector is initialized in sys_init() with map(), 
 * which makes sure the system call numbers are ok. No space is allocated, 
 * because the dummy is declared extern. If an illegal call is given, the 
 * array size will be negative and this won't compile. 
 */
PUBLIC int (*call_vec[NR_SYS_CALLS])(message *m_ptr);

#define map(call_nr, handler) \
	{extern int dummy[NR_SYS_CALLS > (unsigned) (call_nr) ? 1 : -1];} \
	call_vec[(call_nr)] = (handler)

FORWARD _PROTOTYPE( void initialize, (void));


/*===========================================================================*
 *				sys_task				     *
 *===========================================================================*/
PUBLIC void sys_task()
{
/* Main entry point of sys_task.  Get the message and dispatch on type. */
  static message m;
  register int result, debug;

  /* Initialize the system task. */
  initialize();

  while (TRUE) {
      /* Get work. */
      receive(ANY, &m);

      /* Handle the request. */
      if ((unsigned) m.m_type < NR_SYS_CALLS) {
          result = (*call_vec[m.m_type])(&m);	/* do system call */
      } else {
	  kprintf("Warning, illegal SYSTASK request from %d.\n", m.m_source);
	  result = EBADREQUEST;			/* illegal message type */
      }

      /* Send a reply, unless inhibited by a handler function. Use the kernel
       * function lock_send() to prevent a system call trap. The destination
       * is known to be blocked waiting for a message.
       */
      if (result != EDONTREPLY) {
          debug = m.m_type;
  	  m.m_type = result;	/* report status of call */
          if (OK != lock_send(m.m_source, &m)) {
              kprintf("Warning, SYSTASK couldn't reply to request %d", debug);
              kprintf(" from %d\n", m.m_source);
          }
      }
  }
}


/*===========================================================================*
 *			          initialize				     *
 *===========================================================================*/
PRIVATE void initialize(void)
{
  register struct proc *rp;
  int i;

  /* Initialize IRQ handler hooks. Mark all hooks available. */
  for (i=0; i<NR_IRQ_HOOKS; i++) {
      irq_hooks[i].proc_nr = NONE;
  }

  /* Initialize all alarm timers for all processes. */
  for (rp=BEG_PROC_ADDR; rp < END_PROC_ADDR; rp++) {
    tmr_inittimer(&(rp->p_alarm_timer));
  }

  /* Initialize the call vector to a safe default handler. Some system calls 
   * may be disabled or nonexistant. Then explicitely map known calls to their
   * handler functions. This is done with a macro that gives a compile error
   * if an illegal call number is used. The ordering is not important here.
   */
  for (i=0; i<NR_SYS_CALLS; i++) {
      call_vec[i] = do_unused;
  }

  /* Process management. */
  map(SYS_FORK, do_fork); 	/* informs kernel that a process has forked */
  map(SYS_XIT, do_xit);		/* informs kernel that a process has exited */
  map(SYS_NEWMAP, do_newmap);	/* allows PM to set up a process memory map */
  map(SYS_EXEC, do_exec);	/* sets program counter and stack pointer after EXEC */
  map(SYS_TRACE, do_trace);	/* request a trace operation */

  /* Signal handling. */
  map(SYS_KILL, do_kill); 		/* cause a process to be signaled */
  map(SYS_GETSIG, do_getsig);		/* PM checks for pending signals */
  map(SYS_ENDSIG, do_endsig);		/* PM finished processing signal */
  map(SYS_SIGSEND, do_sigsend);		/* start POSIX-style signal */
  map(SYS_SIGRETURN, do_sigreturn);	/* return from POSIX-style signal */

  /* Clock functionality. */
  map(SYS_TIMES, do_times);		/* get uptime and process times */
  map(SYS_SIGNALRM, do_signalrm); 	/* causes an alarm signal */
  map(SYS_SYNCALRM, do_syncalrm);	/* send a notification message */

  /* Device I/O. */
  map(SYS_IRQCTL, do_irqctl);  		/* interrupt control operations */ 
  map(SYS_DEVIO, do_devio);   		/* inb, inw, inl, outb, outw, outl */ 
  map(SYS_SDEVIO, do_sdevio);		/* phys_insb, _insw, _outsb, _outsw */
  map(SYS_VDEVIO, do_vdevio);  		/* vector with devio requests */ 

  /* Server and driver control. */
  map(SYS_SEGCTL, do_segctl);		/* add segment and get selector */
  map(SYS_IOPENABLE, do_iopenable);	/* enable CPU I/O protection bits */
  map(SYS_SVRCTL, do_svrctl);		/* kernel control functions */
  map(SYS_EXIT, do_exit); 		/* exit a system process*/

  /* Copying. */
  map(SYS_UMAP, do_umap);		/* map virtual to physical address */
  map(SYS_VIRCOPY, do_vircopy); 	/* use pure virtual addressing */
  map(SYS_PHYSCOPY, do_physcopy); 	/* use physical addressing */
  map(SYS_PHYSZERO, do_physzero);	/* zero physical memory region */
  map(SYS_VIRVCOPY, do_virvcopy);	/* vector with copy requests */
  map(SYS_PHYSVCOPY, do_physvcopy);	/* vector with copy requests */

  /* Miscellaneous. */
  map(SYS_ABORT, do_abort);		/* abort MINIX */
  map(SYS_GETINFO, do_getinfo); 	/* request system information */ 
}

/*===========================================================================*
 *			         clear_proc				     *
 *===========================================================================*/
PUBLIC void clear_proc(proc_nr)
int proc_nr;				/* slot of process to clean up */
{
  register struct proc *rp, *rc;
#if DEAD_CODE
  struct proc *np, *xp;
#else
  register struct proc **xpp;		/* iterate over caller queue */
#endif
  int i;

  /* Get a pointer to the process that exited. */
  rc = proc_addr(proc_nr);

  /* Turn off any alarm timers at the clock. */   
  reset_timer(&rc->p_alarm_timer);

  /* Make sure the exiting process is no longer scheduled. */
  if (rc->p_flags == 0) lock_unready(rc);

  /* If the process being terminated happens to be queued trying to send a
   * message (e.g., the process was killed by a signal, rather than it doing 
   * an exit or it is forcibly shutdown in the stop sequence), then it must
   * be removed from the message queues.
   */
  if (rc->p_flags & SENDING) {
      /* Check all proc slots to see if the exiting process is queued. */
      for (rp = BEG_PROC_ADDR; rp < END_PROC_ADDR; rp++) {
          if (rp->p_caller_q == NIL_PROC) continue;
#if DEAD_CODE
          if (rp->p_caller_q == rc) {
              /* Exiting process is on front of this queue. */
              rp->p_caller_q = rc->p_q_link;
              break;
          } else {
              /* See if exiting process is in middle of queue. */
              np = rp->p_caller_q;
              while ( ( xp = np->p_q_link) != NIL_PROC) {
                  if (xp == rc) {
                      np->p_q_link = xp->p_q_link;
                      break;
                  } else {
                      np = xp;
                  }
              }
          }
#else
          /* Make sure that the exiting process is not on the queue. */
          xpp = &rp->p_caller_q;
          while (*xpp != NIL_PROC) {		/* check entire queue */
              if (*xpp == rc) {			/* process is on the queue */
                  *xpp = (*xpp)->p_q_link;	/* replace by next process */
                  break;
              }
              xpp = &(*xpp)->p_q_link;		/* proceed to next queued */
          }
#endif
      }
  }

  /* Check the table with IRQ hooks to see if hooks should be released. */
  for (i=0; i < NR_IRQ_HOOKS; i++) {
      if (irq_hooks[i].proc_nr == proc_nr)
          irq_hooks[i].proc_nr = NONE; 
  }

  /* Check if there are pending notifications. Release the buffers. */
  while (rc->p_ntf_q != NULL) {
      i = (int) (rc->p_ntf_q - &notify_buffer[0]);
      free_bit(i, notify_bitmap, NR_NOTIFY_BUFS); 
      rc->p_ntf_q = rc->p_ntf_q->n_next;
  }

  /* Now clean up the process table entry. Reset to defaults. */
  kstrncpy(rc->p_name, "<none>", P_NAME_LEN);	/* unset name */
  sigemptyset(&rc->p_pending);		/* remove pending signals */
  rc->p_pendcount = 0;			/* all signals are gone */
  rc->p_flags = 0;			/* remove all flags */
  rc->p_type = P_NONE;			/* announce slot empty */
  rc->p_sendmask = DENY_ALL_MASK;	/* set most restrictive mask */

#if (CHIP == M68000)
  pmmu_delete(rc);			/* we're done, remove tables */
#endif
}


/*===========================================================================*
 *			       get_randomness				     *
 *===========================================================================*/
PUBLIC void get_randomness()
{
/* Gather random information with help of the CPU's cycle counter. Only use 
 * the lowest bytes because the highest bytes won't differ that much. 
 */ 
  unsigned long tsc_high;
  read_tsc(&tsc_high, &krandom.r_buf[krandom.r_next]);
  if (krandom.r_size < RANDOM_ELEMENTS) krandom.r_size ++;
  krandom.r_next = (krandom.r_next + 1 ) % RANDOM_ELEMENTS;
}


/*===========================================================================*
 *			       generic_handler				     *
 *===========================================================================*/
PUBLIC int generic_handler(hook)
irq_hook_t *hook;	
{
/* This function handles hardware interrupt in a simple and generic way. All
 * interrupts are transformed into messages to a driver. The IRQ line will be
 * reenabled if the policy says so.
 * In addition, the interrupt handler gathers random information in a buffer
 * by timestamping the interrupts.
 */
  message m;

  /* Gather random information. */ 
  get_randomness();

  /* Build notification message and return. */
  m.NOTIFY_TYPE = HARD_INT;
  m.NOTIFY_ARG = hook->irq;
  lock_notify(hook->proc_nr, &m);
  return(hook->policy & IRQ_REENABLE);
}


/*===========================================================================*
 *				cause_sig				     *
 *===========================================================================*/
PUBLIC void cause_sig(proc_nr, sig_nr)
int proc_nr;			/* process to be signalled */
int sig_nr;			/* signal to be sent, 1 to _NSIG */
{
/* A task wants to send a signal to a process.   Examples of such tasks are:
 *   TTY wanting to cause SIGINT upon getting a DEL
 *   CLOCK wanting to cause SIGALRM when timer expires
 * FS also uses this to send a signal, via the SYS_KILL message. Signals are
 * handled by sending a message to PM.  This central function handles the 
 * signals and makes sure the PM gets them by sending a notification. The 
 * process being signaled is blocked while PM has not finished all signals 
 * for it.  These signals are counted in p_pendcount, and the  SIG_PENDING 
 * flag is kept nonzero while there are some.  It is not sufficient to ready 
 * the process when PM is informed, because PM can block waiting for FS to
 * do a core dump.
 */
  register struct proc *rp, *mmp;
  message m;

  rp = proc_addr(proc_nr);
  if (sigismember(&rp->p_pending, sig_nr))
	return;			/* this signal already pending */
  sigaddset(&rp->p_pending, sig_nr);
  ++rp->p_pendcount;		/* count new signal pending */
  if (rp->p_flags & PENDING)
	return;			/* another signal already pending */
  if (rp->p_flags == 0) lock_unready(rp);
  rp->p_flags |= PENDING | SIG_PENDING;
  m.NOTIFY_TYPE = KSIG_PENDING;
  m.NOTIFY_ARG = 0;
  m.NOTIFY_FLAGS = 0;
  lock_notify(PM_PROC_NR, &m);
}


/*===========================================================================*
 *				umap_bios				     *
 *===========================================================================*/
PUBLIC phys_bytes umap_bios(rp, vir_addr, bytes)
register struct proc *rp;	/* pointer to proc table entry for process */
vir_bytes vir_addr;		/* virtual address in BIOS segment */
vir_bytes bytes;		/* # of bytes to be copied */
{
/* Calculate the physical memory address at the BIOS. Note: currently, BIOS
 * address zero (the first BIOS interrupt vector) is not considered, as an 
 * error here, but since the physical address will be zero as well, the 
 * calling function will think an error occurred. This is not a problem,
 * since no one uses the first BIOS interrupt vector.  
 */
  phys_bytes phys_addr;

  /* Check all acceptable ranges. */
#if 0
  if (vir_addr >= BIOS_MEM_BEGIN && vir_addr + bytes <= BIOS_MEM_END)
  	return (phys_bytes) vir_addr;
  else if (vir_addr >= UPPER_MEM_BEGIN && vir_addr + bytes <= UPPER_MEM_END)
  	return (phys_bytes) vir_addr;
#else
  if (vir_addr >= BIOS_MEM_BEGIN && vir_addr + bytes <= UPPER_MEM_END)
  	return (phys_bytes) vir_addr;
#endif

  kprintf("Warning, error in umap_bios, virtual address 0x%x\n", vir_addr);
  return 0;
}


/*===========================================================================*
 *				umap_local				     *
 *===========================================================================*/
PUBLIC phys_bytes umap_local(rp, seg, vir_addr, bytes)
register struct proc *rp;	/* pointer to proc table entry for process */
int seg;			/* T, D, or S segment */
vir_bytes vir_addr;		/* virtual address in bytes within the seg */
vir_bytes bytes;		/* # of bytes to be copied */
{
/* Calculate the physical memory address for a given virtual address. */

  vir_clicks vc;		/* the virtual address in clicks */
  phys_bytes pa;		/* intermediate variables as phys_bytes */
#if (CHIP == INTEL)
  phys_bytes seg_base;
#endif

  /* If 'seg' is D it could really be S and vice versa.  T really means T.
   * If the virtual address falls in the gap,  it causes a problem. On the
   * 8088 it is probably a legal stack reference, since "stackfaults" are
   * not detected by the hardware.  On 8088s, the gap is called S and
   * accepted, but on other machines it is called D and rejected.
   * The Atari ST behaves like the 8088 in this respect.
   */

  if (bytes <= 0) return( (phys_bytes) 0);
  if (vir_addr + bytes <= vir_addr) return 0;	/* overflow */
  vc = (vir_addr + bytes - 1) >> CLICK_SHIFT;	/* last click of data */

#if (CHIP == INTEL) || (CHIP == M68000)
  if (seg != T)
	seg = (vc < rp->p_memmap[D].mem_vir + rp->p_memmap[D].mem_len ? D : S);
#else
  if (seg != T)
	seg = (vc < rp->p_memmap[S].mem_vir ? D : S);
#endif

  if((vir_addr>>CLICK_SHIFT) >= rp->p_memmap[seg].mem_vir + 
  	rp->p_memmap[seg].mem_len) return( (phys_bytes) 0 );

  if(vc >= rp->p_memmap[seg].mem_vir + 
  	rp->p_memmap[seg].mem_len) return( (phys_bytes) 0 );

#if (CHIP == INTEL)
  seg_base = (phys_bytes) rp->p_memmap[seg].mem_phys;
  seg_base = seg_base << CLICK_SHIFT;	/* segment origin in bytes */
#endif
  pa = (phys_bytes) vir_addr;
#if (CHIP != M68000)
  pa -= rp->p_memmap[seg].mem_vir << CLICK_SHIFT;
  return(seg_base + pa);
#endif
#if (CHIP == M68000)
  pa -= (phys_bytes)rp->p_memmap[seg].mem_vir << CLICK_SHIFT;
  pa += (phys_bytes)rp->p_memmap[seg].mem_phys << CLICK_SHIFT;
  return(pa);
#endif
}

/*==========================================================================*
 *				numap_local				    *
 *==========================================================================*/
PUBLIC phys_bytes numap_local(proc_nr, vir_addr, bytes)
int proc_nr;			/* process number to be mapped */
vir_bytes vir_addr;		/* virtual address in bytes within D seg */
vir_bytes bytes;		/* # of bytes required in segment  */
{
/* Do umap_local() starting from a process number instead of a pointer.  
 * This function is used by device drivers, so they need not know about the
 * process table.  To save time, there is no 'seg' parameter. The segment
 * is always D.
 */
  return(umap_local(proc_addr(proc_nr), D, vir_addr, bytes));
}


/*===========================================================================*
 *				umap_remote				     *
 *===========================================================================*/
PUBLIC phys_bytes umap_remote(rp, seg, vir_addr, bytes)
register struct proc *rp;	/* pointer to proc table entry for process */
int seg;			/* index of remote segment */
vir_bytes vir_addr;		/* virtual address in bytes within the seg */
vir_bytes bytes;		/* # of bytes to be copied */
{
/* Calculate the physical memory address for a given virtual address. */
  struct far_mem *fm;

  if (bytes <= 0) return( (phys_bytes) 0);
  if (seg < 0 || seg >= NR_REMOTE_SEGS) return( (phys_bytes) 0);

  fm = &rp->p_farmem[seg];
  if (! fm->in_use) return( (phys_bytes) 0);
  if (vir_addr + bytes > fm->mem_len) return( (phys_bytes) 0);

  return(fm->mem_phys + (phys_bytes) vir_addr); 
}

/*==========================================================================*
 *				virtual_copy				    *
 *==========================================================================*/
PUBLIC int virtual_copy(src_addr, dst_addr, bytes)
struct vir_addr *src_addr;	/* source virtual address */
struct vir_addr *dst_addr;	/* destination virtual address */
vir_bytes bytes;		/* # of bytes to copy  */
{
/* Copy bytes from virtual address src_addr to virtual address dst_addr. 
 * Virtual addresses can be in ABS, LOCAL_SEG, REMOTE_SEG, or BIOS_SEG.
 */
  struct vir_addr *vir_addr[2];	/* virtual source and destination address */ 
  phys_bytes phys_addr[2];	/* absolute source and destination */ 
  int seg_index;
  int i;

  /* Check copy count. */
  if (bytes <= 0) {
      kprintf("v_cp: copy count problem <= 0\n", NO_ARG);
      return(EDOM);
  }

  /* Do some more checks and map virtual addresses to physical addresses. */
  vir_addr[_SRC_] = src_addr;
  vir_addr[_DST_] = dst_addr;
  for (i=_SRC_; i<=_DST_; i++) {

      /* Get physical address. */
      switch((vir_addr[i]->segment & SEGMENT_TYPE)) {
      case LOCAL_SEG:
          seg_index = vir_addr[i]->segment & SEGMENT_INDEX;
          phys_addr[i] = umap_local( proc_addr(vir_addr[i]->proc_nr), 
              seg_index, vir_addr[i]->offset, bytes );
          break;
      case REMOTE_SEG:
          seg_index = vir_addr[i]->segment & SEGMENT_INDEX;
          phys_addr[i] = umap_remote( proc_addr(vir_addr[i]->proc_nr), 
              seg_index, vir_addr[i]->offset, bytes );
          break;
      case BIOS_SEG:
          phys_addr[i] = umap_bios( proc_addr(vir_addr[i]->proc_nr),
              vir_addr[i]->offset, bytes );
          break;
      case PHYS_SEG:
          phys_addr[i] = vir_addr[i]->offset;
          break;
      default:
          kprintf("v_cp: Unknown segment type: %d\n", 
              vir_addr[i]->segment & SEGMENT_TYPE);
          return(EINVAL);
      }

      /* Check if mapping succeeded. */
      if (phys_addr[i] <= 0 && vir_addr[i]->segment != PHYS_SEG) {
          kprintf("v_cp: Mapping failed ... phys <= 0\n", NO_ARG);
          return(EFAULT);
      }
  }

  /* Now copy bytes between physical addresseses. */
  phys_copy(phys_addr[_SRC_], phys_addr[_DST_], (phys_bytes) bytes);
  return(OK);
}


