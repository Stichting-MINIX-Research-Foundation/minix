/*	$NetBSD: mdreloc.c,v 1.47 2011/03/31 12:47:01 nakayama Exp $	*/

/*-
 * Copyright (c) 1999, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg and by Charles M. Hannum.
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
__RCSID("$NetBSD: mdreloc.c,v 1.47 2011/03/31 12:47:01 nakayama Exp $");
#endif /* not lint */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rtldenv.h"
#include "debug.h"
#include "rtld.h"

/*
 * The following table holds for each relocation type:
 *	- the width in bits of the memory location the relocation
 *	  applies to (not currently used)
 *	- the number of bits the relocation value must be shifted to the
 *	  right (i.e. discard least significant bits) to fit into
 *	  the appropriate field in the instruction word.
 *	- flags indicating whether
 *		* the relocation involves a symbol
 *		* the relocation is relative to the current position
 *		* the relocation is for a GOT entry
 *		* the relocation is relative to the load address
 *
 */
#define _RF_S		0x80000000		/* Resolve symbol */
#define _RF_A		0x40000000		/* Use addend */
#define _RF_P		0x20000000		/* Location relative */
#define _RF_G		0x10000000		/* GOT offset */
#define _RF_B		0x08000000		/* Load address relative */
#define _RF_U		0x04000000		/* Unaligned */
#define _RF_SZ(s)	(((s) & 0xff) << 8)	/* memory target size */
#define _RF_RS(s)	( (s) & 0xff)		/* right shift */
static const int reloc_target_flags[R_TYPE(TLS_TPOFF64)+1] = {
	0,							/* NONE */
	_RF_S|_RF_A|		_RF_SZ(8)  | _RF_RS(0),		/* RELOC_8 */
	_RF_S|_RF_A|		_RF_SZ(16) | _RF_RS(0),		/* RELOC_16 */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(0),		/* RELOC_32 */
	_RF_S|_RF_A|_RF_P|	_RF_SZ(8)  | _RF_RS(0),		/* DISP_8 */
	_RF_S|_RF_A|_RF_P|	_RF_SZ(16) | _RF_RS(0),		/* DISP_16 */
	_RF_S|_RF_A|_RF_P|	_RF_SZ(32) | _RF_RS(0),		/* DISP_32 */
	_RF_S|_RF_A|_RF_P|	_RF_SZ(32) | _RF_RS(2),		/* WDISP_30 */
	_RF_S|_RF_A|_RF_P|	_RF_SZ(32) | _RF_RS(2),		/* WDISP_22 */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(10),	/* HI22 */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(0),		/* 22 */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(0),		/* 13 */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(0),		/* LO10 */
	_RF_G|			_RF_SZ(32) | _RF_RS(0),		/* GOT10 */
	_RF_G|			_RF_SZ(32) | _RF_RS(0),		/* GOT13 */
	_RF_G|			_RF_SZ(32) | _RF_RS(10),	/* GOT22 */
	_RF_S|_RF_A|_RF_P|	_RF_SZ(32) | _RF_RS(0),		/* PC10 */
	_RF_S|_RF_A|_RF_P|	_RF_SZ(32) | _RF_RS(10),	/* PC22 */
	      _RF_A|_RF_P|	_RF_SZ(32) | _RF_RS(2),		/* WPLT30 */
				_RF_SZ(32) | _RF_RS(0),		/* COPY */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(0),		/* GLOB_DAT */
				_RF_SZ(32) | _RF_RS(0),		/* JMP_SLOT */
	      _RF_A|	_RF_B|	_RF_SZ(32) | _RF_RS(0),		/* RELATIVE */
	_RF_S|_RF_A|	_RF_U|	_RF_SZ(32) | _RF_RS(0),		/* UA_32 */

	/* TLS and 64 bit relocs not listed here... */
};

#ifdef RTLD_DEBUG_RELOC
static const char *reloc_names[] = {
	"NONE", "RELOC_8", "RELOC_16", "RELOC_32", "DISP_8",
	"DISP_16", "DISP_32", "WDISP_30", "WDISP_22", "HI22",
	"22", "13", "LO10", "GOT10", "GOT13",
	"GOT22", "PC10", "PC22", "WPLT30", "COPY",
	"GLOB_DAT", "JMP_SLOT", "RELATIVE", "UA_32",

	/* not used with 32bit userland, besides a few of the TLS ones */
	"PLT32",
	"HIPLT22", "LOPLT10", "LOPLT10", "PCPLT22", "PCPLT32",
	"10", "11", "64", "OLO10", "HH22",
	"HM10", "LM22", "PC_HH22", "PC_HM10", "PC_LM22", 
	"WDISP16", "WDISP19", "GLOB_JMP", "7", "5", "6",
	"DISP64", "PLT64", "HIX22", "LOX10", "H44", "M44", 
	"L44", "REGISTER", "UA64", "UA16",
	"TLS_GD_HI22", "TLS_GD_LO10", "TLS_GD_ADD", "TLS_GD_CALL",
	"TLS_LDM_HI22", "TLS_LDM_LO10", "TLS_LDM_ADD", "TLS_LDM_CALL",
	"TLS_LDO_HIX22", "TLS_LDO_LOX10", "TLS_LDO_ADD", "TLS_IE_HI22", 
	"TLS_IE_LO10", "TLS_IE_LD", "TLS_IE_LDX", "TLS_IE_ADD", "TLS_LE_HIX22", 
	"TLS_LE_LOX10", "TLS_DTPMOD32", "TLS_DTPMOD64", "TLS_DTPOFF32", 
	"TLS_DTPOFF64", "TLS_TPOFF32", "TLS_TPOFF64",
};
#endif

#define RELOC_RESOLVE_SYMBOL(t)		((reloc_target_flags[t] & _RF_S) != 0)
#define RELOC_PC_RELATIVE(t)		((reloc_target_flags[t] & _RF_P) != 0)
#define RELOC_BASE_RELATIVE(t)		((reloc_target_flags[t] & _RF_B) != 0)
#define RELOC_UNALIGNED(t)		((reloc_target_flags[t] & _RF_U) != 0)
#define RELOC_USE_ADDEND(t)		((reloc_target_flags[t] & _RF_A) != 0)
#define RELOC_TARGET_SIZE(t)		((reloc_target_flags[t] >> 8) & 0xff)
#define RELOC_VALUE_RIGHTSHIFT(t)	(reloc_target_flags[t] & 0xff)
#define RELOC_TLS(t)			(t >= R_TYPE(TLS_GD_HI22))

static const int reloc_target_bitmask[] = {
#define _BM(x)	(~(-(1ULL << (x))))
	0,				/* NONE */
	_BM(8), _BM(16), _BM(32),	/* RELOC_8, _16, _32 */
	_BM(8), _BM(16), _BM(32),	/* DISP8, DISP16, DISP32 */
	_BM(30), _BM(22),		/* WDISP30, WDISP22 */
	_BM(22), _BM(22),		/* HI22, _22 */
	_BM(13), _BM(10),		/* RELOC_13, _LO10 */
	_BM(10), _BM(13), _BM(22),	/* GOT10, GOT13, GOT22 */
	_BM(10), _BM(22),		/* _PC10, _PC22 */  
	_BM(30), 0,			/* _WPLT30, _COPY */
	-1, -1, -1,			/* _GLOB_DAT, JMP_SLOT, _RELATIVE */
	_BM(32)				/* _UA32 */
#undef _BM
};
#define RELOC_VALUE_BITMASK(t)	(reloc_target_bitmask[t])

void _rtld_bind_start(void);
void _rtld_relocate_nonplt_self(Elf_Dyn *, Elf_Addr);
caddr_t _rtld_bind(const Obj_Entry *, Elf_Word);
static inline int _rtld_relocate_plt_object(const Obj_Entry *,
    const Elf_Rela *, Elf_Addr *);

void
_rtld_setup_pltgot(const Obj_Entry *obj)
{
	/*
	 * PLTGOT is the PLT on the sparc.
	 * The first entry holds the call the dynamic linker.
	 * We construct a `call' sequence that transfers
	 * to `_rtld_bind_start()'.
	 * The second entry holds the object identification.
	 * Note: each PLT entry is three words long.
	 */
#define SAVE	0x9de3bfa0	/* i.e. `save %sp,-96,%sp' */
#define CALL	0x40000000
#define NOP	0x01000000
	obj->pltgot[0] = SAVE;
	obj->pltgot[1] = CALL |
	    ((Elf_Addr) &_rtld_bind_start - (Elf_Addr) &obj->pltgot[1]) >> 2;
	obj->pltgot[2] = NOP;
	obj->pltgot[3] = (Elf_Addr) obj;
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
		*where += (Elf_Addr)(relocbase + rela->r_addend);
	}
}

int
_rtld_relocate_nonplt_objects(Obj_Entry *obj)
{
	const Elf_Rela *rela;

	for (rela = obj->rela; rela < obj->relalim; rela++) {
		Elf_Addr *where;
		Elf_Word type, value, mask;
		const Elf_Sym *def = NULL;
		const Obj_Entry *defobj = NULL;
		unsigned long	 symnum;

		where = (Elf_Addr *) (obj->relocbase + rela->r_offset);
		symnum = ELF_R_SYM(rela->r_info);

		type = ELF_R_TYPE(rela->r_info);
		if (type == R_TYPE(NONE))
			continue;

		/* We do JMP_SLOTs in _rtld_bind() below */
		if (type == R_TYPE(JMP_SLOT))
			continue;

		/* COPY relocs are also handled elsewhere */
		if (type == R_TYPE(COPY))
			continue;

		/*
		 * We use the fact that relocation types are an `enum'
		 * Note: R_SPARC_TLS_TPOFF64 is currently numerically largest.
		 */
		if (type > R_TYPE(TLS_TPOFF64))
			return (-1);

		value = rela->r_addend;

		/*
		 * Handle TLS relocations here, they are different.
		 */
		if (RELOC_TLS(type)) {
			switch (type) {
				case R_TYPE(TLS_DTPMOD32):
					def = _rtld_find_symdef(symnum, obj,
					    &defobj, false);
					if (def == NULL)
						return -1;

					*where = (Elf_Addr)defobj->tlsindex;

					rdbg(("TLS_DTPMOD32 %s in %s --> %p",
					    obj->strtab +
					    obj->symtab[symnum].st_name,
					    obj->path, (void *)*where));

					break;

				case R_TYPE(TLS_DTPOFF32):
					def = _rtld_find_symdef(symnum, obj,
					    &defobj, false);
					if (def == NULL)
						return -1;

					*where = (Elf_Addr)(def->st_value
					    + rela->r_addend);

					rdbg(("TLS_DTPOFF32 %s in %s --> %p",
					    obj->strtab +
					        obj->symtab[symnum].st_name,
					    obj->path, (void *)*where));

					break;

				case R_TYPE(TLS_TPOFF32):
					def = _rtld_find_symdef(symnum, obj,
					    &defobj, false);
					if (def == NULL)
						return -1;

					if (!defobj->tls_done &&
						_rtld_tls_offset_allocate(obj))
						     return -1;

					*where = (Elf_Addr)(def->st_value -
			                            defobj->tlsoffset +
						    rela->r_addend);

		                        rdbg(("TLS_TPOFF32 %s in %s --> %p",
		                            obj->strtab +
					    obj->symtab[symnum].st_name,
		                            obj->path, (void *)*where));

	                		break;
			}
			continue;
		}

		/*
		 * If it is no TLS relocation (handled above), we can not
		 * deal with it if it is beyound R_SPARC_6.
		 */
		if (type > R_TYPE(6))
			return (-1);

		/*
		 * Handle relative relocs here, as an optimization.
		 */
		if (type == R_TYPE(RELATIVE)) {
			*where += (Elf_Addr)(obj->relocbase + value);
			rdbg(("RELATIVE in %s --> %p", obj->path,
			    (void *)*where));
			continue;
		}

		if (RELOC_RESOLVE_SYMBOL(type)) {

			/* Find the symbol */
			def = _rtld_find_symdef(symnum, obj, &defobj, false);
			if (def == NULL)
				return (-1);

			/* Add in the symbol's absolute address */
			value += (Elf_Word)(defobj->relocbase + def->st_value);
		}

		if (RELOC_PC_RELATIVE(type)) {
			value -= (Elf_Word)where;
		}

		if (RELOC_BASE_RELATIVE(type)) {
			/*
			 * Note that even though sparcs use `Elf_rela'
			 * exclusively we still need the implicit memory addend
			 * in relocations referring to GOT entries.
			 * Undoubtedly, someone f*cked this up in the distant
			 * past, and now we're stuck with it in the name of
			 * compatibility for all eternity..
			 *
			 * In any case, the implicit and explicit should be
			 * mutually exclusive. We provide a check for that
			 * here.
			 */
#define DIAGNOSTIC
#ifdef DIAGNOSTIC
			if (value != 0 && *where != 0) {
				xprintf("BASE_REL(%s): where=%p, *where 0x%x, "
					"addend=0x%x, base %p\n",
					obj->path, where, *where,
					rela->r_addend, obj->relocbase);
			}
#endif
			value += (Elf_Word)(obj->relocbase + *where);
		}

		mask = RELOC_VALUE_BITMASK(type);
		value >>= RELOC_VALUE_RIGHTSHIFT(type);
		value &= mask;

		if (RELOC_UNALIGNED(type)) {
			/* Handle unaligned relocations. */
			Elf_Addr tmp = 0;
			char *ptr = (char *)where;
			int i, size = RELOC_TARGET_SIZE(type)/8;

			/* Read it in one byte at a time. */
			for (i=0; i<size; i++)
				tmp = (tmp << 8) | ptr[i];

			tmp &= ~mask;
			tmp |= value;

			/* Write it back out. */
			for (i=0; i<size; i++)
				ptr[i] = ((tmp >> (8*i)) & 0xff);
#ifdef RTLD_DEBUG_RELOC
			value = (Elf_Word)tmp;
#endif

		} else {
			*where &= ~mask;
			*where |= value;
#ifdef RTLD_DEBUG_RELOC
			value = (Elf_Word)*where;
#endif
		}
#ifdef RTLD_DEBUG_RELOC
		if (RELOC_RESOLVE_SYMBOL(type)) {
			rdbg(("%s %s in %s --> %p in %s", reloc_names[type],
			    obj->strtab + obj->symtab[symnum].st_name,
			    obj->path, (void *)value, defobj->path));
		} else {
			rdbg(("%s in %s --> %p", reloc_names[type],
			    obj->path, (void *)value));
		}
#endif
	}
	return (0);
}

int
_rtld_relocate_plt_lazy(const Obj_Entry *obj)
{
	return (0);
}

caddr_t
_rtld_bind(const Obj_Entry *obj, Elf_Word reloff)
{
	const Elf_Rela *rela = (const Elf_Rela *)((const uint8_t *)obj->pltrela + reloff);
	Elf_Addr value;
	int err;

	value = 0;	/* XXX gcc */

	_rtld_shared_enter();
	err = _rtld_relocate_plt_object(obj, rela, &value);
	if (err)
		_rtld_die();
	_rtld_shared_exit();

	return (caddr_t)value;
}

int
_rtld_relocate_plt_objects(const Obj_Entry *obj)
{
	const Elf_Rela *rela = obj->pltrela;

	for (; rela < obj->pltrelalim; rela++)
		if (_rtld_relocate_plt_object(obj, rela, NULL) < 0)
			return -1;

	return 0;
}

static inline int
_rtld_relocate_plt_object(const Obj_Entry *obj, const Elf_Rela *rela, Elf_Addr *tp)
{
	const Elf_Sym *def;
	const Obj_Entry *defobj;
	Elf_Word *where = (Elf_Addr *)(obj->relocbase + rela->r_offset);
	Elf_Addr value;
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
	 * At the PLT entry pointed at by `where', we now construct
	 * a direct transfer to the now fully resolved function
	 * address.  The resulting code in the jump slot is:
	 *
	 *	sethi	%hi(roffset), %g1
	 *	sethi	%hi(addr), %g1
	 *	jmp	%g1+%lo(addr)
	 *
	 * We write the third instruction first, since that leaves the
	 * previous `b,a' at the second word in place. Hence the whole
	 * PLT slot can be atomically change to the new sequence by
	 * writing the `sethi' instruction at word 2.
	 */
#define SETHI	0x03000000
#define JMP	0x81c06000
#define NOP	0x01000000
	where[2] = JMP   | (value & 0x000003ff);
	where[1] = SETHI | ((value >> 10) & 0x003fffff);
	__asm volatile("iflush %0+8" : : "r" (where));
	__asm volatile("iflush %0+4" : : "r" (where));

	if (tp)
		*tp = value;

	return 0;
}
