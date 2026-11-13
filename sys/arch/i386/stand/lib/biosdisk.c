/*	$NetBSD: biosdisk.c,v 1.49.6.4 2019/12/17 13:01:39 martin Exp $	*/

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
#include <fs/unicode.h>

#include <lib/libsa/saerrno.h>
#include <machine/cpu.h>

#include "libi386.h"
#include "biosdisk_ll.h"
#include "biosdisk.h"
#ifdef _STANDALONE
#include "bootinfo.h"
#endif

#ifndef NO_GPT
#define MAXDEVNAME 39 /* "NAME=" + 34 char part_name */
#else
#define MAXDEVNAME 16
#endif

#ifndef BIOSDISK_BUFSIZE
#define BIOSDISK_BUFSIZE	2048	/* must be large enough for a CD sector */
#endif

#define BIOSDISKNPART 26

struct biosdisk {
	struct biosdisk_ll ll;
	daddr_t         boff;
	char            buf[BIOSDISK_BUFSIZE];
#if !defined(NO_DISKLABEL) || !defined(NO_GPT)
	struct biosdisk_partition part[BIOSDISKNPART];
#endif
};

#if !defined(__minix)
#include <dev/raidframe/raidframevar.h>
#define RF_COMPONENT_INFO_OFFSET   16384   /* from sys/dev/raidframe/rf_netbsdkintf.c */
#define RF_COMPONENT_LABEL_VERSION     2   /* from <dev/raidframe/rf_raid.h> */

#define RAIDFRAME_NDEV 16 /* abitrary limit to 15 raidframe devices */
struct raidframe {
	int	last_unit;
	int	serial;
	int	biosdev;
	int	parent_part;
#ifndef NO_GPT
	char    parent_name[MAXDEVNAME + 1];
#endif
	daddr_t	offset;
	daddr_t	size;
};
#endif


#ifndef NO_GPT
const struct uuid GET_nbsd_raid = GPT_ENT_TYPE_NETBSD_RAIDFRAME;
const struct uuid GET_nbsd_ffs = GPT_ENT_TYPE_NETBSD_FFS;
const struct uuid GET_nbsd_lfs = GPT_ENT_TYPE_NETBSD_LFS;
const struct uuid GET_nbsd_swap = GPT_ENT_TYPE_NETBSD_SWAP;
const struct uuid GET_nbsd_ccd = GPT_ENT_TYPE_NETBSD_CCD;
const struct uuid GET_nbsd_cgd = GPT_ENT_TYPE_NETBSD_CGD;

const struct uuid GET_efi = GPT_ENT_TYPE_EFI;
const struct uuid GET_mbr = GPT_ENT_TYPE_MBR;
const struct uuid GET_fbsd = GPT_ENT_TYPE_FREEBSD;
const struct uuid GET_fbsd_swap = GPT_ENT_TYPE_FREEBSD_SWAP;
const struct uuid GET_fbsd_ufs = GPT_ENT_TYPE_FREEBSD_UFS;
const struct uuid GET_fbsd_vinum = GPT_ENT_TYPE_FREEBSD_VINUM;
const struct uuid GET_fbsd_zfs = GPT_ENT_TYPE_FREEBSD_ZFS;
const struct uuid GET_ms_rsvd = GPT_ENT_TYPE_MS_RESERVED;
const struct uuid GET_ms_basic_data = GPT_ENT_TYPE_MS_BASIC_DATA;
const struct uuid GET_ms_ldm_metadata = GPT_ENT_TYPE_MS_LDM_METADATA;
const struct uuid GET_ms_ldm_data = GPT_ENT_TYPE_MS_LDM_DATA;
const struct uuid GET_linux_data = GPT_ENT_TYPE_LINUX_DATA;
const struct uuid GET_linux_raid = GPT_ENT_TYPE_LINUX_RAID;
const struct uuid GET_linux_swap = GPT_ENT_TYPE_LINUX_SWAP;
const struct uuid GET_linux_lvm = GPT_ENT_TYPE_LINUX_LVM;
const struct uuid GET_apple_hfs = GPT_ENT_TYPE_APPLE_HFS;
const struct uuid GET_apple_ufs = GPT_ENT_TYPE_APPLE_UFS;
const struct uuid GET_minix_mfs = GPT_ENT_TYPE_MINIX_MFS;
const struct uuid GET_bios = GPT_ENT_TYPE_BIOS;

const struct gpt_part gpt_parts[] = {
	{ &GET_nbsd_raid,	"NetBSD RAID" },
	{ &GET_nbsd_ffs,	"NetBSD FFS" },
	{ &GET_nbsd_lfs,	"NetBSD LFS" },
	{ &GET_nbsd_swap,	"NetBSD Swap" },
	{ &GET_nbsd_ccd,	"NetBSD ccd" },
	{ &GET_nbsd_cgd,	"NetBSD cgd" },
	{ &GET_efi,		"EFI System" },
	{ &GET_mbr,		"MBR" },
	{ &GET_fbsd,		"FreeBSD" },
	{ &GET_fbsd_swap,	"FreeBSD Swap" },
	{ &GET_fbsd_ufs,	"FreeBSD UFS" },
	{ &GET_fbsd_vinum,	"FreeBSD Vinum" },
	{ &GET_fbsd_zfs,	"FreeBSD ZFS" },
	{ &GET_ms_rsvd,		"Microsoft Reserved" },
	{ &GET_ms_basic_data,	"Microsoft Basic data" },
	{ &GET_ms_ldm_metadata,	"Microsoft LDM metadata" },
	{ &GET_ms_ldm_data,	"Microsoft LDM data" },
	{ &GET_linux_data,	"Linux data" },
	{ &GET_linux_raid,	"Linux RAID" },
	{ &GET_linux_swap,	"Linux Swap" },
	{ &GET_linux_lvm,	"Linux LVM" },
	{ &GET_apple_hfs,	"Apple HFS" },
	{ &GET_apple_ufs,	"Apple UFS" },
	{ &GET_minix_mfs,	"Minix MFS" },
	{ &GET_bios,		"BIOS Boot (GRUB)" },
};
#endif /* NO_GPT */

struct btinfo_bootdisk bi_disk;
struct btinfo_bootwedge bi_wedge;

#define MBR_PARTS(buf) ((char *)(buf) + offsetof(struct mbr_sector, mbr_parts))

#ifndef	devb2cdb
#define	devb2cdb(bno)	(((bno) * DEV_BSIZE) / ISO_DEFAULT_BLOCK_SIZE)
#endif

static void
dealloc_biosdisk(struct biosdisk *d)
{
#ifndef NO_GPT
	int i;

	for (i = 0; i < __arraycount(d->part); i++) {
		if (d->part[i].part_name != NULL)
			dealloc(d->part[i].part_name, BIOSDISK_PART_NAME_LEN);
	}
#endif

	dealloc(d, sizeof(*d));

	return;
}

static struct biosdisk_partition *
copy_biosdisk_part(struct biosdisk *d)
{
	struct biosdisk_partition *part;

	part = alloc(sizeof(d->part));
	if (part == NULL)
		goto out;

	memcpy(part, d->part, sizeof(d->part));

#ifndef NO_GPT
	int i;

	for (i = 0; i < __arraycount(d->part); i++) {
		if (d->part[i].part_name != NULL) {
			part[i].part_name = alloc(BIOSDISK_PART_NAME_LEN);
			memcpy(part[i].part_name, d->part[i].part_name,
			       BIOSDISK_PART_NAME_LEN);
		}
	}
#endif

out:
	return part;
}

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
		dblk = devb2cdb(dblk);

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
		dealloc_biosdisk(d);
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
bool
guid_is_nil(const struct uuid *u)
{
	static const struct uuid nil = { .time_low = 0 };
	return (memcmp(u, &nil, sizeof(*u)) == 0 ? true : false);
}

bool
guid_is_equal(const struct uuid *a, const struct uuid *b)
{
	return (memcmp(a, b, sizeof(*a)) == 0 ? true : false);
}

#ifndef NO_GPT
static void
part_name_utf8(const uint16_t *utf16_src, size_t utf16_srclen,
	       char *utf8_dst, size_t utf8_dstlen)
{
	char *c = utf8_dst;
	size_t r = utf8_dstlen - 1;
	size_t n;
	int j;

	if (utf8_dst == NULL)
		return;

	for (j = 0; j < utf16_srclen && utf16_src[j] != 0x0000; j++) {
		n = wput_utf8(c, r, le16toh(utf16_src[j]));
		if (n == 0)
			break;
		c += n; r -= n;
	}
	*c = '\0';

	return;
}
#endif

static int
check_gpt(struct biosdisk *d, daddr_t rf_offset, daddr_t sector)
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

	if (gpth.hdr_lba_self + rf_offset != sector)
		return -1;

#ifdef _STANDALONE
	bi_wedge.matchblk = sector;
	bi_wedge.matchnblks = 1;

	md5(bi_wedge.matchhash, d->buf, d->ll.secsize);
#endif

	sectors = sizeof(d->buf)/d->ll.secsize; /* sectors per buffer */
	entries = sizeof(d->buf)/gpth.hdr_entsz; /* entries per buffer */
	entblk = gpth.hdr_lba_table + rf_offset;
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

		for (i = 0; j < BIOSDISKNPART && i < entries; i++) {
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
				else if (guid_is_equal(u, &GET_nbsd_ccd))
					d->part[j].fstype = FS_CCD;
				else if (guid_is_equal(u, &GET_nbsd_cgd))
					d->part[j].fstype = FS_CGD;
				else if (guid_is_equal(u, &GET_minix_mfs))
					d->part[j].fstype = FS_MINIXFS3;
				else
					d->part[j].fstype = FS_OTHER;
#ifndef NO_GPT
				for (int k = 0;
				     k < __arraycount(gpt_parts);
				     k++) {
					if (guid_is_equal(u, gpt_parts[k].guid))
						d->part[j].guid = &gpt_parts[k];
				}
				d->part[j].attr = ep[i].ent_attr;

				d->part[j].part_name =
				    alloc(BIOSDISK_PART_NAME_LEN);
				part_name_utf8(ep[i].ent_name,
					       sizeof(ep[i].ent_name),
					       d->part[j].part_name,
					       BIOSDISK_PART_NAME_LEN);
#endif
				j++;
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
read_gpt(struct biosdisk *d, daddr_t rf_offset, daddr_t rf_size)
{
	struct biosdisk_extinfo ed;
	daddr_t gptsector[2];
	int i, error;

	if (d->ll.type != BIOSDISK_TYPE_HD)
		/* No GPT on floppy and CD */
		return -1;

	if (rf_offset && rf_size) {
		gptsector[0] = rf_offset + GPT_HDR_BLKNO;
		gptsector[1] = rf_offset + rf_size - 1;
	} else {
		gptsector[0] = GPT_HDR_BLKNO;
		if (set_geometry(&d->ll, &ed) == 0 &&
		    d->ll.flags & BIOSDISK_INT13EXT) {
			gptsector[1] = ed.totsec - 1;
			/* Sanity check values returned from BIOS */
			if (ed.sbytes >= 512 &&
			    (ed.sbytes & (ed.sbytes - 1)) == 0)
				d->ll.secsize = ed.sbytes;
		} else {
#ifdef DISK_DEBUG
			printf("Unable to determine extended disk geometry - "
				"using CHS\n");
#endif
			/* at least try some other reasonable values then */
			gptsector[1] = d->ll.chs_sectors - 1;
		}
	}

	for (i = 0; i < __arraycount(gptsector); i++) {
		error = check_gpt(d, rf_offset, gptsector[i]);
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

#if defined(EFIBOOT) && defined(SUPPORT_CD9660)
static int
check_cd9660(struct biosdisk *d)
{
	struct biosdisk_extinfo ed;
	struct iso_primary_descriptor *vd;
	daddr_t bno;

	for (bno = 16;; bno++) {
		if (readsects(&d->ll, bno, 1, d->buf, 0))
			return -1;
		vd = (struct iso_primary_descriptor *)d->buf;
		if (memcmp(vd->id, ISO_STANDARD_ID, sizeof vd->id) != 0)
			return -1;
		if (isonum_711(vd->type) == ISO_VD_END)
			return -1;
		if (isonum_711(vd->type) == ISO_VD_PRIMARY)
			break;
	}
	if (isonum_723(vd->logical_block_size) != ISO_DEFAULT_BLOCK_SIZE)
		return -1;

	if (set_geometry(&d->ll, &ed))
		return -1;

	memset(d->part, 0, sizeof(d->part));
	d->part[0].fstype = FS_ISO9660;
	d->part[0].offset = 0;
	d->part[0].size = ed.totsec;
	return 0;
}
#endif

static int
read_label(struct biosdisk *d, daddr_t offset)
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
	ext_base = offset;
	next_ext = offset;
	for (;;) {
		this_ext = ext_base + next_ext;
		next_ext = offset;
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
				next_ext = mbr[i].mbrp_start + offset;
				continue;
			}
#ifdef COMPAT_386BSD_MBRPART
			if (this_ext == offset && typ == MBR_PTYPE_386BSD)
				sector_386bsd = sector;
#endif
			if (this_ext != offset) {
				if (dflt_lbl.d_npartitions >= MAXPARTITIONS)
					continue;
				p = &dflt_lbl.d_partitions[dflt_lbl.d_npartitions++];
			} else
				p = &dflt_lbl.d_partitions[i];
			p->p_offset = sector;
			p->p_size = mbr[i].mbrp_size;
			p->p_fstype = xlat_mbr_fstype(typ);
		}
		if (next_ext == offset)
			break;
		if (ext_base == offset) {
			ext_base = next_ext;
			next_ext = offset;
		}
	}

	sector = offset;
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

#if defined(EFIBOOT) && defined(SUPPORT_CD9660)
	/* Check CD/DVD */
	error = check_cd9660(d);
	if (error >= 0)
		return error;
#endif

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
read_partitions(struct biosdisk *d, daddr_t offset, daddr_t size)
{
	int error;

	error = -1;

#ifndef NO_GPT
	error = read_gpt(d, offset, size);
	if (error == 0)
		return 0;

#endif
#ifndef NO_DISKLABEL
	error = read_label(d, offset);
	
#endif
	return error;
}
#endif

#ifndef NO_RAIDFRAME
static void
raidframe_probe(struct raidframe *raidframe, int *raidframe_count,
		struct biosdisk *d, int part)
{
	int i = *raidframe_count;
	struct RF_ComponentLabel_s label;
	daddr_t offset;

	if (i + 1 > RAIDFRAME_NDEV)
		return;

	offset = d->part[part].offset;
	if ((biosdisk_read_raidframe(d->ll.dev, offset, &label)) != 0)
		return;

	if (label.version != RF_COMPONENT_LABEL_VERSION)
		printf("Unexpected raidframe label version\n");

	raidframe[i].last_unit = label.last_unit;
	raidframe[i].serial = label.serial_number;
	raidframe[i].biosdev = d->ll.dev;
	raidframe[i].parent_part = part;
#ifndef NO_GPT
	if (d->part[part].part_name)
		strlcpy(raidframe[i].parent_name,
			d->part[part].part_name, MAXDEVNAME);
	else
		raidframe[i].parent_name[0] = '\0';
#endif
	raidframe[i].offset = offset;
	raidframe[i].size = label.__numBlocks;

	(*raidframe_count)++;

	return;
}
#endif

void
biosdisk_probe(void)
{
	struct biosdisk *d;
	struct biosdisk_extinfo ed;
#ifndef NO_RAIDFRAME
	struct raidframe raidframe[RAIDFRAME_NDEV];
	int raidframe_count = 0;
#endif
	uint64_t size;
	int first;
	int i;
#if !defined(NO_DISKLABEL) || !defined(NO_GPT)
	int part;
#endif

	for (i = 0; i < MAX_BIOSDISKS + 2; i++) {
		first = 1;
		d = alloc(sizeof(*d));
		if (d == NULL) {
			printf("Out of memory\n");
			return;
		}
		memset(d, 0, sizeof(*d));
		memset(&ed, 0, sizeof(ed));
		if (i >= MAX_BIOSDISKS)
			d->ll.dev = 0x00 + i - MAX_BIOSDISKS;	/* fd */
		else
			d->ll.dev = 0x80 + i;			/* hd/cd */
		if (set_geometry(&d->ll, &ed))
			goto next_disk;
		printf("disk ");
		switch (d->ll.type) {
		case BIOSDISK_TYPE_CD:
			printf("cd0\n  cd0a\n");
			break;
		case BIOSDISK_TYPE_FD:
			printf("fd%d\n", d->ll.dev & 0x7f);
			printf("  fd%da\n", d->ll.dev & 0x7f);
			break;
		case BIOSDISK_TYPE_HD:
			printf("hd%d", d->ll.dev & 0x7f);
			if (d->ll.flags & BIOSDISK_INT13EXT) {
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
		if (d->ll.type != BIOSDISK_TYPE_HD)
			goto next_disk;

		if (read_partitions(d, 0, 0) != 0)
			goto next_disk;
			
		for (part = 0; part < BIOSDISKNPART; part++) {
			if (d->part[part].size == 0)
				continue;
			if (d->part[part].fstype == FS_UNUSED)
				continue;
#ifndef NO_RAIDFRAME
			if (d->part[part].fstype == FS_RAID)
				raidframe_probe(raidframe,
						&raidframe_count, d, part);
#endif
			if (first) {
				printf(" ");
				first = 0;
			}
#ifndef NO_GPT
			if (d->part[part].part_name &&
			    d->part[part].part_name[0])
				printf(" NAME=%s(", d->part[part].part_name);
			else
#endif
				printf(" hd%d%c(", d->ll.dev & 0x7f, part + 'a');

#ifndef NO_GPT
			if (d->part[part].guid != NULL)
				printf("%s", d->part[part].guid->name);
			else
#endif

			if (d->part[part].fstype < FSMAXTYPES)
				printf("%s",
				  fstypenames[d->part[part].fstype]);
			else
				printf("%d", d->part[part].fstype);
			printf(")");
		}
#endif
		if (first == 0)
			printf("\n");

next_disk:
		dealloc_biosdisk(d);
	}

#ifndef NO_RAIDFRAME
	for (i = 0; i < raidframe_count; i++) {
		size_t secsize;

		if ((d = alloc_biosdisk(raidframe[i].biosdev)) == NULL) {
			printf("Out of memory\n");
			return;
		}

		secsize = d->ll.secsize;

		printf("raidframe raid%d serial %d in ",
		       raidframe[i].last_unit, raidframe[i].serial);
#ifndef NO_GPT
		if (raidframe[i].parent_name[0])
			printf("NAME=%s size ", raidframe[i].parent_name);
		else
#endif
		printf("hd%d%c size ", d->ll.dev & 0x7f,
		       raidframe[i].parent_part + 'a');
		if (raidframe[i].size >= (10ULL * 1024 * 1024 * 1024 / secsize))
			printf("%"PRIu64" GB",
			    raidframe[i].size / (1024 * 1024 * 1024 / secsize));
		else
			printf("%"PRIu64" MB",
			    raidframe[i].size / (1024 * 1024 / secsize));
		printf("\n");

		if (read_partitions(d,
		    raidframe[i].offset + RF_PROTECTED_SECTORS,
		    raidframe[i].size) != 0)
			goto next_raidrame;
			
		first = 1;
		for (part = 0; part < BIOSDISKNPART; part++) {
#ifndef NO_GPT
			bool bootme = d->part[part].attr & GPT_ENT_ATTR_BOOTME;
#else
			bool bootme = 0;
#endif

			if (d->part[part].size == 0)
				continue;
			if (d->part[part].fstype == FS_UNUSED)
				continue;
			if (d->part[part].fstype == FS_RAID)
				continue;
			if (first) {
				printf(" ");
				first = 0;
			}
#ifndef NO_GPT
			if (d->part[part].part_name &&
			    d->part[part].part_name[0])
				printf(" NAME=%s(", d->part[part].part_name);
			else
#endif
				printf(" raid%d%c(", raidframe[i].last_unit,
				       part + 'a');
#ifndef NO_GPT
			if (d->part[part].guid != NULL)
				printf("%s", d->part[part].guid->name);
			else
#endif
			if (d->part[part].fstype < FSMAXTYPES)
				printf("%s",
				  fstypenames[d->part[part].fstype]);
			else
				printf("%d", d->part[part].fstype);
			printf("%s)", bootme ? ", bootme" : "");
		}

next_raidrame:
		if (first == 0)
			printf("\n");

		dealloc_biosdisk(d);
	}
#endif
}

/* Determine likely partition for possible sector number of dos
 * partition.
 */

int
biosdisk_findpartition(int biosdev, daddr_t sector,
		       int *partition, const char **part_name)
{
#if defined(NO_DISKLABEL) && defined(NO_GPT)
	*partition = 0;
	*part_name = NULL;
	return 0;
#else
	int i;
	struct biosdisk *d;
	int biosboot_sector_part = -1;
	int bootable_fs_part = -1;
	int boot_part = 0;
#ifndef NO_GPT
	int gpt_bootme_part = -1;
	static char namebuf[MAXDEVNAME + 1];
#endif

#ifdef DISK_DEBUG
	printf("looking for partition device %x, sector %"PRId64"\n", biosdev, sector);
#endif

	/* default ot first partition */
	*partition = 0;
	*part_name = NULL;

	/* Look for netbsd partition that is the dos boot one */
	d = alloc_biosdisk(biosdev);
	if (d == NULL)
		return -1;

	if (read_partitions(d, 0, 0) == 0) {
		for (i = 0; i < BIOSDISKNPART; i++) {
			if (d->part[i].fstype == FS_UNUSED)
				continue;

			if (d->part[i].offset == sector &&
			    biosboot_sector_part == -1)
				biosboot_sector_part = i;

#ifndef NO_GPT
			if (d->part[i].attr & GPT_ENT_ATTR_BOOTME &&
			    gpt_bootme_part == -1)
				gpt_bootme_part = i;
#endif
			switch (d->part[i].fstype) {
			case FS_BSDFFS:
			case FS_BSDLFS:
			case FS_RAID:
			case FS_CCD:
			case FS_CGD:
			case FS_ISO9660:
				if (bootable_fs_part == -1)
					bootable_fs_part = i;
				break;

			default:
				break;
			}
		}

#ifndef NO_GPT
		if (gpt_bootme_part != -1)
			boot_part = gpt_bootme_part;
		else
#endif
		if (biosboot_sector_part != -1)
			boot_part = biosboot_sector_part;
		else if (bootable_fs_part != -1)
			boot_part = bootable_fs_part;
		else
			boot_part = 0;

		*partition = boot_part;
#ifndef NO_GPT
		if (part_name &&
		    d->part[boot_part].part_name &&
		    d->part[boot_part].part_name[0]) {
			strlcpy(namebuf, d->part[boot_part].part_name,
				BIOSDISK_PART_NAME_LEN);
			*part_name = namebuf;
		}
#endif
	}

	dealloc_biosdisk(d);
	return 0;
#endif /* NO_DISKLABEL && NO_GPT */
}

int
biosdisk_readpartition(int biosdev, daddr_t offset, daddr_t size,
    struct biosdisk_partition **partpp, int *rnum)
{
#if defined(NO_DISKLABEL) && defined(NO_GPT)
	return ENOTSUP;
#else
	struct biosdisk *d;
	struct biosdisk_partition *part;
	int rv;

	/* Look for netbsd partition that is the dos boot one */
	d = alloc_biosdisk(biosdev);
	if (d == NULL)
		return ENOMEM;

	if (read_partitions(d, offset, size)) {
		rv = EINVAL;
		goto out;
	}

	part = copy_biosdisk_part(d);
	if (part == NULL) {
		rv = ENOMEM;
		goto out;
	}

	*partpp = part;
	*rnum = (int)__arraycount(d->part);
	rv = 0;
out:
	dealloc_biosdisk(d);
	return rv;
#endif /* NO_DISKLABEL && NO_GPT */
}

#ifndef NO_RAIDFRAME
int
biosdisk_read_raidframe(int biosdev, daddr_t offset,
			struct RF_ComponentLabel_s *label)
{
#if defined(NO_DISKLABEL) && defined(NO_GPT)
	return ENOTSUP;
#else
	struct biosdisk *d;
	struct biosdisk_extinfo ed;
	daddr_t size;
	int rv = -1;

	/* Look for netbsd partition that is the dos boot one */
	d = alloc_biosdisk(biosdev);
	if (d == NULL)
		goto out;

	if (d->ll.type != BIOSDISK_TYPE_HD)
		/* No raidframe on floppy and CD */
		goto out;

	if (set_geometry(&d->ll, &ed) != 0)
		goto out;

	/* Sanity check values returned from BIOS */
	if (ed.sbytes >= 512 &&
	    (ed.sbytes & (ed.sbytes - 1)) == 0)
		d->ll.secsize = ed.sbytes;

	offset += (RF_COMPONENT_INFO_OFFSET / d->ll.secsize);
	size = roundup(sizeof(*label), d->ll.secsize) / d->ll.secsize;
	if (readsects(&d->ll, offset, size, d->buf, 0))
		goto out;
	memcpy(label, d->buf, sizeof(*label));
	rv = 0;
out:
	if (d != NULL)
		dealloc_biosdisk(d);
	return rv;
#endif /* NO_DISKLABEL && NO_GPT */
}
#endif /* NO_RAIDFRAME */

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

#ifndef NO_RAIDFRAME
#ifndef NO_GPT
static daddr_t
raidframe_part_offset(struct biosdisk *d, int part)
{
	struct biosdisk raidframe;
	daddr_t rf_offset;
	daddr_t rf_size;
	int i, candidate;

	memset(&raidframe, 0, sizeof(raidframe));
	raidframe.ll = d->ll;

	rf_offset = d->part[part].offset + RF_PROTECTED_SECTORS;
	rf_size = d->part[part].size;
	if (read_gpt(&raidframe, rf_offset, rf_size) != 0)
		return RF_PROTECTED_SECTORS;

	candidate = 0;
	for (i = 0; i < BIOSDISKNPART; i++) {
		if (raidframe.part[i].size == 0)
			continue;
		if (raidframe.part[i].fstype == FS_UNUSED)
			continue;
#ifndef NO_GPT
		if (raidframe.part[i].attr & GPT_ENT_ATTR_BOOTME)
			candidate = i;
#endif
	}

	return RF_PROTECTED_SECTORS + raidframe.part[candidate].offset;
}
#endif
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
	error = read_partitions(d, 0, 0);
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

#ifndef NO_RAIDFRAME
	if (d->part[partition].fstype == FS_RAID)
#ifndef NO_GPT
		d->boff += raidframe_part_offset(d, partition);
#else
		d->boff += RF_PROTECTED_SECTORS;
#endif
#endif

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
		dealloc_biosdisk(d);
	return error;
}

#ifndef NO_GPT
static int
biosdisk_find_name(const char *fname, int *biosdev,
		   daddr_t *offset, daddr_t *size)
{
	struct biosdisk *d;
	char name[MAXDEVNAME + 1];
	char *sep;
#ifndef NO_RAIDFRAME
	struct raidframe raidframe[RAIDFRAME_NDEV];
	int raidframe_count = 0;
#endif
	int i;
	int part;
	int ret = -1;

	/* Strip leadinf NAME= and cut after the coloon included */
	strlcpy(name, fname + 5, MAXDEVNAME);
	sep = strchr(name, ':');
	if (sep)
		*sep = '\0';

	for (i = 0; i < MAX_BIOSDISKS; i++) {
		d = alloc(sizeof(*d));
		if (d == NULL) {
			printf("Out of memory\n");
			goto out;
		}

		memset(d, 0, sizeof(*d));
		d->ll.dev = 0x80 + i;			/* hd/cd */
		if (set_geometry(&d->ll, NULL))
			goto next_disk;

		if (d->ll.type != BIOSDISK_TYPE_HD)
			goto next_disk;

		if (read_partitions(d, 0, 0) != 0)
			goto next_disk;
			
		for (part = 0; part < BIOSDISKNPART; part++) {
			if (d->part[part].size == 0)
				continue;
			if (d->part[part].fstype == FS_UNUSED)
				continue;
#ifndef NO_RAIDFRAME
			if (d->part[part].fstype == FS_RAID) {
				raidframe_probe(raidframe,
						&raidframe_count, d, part);
				/*
				 * Do not match RAID partition for a name,
				 * we want to report an inner partition.
				 */
				continue;
			}
#endif
			if (d->part[part].part_name != NULL &&
			    strcmp(d->part[part].part_name, name) == 0) {
				*biosdev = d->ll.dev;
				*offset = d->part[part].offset;
				*size = d->part[part].size;
				ret = 0;
				goto out;
			}

		}
next_disk:
		dealloc_biosdisk(d);
		d = NULL;
	}

#ifndef NO_RAIDFRAME
	for (i = 0; i < raidframe_count; i++) {
		int candidate = -1;

		if ((d = alloc_biosdisk(raidframe[i].biosdev)) == NULL) {
			printf("Out of memory\n");
			goto out;
		}

		if (read_partitions(d,
		    raidframe[i].offset + RF_PROTECTED_SECTORS,
		    raidframe[i].size) != 0)
			goto next_raidframe;
			
		for (part = 0; part < BIOSDISKNPART; part++) {
			bool bootme = d->part[part].attr & GPT_ENT_ATTR_BOOTME;
			if (d->part[part].size == 0)
				continue;
			if (d->part[part].fstype == FS_UNUSED)
				continue;

			if (d->part[part].part_name != NULL &&
			    strcmp(d->part[part].part_name, name) == 0) {
				*biosdev = raidframe[i].biosdev;
				*offset = raidframe[i].offset
					+ RF_PROTECTED_SECTORS
					+ d->part[part].offset;
				*size = d->part[part].size;
				ret = 0;
				goto out;
			}
			if (strcmp(raidframe[i].parent_name, name) == 0) {
				if (candidate == -1 || bootme)
					candidate = part;
				continue;
			}
		}

		if (candidate != -1) {
			*biosdev = raidframe[i].biosdev;
			*offset = raidframe[i].offset
				+ RF_PROTECTED_SECTORS
				+ d->part[candidate].offset;
			*size = d->part[candidate].size;
			ret = 0;
			goto out;
		}

next_raidframe:
		dealloc_biosdisk(d);
		d = NULL;
	}
#endif

out:
	if (d != NULL)
		dealloc_biosdisk(d);

	return ret;
}
#endif

#ifndef NO_RAIDFRAME
static int
biosdisk_find_raid(const char *name, int *biosdev,
		   daddr_t *offset, daddr_t *size)
{
	struct biosdisk *d = NULL;
	struct raidframe raidframe[RAIDFRAME_NDEV];
	int raidframe_count = 0;
	int i;
	int target_unit = 0;
	int target_part;
	int part;
	int ret = -1;

	if (strstr(name, "raid") != name)
		goto out;

#define isnum(c) ((c) >= '0' && (c) <= '9')
	i = 4; /* skip leading "raid" */
	if (!isnum(name[i]))
		goto out;
	do {
		target_unit *= 10;
		target_unit += name[i++] - '0';
	} while (isnum(name[i]));

#define isvalidpart(c) ((c) >= 'a' && (c) <= 'z')

	if (!isvalidpart(name[i]))
		goto out;
	target_part = name[i] - 'a';

	for (i = 0; i < MAX_BIOSDISKS; i++) {
		d = alloc(sizeof(*d));
		if (d == NULL) {
			printf("Out of memory\n");
			goto out;
		}

		memset(d, 0, sizeof(*d));
		d->ll.dev = 0x80 + i;			/* hd/cd */
		if (set_geometry(&d->ll, NULL))
			goto next_disk;

		if (d->ll.type != BIOSDISK_TYPE_HD)
			goto next_disk;

		if (read_partitions(d, 0, 0) != 0)
			goto next_disk;
			
		for (part = 0; part < BIOSDISKNPART; part++) {
			if (d->part[part].size == 0)
				continue;
			if (d->part[part].fstype != FS_RAID)
				continue;
			raidframe_probe(raidframe,
					&raidframe_count, d, part);

		}
next_disk:
		dealloc_biosdisk(d);
		d = NULL;
	}

	for (i = 0; i < raidframe_count; i++) {
		if (raidframe[i].last_unit != target_unit)
			continue;

		if ((d = alloc_biosdisk(raidframe[i].biosdev)) == NULL) {
			printf("Out of memory\n");
			goto out;
		}

		if (read_partitions(d,
		    raidframe[i].offset + RF_PROTECTED_SECTORS,
		    raidframe[i].size) != 0)
			goto next_raidframe;
			
		for (part = 0; part < BIOSDISKNPART; part++) {
			if (d->part[part].size == 0)
				continue;
			if (d->part[part].fstype == FS_UNUSED)
				continue;
			if (part == target_part) {
				*biosdev = raidframe[i].biosdev;
				*offset = raidframe[i].offset
					+ RF_PROTECTED_SECTORS
					+ d->part[part].offset;
				*size = d->part[part].size;
				ret = 0;
				goto out;
			}
		}
next_raidframe:
		dealloc_biosdisk(d);
		d = NULL;
	}
out:
	if (d != NULL)
		dealloc_biosdisk(d);

	return ret;
}
#endif

int
biosdisk_open_name(struct open_file *f, const char *name)
{
#if defined(NO_GPT) && defined(NO_RAIDFRAME)
	return ENXIO;
#else
	struct biosdisk *d = NULL;
	int biosdev;
	daddr_t offset;
	daddr_t size;
	int error = -1;

#ifndef NO_GPT
	if (strstr(name, "NAME=") == name)
		error = biosdisk_find_name(name, &biosdev, &offset, &size);
#endif
#ifndef NO_RAIDFRAME
	if (strstr(name, "raid") == name)
		error = biosdisk_find_raid(name, &biosdev, &offset, &size);
#endif

	if (error != 0) {
		printf("%s not found\n", name);
		error = ENXIO;
		goto out;
	}

	d = alloc_biosdisk(biosdev);
	if (d == NULL) {
		error = ENXIO;
		goto out;
	}

	bi_disk.biosdev = d->ll.dev;
	bi_disk.partition = 0;
	bi_disk.labelsector = -1;

	bi_wedge.biosdev = d->ll.dev;

	/*
	 * If we did not get wedge match info from check_gpt()
	 * compute it now.
	 */
	if (bi_wedge.matchblk == -1) {
		if (readsects(&d->ll, offset, 1, d->buf, 1)) {
#ifdef DISK_DEBUG
       			printf("Error reading sector at %"PRId64"\n", offset);
#endif
			error =  EIO;
			goto out;
		}

		bi_wedge.matchblk = offset;
		bi_wedge.matchnblks = 1;

		md5(bi_wedge.matchhash, d->buf, d->ll.secsize);
	}

	d->boff = offset;

	bi_wedge.startblk = offset;
	bi_wedge.nblks = size;

#ifdef _STANDALONE
	add_biosdisk_bootinfo();
#endif

	f->f_devdata = d;
out:
	if (error && d != NULL)
		dealloc_biosdisk(d);
	return error;
#endif
}



#ifndef LIBSA_NO_FS_CLOSE
int
biosdisk_close(struct open_file *f)
{
	struct biosdisk *d = f->f_devdata;

	/* let the floppy drive go off */
	if (d->ll.type == BIOSDISK_TYPE_FD)
		wait_sec(3);	/* 2s is enough on all PCs I found */

	dealloc_biosdisk(d);
	f->f_devdata = NULL;
	return 0;
}
#endif

int
biosdisk_ioctl(struct open_file *f, u_long cmd, void *arg)
{
	return EIO;
}
