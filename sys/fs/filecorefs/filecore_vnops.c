/*	$NetBSD: filecore_vnops.c,v 1.44 2015/04/20 23:03:08 riastradh Exp $	*/

/*-
 * Copyright (c) 1994 The Regents of the University of California.
 * All rights reserved.
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
 *	filecore_vnops.c	1.2	1998/8/18
 */

/*-
 * Copyright (c) 1998 Andrew McMurry
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
 *	filecore_vnops.c	1.2	1998/8/18
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: filecore_vnops.c,v 1.44 2015/04/20 23:03:08 riastradh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/resourcevar.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/dirent.h>
#include <sys/kauth.h>

#include <miscfs/genfs/genfs.h>
#include <miscfs/specfs/specdev.h>

#include <fs/filecorefs/filecore.h>
#include <fs/filecorefs/filecore_extern.h>
#include <fs/filecorefs/filecore_node.h>

static int
filecore_check_possible(struct vnode *vp, struct filecore_node *ip,
    mode_t mode)
{

	/*
	 * Disallow write attempts unless the file is a socket,
	 * fifo, or a block or character device resident on the
	 * file system.
	 */
	if (mode & VWRITE) {
		switch (vp->v_type) {
		case VDIR:
		case VLNK:
		case VREG:
			return (EROFS);
		default:
			break;
		}
	}

	return 0;
}

/*
 * Check mode permission on inode pointer. Mode is READ, WRITE or EXEC.
 * The mode is shifted to select the owner/group/other fields. The
 * super user is granted all permissions.
 */
static int
filecore_check_permitted(struct vnode *vp, struct filecore_node *ip,
    mode_t mode, kauth_cred_t cred)
{
	struct filecore_mnt *fcmp = ip->i_mnt;

	return kauth_authorize_vnode(cred, KAUTH_ACCESS_ACTION(mode,
	    vp->v_type, filecore_mode(ip)), vp, NULL,
	    genfs_can_access(vp->v_type, filecore_mode(ip), fcmp->fc_uid,
	    fcmp->fc_gid, mode, cred));
}

int
filecore_access(void *v)
{
	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct filecore_node *ip = VTOI(vp);
	int error;

	error = filecore_check_possible(vp, ip, ap->a_mode);
	if (error)
		return error;

	error = filecore_check_permitted(vp, ip, ap->a_mode, ap->a_cred);

	return error;
}

int
filecore_getattr(void *v)
{
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct filecore_node *ip = VTOI(vp);
	struct vattr *vap = ap->a_vap;
	struct filecore_mnt *fcmp = ip->i_mnt;

	vap->va_fsid	= ip->i_dev;
	vap->va_fileid	= ip->i_number;

	vap->va_mode	= filecore_mode(ip);
	vap->va_nlink	= 1;
	vap->va_uid	= fcmp->fc_uid;
	vap->va_gid	= fcmp->fc_gid;
	vap->va_atime	= filecore_time(ip);
	vap->va_mtime	= filecore_time(ip);
	vap->va_ctime	= filecore_time(ip);
	vap->va_rdev	= 0;  /* We don't support specials */

	vap->va_size	= (u_quad_t) ip->i_size;
	vap->va_flags	= 0;
	vap->va_gen	= 1;
	vap->va_blocksize = fcmp->blksize;
	vap->va_bytes	= vap->va_size;
	vap->va_type	= vp->v_type;
	return (0);
}

/*
 * Vnode op for reading.
 */
int
filecore_read(void *v)
{
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	struct filecore_node *ip = VTOI(vp);
	struct filecore_mnt *fcmp;
	struct buf *bp;
	daddr_t lbn;
	off_t diff;
	int error = 0;
	long size, n, on;

	if (uio->uio_resid == 0)
		return (0);
	if (uio->uio_offset < 0)
		return (EINVAL);
	if (uio->uio_offset >= ip->i_size)
		return (0);
	ip->i_flag |= IN_ACCESS;
	fcmp = ip->i_mnt;

	if (vp->v_type == VREG) {
		const int advice = IO_ADV_DECODE(ap->a_ioflag);
		error = 0;

		while (uio->uio_resid > 0) {
			vsize_t bytelen = MIN(ip->i_size - uio->uio_offset,
					      uio->uio_resid);

			if (bytelen == 0) {
				break;
			}
			error = ubc_uiomove(&vp->v_uobj, uio, bytelen, advice,
			    UBC_READ | UBC_PARTIALOK | UBC_UNMAP_FLAG(vp));
			if (error) {
				break;
			}
		}
		goto out;
	}

	do {
		lbn = filecore_lblkno(fcmp, uio->uio_offset);
		on = filecore_blkoff(fcmp, uio->uio_offset);
		n = MIN(filecore_blksize(fcmp, ip, lbn) - on, uio->uio_resid);
		diff = (off_t)ip->i_size - uio->uio_offset;
		if (diff <= 0)
			return (0);
		if (diff < n)
			n = diff;
		size = filecore_blksize(fcmp, ip, lbn);
		if (ip->i_dirent.attr & FILECORE_ATTR_DIR) {
			error = filecore_dbread(ip, &bp);
			on = uio->uio_offset;
			n = MIN(FILECORE_DIR_SIZE - on, uio->uio_resid);
			size = FILECORE_DIR_SIZE;
		} else {
			error = bread(vp, lbn, size, 0, &bp);
#ifdef FILECORE_DEBUG_BR
			printf("bread(%p, %llx, %ld, CRED, %p)=%d\n",
			    vp, (long long)lbn, size, bp, error);
#endif
		}
		if (error) {
			return (error);
		}
		n = MIN(n, size - bp->b_resid);

		error = uiomove((char *)(bp->b_data) + on, (int)n, uio);
#ifdef FILECORE_DEBUG_BR
		printf("brelse(%p) vn2\n", bp);
#endif
		brelse(bp, 0);
	} while (error == 0 && uio->uio_resid > 0 && n != 0);

out:
	return (error);
}

/*
 * Vnode op for readdir
 */
int
filecore_readdir(void *v)
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
	struct vnode *vdp = ap->a_vp;
	struct filecore_node *dp;
	struct buf *bp = NULL;
	struct dirent *de;
	struct filecore_direntry *dep = NULL;
	int error = 0;
	off_t *cookies = NULL;
	int ncookies = 0;
	int i;
	off_t uiooff;

	dp = VTOI(vdp);

	if ((dp->i_dirent.attr & FILECORE_ATTR_DIR) == 0)
		return (ENOTDIR);

	if (uio->uio_offset % FILECORE_DIRENT_SIZE != 0)
		return (EINVAL);
	i = uio->uio_offset / FILECORE_DIRENT_SIZE;
	uiooff = uio->uio_offset;

	*ap->a_eofflag = 0;

	error = filecore_dbread(dp, &bp);
	if (error) {
		return error;
	}

	if (ap->a_ncookies == NULL)
		cookies = NULL;
	else {
		*ap->a_ncookies = 0;
		ncookies = uio->uio_resid / _DIRENT_MINSIZE((struct dirent *)0);
		cookies = malloc(ncookies * sizeof(off_t), M_TEMP, M_WAITOK);
	}

	de = kmem_zalloc(sizeof(struct dirent), KM_SLEEP);

	for (; ; i++) {
		switch (i) {
		case 0:
			/* Fake the '.' entry */
			de->d_fileno = dp->i_number;
			de->d_type = DT_DIR;
			de->d_namlen = 1;
			strlcpy(de->d_name, ".", sizeof(de->d_name));
			break;
		case 1:
			/* Fake the '..' entry */
			de->d_fileno = filecore_getparent(dp);
			de->d_type = DT_DIR;
			de->d_namlen = 2;
			strlcpy(de->d_name, "..", sizeof(de->d_name));
			break;
		default:
			de->d_fileno = dp->i_dirent.addr +
					((i - 2) << FILECORE_INO_INDEX);
			dep = fcdirentry(bp->b_data, i - 2);
			if (dep->attr & FILECORE_ATTR_DIR)
				de->d_type = DT_DIR;
			else
				de->d_type = DT_REG;
			if (filecore_fn2unix(dep->name, de->d_name,
/*###346 [cc] warning: passing arg 3 of `filecore_fn2unix' from incompatible pointer type%%%*/
			    &de->d_namlen)) {
				*ap->a_eofflag = 1;
				goto out;
			}
			break;
		}
		de->d_reclen = _DIRENT_SIZE(de);
		if (uio->uio_resid < de->d_reclen)
			goto out;
		error = uiomove(de, de->d_reclen, uio);
		if (error)
			goto out;
		uiooff += FILECORE_DIRENT_SIZE;

		if (cookies) {
			*cookies++ = i*FILECORE_DIRENT_SIZE;
			(*ap->a_ncookies)++;
			if (--ncookies == 0) goto out;
		}
	}
out:
	if (cookies) {
		*ap->a_cookies = cookies;
		if (error) {
			free(cookies, M_TEMP);
			*ap->a_ncookies = 0;
			*ap->a_cookies = NULL;
		}
	}
	uio->uio_offset = uiooff;

#ifdef FILECORE_DEBUG_BR
	printf("brelse(%p) vn3\n", bp);
#endif
	brelse (bp, 0);

	kmem_free(de, sizeof(*de));

	return (error);
}

/*
 * Return target name of a symbolic link
 * Shouldn't we get the parent vnode and read the data from there?
 * This could eventually result in deadlocks in filecore_lookup.
 * But otherwise the block read here is in the block buffer two times.
 */
int
filecore_readlink(void *v)
{
#if 0
	struct vop_readlink_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		kauth_cred_t a_cred;
	} */ *ap = v;
#endif

	return (EINVAL);
}

int
filecore_link(void *v)
{
	struct vop_link_v2_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;

	VOP_ABORTOP(ap->a_dvp, ap->a_cnp);
	return (EROFS);
}

int
filecore_symlink(void *v)
{
	struct vop_symlink_v3_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		char *a_target;
	} */ *ap = v;

	VOP_ABORTOP(ap->a_dvp, ap->a_cnp);
	return (EROFS);
}

/*
 * Calculate the logical to physical mapping if not done already,
 * then call the device strategy routine.
 */
int
filecore_strategy(void *v)
{
	struct vop_strategy_args /* {
		struct vnode *a_vp;
		struct buf *a_bp;
	} */ *ap = v;
	struct buf *bp = ap->a_bp;
	struct vnode *vp = ap->a_vp;
	struct filecore_node *ip;
	int error;

	ip = VTOI(vp);
	if (bp->b_blkno == bp->b_lblkno) {
		error = VOP_BMAP(vp, bp->b_lblkno, NULL, &bp->b_blkno, NULL);
		if (error) {
			bp->b_error = error;
			biodone(bp);
			return (error);
		}
		if ((long)bp->b_blkno == -1)
			clrbuf(bp);
	}
	if ((long)bp->b_blkno == -1) {
		biodone(bp);
		return (0);
	}
	vp = ip->i_devvp;
	return (VOP_STRATEGY(vp, bp));
}

/*
 * Print out the contents of an inode.
 */
/*ARGSUSED*/
int
filecore_print(void *v)
{

	printf("tag VT_FILECORE, filecore vnode\n");
	return (0);
}

/*
 * Return POSIX pathconf information applicable to filecore filesystems.
 */
int
filecore_pathconf(void *v)
{
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		register_t *a_retval;
	} */ *ap = v;
	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = 1;
		return (0);
	case _PC_NAME_MAX:
		*ap->a_retval = ap->a_vp->v_mount->mnt_stat.f_namemax;
		return (0);
	case _PC_PATH_MAX:
		*ap->a_retval = 256;
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
		*ap->a_retval = 32;
		return (0);
	default:
		return (EINVAL);
	}
	/* NOTREACHED */
}

/*
 * Global vfs data structures for isofs
 */
#define	filecore_create	genfs_eopnotsupp
#define	filecore_mknod	genfs_eopnotsupp
#define	filecore_write	genfs_eopnotsupp
#define	filecore_setattr	genfs_eopnotsupp
#define	filecore_fcntl	genfs_fcntl
#define	filecore_ioctl	genfs_enoioctl
#define	filecore_fsync	genfs_nullop
#define	filecore_remove	genfs_eopnotsupp
#define	filecore_rename	genfs_eopnotsupp
#define	filecore_mkdir	genfs_eopnotsupp
#define	filecore_rmdir	genfs_eopnotsupp
#define	filecore_advlock	genfs_eopnotsupp
#define	filecore_bwrite	genfs_eopnotsupp
#define filecore_revoke	genfs_revoke

/*
 * Global vfs data structures for filecore
 */
int (**filecore_vnodeop_p)(void *);
const struct vnodeopv_entry_desc filecore_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, filecore_lookup },		/* lookup */
	{ &vop_create_desc, filecore_create },		/* create */
	{ &vop_mknod_desc, filecore_mknod },		/* mknod */
	{ &vop_open_desc, filecore_open },		/* open */
	{ &vop_close_desc, filecore_close },		/* close */
	{ &vop_access_desc, filecore_access },		/* access */
	{ &vop_getattr_desc, filecore_getattr },	/* getattr */
	{ &vop_setattr_desc, filecore_setattr },	/* setattr */
	{ &vop_read_desc, filecore_read },		/* read */
	{ &vop_write_desc, filecore_write },		/* write */
	{ &vop_fallocate_desc, genfs_eopnotsupp },	/* fallocate */
	{ &vop_fdiscard_desc, genfs_eopnotsupp },	/* fdiscard */
	{ &vop_fcntl_desc, filecore_fcntl },		/* fcntl */
	{ &vop_ioctl_desc, filecore_ioctl },		/* ioctl */
	{ &vop_poll_desc, filecore_poll },		/* poll */
	{ &vop_kqfilter_desc, genfs_kqfilter },		/* kqfilter */
	{ &vop_revoke_desc, filecore_revoke },		/* revoke */
	{ &vop_mmap_desc, filecore_mmap },		/* mmap */
	{ &vop_fsync_desc, filecore_fsync },		/* fsync */
	{ &vop_seek_desc, filecore_seek },		/* seek */
	{ &vop_remove_desc, filecore_remove },		/* remove */
	{ &vop_link_desc, filecore_link },		/* link */
	{ &vop_rename_desc, filecore_rename },		/* rename */
	{ &vop_mkdir_desc, filecore_mkdir },		/* mkdir */
	{ &vop_rmdir_desc, filecore_rmdir },		/* rmdir */
	{ &vop_symlink_desc, filecore_symlink },	/* symlink */
	{ &vop_readdir_desc, filecore_readdir },      	/* readdir */
	{ &vop_readlink_desc, filecore_readlink },	/* readlink */
	{ &vop_abortop_desc, filecore_abortop },       	/* abortop */
	{ &vop_inactive_desc, filecore_inactive },	/* inactive */
	{ &vop_reclaim_desc, filecore_reclaim },       	/* reclaim */
	{ &vop_lock_desc, genfs_lock },			/* lock */
	{ &vop_unlock_desc, genfs_unlock },		/* unlock */
	{ &vop_bmap_desc, filecore_bmap },		/* bmap */
	{ &vop_strategy_desc, filecore_strategy },	/* strategy */
	{ &vop_print_desc, filecore_print },		/* print */
	{ &vop_islocked_desc, genfs_islocked },		/* islocked */
	{ &vop_pathconf_desc, filecore_pathconf },	/* pathconf */
	{ &vop_advlock_desc, filecore_advlock },       	/* advlock */
	{ &vop_bwrite_desc, vn_bwrite },		/* bwrite */
	{ &vop_getpages_desc, genfs_getpages },		/* getpages */
	{ &vop_putpages_desc, genfs_putpages },		/* putpages */
	{ NULL, NULL }
};
const struct vnodeopv_desc filecore_vnodeop_opv_desc =
	{ &filecore_vnodeop_p, filecore_vnodeop_entries };
