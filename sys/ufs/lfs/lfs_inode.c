/*	$NetBSD: lfs_inode.c,v 1.147 2015/09/01 06:13:09 dholland Exp $	*/

/*-
 * Copyright (c) 1999, 2000, 2001, 2002, 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Konrad E. Schroder <perseant@hhhh.org>.
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
 * Copyright (c) 1986, 1989, 1991, 1993
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
 *	@(#)lfs_inode.c	8.9 (Berkeley) 5/8/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: lfs_inode.c,v 1.147 2015/09/01 06:13:09 dholland Exp $");

#if defined(_KERNEL_OPT)
#include "opt_quota.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/kernel.h>
#include <sys/trace.h>
#include <sys/resourcevar.h>
#include <sys/kauth.h>

#include <ufs/lfs/ulfs_quotacommon.h>
#include <ufs/lfs/ulfs_inode.h>
#include <ufs/lfs/ulfsmount.h>
#include <ufs/lfs/ulfs_extern.h>

#include <ufs/lfs/lfs.h>
#include <ufs/lfs/lfs_accessors.h>
#include <ufs/lfs/lfs_extern.h>
#include <ufs/lfs/lfs_kernel.h>

static int lfs_update_seguse(struct lfs *, struct inode *ip, long, size_t);
static int lfs_indirtrunc(struct inode *, daddr_t, daddr_t,
			  daddr_t, int, daddr_t *, daddr_t *,
			  long *, size_t *);
static int lfs_blkfree (struct lfs *, struct inode *, daddr_t, size_t, long *, size_t *);
static int lfs_vtruncbuf(struct vnode *, daddr_t, bool, int);

/* Search a block for a specific dinode. */
union lfs_dinode *
lfs_ifind(struct lfs *fs, ino_t ino, struct buf *bp)
{
	union lfs_dinode *ldip;
	unsigned num, i;

	ASSERT_NO_SEGLOCK(fs);
	/*
	 * Read the inode block backwards, since later versions of the
	 * inode will supercede earlier ones.  Though it is unlikely, it is
	 * possible that the same inode will appear in the same inode block.
	 */
	num = LFS_INOPB(fs);
	for (i = num; i-- > 0; ) {
		ldip = DINO_IN_BLOCK(fs, bp->b_data, i);
		if (lfs_dino_getinumber(fs, ldip) == ino)
			return (ldip);
	}

	printf("searched %u entries for %ju\n", num, (uintmax_t)ino);
	printf("offset is 0x%jx (seg %d)\n", (uintmax_t)lfs_sb_getoffset(fs),
	       lfs_dtosn(fs, lfs_sb_getoffset(fs)));
	printf("block is 0x%jx (seg %d)\n",
	       (uintmax_t)LFS_DBTOFSB(fs, bp->b_blkno),
	       lfs_dtosn(fs, LFS_DBTOFSB(fs, bp->b_blkno)));

	return NULL;
}

int
lfs_update(struct vnode *vp, const struct timespec *acc,
    const struct timespec *mod, int updflags)
{
	struct inode *ip;
	struct lfs *fs = VFSTOULFS(vp->v_mount)->um_lfs;
	int flags;

	ASSERT_NO_SEGLOCK(fs);
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return (0);
	ip = VTOI(vp);

	/*
	 * If we are called from vinvalbuf, and the file's blocks have
	 * already been scheduled for writing, but the writes have not
	 * yet completed, lfs_vflush will not be called, and vinvalbuf
	 * will cause a panic.	So, we must wait until any pending write
	 * for our inode completes, if we are called with UPDATE_WAIT set.
	 */
	mutex_enter(vp->v_interlock);
	while ((updflags & (UPDATE_WAIT|UPDATE_DIROP)) == UPDATE_WAIT &&
	    WRITEINPROG(vp)) {
		DLOG((DLOG_SEG, "lfs_update: sleeping on ino %d"
		      " (in progress)\n", ip->i_number));
		cv_wait(&vp->v_cv, vp->v_interlock);
	}
	mutex_exit(vp->v_interlock);
	LFS_ITIMES(ip, acc, mod, NULL);
	if (updflags & UPDATE_CLOSE)
		flags = ip->i_flag & (IN_MODIFIED | IN_ACCESSED | IN_CLEANING);
	else
		flags = ip->i_flag & (IN_MODIFIED | IN_CLEANING);
	if (flags == 0)
		return (0);

	/* If sync, push back the vnode and any dirty blocks it may have. */
	if ((updflags & (UPDATE_WAIT|UPDATE_DIROP)) == UPDATE_WAIT) {
		/* Avoid flushing VU_DIROP. */
		mutex_enter(&lfs_lock);
		++fs->lfs_diropwait;
		while (vp->v_uflag & VU_DIROP) {
			DLOG((DLOG_DIROP, "lfs_update: sleeping on inode %d"
			      " (dirops)\n", ip->i_number));
			DLOG((DLOG_DIROP, "lfs_update: vflags 0x%x, iflags"
			      " 0x%x\n",
			      vp->v_iflag | vp->v_vflag | vp->v_uflag,
			      ip->i_flag));
			if (fs->lfs_dirops == 0)
				lfs_flush_fs(fs, SEGM_SYNC);
			else
				mtsleep(&fs->lfs_writer, PRIBIO+1, "lfs_fsync",
					0, &lfs_lock);
			/* XXX KS - by falling out here, are we writing the vn
			twice? */
		}
		--fs->lfs_diropwait;
		mutex_exit(&lfs_lock);
		return lfs_vflush(vp);
	}
	return 0;
}

#define	SINGLE	0	/* index of single indirect block */
#define	DOUBLE	1	/* index of double indirect block */
#define	TRIPLE	2	/* index of triple indirect block */
/*
 * Truncate the inode oip to at most length size, freeing the
 * disk blocks.
 */
/* VOP_BWRITE 1 + ULFS_NIADDR + lfs_balloc == 2 + 2*ULFS_NIADDR times */

int
lfs_truncate(struct vnode *ovp, off_t length, int ioflag, kauth_cred_t cred)
{
	daddr_t lastblock;
	struct inode *oip = VTOI(ovp);
	daddr_t bn, lbn, lastiblock[ULFS_NIADDR], indir_lbn[ULFS_NIADDR];
	/* note: newblks is set but only actually used if DIAGNOSTIC */
	daddr_t newblks[ULFS_NDADDR + ULFS_NIADDR] __diagused;
	struct lfs *fs;
	struct buf *bp;
	int offset, size, level;
	daddr_t count, rcount;
	daddr_t blocksreleased = 0, real_released = 0;
	int i, nblocks;
	int aflags, error, allerror = 0;
	off_t osize;
	long lastseg;
	size_t bc;
	int obufsize, odb;
	int usepc;

	if (ovp->v_type == VCHR || ovp->v_type == VBLK ||
	    ovp->v_type == VFIFO || ovp->v_type == VSOCK) {
		KASSERT(oip->i_size == 0);
		return 0;
	}

	if (length < 0)
		return (EINVAL);

	/*
	 * Just return and not update modification times.
	 */
	if (oip->i_size == length) {
		/* still do a uvm_vnp_setsize() as writesize may be larger */
		uvm_vnp_setsize(ovp, length);
		return (0);
	}

	fs = oip->i_lfs;

	if (ovp->v_type == VLNK &&
	    (oip->i_size < fs->um_maxsymlinklen ||
	     (fs->um_maxsymlinklen == 0 &&
	      lfs_dino_getblocks(fs, oip->i_din) == 0))) {
#ifdef DIAGNOSTIC
		if (length != 0)
			panic("lfs_truncate: partial truncate of symlink");
#endif
		memset((char *)SHORTLINK(oip), 0, (u_int)oip->i_size);
		oip->i_size = 0;
		lfs_dino_setsize(fs, oip->i_din, 0);
		oip->i_flag |= IN_CHANGE | IN_UPDATE;
		return (lfs_update(ovp, NULL, NULL, 0));
	}
	if (oip->i_size == length) {
		oip->i_flag |= IN_CHANGE | IN_UPDATE;
		return (lfs_update(ovp, NULL, NULL, 0));
	}
	lfs_imtime(fs);
	osize = oip->i_size;
	usepc = (ovp->v_type == VREG && ovp != fs->lfs_ivnode);

	ASSERT_NO_SEGLOCK(fs);
	/*
	 * Lengthen the size of the file. We must ensure that the
	 * last byte of the file is allocated. Since the smallest
	 * value of osize is 0, length will be at least 1.
	 */
	if (osize < length) {
		if (length > fs->um_maxfilesize)
			return (EFBIG);
		aflags = B_CLRBUF;
		if (ioflag & IO_SYNC)
			aflags |= B_SYNC;
		if (usepc) {
			if (lfs_lblkno(fs, osize) < ULFS_NDADDR &&
			    lfs_lblkno(fs, osize) != lfs_lblkno(fs, length) &&
			    lfs_blkroundup(fs, osize) != osize) {
				off_t eob;

				eob = lfs_blkroundup(fs, osize);
				uvm_vnp_setwritesize(ovp, eob);
				error = ulfs_balloc_range(ovp, osize,
				    eob - osize, cred, aflags);
				if (error) {
					(void) lfs_truncate(ovp, osize,
						    ioflag & IO_SYNC, cred);
					return error;
				}
				if (ioflag & IO_SYNC) {
					mutex_enter(ovp->v_interlock);
					VOP_PUTPAGES(ovp,
					    trunc_page(osize & lfs_sb_getbmask(fs)),
					    round_page(eob),
					    PGO_CLEANIT | PGO_SYNCIO);
				}
			}
			uvm_vnp_setwritesize(ovp, length);
			error = ulfs_balloc_range(ovp, length - 1, 1, cred,
						 aflags);
			if (error) {
				(void) lfs_truncate(ovp, osize,
						    ioflag & IO_SYNC, cred);
				return error;
			}
			uvm_vnp_setsize(ovp, length);
			oip->i_flag |= IN_CHANGE | IN_UPDATE;
			KASSERT(ovp->v_size == oip->i_size);
			oip->i_lfs_hiblk = lfs_lblkno(fs, oip->i_size + lfs_sb_getbsize(fs) - 1) - 1;
			return (lfs_update(ovp, NULL, NULL, 0));
		} else {
			error = lfs_reserve(fs, ovp, NULL,
			    lfs_btofsb(fs, (ULFS_NIADDR + 2) << lfs_sb_getbshift(fs)));
			if (error)
				return (error);
			error = lfs_balloc(ovp, length - 1, 1, cred,
					   aflags, &bp);
			lfs_reserve(fs, ovp, NULL,
			    -lfs_btofsb(fs, (ULFS_NIADDR + 2) << lfs_sb_getbshift(fs)));
			if (error)
				return (error);
			oip->i_size = length;
			lfs_dino_setsize(fs, oip->i_din, oip->i_size);
			uvm_vnp_setsize(ovp, length);
			(void) VOP_BWRITE(bp->b_vp, bp);
			oip->i_flag |= IN_CHANGE | IN_UPDATE;
			oip->i_lfs_hiblk = lfs_lblkno(fs, oip->i_size + lfs_sb_getbsize(fs) - 1) - 1;
			return (lfs_update(ovp, NULL, NULL, 0));
		}
	}

	if ((error = lfs_reserve(fs, ovp, NULL,
	    lfs_btofsb(fs, (2 * ULFS_NIADDR + 3) << lfs_sb_getbshift(fs)))) != 0)
		return (error);

	/*
	 * Shorten the size of the file. If the file is not being
	 * truncated to a block boundary, the contents of the
	 * partial block following the end of the file must be
	 * zero'ed in case it ever becomes accessible again because
	 * of subsequent file growth. Directories however are not
	 * zero'ed as they should grow back initialized to empty.
	 */
	offset = lfs_blkoff(fs, length);
	lastseg = -1;
	bc = 0;

	if (ovp != fs->lfs_ivnode)
		lfs_seglock(fs, SEGM_PROT);
	if (offset == 0) {
		oip->i_size = length;
		lfs_dino_setsize(fs, oip->i_din, oip->i_size);
	} else if (!usepc) {
		lbn = lfs_lblkno(fs, length);
		aflags = B_CLRBUF;
		if (ioflag & IO_SYNC)
			aflags |= B_SYNC;
		error = lfs_balloc(ovp, length - 1, 1, cred, aflags, &bp);
		if (error) {
			lfs_reserve(fs, ovp, NULL,
			    -lfs_btofsb(fs, (2 * ULFS_NIADDR + 3) << lfs_sb_getbshift(fs)));
			goto errout;
		}
		obufsize = bp->b_bufsize;
		odb = lfs_btofsb(fs, bp->b_bcount);
		oip->i_size = length;
		lfs_dino_setsize(fs, oip->i_din, oip->i_size);
		size = lfs_blksize(fs, oip, lbn);
		if (ovp->v_type != VDIR)
			memset((char *)bp->b_data + offset, 0,
			       (u_int)(size - offset));
		allocbuf(bp, size, 1);
		if ((bp->b_flags & B_LOCKED) != 0 && bp->b_iodone == NULL) {
			mutex_enter(&lfs_lock);
			locked_queue_bytes -= obufsize - bp->b_bufsize;
			mutex_exit(&lfs_lock);
		}
		if (bp->b_oflags & BO_DELWRI) {
			lfs_sb_addavail(fs, odb - lfs_btofsb(fs, size));
			/* XXX shouldn't this wake up on lfs_availsleep? */
		}
		(void) VOP_BWRITE(bp->b_vp, bp);
	} else { /* vp->v_type == VREG && length < osize && offset != 0 */
		/*
		 * When truncating a regular file down to a non-block-aligned
		 * size, we must zero the part of last block which is past
		 * the new EOF.  We must synchronously flush the zeroed pages
		 * to disk since the new pages will be invalidated as soon
		 * as we inform the VM system of the new, smaller size.
		 * We must do this before acquiring the GLOCK, since fetching
		 * the pages will acquire the GLOCK internally.
		 * So there is a window where another thread could see a whole
		 * zeroed page past EOF, but that's life.
		 */
		daddr_t xlbn;
		voff_t eoz;

		aflags = ioflag & IO_SYNC ? B_SYNC : 0;
		error = ulfs_balloc_range(ovp, length - 1, 1, cred, aflags);
		if (error) {
			lfs_reserve(fs, ovp, NULL,
				    -lfs_btofsb(fs, (2 * ULFS_NIADDR + 3) << lfs_sb_getbshift(fs)));
			goto errout;
		}
		xlbn = lfs_lblkno(fs, length);
		size = lfs_blksize(fs, oip, xlbn);
		eoz = MIN(lfs_lblktosize(fs, xlbn) + size, osize);
		ubc_zerorange(&ovp->v_uobj, length, eoz - length,
		    UBC_UNMAP_FLAG(ovp));
		if (round_page(eoz) > round_page(length)) {
			mutex_enter(ovp->v_interlock);
			error = VOP_PUTPAGES(ovp, round_page(length),
			    round_page(eoz),
			    PGO_CLEANIT | PGO_DEACTIVATE |
			    ((ioflag & IO_SYNC) ? PGO_SYNCIO : 0));
			if (error) {
				lfs_reserve(fs, ovp, NULL,
					    -lfs_btofsb(fs, (2 * ULFS_NIADDR + 3) << lfs_sb_getbshift(fs)));
				goto errout;
			}
		}
	}

	genfs_node_wrlock(ovp);

	oip->i_size = length;
	lfs_dino_setsize(fs, oip->i_din, oip->i_size);
	uvm_vnp_setsize(ovp, length);

	/*
	 * Calculate index into inode's block list of
	 * last direct and indirect blocks (if any)
	 * which we want to keep.  Lastblock is -1 when
	 * the file is truncated to 0.
	 */
	/* Avoid sign overflow - XXX assumes that off_t is a quad_t. */
	if (length > QUAD_MAX - lfs_sb_getbsize(fs))
		lastblock = lfs_lblkno(fs, QUAD_MAX - lfs_sb_getbsize(fs));
	else
		lastblock = lfs_lblkno(fs, length + lfs_sb_getbsize(fs) - 1) - 1;
	lastiblock[SINGLE] = lastblock - ULFS_NDADDR;
	lastiblock[DOUBLE] = lastiblock[SINGLE] - LFS_NINDIR(fs);
	lastiblock[TRIPLE] = lastiblock[DOUBLE] - LFS_NINDIR(fs) * LFS_NINDIR(fs);
	nblocks = lfs_btofsb(fs, lfs_sb_getbsize(fs));
	/*
	 * Record changed file and block pointers before we start
	 * freeing blocks.  lastiblock values are also normalized to -1
	 * for calls to lfs_indirtrunc below.
	 */
	for (i=0; i<ULFS_NDADDR; i++) {
		newblks[i] = lfs_dino_getdb(fs, oip->i_din, i);
	}
	for (i=0; i<ULFS_NIADDR; i++) {
		newblks[ULFS_NDADDR + i] = lfs_dino_getib(fs, oip->i_din, i);
	}
	for (level = TRIPLE; level >= SINGLE; level--)
		if (lastiblock[level] < 0) {
			newblks[ULFS_NDADDR+level] = 0;
			lastiblock[level] = -1;
		}
	for (i = ULFS_NDADDR - 1; i > lastblock; i--)
		newblks[i] = 0;

	oip->i_size = osize;
	lfs_dino_setsize(fs, oip->i_din, oip->i_size);
	error = lfs_vtruncbuf(ovp, lastblock + 1, false, 0);
	if (error && !allerror)
		allerror = error;

	/*
	 * Indirect blocks first.
	 */
	indir_lbn[SINGLE] = -ULFS_NDADDR;
	indir_lbn[DOUBLE] = indir_lbn[SINGLE] - LFS_NINDIR(fs) - 1;
	indir_lbn[TRIPLE] = indir_lbn[DOUBLE] - LFS_NINDIR(fs) * LFS_NINDIR(fs) - 1;
	for (level = TRIPLE; level >= SINGLE; level--) {
		bn = lfs_dino_getib(fs, oip->i_din, level);
		if (bn != 0) {
			error = lfs_indirtrunc(oip, indir_lbn[level],
					       bn, lastiblock[level],
					       level, &count, &rcount,
					       &lastseg, &bc);
			if (error)
				allerror = error;
			real_released += rcount;
			blocksreleased += count;
			if (lastiblock[level] < 0) {
				if (lfs_dino_getib(fs, oip->i_din, level) > 0)
					real_released += nblocks;
				blocksreleased += nblocks;
				lfs_dino_setib(fs, oip->i_din, level, 0);
				lfs_blkfree(fs, oip, bn, lfs_sb_getbsize(fs),
					    &lastseg, &bc);
        			lfs_deregister_block(ovp, bn);
			}
		}
		if (lastiblock[level] >= 0)
			goto done;
	}

	/*
	 * All whole direct blocks or frags.
	 */
	for (i = ULFS_NDADDR - 1; i > lastblock; i--) {
		long bsize, obsize;

		bn = lfs_dino_getdb(fs, oip->i_din, i);
		if (bn == 0)
			continue;
		bsize = lfs_blksize(fs, oip, i);
		if (lfs_dino_getdb(fs, oip->i_din, i) > 0) {
			/* Check for fragment size changes */
			obsize = oip->i_lfs_fragsize[i];
			real_released += lfs_btofsb(fs, obsize);
			oip->i_lfs_fragsize[i] = 0;
		} else
			obsize = 0;
		blocksreleased += lfs_btofsb(fs, bsize);
		lfs_dino_setdb(fs, oip->i_din, i, 0);
		lfs_blkfree(fs, oip, bn, obsize, &lastseg, &bc);
        	lfs_deregister_block(ovp, bn);
	}
	if (lastblock < 0)
		goto done;

	/*
	 * Finally, look for a change in size of the
	 * last direct block; release any frags.
	 */
	bn = lfs_dino_getdb(fs, oip->i_din, lastblock);
	if (bn != 0) {
		long oldspace, newspace;
#if 0
		long olddspace;
#endif

		/*
		 * Calculate amount of space we're giving
		 * back as old block size minus new block size.
		 */
		oldspace = lfs_blksize(fs, oip, lastblock);
#if 0
		olddspace = oip->i_lfs_fragsize[lastblock];
#endif

		oip->i_size = length;
		lfs_dino_setsize(fs, oip->i_din, oip->i_size);
		newspace = lfs_blksize(fs, oip, lastblock);
		if (newspace == 0)
			panic("itrunc: newspace");
		if (oldspace - newspace > 0) {
			blocksreleased += lfs_btofsb(fs, oldspace - newspace);
		}
#if 0
		if (bn > 0 && olddspace - newspace > 0) {
			/* No segment accounting here, just vnode */
			real_released += lfs_btofsb(fs, olddspace - newspace);
		}
#endif
	}

done:
	/* Finish segment accounting corrections */
	lfs_update_seguse(fs, oip, lastseg, bc);
#ifdef DIAGNOSTIC
	for (level = SINGLE; level <= TRIPLE; level++)
		if ((newblks[ULFS_NDADDR + level] == 0) !=
		    (lfs_dino_getib(fs, oip->i_din, level) == 0)) {
			panic("lfs itrunc1");
		}
	for (i = 0; i < ULFS_NDADDR; i++)
		if ((newblks[i] == 0) !=
		    (lfs_dino_getdb(fs, oip->i_din, i) == 0)) {
			panic("lfs itrunc2");
		}
	if (length == 0 &&
	    (!LIST_EMPTY(&ovp->v_cleanblkhd) || !LIST_EMPTY(&ovp->v_dirtyblkhd)))
		panic("lfs itrunc3");
#endif /* DIAGNOSTIC */
	/*
	 * Put back the real size.
	 */
	oip->i_size = length;
	lfs_dino_setsize(fs, oip->i_din, oip->i_size);
	oip->i_lfs_effnblks -= blocksreleased;
	lfs_dino_setblocks(fs, oip->i_din,
	    lfs_dino_getblocks(fs, oip->i_din) - real_released);
	mutex_enter(&lfs_lock);
	lfs_sb_addbfree(fs, blocksreleased);
	mutex_exit(&lfs_lock);
#ifdef DIAGNOSTIC
	if (oip->i_size == 0 &&
	    (lfs_dino_getblocks(fs, oip->i_din) != 0 || oip->i_lfs_effnblks != 0)) {
		printf("lfs_truncate: truncate to 0 but %jd blks/%jd effblks\n",
		       (intmax_t)lfs_dino_getblocks(fs, oip->i_din),
		       (intmax_t)oip->i_lfs_effnblks);
		panic("lfs_truncate: persistent blocks");
	}
#endif

	/*
	 * If we truncated to zero, take us off the paging queue.
	 */
	mutex_enter(&lfs_lock);
	if (oip->i_size == 0 && oip->i_flags & IN_PAGING) {
		oip->i_flags &= ~IN_PAGING;
		TAILQ_REMOVE(&fs->lfs_pchainhd, oip, i_lfs_pchain);
	}
	mutex_exit(&lfs_lock);

	oip->i_flag |= IN_CHANGE;
#if defined(LFS_QUOTA) || defined(LFS_QUOTA2)
	(void) lfs_chkdq(oip, -blocksreleased, NOCRED, 0);
#endif
	lfs_reserve(fs, ovp, NULL,
	    -lfs_btofsb(fs, (2 * ULFS_NIADDR + 3) << lfs_sb_getbshift(fs)));
	genfs_node_unlock(ovp);
  errout:
	oip->i_lfs_hiblk = lfs_lblkno(fs, oip->i_size + lfs_sb_getbsize(fs) - 1) - 1;
	if (ovp != fs->lfs_ivnode)
		lfs_segunlock(fs);
	return (allerror ? allerror : error);
}

/* Update segment and avail usage information when removing a block. */
static int
lfs_blkfree(struct lfs *fs, struct inode *ip, daddr_t daddr,
	    size_t bsize, long *lastseg, size_t *num)
{
	long seg;
	int error = 0;

	ASSERT_SEGLOCK(fs);
	bsize = lfs_fragroundup(fs, bsize);
	if (daddr > 0) {
		if (*lastseg != (seg = lfs_dtosn(fs, daddr))) {
			error = lfs_update_seguse(fs, ip, *lastseg, *num);
			*num = bsize;
			*lastseg = seg;
		} else
			*num += bsize;
	}

	return error;
}

/* Finish the accounting updates for a segment. */
static int
lfs_update_seguse(struct lfs *fs, struct inode *ip, long lastseg, size_t num)
{
	struct segdelta *sd;

	ASSERT_SEGLOCK(fs);
	if (lastseg < 0 || num == 0)
		return 0;

	LIST_FOREACH(sd, &ip->i_lfs_segdhd, list)
		if (sd->segnum == lastseg)
			break;
	if (sd == NULL) {
		sd = malloc(sizeof(*sd), M_SEGMENT, M_WAITOK);
		sd->segnum = lastseg;
		sd->num = 0;
		LIST_INSERT_HEAD(&ip->i_lfs_segdhd, sd, list);
	}
	sd->num += num;

	return 0;
}

static void
lfs_finalize_seguse(struct lfs *fs, void *v)
{
	SEGUSE *sup;
	struct buf *bp;
	struct segdelta *sd;
	LIST_HEAD(, segdelta) *hd = v;

	ASSERT_SEGLOCK(fs);
	while((sd = LIST_FIRST(hd)) != NULL) {
		LIST_REMOVE(sd, list);
		LFS_SEGENTRY(sup, fs, sd->segnum, bp);
		if (sd->num > sup->su_nbytes) {
			printf("lfs_finalize_seguse: segment %ld short by %ld\n",
				sd->segnum, (long)(sd->num - sup->su_nbytes));
			panic("lfs_finalize_seguse: negative bytes");
			sup->su_nbytes = sd->num;
		}
		sup->su_nbytes -= sd->num;
		LFS_WRITESEGENTRY(sup, fs, sd->segnum, bp);
		free(sd, M_SEGMENT);
	}
}

/* Finish the accounting updates for a segment. */
void
lfs_finalize_ino_seguse(struct lfs *fs, struct inode *ip)
{
	ASSERT_SEGLOCK(fs);
	lfs_finalize_seguse(fs, &ip->i_lfs_segdhd);
}

/* Finish the accounting updates for a segment. */
void
lfs_finalize_fs_seguse(struct lfs *fs)
{
	ASSERT_SEGLOCK(fs);
	lfs_finalize_seguse(fs, &fs->lfs_segdhd);
}

/*
 * Release blocks associated with the inode ip and stored in the indirect
 * block bn.  Blocks are free'd in LIFO order up to (but not including)
 * lastbn.  If level is greater than SINGLE, the block is an indirect block
 * and recursive calls to indirtrunc must be used to cleanse other indirect
 * blocks.
 *
 * NB: triple indirect blocks are untested.
 */
static int
lfs_indirtrunc(struct inode *ip, daddr_t lbn, daddr_t dbn,
	       daddr_t lastbn, int level, daddr_t *countp,
	       daddr_t *rcountp, long *lastsegp, size_t *bcp)
{
	int i;
	struct buf *bp;
	struct lfs *fs = ip->i_lfs;
	void *bap;
	bool bap_needs_free;
	struct vnode *vp;
	daddr_t nb, nlbn, last;
	daddr_t blkcount, rblkcount, factor;
	int nblocks;
	daddr_t blocksreleased = 0, real_released = 0;
	int error = 0, allerror = 0;

	ASSERT_SEGLOCK(fs);
	/*
	 * Calculate index in current block of last
	 * block to be kept.  -1 indicates the entire
	 * block so we need not calculate the index.
	 */
	factor = 1;
	for (i = SINGLE; i < level; i++)
		factor *= LFS_NINDIR(fs);
	last = lastbn;
	if (lastbn > 0)
		last /= factor;
	nblocks = lfs_btofsb(fs, lfs_sb_getbsize(fs));
	/*
	 * Get buffer of block pointers, zero those entries corresponding
	 * to blocks to be free'd, and update on disk copy first.  Since
	 * double(triple) indirect before single(double) indirect, calls
	 * to bmap on these blocks will fail.  However, we already have
	 * the on disk address, so we have to set the b_blkno field
	 * explicitly instead of letting bread do everything for us.
	 */
	vp = ITOV(ip);
	bp = getblk(vp, lbn, lfs_sb_getbsize(fs), 0, 0);
	if (bp->b_oflags & (BO_DONE | BO_DELWRI)) {
		/* Braces must be here in case trace evaluates to nothing. */
		trace(TR_BREADHIT, pack(vp, lfs_sb_getbsize(fs)), lbn);
	} else {
		trace(TR_BREADMISS, pack(vp, lfs_sb_getbsize(fs)), lbn);
		curlwp->l_ru.ru_inblock++; /* pay for read */
		bp->b_flags |= B_READ;
		if (bp->b_bcount > bp->b_bufsize)
			panic("lfs_indirtrunc: bad buffer size");
		bp->b_blkno = LFS_FSBTODB(fs, dbn);
		VOP_STRATEGY(vp, bp);
		error = biowait(bp);
	}
	if (error) {
		brelse(bp, 0);
		*countp = *rcountp = 0;
		return (error);
	}

	if (lastbn >= 0) {
		/*
		 * We still need this block, so copy the data for
		 * subsequent processing; then in the original block,
		 * zero out the dying block pointers and send it off.
		 */
		bap = lfs_malloc(fs, lfs_sb_getbsize(fs), LFS_NB_IBLOCK);
		memcpy(bap, bp->b_data, lfs_sb_getbsize(fs));
		bap_needs_free = true;

		for (i = last + 1; i < LFS_NINDIR(fs); i++) {
			lfs_iblock_set(fs, bp->b_data, i, 0);
		}
		error = VOP_BWRITE(bp->b_vp, bp);
		if (error)
			allerror = error;
	} else {
		bap = bp->b_data;
		bap_needs_free = false;
	}

	/*
	 * Recursively free totally unused blocks.
	 */
	for (i = LFS_NINDIR(fs) - 1, nlbn = lbn + 1 - i * factor; i > last;
	    i--, nlbn += factor) {
		nb = lfs_iblock_get(fs, bap, i);
		if (nb == 0)
			continue;
		if (level > SINGLE) {
			error = lfs_indirtrunc(ip, nlbn, nb,
					       (daddr_t)-1, level - 1,
					       &blkcount, &rblkcount,
					       lastsegp, bcp);
			if (error)
				allerror = error;
			blocksreleased += blkcount;
			real_released += rblkcount;
		}
		lfs_blkfree(fs, ip, nb, lfs_sb_getbsize(fs), lastsegp, bcp);
		if (lfs_iblock_get(fs, bap, i) > 0)
			real_released += nblocks;
		blocksreleased += nblocks;
	}

	/*
	 * Recursively free last partial block.
	 */
	if (level > SINGLE && lastbn >= 0) {
		last = lastbn % factor;
		nb = lfs_iblock_get(fs, bap, i);
		if (nb != 0) {
			error = lfs_indirtrunc(ip, nlbn, nb,
					       last, level - 1, &blkcount,
					       &rblkcount, lastsegp, bcp);
			if (error)
				allerror = error;
			real_released += rblkcount;
			blocksreleased += blkcount;
		}
	}

	if (bap_needs_free) {
		lfs_free(fs, bap, LFS_NB_IBLOCK);
	} else {
		mutex_enter(&bufcache_lock);
		if (bp->b_oflags & BO_DELWRI) {
			LFS_UNLOCK_BUF(bp);
			lfs_sb_addavail(fs, lfs_btofsb(fs, bp->b_bcount));
			wakeup(&fs->lfs_availsleep);
		}
		brelsel(bp, BC_INVAL);
		mutex_exit(&bufcache_lock);
	}

	*countp = blocksreleased;
	*rcountp = real_released;
	return (allerror);
}

/*
 * Destroy any in core blocks past the truncation length.
 * Inlined from vtruncbuf, so that lfs_avail could be updated.
 * We take the seglock to prevent cleaning from occurring while we are
 * invalidating blocks.
 */
static int
lfs_vtruncbuf(struct vnode *vp, daddr_t lbn, bool catch, int slptimeo)
{
	struct buf *bp, *nbp;
	int error;
	struct lfs *fs;
	voff_t off;

	off = round_page((voff_t)lbn << vp->v_mount->mnt_fs_bshift);
	mutex_enter(vp->v_interlock);
	error = VOP_PUTPAGES(vp, off, 0, PGO_FREE | PGO_SYNCIO);
	if (error)
		return error;

	fs = VTOI(vp)->i_lfs;

	ASSERT_SEGLOCK(fs);

	mutex_enter(&bufcache_lock);
restart:	
	for (bp = LIST_FIRST(&vp->v_cleanblkhd); bp; bp = nbp) {
		nbp = LIST_NEXT(bp, b_vnbufs);
		if (bp->b_lblkno < lbn)
			continue;
		error = bbusy(bp, catch, slptimeo, NULL);
		if (error == EPASSTHROUGH)
			goto restart;
		if (error != 0) {
			mutex_exit(&bufcache_lock);
			return (error);
		}
		mutex_enter(bp->b_objlock);
		if (bp->b_oflags & BO_DELWRI) {
			bp->b_oflags &= ~BO_DELWRI;
			lfs_sb_addavail(fs, lfs_btofsb(fs, bp->b_bcount));
			wakeup(&fs->lfs_availsleep);
		}
		mutex_exit(bp->b_objlock);
		LFS_UNLOCK_BUF(bp);
		brelsel(bp, BC_INVAL | BC_VFLUSH);
	}

	for (bp = LIST_FIRST(&vp->v_dirtyblkhd); bp; bp = nbp) {
		nbp = LIST_NEXT(bp, b_vnbufs);
		if (bp->b_lblkno < lbn)
			continue;
		error = bbusy(bp, catch, slptimeo, NULL);
		if (error == EPASSTHROUGH)
			goto restart;
		if (error != 0) {
			mutex_exit(&bufcache_lock);
			return (error);
		}
		mutex_enter(bp->b_objlock);
		if (bp->b_oflags & BO_DELWRI) {
			bp->b_oflags &= ~BO_DELWRI;
			lfs_sb_addavail(fs, lfs_btofsb(fs, bp->b_bcount));
			wakeup(&fs->lfs_availsleep);
		}
		mutex_exit(bp->b_objlock);
		LFS_UNLOCK_BUF(bp);
		brelsel(bp, BC_INVAL | BC_VFLUSH);
	}
	mutex_exit(&bufcache_lock);

	return (0);
}

