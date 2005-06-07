#ifndef PROC_H
#define PROC_H

/* Here is the declaration of the process table.  It contains the process'
 * registers, memory map, accounting, and message send/receive information.
 * Many assembly code routines reference fields in it.  The offsets to these
 * fields are defined in the assembler include file sconst.h.  When changing
 * 'proc', be sure to change sconst.h to match.
 *
 * Changes:
 *   May 24, 2005   new field for pending notifications  (Jorrit N. Herder) 
 *   Nov 10, 2004   separated process types/ priorities  (Jorrit N. Herder)
 *   Sep 24, 2004   one timer per type of alarm  (Jorrit N. Herder)
 *   May 01, 2004   new p_sendmask to protect syscalls  (Jorrit N. Herder)
 */
#include <minix/com.h>
#include "protect.h"
#include "const.h"
 
struct proc {
  struct stackframe_s p_reg;	/* process' registers saved in stack frame */

#if (CHIP == INTEL)
  reg_t p_ldt_sel;		/* selector in gdt with ldt base and limit */
  struct segdesc_s p_ldt[2+NR_REMOTE_SEGS]; /* CS, DS and remote segments */
#endif /* (CHIP == INTEL) */

#if (CHIP == M68000)
/* M68000 specific registers and FPU details go here. */
#endif /* (CHIP == M68000) */

  reg_t *p_stguard;		/* stack guard word */

  proc_nr_t p_nr;		/* number of this process (for fast access) */
  struct mem_map p_memmap[NR_LOCAL_SEGS];   /* local memory map (T, D, S) */
  struct far_mem p_farmem[NR_REMOTE_SEGS];  /* remote memory map */

  char p_flags;			/* SENDING, RECEIVING, etc. */
  char p_type;			/* task, system, driver, server, user, idle */
  char p_priority;		/* scheduling priority */
  char p_call_mask;		/* bit map with allowed system call traps */

  send_mask_t p_sendmask;	/* mask indicating to whom proc may send */

  clock_t p_user_time;		/* user time in ticks */
  clock_t p_sys_time;		/* sys time in ticks */

  struct proc *p_nextready;	/* pointer to next ready process */
  struct notification *p_ntf_q;	/* queue of pending notifications */
  struct proc *p_caller_q;	/* head of list of procs wishing to send */
  struct proc *p_q_link;	/* link to next proc wishing to send */
  message *p_messbuf;		/* pointer to passed message buffer */
  proc_nr_t p_getfrom;		/* from whom does process want to receive? */
  proc_nr_t p_sendto;		/* to whom does process want to send? */

  timer_t p_alarm_timer;	/* timer shared by different alarm types */ 
  sigset_t p_pending;		/* bit map for pending kernel signals */
  unsigned p_pendcount;		/* count of pending and unfinished signals */

  char p_name[P_NAME_LEN];	/* name of the process, including \0 */

#if ENABLE_K_DEBUGGING
  int p_ready, p_found;
#endif
};

/* Guard word for task stacks. */
#define STACK_GUARD	((reg_t) (sizeof(reg_t) == 2 ? 0xBEEF : 0xDEADBEEF))

/* Bits for p_flags in proc[].  A process is runnable iff p_flags == 0. */
#define NO_MAP		0x01	/* keeps unmapped forked child from running */
#define SENDING		0x02	/* process blocked trying to SEND */
#define RECEIVING	0x04	/* process blocked trying to RECEIVE */
#define PENDING		0x10	/* set when inform() of signal pending */
#define SIG_PENDING	0x20	/* keeps to-be-signalled proc from running */
#define P_STOP		0x40	/* set when process is being traced */

/* Values for p_type. Non-negative values represent active process types. 
 * Process types are important to model inter-process relationships. When 
 * MINIX is shutdown, all system services are notified in order of possible
 * dependencies, so that, e.g., the FS can rely on drivers to synchronize.
 */
#define P_RESERVED     -2	/* slot is not in use, but reserved */
#define P_NONE         -1	/* slot is not in use, and free */
#define P_TASK		0	/* kernel process */
#define P_SYSTEM        1	/* low-level system service */
#define P_DRIVER        2	/* device driver */
#define P_SERVER	3	/* system service outside the kernel */
#define P_USER		4	/* user process */
#define P_IDLE		5	/* idle process */

/* Scheduling priorities for p_priority. Values must start at zero and 
 * increment. Priorities of system services can be set in the task table.
 * Task, user, and idle priorities are fixed; the rest can be selected. 
 */
#define PPRI_TASK	0	/* reserved for kernel tasks */
#define PPRI_HIGHER	1	 
#define PPRI_HIGH	2	
#define PPRI_NORMAL	3	
#define PPRI_LOW	4
#define PPRI_LOWER	5	
#define PPRI_USER	6	/* reserved for user processes */
#define PPRI_IDLE	7	/* only IDLE process goes here */

#define NR_SCHED_QUEUES 8	/* MUST equal minimum priority + 1 */

/* Magic process table addresses. */
#define BEG_PROC_ADDR (&proc[0])
#define BEG_USER_ADDR (&proc[NR_TASKS])
#define END_PROC_ADDR (&proc[NR_TASKS + NR_PROCS])

#define NIL_PROC          ((struct proc *) 0)
#define cproc_addr(n)     (&(proc + NR_TASKS)[(n)])
#define proc_addr(n)      (pproc_addr + NR_TASKS)[(n)]
#define proc_nr(p) 	  ((p)->p_nr)

#define iskerneltask(n)	  ((n) == CLOCK || (n) == SYSTASK) 
#define isidlehardware(n) ((n) == IDLE || (n) == HARDWARE)
#define isokprocn(n)      ((unsigned) ((n) + NR_TASKS) < NR_PROCS + NR_TASKS)
#define isokprocp(p)      ((p) >= BEG_PROC_ADDR && (p) < END_PROC_ADDR)
#define isalive(n)	  (proc_addr(n)->p_type > P_NONE)
#define isalivep(p)	  ((p)->p_type > P_NONE)
#define isemptyp(p)       ((p)->p_type == P_NONE)
#define istaskp(p)        ((p)->p_type == P_TASK)
#define isdriverp(p)      ((p)->p_type == P_DRIVER)
#define isserverp(p)      ((p)->p_type == P_SERVER)
#define isuserp(p)        ((p)->p_type == P_USER)
#define isidlep(p)        ((p)->p_type == P_IDLE)

#define isuser(n)	  (proc_addr(n)->p_type == P_USER)

/* The process table and pointers to process table slots. The pointers allow
 * faster access because now a process entry can be found by indexing the
 * pproc_addr array, while accessing an element i requires a multiplication
 * with sizeof(struct proc) to determine the address. 
 */
EXTERN struct proc proc[NR_TASKS + NR_PROCS];	/* process table */
EXTERN struct proc *pproc_addr[NR_TASKS + NR_PROCS];
EXTERN struct proc *bill_ptr;	/* ptr to process to bill for clock ticks */
EXTERN struct proc *rdy_head[NR_SCHED_QUEUES]; /* ptrs to ready list headers */
EXTERN struct proc *rdy_tail[NR_SCHED_QUEUES]; /* ptrs to ready list tails */

#endif /* PROC_H */
