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

#include "efiboot.h"
#include "efiacpi.h"
#include "smbios.h"
#include "module.h"

#include <sys/bootblock.h>
#include <sys/queue.h>

#include <loadfile.h>

#include <i386/multiboot2.h>

/*
 * Headroom reserved in the multiboot2 info buffer so the MMAP/EFI_MMAP tags
 * can grow between the sizing and fill passes (the EFI memory map gains
 * descriptors from intervening allocations).  One page is far more than the
 * handful of descriptors that can appear in practice.
 */
#define MB2_MBI_RESERVE	4096

extern const char bootprog_name[], bootprog_rev[], bootprog_kernrev[];
extern char twiddle_toggle;
//extern u_long load_offset;

static bool efi_exited = false;

typedef struct multiboot_header_private {
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
} mhp_t;

typedef struct multiboot_package {
	struct multiboot_header	*mbp_header;
	const char		*mbp_file;
	const char		*mbp_args;
	u_long			*mbp_marks;
	mhp_t			*mbp_mhp;
} mbp_t;

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
			printf("\n");
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
	struct multiboot_header	*mbh = mbp->mbp_header;
	struct multiboot_header_private *mpp = mbp->mbp_mhp;

	printf("\n=== multiboot2 header dump start ===\n");
	printf("header magic: %x\n", mbh->magic);
	printf("header arch: %d\n", mbh->architecture);
	printf("header length: %d\n", mbh->header_length);
	
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

static void
show_marks(u_long *marks)
{
	printf("Mark Start: %8lX\n", marks[MARK_START]);
	printf("Mark Entry: %8lX\n", marks[MARK_ENTRY]);
	printf("Mark Data:  %8lX\n", marks[MARK_DATA]);
	printf("Mark NSym:  %8lX\n", marks[MARK_NSYM]);
	printf("Mark Sym:   %8lX\n", marks[MARK_SYM]);
	printf("Mark End:   %8lX\n", marks[MARK_END]);
}

#endif /* MULTIBOOT2_DEBUG */

static size_t
mbi_cmdline(struct multiboot_package *mbp, void *buf)
{
	struct multiboot_tag_string *mbt = buf;
	size_t cmdlen;
	size_t len;
	const char fmt[] = "%s %s";

	/* strlen(file) + ' ' + strlen(args) + '\0' */
	cmdlen = strlen(mbp->mbp_file) + 1 + strlen(mbp->mbp_args) + 1;
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
	const char *fmt = "%s, Revision %s (from NetBSD %s)";


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

/*
 * Boot modules for the multiboot2 info, loaded from the "load"
 * command list (module_foreach).  Names with a path or device prefix
 * are opened as given (the MINIX mod01_ds... convention); bare names
 * use the NetBSD kernel module path.  Module addresses are 32-bit in
 * multiboot2, so allocate below 4GB regardless of platform.
 */
struct mb2_module {
	char *path;
	EFI_PHYSICAL_ADDRESS addr;
	u_long size;
	TAILQ_ENTRY(mb2_module) entries;
};
static TAILQ_HEAD(, mb2_module) mb2_modules =
    TAILQ_HEAD_INITIALIZER(mb2_modules);
static int mb2_module_nfail;

static void
mb2_load_module_cb(const char *name)
{
	struct mb2_module *m;
	EFI_PHYSICAL_ADDRESS addr;
	EFI_STATUS status;
	struct stat st;
	char path[512];
	ssize_t len;
	int fd;

	if (strchr(name, '/') != NULL || strchr(name, ':') != NULL)
		snprintf(path, sizeof(path), "%s", name);
	else
		snprintf(path, sizeof(path), "%s/%s/%s.kmod",
		    module_prefix, name, name);

	fd = open(path, 0);
	if (fd < 0 || fstat(fd, &st) < 0 || st.st_size <= 0) {
		printf("multiboot2: %s: %s\n", path, strerror(errno));
		if (fd >= 0)
			close(fd);
		mb2_module_nfail++;
		return;
	}

	addr = 0xFFFFFFFFULL;
	status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateMaxAddress,
	    EfiLoaderData, EFI_SIZE_TO_PAGES(st.st_size), &addr);
	if (EFI_ERROR(status)) {
		printf("multiboot2: %s: allocation of %" PRIdMAX
		    " bytes failed\n", path, (intmax_t)st.st_size);
		close(fd);
		mb2_module_nfail++;
		return;
	}

	printf("multiboot2: loading %s ", path);
	len = read(fd, (void *)(uintptr_t)addr, st.st_size);
	close(fd);
	if (len < st.st_size) {
		printf("FAILED\n");
		uefi_call_wrapper(BS->FreePages, 2, addr,
		    EFI_SIZE_TO_PAGES(st.st_size));
		mb2_module_nfail++;
		return;
	}
	printf("done.\n");

	m = alloc(sizeof(*m));
	m->path = alloc(strlen(path) + 1);
	strcpy(m->path, path);
	m->addr = addr;
	m->size = len;
	TAILQ_INSERT_TAIL(&mb2_modules, m, entries);
}

static void
mb2_load_modules(const char *kernel_path)
{
	if (!module_enabled)
		return;

	mb2_module_nfail = 0;
	module_init(kernel_path);
	module_foreach(mb2_load_module_cb);
	if (mb2_module_nfail > 0)
		printf("multiboot2: WARNING: %d module%s failed to load\n",
		    mb2_module_nfail, mb2_module_nfail == 1 ? "" : "s");
}

static void
mb2_free_modules(void)
{
	struct mb2_module *m;

	while ((m = TAILQ_FIRST(&mb2_modules)) != NULL) {
		TAILQ_REMOVE(&mb2_modules, m, entries);
		uefi_call_wrapper(BS->FreePages, 2, m->addr,
		    EFI_SIZE_TO_PAGES(m->size));
		dealloc(m->path, strlen(m->path) + 1);
		dealloc(m, sizeof(*m));
	}
}

static size_t
mbi_modules(struct multiboot_package *mbp, void *buf)
{
	struct multiboot_tag_module *mbt = buf;
	struct mb2_module *m;
	size_t len = 0, mbt_len, mbt_len_align, pathlen;

	TAILQ_FOREACH(m, &mb2_modules, entries) {
		pathlen = strlen(m->path) + 1;
		mbt_len = sizeof(*mbt) + pathlen;
		mbt_len_align = roundup(mbt_len, MULTIBOOT_TAG_ALIGN);
		len += mbt_len_align;

		if (mbt != NULL) {
			mbt->type = MULTIBOOT_TAG_TYPE_MODULE;
			mbt->size = mbt_len;
			mbt->mod_start = m->addr;
			mbt->mod_end = m->addr + m->size;
			memcpy(mbt->cmdline, m->path, pathlen);
			mbt = (struct multiboot_tag_module *)
			    ((char *)mbt + mbt_len_align);
		}
	}

	return len;
}

static size_t
mbi_mmap(struct multiboot_package *mbp, void *buf)
{
	size_t len = 0;
	struct multiboot_tag_mmap *mbt = buf;
	UINTN nentries = 0, mapkey, descsize;
	EFI_MEMORY_DESCRIPTOR *md, *memmap;
	UINT32 descver;

	memmap = LibMemoryMap(&nentries, &mapkey, &descsize, &descver);
	
	len = sizeof(*mbt) + nentries * sizeof(mbt->entries[0]);
	
	if (mbt) {
		int i,j;
		struct multiboot_mmap_entry *mbte;
		struct multiboot_mmap_entry tmp_mme;

		memset(mbt, 0, len);
		mbt->type = MULTIBOOT_TAG_TYPE_MMAP;
		mbt->size = len;
		mbt->entry_size = sizeof(mbt->entries[0]);
		mbt->entry_version = 0;

		mbte = (struct multiboot_mmap_entry *)(mbt + 1);
		for (i = 0, j = 0, md = memmap; i < nentries; i++, 
					md = NextMemoryDescriptor(md, descsize)) {
			tmp_mme.addr = md->PhysicalStart;
			tmp_mme.len = md->NumberOfPages * EFI_PAGE_SIZE;
			switch(md->Type) {
			case EfiLoaderCode:
			case EfiLoaderData:
			case EfiBootServicesCode:
			case EfiBootServicesData:
			case EfiConventionalMemory:
				tmp_mme.type = MULTIBOOT_MEMORY_AVAILABLE;
				break;
			case EfiReservedMemoryType:
				tmp_mme.type = MULTIBOOT_MEMORY_RESERVED;
				break;
			case EfiACPIReclaimMemory:
				tmp_mme.type = 
				    MULTIBOOT_MEMORY_ACPI_RECLAIMABLE;
				break;
			case EfiACPIMemoryNVS:
				tmp_mme.type = MULTIBOOT_MEMORY_NVS;
				break;
			case EfiUnusableMemory:
				tmp_mme.type = MULTIBOOT_MEMORY_BADRAM;
				break;
			default:
				tmp_mme.type = MULTIBOOT_MEMORY_RESERVED;
				break;
			}
			tmp_mme.zero = 0;
			if (j == 0) {
				mbte[j] = tmp_mme;
				j++;
			}
			else if ((mbte[j-1].type == tmp_mme.type) &&
					((mbte[j-1].addr + mbte[j-1].len) == tmp_mme.addr)) {
				mbte[j-1].len += tmp_mme.len;
			}
			else {
				mbte[j] = tmp_mme;
				j++;
			}
		}
	}
	
	return roundup(len, MULTIBOOT_TAG_ALIGN);
}

static size_t
mbi_efi_mmap(struct multiboot_package *mbp, void *buf)
{
	size_t len = 0;
	struct multiboot_tag_efi_mmap *mbt = buf;

	UINTN nentries = 0, mapkey, descsize;
	EFI_MEMORY_DESCRIPTOR *memmap;
	UINT32 descver;

	memmap = LibMemoryMap(&nentries, &mapkey, &descsize, &descver);

	if (memmap == NULL)
		goto out;
		
	len = sizeof(*mbt) + descsize * nentries;

	if (mbt) {
		mbt->type = MULTIBOOT_TAG_TYPE_EFI_MMAP;
		mbt->size = len;
		mbt->descr_size = descsize;
		mbt->descr_vers = descver;
		memcpy(mbt + 1, memmap, descsize * nentries);
	}

out:
	return roundup(len, MULTIBOOT_TAG_ALIGN);
}

static size_t
mbi_framebuffer(struct multiboot_package *mbp, void *buf)
{
	size_t len = 0;
	struct multiboot_tag_framebuffer *mbt = buf;
	EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
	EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *mode;

	gop = (EFI_GRAPHICS_OUTPUT_PROTOCOL *)efi_gop_found();
	if (gop == NULL) return 0;
	mode = gop->Mode;
	
	len = sizeof(*mbt);
	
	if (mbt) {
		mbt->common.type = MULTIBOOT_TAG_TYPE_FRAMEBUFFER;
		mbt->common.size = len;
		mbt->common.reserved = 0;

		mbt->common.framebuffer_addr = mode->FrameBufferBase;
		mbt->common.framebuffer_width = mode->Info->HorizontalResolution;
		mbt->common.framebuffer_height = mode->Info->VerticalResolution;
		mbt->common.framebuffer_bpp = 32;
		/* PixelsPerScanLine is in pixels; multiboot2 pitch is bytes/scanline */
		mbt->common.framebuffer_pitch =
		    mode->Info->PixelsPerScanLine * (mbt->common.framebuffer_bpp / 8);
		mbt->common.framebuffer_type =
		    MULTIBOOT_FRAMEBUFFER_TYPE_RGB;
			
		switch (mbt->common.framebuffer_type) {
		case MULTIBOOT_FRAMEBUFFER_TYPE_RGB:
			if (mode->Info->PixelFormat ==
				 	PixelRedGreenBlueReserved8BitPerColor) {
				mbt->framebuffer_red_field_position = 0;
				mbt->framebuffer_red_mask_size = 8;
				mbt->framebuffer_green_field_position = 8;
				mbt->framebuffer_green_mask_size = 8;
				mbt->framebuffer_blue_field_position = 16;
				mbt->framebuffer_blue_mask_size = 8;
			} else if (mode->Info->PixelFormat ==
				 	PixelBlueGreenRedReserved8BitPerColor) {
				mbt->framebuffer_red_field_position = 16;
				mbt->framebuffer_red_mask_size = 8;
				mbt->framebuffer_green_field_position = 8;
				mbt->framebuffer_green_mask_size = 8;
				mbt->framebuffer_blue_field_position = 0;
				mbt->framebuffer_blue_mask_size = 8;
			}
			break;
		default:
			break;
		}
	}

	return roundup(len, MULTIBOOT_TAG_ALIGN);
}

static size_t
mbi_acpi_new(struct multiboot_package *mbp, void *buf)
{
	size_t len = 0;
	struct multiboot_tag_new_acpi *mbt = buf;
	void *rsdp_phys;

	rsdp_phys = efi_acpi_root();
	if (rsdp_phys == NULL)
		goto out;

	len = sizeof(*mbt) + efi_acpi_length();
		
	if (mbt) {
		mbt->type = MULTIBOOT_TAG_TYPE_ACPI_NEW;
		mbt->size = len;
		bcopy((void *)(vaddr_t)rsdp_phys, mbt->rsdp, efi_acpi_length());
	}
out:
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
	if (smbios3_phys != NULL) {
		bcopy(smbios3_phys, &smbios3, sizeof(smbios3));
		smbios_len = smbios3.len;
		major = smbios3.majrev;
		minor = smbios3.minrev;
		smbios_phys = smbios3_phys;
	} else if (smbios21_phys != NULL) {
		bcopy(smbios21_phys, &smbios21, sizeof(smbios21));
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
		bcopy(smbios_phys, mbt->tables, smbios_len);
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
	union {
	    Elf32_Ehdr e32;
	    Elf64_Ehdr e64;
	} ehdr;
	Elf32_Ehdr *ehdr32 = NULL;
	Elf64_Ehdr *ehdr64 = NULL;
	uint32_t shnum, shentsize, shstrndx, shoff;
	size_t shdr_len;

	if (mbp->mbp_marks[MARK_SYM] == 0)
		goto out;

	bcopy((void *)mbp->mbp_marks[MARK_SYM], &ehdr, sizeof(ehdr));

	/*
	 * Check this is a ELF header
	 */
	if (memcmp(&ehdr.e32.e_ident, ELFMAG, SELFMAG) != 0)
		goto out;

	switch (ehdr.e32.e_ident[EI_CLASS]) {
	case ELFCLASS32:
		ehdr32 = (Elf32_Ehdr *)&ehdr.e32;
		shnum = ehdr32->e_shnum;
		shentsize = ehdr32->e_shentsize;
		shstrndx = ehdr32->e_shstrndx;
		shoff = ehdr32->e_shoff;
		break;
	case ELFCLASS64:
		ehdr64 = (Elf64_Ehdr *)&ehdr.e64;
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

/* Set if EFI ExitBootServices was not called */
static size_t
mbi_efi_bs(struct multiboot_package *mbp, void *buf)
{
	size_t len = 0;
	struct multiboot_tag *mbt = buf;

	if (mbp->mbp_mhp->mpp_efi_bs == NULL)
		goto out;

	len = sizeof(*mbt);

	if (mbt) {
		mbt->type = MULTIBOOT_TAG_TYPE_EFI_BS;
		mbt->size = len;
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

static bool
is_tag_required(struct multiboot_package *mbp, uint16_t tag)
{
	bool ret = false;	
	int i;
	struct multiboot_header_tag_information_request *info_req;
	size_t nreq;

	info_req = mbp->mbp_mhp->mpp_info_req;

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
	case MULTIBOOT_TAG_TYPE_MMAP:
		len = mbi_mmap(mbp, bp);
		break;
	case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
		len = mbi_framebuffer(mbp, bp);
		break;
	case MULTIBOOT_TAG_TYPE_ACPI_NEW:
		len = mbi_acpi_new(mbp, bp);
		break;
	case MULTIBOOT_TAG_TYPE_ELF_SECTIONS:
		len = mbi_elf_sections(mbp, bp);
		break;
	case MULTIBOOT_TAG_TYPE_SMBIOS:
		len = mbi_smbios(mbp, bp);
		break;
	case MULTIBOOT_TAG_TYPE_NETWORK:	
		len = mbi_network(mbp, bp);
		break;
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

static int
start_multiboot2(struct multiboot_package *mbp)
{
	size_t len, alen;
	char *mbi = NULL;
	struct multiboot_header_private *mpp = mbp->mbp_mhp;
	uint16_t tags[] = {
		MULTIBOOT_TAG_TYPE_CMDLINE,
		MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME,
		MULTIBOOT_TAG_TYPE_MODULE,
		MULTIBOOT_TAG_TYPE_FRAMEBUFFER,
		MULTIBOOT_TAG_TYPE_ELF_SECTIONS,
		MULTIBOOT_TAG_TYPE_SMBIOS,
		MULTIBOOT_TAG_TYPE_ACPI_OLD,
		MULTIBOOT_TAG_TYPE_ACPI_NEW,
		MULTIBOOT_TAG_TYPE_NETWORK,
		MULTIBOOT_TAG_TYPE_LOAD_BASE_ADDR,
		MULTIBOOT_TAG_TYPE_EFI_BS,
#ifndef __LP64__
		MULTIBOOT_TAG_TYPE_EFI32,
		MULTIBOOT_TAG_TYPE_EFI32_IH,
#else
		MULTIBOOT_TAG_TYPE_EFI64,
		MULTIBOOT_TAG_TYPE_EFI64_IH,
#endif /* __LP64__ */
		/*
		 * EFI_MMAP and MMAP at the end so that they
		 * catch page allocation made for other tags.
		 */
		MULTIBOOT_TAG_TYPE_MMAP,
		MULTIBOOT_TAG_TYPE_EFI_MMAP,
		MULTIBOOT_TAG_TYPE_END, /* Must be last */
	};
	physaddr_t entry;
	int i;
	UINT32 mode;
	EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;

	len = 2 * sizeof(multiboot_uint32_t);
	for (i = 0; i < sizeof(tags) / sizeof(*tags); i++) {
		if (mbi_dispatch(mbp, tags[i], NULL, &len) != 0)
			goto fail;
	}

	/*
	 * The MMAP/EFI_MMAP tags re-read the EFI memory map on the fill pass,
	 * and the alloc() below (plus LibMemoryMap's own pool allocations)
	 * adds descriptors between the sizing and fill passes, so those tags
	 * legitimately grow.  Reserve a page of headroom in the buffer and let
	 * the fill pass spill into it instead of panicking on any growth.
	 */
	mpp->mpp_mbi_len = len + MULTIBOOT_TAG_ALIGN + MB2_MBI_RESERVE;
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
		 * The fill pass may grow relative to the sizing pass (memory
		 * map changes); only the reserved headroom is guaranteed, so
		 * panic only if we would overrun the allocated buffer.
		 */
		if (alen > len + MB2_MBI_RESERVE)
			panic("multiboot2 info size mismatch");
	}

	((multiboot_uint32_t *)mbi)[0] = alen;	/* total size */
	((multiboot_uint32_t *)mbi)[1] = 0;	/* reserved */

#ifdef MULTIBOOT2_DEBUG
	multiboot2_info_dump(MULTIBOOT2_BOOTLOADER_MAGIC, mbi);
#endif /* MULTIBOOT2_DEBUG */

	printf("Start @ 0x%lx [%ld=0x%lx-0x%lx]...\n",
	    mbp->mbp_marks[MARK_START],
	    mbp->mbp_marks[MARK_ENTRY],
	    mbp->mbp_marks[MARK_SYM],
	    mbp->mbp_marks[MARK_END]);

	entry = mbp->mbp_marks[MARK_ENTRY];

	if (mpp->mpp_entry)
		entry = mpp->mpp_entry->entry_addr;
#ifdef __LP64__
	/* Prefer the EFI64-specific entry point (tag type 9) on 64-bit builds;
	 * it expects to be entered in long mode, unlike the type-3 entry which
	 * is 32-bit protected-mode code. */
	if (mpp->mpp_entry_elf64)
		entry = mpp->mpp_entry_elf64->entry_addr;
#endif

	gop = (EFI_GRAPHICS_OUTPUT_PROTOCOL *)efi_gop_found();
	if (gop != NULL) {
		mode = gop->Mode->Mode;
		efi_gop_setmode(mode);
	}

	/* Call ExitBootService if required */
	if ((mpp->mpp_efi_bs == NULL) && (efi_exited == false))
		efi_cleanup();

	/* Does not return */
	multiboot2(entry, (physaddr_t)mbi, MULTIBOOT2_BOOTLOADER_MAGIC);
	
fail:
	return -1;
}

static void
cleanup_multiboot2(struct multiboot_package *mbp)
{
	if (mbp->mbp_header)
		dealloc(mbp->mbp_header, mbp->mbp_header->header_length);
	if (mbp->mbp_mhp && mbp->mbp_mhp->mpp_mbi)
		dealloc(mbp->mbp_mhp->mpp_mbi, mbp->mbp_mhp->mpp_mbi_len);
	if (mbp->mbp_mhp)
		dealloc(mbp->mbp_mhp, sizeof(*mbp->mbp_mhp));

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

void *
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
		mbp->mbp_header		= alloc(mbh_len);
		mbp->mbp_mhp		= alloc(sizeof(*mbp->mbp_mhp));
		memset(mbp->mbp_mhp, 0, sizeof (*mbp->mbp_mhp));

		break;
	}

	if (mbp == NULL)
		goto out;

	if (lseek(fd, i, SEEK_SET) != i) {
		printf("lseek failed");
		cleanup_multiboot2(mbp);
		mbp = NULL;
		goto out;
	}

	mbh = mbp->mbp_header;
	if (read(fd, mbh, mbh_len) != mbh_len) {
		printf("read failed");
		cleanup_multiboot2(mbp);
		mbp = NULL;
		goto out;
	}

	for (mbt = (struct multiboot_header_tag *)(mbh + 1);
	     (char *)mbt - (char *)mbh < mbh_len;
	     mbt = NEXT_HEADER(mbt)) {

		switch(mbt->type) {
		case MULTIBOOT_HEADER_TAG_INFORMATION_REQUEST:
			mbp->mbp_mhp->mpp_info_req = (void *)mbt;
			break;
		case MULTIBOOT_HEADER_TAG_ADDRESS:
			mbp->mbp_mhp->mpp_address = (void *)mbt;
			break;
		case MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS:
			mbp->mbp_mhp->mpp_entry = (void *)mbt;
			break;
		case MULTIBOOT_HEADER_TAG_CONSOLE_FLAGS:
			mbp->mbp_mhp->mpp_console = (void *)mbt;

		case MULTIBOOT_HEADER_TAG_FRAMEBUFFER:
			mbp->mbp_mhp->mpp_framebuffer = (void *)mbt;
			break;
		case MULTIBOOT_HEADER_TAG_MODULE_ALIGN:
			mbp->mbp_mhp->mpp_module_align = (void *)mbt;
			break;
		case MULTIBOOT_HEADER_TAG_EFI_BS:
			mbp->mbp_mhp->mpp_efi_bs = (void *)mbt;
			break;
		case MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS_EFI32:
			mbp->mbp_mhp->mpp_entry_elf32 = (void *)mbt;
			break;
		case MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS_EFI64:
			mbp->mbp_mhp->mpp_entry_elf64 = (void *)mbt;
			break;
		case MULTIBOOT_HEADER_TAG_RELOCATABLE:
			mbp->mbp_mhp->mpp_relocatable = (void *)mbt;
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

	if (is_header_required((void *)mbp->mbp_mhp->mpp_address)) {
		printf("Unsupported multiboot address header\n");
		cleanup_multiboot2(mbp);
		mbp = NULL;
		goto out;
	}

	/*
	 * We do not fully support the relocatable header, but
	 * at least we honour the alignment request. Xen requires
	 * that to boot.
	 */

	if (is_header_required((void *)mbp->mbp_mhp->mpp_relocatable)) {
		printf("Unsupported multiboot relocatable header\n");
		cleanup_multiboot2(mbp);
		mbp = NULL;
		goto out;
	}

out:

	if (fd != -1)
		close(fd);

	return mbp;
}

int
exec_multiboot2(const char *fname, const char *args)
{
	u_long marks[MARK_MAX];
	int fd;
	struct multiboot_package *mbp = NULL;
	twiddle_toggle = 0;

	generate_efirng();

	if ((mbp = probe_multiboot2(fname)) == NULL) {
		printf("%s is not a multiboot2 kernel\n", fname);
		goto cleanup;
	}
	
	memset(marks, 0, sizeof(marks));
	fd = loadfile(fname, marks, LOAD_KERNEL);
	if (fd < 0) {
		printf("multiboot2: %s: %s\n", fname, strerror(errno));
		goto cleanup;
	}
	close(fd);
	
#ifdef MULTIBOOT2_DEBUG
	show_marks(marks);
#endif

	mb2_load_modules(fname);

	if (efi_md_prepare_boot(fname, args, marks) != 0) {
		goto cleanup;
	}

	mbp->mbp_file = fname;
	mbp->mbp_args = args;
	mbp->mbp_marks = marks;
	
	/* Only returns on error */
	(void)start_multiboot2(mbp);
	
	/* This should not happen.. */
	printf("boot returned\n");

cleanup:
	mb2_free_modules();
	efi_md_cleanup_boot();

	return EIO;
}

