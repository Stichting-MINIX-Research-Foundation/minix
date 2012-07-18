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
#include <minix/ipcconst.h>
#include <machine/archtypes.h>
#include "archconst.h"
#include "config.h"
#include "debug.h"

/* Kernel information structures. This groups vital kernel information. */
extern struct kinfo kinfo;		  /* kernel information for users */
extern struct machine machine;		  /* machine information for users */
extern struct kmessages kmessages;  	  /* diagnostic messages in kernel */
extern struct loadinfo loadinfo;	  /* status of load average */
extern struct minix_kerninfo minix_kerninfo;

EXTERN struct k_randomness krandom; 	/* gather kernel random information */

vir_bytes minix_kerninfo_user;

#define kmess kmessages
#define kloadinfo loadinfo

/* Process scheduling information and the kernel reentry count. */
EXTERN struct proc *vmrequest;  /* first process on vmrequest queue */
EXTERN unsigned lost_ticks;	/* clock ticks counted outside clock task */
EXTERN char *ipc_call_names[IPCNO_HIGHEST+1]; /* human-readable call names */
EXTERN struct proc *kbill_kcall; /* process that made kernel call */
EXTERN struct proc *kbill_ipc; /* process that invoked ipc */

/* Interrupt related variables. */
EXTERN irq_hook_t irq_hooks[NR_IRQ_HOOKS];	/* hooks for general use */
EXTERN int irq_actids[NR_IRQ_VECTORS];		/* IRQ ID bits active */
EXTERN int irq_use;				/* map of all in-use irq's */
EXTERN u32_t system_hz;				/* HZ value */

/* Miscellaneous. */
EXTERN time_t boottime;
EXTERN int verboseboot;			/* verbose boot, init'ed in cstart */

#if DEBUG_TRACE
EXTERN int verboseflags;
#endif

#ifdef USE_APIC
EXTERN int config_no_apic; /* optionaly turn off apic */
EXTERN int config_apic_timer_x; /* apic timer slowdown factor */
#endif

EXTERN u64_t cpu_hz[CONFIG_MAX_CPUS];

#define cpu_set_freq(cpu, freq)	do {cpu_hz[cpu] = freq;} while (0)
#define cpu_get_freq(cpu)	cpu_hz[cpu]

#ifdef CONFIG_SMP
EXTERN int config_no_smp; /* optionaly turn off SMP */
#endif

/* VM */
EXTERN int vm_running;
EXTERN int catch_pagefaults;
EXTERN int kernel_may_alloc;

/* Variables that are initialized elsewhere are just extern here. */
extern struct boot_image image[NR_BOOT_PROCS]; 	/* system image processes */

EXTERN volatile int serial_debug_active;

EXTERN struct cpu_info cpu_info[CONFIG_MAX_CPUS];

/* BKL stats */
EXTERN u64_t kernel_ticks[CONFIG_MAX_CPUS];
EXTERN u64_t bkl_ticks[CONFIG_MAX_CPUS];
EXTERN unsigned bkl_tries[CONFIG_MAX_CPUS];
EXTERN unsigned bkl_succ[CONFIG_MAX_CPUS];

/* Feature flags */
EXTERN int minix_feature_flags;

#endif /* GLO_H */
