/*	$NetBSD: ulfs_vnops.c,v 1.34 2015/09/21 01:24:23 dholland Exp $	*/
/*  from NetBSD: ufs_vnops.c,v 1.213 2013/06/08 05:47:02 kardel Exp  */

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
__KERNEL_RCSID(0, "$NetBSD: ulfs_vnops.c,v 1.34 2015/09/21 01:24:23 dholland Exp $");

#if defined(_KERNEL_OPT)
#include "opt_lfs.h"
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

#include <ufs/lfs/lfs_extern.h>
#include <ufs/lfs/lfs.h>
#include <ufs/lfs/lfs_accessors.h>

#include <ufs/lfs/ulfs_inode.h>
#include <ufs/lfs/ulfsmount.h>
#include <ufs/lfs/ulfs_bswap.h>
#include <ufs/lfs/ulfs_extern.h>
#ifdef LFS_DIRHASH
#include <ufs/lfs/ulfs_dirhash.h>
#endif

#include <uvm/uvm.h>

static int ulfs_chmod(struct vnode *, int, kauth_cred_t, struct lwp *);
static int ulfs_chown(struct vnode *, uid_t, gid_t, kauth_cred_t,
    struct lwp *);

/*
 * Open called.
 *
 * Nothing to do.
 */
/* ARGSUSED */
int
ulfs_open(void *v)
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

static int
ulfs_check_possible(struct vnode *vp, struct inode *ip, mode_t mode,
    kauth_cred_t cred)
{
#if defined(LFS_QUOTA) || defined(LFS_QUOTA2)
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
#if defined(LFS_QUOTA) || defined(LFS_QUOTA2)
			fstrans_start(vp->v_mount, FSTRANS_SHARED);
			error = lfs_chkdq(ip, 0, cred, 0);
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
ulfs_check_permitted(struct vnode *vp, struct inode *ip, mode_t mode,
    kauth_cred_t cred)
{

	return kauth_authorize_vnode(cred, KAUTH_ACCESS_ACTION(mode, vp->v_type,
	    ip->i_mode & ALLPERMS), vp, NULL, genfs_can_access(vp->v_type,
	    ip->i_mode & ALLPERMS, ip->i_uid, ip->i_gid, mode, cred));
}

int
ulfs_access(void *v)
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

	error = ulfs_check_possible(vp, ip, mode, ap->a_cred);
	if (error)
		return error;

	error = ulfs_check_permitted(vp, ip, mode, ap->a_cred);

	return error;
}

/*
 * Set attribute vnode op. called from several syscalls
 */
int
ulfs_setattr(void *v)
{
	struct vop_setattr_args /* {
		struct vnode	*a_vp;
		struct vattr	*a_vap;
		kauth_cred_t	a_cred;
	} */ *ap = v;
	struct vattr	*vap;
	struct vnode	*vp;
	struct inode	*ip;
	struct lfs	*fs;
	kauth_cred_t	cred;
	struct lwp	*l;
	int		error;
	kauth_action_t	action;
	bool		changing_sysflags;

	vap = ap->a_vap;
	vp = ap->a_vp;
	ip = VTOI(vp);
	fs = ip->i_lfs;
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

		if ((vap->va_flags & SF_SETTABLE) != (ip->i_flags & SF_SETTABLE)) {
			action |= KAUTH_VNODE_WRITE_SYSFLAGS;
			changing_sysflags = true;
		}

		error = kauth_authorize_vnode(cred, action, vp, NULL,
		    genfs_can_chflags(cred, vp->v_type, ip->i_uid,
		    changing_sysflags));
		if (error)
			goto out;

		if (changing_sysflags) {
			ip->i_flags = vap->va_flags;
			DIP_ASSIGN(ip, flags, ip->i_flags);
		} else {
			ip->i_flags &= SF_SETTABLE;
			ip->i_flags |= (vap->va_flags & UF_SETTABLE);
			DIP_ASSIGN(ip, flags, ip->i_flags);
		}
		ip->i_flag |= IN_CHANGE;
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
		error = ulfs_chown(vp, vap->va_uid, vap->va_gid, cred, l);
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
			error = lfs_truncate(vp, vap->va_size, 0, cred);
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
		if (vap->va_atime.tv_sec != VNOVAL)
			if (!(vp->v_mount->mnt_flag & MNT_NOATIME))
				ip->i_flag |= IN_ACCESS;
		if (vap->va_mtime.tv_sec != VNOVAL) {
			ip->i_flag |= IN_CHANGE | IN_UPDATE;
			if (vp->v_mount->mnt_flag & MNT_RELATIME)
				ip->i_flag |= IN_ACCESS;
		}
		if (vap->va_birthtime.tv_sec != VNOVAL) {
			lfs_dino_setbirthtime(fs, ip->i_din,
					      &vap->va_birthtime);
		}
		error = lfs_update(vp, &vap->va_atime, &vap->va_mtime, 0);
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
		error = ulfs_chmod(vp, (int)vap->va_mode, cred, l);
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
ulfs_chmod(struct vnode *vp, int mode, kauth_cred_t cred, struct lwp *l)
{
	struct inode	*ip;
	int		error;

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
	fstrans_done(vp->v_mount);
	return (0);
}

/*
 * Perform chown operation on inode ip;
 * inode must be locked prior to call.
 */
static int
ulfs_chown(struct vnode *vp, uid_t uid, gid_t gid, kauth_cred_t cred,
    	struct lwp *l)
{
	struct inode	*ip;
	int		error = 0;
#if defined(LFS_QUOTA) || defined(LFS_QUOTA2)
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
#if defined(LFS_QUOTA) || defined(LFS_QUOTA2)
	ogid = ip->i_gid;
	ouid = ip->i_uid;
	change = DIP(ip, blocks);
	(void) lfs_chkdq(ip, -change, cred, 0);
	(void) lfs_chkiq(ip, -1, cred, 0);
#endif
	ip->i_gid = gid;
	DIP_ASSIGN(ip, gid, gid);
	ip->i_uid = uid;
	DIP_ASSIGN(ip, uid, uid);
#if defined(LFS_QUOTA) || defined(LFS_QUOTA2)
	if ((error = lfs_chkdq(ip, change, cred, 0)) == 0) {
		if ((error = lfs_chkiq(ip, 1, cred, 0)) == 0)
			goto good;
		else
			(void) lfs_chkdq(ip, -change, cred, FORCE);
	}
	ip->i_gid = ogid;
	DIP_ASSIGN(ip, gid, ogid);
	ip->i_uid = ouid;
	DIP_ASSIGN(ip, uid, ouid);
	(void) lfs_chkdq(ip, change, cred, FORCE);
	(void) lfs_chkiq(ip, 1, cred, FORCE);
	fstrans_done(vp->v_mount);
	return (error);
 good:
#endif /* LFS_QUOTA || LFS_QUOTA2 */
	ip->i_flag |= IN_CHANGE;
	fstrans_done(vp->v_mount);
	return (0);
}

int
ulfs_remove(void *v)
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
	struct ulfs_lookup_results *ulr;

	vp = ap->a_vp;
	dvp = ap->a_dvp;
	ip = VTOI(vp);
	mp = dvp->v_mount;
	KASSERT(mp == vp->v_mount); /* XXX Not stable without lock.  */

	/* XXX should handle this material another way */
	ulr = &VTOI(dvp)->i_crap;
	ULFS_CHECK_CRAPCOUNTER(VTOI(dvp));

	fstrans_start(mp, FSTRANS_SHARED);
	if (vp->v_type == VDIR || (ip->i_flags & (IMMUTABLE | APPEND)) ||
	    (VTOI(dvp)->i_flags & APPEND))
		error = EPERM;
	else {
		error = ulfs_dirremove(dvp, ulr,
				      ip, ap->a_cnp->cn_flags, 0);
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
 * ulfs_link: create hard link.
 */
int
ulfs_link(void *v)
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
	int error;
	struct ulfs_lookup_results *ulr;

	KASSERT(dvp != vp);
	KASSERT(vp->v_type != VDIR);
	KASSERT(mp == vp->v_mount); /* XXX Not stable without lock.  */

	/* XXX should handle this material another way */
	ulr = &VTOI(dvp)->i_crap;
	ULFS_CHECK_CRAPCOUNTER(VTOI(dvp));

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
	ip->i_nlink++;
	DIP_ASSIGN(ip, nlink, ip->i_nlink);
	ip->i_flag |= IN_CHANGE;
	error = lfs_update(vp, NULL, NULL, UPDATE_DIROP);
	if (!error) {
		error = ulfs_direnter(dvp, ulr, vp,
				      cnp, ip->i_number, LFS_IFTODT(ip->i_mode), NULL);
	}
	if (error) {
		ip->i_nlink--;
		DIP_ASSIGN(ip, nlink, ip->i_nlink);
		ip->i_flag |= IN_CHANGE;
	}
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
ulfs_whiteout(void *v)
{
	struct vop_whiteout_args /* {
		struct vnode		*a_dvp;
		struct componentname	*a_cnp;
		int			a_flags;
	} */ *ap = v;
	struct vnode		*dvp = ap->a_dvp;
	struct componentname	*cnp = ap->a_cnp;
	int			error;
	struct ulfsmount	*ump = VFSTOULFS(dvp->v_mount);
	struct lfs *fs = ump->um_lfs;
	struct ulfs_lookup_results *ulr;

	/* XXX should handle this material another way */
	ulr = &VTOI(dvp)->i_crap;
	ULFS_CHECK_CRAPCOUNTER(VTOI(dvp));

	error = 0;
	switch (ap->a_flags) {
	case LOOKUP:
		/* 4.4 format directories support whiteout operations */
		if (fs->um_maxsymlinklen > 0)
			return (0);
		return (EOPNOTSUPP);

	case CREATE:
		/* create a new directory whiteout */
		fstrans_start(dvp->v_mount, FSTRANS_SHARED);
#ifdef DIAGNOSTIC
		if (fs->um_maxsymlinklen <= 0)
			panic("ulfs_whiteout: old format filesystem");
#endif

		error = ulfs_direnter(dvp, ulr, NULL,
				      cnp, ULFS_WINO, LFS_DT_WHT,  NULL);
		break;

	case DELETE:
		/* remove an existing directory whiteout */
		fstrans_start(dvp->v_mount, FSTRANS_SHARED);
#ifdef DIAGNOSTIC
		if (fs->um_maxsymlinklen <= 0)
			panic("ulfs_whiteout: old format filesystem");
#endif

		cnp->cn_flags &= ~DOWHITEOUT;
		error = ulfs_dirremove(dvp, ulr, NULL, cnp->cn_flags, 0);
		break;
	default:
		panic("ulfs_whiteout: unknown op");
		/* NOTREACHED */
	}
	fstrans_done(dvp->v_mount);
	return (error);
}

int
ulfs_rmdir(void *v)
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
	struct ulfs_lookup_results *ulr;

	vp = ap->a_vp;
	dvp = ap->a_dvp;
	cnp = ap->a_cnp;
	ip = VTOI(vp);
	dp = VTOI(dvp);

	/* XXX should handle this material another way */
	ulr = &dp->i_crap;
	ULFS_CHECK_CRAPCOUNTER(dp);

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
	    !ulfs_dirempty(ip, dp->i_number, cnp->cn_cred)) {
		error = ENOTEMPTY;
		goto out;
	}
	if ((dp->i_flags & APPEND) ||
		(ip->i_flags & (IMMUTABLE | APPEND))) {
		error = EPERM;
		goto out;
	}
	/*
	 * Delete reference to directory before purging
	 * inode.  If we crash in between, the directory
	 * will be reattached to lost+found,
	 */
	error = ulfs_dirremove(dvp, ulr, ip, cnp->cn_flags, 1);
	if (error) {
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
	ip->i_nlink--;
	DIP_ASSIGN(ip, nlink, ip->i_nlink);
	ip->i_flag |= IN_CHANGE;
	error = lfs_truncate(vp, (off_t)0, IO_SYNC, cnp->cn_cred);
	cache_purge(vp);
#ifdef LFS_DIRHASH
	if (ip->i_dirhash != NULL)
		ulfsdirhash_free(ip);
#endif
 out:
	VN_KNOTE(vp, NOTE_DELETE);
	vput(vp);
	fstrans_done(dvp->v_mount);
	vput(dvp);
	return (error);
}

/*
 * Vnode op for reading directories.
 *
 * This routine handles converting from the on-disk directory format
 * "struct lfs_direct" to the in-memory format "struct dirent" as well as
 * byte swapping the entries if necessary.
 */
int
ulfs_readdir(void *v)
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
	LFS_DIRHEADER	*cdp, *ecdp;
	struct dirent	*ndp;
	char		*cdbuf, *ndbuf, *endp;
	struct uio	auio, *uio;
	struct iovec	aiov;
	int		error;
	size_t		count, ccount, rcount, cdbufsz, ndbufsz;
	off_t		off, *ccp;
	off_t		startoff;
	size_t		skipbytes;
	struct ulfsmount *ump = VFSTOULFS(vp->v_mount);
	struct lfs *fs = ump->um_lfs;
	uio = ap->a_uio;
	count = uio->uio_resid;
	rcount = count - ((uio->uio_offset + count) & (fs->um_dirblksiz - 1));

	if (rcount < LFS_DIRECTSIZ(fs, 0) || count < _DIRENT_MINSIZE(ndp))
		return EINVAL;

	startoff = uio->uio_offset & ~(fs->um_dirblksiz - 1);
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
	error = VOP_READ(vp, &auio, 0, ap->a_cred);
	if (error != 0) {
		kmem_free(cdbuf, cdbufsz);
		return error;
	}

	rcount -= auio.uio_resid;

	cdp = (LFS_DIRHEADER *)(void *)cdbuf;
	ecdp = (LFS_DIRHEADER *)(void *)&cdbuf[rcount];

	ndbufsz = count;
	ndbuf = kmem_alloc(ndbufsz, KM_SLEEP);
	ndp = (struct dirent *)(void *)ndbuf;
	endp = &ndbuf[count];

	off = uio->uio_offset;
	if (ap->a_cookies) {
		ccount = rcount / _DIRENT_RECLEN(ndp, 1);
		ccp = *(ap->a_cookies) = malloc(ccount * sizeof(*ccp),
		    M_TEMP, M_WAITOK);
	} else {
		/* XXX: GCC */
		ccount = 0;
		ccp = NULL;
	}

	while (cdp < ecdp) {
		if (skipbytes > 0) {
			if (lfs_dir_getreclen(fs, cdp) <= skipbytes) {
				skipbytes -= lfs_dir_getreclen(fs, cdp);
				cdp = LFS_NEXTDIR(fs, cdp);
				continue;
			}
			/*
			 * invalid cookie.
			 */
			error = EINVAL;
			goto out;
		}
		if (lfs_dir_getreclen(fs, cdp) == 0) {
			struct dirent *ondp = ndp;
			ndp->d_reclen = _DIRENT_MINSIZE(ndp);
			ndp = _DIRENT_NEXT(ndp);
			ondp->d_reclen = 0;
			cdp = ecdp;
			break;
		}
		ndp->d_type = lfs_dir_gettype(fs, cdp);
		ndp->d_namlen = lfs_dir_getnamlen(fs, cdp);
		ndp->d_reclen = _DIRENT_RECLEN(ndp, ndp->d_namlen);
		if ((char *)(void *)ndp + ndp->d_reclen +
		    _DIRENT_MINSIZE(ndp) > endp)
			break;
		ndp->d_fileno = lfs_dir_getino(fs, cdp);
		(void)memcpy(ndp->d_name, lfs_dir_nameptr(fs, cdp),
			     ndp->d_namlen);
		memset(&ndp->d_name[ndp->d_namlen], 0,
		    ndp->d_reclen - _DIRENT_NAMEOFF(ndp) - ndp->d_namlen);
		off += lfs_dir_getreclen(fs, cdp);
		if (ap->a_cookies) {
			KASSERT(ccp - *(ap->a_cookies) < ccount);
			*(ccp++) = off;
		}
		ndp = _DIRENT_NEXT(ndp);
		cdp = LFS_NEXTDIR(fs, cdp);
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
ulfs_readlink(void *v)
{
	struct vop_readlink_args /* {
		struct vnode	*a_vp;
		struct uio	*a_uio;
		kauth_cred_t	a_cred;
	} */ *ap = v;
	struct vnode	*vp = ap->a_vp;
	struct inode	*ip = VTOI(vp);
	struct ulfsmount *ump = VFSTOULFS(vp->v_mount);
	struct lfs *fs = ump->um_lfs;
	int		isize;

	isize = ip->i_size;
	if (isize < fs->um_maxsymlinklen ||
	    (fs->um_maxsymlinklen == 0 && DIP(ip, blocks) == 0)) {
		uiomove((char *)SHORTLINK(ip), isize, ap->a_uio);
		return (0);
	}
	return (lfs_bufrd(vp, ap->a_uio, 0, ap->a_cred));
}

/*
 * Print out the contents of an inode.
 */
int
ulfs_print(void *v)
{
	struct vop_print_args /* {
		struct vnode	*a_vp;
	} */ *ap = v;
	struct vnode	*vp;
	struct inode	*ip;

	vp = ap->a_vp;
	ip = VTOI(vp);
	printf("tag VT_ULFS, ino %llu, on dev %llu, %llu",
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
ulfsspec_read(void *v)
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
ulfsspec_write(void *v)
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
 * Read wrapper for fifo's
 */
int
ulfsfifo_read(void *v)
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
ulfsfifo_write(void *v)
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
 * Return POSIX pathconf information applicable to ulfs filesystems.
 */
int
ulfs_pathconf(void *v)
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
		*ap->a_retval = LFS_MAXNAMLEN;
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
ulfs_advlock(void *v)
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
ulfs_vinit(struct mount *mntp, int (**specops)(void *), int (**fifoops)(void *),
	struct vnode **vpp)
{
	struct timeval	tv;
	struct inode	*ip;
	struct vnode	*vp;
	dev_t		rdev;
	struct ulfsmount *ump;

	vp = *vpp;
	ip = VTOI(vp);
	switch(vp->v_type = IFTOVT(ip->i_mode)) {
	case VCHR:
	case VBLK:
		vp->v_op = specops;
		ump = ip->i_ump;
		// XXX clean this up
		if (ump->um_fstype == ULFS1)
			rdev = (dev_t)ulfs_rw32(ip->i_din->u_32.di_rdev,
			    ULFS_MPNEEDSWAP(ump->um_lfs));
		else
			rdev = (dev_t)ulfs_rw64(ip->i_din->u_64.di_rdev,
			    ULFS_MPNEEDSWAP(ump->um_lfs));
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
	if (ip->i_number == ULFS_ROOTINO)
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
ulfs_makeinode(struct vattr *vap, struct vnode *dvp,
	const struct ulfs_lookup_results *ulr,
	struct vnode **vpp, struct componentname *cnp)
{
	struct inode	*ip;
	struct vnode	*tvp;
	int		error;

	error = vcache_new(dvp->v_mount, dvp, vap, cnp->cn_cred, &tvp);
	if (error)
		return error;
	error = vn_lock(tvp, LK_EXCLUSIVE);
	if (error) {
		vrele(tvp);
		return error;
	}
	lfs_mark_vnode(tvp);
	*vpp = tvp;
	ip = VTOI(tvp);
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
	if ((error = lfs_update(tvp, NULL, NULL, UPDATE_DIROP)) != 0)
		goto bad;
	error = ulfs_direnter(dvp, ulr, tvp,
			      cnp, ip->i_number, LFS_IFTODT(ip->i_mode), NULL);
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
	/* If IN_ADIROP, account for it */
	lfs_unmark_vnode(tvp);
	vput(tvp);
	return (error);
}

/*
 * Allocate len bytes at offset off.
 */
int
ulfs_gop_alloc(struct vnode *vp, off_t off, off_t len, int flags,
    kauth_cred_t cred)
{
        struct inode *ip = VTOI(vp);
        int error, delta, bshift, bsize;
        UVMHIST_FUNC("ulfs_gop_alloc"); UVMHIST_CALLED(ubchist);

        error = 0;
        bshift = vp->v_mount->mnt_fs_bshift;
        bsize = 1 << bshift;

        delta = off & (bsize - 1);
        off -= delta;
        len += delta;

        while (len > 0) {
                bsize = MIN(bsize, len);

                error = lfs_balloc(vp, off, bsize, cred, flags, NULL);
                if (error) {
                        goto out;
                }

                /*
                 * increase file size now, lfs_balloc() requires that
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
	return error;
}

void
ulfs_gop_markupdate(struct vnode *vp, int flags)
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
ulfs_bufio(enum uio_rw rw, struct vnode *vp, void *buf, size_t len, off_t off,
    int ioflg, kauth_cred_t cred, size_t *aresid, struct lwp *l)
{
	struct iovec iov;
	struct uio uio;
	int error;

	KASSERT(ISSET(ioflg, IO_NODELOCKED));
	KASSERT(VOP_ISLOCKED(vp));
	KASSERT(rw != UIO_WRITE || VOP_ISLOCKED(vp) == LK_EXCLUSIVE);

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
		error = lfs_bufrd(vp, &uio, ioflg, cred);
		break;
	case UIO_WRITE:
		error = lfs_bufwr(vp, &uio, ioflg, cred);
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
