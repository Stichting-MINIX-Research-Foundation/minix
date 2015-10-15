/*	$NetBSD: mdreloc.c,v 1.2 2015/03/27 23:14:53 matt Exp $	*/

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
__RCSID("$NetBSD: mdreloc.c,v 1.2 2015/03/27 23:14:53 matt Exp $");
#endif /* not lint */

#include <sys/types.h>
#include <sys/endian.h>
#include <sys/tls.h>

#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "rtld.h"

void _rtld_bind_start(void);
void _rtld_relocate_nonplt_self(Elf_Dyn *, Elf_Addr);
void *_rtld_bind(const Obj_Entry *, Elf_Word);

void
_rtld_setup_pltgot(const Obj_Entry *obj)
{
	obj->pltgot[0] = (Elf_Addr) &_rtld_bind_start;
	obj->pltgot[1] = (Elf_Addr) obj;
}

void
_rtld_relocate_nonplt_self(Elf_Dyn *dynp, Elf_Addr relocbase)
{
	const Elf_Rela *rela = NULL, *relalim;
	Elf_Addr relasz = 0;

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

	relalim = (const Elf_Rela *)((uintptr_t)rela + relasz);
	for (; rela < relalim; rela++) {
		Elf_Word r_type = ELF_R_TYPE(rela->r_info);
		Elf_Addr *where = (Elf_Addr *)(relocbase + rela->r_offset);

		switch (r_type) {
		case R_TYPE(RELATIVE): {
			Elf_Addr val = relocbase + rela->r_addend;
			*where = val;
			rdbg(("RELATIVE/L(%p) -> %p in <self>",
			    where, (void *)val));
			break;
		}

		case R_TYPE(NONE):
			break;

		default:
			abort();
		}
	}
}

int
_rtld_relocate_nonplt_objects(Obj_Entry *obj)
{
	const Elf_Rela *rela;
	const Elf_Sym *def;
	const Obj_Entry *defobj;

	for (rela = obj->rela; rela < obj->relalim; rela++) {
		Elf_Addr * const where =
		    (Elf_Addr *)(obj->relocbase + rela->r_offset);
		const Elf_Word r_symndx = ELF_R_SYM(rela->r_info);
		const Elf_Word r_type = ELF_R_TYPE(rela->r_info);

		switch (r_type) {
		case R_TYPE(NONE):
			break;

		case R_TYPE(RELATIVE): {
			def = obj->symtab + r_symndx;

			Elf_Addr val = (Elf_Addr)obj->relocbase + rela->r_addend;

			rdbg(("RELATIVE(%p) -> %p (%s) in %s",
			    where, (void *)val,
			    obj->strtab + def->st_name, obj->path));

			*where = val;
			break;
		}

		case R_TYPESZ(ADDR): {
			def = _rtld_find_symdef(r_symndx, obj, &defobj, false);
			if (def == NULL)
				return -1;

			Elf_Addr val = (Elf_Addr)defobj->relocbase + rela->r_addend;

			*where = val;
			rdbg(("ADDR %s in %s --> %p in %s",
			    obj->strtab + obj->symtab[r_symndx].st_name,
			    obj->path, (void *)val, defobj->path));
			break;
		}

		case R_TYPESZ(TLS_DTPMOD): {
			def = _rtld_find_symdef(r_symndx, obj, &defobj, false);
			if (def == NULL)
				return -1;

			Elf_Addr val = (Elf_Addr)defobj->tlsindex + rela->r_addend;

			*where = val;
			rdbg(("DTPMOD %s in %s --> %p in %s",
			    obj->strtab + obj->symtab[r_symndx].st_name,
			    obj->path, (void *)val, defobj->path));
			break;
		}

		case R_TYPESZ(TLS_DTPREL): {
			Elf_Addr old = *where;
			Elf_Addr val = old;

			def = _rtld_find_symdef(r_symndx, obj, &defobj, false);
			if (def == NULL)
				return -1;

			if (!defobj->tls_done && _rtld_tls_offset_allocate(obj))
				return -1;

			val = (Elf_Addr)def->st_value - TLS_DTV_OFFSET;
			*where = val;

			rdbg(("DTPREL %s in %s --> %p in %s",
			    obj->strtab + obj->symtab[r_symndx].st_name,
			    obj->path, (void *)val, defobj->path));
			break;
		}

		default:
			rdbg(("sym = %lu, type = %lu, offset = %p, "
			    "addend = %p, contents = %p, symbol = %s",
			    (u_long)r_symndx, (u_long)r_type,
			    (void *)rela->r_offset, (void *)rela->r_addend,
			    (void *)load_ptr(where, sizeof(Elf_Addr)),
			    obj->strtab + obj->symtab[r_symndx].st_name));
			_rtld_error("%s: Unsupported relocation type %ld "
			    "in non-PLT relocations",
			    obj->path, (u_long)r_type);
			return -1;
		}
	}

	return 0;
}

int
_rtld_relocate_plt_lazy(const Obj_Entry *obj)
{
	/* PLT fixups were done above in the GOT relocation. */
	return 0;
}

static int
_rtld_relocate_plt_object(const Obj_Entry *obj, const Elf_Rel *rel,
    Elf_Addr *tp)
{
	const Obj_Entry *defobj;
	Elf_Addr new_value;

        assert(ELF_R_TYPE(rel->r_info) == R_TYPE(JMP_SLOT));

	const Elf_Sym *def = _rtld_find_plt_symdef(ELF_R_SYM(rel->r_info),
	    obj, &defobj, tp != NULL);
	if (__predict_false(def == NULL))
		return -1;
	if (__predict_false(def == &_rtld_sym_zero))
		return -1;

	if (ELF_ST_TYPE(def->st_info) == STT_GNU_IFUNC) {
		if (tp == NULL)
			return 0;
		new_value = _rtld_resolve_ifunc(defobj, def);
	} else {
		new_value = (Elf_Addr)(defobj->relocbase + def->st_value);
	}
	rdbg(("bind now/fixup in %s --> new=%p",
	    defobj->strtab + def->st_name, (void *)new_value));
	*(Elf_Addr *)(obj->relocbase + rel->r_offset) = new_value;

	if (tp)
		*tp = new_value;
	return 0;
}

void *
_rtld_bind(const Obj_Entry *obj, Elf_Word reloff)
{
	const Elf_Rel *pltrel = (const Elf_Rel *)(obj->pltrel + reloff);
	Elf_Addr new_value;
	int err;

	_rtld_shared_enter();
	err = _rtld_relocate_plt_object(obj, pltrel, &new_value);
	if (err)
		_rtld_die();
	_rtld_shared_exit();

	return (caddr_t)new_value;
}

int
_rtld_relocate_plt_objects(const Obj_Entry *obj)
{
	
	for (const Elf_Rel *rel = obj->pltrel; rel < obj->pltrellim; rel++) {
		if (_rtld_relocate_plt_object(obj, rel, NULL) < 0)
			return -1;
	}

	return 0;
}
