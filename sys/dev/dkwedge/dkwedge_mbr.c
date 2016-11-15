/*	$NetBSD: dkwedge_mbr.c,v 1.8 2014/11/04 07:46:26 mlelstv Exp $	*/

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 * Master Boot Record partition table support for disk wedges
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dkwedge_mbr.c,v 1.8 2014/11/04 07:46:26 mlelstv Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/disk.h>
#include <sys/vnode.h>
#include <sys/malloc.h>

#include <sys/bootblock.h>
#include <sys/disklabel.h>

typedef struct mbr_args {
	struct disk	*pdk;
	struct vnode	*vp;
	void		*buf;
	int		error;
	uint32_t	secsize;
	int		mbr_count;
} mbr_args_t;

static const char *
mbr_ptype_to_str(uint8_t ptype)
{
	const char *str;

	switch (ptype) {
	case MBR_PTYPE_FAT12:	str = DKW_PTYPE_FAT;		break;
	case MBR_PTYPE_FAT16S:	str = DKW_PTYPE_FAT;		break;
	case MBR_PTYPE_FAT16B:	str = DKW_PTYPE_FAT;		break;
	case MBR_PTYPE_NTFS:	str = DKW_PTYPE_NTFS;		break;
	case MBR_PTYPE_FAT32:	str = DKW_PTYPE_FAT;		break;
	case MBR_PTYPE_FAT32L:	str = DKW_PTYPE_FAT;		break;
	case MBR_PTYPE_FAT16L:	str = DKW_PTYPE_FAT;		break;
	case MBR_PTYPE_LNXSWAP:	str = DKW_PTYPE_SWAP;		break;
	case MBR_PTYPE_LNXEXT2:	str = DKW_PTYPE_EXT2FS;		break;
	case MBR_PTYPE_APPLE_UFS:str = DKW_PTYPE_APPLEUFS;	break;
	case MBR_PTYPE_EFI:	str = DKW_PTYPE_FAT;		break;
	default:		str = NULL;			break;
	}

	return (str);
}

static void
getparts(mbr_args_t *a, uint32_t off, uint32_t extoff)
{
	struct dkwedge_info dkw;
	struct mbr_partition *dp;
	struct mbr_sector *mbr;
	const char *ptype;
	int i, error;

	error = dkwedge_read(a->pdk, a->vp, off, a->buf, a->secsize);
	if (error) {
		aprint_error("%s: unable to read MBR @ %u/%u, "
		    "error = %d\n", a->pdk->dk_name, off, a->secsize, a->error);
		a->error = error;
		return;
	}

	mbr = a->buf;
	if (mbr->mbr_magic != htole16(MBR_MAGIC))
		return;

	dp = mbr->mbr_parts;

	for (i = 0; i < MBR_PART_COUNT; i++) {
		/* Extended partitions are handled below. */
		if (dp[i].mbrp_type == 0 ||
		    MBR_IS_EXTENDED(dp[i].mbrp_type))
		    	continue;

		if ((ptype = mbr_ptype_to_str(dp[i].mbrp_type)) == NULL) {
			/*
			 * XXX Should probably just add these...
			 * XXX maybe just have an empty ptype?
			 */
			aprint_verbose("%s: skipping partition %d, "
			    "type 0x%02x\n", a->pdk->dk_name, i,
			    dp[i].mbrp_type);
			continue;
		}
		strcpy(dkw.dkw_ptype, ptype);

		strcpy(dkw.dkw_parent, a->pdk->dk_name);
		dkw.dkw_offset = le32toh(dp[i].mbrp_start);
		dkw.dkw_size = le32toh(dp[i].mbrp_size);

		/*
		 * These get historical disk naming style
		 * wedge names.  We start at 'e', and reserve
		 * 4 slots for each MBR we parse.
		 *
		 * XXX For FAT, we should extract the FAT volume
		 * XXX name.
		 */
		snprintf(dkw.dkw_wname, sizeof(dkw.dkw_wname),
		    "%s%c", a->pdk->dk_name,
		    'e' + (a->mbr_count * MBR_PART_COUNT) + i);

		error = dkwedge_add(&dkw);
		if (error == EEXIST)
			aprint_error("%s: wedge named '%s' already "
			    "exists, manual intervention required\n",
			    a->pdk->dk_name, dkw.dkw_wname);
		else if (error)
			aprint_error("%s: error %d adding partition "
			    "%d type 0x%02x\n", a->pdk->dk_name, error,
			    (a->mbr_count * MBR_PART_COUNT) + i,
			    dp[i].mbrp_type);
	}

	/* We've parsed one MBR. */
	a->mbr_count++;

	/* Recursively scan extended partitions. */
	for (i = 0; i < MBR_PART_COUNT; i++) {
		uint32_t poff;

		if (MBR_IS_EXTENDED(dp[i].mbrp_type)) {
			poff = le32toh(dp[i].mbrp_start) + extoff;
			getparts(a, poff, extoff ? extoff : poff);
		}
	}
}

static int
dkwedge_discover_mbr(struct disk *pdk, struct vnode *vp)
{
	mbr_args_t a;

	a.pdk = pdk;
	a.secsize = DEV_BSIZE << pdk->dk_blkshift;  
	a.vp = vp;
	a.buf = malloc(a.secsize, M_DEVBUF, M_WAITOK);
	a.error = 0;
	a.mbr_count = 0;

	getparts(&a, MBR_BBSECTOR, 0);
	if (a.mbr_count != 0)
		a.error = 0;		/* found it, wedges installed */
	else if (a.error == 0)
		a.error = ESRCH;	/* no MBRs found */

	free(a.buf, M_DEVBUF);
	return (a.error);
}

DKWEDGE_DISCOVERY_METHOD_DECL(MBR, 10, dkwedge_discover_mbr);
