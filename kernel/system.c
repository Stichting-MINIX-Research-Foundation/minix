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
 *   get_priv:		assign privilege structure to user or system process
 *   send_sig:		send a signal directly to a system process
 *   cause_sig:		take action to cause a signal to occur via PM
 *   umap_bios:		map virtual address in BIOS_SEG to physical 
 *   virtual_copy:	copy bytes from one virtual address to another 
 *   get_randomness:	accumulate randomness in a buffer
 *   clear_endpoint:	remove a process' ability to send and receive messages
 *
 * Changes:
 *   Aug 04, 2005   check if system call is allowed  (Jorrit N. Herder)
 *   Jul 20, 2005   send signal to services with message  (Jorrit N. Herder) 
 *   Jan 15, 2005   new, generalized virtual copy function  (Jorrit N. Herder)
 *   Oct 10, 2004   dispatch system calls from call vector  (Jorrit N. Herder)
 *   Sep 30, 2004   source code documentation updated  (Jorrit N. Herder)
 */

#include "debug.h"
#include "kernel.h"
#include "system.h"
#include "proc.h"
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/sigcontext.h>
#include <minix/endpoint.h>
#include <minix/safecopies.h>

/* Declaration of the call vector that defines the mapping of system calls 
 * to handler functions. The vector is initialized in sys_init() with map(), 
 * which makes sure the system call numbers are ok. No space is allocated, 
 * because the dummy is declared extern. If an illegal call is given, the 
 * array size will be negative and this won't compile. 
 */
PUBLIC int (*call_vec[NR_SYS_CALLS])(message *m_ptr);

#define map(call_nr, handler) \
    {extern int dummy[NR_SYS_CALLS>(unsigned)(call_nr-KERNEL_CALL) ? 1:-1];} \
    call_vec[(call_nr-KERNEL_CALL)] = (handler)  

FORWARD _PROTOTYPE( void initialize, (void));

/*===========================================================================*
 *				sys_task				     *
 *===========================================================================*/
PUBLIC void sys_task()
{
/* Main entry point of sys_task.  Get the message and dispatch on type. */
  static message m;
  register int result;
  register struct proc *caller_ptr;
  int s;
  int call_nr;

  /* Initialize the system task. */
  initialize();

  while (TRUE) {
      int r;
      /* Get work. Block and wait until a request message arrives. */
      if((r=receive(ANY, &m)) != OK) panic("system: receive() failed", r);
      sys_call_code = (unsigned) m.m_type;
      call_nr = sys_call_code - KERNEL_CALL;	
      who_e = m.m_source;
      okendpt(who_e, &who_p);
      caller_ptr = proc_addr(who_p);

      /* See if the caller made a valid request and try to handle it. */
      if (call_nr < 0 || call_nr >= NR_SYS_CALLS) {	/* check call number */
#if DEBUG_ENABLE_IPC_WARNINGS
	  kprintf("SYSTEM: illegal request %d from %d.\n",
		call_nr,m.m_source);
#endif
	  result = EBADREQUEST;			/* illegal message type */
      } 
      else if (!GET_BIT(priv(caller_ptr)->s_k_call_mask, call_nr)) {
#if DEBUG_ENABLE_IPC_WARNINGS
	  kprintf("SYSTEM: request %d from %d denied.\n",
		call_nr,m.m_source);
#endif
	  result = ECALLDENIED;			/* illegal message type */
      }
      else {
          result = (*call_vec[call_nr])(&m); /* handle the system call */
      }

      /* Send a reply, unless inhibited by a handler function. Use the kernel
       * function lock_send() to prevent a system call trap. The destination
       * is known to be blocked waiting for a message.
       */
      if (result != EDONTREPLY) {
  	  m.m_type = result;			/* report status of call */
          if (OK != (s=lock_send(m.m_source, &m))) {
              kprintf("SYSTEM, reply to %d failed: %d\n", m.m_source, s);
          }
      }
  }
}

/*===========================================================================*
 *				initialize				     *
 *===========================================================================*/
PRIVATE void initialize(void)
{
  register struct priv *sp;
  int i;

  /* Initialize IRQ handler hooks. Mark all hooks available. */
  for (i=0; i<NR_IRQ_HOOKS; i++) {
      irq_hooks[i].proc_nr_e = NONE;
  }

  /* Initialize all alarm timers for all processes. */
  for (sp=BEG_PRIV_ADDR; sp < END_PRIV_ADDR; sp++) {
    tmr_inittimer(&(sp->s_alarm_timer));
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
  map(SYS_FORK, do_fork); 		/* a process forked a new process */
  map(SYS_EXEC, do_exec);		/* update process after execute */
  map(SYS_EXIT, do_exit);		/* clean up after process exit */
  map(SYS_NICE, do_nice);		/* set scheduling priority */
  map(SYS_PRIVCTL, do_privctl);		/* system privileges control */
  map(SYS_TRACE, do_trace);		/* request a trace operation */
  map(SYS_SETGRANT, do_setgrant);	/* get/set own parameters */

  /* Signal handling. */
  map(SYS_KILL, do_kill); 		/* cause a process to be signaled */
  map(SYS_GETKSIG, do_getksig);		/* PM checks for pending signals */
  map(SYS_ENDKSIG, do_endksig);		/* PM finished processing signal */
  map(SYS_SIGSEND, do_sigsend);		/* start POSIX-style signal */
  map(SYS_SIGRETURN, do_sigreturn);	/* return from POSIX-style signal */

  /* Device I/O. */
  map(SYS_IRQCTL, do_irqctl);  		/* interrupt control operations */ 
  map(SYS_DEVIO, do_devio);   		/* inb, inw, inl, outb, outw, outl */ 
  map(SYS_VDEVIO, do_vdevio);  		/* vector with devio requests */ 

  /* Memory management. */
  map(SYS_NEWMAP, do_newmap);		/* set up a process memory map */
  map(SYS_SEGCTL, do_segctl);		/* add segment and get selector */
  map(SYS_MEMSET, do_memset);		/* write char to memory area */
  map(SYS_VM_SETBUF, do_vm_setbuf); 	/* PM passes buffer for page tables */
  map(SYS_VM_MAP, do_vm_map); 		/* Map/unmap physical (device) memory */

  /* Copying. */
  map(SYS_UMAP, do_umap);		/* map virtual to physical address */
  map(SYS_VIRCOPY, do_vircopy); 	/* use pure virtual addressing */
  map(SYS_PHYSCOPY, do_physcopy); 	/* use physical addressing */
  map(SYS_VIRVCOPY, do_virvcopy);	/* vector with copy requests */
  map(SYS_PHYSVCOPY, do_physvcopy);	/* vector with copy requests */
  map(SYS_SAFECOPYFROM, do_safecopy);	/* copy with pre-granted permission */
  map(SYS_SAFECOPYTO, do_safecopy);	/* copy with pre-granted permission */
  map(SYS_VSAFECOPY, do_vsafecopy);	/* vectored safecopy */

  /* Clock functionality. */
  map(SYS_TIMES, do_times);		/* get uptime and process times */
  map(SYS_SETALARM, do_setalarm);	/* schedule a synchronous alarm */

  /* System control. */
  map(SYS_ABORT, do_abort);		/* abort MINIX */
  map(SYS_GETINFO, do_getinfo); 	/* request system information */ 

  /* Profiling. */
  map(SYS_SPROF, do_sprofile);         /* start/stop statistical profiling */
  map(SYS_CPROF, do_cprofile);         /* get/reset call profiling data */
  map(SYS_PROFBUF, do_profbuf);        /* announce locations to kernel */

  /* i386-specific. */
#if _MINIX_CHIP == _CHIP_INTEL
  map(SYS_INT86, do_int86);  		/* real-mode BIOS calls */ 
  map(SYS_READBIOS, do_readbios);	/* read from BIOS locations */
  map(SYS_IOPENABLE, do_iopenable); 	/* Enable I/O */
  map(SYS_SDEVIO, do_sdevio);		/* phys_insb, _insw, _outsb, _outsw */
#endif
}

/*===========================================================================*
 *				get_priv				     *
 *===========================================================================*/
PUBLIC int get_priv(rc, proc_type)
register struct proc *rc;		/* new (child) process pointer */
int proc_type;				/* system or user process flag */
{
/* Get a privilege structure. All user processes share the same privilege 
 * structure. System processes get their own privilege structure. 
 */
  register struct priv *sp;			/* privilege structure */

  if (proc_type == SYS_PROC) {			/* find a new slot */
      for (sp = BEG_PRIV_ADDR; sp < END_PRIV_ADDR; ++sp) 
          if (sp->s_proc_nr == NONE && sp->s_id != USER_PRIV_ID) break;	
      if (sp->s_proc_nr != NONE) return(ENOSPC);
      rc->p_priv = sp;				/* assign new slot */
      rc->p_priv->s_proc_nr = proc_nr(rc);	/* set association */
      rc->p_priv->s_flags = SYS_PROC;		/* mark as privileged */
  } else {
      rc->p_priv = &priv[USER_PRIV_ID];		/* use shared slot */
      rc->p_priv->s_proc_nr = INIT_PROC_NR;	/* set association */

      /* s_flags of this shared structure are to be once at system startup. */
  }
  return(OK);
}

/*===========================================================================*
 *				get_randomness				     *
 *===========================================================================*/
PUBLIC void get_randomness(source)
int source;
{
/* Use architecture-dependent high-resolution clock for
 * raw entropy gathering.
 */
  int r_next;
  unsigned long tsc_high, tsc_low;

  source %= RANDOM_SOURCES;
  r_next= krandom.bin[source].r_next;
  read_tsc(&tsc_high, &tsc_low);
  krandom.bin[source].r_buf[r_next] = tsc_low;
  if (krandom.bin[source].r_size < RANDOM_ELEMENTS) {
  	krandom.bin[source].r_size ++;
  }
  krandom.bin[source].r_next = (r_next + 1 ) % RANDOM_ELEMENTS;
}

/*===========================================================================*
 *				send_sig				     *
 *===========================================================================*/
PUBLIC void send_sig(int proc_nr, int sig_nr)
{
/* Notify a system process about a signal. This is straightforward. Simply
 * set the signal that is to be delivered in the pending signals map and 
 * send a notification with source SYSTEM.
 *
 * Process number is verified to avoid writing in random places, but we
 * don't kprintf() or panic() because that causes send_sig() invocations.
 */ 
  register struct proc *rp;
  static int n;

  if(!isokprocn(proc_nr) || isemptyn(proc_nr))
	return;

  rp = proc_addr(proc_nr);
  sigaddset(&priv(rp)->s_sig_pending, sig_nr);
  lock_notify(SYSTEM, rp->p_endpoint); 
}

/*===========================================================================*
 *				cause_sig				     *
 *===========================================================================*/
PUBLIC void cause_sig(proc_nr, sig_nr)
int proc_nr;			/* process to be signalled */
int sig_nr;			/* signal to be sent, 1 to _NSIG */
{
/* A system process wants to send a signal to a process.  Examples are:
 *  - HARDWARE wanting to cause a SIGSEGV after a CPU exception
 *  - TTY wanting to cause SIGINT upon getting a DEL
 *  - FS wanting to cause SIGPIPE for a broken pipe 
 * Signals are handled by sending a message to PM.  This function handles the 
 * signals and makes sure the PM gets them by sending a notification. The 
 * process being signaled is blocked while PM has not finished all signals 
 * for it. 
 * Race conditions between calls to this function and the system calls that
 * process pending kernel signals cannot exist. Signal related functions are
 * only called when a user process causes a CPU exception and from the kernel 
 * process level, which runs to completion.
 */
  register struct proc *rp;

  /* Check if the signal is already pending. Process it otherwise. */
  rp = proc_addr(proc_nr);
  if (! sigismember(&rp->p_pending, sig_nr)) {
      sigaddset(&rp->p_pending, sig_nr);
      if (! (RTS_ISSET(rp, SIGNALED))) {		/* other pending */
	  RTS_LOCK_SET(rp, SIGNALED | SIG_PENDING);
          send_sig(PM_PROC_NR, SIGKSIG);
      }
  }
}

#if _MINIX_CHIP == _CHIP_INTEL

/*===========================================================================*
 *				umap_bios				     *
 *===========================================================================*/
PUBLIC phys_bytes umap_bios(rp, vir_addr, bytes)
register struct proc *rp;	/* pointer to proc table entry for process */
vir_bytes vir_addr;		/* virtual address in BIOS segment */
vir_bytes bytes;		/* # of bytes to be copied */
{
/* Calculate the physical memory address at the BIOS. Note: currently, BIOS
 * address zero (the first BIOS interrupt vector) is not considered as an 
 * error here, but since the physical address will be zero as well, the 
 * calling function will think an error occurred. This is not a problem,
 * since no one uses the first BIOS interrupt vector.  
 */

  /* Check all acceptable ranges. */
  if (vir_addr >= BIOS_MEM_BEGIN && vir_addr + bytes <= BIOS_MEM_END)
  	return (phys_bytes) vir_addr;
  else if (vir_addr >= BASE_MEM_TOP && vir_addr + bytes <= UPPER_MEM_END)
  	return (phys_bytes) vir_addr;

  kprintf("Warning, error in umap_bios, virtual address 0x%x\n", vir_addr);
  return 0;
}
#endif

/*===========================================================================*
 *				umap_verify_grant			     *
 *===========================================================================*/
PUBLIC phys_bytes umap_verify_grant(rp, grantee, grant, offset, bytes, access)
struct proc *rp;		/* pointer to proc table entry for process */
endpoint_t grantee;		/* who wants to do this */
cp_grant_id_t grant;		/* grant no. */
vir_bytes offset;		/* offset into grant */
vir_bytes bytes;		/* size */
int access;			/* does grantee want to CPF_READ or _WRITE? */
{
	int proc_nr;
	vir_bytes v_offset;
	endpoint_t granter;

	/* See if the grant in that process is sensible, and
	 * find out the virtual address and (optionally) new
	 * process for that address.
	 *
	 * Then convert that process to a slot number.
	 */
	if(verify_grant(rp->p_endpoint, grantee, grant, bytes, access, offset,
		&v_offset, &granter) != OK
	   || !isokendpt(granter, &proc_nr)) {
		return 0;
	}

	/* Do the mapping from virtual to physical. */
	return umap_local(proc_addr(proc_nr), D, v_offset, bytes);
}

/*===========================================================================*
 *                              umap_grant                                   *
 *===========================================================================*/
PUBLIC phys_bytes umap_grant(rp, grant, bytes)
struct proc *rp;                /* pointer to proc table entry for process */
cp_grant_id_t grant;            /* grant no. */
vir_bytes bytes;                /* size */
{
        int proc_nr;
        vir_bytes offset;
        endpoint_t granter;
 
        /* See if the grant in that process is sensible, and 
         * find out the virtual address and (optionally) new
         * process for that address.
         *
         * Then convert that process to a slot number.
         */
        if(verify_grant(rp->p_endpoint, ANY, grant, bytes, 0, 0,
                &offset, &granter) != OK) {
                return 0;
        }

        if(!isokendpt(granter, &proc_nr)) {
                return 0;
        }
 
        /* Do the mapping from virtual to physical. */
        return umap_local(proc_addr(proc_nr), D, offset, bytes);
}

/*===========================================================================*
 *				virtual_copy				     *
 *===========================================================================*/
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
  if (bytes <= 0) return(EDOM);

  /* Do some more checks and map virtual addresses to physical addresses. */
  vir_addr[_SRC_] = src_addr;
  vir_addr[_DST_] = dst_addr;
  for (i=_SRC_; i<=_DST_; i++) {
	int proc_nr, type;
	struct proc *p;

 	type = vir_addr[i]->segment & SEGMENT_TYPE;
	if(type != PHYS_SEG && isokendpt(vir_addr[i]->proc_nr_e, &proc_nr))
	   p = proc_addr(proc_nr);
	else
	   p = NULL;

      /* Get physical address. */
      switch(type) {
      case LOCAL_SEG:
	  if(!p) return EDEADSRCDST;
          seg_index = vir_addr[i]->segment & SEGMENT_INDEX;
          phys_addr[i] = umap_local(p, seg_index, vir_addr[i]->offset, bytes);
          break;
      case REMOTE_SEG:
	  if(!p) return EDEADSRCDST;
          seg_index = vir_addr[i]->segment & SEGMENT_INDEX;
          phys_addr[i] = umap_remote(p, seg_index, vir_addr[i]->offset, bytes);
          break;
#if _MINIX_CHIP == _CHIP_INTEL
      case BIOS_SEG:
	  if(!p) return EDEADSRCDST;
          phys_addr[i] = umap_bios(p, vir_addr[i]->offset, bytes );
          break;
#endif
      case PHYS_SEG:
          phys_addr[i] = vir_addr[i]->offset;
          break;
      case GRANT_SEG:
	  phys_addr[i] = umap_grant(p, vir_addr[i]->offset, bytes);
	  break;
      default:
          return(EINVAL);
      }

      /* Check if mapping succeeded. */
      if (phys_addr[i] <= 0 && vir_addr[i]->segment != PHYS_SEG) 
          return(EFAULT);
  }

  /* Now copy bytes between physical addresseses. */
  phys_copy(phys_addr[_SRC_], phys_addr[_DST_], (phys_bytes) bytes);
  return(OK);
}


/*===========================================================================*
 *			         clear_endpoint				     *
 *===========================================================================*/
PUBLIC void clear_endpoint(rc)
register struct proc *rc;		/* slot of process to clean up */
{
  register struct proc *rp;		/* iterate over process table */
  register struct proc **xpp;		/* iterate over caller queue */

  if(isemptyp(rc)) panic("clear_proc: empty process", proc_nr(rc));

  /* Make sure that the exiting process is no longer scheduled. */
  RTS_LOCK_SET(rc, NO_ENDPOINT);

  /* If the process happens to be queued trying to send a
   * message, then it must be removed from the message queues.
   */
  if (RTS_ISSET(rc, SENDING)) {
      int target_proc;

      okendpt(rc->p_sendto_e, &target_proc);
      xpp = &proc_addr(target_proc)->p_caller_q; /* destination's queue */
      while (*xpp != NIL_PROC) {		/* check entire queue */
          if (*xpp == rc) {			/* process is on the queue */
              *xpp = (*xpp)->p_q_link;		/* replace by next process */
#if DEBUG_ENABLE_IPC_WARNINGS
	      kprintf("Proc %d removed from queue at %d\n",
	          proc_nr(rc), rc->p_sendto_e);
#endif
              break;				/* can only be queued once */
          }
          xpp = &(*xpp)->p_q_link;		/* proceed to next queued */
      }
      rc->p_rts_flags &= ~SENDING;
  }
  rc->p_rts_flags &= ~RECEIVING;

  /* Likewise, if another process was sending or receive a message to or from 
   * the exiting process, it must be alerted that process no longer is alive.
   * Check all processes. 
   */
  for (rp = BEG_PROC_ADDR; rp < END_PROC_ADDR; rp++) {
      if(isemptyp(rp))
	continue;

      /* Unset pending notification bits. */
      unset_sys_bit(priv(rp)->s_notify_pending, priv(rc)->s_id);

      /* Check if process is receiving from exiting process. */
      if (RTS_ISSET(rp, RECEIVING) && rp->p_getfrom_e == rc->p_endpoint) {
          rp->p_reg.retreg = ESRCDIED;		/* report source died */
	  RTS_LOCK_UNSET(rp, RECEIVING);	/* no longer receiving */
#if DEBUG_ENABLE_IPC_WARNINGS
	  kprintf("Proc %d receive dead src %d\n", proc_nr(rp), proc_nr(rc));
#endif
      } 
      if (RTS_ISSET(rp, SENDING) &&
	  rp->p_sendto_e == rc->p_endpoint) {
          rp->p_reg.retreg = EDSTDIED;		/* report destination died */
	  RTS_LOCK_UNSET(rp, SENDING);
#if DEBUG_ENABLE_IPC_WARNINGS
	  kprintf("Proc %d send dead dst %d\n", proc_nr(rp), proc_nr(rc));
#endif
      } 
  }
}


