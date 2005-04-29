/* This file contains essentially all of the process and message handling.
 * It has two main entry points from the outside:
 *
 *   sys_call:  a system call, that is, the kernel is trapped with an INT 
 *   notify:    notify process of a system event (notifications aren't queued)
 *
 * It also has several minor entry points:
 *
 *   lock_ready:      put a process on one of the ready queues so it can be run
 *   lock_unready:    remove a process from the ready queues
 *   lock_sched:      a process has run too long; schedule another one
 *   lock_pick_proc:  pick a process to run (used by system initialization)
 *   unhold:          repeat all held-up notifications
 *
 * Changes:
 *   Nov 05, 2004     removed lock_mini_send()  (Jorrit N. Herder)
 *   Oct 28, 2004     non-blocking SEND and RECEIVE  (Jorrit N. Herder)
 *   Oct 28, 2004     rewrite of sys_call()  (Jorrit N. Herder)
 *   Oct 10, 2004     require BOTH for kernel sys_call()  (Jorrit N. Herder)
 *		      (to protect kernel tasks from being blocked)
 *   Sep 25, 2004     generalized notify() function  (Jorrit N. Herder)
 *   Sep 23, 2004     removed PM sig check in mini_rec()  (Jorrit N. Herder)
 *   Aug 19, 2004     generalized ready()/unready()  (Jorrit N. Herder)
 *   Aug 18, 2004     added notify() function  (Jorrit N. Herder) 
 *   May 01, 2004     check p_sendmask in mini_send()  (Jorrit N. Herder) 
 */

#include "kernel.h"
#include <minix/callnr.h>
#include <minix/com.h>
#include "proc.h"
#include "sendmask.h"

PRIVATE unsigned char switching;	/* nonzero to inhibit notify() */

FORWARD _PROTOTYPE( int mini_send, (struct proc *caller_ptr, int dest,
		message *m_ptr, int may_block) );
FORWARD _PROTOTYPE( int mini_rec, (struct proc *caller_ptr, int src,
		message *m_ptr, int may_block) );
FORWARD _PROTOTYPE( void ready, (struct proc *rp) );
FORWARD _PROTOTYPE( void sched, (void) );
FORWARD _PROTOTYPE( void unready, (struct proc *rp) );
FORWARD _PROTOTYPE( void pick_proc, (void) );

#if (CHIP == M68000)
FORWARD _PROTOTYPE( void cp_mess, (int src, struct proc *src_p, message *src_m,
		struct proc *dst_p, message *dst_m) );
#endif

#if (CHIP == INTEL)
#define CopyMess(s,sp,sm,dp,dm) \
	cp_mess(s, (sp)->p_memmap[D].mem_phys, (vir_bytes)sm, (dp)->p_memmap[D].mem_phys, (vir_bytes)dm)
#endif /* (CHIP == INTEL) */

#if (CHIP == M68000)
/* M68000 does not have cp_mess() in assembly like INTEL. Declare prototype
 * for cp_mess() here and define the function below. Also define CopyMess. 
 */
#endif /* (CHIP == M68000) */


/* Bit mask operations used to bits of the notification mask. */
#define set_bit(mask, n)	((mask) |= (1 << (n)))
#define clear_bit(mask, n)	((mask) &= ~(1 << (n)))
#define isset_bit(mask, n)	((mask) & (1 << (n)))


/*===========================================================================*
 *				    notify				     * 
 *===========================================================================*/
PUBLIC void notify(proc_nr, notify_type)
int proc_nr;			/* number of process to be started */
int notify_type;		/* notification to be sent */
{
/* A system event has occurred. Send a notification with source HARDWARE to
 * the given process. The notify() function was carefully designed so that it
 * (1) can be used safely from both interrupt handlers and the task level, and
 * (2) realizes asynchronous message passing with at least once semantics, 
 * that is, the notifications are not queued. If a race condition occurs, the
 * notification is queued and repeated later by unhold(). If the receiver is
 * not ready, the notification is blocked and checked later in receive().   
 */
  register struct proc *rp;	/* pointer to task's proc entry */
  message m;			/* message to send the notification */
  unsigned int notify_bit;	/* bit for this notification */

  /* Get notify bit and process pointer. */
  notify_bit = (unsigned int) (notify_type - NOTIFICATION);
  rp = proc_addr(proc_nr);

  /* If this call would compete with other process-switching functions, put
   * it on the 'held' queue to be flushed at the next non-competing restart().
   * The competing conditions are:
   * (1) k_reenter == (typeof k_reenter) -1:
   *     Call from the task level, typically from an output interrupt 
   *     routine. An interrupt handler might reenter notify(). Rare,
   *     so not worth special treatment.
   * (2) k_reenter > 0:
   *     Call from a nested interrupt handler. A previous interrupt 
   *     handler might be inside notify() or sys_call().
   * (3) switching != 0:
   *     A process-switching function other than notify() is being called 
   *     from the task level, typically sched() from CLOCK. An interrupt
   *	 handler might call notify() and pass the 'k_reenter' test.
   */
  if (k_reenter != 0 || switching) {
	lock();
	if (! rp->p_ntf_held) {			/* already on held queue? */
		if (held_head != NIL_PROC)
			held_tail->p_ntf_nextheld = rp;
		else
			held_head = rp;
		held_tail = rp;
		rp->p_ntf_nextheld = NIL_PROC;
	}
	set_bit(rp->p_ntf_held, notify_bit);	/* add bit to held mask */
 	unlock();
	return;
  }
  switching = TRUE;

  /* If process is not waiting for a notification, record the blockage. */
  if ( (rp->p_flags & (RECEIVING | SENDING)) != RECEIVING ||
      !isrxhardware(rp->p_getfrom)) {
	set_bit(rp->p_ntf_blocked, notify_bit);	/* add bit to blocked mask */
	switching = FALSE;
	return;
  }

  /* Destination is waiting for a notification. Send it a message with source
   * HARDWARE and type 'notify_type'. No more information can be reliably 
   * provided since notifications are not queued.
   */
  m.m_source = HARDWARE;     	/* direct copy does not work for servers */
  m.m_type = notify_type;
  CopyMess(HARDWARE, proc_addr(HARDWARE), &m, rp, rp->p_messbuf);
  rp->p_flags &= ~RECEIVING;
  clear_bit(rp->p_ntf_blocked, notify_bit);

  /* Announce the process ready and select a fresh process to run. */
  ready(rp);			
  pick_proc();
  switching = FALSE;
}

/*===========================================================================*
 *				sys_call				     * 
 *===========================================================================*/
PUBLIC int sys_call(call_nr, src_dst, m_ptr)
int call_nr;			/* (NB_)SEND, (NB_)RECEIVE, BOTH */
int src_dst;			/* source to receive from or dest to send to */
message *m_ptr;			/* pointer to message in the caller's space */
{
/* System calls are done by trapping to the kernel with an INT instruction.
 * The trap is caught and sys_call() is called to send or receive a message
 * (or both). The caller is always given by 'proc_ptr'.
 */
  register struct proc *caller_ptr = proc_ptr;	/* get pointer to caller */
  int function = call_nr & SYSCALL_FUNC;	/* get system call function */
  int may_block = ! (call_nr & NON_BLOCKING);	/* (dis)allow blocking? */
  int mask_entry;				/* bit to check in send mask */
  int result;					/* the system call's result */

  /* Calls directed to the kernel may only be sendrec(), because tasks always
   * reply and may not block if the caller doesn't do receive(). Users also
   * may only use sendrec() to protect the process manager and file system.  
   */
  if ((iskernel(src_dst) || isuserp(caller_ptr)) && function != BOTH) {
      result = ECALLDENIED;			/* BOTH was required */
  }
  
  /* Verify that requested source and/ or destination is a valid process. */
  else if (! isoksrc_dst(src_dst)) {
      result = EBADSRCDST;			/* invalid process number */
  }

  /* Now check if the call is known and try to perform the request. The only
   * system calls that exist in MINIX are sending and receiving messages.
   * Receiving is straightforward. Sending requires checks to see if sending
   * is allowed by the caller's send mask and to see if the destination is
   * alive.  
   */
  else {
      switch(function) {
      case SEND:		
          /* fall through, SEND is done in BOTH */
      case BOTH:			
          if (! isalive(src_dst)) { 			
              result = EDEADDST;		/* cannot send to the dead */
              break;
          }
          mask_entry = isuser(src_dst) ? USER_PROC_NR : src_dst;
          if (! isallowed(caller_ptr->p_sendmask, mask_entry)) {
              kprintf("WARNING: sys_call denied %d ", caller_ptr->p_nr);
              kprintf("sending to %d\n", proc_addr(src_dst)->p_nr);
              result = ECALLDENIED;		/* call denied by send mask */
              break;
          } 
          result = mini_send(caller_ptr, src_dst, m_ptr, may_block);
          if (function == SEND || result != OK) {	
              break;				/* done, or SEND failed */
          }					/* fall through for BOTH */
      case RECEIVE:			
          result = mini_rec(caller_ptr, src_dst, m_ptr, may_block);
          break;
      default:
          result = EBADCALL;			/* illegal system call */
      }
  }

  /* Now, return the result of the system call to the caller. */
  return(result);
}


/*===========================================================================*
 *				mini_send				     * 
 *===========================================================================*/
PRIVATE int mini_send(caller_ptr, dest, m_ptr, may_block)
register struct proc *caller_ptr;	/* who is trying to send a message? */
int dest;				/* to whom is message being sent? */
message *m_ptr;				/* pointer to message buffer */
int may_block;				/* (dis)allow blocking */
{
/* Send a message from 'caller_ptr' to 'dest'. If 'dest' is blocked waiting
 * for this message, copy the message to it and unblock 'dest'. If 'dest' is
 * not waiting at all, or is waiting for another source, queue 'caller_ptr'.
 */
  register struct proc *dest_ptr, *next_ptr;
  vir_bytes vb;			/* message buffer pointer as vir_bytes */
  vir_clicks vlo, vhi;		/* virtual clicks containing message to send */

  dest_ptr = proc_addr(dest);	/* pointer to destination's proc entry */

#if ALLOW_GAP_MESSAGES
  /* This check allows a message to be anywhere in data or stack or gap. 
   * It will have to be made more elaborate later for machines which
   * don't have the gap mapped.
   */
  vb = (vir_bytes) m_ptr;
  vlo = vb >> CLICK_SHIFT;	/* vir click for bottom of message */
  vhi = (vb + MESS_SIZE - 1) >> CLICK_SHIFT;	/* vir click for top of msg */
  if (vlo < caller_ptr->p_memmap[D].mem_vir || vlo > vhi ||
      vhi >= caller_ptr->p_memmap[S].mem_vir + caller_ptr->p_memmap[S].mem_len)
        return(EFAULT); 
#else
  /* Check for messages wrapping around top of memory or outside data seg. */
  vb = (vir_bytes) m_ptr;
  vlo = vb >> CLICK_SHIFT;	/* vir click for bottom of message */
  vhi = (vb + MESS_SIZE - 1) >> CLICK_SHIFT;	/* vir click for top of msg */
  if (vhi < vlo ||
      vhi - caller_ptr->p_memmap[D].mem_vir >= caller_ptr->p_memmap[D].mem_len)
	return(EFAULT);
#endif

  /* Check for deadlock by 'caller_ptr' and 'dest' sending to each other. */
  if (dest_ptr->p_flags & SENDING) {
	next_ptr = proc_addr(dest_ptr->p_sendto);
	while (TRUE) {
		if (next_ptr == caller_ptr) return(ELOCKED);
		if (next_ptr->p_flags & SENDING)
			next_ptr = proc_addr(next_ptr->p_sendto);
		else
			break;
	}
  }

  /* Check to see if 'dest' is blocked waiting for this message. */
  if ( (dest_ptr->p_flags & (RECEIVING | SENDING)) == RECEIVING &&
       (dest_ptr->p_getfrom == ANY ||
        dest_ptr->p_getfrom == proc_number(caller_ptr))) {
	/* Destination is indeed waiting for this message. */
	CopyMess(proc_number(caller_ptr), caller_ptr, m_ptr, dest_ptr,
		 dest_ptr->p_messbuf);
	dest_ptr->p_flags &= ~RECEIVING;	/* deblock destination */
	if (dest_ptr->p_flags == 0) ready(dest_ptr);
  } else if (may_block) {
	/* Destination is not waiting.  Block and queue caller. */
	caller_ptr->p_messbuf = m_ptr;
	if (caller_ptr->p_flags == 0) unready(caller_ptr);
	caller_ptr->p_flags |= SENDING;
	caller_ptr->p_sendto= dest;

	/* Process is now blocked.  Put in on the destination's queue. */
	if ( (next_ptr = dest_ptr->p_callerq) == NIL_PROC)
		dest_ptr->p_callerq = caller_ptr;
	else {
		while (next_ptr->p_sendlink != NIL_PROC)
			next_ptr = next_ptr->p_sendlink;
		next_ptr->p_sendlink = caller_ptr;
	}
	caller_ptr->p_sendlink = NIL_PROC;
  } else {
	return(ENOTREADY);
  }
  return(OK);
}

/*===========================================================================*
 *				mini_rec				     * 
 *===========================================================================*/
PRIVATE int mini_rec(caller_ptr, src, m_ptr, may_block)
register struct proc *caller_ptr;	/* process trying to get message */
int src;				/* which message source is wanted */
message *m_ptr;				/* pointer to message buffer */
int may_block;				/* (dis)allow blocking */
{
/* A process or task wants to get a message.  If one is already queued,
 * acquire it and deblock the sender.  If no message from the desired source
 * is available, block the caller.  
 */
  register struct proc *sender_ptr;
  register struct proc *previous_ptr;
  message m;
  int i;

  /* Check to see if a message from desired source is already available. */
  if (!(caller_ptr->p_flags & SENDING)) {

    /* Check caller queue. */
    for (sender_ptr = caller_ptr->p_callerq; sender_ptr != NIL_PROC;
	 previous_ptr = sender_ptr, sender_ptr = sender_ptr->p_sendlink) {
	if (src == ANY || src == proc_number(sender_ptr)) {
		/* An acceptable message has been found. */
		CopyMess(proc_number(sender_ptr), sender_ptr,
			 sender_ptr->p_messbuf, caller_ptr, m_ptr);
		if (sender_ptr == caller_ptr->p_callerq)
			caller_ptr->p_callerq = sender_ptr->p_sendlink;
		else
			previous_ptr->p_sendlink = sender_ptr->p_sendlink;
		if ((sender_ptr->p_flags &= ~SENDING) == 0)
			ready(sender_ptr);	/* deblock sender */
		return(OK);
	}
    }

    /* Check bit mask for blocked notifications. If multiple bits are set, 
     * send the first notification encountered; the rest is handled later.
     * This effectively prioritizes notifications. Notification also have
     * priority of other messages. 
     */
    if (caller_ptr->p_ntf_blocked && isrxhardware(src)) {
        for (i=0; i<NR_NOTIFICATIONS; i++) {
            if (isset_bit(caller_ptr->p_ntf_blocked, i)) {
                m.m_source = HARDWARE;  
                m.m_type = NOTIFICATION + i;
                CopyMess(HARDWARE, proc_addr(HARDWARE), &m, caller_ptr, m_ptr);
	        clear_bit(caller_ptr->p_ntf_blocked, i);
	        return(OK);
	    }
	}
    }
  }

  /* No suitable message is available.  Block the process trying to receive,
   * unless this is not allowed by the system call.
   */
  if (may_block) {
      caller_ptr->p_getfrom = src;
      caller_ptr->p_messbuf = m_ptr;
      if (caller_ptr->p_flags == 0) unready(caller_ptr);
      caller_ptr->p_flags |= RECEIVING;
      return(OK);
  } else {
      return(ENOTREADY);
  }
}

/*===========================================================================*
 *				pick_proc				     * 
 *===========================================================================*/
PRIVATE void pick_proc()
{
/* Decide who to run now.  A new process is selected by setting 'proc_ptr'.
 * When a fresh user (or idle) process is selected, record it in 'bill_ptr',
 * so the clock task can tell who to bill for system time.
 */
  register struct proc *rp;			/* process to run */
  int q;					/* iterate over queues */

  /* Check each of the scheduling queues for ready processes. The number of
   * queues is defined in proc.h, and priorities are set in the task table.
   * The lowest queue contains IDLE, which is always ready.
   */
  for (q=0; q < NR_SCHED_QUEUES; q++) {	
    if ( (rp = rdy_head[q]) != NIL_PROC) {
	proc_ptr = rp;				/* run process 'rp' next */
	if (isuserp(rp) || isidlep(rp)) 	/* possible bill 'rp' */
		bill_ptr = rp;
	return;
    }
  }
}

/*===========================================================================*
 *				ready					     * 
 *===========================================================================*/
PRIVATE void ready(rp)
register struct proc *rp;	/* this process is now runnable */
{
/* Add 'rp' to one of the queues of runnable processes.  */
  int q = rp->p_priority;	/* scheduling queue to use */

  /* Processes, in principle, are added to the end of the queue. However, 
   * user processes are added in front of the queue, because this is a bit 
   * fairer to I/O bound processes. 
   */
  if (isuserp(rp)) {	/* add to front of queue */
  	if (rdy_head[q] == NIL_PROC)
		rdy_tail[q] = rp;
  	rp->p_nextready = rdy_head[q];		/* add to front of queue */
  	rdy_head[q] = rp;
  } 
  else {
      if (rdy_head[q] != NIL_PROC)
		rdy_tail[q]->p_nextready = rp;	/* add to end of queue */
      else 
		rdy_head[q] = rp;		/* add to empty queue */
      rdy_tail[q] = rp;
      rp->p_nextready = NIL_PROC;
  }

  /* Run 'rp' next if it has a higher priority than 'proc_ptr'. This actually
   * should be done via pick_proc(), but mini_send() and mini_rec() rely
   * on this side-effect.
   */
  if (rp->p_priority < proc_ptr->p_priority) proc_ptr = rp;
}

/*===========================================================================*
 *				unready					     * 
 *===========================================================================*/
PRIVATE void unready(rp)
register struct proc *rp;	/* this process is no longer runnable */
{
/* A process has blocked. See ready for a description of the queues. */

  register struct proc *xp;
  register struct proc **qtail; /* queue's rdy_tail */
  int q = rp->p_priority;	/* queue to use */

  /* Side-effect for tasks: check if the task's stack still is ok? */
  if (istaskp(rp)) { 				
	if (*rp->p_stguard != STACK_GUARD)
		panic("stack overrun by task", proc_number(rp));
  }

  /* Now make sure that the process is not in its ready queue. Remove the 
   * process if it is found. The easy part is to check the front of the queue. 
   */
  if ( (xp = rdy_head[q]) == NIL_PROC) return;
  if (xp == rp) {
	rdy_head[q] = xp->p_nextready;		/* remove head of queue */
	if (rp == proc_ptr) 			/* current process removed */
		pick_proc();			/* pick new process to run */
	return;
  }

  /* No match yet. Search body of queue. A process can be made unready even 
   * if it is not running by being sent a signal that kills it.
   */
  while (xp->p_nextready != rp)
	if ( (xp = xp->p_nextready) == NIL_PROC) return;
  xp->p_nextready = xp->p_nextready->p_nextready;
  qtail = &rdy_tail[q];
  if (*qtail == rp) *qtail = xp;
}

/*===========================================================================*
 *				sched					     * 
 *===========================================================================*/
PRIVATE void sched()
{
/* The current process has run too long. If another low priority (user)
 * process is runnable, put the current process on the end of the user queue,
 * possibly promoting another user to head of the queue.
 */
  if (rdy_head[PPRI_USER] == NIL_PROC) return;

  /* One or more user processes queued. */
  rdy_tail[PPRI_USER]->p_nextready = rdy_head[PPRI_USER];
  rdy_tail[PPRI_USER] = rdy_head[PPRI_USER];
  rdy_head[PPRI_USER] = rdy_head[PPRI_USER]->p_nextready;
  rdy_tail[PPRI_USER]->p_nextready = NIL_PROC;
  pick_proc();
}

/*==========================================================================*
 *				lock_pick_proc				    *
 *==========================================================================*/
PUBLIC void lock_pick_proc()
{
/* Safe gateway to pick_proc() for tasks. */

  switching = TRUE;
  pick_proc();
  switching = FALSE;
}

/*==========================================================================*
 *				lock_ready				    *
 *==========================================================================*/
PUBLIC void lock_ready(rp)
struct proc *rp;		/* this process is now runnable */
{
/* Safe gateway to ready() for tasks. */

  switching = TRUE;
  ready(rp);
  switching = FALSE;
}

/*==========================================================================*
 *				lock_unready				    *
 *==========================================================================*/
PUBLIC void lock_unready(rp)
struct proc *rp;		/* this process is no longer runnable */
{
/* Safe gateway to unready() for tasks. */

  switching = TRUE;
  unready(rp);
  switching = FALSE;
}

/*==========================================================================*
 *				lock_sched				    *
 *==========================================================================*/
PUBLIC void lock_sched()
{
/* Safe gateway to sched() for tasks. */

  switching = TRUE;
  sched();
  switching = FALSE;
}

/*==========================================================================*
 *				unhold					    *
 *==========================================================================*/
PUBLIC void unhold()
{
/* Flush any held-up notifications. 'k_reenter' must be 0. 'held_head' must 
 * not be NIL_PROC.  Interrupts must be disabled.  They will be enabled but 
 * will be disabled when this returns.
 */
  register struct proc *rp;	/* current head of held queue */
  int i;

  if (switching) return;
  rp = held_head;
  do {
      for (i=0; i<NR_NOTIFICATIONS; i++) {
          if (isset_bit(rp->p_ntf_held,i)) {
              clear_bit(rp->p_ntf_held,i);
              if (! rp->p_ntf_held)	/* proceed to next in queue? */
                  if ( (held_head = rp->p_ntf_nextheld) == NIL_PROC)
                      held_tail = NIL_PROC;
              unlock();		/* reduce latency; held queue may change! */
              notify(proc_number(rp), NOTIFICATION + i);
              lock();		/* protect the held queue again */
          }
      }
  }
  while ( (rp = held_head) != NIL_PROC);
}

#if (CHIP == M68000)
/*==========================================================================*
 *				cp_mess					    *
 *==========================================================================*/
PRIVATE void cp_mess(src, src_p, src_m, dst_p, dst_m)
int src;			/* sender process */
register struct proc *src_p;	/* source proc entry */
message *src_m;			/* source message */
register struct proc *dst_p;	/* destination proc entry */
message *dst_m;			/* destination buffer */
{
  /* convert virtual address to physical address */
  /* The caller has already checked if all addresses are within bounds */
  
  src_m = (message *)((char *)src_m + (((phys_bytes)src_p->p_map[D].mem_phys
				- src_p->p_map[D].mem_vir) << CLICK_SHIFT));
  dst_m = (message *)((char *)dst_m + (((phys_bytes)dst_p->p_map[D].mem_phys
				- dst_p->p_map[D].mem_vir) << CLICK_SHIFT));

#ifdef NEEDFSTRUCOPY
  phys_copy(src_m,dst_m,(phys_bytes) sizeof(message));
#else
  *dst_m = *src_m;
#endif
  dst_m->m_source = src;
}
#endif

