/*	$NetBSD: efibootaa64.c,v 1.7 2024/08/15 06:15:17 skrll Exp $	*/

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
#include "../efifdt.h"

#include <sys/cdefs.h>
#include <sys/bootblock.h>

#include <loadfile.h>

/* cache.S */
void aarch64_dcache_wbinv_range(vaddr_t, vsize_t);
void aarch64_icache_inv_all(void);
void aarch64_exec_kernel(paddr_t, paddr_t);

void
efi_dcache_flush(u_long start, u_long size)
{
	aarch64_dcache_wbinv_range(start, size);
}

void
efi_boot_kernel(u_long marks[MARK_MAX])
{
	u_long kernel_start, kernel_size, kernel_entry;
	u_long fdt_start, fdt_size;

	kernel_start = marks[MARK_START];
	kernel_size = marks[MARK_END] - kernel_start;
	kernel_entry = marks[MARK_ENTRY];
	fdt_start = (u_long)efi_fdt_data();
	fdt_size = efi_fdt_size();

	aarch64_dcache_wbinv_range(kernel_start, kernel_size);
	if (efi_fdt_size() > 0) {
		aarch64_dcache_wbinv_range(fdt_start, fdt_size);
	}
	aarch64_icache_inv_all();

	aarch64_exec_kernel((paddr_t)kernel_entry, (paddr_t)fdt_start);
}

/*
 * Returns the current exception level.
 */
static u_int
efi_aarch64_current_el(void)
{
	uint64_t el;
	__asm __volatile ("mrs %0, CurrentEL" : "=r" (el));
	return (el >> 2) & 0x3;
}

static bool
efi_aarch64_BigEnd(void)
{
	uint64_t id_aa64mmfr0_el1;
	__asm __volatile ("mrs %[mmfr0], id_aa64mmfr0_el1"
            : [mmfr0] "=r" (id_aa64mmfr0_el1) :: "memory");
	return __SHIFTOUT(id_aa64mmfr0_el1, __BITS(11, 8)) == 1;
}

void
efi_md_show(void)
{
	command_printtab("CurrentEL", "EL%u\n", efi_aarch64_current_el());
}

int
efi_md_prepare_boot(const char *fname, const char *args, u_long *marks)
{
	if (netbsd_elf_data == ELFDATA2MSB) {
		if (!efi_aarch64_BigEnd()) {
			printf("Processor does not support big endian at EL1\n");
			return ENOTSUP;
		}
	}
	return 0;
}
