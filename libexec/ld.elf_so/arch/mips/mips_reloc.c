/*	$NetBSD: mips_reloc.c,v 1.63 2014/08/25 20:40:52 joerg Exp $	*/

/*
 * Copyright 1997 Michael L. Hitch <mhitch@montana.edu>
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: mips_reloc.c,v 1.63 2014/08/25 20:40:52 joerg Exp $");
#endif /* not lint */

#include <sys/types.h>
#include <sys/endian.h>
#include <sys/tls.h>

#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "rtld.h"

#ifdef __mips_o32
#define SUPPORT_OLD_BROKEN_LD
#endif

void _rtld_bind_start(void);
void _rtld_relocate_nonplt_self(Elf_Dyn *, Elf_Addr);
caddr_t _rtld_bind(Elf_Word, Elf_Addr, Elf_Addr, Elf_Addr);

/*
 * It is possible for the compiler to emit relocations for unaligned data.
 * We handle this situation with these inlines.
 */

#if ELFSIZE == 64
/*
 * ELF64 MIPS encodes the relocs uniquely.  The first 32-bits of info contain
 * the symbol index.  The top 32-bits contain three relocation types encoded
 * in big-endian integer with first relocation in LSB.  This means for little
 * endian we have to byte swap that interger (r_type).
 */
#define	Elf_Sxword			Elf64_Sxword
#define	ELF_R_NXTTYPE_64_P(r_type)	((((r_type) >> 8) & 0xff) == R_TYPE(64))
#if BYTE_ORDER == LITTLE_ENDIAN
#undef ELF_R_SYM
#undef ELF_R_TYPE
#define ELF_R_SYM(r_info)		((r_info) & 0xffffffff)
#define ELF_R_TYPE(r_info)		bswap32((r_info) >> 32)
#endif
#else
#define	ELF_R_NXTTYPE_64_P(r_type)	(0)
#define	Elf_Sxword			Elf32_Sword
#endif
#define	GOT1_MASK			(~(Elf_Addr)0 >> 1)

static inline Elf_Sxword
load_ptr(void *where, size_t len)
{
	Elf_Sxword val;

	if (__predict_true(((uintptr_t)where & (len - 1)) == 0)) {
#if ELFSIZE == 64
		if (len == sizeof(Elf_Sxword))
			return *(Elf_Sxword *)where;
#endif
		return *(Elf_Sword *)where;
	}

	val = 0;
#if BYTE_ORDER == LITTLE_ENDIAN
	(void)memcpy(&val, where, len);
#endif
#if BYTE_ORDER == BIG_ENDIAN
	(void)memcpy((uint8_t *)((&val)+1) - len, where, len);
#endif
	return (len == sizeof(Elf_Sxword)) ? val : (Elf_Sword)val;
}

static inline void
store_ptr(void *where, Elf_Sxword val, size_t len)
{
	if (__predict_true(((uintptr_t)where & (len - 1)) == 0)) {
#if ELFSIZE == 64
		if (len == sizeof(Elf_Sxword)) {
			*(Elf_Sxword *)where = val;
			return;
		}
#endif
		*(Elf_Sword *)where = val;
		return;
	}
#if BYTE_ORDER == LITTLE_ENDIAN
	(void)memcpy(where, &val, len);
#endif
#if BYTE_ORDER == BIG_ENDIAN
	(void)memcpy(where, (const uint8_t *)((&val)+1) - len, len);
#endif
}


void
_rtld_setup_pltgot(const Obj_Entry *obj)
{
	obj->pltgot[0] = (Elf_Addr) &_rtld_bind_start;
	/* XXX only if obj->pltgot[1] & 0x80000000 ?? */
	obj->pltgot[1] |= (Elf_Addr) obj;
}

void
_rtld_relocate_nonplt_self(Elf_Dyn *dynp, Elf_Addr relocbase)
{
	const Elf_Rel *rel = 0, *rellim;
	Elf_Addr relsz = 0;
	void *where;
	const Elf_Sym *symtab = NULL, *sym;
	Elf_Addr *got = NULL;
	Elf_Word local_gotno = 0, symtabno = 0, gotsym = 0;
	size_t i;

	for (; dynp->d_tag != DT_NULL; dynp++) {
		switch (dynp->d_tag) {
		case DT_REL:
			rel = (const Elf_Rel *)(relocbase + dynp->d_un.d_ptr);
			break;
		case DT_RELSZ:
			relsz = dynp->d_un.d_val;
			break;
		case DT_SYMTAB:
			symtab = (const Elf_Sym *)(relocbase + dynp->d_un.d_ptr);
			break;
		case DT_PLTGOT:
			got = (Elf_Addr *)(relocbase + dynp->d_un.d_ptr);
			break;
		case DT_MIPS_LOCAL_GOTNO:
			local_gotno = dynp->d_un.d_val;
			break;
		case DT_MIPS_SYMTABNO:
			symtabno = dynp->d_un.d_val;
			break;
		case DT_MIPS_GOTSYM:
			gotsym = dynp->d_un.d_val;
			break;
		}
	}

	i = (got[1] & 0x80000000) ? 2 : 1;
	/* Relocate the local GOT entries */
	got += i;
	for (; i < local_gotno; i++)
		*got++ += relocbase;
	sym = symtab + gotsym;
	/* Now do the global GOT entries */
	for (i = gotsym; i < symtabno; i++) {
		*got = sym->st_value + relocbase;
		++sym;
		++got;
	}

	rellim = (const Elf_Rel *)((uintptr_t)rel + relsz);
	for (; rel < rellim; rel++) {
		Elf_Word r_symndx, r_type;

		where = (void *)(relocbase + rel->r_offset);

		r_symndx = ELF_R_SYM(rel->r_info);
		r_type = ELF_R_TYPE(rel->r_info);

		switch (r_type & 0xff) {
		case R_TYPE(REL32): {
			const size_t rlen =
			    ELF_R_NXTTYPE_64_P(r_type)
				? sizeof(Elf_Sxword)
				: sizeof(Elf_Sword);
			Elf_Sxword old = load_ptr(where, rlen);
			Elf_Sxword val = old;
#if ELFSIZE == 64
			assert(r_type == R_TYPE(REL32)
			    || r_type == (R_TYPE(REL32)|(R_TYPE(64) << 8)));
#endif
			assert(r_symndx < gotsym);
			sym = symtab + r_symndx;
			assert(ELF_ST_BIND(sym->st_info) == STB_LOCAL);
			val += relocbase;
			store_ptr(where, val, sizeof(Elf_Sword));
			rdbg(("REL32/L(%p) %p -> %p in <self>",
			    where, (void *)old, (void *)val));
			store_ptr(where, val, rlen);
			break;
		}

		case R_TYPE(GPREL32):
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
	const Elf_Rel *rel;
	Elf_Addr *got = obj->pltgot;
	const Elf_Sym *sym, *def;
	const Obj_Entry *defobj;
	Elf_Word i;
#ifdef SUPPORT_OLD_BROKEN_LD
	int broken;
#endif

#ifdef SUPPORT_OLD_BROKEN_LD
	broken = 0;
	sym = obj->symtab;
	for (i = 1; i < 12; i++)
		if (sym[i].st_info == ELF_ST_INFO(STB_LOCAL, STT_NOTYPE))
			broken = 1;
	dbg(("%s: broken=%d", obj->path, broken));
#endif

	i = (got[1] & 0x80000000) ? 2 : 1;
	/* Relocate the local GOT entries */
	got += i;
	for (; i < obj->local_gotno; i++)
		*got++ += (Elf_Addr)obj->relocbase;
	sym = obj->symtab + obj->gotsym;
	/* Now do the global GOT entries */
	for (i = obj->gotsym; i < obj->symtabno; i++) {
		rdbg((" doing got %d sym %p (%s, %lx)", i - obj->gotsym, sym,
		    sym->st_name + obj->strtab, (u_long) *got));

#ifdef SUPPORT_OLD_BROKEN_LD
		if (ELF_ST_TYPE(sym->st_info) == STT_FUNC &&
		    broken && sym->st_shndx == SHN_UNDEF) {
			/*
			 * XXX DANGER WILL ROBINSON!
			 * You might think this is stupid, as it intentionally
			 * defeats lazy binding -- and you'd be right.
			 * Unfortunately, for lazy binding to work right, we
			 * need to a way to force the GOT slots used for
			 * function pointers to be resolved immediately.  This
			 * is supposed to be done automatically by the linker,
			 * by not outputting a PLT slot and setting st_value
			 * to 0 if there are non-PLT references, but older
			 * versions of GNU ld do not do this.
			 */
			def = _rtld_find_symdef(i, obj, &defobj, false);
			if (def == NULL)
				return -1;
			*got = def->st_value + (Elf_Addr)defobj->relocbase;
		} else
#endif
		if (ELF_ST_TYPE(sym->st_info) == STT_FUNC &&
		    sym->st_value != 0 && sym->st_shndx == SHN_UNDEF) {
			/*
			 * If there are non-PLT references to the function,
			 * st_value should be 0, forcing us to resolve the
			 * address immediately.
			 *
			 * XXX DANGER WILL ROBINSON!
			 * The linker is not outputting PLT slots for calls to
			 * functions that are defined in the same shared
			 * library.  This is a bug, because it can screw up
			 * link ordering rules if the symbol is defined in
			 * more than one module.  For now, if there is a
			 * definition, we fail the test above and force a full
			 * symbol lookup.  This means that all intra-module
			 * calls are bound immediately.  - mycroft, 2003/09/24
			 */
			*got = sym->st_value + (Elf_Addr)obj->relocbase;
		} else if (sym->st_info == ELF_ST_INFO(STB_GLOBAL, STT_SECTION)) {
			/* Symbols with index SHN_ABS are not relocated. */
			if (sym->st_shndx != SHN_ABS)
				*got = sym->st_value +
				    (Elf_Addr)obj->relocbase;
		} else {
			def = _rtld_find_symdef(i, obj, &defobj, false);
			if (def == NULL)
				return -1;
			*got = def->st_value + (Elf_Addr)defobj->relocbase;
		}

		rdbg(("  --> now %lx", (u_long) *got));
		++sym;
		++got;
	}

	got = obj->pltgot;
	for (rel = obj->rel; rel < obj->rellim; rel++) {
		Elf_Word	r_symndx, r_type;
		void		*where;

		where = obj->relocbase + rel->r_offset;
		r_symndx = ELF_R_SYM(rel->r_info);
		r_type = ELF_R_TYPE(rel->r_info);

		switch (r_type & 0xff) {
		case R_TYPE(NONE):
			break;

		case R_TYPE(REL32): {
			/* 32-bit PC-relative reference */
			const size_t rlen =
			    ELF_R_NXTTYPE_64_P(r_type)
				? sizeof(Elf_Sxword)
				: sizeof(Elf_Sword);
			Elf_Sxword old = load_ptr(where, rlen);
			Elf_Sxword val = old;

			def = obj->symtab + r_symndx;

			if (r_symndx >= obj->gotsym) {
				val += got[obj->local_gotno + r_symndx - obj->gotsym];
				rdbg(("REL32/G(%p) %p --> %p (%s) in %s",
				    where, (void *)old, (void *)val,
				    obj->strtab + def->st_name,
				    obj->path));
			} else {
				/*
				 * XXX: ABI DIFFERENCE!
				 *
				 * Old NetBSD binutils would generate shared
				 * libs with section-relative relocations being
				 * already adjusted for the start address of
				 * the section.
				 *
				 * New binutils, OTOH, generate shared libs
				 * with the same relocations being based at
				 * zero, so we need to add in the start address
				 * of the section.
				 *
				 * --rkb, Oct 6, 2001
				 */

				if (def->st_info ==
				    ELF_ST_INFO(STB_LOCAL, STT_SECTION)
#ifdef SUPPORT_OLD_BROKEN_LD
				    && !broken
#endif
				    )
					val += (Elf_Addr)def->st_value;

				val += (Elf_Addr)obj->relocbase;

				rdbg(("REL32/L(%p) %p -> %p (%s) in %s",
				    where, (void *)old, (void *)val,
				    obj->strtab + def->st_name, obj->path));
			}
			store_ptr(where, val, rlen);
			break;
		}

#if ELFSIZE == 64
		case R_TYPE(TLS_DTPMOD64):
#else
		case R_TYPE(TLS_DTPMOD32): 
#endif
		{
			Elf_Addr old = load_ptr(where, ELFSIZE / 8);
			Elf_Addr val = old;

			def = _rtld_find_symdef(r_symndx, obj, &defobj, false);
			if (def == NULL)
				return -1;

			val += (Elf_Addr)defobj->tlsindex;

			store_ptr(where, val, ELFSIZE / 8);
			rdbg(("DTPMOD %s in %s --> %p in %s",
			    obj->strtab + obj->symtab[r_symndx].st_name,
			    obj->path, (void *)old, defobj->path));
			break;
		}

#if ELFSIZE == 64
		case R_TYPE(TLS_DTPREL64):
#else
		case R_TYPE(TLS_DTPREL32):
#endif
		{
			Elf_Addr old = load_ptr(where, ELFSIZE / 8);
			Elf_Addr val = old;

			def = _rtld_find_symdef(r_symndx, obj, &defobj, false);
			if (def == NULL)
				return -1;

			if (!defobj->tls_done && _rtld_tls_offset_allocate(obj))
				return -1;

			val += (Elf_Addr)def->st_value - TLS_DTV_OFFSET;
			store_ptr(where, val, ELFSIZE / 8);

			rdbg(("DTPREL %s in %s --> %p in %s",
			    obj->strtab + obj->symtab[r_symndx].st_name,
			    obj->path, (void *)old, defobj->path));
			break;
		}

#if ELFSIZE == 64
		case R_TYPE(TLS_TPREL64):
#else
		case R_TYPE(TLS_TPREL32):
#endif
		{
			Elf_Addr old = load_ptr(where, ELFSIZE / 8);
			Elf_Addr val = old;

			def = _rtld_find_symdef(r_symndx, obj, &defobj, false);
			if (def == NULL)
				return -1;

			if (!defobj->tls_done && _rtld_tls_offset_allocate(obj))
				return -1;

			val += (Elf_Addr)(def->st_value + defobj->tlsoffset
			    - TLS_TP_OFFSET);
			store_ptr(where, val, ELFSIZE / 8);

			rdbg(("TPREL %s in %s --> %p in %s",
			    obj->strtab + obj->symtab[r_symndx].st_name,
			    obj->path, (void *)*where, defobj->path));
			break;
		}

		default:
			rdbg(("sym = %lu, type = %lu, offset = %p, "
			    "contents = %p, symbol = %s",
			    (u_long)r_symndx, (u_long)ELF_R_TYPE(rel->r_info),
			    (void *)rel->r_offset,
			    (void *)load_ptr(where, sizeof(Elf_Sword)),
			    obj->strtab + obj->symtab[r_symndx].st_name));
			_rtld_error("%s: Unsupported relocation type %ld "
			    "in non-PLT relocations",
			    obj->path, (u_long) ELF_R_TYPE(rel->r_info));
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

static inline int
_rtld_relocate_plt_object(const Obj_Entry *obj, Elf_Word sym, Elf_Addr *tp)
{
	Elf_Addr *got = obj->pltgot;
	const Elf_Sym *def;
	const Obj_Entry *defobj;
	Elf_Addr new_value;

	def = _rtld_find_plt_symdef(sym, obj, &defobj, tp != NULL);
	if (__predict_false(def == NULL))
		return -1;
	if (__predict_false(def == &_rtld_sym_zero))
		return 0;

	if (ELF_ST_TYPE(def->st_info) == STT_GNU_IFUNC) {
		if (tp == NULL)
			return 0;
		new_value = _rtld_resolve_ifunc(defobj, def);
	} else {
		new_value = (Elf_Addr)(defobj->relocbase + def->st_value);
	}
	rdbg(("bind now/fixup in %s --> new=%p",
	    defobj->strtab + def->st_name, (void *)new_value));
	got[obj->local_gotno + sym - obj->gotsym] = new_value;

	if (tp)
		*tp = new_value;
	return 0;
}

caddr_t
_rtld_bind(Elf_Word a0, Elf_Addr a1, Elf_Addr a2, Elf_Addr a3)
{
	Elf_Addr *got = (Elf_Addr *)(a2 - 0x7ff0);
	const Obj_Entry *obj = (Obj_Entry *)(got[1] & GOT1_MASK);
	Elf_Addr new_value = 0;	/* XXX gcc */
	int err;

	_rtld_shared_enter();
	err = _rtld_relocate_plt_object(obj, a0, &new_value);
	if (err)
		_rtld_die();
	_rtld_shared_exit();

	return (caddr_t)new_value;
}

int
_rtld_relocate_plt_objects(const Obj_Entry *obj)
{
	const Elf_Sym *sym = obj->symtab + obj->gotsym;
	Elf_Word i;

	for (i = obj->gotsym; i < obj->symtabno; i++, sym++) {
		if (ELF_ST_TYPE(sym->st_info) == STT_FUNC)
			if (_rtld_relocate_plt_object(obj, i, NULL) < 0)
				return -1;
	}

	return 0;
}
