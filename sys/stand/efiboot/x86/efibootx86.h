/*	$NetBSD$	*/

/*
 * Copyright (c) 1997
 *	Matthias Drochner.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * x86 (ia32/x64) machine dependent layer: NetBSD bootinfo and
 * EFI memory map helpers.  Include after efiboot.h.
 */

#include <machine/bootinfo.h>

/* bootinfo.c */

struct bootinfo {
	uint32_t nentries;
	uint32_t entry[1];
};

extern struct bootinfo *bootinfo;

#define BTINFO_MAX	64

#define BI_ALLOC(max) (bootinfo = alloc(sizeof(struct bootinfo) \
                                        + ((max) - 1) * sizeof(uint32_t))) \
                      ->nentries = 0

#define BI_FREE() dealloc(bootinfo, 0)

#define BI_ADD(x, type, size) bi_add((struct btinfo_common *)(x), type, size)

void bi_add(struct btinfo_common *, int, int);

/*
 * Kernel entry arguments: howto, bootdev (obsolete), bootinfo pa,
 * esym, extmem, basemem; see the "Load parameters" block in locore.S.
 * Filled by efi_md_prepare_netbsd, consumed by efi_boot_kernel.
 */
#define BOOT_NARGS	6
extern uint32_t boot_argv[BOOT_NARGS];

/* efibootx86.c */

void command_consdev(char *);
void command_memmap(char *);
void command_root(char *);

/* efimemory.c */

physaddr_t vtophys(void *);
EFI_MEMORY_DESCRIPTOR *efi_memory_get_map(UINTN *, UINTN *, UINTN *,
    UINT32 *, bool);
EFI_MEMORY_DESCRIPTOR *efi_memory_compact_map(EFI_MEMORY_DESCRIPTOR *,
    UINTN *, UINTN);
void efi_memory_show_map(bool, bool);
int getbasemem(void);
int getextmemx(void);
