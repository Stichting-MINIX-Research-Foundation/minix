/* $NetBSD: lookup_elf32.c,v 1.3 2010/02/11 21:28:16 martin Exp $ */

/*-
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Martin Husemann.
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

/* If not included by lookup_elf64.c, ELFSIZE won't be defined. */
#ifndef ELFSIZE
#define	ELFSIZE	32
#endif

#include <lib/libkern/libkern.h>
#include <sys/param.h>
#include <sys/exec_elf.h>

#if ((ELFSIZE == 32) && defined(BOOT_ELF32)) || \
    ((ELFSIZE == 64) && defined(BOOT_ELF64))

void * ELFNAMEEND(lookup_symbol)(const char *, void *, void *);

void *
ELFNAMEEND(lookup_symbol)(const char *symname, void *sstab, void *estab)
{
	Elf_Ehdr *elf;
	Elf_Shdr *shp;
	Elf_Sym *symtab_start, *symtab_end, *sp;
	char *strtab_start, *strtab_end;
	int i, j;

	elf = sstab;
	if (elf->e_shoff == 0)
		return NULL;

	switch (elf->e_machine) {
	ELFDEFNNAME(MACHDEP_ID_CASES)
	default:
		return NULL;
	}

	symtab_start = symtab_end = NULL;
	strtab_start = strtab_end = NULL;

	shp = (Elf_Shdr *)((char *)sstab + elf->e_shoff);
	for (i = 0; i < elf->e_shnum; i++) {
		if (shp[i].sh_type != SHT_SYMTAB)
			continue;
		if (shp[i].sh_offset == 0)
			continue;
		symtab_start = (Elf_Sym*)((char*)sstab + shp[i].sh_offset);
		symtab_end = (Elf_Sym*)((char*)sstab + shp[i].sh_offset
			+ shp[i].sh_size);
		j = shp[i].sh_link;
		if (shp[j].sh_offset == 0)
			continue;
		strtab_start = (char*)sstab + shp[j].sh_offset;
		strtab_end = (char*)sstab + shp[j].sh_offset + shp[j].sh_size;
		break;
	}

	if (!symtab_start || !strtab_start)
		return NULL;

	for (sp = symtab_start; sp < symtab_end; sp++)
		if (sp->st_name != 0 &&
		    strcmp(strtab_start + sp->st_name, symname) == 0)
			return (void*)(uintptr_t)sp->st_value;

	return NULL;
}

#endif /* (ELFSIZE == 32 && BOOT_ELF32) || (ELFSIZE == 64 && BOOT_ELF64) */
