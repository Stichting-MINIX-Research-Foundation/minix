/*	$NetBSD: kobj_machdep.c,v 1.7 2009/01/08 01:03:24 pooka Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software developed for The NetBSD Foundation
 * by Andrew Doran.
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

/*-
 * Copyright 1996-1998 John D. Polstra.
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
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kobj_machdep.c,v 1.7 2009/01/08 01:03:24 pooka Exp $");

#define	ELFSIZE		ARCH_ELFSIZE

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kobj.h>
#include <sys/exec.h>
#include <sys/exec_elf.h>
#include <sys/xcall.h>

#include <machine/cpufunc.h>

int
kobj_reloc(kobj_t ko, uintptr_t relocbase, const void *data,
	   bool isrela, bool local)
{
	Elf_Addr *where;
	Elf_Addr addr;
	Elf_Addr addend;
	Elf_Word rtype, symidx;
	const Elf_Rel *rel;
	const Elf_Rela *rela;

	if (isrela) {
		rela = (const Elf_Rela *)data;
		where = (Elf_Addr *) (relocbase + rela->r_offset);
		addend = rela->r_addend;
		rtype = ELF_R_TYPE(rela->r_info);
		symidx = ELF_R_SYM(rela->r_info);
	} else {
		rel = (const Elf_Rel *)data;
		where = (Elf_Addr *) (relocbase + rel->r_offset);
		addend = *where;
		rtype = ELF_R_TYPE(rel->r_info);
		symidx = ELF_R_SYM(rel->r_info);
	}

	switch (rtype) {
	case R_386_NONE:	/* none */
		return 0;

	case R_386_32:		/* S + A */
		addr = kobj_sym_lookup(ko, symidx);
		if (addr == 0)
			return -1;
		addr += addend;
		break;

	case R_386_PC32:	/* S + A - P */
		addr = kobj_sym_lookup(ko, symidx);
		if (addr == 0)
			return -1;
		addr += addend - (Elf_Addr)where;
		break;

	case R_386_GLOB_DAT:	/* S */
		addr = kobj_sym_lookup(ko, symidx);
		if (addr == 0)
			return -1;
		break;

	case R_386_RELATIVE:
		addr = relocbase + addend;
		break;

	default:
		printf("kobj_reloc: unexpected relocation type %d\n", rtype);
		return -1;
	}

	*where = addr;
	return 0;
}

int
kobj_machdep(kobj_t ko, void *base, size_t size, bool load)
{
	uint64_t where;

	/*
	 * Currently we want this to invalidate the Pentium 4 trace cache.
	 * Other caches are snoopably coherent.
	 */
	if (load) {
		if (cold) {
			wbinvd();
		} else {
			where = xc_broadcast(0, (xcfunc_t)wbinvd, NULL, NULL);
			xc_wait(where);
		}
	}

	return 0;
}
