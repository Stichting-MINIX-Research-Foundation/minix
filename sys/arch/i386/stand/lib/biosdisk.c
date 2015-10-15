/*	$NetBSD: biosdisk.c,v 1.44 2015/01/18 20:18:07 jakllsch Exp $	*/

/*
 * Copyright (c) 1996, 1998
 *	Matthias Drochner.  All rights reserved.
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
 *
 */

/*
 * raw BIOS disk device for libsa.
 * needs lowlevel parts from bios_disk.S and biosdisk_ll.c
 * partly from netbsd:sys/arch/i386/boot/disk.c
 * no bad144 handling!
 *
 * A lot of this must match sys/kern/subr_disk_mbr.c
 */

/*
 * Ported to boot 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 *
 * Mach Operating System
 * Copyright (c) 1992, 1991 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#if !defined(NO_DISKLABEL) || !defined(NO_GPT)
#define FSTYPENAMES
#endif

#include <lib/libkern/libkern.h>
#include <lib/libsa/stand.h>

#include <sys/types.h>
#include <sys/md5.h>
#include <sys/param.h>
#include <sys/disklabel.h>
#include <sys/disklabel_gpt.h>
#include <sys/uuid.h>

#include <fs/cd9660/iso.h>

#include <lib/libsa/saerrno.h>
#include <machine/cpu.h>

#include "libi386.h"
#include "biosdisk_ll.h"
#include "biosdisk.h"
#ifdef _STANDALONE
#include "bootinfo.h"
#endif

#define BUFSIZE	2048	/* must be large enough for a CD sector */

#define BIOSDISKNPART 26

struct biosdisk {
	struct biosdisk_ll ll;
	daddr_t         boff;
	char            buf[BUFSIZE];
#if !defined(NO_DISKLABEL) || !defined(NO_GPT)
	struct {
		daddr_t offset;
		daddr_t size;
		int     fstype;
	} part[BIOSDISKNPART];
#endif
};

#ifndef NO_GPT
const struct uuid GET_nbsd_raid = GPT_ENT_TYPE_NETBSD_RAIDFRAME;
const struct uuid GET_nbsd_ffs = GPT_ENT_TYPE_NETBSD_FFS;
const struct uuid GET_nbsd_lfs = GPT_ENT_TYPE_NETBSD_LFS;
const struct uuid GET_nbsd_swap = GPT_ENT_TYPE_NETBSD_SWAP;
#endif /* NO_GPT */

#ifdef _STANDALONE
static struct btinfo_bootdisk bi_disk;
static struct btinfo_bootwedge bi_wedge;
#endif

#define MBR_PARTS(buf) ((char *)(buf) + offsetof(struct mbr_sector, mbr_parts))

#define	RF_PROTECTED_SECTORS	64	/* XXX refer to <.../rf_optnames.h> */

int
biosdisk_strategy(void *devdata, int flag, daddr_t dblk, size_t size,
		  void *buf, size_t *rsize)
{
	struct biosdisk *d;
	int blks, frag;

	if (flag != F_READ)
		return EROFS;

	d = (struct biosdisk *) devdata;

	if (d->ll.type == BIOSDISK_TYPE_CD)
		dblk = dblk * DEV_BSIZE / ISO_DEFAULT_BLOCK_SIZE;

	dblk += d->boff;

	blks = size / d->ll.secsize;
	if (blks && readsects(&d->ll, dblk, blks, buf, 0)) {
		if (rsize)
			*rsize = 0;
		return EIO;
	}

	/* needed for CD */
	frag = size % d->ll.secsize;
	if (frag) {
		if (readsects(&d->ll, dblk + blks, 1, d->buf, 0)) {
			if (rsize)
				*rsize = blks * d->ll.secsize;
			return EIO;
		}
		memcpy(buf + blks * d->ll.secsize, d->buf, frag);
	}

	if (rsize)
		*rsize = size;
	return 0;
}

static struct biosdisk *
alloc_biosdisk(int biosdev)
{
	struct biosdisk *d;

	d = alloc(sizeof(*d));
	if (d == NULL)
		return NULL;
	memset(d, 0, sizeof(*d));

	d->ll.dev = biosdev;
	if (set_geometry(&d->ll, NULL)) {
#ifdef DISK_DEBUG
		printf("no geometry information\n");
#endif
		dealloc(d, sizeof(*d));
		return NULL;
	}
	return d;
}

#if !defined(NO_DISKLABEL) || !defined(NO_GPT)
static void
md5(void *hash, const void *data, size_t len)
{
	MD5_CTX ctx;

	MD5Init(&ctx);
	MD5Update(&ctx, data, len);
	MD5Final(hash, &ctx);

	return;
}
#endif

#ifndef NO_GPT
static bool
guid_is_nil(const struct uuid *u)
{
	static const struct uuid nil = { .time_low = 0 };
	return (memcmp(u, &nil, sizeof(*u)) == 0 ? true : false);
}

static bool
guid_is_equal(const struct uuid *a, const struct uuid *b)
{
	return (memcmp(a, b, sizeof(*a)) == 0 ? true : false);
}

static int
check_gpt(struct biosdisk *d, daddr_t sector)
{
	struct gpt_hdr gpth;
	const struct gpt_ent *ep;
	const struct uuid *u;
	daddr_t entblk;
	size_t size;
	uint32_t crc;
	int sectors;
	int entries;
	int entry;
	int i, j;

	/* read in gpt_hdr sector */
	if (readsects(&d->ll, sector, 1, d->buf, 1)) {
#ifdef DISK_DEBUG
		printf("Error reading GPT header at %"PRId64"\n", sector);
#endif
		return EIO;
	}

	memcpy(&gpth, d->buf, sizeof(gpth));

	if (memcmp(GPT_HDR_SIG, gpth.hdr_sig, sizeof(gpth.hdr_sig)))
		return -1;

	crc = gpth.hdr_crc_self;
	gpth.hdr_crc_self = 0;
	gpth.hdr_crc_self = crc32(0, (const void *)&gpth, GPT_HDR_SIZE);
	if (gpth.hdr_crc_self != crc) {
		return -1;
	}

	if (gpth.hdr_lba_self != sector)
		return -1;

#ifdef _STANDALONE
	bi_wedge.matchblk = sector;
	bi_wedge.matchnblks = 1;

	md5(bi_wedge.matchhash, d->buf, d->ll.secsize);
#endif

	sectors = sizeof(d->buf)/d->ll.secsize; /* sectors per buffer */
	entries = sizeof(d->buf)/gpth.hdr_entsz; /* entries per buffer */
	entblk = gpth.hdr_lba_table;
	crc = crc32(0, NULL, 0);

	j = 0;
	ep = (const struct gpt_ent *)d->buf;

	for (entry = 0; entry < gpth.hdr_entries; entry += entries) {
		size = MIN(sizeof(d->buf),
		    (gpth.hdr_entries - entry) * gpth.hdr_entsz);
		entries = size / gpth.hdr_entsz;
		sectors = roundup(size, d->ll.secsize) / d->ll.secsize;
		if (readsects(&d->ll, entblk, sectors, d->buf, 1))
			return -1;
		entblk += sectors;
		crc = crc32(crc, (const void *)d->buf, size);

		for (i = 0; j < BIOSDISKNPART && i < entries; i++, j++) {
			u = (const struct uuid *)ep[i].ent_type;
			if (!guid_is_nil(u)) {
				d->part[j].offset = ep[i].ent_lba_start;
				d->part[j].size = ep[i].ent_lba_end -
				    ep[i].ent_lba_start + 1;
				if (guid_is_equal(u, &GET_nbsd_ffs))
					d->part[j].fstype = FS_BSDFFS;
				else if (guid_is_equal(u, &GET_nbsd_lfs))
					d->part[j].fstype = FS_BSDLFS;
				else if (guid_is_equal(u, &GET_nbsd_raid))
					d->part[j].fstype = FS_RAID;
				else if (guid_is_equal(u, &GET_nbsd_swap))
					d->part[j].fstype = FS_SWAP;
				else
					d->part[j].fstype = FS_OTHER;
			}
		}

	}

	if (crc != gpth.hdr_crc_table) {
#ifdef DISK_DEBUG	
		printf("GPT table CRC invalid\n");
#endif
		return -1;
	}

	return 0;
}

static int
read_gpt(struct biosdisk *d)
{
	struct biosdisk_extinfo ed;
	daddr_t gptsector[2];
	int i, error;

	if (d->ll.type != BIOSDISK_TYPE_HD)
		/* No GPT on floppy and CD */
		return -1;

	gptsector[0] = GPT_HDR_BLKNO;
	if (set_geometry(&d->ll, &ed) == 0 && d->ll.flags & BIOSDISK_INT13EXT) {
		gptsector[1] = ed.totsec - 1;
		/* Sanity check values returned from BIOS */
		if (ed.sbytes >= 512 && (ed.sbytes & (ed.sbytes - 1)) == 0)
			d->ll.secsize = ed.sbytes;
	} else {
#ifdef DISK_DEBUG
		printf("Unable to determine extended disk geometry - "
			"using CHS\n");
#endif
		/* at least try some other reasonable values then */
		gptsector[1] = d->ll.chs_sectors - 1;
	}

	for (i = 0; i < __arraycount(gptsector); i++) {
		error = check_gpt(d, gptsector[i]);
		if (error == 0)
			break;
	}

	if (i >= __arraycount(gptsector)) {
		memset(d->part, 0, sizeof(d->part));
		return -1;
	}

#ifndef USE_SECONDARY_GPT
	if (i > 0) {
#ifdef DISK_DEBUG
		printf("ignoring valid secondary GPT\n");
#endif
		return -1;
	}
#endif

#ifdef DISK_DEBUG
	printf("using %s GPT\n", (i == 0) ? "primary" : "secondary");
#endif
	return 0;
}
#endif	/* !NO_GPT */

#ifndef NO_DISKLABEL
static void
ingest_label(struct biosdisk *d, struct disklabel *lp)
{
	int part;

	memset(d->part, 0, sizeof(d->part));

	for (part = 0; part < lp->d_npartitions; part++) {
		if (lp->d_partitions[part].p_size == 0)
			continue;
		if (lp->d_partitions[part].p_fstype == FS_UNUSED)
			continue;
		d->part[part].fstype = lp->d_partitions[part].p_fstype;
		d->part[part].offset = lp->d_partitions[part].p_offset;
		d->part[part].size = lp->d_partitions[part].p_size;
	}
}
	
static int
check_label(struct biosdisk *d, daddr_t sector)
{
	struct disklabel *lp;

	/* find partition in NetBSD disklabel */
	if (readsects(&d->ll, sector + LABELSECTOR, 1, d->buf, 0)) {
#ifdef DISK_DEBUG
		printf("Error reading disklabel\n");
#endif
		return EIO;
	}
	lp = (struct disklabel *) (d->buf + LABELOFFSET);
	if (lp->d_magic != DISKMAGIC || dkcksum(lp)) {
#ifdef DISK_DEBUG
		printf("warning: no disklabel in sector %"PRId64"\n", sector);
#endif
		return -1;
	}

	ingest_label(d, lp);

#ifdef _STANDALONE
	bi_disk.labelsector = sector + LABELSECTOR;
	bi_disk.label.type = lp->d_type;
	memcpy(bi_disk.label.packname, lp->d_packname, 16);
	bi_disk.label.checksum = lp->d_checksum;

	bi_wedge.matchblk = sector + LABELSECTOR;
	bi_wedge.matchnblks = 1;

	md5(bi_wedge.matchhash, d->buf, d->ll.secsize);
#endif

	return 0;
}

static int
read_minix_subp(struct biosdisk *d, struct disklabel* dflt_lbl,
			int this_ext, daddr_t sector)
{
	struct mbr_partition mbr[MBR_PART_COUNT];
	int i;
	int typ;
	struct partition *p;

	if (readsects(&d->ll, sector, 1, d->buf, 0)) {
#ifdef DISK_DEBUG
		printf("Error reading MFS sector %"PRId64"\n", sector);
#endif
		return EIO;
	}
	if ((uint8_t)d->buf[510] != 0x55 || (uint8_t)d->buf[511] != 0xAA) {
		return -1;
	}
	memcpy(&mbr, MBR_PARTS(d->buf), sizeof(mbr));
	for (i = 0; i < MBR_PART_COUNT; i++) {
		typ = mbr[i].mbrp_type;
		if (typ == 0)
			continue;
		sector = this_ext + mbr[i].mbrp_start;
		if (dflt_lbl->d_npartitions >= MAXPARTITIONS)
			continue;
		p = &dflt_lbl->d_partitions[dflt_lbl->d_npartitions++];
		p->p_offset = sector;
		p->p_size = mbr[i].mbrp_size;
		p->p_fstype = xlat_mbr_fstype(typ);
	}
	return 0;
}

static int
read_label(struct biosdisk *d)
{
	struct disklabel dflt_lbl;
	struct mbr_partition mbr[MBR_PART_COUNT];
	struct partition *p;
	uint32_t sector;
	int i;
	int error;
	int typ;
	uint32_t ext_base, this_ext, next_ext;
#ifdef COMPAT_386BSD_MBRPART
	int sector_386bsd = -1;
#endif

	memset(&dflt_lbl, 0, sizeof(dflt_lbl));
	dflt_lbl.d_npartitions = 8;

	d->boff = 0;

	if (d->ll.type != BIOSDISK_TYPE_HD)
		/* No label on floppy and CD */
		return -1;

	/*
	 * find NetBSD Partition in DOS partition table
	 * XXX check magic???
	 */
	ext_base = 0;
	next_ext = 0;
	for (;;) {
		this_ext = ext_base + next_ext;
		next_ext = 0;
		if (readsects(&d->ll, this_ext, 1, d->buf, 0)) {
#ifdef DISK_DEBUG
			printf("error reading MBR sector %u\n", this_ext);
#endif
			return EIO;
		}
		memcpy(&mbr, MBR_PARTS(d->buf), sizeof(mbr));
		/* Look for NetBSD partition ID */
		for (i = 0; i < MBR_PART_COUNT; i++) {
			typ = mbr[i].mbrp_type;
			if (typ == 0)
				continue;
			sector = this_ext + mbr[i].mbrp_start;
#ifdef DISK_DEBUG
			printf("ptn type %d in sector %u\n", typ, sector);
#endif
                        if (typ == MBR_PTYPE_MINIX_14B) {
				if (!read_minix_subp(d, &dflt_lbl,
						   this_ext, sector)) {
					/* Don't add "container" partition */
					continue;
				}
			}
			if (typ == MBR_PTYPE_NETBSD) {
				error = check_label(d, sector);
				if (error >= 0)
					return error;
			}
			if (MBR_IS_EXTENDED(typ)) {
				next_ext = mbr[i].mbrp_start;
				continue;
			}
#ifdef COMPAT_386BSD_MBRPART
			if (this_ext == 0 && typ == MBR_PTYPE_386BSD)
				sector_386bsd = sector;
#endif
			if (this_ext != 0) {
				if (dflt_lbl.d_npartitions >= MAXPARTITIONS)
					continue;
				p = &dflt_lbl.d_partitions[dflt_lbl.d_npartitions++];
			} else
				p = &dflt_lbl.d_partitions[i];
			p->p_offset = sector;
			p->p_size = mbr[i].mbrp_size;
			p->p_fstype = xlat_mbr_fstype(typ);
		}
		if (next_ext == 0)
			break;
		if (ext_base == 0) {
			ext_base = next_ext;
			next_ext = 0;
		}
	}

	sector = 0;
#ifdef COMPAT_386BSD_MBRPART
	if (sector_386bsd != -1) {
		printf("old BSD partition ID!\n");
		sector = sector_386bsd;
	}
#endif

	/*
	 * One of two things:
	 * 	1. no MBR
	 *	2. no NetBSD partition in MBR
	 *
	 * We simply default to "start of disk" in this case and
	 * press on.
	 */
	error = check_label(d, sector);
	if (error >= 0)
		return error;

	/*
	 * Nothing at start of disk, return info from mbr partitions.
	 */
	/* XXX fill it to make checksum match kernel one */
	dflt_lbl.d_checksum = dkcksum(&dflt_lbl);
	ingest_label(d, &dflt_lbl);
	return 0;
}
#endif /* NO_DISKLABEL */

#if !defined(NO_DISKLABEL) || !defined(NO_GPT)
static int
read_partitions(struct biosdisk *d)
{
	int error;

	error = -1;

#ifndef NO_GPT
	error = read_gpt(d);
	if (error == 0)
		return 0;

#endif
#ifndef NO_DISKLABEL
	error = read_label(d);
	
#endif
	return error;
}
#endif

void
biosdisk_probe(void)
{
	struct biosdisk d;
	struct biosdisk_extinfo ed;
	uint64_t size;
	int first;
	int i;
#if !defined(NO_DISKLABEL) || !defined(NO_GPT)
	int part;
#endif

	for (i = 0; i < MAX_BIOSDISKS + 2; i++) {
		first = 1;
		memset(&d, 0, sizeof(d));
		memset(&ed, 0, sizeof(ed));
		if (i >= MAX_BIOSDISKS)
			d.ll.dev = 0x00 + i - MAX_BIOSDISKS;	/* fd */
		else
			d.ll.dev = 0x80 + i;			/* hd/cd */
		if (set_geometry(&d.ll, &ed))
			continue;
		printf("disk ");
		switch (d.ll.type) {
		case BIOSDISK_TYPE_CD:
			printf("cd0\n  cd0a\n");
			break;
		case BIOSDISK_TYPE_FD:
			printf("fd%d\n", d.ll.dev & 0x7f);
			printf("  fd%da\n", d.ll.dev & 0x7f);
			break;
		case BIOSDISK_TYPE_HD:
			printf("hd%d", d.ll.dev & 0x7f);
			if (d.ll.flags & BIOSDISK_INT13EXT) {
				printf(" size ");
				size = ed.totsec * ed.sbytes;
				if (size >= (10ULL * 1024 * 1024 * 1024))
					printf("%"PRIu64" GB",
					    size / (1024 * 1024 * 1024));
				else
					printf("%"PRIu64" MB",
					    size / (1024 * 1024));
			}
			printf("\n");
			break;
		}
#if !defined(NO_DISKLABEL) || !defined(NO_GPT)
		if (d.ll.type != BIOSDISK_TYPE_HD)
			continue;

		if (read_partitions(&d) != 0)
			continue;
			
		for (part = 0; part < BIOSDISKNPART; part++) {
			if (d.part[part].size == 0)
				continue;
			if (d.part[part].fstype == FS_UNUSED)
				continue;
			if (first) {
				printf(" ");
				first = 0;
			}
			printf(" hd%d%c(", d.ll.dev & 0x7f, part + 'a');
			if (d.part[part].fstype < FSMAXTYPES)
				printf("%s",
				  fstypenames[d.part[part].fstype]);
			else
				printf("%d", d.part[part].fstype);
			printf(")");
		}
#endif
		if (first == 0)
			printf("\n");
	}
}

/* Determine likely partition for possible sector number of dos
 * partition.
 */

int
biosdisk_findpartition(int biosdev, daddr_t sector)
{
#if defined(NO_DISKLABEL) && defined(NO_GPT)
	return 0;
#else
	struct biosdisk *d;
	int partition = 0;
#ifdef DISK_DEBUG
	printf("looking for partition device %x, sector %"PRId64"\n", biosdev, sector);
#endif

	/* Look for netbsd partition that is the dos boot one */
	d = alloc_biosdisk(biosdev);
	if (d == NULL)
		return 0;

	if (read_partitions(d) == 0) {
		for (partition = (BIOSDISKNPART-1); --partition;) {
			if (d->part[partition].fstype == FS_UNUSED)
				continue;
			if (d->part[partition].offset == sector)
				break;
		}
	}

	dealloc(d, sizeof(*d));
	return partition;
#endif /* NO_DISKLABEL && NO_GPT */
}

#ifdef _STANDALONE
static void
add_biosdisk_bootinfo(void)
{
	static bool done;

	if (bootinfo == NULL) {
		done = false;
		return;
	}
	
	if (done)
		return;

	BI_ADD(&bi_disk, BTINFO_BOOTDISK, sizeof(bi_disk));
	BI_ADD(&bi_wedge, BTINFO_BOOTWEDGE, sizeof(bi_wedge));

	done = true;

	return;
}

#endif

int
biosdisk_open(struct open_file *f, ...)
/* struct open_file *f, int biosdev, int partition */
{
	va_list ap;
	struct biosdisk *d;
	int biosdev;
	int partition;
	int error = 0;

	va_start(ap, f);
	biosdev = va_arg(ap, int);
	d = alloc_biosdisk(biosdev);
	if (d == NULL) {
		error = ENXIO;
		goto out;
	}

	partition = va_arg(ap, int);
#ifdef _STANDALONE
	bi_disk.biosdev = d->ll.dev;
	bi_disk.partition = partition;
	bi_disk.labelsector = -1;

	bi_wedge.biosdev = d->ll.dev;
	bi_wedge.matchblk = -1;
#endif

#if !defined(NO_DISKLABEL) || !defined(NO_GPT)
	error = read_partitions(d);
	if (error == -1) {
		error = 0;
		goto nolabel;
	}
	if (error)
		goto out;

	if (partition >= BIOSDISKNPART ||
	    d->part[partition].fstype == FS_UNUSED) {
#ifdef DISK_DEBUG
		printf("illegal partition\n");
#endif
		error = EPART;
		goto out;
	}

	d->boff = d->part[partition].offset;

	if (d->part[partition].fstype == FS_RAID)
		d->boff += RF_PROTECTED_SECTORS;

#ifdef _STANDALONE
	bi_wedge.startblk = d->part[partition].offset;
	bi_wedge.nblks = d->part[partition].size;
#endif

nolabel:
#endif
#ifdef DISK_DEBUG
	printf("partition @%"PRId64"\n", d->boff);
#endif

#ifdef _STANDALONE
	add_biosdisk_bootinfo();
#endif

	f->f_devdata = d;
out:
        va_end(ap);
	if (error)
		dealloc(d, sizeof(*d));
	return error;
}

#ifndef LIBSA_NO_FS_CLOSE
int
biosdisk_close(struct open_file *f)
{
	struct biosdisk *d = f->f_devdata;

	/* let the floppy drive go off */
	if (d->ll.type == BIOSDISK_TYPE_FD)
		wait_sec(3);	/* 2s is enough on all PCs I found */

	dealloc(d, sizeof(*d));
	f->f_devdata = NULL;
	return 0;
}
#endif

int
biosdisk_ioctl(struct open_file *f, u_long cmd, void *arg)
{
	return EIO;
}
