/*	$NetBSD: mdreloc.c,v 1.40 2011/03/25 18:07:07 joerg Exp $	*/

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Frank van der Linden for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright 1996 John D. Polstra.
 * Copyright 1996 Matt Thomas <matt@3am-software.com>
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by John Polstra.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
#ifndef lint
__RCSID("$NetBSD: mdreloc.c,v 1.40 2011/03/25 18:07:07 joerg Exp $");
#endif /* not lint */

#include <sys/types.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <elf.h>

#include "debug.h"
#include "rtld.h"

void _rtld_bind_start(void);
void _rtld_relocate_nonplt_self(Elf_Dyn *, Elf_Addr);
caddr_t _rtld_bind(const Obj_Entry *, Elf_Word);
static inline int _rtld_relocate_plt_object(const Obj_Entry *,
    const Elf_Rela *, Elf_Addr *);

void
_rtld_setup_pltgot(const Obj_Entry *obj)
{
	obj->pltgot[1] = (Elf_Addr) obj;
	obj->pltgot[2] = (Elf_Addr) &_rtld_bind_start;
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
	/*
	 * Assume only 64-bit relocations here, which should always
	 * be true for the dynamic linker.
	 */
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
	const Elf_Sym *def = NULL;
	const Obj_Entry *defobj =NULL;

	for (rela = obj->rela; rela < obj->relalim; rela++) {
		Elf64_Addr *where64;
		Elf32_Addr *where32;
		Elf64_Addr tmp64;
		Elf32_Addr tmp32;
		unsigned long	 symnum;

		where64 = (Elf64_Addr *)(obj->relocbase + rela->r_offset);
		where32 = (Elf32_Addr *)where64;
		symnum = ELF_R_SYM(rela->r_info);

		switch (ELF_R_TYPE(rela->r_info)) {
		case R_TYPE(NONE):
			break;

		case R_TYPE(32):	/* word32 S + A, truncate */
		case R_TYPE(32S):	/* word32 S + A, signed truncate */
		case R_TYPE(GOT32):	/* word32 G + A (XXX can we see these?) */
			def = _rtld_find_symdef(symnum, obj, &defobj, false);
			if (def == NULL)
				return -1;
			tmp32 = (Elf32_Addr)(u_long)(defobj->relocbase +
			    def->st_value + rela->r_addend);

			if (*where32 != tmp32)
				*where32 = tmp32;
			rdbg(("32/32S %s in %s --> %p in %s",
			    obj->strtab + obj->symtab[symnum].st_name,
			    obj->path, (void *)(uintptr_t)*where32,
			    defobj->path));
			break;
		case R_TYPE(64):	/* word64 S + A */
			def = _rtld_find_symdef(symnum, obj, &defobj, false);
			if (def == NULL)
				return -1;
			tmp64 = (Elf64_Addr)(defobj->relocbase + def->st_value +
			    rela->r_addend);

			if (*where64 != tmp64)
				*where64 = tmp64;
			rdbg(("64 %s in %s --> %p in %s",
			    obj->strtab + obj->symtab[symnum].st_name,
			    obj->path, (void *)*where64, defobj->path));
			break;
		case R_TYPE(PC32):	/* word32 S + A - P */
			def = _rtld_find_symdef(symnum, obj, &defobj, false);
			if (def == NULL)
				return -1;
			tmp32 = (Elf32_Addr)(u_long)(defobj->relocbase +
			    def->st_value + rela->r_addend -
			    (Elf64_Addr)where64);
			if (*where32 != tmp32)
				*where32 = tmp32;
			rdbg(("PC32 %s in %s --> %p in %s",
			    obj->strtab + obj->symtab[symnum].st_name,
			    obj->path, (void *)(unsigned long)*where32,
			    defobj->path));
			break;
		case R_TYPE(GLOB_DAT):	/* word64 S */
			def = _rtld_find_symdef(symnum, obj, &defobj, false);
			if (def == NULL)
				return -1;
			tmp64 = (Elf64_Addr)(defobj->relocbase + def->st_value);

			if (*where64 != tmp64)
				*where64 = tmp64;
			rdbg(("64 %s in %s --> %p in %s",
			    obj->strtab + obj->symtab[symnum].st_name,
			    obj->path, (void *)*where64, defobj->path));
			break;
		case R_TYPE(RELATIVE):  /* word64 B + A */
			tmp64 = (Elf64_Addr)(obj->relocbase + rela->r_addend);
			if (*where64 != tmp64)
				*where64 = tmp64;
			rdbg(("RELATIVE in %s --> %p", obj->path,
			    (void *)*where64));
       			break;

		case R_TYPE(TPOFF64):
			def = _rtld_find_symdef(symnum, obj, &defobj, false);
			if (def == NULL)
				return -1;

			if (!defobj->tls_done &&
			    _rtld_tls_offset_allocate(obj))
				return -1;

			*where64 = (Elf64_Addr)(def->st_value -
			    defobj->tlsoffset + rela->r_addend);

			rdbg(("TPOFF64 %s in %s --> %p",
			    obj->strtab + obj->symtab[symnum].st_name,
			    obj->path, (void *)*where64));

			break;

		case R_TYPE(DTPMOD64):
			def = _rtld_find_symdef(symnum, obj, &defobj, false);
			if (def == NULL)
				return -1;

			*where64 = (Elf64_Addr)defobj->tlsindex;

			rdbg(("DTPMOD64 %s in %s --> %p",
			    obj->strtab + obj->symtab[symnum].st_name,
			    obj->path, (void *)*where64));

			break;

		case R_TYPE(DTPOFF64):
			def = _rtld_find_symdef(symnum, obj, &defobj, false);
			if (def == NULL)
				return -1;

			*where64 = (Elf64_Addr)(def->st_value + rela->r_addend);

			rdbg(("DTPOFF64 %s in %s --> %p",
			    obj->strtab + obj->symtab[symnum].st_name,
			    obj->path, (void *)*where64));

			break;

		case R_TYPE(COPY):
			rdbg(("COPY"));
			break;

		default:
			rdbg(("sym = %lu, type = %lu, offset = %p, "
			    "addend = %p, contents = %p, symbol = %s",
			    symnum, (u_long)ELF_R_TYPE(rela->r_info),
			    (void *)rela->r_offset, (void *)rela->r_addend,
			    (void *)*where64,
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

	if (!obj->relocbase)
		return 0;

	for (rela = obj->pltrela; rela < obj->pltrelalim; rela++) {
		Elf_Addr *where = (Elf_Addr *)(obj->relocbase + rela->r_offset);

		assert(ELF_R_TYPE(rela->r_info) == R_TYPE(JUMP_SLOT));

		/* Just relocate the GOT slots pointing into the PLT */
		*where += (Elf_Addr)obj->relocbase;
		rdbg(("fixup !main in %s --> %p", obj->path, (void *)*where));
	}

	return 0;
}

static inline int
_rtld_relocate_plt_object(const Obj_Entry *obj, const Elf_Rela *rela, Elf_Addr *tp)
{
	Elf_Addr *where = (Elf_Addr *)(obj->relocbase + rela->r_offset);
	Elf_Addr new_value;
	const Elf_Sym  *def;
	const Obj_Entry *defobj;
	unsigned long info = rela->r_info;

	assert(ELF_R_TYPE(info) == R_TYPE(JUMP_SLOT));

	def = _rtld_find_plt_symdef(ELF_R_SYM(info), obj, &defobj, tp != NULL);
	if (__predict_false(def == NULL))
		return -1;
	if (__predict_false(def == &_rtld_sym_zero))
		return 0;

	new_value = (Elf_Addr)(defobj->relocbase + def->st_value +
	    rela->r_addend);
	rdbg(("bind now/fixup in %s --> old=%p new=%p", 
	    defobj->strtab + def->st_name, (void *)*where, (void *)new_value));
	if (*where != new_value)
		*where = new_value;

	if (tp)
		*tp = new_value - rela->r_addend;

	return 0;
}

caddr_t
_rtld_bind(const Obj_Entry *obj, Elf_Word reloff)
{
	const Elf_Rela *rela = obj->pltrela + reloff;
	Elf_Addr new_value;
	int error;

	new_value = 0; /* XXX GCC4 */

	_rtld_shared_enter();
	error = _rtld_relocate_plt_object(obj, rela, &new_value);
	if (error)
		_rtld_die();
	_rtld_shared_exit();

	return (caddr_t)new_value;
}

int
_rtld_relocate_plt_objects(const Obj_Entry *obj)
{
	const Elf_Rela *rela;

	for (rela = obj->pltrela; rela < obj->pltrelalim; rela++)
		if (_rtld_relocate_plt_object(obj, rela, NULL) < 0)
			return -1;

	return 0;
}
