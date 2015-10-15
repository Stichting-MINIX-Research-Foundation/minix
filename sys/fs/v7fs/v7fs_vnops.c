/*	$NetBSD: v7fs_vnops.c,v 1.21 2015/04/20 23:03:08 riastradh Exp $	*/

/*-
 * Copyright (c) 2004, 2011 The NetBSD Foundation, Inc.
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
__KERNEL_RCSID(0, "$NetBSD: v7fs_vnops.c,v 1.21 2015/04/20 23:03:08 riastradh Exp $");
#if defined _KERNEL_OPT
#include "opt_v7fs.h"
#endif

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/resource.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/dirent.h>
#include <sys/kmem.h>
#include <sys/lockf.h>
#include <sys/unistd.h>
#include <sys/fcntl.h>
#include <sys/kauth.h>
#include <sys/buf.h>
#include <sys/stat.h>	/*APPEND */
#include <miscfs/genfs/genfs.h>

#include <fs/v7fs/v7fs.h>
#include <fs/v7fs/v7fs_impl.h>
#include <fs/v7fs/v7fs_inode.h>
#include <fs/v7fs/v7fs_dirent.h>
#include <fs/v7fs/v7fs_file.h>
#include <fs/v7fs/v7fs_datablock.h>
#include <fs/v7fs/v7fs_extern.h>

#ifdef V7FS_VNOPS_DEBUG
#define	DPRINTF(fmt, args...)	printf("%s: " fmt, __func__, ##args)
#else
#define	DPRINTF(arg...)		((void)0)
#endif

static v7fs_mode_t vtype_to_v7fs_mode(enum vtype);
static uint8_t v7fs_mode_to_d_type(v7fs_mode_t);

static v7fs_mode_t
vtype_to_v7fs_mode(enum vtype type)
{
	/* Convert Vnode types to V7FS types (sys/vnode.h)*/
	v7fs_mode_t table[] = { 0, V7FS_IFREG, V7FS_IFDIR, V7FS_IFBLK,
				V7FS_IFCHR, V7FSBSD_IFLNK, V7FSBSD_IFSOCK,
				V7FSBSD_IFFIFO };
	return table[type];
}

static uint8_t
v7fs_mode_to_d_type(v7fs_mode_t mode)
{
	/* Convert V7FS types to dirent d_type (sys/dirent.h)*/

	return (mode & V7FS_IFMT) >> 12;
}

int
v7fs_lookup(void *v)
{
	struct vop_lookup_v2_args /* {
				  struct vnode *a_dvp;
				  struct vnode **a_vpp;
				  struct componentname *a_cnp;
				  } */ *a = v;
	struct vnode *dvp = a->a_dvp;
	struct v7fs_node *parent_node = dvp->v_data;
	struct v7fs_inode *parent = &parent_node->inode;
	struct v7fs_self *fs = parent_node->v7fsmount->core;/* my filesystem */
	struct vnode *vpp;
	struct componentname *cnp = a->a_cnp;
	int nameiop = cnp->cn_nameiop;
	const char *name = cnp->cn_nameptr;
	int namelen = cnp->cn_namelen;
	int flags = cnp->cn_flags;
	bool isdotdot = flags & ISDOTDOT;
	bool islastcn = flags & ISLASTCN;
	v7fs_ino_t ino;
	int error;
#ifdef V7FS_VNOPS_DEBUG
	const char *opname[] = { "LOOKUP", "CREATE", "DELETE", "RENAME" };
#endif
	DPRINTF("'%s' op=%s flags=%d parent=%d %o %dbyte\n", name,
	    opname[nameiop], cnp->cn_flags, parent->inode_number, parent->mode,
	    parent->filesize);

	*a->a_vpp = 0;

	/* Check directory permission for search */
	if ((error = VOP_ACCESS(dvp, VEXEC, cnp->cn_cred))) {
		DPRINTF("***perm.\n");
		return error;
	}

	/* Deny last component write operation on a read-only mount */
	if (islastcn && (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (nameiop == DELETE || nameiop == RENAME)) {
		DPRINTF("***ROFS.\n");
		return EROFS;
	}

	/* No lookup on removed directory */
	if (v7fs_inode_nlink(parent) == 0)
		return ENOENT;

	/* "." */
	if (namelen == 1 && name[0] == '.') {
		if ((nameiop == RENAME) && islastcn) {
			return EISDIR; /* t_vnops rename_dir(3) */
		}
		vref(dvp); /* v_usecount++ */
		*a->a_vpp = dvp;
		DPRINTF("done.(.)\n");
		return 0;
	}

	/* ".." and reguler file. */
	if ((error = v7fs_file_lookup_by_name(fs, parent, name, &ino))) {
		/* Not found. Tell this entry be able to allocate. */
		if (((nameiop == CREATE) || (nameiop == RENAME)) && islastcn) {
			/* Check directory permission to allocate. */
			if ((error = VOP_ACCESS(dvp, VWRITE, cnp->cn_cred))) {
				DPRINTF("access denied. (%s)\n", name);
				return error;
			}
			DPRINTF("EJUSTRETURN op=%d (%s)\n", nameiop, name);
			return EJUSTRETURN;
		}
		DPRINTF("lastcn=%d\n", flags & ISLASTCN);
		return error;
	}

	if ((nameiop == DELETE) && islastcn) {
		if ((error = VOP_ACCESS(dvp, VWRITE, cnp->cn_cred))) {
			DPRINTF("access denied. (%s)\n", name);
			return error;
		}
	}

	/* Entry found. Allocate v-node */
	// Check permissions?
	vpp = 0;
	if (isdotdot) {
		VOP_UNLOCK(dvp); /* preserve reference count. (not vput) */
	}
	DPRINTF("enter vget\n");
	if ((error = v7fs_vget(dvp->v_mount, ino, &vpp))) {
		DPRINTF("***can't get vnode.\n");
		return error;
	}
	DPRINTF("exit vget\n");
	if (isdotdot) {
		vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY);
	}
	if (vpp != dvp)
		VOP_UNLOCK(vpp);
	*a->a_vpp = vpp;
	DPRINTF("done.(%s)\n", name);

	return 0;
}

int
v7fs_create(void *v)
{
	struct vop_create_v3_args /* {
				  struct vnode *a_dvp;
				  struct vnode **a_vpp;
				  struct componentname *a_cnp;
				  struct vattr *a_vap;
				  } */ *a = v;
	struct v7fs_node *parent_node = a->a_dvp->v_data;
	struct v7fs_mount *v7fsmount = parent_node->v7fsmount;
	struct v7fs_self *fs = v7fsmount->core;
	struct mount *mp = v7fsmount->mountp;
	struct v7fs_fileattr attr;
	struct vattr *va = a->a_vap;
	kauth_cred_t cr = a->a_cnp->cn_cred;
	v7fs_ino_t ino;
	int error = 0;

	DPRINTF("%s parent#%d\n", a->a_cnp->cn_nameptr,
	    parent_node->inode.inode_number);
	KDASSERT((va->va_type == VREG) || (va->va_type == VSOCK));

	memset(&attr, 0, sizeof(attr));
	attr.uid = kauth_cred_geteuid(cr);
	attr.gid = kauth_cred_getegid(cr);
	attr.mode = va->va_mode | vtype_to_v7fs_mode (va->va_type);
	attr.device = 0;

	/* Allocate disk entry. and register its entry to parent directory. */
	if ((error = v7fs_file_allocate(fs, &parent_node->inode,
		    a->a_cnp->cn_nameptr, &attr, &ino))) {
		DPRINTF("v7fs_file_allocate failed.\n");
		return error;
	}
	/* Sync dirent size change. */
	uvm_vnp_setsize(a->a_dvp, v7fs_inode_filesize(&parent_node->inode));

	/* Get myself vnode. */
	*a->a_vpp = 0;
	if ((error = v7fs_vget(mp, ino, a->a_vpp))) {
		DPRINTF("v7fs_vget failed.\n");
		return error;
	}

	/* Scheduling update time. real update by v7fs_update */
	struct v7fs_node *newnode = (*a->a_vpp)->v_data;
	newnode->update_ctime = true;
	newnode->update_mtime = true;
	newnode->update_atime = true;
	DPRINTF("allocated %s->#%d\n", a->a_cnp->cn_nameptr, ino);

	if (error == 0)
		VOP_UNLOCK(*a->a_vpp);

	return error;
}

int
v7fs_mknod(void *v)
{
	struct vop_mknod_v3_args /* {
				 struct vnode		*a_dvp;
				 struct vnode		**a_vpp;
				 struct componentname	*a_cnp;
				 struct vattr		*a_vap;
				 } */ *a = v;
	struct componentname *cnp = a->a_cnp;
	kauth_cred_t cr = cnp->cn_cred;
	struct vnode *dvp = a->a_dvp;
	struct vattr *va = a->a_vap;
	struct v7fs_node *parent_node = dvp->v_data;
	struct v7fs_mount *v7fsmount = parent_node->v7fsmount;
	struct v7fs_self *fs = v7fsmount->core;
	struct mount *mp = v7fsmount->mountp;
	struct v7fs_fileattr attr;

	v7fs_ino_t ino;
	int error = 0;

	DPRINTF("%s %06o %lx %d\n", cnp->cn_nameptr, va->va_mode,
	    (long)va->va_rdev, va->va_type);
	memset(&attr, 0, sizeof(attr));
	attr.uid = kauth_cred_geteuid(cr);
	attr.gid = kauth_cred_getegid(cr);
	attr.mode = va->va_mode | vtype_to_v7fs_mode(va->va_type);
	attr.device = va->va_rdev;

	if ((error = v7fs_file_allocate(fs, &parent_node->inode,
	    cnp->cn_nameptr, &attr, &ino)))
		return error;
	/* Sync dirent size change. */
	uvm_vnp_setsize(dvp, v7fs_inode_filesize(&parent_node->inode));

	if ((error = v7fs_vget(mp, ino, a->a_vpp))) {
		DPRINTF("can't get vnode.\n");
		return error;
	}
	struct v7fs_node *newnode = (*a->a_vpp)->v_data;
	newnode->update_ctime = true;
	newnode->update_mtime = true;
	newnode->update_atime = true;

	if (error == 0)
		VOP_UNLOCK(*a->a_vpp);

	return error;
}

int
v7fs_open(void *v)
{
	struct vop_open_args /* {
				struct vnode *a_vp;
				int  a_mode;
				kauth_cred_t a_cred;
				} */ *a = v;

	struct vnode *vp = a->a_vp;
	struct v7fs_node *v7node = vp->v_data;
	struct v7fs_inode *inode = &v7node->inode;

	DPRINTF("inode %d\n", inode->inode_number);
	/* Append mode file pointer is managed by kernel. */
	if (inode->append_mode &&
	    ((a->a_mode & (FWRITE | O_APPEND)) == FWRITE)) {
		DPRINTF("file is already opened by append mode.\n");
		return EPERM;
	}

	return 0;
}

int
v7fs_close(void *v)
{
	struct vop_close_args /* {
				 struct vnodeop_desc *a_desc;
				 struct vnode *a_vp;
				 int  a_fflag;
				 kauth_cred_t a_cred;
				 } */ *a = v;
	struct vnode *vp = a->a_vp;
#ifdef V7FS_VNOPS_DEBUG
	struct v7fs_node *v7node = vp->v_data;
	struct v7fs_inode *inode = &v7node->inode;
#endif
	DPRINTF("#%d (i)%dbyte (v)%zubyte\n", inode->inode_number,
	    v7fs_inode_filesize(inode), vp->v_size);

	/* Update timestamp */
	v7fs_update(vp, 0, 0, UPDATE_WAIT);

	return 0;
}

static int
v7fs_check_possible(struct vnode *vp, struct v7fs_node *v7node,
    mode_t mode)
{

	if (!(mode & VWRITE))
	  return 0;

	switch (vp->v_type) {
	default:
		/*  special file is always writable. */
		return 0;
	case VDIR:
	case VLNK:
	case VREG:
		break;
	}

	return vp->v_mount->mnt_flag & MNT_RDONLY ? EROFS : 0;
}

static int
v7fs_check_permitted(struct vnode *vp, struct v7fs_node *v7node,
    mode_t mode, kauth_cred_t cred)
{

	struct v7fs_inode *inode = &v7node->inode;

	return kauth_authorize_vnode(cred, KAUTH_ACCESS_ACTION(mode,
	    vp->v_type, inode->mode), vp, NULL, genfs_can_access(vp->v_type,
	    inode->mode, inode->uid, inode->gid, mode, cred));
}

int
v7fs_access(void *v)
{
	struct vop_access_args /* {
				  struct vnode	*a_vp;
				  int		a_mode;
				  kauth_cred_t	a_cred;
				  } */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct v7fs_node *v7node = vp->v_data;
	int error;

	error = v7fs_check_possible(vp, v7node, ap->a_mode);
	if (error)
		return error;

	error = v7fs_check_permitted(vp, v7node, ap->a_mode, ap->a_cred);

	return error;
}

int
v7fs_getattr(void *v)
{
	struct vop_getattr_args /* {
				   struct vnode *a_vp;
				   struct vattr *a_vap;
				   kauth_cred_t a_cred;
				   } */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct v7fs_node *v7node = vp->v_data;
	struct v7fs_inode *inode = &v7node->inode;
	struct v7fs_mount *v7fsmount = v7node->v7fsmount;
	struct vattr *vap = ap->a_vap;

	DPRINTF("\n");
	vap->va_type = vp->v_type;
	vap->va_mode = inode->mode;
	vap->va_nlink = inode->nlink;
	vap->va_uid = inode->uid;
	vap->va_gid = inode->gid;
	vap->va_fsid = v7fsmount->devvp->v_rdev;
	vap->va_fileid = inode->inode_number;
	vap->va_size = vp->v_size;
	if (vp->v_type == VLNK) {
		/* Ajust for trailing NUL. */
		KASSERT(vap->va_size > 0);
		vap->va_size -= 1;
	}
	vap->va_atime.tv_sec = inode->atime;
	vap->va_mtime.tv_sec = inode->mtime;
	vap->va_ctime.tv_sec = inode->ctime;
	vap->va_birthtime.tv_sec = 0;
	vap->va_gen = 1;
	vap->va_flags = inode->append_mode ? SF_APPEND : 0;
	vap->va_rdev = inode->device;
	vap->va_bytes = vap->va_size; /* No sparse support. */
	vap->va_filerev = 0;
	vap->va_vaflags = 0;
	/* PAGE_SIZE is larger than sizeof(struct dirent). OK.
	   getcwd_scandir()@vfs_getcwd.c */
	vap->va_blocksize = PAGE_SIZE;

	return 0;
}

int
v7fs_setattr(void *v)
{
	struct vop_setattr_args /* {
				   struct vnode *a_vp;
				   struct vattr *a_vap;
				   kauth_cred_t a_cred;
				   struct proc *p;
				   } */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct vattr *vap = ap->a_vap;
	struct v7fs_node *v7node = vp->v_data;
	struct v7fs_self *fs = v7node->v7fsmount->core;
	struct v7fs_inode *inode = &v7node->inode;
	kauth_cred_t cred = ap->a_cred;
	struct timespec *acc, *mod;
	int error = 0;
	acc = mod = NULL;

	DPRINTF("\n");

	if (vp->v_mount->mnt_flag & MNT_RDONLY) {
		switch (vp->v_type) {
		default:
			/*  special file is always writable. */
			break;
		case VDIR:
		case VLNK:
		case VREG:
			DPRINTF("read-only mount\n");
			return EROFS;
		}
	}

	if ((vap->va_type != VNON) || (vap->va_nlink != VNOVAL) ||
	    (vap->va_fsid != VNOVAL) || (vap->va_fileid != VNOVAL) ||
	    (vap->va_blocksize != VNOVAL) || (vap->va_rdev != VNOVAL) ||
	    ((int)vap->va_bytes != VNOVAL) || (vap->va_gen != VNOVAL)) {
		DPRINTF("invalid request\n");
		return EINVAL;
	}
	/* File pointer mode. */
	if (vap->va_flags != VNOVAL) {
		error = kauth_authorize_vnode(cred, KAUTH_VNODE_WRITE_FLAGS,
		    vp, NULL, genfs_can_chflags(cred, vp->v_type, inode->uid,
		    false));
		if (error)
			return error;
		inode->append_mode = vap->va_flags & SF_APPEND;
	}

	/* File size change. */
	if ((vap->va_size != VNOVAL) && (vp->v_type == VREG)) {
		error = v7fs_datablock_size_change(fs, vap->va_size, inode);
		if (error == 0)
			uvm_vnp_setsize(vp, vap->va_size);
	}
	uid_t uid = inode->uid;
	gid_t gid = inode->gid;

	if (vap->va_uid != (uid_t)VNOVAL) {
		uid = vap->va_uid;
		error = kauth_authorize_vnode(cred,
		    KAUTH_VNODE_CHANGE_OWNERSHIP, vp, NULL,
		    genfs_can_chown(cred, inode->uid, inode->gid, uid,
		    gid));
		if (error)
			return error;
		inode->uid = uid;
	}
	if (vap->va_gid != (uid_t)VNOVAL) {
		gid = vap->va_gid;
		error = kauth_authorize_vnode(cred,
		    KAUTH_VNODE_CHANGE_OWNERSHIP, vp, NULL,
		    genfs_can_chown(cred, inode->uid, inode->gid, uid,
		    gid));
		if (error)
			return error;
		inode->gid = gid;
	}
	if (vap->va_mode != (mode_t)VNOVAL) {
		mode_t mode = vap->va_mode;
		error = kauth_authorize_vnode(cred, KAUTH_VNODE_WRITE_SECURITY,
		    vp, NULL, genfs_can_chmod(vp->v_type, cred, inode->uid, inode->gid,
		    mode));
		if (error) {
			return error;
		}
		v7fs_inode_chmod(inode, mode);
	}
	if ((vap->va_atime.tv_sec != VNOVAL) ||
	    (vap->va_mtime.tv_sec != VNOVAL) ||
	    (vap->va_ctime.tv_sec != VNOVAL)) {
		error = kauth_authorize_vnode(cred, KAUTH_VNODE_WRITE_TIMES, vp,
		    NULL, genfs_can_chtimes(vp, vap->va_vaflags, inode->uid,
		    cred));
		if (error)
			return error;

		if (vap->va_atime.tv_sec != VNOVAL) {
			acc = &vap->va_atime;
		}
		if (vap->va_mtime.tv_sec != VNOVAL) {
			mod = &vap->va_mtime;
			v7node->update_mtime = true;
		}
		if (vap->va_ctime.tv_sec != VNOVAL) {
			v7node->update_ctime = true;
		}
	}

	v7node->update_atime = true;
	v7fs_update(vp, acc, mod, 0);

	return error;
}

int
v7fs_read(void *v)
{
	struct vop_read_args /* {
				struct vnode *a_vp;
				struct uio *a_uio;
				int a_ioflag;
				kauth_cred_t a_cred;
				} */ *a = v;
	struct vnode *vp = a->a_vp;
	struct uio *uio = a->a_uio;
	struct v7fs_node *v7node = vp->v_data;
	struct v7fs_inode *inode = &v7node->inode;
	vsize_t sz, filesz = v7fs_inode_filesize(inode);
	const int advice = IO_ADV_DECODE(a->a_ioflag);
	int error = 0;

	DPRINTF("type=%d inode=%d\n", vp->v_type, v7node->inode.inode_number);

	while (uio->uio_resid > 0) {
		if ((sz = MIN(filesz - uio->uio_offset, uio->uio_resid)) == 0)
			break;

		error = ubc_uiomove(&vp->v_uobj, uio, sz, advice, UBC_READ |
		    UBC_PARTIALOK | UBC_UNMAP_FLAG(v));
		if (error) {
			break;
		}
		DPRINTF("read %zubyte\n", sz);
	}
	v7node->update_atime = true;

	return error;
}

int
v7fs_write(void *v)
{
	struct vop_write_args /* {
				 struct vnode *a_vp;
				 struct uio *a_uio;
				 int  a_ioflag;
				 kauth_cred_t a_cred;
				 } */ *a = v;
	struct vnode *vp = a->a_vp;
	struct uio *uio = a->a_uio;
	int advice = IO_ADV_DECODE(a->a_ioflag);
	struct v7fs_node *v7node = vp->v_data;
	struct v7fs_inode *inode = &v7node->inode;
	struct v7fs_self *fs = v7node->v7fsmount->core;
	vsize_t sz;
	int error = 0;

	if (uio->uio_resid == 0)
		return 0;

	sz = v7fs_inode_filesize(inode);
	DPRINTF("(i)%ld (v)%zu ofs=%zu + res=%zu = %zu\n", sz, vp->v_size,
	    uio->uio_offset, uio->uio_resid, uio->uio_offset + uio->uio_resid);

	/* Append mode file offset is managed by kernel. */
	if (a->a_ioflag & IO_APPEND)
		uio->uio_offset = sz;

	/* If write region is over filesize, expand. */
	size_t newsize= uio->uio_offset + uio->uio_resid;
	ssize_t expand = newsize - sz;
 	if (expand > 0) {
		if ((error = v7fs_datablock_expand(fs, inode, expand)))
			return error;
		uvm_vnp_setsize(vp, newsize);
	}

	while (uio->uio_resid > 0) {
		sz = uio->uio_resid;
		if ((error = ubc_uiomove(&vp->v_uobj, uio, sz, advice,
			    UBC_WRITE | UBC_UNMAP_FLAG(v))))
			break;
		DPRINTF("write %zubyte\n", sz);
	}
	v7node->update_mtime = true;

	return error;
}

int
v7fs_fsync(void *v)
{
	struct vop_fsync_args /* {
				 struct vnode *a_vp;
				 kauth_cred_t a_cred;
				 int a_flags;
				 off_t offlo;
				 off_t offhi;
				 } */ *a = v;
	struct vnode *vp = a->a_vp;
	int error, wait;

	DPRINTF("%p\n", a->a_vp);
	if (a->a_flags & FSYNC_CACHE) {
		return EOPNOTSUPP;
	}

	wait = (a->a_flags & FSYNC_WAIT);
	error = vflushbuf(vp, a->a_flags);

	if (error == 0 && (a->a_flags & FSYNC_DATAONLY) == 0)
		error = v7fs_update(vp, NULL, NULL, wait ? UPDATE_WAIT : 0);

	return error;
}

int
v7fs_remove(void *v)
{
	struct vop_remove_args /* {
				  struct vnodeop_desc *a_desc;
				  struct vnode * a_dvp;
				  struct vnode * a_vp;
				  struct componentname * a_cnp;
				  } */ *a = v;
	struct v7fs_node *parent_node = a->a_dvp->v_data;
	struct v7fs_mount *v7fsmount = parent_node->v7fsmount;
	struct vnode *vp = a->a_vp;
	struct vnode *dvp = a->a_dvp;
	struct v7fs_inode *inode = &((struct v7fs_node *)vp->v_data)->inode;
	struct v7fs_self *fs = v7fsmount->core;
	int error = 0;

	DPRINTF("delete %s\n", a->a_cnp->cn_nameptr);

	if (vp->v_type == VDIR) {
		error = EPERM;
		goto out;
	}

	if ((error = v7fs_file_deallocate(fs, &parent_node->inode,
		    a->a_cnp->cn_nameptr))) {
		DPRINTF("v7fs_file_delete failed.\n");
		goto out;
	}
	error = v7fs_inode_load(fs, inode, inode->inode_number);
	if (error)
		goto out;
	/* Sync dirent size change. */
	uvm_vnp_setsize(dvp, v7fs_inode_filesize(&parent_node->inode));

out:
	if (dvp == vp)
		vrele(vp); /* v_usecount-- of unlocked vp */
	else
		vput(vp); /* unlock vp and then v_usecount-- */
	vput(dvp);

	return error;
}

int
v7fs_link(void *v)
{
	struct vop_link_v2_args /* {
				struct vnode *a_dvp;
				struct vnode *a_vp;
				struct componentname *a_cnp;
				} */ *a = v;
	struct vnode *dvp = a->a_dvp;
	struct vnode *vp = a->a_vp;
	struct v7fs_node *parent_node = dvp->v_data;
	struct v7fs_node *node = vp->v_data;
	struct v7fs_inode *parent = &parent_node->inode;
	struct v7fs_inode *p = &node->inode;
	struct v7fs_self *fs = node->v7fsmount->core;
	struct componentname *cnp = a->a_cnp;
	int error = 0;

	DPRINTF("%p\n", vp);
	/* Lock soruce file */
	if ((error = vn_lock(vp, LK_EXCLUSIVE))) {
		DPRINTF("lock failed. %p\n", vp);
		VOP_ABORTOP(dvp, cnp);
		goto unlock;
	}
	error = v7fs_file_link(fs, parent, p, cnp->cn_nameptr);
	/* Sync dirent size change. */
	uvm_vnp_setsize(dvp, v7fs_inode_filesize(&parent_node->inode));

	VOP_UNLOCK(vp);
unlock:
	return error;
}

int
v7fs_rename(void *v)
{
	struct vop_rename_args /* {
				  struct vnode *a_fdvp;	from parent-directory
				  struct vnode *a_fvp;	from file
				  struct componentname *a_fcnp;
				  struct vnode *a_tdvp;	to parent-directory
				  struct vnode *a_tvp;	to file
				  struct componentname *a_tcnp;
				  } */ *a = v;
	struct vnode *fvp = a->a_fvp;
	struct vnode *tvp = a->a_tvp;
	struct vnode *fdvp = a->a_fdvp;
	struct vnode *tdvp = a->a_tdvp;
	struct v7fs_node *parent_from = fdvp->v_data;
	struct v7fs_node *parent_to = tdvp->v_data;
	struct v7fs_node *v7node = fvp->v_data;
	struct v7fs_self *fs = v7node->v7fsmount->core;
	const char *from_name = a->a_fcnp->cn_nameptr;
	const char *to_name = a->a_tcnp->cn_nameptr;
	int error;

	DPRINTF("%s->%s %p %p\n", from_name, to_name, fvp, tvp);

	if ((fvp->v_mount != tdvp->v_mount) ||
	    (tvp && (fvp->v_mount != tvp->v_mount))) {
		error = EXDEV;
		DPRINTF("cross-device link\n");
		goto out;
	}
	// XXXsource file lock?
	error = v7fs_file_rename(fs, &parent_from->inode, from_name,
	    &parent_to->inode, to_name);
	/* 'to file' inode may be changed. (hard-linked and it is cached.)
	   t_vnops rename_reg_nodir */
	if (error == 0 && tvp) {
		struct v7fs_inode *inode =
		    &((struct v7fs_node *)tvp->v_data)->inode;

		error = v7fs_inode_load(fs, inode, inode->inode_number);
		uvm_vnp_setsize(tvp, v7fs_inode_filesize(inode));
	}
	/* Sync dirent size change. */
	uvm_vnp_setsize(tdvp, v7fs_inode_filesize(&parent_to->inode));
	uvm_vnp_setsize(fdvp, v7fs_inode_filesize(&parent_from->inode));
out:
	if (tvp)
		vput(tvp);  /* locked on entry */
	if (tdvp == tvp)
		vrele(tdvp);
	else
		vput(tdvp);
	vrele(fdvp);
	vrele(fvp);

	return error;
}

int
v7fs_mkdir(void *v)
{
	struct vop_mkdir_v3_args /* {
				 struct vnode		*a_dvp;
				 struct vnode		**a_vpp;
				 struct componentname	*a_cnp;
				 struct vattr		*a_vap;
				 } */ *a = v;
	struct componentname *cnp = a->a_cnp;
	kauth_cred_t cr = cnp->cn_cred;
	struct vnode *dvp = a->a_dvp;
	struct vattr *va = a->a_vap;
	struct v7fs_node *parent_node = dvp->v_data;
	struct v7fs_mount *v7fsmount = parent_node->v7fsmount;
	struct v7fs_self *fs = v7fsmount->core;
	struct v7fs_fileattr attr;
	struct mount *mp = v7fsmount->mountp;
	v7fs_ino_t ino;
	int error = 0;

	DPRINTF("\n");
	memset(&attr, 0, sizeof(attr));
	attr.uid = kauth_cred_geteuid(cr);
	attr.gid = kauth_cred_getegid(cr);
	attr.mode = va->va_mode | vtype_to_v7fs_mode(va->va_type);

	if ((error = v7fs_file_allocate(fs, &parent_node->inode,
	    cnp->cn_nameptr, &attr, &ino)))
		return error;
	/* Sync dirent size change. */
	uvm_vnp_setsize(dvp, v7fs_inode_filesize(&parent_node->inode));

	if ((error = v7fs_vget(mp, ino, a->a_vpp))) {
		DPRINTF("can't get vnode.\n");
	}
	struct v7fs_node *newnode = (*a->a_vpp)->v_data;
	newnode->update_ctime = true;
	newnode->update_mtime = true;
	newnode->update_atime = true;

	if (error == 0)
		VOP_UNLOCK(*a->a_vpp);

	return error;
}

int
v7fs_rmdir(void *v)
{
	struct vop_rmdir_args /* {
				 struct vnode		*a_dvp;
				 struct vnode		*a_vp;
				 struct componentname	*a_cnp;
				 } */ *a = v;
	struct vnode *vp = a->a_vp;
	struct vnode *dvp = a->a_dvp;
	struct v7fs_node *parent_node = dvp->v_data;
	struct v7fs_mount *v7fsmount = parent_node->v7fsmount;
	struct v7fs_inode *inode = &((struct v7fs_node *)vp->v_data)->inode;
	struct v7fs_self *fs = v7fsmount->core;
	int error = 0;

	DPRINTF("delete %s\n", a->a_cnp->cn_nameptr);

	KDASSERT(vp->v_type == VDIR);

	if ((error = v7fs_file_deallocate(fs, &parent_node->inode,
	    a->a_cnp->cn_nameptr))) {
		DPRINTF("v7fs_directory_deallocate failed.\n");
		goto out;
	}
	error = v7fs_inode_load(fs, inode, inode->inode_number);
	if (error)
		goto out;
	uvm_vnp_setsize(vp, v7fs_inode_filesize(inode));
	/* Sync dirent size change. */
	uvm_vnp_setsize(dvp, v7fs_inode_filesize(&parent_node->inode));
out:
	vput(vp);
	vput(dvp);

	return error;
}

struct v7fs_readdir_arg {
	struct dirent *dp;
	struct uio *uio;
	int start;
	int end;
	int cnt;
};
static int readdir_subr(struct v7fs_self *, void *, v7fs_daddr_t, size_t);

int
readdir_subr(struct v7fs_self *fs, void *ctx, v7fs_daddr_t blk, size_t sz)
{
	struct v7fs_readdir_arg *p = (struct v7fs_readdir_arg *)ctx;
	struct v7fs_dirent *dir;
	struct dirent *dp = p->dp;
	struct v7fs_inode inode;
	char filename[V7FS_NAME_MAX + 1];
	int i, n;
	int error = 0;
	void *buf;

	if (!(buf = scratch_read(fs, blk)))
		return EIO;
	dir = (struct v7fs_dirent *)buf;

	n = sz / sizeof(*dir);

	for (i = 0; (i < n) && (p->cnt < p->end); i++, dir++, p->cnt++) {
		if (p->cnt < p->start)
			continue;

		if ((error = v7fs_inode_load(fs, &inode, dir->inode_number)))
			break;

		v7fs_dirent_filename(filename, dir->name);

		DPRINTF("inode=%d name=%s %s\n", dir->inode_number, filename,
		    v7fs_inode_isdir(&inode) ? "DIR" : "FILE");
		memset(dp, 0, sizeof(*dp));
		dp->d_fileno = dir->inode_number;
		dp->d_type = v7fs_mode_to_d_type(inode.mode);
		dp->d_namlen = strlen(filename);
		strcpy(dp->d_name, filename);
		dp->d_reclen = sizeof(*dp);
		if ((error = uiomove(dp, dp->d_reclen, p->uio))) {
			DPRINTF("uiomove failed.\n");
			break;
		}
	}
	scratch_free(fs, buf);

	if (p->cnt == p->end)
		return V7FS_ITERATOR_BREAK;

	return error;
}

int
v7fs_readdir(void *v)
{
	struct vop_readdir_args /* {
				   struct vnode *a_vp;
				   struct uio *a_uio;
				   kauth_cred_t a_cred;
				   int *a_eofflag;
				   off_t **a_cookies;
				   int *a_ncookies;
				   } */ *a = v;
	struct uio *uio = a->a_uio;
	struct vnode *vp = a->a_vp;
	struct v7fs_node *v7node = vp->v_data;
	struct v7fs_inode *inode = &v7node->inode;
	struct v7fs_self *fs = v7node->v7fsmount->core;
	struct dirent *dp;
	int error;

	DPRINTF("offset=%zu residue=%zu\n", uio->uio_offset, uio->uio_resid);

	KDASSERT(vp->v_type == VDIR);
	KDASSERT(uio->uio_offset >= 0);
	KDASSERT(v7fs_inode_isdir(inode));

	struct v7fs_readdir_arg arg;
	arg.start = uio->uio_offset / sizeof(*dp);
	arg.end = arg.start +  uio->uio_resid / sizeof(*dp);
	if (arg.start == arg.end) {/* user buffer has not enuf space. */
		DPRINTF("uio buffer too small\n");
		return ENOMEM;
	}
	dp = kmem_zalloc(sizeof(*dp), KM_SLEEP);
	arg.cnt = 0;
	arg.dp = dp;
	arg.uio = uio;

	*a->a_eofflag = false;
	error = v7fs_datablock_foreach(fs, inode, readdir_subr, &arg);
	if (error == V7FS_ITERATOR_END) {
		*a->a_eofflag = true;
	}
	if (error < 0)
		error = 0;

	kmem_free(dp, sizeof(*dp));

	return error;
}

int
v7fs_inactive(void *v)
{
	struct vop_inactive_args /* {
				    struct vnode *a_vp;
				    bool *a_recycle;
				    } */ *a = v;
	struct vnode *vp = a->a_vp;
	struct v7fs_node *v7node = vp->v_data;
	struct v7fs_inode *inode = &v7node->inode;

	DPRINTF("%p #%d\n", vp, inode->inode_number);
	if (v7fs_inode_nlink(inode) > 0) {
		v7fs_update(vp, 0, 0, UPDATE_WAIT);
		*a->a_recycle = false;
	} else {
		*a->a_recycle = true;
	}

	VOP_UNLOCK(vp);

	return 0;
}

int
v7fs_reclaim(void *v)
{
	/*This vnode is no longer referenced by kernel. */
	extern struct pool v7fs_node_pool;
	struct vop_reclaim_args /* {
				   struct vnode *a_vp;
				   } */ *a = v;
	struct vnode *vp = a->a_vp;
	struct v7fs_node *v7node = vp->v_data;
	struct v7fs_self *fs = v7node->v7fsmount->core;
	struct v7fs_inode *inode = &v7node->inode;

	DPRINTF("%p #%d\n", vp, inode->inode_number);
	if (v7fs_inode_nlink(inode) == 0) {
		v7fs_datablock_size_change(fs, 0, inode);
		DPRINTF("remove datablock\n");
		v7fs_inode_deallocate(fs, inode->inode_number);
		DPRINTF("remove inode\n");
	}
	vcache_remove(vp->v_mount,
	    &inode->inode_number, sizeof(inode->inode_number));
	genfs_node_destroy(vp);
	pool_put(&v7fs_node_pool, v7node);
	mutex_enter(vp->v_interlock);
	vp->v_data = NULL;
	mutex_exit(vp->v_interlock);

	return 0;
}

int
v7fs_bmap(void *v)
{
	struct vop_bmap_args /* {
				struct vnode *a_vp;
				daddr_t  a_bn;
				struct vnode **a_vpp;
				daddr_t *a_bnp;
				int *a_runp;
				} */ *a = v;
	struct vnode *vp = a->a_vp;
	struct v7fs_node *v7node = vp->v_data;
	struct v7fs_mount *v7fsmount = v7node->v7fsmount;
	struct v7fs_self *fs = v7node->v7fsmount->core;
	struct v7fs_inode *inode = &v7node->inode;
	int error = 0;

	DPRINTF("inode=%d offset=%zu %p\n", inode->inode_number, a->a_bn, vp);
	DPRINTF("filesize: %d\n", inode->filesize);
	if (!a->a_bnp)
		return 0;

	v7fs_daddr_t blk;
	if (!(blk = v7fs_datablock_last(fs, inode,
	    (a->a_bn + 1) << V7FS_BSHIFT))) {
		/* +1 converts block # to file offset. */
		return ENOSPC;
	}

	*a->a_bnp = blk;

	if (a->a_vpp)
		*a->a_vpp = v7fsmount->devvp;
	if (a->a_runp)
		*a->a_runp = 0; /*XXX TODO */

	DPRINTF("%d  %zu->%zu status=%d\n", inode->inode_number, a->a_bn,
	    *a->a_bnp, error);

	return error;
}

int
v7fs_strategy(void *v)
{
	struct vop_strategy_args /* {
				    struct vnode *a_vp;
				    struct buf *a_bp;
				    } */ *a = v;
	struct buf *b = a->a_bp;
	struct vnode *vp = a->a_vp;
	struct v7fs_node *v7node = vp->v_data;
	struct v7fs_mount *v7fsmount = v7node->v7fsmount;
	int error;

	DPRINTF("%p\n", vp);
	KDASSERT(vp->v_type == VREG);
	if (b->b_blkno == b->b_lblkno) {
		error = VOP_BMAP(vp, b->b_lblkno, NULL, &b->b_blkno, NULL);
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

	return VOP_STRATEGY(v7fsmount->devvp, b);
}

int
v7fs_print(void *v)
{
	struct vop_print_args /* {
				 struct vnode *a_vp;
				 } */ *a = v;
	struct v7fs_node *v7node = a->a_vp->v_data;

	v7fs_inode_dump(&v7node->inode);

	return 0;
}

int
v7fs_advlock(void *v)
{
	struct vop_advlock_args /* {
				   struct vnode *a_vp;
				   void *a_id;
				   int a_op;
				   struct flock *a_fl;
				   int a_flags;
				   } */ *a = v;
	struct v7fs_node *v7node = a->a_vp->v_data;

	DPRINTF("op=%d\n", a->a_op);

	return lf_advlock(a, &v7node->lockf,
	    v7fs_inode_filesize(&v7node->inode));
}

int
v7fs_pathconf(void *v)
{
	struct vop_pathconf_args /* {
				    struct vnode *a_vp;
				    int a_name;
				    register_t *a_retval;
				    } */ *a = v;
	int err = 0;

	DPRINTF("%p\n", a->a_vp);

	switch (a->a_name) {
	case _PC_LINK_MAX:
		*a->a_retval = V7FS_LINK_MAX;
		break;
	case _PC_NAME_MAX:
		*a->a_retval = V7FS_NAME_MAX;
		break;
	case _PC_PATH_MAX:
		*a->a_retval = V7FS_PATH_MAX;
		break;
	case _PC_CHOWN_RESTRICTED:
		*a->a_retval = 1;
		break;
	case _PC_NO_TRUNC:
		*a->a_retval = 0;
		break;
	case _PC_SYNC_IO:
		*a->a_retval = 1;
		break;
	case _PC_FILESIZEBITS:
		*a->a_retval = 30; /* ~1G */
		break;
	case _PC_SYMLINK_MAX:
		*a->a_retval = V7FSBSD_MAXSYMLINKLEN;
		break;
	case _PC_2_SYMLINKS:
		*a->a_retval = 1;
		break;
	default:
		err = EINVAL;
		break;
	}

	return err;
}

int
v7fs_update(struct vnode *vp, const struct timespec *acc,
    const struct timespec *mod, int flags)
{
	struct v7fs_node *v7node = vp->v_data;
	struct v7fs_inode *inode = &v7node->inode;
	struct v7fs_self *fs = v7node->v7fsmount->core;
	bool update = false;

	DPRINTF("%p %zu %d\n", vp, vp->v_size, v7fs_inode_filesize(inode));
	KDASSERT(vp->v_size == v7fs_inode_filesize(inode));

	if (v7node->update_atime) {
		inode->atime = acc ? acc->tv_sec : time_second;
		v7node->update_atime = false;
		update = true;
	}
	if (v7node->update_ctime) {
		inode->ctime = time_second;
		v7node->update_ctime = false;
		update = true;
	}
	if (v7node->update_mtime) {
		inode->mtime = mod ? mod->tv_sec : time_second;
		v7node->update_mtime = false;
		update = true;
	}

	if (update)
		v7fs_inode_writeback(fs, inode);

	return 0;
}

int
v7fs_symlink(void *v)
{
	struct vop_symlink_v3_args /* {
				   struct vnode		*a_dvp;
				   struct vnode		**a_vpp;
				   struct componentname	*a_cnp;
				   struct vattr		*a_vap;
				   char			*a_target;
				   } */ *a = v;
	struct v7fs_node *parent_node = a->a_dvp->v_data;
	struct v7fs_mount *v7fsmount = parent_node->v7fsmount;
	struct v7fs_self *fs = v7fsmount->core;
	struct vattr *va = a->a_vap;
	kauth_cred_t cr = a->a_cnp->cn_cred;
	struct componentname *cnp = a->a_cnp;
	struct v7fs_fileattr attr;
	v7fs_ino_t ino;
	const char *from = a->a_target;
	const char *to = cnp->cn_nameptr;
	size_t len = strlen(from) + 1;
	int error = 0;

	if (len > V7FS_BSIZE) { /* limited to 512byte pathname */
		DPRINTF("too long pathname.");
		return ENAMETOOLONG;
	}

	memset(&attr, 0, sizeof(attr));
	attr.uid = kauth_cred_geteuid(cr);
	attr.gid = kauth_cred_getegid(cr);
	attr.mode = va->va_mode | vtype_to_v7fs_mode(va->va_type);

	if ((error = v7fs_file_allocate
		(fs, &parent_node->inode, to, &attr, &ino))) {
		return error;
	}
	/* Sync dirent size change. */
	uvm_vnp_setsize(a->a_dvp, v7fs_inode_filesize(&parent_node->inode));

	/* Get myself vnode. */
	if ((error = v7fs_vget(v7fsmount->mountp, ino, a->a_vpp))) {
		DPRINTF("can't get vnode.\n");
	}

	struct v7fs_node *newnode = (*a->a_vpp)->v_data;
	struct v7fs_inode *p = &newnode->inode;
	v7fs_file_symlink(fs, p, from);
	uvm_vnp_setsize(*a->a_vpp, v7fs_inode_filesize(p));

	newnode->update_ctime = true;
	newnode->update_mtime = true;
	newnode->update_atime = true;

	if (error == 0)
		VOP_UNLOCK(*a->a_vpp);

	return error;
}

int
v7fs_readlink(void *v)
{
	struct vop_readlink_args /* {
				    struct vnode	*a_vp;
				    struct uio		*a_uio;
				    kauth_cred_t	a_cred;
				    } */ *a = v;
	struct uio *uio = a->a_uio;
	struct vnode *vp = a->a_vp;
	struct v7fs_node *v7node = vp->v_data;
	struct v7fs_inode *inode = &v7node->inode;
	struct v7fs_self *fs = v7node->v7fsmount->core;
	int error = 0;

	KDASSERT(vp->v_type == VLNK);
	KDASSERT(uio->uio_offset >= 0);
	KDASSERT(v7fs_inode_islnk(inode));

	v7fs_daddr_t blk = inode->addr[0];
	void *buf;
	if (!(buf = scratch_read(fs, blk))) {
		error = EIO;
		goto error_exit;
	}

	if ((error = uiomove(buf, strlen(buf), uio))) {
		DPRINTF("uiomove failed.\n");
	}
	scratch_free(fs, buf);

error_exit:
	return error;
}
