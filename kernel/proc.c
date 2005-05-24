/* This file contains essentially all of the process and message handling.
 * It has one main entry point from the outside:
 *
 *   sys_call: 	      a system call, i.e., the kernel is trapped with an INT 
 *
 * It also has several minor entry points to be used from the task level:
 *
 *   lock_notify:     send a notification to inform a process of a system event
 *   lock_send:	      send a message to a process
 *   lock_ready:      put a process on one of the ready queues so it can be run
 *   lock_unready:    remove a process from the ready queues
 *   lock_sched:      a process has run too long; schedule another one
 *   lock_pick_proc:  pick a process to run (used by system initialization)
 *
 * Changes:
 *   May 24, 2005     new, queued NOTIFY system call  (Jorrit N. Herder)
 *   Oct 28, 2004     non-blocking SEND and RECEIVE  (Jorrit N. Herder)
 *   Oct 28, 2004     rewrite of sys_call()  (Jorrit N. Herder)
 *   Oct 10, 2004     require BOTH for kernel sys_call()  (Jorrit N. Herder)
 *		      (to protect kernel tasks from being blocked)
 *   Aug 19, 2004     generalized ready()/unready()  (Jorrit N. Herder)
 */

#include "kernel.h"
#include <minix/callnr.h>
#include <minix/com.h>
#include "proc.h"
#include "sendmask.h"


/* Scheduling and message passing functions. The functions are available to 
 * other parts of the kernel through lock_...(). The lock temporarily disables 
 * interrupts to prevent race conditions. 
 */
FORWARD _PROTOTYPE( int mini_send, (struct proc *caller_ptr, int dst,
		message *m_ptr, int may_block) );
FORWARD _PROTOTYPE( int mini_rec, (struct proc *caller_ptr, int src,
		message *m_ptr, int may_block) );
FORWARD _PROTOTYPE( int mini_notify, (struct proc *caller_ptr, int dst,
		message *m_ptr ) );

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


/* Declare buffer space and a bit map for notification messages. */
PRIVATE struct notification notify_buffer[NR_NOTIFY_BUFS];
PRIVATE bitchunk_t notify_bitmap[BITMAP_CHUNKS(NR_NOTIFY_BUFS)];     


/*===========================================================================*
 *				sys_call				     * 
 *===========================================================================*/
PUBLIC int sys_call(call_nr, src_dst, m_ptr)
int call_nr;			/* (NB_)SEND, (NB_)RECEIVE, BOTH */
int src_dst;			/* src to receive from or dst to send to */
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
#if DEAD_CODE
  if ((iskernel(src_dst) || isuserp(caller_ptr)) && function != BOTH) {
#else
  if (iskernel(src_dst) && function != BOTH) {
#endif
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
      case NOTIFY:
          result = mini_notify(caller_ptr, src_dst, m_ptr);
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
PRIVATE int mini_send(caller_ptr, dst, m_ptr, may_block)
register struct proc *caller_ptr;	/* who is trying to send a message? */
int dst;				/* to whom is message being sent? */
message *m_ptr;				/* pointer to message buffer */
int may_block;				/* (dis)allow blocking */
{
/* Send a message from 'caller_ptr' to 'dst'. If 'dst' is blocked waiting
 * for this message, copy the message to it and unblock 'dst'. If 'dst' is
 * not waiting at all, or is waiting for another source, queue 'caller_ptr'.
 */
  register struct proc *dst_ptr, *next_ptr;
  vir_bytes vb;			/* message buffer pointer as vir_bytes */
  vir_clicks vlo, vhi;		/* virtual clicks containing message to send */

  dst_ptr = proc_addr(dst);	/* pointer to destination's proc entry */

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

  /* Check for deadlock by 'caller_ptr' and 'dst' sending to each other. */
  if (dst_ptr->p_flags & SENDING) {
	next_ptr = proc_addr(dst_ptr->p_sendto);
	while (TRUE) {
		if (next_ptr == caller_ptr) return(ELOCKED);
		if (next_ptr->p_flags & SENDING)
			next_ptr = proc_addr(next_ptr->p_sendto);
		else
			break;
	}
  }

  /* Check to see if 'dst' is blocked waiting for this message. */
  if ( (dst_ptr->p_flags & (RECEIVING | SENDING)) == RECEIVING &&
       (dst_ptr->p_getfrom == ANY ||
        dst_ptr->p_getfrom == proc_number(caller_ptr))) {
	/* Destination is indeed waiting for this message. */
	CopyMess(proc_number(caller_ptr), caller_ptr, m_ptr, dst_ptr,
		 dst_ptr->p_messbuf);
	dst_ptr->p_flags &= ~RECEIVING;	/* deblock destination */
	if (dst_ptr->p_flags == 0) ready(dst_ptr);
  } else if (may_block) {
	/* Destination is not waiting.  Block and queue caller. */
	caller_ptr->p_messbuf = m_ptr;
	if (caller_ptr->p_flags == 0) unready(caller_ptr);
	caller_ptr->p_flags |= SENDING;
	caller_ptr->p_sendto = dst;

	/* Process is now blocked.  Put in on the destination's queue. */
	if ( (next_ptr = dst_ptr->p_caller_q) == NIL_PROC)
		dst_ptr->p_caller_q = caller_ptr;
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
  register struct notification **ntf_q_pp;
  message m;
  int bit_nr, i;

  /* Check to see if a message from desired source is already available. */
  if (!(caller_ptr->p_flags & SENDING)) {

    /* Check caller queue. */
    for (sender_ptr = caller_ptr->p_caller_q; sender_ptr != NIL_PROC;
	 previous_ptr = sender_ptr, sender_ptr = sender_ptr->p_sendlink) {
	if (src == ANY || src == proc_number(sender_ptr)) {
		/* An acceptable message has been found. */
		CopyMess(proc_number(sender_ptr), sender_ptr,
			 sender_ptr->p_messbuf, caller_ptr, m_ptr);
		if (sender_ptr == caller_ptr->p_caller_q)
			caller_ptr->p_caller_q = sender_ptr->p_sendlink;
		else
			previous_ptr->p_sendlink = sender_ptr->p_sendlink;
		if ((sender_ptr->p_flags &= ~SENDING) == 0)
			ready(sender_ptr);	/* deblock sender */
		return(OK);
	}
    }

    /* Check if there are pending notifications. */
    ntf_q_pp = &caller_ptr->p_ntf_q;		/* get pointer pointer */
    while (*ntf_q_pp) {
	if (src == ANY || src == (*ntf_q_pp)->n_source) {
		/* Found notification. Assemble and copy message. */
		m.NOTIFY_SOURCE = (*ntf_q_pp)->n_source;
		m.NOTIFY_TYPE = (*ntf_q_pp)->n_type;
		m.NOTIFY_FLAGS = (*ntf_q_pp)->n_flags;
		m.NOTIFY_ARG = (*ntf_q_pp)->n_arg;
                CopyMess((*ntf_q_pp)->n_source, proc_addr(HARDWARE), &m, 
                	caller_ptr, m_ptr);
                /* Remove notification from queue and return. */
                bit_nr = ((long)(*ntf_q_pp) - (long) &notify_buffer[0]) / 
                	 sizeof(struct notification);
                *ntf_q_pp = (*ntf_q_pp)->n_next;/* remove from queue */
                free_bit(bit_nr, notify_bitmap, NR_NOTIFY_BUFS);
                return(OK);			/* report success */
	}
	ntf_q_pp = &(*ntf_q_pp)->n_next;	/* proceed to next */
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
 *				mini_notify				     * 
 *===========================================================================*/
PRIVATE int mini_notify(caller_ptr, dst, m_ptr)
register struct proc *caller_ptr;	/* process trying to notify */
int dst;				/* which process to notify */
message *m_ptr;				/* pointer to message buffer */
{
  register struct proc *dst_ptr = proc_addr(dst);
  register struct notification *ntf_p ;
  register struct notification **ntf_q_pp;
  int ntf_index;
  message ntf_mess;

  /* Check to see if target is blocked waiting for this message. */
  if ( (dst_ptr->p_flags & (RECEIVING | SENDING)) == RECEIVING &&
       (dst_ptr->p_getfrom == ANY ||
        dst_ptr->p_getfrom == proc_number(caller_ptr))) {

	/* Destination is indeed waiting for this message. */
	CopyMess(proc_number(caller_ptr), caller_ptr, m_ptr, dst_ptr,
		 dst_ptr->p_messbuf);
	dst_ptr->p_flags &= ~RECEIVING;	/* deblock destination */
	if (dst_ptr->p_flags == 0) ready(dst_ptr);
  } 
  /* Destination is not ready. Add the notification to the pending queue. */
  else {
	/* Get pointer to notification message. */
	if (! istaskp(caller_ptr)) {
	    CopyMess(proc_number(caller_ptr), caller_ptr, m_ptr, 
		 proc_addr(HARDWARE), &ntf_mess);
	    m_ptr = &ntf_mess;
	}

	/* Enqueue the message. Existing notifications are overwritten with 
	 * the newer one. New notifications are added to the end of the list.
	 */
	ntf_q_pp = &dst_ptr->p_ntf_q;
	while (*ntf_q_pp) {
		/* Replace notifications with same source and type. */
		if ((*ntf_q_pp)->n_type == m_ptr->m_type && 
		    (*ntf_q_pp)->n_source == m_ptr->m_source) {
			(*ntf_q_pp)->n_flags = m_ptr->NOTIFY_FLAGS;
			(*ntf_q_pp)->n_arg = m_ptr->NOTIFY_ARG;
			break;
		}
		return(OK);
	}

  	/* Add to end of queue. Get a free notification buffer. */
  	if ((ntf_index = alloc_bit(notify_bitmap, NR_NOTIFY_BUFS)) < 0) 
  		return(ENOSPC);		 	/* should be atomic! */
	ntf_p = &notify_buffer[ntf_index];
	ntf_p->n_source = proc_number(caller_ptr);
	ntf_p->n_type = m_ptr->NOTIFY_TYPE;
	ntf_p->n_flags = m_ptr->NOTIFY_FLAGS;
	ntf_p->n_arg = m_ptr->NOTIFY_ARG;
	*ntf_q_pp = ntf_p;
  }
  return(OK);
}

/*==========================================================================*
 *				lock_notify				    *
 *==========================================================================*/
PUBLIC int lock_notify(src, dst, m_ptr)
int src;			/* who is trying to send a message? */
int dst;			/* to whom is message being sent? */
message *m_ptr;			/* pointer to message buffer */
{
/* Safe gateway to mini_notify() for tasks. */
  int result;
  lock();
  result = mini_notify(proc_addr(src), dst, m_ptr); 
  unlock();
  return(result);
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
   * should be done via pick_proc(), but the message passing functions rely
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
  register struct proc **qtail; 	/* queue's rdy_tail */
  int q = rp->p_priority;		/* queue to use */

  /* Side-effect for tasks: check if the task's stack still is ok? */
  if (istaskp(rp)) { 				
	if (*rp->p_stguard != STACK_GUARD)
		panic("stack overrun by task", proc_number(rp));
  }

  /* Now make sure that the process is not in its ready queue. Remove the 
   * process if it is found. A process can be made unready even if it is not 
   * running by being sent a signal that kills it.
   */
  if ( (xp = rdy_head[q]) != NIL_PROC) {	/* ready queue is empty */
      if (xp == rp) {				/* check head of queue */
          rdy_head[q] = xp->p_nextready;	/* new head of queue */
          if (rp == proc_ptr) 			/* current process removed */
              pick_proc();			/* pick new process to run */
      } 
      else {					/* check body of queue */
          while (xp->p_nextready != rp)		/* stop if process is next */
              if ( (xp = xp->p_nextready) == NIL_PROC) 
                  return;	
          xp->p_nextready = xp->p_nextready->p_nextready;
          if (rdy_tail[q] == rp) 		/* possibly update tail */
              rdy_tail[q] = rp;
      }
  }
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
  lock();
  pick_proc();
  unlock();
}


/*==========================================================================*
 *				lock_send				    *
 *==========================================================================*/
PUBLIC int lock_send(src, dst, m_ptr)
int src;			/* who is trying to send a message? */
int dst;			/* to whom is message being sent? */
message *m_ptr;			/* pointer to message buffer */
{
/* Safe gateway to mini_send() for tasks. */
  int result;
  lock();
  result = mini_send(proc_addr(src), dst, m_ptr, FALSE);
  unlock();
  return(result);
}


/*==========================================================================*
 *				lock_ready				    *
 *==========================================================================*/
PUBLIC void lock_ready(rp)
struct proc *rp;		/* this process is now runnable */
{
/* Safe gateway to ready() for tasks. */
  lock();
  ready(rp);
  unlock();
}

/*==========================================================================*
 *				lock_unready				    *
 *==========================================================================*/
PUBLIC void lock_unready(rp)
struct proc *rp;		/* this process is no longer runnable */
{
/* Safe gateway to unready() for tasks. */
  lock();
  unready(rp);
  unlock();
}

/*==========================================================================*
 *				lock_sched				    *
 *==========================================================================*/
PUBLIC void lock_sched()
{
/* Safe gateway to sched() for tasks. */
  lock();
  sched();
  unlock();
}

