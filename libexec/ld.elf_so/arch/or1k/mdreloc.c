/*	$NetBSD: mdreloc.c,v 1.1 2014/09/03 19:34:26 matt Exp $	*/

/*-
 * Copyright (c) 2014 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas of 3am Software Foundry.
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

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: mdreloc.c,v 1.1 2014/09/03 19:34:26 matt Exp $");
#endif /* not lint */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <machine/cpu.h>

#include "debug.h"
#include "rtld.h"

void _rtld_bind_start(void);
Elf_Addr _rtld_bind(const Obj_Entry *, Elf_Word);
void _rtld_relocate_nonplt_self(Elf_Dyn *, Elf_Addr);
static int _rtld_relocate_plt_object(const Obj_Entry *,
    const Elf_Rela *, int, Elf_Addr *);

/*
 * The OR1k PLT format is simple.
 */

void
_rtld_setup_pltgot(const Obj_Entry *obj)
{
	obj->pltgot[1] = (Elf_Addr) obj;
	obj->pltgot[2] = (Elf_Addr) _rtld_bind_start;

	dbg(("obj %s plt pltgot=%p start=%p obj=%p",
	    obj->path, obj->pltgot,
	    (void *) obj->pltgot[1], (void *) obj->pltgot[2]));
}

void
_rtld_relocate_nonplt_self(Elf_Dyn *dynp, Elf_Addr relocbase)
{
	const Elf_Rela *rela = 0, *relalim;
	Elf_Addr relasz = 0;
	Elf_Addr *where;

	for (; dynp->d_tag != DT_NULL; dynp++) {
		switch (dynp->d_tag) {
		case DT_RELA:
			rela = (const Elf_Rela *)(relocbase + dynp->d_un.d_ptr);
			break;
		case DT_RELASZ:
			relasz = dynp->d_un.d_val;
			break;
		}
	}
	relalim = (const Elf_Rela *)((const uint8_t *)rela + relasz);
	for (; rela < relalim; rela++) {
		where = (Elf_Addr *)(relocbase + rela->r_offset);
		*where = (Elf_Addr)(relocbase + rela->r_addend);
	}
}

int
_rtld_relocate_nonplt_objects(Obj_Entry *obj)
{
	const Elf_Rela *rela;

	for (rela = obj->rela; rela < obj->relalim; rela++) {
		Elf_Addr        *where;
		const Elf_Sym   *def;
		const Obj_Entry *defobj;
		Elf_Addr         tmp;
		unsigned long	 symnum;

		where = (Elf_Addr *)(obj->relocbase + rela->r_offset);
		symnum = ELF_R_SYM(rela->r_info);

		switch (ELF_R_TYPE(rela->r_info)) {
#if 1 /* XXX Should not be necessary. */
		case R_TYPE(JMP_SLOT):
#endif
		case R_TYPE(NONE):
			break;

		case R_TYPE(32):	/* <address> S + A */
		case R_TYPE(GLOB_DAT):	/* <address> S + A */
			def = _rtld_find_symdef(symnum, obj, &defobj, false);
			if (def == NULL)
				return -1;

			tmp = (Elf_Addr)(defobj->relocbase + def->st_value +
			    rela->r_addend);
			if (*where != tmp)
				*where = tmp;
			rdbg(("32/GLOB_DAT %s in %s --> %p in %s",
			    obj->strtab + obj->symtab[symnum].st_name,
			    obj->path, (void *)*where, defobj->path));
			break;

		case R_TYPE(RELATIVE):	/* <address> B + A */
			*where = (Elf_Addr)(obj->relocbase + rela->r_addend);
			rdbg(("RELATIVE in %s --> %p", obj->path,
			    (void *)*where));
			break;

		case R_TYPE(COPY):
			/*
			 * These are deferred until all other relocations have
			 * been done.  All we do here is make sure that the
			 * COPY relocation is not in a shared library.  They
			 * are allowed only in executable files.
			 */
			if (obj->isdynamic) {
				_rtld_error(
			"%s: Unexpected R_COPY relocation in shared library",
				    obj->path);
				return -1;
			}
			rdbg(("COPY (avoid in main)"));
			break;

		case R_TYPE(TLS_DTPMOD):
			def = _rtld_find_symdef(symnum, obj, &defobj, false);
			if (def == NULL)
				return -1;

			*where = (Elf_Addr)defobj->tlsindex;
			rdbg(("DTPMOD32 %s in %s --> %p in %s",
			    obj->strtab + obj->symtab[symnum].st_name,
			    obj->path, (void *)*where, defobj->path));
			break;

		case R_TYPE(TLS_DTPOFF):
			def = _rtld_find_symdef(symnum, obj, &defobj, false);
			if (def == NULL)
				return -1;

			if (!defobj->tls_done && _rtld_tls_offset_allocate(obj))
				return -1;

			*where = (Elf_Addr)(def->st_value + rela->r_addend
			    - TLS_DTV_OFFSET);
			rdbg(("DTPOFF %s in %s --> %p in %s",
			    obj->strtab + obj->symtab[symnum].st_name,
			    obj->path, (void *)*where, defobj->path));
			break;

		case R_TYPE(TLS_TPOFF):
			def = _rtld_find_symdef(symnum, obj, &defobj, false);
			if (def == NULL)
				return -1;

			if (!defobj->tls_done && _rtld_tls_offset_allocate(obj))
				return -1;

			*where = (Elf_Addr)(def->st_value + rela->r_addend
			    + defobj->tlsoffset - TLS_TP_OFFSET);
			rdbg(("TPREL %s in %s --> %p in %s",
			    obj->strtab + obj->symtab[symnum].st_name,
			    obj->path, (void *)*where, defobj->path));
			break;

		default:
			rdbg(("sym = %lu, type = %lu, offset = %p, "
			    "addend = %p, contents = %p, symbol = %s",
			    symnum, (u_long)ELF_R_TYPE(rela->r_info),
			    (void *)rela->r_offset, (void *)rela->r_addend,
			    (void *)*where,
			    obj->strtab + obj->symtab[symnum].st_name));
			_rtld_error("%s: Unsupported relocation type %ld "
			    "in non-PLT relocations",
			    obj->path, (u_long) ELF_R_TYPE(rela->r_info));
			return -1;
		}
	}
	return 0;
}

int
_rtld_relocate_plt_lazy(const Obj_Entry *obj)
{
	const Elf_Rela *rela;
	int reloff;

	for (rela = obj->pltrela, reloff = 0;
	     rela < obj->pltrelalim;
	     rela++, reloff++) {
		Elf_Word *where = (Elf_Word *)(obj->relocbase + rela->r_offset);

		assert(ELF_R_TYPE(rela->r_info) == R_TYPE(JMP_SLOT));

		/*
		 * For now, simply treat then as relative.
		 */
		*where += (Elf_Addr)obj->relocbase;
	}

	return 0;
}

static int
_rtld_relocate_plt_object(const Obj_Entry *obj, const Elf_Rela *rela, int reloff, Elf_Addr *tp)
{
	Elf_Word *where = (Elf_Word *)(obj->relocbase + rela->r_offset);
	Elf_Addr value;
	const Elf_Sym *def;
	const Obj_Entry *defobj;
	unsigned long info = rela->r_info;

	assert(ELF_R_TYPE(info) == R_TYPE(JMP_SLOT));

	def = _rtld_find_plt_symdef(ELF_R_SYM(info), obj, &defobj, tp != NULL);
	if (__predict_false(def == NULL))
		return -1;
	if (__predict_false(def == &_rtld_sym_zero))
		return 0;

	value = (Elf_Addr)(defobj->relocbase + def->st_value);
	rdbg(("bind now/fixup in %s --> new=%p", 
	    defobj->strtab + def->st_name, (void *)value));

	/*
	 * Simply replace the entry in GOT with the
	 * address of the routine.
	 */
	assert(where >= (Elf_Word *)obj->pltgot);
	assert(where < (Elf_Word *)obj->pltgot + (obj->pltrelalim - obj->pltrela));
	*where = value;

	if (tp)
		*tp = value;
	return 0;
}

Elf_Addr
_rtld_bind(const Obj_Entry *obj, Elf_Word reloff)
{
	const Elf_Rela *rela = obj->pltrela + reloff;
	Elf_Addr new_value;
	int err;

	new_value = 0;	/* XXX gcc */

	_rtld_shared_enter();
	err = _rtld_relocate_plt_object(obj, rela, reloff, &new_value); 
	if (err)
		_rtld_die();
	_rtld_shared_exit();

	return new_value;
}

int
_rtld_relocate_plt_objects(const Obj_Entry *obj)
{
	const Elf_Rela *rela;
	int reloff;
	
	for (rela = obj->pltrela, reloff = 0; rela < obj->pltrelalim; rela++, reloff++) {
		if (_rtld_relocate_plt_object(obj, rela, reloff, NULL) < 0)
			return -1;
	}
	return 0;
}
