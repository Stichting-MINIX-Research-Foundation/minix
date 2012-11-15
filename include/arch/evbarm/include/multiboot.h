/*	$NetBSD: multiboot.h,v 1.8 2009/02/22 18:05:42 ahoka Exp $	*/

/*-
 * Copyright (c) 2005, 2006 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Julio M. Merino Vidal.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef __MULTIBOOT_H__
#define __MULTIBOOT_H__

#if !defined(_KERNEL) && defined(_STANDALONE)

/* --------------------------------------------------------------------- */

/*
 * Multiboot header structure.
 */
#define MULTIBOOT_HEADER_MAGIC		0x1BADB002
#define MULTIBOOT_HEADER_MODS_ALIGNED	0x00000001
#define MULTIBOOT_HEADER_WANT_MEMORY	0x00000002
#define MULTIBOOT_HEADER_HAS_VBE	0x00000004
#define MULTIBOOT_HEADER_HAS_ADDR	0x00010000

#if !defined(_LOCORE)
struct multiboot_header {
	uint32_t	mh_magic;
	uint32_t	mh_flags;
	uint32_t	mh_checksum;

	/* Valid if mh_flags sets MULTIBOOT_HEADER_HAS_ADDR. */
	paddr_t		mh_header_addr;
	paddr_t		mh_load_addr;
	paddr_t		mh_load_end_addr;
	paddr_t		mh_bss_end_addr;
	paddr_t		mh_entry_addr;

	/* Valid if mh_flags sets MULTIBOOT_HEADER_HAS_VBE. */
	uint32_t	mh_mode_type;
	uint32_t	mh_width;
	uint32_t	mh_height;
	uint32_t	mh_depth;
};
#endif /* !defined(_LOCORE) */

/*
 * Symbols defined in locore.S.
 */
#if !defined(_LOCORE) && defined(_KERNEL)
extern struct multiboot_header *Multiboot_Header;
#endif /* !defined(_LOCORE) && defined(_KERNEL) */

/* --------------------------------------------------------------------- */

/*
 * Multiboot information structure.
 */
#define MULTIBOOT_INFO_MAGIC		0x2BADB002
#define MULTIBOOT_INFO_HAS_MEMORY	0x00000001
#define MULTIBOOT_INFO_HAS_BOOT_DEVICE	0x00000002
#define MULTIBOOT_INFO_HAS_CMDLINE	0x00000004
#define MULTIBOOT_INFO_HAS_MODS		0x00000008
#define MULTIBOOT_INFO_HAS_AOUT_SYMS	0x00000010
#define MULTIBOOT_INFO_HAS_ELF_SYMS	0x00000020
#define MULTIBOOT_INFO_HAS_MMAP		0x00000040
#define MULTIBOOT_INFO_HAS_DRIVES	0x00000080
#define MULTIBOOT_INFO_HAS_CONFIG_TABLE	0x00000100
#define MULTIBOOT_INFO_HAS_LOADER_NAME	0x00000200
#define MULTIBOOT_INFO_HAS_APM_TABLE	0x00000400
#define MULTIBOOT_INFO_HAS_VBE		0x00000800

#if !defined(_LOCORE)
struct multiboot_info {
	uint32_t	mi_flags;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_MEMORY. */
	uint32_t	mi_mem_lower;
	uint32_t	mi_mem_upper;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_BOOT_DEVICE. */
	uint8_t		mi_boot_device_part3;
	uint8_t		mi_boot_device_part2;
	uint8_t		mi_boot_device_part1;
	uint8_t		mi_boot_device_drive;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_CMDLINE. */
	char *		mi_cmdline;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_MODS. */
	uint32_t	mi_mods_count;
	vaddr_t		mi_mods_addr;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_{AOUT,ELF}_SYMS. */
	uint32_t	mi_elfshdr_num;
	uint32_t	mi_elfshdr_size;
	vaddr_t		mi_elfshdr_addr;
	uint32_t	mi_elfshdr_shndx;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_MMAP. */
	uint32_t	mi_mmap_length;
	vaddr_t		mi_mmap_addr;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_DRIVES. */
	uint32_t	mi_drives_length;
	vaddr_t		mi_drives_addr;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_CONFIG_TABLE. */
	void *		unused_mi_config_table;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_LOADER_NAME. */
	char *		mi_loader_name;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_APM. */
	void *		unused_mi_apm_table;

	/* Valid if mi_flags sets MULTIBOOT_INFO_HAS_VBE. */
	void *		unused_mi_vbe_control_info;
	void *		unused_mi_vbe_mode_info;
	paddr_t		unused_mi_vbe_interface_seg;
	paddr_t		unused_mi_vbe_interface_off;
	uint32_t	unused_mi_vbe_interface_len;
};

/* --------------------------------------------------------------------- */

/*
 * Drive information.  This describes an entry in the drives table as
 * pointed to by mi_drives_addr.
 */
struct multiboot_drive {
	uint32_t	md_length;
	uint8_t		md_number;
	uint8_t		md_mode;
	uint16_t	md_cylinders;
	uint8_t		md_heads;
	uint8_t		md_sectors;

	/* The variable-sized 'ports' field comes here, so this structure
	 * can be longer. */
};

/* --------------------------------------------------------------------- */

/*
 * Memory mapping.  This describes an entry in the memory mappings table
 * as pointed to by mi_mmap_addr.
 *
 * Be aware that mm_size specifies the size of all other fields *except*
 * for mm_size.  In order to jump between two different entries, you
 * have to count mm_size + 4 bytes.
 */
struct multiboot_mmap {
	uint32_t	mm_size;
	uint64_t	mm_base_addr;
	uint64_t	mm_length;
	uint32_t	mm_type;
};

/*
 * Modules. This describes an entry in the modules table as pointed
 * to by mi_mods_addr.
 */

struct multiboot_module {
	uint32_t	mmo_start;
	uint32_t	mmo_end;
	char *		mmo_string;
	uint32_t	mmo_reserved;
};

#endif /* !defined(_LOCORE) */

/* --------------------------------------------------------------------- */

/*
 * Prototypes for public functions defined in multiboot.c.
 */
#if !defined(_LOCORE) && defined(_KERNEL)
void		multiboot_pre_reloc(struct multiboot_info *);
void		multiboot_post_reloc(void);
void		multiboot_print_info(void);
bool		multiboot_ksyms_addsyms_elf(void);
#endif /* !defined(_LOCORE) */

/* --------------------------------------------------------------------- */
#else /* !defined(_KERNEL) && defined(_STANDALONE) */
/* LSC FIXME: OLD MINIX DEFINITION: should be removed and replace with 
   definition above... */
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
#endif /* !defined(_KERNEL) && defined(_STANDALONE) */
#endif /* __MULTIBOOT_H__ */
