/* $NetBSD: exec.c,v 1.26 2024/09/19 06:26:11 mlelstv Exp $ */

/*-
 * Copyright (c) 2019 Jason R. Thorpe
 * Copyright (c) 2018 Jared McNeill <jmcneill@invisible.ca>
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

#include "efiboot.h"
#ifdef EFIBOOT_FDT
#include "efifdt.h"
#endif
#ifdef EFIBOOT_ACPI
#include "efiacpi.h"
#endif
#include "efirng.h"
#include "module.h"

#include <sys/param.h>
#include <sys/reboot.h>

extern char twiddle_toggle;

u_long load_offset = 0;

EFI_PHYSICAL_ADDRESS efirng_addr;
u_long efirng_size = 0;

int
load_file(const char *path, u_long extra, bool quiet_errors,
    EFI_PHYSICAL_ADDRESS *paddr, u_long *psize)
{
	EFI_STATUS status;
	struct stat st;
	ssize_t len;
	ssize_t expectedlen;
	int fd;

	if (strlen(path) == 0)
		return 0;

	fd = open(path, 0);
	if (fd < 0) {
		if (!quiet_errors) {
			printf("boot: failed to open %s: %s\n", path,
			    strerror(errno));
		}
		return errno;
	}
	if (fstat(fd, &st) < 0) {
		printf("boot: failed to fstat %s: %s\n", path, strerror(errno));
		close(fd);
		return errno;
	}
	if (st.st_size == 0) {
		if (!quiet_errors) {
			printf("boot: empty file %s\n", path);
		}
		close(fd);
		return EINVAL;
	}

	expectedlen = st.st_size;
	*psize = st.st_size + extra;

#ifdef EFIBOOT_ALLOCATE_MAX_ADDRESS
	*paddr = EFIBOOT_ALLOCATE_MAX_ADDRESS;
	status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateMaxAddress, EfiLoaderData,
	    EFI_SIZE_TO_PAGES(*psize), paddr);
#else
	*paddr = 0;
	status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, EfiLoaderData,
	    EFI_SIZE_TO_PAGES(*psize), paddr);
#endif
	if (EFI_ERROR(status)) {
		printf("Failed to allocate %lu bytes for %s (error %lu)\n",
		    *psize, path, (u_long)status);
		close(fd);
		*paddr = 0;
		return ENOMEM;
	}

	printf("boot: loading %s ", path);
	len = read(fd, (void *)(uintptr_t)*paddr, expectedlen);
	close(fd);

	if (len != expectedlen) {
		if (len < 0) {
			printf(": %s\n", strerror(errno));
		} else {
			printf(": returned %zd (expected %zd)\n", len,
			    expectedlen);
		}
		return EIO;
	}

	printf("done.\n");

	efi_dcache_flush(*paddr, *psize);

	return 0;
}

void
generate_efirng(void)
{
	EFI_PHYSICAL_ADDRESS addr;
	u_long size = EFI_PAGE_SIZE;
	EFI_STATUS status;

	/* Check whether the RNG is available before bothering.  */
	if (!efi_rng_available())
		return;

	/*
	 * Allocate a page.  This is the smallest unit we can pass into
	 * the kernel conveniently.
	 */
#ifdef EFIBOOT_ALLOCATE_MAX_ADDRESS
	addr = EFIBOOT_ALLOCATE_MAX_ADDRESS;
	status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateMaxAddress,
	    EfiLoaderData, EFI_SIZE_TO_PAGES(size), &addr);
#else
	addr = 0;
	status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages,
	    EfiLoaderData, EFI_SIZE_TO_PAGES(size), &addr);
#endif
	if (EFI_ERROR(status)) {
		Print(L"Failed to allocate page for EFI RNG output: %r\n",
		    status);
		return;
	}

	/* Fill the page with whatever the EFI RNG will do.  */
	if (efi_rng((void *)(uintptr_t)addr, size)) {
		uefi_call_wrapper(BS->FreePages, 2, addr,
		    EFI_SIZE_TO_PAGES(size));
		return;
	}

	/* Success!  */
	efirng_addr = addr;
	efirng_size = size;
}

int
exec_netbsd(const char *fname, const char *args)
{
	EFI_PHYSICAL_ADDRESS addr;
	u_long marks[MARK_MAX], alloc_size, kernel_base;
	EFI_STATUS status;
	int fd, ohowto;

	twiddle_toggle = 0;

	generate_efirng();

	memset(marks, 0, sizeof(marks));
	ohowto = howto;
	howto |= AB_SILENT;
	fd = loadfile(fname, marks, COUNT_KERNEL | LOAD_NOTE);
	howto = ohowto;
	if (fd < 0) {
		printf("boot: %s: %s\n", fname, strerror(errno));
		return EIO;
	}
	close(fd);
	marks[MARK_END] = (((u_long) marks[MARK_END] + sizeof(int) - 1)) & -sizeof(int);
	alloc_size = marks[MARK_END] - marks[MARK_START] + efi_md_boot_alloc_size(fname) + EFIBOOT_ALIGN;
	kernel_base = marks[MARK_START];

#ifdef EFIBOOT_ALLOCATE_MAX_ADDRESS
	addr = EFIBOOT_ALLOCATE_MAX_ADDRESS;
	status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateMaxAddress, EfiLoaderData,
	    EFI_SIZE_TO_PAGES(alloc_size), &addr);
#else
	addr = 0;
	status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, EfiLoaderData,
	    EFI_SIZE_TO_PAGES(alloc_size), &addr);
#endif
	if (EFI_ERROR(status)) {
		printf("Failed to allocate %lu bytes for kernel image (error %lu)\n",
		    alloc_size, (u_long)status);
		return ENOMEM;
	}

	/*
	 * Load into the allocated buffer.  LOADADDR maps the kernel's
	 * link address to its physical load address; kernel_base is that
	 * physical base (0 on the FDT platforms by choice of address
	 * mask), so the image starts right at the aligned buffer.
	 * load_offset stays valid until cleanup: the MD code needs it to
	 * translate marks[] back to kernel physical addresses.
	 */
	memset(marks, 0, sizeof(marks));
	load_offset = ((addr + EFIBOOT_ALIGN - 1) & -EFIBOOT_ALIGN) - kernel_base;
	fd = loadfile(fname, marks, LOAD_KERNEL);
	if (fd < 0) {
		printf("boot: %s: %s\n", fname, strerror(errno));
		goto cleanup;
	}
	close(fd);

	if (efi_md_prepare_netbsd(fname, args, marks) != 0) {
		goto cleanup;
	}

	efi_boot_kernel(marks);

	/* This should not happen.. */
	printf("boot returned\n");

cleanup:
	load_offset = 0;
	uefi_call_wrapper(BS->FreePages, 2, addr, EFI_SIZE_TO_PAGES(alloc_size));
	efi_md_cleanup_boot();

	return EIO;
}
