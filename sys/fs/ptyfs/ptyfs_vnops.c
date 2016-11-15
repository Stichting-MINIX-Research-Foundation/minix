/*	$NetBSD: ptyfs_vnops.c,v 1.51 2015/06/23 10:41:06 hannken Exp $	*/

/*
 * Copyright (c) 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
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
 *	@(#)procfs_vnops.c	8.18 (Berkeley) 5/21/95
 */

/*
 * Copyright (c) 1993 Jan-Simon Pendry
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)procfs_vnops.c	8.18 (Berkeley) 5/21/95
 */

/*
 * ptyfs vnode interface
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ptyfs_vnops.c,v 1.51 2015/06/23 10:41:06 hannken Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/select.h>
#include <sys/dirent.h>
#include <sys/resourcevar.h>
#include <sys/stat.h>
#include <sys/conf.h>
#include <sys/tty.h>
#include <sys/pty.h>
#include <sys/kauth.h>

#include <uvm/uvm_extern.h>	/* for PAGE_SIZE */

#include <machine/reg.h>

#include <fs/ptyfs/ptyfs.h>
#include <miscfs/genfs/genfs.h>
#include <miscfs/specfs/specdev.h>

MALLOC_DECLARE(M_PTYFSTMP);

/*
 * Vnode Operations.
 *
 */

int	ptyfs_lookup	(void *);
#define	ptyfs_create	genfs_eopnotsupp
#define	ptyfs_mknod	genfs_eopnotsupp
int	ptyfs_open	(void *);
int	ptyfs_close	(void *);
int	ptyfs_access	(void *);
int	ptyfs_getattr	(void *);
int	ptyfs_setattr	(void *);
int	ptyfs_read	(void *);
int	ptyfs_write	(void *);
#define	ptyfs_fcntl	genfs_fcntl
int	ptyfs_ioctl	(void *);
int	ptyfs_poll	(void *);
int	ptyfs_kqfilter	(void *);
#define ptyfs_revoke	genfs_revoke
#define	ptyfs_mmap	genfs_eopnotsupp
#define	ptyfs_fsync	genfs_nullop
#define	ptyfs_seek	genfs_nullop
#define	ptyfs_remove	genfs_eopnotsupp
#define	ptyfs_link	genfs_abortop
#define	ptyfs_rename	genfs_eopnotsupp
#define	ptyfs_mkdir	genfs_eopnotsupp
#define	ptyfs_rmdir	genfs_eopnotsupp
#define	ptyfs_symlink	genfs_abortop
int	ptyfs_readdir	(void *);
#define	ptyfs_readlink	genfs_eopnotsupp
#define	ptyfs_abortop	genfs_abortop
int	ptyfs_reclaim	(void *);
int	ptyfs_inactive	(void *);
#define	ptyfs_lock	genfs_lock
#define	ptyfs_unlock	genfs_unlock
#define	ptyfs_bmap	genfs_badop
#define	ptyfs_strategy	genfs_badop
int	ptyfs_print	(void *);
int	ptyfs_pathconf	(void *);
#define	ptyfs_islocked	genfs_islocked
int	ptyfs_advlock	(void *);
#define	ptyfs_bwrite	genfs_eopnotsupp
#define ptyfs_putpages	genfs_null_putpages

static int ptyfs_update(struct vnode *, const struct timespec *,
    const struct timespec *, int);
static int ptyfs_chown(struct vnode *, uid_t, gid_t, kauth_cred_t,
    struct lwp *);
static int ptyfs_chmod(struct vnode *, mode_t, kauth_cred_t, struct lwp *);
static int atoi(const char *, size_t);

/*
 * ptyfs vnode operations.
 */
int (**ptyfs_vnodeop_p)(void *);
const struct vnodeopv_entry_desc ptyfs_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, ptyfs_lookup },		/* lookup */
	{ &vop_create_desc, ptyfs_create },		/* create */
	{ &vop_mknod_desc, ptyfs_mknod },		/* mknod */
	{ &vop_open_desc, ptyfs_open },			/* open */
	{ &vop_close_desc, ptyfs_close },		/* close */
	{ &vop_access_desc, ptyfs_access },		/* access */
	{ &vop_getattr_desc, ptyfs_getattr },		/* getattr */
	{ &vop_setattr_desc, ptyfs_setattr },		/* setattr */
	{ &vop_read_desc, ptyfs_read },			/* read */
	{ &vop_write_desc, ptyfs_write },		/* write */
	{ &vop_fallocate_desc, genfs_eopnotsupp },	/* fallocate */
	{ &vop_fdiscard_desc, genfs_eopnotsupp },	/* fdiscard */
	{ &vop_ioctl_desc, ptyfs_ioctl },		/* ioctl */
	{ &vop_fcntl_desc, ptyfs_fcntl },		/* fcntl */
	{ &vop_poll_desc, ptyfs_poll },			/* poll */
	{ &vop_kqfilter_desc, ptyfs_kqfilter },		/* kqfilter */
	{ &vop_revoke_desc, ptyfs_revoke },		/* revoke */
	{ &vop_mmap_desc, ptyfs_mmap },			/* mmap */
	{ &vop_fsync_desc, ptyfs_fsync },		/* fsync */
	{ &vop_seek_desc, ptyfs_seek },			/* seek */
	{ &vop_remove_desc, ptyfs_remove },		/* remove */
	{ &vop_link_desc, ptyfs_link },			/* link */
	{ &vop_rename_desc, ptyfs_rename },		/* rename */
	{ &vop_mkdir_desc, ptyfs_mkdir },		/* mkdir */
	{ &vop_rmdir_desc, ptyfs_rmdir },		/* rmdir */
	{ &vop_symlink_desc, ptyfs_symlink },		/* symlink */
	{ &vop_readdir_desc, ptyfs_readdir },		/* readdir */
	{ &vop_readlink_desc, ptyfs_readlink },		/* readlink */
	{ &vop_abortop_desc, ptyfs_abortop },		/* abortop */
	{ &vop_inactive_desc, ptyfs_inactive },		/* inactive */
	{ &vop_reclaim_desc, ptyfs_reclaim },		/* reclaim */
	{ &vop_lock_desc, ptyfs_lock },			/* lock */
	{ &vop_unlock_desc, ptyfs_unlock },		/* unlock */
	{ &vop_bmap_desc, ptyfs_bmap },			/* bmap */
	{ &vop_strategy_desc, ptyfs_strategy },		/* strategy */
	{ &vop_print_desc, ptyfs_print },		/* print */
	{ &vop_islocked_desc, ptyfs_islocked },		/* islocked */
	{ &vop_pathconf_desc, ptyfs_pathconf },		/* pathconf */
	{ &vop_advlock_desc, ptyfs_advlock },		/* advlock */
	{ &vop_bwrite_desc, ptyfs_bwrite },		/* bwrite */
	{ &vop_putpages_desc, ptyfs_putpages },		/* putpages */
	{ NULL, NULL }
};
const struct vnodeopv_desc ptyfs_vnodeop_opv_desc =
	{ &ptyfs_vnodeop_p, ptyfs_vnodeop_entries };

/*
 * free any private data and remove the node
 * from any private lists.
 */
int
ptyfs_reclaim(void *v)
{
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct ptyfsnode *ptyfs = VTOPTYFS(vp);

	vcache_remove(vp->v_mount, &ptyfs->ptyfs_key, sizeof(ptyfs->ptyfs_key));
	vp->v_data = NULL;
	return 0;
}

int
ptyfs_inactive(void *v)
{
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		bool *a_recycle;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct ptyfsnode *ptyfs = VTOPTYFS(vp);

	if (ptyfs->ptyfs_type == PTYFSptc)
		ptyfs_clr_active(vp->v_mount, ptyfs->ptyfs_pty);
	VOP_UNLOCK(vp);
	return 0;
}

/*
 * Return POSIX pathconf information applicable to special devices.
 */
int
ptyfs_pathconf(void *v)
{
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		register_t *a_retval;
	} */ *ap = v;

	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = LINK_MAX;
		return 0;
	case _PC_MAX_CANON:
		*ap->a_retval = MAX_CANON;
		return 0;
	case _PC_MAX_INPUT:
		*ap->a_retval = MAX_INPUT;
		return 0;
	case _PC_PIPE_BUF:
		*ap->a_retval = PIPE_BUF;
		return 0;
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		return 0;
	case _PC_VDISABLE:
		*ap->a_retval = _POSIX_VDISABLE;
		return 0;
	case _PC_SYNC_IO:
		*ap->a_retval = 1;
		return 0;
	default:
		return EINVAL;
	}
}

/*
 * _print is used for debugging.
 * just print a readable description
 * of (vp).
 */
int
ptyfs_print(void *v)
{
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct ptyfsnode *ptyfs = VTOPTYFS(ap->a_vp);

	printf("tag VT_PTYFS, type %d, pty %d\n",
	    ptyfs->ptyfs_type, ptyfs->ptyfs_pty);
	return 0;
}

/*
 * support advisory locking on pty nodes
 */
int
ptyfs_advlock(void *v)
{
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct ptyfsnode *ptyfs = VTOPTYFS(ap->a_vp);

	switch (ptyfs->ptyfs_type) {
	case PTYFSpts:
	case PTYFSptc:
		return spec_advlock(v);
	default:
		return EOPNOTSUPP;
	}
}

/*
 * Invent attributes for ptyfsnode (vp) and store
 * them in (vap).
 * Directories lengths are returned as zero since
 * any real length would require the genuine size
 * to be computed, and nothing cares anyway.
 *
 * this is relatively minimal for ptyfs.
 */
int
ptyfs_getattr(void *v)
{
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct ptyfsnode *ptyfs = VTOPTYFS(ap->a_vp);
	struct vattr *vap = ap->a_vap;

	PTYFS_ITIMES(ptyfs, NULL, NULL, NULL);

	/* start by zeroing out the attributes */
	vattr_null(vap);

	/* next do all the common fields */
	vap->va_type = ap->a_vp->v_type;
	vap->va_fsid = ap->a_vp->v_mount->mnt_stat.f_fsidx.__fsid_val[0];
	vap->va_fileid = ptyfs->ptyfs_fileno;
	vap->va_gen = 0;
	vap->va_flags = 0;
	vap->va_blocksize = PAGE_SIZE;

	vap->va_atime = ptyfs->ptyfs_atime;
	vap->va_mtime = ptyfs->ptyfs_mtime;
	vap->va_ctime = ptyfs->ptyfs_ctime;
	vap->va_birthtime = ptyfs->ptyfs_birthtime;
	vap->va_mode = ptyfs->ptyfs_mode;
	vap->va_flags = ptyfs->ptyfs_flags;
	vap->va_uid = ptyfs->ptyfs_uid;
	vap->va_gid = ptyfs->ptyfs_gid;

	switch (ptyfs->ptyfs_type) {
	case PTYFSpts:
	case PTYFSptc:
		if (pty_isfree(ptyfs->ptyfs_pty, 1))
			return ENOENT;
		vap->va_bytes = vap->va_size = 0;
		vap->va_rdev = ap->a_vp->v_rdev;
		vap->va_nlink = 1;
		break;
	case PTYFSroot:
		vap->va_rdev = 0;
		vap->va_bytes = vap->va_size = DEV_BSIZE;
		vap->va_nlink = 2;
		break;
	default:
		return EOPNOTSUPP;
	}

	return 0;
}

/*ARGSUSED*/
int
ptyfs_setattr(void *v)
{
	struct vop_setattr_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct ptyfsnode *ptyfs = VTOPTYFS(vp);
	struct vattr *vap = ap->a_vap;
	kauth_cred_t cred = ap->a_cred;
	struct lwp *l = curlwp;
	int error;
	kauth_action_t action = KAUTH_VNODE_WRITE_FLAGS;
	bool changing_sysflags = false;

	if (vap->va_size != VNOVAL) {
 		switch (ptyfs->ptyfs_type) {
 		case PTYFSroot:
 			return EISDIR;
 		case PTYFSpts:
 		case PTYFSptc:
			break;
		default:
			return EINVAL;
		}
	}

	if (vap->va_flags != VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return EROFS;

		/* Immutable and append-only flags are not supported on ptyfs. */
		if (vap->va_flags & (IMMUTABLE | APPEND))
			return EINVAL;

		/* Snapshot flag cannot be set or cleared */
		if ((vap->va_flags & SF_SNAPSHOT) != (ptyfs->ptyfs_flags & SF_SNAPSHOT))
			return EPERM;

		if ((ptyfs->ptyfs_flags & SF_SETTABLE) != (vap->va_flags & SF_SETTABLE)) {
			changing_sysflags = true;
			action |= KAUTH_VNODE_WRITE_SYSFLAGS;
		}

		error = kauth_authorize_vnode(cred, action, vp, NULL,
		    genfs_can_chflags(cred, vp->v_type, ptyfs->ptyfs_uid,
		    changing_sysflags));
		if (error)
			return error;

		if (changing_sysflags) {
			ptyfs->ptyfs_flags = vap->va_flags;
		} else {
			ptyfs->ptyfs_flags &= SF_SETTABLE;
			ptyfs->ptyfs_flags |= (vap->va_flags & UF_SETTABLE);
		}
		ptyfs->ptyfs_status |= PTYFS_CHANGE;
	}

	/*
	 * Go through the fields and update iff not VNOVAL.
	 */
	if (vap->va_uid != (uid_t)VNOVAL || vap->va_gid != (gid_t)VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return EROFS;
		if (ptyfs->ptyfs_type == PTYFSroot)
			return EPERM;
		error = ptyfs_chown(vp, vap->va_uid, vap->va_gid, cred, l);
		if (error)
			return error;
	}

	if (vap->va_atime.tv_sec != VNOVAL || vap->va_mtime.tv_sec != VNOVAL ||
	    vap->va_birthtime.tv_sec != VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return EROFS;
		if ((ptyfs->ptyfs_flags & SF_SNAPSHOT) != 0)
			return EPERM;
		error = kauth_authorize_vnode(cred, KAUTH_VNODE_WRITE_TIMES, vp,
		    NULL, genfs_can_chtimes(vp, vap->va_vaflags,
		    ptyfs->ptyfs_uid, cred));
		if (error)
			return (error);
		if (vap->va_atime.tv_sec != VNOVAL)
			if (!(vp->v_mount->mnt_flag & MNT_NOATIME))
				ptyfs->ptyfs_status |= PTYFS_ACCESS;
		if (vap->va_mtime.tv_sec != VNOVAL) {
			ptyfs->ptyfs_status |= PTYFS_CHANGE | PTYFS_MODIFY;
			if (vp->v_mount->mnt_flag & MNT_RELATIME)
				ptyfs->ptyfs_status |= PTYFS_ACCESS;
		}
		if (vap->va_birthtime.tv_sec != VNOVAL)
			ptyfs->ptyfs_birthtime = vap->va_birthtime;
		ptyfs->ptyfs_status |= PTYFS_CHANGE;
		error = ptyfs_update(vp, &vap->va_atime, &vap->va_mtime, 0);
		if (error)
			return error;
	}
	if (vap->va_mode != (mode_t)VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return EROFS;
		if (ptyfs->ptyfs_type == PTYFSroot)
			return EPERM;
		if ((ptyfs->ptyfs_flags & SF_SNAPSHOT) != 0 &&
		    (vap->va_mode &
		    (S_IXUSR|S_IWUSR|S_IXGRP|S_IWGRP|S_IXOTH|S_IWOTH)))
			return EPERM;
		error = ptyfs_chmod(vp, vap->va_mode, cred, l);
		if (error)
			return error;
	}
	VN_KNOTE(vp, NOTE_ATTRIB);
	return 0;
}

/*
 * Change the mode on a file.
 * Inode must be locked before calling.
 */
static int
ptyfs_chmod(struct vnode *vp, mode_t mode, kauth_cred_t cred, struct lwp *l)
{
	struct ptyfsnode *ptyfs = VTOPTYFS(vp);
	int error;

	error = kauth_authorize_vnode(cred, KAUTH_VNODE_WRITE_SECURITY, vp,
	    NULL, genfs_can_chmod(vp->v_type, cred, ptyfs->ptyfs_uid,
	    ptyfs->ptyfs_gid, mode));
	if (error)
		return (error);

	ptyfs->ptyfs_mode &= ~ALLPERMS;
	ptyfs->ptyfs_mode |= (mode & ALLPERMS);
	return 0;
}

/*
 * Perform chown operation on inode ip;
 * inode must be locked prior to call.
 */
static int
ptyfs_chown(struct vnode *vp, uid_t uid, gid_t gid, kauth_cred_t cred,
    struct lwp *l)
{
	struct ptyfsnode *ptyfs = VTOPTYFS(vp);
	int error;

	if (uid == (uid_t)VNOVAL)
		uid = ptyfs->ptyfs_uid;
	if (gid == (gid_t)VNOVAL)
		gid = ptyfs->ptyfs_gid;

	error = kauth_authorize_vnode(cred, KAUTH_VNODE_CHANGE_OWNERSHIP, vp,
	    NULL, genfs_can_chown(cred, ptyfs->ptyfs_uid, ptyfs->ptyfs_gid,
	    uid, gid));
	if (error)
		return (error);

	ptyfs->ptyfs_gid = gid;
	ptyfs->ptyfs_uid = uid;
	return 0;
}

/*
 * implement access checking.
 *
 * actually, the check for super-user is slightly
 * broken since it will allow read access to write-only
 * objects.  this doesn't cause any particular trouble
 * but does mean that the i/o entry points need to check
 * that the operation really does make sense.
 */
int
ptyfs_access(void *v)
{
	struct vop_access_args /* {
		struct vnode *a_vp;
		int a_mode;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vattr va;
	int error;

	if ((error = VOP_GETATTR(ap->a_vp, &va, ap->a_cred)) != 0)
		return error;

	return kauth_authorize_vnode(ap->a_cred,
	    KAUTH_ACCESS_ACTION(ap->a_mode, ap->a_vp->v_type, va.va_mode),
	    ap->a_vp, NULL, genfs_can_access(va.va_type, va.va_mode, va.va_uid,
	    va.va_gid, ap->a_mode, ap->a_cred));
}

/*
 * lookup.  this is incredibly complicated in the
 * general case, however for most pseudo-filesystems
 * very little needs to be done.
 *
 * Locking isn't hard here, just poorly documented.
 *
 * If we're looking up ".", just vref the parent & return it.
 *
 * If we're looking up "..", unlock the parent, and lock "..". If everything
 * went ok, try to re-lock the parent. We do this to prevent lock races.
 *
 * For anything else, get the needed node.
 *
 * We try to exit with the parent locked in error cases.
 */
int
ptyfs_lookup(void *v)
{
	struct vop_lookup_v2_args /* {
		struct vnode * a_dvp;
		struct vnode ** a_vpp;
		struct componentname * a_cnp;
	} */ *ap = v;
	struct componentname *cnp = ap->a_cnp;
	struct vnode **vpp = ap->a_vpp;
	struct vnode *dvp = ap->a_dvp;
	const char *pname = cnp->cn_nameptr;
	struct ptyfsnode *ptyfs;
	int pty, error;

	*vpp = NULL;

	if (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME)
		return EROFS;

	if (cnp->cn_namelen == 1 && *pname == '.') {
		*vpp = dvp;
		vref(dvp);
		return 0;
	}

	ptyfs = VTOPTYFS(dvp);
	switch (ptyfs->ptyfs_type) {
	case PTYFSroot:
		/*
		 * Shouldn't get here with .. in the root node.
		 */
		if (cnp->cn_flags & ISDOTDOT)
			return EIO;

		pty = atoi(pname, cnp->cn_namelen);
		if (pty < 0 || ptyfs_next_active(dvp->v_mount, pty) != pty)
			break;
		error = ptyfs_allocvp(dvp->v_mount, vpp, PTYFSpts, pty);
		if (error)
			return error;
		if (ptyfs_next_active(dvp->v_mount, pty) != pty) {
			vrele(*vpp);
			*vpp = NULL;
			return ENOENT;
		}
		return 0;

	default:
		return ENOTDIR;
	}

	return cnp->cn_nameiop == LOOKUP ? ENOENT : EROFS;
}

/*
 * readdir returns directory entries from ptyfsnode (vp).
 *
 * the strategy here with ptyfs is to generate a single
 * directory entry at a time (struct dirent) and then
 * copy that out to userland using uiomove.  a more efficent
 * though more complex implementation, would try to minimize
 * the number of calls to uiomove().  for ptyfs, this is
 * hardly worth the added code complexity.
 *
 * this should just be done through read()
 */
int
ptyfs_readdir(void *v)
{
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		kauth_cred_t a_cred;
		int *a_eofflag;
		off_t **a_cookies;
		int *a_ncookies;
	} */ *ap = v;
	struct uio *uio = ap->a_uio;
	struct dirent *dp;
	struct ptyfsnode *ptyfs;
	off_t i;
	int error;
	off_t *cookies = NULL;
	int ncookies;
	struct vnode *vp;
	int n, nc = 0;

	vp = ap->a_vp;
	ptyfs = VTOPTYFS(vp);

	if (uio->uio_resid < UIO_MX)
		return EINVAL;
	if (uio->uio_offset < 0)
		return EINVAL;

	dp = malloc(sizeof(struct dirent), M_PTYFSTMP, M_WAITOK | M_ZERO);

	error = 0;
	i = uio->uio_offset;
	dp->d_reclen = UIO_MX;
	ncookies = uio->uio_resid / UIO_MX;

	if (ptyfs->ptyfs_type != PTYFSroot) {
		error = ENOTDIR;
		goto out;
	}

	if (i >= npty)
		goto out;

	if (ap->a_ncookies) {
		ncookies = min(ncookies, (npty + 2 - i));
		cookies = malloc(ncookies * sizeof (off_t),
		    M_TEMP, M_WAITOK);
		*ap->a_cookies = cookies;
	}

	for (; i < 2; i++) {
		/* `.' and/or `..' */
		dp->d_fileno = PTYFS_FILENO(0, PTYFSroot);
		dp->d_namlen = i + 1;
		(void)memcpy(dp->d_name, "..", dp->d_namlen);
		dp->d_name[i + 1] = '\0';
		dp->d_type = DT_DIR;
		if ((error = uiomove(dp, UIO_MX, uio)) != 0)
			goto out;
		if (cookies)
			*cookies++ = i + 1;
		nc++;
	}
	while (uio->uio_resid >= UIO_MX) {
		/* check for used ptys */
		n = ptyfs_next_active(vp->v_mount, i - 2);
		if (n < 0)
			break;
		dp->d_fileno = PTYFS_FILENO(n, PTYFSpts);
		dp->d_namlen = snprintf(dp->d_name, sizeof(dp->d_name),
		    "%lld", (long long)(n));
		dp->d_type = DT_CHR;
		if ((error = uiomove(dp, UIO_MX, uio)) != 0)
			goto out;
		i = n + 3;
		if (cookies)
			*cookies++ = i;
		nc++;
	}

out:
	/* not pertinent in error cases */
	ncookies = nc;

	if (ap->a_ncookies) {
		if (error) {
			if (cookies)
				free(*ap->a_cookies, M_TEMP);
			*ap->a_ncookies = 0;
			*ap->a_cookies = NULL;
		} else
			*ap->a_ncookies = ncookies;
	}
	uio->uio_offset = i;
	free(dp, M_PTYFSTMP);
	return error;
}

int
ptyfs_open(void *v)
{
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct ptyfsnode *ptyfs = VTOPTYFS(vp);

	switch (ptyfs->ptyfs_type) {
	case PTYFSpts:
	case PTYFSptc:
		return spec_open(v);
	case PTYFSroot:
		return 0;
	default:
		return EINVAL;
	}
}

int
ptyfs_close(void *v)
{
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct ptyfsnode *ptyfs = VTOPTYFS(vp);

	mutex_enter(vp->v_interlock);
	if (vp->v_usecount > 1)
		PTYFS_ITIMES(ptyfs, NULL, NULL, NULL);
	mutex_exit(vp->v_interlock);

	switch (ptyfs->ptyfs_type) {
	case PTYFSpts:
	case PTYFSptc:
		return spec_close(v);
	case PTYFSroot:
		return 0;
	default:
		return EINVAL;
	}
}

int
ptyfs_read(void *v)
{
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct timespec ts;
	struct vnode *vp = ap->a_vp;
	struct ptyfsnode *ptyfs = VTOPTYFS(vp);
	int error;

	if (vp->v_type == VDIR)
		return EISDIR;

	ptyfs->ptyfs_status |= PTYFS_ACCESS;
	/* hardclock() resolution is good enough for ptyfs */
	getnanotime(&ts);
	(void)ptyfs_update(vp, &ts, &ts, 0);

	switch (ptyfs->ptyfs_type) {
	case PTYFSpts:
	case PTYFSptc:
		VOP_UNLOCK(vp);
		error = cdev_read(vp->v_rdev, ap->a_uio, ap->a_ioflag);
		vn_lock(vp, LK_RETRY|LK_EXCLUSIVE);
		return error;
	default:
		return EOPNOTSUPP;
	}
}

int
ptyfs_write(void *v)
{
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct timespec ts;
	struct vnode *vp = ap->a_vp;
	struct ptyfsnode *ptyfs = VTOPTYFS(vp);
	int error;

	ptyfs->ptyfs_status |= PTYFS_MODIFY;
	getnanotime(&ts);
	(void)ptyfs_update(vp, &ts, &ts, 0);

	switch (ptyfs->ptyfs_type) {
	case PTYFSpts:
	case PTYFSptc:
		VOP_UNLOCK(vp);
		error = cdev_write(vp->v_rdev, ap->a_uio, ap->a_ioflag);
		vn_lock(vp, LK_RETRY|LK_EXCLUSIVE);
		return error;
	default:
		return EOPNOTSUPP;
	}
}

int
ptyfs_ioctl(void *v)
{
	struct vop_ioctl_args /* {
		struct vnode *a_vp;
		u_long a_command;
		void *a_data;
		int  a_fflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct ptyfsnode *ptyfs = VTOPTYFS(vp);

	switch (ptyfs->ptyfs_type) {
	case PTYFSpts:
	case PTYFSptc:
		return cdev_ioctl(vp->v_rdev, ap->a_command,
		    ap->a_data, ap->a_fflag, curlwp);
	default:
		return EOPNOTSUPP;
	}
}

int
ptyfs_poll(void *v)
{
	struct vop_poll_args /* {
		struct vnode *a_vp;
		int a_events;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct ptyfsnode *ptyfs = VTOPTYFS(vp);

	switch (ptyfs->ptyfs_type) {
	case PTYFSpts:
	case PTYFSptc:
		return cdev_poll(vp->v_rdev, ap->a_events, curlwp);
	default:
		return genfs_poll(v);
	}
}

int
ptyfs_kqfilter(void *v)
{
	struct vop_kqfilter_args /* {
		struct vnode *a_vp;
		struct knote *a_kn;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct ptyfsnode *ptyfs = VTOPTYFS(vp);

	switch (ptyfs->ptyfs_type) {
	case PTYFSpts:
	case PTYFSptc:
		return cdev_kqfilter(vp->v_rdev, ap->a_kn);
	default:
		return genfs_kqfilter(v);
	}
}

static int
ptyfs_update(struct vnode *vp, const struct timespec *acc,
    const struct timespec *mod, int flags)
{
	struct ptyfsnode *ptyfs = VTOPTYFS(vp);

	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return 0;

	PTYFS_ITIMES(ptyfs, acc, mod, NULL);
	return 0;
}

void
ptyfs_itimes(struct ptyfsnode *ptyfs, const struct timespec *acc,
    const struct timespec *mod, const struct timespec *cre)
{
	struct timespec now;
 
	KASSERT(ptyfs->ptyfs_status & (PTYFS_ACCESS|PTYFS_CHANGE|PTYFS_MODIFY));

	getnanotime(&now);
	if (ptyfs->ptyfs_status & PTYFS_ACCESS) {
		if (acc == NULL)
			acc = &now;
		ptyfs->ptyfs_atime = *acc;
	}
	if (ptyfs->ptyfs_status & PTYFS_MODIFY) {
		if (mod == NULL)
			mod = &now;
		ptyfs->ptyfs_mtime = *mod;
	}
	if (ptyfs->ptyfs_status & PTYFS_CHANGE) {
		if (cre == NULL)
			cre = &now;
		ptyfs->ptyfs_ctime = *cre;
	}
	ptyfs->ptyfs_status &= ~(PTYFS_ACCESS|PTYFS_CHANGE|PTYFS_MODIFY);
}

/*
 * convert decimal ascii to int
 */
static int
atoi(const char *b, size_t len)
{
	int p = 0;

	while (len--) {
		char c = *b++;
		if (c < '0' || c > '9')
			return -1;
		p = 10 * p + (c - '0');
	}

	return p;
}
