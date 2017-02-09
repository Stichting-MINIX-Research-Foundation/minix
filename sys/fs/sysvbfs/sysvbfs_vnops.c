/*	$NetBSD: sysvbfs_vnops.c,v 1.58 2015/04/04 13:28:36 riastradh Exp $	*/

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by UCHIYAMA Yasushi.
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
__KERNEL_RCSID(0, "$NetBSD: sysvbfs_vnops.c,v 1.58 2015/04/04 13:28:36 riastradh Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/resource.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/dirent.h>
#include <sys/malloc.h>
#include <sys/lockf.h>
#include <sys/unistd.h>
#include <sys/fcntl.h>
#include <sys/kauth.h>
#include <sys/buf.h>

#include <miscfs/genfs/genfs.h>

#include <fs/sysvbfs/sysvbfs.h>
#include <fs/sysvbfs/bfs.h>

#ifdef SYSVBFS_VNOPS_DEBUG
#define	DPRINTF(fmt, args...)	printf(fmt, ##args)
#else
#define	DPRINTF(arg...)		((void)0)
#endif
#define	ROUND_SECTOR(x)		(((x) + 511) & ~511)

MALLOC_JUSTDEFINE(M_SYSVBFS_VNODE, "sysvbfs vnode", "sysvbfs vnode structures");
MALLOC_DECLARE(M_BFS);

static void sysvbfs_file_setsize(struct vnode *, size_t);

int
sysvbfs_lookup(void *arg)
{
	struct vop_lookup_v2_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *a = arg;
	struct vnode *v = a->a_dvp;
	struct sysvbfs_node *bnode = v->v_data;
	struct bfs *bfs = bnode->bmp->bfs;	/* my filesystem */
	struct vnode *vpp = NULL;
	struct bfs_dirent *dirent = NULL;
	struct componentname *cnp = a->a_cnp;
	int nameiop = cnp->cn_nameiop;
	const char *name = cnp->cn_nameptr;
	int namelen = cnp->cn_namelen;
	int error;

	DPRINTF("%s: %s op=%d %d\n", __func__, name, nameiop,
	    cnp->cn_flags);

	*a->a_vpp = NULL;

	KASSERT((cnp->cn_flags & ISDOTDOT) == 0);

	if ((error = VOP_ACCESS(a->a_dvp, VEXEC, cnp->cn_cred)) != 0) {
		return error;	/* directory permission. */
	}

	/* Deny last component write operation on a read-only mount */
	if ((cnp->cn_flags & ISLASTCN) && (v->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return EROFS;

	if (namelen == 1 && name[0] == '.') {	/* "." */
		vref(v);
		*a->a_vpp = v;
	} else {				/* Regular file */
		if (!bfs_dirent_lookup_by_name(bfs, cnp->cn_nameptr,
		    &dirent)) {
			if (nameiop != CREATE && nameiop != RENAME) {
				DPRINTF("%s: no such a file. (1)\n",
				    __func__);
				return ENOENT;
			}
			if ((error = VOP_ACCESS(v, VWRITE, cnp->cn_cred)) != 0)
				return error;
			return EJUSTRETURN;
		}

		/* Allocate v-node */
		if ((error = sysvbfs_vget(v->v_mount, dirent->inode, &vpp)) != 0) {
			DPRINTF("%s: can't get vnode.\n", __func__);
			return error;
		}
		VOP_UNLOCK(vpp);
		*a->a_vpp = vpp;
	}

	return 0;
}

int
sysvbfs_create(void *arg)
{
	struct vop_create_v3_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *a = arg;
	struct sysvbfs_node *bnode = a->a_dvp->v_data;
	struct sysvbfs_mount *bmp = bnode->bmp;
	struct bfs *bfs = bmp->bfs;
	struct mount *mp = bmp->mountp;
	struct bfs_dirent *dirent;
	struct bfs_fileattr attr;
	struct vattr *va = a->a_vap;
	kauth_cred_t cr = a->a_cnp->cn_cred;
	int err = 0;

	DPRINTF("%s: %s\n", __func__, a->a_cnp->cn_nameptr);
	KDASSERT(a->a_vap->va_type == VREG);
	attr.uid = kauth_cred_geteuid(cr);
	attr.gid = kauth_cred_getegid(cr);
	attr.mode = va->va_mode;

	if ((err = bfs_file_create(bfs, a->a_cnp->cn_nameptr, 0, 0, &attr))
	    != 0) {
		DPRINTF("%s: bfs_file_create failed.\n", __func__);
		return err;
	}

	if (!bfs_dirent_lookup_by_name(bfs, a->a_cnp->cn_nameptr, &dirent))
		panic("no dirent for created file.");

	if ((err = sysvbfs_vget(mp, dirent->inode, a->a_vpp)) != 0) {
		DPRINTF("%s: sysvbfs_vget failed.\n", __func__);
		return err;
	}
	bnode = (*a->a_vpp)->v_data;
	bnode->update_ctime = true;
	bnode->update_mtime = true;
	bnode->update_atime = true;
	VOP_UNLOCK(*a->a_vpp);

	return err;
}

int
sysvbfs_open(void *arg)
{
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		kauth_cred_t a_cred;
	} */ *a = arg;
	struct vnode *v = a->a_vp;
	struct sysvbfs_node *bnode = v->v_data;
	struct bfs_inode *inode = bnode->inode;

	DPRINTF("%s:\n", __func__);
	KDASSERT(v->v_type == VREG || v->v_type == VDIR);

	bnode->update_atime = true;
	if ((a->a_mode & FWRITE) && !(a->a_mode & O_APPEND)) {
		bnode->size = 0;
	} else {
		bnode->size = bfs_file_size(inode);
	}
	bnode->data_block = inode->start_sector;

	return 0;
}

int
sysvbfs_close(void *arg)
{
	struct vop_close_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		int  a_fflag;
		kauth_cred_t a_cred;
	} */ *a = arg;
	struct vnode *v = a->a_vp;
	struct sysvbfs_node *bnode = v->v_data;
	struct bfs_fileattr attr;

	DPRINTF("%s:\n", __func__);

	if (v->v_mount->mnt_flag & MNT_RDONLY)
		goto out;

	uvm_vnp_setsize(v, bnode->size);

	memset(&attr, 0xff, sizeof attr);	/* Set VNOVAL all */
	if (bnode->update_atime)
		attr.atime = time_second;
	if (bnode->update_ctime)
		attr.ctime = time_second;
	if (bnode->update_mtime)
		attr.mtime = time_second;
	bfs_inode_set_attr(bnode->bmp->bfs, bnode->inode, &attr);

	VOP_FSYNC(a->a_vp, a->a_cred, FSYNC_WAIT, 0, 0);

 out:
	return 0;
}

static int
sysvbfs_check_possible(struct vnode *vp, struct sysvbfs_node *bnode,
    mode_t mode)
{

	if ((mode & VWRITE) && (vp->v_mount->mnt_flag & MNT_RDONLY))
		return EROFS;

	return 0;
}

static int
sysvbfs_check_permitted(struct vnode *vp, struct sysvbfs_node *bnode,
    mode_t mode, kauth_cred_t cred)
{
	struct bfs_fileattr *attr = &bnode->inode->attr;

	return kauth_authorize_vnode(cred, KAUTH_ACCESS_ACTION(mode,
	    vp->v_type, attr->mode), vp, NULL, genfs_can_access(vp->v_type,
	    attr->mode, attr->uid, attr->gid, mode, cred));
}

int
sysvbfs_access(void *arg)
{
	struct vop_access_args /* {
		struct vnode	*a_vp;
		int		a_mode;
		kauth_cred_t	a_cred;
	} */ *ap = arg;
	struct vnode *vp = ap->a_vp;
	struct sysvbfs_node *bnode = vp->v_data;
	int error;

	DPRINTF("%s:\n", __func__);

	error = sysvbfs_check_possible(vp, bnode, ap->a_mode);
	if (error)
		return error;

	error = sysvbfs_check_permitted(vp, bnode, ap->a_mode, ap->a_cred);

	return error;
}

int
sysvbfs_getattr(void *v)
{
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct sysvbfs_node *bnode = vp->v_data;
	struct bfs_inode *inode = bnode->inode;
	struct bfs_fileattr *attr = &inode->attr;
	struct sysvbfs_mount *bmp = bnode->bmp;
	struct vattr *vap = ap->a_vap;

	DPRINTF("%s:\n", __func__);

	vap->va_type = vp->v_type;
	vap->va_mode = attr->mode;
	vap->va_nlink = attr->nlink;
	vap->va_uid = attr->uid;
	vap->va_gid = attr->gid;
	vap->va_fsid = bmp->devvp->v_rdev;
	vap->va_fileid = inode->number;
	vap->va_size = bfs_file_size(inode);
	vap->va_blocksize = BFS_BSIZE;
	vap->va_atime.tv_sec = attr->atime;
	vap->va_mtime.tv_sec = attr->mtime;
	vap->va_ctime.tv_sec = attr->ctime;
	vap->va_birthtime.tv_sec = 0;
	vap->va_gen = 1;
	vap->va_flags = 0;
	vap->va_rdev = 0;	/* No device file */
	vap->va_bytes = vap->va_size;
	vap->va_filerev = 0;
	vap->va_vaflags = 0;

	return 0;
}

int
sysvbfs_setattr(void *arg)
{
	struct vop_setattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
		struct proc *p;
	} */ *ap = arg;
	struct vnode *vp = ap->a_vp;
	struct vattr *vap = ap->a_vap;
	struct sysvbfs_node *bnode = vp->v_data;
	struct bfs_inode *inode = bnode->inode;
	struct bfs_fileattr *attr = &inode->attr;
	struct bfs *bfs = bnode->bmp->bfs;
	kauth_cred_t cred = ap->a_cred;
	int error;

	DPRINTF("%s:\n", __func__);
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return EROFS;

	if ((vap->va_type != VNON) || (vap->va_nlink != VNOVAL) ||
	    (vap->va_fsid != VNOVAL) || (vap->va_fileid != VNOVAL) ||
	    (vap->va_blocksize != VNOVAL) || (vap->va_rdev != VNOVAL) ||
	    ((int)vap->va_bytes != VNOVAL) || (vap->va_gen != VNOVAL))
		return EINVAL;

	if (vap->va_flags != VNOVAL)
		return EOPNOTSUPP;

	if (vap->va_uid != (uid_t)VNOVAL || vap->va_gid != (gid_t)VNOVAL) {
		uid_t uid =
		    (vap->va_uid != (uid_t)VNOVAL) ? vap->va_uid : attr->uid;
		gid_t gid =
		    (vap->va_gid != (gid_t)VNOVAL) ? vap->va_gid : attr->gid;
		error = kauth_authorize_vnode(cred,
		    KAUTH_VNODE_CHANGE_OWNERSHIP, vp, NULL,
		    genfs_can_chown(cred, attr->uid, attr->gid, uid, gid));
		if (error)
			return error;
		attr->uid = uid;
		attr->gid = gid;
	}

	if (vap->va_size != VNOVAL)
		switch (vp->v_type) {
		case VDIR:
			return EISDIR;
		case VCHR:
		case VBLK:
		case VFIFO:
			break;
		case VREG:
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return EROFS;
			sysvbfs_file_setsize(vp, vap->va_size);
			break;
		default:
			return EOPNOTSUPP;
		}

	if (vap->va_mode != (mode_t)VNOVAL) {
		mode_t mode = vap->va_mode;
		error = kauth_authorize_vnode(cred, KAUTH_VNODE_WRITE_SECURITY,
		    vp, NULL, genfs_can_chmod(vp->v_type, cred, attr->uid,
		    attr->gid, mode));
		if (error)
			return error;
		attr->mode = mode;
	}

	if ((vap->va_atime.tv_sec != VNOVAL) ||
	    (vap->va_mtime.tv_sec != VNOVAL) ||
	    (vap->va_ctime.tv_sec != VNOVAL)) {
		error = kauth_authorize_vnode(cred, KAUTH_VNODE_WRITE_TIMES, vp,
		    NULL, genfs_can_chtimes(vp, vap->va_vaflags, attr->uid,
		    cred));
		if (error)
			return error;

		if (vap->va_atime.tv_sec != VNOVAL)
			attr->atime = vap->va_atime.tv_sec;
		if (vap->va_mtime.tv_sec != VNOVAL)
			attr->mtime = vap->va_mtime.tv_sec;
		if (vap->va_ctime.tv_sec != VNOVAL)
			attr->ctime = vap->va_ctime.tv_sec;
	}

	bfs_inode_set_attr(bfs, inode, attr);

	return 0;
}

int
sysvbfs_read(void *arg)
{
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		kauth_cred_t a_cred;
	} */ *a = arg;
	struct vnode *v = a->a_vp;
	struct uio *uio = a->a_uio;
	struct sysvbfs_node *bnode = v->v_data;
	struct bfs_inode *inode = bnode->inode;
	vsize_t sz, filesz = bfs_file_size(inode);
	int err, uerr;
	const int advice = IO_ADV_DECODE(a->a_ioflag);

	DPRINTF("%s: type=%d\n", __func__, v->v_type);
	switch (v->v_type) {
	case VREG:
		break;
	case VDIR:
		return EISDIR;
	default:
		return EINVAL;
	}

	err = 0;
	while (uio->uio_resid > 0) {
		if ((sz = MIN(filesz - uio->uio_offset, uio->uio_resid)) == 0)
			break;

		err = ubc_uiomove(&v->v_uobj, uio, sz, advice,
		    UBC_READ | UBC_PARTIALOK | UBC_UNMAP_FLAG(v));
		if (err)
			break;
		DPRINTF("%s: read %ldbyte\n", __func__, sz);
	}

	uerr = sysvbfs_update(v, NULL, NULL, UPDATE_WAIT);
	if (err == 0)
		err = uerr;

	return err;
}

int
sysvbfs_write(void *arg)
{
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		kauth_cred_t a_cred;
	} */ *a = arg;
	struct vnode *v = a->a_vp;
	struct uio *uio = a->a_uio;
	int advice = IO_ADV_DECODE(a->a_ioflag);
	struct sysvbfs_node *bnode = v->v_data;
	bool extended = false;
	vsize_t sz;
	int err = 0;

	if (a->a_vp->v_type != VREG)
		return EISDIR;

	if (a->a_ioflag & IO_APPEND)
		uio->uio_offset = bnode->size;

	if (uio->uio_resid == 0)
		return 0;

	if (bnode->size < uio->uio_offset + uio->uio_resid) {
		sysvbfs_file_setsize(v, uio->uio_offset + uio->uio_resid);
		extended = true;
	}

	while (uio->uio_resid > 0) {
		sz = uio->uio_resid;
		err = ubc_uiomove(&v->v_uobj, uio, sz, advice,
		    UBC_WRITE | UBC_UNMAP_FLAG(v));
		if (err)
			break;
		DPRINTF("%s: write %ldbyte\n", __func__, sz);
	}
	if (err)
		sysvbfs_file_setsize(v, bnode->size - uio->uio_resid);

	VN_KNOTE(v, NOTE_WRITE | (extended ? NOTE_EXTEND : 0));

	return err;
}

int
sysvbfs_remove(void *arg)
{
	struct vop_remove_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode * a_dvp;
		struct vnode * a_vp;
		struct componentname * a_cnp;
	} */ *ap = arg;
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct sysvbfs_node *bnode = vp->v_data;
	struct sysvbfs_mount *bmp = bnode->bmp;
	struct bfs *bfs = bmp->bfs;
	int err;

	DPRINTF("%s: delete %s\n", __func__, ap->a_cnp->cn_nameptr);

	if (vp->v_type == VDIR)
		return EPERM;

	if ((err = bfs_file_delete(bfs, ap->a_cnp->cn_nameptr, true)) != 0)
		DPRINTF("%s: bfs_file_delete failed.\n", __func__);

	VN_KNOTE(ap->a_vp, NOTE_DELETE);
	VN_KNOTE(ap->a_dvp, NOTE_WRITE);
	if (dvp == vp)
		vrele(vp);
	else
		vput(vp);
	vput(dvp);

	if (err == 0) {
		bnode->removed = 1;
	}

	return err;
}

int
sysvbfs_rename(void *arg)
{
	struct vop_rename_args /* {
		struct vnode *a_fdvp;	from parent-directory v-node
		struct vnode *a_fvp;	from file v-node
		struct componentname *a_fcnp;
		struct vnode *a_tdvp;	to parent-directory
		struct vnode *a_tvp;	to file v-node
		struct componentname *a_tcnp;
	} */ *ap = arg;
	struct vnode *fvp = ap->a_fvp;
	struct vnode *fdvp = ap->a_fdvp;
	struct vnode *tvp = ap->a_tvp;
	struct vnode *tdvp = ap->a_tdvp;
	struct sysvbfs_node *bnode = fvp->v_data;
	struct bfs *bfs = bnode->bmp->bfs;
	const char *from_name = ap->a_fcnp->cn_nameptr;
	const char *to_name = ap->a_tcnp->cn_nameptr;
	int error;

	DPRINTF("%s: %s->%s\n", __func__, from_name, to_name);
	if ((fvp->v_mount != tdvp->v_mount) ||
	    (tvp && (fvp->v_mount != tvp->v_mount))) {
		error = EXDEV;
		printf("cross-device link\n");
		goto out;
	}

	KDASSERT(fvp->v_type == VREG);
	KDASSERT(tvp == NULL ? true : tvp->v_type == VREG);
	KASSERT(tdvp == fdvp);

	/*
	 * Make sure the source hasn't been removed between lookup
	 * and target directory lock.
	 */
	if (bnode->removed) {
		error = ENOENT;
		goto out;
	}

	/*
	 * Remove the target if it exists.
	 */
	if (tvp != NULL) {
		error = bfs_file_delete(bfs, to_name, true);
		if (error)
			goto out;
	}
	error = bfs_file_rename(bfs, from_name, to_name);
 out:
	/* tdvp == tvp probably can't happen with this fs, but safety first */
	if (tdvp == tvp)
		vrele(tdvp);
	else
		vput(tdvp);
	if (tvp)
		vput(tvp);

	vrele(fdvp);
	vrele(fvp);

	return 0;
}

int
sysvbfs_readdir(void *v)
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
	struct vnode *vp = ap->a_vp;
	struct sysvbfs_node *bnode = vp->v_data;
	struct bfs *bfs = bnode->bmp->bfs;
	struct dirent *dp;
	struct bfs_dirent *file;
	int i, n, error;

	DPRINTF("%s: offset=%" PRId64 " residue=%zu\n", __func__,
	    uio->uio_offset, uio->uio_resid);

	KDASSERT(vp->v_type == VDIR);
	KDASSERT(uio->uio_offset >= 0);

	dp = malloc(sizeof(struct dirent), M_BFS, M_WAITOK | M_ZERO);

	i = uio->uio_offset / sizeof(struct dirent);
	n = uio->uio_resid / sizeof(struct dirent);
	if ((i + n) > bfs->n_dirent)
		n = bfs->n_dirent - i;

	for (file = &bfs->dirent[i]; i < n; file++) {
		if (file->inode == 0)
			continue;
		if (i == bfs->max_dirent) {
			DPRINTF("%s: file system inconsistent.\n",
			    __func__);
			break;
		}
		i++;
		memset(dp, 0, sizeof(struct dirent));
		dp->d_fileno = file->inode;
		dp->d_type = file->inode == BFS_ROOT_INODE ? DT_DIR : DT_REG;
		dp->d_namlen = strlen(file->name);
		strncpy(dp->d_name, file->name, BFS_FILENAME_MAXLEN);
		dp->d_reclen = sizeof(struct dirent);
		if ((error = uiomove(dp, dp->d_reclen, uio)) != 0) {
			DPRINTF("%s: uiomove failed.\n", __func__);
			free(dp, M_BFS);
			return error;
		}
	}
	DPRINTF("%s: %d %d %d\n", __func__, i, n, bfs->n_dirent);
	*ap->a_eofflag = (i == bfs->n_dirent);

	free(dp, M_BFS);
	return 0;
}

int
sysvbfs_inactive(void *arg)
{
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		bool *a_recycle;
	} */ *a = arg;
	struct vnode *v = a->a_vp;
	struct sysvbfs_node *bnode = v->v_data;

	DPRINTF("%s:\n", __func__);
	if (bnode->removed)
		*a->a_recycle = true;
	else
		*a->a_recycle = false;
	VOP_UNLOCK(v);

	return 0;
}

int
sysvbfs_reclaim(void *v)
{
	extern struct pool sysvbfs_node_pool;
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct sysvbfs_node *bnode = vp->v_data;
	struct bfs *bfs = bnode->bmp->bfs;

	DPRINTF("%s:\n", __func__);

	vcache_remove(vp->v_mount,
	    &bnode->inode->number, sizeof(bnode->inode->number));
	if (bnode->removed) {
		if (bfs_inode_delete(bfs, bnode->inode->number) != 0)
			DPRINTF("%s: delete inode failed\n", __func__);
	}
	genfs_node_destroy(vp);
	pool_put(&sysvbfs_node_pool, bnode);
	vp->v_data = NULL;

	return 0;
}

int
sysvbfs_bmap(void *arg)
{
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
	} */ *a = arg;
	struct vnode *v = a->a_vp;
	struct sysvbfs_node *bnode = v->v_data;
	struct sysvbfs_mount *bmp = bnode->bmp;
	struct bfs_inode *inode = bnode->inode;
	daddr_t blk;

	DPRINTF("%s:\n", __func__);
	/* BFS algorithm is contiguous allocation */
	blk = inode->start_sector + a->a_bn;

	if (blk * BFS_BSIZE > bmp->bfs->data_end)
		return ENOSPC;

	*a->a_vpp = bmp->devvp;
	*a->a_runp = 0;
	DPRINTF("%s: %d + %" PRId64 "\n", __func__, inode->start_sector,
	    a->a_bn);

	*a->a_bnp = blk;


	return 0;
}

int
sysvbfs_strategy(void *arg)
{
	struct vop_strategy_args /* {
		struct vnode *a_vp;
		struct buf *a_bp;
	} */ *a = arg;
	struct buf *b = a->a_bp;
	struct vnode *v = a->a_vp;
	struct sysvbfs_node *bnode = v->v_data;
	struct sysvbfs_mount *bmp = bnode->bmp;
	int error;

	DPRINTF("%s:\n", __func__);
	KDASSERT(v->v_type == VREG);
	if (b->b_blkno == b->b_lblkno) {
		error = VOP_BMAP(v, b->b_lblkno, NULL, &b->b_blkno, NULL);
		if (error) {
			b->b_error = error;
			biodone(b);
			return error;
		}
		if ((long)b->b_blkno == -1)
			clrbuf(b);
	}
	if ((long)b->b_blkno == -1) {
		biodone(b);
		return 0;
	}

	return VOP_STRATEGY(bmp->devvp, b);
}

int
sysvbfs_print(void *v)
{
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct sysvbfs_node *bnode = ap->a_vp->v_data;

	DPRINTF("%s:\n", __func__);
	bfs_dump(bnode->bmp->bfs);

	return 0;
}

int
sysvbfs_advlock(void *v)
{
	struct vop_advlock_args /* {
		struct vnode *a_vp;
		void *a_id;
		int a_op;
		struct flock *a_fl;
		int a_flags;
	} */ *ap = v;
	struct sysvbfs_node *bnode = ap->a_vp->v_data;

	DPRINTF("%s: op=%d\n", __func__, ap->a_op);

	return lf_advlock(ap, &bnode->lockf, bfs_file_size(bnode->inode));
}

int
sysvbfs_pathconf(void *v)
{
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		register_t *a_retval;
	} */ *ap = v;
	int err = 0;

	DPRINTF("%s:\n", __func__);

	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = 1;
		break;
	case _PC_NAME_MAX:
		*ap->a_retval = BFS_FILENAME_MAXLEN;
		break;
	case _PC_PATH_MAX:
		*ap->a_retval = BFS_FILENAME_MAXLEN;
		break;
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		break;
	case _PC_NO_TRUNC:
		*ap->a_retval = 0;
		break;
	case _PC_SYNC_IO:
		*ap->a_retval = 1;
		break;
	case _PC_FILESIZEBITS:
		*ap->a_retval = 32;
		break;
	default:
		err = EINVAL;
		break;
	}

	return err;
}

int
sysvbfs_fsync(void *v)
{
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		kauth_cred_t a_cred;
		int a_flags;
		off_t offlo;
		off_t offhi;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	int error, wait;

	if (ap->a_flags & FSYNC_CACHE) {
		return EOPNOTSUPP;
	}

	wait = (ap->a_flags & FSYNC_WAIT) != 0;
	error = vflushbuf(vp, ap->a_flags);
	if (error == 0 && (ap->a_flags & FSYNC_DATAONLY) == 0)
		error = sysvbfs_update(vp, NULL, NULL, wait ? UPDATE_WAIT : 0);

	return error;
}

int
sysvbfs_update(struct vnode *vp, const struct timespec *acc,
    const struct timespec *mod, int flags)
{
	struct sysvbfs_node *bnode = vp->v_data;
	struct bfs_fileattr attr;

	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return 0;

	DPRINTF("%s:\n", __func__);
	memset(&attr, 0xff, sizeof attr);	/* Set VNOVAL all */
	if (bnode->update_atime) {
		attr.atime = acc ? acc->tv_sec : time_second;
		bnode->update_atime = false;
	}
	if (bnode->update_ctime) {
		attr.ctime = time_second;
		bnode->update_ctime = false;
	}
	if (bnode->update_mtime) {
		attr.mtime = mod ? mod->tv_sec : time_second;
		bnode->update_mtime = false;
	}
	bfs_inode_set_attr(bnode->bmp->bfs, bnode->inode, &attr);

	return 0;
}

static void
sysvbfs_file_setsize(struct vnode *v, size_t size)
{
	struct sysvbfs_node *bnode = v->v_data;
	struct bfs_inode *inode = bnode->inode;

	bnode->size = size;
	uvm_vnp_setsize(v, bnode->size);
	inode->end_sector = bnode->data_block +
	    (ROUND_SECTOR(bnode->size) >> DEV_BSHIFT) - 1;
	inode->eof_offset_byte = bnode->data_block * DEV_BSIZE +
	    bnode->size - 1;
	bnode->update_mtime = true;
}
