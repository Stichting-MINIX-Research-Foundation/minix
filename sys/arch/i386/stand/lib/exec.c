/*	$NetBSD: exec.c,v 1.59 2014/04/06 19:18:00 jakllsch Exp $	 */

/*-
 * Copyright (c) 2008, 2009 The NetBSD Foundation, Inc.
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

/*
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * 	@(#)boot.c	8.1 (Berkeley) 6/10/93
 */

/*
 * Copyright (c) 1996
 *	Matthias Drochner.  All rights reserved.
 * Copyright (c) 1996
 * 	Perry E. Metzger.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * 	@(#)boot.c	8.1 (Berkeley) 6/10/93
 */

/*
 * starts NetBSD a.out kernel
 * needs lowlevel startup from startprog.S
 * This is a special version of exec.c to support use of XMS.
 */

#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/reboot.h>

#include <i386/multiboot.h>

#include <lib/libsa/stand.h>
#include <lib/libkern/libkern.h>

#include "loadfile.h"
#include "libi386.h"
#include "bootinfo.h"
#include "bootmod.h"
#include "vbe.h"
#ifdef SUPPORT_PS2
#include "biosmca.h"
#endif

#define BOOT_NARGS	6

#ifndef	PAGE_SIZE
#define	PAGE_SIZE	4096
#endif

#define MODULE_WARNING_SEC	5

extern struct btinfo_console btinfo_console;

boot_module_t *boot_modules;
bool boot_modules_enabled = true;
bool kernel_loaded;

typedef struct userconf_command {
	char *uc_text;
	size_t uc_len;
	struct userconf_command *uc_next;
} userconf_command_t;
userconf_command_t *userconf_commands = NULL;

static struct btinfo_framebuffer btinfo_framebuffer;

static struct btinfo_modulelist *btinfo_modulelist;
static size_t btinfo_modulelist_size;
static uint32_t image_end;
static char module_base[64] = "/";
static int howto;

static struct btinfo_userconfcommands *btinfo_userconfcommands = NULL;
static size_t btinfo_userconfcommands_size = 0;

static void	module_init(const char *);
static void	module_add_common(const char *, uint8_t);

static void	userconf_init(void);

void
framebuffer_configure(struct btinfo_framebuffer *fb)
{
	if (fb)
		btinfo_framebuffer = *fb;
	else {
		btinfo_framebuffer.physaddr = 0;
		btinfo_framebuffer.flags = 0;
	}
}

void
module_add(char *name)
{
	return module_add_common(name, BM_TYPE_KMOD);
}

void
splash_add(char *name)
{
	return module_add_common(name, BM_TYPE_IMAGE);
}

void
rnd_add(char *name)
{
	return module_add_common(name, BM_TYPE_RND);
}

void
fs_add(char *name)
{
	return module_add_common(name, BM_TYPE_FS);
}

static void
module_add_common(const char *name, uint8_t type)
{
	boot_module_t *bm, *bmp;
	size_t len;
	char *str;

	while (*name == ' ' || *name == '\t')
		++name;

	bm = alloc(sizeof(boot_module_t));
	len = strlen(name) + 1;
	str = alloc(len);
	if (bm == NULL || str == NULL) {
		printf("couldn't allocate module\n");
		return;
	}
	memcpy(str, name, len);
	bm->bm_path = str;
	bm->bm_next = NULL;
	bm->bm_type = type;
	if (boot_modules == NULL)
		boot_modules = bm;
	else {
		for (bmp = boot_modules; bmp->bm_next;
		    bmp = bmp->bm_next)
			;
		bmp->bm_next = bm;
	}
}

void
userconf_add(char *cmd)
{
	userconf_command_t *uc;
	size_t len;
	char *text;

	while (*cmd == ' ' || *cmd == '\t')
		++cmd;

	uc = alloc(sizeof(*uc));
	if (uc == NULL) {
		printf("couldn't allocate command\n");
		return;
	}

	len = strlen(cmd) + 1;
	text = alloc(len);
	if (text == NULL) {
		dealloc(uc, sizeof(*uc));
		printf("couldn't allocate command\n");
		return;
	}
	memcpy(text, cmd, len);

	uc->uc_text = text;
	uc->uc_len = len;
	uc->uc_next = NULL;

	if (userconf_commands == NULL)
		userconf_commands = uc;
	else {
		userconf_command_t *ucp;
		for (ucp = userconf_commands; ucp->uc_next != NULL;
		     ucp = ucp->uc_next)
			;
		ucp->uc_next = uc;
	}
}

static int
common_load_kernel(const char *file, u_long *basemem, u_long *extmem,
    physaddr_t loadaddr, int floppy, u_long marks[MARK_MAX])
{
	int fd;
#ifdef XMS
	u_long		xmsmem;
	physaddr_t	origaddr = loadaddr;
#endif

	*extmem = getextmem();
	*basemem = getbasemem();

#ifdef XMS
	if ((getextmem1() == 0) && (xmsmem = checkxms())) {
	        u_long kernsize;

		/*
		 * With "CONSERVATIVE_MEMDETECT", extmem is 0 because
		 *  getextmem() is getextmem1(). Without, the "smart"
		 *  methods could fail to report all memory as well.
		 * xmsmem is a few kB less than the actual size, but
		 *  better than nothing.
		 */
		if (xmsmem > *extmem)
			*extmem = xmsmem;
		/*
		 * Get the size of the kernel
		 */
		marks[MARK_START] = loadaddr;
		if ((fd = loadfile(file, marks, COUNT_KERNEL)) == -1)
			return EIO;
		close(fd);

		kernsize = marks[MARK_END];
		kernsize = (kernsize + 1023) / 1024;

		loadaddr = xmsalloc(kernsize);
		if (!loadaddr)
			return ENOMEM;
	}
#endif
	marks[MARK_START] = loadaddr;
	if ((fd = loadfile(file, marks,
	    LOAD_KERNEL & ~(floppy ? LOAD_BACKWARDS : 0))) == -1)
		return EIO;

	close(fd);

#if !defined(__minix) /* this just gives us a cd9660-not-found warning.. */
	/* If the root fs type is unusual, load its module. */
	if (fsmod != NULL)
		module_add_common(fsmod, BM_TYPE_KMOD);
#endif /* !defined(__minix) */

	/*
	 * Gather some information for the kernel. Do this after the
	 * "point of no return" to avoid memory leaks.
	 * (but before DOS might be trashed in the XMS case)
	 */
#ifdef PASS_BIOSGEOM
	bi_getbiosgeom();
#endif
#ifdef PASS_MEMMAP
	bi_getmemmap();
#endif

#ifdef XMS
	if (loadaddr != origaddr) {
		/*
		 * We now have done our last DOS IO, so we may
		 * trash the OS. Copy the data from the temporary
		 * buffer to its real address.
		 */
		marks[MARK_START] -= loadaddr;
		marks[MARK_END] -= loadaddr;
		marks[MARK_SYM] -= loadaddr;
		marks[MARK_END] -= loadaddr;
		ppbcopy(loadaddr, origaddr, marks[MARK_END]);
	}
#endif
	marks[MARK_END] = (((u_long) marks[MARK_END] + sizeof(int) - 1)) &
	    (-sizeof(int));
	image_end = marks[MARK_END];
	kernel_loaded = true;

	return 0;
}

int
exec_netbsd(const char *file, physaddr_t loadaddr, int boothowto, int floppy,
	    void (*callback)(void))
{
	uint32_t	boot_argv[BOOT_NARGS];
	u_long		marks[MARK_MAX];
	struct btinfo_symtab btinfo_symtab;
	u_long		extmem;
	u_long		basemem;

#ifdef	DEBUG
	printf("exec: file=%s loadaddr=0x%lx\n",
	       file ? file : "NULL", loadaddr);
#endif

	BI_ALLOC(32); /* ??? */

	BI_ADD(&btinfo_console, BTINFO_CONSOLE, sizeof(struct btinfo_console));

	howto = boothowto;

	memset(marks, 0, sizeof(marks));

	if (common_load_kernel(file, &basemem, &extmem, loadaddr, floppy, marks))
		goto out;

	boot_argv[0] = boothowto;
	boot_argv[1] = 0;
	boot_argv[2] = vtophys(bootinfo);	/* old cyl offset */
	boot_argv[3] = marks[MARK_END];
	boot_argv[4] = extmem;
	boot_argv[5] = basemem;

	/* pull in any modules if necessary */
	if (boot_modules_enabled) {
		module_init(file);
		if (btinfo_modulelist) {
			BI_ADD(btinfo_modulelist, BTINFO_MODULELIST,
			    btinfo_modulelist_size);
		}
	}

	userconf_init();
	if (btinfo_userconfcommands != NULL)
		BI_ADD(btinfo_userconfcommands, BTINFO_USERCONFCOMMANDS,
		    btinfo_userconfcommands_size);

#ifdef DEBUG
	printf("Start @ 0x%lx [%ld=0x%lx-0x%lx]...\n", marks[MARK_ENTRY],
	    marks[MARK_NSYM], marks[MARK_SYM], marks[MARK_END]);
#endif

	btinfo_symtab.nsym = marks[MARK_NSYM];
	btinfo_symtab.ssym = marks[MARK_SYM];
	btinfo_symtab.esym = marks[MARK_END];
	BI_ADD(&btinfo_symtab, BTINFO_SYMTAB, sizeof(struct btinfo_symtab));

	/* set new video mode if necessary */
	vbe_commit();
	BI_ADD(&btinfo_framebuffer, BTINFO_FRAMEBUFFER,
	    sizeof(struct btinfo_framebuffer));

	if (callback != NULL)
		(*callback)();
	startprog(marks[MARK_ENTRY], BOOT_NARGS, boot_argv,
		  x86_trunc_page(basemem*1024));
	panic("exec returned");

out:
	BI_FREE();
	bootinfo = 0;
	return -1;
}

static void
extract_device(const char *path, char *buf, size_t buflen)
{
	size_t i;

	if (strchr(path, ':') != NULL) {
		for (i = 0; i < buflen - 2 && path[i] != ':'; i++)
			buf[i] = path[i];
		buf[i++] = ':';
		buf[i] = '\0';
	} else
		buf[0] = '\0';
}

static const char *
module_path(boot_module_t *bm, const char *kdev)
{
	static char buf[256];
	char name_buf[256], dev_buf[64];
	const char *name, *name2, *p;

	name = bm->bm_path;
	for (name2 = name; *name2; ++name2) {
		if (*name2 == ' ' || *name2 == '\t') {
			strlcpy(name_buf, name, sizeof(name_buf));
			if ((uintptr_t)name2 - (uintptr_t)name < sizeof(name_buf))
				name_buf[name2 - name] = '\0';
			name = name_buf;
			break;
		}
	}
	if ((p = strchr(name, ':')) != NULL) {
		/* device specified, use it */
		if (p[1] == '/')
			snprintf(buf, sizeof(buf), "%s", name);
		else {
			p++;
			extract_device(name, dev_buf, sizeof(dev_buf));
			snprintf(buf, sizeof(buf), "%s%s/%s/%s.kmod",
			    dev_buf, module_base, p, p);
		}
	} else {
		/* device not specified; load from kernel device if known */
		if (name[0] == '/')
			snprintf(buf, sizeof(buf), "%s%s", kdev, name);
		else
			snprintf(buf, sizeof(buf), "%s%s/%s/%s.kmod",
			    kdev, module_base, name, name);
	}

	return buf;
}

static int
module_open(boot_module_t *bm, int mode, const char *kdev, bool doload)
{
	int fd;
	const char *path;

	/* check the expanded path first */
	path = module_path(bm, kdev);
	fd = open(path, mode);
	if (fd != -1) {
		if ((howto & AB_SILENT) == 0 && doload)
			printf("Loading %s ", path);
	} else {
		/* now attempt the raw path provided */
		fd = open(bm->bm_path, mode);
		if (fd != -1 && (howto & AB_SILENT) == 0 && doload)
			printf("Loading %s ", bm->bm_path);
	}
	if (!doload && fd == -1) {
		printf("WARNING: couldn't open %s", bm->bm_path);
		if (strcmp(bm->bm_path, path) != 0)
			printf(" (%s)", path);
		printf("\n");
	}
	return fd;
}

static void
module_init(const char *kernel_path)
{
	struct bi_modulelist_entry *bi;
	struct stat st;
	const char *machine;
	char kdev[64];
	char *buf;
	boot_module_t *bm;
	ssize_t len;
	off_t off;
	int err, fd, nfail = 0;

	extract_device(kernel_path, kdev, sizeof(kdev));

	switch (netbsd_elf_class) {
	case ELFCLASS32:
		machine = "i386";
		break;
	case ELFCLASS64:
		machine = "amd64";
		break;
	default:
		machine = "generic";
		break;
	}
	if (netbsd_version / 1000000 % 100 == 99) {
		/* -current */
		snprintf(module_base, sizeof(module_base),
		    "/stand/%s/%d.%d.%d/modules", machine,
		    netbsd_version / 100000000,
		    netbsd_version / 1000000 % 100,
		    netbsd_version / 100 % 100);
	} else if (netbsd_version != 0) {
		/* release */
		snprintf(module_base, sizeof(module_base),
		    "/stand/%s/%d.%d/modules", machine,
		    netbsd_version / 100000000,
		    netbsd_version / 1000000 % 100);
	}

	/* First, see which modules are valid and calculate btinfo size */
	len = sizeof(struct btinfo_modulelist);
	for (bm = boot_modules; bm; bm = bm->bm_next) {
		fd = module_open(bm, 0, kdev, false);
		if (fd == -1) {
			bm->bm_len = -1;
			++nfail;
			continue;
		}
		err = fstat(fd, &st);
		if (err == -1 || st.st_size == -1) {
			printf("WARNING: couldn't stat %s\n", bm->bm_path);
			close(fd);
			bm->bm_len = -1;
			++nfail;
			continue;
		}
		bm->bm_len = st.st_size;
		close(fd);
		len += sizeof(struct bi_modulelist_entry);
	}

	/* Allocate the module list */
	btinfo_modulelist = alloc(len);
	if (btinfo_modulelist == NULL) {
		printf("WARNING: couldn't allocate module list\n");
		wait_sec(MODULE_WARNING_SEC);
		return;
	}
	memset(btinfo_modulelist, 0, len);
	btinfo_modulelist_size = len;

	/* Fill in btinfo structure */
	buf = (char *)btinfo_modulelist;
	btinfo_modulelist->num = 0;
	off = sizeof(struct btinfo_modulelist);

	for (bm = boot_modules; bm; bm = bm->bm_next) {
		if (bm->bm_len == -1)
			continue;
		fd = module_open(bm, 0, kdev, true);
		if (fd == -1)
			continue;
		image_end = (image_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
		len = pread(fd, (void *)(uintptr_t)image_end, SSIZE_MAX);
		if (len < bm->bm_len) {
			if ((howto & AB_SILENT) != 0)
				printf("Loading %s ", bm->bm_path);
			printf(" FAILED\n");
		} else {
			btinfo_modulelist->num++;
			bi = (struct bi_modulelist_entry *)(buf + off);
			off += sizeof(struct bi_modulelist_entry);
			strncpy(bi->path, bm->bm_path, sizeof(bi->path) - 1);
			bi->base = image_end;
			bi->len = len;
			switch (bm->bm_type) {
			    case BM_TYPE_KMOD:
				bi->type = BI_MODULE_ELF;
				break;
			    case BM_TYPE_IMAGE:
				bi->type = BI_MODULE_IMAGE;
				break;
			    case BM_TYPE_FS:
				bi->type = BI_MODULE_FS;
				break;
			    case BM_TYPE_RND:
			    default:
				/* safest -- rnd checks the sha1 */
				bi->type = BI_MODULE_RND;
				break;
			}
			if ((howto & AB_SILENT) == 0)
				printf(" \n");
		}
		if (len > 0)
			image_end += len;
		close(fd);
	}
	btinfo_modulelist->endpa = image_end;

	if (nfail > 0) {
		printf("WARNING: %d module%s failed to load\n",
		    nfail, nfail == 1 ? "" : "s");
#if notyet
		wait_sec(MODULE_WARNING_SEC);
#endif
	}
}

static void
userconf_init(void)
{
	size_t count, len;
	userconf_command_t *uc;
	char *buf;
	off_t off;

	/* Calculate the userconf commands list size */
	count = 0;
	for (uc = userconf_commands; uc != NULL; uc = uc->uc_next)
		count++;
	len = sizeof(*btinfo_userconfcommands) +
	      count * sizeof(struct bi_userconfcommand);

	/* Allocate the userconf commands list */
	btinfo_userconfcommands = alloc(len);
	if (btinfo_userconfcommands == NULL) {
		printf("WARNING: couldn't allocate userconf commands list\n");
		return;
	}
	memset(btinfo_userconfcommands, 0, len);
	btinfo_userconfcommands_size = len;

	/* Fill in btinfo structure */
	buf = (char *)btinfo_userconfcommands;
	off = sizeof(*btinfo_userconfcommands);
	btinfo_userconfcommands->num = 0;
	for (uc = userconf_commands; uc != NULL; uc = uc->uc_next) {
		struct bi_userconfcommand *bi;
		bi = (struct bi_userconfcommand *)(buf + off);
		strncpy(bi->text, uc->uc_text, sizeof(bi->text) - 1);

		off += sizeof(*bi);
		btinfo_userconfcommands->num++;
	}
}

#if defined(PASS_MEMMAP) && defined(__minix)
/*
 * Construct a memory map for the multiboot info structure, with memory ranges
 * as reported by the BIOS.  If successful, set the HAS_MMAP flag.  Code copied
 * largely from bootinfo_memmap.c.
 */
static void
memmap_init(struct multiboot_info * mbi)
{
	multiboot_memory_map_t *mmap;
	int buf[5], i, nranges;

	nranges = 0;
	i = 0;
	do {
		if (getmementry(&i, buf))
			break;
		nranges++;
	} while (i);

	if (nranges == 0)
		return;

	mbi->mmap_length = sizeof(multiboot_memory_map_t) * nranges;

	mmap = alloc(mbi->mmap_length);

	mbi->mmap_addr = vtophys(mmap);

	i = 0;
	while (nranges-- > 0) {
		getmementry(&i, buf);

		/* Stupid tricks to deal with alignment issues. */
		memcpy(&mmap->mm_base_addr, buf, sizeof(buf));
		mmap->mm_size = sizeof(*mmap) - sizeof(mmap->mm_size);
		mmap++;
	}

	mbi->mi_flags |= MULTIBOOT_INFO_HAS_MMAP;
}
#endif /* PASS_MEMMAP && __minix */

int
exec_multiboot(const char *file, char *args)
{
	struct multiboot_info *mbi;
	struct multiboot_module *mbm;
	struct bi_modulelist_entry *bim;
	int		i, len;
	u_long		marks[MARK_MAX];
	u_long		extmem;
	u_long		basemem;
	char		*cmdline;

	mbi = alloc(sizeof(struct multiboot_info));
	mbi->mi_flags = MULTIBOOT_INFO_HAS_MEMORY;

	if (common_load_kernel(file, &basemem, &extmem, 0, 0, marks))
		goto out;

	mbi->mi_mem_upper = extmem;
	mbi->mi_mem_lower = basemem;

	if (args) {
		mbi->mi_flags |= MULTIBOOT_INFO_HAS_CMDLINE;
		len = strlen(file) + 1 + strlen(args) + 1;
		cmdline = alloc(len);
		snprintf(cmdline, len, "%s %s", file, args);
		mbi->mi_cmdline = (char *) vtophys(cmdline);
	}

	/* pull in any modules if necessary */
	if (boot_modules_enabled) {
		module_init(file);
		if (btinfo_modulelist) {
			mbm = alloc(sizeof(struct multiboot_module) *
					   btinfo_modulelist->num);

			bim = (struct bi_modulelist_entry *)
			  (((char *) btinfo_modulelist) +
			   sizeof(struct btinfo_modulelist));
			for (i = 0; i < btinfo_modulelist->num; i++) {
				mbm[i].mmo_start = bim->base;
				mbm[i].mmo_end = bim->base + bim->len;
				mbm[i].mmo_string = (char *)vtophys(bim->path);
				mbm[i].mmo_reserved = 0;
				bim++;
			}
			mbi->mi_flags |= MULTIBOOT_INFO_HAS_MODS;
			mbi->mi_mods_count = btinfo_modulelist->num;
			mbi->mi_mods_addr = vtophys(mbm);
		}
	}

#if defined(PASS_MEMMAP) && defined(__minix)
	/*
	 * The MINIX3 kernel needs a full memory map.  Without it, it will do
	 * silly things such as overwriting the ACPI tables.
	 */
	memmap_init(mbi);
#endif /* PASS_MEMMAP && __minix */

#ifdef DEBUG
	printf("Start @ 0x%lx [%ld=0x%lx-0x%lx]...\n", marks[MARK_ENTRY],
	    marks[MARK_NSYM], marks[MARK_SYM], marks[MARK_END]);
#endif


#if 0
	if (btinfo_symtab.nsym) {
		mbi->mi_flags |= MULTIBOOT_INFO_HAS_ELF_SYMS;
		mbi->mi_elfshdr_addr = marks[MARK_SYM];
	btinfo_symtab.nsym = marks[MARK_NSYM];
	btinfo_symtab.ssym = marks[MARK_SYM];
	btinfo_symtab.esym = marks[MARK_END];
#endif

	multiboot(marks[MARK_ENTRY], vtophys(mbi),
		  x86_trunc_page(mbi->mi_mem_lower*1024));
	panic("exec returned");

out:
        dealloc(mbi, 0);
	return -1;
}

void
x86_progress(const char *fmt, ...)
{
	va_list ap;

	if ((howto & AB_SILENT) != 0)
		return;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}
