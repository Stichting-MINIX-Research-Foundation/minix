/*	$NetBSD: lfs_rfw.c,v 1.12 2009/02/22 20:28:07 ad Exp $	*/

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: lfs_rfw.c,v 1.12 2009/02/22 20:28:07 ad Exp $");

#if defined(_KERNEL_OPT)
#include "opt_quota.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/kthread.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/mbuf.h>
#include <sys/file.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <uvm/uvm_extern.h>
#include <sys/sysctl.h>
#include <sys/conf.h>
#include <sys/kauth.h>

#include <miscfs/specfs/specdev.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

#include <uvm/uvm.h>
#include <uvm/uvm_stat.h>
#include <uvm/uvm_pager.h>
#include <uvm/uvm_pdaemon.h>

#include <ufs/lfs/lfs.h>
#include <ufs/lfs/lfs_extern.h>

#include <miscfs/genfs/genfs.h>
#include <miscfs/genfs/genfs_node.h>

/*
 * Roll-forward code.
 */
static daddr_t check_segsum(struct lfs *, daddr_t, u_int64_t,
    kauth_cred_t, int, int *, struct lwp *);

extern int lfs_do_rfw;

/*
 * Allocate a particular inode with a particular version number, freeing
 * any previous versions of this inode that may have gone before.
 * Used by the roll-forward code.
 *
 * XXX this function does not have appropriate locking to be used on a live fs;
 * XXX but something similar could probably be used for an "undelete" call.
 *
 * Called with the Ifile inode locked.
 */
int
lfs_rf_valloc(struct lfs *fs, ino_t ino, int vers, struct lwp *l,
	      struct vnode **vpp)
{
	IFILE *ifp;
	struct buf *bp, *cbp;
	struct vnode *vp;
	struct inode *ip;
	ino_t tino, oldnext;
	int error;
	CLEANERINFO *cip;

	ASSERT_SEGLOCK(fs); /* XXX it doesn't, really */

	/*
	 * First, just try a vget. If the version number is the one we want,
	 * we don't have to do anything else.  If the version number is wrong,
	 * take appropriate action.
	 */
	error = VFS_VGET(fs->lfs_ivnode->v_mount, ino, &vp);
	if (error == 0) {
		DLOG((DLOG_RF, "lfs_rf_valloc[1]: ino %d vp %p\n", ino, vp));

		*vpp = vp;
		ip = VTOI(vp);
		if (ip->i_gen == vers)
			return 0;
		else if (ip->i_gen < vers) {
			lfs_truncate(vp, (off_t)0, 0, NOCRED);
			ip->i_gen = ip->i_ffs1_gen = vers;
			LFS_SET_UINO(ip, IN_CHANGE | IN_UPDATE);
			return 0;
		} else {
			DLOG((DLOG_RF, "ino %d: sought version %d, got %d\n",
			       ino, vers, ip->i_ffs1_gen));
			vput(vp);
			*vpp = NULLVP;
			return EEXIST;
		}
	}

	/*
	 * The inode is not in use.  Find it on the free list.
	 */
	/* If the Ifile is too short to contain this inum, extend it */
	while (VTOI(fs->lfs_ivnode)->i_size <= (ino /
		fs->lfs_ifpb + fs->lfs_cleansz + fs->lfs_segtabsz)
		<< fs->lfs_bshift) {
		lfs_extend_ifile(fs, NOCRED);
	}

	LFS_IENTRY(ifp, fs, ino, bp);
	oldnext = ifp->if_nextfree;
	ifp->if_version = vers;
	brelse(bp, 0);

	LFS_GET_HEADFREE(fs, cip, cbp, &ino);
	if (ino) {
		LFS_PUT_HEADFREE(fs, cip, cbp, oldnext);
	} else {
		tino = ino;
		while (1) {
			LFS_IENTRY(ifp, fs, tino, bp);
			if (ifp->if_nextfree == ino ||
			    ifp->if_nextfree == LFS_UNUSED_INUM)
				break;
			tino = ifp->if_nextfree;
			brelse(bp, 0);
		}
		if (ifp->if_nextfree == LFS_UNUSED_INUM) {
			brelse(bp, 0);
			return ENOENT;
		}
		ifp->if_nextfree = oldnext;
		LFS_BWRITE_LOG(bp);
	}

	error = lfs_ialloc(fs, fs->lfs_ivnode, ino, vers, &vp);
	if (error == 0) {
		/*
		 * Make it VREG so we can put blocks on it.  We will change
		 * this later if it turns out to be some other kind of file.
		 */
		ip = VTOI(vp);
		ip->i_mode = ip->i_ffs1_mode = IFREG;
		ip->i_nlink = ip->i_ffs1_nlink = 1;
		ufs_vinit(vp->v_mount, lfs_specop_p, lfs_fifoop_p, &vp);
		ip = VTOI(vp);

		DLOG((DLOG_RF, "lfs_rf_valloc: ino %d vp %p\n", ino, vp));

		/* The dirop-nature of this vnode is past */
		lfs_unmark_vnode(vp);
		(void)lfs_vunref(vp);
		vp->v_uflag &= ~VU_DIROP;
		mutex_enter(&lfs_lock);
		--lfs_dirvcount;
		--fs->lfs_dirvcount;
		TAILQ_REMOVE(&fs->lfs_dchainhd, ip, i_lfs_dchain);
		wakeup(&lfs_dirvcount);
		wakeup(&fs->lfs_dirvcount);
		mutex_exit(&lfs_lock);
	}
	*vpp = vp;
	return error;
}

/*
 * Load the appropriate indirect block, and change the appropriate pointer.
 * Mark the block dirty.  Do segment and avail accounting.
 */
static int
update_meta(struct lfs *fs, ino_t ino, int vers, daddr_t lbn,
	    daddr_t ndaddr, size_t size, struct lwp *l)
{
	int error;
	struct vnode *vp;
	struct inode *ip;
#ifdef DEBUG
	daddr_t odaddr;
	struct indir a[NIADDR];
	int num;
	int i;
#endif /* DEBUG */
	struct buf *bp;
	SEGUSE *sup;

	KASSERT(lbn >= 0);	/* no indirect blocks */

	if ((error = lfs_rf_valloc(fs, ino, vers, l, &vp)) != 0) {
		DLOG((DLOG_RF, "update_meta: ino %d: lfs_rf_valloc"
		      " returned %d\n", ino, error));
		return error;
	}

	if ((error = lfs_balloc(vp, (lbn << fs->lfs_bshift), size,
				NOCRED, 0, &bp)) != 0) {
		vput(vp);
		return (error);
	}
	/* No need to write, the block is already on disk */
	if (bp->b_oflags & BO_DELWRI) {
		LFS_UNLOCK_BUF(bp);
		fs->lfs_avail += btofsb(fs, bp->b_bcount);
	}
	brelse(bp, BC_INVAL);

	/*
	 * Extend the file, if it is not large enough already.
	 * XXX this is not exactly right, we don't know how much of the
	 * XXX last block is actually used.  We hope that an inode will
	 * XXX appear later to give the correct size.
	 */
	ip = VTOI(vp);
	if (ip->i_size <= (lbn << fs->lfs_bshift)) {
		u_int64_t newsize;

		if (lbn < NDADDR)
			newsize = ip->i_ffs1_size = (lbn << fs->lfs_bshift) +
				(size - fs->lfs_fsize) + 1;
		else
			newsize = ip->i_ffs1_size = (lbn << fs->lfs_bshift) + 1;

		if (ip->i_size < newsize) {
			ip->i_size = newsize;
			/*
			 * tell vm our new size for the case the inode won't
			 * appear later.
			 */
			uvm_vnp_setsize(vp, newsize);
		}
	}

	lfs_update_single(fs, NULL, vp, lbn, ndaddr, size);

	LFS_SEGENTRY(sup, fs, dtosn(fs, ndaddr), bp);
	sup->su_nbytes += size;
	LFS_WRITESEGENTRY(sup, fs, dtosn(fs, ndaddr), bp);

	/* differences here should be due to UNWRITTEN indirect blocks. */
	KASSERT((lblkno(fs, ip->i_size) > NDADDR &&
	    ip->i_lfs_effnblks == ip->i_ffs1_blocks) ||
	    ip->i_lfs_effnblks >= ip->i_ffs1_blocks);

#ifdef DEBUG
	/* Now look again to make sure it worked */
	ufs_bmaparray(vp, lbn, &odaddr, &a[0], &num, NULL, NULL);
	for (i = num; i > 0; i--) {
		if (!a[i].in_exists)
			panic("update_meta: absent %d lv indirect block", i);
	}
	if (dbtofsb(fs, odaddr) != ndaddr)
		DLOG((DLOG_RF, "update_meta: failed setting ino %d lbn %"
		      PRId64 " to %" PRId64 "\n", ino, lbn, ndaddr));
#endif /* DEBUG */
	vput(vp);
	return 0;
}

static int
update_inoblk(struct lfs *fs, daddr_t offset, kauth_cred_t cred,
	      struct lwp *l)
{
	struct vnode *devvp, *vp;
	struct inode *ip;
	struct ufs1_dinode *dip;
	struct buf *dbp, *ibp;
	int error;
	daddr_t daddr;
	IFILE *ifp;
	SEGUSE *sup;

	devvp = VTOI(fs->lfs_ivnode)->i_devvp;

	/*
	 * Get the inode, update times and perms.
	 * DO NOT update disk blocks, we do that separately.
	 */
	error = bread(devvp, fsbtodb(fs, offset), fs->lfs_ibsize,
	    cred, 0, &dbp);
	if (error) {
		DLOG((DLOG_RF, "update_inoblk: bread returned %d\n", error));
		return error;
	}
	dip = ((struct ufs1_dinode *)(dbp->b_data)) + INOPB(fs);
	while (--dip >= (struct ufs1_dinode *)dbp->b_data) {
		if (dip->di_inumber > LFS_IFILE_INUM) {
			error = lfs_rf_valloc(fs, dip->di_inumber, dip->di_gen,
					      l, &vp);
			if (error) {
				DLOG((DLOG_RF, "update_inoblk: lfs_rf_valloc"
				      " returned %d\n", error));
				continue;
			}
			ip = VTOI(vp);
			if (dip->di_size != ip->i_size)
				lfs_truncate(vp, dip->di_size, 0, NOCRED);
			/* Get mode, link count, size, and times */
			memcpy(ip->i_din.ffs1_din, dip,
			       offsetof(struct ufs1_dinode, di_db[0]));

			/* Then the rest, except di_blocks */
			ip->i_flags = ip->i_ffs1_flags = dip->di_flags;
			ip->i_gen = ip->i_ffs1_gen = dip->di_gen;
			ip->i_uid = ip->i_ffs1_uid = dip->di_uid;
			ip->i_gid = ip->i_ffs1_gid = dip->di_gid;

			ip->i_mode = ip->i_ffs1_mode;
			ip->i_nlink = ip->i_ffs1_nlink;
			ip->i_size = ip->i_ffs1_size;

			LFS_SET_UINO(ip, IN_CHANGE | IN_UPDATE);

			/* Re-initialize to get type right */
			ufs_vinit(vp->v_mount, lfs_specop_p, lfs_fifoop_p,
				  &vp);
			vput(vp);

			/* Record change in location */
			LFS_IENTRY(ifp, fs, dip->di_inumber, ibp);
			daddr = ifp->if_daddr;
			ifp->if_daddr = dbtofsb(fs, dbp->b_blkno);
			error = LFS_BWRITE_LOG(ibp); /* Ifile */
			/* And do segment accounting */
			if (dtosn(fs, daddr) != dtosn(fs, dbtofsb(fs, dbp->b_blkno))) {
				if (daddr > 0) {
					LFS_SEGENTRY(sup, fs, dtosn(fs, daddr),
						     ibp);
					sup->su_nbytes -= sizeof (struct ufs1_dinode);
					LFS_WRITESEGENTRY(sup, fs,
							  dtosn(fs, daddr),
							  ibp);
				}
				LFS_SEGENTRY(sup, fs, dtosn(fs, dbtofsb(fs, dbp->b_blkno)),
					     ibp);
				sup->su_nbytes += sizeof (struct ufs1_dinode);
				LFS_WRITESEGENTRY(sup, fs,
						  dtosn(fs, dbtofsb(fs, dbp->b_blkno)),
						  ibp);
			}
		}
	}
	brelse(dbp, BC_AGE);

	return 0;
}

#define CHECK_CKSUM   0x0001  /* Check the checksum to make sure it's valid */
#define CHECK_UPDATE  0x0002  /* Update Ifile for new data blocks / inodes */

static daddr_t
check_segsum(struct lfs *fs, daddr_t offset, u_int64_t nextserial,
	     kauth_cred_t cred, int flags, int *pseg_flags, struct lwp *l)
{
	struct vnode *devvp;
	struct buf *bp, *dbp;
	int error, nblocks = 0, ninos, i, j; /* XXX: gcc */
	SEGSUM *ssp;
	u_long *dp = NULL, *datap = NULL; /* XXX u_int32_t */
	daddr_t oldoffset;
	int32_t *iaddr;	/* XXX ondisk32 */
	FINFO *fip;
	SEGUSE *sup;
	size_t size;

	devvp = VTOI(fs->lfs_ivnode)->i_devvp;
	/*
	 * If the segment has a superblock and we're at the top
	 * of the segment, skip the superblock.
	 */
	if (sntod(fs, dtosn(fs, offset)) == offset) {
		LFS_SEGENTRY(sup, fs, dtosn(fs, offset), bp);
		if (sup->su_flags & SEGUSE_SUPERBLOCK)
			offset += btofsb(fs, LFS_SBPAD);
		brelse(bp, 0);
	}

	/* Read in the segment summary */
	error = bread(devvp, fsbtodb(fs, offset), fs->lfs_sumsize,
	    cred, 0, &bp);
	if (error)
		return -1;

	/* Check summary checksum */
	ssp = (SEGSUM *)bp->b_data;
	if (flags & CHECK_CKSUM) {
		if (ssp->ss_sumsum != cksum(&ssp->ss_datasum,
					   fs->lfs_sumsize -
					   sizeof(ssp->ss_sumsum))) {
			DLOG((DLOG_RF, "Sumsum error at 0x%" PRIx64 "\n", offset));
			offset = -1;
			goto err1;
		}
		if (ssp->ss_nfinfo == 0 && ssp->ss_ninos == 0) {
			DLOG((DLOG_RF, "Empty pseg at 0x%" PRIx64 "\n", offset));
			offset = -1;
			goto err1;
		}
		if (ssp->ss_create < fs->lfs_tstamp) {
			DLOG((DLOG_RF, "Old data at 0x%" PRIx64 "\n", offset));
			offset = -1;
			goto err1;
		}
	}
	if (fs->lfs_version > 1) {
		if (ssp->ss_serial != nextserial) {
			DLOG((DLOG_RF, "Unexpected serial number at 0x%" PRIx64
			      "\n", offset));
			offset = -1;
			goto err1;
		}
		if (ssp->ss_ident != fs->lfs_ident) {
			DLOG((DLOG_RF, "Incorrect fsid (0x%x vs 0x%x) at 0x%"
			      PRIx64 "\n", ssp->ss_ident, fs->lfs_ident, offset));
			offset = -1;
			goto err1;
		}
	}
	if (pseg_flags)
		*pseg_flags = ssp->ss_flags;
	oldoffset = offset;
	offset += btofsb(fs, fs->lfs_sumsize);

	ninos = howmany(ssp->ss_ninos, INOPB(fs));
	/* XXX ondisk32 */
	iaddr = (int32_t *)((char*)bp->b_data + fs->lfs_sumsize - sizeof(int32_t));
	if (flags & CHECK_CKSUM) {
		/* Count blocks */
		nblocks = 0;
		fip = (FINFO *)((char*)bp->b_data + SEGSUM_SIZE(fs));
		for (i = 0; i < ssp->ss_nfinfo; ++i) {
			nblocks += fip->fi_nblocks;
			if (fip->fi_nblocks <= 0)
				break;
			/* XXX ondisk32 */
			fip = (FINFO *)(((char *)fip) + FINFOSIZE +
					(fip->fi_nblocks * sizeof(int32_t)));
		}
		nblocks += ninos;
		/* Create the sum array */
		datap = dp = (u_long *)malloc(nblocks * sizeof(u_long),
					      M_SEGMENT, M_WAITOK);
	}

	/* Handle individual blocks */
	fip = (FINFO *)((char*)bp->b_data + SEGSUM_SIZE(fs));
	for (i = 0; i < ssp->ss_nfinfo || ninos; ++i) {
		/* Inode block? */
		if (ninos && *iaddr == offset) {
			if (flags & CHECK_CKSUM) {
				/* Read in the head and add to the buffer */
				error = bread(devvp, fsbtodb(fs, offset), fs->lfs_bsize,
					      cred, 0, &dbp);
				if (error) {
					offset = -1;
					goto err2;
				}
				(*dp++) = ((u_long *)(dbp->b_data))[0];
				brelse(dbp, BC_AGE);
			}
			if (flags & CHECK_UPDATE) {
				if ((error = update_inoblk(fs, offset, cred, l))
				    != 0) {
					offset = -1;
					goto err2;
				}
			}
			offset += btofsb(fs, fs->lfs_ibsize);
			--iaddr;
			--ninos;
			--i; /* compensate */
			continue;
		}
		size = fs->lfs_bsize;
		for (j = 0; j < fip->fi_nblocks; ++j) {
			if (j == fip->fi_nblocks - 1)
				size = fip->fi_lastlength;
			if (flags & CHECK_CKSUM) {
				error = bread(devvp, fsbtodb(fs, offset), size,
				    cred, 0, &dbp);
				if (error) {
					offset = -1;
					goto err2;
				}
				(*dp++) = ((u_long *)(dbp->b_data))[0];
				brelse(dbp, BC_AGE);
			}
			/* Account for and update any direct blocks */
			if ((flags & CHECK_UPDATE) &&
			   fip->fi_ino > LFS_IFILE_INUM &&
			   fip->fi_blocks[j] >= 0) {
				update_meta(fs, fip->fi_ino, fip->fi_version,
					    fip->fi_blocks[j], offset, size, l);
			}
			offset += btofsb(fs, size);
		}
		/* XXX ondisk32 */
		fip = (FINFO *)(((char *)fip) + FINFOSIZE
				+ fip->fi_nblocks * sizeof(int32_t));
	}
	/* Checksum the array, compare */
	if ((flags & CHECK_CKSUM) &&
	   ssp->ss_datasum != cksum(datap, nblocks * sizeof(u_long)))
	{
		DLOG((DLOG_RF, "Datasum error at 0x%" PRIx64
		      " (wanted %x got %x)\n",
		      offset, ssp->ss_datasum, cksum(datap, nblocks *
						     sizeof(u_long))));
		offset = -1;
		goto err2;
	}

	/* If we're at the end of the segment, move to the next */
	if (dtosn(fs, offset + btofsb(fs, fs->lfs_sumsize + fs->lfs_bsize)) !=
	   dtosn(fs, offset)) {
		if (dtosn(fs, offset) == dtosn(fs, ssp->ss_next)) {
			offset = -1;
			goto err2;
		}
		offset = ssp->ss_next;
		DLOG((DLOG_RF, "LFS roll forward: moving to offset 0x%" PRIx64
		       " -> segment %d\n", offset, dtosn(fs,offset)));
	}

	if (flags & CHECK_UPDATE) {
		fs->lfs_avail -= (offset - oldoffset);
		/* Don't clog the buffer queue */
		mutex_enter(&lfs_lock);
		if (locked_queue_count > LFS_MAX_BUFS ||
		    locked_queue_bytes > LFS_MAX_BYTES) {
			lfs_flush(fs, SEGM_CKP, 0);
		}
		mutex_exit(&lfs_lock);
	}

    err2:
	if (flags & CHECK_CKSUM)
		free(datap, M_SEGMENT);
    err1:
	brelse(bp, BC_AGE);

	/* XXX should we update the serial number even for bad psegs? */
	if ((flags & CHECK_UPDATE) && offset > 0 && fs->lfs_version > 1)
		fs->lfs_serial = nextserial;
	return offset;
}

void
lfs_roll_forward(struct lfs *fs, struct mount *mp, struct lwp *l)
{
	int flags, dirty;
	daddr_t offset, oldoffset, lastgoodpseg;
	int sn, curseg, do_rollforward;
	struct proc *p;
	kauth_cred_t cred;
	SEGUSE *sup;
	struct buf *bp;

	p = l ? l->l_proc : NULL;
	cred = p ? p->p_cred : NOCRED;

	/*
	 * Roll forward.
	 *
	 * We don't roll forward for v1 filesystems, because
	 * of the danger that the clock was turned back between the last
	 * checkpoint and crash.  This would roll forward garbage.
	 *
	 * v2 filesystems don't have this problem because they use a
	 * monotonically increasing serial number instead of a timestamp.
	 */
	do_rollforward = (!(fs->lfs_pflags & LFS_PF_CLEAN) &&
			  lfs_do_rfw && fs->lfs_version > 1 && p != NULL);
	if (do_rollforward) {
		u_int64_t nextserial;
		/*
		 * Phase I: Find the address of the last good partial
		 * segment that was written after the checkpoint.  Mark
		 * the segments in question dirty, so they won't be
		 * reallocated.
		 */
		lastgoodpseg = oldoffset = offset = fs->lfs_offset;
		flags = 0x0;
		DLOG((DLOG_RF, "LFS roll forward phase 1: start at offset 0x%"
		      PRIx64 "\n", offset));
		LFS_SEGENTRY(sup, fs, dtosn(fs, offset), bp);
		if (!(sup->su_flags & SEGUSE_DIRTY))
			--fs->lfs_nclean;
		sup->su_flags |= SEGUSE_DIRTY;
		LFS_WRITESEGENTRY(sup, fs, dtosn(fs, offset), bp);
		nextserial = fs->lfs_serial + 1;
		while ((offset = check_segsum(fs, offset, nextserial,
		    cred, CHECK_CKSUM, &flags, l)) > 0) {
			nextserial++;
			if (sntod(fs, oldoffset) != sntod(fs, offset)) {
				LFS_SEGENTRY(sup, fs, dtosn(fs, oldoffset),
					     bp);
				if (!(sup->su_flags & SEGUSE_DIRTY))
					--fs->lfs_nclean;
				sup->su_flags |= SEGUSE_DIRTY;
				LFS_WRITESEGENTRY(sup, fs, dtosn(fs, oldoffset),
					     bp);
			}

			DLOG((DLOG_RF, "LFS roll forward phase 1: offset=0x%"
			      PRIx64 "\n", offset));
			if (flags & SS_DIROP) {
				DLOG((DLOG_RF, "lfs_mountfs: dirops at 0x%"
				      PRIx64 "\n", oldoffset));
				if (!(flags & SS_CONT)) {
				     DLOG((DLOG_RF, "lfs_mountfs: dirops end "
					   "at 0x%" PRIx64 "\n", oldoffset));
				}
			}
			if (!(flags & SS_CONT))
				lastgoodpseg = offset;
			oldoffset = offset;
		}
		if (flags & SS_CONT) {
			DLOG((DLOG_RF, "LFS roll forward: warning: incomplete "
			      "dirops discarded\n"));
		}
		DLOG((DLOG_RF, "LFS roll forward phase 1: completed: "
		      "lastgoodpseg=0x%" PRIx64 "\n", lastgoodpseg));
		oldoffset = fs->lfs_offset;
		if (fs->lfs_offset != lastgoodpseg) {
			/* Don't overwrite what we're trying to preserve */
			offset = fs->lfs_offset;
			fs->lfs_offset = lastgoodpseg;
			fs->lfs_curseg = sntod(fs, dtosn(fs, fs->lfs_offset));
			for (sn = curseg = dtosn(fs, fs->lfs_curseg);;) {
				sn = (sn + 1) % fs->lfs_nseg;
				if (sn == curseg)
					panic("lfs_mountfs: no clean segments");
				LFS_SEGENTRY(sup, fs, sn, bp);
				dirty = (sup->su_flags & SEGUSE_DIRTY);
				brelse(bp, 0);
				if (!dirty)
					break;
			}
			fs->lfs_nextseg = sntod(fs, sn);

			/*
			 * Phase II: Roll forward from the first superblock.
			 */
			while (offset != lastgoodpseg) {
				DLOG((DLOG_RF, "LFS roll forward phase 2: 0x%"
				      PRIx64 "\n", offset));
				offset = check_segsum(fs, offset,
				    fs->lfs_serial + 1, cred, CHECK_UPDATE,
				    NULL, l);
			}

			/*
			 * Finish: flush our changes to disk.
			 */
			lfs_segwrite(mp, SEGM_CKP | SEGM_SYNC);
			DLOG((DLOG_RF, "lfs_mountfs: roll forward ",
			      "recovered %lld blocks\n",
			      (long long)(lastgoodpseg - oldoffset)));
		}
		DLOG((DLOG_RF, "LFS roll forward complete\n"));
	}
}
