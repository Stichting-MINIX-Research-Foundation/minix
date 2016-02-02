/*	$NetBSD: lfs_syscalls.c,v 1.170 2015/09/01 06:08:37 dholland Exp $	*/

/*-
 * Copyright (c) 1999, 2000, 2001, 2002, 2003, 2007, 2007, 2008
 *    The NetBSD Foundation, Inc.
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
/*-
 * Copyright (c) 1991, 1993, 1994
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
 *	@(#)lfs_syscalls.c	8.10 (Berkeley) 5/14/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: lfs_syscalls.c,v 1.170 2015/09/01 06:08:37 dholland Exp $");

#ifndef LFS
# define LFS		/* for prototypes in syscallargs.h */
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/kernel.h>
#include <sys/kauth.h>
#include <sys/syscallargs.h>

#include <ufs/lfs/ulfs_inode.h>
#include <ufs/lfs/ulfsmount.h>
#include <ufs/lfs/ulfs_extern.h>

#include <ufs/lfs/lfs.h>
#include <ufs/lfs/lfs_accessors.h>
#include <ufs/lfs/lfs_kernel.h>
#include <ufs/lfs/lfs_extern.h>

static int lfs_fastvget(struct mount *, ino_t, BLOCK_INFO *, int,
    struct vnode **);
static struct buf *lfs_fakebuf(struct lfs *, struct vnode *, daddr_t,
    size_t, void *);

/*
 * sys_lfs_markv:
 *
 * This will mark inodes and blocks dirty, so they are written into the log.
 * It will block until all the blocks have been written.  The segment create
 * time passed in the block_info and inode_info structures is used to decide
 * if the data is valid for each block (in case some process dirtied a block
 * or inode that is being cleaned between the determination that a block is
 * live and the lfs_markv call).
 *
 *  0 on success
 * -1/errno is return on error.
 */
#ifdef USE_64BIT_SYSCALLS
int
sys_lfs_markv(struct lwp *l, const struct sys_lfs_markv_args *uap, register_t *retval)
{
	/* {
		syscallarg(fsid_t *) fsidp;
		syscallarg(struct block_info *) blkiov;
		syscallarg(int) blkcnt;
	} */
	BLOCK_INFO *blkiov;
	int blkcnt, error;
	fsid_t fsid;
	struct lfs *fs;
	struct mount *mntp;

	if ((error = copyin(SCARG(uap, fsidp), &fsid, sizeof(fsid_t))) != 0)
		return (error);

	if ((mntp = vfs_getvfs(fsidp)) == NULL) 
		return (ENOENT);
	fs = VFSTOULFS(mntp)->um_lfs;

	blkcnt = SCARG(uap, blkcnt);
	if ((u_int) blkcnt > LFS_MARKV_MAXBLKCNT)
		return (EINVAL);

	KERNEL_LOCK(1, NULL);
	blkiov = lfs_malloc(fs, blkcnt * sizeof(BLOCK_INFO), LFS_NB_BLKIOV);
	if ((error = copyin(SCARG(uap, blkiov), blkiov,
			    blkcnt * sizeof(BLOCK_INFO))) != 0)
		goto out;

	if ((error = lfs_markv(p, &fsid, blkiov, blkcnt)) == 0)
		copyout(blkiov, SCARG(uap, blkiov),
			blkcnt * sizeof(BLOCK_INFO));
    out:
	lfs_free(fs, blkiov, LFS_NB_BLKIOV);
	KERNEL_UNLOCK_ONE(NULL);
	return error;
}
#else
int
sys_lfs_markv(struct lwp *l, const struct sys_lfs_markv_args *uap, register_t *retval)
{
	/* {
		syscallarg(fsid_t *) fsidp;
		syscallarg(struct block_info *) blkiov;
		syscallarg(int) blkcnt;
	} */
	BLOCK_INFO *blkiov;
	BLOCK_INFO_15 *blkiov15;
	int i, blkcnt, error;
	fsid_t fsid;
	struct lfs *fs;
	struct mount *mntp;

	if ((error = copyin(SCARG(uap, fsidp), &fsid, sizeof(fsid_t))) != 0)
		return (error);

	if ((mntp = vfs_getvfs(&fsid)) == NULL) 
		return (ENOENT);
	fs = VFSTOULFS(mntp)->um_lfs;

	blkcnt = SCARG(uap, blkcnt);
	if ((u_int) blkcnt > LFS_MARKV_MAXBLKCNT)
		return (EINVAL);

	KERNEL_LOCK(1, NULL);
	blkiov = lfs_malloc(fs, blkcnt * sizeof(BLOCK_INFO), LFS_NB_BLKIOV);
	blkiov15 = lfs_malloc(fs, blkcnt * sizeof(BLOCK_INFO_15), LFS_NB_BLKIOV);
	if ((error = copyin(SCARG(uap, blkiov), blkiov15,
			    blkcnt * sizeof(BLOCK_INFO_15))) != 0)
		goto out;

	for (i = 0; i < blkcnt; i++) {
		blkiov[i].bi_inode     = blkiov15[i].bi_inode;
		blkiov[i].bi_lbn       = blkiov15[i].bi_lbn;
		blkiov[i].bi_daddr     = blkiov15[i].bi_daddr;
		blkiov[i].bi_segcreate = blkiov15[i].bi_segcreate;
		blkiov[i].bi_version   = blkiov15[i].bi_version;
		blkiov[i].bi_bp	       = blkiov15[i].bi_bp;
		blkiov[i].bi_size      = blkiov15[i].bi_size;
	}

	if ((error = lfs_markv(l, &fsid, blkiov, blkcnt)) == 0) {
		for (i = 0; i < blkcnt; i++) {
			blkiov15[i].bi_inode	 = blkiov[i].bi_inode;
			blkiov15[i].bi_lbn	 = blkiov[i].bi_lbn;
			blkiov15[i].bi_daddr	 = blkiov[i].bi_daddr;
			blkiov15[i].bi_segcreate = blkiov[i].bi_segcreate;
			blkiov15[i].bi_version	 = blkiov[i].bi_version;
			blkiov15[i].bi_bp	 = blkiov[i].bi_bp;
			blkiov15[i].bi_size	 = blkiov[i].bi_size;
		}
		copyout(blkiov15, SCARG(uap, blkiov),
			blkcnt * sizeof(BLOCK_INFO_15));
	}
    out:
	lfs_free(fs, blkiov, LFS_NB_BLKIOV);
	lfs_free(fs, blkiov15, LFS_NB_BLKIOV);
	KERNEL_UNLOCK_ONE(NULL);
	return error;
}
#endif

#define	LFS_MARKV_MAX_BLOCKS	(LFS_MAX_BUFS)

int
lfs_markv(struct lwp *l, fsid_t *fsidp, BLOCK_INFO *blkiov,
    int blkcnt)
{
	BLOCK_INFO *blkp;
	IFILE *ifp;
	struct buf *bp;
	struct inode *ip = NULL;
	struct lfs *fs;
	struct mount *mntp;
	struct ulfsmount *ump;
	struct vnode *vp;
	ino_t lastino;
	daddr_t b_daddr;
	int cnt, error;
	int do_again = 0;
	int numrefed = 0;
	ino_t maxino;
	size_t obsize;

	/* number of blocks/inodes that we have already bwrite'ed */
	int nblkwritten, ninowritten;

	error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_LFS,
	    KAUTH_REQ_SYSTEM_LFS_MARKV, NULL, NULL, NULL);
	if (error)
		return (error);

	if ((mntp = vfs_getvfs(fsidp)) == NULL)
		return (ENOENT);

	ump = VFSTOULFS(mntp);
	fs = ump->um_lfs;

	if (fs->lfs_ronly)
		return EROFS;

	maxino = (lfs_fragstoblks(fs, lfs_dino_getblocks(fs, VTOI(fs->lfs_ivnode)->i_din)) -
		      lfs_sb_getcleansz(fs) - lfs_sb_getsegtabsz(fs)) * lfs_sb_getifpb(fs);

	cnt = blkcnt;

	if ((error = vfs_busy(mntp, NULL)) != 0)
		return (error);

	/*
	 * This seglock is just to prevent the fact that we might have to sleep
	 * from allowing the possibility that our blocks might become
	 * invalid.
	 *
	 * It is also important to note here that unless we specify SEGM_CKP,
	 * any Ifile blocks that we might be asked to clean will never get
	 * to the disk.
	 */
	lfs_seglock(fs, SEGM_CLEAN | SEGM_CKP | SEGM_SYNC);

	/* Mark blocks/inodes dirty.  */
	error = 0;

	/* these were inside the initialization for the for loop */
	vp = NULL;
	lastino = LFS_UNUSED_INUM;
	nblkwritten = ninowritten = 0;
	for (blkp = blkiov; cnt--; ++blkp)
	{
		/* Bounds-check incoming data, avoid panic for failed VGET */
		if (blkp->bi_inode <= 0 || blkp->bi_inode >= maxino) {
			error = EINVAL;
			goto err3;
		}
		/*
		 * Get the IFILE entry (only once) and see if the file still
		 * exists.
		 */
		if (lastino != blkp->bi_inode) {
			/*
			 * Finish the old file, if there was one.
			 */
			if (vp != NULL) {
				vput(vp);
				vp = NULL;
				numrefed--;
			}

			/*
			 * Start a new file
			 */
			lastino = blkp->bi_inode;

			/* Get the vnode/inode. */
			error = lfs_fastvget(mntp, blkp->bi_inode, blkp,
			    LK_EXCLUSIVE | LK_NOWAIT, &vp);
			if (error) {
				DLOG((DLOG_CLEAN, "lfs_markv: lfs_fastvget"
				      " failed with %d (ino %d, segment %d)\n",
				      error, blkp->bi_inode,
				      lfs_dtosn(fs, blkp->bi_daddr)));
				/*
				 * If we got EAGAIN, that means that the
				 * Inode was locked.  This is
				 * recoverable: just clean the rest of
				 * this segment, and let the cleaner try
				 * again with another.	(When the
				 * cleaner runs again, this segment will
				 * sort high on the list, since it is
				 * now almost entirely empty.)
				 */
				if (error == EAGAIN) {
					error = 0;
					do_again++;
				} else
					KASSERT(error == ENOENT);
				KASSERT(vp == NULL);
				ip = NULL;
				continue;
			}

			ip = VTOI(vp);
			numrefed++;
			ninowritten++;
		} else if (vp == NULL) {
			/*
			 * This can only happen if the vnode is dead (or
			 * in any case we can't get it...e.g., it is
			 * inlocked).  Keep going.
			 */
			continue;
		}

		/* Past this point we are guaranteed that vp, ip are valid. */

		/* Can't clean VU_DIROP directories in case of truncation */
		/* XXX - maybe we should mark removed dirs specially? */
		if (vp->v_type == VDIR && (vp->v_uflag & VU_DIROP)) {
			do_again++;
			continue;
		}

		/* If this BLOCK_INFO didn't contain a block, keep going. */
		if (blkp->bi_lbn == LFS_UNUSED_LBN) {
			/* XXX need to make sure that the inode gets written in this case */
			/* XXX but only write the inode if it's the right one */
			if (blkp->bi_inode != LFS_IFILE_INUM) {
				LFS_IENTRY(ifp, fs, blkp->bi_inode, bp);
				if (lfs_if_getdaddr(fs, ifp) == blkp->bi_daddr) {
					mutex_enter(&lfs_lock);
					LFS_SET_UINO(ip, IN_CLEANING);
					mutex_exit(&lfs_lock);
				}
				brelse(bp, 0);
			}
			continue;
		}

		b_daddr = 0;
		if (VOP_BMAP(vp, blkp->bi_lbn, NULL, &b_daddr, NULL) ||
		    LFS_DBTOFSB(fs, b_daddr) != blkp->bi_daddr)
		{
			if (lfs_dtosn(fs, LFS_DBTOFSB(fs, b_daddr)) ==
			    lfs_dtosn(fs, blkp->bi_daddr))
			{
				DLOG((DLOG_CLEAN, "lfs_markv: wrong da same seg: %jx vs %jx\n",
				      (intmax_t)blkp->bi_daddr, (intmax_t)LFS_DBTOFSB(fs, b_daddr)));
			}
			do_again++;
			continue;
		}

		/*
		 * Check block sizes.  The blocks being cleaned come from
		 * disk, so they should have the same size as their on-disk
		 * counterparts.
		 */
		if (blkp->bi_lbn >= 0)
			obsize = lfs_blksize(fs, ip, blkp->bi_lbn);
		else
			obsize = lfs_sb_getbsize(fs);
		/* Check for fragment size change */
		if (blkp->bi_lbn >= 0 && blkp->bi_lbn < ULFS_NDADDR) {
			obsize = ip->i_lfs_fragsize[blkp->bi_lbn];
		}
		if (obsize != blkp->bi_size) {
			DLOG((DLOG_CLEAN, "lfs_markv: ino %d lbn %jd wrong"
			      " size (%ld != %d), try again\n",
			      blkp->bi_inode, (intmax_t)blkp->bi_lbn,
			      (long) obsize, blkp->bi_size));
			do_again++;
			continue;
		}

		/*
		 * If we get to here, then we are keeping the block.  If
		 * it is an indirect block, we want to actually put it
		 * in the buffer cache so that it can be updated in the
		 * finish_meta section.	 If it's not, we need to
		 * allocate a fake buffer so that writeseg can perform
		 * the copyin and write the buffer.
		 */
		if (ip->i_number != LFS_IFILE_INUM && blkp->bi_lbn >= 0) {
			/* Data Block */
			bp = lfs_fakebuf(fs, vp, blkp->bi_lbn,
					 blkp->bi_size, blkp->bi_bp);
			/* Pretend we used bread() to get it */
			bp->b_blkno = LFS_FSBTODB(fs, blkp->bi_daddr);
		} else {
			/* Indirect block or ifile */
			if (blkp->bi_size != lfs_sb_getbsize(fs) &&
			    ip->i_number != LFS_IFILE_INUM)
				panic("lfs_markv: partial indirect block?"
				    " size=%d\n", blkp->bi_size);
			bp = getblk(vp, blkp->bi_lbn, blkp->bi_size, 0, 0);
			if (!(bp->b_oflags & (BO_DONE|BO_DELWRI))) {
				/*
				 * The block in question was not found
				 * in the cache; i.e., the block that
				 * getblk() returned is empty.	So, we
				 * can (and should) copy in the
				 * contents, because we've already
				 * determined that this was the right
				 * version of this block on disk.
				 *
				 * And, it can't have changed underneath
				 * us, because we have the segment lock.
				 */
				error = copyin(blkp->bi_bp, bp->b_data, blkp->bi_size);
				if (error)
					goto err2;
			}
		}
		if ((error = lfs_bwrite_ext(bp, BW_CLEAN)) != 0)
			goto err2;

		nblkwritten++;
		/*
		 * XXX should account indirect blocks and ifile pages as well
		 */
		if (nblkwritten + lfs_lblkno(fs, ninowritten * DINOSIZE(fs))
		    > LFS_MARKV_MAX_BLOCKS) {
			DLOG((DLOG_CLEAN, "lfs_markv: writing %d blks %d inos\n",
			      nblkwritten, ninowritten));
			lfs_segwrite(mntp, SEGM_CLEAN);
			nblkwritten = ninowritten = 0;
		}
	}

	/*
	 * Finish the old file, if there was one
	 */
	if (vp != NULL) {
		vput(vp);
		vp = NULL;
		numrefed--;
	}

#ifdef DIAGNOSTIC
	if (numrefed != 0)
		panic("lfs_markv: numrefed=%d", numrefed);
#endif
	DLOG((DLOG_CLEAN, "lfs_markv: writing %d blks %d inos (check point)\n",
	      nblkwritten, ninowritten));

	/*
	 * The last write has to be SEGM_SYNC, because of calling semantics.
	 * It also has to be SEGM_CKP, because otherwise we could write
	 * over the newly cleaned data contained in a checkpoint, and then
	 * we'd be unhappy at recovery time.
	 */
	lfs_segwrite(mntp, SEGM_CLEAN | SEGM_CKP | SEGM_SYNC);

	lfs_segunlock(fs);

	vfs_unbusy(mntp, false, NULL);
	if (error)
		return (error);
	else if (do_again)
		return EAGAIN;

	return 0;

err2:
	DLOG((DLOG_CLEAN, "lfs_markv err2\n"));

	/*
	 * XXX we're here because copyin() failed.
	 * XXX it means that we can't trust the cleanerd.  too bad.
	 * XXX how can we recover from this?
	 */

err3:
	/*
	 * XXX should do segwrite here anyway?
	 */

	if (vp != NULL) {
		vput(vp);
		vp = NULL;
		--numrefed;
	}

	lfs_segunlock(fs);
	vfs_unbusy(mntp, false, NULL);
#ifdef DIAGNOSTIC
	if (numrefed != 0)
		panic("lfs_markv: numrefed=%d", numrefed);
#endif

	return (error);
}

/*
 * sys_lfs_bmapv:
 *
 * This will fill in the current disk address for arrays of blocks.
 *
 *  0 on success
 * -1/errno is return on error.
 */
#ifdef USE_64BIT_SYSCALLS
int
sys_lfs_bmapv(struct lwp *l, const struct sys_lfs_bmapv_args *uap, register_t *retval)
{
	/* {
		syscallarg(fsid_t *) fsidp;
		syscallarg(struct block_info *) blkiov;
		syscallarg(int) blkcnt;
	} */
	BLOCK_INFO *blkiov;
	int blkcnt, error;
	fsid_t fsid;
	struct lfs *fs;
	struct mount *mntp;

	if ((error = copyin(SCARG(uap, fsidp), &fsid, sizeof(fsid_t))) != 0)
		return (error);

	if ((mntp = vfs_getvfs(&fsid)) == NULL) 
		return (ENOENT);
	fs = VFSTOULFS(mntp)->um_lfs;

	blkcnt = SCARG(uap, blkcnt);
	if ((u_int) blkcnt > SIZE_T_MAX / sizeof(BLOCK_INFO))
		return (EINVAL);
	KERNEL_LOCK(1, NULL);
	blkiov = lfs_malloc(fs, blkcnt * sizeof(BLOCK_INFO), LFS_NB_BLKIOV);
	if ((error = copyin(SCARG(uap, blkiov), blkiov,
			    blkcnt * sizeof(BLOCK_INFO))) != 0)
		goto out;

	if ((error = lfs_bmapv(p, &fsid, blkiov, blkcnt)) == 0)
		copyout(blkiov, SCARG(uap, blkiov),
			blkcnt * sizeof(BLOCK_INFO));
    out:
	lfs_free(fs, blkiov, LFS_NB_BLKIOV);
	KERNEL_UNLOCK_ONE(NULL);
	return error;
}
#else
int
sys_lfs_bmapv(struct lwp *l, const struct sys_lfs_bmapv_args *uap, register_t *retval)
{
	/* {
		syscallarg(fsid_t *) fsidp;
		syscallarg(struct block_info *) blkiov;
		syscallarg(int) blkcnt;
	} */
	BLOCK_INFO *blkiov;
	BLOCK_INFO_15 *blkiov15;
	int i, blkcnt, error;
	fsid_t fsid;
	struct lfs *fs;
	struct mount *mntp;

	if ((error = copyin(SCARG(uap, fsidp), &fsid, sizeof(fsid_t))) != 0)
		return (error);

	if ((mntp = vfs_getvfs(&fsid)) == NULL) 
		return (ENOENT);
	fs = VFSTOULFS(mntp)->um_lfs;

	blkcnt = SCARG(uap, blkcnt);
	if ((size_t) blkcnt > SIZE_T_MAX / sizeof(BLOCK_INFO))
		return (EINVAL);
	KERNEL_LOCK(1, NULL);
	blkiov = lfs_malloc(fs, blkcnt * sizeof(BLOCK_INFO), LFS_NB_BLKIOV);
	blkiov15 = lfs_malloc(fs, blkcnt * sizeof(BLOCK_INFO_15), LFS_NB_BLKIOV);
	if ((error = copyin(SCARG(uap, blkiov), blkiov15,
			    blkcnt * sizeof(BLOCK_INFO_15))) != 0)
		goto out;

	for (i = 0; i < blkcnt; i++) {
		blkiov[i].bi_inode     = blkiov15[i].bi_inode;
		blkiov[i].bi_lbn       = blkiov15[i].bi_lbn;
		blkiov[i].bi_daddr     = blkiov15[i].bi_daddr;
		blkiov[i].bi_segcreate = blkiov15[i].bi_segcreate;
		blkiov[i].bi_version   = blkiov15[i].bi_version;
		blkiov[i].bi_bp	       = blkiov15[i].bi_bp;
		blkiov[i].bi_size      = blkiov15[i].bi_size;
	}

	if ((error = lfs_bmapv(l, &fsid, blkiov, blkcnt)) == 0) {
		for (i = 0; i < blkcnt; i++) {
			blkiov15[i].bi_inode	 = blkiov[i].bi_inode;
			blkiov15[i].bi_lbn	 = blkiov[i].bi_lbn;
			blkiov15[i].bi_daddr	 = blkiov[i].bi_daddr;
			blkiov15[i].bi_segcreate = blkiov[i].bi_segcreate;
			blkiov15[i].bi_version	 = blkiov[i].bi_version;
			blkiov15[i].bi_bp	 = blkiov[i].bi_bp;
			blkiov15[i].bi_size	 = blkiov[i].bi_size;
		}
		copyout(blkiov15, SCARG(uap, blkiov),
			blkcnt * sizeof(BLOCK_INFO_15));
	}
    out:
	lfs_free(fs, blkiov, LFS_NB_BLKIOV);
	lfs_free(fs, blkiov15, LFS_NB_BLKIOV);
	KERNEL_UNLOCK_ONE(NULL);
	return error;
}
#endif

int
lfs_bmapv(struct lwp *l, fsid_t *fsidp, BLOCK_INFO *blkiov, int blkcnt)
{
	BLOCK_INFO *blkp;
	IFILE *ifp;
	struct buf *bp;
	struct inode *ip = NULL;
	struct lfs *fs;
	struct mount *mntp;
	struct ulfsmount *ump;
	struct vnode *vp;
	ino_t lastino;
	daddr_t v_daddr;
	int cnt, error;
	int numrefed = 0;

	error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_LFS,
	    KAUTH_REQ_SYSTEM_LFS_BMAPV, NULL, NULL, NULL);
	if (error)
		return (error);

	if ((mntp = vfs_getvfs(fsidp)) == NULL)
		return (ENOENT);

	ump = VFSTOULFS(mntp);
	if ((error = vfs_busy(mntp, NULL)) != 0)
		return (error);

	if (ump->um_cleaner_thread == NULL)
		ump->um_cleaner_thread = curlwp;
	KASSERT(ump->um_cleaner_thread == curlwp);

	cnt = blkcnt;

	fs = VFSTOULFS(mntp)->um_lfs;

	error = 0;

	/* these were inside the initialization for the for loop */
	vp = NULL;
	v_daddr = LFS_UNUSED_DADDR;
	lastino = LFS_UNUSED_INUM;
	for (blkp = blkiov; cnt--; ++blkp)
	{
		/*
		 * Get the IFILE entry (only once) and see if the file still
		 * exists.
		 */
		if (lastino != blkp->bi_inode) {
			/*
			 * Finish the old file, if there was one.
			 */
			if (vp != NULL) {
				vput(vp);
				vp = NULL;
				numrefed--;
			}

			/*
			 * Start a new file
			 */
			lastino = blkp->bi_inode;
			if (blkp->bi_inode == LFS_IFILE_INUM)
				v_daddr = lfs_sb_getidaddr(fs);
			else {
				LFS_IENTRY(ifp, fs, blkp->bi_inode, bp);
				v_daddr = lfs_if_getdaddr(fs, ifp);
				brelse(bp, 0);
			}
			if (v_daddr == LFS_UNUSED_DADDR) {
				blkp->bi_daddr = LFS_UNUSED_DADDR;
				continue;
			}
			error = lfs_fastvget(mntp, blkp->bi_inode, NULL,
			    LK_SHARED, &vp);
			if (error) {
				DLOG((DLOG_CLEAN, "lfs_bmapv: lfs_fastvget ino"
				      "%d failed with %d",
				      blkp->bi_inode,error));
				KASSERT(vp == NULL);
				continue;
			} else {
				KASSERT(VOP_ISLOCKED(vp));
				numrefed++;
			}
			ip = VTOI(vp);
		} else if (vp == NULL) {
			/*
			 * This can only happen if the vnode is dead.
			 * Keep going.	Note that we DO NOT set the
			 * bi_addr to anything -- if we failed to get
			 * the vnode, for example, we want to assume
			 * conservatively that all of its blocks *are*
			 * located in the segment in question.
			 * lfs_markv will throw them out if we are
			 * wrong.
			 */
			continue;
		}

		/* Past this point we are guaranteed that vp, ip are valid. */

		if (blkp->bi_lbn == LFS_UNUSED_LBN) {
			/*
			 * We just want the inode address, which is
			 * conveniently in v_daddr.
			 */
			blkp->bi_daddr = v_daddr;
		} else {
			daddr_t bi_daddr;

			error = VOP_BMAP(vp, blkp->bi_lbn, NULL,
					 &bi_daddr, NULL);
			if (error)
			{
				blkp->bi_daddr = LFS_UNUSED_DADDR;
				continue;
			}
			blkp->bi_daddr = LFS_DBTOFSB(fs, bi_daddr);
			/* Fill in the block size, too */
			if (blkp->bi_lbn >= 0)
				blkp->bi_size = lfs_blksize(fs, ip, blkp->bi_lbn);
			else
				blkp->bi_size = lfs_sb_getbsize(fs);
		}
	}

	/*
	 * Finish the old file, if there was one.
	 */
	if (vp != NULL) {
		vput(vp);
		vp = NULL;
		numrefed--;
	}

#ifdef DIAGNOSTIC
	if (numrefed != 0)
		panic("lfs_bmapv: numrefed=%d", numrefed);
#endif

	vfs_unbusy(mntp, false, NULL);

	return 0;
}

/*
 * sys_lfs_segclean:
 *
 * Mark the segment clean.
 *
 *  0 on success
 * -1/errno is return on error.
 */
int
sys_lfs_segclean(struct lwp *l, const struct sys_lfs_segclean_args *uap, register_t *retval)
{
	/* {
		syscallarg(fsid_t *) fsidp;
		syscallarg(u_long) segment;
	} */
	struct lfs *fs;
	struct mount *mntp;
	fsid_t fsid;
	int error;
	unsigned long segnum;

	error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_LFS,
	    KAUTH_REQ_SYSTEM_LFS_SEGCLEAN, NULL, NULL, NULL);
	if (error)
		return (error);

	if ((error = copyin(SCARG(uap, fsidp), &fsid, sizeof(fsid_t))) != 0)
		return (error);
	if ((mntp = vfs_getvfs(&fsid)) == NULL)
		return (ENOENT);

	fs = VFSTOULFS(mntp)->um_lfs;
	segnum = SCARG(uap, segment);

	if ((error = vfs_busy(mntp, NULL)) != 0)
		return (error);

	KERNEL_LOCK(1, NULL);
	lfs_seglock(fs, SEGM_PROT);
	error = lfs_do_segclean(fs, segnum);
	lfs_segunlock(fs);
	KERNEL_UNLOCK_ONE(NULL);
	vfs_unbusy(mntp, false, NULL);
	return error;
}

/*
 * Actually mark the segment clean.
 * Must be called with the segment lock held.
 */
int
lfs_do_segclean(struct lfs *fs, unsigned long segnum)
{
	extern int lfs_dostats;
	struct buf *bp;
	CLEANERINFO *cip;
	SEGUSE *sup;

	if (lfs_dtosn(fs, lfs_sb_getcurseg(fs)) == segnum) {
		return (EBUSY);
	}

	LFS_SEGENTRY(sup, fs, segnum, bp);
	if (sup->su_nbytes) {
		DLOG((DLOG_CLEAN, "lfs_segclean: not cleaning segment %lu:"
		      " %d live bytes\n", segnum, sup->su_nbytes));
		brelse(bp, 0);
		return (EBUSY);
	}
	if (sup->su_flags & SEGUSE_ACTIVE) {
		DLOG((DLOG_CLEAN, "lfs_segclean: not cleaning segment %lu:"
		      " segment is active\n", segnum));
		brelse(bp, 0);
		return (EBUSY);
	}
	if (!(sup->su_flags & SEGUSE_DIRTY)) {
		DLOG((DLOG_CLEAN, "lfs_segclean: not cleaning segment %lu:"
		      " segment is already clean\n", segnum));
		brelse(bp, 0);
		return (EALREADY);
	}

	lfs_sb_addavail(fs, lfs_segtod(fs, 1));
	if (sup->su_flags & SEGUSE_SUPERBLOCK)
		lfs_sb_subavail(fs, lfs_btofsb(fs, LFS_SBPAD));
	if (lfs_sb_getversion(fs) > 1 && segnum == 0 &&
	    lfs_sb_gets0addr(fs) < lfs_btofsb(fs, LFS_LABELPAD))
		lfs_sb_subavail(fs, lfs_btofsb(fs, LFS_LABELPAD) - lfs_sb_gets0addr(fs));
	mutex_enter(&lfs_lock);
	lfs_sb_addbfree(fs, sup->su_nsums * lfs_btofsb(fs, lfs_sb_getsumsize(fs)) +
		lfs_btofsb(fs, sup->su_ninos * lfs_sb_getibsize(fs)));
	lfs_sb_subdmeta(fs, sup->su_nsums * lfs_btofsb(fs, lfs_sb_getsumsize(fs)) +
		lfs_btofsb(fs, sup->su_ninos * lfs_sb_getibsize(fs)));
	if (lfs_sb_getdmeta(fs) < 0)
		lfs_sb_setdmeta(fs, 0);
	mutex_exit(&lfs_lock);
	sup->su_flags &= ~SEGUSE_DIRTY;
	LFS_WRITESEGENTRY(sup, fs, segnum, bp);

	LFS_CLEANERINFO(cip, fs, bp);
	lfs_ci_shiftdirtytoclean(fs, cip, 1);
	lfs_sb_setnclean(fs, lfs_ci_getclean(fs, cip));
	mutex_enter(&lfs_lock);
	lfs_ci_setbfree(fs, cip, lfs_sb_getbfree(fs));
	lfs_ci_setavail(fs, cip, lfs_sb_getavail(fs)
			- fs->lfs_ravail - fs->lfs_favail);
	wakeup(&fs->lfs_availsleep);
	mutex_exit(&lfs_lock);
	(void) LFS_BWRITE_LOG(bp);

	if (lfs_dostats)
		++lfs_stats.segs_reclaimed;

	return (0);
}

/*
 * This will block until a segment in file system fsid is written.  A timeout
 * in milliseconds may be specified which will awake the cleaner automatically.
 * An fsid of -1 means any file system, and a timeout of 0 means forever.
 */
int
lfs_segwait(fsid_t *fsidp, struct timeval *tv)
{
	struct mount *mntp;
	void *addr;
	u_long timeout;
	int error;

	KERNEL_LOCK(1, NULL);
	if (fsidp == NULL || (mntp = vfs_getvfs(fsidp)) == NULL)
		addr = &lfs_allclean_wakeup;
	else
		addr = &VFSTOULFS(mntp)->um_lfs->lfs_nextsegsleep;
	/*
	 * XXX THIS COULD SLEEP FOREVER IF TIMEOUT IS {0,0}!
	 * XXX IS THAT WHAT IS INTENDED?
	 */
	timeout = tvtohz(tv);
	error = tsleep(addr, PCATCH | PVFS, "segment", timeout);
	KERNEL_UNLOCK_ONE(NULL);
	return (error == ERESTART ? EINTR : 0);
}

/*
 * sys_lfs_segwait:
 *
 * System call wrapper around lfs_segwait().
 *
 *  0 on success
 *  1 on timeout
 * -1/errno is return on error.
 */
int
sys___lfs_segwait50(struct lwp *l, const struct sys___lfs_segwait50_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(fsid_t *) fsidp;
		syscallarg(struct timeval *) tv;
	} */
	struct timeval atv;
	fsid_t fsid;
	int error;

	/* XXX need we be su to segwait? */
	error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_LFS,
	    KAUTH_REQ_SYSTEM_LFS_SEGWAIT, NULL, NULL, NULL);
	if (error)
		return (error);
	if ((error = copyin(SCARG(uap, fsidp), &fsid, sizeof(fsid_t))) != 0)
		return (error);

	if (SCARG(uap, tv)) {
		error = copyin(SCARG(uap, tv), &atv, sizeof(struct timeval));
		if (error)
			return (error);
		if (itimerfix(&atv))
			return (EINVAL);
	} else /* NULL or invalid */
		atv.tv_sec = atv.tv_usec = 0;
	return lfs_segwait(&fsid, &atv);
}

/*
 * VFS_VGET call specialized for the cleaner.  If the cleaner is
 * processing IINFO structures, it may have the ondisk inode already, so
 * don't go retrieving it again.
 *
 * Return the vnode referenced and locked.
 */

static int
lfs_fastvget(struct mount *mp, ino_t ino, BLOCK_INFO *blkp, int lk_flags,
    struct vnode **vpp)
{
	struct ulfsmount *ump;
	int error;

	ump = VFSTOULFS(mp);
	ump->um_cleaner_hint = blkp;
	error = vcache_get(mp, &ino, sizeof(ino), vpp);
	ump->um_cleaner_hint = NULL;
	if (error)
		return error;
	error = vn_lock(*vpp, lk_flags);
	if (error) {
		if (error == EBUSY)
			error = EAGAIN;
		vrele(*vpp);
		*vpp = NULL;
		return error;
	}

	return 0;
}

/*
 * Make up a "fake" cleaner buffer, copy the data from userland into it.
 */
static struct buf *
lfs_fakebuf(struct lfs *fs, struct vnode *vp, daddr_t lbn, size_t size, void *uaddr)
{
	struct buf *bp;
	int error;

	KASSERT(VTOI(vp)->i_number != LFS_IFILE_INUM);

	bp = lfs_newbuf(VTOI(vp)->i_lfs, vp, lbn, size, LFS_NB_CLEAN);
	error = copyin(uaddr, bp->b_data, size);
	if (error) {
		lfs_freebuf(fs, bp);
		return NULL;
	}
	KDASSERT(bp->b_iodone == lfs_callback);

#if 0
	mutex_enter(&lfs_lock);
	++fs->lfs_iocount;
	mutex_exit(&lfs_lock);
#endif
	bp->b_bufsize = size;
	bp->b_bcount = size;
	return (bp);
}
