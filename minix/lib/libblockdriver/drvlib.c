/* IBM device driver utility functions.			Author: Kees J. Bot
 *								7 Dec 1995
 * Entry point:
 *   partition:	partition a disk to the partition table(s) on it.
 */

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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stdbool.h>
#include <unistd.h>
#include <sys/disklabel.h>
#include <sys/disklabel_gpt.h>
#include <sys/uuid.h>

#include <minix/blockdriver.h>
#include <minix/drvlib.h>

extern uint32_t crc32(uint32_t crc, const uint8_t *buf, size_t len);

/* Extended partition? */
#define ext_part(s)	((s) == 0x05 || (s) == 0x0F)


static void parse_part_table(struct blockdriver *bdp, int device,
	int style, int atapi, u8_t *tmp_buf);

static void extpartition(struct blockdriver *bdp, int extdev,
	unsigned long extbase, u8_t *tmp_buf);

static void gptpartition(struct blockdriver *bdp, int rddev, int device,
	int atapi, u8_t *tmp_buf);

static int get_part_table(struct blockdriver *bdp, int device,
	unsigned long offset, struct part_entry *table, u8_t *tmp_buf);

static void sort(struct part_entry *table);

static int read_gpthder(struct blockdriver *bdp, int device,
	struct gpt_hdr *gpth, u8_t *tmp_buf);


#define GPTNPART 26


struct uuid;
bool guid_is_nil(const struct uuid *);
bool guid_is_equal(const struct uuid *, const struct uuid *);


/*============================================================================*
 *				partition				      *
 *============================================================================*/
void partition(
	struct blockdriver *bdp,	/* device dependent entry points */
	int device,             	/* device to partition */
	int style,              	/* partitioning style: floppy, primary, sub. */
	int atapi               	/* atapi device */
)
{
/* This routine is called on first open to initialize the partition tables
 * of a device.
 */
  u8_t *tmp_buf;

  if ((*bdp->bdr_part)(device) == NULL)
	return;

  /* For multithreaded drivers, multiple partition() calls may be made on
   * different devices in parallel. Hence we need a separate temporary buffer
   * for each request.
   */
  if (!(tmp_buf = alloc_contig(CD_SECTOR_SIZE, AC_ALIGN4K, NULL)))
	panic("partition: unable to allocate temporary buffer");

  parse_part_table(bdp, device, style, atapi, tmp_buf);

  free_contig(tmp_buf, CD_SECTOR_SIZE);
}

/*============================================================================*
 *				parse_part_table			      *
 *============================================================================*/
static void parse_part_table(
	struct blockdriver *bdp,	/* device dependent entry points */
	int device,             	/* device to partition */
	int style,              	/* partitioning style: floppy, primary, sub. */
	int atapi,              	/* atapi device */
	u8_t *tmp_buf           	/* temporary buffer */
)
{
/* This routine reads and parses a partition table.  It may be called
 * recursively.  It makes sure that each partition falls safely within the
 * device's limits.  Depending on the partition style we are either making
 * floppy partitions, primary partitions or subpartitions.  Only primary
 * partitions are sorted, because they are shared with other operating
 * systems that expect this.
 */
  struct part_entry table[NR_PARTITIONS], *pe;
  int disk, par;
  int ptype;
  struct device *dv;
  unsigned long base, limit, part_limit;

  /* Get the geometry of the device to partition */
  if ((dv = (*bdp->bdr_part)(device)) == NULL
				|| dv->dv_size == 0) return;
  base = (unsigned long)(dv->dv_base / SECTOR_SIZE);
  limit = base + (unsigned long)(dv->dv_size / SECTOR_SIZE);

  /* Read the partition table for the device. */
  ptype = get_part_table(bdp, device, 0L, table, tmp_buf);
  if (ptype == 0) { /* No Partition */
	  return;
  }

  /* Compute the device number of the first partition. */
  switch (style) {
  case P_FLOPPY:
	device += MINOR_fd0p0;
	break;
  case P_PRIMARY:
	sort(table);		/* sort a primary partition table */
	device += 1;
	break;
  case P_SUB:
	disk = device / DEV_PER_DRIVE;
	par = device % DEV_PER_DRIVE - 1;
	device = MINOR_d0p0s0 + (disk * NR_PARTITIONS + par) * NR_PARTITIONS;
  }

  /* Find an array of devices. */
  if ((dv = (*bdp->bdr_part)(device)) == NULL) return;

  /* Set the geometry of the partitions from the partition table. */
  for (par = 0; par < NR_PARTITIONS; par++, dv++) {
	/* Shrink the partition to fit within the device. */
	pe = &table[par];
	/* GPT Protective partition*/
	if (pe->sysind == 0xEE) {
		/* device-1 is the whole-disk minor (P_PRIMARY added 1); use it
		 * for reads so bounds-checking passes on the unsized partition. */
		gptpartition(bdp, device - 1, device, atapi, tmp_buf);
		return;
	}
	part_limit = pe->lowsec + pe->size;
	if (part_limit < pe->lowsec) part_limit = limit;
	if (part_limit > limit) part_limit = limit;
	if (pe->lowsec < base) pe->lowsec = base;
	if (part_limit < pe->lowsec) part_limit = pe->lowsec;

	dv->dv_base = (u64_t)pe->lowsec * SECTOR_SIZE;
	dv->dv_size = (u64_t)(part_limit - pe->lowsec) * SECTOR_SIZE;

	if (style == P_PRIMARY) {
		/* Each Minix primary partition can be subpartitioned. */
		if (pe->sysind == MINIX_PART)
			parse_part_table(bdp, device + par, P_SUB, atapi,
				tmp_buf);

		/* An extended partition has logical partitions. */
		if (ext_part(pe->sysind))
			extpartition(bdp, device + par, pe->lowsec, tmp_buf);
	}
  }
}

/*============================================================================*
 *				extpartition				      *
 *============================================================================*/
static void extpartition(
	struct blockdriver *bdp,	/* device dependent entry points */
	int extdev,             	/* extended partition to scan */
	unsigned long extbase,  	/* sector offset of the base ext. partition */
	u8_t *tmp_buf           	/* temporary buffer */
)
{
/* Extended partitions cannot be ignored alas, because people like to move
 * files to and from DOS partitions.  Avoid reading this code, it's no fun.
 */
  struct part_entry table[NR_PARTITIONS], *pe;
  int subdev, disk, par;
  struct device *dv;
  unsigned long offset, nextoffset;

  disk = extdev / DEV_PER_DRIVE;
  par = extdev % DEV_PER_DRIVE - 1;
  subdev = MINOR_d0p0s0 + (disk * NR_PARTITIONS + par) * NR_PARTITIONS;

  offset = 0;
  do {
	if (get_part_table(bdp, extdev, offset, table, tmp_buf) != 1) return;
	sort(table);

	/* The table should contain one logical partition and optionally
	 * another extended partition.  (It's a linked list.)
	 */
	nextoffset = 0;
	for (par = 0; par < NR_PARTITIONS; par++) {
		pe = &table[par];
		if (ext_part(pe->sysind)) {
			nextoffset = pe->lowsec;
		} else
		if (pe->sysind != NO_PART) {
			if ((dv = (*bdp->bdr_part)(subdev)) == NULL) return;

			dv->dv_base = (u64_t)(extbase + offset + pe->lowsec) *
								SECTOR_SIZE;
			dv->dv_size = (u64_t)pe->size * SECTOR_SIZE;

			/* Out of devices? */
			if (++subdev % NR_PARTITIONS == 0) return;
		}
	}
  } while ((offset = nextoffset) != 0);
}

/*============================================================================*
 *				get_part_table				      *
 *============================================================================*/
static int get_part_table(
	struct blockdriver *bdp,
	int device,
	unsigned long offset,    	/* sector offset to the table */
	struct part_entry *table,	/* four entries */
	u8_t *tmp_buf)           	/* temporary buffer */
{
/* Read the partition table for the device, return true iff there were no
 * errors.
 * NN: extend to recognize gpt.
 */
  iovec_t iovec1;
  u64_t position;
  int r;

  position = (u64_t)offset * SECTOR_SIZE;
  iovec1.iov_addr = (vir_bytes) tmp_buf;
  iovec1.iov_size = CD_SECTOR_SIZE;
  r = (*bdp->bdr_transfer)(device, FALSE /*do_write*/, position, SELF,
	&iovec1, 1, BDEV_NOFLAGS);
  if (r != CD_SECTOR_SIZE) {
	return 0;
  }
  if (tmp_buf[510] != 0x55 || tmp_buf[511] != 0xAA) {
	/* Invalid partition table. */
	return 0;
  }
  memcpy(table, (tmp_buf + PART_TABLE_OFF), NR_PARTITIONS * sizeof(table[0]));
  return 1;
}

/*===========================================================================*
 *				sort					     *
 *===========================================================================*/
static void sort(struct part_entry *table)
{
/* Sort a partition table. */
  struct part_entry *pe, tmp;
  int n = NR_PARTITIONS;

  do {
	for (pe = table; pe < table + NR_PARTITIONS-1; pe++) {
		if (pe[0].sysind == NO_PART
			|| (pe[0].lowsec > pe[1].lowsec
					&& pe[1].sysind != NO_PART)) {
			tmp = pe[0]; pe[0] = pe[1]; pe[1] = tmp;
		}
	}
  } while (--n > 0);
}



static int readsects(struct blockdriver *bdp, int device, daddr_t dblk, int num, u8_t *tmp_buf)
{
  iovec_t iovec1;
  u64_t position;
  int r;

  position = (u64_t)dblk * SECTOR_SIZE;
  iovec1.iov_addr = (vir_bytes) tmp_buf;
  iovec1.iov_size = (vir_bytes)num * SECTOR_SIZE;
  r = (*bdp->bdr_transfer)(device, FALSE /*do_write*/, position, SELF,
	&iovec1, 1, BDEV_NOFLAGS);
  if (r != (int)((vir_bytes)num * SECTOR_SIZE)) {
	return -1;
  }
  return 0;
}

bool guid_is_nil(const struct uuid *u)
{
	static const struct uuid nil = { .time_low = 0 };
	return (memcmp(u, &nil, sizeof(*u)) == 0 ? true : false);
}

bool guid_is_equal(const struct uuid *a, const struct uuid *b)
{
	return (memcmp(a, b, sizeof(*a)) == 0 ? true : false);
}

static const struct {
	struct uuid guid;
	int fstype;
} gpt_guid_to_fstype[] = {
	{ GPT_ENT_TYPE_NETBSD_FFS,		FS_BSDFFS },
	{ GPT_ENT_TYPE_NETBSD_LFS,		FS_BSDLFS },
	{ GPT_ENT_TYPE_MS_BASIC_DATA,		FS_MSDOS },	/* or NTFS? ambiguous */
	{ GPT_ENT_TYPE_EFI,			FS_MSDOS },
	{ GPT_ENT_TYPE_LINUX_DATA,		FS_EX2FS },
	{ GPT_ENT_TYPE_MINIX_MFS,		FS_MINIXFS3 },
};

static int get_gptent_fstype(const struct gpt_ent *ep)
{
	int fstype = FS_OTHER;
	uint n;

	for (n = 0; n < __arraycount(gpt_guid_to_fstype); n++)
		if (guid_is_equal((const struct uuid *)&ep->ent_type,
				&gpt_guid_to_fstype[n].guid)) {
			fstype = gpt_guid_to_fstype[n].fstype;
			break;
		}
	return fstype;
}

static int check_gpthdr(struct blockdriver *bdp, int device, daddr_t sector,
	struct gpt_hdr *gpth, u8_t *tmp_buf)
{
	daddr_t entblk;
	size_t size;
	uint32_t crc;
	int sectors;
	int entries;
	uint32_t entry;

	/* read in gpt_hdr sector */
	if (readsects(bdp, device, sector, 1, tmp_buf)) {
		return -1;
	}

	memcpy(gpth, tmp_buf, sizeof(struct gpt_hdr));

	if (memcmp(GPT_HDR_SIG, gpth->hdr_sig, sizeof(gpth->hdr_sig)))
		return -1;

	crc = gpth->hdr_crc_self;
	gpth->hdr_crc_self = 0;
	gpth->hdr_crc_self = crc32(0, (const void *)gpth, GPT_HDR_SIZE);
	if (gpth->hdr_crc_self != crc) {
		return -1;
	}

	if (gpth->hdr_lba_self != (uint64_t)sector)
		return -1;

	/* hdr_entsz must be at least sizeof(struct gpt_ent) (UEFI spec: >= 128,
	 * multiple of 8) and fit within our read buffer to avoid division by
	 * zero, an infinite loop, and a buffer overread in the entry scan. */
	if (gpth->hdr_entsz < sizeof(struct gpt_ent) ||
	    gpth->hdr_entsz > CD_SECTOR_SIZE)
		return -1;

	sectors = CD_SECTOR_SIZE/SECTOR_SIZE; /* sectors per buffer */
	entries = CD_SECTOR_SIZE/gpth->hdr_entsz; /* entries per buffer */
	entblk = gpth->hdr_lba_table;
	crc = crc32(0, NULL, 0);

	for (entry = 0; entry < gpth->hdr_entries; entry += entries) {
		size = MIN(CD_SECTOR_SIZE,
		    (gpth->hdr_entries - entry) * gpth->hdr_entsz);
		entries = size / gpth->hdr_entsz;
		sectors = roundup(size, SECTOR_SIZE) / SECTOR_SIZE;
		if (readsects(bdp, device, entblk, sectors, tmp_buf))
			return -1;
		entblk += sectors;
		crc = crc32(crc, (const void *)tmp_buf, size);
	}

	if (crc != gpth->hdr_crc_table) {
		return -1;
	}

	return 0;
}

static int read_gpthder(struct blockdriver *bdp, int device, struct gpt_hdr *gpth,
	u8_t *tmp_buf)
{
	daddr_t gptsector[2];
	struct device *dv;
	int i, error;

	dv = (*bdp->bdr_part)(device);
	if (dv == NULL)
		return -1;

	gptsector[0] = GPT_HDR_BLKNO;
	gptsector[1] = (daddr_t)(dv->dv_size / SECTOR_SIZE) - 1;

	for (i = 0; i < 2; i++) {
		error = check_gpthdr(bdp, device, gptsector[i], gpth, tmp_buf);
		if (error == 0)
			break;
	}

	if (i >= 2) {
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

/*
 *  GPT functions. 
 *  Copied from NetBSD sys/arch/i386/stand/lib/biosdisk.c
 */
/*============================================================================*
 *				gptpartition				      *
 *============================================================================*/
static void gptpartition(
	struct blockdriver *bdp,	/* device dependent entry points */
	int rddev,              	/* whole-disk device for sector reads */
	int device,             	/* first-partition device for entry writes */
	int atapi,              	/* atapi device */
	u8_t *tmp_buf           	/* temporary buffer */
)
{
	struct gpt_hdr gpth;
	struct device gpt_partitions[GPTNPART];
	struct device *dv;
	struct device *part = gpt_partitions;
	daddr_t entblk;
	size_t size;
	int sectors;
	int entries;
	uint32_t entry;
	int i, j;
	int par;

	memset(gpt_partitions, 0, sizeof(gpt_partitions));

	if ((dv = (*bdp->bdr_part)(device)) == NULL)
		return;

	if (read_gpthder(bdp, rddev, &gpth, tmp_buf))
		return;

	/* Find an array of devices. */
	sectors = CD_SECTOR_SIZE/SECTOR_SIZE; /* sectors per buffer */
	entries = CD_SECTOR_SIZE/gpth.hdr_entsz; /* entries per buffer */
	entblk = gpth.hdr_lba_table;

	j = 0;

	for (entry = 0; entry < gpth.hdr_entries; entry += entries) {
		size = MIN(CD_SECTOR_SIZE,
		    (gpth.hdr_entries - entry) * gpth.hdr_entsz);
		entries = size / gpth.hdr_entsz;
		sectors = roundup(size, SECTOR_SIZE) / SECTOR_SIZE;
		if (readsects(bdp, rddev, entblk, sectors, tmp_buf))
			return;
		entblk += sectors;

		for (i = 0; j < GPTNPART && i < entries; i++) {
			const struct gpt_ent *ent = (const struct gpt_ent *)
			    ((const uint8_t *)tmp_buf + (size_t)i * gpth.hdr_entsz);
			if (!guid_is_nil((const struct uuid *)ent->ent_type)) {
				part[j].dv_base = (u64_t)ent->ent_lba_start * SECTOR_SIZE;
				part[j].dv_size = (u64_t)(ent->ent_lba_end -
				    ent->ent_lba_start + 1) * SECTOR_SIZE;
				part[j].fstype = get_gptent_fstype(ent);

				memcpy(&part[j].ent_guid, &ent->ent_guid,
					sizeof(ent->ent_guid));
				memcpy(&part[j].ent_name, &ent->ent_name,
					sizeof(ent->ent_name));
				part[j].ent_attr = ent->ent_attr;

				j++;
			}
		}

	}

    /* Set the geometry of the partitions from the partition table. */
    part = gpt_partitions;
    for (par = 0; par < NR_PARTITIONS; par++, dv++) {
    	*dv = part[par];
		/* Each Minix primary partition can be subpartitioned. */
		if (part[par].fstype == FS_MINIXFS3)
			parse_part_table(bdp, device + par, P_SUB, atapi,
				tmp_buf);
    }
}

