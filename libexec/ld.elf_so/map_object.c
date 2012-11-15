/*	$NetBSD: map_object.c,v 1.45 2012/10/13 21:13:07 dholland Exp $	 */

/*
 * Copyright 1996 John D. Polstra.
 * Copyright 1996 Matt Thomas <matt@3am-software.com>
 * Copyright 2002 Charles M. Hannum <root@ihack.net>
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
__RCSID("$NetBSD: map_object.c,v 1.45 2012/10/13 21:13:07 dholland Exp $");
#endif /* not lint */

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>

#include "debug.h"
#include "rtld.h"

#ifdef __minix
#define munmap minix_munmap
#endif

#define MINIXVERBOSE 0

#if MINIXVERBOSE
#include <stdio.h>
#endif

static int protflags(int);	/* Elf flags -> mmap protection */

#define EA_UNDEF		(~(Elf_Addr)0)

static void Pread(void *addr, size_t size, int fd, off_t off)
{
	int s;
	if((s=pread(fd,addr, size, off)) < 0) {
		_rtld_error("pread failed");
		exit(1);
	}

#if MINIXVERBOSE
	fprintf(stderr, "read 0x%lx bytes from offset 0x%lx to addr 0x%lx\n", size, off, addr);
#endif
}

/*
 * Map a shared object into memory.  The argument is a file descriptor,
 * which must be open on the object and positioned at its beginning.
 *
 * The return value is a pointer to a newly-allocated Obj_Entry structure
 * for the shared object.  Returns NULL on failure.
 */
Obj_Entry *
_rtld_map_object(const char *path, int fd, const struct stat *sb)
{
	Obj_Entry	*obj;
	Elf_Ehdr	*ehdr;
	Elf_Phdr	*phdr;
#if defined(__HAVE_TLS_VARIANT_I) || defined(__HAVE_TLS_VARIANT_II)
	Elf_Phdr	*phtls;
#endif
	size_t		 phsize;
	Elf_Phdr	*phlimit;
	Elf_Phdr	*segs[2];
	int		 nsegs;
	caddr_t		 mapbase = MAP_FAILED;
	size_t		 mapsize = 0;
	int		 mapflags;
	Elf_Off		 base_offset;
#ifdef MAP_ALIGNED
	Elf_Addr	 base_alignment;
#endif
	Elf_Addr	 base_vaddr;
	Elf_Addr	 base_vlimit;
	Elf_Addr	 text_vlimit;
	int		 text_flags;
	caddr_t		 base_addr;
	Elf_Off		 data_offset;
	Elf_Addr	 data_vaddr;
	Elf_Addr	 data_vlimit;
	int		 data_flags;
	caddr_t		 data_addr;
#if defined(__HAVE_TLS_VARIANT_I) || defined(__HAVE_TLS_VARIANT_II)
	Elf_Addr	 tls_vaddr = 0; /* Noise GCC */
#endif
	Elf_Addr	 phdr_vaddr;
	size_t		 phdr_memsz;
#if defined(__HAVE_TLS_VARIANT_I) || defined(__HAVE_TLS_VARIANT_II)
	caddr_t		 gap_addr;
	size_t		 gap_size;
#endif
	int i;
#ifdef RTLD_LOADER
	Elf_Addr	 clear_vaddr;
	caddr_t		 clear_addr;
	size_t		 nclear;
#endif

	if (sb != NULL && sb->st_size < (off_t)sizeof (Elf_Ehdr)) {
		_rtld_error("%s: not ELF file (too short)", path);
		return NULL;
	}

	obj = _rtld_obj_new();
	obj->path = xstrdup(path);
	obj->pathlen = strlen(path);
	if (sb != NULL) {
		obj->dev = sb->st_dev;
		obj->ino = sb->st_ino;
	}

#ifdef __minix
	ehdr = minix_mmap(NULL, _rtld_pagesz, PROT_READ|PROT_WRITE,
		MAP_PREALLOC|MAP_ANON, -1, (off_t)0);
	Pread(ehdr, _rtld_pagesz, fd, 0);
#if MINIXVERBOSE
	fprintf(stderr, "minix mmap for header: 0x%lx\n", ehdr);
#endif
#else
	ehdr = mmap(NULL, _rtld_pagesz, PROT_READ, MAP_FILE | MAP_SHARED, fd,
	    (off_t)0);
#endif
	obj->ehdr = ehdr;
	if (ehdr == MAP_FAILED) {
		_rtld_error("%s: read error: %s", path, xstrerror(errno));
		goto bad;
	}
	/* Make sure the file is valid */
	if (memcmp(ELFMAG, ehdr->e_ident, SELFMAG) != 0) {
		_rtld_error("%s: not ELF file (magic number bad)", path);
		goto bad;
	}
	if (ehdr->e_ident[EI_CLASS] != ELFCLASS) {
		_rtld_error("%s: invalid ELF class %x; expected %x", path,
		    ehdr->e_ident[EI_CLASS], ELFCLASS);
		goto bad;
	}
	/* Elf_e_ident includes class */
	if (ehdr->e_ident[EI_VERSION] != EV_CURRENT ||
	    ehdr->e_version != EV_CURRENT ||
	    ehdr->e_ident[EI_DATA] != ELFDEFNNAME(MACHDEP_ENDIANNESS)) {
		_rtld_error("%s: unsupported file version", path);
		goto bad;
	}
	if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
		_rtld_error("%s: unsupported file type", path);
		goto bad;
	}
	switch (ehdr->e_machine) {
		ELFDEFNNAME(MACHDEP_ID_CASES)
	default:
		_rtld_error("%s: unsupported machine", path);
		goto bad;
	}

	/*
         * We rely on the program header being in the first page.  This is
         * not strictly required by the ABI specification, but it seems to
         * always true in practice.  And, it simplifies things considerably.
         */
	assert(ehdr->e_phentsize == sizeof(Elf_Phdr));
	assert(ehdr->e_phoff + ehdr->e_phnum * sizeof(Elf_Phdr) <=
	    _rtld_pagesz);

	/*
         * Scan the program header entries, and save key information.
         *
         * We rely on there being exactly two load segments, text and data,
         * in that order.
         */
	phdr = (Elf_Phdr *) ((caddr_t)ehdr + ehdr->e_phoff);
#if defined(__HAVE_TLS_VARIANT_I) || defined(__HAVE_TLS_VARIANT_II)
	phtls = NULL;
#endif
	phsize = ehdr->e_phnum * sizeof(phdr[0]);
	obj->phdr = NULL;
	phdr_vaddr = EA_UNDEF;
	phdr_memsz = 0;
	phlimit = phdr + ehdr->e_phnum;
	nsegs = 0;
	while (phdr < phlimit) {
		switch (phdr->p_type) {
		case PT_INTERP:
			obj->interp = (void *)(uintptr_t)phdr->p_vaddr;
 			dbg(("%s: PT_INTERP %p", obj->path, obj->interp));
			break;

		case PT_LOAD:
			if (nsegs < 2)
				segs[nsegs] = phdr;
			++nsegs;

#if ELFSIZE == 64
#define	PRImemsz	PRIu64
#else
#define PRImemsz	PRIu32
#endif
			dbg(("%s: %s %p phsize %" PRImemsz, obj->path, "PT_LOAD",
			    (void *)(uintptr_t)phdr->p_vaddr, phdr->p_memsz));
			break;

		case PT_PHDR:
			phdr_vaddr = phdr->p_vaddr;
			phdr_memsz = phdr->p_memsz;
			dbg(("%s: %s %p phsize %" PRImemsz, obj->path, "PT_PHDR",
			    (void *)(uintptr_t)phdr->p_vaddr, phdr->p_memsz));
			break;
		
		case PT_DYNAMIC:
			obj->dynamic = (void *)(uintptr_t)phdr->p_vaddr;
			dbg(("%s: %s %p phsize %" PRImemsz, obj->path, "PT_DYNAMIC",
			    (void *)(uintptr_t)phdr->p_vaddr, phdr->p_memsz));
			break;

#if defined(__HAVE_TLS_VARIANT_I) || defined(__HAVE_TLS_VARIANT_II)
		case PT_TLS:
			phtls = phdr;
			dbg(("%s: %s %p phsize %" PRImemsz, obj->path, "PT_TLS",
			    (void *)(uintptr_t)phdr->p_vaddr, phdr->p_memsz));
			break;
#endif
		}

		++phdr;
	}
	phdr = (Elf_Phdr *) ((caddr_t)ehdr + ehdr->e_phoff);
	obj->entry = (void *)(uintptr_t)ehdr->e_entry;
	if (!obj->dynamic) {
		_rtld_error("%s: not dynamically linked", path);
		goto bad;
	}
	if (nsegs != 2) {
		_rtld_error("%s: wrong number of segments (%d != 2)", path,
		    nsegs);
		goto bad;
	}

	/*
	 * Map the entire address space of the object as a file
	 * region to stake out our contiguous region and establish a
	 * base for relocation.  We use a file mapping so that
	 * the kernel will give us whatever alignment is appropriate
	 * for the platform we're running on.
	 *
	 * We map it using the text protection, map the data segment
	 * into the right place, then map an anon segment for the bss
	 * and unmap the gaps left by padding to alignment.
	 */

#ifdef MAP_ALIGNED
	base_alignment = segs[0]->p_align;
#endif
	base_offset = round_down(segs[0]->p_offset);
	base_vaddr = round_down(segs[0]->p_vaddr);
	base_vlimit = round_up(segs[1]->p_vaddr + segs[1]->p_memsz);
	text_vlimit = round_up(segs[0]->p_vaddr + segs[0]->p_memsz);
	text_flags = protflags(segs[0]->p_flags);
	data_offset = round_down(segs[1]->p_offset);
	data_vaddr = round_down(segs[1]->p_vaddr);
	data_vlimit = round_up(segs[1]->p_vaddr + segs[1]->p_filesz);
	data_flags = protflags(segs[1]->p_flags);
#ifdef RTLD_LOADER
	clear_vaddr = segs[1]->p_vaddr + segs[1]->p_filesz;
#endif

	obj->textsize = text_vlimit - base_vaddr;
	obj->vaddrbase = base_vaddr;
	obj->isdynamic = ehdr->e_type == ET_DYN;

#if defined(__HAVE_TLS_VARIANT_I) || defined(__HAVE_TLS_VARIANT_II)
	if (phtls != NULL) {
		++_rtld_tls_dtv_generation;
		obj->tlsindex = ++_rtld_tls_max_index;
		obj->tlssize = phtls->p_memsz;
		obj->tlsalign = phtls->p_align;
		obj->tlsinitsize = phtls->p_filesz;
		tls_vaddr = phtls->p_vaddr;
	}
#endif

	obj->phdr_loaded = false;
	for (i = 0; i < nsegs; i++) {
		if (phdr_vaddr != EA_UNDEF &&
		    segs[i]->p_vaddr <= phdr_vaddr &&
		    segs[i]->p_memsz >= phdr_memsz) {
			obj->phdr_loaded = true;
			break;
		}
		if (segs[i]->p_offset <= ehdr->e_phoff &&
		    segs[i]->p_memsz >= phsize) {
			phdr_vaddr = segs[i]->p_vaddr + ehdr->e_phoff;
			phdr_memsz = phsize;
			obj->phdr_loaded = true;
			break;
		}
	}
	if (obj->phdr_loaded) {
		obj->phdr = (void *)(uintptr_t)phdr_vaddr;
		obj->phsize = phdr_memsz;
	} else {
		Elf_Phdr *buf;
		buf = xmalloc(phsize);
		if (buf == NULL) {
			_rtld_error("%s: cannot allocate program header", path);
			goto bad;
		}
		memcpy(buf, phdr, phsize);
		obj->phdr = buf;
		obj->phsize = phsize;
	}
	dbg(("%s: phdr %p phsize %zu (%s)", obj->path, obj->phdr, obj->phsize,
	     obj->phdr_loaded ? "loaded" : "allocated"));

	/* Unmap header if it overlaps the first load section. */
	if (base_offset < _rtld_pagesz) {
		munmap(ehdr, _rtld_pagesz);
		obj->ehdr = MAP_FAILED;
	}

	/*
	 * Calculate log2 of the base section alignment.
	 */
	mapflags = 0;
#ifdef MAP_ALIGNED
	if (base_alignment > _rtld_pagesz) {
		unsigned int log2 = 0;
		for (; base_alignment > 1; base_alignment >>= 1)
			log2++;
		mapflags = MAP_ALIGNED(log2);
	}
#endif

#ifdef RTLD_LOADER
	base_addr = obj->isdynamic ? NULL : (caddr_t)base_vaddr;
#else
	base_addr = NULL;
#endif
	mapsize = base_vlimit - base_vaddr;

#ifndef __minix
	mapbase = mmap(base_addr, mapsize, text_flags,
	    mapflags | MAP_FILE | MAP_PRIVATE, fd, base_offset);
#else
	mapbase = minix_mmap(base_addr, mapsize, PROT_READ|PROT_WRITE,
	    MAP_ANON | MAP_PREALLOC, -1, 0);
#if MINIXVERBOSE
	fprintf(stderr, "minix mmap for whole block: 0x%lx-0x%lx\n", mapbase, mapbase+mapsize);
#endif
	Pread(mapbase, obj->textsize, fd, 0);
#endif
	if (mapbase == MAP_FAILED) {
		_rtld_error("mmap of entire address space failed: %s",
		    xstrerror(errno));
		goto bad;
	}

	/* Overlay the data segment onto the proper region. */
	data_addr = mapbase + (data_vaddr - base_vaddr);
#ifdef __minix
	Pread(data_addr, data_vlimit - data_vaddr, fd, data_offset);
#else
	if (mmap(data_addr, data_vlimit - data_vaddr, data_flags,
	    MAP_FILE | MAP_PRIVATE | MAP_FIXED, fd, data_offset) ==
	    MAP_FAILED) {
		_rtld_error("mmap of data failed: %s", xstrerror(errno));
		goto bad;
	}

	/* Overlay the bss segment onto the proper region. */
	if (mmap(mapbase + data_vlimit - base_vaddr, base_vlimit - data_vlimit,
	    data_flags, MAP_ANON | MAP_PRIVATE | MAP_FIXED, -1, 0) ==
	    MAP_FAILED) {
		_rtld_error("mmap of bss failed: %s", xstrerror(errno));
		goto bad;
	}

	/* Unmap the gap between the text and data. */
	gap_addr = mapbase + round_up(text_vlimit - base_vaddr);
	gap_size = data_addr - gap_addr;
	if (gap_size != 0 && mprotect(gap_addr, gap_size, PROT_NONE) == -1) {
		_rtld_error("mprotect of text -> data gap failed: %s",
		    xstrerror(errno));
		goto bad;
	}
#endif

#ifdef RTLD_LOADER
	/* Clear any BSS in the last page of the data segment. */
	clear_addr = mapbase + (clear_vaddr - base_vaddr);
	if ((nclear = data_vlimit - clear_vaddr) > 0)
		memset(clear_addr, 0, nclear);

	/* Non-file portion of BSS mapped above. */
#endif

#if defined(__HAVE_TLS_VARIANT_I) || defined(__HAVE_TLS_VARIANT_II)
	if (phtls != NULL)
		obj->tlsinit = mapbase + tls_vaddr;
#endif

	obj->mapbase = mapbase;
	obj->mapsize = mapsize;
	obj->relocbase = mapbase - base_vaddr;

	if (obj->dynamic)
		obj->dynamic = (void *)(obj->relocbase + (Elf_Addr)(uintptr_t)obj->dynamic);
	if (obj->entry)
		obj->entry = (void *)(obj->relocbase + (Elf_Addr)(uintptr_t)obj->entry);
	if (obj->interp)
		obj->interp = (void *)(obj->relocbase + (Elf_Addr)(uintptr_t)obj->interp);
	if (obj->phdr_loaded)
		obj->phdr =  (void *)(obj->relocbase + (Elf_Addr)(uintptr_t)obj->phdr);

	return obj;

bad:
	if (obj->ehdr != MAP_FAILED)
		munmap(obj->ehdr, _rtld_pagesz);
	if (mapbase != MAP_FAILED)
		munmap(mapbase, mapsize);
	_rtld_obj_free(obj);
	return NULL;
}

void
_rtld_obj_free(Obj_Entry *obj)
{
	Objlist_Entry *elm;

#if defined(__HAVE_TLS_VARIANT_I) || defined(__HAVE_TLS_VARIANT_II)
	if (obj->tls_done)
		_rtld_tls_offset_free(obj);
#endif
	xfree(obj->path);
	while (obj->needed != NULL) {
		Needed_Entry *needed = obj->needed;
		obj->needed = needed->next;
		xfree(needed);
	}
	while ((elm = SIMPLEQ_FIRST(&obj->dldags)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&obj->dldags, link);
		xfree(elm);
	}
	while ((elm = SIMPLEQ_FIRST(&obj->dagmembers)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&obj->dagmembers, link);
		xfree(elm);
	}
	if (!obj->phdr_loaded)
		xfree((void *)(uintptr_t)obj->phdr);
	xfree(obj);
#ifdef COMBRELOC
	_rtld_combreloc_reset(obj);
#endif
}

Obj_Entry *
_rtld_obj_new(void)
{
	Obj_Entry *obj;

	obj = CNEW(Obj_Entry);
	SIMPLEQ_INIT(&obj->dldags);
	SIMPLEQ_INIT(&obj->dagmembers);
	return obj;
}

/*
 * Given a set of ELF protection flags, return the corresponding protection
 * flags for MMAP.
 */
static int
protflags(int elfflags)
{
	int prot = 0;

	if (elfflags & PF_R)
		prot |= PROT_READ;
#ifdef RTLD_LOADER
	if (elfflags & PF_W)
		prot |= PROT_WRITE;
#endif
	if (elfflags & PF_X)
		prot |= PROT_EXEC;
	return prot;
}
