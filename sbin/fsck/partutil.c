/*	$NetBSD: partutil.c,v 1.10 2010/03/06 00:30:54 christos Exp $	*/

/*-
 * Copyright (c) 2006 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
__RCSID("$NetBSD: partutil.c,v 1.10 2010/03/06 00:30:54 christos Exp $");

#include <sys/types.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/ioctl.h>
#include <sys/stat.h>


#include <disktab.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <util.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <prop/proplib.h>

#include "partutil.h"

/*
 * Convert disklabel geometry info to disk_geom.
 */
static void
label2geom(struct disk_geom *geo, const struct disklabel *lp)
{
	geo->dg_secperunit = lp->d_secperunit;
	geo->dg_secsize = lp->d_secsize;
	geo->dg_nsectors = lp->d_nsectors;
	geo->dg_ntracks = lp->d_ntracks;
	geo->dg_ncylinders = lp->d_ncylinders;
	geo->dg_secpercyl = lp->d_secpercyl;
	geo->dg_pcylinders = lp->d_ncylinders;
	geo->dg_sparespertrack = lp->d_sparespertrack;
	geo->dg_sparespercyl = lp->d_sparespercyl;
	geo->dg_acylinders = lp->d_acylinders;
}

/*
 * Set what we need to know about disk geometry.
 */
static void
dict2geom(struct disk_geom *geo, prop_dictionary_t dict)
{
	(void)memset(geo, 0, sizeof(struct disk_geom));
	prop_dictionary_get_int64(dict, "sectors-per-unit",
	    &geo->dg_secperunit);
	prop_dictionary_get_uint32(dict, "sector-size", &geo->dg_secsize);
	prop_dictionary_get_uint32(dict, "sectors-per-track",
	    &geo->dg_nsectors);
	prop_dictionary_get_uint32(dict, "tracks-per-cylinder",
	    &geo->dg_ntracks);
	prop_dictionary_get_uint32(dict, "cylinders-per-unit",
	    &geo->dg_ncylinders);
}


static void
part2wedge(struct dkwedge_info *dkw, const struct disklabel *lp, const char *s)
{
#ifdef __minix
	errx(1, "minix doesn't know about wedges");
#else
	struct stat sb;
	const struct partition *pp;
	int ptn;

	(void)memset(dkw, 0, sizeof(*dkw));
	if (stat(s, &sb) == -1)
		return;

	ptn = strchr(s, '\0')[-1] - 'a';
	if ((unsigned)ptn >= lp->d_npartitions ||
	    (devminor_t)ptn != DISKPART(sb.st_rdev))
		return;

	pp = &lp->d_partitions[ptn];
	dkw->dkw_offset = pp->p_offset;
	dkw->dkw_size = pp->p_size;
	dkw->dkw_parent[0] = '*';
	switch (pp->p_fstype) {
	default:
		(void)strcpy(dkw->dkw_ptype, DKW_PTYPE_UNKNOWN);
		break;
	case FS_UNUSED:
		(void)strcpy(dkw->dkw_ptype, DKW_PTYPE_UNUSED);
		break;
	case FS_SWAP:
		(void)strcpy(dkw->dkw_ptype, DKW_PTYPE_SWAP);
		break;
	case FS_BSDFFS:
		(void)strcpy(dkw->dkw_ptype, DKW_PTYPE_FFS);
		break;
	case FS_BSDLFS:
		(void)strcpy(dkw->dkw_ptype, DKW_PTYPE_LFS);
		break;
	case FS_EX2FS:
		(void)strcpy(dkw->dkw_ptype, DKW_PTYPE_EXT2FS);
		break;
	case FS_ISO9660:
		(void)strcpy(dkw->dkw_ptype, DKW_PTYPE_ISO9660);
		break;
	case FS_ADOS:
		(void)strcpy(dkw->dkw_ptype, DKW_PTYPE_AMIGADOS);
		break;
	case FS_HFS:
		(void)strcpy(dkw->dkw_ptype, DKW_PTYPE_APPLEHFS);
		break;
	case FS_MSDOS:
		(void)strcpy(dkw->dkw_ptype, DKW_PTYPE_FAT);
		break;
	case FS_FILECORE:
		(void)strcpy(dkw->dkw_ptype, DKW_PTYPE_FILECORE);
		break;
	case FS_APPLEUFS:
		(void)strcpy(dkw->dkw_ptype, DKW_PTYPE_APPLEUFS);
		break;
	case FS_NTFS:
		(void)strcpy(dkw->dkw_ptype, DKW_PTYPE_NTFS);
		break;
	}
#endif
}

int
getdiskinfo(const char *s, int fd, const char *dt, struct disk_geom *geo,
    struct dkwedge_info *dkw)
{
	struct disklabel lab;
	struct disklabel *lp = &lab;
	prop_dictionary_t disk_dict, geom_dict;

	if (dt) {
#ifdef __minix
		errx(1, "minix doesn't know about disk types (%s)", dt);
#else
		lp = getdiskbyname(dt);
		if (lp == NULL)
			errx(1, "unknown disk type `%s'", dt);
#endif
	}

	/* Get disk description dictionary */
#ifndef __minix
	if (prop_dictionary_recv_ioctl(fd, DIOCGDISKINFO, &disk_dict)) {
#else
	if (-1) {
#endif
		/*
		 * Ask for disklabel if DIOCGDISKINFO failed. This is
		 * compatibility call and can be removed when all devices
		 * will support DIOCGDISKINFO.
		 * cgd, ccd pseudo disk drives doesn't support DIOCGDDISKINFO
		 */
		if (ioctl(fd, DIOCGDINFO, lp) == -1) {
			warn("DIOCGDINFO on %s failed", s);
			return -1;
		}
		label2geom(geo, lp);
	} else {
		geom_dict = prop_dictionary_get(disk_dict, "geometry");
		dict2geom(geo, geom_dict);
	}
	
	/* Get info about partition/wedge */
	if (ioctl(fd, DIOCGWEDGEINFO, dkw) == -1) {
		if (ioctl(fd, DIOCGDINFO, lp) == -1)
			err(1, "Please implement DIOCGWEDGEINFO or "
			    "DIOCGDINFO for disk device %s", s);

		part2wedge(dkw, lp, s);
	}

	return 0;
}
