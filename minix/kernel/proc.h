#ifndef PROC_H
#define PROC_H

#include <minix/const.h>
#include <sys/cdefs.h>

#ifndef __ASSEMBLY__

/* Here is the declaration of the process table.  It contains all process
 * data, including registers, flags, scheduling priority, memory map, 
 * accounting, message passing (IPC) information, and so on. 
 *
 * Many assembly code routines reference fields in it.  The offsets to these
 * fields are defined in the assembler include file sconst.h.  When changing
 * struct proc, be sure to change sconst.h to match.
 */
#include <minix/com.h>
#include <minix/portio.h>
#include "const.h"
#include "priv.h"

struct proc {
  struct stackframe_s p_reg;	/* process' registers saved in stack frame */
  struct segframe p_seg;	/* segment descriptors */
  proc_nr_t p_nr;		/* number of this process (for fast access) */
  struct priv *p_priv;		/* system privileges structure */
  volatile u32_t p_rts_flags;	/* process is runnable only if zero */
  volatile u32_t p_misc_flags;	/* flags that do not suspend the process */

  char p_priority;		/* current process priority */
  u64_t p_cpu_time_left;	/* time left to use the cpu */
  unsigned p_quantum_size_ms;	/* assigned time quantum in ms
				   FIXME remove this */
  struct proc *p_scheduler;	/* who should get out of quantum msg */
  unsigned p_cpu;		/* what CPU is the process running on */
#ifdef CONFIG_SMP
  bitchunk_t p_cpu_mask[BITMAP_CHUNKS(CONFIG_MAX_CPUS)]; /* what CPUs is the
							    process allowed to
							    run on */
  bitchunk_t p_stale_tlb[BITMAP_CHUNKS(CONFIG_MAX_CPUS)]; /* On which cpu are
				possibly stale entries from this process and has
				to be fresed the next kernel touches this
				processes memory
				 */
#endif

  /* Accounting statistics that get passed to the process' scheduler */
  struct {
	u64_t enter_queue;	/* time when enqueued (cycles) */
	u64_t time_in_queue;	/* time spent in queue */
	unsigned long dequeues;
	unsigned long ipc_sync;
	unsigned long ipc_async;
	unsigned long preempted;
  } p_accounting;

  clock_t p_dequeued;		/* uptime at which process was last dequeued */

  clock_t p_user_time;		/* user time in ticks */
  clock_t p_sys_time;		/* sys time in ticks */

  clock_t p_virt_left;		/* number of ticks left on virtual timer */
  clock_t p_prof_left;		/* number of ticks left on profile timer */

  u64_t p_cycles;		/* how many cycles did the process use */
  u64_t p_kcall_cycles;		/* kernel cycles caused by this proc (kcall) */
  u64_t p_kipc_cycles;		/* cycles caused by this proc (ipc) */

  u64_t p_tick_cycles;		/* cycles accumulated for up to a clock tick */
  struct cpuavg p_cpuavg;	/* running CPU average, for ps(1) */

  struct proc *p_nextready;	/* pointer to next ready process */
  struct proc *p_caller_q;	/* head of list of procs wishing to send */
  struct proc *p_q_link;	/* link to next proc wishing to send */
  endpoint_t p_getfrom_e;	/* from whom does process want to receive? */
  endpoint_t p_sendto_e;	/* to whom does process want to send? */

  sigset_t p_pending;		/* bit map for pending kernel signals */

  char p_name[PROC_NAME_LEN];	/* name of the process, including \0 */

  endpoint_t p_endpoint;	/* endpoint number, generation-aware */

  message p_sendmsg;		/* Message from this process if SENDING */
  message p_delivermsg;		/* Message for this process if MF_DELIVERMSG */
  vir_bytes p_delivermsg_vir;	/* Virtual addr this proc wants message at */

  /* If handler functions detect a process wants to do something with
   * memory that isn't present, VM has to fix it. Until it has asked
   * what needs to be done and fixed it, save necessary state here.
   *
   * The requester gets a copy of its request message in reqmsg and gets
   * VMREQUEST set.
   */
  struct {
	struct proc	*nextrestart;	/* next in vmrestart chain */
	struct proc	*nextrequestor;	/* next in vmrequest chain */
#define VMSTYPE_SYS_NONE	0
#define VMSTYPE_KERNELCALL	1
#define VMSTYPE_DELIVERMSG	2
#define VMSTYPE_MAP		3

	int		type;		/* suspended operation */
	union ixfer_saved{
		/* VMSTYPE_SYS_MESSAGE */
		message		reqmsg;	/* suspended request message */
	} saved;

	/* Parameters of request to VM */
	int		req_type;
	endpoint_t	target;
	union ixfer_params{
		struct {
			vir_bytes 	start, length;	/* memory range */
			u8_t		writeflag;	/* nonzero for write access */
		} check;
	} params;
	/* VM result when available */
	int		vmresult;

	/* If the suspended operation is a sys_call, its details are
	 * stored here.
	 */
  } p_vmrequest;

  int p_found;	/* consistency checking variables */
  int p_magic;		/* check validity of proc pointers */

  /* if MF_SC_DEFER is set, this struct is valid and contains the
   * do_ipc() arguments that are still to be executed
   */
  struct { reg_t r1, r2, r3; } p_defer;

#if DEBUG_TRACE
  int p_schedules;
#endif
};

#endif /* __ASSEMBLY__ */

/* Bits for the runtime flags. A process is runnable iff p_rts_flags == 0. */
#define RTS_SLOT_FREE	0x01	/* process slot is free */
#define RTS_PROC_STOP	0x02	/* process has been stopped */
#define RTS_SENDING	0x04	/* process blocked trying to send */
#define RTS_RECEIVING	0x08	/* process blocked trying to receive */
#define RTS_SIGNALED	0x10	/* set when new kernel signal arrives */
#define RTS_SIG_PENDING	0x20	/* unready while signal being processed */
#define RTS_P_STOP	0x40	/* set when process is being traced */
#define RTS_NO_PRIV	0x80	/* keep forked system process from running */
#define RTS_NO_ENDPOINT	0x100	/* process cannot send or receive messages */
#define RTS_VMINHIBIT	0x200	/* not scheduled until pagetable set by VM */
#define RTS_PAGEFAULT	0x400	/* process has unhandled pagefault */
#define RTS_VMREQUEST	0x800	/* originator of vm memory request */
#define RTS_VMREQTARGET	0x1000	/* target of vm memory request */
#define RTS_PREEMPTED	0x4000	/* this process was preempted by a higher
				   priority process and we should pick a new one
				   to run. Processes with this flag should be
				   returned to the front of their current
				   priority queue if they are still runnable
				   before we pick a new one
				 */
#define RTS_NO_QUANTUM	0x8000	/* process ran out of its quantum and we should
				   pick a new one. Process was dequeued and
				   should be enqueued at the end of some run
				   queue again */
#define RTS_BOOTINHIBIT	0x10000	/* not ready until VM has made it */

/* A process is runnable iff p_rts_flags == 0. */
#define rts_f_is_runnable(flg)	((flg) == 0)
#define proc_is_runnable(p)	(rts_f_is_runnable((p)->p_rts_flags))

#define proc_is_preempted(p)	((p)->p_rts_flags & RTS_PREEMPTED)
#define proc_no_quantum(p)	((p)->p_rts_flags & RTS_NO_QUANTUM)
#define proc_ptr_ok(p)		((p)->p_magic == PMAGIC)
#define proc_used_fpu(p)	((p)->p_misc_flags & (MF_FPU_INITIALIZED))

/* test whether the process is scheduled by the kernel's default policy  */
#define proc_kernel_scheduler(p)	((p)->p_scheduler == NULL || \
					(p)->p_scheduler == (p))

/* Macro to return: on which process is a certain process blocked?
 * return endpoint number (can be ANY) or NONE. It's important to
 * check RTS_SENDING first, and then RTS_RECEIVING, as they could
 * both be on (if a ipc_sendrec() blocks on sending), and p_getfrom_e
 * could be nonsense even though RTS_RECEIVING is on.
 */
#define P_BLOCKEDON(p)							\
	(								\
		((p)->p_rts_flags & RTS_SENDING) ? 			\
		(p)->p_sendto_e : 					\
		(							\
			(						\
				((p)->p_rts_flags & RTS_RECEIVING) ?	\
				(p)->p_getfrom_e : 			\
				NONE					\
			) 						\
		)							\
	)

/* These runtime flags can be tested and manipulated by these macros. */

#define RTS_ISSET(rp, f) (((rp)->p_rts_flags & (f)) == (f))


/* Set flag and dequeue if the process was runnable. */
#define RTS_SET(rp, f)							\
	do {								\
		const int rts = (rp)->p_rts_flags;			\
		(rp)->p_rts_flags |= (f);				\
		if(rts_f_is_runnable(rts) && !proc_is_runnable(rp)) {	\
			dequeue(rp);					\
		}							\
	} while(0)

/* Clear flag and enqueue if the process was not runnable but is now. */
#define RTS_UNSET(rp, f) 						\
	do {								\
		int rts;						\
		rts = (rp)->p_rts_flags;				\
		(rp)->p_rts_flags &= ~(f);				\
		if(!rts_f_is_runnable(rts) && proc_is_runnable(rp)) {	\
			enqueue(rp);					\
		}							\
	} while(0)

/* Set flags to this value. */
#define RTS_SETFLAGS(rp, f)					\
	do {								\
		if(proc_is_runnable(rp) && (f)) { dequeue(rp); }		\
		(rp)->p_rts_flags = (f);				\
	} while(0)

/* Misc flags */
#define MF_REPLY_PEND	0x001	/* reply to IPC_REQUEST is pending */
#define MF_VIRT_TIMER	0x002	/* process-virtual timer is running */
#define MF_PROF_TIMER	0x004	/* process-virtual profile timer is running */
#define MF_KCALL_RESUME 0x008	/* processing a kernel call was interrupted,
				   most likely because we need VM to resolve a
				   problem or a long running copy was preempted.
				   We need to resume the kernel call execution
				   now
				 */
#define MF_DELIVERMSG	0x040	/* Copy message for him before running */
#define MF_SIG_DELAY	0x080	/* Send signal when no longer sending */
#define MF_SC_ACTIVE	0x100	/* Syscall tracing: in a system call now */
#define MF_SC_DEFER	0x200	/* Syscall tracing: deferred system call */
#define MF_SC_TRACE	0x400	/* Syscall tracing: trigger syscall events */
#define MF_FPU_INITIALIZED	0x1000  /* process already used math, so fpu
					 * regs are significant (initialized)*/
#define MF_SENDING_FROM_KERNEL	0x2000 /* message of this process is from kernel */
#define MF_CONTEXT_SET	0x4000 /* don't touch context */
#define MF_SPROF_SEEN	0x8000 /* profiling has seen this process */
#define MF_FLUSH_TLB	0x10000	/* if set, TLB must be flushed before letting
				   this process run again. Currently it only
				   applies to SMP */
#define MF_SENDA_VM_MISS 0x20000 /* set if a processes wanted to receive an asyn
				    message from this sender but could not
				    because of VM modifying the sender's address
				    space*/
#define MF_STEP		 0x40000 /* Single-step process */
#define MF_MSGFAILED	 0x80000
#define MF_NICED	0x100000 /* user has lowered max process priority */

/* Magic process table addresses. */
#define BEG_PROC_ADDR (&proc[0])
#define BEG_USER_ADDR (&proc[NR_TASKS])
#define END_PROC_ADDR (&proc[NR_TASKS + NR_PROCS])

#define proc_addr(n)      (&(proc[NR_TASKS + (n)]))
#define proc_nr(p) 	  ((p)->p_nr)

#define isokprocn(n)      ((unsigned) ((n) + NR_TASKS) < NR_PROCS + NR_TASKS)
#define isemptyn(n)       isemptyp(proc_addr(n)) 
#define isemptyp(p)       ((p)->p_rts_flags == RTS_SLOT_FREE)
#define iskernelp(p)	  ((p) < BEG_USER_ADDR)
#define iskerneln(n)	  ((n) < 0)
#define isuserp(p)        isusern((p) >= BEG_USER_ADDR)
#define isusern(n)        ((n) >= 0)
#define isrootsysn(n)	  ((n) == ROOT_SYS_PROC_NR)

#ifndef __ASSEMBLY__

EXTERN struct proc proc[NR_TASKS + NR_PROCS];	/* process table */

int mini_send(struct proc *caller_ptr, endpoint_t dst_e, message *m_ptr,
	int flags);

#endif /* __ASSEMBLY__ */

#endif /* PROC_H */
