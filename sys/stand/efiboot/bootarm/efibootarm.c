/* $NetBSD: efibootarm.c,v 1.5 2024/09/18 19:43:29 skrll Exp $ */

/*-
 * Copyright (c) 2019 Jared McNeill <jmcneill@invisible.ca>
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

/* cache.S */
void armv7_dcache_wbinv_range(vaddr_t, vsize_t);
void armv7_dcache_wbinv_all(void);
void armv7_icache_inv_all(void);
void armv7_exec_kernel(register_t, register_t);

void
efi_dcache_flush(u_long start, u_long size)
{
	armv7_dcache_wbinv_range(start, size);
}

void
efi_boot_kernel(u_long marks[MARK_MAX])
{
	armv7_dcache_wbinv_all();
	armv7_icache_inv_all();

	armv7_exec_kernel((register_t)marks[MARK_ENTRY], (register_t)efi_fdt_data());
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
