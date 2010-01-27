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
 *   set_sendto_bit:	allow a process to send messages to a new target
 *   unset_sendto_bit:	disallow a process from sending messages to a target
 *   send_sig:		send a signal directly to a system process
 *   cause_sig:		take action to cause a signal to occur via PM
 *   sig_delay_done:	tell PM that a process is not sending
 *   umap_bios:		map virtual address in BIOS_SEG to physical 
 *   get_randomness:	accumulate randomness in a buffer
 *   clear_endpoint:	remove a process' ability to send and receive messages
 *
 * Changes:
*    Nov 22, 2009   get_priv supports static priv ids (Cristiano Giuffrida)
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
#include "vm.h"
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/sigcontext.h>
#include <minix/endpoint.h>
#include <minix/safecopies.h>
#include <minix/portio.h>

/* Declaration of the call vector that defines the mapping of system calls 
 * to handler functions. The vector is initialized in sys_init() with map(), 
 * which makes sure the system call numbers are ok. No space is allocated, 
 * because the dummy is declared extern. If an illegal call is given, the 
 * array size will be negative and this won't compile. 
 */
PUBLIC int (*call_vec[NR_SYS_CALLS])(message *m_ptr);
char *callnames[NR_SYS_CALLS];

#define map(call_nr, handler) \
    {extern int dummy[NR_SYS_CALLS>(unsigned)(call_nr-KERNEL_CALL) ? 1:-1];} \
    callnames[(call_nr-KERNEL_CALL)] = #call_nr;	\
    call_vec[(call_nr-KERNEL_CALL)] = (handler)  

FORWARD _PROTOTYPE( void initialize, (void));
FORWARD _PROTOTYPE( struct proc *vmrestart_check, (message *));

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
      struct proc *restarting;

      restarting = vmrestart_check(&m);

      if(!restarting) {
        int r;
	/* Get work. Block and wait until a request message arrives. */
	if((r=receive(ANY, &m)) != OK)
		minix_panic("receive() failed", r);
      } 

      sys_call_code = (unsigned) m.m_type;
      call_nr = sys_call_code - KERNEL_CALL;	
      who_e = m.m_source;
      okendpt(who_e, &who_p);
      caller_ptr = proc_addr(who_p);

      /* See if the caller made a valid request and try to handle it. */
      if (call_nr < 0 || call_nr >= NR_SYS_CALLS) {	/* check call number */
	  kprintf("SYSTEM: illegal request %d from %d.\n",
		call_nr,m.m_source);
	  result = EBADREQUEST;			/* illegal message type */
      } 
      else if (!GET_BIT(priv(caller_ptr)->s_k_call_mask, call_nr)) {
	  result = ECALLDENIED;			/* illegal message type */
      }
      else {
          result = (*call_vec[call_nr])(&m); /* handle the system call */
      }

      if(result == VMSUSPEND) {
	/* Special case: message has to be saved for handling
	 * until VM tells us it's allowed. VM has been notified
	 * and we must wait for its reply to restart the call.
	 */
        vmassert(RTS_ISSET(caller_ptr, RTS_VMREQUEST));
	vmassert(caller_ptr->p_vmrequest.type == VMSTYPE_KERNELCALL);
	caller_ptr->p_vmrequest.saved.reqmsg = m;
      } else if (result != EDONTREPLY) {
	/* Send a reply, unless inhibited by a handler function.
	 * Use the kernel function lock_send() to prevent a system
	 * call trap.
	 */
		if(restarting) {
        		vmassert(!RTS_ISSET(restarting, RTS_VMREQUEST));
#if 0
        		vmassert(!RTS_ISSET(restarting, RTS_VMREQTARGET));
#endif
		}
		m.m_type = result;		/* report status of call */
		if(WILLRECEIVE(caller_ptr, SYSTEM)) {
		  if (OK != (s=lock_send(m.m_source, &m))) {
			kprintf("SYSTEM, reply to %d failed: %d\n",
			m.m_source, s);
		  }
		} else {
			kprintf("SYSTEM: not replying to %d; not ready\n", 
				caller_ptr->p_endpoint);
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
      callnames[i] = "unused";
  }

  /* Process management. */
  map(SYS_FORK, do_fork); 		/* a process forked a new process */
  map(SYS_EXEC, do_exec);		/* update process after execute */
  map(SYS_EXIT, do_exit);		/* clean up after process exit */
  map(SYS_NICE, do_nice);		/* set scheduling priority */
  map(SYS_PRIVCTL, do_privctl);		/* system privileges control */
  map(SYS_TRACE, do_trace);		/* request a trace operation */
  map(SYS_SETGRANT, do_setgrant);	/* get/set own parameters */
  map(SYS_RUNCTL, do_runctl);		/* set/clear stop flag of a process */

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
  map(SYS_VMCTL, do_vmctl);		/* various VM process settings */

  /* Copying. */
  map(SYS_UMAP, do_umap);		/* map virtual to physical address */
  map(SYS_VIRCOPY, do_vircopy); 	/* use pure virtual addressing */
  map(SYS_PHYSCOPY, do_copy);	 	/* use physical addressing */
  map(SYS_SAFECOPYFROM, do_safecopy);	/* copy with pre-granted permission */
  map(SYS_SAFECOPYTO, do_safecopy);	/* copy with pre-granted permission */
  map(SYS_VSAFECOPY, do_vsafecopy);	/* vectored safecopy */

  /* Mapping. */
  map(SYS_SAFEMAP, do_safemap);		/* map pages from other process */
  map(SYS_SAFEREVMAP, do_saferevmap);	/* grantor revokes the map grant */
  map(SYS_SAFEUNMAP, do_safeunmap);	/* requestor unmaps the mapped pages */

  /* Clock functionality. */
  map(SYS_TIMES, do_times);		/* get uptime and process times */
  map(SYS_SETALARM, do_setalarm);	/* schedule a synchronous alarm */
  map(SYS_STIME, do_stime);		/* set the boottime */
  map(SYS_VTIMER, do_vtimer);		/* set or retrieve a virtual timer */

  /* System control. */
  map(SYS_ABORT, do_abort);		/* abort MINIX */
  map(SYS_GETINFO, do_getinfo); 	/* request system information */ 
  map(SYS_SYSCTL, do_sysctl); 		/* misc system manipulation */ 

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
PUBLIC int get_priv(rc, priv_id)
register struct proc *rc;		/* new (child) process pointer */
int priv_id;				/* privilege id */
{
/* Allocate a new privilege structure for a system process. Privilege ids
 * can be assigned either statically or dynamically.
 */
  register struct priv *sp;                 /* privilege structure */

  if(priv_id == NULL_PRIV_ID) {             /* allocate slot dynamically */
      for (sp = BEG_DYN_PRIV_ADDR; sp < END_DYN_PRIV_ADDR; ++sp) 
          if (sp->s_proc_nr == NONE) break;	
      if (sp >= END_DYN_PRIV_ADDR) return(ENOSPC);
  }
  else {                                    /* allocate slot from id */
      if(!is_static_priv_id(priv_id)) {
          return EINVAL;                    /* invalid static priv id */
      }
      if(priv[priv_id].s_proc_nr != NONE) {
          return EBUSY;                     /* slot already in use */
      }
      sp = &priv[priv_id];
  }
  rc->p_priv = sp;			    /* assign new slot */
  rc->p_priv->s_proc_nr = proc_nr(rc);	    /* set association */

  return(OK);
}

/*===========================================================================*
 *				set_sendto_bit				     *
 *===========================================================================*/
PUBLIC void set_sendto_bit(struct proc *rp, int id)
{
/* Allow a process to send messages to the process(es) associated with the
 * system privilege structure with the given ID. 
 */

  /* Disallow the process from sending to a process privilege structure with no
   * associated process, and disallow the process from sending to itself.
   */
  if (id_to_nr(id) == NONE || priv_id(rp) == id) {
	unset_sys_bit(priv(rp)->s_ipc_to, id);
	return;
  }

  set_sys_bit(priv(rp)->s_ipc_to, id);

  /* The process that this process can now send to, must be able to reply (or
   * vice versa). Therefore, its send mask should be updated as well. Ignore
   * receivers that don't support traps other than RECEIVE, they can't reply
   * or send messages anyway.
   */
  if (priv_addr(id)->s_trap_mask & ~((1 << RECEIVE)))
      set_sys_bit(priv_addr(id)->s_ipc_to, priv_id(rp));
}

/*===========================================================================*
 *				unset_sendto_bit			     *
 *===========================================================================*/
PUBLIC void unset_sendto_bit(struct proc *rp, int id)
{
/* Prevent a process from sending to another process. Retain the send mask
 * symmetry by also unsetting the bit for the other direction.
 */

  unset_sys_bit(priv(rp)->s_ipc_to, id);

  unset_sys_bit(priv_addr(id)->s_ipc_to, priv_id(rp));
}

/*===========================================================================*
 *				send_sig				     *
 *===========================================================================*/
PUBLIC void send_sig(int proc_nr, int sig_nr)
{
/* Notify a system process about a signal. This is straightforward. Simply
 * set the signal that is to be delivered in the pending signals map and 
 * send a notification with source SYSTEM.
 */ 
  register struct proc *rp;
  static int n;

  if(!isokprocn(proc_nr) || isemptyn(proc_nr))
	minix_panic("send_sig to empty process", proc_nr);

  rp = proc_addr(proc_nr);
  sigaddset(&priv(rp)->s_sig_pending, sig_nr);
  if(!intr_disabled()) {
	  lock_notify(SYSTEM, rp->p_endpoint); 
  } else {
	  mini_notify(proc_addr(SYSTEM), rp->p_endpoint); 
  }
}

/*===========================================================================*
 *				cause_sig				     *
 *===========================================================================*/
PUBLIC void cause_sig(proc_nr, sig_nr)
proc_nr_t proc_nr;		/* process to be signalled */
int sig_nr;			/* signal to be sent */
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

  if (proc_nr == PM_PROC_NR)
	minix_panic("cause_sig: PM gets signal", NO_NUM);

  /* Check if the signal is already pending. Process it otherwise. */
  rp = proc_addr(proc_nr);
  if (! sigismember(&rp->p_pending, sig_nr)) {
      sigaddset(&rp->p_pending, sig_nr);
      if (! (RTS_ISSET(rp, RTS_SIGNALED))) {		/* other pending */
	  RTS_LOCK_SET(rp, RTS_SIGNALED | RTS_SIG_PENDING);
          send_sig(PM_PROC_NR, SIGKSIG);
      }
  }
}

/*===========================================================================*
 *				sig_delay_done				     *
 *===========================================================================*/
PUBLIC void sig_delay_done(rp)
struct proc *rp;
{
/* A process is now known not to send any direct messages.
 * Tell PM that the stop delay has ended, by sending a signal to the process.
 * Used for actual signal delivery.
 */

  rp->p_misc_flags &= ~MF_SIG_DELAY;

  cause_sig(proc_nr(rp), SIGNDELAY);
}

#if _MINIX_CHIP == _CHIP_INTEL

/*===========================================================================*
 *				umap_bios				     *
 *===========================================================================*/
PUBLIC phys_bytes umap_bios(vir_addr, bytes)
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
 *                              umap_grant                                   *
 *===========================================================================*/
PUBLIC phys_bytes umap_grant(rp, grant, bytes)
struct proc *rp;                /* pointer to proc table entry for process */
cp_grant_id_t grant;            /* grant no. */
vir_bytes bytes;                /* size */
{
        int proc_nr;
        vir_bytes offset, ret;
        endpoint_t granter;

        /* See if the grant in that process is sensible, and 
         * find out the virtual address and (optionally) new
         * process for that address.
         *
         * Then convert that process to a slot number.
         */
        if(verify_grant(rp->p_endpoint, ANY, grant, bytes, 0, 0,
                &offset, &granter) != OK) {
		kprintf("SYSTEM: umap_grant: verify_grant failed\n");
                return 0;
        }

        if(!isokendpt(granter, &proc_nr)) {
		kprintf("SYSTEM: umap_grant: isokendpt failed\n");
                return 0;
        }
 
        /* Do the mapping from virtual to physical. */
        ret = umap_virtual(proc_addr(proc_nr), D, offset, bytes);
	if(!ret) {
		kprintf("SYSTEM:umap_grant:umap_virtual failed; grant %s:%d -> %s: vir 0x%lx\n",
			rp->p_name, grant, 
			proc_addr(proc_nr)->p_name, offset);
	}
	return ret;
}

/*===========================================================================*
 *			         clear_endpoint				     *
 *===========================================================================*/
PUBLIC void clear_endpoint(rc)
register struct proc *rc;		/* slot of process to clean up */
{
  register struct proc *rp;		/* iterate over process table */
  register struct proc **xpp;		/* iterate over caller queue */

  if(isemptyp(rc)) minix_panic("clear_proc: empty process", rc->p_endpoint);

  if(rc->p_endpoint == PM_PROC_NR || rc->p_endpoint == VFS_PROC_NR ||
	rc->p_endpoint == VM_PROC_NR)
  {
	/* This test is great for debugging system processes dying,
	 * but as this happens normally on reboot, not good permanent code.
	 */
	kprintf("died: ");
	proc_stacktrace(rc);
	minix_panic("system process died", rc->p_endpoint);
  }

  /* Make sure that the exiting process is no longer scheduled. */
  RTS_LOCK_SET(rc, RTS_NO_ENDPOINT);
  if (priv(rc)->s_flags & SYS_PROC)
  {
	if (priv(rc)->s_asynsize) {
#if 0
		kprintf("clear_endpoint: clearing s_asynsize of %s / %d\n",
			rc->p_name, rc->p_endpoint);
		proc_stacktrace(rc);
#endif
	}
	priv(rc)->s_asynsize= 0;
  }

  /* If the process happens to be queued trying to send a
   * message, then it must be removed from the message queues.
   */
  if (RTS_ISSET(rc, RTS_SENDING)) {
      int target_proc;

      okendpt(rc->p_sendto_e, &target_proc);
      xpp = &proc_addr(target_proc)->p_caller_q; /* destination's queue */
      while (*xpp != NIL_PROC) {		/* check entire queue */
          if (*xpp == rc) {			/* process is on the queue */
              *xpp = (*xpp)->p_q_link;		/* replace by next process */
#if DEBUG_ENABLE_IPC_WARNINGS
	      kprintf("endpoint %d / %s removed from queue at %d\n",
	          rc->p_endpoint, rc->p_name, rc->p_sendto_e);
#endif
              break;				/* can only be queued once */
          }
          xpp = &(*xpp)->p_q_link;		/* proceed to next queued */
      }
      rc->p_rts_flags &= ~RTS_SENDING;
  }
  rc->p_rts_flags &= ~RTS_RECEIVING;

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
      if (RTS_ISSET(rp, RTS_RECEIVING) && rp->p_getfrom_e == rc->p_endpoint) {
          rp->p_reg.retreg = ESRCDIED;		/* report source died */
	  RTS_LOCK_UNSET(rp, RTS_RECEIVING);	/* no longer receiving */
#if DEBUG_ENABLE_IPC_WARNINGS
	  kprintf("endpoint %d / %s receiving from dead src ep %d / %s\n",
		rp->p_endpoint, rp->p_name, rc->p_endpoint, rc->p_name);
#endif
      } 
      if (RTS_ISSET(rp, RTS_SENDING) &&
	  rp->p_sendto_e == rc->p_endpoint) {
          rp->p_reg.retreg = EDSTDIED;		/* report destination died */
	  RTS_LOCK_UNSET(rp, RTS_SENDING);
#if DEBUG_ENABLE_IPC_WARNINGS
	  kprintf("endpoint %d / %s send to dying dst ep %d (%s)\n",
		rp->p_endpoint, rp->p_name, rc->p_endpoint, rc->p_name);
#endif
      } 
  }
}

/*===========================================================================*
 *                              vmrestart_check                            *
 *===========================================================================*/
PRIVATE struct proc *vmrestart_check(message *m)
{
	int type;
	struct proc *restarting;

      /* Anyone waiting to be vm-restarted? */

	if(!(restarting = vmrestart))
		return NULL;

	vmassert(!RTS_ISSET(restarting, RTS_SLOT_FREE));
	vmassert(RTS_ISSET(restarting, RTS_VMREQUEST));

	type = restarting->p_vmrequest.type;
	restarting->p_vmrequest.type = VMSTYPE_SYS_NONE;
	vmrestart = restarting->p_vmrequest.nextrestart;

	switch(type) {
		case VMSTYPE_KERNELCALL:
			*m = restarting->p_vmrequest.saved.reqmsg;
			restarting->p_vmrequest.saved.reqmsg.m_source = NONE;
			vmassert(m->m_source == restarting->p_endpoint);
			/* Original caller could've disappeared in the meantime. */
		        if(!isokendpt(m->m_source, &who_p)) {
				kprintf("SYSTEM: ignoring call %d from dead %d\n",
					m->m_type, m->m_source);
				return NULL;
			}
			{ int i;
				i = m->m_type - KERNEL_CALL;
				if(i >= 0 && i < NR_SYS_CALLS) {
#if 0
					kprintf("SYSTEM: restart %s from %d\n",
					callnames[i], m->m_source);
#endif
				} else {
	   				minix_panic("call number out of range", i);
				}
			}
			return restarting;
		default:
	   		minix_panic("strange restart type", type);
	}
	minix_panic("fell out of switch", NO_NUM);
}
