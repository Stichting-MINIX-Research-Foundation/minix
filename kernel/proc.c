/* This file contains essentially all of the process and message handling.
 * Together with "mpx.s" it forms the lowest layer of the MINIX kernel.
 * There is one entry point from the outside:
 *
 *   sys_call: 	      a system call, i.e., the kernel is trapped with an INT
 *
 * As well as several entry points used from the interrupt and task level:
 *
 *   lock_send:	      send a message to a process
 *
 * Changes:
 *   Aug 19, 2005     rewrote scheduling code  (Jorrit N. Herder)
 *   Jul 25, 2005     rewrote system call handling  (Jorrit N. Herder)
 *   May 26, 2005     rewrote message passing functions  (Jorrit N. Herder)
 *   May 24, 2005     new notification system call  (Jorrit N. Herder)
 *   Oct 28, 2004     nonblocking send and receive calls  (Jorrit N. Herder)
 *
 * The code here is critical to make everything work and is important for the
 * overall performance of the system. A large fraction of the code deals with
 * list manipulation. To make this both easy to understand and fast to execute 
 * pointer pointers are used throughout the code. Pointer pointers prevent
 * exceptions for the head or tail of a linked list. 
 *
 *  node_t *queue, *new_node;	// assume these as global variables
 *  node_t **xpp = &queue; 	// get pointer pointer to head of queue 
 *  while (*xpp != NULL) 	// find last pointer of the linked list
 *      xpp = &(*xpp)->next;	// get pointer to next pointer 
 *  *xpp = new_node;		// now replace the end (the NULL pointer) 
 *  new_node->next = NULL;	// and mark the new end of the list
 * 
 * For example, when adding a new node to the end of the list, one normally 
 * makes an exception for an empty list and looks up the end of the list for 
 * nonempty lists. As shown above, this is not required with pointer pointers.
 */

#include <minix/com.h>
#include <minix/endpoint.h>
#include <stddef.h>
#include <signal.h>
#include <minix/portio.h>
#include <minix/syslib.h>

#include "debug.h"
#include "kernel.h"
#include "proc.h"
#include "vm.h"

/* Scheduling and message passing functions. The functions are available to 
 * other parts of the kernel through lock_...(). The lock temporarily disables 
 * interrupts to prevent race conditions. 
 */
FORWARD _PROTOTYPE( void idle, (void));
FORWARD _PROTOTYPE( int mini_send, (struct proc *caller_ptr, int dst_e,
		message *m_ptr, int flags));
FORWARD _PROTOTYPE( int mini_receive, (struct proc *caller_ptr, int src,
		message *m_ptr, int flags));
FORWARD _PROTOTYPE( int mini_senda, (struct proc *caller_ptr,
	asynmsg_t *table, size_t size));
FORWARD _PROTOTYPE( int deadlock, (int function,
		register struct proc *caller, int src_dst));
FORWARD _PROTOTYPE( int try_async, (struct proc *caller_ptr));
FORWARD _PROTOTYPE( int try_one, (struct proc *src_ptr, struct proc *dst_ptr,
		int *postponed));
FORWARD _PROTOTYPE( void sched, (struct proc *rp, int *queue, int *front));
FORWARD _PROTOTYPE( struct proc * pick_proc, (void));
FORWARD _PROTOTYPE( void enqueue_head, (struct proc *rp));

#define PICK_ANY	1
#define PICK_HIGHERONLY	2

#define BuildNotifyMessage(m_ptr, src, dst_ptr) \
	(m_ptr)->m_type = NOTIFY_FROM(src);				\
	(m_ptr)->NOTIFY_TIMESTAMP = get_uptime();			\
	switch (src) {							\
	case HARDWARE:							\
		(m_ptr)->NOTIFY_ARG = priv(dst_ptr)->s_int_pending;	\
		priv(dst_ptr)->s_int_pending = 0;			\
		break;							\
	case SYSTEM:							\
		(m_ptr)->NOTIFY_ARG = priv(dst_ptr)->s_sig_pending;	\
		priv(dst_ptr)->s_sig_pending = 0;			\
		break;							\
	}

/*===========================================================================*
 *				QueueMess				     * 
 *===========================================================================*/
PRIVATE int QueueMess(endpoint_t ep, vir_bytes msg_lin, struct proc *dst)
{
	int k;
	phys_bytes addr;
	NOREC_ENTER(queuemess);
	/* Queue a message from the src process (in memory) to the dst
	 * process (using dst process table entry). Do actual copy to
	 * kernel here; it's an error if the copy fails into kernel.
	 */
	vmassert(!(dst->p_misc_flags & MF_DELIVERMSG));	
	vmassert(dst->p_delivermsg_lin);
	vmassert(isokendpt(ep, &k));

#if 0
	if(INMEMORY(dst)) {
		PHYS_COPY_CATCH(msg_lin, dst->p_delivermsg_lin,
			sizeof(message), addr);
		if(!addr) {
			PHYS_COPY_CATCH(vir2phys(&ep), dst->p_delivermsg_lin,
				sizeof(ep), addr);
			if(!addr) {
				NOREC_RETURN(queuemess, OK);
			}
		}
	}
#endif

	PHYS_COPY_CATCH(msg_lin, vir2phys(&dst->p_delivermsg), sizeof(message), addr);
	if(addr) {
		NOREC_RETURN(queuemess, EFAULT);
	}

	dst->p_delivermsg.m_source = ep;
	dst->p_misc_flags |= MF_DELIVERMSG;

	NOREC_RETURN(queuemess, OK);
}

/*===========================================================================*
 *				idle					     * 
 *===========================================================================*/
PRIVATE void idle()
{
	/* This function is called whenever there is no work to do.
	 * Halt the CPU, and measure how many timestamp counter ticks are
	 * spent not doing anything. This allows test setups to measure
	 * the CPU utiliziation of certain workloads with high precision.
	 */
#ifdef CONFIG_IDLE_TSC
	u64_t idle_start;

	read_tsc_64(&idle_start);
	idle_active = 1;
#endif

	halt_cpu();

#ifdef CONFIG_IDLE_TSC
	if (idle_active) {
		IDLE_STOP;
		printf("Kernel: idle active after resuming CPU\n");
	}

	idle_tsc = add64(idle_tsc, sub64(idle_stop, idle_start));
#endif
}

/*===========================================================================*
 *				schedcheck				     * 
 *===========================================================================*/
PUBLIC struct proc * schedcheck(void)
{
	/* This function is called an instant before proc_ptr is
	 * to be scheduled again.
	 */
  	NOREC_ENTER(schedch);
	vmassert(intr_disabled());

	/*
	 * if the current process is still runnable check the misc flags and let
	 * it run unless it becomes not runnable in the meantime
	 */
	if (proc_is_runnable(proc_ptr))
		goto check_misc_flags;

	/*
	 * if a process becomes not runnable while handling the misc flags, we
	 * need to pick a new one here and start from scratch. Also if the
	 * current process wasn' runnable, we pick a new one here
	 */
not_runnable_pick_new:
	if (proc_is_preempted(proc_ptr)) {
		proc_ptr->p_rts_flags &= ~RTS_PREEMPTED;
		if (proc_is_runnable(proc_ptr))
			enqueue_head(proc_ptr);
	}
	/* this enqueues the process again */
	if (proc_no_quantum(proc_ptr))
		RTS_UNSET(proc_ptr, RTS_NO_QUANTUM);

	/*
	 * if we have no process to run, set IDLE as the current process for
	 * time accounting and put the cpu in and idle state. After the next
	 * timer interrupt the execution resumes here and we can pick another
	 * process. If there is still nothing runnable we "schedule" IDLE again
	 */
	while (!(proc_ptr = pick_proc())) {
		proc_ptr = proc_addr(IDLE);
		if (priv(proc_ptr)->s_flags & BILLABLE)
			bill_ptr = proc_ptr;
		idle();
	}

check_misc_flags:

	vmassert(proc_ptr);
	vmassert(proc_is_runnable(proc_ptr));
	while (proc_ptr->p_misc_flags &
		(MF_DELIVERMSG | MF_SC_DEFER | MF_SC_TRACE | MF_SC_ACTIVE)) {

		vmassert(proc_is_runnable(proc_ptr));
		if (proc_ptr->p_misc_flags & MF_DELIVERMSG) {
			TRACE(VF_SCHEDULING, printf("delivering to %s / %d\n",
				proc_ptr->p_name, proc_ptr->p_endpoint););
			if(delivermsg(proc_ptr) == VMSUSPEND) {
				TRACE(VF_SCHEDULING,
					printf("suspending %s / %d\n",
					proc_ptr->p_name,
					proc_ptr->p_endpoint););
				vmassert(!proc_is_runnable(proc_ptr));
			}
		}
		else if (proc_ptr->p_misc_flags & MF_SC_DEFER) {
			/* Perform the system call that we deferred earlier. */

#if DEBUG_SCHED_CHECK
			if (proc_ptr->p_misc_flags & MF_SC_ACTIVE)
				minix_panic("MF_SC_ACTIVE and MF_SC_DEFER set",
					NO_NUM);
#endif

			arch_do_syscall(proc_ptr);

			/* If the process is stopped for signal delivery, and
			 * not blocked sending a message after the system call,
			 * inform PM.
			 */
			if ((proc_ptr->p_misc_flags & MF_SIG_DELAY) &&
					!RTS_ISSET(proc_ptr, RTS_SENDING))
				sig_delay_done(proc_ptr);
		}
		else if (proc_ptr->p_misc_flags & MF_SC_TRACE) {
			/* Trigger a system call leave event if this was a
			 * system call. We must do this after processing the
			 * other flags above, both for tracing correctness and
			 * to be able to use 'break'.
			 */
			if (!(proc_ptr->p_misc_flags & MF_SC_ACTIVE))
				break;

			proc_ptr->p_misc_flags &=
				~(MF_SC_TRACE | MF_SC_ACTIVE);

			/* Signal the "leave system call" event.
			 * Block the process.
			 */
			cause_sig(proc_nr(proc_ptr), SIGTRAP);
		}
		else if (proc_ptr->p_misc_flags & MF_SC_ACTIVE) {
			/* If MF_SC_ACTIVE was set, remove it now:
			 * we're leaving the system call.
			 */
			proc_ptr->p_misc_flags &= ~MF_SC_ACTIVE;

			break;
		}

		/*
		 * the selected process might not be runnable anymore. We have
		 * to checkit and schedule another one
		 */
		if (!proc_is_runnable(proc_ptr))
			goto not_runnable_pick_new;
	}
	TRACE(VF_SCHEDULING, printf("starting %s / %d\n",
		proc_ptr->p_name, proc_ptr->p_endpoint););
#if DEBUG_TRACE
	proc_ptr->p_schedules++;
#endif

	proc_ptr = arch_finish_schedcheck();

	NOREC_RETURN(schedch, proc_ptr);
}

/*===========================================================================*
 *				sys_call				     * 
 *===========================================================================*/
PUBLIC int sys_call(call_nr, src_dst_e, m_ptr, bit_map)
int call_nr;			/* system call number and flags */
int src_dst_e;			/* src to receive from or dst to send to */
message *m_ptr;			/* pointer to message in the caller's space */
long bit_map;			/* notification event set or flags */
{
/* System calls are done by trapping to the kernel with an INT instruction.
 * The trap is caught and sys_call() is called to send or receive a message
 * (or both). The caller is always given by 'proc_ptr'.
 */
  register struct proc *caller_ptr = proc_ptr;	/* get pointer to caller */
  int result;					/* the system call's result */
  int src_dst_p;				/* Process slot number */
  size_t msg_size;

  /* If this process is subject to system call tracing, handle that first. */
  if (caller_ptr->p_misc_flags & (MF_SC_TRACE | MF_SC_DEFER)) {
	/* Are we tracing this process, and is it the first sys_call entry? */
	if ((caller_ptr->p_misc_flags & (MF_SC_TRACE | MF_SC_DEFER)) ==
							MF_SC_TRACE) {
		/* We must notify the tracer before processing the actual
		 * system call. If we don't, the tracer could not obtain the
		 * input message. Postpone the entire system call.
		 */
		caller_ptr->p_misc_flags &= ~MF_SC_TRACE;
		caller_ptr->p_misc_flags |= MF_SC_DEFER;

		/* Signal the "enter system call" event. Block the process. */
		cause_sig(proc_nr(caller_ptr), SIGTRAP);

		/* Preserve the return register's value. */
		return caller_ptr->p_reg.retreg;
	}

	/* If the MF_SC_DEFER flag is set, the syscall is now being resumed. */
	caller_ptr->p_misc_flags &= ~MF_SC_DEFER;

#if DEBUG_SCHED_CHECK
	if (caller_ptr->p_misc_flags & MF_SC_ACTIVE)
		minix_panic("MF_SC_ACTIVE already set", NO_NUM);
#endif

	/* Set a flag to allow reliable tracing of leaving the system call. */
	caller_ptr->p_misc_flags |= MF_SC_ACTIVE;
  }

#if DEBUG_SCHED_CHECK
  if(caller_ptr->p_misc_flags & MF_DELIVERMSG) {
	kprintf("sys_call: MF_DELIVERMSG on for %s / %d\n",
		caller_ptr->p_name, caller_ptr->p_endpoint);
	minix_panic("MF_DELIVERMSG on", NO_NUM);
  }
#endif

#if 0
  if(src_dst_e != 4 && src_dst_e != 5 &&
	caller_ptr->p_endpoint != 4 && caller_ptr->p_endpoint != 5) {
	if(call_nr == SEND)
		kprintf("(%d SEND to %d) ", caller_ptr->p_endpoint, src_dst_e);
	else if(call_nr == RECEIVE)
		kprintf("(%d RECEIVE from %d) ", caller_ptr->p_endpoint, src_dst_e);
	else if(call_nr == SENDREC)
		kprintf("(%d SENDREC to %d) ", caller_ptr->p_endpoint, src_dst_e);
	else
		kprintf("(%d %d to/from %d) ", caller_ptr->p_endpoint, call_nr, src_dst_e);
  }
#endif

#if DEBUG_SCHED_CHECK
  if (RTS_ISSET(caller_ptr, RTS_SLOT_FREE))
  {
	kprintf("called by the dead?!?\n");
	return EINVAL;
  }
#endif

  /* Check destination. SENDA is special because its argument is a table and
   * not a single destination. RECEIVE is the only call that accepts ANY (in
   * addition to a real endpoint). The other calls (SEND, SENDREC,
   * and NOTIFY) require an endpoint to corresponds to a process. In addition,
   * it is necessary to check whether a process is allowed to send to a given
   * destination.
   */
  if (call_nr == SENDA)
  {
	/* No destination argument */
  }
  else if (src_dst_e == ANY)
  {
	if (call_nr != RECEIVE)
	{
#if 0
		kprintf("sys_call: trap %d by %d with bad endpoint %d\n", 
			call_nr, proc_nr(caller_ptr), src_dst_e);
#endif
		return EINVAL;
	}
	src_dst_p = src_dst_e;
  }
  else
  {
	/* Require a valid source and/or destination process. */
	if(!isokendpt(src_dst_e, &src_dst_p)) {
#if 0
		kprintf("sys_call: trap %d by %d with bad endpoint %d\n", 
			call_nr, proc_nr(caller_ptr), src_dst_e);
#endif
		return EDEADSRCDST;
	}

	/* If the call is to send to a process, i.e., for SEND, SENDNB,
	 * SENDREC or NOTIFY, verify that the caller is allowed to send to
	 * the given destination. 
	 */
	if (call_nr != RECEIVE)
	{
		if (!may_send_to(caller_ptr, src_dst_p)) {
#if DEBUG_ENABLE_IPC_WARNINGS
			kprintf(
			"sys_call: ipc mask denied trap %d from %d to %d\n",
				call_nr, caller_ptr->p_endpoint, src_dst_e);
#endif
			return(ECALLDENIED);	/* call denied by ipc mask */
		}
	}
  }

  /* Only allow non-negative call_nr values less than 32 */
  if (call_nr < 0 || call_nr >= 32)
  {
#if DEBUG_ENABLE_IPC_WARNINGS
      kprintf("sys_call: trap %d not allowed, caller %d, src_dst %d\n", 
          call_nr, proc_nr(caller_ptr), src_dst_p);
#endif
	return(ETRAPDENIED);		/* trap denied by mask or kernel */
  }

  /* Check if the process has privileges for the requested call. Calls to the 
   * kernel may only be SENDREC, because tasks always reply and may not block 
   * if the caller doesn't do receive(). 
   */
  if (!(priv(caller_ptr)->s_trap_mask & (1 << call_nr))) {
#if DEBUG_ENABLE_IPC_WARNINGS
      kprintf("sys_call: trap %d not allowed, caller %d, src_dst %d\n", 
          call_nr, proc_nr(caller_ptr), src_dst_p);
#endif
	return(ETRAPDENIED);		/* trap denied by mask or kernel */
  }

  /* SENDA has no src_dst value here, so this check is in mini_senda() as well.
   */
  if (call_nr != SENDREC && call_nr != RECEIVE && call_nr != SENDA &&
	iskerneln(src_dst_p)) {
#if DEBUG_ENABLE_IPC_WARNINGS
      kprintf("sys_call: trap %d not allowed, caller %d, src_dst %d\n", 
          call_nr, proc_nr(caller_ptr), src_dst_e);
#endif
	return(ETRAPDENIED);		/* trap denied by mask or kernel */
  }

  /* Get and check the size of the argument in bytes.
   * Normally this is just the size of a regular message, but in the
   * case of SENDA the argument is a table.
   */
  if(call_nr == SENDA) {
	msg_size = (size_t) src_dst_e;

	/* Limit size to something reasonable. An arbitrary choice is 16
	 * times the number of process table entries.
	 */
	if (msg_size > 16*(NR_TASKS + NR_PROCS))
		return EDOM;
	msg_size *= sizeof(asynmsg_t);	/* convert to bytes */
  } else {
	msg_size = sizeof(*m_ptr);
  }

  /* Now check if the call is known and try to perform the request. The only
   * system calls that exist in MINIX are sending and receiving messages.
   *   - SENDREC: combines SEND and RECEIVE in a single system call
   *   - SEND:    sender blocks until its message has been delivered
   *   - RECEIVE: receiver blocks until an acceptable message has arrived
   *   - NOTIFY:  asynchronous call; deliver notification or mark pending
   *   - SENDA:   list of asynchronous send requests
   */
  switch(call_nr) {
  case SENDREC:
	/* A flag is set so that notifications cannot interrupt SENDREC. */
	caller_ptr->p_misc_flags |= MF_REPLY_PEND;
	/* fall through */
  case SEND:			
	result = mini_send(caller_ptr, src_dst_e, m_ptr, 0);
	if (call_nr == SEND || result != OK)
		break;				/* done, or SEND failed */
	/* fall through for SENDREC */
  case RECEIVE:			
	if (call_nr == RECEIVE)
		caller_ptr->p_misc_flags &= ~MF_REPLY_PEND;
	result = mini_receive(caller_ptr, src_dst_e, m_ptr, 0);
	break;
  case NOTIFY:
	result = mini_notify(caller_ptr, src_dst_e);
	break;
  case SENDNB:
        result = mini_send(caller_ptr, src_dst_e, m_ptr, NON_BLOCKING);
        break;
  case SENDA:
	result = mini_senda(caller_ptr, (asynmsg_t *)m_ptr, (size_t)src_dst_e);
	break;
  default:
	result = EBADCALL;			/* illegal system call */
  }

  /* Now, return the result of the system call to the caller. */
  return(result);
}

/*===========================================================================*
 *				deadlock				     * 
 *===========================================================================*/
PRIVATE int deadlock(function, cp, src_dst) 
int function;					/* trap number */
register struct proc *cp;			/* pointer to caller */
proc_nr_t src_dst;				/* src or dst process */
{
/* Check for deadlock. This can happen if 'caller_ptr' and 'src_dst' have
 * a cyclic dependency of blocking send and receive calls. The only cyclic 
 * depency that is not fatal is if the caller and target directly SEND(REC)
 * and RECEIVE to each other. If a deadlock is found, the group size is 
 * returned. Otherwise zero is returned. 
 */
  register struct proc *xp;			/* process pointer */
  int group_size = 1;				/* start with only caller */
#if DEBUG_ENABLE_IPC_WARNINGS
  static struct proc *processes[NR_PROCS + NR_TASKS];
  processes[0] = cp;
#endif

  while (src_dst != ANY) { 			/* check while process nr */
      xp = proc_addr(src_dst);			/* follow chain of processes */
#if DEBUG_ENABLE_IPC_WARNINGS
      processes[group_size] = xp;
#endif
      group_size ++;				/* extra process in group */

      /* Check whether the last process in the chain has a dependency. If it 
       * has not, the cycle cannot be closed and we are done.
       */
      if (RTS_ISSET(xp, RTS_RECEIVING)) {	/* xp has dependency */
	  if(xp->p_getfrom_e == ANY) src_dst = ANY;
	  else okendpt(xp->p_getfrom_e, &src_dst);
      } else if (RTS_ISSET(xp, RTS_SENDING)) {	/* xp has dependency */
	  okendpt(xp->p_sendto_e, &src_dst);
      } else {
	  return(0);				/* not a deadlock */
      }

      /* Now check if there is a cyclic dependency. For group sizes of two,  
       * a combination of SEND(REC) and RECEIVE is not fatal. Larger groups
       * or other combinations indicate a deadlock.  
       */
      if (src_dst == proc_nr(cp)) {		/* possible deadlock */
	  if (group_size == 2) {		/* caller and src_dst */
	      /* The function number is magically converted to flags. */
	      if ((xp->p_rts_flags ^ (function << 2)) & RTS_SENDING) { 
	          return(0);			/* not a deadlock */
	      }
	  }
#if DEBUG_ENABLE_IPC_WARNINGS
	  {
		int i;
		kprintf("deadlock between these processes:\n");
		for(i = 0; i < group_size; i++) {
			kprintf(" %10s ", processes[i]->p_name);
			proc_stacktrace(processes[i]);
		}
	  }
#endif
          return(group_size);			/* deadlock found */
      }
  }
  return(0);					/* not a deadlock */
}

/*===========================================================================*
 *				mini_send				     * 
 *===========================================================================*/
PRIVATE int mini_send(caller_ptr, dst_e, m_ptr, flags)
register struct proc *caller_ptr;	/* who is trying to send a message? */
int dst_e;				/* to whom is message being sent? */
message *m_ptr;				/* pointer to message buffer */
int flags;
{
/* Send a message from 'caller_ptr' to 'dst'. If 'dst' is blocked waiting
 * for this message, copy the message to it and unblock 'dst'. If 'dst' is
 * not waiting at all, or is waiting for another source, queue 'caller_ptr'.
 */
  register struct proc *dst_ptr;
  register struct proc **xpp;
  int dst_p;
  phys_bytes linaddr;
  vir_bytes addr;
  int r;

  if(!(linaddr = umap_local(caller_ptr, D, (vir_bytes) m_ptr,
	sizeof(message)))) {
	return EFAULT;
  }
  dst_p = _ENDPOINT_P(dst_e);
  dst_ptr = proc_addr(dst_p);

  if (RTS_ISSET(dst_ptr, RTS_NO_ENDPOINT))
  {
	return EDSTDIED;
  }

  /* Check if 'dst' is blocked waiting for this message. The destination's 
   * RTS_SENDING flag may be set when its SENDREC call blocked while sending.  
   */
  if (WILLRECEIVE(dst_ptr, caller_ptr->p_endpoint)) {
	/* Destination is indeed waiting for this message. */
	vmassert(!(dst_ptr->p_misc_flags & MF_DELIVERMSG));	
	if((r=QueueMess(caller_ptr->p_endpoint, linaddr, dst_ptr)) != OK)
		return r;
	RTS_UNSET(dst_ptr, RTS_RECEIVING);
  } else {
	if(flags & NON_BLOCKING) {
		return(ENOTREADY);
	}

	/* Check for a possible deadlock before actually blocking. */
	if (deadlock(SEND, caller_ptr, dst_p)) {
		return(ELOCKED);
	}

	/* Destination is not waiting.  Block and dequeue caller. */
	PHYS_COPY_CATCH(linaddr, vir2phys(&caller_ptr->p_sendmsg),
		sizeof(message), addr);

	if(addr) { return EFAULT; }
	RTS_SET(caller_ptr, RTS_SENDING);
	caller_ptr->p_sendto_e = dst_e;

	/* Process is now blocked.  Put in on the destination's queue. */
	xpp = &dst_ptr->p_caller_q;		/* find end of list */
	while (*xpp != NIL_PROC) xpp = &(*xpp)->p_q_link;	
	*xpp = caller_ptr;			/* add caller to end */
	caller_ptr->p_q_link = NIL_PROC;	/* mark new end of list */
  }
  return(OK);
}

/*===========================================================================*
 *				mini_receive				     * 
 *===========================================================================*/
PRIVATE int mini_receive(caller_ptr, src_e, m_ptr, flags)
register struct proc *caller_ptr;	/* process trying to get message */
int src_e;				/* which message source is wanted */
message *m_ptr;				/* pointer to message buffer */
int flags;
{
/* A process or task wants to get a message.  If a message is already queued,
 * acquire it and deblock the sender.  If no message from the desired source
 * is available block the caller.
 */
  register struct proc **xpp;
  message m;
  sys_map_t *map;
  bitchunk_t *chunk;
  int i, r, src_id, src_proc_nr, src_p;
  phys_bytes linaddr;

  vmassert(!(caller_ptr->p_misc_flags & MF_DELIVERMSG));

  if(!(linaddr = umap_local(caller_ptr, D, (vir_bytes) m_ptr,
	sizeof(message)))) {
	return EFAULT;
  }

  /* This is where we want our message. */
  caller_ptr->p_delivermsg_lin = linaddr;
  caller_ptr->p_delivermsg_vir = (vir_bytes) m_ptr;

  if(src_e == ANY) src_p = ANY;
  else
  {
	okendpt(src_e, &src_p);
	if (RTS_ISSET(proc_addr(src_p), RTS_NO_ENDPOINT))
	{
		return ESRCDIED;
	}
  }


  /* Check to see if a message from desired source is already available.  The
   * caller's RTS_SENDING flag may be set if SENDREC couldn't send. If it is
   * set, the process should be blocked.
   */
  if (!RTS_ISSET(caller_ptr, RTS_SENDING)) {

    /* Check if there are pending notifications, except for SENDREC. */
    if (! (caller_ptr->p_misc_flags & MF_REPLY_PEND)) {

        map = &priv(caller_ptr)->s_notify_pending;
        for (chunk=&map->chunk[0]; chunk<&map->chunk[NR_SYS_CHUNKS]; chunk++) {
		endpoint_t hisep;

            /* Find a pending notification from the requested source. */ 
            if (! *chunk) continue; 			/* no bits in chunk */
            for (i=0; ! (*chunk & (1<<i)); ++i) {} 	/* look up the bit */
            src_id = (chunk - &map->chunk[0]) * BITCHUNK_BITS + i;
            if (src_id >= NR_SYS_PROCS) break;		/* out of range */
            src_proc_nr = id_to_nr(src_id);		/* get source proc */
#if DEBUG_ENABLE_IPC_WARNINGS
	    if(src_proc_nr == NONE) {
		kprintf("mini_receive: sending notify from NONE\n");
	    }
#endif
            if (src_e!=ANY && src_p != src_proc_nr) continue;/* source not ok */
            *chunk &= ~(1 << i);			/* no longer pending */

            /* Found a suitable source, deliver the notification message. */
	    BuildNotifyMessage(&m, src_proc_nr, caller_ptr);	/* assemble message */
	    hisep = proc_addr(src_proc_nr)->p_endpoint;
	    vmassert(!(caller_ptr->p_misc_flags & MF_DELIVERMSG));	
	    vmassert(src_e == ANY || hisep == src_e);
	    if((r=QueueMess(hisep, vir2phys(&m), caller_ptr)) != OK)  {
		minix_panic("mini_receive: local QueueMess failed", NO_NUM);
	    }
            return(OK);					/* report success */
        }
    }

    /* Check caller queue. Use pointer pointers to keep code simple. */
    xpp = &caller_ptr->p_caller_q;
    while (*xpp != NIL_PROC) {
        if (src_e == ANY || src_p == proc_nr(*xpp)) {
#if DEBUG_SCHED_CHECK
	    if (RTS_ISSET(*xpp, RTS_SLOT_FREE) || RTS_ISSET(*xpp, RTS_NO_ENDPOINT))
	    {
		kprintf("%d: receive from %d; found dead %d (%s)?\n",
			caller_ptr->p_endpoint, src_e, (*xpp)->p_endpoint,
			(*xpp)->p_name);
		return EINVAL;
	    }
#endif

	    /* Found acceptable message. Copy it and update status. */
  	    vmassert(!(caller_ptr->p_misc_flags & MF_DELIVERMSG));
	    QueueMess((*xpp)->p_endpoint,
		vir2phys(&(*xpp)->p_sendmsg), caller_ptr);
	    if ((*xpp)->p_misc_flags & MF_SIG_DELAY)
		sig_delay_done(*xpp);
	    RTS_UNSET(*xpp, RTS_SENDING);
            *xpp = (*xpp)->p_q_link;		/* remove from queue */
            return(OK);				/* report success */
	}
	xpp = &(*xpp)->p_q_link;		/* proceed to next */
    }

    if (caller_ptr->p_misc_flags & MF_ASYNMSG)
    {
	if (src_e != ANY)
		r= try_one(proc_addr(src_p), caller_ptr, NULL);
	else
		r= try_async(caller_ptr);

	if (r == OK)
		return OK;	/* Got a message */
    }
  }

  /* No suitable message is available or the caller couldn't send in SENDREC. 
   * Block the process trying to receive, unless the flags tell otherwise.
   */
  if ( ! (flags & NON_BLOCKING)) {
      /* Check for a possible deadlock before actually blocking. */
      if (deadlock(RECEIVE, caller_ptr, src_p)) {
          return(ELOCKED);
      }

      caller_ptr->p_getfrom_e = src_e;		
      RTS_SET(caller_ptr, RTS_RECEIVING);
      return(OK);
  } else {
	return(ENOTREADY);
  }
}

/*===========================================================================*
 *				mini_notify				     * 
 *===========================================================================*/
PUBLIC int mini_notify(caller_ptr, dst_e)
register struct proc *caller_ptr;	/* sender of the notification */
endpoint_t dst_e;			/* which process to notify */
{
  register struct proc *dst_ptr;
  int src_id;				/* source id for late delivery */
  message m;				/* the notification message */
  int r;
  int dst_p;

  vmassert(intr_disabled());

  if (!isokendpt(dst_e, &dst_p)) {
	util_stacktrace();
	kprintf("mini_notify: bogus endpoint %d\n", dst_e);
	return EDEADSRCDST;
  }

  dst_ptr = proc_addr(dst_p);

  /* Check to see if target is blocked waiting for this message. A process 
   * can be both sending and receiving during a SENDREC system call.
   */
    if (WILLRECEIVE(dst_ptr, caller_ptr->p_endpoint) &&
      ! (dst_ptr->p_misc_flags & MF_REPLY_PEND)) {
      /* Destination is indeed waiting for a message. Assemble a notification 
       * message and deliver it. Copy from pseudo-source HARDWARE, since the
       * message is in the kernel's address space.
       */ 
      BuildNotifyMessage(&m, proc_nr(caller_ptr), dst_ptr);
      vmassert(!(dst_ptr->p_misc_flags & MF_DELIVERMSG));
      if((r=QueueMess(caller_ptr->p_endpoint, vir2phys(&m), dst_ptr)) != OK) {
	minix_panic("mini_notify: local QueueMess failed", NO_NUM);
      }
      RTS_UNSET(dst_ptr, RTS_RECEIVING);
      return(OK);
  } 

  /* Destination is not ready to receive the notification. Add it to the 
   * bit map with pending notifications. Note the indirectness: the privilege id
   * instead of the process number is used in the pending bit map.
   */ 
  src_id = priv(caller_ptr)->s_id;
  set_sys_bit(priv(dst_ptr)->s_notify_pending, src_id); 
  return(OK);
}

#define ASCOMPLAIN(caller, entry, field)	\
	kprintf("kernel:%s:%d: asyn failed for %s in %s "	\
	"(%d/%d, tab 0x%lx)\n",__FILE__,__LINE__,	\
field, caller->p_name, entry, priv(caller)->s_asynsize, priv(caller)->s_asyntab)

#define A_RETRIEVE(entry, field)	\
  if(data_copy(caller_ptr->p_endpoint,	\
	 table_v + (entry)*sizeof(asynmsg_t) + offsetof(struct asynmsg,field),\
		SYSTEM, (vir_bytes) &tabent.field,	\
			sizeof(tabent.field)) != OK) {\
		ASCOMPLAIN(caller_ptr, entry, #field);	\
		return EFAULT; \
	}

#define A_INSERT(entry, field)	\
  if(data_copy(SYSTEM, (vir_bytes) &tabent.field, \
	caller_ptr->p_endpoint,	\
 	table_v + (entry)*sizeof(asynmsg_t) + offsetof(struct asynmsg,field),\
		sizeof(tabent.field)) != OK) {\
		ASCOMPLAIN(caller_ptr, entry, #field);	\
		return EFAULT; \
	}

/*===========================================================================*
 *				mini_senda				     *
 *===========================================================================*/
PRIVATE int mini_senda(struct proc *caller_ptr, asynmsg_t *table, size_t size)
{
	int i, dst_p, done, do_notify;
	unsigned flags;
	struct proc *dst_ptr;
	struct priv *privp;
	asynmsg_t tabent;
	vir_bytes table_v = (vir_bytes) table;
	vir_bytes linaddr;

	privp= priv(caller_ptr);
	if (!(privp->s_flags & SYS_PROC))
	{
		kprintf(
		"mini_senda: warning caller has no privilege structure\n");
		return EPERM;
	}

	/* Clear table */
	privp->s_asyntab= -1;	
	privp->s_asynsize= 0;

	if (size == 0)
	{
		/* Nothing to do, just return */
		return OK;
	}

	if(!(linaddr = umap_local(caller_ptr, D, (vir_bytes) table,
		size * sizeof(*table)))) {
		printf("mini_senda: umap_local failed; 0x%lx len 0x%lx\n",
			table, size * sizeof(*table));
		return EFAULT;
	}

	/* Limit size to something reasonable. An arbitrary choice is 16
	 * times the number of process table entries.
	 *
	 * (this check has been duplicated in sys_call but is left here
	 * as a sanity check)
	 */
	if (size > 16*(NR_TASKS + NR_PROCS))
	{
		return EDOM;
	}
	
	/* Scan the table */
	do_notify= FALSE;	
	done= TRUE;
	for (i= 0; i<size; i++)
	{

		/* Read status word */
		A_RETRIEVE(i, flags);
		flags= tabent.flags;

		/* Skip empty entries */
		if (flags == 0)
			continue;

		/* Check for reserved bits in the flags field */
		if (flags & ~(AMF_VALID|AMF_DONE|AMF_NOTIFY|AMF_NOREPLY) ||
			!(flags & AMF_VALID))
		{
			return EINVAL;
		}

		/* Skip entry if AMF_DONE is already set */
		if (flags & AMF_DONE)
			continue;

		/* Get destination */
		A_RETRIEVE(i, dst);

		if (!isokendpt(tabent.dst, &dst_p))
		{
			/* Bad destination, report the error */
			tabent.result= EDEADSRCDST;
			A_INSERT(i, result);
			tabent.flags= flags | AMF_DONE;
			A_INSERT(i, flags);

			if (flags & AMF_NOTIFY)
				do_notify= 1;
			continue;
		}

		if (iskerneln(dst_p))
		{
			/* Asynchronous sends to the kernel are not allowed */
			tabent.result= ECALLDENIED;
			A_INSERT(i, result);
			tabent.flags= flags | AMF_DONE;
			A_INSERT(i, flags);

			if (flags & AMF_NOTIFY)
				do_notify= 1;
			continue;
		}

		if (!may_send_to(caller_ptr, dst_p))
		{
			/* Send denied by IPC mask */
			tabent.result= ECALLDENIED;
			A_INSERT(i, result);
			tabent.flags= flags | AMF_DONE;
			A_INSERT(i, flags);

			if (flags & AMF_NOTIFY)
				do_notify= 1;
			continue;
		}

#if 0
		kprintf("mini_senda: entry[%d]: flags 0x%x dst %d/%d\n",
			i, tabent.flags, tabent.dst, dst_p);
#endif

		dst_ptr = proc_addr(dst_p);

		/* RTS_NO_ENDPOINT should be removed */
		if (dst_ptr->p_rts_flags & RTS_NO_ENDPOINT)
		{
			tabent.result= EDSTDIED;
			A_INSERT(i, result);
			tabent.flags= flags | AMF_DONE;
			A_INSERT(i, flags);

			if (flags & AMF_NOTIFY)
				do_notify= TRUE;
			continue;
		}

		/* Check if 'dst' is blocked waiting for this message.
		 * If AMF_NOREPLY is set, do not satisfy the receiving part of
		 * a SENDREC.
		 */
		if (WILLRECEIVE(dst_ptr, caller_ptr->p_endpoint) &&
			(!(flags & AMF_NOREPLY) ||
			!(dst_ptr->p_misc_flags & MF_REPLY_PEND)))
		{
			/* Destination is indeed waiting for this message. */
			/* Copy message from sender. */
			tabent.result= QueueMess(caller_ptr->p_endpoint,
				linaddr + (vir_bytes) &table[i].msg -
					(vir_bytes) table, dst_ptr);
			if(tabent.result == OK)
				RTS_UNSET(dst_ptr, RTS_RECEIVING);

			A_INSERT(i, result);
			tabent.flags= flags | AMF_DONE;
			A_INSERT(i, flags);

			if (flags & AMF_NOTIFY)
				do_notify= 1;
			continue;
		}
		else 
		{
			/* Should inform receiver that something is pending */
			dst_ptr->p_misc_flags |= MF_ASYNMSG;
			done= FALSE;
			continue;
		} 
	}
	if (do_notify)
		kprintf("mini_senda: should notify caller\n");
	if (!done)
	{
		privp->s_asyntab= (vir_bytes)table;
		privp->s_asynsize= size;
	}
	return OK;
}


/*===========================================================================*
 *				try_async				     * 
 *===========================================================================*/
PRIVATE int try_async(caller_ptr)
struct proc *caller_ptr;
{
	int r;
	struct priv *privp;
	struct proc *src_ptr;
	int postponed = FALSE;

	/* Try all privilege structures */
	for (privp = BEG_PRIV_ADDR; privp < END_PRIV_ADDR; ++privp) 
	{
		if (privp->s_proc_nr == NONE)
			continue;

		src_ptr= proc_addr(privp->s_proc_nr);

	  	vmassert(!(caller_ptr->p_misc_flags & MF_DELIVERMSG));
		r= try_one(src_ptr, caller_ptr, &postponed);
		if (r == OK)
			return r;
	}

	/* Nothing found, clear MF_ASYNMSG unless messages were postponed */
	if (postponed == FALSE)
		caller_ptr->p_misc_flags &= ~MF_ASYNMSG;

	return ESRCH;
}


/*===========================================================================*
 *				try_one					     *
 *===========================================================================*/
PRIVATE int try_one(struct proc *src_ptr, struct proc *dst_ptr, int *postponed)
{
	int i, do_notify, done;
	unsigned flags;
	size_t size;
	endpoint_t dst_e;
	struct priv *privp;
	asynmsg_t tabent;
	vir_bytes table_v;
	struct proc *caller_ptr;
	int r;

	privp= priv(src_ptr);

	/* Basic validity checks */
	if (privp->s_id == USER_PRIV_ID) return EAGAIN;
	if (privp->s_asynsize == 0) return EAGAIN;
	if (!may_send_to(src_ptr, proc_nr(dst_ptr))) return EAGAIN;

	size= privp->s_asynsize;
	table_v = privp->s_asyntab;
	caller_ptr = src_ptr;

	dst_e= dst_ptr->p_endpoint;

	/* Scan the table */
	do_notify= FALSE;	
	done= TRUE;
	for (i= 0; i<size; i++)
	{
		/* Read status word */
		A_RETRIEVE(i, flags);
		flags= tabent.flags;

		/* Skip empty entries */
		if (flags == 0)
		{
			continue;
		}

		/* Check for reserved bits in the flags field */
		if (flags & ~(AMF_VALID|AMF_DONE|AMF_NOTIFY|AMF_NOREPLY) ||
			!(flags & AMF_VALID))
		{
			kprintf("try_one: bad bits in table\n");
			privp->s_asynsize= 0;
			return EINVAL;
		}

		/* Skip entry is AMF_DONE is already set */
		if (flags & AMF_DONE)
		{
			continue;
		}

		/* Clear done. We are done when all entries are either empty
		 * or done at the start of the call.
		 */
		done= FALSE;

		/* Get destination */
		A_RETRIEVE(i, dst);

		if (tabent.dst != dst_e)
		{
			continue;
		}

		/* If AMF_NOREPLY is set, do not satisfy the receiving part of
		 * a SENDREC. Do not unset MF_ASYNMSG later because of this,
		 * though: this message is still to be delivered later.
		 */
		if ((flags & AMF_NOREPLY) &&
			(dst_ptr->p_misc_flags & MF_REPLY_PEND))
		{
			if (postponed != NULL)
				*postponed = TRUE;

			continue;
		}

		/* Deliver message */
		A_RETRIEVE(i, msg);
		r = QueueMess(src_ptr->p_endpoint, vir2phys(&tabent.msg),
			dst_ptr);

		tabent.result= r;
		A_INSERT(i, result);
		tabent.flags= flags | AMF_DONE;
		A_INSERT(i, flags);

		if (flags & AMF_NOTIFY)
		{
			kprintf("try_one: should notify caller\n");
		}
		return OK;
	}
	if (done)
		privp->s_asynsize= 0;
	return EAGAIN;
}

 /*===========================================================================*
 *				lock_notify				     *
 *===========================================================================*/
PUBLIC int lock_notify(src_e, dst_e)
int src_e;			/* (endpoint) sender of the notification */
int dst_e;			/* (endpoint) who is to be notified */
{
/* Safe gateway to mini_notify() for tasks and interrupt handlers. The sender
 * is explicitely given to prevent confusion where the call comes from. MINIX 
 * kernel is not reentrant, which means to interrupts are disabled after 
 * the first kernel entry (hardware interrupt, trap, or exception). Locking
 * is done by temporarily disabling interrupts. 
 */
  int result, src_p;

  vmassert(!intr_disabled());

  if (!isokendpt(src_e, &src_p)) {
	kprintf("lock_notify: bogus src: %d\n", src_e);
	return EDEADSRCDST;
  }

      lock;
  vmassert(intr_disabled());
      result = mini_notify(proc_addr(src_p), dst_e); 
  vmassert(intr_disabled());
      unlock;
  vmassert(!intr_disabled());

  return(result);
}

/*===========================================================================*
 *				enqueue					     * 
 *===========================================================================*/
PUBLIC void enqueue(rp)
register struct proc *rp;	/* this process is now runnable */
{
/* Add 'rp' to one of the queues of runnable processes.  This function is 
 * responsible for inserting a process into one of the scheduling queues. 
 * The mechanism is implemented here.   The actual scheduling policy is
 * defined in sched() and pick_proc().
 */
  int q;	 				/* scheduling queue to use */
  int front;					/* add to front or back */

  NOREC_ENTER(enqueuefunc);

#if DEBUG_SCHED_CHECK
  if(!intr_disabled()) { minix_panic("enqueue with interrupts enabled", NO_NUM); }
  if (rp->p_ready) minix_panic("enqueue already ready process", NO_NUM);
#endif

  /* Determine where to insert to process. */
  sched(rp, &q, &front);

  vmassert(q >= 0);

  /* Now add the process to the queue. */
  if (rdy_head[q] == NIL_PROC) {		/* add to empty queue */
      rdy_head[q] = rdy_tail[q] = rp; 		/* create a new queue */
      rp->p_nextready = NIL_PROC;		/* mark new end */
  } 
  else if (front) {				/* add to head of queue */
      rp->p_nextready = rdy_head[q];		/* chain head of queue */
      rdy_head[q] = rp;				/* set new queue head */
  } 
  else {					/* add to tail of queue */
      rdy_tail[q]->p_nextready = rp;		/* chain tail of queue */	
      rdy_tail[q] = rp;				/* set new queue tail */
      rp->p_nextready = NIL_PROC;		/* mark new end */
  }

#if DEBUG_SCHED_CHECK
  rp->p_ready = 1;
  CHECK_RUNQUEUES;
#endif

  /*
   * enqueueing a process with a higher priority than the current one, it gets
   * preempted. The current process must be preemptible. Testing the priority
   * also makes sure that a process does not preempt itself
   */
  vmassert(proc_ptr);
  if ((proc_ptr->p_priority > rp->p_priority) &&
		  (priv(proc_ptr)->s_flags & PREEMPTIBLE))
     RTS_SET(proc_ptr, RTS_PREEMPTED); /* calls dequeue() */

#if DEBUG_SCHED_CHECK
  CHECK_RUNQUEUES;
#endif

  NOREC_RETURN(enqueuefunc, );
}

/*===========================================================================*
 *				enqueue_head				     *
 *===========================================================================*/
/*
 * put a process at the front of its run queue. It comes handy when a process is
 * preempted and removed from run queue to not to have a currently not-runnable
 * process on a run queue. We have to put this process back at the fron to be
 * fair
 */
PRIVATE void enqueue_head(struct proc *rp)
{
  int q;	 				/* scheduling queue to use */

#if DEBUG_SCHED_CHECK
  if(!intr_disabled()) { minix_panic("enqueue with interrupts enabled", NO_NUM); }
  if (rp->p_ready) minix_panic("enqueue already ready process", NO_NUM);
#endif

  /*
   * the process was runnable without its quantum expired when dequeued. A
   * process with no time left should vahe been handled else and differently
   */
  vmassert(rp->p_ticks_left);

  vmassert(q >= 0);

  q = rp->p_priority;

  /* Now add the process to the queue. */
  if (rdy_head[q] == NIL_PROC) {		/* add to empty queue */
      rdy_head[q] = rdy_tail[q] = rp; 		/* create a new queue */
      rp->p_nextready = NIL_PROC;		/* mark new end */
  }
  else						/* add to head of queue */
      rp->p_nextready = rdy_head[q];		/* chain head of queue */
      rdy_head[q] = rp;				/* set new queue head */

#if DEBUG_SCHED_CHECK
  rp->p_ready = 1;
  CHECK_RUNQUEUES;
#endif
}

/*===========================================================================*
 *				dequeue					     * 
 *===========================================================================*/
PUBLIC void dequeue(rp)
register struct proc *rp;	/* this process is no longer runnable */
{
/* A process must be removed from the scheduling queues, for example, because
 * it has blocked.  If the currently active process is removed, a new process
 * is picked to run by calling pick_proc().
 */
  register int q = rp->p_priority;		/* queue to use */
  register struct proc **xpp;			/* iterate over queue */
  register struct proc *prev_xp;

  NOREC_ENTER(dequeuefunc);

#if DEBUG_STACK_CHECK
  /* Side-effect for kernel: check if the task's stack still is ok? */
  if (iskernelp(rp)) { 				
	if (*priv(rp)->s_stack_guard != STACK_GUARD)
		minix_panic("stack overrun by task", proc_nr(rp));
  }
#endif

#if DEBUG_SCHED_CHECK
  if(!intr_disabled()) { minix_panic("dequeue with interrupts enabled", NO_NUM); }
  if (! rp->p_ready) minix_panic("dequeue() already unready process", NO_NUM);
#endif

  /* Now make sure that the process is not in its ready queue. Remove the 
   * process if it is found. A process can be made unready even if it is not 
   * running by being sent a signal that kills it.
   */
  prev_xp = NIL_PROC;				
  for (xpp = &rdy_head[q]; *xpp != NIL_PROC; xpp = &(*xpp)->p_nextready) {

      if (*xpp == rp) {				/* found process to remove */
          *xpp = (*xpp)->p_nextready;		/* replace with next chain */
          if (rp == rdy_tail[q])		/* queue tail removed */
              rdy_tail[q] = prev_xp;		/* set new tail */

#if DEBUG_SCHED_CHECK
  		rp->p_ready = 0;
		  CHECK_RUNQUEUES;
#endif
          break;
      }
      prev_xp = *xpp;				/* save previous in chain */
  }

#if DEBUG_SCHED_CHECK
  CHECK_RUNQUEUES;
#endif

  NOREC_RETURN(dequeuefunc, );
}

/*===========================================================================*
 *				sched					     * 
 *===========================================================================*/
PRIVATE void sched(rp, queue, front)
register struct proc *rp;			/* process to be scheduled */
int *queue;					/* return: queue to use */
int *front;					/* return: front or back */
{
/* This function determines the scheduling policy.  It is called whenever a
 * process must be added to one of the scheduling queues to decide where to
 * insert it.  As a side-effect the process' priority may be updated.  
 */
  int time_left = (rp->p_ticks_left > 0);	/* quantum fully consumed */

  /* Check whether the process has time left. Otherwise give a new quantum 
   * and lower the process' priority, unless the process already is in the 
   * lowest queue.  
   */
  if (! time_left) {				/* quantum consumed ? */
      rp->p_ticks_left = rp->p_quantum_size; 	/* give new quantum */
      if (rp->p_priority < (NR_SCHED_QUEUES-1)) {
          rp->p_priority += 1;			/* lower priority */
      }
  }

  /* If there is time left, the process is added to the front of its queue, 
   * so that it can immediately run. The queue to use simply is always the
   * process' current priority. 
   */
  *queue = rp->p_priority;
  *front = time_left;
}

/*===========================================================================*
 *				pick_proc				     * 
 *===========================================================================*/
PRIVATE struct proc * pick_proc(void)
{
/* Decide who to run now.  A new process is selected an returned.
 * When a billable process is selected, record it in 'bill_ptr', so that the 
 * clock task can tell who to bill for system time.
 */
  register struct proc *rp;			/* process to run */
  int q;				/* iterate over queues */

  /* Check each of the scheduling queues for ready processes. The number of
   * queues is defined in proc.h, and priorities are set in the task table.
   * The lowest queue contains IDLE, which is always ready.
   */
  for (q=0; q < NR_SCHED_QUEUES; q++) {	
	if(!(rp = rdy_head[q])) {
		TRACE(VF_PICKPROC, printf("queue %d empty\n", q););
		continue;
	}
	TRACE(VF_PICKPROC, printf("found %s / %d on queue %d\n", 
		rp->p_name, rp->p_endpoint, q););
	vmassert(!proc_is_runnable(rp));
	if (priv(rp)->s_flags & BILLABLE)	 	
		bill_ptr = rp;		/* bill for system time */
	return rp;
  }
  return NULL;
}

/*===========================================================================*
 *				balance_queues				     *
 *===========================================================================*/
#define Q_BALANCE_TICKS	 100
PUBLIC void balance_queues(tp)
timer_t *tp;					/* watchdog timer pointer */
{
/* Check entire process table and give all process a higher priority. This
 * effectively means giving a new quantum. If a process already is at its 
 * maximum priority, its quantum will be renewed.
 */
  static timer_t queue_timer;			/* timer structure to use */
  register struct proc* rp;			/* process table pointer  */
  clock_t next_period;				/* time of next period  */
  int ticks_added = 0;				/* total time added */

  vmassert(!intr_disabled());

  lock;
  for (rp=BEG_PROC_ADDR; rp<END_PROC_ADDR; rp++) {
      if (! isemptyp(rp)) {				/* check slot use */
	  if (rp->p_priority > rp->p_max_priority) {	/* update priority? */
	      if (proc_is_runnable(rp)) dequeue(rp);	/* take off queue */
	      ticks_added += rp->p_quantum_size;	/* do accounting */
	      rp->p_priority -= 1;			/* raise priority */
	      if (proc_is_runnable(rp)) enqueue(rp);	/* put on queue */
	  }
	  else {
	      ticks_added += rp->p_quantum_size - rp->p_ticks_left;
              rp->p_ticks_left = rp->p_quantum_size; 	/* give new quantum */
	  }
      }
  }
  unlock;

  /* Now schedule a new watchdog timer to balance the queues again.  The 
   * period depends on the total amount of quantum ticks added.
   */
  next_period = MAX(Q_BALANCE_TICKS, ticks_added);	/* calculate next */
  set_timer(&queue_timer, get_uptime() + next_period, balance_queues);
}

/*===========================================================================*
 *				lock_send				     *
 *===========================================================================*/
PUBLIC int lock_send(dst_e, m_ptr)
int dst_e;			/* to whom is message being sent? */
message *m_ptr;			/* pointer to message buffer */
{
/* Safe gateway to mini_send() for tasks. */
  int result;
  lock;
  result = mini_send(proc_ptr, dst_e, m_ptr, 0);
  unlock;
  return(result);
}

/*===========================================================================*
 *				endpoint_lookup				     *
 *===========================================================================*/
PUBLIC struct proc *endpoint_lookup(endpoint_t e)
{
	int n;

	if(!isokendpt(e, &n)) return NULL;

	return proc_addr(n);
}

/*===========================================================================*
 *				isokendpt_f				     *
 *===========================================================================*/
#if DEBUG_ENABLE_IPC_WARNINGS
PUBLIC int isokendpt_f(file, line, e, p, fatalflag)
char *file;
int line;
#else
PUBLIC int isokendpt_f(e, p, fatalflag)
#endif
endpoint_t e;
int *p, fatalflag;
{
	int ok = 0;
	/* Convert an endpoint number into a process number.
	 * Return nonzero if the process is alive with the corresponding
	 * generation number, zero otherwise.
	 *
	 * This function is called with file and line number by the
	 * isokendpt_d macro if DEBUG_ENABLE_IPC_WARNINGS is defined,
	 * otherwise without. This allows us to print the where the
	 * conversion was attempted, making the errors verbose without
	 * adding code for that at every call.
	 * 
	 * If fatalflag is nonzero, we must panic if the conversion doesn't
	 * succeed.
	 */
	*p = _ENDPOINT_P(e);
	if(!isokprocn(*p)) {
#if DEBUG_ENABLE_IPC_WARNINGS
		kprintf("kernel:%s:%d: bad endpoint %d: proc %d out of range\n",
		file, line, e, *p);
#endif
	} else if(isemptyn(*p)) {
#if 0
	kprintf("kernel:%s:%d: bad endpoint %d: proc %d empty\n", file, line, e, *p);
#endif
	} else if(proc_addr(*p)->p_endpoint != e) {
#if DEBUG_ENABLE_IPC_WARNINGS
		kprintf("kernel:%s:%d: bad endpoint %d: proc %d has ept %d (generation %d vs. %d)\n", file, line,
		e, *p, proc_addr(*p)->p_endpoint,
		_ENDPOINT_G(e), _ENDPOINT_G(proc_addr(*p)->p_endpoint));
#endif
	} else ok = 1;
	if(!ok && fatalflag) {
		minix_panic("invalid endpoint ", e);
	}
	return ok;
}

