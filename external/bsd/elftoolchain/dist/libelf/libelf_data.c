/*	$NetBSD: libelf_data.c,v 1.2 2014/03/09 16:58:04 christos Exp $	*/

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

#include <sys/cdefs.h>

#include <libelf.h>

#include "_libelf.h"

__RCSID("$NetBSD: libelf_data.c,v 1.2 2014/03/09 16:58:04 christos Exp $");
ELFTC_VCSID("Id: libelf_data.c 2225 2011-11-26 18:55:54Z jkoshy ");

int
_libelf_xlate_shtype(uint32_t sht)
{
	switch (sht) {
	case SHT_DYNAMIC:
		return (ELF_T_DYN);
	case SHT_DYNSYM:
		return (ELF_T_SYM);
	case SHT_FINI_ARRAY:
		return (ELF_T_ADDR);
	case SHT_GNU_HASH:
		return (ELF_T_GNUHASH);
	case SHT_GNU_LIBLIST:
		return (ELF_T_WORD);
	case SHT_GROUP:
		return (ELF_T_WORD);
	case SHT_HASH:
		return (ELF_T_WORD);
	case SHT_INIT_ARRAY:
		return (ELF_T_ADDR);
	case SHT_NOBITS:
		return (ELF_T_BYTE);
	case SHT_NOTE:
		return (ELF_T_NOTE);
	case SHT_PREINIT_ARRAY:
		return (ELF_T_ADDR);
	case SHT_PROGBITS:
		return (ELF_T_BYTE);
	case SHT_REL:
		return (ELF_T_REL);
	case SHT_RELA:
		return (ELF_T_RELA);
	case SHT_STRTAB:
		return (ELF_T_BYTE);
	case SHT_SYMTAB:
		return (ELF_T_SYM);
	case SHT_SYMTAB_SHNDX:
		return (ELF_T_WORD);
	case SHT_SUNW_dof:
		return (ELF_T_BYTE);
	case SHT_SUNW_move:
		return (ELF_T_MOVE);
	case SHT_SUNW_syminfo:
		return (ELF_T_SYMINFO);
	case SHT_SUNW_verdef:	/* == SHT_GNU_verdef */
		return (ELF_T_VDEF);
	case SHT_SUNW_verneed:	/* == SHT_GNU_verneed */
		return (ELF_T_VNEED);
	case SHT_SUNW_versym:	/* == SHT_GNU_versym */
		return (ELF_T_HALF);

	case SHT_ARM_PREEMPTMAP:
	case SHT_ARM_ATTRIBUTES:
	case SHT_ARM_DEBUGOVERLAY:
	case SHT_ARM_OVERLAYSECTION:
	case SHT_MIPS_DWARF:
	case SHT_MIPS_REGINFO:
	case SHT_MIPS_OPTIONS:
	case SHT_AMD64_UNWIND:	/* == SHT_IA_64_UNWIND == SHT_ARM_EXIDX */
		return (ELF_T_BYTE);

	default:
		return (-1);
	}
}
