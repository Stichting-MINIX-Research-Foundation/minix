/* $NetBSD: loadfile_elf32.c,v 1.55.2.1 2019/10/23 19:17:15 martin Exp $ */

/*
 * Copyright (c) 1997, 2008, 2017 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center, by Christos Zoulas, and by Maxime Villard.
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
/*
 * Byte swapping may be necessary in the non-_STANDLONE case because
 * we may be built with a host compiler.
 */
#ifndef LIBSA_BIENDIAN_SUPPORT
#define LIBSA_BIENDIAN_SUPPORT
#endif
#endif

#ifdef LIBSA_BIENDIAN_SUPPORT
#include "byteorder.h"

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
#else /* LIBSA_BIENDIAN_SUPPORT */
/*
 * Byte swapping is never necessary in the !LIBSA_BIENDIAN_SUPPORT case
 * because we are being built with the target compiler.
 */
#define	internalize_ehdr(bo, ehdr)	/* nothing */
#define	externalize_ehdr(bo, ehdr)	/* nothing */

#define	internalize_phdr(bo, phdr)	/* nothing */

#define	internalize_shdr(bo, shdr)	/* nothing */
#define	externalize_shdr(bo, shdr)	/* nothing */
#endif /* LIBSA_BIENDIAN_SUPPORT */

#define IS_TEXT(p)	(p.p_flags & PF_X)
#define IS_DATA(p)	((p.p_flags & PF_X) == 0)
#define IS_BSS(p)	(p.p_filesz < p.p_memsz)

#ifndef MD_LOADSEG /* Allow processor ABI specific segment loads */
#define MD_LOADSEG(a) /*CONSTCOND*/0
#endif

/* -------------------------------------------------------------------------- */

#define KERNALIGN_SMALL (1 << 12)	/* XXX should depend on marks[] */
#define KERNALIGN_LARGE (1 << 21)	/* XXX should depend on marks[] */

/*
 * Read some data from a file, and put it in the bootloader memory (local).
 */
static int
ELFNAMEEND(readfile_local)(int fd, Elf_Off elfoff, void *addr, size_t size)
{
	ssize_t nr;

	if (lseek(fd, elfoff, SEEK_SET) == -1)  {
		WARN(("lseek section headers"));
		return -1;
	}
	nr = read(fd, addr, size);
	if (nr == -1) {
		WARN(("read section headers"));
		return -1;
	}
	if (nr != (ssize_t)size) {
		errno = EIO;
		WARN(("read section headers"));
		return -1;
	}

	return 0;
}

/*
 * Read some data from a file, and put it in wherever in memory (global).
 */
static int
ELFNAMEEND(readfile_global)(int fd, u_long offset, Elf_Off elfoff,
    Elf_Addr addr, size_t size)
{
	ssize_t nr;

	/* some ports dont use the offset */
	(void)&offset;

	if (lseek(fd, elfoff, SEEK_SET) == -1) {
		WARN(("lseek section"));
		return -1;
	}
	nr = READ(fd, addr, size);
	if (nr == -1) {
		WARN(("read section"));
		return -1;
	}
	if (nr != (ssize_t)size) {
		errno = EIO;
		WARN(("read section"));
		return -1;
	}

	return 0;
}

/*
 * Load a dynamic ELF binary into memory. Layout of the memory:
 * +------------+-----------------+-----------------+------------------+
 * | ELF HEADER | SECTION HEADERS | KERNEL SECTIONS | SYM+REL SECTIONS |
 * +------------+-----------------+-----------------+------------------+
 * The ELF HEADER start address is marks[MARK_END]. We then map the rest
 * by increasing maxp. An alignment is enforced between the code sections.
 *
 * The offsets of the KERNEL and SYM+REL sections are relative to the start
 * address of the ELF HEADER. We just give the kernel a pointer to the ELF
 * HEADER, and we let the kernel find the location and number of symbols by
 * itself.
 */
static int
ELFNAMEEND(loadfile_dynamic)(int fd, Elf_Ehdr *elf, u_long *marks, int flags)
{
	const u_long offset = marks[MARK_START];
	Elf_Shdr *shdr;
	Elf_Addr shpp, addr;
	int i, j, loaded;
	size_t size, shdrsz, align;
	Elf_Addr maxp, elfp = 0;
	int ret;

	maxp = marks[MARK_END] - offset;

	internalize_ehdr(elf->e_ident[EI_DATA], elf);

	if (elf->e_type != ET_REL) {
		errno = EINVAL;
		return 1;
	}

	/* Create a local copy of the SECTION HEADERS. */
	shdrsz = elf->e_shnum * sizeof(Elf_Shdr);
	shdr = ALLOC(shdrsz);
	ret = ELFNAMEEND(readfile_local)(fd, elf->e_shoff, shdr, shdrsz);
	if (ret == -1) {
		goto out;
	}

	/*
	 * Load the ELF HEADER. Update the section offset, to be relative to
	 * elfp.
	 */
	elf->e_phoff = 0;
	elf->e_shoff = sizeof(Elf_Ehdr);
	elf->e_phentsize = 0;
	elf->e_phnum = 0;
	elfp = maxp;
	externalize_ehdr(elf->e_ident[EI_DATA], elf);
	BCOPY(elf, elfp, sizeof(*elf));
	internalize_ehdr(elf->e_ident[EI_DATA], elf);
	maxp += sizeof(Elf_Ehdr);

#ifdef LIBSA_BIENDIAN_SUPPORT
	for (i = 0; i < elf->e_shnum; i++)
		internalize_shdr(elf->e_ident[EI_DATA], &shdr[i]);
#endif

	/* Save location of the SECTION HEADERS. */
	shpp = maxp;
	maxp += roundup(shdrsz, ELFROUND);

	/*
	 * Load the KERNEL SECTIONS.
	 */
	maxp = roundup(maxp, KERNALIGN_SMALL);
	for (i = 0; i < elf->e_shnum; i++) {
		if (!(shdr[i].sh_flags & SHF_ALLOC)) {
			continue;
		}
		size = (size_t)shdr[i].sh_size;
		if (size <= KERNALIGN_SMALL) {
			align = KERNALIGN_SMALL;
		} else {
			align = KERNALIGN_LARGE;
		}
		addr = roundup(maxp, align);

		loaded = 0;
		switch (shdr[i].sh_type) {
		case SHT_NOBITS:
			/* Zero out bss. */
			BZERO(addr, size);
			loaded = 1;
			break;
		case SHT_PROGBITS:
			ret = ELFNAMEEND(readfile_global)(fd, offset,
			    shdr[i].sh_offset, addr, size);
			if (ret == -1) {
				goto out;
			}
			loaded = 1;
			break;
		default:
			loaded = 0;
			break;
		}

		if (loaded) {
			shdr[i].sh_offset = addr - elfp;
			maxp = roundup(addr + size, align);
		}
	}
	maxp = roundup(maxp, KERNALIGN_LARGE);

	/*
	 * Load the SYM+REL SECTIONS.
	 */
	maxp = roundup(maxp, ELFROUND);
	for (i = 0; i < elf->e_shnum; i++) {
		addr = maxp;
		size = (size_t)shdr[i].sh_size;

		switch (shdr[i].sh_type) {
		case SHT_STRTAB:
			for (j = 0; j < elf->e_shnum; j++)
				if (shdr[j].sh_type == SHT_SYMTAB &&
				    shdr[j].sh_link == (unsigned int)i)
					goto havesym;
			if (elf->e_shstrndx == i)
				goto havesym;
			/*
			 * Don't bother with any string table that isn't
			 * referenced by a symbol table.
			 */
			shdr[i].sh_offset = 0;
			break;
	havesym:
		case SHT_REL:
		case SHT_RELA:
		case SHT_SYMTAB:
			ret = ELFNAMEEND(readfile_global)(fd, offset,
			    shdr[i].sh_offset, addr, size);
			if (ret == -1) {
				goto out;
			}
			shdr[i].sh_offset = maxp - elfp;
			maxp += roundup(size, ELFROUND);
			break;
		}
	}
	maxp = roundup(maxp, KERNALIGN_SMALL);

	/*
	 * Finally, load the SECTION HEADERS.
	 */
#ifdef LIBSA_BIENDIAN_SUPPORT
	for (i = 0; i < elf->e_shnum; i++)
		externalize_shdr(elf->e_ident[EI_DATA], &shdr[i]);
#endif
	BCOPY(shdr, shpp, shdrsz);

	DEALLOC(shdr, shdrsz);

	/*
	 * Just update MARK_SYM and MARK_END without touching the rest.
	 */
	marks[MARK_SYM] = LOADADDR(elfp);
	marks[MARK_END] = LOADADDR(maxp);
	return 0;

out:
	DEALLOC(shdr, shdrsz);
	return 1;
}

/* -------------------------------------------------------------------------- */

/*
 * See comment below. This function is in charge of loading the SECTION HEADERS.
 */
static int
ELFNAMEEND(loadsym)(int fd, Elf_Ehdr *elf, Elf_Addr maxp, Elf_Addr elfp,
    u_long *marks, int flags, Elf_Addr *nmaxp)
{
	const u_long offset = marks[MARK_START];
	int boot_load_ctf = 1;
	Elf_Shdr *shp;
	Elf_Addr shpp;
	char *shstr = NULL;
	size_t sz;
	size_t i, j, shstrsz = 0;
	struct __packed {
		Elf_Nhdr nh;
		uint8_t name[ELF_NOTE_NETBSD_NAMESZ + 1];
		uint8_t desc[ELF_NOTE_NETBSD_DESCSZ];
	} note;
	int first;
	int ret;

	sz = elf->e_shnum * sizeof(Elf_Shdr);
	shp = ALLOC(sz);
	ret = ELFNAMEEND(readfile_local)(fd, elf->e_shoff, shp, sz);
	if (ret == -1) {
		goto out;
	}

	shpp = maxp;
	maxp += roundup(sz, ELFROUND);

#ifdef LIBSA_BIENDIAN_SUPPORT
	for (i = 0; i < elf->e_shnum; i++)
		internalize_shdr(elf->e_ident[EI_DATA], &shp[i]);
#endif

	/*
	 * First load the section names section. Only useful for CTF.
	 */
	if (boot_load_ctf && (elf->e_shstrndx != SHN_UNDEF)) {
		Elf_Off shstroff = shp[elf->e_shstrndx].sh_offset;
		shstrsz = shp[elf->e_shstrndx].sh_size;
		if (flags & LOAD_SYM) {
			ret = ELFNAMEEND(readfile_global)(fd, offset,
			    shstroff, maxp, shstrsz);
			if (ret == -1) {
				goto out;
			}
		}

		/* Create a local copy */
		shstr = ALLOC(shstrsz);
		ret = ELFNAMEEND(readfile_local)(fd, shstroff, shstr, shstrsz);
		if (ret == -1) {
			goto out;
		}
		shp[elf->e_shstrndx].sh_offset = maxp - elfp;
		maxp += roundup(shstrsz, ELFROUND);
	}

	/*
	 * Now load the symbol sections themselves. Make sure the sections are
	 * ELFROUND-aligned. Update sh_offset to be relative to elfp. Set it to
	 * zero when we don't want the sections to be taken care of, the kernel
	 * will properly skip them.
	 */
	first = 1;
	for (i = 1; i < elf->e_shnum; i++) {
		if (i == elf->e_shstrndx) {
			/* already loaded this section */
			continue;
		}

		switch (shp[i].sh_type) {
		case SHT_PROGBITS:
			if (boot_load_ctf && shstr) {
				/* got a CTF section? */
				if (strncmp(&shstr[shp[i].sh_name],
					    ".SUNW_ctf", 10) == 0) {
					goto havesym;
				}
			}

			shp[i].sh_offset = 0;
			break;
		case SHT_STRTAB:
			for (j = 1; j < elf->e_shnum; j++)
				if (shp[j].sh_type == SHT_SYMTAB &&
				    shp[j].sh_link == (unsigned int)i)
					goto havesym;
			/*
			 * Don't bother with any string table that isn't
			 * referenced by a symbol table.
			 */
			shp[i].sh_offset = 0;
			break;
havesym:
		case SHT_SYMTAB:
			if (flags & LOAD_SYM) {
				PROGRESS(("%s%ld", first ? " [" : "+",
				    (u_long)shp[i].sh_size));
				ret = ELFNAMEEND(readfile_global)(fd, offset,
				    shp[i].sh_offset, maxp, shp[i].sh_size);
				if (ret == -1) {
					goto out;
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

			ret = ELFNAMEEND(readfile_local)(fd, shp[i].sh_offset,
			    &note, sizeof(note));
			if (ret == -1) {
				goto out;
			}

			if (note.nh.n_namesz == ELF_NOTE_NETBSD_NAMESZ &&
			    note.nh.n_descsz == ELF_NOTE_NETBSD_DESCSZ &&
			    note.nh.n_type == ELF_NOTE_TYPE_NETBSD_TAG &&
			    memcmp(note.name, ELF_NOTE_NETBSD_NAME,
			    sizeof(note.name)) == 0) {
				memcpy(&netbsd_version, &note.desc,
				    sizeof(netbsd_version));
			}
			shp[i].sh_offset = 0;
			break;
		default:
			shp[i].sh_offset = 0;
			break;
		}
	}
	if (flags & LOAD_SYM) {
#ifdef LIBSA_BIENDIAN_SUPPORT
		for (i = 0; i < elf->e_shnum; i++)
			externalize_shdr(elf->e_ident[EI_DATA], &shp[i]);
#endif
		BCOPY(shp, shpp, sz);

		if (first == 0)
			PROGRESS(("]"));
	}

	*nmaxp = maxp;
	DEALLOC(shp, sz);
	if (shstr != NULL)
		DEALLOC(shstr, shstrsz);
	return 0;

out:
	DEALLOC(shp, sz);
	if (shstr != NULL)
		DEALLOC(shstr, shstrsz);
	return -1;
}

/* -------------------------------------------------------------------------- */

/*
 * Load a static ELF binary into memory. Layout of the memory:
 * +-----------------+------------+-----------------+-----------------+
 * | KERNEL SEGMENTS | ELF HEADER | SECTION HEADERS | SYMBOL SECTIONS |
 * +-----------------+------------+-----------------+-----------------+
 * The KERNEL SEGMENTS start address is fixed by the segments themselves. We
 * then map the rest by increasing maxp.
 *
 * The offsets of the SYMBOL SECTIONS are relative to the start address of the
 * ELF HEADER. The shdr offset of ELF HEADER points to SECTION HEADERS.
 *
 * We just give the kernel a pointer to the ELF HEADER, which is enough for it
 * to find the location and number of symbols by itself later.
 */
static int
ELFNAMEEND(loadfile_static)(int fd, Elf_Ehdr *elf, u_long *marks, int flags)
{
	const u_long offset = marks[MARK_START];
	Elf_Phdr *phdr;
	int i, first;
	size_t sz;
	Elf_Addr minp = ~0, maxp = 0, pos = 0, elfp = 0;
	int ret;

	/* for ports that define progress to nothing */
	(void)&first;

	/* have not seen a data segment so far */
	marks[MARK_DATA] = 0;

	internalize_ehdr(elf->e_ident[EI_DATA], elf);

	if (elf->e_type != ET_EXEC) {
		errno = EINVAL;
		return 1;
	}

	sz = elf->e_phnum * sizeof(Elf_Phdr);
	phdr = ALLOC(sz);
	ret = ELFNAMEEND(readfile_local)(fd, elf->e_phoff, phdr, sz);
	if (ret == -1) {
		goto freephdr;
	}

	first = 1;
	for (i = 0; i < elf->e_phnum; i++) {
		internalize_phdr(elf->e_ident[EI_DATA], &phdr[i]);

		if (MD_LOADSEG(&phdr[i]))
			goto loadseg;

		if (phdr[i].p_type != PT_LOAD ||
		    (phdr[i].p_flags & (PF_W|PF_R|PF_X)) == 0)
			continue;

		if ((IS_TEXT(phdr[i]) && (flags & LOAD_TEXT)) ||
		    (IS_DATA(phdr[i]) && (flags & LOAD_DATA))) {
		loadseg:
			/* XXX: Assume first address is lowest */
			if (marks[MARK_DATA] == 0 && IS_DATA(phdr[i]))
				marks[MARK_DATA] = LOADADDR(phdr[i].p_vaddr);

			/* Read in segment. */
			PROGRESS(("%s%lu", first ? "" : "+",
			    (u_long)phdr[i].p_filesz));

			ret = ELFNAMEEND(readfile_global)(fd, offset,
			    phdr[i].p_offset, phdr[i].p_vaddr,
			    phdr[i].p_filesz);
			if (ret == -1) {
				goto freephdr;
			}

			first = 0;
		}
		if ((IS_TEXT(phdr[i]) && (flags & (LOAD_TEXT|COUNT_TEXT))) ||
		    (IS_DATA(phdr[i]) && (flags & (LOAD_DATA|COUNT_DATA)))) {
			/* XXX: Assume first address is lowest */
			if (marks[MARK_DATA] == 0 && IS_DATA(phdr[i]))
				marks[MARK_DATA] = LOADADDR(phdr[i].p_vaddr);

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
	maxp = roundup(maxp, ELFROUND);

	/*
	 * Load the ELF HEADER, SECTION HEADERS and possibly the SYMBOL
	 * SECTIONS.
	 */
	if (flags & (LOAD_HDR|COUNT_HDR)) {
		elfp = maxp;
		maxp += sizeof(Elf_Ehdr);
	}
	if (flags & (LOAD_SYM|COUNT_SYM)) {
		if (ELFNAMEEND(loadsym)(fd, elf, maxp, elfp, marks, flags,
		    &maxp) == -1) {
 			return 1;
		}
	}

	/*
	 * Update the ELF HEADER to give information relative to elfp.
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
	marks[MARK_NSYM] = 1;	/* XXX: Kernel needs >= 0 */
	marks[MARK_SYM] = LOADADDR(elfp);
	marks[MARK_END] = LOADADDR(maxp);
	return 0;

freephdr:
	DEALLOC(phdr, sz);
	return 1;
}

/* -------------------------------------------------------------------------- */

int
ELFNAMEEND(loadfile)(int fd, Elf_Ehdr *elf, u_long *marks, int flags)
{
	if (flags & LOAD_DYN) {
		return ELFNAMEEND(loadfile_dynamic)(fd, elf, marks, flags);
	} else {
		return ELFNAMEEND(loadfile_static)(fd, elf, marks, flags);
	}
}

#endif /* (ELFSIZE == 32 && BOOT_ELF32) || (ELFSIZE == 64 && BOOT_ELF64) */
