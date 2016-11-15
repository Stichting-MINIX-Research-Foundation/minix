/*	$NetBSD: dkwedge_apple.c,v 1.2 2015/01/24 02:58:56 christos Exp $	*/

/*-
 * Copyright (c) 2012 The NetBSD Foundation, Inc.
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

/*
 * Apple support for disk wedges
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dkwedge_apple.c,v 1.2 2015/01/24 02:58:56 christos Exp $");

#include <sys/param.h>
#ifdef _KERNEL
#include <sys/systm.h>
#endif
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/disk.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/bitops.h>

#include <sys/bootblock.h>

#define	SWAP16(x)	ap->x = be16toh(ap->x)
#define	SWAP32(x)	ap->x = be32toh(ap->x)

#ifdef DKWEDGE_DEBUG
#define DPRINTF(fmt, ...)	printf(fmt, __VA_ARGS__)
#else
#define DPRINTF(fmt, ...)	
#endif

static void
swap_apple_drvr_descriptor(struct apple_drvr_descriptor *ap)
{
	SWAP32(descBlock);
	SWAP16(descSize);
	SWAP16(descType);
}

static void
swap_apple_drvr_map(struct apple_drvr_map *ap)
{
	uint16_t i;

	SWAP16(sbSig);
	SWAP16(sbBlockSize);
	SWAP32(sbBlkCount);
	SWAP16(sbDevType);
	SWAP16(sbDevID);
	SWAP32(sbData);
	SWAP16(sbDrvrCount);

	if (ap->sbDrvrCount >= APPLE_DRVR_MAP_MAX_DESCRIPTORS)
		ap->sbDrvrCount = APPLE_DRVR_MAP_MAX_DESCRIPTORS;

	for (i = 0; i < ap->sbDrvrCount; i++)
		swap_apple_drvr_descriptor(&ap->sb_dd[i]);
}

static void
swap_apple_part_map_entry(struct apple_part_map_entry *ap)
{
	SWAP16(pmSig);
	SWAP16(pmSigPad);
	SWAP32(pmMapBlkCnt);
	SWAP32(pmPyPartStart);
	SWAP32(pmPartBlkCnt);
	SWAP32(pmLgDataStart);
	SWAP32(pmDataCnt);
	SWAP32(pmPartStatus);
	SWAP32(pmLgBootStart);
	SWAP32(pmBootSize);
	SWAP32(pmBootLoad);
	SWAP32(pmBootLoad2);
	SWAP32(pmBootEntry);
	SWAP32(pmBootEntry2);
	SWAP32(pmBootCksum);
}

static void
swap_apple_blockzeroblock(struct apple_blockzeroblock *ap)
{
        SWAP32(bzbMagic);
        SWAP16(bzbBadBlockInode);
        SWAP16(bzbFlags);
        SWAP16(bzbReserved);
        SWAP32(bzbCreationTime);
        SWAP32(bzbMountTime);
        SWAP32(bzbUMountTime);
}

#undef SWAP16
#undef SWAP32

#define ASIZE	16384
 
#ifdef _KERNEL
#define	DKW_MALLOC(SZ)	malloc((SZ), M_DEVBUF, M_WAITOK)
#define	DKW_FREE(PTR)	free((PTR), M_DEVBUF)
#else
#define	DKW_MALLOC(SZ)	malloc((SZ))
#define	DKW_FREE(PTR)	free((PTR))
#endif

static struct {
	const char *name;
	const char *type;
} map[] = {
	{ APPLE_PART_TYPE_UNIX, DKW_PTYPE_SYSV },
	{ APPLE_PART_TYPE_MAC, DKW_PTYPE_APPLEHFS },
};


static int
dkwedge_discover_apple(struct disk *pdk, struct vnode *vp)
{
	size_t i;
	int error;
	void *buf;
	uint32_t blocksize, offset, rsize;
	struct apple_drvr_map *am;
	struct apple_part_map_entry *ae;
	struct apple_blockzeroblock ab;
	const char *ptype;

	buf = DKW_MALLOC(ASIZE);
	if ((error = dkwedge_read(pdk, vp, 0, buf, ASIZE)) != 0) {
		DPRINTF("%s: read @%u %d\n", __func__, 0, error);
		goto out;
	}

	am = buf;
	swap_apple_drvr_map(am);

	error = ESRCH;

	if (am->sbSig != APPLE_DRVR_MAP_MAGIC) {
		DPRINTF("%s: drvr magic %x != %x\n", __func__, am->sbSig,
		    APPLE_DRVR_MAP_MAGIC);
		goto out;
	}

	blocksize = am->sbBlockSize;

	rsize = 1 << (ilog2(MAX(sizeof(*ae), blocksize) - 1) + 1);
	if (ASIZE < rsize) {
		DPRINTF("%s: buffer too small %u < %u\n", __func__, ASIZE,
		    rsize);
		goto out;
	}

	ae = buf;
	for (offset = blocksize;; offset += rsize) {
		DPRINTF("%s: offset %x rsize %x\n", __func__, offset, rsize);
		if ((error = dkwedge_read(pdk, vp, offset / DEV_BSIZE, buf,
		    rsize)) != 0) {
			DPRINTF("%s: read @%u %d\n", __func__, offset,
			    error);
			goto out;
		}
		
		swap_apple_part_map_entry(ae);
		if (ae->pmSig != APPLE_PART_MAP_ENTRY_MAGIC) {
			DPRINTF("%s: part magic %x != %x\n", __func__,
			    ae->pmSig, APPLE_PART_MAP_ENTRY_MAGIC);
			break;
		}

		for (i = 0; i < __arraycount(map); i++)
			if (strcasecmp(map[i].name, ae->pmPartType) == 0)
				break;

		DPRINTF("%s: %s/%s PH=%u/%u LG=%u/%u\n", __func__,
		    ae->pmPartName, ae->pmPartType,
		    ae->pmPyPartStart, ae->pmPartBlkCnt,
		    ae->pmLgDataStart, ae->pmDataCnt);

		if (i == __arraycount(map))
			continue;

		ptype = map[i].type;
		memcpy(&ab, ae->pmBootArgs, sizeof(ab));
		swap_apple_blockzeroblock(&ab);
		if (ab.bzbMagic == APPLE_BZB_MAGIC) {
			if (ab.bzbType == APPLE_BZB_TYPESWAP)
				ptype = DKW_PTYPE_SWAP;
		}

		struct dkwedge_info dkw;

		strcpy(dkw.dkw_ptype, ptype);
		strcpy(dkw.dkw_parent, pdk->dk_name);
		dkw.dkw_offset = ae->pmPyPartStart;
		dkw.dkw_size = ae->pmPartBlkCnt;
		strlcpy(dkw.dkw_wname, ae->pmPartName, sizeof(dkw.dkw_wname));
		error = dkwedge_add(&dkw);
		if (error == EEXIST)
			aprint_error("%s: wedge named '%s' already "
			    "exists, manual intervention required\n",
			    pdk->dk_name, dkw.dkw_wname);
		else if (error)
			aprint_error("%s: error %d adding partition "
			    "%s type %s\n", pdk->dk_name, error,
			    ae->pmPartType, dkw.dkw_ptype);
	}

out:
	DKW_FREE(buf);
	DPRINTF("%s: return %d\n", __func__, error);
	return error;
}

#ifdef _KERNEL
DKWEDGE_DISCOVERY_METHOD_DECL(APPLE, -5, dkwedge_discover_apple);
#endif
