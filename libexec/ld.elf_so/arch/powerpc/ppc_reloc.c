/*	$NetBSD: ppc_reloc.c,v 1.49 2011/03/25 18:07:06 joerg Exp $	*/

/*-
 * Copyright (C) 1998	Tsubai Masanari
 * Portions copyright 2002 Charles M. Hannum <root@ihack.net>
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
 * 3. The name of the author may not be used to endorse or promote products
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: ppc_reloc.c,v 1.49 2011/03/25 18:07:06 joerg Exp $");
#endif /* not lint */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <machine/cpu.h>

#include "debug.h"
#include "rtld.h"

void _rtld_powerpc_pltcall(Elf_Word);
void _rtld_powerpc_pltresolve(Elf_Word, Elf_Word);

#define ha(x) ((((u_int32_t)(x) & 0x8000) ? \
			((u_int32_t)(x) + 0x10000) : (u_int32_t)(x)) >> 16)
#define l(x) ((u_int32_t)(x) & 0xffff)

void _rtld_bind_bssplt_start(void);
void _rtld_bind_secureplt_start(void);
void _rtld_relocate_nonplt_self(Elf_Dyn *, Elf_Addr);
caddr_t _rtld_bind(const Obj_Entry *, Elf_Word);
static int _rtld_relocate_plt_object(const Obj_Entry *,
    const Elf_Rela *, int, Elf_Addr *);

/*
 * The PPC PLT format consists of three sections:
 * (1) The "pltcall" and "pltresolve" glue code.  This is always 18 words.
 * (2) The code part of the PLT entries.  There are 2 words per entry for
 *     up to 8192 entries, then 4 words per entry for any additional entries.
 * (3) The data part of the PLT entries, comprising a jump table.
 *     This section is half the size of the second section (ie. 1 or 2 words
 *     per entry).
 */

/*
 * Setup the plt glue routines (for bss-plt).
 */
#define PLTCALL_SIZE	20
#define PLTRESOLVE_SIZE	24

void
_rtld_setup_pltgot(const Obj_Entry *obj)
{
	/*
	 * Secure-PLT is much more sane.
	 */
	if (obj->gotptr != NULL) {
		obj->gotptr[1] = (Elf_Addr) _rtld_bind_secureplt_start;
		obj->gotptr[2] = (Elf_Addr) obj;
		dbg(("obj %s secure-plt gotptr=%p start=%p obj=%p",
		    obj->path, obj->gotptr,
		    (void *) obj->gotptr[1], (void *) obj->gotptr[2]));
	} else {
		Elf_Word *pltcall, *pltresolve;
		Elf_Word *jmptab;
		int N = obj->pltrelalim - obj->pltrela;

		/* Entries beyond 8192 take twice as much space. */
		if (N > 8192)
			N += N-8192;

		dbg(("obj %s bss-plt pltgot=%p jmptab=%u start=%p obj=%p",
		    obj->path, obj->pltgot, 18 + N * 2,
		    _rtld_bind_bssplt_start, obj));

		pltcall = obj->pltgot;
		jmptab = pltcall + 18 + N * 2;

		memcpy(pltcall, _rtld_powerpc_pltcall, PLTCALL_SIZE);
		pltcall[1] |= ha(jmptab);
		pltcall[2] |= l(jmptab);

		pltresolve = obj->pltgot + 8;

		memcpy(pltresolve, _rtld_powerpc_pltresolve, PLTRESOLVE_SIZE);
		pltresolve[0] |= ha(_rtld_bind_bssplt_start);
		pltresolve[1] |= l(_rtld_bind_bssplt_start);
		pltresolve[3] |= ha(obj);
		pltresolve[4] |= l(obj);

		/*
		 * Invalidate the icache for only the code part of the PLT
		 * (and not the jump table at the end).
		 */
		__syncicache(pltcall, (char *)jmptab - (char *)pltcall);
	}
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

		case R_TYPE(32):	/* word32 S + A */
		case R_TYPE(GLOB_DAT):	/* word32 S + A */
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

		case R_TYPE(RELATIVE):	/* word32 B + A */
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

		case R_TYPE(DTPMOD32):
			def = _rtld_find_symdef(symnum, obj, &defobj, false);
			if (def == NULL)
				return -1;

			*where = (Elf_Addr)defobj->tlsindex;
			rdbg(("DTPMOD32 %s in %s --> %p in %s",
			    obj->strtab + obj->symtab[symnum].st_name,
			    obj->path, (void *)*where, defobj->path));
			break;

		case R_TYPE(DTPREL32):
			def = _rtld_find_symdef(symnum, obj, &defobj, false);
			if (def == NULL)
				return -1;

			if (!defobj->tls_done && _rtld_tls_offset_allocate(obj))
				return -1;

			*where = (Elf_Addr)(def->st_value + rela->r_addend
			    - TLS_DTV_OFFSET);
			rdbg(("DTPREL32 %s in %s --> %p in %s",
			    obj->strtab + obj->symtab[symnum].st_name,
			    obj->path, (void *)*where, defobj->path));
			break;

		case R_TYPE(TPREL32):
			def = _rtld_find_symdef(symnum, obj, &defobj, false);
			if (def == NULL)
				return -1;

			if (!defobj->tls_done && _rtld_tls_offset_allocate(obj))
				return -1;

			*where = (Elf_Addr)(def->st_value + rela->r_addend
			    + defobj->tlsoffset - TLS_TP_OFFSET);
			rdbg(("TPREL32 %s in %s --> %p in %s",
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
	Elf_Addr * const pltresolve = obj->pltgot + 8;
	const Elf_Rela *rela;
	int reloff;

	for (rela = obj->pltrela, reloff = 0;
	     rela < obj->pltrelalim;
	     rela++, reloff++) {
		Elf_Word *where = (Elf_Word *)(obj->relocbase + rela->r_offset);

		assert(ELF_R_TYPE(rela->r_info) == R_TYPE(JMP_SLOT));

		if (obj->gotptr != NULL) {
			/*
			 * For now, simply treat then as relative.
			 */
			*where += (Elf_Addr)obj->relocbase;
		} else {
			int distance;

			if (reloff < 32768) {
				/* li	r11,reloff */
				*where++ = 0x39600000 | reloff;
			} else {
				/* lis  r11,ha(reloff) */
				/* addi	r11,l(reloff) */
				*where++ = 0x3d600000 | ha(reloff);
				*where++ = 0x396b0000 | l(reloff);
			}
			/* b	pltresolve */
			distance = (Elf_Addr)pltresolve - (Elf_Addr)where;
			*where++ = 0x48000000 | (distance & 0x03fffffc);

			/*
			 * Icache invalidation is not done for each entry here
			 * because we sync the entire code part of the PLT once
			 * in _rtld_setup_pltgot() after all the entries have been
			 * initialized.
			 */
			/* __syncicache(where - 3, 12); */
		}
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
	int distance;
	unsigned long info = rela->r_info;

	assert(ELF_R_TYPE(info) == R_TYPE(JMP_SLOT));

	def = _rtld_find_plt_symdef(ELF_R_SYM(info), obj, &defobj, tp != NULL);
	if (__predict_false(def == NULL))
		return -1;
	if (__predict_false(def == &_rtld_sym_zero))
		return 0;

	value = (Elf_Addr)(defobj->relocbase + def->st_value);
	distance = value - (Elf_Addr)where;
	rdbg(("bind now/fixup in %s --> new=%p", 
	    defobj->strtab + def->st_name, (void *)value));

	if (obj->gotptr != NULL) {
		/*
		 * For Secure-PLT we simply replace the entry in GOT with the address
		 * of the routine.
		 */
		assert(where >= (Elf_Word *)obj->pltgot);
		assert(where < (Elf_Word *)obj->pltgot + (obj->pltrelalim - obj->pltrela));
		*where = value;
	} else if (abs(distance) < 32*1024*1024) {	/* inside 32MB? */
		/* b	value	# branch directly */
		*where = 0x48000000 | (distance & 0x03fffffc);
		__syncicache(where, 4);
	} else {
		Elf_Addr *pltcall, *jmptab;
		int N = obj->pltrelalim - obj->pltrela;
	
		/* Entries beyond 8192 take twice as much space. */
		if (N > 8192)
			N += N-8192;

		pltcall = obj->pltgot;
		jmptab = pltcall + 18 + N * 2;
	
		jmptab[reloff] = value;

		if (reloff < 32768) {
			/* li	r11,reloff */
			*where++ = 0x39600000 | reloff;
		} else {
#ifdef notyet
			/* lis  r11,ha(value) */
			/* addi	r11,l(value) */
			/* mtctr r11 */
			/* bctr */
			*where++ = 0x3d600000 | ha(value);
			*where++ = 0x396b0000 | l(value);
			*where++ = 0x7d6903a6;
			*where++ = 0x4e800420;
#else
			/* lis  r11,ha(reloff) */
			/* addi	r11,l(reloff) */
			*where++ = 0x3d600000 | ha(reloff);
			*where++ = 0x396b0000 | l(reloff);
#endif
		}
		/* b	pltcall	*/
		distance = (Elf_Addr)pltcall - (Elf_Addr)where;
		*where++ = 0x48000000 | (distance & 0x03fffffc);
		__syncicache(where - 3, 12);
	}

	if (tp)
		*tp = value;
	return 0;
}

caddr_t
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

	return (caddr_t)new_value;
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
