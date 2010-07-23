#ifndef __MULTIBOOT_H__
#define __MULTIBOOT_H__

#define MULTIBOOT_HEADER_MAGIC 0x1BADB002

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

/* Must pass memory information to OS. */
#define MULTIBOOT_MEMORY_INFO 0x00000002

#define MULTIBOOT_VIDEO_MODE 0x00000004

#define MULTIBOOT_AOUT_KLUDGE 0x00010000

#define MULTIBOOT_FLAGS (MULTIBOOT_MEMORY_INFO | \
						MULTIBOOT_VIDEO_MODE | \
						MULTIBOOT_AOUT_KLUDGE)
						
/* consts used for Multiboot pre-init */

#define MULTIBOOT_ENTRY_OFFSET 0x200

#define MULTIBOOT_LOAD_ADDRESS 0x200000-MULTIBOOT_ENTRY_OFFSET

#define MULTIBOOT_VIDEO_MODE_EGA 1

#define MULTIBOOT_VIDEO_BUFFER 0xB8000

/* Usable lower memory chunk has a upper bound */
#define MULTIBOOT_LOWER_MEM_MAX 0x7f800

#define MULTIBOOT_CONSOLE_LINES 25
#define MULTIBOOT_CONSOLE_COLS 80


#define MULTIBOOT_STACK_SIZE 4096
#define MULTIBOOT_PARAM_BUF_SIZE 1024

#define MULTIBOOT_KERNEL_a_text 0x48
#define MULTIBOOT_KERNEL_a_data (0x48+4)
#define MULTIBOOT_KERNEL_a_total (0x48+16)

/* Flags to be set in the ’flags’ member of the multiboot info structure. */

#define MULTIBOOT_INFO_MEMORY 0x00000001

/* Is there a boot device set? */
#define MULTIBOOT_INFO_BOOTDEV 0x00000002

/* Is the command-line defined? */
#define MULTIBOOT_INFO_CMDLINE 0x00000004

/* Are there modules to do something with? */
#define MULTIBOOT_INFO_MODS 0x00000008

/* get physical address by data pointer*/
#define PTR2PHY(ptr) (kernel_data_addr+(u32_t)(ptr))

/* get data pointer by physical address*/
#define PHY2PTR(phy) ((char *)((u32_t)(phy)-kernel_data_addr))

/* Get physical address by function pointer*/
#define FUNC2PHY(fun) (MULTIBOOT_LOAD_ADDRESS + MULTIBOOT_ENTRY_OFFSET + (u32_t)(fun))

#ifndef __ASSEMBLY__

#include <minix/types.h>
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
	u32_t mem_lower;
	u32_t mem_upper;
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

/* Buffer for multiboot parameters */
extern char multiboot_param_buf[];

/* Physical address of kernel data segment */
extern phys_bytes kernel_data_addr;

#endif /* __ASSEMBLY__ */
#endif /* __MULTIBOOT_H__ */
