#ifndef TYPE_H
#define TYPE_H

typedef _PROTOTYPE( void task_t, (void) );

/* Type accepted by kprintf(). This is a hack to accept both integers and
 * char pointers in the same argument. 
 */
typedef long karg_t;			/* use largest type here */

/* Process related types. 
 * A process number defines the index into the process table. With a signed
 * short we can support up to 256 user processes and more kernel tasks than
 * one can ever create.
 */ 
typedef short proc_nr_t;		/* process table entry number */
typedef unsigned long send_mask_t;	/* bit mask for sender */

struct system_image {
  proc_nr_t proc_nr;			/* process number to use */
  task_t *initial_pc;			/* start function for tasks */
  int type;				/* type of process */
  int priority;				/* scheduling priority */
  int stksize;				/* stack size for tasks */
  char call_mask;			/* allowed system calls */
  send_mask_t sendmask;			/* send mask protection */
  char proc_name[P_NAME_LEN];		/* name in process table */
};

struct memory {
  phys_clicks base;			/* start address of chunk */
  phys_clicks size;			/* size of memory chunk */
};

typedef unsigned long notify_mask_t;	/* bit mask for notifications */
typedef short notify_type_t;		/* notification type */
typedef char notify_flags_t;		/* notification flags */
typedef int notify_arg_t;		/* notification argument */

struct notification {
  proc_nr_t 	 n_source;		/* sender of notification */
  notify_type_t	 n_type;		/* notification type */
  notify_arg_t 	 n_arg;			/* notification argument */
  notify_flags_t n_flags;		/* notification flags */
  struct notification* n_next;		/* pointer to next notification */
};

/* The kernel outputs diagnostic messages in a circular buffer. */
struct kmessages {
  int km_next;				/* next index to write */
  int km_size;				/* current size in buffer */
  char km_buf[KMESS_BUF_SIZE];		/* buffer for messages */
};

struct randomness {
  int r_next;				/* next index to write */
  int r_size;				/* number of random elements */
  unsigned long r_buf[RANDOM_ELEMENTS]; /* buffer for random info */
};

#if (CHIP == INTEL)
typedef unsigned reg_t;		/* machine register */

/* The stack frame layout is determined by the software, but for efficiency
 * it is laid out so the assembly code to use it is as simple as possible.
 * 80286 protected mode and all real modes use the same frame, built with
 * 16-bit registers.  Real mode lacks an automatic stack switch, so little
 * is lost by using the 286 frame for it.  The 386 frame differs only in
 * having 32-bit registers and more segment registers.  The same names are
 * used for the larger registers to avoid differences in the code.
 */
struct stackframe_s {           /* proc_ptr points here */
#if _WORD_SIZE == 4
  u16_t gs;                     /* last item pushed by save */
  u16_t fs;                     /*  ^ */
#endif
  u16_t es;                     /*  | */
  u16_t ds;                     /*  | */
  reg_t di;			/* di through cx are not accessed in C */
  reg_t si;			/* order is to match pusha/popa */
  reg_t fp;			/* bp */
  reg_t st;			/* hole for another copy of sp */
  reg_t bx;                     /*  | */
  reg_t dx;                     /*  | */
  reg_t cx;                     /*  | */
  reg_t retreg;			/* ax and above are all pushed by save */
  reg_t retadr;			/* return address for assembly code save() */
  reg_t pc;			/*  ^  last item pushed by interrupt */
  reg_t cs;                     /*  | */
  reg_t psw;                    /*  | */
  reg_t sp;                     /*  | */
  reg_t ss;                     /* these are pushed by CPU during interrupt */
};

struct segdesc_s {		/* segment descriptor for protected mode */
  u16_t limit_low;
  u16_t base_low;
  u8_t base_middle;
  u8_t access;			/* |P|DL|1|X|E|R|A| */
  u8_t granularity;		/* |G|X|0|A|LIMT| */
  u8_t base_high;
};

typedef unsigned long irq_policy_t;	

typedef struct irq_hook {
  struct irq_hook *next;		/* next hook in chain */
  int (*handler)(struct irq_hook *);	/* interrupt handler */
  int irq;				/* IRQ vector number */ 
  int id;				/* id of this hook */
  int proc_nr;				/* NONE if not in use */
  irq_policy_t policy;			/* bit mask for policy */
} irq_hook_t;

typedef int (*irq_handler_t)(struct irq_hook *);

#endif /* (CHIP == INTEL) */

#if (CHIP == M68000)
/* M68000 specific types go here. */
#endif /* (CHIP == M68000) */

#endif /* TYPE_H */
