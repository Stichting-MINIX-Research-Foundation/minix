/*	$NetBSD: ffs_vnops.c,v 1.120 2011/06/27 16:34:47 manu Exp $	*/

/*-
 * Copyright (c) 2008, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Wasabi Systems, Inc, and by Andrew Doran.
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
 *	@(#)ffs_vnops.c	8.15 (Berkeley) 5/14/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ffs_vnops.c,v 1.120 2011/06/27 16:34:47 manu Exp $");

#if defined(_KERNEL_OPT)
#include "opt_ffs.h"
#include "opt_wapbl.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/resourcevar.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/event.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/pool.h>
#include <sys/signalvar.h>
#include <sys/kauth.h>
#include <sys/wapbl.h>
#include <sys/fstrans.h>

#include <miscfs/fifofs/fifo.h>
#include <miscfs/genfs/genfs.h>
#include <miscfs/specfs/specdev.h>

#include <ufs/ufs/inode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ufs/ufs_extern.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_wapbl.h>

#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>

#include <uvm/uvm.h>

/* Global vfs data structures for ufs. */
int (**ffs_vnodeop_p)(void *);
const struct vnodeopv_entry_desc ffs_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, ufs_lookup },		/* lookup */
	{ &vop_create_desc, ufs_create },		/* create */
	{ &vop_whiteout_desc, ufs_whiteout },		/* whiteout */
	{ &vop_mknod_desc, ufs_mknod },			/* mknod */
	{ &vop_open_desc, ufs_open },			/* open */
	{ &vop_close_desc, ufs_close },			/* close */
	{ &vop_access_desc, ufs_access },		/* access */
	{ &vop_getattr_desc, ufs_getattr },		/* getattr */
	{ &vop_setattr_desc, ufs_setattr },		/* setattr */
	{ &vop_read_desc, ffs_read },			/* read */
	{ &vop_write_desc, ffs_write },			/* write */
	{ &vop_ioctl_desc, ufs_ioctl },			/* ioctl */
	{ &vop_fcntl_desc, ufs_fcntl },			/* fcntl */
	{ &vop_poll_desc, ufs_poll },			/* poll */
	{ &vop_kqfilter_desc, genfs_kqfilter },		/* kqfilter */
	{ &vop_revoke_desc, ufs_revoke },		/* revoke */
	{ &vop_mmap_desc, ufs_mmap },			/* mmap */
	{ &vop_fsync_desc, ffs_fsync },			/* fsync */
	{ &vop_seek_desc, ufs_seek },			/* seek */
	{ &vop_remove_desc, ufs_remove },		/* remove */
	{ &vop_link_desc, ufs_link },			/* link */
	{ &vop_rename_desc, ufs_rename },		/* rename */
	{ &vop_mkdir_desc, ufs_mkdir },			/* mkdir */
	{ &vop_rmdir_desc, ufs_rmdir },			/* rmdir */
	{ &vop_symlink_desc, ufs_symlink },		/* symlink */
	{ &vop_readdir_desc, ufs_readdir },		/* readdir */
	{ &vop_readlink_desc, ufs_readlink },		/* readlink */
	{ &vop_abortop_desc, ufs_abortop },		/* abortop */
	{ &vop_inactive_desc, ufs_inactive },		/* inactive */
	{ &vop_reclaim_desc, ffs_reclaim },		/* reclaim */
	{ &vop_lock_desc, ufs_lock },			/* lock */
	{ &vop_unlock_desc, ufs_unlock },		/* unlock */
	{ &vop_bmap_desc, ufs_bmap },			/* bmap */
	{ &vop_strategy_desc, ufs_strategy },		/* strategy */
	{ &vop_print_desc, ufs_print },			/* print */
	{ &vop_islocked_desc, ufs_islocked },		/* islocked */
	{ &vop_pathconf_desc, ufs_pathconf },		/* pathconf */
	{ &vop_advlock_desc, ufs_advlock },		/* advlock */
	{ &vop_bwrite_desc, vn_bwrite },		/* bwrite */
	{ &vop_getpages_desc, genfs_getpages },		/* getpages */
	{ &vop_putpages_desc, genfs_putpages },		/* putpages */
	{ &vop_openextattr_desc, ffs_openextattr },	/* openextattr */
	{ &vop_closeextattr_desc, ffs_closeextattr },	/* closeextattr */
	{ &vop_getextattr_desc, ffs_getextattr },	/* getextattr */
	{ &vop_setextattr_desc, ffs_setextattr },	/* setextattr */
	{ &vop_listextattr_desc, ffs_listextattr },	/* listextattr */
	{ &vop_deleteextattr_desc, ffs_deleteextattr },	/* deleteextattr */
	{ NULL, NULL }
};
const struct vnodeopv_desc ffs_vnodeop_opv_desc =
	{ &ffs_vnodeop_p, ffs_vnodeop_entries };

int (**ffs_specop_p)(void *);
const struct vnodeopv_entry_desc ffs_specop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, spec_lookup },		/* lookup */
	{ &vop_create_desc, spec_create },		/* create */
	{ &vop_mknod_desc, spec_mknod },		/* mknod */
	{ &vop_open_desc, spec_open },			/* open */
	{ &vop_close_desc, ufsspec_close },		/* close */
	{ &vop_access_desc, ufs_access },		/* access */
	{ &vop_getattr_desc, ufs_getattr },		/* getattr */
	{ &vop_setattr_desc, ufs_setattr },		/* setattr */
	{ &vop_read_desc, ufsspec_read },		/* read */
	{ &vop_write_desc, ufsspec_write },		/* write */
	{ &vop_ioctl_desc, spec_ioctl },		/* ioctl */
	{ &vop_fcntl_desc, ufs_fcntl },			/* fcntl */
	{ &vop_poll_desc, spec_poll },			/* poll */
	{ &vop_kqfilter_desc, spec_kqfilter },		/* kqfilter */
	{ &vop_revoke_desc, spec_revoke },		/* revoke */
	{ &vop_mmap_desc, spec_mmap },			/* mmap */
	{ &vop_fsync_desc, ffs_spec_fsync },		/* fsync */
	{ &vop_seek_desc, spec_seek },			/* seek */
	{ &vop_remove_desc, spec_remove },		/* remove */
	{ &vop_link_desc, spec_link },			/* link */
	{ &vop_rename_desc, spec_rename },		/* rename */
	{ &vop_mkdir_desc, spec_mkdir },		/* mkdir */
	{ &vop_rmdir_desc, spec_rmdir },		/* rmdir */
	{ &vop_symlink_desc, spec_symlink },		/* symlink */
	{ &vop_readdir_desc, spec_readdir },		/* readdir */
	{ &vop_readlink_desc, spec_readlink },		/* readlink */
	{ &vop_abortop_desc, spec_abortop },		/* abortop */
	{ &vop_inactive_desc, ufs_inactive },		/* inactive */
	{ &vop_reclaim_desc, ffs_reclaim },		/* reclaim */
	{ &vop_lock_desc, ufs_lock },			/* lock */
	{ &vop_unlock_desc, ufs_unlock },		/* unlock */
	{ &vop_bmap_desc, spec_bmap },			/* bmap */
	{ &vop_strategy_desc, spec_strategy },		/* strategy */
	{ &vop_print_desc, ufs_print },			/* print */
	{ &vop_islocked_desc, ufs_islocked },		/* islocked */
	{ &vop_pathconf_desc, spec_pathconf },		/* pathconf */
	{ &vop_advlock_desc, spec_advlock },		/* advlock */
	{ &vop_bwrite_desc, vn_bwrite },		/* bwrite */
	{ &vop_getpages_desc, spec_getpages },		/* getpages */
	{ &vop_putpages_desc, spec_putpages },		/* putpages */
	{ &vop_openextattr_desc, ffs_openextattr },	/* openextattr */
	{ &vop_closeextattr_desc, ffs_closeextattr },	/* closeextattr */
	{ &vop_getextattr_desc, ffs_getextattr },	/* getextattr */
	{ &vop_setextattr_desc, ffs_setextattr },	/* setextattr */
	{ &vop_listextattr_desc, ffs_listextattr },	/* listextattr */
	{ &vop_deleteextattr_desc, ffs_deleteextattr },	/* deleteextattr */
	{ NULL, NULL }
};
const struct vnodeopv_desc ffs_specop_opv_desc =
	{ &ffs_specop_p, ffs_specop_entries };

int (**ffs_fifoop_p)(void *);
const struct vnodeopv_entry_desc ffs_fifoop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, vn_fifo_bypass },		/* lookup */
	{ &vop_create_desc, vn_fifo_bypass },		/* create */
	{ &vop_mknod_desc, vn_fifo_bypass },		/* mknod */
	{ &vop_open_desc, vn_fifo_bypass },		/* open */
	{ &vop_close_desc, ufsfifo_close },		/* close */
	{ &vop_access_desc, ufs_access },		/* access */
	{ &vop_getattr_desc, ufs_getattr },		/* getattr */
	{ &vop_setattr_desc, ufs_setattr },		/* setattr */
	{ &vop_read_desc, ufsfifo_read },		/* read */
	{ &vop_write_desc, ufsfifo_write },		/* write */
	{ &vop_ioctl_desc, vn_fifo_bypass },		/* ioctl */
	{ &vop_fcntl_desc, ufs_fcntl },			/* fcntl */
	{ &vop_poll_desc, vn_fifo_bypass },		/* poll */
	{ &vop_kqfilter_desc, vn_fifo_bypass },		/* kqfilter */
	{ &vop_revoke_desc, vn_fifo_bypass },		/* revoke */
	{ &vop_mmap_desc, vn_fifo_bypass },		/* mmap */
	{ &vop_fsync_desc, ffs_fsync },			/* fsync */
	{ &vop_seek_desc, vn_fifo_bypass },		/* seek */
	{ &vop_remove_desc, vn_fifo_bypass },		/* remove */
	{ &vop_link_desc, vn_fifo_bypass },		/* link */
	{ &vop_rename_desc, vn_fifo_bypass },		/* rename */
	{ &vop_mkdir_desc, vn_fifo_bypass },		/* mkdir */
	{ &vop_rmdir_desc, vn_fifo_bypass },		/* rmdir */
	{ &vop_symlink_desc, vn_fifo_bypass },		/* symlink */
	{ &vop_readdir_desc, vn_fifo_bypass },		/* readdir */
	{ &vop_readlink_desc, vn_fifo_bypass },		/* readlink */
	{ &vop_abortop_desc, vn_fifo_bypass },		/* abortop */
	{ &vop_inactive_desc, ufs_inactive },		/* inactive */
	{ &vop_reclaim_desc, ffs_reclaim },		/* reclaim */
	{ &vop_lock_desc, ufs_lock },			/* lock */
	{ &vop_unlock_desc, ufs_unlock },		/* unlock */
	{ &vop_bmap_desc, vn_fifo_bypass },		/* bmap */
	{ &vop_strategy_desc, vn_fifo_bypass },		/* strategy */
	{ &vop_print_desc, ufs_print },			/* print */
	{ &vop_islocked_desc, ufs_islocked },		/* islocked */
	{ &vop_pathconf_desc, vn_fifo_bypass },		/* pathconf */
	{ &vop_advlock_desc, vn_fifo_bypass },		/* advlock */
	{ &vop_bwrite_desc, vn_bwrite },		/* bwrite */
	{ &vop_putpages_desc, vn_fifo_bypass }, 	/* putpages */
	{ &vop_openextattr_desc, ffs_openextattr },	/* openextattr */
	{ &vop_closeextattr_desc, ffs_closeextattr },	/* closeextattr */
	{ &vop_getextattr_desc, ffs_getextattr },	/* getextattr */
	{ &vop_setextattr_desc, ffs_setextattr },	/* setextattr */
	{ &vop_listextattr_desc, ffs_listextattr },	/* listextattr */
	{ &vop_deleteextattr_desc, ffs_deleteextattr },	/* deleteextattr */
	{ NULL, NULL }
};
const struct vnodeopv_desc ffs_fifoop_opv_desc =
	{ &ffs_fifoop_p, ffs_fifoop_entries };

#include <ufs/ufs/ufs_readwrite.c>

int
ffs_spec_fsync(void *v)
{
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		kauth_cred_t a_cred;
		int a_flags;
		off_t a_offlo;
		off_t a_offhi;
		struct lwp *a_l;
	} */ *ap = v;
	int error, flags, uflags;
	struct vnode *vp;
	struct mount *mp;

	flags = ap->a_flags;
	uflags = UPDATE_CLOSE | ((flags & FSYNC_WAIT) ? UPDATE_WAIT : 0);
	vp = ap->a_vp;
	mp = vp->v_mount;

	fstrans_start(mp, FSTRANS_LAZY);

	error = spec_fsync(v);
	if (error)
		goto out;

#ifdef WAPBL
	if (mp && mp->mnt_wapbl) {
		/*
		 * Don't bother writing out metadata if the syncer is
		 * making the request.  We will let the sync vnode
		 * write it out in a single burst through a call to
		 * VFS_SYNC().
		 */
		if ((flags & (FSYNC_DATAONLY | FSYNC_LAZY)) != 0)
			goto out;
		if ((VTOI(vp)->i_flag & (IN_ACCESS | IN_CHANGE | IN_UPDATE
		    | IN_MODIFY | IN_MODIFIED | IN_ACCESSED)) != 0) {
			error = UFS_WAPBL_BEGIN(mp);
			if (error != 0)
				goto out;
			error = ffs_update(vp, NULL, NULL, uflags);
			UFS_WAPBL_END(mp);
		}
		goto out;
	}
#endif /* WAPBL */

	error = ffs_update(vp, NULL, NULL, uflags);

out:
	fstrans_done(mp);
	return error;
}

int
ffs_fsync(void *v)
{
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		kauth_cred_t a_cred;
		int a_flags;
		off_t a_offlo;
		off_t a_offhi;
		struct lwp *a_l;
	} */ *ap = v;
	struct buf *bp;
	int num, error, i;
	struct indir ia[NIADDR + 1];
	int bsize;
	daddr_t blk_high;
	struct vnode *vp;
	struct mount *mp;

	vp = ap->a_vp;
	mp = vp->v_mount;

	fstrans_start(mp, FSTRANS_LAZY);
	if ((ap->a_offlo == 0 && ap->a_offhi == 0) || (vp->v_type != VREG)) {
		error = ffs_full_fsync(vp, ap->a_flags);
		goto out;
	}

	bsize = mp->mnt_stat.f_iosize;
	blk_high = ap->a_offhi / bsize;
	if (ap->a_offhi % bsize != 0)
		blk_high++;

	/*
	 * First, flush all pages in range.
	 */

	mutex_enter(vp->v_interlock);
	error = VOP_PUTPAGES(vp, trunc_page(ap->a_offlo),
	    round_page(ap->a_offhi), PGO_CLEANIT |
	    ((ap->a_flags & FSYNC_WAIT) ? PGO_SYNCIO : 0));
	if (error) {
		goto out;
	}

#ifdef WAPBL
	KASSERT(vp->v_type == VREG);
	if (mp->mnt_wapbl) {
		/*
		 * Don't bother writing out metadata if the syncer is
		 * making the request.  We will let the sync vnode
		 * write it out in a single burst through a call to
		 * VFS_SYNC().
		 */
		if ((ap->a_flags & (FSYNC_DATAONLY | FSYNC_LAZY)) != 0) {
			fstrans_done(mp);
			return 0;
		}
		error = 0;
		if (vp->v_tag == VT_UFS && VTOI(vp)->i_flag &
		    (IN_ACCESS | IN_CHANGE | IN_UPDATE | IN_MODIFY |
				 IN_MODIFIED | IN_ACCESSED)) {
			error = UFS_WAPBL_BEGIN(mp);
			if (error) {
				fstrans_done(mp);
				return error;
			}
			error = ffs_update(vp, NULL, NULL, UPDATE_CLOSE |
			    ((ap->a_flags & FSYNC_WAIT) ? UPDATE_WAIT : 0));
			UFS_WAPBL_END(mp);
		}
		if (error || (ap->a_flags & FSYNC_NOLOG) != 0) {
			fstrans_done(mp);
			return error;
		}
		error = wapbl_flush(mp->mnt_wapbl, 0);
		fstrans_done(mp);
		return error;
	}
#endif /* WAPBL */

	/*
	 * Then, flush indirect blocks.
	 */

	if (blk_high >= NDADDR) {
		error = ufs_getlbns(vp, blk_high, ia, &num);
		if (error)
			goto out;

		mutex_enter(&bufcache_lock);
		for (i = 0; i < num; i++) {
			if ((bp = incore(vp, ia[i].in_lbn)) == NULL)
				continue;
			if ((bp->b_cflags & BC_BUSY) != 0 ||
			    (bp->b_oflags & BO_DELWRI) == 0)
				continue;
			bp->b_cflags |= BC_BUSY | BC_VFLUSH;
			mutex_exit(&bufcache_lock);
			bawrite(bp);
			mutex_enter(&bufcache_lock);
		}
		mutex_exit(&bufcache_lock);
	}

	if (ap->a_flags & FSYNC_WAIT) {
		mutex_enter(vp->v_interlock);
		while (vp->v_numoutput > 0)
			cv_wait(&vp->v_cv, vp->v_interlock);
		mutex_exit(vp->v_interlock);
	}

	error = ffs_update(vp, NULL, NULL, UPDATE_CLOSE |
	    (((ap->a_flags & (FSYNC_WAIT | FSYNC_DATAONLY)) == FSYNC_WAIT)
	    ? UPDATE_WAIT : 0));

	if (error == 0 && ap->a_flags & FSYNC_CACHE) {
		int l = 0;
		VOP_IOCTL(VTOI(vp)->i_devvp, DIOCCACHESYNC, &l, FWRITE,
			curlwp->l_cred);
	}

out:
	fstrans_done(mp);
	return error;
}

/*
 * Synch an open file.  Called for VOP_FSYNC().
 */
/* ARGSUSED */
int
ffs_full_fsync(struct vnode *vp, int flags)
{
	int error, i, uflags;
	struct mount *mp;

	KASSERT(vp->v_tag == VT_UFS);
	KASSERT(VTOI(vp) != NULL);
	KASSERT(vp->v_type != VCHR && vp->v_type != VBLK);

	error = 0;
	uflags = UPDATE_CLOSE | ((flags & FSYNC_WAIT) ? UPDATE_WAIT : 0);

	mp = vp->v_mount;

	/*
	 * Flush all dirty data associated with the vnode.
	 */
	if (vp->v_type == VREG) {
		int pflags = PGO_ALLPAGES | PGO_CLEANIT;

		if ((flags & FSYNC_WAIT))
			pflags |= PGO_SYNCIO;
		if (fstrans_getstate(mp) == FSTRANS_SUSPENDING)
			pflags |= PGO_FREE;
		mutex_enter(vp->v_interlock);
		error = VOP_PUTPAGES(vp, 0, 0, pflags);
		if (error)
			return error;
	}

#ifdef WAPBL
	if (mp && mp->mnt_wapbl) {
		/*
		 * Don't bother writing out metadata if the syncer is
		 * making the request.  We will let the sync vnode
		 * write it out in a single burst through a call to
		 * VFS_SYNC().
		 */
		if ((flags & (FSYNC_DATAONLY | FSYNC_LAZY)) != 0)
			return 0;

		if ((VTOI(vp)->i_flag & (IN_ACCESS | IN_CHANGE | IN_UPDATE
		    | IN_MODIFY | IN_MODIFIED | IN_ACCESSED)) != 0) {
			error = UFS_WAPBL_BEGIN(mp);
			if (error)
				return error;
			error = ffs_update(vp, NULL, NULL, uflags);
			UFS_WAPBL_END(mp);
		}
		if (error || (flags & FSYNC_NOLOG) != 0)
			return error;

		/*
		 * Don't flush the log if the vnode being flushed
		 * contains no dirty buffers that could be in the log.
		 */
		if (!LIST_EMPTY(&vp->v_dirtyblkhd)) {
			error = wapbl_flush(mp->mnt_wapbl, 0);
			if (error)
				return error;
		}

		if ((flags & FSYNC_WAIT) != 0) {
			mutex_enter(vp->v_interlock);
			while (vp->v_numoutput != 0)
				cv_wait(&vp->v_cv, vp->v_interlock);
			mutex_exit(vp->v_interlock);
		}

		return error;
	}
#endif /* WAPBL */

	error = vflushbuf(vp, (flags & FSYNC_WAIT) != 0);
	if (error == 0)
		error = ffs_update(vp, NULL, NULL, uflags);
	if (error == 0 && (flags & FSYNC_CACHE) != 0) {
		i = 1;
		(void)VOP_IOCTL(VTOI(vp)->i_devvp, DIOCCACHESYNC, &i, FWRITE,
		    kauth_cred_get());
	}

	return error;
}

/*
 * Reclaim an inode so that it can be used for other purposes.
 */
int
ffs_reclaim(void *v)
{
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
		struct lwp *a_l;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	struct mount *mp = vp->v_mount;
	struct ufsmount *ump = ip->i_ump;
	void *data;
	int error;

	fstrans_start(mp, FSTRANS_LAZY);
	/*
	 * The inode must be freed and updated before being removed
	 * from its hash chain.  Other threads trying to gain a hold
	 * on the inode will be stalled because it is locked (VI_XLOCK).
	 */
	error = UFS_WAPBL_BEGIN(mp);
	if (error) {
		fstrans_done(mp);
		return error;
	}
	if (ip->i_nlink <= 0 && ip->i_omode != 0 &&
	    (vp->v_mount->mnt_flag & MNT_RDONLY) == 0)
		ffs_vfree(vp, ip->i_number, ip->i_omode);
	UFS_WAPBL_END(mp);
	if ((error = ufs_reclaim(vp)) != 0) {
		fstrans_done(mp);
		return (error);
	}
	if (ip->i_din.ffs1_din != NULL) {
		if (ump->um_fstype == UFS1)
			pool_cache_put(ffs_dinode1_cache, ip->i_din.ffs1_din);
		else
			pool_cache_put(ffs_dinode2_cache, ip->i_din.ffs2_din);
	}
	/*
	 * To interlock with ffs_sync().
	 */
	genfs_node_destroy(vp);
	mutex_enter(vp->v_interlock);
	data = vp->v_data;
	vp->v_data = NULL;
	mutex_exit(vp->v_interlock);

	/*
	 * XXX MFS ends up here, too, to free an inode.  Should we create
	 * XXX a separate pool for MFS inodes?
	 */
	pool_cache_put(ffs_inode_cache, data);
	fstrans_done(mp);
	return (0);
}

/*
 * Return the last logical file offset that should be written for this file
 * if we're doing a write that ends at "size".
 */

void
ffs_gop_size(struct vnode *vp, off_t size, off_t *eobp, int flags)
{
	struct inode *ip = VTOI(vp);
	struct fs *fs = ip->i_fs;
	daddr_t olbn, nlbn;

	olbn = lblkno(fs, ip->i_size);
	nlbn = lblkno(fs, size);
	if (nlbn < NDADDR && olbn <= nlbn) {
		*eobp = fragroundup(fs, size);
	} else {
		*eobp = blkroundup(fs, size);
	}
}

int
ffs_openextattr(void *v)
{
	struct vop_openextattr_args /* {
		struct vnode *a_vp;
		kauth_cred_t a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct inode *ip = VTOI(ap->a_vp);
	struct fs *fs = ip->i_fs;

	/* Not supported for UFS1 file systems. */
	if (fs->fs_magic == FS_UFS1_MAGIC)
		return (EOPNOTSUPP);

	/* XXX Not implemented for UFS2 file systems. */
	return (EOPNOTSUPP);
}

int
ffs_closeextattr(void *v)
{
	struct vop_closeextattr_args /* {
		struct vnode *a_vp;
		int a_commit;
		kauth_cred_t a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct inode *ip = VTOI(ap->a_vp);
	struct fs *fs = ip->i_fs;

	/* Not supported for UFS1 file systems. */
	if (fs->fs_magic == FS_UFS1_MAGIC)
		return (EOPNOTSUPP);

	/* XXX Not implemented for UFS2 file systems. */
	return (EOPNOTSUPP);
}

int
ffs_getextattr(void *v)
{
	struct vop_getextattr_args /* {
		struct vnode *a_vp;
		int a_attrnamespace;
		const char *a_name;
		struct uio *a_uio;
		size_t *a_size;
		kauth_cred_t a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	struct fs *fs = ip->i_fs;

	if (fs->fs_magic == FS_UFS1_MAGIC) {
#ifdef UFS_EXTATTR
		int error;

		fstrans_start(vp->v_mount, FSTRANS_SHARED);
		error = ufs_getextattr(ap);
		fstrans_done(vp->v_mount);
		return error;
#else
		return (EOPNOTSUPP);
#endif
	}

	/* XXX Not implemented for UFS2 file systems. */
	return (EOPNOTSUPP);
}

int
ffs_setextattr(void *v)
{
	struct vop_setextattr_args /* {
		struct vnode *a_vp;
		int a_attrnamespace;
		const char *a_name;
		struct uio *a_uio;
		kauth_cred_t a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	struct fs *fs = ip->i_fs;

	if (fs->fs_magic == FS_UFS1_MAGIC) {
#ifdef UFS_EXTATTR
		int error;

		fstrans_start(vp->v_mount, FSTRANS_SHARED);
		error = ufs_setextattr(ap);
		fstrans_done(vp->v_mount);
		return error;
#else
		return (EOPNOTSUPP);
#endif
	}

	/* XXX Not implemented for UFS2 file systems. */
	return (EOPNOTSUPP);
}

int
ffs_listextattr(void *v)
{
	struct vop_listextattr_args /* {
		struct vnode *a_vp;
		int a_attrnamespace;
		struct uio *a_uio;
		size_t *a_size;
		kauth_cred_t a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct inode *ip = VTOI(ap->a_vp);
	struct fs *fs = ip->i_fs;

	if (fs->fs_magic == FS_UFS1_MAGIC) {
#ifdef UFS_EXTATTR
		struct vnode *vp = ap->a_vp;
		int error;

		fstrans_start(vp->v_mount, FSTRANS_SHARED);
		error = ufs_listextattr(ap);
		fstrans_done(vp->v_mount);
		return error;
#else
		return (EOPNOTSUPP);
#endif
	}

	/* XXX Not implemented for UFS2 file systems. */
	return (EOPNOTSUPP);
}

int
ffs_deleteextattr(void *v)
{
	struct vop_deleteextattr_args /* {
		struct vnode *a_vp;
		int a_attrnamespace;
		kauth_cred_t a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	struct fs *fs = ip->i_fs;

	if (fs->fs_magic == FS_UFS1_MAGIC) {
#ifdef UFS_EXTATTR
		int error;

		fstrans_start(vp->v_mount, FSTRANS_SHARED);
		error = ufs_deleteextattr(ap);
		fstrans_done(vp->v_mount);
		return error;
#else
		return (EOPNOTSUPP);
#endif
	}

	/* XXX Not implemented for UFS2 file systems. */
	return (EOPNOTSUPP);
}
