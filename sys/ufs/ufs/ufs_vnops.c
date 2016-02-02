/*	$NetBSD: ufs_vnops.c,v 1.231 2015/09/01 06:09:23 dholland Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
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

/*
 * Copyright (c) 1982, 1986, 1989, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)ufs_vnops.c	8.28 (Berkeley) 7/31/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ufs_vnops.c,v 1.231 2015/09/01 06:09:23 dholland Exp $");

#if defined(_KERNEL_OPT)
#include "opt_ffs.h"
#include "opt_quota.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/resourcevar.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/kmem.h>
#include <sys/malloc.h>
#include <sys/dirent.h>
#include <sys/lockf.h>
#include <sys/kauth.h>
#include <sys/wapbl.h>
#include <sys/fstrans.h>

#include <miscfs/specfs/specdev.h>
#include <miscfs/fifofs/fifo.h>
#include <miscfs/genfs/genfs.h>

#include <ufs/ufs/inode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_bswap.h>
#include <ufs/ufs/ufs_extern.h>
#include <ufs/ufs/ufs_wapbl.h>
#ifdef UFS_DIRHASH
#include <ufs/ufs/dirhash.h>
#endif
#include <ufs/ext2fs/ext2fs_extern.h>
#include <ufs/ext2fs/ext2fs_dir.h>
#include <ufs/ffs/ffs_extern.h>
#include <ufs/lfs/lfs_extern.h>
#include <ufs/lfs/lfs.h>

#include <uvm/uvm.h>

__CTASSERT(EXT2FS_MAXNAMLEN == FFS_MAXNAMLEN);
__CTASSERT(LFS_MAXNAMLEN == FFS_MAXNAMLEN);

static int ufs_chmod(struct vnode *, int, kauth_cred_t, struct lwp *);
static int ufs_chown(struct vnode *, uid_t, gid_t, kauth_cred_t,
    struct lwp *);
static int ufs_makeinode(struct vattr *, struct vnode *,
    const struct ufs_lookup_results *, struct vnode **, struct componentname *);

/*
 * A virgin directory (no blushing please).
 */
static const struct dirtemplate mastertemplate = {
	0,	12,			DT_DIR,	1,	".",
	0,	UFS_DIRBLKSIZ - 12,	DT_DIR,	2,	".."
};

/*
 * Create a regular file
 */
int
ufs_create(void *v)
{
	struct vop_create_v3_args /* {
		struct vnode		*a_dvp;
		struct vnode		**a_vpp;
		struct componentname	*a_cnp;
		struct vattr		*a_vap;
	} */ *ap = v;
	int	error;
	struct vnode *dvp = ap->a_dvp;
	struct ufs_lookup_results *ulr;

	/* XXX should handle this material another way */
	ulr = &VTOI(dvp)->i_crap;
	UFS_CHECK_CRAPCOUNTER(VTOI(dvp));

	/*
	 * UFS_WAPBL_BEGIN1(dvp->v_mount, dvp) performed by successful
	 * ufs_makeinode
	 */
	fstrans_start(dvp->v_mount, FSTRANS_SHARED);
	error = ufs_makeinode(ap->a_vap, dvp, ulr, ap->a_vpp, ap->a_cnp);
	if (error) {
		fstrans_done(dvp->v_mount);
		return (error);
	}
	UFS_WAPBL_END1(dvp->v_mount, dvp);
	fstrans_done(dvp->v_mount);
	VN_KNOTE(dvp, NOTE_WRITE);
	VOP_UNLOCK(*ap->a_vpp);
	return (0);
}

/*
 * Mknod vnode call
 */
/* ARGSUSED */
int
ufs_mknod(void *v)
{
	struct vop_mknod_v3_args /* {
		struct vnode		*a_dvp;
		struct vnode		**a_vpp;
		struct componentname	*a_cnp;
		struct vattr		*a_vap;
	} */ *ap = v;
	struct vattr	*vap;
	struct vnode	**vpp;
	struct inode	*ip;
	int		error;
	struct ufs_lookup_results *ulr;

	vap = ap->a_vap;
	vpp = ap->a_vpp;

	/* XXX should handle this material another way */
	ulr = &VTOI(ap->a_dvp)->i_crap;
	UFS_CHECK_CRAPCOUNTER(VTOI(ap->a_dvp));

	/*
	 * UFS_WAPBL_BEGIN1(dvp->v_mount, dvp) performed by successful
	 * ufs_makeinode
	 */
	fstrans_start(ap->a_dvp->v_mount, FSTRANS_SHARED);
	if ((error = ufs_makeinode(vap, ap->a_dvp, ulr, vpp, ap->a_cnp)) != 0)
		goto out;
	VN_KNOTE(ap->a_dvp, NOTE_WRITE);
	ip = VTOI(*vpp);
	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
	UFS_WAPBL_UPDATE(*vpp, NULL, NULL, 0);
	UFS_WAPBL_END1(ap->a_dvp->v_mount, ap->a_dvp);
	VOP_UNLOCK(*vpp);
out:
	fstrans_done(ap->a_dvp->v_mount);
	if (error != 0) {
		*vpp = NULL;
		return (error);
	}
	return (0);
}

/*
 * Open called.
 *
 * Nothing to do.
 */
/* ARGSUSED */
int
ufs_open(void *v)
{
	struct vop_open_args /* {
		struct vnode	*a_vp;
		int		a_mode;
		kauth_cred_t	a_cred;
	} */ *ap = v;

	/*
	 * Files marked append-only must be opened for appending.
	 */
	if ((VTOI(ap->a_vp)->i_flags & APPEND) &&
	    (ap->a_mode & (FWRITE | O_APPEND)) == FWRITE)
		return (EPERM);
	return (0);
}

/*
 * Close called.
 *
 * Update the times on the inode.
 */
/* ARGSUSED */
int
ufs_close(void *v)
{
	struct vop_close_args /* {
		struct vnode	*a_vp;
		int		a_fflag;
		kauth_cred_t	a_cred;
	} */ *ap = v;
	struct vnode	*vp;

	vp = ap->a_vp;
	fstrans_start(vp->v_mount, FSTRANS_SHARED);
	if (vp->v_usecount > 1)
		UFS_ITIMES(vp, NULL, NULL, NULL);
	fstrans_done(vp->v_mount);
	return (0);
}

static int
ufs_check_possible(struct vnode *vp, struct inode *ip, mode_t mode,
    kauth_cred_t cred)
{
#if defined(QUOTA) || defined(QUOTA2)
	int error;
#endif

	/*
	 * Disallow write attempts on read-only file systems;
	 * unless the file is a socket, fifo, or a block or
	 * character device resident on the file system.
	 */
	if (mode & VWRITE) {
		switch (vp->v_type) {
		case VDIR:
		case VLNK:
		case VREG:
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);
#if defined(QUOTA) || defined(QUOTA2)
			fstrans_start(vp->v_mount, FSTRANS_SHARED);
			error = chkdq(ip, 0, cred, 0);
			fstrans_done(vp->v_mount);
			if (error != 0)
				return error;
#endif
			break;
		case VBAD:
		case VBLK:
		case VCHR:
		case VSOCK:
		case VFIFO:
		case VNON:
		default:
			break;
		}
	}

	/* If it is a snapshot, nobody gets access to it. */
	if ((ip->i_flags & SF_SNAPSHOT))
		return (EPERM);
	/* If immutable bit set, nobody gets to write it. */
	if ((mode & VWRITE) && (ip->i_flags & IMMUTABLE))
		return (EPERM);

	return 0;
}

static int
ufs_check_permitted(struct vnode *vp, struct inode *ip, mode_t mode,
    kauth_cred_t cred)
{

	return kauth_authorize_vnode(cred, KAUTH_ACCESS_ACTION(mode, vp->v_type,
	    ip->i_mode & ALLPERMS), vp, NULL, genfs_can_access(vp->v_type,
	    ip->i_mode & ALLPERMS, ip->i_uid, ip->i_gid, mode, cred));
}

int
ufs_access(void *v)
{
	struct vop_access_args /* {
		struct vnode	*a_vp;
		int		a_mode;
		kauth_cred_t	a_cred;
	} */ *ap = v;
	struct vnode	*vp;
	struct inode	*ip;
	mode_t		mode;
	int		error;

	vp = ap->a_vp;
	ip = VTOI(vp);
	mode = ap->a_mode;

	error = ufs_check_possible(vp, ip, mode, ap->a_cred);
	if (error)
		return error;

	error = ufs_check_permitted(vp, ip, mode, ap->a_cred);

	return error;
}

/* ARGSUSED */
int
ufs_getattr(void *v)
{
	struct vop_getattr_args /* {
		struct vnode	*a_vp;
		struct vattr	*a_vap;
		kauth_cred_t	a_cred;
	} */ *ap = v;
	struct vnode	*vp;
	struct inode	*ip;
	struct vattr	*vap;

	vp = ap->a_vp;
	ip = VTOI(vp);
	vap = ap->a_vap;
	fstrans_start(vp->v_mount, FSTRANS_SHARED);
	UFS_ITIMES(vp, NULL, NULL, NULL);

	/*
	 * Copy from inode table
	 */
	vap->va_fsid = ip->i_dev;
	vap->va_fileid = ip->i_number;
	vap->va_mode = ip->i_mode & ALLPERMS;
	vap->va_nlink = ip->i_nlink;
	vap->va_uid = ip->i_uid;
	vap->va_gid = ip->i_gid;
	vap->va_size = vp->v_size;
	if (ip->i_ump->um_fstype == UFS1) {
		switch (vp->v_type) {
		    case VBLK:
		    case VCHR:
			vap->va_rdev = (dev_t)ufs_rw32(ip->i_ffs1_rdev,
			    UFS_MPNEEDSWAP(ip->i_ump));
			break;
		    default:
			vap->va_rdev = NODEV;
			break;
		}
		vap->va_atime.tv_sec = ip->i_ffs1_atime;
		vap->va_atime.tv_nsec = ip->i_ffs1_atimensec;
		vap->va_mtime.tv_sec = ip->i_ffs1_mtime;
		vap->va_mtime.tv_nsec = ip->i_ffs1_mtimensec;
		vap->va_ctime.tv_sec = ip->i_ffs1_ctime;
		vap->va_ctime.tv_nsec = ip->i_ffs1_ctimensec;
		vap->va_birthtime.tv_sec = 0;
		vap->va_birthtime.tv_nsec = 0;
		vap->va_bytes = dbtob((u_quad_t)ip->i_ffs1_blocks);
	} else {
		switch (vp->v_type) {
		    case VBLK:
		    case VCHR:
			vap->va_rdev = (dev_t)ufs_rw64(ip->i_ffs2_rdev,
			    UFS_MPNEEDSWAP(ip->i_ump));
			break;
		    default:
			vap->va_rdev = NODEV;
			break;
		}
		vap->va_atime.tv_sec = ip->i_ffs2_atime;
		vap->va_atime.tv_nsec = ip->i_ffs2_atimensec;
		vap->va_mtime.tv_sec = ip->i_ffs2_mtime;
		vap->va_mtime.tv_nsec = ip->i_ffs2_mtimensec;
		vap->va_ctime.tv_sec = ip->i_ffs2_ctime;
		vap->va_ctime.tv_nsec = ip->i_ffs2_ctimensec;
		vap->va_birthtime.tv_sec = ip->i_ffs2_birthtime;
		vap->va_birthtime.tv_nsec = ip->i_ffs2_birthnsec;
		vap->va_bytes = dbtob(ip->i_ffs2_blocks);
	}
	vap->va_gen = ip->i_gen;
	vap->va_flags = ip->i_flags;

	/* this doesn't belong here */
	if (vp->v_type == VBLK)
		vap->va_blocksize = BLKDEV_IOSIZE;
	else if (vp->v_type == VCHR)
		vap->va_blocksize = MAXBSIZE;
	else
		vap->va_blocksize = vp->v_mount->mnt_stat.f_iosize;
	vap->va_type = vp->v_type;
	vap->va_filerev = ip->i_modrev;
	fstrans_done(vp->v_mount);
	return (0);
}

/*
 * Set attribute vnode op. called from several syscalls
 */
int
ufs_setattr(void *v)
{
	struct vop_setattr_args /* {
		struct vnode	*a_vp;
		struct vattr	*a_vap;
		kauth_cred_t	a_cred;
	} */ *ap = v;
	struct vattr	*vap;
	struct vnode	*vp;
	struct inode	*ip;
	kauth_cred_t	cred;
	struct lwp	*l;
	int		error;
	kauth_action_t	action;
	bool		changing_sysflags;

	vap = ap->a_vap;
	vp = ap->a_vp;
	ip = VTOI(vp);
	cred = ap->a_cred;
	l = curlwp;
	action = KAUTH_VNODE_WRITE_FLAGS;
	changing_sysflags = false;

	/*
	 * Check for unsettable attributes.
	 */
	if ((vap->va_type != VNON) || (vap->va_nlink != VNOVAL) ||
	    (vap->va_fsid != VNOVAL) || (vap->va_fileid != VNOVAL) ||
	    (vap->va_blocksize != VNOVAL) || (vap->va_rdev != VNOVAL) ||
	    ((int)vap->va_bytes != VNOVAL) || (vap->va_gen != VNOVAL)) {
		return (EINVAL);
	}

	fstrans_start(vp->v_mount, FSTRANS_SHARED);

	if (vap->va_flags != VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY) {
			error = EROFS;
			goto out;
		}

		/* Snapshot flag cannot be set or cleared */
		if ((vap->va_flags & (SF_SNAPSHOT | SF_SNAPINVAL)) !=
		    (ip->i_flags & (SF_SNAPSHOT | SF_SNAPINVAL))) {
			error = EPERM;
			goto out;
		}

		if (ip->i_flags & (SF_IMMUTABLE | SF_APPEND)) {
			action |= KAUTH_VNODE_HAS_SYSFLAGS;
		}

		if ((vap->va_flags & SF_SETTABLE) !=
		    (ip->i_flags & SF_SETTABLE)) {
			action |= KAUTH_VNODE_WRITE_SYSFLAGS;
			changing_sysflags = true;
		}

		error = kauth_authorize_vnode(cred, action, vp, NULL,
		    genfs_can_chflags(cred, vp->v_type, ip->i_uid,
		    changing_sysflags));
		if (error)
			goto out;

		if (changing_sysflags) {
			error = UFS_WAPBL_BEGIN(vp->v_mount);
			if (error)
				goto out;
			ip->i_flags = vap->va_flags;
			DIP_ASSIGN(ip, flags, ip->i_flags);
		} else {
			error = UFS_WAPBL_BEGIN(vp->v_mount);
			if (error)
				goto out;
			ip->i_flags &= SF_SETTABLE;
			ip->i_flags |= (vap->va_flags & UF_SETTABLE);
			DIP_ASSIGN(ip, flags, ip->i_flags);
		}
		ip->i_flag |= IN_CHANGE;
		UFS_WAPBL_UPDATE(vp, NULL, NULL, 0);
		UFS_WAPBL_END(vp->v_mount);
		if (vap->va_flags & (IMMUTABLE | APPEND)) {
			error = 0;
			goto out;
		}
	}
	if (ip->i_flags & (IMMUTABLE | APPEND)) {
		error = EPERM;
		goto out;
	}
	/*
	 * Go through the fields and update iff not VNOVAL.
	 */
	if (vap->va_uid != (uid_t)VNOVAL || vap->va_gid != (gid_t)VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY) {
			error = EROFS;
			goto out;
		}
		error = UFS_WAPBL_BEGIN(vp->v_mount);
		if (error)
			goto out;
		error = ufs_chown(vp, vap->va_uid, vap->va_gid, cred, l);
		UFS_WAPBL_END(vp->v_mount);
		if (error)
			goto out;
	}
	if (vap->va_size != VNOVAL) {
		/*
		 * Disallow write attempts on read-only file systems;
		 * unless the file is a socket, fifo, or a block or
		 * character device resident on the file system.
		 */
		switch (vp->v_type) {
		case VDIR:
			error = EISDIR;
			goto out;
		case VCHR:
		case VBLK:
		case VFIFO:
			break;
		case VREG:
			if (vp->v_mount->mnt_flag & MNT_RDONLY) {
				error = EROFS;
				goto out;
			}
			if ((ip->i_flags & SF_SNAPSHOT) != 0) {
				error = EPERM;
				goto out;
			}
			error = ufs_truncate(vp, vap->va_size, cred);
			if (error)
				goto out;
			break;
		default:
			error = EOPNOTSUPP;
			goto out;
		}
	}
	ip = VTOI(vp);
	if (vap->va_atime.tv_sec != VNOVAL || vap->va_mtime.tv_sec != VNOVAL ||
	    vap->va_birthtime.tv_sec != VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY) {
			error = EROFS;
			goto out;
		}
		if ((ip->i_flags & SF_SNAPSHOT) != 0) {
			error = EPERM;
			goto out;
		}
		error = kauth_authorize_vnode(cred, KAUTH_VNODE_WRITE_TIMES, vp,
		    NULL, genfs_can_chtimes(vp, vap->va_vaflags, ip->i_uid, cred));
		if (error)
			goto out;
		error = UFS_WAPBL_BEGIN(vp->v_mount);
		if (error)
			goto out;
		if (vap->va_atime.tv_sec != VNOVAL)
			if (!(vp->v_mount->mnt_flag & MNT_NOATIME))
				ip->i_flag |= IN_ACCESS;
		if (vap->va_mtime.tv_sec != VNOVAL) {
			ip->i_flag |= IN_CHANGE | IN_UPDATE;
			if (vp->v_mount->mnt_flag & MNT_RELATIME)
				ip->i_flag |= IN_ACCESS;
		}
		if (vap->va_birthtime.tv_sec != VNOVAL &&
		    ip->i_ump->um_fstype == UFS2) {
			ip->i_ffs2_birthtime = vap->va_birthtime.tv_sec;
			ip->i_ffs2_birthnsec = vap->va_birthtime.tv_nsec;
		}
		error = UFS_UPDATE(vp, &vap->va_atime, &vap->va_mtime, 0);
		UFS_WAPBL_END(vp->v_mount);
		if (error)
			goto out;
	}
	error = 0;
	if (vap->va_mode != (mode_t)VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY) {
			error = EROFS;
			goto out;
		}
		if ((ip->i_flags & SF_SNAPSHOT) != 0 &&
		    (vap->va_mode & (S_IXUSR | S_IWUSR | S_IXGRP | S_IWGRP |
		     S_IXOTH | S_IWOTH))) {
			error = EPERM;
			goto out;
		}
		error = UFS_WAPBL_BEGIN(vp->v_mount);
		if (error)
			goto out;
		error = ufs_chmod(vp, (int)vap->va_mode, cred, l);
		UFS_WAPBL_END(vp->v_mount);
	}
	VN_KNOTE(vp, NOTE_ATTRIB);
out:
	fstrans_done(vp->v_mount);
	return (error);
}

/*
 * Change the mode on a file.
 * Inode must be locked before calling.
 */
static int
ufs_chmod(struct vnode *vp, int mode, kauth_cred_t cred, struct lwp *l)
{
	struct inode	*ip;
	int		error;

	UFS_WAPBL_JLOCK_ASSERT(vp->v_mount);

	ip = VTOI(vp);

	error = kauth_authorize_vnode(cred, KAUTH_VNODE_WRITE_SECURITY, vp,
	    NULL, genfs_can_chmod(vp->v_type, cred, ip->i_uid, ip->i_gid, mode));
	if (error)
		return (error);

	fstrans_start(vp->v_mount, FSTRANS_SHARED);
	ip->i_mode &= ~ALLPERMS;
	ip->i_mode |= (mode & ALLPERMS);
	ip->i_flag |= IN_CHANGE;
	DIP_ASSIGN(ip, mode, ip->i_mode);
	UFS_WAPBL_UPDATE(vp, NULL, NULL, 0);
	fstrans_done(vp->v_mount);
	return (0);
}

/*
 * Perform chown operation on inode ip;
 * inode must be locked prior to call.
 */
static int
ufs_chown(struct vnode *vp, uid_t uid, gid_t gid, kauth_cred_t cred,
    	struct lwp *l)
{
	struct inode	*ip;
	int		error = 0;
#if defined(QUOTA) || defined(QUOTA2)
	uid_t		ouid;
	gid_t		ogid;
	int64_t		change;
#endif
	ip = VTOI(vp);
	error = 0;

	if (uid == (uid_t)VNOVAL)
		uid = ip->i_uid;
	if (gid == (gid_t)VNOVAL)
		gid = ip->i_gid;

	error = kauth_authorize_vnode(cred, KAUTH_VNODE_CHANGE_OWNERSHIP, vp,
	    NULL, genfs_can_chown(cred, ip->i_uid, ip->i_gid, uid, gid));
	if (error)
		return (error);

	fstrans_start(vp->v_mount, FSTRANS_SHARED);
#if defined(QUOTA) || defined(QUOTA2)
	ogid = ip->i_gid;
	ouid = ip->i_uid;
	change = DIP(ip, blocks);
	(void) chkdq(ip, -change, cred, 0);
	(void) chkiq(ip, -1, cred, 0);
#endif
	ip->i_gid = gid;
	DIP_ASSIGN(ip, gid, gid);
	ip->i_uid = uid;
	DIP_ASSIGN(ip, uid, uid);
#if defined(QUOTA) || defined(QUOTA2)
	if ((error = chkdq(ip, change, cred, 0)) == 0) {
		if ((error = chkiq(ip, 1, cred, 0)) == 0)
			goto good;
		else
			(void) chkdq(ip, -change, cred, FORCE);
	}
	ip->i_gid = ogid;
	DIP_ASSIGN(ip, gid, ogid);
	ip->i_uid = ouid;
	DIP_ASSIGN(ip, uid, ouid);
	(void) chkdq(ip, change, cred, FORCE);
	(void) chkiq(ip, 1, cred, FORCE);
	fstrans_done(vp->v_mount);
	return (error);
 good:
#endif /* QUOTA || QUOTA2 */
	ip->i_flag |= IN_CHANGE;
	UFS_WAPBL_UPDATE(vp, NULL, NULL, 0);
	fstrans_done(vp->v_mount);
	return (0);
}

int
ufs_remove(void *v)
{
	struct vop_remove_args /* {
		struct vnode		*a_dvp;
		struct vnode		*a_vp;
		struct componentname	*a_cnp;
	} */ *ap = v;
	struct vnode	*vp, *dvp;
	struct inode	*ip;
	struct mount	*mp;
	int		error;
	struct ufs_lookup_results *ulr;

	vp = ap->a_vp;
	dvp = ap->a_dvp;
	ip = VTOI(vp);
	mp = dvp->v_mount;
	KASSERT(mp == vp->v_mount); /* XXX Not stable without lock.  */

	/* XXX should handle this material another way */
	ulr = &VTOI(dvp)->i_crap;
	UFS_CHECK_CRAPCOUNTER(VTOI(dvp));

	fstrans_start(mp, FSTRANS_SHARED);
	if (vp->v_type == VDIR || (ip->i_flags & (IMMUTABLE | APPEND)) ||
	    (VTOI(dvp)->i_flags & APPEND))
		error = EPERM;
	else {
		error = UFS_WAPBL_BEGIN(mp);
		if (error == 0) {
			error = ufs_dirremove(dvp, ulr,
					      ip, ap->a_cnp->cn_flags, 0);
			UFS_WAPBL_END(mp);
		}
	}
	VN_KNOTE(vp, NOTE_DELETE);
	VN_KNOTE(dvp, NOTE_WRITE);
	if (dvp == vp)
		vrele(vp);
	else
		vput(vp);
	vput(dvp);
	fstrans_done(mp);
	return (error);
}

/*
 * ufs_link: create hard link.
 */
int
ufs_link(void *v)
{
	struct vop_link_v2_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	struct vnode *dvp = ap->a_dvp;
	struct vnode *vp = ap->a_vp;
	struct componentname *cnp = ap->a_cnp;
	struct mount *mp = dvp->v_mount;
	struct inode *ip;
	struct direct *newdir;
	int error;
	struct ufs_lookup_results *ulr;

	KASSERT(dvp != vp);
	KASSERT(vp->v_type != VDIR);
	KASSERT(mp == vp->v_mount); /* XXX Not stable without lock.  */

	/* XXX should handle this material another way */
	ulr = &VTOI(dvp)->i_crap;
	UFS_CHECK_CRAPCOUNTER(VTOI(dvp));

	fstrans_start(mp, FSTRANS_SHARED);
	error = vn_lock(vp, LK_EXCLUSIVE);
	if (error) {
		VOP_ABORTOP(dvp, cnp);
		goto out2;
	}
	ip = VTOI(vp);
	if ((nlink_t)ip->i_nlink >= LINK_MAX) {
		VOP_ABORTOP(dvp, cnp);
		error = EMLINK;
		goto out1;
	}
	if (ip->i_flags & (IMMUTABLE | APPEND)) {
		VOP_ABORTOP(dvp, cnp);
		error = EPERM;
		goto out1;
	}
	error = UFS_WAPBL_BEGIN(mp);
	if (error) {
		VOP_ABORTOP(dvp, cnp);
		goto out1;
	}
	ip->i_nlink++;
	DIP_ASSIGN(ip, nlink, ip->i_nlink);
	ip->i_flag |= IN_CHANGE;
	error = UFS_UPDATE(vp, NULL, NULL, UPDATE_DIROP);
	if (!error) {
		newdir = pool_cache_get(ufs_direct_cache, PR_WAITOK);
		ufs_makedirentry(ip, cnp, newdir);
		error = ufs_direnter(dvp, ulr, vp, newdir, cnp, NULL);
		pool_cache_put(ufs_direct_cache, newdir);
	}
	if (error) {
		ip->i_nlink--;
		DIP_ASSIGN(ip, nlink, ip->i_nlink);
		ip->i_flag |= IN_CHANGE;
		UFS_WAPBL_UPDATE(vp, NULL, NULL, UPDATE_DIROP);
	}
	UFS_WAPBL_END(mp);
 out1:
	VOP_UNLOCK(vp);
 out2:
	VN_KNOTE(vp, NOTE_LINK);
	VN_KNOTE(dvp, NOTE_WRITE);
	fstrans_done(mp);
	return (error);
}

/*
 * whiteout vnode call
 */
int
ufs_whiteout(void *v)
{
	struct vop_whiteout_args /* {
		struct vnode		*a_dvp;
		struct componentname	*a_cnp;
		int			a_flags;
	} */ *ap = v;
	struct vnode		*dvp = ap->a_dvp;
	struct componentname	*cnp = ap->a_cnp;
	struct direct		*newdir;
	int			error;
	struct ufsmount		*ump = VFSTOUFS(dvp->v_mount);
	struct ufs_lookup_results *ulr;

	/* XXX should handle this material another way */
	ulr = &VTOI(dvp)->i_crap;
	UFS_CHECK_CRAPCOUNTER(VTOI(dvp));

	error = 0;
	switch (ap->a_flags) {
	case LOOKUP:
		/* 4.4 format directories support whiteout operations */
		if (ump->um_maxsymlinklen > 0)
			return (0);
		return (EOPNOTSUPP);

	case CREATE:
		/* create a new directory whiteout */
		fstrans_start(dvp->v_mount, FSTRANS_SHARED);
		error = UFS_WAPBL_BEGIN(dvp->v_mount);
		if (error)
			break;
#ifdef DIAGNOSTIC
		if (ump->um_maxsymlinklen <= 0)
			panic("ufs_whiteout: old format filesystem");
#endif

		newdir = pool_cache_get(ufs_direct_cache, PR_WAITOK);
		newdir->d_ino = UFS_WINO;
		newdir->d_namlen = cnp->cn_namelen;
		memcpy(newdir->d_name, cnp->cn_nameptr,
		    (size_t)cnp->cn_namelen);
		newdir->d_name[cnp->cn_namelen] = '\0';
		newdir->d_type = DT_WHT;
		error = ufs_direnter(dvp, ulr, NULL, newdir, cnp, NULL);
		pool_cache_put(ufs_direct_cache, newdir);
		break;

	case DELETE:
		/* remove an existing directory whiteout */
		fstrans_start(dvp->v_mount, FSTRANS_SHARED);
		error = UFS_WAPBL_BEGIN(dvp->v_mount);
		if (error)
			break;
#ifdef DIAGNOSTIC
		if (ump->um_maxsymlinklen <= 0)
			panic("ufs_whiteout: old format filesystem");
#endif

		cnp->cn_flags &= ~DOWHITEOUT;
		error = ufs_dirremove(dvp, ulr, NULL, cnp->cn_flags, 0);
		break;
	default:
		panic("ufs_whiteout: unknown op");
		/* NOTREACHED */
	}
	UFS_WAPBL_END(dvp->v_mount);
	fstrans_done(dvp->v_mount);
	return (error);
}

int
ufs_mkdir(void *v)
{
	struct vop_mkdir_v3_args /* {
		struct vnode		*a_dvp;
		struct vnode		**a_vpp;
		struct componentname	*a_cnp;
		struct vattr		*a_vap;
	} */ *ap = v;
	struct vnode		*dvp = ap->a_dvp, *tvp;
	struct vattr		*vap = ap->a_vap;
	struct componentname	*cnp = ap->a_cnp;
	struct inode		*ip, *dp = VTOI(dvp);
	struct buf		*bp;
	struct dirtemplate	dirtemplate;
	struct direct		*newdir;
	int			error;
	struct ufsmount		*ump = dp->i_ump;
	int			dirblksiz = ump->um_dirblksiz;
	struct ufs_lookup_results *ulr;

	fstrans_start(dvp->v_mount, FSTRANS_SHARED);

	/* XXX should handle this material another way */
	ulr = &dp->i_crap;
	UFS_CHECK_CRAPCOUNTER(dp);

	KASSERT(vap->va_type == VDIR);

	if ((nlink_t)dp->i_nlink >= LINK_MAX) {
		error = EMLINK;
		goto out;
	}
	/*
	 * Must simulate part of ufs_makeinode here to acquire the inode,
	 * but not have it entered in the parent directory. The entry is
	 * made later after writing "." and ".." entries.
	 */
	error = vcache_new(dvp->v_mount, dvp, vap, cnp->cn_cred, ap->a_vpp);
	if (error)
		goto out;
	error = vn_lock(*ap->a_vpp, LK_EXCLUSIVE);
	if (error) {
		vrele(*ap->a_vpp);
		*ap->a_vpp = NULL;
		goto out;
	}
	error = UFS_WAPBL_BEGIN(ap->a_dvp->v_mount);
	if (error) {
		vput(*ap->a_vpp);
		goto out;
	}

	tvp = *ap->a_vpp;
	ip = VTOI(tvp);
	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
	ip->i_nlink = 2;
	DIP_ASSIGN(ip, nlink, 2);
	if (cnp->cn_flags & ISWHITEOUT) {
		ip->i_flags |= UF_OPAQUE;
		DIP_ASSIGN(ip, flags, ip->i_flags);
	}

	/*
	 * Bump link count in parent directory to reflect work done below.
	 * Should be done before reference is created so cleanup is
	 * possible if we crash.
	 */
	dp->i_nlink++;
	DIP_ASSIGN(dp, nlink, dp->i_nlink);
	dp->i_flag |= IN_CHANGE;
	if ((error = UFS_UPDATE(dvp, NULL, NULL, UPDATE_DIROP)) != 0)
		goto bad;

	/*
	 * Initialize directory with "." and ".." from static template.
	 */
	dirtemplate = mastertemplate;
	dirtemplate.dotdot_reclen = dirblksiz - dirtemplate.dot_reclen;
	dirtemplate.dot_ino = ufs_rw32(ip->i_number, UFS_MPNEEDSWAP(ump));
	dirtemplate.dotdot_ino = ufs_rw32(dp->i_number, UFS_MPNEEDSWAP(ump));
	dirtemplate.dot_reclen = ufs_rw16(dirtemplate.dot_reclen,
	    UFS_MPNEEDSWAP(ump));
	dirtemplate.dotdot_reclen = ufs_rw16(dirtemplate.dotdot_reclen,
	    UFS_MPNEEDSWAP(ump));
	if (ump->um_maxsymlinklen <= 0) {
#if BYTE_ORDER == LITTLE_ENDIAN
		if (UFS_MPNEEDSWAP(ump) == 0)
#else
		if (UFS_MPNEEDSWAP(ump) != 0)
#endif
		{
			dirtemplate.dot_type = dirtemplate.dot_namlen;
			dirtemplate.dotdot_type = dirtemplate.dotdot_namlen;
			dirtemplate.dot_namlen = dirtemplate.dotdot_namlen = 0;
		} else
			dirtemplate.dot_type = dirtemplate.dotdot_type = 0;
	}
	if ((error = UFS_BALLOC(tvp, (off_t)0, dirblksiz, cnp->cn_cred,
	    B_CLRBUF, &bp)) != 0)
		goto bad;
	ip->i_size = dirblksiz;
	DIP_ASSIGN(ip, size, dirblksiz);
	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
	uvm_vnp_setsize(tvp, ip->i_size);
	memcpy((void *)bp->b_data, (void *)&dirtemplate, sizeof dirtemplate);

	/*
	 * Directory set up, now install its entry in the parent directory.
	 * We must write out the buffer containing the new directory body
	 * before entering the new name in the parent.
	 */
	if ((error = VOP_BWRITE(bp->b_vp, bp)) != 0)
		goto bad;
	if ((error = UFS_UPDATE(tvp, NULL, NULL, UPDATE_DIROP)) != 0) {
		goto bad;
	}
	newdir = pool_cache_get(ufs_direct_cache, PR_WAITOK);
	ufs_makedirentry(ip, cnp, newdir);
	error = ufs_direnter(dvp, ulr, tvp, newdir, cnp, bp);
	pool_cache_put(ufs_direct_cache, newdir);
 bad:
	if (error == 0) {
		VN_KNOTE(dvp, NOTE_WRITE | NOTE_LINK);
		VOP_UNLOCK(tvp);
		UFS_WAPBL_END(dvp->v_mount);
	} else {
		dp->i_nlink--;
		DIP_ASSIGN(dp, nlink, dp->i_nlink);
		dp->i_flag |= IN_CHANGE;
		UFS_WAPBL_UPDATE(dvp, NULL, NULL, UPDATE_DIROP);
		/*
		 * No need to do an explicit UFS_TRUNCATE here, vrele will
		 * do this for us because we set the link count to 0.
		 */
		ip->i_nlink = 0;
		DIP_ASSIGN(ip, nlink, 0);
		ip->i_flag |= IN_CHANGE;
		UFS_WAPBL_UPDATE(tvp, NULL, NULL, UPDATE_DIROP);
		UFS_WAPBL_END(dvp->v_mount);
		vput(tvp);
	}
 out:
	fstrans_done(dvp->v_mount);
	return (error);
}

int
ufs_rmdir(void *v)
{
	struct vop_rmdir_args /* {
		struct vnode		*a_dvp;
		struct vnode		*a_vp;
		struct componentname	*a_cnp;
	} */ *ap = v;
	struct vnode		*vp, *dvp;
	struct componentname	*cnp;
	struct inode		*ip, *dp;
	int			error;
	struct ufs_lookup_results *ulr;

	vp = ap->a_vp;
	dvp = ap->a_dvp;
	cnp = ap->a_cnp;
	ip = VTOI(vp);
	dp = VTOI(dvp);

	/* XXX should handle this material another way */
	ulr = &dp->i_crap;
	UFS_CHECK_CRAPCOUNTER(dp);

	/*
	 * No rmdir "." or of mounted directories please.
	 */
	if (dp == ip || vp->v_mountedhere != NULL) {
		if (dp == ip)
			vrele(dvp);
		else
			vput(dvp);
		vput(vp);
		return (EINVAL);
	}

	fstrans_start(dvp->v_mount, FSTRANS_SHARED);

	/*
	 * Do not remove a directory that is in the process of being renamed.
	 * Verify that the directory is empty (and valid). (Rmdir ".." won't
	 * be valid since ".." will contain a reference to the current
	 * directory and thus be non-empty.)
	 */
	error = 0;
	if (ip->i_nlink != 2 ||
	    !ufs_dirempty(ip, dp->i_number, cnp->cn_cred)) {
		error = ENOTEMPTY;
		goto out;
	}
	if ((dp->i_flags & APPEND) ||
		(ip->i_flags & (IMMUTABLE | APPEND))) {
		error = EPERM;
		goto out;
	}
	error = UFS_WAPBL_BEGIN(dvp->v_mount);
	if (error)
		goto out;
	/*
	 * Delete reference to directory before purging
	 * inode.  If we crash in between, the directory
	 * will be reattached to lost+found,
	 */
	error = ufs_dirremove(dvp, ulr, ip, cnp->cn_flags, 1);
	if (error) {
		UFS_WAPBL_END(dvp->v_mount);
		goto out;
	}
	VN_KNOTE(dvp, NOTE_WRITE | NOTE_LINK);
	cache_purge(dvp);
	/*
	 * Truncate inode.  The only stuff left in the directory is "." and
	 * "..".  The "." reference is inconsequential since we're quashing
	 * it.
	 */
	dp->i_nlink--;
	DIP_ASSIGN(dp, nlink, dp->i_nlink);
	dp->i_flag |= IN_CHANGE;
	UFS_WAPBL_UPDATE(dvp, NULL, NULL, UPDATE_DIROP);
	ip->i_nlink--;
	DIP_ASSIGN(ip, nlink, ip->i_nlink);
	ip->i_flag |= IN_CHANGE;
	error = UFS_TRUNCATE(vp, (off_t)0, IO_SYNC, cnp->cn_cred);
	cache_purge(vp);
	/*
	 * Unlock the log while we still have reference to unlinked
	 * directory vp so that it will not get locked for recycling
	 */
	UFS_WAPBL_END(dvp->v_mount);
#ifdef UFS_DIRHASH
	if (ip->i_dirhash != NULL)
		ufsdirhash_free(ip);
#endif
 out:
	VN_KNOTE(vp, NOTE_DELETE);
	vput(vp);
	fstrans_done(dvp->v_mount);
	vput(dvp);
	return (error);
}

/*
 * symlink -- make a symbolic link
 */
int
ufs_symlink(void *v)
{
	struct vop_symlink_v3_args /* {
		struct vnode		*a_dvp;
		struct vnode		**a_vpp;
		struct componentname	*a_cnp;
		struct vattr		*a_vap;
		char			*a_target;
	} */ *ap = v;
	struct vnode	*vp, **vpp;
	struct inode	*ip;
	int		len, error;
	struct ufs_lookup_results *ulr;

	vpp = ap->a_vpp;

	/* XXX should handle this material another way */
	ulr = &VTOI(ap->a_dvp)->i_crap;
	UFS_CHECK_CRAPCOUNTER(VTOI(ap->a_dvp));

	/*
	 * UFS_WAPBL_BEGIN1(dvp->v_mount, dvp) performed by successful
	 * ufs_makeinode
	 */
	fstrans_start(ap->a_dvp->v_mount, FSTRANS_SHARED);
	KASSERT(ap->a_vap->va_type == VLNK);
	error = ufs_makeinode(ap->a_vap, ap->a_dvp, ulr, vpp, ap->a_cnp);
	if (error)
		goto out;
	VN_KNOTE(ap->a_dvp, NOTE_WRITE);
	vp = *vpp;
	len = strlen(ap->a_target);
	ip = VTOI(vp);
	/*
	 * This test is off by one. um_maxsymlinklen contains the
	 * number of bytes available, and we aren't storing a \0, so
	 * the test should properly be <=. However, it cannot be
	 * changed as this would break compatibility with existing fs
	 * images -- see the way ufs_readlink() works.
	 */
	if (len < ip->i_ump->um_maxsymlinklen) {
		memcpy((char *)SHORTLINK(ip), ap->a_target, len);
		ip->i_size = len;
		DIP_ASSIGN(ip, size, len);
		uvm_vnp_setsize(vp, ip->i_size);
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		if (vp->v_mount->mnt_flag & MNT_RELATIME)
			ip->i_flag |= IN_ACCESS;
		UFS_WAPBL_UPDATE(vp, NULL, NULL, 0);
	} else
		error = ufs_bufio(UIO_WRITE, vp, ap->a_target, len, (off_t)0,
		    IO_NODELOCKED | IO_JOURNALLOCKED, ap->a_cnp->cn_cred, NULL,
		    NULL);
	UFS_WAPBL_END1(ap->a_dvp->v_mount, ap->a_dvp);
	VOP_UNLOCK(vp);
	if (error)
		vrele(vp);
out:
	fstrans_done(ap->a_dvp->v_mount);
	return (error);
}

/*
 * Vnode op for reading directories.
 *
 * This routine handles converting from the on-disk directory format
 * "struct direct" to the in-memory format "struct dirent" as well as
 * byte swapping the entries if necessary.
 */
int
ufs_readdir(void *v)
{
	struct vop_readdir_args /* {
		struct vnode	*a_vp;
		struct uio	*a_uio;
		kauth_cred_t	a_cred;
		int		*a_eofflag;
		off_t		**a_cookies;
		int		*ncookies;
	} */ *ap = v;
	struct vnode	*vp = ap->a_vp;
	struct direct	*cdp, *ecdp;
	struct dirent	*ndp;
	char		*cdbuf, *ndbuf, *endp;
	struct uio	auio, *uio;
	struct iovec	aiov;
	int		error;
	size_t		count, ccount, rcount, cdbufsz, ndbufsz;
	off_t		off, *ccp;
	off_t		startoff;
	size_t		skipbytes;
	struct ufsmount	*ump = VFSTOUFS(vp->v_mount);
	int nswap = UFS_MPNEEDSWAP(ump);
#if BYTE_ORDER == LITTLE_ENDIAN
	int needswap = ump->um_maxsymlinklen <= 0 && nswap == 0;
#else
	int needswap = ump->um_maxsymlinklen <= 0 && nswap != 0;
#endif
	uio = ap->a_uio;
	count = uio->uio_resid;
	rcount = count - ((uio->uio_offset + count) & (ump->um_dirblksiz - 1));

	if (rcount < _DIRENT_MINSIZE(cdp) || count < _DIRENT_MINSIZE(ndp))
		return EINVAL;

	startoff = uio->uio_offset & ~(ump->um_dirblksiz - 1);
	skipbytes = uio->uio_offset - startoff;
	rcount += skipbytes;

	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = startoff;
	auio.uio_resid = rcount;
	UIO_SETUP_SYSSPACE(&auio);
	auio.uio_rw = UIO_READ;
	cdbufsz = rcount;
	cdbuf = kmem_alloc(cdbufsz, KM_SLEEP);
	aiov.iov_base = cdbuf;
	aiov.iov_len = rcount;
	error = UFS_BUFRD(vp, &auio, 0, ap->a_cred);
	if (error != 0) {
		kmem_free(cdbuf, cdbufsz);
		return error;
	}

	rcount -= auio.uio_resid;

	cdp = (struct direct *)(void *)cdbuf;
	ecdp = (struct direct *)(void *)&cdbuf[rcount];

	ndbufsz = count;
	ndbuf = kmem_alloc(ndbufsz, KM_SLEEP);
	ndp = (struct dirent *)(void *)ndbuf;
	endp = &ndbuf[count];

	off = uio->uio_offset;
	if (ap->a_cookies) {
		ccount = rcount / _DIRENT_RECLEN(cdp, 1);
		ccp = *(ap->a_cookies) = malloc(ccount * sizeof(*ccp),
		    M_TEMP, M_WAITOK);
	} else {
		/* XXX: GCC */
		ccount = 0;
		ccp = NULL;
	}

	while (cdp < ecdp) {
		cdp->d_reclen = ufs_rw16(cdp->d_reclen, nswap);
		if (skipbytes > 0) {
			if (cdp->d_reclen <= skipbytes) {
				skipbytes -= cdp->d_reclen;
				cdp = _DIRENT_NEXT(cdp);
				continue;
			}
			/*
			 * invalid cookie.
			 */
			error = EINVAL;
			goto out;
		}
		if (cdp->d_reclen == 0) {
			struct dirent *ondp = ndp;
			ndp->d_reclen = _DIRENT_MINSIZE(ndp);
			ndp = _DIRENT_NEXT(ndp);
			ondp->d_reclen = 0;
			cdp = ecdp;
			break;
		}
		if (needswap) {
			ndp->d_type = cdp->d_namlen;
			ndp->d_namlen = cdp->d_type;
		} else {
			ndp->d_type = cdp->d_type;
			ndp->d_namlen = cdp->d_namlen;
		}
		ndp->d_reclen = _DIRENT_RECLEN(ndp, ndp->d_namlen);
		if ((char *)(void *)ndp + ndp->d_reclen +
		    _DIRENT_MINSIZE(ndp) > endp)
			break;
		ndp->d_fileno = ufs_rw32(cdp->d_ino, nswap);
		(void)memcpy(ndp->d_name, cdp->d_name, ndp->d_namlen);
		memset(&ndp->d_name[ndp->d_namlen], 0,
		    ndp->d_reclen - _DIRENT_NAMEOFF(ndp) - ndp->d_namlen);
		off += cdp->d_reclen;
		if (ap->a_cookies) {
			KASSERT(ccp - *(ap->a_cookies) < ccount);
			*(ccp++) = off;
		}
		ndp = _DIRENT_NEXT(ndp);
		cdp = _DIRENT_NEXT(cdp);
	}

	count = ((char *)(void *)ndp - ndbuf);
	error = uiomove(ndbuf, count, uio);
out:
	if (ap->a_cookies) {
		if (error) {
			free(*(ap->a_cookies), M_TEMP);
			*(ap->a_cookies) = NULL;
			*(ap->a_ncookies) = 0;
		} else {
			*ap->a_ncookies = ccp - *(ap->a_cookies);
		}
	}
	uio->uio_offset = off;
	kmem_free(ndbuf, ndbufsz);
	kmem_free(cdbuf, cdbufsz);
	*ap->a_eofflag = VTOI(vp)->i_size <= uio->uio_offset;
	return error;
}

/*
 * Return target name of a symbolic link
 */
int
ufs_readlink(void *v)
{
	struct vop_readlink_args /* {
		struct vnode	*a_vp;
		struct uio	*a_uio;
		kauth_cred_t	a_cred;
	} */ *ap = v;
	struct vnode	*vp = ap->a_vp;
	struct inode	*ip = VTOI(vp);
	struct ufsmount	*ump = VFSTOUFS(vp->v_mount);
	int		isize;

	/*
	 * The test against um_maxsymlinklen is off by one; it should
	 * theoretically be <=, not <. However, it cannot be changed
	 * as that would break compatibility with existing fs images.
	 */

	isize = ip->i_size;
	if (isize < ump->um_maxsymlinklen ||
	    (ump->um_maxsymlinklen == 0 && DIP(ip, blocks) == 0)) {
		uiomove((char *)SHORTLINK(ip), isize, ap->a_uio);
		return (0);
	}
	return (UFS_BUFRD(vp, ap->a_uio, 0, ap->a_cred));
}

/*
 * Calculate the logical to physical mapping if not done already,
 * then call the device strategy routine.
 */
int
ufs_strategy(void *v)
{
	struct vop_strategy_args /* {
		struct vnode *a_vp;
		struct buf *a_bp;
	} */ *ap = v;
	struct buf	*bp;
	struct vnode	*vp;
	struct inode	*ip;
	struct mount	*mp;
	int		error;

	bp = ap->a_bp;
	vp = ap->a_vp;
	ip = VTOI(vp);
	if (vp->v_type == VBLK || vp->v_type == VCHR)
		panic("ufs_strategy: spec");
	KASSERT(bp->b_bcount != 0);
	if (bp->b_blkno == bp->b_lblkno) {
		error = VOP_BMAP(vp, bp->b_lblkno, NULL, &bp->b_blkno,
				 NULL);
		if (error) {
			bp->b_error = error;
			biodone(bp);
			return (error);
		}
		if (bp->b_blkno == -1) /* no valid data */
			clrbuf(bp);
	}
	if (bp->b_blkno < 0) { /* block is not on disk */
		biodone(bp);
		return (0);
	}
	vp = ip->i_devvp;

	error = VOP_STRATEGY(vp, bp);
	if (error)
		return error;

	if (!BUF_ISREAD(bp))
		return 0;

	mp = wapbl_vptomp(vp);
	if (mp == NULL || mp->mnt_wapbl_replay == NULL ||
	    !WAPBL_REPLAY_ISOPEN(mp) ||
	    !WAPBL_REPLAY_CAN_READ(mp, bp->b_blkno, bp->b_bcount))
		return 0;

	error = biowait(bp);
	if (error)
		return error;

	error = WAPBL_REPLAY_READ(mp, bp->b_data, bp->b_blkno, bp->b_bcount);
	if (error) {
		mutex_enter(&bufcache_lock);
		SET(bp->b_cflags, BC_INVAL);
		mutex_exit(&bufcache_lock);
	}
	return error;
}

/*
 * Print out the contents of an inode.
 */
int
ufs_print(void *v)
{
	struct vop_print_args /* {
		struct vnode	*a_vp;
	} */ *ap = v;
	struct vnode	*vp;
	struct inode	*ip;

	vp = ap->a_vp;
	ip = VTOI(vp);
	printf("tag VT_UFS, ino %llu, on dev %llu, %llu",
	    (unsigned long long)ip->i_number,
	    (unsigned long long)major(ip->i_dev),
	    (unsigned long long)minor(ip->i_dev));
	printf(" flags 0x%x, nlink %d\n",
	    ip->i_flag, ip->i_nlink);
	printf("\tmode 0%o, owner %d, group %d, size %qd",
	    ip->i_mode, ip->i_uid, ip->i_gid,
	    (long long)ip->i_size);
	if (vp->v_type == VFIFO)
		VOCALL(fifo_vnodeop_p, VOFFSET(vop_print), v);
	printf("\n");
	return (0);
}

/*
 * Read wrapper for special devices.
 */
int
ufsspec_read(void *v)
{
	struct vop_read_args /* {
		struct vnode	*a_vp;
		struct uio	*a_uio;
		int		a_ioflag;
		kauth_cred_t	a_cred;
	} */ *ap = v;

	/*
	 * Set access flag.
	 */
	if ((ap->a_vp->v_mount->mnt_flag & MNT_NODEVMTIME) == 0)
		VTOI(ap->a_vp)->i_flag |= IN_ACCESS;
	return (VOCALL (spec_vnodeop_p, VOFFSET(vop_read), ap));
}

/*
 * Write wrapper for special devices.
 */
int
ufsspec_write(void *v)
{
	struct vop_write_args /* {
		struct vnode	*a_vp;
		struct uio	*a_uio;
		int		a_ioflag;
		kauth_cred_t	a_cred;
	} */ *ap = v;

	/*
	 * Set update and change flags.
	 */
	if ((ap->a_vp->v_mount->mnt_flag & MNT_NODEVMTIME) == 0)
		VTOI(ap->a_vp)->i_flag |= IN_MODIFY;
	return (VOCALL (spec_vnodeop_p, VOFFSET(vop_write), ap));
}

/*
 * Close wrapper for special devices.
 *
 * Update the times on the inode then do device close.
 */
int
ufsspec_close(void *v)
{
	struct vop_close_args /* {
		struct vnode	*a_vp;
		int		a_fflag;
		kauth_cred_t	a_cred;
	} */ *ap = v;
	struct vnode	*vp;

	vp = ap->a_vp;
	if (vp->v_usecount > 1)
		UFS_ITIMES(vp, NULL, NULL, NULL);
	return (VOCALL (spec_vnodeop_p, VOFFSET(vop_close), ap));
}

/*
 * Read wrapper for fifo's
 */
int
ufsfifo_read(void *v)
{
	struct vop_read_args /* {
		struct vnode	*a_vp;
		struct uio	*a_uio;
		int		a_ioflag;
		kauth_cred_t	a_cred;
	} */ *ap = v;

	/*
	 * Set access flag.
	 */
	VTOI(ap->a_vp)->i_flag |= IN_ACCESS;
	return (VOCALL (fifo_vnodeop_p, VOFFSET(vop_read), ap));
}

/*
 * Write wrapper for fifo's.
 */
int
ufsfifo_write(void *v)
{
	struct vop_write_args /* {
		struct vnode	*a_vp;
		struct uio	*a_uio;
		int		a_ioflag;
		kauth_cred_t	a_cred;
	} */ *ap = v;

	/*
	 * Set update and change flags.
	 */
	VTOI(ap->a_vp)->i_flag |= IN_MODIFY;
	return (VOCALL (fifo_vnodeop_p, VOFFSET(vop_write), ap));
}

/*
 * Close wrapper for fifo's.
 *
 * Update the times on the inode then do device close.
 */
int
ufsfifo_close(void *v)
{
	struct vop_close_args /* {
		struct vnode	*a_vp;
		int		a_fflag;
		kauth_cred_t	a_cred;
	} */ *ap = v;
	struct vnode	*vp;

	vp = ap->a_vp;
	if (ap->a_vp->v_usecount > 1)
		UFS_ITIMES(vp, NULL, NULL, NULL);
	return (VOCALL (fifo_vnodeop_p, VOFFSET(vop_close), ap));
}

/*
 * Return POSIX pathconf information applicable to ufs filesystems.
 */
int
ufs_pathconf(void *v)
{
	struct vop_pathconf_args /* {
		struct vnode	*a_vp;
		int		a_name;
		register_t	*a_retval;
	} */ *ap = v;

	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = LINK_MAX;
		return (0);
	case _PC_NAME_MAX:
		*ap->a_retval = FFS_MAXNAMLEN;
		return (0);
	case _PC_PATH_MAX:
		*ap->a_retval = PATH_MAX;
		return (0);
	case _PC_PIPE_BUF:
		*ap->a_retval = PIPE_BUF;
		return (0);
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		return (0);
	case _PC_NO_TRUNC:
		*ap->a_retval = 1;
		return (0);
	case _PC_SYNC_IO:
		*ap->a_retval = 1;
		return (0);
	case _PC_FILESIZEBITS:
		*ap->a_retval = 42;
		return (0);
	case _PC_SYMLINK_MAX:
		*ap->a_retval = MAXPATHLEN;
		return (0);
	case _PC_2_SYMLINKS:
		*ap->a_retval = 1;
		return (0);
	default:
		return (EINVAL);
	}
	/* NOTREACHED */
}

/*
 * Advisory record locking support
 */
int
ufs_advlock(void *v)
{
	struct vop_advlock_args /* {
		struct vnode	*a_vp;
		void *		a_id;
		int		a_op;
		struct flock	*a_fl;
		int		a_flags;
	} */ *ap = v;
	struct inode *ip;

	ip = VTOI(ap->a_vp);
	return lf_advlock(ap, &ip->i_lockf, ip->i_size);
}

/*
 * Initialize the vnode associated with a new inode, handle aliased
 * vnodes.
 */
void
ufs_vinit(struct mount *mntp, int (**specops)(void *), int (**fifoops)(void *),
	struct vnode **vpp)
{
	struct timeval	tv;
	struct inode	*ip;
	struct vnode	*vp;
	dev_t		rdev;
	struct ufsmount	*ump;

	vp = *vpp;
	ip = VTOI(vp);
	switch(vp->v_type = IFTOVT(ip->i_mode)) {
	case VCHR:
	case VBLK:
		vp->v_op = specops;
		ump = ip->i_ump;
		if (ump->um_fstype == UFS1)
			rdev = (dev_t)ufs_rw32(ip->i_ffs1_rdev,
			    UFS_MPNEEDSWAP(ump));
		else
			rdev = (dev_t)ufs_rw64(ip->i_ffs2_rdev,
			    UFS_MPNEEDSWAP(ump));
		spec_node_init(vp, rdev);
		break;
	case VFIFO:
		vp->v_op = fifoops;
		break;
	case VNON:
	case VBAD:
	case VSOCK:
	case VLNK:
	case VDIR:
	case VREG:
		break;
	}
	if (ip->i_number == UFS_ROOTINO)
                vp->v_vflag |= VV_ROOT;
	/*
	 * Initialize modrev times
	 */
	getmicrouptime(&tv);
	ip->i_modrev = (uint64_t)(uint)tv.tv_sec << 32
			| tv.tv_usec * 4294u;
	*vpp = vp;
}

/*
 * Allocate a new inode.
 */
int
ufs_makeinode(struct vattr *vap, struct vnode *dvp,
	const struct ufs_lookup_results *ulr,
	struct vnode **vpp, struct componentname *cnp)
{
	struct inode	*ip;
	struct direct	*newdir;
	struct vnode	*tvp;
	int		error;

	UFS_WAPBL_JUNLOCK_ASSERT(dvp->v_mount);

	error = vcache_new(dvp->v_mount, dvp, vap, cnp->cn_cred, &tvp);
	if (error)
		return error;
	error = vn_lock(tvp, LK_EXCLUSIVE);
	if (error) {
		vrele(tvp);
		return error;
	}
	*vpp = tvp;
	ip = VTOI(tvp);
	error = UFS_WAPBL_BEGIN1(dvp->v_mount, dvp);
	if (error) {
		vput(tvp);
		return (error);
	}
	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
	ip->i_nlink = 1;
	DIP_ASSIGN(ip, nlink, 1);

	/* Authorize setting SGID if needed. */
	if (ip->i_mode & ISGID) {
		error = kauth_authorize_vnode(cnp->cn_cred, KAUTH_VNODE_WRITE_SECURITY,
		    tvp, NULL, genfs_can_chmod(tvp->v_type, cnp->cn_cred, ip->i_uid,
		    ip->i_gid, MAKEIMODE(vap->va_type, vap->va_mode)));
		if (error) {
			ip->i_mode &= ~ISGID;
			DIP_ASSIGN(ip, mode, ip->i_mode);
		}
	}

	if (cnp->cn_flags & ISWHITEOUT) {
		ip->i_flags |= UF_OPAQUE;
		DIP_ASSIGN(ip, flags, ip->i_flags);
	}

	/*
	 * Make sure inode goes to disk before directory entry.
	 */
	if ((error = UFS_UPDATE(tvp, NULL, NULL, UPDATE_DIROP)) != 0)
		goto bad;
	newdir = pool_cache_get(ufs_direct_cache, PR_WAITOK);
	ufs_makedirentry(ip, cnp, newdir);
	error = ufs_direnter(dvp, ulr, tvp, newdir, cnp, NULL);
	pool_cache_put(ufs_direct_cache, newdir);
	if (error)
		goto bad;
	*vpp = tvp;
	return (0);

 bad:
	/*
	 * Write error occurred trying to update the inode
	 * or the directory so must deallocate the inode.
	 */
	ip->i_nlink = 0;
	DIP_ASSIGN(ip, nlink, 0);
	ip->i_flag |= IN_CHANGE;
	UFS_WAPBL_UPDATE(tvp, NULL, NULL, 0);
	UFS_WAPBL_END1(dvp->v_mount, dvp);
	vput(tvp);
	return (error);
}

/*
 * Allocate len bytes at offset off.
 */
int
ufs_gop_alloc(struct vnode *vp, off_t off, off_t len, int flags,
    kauth_cred_t cred)
{
        struct inode *ip = VTOI(vp);
        int error, delta, bshift, bsize;
        UVMHIST_FUNC("ufs_gop_alloc"); UVMHIST_CALLED(ubchist);

        error = 0;
        bshift = vp->v_mount->mnt_fs_bshift;
        bsize = 1 << bshift;

        delta = off & (bsize - 1);
        off -= delta;
        len += delta;

        while (len > 0) {
                bsize = MIN(bsize, len);

                error = UFS_BALLOC(vp, off, bsize, cred, flags, NULL);
                if (error) {
                        goto out;
                }

                /*
                 * increase file size now, UFS_BALLOC() requires that
                 * EOF be up-to-date before each call.
                 */

                if (ip->i_size < off + bsize) {
                        UVMHIST_LOG(ubchist, "vp %p old 0x%x new 0x%x",
                            vp, ip->i_size, off + bsize, 0);
                        ip->i_size = off + bsize;
			DIP_ASSIGN(ip, size, ip->i_size);
                }

                off += bsize;
                len -= bsize;
        }

out:
	UFS_WAPBL_UPDATE(vp, NULL, NULL, 0);
	return error;
}

void
ufs_gop_markupdate(struct vnode *vp, int flags)
{
	u_int32_t mask = 0;

	if ((flags & GOP_UPDATE_ACCESSED) != 0) {
		mask = IN_ACCESS;
	}
	if ((flags & GOP_UPDATE_MODIFIED) != 0) {
		if (vp->v_type == VREG) {
			mask |= IN_CHANGE | IN_UPDATE;
		} else {
			mask |= IN_MODIFY;
		}
	}
	if (mask) {
		struct inode *ip = VTOI(vp);

		ip->i_flag |= mask;
	}
}

int
ufs_bufio(enum uio_rw rw, struct vnode *vp, void *buf, size_t len, off_t off,
    int ioflg, kauth_cred_t cred, size_t *aresid, struct lwp *l)
{
	struct iovec iov;
	struct uio uio;
	int error;

	KASSERT(ISSET(ioflg, IO_NODELOCKED));
	KASSERT(VOP_ISLOCKED(vp));
	KASSERT(rw != UIO_WRITE || VOP_ISLOCKED(vp) == LK_EXCLUSIVE);
	KASSERT(rw != UIO_WRITE || vp->v_mount->mnt_wapbl == NULL ||
	    ISSET(ioflg, IO_JOURNALLOCKED));

	iov.iov_base = buf;
	iov.iov_len = len;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_resid = len;
	uio.uio_offset = off;
	uio.uio_rw = rw;
	UIO_SETUP_SYSSPACE(&uio);

	switch (rw) {
	case UIO_READ:
		error = UFS_BUFRD(vp, &uio, ioflg, cred);
		break;
	case UIO_WRITE:
		error = UFS_BUFWR(vp, &uio, ioflg, cred);
		break;
	default:
		panic("invalid uio rw: %d", (int)rw);
	}

	if (aresid)
		*aresid = uio.uio_resid;
	else if (uio.uio_resid && error == 0)
		error = EIO;

	KASSERT(VOP_ISLOCKED(vp));
	KASSERT(rw != UIO_WRITE || VOP_ISLOCKED(vp) == LK_EXCLUSIVE);
	return error;
}
