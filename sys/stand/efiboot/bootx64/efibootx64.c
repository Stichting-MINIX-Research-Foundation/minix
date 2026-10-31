/*	$NetBSD: efibootx64.c,v 1.4.6.1 2019/09/17 19:32:00 martin Exp $	*/

/*-
 * Copyright (c) 2016 Kimihiro Nonaka <nonaka@netbsd.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "../efiboot.h"
#include "../x86/efibootx86.h"

#include <sys/bootblock.h>

#include <loadfile.h>

void startprog64_start(physaddr_t, physaddr_t, physaddr_t, u_long,
    void *, physaddr_t);
extern void (*startprog64)(physaddr_t, physaddr_t, physaddr_t, u_long,
    void *, physaddr_t);
extern u_int startprog64_size;

void multiboot64_start(physaddr_t, physaddr_t, uint32_t);
extern void (*multiboot64)(physaddr_t, physaddr_t, uint32_t);
extern u_int multiboot64_size;

void
efi_md_init(void)
{
	EFI_STATUS status;
	EFI_PHYSICAL_ADDRESS addr;
	u_int sz;

	addr = EFIBOOT_ALLOCATE_MAX_ADDRESS;
	sz = EFI_SIZE_TO_PAGES(startprog64_size);
	status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateMaxAddress,
	    EfiLoaderData, sz, &addr);
	if (EFI_ERROR(status))
		panic("%s: AllocatePages() failed: %d page(s): %" PRIxMAX,
		    __func__, sz, (uintmax_t)status);
	startprog64 = (void *)addr;
	CopyMem(startprog64, startprog64_start, startprog64_size);

	addr = EFIBOOT_ALLOCATE_MAX_ADDRESS;
	sz = EFI_SIZE_TO_PAGES(multiboot64_size);
	status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateMaxAddress,
	    EfiLoaderData, sz, &addr);
	if (EFI_ERROR(status))
		panic("%s: AllocatePages() failed: %d page(s): %" PRIxMAX,
		    __func__, sz, (uintmax_t)status);
	multiboot64 = (void *)addr;
	CopyMem(multiboot64, multiboot64_start, multiboot64_size);
}

void
efi_dcache_flush(u_long start, u_long size)
{
	/* x86_64 has cache-coherent DMA; no flush needed */
}

void
efi_boot_kernel(u_long marks[MARK_MAX])
{
	physaddr_t kernel_start = marks[MARK_START] - load_offset;
	physaddr_t load_start = marks[MARK_START];
	physaddr_t kernel_entry = marks[MARK_ENTRY] - load_offset;
	u_long kernel_size = marks[MARK_END] - marks[MARK_START];
	uint32_t *newsp;

	/*
	 * The trampoline embeds its own stack; the kernel picks the
	 * arguments up from it (its entry is a call, so they end up at
	 * 4(%esp) as locore expects).
	 */
	newsp = (uint32_t *)((char *)startprog64 + startprog64_size);
	newsp -= BOOT_NARGS;
	memcpy(newsp, boot_argv, sizeof(boot_argv));

	(*startprog64)(kernel_start, load_start, (physaddr_t)newsp,
	    kernel_size, startprog64, kernel_entry);
}

void
efi_md_show(void)
{
}

int
efi_md_prepare_boot(const char *fname, const char *args, u_long *marks)
{
	return 0;
}
void
multiboot2(physaddr_t entry, physaddr_t header, uint32_t magic)
{
	(*multiboot64)(entry, header, magic);
}
