/*	$NetBSD: dkwedge_bsdlabel.c,v 1.23 2014/11/04 07:45:45 mlelstv Exp $	*/

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
 * Adapted from kern/subr_disk_mbr.c:
 *
 * Copyright (c) 1982, 1986, 1988 Regents of the University of California.
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
 *	@(#)ufs_disksubr.c      7.16 (Berkeley) 5/4/91
 */

/*
 * 4.4BSD disklabel support for disk wedges
 *
 * Here is the basic search algorithm in use here:
 *
 * For historical reasons, we scan for x86-style MBR partitions looking
 * for a MBR_PTYPE_NETBSD (or MBR_PTYPE_386BSD) partition.  The first
 * 4.4BSD disklabel found in the 2nd sector of such a partition is used.
 * We assume that the 4.4BSD disklabel describes all partitions on the
 * disk; we do not use any partition information from the MBR partition
 * table.
 *
 * If that fails, then we fall back on a table of known locations for
 * various platforms.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dkwedge_bsdlabel.c,v 1.23 2014/11/04 07:45:45 mlelstv Exp $");

#include <sys/param.h>
#ifdef _KERNEL
#include <sys/systm.h>
#endif
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/disk.h>
#include <sys/vnode.h>
#include <sys/malloc.h>

#include <sys/bootblock.h>
#include <sys/disklabel.h>

#define	BSD44_MBR_LABELSECTOR	1

#define	DISKLABEL_SIZE(x)						\
	(offsetof(struct disklabel, d_partitions) +			\
	 (sizeof(struct partition) * (x)))

/*
 * Note the smallest MAXPARTITIONS was 8, so we allow a disklabel
 * that size to be locted at the end of the sector.
 */
#define	DISKLABEL_MINSIZE	DISKLABEL_SIZE(8)

/*
 * Table of known platform-specific disklabel locations.
 */
static const struct disklabel_location {
	daddr_t		label_sector;	/* sector containing label */
	size_t		label_offset;	/* byte offset of label in sector */
} disklabel_locations[] = {
	{ 0,	0 },	/* mvme68k, next68k */
	{ 0,	64 },	/* algor, alpha, amiga, amigappc, evbmips, evbppc,
			   luna68k, mac68k, macppc, news68k, newsmips,
			   pc532, pdp11, pmax, vax, x68k */
	{ 0,	128 },	/* sparc, sun68k */
	{ 1,	0 },	/* amd64, arc, arm, bebox, cobalt, evbppc, hppa,
			   hpcarm, hpcmips, i386, ibmnws, mipsco, mvmeppc,
			   ofppc, playstation2, pmppc, prep, sandpoint,
			   sbmips, sgimips, sh3 */
	/* XXX atari is weird */
	{ 2,	0 },	/* cesfic, hp300 */

	{ -1,	0 },
};

#define	SCAN_CONTINUE	0
#define	SCAN_FOUND	1
#define	SCAN_ERROR	2

typedef struct mbr_args {
	struct disk	*pdk;
	struct vnode	*vp;
	void		*buf;
	int		error;
	uint32_t	secsize;
} mbr_args_t;

static const char *
bsdlabel_fstype_to_str(uint8_t fstype)
{
	const char *str;

	/*
	 * For each type known to FSTYPE_DEFN (from <sys/disklabel.h>),
	 * a suitable case branch will convert the type number to a string.
	 */
	switch (fstype) {
#define FSTYPE_TO_STR_CASE(tag, number, name, fsck, mount) \
	case __CONCAT(FS_,tag):	str = __CONCAT(DKW_PTYPE_,tag);			break;
	FSTYPE_DEFN(FSTYPE_TO_STR_CASE)
#undef FSTYPE_TO_STR_CASE
	default:		str = NULL;			break;
	}

	return (str);
}

static void
swap_disklabel(struct disklabel *lp)
{
	int i;

#define	SWAP16(x)	lp->x = bswap16(lp->x)
#define	SWAP32(x)	lp->x = bswap32(lp->x)

	SWAP32(d_magic);
	SWAP16(d_type);
	SWAP16(d_subtype);
	SWAP32(d_secsize);
	SWAP32(d_nsectors);
	SWAP32(d_ntracks);
	SWAP32(d_ncylinders);
	SWAP32(d_secpercyl);
	SWAP32(d_secperunit);
	SWAP16(d_sparespertrack);
	SWAP16(d_sparespercyl);
	SWAP32(d_acylinders);
	SWAP16(d_rpm);
	SWAP16(d_interleave);
	SWAP16(d_trackskew);
	SWAP16(d_cylskew);
	SWAP32(d_headswitch);
	SWAP32(d_trkseek);
	SWAP32(d_flags);

	for (i = 0; i < NDDATA; i++)
		SWAP32(d_drivedata[i]);
	for (i = 0; i < NSPARE; i++)
		SWAP32(d_spare[i]);

	SWAP32(d_magic2);
	SWAP16(d_checksum);
	SWAP16(d_npartitions);
	SWAP32(d_bbsize);
	SWAP32(d_sbsize);

	for (i = 0; i < lp->d_npartitions; i++) {
		SWAP32(d_partitions[i].p_size);
		SWAP32(d_partitions[i].p_offset);
		SWAP32(d_partitions[i].p_fsize);
		SWAP16(d_partitions[i].p_cpg);
	}

#undef SWAP16
#undef SWAP32
}

/*
 * Add wedges for a valid NetBSD disklabel.
 */
static void
addwedges(const mbr_args_t *a, const struct disklabel *lp)
{
	int error, i;

	for (i = 0; i < lp->d_npartitions; i++) {
		struct dkwedge_info dkw;
		const struct partition *p;
		const char *ptype;

		p = &lp->d_partitions[i];

		if (p->p_fstype == FS_UNUSED)
			continue;
		ptype = bsdlabel_fstype_to_str(p->p_fstype);
		if (ptype == NULL)
			snprintf(dkw.dkw_ptype, sizeof(dkw.dkw_ptype),
			    "unknown#%u", p->p_fstype);
		else
			strlcpy(dkw.dkw_ptype, ptype, sizeof(dkw.dkw_ptype));

		strlcpy(dkw.dkw_parent, a->pdk->dk_name,
		    sizeof(dkw.dkw_parent));
		dkw.dkw_offset = p->p_offset;
		dkw.dkw_size = p->p_size;

		/*
		 * If the label defines a name, append the partition
		 * letter and use it as the wedge name.
		 * Otherwise use historical disk naming style
		 * wedge names.
		 */
		if (lp->d_packname[0] &&
		    strcmp(lp->d_packname,"fictitious") != 0) {
			snprintf((char *)&dkw.dkw_wname, sizeof(dkw.dkw_wname),
		    		"%.*s/%c", (int)sizeof(dkw.dkw_wname)-3,
				lp->d_packname, 'a' + i);
		} else {
			snprintf((char *)&dkw.dkw_wname, sizeof(dkw.dkw_wname),
			    "%s%c", a->pdk->dk_name, 'a' + i);
		}

		error = dkwedge_add(&dkw);
		if (error == EEXIST)
			aprint_error("%s: wedge named '%s' already "
			    "exists, manual intervention required\n",
			    a->pdk->dk_name, dkw.dkw_wname);
		else if (error)
			aprint_error("%s: error %d adding partition "
			    "%d type %d\n", a->pdk->dk_name, error,
			    i, p->p_fstype);
	}
}

static int
validate_label(mbr_args_t *a, daddr_t label_sector, size_t label_offset)
{
	struct disklabel *lp;
	void *lp_lim;
	int error, swapped;
	uint16_t npartitions;

	error = dkwedge_read(a->pdk, a->vp, label_sector, a->buf, a->secsize);
	if (error) {
		aprint_error("%s: unable to read BSD disklabel @ %" PRId64
		    ", error = %d\n", a->pdk->dk_name, label_sector, error);
		a->error = error;
		return (SCAN_ERROR);
	}

	/*
	 * We ignore label_offset; this seems to have not been used
	 * consistently in the old code, requiring us to do the search
	 * in the sector.
	 */
	lp = a->buf;
	lp_lim = (char *)a->buf + a->secsize - DISKLABEL_MINSIZE;
	for (;; lp = (void *)((char *)lp + sizeof(uint32_t))) {
		if ((char *)lp > (char *)lp_lim)
			return (SCAN_CONTINUE);
		label_offset = (size_t)((char *)lp - (char *)a->buf);
		if (lp->d_magic != DISKMAGIC || lp->d_magic2 != DISKMAGIC) {
			if (lp->d_magic != bswap32(DISKMAGIC) ||
			    lp->d_magic2 != bswap32(DISKMAGIC))
				continue;
			/* Label is in the other byte order. */
			swapped = 1;
		} else
			swapped = 0;

		npartitions = (swapped) ? bswap16(lp->d_npartitions)
					: lp->d_npartitions;

		/* Validate label length. */
		if ((char *)lp + DISKLABEL_SIZE(npartitions) >
		    (char *)a->buf + a->secsize) {
			aprint_error("%s: BSD disklabel @ "
			    "%" PRId64 "+%zd has bogus partition count (%u)\n",
			    a->pdk->dk_name, label_sector, label_offset,
			    npartitions);
			continue;
		}

		/*
		 * We have validated the partition count.  Checksum it.
		 * Note that we purposefully checksum before swapping
		 * the byte order.
		 */
		if (dkcksum_sized(lp, npartitions) != 0) {
			aprint_error("%s: BSD disklabel @ %" PRId64
			    "+%zd has bad checksum\n", a->pdk->dk_name,
			    label_sector, label_offset);
			continue;
		}
		/* Put the disklabel in the right order. */
		if (swapped)
			swap_disklabel(lp);
		addwedges(a, lp);
		return (SCAN_FOUND);
	}
}

static int
scan_mbr(mbr_args_t *a, int (*actn)(mbr_args_t *, struct mbr_partition *,
				    int, u_int))
{
	struct mbr_partition ptns[MBR_PART_COUNT];
	struct mbr_partition *dp;
	struct mbr_sector *mbr;
	u_int ext_base, this_ext, next_ext;
	int i, rval;
#ifdef COMPAT_386BSD_MBRPART
	int dp_386bsd = -1;
#endif

	ext_base = 0;
	this_ext = 0;
	for (;;) {
		a->error = dkwedge_read(a->pdk, a->vp, this_ext, a->buf,
					a->secsize);
		if (a->error) {
			aprint_error("%s: unable to read MBR @ %u, "
			    "error = %d\n", a->pdk->dk_name, this_ext,
			    a->error);
			return (SCAN_ERROR);
		}

		mbr = a->buf;
		if (mbr->mbr_magic != htole16(MBR_MAGIC))
			return (SCAN_CONTINUE);

		/* Copy data out of buffer so action can use the buffer. */
		memcpy(ptns, &mbr->mbr_parts, sizeof(ptns));

		/* Looks for NetBSD partition. */
		next_ext = 0;
		dp = ptns;
		for (i = 0; i < MBR_PART_COUNT; i++, dp++) {
			if (dp->mbrp_type == 0)
				continue;
			if (MBR_IS_EXTENDED(dp->mbrp_type)) {
				next_ext = le32toh(dp->mbrp_start);
				continue;
			}
#ifdef COMPAT_386BSD_MBRPART
			if (dp->mbrp_type == MBR_PTYPE_386BSD) {
				/*
				 * If more than one matches, take last,
				 * as NetBSD install tool does.
				 */
				if (this_ext == 0)
					dp_386bsd = i;
				continue;
			}
#endif
			rval = (*actn)(a, dp, i, this_ext);
			if (rval != SCAN_CONTINUE)
				return (rval);
		}
		if (next_ext == 0)
			break;
		if (ext_base == 0) {
			ext_base = next_ext;
			next_ext = 0;
		}
		next_ext += ext_base;
		if (next_ext <= this_ext)
			break;
		this_ext = next_ext;
	}
#ifdef COMPAT_386BSD_MBRPART
	if (this_ext == 0 && dp_386bsd != -1)
		return ((*actn)(a, &ptns[dp_386bsd], dp_386bsd, 0));
#endif
	return (SCAN_CONTINUE);
}

static int
look_netbsd_part(mbr_args_t *a, struct mbr_partition *dp, int slot,
    u_int ext_base)
{
	int ptn_base = ext_base + le32toh(dp->mbrp_start);
	int rval;

	if (
#ifdef COMPAT_386BSD_MBRPART
	    dp->mbrp_type == MBR_PTYPE_386BSD ||
#endif
	    dp->mbrp_type == MBR_PTYPE_NETBSD) {
		rval = validate_label(a, ptn_base + BSD44_MBR_LABELSECTOR, 0);

		/* If we got a NetBSD label, look no further. */
		if (rval == SCAN_FOUND)
			return (rval);
	}

	return (SCAN_CONTINUE);
}
 
#ifdef _KERNEL
#define	DKW_MALLOC(SZ)	malloc((SZ), M_DEVBUF, M_WAITOK)
#define	DKW_FREE(PTR)	free((PTR), M_DEVBUF)
#else
#define	DKW_MALLOC(SZ)	malloc((SZ))
#define	DKW_FREE(PTR)	free((PTR))
#endif

static int
dkwedge_discover_bsdlabel(struct disk *pdk, struct vnode *vp)
{
	mbr_args_t a;
	const struct disklabel_location *dl;
	int rval;

	a.pdk = pdk;
	a.secsize = DEV_BSIZE << pdk->dk_blkshift;
	a.vp = vp;
	a.buf = DKW_MALLOC(a.secsize);
	a.error = 0;

	/* MBR search. */
	rval = scan_mbr(&a, look_netbsd_part);
	if (rval != SCAN_CONTINUE) {
		if (rval == SCAN_FOUND)
			a.error = 0;	/* found it, wedges installed */
		goto out;
	}

	/* Known location search. */
	for (dl = disklabel_locations; dl->label_sector != -1; dl++) {
		rval = validate_label(&a, dl->label_sector, dl->label_offset);
		if (rval != SCAN_CONTINUE) {
			if (rval == SCAN_FOUND)
				a.error = 0;	/* found it, wedges installed */
			goto out;
		}
	}

	/* No NetBSD disklabel found. */
	a.error = ESRCH;
 out:
	DKW_FREE(a.buf);
	return (a.error);
}

#ifdef _KERNEL
DKWEDGE_DISCOVERY_METHOD_DECL(BSD44, 5, dkwedge_discover_bsdlabel);
#endif
