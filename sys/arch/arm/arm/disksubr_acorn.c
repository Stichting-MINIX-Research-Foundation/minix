/*	$NetBSD: disksubr_acorn.c,v 1.12 2013/03/09 16:02:25 christos Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: disksubr_acorn.c,v 1.12 2013/03/09 16:02:25 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/disklabel.h>

static int filecore_checksum(u_char *);

/*
 * static int filecore_checksum(u_char *bootblock)
 *
 * Calculates the filecore boot block checksum. This is used to validate
 * a filecore boot block on the disk.  If a boot block is validated then
 * it is used to locate the partition table. If the boot block is not
 * validated, it is assumed that the whole disk is NetBSD.
 *
 * The basic algorithm is:
 *
 *	for (each byte in block, excluding checksum) {
 *		sum += byte;
 *		if (sum > 255)
 *			sum -= 255;
 *	}
 *
 * That's equivalent to summing all of the bytes in the block
 * (excluding the checksum byte, of course), then calculating the
 * checksum as "cksum = sum - ((sum - 1) / 255) * 255)".  That
 * expression may or may not yield a faster checksum function,
 * but it's easier to reason about.
 *
 * Note that if you have a block filled with bytes of a single
 * value "X" (regardless of that value!) and calculate the cksum
 * of the block (excluding the checksum byte), you will _always_
 * end up with a checksum of X.  (Do the math; that can be derived
 * from the checksum calculation function!)  That means that
 * blocks which contain bytes which all have the same value will
 * always checksum properly.  That's a _very_ unlikely occurence
 * (probably impossible, actually) for a valid filecore boot block,
 * so we treat such blocks as invalid.
 */
static int
filecore_checksum(u_char *bootblock)
{  
	u_char byte0, accum_diff;
	u_int sum;
	int i;
 
	sum = 0;
	accum_diff = 0;
	byte0 = bootblock[0];
 
	/*
	 * Sum the contents of the block, keeping track of whether
	 * or not all bytes are the same.  If 'accum_diff' ends up
	 * being zero, all of the bytes are, in fact, the same.
	 */
	for (i = 0; i < 511; ++i) {
		sum += bootblock[i];
		accum_diff |= bootblock[i] ^ byte0;
	}

	/*
	 * Check to see if the checksum byte is the same as the
	 * rest of the bytes, too.  (Note that if all of the bytes
	 * are the same except the checksum, a checksum compare
	 * won't succeed, but that's not our problem.)
	 */
	accum_diff |= bootblock[i] ^ byte0;

	/* All bytes in block are the same; call it invalid. */
	if (accum_diff == 0)
		return (-1);

	return (sum - ((sum - 1) / 255) * 255);
}


int
filecore_label_read(dev_t dev, void (*strat)(struct buf *),
	struct disklabel *lp, struct cpu_disklabel *osdep,
	const char **msgp,
	int *cylp, int *netbsd_label_offp)
{
	struct filecore_bootblock *bb;
	int heads;
	int sectors;
	int rv = 1;
	int cyl, netbsdpartoff;
	struct buf *bp;

#ifdef __GNUC__
	netbsdpartoff = 0;		/* XXX -Wuninitialized */
#endif

	/* get a buffer and initialize it */
        bp = geteblk((int)lp->d_secsize);
        bp->b_dev = dev;

	/* read the Acorn filecore boot block */

	bp->b_blkno = FILECORE_BOOT_SECTOR;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags |= B_READ;
	bp->b_cylinder = bp->b_blkno / lp->d_secpercyl;
	(*strat)(bp);

	/*
	 * if successful, validate boot block and
	 * locate partition table
	 */

	if (biowait(bp)) {
		*msgp = "filecore boot block I/O error";
		goto out;
	}

	bb = (struct filecore_bootblock *)bp->b_data;

	/* Validate boot block */
       
	if (bb->checksum != filecore_checksum((u_char *)bb)) {
		/*
		 * Invalid boot block so lets assume the
		 *  entire disc is NetBSD
		 */
		rv = 0;
		goto out;
	}

	/* Get some information from the boot block */

	cyl = bb->partition_cyl_low + (bb->partition_cyl_high << 8);

	heads = bb->heads;
	sectors = bb->secspertrack;
                        
	/* Do we have a NETBSD partition table ? */

	if (bb->partition_type == PARTITION_FORMAT_RISCBSD) {
#ifdef DEBUG_LABEL
		printf("%s; heads = %d nsectors = %d\n",
		    __func__, heads, sectors);
#endif
		netbsdpartoff = cyl * heads * sectors;
	} else if (bb->partition_type == PARTITION_FORMAT_RISCIX) {
		struct riscix_partition_table *rpt;
		int loop;
		
		/*
		 * We have a RISCiX partition table :-( groan
		 * 
		 * Read the RISCiX partition table and see if
		 * there is a NetBSD partition
		 */

		bp->b_blkno = cyl * heads * sectors;
#ifdef DEBUG_LABEL
		printf("%s: Found RiscIX partition table @ %08x\n",
		    __func__, bp->b_blkno);
#endif
		bp->b_cylinder = bp->b_blkno / lp->d_secpercyl;
		bp->b_bcount = lp->d_secsize;
		bp->b_oflags &= ~(BO_DONE);
		bp->b_flags |= B_READ;
		(*strat)(bp);

		/*
		 * if successful, locate disk label within block
		 * and validate
		 */

		if (biowait(bp)) {
			*msgp = "disk label I/O error";
			goto out;
		}

		rpt = (struct riscix_partition_table *)bp->b_data;
#ifdef DEBUG_LABEL
		for (loop = 0; loop < NRISCIX_PARTITIONS; ++loop)
			printf("%s: p%d: %16s %08x %08x %08x\n", loop,
			    __func__, rpt->partitions[loop].rp_name,
			    rpt->partitions[loop].rp_start,
			    rpt->partitions[loop].rp_length,
			    rpt->partitions[loop].rp_type);
#endif
		for (loop = 0; loop < NRISCIX_PARTITIONS; ++loop) {
			if (strcmp(rpt->partitions[loop].rp_name,
			    "RiscBSD") == 0 ||
			    strcmp(rpt->partitions[loop].rp_name,
			    "NetBSD") == 0 ||
			    strcmp(rpt->partitions[loop].rp_name,
			    "Empty:") == 0) {
				netbsdpartoff =
				    rpt->partitions[loop].rp_start;
				break;
			}
		}
		if (loop == NRISCIX_PARTITIONS) {
			*msgp = "NetBSD partition identifier string not found.";
			goto out;
		}
	} else {
		*msgp = "Invalid partition format";
		goto out;
	}

	*cylp = cyl;
	*netbsd_label_offp = netbsdpartoff;
	*msgp = NULL;
out:
        brelse(bp, 0);
	return (rv);
}


/*
 * Return -1 not found, 0 found positive errno
 */
int
filecore_label_locate(dev_t dev,
	void (*strat)(struct buf *),
	struct disklabel *lp,
	struct cpu_disklabel *osdep,
	int *cylp, int *netbsd_label_offp)
{
	struct filecore_bootblock *bb;
	int heads;
	int sectors;
	int rv;
	int cyl, netbsdpartoff;
	struct buf *bp;

	/* get a buffer and initialize it */
        bp = geteblk((int)lp->d_secsize);
        bp->b_dev = dev;

	/* read the filecore boot block */

#ifdef DEBUG_LABEL
	printf("%s: Reading boot block\n", __func__);
#endif

	bp->b_blkno = FILECORE_BOOT_SECTOR;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags |= B_READ;
	bp->b_cylinder = bp->b_blkno / lp->d_secpercyl;
	(*strat)(bp);

	/*
	 * if successful, validate boot block and locate
	 * partition table
	 */

	if ((rv = biowait(bp)) != 0) {
		goto out;
	}

	bb = (struct filecore_bootblock *)bp->b_data;
	rv = 0;

	/* Validate boot block */
       
	if (bb->checksum != filecore_checksum((u_char *)bb)) {
		/*
		 * Invalid boot block so lets assume the
		 * entire disc is NetBSD
		 */
#ifdef DEBUG_LABEL
		printf("%s: Bad filecore boot block (incorrect checksum)\n",
		    __func__);
#endif
		rv = -1;
		goto out;
	}

	/* Do we have a NetBSD partition ? */

	if (bb->partition_type != PARTITION_FORMAT_RISCBSD) {
#ifdef DEBUG_LABEL
		printf("%s: Invalid partition format\n", __func__);
#endif
		rv = EINVAL;
		goto out;
	}

	cyl = bb->partition_cyl_low + (bb->partition_cyl_high << 8);

	heads = bb->heads;
	sectors = bb->secspertrack;

#ifdef DEBUG_LABEL
	printf("%s: heads = %d nsectors = %d\n", __func__, heads, sectors);
#endif

	netbsdpartoff = cyl * heads * sectors;

	*cylp = cyl;
	*netbsd_label_offp = netbsdpartoff;
out:
        brelse(bp, 0);
	return (rv);
}
