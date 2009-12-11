#ifndef TYPE_H
#define TYPE_H

#include <minix/com.h>
#include <ibm/interrupt.h>

typedef _PROTOTYPE( void task_t, (void) );

/* Process table and system property related types. */ 
typedef int proc_nr_t;			/* process table entry number */
typedef short sys_id_t;			/* system process index */
typedef struct {			/* bitmap for system indexes */
  bitchunk_t chunk[BITMAP_CHUNKS(NR_SYS_PROCS)];
} sys_map_t;

struct boot_image {
  proc_nr_t proc_nr;			/* process number to use */
  task_t *initial_pc;			/* start function for tasks */
  int flags;				/* process flags */
  unsigned char quantum;		/* quantum (tick count) */
  int priority;				/* scheduling priority */
  int stksize;				/* stack size for tasks */
  char proc_name[P_NAME_LEN];		/* name in process table */
  endpoint_t endpoint;			/* endpoint number when started */
};

typedef unsigned long irq_policy_t;	
typedef unsigned long irq_id_t;	

typedef struct irq_hook {
  struct irq_hook *next;		/* next hook in chain */
  int (*handler)(struct irq_hook *);	/* interrupt handler */
  int irq;				/* IRQ vector number */ 
  int id;				/* id of this hook */
  int proc_nr_e;			/* (endpoint) NONE if not in use */
  irq_id_t notify_id;			/* id to return on interrupt */
  irq_policy_t policy;			/* bit mask for policy */
} irq_hook_t;

typedef int (*irq_handler_t)(struct irq_hook *);

#endif /* TYPE_H */
