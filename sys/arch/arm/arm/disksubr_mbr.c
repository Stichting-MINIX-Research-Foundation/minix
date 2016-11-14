/*	$NetBSD: disksubr_mbr.c,v 1.18 2013/08/13 00:04:08 matt Exp $	*/

/*
 * Copyright (c) 1998 Christopher G. Demetriou.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

/*
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
 *	@(#)ufs_disksubr.c	7.16 (Berkeley) 5/4/91
 */

/*
 * From i386 disklabel.c rev 1.29, with cleanups and modifications to
 * make it easier to use on the arm32 and to use as MI code (not quite
 * clean enough, yet).
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: disksubr_mbr.c,v 1.18 2013/08/13 00:04:08 matt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/disklabel.h>

#include "opt_mbr.h"

#define MBRSIGOFS 0x1fe
static char mbrsig[2] = {0x55, 0xaa};

int
mbr_label_read(dev_t dev,
	void (*strat)(struct buf *),
	struct disklabel *lp,
	struct cpu_disklabel *osdep,
	const char **msgp,
	int *cylp, int *netbsd_label_offp)
{
	struct mbr_partition *mbrp;
	struct partition *pp;
	int cyl, mbrpartoff, i;
	struct buf *bp;
	int rv = 1;

	/* get a buffer and initialize it */
        bp = geteblk((int)lp->d_secsize);
        bp->b_dev = dev;

	/* In case nothing sets them */
	mbrpartoff = 0;
	cyl = LABELSECTOR / lp->d_secpercyl;

	mbrp = osdep->mbrparts;

	/* read master boot record */
	bp->b_blkno = MBR_BBSECTOR;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags |= B_READ;
	bp->b_cylinder = MBR_BBSECTOR / lp->d_secpercyl;
	(*strat)(bp);

	/* if successful, wander through dos partition table */
	if (biowait(bp)) {
		*msgp = "dos partition I/O error";
		goto out;
	} else {
		struct mbr_partition *ourmbrp = NULL;
		int nfound = 0;

		/* XXX "there has to be a better check than this." */
		if (memcmp((char *)bp->b_data + MBRSIGOFS, mbrsig,
		    sizeof(mbrsig))) {
			rv = 0;
			goto out;
		}

		/* XXX how do we check veracity/bounds of this? */
		memcpy(mbrp, (char *)bp->b_data + MBR_PART_OFFSET,
		      MBR_PART_COUNT * sizeof(*mbrp));

		/* look for NetBSD partition */
		ourmbrp = NULL;
		for (i = 0; !ourmbrp && i < MBR_PART_COUNT; i++) {
			if (mbrp[i].mbrp_type == MBR_PTYPE_NETBSD)
				ourmbrp = &mbrp[i];
		}
#ifdef COMPAT_386BSD_MBRPART
		/* didn't find it -- look for 386BSD partition */
		for (i = 0; !ourmbrp && i < MBR_PART_COUNT; i++) {
			if (mbrp[i].mbrp_type == MBR_PTYPE_386BSD) {
				printf("WARNING: old BSD partition ID!\n");
				ourmbrp = &mbrp[i];
				break;
			}
		}
#endif
		pp = &lp->d_partitions['e' - 'a'];
		for (i = 0; i < MBR_PART_COUNT; i++, mbrp++, pp++) {
			if ((i == 0 && mbrp->mbrp_type == MBR_PTYPE_PMBR)
			    || mbrp->mbrp_type == MBR_PTYPE_UNUSED) {
				memset(pp, 0, sizeof(*pp));
				continue;
			}
			if (le32toh(mbrp->mbrp_start) +
			    le32toh(mbrp->mbrp_size) > lp->d_secperunit) {
				/* This mbr doesn't look good.... */
				memset(pp, 0, sizeof(*pp));
				continue;
			}
			nfound++;

			/* Install in partition e, f, g, or h. */
			pp->p_offset = le32toh(mbrp->mbrp_start);
			pp->p_size = le32toh(mbrp->mbrp_size);
			pp->p_fstype = xlat_mbr_fstype(mbrp->mbrp_type);

			/* is this ours? */
			if (mbrp != ourmbrp)
				continue;

			/* need sector address for SCSI/IDE,
			   cylinder for ESDI/ST506/RLL */
			mbrpartoff = le32toh(mbrp->mbrp_start);
			cyl = MBR_PCYL(mbrp->mbrp_scyl, mbrp->mbrp_ssect);

#ifdef __i386__ /* XXX? */
			/* update disklabel with details */
			lp->d_partitions[2].p_size = le32toh(mbrp->mbrp_size);
			lp->d_partitions[2].p_offset = 
			    le32toh(mbrp->mbrp_start);
			lp->d_ntracks = mbrp->mbrp_ehd + 1;
			lp->d_nsectors = MBR_PSECT(mbrp->mbrp_esect);
			lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;
#endif
		}
		i += 'e' - 'a';
		if (nfound > 0) {
			lp->d_npartitions = i;
			strncpy(lp->d_packname, "fictitious-MBR",
			    sizeof lp->d_packname);
		}
		if (lp->d_npartitions < MAXPARTITIONS) {
			memset(pp, 0, (MAXPARTITIONS - i) * sizeof(*pp));
		}
	}

	*cylp = cyl;
	*netbsd_label_offp = mbrpartoff;
	*msgp = NULL;
out:
        brelse(bp, 0);
	return (rv);
}

/*
 * Return -1 not found, 0 found positive errno
 */
int
mbr_label_locate(dev_t dev,
	void (*strat)(struct buf *),
	struct disklabel *lp,
	struct cpu_disklabel *osdep,
	int *cylp, int *netbsd_label_offp)
{
	struct mbr_partition *mbrp;
	int cyl, mbrpartoff, i;
	struct mbr_partition *ourmbrp = NULL;
	struct buf *bp;
	int rv;

	/* get a buffer and initialize it */
        bp = geteblk((int)lp->d_secsize);
        bp->b_dev = dev;

	/* do MBR partitions in the process of getting disklabel? */
	mbrpartoff = 0;
	cyl = LABELSECTOR / lp->d_secpercyl;

	mbrp = osdep->mbrparts;

	/* read master boot record */
	bp->b_blkno = MBR_BBSECTOR;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags |= B_READ;
	bp->b_cylinder = MBR_BBSECTOR / lp->d_secpercyl;
	(*strat)(bp);

	if ((rv = biowait(bp)) != 0) {
		goto out;
	}

	if (memcmp((char *)bp->b_data + MBRSIGOFS, mbrsig, sizeof(mbrsig))) {
		rv = 0;
		goto out;
	}

	/* XXX how do we check veracity/bounds of this? */
	memcpy(mbrp, (char *)bp->b_data + MBR_PART_OFFSET,
	    MBR_PART_COUNT * sizeof(*mbrp));

	/* look for NetBSD partition */
	ourmbrp = NULL;
	for (i = 0; !ourmbrp && i < MBR_PART_COUNT; i++) {
		if (mbrp[i].mbrp_type == MBR_PTYPE_NETBSD)
			ourmbrp = &mbrp[i];
	}
#ifdef COMPAT_386BSD_MBRPART
	/* didn't find it -- look for 386BSD partition */
	for (i = 0; !ourmbrp && i < MBR_PART_COUNT; i++) {
		if (mbrp[i].mbrp_type == MBR_PTYPE_386BSD) {
			printf("WARNING: old BSD partition ID!\n");
			ourmbrp = &mbrp[i];
		}
	}
#endif
	if (!ourmbrp) {
		rv = 0;			/* XXX allow easy clobber? */
		goto out;
	}

	/* need sector address for SCSI/IDE, cylinder for ESDI/ST506/RLL */
	mbrpartoff = le32toh(ourmbrp->mbrp_start);
	cyl = MBR_PCYL(ourmbrp->mbrp_scyl, ourmbrp->mbrp_ssect);

	*cylp = cyl;
	*netbsd_label_offp = mbrpartoff;
	rv = -1;
out:
        brelse(bp, 0);
	return (rv);
}
