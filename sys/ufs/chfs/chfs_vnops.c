/*	$NetBSD: chfs_vnops.c,v 1.2 2011/11/24 21:09:37 agc Exp $	*/

/*-
 * Copyright (c) 2010 Department of Software Engineering,
 *		      University of Szeged, Hungary
 * Copyright (C) 2010 Tamas Toth <ttoth@inf.u-szeged.hu>
 * Copyright (C) 2010 Adam Hoka <ahoka@NetBSD.org>
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by the Department of Software Engineering, University of Szeged, Hungary
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <miscfs/specfs/specdev.h>
#include <miscfs/fifofs/fifo.h>
#include <miscfs/genfs/genfs.h>
#include <ufs/ufs/dir.h>
#include <ufs/ufs/ufs_extern.h>
#include <uvm/uvm.h>
#include <sys/namei.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/buf.h>
#include <sys/fstrans.h>
#include <sys/vnode.h>

#include "chfs.h"

#define READ_S  "chfs_read"

int
chfs_lookup(void *v)
{
	struct vnode *dvp = ((struct vop_lookup_args *) v)->a_dvp;
	struct vnode **vpp = ((struct vop_lookup_args *) v)->a_vpp;
	struct componentname *cnp = ((struct vop_lookup_args *) v)->a_cnp;

	int error;
	struct chfs_inode* ip;
	struct ufsmount* ump;
	struct chfs_mount* chmp;
	struct chfs_vnode_cache* chvc;
	struct chfs_dirent* fd;

	dbg("lookup(): %s\n", cnp->cn_nameptr);

	KASSERT(VOP_ISLOCKED(dvp));

	*vpp = NULL;

	// Check accessibility of requested node as a first step.
	error = VOP_ACCESS(dvp, VEXEC, cnp->cn_cred);
	if (error != 0) {
		goto out;
	}

	// If requesting the last path component on a read-only file system
	// with a write operation, deny it.
	if ((cnp->cn_flags & ISLASTCN) && (dvp->v_mount->mnt_flag & MNT_RDONLY)
	    && (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME)) {
		error = EROFS;
		goto out;
	}

	// Avoid doing a linear scan of the directory if the requested
	// directory/name couple is already in the cache.
	error = cache_lookup(dvp, vpp, cnp);
	if (error >= 0) {
		goto out;
	}

	ip = VTOI(dvp);
	ump = VFSTOUFS(dvp->v_mount);
	chmp = ump->um_chfs;
	if (ip->ino == 0) {
		ip->ino = ++chmp->chm_max_vno;
	}
	mutex_enter(&chmp->chm_lock_vnocache);
	chvc = chfs_vnode_cache_get(chmp, ip->ino);
	mutex_exit(&chmp->chm_lock_vnocache);

	// We cannot be requesting the parent directory of the root node.
	KASSERT(IMPLIES(dvp->v_type == VDIR && chvc->pvno == chvc->vno,
		!(cnp->cn_flags & ISDOTDOT)));

	if (cnp->cn_flags & ISDOTDOT) {
		VOP_UNLOCK(dvp);
		error = VFS_VGET(dvp->v_mount, ip->chvc->pvno, vpp);
		vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY);
	} else if (cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.') {
		vref(dvp);
		*vpp = dvp;
		error = 0;
	} else {
		fd = chfs_dir_lookup(ip, cnp);

		if (fd == NULL) {
			dbg("fd null\n");
			// The entry was not found in the directory.
			// This is OK if we are creating or renaming an
			// entry and are working on the last component of
			// the path name.
			if ((cnp->cn_flags & ISLASTCN) && (cnp->cn_nameiop == CREATE
				|| cnp->cn_nameiop == RENAME)) {
				error = VOP_ACCESS(dvp, VWRITE, cnp->cn_cred);
				if (error) {
					dbg("after the entry was not found in dir\n");
					goto out;
				}

				dbg("return EJUSTRETURN\n");
				error = EJUSTRETURN;
			} else {
				error = ENOENT;
			}
		} else {
			// If we are not at the last path component and
			// found a non-directory or non-link entry (which
			// may itself be pointing to a directory), raise
			// an error.
			if ((fd->type != VDIR && fd->type != VLNK) && !(cnp->cn_flags
				& ISLASTCN)) {
				error = ENOTDIR;
				goto out;
			}

			dbg("vno@allocating new vnode: %llu\n",
				(unsigned long long)fd->vno);
			error = VFS_VGET(dvp->v_mount, fd->vno, vpp);
		}
	}
	// Store the result of this lookup in the cache.  Avoid this if the
	// request was for creation, as it does not improve timings on
	// emprical tests.
	if ((cnp->cn_flags & MAKEENTRY) && cnp->cn_nameiop != CREATE
	    && (cnp->cn_flags & ISDOTDOT) == 0)
		cache_enter(dvp, *vpp, cnp);

out:
	// If there were no errors, *vpp cannot be null and it must be
	// locked.
	KASSERT(IFF(error == 0, *vpp != NULL && VOP_ISLOCKED(*vpp)));

	// dvp must always be locked.
	KASSERT(VOP_ISLOCKED(dvp));

	return error;
}

/* --------------------------------------------------------------------- */

int
chfs_create(void *v)
{
	struct vop_create_args /* {
				  struct vnode *a_dvp;
				  struct vnode **a_vpp;
				  struct componentname *a_cnp;
				  struct vattr *a_vap;
				  } */*ap = v;
	int error, mode;
	dbg("create()\n");

	mode = MAKEIMODE(ap->a_vap->va_type, ap->a_vap->va_mode);

	if ((mode & IFMT) == 0) {
		if (ap->a_vap->va_type == VREG)
			mode |= IFREG;
		if (ap->a_vap->va_type == VSOCK)
			mode |= IFSOCK;
	}

	error = chfs_makeinode(mode, ap->a_dvp,	ap->a_vpp, ap->a_cnp, ap->a_vap->va_type);

	if (error) {
		dbg("error: %d\n", error);
		return error;
	}

	VN_KNOTE(ap->a_dvp, NOTE_WRITE);
	return 0;
}
/* --------------------------------------------------------------------- */

int
chfs_mknod(void *v)
{
	struct vnode *dvp = ((struct vop_mknod_args *) v)->a_dvp;
	struct vnode **vpp = ((struct vop_mknod_args *) v)->a_vpp;
	struct componentname *cnp = ((struct vop_mknod_args *) v)->a_cnp;
	struct vattr *vap = ((struct vop_mknod_args *) v)->a_vap;
	int mode, err = 0;
	struct chfs_inode *ip;
	struct vnode *vp;

	struct ufsmount *ump;
	struct chfs_mount *chmp;
	ino_t ino;

	struct chfs_full_dnode *fd;
	struct buf *bp;
	int len;
	dbg("mknod()\n");

	ump = VFSTOUFS(dvp->v_mount);
	chmp = ump->um_chfs;

	if (vap->va_type != VBLK && vap->va_type != VCHR && vap->va_type != VFIFO)
		return EINVAL;

	vp = *vpp;

	mode = MAKEIMODE(vap->va_type, vap->va_mode);

	if ((mode & IFMT) == 0) {
		switch (vap->va_type) {
		case VBLK:
			mode |= IFBLK;
			break;
		case VCHR:
			mode |= IFCHR;
			break;
		case VFIFO:
			mode |= IFIFO;
			break;
		default:
			break;
		}
	}

	err = chfs_makeinode(mode, dvp, &vp, cnp, vap->va_type);

	ip = VTOI(vp);
	ino = ip->ino;
	if (vap->va_rdev != VNOVAL)
		ip->rdev = vap->va_rdev;

	if (vap->va_type == VFIFO)
		vp->v_op = chfs_fifoop_p;
	else {
		vp->v_op = chfs_specop_p;
		spec_node_init(vp, ip->rdev);
	}

	if (err)
		return err;

	len = sizeof(dev_t);
	chfs_set_vnode_size(vp, len);
	bp = getiobuf(vp, true);
	bp->b_bufsize = bp->b_resid = len;
	bp->b_data = kmem_alloc(len, KM_SLEEP);
	memcpy(bp->b_data, &ip->rdev, len);
	bp->b_blkno = 0;

	fd = chfs_alloc_full_dnode();

	mutex_enter(&chmp->chm_lock_mountfields);

	err = chfs_write_flash_dnode(chmp, vp, bp, fd);
	if (err) {
		mutex_exit(&chmp->chm_lock_mountfields);
		kmem_free(bp->b_data, len);
		return err;
	}

	err = chfs_add_full_dnode_to_inode(chmp, ip, fd);
	if (err) {
		mutex_exit(&chmp->chm_lock_mountfields);
		kmem_free(bp->b_data, len);
		return err;
	}

	mutex_exit(&chmp->chm_lock_mountfields);

	*vpp = vp;
	kmem_free(bp->b_data, len);
	putiobuf(bp);

	return 0;
}

/* --------------------------------------------------------------------- */

int
chfs_open(void *v)
{
	struct vnode *vp = ((struct vop_open_args *) v)->a_vp;
	int mode = ((struct vop_open_args *) v)->a_mode;
	dbg("open()\n");

	int error;
	struct chfs_inode *ip;

	KASSERT(VOP_ISLOCKED(vp));

	ip = VTOI(vp);

	KASSERT(vp->v_size == ip->size);
	if (ip->chvc->nlink < 1) {
		error = ENOENT;
		goto out;
	}

	// If the file is marked append-only, deny write requests.
	if (ip->flags & APPEND && (mode & (FWRITE | O_APPEND)) == FWRITE)
		error = EPERM;
	else
		error = 0;

out:
	KASSERT(VOP_ISLOCKED(vp));
	return error;
}

/* --------------------------------------------------------------------- */

int
chfs_close(void *v)
{
	struct vnode *vp = ((struct vop_close_args *) v)->a_vp;
	dbg("close()\n");

	struct chfs_inode *ip;

	KASSERT(VOP_ISLOCKED(vp));

	ip = VTOI(vp);

	if (ip->chvc->nlink > 0) {
		//ip->chvc->nlink = 0;
		chfs_update(vp, NULL, NULL, UPDATE_CLOSE);
	}

	return 0;
}

/* --------------------------------------------------------------------- */

int
chfs_access(void *v)
{
	struct vnode *vp = ((struct vop_access_args *) v)->a_vp;
	int mode = ((struct vop_access_args *) v)->a_mode;
	kauth_cred_t cred = ((struct vop_access_args *) v)->a_cred;

	dbg("access()\n");
	struct chfs_inode *ip = VTOI(vp);

	if (mode & VWRITE) {
		switch (vp->v_type) {
		case VLNK:
		case VDIR:
		case VREG:
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);
			break;
		case VBLK:
		case VCHR:
		case VSOCK:
		case VFIFO:
			break;
		default:
			break;
		}
	}

	if (mode & VWRITE && ip->flags & IMMUTABLE)
		return (EPERM);

	return genfs_can_access(vp->v_type, ip->mode & ALLPERMS,
	    ip->uid, ip->gid, mode, cred);
}

/* --------------------------------------------------------------------- */

int
chfs_getattr(void *v)
{
	struct vnode *vp = ((struct vop_getattr_args *) v)->a_vp;
	struct vattr *vap = ((struct vop_getattr_args *) v)->a_vap;

	struct chfs_inode *ip = VTOI(vp);
	dbg("getattr()\n");

	KASSERT(vp->v_size == ip->size);

	vattr_null(vap);
	CHFS_ITIMES(ip, NULL, NULL, NULL);

	vap->va_type = vp->v_type;
	vap->va_mode = ip->mode & ALLPERMS;
	vap->va_nlink = ip->chvc->nlink;
	vap->va_uid = ip->uid;
	vap->va_gid = ip->gid;
	vap->va_fsid = ip->dev;
	vap->va_fileid = ip->ino;
	vap->va_size = ip->size;
	vap->va_blocksize = PAGE_SIZE;
	vap->va_atime.tv_sec = ip->atime;
	vap->va_atime.tv_nsec = 0;
	vap->va_mtime.tv_sec = ip->mtime;
	vap->va_mtime.tv_nsec = 0;
	vap->va_ctime.tv_sec = ip->ctime;
	vap->va_ctime.tv_nsec = 0;
	vap->va_gen = ip->version;
	vap->va_flags = ip->flags;
	vap->va_rdev = ip->rdev;
	vap->va_bytes = round_page(ip->size);
	vap->va_filerev = VNOVAL;
	vap->va_vaflags = 0;
	vap->va_spare = VNOVAL;

	return 0;
}

/* --------------------------------------------------------------------- */

/* Note: modelled after tmpfs's same function */

int
chfs_setattr(void *v)
{
	struct vnode *vp = ((struct vop_setattr_args *) v)->a_vp;
	struct vattr *vap = ((struct vop_setattr_args *) v)->a_vap;
	kauth_cred_t cred = ((struct vop_setattr_args *) v)->a_cred;

	struct chfs_inode *ip;
	struct ufsmount *ump = VFSTOUFS(vp->v_mount);
	struct chfs_mount *chmp = ump->um_chfs;
	int error = 0;

	dbg("setattr()\n");

	KASSERT(VOP_ISLOCKED(vp));
	ip = VTOI(vp);

	/* Abort if any unsettable attribute is given. */
	if (vap->va_type != VNON || vap->va_nlink != VNOVAL ||
	    vap->va_fsid != VNOVAL || vap->va_fileid != VNOVAL ||
	    vap->va_blocksize != VNOVAL /*|| GOODTIME(&vap->va_ctime)*/ ||
	    vap->va_gen != VNOVAL || vap->va_rdev != VNOVAL ||
	    vap->va_bytes != VNOVAL) {
		return EINVAL;
	}

	if (error == 0 && (vap->va_flags != VNOVAL))
		error = chfs_chflags(vp, vap->va_flags, cred);

	if (error == 0 && (vap->va_size != VNOVAL))
		error = chfs_chsize(vp, vap->va_size, cred);

	if (error == 0 && (vap->va_uid != VNOVAL || vap->va_gid != VNOVAL))
		error = chfs_chown(vp, vap->va_uid, vap->va_gid, cred);

	if (error == 0 && (vap->va_mode != VNOVAL))
		error = chfs_chmod(vp, vap->va_mode, cred);

#if 0
	/* why do we need that? */
	if (ip->flags & (IMMUTABLE | APPEND))
		return EPERM;
#endif

	if (vap->va_atime.tv_sec != VNOVAL || vap->va_mtime.tv_sec != VNOVAL) {
		error = genfs_can_chtimes(vp, vap->va_vaflags, ip->uid, cred);
		if (error)
			return error;
		if (vap->va_atime.tv_sec != VNOVAL)
			ip->iflag |= IN_ACCESS;
		if (vap->va_mtime.tv_sec != VNOVAL)
			ip->iflag |= IN_CHANGE | IN_UPDATE;
		error = chfs_update(vp,
		    &vap->va_atime, &vap->va_mtime, UPDATE_WAIT);
		if (error)
			return error;
	}

	mutex_enter(&chmp->chm_lock_mountfields);
	error = chfs_write_flash_vnode(chmp, ip, ALLOC_NORMAL);
	mutex_exit(&chmp->chm_lock_mountfields);

	return error;
}

int
chfs_chmod(struct vnode *vp, int mode, kauth_cred_t cred)
{
	struct chfs_inode *ip = VTOI(vp);
	int error;
	dbg("chmod\n");

	error = genfs_can_chmod(vp, cred, ip->uid, ip->gid, mode);
	if (error)
		return error;
	ip->mode &= ~ALLPERMS;
	ip->mode |= (mode & ALLPERMS);
	ip->iflag |= IN_CHANGE;

	error = chfs_update(vp, NULL, NULL, UPDATE_WAIT);
	if (error)
		return error;

	return 0;
}

int
chfs_chown(struct vnode *vp, uid_t uid, gid_t gid, kauth_cred_t cred)
{
	struct chfs_inode *ip = VTOI(vp);
	int error;
	dbg("chown\n");

	if (uid == (uid_t)VNOVAL)
		uid = ip->uid;
	if (gid == (gid_t)VNOVAL)
		gid = ip->gid;

	error = genfs_can_chown(vp, cred, ip->uid, ip->gid, uid, gid);
	if (error)
		return error;

	ip->gid = gid;
	ip->uid = uid;
	ip->iflag |= IN_CHANGE;

	error = chfs_update(vp, NULL, NULL, UPDATE_WAIT);
	if (error)
		return error;

	return 0;
}


/* --------------------------------------------------------------------- */
/* calculates ((off_t)blk * chmp->chm_chm_fs_bsize) */
#define	lblktosize(chmp, blk)						      \
	(((off_t)(blk)) << (chmp)->chm_fs_bshift)

/* calculates (loc % chmp->chm_chm_fs_bsize) */
#define	blkoff(chmp, loc)							      \
	((loc) & (chmp)->chm_fs_qbmask)

/* calculates (loc / chmp->chm_chm_fs_bsize) */
#define	lblkno(chmp, loc)							      \
	((loc) >> (chmp)->chm_fs_bshift)

/* calculates roundup(size, chmp->chm_chm_fs_fsize) */
#define	fragroundup(chmp, size)						      \
	(((size) + (chmp)->chm_fs_qfmask) & (chmp)->chm_fs_fmask)

#define	blksize(chmp, ip, lbn)						      \
	(((lbn) >= NDADDR || (ip)->size >= lblktosize(chmp, (lbn) + 1))	      \
	    ? (chmp)->chm_fs_bsize					      \
	    : (fragroundup(chmp, blkoff(chmp, (ip)->size))))

/* calculates roundup(size, chmp->chm_chm_fs_bsize) */
#define	blkroundup(chmp, size)						      \
 	(((size) + (chmp)->chm_fs_qbmask) & (chmp)->chm_fs_bmask)

int
chfs_read(void *v)
{
	struct vop_read_args /* {
				struct vnode *a_vp;
				struct uio *a_uio;
				int a_ioflag;
				kauth_cred_t a_cred;
				} */ *ap = v;
	struct vnode *vp;
	struct chfs_inode *ip;
	struct uio *uio;
	struct ufsmount *ump;
	struct buf *bp;
	struct chfs_mount *chmp;
	daddr_t lbn, nextlbn;
	off_t bytesinfile;
	long size, xfersize, blkoffset;
	int error, ioflag;
	vsize_t bytelen;
	bool usepc = false;

	dbg("chfs_read\n");

	vp = ap->a_vp;
	ip = VTOI(vp);
	ump = ip->ump;
	uio = ap->a_uio;
	ioflag = ap->a_ioflag;
	error = 0;

	dbg("ip->size:%llu\n", (unsigned long long)ip->size);

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_READ)
		panic("%s: mode", READ_S);

	if (vp->v_type == VLNK) {
		if (ip->size < ump->um_maxsymlinklen)
			panic("%s: short symlink", READ_S);
	} else if (vp->v_type != VREG && vp->v_type != VDIR)
		panic("%s: type %d", READ_S, vp->v_type);
#endif
	chmp = ip->chmp;
	if ((u_int64_t)uio->uio_offset > ump->um_maxfilesize)
		return (EFBIG);
	if (uio->uio_resid == 0)
		return (0);

	fstrans_start(vp->v_mount, FSTRANS_SHARED);

	if (uio->uio_offset >= ip->size)
		goto out;

	usepc = vp->v_type == VREG;
	bytelen = 0;
	if (usepc) {
		const int advice = IO_ADV_DECODE(ap->a_ioflag);

		while (uio->uio_resid > 0) {
			if (ioflag & IO_DIRECT) {
				genfs_directio(vp, uio, ioflag);
			}
			bytelen = MIN(ip->size - uio->uio_offset,
			    uio->uio_resid);
			if (bytelen == 0)
				break;
			error = ubc_uiomove(&vp->v_uobj, uio, bytelen, advice,
			    UBC_READ | UBC_PARTIALOK |
			    (UBC_WANT_UNMAP(vp) ? UBC_UNMAP : 0));
			if (error)
				break;

		}
		goto out;
	}


	dbg("start reading\n");
	for (error = 0, bp = NULL; uio->uio_resid > 0; bp = NULL) {
		bytesinfile = ip->size - uio->uio_offset;
		if (bytesinfile <= 0)
			break;
		lbn = lblkno(chmp, uio->uio_offset);
		nextlbn = lbn + 1;
		size = blksize(chmp, ip, lbn);
		blkoffset = blkoff(chmp, uio->uio_offset);
		xfersize = MIN(MIN(chmp->chm_fs_bsize - blkoffset, uio->uio_resid),
		    bytesinfile);

		if (lblktosize(chmp, nextlbn) >= ip->size) {
			error = bread(vp, lbn, size, NOCRED, 0, &bp);
			dbg("after bread\n");
		} else {
			int nextsize = blksize(chmp, ip, nextlbn);
			dbg("size: %ld\n", size);
			error = breadn(vp, lbn,
			    size, &nextlbn, &nextsize, 1, NOCRED, 0, &bp);
			dbg("after breadN\n");
		}
		if (error)
			break;

		/*
		 * We should only get non-zero b_resid when an I/O error
		 * has occurred, which should cause us to break above.
		 * However, if the short read did not cause an error,
		 * then we want to ensure that we do not uiomove bad
		 * or uninitialized data.
		 */
		size -= bp->b_resid;
		if (size < xfersize) {
			if (size == 0)
				break;
			xfersize = size;
		}
		dbg("uiomove\n");
		error = uiomove((char *)bp->b_data + blkoffset, xfersize, uio);
		if (error)
			break;
		brelse(bp, 0);
	}
	if (bp != NULL)
		brelse(bp, 0);

out:
	if (!(vp->v_mount->mnt_flag & MNT_NOATIME)) {
		ip->iflag |= IN_ACCESS;
		if ((ap->a_ioflag & IO_SYNC) == IO_SYNC) {
			//error = UFS_WAPBL_BEGIN(vp->v_mount);
			if (error) {
				fstrans_done(vp->v_mount);
				return error;
			}
			error = chfs_update(vp, NULL, NULL, UPDATE_WAIT);
			//UFS_WAPBL_END(vp->v_mount);
		}
	}

	dbg("[END]\n");
	fstrans_done(vp->v_mount);
	return (error);
}


/* --------------------------------------------------------------------- */

/*from ffs write*/
int
chfs_write(void *v)
{
	struct vop_write_args /* {
				 struct vnode *a_vp;
				 struct uio *a_uio;
				 int a_ioflag;
				 kauth_cred_t a_cred;
				 } */ *ap = v;
	struct vnode *vp ;
	struct uio *uio;
	struct chfs_inode *ip;
	struct chfs_mount *chmp;
	struct lwp *l;
	kauth_cred_t cred;
	off_t osize, origoff, oldoff, preallocoff, endallocoff, nsize;
	int blkoffset, error, flags, ioflag, resid;
	int aflag;
	int extended=0;
	vsize_t bytelen;
	bool async;
	struct ufsmount *ump;


	cred = ap->a_cred;
	ioflag = ap->a_ioflag;
	uio = ap->a_uio;
	vp = ap->a_vp;
	ip = VTOI(vp);
	//dbg("file size (vp): %llu\n", (unsigned long long)vp->v_size);
	//dbg("file size (ip): %llu\n", (unsigned long long)ip->i_size);
	ump = ip->ump;

	//dbg("uio->resid: %d\n", uio->uio_resid);
	dbg("write\n");

	KASSERT(vp->v_size == ip->size);

	switch (vp->v_type) {
	case VREG:
		if (ioflag & IO_APPEND)
			uio->uio_offset = ip->size;
		if ((ip->flags & APPEND) && uio->uio_offset != ip->size)
			return (EPERM);
		/* FALLTHROUGH */
	case VLNK:
		break;
	case VDIR:
		if ((ioflag & IO_SYNC) == 0)
			panic("chfs_write: nonsync dir write");
		break;
	default:
		panic("chfs_write: type");
	}

	chmp = ip->chmp;
	if (uio->uio_offset < 0 ||
	    (u_int64_t)uio->uio_offset +
	    uio->uio_resid > ump->um_maxfilesize) {
		dbg("uio->uio_offset = %lld | uio->uio_offset + "
		    "uio->uio_resid (%llu) > ump->um_maxfilesize (%lld)\n",
		    (long long)uio->uio_offset,
		    (uint64_t)uio->uio_offset + uio->uio_resid,
		    (long long)ump->um_maxfilesize);
		return (EFBIG);
	}
	/*
	 * Maybe this should be above the vnode op call, but so long as
	 * file servers have no limits, I don't think it matters.
	 */
	l = curlwp;
	if (vp->v_type == VREG && l &&
	    uio->uio_offset + uio->uio_resid >
	    l->l_proc->p_rlimit[RLIMIT_FSIZE].rlim_cur) {
		mutex_enter(proc_lock);
		psignal(l->l_proc, SIGXFSZ);
		mutex_exit(proc_lock);
		return (EFBIG);
	}
	if (uio->uio_resid == 0)
		return (0);

	//mutex_enter(&ip->inode_lock);
	fstrans_start(vp->v_mount, FSTRANS_SHARED);

	flags = ioflag & IO_SYNC ? B_SYNC : 0;
	async = vp->v_mount->mnt_flag & MNT_ASYNC;
	origoff = uio->uio_offset;
	resid = uio->uio_resid;
	osize = ip->size;
	error = 0;


	/*if ((ioflag & IO_JOURNALLOCKED) == 0) {
	  error = UFS_WAPBL_BEGIN(vp->v_mount);
	  if (error) {
	  fstrans_done(vp->v_mount);
	  return error;
	  }
	  }*/

	preallocoff = round_page(blkroundup(chmp,
		MAX(osize, uio->uio_offset)));
	aflag = ioflag & IO_SYNC ? B_SYNC : 0;
	nsize = MAX(osize, uio->uio_offset + uio->uio_resid);
	endallocoff = nsize - blkoff(chmp, nsize);

	/*
	 * if we're increasing the file size, deal with expanding
	 * the fragment if there is one.
	 */

	if (nsize > osize && lblkno(chmp, osize) < NDADDR &&
	    lblkno(chmp, osize) != lblkno(chmp, nsize) &&
	    blkroundup(chmp, osize) != osize) {
		off_t eob;

		eob = blkroundup(chmp, osize);
		uvm_vnp_setwritesize(vp, eob);
		error = ufs_balloc_range(vp, osize, eob - osize, cred, aflag);
		if (error)
			goto out;
		if (flags & B_SYNC) {
			mutex_enter(vp->v_interlock);
			VOP_PUTPAGES(vp,
			    trunc_page(osize & chmp->chm_fs_bmask),
			    round_page(eob),
			    PGO_CLEANIT | PGO_SYNCIO | PGO_JOURNALLOCKED);
		}
	}

	while (uio->uio_resid > 0) {
		int ubc_flags = UBC_WRITE;
		bool overwrite; /* if we're overwrite a whole block */
		off_t newoff;

		if (ioflag & IO_DIRECT) {
			genfs_directio(vp, uio, ioflag | IO_JOURNALLOCKED);
		}

		oldoff = uio->uio_offset;
		blkoffset = blkoff(chmp, uio->uio_offset);
		bytelen = MIN(chmp->chm_fs_bsize - blkoffset, uio->uio_resid);
		if (bytelen == 0) {
			break;
		}

		/*
		 * if we're filling in a hole, allocate the blocks now and
		 * initialize the pages first.  if we're extending the file,
		 * we can safely allocate blocks without initializing pages
		 * since the new blocks will be inaccessible until the write
		 * is complete.
		 */
		overwrite = uio->uio_offset >= preallocoff &&
		    uio->uio_offset < endallocoff;
		if (!overwrite && (vp->v_vflag & VV_MAPPED) == 0 &&
		    blkoff(chmp, uio->uio_offset) == 0 &&
		    (uio->uio_offset & PAGE_MASK) == 0) {
			vsize_t len;

			len = trunc_page(bytelen);
			len -= blkoff(chmp, len);
			if (len > 0) {
				overwrite = true;
				bytelen = len;
			}
		}

		newoff = oldoff + bytelen;
		if (vp->v_size < newoff) {
			uvm_vnp_setwritesize(vp, newoff);
		}

		if (!overwrite) {
			error = ufs_balloc_range(vp, uio->uio_offset, bytelen,
			    cred, aflag);
			if (error)
				break;
		} else {
			genfs_node_wrlock(vp);
			error = GOP_ALLOC(vp, uio->uio_offset, bytelen,
			    aflag, cred);
			genfs_node_unlock(vp);
			if (error)
				break;
			ubc_flags |= UBC_FAULTBUSY;
		}

		/*
		 * copy the data.
		 */

		ubc_flags |= UBC_WANT_UNMAP(vp) ? UBC_UNMAP : 0;
		error = ubc_uiomove(&vp->v_uobj, uio, bytelen,
		    IO_ADV_DECODE(ioflag), ubc_flags);

		/*
		 * update UVM's notion of the size now that we've
		 * copied the data into the vnode's pages.
		 *
		 * we should update the size even when uiomove failed.
		 */

		if (vp->v_size < newoff) {
			uvm_vnp_setsize(vp, newoff);
			extended = 1;
		}

		if (error)
			break;

		/*
		 * flush what we just wrote if necessary.
		 * XXXUBC simplistic async flushing.
		 */

		if (!async && oldoff >> 16 != uio->uio_offset >> 16) {
			mutex_enter(vp->v_interlock);
			error = VOP_PUTPAGES(vp, (oldoff >> 16) << 16,
			    (uio->uio_offset >> 16) << 16,
			    PGO_CLEANIT | PGO_JOURNALLOCKED);
			if (error)
				break;
		}
	}
out:
	if (error == 0 && ioflag & IO_SYNC) {
		mutex_enter(vp->v_interlock);
		error = VOP_PUTPAGES(vp,
		    trunc_page(origoff & chmp->chm_fs_bmask),
		    round_page(blkroundup(chmp, uio->uio_offset)),
		    PGO_CLEANIT | PGO_SYNCIO | PGO_JOURNALLOCKED);
	}
	ip->iflag |= IN_CHANGE | IN_UPDATE;
	if (resid > uio->uio_resid && ap->a_cred &&
	    kauth_authorize_generic(ap->a_cred, KAUTH_GENERIC_ISSUSER, NULL)) {
		ip->mode &= ~(ISUID | ISGID);
	}
	if (resid > uio->uio_resid)
		VN_KNOTE(vp, NOTE_WRITE | (extended ? NOTE_EXTEND : 0));
	if (error) {
		(void) UFS_TRUNCATE(vp, osize, ioflag & IO_SYNC, ap->a_cred);
		uio->uio_offset -= resid - uio->uio_resid;
		uio->uio_resid = resid;
	} else if (resid > uio->uio_resid && (ioflag & IO_SYNC) == IO_SYNC)
		error = UFS_UPDATE(vp, NULL, NULL, UPDATE_WAIT);

	//XXX hack, i write the next line after i know ip->i_size and vp->v_size don't equal
	chfs_set_vnode_size(vp, vp->v_size);


	//dbg("end file size (vp): %llu\n", (unsigned long long)vp->v_size);
	//dbg("end file size (ip): %llu\n", (unsigned long long)ip->i_size);
	KASSERT(vp->v_size == ip->size);
	fstrans_done(vp->v_mount);

	mutex_enter(&chmp->chm_lock_mountfields);
	error = chfs_write_flash_vnode(chmp, ip, ALLOC_NORMAL);
	mutex_exit(&chmp->chm_lock_mountfields);

	//mutex_exit(&ip->inode_lock);
	//dbg("end\n");
	return (error);
}


/* --------------------------------------------------------------------- */

int
chfs_fsync(void *v)
{
	//dbg("fsync\n");
	struct vop_fsync_args /* {
				 struct vnode *a_vp;
				 kauth_cred_t a_cred;
				 int a_flags;
				 off_t offlo;
				 off_t offhi;
				 } */ *ap = v;
	struct vnode *vp = ap->a_vp;
	int wait;

	if (ap->a_flags & FSYNC_CACHE) {
		return ENODEV;
	}
	wait = (ap->a_flags & FSYNC_WAIT) != 0;
 	vflushbuf(vp, wait);
	//struct chfs_inode *ip = VTOI(vp);
	//chfs_set_vnode_size(vp, ip->write_size);

	return 0;
}

/* --------------------------------------------------------------------- */

int
chfs_remove(void *v)
{
	struct vnode *dvp = ((struct vop_remove_args *) v)->a_dvp;
	struct vnode *vp = ((struct vop_remove_args *) v)->a_vp;
	struct componentname *cnp = (((struct vop_remove_args *) v)->a_cnp);
	dbg("remove\n");

	KASSERT(VOP_ISLOCKED(dvp));
	KASSERT(VOP_ISLOCKED(vp));

	struct chfs_inode *ip = VTOI(vp);
	struct chfs_inode *parent = VTOI(dvp);
	int error = 0;

	KASSERT(ip->chvc->vno != ip->chvc->pvno);

	error = chfs_do_unlink(ip,
	    parent, cnp->cn_nameptr, cnp->cn_namelen);

	vput(dvp);
	vput(vp);

	return error;
}

/* --------------------------------------------------------------------- */

int
chfs_link(void *v)
{
	struct vnode *dvp = ((struct vop_link_args *) v)->a_dvp;
	struct vnode *vp = ((struct vop_link_args *) v)->a_vp;
	struct componentname *cnp = ((struct vop_link_args *) v)->a_cnp;

	struct chfs_inode *ip, *parent;
	int error = 0;

	if (vp->v_type == VDIR) {
		VOP_ABORTOP(dvp, cnp);
		error = EISDIR;
		goto out;
	}
	if (dvp->v_mount != vp->v_mount) {
		VOP_ABORTOP(dvp, cnp);
		error = EXDEV;
		goto out;
	}
	if (dvp != vp && (error = vn_lock(vp, LK_EXCLUSIVE))) {
		VOP_ABORTOP(dvp, cnp);
		goto out;
	}

	parent = VTOI(dvp);
	ip = VTOI(vp);

	error = chfs_do_link(ip,
	    parent, cnp->cn_nameptr, cnp->cn_namelen, vp->v_type);

	if (dvp != vp)
		VOP_UNLOCK(vp);
out:
	vput(dvp);
	return error;
}

/* --------------------------------------------------------------------- */

int
chfs_rename(void *v)
{
	struct vnode *fdvp = ((struct vop_rename_args *) v)->a_fdvp;
	struct vnode *fvp = ((struct vop_rename_args *) v)->a_fvp;
	struct componentname *fcnp = ((struct vop_rename_args *) v)->a_fcnp;
	struct vnode *tdvp = ((struct vop_rename_args *) v)->a_tdvp;
	struct vnode *tvp = ((struct vop_rename_args *) v)->a_tvp;
	struct componentname *tcnp = ((struct vop_rename_args *) v)->a_tcnp;

	struct chfs_inode *oldparent, *old;
	struct chfs_inode *newparent;
	struct chfs_dirent *fd;//, *oldfd;
	struct chfs_inode *ip;
	int error = 0;
	dbg("rename\n");

	KASSERT(VOP_ISLOCKED(tdvp));
	KASSERT(IMPLIES(tvp != NULL, VOP_ISLOCKED(tvp) == LK_EXCLUSIVE));

	oldparent = VTOI(fdvp);
	old = VTOI(fvp);
	newparent = VTOI(tdvp);
	if (tvp) {
		dbg("tvp not null\n");
		ip = VTOI(tvp);
		if (tvp->v_type == VDIR) {
			//TODO: lock
//			fd = ip->dents;
//			while (fd) {
			TAILQ_FOREACH(fd, &ip->dents, fds) {
				if (fd->vno) {
					//TODO: unlock
					error = ENOTEMPTY;
					goto out_unlocked;
				}
//				fd = fd->next;
			}
			//TODO: unlock
		}
		error = chfs_do_unlink(ip,
		    newparent, tcnp->cn_nameptr, tcnp->cn_namelen);
		vput(tvp);
	}
	VFS_VGET(tdvp->v_mount, old->ino, &tvp);
	ip = VTOI(tvp);

//	for (oldfd = oldparent->dents;
//	     oldfd->vno != old->ino;
//	     oldfd = oldfd->next);

	error = chfs_do_link(ip,
	    newparent, tcnp->cn_nameptr, tcnp->cn_namelen, tvp->v_type);
	error = chfs_do_unlink(old,
	    oldparent, fcnp->cn_nameptr, fcnp->cn_namelen);

//out:
//	if (fchnode != tchnode)
//	VOP_UNLOCK(fdvp, 0);

out_unlocked:
	// Release target nodes.
	if (tdvp == tvp)
		vrele(tdvp);
	else
		vput(tdvp);
	if (tvp != NULL)
		vput(tvp);

	// Release source nodes.
	vrele(fdvp);
	vrele(fvp);

	return error;
}

/* --------------------------------------------------------------------- */

int
chfs_mkdir(void *v)
{
	struct vnode *dvp = ((struct vop_mkdir_args *) v)->a_dvp;
	struct vnode **vpp = ((struct vop_mkdir_args *)v)->a_vpp;
	struct componentname *cnp = ((struct vop_mkdir_args *) v)->a_cnp;
	struct vattr *vap = ((struct vop_mkdir_args *) v)->a_vap;
	dbg("mkdir()\n");

	int mode;

	mode = vap->va_mode & ACCESSPERMS;
	if ((mode & IFMT) == 0) {
		mode |= IFDIR;
	}

	KASSERT(vap->va_type == VDIR);

	return chfs_makeinode(mode, dvp, vpp, cnp, VDIR);
}

/* --------------------------------------------------------------------- */

int
chfs_rmdir(void *v)
{
	struct vnode *dvp = ((struct vop_rmdir_args *) v)->a_dvp;
	struct vnode *vp = ((struct vop_rmdir_args *) v)->a_vp;
	struct componentname *cnp = ((struct vop_rmdir_args *) v)->a_cnp;
	dbg("rmdir()\n");

	KASSERT(VOP_ISLOCKED(dvp));
	KASSERT(VOP_ISLOCKED(vp));

	struct chfs_inode *ip = VTOI(vp);
	struct chfs_inode *parent = VTOI(dvp);
	struct chfs_dirent *fd;
	int error = 0;

	if (vp->v_type != VDIR) {
		error = ENOTDIR;
		goto out;
	}

	KASSERT(ip->chvc->vno != ip->chvc->pvno);

//	for (fd = ip->dents; fd; fd = fd->next) {
	TAILQ_FOREACH(fd, &ip->dents, fds) {
		if (fd->vno) {
			error = ENOTEMPTY;
			goto out;
		}
	}

	error = chfs_do_unlink(ip,
	    parent, cnp->cn_nameptr, cnp->cn_namelen);

out:
	vput(dvp);
	vput(vp);

	return error;
}

/* --------------------------------------------------------------------- */

int
chfs_symlink(void *v)
{
	struct vnode *dvp = ((struct vop_symlink_args *) v)->a_dvp;
	struct vnode **vpp = ((struct vop_symlink_args *) v)->a_vpp;
	struct componentname *cnp = ((struct vop_symlink_args *) v)->a_cnp;
	struct vattr *vap = ((struct vop_symlink_args *) v)->a_vap;
	char *target = ((struct vop_symlink_args *) v)->a_target;

	struct ufsmount *ump;
	struct chfs_mount *chmp;
	struct vnode *vp;
	struct chfs_inode *ip;
	int len, err;
	struct chfs_full_dnode *fd;
	struct buf *bp;
	dbg("symlink()\n");

	ump = VFSTOUFS(dvp->v_mount);
	chmp = ump->um_chfs;

	err = chfs_makeinode(IFLNK | vap->va_mode, dvp, vpp, cnp, VLNK);
	if (err)
		return (err);
	VN_KNOTE(dvp, NOTE_WRITE);
	vp = *vpp;
	len = strlen(target);
	ip = VTOI(vp);
	/* TODO max symlink len instead of "100" */
	if (len < 100) {
		ip->target = kmem_alloc(len, KM_SLEEP);
		memcpy(ip->target, target, len);
		chfs_set_vnode_size(vp, len);
		ip->iflag |= IN_CHANGE | IN_UPDATE;

		bp = getiobuf(vp, true);
		bp->b_bufsize = bp->b_resid = len;
		bp->b_data = kmem_alloc(len, KM_SLEEP);
		memcpy(bp->b_data, target, len);
		bp->b_blkno = 0;

		fd = chfs_alloc_full_dnode();

		mutex_enter(&chmp->chm_lock_mountfields);

		err = chfs_write_flash_dnode(chmp, vp, bp, fd);
		if (err) {
			mutex_exit(&chmp->chm_lock_mountfields);
			goto out;
		}

		err = chfs_add_full_dnode_to_inode(chmp, ip, fd);
		if (err) {
			mutex_exit(&chmp->chm_lock_mountfields);
			goto out;
		}

		mutex_exit(&chmp->chm_lock_mountfields);

		kmem_free(bp->b_data, len);
		putiobuf(bp);

		uvm_vnp_setsize(vp, len);
	} else {
		err = vn_rdwr(UIO_WRITE, vp, target, len, (off_t)0,
		    UIO_SYSSPACE, IO_NODELOCKED, cnp->cn_cred,
		    (size_t *)0, NULL);
	}

out:
	if (err)
		vput(vp);

	return (err);
}

/* --------------------------------------------------------------------- */

int
chfs_readdir(void *v)
{
	struct vnode *vp = ((struct vop_readdir_args *) v)->a_vp;
	struct uio *uio = ((struct vop_readdir_args *) v)->a_uio;
	int *eofflag = ((struct vop_readdir_args *) v)->a_eofflag;

	int error = 0;
	off_t skip, offset;
	struct chfs_inode *ip;
	struct chfs_dirent *fd;

	struct ufsmount *ump;
	struct chfs_mount *chmp;
	struct chfs_vnode_cache *chvc;

	KASSERT(VOP_ISLOCKED(vp));

	/* This operation only makes sense on directory nodes. */
	if (vp->v_type != VDIR) {
		error = ENOTDIR;
		goto out;
	}

	ip = VTOI(vp);

	/* uiomove in chfs_filldir automatically increments the
	 * uio_offset by an arbitrary size, so we discard any change
	 * to uio_offset and set it to our own value on return
	 */
	offset = uio->uio_offset;

	if (offset == CHFS_OFFSET_DOT) {
		error = chfs_filldir(uio, ip->ino, ".", 1, VDIR);
		if (error == -1) {
			error = 0;
			goto outok;
		} else if (error != 0)
			goto outok;

		offset = CHFS_OFFSET_DOTDOT;
	}

	if (offset == CHFS_OFFSET_DOTDOT) {
		ump = VFSTOUFS(vp->v_mount);
		chmp = ump->um_chfs;
		mutex_enter(&chmp->chm_lock_vnocache);
		chvc = chfs_vnode_cache_get(chmp, ip->ino);
		mutex_exit(&chmp->chm_lock_vnocache);

		error = chfs_filldir(uio, chvc->pvno, "..", 2, VDIR);
		if (error == -1) {
			error = 0;
			goto outok;
		} else if (error != 0) {
			goto outok;
		}

		if (TAILQ_EMPTY(&ip->dents)) {
			offset = CHFS_OFFSET_EOF;
		} else {
			offset = CHFS_OFFSET_FIRST;
		}
	}

	if (offset != CHFS_OFFSET_EOF) {
		skip = offset - CHFS_OFFSET_FIRST;

		TAILQ_FOREACH(fd, &ip->dents, fds) {
			/* seek to offset by skipping items */
			/* XXX race conditions by changed dirent? */
			if (skip > 0) {
				skip--;
				continue;
			}

			if (fd->vno != 0) {
				error = chfs_filldir(uio, fd->vno,
				    fd->name, fd->nsize, fd->type);
				if (error == -1) {
					error = 0;
					goto outok;
				} else if (error != 0) {
					dbg("err %d\n", error);
					goto outok;
				}
			}
			offset++;
		}
	}
	offset = CHFS_OFFSET_EOF;

outok:
	uio->uio_offset = offset;

	if (eofflag != NULL) {
		*eofflag = (error == 0 &&
		    uio->uio_offset == CHFS_OFFSET_EOF);
	}

out:
	KASSERT(VOP_ISLOCKED(vp));

	return error;
}

/* --------------------------------------------------------------------- */

int
chfs_readlink(void *v)
{

	struct vnode *vp = ((struct vop_readlink_args *) v)->a_vp;
	struct uio *uio = ((struct vop_readlink_args *) v)->a_uio;
	kauth_cred_t cred = ((struct vop_readlink_args *) v)->a_cred;

	struct chfs_inode *ip = VTOI(vp);

	dbg("readlink()\n");

	/* TODO max symlink len instead of "100" */
	if (ip->size < 100) {
		uiomove(ip->target, ip->size, uio);
		return (0);
	}

	return (VOP_READ(vp, uio, 0, cred));
}

/* --------------------------------------------------------------------- */

int
chfs_inactive(void *v)
{
	struct vnode *vp = ((struct vop_inactive_args *) v)->a_vp;
	struct chfs_inode *ip = VTOI(vp);
	struct chfs_vnode_cache *chvc;
	dbg("inactive | vno: %llu\n", (unsigned long long)ip->ino);

	KASSERT(VOP_ISLOCKED(vp));

	if (ip->ino) {
		chvc = ip->chvc;
		if (chvc->nlink)
			*((struct vop_inactive_args *) v)->a_recycle = 0;
	} else {
		*((struct vop_inactive_args *) v)->a_recycle = 1;
	}

	VOP_UNLOCK(vp);

	return 0;
}

/* --------------------------------------------------------------------- */

int
chfs_reclaim(void *v)
{
	struct vop_reclaim_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct chfs_inode *ip = VTOI(vp);
	struct chfs_mount *chmp = ip->chmp;
	struct chfs_dirent *fd;

	//dbg("reclaim() | ino: %llu\n", (unsigned long long)ip->ino);
	//mutex_enter(&ip->inode_lock);

	mutex_enter(&chmp->chm_lock_vnocache);
	chfs_vnode_cache_set_state(chmp,
	    ip->chvc, VNO_STATE_CHECKEDABSENT);
	mutex_exit(&chmp->chm_lock_vnocache);

	chfs_update(vp, NULL, NULL, UPDATE_CLOSE);

	if (vp->v_type == VREG || vp->v_type == VLNK || vp->v_type == VCHR ||
	    vp->v_type == VBLK || vp->v_type == VFIFO || vp->v_type == VSOCK)
		chfs_kill_fragtree(&ip->fragtree);

	fd = TAILQ_FIRST(&ip->dents);
	while(fd) {
		TAILQ_REMOVE(&ip->dents, fd, fds);
		chfs_free_dirent(fd);
		fd = TAILQ_FIRST(&ip->dents);
	}
	//mutex_exit(&ip->inode_lock);
	//mutex_destroy(&ip->inode_lock);

	cache_purge(vp);
	if (ip->devvp) {
		vrele(ip->devvp);
		ip->devvp = 0;
	}
	chfs_ihashrem(ip);

	genfs_node_destroy(vp);
	pool_put(&chfs_inode_pool, vp->v_data);
	vp->v_data = NULL;
	return (0);
}

/* --------------------------------------------------------------------- */

int
chfs_advlock(void *v)
{
	//struct vnode *vp = ((struct vop_advlock_args *) v)->a_vp;
	dbg("advlock()\n");
	/*
	  struct chfs_node *node;

	  node = VP_TO_CHFS_NODE(vp);

	  return lf_advlock(v, &node->chn_lockf, node->chn_size);
	*/
	return 0;
}

/* --------------------------------------------------------------------- */
int
chfs_strategy(void *v)
{
	struct vop_strategy_args /* {
				    const struct vnodeop_desc *a_desc;
				    struct vnode *a_vp;
				    struct buf *a_bp;
				    } */ *ap = v;
	struct chfs_full_dnode *fd;
	struct buf *bp = ap->a_bp;
	struct vnode *vp = ap->a_vp;
	struct chfs_inode *ip = VTOI(vp);
	struct chfs_mount *chmp = ip->chmp;
	int read = (bp->b_flags & B_READ) ? 1 : 0;
	int err = 0;

/*	dbg("bp dump:\n");
	dbg("	->b_bcount: %d\n", bp->b_bcount);
	dbg("	->b_resid:  %d\n", bp->b_resid);
	dbg("	->b_blkno:  %llu\n", (unsigned long long)bp->b_blkno);
	dbg("	->b_error:  %d\n", bp->b_error);*/
	if (read) {
		err = chfs_read_data(chmp, vp, bp);
	} else {
		fd = chfs_alloc_full_dnode();

		mutex_enter(&chmp->chm_lock_mountfields);

		err = chfs_write_flash_dnode(chmp, vp, bp, fd);
		if (err) {
			mutex_exit(&chmp->chm_lock_mountfields);
			goto out;
		}

		err = chfs_add_full_dnode_to_inode(chmp, ip, fd);
		/*if (err) {
			mutex_exit(&chmp->chm_lock_mountfields);
			goto out;
		}*/

		mutex_exit(&chmp->chm_lock_mountfields);
	}
out:
	biodone(bp);
	//dbg("end\n");
	return err;
}

int
chfs_bmap(void *v)
{
	struct vop_bmap_args /* {
				struct vnode *a_vp;
				daddr_t  a_bn;
				struct vnode **a_vpp;
				daddr_t *a_bnp;
				int *a_runp;
				int *a_runb;
				} */ *ap = v;
	if (ap->a_vpp != NULL)
		*ap->a_vpp = ap->a_vp;
	if (ap->a_bnp != NULL)
		*ap->a_bnp = ap->a_bn;
	if (ap->a_runp != NULL)
		*ap->a_runp = 0;
	return (0);
}

/*
 * vnode operations vector used for files stored in a chfs file system.
 */
int
(**chfs_vnodeop_p)(void *);
const struct vnodeopv_entry_desc chfs_vnodeop_entries[] =
	{
		{ &vop_default_desc, vn_default_error },
		{ &vop_lookup_desc, chfs_lookup },
		{ &vop_create_desc, chfs_create },
		{ &vop_mknod_desc, chfs_mknod },
		{ &vop_open_desc, chfs_open },
		{ &vop_close_desc, chfs_close },
		{ &vop_access_desc, chfs_access },
		{ &vop_getattr_desc, chfs_getattr },
		{ &vop_setattr_desc, chfs_setattr },
		{ &vop_read_desc, chfs_read },
		{ &vop_write_desc, chfs_write },
		{ &vop_ioctl_desc, genfs_enoioctl },
		{ &vop_fcntl_desc, genfs_fcntl },
		{ &vop_poll_desc, genfs_poll },
		{ &vop_kqfilter_desc, genfs_kqfilter },
		{ &vop_revoke_desc, genfs_revoke },
		{ &vop_mmap_desc, genfs_mmap },
		{ &vop_fsync_desc, chfs_fsync },
		{ &vop_seek_desc, genfs_seek },
		{ &vop_remove_desc, chfs_remove },
		{ &vop_link_desc, chfs_link },
		{ &vop_rename_desc, chfs_rename },
		{ &vop_mkdir_desc, chfs_mkdir },
		{ &vop_rmdir_desc, chfs_rmdir },
		{ &vop_symlink_desc, chfs_symlink },
		{ &vop_readdir_desc, chfs_readdir },
		{ &vop_readlink_desc, chfs_readlink },
		{ &vop_abortop_desc, genfs_abortop },
		{ &vop_inactive_desc, chfs_inactive },
		{ &vop_reclaim_desc, chfs_reclaim },
		{ &vop_lock_desc, genfs_lock },
		{ &vop_unlock_desc, genfs_unlock },
		{ &vop_bmap_desc, chfs_bmap },
		{ &vop_strategy_desc, chfs_strategy },
		{ &vop_print_desc, ufs_print },
		{ &vop_pathconf_desc, ufs_pathconf },
		{ &vop_islocked_desc, genfs_islocked },
		{ &vop_advlock_desc, chfs_advlock },
		{ &vop_bwrite_desc, vn_bwrite },
		{ &vop_getpages_desc, genfs_getpages },
		{ &vop_putpages_desc, genfs_putpages },
		{ NULL, NULL } };

const struct vnodeopv_desc chfs_vnodeop_opv_desc =
	{ &chfs_vnodeop_p, chfs_vnodeop_entries };

/* --------------------------------------------------------------------- */

/*
 * vnode operations vector used for special devices stored in a chfs
 * file system.
 */
int
(**chfs_specop_p)(void *);
const struct vnodeopv_entry_desc chfs_specop_entries[] =
	{
		{ &vop_default_desc, vn_default_error },
		{ &vop_lookup_desc, spec_lookup },
		{ &vop_create_desc, spec_create },
		{ &vop_mknod_desc, spec_mknod },
		{ &vop_open_desc, spec_open },
		{ &vop_close_desc, ufsspec_close },
		{ &vop_access_desc, chfs_access },
		{ &vop_getattr_desc, chfs_getattr },
		{ &vop_setattr_desc, chfs_setattr },
		{ &vop_read_desc, chfs_read },
		{ &vop_write_desc, chfs_write },
		{ &vop_ioctl_desc, spec_ioctl },
		{ &vop_fcntl_desc, genfs_fcntl },
		{ &vop_poll_desc, spec_poll },
		{ &vop_kqfilter_desc, spec_kqfilter },
		{ &vop_revoke_desc, spec_revoke },
		{ &vop_mmap_desc, spec_mmap },
		{ &vop_fsync_desc, spec_fsync },
		{ &vop_seek_desc, spec_seek },
		{ &vop_remove_desc, spec_remove },
		{ &vop_link_desc, spec_link },
		{ &vop_rename_desc, spec_rename },
		{ &vop_mkdir_desc, spec_mkdir },
		{ &vop_rmdir_desc, spec_rmdir },
		{ &vop_symlink_desc, spec_symlink },
		{ &vop_readdir_desc, spec_readdir },
		{ &vop_readlink_desc, spec_readlink },
		{ &vop_abortop_desc, spec_abortop },
		{ &vop_inactive_desc, chfs_inactive },
		{ &vop_reclaim_desc, chfs_reclaim },
		{ &vop_lock_desc, genfs_lock },
		{ &vop_unlock_desc, genfs_unlock },
		{ &vop_bmap_desc, spec_bmap },
		{ &vop_strategy_desc, spec_strategy },
		{ &vop_print_desc, ufs_print },
		{ &vop_pathconf_desc, spec_pathconf },
		{ &vop_islocked_desc, genfs_islocked },
		{ &vop_advlock_desc, spec_advlock },
		{ &vop_bwrite_desc, vn_bwrite },
		{ &vop_getpages_desc, spec_getpages },
		{ &vop_putpages_desc, spec_putpages },
		{ NULL, NULL } };

const struct vnodeopv_desc chfs_specop_opv_desc =
	{ &chfs_specop_p, chfs_specop_entries };

/* --------------------------------------------------------------------- */
/*
 * vnode operations vector used for fifos stored in a chfs file system.
 */
int
(**chfs_fifoop_p)(void *);
const struct vnodeopv_entry_desc chfs_fifoop_entries[] =
	{
		{ &vop_default_desc, vn_default_error },
		{ &vop_lookup_desc, vn_fifo_bypass },
		{ &vop_create_desc, vn_fifo_bypass },
		{ &vop_mknod_desc, vn_fifo_bypass },
		{ &vop_open_desc, vn_fifo_bypass },
		{ &vop_close_desc, ufsfifo_close },
		{ &vop_access_desc, chfs_access },
		{ &vop_getattr_desc, chfs_getattr },
		{ &vop_setattr_desc, chfs_setattr },
		{ &vop_read_desc, ufsfifo_read },
		{ &vop_write_desc, ufsfifo_write },
		{ &vop_ioctl_desc, vn_fifo_bypass },
		{ &vop_fcntl_desc, genfs_fcntl },
		{ &vop_poll_desc, vn_fifo_bypass },
		{ &vop_kqfilter_desc, vn_fifo_bypass },
		{ &vop_revoke_desc, vn_fifo_bypass },
		{ &vop_mmap_desc, vn_fifo_bypass },
		{ &vop_fsync_desc, vn_fifo_bypass },
		{ &vop_seek_desc, vn_fifo_bypass },
		{ &vop_remove_desc, vn_fifo_bypass },
		{ &vop_link_desc, vn_fifo_bypass },
		{ &vop_rename_desc, vn_fifo_bypass },
		{ &vop_mkdir_desc, vn_fifo_bypass },
		{ &vop_rmdir_desc, vn_fifo_bypass },
		{ &vop_symlink_desc, vn_fifo_bypass },
		{ &vop_readdir_desc, vn_fifo_bypass },
		{ &vop_readlink_desc, vn_fifo_bypass },
		{ &vop_abortop_desc, vn_fifo_bypass },
		{ &vop_inactive_desc, chfs_inactive },
		{ &vop_reclaim_desc, chfs_reclaim },
		{ &vop_lock_desc, genfs_lock },
		{ &vop_unlock_desc, genfs_unlock },
		{ &vop_bmap_desc, vn_fifo_bypass },
		{ &vop_strategy_desc, vn_fifo_bypass },
		{ &vop_print_desc, ufs_print },
		{ &vop_pathconf_desc, vn_fifo_bypass },
		{ &vop_islocked_desc, genfs_islocked },
		{ &vop_advlock_desc, vn_fifo_bypass },
		{ &vop_bwrite_desc, genfs_nullop },
		{ &vop_getpages_desc, genfs_badop },
		{ &vop_putpages_desc, vn_fifo_bypass },
		{ NULL, NULL } };

const struct vnodeopv_desc chfs_fifoop_opv_desc =
	{ &chfs_fifoop_p, chfs_fifoop_entries };
