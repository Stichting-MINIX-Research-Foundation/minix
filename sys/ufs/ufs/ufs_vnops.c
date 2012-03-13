/*	$NetBSD: ufs_vnops.c,v 1.206 2011/11/18 21:18:52 christos Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: ufs_vnops.c,v 1.206 2011/11/18 21:18:52 christos Exp $");

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

/*
 * A virgin directory (no blushing please).
 */
static const struct dirtemplate mastertemplate = {
	0,	12,		DT_DIR,	1,	".",
	0,	DIRBLKSIZ - 12,	DT_DIR,	2,	".."
};

/*
 * Create a regular file
 */
int
ufs_create(void *v)
{
	struct vop_create_args /* {
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
	error =
	    ufs_makeinode(MAKEIMODE(ap->a_vap->va_type, ap->a_vap->va_mode),
			  dvp, ulr, ap->a_vpp, ap->a_cnp);
	if (error) {
		fstrans_done(dvp->v_mount);
		return (error);
	}
	UFS_WAPBL_END1(dvp->v_mount, dvp);
	fstrans_done(dvp->v_mount);
	VN_KNOTE(dvp, NOTE_WRITE);
	return (0);
}

/*
 * Mknod vnode call
 */
/* ARGSUSED */
int
ufs_mknod(void *v)
{
	struct vop_mknod_args /* {
		struct vnode		*a_dvp;
		struct vnode		**a_vpp;
		struct componentname	*a_cnp;
		struct vattr		*a_vap;
	} */ *ap = v;
	struct vattr	*vap;
	struct vnode	**vpp;
	struct inode	*ip;
	int		error;
	struct mount	*mp;
	ino_t		ino;
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
	if ((error =
	    ufs_makeinode(MAKEIMODE(vap->va_type, vap->va_mode),
	    ap->a_dvp, ulr, vpp, ap->a_cnp)) != 0)
		goto out;
	VN_KNOTE(ap->a_dvp, NOTE_WRITE);
	ip = VTOI(*vpp);
	mp  = (*vpp)->v_mount;
	ino = ip->i_number;
	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
	if (vap->va_rdev != VNOVAL) {
		struct ufsmount *ump = ip->i_ump;
		/*
		 * Want to be able to use this to make badblock
		 * inodes, so don't truncate the dev number.
		 */
		if (ump->um_fstype == UFS1)
			ip->i_ffs1_rdev = ufs_rw32(vap->va_rdev,
			    UFS_MPNEEDSWAP(ump));
		else
			ip->i_ffs2_rdev = ufs_rw64(vap->va_rdev,
			    UFS_MPNEEDSWAP(ump));
	}
	UFS_WAPBL_UPDATE(*vpp, NULL, NULL, 0);
	UFS_WAPBL_END1(ap->a_dvp->v_mount, ap->a_dvp);
	/*
	 * Remove inode so that it will be reloaded by VFS_VGET and
	 * checked to see if it is an alias of an existing entry in
	 * the inode cache.
	 */
	(*vpp)->v_type = VNON;
	VOP_UNLOCK(*vpp);
	vgone(*vpp);
	error = VFS_VGET(mp, ino, vpp);
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
	struct inode	*ip;

	vp = ap->a_vp;
	ip = VTOI(vp);
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

	return genfs_can_access(vp->v_type, ip->i_mode & ALLPERMS, ip->i_uid,
	    ip->i_gid, mode, cred);
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
		vap->va_rdev = (dev_t)ufs_rw32(ip->i_ffs1_rdev,
		    UFS_MPNEEDSWAP(ip->i_ump));
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
		vap->va_rdev = (dev_t)ufs_rw64(ip->i_ffs2_rdev,
		    UFS_MPNEEDSWAP(ip->i_ump));
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

	vap = ap->a_vap;
	vp = ap->a_vp;
	ip = VTOI(vp);
	cred = ap->a_cred;
	l = curlwp;

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
		if (kauth_cred_geteuid(cred) != ip->i_uid &&
		    (error = kauth_authorize_generic(cred,
		    KAUTH_GENERIC_ISSUSER, NULL)))
			goto out;
		if (kauth_authorize_generic(cred, KAUTH_GENERIC_ISSUSER,
		    NULL) == 0) {
			if ((ip->i_flags & (SF_IMMUTABLE | SF_APPEND)) &&
			    kauth_authorize_system(l->l_cred,
			     KAUTH_SYSTEM_CHSYSFLAGS, 0, NULL, NULL, NULL)) {
				error = EPERM;
				goto out;
			}
			/* Snapshot flag cannot be set or cleared */
			if ((vap->va_flags & (SF_SNAPSHOT | SF_SNAPINVAL)) !=
			    (ip->i_flags & (SF_SNAPSHOT | SF_SNAPINVAL))) {
				error = EPERM;
				goto out;
			}
			error = UFS_WAPBL_BEGIN(vp->v_mount);
			if (error)
				goto out;
			ip->i_flags = vap->va_flags;
			DIP_ASSIGN(ip, flags, ip->i_flags);
		} else {
			if ((ip->i_flags & (SF_IMMUTABLE | SF_APPEND)) ||
			    (vap->va_flags & UF_SETTABLE) != vap->va_flags) {
				error = EPERM;
				goto out;
			}
			if ((ip->i_flags & SF_SETTABLE) !=
			    (vap->va_flags & SF_SETTABLE)) {
				error = EPERM;
				goto out;
			}
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
			error = UFS_WAPBL_BEGIN(vp->v_mount);
			if (error)
				goto out;
			/*
			 * When journaling, only truncate one indirect block
			 * at a time.
			 */
			if (vp->v_mount->mnt_wapbl) {
				uint64_t incr = MNINDIR(ip->i_ump) <<
				    vp->v_mount->mnt_fs_bshift; /* Power of 2 */
				uint64_t base = NDADDR <<
				    vp->v_mount->mnt_fs_bshift;
				while (!error && ip->i_size > base + incr &&
				    ip->i_size > vap->va_size + incr) {
					/*
					 * round down to next full indirect
					 * block boundary.
					 */
					uint64_t nsize = base +
					    ((ip->i_size - base - 1) &
					    ~(incr - 1));
					error = UFS_TRUNCATE(vp, nsize, 0,
					    cred);
					if (error == 0) {
						UFS_WAPBL_END(vp->v_mount);
						error =
						   UFS_WAPBL_BEGIN(vp->v_mount);
					}
				}
			}
			if (!error)
				error = UFS_TRUNCATE(vp, vap->va_size, 0, cred);
			UFS_WAPBL_END(vp->v_mount);
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
		error = genfs_can_chtimes(vp, vap->va_vaflags, ip->i_uid, cred);
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

	error = genfs_can_chmod(vp, cred, ip->i_uid, ip->i_gid, mode);
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

	error = genfs_can_chown(vp, cred, ip->i_uid, ip->i_gid, uid, gid);
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
	int		error;
	struct ufs_lookup_results *ulr;

	vp = ap->a_vp;
	dvp = ap->a_dvp;
	ip = VTOI(vp);

	/* XXX should handle this material another way */
	ulr = &VTOI(dvp)->i_crap;
	UFS_CHECK_CRAPCOUNTER(VTOI(dvp));

	fstrans_start(dvp->v_mount, FSTRANS_SHARED);
	if (vp->v_type == VDIR || (ip->i_flags & (IMMUTABLE | APPEND)) ||
	    (VTOI(dvp)->i_flags & APPEND))
		error = EPERM;
	else {
		error = UFS_WAPBL_BEGIN(dvp->v_mount);
		if (error == 0) {
			error = ufs_dirremove(dvp, ulr,
					      ip, ap->a_cnp->cn_flags, 0);
			UFS_WAPBL_END(dvp->v_mount);
		}
	}
	VN_KNOTE(vp, NOTE_DELETE);
	VN_KNOTE(dvp, NOTE_WRITE);
	if (dvp == vp)
		vrele(vp);
	else
		vput(vp);
	vput(dvp);
	fstrans_done(dvp->v_mount);
	return (error);
}

/*
 * ufs_link: create hard link.
 */
int
ufs_link(void *v)
{
	struct vop_link_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	struct vnode *dvp = ap->a_dvp;
	struct vnode *vp = ap->a_vp;
	struct componentname *cnp = ap->a_cnp;
	struct inode *ip;
	struct direct *newdir;
	int error;
	struct ufs_lookup_results *ulr;

	KASSERT(dvp != vp);
	KASSERT(vp->v_type != VDIR);
	KASSERT(dvp->v_mount == vp->v_mount);

	/* XXX should handle this material another way */
	ulr = &VTOI(dvp)->i_crap;
	UFS_CHECK_CRAPCOUNTER(VTOI(dvp));

	fstrans_start(dvp->v_mount, FSTRANS_SHARED);
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
	error = UFS_WAPBL_BEGIN(vp->v_mount);
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
	UFS_WAPBL_END(vp->v_mount);
 out1:
	VOP_UNLOCK(vp);
 out2:
	VN_KNOTE(vp, NOTE_LINK);
	VN_KNOTE(dvp, NOTE_WRITE);
	vput(dvp);
	fstrans_done(dvp->v_mount);
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
		newdir->d_ino = WINO;
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


/*
 * Rename vnode operation
 * 	rename("foo", "bar");
 * is essentially
 *	unlink("bar");
 *	link("foo", "bar");
 *	unlink("foo");
 * but ``atomically''.  Can't do full commit without saving state in the
 * inode on disk which isn't feasible at this time.  Best we can do is
 * always guarantee the target exists.
 *
 * Basic algorithm is:
 *
 * 1) Bump link count on source while we're linking it to the
 *    target.  This also ensure the inode won't be deleted out
 *    from underneath us while we work (it may be truncated by
 *    a concurrent `trunc' or `open' for creation).
 * 2) Link source to destination.  If destination already exists,
 *    delete it first.
 * 3) Unlink source reference to inode if still around. If a
 *    directory was moved and the parent of the destination
 *    is different from the source, patch the ".." entry in the
 *    directory.
 */

/*
 * Notes on rename locking:
 *
 * We lock parent vnodes before child vnodes. This means in particular
 * that if A is above B in the directory tree then A must be locked
 * before B. (This is true regardless of how many steps appear in
 * between, because an arbitrary number of other processes could lock
 * parent/child in between and establish a lock cycle and deadlock.)
 *
 * Therefore, if tdvp is above fdvp we must lock tdvp first; if fdvp
 * is above tdvp we must lock fdvp first; and if they're
 * incommensurate it doesn't matter. (But, we rely on the fact that
 * there's a whole-volume rename lock to prevent deadlock among groups
 * of renames upon overlapping sets of incommensurate vnodes.)
 *
 * In addition to establishing lock ordering the parent check also
 * serves to rule out cases where someone tries to move a directory
 * underneath itself, e.g. rename("a/b", "a/b/c"). If allowed to
 * proceed such renames would detach portions of the directory tree
 * and make fsck very unhappy.
 *
 * Note that it is an error for *fvp* to be above tdvp; however,
 * *fdvp* can be above tdvp, as in rename("a/b", "a/c/d").
 *
 * The parent check searches up the tree from tdvp until it either
 * finds fdvp or the root of the volume. It also returns the vnode it
 * saw immediately before fdvp, if any. Later on (after looking up
 * fvp) we will check to see if this *is* fvp and if so fail.
 *
 * If the parent check finds fdvp, it means fdvp is above tdvp, so we
 * lock fdvp first and then tdvp. Otherwise, either tdvp is above fdvp
 * or they're incommensurate and we lock tdvp first.
 *
 * In either case each of the child vnodes has to be looked up and
 * locked immediately after its parent. The cases
 *
 *       fdvp/fvp/[.../]tdvp/tvp
 *       tdvp/tvp/[.../]fdvp/fvp
 *
 * can cause deadlock otherwise. Note that both of these are error
 * cases; the first fails the parent check and the second fails
 * because tvp isn't empty. The parent check case is handled before
 * we start locking; however, the nonempty case requires locking tvp
 * to find out safely that it's nonempty.
 *
 * Therefore the procedure is either
 *
 *   lock fdvp
 *   lookup fvp
 *   lock fvp
 *   lock tdvp
 *   lookup tvp
 *   lock tvp
 *
 * or
 *
 *   lock tdvp
 *   lookup tvp
 *   lock tvp
 *   lock fdvp
 *   lookup fvp
 *   lock fvp
 *
 * This could in principle be simplified by always looking up fvp
 * last; because of the parent check we know by the time we start
 * locking that fvp cannot be directly above tdvp, so (given the
 * whole-volume rename lock and other assumptions) it's safe to lock
 * tdvp before fvp. This would allow the following scheme:
 *
 *   lock fdvp
 *   lock tdvp
 * or
 *   lock tdvp
 *   lock fdvp
 *
 * then
 *   lookup tvp
 *   lock tvp
 *   lookup fvp
 *   check if fvp is above of tdvp, fail if so
 *   lock fvp
 *
 * which is much, much simpler.
 *
 * However, current levels of vfs namei/lookup sanity do not permit
 * this. It is impossible currently to look up fvp without locking it.
 * (It gets locked regardless of whether LOCKLEAF is set; without
 * LOCKLEAF it just gets unlocked again, which doesn't help.)
 *
 * Therefore, because we must look up fvp to know if it's above tdvp,
 * which locks fvp, we must, at least in the case where fdvp is above
 * tdvp, do that before locking tdvp. The longer scheme does that; the
 * simpler scheme is not safe.
 *
 * Note that for now we aren't doing lookup() but relookup(); however,
 * the differences are minor.
 *
 * On top of all the above, just to make everything more
 * exciting, any two of the vnodes might end up being the same.
 *
 * FROMPARENT == FROMCHILD	mv a/. foo	is an error.
 * FROMPARENT == TOPARENT	mv a/b a/c	is ok.
 * FROMPARENT == TOCHILD	mv a/b/c a/b	will give ENOTEMPTY.
 * FROMCHILD == TOPARENT	mv a/b a/b/c	fails the parent check.
 * FROMCHILD == TOCHILD		mv a/b a/b	is ok.
 * TOPARENT == TOCHILD		mv foo a/.	is an error.
 *
 * This introduces more cases in the locking, because each distinct
 * vnode must be locked exactly once.
 *
 * When FROMPARENT == TOPARENT and FROMCHILD != TOCHILD we assume it
 * doesn't matter what order the children are locked in, because the
 * per-volume rename lock excludes other renames and no other
 * operation locks two files in the same directory at once. (Note: if
 * it turns out that link() does, link() is wrong.)
 *
 * Until such time as we can do lookups without the namei and lookup
 * machinery "helpfully" locking the result vnode for us, we can't
 * avoid tripping on cases where FROMCHILD == TOCHILD. Currently for
 * non-directories we unlock the first one we lock while looking up
 * the second, then relock it if necessary. This is more or less
 * harmless since not much of interest can happen to the objects in
 * that window while we have the containing directory locked; but it's
 * not desirable and should be cleaned up when that becomes possible.
 * The right way to do it is to check after looking the second one up
 * and only lock it if it's different. (Note: for directories we don't
 * do this dance because the same directory can't appear more than
 * once.)
 */

/* XXX following lifted from ufs_lookup.c */
#define	FSFMT(vp)	(((vp)->v_mount->mnt_iflag & IMNT_DTYPE) == 0)

/*
 * Check if either entry referred to by FROM_ULR is within the range
 * of entries named by TO_ULR.
 */
static int
ulr_overlap(const struct ufs_lookup_results *from_ulr,
	    const struct ufs_lookup_results *to_ulr)
{
	doff_t from_start, from_prevstart;
	doff_t to_start, to_end;

	/*
	 * FROM is a DELETE result; offset points to the entry to
	 * remove and subtracting count gives the previous entry.
	 */
	from_start = from_ulr->ulr_offset - from_ulr->ulr_count;
	from_prevstart = from_ulr->ulr_offset;

	/*
	 * TO is a RENAME (thus non-DELETE) result; offset points
	 * to the beginning of a region to write in, and adding
	 * count gives the end of the region.
	 */
	to_start = to_ulr->ulr_offset;
	to_end = to_ulr->ulr_offset + to_ulr->ulr_count;

	if (from_prevstart >= to_start && from_prevstart < to_end) {
		return 1;
	}
	if (from_start >= to_start && from_start < to_end) {
		return 1;
	}
	return 0;
}

/*
 * Wrapper for relookup that also updates the supplemental results.
 */
static int
do_relookup(struct vnode *dvp, struct ufs_lookup_results *ulr,
	    struct vnode **vp, struct componentname *cnp)
{
	int error;

	error = relookup(dvp, vp, cnp, 0);
	if (error) {
		return error;
	}
	/* update the supplemental reasults */
	*ulr = VTOI(dvp)->i_crap;
	UFS_CHECK_CRAPCOUNTER(VTOI(dvp));
	return 0;
}

/*
 * Lock and relookup a sequence of two directories and two children.
 *
 */
static int
lock_vnode_sequence(struct vnode *d1, struct ufs_lookup_results *ulr1,
		    struct vnode **v1_ret, struct componentname *cn1, 
		    int v1_missing_ok,
		    int overlap_error,
		    struct vnode *d2, struct ufs_lookup_results *ulr2,
		    struct vnode **v2_ret, struct componentname *cn2, 
		    int v2_missing_ok)
{
	struct vnode *v1, *v2;
	int error;

	KASSERT(d1 != d2);

	vn_lock(d1, LK_EXCLUSIVE | LK_RETRY);
	if (VTOI(d1)->i_size == 0) {
		/* d1 has been rmdir'd */
		VOP_UNLOCK(d1);
		return ENOENT;
	}
	error = do_relookup(d1, ulr1, &v1, cn1);
	if (v1_missing_ok) {
		if (error == ENOENT) {
			/*
			 * Note: currently if the name doesn't exist,
			 * relookup succeeds (it intercepts the
			 * EJUSTRETURN from VOP_LOOKUP) and sets tvp
			 * to NULL. Therefore, we will never get
			 * ENOENT and this branch is not needed.
			 * However, in a saner future the EJUSTRETURN
			 * garbage will go away, so let's DTRT.
			 */
			v1 = NULL;
			error = 0;
		}
	} else {
		if (error == 0 && v1 == NULL) {
			/* This is what relookup sets if v1 disappeared. */
			error = ENOENT;
		}
	}
	if (error) {
		VOP_UNLOCK(d1);
		return error;
	}
	if (v1 && v1 == d2) {
		VOP_UNLOCK(d1);
		VOP_UNLOCK(v1);
		vrele(v1);
		return overlap_error;
	}

	/*
	 * The right way to do this is to do lookups without locking
	 * the results, and lock the results afterwards; then at the
	 * end we can avoid trying to lock v2 if v2 == v1.
	 *
	 * However, for the reasons described in the fdvp == tdvp case
	 * in rename below, we can't do that safely. So, in the case
	 * where v1 is not a directory, unlock it and lock it again
	 * afterwards. This is safe in locking order because a
	 * non-directory can't be above anything else in the tree. If
	 * v1 *is* a directory, that's not true, but then because d1
	 * != d2, v1 != v2.
	 */
	if (v1 && v1->v_type != VDIR) {
		VOP_UNLOCK(v1);
	}
	vn_lock(d2, LK_EXCLUSIVE | LK_RETRY);
	if (VTOI(d2)->i_size == 0) {
		/* d2 has been rmdir'd */
		VOP_UNLOCK(d2);
		if (v1 && v1->v_type == VDIR) {
			VOP_UNLOCK(v1);
		}
		VOP_UNLOCK(d1);
		if (v1) {
			vrele(v1);
		}
		return ENOENT;
	}
	error = do_relookup(d2, ulr2, &v2, cn2);
	if (v2_missing_ok) {
		if (error == ENOENT) {
			/* as above */
			v2 = NULL;
			error = 0;
		}
	} else {
		if (error == 0 && v2 == NULL) {
			/* This is what relookup sets if v2 disappeared. */
			error = ENOENT;
		}
	}
	if (error) {
		VOP_UNLOCK(d2);
		if (v1 && v1->v_type == VDIR) {
			VOP_UNLOCK(v1);
		}
		VOP_UNLOCK(d1);
		if (v1) {
			vrele(v1);
		}
		return error;
	}
	if (v1 && v1->v_type != VDIR && v1 != v2) {
		vn_lock(v1, LK_EXCLUSIVE | LK_RETRY);
	}
	*v1_ret = v1;
	*v2_ret = v2;
	return 0;
}

/*
 * Rename vnode operation
 * 	rename("foo", "bar");
 * is essentially
 *	unlink("bar");
 *	link("foo", "bar");
 *	unlink("foo");
 * but ``atomically''.  Can't do full commit without saving state in the
 * inode on disk which isn't feasible at this time.  Best we can do is
 * always guarantee the target exists.
 *
 * Basic algorithm is:
 *
 * 1) Bump link count on source while we're linking it to the
 *    target.  This also ensure the inode won't be deleted out
 *    from underneath us while we work (it may be truncated by
 *    a concurrent `trunc' or `open' for creation).
 * 2) Link source to destination.  If destination already exists,
 *    delete it first.
 * 3) Unlink source reference to inode if still around. If a
 *    directory was moved and the parent of the destination
 *    is different from the source, patch the ".." entry in the
 *    directory.
 */
int
ufs_rename(void *v)
{
	struct vop_rename_args  /* {
		struct vnode		*a_fdvp;
		struct vnode		*a_fvp;
		struct componentname	*a_fcnp;
		struct vnode		*a_tdvp;
		struct vnode		*a_tvp;
		struct componentname	*a_tcnp;
	} */ *ap = v;
	struct vnode		*tvp, *tdvp, *fvp, *fdvp;
	struct componentname	*tcnp, *fcnp;
	struct inode		*ip, *txp, *fxp, *tdp, *fdp;
	struct mount		*mp;
	struct direct		*newdir;
	int			doingdirectory, error;
	ino_t			oldparent, newparent;

	struct ufs_lookup_results from_ulr, to_ulr;

	tvp = ap->a_tvp;
	tdvp = ap->a_tdvp;
	fvp = ap->a_fvp;
	fdvp = ap->a_fdvp;
	tcnp = ap->a_tcnp;
	fcnp = ap->a_fcnp;
	doingdirectory = error = 0;
	oldparent = newparent = 0;

	/* save the supplemental lookup results as they currently exist */
	from_ulr = VTOI(fdvp)->i_crap;
	to_ulr = VTOI(tdvp)->i_crap;
	UFS_CHECK_CRAPCOUNTER(VTOI(fdvp));
	UFS_CHECK_CRAPCOUNTER(VTOI(tdvp));

	/*
	 * Owing to VFS oddities we are currently called with tdvp/tvp
	 * locked and not fdvp/fvp. In a sane world we'd be passed
	 * tdvp and fdvp only, unlocked, and two name strings. Pretend
	 * we have a sane world and unlock tdvp and tvp.
	 */
	VOP_UNLOCK(tdvp);
	if (tvp && tvp != tdvp) {
		VOP_UNLOCK(tvp);
	}

	/* Also pretend we have a sane world and vrele fvp/tvp. */
	vrele(fvp);
	fvp = NULL;
	if (tvp) {
		vrele(tvp);
		tvp = NULL;
	}

	/*
	 * Check for cross-device rename.
	 */
	if (fdvp->v_mount != tdvp->v_mount) {
		error = EXDEV;
		goto abort;
	}

	/*
	 * Reject "." and ".."
	 */
	if ((fcnp->cn_flags & ISDOTDOT) || (tcnp->cn_flags & ISDOTDOT) ||
	    (fcnp->cn_namelen == 1 && fcnp->cn_nameptr[0] == '.') ||
	    (tcnp->cn_namelen == 1 && tcnp->cn_nameptr[0] == '.')) {
		error = EINVAL;
		goto abort;
	}
	    
	/*
	 * Get locks.
	 */

	/* paranoia */
	fcnp->cn_flags |= LOCKPARENT|LOCKLEAF;
	tcnp->cn_flags |= LOCKPARENT|LOCKLEAF;

	if (fdvp == tdvp) {
		/* One directory. Lock it and relookup both children. */
		vn_lock(fdvp, LK_EXCLUSIVE | LK_RETRY);

		if (VTOI(fdvp)->i_size == 0) {
			/* directory has been rmdir'd */
			VOP_UNLOCK(fdvp);
			error = ENOENT;
			goto abort;
		}

		error = do_relookup(fdvp, &from_ulr, &fvp, fcnp);
		if (error == 0 && fvp == NULL) {
			/* relookup may produce this if fvp disappears */
			error = ENOENT;
		}
		if (error) {
			VOP_UNLOCK(fdvp);
			goto abort;
		}

		/*
		 * The right way to do this is to look up both children
		 * without locking either, and then lock both unless they
		 * turn out to be the same. However, due to deep-seated
		 * VFS-level issues all lookups lock the child regardless
		 * of whether LOCKLEAF is set (if LOCKLEAF is not set,
		 * the child is locked during lookup and then unlocked)
		 * so it is not safe to look up tvp while fvp is locked.
		 *
		 * Unlocking fvp here temporarily is more or less safe,
		 * because with the directory locked there's not much
		 * that can happen to it. However, ideally it wouldn't
		 * be necessary. XXX.
		 */
		VOP_UNLOCK(fvp);
		/* remember fdvp == tdvp so tdvp is locked */
		error = do_relookup(tdvp, &to_ulr, &tvp, tcnp);
		if (error && error != ENOENT) {
			VOP_UNLOCK(fdvp);
			goto abort;
		}
		if (error == ENOENT) {
			/*
			 * Note: currently if the name doesn't exist,
			 * relookup succeeds (it intercepts the
			 * EJUSTRETURN from VOP_LOOKUP) and sets tvp
			 * to NULL. Therefore, we will never get
			 * ENOENT and this branch is not needed.
			 * However, in a saner future the EJUSTRETURN
			 * garbage will go away, so let's DTRT.
			 */
			tvp = NULL;
		}

		/* tvp is locked; lock fvp if necessary */
		if (!tvp || tvp != fvp) {
			vn_lock(fvp, LK_EXCLUSIVE | LK_RETRY);
		}
	} else {
		int found_fdvp;
		struct vnode *illegal_fvp;

		/*
		 * The source must not be above the destination. (If
		 * it were, the rename would detach a section of the
		 * tree.)
		 *
		 * Look up the tree from tdvp to see if we find fdvp,
		 * and if so, return the immediate child of fdvp we're
		 * under; that must not turn out to be the same as
		 * fvp.
		 *
		 * The per-volume rename lock guarantees that the
		 * result of this check remains true until we finish
		 * looking up and locking.
		 */
		error = ufs_parentcheck(fdvp, tdvp, fcnp->cn_cred,
					&found_fdvp, &illegal_fvp);
		if (error) {
			goto abort;
		}

		/* Must lock in tree order. */

		if (found_fdvp) {
			/* fdvp -> fvp -> tdvp -> tvp */
			error = lock_vnode_sequence(fdvp, &from_ulr,
						    &fvp, fcnp, 0,
						    EINVAL,
						    tdvp, &to_ulr,
						    &tvp, tcnp, 1);
		} else {
			/* tdvp -> tvp -> fdvp -> fvp */
			error = lock_vnode_sequence(tdvp, &to_ulr,
						    &tvp, tcnp, 1,
						    ENOTEMPTY,
						    fdvp, &from_ulr,
						    &fvp, fcnp, 0);
		}
		if (error) {
			if (illegal_fvp) {
				vrele(illegal_fvp);
			}
			goto abort;
		}
		KASSERT(fvp != NULL);

		if (illegal_fvp && fvp == illegal_fvp) {
			vrele(illegal_fvp);
			error = EINVAL;
			goto abort_withlocks;
		}

		if (illegal_fvp) {
			vrele(illegal_fvp);
		}
	}

	KASSERT(fdvp && VOP_ISLOCKED(fdvp));
	KASSERT(fvp && VOP_ISLOCKED(fvp));
	KASSERT(tdvp && VOP_ISLOCKED(tdvp));
	KASSERT(tvp == NULL || VOP_ISLOCKED(tvp));

	/* --- everything is now locked --- */

	if (tvp && ((VTOI(tvp)->i_flags & (IMMUTABLE | APPEND)) ||
	    (VTOI(tdvp)->i_flags & APPEND))) {
		error = EPERM;
		goto abort_withlocks;
	}

	/*
	 * Check if just deleting a link name.
	 */
	if (fvp == tvp) {
		if (fvp->v_type == VDIR) {
			error = EINVAL;
			goto abort_withlocks;
		}

		/* Release destination completely. Leave fdvp locked. */
		VOP_ABORTOP(tdvp, tcnp);
		if (fdvp != tdvp) {
			VOP_UNLOCK(tdvp);
		}
		VOP_UNLOCK(tvp);
		vrele(tdvp);
		vrele(tvp);

		/* Delete source. */
		/* XXX: do we really need to relookup again? */

		/*
		 * fdvp is still locked, but we just unlocked fvp
		 * (because fvp == tvp) so just decref fvp
		 */
		vrele(fvp);
		fcnp->cn_flags &= ~(MODMASK);
		fcnp->cn_flags |= LOCKPARENT | LOCKLEAF;
		fcnp->cn_nameiop = DELETE;
		if ((error = relookup(fdvp, &fvp, fcnp, 0))) {
			vput(fdvp);
			return (error);
		}
		return (VOP_REMOVE(fdvp, fvp, fcnp));
	}
	fdp = VTOI(fdvp);
	ip = VTOI(fvp);
	if ((nlink_t) ip->i_nlink >= LINK_MAX) {
		error = EMLINK;
		goto abort_withlocks;
	}
	if ((ip->i_flags & (IMMUTABLE | APPEND)) ||
		(fdp->i_flags & APPEND)) {
		error = EPERM;
		goto abort_withlocks;
	}
	if ((ip->i_mode & IFMT) == IFDIR) {
		/*
		 * Avoid ".", "..", and aliases of "." for obvious reasons.
		 */
		if ((fcnp->cn_namelen == 1 && fcnp->cn_nameptr[0] == '.') ||
		    fdp == ip ||
		    (fcnp->cn_flags & ISDOTDOT) ||
		    (tcnp->cn_flags & ISDOTDOT) ||
		    (ip->i_flag & IN_RENAME)) {
			error = EINVAL;
			goto abort_withlocks;
		}
		ip->i_flag |= IN_RENAME;
		doingdirectory = 1;
	}
	oldparent = fdp->i_number;
	VN_KNOTE(fdvp, NOTE_WRITE);		/* XXXLUKEM/XXX: right place? */

	/*
	 * Both the directory
	 * and target vnodes are locked.
	 */
	tdp = VTOI(tdvp);
	txp = NULL;
	if (tvp)
		txp = VTOI(tvp);

	mp = fdvp->v_mount;
	fstrans_start(mp, FSTRANS_SHARED);

	if (oldparent != tdp->i_number)
		newparent = tdp->i_number;

	/*
	 * If ".." must be changed (ie the directory gets a new
	 * parent) the user must have write permission in the source
	 * so as to be able to change "..".
	 */
	if (doingdirectory && newparent) {
		error = VOP_ACCESS(fvp, VWRITE, tcnp->cn_cred);
		if (error)
			goto out;
	}

	KASSERT(fdvp != tvp);

	if (newparent) {
		/* Check for the rename("foo/foo", "foo") case. */
		if (fdvp == tvp) {
			error = doingdirectory ? ENOTEMPTY : EISDIR;
			goto out;
		}
	}

	fxp = VTOI(fvp);
	fdp = VTOI(fdvp);

	error = UFS_WAPBL_BEGIN(fdvp->v_mount);
	if (error)
		goto out2;

	/*
	 * 1) Bump link count while we're moving stuff
	 *    around.  If we crash somewhere before
	 *    completing our work, the link count
	 *    may be wrong, but correctable.
	 */
	ip->i_nlink++;
	DIP_ASSIGN(ip, nlink, ip->i_nlink);
	ip->i_flag |= IN_CHANGE;
	if ((error = UFS_UPDATE(fvp, NULL, NULL, UPDATE_DIROP)) != 0) {
		goto bad;
	}

	/*
	 * 2) If target doesn't exist, link the target
	 *    to the source and unlink the source.
	 *    Otherwise, rewrite the target directory
	 *    entry to reference the source inode and
	 *    expunge the original entry's existence.
	 */
	if (txp == NULL) {
		if (tdp->i_dev != ip->i_dev)
			panic("rename: EXDEV");
		/*
		 * Account for ".." in new directory.
		 * When source and destination have the same
		 * parent we don't fool with the link count.
		 */
		if (doingdirectory && newparent) {
			if ((nlink_t)tdp->i_nlink >= LINK_MAX) {
				error = EMLINK;
				goto bad;
			}
			tdp->i_nlink++;
			DIP_ASSIGN(tdp, nlink, tdp->i_nlink);
			tdp->i_flag |= IN_CHANGE;
			if ((error = UFS_UPDATE(tdvp, NULL, NULL,
			    UPDATE_DIROP)) != 0) {
				tdp->i_nlink--;
				DIP_ASSIGN(tdp, nlink, tdp->i_nlink);
				tdp->i_flag |= IN_CHANGE;
				goto bad;
			}
		}
		newdir = pool_cache_get(ufs_direct_cache, PR_WAITOK);
		ufs_makedirentry(ip, tcnp, newdir);
		error = ufs_direnter(tdvp, &to_ulr,
				     NULL, newdir, tcnp, NULL);
		pool_cache_put(ufs_direct_cache, newdir);
		if (error != 0) {
			if (doingdirectory && newparent) {
				tdp->i_nlink--;
				DIP_ASSIGN(tdp, nlink, tdp->i_nlink);
				tdp->i_flag |= IN_CHANGE;
				(void)UFS_UPDATE(tdvp, NULL, NULL,
						 UPDATE_WAIT | UPDATE_DIROP);
			}
			goto bad;
		}
		VN_KNOTE(tdvp, NOTE_WRITE);
	} else {
		if (txp->i_dev != tdp->i_dev || txp->i_dev != ip->i_dev)
			panic("rename: EXDEV");
		/*
		 * Short circuit rename(foo, foo).
		 */
		if (txp->i_number == ip->i_number)
			panic("rename: same file");
		/*
		 * If the parent directory is "sticky", then the user must
		 * own the parent directory, or the destination of the rename,
		 * otherwise the destination may not be changed (except by
		 * root). This implements append-only directories.
		 */
		if ((tdp->i_mode & S_ISTXT) &&
		    kauth_authorize_generic(tcnp->cn_cred,
		     KAUTH_GENERIC_ISSUSER, NULL) != 0 &&
		    kauth_cred_geteuid(tcnp->cn_cred) != tdp->i_uid &&
		    txp->i_uid != kauth_cred_geteuid(tcnp->cn_cred)) {
			error = EPERM;
			goto bad;
		}
		/*
		 * Target must be empty if a directory and have no links
		 * to it. Also, ensure source and target are compatible
		 * (both directories, or both not directories).
		 */
		if ((txp->i_mode & IFMT) == IFDIR) {
			if (txp->i_nlink > 2 ||
			    !ufs_dirempty(txp, tdp->i_number, tcnp->cn_cred)) {
				error = ENOTEMPTY;
				goto bad;
			}
			if (!doingdirectory) {
				error = ENOTDIR;
				goto bad;
			}
			cache_purge(tdvp);
		} else if (doingdirectory) {
			error = EISDIR;
			goto bad;
		}
		if ((error = ufs_dirrewrite(tdp, to_ulr.ulr_offset,
		    txp, ip->i_number,
		    IFTODT(ip->i_mode), doingdirectory && newparent ?
		    newparent : doingdirectory, IN_CHANGE | IN_UPDATE)) != 0)
			goto bad;
		if (doingdirectory) {
			/*
			 * Truncate inode. The only stuff left in the directory
			 * is "." and "..". The "." reference is inconsequential
			 * since we are quashing it. We have removed the "."
			 * reference and the reference in the parent directory,
			 * but there may be other hard links.
			 */
			if (!newparent) {
				tdp->i_nlink--;
				DIP_ASSIGN(tdp, nlink, tdp->i_nlink);
				tdp->i_flag |= IN_CHANGE;
				UFS_WAPBL_UPDATE(tdvp, NULL, NULL, 0);
			}
			txp->i_nlink--;
			DIP_ASSIGN(txp, nlink, txp->i_nlink);
			txp->i_flag |= IN_CHANGE;
			if ((error = UFS_TRUNCATE(tvp, (off_t)0, IO_SYNC,
			    tcnp->cn_cred)))
				goto bad;
		}
		VN_KNOTE(tdvp, NOTE_WRITE);
		VN_KNOTE(tvp, NOTE_DELETE);
	}

	/*
	 * Handle case where the directory entry we need to remove,
	 * which is/was at from_ulr.ulr_offset, or the one before it,
	 * which is/was at from_ulr.ulr_offset - from_ulr.ulr_count,
	 * may have been moved when the directory insertion above
	 * performed compaction.
	 */
	if (tdp->i_number == fdp->i_number &&
	    ulr_overlap(&from_ulr, &to_ulr)) {

		struct buf *bp;
		struct direct *ep;
		struct ufsmount *ump = fdp->i_ump;
		doff_t curpos;
		doff_t endsearch;	/* offset to end directory search */
		uint32_t prev_reclen;
		int dirblksiz = ump->um_dirblksiz;
		const int needswap = UFS_MPNEEDSWAP(ump);
		u_long bmask;
		int namlen, entryoffsetinblock;
		char *dirbuf;

		bmask = fdvp->v_mount->mnt_stat.f_iosize - 1;

		/*
		 * The fcnp entry will be somewhere between the start of
		 * compaction (to_ulr.ulr_offset) and the original location
		 * (from_ulr.ulr_offset).
		 */
		curpos = to_ulr.ulr_offset;
		endsearch = from_ulr.ulr_offset + from_ulr.ulr_reclen;
		entryoffsetinblock = 0;

		/*
		 * Get the directory block containing the start of
		 * compaction.
		 */
		error = ufs_blkatoff(fdvp, (off_t)to_ulr.ulr_offset, &dirbuf,
		    &bp, false);
		if (error)
			goto bad;

		/*
		 * Keep existing ulr_count (length of previous record)
		 * for the case where compaction did not include the
		 * previous entry but started at the from-entry.
		 */
		prev_reclen = from_ulr.ulr_count;

		while (curpos < endsearch) {
			uint32_t reclen;

			/*
			 * If necessary, get the next directory block.
			 *
			 * dholland 7/13/11 to the best of my understanding
			 * this should never happen; compaction occurs only
			 * within single blocks. I think.
			 */
			if ((curpos & bmask) == 0) {
				if (bp != NULL)
					brelse(bp, 0);
				error = ufs_blkatoff(fdvp, (off_t)curpos,
				    &dirbuf, &bp, false);
				if (error)
					goto bad;
				entryoffsetinblock = 0;
			}

			KASSERT(bp != NULL);
			ep = (struct direct *)(dirbuf + entryoffsetinblock);
			reclen = ufs_rw16(ep->d_reclen, needswap);

#if (BYTE_ORDER == LITTLE_ENDIAN)
			if (FSFMT(fdvp) && needswap == 0)
				namlen = ep->d_type;
			else
				namlen = ep->d_namlen;
#else
			if (FSFMT(fdvp) && needswap != 0)
				namlen = ep->d_type;
			else
				namlen = ep->d_namlen;
#endif
			if ((ep->d_ino != 0) &&
			    (ufs_rw32(ep->d_ino, needswap) != WINO) &&
			    (namlen == fcnp->cn_namelen) &&
			    memcmp(ep->d_name, fcnp->cn_nameptr, namlen) == 0) {
				from_ulr.ulr_reclen = reclen;
				break;
			}
			curpos += reclen;
			entryoffsetinblock += reclen;
			prev_reclen = reclen;
		}

		from_ulr.ulr_offset = curpos;
		from_ulr.ulr_count = prev_reclen;

		KASSERT(curpos <= endsearch);

		/*
		 * If ulr_offset points to start of a directory block,
		 * clear ulr_count so ufs_dirremove() doesn't try to
		 * merge free space over a directory block boundary.
		 */
		if ((from_ulr.ulr_offset & (dirblksiz - 1)) == 0)
			from_ulr.ulr_count = 0;

		brelse(bp, 0);
	}

	/*
	 * 3) Unlink the source.
	 */

#if 0
	/*
	 * Ensure that the directory entry still exists and has not
	 * changed while the new name has been entered. If the source is
	 * a file then the entry may have been unlinked or renamed. In
	 * either case there is no further work to be done. If the source
	 * is a directory then it cannot have been rmdir'ed; The IRENAME
	 * flag ensures that it cannot be moved by another rename or removed
	 * by a rmdir.
	 */
#endif
	KASSERT(fxp == ip);

	/*
	 * If the source is a directory with a new parent, the link
	 * count of the old parent directory must be decremented and
	 * ".." set to point to the new parent.
	 */
	if (doingdirectory && newparent) {
		KASSERT(fdp != NULL);
		ufs_dirrewrite(fxp, mastertemplate.dot_reclen,
			       fdp, newparent, DT_DIR, 0, IN_CHANGE);
		cache_purge(fdvp);
	}
	error = ufs_dirremove(fdvp, &from_ulr,
			      fxp, fcnp->cn_flags, 0);
	fxp->i_flag &= ~IN_RENAME;

	VN_KNOTE(fvp, NOTE_RENAME);
	goto done;

 out:
	goto out2;

	/* exit routines from steps 1 & 2 */
 bad:
	if (doingdirectory)
		ip->i_flag &= ~IN_RENAME;
	ip->i_nlink--;
	DIP_ASSIGN(ip, nlink, ip->i_nlink);
	ip->i_flag |= IN_CHANGE;
	ip->i_flag &= ~IN_RENAME;
	UFS_WAPBL_UPDATE(fvp, NULL, NULL, 0);
 done:
	UFS_WAPBL_END(fdvp->v_mount);
 out2:
	/*
	 * clear IN_RENAME - some exit paths happen too early to go
	 * through the cleanup done in the "bad" case above, so we
	 * always do this mini-cleanup here.
	 */
	ip->i_flag &= ~IN_RENAME;

	VOP_UNLOCK(fdvp);
	if (tdvp != fdvp) {
		VOP_UNLOCK(tdvp);
	}
	VOP_UNLOCK(fvp);
	if (tvp && tvp != fvp) {
		VOP_UNLOCK(tvp);
	}

	vrele(fdvp);
	vrele(tdvp);
	vrele(fvp);
	if (tvp) {
		vrele(tvp);
	}

	fstrans_done(mp);
	return (error);

 abort_withlocks:
	VOP_UNLOCK(fdvp);
	if (tdvp != fdvp) {
		VOP_UNLOCK(tdvp);
	}
	VOP_UNLOCK(fvp);
	if (tvp && tvp != fvp) {
		VOP_UNLOCK(tvp);
	}

 abort:
	VOP_ABORTOP(fdvp, fcnp); /* XXX, why not in NFS? */
	VOP_ABORTOP(tdvp, tcnp); /* XXX, why not in NFS? */
	vrele(tdvp);
	if (tvp) {
		vrele(tvp);
	}
	vrele(fdvp);
	if (fvp) {
		vrele(fvp);
	}
	return (error);
}

int
ufs_mkdir(void *v)
{
	struct vop_mkdir_args /* {
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
	int			error, dmode;
	struct ufsmount		*ump = dp->i_ump;
	int			dirblksiz = ump->um_dirblksiz;
	struct ufs_lookup_results *ulr;

	fstrans_start(dvp->v_mount, FSTRANS_SHARED);

	/* XXX should handle this material another way */
	ulr = &dp->i_crap;
	UFS_CHECK_CRAPCOUNTER(dp);

	if ((nlink_t)dp->i_nlink >= LINK_MAX) {
		error = EMLINK;
		goto out;
	}
	dmode = vap->va_mode & ACCESSPERMS;
	dmode |= IFDIR;
	/*
	 * Must simulate part of ufs_makeinode here to acquire the inode,
	 * but not have it entered in the parent directory. The entry is
	 * made later after writing "." and ".." entries.
	 */
	if ((error = UFS_VALLOC(dvp, dmode, cnp->cn_cred, ap->a_vpp)) != 0)
		goto out;

	tvp = *ap->a_vpp;
	ip = VTOI(tvp);

	error = UFS_WAPBL_BEGIN(ap->a_dvp->v_mount);
	if (error) {
		UFS_VFREE(tvp, ip->i_number, dmode);
		vput(tvp);
		goto out;
	}
	ip->i_uid = kauth_cred_geteuid(cnp->cn_cred);
	DIP_ASSIGN(ip, uid, ip->i_uid);
	ip->i_gid = dp->i_gid;
	DIP_ASSIGN(ip, gid, ip->i_gid);
#if defined(QUOTA) || defined(QUOTA2)
	if ((error = chkiq(ip, 1, cnp->cn_cred, 0))) {
		UFS_VFREE(tvp, ip->i_number, dmode);
		UFS_WAPBL_END(dvp->v_mount);
		fstrans_done(dvp->v_mount);
		vput(tvp);
		vput(dvp);
		return (error);
	}
#endif
	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
	ip->i_mode = dmode;
	DIP_ASSIGN(ip, mode, dmode);
	tvp->v_type = VDIR;	/* Rest init'd in getnewvnode(). */
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
	 * Directory set up, now install it's entry in the parent directory.
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
		/* If IN_ADIROP, account for it */
		UFS_UNMARK_VNODE(tvp);
		UFS_WAPBL_UPDATE(tvp, NULL, NULL, UPDATE_DIROP);
		UFS_WAPBL_END(dvp->v_mount);
		vput(tvp);
	}
 out:
	fstrans_done(dvp->v_mount);
	vput(dvp);
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
	if (ip->i_flag & IN_RENAME) {
		error = EINVAL;
		goto out;
	}
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
	struct vop_symlink_args /* {
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
	error = ufs_makeinode(IFLNK | ap->a_vap->va_mode, ap->a_dvp, ulr,
			      vpp, ap->a_cnp);
	if (error)
		goto out;
	VN_KNOTE(ap->a_dvp, NOTE_WRITE);
	vp = *vpp;
	len = strlen(ap->a_target);
	ip = VTOI(vp);
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
		error = vn_rdwr(UIO_WRITE, vp, ap->a_target, len, (off_t)0,
		    UIO_SYSSPACE, IO_NODELOCKED | IO_JOURNALLOCKED,
		    ap->a_cnp->cn_cred, NULL, NULL);
	UFS_WAPBL_END1(ap->a_dvp->v_mount, ap->a_dvp);
	if (error)
		vput(vp);
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
	size_t		count, ccount, rcount;
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
	cdbuf = malloc(rcount, M_TEMP, M_WAITOK);
	aiov.iov_base = cdbuf;
	aiov.iov_len = rcount;
	error = VOP_READ(vp, &auio, 0, ap->a_cred);
	if (error != 0) {
		free(cdbuf, M_TEMP);
		return error;
	}

	rcount -= auio.uio_resid;

	cdp = (struct direct *)(void *)cdbuf;
	ecdp = (struct direct *)(void *)&cdbuf[rcount];

	ndbuf = malloc(count, M_TEMP, M_WAITOK);
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
	free(ndbuf, M_TEMP);
	free(cdbuf, M_TEMP);
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

	isize = ip->i_size;
	if (isize < ump->um_maxsymlinklen ||
	    (ump->um_maxsymlinklen == 0 && DIP(ip, blocks) == 0)) {
		uiomove((char *)SHORTLINK(ip), isize, ap->a_uio);
		return (0);
	}
	return (VOP_READ(vp, ap->a_uio, 0, ap->a_cred));
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
	struct inode	*ip;

	vp = ap->a_vp;
	ip = VTOI(vp);
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
	struct inode	*ip;

	vp = ap->a_vp;
	ip = VTOI(vp);
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
	if (ip->i_number == ROOTINO)
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
ufs_makeinode(int mode, struct vnode *dvp, const struct ufs_lookup_results *ulr,
	struct vnode **vpp, struct componentname *cnp)
{
	struct inode	*ip, *pdir;
	struct direct	*newdir;
	struct vnode	*tvp;
	int		error, ismember = 0;

	UFS_WAPBL_JUNLOCK_ASSERT(dvp->v_mount);

	pdir = VTOI(dvp);

	if ((mode & IFMT) == 0)
		mode |= IFREG;

	if ((error = UFS_VALLOC(dvp, mode, cnp->cn_cred, vpp)) != 0) {
		vput(dvp);
		return (error);
	}
	tvp = *vpp;
	ip = VTOI(tvp);
	ip->i_gid = pdir->i_gid;
	DIP_ASSIGN(ip, gid, ip->i_gid);
	ip->i_uid = kauth_cred_geteuid(cnp->cn_cred);
	DIP_ASSIGN(ip, uid, ip->i_uid);
	error = UFS_WAPBL_BEGIN1(dvp->v_mount, dvp);
	if (error) {
		/*
		 * Note, we can't VOP_VFREE(tvp) here like we should
		 * because we can't write to the disk.  Instead, we leave
		 * the vnode dangling from the journal.
		 */
		vput(tvp);
		vput(dvp);
		return (error);
	}
#if defined(QUOTA) || defined(QUOTA2)
	if ((error = chkiq(ip, 1, cnp->cn_cred, 0))) {
		UFS_VFREE(tvp, ip->i_number, mode);
		UFS_WAPBL_END1(dvp->v_mount, dvp);
		vput(tvp);
		vput(dvp);
		return (error);
	}
#endif
	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
	ip->i_mode = mode;
	DIP_ASSIGN(ip, mode, mode);
	tvp->v_type = IFTOVT(mode);	/* Rest init'd in getnewvnode(). */
	ip->i_nlink = 1;
	DIP_ASSIGN(ip, nlink, 1);
	if ((ip->i_mode & ISGID) && (kauth_cred_ismember_gid(cnp->cn_cred,
	    ip->i_gid, &ismember) != 0 || !ismember) &&
	    kauth_authorize_generic(cnp->cn_cred, KAUTH_GENERIC_ISSUSER, NULL)) {
		ip->i_mode &= ~ISGID;
		DIP_ASSIGN(ip, mode, ip->i_mode);
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
	vput(dvp);
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
	/* If IN_ADIROP, account for it */
	UFS_UNMARK_VNODE(tvp);
	UFS_WAPBL_UPDATE(tvp, NULL, NULL, 0);
	tvp->v_type = VNON;		/* explodes later if VBLK */
	UFS_WAPBL_END1(dvp->v_mount, dvp);
	vput(tvp);
	vput(dvp);
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
