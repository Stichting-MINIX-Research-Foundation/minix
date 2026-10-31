/* $NetBSD: exec_multiboot2.c,v 1.2.2.2 2019/09/17 19:32:00 martin Exp $ */

/*
 * Copyright (c) 2019 The NetBSD Foundation, Inc.
 * All rights reserved.
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

#include <sys/param.h>
#include <sys/reboot.h> 
#include <sys/types.h> 
 
#include <i386/multiboot2.h>

#include <dev/acpi/acpica.h>
#include <x86/acpi_machdep.h>
#include <x86/smbiosvar.h>

#include <lib/libsa/stand.h> 
#include <lib/libkern/libkern.h> 


#include "loadfile.h"
#include "libi386.h"
#include "biosdisk.h"
#include "bootinfo.h"
#include "bootmod.h"
#include "vbe.h"
#if defined(EFIBOOT)
#include "efiboot.h"
#endif

#define CGA_BUF 0xb8000 /* From isa_machdep.h */

extern const char bootprog_name[], bootprog_rev[], bootprog_kernrev[];
extern const uint8_t rasops_cmap[];
extern struct btinfo_framebuffer btinfo_framebuffer;
extern struct btinfo_modulelist *btinfo_modulelist;
extern struct btinfo_memmap *btinfo_memmap;
#if defined(EFIBOOT)
static bool efi_exited = false;
#endif

struct multiboot_package_priv {
	struct multiboot_tag 			       *mpp_mbi;
	size_t						mpp_mbi_len;
	struct multiboot_header_tag_information_request*mpp_info_req;
	struct multiboot_header_tag_address		*mpp_address;
	struct multiboot_header_tag_entry_address	*mpp_entry;
	struct multiboot_header_tag_console_flags	*mpp_console;
	struct multiboot_header_tag_framebuffer		*mpp_framebuffer;
	struct multiboot_header_tag			*mpp_module_align;
	struct multiboot_header_tag			*mpp_efi_bs;
	struct multiboot_header_tag_entry_address	*mpp_entry_elf32;
	struct multiboot_header_tag_entry_address	*mpp_entry_elf64;
	struct multiboot_header_tag_relocatable		*mpp_relocatable;
};

#ifndef NO_MULTIBOOT2

#if defined(EFIBOOT)
static void
efi_get_mode_info(struct btinfo_framebuffer *fb)
{
	// TODO
}
#endif

#ifdef MULTIBOOT2_DEBUG
static void
mbi_hexdump(char *addr, size_t len)
{
	int i,j;

	for (i = 0; i < len; i += 16) {
		printf("  %p ", addr + i);
		for (j = 0; j < 16 && i + j < len; j++) {
			char *cp = addr + i + j;
			printf("%s%s%x", 
			       (i+j) % 4 ? "" : " ",
			       (unsigned char)*cp < 0x10 ? "0" : "",
			       (unsigned char)*cp);
		}
		printf("\n");
	}

	return;
}

static const char *
mbi_tag_name(uint32_t type)
{
	const char *tag_name;

	switch (type) {
	case MULTIBOOT_TAG_TYPE_END:
		tag_name = "END"; break;
	case MULTIBOOT_TAG_TYPE_CMDLINE:
		tag_name = "CMDLINE"; break;
	case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME:
		tag_name = "BOOT_LOADER_NAME"; break;
	case MULTIBOOT_TAG_TYPE_MODULE:
		tag_name = "MODULE"; break;
	case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:
		tag_name = "BASIC_MEMINFO"; break;
	case MULTIBOOT_TAG_TYPE_BOOTDEV:
		tag_name = "BOOTDEV"; break;
	case MULTIBOOT_TAG_TYPE_MMAP:
		tag_name = "MMAP"; break;
	case MULTIBOOT_TAG_TYPE_VBE:
		tag_name = "VBE"; break;
	case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
		tag_name = "FRAMEBUFFER"; break;
	case MULTIBOOT_TAG_TYPE_ELF_SECTIONS:
		tag_name = "ELF_SECTIONS"; break;
	case MULTIBOOT_TAG_TYPE_APM:
		tag_name = "APM"; break;
	case MULTIBOOT_TAG_TYPE_EFI32:
		tag_name = "EFI32"; break;
	case MULTIBOOT_TAG_TYPE_EFI64:
		tag_name = "EFI64"; break;
	case MULTIBOOT_TAG_TYPE_SMBIOS:
		tag_name = "SMBIOS"; break;
	case MULTIBOOT_TAG_TYPE_ACPI_OLD:
		tag_name = "ACPI_OLD"; break;
	case MULTIBOOT_TAG_TYPE_ACPI_NEW:
		tag_name = "ACPI_NEW"; break;
	case MULTIBOOT_TAG_TYPE_NETWORK:
		tag_name = "NETWORK"; break;
	case MULTIBOOT_TAG_TYPE_EFI_MMAP:
		tag_name = "EFI_MMAP"; break;
	case MULTIBOOT_TAG_TYPE_EFI_BS:
		tag_name = "EFI_BS"; break;
	case MULTIBOOT_TAG_TYPE_EFI32_IH:
		tag_name = "EFI32_IH"; break;
	case MULTIBOOT_TAG_TYPE_EFI64_IH:
		tag_name = "EFI64_IH"; break;
	case MULTIBOOT_TAG_TYPE_LOAD_BASE_ADDR:
		tag_name = "LOAD_BASE_ADDR"; break;
	default:
		tag_name = "unknown"; break;
	}

	return tag_name;
}

static void
multiboot2_info_dump(uint32_t magic, char *mbi)
{
	struct multiboot_tag *mbt;
	char *cp;
	uint32_t total_size;
	uint32_t actual_size;
	uint32_t reserved;
	int i = 0;

	printf("=== multiboot2 info dump start  ===\n");

	if (magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
		printf("Unexpected multiboot2 magic number: 0x%x\n", magic);
		goto out;
	}

	if (mbi != (char *)rounddown((vaddr_t)mbi, MULTIBOOT_TAG_ALIGN)) {
		printf("mbi at %p is not properly aligned\n", mbi);
		goto out;
	}

	printf("mbi address = %p \n", mbi);
	total_size = *(uint32_t *)mbi;
	reserved = *((uint32_t *)mbi + 1);
	mbt = (struct multiboot_tag *)((uint32_t *)mbi + 2);
	actual_size = (char *)mbt - mbi;
	printf("mbi.total_size = %d\n", total_size);
	printf("mbi.reserved = %d\n", reserved);

	for (cp = mbi + sizeof(total_size) + sizeof(reserved);
	     cp - mbi < total_size;
	     cp = cp + roundup(mbt->size, MULTIBOOT_TAG_ALIGN)) {
		mbt = (struct multiboot_tag *)cp;
		actual_size += roundup(mbt->size, MULTIBOOT_TAG_ALIGN);

		printf("mbi[%d].type = %d(%s), .size = %d ",
		    i++, mbt->type, mbi_tag_name(mbt->type), mbt->size);

		switch (mbt->type) {
		case MULTIBOOT_TAG_TYPE_CMDLINE:
			printf(".string = \"%s\"\n",
			    ((struct multiboot_tag_string *)mbt)->string);
			break;
		case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME:
			printf(".string = \"%s\"\n",
			    ((struct multiboot_tag_string *)mbt)->string);
			break;
		case MULTIBOOT_TAG_TYPE_MODULE:
			printf(".mod_start = 0x%x, mod_end = 0x%x, "
			    "string = \"%s\"\n",
			    ((struct multiboot_tag_module *)mbt)->mod_start,
			    ((struct multiboot_tag_module *)mbt)->mod_end,
			    ((struct multiboot_tag_module *)mbt)->cmdline);
			break;
		case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO: {
			struct multiboot_tag_basic_meminfo *meminfo;

			meminfo = (struct multiboot_tag_basic_meminfo *)mbt;
			printf(".mem_lower = %uKB, .mem_upper = %uKB\n",
			    meminfo->mem_lower, meminfo->mem_upper);
			break;
		}
		case MULTIBOOT_TAG_TYPE_BOOTDEV:
			printf (".biosdev = 0x%x, .slice = %d, .part = %d\n",
			    ((struct multiboot_tag_bootdev *)mbt)->biosdev,
			    ((struct multiboot_tag_bootdev *)mbt)->slice,
			    ((struct multiboot_tag_bootdev *)mbt)->part);
			break;
		case MULTIBOOT_TAG_TYPE_MMAP: {
			struct multiboot_tag_mmap *memmap;
			multiboot_memory_map_t *mmap;
			uint32_t entry_size;
			uint32_t entry_version;
			int j = 0;

			memmap = (struct multiboot_tag_mmap *)mbt;
			entry_size = memmap->entry_size;
			entry_version = memmap->entry_version;
			printf (".entry_size = %d, .entry_version = %d\n",
			    entry_size, entry_version);
			
			for (mmap = ((struct multiboot_tag_mmap *)mbt)->entries;
		 	    (char *)mmap - (char *)mbt < mbt->size;
		 	    mmap = (void *)((char *)mmap + entry_size))
				printf("  entry[%d].base_addr = 0x%"PRIx64",\t" 
				    ".len = 0x%"PRIx64",\t.type = 0x%x\n",
				    j++, (uint64_t)mmap->addr,
				    (uint64_t)mmap->len,
				    mmap->type);
			break;
		}
		case MULTIBOOT_TAG_TYPE_FRAMEBUFFER: {
			struct multiboot_tag_framebuffer *fb = (void *)mbt;

			printf ("%dx%dx%d at 0x%"PRIx64"\n",
			    fb->common.framebuffer_width,
			    fb->common.framebuffer_height,
			    fb->common.framebuffer_bpp,
			    (uint64_t)fb->common.framebuffer_addr);
			mbi_hexdump((char *)mbt, mbt->size);
			break;
		}
		case MULTIBOOT_TAG_TYPE_ELF_SECTIONS:
			printf(".num = %d, .entsize = %d, .shndx = %d\n",
			    ((struct multiboot_tag_elf_sections *)mbt)->num,
			    ((struct multiboot_tag_elf_sections *)mbt)->entsize,
			    ((struct multiboot_tag_elf_sections *)mbt)->shndx);
			mbi_hexdump((char *)mbt, mbt->size);
			break;
		case MULTIBOOT_TAG_TYPE_APM:
			printf(".version = %d, .cseg = 0x%x, .offset = 0x%x, "
			    ".cseg_16 = 0x%x, .dseg = 0x%x, .flags = 0x%x, "
			    ".cseg_len = %d, .cseg_16_len = %d, "
			    ".dseg_len = %d\n",
			    ((struct multiboot_tag_apm *)mbt)->version,
			    ((struct multiboot_tag_apm *)mbt)->cseg,
			    ((struct multiboot_tag_apm *)mbt)->offset,
			    ((struct multiboot_tag_apm *)mbt)->cseg_16,
			    ((struct multiboot_tag_apm *)mbt)->dseg,
			    ((struct multiboot_tag_apm *)mbt)->flags,
			    ((struct multiboot_tag_apm *)mbt)->cseg_len,
			    ((struct multiboot_tag_apm *)mbt)->cseg_16_len,
			    ((struct multiboot_tag_apm *)mbt)->dseg_len);
			break;
		case MULTIBOOT_TAG_TYPE_EFI32:
			printf(".pointer = 0x%x\n",
			    ((struct multiboot_tag_efi32 *)mbt)->pointer);
			break;
		case MULTIBOOT_TAG_TYPE_EFI64:
			printf(".pointer = 0x%"PRIx64"\n", (uint64_t)
			    ((struct multiboot_tag_efi64 *)mbt)->pointer);
			break;
		case MULTIBOOT_TAG_TYPE_SMBIOS:
			printf(".major = %d, .minor = %d\n",
			    ((struct multiboot_tag_smbios *)mbt)->major,
		 	    ((struct multiboot_tag_smbios *)mbt)->minor);
			mbi_hexdump((char *)mbt, mbt->size);
			break;
		case MULTIBOOT_TAG_TYPE_ACPI_OLD:
			printf("\n");
			mbi_hexdump((char *)mbt, mbt->size);
			break;
		case MULTIBOOT_TAG_TYPE_ACPI_NEW:
			printf("\n");
			mbi_hexdump((char *)mbt, mbt->size);
			break;
		case MULTIBOOT_TAG_TYPE_NETWORK:
			printf("\n");
			mbi_hexdump((char *)mbt, mbt->size);
			break;
		case MULTIBOOT_TAG_TYPE_EFI_MMAP: {
			struct multiboot_tag_efi_mmap *efi_mmap;
			uint32_t descr_size;
			uint32_t descr_vers;
			int i = 0, NoEntries;
			EFI_MEMORY_DESCRIPTOR *desc, *md, *next;

			efi_mmap = (struct multiboot_tag_efi_mmap *)mbt;
			descr_size = efi_mmap->descr_size;
			descr_vers = efi_mmap->descr_vers;
			desc = (EFI_MEMORY_DESCRIPTOR *)efi_mmap->efi_mmap;
			NoEntries = (mbt->size - 8) / descr_size;
			printf (".descr_size = %d, .descr_vers = %d\n",
			    descr_size, descr_vers);
			for (i = 0, md = desc; i < NoEntries; i++, md = next) {
				printf("  desc[%d].PhysicalStart = 0x%"PRIx64",\t" 
					".VirtualStart = 0x%"PRIx64",\t"
				    ".Pages = 0x%"PRIx64",\t"
				    ".type = 0x%x\t.Attr = 0x%"PRIx64"\n",
				    i, md->PhysicalStart,
				    md->VirtualStart,
				    md->NumberOfPages,
				    md->Type,
				    md->Attribute);
				next = NextMemoryDescriptor(md, descr_size);
			}
			break;
		}
		case MULTIBOOT_TAG_TYPE_EFI_BS:
			printf("\n");
			break;
		case MULTIBOOT_TAG_TYPE_EFI32_IH:
			printf(".pointer = 0x%"PRIx32"\n", 
			    ((struct multiboot_tag_efi32_ih *)mbt)->pointer);
			break;
		case MULTIBOOT_TAG_TYPE_EFI64_IH:
			printf(".pointer = 0x%"PRIx64"\n", (uint64_t)
			    ((struct multiboot_tag_efi64_ih *)mbt)->pointer);
			break;
		case MULTIBOOT_TAG_TYPE_LOAD_BASE_ADDR: {
			struct multiboot_tag_load_base_addr *ld = (void *)mbt;
			printf(".load_base_addr = 0x%x\n", ld->load_base_addr);
			break;
		}
		case MULTIBOOT_TAG_TYPE_END:
			break;
		default:
			printf("\n");
			mbi_hexdump((char *)mbt, mbt->size);
			break;
		}
	}

	if (total_size != actual_size)
		printf("Size mismatch: announded %d, actual %d\n",
		    total_size, actual_size);

out:
	printf("=== multiboot2 info dump end  ===\n");
	return;
}

#define MPP_OPT(flags) \
    (flags & MULTIBOOT_HEADER_TAG_OPTIONAL) ? " (opt)" : " (req)"

static
void multiboot2_header_dump(struct multiboot_package *mbp)
{
	struct multiboot_package_priv *mpp = mbp->mbp_priv;

	printf("=== multiboot2 header dump start ===\n");
	if (mpp->mpp_info_req) {
		struct multiboot_header_tag_information_request *info_req;	
		size_t nreq;
		int i;

		info_req = mpp->mpp_info_req;

		nreq = (info_req->size - sizeof(*info_req))
		     / sizeof(info_req->requests[0]);

		printf("Information tag request%s: ",
		       MPP_OPT(info_req->flags));	
		for (i = 0; i < nreq; i++)
			printf("%d(%s) ",
			    info_req->requests[i],
			    mbi_tag_name(info_req->requests[i]));
		printf("\n");
	}

	if (mpp->mpp_address)
		printf("Addresses%s: header = %"PRIx32", load = %"PRIx32", "
		       "end = %"PRIx32", bss = %"PRIx32"\n", 
		       MPP_OPT(mpp->mpp_address->flags),
		       mpp->mpp_address->header_addr,
		       mpp->mpp_address->load_addr,
		       mpp->mpp_address->load_end_addr,
		       mpp->mpp_address->bss_end_addr);

	if (mpp->mpp_entry)
		printf("Entry point%s: %"PRIx32"\n", 
		       MPP_OPT(mpp->mpp_entry->flags),
		       mpp->mpp_entry->entry_addr);

	if (mpp->mpp_console) {
		int flags = mpp->mpp_console->console_flags;
		char *req_flag = "";
		char *ega_flag = "";

		if (flags & MULTIBOOT_CONSOLE_FLAGS_EGA_TEXT_SUPPORTED)
			ega_flag = " EGA";
		if (flags & MULTIBOOT_CONSOLE_FLAGS_CONSOLE_REQUIRED)
			req_flag = " required";

		printf("Console flags%s: %s %s\n",
		       MPP_OPT(mpp->mpp_console->flags), 
		       ega_flag, req_flag);
	}

	if (mpp->mpp_framebuffer)
		printf("Framebuffer%s: width = %d, height = %d, depth = %d\n",
		       MPP_OPT(mpp->mpp_framebuffer->flags),
		       mpp->mpp_framebuffer->width,
		       mpp->mpp_framebuffer->height,
		       mpp->mpp_framebuffer->depth);

	if (mpp->mpp_module_align)
		printf("Module alignmenet%s\n",
		       MPP_OPT(mpp->mpp_module_align->flags));
	
	if (mpp->mpp_efi_bs)
		printf("Do not call EFI Boot service exit%s\n",
		       MPP_OPT(mpp->mpp_efi_bs->flags));
	
	if (mpp->mpp_entry_elf32)
		printf("EFI32 entry point%s: %"PRIx32"\n", 
		       MPP_OPT(mpp->mpp_entry_elf32->flags),
		       mpp->mpp_entry_elf32->entry_addr);

	if (mpp->mpp_entry_elf64)
		printf("EFI64 entry point%s: %"PRIx32"\n", 
		       MPP_OPT(mpp->mpp_entry_elf64->flags),
		       mpp->mpp_entry_elf64->entry_addr);

	if (mpp->mpp_relocatable) {
		char *pref;

		switch (mpp->mpp_relocatable->preference) {
		case MULTIBOOT_LOAD_PREFERENCE_NONE: pref = "none"; break;
		case MULTIBOOT_LOAD_PREFERENCE_LOW:  pref = "low"; break;
		case MULTIBOOT_LOAD_PREFERENCE_HIGH: pref = "high"; break;
		default:
			pref = "(unknown)"; break;
		}
		printf("Relocatable%s: min_addr = %"PRIx32", "
		       "max_addr = %"PRIx32", align = %"PRIx32", pref %s\n",
		       MPP_OPT(mpp->mpp_relocatable->flags),
		       mpp->mpp_relocatable->min_addr,
		       mpp->mpp_relocatable->max_addr,
		       mpp->mpp_relocatable->align, pref);
	}

	printf("=== multiboot2 header dump end  ===\n");
	return;
}
#endif /* MULTIBOOT2_DEBUG */

static size_t
mbi_cmdline(struct multiboot_package *mbp, void *buf)
{
	struct multiboot_tag_string *mbt = buf;
	size_t cmdlen;
	size_t len;
	const char fmt[] = "%s %s";

	/* +1 for trailing \0 */
	cmdlen = snprintf(NULL, SIZE_T_MAX, fmt, mbp->mbp_file, mbp->mbp_args)
	       + 1;
	len = sizeof(*mbt) + cmdlen;

	if (mbt) {
		mbt->type = MULTIBOOT_TAG_TYPE_CMDLINE;
		mbt->size = len;
		(void)snprintf(mbt->string, cmdlen, fmt, 
			       mbp->mbp_file, mbp->mbp_args);
	}

	return roundup(len, MULTIBOOT_TAG_ALIGN);
}

static size_t
mbi_boot_loader_name(struct multiboot_package *mbp, void *buf)
{
	struct multiboot_tag_string *mbt = buf;
	size_t len;
	size_t strlen;
	const char fmt[] = "%s, Revision %s (from NetBSD %s)";


	/* +1 for trailing \0 */
	strlen = snprintf(NULL, SIZE_T_MAX, fmt,
			  bootprog_name, bootprog_rev, bootprog_kernrev)
	       + 1;
	len = sizeof(*mbt) + strlen;

	if (mbt) {
		mbt->type = MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME;
		mbt->size = len;
	    	(void)snprintf(mbt->string, strlen, fmt, bootprog_name, 
			       bootprog_rev, bootprog_kernrev);
	}

	return roundup(len, MULTIBOOT_TAG_ALIGN);
}

static size_t
mbi_modules(struct multiboot_package *mbp, void *buf)
{
	struct multiboot_tag_module *mbt = buf;
	struct bi_modulelist_entry *bim;
	size_t len;
	int i;

	if (btinfo_modulelist == NULL)
		return 0;

	len = 0;
		
	bim = (struct bi_modulelist_entry *)(btinfo_modulelist + 1);
	for (i = 0; i < btinfo_modulelist->num; i++) {
		size_t pathlen = strlen(bim->path) + 1;
		size_t mbt_len = sizeof(*mbt) + pathlen;
		size_t mbt_len_align = roundup(mbt_len, MULTIBOOT_TAG_ALIGN);
		len += mbt_len_align;

		if (mbt) {
			mbt->type = MULTIBOOT_TAG_TYPE_MODULE;
			mbt->size = mbt_len;
			mbt->mod_start = bim->base;
			mbt->mod_end = bim->base + bim->len;
			strncpy(mbt->cmdline, bim->path, pathlen);
			bim++;
			mbt = (struct multiboot_tag_module *)
			    ((char *)mbt + mbt_len_align);
		}
	}

	return len;
}

static size_t
mbi_basic_meminfo(struct multiboot_package *mbp, void *buf)
{
	struct multiboot_tag_basic_meminfo *mbt = buf;
	size_t len;

	len = sizeof(*mbt);
		
	if (mbt) {
		mbt->type = MULTIBOOT_TAG_TYPE_BASIC_MEMINFO;
		mbt->size = len;
		mbt->mem_lower = mbp->mbp_basemem;
		mbt->mem_upper = mbp->mbp_extmem;
	}

	return roundup(len, MULTIBOOT_TAG_ALIGN);
}

static size_t
mbi_bootdev(struct multiboot_package *mbp, void *buf)
{
	struct multiboot_tag_bootdev *mbt = buf;
	size_t len;

	len = sizeof(*mbt);
		
	/*
	 * According to the specification:
	 * - sub_partition is used for BSD disklabel.
	 * - Extendded MBR partitions are counted from 4 and increasing,
	 *   with no subpartition.
	 */
	if (mbt) {
		mbt->type = MULTIBOOT_TAG_TYPE_BOOTDEV;
		mbt->size = len;
		mbt->biosdev = bi_disk.biosdev;
		mbt->slice = bi_disk.partition;
		mbt->part = 0xFFFFFFFF;	/* aka sub_partition, for disklabel */
	}

	return roundup(len, MULTIBOOT_TAG_ALIGN);
	return 0;
}

static size_t
mbi_mmap(struct multiboot_package *mbp, void *buf)
{
	size_t len = 0;
	struct multiboot_tag_mmap *mbt = buf;
	struct bi_memmap_entry *memmap;
	size_t num;

#if !defined(EFIBOOT)
	bi_getmemmap();

	if (btinfo_memmap == NULL)
		goto out;

	memmap = btinfo_memmap->entry;
	num = btinfo_memmap->num;
#else
	if (efi_memory_get_memmap(&memmap, &num) != 0)
		goto out;
#endif

	len = sizeof(*mbt) + num * sizeof(mbt->entries[0]);
	
	if (mbt) {
		int i;
		struct multiboot_mmap_entry *mbte;

		mbt->type = MULTIBOOT_TAG_TYPE_MMAP;
		mbt->size = len;
		mbt->entry_size = sizeof(mbt->entries[0]);
		mbt->entry_version = 0;

		mbte = (struct multiboot_mmap_entry *)(mbt + 1);
		for (i = 0; i < num; i++) {
			mbte[i].addr = memmap[i].addr;
			mbte[i].len = memmap[i].size;
			switch(memmap[i].type) {
			case BIM_Memory:
				mbte[i].type = MULTIBOOT_MEMORY_AVAILABLE;
				break;
			case BIM_Reserved:
				mbte[i].type = MULTIBOOT_MEMORY_RESERVED;
				break;
			case BIM_ACPI:
				mbte[i].type = 
				    MULTIBOOT_MEMORY_ACPI_RECLAIMABLE;
				break;
			case BIM_NVS:
				mbte[i].type = MULTIBOOT_MEMORY_NVS;
				break;
			case BIM_Unusable:
				mbte[i].type = MULTIBOOT_MEMORY_BADRAM;
				break;
			default:
				mbte[i].type = MULTIBOOT_MEMORY_RESERVED;
				break;
			}
			mbte[i].zero = 0;
		}
	}
#if defined(EFIBOOT)
	dealloc(memmap, num * sizeof(memmap));
#endif
out:
	return roundup(len, MULTIBOOT_TAG_ALIGN);
}

static size_t
mbi_vbe(struct multiboot_package *mbp, void *buf)
{
	size_t len = 0;

#if !defined(EFIBOOT)
	struct multiboot_tag_vbe *mbt = buf;

	len = sizeof(*mbt);
	
	if (mbt) {
		mbt->type = MULTIBOOT_TAG_TYPE_VBE;
		mbt->size = len;
		mbt->vbe_mode = btinfo_framebuffer.vbemode;
		mbt->vbe_interface_seg = 0;
		mbt->vbe_interface_off = 0;
		mbt->vbe_interface_len = 0;
		biosvbe_info((struct vbeinfoblock *)&mbt->vbe_control_info);
		biosvbe_get_mode_info(mbt->vbe_mode,
		    (struct modeinfoblock *)&mbt->vbe_mode_info);
	}
#endif
	return roundup(len, MULTIBOOT_TAG_ALIGN);
}

static size_t
mbi_framebuffer(struct multiboot_package *mbp, void *buf)
{
	size_t len = 0;
	struct multiboot_tag_framebuffer *mbt = buf;
	struct btinfo_framebuffer *fb = &btinfo_framebuffer;

#if !defined(EFIBOOT)
	struct modeinfoblock mi;

	if (fb->physaddr != 0) {
		int ret;

		ret = biosvbe_get_mode_info(fb->vbemode, &mi);
		if (ret != 0x004f)
			return 0;
	}
#else
	efi_get_mode_info(fb);
#endif

	len = sizeof(*mbt);
	
	if (mbt) {
		mbt->common.type = MULTIBOOT_TAG_TYPE_FRAMEBUFFER;
		mbt->common.size = len;
		mbt->common.reserved = 0;

		/*
		 * No framebuffer, default to 80x25 console
		 */ 
		if (fb->physaddr == 0) {
			int width = 80;
			int height = 25;
			int charlen = 2;
			mbt->common.framebuffer_addr = CGA_BUF;
			mbt->common.framebuffer_width = width;
			mbt->common.framebuffer_height = height;
			mbt->common.framebuffer_bpp = charlen * 8;
			mbt->common.framebuffer_pitch = width * charlen;
			mbt->common.framebuffer_type = 
			    MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT;
		} else {
			mbt->common.framebuffer_addr = fb->physaddr;
			mbt->common.framebuffer_pitch = fb->stride;
			mbt->common.framebuffer_width = fb->width;
			mbt->common.framebuffer_height = fb->height;
			mbt->common.framebuffer_bpp = fb->depth;
			mbt->common.framebuffer_type =
			    MULTIBOOT_FRAMEBUFFER_TYPE_RGB;
#if !defined(EFIBOOT)
			if (mi.MemoryModel == 0x04)
				mbt->common.framebuffer_type =
				    MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED;
#endif
		}
			
		switch (mbt->common.framebuffer_type) {
#if !defined(EFIBOOT)
		case MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED: 
			mbt->framebuffer_palette_num_colors = 256;

			for (int i = 0; i < 256; i++) {
				mbt->framebuffer_palette[i].red =
				    rasops_cmap[3 * i];
				mbt->framebuffer_palette[i].green =
				    rasops_cmap[(3 * i) + 1];
				mbt->framebuffer_palette[i].blue =
				    rasops_cmap[(3 * i) + 2];
			}	
			break;
#endif
		case MULTIBOOT_FRAMEBUFFER_TYPE_RGB:
			mbt->framebuffer_red_field_position = fb->rpos;
			mbt->framebuffer_red_mask_size = fb->rnum;
			mbt->framebuffer_green_field_position = fb->gpos;
			mbt->framebuffer_green_mask_size = fb->gnum;
			mbt->framebuffer_blue_field_position = fb->bpos;
			mbt->framebuffer_blue_mask_size = fb->bnum;
			break;
		default:
			break;
		}
	}

	return roundup(len, MULTIBOOT_TAG_ALIGN);
}

static size_t
mbi_acpi_old(struct multiboot_package *mbp, void *buf)
{
	size_t len = 0;
	struct multiboot_tag_old_acpi *mbt = buf;
	ACPI_PHYSICAL_ADDRESS rsdp_phys = -1;
	ACPI_RSDP_COMMON rsdp;
#if defined(EFIBOOT)
	const EFI_GUID acpi_table_guid = ACPI_TABLE_GUID;
	int i;

	if (ST == NULL)
		goto out;

	for (i = 0; i < ST->NumberOfTableEntries; i++)  {
		if (memcmp(&ST->ConfigurationTable[i].VendorGuid,
		   &acpi_table_guid, sizeof(acpi_table_guid)) == 0) {
			rsdp_phys = (ACPI_PHYSICAL_ADDRESS)
			    ST->ConfigurationTable[i].VendorTable; 
			break;
		}
	}
#else
#ifdef notyet
	rsdp_phys = acpi_md_OsGetRootPointer();
	pvbcopy((void *)(vaddr_t)rsdp_phys, &rsdp, sizeof(rsdp));

	/* Check ACPI 1.0 */
	if (rsdp.Revision != 0)
		rsdp_phys = -1;
#endif
#endif

	if (rsdp_phys == -1)
		goto out;

	len = sizeof(*mbt) + sizeof(rsdp);
	if (mbt) {
		mbt->type = MULTIBOOT_TAG_TYPE_ACPI_OLD;
		mbt->size = len;
		pvbcopy((void *)(vaddr_t)rsdp_phys, mbt->rsdp, sizeof(rsdp));
	}
out:
	return roundup(len, MULTIBOOT_TAG_ALIGN);
}

static size_t
mbi_acpi_new(struct multiboot_package *mbp, void *buf)
{
	size_t len = 0;
	struct multiboot_tag_new_acpi *mbt = buf;
	ACPI_PHYSICAL_ADDRESS rsdp_phys = -1;
	ACPI_TABLE_RSDP rsdp;
#if defined(EFIBOOT)
	const EFI_GUID acpi_20_table_guid = ACPI_20_TABLE_GUID;
	int i;

	if (ST == NULL)
		goto out;

	for (i = 0; i < ST->NumberOfTableEntries; i++)  {
		if (memcmp(&ST->ConfigurationTable[i].VendorGuid,
		   &acpi_20_table_guid, sizeof(acpi_20_table_guid)) == 0) {
			rsdp_phys = (ACPI_PHYSICAL_ADDRESS)
			    ST->ConfigurationTable[i].VendorTable; 
			break;
		}
	}
#else
#ifdef notyet
	rsdp_phys = acpi_md_OsGetRootPointer();
	pvbcopy((void *)(vaddr_t)rsdp_phys, &rsdp, sizeof(rsdp));

	/* Check ACPI 2.0 */
	if (rsdp.Revision != 2)
		rsdp_phys = -1;
#endif
#endif
	if (rsdp_phys == -1)
		goto out;

	len = sizeof(*mbt) + sizeof(rsdp);
	if (mbt) {
		mbt->type = MULTIBOOT_TAG_TYPE_ACPI_NEW;
		mbt->size = len;
		pvbcopy((void *)(vaddr_t)rsdp_phys, mbt->rsdp, sizeof(rsdp));
	}
out:
	return roundup(len, MULTIBOOT_TAG_ALIGN);
}

static size_t
mbi_apm(struct multiboot_package *mbp, void *buf)
{
	size_t len = 0;
#ifdef notyet
	struct multiboot_tag_apm *mbt = buf;

	len = sizeof(*mbt):

	if (mbt) {
		mbt->type = MULTIBOOT_TAG_TYPE_A;
		mbt->size = len;
		mbt->version = 0;
		mbt->cseg = 0;
		mbt->offset = 0;
		mbt->cseg_16 = 0;
		mbt->dseg = 0;; 
		mbt->flags = 0;
		mbt->cseg_len = 0;
		mbt->cseg_16_len = 0;
		mbt->dseg_len = 0;
	}
out:
#endif
	return roundup(len, MULTIBOOT_TAG_ALIGN);
}

static size_t
mbi_smbios(struct multiboot_package *mbp, void *buf)
{
	size_t len = 0;
	struct multiboot_tag_smbios *mbt = buf;
	void *smbios_phys;
	struct smb3hdr *smbios3_phys = NULL;
	struct smb3hdr smbios3;
	struct smbhdr *smbios21_phys = NULL;
	struct smbhdr smbios21;
	size_t smbios_len;
	int major;
	int minor;
#if defined(EFIBOOT)
	const EFI_GUID smbios3_guid = SMBIOS3_TABLE_GUID;
	const EFI_GUID smbios21_guid = SMBIOS_TABLE_GUID;
	int i;

	if (ST == NULL)
		goto out;

	for (i = 0; i < ST->NumberOfTableEntries; i++)  {
		if (memcmp(&ST->ConfigurationTable[i].VendorGuid,
		   &smbios3_guid, sizeof(smbios3_guid)) == 0)
			smbios3_phys = ST->ConfigurationTable[i].VendorTable; 

		if (memcmp(&ST->ConfigurationTable[i].VendorGuid,
		   &smbios21_guid, sizeof(smbios21_guid)) == 0)
			smbios21_phys = ST->ConfigurationTable[i].VendorTable; 
	}
#else
	char *cp;
	char line[16];
	const char *smbios21_anchor = "_SM_";
	const char *smbios3_anchor = "_SM3_";

	for (cp = (char *)SMBIOS_START;
	     cp < (char *)SMBIOS_END;
	     cp += sizeof(buf)) {
		pvbcopy(cp, line, sizeof(line));
		if (memcmp(line, smbios3_anchor, strlen(smbios3_anchor)) == 0)
			smbios3_phys = (struct smb3hdr *)cp;
		if (memcmp(line, smbios21_anchor, strlen(smbios21_anchor)) == 0)
			smbios21_phys = (struct smbhdr *)cp;
	}
#endif
	if (smbios3_phys != NULL) {
		pvbcopy(smbios3_phys, &smbios3, sizeof(smbios3));
		smbios_len = smbios3.len;
		major = smbios3.majrev;
		minor = smbios3.minrev;
		smbios_phys = smbios3_phys;
	} else if (smbios21_phys != NULL) {
		pvbcopy(smbios21_phys, &smbios21, sizeof(smbios21));
		smbios_len = smbios21.len;
		major = smbios21.majrev;
		minor = smbios21.minrev;
		smbios_phys = smbios21_phys;
	} else {
		goto out;
	}

	len = sizeof(*mbt) + smbios_len;
	if (mbt) {
		mbt->type = MULTIBOOT_TAG_TYPE_SMBIOS;
		mbt->size = len;
		mbt->major = major;
		mbt->minor = minor;
		pvbcopy(smbios_phys, mbt->tables, smbios_len);
	}
out:
	return roundup(len, MULTIBOOT_TAG_ALIGN);
}

static size_t
mbi_network(struct multiboot_package *mbp, void *buf)
{
	size_t len = 0;
#ifdef notyet
	struct multiboot_tag_network *mbt = buf;

	if (saved_dhcpack == NULL || saved_dhcpack_len == 0)
		goto out;

	len = sizeof(*mbt) + saved_dhcpack_len;

	if (mbt) {
		mbt->type = MULTIBOOT_TAG_TYPE_NETWORK;
		mbt->size = len;
		memcpy(mbt->dhcpack, saved_dhcpack, saved_dhcpack_len);
	}
out:
#endif
	return roundup(len, MULTIBOOT_TAG_ALIGN);
}

static size_t
mbi_elf_sections(struct multiboot_package *mbp, void *buf)
{
	size_t len = 0;
	struct multiboot_tag_elf_sections *mbt = buf;
	Elf_Ehdr ehdr;
	Elf32_Ehdr *ehdr32 = NULL;
	Elf64_Ehdr *ehdr64 = NULL;
	uint32_t shnum, shentsize, shstrndx, shoff;
	size_t shdr_len;

	if (mbp->mbp_marks[MARK_SYM] == 0)
		goto out;

	pvbcopy((void *)mbp->mbp_marks[MARK_SYM], &ehdr, sizeof(ehdr));

	/*
	 * Check this is a ELF header
	 */
	if (memcmp(&ehdr.e_ident, ELFMAG, SELFMAG) != 0)
		goto out;

	switch (ehdr.e_ident[EI_CLASS]) {
	case ELFCLASS32:
		ehdr32 = (Elf32_Ehdr *)&ehdr;
		shnum = ehdr32->e_shnum;
		shentsize = ehdr32->e_shentsize;
		shstrndx = ehdr32->e_shstrndx;
		shoff = ehdr32->e_shoff;
		break;
	case ELFCLASS64:
		ehdr64 = (Elf64_Ehdr *)&ehdr;
		shnum = ehdr64->e_shnum;
		shentsize = ehdr64->e_shentsize;
		shstrndx = ehdr64->e_shstrndx;
		shoff = ehdr64->e_shoff;
		break;
	default:
		goto out;
	}

	shdr_len = shnum * shentsize;
	if (shdr_len == 0)
		goto out;

	len = sizeof(*mbt) + shdr_len;
	if (mbt) {
		int fd = -1;
		int ret = -1;

		mbt->type = MULTIBOOT_TAG_TYPE_ELF_SECTIONS;
		mbt->size = len;
		mbt->num = shnum;
		mbt->entsize = shentsize;
		mbt->shndx = shstrndx;
		
		if ((fd = open(mbp->mbp_file, 0)) == -1)
			goto out_read;

		if (lseek(fd, shoff, SEEK_SET) != shoff)
			goto out_read;
 
		if (read(fd, mbt + 1,  shdr_len) != shdr_len)
			goto out_read;

		ret = 0;
out_read:
		if (fd != -1)
			close(fd);

		if (ret != 0) {
			printf("Error reading ELF sections from %s\n",
			    mbp->mbp_file);
			len = 0;
		}
	}
out:
	return roundup(len, MULTIBOOT_TAG_ALIGN);
}

static size_t
mbi_end(struct multiboot_package *mbp, void *buf)
{
	struct multiboot_tag *mbt = buf;
	size_t len = sizeof(*mbt);

	if (mbt) {
		mbt->type = MULTIBOOT_TAG_TYPE_END;
		mbt->size = len;
	}

	return roundup(len, MULTIBOOT_TAG_ALIGN);
}

static size_t
mbi_load_base_addr(struct multiboot_package *mbp, void *buf)
{
	size_t len = 0;
	struct multiboot_tag_load_base_addr *mbt = buf;

	len = sizeof(*mbt);

	if (mbt) {
		mbt->type = MULTIBOOT_TAG_TYPE_LOAD_BASE_ADDR;
		mbt->size = len;
		mbt->load_base_addr = mbp->mbp_marks[MARK_START];
	}
	return roundup(len, MULTIBOOT_TAG_ALIGN);
}

#if defined(EFIBOOT)
/* Set if EFI ExitBootServices was not called */
static size_t
mbi_efi_bs(struct multiboot_package *mbp, void *buf)
{
	size_t len = 0;
	struct multiboot_tag *mbt = buf;

	if (mbp->mbp_priv->mpp_efi_bs == NULL)
		goto out;

	len = sizeof(*mbt);

	if (mbt) {
		mbt->type = MULTIBOOT_TAG_TYPE_EFI_BS;
		mbt->size = len;
	}

out:
	return roundup(len, MULTIBOOT_TAG_ALIGN);
}


static size_t
mbi_efi_mmap(struct multiboot_package *mbp, void *buf)
{
	EFI_STATUS status;
	EFI_MEMORY_DESCRIPTOR *desc;
	UINTN NoEntries, MapKey, DescriptorSize;
	UINT32 DescriptorVersion;
	size_t len = 0;
	struct multiboot_tag_efi_mmap *mbt = buf;

	desc = efi_memory_get_map(&NoEntries, &MapKey, &DescriptorSize,
	    &DescriptorVersion, false);

	if (desc == NULL)
		goto out;
		
	len = sizeof(*mbt) + DescriptorSize * NoEntries;

	if (mbt) {
		/* Call ExitBootService if required */
		//if (mbp->mbp_priv->mpp_efi_bs == NULL) {
		if (0) {
			status = uefi_call_wrapper(BS->ExitBootServices, 2, IH, MapKey);
			if (EFI_ERROR(status)) {
				FreePool(desc);
				desc = efi_memory_get_map(&NoEntries, &MapKey, &DescriptorSize,
				    &DescriptorVersion, false);
				status = uefi_call_wrapper(BS->ExitBootServices, 2, IH, MapKey);
				if (EFI_ERROR(status))
					panic("ExitBootServices failed");
			}
			len = sizeof(*mbt) + DescriptorSize * NoEntries;
			efi_exited = true;
		}

		mbt->type = MULTIBOOT_TAG_TYPE_EFI_MMAP;
		mbt->size = len;
		mbt->descr_size = DescriptorSize;
		mbt->descr_vers = DescriptorVersion;
		memcpy(mbt + 1, desc, DescriptorSize * NoEntries);
	}

out:
	return roundup(len, MULTIBOOT_TAG_ALIGN);
}



#ifndef __LP64__
static size_t
mbi_efi32_ih(struct multiboot_package *mbp, void *buf)
{
	size_t len = 0;
	struct multiboot_tag_efi32_ih *mbt = buf;

	len = sizeof(*mbt);

	if (mbt) {
		mbt->type = MULTIBOOT_TAG_TYPE_EFI32_IH;
		mbt->size = len;
		mbt->pointer = (multiboot_uint32_t)IH;
	}
	return roundup(len, MULTIBOOT_TAG_ALIGN);
}

static size_t
mbi_efi32(struct multiboot_package *mbp, void *buf)
{
	size_t len = 0;
	struct multiboot_tag_efi32 *mbt = buf;

	len = sizeof(*mbt);

	if (mbt) {
		mbt->type = MULTIBOOT_TAG_TYPE_EFI32;
		mbt->size = len;
		mbt->pointer = (multiboot_uint32_t)ST;
	}
	return roundup(len, MULTIBOOT_TAG_ALIGN);
}
#endif

#ifdef __LP64__
static size_t
mbi_efi64_ih(struct multiboot_package *mbp, void *buf)
{
	size_t len = 0;
	struct multiboot_tag_efi64_ih *mbt = buf;

	len = sizeof(*mbt);

	if (mbt) {
		mbt->type = MULTIBOOT_TAG_TYPE_EFI64_IH;
		mbt->size = len;
		mbt->pointer = (multiboot_uint64_t)IH;
	}
	return roundup(len, MULTIBOOT_TAG_ALIGN);
}

static size_t
mbi_efi64(struct multiboot_package *mbp, void *buf)
{
	size_t len = 0;
	struct multiboot_tag_efi64 *mbt = buf;

	len = sizeof(*mbt);

	if (mbt) {
		mbt->type = MULTIBOOT_TAG_TYPE_EFI64;
		mbt->size = len;
		mbt->pointer = (multiboot_uint64_t)ST;
	}
	return roundup(len, MULTIBOOT_TAG_ALIGN);
}
#endif /* __LP64__ */
#endif /* EFIBOOT */

static bool
is_tag_required(struct multiboot_package *mbp, uint16_t tag)
{
	bool ret = false;	
	int i;
	struct multiboot_header_tag_information_request *info_req;
	size_t nreq;

	info_req = mbp->mbp_priv->mpp_info_req;

	if (info_req == NULL)
		goto out;

	if (info_req->flags & MULTIBOOT_HEADER_TAG_OPTIONAL)
		goto out;

	nreq = (info_req->size - sizeof(*info_req))
	     / sizeof(info_req->requests[0]);

	for (i = 0; i < nreq; i++) {
		if (info_req->requests[i] == tag) {
			ret = true;
			break;
		}
	}
			
out:
	return ret;
}

static int 
mbi_dispatch(struct multiboot_package *mbp, uint16_t type, 
    char *bp, size_t *total_len)
{
	int ret = 0;
	size_t len = 0;

	printf("Debug: mbi_dispatch: type =%d\n", type);

	switch (type) {
	case MULTIBOOT_TAG_TYPE_END:
		len = mbi_end(mbp, bp);
		break;
	case MULTIBOOT_TAG_TYPE_CMDLINE:
		len = mbi_cmdline(mbp, bp);
		break;
	case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME:
		len = mbi_boot_loader_name(mbp, bp);
		break;
	case MULTIBOOT_TAG_TYPE_MODULE:
		len = mbi_modules(mbp, bp);
		break;
	case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:
		len = mbi_basic_meminfo(mbp, bp);
		break;
	case MULTIBOOT_TAG_TYPE_BOOTDEV:
		len = mbi_bootdev(mbp, bp);
		break;
	case MULTIBOOT_TAG_TYPE_MMAP:
		len = mbi_mmap(mbp, bp);
		break;
	case MULTIBOOT_TAG_TYPE_VBE:
		len = mbi_vbe(mbp, bp);
		break;
	case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
		len = mbi_framebuffer(mbp, bp);
		break;
	case MULTIBOOT_TAG_TYPE_ACPI_OLD:
		len = mbi_acpi_old(mbp, bp);
		break;
	case MULTIBOOT_TAG_TYPE_ACPI_NEW:
		len = mbi_acpi_new(mbp, bp);
		break;
	case MULTIBOOT_TAG_TYPE_ELF_SECTIONS:
		len = mbi_elf_sections(mbp, bp);
		break;
	case MULTIBOOT_TAG_TYPE_APM:
		len = mbi_apm(mbp, bp);
		break;
	case MULTIBOOT_TAG_TYPE_SMBIOS:
		len = mbi_smbios(mbp, bp);
		break;
	case MULTIBOOT_TAG_TYPE_NETWORK:	
		len = mbi_network(mbp, bp);
		break;
#if defined(EFIBOOT)
	case MULTIBOOT_TAG_TYPE_EFI_MMAP:
		len = mbi_efi_mmap(mbp, bp);
		break;
	case MULTIBOOT_TAG_TYPE_EFI_BS:
		len = mbi_efi_bs(mbp, bp);
		break;
#ifndef __LP64__
	case MULTIBOOT_TAG_TYPE_EFI32_IH:
		len = mbi_efi32_ih(mbp, bp);
		break;
	case MULTIBOOT_TAG_TYPE_EFI32:
		len = mbi_efi32(mbp, bp);
		break;
#else /* __LP64__ */
	case MULTIBOOT_TAG_TYPE_EFI64_IH:
		len = mbi_efi64_ih(mbp, bp);
		break;
	case MULTIBOOT_TAG_TYPE_EFI64:
		len = mbi_efi64(mbp, bp);
		break;
#endif /* __LP64__ */
#endif /* EFIBOOT */
	case MULTIBOOT_TAG_TYPE_LOAD_BASE_ADDR:
		len = mbi_load_base_addr(mbp, bp);
		break;
	default:
		len = 0;
		break;
	}

	if (len == 0 && is_tag_required(mbp, type))
		ret = -1;

	*total_len += len;
	return ret;
}

#if defined(EFIBOOT)
static void efi_setfb(struct multiboot_header_tag_framebuffer *fb)
{
	// TODO
}

#endif

static int
exec_multiboot2(struct multiboot_package *mbp)
{
	size_t len, alen;
	char *mbi = NULL;
	struct multiboot_package_priv *mpp = mbp->mbp_priv;
	uint16_t tags[] = {
		MULTIBOOT_TAG_TYPE_CMDLINE,
		MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME,
		MULTIBOOT_TAG_TYPE_MODULE,
		MULTIBOOT_TAG_TYPE_BASIC_MEMINFO,
		MULTIBOOT_TAG_TYPE_BOOTDEV,
		MULTIBOOT_TAG_TYPE_VBE,
		MULTIBOOT_TAG_TYPE_FRAMEBUFFER,
		MULTIBOOT_TAG_TYPE_ELF_SECTIONS,
		MULTIBOOT_TAG_TYPE_APM,
		MULTIBOOT_TAG_TYPE_SMBIOS,
		MULTIBOOT_TAG_TYPE_ACPI_OLD,
		MULTIBOOT_TAG_TYPE_ACPI_NEW,
		MULTIBOOT_TAG_TYPE_NETWORK,
		MULTIBOOT_TAG_TYPE_LOAD_BASE_ADDR,
#if defined(EFIBOOT)
		MULTIBOOT_TAG_TYPE_EFI_BS,
#ifndef __LP64__
		MULTIBOOT_TAG_TYPE_EFI32,
		MULTIBOOT_TAG_TYPE_EFI32_IH,
#else
		MULTIBOOT_TAG_TYPE_EFI64,
		MULTIBOOT_TAG_TYPE_EFI64_IH,
#endif /* __LP64__ */
#endif /* EFIBOOT */
		/*
		 * EFI_MMAP and MMAP at the end so that they
		 * catch page allocation made for other tags.
		 */
		MULTIBOOT_TAG_TYPE_MMAP,
#if defined(EFIBOOT)
		MULTIBOOT_TAG_TYPE_EFI_MMAP,
#endif /* EFIBOOT */
		MULTIBOOT_TAG_TYPE_END, /* Must be last */
	};
	physaddr_t entry;
	int i;

	printf("Debug: exec_multiboot2 start\n");

	BI_ALLOC(BTINFO_MAX);

	/* set new video mode if text mode was not requested */
	if (mpp->mpp_framebuffer == NULL ||
	    mpp->mpp_framebuffer->depth != 0)
#if defined(EFIBOOT)
	efi_setfb(mpp->mpp_framebuffer);
#else
	vbe_commit();  
#endif

	len = 2 * sizeof(multiboot_uint32_t);
	for (i = 0; i < sizeof(tags) / sizeof(*tags); i++) {
		if (mbi_dispatch(mbp, tags[i], NULL, &len) != 0)
			goto fail;
	}

	/* Add extra 256 for mmap reserve */
	mpp->mpp_mbi_len = len + MULTIBOOT_TAG_ALIGN + 256;
	mpp->mpp_mbi = alloc(mpp->mpp_mbi_len);
	if (mpp->mpp_mbi == NULL) {
		printf("Failed to allocate mbi\n");
		return -1;
	}
	mbi = (char *)roundup((vaddr_t)mpp->mpp_mbi, MULTIBOOT_TAG_ALIGN);

	alen = 2 * sizeof(multiboot_uint32_t);
	for (i = 0; i < sizeof(tags) / sizeof(*tags); i++) {
		if (mbi_dispatch(mbp, tags[i], mbi + alen, &alen) != 0)
			goto fail;

		/*
		 * It may shrink because of failure when filling
		 * structures, but it should not grow.
		 */
		if (alen > len)
			panic("multiboot2 info size mismatch");
	}

	((multiboot_uint32_t *)mbi)[0] = alen;	/* total size */
	((multiboot_uint32_t *)mbi)[1] = 0;	/* reserved */

	printf("Debug: mbi alloc passed\n");

#ifdef MULTIBOOT2_DEBUG
	multiboot2_info_dump(MULTIBOOT2_BOOTLOADER_MAGIC, mbi);
#endif /* MULTIBOOT2_DEBUG */

#if defined(EFIBOOT)
	/* Call ExitBootService if required */
	if ((mpp->mpp_efi_bs == NULL) && (efi_exited == false))
		efi_cleanup();
#endif

	printf("Start @ 0x%lx [%ld=0x%lx-0x%lx]...\n",
	    mbp->mbp_marks[MARK_ENTRY],
	    mbp->mbp_marks[MARK_NSYM],
	    mbp->mbp_marks[MARK_SYM],
	    mbp->mbp_marks[MARK_END]);

	entry = mbp->mbp_marks[MARK_ENTRY];

	if (mpp->mpp_entry)
		entry = mpp->mpp_entry->entry_addr;
#if defined(EFIBOOT)
#ifdef __LP64__
	if (mpp->mpp_entry_elf64)
		entry = mpp->mpp_entry_elf64->entry_addr
		      + efi_loadaddr;
#else
	if (mpp->mpp_entry_elf32)
		entry = mpp->mpp_entry_elf32->entry_addr
		      + efi_loadaddr;
#endif /* __LP64__ */
#endif /* EFIBOOT */

	/* Does not return */
	multiboot(entry, vtophys(mbi),
	    x86_trunc_page(mbp->mbp_basemem * 1024),
	    MULTIBOOT2_BOOTLOADER_MAGIC);
fail:
	return -1;
}

static void
cleanup_multiboot2(struct multiboot_package *mbp)
{
	if (mbp->mbp_header)
		dealloc(mbp->mbp_header, mbp->mbp_header->header_length);
	if (mbp->mbp_priv && mbp->mbp_priv->mpp_mbi)
		dealloc(mbp->mbp_priv->mpp_mbi, mbp->mbp_priv->mpp_mbi_len);
	if (mbp->mbp_priv)
		dealloc(mbp->mbp_priv, sizeof(*mbp->mbp_priv));

	dealloc(mbp, sizeof(*mbp));

	return;
}

static bool
is_header_required(struct multiboot_header_tag *mbt)
{
	bool ret = false;

	if (mbt == NULL)
		goto out;

	if (mbt->flags & MULTIBOOT_HEADER_TAG_OPTIONAL)
		goto out;

	ret = true;
out:
	return ret;
}

#define NEXT_HEADER(mbt) ((struct multiboot_header_tag *) \
   ((char *)mbt + roundup(mbt->size, MULTIBOOT_HEADER_ALIGN)))

struct multiboot_package *
probe_multiboot2(const char *path)
{
	int fd = -1;
	size_t i;
	char buf[MULTIBOOT_SEARCH + sizeof(struct multiboot_header)];
	ssize_t readen;
	struct multiboot_package *mbp = NULL;
	struct multiboot_header *mbh;
	struct multiboot_header_tag *mbt;
	size_t mbh_len = 0;

	if ((fd = open(path, 0)) == -1)
		goto out;
 
	readen = read(fd, buf, sizeof(buf));
	if (readen < sizeof(struct multiboot_header))
		goto out;

	for (i = 0; i < readen; i += MULTIBOOT_HEADER_ALIGN) {
		mbh = (struct multiboot_header *)(buf + i);
		
		if (mbh->magic != MULTIBOOT2_HEADER_MAGIC)
			continue;

		if (mbh->architecture != MULTIBOOT_ARCHITECTURE_I386)
			continue;
		
		if (mbh->magic + mbh->architecture + 
		    mbh->header_length + mbh->checksum)
			continue;
		mbh_len = mbh->header_length;

		mbp = alloc(sizeof(*mbp));
		mbp->mbp_version	= 2;
		mbp->mbp_file		= path;
		mbp->mbp_header		= alloc(mbh_len);
		mbp->mbp_priv		= alloc(sizeof(*mbp->mbp_priv));
		memset(mbp->mbp_priv, 0, sizeof (*mbp->mbp_priv));
		mbp->mbp_probe		= *probe_multiboot2;
		mbp->mbp_exec		= *exec_multiboot2;
		mbp->mbp_cleanup	= *cleanup_multiboot2;

		break;
	}

	if (mbp == NULL)
		goto out;

	if (lseek(fd, i, SEEK_SET) != i) {
		printf("lseek failed");
		mbp->mbp_cleanup(mbp);
		mbp = NULL;
		goto out;
	}

	mbh = mbp->mbp_header;
	if (read(fd, mbh, mbh_len) != mbh_len) {
		printf("read failed");
		mbp->mbp_cleanup(mbp);
		mbp = NULL;
		goto out;
	}

	for (mbt = (struct multiboot_header_tag *)(mbh + 1);
	     (char *)mbt - (char *)mbh < mbh_len;
	     mbt = NEXT_HEADER(mbt)) {

		switch(mbt->type) {
		case MULTIBOOT_HEADER_TAG_INFORMATION_REQUEST:
			mbp->mbp_priv->mpp_info_req = (void *)mbt;
			break;
		case MULTIBOOT_HEADER_TAG_ADDRESS:
			mbp->mbp_priv->mpp_address = (void *)mbt;
			break;
		case MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS:
			mbp->mbp_priv->mpp_entry = (void *)mbt;
			break;
		case MULTIBOOT_HEADER_TAG_CONSOLE_FLAGS:
			mbp->mbp_priv->mpp_console = (void *)mbt;

		case MULTIBOOT_HEADER_TAG_FRAMEBUFFER:
			mbp->mbp_priv->mpp_framebuffer = (void *)mbt;
			break;
		case MULTIBOOT_HEADER_TAG_MODULE_ALIGN:
			mbp->mbp_priv->mpp_module_align = (void *)mbt;
			break;
#if defined(EFIBOOT)
		case MULTIBOOT_HEADER_TAG_EFI_BS:
			mbp->mbp_priv->mpp_efi_bs = (void *)mbt;
			break;
		case MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS_EFI32:
			mbp->mbp_priv->mpp_entry_elf32 = (void *)mbt;
			break;
		case MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS_EFI64:
			mbp->mbp_priv->mpp_entry_elf64 = (void *)mbt;
			break;
#endif
		case MULTIBOOT_HEADER_TAG_RELOCATABLE:
			mbp->mbp_priv->mpp_relocatable = (void *)mbt;
			break;
		case MULTIBOOT_HEADER_TAG_END: /* FALLTHROUGH */
		default:
			break;
		}
	}

#ifdef MULTIBOOT2_DEBUG
	multiboot2_header_dump(mbp);
#endif /* MULTIBOOT2_DEBUG */

	/*
	 * multiboot header fully supported
	 *  MULTIBOOT_HEADER_TAG_INFORMATION_REQUEST
	 *  MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS
	 *  MULTIBOOT_HEADER_TAG_MODULE_ALIGN (we always load as page aligned)
	 *  MULTIBOOT_HEADER_TAG_EFI_BS
	 *  MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS_EFI32
	 *  MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS_EFI64
	 *  MULTIBOOT_HEADER_TAG_CONSOLE_FLAGS (we always have a console)
	 *
	 * Not supported:
	 *  MULTIBOOT_HEADER_TAG_ADDRESS
	 *  MULTIBOOT_HEADER_TAG_FRAMEBUFFER (but spec says it is onty a hint)
	 *  MULTIBOOT_HEADER_TAG_RELOCATABLE
	 */

	if (is_header_required((void *)mbp->mbp_priv->mpp_address)) {
		printf("Unsupported multiboot address header\n");
		mbp->mbp_cleanup(mbp);
		mbp = NULL;
		goto out;
	}

#if defined(EFIBOOT)
	/* uefi requires framubuffer */
	if (mbp->mbp_priv->mpp_framebuffer == NULL) {
		static struct multiboot_header_tag_framebuffer default_fb;
		default_fb.type = MULTIBOOT_HEADER_TAG_FRAMEBUFFER;
		default_fb.flags = 0;
		default_fb.size = 20;
		default_fb.width = 800;
		default_fb.height = 600;
		default_fb.depth = 32;

		mbp->mbp_priv->mpp_framebuffer = &default_fb;
	}
#endif

#if defined(EFIBOOT)
	/*
	 * We do not fully support the relocatable header, but
	 * at least we honour the alignment request. Xen requires
	 * that to boot.
	 */
	struct multiboot_header_tag_relocatable *reloc = 
	    mbp->mbp_priv->mpp_relocatable;
	if (reloc)
		efi_loadaddr = roundup(efi_loadaddr, reloc->align);
#endif

	if (is_header_required((void *)mbp->mbp_priv->mpp_relocatable)) {
		printf("Unsupported multiboot relocatable header\n");
		mbp->mbp_cleanup(mbp);
		mbp = NULL;
		goto out;
	}

out:

	if (fd != -1)
		close(fd);

	return mbp;
}

#endif /* NO_MULTIBOOT2 */
