/*	$NetBSD: ulfs_vnops.c,v 1.18 2013/07/28 01:10:49 dholland Exp $	*/
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
__KERNEL_RCSID(0, "$NetBSD: ulfs_vnops.c,v 1.18 2013/07/28 01:10:49 dholland Exp $");

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

#include <ufs/lfs/ulfs_inode.h>
#include <ufs/lfs/ulfsmount.h>
#include <ufs/lfs/ulfs_bswap.h>
#include <ufs/lfs/ulfs_extern.h>
#ifdef LFS_DIRHASH
#include <ufs/lfs/ulfs_dirhash.h>
#endif
#include <ufs/lfs/lfs_extern.h>
#include <ufs/lfs/lfs.h>

#include <uvm/uvm.h>

static int ulfs_chmod(struct vnode *, int, kauth_cred_t, struct lwp *);
static int ulfs_chown(struct vnode *, uid_t, gid_t, kauth_cred_t,
    struct lwp *);

/*
 * A virgin directory (no blushing please).
 */
static const struct lfs_dirtemplate mastertemplate = {
	0,	12,			LFS_DT_DIR,	1,	".",
	0,	LFS_DIRBLKSIZ - 12,	LFS_DT_DIR,	2,	".."
};

/*
 * Create a regular file
 */
int
ulfs_create(void *v)
{
	struct vop_create_args /* {
		struct vnode		*a_dvp;
		struct vnode		**a_vpp;
		struct componentname	*a_cnp;
		struct vattr		*a_vap;
	} */ *ap = v;
	int	error;
	struct vnode *dvp = ap->a_dvp;
	struct ulfs_lookup_results *ulr;

	/* XXX should handle this material another way */
	ulr = &VTOI(dvp)->i_crap;
	ULFS_CHECK_CRAPCOUNTER(VTOI(dvp));

	fstrans_start(dvp->v_mount, FSTRANS_SHARED);
	error =
	    ulfs_makeinode(MAKEIMODE(ap->a_vap->va_type, ap->a_vap->va_mode),
			  dvp, ulr, ap->a_vpp, ap->a_cnp);
	if (error) {
		fstrans_done(dvp->v_mount);
		return (error);
	}
	fstrans_done(dvp->v_mount);
	VN_KNOTE(dvp, NOTE_WRITE);
	return (0);
}

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
		if (vap->va_birthtime.tv_sec != VNOVAL &&
		    ip->i_ump->um_fstype == ULFS2) {
			ip->i_ffs2_birthtime = vap->va_birthtime.tv_sec;
			ip->i_ffs2_birthnsec = vap->va_birthtime.tv_nsec;
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
	int		error;
	struct ulfs_lookup_results *ulr;

	vp = ap->a_vp;
	dvp = ap->a_dvp;
	ip = VTOI(vp);

	/* XXX should handle this material another way */
	ulr = &VTOI(dvp)->i_crap;
	ULFS_CHECK_CRAPCOUNTER(VTOI(dvp));

	fstrans_start(dvp->v_mount, FSTRANS_SHARED);
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
	fstrans_done(dvp->v_mount);
	return (error);
}

/*
 * ulfs_link: create hard link.
 */
int
ulfs_link(void *v)
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
	struct lfs_direct *newdir;
	int error;
	struct ulfs_lookup_results *ulr;

	KASSERT(dvp != vp);
	KASSERT(vp->v_type != VDIR);
	KASSERT(dvp->v_mount == vp->v_mount);

	/* XXX should handle this material another way */
	ulr = &VTOI(dvp)->i_crap;
	ULFS_CHECK_CRAPCOUNTER(VTOI(dvp));

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
	ip->i_nlink++;
	DIP_ASSIGN(ip, nlink, ip->i_nlink);
	ip->i_flag |= IN_CHANGE;
	error = lfs_update(vp, NULL, NULL, UPDATE_DIROP);
	if (!error) {
		newdir = pool_cache_get(ulfs_direct_cache, PR_WAITOK);
		ulfs_makedirentry(ip, cnp, newdir);
		error = ulfs_direnter(dvp, ulr, vp, newdir, cnp, NULL);
		pool_cache_put(ulfs_direct_cache, newdir);
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
	vput(dvp);
	fstrans_done(dvp->v_mount);
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
	struct lfs_direct		*newdir;
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

		newdir = pool_cache_get(ulfs_direct_cache, PR_WAITOK);
		newdir->d_ino = ULFS_WINO;
		newdir->d_namlen = cnp->cn_namelen;
		memcpy(newdir->d_name, cnp->cn_nameptr,
		    (size_t)cnp->cn_namelen);
		newdir->d_name[cnp->cn_namelen] = '\0';
		newdir->d_type = LFS_DT_WHT;
		error = ulfs_direnter(dvp, ulr, NULL, newdir, cnp, NULL);
		pool_cache_put(ulfs_direct_cache, newdir);
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
ulfs_mkdir(void *v)
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
	struct lfs_dirtemplate	dirtemplate;
	struct lfs_direct		*newdir;
	int			error, dmode;
	struct ulfsmount	*ump = dp->i_ump;
	struct lfs *fs = ump->um_lfs;
	int dirblksiz = fs->um_dirblksiz;
	struct ulfs_lookup_results *ulr;

	fstrans_start(dvp->v_mount, FSTRANS_SHARED);

	/* XXX should handle this material another way */
	ulr = &dp->i_crap;
	ULFS_CHECK_CRAPCOUNTER(dp);

	if ((nlink_t)dp->i_nlink >= LINK_MAX) {
		error = EMLINK;
		goto out;
	}
	dmode = vap->va_mode & ACCESSPERMS;
	dmode |= LFS_IFDIR;
	/*
	 * Must simulate part of ulfs_makeinode here to acquire the inode,
	 * but not have it entered in the parent directory. The entry is
	 * made later after writing "." and ".." entries.
	 */
	if ((error = lfs_valloc(dvp, dmode, cnp->cn_cred, ap->a_vpp)) != 0)
		goto out;

	tvp = *ap->a_vpp;
	ip = VTOI(tvp);

	ip->i_uid = kauth_cred_geteuid(cnp->cn_cred);
	DIP_ASSIGN(ip, uid, ip->i_uid);
	ip->i_gid = dp->i_gid;
	DIP_ASSIGN(ip, gid, ip->i_gid);
#if defined(LFS_QUOTA) || defined(LFS_QUOTA2)
	if ((error = lfs_chkiq(ip, 1, cnp->cn_cred, 0))) {
		lfs_vfree(tvp, ip->i_number, dmode);
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
	if ((error = lfs_update(dvp, NULL, NULL, UPDATE_DIROP)) != 0)
		goto bad;

	/*
	 * Initialize directory with "." and ".." from static template.
	 */
	dirtemplate = mastertemplate;
	dirtemplate.dotdot_reclen = dirblksiz - dirtemplate.dot_reclen;
	dirtemplate.dot_ino = ulfs_rw32(ip->i_number, ULFS_MPNEEDSWAP(fs));
	dirtemplate.dotdot_ino = ulfs_rw32(dp->i_number, ULFS_MPNEEDSWAP(fs));
	dirtemplate.dot_reclen = ulfs_rw16(dirtemplate.dot_reclen,
	    ULFS_MPNEEDSWAP(fs));
	dirtemplate.dotdot_reclen = ulfs_rw16(dirtemplate.dotdot_reclen,
	    ULFS_MPNEEDSWAP(fs));
	if (fs->um_maxsymlinklen <= 0) {
#if BYTE_ORDER == LITTLE_ENDIAN
		if (ULFS_MPNEEDSWAP(fs) == 0)
#else
		if (ULFS_MPNEEDSWAP(fs) != 0)
#endif
		{
			dirtemplate.dot_type = dirtemplate.dot_namlen;
			dirtemplate.dotdot_type = dirtemplate.dotdot_namlen;
			dirtemplate.dot_namlen = dirtemplate.dotdot_namlen = 0;
		} else
			dirtemplate.dot_type = dirtemplate.dotdot_type = 0;
	}
	if ((error = lfs_balloc(tvp, (off_t)0, dirblksiz, cnp->cn_cred,
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
	if ((error = lfs_update(tvp, NULL, NULL, UPDATE_DIROP)) != 0) {
		goto bad;
	}
	newdir = pool_cache_get(ulfs_direct_cache, PR_WAITOK);
	ulfs_makedirentry(ip, cnp, newdir);
	error = ulfs_direnter(dvp, ulr, tvp, newdir, cnp, bp);
	pool_cache_put(ulfs_direct_cache, newdir);
 bad:
	if (error == 0) {
		VN_KNOTE(dvp, NOTE_WRITE | NOTE_LINK);
	} else {
		dp->i_nlink--;
		DIP_ASSIGN(dp, nlink, dp->i_nlink);
		dp->i_flag |= IN_CHANGE;
		/*
		 * No need to do an explicit lfs_truncate here, vrele will
		 * do this for us because we set the link count to 0.
		 */
		ip->i_nlink = 0;
		DIP_ASSIGN(ip, nlink, 0);
		ip->i_flag |= IN_CHANGE;
		/* If IN_ADIROP, account for it */
		lfs_unmark_vnode(tvp);
		vput(tvp);
	}
 out:
	fstrans_done(dvp->v_mount);
	vput(dvp);
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
 * symlink -- make a symbolic link
 */
int
ulfs_symlink(void *v)
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
	struct ulfs_lookup_results *ulr;

	vpp = ap->a_vpp;

	/* XXX should handle this material another way */
	ulr = &VTOI(ap->a_dvp)->i_crap;
	ULFS_CHECK_CRAPCOUNTER(VTOI(ap->a_dvp));

	fstrans_start(ap->a_dvp->v_mount, FSTRANS_SHARED);
	error = ulfs_makeinode(LFS_IFLNK | ap->a_vap->va_mode, ap->a_dvp, ulr,
			      vpp, ap->a_cnp);
	if (error)
		goto out;
	VN_KNOTE(ap->a_dvp, NOTE_WRITE);
	vp = *vpp;
	len = strlen(ap->a_target);
	ip = VTOI(vp);
	if (len < ip->i_lfs->um_maxsymlinklen) {
		memcpy((char *)SHORTLINK(ip), ap->a_target, len);
		ip->i_size = len;
		DIP_ASSIGN(ip, size, len);
		uvm_vnp_setsize(vp, ip->i_size);
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		if (vp->v_mount->mnt_flag & MNT_RELATIME)
			ip->i_flag |= IN_ACCESS;
	} else
		error = vn_rdwr(UIO_WRITE, vp, ap->a_target, len, (off_t)0,
		    UIO_SYSSPACE, IO_NODELOCKED | IO_JOURNALLOCKED,
		    ap->a_cnp->cn_cred, NULL, NULL);
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
	struct lfs_direct	*cdp, *ecdp;
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
	int nswap = ULFS_MPNEEDSWAP(fs);
#if BYTE_ORDER == LITTLE_ENDIAN
	int needswap = fs->um_maxsymlinklen <= 0 && nswap == 0;
#else
	int needswap = fs->um_maxsymlinklen <= 0 && nswap != 0;
#endif
	uio = ap->a_uio;
	count = uio->uio_resid;
	rcount = count - ((uio->uio_offset + count) & (fs->um_dirblksiz - 1));

	if (rcount < _DIRENT_MINSIZE(cdp) || count < _DIRENT_MINSIZE(ndp))
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

	cdp = (struct lfs_direct *)(void *)cdbuf;
	ecdp = (struct lfs_direct *)(void *)&cdbuf[rcount];

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
		cdp->d_reclen = ulfs_rw16(cdp->d_reclen, nswap);
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
		ndp->d_fileno = ulfs_rw32(cdp->d_ino, nswap);
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
	return (VOP_READ(vp, ap->a_uio, 0, ap->a_cred));
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
		if (ump->um_fstype == ULFS1)
			rdev = (dev_t)ulfs_rw32(ip->i_ffs1_rdev,
			    ULFS_MPNEEDSWAP(ump->um_lfs));
		else
			rdev = (dev_t)ulfs_rw64(ip->i_ffs2_rdev,
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
ulfs_makeinode(int mode, struct vnode *dvp, const struct ulfs_lookup_results *ulr,
	struct vnode **vpp, struct componentname *cnp)
{
	struct inode	*ip, *pdir;
	struct lfs_direct	*newdir;
	struct vnode	*tvp;
	int		error;

	pdir = VTOI(dvp);

	if ((mode & LFS_IFMT) == 0)
		mode |= LFS_IFREG;

	if ((error = lfs_valloc(dvp, mode, cnp->cn_cred, vpp)) != 0) {
		vput(dvp);
		return (error);
	}
	tvp = *vpp;
	ip = VTOI(tvp);
	ip->i_gid = pdir->i_gid;
	DIP_ASSIGN(ip, gid, ip->i_gid);
	ip->i_uid = kauth_cred_geteuid(cnp->cn_cred);
	DIP_ASSIGN(ip, uid, ip->i_uid);
#if defined(LFS_QUOTA) || defined(LFS_QUOTA2)
	if ((error = lfs_chkiq(ip, 1, cnp->cn_cred, 0))) {
		lfs_vfree(tvp, ip->i_number, mode);
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

	/* Authorize setting SGID if needed. */
	if (ip->i_mode & ISGID) {
		error = kauth_authorize_vnode(cnp->cn_cred, KAUTH_VNODE_WRITE_SECURITY,
		    tvp, NULL, genfs_can_chmod(tvp->v_type, cnp->cn_cred, ip->i_uid,
		    ip->i_gid, mode));
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
	newdir = pool_cache_get(ulfs_direct_cache, PR_WAITOK);
	ulfs_makedirentry(ip, cnp, newdir);
	error = ulfs_direnter(dvp, ulr, tvp, newdir, cnp, NULL);
	pool_cache_put(ulfs_direct_cache, newdir);
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
	lfs_unmark_vnode(tvp);
	tvp->v_type = VNON;		/* explodes later if VBLK */
	vput(tvp);
	vput(dvp);
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
