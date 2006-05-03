#ifndef _TYPE_H
#define _TYPE_H

#ifndef _MINIX_SYS_CONFIG_H
#include <minix/sys_config.h>
#endif

#ifndef _TYPES_H
#include <sys/types.h>
#endif

/* Type definitions. */
typedef unsigned int vir_clicks; 	/*  virtual addr/length in clicks */
typedef unsigned long phys_bytes;	/* physical addr/length in bytes */
typedef unsigned int phys_clicks;	/* physical addr/length in clicks */

#if (_MINIX_CHIP == _CHIP_INTEL)
typedef unsigned int vir_bytes;	/* virtual addresses and lengths in bytes */
#endif

#if (_MINIX_CHIP == _CHIP_M68000)
typedef unsigned long vir_bytes;/* virtual addresses and lengths in bytes */
#endif

#if (_MINIX_CHIP == _CHIP_SPARC)
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
  int proc_nr_e;
  int segment;
  vir_bytes offset;
};

/* Memory allocation by PM. */
struct hole {
  struct hole *h_next;          /* pointer to next entry on the list */
  phys_clicks h_base;           /* where does the hole begin? */
  phys_clicks h_len;            /* how big is the hole? */
};

/* Memory info from PM. */
struct pm_mem_info {
	struct hole pmi_holes[_NR_HOLES];/* memory (un)allocations */
	u32_t pmi_hi_watermark;		 /* highest ever-used click + 1 */
};

#define phys_cp_req vir_cp_req 
struct vir_cp_req {
  struct vir_addr src;
  struct vir_addr dst;
  phys_bytes count;
};

typedef struct {
  vir_bytes iov_addr;		/* address of an I/O buffer */
  vir_bytes iov_size;		/* sizeof an I/O buffer */
} iovec_t;

/* PM passes the address of a structure of this type to KERNEL when
 * sys_sendsig() is invoked as part of the signal catching mechanism.
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
  phys_bytes ramdev_base;	/* boot device from boot image (/dev/boot) */
  phys_bytes ramdev_size;
  phys_bytes params_base;	/* parameters passed by boot monitor */
  phys_bytes params_size;
  int nr_procs;			/* number of user processes */
  int nr_tasks;			/* number of kernel tasks */
  char release[6];		/* kernel release number */
  char version[6];		/* kernel version number */
#if DEBUG_LOCK_CHECK
  int relocking;		/* interrupt locking depth (should be 0) */
#endif
};

/* Load data accounted every this no. of seconds. */
#define _LOAD_UNIT_SECS		 6 

/* Load data history is kept for this long. */
#define _LOAD_HISTORY_MINUTES	15
#define _LOAD_HISTORY_SECONDS	(60*_LOAD_HISTORY_MINUTES)

/* We need this many slots to store the load history. */
#define _LOAD_HISTORY	(_LOAD_HISTORY_SECONDS/_LOAD_UNIT_SECS)

/* Runnable processes and other load-average information. */
struct loadinfo {
  u16_t proc_load_history[_LOAD_HISTORY];	/* history of proc_s_cur */
  u16_t proc_last_slot;
  clock_t last_clock;
};

struct machine {
  int pc_at;
  int ps_mca;
  int processor;
  int prot;
  int vdu_ega;
  int vdu_vga;
};

struct io_range
{
	unsigned ior_base;	/* Lowest I/O port in range */
	unsigned ior_limit;	/* Highest I/O port in range */
};

struct mem_range
{
	phys_bytes mr_base;	/* Lowest memory address in range */
	phys_bytes mr_limit;	/* Highest memory address in range */
};

#endif /* _TYPE_H */
