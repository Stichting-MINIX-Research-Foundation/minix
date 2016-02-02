/*	$NetBSD: cd9660_vnops.c,v 1.52 2015/04/20 23:03:07 riastradh Exp $	*/

/*-
 * Copyright (c) 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley
 * by Pace Willisson (pace@blitz.com).  The Rock Ridge Extension
 * Support code is derived from software contributed to Berkeley
 * by Atsushi Murai (amurai@spec.co.jp).
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
 *	@(#)cd9660_vnops.c	8.15 (Berkeley) 5/27/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cd9660_vnops.c,v 1.52 2015/04/20 23:03:07 riastradh Exp $");

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
#include <sys/kauth.h>

#include <miscfs/fifofs/fifo.h>
#include <miscfs/genfs/genfs.h>
#include <miscfs/specfs/specdev.h>

#include <fs/cd9660/iso.h>
#include <fs/cd9660/cd9660_extern.h>
#include <fs/cd9660/cd9660_node.h>
#include <fs/cd9660/iso_rrip.h>
#include <fs/cd9660/cd9660_mount.h>

/*
 * Structure for reading directories
 */
struct isoreaddir {
	struct dirent saveent;
	struct dirent assocent;
	struct dirent current;
	off_t saveoff;
	off_t assocoff;
	off_t curroff;
	struct uio *uio;
	off_t uio_off;
	int eofflag;
	off_t *cookies;
	int ncookies;
};

int	iso_uiodir(struct isoreaddir *, struct dirent *, off_t);
int	iso_shipdir(struct isoreaddir *);

static int
cd9660_check_possible(struct vnode *vp, struct iso_node *ip, mode_t mode)
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
cd9660_check_permitted(struct vnode *vp, struct iso_node *ip, mode_t mode,
    kauth_cred_t cred)
{

	return kauth_authorize_vnode(cred, KAUTH_ACCESS_ACTION(mode,
	    vp->v_type, ip->inode.iso_mode & ALLPERMS), vp, NULL,
	    genfs_can_access(vp->v_type, ip->inode.iso_mode & ALLPERMS,
	    ip->inode.iso_uid, ip->inode.iso_gid, mode, cred));
}

int
cd9660_access(void *v)
{
	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct iso_node *ip = VTOI(vp);
	int error;

	error = cd9660_check_possible(vp, ip, ap->a_mode);
	if (error)
		return error;

	error = cd9660_check_permitted(vp, ip, ap->a_mode, ap->a_cred);

	return error;
}

int
cd9660_getattr(void *v)
{
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct iso_node *ip = VTOI(vp);
	struct vattr *vap = ap->a_vap;

	vap->va_fsid	= ip->i_dev;
	vap->va_fileid	= ip->i_number;

	vap->va_mode	= ip->inode.iso_mode & ALLPERMS;
	vap->va_nlink	= ip->inode.iso_links;
	vap->va_uid	= ip->inode.iso_uid;
	vap->va_gid	= ip->inode.iso_gid;
	vap->va_atime	= ip->inode.iso_atime;
	vap->va_mtime	= ip->inode.iso_mtime;
	vap->va_ctime	= ip->inode.iso_ctime;
	vap->va_rdev	= ip->inode.iso_rdev;

	vap->va_size	= (u_quad_t) ip->i_size;
	if (ip->i_size == 0 && vp->v_type == VLNK) {
		struct vop_readlink_args rdlnk;
		struct iovec aiov;
		struct uio auio;
		char *cp;

		cp = (char *)malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
		aiov.iov_base = cp;
		aiov.iov_len = MAXPATHLEN;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = 0;
		auio.uio_rw = UIO_READ;
		auio.uio_resid = MAXPATHLEN;
		UIO_SETUP_SYSSPACE(&auio);
		rdlnk.a_uio = &auio;
		rdlnk.a_vp = ap->a_vp;
		rdlnk.a_cred = ap->a_cred;
		if (cd9660_readlink(&rdlnk) == 0)
			vap->va_size = MAXPATHLEN - auio.uio_resid;
		free(cp, M_TEMP);
	}
	vap->va_flags	= 0;
	vap->va_gen = 1;
	vap->va_blocksize = ip->i_mnt->logical_block_size;
	vap->va_bytes	= (u_quad_t) ip->i_size;
	vap->va_type	= vp->v_type;
	return (0);
}

/*
 * Vnode op for reading.
 */
int
cd9660_read(void *v)
{
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	struct iso_node *ip = VTOI(vp);
	struct iso_mnt *imp;
	struct buf *bp;
	daddr_t lbn, rablock;
	off_t diff;
	int rasize, error = 0;
	long size, n, on;

	if (uio->uio_resid == 0)
		return (0);
	if (uio->uio_offset < 0)
		return (EINVAL);
	if (uio->uio_offset >= ip->i_size)
		return 0;
	ip->i_flag |= IN_ACCESS;
	imp = ip->i_mnt;

	if (vp->v_type == VREG) {
		const int advice = IO_ADV_DECODE(ap->a_ioflag);
		error = 0;

		while (uio->uio_resid > 0) {
			vsize_t bytelen = MIN(ip->i_size - uio->uio_offset,
					      uio->uio_resid);

			if (bytelen == 0)
				break;
			error = ubc_uiomove(&vp->v_uobj, uio, bytelen, advice,
			    UBC_READ | UBC_PARTIALOK | UBC_UNMAP_FLAG(vp));
			if (error)
				break;
		}
		goto out;
	}

	do {
		lbn = cd9660_lblkno(imp, uio->uio_offset);
		on = cd9660_blkoff(imp, uio->uio_offset);
		n = MIN(imp->logical_block_size - on, uio->uio_resid);
		diff = (off_t)ip->i_size - uio->uio_offset;
		if (diff <= 0)
			return (0);
		if (diff < n)
			n = diff;
		size = cd9660_blksize(imp, ip, lbn);
		rablock = lbn + 1;
		if (cd9660_lblktosize(imp, rablock) < ip->i_size) {
			rasize = cd9660_blksize(imp, ip, rablock);
			error = breadn(vp, lbn, size, &rablock,
				       &rasize, 1, 0, &bp);
		} else {
			error = bread(vp, lbn, size, 0, &bp);
		}
		if (error) {
			return (error);
		}
		n = MIN(n, size - bp->b_resid);

		error = uiomove((char *)bp->b_data + on, (int)n, uio);
		brelse(bp, 0);
	} while (error == 0 && uio->uio_resid > 0 && n != 0);

out:
	return (error);
}

int
iso_uiodir(struct isoreaddir *idp, struct dirent *dp, off_t off)
{
	int error;

	dp->d_name[dp->d_namlen] = 0;
	dp->d_reclen = _DIRENT_SIZE(dp);

	if (idp->uio->uio_resid < dp->d_reclen) {
		idp->eofflag = 0;
		return (-1);
	}

	if (idp->cookies) {
		if (idp->ncookies <= 0) {
			idp->eofflag = 0;
			return (-1);
		}

		*idp->cookies++ = off;
		--idp->ncookies;
	}

	if ((error = uiomove(dp, dp->d_reclen, idp->uio)) != 0)
		return (error);
	idp->uio_off = off;
	return (0);
}

int
iso_shipdir(struct isoreaddir *idp)
{
	struct dirent *dp;
	int cl, sl, assoc;
	int error;
	char *cname, *sname;

	cl = idp->current.d_namlen;
	cname = idp->current.d_name;

	if ((assoc = cl > 1 && *cname == ASSOCCHAR)) {
		cl--;
		cname++;
	}

	dp = &idp->saveent;
	sname = dp->d_name;
	if (!(sl = dp->d_namlen)) {
		dp = &idp->assocent;
		sname = dp->d_name + 1;
		sl = dp->d_namlen - 1;
	}
	if (sl > 0) {
		if (sl != cl
		    || memcmp(sname, cname, sl)) {
			if (idp->assocent.d_namlen) {
				error = iso_uiodir(idp, &idp->assocent,
						   idp->assocoff);
				if (error)
					return (error);
				idp->assocent.d_namlen = 0;
			}
			if (idp->saveent.d_namlen) {
				error = iso_uiodir(idp, &idp->saveent,
						   idp->saveoff);
				if (error)
					return (error);
				idp->saveent.d_namlen = 0;
			}
		}
	}
	idp->current.d_reclen = _DIRENT_SIZE(&idp->current);
	if (assoc) {
		idp->assocoff = idp->curroff;
		memcpy(&idp->assocent, &idp->current, idp->current.d_reclen);
	} else {
		idp->saveoff = idp->curroff;
		memcpy(&idp->saveent, &idp->current, idp->current.d_reclen);
	}
	return (0);
}

/*
 * Vnode op for readdir
 */
int
cd9660_readdir(void *v)
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
	struct isoreaddir *idp;
	struct vnode *vdp = ap->a_vp;
	struct iso_node *dp;
	struct iso_mnt *imp;
	struct buf *bp = NULL;
	struct iso_directory_record *ep;
	int entryoffsetinblock;
	doff_t endsearch;
	u_long bmask;
	int error = 0;
	int reclen;
	u_short namelen;
	off_t *cookies = NULL;
	int ncookies = 0;

	if (vdp->v_type != VDIR)
		return (ENOTDIR);

	dp = VTOI(vdp);
	imp = dp->i_mnt;
	bmask = imp->im_bmask;

	idp = (struct isoreaddir *)malloc(sizeof(*idp), M_TEMP, M_WAITOK);
	idp->saveent.d_namlen = idp->assocent.d_namlen = 0;
	/*
	 * XXX
	 * Is it worth trying to figure out the type?
	 */
	idp->saveent.d_type = idp->assocent.d_type = idp->current.d_type =
	    DT_UNKNOWN;
	idp->uio = uio;
	if (ap->a_ncookies == NULL)
		idp->cookies = NULL;
	else {
		ncookies = uio->uio_resid / _DIRENT_MINSIZE((struct dirent *)0);
		cookies = malloc(ncookies * sizeof(off_t), M_TEMP, M_WAITOK);
		idp->cookies = cookies;
		idp->ncookies = ncookies;
	}
	idp->eofflag = 1;
	idp->curroff = uio->uio_offset;

	if ((entryoffsetinblock = idp->curroff & bmask) &&
	    (error = cd9660_blkatoff(vdp, (off_t)idp->curroff, NULL, &bp))) {
		free(idp, M_TEMP);
		return (error);
	}
	endsearch = dp->i_size;

	while (idp->curroff < endsearch) {
		/*
		 * If offset is on a block boundary,
		 * read the next directory block.
		 * Release previous if it exists.
		 */
		if ((idp->curroff & bmask) == 0) {
			if (bp != NULL)
				brelse(bp, 0);
			error = cd9660_blkatoff(vdp, (off_t)idp->curroff,
					     NULL, &bp);
			if (error)
				break;
			entryoffsetinblock = 0;
		}
		/*
		 * Get pointer to next entry.
		 */
		KASSERT(bp != NULL);
		ep = (struct iso_directory_record *)
			((char *)bp->b_data + entryoffsetinblock);

		reclen = isonum_711(ep->length);
		if (reclen == 0) {
			/* skip to next block, if any */
			idp->curroff =
			    (idp->curroff & ~bmask) + imp->logical_block_size;
			continue;
		}

		if (reclen < ISO_DIRECTORY_RECORD_SIZE) {
			error = EINVAL;
			/* illegal entry, stop */
			break;
		}

		if (entryoffsetinblock + reclen > imp->logical_block_size) {
			error = EINVAL;
			/* illegal directory, so stop looking */
			break;
		}

		idp->current.d_namlen = isonum_711(ep->name_len);

		if (reclen < ISO_DIRECTORY_RECORD_SIZE + idp->current.d_namlen) {
			error = EINVAL;
			/* illegal entry, stop */
			break;
		}

		if (isonum_711(ep->flags)&2)
			idp->current.d_fileno = isodirino(ep, imp);
		else
			idp->current.d_fileno = dbtob(bp->b_blkno) +
				entryoffsetinblock;

		idp->curroff += reclen;

		switch (imp->iso_ftype) {
		case ISO_FTYPE_RRIP:
			cd9660_rrip_getname(ep, idp->current.d_name, &namelen,
			    &idp->current.d_fileno, imp);
			idp->current.d_namlen = (u_char)namelen;
			if (idp->current.d_namlen)
				error = iso_uiodir(idp, &idp->current,
				    idp->curroff);
			break;
		default:	/* ISO_FTYPE_DEFAULT || ISO_FTYPE_9660 */
			isofntrans(ep->name, idp->current.d_namlen,
				   idp->current.d_name, &namelen,
				   imp->iso_ftype == ISO_FTYPE_9660,
				   (imp->im_flags & ISOFSMNT_NOCASETRANS) == 0,
				   isonum_711(ep->flags)&4,
				   imp->im_joliet_level);
			switch (idp->current.d_name[0]) {
			case 0:
				idp->current.d_name[0] = '.';
				idp->current.d_namlen = 1;
				error = iso_uiodir(idp, &idp->current,
				    idp->curroff);
				break;
			case 1:
				strlcpy(idp->current.d_name, "..",
				    sizeof(idp->current.d_name));
				idp->current.d_namlen = 2;
				error = iso_uiodir(idp, &idp->current,
				    idp->curroff);
				break;
			default:
				idp->current.d_namlen = (u_char)namelen;
				if (imp->iso_ftype == ISO_FTYPE_DEFAULT)
					error = iso_shipdir(idp);
				else
					error = iso_uiodir(idp, &idp->current,
					    idp->curroff);
				break;
			}
		}
		if (error)
			break;

		entryoffsetinblock += reclen;
	}

	if (!error && imp->iso_ftype == ISO_FTYPE_DEFAULT) {
		idp->current.d_namlen = 0;
		error = iso_shipdir(idp);
	}
	if (error < 0)
		error = 0;

	if (ap->a_ncookies != NULL) {
		if (error)
			free(cookies, M_TEMP);
		else {
			/*
			 * Work out the number of cookies actually used.
			 */
			*ap->a_ncookies = ncookies - idp->ncookies;
			*ap->a_cookies = cookies;
		}
	}

	if (bp)
		brelse(bp, 0);

	uio->uio_offset = idp->uio_off;
	*ap->a_eofflag = idp->eofflag;

	free(idp, M_TEMP);

	return (error);
}

/*
 * Return target name of a symbolic link
 * Shouldn't we get the parent vnode and read the data from there?
 * This could eventually result in deadlocks in cd9660_lookup.
 * But otherwise the block read here is in the block buffer two times.
 */
typedef struct iso_directory_record ISODIR;
typedef struct iso_node             ISONODE;
typedef struct iso_mnt              ISOMNT;

int
cd9660_readlink(void *v)
{
	struct vop_readlink_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		kauth_cred_t a_cred;
	} */ *ap = v;
	ISONODE	*ip;
	ISODIR	*dirp;
	ISOMNT	*imp;
	struct	buf *bp;
	struct	uio *uio;
	u_short	symlen;
	int	error;
	char	*symname;
	bool use_pnbuf;

	ip  = VTOI(ap->a_vp);
	imp = ip->i_mnt;
	uio = ap->a_uio;

	if (imp->iso_ftype != ISO_FTYPE_RRIP)
		return (EINVAL);

	/*
	 * Get parents directory record block that this inode included.
	 */
	error = bread(imp->im_devvp,
		      (ip->i_number >> imp->im_bshift) <<
		      (imp->im_bshift - DEV_BSHIFT),
		      imp->logical_block_size, 0, &bp);
	if (error) {
		return (EINVAL);
	}

	/*
	 * Setup the directory pointer for this inode
	 */
	dirp = (ISODIR *)((char *)bp->b_data + (ip->i_number & imp->im_bmask));

	/*
	 * Just make sure, we have a right one....
	 *   1: Check not cross boundary on block
	 */
	if ((ip->i_number & imp->im_bmask) + isonum_711(dirp->length)
	    > imp->logical_block_size) {
		brelse(bp, 0);
		return (EINVAL);
	}

	/*
	 * Now get a buffer
	 * Abuse a namei buffer for now.
	 */
	use_pnbuf = !VMSPACE_IS_KERNEL_P(uio->uio_vmspace) ||
	    uio->uio_iov->iov_len < MAXPATHLEN;
	if (use_pnbuf) {
		symname = PNBUF_GET();
	} else {
		symname = uio->uio_iov->iov_base;
	}

	/*
	 * Ok, we just gathering a symbolic name in SL record.
	 */
	if (cd9660_rrip_getsymname(dirp, symname, &symlen, imp) == 0) {
		if (use_pnbuf) {
			PNBUF_PUT(symname);
		}
		brelse(bp, 0);
		return (EINVAL);
	}
	/*
	 * Don't forget before you leave from home ;-)
	 */
	brelse(bp, 0);

	/*
	 * return with the symbolic name to caller's.
	 */
	if (use_pnbuf) {
		error = uiomove(symname, symlen, uio);
		PNBUF_PUT(symname);
		return (error);
	}
	uio->uio_resid -= symlen;
	uio->uio_iov->iov_base = (char *)uio->uio_iov->iov_base + symlen;
	uio->uio_iov->iov_len -= symlen;
	return (0);
}

int
cd9660_link(void *v)
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
cd9660_symlink(void *v)
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
cd9660_strategy(void *v)
{
	struct vop_strategy_args /* {
		struct vnode *a_vp;
		struct buf *a_bp;
	} */ *ap = v;
	struct buf *bp = ap->a_bp;
	struct vnode *vp = ap->a_vp;
	struct iso_node *ip;
	int error;

	ip = VTOI(vp);
	if (vp->v_type == VBLK || vp->v_type == VCHR)
		panic("cd9660_strategy: spec");
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
	vp = ip->i_mnt->im_devvp;
	return (VOP_STRATEGY(vp, bp));
}

/*
 * Print out the contents of an inode.
 */
/*ARGSUSED*/
int
cd9660_print(void *v)
{

	printf("tag VT_ISOFS, isofs vnode\n");
	return (0);
}

/*
 * Return POSIX pathconf information applicable to cd9660 filesystems.
 */
int
cd9660_pathconf(void *v)
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
		if (VTOI(ap->a_vp)->i_mnt->iso_ftype == ISO_FTYPE_RRIP)
			*ap->a_retval = ISO_MAXNAMLEN;
		else
			*ap->a_retval = 37;
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
		*ap->a_retval = 32;
		return (0);
	default:
		return (EINVAL);
	}
	/* NOTREACHED */
}

/*
 * Allow changing the size for special files (and fifos).
 */
int
cd9660_setattr(void *v)
{
	struct vop_setattr_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vattr *vap = ap->a_vap;
	struct vnode *vp = ap->a_vp;

	/*
	 * Only size is changeable.
	 */
	if (vap->va_type != VNON
	    || vap->va_nlink != (nlink_t)VNOVAL
	    || vap->va_fsid != VNOVAL
	    || vap->va_fileid != VNOVAL
	    || vap->va_blocksize != VNOVAL
	    || vap->va_rdev != (dev_t)VNOVAL
	    || (int)vap->va_bytes != VNOVAL
	    || vap->va_gen != VNOVAL
	    || vap->va_flags != VNOVAL
	    || vap->va_uid != (uid_t)VNOVAL
	    || vap->va_gid != (gid_t)VNOVAL
	    || vap->va_atime.tv_sec != VNOVAL
	    || vap->va_mtime.tv_sec != VNOVAL
	    || vap->va_mode != (mode_t)VNOVAL)
		return EOPNOTSUPP;

	if (vap->va_size != VNOVAL
	    && vp->v_type != VCHR
	    && vp->v_type != VBLK
	    && vp->v_type != VFIFO)
		return EOPNOTSUPP;

	return 0;
}

/*
 * Global vfs data structures for isofs
 */
#define	cd9660_create	genfs_eopnotsupp
#define	cd9660_mknod	genfs_eopnotsupp
#define	cd9660_write	genfs_eopnotsupp
#define	cd9660_fsync	genfs_nullop
#define	cd9660_remove	genfs_eopnotsupp
#define	cd9660_rename	genfs_eopnotsupp
#define	cd9660_mkdir	genfs_eopnotsupp
#define	cd9660_rmdir	genfs_eopnotsupp
#define	cd9660_advlock	genfs_einval
#define	cd9660_bwrite	genfs_eopnotsupp
#define cd9660_revoke	genfs_revoke

/*
 * Global vfs data structures for cd9660
 */
int (**cd9660_vnodeop_p)(void *);
const struct vnodeopv_entry_desc cd9660_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, cd9660_lookup },		/* lookup */
	{ &vop_create_desc, cd9660_create },		/* create */
	{ &vop_mknod_desc, cd9660_mknod },		/* mknod */
	{ &vop_open_desc, cd9660_open },		/* open */
	{ &vop_close_desc, cd9660_close },		/* close */
	{ &vop_access_desc, cd9660_access },		/* access */
	{ &vop_getattr_desc, cd9660_getattr },		/* getattr */
	{ &vop_setattr_desc, cd9660_setattr },		/* setattr */
	{ &vop_read_desc, cd9660_read },		/* read */
	{ &vop_write_desc, cd9660_write },		/* write */
	{ &vop_fallocate_desc, genfs_eopnotsupp },	/* fallocate */
	{ &vop_fdiscard_desc, genfs_eopnotsupp },	/* fdiscard */
	{ &vop_fcntl_desc, genfs_fcntl },		/* fcntl */
	{ &vop_ioctl_desc, cd9660_ioctl },		/* ioctl */
	{ &vop_poll_desc, cd9660_poll },		/* poll */
	{ &vop_revoke_desc, cd9660_revoke },		/* revoke */
	{ &vop_mmap_desc, cd9660_mmap },		/* mmap */
	{ &vop_fsync_desc, cd9660_fsync },		/* fsync */
	{ &vop_seek_desc, cd9660_seek },		/* seek */
	{ &vop_remove_desc, cd9660_remove },		/* remove */
	{ &vop_link_desc, cd9660_link },		/* link */
	{ &vop_rename_desc, cd9660_rename },		/* rename */
	{ &vop_mkdir_desc, cd9660_mkdir },		/* mkdir */
	{ &vop_rmdir_desc, cd9660_rmdir },		/* rmdir */
	{ &vop_symlink_desc, cd9660_symlink },		/* symlink */
	{ &vop_readdir_desc, cd9660_readdir },		/* readdir */
	{ &vop_readlink_desc, cd9660_readlink },	/* readlink */
	{ &vop_abortop_desc, cd9660_abortop },		/* abortop */
	{ &vop_inactive_desc, cd9660_inactive },	/* inactive */
	{ &vop_reclaim_desc, cd9660_reclaim },		/* reclaim */
	{ &vop_lock_desc, genfs_lock },			/* lock */
	{ &vop_unlock_desc, genfs_unlock },		/* unlock */
	{ &vop_bmap_desc, cd9660_bmap },		/* bmap */
	{ &vop_strategy_desc, cd9660_strategy },	/* strategy */
	{ &vop_print_desc, cd9660_print },		/* print */
	{ &vop_islocked_desc, genfs_islocked },		/* islocked */
	{ &vop_pathconf_desc, cd9660_pathconf },	/* pathconf */
	{ &vop_advlock_desc, cd9660_advlock },		/* advlock */
	{ &vop_bwrite_desc, vn_bwrite },		/* bwrite */
	{ &vop_getpages_desc, genfs_getpages },		/* getpages */
	{ &vop_putpages_desc, genfs_putpages },		/* putpages */
	{ NULL, NULL }
};
const struct vnodeopv_desc cd9660_vnodeop_opv_desc =
	{ &cd9660_vnodeop_p, cd9660_vnodeop_entries };

/*
 * Special device vnode ops
 */
int (**cd9660_specop_p)(void *);
const struct vnodeopv_entry_desc cd9660_specop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, spec_lookup },		/* lookup */
	{ &vop_create_desc, spec_create },		/* create */
	{ &vop_mknod_desc, spec_mknod },		/* mknod */
	{ &vop_open_desc, spec_open },			/* open */
	{ &vop_close_desc, spec_close },		/* close */
	{ &vop_access_desc, cd9660_access },		/* access */
	{ &vop_getattr_desc, cd9660_getattr },		/* getattr */
	{ &vop_setattr_desc, cd9660_setattr },		/* setattr */
	{ &vop_read_desc, spec_read },			/* read */
	{ &vop_write_desc, spec_write },		/* write */
	{ &vop_fallocate_desc, spec_fallocate },	/* fallocate */
	{ &vop_fdiscard_desc, spec_fdiscard },		/* fdiscard */
	{ &vop_fcntl_desc, genfs_fcntl },		/* fcntl */
	{ &vop_ioctl_desc, spec_ioctl },		/* ioctl */
	{ &vop_poll_desc, spec_poll },			/* poll */
	{ &vop_kqfilter_desc, spec_kqfilter },		/* kqfilter */
	{ &vop_revoke_desc, spec_revoke },		/* revoke */
	{ &vop_mmap_desc, spec_mmap },			/* mmap */
	{ &vop_fsync_desc, spec_fsync },		/* fsync */
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
	{ &vop_inactive_desc, cd9660_inactive },	/* inactive */
	{ &vop_reclaim_desc, cd9660_reclaim },		/* reclaim */
	{ &vop_lock_desc, genfs_lock },			/* lock */
	{ &vop_unlock_desc, genfs_unlock },		/* unlock */
	{ &vop_bmap_desc, spec_bmap },			/* bmap */
	{ &vop_strategy_desc, spec_strategy },		/* strategy */
	{ &vop_print_desc, cd9660_print },		/* print */
	{ &vop_islocked_desc, genfs_islocked },		/* islocked */
	{ &vop_pathconf_desc, spec_pathconf },		/* pathconf */
	{ &vop_advlock_desc, spec_advlock },		/* advlock */
	{ &vop_bwrite_desc, vn_bwrite },		/* bwrite */
	{ &vop_getpages_desc, spec_getpages },		/* getpages */
	{ &vop_putpages_desc, spec_putpages },		/* putpages */
	{ NULL, NULL }
};
const struct vnodeopv_desc cd9660_specop_opv_desc =
	{ &cd9660_specop_p, cd9660_specop_entries };

int (**cd9660_fifoop_p)(void *);
const struct vnodeopv_entry_desc cd9660_fifoop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, vn_fifo_bypass },		/* lookup */
	{ &vop_create_desc, vn_fifo_bypass },		/* create */
	{ &vop_mknod_desc, vn_fifo_bypass },		/* mknod */
	{ &vop_open_desc, vn_fifo_bypass },		/* open */
	{ &vop_close_desc, vn_fifo_bypass },		/* close */
	{ &vop_access_desc, cd9660_access },		/* access */
	{ &vop_getattr_desc, cd9660_getattr },		/* getattr */
	{ &vop_setattr_desc, cd9660_setattr },		/* setattr */
	{ &vop_read_desc, vn_fifo_bypass },		/* read */
	{ &vop_write_desc, vn_fifo_bypass },		/* write */
	{ &vop_fallocate_desc, vn_fifo_bypass },	/* fallocate */
	{ &vop_fdiscard_desc, vn_fifo_bypass },		/* fdiscard */
	{ &vop_fcntl_desc, genfs_fcntl },		/* fcntl */
	{ &vop_ioctl_desc, vn_fifo_bypass },		/* ioctl */
	{ &vop_poll_desc, vn_fifo_bypass },		/* poll */
	{ &vop_kqfilter_desc, vn_fifo_bypass },		/* kqfilter */
	{ &vop_revoke_desc, vn_fifo_bypass },		/* revoke */
	{ &vop_mmap_desc, vn_fifo_bypass },		/* mmap */
	{ &vop_fsync_desc, vn_fifo_bypass },		/* fsync */
	{ &vop_seek_desc, vn_fifo_bypass },		/* seek */
	{ &vop_remove_desc, vn_fifo_bypass },		/* remove */
	{ &vop_link_desc, vn_fifo_bypass } ,		/* link */
	{ &vop_rename_desc, vn_fifo_bypass },		/* rename */
	{ &vop_mkdir_desc, vn_fifo_bypass },		/* mkdir */
	{ &vop_rmdir_desc, vn_fifo_bypass },		/* rmdir */
	{ &vop_symlink_desc, vn_fifo_bypass },		/* symlink */
	{ &vop_readdir_desc, vn_fifo_bypass },		/* readdir */
	{ &vop_readlink_desc, vn_fifo_bypass },		/* readlink */
	{ &vop_abortop_desc, vn_fifo_bypass },		/* abortop */
	{ &vop_inactive_desc, cd9660_inactive },	/* inactive */
	{ &vop_reclaim_desc, cd9660_reclaim },		/* reclaim */
	{ &vop_lock_desc, genfs_lock },			/* lock */
	{ &vop_unlock_desc, genfs_unlock },		/* unlock */
	{ &vop_bmap_desc, vn_fifo_bypass },		/* bmap */
	{ &vop_strategy_desc, vn_fifo_bypass },		/* strategy */
	{ &vop_print_desc, cd9660_print },		/* print */
	{ &vop_islocked_desc, genfs_islocked },		/* islocked */
	{ &vop_pathconf_desc, vn_fifo_bypass },		/* pathconf */
	{ &vop_advlock_desc, vn_fifo_bypass },		/* advlock */
	{ &vop_bwrite_desc, vn_bwrite },		/* bwrite */
	{ &vop_putpages_desc, vn_fifo_bypass }, 	/* putpages */
	{ NULL, NULL }
};
const struct vnodeopv_desc cd9660_fifoop_opv_desc =
	{ &cd9660_fifoop_p, cd9660_fifoop_entries };
