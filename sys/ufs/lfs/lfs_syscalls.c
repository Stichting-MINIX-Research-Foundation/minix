/*	$NetBSD: lfs_syscalls.c,v 1.139 2011/06/12 03:36:01 rmind Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: lfs_syscalls.c,v 1.139 2011/06/12 03:36:01 rmind Exp $");

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

#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/lfs/lfs.h>
#include <ufs/lfs/lfs_extern.h>

struct buf *lfs_fakebuf(struct lfs *, struct vnode *, int, size_t, void *);
int lfs_fasthashget(dev_t, ino_t, struct vnode **);

pid_t lfs_cleaner_pid = 0;

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

	if ((error = kauth_authorize_generic(l->l_cred, KAUTH_GENERIC_ISSUSER,
	    NULL)) != 0)
		return (error);

	if ((error = copyin(SCARG(uap, fsidp), &fsid, sizeof(fsid_t))) != 0)
		return (error);

	if ((mntp = vfs_getvfs(fsidp)) == NULL) 
		return (ENOENT);
	fs = VFSTOUFS(mntp)->um_lfs;

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

	if ((error = kauth_authorize_generic(l->l_cred, KAUTH_GENERIC_ISSUSER,
	    NULL)) != 0)
		return (error);

	if ((error = copyin(SCARG(uap, fsidp), &fsid, sizeof(fsid_t))) != 0)
		return (error);

	if ((mntp = vfs_getvfs(&fsid)) == NULL) 
		return (ENOENT);
	fs = VFSTOUFS(mntp)->um_lfs;

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

	if ((error = lfs_markv(l->l_proc, &fsid, blkiov, blkcnt)) == 0) {
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
lfs_markv(struct proc *p, fsid_t *fsidp, BLOCK_INFO *blkiov,
    int blkcnt)
{
	BLOCK_INFO *blkp;
	IFILE *ifp;
	struct buf *bp;
	struct inode *ip = NULL;
	struct lfs *fs;
	struct mount *mntp;
	struct vnode *vp = NULL;
	ino_t lastino;
	daddr_t b_daddr, v_daddr;
	int cnt, error;
	int do_again = 0;
	int numrefed = 0;
	ino_t maxino;
	size_t obsize;

	/* number of blocks/inodes that we have already bwrite'ed */
	int nblkwritten, ninowritten;

	if ((mntp = vfs_getvfs(fsidp)) == NULL)
		return (ENOENT);

	fs = VFSTOUFS(mntp)->um_lfs;

	if (fs->lfs_ronly)
		return EROFS;

	maxino = (fragstoblks(fs, VTOI(fs->lfs_ivnode)->i_ffs1_blocks) -
		      fs->lfs_cleansz - fs->lfs_segtabsz) * fs->lfs_ifpb;

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
	v_daddr = LFS_UNUSED_DADDR;
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
			 * Finish the old file, if there was one.  The presence
			 * of a usable vnode in vp is signaled by a valid v_daddr.
			 */
			if (v_daddr != LFS_UNUSED_DADDR) {
				lfs_vunref(vp);
				numrefed--;
			}

			/*
			 * Start a new file
			 */
			lastino = blkp->bi_inode;
			if (blkp->bi_inode == LFS_IFILE_INUM)
				v_daddr = fs->lfs_idaddr;
			else {
				LFS_IENTRY(ifp, fs, blkp->bi_inode, bp);
				/* XXX fix for force write */
				v_daddr = ifp->if_daddr;
				brelse(bp, 0);
			}
			if (v_daddr == LFS_UNUSED_DADDR)
				continue;

			/* Get the vnode/inode. */
			error = lfs_fastvget(mntp, blkp->bi_inode, v_daddr,
					   &vp,
					   (blkp->bi_lbn == LFS_UNUSED_LBN
					    ? blkp->bi_bp
					    : NULL));

			if (!error) {
				numrefed++;
			}
			if (error) {
				DLOG((DLOG_CLEAN, "lfs_markv: lfs_fastvget"
				      " failed with %d (ino %d, segment %d)\n",
				      error, blkp->bi_inode,
				      dtosn(fs, blkp->bi_daddr)));
				/*
				 * If we got EAGAIN, that means that the
				 * Inode was locked.  This is
				 * recoverable: just clean the rest of
				 * this segment, and let the cleaner try
				 * again with another.	(When the
				 * cleaner runs again, this segment will
				 * sort high on the list, since it is
				 * now almost entirely empty.) But, we
				 * still set v_daddr = LFS_UNUSED_ADDR
				 * so as not to test this over and over
				 * again.
				 */
				if (error == EAGAIN) {
					error = 0;
					do_again++;
				}
#ifdef DIAGNOSTIC
				else if (error != ENOENT)
					panic("lfs_markv VFS_VGET FAILED");
#endif
				/* lastino = LFS_UNUSED_INUM; */
				v_daddr = LFS_UNUSED_DADDR;
				vp = NULL;
				ip = NULL;
				continue;
			}
			ip = VTOI(vp);
			ninowritten++;
		} else if (v_daddr == LFS_UNUSED_DADDR) {
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
				if (ifp->if_daddr == blkp->bi_daddr) {
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
		    dbtofsb(fs, b_daddr) != blkp->bi_daddr)
		{
			if (dtosn(fs, dbtofsb(fs, b_daddr)) ==
			    dtosn(fs, blkp->bi_daddr))
			{
				DLOG((DLOG_CLEAN, "lfs_markv: wrong da same seg: %llx vs %llx\n",
				      (long long)blkp->bi_daddr, (long long)dbtofsb(fs, b_daddr)));
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
			obsize = blksize(fs, ip, blkp->bi_lbn);
		else
			obsize = fs->lfs_bsize;
		/* Check for fragment size change */
		if (blkp->bi_lbn >= 0 && blkp->bi_lbn < NDADDR) {
			obsize = ip->i_lfs_fragsize[blkp->bi_lbn];
		}
		if (obsize != blkp->bi_size) {
			DLOG((DLOG_CLEAN, "lfs_markv: ino %d lbn %lld wrong"
			      " size (%ld != %d), try again\n",
			      blkp->bi_inode, (long long)blkp->bi_lbn,
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
			bp->b_blkno = fsbtodb(fs, blkp->bi_daddr);
		} else {
			/* Indirect block or ifile */
			if (blkp->bi_size != fs->lfs_bsize &&
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
		if (nblkwritten + lblkno(fs, ninowritten * sizeof (struct ufs1_dinode))
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
	if (v_daddr != LFS_UNUSED_DADDR) {
		lfs_vunref(vp);
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
	KERNEL_UNLOCK_ONE(NULL);
	/*
	 * XXX should do segwrite here anyway?
	 */

	if (v_daddr != LFS_UNUSED_DADDR) {
		lfs_vunref(vp);
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

	if ((error = kauth_authorize_generic(l->l_cred, KAUTH_GENERIC_ISSUSER,
	    NULL)) != 0)
		return (error);

	if ((error = copyin(SCARG(uap, fsidp), &fsid, sizeof(fsid_t))) != 0)
		return (error);

	if ((mntp = vfs_getvfs(&fsid)) == NULL) 
		return (ENOENT);
	fs = VFSTOUFS(mntp)->um_lfs;

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

	if ((error = kauth_authorize_generic(l->l_cred, KAUTH_GENERIC_ISSUSER,
	    NULL)) != 0)
		return (error);

	if ((error = copyin(SCARG(uap, fsidp), &fsid, sizeof(fsid_t))) != 0)
		return (error);

	if ((mntp = vfs_getvfs(&fsid)) == NULL) 
		return (ENOENT);
	fs = VFSTOUFS(mntp)->um_lfs;

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

	if ((error = lfs_bmapv(l->l_proc, &fsid, blkiov, blkcnt)) == 0) {
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
lfs_bmapv(struct proc *p, fsid_t *fsidp, BLOCK_INFO *blkiov, int blkcnt)
{
	BLOCK_INFO *blkp;
	IFILE *ifp;
	struct buf *bp;
	struct inode *ip = NULL;
	struct lfs *fs;
	struct mount *mntp;
	struct ufsmount *ump;
	struct vnode *vp;
	ino_t lastino;
	daddr_t v_daddr;
	int cnt, error;
	int numrefed = 0;

	lfs_cleaner_pid = p->p_pid;

	if ((mntp = vfs_getvfs(fsidp)) == NULL)
		return (ENOENT);

	ump = VFSTOUFS(mntp);
	if ((error = vfs_busy(mntp, NULL)) != 0)
		return (error);

	cnt = blkcnt;

	fs = VFSTOUFS(mntp)->um_lfs;

	error = 0;

	/* these were inside the initialization for the for loop */
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
			 * Finish the old file, if there was one.  The presence
			 * of a usable vnode in vp is signaled by a valid
			 * v_daddr.
			 */
			if (v_daddr != LFS_UNUSED_DADDR) {
				lfs_vunref(vp);
				numrefed--;
			}

			/*
			 * Start a new file
			 */
			lastino = blkp->bi_inode;
			if (blkp->bi_inode == LFS_IFILE_INUM)
				v_daddr = fs->lfs_idaddr;
			else {
				LFS_IENTRY(ifp, fs, blkp->bi_inode, bp);
				v_daddr = ifp->if_daddr;
				brelse(bp, 0);
			}
			if (v_daddr == LFS_UNUSED_DADDR) {
				blkp->bi_daddr = LFS_UNUSED_DADDR;
				continue;
			}
			/*
			 * A regular call to VFS_VGET could deadlock
			 * here.  Instead, we try an unlocked access.
			 */
			mutex_enter(&ufs_ihash_lock);
			vp = ufs_ihashlookup(ump->um_dev, blkp->bi_inode);
			if (vp != NULL && !(vp->v_iflag & VI_XLOCK)) {
				ip = VTOI(vp);
				mutex_enter(vp->v_interlock);
				mutex_exit(&ufs_ihash_lock);
				if (lfs_vref(vp)) {
					v_daddr = LFS_UNUSED_DADDR;
					continue;
				}
				numrefed++;
			} else {
				mutex_exit(&ufs_ihash_lock);
				/*
				 * Don't VFS_VGET if we're being unmounted,
				 * since we hold vfs_busy().
				 */
				if (mntp->mnt_iflag & IMNT_UNMOUNT) {
					v_daddr = LFS_UNUSED_DADDR;
					continue;
				}
				error = VFS_VGET(mntp, blkp->bi_inode, &vp);
				if (error) {
					DLOG((DLOG_CLEAN, "lfs_bmapv: vget ino"
					      "%d failed with %d",
					      blkp->bi_inode,error));
					v_daddr = LFS_UNUSED_DADDR;
					continue;
				} else {
					KASSERT(VOP_ISLOCKED(vp));
					VOP_UNLOCK(vp);
					numrefed++;
				}
			}
			ip = VTOI(vp);
		} else if (v_daddr == LFS_UNUSED_DADDR) {
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
			/* blkp->bi_daddr = LFS_UNUSED_DADDR; */
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

			/* XXX ondisk32 */
			error = VOP_BMAP(vp, blkp->bi_lbn, NULL,
					 &bi_daddr, NULL);
			if (error)
			{
				blkp->bi_daddr = LFS_UNUSED_DADDR;
				continue;
			}
			blkp->bi_daddr = dbtofsb(fs, bi_daddr);
			/* Fill in the block size, too */
			if (blkp->bi_lbn >= 0)
				blkp->bi_size = blksize(fs, ip, blkp->bi_lbn);
			else
				blkp->bi_size = fs->lfs_bsize;
		}
	}

	/*
	 * Finish the old file, if there was one.  The presence
	 * of a usable vnode in vp is signaled by a valid v_daddr.
	 */
	if (v_daddr != LFS_UNUSED_DADDR) {
		lfs_vunref(vp);
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

	if ((error = kauth_authorize_generic(l->l_cred, KAUTH_GENERIC_ISSUSER,
	    NULL)) != 0)
		return (error);

	if ((error = copyin(SCARG(uap, fsidp), &fsid, sizeof(fsid_t))) != 0)
		return (error);
	if ((mntp = vfs_getvfs(&fsid)) == NULL)
		return (ENOENT);

	fs = VFSTOUFS(mntp)->um_lfs;
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

	if (dtosn(fs, fs->lfs_curseg) == segnum) {
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

	fs->lfs_avail += segtod(fs, 1);
	if (sup->su_flags & SEGUSE_SUPERBLOCK)
		fs->lfs_avail -= btofsb(fs, LFS_SBPAD);
	if (fs->lfs_version > 1 && segnum == 0 &&
	    fs->lfs_start < btofsb(fs, LFS_LABELPAD))
		fs->lfs_avail -= btofsb(fs, LFS_LABELPAD) - fs->lfs_start;
	mutex_enter(&lfs_lock);
	fs->lfs_bfree += sup->su_nsums * btofsb(fs, fs->lfs_sumsize) +
		btofsb(fs, sup->su_ninos * fs->lfs_ibsize);
	fs->lfs_dmeta -= sup->su_nsums * btofsb(fs, fs->lfs_sumsize) +
		btofsb(fs, sup->su_ninos * fs->lfs_ibsize);
	if (fs->lfs_dmeta < 0)
		fs->lfs_dmeta = 0;
	mutex_exit(&lfs_lock);
	sup->su_flags &= ~SEGUSE_DIRTY;
	LFS_WRITESEGENTRY(sup, fs, segnum, bp);

	LFS_CLEANERINFO(cip, fs, bp);
	++cip->clean;
	--cip->dirty;
	fs->lfs_nclean = cip->clean;
	cip->bfree = fs->lfs_bfree;
	mutex_enter(&lfs_lock);
	cip->avail = fs->lfs_avail - fs->lfs_ravail - fs->lfs_favail;
	wakeup(&fs->lfs_avail);
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
		addr = &VFSTOUFS(mntp)->um_lfs->lfs_nextseg;
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
	if ((error = kauth_authorize_generic(l->l_cred, KAUTH_GENERIC_ISSUSER,
	    NULL)) != 0)
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
 * VFS_VGET call specialized for the cleaner.  The cleaner already knows the
 * daddr from the ifile, so don't look it up again.  If the cleaner is
 * processing IINFO structures, it may have the ondisk inode already, so
 * don't go retrieving it again.
 *
 * we lfs_vref, and it is the caller's responsibility to lfs_vunref
 * when finished.
 */

int
lfs_fasthashget(dev_t dev, ino_t ino, struct vnode **vpp)
{
	struct vnode *vp;

	mutex_enter(&ufs_ihash_lock);
	if ((vp = ufs_ihashlookup(dev, ino)) != NULL) {
		mutex_enter(vp->v_interlock);
		mutex_exit(&ufs_ihash_lock);
		if (vp->v_iflag & VI_XLOCK) {
			DLOG((DLOG_CLEAN, "lfs_fastvget: ino %d VI_XLOCK\n",
			      ino));
			lfs_stats.clean_vnlocked++;
			mutex_exit(vp->v_interlock);
			return EAGAIN;
		}
		if (lfs_vref(vp)) {
			DLOG((DLOG_CLEAN, "lfs_fastvget: lfs_vref failed"
			      " for ino %d\n", ino));
			lfs_stats.clean_inlocked++;
			return EAGAIN;
		}
	} else {
		mutex_exit(&ufs_ihash_lock);
	}
	*vpp = vp;

	return (0);
}

int
lfs_fastvget(struct mount *mp, ino_t ino, daddr_t daddr, struct vnode **vpp,
	     struct ufs1_dinode *dinp)
{
	struct inode *ip;
	struct ufs1_dinode *dip;
	struct vnode *vp;
	struct ufsmount *ump;
	dev_t dev;
	int error, retries;
	struct buf *bp;
	struct lfs *fs;

	ump = VFSTOUFS(mp);
	dev = ump->um_dev;
	fs = ump->um_lfs;

	/*
	 * Wait until the filesystem is fully mounted before allowing vget
	 * to complete.	 This prevents possible problems with roll-forward.
	 */
	mutex_enter(&lfs_lock);
	while (fs->lfs_flags & LFS_NOTYET) {
		mtsleep(&fs->lfs_flags, PRIBIO+1, "lfs_fnotyet", 0,
			&lfs_lock);
	}
	mutex_exit(&lfs_lock);

	/*
	 * This is playing fast and loose.  Someone may have the inode
	 * locked, in which case they are going to be distinctly unhappy
	 * if we trash something.
	 */

	error = lfs_fasthashget(dev, ino, vpp);
	if (error != 0 || *vpp != NULL)
		return (error);

	/*
	 * getnewvnode(9) will call vfs_busy, which will block if the
	 * filesystem is being unmounted; but umount(9) is waiting for
	 * us because we're already holding the fs busy.
	 * XXXMP
	 */
	if (mp->mnt_iflag & IMNT_UNMOUNT) {
		*vpp = NULL;
		return EDEADLK;
	}
	error = getnewvnode(VT_LFS, mp, lfs_vnodeop_p, NULL, &vp);
	if (error) {
		*vpp = NULL;
		return (error);
	}

	mutex_enter(&ufs_hashlock);
	error = lfs_fasthashget(dev, ino, vpp);
	if (error != 0 || *vpp != NULL) {
		mutex_exit(&ufs_hashlock);
		ungetnewvnode(vp);
		return (error);
	}

	/* Allocate new vnode/inode. */
	lfs_vcreate(mp, ino, vp);

	/*
	 * Put it onto its hash chain and lock it so that other requests for
	 * this inode will block if they arrive while we are sleeping waiting
	 * for old data structures to be purged or for the contents of the
	 * disk portion of this inode to be read.
	 */
	ip = VTOI(vp);
	ufs_ihashins(ip);
	mutex_exit(&ufs_hashlock);

	/*
	 * XXX
	 * This may not need to be here, logically it should go down with
	 * the i_devvp initialization.
	 * Ask Kirk.
	 */
	ip->i_lfs = fs;

	/* Read in the disk contents for the inode, copy into the inode. */
	if (dinp) {
		error = copyin(dinp, ip->i_din.ffs1_din, sizeof (struct ufs1_dinode));
		if (error) {
			DLOG((DLOG_CLEAN, "lfs_fastvget: dinode copyin failed"
			      " for ino %d\n", ino));
			ufs_ihashrem(ip);

			/* Unlock and discard unneeded inode. */
			VOP_UNLOCK(vp);
			lfs_vunref(vp);
			*vpp = NULL;
			return (error);
		}
		if (ip->i_number != ino)
			panic("lfs_fastvget: I was fed the wrong inode!");
	} else {
		retries = 0;
	    again:
		error = bread(ump->um_devvp, fsbtodb(fs, daddr), fs->lfs_ibsize,
			      NOCRED, 0, &bp);
		if (error) {
			DLOG((DLOG_CLEAN, "lfs_fastvget: bread failed (%d)\n",
			      error));
			/*
			 * The inode does not contain anything useful, so it
			 * would be misleading to leave it on its hash chain.
			 * Iput() will return it to the free list.
			 */
			ufs_ihashrem(ip);

			/* Unlock and discard unneeded inode. */
			VOP_UNLOCK(vp);
			lfs_vunref(vp);
			brelse(bp, 0);
			*vpp = NULL;
			return (error);
		}
		dip = lfs_ifind(ump->um_lfs, ino, bp);
		if (dip == NULL) {
			/* Assume write has not completed yet; try again */
			brelse(bp, BC_INVAL);
			++retries;
			if (retries > LFS_IFIND_RETRIES)
				panic("lfs_fastvget: dinode not found");
			DLOG((DLOG_CLEAN, "lfs_fastvget: dinode not found,"
			      " retrying...\n"));
			goto again;
		}
		*ip->i_din.ffs1_din = *dip;
		brelse(bp, 0);
	}
	lfs_vinit(mp, &vp);

	*vpp = vp;

	KASSERT(VOP_ISLOCKED(vp));
	VOP_UNLOCK(vp);

	return (0);
}

/*
 * Make up a "fake" cleaner buffer, copy the data from userland into it.
 */
struct buf *
lfs_fakebuf(struct lfs *fs, struct vnode *vp, int lbn, size_t size, void *uaddr)
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
