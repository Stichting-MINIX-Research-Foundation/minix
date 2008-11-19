#ifndef GLO_H
#define GLO_H

/* Global variables used in the kernel. This file contains the declarations;
 * storage space for the variables is allocated in table.c, because EXTERN is
 * defined as extern unless the _TABLE definition is seen. We rely on the 
 * compiler's default initialization (0) for several global variables. 
 */
#ifdef _TABLE
#undef EXTERN
#define EXTERN
#endif

#include <minix/config.h>
#include <archtypes.h>
#include "config.h"

/* Variables relating to shutting down MINIX. */
EXTERN char kernel_exception;		/* TRUE after system exceptions */
EXTERN char shutdown_started;		/* TRUE after shutdowns / reboots */

/* Kernel information structures. This groups vital kernel information. */
EXTERN struct kinfo kinfo;		/* kernel information for users */
EXTERN struct machine machine;		/* machine information for users */
EXTERN struct kmessages kmess;  	/* diagnostic messages in kernel */
EXTERN struct randomness krandom;	/* gather kernel random information */
EXTERN struct loadinfo kloadinfo;	/* status of load average */

/* Process scheduling information and the kernel reentry count. */
EXTERN struct proc *prev_ptr;	/* previously running process */
EXTERN struct proc *proc_ptr;	/* pointer to currently running process */
EXTERN struct proc *next_ptr;	/* next process to run after restart() */
EXTERN struct proc *bill_ptr;	/* process to bill for clock ticks */
EXTERN struct proc *vmrestart;  /* first process on vmrestart queue */
EXTERN struct proc *vmrequest;  /* first process on vmrequest queue */
EXTERN struct proc *pagefaults; /* first process on pagefault queue */
EXTERN struct proc *softnotify;	/* first process on softnotify queue */
EXTERN char k_reenter;		/* kernel reentry count (entry count less 1) */
EXTERN unsigned lost_ticks;	/* clock ticks counted outside clock task */


/* Interrupt related variables. */
EXTERN irq_hook_t irq_hooks[NR_IRQ_HOOKS];	/* hooks for general use */
EXTERN int irq_actids[NR_IRQ_VECTORS];		/* IRQ ID bits active */
EXTERN int irq_use;				/* map of all in-use irq's */

EXTERN struct ipc_stats
{
	unsigned long deadproc;
	unsigned long bad_endpoint;
	unsigned long dst_not_allowed;
	unsigned long bad_call;
	unsigned long call_not_allowed;
	unsigned long bad_buffer;
	unsigned long deadlock;
	unsigned long not_ready;
	unsigned long src_died;
	unsigned long dst_died;
	unsigned long no_priv;
	unsigned long bad_size;
	unsigned long bad_senda;
	u64_t total;
} ipc_stats;
extern endpoint_t ipc_stats_target;

EXTERN struct system_stats
{
	unsigned long bad_req;
	unsigned long not_allowed;
	u64_t total;
} sys_stats;

/* Miscellaneous. */
EXTERN reg_t mon_ss, mon_sp;		/* boot monitor stack */
EXTERN int mon_return;			/* true if we can return to monitor */
EXTERN int do_serial_debug;
EXTERN endpoint_t who_e;		/* message source endpoint */
EXTERN int who_p;			/* message source proc */
EXTERN int sys_call_code;		/* kernel call number in SYSTEM */
EXTERN time_t boottime;
EXTERN char params_buffer[512];		/* boot monitor parameters */
EXTERN int minix_panicing;
EXTERN int locklevel;

EXTERN unsigned long cr3switch;
EXTERN unsigned long cr3reload;

/* VM */
EXTERN phys_bytes vm_base;
EXTERN phys_bytes vm_size;
EXTERN phys_bytes vm_mem_high;
EXTERN int vm_running;
EXTERN int must_notify_vm;

/* Verbose flags (debugging). */
EXTERN int verbose_vm;

/* Timing measurements. */
EXTERN struct lock_timingdata timingdata[TIMING_CATEGORIES];

/* Variables that are initialized elsewhere are just extern here. */
extern struct boot_image image[]; 	/* system image processes */
extern char *t_stack[];			/* task stack space */
extern struct segdesc_s gdt[];		/* global descriptor table */

EXTERN _PROTOTYPE( void (*level0_func), (void) );

#endif /* GLO_H */
