
#ifndef _MINIX_PARAM_H
#define _MINIX_PARAM_H 1

#include <minix/com.h>
#include <minix/const.h>

/* Number of processes contained in the system image. */
#define NR_BOOT_PROCS   (NR_TASKS + LAST_SPECIAL_PROC_NR + 1)

#ifdef _MINIX_SYSTEM

/* Multiboot derived Informations */

struct kinfo_framebuffer {
	uint64_t	framebuffer_addr;
	uint32_t	framebuffer_pitch;
	uint32_t	framebuffer_width;
	uint32_t	framebuffer_height;
	uint8_t		framebuffer_bpp;
#define MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED 	0
#define MULTIBOOT_FRAMEBUFFER_TYPE_RGB     	1
#define MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT     2
	uint8_t framebuffer_type;
	union {
		struct {
			uint32_t framebuffer_palette_addr;
			uint16_t framebuffer_palette_num_colors;
		};
		struct {
			uint8_t framebuffer_red_field_position;
			uint8_t framebuffer_red_mask_size;
			uint8_t framebuffer_green_field_position;
			uint8_t framebuffer_green_mask_size;
			uint8_t framebuffer_blue_field_position;
			uint8_t framebuffer_blue_mask_size;
		};
	};
};

struct kinfo_mmap {
	uint32_t	mm_size;
	uint64_t	mm_base_addr;
	uint64_t	mm_length;
	uint32_t	mm_type;
};

struct kinfo_module {
	uint32_t	mod_start;
	uint32_t	mod_end;
	char *		mod_string;
};

#define MULTIBOOT_VIDEO_MODE		0x00000004
#define MULTIBOOT_VIDEO_MODE_EGA	1
#define MULTIBOOT_VIDEO_BUFFER		0xB8000

#define MULTIBOOT_CONSOLE_LINES		25
#define MULTIBOOT_CONSOLE_COLS		80

#define MULTIBOOT_MEMORY_AVAILABLE	1
#define MULTIBOOT_MAX_MODS			20
#define MULTIBOOT_PARAM_BUF_SIZE	1024

//typedef struct multiboot1_info multiboot_info_t;
typedef struct kinfo_module kinfo_module_t;
typedef struct kinfo_mmap kinfo_memory_map_t;

/* This is used to obtain system information through SYS_GETINFO. */
#define MAXMEMMAP 40
typedef struct kinfo {
		int						boot_mode;	/* 0:BIOS, 1:UEFI */
        /* Straight multiboot-provided info */
        int						mb_version;
        //multiboot_info_t        mbi1;
        /* mutiboot2 info */
        struct kinfo_framebuffer	fb;
        uint32_t	            module_count;
        kinfo_module_t      	module_list[MULTIBOOT_MAX_MODS];
        kinfo_memory_map_t  	memmap[MAXMEMMAP]; /* free mem list */
        phys_bytes              mem_high_phys;
        int                     mmap_size;
        void					*rsdp_p;

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
