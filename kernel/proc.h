#ifndef PROC_H
#define PROC_H

/* Here is the declaration of the process table.  It contains all process
 * data, including registers, flags, scheduling priority, memory map, 
 * accounting, message passing (IPC) information, and so on. 
 *
 * Many assembly code routines reference fields in it.  The offsets to these
 * fields are defined in the assembler include file sconst.h.  When changing
 * struct proc, be sure to change sconst.h to match.
 */
#include <minix/com.h>
#include "protect.h"
#include "const.h"
 
struct proc {
  struct stackframe_s p_reg;	/* process' registers saved in stack frame */

#if (CHIP == INTEL)
  reg_t p_ldt_sel;		/* selector in gdt with ldt base and limit */
  struct segdesc_s p_ldt[2+NR_REMOTE_SEGS]; /* CS, DS and remote segments */
#endif 

#if (CHIP == M68000)
/* M68000 specific registers and FPU details go here. */
#endif 

  reg_t *p_stguard;		/* stack guard word */
  proc_nr_t p_nr;		/* number of this process (for fast access) */

  struct mem_map p_memmap[NR_LOCAL_SEGS];   /* local memory map (T, D, S) */
  struct far_mem p_farmem[NR_REMOTE_SEGS];  /* remote memory map */

  char p_flags;			/* SENDING, RECEIVING, etc. */
  char p_priority;		/* current scheduling priority */
  char p_max_priority;		/* maximum (default) scheduling priority */
  char p_used_quantums;		/* number of full quantums used in a row */
  char p_allowed_quantums;	/* maximum quantums allowed in a row */

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

  char p_name[P_NAME_LEN];	/* name of the process, including \0 */

#if ENABLE_K_DEBUGGING
  int p_ready, p_found;
#endif
};

/* Guard word for task stacks. */
#define STACK_GUARD	((reg_t) (sizeof(reg_t) == 2 ? 0xBEEF : 0xDEADBEEF))

/* Bits for the process flags.  A process is runnable iff p_flags == 0. */
#define SLOT_FREE	0x01	/* process slot is free */
#define NO_MAP		0x02	/* keeps unmapped forked child from running */
#define SENDING		0x04	/* process blocked trying to SEND */
#define RECEIVING	0x08	/* process blocked trying to RECEIVE */
#define SIGNALED	0x10	/* set when new kernel signal arrives */
#define SIG_PENDING	0x20	/* unready while signal being processed */
#define P_STOP		0x40	/* set when process is being traced */


/* Scheduling priorities for p_priority. Values must start at zero (highest
 * priority) and increment. Priorities of the processes in the boot image can 
 * be set in table.c.
 */
#define NR_SCHED_QUEUES    8	/* MUST equal minimum priority + 1 */
#define TASK_Q		   0	/* highest, reserved for kernel tasks */
#define USER_Q  	   4    /* default priority for user processes */   
#define IDLE_Q		   7    /* lowest, only IDLE process goes here */


/* Magic process table addresses. */
#define BEG_PROC_ADDR (&proc[0])
#define BEG_USER_ADDR (&proc[NR_TASKS])
#define END_PROC_ADDR (&proc[NR_TASKS + NR_PROCS])

#define NIL_PROC          ((struct proc *) 0)
#define cproc_addr(n)     (&(proc + NR_TASKS)[(n)])
#define proc_addr(n)      (pproc_addr + NR_TASKS)[(n)]
#define proc_nr(p) 	  ((p)->p_nr)

#define iskerneltask(n)	  ((n) == CLOCK || (n) == SYSTASK) 
#define isokprocn(n)      ((unsigned) ((n) + NR_TASKS) < NR_PROCS + NR_TASKS)
#define isokprocp(p)      ((p) >= BEG_PROC_ADDR && (p) < END_PROC_ADDR)

#define iskernelp(p)	  ((p)->p_nr < 0)
#define isuserp(p)        ((p)->p_nr >= 0)
#define isidlep(p)        ((p)->p_nr == IDLE)
#define isemptyp(p)       ((p)->p_flags == SLOT_FREE)

/* The process table and pointers to process table slots. The pointers allow
 * faster access because now a process entry can be found by indexing the
 * pproc_addr array, while accessing an element i requires a multiplication
 * with sizeof(struct proc) to determine the address. 
 */
EXTERN struct proc proc[NR_TASKS + NR_PROCS];	/* process table */
EXTERN struct proc *pproc_addr[NR_TASKS + NR_PROCS];
EXTERN struct proc *rdy_head[NR_SCHED_QUEUES]; /* ptrs to ready list headers */
EXTERN struct proc *rdy_tail[NR_SCHED_QUEUES]; /* ptrs to ready list tails */

#endif /* PROC_H */
