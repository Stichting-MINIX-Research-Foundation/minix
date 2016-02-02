/*	$NetBSD: ffs_alloc.c,v 1.28 2015/03/29 05:52:59 agc Exp $	*/
/* From: NetBSD: ffs_alloc.c,v 1.50 2001/09/06 02:16:01 lukem Exp */

/*
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Marshall
 * Kirk McKusick and Network Associates Laboratories, the Security
 * Research Division of Network Associates, Inc. under DARPA/SPAWAR
 * contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA CHATS
 * research program
 *
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)ffs_alloc.c	8.19 (Berkeley) 7/13/95
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#if defined(__RCSID) && !defined(__lint)
__RCSID("$NetBSD: ffs_alloc.c,v 1.28 2015/03/29 05:52:59 agc Exp $");
#endif	/* !__lint */

#include <sys/param.h>
#include <sys/time.h>

#include <errno.h>

#include "makefs.h"

#include <ufs/ufs/dinode.h>
#include <ufs/ufs/ufs_bswap.h>
#include <ufs/ffs/fs.h>

#include "ffs/buf.h"
#include "ffs/ufs_inode.h"
#include "ffs/ffs_extern.h"


static int scanc(u_int, const u_char *, const u_char *, int);

static daddr_t ffs_alloccg(struct inode *, int, daddr_t, int);
static daddr_t ffs_alloccgblk(struct inode *, struct buf *, daddr_t);
static daddr_t ffs_hashalloc(struct inode *, int, daddr_t, int,
		     daddr_t (*)(struct inode *, int, daddr_t, int));
static int32_t ffs_mapsearch(struct fs *, struct cg *, daddr_t, int);

/* in ffs_tables.c */
extern const int inside[], around[];
extern const u_char * const fragtbl[];

/*
 * Allocate a block in the file system.
 * 
 * The size of the requested block is given, which must be some
 * multiple of fs_fsize and <= fs_bsize.
 * A preference may be optionally specified. If a preference is given
 * the following hierarchy is used to allocate a block:
 *   1) allocate the requested block.
 *   2) allocate a rotationally optimal block in the same cylinder.
 *   3) allocate a block in the same cylinder group.
 *   4) quadradically rehash into other cylinder groups, until an
 *      available block is located.
 * If no block preference is given the following hierarchy is used
 * to allocate a block:
 *   1) allocate a block in the cylinder group that contains the
 *      inode for the file.
 *   2) quadradically rehash into other cylinder groups, until an
 *      available block is located.
 */
int
ffs_alloc(struct inode *ip, daddr_t lbn __unused, daddr_t bpref, int size,
    daddr_t *bnp)
{
	struct fs *fs = ip->i_fs;
	daddr_t bno;
	int cg;
	
	*bnp = 0;
	if (size > fs->fs_bsize || ffs_fragoff(fs, size) != 0) {
		errx(1, "ffs_alloc: bad size: bsize %d size %d",
		    fs->fs_bsize, size);
	}
	if (size == fs->fs_bsize && fs->fs_cstotal.cs_nbfree == 0)
		goto nospace;
	if (bpref >= fs->fs_size)
		bpref = 0;
	if (bpref == 0)
		cg = ino_to_cg(fs, ip->i_number);
	else
		cg = dtog(fs, bpref);
	bno = ffs_hashalloc(ip, cg, bpref, size, ffs_alloccg);
	if (bno > 0) {
		DIP_ADD(ip, blocks, size / DEV_BSIZE);
		*bnp = bno;
		return (0);
	}
nospace:
	return (ENOSPC);
}

/*
 * Select the desired position for the next block in a file.  The file is
 * logically divided into sections. The first section is composed of the
 * direct blocks. Each additional section contains fs_maxbpg blocks.
 * 
 * If no blocks have been allocated in the first section, the policy is to
 * request a block in the same cylinder group as the inode that describes
 * the file. If no blocks have been allocated in any other section, the
 * policy is to place the section in a cylinder group with a greater than
 * average number of free blocks.  An appropriate cylinder group is found
 * by using a rotor that sweeps the cylinder groups. When a new group of
 * blocks is needed, the sweep begins in the cylinder group following the
 * cylinder group from which the previous allocation was made. The sweep
 * continues until a cylinder group with greater than the average number
 * of free blocks is found. If the allocation is for the first block in an
 * indirect block, the information on the previous allocation is unavailable;
 * here a best guess is made based upon the logical block number being
 * allocated.
 * 
 * If a section is already partially allocated, the policy is to
 * contiguously allocate fs_maxcontig blocks.  The end of one of these
 * contiguous blocks and the beginning of the next is physically separated
 * so that the disk head will be in transit between them for at least
 * fs_rotdelay milliseconds.  This is to allow time for the processor to
 * schedule another I/O transfer.
 */
/* XXX ondisk32 */
daddr_t
ffs_blkpref_ufs1(struct inode *ip, daddr_t lbn, int indx, int32_t *bap)
{
	struct fs *fs;
	int cg;
	int avgbfree, startcg;

	fs = ip->i_fs;
	if (indx % fs->fs_maxbpg == 0 || bap[indx - 1] == 0) {
		if (lbn < UFS_NDADDR + FFS_NINDIR(fs)) {
			cg = ino_to_cg(fs, ip->i_number);
			return (fs->fs_fpg * cg + fs->fs_frag);
		}
		/*
		 * Find a cylinder with greater than average number of
		 * unused data blocks.
		 */
		if (indx == 0 || bap[indx - 1] == 0)
			startcg =
			    ino_to_cg(fs, ip->i_number) + lbn / fs->fs_maxbpg;
		else
			startcg = dtog(fs,
				ufs_rw32(bap[indx - 1], UFS_FSNEEDSWAP(fs)) + 1);
		startcg %= fs->fs_ncg;
		avgbfree = fs->fs_cstotal.cs_nbfree / fs->fs_ncg;
		for (cg = startcg; cg < fs->fs_ncg; cg++)
			if (fs->fs_cs(fs, cg).cs_nbfree >= avgbfree)
				return (fs->fs_fpg * cg + fs->fs_frag);
		for (cg = 0; cg <= startcg; cg++)
			if (fs->fs_cs(fs, cg).cs_nbfree >= avgbfree)
				return (fs->fs_fpg * cg + fs->fs_frag);
		return (0);
	}
	/*
	 * We just always try to lay things out contiguously.
	 */
	return ufs_rw32(bap[indx - 1], UFS_FSNEEDSWAP(fs)) + fs->fs_frag;
}

daddr_t
ffs_blkpref_ufs2(struct inode *ip, daddr_t lbn, int indx, int64_t *bap)
{
	struct fs *fs;
	int cg;
	int avgbfree, startcg;

	fs = ip->i_fs;
	if (indx % fs->fs_maxbpg == 0 || bap[indx - 1] == 0) {
		if (lbn < UFS_NDADDR + FFS_NINDIR(fs)) {
			cg = ino_to_cg(fs, ip->i_number);
			return (fs->fs_fpg * cg + fs->fs_frag);
		}
		/*
		 * Find a cylinder with greater than average number of
		 * unused data blocks.
		 */
		if (indx == 0 || bap[indx - 1] == 0)
			startcg =
			    ino_to_cg(fs, ip->i_number) + lbn / fs->fs_maxbpg;
		else
			startcg = dtog(fs,
				ufs_rw64(bap[indx - 1], UFS_FSNEEDSWAP(fs)) + 1);
		startcg %= fs->fs_ncg;
		avgbfree = fs->fs_cstotal.cs_nbfree / fs->fs_ncg;
		for (cg = startcg; cg < fs->fs_ncg; cg++)
			if (fs->fs_cs(fs, cg).cs_nbfree >= avgbfree) {
				return (fs->fs_fpg * cg + fs->fs_frag);
			}
		for (cg = 0; cg < startcg; cg++)
			if (fs->fs_cs(fs, cg).cs_nbfree >= avgbfree) {
				return (fs->fs_fpg * cg + fs->fs_frag);
			}
		return (0);
	}
	/*
	 * We just always try to lay things out contiguously.
	 */
	return ufs_rw64(bap[indx - 1], UFS_FSNEEDSWAP(fs)) + fs->fs_frag;
}

/*
 * Implement the cylinder overflow algorithm.
 *
 * The policy implemented by this algorithm is:
 *   1) allocate the block in its requested cylinder group.
 *   2) quadradically rehash on the cylinder group number.
 *   3) brute force search for a free block.
 *
 * `size':	size for data blocks, mode for inodes
 */
/*VARARGS5*/
static daddr_t
ffs_hashalloc(struct inode *ip, int cg, daddr_t pref, int size,
    daddr_t (*allocator)(struct inode *, int, daddr_t, int))
{
	struct fs *fs;
	daddr_t result;
	int i, icg = cg;

	fs = ip->i_fs;
	/*
	 * 1: preferred cylinder group
	 */
	result = (*allocator)(ip, cg, pref, size);
	if (result)
		return (result);
	/*
	 * 2: quadratic rehash
	 */
	for (i = 1; i < fs->fs_ncg; i *= 2) {
		cg += i;
		if (cg >= fs->fs_ncg)
			cg -= fs->fs_ncg;
		result = (*allocator)(ip, cg, 0, size);
		if (result)
			return (result);
	}
	/*
	 * 3: brute force search
	 * Note that we start at i == 2, since 0 was checked initially,
	 * and 1 is always checked in the quadratic rehash.
	 */
	cg = (icg + 2) % fs->fs_ncg;
	for (i = 2; i < fs->fs_ncg; i++) {
		result = (*allocator)(ip, cg, 0, size);
		if (result)
			return (result);
		cg++;
		if (cg == fs->fs_ncg)
			cg = 0;
	}
	return (0);
}

/*
 * Determine whether a block can be allocated.
 *
 * Check to see if a block of the appropriate size is available,
 * and if it is, allocate it.
 */
static daddr_t
ffs_alloccg(struct inode *ip, int cg, daddr_t bpref, int size)
{
	struct cg *cgp;
	struct buf *bp;
	daddr_t bno, blkno;
	int error, frags, allocsiz, i;
	struct fs *fs = ip->i_fs;
	const int needswap = UFS_FSNEEDSWAP(fs);

	if (fs->fs_cs(fs, cg).cs_nbfree == 0 && size == fs->fs_bsize)
		return (0);
	error = bread(ip->i_devvp, FFS_FSBTODB(fs, cgtod(fs, cg)),
	    (int)fs->fs_cgsize, 0, &bp);
	if (error) {
		return (0);
	}
	cgp = (struct cg *)bp->b_data;
	if (!cg_chkmagic(cgp, needswap) ||
	    (cgp->cg_cs.cs_nbfree == 0 && size == fs->fs_bsize)) {
		brelse(bp, 0);
		return (0);
	}
	if (size == fs->fs_bsize) {
		bno = ffs_alloccgblk(ip, bp, bpref);
		bwrite(bp);
		return (bno);
	}
	/*
	 * check to see if any fragments are already available
	 * allocsiz is the size which will be allocated, hacking
	 * it down to a smaller size if necessary
	 */
	frags = ffs_numfrags(fs, size);
	for (allocsiz = frags; allocsiz < fs->fs_frag; allocsiz++)
		if (cgp->cg_frsum[allocsiz] != 0)
			break;
	if (allocsiz == fs->fs_frag) {
		/*
		 * no fragments were available, so a block will be 
		 * allocated, and hacked up
		 */
		if (cgp->cg_cs.cs_nbfree == 0) {
			brelse(bp, 0);
			return (0);
		}
		bno = ffs_alloccgblk(ip, bp, bpref);
		bpref = dtogd(fs, bno);
		for (i = frags; i < fs->fs_frag; i++)
			setbit(cg_blksfree(cgp, needswap), bpref + i);
		i = fs->fs_frag - frags;
		ufs_add32(cgp->cg_cs.cs_nffree, i, needswap);
		fs->fs_cstotal.cs_nffree += i;
		fs->fs_cs(fs, cg).cs_nffree += i;
		fs->fs_fmod = 1;
		ufs_add32(cgp->cg_frsum[i], 1, needswap);
		bdwrite(bp);
		return (bno);
	}
	bno = ffs_mapsearch(fs, cgp, bpref, allocsiz);
	for (i = 0; i < frags; i++)
		clrbit(cg_blksfree(cgp, needswap), bno + i);
	ufs_add32(cgp->cg_cs.cs_nffree, -frags, needswap);
	fs->fs_cstotal.cs_nffree -= frags;
	fs->fs_cs(fs, cg).cs_nffree -= frags;
	fs->fs_fmod = 1;
	ufs_add32(cgp->cg_frsum[allocsiz], -1, needswap);
	if (frags != allocsiz)
		ufs_add32(cgp->cg_frsum[allocsiz - frags], 1, needswap);
	blkno = cg * fs->fs_fpg + bno;
	bdwrite(bp);
	return blkno;
}

/*
 * Allocate a block in a cylinder group.
 *
 * This algorithm implements the following policy:
 *   1) allocate the requested block.
 *   2) allocate a rotationally optimal block in the same cylinder.
 *   3) allocate the next available block on the block rotor for the
 *      specified cylinder group.
 * Note that this routine only allocates fs_bsize blocks; these
 * blocks may be fragmented by the routine that allocates them.
 */
static daddr_t
ffs_alloccgblk(struct inode *ip, struct buf *bp, daddr_t bpref)
{
	struct cg *cgp;
	daddr_t blkno;
	int32_t bno;
	struct fs *fs = ip->i_fs;
	const int needswap = UFS_FSNEEDSWAP(fs);
	u_int8_t *blksfree;

	cgp = (struct cg *)bp->b_data;
	blksfree = cg_blksfree(cgp, needswap);
	if (bpref == 0 || dtog(fs, bpref) != ufs_rw32(cgp->cg_cgx, needswap)) {
		bpref = ufs_rw32(cgp->cg_rotor, needswap);
	} else {
		bpref = ffs_blknum(fs, bpref);
		bno = dtogd(fs, bpref);
		/*
		 * if the requested block is available, use it
		 */
		if (ffs_isblock(fs, blksfree, ffs_fragstoblks(fs, bno)))
			goto gotit;
	}
	/*
	 * Take the next available one in this cylinder group.
	 */
	bno = ffs_mapsearch(fs, cgp, bpref, (int)fs->fs_frag);
	if (bno < 0)
		return (0);
	cgp->cg_rotor = ufs_rw32(bno, needswap);
gotit:
	blkno = ffs_fragstoblks(fs, bno);
	ffs_clrblock(fs, blksfree, (long)blkno);
	ffs_clusteracct(fs, cgp, blkno, -1);
	ufs_add32(cgp->cg_cs.cs_nbfree, -1, needswap);
	fs->fs_cstotal.cs_nbfree--;
	fs->fs_cs(fs, ufs_rw32(cgp->cg_cgx, needswap)).cs_nbfree--;
	fs->fs_fmod = 1;
	blkno = ufs_rw32(cgp->cg_cgx, needswap) * fs->fs_fpg + bno;
	return (blkno);
}

/*
 * Free a block or fragment.
 *
 * The specified block or fragment is placed back in the
 * free map. If a fragment is deallocated, a possible 
 * block reassembly is checked.
 */
void
ffs_blkfree(struct inode *ip, daddr_t bno, long size)
{
	struct cg *cgp;
	struct buf *bp;
	int32_t fragno, cgbno;
	int i, error, cg, blk, frags, bbase;
	struct fs *fs = ip->i_fs;
	const int needswap = UFS_FSNEEDSWAP(fs);

	if (size > fs->fs_bsize || ffs_fragoff(fs, size) != 0 ||
	    ffs_fragnum(fs, bno) + ffs_numfrags(fs, size) > fs->fs_frag) {
		errx(1, "blkfree: bad size: bno %lld bsize %d size %ld",
		    (long long)bno, fs->fs_bsize, size);
	}
	cg = dtog(fs, bno);
	if (bno >= fs->fs_size) {
		warnx("bad block %lld, ino %llu", (long long)bno,
		    (unsigned long long)ip->i_number);
		return;
	}
	error = bread(ip->i_devvp, FFS_FSBTODB(fs, cgtod(fs, cg)),
	    (int)fs->fs_cgsize, 0, &bp);
	if (error) {
		brelse(bp, 0);
		return;
	}
	cgp = (struct cg *)bp->b_data;
	if (!cg_chkmagic(cgp, needswap)) {
		brelse(bp, 0);
		return;
	}
	cgbno = dtogd(fs, bno);
	if (size == fs->fs_bsize) {
		fragno = ffs_fragstoblks(fs, cgbno);
		if (!ffs_isfreeblock(fs, cg_blksfree(cgp, needswap), fragno)) {
			errx(1, "blkfree: freeing free block %lld",
			    (long long)bno);
		}
		ffs_setblock(fs, cg_blksfree(cgp, needswap), fragno);
		ffs_clusteracct(fs, cgp, fragno, 1);
		ufs_add32(cgp->cg_cs.cs_nbfree, 1, needswap);
		fs->fs_cstotal.cs_nbfree++;
		fs->fs_cs(fs, cg).cs_nbfree++;
	} else {
		bbase = cgbno - ffs_fragnum(fs, cgbno);
		/*
		 * decrement the counts associated with the old frags
		 */
		blk = blkmap(fs, cg_blksfree(cgp, needswap), bbase);
		ffs_fragacct(fs, blk, cgp->cg_frsum, -1, needswap);
		/*
		 * deallocate the fragment
		 */
		frags = ffs_numfrags(fs, size);
		for (i = 0; i < frags; i++) {
			if (isset(cg_blksfree(cgp, needswap), cgbno + i)) {
				errx(1, "blkfree: freeing free frag: block %lld",
				    (long long)(cgbno + i));
			}
			setbit(cg_blksfree(cgp, needswap), cgbno + i);
		}
		ufs_add32(cgp->cg_cs.cs_nffree, i, needswap);
		fs->fs_cstotal.cs_nffree += i;
		fs->fs_cs(fs, cg).cs_nffree += i;
		/*
		 * add back in counts associated with the new frags
		 */
		blk = blkmap(fs, cg_blksfree(cgp, needswap), bbase);
		ffs_fragacct(fs, blk, cgp->cg_frsum, 1, needswap);
		/*
		 * if a complete block has been reassembled, account for it
		 */
		fragno = ffs_fragstoblks(fs, bbase);
		if (ffs_isblock(fs, cg_blksfree(cgp, needswap), fragno)) {
			ufs_add32(cgp->cg_cs.cs_nffree, -fs->fs_frag, needswap);
			fs->fs_cstotal.cs_nffree -= fs->fs_frag;
			fs->fs_cs(fs, cg).cs_nffree -= fs->fs_frag;
			ffs_clusteracct(fs, cgp, fragno, 1);
			ufs_add32(cgp->cg_cs.cs_nbfree, 1, needswap);
			fs->fs_cstotal.cs_nbfree++;
			fs->fs_cs(fs, cg).cs_nbfree++;
		}
	}
	fs->fs_fmod = 1;
	bdwrite(bp);
}


static int
scanc(u_int size, const u_char *cp, const u_char table[], int mask)
{
	const u_char *end = &cp[size];

	while (cp < end && (table[*cp] & mask) == 0)
		cp++;
	return (end - cp);
}

/*
 * Find a block of the specified size in the specified cylinder group.
 *
 * It is a panic if a request is made to find a block if none are
 * available.
 */
static int32_t
ffs_mapsearch(struct fs *fs, struct cg *cgp, daddr_t bpref, int allocsiz)
{
	int32_t bno;
	int start, len, loc, i;
	int blk, field, subfield, pos;
	int ostart, olen;
	const int needswap = UFS_FSNEEDSWAP(fs);

	/*
	 * find the fragment by searching through the free block
	 * map for an appropriate bit pattern
	 */
	if (bpref)
		start = dtogd(fs, bpref) / NBBY;
	else
		start = ufs_rw32(cgp->cg_frotor, needswap) / NBBY;
	len = howmany(fs->fs_fpg, NBBY) - start;
	ostart = start;
	olen = len;
	loc = scanc((u_int)len,
		(const u_char *)&cg_blksfree(cgp, needswap)[start],
		(const u_char *)fragtbl[fs->fs_frag],
		(1 << (allocsiz - 1 + (fs->fs_frag % NBBY))));
	if (loc == 0) {
		len = start + 1;
		start = 0;
		loc = scanc((u_int)len,
			(const u_char *)&cg_blksfree(cgp, needswap)[0],
			(const u_char *)fragtbl[fs->fs_frag],
			(1 << (allocsiz - 1 + (fs->fs_frag % NBBY))));
		if (loc == 0) {
			errx(1,
    "ffs_alloccg: map corrupted: start %d len %d offset %d %ld",
				ostart, olen,
				ufs_rw32(cgp->cg_freeoff, needswap),
				(long)cg_blksfree(cgp, needswap) - (long)cgp);
			/* NOTREACHED */
		}
	}
	bno = (start + len - loc) * NBBY;
	cgp->cg_frotor = ufs_rw32(bno, needswap);
	/*
	 * found the byte in the map
	 * sift through the bits to find the selected frag
	 */
	for (i = bno + NBBY; bno < i; bno += fs->fs_frag) {
		blk = blkmap(fs, cg_blksfree(cgp, needswap), bno);
		blk <<= 1;
		field = around[allocsiz];
		subfield = inside[allocsiz];
		for (pos = 0; pos <= fs->fs_frag - allocsiz; pos++) {
			if ((blk & field) == subfield)
				return (bno + pos);
			field <<= 1;
			subfield <<= 1;
		}
	}
	errx(1, "ffs_alloccg: block not in map: bno %lld", (long long)bno);
	return (-1);
}
