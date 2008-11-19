/* This file contains essentially all of the process and message handling.
 * Together with "mpx.s" it forms the lowest layer of the MINIX kernel.
 * There is one entry point from the outside:
 *
 *   sys_call: 	      a system call, i.e., the kernel is trapped with an INT
 *
 * As well as several entry points used from the interrupt and task level:
 *
 *   lock_notify:     notify a process of a system event
 *   lock_send:	      send a message to a process
 *   lock_enqueue:    put a process on one of the scheduling queues 
 *   lock_dequeue:    remove a process from the scheduling queues
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
#include <minix/callnr.h>
#include <minix/endpoint.h>
#include <stddef.h>
#include <signal.h>
#include <minix/portio.h>
#include <minix/u64.h>

#include "debug.h"
#include "kernel.h"
#include "proc.h"
#include "vm.h"

/* Scheduling and message passing functions. The functions are available to 
 * other parts of the kernel through lock_...(). The lock temporarily disables 
 * interrupts to prevent race conditions. 
 */
FORWARD _PROTOTYPE( int mini_send, (struct proc *caller_ptr, int dst_e,
		message *m_ptr, int flags));
FORWARD _PROTOTYPE( int mini_receive, (struct proc *caller_ptr, int src,
		message *m_ptr, int flags));
FORWARD _PROTOTYPE( int mini_notify, (struct proc *caller_ptr, int dst));
FORWARD _PROTOTYPE( int mini_senda, (struct proc *caller_ptr,
	asynmsg_t *table, size_t size));
FORWARD _PROTOTYPE( int deadlock, (int function,
		register struct proc *caller, int src_dst));
FORWARD _PROTOTYPE( int try_async, (struct proc *caller_ptr));
FORWARD _PROTOTYPE( int try_one, (struct proc *src_ptr, struct proc *dst_ptr));
FORWARD _PROTOTYPE( void sched, (struct proc *rp, int *queue, int *front));
FORWARD _PROTOTYPE( void pick_proc, (void));

#define BuildMess(m_ptr, src, dst_ptr) \
	(m_ptr)->m_source = proc_addr(src)->p_endpoint;		\
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

#define CopyMess(s,sp,sm,dp,dm) do { 			\
	vir_bytes dstlin;				\
	endpoint_t e = proc_addr(s)->p_endpoint;	\
	struct vir_addr src, dst;			\
	int r;						\
	timer_start(0, "copymess");			\
	if((dstlin = umap_local((dp), D, (vir_bytes) dm, sizeof(message))) == 0){\
		minix_panic("CopyMess: umap_local failed", __LINE__);	\
	}						\
			\
	if(vm_running &&	\
	 (r=vm_checkrange((dp), (dp), dstlin, sizeof(message), 1, 0)) != OK) { \
		if(r != VMSUSPEND) 			\
		  minix_panic("CopyMess: vm_checkrange error", __LINE__); \
		(dp)->p_vmrequest.saved.msgcopy.dst = (dp);	\
		(dp)->p_vmrequest.saved.msgcopy.dst_v = (vir_bytes) dm;	\
  		if(data_copy((sp)->p_endpoint,	\
			(vir_bytes) (sm), SYSTEM,	\
			(vir_bytes) &(dp)->p_vmrequest.saved.msgcopy.msgbuf, \
			sizeof(message)) != OK) {		\
				minix_panic("CopyMess: data_copy failed", __LINE__);\
			}				\
			(dp)->p_vmrequest.saved.msgcopy.msgbuf.m_source = e; \
			(dp)->p_vmrequest.type = VMSTYPE_MSGCOPY; \
	} else 	{					\
		src.proc_nr_e = (sp)->p_endpoint;		\
		dst.proc_nr_e = (dp)->p_endpoint;		\
		src.segment = dst.segment = D;			\
		src.offset = (vir_bytes) (sm);			\
		dst.offset = (vir_bytes) (dm);			\
		if(virtual_copy(&src, &dst, sizeof(message)) != OK) {	\
			kprintf("copymess: copy %d:%lx to %d:%lx failed\n",\
				(sp)->p_endpoint, (sm), (dp)->p_endpoint, dm);\
			minix_panic("CopyMess: virtual_copy (1) failed", __LINE__); \
		}		\
		src.proc_nr_e = SYSTEM;				\
		src.offset = (vir_bytes) &e;			\
		if(virtual_copy(&src, &dst, sizeof(e)) != OK) {		\
			kprintf("copymess: copy %d:%lx to %d:%lx\n",	\
				(sp)->p_endpoint, (sm), (dp)->p_endpoint, dm);\
			minix_panic("CopyMess: virtual_copy (2) failed", __LINE__); \
		}					\
	}	\
	timer_end(0);	\
} while(0)

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
  int mask_entry;				/* bit to check in send mask */
  int group_size;				/* used for deadlock check */
  int result;					/* the system call's result */
  int src_dst_p;				/* Process slot number */
  size_t msg_size;

  if (caller_ptr->p_endpoint == ipc_stats_target)
	ipc_stats.total= add64u(ipc_stats.total, 1);

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

#if 1
  if (RTS_ISSET(caller_ptr, SLOT_FREE))
  {
	kprintf("called by the dead?!?\n");
	if (caller_ptr->p_endpoint == ipc_stats_target)
		ipc_stats.deadproc++;
	return EINVAL;
  }
#endif

  /* Check destination. SENDA is special because its argument is a table and
   * not a single destination. RECEIVE is the only call that accepts ANY (in
   * addition to a real endpoint). The other calls (SEND, SENDREC,
   * and NOTIFY) require an endpoint to corresponds to a process. In addition,
   * it is necessary to check whether a process is allowed to send to a given
   * destination. For SENDREC we check s_ipc_sendrec, and for SEND,
   * and NOTIFY we check s_ipc_to.
   */
  if (call_nr == SENDA)
  {
	/* No destination argument */
  }
  else if (src_dst_e == ANY)
  {
	if (call_nr != RECEIVE)
	{
#if DEBUG_ENABLE_IPC_WARNINGS
		kprintf("sys_call: trap %d by %d with bad endpoint %d\n", 
			call_nr, proc_nr(caller_ptr), src_dst_e);
#endif
  		if (caller_ptr->p_endpoint == ipc_stats_target)
			ipc_stats.bad_endpoint++;
		return EINVAL;
	}
	src_dst_p = src_dst_e;
  }
  else
  {
	/* Require a valid source and/or destination process. */
	if(!isokendpt(src_dst_e, &src_dst_p)) {
#if DEBUG_ENABLE_IPC_WARNINGS
		kprintf("sys_call: trap %d by %d with bad endpoint %d\n", 
			call_nr, proc_nr(caller_ptr), src_dst_e);
#endif
  		if (caller_ptr->p_endpoint == ipc_stats_target)
			ipc_stats.bad_endpoint++;
		return EDEADSRCDST;
	}

	/* If the call is to send to a process, i.e., for SEND,
	 * SENDREC or NOTIFY, verify that the caller is allowed to send to
	 * the given destination. 
	 */
	if (call_nr == SENDREC)
	{
		if (! get_sys_bit(priv(caller_ptr)->s_ipc_sendrec,
			nr_to_id(src_dst_p))) {
#if DEBUG_ENABLE_IPC_WARNINGS
			kprintf(
	"sys_call: ipc sendrec mask denied trap %d from %d ('%s') to %d\n",
				call_nr, proc_nr(caller_ptr),
				caller_ptr->p_name, src_dst_p);
#endif
			if (caller_ptr->p_endpoint == ipc_stats_target)
				ipc_stats.dst_not_allowed++;
			return(ECALLDENIED);	/* call denied by ipc mask */
		}
	}
	else if (call_nr == SEND || call_nr == SENDNB || call_nr == NOTIFY)
	{
		if (! get_sys_bit(priv(caller_ptr)->s_ipc_to,
			nr_to_id(src_dst_p))) {
#if DEBUG_ENABLE_IPC_WARNINGS
			kprintf(
			"sys_call: ipc mask denied trap %d from %d to %d\n",
				call_nr, proc_nr(caller_ptr), src_dst_p);
#endif
			if (caller_ptr->p_endpoint == ipc_stats_target)
				ipc_stats.dst_not_allowed++;
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
	if (caller_ptr->p_endpoint == ipc_stats_target)
		ipc_stats.bad_call++;
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
	if (caller_ptr->p_endpoint == ipc_stats_target)
		ipc_stats.call_not_allowed++;
	return(ETRAPDENIED);		/* trap denied by mask or kernel */
  }

  if ((iskerneln(src_dst_p) && call_nr != SENDREC && call_nr != RECEIVE)) {
#if DEBUG_ENABLE_IPC_WARNINGS
      kprintf("sys_call: trap %d not allowed, caller %d, src_dst %d\n", 
          call_nr, proc_nr(caller_ptr), src_dst_e);
#endif
	if (caller_ptr->p_endpoint == ipc_stats_target)
		ipc_stats.call_not_allowed++;
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

  /* If the call involves a message buffer, i.e., for SEND, SENDREC, 
   * or RECEIVE, check the message pointer. This check allows a message to be 
   * anywhere in data or stack or gap. It will have to be made more elaborate 
   * for machines which don't have the gap mapped. 
   *
   * We use msg_size decided above.
   */
  if (call_nr == SEND || call_nr == SENDREC ||
	call_nr == RECEIVE || call_nr == SENDA || call_nr == SENDNB) {
	int r;
	phys_bytes lin;

	/* Map to linear address. */
	if((lin = umap_local(caller_ptr, D, (vir_bytes) m_ptr, msg_size)) == 0)
		return EFAULT;

	/* Check if message pages in calling process are mapped.
	 * We don't have to check the recipient if this is a send,
	 * because this code will do that before its receive() starts.
	 *
	 * It is important the range is verified as _writable_, because
	 * the kernel will want to write to the SENDA buffer in the future,
	 * and those pages may not be shared between processes.
	 */

	if(vm_running &&
	 (r=vm_checkrange(caller_ptr, caller_ptr, lin, msg_size, 1, 0)) != OK) {
		if(r != VMSUSPEND) {
			kprintf("SYSTEM:sys_call:vm_checkrange: err %d\n", r);
			return r;
		}
		minix_panic("vmsuspend", __LINE__);
		
		/* We can't go ahead with this call. Caller is suspended
		 * and we have to save the state in its process struct.
		 */
		caller_ptr->p_vmrequest.saved.sys_call.call_nr = call_nr;
		caller_ptr->p_vmrequest.saved.sys_call.m_ptr = m_ptr;
		caller_ptr->p_vmrequest.saved.sys_call.src_dst_e = src_dst_e;
		caller_ptr->p_vmrequest.saved.sys_call.bit_map = bit_map;
		caller_ptr->p_vmrequest.type = VMSTYPE_SYS_CALL;

		kprintf("SYSTEM: %s:%d: suspending call 0x%lx on ipc buffer 0x%lx\n",
			caller_ptr->p_name, caller_ptr->p_endpoint, call_nr, m_ptr);

		/* vm_checkrange() will have suspended caller with VMREQUEST. */
		return OK;
	}

  } 

  /* Check for a possible deadlock for blocking SEND(REC) and RECEIVE. */
  if (call_nr == SEND || call_nr == SENDREC || call_nr == RECEIVE) {
      if (group_size = deadlock(call_nr, caller_ptr, src_dst_p)) {
#if 0
          kprintf("sys_call: trap %d from %d to %d deadlocked, group size %d\n",
              call_nr, proc_nr(caller_ptr), src_dst_p, group_size);
#endif
	if (caller_ptr->p_endpoint == ipc_stats_target)
		ipc_stats.deadlock++;
        return(ELOCKED);
      }
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
	caller_ptr->p_misc_flags |= REPLY_PENDING;
	/* fall through */
  case SEND:			
	result = mini_send(caller_ptr, src_dst_e, m_ptr, 0);
	if (call_nr == SEND || result != OK)
		break;				/* done, or SEND failed */
	/* fall through for SENDREC */
  case RECEIVE:			
	if (call_nr == RECEIVE)
		caller_ptr->p_misc_flags &= ~REPLY_PENDING;
	result = mini_receive(caller_ptr, src_dst_e, m_ptr, 0);
	break;
  case NOTIFY:
	result = mini_notify(caller_ptr, src_dst_p);
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
int src_dst;					/* src or dst process */
{
/* Check for deadlock. This can happen if 'caller_ptr' and 'src_dst' have
 * a cyclic dependency of blocking send and receive calls. The only cyclic 
 * depency that is not fatal is if the caller and target directly SEND(REC)
 * and RECEIVE to each other. If a deadlock is found, the group size is 
 * returned. Otherwise zero is returned. 
 */
  register struct proc *xp;			/* process pointer */
  int group_size = 1;				/* start with only caller */
  int trap_flags;
#if DEBUG_ENABLE_IPC_WARNINGS
  static struct proc *processes[NR_PROCS + NR_TASKS];
  processes[0] = cp;
#endif

  while (src_dst != ANY) { 			/* check while process nr */
      int src_dst_e;
      xp = proc_addr(src_dst);			/* follow chain of processes */
#if DEBUG_ENABLE_IPC_WARNINGS
      processes[group_size] = xp;
#endif
      group_size ++;				/* extra process in group */

      /* Check whether the last process in the chain has a dependency. If it 
       * has not, the cycle cannot be closed and we are done.
       */
      if (RTS_ISSET(xp, RECEIVING)) {	/* xp has dependency */
	  if(xp->p_getfrom_e == ANY) src_dst = ANY;
	  else okendpt(xp->p_getfrom_e, &src_dst);
      } else if (RTS_ISSET(xp, SENDING)) {	/* xp has dependency */
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
	      if ((xp->p_rts_flags ^ (function << 2)) & SENDING) { 
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
 *				sys_call_restart			     * 
 *===========================================================================*/
PUBLIC void sys_call_restart(caller)
struct proc *caller;
{
	int r;
	minix_panic("sys_call_restart", NO_NUM);
	kprintf("restarting sys_call code 0x%lx, "
		"m_ptr 0x%lx, srcdst %d, bitmap 0x%lx, but not really\n",
		caller->p_vmrequest.saved.sys_call.call_nr,
		caller->p_vmrequest.saved.sys_call.m_ptr,
		caller->p_vmrequest.saved.sys_call.src_dst_e,
		caller->p_vmrequest.saved.sys_call.bit_map);
	caller->p_reg.retreg = r;
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

  dst_p = _ENDPOINT_P(dst_e);
  dst_ptr = proc_addr(dst_p);

  if (RTS_ISSET(dst_ptr, NO_ENDPOINT))
  {
	if (caller_ptr->p_endpoint == ipc_stats_target)
		ipc_stats.dst_died++;
	return EDSTDIED;
  }

  /* Check if 'dst' is blocked waiting for this message. The destination's 
   * SENDING flag may be set when its SENDREC call blocked while sending.  
   */
  if (WILLRECEIVE(dst_ptr, caller_ptr->p_endpoint)) {
	/* Destination is indeed waiting for this message. */
	CopyMess(caller_ptr->p_nr, caller_ptr, m_ptr, dst_ptr,
		 dst_ptr->p_messbuf);
	RTS_UNSET(dst_ptr, RECEIVING);
  } else {
	if(flags & NON_BLOCKING) {
		if (caller_ptr->p_endpoint == ipc_stats_target)
			ipc_stats.not_ready++;
		return(ENOTREADY);
	}

	/* Destination is not waiting.  Block and dequeue caller. */
	caller_ptr->p_messbuf = m_ptr;
	RTS_SET(caller_ptr, SENDING);
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
  register struct notification **ntf_q_pp;
  message m;
  int bit_nr;
  sys_map_t *map;
  bitchunk_t *chunk;
  int i, r, src_id, src_proc_nr, src_p;

  if(src_e == ANY) src_p = ANY;
  else
  {
	okendpt(src_e, &src_p);
	if (RTS_ISSET(proc_addr(src_p), NO_ENDPOINT))
	{
		if (caller_ptr->p_endpoint == ipc_stats_target)
			ipc_stats.src_died++;
		return ESRCDIED;
	}
  }


  /* Check to see if a message from desired source is already available.
   * The caller's SENDING flag may be set if SENDREC couldn't send. If it is
   * set, the process should be blocked.
   */
  if (!RTS_ISSET(caller_ptr, SENDING)) {

    /* Check if there are pending notifications, except for SENDREC. */
    if (! (caller_ptr->p_misc_flags & REPLY_PENDING)) {

        map = &priv(caller_ptr)->s_notify_pending;
        for (chunk=&map->chunk[0]; chunk<&map->chunk[NR_SYS_CHUNKS]; chunk++) {

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
	    BuildMess(&m, src_proc_nr, caller_ptr);	/* assemble message */
            CopyMess(src_proc_nr, proc_addr(HARDWARE), &m, caller_ptr, m_ptr);
            return(OK);					/* report success */
        }
    }

    /* Check caller queue. Use pointer pointers to keep code simple. */
    xpp = &caller_ptr->p_caller_q;
    while (*xpp != NIL_PROC) {
        if (src_e == ANY || src_p == proc_nr(*xpp)) {
#if 1
	    if (RTS_ISSET(*xpp, SLOT_FREE))
	    {
		kprintf("%d: receive from %d; found dead %d (%s)?\n",
			caller_ptr->p_endpoint, src_e, (*xpp)->p_endpoint,
			(*xpp)->p_name);
		if (caller_ptr->p_endpoint == ipc_stats_target)
			ipc_stats.deadproc++;
		return EINVAL;
	    }
#endif

	    /* Found acceptable message. Copy it and update status. */
	    CopyMess((*xpp)->p_nr, *xpp, (*xpp)->p_messbuf, caller_ptr, m_ptr);
	    RTS_UNSET(*xpp, SENDING);
            *xpp = (*xpp)->p_q_link;		/* remove from queue */
            return(OK);				/* report success */
	}
	xpp = &(*xpp)->p_q_link;		/* proceed to next */
    }

    if (caller_ptr->p_misc_flags & MF_ASYNMSG)
    {
	if (src_e != ANY)
	{
#if 0
		kprintf("mini_receive: should try async from %d\n", src_e);
#endif
		r= EAGAIN;
	}
	else
	{
		caller_ptr->p_messbuf = m_ptr;
		r= try_async(caller_ptr);
	}
	if (r == OK)
		return OK;	/* Got a message */
    }
  }

  /* No suitable message is available or the caller couldn't send in SENDREC. 
   * Block the process trying to receive, unless the flags tell otherwise.
   */
  if ( ! (flags & NON_BLOCKING)) {
      caller_ptr->p_getfrom_e = src_e;		
      caller_ptr->p_messbuf = m_ptr;
      RTS_SET(caller_ptr, RECEIVING);
      return(OK);
  } else {
	if (caller_ptr->p_endpoint == ipc_stats_target)
		ipc_stats.not_ready++;
	return(ENOTREADY);
  }
}

/*===========================================================================*
 *				mini_notify				     * 
 *===========================================================================*/
PRIVATE int mini_notify(caller_ptr, dst)
register struct proc *caller_ptr;	/* sender of the notification */
int dst;				/* which process to notify */
{
  register struct proc *dst_ptr = proc_addr(dst);
  int src_id;				/* source id for late delivery */
  message m;				/* the notification message */

  /* Check to see if target is blocked waiting for this message. A process 
   * can be both sending and receiving during a SENDREC system call.
   */
  if ( (RTS_ISSET(dst_ptr, RECEIVING) && !RTS_ISSET(dst_ptr, SENDING)) &&
      ! (dst_ptr->p_misc_flags & REPLY_PENDING) &&
      (dst_ptr->p_getfrom_e == ANY || 
      dst_ptr->p_getfrom_e == caller_ptr->p_endpoint)) {

      /* Destination is indeed waiting for a message. Assemble a notification 
       * message and deliver it. Copy from pseudo-source HARDWARE, since the
       * message is in the kernel's address space.
       */ 
      BuildMess(&m, proc_nr(caller_ptr), dst_ptr);
      CopyMess(proc_nr(caller_ptr), proc_addr(HARDWARE), &m, 
          dst_ptr, dst_ptr->p_messbuf);
      RTS_UNSET(dst_ptr, RECEIVING);
      return(OK);
  } 

  /* Destination is not ready to receive the notification. Add it to the 
   * bit map with pending notifications. Note the indirectness: the system id 
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
PRIVATE int mini_senda(caller_ptr, table, size)
struct proc *caller_ptr;
asynmsg_t *table;
size_t size;
{
	int i, dst_p, done, do_notify;
	unsigned flags;
	struct proc *dst_ptr;
	struct priv *privp;
	message *m_ptr;
	asynmsg_t tabent;
	vir_bytes table_v = (vir_bytes) table;

	privp= priv(caller_ptr);
	if (!(privp->s_flags & SYS_PROC))
	{
		kprintf(
		"mini_senda: warning caller has no privilege structure\n");
		if (caller_ptr->p_endpoint == ipc_stats_target)
			ipc_stats.no_priv++;
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
		if (caller_ptr->p_endpoint == ipc_stats_target)
			ipc_stats.bad_size++;
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
		if (flags & ~(AMF_VALID|AMF_DONE|AMF_NOTIFY) ||
			!(flags & AMF_VALID))
		{
			if (caller_ptr->p_endpoint == ipc_stats_target)
				ipc_stats.bad_senda++;
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

#if 0
		kprintf("mini_senda: entry[%d]: flags 0x%x dst %d/%d\n",
			i, tabent.flags, tabent.dst, dst_p);
#endif

		dst_ptr = proc_addr(dst_p);

		/* NO_ENDPOINT should be removed */
		if (dst_ptr->p_rts_flags & NO_ENDPOINT)
		{
			tabent.result= EDSTDIED;
			A_INSERT(i, result);
			tabent.flags= flags | AMF_DONE;
			A_INSERT(i, flags);

			if (flags & AMF_NOTIFY)
				do_notify= TRUE;
			continue;
		}

		/* Check if 'dst' is blocked waiting for this message. The
		 * destination's SENDING flag may be set when its SENDREC call
		 * blocked while sending. 
		 */
		if ( (dst_ptr->p_rts_flags & (RECEIVING | SENDING)) ==
			RECEIVING &&
			(dst_ptr->p_getfrom_e == ANY ||
			dst_ptr->p_getfrom_e == caller_ptr->p_endpoint))
		{
			/* Destination is indeed waiting for this message. */
			m_ptr= &table[i].msg;	/* Note: pointer in the
						 * caller's address space.
						 */
			CopyMess(caller_ptr->p_nr, caller_ptr, m_ptr, dst_ptr,
				dst_ptr->p_messbuf);

			RTS_UNSET(dst_ptr, RECEIVING);

			tabent.result= OK;
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
#if 0
		if(caller_ptr->p_endpoint > INIT_PROC_NR) {
			kprintf("kernel: %s (%d) asynsend table at 0x%lx, %d\n", 
				caller_ptr->p_name, caller_ptr->p_endpoint,
				table, size);
		}
#endif
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

	/* Try all privilege structures */
	for (privp = BEG_PRIV_ADDR; privp < END_PRIV_ADDR; ++privp) 
	{
		if (privp->s_proc_nr == NONE || privp->s_id == USER_PRIV_ID)
			continue;
		if (privp->s_asynsize == 0)
			continue;
#if 0
		kprintf("try_async: found asyntable for proc %d\n",
			privp->s_proc_nr);
#endif
		src_ptr= proc_addr(privp->s_proc_nr);
		r= try_one(src_ptr, caller_ptr);
		if (r == OK)
			return r;
	}

	/* Nothing found, clear MF_ASYNMSG */
	caller_ptr->p_misc_flags &= ~MF_ASYNMSG;

	return ESRCH;
}


/*===========================================================================*
 *				try_one					     *
 *===========================================================================*/
PRIVATE int try_one(src_ptr, dst_ptr)
struct proc *src_ptr;
struct proc *dst_ptr;
{
	int i, do_notify, done;
	unsigned flags;
	size_t size;
	endpoint_t dst_e;
	asynmsg_t *table_ptr;
	message *m_ptr;
	struct priv *privp;
	asynmsg_t tabent;
	vir_bytes table_v;
	struct proc *caller_ptr;

	privp= priv(src_ptr);
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
		if (flags & ~(AMF_VALID|AMF_DONE|AMF_NOTIFY) ||
			!(flags & AMF_VALID))
		{
			kprintf("try_one: bad bits in table\n");
			privp->s_asynsize= 0;
			if (src_ptr->p_endpoint == ipc_stats_target)
				ipc_stats.bad_senda++;
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

		/* Deliver message */
		table_ptr= (asynmsg_t *)privp->s_asyntab;
		m_ptr= &table_ptr[i].msg;	/* Note: pointer in the
					 	 * caller's address space.
					 	 */
		CopyMess(src_ptr->p_nr, src_ptr, m_ptr, dst_ptr,
			dst_ptr->p_messbuf);

		tabent.result= OK;
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
  int result, src, dst;

  if(!isokendpt(src_e, &src) || !isokendpt(dst_e, &dst))
	return EDEADSRCDST;

  /* Exception or interrupt occurred, thus already locked. */
  if (k_reenter >= 0) {
      result = mini_notify(proc_addr(src), dst); 
  }

  /* Call from task level, locking is required. */
  else {
      lock;
      result = mini_notify(proc_addr(src), dst); 
      unlock;
  }
  return(result);
}

/*===========================================================================*
 *				soft_notify				     *
 *===========================================================================*/
PUBLIC int soft_notify(dst_e)
int dst_e;			/* (endpoint) who is to be notified */
{
	int dst, u = 0;
	struct proc *dstp, *sys = proc_addr(SYSTEM);

/* Delayed interface to notify() from SYSTEM that is safe/easy to call
 * from more places than notify().
 */
	if(!intr_disabled()) { lock; u = 1; }

	{
		if(!isokendpt(dst_e, &dst))
			minix_panic("soft_notify to dead ep", dst_e);

		dstp = proc_addr(dst);

		if(!dstp->p_softnotified) {
			dstp->next_soft_notify = softnotify;
			softnotify = dstp;
			dstp->p_softnotified = 1;
	
			if (RTS_ISSET(sys, RECEIVING)) {
				sys->p_messbuf->m_source = SYSTEM;
				RTS_UNSET(sys, RECEIVING);
			}
		}
	}

	if(u) { unlock; }

	return OK;
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

#if DEBUG_SCHED_CHECK
  if(!intr_disabled()) { minix_panic("enqueue with interrupts enabled", NO_NUM); }
  CHECK_RUNQUEUES;
  if (rp->p_ready) minix_panic("enqueue already ready process", NO_NUM);
#endif

  /* Determine where to insert to process. */
  sched(rp, &q, &front);

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

  /* Now select the next process to run, if there isn't a current
   * process yet or current process isn't ready any more, or
   * it's PREEMPTIBLE.
   */
  if(!proc_ptr || proc_ptr->p_rts_flags ||
    (priv(proc_ptr)->s_flags & PREEMPTIBLE)) {
     pick_proc();
  }

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

  /* Side-effect for kernel: check if the task's stack still is ok? */
  if (iskernelp(rp)) { 				
	if (*priv(rp)->s_stack_guard != STACK_GUARD)
		minix_panic("stack overrun by task", proc_nr(rp));
  }

#if DEBUG_SCHED_CHECK
  CHECK_RUNQUEUES;
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
          if (rp == proc_ptr || rp == next_ptr)	/* active process removed */
              pick_proc();			/* pick new process to run */
          break;
      }
      prev_xp = *xpp;				/* save previous in chain */
  }

#if DEBUG_SCHED_CHECK
  rp->p_ready = 0;
  CHECK_RUNQUEUES;
#endif
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
      if (rp->p_priority < (IDLE_Q-1)) {  	 
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
PRIVATE void pick_proc()
{
/* Decide who to run now.  A new process is selected by setting 'next_ptr'.
 * When a billable process is selected, record it in 'bill_ptr', so that the 
 * clock task can tell who to bill for system time.
 */
  register struct proc *rp;			/* process to run */
  int q;					/* iterate over queues */

  /* Check each of the scheduling queues for ready processes. The number of
   * queues is defined in proc.h, and priorities are set in the task table.
   * The lowest queue contains IDLE, which is always ready.
   */
  for (q=0; q < NR_SCHED_QUEUES; q++) {	
      if ( (rp = rdy_head[q]) != NIL_PROC) {
          next_ptr = rp;			/* run process 'rp' next */
#if 0
	  if(rp->p_endpoint != 4 && rp->p_endpoint != 5 && rp->p_endpoint != IDLE && rp->p_endpoint != SYSTEM)
	  	kprintf("[run %s]",  rp->p_name);
#endif
          if (priv(rp)->s_flags & BILLABLE)	 	
              bill_ptr = rp;			/* bill for system time */
          return;				 
      }
  }
  minix_panic("no ready process", NO_NUM);
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

  for (rp=BEG_PROC_ADDR; rp<END_PROC_ADDR; rp++) {
      if (! isemptyp(rp)) {				/* check slot use */
	  lock;
	  if (rp->p_priority > rp->p_max_priority) {	/* update priority? */
	      if (rp->p_rts_flags == 0) dequeue(rp);	/* take off queue */
	      ticks_added += rp->p_quantum_size;	/* do accounting */
	      rp->p_priority -= 1;			/* raise priority */
	      if (rp->p_rts_flags == 0) enqueue(rp);	/* put on queue */
	  }
	  else {
	      ticks_added += rp->p_quantum_size - rp->p_ticks_left;
              rp->p_ticks_left = rp->p_quantum_size; 	/* give new quantum */
	  }
	  unlock;
      }
  }
#if DEBUG
  kprintf("ticks_added: %d\n", ticks_added);
#endif

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
 *				lock_enqueue				     *
 *===========================================================================*/
PUBLIC void lock_enqueue(rp)
struct proc *rp;		/* this process is now runnable */
{
/* Safe gateway to enqueue() for tasks. */
  lock;
  enqueue(rp);
  unlock;
}

/*===========================================================================*
 *				lock_dequeue				     *
 *===========================================================================*/
PUBLIC void lock_dequeue(rp)
struct proc *rp;		/* this process is no longer runnable */
{
/* Safe gateway to dequeue() for tasks. */
  if (k_reenter >= 0) {
	/* We're in an exception or interrupt, so don't lock (and ... 
	 * don't unlock).
	 */
	dequeue(rp);
  } else {
	lock;
	dequeue(rp);
	unlock;
  }
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
#if 0
		kprintf("kernel:%s:%d: bad endpoint %d: proc %d out of range\n",
		file, line, e, *p);
#endif
#endif
	} else if(isemptyn(*p)) {
#if DEBUG_ENABLE_IPC_WARNINGS
#if 0
	kprintf("kernel:%s:%d: bad endpoint %d: proc %d empty\n", file, line, e, *p);
#endif
#endif
	} else if(proc_addr(*p)->p_endpoint != e) {
#if DEBUG_ENABLE_IPC_WARNINGS
#if 0
		kprintf("kernel:%s:%d: bad endpoint %d: proc %d has ept %d (generation %d vs. %d)\n", file, line,
		e, *p, proc_addr(*p)->p_endpoint,
		_ENDPOINT_G(e), _ENDPOINT_G(proc_addr(*p)->p_endpoint));
#endif
#endif
	} else ok = 1;
	if(!ok && fatalflag) {
		minix_panic("invalid endpoint ", e);
	}
	return ok;
}

