/*      $NetBSD: rumpuser_dl.c,v 1.30 2014/11/04 19:05:17 pooka Exp $	*/

/*
 * Copyright (c) 2009 Antti Kantee.  All Rights Reserved.
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

/*
 * Load all module link sets and feed symbol table to the kernel.
 * Called during rump bootstrap.
 */

/*
 * Solaris libelf.h doesn't support _FILE_OFFSET_BITS=64.  Luckily,
 * for this module it doesn't matter.
 */
#if defined(__sun__)
#define RUMPUSER_NO_FILE_OFFSET_BITS
#endif
#include "rumpuser_port.h"

#if !defined(lint)
__RCSID("$NetBSD: rumpuser_dl.c,v 1.30 2014/11/04 19:05:17 pooka Exp $");
#endif /* !lint */

#include <sys/types.h>
#include <sys/time.h>
#include <assert.h>

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rump/rumpuser.h>

#if defined(__ELF__) && defined(HAVE_DLINFO)
#include <elf.h>
#include <link.h>

static size_t symtabsize = 0, strtabsize = 0;
static size_t symtaboff = 0, strtaboff = 0;
static uint8_t *symtab = NULL;
static char *strtab = NULL;
static unsigned char eident;

/* nb5 compat */
#ifndef Elf_Symindx
#define Elf_Symindx uint32_t
#endif

static void *
reservespace(void *store, size_t *storesize,
	size_t storeoff, size_t required)
{
	size_t chunk, newsize;

	assert(storeoff <= *storesize);
	chunk = *storesize - storeoff;

	if (chunk >= required)
		return store;

	newsize = *storesize + ((size_t)required - chunk);
	store = realloc(store, newsize);
	if (store == NULL) {
		return NULL;
	}
	*((uint8_t *)store + storeoff) = '\0';
	*storesize = newsize;

	return store;
}

/*
 * Macros to make handling elf32/64 in the code a little saner.
 */

#define DYNn_GETMEMBER(base, n, thevar, result)				\
do {									\
	if (eident == ELFCLASS32) {					\
		const Elf32_Dyn *dyn = base;				\
		/*LINTED*/						\
		result = dyn[n].thevar;					\
	} else {							\
		const Elf64_Dyn *dyn = base;				\
		/*LINTED*/						\
		result = dyn[n].thevar;					\
	}								\
} while (/*CONSTCOND*/0)

#define SYMn_GETMEMBER(base, n, thevar, result)				\
do {									\
	if (eident == ELFCLASS32) {					\
		const Elf32_Sym *sym = base;				\
		/*LINTED*/						\
		result = sym[n].thevar;					\
	} else {							\
		const Elf64_Sym *sym = base;				\
		/*LINTED*/						\
		result = sym[n].thevar;					\
	}								\
} while (/*CONSTCOND*/0)

#define SYMn_SETMEMBER(base, n, thevar, value)				\
do {									\
	if (eident == ELFCLASS32) {					\
		Elf32_Sym *sym = base;					\
		/*LINTED*/						\
		sym[n].thevar = value;					\
	} else {							\
		Elf64_Sym *sym = base;					\
		/*LINTED*/						\
		sym[n].thevar = value;					\
	}								\
} while (/*CONSTCOND*/0)

#define SYM_GETSIZE() ((eident==ELFCLASS32)?sizeof(Elf32_Sym):sizeof(Elf64_Sym))

/*
 * On NetBSD, the dynamic section pointer values seem to be relative to
 * the address the dso is mapped at.  On glibc, they seem to contain
 * the absolute address.  I couldn't find anything definite from a quick
 * read of the standard and therefore I will not go and figure beyond ifdef.
 * On Solaris and DragonFly / FreeBSD, the main object works differently
 * ... uuuuh.
 */
#if defined(__GLIBC__) && !defined(__mips__)
#define adjptr(_map_, _ptr_) ((void *)(_ptr_))
#elif defined(__sun__) || defined(__DragonFly__) || defined(__FreeBSD__)
#define adjptr(_map_, _ptr_) \
    (ismainobj ? (void *)(_ptr_) : (void *)(_map_->l_addr + (_ptr_)))
#else
/* NetBSD and some others, e.g. Linux + musl */
#define adjptr(_map_, _ptr_) ((void *)(_map_->l_addr + (_ptr_)))
#endif

static int
getsymbols(struct link_map *map, int ismainobj)
{
	char *str_base;
	void *syms_base = NULL; /* XXXgcc */
	size_t curstrsize;
	const void *ed_base;
	uint64_t ed_tag;
	size_t cursymcount;
	unsigned i;

	if (map->l_addr) {
		if (memcmp((void *)map->l_addr, ELFMAG, SELFMAG) != 0)
			return ENOEXEC;
		eident = *(unsigned char *)(map->l_addr + EI_CLASS);
		if (eident != ELFCLASS32 && eident != ELFCLASS64)
			return ENOEXEC;
	}

	/*
	 * ok, we probably have only the main object.  instead of going
	 * to disk and reading the ehdr, just try to guess the size.
	 */
	if (eident == 0) {
		if (/*CONSTCOND*/sizeof(void *) == 4)
			eident = ELFCLASS32;
		else
			eident = ELFCLASS64;
	}

	/*
	 * Find symtab and strtab and their sizes.
	 */
	str_base = NULL;
	curstrsize = 0;
	cursymcount = 0;
	ed_base = map->l_ld;
	DYNn_GETMEMBER(ed_base, 0, d_tag, ed_tag);
	for (i = 0; ed_tag != DT_NULL;) {
		uintptr_t edptr;
		size_t edval;
		Elf_Symindx *hashtab;

		switch (ed_tag) {
		case DT_SYMTAB:
			DYNn_GETMEMBER(ed_base, i, d_un.d_ptr, edptr);
			syms_base = adjptr(map, edptr);
			break;
		case DT_STRTAB:
			DYNn_GETMEMBER(ed_base, i, d_un.d_ptr, edptr);
			str_base = adjptr(map, edptr);
			break;
		case DT_STRSZ:
			DYNn_GETMEMBER(ed_base, i, d_un.d_val, edval);
			curstrsize = edval;
			break;
		case DT_HASH:
			DYNn_GETMEMBER(ed_base, i, d_un.d_ptr, edptr);
			hashtab = (Elf_Symindx *)adjptr(map, edptr);
			cursymcount = hashtab[1];
			break;
#ifdef DT_GNU_HASH
		/*
		 * DT_GNU_HASH is a bit more complicated than DT_HASH
		 * in this regard since apparently there is no field
		 * telling us the total symbol count.  Instead, we look
		 * for the last valid hash bucket and add its chain lenght
		 * to the bucket's base index.
		 */
		case DT_GNU_HASH: {
			Elf32_Word nbuck, symndx, maskwords, maxchain = 0;
			Elf32_Word *gnuhash, *buckets, *ptr;
			int bi;

			DYNn_GETMEMBER(ed_base, i, d_un.d_ptr, edptr);
			gnuhash = (Elf32_Word *)adjptr(map, edptr);

			nbuck = gnuhash[0];
			symndx = gnuhash[1];
			maskwords = gnuhash[2];

			/*
			 * First, find the last valid bucket and grab its index
			 */
			if (eident == ELFCLASS64)
				maskwords *= 2; /* sizeof(*buckets) == 4 */
			buckets = gnuhash + 4 + maskwords;
			for (bi = nbuck-1; bi >= 0; bi--) {
				if (buckets[bi] != 0) {
					maxchain = buckets[bi];
					break;
				}
			}
			if (maxchain == 0 || maxchain < symndx)
				break;

			/*
			 * Then, traverse the last chain and count symbols.
			 */

			cursymcount = maxchain;
			ptr = buckets + nbuck + (maxchain - symndx);
			do {
				cursymcount++;
			} while ((*ptr++ & 1) == 0);
		}	
			break;
#endif
		case DT_SYMENT:
			DYNn_GETMEMBER(ed_base, i, d_un.d_val, edval);
			assert(edval == SYM_GETSIZE());
			break;
		default:
			break;
		}
		i++;
		DYNn_GETMEMBER(ed_base, i, d_tag, ed_tag);
	}

	if (str_base == NULL || syms_base == NULL ||
	    curstrsize == 0 || cursymcount == 0) {
		fprintf(stderr, "could not find strtab, symtab or their sizes "
		    "in %s\n", map->l_name);
		return ENOEXEC;
	}

	/*
	 * Make sure we have enough space for the contents of the symbol
	 * and string tables we are currently processing.  The total used
	 * space will be smaller due to undefined symbols we are not
	 * interested in.
	 */
	symtab = reservespace(symtab, &symtabsize,
	    symtaboff, cursymcount * SYM_GETSIZE());
	strtab = reservespace(strtab, &strtabsize, strtaboff, curstrsize);
	if (symtab == NULL || strtab == NULL) {
		fprintf(stderr, "failed to reserve memory");
		return ENOMEM;
	}

	/* iterate over all symbols in current symtab */
	for (i = 0; i < cursymcount; i++) {
		const char *cursymname;
		int shndx, name;
		uintptr_t value;
		void *csym;

		SYMn_GETMEMBER(syms_base, i, st_shndx, shndx);
		SYMn_GETMEMBER(syms_base, i, st_value, value);
		if (shndx == SHN_UNDEF || value == 0)
			continue;

		/* get symbol name */
		SYMn_GETMEMBER(syms_base, i, st_name, name);
		cursymname = name + str_base;

		/*
		 * Only accept symbols which are decidedly in
		 * the rump kernel namespace.
		 * XXX: quirks, but they wouldn't matter here
		 */
		if (strncmp(cursymname, "rump", 4) != 0 &&
		    strncmp(cursymname, "RUMP", 4) != 0 &&
		    strncmp(cursymname, "__", 2) != 0) {
			continue;
		}

		memcpy(symtab + symtaboff,
		    (const uint8_t *)syms_base + i*SYM_GETSIZE(),SYM_GETSIZE());

		/*
		 * set name to point at new strtab, offset symbol value
		 * with lib base address.
		 */
		csym = symtab + symtaboff;
		SYMn_SETMEMBER(csym, 0, st_name, strtaboff);
		SYMn_GETMEMBER(csym, 0, st_value, value);
		SYMn_SETMEMBER(csym, 0, st_value,(intptr_t)(value+map->l_addr));
		symtaboff += SYM_GETSIZE();

		strcpy(strtab + strtaboff, cursymname);
		strtaboff += strlen(cursymname)+1;
	}

	return 0;
}

static void
process_object(void *handle,
	rump_modinit_fn domodinit, rump_compload_fn docompload)
{
	const struct modinfo *const *mi_start, *const *mi_end;
	struct rump_component *const *rc, *const *rc_end;

	mi_start = dlsym(handle, "__start_link_set_modules");
	mi_end = dlsym(handle, "__stop_link_set_modules");
	if (mi_start && mi_end)
		domodinit(mi_start, (size_t)(mi_end-mi_start));

	rc = dlsym(handle, "__start_link_set_rump_components");
	rc_end = dlsym(handle, "__stop_link_set_rump_components");
	if (rc && rc_end) {
		for (; rc < rc_end; rc++)
			docompload(*rc);
		assert(rc == rc_end);
	}
}

/*
 * Get the linkmap from the dynlinker.  Try to load kernel modules
 * from all objects in the linkmap.
 */
void
rumpuser_dl_bootstrap(rump_modinit_fn domodinit,
	rump_symload_fn symload, rump_compload_fn compload)
{
	struct link_map *map, *origmap, *mainmap;
	void *mainhandle;
	int error;

	mainhandle = dlopen(NULL, RTLD_NOW);
	/* Will be null if statically linked so just return */
	if (mainhandle == NULL)
		return;
	if (dlinfo(mainhandle, RTLD_DI_LINKMAP, &mainmap) == -1) {
		fprintf(stderr, "warning: rumpuser module bootstrap "
		    "failed: %s\n", dlerror());
		return;
	}
	origmap = mainmap;

	/*
	 * Use a heuristic to determine if we are static linked.
	 * A dynamically linked binary should always have at least
	 * two objects: itself and ld.so.
	 *
	 * In a statically linked binary with glibc the linkmap
	 * contains some "info" that leads to a segfault.  Since we
	 * can't really do anything useful in here without ld.so, just
	 * simply bail and let the symbol references in librump do the
	 * right things.
	 */
	if (origmap->l_next == NULL && origmap->l_prev == NULL) {
		dlclose(mainhandle);
		return;
	}

	/*
	 * Process last->first because that's the most probable
	 * order for dependencies
	 */
	for (; origmap->l_next; origmap = origmap->l_next)
		continue;

	/*
	 * Build symbol table to hand to the rump kernel.  Do this by
	 * iterating over all rump libraries and collecting symbol
	 * addresses and relocation info.
	 */
	error = 0;
	for (map = origmap; map && !error; map = map->l_prev) {
		if (strstr(map->l_name, "librump") != NULL || map == mainmap)
			error = getsymbols(map, map == mainmap);
	}

	if (error == 0) {
		void *trimmedsym, *trimmedstr;

		/*
		 * Allocate optimum-sized memory for storing tables
		 * and feed to kernel.  If memory allocation fails,
		 * just give the ones with extra context (although
		 * I'm pretty sure we'll die moments later due to
		 * memory running out).
		 */
		if ((trimmedsym = malloc(symtaboff)) != NULL) {
			memcpy(trimmedsym, symtab, symtaboff);
		} else {
			trimmedsym = symtab;
			symtab = NULL;
		}
		if ((trimmedstr = malloc(strtaboff)) != NULL) {
			memcpy(trimmedstr, strtab, strtaboff);
		} else {
			trimmedstr = strtab;
			strtab = NULL;
		}
		symload(trimmedsym, symtaboff, trimmedstr, strtaboff);
	}
	free(symtab);
	free(strtab);

	/*
	 * Next, load modules and components.
	 *
	 * Simply loop through all objects, ones unrelated to rump kernels
	 * will not contain link_set_rump_components (well, not including
	 * "sabotage", but that needs to be solved at another level anyway).
	 */
	for (map = origmap; map; map = map->l_prev) {
		void *handle;

		if (map == mainmap) {
			handle = mainhandle;
		} else {
			handle = dlopen(map->l_name, RTLD_LAZY);
			if (handle == NULL)
				continue;
		}
		process_object(handle, domodinit, compload);
		if (map != mainmap)
			dlclose(handle);
	}
}
#else
/*
 * no dynamic linking supported
 */
void
rumpuser_dl_bootstrap(rump_modinit_fn domodinit,
	rump_symload_fn symload, rump_compload_fn compload)
{

	return;
}
#endif
