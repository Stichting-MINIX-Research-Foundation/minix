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
#include <minix/ipcconst.h>
#include <stddef.h>
#include <signal.h>
#include <assert.h>

#include "kernel/kernel.h"
#include "vm.h"
#include "clock.h"
#include "spinlock.h"
#include "arch_proto.h"

#include <minix/syslib.h>

/* Scheduling and message passing functions */
static void idle(void);
/**
 * Made public for use in clock.c (for user-space scheduling)
static int mini_send(struct proc *caller_ptr, endpoint_t dst_e, message
	*m_ptr, int flags);
*/
static int mini_receive(struct proc *caller_ptr, endpoint_t src,
	message *m_ptr, int flags);
static int mini_senda(struct proc *caller_ptr, asynmsg_t *table, size_t
	size);
static int deadlock(int function, register struct proc *caller,
	endpoint_t src_dst_e);
static int try_async(struct proc *caller_ptr);
static int try_one(struct proc *src_ptr, struct proc *dst_ptr);
static struct proc * pick_proc(void);
static void enqueue_head(struct proc *rp);

/* all idles share the same idle_priv structure */
static struct priv idle_priv;

static void set_idle_name(char * name, int n)
{
        int i, c;
        int p_z = 0;

        if (n > 999) 
                n = 999; 

        name[0] = 'i'; 
        name[1] = 'd'; 
        name[2] = 'l'; 
        name[3] = 'e'; 

        for (i = 4, c = 100; c > 0; c /= 10) {
                int digit;

                digit = n / c;  
                n -= digit * c;  

                if (p_z || digit != 0 || c == 1) {
                        p_z = 1;
                        name[i++] = '0' + digit;
                }   
        }    

        name[i] = '\0';

}


#define PICK_ANY	1
#define PICK_HIGHERONLY	2

#define BuildNotifyMessage(m_ptr, src, dst_ptr) \
	(m_ptr)->m_type = NOTIFY_MESSAGE;				\
	(m_ptr)->NOTIFY_TIMESTAMP = get_monotonic();			\
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

void proc_init(void)
{
	struct proc * rp;
	struct priv *sp;
	int i;

	/* Clear the process table. Anounce each slot as empty and set up
	 * mappings for proc_addr() and proc_nr() macros. Do the same for the
	 * table with privilege structures for the system processes. 
	 */
	for (rp = BEG_PROC_ADDR, i = -NR_TASKS; rp < END_PROC_ADDR; ++rp, ++i) {
		rp->p_rts_flags = RTS_SLOT_FREE;/* initialize free slot */
		rp->p_magic = PMAGIC;
		rp->p_nr = i;			/* proc number from ptr */
		rp->p_endpoint = _ENDPOINT(0, rp->p_nr); /* generation no. 0 */
		rp->p_scheduler = NULL;		/* no user space scheduler */
		rp->p_priority = 0;		/* no priority */
		rp->p_quantum_size_ms = 0;	/* no quantum size */

		/* arch-specific initialization */
		arch_proc_reset(rp);
	}
	for (sp = BEG_PRIV_ADDR, i = 0; sp < END_PRIV_ADDR; ++sp, ++i) {
		sp->s_proc_nr = NONE;		/* initialize as free */
		sp->s_id = (sys_id_t) i;	/* priv structure index */
		ppriv_addr[i] = sp;		/* priv ptr from number */
		sp->s_sig_mgr = NONE;		/* clear signal managers */
		sp->s_bak_sig_mgr = NONE;
	}

	idle_priv.s_flags = IDL_F;
	/* initialize IDLE structures for every CPU */
	for (i = 0; i < CONFIG_MAX_CPUS; i++) {
		struct proc * ip = get_cpu_var_ptr(i, idle_proc);
		ip->p_endpoint = IDLE;
		ip->p_priv = &idle_priv;
		/* must not let idle ever get scheduled */
		ip->p_rts_flags |= RTS_PROC_STOP;
		set_idle_name(ip->p_name, i);
	}
}

static void switch_address_space_idle(void)
{
#ifdef CONFIG_SMP
	/*
	 * currently we bet that VM is always alive and its pages available so
	 * when the CPU wakes up the kernel is mapped and no surprises happen.
	 * This is only a problem if more than 1 cpus are available
	 */
	switch_address_space(proc_addr(VM_PROC_NR));
#endif
}

/*===========================================================================*
 *				idle					     * 
 *===========================================================================*/
static void idle(void)
{
	struct proc * p;

	/* This function is called whenever there is no work to do.
	 * Halt the CPU, and measure how many timestamp counter ticks are
	 * spent not doing anything. This allows test setups to measure
	 * the CPU utiliziation of certain workloads with high precision.
	 */

	p = get_cpulocal_var(proc_ptr) = get_cpulocal_var_ptr(idle_proc);
	if (priv(p)->s_flags & BILLABLE)
		get_cpulocal_var(bill_ptr) = p;

	switch_address_space_idle();

#ifdef CONFIG_SMP
	get_cpulocal_var(cpu_is_idle) = 1;
	/* we don't need to keep time on APs as it is handled on the BSP */
	if (cpuid != bsp_cpu_id)
		stop_local_timer();
	else
#endif
	{
		/*
		 * If the timer has expired while in kernel we must
		 * rearm it before we go to sleep
		 */
		restart_local_timer();
	}

	/* start accounting for the idle time */
	context_stop(proc_addr(KERNEL));
#if !SPROFILE
	halt_cpu();
#else
	if (!sprofiling)
		halt_cpu();
	else {
		volatile int * v;

		v = get_cpulocal_var_ptr(idle_interrupted);
		interrupts_enable();
		while (!*v)
			arch_pause();
		interrupts_disable();
		*v = 0;
	}
#endif
	/*
	 * end of accounting for the idle task does not happen here, the kernel
	 * is handling stuff for quite a while before it gets back here!
	 */
}

/*===========================================================================*
 *				switch_to_user				     * 
 *===========================================================================*/
void switch_to_user(void)
{
	/* This function is called an instant before proc_ptr is
	 * to be scheduled again.
	 */
	struct proc * p;
#ifdef CONFIG_SMP
	int tlb_must_refresh = 0;
#endif

	p = get_cpulocal_var(proc_ptr);
	/*
	 * if the current process is still runnable check the misc flags and let
	 * it run unless it becomes not runnable in the meantime
	 */
	if (proc_is_runnable(p))
		goto check_misc_flags;
	/*
	 * if a process becomes not runnable while handling the misc flags, we
	 * need to pick a new one here and start from scratch. Also if the
	 * current process wasn' runnable, we pick a new one here
	 */
not_runnable_pick_new:
	if (proc_is_preempted(p)) {
		p->p_rts_flags &= ~RTS_PREEMPTED;
		if (proc_is_runnable(p)) {
			if (!is_zero64(p->p_cpu_time_left))
				enqueue_head(p);
			else
				enqueue(p);
		}
	}

	/*
	 * if we have no process to run, set IDLE as the current process for
	 * time accounting and put the cpu in and idle state. After the next
	 * timer interrupt the execution resumes here and we can pick another
	 * process. If there is still nothing runnable we "schedule" IDLE again
	 */
	while (!(p = pick_proc())) {
		idle();
	}

	/* update the global variable */
	get_cpulocal_var(proc_ptr) = p;

#ifdef CONFIG_SMP
	if (p->p_misc_flags & MF_FLUSH_TLB && get_cpulocal_var(ptproc) == p)
		tlb_must_refresh = 1;
#endif
	switch_address_space(p);

check_misc_flags:

	assert(p);
	assert(proc_is_runnable(p));
	while (p->p_misc_flags &
		(MF_KCALL_RESUME | MF_DELIVERMSG |
		 MF_SC_DEFER | MF_SC_TRACE | MF_SC_ACTIVE)) {

		assert(proc_is_runnable(p));
		if (p->p_misc_flags & MF_KCALL_RESUME) {
			kernel_call_resume(p);
		}
		else if (p->p_misc_flags & MF_DELIVERMSG) {
			TRACE(VF_SCHEDULING, printf("delivering to %s / %d\n",
				p->p_name, p->p_endpoint););
			delivermsg(p);
		}
		else if (p->p_misc_flags & MF_SC_DEFER) {
			/* Perform the system call that we deferred earlier. */

			assert (!(p->p_misc_flags & MF_SC_ACTIVE));

			arch_do_syscall(p);

			/* If the process is stopped for signal delivery, and
			 * not blocked sending a message after the system call,
			 * inform PM.
			 */
			if ((p->p_misc_flags & MF_SIG_DELAY) &&
					!RTS_ISSET(p, RTS_SENDING))
				sig_delay_done(p);
		}
		else if (p->p_misc_flags & MF_SC_TRACE) {
			/* Trigger a system call leave event if this was a
			 * system call. We must do this after processing the
			 * other flags above, both for tracing correctness and
			 * to be able to use 'break'.
			 */
			if (!(p->p_misc_flags & MF_SC_ACTIVE))
				break;

			p->p_misc_flags &=
				~(MF_SC_TRACE | MF_SC_ACTIVE);

			/* Signal the "leave system call" event.
			 * Block the process.
			 */
			cause_sig(proc_nr(p), SIGTRAP);
		}
		else if (p->p_misc_flags & MF_SC_ACTIVE) {
			/* If MF_SC_ACTIVE was set, remove it now:
			 * we're leaving the system call.
			 */
			p->p_misc_flags &= ~MF_SC_ACTIVE;

			break;
		}

		/*
		 * the selected process might not be runnable anymore. We have
		 * to checkit and schedule another one
		 */
		if (!proc_is_runnable(p))
			goto not_runnable_pick_new;
	}
	/*
	 * check the quantum left before it runs again. We must do it only here
	 * as we are sure that a possible out-of-quantum message to the
	 * scheduler will not collide with the regular ipc
	 */
	if (is_zero64(p->p_cpu_time_left))
		proc_no_time(p);
	/*
	 * After handling the misc flags the selected process might not be
	 * runnable anymore. We have to checkit and schedule another one
	 */
	if (!proc_is_runnable(p))
		goto not_runnable_pick_new;

	TRACE(VF_SCHEDULING, printf("cpu %d starting %s / %d "
				"pc 0x%08x\n",
		cpuid, p->p_name, p->p_endpoint, p->p_reg.pc););
#if DEBUG_TRACE
	p->p_schedules++;
#endif

	p = arch_finish_switch_to_user();
	assert(!is_zero64(p->p_cpu_time_left));

	context_stop(proc_addr(KERNEL));

	/* If the process isn't the owner of FPU, enable the FPU exception */
	if(get_cpulocal_var(fpu_owner) != p)
		enable_fpu_exception();
	else
		disable_fpu_exception();

	/* If MF_CONTEXT_SET is set, don't clobber process state within
	 * the kernel. The next kernel entry is OK again though.
	 */
	p->p_misc_flags &= ~MF_CONTEXT_SET;

#if defined(__i386__)
  	assert(p->p_seg.p_cr3 != 0);
#elif defined(__arm__)
	assert(p->p_seg.p_ttbr != 0);
#endif
#ifdef CONFIG_SMP
	if (p->p_misc_flags & MF_FLUSH_TLB) {
		if (tlb_must_refresh)
			refresh_tlb();
		p->p_misc_flags &= ~MF_FLUSH_TLB;
	}
#endif
	
	restart_local_timer();
	
	/*
	 * restore_user_context() carries out the actual mode switch from kernel
	 * to userspace. This function does not return
	 */
	restore_user_context(p);
	NOT_REACHABLE;
}

/*
 * handler for all synchronous IPC calls
 */
static int do_sync_ipc(struct proc * caller_ptr, /* who made the call */
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
          call_nr, proc_nr(caller_ptr), src_dst_e);
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
      printf("sys_call: trap %s not allowed, caller %d, src_dst %d\n",
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

int do_ipc(reg_t r1, reg_t r2, reg_t r3)
{
  struct proc *const caller_ptr = get_cpulocal_var(proc_ptr);	/* get pointer to caller */
  int call_nr = (int) r1;

  assert(!RTS_ISSET(caller_ptr, RTS_SLOT_FREE));

  /* bill kernel time to this process. */
  kbill_ipc = caller_ptr;

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
		assert(!(caller_ptr->p_misc_flags & MF_SC_DEFER));
		caller_ptr->p_misc_flags |= MF_SC_DEFER;
		caller_ptr->p_defer.r1 = r1;
		caller_ptr->p_defer.r2 = r2;
		caller_ptr->p_defer.r3 = r3;

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
  	{
  	    /* Process accounting for scheduling */
	    caller_ptr->p_accounting.ipc_sync++;

  	    return do_sync_ipc(caller_ptr, call_nr, (endpoint_t) r2,
			    (message *) r3);
  	}
  	case SENDA:
  	{
 	    /*
  	     * Get and check the size of the argument in bytes as it is a
  	     * table
  	     */
  	    size_t msg_size = (size_t) r2;
  
  	    /* Process accounting for scheduling */
	    caller_ptr->p_accounting.ipc_async++;
 
  	    /* Limit size to something reasonable. An arbitrary choice is 16
  	     * times the number of process table entries.
  	     */
  	    if (msg_size > 16*(NR_TASKS + NR_PROCS))
	        return EDOM;
  	    return mini_senda(caller_ptr, (asynmsg_t *) r3, msg_size);
  	}
  	case MINIX_KERNINFO:
	{
		/* It might not be initialized yet. */
	  	if(!minix_kerninfo_user) {
			return EBADCALL;
		}

  		arch_set_secondary_ipc_return(caller_ptr, minix_kerninfo_user);
  		return OK;
	}
  	default:
	return EBADCALL;		/* illegal system call */
  }
}

/*===========================================================================*
 *				deadlock				     * 
 *===========================================================================*/
static int deadlock(function, cp, src_dst_e) 
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
 *				has_pending				     * 
 *===========================================================================*/
static int has_pending(sys_map_t *map, int src_p, int asynm)
{
/* Check to see if there is a pending message from the desired source
 * available.
 */

  int src_id;
  sys_id_t id = NULL_PRIV_ID;
#ifdef CONFIG_SMP
  struct proc * p;
#endif

  /* Either check a specific bit in the mask map, or find the first bit set in
   * it (if any), depending on whether the receive was called on a specific
   * source endpoint.
   */
  if (src_p != ANY) {
	src_id = nr_to_id(src_p);
	if (get_sys_bit(*map, src_id)) {
#ifdef CONFIG_SMP
		p = proc_addr(id_to_nr(src_id));
		if (asynm && RTS_ISSET(p, RTS_VMINHIBIT))
			p->p_misc_flags |= MF_SENDA_VM_MISS;
		else
#endif
			id = src_id;
	}
  } else {
	/* Find a source with a pending message */
	for (src_id = 0; src_id < NR_SYS_PROCS; src_id += BITCHUNK_BITS) {
		if (get_sys_bits(*map, src_id) != 0) {
#ifdef CONFIG_SMP
			while (src_id < NR_SYS_PROCS) {
				while (!get_sys_bit(*map, src_id)) {
					if (src_id == NR_SYS_PROCS)
						goto quit_search;
					src_id++;
				}
				p = proc_addr(id_to_nr(src_id));
				/*
				 * We must not let kernel fiddle with pages of a
				 * process which are currently being changed by
				 * VM.  It is dangerous! So do not report such a
				 * process as having pending async messages.
				 * Skip it.
				 */
				if (asynm && RTS_ISSET(p, RTS_VMINHIBIT)) {
					p->p_misc_flags |= MF_SENDA_VM_MISS;
					src_id++;
				} else
					goto quit_search;
			}
#else
			while (!get_sys_bit(*map, src_id)) src_id++;
			goto quit_search;
#endif
		}
	}

quit_search:
	if (src_id < NR_SYS_PROCS)	/* Found one */
		id = src_id;
  }

  return(id);
}

/*===========================================================================*
 *				has_pending_notify			     *
 *===========================================================================*/
int has_pending_notify(struct proc * caller, int src_p)
{
	sys_map_t * map = &priv(caller)->s_notify_pending;
	return has_pending(map, src_p, 0);
}

/*===========================================================================*
 *				has_pending_asend			     *
 *===========================================================================*/
int has_pending_asend(struct proc * caller, int src_p)
{
	sys_map_t * map = &priv(caller)->s_asyn_pending;
	return has_pending(map, src_p, 1);
}

/*===========================================================================*
 *				unset_notify_pending			     *
 *===========================================================================*/
void unset_notify_pending(struct proc * caller, int src_p)
{
	sys_map_t * map = &priv(caller)->s_notify_pending;
	unset_sys_bit(*map, src_p);
}

/*===========================================================================*
 *				mini_send				     * 
 *===========================================================================*/
int mini_send(
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
		if(copy_msg_from_user(m_ptr, &dst_ptr->p_delivermsg))
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

#if DEBUG_IPC_HOOK
	hook_ipc_msgsend(&dst_ptr->p_delivermsg, caller_ptr, dst_ptr);
	hook_ipc_msgrecv(&dst_ptr->p_delivermsg, caller_ptr, dst_ptr);
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
		if(copy_msg_from_user(m_ptr, &caller_ptr->p_sendmsg))
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

#if DEBUG_IPC_HOOK
	hook_ipc_msgsend(&caller_ptr->p_sendmsg, caller_ptr, dst_ptr);
#endif
  }
  return(OK);
}

/*===========================================================================*
 *				mini_receive				     * 
 *===========================================================================*/
static int mini_receive(struct proc * caller_ptr,
			endpoint_t src_e, /* which message source is wanted */
			message * m_buff_usr, /* pointer to message buffer */
			const int flags)
{
/* A process or task wants to get a message.  If a message is already queued,
 * acquire it and deblock the sender.  If no message from the desired source
 * is available block the caller.
 */
  register struct proc **xpp;
  int r, src_id, src_proc_nr, src_p;

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

	/* Check for pending notifications */
        if ((src_id = has_pending_notify(caller_ptr, src_p)) != NULL_PRIV_ID) {
            endpoint_t hisep;

            src_proc_nr = id_to_nr(src_id);		/* get source proc */
#if DEBUG_ENABLE_IPC_WARNINGS
	    if(src_proc_nr == NONE) {
		printf("mini_receive: sending notify from NONE\n");
	    }
#endif
	    assert(src_proc_nr != NONE);
            unset_notify_pending(caller_ptr, src_id);	/* no longer pending */

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

    /* Check for pending asynchronous messages */
    if (has_pending_asend(caller_ptr, src_p) != NULL_PRIV_ID) {
        if (src_p != ANY)
        	r = try_one(proc_addr(src_p), caller_ptr);
        else
        	r = try_async(caller_ptr);

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

#if DEBUG_IPC_HOOK
            hook_ipc_msgrecv(&caller_ptr->p_delivermsg, *xpp, caller_ptr);
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
int mini_notify(
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

#define A_RETR_FLD(entry, field)	\
  if(data_copy(caller_ptr->p_endpoint,	\
	 table_v + (entry)*sizeof(asynmsg_t) + offsetof(struct asynmsg,field),\
		KERNEL, (vir_bytes) &tabent.field,	\
			sizeof(tabent.field)) != OK) {\
		ASCOMPLAIN(caller_ptr, entry, #field);	\
		r = EFAULT; \
	        goto asyn_error; \
	}

#define A_RETR(entry) do {			\
  if (data_copy(				\
  		caller_ptr->p_endpoint, table_v + (entry)*sizeof(asynmsg_t),\
  		KERNEL, (vir_bytes) &tabent,	\
  		sizeof(tabent)) != OK) {	\
  			ASCOMPLAIN(caller_ptr, entry, "message entry");	\
  			r = EFAULT;		\
	                goto asyn_error; \
  }						\
  			 } while(0)

#define A_INSRT_FLD(entry, field)	\
  if(data_copy(KERNEL, (vir_bytes) &tabent.field, \
	caller_ptr->p_endpoint,	\
 	table_v + (entry)*sizeof(asynmsg_t) + offsetof(struct asynmsg,field),\
		sizeof(tabent.field)) != OK) {\
		ASCOMPLAIN(caller_ptr, entry, #field);	\
		r = EFAULT; \
	        goto asyn_error; \
	}

#define A_INSRT(entry) do {			\
  if (data_copy(KERNEL, (vir_bytes) &tabent,	\
  		caller_ptr->p_endpoint, table_v + (entry)*sizeof(asynmsg_t),\
  		sizeof(tabent)) != OK) {	\
  			ASCOMPLAIN(caller_ptr, entry, "message entry");	\
  			r = EFAULT;		\
	                goto asyn_error; \
  }						\
  			  } while(0)	

/*===========================================================================*
 *				try_deliver_senda			     *
 *===========================================================================*/
int try_deliver_senda(struct proc *caller_ptr,
				asynmsg_t *table,
				size_t size)
{
  int r, dst_p, done, do_notify;
  unsigned int i;
  unsigned flags;
  endpoint_t dst;
  struct proc *dst_ptr;
  struct priv *privp;
  asynmsg_t tabent;
  const vir_bytes table_v = (vir_bytes) table;

  privp = priv(caller_ptr);

  /* Clear table */
  privp->s_asyntab = -1;
  privp->s_asynsize = 0;

  if (size == 0) return(OK);  /* Nothing to do, just return */

  /* Scan the table */
  do_notify = FALSE;
  done = TRUE;

  /* Limit size to something reasonable. An arbitrary choice is 16
   * times the number of process table entries.
   *
   * (this check has been duplicated in sys_call but is left here
   * as a sanity check)
   */
  if (size > 16*(NR_TASKS + NR_PROCS)) {
    r = EDOM;
    return r;
  }

  for (i = 0; i < size; i++) {
	/* Process each entry in the table and store the result in the table.
	 * If we're done handling a message, copy the result to the sender. */

	dst = NONE;
	/* Copy message to kernel */
	A_RETR(i);
	flags = tabent.flags;
	dst = tabent.dst;

	if (flags == 0) continue; /* Skip empty entries */

	/* 'flags' field must contain only valid bits */
	if(flags & ~(AMF_VALID|AMF_DONE|AMF_NOTIFY|AMF_NOREPLY|AMF_NOTIFY_ERR)) {
		r = EINVAL;
		goto asyn_error;
	}
	if (!(flags & AMF_VALID)) { /* Must contain message */
		r = EINVAL;
		goto asyn_error;
	}
	if (flags & AMF_DONE) continue;	/* Already done processing */

	r = OK;
	if (!isokendpt(tabent.dst, &dst_p)) 
		r = EDEADSRCDST; /* Bad destination, report the error */
	else if (iskerneln(dst_p)) 
		r = ECALLDENIED; /* Asyn sends to the kernel are not allowed */
	else if (!may_send_to(caller_ptr, dst_p)) 
		r = ECALLDENIED; /* Send denied by IPC mask */
	else 	/* r == OK */
		dst_ptr = proc_addr(dst_p);

	/* XXX: RTS_NO_ENDPOINT should be removed */
	if (r == OK && RTS_ISSET(dst_ptr, RTS_NO_ENDPOINT)) {
		r = EDEADSRCDST;
	}

	/* Check if 'dst' is blocked waiting for this message.
	 * If AMF_NOREPLY is set, do not satisfy the receiving part of
	 * a SENDREC.
	 */
	if (r == OK && WILLRECEIVE(dst_ptr, caller_ptr->p_endpoint) &&
	    (!(flags&AMF_NOREPLY) || !(dst_ptr->p_misc_flags&MF_REPLY_PEND))) {
		/* Destination is indeed waiting for this message. */
		dst_ptr->p_delivermsg = tabent.msg;
		dst_ptr->p_delivermsg.m_source = caller_ptr->p_endpoint;
		dst_ptr->p_misc_flags |= MF_DELIVERMSG;
		IPC_STATUS_ADD_CALL(dst_ptr, SENDA);
		RTS_UNSET(dst_ptr, RTS_RECEIVING);
#if DEBUG_IPC_HOOK
		hook_ipc_msgrecv(&dst_ptr->p_delivermsg, caller_ptr, dst_ptr);
#endif
	} else if (r == OK) {
		/* Inform receiver that something is pending */
		set_sys_bit(priv(dst_ptr)->s_asyn_pending, 
			    priv(caller_ptr)->s_id); 
		done = FALSE;
		continue;
	} 

	/* Store results */
	tabent.result = r;
	tabent.flags = flags | AMF_DONE;
	if (flags & AMF_NOTIFY)
		do_notify = TRUE;
	else if (r != OK && (flags & AMF_NOTIFY_ERR))
		do_notify = TRUE;
	A_INSRT(i);	/* Copy results to caller */
	continue;

asyn_error:
	if (dst != NONE)
		printf("KERNEL senda error %d to %d\n", r, dst);
	else
		printf("KERNEL senda error %d\n", r);
  }

  if (do_notify) 
	mini_notify(proc_addr(ASYNCM), caller_ptr->p_endpoint);

  if (!done) {
	privp->s_asyntab = (vir_bytes) table;
	privp->s_asynsize = size;
  }

  return(OK);
}

/*===========================================================================*
 *				mini_senda				     *
 *===========================================================================*/
static int mini_senda(struct proc *caller_ptr, asynmsg_t *table, size_t size)
{
  struct priv *privp;

  privp = priv(caller_ptr);
  if (!(privp->s_flags & SYS_PROC)) {
	printf( "mini_senda: warning caller has no privilege structure\n");
	return(EPERM);
  }

  return try_deliver_senda(caller_ptr, table, size);
}


/*===========================================================================*
 *				try_async				     * 
 *===========================================================================*/
static int try_async(caller_ptr)
struct proc *caller_ptr;
{
  int r;
  struct priv *privp;
  struct proc *src_ptr;
  sys_map_t *map;

  map = &priv(caller_ptr)->s_asyn_pending;

  /* Try all privilege structures */
  for (privp = BEG_PRIV_ADDR; privp < END_PRIV_ADDR; ++privp)  {
	if (privp->s_proc_nr == NONE)
		continue;

	if (!get_sys_bit(*map, privp->s_id)) 
		continue;

	src_ptr = proc_addr(privp->s_proc_nr);

#ifdef CONFIG_SMP
	/*
	 * Do not copy from a process which does not have a stable address space
	 * due to VM fiddling with it
	 */
	if (RTS_ISSET(src_ptr, RTS_VMINHIBIT)) {
		src_ptr->p_misc_flags |= MF_SENDA_VM_MISS;
		continue;
	}
#endif

	assert(!(caller_ptr->p_misc_flags & MF_DELIVERMSG));
	if ((r = try_one(src_ptr, caller_ptr)) == OK)
		return(r);
  }

  return(ESRCH);
}


/*===========================================================================*
 *				try_one					     *
 *===========================================================================*/
static int try_one(struct proc *src_ptr, struct proc *dst_ptr)
{
/* Try to receive an asynchronous message from 'src_ptr' */
  int r = EAGAIN, done, do_notify;
  unsigned int flags, i;
  size_t size;
  endpoint_t dst;
  struct proc *caller_ptr;
  struct priv *privp;
  asynmsg_t tabent;
  vir_bytes table_v;

  privp = priv(src_ptr);
  if (!(privp->s_flags & SYS_PROC)) return(EPERM);
  size = privp->s_asynsize;
  table_v = privp->s_asyntab;

  /* Clear table pending message flag. We're done unless we're not. */
  unset_sys_bit(priv(dst_ptr)->s_asyn_pending, privp->s_id);

  if (size == 0) return(EAGAIN);
  if (!may_send_to(src_ptr, proc_nr(dst_ptr))) return(ECALLDENIED);

  caller_ptr = src_ptr;	/* Needed for A_ macros later on */

  /* Scan the table */
  do_notify = FALSE;
  done = TRUE;

  for (i = 0; i < size; i++) {
  	/* Process each entry in the table and store the result in the table.
  	 * If we're done handling a message, copy the result to the sender.
  	 * Some checks done in mini_senda are duplicated here, as the sender
  	 * could've altered the contents of the table in the meantime.
  	 */

	/* Copy message to kernel */
	A_RETR(i);
	flags = tabent.flags;
	dst = tabent.dst;

	if (flags == 0) continue;	/* Skip empty entries */

	/* 'flags' field must contain only valid bits */
	if(flags & ~(AMF_VALID|AMF_DONE|AMF_NOTIFY|AMF_NOREPLY|AMF_NOTIFY_ERR))
		r = EINVAL;
	else if (!(flags & AMF_VALID)) /* Must contain message */
		r = EINVAL; 
	else if (flags & AMF_DONE) continue; /* Already done processing */

	/* Clear done flag. The sender is done sending when all messages in the
	 * table are marked done or empty. However, we will know that only
	 * the next time we enter this function or when the sender decides to
	 * send additional asynchronous messages and manages to deliver them
	 * all.
	 */
	done = FALSE;

	if (r == EINVAL)
		goto store_result;

	/* Message must be directed at receiving end */
	if (dst != dst_ptr->p_endpoint) continue;

	/* If AMF_NOREPLY is set, then this message is not a reply to a
	 * SENDREC and thus should not satisfy the receiving part of the
	 * SENDREC. This message is to be delivered later.
	 */
	if ((flags & AMF_NOREPLY) && (dst_ptr->p_misc_flags & MF_REPLY_PEND)) 
		continue;

	/* Destination is ready to receive the message; deliver it */
	r = OK;
	dst_ptr->p_delivermsg = tabent.msg;
	dst_ptr->p_delivermsg.m_source = src_ptr->p_endpoint;
	dst_ptr->p_misc_flags |= MF_DELIVERMSG;
#if DEBUG_IPC_HOOK
	hook_ipc_msgrecv(&dst_ptr->p_delivermsg, src_ptr, dst_ptr);
#endif

store_result:
	/* Store results for sender */
	tabent.result = r;
	tabent.flags = flags | AMF_DONE;
	if (flags & AMF_NOTIFY) do_notify = TRUE;
	else if (r != OK && (flags & AMF_NOTIFY_ERR)) do_notify = TRUE;
	A_INSRT(i);	/* Copy results to sender */

	break;
  }

  if (do_notify) 
	mini_notify(proc_addr(ASYNCM), src_ptr->p_endpoint);

  if (done) {
	privp->s_asyntab = -1;
	privp->s_asynsize = 0;
  } else {
	set_sys_bit(priv(dst_ptr)->s_asyn_pending, privp->s_id);
  }

asyn_error:
  return(r);
}

/*===========================================================================*
 *				cancel_async				     *
 *===========================================================================*/
int cancel_async(struct proc *src_ptr, struct proc *dst_ptr)
{
/* Cancel asynchronous messages from src to dst, because dst is not interested
 * in them (e.g., dst has been restarted) */
  int done, do_notify;
  unsigned int flags, i;
  size_t size;
  endpoint_t dst;
  struct proc *caller_ptr;
  struct priv *privp;
  asynmsg_t tabent;
  vir_bytes table_v;

  privp = priv(src_ptr);
  if (!(privp->s_flags & SYS_PROC)) return(EPERM);
  size = privp->s_asynsize;
  table_v = privp->s_asyntab;

  /* Clear table pending message flag. We're done unless we're not. */
  privp->s_asyntab = -1;
  privp->s_asynsize = 0;
  unset_sys_bit(priv(dst_ptr)->s_asyn_pending, privp->s_id);

  if (size == 0) return(EAGAIN);
  if (!may_send_to(src_ptr, proc_nr(dst_ptr))) return(ECALLDENIED);

  caller_ptr = src_ptr;	/* Needed for A_ macros later on */

  /* Scan the table */
  do_notify = FALSE;
  done = TRUE;


  for (i = 0; i < size; i++) {
  	/* Process each entry in the table and store the result in the table.
  	 * If we're done handling a message, copy the result to the sender.
  	 * Some checks done in mini_senda are duplicated here, as the sender
  	 * could've altered the contents of the table in the mean time.
  	 */

  	int r = EDEADSRCDST;	/* Cancel delivery due to dead dst */

	/* Copy message to kernel */
	A_RETR(i);
	flags = tabent.flags;
	dst = tabent.dst;

	if (flags == 0) continue;	/* Skip empty entries */

	/* 'flags' field must contain only valid bits */
	if(flags & ~(AMF_VALID|AMF_DONE|AMF_NOTIFY|AMF_NOREPLY|AMF_NOTIFY_ERR))
		r = EINVAL;
	else if (!(flags & AMF_VALID)) /* Must contain message */
		r = EINVAL; 
	else if (flags & AMF_DONE) continue; /* Already done processing */

	/* Message must be directed at receiving end */
	if (dst != dst_ptr->p_endpoint) {
		done = FALSE;
		continue;
	}

	/* Store results for sender */
	tabent.result = r;
	tabent.flags = flags | AMF_DONE;
	if (flags & AMF_NOTIFY) do_notify = TRUE;
	else if (r != OK && (flags & AMF_NOTIFY_ERR)) do_notify = TRUE;
	A_INSRT(i);	/* Copy results to sender */
  }

  if (do_notify) 
	mini_notify(proc_addr(ASYNCM), src_ptr->p_endpoint);

  if (!done) {
	privp->s_asyntab = table_v;
	privp->s_asynsize = size;
  }

asyn_error:
  return(OK);
}

/*===========================================================================*
 *				enqueue					     * 
 *===========================================================================*/
void enqueue(
  register struct proc *rp	/* this process is now runnable */
)
{
/* Add 'rp' to one of the queues of runnable processes.  This function is 
 * responsible for inserting a process into one of the scheduling queues. 
 * The mechanism is implemented here.   The actual scheduling policy is
 * defined in sched() and pick_proc().
 *
 * This function can be used x-cpu as it always uses the queues of the cpu the
 * process is assigned to.
 */
  int q = rp->p_priority;	 		/* scheduling queue to use */
  struct proc **rdy_head, **rdy_tail;
  
  assert(proc_is_runnable(rp));

  assert(q >= 0);

  rdy_head = get_cpu_var(rp->p_cpu, run_q_head);
  rdy_tail = get_cpu_var(rp->p_cpu, run_q_tail);

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

  if (cpuid == rp->p_cpu) {
	  /*
	   * enqueueing a process with a higher priority than the current one,
	   * it gets preempted. The current process must be preemptible. Testing
	   * the priority also makes sure that a process does not preempt itself
	   */
	  struct proc * p;
	  p = get_cpulocal_var(proc_ptr);
	  assert(p);
	  if((p->p_priority > rp->p_priority) &&
			  (priv(p)->s_flags & PREEMPTIBLE))
		  RTS_SET(p, RTS_PREEMPTED); /* calls dequeue() */
  }
#ifdef CONFIG_SMP
  /*
   * if the process was enqueued on a different cpu and the cpu is idle, i.e.
   * the time is off, we need to wake up that cpu and let it schedule this new
   * process
   */
  else if (get_cpu_var(rp->p_cpu, cpu_is_idle)) {
	  smp_schedule(rp->p_cpu);
  }
#endif

  /* Make note of when this process was added to queue */
  read_tsc_64(&(get_cpulocal_var(proc_ptr)->p_accounting.enter_queue));


#if DEBUG_SANITYCHECKS
  assert(runqueues_ok_local());
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
static void enqueue_head(struct proc *rp)
{
  const int q = rp->p_priority;	 		/* scheduling queue to use */

  struct proc **rdy_head, **rdy_tail;

  assert(proc_ptr_ok(rp));
  assert(proc_is_runnable(rp));

  /*
   * the process was runnable without its quantum expired when dequeued. A
   * process with no time left should vahe been handled else and differently
   */
  assert(!is_zero64(rp->p_cpu_time_left));

  assert(q >= 0);


  rdy_head = get_cpu_var(rp->p_cpu, run_q_head);
  rdy_tail = get_cpu_var(rp->p_cpu, run_q_tail);

  /* Now add the process to the queue. */
  if (!rdy_head[q]) {		/* add to empty queue */
      rdy_head[q] = rdy_tail[q] = rp; 		/* create a new queue */
      rp->p_nextready = NULL;		/* mark new end */
  }
  else						/* add to head of queue */
      rp->p_nextready = rdy_head[q];		/* chain head of queue */
      rdy_head[q] = rp;				/* set new queue head */

  /* Make note of when this process was added to queue */
  read_tsc_64(&(get_cpulocal_var(proc_ptr->p_accounting.enter_queue)));


  /* Process accounting for scheduling */
  rp->p_accounting.dequeues--;
  rp->p_accounting.preempted++;

#if DEBUG_SANITYCHECKS
  assert(runqueues_ok_local());
#endif
}

/*===========================================================================*
 *				dequeue					     * 
 *===========================================================================*/
void dequeue(struct proc *rp)
/* this process is no longer runnable */
{
/* A process must be removed from the scheduling queues, for example, because
 * it has blocked.  If the currently active process is removed, a new process
 * is picked to run by calling pick_proc().
 *
 * This function can operate x-cpu as it always removes the process from the
 * queue of the cpu the process is currently assigned to.
 */
  int q = rp->p_priority;		/* queue to use */
  struct proc **xpp;			/* iterate over queue */
  struct proc *prev_xp;
  u64_t tsc, tsc_delta;

  struct proc **rdy_tail;

  assert(proc_ptr_ok(rp));
  assert(!proc_is_runnable(rp));

  /* Side-effect for kernel: check if the task's stack still is ok? */
  assert (!iskernelp(rp) || *priv(rp)->s_stack_guard == STACK_GUARD);

  rdy_tail = get_cpu_var(rp->p_cpu, run_q_tail);

  /* Now make sure that the process is not in its ready queue. Remove the 
   * process if it is found. A process can be made unready even if it is not 
   * running by being sent a signal that kills it.
   */
  prev_xp = NULL;				
  for (xpp = get_cpu_var_ptr(rp->p_cpu, run_q_head[q]); *xpp;
		  xpp = &(*xpp)->p_nextready) {
      if (*xpp == rp) {				/* found process to remove */
          *xpp = (*xpp)->p_nextready;		/* replace with next chain */
          if (rp == rdy_tail[q]) {		/* queue tail removed */
              rdy_tail[q] = prev_xp;		/* set new tail */
	  }

          break;
      }
      prev_xp = *xpp;				/* save previous in chain */
  }

	
  /* Process accounting for scheduling */
  rp->p_accounting.dequeues++;

  /* this is not all that accurate on virtual machines, especially with
     IO bound processes that only spend a short amount of time in the queue
     at a time. */
  if (!is_zero64(rp->p_accounting.enter_queue)) {
	read_tsc_64(&tsc);
	tsc_delta = sub64(tsc, rp->p_accounting.enter_queue);
	rp->p_accounting.time_in_queue = add64(rp->p_accounting.time_in_queue,
		tsc_delta);
	make_zero64(rp->p_accounting.enter_queue);
  }


#if DEBUG_SANITYCHECKS
  assert(runqueues_ok_local());
#endif
}

/*===========================================================================*
 *				pick_proc				     * 
 *===========================================================================*/
static struct proc * pick_proc(void)
{
/* Decide who to run now.  A new process is selected an returned.
 * When a billable process is selected, record it in 'bill_ptr', so that the 
 * clock task can tell who to bill for system time.
 *
 * This function always uses the run queues of the local cpu!
 */
  register struct proc *rp;			/* process to run */
  struct proc **rdy_head;
  int q;				/* iterate over queues */

  /* Check each of the scheduling queues for ready processes. The number of
   * queues is defined in proc.h, and priorities are set in the task table.
   * If there are no processes ready to run, return NULL.
   */
  rdy_head = get_cpulocal_var(run_q_head);
  for (q=0; q < NR_SCHED_QUEUES; q++) {	
	if(!(rp = rdy_head[q])) {
		TRACE(VF_PICKPROC, printf("cpu %d queue %d empty\n", cpuid, q););
		continue;
	}
	assert(proc_is_runnable(rp));
	if (priv(rp)->s_flags & BILLABLE)	 	
		get_cpulocal_var(bill_ptr) = rp; /* bill for system time */
	return rp;
  }
  return NULL;
}

/*===========================================================================*
 *				endpoint_lookup				     *
 *===========================================================================*/
struct proc *endpoint_lookup(endpoint_t e)
{
	int n;

	if(!isokendpt(e, &n)) return NULL;

	return proc_addr(n);
}

/*===========================================================================*
 *				isokendpt_f				     *
 *===========================================================================*/
#if DEBUG_ENABLE_IPC_WARNINGS
int isokendpt_f(file, line, e, p, fatalflag)
const char *file;
int line;
#else
int isokendpt_f(e, p, fatalflag)
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
	ok = 0;
	if(isokprocn(*p) && !isemptyn(*p) && proc_addr(*p)->p_endpoint == e)
		ok = 1;
	if(!ok && fatalflag)
		panic("invalid endpoint: %d",  e);
	return ok;
}

static void notify_scheduler(struct proc *p)
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
	m_no_quantum.SCHEDULING_ACNT_QUEUE = cpu_time_2_ms(p->p_accounting.time_in_queue);
	m_no_quantum.SCHEDULING_ACNT_DEQS      = p->p_accounting.dequeues;
	m_no_quantum.SCHEDULING_ACNT_IPC_SYNC  = p->p_accounting.ipc_sync;
	m_no_quantum.SCHEDULING_ACNT_IPC_ASYNC = p->p_accounting.ipc_async;
	m_no_quantum.SCHEDULING_ACNT_PREEMPT   = p->p_accounting.preempted;
	m_no_quantum.SCHEDULING_ACNT_CPU       = cpuid;
	m_no_quantum.SCHEDULING_ACNT_CPU_LOAD  = cpu_load();

	/* Reset accounting */
	reset_proc_accounting(p);

	if ((err = mini_send(p, p->p_scheduler->p_endpoint,
					&m_no_quantum, FROM_KERNEL))) {
		panic("WARNING: Scheduling: mini_send returned %d\n", err);
	}
}

void proc_no_time(struct proc * p)
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
		RTS_SET(p, RTS_PREEMPTED);
		RTS_UNSET(p, RTS_PREEMPTED);
#endif
	}
}

void reset_proc_accounting(struct proc *p)
{
  p->p_accounting.preempted = 0;
  p->p_accounting.ipc_sync  = 0;
  p->p_accounting.ipc_async = 0;
  p->p_accounting.dequeues  = 0;
  make_zero64(p->p_accounting.time_in_queue);
  make_zero64(p->p_accounting.enter_queue);
}
	
void copr_not_available_handler(void)
{
	struct proc * p;
	struct proc ** local_fpu_owner;
	/*
	 * Disable the FPU exception (both for the kernel and for the process
	 * once it's scheduled), and initialize or restore the FPU state.
	 */

	disable_fpu_exception();

	p = get_cpulocal_var(proc_ptr);

	/* if FPU is not owned by anyone, do not store anything */
	local_fpu_owner = get_cpulocal_var_ptr(fpu_owner);
	if (*local_fpu_owner != NULL) {
		assert(*local_fpu_owner != p);
		save_local_fpu(*local_fpu_owner, FALSE /*retain*/);
	}

	/*
	 * restore the current process' state and let it run again, do not
	 * schedule!
	 */
	if (restore_fpu(p) != OK) {
		/* Restoring FPU state failed. This is always the process's own
		 * fault. Send a signal, and schedule another process instead.
		 */
		*local_fpu_owner = NULL;		/* release FPU */
		cause_sig(proc_nr(p), SIGFPE);
		return;
	}

	*local_fpu_owner = p;
	context_stop(proc_addr(KERNEL));
	restore_user_context(p);
	NOT_REACHABLE;
}

void release_fpu(struct proc * p) {
	struct proc ** fpu_owner_ptr;

	fpu_owner_ptr = get_cpu_var_ptr(p->p_cpu, fpu_owner);

	if (*fpu_owner_ptr == p)
		*fpu_owner_ptr = NULL;
}
