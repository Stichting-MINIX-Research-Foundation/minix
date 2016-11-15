/*	$NetBSD: tmpfs_vnops.c,v 1.123 2015/07/06 10:07:12 hannken Exp $	*/

/*
 * Copyright (c) 2005, 2006, 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Julio M. Merino Vidal, developed as part of Google's Summer of Code
 * 2005 program.
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
 * tmpfs vnode interface.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: tmpfs_vnops.c,v 1.123 2015/07/06 10:07:12 hannken Exp $");

#include <sys/param.h>
#include <sys/dirent.h>
#include <sys/fcntl.h>
#include <sys/event.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <sys/lockf.h>
#include <sys/kauth.h>
#include <sys/atomic.h>

#include <uvm/uvm.h>

#include <miscfs/fifofs/fifo.h>
#include <miscfs/genfs/genfs.h>
#include <fs/tmpfs/tmpfs_vnops.h>
#include <fs/tmpfs/tmpfs.h>

/*
 * vnode operations vector used for files stored in a tmpfs file system.
 */
int (**tmpfs_vnodeop_p)(void *);
const struct vnodeopv_entry_desc tmpfs_vnodeop_entries[] = {
	{ &vop_default_desc,		vn_default_error },
	{ &vop_lookup_desc,		tmpfs_lookup },
	{ &vop_create_desc,		tmpfs_create },
	{ &vop_mknod_desc,		tmpfs_mknod },
	{ &vop_open_desc,		tmpfs_open },
	{ &vop_close_desc,		tmpfs_close },
	{ &vop_access_desc,		tmpfs_access },
	{ &vop_getattr_desc,		tmpfs_getattr },
	{ &vop_setattr_desc,		tmpfs_setattr },
	{ &vop_read_desc,		tmpfs_read },
	{ &vop_write_desc,		tmpfs_write },
	{ &vop_fallocate_desc,		genfs_eopnotsupp },
	{ &vop_fdiscard_desc,		genfs_eopnotsupp },
	{ &vop_ioctl_desc,		tmpfs_ioctl },
	{ &vop_fcntl_desc,		tmpfs_fcntl },
	{ &vop_poll_desc,		tmpfs_poll },
	{ &vop_kqfilter_desc,		tmpfs_kqfilter },
	{ &vop_revoke_desc,		tmpfs_revoke },
	{ &vop_mmap_desc,		tmpfs_mmap },
	{ &vop_fsync_desc,		tmpfs_fsync },
	{ &vop_seek_desc,		tmpfs_seek },
	{ &vop_remove_desc,		tmpfs_remove },
	{ &vop_link_desc,		tmpfs_link },
	{ &vop_rename_desc,		tmpfs_rename },
	{ &vop_mkdir_desc,		tmpfs_mkdir },
	{ &vop_rmdir_desc,		tmpfs_rmdir },
	{ &vop_symlink_desc,		tmpfs_symlink },
	{ &vop_readdir_desc,		tmpfs_readdir },
	{ &vop_readlink_desc,		tmpfs_readlink },
	{ &vop_abortop_desc,		tmpfs_abortop },
	{ &vop_inactive_desc,		tmpfs_inactive },
	{ &vop_reclaim_desc,		tmpfs_reclaim },
	{ &vop_lock_desc,		tmpfs_lock },
	{ &vop_unlock_desc,		tmpfs_unlock },
	{ &vop_bmap_desc,		tmpfs_bmap },
	{ &vop_strategy_desc,		tmpfs_strategy },
	{ &vop_print_desc,		tmpfs_print },
	{ &vop_pathconf_desc,		tmpfs_pathconf },
	{ &vop_islocked_desc,		tmpfs_islocked },
	{ &vop_advlock_desc,		tmpfs_advlock },
	{ &vop_bwrite_desc,		tmpfs_bwrite },
	{ &vop_getpages_desc,		tmpfs_getpages },
	{ &vop_putpages_desc,		tmpfs_putpages },
	{ &vop_whiteout_desc,		tmpfs_whiteout },
	{ NULL, NULL }
};

const struct vnodeopv_desc tmpfs_vnodeop_opv_desc = {
	&tmpfs_vnodeop_p, tmpfs_vnodeop_entries
};

/*
 * tmpfs_lookup: path name traversal routine.
 *
 * Arguments: dvp (directory being searched), vpp (result),
 * cnp (component name - path).
 *
 * => Caller holds a reference and lock on dvp.
 * => We return looked-up vnode (vpp) locked, with a reference held.
 */
int
tmpfs_lookup(void *v)
{
	struct vop_lookup_v2_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap = v;
	vnode_t *dvp = ap->a_dvp, **vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	const bool lastcn = (cnp->cn_flags & ISLASTCN) != 0;
	tmpfs_node_t *dnode, *tnode;
	tmpfs_dirent_t *de;
	int cachefound, iswhiteout;
	int error;

	KASSERT(VOP_ISLOCKED(dvp));

	dnode = VP_TO_TMPFS_DIR(dvp);
	*vpp = NULL;

	/* Check accessibility of directory. */
	error = VOP_ACCESS(dvp, VEXEC, cnp->cn_cred);
	if (error) {
		goto out;
	}

	/*
	 * If requesting the last path component on a read-only file system
	 * with a write operation, deny it.
	 */
	if (lastcn && (dvp->v_mount->mnt_flag & MNT_RDONLY) != 0 &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME)) {
		error = EROFS;
		goto out;
	}

	/*
	 * Avoid doing a linear scan of the directory if the requested
	 * directory/name couple is already in the cache.
	 */
	cachefound = cache_lookup(dvp, cnp->cn_nameptr, cnp->cn_namelen,
				  cnp->cn_nameiop, cnp->cn_flags,
				  &iswhiteout, vpp);
	if (iswhiteout) {
		cnp->cn_flags |= ISWHITEOUT;
	}
	if (cachefound && *vpp == NULLVP) {
		/* Negative cache hit. */
		error = ENOENT;
		goto out;
	} else if (cachefound) {
		error = 0;
		goto out;
	}

	/*
	 * Treat an unlinked directory as empty (no "." or "..")
	 */
	if (dnode->tn_links == 0) {
		KASSERT(dnode->tn_size == 0);
		error = ENOENT;
		goto out;
	}

	if (cnp->cn_flags & ISDOTDOT) {
		tmpfs_node_t *pnode;

		/*
		 * Lookup of ".." case.
		 */
		if (lastcn && cnp->cn_nameiop == RENAME) {
			error = EINVAL;
			goto out;
		}
		KASSERT(dnode->tn_type == VDIR);
		pnode = dnode->tn_spec.tn_dir.tn_parent;
		if (pnode == NULL) {
			error = ENOENT;
			goto out;
		}

		error = vcache_get(dvp->v_mount, &pnode, sizeof(pnode), vpp);
		goto out;
	} else if (cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.') {
		/*
		 * Lookup of "." case.
		 */
		if (lastcn && cnp->cn_nameiop == RENAME) {
			error = EISDIR;
			goto out;
		}
		vref(dvp);
		*vpp = dvp;
		error = 0;
		goto done;
	}

	/*
	 * Other lookup cases: perform directory scan.
	 */
	de = tmpfs_dir_lookup(dnode, cnp);
	if (de == NULL || de->td_node == TMPFS_NODE_WHITEOUT) {
		/*
		 * The entry was not found in the directory.  This is valid
		 * if we are creating or renaming an entry and are working
		 * on the last component of the path name.
		 */
		if (lastcn && (cnp->cn_nameiop == CREATE ||
		    cnp->cn_nameiop == RENAME)) {
			error = VOP_ACCESS(dvp, VWRITE, cnp->cn_cred);
			if (error) {
				goto out;
			}
			error = EJUSTRETURN;
		} else {
			error = ENOENT;
		}
		if (de) {
			KASSERT(de->td_node == TMPFS_NODE_WHITEOUT);
			cnp->cn_flags |= ISWHITEOUT;
		}
		goto done;
	}

	tnode = de->td_node;

	/*
	 * If it is not the last path component and found a non-directory
	 * or non-link entry (which may itself be pointing to a directory),
	 * raise an error.
	 */
	if (!lastcn && tnode->tn_type != VDIR && tnode->tn_type != VLNK) {
		error = ENOTDIR;
		goto out;
	}

	/* Check the permissions. */
	if (lastcn && (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME)) {
		error = VOP_ACCESS(dvp, VWRITE, cnp->cn_cred);
		if (error)
			goto out;

		if ((dnode->tn_mode & S_ISTXT) != 0) {
			error = kauth_authorize_vnode(cnp->cn_cred,
			    KAUTH_VNODE_DELETE, tnode->tn_vnode,
			    dnode->tn_vnode, genfs_can_sticky(cnp->cn_cred,
			    dnode->tn_uid, tnode->tn_uid));
			if (error) {
				error = EPERM;
				goto out;
			}
		}
	}

	/* Get a vnode for the matching entry. */
	error = vcache_get(dvp->v_mount, &tnode, sizeof(tnode), vpp);
done:
	/*
	 * Cache the result, unless request was for creation (as it does
	 * not improve the performance).
	 */
	if (cnp->cn_nameiop != CREATE) {
		cache_enter(dvp, *vpp, cnp->cn_nameptr, cnp->cn_namelen,
			    cnp->cn_flags);
	}
out:
	KASSERT(VOP_ISLOCKED(dvp));

	return error;
}

int
tmpfs_create(void *v)
{
	struct vop_create_v3_args /* {
		struct vnode		*a_dvp;
		struct vnode		**a_vpp;
		struct componentname	*a_cnp;
		struct vattr		*a_vap;
	} */ *ap = v;
	vnode_t *dvp = ap->a_dvp, **vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	struct vattr *vap = ap->a_vap;

	KASSERT(VOP_ISLOCKED(dvp));
	KASSERT(vap->va_type == VREG || vap->va_type == VSOCK);
	return tmpfs_construct_node(dvp, vpp, vap, cnp, NULL);
}

int
tmpfs_mknod(void *v)
{
	struct vop_mknod_v3_args /* {
		struct vnode		*a_dvp;
		struct vnode		**a_vpp;
		struct componentname	*a_cnp;
		struct vattr		*a_vap;
	} */ *ap = v;
	vnode_t *dvp = ap->a_dvp, **vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	struct vattr *vap = ap->a_vap;
	enum vtype vt = vap->va_type;

	if (vt != VBLK && vt != VCHR && vt != VFIFO) {
		*vpp = NULL;
		return EINVAL;
	}
	return tmpfs_construct_node(dvp, vpp, vap, cnp, NULL);
}

int
tmpfs_open(void *v)
{
	struct vop_open_args /* {
		struct vnode	*a_vp;
		int		a_mode;
		kauth_cred_t	a_cred;
	} */ *ap = v;
	vnode_t *vp = ap->a_vp;
	mode_t mode = ap->a_mode;
	tmpfs_node_t *node;

	KASSERT(VOP_ISLOCKED(vp));

	node = VP_TO_TMPFS_NODE(vp);

	/* If the file is marked append-only, deny write requests. */
	if ((node->tn_flags & APPEND) != 0 &&
	    (mode & (FWRITE | O_APPEND)) == FWRITE) {
		return EPERM;
	}
	return 0;
}

int
tmpfs_close(void *v)
{
	struct vop_close_args /* {
		struct vnode	*a_vp;
		int		a_fflag;
		kauth_cred_t	a_cred;
	} */ *ap = v;
	vnode_t *vp __diagused = ap->a_vp;

	KASSERT(VOP_ISLOCKED(vp));
	return 0;
}

int
tmpfs_access(void *v)
{
	struct vop_access_args /* {
		struct vnode	*a_vp;
		int		a_mode;
		kauth_cred_t	a_cred;
	} */ *ap = v;
	vnode_t *vp = ap->a_vp;
	mode_t mode = ap->a_mode;
	kauth_cred_t cred = ap->a_cred;
	tmpfs_node_t *node = VP_TO_TMPFS_NODE(vp);
	const bool writing = (mode & VWRITE) != 0;

	KASSERT(VOP_ISLOCKED(vp));

	/* Possible? */
	switch (vp->v_type) {
	case VDIR:
	case VLNK:
	case VREG:
		if (writing && (vp->v_mount->mnt_flag & MNT_RDONLY) != 0) {
			return EROFS;
		}
		break;
	case VBLK:
	case VCHR:
	case VSOCK:
	case VFIFO:
		break;
	default:
		return EINVAL;
	}
	if (writing && (node->tn_flags & IMMUTABLE) != 0) {
		return EPERM;
	}

	return kauth_authorize_vnode(cred, KAUTH_ACCESS_ACTION(mode,
	    vp->v_type, node->tn_mode), vp, NULL, genfs_can_access(vp->v_type,
	    node->tn_mode, node->tn_uid, node->tn_gid, mode, cred));
}

int
tmpfs_getattr(void *v)
{
	struct vop_getattr_args /* {
		struct vnode	*a_vp;
		struct vattr	*a_vap;
		kauth_cred_t	a_cred;
	} */ *ap = v;
	vnode_t *vp = ap->a_vp;
	struct vattr *vap = ap->a_vap;
	tmpfs_node_t *node = VP_TO_TMPFS_NODE(vp);

	vattr_null(vap);

	vap->va_type = vp->v_type;
	vap->va_mode = node->tn_mode;
	vap->va_nlink = node->tn_links;
	vap->va_uid = node->tn_uid;
	vap->va_gid = node->tn_gid;
	vap->va_fsid = vp->v_mount->mnt_stat.f_fsidx.__fsid_val[0];
	vap->va_fileid = node->tn_id;
	vap->va_size = node->tn_size;
	vap->va_blocksize = PAGE_SIZE;
	vap->va_atime = node->tn_atime;
	vap->va_mtime = node->tn_mtime;
	vap->va_ctime = node->tn_ctime;
	vap->va_birthtime = node->tn_birthtime;
	vap->va_gen = TMPFS_NODE_GEN(node);
	vap->va_flags = node->tn_flags;
	vap->va_rdev = (vp->v_type == VBLK || vp->v_type == VCHR) ?
	    node->tn_spec.tn_dev.tn_rdev : VNOVAL;
	vap->va_bytes = round_page(node->tn_size);
	vap->va_filerev = VNOVAL;
	vap->va_vaflags = 0;
	vap->va_spare = VNOVAL; /* XXX */

	return 0;
}

int
tmpfs_setattr(void *v)
{
	struct vop_setattr_args /* {
		struct vnode	*a_vp;
		struct vattr	*a_vap;
		kauth_cred_t	a_cred;
	} */ *ap = v;
	vnode_t *vp = ap->a_vp;
	struct vattr *vap = ap->a_vap;
	kauth_cred_t cred = ap->a_cred;
	lwp_t *l = curlwp;
	int error = 0;

	KASSERT(VOP_ISLOCKED(vp));

	/* Abort if any unsettable attribute is given. */
	if (vap->va_type != VNON || vap->va_nlink != VNOVAL ||
	    vap->va_fsid != VNOVAL || vap->va_fileid != VNOVAL ||
	    vap->va_blocksize != VNOVAL || vap->va_ctime.tv_sec != VNOVAL ||
	    vap->va_gen != VNOVAL || vap->va_rdev != VNOVAL ||
	    vap->va_bytes != VNOVAL) {
		return EINVAL;
	}

	if (error == 0 && vap->va_flags != VNOVAL)
		error = tmpfs_chflags(vp, vap->va_flags, cred, l);

	if (error == 0 && vap->va_size != VNOVAL)
		error = tmpfs_chsize(vp, vap->va_size, cred, l);

	if (error == 0 && (vap->va_uid != VNOVAL || vap->va_gid != VNOVAL))
		error = tmpfs_chown(vp, vap->va_uid, vap->va_gid, cred, l);

	if (error == 0 && vap->va_mode != VNOVAL)
		error = tmpfs_chmod(vp, vap->va_mode, cred, l);

	const bool chsometime =
	    vap->va_atime.tv_sec != VNOVAL ||
	    vap->va_mtime.tv_sec != VNOVAL ||
	    vap->va_birthtime.tv_sec != VNOVAL;
	if (error == 0 && chsometime) {
		error = tmpfs_chtimes(vp, &vap->va_atime, &vap->va_mtime,
		    &vap->va_birthtime, vap->va_vaflags, cred, l);
	}
	return error;
}

int
tmpfs_read(void *v)
{
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	vnode_t *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	const int ioflag = ap->a_ioflag;
	tmpfs_node_t *node;
	struct uvm_object *uobj;
	int error;

	KASSERT(VOP_ISLOCKED(vp));

	if (vp->v_type == VDIR) {
		return EISDIR;
	}
	if (uio->uio_offset < 0 || vp->v_type != VREG) {
		return EINVAL;
	}

	/* Note: reading zero bytes should not update atime. */
	if (uio->uio_resid == 0) {
		return 0;
	}

	node = VP_TO_TMPFS_NODE(vp);
	uobj = node->tn_spec.tn_reg.tn_aobj;
	error = 0;

	while (error == 0 && uio->uio_resid > 0) {
		vsize_t len;

		if (node->tn_size <= uio->uio_offset) {
			break;
		}
		len = MIN(node->tn_size - uio->uio_offset, uio->uio_resid);
		if (len == 0) {
			break;
		}
		error = ubc_uiomove(uobj, uio, len, IO_ADV_DECODE(ioflag),
		    UBC_READ | UBC_PARTIALOK | UBC_UNMAP_FLAG(vp));
	}

	tmpfs_update(vp, TMPFS_UPDATE_ATIME);
	return error;
}

int
tmpfs_write(void *v)
{
	struct vop_write_args /* {
		struct vnode	*a_vp;
		struct uio	*a_uio;
		int		a_ioflag;
		kauth_cred_t	a_cred;
	} */ *ap = v;
	vnode_t *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	const int ioflag = ap->a_ioflag;
	tmpfs_node_t *node;
	struct uvm_object *uobj;
	off_t oldsize;
	int error;

	KASSERT(VOP_ISLOCKED(vp));

	node = VP_TO_TMPFS_NODE(vp);
	oldsize = node->tn_size;

	if (uio->uio_offset < 0 || vp->v_type != VREG) {
		error = EINVAL;
		goto out;
	}
	if (uio->uio_resid == 0) {
		error = 0;
		goto out;
	}
	if (ioflag & IO_APPEND) {
		uio->uio_offset = node->tn_size;
	}

	if (uio->uio_offset + uio->uio_resid > node->tn_size) {
		error = tmpfs_reg_resize(vp, uio->uio_offset + uio->uio_resid);
		if (error)
			goto out;
	}

	uobj = node->tn_spec.tn_reg.tn_aobj;
	error = 0;
	while (error == 0 && uio->uio_resid > 0) {
		vsize_t len;

		len = MIN(node->tn_size - uio->uio_offset, uio->uio_resid);
		if (len == 0) {
			break;
		}
		error = ubc_uiomove(uobj, uio, len, IO_ADV_DECODE(ioflag),
		    UBC_WRITE | UBC_UNMAP_FLAG(vp));
	}
	if (error) {
		(void)tmpfs_reg_resize(vp, oldsize);
	}

	tmpfs_update(vp, TMPFS_UPDATE_MTIME | TMPFS_UPDATE_CTIME);
	VN_KNOTE(vp, NOTE_WRITE);
out:
	if (error) {
		KASSERT(oldsize == node->tn_size);
	} else {
		KASSERT(uio->uio_resid == 0);
	}
	return error;
}

int
tmpfs_fsync(void *v)
{
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		kauth_cred_t a_cred;
		int a_flags;
		off_t a_offlo;
		off_t a_offhi;
		struct lwp *a_l;
	} */ *ap = v;
	vnode_t *vp __diagused = ap->a_vp;

	/* Nothing to do.  Should be up to date. */
	KASSERT(VOP_ISLOCKED(vp));
	return 0;
}

/*
 * tmpfs_remove: unlink a file.
 *
 * => Both directory (dvp) and file (vp) are locked.
 * => We unlock and drop the reference on both.
 */
int
tmpfs_remove(void *v)
{
	struct vop_remove_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	vnode_t *dvp = ap->a_dvp, *vp = ap->a_vp;
	tmpfs_node_t *dnode, *node;
	tmpfs_dirent_t *de;
	int error;

	KASSERT(VOP_ISLOCKED(dvp));
	KASSERT(VOP_ISLOCKED(vp));

	if (vp->v_type == VDIR) {
		error = EPERM;
		goto out;
	}
	dnode = VP_TO_TMPFS_DIR(dvp);
	node = VP_TO_TMPFS_NODE(vp);

	/*
	 * Files marked as immutable or append-only cannot be deleted.
	 * Likewise, files residing on directories marked as append-only
	 * cannot be deleted.
	 */
	if (node->tn_flags & (IMMUTABLE | APPEND)) {
		error = EPERM;
		goto out;
	}
	if (dnode->tn_flags & APPEND) {
		error = EPERM;
		goto out;
	}

	/* Lookup the directory entry (check the cached hint first). */
	de = tmpfs_dir_cached(node);
	if (de == NULL) {
		struct componentname *cnp = ap->a_cnp;
		de = tmpfs_dir_lookup(dnode, cnp);
	}
	KASSERT(de && de->td_node == node);

	/*
	 * Remove the entry from the directory (drops the link count) and
	 * destroy it or replace with a whiteout.
	 *
	 * Note: the inode referred by it will not be destroyed until the
	 * vnode is reclaimed/recycled.
	 */

	tmpfs_dir_detach(dnode, de);

	if (ap->a_cnp->cn_flags & DOWHITEOUT)
		tmpfs_dir_attach(dnode, de, TMPFS_NODE_WHITEOUT);
	else
		tmpfs_free_dirent(VFS_TO_TMPFS(vp->v_mount), de);

	if (node->tn_links > 0) {
		/* We removed a hard link. */
		tmpfs_update(vp, TMPFS_UPDATE_CTIME);
	}
	tmpfs_update(dvp, TMPFS_UPDATE_MTIME | TMPFS_UPDATE_CTIME);
	error = 0;
out:
	/* Drop the references and unlock the vnodes. */
	vput(vp);
	if (dvp == vp) {
		vrele(dvp);
	} else {
		vput(dvp);
	}
	return error;
}

/*
 * tmpfs_link: create a hard link.
 */
int
tmpfs_link(void *v)
{
	struct vop_link_v2_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	vnode_t *dvp = ap->a_dvp;
	vnode_t *vp = ap->a_vp;
	struct componentname *cnp = ap->a_cnp;
	tmpfs_node_t *dnode, *node;
	tmpfs_dirent_t *de;
	int error;

	KASSERT(dvp != vp);
	KASSERT(VOP_ISLOCKED(dvp));
	KASSERT(vp->v_type != VDIR);
	KASSERT(dvp->v_mount == vp->v_mount);

	dnode = VP_TO_TMPFS_DIR(dvp);
	node = VP_TO_TMPFS_NODE(vp);

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);

	/* Check for maximum number of links limit. */
	if (node->tn_links == LINK_MAX) {
		error = EMLINK;
		goto out;
	}
	KASSERT(node->tn_links < LINK_MAX);

	/* We cannot create links of files marked immutable or append-only. */
	if (node->tn_flags & (IMMUTABLE | APPEND)) {
		error = EPERM;
		goto out;
	}

	/* Allocate a new directory entry to represent the inode. */
	error = tmpfs_alloc_dirent(VFS_TO_TMPFS(vp->v_mount),
	    cnp->cn_nameptr, cnp->cn_namelen, &de);
	if (error) {
		goto out;
	}

	/*
	 * Insert the entry into the directory.
	 * It will increase the inode link count.
	 */
	tmpfs_dir_attach(dnode, de, node);
	tmpfs_update(dvp, TMPFS_UPDATE_MTIME | TMPFS_UPDATE_CTIME);

	/* Update the timestamps and trigger the event. */
	if (node->tn_vnode) {
		VN_KNOTE(node->tn_vnode, NOTE_LINK);
	}
	tmpfs_update(vp, TMPFS_UPDATE_CTIME);
	error = 0;
out:
	VOP_UNLOCK(vp);
	return error;
}

int
tmpfs_mkdir(void *v)
{
	struct vop_mkdir_v3_args /* {
		struct vnode		*a_dvp;
		struct vnode		**a_vpp;
		struct componentname	*a_cnp;
		struct vattr		*a_vap;
	} */ *ap = v;
	vnode_t *dvp = ap->a_dvp;
	vnode_t **vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	struct vattr *vap = ap->a_vap;

	KASSERT(vap->va_type == VDIR);
	return tmpfs_construct_node(dvp, vpp, vap, cnp, NULL);
}

int
tmpfs_rmdir(void *v)
{
	struct vop_rmdir_args /* {
		struct vnode		*a_dvp;
		struct vnode		*a_vp;
		struct componentname	*a_cnp;
	} */ *ap = v;
	vnode_t *dvp = ap->a_dvp;
	vnode_t *vp = ap->a_vp;
	tmpfs_mount_t *tmp = VFS_TO_TMPFS(dvp->v_mount);
	tmpfs_node_t *dnode = VP_TO_TMPFS_DIR(dvp);
	tmpfs_node_t *node = VP_TO_TMPFS_DIR(vp);
	tmpfs_dirent_t *de;
	int error = 0;

	KASSERT(VOP_ISLOCKED(dvp));
	KASSERT(VOP_ISLOCKED(vp));

	/*
	 * Directories with more than two entries ('.' and '..') cannot be
	 * removed.  There may be whiteout entries, which we will destroy.
	 */
	if (node->tn_size > 0) {
		/*
		 * If never had whiteout entries, the directory is certainly
		 * not empty.  Otherwise, scan for any non-whiteout entry.
		 */
		if ((node->tn_gen & TMPFS_WHITEOUT_BIT) == 0) {
			error = ENOTEMPTY;
			goto out;
		}
		TAILQ_FOREACH(de, &node->tn_spec.tn_dir.tn_dir, td_entries) {
			if (de->td_node != TMPFS_NODE_WHITEOUT) {
				error = ENOTEMPTY;
				goto out;
			}
		}
		KASSERT(error == 0);
	}

	KASSERT(node->tn_spec.tn_dir.tn_parent == dnode);

	/* Lookup the directory entry (check the cached hint first). */
	de = tmpfs_dir_cached(node);
	if (de == NULL) {
		struct componentname *cnp = ap->a_cnp;
		de = tmpfs_dir_lookup(dnode, cnp);
	}
	KASSERT(de && de->td_node == node);

	/* Check flags to see if we are allowed to remove the directory. */
	if (dnode->tn_flags & APPEND || node->tn_flags & (IMMUTABLE | APPEND)) {
		error = EPERM;
		goto out;
	}

	/* Decrement the link count for the virtual '.' entry. */
	node->tn_links--;

	/* Detach the directory entry from the directory. */
	tmpfs_dir_detach(dnode, de);

	/* Purge the cache for parent. */
	cache_purge(dvp);

	/*
	 * Destroy the directory entry or replace it with a whiteout.
	 *
	 * Note: the inode referred by it will not be destroyed until the
	 * vnode is reclaimed.
	 */
	if (ap->a_cnp->cn_flags & DOWHITEOUT)
		tmpfs_dir_attach(dnode, de, TMPFS_NODE_WHITEOUT);
	else
		tmpfs_free_dirent(tmp, de);

	/* Destroy the whiteout entries from the node. */
	while ((de = TAILQ_FIRST(&node->tn_spec.tn_dir.tn_dir)) != NULL) {
		KASSERT(de->td_node == TMPFS_NODE_WHITEOUT);
		tmpfs_dir_detach(node, de);
		tmpfs_free_dirent(tmp, de);
	}
	tmpfs_update(dvp, TMPFS_UPDATE_MTIME | TMPFS_UPDATE_CTIME);

	KASSERT(node->tn_size == 0);
	KASSERT(node->tn_links == 0);
out:
	/* Release the nodes. */
	vput(dvp);
	vput(vp);
	return error;
}

int
tmpfs_symlink(void *v)
{
	struct vop_symlink_v3_args /* {
		struct vnode		*a_dvp;
		struct vnode		**a_vpp;
		struct componentname	*a_cnp;
		struct vattr		*a_vap;
		char			*a_target;
	} */ *ap = v;
	vnode_t *dvp = ap->a_dvp;
	vnode_t **vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	struct vattr *vap = ap->a_vap;
	char *target = ap->a_target;

	KASSERT(vap->va_type == VLNK);
	return tmpfs_construct_node(dvp, vpp, vap, cnp, target);
}

int
tmpfs_readdir(void *v)
{
	struct vop_readdir_args /* {
		struct vnode	*a_vp;
		struct uio	*a_uio;
		kauth_cred_t	a_cred;
		int		*a_eofflag;
		off_t		**a_cookies;
		int		*ncookies;
	} */ *ap = v;
	vnode_t *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	int *eofflag = ap->a_eofflag;
	off_t **cookies = ap->a_cookies;
	int *ncookies = ap->a_ncookies;
	off_t startoff, cnt;
	tmpfs_node_t *node;
	int error;

	KASSERT(VOP_ISLOCKED(vp));

	/* This operation only makes sense on directory nodes. */
	if (vp->v_type != VDIR) {
		return ENOTDIR;
	}
	node = VP_TO_TMPFS_DIR(vp);
	startoff = uio->uio_offset;
	cnt = 0;

	/*
	 * Retrieve the directory entries, unless it is being destroyed.
	 */
	if (node->tn_links) {
		error = tmpfs_dir_getdents(node, uio, &cnt);
	} else {
		error = 0;
	}

	if (eofflag != NULL) {
		*eofflag = !error && uio->uio_offset == TMPFS_DIRSEQ_EOF;
	}
	if (error || cookies == NULL || ncookies == NULL) {
		return error;
	}

	/* Update NFS-related variables, if any. */
	tmpfs_dirent_t *de = NULL;
	off_t i, off = startoff;

	*cookies = malloc(cnt * sizeof(off_t), M_TEMP, M_WAITOK);
	*ncookies = cnt;

	for (i = 0; i < cnt; i++) {
		KASSERT(off != TMPFS_DIRSEQ_EOF);
		if (off != TMPFS_DIRSEQ_DOT) {
			if (off == TMPFS_DIRSEQ_DOTDOT) {
				de = TAILQ_FIRST(&node->tn_spec.tn_dir.tn_dir);
			} else if (de != NULL) {
				de = TAILQ_NEXT(de, td_entries);
			} else {
				de = tmpfs_dir_lookupbyseq(node, off);
				KASSERT(de != NULL);
				de = TAILQ_NEXT(de, td_entries);
			}
			if (de == NULL) {
				off = TMPFS_DIRSEQ_EOF;
			} else {
				off = tmpfs_dir_getseq(node, de);
			}
		} else {
			off = TMPFS_DIRSEQ_DOTDOT;
		}
		(*cookies)[i] = off;
	}
	KASSERT(uio->uio_offset == off);
	return error;
}

int
tmpfs_readlink(void *v)
{
	struct vop_readlink_args /* {
		struct vnode	*a_vp;
		struct uio	*a_uio;
		kauth_cred_t	a_cred;
	} */ *ap = v;
	vnode_t *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	tmpfs_node_t *node = VP_TO_TMPFS_NODE(vp);
	int error;

	KASSERT(VOP_ISLOCKED(vp));
	KASSERT(uio->uio_offset == 0);
	KASSERT(vp->v_type == VLNK);

	/* Note: readlink(2) returns the path without NUL terminator. */
	if (node->tn_size > 0) {
		error = uiomove(node->tn_spec.tn_lnk.tn_link,
		    MIN(node->tn_size, uio->uio_resid), uio);
	} else {
		error = 0;
	}
	tmpfs_update(vp, TMPFS_UPDATE_ATIME);

	return error;
}

int
tmpfs_inactive(void *v)
{
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		bool *a_recycle;
	} */ *ap = v;
	vnode_t *vp = ap->a_vp;
	tmpfs_node_t *node;

	KASSERT(VOP_ISLOCKED(vp));

	node = VP_TO_TMPFS_NODE(vp);
	if (node->tn_links == 0) {
		/*
		 * Mark node as dead by setting its generation to zero.
		 */
		atomic_and_32(&node->tn_gen, ~TMPFS_NODE_GEN_MASK);
		*ap->a_recycle = true;
	} else {
		*ap->a_recycle = false;
	}
	VOP_UNLOCK(vp);

	return 0;
}

int
tmpfs_reclaim(void *v)
{
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	vnode_t *vp = ap->a_vp;
	tmpfs_mount_t *tmp = VFS_TO_TMPFS(vp->v_mount);
	tmpfs_node_t *node = VP_TO_TMPFS_NODE(vp);

	/* Disassociate inode from vnode. */
	node->tn_vnode = NULL;
	vcache_remove(vp->v_mount, &node, sizeof(node));
	vp->v_data = NULL;

	/* If inode is not referenced, i.e. no links, then destroy it. */
	if (node->tn_links == 0)
		tmpfs_free_node(tmp, node);
	return 0;
}

int
tmpfs_pathconf(void *v)
{
	struct vop_pathconf_args /* {
		struct vnode	*a_vp;
		int		a_name;
		register_t	*a_retval;
	} */ *ap = v;
	const int name = ap->a_name;
	register_t *retval = ap->a_retval;
	int error = 0;

	switch (name) {
	case _PC_LINK_MAX:
		*retval = LINK_MAX;
		break;
	case _PC_NAME_MAX:
		*retval = TMPFS_MAXNAMLEN;
		break;
	case _PC_PATH_MAX:
		*retval = PATH_MAX;
		break;
	case _PC_PIPE_BUF:
		*retval = PIPE_BUF;
		break;
	case _PC_CHOWN_RESTRICTED:
		*retval = 1;
		break;
	case _PC_NO_TRUNC:
		*retval = 1;
		break;
	case _PC_SYNC_IO:
		*retval = 1;
		break;
	case _PC_FILESIZEBITS:
		*retval = sizeof(off_t) * CHAR_BIT;
		break;
	default:
		error = EINVAL;
	}
	return error;
}

int
tmpfs_advlock(void *v)
{
	struct vop_advlock_args /* {
		struct vnode	*a_vp;
		void *		a_id;
		int		a_op;
		struct flock	*a_fl;
		int		a_flags;
	} */ *ap = v;
	vnode_t *vp = ap->a_vp;
	tmpfs_node_t *node = VP_TO_TMPFS_NODE(vp);

	return lf_advlock(v, &node->tn_lockf, node->tn_size);
}

int
tmpfs_getpages(void *v)
{
	struct vop_getpages_args /* {
		struct vnode *a_vp;
		voff_t a_offset;
		struct vm_page **a_m;
		int *a_count;
		int a_centeridx;
		vm_prot_t a_access_type;
		int a_advice;
		int a_flags;
	} */ * const ap = v;
	vnode_t *vp = ap->a_vp;
	const voff_t offset = ap->a_offset;
	struct vm_page **pgs = ap->a_m;
	const int centeridx = ap->a_centeridx;
	const vm_prot_t access_type = ap->a_access_type;
	const int advice = ap->a_advice;
	const int flags = ap->a_flags;
	int error, npages = *ap->a_count;
	tmpfs_node_t *node;
	struct uvm_object *uobj;

	KASSERT(vp->v_type == VREG);
	KASSERT(mutex_owned(vp->v_interlock));

	node = VP_TO_TMPFS_NODE(vp);
	uobj = node->tn_spec.tn_reg.tn_aobj;

	/*
	 * Currently, PGO_PASTEOF is not supported.
	 */
	if (vp->v_size <= offset + (centeridx << PAGE_SHIFT)) {
		if ((flags & PGO_LOCKED) == 0)
			mutex_exit(vp->v_interlock);
		return EINVAL;
	}

	if (vp->v_size < offset + (npages << PAGE_SHIFT)) {
		npages = (round_page(vp->v_size) - offset) >> PAGE_SHIFT;
	}

	if ((flags & PGO_LOCKED) != 0)
		return EBUSY;

	if ((flags & PGO_NOTIMESTAMP) == 0) {
		u_int tflags = 0;

		if ((vp->v_mount->mnt_flag & MNT_NOATIME) == 0)
			tflags |= TMPFS_UPDATE_ATIME;

		if ((access_type & VM_PROT_WRITE) != 0) {
			tflags |= TMPFS_UPDATE_MTIME;
			if (vp->v_mount->mnt_flag & MNT_RELATIME)
				tflags |= TMPFS_UPDATE_ATIME;
		}
		tmpfs_update(vp, tflags);
	}

	/*
	 * Invoke the pager.
	 *
	 * Clean the array of pages before.  XXX: PR/32166
	 * Note that vnode lock is shared with underlying UVM object.
	 */
	if (pgs) {
		memset(pgs, 0, sizeof(struct vm_pages *) * npages);
	}
	KASSERT(vp->v_interlock == uobj->vmobjlock);

	error = (*uobj->pgops->pgo_get)(uobj, offset, pgs, &npages, centeridx,
	    access_type, advice, flags | PGO_ALLPAGES);

#if defined(DEBUG)
	if (!error && pgs) {
		for (int i = 0; i < npages; i++) {
			KASSERT(pgs[i] != NULL);
		}
	}
#endif
	return error;
}

int
tmpfs_putpages(void *v)
{
	struct vop_putpages_args /* {
		struct vnode *a_vp;
		voff_t a_offlo;
		voff_t a_offhi;
		int a_flags;
	} */ * const ap = v;
	vnode_t *vp = ap->a_vp;
	const voff_t offlo = ap->a_offlo;
	const voff_t offhi = ap->a_offhi;
	const int flags = ap->a_flags;
	tmpfs_node_t *node;
	struct uvm_object *uobj;
	int error;

	KASSERT(mutex_owned(vp->v_interlock));

	if (vp->v_type != VREG) {
		mutex_exit(vp->v_interlock);
		return 0;
	}

	node = VP_TO_TMPFS_NODE(vp);
	uobj = node->tn_spec.tn_reg.tn_aobj;

	KASSERT(vp->v_interlock == uobj->vmobjlock);
	error = (*uobj->pgops->pgo_put)(uobj, offlo, offhi, flags);

	/* XXX mtime */

	return error;
}

int
tmpfs_whiteout(void *v)
{
	struct vop_whiteout_args /* {
		struct vnode		*a_dvp;
		struct componentname	*a_cnp;
		int			a_flags;
	} */ *ap = v;
	vnode_t *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	const int flags = ap->a_flags;
	tmpfs_mount_t *tmp = VFS_TO_TMPFS(dvp->v_mount);
	tmpfs_node_t *dnode = VP_TO_TMPFS_DIR(dvp);
	tmpfs_dirent_t *de;
	int error;

	switch (flags) {
	case LOOKUP:
		break;
	case CREATE:
		error = tmpfs_alloc_dirent(tmp, cnp->cn_nameptr,
		    cnp->cn_namelen, &de);
		if (error)
			return error;
		tmpfs_dir_attach(dnode, de, TMPFS_NODE_WHITEOUT);
		break;
	case DELETE:
		cnp->cn_flags &= ~DOWHITEOUT; /* when in doubt, cargo cult */
		de = tmpfs_dir_lookup(dnode, cnp);
		if (de == NULL)
			return ENOENT;
		tmpfs_dir_detach(dnode, de);
		tmpfs_free_dirent(tmp, de);
		break;
	}
	tmpfs_update(dvp, TMPFS_UPDATE_MTIME | TMPFS_UPDATE_CTIME);
	return 0;
}

int
tmpfs_print(void *v)
{
	struct vop_print_args /* {
		struct vnode	*a_vp;
	} */ *ap = v;
	vnode_t *vp = ap->a_vp;
	tmpfs_node_t *node = VP_TO_TMPFS_NODE(vp);

	printf("tag VT_TMPFS, tmpfs_node %p, flags 0x%x, links %d\n"
	    "\tmode 0%o, owner %d, group %d, size %" PRIdMAX,
	    node, node->tn_flags, node->tn_links, node->tn_mode, node->tn_uid,
	    node->tn_gid, (uintmax_t)node->tn_size);
	if (vp->v_type == VFIFO) {
		VOCALL(fifo_vnodeop_p, VOFFSET(vop_print), v);
	}
	printf("\n");
	return 0;
}
