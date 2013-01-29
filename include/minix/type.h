#ifndef _TYPE_H
#define _TYPE_H
#include <machine/multiboot.h>

#ifndef _MINIX_SYS_CONFIG_H
#include <minix/sys_config.h>
#endif

#include <sys/types.h>

#include <stdint.h>

/* Type definitions. */
typedef unsigned int vir_clicks; 	/*  virtual addr/length in clicks */
typedef unsigned long phys_bytes;	/* physical addr/length in bytes */
typedef unsigned int phys_clicks;	/* physical addr/length in clicks */
typedef int endpoint_t;			/* process identifier */
typedef int32_t cp_grant_id_t;		/* A grant ID. */
typedef long unsigned int vir_bytes;	/* virtual addresses/lengths in bytes */

/* Structure for virtual copying by means of a vector with requests. */
struct vir_addr {
  endpoint_t proc_nr_e; /* NONE for phys, otherwise process endpoint */
  vir_bytes offset;
};

#define phys_cp_req vir_cp_req 
struct vir_cp_req {
  struct vir_addr src;
  struct vir_addr dst;
  phys_bytes count;
};

/* Structures for SYS_VUMAP. */
struct vumap_vir {
  union {
	cp_grant_id_t u_grant;	/* grant identifier, for non-SELF endpoint */
	vir_bytes u_addr;	/* local virtual address, for SELF endpoint */
  } vv_u;
  size_t vv_size;		/* size in bytes */
};
#define vv_grant	vv_u.u_grant
#define vv_addr		vv_u.u_addr

struct vumap_phys {
  phys_bytes vp_addr;		/* physical address */
  size_t vp_size;		/* size in bytes */
};

/* I/O vector structures used in protocols between services. */
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
  unsigned processors_count;	/* how many cpus are available */
  unsigned bsp_id;		/* id of the bootstrap cpu */
  int padding;			/* used to be protected */
  int apic_enabled; /* does the kernel use APIC or not? */
  phys_bytes	acpi_rsdp; /* where is the acpi RSDP */
};

struct io_range
{
	unsigned ior_base;	/* Lowest I/O port in range */
	unsigned ior_limit;	/* Highest I/O port in range */
};

struct minix_mem_range
{
	phys_bytes mr_base;	/* Lowest memory address in range */
	phys_bytes mr_limit;	/* Highest memory address in range */
};

#define PROC_NAME_LEN   16

/* List of boot-time processes set in kernel/table.c. */
struct boot_image {
  int proc_nr;                    	/* process number to use */
  char proc_name[PROC_NAME_LEN];        /* name in process table */
  endpoint_t endpoint;                  /* endpoint number when started */
  phys_bytes start_addr;		/* Where it's in memory */
  phys_bytes len;
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
  char kmess_buf[80*25];           /* printable copy of message buffer */
  int blpos;				/* kmess_buf position */
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

struct minix_kerninfo {
	/* Binaries will depend on the offsets etc. in this
	 * structure, so it can't be changed willy-nilly. In
	 * other words, it is ABI-restricted.
	 */
#define KERNINFO_MAGIC 0xfc3b84bf
	u32_t kerninfo_magic;
	u32_t minix_feature_flags;	/* features in minix kernel */
	u32_t ki_flags;			/* what is present in this struct */
	u32_t minix_frclock;
	u32_t flags_unused3;
	u32_t flags_unused4;
	struct kinfo		*kinfo;
	struct machine		*machine;
	struct kmessages	*kmessages;
	struct loadinfo		*loadinfo;
	struct minix_ipcvecs	*minix_ipcvecs;
} __packed;

#define MINIX_KIF_IPCVECS	(1L << 0)

#endif /* _TYPE_H */

