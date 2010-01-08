#ifndef PRIV_H
#define PRIV_H

/* Declaration of the system privileges structure. It defines flags, system 
 * call masks, an synchronous alarm timer, I/O privileges, pending hardware 
 * interrupts and notifications, and so on.
 * System processes each get their own structure with properties, whereas all 
 * user processes share one structure. This setup provides a clear separation
 * between common and privileged process fields and is very space efficient. 
 *
 * Changes:
 *   Nov 22, 2009  rewrite of privilege management (Cristiano Giuffrida)
 *   Jul 01, 2005  Created.  (Jorrit N. Herder)	
 */
#include <minix/com.h>
#include "const.h"
#include "type.h"

/* Max. number of I/O ranges that can be assigned to a process */
#define NR_IO_RANGE	32

/* Max. number of device memory ranges that can be assigned to a process */
#define NR_MEM_RANGE	10

/* Max. number of IRQs that can be assigned to a process */
#define NR_IRQ	4
 
struct priv {
  proc_nr_t s_proc_nr;		/* number of associated process */
  sys_id_t s_id;		/* index of this system structure */
  short s_flags;		/* PREEMTIBLE, BILLABLE, etc. */

  /* Asynchronous sends */
  vir_bytes s_asyntab;		/* addr. of table in process' address space */
  size_t s_asynsize;		/* number of elements in table. 0 when not in
				 * use
				 */

  short s_trap_mask;		/* allowed system call traps */
  sys_map_t s_ipc_to;		/* allowed destination processes */

  /* allowed kernel calls */
  bitchunk_t s_k_call_mask[SYS_CALL_MASK_SIZE];  

  sys_map_t s_notify_pending;  	/* bit map with pending notifications */
  irq_id_t s_int_pending;	/* pending hardware interrupts */
  sigset_t s_sig_pending;	/* pending signals */

  timer_t s_alarm_timer;	/* synchronous alarm timer */ 
  struct far_mem s_farmem[NR_REMOTE_SEGS];  /* remote memory map */
  reg_t *s_stack_guard;		/* stack guard word for kernel tasks */

  int s_nr_io_range;		/* allowed I/O ports */
  struct io_range s_io_tab[NR_IO_RANGE];

  int s_nr_mem_range;		/* allowed memory ranges */
  struct mem_range s_mem_tab[NR_MEM_RANGE];

  int s_nr_irq;			/* allowed IRQ lines */
  int s_irq_tab[NR_IRQ];
  vir_bytes s_grant_table;	/* grant table address of process, or 0 */
  int s_grant_entries;		/* no. of entries, or 0 */
};

/* Guard word for task stacks. */
#define STACK_GUARD	((reg_t) (sizeof(reg_t) == 2 ? 0xBEEF : 0xDEADBEEF))

/* Static privilege id definitions. */
#define NR_STATIC_PRIV_IDS         NR_BOOT_PROCS
#define is_static_priv_id(id)	   (id >= 0 && id < NR_STATIC_PRIV_IDS)
#define static_priv_id(n)          (NR_TASKS + (n))

/* Magic system structure table addresses. */
#define BEG_PRIV_ADDR              (&priv[0])
#define END_PRIV_ADDR              (&priv[NR_SYS_PROCS])
#define BEG_STATIC_PRIV_ADDR       BEG_PRIV_ADDR
#define END_STATIC_PRIV_ADDR       (BEG_STATIC_PRIV_ADDR + NR_STATIC_PRIV_IDS)
#define BEG_DYN_PRIV_ADDR          END_STATIC_PRIV_ADDR
#define END_DYN_PRIV_ADDR          END_PRIV_ADDR

#define priv_addr(i)      (ppriv_addr)[(i)]
#define priv_id(rp)	  ((rp)->p_priv->s_id)
#define priv(rp)	  ((rp)->p_priv)

#define id_to_nr(id)	priv_addr(id)->s_proc_nr
#define nr_to_id(nr)    priv(proc_addr(nr))->s_id

#define may_send_to(rp, nr) (get_sys_bit(priv(rp)->s_ipc_to, nr_to_id(nr)))

/* Privilege management shorthands. */
#define spi_to(n)          (1 << (static_priv_id(n)))
#define unset_usr_to(m)    ((m) & ~(1 << USER_PRIV_ID))

/* The system structures table and pointers to individual table slots. The 
 * pointers allow faster access because now a process entry can be found by 
 * indexing the psys_addr array, while accessing an element i requires a 
 * multiplication with sizeof(struct sys) to determine the address. 
 */
EXTERN struct priv priv[NR_SYS_PROCS];		/* system properties table */
EXTERN struct priv *ppriv_addr[NR_SYS_PROCS];	/* direct slot pointers */

/* Unprivileged user processes all share the privilege structure of the
 * root user process.
 * This id must be fixed because it is used to check send mask entries.
 */
#define USER_PRIV_ID	static_priv_id(ROOT_USR_PROC_NR)
/* Specifies a null privilege id.
 */
#define NULL_PRIV_ID	(-1)

/* Make sure the system can boot. The following sanity check verifies that
 * the system privileges table is large enough for the number of processes
 * in the boot image. 
 */
#if (NR_BOOT_PROCS > NR_SYS_PROCS)
#error NR_SYS_PROCS must be larger than NR_BOOT_PROCS
#endif

/*
 * Privileges masks used by the kernel.
 */
#define IDL_F     (SYS_PROC | BILLABLE) /* idle task is not preemptible as we
                                         * don't want it to interfere with the
                                         * timer tick interrupt handler code.
                                         * Unlike other processes idle task is
                                         * handled in a special way and is
                                         * preempted always if timer tick occurs
                                         * and there is another runnable process
                                         */
#define TSK_F     (SYS_PROC)                            /* other kernel tasks */
#define RSYS_F    (SYS_PROC | PREEMPTIBLE)              /* root system proc */
#define DEF_SYS_F (RSYS_F | DYN_PRIV_ID)                /* default sys proc */

/* allowed traps */
#define CSK_T     (1 << RECEIVE)                        /* clock and system */
#define TSK_T     0                                     /* other kernel tasks */
#define RSYS_T    (~0)                                  /* root system proc */
#define DEF_SYS_T RSYS_T                                /* default sys proc */

/* allowed targets */
#define TSK_M     0                                     /* all kernel tasks */
#define RSYS_M    (~0)                                  /* root system proc */
#define DEF_SYS_M unset_usr_to(RSYS_M)                  /* default sys proc */

/* allowed kernel calls */
#define NO_C 0              /* no calls allowed */
#define ALL_C 1             /* all calls allowed */
#define TSK_KC     NO_C                                 /* all kernel tasks */
#define RSYS_KC    ALL_C                                /* root system proc */
#define DEF_SYS_KC RSYS_KC                              /* default sys proc */

#endif /* PRIV_H */
