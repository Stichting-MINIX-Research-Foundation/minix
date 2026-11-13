/* $NetBSD: efibootriscv64.c,v 1.2 2024/08/15 06:15:17 skrll Exp $ */

/*-
 * Copyright (c) 2021 Jared McNeill <jmcneill@invisible.ca>
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

#include <sys/bootblock.h>

#include <loadfile.h>
#include <libfdt.h>

typedef void (*riscv_kernel_entry_t)(register_t, register_t);

static uint32_t
efi_fdt_get_boot_hartid(void)
{
	const int chosen = fdt_path_offset(efi_fdt_data(), "/chosen");
	const uint32_t *data;

	data = fdt_getprop(efi_fdt_data(), chosen, "boot-hartid", NULL);
	if (data == NULL) {
		return 0;	/* XXX */
	}

	return fdt32_to_cpu(*data);
}

void
efi_boot_kernel(u_long marks[MARK_MAX])
{
	riscv_kernel_entry_t entry_fn;
	register_t hart_id; /* a0 */
	register_t fdt_start; /* a1 */

	entry_fn = (riscv_kernel_entry_t)(uintptr_t)marks[MARK_ENTRY];

	hart_id = efi_fdt_get_boot_hartid();
	fdt_start = (register_t)efi_fdt_data();

	asm volatile("fence rw,rw; fence.i" ::: "memory");

	entry_fn(hart_id, fdt_start);
}

void
efi_md_show(void)
{
}

void
efi_dcache_flush(u_long start, u_long size)
{
}

int
efi_md_prepare_boot(const char *fname, const char *args, u_long *marks)
{
	return 0;
}
