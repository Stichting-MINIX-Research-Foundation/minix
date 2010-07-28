/* This file contains essentially all of the process and message handling.
 * Together with "mpx.s" it forms the lowest layer of the MINIX kernel.
 * There is one entry point from the outside:
 *
 *   sys_call: 	      a system call, i.e., the kernel is trapped with an INT
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
#include <minix/ipcconst.h>
#include <stddef.h>
#include <signal.h>
#include <minix/syslib.h>
#include <assert.h>

#include "debug.h"
#include "kernel.h"
#include "proc.h"
#include "vm.h"
#include "clock.h"

/* Scheduling and message passing functions */
FORWARD _PROTOTYPE( void idle, (void));
/**
 * Made public for use in clock.c (for user-space scheduling)
FORWARD _PROTOTYPE( int mini_send, (struct proc *caller_ptr, endpoint_t dst_e,
		message *m_ptr, int flags));
*/
FORWARD _PROTOTYPE( int mini_receive, (struct proc *caller_ptr, endpoint_t src,
		message *m_ptr, int flags));
FORWARD _PROTOTYPE( int mini_senda, (struct proc *caller_ptr,
	asynmsg_t *table, size_t size));
FORWARD _PROTOTYPE( int deadlock, (int function,
		register struct proc *caller, endpoint_t src_dst_e));
FORWARD _PROTOTYPE( int try_async, (struct proc *caller_ptr));
FORWARD _PROTOTYPE( int try_one, (struct proc *src_ptr, struct proc *dst_ptr,
		int *postponed));
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
 *				idle					     * 
 *===========================================================================*/
PRIVATE void idle(void)
{
	/* This function is called whenever there is no work to do.
	 * Halt the CPU, and measure how many timestamp counter ticks are
	 * spent not doing anything. This allows test setups to measure
	 * the CPU utiliziation of certain workloads with high precision.
	 */

	/* start accounting for the idle time */
	context_stop(proc_addr(KERNEL));
	halt_cpu();
	/*
	 * end of accounting for the idle task does not happen here, the kernel
	 * is handling stuff for quite a while before it gets back here!
	 */
}

/*===========================================================================*
 *				switch_to_user				     * 
 *===========================================================================*/
PUBLIC void switch_to_user(void)
{
	/* This function is called an instant before proc_ptr is
	 * to be scheduled again.
	 */

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
		if (proc_is_runnable(proc_ptr)) {
			if (!is_zero64(proc_ptr->p_cpu_time_left))
				enqueue_head(proc_ptr);
			else
				enqueue(proc_ptr);
		}
	}

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

	switch_address_space(proc_ptr);

check_misc_flags:

	assert(proc_ptr);
	assert(proc_is_runnable(proc_ptr));
	while (proc_ptr->p_misc_flags &
		(MF_KCALL_RESUME | MF_DELIVERMSG |
		 MF_SC_DEFER | MF_SC_TRACE | MF_SC_ACTIVE)) {

		assert(proc_is_runnable(proc_ptr));
		if (proc_ptr->p_misc_flags & MF_KCALL_RESUME) {
			kernel_call_resume(proc_ptr);
		}
		else if (proc_ptr->p_misc_flags & MF_DELIVERMSG) {
			TRACE(VF_SCHEDULING, printf("delivering to %s / %d\n",
				proc_ptr->p_name, proc_ptr->p_endpoint););
			delivermsg(proc_ptr);
		}
		else if (proc_ptr->p_misc_flags & MF_SC_DEFER) {
			/* Perform the system call that we deferred earlier. */

			assert (!(proc_ptr->p_misc_flags & MF_SC_ACTIVE));

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

		if (!proc_is_runnable(proc_ptr))
			break;
	}
	/*
	 * check the quantum left before it runs again. We must do it only here
	 * as we are sure that a possible out-of-quantum message to the
	 * scheduler will not collide with the regular ipc
	 */
	if (is_zero64(proc_ptr->p_cpu_time_left))
		proc_no_time(proc_ptr);
	/*
	 * After handling the misc flags the selected process might not be
	 * runnable anymore. We have to checkit and schedule another one
	 */
	if (!proc_is_runnable(proc_ptr))
		goto not_runnable_pick_new;

	TRACE(VF_SCHEDULING, printf("starting %s / %d\n",
		proc_ptr->p_name, proc_ptr->p_endpoint););
#if DEBUG_TRACE
	proc_ptr->p_schedules++;
#endif


	proc_ptr = arch_finish_switch_to_user();
	assert(!is_zero64(proc_ptr->p_cpu_time_left));

	context_stop(proc_addr(KERNEL));

	/* If the process isn't the owner of FPU, enable the FPU exception */
	if(fpu_owner != proc_ptr)
		enable_fpu_exception();
	else
		disable_fpu_exception();

	/* If MF_CONTEXT_SET is set, don't clobber process state within
	 * the kernel. The next kernel entry is OK again though.
	 */
	proc_ptr->p_misc_flags &= ~MF_CONTEXT_SET;

	/*
	 * restore_user_context() carries out the actual mode switch from kernel
	 * to userspace. This function does not return
	 */
	restore_user_context(proc_ptr);
	NOT_REACHABLE;
}

/*
 * handler for all synchronous IPC calls
 */
PRIVATE int do_sync_ipc(struct proc * caller_ptr, /* who made the call */
			int call_nr,	/* system call number and flags */
			endpoint_t src_dst_e,	/* src or dst of the call */
			message *m_ptr)	/* users pointer to a message */
{
  int result;					/* the system call's result */
  int src_dst_p;				/* Process slot number */
  char *callname;

  /* Check destination. RECEIVE is the only call that accepts ANY (in addition
   * to a real endpoint). The other calls (SEND, SENDREC, and NOTIFY) require an
   * endpoint to corresponds to a process. In addition, it is necessary to check
   * whether a process is allowed to send to a given destination.
   */
  assert(call_nr != SENDA);

  /* Only allow non-negative call_nr values less than 32 */
  if (call_nr < 0 || call_nr > IPCNO_HIGHEST || call_nr >= 32
      || !(callname = ipc_call_names[call_nr])) {
#if DEBUG_ENABLE_IPC_WARNINGS
      printf("sys_call: trap %d not allowed, caller %d, src_dst %d\n", 
          call_nr, proc_nr(caller_ptr), src_dst_p);
#endif
	return(ETRAPDENIED);		/* trap denied by mask or kernel */
  }

  if (src_dst_e == ANY)
  {
	if (call_nr != RECEIVE)
	{
#if 0
		printf("sys_call: %s by %d with bad endpoint %d\n", 
			callname,
			proc_nr(caller_ptr), src_dst_e);
#endif
		return EINVAL;
	}
	src_dst_p = (int) src_dst_e;
  }
  else
  {
	/* Require a valid source and/or destination process. */
	if(!isokendpt(src_dst_e, &src_dst_p)) {
#if 0
		printf("sys_call: %s by %d with bad endpoint %d\n", 
			callname,
			proc_nr(caller_ptr), src_dst_e);
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
			printf(
			"sys_call: ipc mask denied %s from %d to %d\n",
				callname,
				caller_ptr->p_endpoint, src_dst_e);
#endif
			return(ECALLDENIED);	/* call denied by ipc mask */
		}
	}
  }

  /* Check if the process has privileges for the requested call. Calls to the 
   * kernel may only be SENDREC, because tasks always reply and may not block 
   * if the caller doesn't do receive(). 
   */
  if (!(priv(caller_ptr)->s_trap_mask & (1 << call_nr))) {
#if DEBUG_ENABLE_IPC_WARNINGS
      printf("sys_call: %s not allowed, caller %d, src_dst %d\n", 
          callname, proc_nr(caller_ptr), src_dst_p);
#endif
	return(ETRAPDENIED);		/* trap denied by mask or kernel */
  }

  if (call_nr != SENDREC && call_nr != RECEIVE && iskerneln(src_dst_p)) {
#if DEBUG_ENABLE_IPC_WARNINGS
      printf("sys_call: trap %d not allowed, caller %d, src_dst %d\n", 
           callname, proc_nr(caller_ptr), src_dst_e);
#endif
	return(ETRAPDENIED);		/* trap denied by mask or kernel */
  }

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
	if (call_nr == RECEIVE) {
		caller_ptr->p_misc_flags &= ~MF_REPLY_PEND;
		IPC_STATUS_CLEAR(caller_ptr);  /* clear IPC status code */
	}
	result = mini_receive(caller_ptr, src_dst_e, m_ptr, 0);
	break;
  case NOTIFY:
	result = mini_notify(caller_ptr, src_dst_e);
	break;
  case SENDNB:
        result = mini_send(caller_ptr, src_dst_e, m_ptr, NON_BLOCKING);
        break;
  default:
	result = EBADCALL;			/* illegal system call */
  }

  /* Now, return the result of the system call to the caller. */
  return(result);
}

PUBLIC int do_ipc(reg_t r1, reg_t r2, reg_t r3)
{
  struct proc * caller_ptr = proc_ptr;	/* always the current process */
  int call_nr = (int) r1;

  assert(!RTS_ISSET(caller_ptr, RTS_SLOT_FREE));

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

	assert (!(caller_ptr->p_misc_flags & MF_SC_ACTIVE));

	/* Set a flag to allow reliable tracing of leaving the system call. */
	caller_ptr->p_misc_flags |= MF_SC_ACTIVE;
  }

  if(caller_ptr->p_misc_flags & MF_DELIVERMSG) {
	panic("sys_call: MF_DELIVERMSG on for %s / %d\n",
		caller_ptr->p_name, caller_ptr->p_endpoint);
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
  	case SEND:			
  	case RECEIVE:			
  	case NOTIFY:
  	case SENDNB:
  	    return do_sync_ipc(caller_ptr, call_nr, (endpoint_t) r2,
			    (message *) r3);
  	case SENDA:
  	{
  	    /*
  	     * Get and check the size of the argument in bytes as it is a
  	     * table
  	     */
  	    size_t msg_size = (size_t) r2;
  
  	    /* Limit size to something reasonable. An arbitrary choice is 16
  	     * times the number of process table entries.
  	     */
  	    if (msg_size > 16*(NR_TASKS + NR_PROCS))
	        return EDOM;
  	    return mini_senda(caller_ptr, (asynmsg_t *) r3, msg_size);
  	}
  	default:
	return EBADCALL;		/* illegal system call */
  }
}

/*===========================================================================*
 *				deadlock				     * 
 *===========================================================================*/
PRIVATE int deadlock(function, cp, src_dst_e) 
int function;					/* trap number */
register struct proc *cp;			/* pointer to caller */
endpoint_t src_dst_e;				/* src or dst process */
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

  while (src_dst_e != ANY) { 			/* check while process nr */
      int src_dst_slot;
      okendpt(src_dst_e, &src_dst_slot);
      xp = proc_addr(src_dst_slot);		/* follow chain of processes */
      assert(proc_ptr_ok(xp));
      assert(!RTS_ISSET(xp, RTS_SLOT_FREE));
#if DEBUG_ENABLE_IPC_WARNINGS
      processes[group_size] = xp;
#endif
      group_size ++;				/* extra process in group */

      /* Check whether the last process in the chain has a dependency. If it 
       * has not, the cycle cannot be closed and we are done.
       */
      if((src_dst_e = P_BLOCKEDON(xp)) == NONE)
	return 0;

      /* Now check if there is a cyclic dependency. For group sizes of two,  
       * a combination of SEND(REC) and RECEIVE is not fatal. Larger groups
       * or other combinations indicate a deadlock.  
       */
      if (src_dst_e == cp->p_endpoint) {	/* possible deadlock */
	  if (group_size == 2) {		/* caller and src_dst */
	      /* The function number is magically converted to flags. */
	      if ((xp->p_rts_flags ^ (function << 2)) & RTS_SENDING) { 
	          return(0);			/* not a deadlock */
	      }
	  }
#if DEBUG_ENABLE_IPC_WARNINGS
	  {
		int i;
		printf("deadlock between these processes:\n");
		for(i = 0; i < group_size; i++) {
			printf(" %10s ", processes[i]->p_name);
		}
		printf("\n\n");
		for(i = 0; i < group_size; i++) {
			print_proc(processes[i]);
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
PUBLIC int mini_send(
  register struct proc *caller_ptr,	/* who is trying to send a message? */
  endpoint_t dst_e,			/* to whom is message being sent? */
  message *m_ptr,			/* pointer to message buffer */
  const int flags
)
{
/* Send a message from 'caller_ptr' to 'dst'. If 'dst' is blocked waiting
 * for this message, copy the message to it and unblock 'dst'. If 'dst' is
 * not waiting at all, or is waiting for another source, queue 'caller_ptr'.
 */
  register struct proc *dst_ptr;
  register struct proc **xpp;
  int dst_p;
  dst_p = _ENDPOINT_P(dst_e);
  dst_ptr = proc_addr(dst_p);

  if (RTS_ISSET(dst_ptr, RTS_NO_ENDPOINT))
  {
	return EDEADSRCDST;
  }

  /* Check if 'dst' is blocked waiting for this message. The destination's 
   * RTS_SENDING flag may be set when its SENDREC call blocked while sending.  
   */
  if (WILLRECEIVE(dst_ptr, caller_ptr->p_endpoint)) {
	int call;
	/* Destination is indeed waiting for this message. */
	assert(!(dst_ptr->p_misc_flags & MF_DELIVERMSG));	

	if (!(flags & FROM_KERNEL)) {
		if(copy_msg_from_user(caller_ptr, m_ptr, &dst_ptr->p_delivermsg))
			return EFAULT;
	} else {
		dst_ptr->p_delivermsg = *m_ptr;
		IPC_STATUS_ADD_FLAGS(dst_ptr, IPC_FLG_MSG_FROM_KERNEL);
	}

	dst_ptr->p_delivermsg.m_source = caller_ptr->p_endpoint;
	dst_ptr->p_misc_flags |= MF_DELIVERMSG;

	call = (caller_ptr->p_misc_flags & MF_REPLY_PEND ? SENDREC
		: (flags & NON_BLOCKING ? SENDNB : SEND));
	IPC_STATUS_ADD_CALL(dst_ptr, call);

	if (dst_ptr->p_misc_flags & MF_REPLY_PEND)
		dst_ptr->p_misc_flags &= ~MF_REPLY_PEND;

	RTS_UNSET(dst_ptr, RTS_RECEIVING);

#if DEBUG_DUMPIPC
	printmsgsend(&dst_ptr->p_delivermsg, caller_ptr, dst_ptr);
	printmsgrecv(&dst_ptr->p_delivermsg, caller_ptr, dst_ptr);
#endif
  } else {
	if(flags & NON_BLOCKING) {
		return(ENOTREADY);
	}

	/* Check for a possible deadlock before actually blocking. */
	if (deadlock(SEND, caller_ptr, dst_e)) {
		return(ELOCKED);
	}

	/* Destination is not waiting.  Block and dequeue caller. */
	if (!(flags & FROM_KERNEL)) {
		if(copy_msg_from_user(caller_ptr, m_ptr, &caller_ptr->p_sendmsg))
			return EFAULT;
	} else {
		caller_ptr->p_sendmsg = *m_ptr;
		/*
		 * we need to remember that this message is from kernel so we
		 * can set the delivery status flags when the message is
		 * actually delivered
		 */
		caller_ptr->p_misc_flags |= MF_SENDING_FROM_KERNEL;
	}

	RTS_SET(caller_ptr, RTS_SENDING);
	caller_ptr->p_sendto_e = dst_e;

	/* Process is now blocked.  Put in on the destination's queue. */
	assert(caller_ptr->p_q_link == NULL);
	xpp = &dst_ptr->p_caller_q;		/* find end of list */
	while (*xpp) xpp = &(*xpp)->p_q_link;	
	*xpp = caller_ptr;			/* add caller to end */

#if DEBUG_DUMPIPC
	printmsgsend(&caller_ptr->p_sendmsg, caller_ptr, dst_ptr);
#endif
  }
  return(OK);
}

/*===========================================================================*
 *				mini_receive				     * 
 *===========================================================================*/
PRIVATE int mini_receive(struct proc * caller_ptr,
			endpoint_t src_e, /* which message source is wanted */
			message * m_buff_usr, /* pointer to message buffer */
			const int flags)
{
/* A process or task wants to get a message.  If a message is already queued,
 * acquire it and deblock the sender.  If no message from the desired source
 * is available block the caller.
 */
  register struct proc **xpp;
  sys_map_t *map;
  bitchunk_t *chunk;
  int i, r, src_id, src_proc_nr, src_p;

  assert(!(caller_ptr->p_misc_flags & MF_DELIVERMSG));

  /* This is where we want our message. */
  caller_ptr->p_delivermsg_vir = (vir_bytes) m_buff_usr;

  if(src_e == ANY) src_p = ANY;
  else
  {
	okendpt(src_e, &src_p);
	if (RTS_ISSET(proc_addr(src_p), RTS_NO_ENDPOINT))
	{
		return EDEADSRCDST;
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
		printf("mini_receive: sending notify from NONE\n");
	    }
#endif
            if (src_e!=ANY && src_p != src_proc_nr) continue;/* source not ok */
            *chunk &= ~(1 << i);			/* no longer pending */

            /* Found a suitable source, deliver the notification message. */
	    hisep = proc_addr(src_proc_nr)->p_endpoint;
	    assert(!(caller_ptr->p_misc_flags & MF_DELIVERMSG));	
	    assert(src_e == ANY || hisep == src_e);

	    /* assemble message */
	    BuildNotifyMessage(&caller_ptr->p_delivermsg, src_proc_nr, caller_ptr);
	    caller_ptr->p_delivermsg.m_source = hisep;
	    caller_ptr->p_misc_flags |= MF_DELIVERMSG;

	    IPC_STATUS_ADD_CALL(caller_ptr, NOTIFY);

	    goto receive_done;
        }
    }

    /* Check if there are pending senda(). */
    if (caller_ptr->p_misc_flags & MF_ASYNMSG)
    {
	if (src_e != ANY)
		r= try_one(proc_addr(src_p), caller_ptr, NULL);
	else
		r= try_async(caller_ptr);

	if (r == OK) {
		IPC_STATUS_ADD_CALL(caller_ptr, SENDA);
		goto receive_done;
	}
    }

    /* Check caller queue. Use pointer pointers to keep code simple. */
    xpp = &caller_ptr->p_caller_q;
    while (*xpp) {
	struct proc * sender = *xpp;

        if (src_e == ANY || src_p == proc_nr(sender)) {
            int call;
	    assert(!RTS_ISSET(sender, RTS_SLOT_FREE));
	    assert(!RTS_ISSET(sender, RTS_NO_ENDPOINT));

	    /* Found acceptable message. Copy it and update status. */
  	    assert(!(caller_ptr->p_misc_flags & MF_DELIVERMSG));
	    caller_ptr->p_delivermsg = sender->p_sendmsg;
	    caller_ptr->p_delivermsg.m_source = sender->p_endpoint;
	    caller_ptr->p_misc_flags |= MF_DELIVERMSG;
	    RTS_UNSET(sender, RTS_SENDING);

	    call = (sender->p_misc_flags & MF_REPLY_PEND ? SENDREC : SEND);
	    IPC_STATUS_ADD_CALL(caller_ptr, call);

	    /*
	     * if the message is originaly from the kernel on behalf of this
	     * process, we must send the status flags accordingly
	     */
	    if (sender->p_misc_flags & MF_SENDING_FROM_KERNEL) {
		IPC_STATUS_ADD_FLAGS(caller_ptr, IPC_FLG_MSG_FROM_KERNEL);
		/* we can clean the flag now, not need anymore */
		sender->p_misc_flags &= ~MF_SENDING_FROM_KERNEL;
	    }
	    if (sender->p_misc_flags & MF_SIG_DELAY)
		sig_delay_done(sender);

#if DEBUG_DUMPIPC
            printmsgrecv(&caller_ptr->p_delivermsg, *xpp, caller_ptr);
#endif
		
            *xpp = sender->p_q_link;		/* remove from queue */
	    sender->p_q_link = NULL;
	    goto receive_done;
	}
	xpp = &sender->p_q_link;		/* proceed to next */
    }
  }

  /* No suitable message is available or the caller couldn't send in SENDREC. 
   * Block the process trying to receive, unless the flags tell otherwise.
   */
  if ( ! (flags & NON_BLOCKING)) {
      /* Check for a possible deadlock before actually blocking. */
      if (deadlock(RECEIVE, caller_ptr, src_e)) {
          return(ELOCKED);
      }

      caller_ptr->p_getfrom_e = src_e;		
      RTS_SET(caller_ptr, RTS_RECEIVING);
      return(OK);
  } else {
	return(ENOTREADY);
  }

receive_done:
  if (caller_ptr->p_misc_flags & MF_REPLY_PEND)
	  caller_ptr->p_misc_flags &= ~MF_REPLY_PEND;
  return OK;
}

/*===========================================================================*
 *				mini_notify				     * 
 *===========================================================================*/
PUBLIC int mini_notify(
  const struct proc *caller_ptr,	/* sender of the notification */
  endpoint_t dst_e			/* which process to notify */
)
{
  register struct proc *dst_ptr;
  int src_id;				/* source id for late delivery */
  int dst_p;

  if (!isokendpt(dst_e, &dst_p)) {
	util_stacktrace();
	printf("mini_notify: bogus endpoint %d\n", dst_e);
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
      assert(!(dst_ptr->p_misc_flags & MF_DELIVERMSG));

      BuildNotifyMessage(&dst_ptr->p_delivermsg, proc_nr(caller_ptr), dst_ptr);
      dst_ptr->p_delivermsg.m_source = caller_ptr->p_endpoint;
      dst_ptr->p_misc_flags |= MF_DELIVERMSG;

      IPC_STATUS_ADD_CALL(dst_ptr, NOTIFY);
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
	printf("kernel:%s:%d: asyn failed for %s in %s "	\
	"(%d/%d, tab 0x%lx)\n",__FILE__,__LINE__,	\
field, caller->p_name, entry, priv(caller)->s_asynsize, priv(caller)->s_asyntab)

#define A_RETRIEVE(entry, field)	\
  if(data_copy(caller_ptr->p_endpoint,	\
	 table_v + (entry)*sizeof(asynmsg_t) + offsetof(struct asynmsg,field),\
		KERNEL, (vir_bytes) &tabent.field,	\
			sizeof(tabent.field)) != OK) {\
		ASCOMPLAIN(caller_ptr, entry, #field);	\
		return EFAULT; \
	}

#define A_INSERT(entry, field)	\
  if(data_copy(KERNEL, (vir_bytes) &tabent.field, \
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
	const vir_bytes table_v = (vir_bytes) table;

	privp= priv(caller_ptr);
	if (!(privp->s_flags & SYS_PROC))
	{
		printf(
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
		printf("mini_senda: entry[%d]: flags 0x%x dst %d/%d\n",
			i, tabent.flags, tabent.dst, dst_p);
#endif

		dst_ptr = proc_addr(dst_p);

		/* RTS_NO_ENDPOINT should be removed */
		if (RTS_ISSET(dst_ptr, RTS_NO_ENDPOINT))
		{
			tabent.result= EDEADSRCDST;
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
			if(copy_msg_from_user(caller_ptr, &table[i].msg,
						&dst_ptr->p_delivermsg))
				tabent.result = EFAULT;
			else {
				dst_ptr->p_delivermsg.m_source = caller_ptr->p_endpoint;
				dst_ptr->p_misc_flags |= MF_DELIVERMSG;
				IPC_STATUS_ADD_CALL(dst_ptr, SENDA);
				RTS_UNSET(dst_ptr, RTS_RECEIVING);
				tabent.result = OK;
			}

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
		printf("mini_senda: should notify caller\n");
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

	  	assert(!(caller_ptr->p_misc_flags & MF_DELIVERMSG));
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
	int i, done;
	unsigned flags;
	size_t size;
	endpoint_t dst_e;
	struct priv *privp;
	asynmsg_t tabent;
	vir_bytes table_v;
	struct proc *caller_ptr;

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
			printf("try_one: bad bits in table\n");
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
		dst_ptr->p_delivermsg = tabent.msg;
		dst_ptr->p_delivermsg.m_source = src_ptr->p_endpoint;
		dst_ptr->p_misc_flags |= MF_DELIVERMSG;

		tabent.result = OK;
		A_INSERT(i, result);
		tabent.flags= flags | AMF_DONE;
		A_INSERT(i, flags);

		if (flags & AMF_NOTIFY)
		{
			printf("try_one: should notify caller\n");
		}
		return OK;
	}
	if (done)
		privp->s_asynsize= 0;
	return EAGAIN;
}

/*===========================================================================*
 *				enqueue					     * 
 *===========================================================================*/
PUBLIC void enqueue(
  register struct proc *rp	/* this process is now runnable */
)
{
/* Add 'rp' to one of the queues of runnable processes.  This function is 
 * responsible for inserting a process into one of the scheduling queues. 
 * The mechanism is implemented here.   The actual scheduling policy is
 * defined in sched() and pick_proc().
 */
  int q = rp->p_priority;	 		/* scheduling queue to use */

#if DEBUG_RACE
  /* With DEBUG_RACE, schedule everyone at the same priority level. */
  rp->p_priority = q = MIN_USER_Q;
#endif

  assert(proc_is_runnable(rp));

  assert(q >= 0);

  /* Now add the process to the queue. */
  if (!rdy_head[q]) {		/* add to empty queue */
      rdy_head[q] = rdy_tail[q] = rp; 		/* create a new queue */
      rp->p_nextready = NULL;		/* mark new end */
  } 
  else {					/* add to tail of queue */
      rdy_tail[q]->p_nextready = rp;		/* chain tail of queue */	
      rdy_tail[q] = rp;				/* set new queue tail */
      rp->p_nextready = NULL;		/* mark new end */
  }

  /*
   * enqueueing a process with a higher priority than the current one, it gets
   * preempted. The current process must be preemptible. Testing the priority
   * also makes sure that a process does not preempt itself
   */
  assert(proc_ptr && proc_ptr_ok(proc_ptr));
  if ((proc_ptr->p_priority > rp->p_priority) &&
		  (priv(proc_ptr)->s_flags & PREEMPTIBLE))
     RTS_SET(proc_ptr, RTS_PREEMPTED); /* calls dequeue() */

#if DEBUG_SANITYCHECKS
  assert(runqueues_ok());
#endif
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
  const int q = rp->p_priority;	 		/* scheduling queue to use */

  assert(proc_ptr_ok(rp));
  assert(proc_is_runnable(rp));

  /*
   * the process was runnable without its quantum expired when dequeued. A
   * process with no time left should vahe been handled else and differently
   */
  assert(!is_zero64(rp->p_cpu_time_left));

  assert(q >= 0);


  /* Now add the process to the queue. */
  if (!rdy_head[q]) {		/* add to empty queue */
      rdy_head[q] = rdy_tail[q] = rp; 		/* create a new queue */
      rp->p_nextready = NULL;		/* mark new end */
  }
  else						/* add to head of queue */
      rp->p_nextready = rdy_head[q];		/* chain head of queue */
      rdy_head[q] = rp;				/* set new queue head */

#if DEBUG_SANITYCHECKS
  assert(runqueues_ok());
#endif
}

/*===========================================================================*
 *				dequeue					     * 
 *===========================================================================*/
PUBLIC void dequeue(const struct proc *rp)
/* this process is no longer runnable */
{
/* A process must be removed from the scheduling queues, for example, because
 * it has blocked.  If the currently active process is removed, a new process
 * is picked to run by calling pick_proc().
 */
  register int q = rp->p_priority;		/* queue to use */
  register struct proc **xpp;			/* iterate over queue */
  register struct proc *prev_xp;

  assert(proc_ptr_ok(rp));
  assert(!proc_is_runnable(rp));

  /* Side-effect for kernel: check if the task's stack still is ok? */
  assert (!iskernelp(rp) || *priv(rp)->s_stack_guard == STACK_GUARD);

  /* Now make sure that the process is not in its ready queue. Remove the 
   * process if it is found. A process can be made unready even if it is not 
   * running by being sent a signal that kills it.
   */
  prev_xp = NULL;				
  for (xpp = &rdy_head[q]; *xpp; xpp = &(*xpp)->p_nextready) {
      if (*xpp == rp) {				/* found process to remove */
          *xpp = (*xpp)->p_nextready;		/* replace with next chain */
          if (rp == rdy_tail[q]) {		/* queue tail removed */
              rdy_tail[q] = prev_xp;		/* set new tail */
	  }

          break;
      }
      prev_xp = *xpp;				/* save previous in chain */
  }

#if DEBUG_SANITYCHECKS
  assert(runqueues_ok());
#endif
}

#if DEBUG_RACE
/*===========================================================================*
 *				random_process				     * 
 *===========================================================================*/
PRIVATE struct proc *random_process(struct proc *head)
{
	int i, n = 0;
	struct proc *rp;
	u64_t r;
	read_tsc_64(&r);

	for(rp = head; rp; rp = rp->p_nextready)
		n++;

	/* Use low-order word of TSC as pseudorandom value. */
	i = r.lo % n;

	for(rp = head; i--; rp = rp->p_nextready)
		;

	assert(rp);

	return rp;
}
#endif

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

#if DEBUG_RACE
	rp = random_process(rdy_head[q]);
#endif

	TRACE(VF_PICKPROC, printf("found %s / %d on queue %d\n", 
		rp->p_name, rp->p_endpoint, q););
	assert(proc_is_runnable(rp));
	if (priv(rp)->s_flags & BILLABLE)	 	
		bill_ptr = rp;		/* bill for system time */
	return rp;
  }
  return NULL;
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
const char *file;
int line;
#else
PUBLIC int isokendpt_f(e, p, fatalflag)
#endif
endpoint_t e;
int *p;
const int fatalflag;
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
		printf("kernel:%s:%d: bad endpoint %d: proc %d out of range\n",
		file, line, e, *p);
#endif
	} else if(isemptyn(*p)) {
#if 0
	printf("kernel:%s:%d: bad endpoint %d: proc %d empty\n", file, line, e, *p);
#endif
	} else if(proc_addr(*p)->p_endpoint != e) {
#if DEBUG_ENABLE_IPC_WARNINGS
		printf("kernel:%s:%d: bad endpoint %d: proc %d has ept %d (generation %d vs. %d)\n", file, line,
		e, *p, proc_addr(*p)->p_endpoint,
		_ENDPOINT_G(e), _ENDPOINT_G(proc_addr(*p)->p_endpoint));
#endif
	} else ok = 1;
	if(!ok && fatalflag) {
		panic("invalid endpoint: %d",  e);
	}
	return ok;
}

PRIVATE void notify_scheduler(struct proc *p)
{
	message m_no_quantum;
	int err;

	assert(!proc_kernel_scheduler(p));

	/* dequeue the process */
	RTS_SET(p, RTS_NO_QUANTUM);
	/*
	 * Notify the process's scheduler that it has run out of
	 * quantum. This is done by sending a message to the scheduler
	 * on the process's behalf
	 */
	m_no_quantum.m_source = p->p_endpoint;
	m_no_quantum.m_type   = SCHEDULING_NO_QUANTUM;

	if ((err = mini_send(p, p->p_scheduler->p_endpoint,
					&m_no_quantum, FROM_KERNEL))) {
		panic("WARNING: Scheduling: mini_send returned %d\n", err);
	}
}

PUBLIC void proc_no_time(struct proc * p)
{
	if (!proc_kernel_scheduler(p) && priv(p)->s_flags & PREEMPTIBLE) {
		/* this dequeues the process */
		notify_scheduler(p);
	}
	else {
		/*
		 * non-preemptible processes only need their quantum to
		 * be renewed. In fact, they by pass scheduling
		 */
		p->p_cpu_time_left = ms_2_cpu_time(p->p_quantum_size_ms);
#if DEBUG_RACE
		RTS_SET(proc_ptr, RTS_PREEMPTED);
		RTS_UNSET(proc_ptr, RTS_PREEMPTED);
#endif
	}
}
	
PUBLIC void copr_not_available_handler(void)
{
	/*
	 * Disable the FPU exception (both for the kernel and for the process
	 * once it's scheduled), and initialize or restore the FPU state.
	 */

	disable_fpu_exception();

	/* if FPU is not owned by anyone, do not store anything */
	if (fpu_owner != NULL) {
		assert(fpu_owner != proc_ptr);
		save_fpu(fpu_owner);
	}

	/*
	 * restore the current process' state and let it run again, do not
	 * schedule!
	 */
	restore_fpu(proc_ptr);
	fpu_owner = proc_ptr;
	context_stop(proc_addr(KERNEL));
	restore_user_context(proc_ptr);
	NOT_REACHABLE;
}

PUBLIC void release_fpu(void) {
	fpu_owner = NULL;
}
