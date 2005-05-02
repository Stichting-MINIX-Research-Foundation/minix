/* Global variables used in the kernel. This file contains the declarations;
 * storage space for the variables is allocated in table.c, because EXTERN is
 * defined as extern unless the _TABLE definition is seen. 
 */
#ifdef _TABLE
#undef EXTERN
#define EXTERN
#endif

/* MINIX' shutdown sequence uses watchdog timers to stop system services. The
 * flag shutting_down must be initialized to FALSE. We rely on the compiler's 
 * default initialization (0) of global variables here.
 */
EXTERN int skip_stop_sequence;	/* set to TRUE in case of an exception() */
EXTERN int shutting_down;	/* TRUE if the system is shutting down */
EXTERN struct proc *shutdown_process;	/* process awaiting shutdown of */
EXTERN timer_t shutdown_timer;  /* watchdog function called after timeout */ 

/* Kernel information structures. This groups vital kernel information. */
EXTERN phys_bytes aout;		/* address of a.out headers */
EXTERN struct kinfo kinfo;	/* kernel information for users */
EXTERN struct machine machine;	/* machine information for users */
EXTERN struct kmessages kmess;  /* diagnostic messages in kernel */
EXTERN struct memory mem[NR_MEMS];	/* base and size of chunks of memory */

/* Low level notifications may be put on the 'held' queue to prevent races. */
EXTERN struct proc *held_head;	/* head of queue of held-up interrupts */
EXTERN struct proc *held_tail;	/* tail of queue of held-up interrupts */
EXTERN unsigned char k_reenter;	/* kernel reentry count (entry count less 1)*/

/* Process table.  Here to stop too many things having to include proc.h. */
EXTERN struct proc *proc_ptr;	/* pointer to currently running process */

/* Miscellaneous. */
EXTERN unsigned lost_ticks;	/* clock ticks counted outside the clock task */

#if (CHIP == INTEL)

/* Interrupt related variables. */
EXTERN irq_hook_t irq_hooks[NR_IRQ_HOOKS];	/* hooks for general use */
EXTERN irq_hook_t *irq_handlers[NR_IRQ_VECTORS];/* list of IRQ handlers */
EXTERN int irq_actids[NR_IRQ_VECTORS];		/* IRQ ID bits active */
EXTERN int irq_use;			/* bit map of all in-use irq's */

/* Miscellaneous. */
EXTERN reg_t mon_ss, mon_sp;	/* monitor stack */
EXTERN int mon_return;		/* true if return to the monitor possible */

/* Variables that are initialized elsewhere are just extern here. */
extern struct system_image image[]; 	/* system image processes (table.c) */
extern char *t_stack[];		/* stack space for kernel tasks (table.c) */
extern struct segdesc_s gdt[];	/* protected mode global descriptor (protect.c) */

EXTERN _PROTOTYPE( void (*level0_func), (void) );
#endif /* (CHIP == INTEL) */

#if (CHIP == M68000)
/* M68000 specific variables go here. */
#endif


