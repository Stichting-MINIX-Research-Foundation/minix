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
EXTERN struct kinfo kinfo;		/* kernel information for users */
EXTERN struct machine machine;		/* machine information for users */
EXTERN struct kmessages kmess;  	/* diagnostic messages in kernel */
EXTERN struct k_randomness krandom;	/* gather kernel random information */
EXTERN struct loadinfo kloadinfo;	/* status of load average */

/* Process scheduling information and the kernel reentry count. */
EXTERN struct proc *proc_ptr;	/* pointer to currently running process */
EXTERN struct proc *bill_ptr;	/* process to bill for clock ticks */
EXTERN struct proc *vmrequest;  /* first process on vmrequest queue */
EXTERN unsigned lost_ticks;	/* clock ticks counted outside clock task */
EXTERN char *ipc_call_names[IPCNO_HIGHEST+1]; /* human-readable call names */

/* Interrupt related variables. */
EXTERN irq_hook_t irq_hooks[NR_IRQ_HOOKS];	/* hooks for general use */
EXTERN int irq_actids[NR_IRQ_VECTORS];		/* IRQ ID bits active */
EXTERN int irq_use;				/* map of all in-use irq's */
EXTERN u32_t system_hz;				/* HZ value */

/* Miscellaneous. */
EXTERN reg_t mon_sp;			/* boot monitor stack */
EXTERN int mon_return;			/* true if we can return to monitor */
EXTERN int do_serial_debug;
EXTERN time_t boottime;
EXTERN char params_buffer[512];		/* boot monitor parameters */
EXTERN int minix_panicing;
EXTERN char fpu_presence;
EXTERN struct proc * fpu_owner;
EXTERN int verboseboot;			/* verbose boot, init'ed in cstart */
#define MAGICTEST 0xC0FFEE23
EXTERN u32_t magictest;			/* global magic number */

#if DEBUG_TRACE
EXTERN int verboseflags;
#endif

#ifdef CONFIG_APIC
EXTERN int config_no_apic; /* optionaly turn off apic */
#endif

EXTERN u64_t cpu_hz[CONFIG_MAX_CPUS];

#define cpu_set_freq(cpu, freq)	do {cpu_hz[cpu] = freq;} while (0)
#define cpu_get_freq(cpu)	cpu_hz[cpu]

/* VM */
EXTERN int vm_running;
EXTERN int catch_pagefaults;
EXTERN struct proc *ptproc;

/* Timing */
EXTERN util_timingdata_t timingdata[TIMING_CATEGORIES];

/* Variables that are initialized elsewhere are just extern here. */
extern struct boot_image image[]; 	/* system image processes */
extern char *t_stack[];			/* task stack space */
extern struct segdesc_s gdt[];		/* global descriptor table */

EXTERN volatile int serial_debug_active;

#endif /* GLO_H */
