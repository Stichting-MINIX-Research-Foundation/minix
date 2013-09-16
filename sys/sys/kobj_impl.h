/*	$NetBSD: kobj_impl.h,v 1.3 2011/08/13 21:04:07 christos Exp $	*/

/*-
 * Copyright (c) 2008, 2009 The NetBSD Foundation, Inc.
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
 * Copyright (c) 1998-2000 Doug Rabson
 * Copyright (c) 2004 Peter Wemm
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

/*
 * Data structures private to kobj, shared only with kernel grovellers.
 */

#ifndef _SYS_KOBJ_IMPL_H_
#define	_SYS_KOBJ_IMPL_H_

#define	ELFSIZE		ARCH_ELFSIZE

#include <sys/systm.h>
#include <sys/kobj.h>
#include <sys/exec.h>
#include <sys/exec_elf.h>
#include <sys/module.h>

typedef struct {
	void		*addr;
	Elf_Off		size;
	int		flags;
	int		sec;		/* Original section */
	const char	*name;
} progent_t;

typedef struct {
	Elf_Rel		*rel;
	int 		nrel;
	int 		sec;
	size_t		size;
} relent_t;

typedef struct {
	Elf_Rela	*rela;
	int		nrela;
	int		sec;
	size_t		size;
} relaent_t;

typedef enum kobjtype {
	KT_UNSET,
	KT_VNODE,
	KT_MEMORY
} kobjtype_t;

typedef int (*kobj_read_fn)(kobj_t, void **, size_t, off_t, bool);
typedef void (*kobj_close_fn)(kobj_t);

struct kobj {
	char		ko_name[MAXMODNAME];
	kobjtype_t	ko_type;
	void		*ko_source;
	ssize_t		ko_memsize;
	vaddr_t		ko_address;	/* Relocation address */
	Elf_Shdr	*ko_shdr;
	progent_t	*ko_progtab;
	relaent_t	*ko_relatab;
	relent_t	*ko_reltab;
	Elf_Sym		*ko_symtab;	/* Symbol table */
	char		*ko_strtab;	/* String table */
	char		*ko_shstrtab;	/* Section name string table */
	size_t		ko_size;	/* Size of text/data/bss */
	size_t		ko_symcnt;	/* Number of symbols */
	size_t		ko_strtabsz;	/* Number of bytes in string table */
	size_t		ko_shstrtabsz;	/* Number of bytes in scn str table */
	size_t		ko_shdrsz;
	int		ko_nrel;
	int		ko_nrela;
	int		ko_nprogtab;
	bool		ko_ksyms;
	bool		ko_loaded;
	kobj_read_fn	ko_read;
	kobj_close_fn	ko_close;
};

#ifdef _KERNEL
int	kobj_load(kobj_t);
void	kobj_setname(kobj_t, const char *);
#endif

#endif	/* _SYS_KOBJ_IMPL_H_ */
