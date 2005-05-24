#ifndef _TYPE_H
#define _TYPE_H

#ifndef _TYPES_H
#include <sys/types.h>
#endif

/* Type definitions. */
typedef unsigned int vir_clicks; /* virtual  addresses and lengths in clicks */
typedef unsigned long phys_bytes;/* physical addresses and lengths in bytes */
typedef unsigned int phys_clicks;/* physical addresses and lengths in clicks */

#if (CHIP == INTEL)
typedef unsigned int vir_bytes;	/* virtual addresses and lengths in bytes */
#endif

#if (CHIP == M68000)
typedef unsigned long vir_bytes;/* virtual addresses and lengths in bytes */
#endif

#if (CHIP == SPARC)
typedef unsigned long vir_bytes;/* virtual addresses and lengths in bytes */
#endif

/* Memory map for local text, stack, data segments. */
struct mem_map {
  vir_clicks mem_vir;		/* virtual address */
  phys_clicks mem_phys;		/* physical address */
  vir_clicks mem_len;		/* length */
};

/* Memory map for remote memory areas, e.g., for the RAM disk. */
struct far_mem {
  int in_use;			/* entry in use, unless zero */
  phys_clicks mem_phys;		/* physical address */
  vir_clicks mem_len;		/* length */
};

/* Structure for virtual copying by means of a vector with requests. */
struct vir_addr {
  int proc_nr;
  int segment;
  vir_bytes offset;
};

struct vir_cp_req {
  struct vir_addr src;
  struct vir_addr dst;
  phys_bytes count;
};

typedef struct {
  vir_bytes iov_addr;		/* address of an I/O buffer */
  vir_bytes iov_size;		/* sizeof an I/O buffer */
} iovec_t;

typedef struct {
  vir_bytes cpv_src;		/* src address of data */
  vir_bytes cpv_dst;		/* dst address of data */
  vir_bytes cpv_size;		/* size of data */
} cpvec_t;

/* virtual copy vector */
typedef struct {
  int cpvv_src_seg, cpvv_dst_seg;	/* src & dst segments */
  vir_bytes cpvv_src;			/* src address of data */
  vir_bytes cpvv_dst;			/* dst address of data */
  vir_bytes cpvv_size;			/* size of data */
} cpvvec_t;

/* MM passes the address of a structure of this type to KERNEL when
 * do_sendsig() is invoked as part of the signal catching mechanism.
 * The structure contain all the information that KERNEL needs to build
 * the signal stack.
 */
struct sigmsg {
  int sm_signo;			/* signal number being caught */
  unsigned long sm_mask;	/* mask to restore when handler returns */
  vir_bytes sm_sighandler;	/* address of handler */
  vir_bytes sm_sigreturn;	/* address of _sigreturn in C library */
  vir_bytes sm_stkptr;		/* user stack pointer */
};

#define MESS_SIZE (sizeof(message))	/* might need usizeof from fs here */
#define NIL_MESS ((message *) 0)

struct psinfo {			/* information for the ps(1) program */
  u16_t nr_tasks, nr_procs;	/* NR_TASKS and NR_PROCS constants. */
  vir_bytes proc, mproc, fproc;	/* addresses of the main process tables. */
};

/* This is used to obtain system information through SYS_GETINFO. */
struct kinfo {
  phys_bytes code_base;		/* base of kernel code */
  phys_bytes code_size;		
  phys_bytes data_base;		/* base of kernel data */
  phys_bytes data_size;
  vir_bytes proc_addr;		/* virtual address of process table */
  phys_bytes kmem_base;		/* kernel memory layout (/dev/kmem) */
  phys_bytes kmem_size;
  phys_bytes bootdev_base;	/* boot device from boot image (/dev/boot) */
  phys_bytes bootdev_size;
  phys_bytes params_base;	/* parameters passed by boot monitor */
  phys_bytes params_size;
  long notify_held;
  long notify_blocked;
  long notify_switching;
  long notify_reenter;
  long notify_taskcall;
  long notify_ok;
  long notify_unhold;
  long notify_int;
  long notify_alarm;
  long notify_sig;
  long notify_kmess;
  long notify_stop;
  int nr_procs;			/* number of user processes */
  int nr_tasks;			/* number of kernel tasks */
  char version[8];		/* kernel version number */
};

struct machine {
  int pc_at;
  int ps_mca;
  int processor;
  int protected;
  int vdu_ega;
  int vdu_vga;
};

/* The kernel outputs messages in a local buffer, which can be requested and
 * printed by the TTY driver. The buffer structure is defined here. 
 */
struct kmessages {
  int km_next;				/* next index to write */
  int km_size;				/* current size in buffer */
  char km_buf[KMESS_BUF_SIZE];		/* buffer for messages */
};


#endif /* _TYPE_H */
