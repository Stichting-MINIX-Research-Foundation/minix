/*	$NetBSD: ffs_wapbl.c,v 1.30 2015/03/28 19:24:04 maxv Exp $	*/

/*-
 * Copyright (c) 2003,2006,2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Wasabi Systems, Inc.
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
__KERNEL_RCSID(0, "$NetBSD: ffs_wapbl.c,v 1.30 2015/03/28 19:24:04 maxv Exp $");

#define WAPBL_INTERNAL

#if defined(_KERNEL_OPT)
#include "opt_ffs.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/file.h>
#include <sys/disk.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/kauth.h>
#include <sys/wapbl.h>

#include <ufs/ufs/inode.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_bswap.h>
#include <ufs/ufs/ufs_extern.h>
#include <ufs/ufs/ufs_wapbl.h>

#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>

#undef	WAPBL_DEBUG
#ifdef WAPBL_DEBUG
int ffs_wapbl_debug = 1;
#define DPRINTF(fmt, args...)						\
do {									\
	if (ffs_wapbl_debug)						\
		printf("%s:%d "fmt, __func__ , __LINE__, ##args);	\
} while (/* CONSTCOND */0)
#else
#define	DPRINTF(fmt, args...)						\
do {									\
	/* nothing */							\
} while (/* CONSTCOND */0)
#endif

static int ffs_superblock_layout(struct fs *);
static int wapbl_log_position(struct mount *, struct fs *, struct vnode *,
    daddr_t *, size_t *, size_t *, uint64_t *);
static int wapbl_create_infs_log(struct mount *, struct fs *, struct vnode *,
    daddr_t *, size_t *, uint64_t *);
static void wapbl_find_log_start(struct mount *, struct vnode *, off_t,
    daddr_t *, daddr_t *, size_t *);
static int wapbl_remove_log(struct mount *);
static int wapbl_allocate_log_file(struct mount *, struct vnode *,
    daddr_t *, size_t *, uint64_t *);

/*
 * Return the super block layout format - UFS1 or UFS2.
 * WAPBL only works with UFS2 layout (which is still available
 * with FFSv1).
 *
 * XXX Should this be in ufs/ffs/fs.h?  Same style of check is
 * also used in ffs_alloc.c in a few places.
 */
static int
ffs_superblock_layout(struct fs *fs)
{
	if ((fs->fs_magic == FS_UFS1_MAGIC) &&
	    ((fs->fs_old_flags & FS_FLAGS_UPDATED) == 0))
		return 1;
	else
		return 2;
}

/*
 * This function is invoked after a log is replayed to
 * disk to perform logical cleanup actions as described by
 * the log
 */
void
ffs_wapbl_replay_finish(struct mount *mp)
{
	struct wapbl_replay *wr = mp->mnt_wapbl_replay;
	int i;
	int error;

	if (!wr)
		return;

	KDASSERT((mp->mnt_flag & MNT_RDONLY) == 0);

	for (i = 0; i < wr->wr_inodescnt; i++) {
		struct vnode *vp;
		struct inode *ip;
		error = VFS_VGET(mp, wr->wr_inodes[i].wr_inumber, &vp);
		if (error) {
			printf("ffs_wapbl_replay_finish: "
			    "unable to cleanup inode %" PRIu32 "\n",
			    wr->wr_inodes[i].wr_inumber);
			continue;
		}
		ip = VTOI(vp);
		KDASSERT(wr->wr_inodes[i].wr_inumber == ip->i_number);
#ifdef WAPBL_DEBUG
		printf("ffs_wapbl_replay_finish: "
		    "cleaning inode %" PRIu64 " size=%" PRIu64 " mode=%o nlink=%d\n",
		    ip->i_number, ip->i_size, ip->i_mode, ip->i_nlink);
#endif
		KASSERT(ip->i_nlink == 0);

		/*
		 * The journal may have left partially allocated inodes in mode
		 * zero.  This may occur if a crash occurs betweeen the node
		 * allocation in ffs_nodeallocg and when the node is properly
		 * initialized in ufs_makeinode.  If so, just dallocate them.
		 */
		if (ip->i_mode == 0) {
			error = UFS_WAPBL_BEGIN(mp);
			if (error) {
				printf("ffs_wapbl_replay_finish: "
				    "unable to cleanup inode %" PRIu32 "\n",
				    wr->wr_inodes[i].wr_inumber);
			} else {
				ffs_vfree(vp, ip->i_number,
				    wr->wr_inodes[i].wr_imode);
				UFS_WAPBL_END(mp);
			}
		}
		vput(vp);
	}
	wapbl_replay_stop(wr);
	wapbl_replay_free(wr);
	mp->mnt_wapbl_replay = NULL;
}

/* Callback for wapbl */
void
ffs_wapbl_sync_metadata(struct mount *mp, daddr_t *deallocblks,
    int *dealloclens, int dealloccnt)
{
	struct ufsmount *ump = VFSTOUFS(mp);
	struct fs *fs = ump->um_fs;
	int i, error __diagused;

#ifdef WAPBL_DEBUG_INODES
	ufs_wapbl_verify_inodes(mp, "ffs_wapbl_sync_metadata");
#endif

	for (i = 0; i< dealloccnt; i++) {
		/*
		 * blkfree errors are unreported, might silently fail
		 * if it cannot read the cylinder group block
		 */
		ffs_blkfree(fs, ump->um_devvp,
		    FFS_DBTOFSB(fs, deallocblks[i]), dealloclens[i], -1);
	}

	fs->fs_fmod = 0;
	fs->fs_time = time_second;
	error = ffs_cgupdate(ump, 0);
	KASSERT(error == 0);
}

void
ffs_wapbl_abort_sync_metadata(struct mount *mp, daddr_t *deallocblks,
    int *dealloclens, int dealloccnt)
{
	struct ufsmount *ump = VFSTOUFS(mp);
	struct fs *fs = ump->um_fs;
	int i;

	for (i = 0; i < dealloccnt; i++) {
		/*
		 * Since the above blkfree may have failed, this blkalloc might
		 * fail as well, so don't check its error.  Note that if the
		 * blkfree succeeded above, then this shouldn't fail because
		 * the buffer will be locked in the current transaction.
		 */
		ffs_blkalloc_ump(ump, FFS_DBTOFSB(fs, deallocblks[i]),
		    dealloclens[i]);
	}
}

static int
wapbl_remove_log(struct mount *mp)
{
	struct ufsmount *ump = VFSTOUFS(mp);
	struct fs *fs = ump->um_fs;
	struct vnode *vp;
	struct inode *ip;
	ino_t log_ino;
	int error;

	/* If super block layout is too old to support WAPBL, return */
	if (ffs_superblock_layout(fs) < 2)
		return 0;

	/* If all the log locators are 0, just clean up */
	if (fs->fs_journallocs[0] == 0 &&
	    fs->fs_journallocs[1] == 0 &&
	    fs->fs_journallocs[2] == 0 &&
	    fs->fs_journallocs[3] == 0) {
		DPRINTF("empty locators, just clear\n");
		goto done;
	}

	switch (fs->fs_journal_location) {
	case UFS_WAPBL_JOURNALLOC_NONE:
		/* nothing! */
		DPRINTF("no log\n");
		break;

	case UFS_WAPBL_JOURNALLOC_IN_FILESYSTEM:
		log_ino = fs->fs_journallocs[UFS_WAPBL_INFS_INO];
		DPRINTF("in-fs log, ino = %" PRId64 "\n",log_ino);

		/* if no existing log inode, just clear all fields and bail */
		if (log_ino == 0)
			goto done;
		error = VFS_VGET(mp, log_ino, &vp);
		if (error != 0) {
			printf("ffs_wapbl: vget failed %d\n",
			    error);
			/* clear out log info on error */
			goto done;
		}
		ip = VTOI(vp);
		KASSERT(log_ino == ip->i_number);
		if ((ip->i_flags & SF_LOG) == 0) {
			printf("ffs_wapbl: try to clear non-log inode "
			    "%" PRId64 "\n", log_ino);
			vput(vp);
			/* clear out log info on error */
			goto done;
		}

		/*
		 * remove the log inode by setting its link count back
		 * to zero and bail.
		 */
		ip->i_nlink = 0;
		DIP_ASSIGN(ip, nlink, 0);
		vput(vp);

	case UFS_WAPBL_JOURNALLOC_END_PARTITION:
		DPRINTF("end-of-partition log\n");
		/* no extra work required */
		break;

	default:
		printf("ffs_wapbl: unknown journal type %d\n",
		    fs->fs_journal_location);
		break;
	}


done:
	/* Clear out all previous knowledge of journal */
	fs->fs_journal_version = 0;
	fs->fs_journal_location = 0;
	fs->fs_journal_flags = 0;
	fs->fs_journallocs[0] = 0;
	fs->fs_journallocs[1] = 0;
	fs->fs_journallocs[2] = 0;
	fs->fs_journallocs[3] = 0;
	(void) ffs_sbupdate(ump, MNT_WAIT);

	return 0;
}

int
ffs_wapbl_start(struct mount *mp)
{
	struct ufsmount *ump = VFSTOUFS(mp);
	struct fs *fs = ump->um_fs;
	struct vnode *devvp = ump->um_devvp;
	daddr_t off;
	size_t count;
	size_t blksize;
	uint64_t extradata;
	int error;

	if (mp->mnt_wapbl == NULL) {
		if (fs->fs_journal_flags & UFS_WAPBL_FLAGS_CLEAR_LOG) {
			/* Clear out any existing journal file */
			error = wapbl_remove_log(mp);
			if (error != 0)
				return error;
		}

		if (mp->mnt_flag & MNT_LOG) {
			KDASSERT(fs->fs_ronly == 0);

			/* WAPBL needs UFS2 format super block */
			if (ffs_superblock_layout(fs) < 2) {
				printf("%s fs superblock in old format, "
				   "not journaling\n",
				   VFSTOUFS(mp)->um_fs->fs_fsmnt);
				mp->mnt_flag &= ~MNT_LOG;
				return EINVAL;
			}

			error = wapbl_log_position(mp, fs, devvp, &off,
			    &count, &blksize, &extradata);
			if (error)
				return error;

			error = wapbl_start(&mp->mnt_wapbl, mp, devvp, off,
			    count, blksize, mp->mnt_wapbl_replay,
			    ffs_wapbl_sync_metadata,
			    ffs_wapbl_abort_sync_metadata);
			if (error)
				return error;

			mp->mnt_wapbl_op = &wapbl_ops;

#ifdef WAPBL_DEBUG
			printf("%s: enabling logging\n", fs->fs_fsmnt);
#endif

			if ((fs->fs_flags & FS_DOWAPBL) == 0) {
				fs->fs_flags |= FS_DOWAPBL;
				if ((error = UFS_WAPBL_BEGIN(mp)) != 0)
					goto out;
				error = ffs_sbupdate(ump, MNT_WAIT);
				if (error) {
					UFS_WAPBL_END(mp);
					goto out;
				}
				UFS_WAPBL_END(mp);
				error = wapbl_flush(mp->mnt_wapbl, 1);
				if (error)
					goto out;
			}
		} else if (fs->fs_flags & FS_DOWAPBL) {
			fs->fs_fmod = 1;
			fs->fs_flags &= ~FS_DOWAPBL;
		}
	}

	/*
	 * It is recommended that you finish replay with logging enabled.
	 * However, even if logging is not enabled, the remaining log
	 * replay should be safely recoverable with an fsck, so perform
	 * it anyway.
	 */
	if ((fs->fs_ronly == 0) && mp->mnt_wapbl_replay) {
		int saveflag = mp->mnt_flag & MNT_RDONLY;
		/*
		 * Make sure MNT_RDONLY is not set so that the inode
		 * cleanup in ufs_inactive will actually do its work.
		 */
		mp->mnt_flag &= ~MNT_RDONLY;
		ffs_wapbl_replay_finish(mp);
		mp->mnt_flag |= saveflag;
		KASSERT(fs->fs_ronly == 0);
	}

	return 0;
out:
	ffs_wapbl_stop(mp, MNT_FORCE);
	return error;
}

int
ffs_wapbl_stop(struct mount *mp, int force)
{
	struct ufsmount *ump = VFSTOUFS(mp);
	struct fs *fs = ump->um_fs;
	int error;

	if (mp->mnt_wapbl) {
		KDASSERT(fs->fs_ronly == 0);

		/*
		 * Make sure turning off FS_DOWAPBL is only removed
		 * as the only change in the final flush since otherwise
		 * a transaction may reorder writes.
		 */
		error = wapbl_flush(mp->mnt_wapbl, 1);
		if (error && !force)
			return error;
		if (error && force)
			goto forceout;
		error = UFS_WAPBL_BEGIN(mp);
		if (error && !force)
			return error;
		if (error && force)
			goto forceout;
		KASSERT(fs->fs_flags & FS_DOWAPBL);

		fs->fs_flags &= ~FS_DOWAPBL;
		error = ffs_sbupdate(ump, MNT_WAIT);
		KASSERT(error == 0);	/* XXX a bit drastic! */
		UFS_WAPBL_END(mp);
	forceout:
		error = wapbl_stop(mp->mnt_wapbl, force);
		if (error) {
			KASSERT(!force);
			fs->fs_flags |= FS_DOWAPBL;
			return error;
		}
		fs->fs_flags &= ~FS_DOWAPBL; /* Repeat in case of forced error */
		mp->mnt_wapbl = NULL;

#ifdef WAPBL_DEBUG
		printf("%s: disabled logging\n", fs->fs_fsmnt);
#endif
	}

	return 0;
}

int
ffs_wapbl_replay_start(struct mount *mp, struct fs *fs, struct vnode *devvp)
{
	int error;
	daddr_t off;
	size_t count;
	size_t blksize;
	uint64_t extradata;

	/*
	 * WAPBL needs UFS2 format super block, if we got here with a
	 * UFS1 format super block something is amiss...
	 */
	if (ffs_superblock_layout(fs) < 2)
		return EINVAL;

	error = wapbl_log_position(mp, fs, devvp, &off, &count, &blksize,
	    &extradata);

	if (error)
		return error;

	error = wapbl_replay_start(&mp->mnt_wapbl_replay, devvp, off,
		count, blksize);
	if (error)
		return error;

	mp->mnt_wapbl_op = &wapbl_ops;

	return 0;
}

/*
 * If the superblock doesn't already have a recorded journal location
 * then we allocate the journal in one of two positions:
 *
 *  - At the end of the partition after the filesystem if there's
 *    enough space.  "Enough space" is defined as >= 1MB of journal
 *    per 1GB of filesystem or 64MB, whichever is smaller.
 *
 *  - Inside the filesystem.  We try to allocate a contiguous journal
 *    based on the total filesystem size - the target is 1MB of journal
 *    per 1GB of filesystem, up to a maximum journal size of 64MB.  As
 *    a worst case allowing for fragmentation, we'll allocate a journal
 *    1/4 of the desired size but never smaller than 1MB.
 *
 *    XXX In the future if we allow for non-contiguous journal files we
 *    can tighten the above restrictions.
 *
 * XXX
 * These seems like a lot of duplication both here and in some of
 * the userland tools (fsck_ffs, dumpfs, tunefs) with similar 
 * "switch (fs_journal_location)" constructs.  Can we centralise
 * this sort of code somehow/somewhere?
 */
static int
wapbl_log_position(struct mount *mp, struct fs *fs, struct vnode *devvp,
    daddr_t *startp, size_t *countp, size_t *blksizep, uint64_t *extradatap)
{
	struct ufsmount *ump = VFSTOUFS(mp);
	daddr_t logstart, logend, desired_logsize;
	uint64_t numsecs;
	unsigned secsize;
	int error, location;

	if (fs->fs_journal_version == UFS_WAPBL_VERSION) {
		switch (fs->fs_journal_location) {
		case UFS_WAPBL_JOURNALLOC_END_PARTITION:
			DPRINTF("found existing end-of-partition log\n");
			*startp = fs->fs_journallocs[UFS_WAPBL_EPART_ADDR];
			*countp = fs->fs_journallocs[UFS_WAPBL_EPART_COUNT];
			*blksizep = fs->fs_journallocs[UFS_WAPBL_EPART_BLKSZ];
			DPRINTF(" start = %" PRId64 ", size = %zu, "
			    "blksize = %zu\n", *startp, *countp, *blksizep);
			return 0;

		case UFS_WAPBL_JOURNALLOC_IN_FILESYSTEM:
			DPRINTF("found existing in-filesystem log\n");
			*startp = fs->fs_journallocs[UFS_WAPBL_INFS_ADDR];
			*countp = fs->fs_journallocs[UFS_WAPBL_INFS_COUNT];
			*blksizep = fs->fs_journallocs[UFS_WAPBL_INFS_BLKSZ];
			DPRINTF(" start = %" PRId64 ", size = %zu, "
			    "blksize = %zu\n", *startp, *countp, *blksizep);
			return 0;

		default:
			printf("ffs_wapbl: unknown journal type %d\n",
			    fs->fs_journal_location);
			return EINVAL;
		}
	}

	desired_logsize =
	    ffs_lfragtosize(fs, fs->fs_size) / UFS_WAPBL_JOURNAL_SCALE;
	DPRINTF("desired log size = %" PRId64 " kB\n", desired_logsize / 1024);
	desired_logsize = max(desired_logsize, UFS_WAPBL_MIN_JOURNAL_SIZE);
	desired_logsize = min(desired_logsize, UFS_WAPBL_MAX_JOURNAL_SIZE);
	DPRINTF("adjusted desired log size = %" PRId64 " kB\n",
	    desired_logsize / 1024);

	/* Is there space after after filesystem on partition for log? */
	logstart = FFS_FSBTODB(fs, fs->fs_size);
	error = getdisksize(devvp, &numsecs, &secsize);
	if (error)
		return error;
	KDASSERT(secsize != 0);
	logend = btodb(numsecs * secsize);

	if (dbtob(logend - logstart) >= desired_logsize) {
		DPRINTF("enough space, use end-of-partition log\n");

		location = UFS_WAPBL_JOURNALLOC_END_PARTITION;
		*blksizep = secsize;

		*startp = logstart;
		*countp = (logend - logstart);
		*extradatap = 0;

		/* convert to physical block numbers */
		*startp = dbtob(*startp) / secsize;
		*countp = dbtob(*countp) / secsize;

		fs->fs_journallocs[UFS_WAPBL_EPART_ADDR] = *startp;
		fs->fs_journallocs[UFS_WAPBL_EPART_COUNT] = *countp;
		fs->fs_journallocs[UFS_WAPBL_EPART_BLKSZ] = *blksizep;
		fs->fs_journallocs[UFS_WAPBL_EPART_UNUSED] = *extradatap;
	} else {
		DPRINTF("end-of-partition has only %" PRId64 " free\n",
		    logend - logstart);

		location = UFS_WAPBL_JOURNALLOC_IN_FILESYSTEM;
		*blksizep = secsize;

		error = wapbl_create_infs_log(mp, fs, devvp,
		                  startp, countp, extradatap);
		ffs_sync(mp, MNT_WAIT, FSCRED);

		/* convert to physical block numbers */
		*startp = dbtob(*startp) / secsize;
		*countp = dbtob(*countp) / secsize;

		fs->fs_journallocs[UFS_WAPBL_INFS_ADDR] = *startp;
		fs->fs_journallocs[UFS_WAPBL_INFS_COUNT] = *countp;
		fs->fs_journallocs[UFS_WAPBL_INFS_BLKSZ] = *blksizep;
		fs->fs_journallocs[UFS_WAPBL_INFS_INO] = *extradatap;
	}

	if (error == 0) {
		/* update superblock with log location */
		fs->fs_journal_version = UFS_WAPBL_VERSION;
		fs->fs_journal_location = location;
		fs->fs_journal_flags = 0;

		error = ffs_sbupdate(ump, MNT_WAIT);
	}

	return error;
}

/*
 * Try to create a journal log inside the filesystem.
 */
static int
wapbl_create_infs_log(struct mount *mp, struct fs *fs, struct vnode *devvp,
    daddr_t *startp, size_t *countp, uint64_t *extradatap)
{
	struct vnode *vp, *rvp;
	struct vattr va;
	struct inode *ip;
	int error;

	if ((error = VFS_ROOT(mp, &rvp)) != 0)
		return error;

	vattr_null(&va);
	va.va_type = VREG;
	va.va_mode = 0;

	error = vcache_new(mp, rvp, &va, NOCRED, &vp);
	vput(rvp);
	if (error)
		return error;

	error = vn_lock(vp, LK_EXCLUSIVE);
	if (error) {
		vrele(vp);
		return error;
	}

	ip = VTOI(vp);
	ip->i_flags = SF_LOG;
	DIP_ASSIGN(ip, flags, ip->i_flags);
	ip->i_nlink = 1;
	DIP_ASSIGN(ip, nlink, 1);
	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
	ffs_update(vp, NULL, NULL, UPDATE_WAIT);

	if ((error = wapbl_allocate_log_file(mp, vp,
	                 startp, countp, extradatap)) != 0) {
		/*
		 * If we couldn't allocate the space for the log file,
		 * remove the inode by setting its link count back to
		 * zero and bail.
		 */
		ip->i_nlink = 0;
		DIP_ASSIGN(ip, nlink, 0);
		VOP_UNLOCK(vp);
		vgone(vp);

		return error;
	}

	/*
	 * Now that we have the place-holder inode for the journal,
	 * we don't need the vnode ever again.
	 */
	VOP_UNLOCK(vp);
	vgone(vp);

	return 0;
}

int
wapbl_allocate_log_file(struct mount *mp, struct vnode *vp,
    daddr_t *startp, size_t *countp, uint64_t *extradatap)
{
	struct ufsmount *ump = VFSTOUFS(mp);
	struct fs *fs = ump->um_fs;
	daddr_t addr, indir_addr;
	off_t logsize;
	size_t size;
	int error;

	logsize = 0;
	/* check if there's a suggested log size */
	if (fs->fs_journal_flags & UFS_WAPBL_FLAGS_CREATE_LOG &&
	    fs->fs_journal_location == UFS_WAPBL_JOURNALLOC_IN_FILESYSTEM)
		logsize = fs->fs_journallocs[UFS_WAPBL_INFS_COUNT];

	if (vp->v_size > 0) {
		printf("%s: file size (%" PRId64 ") non zero\n", __func__,
		    vp->v_size);
		return EEXIST;
	}
	wapbl_find_log_start(mp, vp, logsize, &addr, &indir_addr, &size);
	if (addr == 0) {
		printf("%s: log not allocated, largest extent is "
		    "%" PRId64 "MB\n", __func__,
		    ffs_lblktosize(fs, size) / (1024 * 1024));
		return ENOSPC;
	}

	logsize = ffs_lblktosize(fs, size);	/* final log size */

	VTOI(vp)->i_ffs_first_data_blk = addr;
	VTOI(vp)->i_ffs_first_indir_blk = indir_addr;

	error = GOP_ALLOC(vp, 0, logsize, B_CONTIG, FSCRED);
	if (error) {
		printf("%s: GOP_ALLOC error %d\n", __func__, error);
		return error;
	}

	*startp     = FFS_FSBTODB(fs, addr);
	*countp     = btodb(logsize);
	*extradatap = VTOI(vp)->i_number;

	return 0;
}

/*
 * Find a suitable location for the journal in the filesystem.
 *
 * Our strategy here is to look for a contiguous block of free space
 * at least "logfile" MB in size (plus room for any indirect blocks).
 * We start at the middle of the filesystem and check each cylinder
 * group working outwards.  If "logfile" MB is not available as a
 * single contigous chunk, then return the address and size of the
 * largest chunk found.
 *
 * XXX 
 * At what stage does the search fail?  Is if the largest space we could
 * find is less than a quarter the requested space reasonable?  If the
 * search fails entirely, return a block address if "0" it indicate this.
 */
static void
wapbl_find_log_start(struct mount *mp, struct vnode *vp, off_t logsize,
    daddr_t *addr, daddr_t *indir_addr, size_t *size)
{
	struct ufsmount *ump = VFSTOUFS(mp);
	struct fs *fs = ump->um_fs;
	struct vnode *devvp = ump->um_devvp;
	struct cg *cgp;
	struct buf *bp;
	uint8_t *blksfree;
	daddr_t blkno, best_addr, start_addr;
	daddr_t desired_blks, min_desired_blks;
	daddr_t freeblks, best_blks;
	int bpcg, cg, error, fixedsize, indir_blks, n, s;
	const int needswap = UFS_FSNEEDSWAP(fs);

	if (logsize == 0) {
		fixedsize = 0;	/* We can adjust the size if tight */
		logsize = ffs_lfragtosize(fs, fs->fs_dsize) /
		    UFS_WAPBL_JOURNAL_SCALE;
		DPRINTF("suggested log size = %" PRId64 "\n", logsize);
		logsize = max(logsize, UFS_WAPBL_MIN_JOURNAL_SIZE);
		logsize = min(logsize, UFS_WAPBL_MAX_JOURNAL_SIZE);
		DPRINTF("adjusted log size = %" PRId64 "\n", logsize);
	} else {
		fixedsize = 1;
		DPRINTF("fixed log size = %" PRId64 "\n", logsize);
	}

	desired_blks = logsize / fs->fs_bsize;
	DPRINTF("desired blocks = %" PRId64 "\n", desired_blks);

	/* add in number of indirect blocks needed */
	indir_blks = 0;
	if (desired_blks >= UFS_NDADDR) {
		struct indir indirs[UFS_NIADDR + 2];
		int num;

		error = ufs_getlbns(vp, desired_blks, indirs, &num);
		if (error) {
			printf("%s: ufs_getlbns failed, error %d!\n",
			    __func__, error);
			goto bad;
		}

		switch (num) {
		case 2:
			indir_blks = 1;		/* 1st level indirect */
			break;
		case 3:
			indir_blks = 1 +	/* 1st level indirect */
			    1 +			/* 2nd level indirect */
			    indirs[1].in_off + 1; /* extra 1st level indirect */
			break;
		default:
			printf("%s: unexpected numlevels %d from ufs_getlbns\n",
			    __func__, num);
			*size = 0;
			goto bad;
		}
		desired_blks += indir_blks;
	}
	DPRINTF("desired blocks = %" PRId64 " (including indirect)\n",
	    desired_blks);

	/*
	 * If a specific size wasn't requested, allow for a smaller log
	 * if we're really tight for space...
	 */
	min_desired_blks = desired_blks;
	if (!fixedsize)
		min_desired_blks = desired_blks / 4;

	/* Look at number of blocks per CG.  If it's too small, bail early. */
	bpcg = ffs_fragstoblks(fs, fs->fs_fpg);
	if (min_desired_blks > bpcg) {
		printf("ffs_wapbl: cylinder group size of %" PRId64 " MB "
		    " is not big enough for journal\n",
		    ffs_lblktosize(fs, bpcg) / (1024 * 1024));
		goto bad;
	}

	/*
	 * Start with the middle cylinder group, and search outwards in
	 * both directions until we either find the requested log size
	 * or reach the start/end of the file system.  If we reach the
	 * start/end without finding enough space for the full requested
	 * log size, use the largest extent found if it is large enough
	 * to satisfy the our minimum size.
	 *
	 * XXX
	 * Can we just use the cluster contigsum stuff (esp on UFS2)
	 * here to simplify this search code?
	 */
	best_addr = 0;
	best_blks = 0;
	for (cg = fs->fs_ncg / 2, s = 0, n = 1;
	    best_blks < desired_blks && cg >= 0 && cg < fs->fs_ncg;
	    s++, n = -n, cg += n * s) {
		DPRINTF("check cg %d of %d\n", cg, fs->fs_ncg);
		error = bread(devvp, FFS_FSBTODB(fs, cgtod(fs, cg)),
		    fs->fs_cgsize, 0, &bp);
		if (error) {
			continue;
		}
		cgp = (struct cg *)bp->b_data;
		if (!cg_chkmagic(cgp, UFS_FSNEEDSWAP(fs))) {
			brelse(bp, 0);
			continue;
		}

		blksfree = cg_blksfree(cgp, needswap);

		for (blkno = 0; blkno < bpcg;) {
			/* look for next free block */
			/* XXX use scanc() and fragtbl[] here? */
			for (; blkno < bpcg - min_desired_blks; blkno++)
				if (ffs_isblock(fs, blksfree, blkno))
					break;

			/* past end of search space in this CG? */
			if (blkno >= bpcg - min_desired_blks)
				break;

			/* count how many free blocks in this extent */
			start_addr = blkno;
			for (freeblks = 0; blkno < bpcg; blkno++, freeblks++)
				if (!ffs_isblock(fs, blksfree, blkno))
					break;

			if (freeblks > best_blks) {
				best_blks = freeblks;
				best_addr = ffs_blkstofrags(fs, start_addr) +
				    cgbase(fs, cg);

				if (freeblks >= desired_blks) {
					DPRINTF("found len %" PRId64
					    " at offset %" PRId64 " in gc\n",
					    freeblks, start_addr);
					break;
				}
			}
		}
		brelse(bp, 0);
	}
	DPRINTF("best found len = %" PRId64 ", wanted %" PRId64
	    " at addr %" PRId64 "\n", best_blks, desired_blks, best_addr);

	if (best_blks < min_desired_blks) {
		*addr = 0;
		*indir_addr = 0;
	} else {
		/* put indirect blocks at start, and data blocks after */
		*addr = best_addr + ffs_blkstofrags(fs, indir_blks);
		*indir_addr = best_addr;
	}
	*size = min(desired_blks, best_blks) - indir_blks;
	return;

bad:
	*addr = 0;
	*indir_addr = 0;
	*size = 0;
	return;
}
