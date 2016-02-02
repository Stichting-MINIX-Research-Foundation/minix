/*	$NetBSD: gelf_sym.c,v 1.2 2014/03/09 16:58:04 christos Exp $	*/

/*-
 * Copyright (c) 2006,2008 Joseph Koshy
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if HAVE_NBTOOL_CONFIG_H
# include "nbtool_config.h"
#endif

#include <sys/cdefs.h>

#include <assert.h>
#include <gelf.h>
#include <limits.h>

#include "_libelf.h"

__RCSID("$NetBSD: gelf_sym.c,v 1.2 2014/03/09 16:58:04 christos Exp $");
ELFTC_VCSID("Id: gelf_sym.c 2272 2011-12-03 17:07:31Z jkoshy ");

GElf_Sym *
gelf_getsym(Elf_Data *ed, int ndx, GElf_Sym *dst)
{
	int ec;
	Elf *e;
	size_t msz;
	Elf_Scn *scn;
	uint32_t sh_type;
	Elf32_Sym *sym32;
	Elf64_Sym *sym64;
	struct _Libelf_Data *d;

	d = (struct _Libelf_Data *) ed;

	if (d == NULL || ndx < 0 || dst == NULL ||
	    (scn = d->d_scn) == NULL ||
	    (e = scn->s_elf) == NULL) {
		LIBELF_SET_ERROR(ARGUMENT, 0);
		return (NULL);
	}

	ec = e->e_class;
	assert(ec == ELFCLASS32 || ec == ELFCLASS64);

	if (ec == ELFCLASS32)
		sh_type = scn->s_shdr.s_shdr32.sh_type;
	else
		sh_type = scn->s_shdr.s_shdr64.sh_type;

	if (_libelf_xlate_shtype(sh_type) != ELF_T_SYM) {
		LIBELF_SET_ERROR(ARGUMENT, 0);
		return (NULL);
	}

	msz = _libelf_msize(ELF_T_SYM, ec, e->e_version);

	assert(msz > 0);

	if (msz * ndx >= d->d_data.d_size) {
		LIBELF_SET_ERROR(ARGUMENT, 0);
		return (NULL);
	}

	if (ec == ELFCLASS32) {

		sym32 = (Elf32_Sym *) d->d_data.d_buf + ndx;

		dst->st_name  = sym32->st_name;
		dst->st_value = (Elf64_Addr) sym32->st_value;
		dst->st_size  = (Elf64_Xword) sym32->st_size;
		dst->st_info  = ELF64_ST_INFO(ELF32_ST_BIND(sym32->st_info),
		    ELF32_ST_TYPE(sym32->st_info));
		dst->st_other = sym32->st_other;
		dst->st_shndx = sym32->st_shndx;
	} else {

		sym64 = (Elf64_Sym *) d->d_data.d_buf + ndx;

		*dst = *sym64;
	}

	return (dst);
}

int
gelf_update_sym(Elf_Data *ed, int ndx, GElf_Sym *gs)
{
	int ec;
	Elf *e;
	size_t msz;
	Elf_Scn *scn;
	uint32_t sh_type;
	Elf32_Sym *sym32;
	Elf64_Sym *sym64;
	struct _Libelf_Data *d;

	d = (struct _Libelf_Data *) ed;

	if (d == NULL || ndx < 0 || gs == NULL ||
	    (scn = d->d_scn) == NULL ||
	    (e = scn->s_elf) == NULL) {
		LIBELF_SET_ERROR(ARGUMENT, 0);
		return (0);
	}

	ec = e->e_class;
	assert(ec == ELFCLASS32 || ec == ELFCLASS64);

	if (ec == ELFCLASS32)
		sh_type = scn->s_shdr.s_shdr32.sh_type;
	else
		sh_type = scn->s_shdr.s_shdr64.sh_type;

	if (_libelf_xlate_shtype(sh_type) != ELF_T_SYM) {
		LIBELF_SET_ERROR(ARGUMENT, 0);
		return (0);
	}

	msz = _libelf_msize(ELF_T_SYM, ec, e->e_version);
	assert(msz > 0);

	if (msz * ndx >= d->d_data.d_size) {
		LIBELF_SET_ERROR(ARGUMENT, 0);
		return (0);
	}

	if (ec == ELFCLASS32) {
		sym32 = (Elf32_Sym *) d->d_data.d_buf + ndx;

		sym32->st_name  = gs->st_name;
		sym32->st_info  = gs->st_info;
		sym32->st_other = gs->st_other;
		sym32->st_shndx = gs->st_shndx;

		LIBELF_COPY_U32(sym32, gs, st_value);
		LIBELF_COPY_U32(sym32, gs, st_size);
	} else {
		sym64 = (Elf64_Sym *) d->d_data.d_buf + ndx;

		*sym64 = *gs;
	}

	return (1);
}
