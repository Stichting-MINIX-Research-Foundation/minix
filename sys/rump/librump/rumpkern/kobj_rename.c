/*	$NetBSD: kobj_rename.c,v 1.2 2014/04/25 18:31:35 pooka Exp $	*/

/*-
 * Copyright (c) 2010 Antti Kantee.  All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kobj_rename.c,v 1.2 2014/04/25 18:31:35 pooka Exp $");

#define ELFSIZE ARCH_ELFSIZE

#include <sys/param.h>
#include <sys/exec_elf.h>
#include <sys/kmem.h>
#include <sys/kobj.h>
#include <sys/systm.h>

#include <rump/rump.h>

/*
 * Mangle symbols into rump kernel namespace.  This means
 * putting "rumpns" in front of select symbols.
 * See src/sys/rump/Makefile.rump for more details on the rump kernel
 * namespace.
 */
const char *norentab[] = {
	"RUMP",
	"rump",
	"__",
	"_GLOBAL_OFFSET_TABLE",
};
static int
norename(const char *name)
{
	unsigned i;

	for (i = 0; i < __arraycount(norentab); i++) {
		if (strncmp(norentab[i], name, strlen(norentab[i])) == 0)
			return 1;
	}
	return 0;
}

#define RUMPNS "rumpns_"
int
kobj_renamespace(Elf_Sym *symtab, size_t symcount,
	char **strtab, size_t *strtabsz)
{
	Elf_Sym *sym;
	char *worktab, *newtab;
	size_t worktabsz, worktabidx;
	unsigned i;
	const size_t prefixlen = strlen(RUMPNS);
	static int warned;

	if (!rump_nativeabi_p() && !warned) {
		printf("warning: kernel ABI not supported on this arch\n");
		warned = 1;
	}

	/* allocate space for worst-case stringtab */
	worktabsz = *strtabsz + symcount * prefixlen;
	worktab = kmem_alloc(worktabsz, KM_SLEEP);

	/* now, adjust stringtab into temporary space */
#define WORKTABP (worktab + worktabidx)
	for (i = 0, worktabidx = 0; i < symcount; i++) {
		const char *fromname;

		sym = &symtab[i];
		if (sym->st_name == 0) {
			continue;
		}

		fromname = *strtab + sym->st_name;
		sym->st_name = worktabidx;

		if (ELF_ST_BIND(sym->st_info) != STB_GLOBAL ||
		    norename(fromname)) {
			strcpy(WORKTABP, fromname);
			worktabidx += strlen(fromname) + 1;
		} else {
			strcpy(WORKTABP, RUMPNS);
			worktabidx += prefixlen;
			strcpy(WORKTABP, fromname);
			worktabidx += strlen(fromname) + 1;
		}
		KASSERT(worktabidx <= worktabsz);
	}
#undef WORKTABP

	/*
	 * Finally, free old strtab, allocate new one, and copy contents.
	 */
	kmem_free(*strtab, *strtabsz);
	*strtab = NULL; /* marvin the paradroid 999 */
	newtab = kmem_alloc(worktabidx, KM_SLEEP);
	memcpy(newtab, worktab, worktabidx);

	kmem_free(worktab, worktabsz);

	*strtab = newtab;
	*strtabsz = worktabidx;

	return 0;
}
