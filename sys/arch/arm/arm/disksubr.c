/*	$NetBSD: disksubr.c,v 1.25 2014/04/25 20:17:28 martin Exp $	*/

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
 *	from: @(#)ufs_disksubr.c	7.16 (Berkeley) 5/4/91
 */

/*
 * Copyright (c) 1995 Mark Brinicombe
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	from: @(#)ufs_disksubr.c	7.16 (Berkeley) 5/4/91
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: disksubr.c,v 1.25 2014/04/25 20:17:28 martin Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/dkbad.h>
#include <sys/disklabel.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/disk.h>

/*
 * Attempt to read a disk label from a device
 * using the indicated strategy routine.
 * The label must be partly set up before this:
 * secpercyl, secsize and anything required for a block i/o read
 * operation in the driver's strategy/start routines
 * must be filled in before calling us.
 *
 * If dos partition table requested, attempt to load it and
 * find disklabel inside a DOS partition. Also, if bad block
 * table needed, attempt to extract it as well. Return buffer
 * for use in signalling errors if requested.
 *
 * Returns null on success and an error string on failure.
 */

const char *
readdisklabel(dev_t dev, void (*strat)(struct buf *),
		struct disklabel *lp, struct cpu_disklabel *osdep)
{
	struct buf *bp;
	struct disklabel *dlp;
	const char *msg = NULL;
	int cyl, netbsdpartoff, i, found = 0;

	/* minimal requirements for archtypal disk label */

	if (lp->d_secsize == 0)
		lp->d_secsize = DEV_BSIZE;

	if (lp->d_secperunit == 0)
		lp->d_secperunit = 0x1fffffff;

	if (lp->d_npartitions < RAW_PART + 1)
	      lp->d_npartitions = RAW_PART + 1;

	if (lp->d_partitions[RAW_PART].p_size == 0) {
		lp->d_partitions[RAW_PART].p_fstype = FS_UNUSED; 
		lp->d_partitions[RAW_PART].p_offset = 0; 
		lp->d_partitions[RAW_PART].p_size = 0x1fffffff;
	}
	/*
	 * Set partition 'a' to be the whole disk.
	 * Cleared if we find a netbsd label.
	 */
	lp->d_partitions[0].p_size = lp->d_partitions[RAW_PART].p_size;
	lp->d_partitions[0].p_fstype = FS_BSDFFS;

	/* obtain buffer to probe drive with */
    
	bp = geteblk((int)lp->d_secsize);
	
	/* request no partition relocation by driver on I/O operations */

	bp->b_dev = dev;

	/* do netbsd partitions in the process of getting disklabel? */

	netbsdpartoff = 0;
	cyl = LABELSECTOR / lp->d_secpercyl;

	if (osdep) {
		if (filecore_label_read(dev, strat,lp, osdep, &msg, &cyl,
		      &netbsdpartoff) ||
		    mbr_label_read(dev, strat, lp, osdep, &msg, &cyl,
		      &netbsdpartoff)) {
			if (msg != NULL)
				goto done;
		} else {
			/*
			 * We didn't find anything we like; NetBSD native.
			 * netbsdpartoff and cyl should be unchanged.
			 */
			KASSERT(netbsdpartoff == 0);
			KASSERT(cyl == (LABELSECTOR / lp->d_secpercyl));
		}
	}

	/* next, dig out disk label */

	bp->b_blkno = netbsdpartoff + LABELSECTOR;
	bp->b_cylinder = bp->b_blkno / lp->d_secpercyl;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags |= B_READ;
	(*strat)(bp);

	/* if successful, locate disk label within block and validate */

	if (biowait(bp)) {
		msg = "disk label I/O error";
		goto done;
	}
	for (dlp = (struct disklabel *)bp->b_data;
	    dlp <= (struct disklabel *)((char *)bp->b_data + lp->d_secsize
		- sizeof(*dlp));
	    dlp = (struct disklabel *)((char *)dlp + sizeof(long))) {
		if (dlp->d_magic != DISKMAGIC || dlp->d_magic2 != DISKMAGIC) {
			continue;
		} else if (dlp->d_npartitions > MAXPARTITIONS ||
			   dkcksum(dlp) != 0)
			msg = "disk label corrupted";
		else {
			*lp = *dlp;
			msg = NULL;
			found = 1;
			break;
		}
	}

	if (msg != NULL || found == 0)
		goto done;

	/* obtain bad sector table if requested and present */
	if (osdep && (lp->d_flags & D_BADSECT)) {
		struct dkbad *bdp = &osdep->bad;
		struct dkbad *db;

		i = 0;
		do {
			/* read a bad sector table */
			bp->b_oflags &= ~(BO_DONE);
			bp->b_flags |= B_READ;
			bp->b_blkno = lp->d_secperunit - lp->d_nsectors + i;
			if (lp->d_secsize > DEV_BSIZE)
				bp->b_blkno *= lp->d_secsize / DEV_BSIZE;
			else
				bp->b_blkno /= DEV_BSIZE / lp->d_secsize;
			bp->b_bcount = lp->d_secsize;
			bp->b_cylinder = lp->d_ncylinders - 1;
			(*strat)(bp);

			/* if successful, validate, otherwise try another */
			if (biowait(bp)) {
				msg = "bad sector table I/O error";
			} else {
				db = (struct dkbad *)(bp->b_data);
#define DKBAD_MAGIC 0x4321
				if (db->bt_mbz == 0
					&& db->bt_flag == DKBAD_MAGIC) {
					msg = NULL;
					*bdp = *db;
					break;
				} else
					msg = "bad sector table corrupted";
			}
		} while (bp->b_error != 0 && (i += 2) < 10 &&
			i < lp->d_nsectors);
	}

done:
	brelse(bp, 0);
	return (msg);
}


/*
 * Check new disk label for sensibility
 * before setting it.
 */

int
setdisklabel(struct disklabel *olp, struct disklabel *nlp, u_long openmask,
    struct cpu_disklabel *osdep)
{
	int i;
	struct partition *opp, *npp;

	/* sanity clause */

	if (nlp->d_secpercyl == 0 || nlp->d_secsize == 0
	    || (nlp->d_secsize % DEV_BSIZE) != 0)
		return(EINVAL);

	/* special case to allow disklabel to be invalidated */

	if (nlp->d_magic == 0xffffffff) {
		*olp = *nlp;
		return (0);
	}

	if (nlp->d_magic != DISKMAGIC || nlp->d_magic2 != DISKMAGIC
	    || dkcksum(nlp) != 0)
		return (EINVAL);

	/* XXX add check if other acorn/dos partitions will be overwritten */

	while (openmask != 0) {
		i = ffs(openmask) - 1;
		openmask &= ~(1 << i);
		if (nlp->d_npartitions <= i)
			return (EBUSY);
		opp = &olp->d_partitions[i];
		npp = &nlp->d_partitions[i];
		if (npp->p_offset != opp->p_offset || npp->p_size < opp->p_size)
			return (EBUSY);
		/*
		 * Copy internally-set partition information
		 * if new label doesn't include it.		XXX
		 */
		if (npp->p_fstype == FS_UNUSED && opp->p_fstype != FS_UNUSED) {
			npp->p_fstype = opp->p_fstype;
			npp->p_fsize = opp->p_fsize;
			npp->p_frag = opp->p_frag;
			npp->p_cpg = opp->p_cpg;
		}
	}

	nlp->d_checksum = 0;
	nlp->d_checksum = dkcksum(nlp);
	*olp = *nlp;
	return (0);
}


/*
 * Write disk label back to device after modification.
 */
 
int
writedisklabel(dev_t dev, void (*strat)(struct buf *),
	struct disklabel *lp, struct cpu_disklabel *osdep)
{
	struct buf *bp;
	struct disklabel *dlp;
	int cyl, netbsdpartoff;
	int error = 0, rv;

	/* get a buffer and initialize it */

	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;

	/* do netbsd partitions in the process of getting disklabel? */

	netbsdpartoff = 0;
	cyl = LABELSECTOR / lp->d_secpercyl;

	if (osdep) {
		if ((rv = filecore_label_locate(dev, strat,lp, osdep, &cyl,
		      &netbsdpartoff)) != 0 ||
		    (rv = mbr_label_locate(dev, strat, lp, osdep, &cyl,
		      &netbsdpartoff)) != 0) {
			if (rv > 0)
			    goto done;
		} else {
			/*
			 * We didn't find anything we like; NetBSD native.
			 * netbsdpartoff and cyl should be unchanged.
			 */
			KASSERT(netbsdpartoff == 0);
			KASSERT(cyl == (LABELSECTOR / lp->d_secpercyl));
		} 
	}

	/* writelabel: */

#ifdef DEBUG_LABEL
	printf("%s: Reading disklabel addr=%08x\n", __func__,
	     netbsdpartoff * DEV_BSIZE);
#endif

	/* next, dig out disk label */

	bp->b_blkno = netbsdpartoff + LABELSECTOR;
	bp->b_cylinder = cyl;
	bp->b_bcount = lp->d_secsize;
	bp->b_oflags &= ~(BO_DONE);
	bp->b_flags |= B_READ;
	(*strat)(bp);

	/* if successful, locate disk label within block and validate */

	if ((error = biowait(bp)))
		goto done;
	for (dlp = (struct disklabel *)bp->b_data;
	    dlp <= (struct disklabel *)((char *)bp->b_data + lp->d_secsize
		- sizeof(*dlp));
	    dlp = (struct disklabel *)((char *)dlp + sizeof(long))) {
		if (dlp->d_magic == DISKMAGIC && dlp->d_magic2 == DISKMAGIC &&
		    dkcksum(dlp) == 0) {
			*dlp = *lp;
			bp->b_flags &= ~(B_READ);
			bp->b_oflags &= ~(BO_DONE);
			bp->b_flags |= B_WRITE;
			(*strat)(bp);
			error = biowait(bp);
			goto done;
		}
	}

	error = ESRCH;

done:
	brelse(bp, 0);
	return (error);
}

/* End of disksubr.c */
