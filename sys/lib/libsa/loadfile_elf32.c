/* $NetBSD: loadfile_elf32.c,v 1.29 2011/02/17 21:15:31 christos Exp $ */

/*-
 * Copyright (c) 1997, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center and by Christos Zoulas.
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

/* If not included by exec_elf64.c, ELFSIZE won't be defined. */
#ifndef ELFSIZE
#define	ELFSIZE	32
#endif

#ifdef _STANDALONE
#include <lib/libsa/stand.h>
#include <lib/libkern/libkern.h>
#else
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#endif

#include <sys/param.h>
#include <sys/exec.h>

#include "loadfile.h"

#if ((ELFSIZE == 32) && defined(BOOT_ELF32)) || \
    ((ELFSIZE == 64) && defined(BOOT_ELF64))

#define	ELFROUND	(ELFSIZE / 8)

#ifndef _STANDALONE
#include "byteorder.h"

/*
 * Byte swapping may be necessary in the non-_STANDLONE case because
 * we may be built with a host compiler.
 */
#define	E16(f)								\
	f = (bo == ELFDATA2LSB) ? sa_htole16(f) : sa_htobe16(f)
#define	E32(f)								\
	f = (bo == ELFDATA2LSB) ? sa_htole32(f) : sa_htobe32(f)
#define	E64(f)								\
	f = (bo == ELFDATA2LSB) ? sa_htole64(f) : sa_htobe64(f)

#define	I16(f)								\
	f = (bo == ELFDATA2LSB) ? sa_le16toh(f) : sa_be16toh(f)
#define	I32(f)								\
	f = (bo == ELFDATA2LSB) ? sa_le32toh(f) : sa_be32toh(f)
#define	I64(f)								\
	f = (bo == ELFDATA2LSB) ? sa_le64toh(f) : sa_be64toh(f)

static void
internalize_ehdr(Elf_Byte bo, Elf_Ehdr *ehdr)
{

#if ELFSIZE == 32
	I16(ehdr->e_type);
	I16(ehdr->e_machine);
	I32(ehdr->e_version);
	I32(ehdr->e_entry);
	I32(ehdr->e_phoff);
	I32(ehdr->e_shoff);
	I32(ehdr->e_flags);
	I16(ehdr->e_ehsize);
	I16(ehdr->e_phentsize);
	I16(ehdr->e_phnum);
	I16(ehdr->e_shentsize);
	I16(ehdr->e_shnum);
	I16(ehdr->e_shstrndx);
#elif ELFSIZE == 64
	I16(ehdr->e_type);
	I16(ehdr->e_machine);
	I32(ehdr->e_version);
	I64(ehdr->e_entry);
	I64(ehdr->e_phoff);
	I64(ehdr->e_shoff);
	I32(ehdr->e_flags);
	I16(ehdr->e_ehsize);
	I16(ehdr->e_phentsize);
	I16(ehdr->e_phnum);
	I16(ehdr->e_shentsize);
	I16(ehdr->e_shnum);
	I16(ehdr->e_shstrndx);
#else
#error ELFSIZE is not 32 or 64
#endif
}

static void
externalize_ehdr(Elf_Byte bo, Elf_Ehdr *ehdr)
{

#if ELFSIZE == 32
	E16(ehdr->e_type);
	E16(ehdr->e_machine);
	E32(ehdr->e_version);
	E32(ehdr->e_entry);
	E32(ehdr->e_phoff);
	E32(ehdr->e_shoff);
	E32(ehdr->e_flags);
	E16(ehdr->e_ehsize);
	E16(ehdr->e_phentsize);
	E16(ehdr->e_phnum);
	E16(ehdr->e_shentsize);
	E16(ehdr->e_shnum);
	E16(ehdr->e_shstrndx);
#elif ELFSIZE == 64
	E16(ehdr->e_type);
	E16(ehdr->e_machine);
	E32(ehdr->e_version);
	E64(ehdr->e_entry);
	E64(ehdr->e_phoff);
	E64(ehdr->e_shoff);
	E32(ehdr->e_flags);
	E16(ehdr->e_ehsize);
	E16(ehdr->e_phentsize);
	E16(ehdr->e_phnum);
	E16(ehdr->e_shentsize);
	E16(ehdr->e_shnum);
	E16(ehdr->e_shstrndx);
#else
#error ELFSIZE is not 32 or 64
#endif
}

static void
internalize_phdr(Elf_Byte bo, Elf_Phdr *phdr)
{

#if ELFSIZE == 32
	I32(phdr->p_type);
	I32(phdr->p_offset);
	I32(phdr->p_vaddr);
	I32(phdr->p_paddr);
	I32(phdr->p_filesz);
	I32(phdr->p_memsz);
	I32(phdr->p_flags);
	I32(phdr->p_align);
#elif ELFSIZE == 64
	I32(phdr->p_type);
	I32(phdr->p_offset);
	I64(phdr->p_vaddr);
	I64(phdr->p_paddr);
	I64(phdr->p_filesz);
	I64(phdr->p_memsz);
	I64(phdr->p_flags);
	I64(phdr->p_align);
#else
#error ELFSIZE is not 32 or 64
#endif
}

static void
internalize_shdr(Elf_Byte bo, Elf_Shdr *shdr)
{

#if ELFSIZE == 32
	I32(shdr->sh_name);
	I32(shdr->sh_type);
	I32(shdr->sh_flags);
	I32(shdr->sh_addr);
	I32(shdr->sh_offset);
	I32(shdr->sh_size);
	I32(shdr->sh_link);
	I32(shdr->sh_info);
	I32(shdr->sh_addralign);
	I32(shdr->sh_entsize);
#elif ELFSIZE == 64
	I32(shdr->sh_name);
	I32(shdr->sh_type);
	I64(shdr->sh_flags);
	I64(shdr->sh_addr);
	I64(shdr->sh_offset);
	I64(shdr->sh_size);
	I32(shdr->sh_link);
	I32(shdr->sh_info);
	I64(shdr->sh_addralign);
	I64(shdr->sh_entsize);
#else
#error ELFSIZE is not 32 or 64
#endif
}

static void
externalize_shdr(Elf_Byte bo, Elf_Shdr *shdr)
{

#if ELFSIZE == 32
	E32(shdr->sh_name);
	E32(shdr->sh_type);
	E32(shdr->sh_flags);
	E32(shdr->sh_addr);
	E32(shdr->sh_offset);
	E32(shdr->sh_size);
	E32(shdr->sh_link);
	E32(shdr->sh_info);
	E32(shdr->sh_addralign);
	E32(shdr->sh_entsize);
#elif ELFSIZE == 64
	E32(shdr->sh_name);
	E32(shdr->sh_type);
	E64(shdr->sh_flags);
	E64(shdr->sh_addr);
	E64(shdr->sh_offset);
	E64(shdr->sh_size);
	E32(shdr->sh_link);
	E32(shdr->sh_info);
	E64(shdr->sh_addralign);
	E64(shdr->sh_entsize);
#else
#error ELFSIZE is not 32 or 64
#endif
}
#else /* _STANDALONE */
/*
 * Byte swapping is never necessary in the _STANDALONE case because
 * we are being built with the target compiler.
 */
#define	internalize_ehdr(bo, ehdr)	/* nothing */
#define	externalize_ehdr(bo, ehdr)	/* nothing */

#define	internalize_phdr(bo, phdr)	/* nothing */

#define	internalize_shdr(bo, shdr)	/* nothing */
#define	externalize_shdr(bo, shdr)	/* nothing */
#endif /* _STANDALONE */

int
ELFNAMEEND(loadfile)(int fd, Elf_Ehdr *elf, u_long *marks, int flags)
{
	Elf_Shdr *shp;
	Elf_Phdr *phdr;
	int i, j;
	ssize_t sz;
	int first;
	Elf_Addr shpp;
	Elf_Addr minp = ~0, maxp = 0, pos = 0, elfp = 0;
	u_long offset = marks[MARK_START];
	ssize_t nr;
	struct __packed {
		Elf_Nhdr	nh;
		uint8_t		name[ELF_NOTE_NETBSD_NAMESZ + 1];
		uint8_t		desc[ELF_NOTE_NETBSD_DESCSZ];
	} note;
	char *shstr = NULL;
	int boot_load_ctf = 1;

	/* some ports dont use the offset */
	(void)&offset;

	internalize_ehdr(elf->e_ident[EI_DATA], elf);

	sz = elf->e_phnum * sizeof(Elf_Phdr);
	phdr = ALLOC(sz);

	if (lseek(fd, elf->e_phoff, SEEK_SET) == -1)  {
		WARN(("lseek phdr"));
		goto freephdr;
	}
	nr = read(fd, phdr, sz);
	if (nr == -1) {
		WARN(("read program headers"));
		goto freephdr;
	}
	if (nr != sz) {
		errno = EIO;
		WARN(("read program headers"));
		goto freephdr;
	}

	for (first = 1, i = 0; i < elf->e_phnum; i++) {
		internalize_phdr(elf->e_ident[EI_DATA], &phdr[i]);

#ifndef MD_LOADSEG /* Allow processor ABI specific segment loads */
#define MD_LOADSEG(a) /*CONSTCOND*/0
#endif
		if (MD_LOADSEG(&phdr[i]))
			goto loadseg;

		if (phdr[i].p_type != PT_LOAD ||
		    (phdr[i].p_flags & (PF_W|PF_X)) == 0)
			continue;

#define IS_TEXT(p)	(p.p_flags & PF_X)
#define IS_DATA(p)	(p.p_flags & PF_W)
#define IS_BSS(p)	(p.p_filesz < p.p_memsz)
		/*
		 * XXX: Assume first address is lowest
		 */
		if ((IS_TEXT(phdr[i]) && (flags & LOAD_TEXT)) ||
		    (IS_DATA(phdr[i]) && (flags & LOAD_DATA))) {

		loadseg:
			if (marks[MARK_DATA] == 0 && IS_DATA(phdr[i]))
				marks[MARK_DATA] = LOADADDR(phdr[i].p_vaddr);

			/* Read in segment. */
			PROGRESS(("%s%lu", first ? "" : "+",
			    (u_long)phdr[i].p_filesz));

			if (lseek(fd, phdr[i].p_offset, SEEK_SET) == -1)  {
				WARN(("lseek text"));
				goto freephdr;
			}
			nr = READ(fd, phdr[i].p_vaddr, phdr[i].p_filesz);
			if (nr == -1) {
				WARN(("read text error"));
				goto freephdr;
			}
			if (nr != (ssize_t)phdr[i].p_filesz) {
				errno = EIO;
				WARN(("read text"));
				goto freephdr;
			}
			first = 0;

		}
		if ((IS_TEXT(phdr[i]) && (flags & (LOAD_TEXT|COUNT_TEXT))) ||
		    (IS_DATA(phdr[i]) && (flags & (LOAD_DATA|COUNT_TEXT)))) {
			pos = phdr[i].p_vaddr;
			if (minp > pos)
				minp = pos;
			pos += phdr[i].p_filesz;
			if (maxp < pos)
				maxp = pos;
		}

		/* Zero out bss. */
		if (IS_BSS(phdr[i]) && (flags & LOAD_BSS)) {
			PROGRESS(("+%lu",
			    (u_long)(phdr[i].p_memsz - phdr[i].p_filesz)));
			BZERO((phdr[i].p_vaddr + phdr[i].p_filesz),
			    phdr[i].p_memsz - phdr[i].p_filesz);
		}
		if (IS_BSS(phdr[i]) && (flags & (LOAD_BSS|COUNT_BSS))) {
			pos += phdr[i].p_memsz - phdr[i].p_filesz;
			if (maxp < pos)
				maxp = pos;
		}
	}
	DEALLOC(phdr, sz);

	/*
	 * Copy the ELF and section headers.
	 */
	maxp = roundup(maxp, ELFROUND);
	if (flags & (LOAD_HDR|COUNT_HDR)) {
		elfp = maxp;
		maxp += sizeof(Elf_Ehdr);
	}

	if (flags & (LOAD_SYM|COUNT_SYM)) {
		if (lseek(fd, elf->e_shoff, SEEK_SET) == -1)  {
			WARN(("lseek section headers"));
			return 1;
		}
		sz = elf->e_shnum * sizeof(Elf_Shdr);

		shp = ALLOC(sz);

		nr = read(fd, shp, sz);
		if (nr == -1) {
			WARN(("read section headers"));
			goto freeshp;
		}
		if (nr != sz) {
			errno = EIO;
			WARN(("read section headers"));
			goto freeshp;
		}

		shpp = maxp;
		maxp += roundup(sz, ELFROUND);

#ifndef _STANDALONE
		/* Internalize the section headers. */
		for (i = 0; i < elf->e_shnum; i++)
			internalize_shdr(elf->e_ident[EI_DATA], &shp[i]);
#endif /* ! _STANDALONE */

		/*
		 * First load the section names section.
		 */
		if (boot_load_ctf && (elf->e_shstrndx != 0)) {
			if (flags & LOAD_SYM) {
				if (lseek(fd, shp[elf->e_shstrndx].sh_offset,
				    SEEK_SET) == -1) {
					WARN(("lseek symbols"));
					goto freeshp;
				}
				nr = READ(fd, maxp,
				    shp[elf->e_shstrndx].sh_size);
				if (nr == -1) {
					WARN(("read symbols"));
					goto freeshp;
				}
				if (nr !=
				    (ssize_t)shp[elf->e_shstrndx].sh_size) {
					errno = EIO;
					WARN(("read symbols"));
					goto freeshp;
				}

				shstr = ALLOC(shp[elf->e_shstrndx].sh_size);
				if (lseek(fd, shp[elf->e_shstrndx].sh_offset,
				    SEEK_SET) == -1) {
					WARN(("lseek symbols"));
					goto freeshp;
				}
				nr = read(fd, shstr,
				    shp[elf->e_shstrndx].sh_size);
				if (nr == -1) {
					WARN(("read symbols"));
					goto freeshp;
				}
			}
			shp[elf->e_shstrndx].sh_offset = maxp - elfp;
			maxp += roundup(shp[elf->e_shstrndx].sh_size, ELFROUND);
		}

		/*
		 * Now load the symbol sections themselves.  Make sure
		 * the sections are aligned. Don't bother with any
		 * string table that isn't referenced by a symbol
		 * table.
		 */
		for (first = 1, i = 0; i < elf->e_shnum; i++) {
		    	if (i == elf->e_shstrndx) {
			    /* already loaded this section */
			    continue;
			}
			switch (shp[i].sh_type) {
			case SHT_PROGBITS:
			    	if (boot_load_ctf && shstr) {
					/* got a CTF section? */
					if (strncmp(".SUNW_ctf",
						    &shstr[shp[i].sh_name],
						    10) == 0) {
					    	goto havesym;
					}
				}

				/* Not loading this, so zero out the offset. */
				shp[i].sh_offset = 0;
			    	break;
			case SHT_STRTAB:
				for (j = 0; j < elf->e_shnum; j++)
					if (shp[j].sh_type == SHT_SYMTAB &&
					    shp[j].sh_link == (unsigned int)i)
						goto havesym;
				/* FALLTHROUGH */
			default:
				/* Not loading this, so zero out the offset. */
				shp[i].sh_offset = 0;
				break;
			havesym:
			case SHT_SYMTAB:
				if (flags & LOAD_SYM) {
					PROGRESS(("%s%ld", first ? " [" : "+",
					    (u_long)shp[i].sh_size));
					if (lseek(fd, shp[i].sh_offset,
					    SEEK_SET) == -1) {
						WARN(("lseek symbols"));
						goto freeshp;
					}
					nr = READ(fd, maxp, shp[i].sh_size);
					if (nr == -1) {
						WARN(("read symbols"));
						goto freeshp;
					}
					if (nr != (ssize_t)shp[i].sh_size) {
						errno = EIO;
						WARN(("read symbols"));
						goto freeshp;
					}
				}
				shp[i].sh_offset = maxp - elfp;
				maxp += roundup(shp[i].sh_size, ELFROUND);
				first = 0;
				break;
			case SHT_NOTE:
				if ((flags & LOAD_NOTE) == 0)
					break;
				if (shp[i].sh_size < sizeof(note)) {
					shp[i].sh_offset = 0;
					break;
				}
				if (lseek(fd, shp[i].sh_offset, SEEK_SET)
				    == -1) {
					WARN(("lseek note"));
					goto freeshp;
				}
				nr = read(fd, &note, sizeof(note));
				if (nr == -1) {
					WARN(("read note"));
					goto freeshp;
				}
				if (note.nh.n_namesz ==
				    ELF_NOTE_NETBSD_NAMESZ &&
				    note.nh.n_descsz ==
				    ELF_NOTE_NETBSD_DESCSZ &&
				    note.nh.n_type ==
				    ELF_NOTE_TYPE_NETBSD_TAG &&
				    memcmp(note.name, ELF_NOTE_NETBSD_NAME,
				    sizeof(note.name)) == 0) {
				    	memcpy(&netbsd_version, &note.desc,
				    	    sizeof(netbsd_version));
				}
				shp[i].sh_offset = 0;
				break;
			}
		}
		if (flags & LOAD_SYM) {
#ifndef _STANDALONE
			/* Externalize the section headers. */
			for (i = 0; i < elf->e_shnum; i++)
				externalize_shdr(elf->e_ident[EI_DATA],
				    &shp[i]);
#endif /* ! _STANDALONE */
			BCOPY(shp, shpp, sz);

			if (first == 0)
				PROGRESS(("]"));
		}
		DEALLOC(shp, sz);
	}
	
	if (shstr) {
	    DEALLOC(shstr, shp[elf->e_shstrndx].sh_size);
	}

	/*
	 * Frob the copied ELF header to give information relative
	 * to elfp.
	 */
	if (flags & LOAD_HDR) {
		elf->e_phoff = 0;
		elf->e_shoff = sizeof(Elf_Ehdr);
		elf->e_phentsize = 0;
		elf->e_phnum = 0;
		externalize_ehdr(elf->e_ident[EI_DATA], elf);
		BCOPY(elf, elfp, sizeof(*elf));
		internalize_ehdr(elf->e_ident[EI_DATA], elf);
	}

	marks[MARK_START] = LOADADDR(minp);
	marks[MARK_ENTRY] = LOADADDR(elf->e_entry);
	/*
	 * Since there can be more than one symbol section in the code
	 * and we need to find strtab too in order to do anything
	 * useful with the symbols, we just pass the whole elf
	 * header back and we let the kernel debugger find the
	 * location and number of symbols by itself.
	 */
	marks[MARK_NSYM] = 1;	/* XXX: Kernel needs >= 0 */
	marks[MARK_SYM] = LOADADDR(elfp);
	marks[MARK_END] = LOADADDR(maxp);
	return 0;
freephdr:
	DEALLOC(phdr, sz);
	return 1;
freeshp:
	DEALLOC(shp, sz);
	return 1;
}

#ifdef TEST
#include <stdlib.h>
#include <fcntl.h>
#include <err.h>
#include <stdio.h>
u_int32_t netbsd_version;
int
main(int argc, char *argv[])
{
	int fd;
	u_long marks[MARK_MAX];
	Elf_Ehdr elf;
	if (argc != 2) {
		(void)fprintf(stderr, "Usage: %s <file>\n", getprogname());
		return 1;
	}
	if ((fd = open(argv[1], O_RDONLY)) == -1)
		err(1, "Can't open `%s'", argv[1]);
	if (read(fd, &elf, sizeof(elf)) != sizeof(elf))
		err(1, "Can't read `%s'", argv[1]);
	memset(marks, 0, sizeof(marks));
	marks[MARK_START] = (u_long)malloc(2LL * 1024 * 2024 * 1024);
	ELFNAMEEND(loadfile)(fd, &elf, marks, LOAD_ALL);
	printf("%d\n", netbsd_version);
	return 0;
}
#endif

#endif /* (ELFSIZE == 32 && BOOT_ELF32) || (ELFSIZE == 64 && BOOT_ELF64) */
