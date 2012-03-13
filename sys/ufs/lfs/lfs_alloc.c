/*	$NetBSD: lfs_alloc.c,v 1.111 2011/06/12 03:36:01 rmind Exp $	*/

/*-
 * Copyright (c) 1999, 2000, 2001, 2002, 2003, 2007 The NetBSD Foundation, Inc.
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
 * Copyright (c) 1991, 1993
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
 *	@(#)lfs_alloc.c	8.4 (Berkeley) 1/4/94
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: lfs_alloc.c,v 1.111 2011/06/12 03:36:01 rmind Exp $");

#if defined(_KERNEL_OPT)
#include "opt_quota.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/lock.h>
#include <sys/vnode.h>
#include <sys/syslog.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/proc.h>
#include <sys/tree.h>
#include <sys/kauth.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/lfs/lfs.h>
#include <ufs/lfs/lfs_extern.h>

/* Constants for inode free bitmap */
#define BMSHIFT 5	/* 2 ** 5 = 32 */
#define BMMASK  ((1 << BMSHIFT) - 1)
#define SET_BITMAP_FREE(F, I) do { \
	DLOG((DLOG_ALLOC, "lfs: ino %d wrd %d bit %d set\n", (int)(I), 	\
	     (int)((I) >> BMSHIFT), (int)((I) & BMMASK)));		\
	(F)->lfs_ino_bitmap[(I) >> BMSHIFT] |= (1 << ((I) & BMMASK));	\
} while (0)
#define CLR_BITMAP_FREE(F, I) do { \
	DLOG((DLOG_ALLOC, "lfs: ino %d wrd %d bit %d clr\n", (int)(I), 	\
	     (int)((I) >> BMSHIFT), (int)((I) & BMMASK)));		\
	(F)->lfs_ino_bitmap[(I) >> BMSHIFT] &= ~(1 << ((I) & BMMASK));	\
} while(0)

#define ISSET_BITMAP_FREE(F, I) \
	((F)->lfs_ino_bitmap[(I) >> BMSHIFT] & (1 << ((I) & BMMASK)))

/*
 * Add a new block to the Ifile, to accommodate future file creations.
 * Called with the segment lock held.
 */
int
lfs_extend_ifile(struct lfs *fs, kauth_cred_t cred)
{
	struct vnode *vp;
	struct inode *ip;
	IFILE *ifp;
	IFILE_V1 *ifp_v1;
	struct buf *bp, *cbp;
	int error;
	daddr_t i, blkno, xmax;
	ino_t oldlast, maxino;
	CLEANERINFO *cip;

	ASSERT_SEGLOCK(fs);

	vp = fs->lfs_ivnode;
	ip = VTOI(vp);
	blkno = lblkno(fs, ip->i_size);
	if ((error = lfs_balloc(vp, ip->i_size, fs->lfs_bsize, cred, 0,
				&bp)) != 0) {
		return (error);
	}
	ip->i_size += fs->lfs_bsize;
	ip->i_ffs1_size = ip->i_size;
	uvm_vnp_setsize(vp, ip->i_size);

	maxino = ((ip->i_size >> fs->lfs_bshift) - fs->lfs_cleansz -
		  fs->lfs_segtabsz) * fs->lfs_ifpb;
	fs->lfs_ino_bitmap = (lfs_bm_t *)
		realloc(fs->lfs_ino_bitmap, ((maxino + BMMASK) >> BMSHIFT) *
			sizeof(lfs_bm_t), M_SEGMENT, M_WAITOK);
	KASSERT(fs->lfs_ino_bitmap != NULL);

	i = (blkno - fs->lfs_segtabsz - fs->lfs_cleansz) *
		fs->lfs_ifpb;

	/*
	 * We insert the new inodes at the head of the free list.
	 * Under normal circumstances, the free list is empty here,
	 * so we are also incidentally placing them at the end (which
	 * we must do if we are to keep them in order).
	 */
	LFS_GET_HEADFREE(fs, cip, cbp, &oldlast);
	LFS_PUT_HEADFREE(fs, cip, cbp, i);
#ifdef DIAGNOSTIC
	if (fs->lfs_freehd == LFS_UNUSED_INUM)
		panic("inode 0 allocated [2]");
#endif /* DIAGNOSTIC */
	xmax = i + fs->lfs_ifpb;

	if (fs->lfs_version == 1) {
		for (ifp_v1 = (IFILE_V1 *)bp->b_data; i < xmax; ++ifp_v1) {
			SET_BITMAP_FREE(fs, i);
			ifp_v1->if_version = 1;
			ifp_v1->if_daddr = LFS_UNUSED_DADDR;
			ifp_v1->if_nextfree = ++i;
		}
		ifp_v1--;
		ifp_v1->if_nextfree = oldlast;
	} else {
		for (ifp = (IFILE *)bp->b_data; i < xmax; ++ifp) {
			SET_BITMAP_FREE(fs, i);
			ifp->if_version = 1;
			ifp->if_daddr = LFS_UNUSED_DADDR;
			ifp->if_nextfree = ++i;
		}
		ifp--;
		ifp->if_nextfree = oldlast;
	}
	LFS_PUT_TAILFREE(fs, cip, cbp, xmax - 1);

	(void) LFS_BWRITE_LOG(bp); /* Ifile */

	return 0;
}

/* Allocate a new inode. */
/* ARGSUSED */
/* VOP_BWRITE 2i times */
int
lfs_valloc(struct vnode *pvp, int mode, kauth_cred_t cred,
    struct vnode **vpp)
{
	struct lfs *fs;
	struct buf *bp, *cbp;
	struct ifile *ifp;
	ino_t new_ino;
	int error;
	int new_gen;
	CLEANERINFO *cip;

	fs = VTOI(pvp)->i_lfs;
	if (fs->lfs_ronly)
		return EROFS;

	ASSERT_NO_SEGLOCK(fs);

	lfs_seglock(fs, SEGM_PROT);
	vn_lock(fs->lfs_ivnode, LK_EXCLUSIVE);

	/* Get the head of the freelist. */
	LFS_GET_HEADFREE(fs, cip, cbp, &new_ino);
	KASSERT(new_ino != LFS_UNUSED_INUM && new_ino != LFS_IFILE_INUM);

	DLOG((DLOG_ALLOC, "lfs_valloc: allocate inode %lld\n",
	     (long long)new_ino));

	/*
	 * Remove the inode from the free list and write the new start
	 * of the free list into the superblock.
	 */
	CLR_BITMAP_FREE(fs, new_ino);
	LFS_IENTRY(ifp, fs, new_ino, bp);
	if (ifp->if_daddr != LFS_UNUSED_DADDR)
		panic("lfs_valloc: inuse inode %llu on the free list",
		    (unsigned long long)new_ino);
	LFS_PUT_HEADFREE(fs, cip, cbp, ifp->if_nextfree);
	DLOG((DLOG_ALLOC, "lfs_valloc: headfree %lld -> %lld\n",
	     (long long)new_ino, (long long)ifp->if_nextfree));

	new_gen = ifp->if_version; /* version was updated by vfree */
	brelse(bp, 0);

	/* Extend IFILE so that the next lfs_valloc will succeed. */
	if (fs->lfs_freehd == LFS_UNUSED_INUM) {
		if ((error = lfs_extend_ifile(fs, cred)) != 0) {
			LFS_PUT_HEADFREE(fs, cip, cbp, new_ino);
			VOP_UNLOCK(fs->lfs_ivnode);
			lfs_segunlock(fs);
			return error;
		}
	}
#ifdef DIAGNOSTIC
	if (fs->lfs_freehd == LFS_UNUSED_INUM)
		panic("inode 0 allocated [3]");
#endif /* DIAGNOSTIC */

	/* Set superblock modified bit and increment file count. */
	mutex_enter(&lfs_lock);
	fs->lfs_fmod = 1;
	mutex_exit(&lfs_lock);
	++fs->lfs_nfiles;

	VOP_UNLOCK(fs->lfs_ivnode);
	lfs_segunlock(fs);

	return lfs_ialloc(fs, pvp, new_ino, new_gen, vpp);
}

/*
 * Finish allocating a new inode, given an inode and generation number.
 */
int
lfs_ialloc(struct lfs *fs, struct vnode *pvp, ino_t new_ino, int new_gen,
	   struct vnode **vpp)
{
	struct inode *ip;
	struct vnode *vp;

	ASSERT_NO_SEGLOCK(fs);

	vp = *vpp;
	mutex_enter(&ufs_hashlock);
	/* Create an inode to associate with the vnode. */
	lfs_vcreate(pvp->v_mount, new_ino, vp);

	ip = VTOI(vp);
	mutex_enter(&lfs_lock);
	LFS_SET_UINO(ip, IN_CHANGE);
	mutex_exit(&lfs_lock);
	/* on-disk structure has been zeroed out by lfs_vcreate */
	ip->i_din.ffs1_din->di_inumber = new_ino;

	/* Note no blocks yet */
	ip->i_lfs_hiblk = -1;

	/* Set a new generation number for this inode. */
	if (new_gen) {
		ip->i_gen = new_gen;
		ip->i_ffs1_gen = new_gen;
	}

	/* Insert into the inode hash table. */
	ufs_ihashins(ip);
	mutex_exit(&ufs_hashlock);

	ufs_vinit(vp->v_mount, lfs_specop_p, lfs_fifoop_p, vpp);
	vp = *vpp;
	ip = VTOI(vp);

	memset(ip->i_lfs_fragsize, 0, NDADDR * sizeof(*ip->i_lfs_fragsize));

	uvm_vnp_setsize(vp, 0);
	lfs_mark_vnode(vp);
	genfs_node_init(vp, &lfs_genfsops);
	vref(ip->i_devvp);
	return (0);
}

/* Create a new vnode/inode pair and initialize what fields we can. */
void
lfs_vcreate(struct mount *mp, ino_t ino, struct vnode *vp)
{
	struct inode *ip;
	struct ufs1_dinode *dp;
	struct ufsmount *ump;

	/* Get a pointer to the private mount structure. */
	ump = VFSTOUFS(mp);

	ASSERT_NO_SEGLOCK(ump->um_lfs);

	/* Initialize the inode. */
	ip = pool_get(&lfs_inode_pool, PR_WAITOK);
	memset(ip, 0, sizeof(*ip));
	dp = pool_get(&lfs_dinode_pool, PR_WAITOK);
	memset(dp, 0, sizeof(*dp));
	ip->inode_ext.lfs = pool_get(&lfs_inoext_pool, PR_WAITOK);
	memset(ip->inode_ext.lfs, 0, sizeof(*ip->inode_ext.lfs));
	vp->v_data = ip;
	ip->i_din.ffs1_din = dp;
	ip->i_ump = ump;
	ip->i_vnode = vp;
	ip->i_devvp = ump->um_devvp;
	ip->i_dev = ump->um_dev;
	ip->i_number = dp->di_inumber = ino;
	ip->i_lfs = ump->um_lfs;
	ip->i_lfs_effnblks = 0;
	SPLAY_INIT(&ip->i_lfs_lbtree);
	ip->i_lfs_nbtree = 0;
	LIST_INIT(&ip->i_lfs_segdhd);
#ifdef QUOTA
	ufsquota_init(ip);
#endif
}

#if 0
/*
 * Find the highest-numbered allocated inode.
 * This will be used to shrink the Ifile.
 */
static inline ino_t
lfs_last_alloc_ino(struct lfs *fs)
{
	ino_t ino, maxino;

	maxino = ((fs->lfs_ivnode->v_size >> fs->lfs_bshift) -
		  fs->lfs_cleansz - fs->lfs_segtabsz) * fs->lfs_ifpb;
	for (ino = maxino - 1; ino > LFS_UNUSED_INUM; --ino) {
		if (ISSET_BITMAP_FREE(fs, ino) == 0)
			break;
	}
	return ino;
}
#endif

/*
 * Find the previous (next lowest numbered) free inode, if any.
 * If there is none, return LFS_UNUSED_INUM.
 */
static inline ino_t
lfs_freelist_prev(struct lfs *fs, ino_t ino)
{
	ino_t tino, bound, bb, freehdbb;

	if (fs->lfs_freehd == LFS_UNUSED_INUM)	 /* No free inodes at all */
		return LFS_UNUSED_INUM;

	/* Search our own word first */
	bound = ino & ~BMMASK;
	for (tino = ino - 1; tino >= bound && tino > LFS_UNUSED_INUM; tino--)
		if (ISSET_BITMAP_FREE(fs, tino))
			return tino;
	/* If there are no lower words to search, just return */
	if (ino >> BMSHIFT == 0)
		return LFS_UNUSED_INUM;

	/*
	 * Find a word with a free inode in it.  We have to be a bit
	 * careful here since ino_t is unsigned.
	 */
	freehdbb = (fs->lfs_freehd >> BMSHIFT);
	for (bb = (ino >> BMSHIFT) - 1; bb >= freehdbb && bb > 0; --bb)
		if (fs->lfs_ino_bitmap[bb])
			break;
	if (fs->lfs_ino_bitmap[bb] == 0)
		return LFS_UNUSED_INUM;

	/* Search the word we found */
	for (tino = (bb << BMSHIFT) | BMMASK; tino >= (bb << BMSHIFT) &&
	     tino > LFS_UNUSED_INUM; tino--)
		if (ISSET_BITMAP_FREE(fs, tino))
			break;

	if (tino <= LFS_IFILE_INUM)
		tino = LFS_UNUSED_INUM;

	return tino;
}

/* Free an inode. */
/* ARGUSED */
/* VOP_BWRITE 2i times */
int
lfs_vfree(struct vnode *vp, ino_t ino, int mode)
{
	SEGUSE *sup;
	CLEANERINFO *cip;
	struct buf *cbp, *bp;
	struct ifile *ifp;
	struct inode *ip;
	struct lfs *fs;
	daddr_t old_iaddr;
	ino_t otail;

	/* Get the inode number and file system. */
	ip = VTOI(vp);
	fs = ip->i_lfs;
	ino = ip->i_number;

	ASSERT_NO_SEGLOCK(fs);
	DLOG((DLOG_ALLOC, "lfs_vfree: free ino %lld\n", (long long)ino));

	/* Drain of pending writes */
	mutex_enter(vp->v_interlock);
	while (fs->lfs_version > 1 && WRITEINPROG(vp)) {
		cv_wait(&vp->v_cv, vp->v_interlock);
	}
	mutex_exit(vp->v_interlock);

	lfs_seglock(fs, SEGM_PROT);
	vn_lock(fs->lfs_ivnode, LK_EXCLUSIVE);

	lfs_unmark_vnode(vp);
	mutex_enter(&lfs_lock);
	if (vp->v_uflag & VU_DIROP) {
		vp->v_uflag &= ~VU_DIROP;
		--lfs_dirvcount;
		--fs->lfs_dirvcount;
		TAILQ_REMOVE(&fs->lfs_dchainhd, ip, i_lfs_dchain);
		wakeup(&fs->lfs_dirvcount);
		wakeup(&lfs_dirvcount);
		mutex_exit(&lfs_lock);
		lfs_vunref(vp);

		/*
		 * If this inode is not going to be written any more, any
		 * segment accounting left over from its truncation needs
		 * to occur at the end of the next dirops flush.  Attach
		 * them to the fs-wide list for that purpose.
		 */
		if (LIST_FIRST(&ip->i_lfs_segdhd) != NULL) {
			struct segdelta *sd;
	
			while((sd = LIST_FIRST(&ip->i_lfs_segdhd)) != NULL) {
				LIST_REMOVE(sd, list);
				LIST_INSERT_HEAD(&fs->lfs_segdhd, sd, list);
			}
		}
	} else {
		/*
		 * If it's not a dirop, we can finalize right away.
		 */
		mutex_exit(&lfs_lock);
		lfs_finalize_ino_seguse(fs, ip);
	}

	mutex_enter(&lfs_lock);
	LFS_CLR_UINO(ip, IN_ACCESSED|IN_CLEANING|IN_MODIFIED);
	mutex_exit(&lfs_lock);
	ip->i_flag &= ~IN_ALLMOD;
	ip->i_lfs_iflags |= LFSI_DELETED;
	
	/*
	 * Set the ifile's inode entry to unused, increment its version number
	 * and link it onto the free chain.
	 */
	SET_BITMAP_FREE(fs, ino);
	LFS_IENTRY(ifp, fs, ino, bp);
	old_iaddr = ifp->if_daddr;
	ifp->if_daddr = LFS_UNUSED_DADDR;
	++ifp->if_version;
	if (fs->lfs_version == 1) {
		LFS_GET_HEADFREE(fs, cip, cbp, &(ifp->if_nextfree));
		LFS_PUT_HEADFREE(fs, cip, cbp, ino);
		(void) LFS_BWRITE_LOG(bp); /* Ifile */
	} else {
		ino_t tino, onf;

		ifp->if_nextfree = LFS_UNUSED_INUM;
		(void) LFS_BWRITE_LOG(bp); /* Ifile */

		tino = lfs_freelist_prev(fs, ino);
		if (tino == LFS_UNUSED_INUM) {
			/* Nothing free below us, put us on the head */
			LFS_IENTRY(ifp, fs, ino, bp);
			LFS_GET_HEADFREE(fs, cip, cbp, &(ifp->if_nextfree));
			LFS_PUT_HEADFREE(fs, cip, cbp, ino);
			DLOG((DLOG_ALLOC, "lfs_vfree: headfree %lld -> %lld\n",
			     (long long)ifp->if_nextfree, (long long)ino));
			LFS_BWRITE_LOG(bp); /* Ifile */

			/* If the list was empty, set tail too */
			LFS_GET_TAILFREE(fs, cip, cbp, &otail);
			if (otail == LFS_UNUSED_INUM) {
				LFS_PUT_TAILFREE(fs, cip, cbp, ino);
				DLOG((DLOG_ALLOC, "lfs_vfree: tailfree %lld "
				      "-> %lld\n", (long long)otail,
				      (long long)ino));
			}
		} else {
			/*
			 * Insert this inode into the list after tino.
			 * We hold the segment lock so we don't have to
			 * worry about blocks being written out of order.
			 */
			DLOG((DLOG_ALLOC, "lfs_vfree: insert ino %lld "
			      " after %lld\n", ino, tino));

			LFS_IENTRY(ifp, fs, tino, bp);
			onf = ifp->if_nextfree;
			ifp->if_nextfree = ino;
			LFS_BWRITE_LOG(bp);	/* Ifile */

			LFS_IENTRY(ifp, fs, ino, bp);
			ifp->if_nextfree = onf;
			LFS_BWRITE_LOG(bp);	/* Ifile */

			/* If we're last, put us on the tail */
			if (onf == LFS_UNUSED_INUM) {
				LFS_GET_TAILFREE(fs, cip, cbp, &otail);
				LFS_PUT_TAILFREE(fs, cip, cbp, ino);
				DLOG((DLOG_ALLOC, "lfs_vfree: tailfree %lld "
				      "-> %lld\n", (long long)otail,
				      (long long)ino));
			}
		}
	}
#ifdef DIAGNOSTIC
	if (ino == LFS_UNUSED_INUM) {
		panic("inode 0 freed");
	}
#endif /* DIAGNOSTIC */
	if (old_iaddr != LFS_UNUSED_DADDR) {
		LFS_SEGENTRY(sup, fs, dtosn(fs, old_iaddr), bp);
#ifdef DIAGNOSTIC
		if (sup->su_nbytes < sizeof (struct ufs1_dinode)) {
			printf("lfs_vfree: negative byte count"
			       " (segment %" PRIu32 " short by %d)\n",
			       dtosn(fs, old_iaddr),
			       (int)sizeof (struct ufs1_dinode) -
				    sup->su_nbytes);
			panic("lfs_vfree: negative byte count");
			sup->su_nbytes = sizeof (struct ufs1_dinode);
		}
#endif
		sup->su_nbytes -= sizeof (struct ufs1_dinode);
		LFS_WRITESEGENTRY(sup, fs, dtosn(fs, old_iaddr), bp); /* Ifile */
	}

	/* Set superblock modified bit and decrement file count. */
	mutex_enter(&lfs_lock);
	fs->lfs_fmod = 1;
	mutex_exit(&lfs_lock);
	--fs->lfs_nfiles;

	VOP_UNLOCK(fs->lfs_ivnode);
	lfs_segunlock(fs);

	return (0);
}

/*
 * Sort the freelist and set up the free-inode bitmap.
 * To be called by lfs_mountfs().
 */
void
lfs_order_freelist(struct lfs *fs)
{
	CLEANERINFO *cip;
	IFILE *ifp = NULL;
	struct buf *bp;
	ino_t ino, firstino, lastino, maxino;
#ifdef notyet
	struct vnode *vp;
#endif
	
	ASSERT_NO_SEGLOCK(fs);
	lfs_seglock(fs, SEGM_PROT);

	maxino = ((fs->lfs_ivnode->v_size >> fs->lfs_bshift) -
		  fs->lfs_cleansz - fs->lfs_segtabsz) * fs->lfs_ifpb;
	fs->lfs_ino_bitmap = (lfs_bm_t *)
		malloc(((maxino + BMMASK) >> BMSHIFT) * sizeof(lfs_bm_t),
		       M_SEGMENT, M_WAITOK | M_ZERO);
	KASSERT(fs->lfs_ino_bitmap != NULL);

	firstino = lastino = LFS_UNUSED_INUM;
	for (ino = 0; ino < maxino; ino++) {
		if (ino % fs->lfs_ifpb == 0)
			LFS_IENTRY(ifp, fs, ino, bp);
		else
			++ifp;

		/* Don't put zero or ifile on the free list */
		if (ino == LFS_UNUSED_INUM || ino == LFS_IFILE_INUM)
			continue;

#ifdef notyet
		/* Address orphaned files */
		if (ifp->if_nextfree == LFS_ORPHAN_NEXTFREE &&
		    VFS_VGET(fs->lfs_ivnode->v_mount, ino, &vp) == 0) {
			lfs_truncate(vp, 0, 0, NOCRED);
			vput(vp);
			LFS_SEGENTRY(sup, fs, dtosn(fs, ifp->if_daddr), bp);
			KASSERT(sup->su_nbytes >= DINODE1_SIZE);
			sup->su_nbytes -= DINODE1_SIZE;
			LFS_WRITESEGENTRY(sup, fs, dtosn(fs, ifp->if_daddr), bp);

			/* Set up to fall through to next section */
			ifp->if_daddr = LFS_UNUSED_DADDR;
			LFS_BWRITE_LOG(bp);
			LFS_IENTRY(ifp, fs, ino, bp);
		}
#endif

		if (ifp->if_daddr == LFS_UNUSED_DADDR) {
			if (firstino == LFS_UNUSED_INUM)
				firstino = ino;
			else {
				brelse(bp, 0);

				LFS_IENTRY(ifp, fs, lastino, bp);
				ifp->if_nextfree = ino;
				LFS_BWRITE_LOG(bp);
				
				LFS_IENTRY(ifp, fs, ino, bp);
			}
			lastino = ino;

			SET_BITMAP_FREE(fs, ino);
		}

		if ((ino + 1) % fs->lfs_ifpb == 0)
			brelse(bp, 0);
	}

	LFS_PUT_HEADFREE(fs, cip, bp, firstino);
	LFS_PUT_TAILFREE(fs, cip, bp, lastino);

	lfs_segunlock(fs);
}

void
lfs_orphan(struct lfs *fs, ino_t ino)
{
	IFILE *ifp;
	struct buf *bp;

	LFS_IENTRY(ifp, fs, ino, bp);
	ifp->if_nextfree = LFS_ORPHAN_NEXTFREE;
	LFS_BWRITE_LOG(bp);
}
