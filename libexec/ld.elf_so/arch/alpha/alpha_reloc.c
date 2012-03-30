/*	$NetBSD: alpha_reloc.c,v 1.38 2010/09/30 09:11:18 skrll Exp $	*/

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
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
 * Copyright 1996, 1997, 1998, 1999 John D. Polstra.   
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
#ifndef lint
__RCSID("$NetBSD: alpha_reloc.c,v 1.38 2010/09/30 09:11:18 skrll Exp $");
#endif /* not lint */

#include <sys/types.h>
#include <string.h>

#include "rtld.h"
#include "debug.h"

#ifdef RTLD_DEBUG_ALPHA
#define	adbg(x)		xprintf x
#else
#define	adbg(x)		/* nothing */
#endif

void _rtld_bind_start(void);
void _rtld_bind_start_old(void);
void _rtld_relocate_nonplt_self(Elf_Dyn *, Elf_Addr);
caddr_t _rtld_bind(const Obj_Entry *, Elf_Addr);
static inline int _rtld_relocate_plt_object(const Obj_Entry *,
    const Elf_Rela *, Elf_Addr *);

void
_rtld_setup_pltgot(const Obj_Entry *obj)
{
	uint32_t word0;

	/*
	 * The PLTGOT on the Alpha looks like this:
	 *
	 *	PLT HEADER
	 *	.
	 *	. 32 bytes
	 *	.
	 *	PLT ENTRY #0
	 *	.
	 *	. 12 bytes
	 *	.
	 *	PLT ENTRY #1
	 *	.
	 *	. 12 bytes
	 *	.
	 *	etc.
	 *
	 * The old-format entries look like (displacements filled in
	 * by the linker):
	 *
	 *	ldah	$28, 0($31)		# 0x279f0000
	 *	lda	$28, 0($28)		# 0x239c0000
	 *	br	$31, plt0		# 0xc3e00000
	 *
	 * The new-format entries look like:
	 *
	 *	br	$28, plt0		# 0xc3800000
	 *					# 0x00000000
	 *					# 0x00000000
	 *
	 * What we do is fetch the first PLT entry and check to
	 * see the first word of it matches the first word of the
	 * old format.  If so, we use a binding routine that can
	 * handle the old format, otherwise we use a binding routine
	 * that handles the new format.
	 *
	 * Note that this is done on a per-object basis, we can mix
	 * and match shared objects build with both the old and new
	 * linker.
	 */
	word0 = *(uint32_t *)(((char *) obj->pltgot) + 32);
	if ((word0 & 0xffff0000) == 0x279f0000) {
		/* Old PLT entry format. */
		adbg(("ALPHA: object %p has old PLT format\n", obj));
		obj->pltgot[2] = (Elf_Addr) &_rtld_bind_start_old;
		obj->pltgot[3] = (Elf_Addr) obj;
	} else {
		/* New PLT entry format. */
		adbg(("ALPHA: object %p has new PLT format\n", obj));
		obj->pltgot[2] = (Elf_Addr) &_rtld_bind_start;
		obj->pltgot[3] = (Elf_Addr) obj;
	}

	__asm volatile("imb");
}

/*
 * It is possible for the compiler to emit relocations for unaligned data.
 * We handle this situation with these inlines.
 */
#define	RELOC_ALIGNED_P(x) \
	(((uintptr_t)(x) & (sizeof(void *) - 1)) == 0)

static inline Elf_Addr
load_ptr(void *where)
{
	Elf_Addr res;

	memcpy(&res, where, sizeof(res));

	return (res);
}

static inline void
store_ptr(void *where, Elf_Addr val)
{

	memcpy(where, &val, sizeof(val));
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
		/* XXX For some reason I see a few GLOB_DAT relocs here. */
		*where += (Elf_Addr)relocbase;
	}
}

int
_rtld_relocate_nonplt_objects(Obj_Entry *obj)
{
	const Elf_Rela *rela;
	Elf_Addr target = -1;

	for (rela = obj->rela; rela < obj->relalim; rela++) {
		Elf_Addr        *where;
		const Elf_Sym   *def;
		const Obj_Entry *defobj;
		Elf_Addr         tmp;
		unsigned long	 symnum;

		where = (Elf_Addr *)(obj->relocbase + rela->r_offset);
		symnum = ELF_R_SYM(rela->r_info);

		switch (ELF_R_TYPE(rela->r_info)) {
		case R_TYPE(NONE):
			break;

		case R_TYPE(REFQUAD):
		case R_TYPE(GLOB_DAT):
			def = _rtld_find_symdef(symnum, obj, &defobj, false);
			if (def == NULL)
				return -1;
			target = (Elf_Addr)(defobj->relocbase +
			    def->st_value);

			tmp = target + rela->r_addend;
			if (__predict_true(RELOC_ALIGNED_P(where))) {
				if (*where != tmp)
					*where = tmp;
			} else {
				if (load_ptr(where) != tmp)
					store_ptr(where, tmp);
			}
			rdbg(("REFQUAD/GLOB_DAT %s in %s --> %p in %s",
			    obj->strtab + obj->symtab[symnum].st_name,
			    obj->path, (void *)tmp, defobj->path));
			break;

		case R_TYPE(RELATIVE):
			if (__predict_true(RELOC_ALIGNED_P(where)))
				*where += (Elf_Addr)obj->relocbase;
			else
				store_ptr(where,
				    load_ptr(where) + (Elf_Addr)obj->relocbase);
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

		default:
			rdbg(("sym = %lu, type = %lu, offset = %p, "
			    "addend = %p, contents = %p, symbol = %s",
			    symnum, (u_long)ELF_R_TYPE(rela->r_info),
			    (void *)rela->r_offset, (void *)rela->r_addend,
			    (void *)load_ptr(where),
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

		assert(ELF_R_TYPE(rela->r_info) == R_TYPE(JMP_SLOT));

		/* Just relocate the GOT slots pointing into the PLT */
		*where += (Elf_Addr)obj->relocbase;
		rdbg(("fixup !main in %s --> %p", obj->path, (void *)*where));
	}

	return 0;
}

static inline int
_rtld_relocate_plt_object(const Obj_Entry *obj, const Elf_Rela *rela,
    Elf_Addr *tp)
{
	Elf_Addr *where = (Elf_Addr *)(obj->relocbase + rela->r_offset);
	Elf_Addr new_value;
	const Elf_Sym *def;
	const Obj_Entry *defobj;
	Elf_Addr stubaddr; 
	unsigned long info = rela->r_info;

	assert(ELF_R_TYPE(info) == R_TYPE(JMP_SLOT));

	def = _rtld_find_plt_symdef(ELF_R_SYM(info), obj, &defobj, tp != NULL);
	if (__predict_false(def == NULL))
		return -1;
	if (__predict_false(def == &_rtld_sym_zero))
		return 0;

	new_value = (Elf_Addr)(defobj->relocbase + def->st_value);
	rdbg(("bind now/fixup in %s --> old=%p new=%p",
	    defobj->strtab + def->st_name, (void *)*where, (void *)new_value));

	if ((stubaddr = *where) != new_value) {
		int64_t delta, idisp;
		uint32_t insn[3], *stubptr;
		int insncnt;
		Elf_Addr pc;

		/* Point this GOT entry at the target. */
		*where = new_value;

		/*
		 * Alpha shared objects may have multiple GOTs, each
		 * of which may point to this entry in the PLT.  But,
		 * we only have a reference to the first GOT entry which
		 * points to this PLT entry.  In order to avoid having to
		 * re-bind this call every time a non-first GOT entry is
		 * used, we will attempt to patch up the PLT entry to
		 * reference the target, rather than the binder.
		 *
		 * When the PLT stub gets control, PV contains the address
		 * of the PLT entry.  Each PLT entry has room for 3 insns.
		 * If the displacement of the target from PV fits in a signed
		 * 32-bit integer, we can simply add it to PV.  Otherwise,
		 * we must load the GOT entry itself into PV.
		 *
		 * Note if the shared object uses the old PLT format, then
		 * we cannot patch up the PLT safely, and so we skip it
		 * in that case[*].
		 *
		 * [*] Actually, if we're not doing lazy-binding, then
		 * we *can* (and do) patch up this PLT entry; the PLTGOT
		 * thunk won't yet point to any binder entry point, and
		 * so this test will fail as it would for the new PLT
		 * entry format.
		 */
		if (obj->pltgot[2] == (Elf_Addr) &_rtld_bind_start_old) {
			rdbg(("  old PLT format"));
			goto out;
		}

		delta = new_value - stubaddr;
		rdbg(("  stubaddr=%p, where-stubaddr=%ld, delta=%ld",
		    (void *)stubaddr, (long)where - (long)stubaddr,
		    (long)delta));
		insncnt = 0;
		if ((int32_t)delta == delta) {
			/*
			 * We can adjust PV with an LDA, LDAH sequence.
			 *
			 * First, build an LDA insn to adjust the low 16
			 * bits.
			 */
			insn[insncnt++] = 0x08 << 26 | 27 << 21 | 27 << 16 |
			    (delta & 0xffff);
			rdbg(("  LDA  $27,%d($27)", (int16_t)delta));
			/*
			 * Adjust the delta to account for the effects of
			 * the LDA, including sign-extension.
			 */
			delta -= (int16_t)delta;
			if (delta != 0) {
				/*
				 * Build an LDAH instruction to adjust the
				 * high 16 bits.
				 */
				insn[insncnt++] = 0x09 << 26 | 27 << 21 |
				    27 << 16 | ((delta >> 16) & 0xffff);
				rdbg(("  LDAH $27,%d($27)",
				    (int16_t)(delta >> 16)));
			}
		} else {
			int64_t dhigh;

			/* We must load the GOT entry. */
			delta = (Elf_Addr)where - stubaddr;

			/*
			 * If the GOT entry is too far away from the PLT
			 * entry, then we can't patch up the PLT entry.
			 * This PLT entry will have to be bound for each
			 * GOT entry except for the first one.  This program
			 * will still run, albeit very slowly.  It is very
			 * unlikely that this case will ever happen in
			 * practice.
			 */
			if ((int32_t)delta != delta) {
				rdbg(("  PLT stub too far from GOT to relocate"));
				goto out;
			}
			dhigh = delta - (int16_t)delta;
			if (dhigh != 0) {
				/*
				 * Build an LDAH instruction to adjust the
				 * high 16 bits.
				 */
				insn[insncnt++] = 0x09 << 26 | 27 << 21 |
				    27 << 16 | ((dhigh >> 16) & 0xffff);
				rdbg(("  LDAH $27,%d($27)",
				    (int16_t)(dhigh >> 16)));
			}
			/* Build an LDQ to load the GOT entry. */
			insn[insncnt++] = 0x29 << 26 | 27 << 21 |
			    27 << 16 | (delta & 0xffff);
			rdbg(("  LDQ  $27,%d($27)",
			    (int16_t)delta));
		}

		/*
		 * Now, build a JMP or BR insn to jump to the target.  If
		 * the displacement fits in a sign-extended 21-bit field,
		 * we can use the more efficient BR insn.  Otherwise, we
		 * have to jump indirect through PV.
		 */
		pc = stubaddr + (4 * (insncnt + 1));
		idisp = (int64_t)(new_value - pc) >> 2;
		if (-0x100000 <= idisp && idisp < 0x100000) {
			insn[insncnt++] = 0x30 << 26 | 31 << 21 |
			    (idisp & 0x1fffff);
			rdbg(("  BR   $31,%p", (void *)new_value));
		} else {
			insn[insncnt++] = 0x1a << 26 | 31 << 21 |
			    27 << 16 | (idisp & 0x3fff);
			rdbg(("  JMP  $31,($27),%d",
			    (int)(idisp & 0x3fff)));
		}

		/*
		 * Fill in the tail of the PLT entry first, for reentrancy.
		 * Until we have overwritten the first insn (an unconditional
		 * branch), the remaining insns have no effect.
		 */
		stubptr = (uint32_t *)stubaddr;
		while (insncnt > 1) {
			insncnt--;
			stubptr[insncnt] = insn[insncnt];
		}
		/*
		 * Commit the tail of the insn sequence to memory
		 * before overwriting the first insn.
		 */
		__asm volatile("wmb" ::: "memory");
		stubptr[0] = insn[0];
		/*
		 * I-stream will be sync'd when we either return from
		 * the binder (lazy bind case) or when the PLTGOT thunk
		 * is patched up (bind-now case).
		 */
	}
out:
	if (tp)
		*tp = new_value;

	return 0;
}

caddr_t
_rtld_bind(const Obj_Entry *obj, Elf_Addr reloff)
{
	const Elf_Rela *rela = 
	    (const Elf_Rela *)((const uint8_t *)obj->pltrela + reloff);
	Elf_Addr result = 0; /* XXX gcc */
	int err;

	err = _rtld_relocate_plt_object(obj, rela, &result);
	if (err)
		_rtld_die();

	return (caddr_t)result;
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
