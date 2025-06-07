#ifndef TYPE_H
#define TYPE_H

#include <minix/com.h>          // Kept
#include <machine/interrupt.h>  // Kept
#include <machine/multiboot.h>  // Kept

// Added kernel headers
#include <minix/kernel_types.h> // For base types like k_size_t if any typedefs expand to it
#include <klib/include/kprintf.h>
#include <klib/include/kstring.h>
#include <klib/include/kmemory.h>


/* Process table and system property related types. */ 
typedef int proc_nr_t;			/* process table entry number */
typedef short sys_id_t;			/* system process index */
typedef struct {			/* bitmap for system indexes */
  bitchunk_t chunk[BITMAP_CHUNKS(NR_SYS_PROCS)];
} sys_map_t;

typedef unsigned long irq_policy_t;	
typedef unsigned long irq_id_t;	

typedef struct irq_hook {
  struct irq_hook *next;		/* next hook in chain */
  int (*handler)(struct irq_hook *);	/* interrupt handler */
  int irq;				/* IRQ vector number */ 
  int id;				/* id of this hook */
  endpoint_t proc_nr_e;			/* (endpoint) NONE if not in use */
  irq_id_t notify_id;			/* id to return on interrupt */
  irq_policy_t policy;			/* bit mask for policy */
} irq_hook_t;

typedef int (*irq_handler_t)(struct irq_hook *);

#endif /* TYPE_H */
