#ifndef TYPE_H
#define TYPE_H

#include <minix/com.h>
#include <machine/interrupt.h>

/* Process table and system property related types. */ 
typedef int proc_nr_t;			/* process table entry number */
typedef short sys_id_t;			/* system process index */
typedef struct {			/* bitmap for system indexes */
  bitchunk_t chunk[BITMAP_CHUNKS(NR_SYS_PROCS)];
} sys_map_t;

struct boot_image_memmap {
  phys_bytes text_vaddr;		/* Virtual start address of text */
  phys_bytes text_paddr;		/* Physical start address of text */
  phys_bytes text_bytes;		/* Text segment's size (bytes) */
  phys_bytes data_vaddr;		/* Virtual start address of data */
  phys_bytes data_paddr;		/* Physical start address of data */
  phys_bytes data_bytes;		/* Data segment's size (bytes) */
  phys_bytes stack_bytes;		/* Size of stack set aside (bytes) */
  phys_bytes entry;			/* Entry point of executable */
};

struct boot_image {
  proc_nr_t proc_nr;			/* process number to use */
  int flags;				/* process flags */
  int stack_kbytes;			/* stack size (in KB) */
  char proc_name[P_NAME_LEN];		/* name in process table */
  endpoint_t endpoint;			/* endpoint number when started */
  struct boot_image_memmap memmap;	/* memory map info for boot image */
};

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
