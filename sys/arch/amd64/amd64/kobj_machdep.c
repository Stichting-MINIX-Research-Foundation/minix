/*	$NetBSD: kobj_machdep.c,v 1.8 2019/05/28 03:52:08 kamil Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: kobj_machdep.c,v 1.8 2019/05/28 03:52:08 kamil Exp $");

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
	Elf64_Addr *where, val;
	Elf32_Addr *where32, val32;
	Elf64_Addr addr;
	Elf64_Addr addend;
	uintptr_t rtype, symidx;
	const Elf_Rel *rel;
	const Elf_Rela *rela;
	int error;

	if (isrela) {
		rela = (const Elf_Rela *)data;
		where = (Elf64_Addr *)(relocbase + rela->r_offset);
		addend = rela->r_addend;
		rtype = ELF_R_TYPE(rela->r_info);
		symidx = ELF_R_SYM(rela->r_info);
	} else {
		rel = (const Elf_Rel *)data;
		where = (Elf64_Addr *)(relocbase + rel->r_offset);
		rtype = ELF_R_TYPE(rel->r_info);
		symidx = ELF_R_SYM(rel->r_info);
		/* Addend is 32 bit on 32 bit relocs */
		switch (rtype) {
		case R_X86_64_PC32:
		case R_X86_64_32:
		case R_X86_64_32S:
			addend = *(Elf32_Addr *)where;
			break;
		default:
			addend = *where;
			break;
		}
	}

	switch (rtype) {
	case R_X86_64_NONE:	/* none */
		break;

	case R_X86_64_64:		/* S + A */
		error = kobj_sym_lookup(ko, symidx, &addr);
		if (error)
			return -1;
		val = addr + addend;
		memcpy(where, &val, sizeof(val));
		break;

	case R_X86_64_PC32:	/* S + A - P */
	case R_X86_64_PLT32:
		error = kobj_sym_lookup(ko, symidx, &addr);
		if (error)
			return -1;
		where32 = (Elf32_Addr *)where;
		val32 = (Elf32_Addr)(addr + addend - (Elf64_Addr)where);
		memcpy(where32, &val32, sizeof(val32));
		break;

	case R_X86_64_32:	/* S + A */
	case R_X86_64_32S:	/* S + A sign extend */
		error = kobj_sym_lookup(ko, symidx, &addr);
		if (error)
			return -1;
		val32 = (Elf32_Addr)(addr + addend);
		where32 = (Elf32_Addr *)where;
		memcpy(where32, &val32, sizeof(val32));
		break;

	case R_X86_64_GLOB_DAT:	/* S */
	case R_X86_64_JUMP_SLOT:/* XXX need addend + offset */
		error = kobj_sym_lookup(ko, symidx, &addr);
		if (error)
			return -1;
		memcpy(where, &addr, sizeof(addr));
		break;

	case R_X86_64_RELATIVE:	/* B + A */
		addr = relocbase + addend;
		val = addr;
		memcpy(where, &val, sizeof(val));
		break;

	default:
		printf("kobj_reloc: unexpected relocation type %ld\n", rtype);
		return -1;
	}

	return 0;
}

int
kobj_machdep(kobj_t ko, void *base, size_t size, bool load)
{
	uint64_t where;

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
