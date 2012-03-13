/*	$NetBSD: lfs_balloc.c,v 1.70 2011/07/11 08:27:40 hannken Exp $	*/

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
 * Copyright (c) 1989, 1991, 1993
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
 *	@(#)lfs_balloc.c	8.4 (Berkeley) 5/8/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: lfs_balloc.c,v 1.70 2011/07/11 08:27:40 hannken Exp $");

#if defined(_KERNEL_OPT)
#include "opt_quota.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/resourcevar.h>
#include <sys/tree.h>
#include <sys/trace.h>
#include <sys/kauth.h>

#include <miscfs/specfs/specdev.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/lfs/lfs.h>
#include <ufs/lfs/lfs_extern.h>

#include <uvm/uvm.h>

int lfs_fragextend(struct vnode *, int, int, daddr_t, struct buf **, kauth_cred_t);

u_int64_t locked_fakequeue_count;

/*
 * Allocate a block, and to inode and filesystem block accounting for it
 * and for any indirect blocks the may need to be created in order for
 * this block to be created.
 *
 * Blocks which have never been accounted for (i.e., which "do not exist")
 * have disk address 0, which is translated by ufs_bmap to the special value
 * UNASSIGNED == -1, as in the historical UFS.
 *
 * Blocks which have been accounted for but which have not yet been written
 * to disk are given the new special disk address UNWRITTEN == -2, so that
 * they can be differentiated from completely new blocks.
 */
/* VOP_BWRITE NIADDR+2 times */
int
lfs_balloc(struct vnode *vp, off_t startoffset, int iosize, kauth_cred_t cred,
    int flags, struct buf **bpp)
{
	int offset;
	daddr_t daddr, idaddr;
	struct buf *ibp, *bp;
	struct inode *ip;
	struct lfs *fs;
	struct indir indirs[NIADDR+2], *idp;
	daddr_t	lbn, lastblock;
	int bcount;
	int error, frags, i, nsize, osize, num;

	ip = VTOI(vp);
	fs = ip->i_lfs;
	offset = blkoff(fs, startoffset);
	KASSERT(iosize <= fs->lfs_bsize);
	lbn = lblkno(fs, startoffset);
	/* (void)lfs_check(vp, lbn, 0); */

	ASSERT_MAYBE_SEGLOCK(fs);

	/*
	 * Three cases: it's a block beyond the end of file, it's a block in
	 * the file that may or may not have been assigned a disk address or
	 * we're writing an entire block.
	 *
	 * Note, if the daddr is UNWRITTEN, the block already exists in
	 * the cache (it was read or written earlier).	If so, make sure
	 * we don't count it as a new block or zero out its contents. If
	 * it did not, make sure we allocate any necessary indirect
	 * blocks.
	 *
	 * If we are writing a block beyond the end of the file, we need to
	 * check if the old last block was a fragment.	If it was, we need
	 * to rewrite it.
	 */

	if (bpp)
		*bpp = NULL;

	/* Check for block beyond end of file and fragment extension needed. */
	lastblock = lblkno(fs, ip->i_size);
	if (lastblock < NDADDR && lastblock < lbn) {
		osize = blksize(fs, ip, lastblock);
		if (osize < fs->lfs_bsize && osize > 0) {
			if ((error = lfs_fragextend(vp, osize, fs->lfs_bsize,
						    lastblock,
						    (bpp ? &bp : NULL), cred)))
				return (error);
			ip->i_ffs1_size = ip->i_size =
			    (lastblock + 1) * fs->lfs_bsize;
			uvm_vnp_setsize(vp, ip->i_size);
			ip->i_flag |= IN_CHANGE | IN_UPDATE;
			if (bpp)
				(void) VOP_BWRITE(bp->b_vp, bp);
		}
	}

	/*
	 * If the block we are writing is a direct block, it's the last
	 * block in the file, and offset + iosize is less than a full
	 * block, we can write one or more fragments.  There are two cases:
	 * the block is brand new and we should allocate it the correct
	 * size or it already exists and contains some fragments and
	 * may need to extend it.
	 */
	if (lbn < NDADDR && lblkno(fs, ip->i_size) <= lbn) {
		osize = blksize(fs, ip, lbn);
		nsize = fragroundup(fs, offset + iosize);
		if (lblktosize(fs, lbn) >= ip->i_size) {
			/* Brand new block or fragment */
			frags = numfrags(fs, nsize);
			if (!ISSPACE(fs, frags, cred))
				return ENOSPC;
			if (bpp) {
				*bpp = bp = getblk(vp, lbn, nsize, 0, 0);
				bp->b_blkno = UNWRITTEN;
				if (flags & B_CLRBUF)
					clrbuf(bp);
			}
			ip->i_lfs_effnblks += frags;
			mutex_enter(&lfs_lock);
			fs->lfs_bfree -= frags;
			mutex_exit(&lfs_lock);
			ip->i_ffs1_db[lbn] = UNWRITTEN;
		} else {
			if (nsize <= osize) {
				/* No need to extend */
				if (bpp && (error = bread(vp, lbn, osize,
				    NOCRED, 0, &bp)))
					return error;
			} else {
				/* Extend existing block */
				if ((error =
				     lfs_fragextend(vp, osize, nsize, lbn,
						    (bpp ? &bp : NULL), cred)))
					return error;
			}
			if (bpp)
				*bpp = bp;
		}
		return 0;
	}

	error = ufs_bmaparray(vp, lbn, &daddr, &indirs[0], &num, NULL, NULL);
	if (error)
		return (error);

	daddr = (daddr_t)((int32_t)daddr); /* XXX ondisk32 */
	KASSERT(daddr <= LFS_MAX_DADDR);

	/*
	 * Do byte accounting all at once, so we can gracefully fail *before*
	 * we start assigning blocks.
	 */
	frags = VFSTOUFS(vp->v_mount)->um_seqinc;
	bcount = 0;
	if (daddr == UNASSIGNED) {
		bcount = frags;
	}
	for (i = 1; i < num; ++i) {
		if (!indirs[i].in_exists) {
			bcount += frags;
		}
	}
	if (ISSPACE(fs, bcount, cred)) {
		mutex_enter(&lfs_lock);
		fs->lfs_bfree -= bcount;
		mutex_exit(&lfs_lock);
		ip->i_lfs_effnblks += bcount;
	} else {
		return ENOSPC;
	}

	if (daddr == UNASSIGNED) {
		if (num > 0 && ip->i_ffs1_ib[indirs[0].in_off] == 0) {
			ip->i_ffs1_ib[indirs[0].in_off] = UNWRITTEN;
		}

		/*
		 * Create new indirect blocks if necessary
		 */
		if (num > 1) {
			idaddr = ip->i_ffs1_ib[indirs[0].in_off];
			for (i = 1; i < num; ++i) {
				ibp = getblk(vp, indirs[i].in_lbn,
				    fs->lfs_bsize, 0,0);
				if (!indirs[i].in_exists) {
					clrbuf(ibp);
					ibp->b_blkno = UNWRITTEN;
				} else if (!(ibp->b_oflags & (BO_DELWRI | BO_DONE))) {
					ibp->b_blkno = fsbtodb(fs, idaddr);
					ibp->b_flags |= B_READ;
					VOP_STRATEGY(vp, ibp);
					biowait(ibp);
				}
				/*
				 * This block exists, but the next one may not.
				 * If that is the case mark it UNWRITTEN to keep
				 * the accounting straight.
				 */
				/* XXX ondisk32 */
				if (((int32_t *)ibp->b_data)[indirs[i].in_off] == 0)
					((int32_t *)ibp->b_data)[indirs[i].in_off] =
						UNWRITTEN;
				/* XXX ondisk32 */
				idaddr = ((int32_t *)ibp->b_data)[indirs[i].in_off];
#ifdef DEBUG
				if (vp == fs->lfs_ivnode) {
					LFS_ENTER_LOG("balloc", __FILE__,
						__LINE__, indirs[i].in_lbn,
						ibp->b_flags, curproc->p_pid);
				}
#endif
				if ((error = VOP_BWRITE(ibp->b_vp, ibp)))
					return error;
			}
		}
	}


	/*
	 * Get the existing block from the cache, if requested.
	 */
	if (bpp)
		*bpp = bp = getblk(vp, lbn, blksize(fs, ip, lbn), 0, 0);

	/*
	 * Do accounting on blocks that represent pages.
	 */
	if (!bpp)
		lfs_register_block(vp, lbn);

	/*
	 * The block we are writing may be a brand new block
	 * in which case we need to do accounting.
	 *
	 * We can tell a truly new block because ufs_bmaparray will say
	 * it is UNASSIGNED.  Once we allocate it we will assign it the
	 * disk address UNWRITTEN.
	 */
	if (daddr == UNASSIGNED) {
		if (bpp) {
			if (flags & B_CLRBUF)
				clrbuf(bp);

			/* Note the new address */
			bp->b_blkno = UNWRITTEN;
		}

		switch (num) {
		    case 0:
			ip->i_ffs1_db[lbn] = UNWRITTEN;
			break;
		    case 1:
			ip->i_ffs1_ib[indirs[0].in_off] = UNWRITTEN;
			break;
		    default:
			idp = &indirs[num - 1];
			if (bread(vp, idp->in_lbn, fs->lfs_bsize, NOCRED,
				  B_MODIFY, &ibp))
				panic("lfs_balloc: bread bno %lld",
				    (long long)idp->in_lbn);
			/* XXX ondisk32 */
			((int32_t *)ibp->b_data)[idp->in_off] = UNWRITTEN;
#ifdef DEBUG
			if (vp == fs->lfs_ivnode) {
				LFS_ENTER_LOG("balloc", __FILE__,
					__LINE__, idp->in_lbn,
					ibp->b_flags, curproc->p_pid);
			}
#endif
			VOP_BWRITE(ibp->b_vp, ibp);
		}
	} else if (bpp && !(bp->b_oflags & (BO_DONE|BO_DELWRI))) {
		/*
		 * Not a brand new block, also not in the cache;
		 * read it in from disk.
		 */
		if (iosize == fs->lfs_bsize)
			/* Optimization: I/O is unnecessary. */
			bp->b_blkno = daddr;
		else {
			/*
			 * We need to read the block to preserve the
			 * existing bytes.
			 */
			bp->b_blkno = daddr;
			bp->b_flags |= B_READ;
			VOP_STRATEGY(vp, bp);
			return (biowait(bp));
		}
	}

	return (0);
}

/* VOP_BWRITE 1 time */
int
lfs_fragextend(struct vnode *vp, int osize, int nsize, daddr_t lbn, struct buf **bpp,
    kauth_cred_t cred)
{
	struct inode *ip;
	struct lfs *fs;
	long frags;
	int error;
	extern long locked_queue_bytes;
	size_t obufsize;

	ip = VTOI(vp);
	fs = ip->i_lfs;
	frags = (long)numfrags(fs, nsize - osize);
	error = 0;

	ASSERT_NO_SEGLOCK(fs);

	/*
	 * Get the seglock so we don't enlarge blocks while a segment
	 * is being written.  If we're called with bpp==NULL, though,
	 * we are only pretending to change a buffer, so we don't have to
	 * lock.
	 */
    top:
	if (bpp) {
		rw_enter(&fs->lfs_fraglock, RW_READER);
		LFS_DEBUG_COUNTLOCKED("frag");
	}

	if (!ISSPACE(fs, frags, cred)) {
		error = ENOSPC;
		goto out;
	}

	/*
	 * If we are not asked to actually return the block, all we need
	 * to do is allocate space for it.  UBC will handle dirtying the
	 * appropriate things and making sure it all goes to disk.
	 * Don't bother to read in that case.
	 */
	if (bpp && (error = bread(vp, lbn, osize, NOCRED, 0, bpp))) {
		brelse(*bpp, 0);
		goto out;
	}
#ifdef QUOTA
	if ((error = chkdq(ip, frags, cred, 0))) {
		if (bpp)
			brelse(*bpp, 0);
		goto out;
	}
#endif
	/*
	 * Adjust accounting for lfs_avail.  If there's not enough room,
	 * we will have to wait for the cleaner, which we can't do while
	 * holding a block busy or while holding the seglock.  In that case,
	 * release both and start over after waiting.
	 */

	if (bpp && ((*bpp)->b_oflags & BO_DELWRI)) {
		if (!lfs_fits(fs, frags)) {
			if (bpp)
				brelse(*bpp, 0);
#ifdef QUOTA
			chkdq(ip, -frags, cred, 0);
#endif
			rw_exit(&fs->lfs_fraglock);
			lfs_availwait(fs, frags);
			goto top;
		}
		fs->lfs_avail -= frags;
	}

	mutex_enter(&lfs_lock);
	fs->lfs_bfree -= frags;
	mutex_exit(&lfs_lock);
	ip->i_lfs_effnblks += frags;
	ip->i_flag |= IN_CHANGE | IN_UPDATE;

	if (bpp) {
		obufsize = (*bpp)->b_bufsize;
		allocbuf(*bpp, nsize, 1);

		/* Adjust locked-list accounting */
		if (((*bpp)->b_flags & B_LOCKED) != 0 &&
		    (*bpp)->b_iodone == NULL) {
			mutex_enter(&lfs_lock);
			locked_queue_bytes += (*bpp)->b_bufsize - obufsize;
			mutex_exit(&lfs_lock);
		}

		memset((char *)((*bpp)->b_data) + osize, 0, (u_int)(nsize - osize));
	}

    out:
	if (bpp) {
		rw_exit(&fs->lfs_fraglock);
	}
	return (error);
}

static inline int
lge(struct lbnentry *a, struct lbnentry *b)
{
	return a->lbn - b->lbn;
}

SPLAY_PROTOTYPE(lfs_splay, lbnentry, entry, lge);

SPLAY_GENERATE(lfs_splay, lbnentry, entry, lge);

/*
 * Record this lbn as being "write pending".  We used to have this information
 * on the buffer headers, but since pages don't have buffer headers we
 * record it here instead.
 */
void
lfs_register_block(struct vnode *vp, daddr_t lbn)
{
	struct lfs *fs;
	struct inode *ip;
	struct lbnentry *lbp;

	ip = VTOI(vp);

	/* Don't count metadata */
	if (lbn < 0 || vp->v_type != VREG || ip->i_number == LFS_IFILE_INUM)
		return;

	fs = ip->i_lfs;

	ASSERT_NO_SEGLOCK(fs);

	/* If no space, wait for the cleaner */
	lfs_availwait(fs, btofsb(fs, 1 << fs->lfs_bshift));

	lbp = (struct lbnentry *)pool_get(&lfs_lbnentry_pool, PR_WAITOK);
	lbp->lbn = lbn;
	mutex_enter(&lfs_lock);
	if (SPLAY_INSERT(lfs_splay, &ip->i_lfs_lbtree, lbp) != NULL) {
		mutex_exit(&lfs_lock);
		/* Already there */
		pool_put(&lfs_lbnentry_pool, lbp);
		return;
	}

	++ip->i_lfs_nbtree;
	fs->lfs_favail += btofsb(fs, (1 << fs->lfs_bshift));
	fs->lfs_pages += fs->lfs_bsize >> PAGE_SHIFT;
	++locked_fakequeue_count;
	lfs_subsys_pages += fs->lfs_bsize >> PAGE_SHIFT;
	mutex_exit(&lfs_lock);
}

static void
lfs_do_deregister(struct lfs *fs, struct inode *ip, struct lbnentry *lbp)
{
	ASSERT_MAYBE_SEGLOCK(fs);

	mutex_enter(&lfs_lock);
	--ip->i_lfs_nbtree;
	SPLAY_REMOVE(lfs_splay, &ip->i_lfs_lbtree, lbp);
	if (fs->lfs_favail > btofsb(fs, (1 << fs->lfs_bshift)))
		fs->lfs_favail -= btofsb(fs, (1 << fs->lfs_bshift));
	fs->lfs_pages -= fs->lfs_bsize >> PAGE_SHIFT;
	if (locked_fakequeue_count > 0)
		--locked_fakequeue_count;
	lfs_subsys_pages -= fs->lfs_bsize >> PAGE_SHIFT;
	mutex_exit(&lfs_lock);

	pool_put(&lfs_lbnentry_pool, lbp);
}

void
lfs_deregister_block(struct vnode *vp, daddr_t lbn)
{
	struct lfs *fs;
	struct inode *ip;
	struct lbnentry *lbp;
	struct lbnentry tmp;

	ip = VTOI(vp);

	/* Don't count metadata */
	if (lbn < 0 || vp->v_type != VREG || ip->i_number == LFS_IFILE_INUM)
		return;

	fs = ip->i_lfs;
	tmp.lbn = lbn;
	lbp = SPLAY_FIND(lfs_splay, &ip->i_lfs_lbtree, &tmp);
	if (lbp == NULL)
		return;

	lfs_do_deregister(fs, ip, lbp);
}

void
lfs_deregister_all(struct vnode *vp)
{
	struct lbnentry *lbp, *nlbp;
	struct lfs_splay *hd;
	struct lfs *fs;
	struct inode *ip;

	ip = VTOI(vp);
	fs = ip->i_lfs;
	hd = &ip->i_lfs_lbtree;

	for (lbp = SPLAY_MIN(lfs_splay, hd); lbp != NULL; lbp = nlbp) {
		nlbp = SPLAY_NEXT(lfs_splay, hd, lbp);
		lfs_do_deregister(fs, ip, lbp);
	}
}
