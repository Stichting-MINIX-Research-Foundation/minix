#ifndef _TYPE_H
#define _TYPE_H

#ifndef _MINIX_SYS_CONFIG_H
#include <minix/sys_config.h>
#endif

#ifndef _TYPES_H
#include <minix/types.h>
#endif

#include <stdint.h>

/* Type definitions. */
typedef unsigned int vir_clicks; 	/*  virtual addr/length in clicks */
typedef unsigned long phys_bytes;	/* physical addr/length in bytes */
typedef unsigned int phys_clicks;	/* physical addr/length in clicks */
typedef int endpoint_t;			/* process identifier */

typedef int32_t cp_grant_id_t;		/* A grant ID. */

#if (_MINIX_CHIP == _CHIP_INTEL)
typedef long unsigned int vir_bytes;	/* virtual addresses/lengths in bytes */
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
  endpoint_t proc_nr_e;
  int segment;
  vir_bytes offset;
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

typedef struct {
  cp_grant_id_t iov_grant;	/* grant ID of an I/O buffer */
  vir_bytes iov_size;		/* sizeof an I/O buffer */
} iovec_s_t;

/* PM passes the address of a structure of this type to KERNEL when
 * sys_sigsend() is invoked as part of the signal catching mechanism.
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
  phys_bytes _kmem_base;	/* kernel memory layout (/dev/kmem) */
  phys_bytes _kmem_size;
  phys_bytes bootdev_base;	/* boot device from boot image (/dev/boot) */
  phys_bytes bootdev_size;
  phys_bytes ramdev_base;	/* boot device from boot image (/dev/boot) */
  phys_bytes ramdev_size;
  phys_bytes _params_base;	/* parameters passed by boot monitor */
  phys_bytes _params_size;
  int nr_procs;			/* number of user processes */
  int nr_tasks;			/* number of kernel tasks */
  char release[6];		/* kernel release number */
  char version[6];		/* kernel version number */
};

/* Load data accounted every this no. of seconds. */
#define _LOAD_UNIT_SECS		 6 	/* Changing this breaks ABI. */

/* Load data history is kept for this long. */
#define _LOAD_HISTORY_MINUTES	15	/* Changing this breaks ABI. */
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
  int padding;	/* used to be protected */
  int vdu_ega;
  int vdu_vga;
  int apic_enabled; /* does the kernel use APIC or not? */
  phys_bytes	acpi_rsdp; /* where is the acpi RSDP */
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

/* For EXEC_NEWMEM */
struct exec_newmem
{
	vir_bytes text_bytes;
	vir_bytes data_bytes;
	vir_bytes bss_bytes;
	vir_bytes tot_bytes;
	vir_bytes args_bytes;
	int sep_id;
	dev_t st_dev;
	ino_t st_ino;
	time_t st_ctime;
	uid_t new_uid;
	gid_t new_gid;
	char progname[16];	/* Should be at least PROC_NAME_LEN */
};

/* Memory chunks. */
struct memory {
	phys_bytes	base;
	phys_bytes	size;
};

#define STATICINIT(v, n) \
	if(!(v)) {	\
		if(!((v) = alloc_contig(sizeof(*(v)) * (n), 0, NULL))) { \
			panic("allocating " #v " failed: %d", n);	\
		}	\
	}

/* The kernel outputs diagnostic messages in a circular buffer. */
struct kmessages {
  int km_next;                          /* next index to write */
  int km_size;                          /* current size in buffer */
  char km_buf[_KMESS_BUF_SIZE];          /* buffer for messages */
};

#include <minix/config.h>
#include <machine/interrupt.h>

/* randomness struct: random sources after interrupts: */
#define RANDOM_SOURCES			16
#define RANDOM_ELEMENTS			64

typedef unsigned short rand_t;

struct k_randomness {
  int random_elements, random_sources;
  struct k_randomness_bin {
        int r_next;                             /* next index to write */
        int r_size;                             /* number of random elements */
        rand_t r_buf[RANDOM_ELEMENTS]; /* buffer for random info */
  } bin[RANDOM_SOURCES];
};

/* information on PCI devices */

#define PCIINFO_ENTRY_SIZE 80

struct pciinfo_entry {
	u16_t pie_vid;
	u16_t pie_did;
	char pie_name[PCIINFO_ENTRY_SIZE];
};

struct pciinfo {
	size_t pi_count;
	struct pciinfo_entry pi_entries[NR_PCIDEV];
};

#endif /* _TYPE_H */

