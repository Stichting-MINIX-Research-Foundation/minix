/* $NetBSD: efifdt.h,v 1.14 2024/08/15 15:57:36 riastradh Exp $ */

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

#if !defined(EFIBOOT_FDT)
#error FDT support not enabled
#endif

int efi_fdt_probe(void);
void efi_fdt_memory_map(void);
void efi_fdt_gop(void);
int efi_fdt_set_data(void *);
void *efi_fdt_data(void);
int efi_fdt_size(void);
bool efi_fdt_overlay_is_compatible(void *);
int efi_fdt_overlay_apply(void *, int *);
void efi_fdt_show(void);
void efi_fdt_bootargs(const char *);
void efi_fdt_initrd(u_long, u_long);
void efi_fdt_rndseed(u_long, u_long);
void efi_fdt_efirng(u_long, u_long);
void efi_fdt_module(const char *, u_long, u_long);
void efi_fdt_module_foreach(void (*fn)(const char *, const uint64_t, const uint64_t, void *, void *), void *, void *);
void efi_fdt_userconf(void);
void efi_fdt_init(u_long, u_long);
void efi_fdt_fini(void);
void efi_fdt_system_table(void);

int efi_fdt_prepare_boot(const char *, const char *, u_long *);
void efi_fdt_cleanup_boot(void);
size_t efi_fdt_alloc_size(void);
void efi_fdt_set_virtual_address_map(EFI_MEMORY_DESCRIPTOR *, UINTN, UINTN,
    UINTN, UINT32);
