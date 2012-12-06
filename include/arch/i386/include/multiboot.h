#ifndef __MULTIBOOT_H__
#define __MULTIBOOT_H__

#define MULTIBOOT_HEADER_MAGIC 0x1BADB002

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

/* Must pass memory information to OS. */
#define MULTIBOOT_PAGE_ALIGN 0x00000001

#define MULTIBOOT_MEMORY_INFO 0x00000002

#define MULTIBOOT_VIDEO_MODE 0x00000004

#define MULTIBOOT_AOUT_KLUDGE 0x00010000

/* consts used for Multiboot pre-init */

#define MULTIBOOT_VIDEO_MODE_EGA 1

#define MULTIBOOT_VIDEO_BUFFER 0xB8000

/* Usable lower memory chunk has a upper bound */
#define MULTIBOOT_LOWER_MEM_MAX 0x7f800

#define MULTIBOOT_CONSOLE_LINES 25
#define MULTIBOOT_CONSOLE_COLS 80

#define MULTIBOOT_VIDEO_BUFFER_BYTES \
	(MULTIBOOT_CONSOLE_LINES*MULTIBOOT_CONSOLE_COLS*2)

#define MULTIBOOT_STACK_SIZE 4096
#define MULTIBOOT_PARAM_BUF_SIZE 1024

#define MULTIBOOT_MAX_MODS	20

/* Flags to be set in the ’flags’ member of the multiboot info structure. */

#define MULTIBOOT_INFO_MEMORY 0x00000001
#define MULTIBOOT_INFO_MEM_MAP 0x00000040

/* Is there a boot device set? */
#define MULTIBOOT_INFO_BOOTDEV 0x00000002

/* Is the command-line defined? */
#define MULTIBOOT_INFO_CMDLINE 0x00000004

/* Are there modules to do something with? */
#define MULTIBOOT_INFO_MODS 0x00000008

#define MULTIBOOT_HIGH_MEM_BASE 0x100000

#ifndef __ASSEMBLY__

#include <sys/types.h>
/* The symbol table for a.out. */
struct multiboot_aout_symbol_table
{
	u32_t tabsize;
	u32_t strsize;
	u32_t addr;
	u32_t reserved;
};
/* The section header table for ELF. */
struct multiboot_elf_section_header_table
{
	u32_t num;
	u32_t size;
	u32_t addr;
	u32_t shndx;
};

typedef struct multiboot_elf_section_header_table multiboot_elf_section_header_table_t;
typedef struct multiboot_aout_symbol_table multiboot_aout_symbol_table_t;

struct multiboot_info
{
	/* Multiboot info version number */
	u32_t flags;
	/* Available memory from BIOS */
	u32_t mem_lower_unused;	/* minix uses memmap instead */
	u32_t mem_upper_unused;
	/* "root" partition */
	u32_t boot_device;
	/* Kernel command line */
	u32_t cmdline;
	/* Boot-Module list */
	u32_t mods_count;
	u32_t mods_addr;
	union
	{
		multiboot_aout_symbol_table_t aout_sym;
		multiboot_elf_section_header_table_t elf_sec;
	} u;
	/* Memory Mapping buffer */
	u32_t mmap_length;
	u32_t mmap_addr;
	/* Drive Info buffer */
	u32_t drives_length;
	u32_t drives_addr;
	/* ROM configuration table */
	u32_t config_table;
	/* Boot Loader Name */
	u32_t boot_loader_name;
	/* APM table */
	u32_t apm_table;
	/* Video */
	u32_t vbe_control_info;
	u32_t vbe_mode_info;
	u16_t vbe_mode;
	u16_t vbe_interface_seg;
	u16_t vbe_interface_off;
	u16_t vbe_interface_len;
};
typedef struct multiboot_info multiboot_info_t;

struct multiboot_mod_list
{
	/* Memory used goes from bytes 'mod_start' to 'mod_end-1' inclusive */
	u32_t mod_start;
	u32_t mod_end;
	/* Module command line */
	u32_t cmdline;
	/* Pad struct to 16 bytes (must be zero) */
	u32_t pad;
};
typedef struct multiboot_mod_list multiboot_module_t;

#define MULTIBOOT_MEMORY_AVAILABLE              1
#define MULTIBOOT_MEMORY_RESERVED               2
struct multiboot_mmap_entry
{
	u32_t size;
	u64_t addr;
	u64_t len;
#define MULTIBOOT_MEMORY_AVAILABLE              1
#define MULTIBOOT_MEMORY_RESERVED               2
	u32_t type;
} __attribute__((packed));
typedef struct multiboot_mmap_entry multiboot_memory_map_t;

#endif /* __ASSEMBLY__ */
#endif /* __MULTIBOOT_H__ */
