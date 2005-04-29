#ifndef TYPE_H
#define TYPE_H

typedef _PROTOTYPE( void task_t, (void) );
typedef _PROTOTYPE( int (*rdwt_t), (message *m_ptr) );
typedef _PROTOTYPE( void (*watchdog_t), (void) );

/* Type accepted by kprintf(). This is a hack to accept both integers and
 * char pointers in the same argument. 
 */
typedef long karg_t;			/* use largest type here */

typedef unsigned int notify_mask_t;	/* bit mask for notifications */
typedef unsigned long send_mask_t;	/* bit mask for sender */

struct system_image {
  int proc_nr;				/* process number to use */
  task_t *initial_pc;			/* start function for tasks */
  int type;				/* type of process */
  int priority;				/* scheduling priority */
  int stksize;				/* stack size for tasks */
  send_mask_t sendmask;			/* send mask protection */
  char name[PROC_NAME_LEN];		/* name in process table */
};

struct memory {
  phys_clicks base;			/* start address of chunk */
  phys_clicks size;			/* size of memory chunk */
};

struct bios {
  phys_bytes bios_addr;			/* physical address at BIOS */
  size_t bios_length;			/* size of value */
};


#if (CHIP == INTEL)
typedef u16_t port_t;
typedef U16_t Port_t;
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

typedef struct irq_hook {
  struct irq_hook *next;
  int (*handler)(struct irq_hook *);
  int irq;
  int id;
} irq_hook_t;

typedef int (*irq_handler_t)(struct irq_hook *);

/* The IRQ table is used to handle harware interrupts based on a policy set
 * by a device driver. The policy is stored with a SYS_IRQCTL system call and
 * used by a generic function to handle hardware interrupts in an appropriate
 * way for the device. 
 */
typedef unsigned long irq_policy_t;	
struct irqtab {
  irq_hook_t hook;	/* its irq hook */
  irq_policy_t policy;	/* bit mask for the policy */
  int proc_nr;		/* process number to be notified */
  long port;		/* port to be read or written */
  phys_bytes addr;	/* absolute address to store or get value */
  long mask_val;	/* mask for strobing or value to be written */
};

#endif /* (CHIP == INTEL) */

#if (CHIP == M68000)
/* M68000 specific types go here. */
#endif /* (CHIP == M68000) */

#endif /* TYPE_H */
