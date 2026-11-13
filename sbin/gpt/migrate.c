/*-
 * Copyright (c) 2002 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#ifdef __FBSDID
__FBSDID("$FreeBSD: src/sbin/gpt/migrate.c,v 1.16 2005/09/01 02:42:52 marcel Exp $");
#endif
#ifdef __RCSID
__RCSID("$NetBSD: migrate.c,v 1.35 2019/03/03 02:28:14 jnemeth Exp $");
#endif

#include <sys/types.h>
#include <sys/param.h>
#define FSTYPENAMES
#define MBRPTYPENAMES
#ifdef HAVE_NBTOOL_CONFIG_H
#include <nbinclude/sys/bootblock.h>
#include <nbinclude/sys/disklabel.h>
#else
#include <sys/bootblock.h>
#include <sys/disklabel.h>
#endif

#include <err.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "map.h"
#include "gpt.h"
#include "gpt_private.h"

/*
 * Allow compilation on platforms that do not have a BSD label.
 * The values are valid for amd64, i386 and ia64 disklabels.
 * XXX: use disklabel_params from disklabel.c
 */
#ifndef LABELOFFSET
#define	LABELOFFSET	0
#endif
#ifndef LABELSECTOR
#define	LABELSECTOR	1
#endif
#ifndef RAW_PART
#define	RAW_PART	3
#endif

/* FreeBSD filesystem types that don't match corresponding NetBSD types */
#define	FREEBSD_FS_VINUM	14
#define	FREEBSD_FS_ZFS		27

static int cmd_migrate(gpt_t, int, char *[]);

static const char *migratehelp[] = {
	"[-Afs] [-p partitions]",
};

struct gpt_cmd c_migrate = {
	"migrate",
	cmd_migrate,
	migratehelp, __arraycount(migratehelp),
	GPT_SYNC,
};

#define usage() gpt_usage(NULL, &c_migrate)

static const char *
fstypename(u_int t)
{
	static char buf[64];
	if (t >= __arraycount(fstypenames)) {
		snprintf(buf, sizeof(buf), "*%u*", t);
		return buf;
	}
	return fstypenames[t];
}

static const char *
mbrptypename(u_int t)
{
	static char buf[64];
	size_t i;

	for (i = 0; i < __arraycount(mbr_ptypes); i++)
		if ((u_int)mbr_ptypes[i].id == t)
			return mbr_ptypes[i].name;

	snprintf(buf, sizeof(buf), "*%u*", t);
	return buf;
}

static gpt_type_t
freebsd_fstype_to_gpt_type(gpt_t gpt, u_int i, u_int fstype)
{
	switch (fstype) {
	case FS_UNUSED:
		return GPT_TYPE_INVALID;
	case FS_SWAP:
		return GPT_TYPE_FREEBSD_SWAP;
	case FS_BSDFFS:
		return GPT_TYPE_FREEBSD_UFS;
	case FREEBSD_FS_VINUM:
		return GPT_TYPE_FREEBSD_VINUM;
	case FREEBSD_FS_ZFS:
		return GPT_TYPE_FREEBSD_ZFS;
	default:
		gpt_warnx(gpt, "Unknown FreeBSD partition (%d)", fstype);
		return GPT_TYPE_INVALID;
	}
}

static gpt_type_t
netbsd_fstype_to_gpt_type(gpt_t gpt, u_int i, u_int fstype)
{
	switch (fstype) {
	case FS_UNUSED:
		return GPT_TYPE_INVALID;
	case FS_HFS:
		return GPT_TYPE_APPLE_HFS;
	case FS_EX2FS:
		return GPT_TYPE_LINUX_DATA;
	case FS_SWAP:
		return GPT_TYPE_NETBSD_SWAP;
	case FS_BSDFFS:
		return GPT_TYPE_NETBSD_FFS;
	case FS_BSDLFS:
		return GPT_TYPE_NETBSD_LFS;
	case FS_RAID:
		return GPT_TYPE_NETBSD_RAIDFRAME;
	case FS_CCD:
		return GPT_TYPE_NETBSD_CCD;
	case FS_CGD:
		return GPT_TYPE_NETBSD_CGD;
	default:
		gpt_warnx(gpt, "Partition %u unknown type %s, "
		    "using \"Microsoft Basic Data\"", i, fstypename(fstype));
		return GPT_TYPE_MS_BASIC_DATA;
	}
}

static struct gpt_ent *
migrate_disklabel(gpt_t gpt, off_t start, struct gpt_ent *ent,
    gpt_type_t (*convert)(gpt_t, u_int, u_int))
{
	char *buf;
	struct disklabel *dl;
	off_t ofs, rawofs;
	unsigned int i;
	gpt_type_t type;

	buf = gpt_read(gpt, start + LABELSECTOR, 1);
	if (buf == NULL) {
		gpt_warn(gpt, "Error reading label");
		return NULL;
	}
	dl = (void*)(buf + LABELOFFSET);

	if (le32toh(dl->d_magic) != DISKMAGIC ||
	    le32toh(dl->d_magic2) != DISKMAGIC) {
		gpt_warnx(gpt, "MBR partition without disklabel");
		free(buf);
		return ent;
	}

	rawofs = le32toh(dl->d_partitions[RAW_PART].p_offset) *
	    le32toh(dl->d_secsize);
	for (i = 0; i < le16toh(dl->d_npartitions); i++) {
		if (dl->d_partitions[i].p_fstype == FS_UNUSED)
			continue;
		ofs = le32toh(dl->d_partitions[i].p_offset) *
		    le32toh(dl->d_secsize);
		if (ofs < rawofs)
			rawofs = 0;
	}

	if (gpt->verbose > 1)
		gpt_msg(gpt, "rawofs=%ju", (uintmax_t)rawofs);
	rawofs /= gpt->secsz;

	for (i = 0; i < le16toh(dl->d_npartitions); i++) {
		if (gpt->verbose > 1)
			gpt_msg(gpt, "Disklabel partition %u type %s", i,
			    fstypename(dl->d_partitions[i].p_fstype));

		type = (*convert)(gpt, i, dl->d_partitions[i].p_fstype);
		if (type == GPT_TYPE_INVALID)
			continue;

		gpt_uuid_create(type, ent->ent_type,
		    ent->ent_name, sizeof(ent->ent_name));

		ofs = (le32toh(dl->d_partitions[i].p_offset) *
		    le32toh(dl->d_secsize)) / gpt->secsz;
		ofs = (ofs > 0) ? ofs - rawofs : 0;
		ent->ent_lba_start = htole64((uint64_t)ofs);
		ent->ent_lba_end = htole64((uint64_t)(ofs +
		    (off_t)le32toh((uint64_t)dl->d_partitions[i].p_size)
		    - 1LL));
		ent++;
	}

	free(buf);
	return ent;
}

static int
migrate(gpt_t gpt, u_int parts, int force, int slice, int active)
{
	off_t last = gpt_last(gpt);
	map_t map;
	struct gpt_ent *ent;
	struct mbr *mbr;
	uint32_t start, size;
	unsigned int i;
	gpt_type_t type = GPT_TYPE_INVALID;

	map = map_find(gpt, MAP_TYPE_MBR);
	if (map == NULL || map->map_start != 0) {
		gpt_warnx(gpt, "No MBR in disk to convert");
		return -1;
	}

	mbr = map->map_data;

	if (gpt_create(gpt, last, parts, 0) == -1)
		return -1;

	ent = gpt->tbl->map_data;

	/* Mirror partitions. */
	for (i = 0; i < 4; i++) {
		start = le16toh(mbr->mbr_part[i].part_start_hi);
		start = (start << 16) + le16toh(mbr->mbr_part[i].part_start_lo);
		size = le16toh(mbr->mbr_part[i].part_size_hi);
		size = (size << 16) + le16toh(mbr->mbr_part[i].part_size_lo);

		if (gpt->verbose > 1)
			gpt_msg(gpt, "MBR partition %u type %s", i,
			    mbrptypename(mbr->mbr_part[i].part_typ));
		switch (mbr->mbr_part[i].part_typ) {
		case MBR_PTYPE_UNUSED:
			continue;

		case MBR_PTYPE_386BSD: /* FreeBSD */
			if (slice) {
				type = GPT_TYPE_FREEBSD;
				break;
			} else {
				ent = migrate_disklabel(gpt, start, ent,
				    freebsd_fstype_to_gpt_type);
				continue;
			}

		case MBR_PTYPE_NETBSD:	/* NetBSD */
			ent = migrate_disklabel(gpt, start, ent,
			    netbsd_fstype_to_gpt_type);
			continue;

		case MBR_PTYPE_EFI:
			type = GPT_TYPE_EFI;
			break;

		case MBR_PTYPE_FAT12:
		case MBR_PTYPE_FAT16S:
		case MBR_PTYPE_FAT16B:
		case MBR_PTYPE_NTFS:
		case MBR_PTYPE_FAT32:
		case MBR_PTYPE_FAT32L:
		case MBR_PTYPE_FAT16L:
		case MBR_PTYPE_OS2_DOS12:
		case MBR_PTYPE_OS2_DOS16S:
		case MBR_PTYPE_OS2_DOS16B:
		case MBR_PTYPE_OS2_IFS:
		case MBR_PTYPE_HID_FAT32:
		case MBR_PTYPE_HID_FAT32_LBA:
		case MBR_PTYPE_HID_FAT16_LBA:
			type = GPT_TYPE_MS_BASIC_DATA;
			break;

		default:
			if (!force) {
				gpt_warnx(gpt, "unknown partition type (%d)",
				    mbr->mbr_part[i].part_typ);
				return -1;
			}
			continue;
		}
		gpt_uuid_create(type, ent->ent_type, ent->ent_name,
		    sizeof(ent->ent_name));
		ent->ent_lba_start = htole64((uint64_t)start);
		ent->ent_lba_end = htole64((uint64_t)(start + size - 1LL));
		ent++;
	}

	if (gpt_write_primary(gpt) == -1)
		return -1;

	if (gpt_write_backup(gpt) == -1)
		return -1;

	/*
	 * Turn the MBR into a Protective MBR.
	 */
	memset(mbr->mbr_part, 0, sizeof(mbr->mbr_part));
	gpt_create_pmbr_part(mbr->mbr_part, last, active);
	if (gpt_write(gpt, map) == -1) {
		gpt_warn(gpt, "Cant write PMBR");
		return -1;
	}
	return 0;
}

static int
cmd_migrate(gpt_t gpt, int argc, char *argv[])
{
	int ch;
	int force = 0;
	int slice = 0;
	int active = 0;
	u_int parts = 128;

	/* Get the migrate options */
	while ((ch = getopt(argc, argv, "Afp:s")) != -1) {
		switch(ch) {
		case 'A':
			active = 1;
			break;
		case 'f':
			force = 1;
			break;
		case 'p':
			if (gpt_uint_get(gpt, &parts) == -1)
				return usage();
			break;
		case 's':
			slice = 1;
			break;
		default:
			return usage();
		}
	}

	if (argc != optind)
		return usage();

	return migrate(gpt, parts, force, slice, active);
}
