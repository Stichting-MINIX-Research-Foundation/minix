/*	$NetBSD: lfs_rfw.c,v 1.32 2015/10/03 08:27:55 dholland Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: lfs_rfw.c,v 1.32 2015/10/03 08:27:55 dholland Exp $");

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

#include <ufs/lfs/ulfs_quotacommon.h>
#include <ufs/lfs/ulfs_inode.h>
#include <ufs/lfs/ulfsmount.h>
#include <ufs/lfs/ulfs_extern.h>

#include <uvm/uvm.h>
#include <uvm/uvm_stat.h>
#include <uvm/uvm_pager.h>
#include <uvm/uvm_pdaemon.h>

#include <ufs/lfs/lfs.h>
#include <ufs/lfs/lfs_accessors.h>
#include <ufs/lfs/lfs_kernel.h>
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
	struct vattr va;
	struct vnode *vp;
	struct inode *ip;
	int error;

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
			ip->i_gen = vers;
			lfs_dino_setgen(fs, ip->i_din, vers);
			LFS_SET_UINO(ip, IN_CHANGE | IN_UPDATE);
			return 0;
		} else {
			DLOG((DLOG_RF, "ino %d: sought version %d, got %d\n",
			       ino, vers, lfs_dino_getgen(fs, ip->i_din)));
			vput(vp);
			*vpp = NULLVP;
			return EEXIST;
		}
	}

	/* Not found, create as regular file. */
	vattr_null(&va);
	va.va_type = VREG;
	va.va_mode = 0;
	va.va_fileid = ino;
	va.va_gen = vers;
	error = vcache_new(fs->lfs_ivnode->v_mount, NULL, &va, NOCRED, &vp);
	if (error)
		return error;
	error = vn_lock(vp, LK_EXCLUSIVE);
	if (error) {
		vrele(vp);
		*vpp = NULLVP;
		return error;
	}
	ip = VTOI(vp);
	ip->i_nlink = 1;
	lfs_dino_setnlink(fs, ip->i_din, 1);
	*vpp = vp;
	return 0;
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
	struct indir a[ULFS_NIADDR];
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

	if ((error = lfs_balloc(vp, (lbn << lfs_sb_getbshift(fs)), size,
				NOCRED, 0, &bp)) != 0) {
		vput(vp);
		return (error);
	}
	/* No need to write, the block is already on disk */
	if (bp->b_oflags & BO_DELWRI) {
		LFS_UNLOCK_BUF(bp);
		lfs_sb_addavail(fs, lfs_btofsb(fs, bp->b_bcount));
		/* XXX should this wake up fs->lfs_availsleep? */
	}
	brelse(bp, BC_INVAL);

	/*
	 * Extend the file, if it is not large enough already.
	 * XXX this is not exactly right, we don't know how much of the
	 * XXX last block is actually used.  We hope that an inode will
	 * XXX appear later to give the correct size.
	 */
	ip = VTOI(vp);
	if (ip->i_size <= (lbn << lfs_sb_getbshift(fs))) {
		u_int64_t newsize;

		if (lbn < ULFS_NDADDR) {
			newsize = (lbn << lfs_sb_getbshift(fs)) +
				(size - lfs_sb_getfsize(fs)) + 1;
		} else {
			newsize = (lbn << lfs_sb_getbshift(fs)) + 1;
		}
		lfs_dino_setsize(fs, ip->i_din, newsize);

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

	LFS_SEGENTRY(sup, fs, lfs_dtosn(fs, ndaddr), bp);
	sup->su_nbytes += size;
	LFS_WRITESEGENTRY(sup, fs, lfs_dtosn(fs, ndaddr), bp);

	/* differences here should be due to UNWRITTEN indirect blocks. */
	KASSERT((lfs_lblkno(fs, ip->i_size) > ULFS_NDADDR &&
	    ip->i_lfs_effnblks == lfs_dino_getblocks(fs, ip->i_din)) ||
	    ip->i_lfs_effnblks >= lfs_dino_getblocks(fs, ip->i_din));

#ifdef DEBUG
	/* Now look again to make sure it worked */
	ulfs_bmaparray(vp, lbn, &odaddr, &a[0], &num, NULL, NULL);
	for (i = num; i > 0; i--) {
		if (!a[i].in_exists)
			panic("update_meta: absent %d lv indirect block", i);
	}
	if (LFS_DBTOFSB(fs, odaddr) != ndaddr)
		DLOG((DLOG_RF, "update_meta: failed setting ino %d lbn %"
		      PRId64 " to %" PRId64 "\n", ino, lbn, ndaddr));
#endif /* DEBUG */
	vput(vp);
	return 0;
}

/*
 * Copy some the fields of the dinode as needed by update_inoblk().
 */
static void
update_inoblk_copy_dinode(struct lfs *fs,
    union lfs_dinode *dstu, const union lfs_dinode *srcu)
{
	if (fs->lfs_is64) {
		struct lfs64_dinode *dst = &dstu->u_64;
		const struct lfs64_dinode *src = &srcu->u_64;
		unsigned i;

		/*
		 * Copy everything but the block pointers and di_blocks.
		 * XXX what about di_extb?
		 */
		dst->di_mode = src->di_mode;
		dst->di_nlink = src->di_nlink;
		dst->di_uid = src->di_uid;
		dst->di_gid = src->di_gid;
		dst->di_blksize = src->di_blksize;
		dst->di_size = src->di_size;
		dst->di_atime = src->di_atime;
		dst->di_mtime = src->di_mtime;
		dst->di_ctime = src->di_ctime;
		dst->di_birthtime = src->di_birthtime;
		dst->di_mtimensec = src->di_mtimensec;
		dst->di_atimensec = src->di_atimensec;
		dst->di_ctimensec = src->di_ctimensec;
		dst->di_birthnsec = src->di_birthnsec;
		dst->di_gen = src->di_gen;
		dst->di_kernflags = src->di_kernflags;
		dst->di_flags = src->di_flags;
		dst->di_extsize = src->di_extsize;
		dst->di_modrev = src->di_modrev;
		dst->di_inumber = src->di_inumber;
		for (i = 0; i < __arraycount(src->di_spare); i++) {
			dst->di_spare[i] = src->di_spare[i];
		}
	} else {
		struct lfs32_dinode *dst = &dstu->u_32;
		const struct lfs32_dinode *src = &srcu->u_32;

		/* Get mode, link count, size, and times */
		memcpy(dst, src, offsetof(struct lfs32_dinode, di_db[0]));

		/* Then the rest, except di_blocks */
		dst->di_flags = src->di_flags;
		dst->di_gen = src->di_gen;
		dst->di_uid = src->di_uid;
		dst->di_gid = src->di_gid;
		dst->di_modrev = src->di_modrev;
	}
}

static int
update_inoblk(struct lfs *fs, daddr_t offset, kauth_cred_t cred,
	      struct lwp *l)
{
	struct vnode *devvp, *vp;
	struct inode *ip;
	union lfs_dinode *dip;
	struct buf *dbp, *ibp;
	int error;
	daddr_t daddr;
	IFILE *ifp;
	SEGUSE *sup;
	unsigned i, num;

	devvp = VTOI(fs->lfs_ivnode)->i_devvp;

	/*
	 * Get the inode, update times and perms.
	 * DO NOT update disk blocks, we do that separately.
	 */
	error = bread(devvp, LFS_FSBTODB(fs, offset), lfs_sb_getibsize(fs),
	    0, &dbp);
	if (error) {
		DLOG((DLOG_RF, "update_inoblk: bread returned %d\n", error));
		return error;
	}
	num = LFS_INOPB(fs);
	for (i = num; i-- > 0; ) {
		dip = DINO_IN_BLOCK(fs, dbp->b_data, i);
		if (lfs_dino_getinumber(fs, dip) > LFS_IFILE_INUM) {
			error = lfs_rf_valloc(fs, lfs_dino_getinumber(fs, dip),
					      lfs_dino_getgen(fs, dip),
					      l, &vp);
			if (error) {
				DLOG((DLOG_RF, "update_inoblk: lfs_rf_valloc"
				      " returned %d\n", error));
				continue;
			}
			ip = VTOI(vp);
			if (lfs_dino_getsize(fs, dip) != ip->i_size)
				lfs_truncate(vp, lfs_dino_getsize(fs, dip), 0,
					     NOCRED);
			update_inoblk_copy_dinode(fs, ip->i_din, dip);

			ip->i_flags = lfs_dino_getflags(fs, dip);
			ip->i_gen = lfs_dino_getgen(fs, dip);
			ip->i_uid = lfs_dino_getuid(fs, dip);
			ip->i_gid = lfs_dino_getgid(fs, dip);

			ip->i_mode = lfs_dino_getmode(fs, dip);
			ip->i_nlink = lfs_dino_getnlink(fs, dip);
			ip->i_size = lfs_dino_getsize(fs, dip);

			LFS_SET_UINO(ip, IN_CHANGE | IN_UPDATE);

			/* Re-initialize to get type right */
			ulfs_vinit(vp->v_mount, lfs_specop_p, lfs_fifoop_p,
				  &vp);
			vput(vp);

			/* Record change in location */
			LFS_IENTRY(ifp, fs, lfs_dino_getinumber(fs, dip), ibp);
			daddr = lfs_if_getdaddr(fs, ifp);
			lfs_if_setdaddr(fs, ifp, LFS_DBTOFSB(fs, dbp->b_blkno));
			error = LFS_BWRITE_LOG(ibp); /* Ifile */
			/* And do segment accounting */
			if (lfs_dtosn(fs, daddr) != lfs_dtosn(fs, LFS_DBTOFSB(fs, dbp->b_blkno))) {
				if (daddr > 0) {
					LFS_SEGENTRY(sup, fs, lfs_dtosn(fs, daddr),
						     ibp);
					sup->su_nbytes -= DINOSIZE(fs);
					LFS_WRITESEGENTRY(sup, fs,
							  lfs_dtosn(fs, daddr),
							  ibp);
				}
				LFS_SEGENTRY(sup, fs, lfs_dtosn(fs, LFS_DBTOFSB(fs, dbp->b_blkno)),
					     ibp);
				sup->su_nbytes += DINOSIZE(fs);
				LFS_WRITESEGENTRY(sup, fs,
						  lfs_dtosn(fs, LFS_DBTOFSB(fs, dbp->b_blkno)),
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
	IINFO *iip;
	FINFO *fip;
	SEGUSE *sup;
	size_t size;
	uint32_t datasum, foundsum;

	devvp = VTOI(fs->lfs_ivnode)->i_devvp;
	/*
	 * If the segment has a superblock and we're at the top
	 * of the segment, skip the superblock.
	 */
	if (lfs_sntod(fs, lfs_dtosn(fs, offset)) == offset) {
		LFS_SEGENTRY(sup, fs, lfs_dtosn(fs, offset), bp);
		if (sup->su_flags & SEGUSE_SUPERBLOCK)
			offset += lfs_btofsb(fs, LFS_SBPAD);
		brelse(bp, 0);
	}

	/* Read in the segment summary */
	error = bread(devvp, LFS_FSBTODB(fs, offset), lfs_sb_getsumsize(fs),
	    0, &bp);
	if (error)
		return -1;

	/* Check summary checksum */
	ssp = (SEGSUM *)bp->b_data;
	if (flags & CHECK_CKSUM) {
		size_t sumstart;

		sumstart = lfs_ss_getsumstart(fs);
		if (lfs_ss_getsumsum(fs, ssp) !=
		    cksum((char *)ssp + sumstart,
			  lfs_sb_getsumsize(fs) - sumstart)) {
			DLOG((DLOG_RF, "Sumsum error at 0x%" PRIx64 "\n", offset));
			offset = -1;
			goto err1;
		}
		if (lfs_ss_getnfinfo(fs, ssp) == 0 &&
		    lfs_ss_getninos(fs, ssp) == 0) {
			DLOG((DLOG_RF, "Empty pseg at 0x%" PRIx64 "\n", offset));
			offset = -1;
			goto err1;
		}
		if (lfs_ss_getcreate(fs, ssp) < lfs_sb_gettstamp(fs)) {
			DLOG((DLOG_RF, "Old data at 0x%" PRIx64 "\n", offset));
			offset = -1;
			goto err1;
		}
	}
	if (lfs_sb_getversion(fs) > 1) {
		if (lfs_ss_getserial(fs, ssp) != nextserial) {
			DLOG((DLOG_RF, "Unexpected serial number at 0x%" PRIx64
			      "\n", offset));
			offset = -1;
			goto err1;
		}
		if (lfs_ss_getident(fs, ssp) != lfs_sb_getident(fs)) {
			DLOG((DLOG_RF, "Incorrect fsid (0x%x vs 0x%x) at 0x%"
			      PRIx64 "\n", lfs_ss_getident(fs, ssp),
			      lfs_sb_getident(fs), offset));
			offset = -1;
			goto err1;
		}
	}
	if (pseg_flags)
		*pseg_flags = lfs_ss_getflags(fs, ssp);
	oldoffset = offset;
	offset += lfs_btofsb(fs, lfs_sb_getsumsize(fs));

	ninos = howmany(lfs_ss_getninos(fs, ssp), LFS_INOPB(fs));
	iip = SEGSUM_IINFOSTART(fs, bp->b_data);
	if (flags & CHECK_CKSUM) {
		/* Count blocks */
		nblocks = 0;
		fip = SEGSUM_FINFOBASE(fs, (SEGSUM *)bp->b_data);
		for (i = 0; i < lfs_ss_getnfinfo(fs, ssp); ++i) {
			nblocks += lfs_fi_getnblocks(fs, fip);
			if (lfs_fi_getnblocks(fs, fip) <= 0)
				break;
			fip = NEXT_FINFO(fs, fip);
		}
		nblocks += ninos;
		/* Create the sum array */
		datap = dp = malloc(nblocks * sizeof(u_long),
				    M_SEGMENT, M_WAITOK);
	}

	/* Handle individual blocks */
	fip = SEGSUM_FINFOBASE(fs, (SEGSUM *)bp->b_data);
	for (i = 0; i < lfs_ss_getnfinfo(fs, ssp) || ninos; ++i) {
		/* Inode block? */
		if (ninos && lfs_ii_getblock(fs, iip) == offset) {
			if (flags & CHECK_CKSUM) {
				/* Read in the head and add to the buffer */
				error = bread(devvp, LFS_FSBTODB(fs, offset), lfs_sb_getbsize(fs),
					      0, &dbp);
				if (error) {
					offset = -1;
					goto err2;
				}
				/* XXX this can't be right, on-disk u_long? */
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
			offset += lfs_btofsb(fs, lfs_sb_getibsize(fs));
			iip = NEXTLOWER_IINFO(fs, iip);
			--ninos;
			--i; /* compensate for ++i in loop header */
			continue;
		}
		size = lfs_sb_getbsize(fs);
		for (j = 0; j < lfs_fi_getnblocks(fs, fip); ++j) {
			if (j == lfs_fi_getnblocks(fs, fip) - 1)
				size = lfs_fi_getlastlength(fs, fip);
			if (flags & CHECK_CKSUM) {
				error = bread(devvp, LFS_FSBTODB(fs, offset), size,
				    0, &dbp);
				if (error) {
					offset = -1;
					goto err2;
				}
				(*dp++) = ((u_long *)(dbp->b_data))[0];
				brelse(dbp, BC_AGE);
			}
			/* Account for and update any direct blocks */
			if ((flags & CHECK_UPDATE) &&
			   lfs_fi_getino(fs, fip) > LFS_IFILE_INUM &&
			   lfs_fi_getblock(fs, fip, j) >= 0) {
				update_meta(fs, lfs_fi_getino(fs, fip),
					    lfs_fi_getversion(fs, fip),
					    lfs_fi_getblock(fs, fip, j),
					    offset, size, l);
			}
			offset += lfs_btofsb(fs, size);
		}
		fip = NEXT_FINFO(fs, fip);
	}
	/* Checksum the array, compare */
	datasum = lfs_ss_getdatasum(fs, ssp);
	foundsum = cksum(datap, nblocks * sizeof(u_long));
	if ((flags & CHECK_CKSUM) && datasum != foundsum) {
		DLOG((DLOG_RF, "Datasum error at 0x%" PRIx64
		      " (wanted %x got %x)\n",
		      offset, datasum, foundsum));
		offset = -1;
		goto err2;
	}

	/* If we're at the end of the segment, move to the next */
	if (lfs_dtosn(fs, offset + lfs_btofsb(fs, lfs_sb_getsumsize(fs) + lfs_sb_getbsize(fs))) !=
	   lfs_dtosn(fs, offset)) {
		if (lfs_dtosn(fs, offset) == lfs_dtosn(fs, lfs_ss_getnext(fs, ssp))) {
			offset = -1;
			goto err2;
		}
		offset = lfs_ss_getnext(fs, ssp);
		DLOG((DLOG_RF, "LFS roll forward: moving to offset 0x%" PRIx64
		       " -> segment %d\n", offset, lfs_dtosn(fs,offset)));
	}

	if (flags & CHECK_UPDATE) {
		lfs_sb_subavail(fs, offset - oldoffset);
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
	if ((flags & CHECK_UPDATE) && offset > 0 && lfs_sb_getversion(fs) > 1)
		lfs_sb_setserial(fs, nextserial);
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
	do_rollforward = (!(lfs_sb_getpflags(fs) & LFS_PF_CLEAN) &&
			  lfs_do_rfw && lfs_sb_getversion(fs) > 1 && p != NULL);
	if (do_rollforward) {
		u_int64_t nextserial;
		/*
		 * Phase I: Find the address of the last good partial
		 * segment that was written after the checkpoint.  Mark
		 * the segments in question dirty, so they won't be
		 * reallocated.
		 */
		lastgoodpseg = oldoffset = offset = lfs_sb_getoffset(fs);
		flags = 0x0;
		DLOG((DLOG_RF, "LFS roll forward phase 1: start at offset 0x%"
		      PRIx64 "\n", offset));
		LFS_SEGENTRY(sup, fs, lfs_dtosn(fs, offset), bp);
		if (!(sup->su_flags & SEGUSE_DIRTY))
			lfs_sb_subnclean(fs, 1);
		sup->su_flags |= SEGUSE_DIRTY;
		LFS_WRITESEGENTRY(sup, fs, lfs_dtosn(fs, offset), bp);
		nextserial = lfs_sb_getserial(fs) + 1;
		while ((offset = check_segsum(fs, offset, nextserial,
		    cred, CHECK_CKSUM, &flags, l)) > 0) {
			nextserial++;
			if (lfs_sntod(fs, oldoffset) != lfs_sntod(fs, offset)) {
				LFS_SEGENTRY(sup, fs, lfs_dtosn(fs, oldoffset),
					     bp);
				if (!(sup->su_flags & SEGUSE_DIRTY))
					lfs_sb_subnclean(fs, 1);
				sup->su_flags |= SEGUSE_DIRTY;
				LFS_WRITESEGENTRY(sup, fs, lfs_dtosn(fs, oldoffset),
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
		oldoffset = lfs_sb_getoffset(fs);
		if (lfs_sb_getoffset(fs) != lastgoodpseg) {
			/* Don't overwrite what we're trying to preserve */
			offset = lfs_sb_getoffset(fs);
			lfs_sb_setoffset(fs, lastgoodpseg);
			lfs_sb_setcurseg(fs, lfs_sntod(fs, lfs_dtosn(fs, lfs_sb_getoffset(fs))));
			for (sn = curseg = lfs_dtosn(fs, lfs_sb_getcurseg(fs));;) {
				sn = (sn + 1) % lfs_sb_getnseg(fs);
				if (sn == curseg)
					panic("lfs_mountfs: no clean segments");
				LFS_SEGENTRY(sup, fs, sn, bp);
				dirty = (sup->su_flags & SEGUSE_DIRTY);
				brelse(bp, 0);
				if (!dirty)
					break;
			}
			lfs_sb_setnextseg(fs, lfs_sntod(fs, sn));

			/*
			 * Phase II: Roll forward from the first superblock.
			 */
			while (offset != lastgoodpseg) {
				DLOG((DLOG_RF, "LFS roll forward phase 2: 0x%"
				      PRIx64 "\n", offset));
				offset = check_segsum(fs, offset,
				    lfs_sb_getserial(fs) + 1, cred, CHECK_UPDATE,
				    NULL, l);
			}

			/*
			 * Finish: flush our changes to disk.
			 */
			lfs_segwrite(mp, SEGM_CKP | SEGM_SYNC);
			DLOG((DLOG_RF, "lfs_mountfs: roll forward ",
			      "recovered %jd blocks\n",
			      (intmax_t)(lastgoodpseg - oldoffset)));
		}
		DLOG((DLOG_RF, "LFS roll forward complete\n"));
	}
}
