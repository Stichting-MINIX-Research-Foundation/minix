/*	$NetBSD: ext2fs_vnops.c,v 1.101 2011/11/18 21:18:51 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)ufs_vnops.c	8.14 (Berkeley) 10/26/94
 * Modified for ext2fs by Manuel Bouyer.
 */

/*
 * Copyright (c) 1997 Manuel Bouyer.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	@(#)ufs_vnops.c	8.14 (Berkeley) 10/26/94
 * Modified for ext2fs by Manuel Bouyer.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ext2fs_vnops.c,v 1.101 2011/11/18 21:18:51 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/resourcevar.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/lockf.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/signalvar.h>
#include <sys/kauth.h>

#include <miscfs/fifofs/fifo.h>
#include <miscfs/genfs/genfs.h>
#include <miscfs/specfs/specdev.h>

#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufs_extern.h>
#include <ufs/ufs/ufsmount.h>

#include <ufs/ext2fs/ext2fs.h>
#include <ufs/ext2fs/ext2fs_extern.h>
#include <ufs/ext2fs/ext2fs_dir.h>

extern int prtactive;

static int ext2fs_chmod(struct vnode *, int, kauth_cred_t, struct lwp *);
static int ext2fs_chown(struct vnode *, uid_t, gid_t, kauth_cred_t,
				struct lwp *);

union _qcvt {
	int64_t	qcvt;
	int32_t val[2];
};

#define SETHIGH(q, h) { \
	union _qcvt tmp; \
	tmp.qcvt = (q); \
	tmp.val[_QUAD_HIGHWORD] = (h); \
	(q) = tmp.qcvt; \
}
#define SETLOW(q, l) { \
	union _qcvt tmp; \
	tmp.qcvt = (q); \
	tmp.val[_QUAD_LOWWORD] = (l); \
	(q) = tmp.qcvt; \
}

/*
 * Create a regular file
 */
int
ext2fs_create(void *v)
{
	struct vop_create_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	int	error;

	error =
	    ext2fs_makeinode(MAKEIMODE(ap->a_vap->va_type, ap->a_vap->va_mode),
			     ap->a_dvp, ap->a_vpp, ap->a_cnp);

	if (error)
		return (error);
	VN_KNOTE(ap->a_dvp, NOTE_WRITE);
	return (0);
}

/*
 * Mknod vnode call
 */
/* ARGSUSED */
int
ext2fs_mknod(void *v)
{
	struct vop_mknod_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	struct vattr *vap = ap->a_vap;
	struct vnode **vpp = ap->a_vpp;
	struct inode *ip;
	int error;
	struct mount	*mp;
	ino_t		ino;

	if ((error = ext2fs_makeinode(MAKEIMODE(vap->va_type, vap->va_mode),
		    ap->a_dvp, vpp, ap->a_cnp)) != 0)
		return (error);
	VN_KNOTE(ap->a_dvp, NOTE_WRITE);
	ip = VTOI(*vpp);
	mp  = (*vpp)->v_mount;
	ino = ip->i_number;
	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
	if (vap->va_rdev != VNOVAL) {
		/*
		 * Want to be able to use this to make badblock
		 * inodes, so don't truncate the dev number.
		 */
		ip->i_din.e2fs_din->e2di_rdev = h2fs32(vap->va_rdev);
	}
	/*
	 * Remove inode so that it will be reloaded by VFS_VGET and
	 * checked to see if it is an alias of an existing entry in
	 * the inode cache.
	 */
	VOP_UNLOCK(*vpp);
	(*vpp)->v_type = VNON;
	vgone(*vpp);
	error = VFS_VGET(mp, ino, vpp);
	if (error != 0) {
		*vpp = NULL;
		return (error);
	}
	return (0);
}

/*
 * Open called.
 *
 * Just check the APPEND flag.
 */
/* ARGSUSED */
int
ext2fs_open(void *v)
{
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		kauth_cred_t a_cred;
	} */ *ap = v;

	/*
	 * Files marked append-only must be opened for appending.
	 */
	if ((VTOI(ap->a_vp)->i_e2fs_flags & EXT2_APPEND) &&
		(ap->a_mode & (FWRITE | O_APPEND)) == FWRITE)
		return (EPERM);
	return (0);
}

static int
ext2fs_check_possible(struct vnode *vp, struct inode *ip, mode_t mode)
{

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
			break;
		default:
			break;
		}
	}

	/* If immutable bit set, nobody gets to write it. */
	if ((mode & VWRITE) && (ip->i_e2fs_flags & EXT2_IMMUTABLE))
		return (EPERM);

	return 0;
}

static int
ext2fs_check_permitted(struct vnode *vp, struct inode *ip, mode_t mode,
    kauth_cred_t cred)
{

	return genfs_can_access(vp->v_type, ip->i_e2fs_mode & ALLPERMS,
	    ip->i_uid, ip->i_gid, mode, cred);
}

int
ext2fs_access(void *v)
{
	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	mode_t mode = ap->a_mode;
	int error;

	error = ext2fs_check_possible(vp, ip, mode);
	if (error)
		return error;

	error = ext2fs_check_permitted(vp, ip, mode, ap->a_cred);

	return error;
}

/* ARGSUSED */
int
ext2fs_getattr(void *v)
{
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	struct vattr *vap = ap->a_vap;

	EXT2FS_ITIMES(ip, NULL, NULL, NULL);
	/*
	 * Copy from inode table
	 */
	vap->va_fsid = ip->i_dev;
	vap->va_fileid = ip->i_number;
	vap->va_mode = ip->i_e2fs_mode & ALLPERMS;
	vap->va_nlink = ip->i_e2fs_nlink;
	vap->va_uid = ip->i_uid;
	vap->va_gid = ip->i_gid;
	vap->va_rdev = (dev_t)fs2h32(ip->i_din.e2fs_din->e2di_rdev);
	vap->va_size = vp->v_size;
	vap->va_atime.tv_sec = ip->i_e2fs_atime;
	vap->va_atime.tv_nsec = 0;
	vap->va_mtime.tv_sec = ip->i_e2fs_mtime;
	vap->va_mtime.tv_nsec = 0;
	vap->va_ctime.tv_sec = ip->i_e2fs_ctime;
	vap->va_ctime.tv_nsec = 0;
#ifdef EXT2FS_SYSTEM_FLAGS
	vap->va_flags = (ip->i_e2fs_flags & EXT2_APPEND) ? SF_APPEND : 0;
	vap->va_flags |= (ip->i_e2fs_flags & EXT2_IMMUTABLE) ? SF_IMMUTABLE : 0;
#else
	vap->va_flags = (ip->i_e2fs_flags & EXT2_APPEND) ? UF_APPEND : 0;
	vap->va_flags |= (ip->i_e2fs_flags & EXT2_IMMUTABLE) ? UF_IMMUTABLE : 0;
#endif
	vap->va_gen = ip->i_e2fs_gen;
	/* this doesn't belong here */
	if (vp->v_type == VBLK)
		vap->va_blocksize = BLKDEV_IOSIZE;
	else if (vp->v_type == VCHR)
		vap->va_blocksize = MAXBSIZE;
	else
		vap->va_blocksize = vp->v_mount->mnt_stat.f_iosize;
	vap->va_bytes = dbtob((u_quad_t)ip->i_e2fs_nblock);
	vap->va_type = vp->v_type;
	vap->va_filerev = ip->i_modrev;
	return (0);
}

/*
 * Set attribute vnode op. called from several syscalls
 */
int
ext2fs_setattr(void *v)
{
	struct vop_setattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vattr *vap = ap->a_vap;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	kauth_cred_t cred = ap->a_cred;
	struct lwp *l = curlwp;
	int error;

	/*
	 * Check for unsettable attributes.
	 */
	if ((vap->va_type != VNON) || (vap->va_nlink != (nlink_t)VNOVAL) ||
	    (vap->va_fsid != VNOVAL) || (vap->va_fileid != VNOVAL) ||
	    (vap->va_blocksize != VNOVAL) || (vap->va_rdev != VNOVAL) ||
	    ((int)vap->va_bytes != VNOVAL) || (vap->va_gen != VNOVAL)) {
		return (EINVAL);
	}
	if (vap->va_flags != VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return (EROFS);
		if (kauth_cred_geteuid(cred) != ip->i_uid &&
		    (error = kauth_authorize_generic(cred, KAUTH_GENERIC_ISSUSER,
		    NULL)))
			return (error);
#ifdef EXT2FS_SYSTEM_FLAGS
		if (kauth_authorize_generic(cred, KAUTH_GENERIC_ISSUSER,
		    NULL) == 0) {
			if ((ip->i_e2fs_flags &
			    (EXT2_APPEND | EXT2_IMMUTABLE)) &&
			    kauth_authorize_system(l->l_cred,
			     KAUTH_SYSTEM_CHSYSFLAGS, 0, NULL, NULL, NULL))
				return (EPERM);
			ip->i_e2fs_flags &= ~(EXT2_APPEND | EXT2_IMMUTABLE);
			ip->i_e2fs_flags |=
			    (vap->va_flags & SF_APPEND) ?  EXT2_APPEND : 0 |
			    (vap->va_flags & SF_IMMUTABLE) ? EXT2_IMMUTABLE : 0;
		} else
			return (EPERM);
#else
		ip->i_e2fs_flags &= ~(EXT2_APPEND | EXT2_IMMUTABLE);
		ip->i_e2fs_flags |=
		    (vap->va_flags & UF_APPEND) ? EXT2_APPEND : 0 |
		    (vap->va_flags & UF_IMMUTABLE) ? EXT2_IMMUTABLE : 0;
#endif
		ip->i_flag |= IN_CHANGE;
		if (vap->va_flags & (IMMUTABLE | APPEND))
			return (0);
	}
	if (ip->i_e2fs_flags & (EXT2_APPEND | EXT2_IMMUTABLE))
		return (EPERM);
	/*
	 * Go through the fields and update iff not VNOVAL.
	 */
	if (vap->va_uid != (uid_t)VNOVAL || vap->va_gid != (gid_t)VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return (EROFS);
		error = ext2fs_chown(vp, vap->va_uid, vap->va_gid, cred, l);
		if (error)
			return (error);
	}
	if (vap->va_size != VNOVAL) {
		/*
		 * Disallow write attempts on read-only file systems;
		 * unless the file is a socket, fifo, or a block or
		 * character device resident on the file system.
		 */
		switch (vp->v_type) {
		case VDIR:
			return (EISDIR);
		case VLNK:
		case VREG:
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);
		default:
			break;
		}
		error = ext2fs_truncate(vp, vap->va_size, 0, cred);
		if (error)
			return (error);
	}
	ip = VTOI(vp);
	if (vap->va_atime.tv_sec != VNOVAL || vap->va_mtime.tv_sec != VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return (EROFS);
		error = genfs_can_chtimes(vp, vap->va_vaflags, ip->i_uid, cred);
		if (error)
			return (error);
		if (vap->va_atime.tv_sec != VNOVAL)
			if (!(vp->v_mount->mnt_flag & MNT_NOATIME))
				ip->i_flag |= IN_ACCESS;
		if (vap->va_mtime.tv_sec != VNOVAL) {
			ip->i_flag |= IN_CHANGE | IN_UPDATE;
			if (vp->v_mount->mnt_flag & MNT_RELATIME)
				ip->i_flag |= IN_ACCESS;
		}
		error = ext2fs_update(vp, &vap->va_atime, &vap->va_mtime,
			UPDATE_WAIT);
		if (error)
			return (error);
	}
	error = 0;
	if (vap->va_mode != (mode_t)VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return (EROFS);
		error = ext2fs_chmod(vp, (int)vap->va_mode, cred, l);
	}
	VN_KNOTE(vp, NOTE_ATTRIB);
	return (error);
}

/*
 * Change the mode on a file.
 * Inode must be locked before calling.
 */
static int
ext2fs_chmod(struct vnode *vp, int mode, kauth_cred_t cred, struct lwp *l)
{
	struct inode *ip = VTOI(vp);
	int error;

	error = genfs_can_chmod(vp, cred, ip->i_uid, ip->i_gid, mode);
	if (error)
		return (error);

	ip->i_e2fs_mode &= ~ALLPERMS;
	ip->i_e2fs_mode |= (mode & ALLPERMS);
	ip->i_flag |= IN_CHANGE;
	return (0);
}

/*
 * Perform chown operation on inode ip;
 * inode must be locked prior to call.
 */
static int
ext2fs_chown(struct vnode *vp, uid_t uid, gid_t gid, kauth_cred_t cred,
		struct lwp *l)
{
	struct inode *ip = VTOI(vp);
	uid_t ouid;
	gid_t ogid;
	int error;

	if (uid == (uid_t)VNOVAL)
		uid = ip->i_uid;
	if (gid == (gid_t)VNOVAL)
		gid = ip->i_gid;

	error = genfs_can_chown(vp, cred, ip->i_uid, ip->i_gid, uid, gid);
	if (error)
		return (error);

	ogid = ip->i_gid;
	ouid = ip->i_uid;

	ip->i_e2fs_gid = gid & 0xffff;
	ip->i_e2fs_uid = uid & 0xffff;
	if (ip->i_e2fs->e2fs.e2fs_rev > E2FS_REV0) {
		ip->i_e2fs_gid_high = (gid >> 16) & 0xffff;
		ip->i_e2fs_uid_high = (uid >> 16) & 0xffff;
	} else {
		ip->i_e2fs_gid_high = 0;
		ip->i_e2fs_uid_high = 0;
	}
	if (ouid != uid || ogid != gid) {
		ext2fs_set_inode_guid(ip);
		ip->i_flag |= IN_CHANGE;
	}
	if (ouid != uid && kauth_authorize_generic(cred,
	    KAUTH_GENERIC_ISSUSER, NULL) != 0)
		ip->i_e2fs_mode &= ~ISUID;
	if (ogid != gid && kauth_authorize_generic(cred,
	    KAUTH_GENERIC_ISSUSER, NULL) != 0)
		ip->i_e2fs_mode &= ~ISGID;
	return (0);
}

int
ext2fs_remove(void *v)
{
	struct vop_remove_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	struct inode *ip;
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct ufs_lookup_results *ulr;
	int error;

	/* XXX should handle this material another way */
	ulr = &VTOI(dvp)->i_crap;
	UFS_CHECK_CRAPCOUNTER(VTOI(dvp));

	ip = VTOI(vp);
	if (vp->v_type == VDIR ||
		(ip->i_e2fs_flags & (EXT2_IMMUTABLE | EXT2_APPEND)) ||
		(VTOI(dvp)->i_e2fs_flags & EXT2_APPEND)) {
		error = EPERM;
	} else {
		error = ext2fs_dirremove(dvp, ulr, ap->a_cnp);
		if (error == 0) {
			ip->i_e2fs_nlink--;
			ip->i_flag |= IN_CHANGE;
		}
	}

	VN_KNOTE(vp, NOTE_DELETE);
	VN_KNOTE(dvp, NOTE_WRITE);
	if (dvp == vp)
		vrele(vp);
	else
		vput(vp);
	vput(dvp);
	return (error);
}

/*
 * ext2fs_link: create hard link.
 */
int
ext2fs_link(void *v)
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
	int error;
	struct ufs_lookup_results *ulr;

	KASSERT(dvp != vp);
	KASSERT(vp->v_type != VDIR);
	KASSERT(dvp->v_mount == vp->v_mount);

	/* XXX should handle this material another way */
	ulr = &VTOI(dvp)->i_crap;
	UFS_CHECK_CRAPCOUNTER(VTOI(dvp));

	error = vn_lock(vp, LK_EXCLUSIVE);
	if (error) {
		VOP_ABORTOP(dvp, cnp);
		goto out2;
	}
	ip = VTOI(vp);
	if ((nlink_t)ip->i_e2fs_nlink >= LINK_MAX) {
		VOP_ABORTOP(dvp, cnp);
		error = EMLINK;
		goto out1;
	}
	if (ip->i_e2fs_flags & (EXT2_IMMUTABLE | EXT2_APPEND)) {
		VOP_ABORTOP(dvp, cnp);
		error = EPERM;
		goto out1;
	}
	ip->i_e2fs_nlink++;
	ip->i_flag |= IN_CHANGE;
	error = ext2fs_update(vp, NULL, NULL, UPDATE_WAIT);
	if (!error)
		error = ext2fs_direnter(ip, dvp, ulr, cnp);
	if (error) {
		ip->i_e2fs_nlink--;
		ip->i_flag |= IN_CHANGE;
	}
out1:
	VOP_UNLOCK(vp);
out2:
	VN_KNOTE(vp, NOTE_LINK);
	VN_KNOTE(dvp, NOTE_WRITE);
	vput(dvp);
	return (error);
}

/*
 * Rename system call.
 *	rename("foo", "bar");
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
ext2fs_rename(void *v)
{
	struct vop_rename_args  /* {
		struct vnode *a_fdvp;
		struct vnode *a_fvp;
		struct componentname *a_fcnp;
		struct vnode *a_tdvp;
		struct vnode *a_tvp;
		struct componentname *a_tcnp;
	} */ *ap = v;
	struct vnode *tvp = ap->a_tvp;
	struct vnode *tdvp = ap->a_tdvp;
	struct vnode *fvp = ap->a_fvp;
	struct vnode *fdvp = ap->a_fdvp;
	struct componentname *tcnp = ap->a_tcnp;
	struct componentname *fcnp = ap->a_fcnp;
	struct inode *ip, *xp, *dp;
	struct ext2fs_dirtemplate dirbuf;
	int doingdirectory = 0, oldparent = 0, newparent = 0;
	int error = 0;
	u_char namlen;

	/*
	 * Check for cross-device rename.
	 */
	if ((fvp->v_mount != tdvp->v_mount) ||
	    (tvp && (fvp->v_mount != tvp->v_mount))) {
		error = EXDEV;
abortit:
		VOP_ABORTOP(tdvp, tcnp); /* XXX, why not in NFS? */
		if (tdvp == tvp)
			vrele(tdvp);
		else
			vput(tdvp);
		if (tvp)
			vput(tvp);
		VOP_ABORTOP(fdvp, fcnp); /* XXX, why not in NFS? */
		vrele(fdvp);
		vrele(fvp);
		return (error);
	}

	/*
	 * Check if just deleting a link name.
	 */
	if (tvp && ((VTOI(tvp)->i_e2fs_flags & (EXT2_IMMUTABLE | EXT2_APPEND)) ||
	    (VTOI(tdvp)->i_e2fs_flags & EXT2_APPEND))) {
		error = EPERM;
		goto abortit;
	}
	if (fvp == tvp) {
		if (fvp->v_type == VDIR) {
			error = EINVAL;
			goto abortit;
		}

		/* Release destination completely. */
		VOP_ABORTOP(tdvp, tcnp);
		vput(tdvp);
		vput(tvp);

		/* Delete source. */
		vrele(fvp);
		fcnp->cn_flags &= ~(MODMASK);
		fcnp->cn_flags |= LOCKPARENT | LOCKLEAF;
		fcnp->cn_nameiop = DELETE;
		vn_lock(fdvp, LK_EXCLUSIVE | LK_RETRY);
		if ((error = relookup(fdvp, &fvp, fcnp, 0))) {
			vput(fdvp);
			return (error);
		}
		return (VOP_REMOVE(fdvp, fvp, fcnp));
	}
	if ((error = vn_lock(fvp, LK_EXCLUSIVE)) != 0)
		goto abortit;
	dp = VTOI(fdvp);
	ip = VTOI(fvp);
	if ((nlink_t) ip->i_e2fs_nlink >= LINK_MAX) {
		VOP_UNLOCK(fvp);
		error = EMLINK;
		goto abortit;
	}
	if ((ip->i_e2fs_flags & (EXT2_IMMUTABLE | EXT2_APPEND)) ||
		(dp->i_e2fs_flags & EXT2_APPEND)) {
		VOP_UNLOCK(fvp);
		error = EPERM;
		goto abortit;
	}
	if ((ip->i_e2fs_mode & IFMT) == IFDIR) {
		error = VOP_ACCESS(fvp, VWRITE, tcnp->cn_cred);
		if (!error && tvp)
			error = VOP_ACCESS(tvp, VWRITE, tcnp->cn_cred);
		if (error) {
			VOP_UNLOCK(fvp);
			error = EACCES;
			goto abortit;
		}
		/*
		 * Avoid ".", "..", and aliases of "." for obvious reasons.
		 */
		if ((fcnp->cn_namelen == 1 && fcnp->cn_nameptr[0] == '.') ||
		    dp == ip ||
		    (fcnp->cn_flags & ISDOTDOT) ||
		    (tcnp->cn_flags & ISDOTDOT) ||
		    (ip->i_flag & IN_RENAME)) {
			VOP_UNLOCK(fvp);
			error = EINVAL;
			goto abortit;
		}
		ip->i_flag |= IN_RENAME;
		oldparent = dp->i_number;
		doingdirectory = 1;
	}
	VN_KNOTE(fdvp, NOTE_WRITE);		/* XXXLUKEM/XXX: right place? */

	/*
	 * When the target exists, both the directory
	 * and target vnodes are returned locked.
	 */
	dp = VTOI(tdvp);
	xp = NULL;
	if (tvp)
		xp = VTOI(tvp);

	/*
	 * 1) Bump link count while we're moving stuff
	 *    around.  If we crash somewhere before
	 *    completing our work, the link count
	 *    may be wrong, but correctable.
	 */
	ip->i_e2fs_nlink++;
	ip->i_flag |= IN_CHANGE;
	if ((error = ext2fs_update(fvp, NULL, NULL, UPDATE_WAIT)) != 0) {
		VOP_UNLOCK(fvp);
		goto bad;
	}

	/*
	 * If ".." must be changed (ie the directory gets a new
	 * parent) then the source directory must not be in the
	 * directory hierarchy above the target, as this would
	 * orphan everything below the source directory. Also
	 * the user must have write permission in the source so
	 * as to be able to change "..". We must repeat the call
	 * to namei, as the parent directory is unlocked by the
	 * call to checkpath().
	 */
	error = VOP_ACCESS(fvp, VWRITE, tcnp->cn_cred);
	VOP_UNLOCK(fvp);
	if (oldparent != dp->i_number)
		newparent = dp->i_number;
	if (doingdirectory && newparent) {
		if (error)	/* write access check above */
			goto bad;
		if (xp != NULL)
			vput(tvp);
		vref(tdvp);     /* compensate for the ref checkpath loses */
		error = ext2fs_checkpath(ip, dp, tcnp->cn_cred);
		if (error != 0) {
			vrele(tdvp);
			goto out;
		}
		vn_lock(tdvp, LK_EXCLUSIVE | LK_RETRY);
		if ((error = relookup(tdvp, &tvp, tcnp, 0)) != 0) {
			vput(tdvp);
			goto out;
		}
		dp = VTOI(tdvp);
		xp = NULL;
		if (tvp)
			xp = VTOI(tvp);
	}
	/*
	 * 2) If target doesn't exist, link the target
	 *    to the source and unlink the source.
	 *    Otherwise, rewrite the target directory
	 *    entry to reference the source inode and
	 *    expunge the original entry's existence.
	 */
	if (xp == NULL) {
		if (dp->i_dev != ip->i_dev)
			panic("rename: EXDEV");
		/*
		 * Account for ".." in new directory.
		 * When source and destination have the same
		 * parent we don't fool with the link count.
		 */
		if (doingdirectory && newparent) {
			if ((nlink_t)dp->i_e2fs_nlink >= LINK_MAX) {
				error = EMLINK;
				goto bad;
			}
			dp->i_e2fs_nlink++;
			dp->i_flag |= IN_CHANGE;
			if ((error = ext2fs_update(tdvp, NULL, NULL,
			    UPDATE_WAIT)) != 0)
				goto bad;
		}
		error = ext2fs_direnter(ip, tdvp, &VTOI(tdvp)->i_crap, tcnp);
		if (error != 0) {
			if (doingdirectory && newparent) {
				dp->i_e2fs_nlink--;
				dp->i_flag |= IN_CHANGE;
				(void)ext2fs_update(tdvp, NULL, NULL,
				    UPDATE_WAIT);
			}
			goto bad;
		}
		VN_KNOTE(tdvp, NOTE_WRITE);
		vput(tdvp);
	} else {
		if (xp->i_dev != dp->i_dev || xp->i_dev != ip->i_dev)
			panic("rename: EXDEV");
		/*
		 * Short circuit rename(foo, foo).
		 */
		if (xp->i_number == ip->i_number)
			panic("rename: same file");
		/*
		 * If the parent directory is "sticky", then the user must
		 * own the parent directory, or the destination of the rename,
		 * otherwise the destination may not be changed (except by
		 * root). This implements append-only directories.
		 */
		if ((dp->i_e2fs_mode & S_ISTXT) &&
		    kauth_authorize_generic(tcnp->cn_cred,
		     KAUTH_GENERIC_ISSUSER, NULL) != 0 &&
		    kauth_cred_geteuid(tcnp->cn_cred) != dp->i_uid &&
		    xp->i_uid != kauth_cred_geteuid(tcnp->cn_cred)) {
			error = EPERM;
			goto bad;
		}
		/*
		 * Target must be empty if a directory and have no links
		 * to it. Also, ensure source and target are compatible
		 * (both directories, or both not directories).
		 */
		if ((xp->i_e2fs_mode & IFMT) == IFDIR) {
			if (!ext2fs_dirempty(xp, dp->i_number, tcnp->cn_cred) ||
				xp->i_e2fs_nlink > 2) {
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
		error = ext2fs_dirrewrite(dp, &dp->i_crap, ip, tcnp);
		if (error != 0)
			goto bad;
		/*
		 * If the target directory is in the same
		 * directory as the source directory,
		 * decrement the link count on the parent
		 * of the target directory.
		 */
		 if (doingdirectory && !newparent) {
			dp->i_e2fs_nlink--;
			dp->i_flag |= IN_CHANGE;
		}
		/*
		 * Adjust the link count of the target to
		 * reflect the dirrewrite above.  If this is
		 * a directory it is empty and there are
		 * no links to it, so we can squash the inode and
		 * any space associated with it.  We disallowed
		 * renaming over top of a directory with links to
		 * it above, as the remaining link would point to
		 * a directory without "." or ".." entries.
		 */
		xp->i_e2fs_nlink--;
		if (doingdirectory) {
			if (--xp->i_e2fs_nlink != 0)
				panic("rename: linked directory");
			error = ext2fs_truncate(tvp, (off_t)0, IO_SYNC,
			    tcnp->cn_cred);
		}
		xp->i_flag |= IN_CHANGE;
		VN_KNOTE(tdvp, NOTE_WRITE);
		vput(tdvp);
		VN_KNOTE(tvp, NOTE_DELETE);
		vput(tvp);
		xp = NULL;
	}

	/*
	 * 3) Unlink the source.
	 */
	fcnp->cn_flags &= ~(MODMASK);
	fcnp->cn_flags |= LOCKPARENT | LOCKLEAF;
	vn_lock(fdvp, LK_EXCLUSIVE | LK_RETRY);
	if ((error = relookup(fdvp, &fvp, fcnp, 0))) {
		vput(fdvp);
		vrele(ap->a_fvp);
		return (error);
	}
	if (fvp != NULL) {
		xp = VTOI(fvp);
		dp = VTOI(fdvp);
	} else {
		/*
		 * From name has disappeared.
		 */
		if (doingdirectory)
			panic("ext2fs_rename: lost dir entry");
		vrele(ap->a_fvp);
		return (0);
	}
	/*
	 * Ensure that the directory entry still exists and has not
	 * changed while the new name has been entered. If the source is
	 * a file then the entry may have been unlinked or renamed. In
	 * either case there is no further work to be done. If the source
	 * is a directory then it cannot have been rmdir'ed; its link
	 * count of three would cause a rmdir to fail with ENOTEMPTY.
	 * The IRENAME flag ensures that it cannot be moved by another
	 * rename.
	 */
	if (xp != ip) {
		if (doingdirectory)
			panic("ext2fs_rename: lost dir entry");
	} else {
		/*
		 * If the source is a directory with a
		 * new parent, the link count of the old
		 * parent directory must be decremented
		 * and ".." set to point to the new parent.
		 */
		if (doingdirectory && newparent) {
			KASSERT(dp != NULL);
			dp->i_e2fs_nlink--;
			dp->i_flag |= IN_CHANGE;
			error = vn_rdwr(UIO_READ, fvp, (void *)&dirbuf,
				sizeof (struct ext2fs_dirtemplate), (off_t)0,
				UIO_SYSSPACE, IO_NODELOCKED,
				tcnp->cn_cred, (size_t *)0, NULL);
			if (error == 0) {
					namlen = dirbuf.dotdot_namlen;
				if (namlen != 2 ||
				    dirbuf.dotdot_name[0] != '.' ||
				    dirbuf.dotdot_name[1] != '.') {
					ufs_dirbad(xp, (doff_t)12,
					    "ext2fs_rename: mangled dir");
				} else {
					dirbuf.dotdot_ino = h2fs32(newparent);
					(void) vn_rdwr(UIO_WRITE, fvp,
					    (void *)&dirbuf,
					    sizeof (struct dirtemplate),
					    (off_t)0, UIO_SYSSPACE,
					    IO_NODELOCKED|IO_SYNC,
					    tcnp->cn_cred, (size_t *)0,
					    NULL);
					cache_purge(fdvp);
				}
			}
		}
		error = ext2fs_dirremove(fdvp, &VTOI(fdvp)->i_crap, fcnp);
		if (!error) {
			xp->i_e2fs_nlink--;
			xp->i_flag |= IN_CHANGE;
		}
		xp->i_flag &= ~IN_RENAME;
	}
	VN_KNOTE(fvp, NOTE_RENAME);
	if (dp)
		vput(fdvp);
	if (xp)
		vput(fvp);
	vrele(ap->a_fvp);
	return (error);

bad:
	if (xp)
		vput(ITOV(xp));
	vput(ITOV(dp));
out:
	if (doingdirectory)
		ip->i_flag &= ~IN_RENAME;
	if (vn_lock(fvp, LK_EXCLUSIVE) == 0) {
		ip->i_e2fs_nlink--;
		ip->i_flag |= IN_CHANGE;
		vput(fvp);
	} else
		vrele(fvp);
	vrele(fdvp);
	return (error);
}

/*
 * Mkdir system call
 */
int
ext2fs_mkdir(void *v)
{
	struct vop_mkdir_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	struct vnode		*dvp = ap->a_dvp;
	struct vattr		*vap = ap->a_vap;
	struct componentname	*cnp = ap->a_cnp;
	struct inode		*ip, *dp = VTOI(dvp);
	struct vnode		*tvp;
	struct ext2fs_dirtemplate dirtemplate;
	int			error, dmode;
	struct ufs_lookup_results *ulr;

	/* XXX should handle this material another way */
	ulr = &VTOI(dvp)->i_crap;
	UFS_CHECK_CRAPCOUNTER(VTOI(dvp));

	if ((nlink_t)dp->i_e2fs_nlink >= LINK_MAX) {
		error = EMLINK;
		goto out;
	}
	dmode = vap->va_mode & ACCESSPERMS;
	dmode |= IFDIR;
	/*
	 * Must simulate part of ext2fs_makeinode here to acquire the inode,
	 * but not have it entered in the parent directory. The entry is
	 * made later after writing "." and ".." entries.
	 */
	if ((error = ext2fs_valloc(dvp, dmode, cnp->cn_cred, &tvp)) != 0)
		goto out;
	ip = VTOI(tvp);
	ip->i_uid = kauth_cred_geteuid(cnp->cn_cred);
	ip->i_e2fs_uid = ip->i_uid & 0xffff;
	ip->i_e2fs_gid = dp->i_e2fs_gid;
	if (ip->i_e2fs->e2fs.e2fs_rev > E2FS_REV0) {
		ip->i_e2fs_uid_high = (ip->i_uid >> 16) & 0xffff;
		ip->i_e2fs_gid_high = dp->i_e2fs_gid_high;
	} else {
		ip->i_e2fs_uid_high = 0;
		ip->i_e2fs_gid_high = 0;
	}
	ip->i_gid = ip->i_e2fs_gid | (ip->i_e2fs_gid_high << 16);
	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
	ip->i_e2fs_mode = dmode;
	tvp->v_type = VDIR;	/* Rest init'd in getnewvnode(). */
	ip->i_e2fs_nlink = 2;

	/*
	 * Bump link count in parent directory
	 * to reflect work done below.  Should
	 * be done before reference is created
	 * so reparation is possible if we crash.
	 */
	dp->i_e2fs_nlink++;
	dp->i_flag |= IN_CHANGE;
	if ((error = ext2fs_update(dvp, NULL, NULL, UPDATE_DIROP)) != 0)
		goto bad;

	/* Initialize directory with "." and ".." from static template. */
	memset(&dirtemplate, 0, sizeof(dirtemplate));
	dirtemplate.dot_ino = h2fs32(ip->i_number);
	dirtemplate.dot_reclen = h2fs16(12);
	dirtemplate.dot_namlen = 1;
	if (ip->i_e2fs->e2fs.e2fs_rev > E2FS_REV0 &&
	    (ip->i_e2fs->e2fs.e2fs_features_incompat & EXT2F_INCOMPAT_FTYPE)) {
		dirtemplate.dot_type = EXT2_FT_DIR;
	}
	dirtemplate.dot_name[0] = '.';
	dirtemplate.dotdot_ino = h2fs32(dp->i_number);
    dirtemplate.dotdot_reclen = h2fs16(VTOI(dvp)->i_e2fs->e2fs_bsize - 12);
	dirtemplate.dotdot_namlen = 2;
	if (ip->i_e2fs->e2fs.e2fs_rev > E2FS_REV0 &&
	    (ip->i_e2fs->e2fs.e2fs_features_incompat & EXT2F_INCOMPAT_FTYPE)) {
		dirtemplate.dotdot_type = EXT2_FT_DIR;
	}
	dirtemplate.dotdot_name[0] = dirtemplate.dotdot_name[1] = '.';
	error = vn_rdwr(UIO_WRITE, tvp, (void *)&dirtemplate,
	    sizeof (dirtemplate), (off_t)0, UIO_SYSSPACE,
	    IO_NODELOCKED|IO_SYNC, cnp->cn_cred, (size_t *)0, NULL);
	if (error) {
		dp->i_e2fs_nlink--;
		dp->i_flag |= IN_CHANGE;
		goto bad;
	}
	if (VTOI(dvp)->i_e2fs->e2fs_bsize > dvp->v_mount->mnt_stat.f_bsize)
		panic("ext2fs_mkdir: blksize"); /* XXX should grow with balloc() */
	else {
		error = ext2fs_setsize(ip, VTOI(dvp)->i_e2fs->e2fs_bsize);
		if (error) {
			dp->i_e2fs_nlink--;
			dp->i_flag |= IN_CHANGE;
			goto bad;
		}
		ip->i_flag |= IN_CHANGE;
		uvm_vnp_setsize(tvp, ext2fs_size(ip));
	}

	/* Directory set up, now install it's entry in the parent directory. */
	error = ext2fs_direnter(ip, dvp, ulr, cnp);
	if (error != 0) {
		dp->i_e2fs_nlink--;
		dp->i_flag |= IN_CHANGE;
	}
bad:
	/*
	 * No need to do an explicit ext2fs_truncate here, vrele will do this
	 * for us because we set the link count to 0.
	 */
	if (error) {
		ip->i_e2fs_nlink = 0;
		ip->i_flag |= IN_CHANGE;
		vput(tvp);
	} else {
		VN_KNOTE(dvp, NOTE_WRITE | NOTE_LINK);
		*ap->a_vpp = tvp;
	}
out:
	vput(dvp);
	return (error);
}

/*
 * Rmdir system call.
 */
int
ext2fs_rmdir(void *v)
{
	struct vop_rmdir_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	struct inode *ip, *dp;
	int error;
	struct ufs_lookup_results *ulr;

	ip = VTOI(vp);
	dp = VTOI(dvp);

	/* XXX should handle this material another way */
	ulr = &dp->i_crap;
	UFS_CHECK_CRAPCOUNTER(dp);

	/*
	 * No rmdir "." please.
	 */
	if (dp == ip) {
		vrele(dvp);
		vput(vp);
		return (EINVAL);
	}
	/*
	 * Verify the directory is empty (and valid).
	 * (Rmdir ".." won't be valid since
	 *  ".." will contain a reference to
	 *  the current directory and thus be
	 *  non-empty.)
	 */
	error = 0;
	if (ip->i_e2fs_nlink != 2 ||
	    !ext2fs_dirempty(ip, dp->i_number, cnp->cn_cred)) {
		error = ENOTEMPTY;
		goto out;
	}
	if ((dp->i_e2fs_flags & EXT2_APPEND) ||
				 (ip->i_e2fs_flags & (EXT2_IMMUTABLE | EXT2_APPEND))) {
		error = EPERM;
		goto out;
	}
	/*
	 * Delete reference to directory before purging
	 * inode.  If we crash in between, the directory
	 * will be reattached to lost+found,
	 */
	error = ext2fs_dirremove(dvp, ulr, cnp);
	if (error != 0)
		goto out;
	dp->i_e2fs_nlink--;
	dp->i_flag |= IN_CHANGE;
	VN_KNOTE(dvp, NOTE_WRITE | NOTE_LINK);
	cache_purge(dvp);
	vput(dvp);
	dvp = NULL;
	/*
	 * Truncate inode.  The only stuff left
	 * in the directory is "." and "..".  The
	 * "." reference is inconsequential since
	 * we're quashing it.  The ".." reference
	 * has already been adjusted above.  We've
	 * removed the "." reference and the reference
	 * in the parent directory, but there may be
	 * other hard links so decrement by 2 and
	 * worry about them later.
	 */
	ip->i_e2fs_nlink -= 2;
	error = ext2fs_truncate(vp, (off_t)0, IO_SYNC, cnp->cn_cred);
	cache_purge(ITOV(ip));
out:
	VN_KNOTE(vp, NOTE_DELETE);
	if (dvp)
		vput(dvp);
	vput(vp);
	return (error);
}

/*
 * symlink -- make a symbolic link
 */
int
ext2fs_symlink(void *v)
{
	struct vop_symlink_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		char *a_target;
	} */ *ap = v;
	struct vnode	*vp, **vpp;
	struct inode	*ip;
	int		len, error;

	vpp = ap->a_vpp;
	error = ext2fs_makeinode(IFLNK | ap->a_vap->va_mode, ap->a_dvp,
			      vpp, ap->a_cnp);
	if (error)
		return (error);
	VN_KNOTE(ap->a_dvp, NOTE_WRITE);
	vp = *vpp;
	len = strlen(ap->a_target);
	ip = VTOI(vp);
	if (len < ip->i_ump->um_maxsymlinklen) {
		memcpy((char *)ip->i_din.e2fs_din->e2di_shortlink, ap->a_target, len);
		error = ext2fs_setsize(ip, len);
		if (error)
			goto bad;
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		if (vp->v_mount->mnt_flag & MNT_RELATIME)
			ip->i_flag |= IN_ACCESS;
		uvm_vnp_setsize(vp, len);
	} else
		error = vn_rdwr(UIO_WRITE, vp, ap->a_target, len, (off_t)0,
		    UIO_SYSSPACE, IO_NODELOCKED, ap->a_cnp->cn_cred,
		    (size_t *)0, NULL);
bad:
	if (error)
		vput(vp);
	return (error);
}

/*
 * Return target name of a symbolic link
 */
int
ext2fs_readlink(void *v)
{
	struct vop_readlink_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode	*vp = ap->a_vp;
	struct inode	*ip = VTOI(vp);
	struct ufsmount	*ump = ip->i_ump;
	int		isize;

	isize = ext2fs_size(ip);
	if (isize < ump->um_maxsymlinklen ||
	    (ump->um_maxsymlinklen == 0 && ip->i_e2fs_nblock == 0)) {
		uiomove((char *)ip->i_din.e2fs_din->e2di_shortlink, isize, ap->a_uio);
		return (0);
	}
	return (VOP_READ(vp, ap->a_uio, 0, ap->a_cred));
}

/*
 * Advisory record locking support
 */
int
ext2fs_advlock(void *v)
{
	struct vop_advlock_args /* {
		struct vnode *a_vp;
		void * a_id;
		int  a_op;
		struct flock *a_fl;
		int  a_flags;
	} */ *ap = v;
	struct inode *ip = VTOI(ap->a_vp);

	return lf_advlock(ap, &ip->i_lockf, ext2fs_size(ip));
}

int
ext2fs_fsync(void *v)
{
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		kauth_cred_t a_cred;
		int a_flags;
		off_t offlo;
		off_t offhi;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	int wait;
	int error;

	wait = (ap->a_flags & FSYNC_WAIT) != 0;

	if (vp->v_type == VBLK)
		error = spec_fsync(v);
	else
		error = vflushbuf(vp, wait);
	if (error == 0 && (ap->a_flags & FSYNC_DATAONLY) == 0)
		error = ext2fs_update(vp, NULL, NULL, wait ? UPDATE_WAIT : 0);

	if (error == 0 && ap->a_flags & FSYNC_CACHE) {
		int l = 0;
		error = VOP_IOCTL(VTOI(vp)->i_devvp, DIOCCACHESYNC, &l, FWRITE,
		    curlwp->l_cred);
	}

	return error;
}

/*
 * Initialize the vnode associated with a new inode, handle aliased
 * vnodes.
 */
int
ext2fs_vinit(struct mount *mntp, int (**specops)(void *),
	int (**fifoops)(void *), struct vnode **vpp)
{
	struct timeval tv;
	struct inode *ip;
	struct vnode *vp;

	vp = *vpp;
	ip = VTOI(vp);
	switch(vp->v_type = IFTOVT(ip->i_e2fs_mode)) {
	case VCHR:
	case VBLK:
		vp->v_op = specops;
		spec_node_init(vp, fs2h32(ip->i_din.e2fs_din->e2di_rdev));
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
	SETHIGH(ip->i_modrev, tv.tv_sec);
	SETLOW(ip->i_modrev, tv.tv_usec * 4294);
	*vpp = vp;
	return (0);
}

/*
 * Allocate a new inode.
 */
int
ext2fs_makeinode(int mode, struct vnode *dvp, struct vnode **vpp,
		struct componentname *cnp)
{
	struct inode *ip, *pdir;
	struct vnode *tvp;
	int error, ismember = 0;
	struct ufs_lookup_results *ulr;

	pdir = VTOI(dvp);

	/* XXX should handle this material another way */
	ulr = &pdir->i_crap;
	UFS_CHECK_CRAPCOUNTER(pdir);

	*vpp = NULL;
	if ((mode & IFMT) == 0)
		mode |= IFREG;

	if ((error = ext2fs_valloc(dvp, mode, cnp->cn_cred, &tvp)) != 0) {
		vput(dvp);
		return (error);
	}
	ip = VTOI(tvp);
	ip->i_uid = kauth_cred_geteuid(cnp->cn_cred);
	ip->i_e2fs_uid = ip->i_uid & 0xffff;
	ip->i_e2fs_gid = pdir->i_e2fs_gid;
	if (ip->i_e2fs->e2fs.e2fs_rev > E2FS_REV0) {
		ip->i_e2fs_uid_high = (ip->i_uid >> 16) & 0xffff;
		ip->i_e2fs_gid_high = pdir->i_e2fs_gid_high;
	} else {
		ip->i_e2fs_uid_high = 0;
		ip->i_e2fs_gid_high = 0;
	}
	ip->i_gid = ip->i_e2fs_gid | (ip->i_e2fs_gid_high << 16);
	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
	ip->i_e2fs_mode = mode;
	tvp->v_type = IFTOVT(mode);	/* Rest init'd in getnewvnode(). */
	ip->i_e2fs_nlink = 1;
	if ((ip->i_e2fs_mode & ISGID) && (kauth_cred_ismember_gid(cnp->cn_cred,
	    ip->i_gid, &ismember) != 0 || !ismember) &&
	    kauth_authorize_generic(cnp->cn_cred, KAUTH_GENERIC_ISSUSER, NULL))
		ip->i_e2fs_mode &= ~ISGID;

	/*
	 * Make sure inode goes to disk before directory entry.
	 */
	if ((error = ext2fs_update(tvp, NULL, NULL, UPDATE_WAIT)) != 0)
		goto bad;
	error = ext2fs_direnter(ip, dvp, ulr, cnp);
	if (error != 0)
		goto bad;
	vput(dvp);
	*vpp = tvp;
	return (0);

bad:
	/*
	 * Write error occurred trying to update the inode
	 * or the directory so must deallocate the inode.
	 */
	tvp->v_type = VNON;	/* Stop explosion if VBLK */
	ip->i_e2fs_nlink = 0;
	ip->i_flag |= IN_CHANGE;
	vput(tvp);
	vput(dvp);
	return (error);
}

/*
 * Reclaim an inode so that it can be used for other purposes.
 */
int
ext2fs_reclaim(void *v)
{
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	int error;

	/*
	 * The inode must be freed and updated before being removed
	 * from its hash chain.  Other threads trying to gain a hold
	 * on the inode will be stalled because it is locked (VI_XLOCK).
	 */
	if (ip->i_omode == 1 && (vp->v_mount->mnt_flag & MNT_RDONLY) == 0)
		ext2fs_vfree(vp, ip->i_number, ip->i_e2fs_mode);
	if ((error = ufs_reclaim(vp)) != 0)
		return (error);
	if (ip->i_din.e2fs_din != NULL)
		pool_put(&ext2fs_dinode_pool, ip->i_din.e2fs_din);
	genfs_node_destroy(vp);
	pool_put(&ext2fs_inode_pool, vp->v_data);
	vp->v_data = NULL;
	return (0);
}

/* Global vfs data structures for ext2fs. */
int (**ext2fs_vnodeop_p)(void *);
const struct vnodeopv_entry_desc ext2fs_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, ext2fs_lookup },		/* lookup */
	{ &vop_create_desc, ext2fs_create },		/* create */
	{ &vop_mknod_desc, ext2fs_mknod },		/* mknod */
	{ &vop_open_desc, ext2fs_open },		/* open */
	{ &vop_close_desc, ufs_close },			/* close */
	{ &vop_access_desc, ext2fs_access },		/* access */
	{ &vop_getattr_desc, ext2fs_getattr },		/* getattr */
	{ &vop_setattr_desc, ext2fs_setattr },		/* setattr */
	{ &vop_read_desc, ext2fs_read },		/* read */
	{ &vop_write_desc, ext2fs_write },		/* write */
	{ &vop_ioctl_desc, ufs_ioctl },			/* ioctl */
	{ &vop_fcntl_desc, ufs_fcntl },			/* fcntl */
	{ &vop_poll_desc, ufs_poll },			/* poll */
	{ &vop_kqfilter_desc, genfs_kqfilter },		/* kqfilter */
	{ &vop_revoke_desc, ufs_revoke },		/* revoke */
	{ &vop_mmap_desc, ufs_mmap },			/* mmap */
	{ &vop_fsync_desc, ext2fs_fsync },		/* fsync */
	{ &vop_seek_desc, ufs_seek },			/* seek */
	{ &vop_remove_desc, ext2fs_remove },		/* remove */
	{ &vop_link_desc, ext2fs_link },		/* link */
	{ &vop_rename_desc, ext2fs_rename },		/* rename */
	{ &vop_mkdir_desc, ext2fs_mkdir },		/* mkdir */
	{ &vop_rmdir_desc, ext2fs_rmdir },		/* rmdir */
	{ &vop_symlink_desc, ext2fs_symlink },		/* symlink */
	{ &vop_readdir_desc, ext2fs_readdir },		/* readdir */
	{ &vop_readlink_desc, ext2fs_readlink },	/* readlink */
	{ &vop_abortop_desc, ufs_abortop },		/* abortop */
	{ &vop_inactive_desc, ext2fs_inactive },	/* inactive */
	{ &vop_reclaim_desc, ext2fs_reclaim },		/* reclaim */
	{ &vop_lock_desc, ufs_lock },			/* lock */
	{ &vop_unlock_desc, ufs_unlock },		/* unlock */
	{ &vop_bmap_desc, ext2fs_bmap },		/* bmap */
	{ &vop_strategy_desc, ufs_strategy },		/* strategy */
	{ &vop_print_desc, ufs_print },			/* print */
	{ &vop_islocked_desc, ufs_islocked },		/* islocked */
	{ &vop_pathconf_desc, ufs_pathconf },		/* pathconf */
	{ &vop_advlock_desc, ext2fs_advlock },		/* advlock */
	{ &vop_bwrite_desc, vn_bwrite },		/* bwrite */
	{ &vop_getpages_desc, genfs_getpages },		/* getpages */
	{ &vop_putpages_desc, genfs_putpages },		/* putpages */
	{ NULL, NULL }
};
const struct vnodeopv_desc ext2fs_vnodeop_opv_desc =
	{ &ext2fs_vnodeop_p, ext2fs_vnodeop_entries };

int (**ext2fs_specop_p)(void *);
const struct vnodeopv_entry_desc ext2fs_specop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, spec_lookup },		/* lookup */
	{ &vop_create_desc, spec_create },		/* create */
	{ &vop_mknod_desc, spec_mknod },		/* mknod */
	{ &vop_open_desc, spec_open },			/* open */
	{ &vop_close_desc, ufsspec_close },		/* close */
	{ &vop_access_desc, ext2fs_access },		/* access */
	{ &vop_getattr_desc, ext2fs_getattr },		/* getattr */
	{ &vop_setattr_desc, ext2fs_setattr },		/* setattr */
	{ &vop_read_desc, ufsspec_read },		/* read */
	{ &vop_write_desc, ufsspec_write },		/* write */
	{ &vop_ioctl_desc, spec_ioctl },		/* ioctl */
	{ &vop_fcntl_desc, ufs_fcntl },			/* fcntl */
	{ &vop_poll_desc, spec_poll },			/* poll */
	{ &vop_kqfilter_desc, spec_kqfilter },		/* kqfilter */
	{ &vop_revoke_desc, spec_revoke },		/* revoke */
	{ &vop_mmap_desc, spec_mmap },			/* mmap */
	{ &vop_fsync_desc, ext2fs_fsync },		/* fsync */
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
	{ &vop_inactive_desc, ext2fs_inactive },	/* inactive */
	{ &vop_reclaim_desc, ext2fs_reclaim },		/* reclaim */
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
	{ NULL, NULL }
};
const struct vnodeopv_desc ext2fs_specop_opv_desc =
	{ &ext2fs_specop_p, ext2fs_specop_entries };

int (**ext2fs_fifoop_p)(void *);
const struct vnodeopv_entry_desc ext2fs_fifoop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, vn_fifo_bypass },		/* lookup */
	{ &vop_create_desc, vn_fifo_bypass },		/* create */
	{ &vop_mknod_desc, vn_fifo_bypass },		/* mknod */
	{ &vop_open_desc, vn_fifo_bypass },		/* open */
	{ &vop_close_desc, ufsfifo_close },		/* close */
	{ &vop_access_desc, ext2fs_access },		/* access */
	{ &vop_getattr_desc, ext2fs_getattr },		/* getattr */
	{ &vop_setattr_desc, ext2fs_setattr },		/* setattr */
	{ &vop_read_desc, ufsfifo_read },		/* read */
	{ &vop_write_desc, ufsfifo_write },		/* write */
	{ &vop_ioctl_desc, vn_fifo_bypass },		/* ioctl */
	{ &vop_fcntl_desc, ufs_fcntl },			/* fcntl */
	{ &vop_poll_desc, vn_fifo_bypass },		/* poll */
	{ &vop_kqfilter_desc, vn_fifo_bypass },		/* kqfilter */
	{ &vop_revoke_desc, vn_fifo_bypass },		/* revoke */
	{ &vop_mmap_desc, vn_fifo_bypass },		/* mmap */
	{ &vop_fsync_desc, ext2fs_fsync },		/* fsync */
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
	{ &vop_inactive_desc, ext2fs_inactive },	/* inactive */
	{ &vop_reclaim_desc, ext2fs_reclaim },		/* reclaim */
	{ &vop_lock_desc, ufs_lock },			/* lock */
	{ &vop_unlock_desc, ufs_unlock },		/* unlock */
	{ &vop_bmap_desc, vn_fifo_bypass },		/* bmap */
	{ &vop_strategy_desc, vn_fifo_bypass },		/* strategy */
	{ &vop_print_desc, ufs_print },			/* print */
	{ &vop_islocked_desc, ufs_islocked },		/* islocked */
	{ &vop_pathconf_desc, vn_fifo_bypass },		/* pathconf */
	{ &vop_advlock_desc, vn_fifo_bypass },		/* advlock */
	{ &vop_bwrite_desc, vn_bwrite },		/* bwrite */
	{ &vop_putpages_desc, vn_fifo_bypass },		/* putpages */
	{ NULL, NULL }
};
const struct vnodeopv_desc ext2fs_fifoop_opv_desc =
	{ &ext2fs_fifoop_p, ext2fs_fifoop_entries };
