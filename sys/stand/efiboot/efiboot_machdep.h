/* $NetBSD: efiboot_machdep.h,v 1.5 2024/08/15 06:15:16 skrll Exp $ */

/*-
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

#ifndef EFIBOOT_ALIGN
#define	EFIBOOT_ALIGN 0
#endif

typedef unsigned long physaddr_t;

void efi_dcache_flush(u_long, u_long);
void efi_boot_kernel(u_long[MARK_MAX]);
void efi_md_show(void);
void efi_md_init(void);
int efi_md_prepare_boot(const char *, const char *, u_long *);

/*
 * Hooks for the native NetBSD boot path (exec.c).  On success,
 * efi_md_prepare_netbsd has called efi_cleanup() (ExitBootServices);
 * efi_boot_kernel then only transfers control.
 */
size_t efi_md_boot_alloc_size(const char *);
int efi_md_prepare_netbsd(const char *, const char *, u_long *);
void efi_md_cleanup_boot(void);

/* multiboot2 */

void multiboot2(physaddr_t, physaddr_t, uint32_t);
int exec_multiboot2(const char *, const char *);
void *probe_multiboot2(const char *);
