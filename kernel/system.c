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
 *   vir_copy: 		copy bytes from one process to another
 *   generic_handler:	interrupt handler for user-level device drivers
 *
 * Changes:
 *   Oct 29, 2004   new clear_proc() function  (Jorrit N. Herder)
 *   Oct 17, 2004   generic handler and IRQ policies  (Jorrit N. Herder)
 *   Oct 10, 2004   dispatch system calls from call vector  (Jorrit N. Herder)
 *   Sep 30, 2004   source code documentation updated  (Jorrit N. Herder)
 *   Sep 10, 2004   system call functions in library  (Jorrit N. Herder)
 *      2003/2004   various new syscalls (see syslib.h)  (Jorrit N. Herder)
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
#include "protect.h"
#endif

FORWARD _PROTOTYPE( void initialize, (void));

/* Declaration of the call vector that defines the mapping of system calls to
 * handler functions. The order of the do_call handler functions must match 
 * the SYS_CALL numbering defined in <minix/com.h>.
 */
PUBLIC _PROTOTYPE (int (*call_vec[]), (message *m_ptr) ) = {
  do_times,	/*  0: get uptime and process CPU time consumption */
  do_xit,	/*  1: informs kernel that a process has exited */
  do_unused,    /*  2: unused */
  do_sigctl,	/*  3: MM signal control (incl. POSIX style handling) */
  do_fork, 	/*  4: informs kernel that a process has forked */
  do_newmap,	/*  5: allows MM to set up a process memory map */
  do_copy,	/*  6: copy a block of data between processes */
  do_exec,	/*  7: sets program counter and stack pointer after EXEC */
  do_unused,	/*  8: unused */
  do_abort,	/*  9: MM or FS cannot go on; abort MINIX */
  do_kill, 	/* 10: cause a signal to be sent via MM */
  do_umap,	/* 11: compute the physical address for a virtual address */
  do_unused,	/* 12: returns the next free chunk of physical memory */
  do_trace,	/* 13: request a trace operation */
  do_vcopy, 	/* 14: request a series of data blocks to be copied */ 
  do_signalrm, 	/* 15: schedule an alarm that causes an alarm signal */
  do_syncalrm,	/* 16: schedule an alarm that sends a notification message */
  do_flagalrm,	/* 17: schedule an alarm that sets a timeout flag to 1 */
  do_unused,	/* 18: unused */
  do_svrctl,	/* 19: handles miscelleneous kernel control functions */
  do_sdevio,	/* 20: device I/O: phys_insb, _insw, _outsb, _outsw */
  do_unused, 	/* 21: unused */
  do_getinfo, 	/* 22: request some kind of system information */ 
  do_devio,   	/* 23: device I/O: inb, inw, inl, outb, outw, outl */ 
  do_vdevio,  	/* 24: device I/O: vector with in[b|w|l], out[b|w|l] */ 
  do_irqctl,  	/* 25: request an interrupt control operation */ 
  do_kmalloc, 	/* 26: request allocation of (DMA) buffer in mem chunk */ 
  do_iopenable,	/* 27: allow a user process to use I/O instructions */
  do_phys2seg,	/* 28: do a phys addr to segment selector/ offset conversion */
  do_exit, 	/* 29: an server or driver requests to be aborted */
  do_vircopy, 	/* 30: copy from process to process (virtual addressing) */
  do_physcopy, 	/* 31: copy from anywhere to anywhere (physical addressing) */
};

/* Check if system call table is correct. This should not fail. No space is
 * allocated here, because the dummy is declared extern. If the call vector
 * is unbalanced, the array size will be negative and this won't compile. 
 */
extern int dummy[sizeof(call_vec)==NR_SYS_CALLS*sizeof(call_vec[0]) ? 1 : -1];

/* Some system task variables. */
PRIVATE message m;				/* used to receive requests */


/*===========================================================================*
 *				sys_task				     *
 *===========================================================================*/
PUBLIC void sys_task()
{
/* Main entry point of sys_task.  Get the message and dispatch on type. */

  register int result;

  /* Initialize the system task. */
  initialize();

  while (TRUE) {
      /* Get work. */
      receive(ANY, &m);

      /* Handle the request. */
      if ((unsigned) m.m_type < NR_SYS_CALLS) {
          result = (*call_vec[m.m_type])(&m);	/* do system call */
      } else {
	  kprintf("SYS task got illegal request from %d.\n", m.m_source);
	  result = EBADREQUEST;			/* illegal message type */
      }

      /* Send a reply, unless inhibited by a handler function. */
      if (result != EDONTREPLY) {
  	  m.m_type = result;	/* report status of call */
	  send(m.m_source, &m);	/* send reply to caller */
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

  /* Initialize IRQ table. */
  for (i=0; i<NR_IRQ_VECTORS; i++)
      irqtab[i].proc_nr = NONE;

  /* Initialize all alarm timers for all processes. */
  for (rp=BEG_PROC_ADDR; rp < END_PROC_ADDR; rp++) {
    tmr_inittimer(&(rp->p_signalrm));
    tmr_inittimer(&(rp->p_syncalrm));
    tmr_inittimer(&(rp->p_flagalrm));
  }
}

/*===========================================================================*
 *			         clear_proc				     *
 *===========================================================================*/
PUBLIC void clear_proc(proc_nr)
int proc_nr;				/* slot of process to clean up */
{
  register struct proc *rp, *rc;
  struct proc *np, *xp;

  /* Get a pointer to the process that exited. */
  rc = proc_addr(proc_nr);

  /* Turn off any alarm timers at the clock. */   
  reset_timer(&rc->p_signalrm);
  reset_timer(&rc->p_flagalrm);
  reset_timer(&rc->p_syncalrm);

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
          if (rp->p_callerq == NIL_PROC) continue;
          if (rp->p_callerq == rc) {
              /* Exiting process is on front of this queue. */
              rp->p_callerq = rc->p_sendlink;
              break;
          } else {
              /* See if exiting process is in middle of queue. */
              np = rp->p_callerq;
              while ( ( xp = np->p_sendlink) != NIL_PROC) {
                  if (xp == rc) {
                      np->p_sendlink = xp->p_sendlink;
                      break;
                  } else {
                      np = xp;
                  }
              }
          }
      }
  }

  /* Now clean up the process table entry. Reset to defaults. */
  kstrncpy(rc->p_name, "<noname>", PROC_NAME_LEN);	/* unset name */
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
 *			       generic_handler				     *
 *===========================================================================*/
PUBLIC int generic_handler(hook)
irq_hook_t *hook;	
{
/* This function handles hardware interrupt in a generic way, according to 
 * the policy set with SYS_IRQCTL. This is rather complicated since different
 * devices require different actions. Options are (1) do nothing, (2a) read a
 * port and optionally (2b) strobe the port high or (2c) low with the value
 * read, or (3) write a value to a port. Finally, the policy may or may not
 * reenable IRQs. A notification is sent in all cases.
 */ 
  irq_policy_t policy = irqtab[hook->irq].policy;
  int proc_nr = irqtab[hook->irq].proc_nr;
  long port = irqtab[hook->irq].port;
  phys_bytes addr = irqtab[hook->irq].addr;
  long mask_val = irqtab[hook->irq].mask_val;

  /* Read a value from the given port. Possibly also strobe the port with the
   * read value. Strobe it high by using the mask provided by the caller;
   * strobe it low by writing back the value we read.
   */
  if (policy & (IRQ_READ_PORT|IRQ_STROBE|IRQ_ECHO_VAL)) {
      switch(policy & (IRQ_BYTE|IRQ_WORD|IRQ_LONG)) {
      case IRQ_BYTE: {				/* byte values */
          u8_t byteval = inb(port);			
          if (policy & IRQ_STROBE) outb(port, byteval | mask_val); 
          if (policy & IRQ_ECHO_VAL) outb(port, byteval); 
          if (policy & IRQ_READ_PORT) 
          	phys_copy(vir2phys(&byteval), addr, sizeof(u8_t));
          break;
      } case IRQ_WORD: {			/* word values */
          u16_t wordval = inw(port); 
          if (policy & IRQ_STROBE) outw(port, wordval | mask_val); 
          if (policy & IRQ_ECHO_VAL) outw(port, wordval); 
          if (policy & IRQ_READ_PORT)		
          	phys_copy(vir2phys(&wordval), addr, sizeof(u16_t));
          break;
      } case IRQ_LONG: { 				/* long values */
          u32_t longval = inl(port); 
          if (policy & IRQ_STROBE) outl(port, longval | mask_val); 
          if (policy & IRQ_ECHO_VAL) outl(port, longval); 
          if (policy & IRQ_READ_PORT)	
          	phys_copy(vir2phys(&longval), addr, sizeof(u32_t));
          break;
      } default: /* do nothing */ ;		/* wrong type flags */
      }
  }
  /* Write a value to some port. This is straightforward. Note that both
   * reading and writing is not possible, hence 'else if' instead of 'if'.
   */
  else if (policy & (IRQ_WRITE_PORT)) {
      switch(policy & (IRQ_BYTE|IRQ_WORD|IRQ_LONG)) {
      case IRQ_BYTE: outb(port,  (u8_t) mask_val); break;
      case IRQ_WORD: outw(port, (u16_t) mask_val); break;
      case IRQ_LONG: outl(port, (u32_t) mask_val); break;
      default: /* do nothing */ ;		/* wrong type flags */
      }
  }

  /* Almost done, send a HARD_INT notification to allow further processing 
   * and possibly reenable interrupts - this depends on the policy given.
   */
  notify(proc_nr, HARD_INT);
  return(policy & IRQ_REENABLE);
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
 * handled by sending a message to MM.  This central function handles the 
 * signals and makes sure the MM gets them by sending a notification. The 
 * process being signaled is blocked while MM has not finished all signals 
 * for it.  These signals are counted in p_pendcount, and the  SIG_PENDING 
 * flag is kept nonzero while there are some.  It is not sufficient to ready 
 * the process when MM is informed, because MM can block waiting for FS to
 * do a core dump.
 */
  register struct proc *rp, *mmp;

  rp = proc_addr(proc_nr);
  if (sigismember(&rp->p_pending, sig_nr))
	return;			/* this signal already pending */
  sigaddset(&rp->p_pending, sig_nr);
  ++rp->p_pendcount;		/* count new signal pending */
  if (rp->p_flags & PENDING)
	return;			/* another signal already pending */
  if (rp->p_flags == 0) lock_unready(rp);
  rp->p_flags |= PENDING | SIG_PENDING;
  notify(MM_PROC_NR, KSIG_PENDING);
}


/*===========================================================================*
 *				umap_bios					     *
 *===========================================================================*/
PUBLIC phys_bytes umap_bios(rp, vir_addr, bytes)
register struct proc *rp;	/* pointer to proc table entry for process */
vir_bytes vir_addr;		/* virtual address in BIOS segment */
vir_bytes bytes;		/* # of bytes to be copied */
{
/* Calculate the physical memory address at the BIOS. */
  phys_bytes phys_addr;

  phys_addr = (phys_bytes) vir_addr;		/* no check currently! */

  return phys_addr;
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


#if ENABLE_MESSAGE_STATS

/*===========================================================================*
 *				do_mstats				     *
 *===========================================================================*/
PRIVATE int do_mstats(m_ptr)
message *m_ptr;			/* pointer to request message */
{
	int r = 0;

	if(m_ptr->m1_i1 > 0) {
		struct message_statentry *dest;
		struct proc *p;
		p = proc_addr(m_ptr->m1_i3);
		dest = proc_vir2phys(p, m_ptr->m1_p1);
		r = mstat_copy(dest, m_ptr->m1_i1);
	}

	if(m_ptr->m1_i2) {
		mstat_reset();
	}

	return r;
}

#endif /* ENABLE_MESSAGE_STATS */

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
  phys_bytes phys_addr;

  phys_addr = (phys_bytes) 0;		/* no yet supported currently! */

  return phys_addr;
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
 * Virtual addresses can be in LOCAL_SEG, REMOTE_SEG, or BIOS_SEG.
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
      default:
          kprintf("v_cp: Unknown segment type: %d\n", 
              vir_addr[i]->segment & SEGMENT_TYPE);
          return(EINVAL);
      }

      /* Check if mapping succeeded. */
      if (phys_addr[i] <= 0) {
          kprintf("v_cp: Mapping failed ... phys <= 0\n", NO_ARG);
          return(EFAULT);
      }
  }

  /* Now copy bytes between physical addresseses. */
  phys_copy(phys_addr[_SRC_], phys_addr[_DST_], (phys_bytes) bytes);
  return(OK);
}

/*==========================================================================*
 *				vir_copy					    *
 *==========================================================================*/
PUBLIC int vir_copy(src_proc, src_vir, dst_proc, dst_vir, bytes)
int src_proc;			/* source process */
vir_bytes src_vir;		/* source virtual address within D seg */
int dst_proc;			/* destination process */
vir_bytes dst_vir;		/* destination virtual address within D seg */
vir_bytes bytes;		/* # of bytes to copy  */
{
/* Copy bytes from one process to another.  Meant for the easy cases, where
 * speed isn't required.  (One can normally do without one of the umaps.)
 */
  phys_bytes src_phys, dst_phys;

  src_phys = umap_local(proc_addr(src_proc), D, src_vir, bytes);
  dst_phys = umap_local(proc_addr(dst_proc), D, dst_vir, bytes);
  if (src_phys == 0 || dst_phys == 0) return(EFAULT);
  phys_copy(src_phys, dst_phys, (phys_bytes) bytes);
  return(OK);
}



