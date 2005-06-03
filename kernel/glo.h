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

/* Variables relating to shutting down MINIX. */
EXTERN char kernel_exception;		/* TRUE after system exceptions */
EXTERN char shutting_down;		/* TRUE if shutting down */
EXTERN struct proc *shutdown_process;	/* process awaiting shutdown of */
EXTERN timer_t shutdown_timer;  	/* timer for watchdog function */ 

/* Kernel information structures. This groups vital kernel information. */
EXTERN phys_bytes aout;			/* address of a.out headers */
EXTERN struct kinfo kinfo;		/* kernel information for users */
EXTERN struct machine machine;		/* machine information for users */
EXTERN struct kmessages kmess;  	/* diagnostic messages in kernel */
EXTERN struct randomness krandom;	/* gather kernel random information */
EXTERN struct memory mem[NR_MEMS];	/* base and size of chunks of memory */

/* Process scheduling information and the kernel reentry count. */
EXTERN struct proc *proc_ptr;	/* pointer to currently running process */
EXTERN struct proc *next_ptr;	/* pointer to next process to run */
EXTERN char k_reenter;		/* kernel reentry count (entry count less 1) */
EXTERN unsigned lost_ticks;	/* clock ticks counted outside clock task */

/* Declare buffer space and a bit map for notification messages. */
EXTERN struct notification notify_buffer[NR_NOTIFY_BUFS];
EXTERN bitchunk_t notify_bitmap[BITMAP_CHUNKS(NR_NOTIFY_BUFS)];     


#if (CHIP == INTEL)

/* Interrupt related variables. */
EXTERN irq_hook_t irq_hooks[NR_IRQ_HOOKS];	/* hooks for general use */
EXTERN irq_hook_t *irq_handlers[NR_IRQ_VECTORS];/* list of IRQ handlers */
EXTERN int irq_actids[NR_IRQ_VECTORS];		/* IRQ ID bits active */
EXTERN int irq_use;				/* map of all in-use irq's */

/* Data structure to store lock() timing data. */
#if ENABLE_LOCK_TIMING
EXTERN struct lock_timedata timingdata[TIMING_CATEGORIES];
#endif

/* Miscellaneous. */
EXTERN reg_t mon_ss, mon_sp;		/* boot monitor stack */
EXTERN int mon_return;			/* true if we can return to monitor */

/* Variables that are initialized elsewhere are just extern here. */
extern struct system_image image[]; 	/* system image processes */
extern char *t_stack[];			/* task stack space */
extern struct segdesc_s gdt[];		/* global descriptor table */

EXTERN _PROTOTYPE( void (*level0_func), (void) );
#endif /* (CHIP == INTEL) */

#if (CHIP == M68000)
/* M68000 specific variables go here. */
#endif

