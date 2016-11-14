/*	$NetBSD: multiboot.c,v 1.22 2012/12/07 04:49:08 msaitoh Exp $	*/

/*-
 * Copyright (c) 2005, 2006 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Julio M. Merino Vidal.
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
__KERNEL_RCSID(0, "$NetBSD: multiboot.c,v 1.22 2012/12/07 04:49:08 msaitoh Exp $");

#include "opt_multiboot.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cdefs_elf.h>
#include <sys/boot_flag.h>
#include <sys/exec.h>
#include <sys/exec_elf.h>
#include <sys/optstr.h>
#include <sys/ksyms.h>

#include <machine/bootinfo.h>
#include <machine/multiboot.h>

#if !defined(MULTIBOOT)
#  error "MULTIBOOT not defined; this cannot happen."
#endif

/* --------------------------------------------------------------------- */

/*
 * Symbol and string table for the loaded kernel.
 */

struct multiboot_symbols {
	void *		s_symstart;
	size_t		s_symsize;
	void *		s_strstart;
	size_t		s_strsize;
};

/* --------------------------------------------------------------------- */

/*
 * External variables.  All of them, with the exception of 'end', must
 * be set at some point within this file.
 *
 * XXX these should be found in a header file!
 */
extern int		biosbasemem;
extern int		biosextmem;
extern int		biosmem_implicit;
extern int		boothowto;
extern struct bootinfo	bootinfo;
extern int		end;
extern int *		esym;

/* --------------------------------------------------------------------- */

/*
 * Copy of the Multiboot information structure passed to us by the boot
 * loader.  The Multiboot_Info structure has some pointers adjusted to the
 * other variables -- see multiboot_pre_reloc() -- so you oughtn't access
 * them directly.  In other words, always access them through the
 * Multiboot_Info variable.
 */
static char			Multiboot_Cmdline[255];
static uint8_t			Multiboot_Drives[255];
static struct multiboot_info	Multiboot_Info;
static bool			Multiboot_Loader = false;
static char			Multiboot_Loader_Name[255];
static uint8_t			Multiboot_Mmap[1024];
static struct multiboot_symbols	Multiboot_Symbols;

/* --------------------------------------------------------------------- */

/*
 * Prototypes for private functions.
 */
static void	bootinfo_add(struct btinfo_common *, int, int);
static void	copy_syms(struct multiboot_info *);
static void	setup_biosgeom(struct multiboot_info *);
static void	setup_bootdisk(struct multiboot_info *);
static void	setup_bootpath(struct multiboot_info *);
static void	setup_console(struct multiboot_info *);
static void	setup_howto(struct multiboot_info *);
static void	setup_memory(struct multiboot_info *);
static void	setup_memmap(struct multiboot_info *);

/* --------------------------------------------------------------------- */

/*
 * Sets up the kernel if it was booted by a Multiboot-compliant boot
 * loader.  This is executed before the kernel has relocated itself.
 * The main purpose of this function is to copy all the information
 * passed in by the boot loader to a safe place, so that it is available
 * after it has been relocated.
 *
 * WARNING: Because the kernel has not yet relocated itself to KERNBASE,
 * special care has to be taken when accessing memory because absolute
 * addresses (referring to kernel symbols) do not work.  So:
 *
 *     1) Avoid jumps to absolute addresses (such as gotos and switches).
 *     2) To access global variables use their physical address, which
 *        can be obtained using the RELOC macro.
 */
void
multiboot_pre_reloc(struct multiboot_info *mi)
{
#define RELOC(type, x) ((type)((vaddr_t)(x) - KERNBASE))
	struct multiboot_info *midest =
	    RELOC(struct multiboot_info *, &Multiboot_Info);

	*RELOC(bool *, &Multiboot_Loader) = true;
	memcpy(midest, mi, sizeof(Multiboot_Info));

	if (mi->mi_flags & MULTIBOOT_INFO_HAS_CMDLINE) {
		strncpy(RELOC(void *, Multiboot_Cmdline), mi->mi_cmdline,
		    sizeof(Multiboot_Cmdline));
		midest->mi_cmdline = (char *)&Multiboot_Cmdline;
	}

	if (mi->mi_flags & MULTIBOOT_INFO_HAS_LOADER_NAME) {
		strncpy(RELOC(void *, Multiboot_Loader_Name),
		    mi->mi_loader_name, sizeof(Multiboot_Loader_Name));
		midest->mi_loader_name = (char *)&Multiboot_Loader_Name;
	}

	if (mi->mi_flags & MULTIBOOT_INFO_HAS_MMAP) {
		memcpy(RELOC(void *, Multiboot_Mmap),
		    (void *)mi->mi_mmap_addr, mi->mi_mmap_length);
		midest->mi_mmap_addr = (vaddr_t)&Multiboot_Mmap;
	}

	if (mi->mi_flags & MULTIBOOT_INFO_HAS_DRIVES) {
		memcpy(RELOC(void *, Multiboot_Drives),
		    (void *)mi->mi_drives_addr, mi->mi_drives_length);
		midest->mi_drives_addr = (vaddr_t)&Multiboot_Drives;
	}

	copy_syms(mi);
#undef RELOC
}

/* --------------------------------------------------------------------- */

/*
 * Sets up the kernel if it was booted by a Multiboot-compliant boot
 * loader.  This is executed just after the kernel has relocated itself.
 * At this point, executing any kind of code is safe, keeping in mind
 * that no devices have been initialized yet (not even the console!).
 */
void
multiboot_post_reloc(void)
{
	struct multiboot_info *mi;

	if (! Multiboot_Loader)
		return;

	mi = &Multiboot_Info;
	bootinfo.bi_nentries = 0;

	setup_memory(mi);
	setup_console(mi);
	setup_howto(mi);
	setup_bootpath(mi);
	setup_biosgeom(mi);
	setup_bootdisk(mi);
	setup_memmap(mi);
}

/* --------------------------------------------------------------------- */

/*
 * Prints a summary of the information collected in the Multiboot
 * information header (if present).  Done as a separate function because
 * the console has to be available.
 */
void
multiboot_print_info(void)
{
	struct multiboot_info *mi = &Multiboot_Info;
	struct multiboot_symbols *ms = &Multiboot_Symbols;

	if (! Multiboot_Loader)
		return;

	printf("multiboot: Information structure flags: 0x%08x\n",
	    mi->mi_flags);

	if (mi->mi_flags & MULTIBOOT_INFO_HAS_LOADER_NAME)
		printf("multiboot: Boot loader: %s\n", mi->mi_loader_name);

	if (mi->mi_flags & MULTIBOOT_INFO_HAS_CMDLINE)
		printf("multiboot: Command line: %s\n", mi->mi_cmdline);

	if (mi->mi_flags & MULTIBOOT_INFO_HAS_MEMORY)
		printf("multiboot: %u KB lower memory, %u KB upper memory\n",
		    mi->mi_mem_lower, mi->mi_mem_upper);

	if (mi->mi_flags & MULTIBOOT_INFO_HAS_ELF_SYMS) {
		KASSERT(esym != 0);
		printf("multiboot: Symbol table at %p, length %d bytes\n",
		    ms->s_symstart, ms->s_symsize);
		printf("multiboot: String table at %p, length %d bytes\n",
		    ms->s_strstart, ms->s_strsize);
	}
}

/* --------------------------------------------------------------------- */

/*
 * Adds the bootinfo entry given in 'item' to the bootinfo tables.
 * Sets the item type to 'type' and its length to 'len'.
 */
static void
bootinfo_add(struct btinfo_common *item, int type, int len)
{
	int i;
	struct bootinfo *bip = (struct bootinfo *)&bootinfo;
	vaddr_t data;

	item->type = type;
	item->len = len;

	data = (vaddr_t)&bip->bi_data;
	for (i = 0; i < bip->bi_nentries; i++) {
		struct btinfo_common *tmp;

		tmp = (struct btinfo_common *)data;
		data += tmp->len;
	}
	if (data + len < (vaddr_t)&bip->bi_data + sizeof(bip->bi_data)) {
		memcpy((void *)data, item, len);
		bip->bi_nentries++;
	}
}

/* --------------------------------------------------------------------- */

/*
 * Copies the symbol table and the strings table passed in by the boot
 * loader after the kernel's image, and sets up 'esym' accordingly so
 * that this data is properly copied into upper memory during relocation.
 *
 * WARNING: This code runs before the kernel has relocated itself.  See
 * the note in multiboot_pre_reloc() for more information.
 */
static void
copy_syms(struct multiboot_info *mi)
{
#define RELOC(type, x) ((type)((vaddr_t)(x) - KERNBASE))
	int i;
	struct multiboot_symbols *ms;
	Elf32_Shdr *symtabp, *strtabp;
	Elf32_Word symsize, strsize;
	Elf32_Addr symaddr, straddr;
	Elf32_Addr symstart, strstart;

	/*
	 * Check if the Multiboot information header has symbols or not.
	 */
	if (!(mi->mi_flags & MULTIBOOT_INFO_HAS_ELF_SYMS))
		return;

	ms = RELOC(struct multiboot_symbols *, &Multiboot_Symbols);

	/*
	 * Locate a symbol table and its matching string table in the
	 * section headers passed in by the boot loader.  Set 'symtabp'
	 * and 'strtabp' with pointers to the matching entries.
	 */
	symtabp = strtabp = NULL;
	for (i = 0; i < mi->mi_elfshdr_num && symtabp == NULL &&
	    strtabp == NULL; i++) {
		Elf32_Shdr *shdrp;

		shdrp = &((Elf32_Shdr *)mi->mi_elfshdr_addr)[i];

		if ((shdrp->sh_type & SHT_SYMTAB) &&
		    shdrp->sh_link != SHN_UNDEF) {
			Elf32_Shdr *shdrp2;

			shdrp2 = &((Elf32_Shdr *)mi->mi_elfshdr_addr)
			    [shdrp->sh_link];

			if (shdrp2->sh_type & SHT_STRTAB) {
				symtabp = shdrp;
				strtabp = shdrp2;
			}
		}
	}
	if (symtabp == NULL || strtabp == NULL)
		return;

	symaddr = symtabp->sh_addr;
	straddr = strtabp->sh_addr;
	symsize = symtabp->sh_size;
	strsize = strtabp->sh_size;

	/*
	 * Copy the symbol and string tables just after the kernel's
	 * end address, in this order.  Only the contents of these ELF
	 * sections are copied; headers are discarded.  esym is later
	 * updated to point to the lowest "free" address after the tables
	 * so that they are mapped appropriately when enabling paging.
	 *
	 * We need to be careful to not overwrite valid data doing the
	 * copies, hence all the different cases below.  We can assume
	 * that if the tables start before the kernel's end address,
	 * they will not grow over this address.
	 */
        if ((void *)symtabp < RELOC(void *, &end) &&
	    (void *)strtabp < RELOC(void *, &end)) {
		symstart = RELOC(Elf32_Addr, &end);
		strstart = symstart + symsize;
		memcpy((void *)symstart, (void *)symaddr, symsize);
		memcpy((void *)strstart, (void *)straddr, strsize);
        } else if ((void *)symtabp > RELOC(void *, &end) &&
	           (void *)strtabp < RELOC(void *, &end)) {
		symstart = RELOC(Elf32_Addr, &end);
		strstart = symstart + symsize;
		memcpy((void *)symstart, (void *)symaddr, symsize);
		memcpy((void *)strstart, (void *)straddr, strsize);
        } else if ((void *)symtabp < RELOC(void *, &end) &&
	           (void *)strtabp > RELOC(void *, &end)) {
		strstart = RELOC(Elf32_Addr, &end);
		symstart = strstart + strsize;
		memcpy((void *)strstart, (void *)straddr, strsize);
		memcpy((void *)symstart, (void *)symaddr, symsize);
	} else {
		/* symtabp and strtabp are both over end */
		if (symtabp < strtabp) {
			symstart = RELOC(Elf32_Addr, &end);
			strstart = symstart + symsize;
			memcpy((void *)symstart, (void *)symaddr, symsize);
			memcpy((void *)strstart, (void *)straddr, strsize);
		} else {
			strstart = RELOC(Elf32_Addr, &end);
			symstart = strstart + strsize;
			memcpy((void *)strstart, (void *)straddr, strsize);
			memcpy((void *)symstart, (void *)symaddr, symsize);
		}
	}

	*RELOC(int *, &esym) =
	    (int)(symstart + symsize + strsize + KERNBASE);

	ms->s_symstart = (void *)(symstart + KERNBASE);
	ms->s_symsize  = symsize;
	ms->s_strstart = (void *)(strstart + KERNBASE);
	ms->s_strsize  = strsize;
#undef RELOC
}

/* --------------------------------------------------------------------- */

/*
 * Sets up the biosgeom bootinfo structure if the Multiboot information
 * structure provides information about disk drives.
 */
static void
setup_biosgeom(struct multiboot_info *mi)
{
	size_t pos;
	uint8_t bidata[1024];
	struct btinfo_biosgeom *bi;

	if (!(mi->mi_flags & MULTIBOOT_INFO_HAS_DRIVES))
		return;

	memset(bidata, 0, sizeof(bidata));
	bi = (struct btinfo_biosgeom *)bidata;
	pos = 0;

	while (pos < mi->mi_drives_length) {
		struct multiboot_drive *md;
		struct bi_biosgeom_entry bbe;

		md = (struct multiboot_drive *)
		    &((uint8_t *)mi->mi_drives_addr)[pos];

		memset(&bbe, 0, sizeof(bbe));
		bbe.sec = md->md_sectors;
		bbe.head = md->md_heads;
		bbe.cyl = md->md_cylinders;
		bbe.dev = md->md_number;

		memcpy(&bi->disk[bi->num], &bbe, sizeof(bbe));
		bi->num++;

		pos += md->md_length;
	}

	bootinfo_add((struct btinfo_common *)bi, BTINFO_BIOSGEOM,
	    sizeof(struct btinfo_biosgeom) +
	    bi->num * sizeof(struct bi_biosgeom_entry));
}

/* --------------------------------------------------------------------- */

/*
 * Sets up the default root device if the Multiboot information
 * structure provides information about the boot drive (where the kernel
 * image was loaded from) or if the user gave a 'root' parameter on the
 * boot command line.
 */
static void
setup_bootdisk(struct multiboot_info *mi)
{
	bool found;
	struct btinfo_rootdevice bi;

	found = false;

	if (mi->mi_flags & MULTIBOOT_INFO_HAS_CMDLINE)
		found = optstr_get(mi->mi_cmdline, "root", bi.devname,
		    sizeof(bi.devname));

	if (!found && (mi->mi_flags & MULTIBOOT_INFO_HAS_BOOT_DEVICE)) {
		const char *devprefix;

		/* Attempt to match the BIOS boot disk to a device.  There
		 * is not much we can do to get it right.  (Well, strictly
		 * speaking, we could, but it is certainly not worth the
		 * extra effort.) */
		switch (mi->mi_boot_device_drive) {
		case 0x00:	devprefix = "fd0";	break;
		case 0x01:	devprefix = "fd1";	break;
		case 0x80:	devprefix = "wd0";	break;
		case 0x81:	devprefix = "wd1";	break;
		case 0x82:	devprefix = "wd2";	break;
		case 0x83:	devprefix = "wd3";	break;
		default:	devprefix = "wd0";
		}

		strcpy(bi.devname, devprefix);
		if (mi->mi_boot_device_part2 != 0xFF)
			bi.devname[3] = mi->mi_boot_device_part2 + 'a';
		else
			bi.devname[3] = 'a';
		bi.devname[4] = '\0';

		found = true;
	}

	if (found) {
		bootinfo_add((struct btinfo_common *)&bi, BTINFO_ROOTDEVICE,
		    sizeof(struct btinfo_rootdevice));
	}
}

/* --------------------------------------------------------------------- */

/*
 * Sets up the bootpath bootinfo structure with an appropriate kernel
 * name derived from the boot command line.  The Multiboot information
 * structure does not provide this detail directly, so we try to derive
 * it from the command line setting.
 */
static void
setup_bootpath(struct multiboot_info *mi)
{
	struct btinfo_bootpath bi;
	char *cl, *cl2, old;
	int len;

	if (strncmp(Multiboot_Loader_Name, "GNU GRUB ",
	    sizeof(Multiboot_Loader_Name)) > 0) {
		cl = mi->mi_cmdline;
		while (*cl != '\0' && *cl != '/')
			cl++;
		cl2 = cl;
		len = 0;
		while (*cl2 != '\0' && *cl2 != ' ') {
			len++;
			cl2++;
		}

		old = *cl2;
		*cl2 = '\0';
		memcpy(bi.bootpath, cl, MIN(sizeof(bi.bootpath), len));
		*cl2 = old;
		bi.bootpath[MIN(sizeof(bi.bootpath) - 1, len)] = '\0';

		bootinfo_add((struct btinfo_common *)&bi, BTINFO_BOOTPATH,
		    sizeof(struct btinfo_bootpath));
	}
}

/* --------------------------------------------------------------------- */

/*
 * Sets up the console bootinfo structure if the user gave a 'console'
 * argument on the boot command line.  The Multiboot information
 * structure gives no hint about this, so the only way to know where the
 * console is is to let the user specify it.
 *
 * If there wasn't any 'console' argument, this does not generate any
 * bootinfo entry, falling back to the kernel's default console.
 *
 * If there weren't any of 'console_speed' or 'console_addr' arguments,
 * this falls back to the default values for the serial port.
 */
static void
setup_console(struct multiboot_info *mi)
{
	struct btinfo_console bi;
	bool found;

	found = false;

	if (mi->mi_flags & MULTIBOOT_INFO_HAS_CMDLINE)
		found = optstr_get(mi->mi_cmdline, "console", bi.devname,
		    sizeof(bi.devname));

	if (found) {
		bool valid;

		if (strncmp(bi.devname, "com", sizeof(bi.devname)) == 0) {
			char tmp[10];

			found = optstr_get(mi->mi_cmdline, "console_speed",
			    tmp, sizeof(tmp));
			if (found)
				bi.speed = strtoul(tmp, NULL, 10);
			else
				bi.speed = 0; /* Use default speed. */

			found = optstr_get(mi->mi_cmdline, "console_addr",
			    tmp, sizeof(tmp));
			if (found) {
				if (tmp[0] == '0' && tmp[1] == 'x')
					bi.addr = strtoul(tmp + 2, NULL, 16);
				else
					bi.addr = strtoul(tmp, NULL, 10);
			} else
				bi.addr = 0; /* Use default address. */

			valid = true;
		} else if (strncmp(bi.devname, "pc", sizeof(bi.devname)) == 0)
			valid = true;
		else
			valid = false;

		if (valid)
			bootinfo_add((struct btinfo_common *)&bi,
			    BTINFO_CONSOLE, sizeof(struct btinfo_console));
	}
}

/* --------------------------------------------------------------------- */

/*
 * Sets up the 'boothowto' variable based on the options given in the
 * boot command line, if any.
 */
static void
setup_howto(struct multiboot_info *mi)
{
	char *cl;

	if (!(mi->mi_flags & MULTIBOOT_INFO_HAS_CMDLINE))
		return;

	cl = mi->mi_cmdline;

	/* Skip kernel file name. */
	while (*cl != '\0' && *cl != ' ')
		cl++;
	while (*cl != '\0' && *cl == ' ')
		cl++;

	/* Check if there are flags and set 'howto' accordingly. */
	if (*cl == '-') {
		int howto = 0;

		cl++;
		while (*cl != '\0' && *cl != ' ') {
			BOOT_FLAG(*cl, howto);
			cl++;
		}
		if (*cl == ' ')
			cl++;

		boothowto = howto;
	}
}

/* --------------------------------------------------------------------- */

/*
 * Sets up the memmap bootinfo structure to describe available memory as
 * given by the BIOS.
 */
static void
setup_memmap(struct multiboot_info *mi)
{
	char data[1024];
	size_t i;
	struct btinfo_memmap *bi;

	if (!(mi->mi_flags & MULTIBOOT_INFO_HAS_MMAP))
		return;

	bi = (struct btinfo_memmap *)data;
	bi->num = 0;

	i = 0;
	while (i < mi->mi_mmap_length) {
		struct multiboot_mmap *mm;
		struct bi_memmap_entry *bie;

		bie = &bi->entry[bi->num];

		mm = (struct multiboot_mmap *)(mi->mi_mmap_addr + i);
		bie->addr = mm->mm_base_addr;
		bie->size = mm->mm_length;
		if (mm->mm_type == 1)
			bie->type = BIM_Memory;
		else
			bie->type = BIM_Reserved;

		bi->num++;
		i += mm->mm_size + 4;
	}

	bootinfo_add((struct btinfo_common *)bi, BTINFO_MEMMAP,
	    sizeof(data));
}

/* --------------------------------------------------------------------- */

/*
 * Sets up the 'biosbasemem' and 'biosextmem' variables if the
 * Multiboot information structure provides information about memory.
 */
static void
setup_memory(struct multiboot_info *mi)
{

	if (!(mi->mi_flags & MULTIBOOT_INFO_HAS_MEMORY))
		return;

	/* Make sure we don't override user-set variables. */
	if (biosbasemem == 0) {
		biosbasemem = mi->mi_mem_lower;
		biosmem_implicit = 1;
	}
	if (biosextmem == 0) {
		biosextmem = mi->mi_mem_upper;
		biosmem_implicit = 1;
	}
}

/* --------------------------------------------------------------------- */

/*
 * Sets up the initial kernel symbol table.  Returns true if this was
 * passed in by Multiboot; false otherwise.
 */
bool
multiboot_ksyms_addsyms_elf(void)
{
	struct multiboot_info *mi = &Multiboot_Info;
	struct multiboot_symbols *ms = &Multiboot_Symbols;

	if (mi->mi_flags & MULTIBOOT_INFO_HAS_ELF_SYMS) {
		Elf32_Ehdr ehdr;

		KASSERT(esym != 0);

		memcpy(ehdr.e_ident, ELFMAG, SELFMAG);
		ehdr.e_ident[EI_CLASS] = ELFCLASS32;
		ehdr.e_ident[EI_DATA] = ELFDATA2LSB;
		ehdr.e_ident[EI_VERSION] = EV_CURRENT;
		ehdr.e_type = ET_EXEC;
		ehdr.e_machine = EM_386;
		ehdr.e_version = 1;
		ehdr.e_ehsize = sizeof(ehdr);

		ksyms_addsyms_explicit((void *)&ehdr,
		    ms->s_symstart, ms->s_symsize,
		    ms->s_strstart, ms->s_strsize);
	}

	return mi->mi_flags & MULTIBOOT_INFO_HAS_ELF_SYMS;
}
