
#ifndef _MINIX_PARAM_H
#define _MINIX_PARAM_H 1

#include <minix/com.h>
#include <minix/const.h>

/* Number of processes contained in the system image. */
#define NR_BOOT_PROCS   (NR_TASKS + LAST_SPECIAL_PROC_NR + 1)

#ifdef _MINIX_SYSTEM
/* This is used to obtain system information through SYS_GETINFO. */
#define MAXMEMMAP 40
typedef struct kinfo {
        /* Straight multiboot-provided info */
        multiboot_info_t        mbi;
        multiboot_module_t      module_list[MULTIBOOT_MAX_MODS];
        multiboot_memory_map_t  memmap[MAXMEMMAP]; /* free mem list */
        phys_bytes              mem_high_phys;
        int                     mmap_size;

        /* Multiboot-derived */
        int                     mods_with_kernel; /* no. of mods incl kernel */
        int                     kern_mod; /* which one is kernel */

        /* Minix stuff, started at bootstrap phase */
        int                     freepde_start;  /* lowest pde unused kernel pde */
        char                    param_buf[MULTIBOOT_PARAM_BUF_SIZE];

        /* Minix stuff */
        struct kmessages *kmessages;
        int do_serial_debug;    /* system serial output */
        int serial_debug_baud;  /* serial baud rate */
        int minix_panicing;     /* are we panicing? */
        vir_bytes               user_sp; /* where does kernel want stack set */
        vir_bytes               user_end; /* upper proc limit */
        vir_bytes               vir_kern_start; /* kernel addrspace starts */
        vir_bytes               bootstrap_start, bootstrap_len;
        struct boot_image       boot_procs[NR_BOOT_PROCS];
        int nr_procs;           /* number of user processes */
        int nr_tasks;           /* number of kernel tasks */
        char release[6];        /* kernel release number */
        char version[6];        /* kernel version number */
	int vm_allocated_bytes; /* allocated by kernel to load vm */
	int kernel_allocated_bytes;		/* used by kernel */
	int kernel_allocated_bytes_dynamic;	/* used by kernel (runtime) */
} kinfo_t;
#endif /* _MINIX_SYSTEM */

#endif
