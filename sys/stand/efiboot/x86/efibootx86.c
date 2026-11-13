/*	$NetBSD$	*/

/*-
 * Copyright (c) 2026 Naomichi Nonaka
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
 */

/*
 * x86 (ia32/x64) common machine dependent code.
 *
 * The native NetBSD/x86 boot protocol hands the kernel a bootinfo
 * (BTINFO_*) list; it is assembled here and finalized after
 * ExitBootServices, which efi_md_prepare_netbsd() calls on success.
 */

#include "../efiboot.h"
#include "../efiblock.h"
#include "../module.h"

#include "efibootx86.h"

#include <sys/bitops.h>
#include <sys/boot_flag.h>
#include <sys/md5.h>
#include <sys/param.h>

#include <loadfile.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE	4096
#endif

uint32_t boot_argv[BOOT_NARGS];

static struct btinfo_console btinfo_console = { .devname = "pc" };
static struct btinfo_bootpath btinfo_bootpath;
static struct btinfo_rootdevice bi_root;
static struct btinfo_bootdisk bi_disk;
static struct btinfo_bootwedge bi_wedge;
static struct btinfo_symtab btinfo_symtab;
static struct btinfo_framebuffer btinfo_framebuffer;
static struct btinfo_efi btinfo_efi;
static struct btinfo_efimemmap *btinfo_efimemmap;
static struct btinfo_modulelist *btinfo_modulelist;
static size_t btinfo_modulelist_size;

/* per-boot state shared with the module_foreach/userconf_foreach callbacks */
static size_t bi_modules_space;
static int bi_modules_count;
static struct bi_modulelist_entry *bi_module_entry;
static u_long bi_module_curaddr;
static int bi_module_nfail;
static int bi_userconf_count;
static struct bi_userconfcommand *bi_userconf_entry;

/*
 * Set BTINFO_CONSOLE for the kernel.  The loader itself stays on the
 * EFI console; use the firmware's console redirection for a serial
 * loader console.
 */
void
command_consdev(char *arg)
{
	static const int comioport[4] = { 0x3f8, 0x2f8, 0x3e8, 0x2e8 };
	char *sep;
	int speed = 9600, unit;

	if (arg == NULL || *arg == '\0') {
		if (strcmp(btinfo_console.devname, "com") == 0) {
			printf("console: com, addr 0x%x, speed %d\n",
			    btinfo_console.addr, btinfo_console.speed);
		} else {
			printf("console: %s\n", btinfo_console.devname);
		}
		return;
	}

	if (strcmp(arg, "pc") == 0) {
		memset(&btinfo_console, 0, sizeof(btinfo_console));
		strlcpy(btinfo_console.devname, "pc",
		    sizeof(btinfo_console.devname));
		return;
	}

	if (strncmp(arg, "com", 3) == 0 && arg[3] >= '0' && arg[3] <= '3' &&
	    (arg[4] == '\0' || arg[4] == ',')) {
		unit = arg[3] - '0';
		sep = strchr(arg, ',');
		if (sep != NULL) {
			speed = atoi(sep + 1);
			if (speed <= 0)
				goto error;
		}
		strlcpy(btinfo_console.devname, "com",
		    sizeof(btinfo_console.devname));
		btinfo_console.addr = comioport[unit];
		btinfo_console.speed = speed;
		printf("kernel console: com%d, addr 0x%x, speed %d\n",
		    unit, btinfo_console.addr, btinfo_console.speed);
		return;
	}

error:
	printf("invalid console device.\n");
}

void
command_memmap(char *arg)
{
	bool sorted = true;
	bool compact = false;

	if (arg == NULL || *arg == '\0' || strcmp(arg, "sorted") == 0)
		/* Already sorted is true. */;
	else if (strcmp(arg, "unsorted") == 0)
		sorted = false;
	else if (strcmp(arg, "compact") == 0)
		compact = true;
	else {
		printf("invalid flag, "
		    "must be 'sorted', 'unsorted' or 'compact'.\n");
		return;
	}

	efi_memory_show_map(sorted, compact);
}

void
command_root(char *arg)
{
	struct btinfo_rootdevice *biv = &bi_root;

	strncpy(biv->devname, arg, sizeof(biv->devname));
	if (biv->devname[sizeof(biv->devname) - 1] != '\0') {
		biv->devname[sizeof(biv->devname) - 1] = '\0';
		printf("truncated to %s\n", biv->devname);
	}
}

static int
parse_bootargs(const char *args)
{
	const char *cp;
	int bhowto = 0;

	for (cp = args; *cp != '\0'; cp++) {
		if (*cp != '-')
			continue;
		while (*++cp != '\0' && *cp != ' ')
			BOOT_FLAG(*cp, bhowto);
		if (*cp == '\0')
			break;
	}

	return bhowto;
}

static uint8_t
getdepth(const EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info)
{

	switch (info->PixelFormat) {
	case PixelBlueGreenRedReserved8BitPerColor:
	case PixelRedGreenBlueReserved8BitPerColor:
		return 32;

	case PixelBitMask:
		return fls32(info->PixelInformation.RedMask
		    | info->PixelInformation.GreenMask
		    | info->PixelInformation.BlueMask
		    | info->PixelInformation.ReservedMask);

	case PixelBltOnly:
	case PixelFormatMax:
		return 0;
	}
	return 0;
}

static void
setpixelformat(UINT32 mask, uint8_t *num, uint8_t *pos)
{
	uint8_t n, p;

	n = popcount32(mask);
	p = ffs32(mask);
	if (p > 0)
		p--;

	*num = n;
	*pos = p;
}

static void
bi_framebuffer(void)
{
	EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
	EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
	struct btinfo_framebuffer *fb = &btinfo_framebuffer;

	memset(fb, 0, sizeof(*fb));

	gop = efi_gop_found();
	if (gop == NULL)
		goto out;
	info = gop->Mode->Info;
	if (info->PixelFormat == PixelBltOnly ||
	    info->PixelFormat == PixelFormatMax)
		goto out;

	fb->physaddr = gop->Mode->FrameBufferBase;
	fb->flags = 0;
	fb->width = info->HorizontalResolution;
	fb->height = info->VerticalResolution;
	fb->depth = getdepth(info);
	fb->stride = info->PixelsPerScanLine * ((fb->depth + 7) / 8);
	fb->vbemode = 0;

	switch (info->PixelFormat) {
	case PixelBlueGreenRedReserved8BitPerColor:
		fb->rnum = 8;
		fb->gnum = 8;
		fb->bnum = 8;
		fb->rpos = 16;
		fb->gpos = 8;
		fb->bpos = 0;
		break;

	case PixelRedGreenBlueReserved8BitPerColor:
		fb->rnum = 8;
		fb->gnum = 8;
		fb->bnum = 8;
		fb->rpos = 0;
		fb->gpos = 8;
		fb->bpos = 16;
		break;

	case PixelBitMask:
		setpixelformat(info->PixelInformation.RedMask,
		    &fb->rnum, &fb->rpos);
		setpixelformat(info->PixelInformation.GreenMask,
		    &fb->gnum, &fb->gpos);
		setpixelformat(info->PixelInformation.BlueMask,
		    &fb->bnum, &fb->bpos);
		break;

	default:
		break;
	}

out:
	BI_ADD(fb, BTINFO_FRAMEBUFFER, sizeof(*fb));
}

/*
 * Boot modules (and the rndseed) are loaded page-aligned after the
 * kernel image inside the same allocation, so the startprog copy
 * carries them to their physical home and the kernel's bootstrap
 * mapping (which extends to btinfo_modulelist::endpa) covers them.
 */

static void
bi_module_path(char *buf, size_t bufsize, const char *name)
{
	/* names with a path or device prefix are used as given */
	if (strchr(name, '/') != NULL || strchr(name, ':') != NULL)
		snprintf(buf, bufsize, "%s", name);
	else
		snprintf(buf, bufsize, "%s/%s/%s.kmod",
		    module_prefix, name, name);
}

static off_t
bi_file_size(const char *path)
{
	struct stat st;
	int fd;

	fd = open(path, 0);
	if (fd < 0)
		return -1;
	if (fstat(fd, &st) < 0)
		st.st_size = -1;
	close(fd);
	return st.st_size;
}

static void
bi_module_size_cb(const char *name)
{
	char path[512];
	off_t sz;

	bi_modules_count++;
	bi_module_path(path, sizeof(path), name);
	sz = bi_file_size(path);
	if (sz > 0)
		bi_modules_space += sz + PAGE_SIZE;
}

static void
bi_load_module_file(const char *path, int type)
{
	struct bi_modulelist_entry *bi;
	struct stat st;
	ssize_t len;
	int fd;

	fd = open(path, 0);
	if (fd < 0 || fstat(fd, &st) < 0 || st.st_size <= 0) {
		printf("boot: %s: %s\n", path, strerror(errno));
		if (fd >= 0)
			close(fd);
		bi_module_nfail++;
		return;
	}

	bi_module_curaddr = roundup(bi_module_curaddr, PAGE_SIZE);
	printf("boot: loading %s ", path);
	len = read(fd, (void *)bi_module_curaddr, st.st_size);
	close(fd);
	if (len < st.st_size) {
		printf("FAILED\n");
		bi_module_nfail++;
		return;
	}
	printf("done.\n");

	bi = bi_module_entry++;
	strncpy(bi->path, path, sizeof(bi->path) - 1);
	bi->base = bi_module_curaddr - load_offset;
	bi->len = len;
	bi->type = type;
	btinfo_modulelist->num++;

	bi_module_curaddr += len;
}

static void
bi_module_load_cb(const char *name)
{
	char path[512];

	bi_module_path(path, sizeof(path), name);
	bi_load_module_file(path, BI_MODULE_ELF);
}

static void
bi_modules(u_long *marks)
{
	if (bi_modules_count == 0)
		return;

	btinfo_modulelist_size = sizeof(*btinfo_modulelist) +
	    bi_modules_count * sizeof(struct bi_modulelist_entry);
	btinfo_modulelist = alloc(btinfo_modulelist_size);
	if (btinfo_modulelist == NULL) {
		printf("WARNING: couldn't allocate module list\n");
		return;
	}
	memset(btinfo_modulelist, 0, btinfo_modulelist_size);

	bi_module_entry = (struct bi_modulelist_entry *)(btinfo_modulelist + 1);
	bi_module_curaddr = marks[MARK_END];
	bi_module_nfail = 0;

	if (module_enabled)
		module_foreach(bi_module_load_cb);
	if (get_rndseed_path()[0] != '\0')
		bi_load_module_file(get_rndseed_path(), BI_MODULE_RND);

	if (bi_module_nfail > 0)
		printf("WARNING: %d module%s failed to load\n",
		    bi_module_nfail, bi_module_nfail == 1 ? "" : "s");

	if (btinfo_modulelist->num == 0)
		return;

	btinfo_modulelist->endpa = bi_module_curaddr - load_offset;
	marks[MARK_END] = bi_module_curaddr;

	BI_ADD(btinfo_modulelist, BTINFO_MODULELIST, btinfo_modulelist_size);
}

static void
bi_userconf_count_cb(const char *cmd)
{
	bi_userconf_count++;
}

static void
bi_userconf_fill_cb(const char *cmd)
{
	strncpy(bi_userconf_entry->text, cmd,
	    sizeof(bi_userconf_entry->text) - 1);
	bi_userconf_entry++;
}

static void
bi_userconf(void)
{
	struct btinfo_userconfcommands *bi;
	size_t size;

	bi_userconf_count = 0;
	userconf_foreach(bi_userconf_count_cb);
	if (bi_userconf_count == 0)
		return;

	size = sizeof(*bi) +
	    bi_userconf_count * sizeof(struct bi_userconfcommand);
	bi = alloc(size);
	if (bi == NULL)
		return;
	memset(bi, 0, size);
	bi->num = bi_userconf_count;

	bi_userconf_entry = (struct bi_userconfcommand *)(bi + 1);
	userconf_foreach(bi_userconf_fill_cb);

	BI_ADD(bi, BTINFO_USERCONFCOMMANDS, size);
}

/*
 * Identify the disk/partition the kernel was loaded from so the
 * kernel can find its boot (and thus root) device.  The wedge match
 * hash covers the sector the kernel reads back at DEV_BSIZE
 * granularity (see match_bootwedge in x86_autoconf.c).
 */
static void
bi_bootdisk(void)
{
	struct efi_block_part *bpart;
	struct efi_block_dev *bdev;
	EFI_STATUS status;
	MD5_CTX md5ctx;
	uint32_t bsize;
	char *buf;

	bpart = efi_block_boot_part();
	if (bpart == NULL)
		return;		/* not booted from a block device */
	bdev = bpart->bdev;
	bsize = bdev->bio->Media->BlockSize;

	memset(&bi_disk, 0, sizeof(bi_disk));
	bi_disk.biosdev = 0x80 + bdev->index;
	bi_disk.partition = bpart->index;
	bi_disk.labelsector = -1;

	memset(&bi_wedge, 0, sizeof(bi_wedge));
	bi_wedge.biosdev = bi_disk.biosdev;
	bi_wedge.matchblk = -1;

	switch (bpart->type) {
	case EFI_BLOCK_PART_GPT:
		bi_wedge.startblk = le64toh(bpart->gpt.ent.ent_lba_start);
		bi_wedge.nblks = le64toh(bpart->gpt.ent.ent_lba_end) -
		    le64toh(bpart->gpt.ent.ent_lba_start) + 1;
		bi_wedge.matchblk =
		    (daddr_t)GPT_HDR_BLKNO * (bsize / DEV_BSIZE);
		break;
	case EFI_BLOCK_PART_DISKLABEL:
		bi_disk.labelsector = bpart->disklabel.labelsector;
		bi_disk.label.type = bpart->disklabel.label.type;
		bi_disk.label.checksum = bpart->disklabel.label.checksum;
		memcpy(bi_disk.label.packname, bpart->disklabel.label.packname,
		    sizeof(bi_disk.label.packname));
		bi_wedge.startblk = bpart->disklabel.part.p_offset;
		bi_wedge.nblks = bpart->disklabel.part.p_size;
		bi_wedge.matchblk = bpart->disklabel.labelsector *
		    (daddr_t)(bsize / DEV_BSIZE);
		break;
	case EFI_BLOCK_PART_CD9660:
	default:
		break;
	}

	if (bi_wedge.matchblk != -1) {
		bi_wedge.matchnblks = bsize / DEV_BSIZE;

		buf = alloc(bsize);
		status = efi_block_read(bdev,
		    (UINT64)bi_wedge.matchblk * DEV_BSIZE, buf, bsize);
		if (EFI_ERROR(status)) {
			bi_wedge.matchblk = -1;
			bi_wedge.matchnblks = 0;
		} else {
			MD5Init(&md5ctx);
			MD5Update(&md5ctx, buf, bsize);
			MD5Final(bi_wedge.matchhash, &md5ctx);
		}
		dealloc(buf, bsize);
	}

	BI_ADD(&bi_disk, BTINFO_BOOTDISK, sizeof(bi_disk));
	if (bi_wedge.matchblk != -1)
		BI_ADD(&bi_wedge, BTINFO_BOOTWEDGE, sizeof(bi_wedge));
}

/*
 * Add BTINFO_EFI, leave boot services and capture the final memory
 * map as BTINFO_EFIMEMMAP.  No EFI (console) calls are allowed once
 * this returns.
 */
static void
bi_exit_boot_services(void)
{
	EFI_STATUS status;
	EFI_MEMORY_DESCRIPTOR *desc;
	UINTN NoEntries, MapKey, DescriptorSize;
	UINT32 DescriptorVersion;
	size_t allocsz;
	uint32_t i;

	memset(&btinfo_efi, 0, sizeof(btinfo_efi));
	btinfo_efi.systblpa = (intptr_t)ST;
#ifdef	__i386__	/* bootia32.efi */
	btinfo_efi.flags |= BI_EFI_32BIT;
#endif
	BI_ADD(&btinfo_efi, BTINFO_EFI, sizeof(btinfo_efi));

	NoEntries = 0;
	desc = efi_memory_get_map(&NoEntries, &MapKey, &DescriptorSize,
	    &DescriptorVersion, true);
	status = uefi_call_wrapper(BS->ExitBootServices, 2, IH, MapKey);
	if (EFI_ERROR(status)) {
		FreePool(desc);
		desc = efi_memory_get_map(&NoEntries, &MapKey, &DescriptorSize,
		    &DescriptorVersion, true);
		status = uefi_call_wrapper(BS->ExitBootServices, 2, IH, MapKey);
		if (EFI_ERROR(status))
			panic("ExitBootServices failed");
	}

	efi_memory_compact_map(desc, &NoEntries, DescriptorSize);
	allocsz = sizeof(struct btinfo_efimemmap) - 1
	    + NoEntries * DescriptorSize;
	btinfo_efimemmap = alloc(allocsz);
	btinfo_efimemmap->num = NoEntries;
	btinfo_efimemmap->version = DescriptorVersion;
	btinfo_efimemmap->size = DescriptorSize;
	memcpy(btinfo_efimemmap->memmap, desc, NoEntries * DescriptorSize);
	BI_ADD(btinfo_efimemmap, BTINFO_EFIMEMMAP, allocsz);

	/*
	 * The kernel copy in efi_boot_kernel() may overwrite the loader
	 * image where the static btinfo entries live; move everything
	 * into heap memory.
	 */
	for (i = 0; i < bootinfo->nentries; i++) {
		struct btinfo_common *bi =
		    (void *)(u_long)bootinfo->entry[i];
		char *p = alloc(bi->len);

		memcpy(p, bi, bi->len);
		bootinfo->entry[i] = vtophys(p);
	}
}

/*
 * Native NetBSD boot hooks (exec.c).
 */

size_t
efi_md_boot_alloc_size(const char *fname)
{
	off_t sz;

	bi_modules_space = 0;
	bi_modules_count = 0;

	if (module_enabled) {
		module_init(fname);
		module_foreach(bi_module_size_cb);
	}
	if (get_rndseed_path()[0] != '\0') {
		bi_modules_count++;
		sz = bi_file_size(get_rndseed_path());
		if (sz > 0)
			bi_modules_space += sz + PAGE_SIZE;
	}

	return bi_modules_space;
}

int
efi_md_prepare_netbsd(const char *fname, const char *args, u_long *marks)
{
	u_long basemem, extmem;
	int error;

	error = efi_md_prepare_boot(fname, args, marks);
	if (error)
		return error;

	howto = parse_bootargs(args);

	/* both walk the EFI memory map; boot services still needed */
	basemem = getbasemem();
	extmem = getextmemx();

	BI_ALLOC(BTINFO_MAX);

	BI_ADD(&btinfo_console, BTINFO_CONSOLE, sizeof(btinfo_console));

	if (bi_root.devname[0] != '\0')
		BI_ADD(&bi_root, BTINFO_ROOTDEVICE, sizeof(bi_root));

	strlcpy(btinfo_bootpath.bootpath, fname,
	    sizeof(btinfo_bootpath.bootpath));
	BI_ADD(&btinfo_bootpath, BTINFO_BOOTPATH, sizeof(btinfo_bootpath));

	bi_bootdisk();

	/* marks[] carry the load address; the kernel wants its phys addrs */
	btinfo_symtab.nsym = marks[MARK_NSYM];
	btinfo_symtab.ssym = marks[MARK_SYM] - load_offset;
	btinfo_symtab.esym = marks[MARK_END] - load_offset;
	BI_ADD(&btinfo_symtab, BTINFO_SYMTAB, sizeof(btinfo_symtab));

	/* modules extend marks[MARK_END]; esym above must not include them */
	bi_modules(marks);

	bi_userconf();

	bi_framebuffer();

	bi_exit_boot_services();

	boot_argv[0] = howto;
	boot_argv[1] = 0;
	boot_argv[2] = vtophys(bootinfo);
	boot_argv[3] = btinfo_symtab.esym;
	boot_argv[4] = extmem;
	boot_argv[5] = basemem;

	return 0;
}

void
efi_md_cleanup_boot(void)
{
	if (bootinfo != NULL) {
		BI_FREE();
		bootinfo = NULL;
	}
}
