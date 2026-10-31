/*	$NetBSD: ffs_extattr.c,v 1.10 2022/11/28 04:52:04 chs Exp $	*/

/*-
 * SPDX-License-Identifier: (BSD-2-Clause-FreeBSD AND BSD-3-Clause)
 *
 * Copyright (c) 2002, 2003 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Marshall
 * Kirk McKusick and Network Associates Laboratories, the Security
 * Research Division of Network Associates, Inc. under DARPA/SPAWAR
 * contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA CHATS
 * research program
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	from: @(#)ufs_readwrite.c	8.11 (Berkeley) 5/8/95
 * from: $FreeBSD: .../ufs/ufs_readwrite.c,v 1.96 2002/08/12 09:22:11 phk ...
 *	@(#)ffs_vnops.c	8.15 (Berkeley) 5/14/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ffs_extattr.c,v 1.10 2022/11/28 04:52:04 chs Exp $");

#if defined(_KERNEL_OPT)
#include "opt_ffs.h"
#include "opt_wapbl.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/resourcevar.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/event.h>
#include <sys/extattr.h>
#include <sys/kauth.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/signalvar.h>
#include <sys/kauth.h>
#include <sys/wapbl.h>

#include <miscfs/fifofs/fifo.h>
#include <miscfs/genfs/genfs.h>
#include <miscfs/specfs/specdev.h>

#include <ufs/ufs/inode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ufs/ufs_extern.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_wapbl.h>

#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>

#define ALIGNED_TO(ptr, s)  \
    (((uintptr_t)(ptr) & (_Alignof(s) - 1)) == 0)
#define uoff_t uintmax_t
#define ITOFS(ip) (ip)->i_fs
#define i_din2 i_din.ffs2_din
#define VI_LOCK(vp)		mutex_enter((vp)->v_interlock)
#define VI_UNLOCK(vp)		mutex_exit((vp)->v_interlock)
#define UFS_INODE_SET_FLAG(ip, f)	((ip)->i_flag |= (f))
#define ASSERT_VOP_ELOCKED(vp, m)	KASSERT(VOP_ISLOCKED(vp))
#define I_IS_UFS2(ip)		((ip)->i_ump->um_fstype == UFS2)
#define	lblktosize(fs, o)	ffs_lblktosize(fs, o)
#define	lblkno(fs, o)		ffs_lblkno(fs, o)
#define	blkoff(fs, o)		ffs_blkoff(fs, o)
#define	sblksize(fs, o, lbn)	ffs_sblksize(fs, o, lbn)
typedef daddr_t ufs_lbn_t;
#define msleep(chan, mtx, pri, wmesg, timeo) \
    mtsleep((chan), (pri), (wmesg), (timeo), *(mtx))
#define vm_page_count_severe()		0
#define buf_dirty_count_severe()	0
#define BA_CLRBUF B_CLRBUF
#define IO_ASYNC 0
#define vfs_bio_brelse(bp, ioflag) 	brelse(bp, 0)
#define vfs_bio_clrbuf(bp) 		clrbuf(bp)
#define vfs_bio_set_flags(bp, ioflag) 	__nothing

/*
 * Extended attribute area reading.
 */
static int
ffs_extread(struct vnode *vp, struct uio *uio, int ioflag)
{
	struct inode *ip;
	struct ufs2_dinode *dp;
	struct fs *fs;
	struct buf *bp;
	ufs_lbn_t lbn, nextlbn;
	off_t bytesinfile;
	long size, xfersize, blkoffset;
	ssize_t orig_resid;
	int error;

	ip = VTOI(vp);
	fs = ITOFS(ip);
	dp = ip->i_din2;

#ifdef INVARIANTS
	if (uio->uio_rw != UIO_READ || ip->i_ump->um_fstype != UFS2)
		panic("ffs_extread: mode");

#endif
	orig_resid = uio->uio_resid;
	KASSERT(orig_resid >= 0);
	if (orig_resid == 0)
		return (0);
	KASSERT(uio->uio_offset >= 0);

	for (error = 0, bp = NULL; uio->uio_resid > 0; bp = NULL) {
		if ((bytesinfile = dp->di_extsize - uio->uio_offset) <= 0)
			break;
		lbn = lblkno(fs, uio->uio_offset);
		nextlbn = lbn + 1;

		/*
		 * size of buffer.  The buffer representing the
		 * end of the file is rounded up to the size of
		 * the block type ( fragment or full block,
		 * depending ).
		 */
		size = sblksize(fs, dp->di_extsize, lbn);
		blkoffset = blkoff(fs, uio->uio_offset);

		/*
		 * The amount we want to transfer in this iteration is
		 * one FS block less the amount of the data before
		 * our startpoint (duh!)
		 */
		xfersize = fs->fs_bsize - blkoffset;

		/*
		 * But if we actually want less than the block,
		 * or the file doesn't have a whole block more of data,
		 * then use the lesser number.
		 */
		if (uio->uio_resid < xfersize)
			xfersize = uio->uio_resid;
		if (bytesinfile < xfersize)
			xfersize = bytesinfile;

		if (lblktosize(fs, nextlbn) >= dp->di_extsize) {
			/*
			 * Don't do readahead if this is the end of the info.
			 */
			error = bread(vp, -1 - lbn, size, 0, &bp);
		} else {
			/*
			 * If we have a second block, then
			 * fire off a request for a readahead
			 * as well as a read. Note that the 4th and 5th
			 * arguments point to arrays of the size specified in
			 * the 6th argument.
			 */
			u_int nextsize = sblksize(fs, dp->di_extsize, nextlbn);

			nextlbn = -1 - nextlbn;
			error = breadn(vp, -1 - lbn,
			    size, &nextlbn, &nextsize, 1, 0, &bp);
		}
		if (error) {
			brelse(bp, 0);
			bp = NULL;
			break;
		}

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

		error = uiomove((char *)bp->b_data + blkoffset,
					(int)xfersize, uio);
		if (error)
			break;
		vfs_bio_brelse(bp, ioflag);
	}

	/*
	 * This can only happen in the case of an error
	 * because the loop above resets bp to NULL on each iteration
	 * and on normal completion has not set a new value into it.
	 * so it must have come from a 'break' statement
	 */
	if (bp != NULL)
		vfs_bio_brelse(bp, ioflag);
	return (error);
}
/*
 * Extended attribute area writing.
 */
static int
ffs_extwrite(struct vnode *vp, struct uio *uio, int ioflag, kauth_cred_t ucred)
{
	struct inode *ip;
	struct ufs2_dinode *dp;
	struct fs *fs;
	struct buf *bp;
	ufs_lbn_t lbn;
	off_t osize;
	ssize_t resid;
	int blkoffset, error, flags, size, xfersize;

	ip = VTOI(vp);
	fs = ITOFS(ip);
	dp = ip->i_din2;

#ifdef INVARIANTS
	if (uio->uio_rw != UIO_WRITE || ip->i_ump->um_fstype != UFS2)
		panic("ffs_extwrite: mode");
#endif

	if (ioflag & IO_APPEND)
		uio->uio_offset = dp->di_extsize;
	KASSERT(uio->uio_offset >= 0);
	if ((uoff_t)uio->uio_offset + uio->uio_resid >
	    UFS_NXADDR * fs->fs_bsize)
		return (EFBIG);

	resid = uio->uio_resid;
	osize = dp->di_extsize;
	flags = IO_EXT;
	if (ioflag & IO_SYNC)
		flags |= IO_SYNC;

	if ((error = UFS_WAPBL_BEGIN(vp->v_mount)) != 0)
		return error;

	for (error = 0; uio->uio_resid > 0;) {
		lbn = lblkno(fs, uio->uio_offset);
		blkoffset = blkoff(fs, uio->uio_offset);
		xfersize = fs->fs_bsize - blkoffset;
		if (uio->uio_resid < xfersize)
			xfersize = uio->uio_resid;

		/*
		 * We must perform a read-before-write if the transfer size
		 * does not cover the entire buffer.
		 */
		if (fs->fs_bsize > xfersize)
			flags |= BA_CLRBUF;
		else
			flags &= ~BA_CLRBUF;
		error = UFS_BALLOC(vp, uio->uio_offset, xfersize,
		    ucred, flags, &bp);
		if (error != 0)
			break;
		/*
		 * If the buffer is not valid we have to clear out any
		 * garbage data from the pages instantiated for the buffer.
		 * If we do not, a failed uiomove() during a write can leave
		 * the prior contents of the pages exposed to a userland
		 * mmap().  XXX deal with uiomove() errors a better way.
		 */
		if ((bp->b_flags & BC_NOCACHE) && fs->fs_bsize <= xfersize)
			vfs_bio_clrbuf(bp);

		if (uio->uio_offset + xfersize > dp->di_extsize)
			dp->di_extsize = uio->uio_offset + xfersize;

		size = sblksize(fs, dp->di_extsize, lbn) - bp->b_resid;
		if (size < xfersize)
			xfersize = size;

		error =
		    uiomove((char *)bp->b_data + blkoffset, (int)xfersize, uio);

		vfs_bio_set_flags(bp, ioflag);

		/*
		 * If IO_SYNC each buffer is written synchronously.  Otherwise
		 * if we have a severe page deficiency write the buffer
		 * asynchronously.  Otherwise try to cluster, and if that
		 * doesn't do it then either do an async write (if O_DIRECT),
		 * or a delayed write (if not).
		 */
		if (ioflag & IO_SYNC) {
			(void)bwrite(bp);
		} else if (vm_page_count_severe() ||
			    buf_dirty_count_severe() ||
			    xfersize + blkoffset == fs->fs_bsize ||
			    (ioflag & (IO_ASYNC | IO_DIRECT)))
			bawrite(bp);
		else
			bdwrite(bp);
		if (error || xfersize == 0)
			break;
		UFS_INODE_SET_FLAG(ip, IN_CHANGE);
	}
	/*
	 * If we successfully wrote any data, and we are not the superuser
	 * we clear the setuid and setgid bits as a precaution against
	 * tampering.
	 */
	if ((ip->i_mode & (ISUID | ISGID)) && resid > uio->uio_resid && ucred) {
		ip->i_mode &= ~(ISUID | ISGID);
		dp->di_mode = ip->i_mode;
	}
	if (error) {
		if (ioflag & IO_UNIT) {
			(void)ffs_truncate(vp, osize,
			    IO_EXT | (ioflag&IO_SYNC), ucred);
			uio->uio_offset -= resid - uio->uio_resid;
			uio->uio_resid = resid;
		}
	} else if (resid > uio->uio_resid && (ioflag & IO_SYNC))
		error = ffs_update(vp, NULL, NULL, UPDATE_WAIT);
	UFS_WAPBL_END(vp->v_mount);
	return (error);
}

/*
 * Vnode operating to retrieve a named extended attribute.
 *
 * Locate a particular EA (nspace:name) in the area (ptr:length), and return
 * the length of the EA, and possibly the pointer to the entry and to the data.
 */
static int
ffs_findextattr(u_char *ptr, u_int length, int nspace, const char *name,
    struct extattr **eapp, u_char **eac)
{
	struct extattr *eap, *eaend;
	size_t nlen;

	nlen = strlen(name);
	KASSERT(ALIGNED_TO(ptr, struct extattr));
	eap = (struct extattr *)ptr;
	eaend = (struct extattr *)(ptr + length);
	for (; eap < eaend; eap = EXTATTR_NEXT(eap)) {
		/* make sure this entry is complete */
		if (EXTATTR_NEXT(eap) > eaend)
			break;
		if (eap->ea_namespace != nspace || eap->ea_namelength != nlen
		    || memcmp(eap->ea_name, name, nlen) != 0)
			continue;
		if (eapp != NULL)
			*eapp = eap;
		if (eac != NULL)
			*eac = EXTATTR_CONTENT(eap);
		return (EXTATTR_CONTENT_SIZE(eap));
	}
	return (-1);
}

static int
ffs_rdextattr(u_char **p, struct vnode *vp, int extra)
{
	struct inode *ip;
	struct ufs2_dinode *dp;
	struct fs *fs;
	struct uio luio;
	struct iovec liovec;
	u_int easize;
	int error;
	u_char *eae;

	ip = VTOI(vp);
	fs = ITOFS(ip);
	dp = ip->i_din2;
	easize = dp->di_extsize;
	if ((uoff_t)easize + extra > UFS_NXADDR * fs->fs_bsize)
		return (EFBIG);

	eae = malloc(easize + extra, M_TEMP, M_WAITOK);

	liovec.iov_base = eae;
	liovec.iov_len = easize;
	luio.uio_iov = &liovec;
	luio.uio_iovcnt = 1;
	luio.uio_offset = 0;
	luio.uio_resid = easize;
	luio.uio_vmspace = vmspace_kernel();
	luio.uio_rw = UIO_READ;

	error = ffs_extread(vp, &luio, IO_EXT | IO_SYNC);
	if (error) {
		free(eae, M_TEMP);
		return(error);
	}
	*p = eae;
	return (0);
}

static void
ffs_lock_ea(struct vnode *vp)
{
	genfs_node_wrlock(vp);
}

static void
ffs_unlock_ea(struct vnode *vp)
{
	genfs_node_unlock(vp);
}

static int
ffs_open_ea(struct vnode *vp, kauth_cred_t cred)
{
	struct inode *ip;
	struct ufs2_dinode *dp;
	int error;

	ip = VTOI(vp);
	if ((ip->i_ump->um_flags & UFS_EA) == 0) {
		return EOPNOTSUPP;
	}

	ffs_lock_ea(vp);
	if (ip->i_ea_area != NULL) {
		ip->i_ea_refs++;
		ffs_unlock_ea(vp);
		return (0);
	}
	dp = ip->i_din2;
	error = ffs_rdextattr(&ip->i_ea_area, vp, 0);
	if (error) {
		ffs_unlock_ea(vp);
		return (error);
	}
	ip->i_ea_len = dp->di_extsize;
	ip->i_ea_error = 0;
	ip->i_ea_refs++;
	ffs_unlock_ea(vp);
	return (0);
}

/*
 * Vnode extattr transaction commit/abort
 */
static int
ffs_close_ea(struct vnode *vp, int commit, kauth_cred_t cred)
{
	struct inode *ip;
	struct uio luio;
	struct iovec liovec;
	int error;
	struct ufs2_dinode *dp;

	ip = VTOI(vp);
	KASSERT((ip->i_ump->um_flags & UFS_EA) != 0);

	if (commit)
		KASSERT(VOP_ISLOCKED(vp) == LK_EXCLUSIVE);
	else
		KASSERT(VOP_ISLOCKED(vp));
	ffs_lock_ea(vp);
	if (ip->i_ea_area == NULL) {
		ffs_unlock_ea(vp);
		return (EINVAL);
	}
	dp = ip->i_din2;
	error = ip->i_ea_error;
	if (commit && error == 0) {
		ASSERT_VOP_ELOCKED(vp, "ffs_close_ea commit");
		if (cred == NOCRED)
			cred =  lwp0.l_cred;
		liovec.iov_base = ip->i_ea_area;
		liovec.iov_len = ip->i_ea_len;
		luio.uio_iov = &liovec;
		luio.uio_iovcnt = 1;
		luio.uio_offset = 0;
		luio.uio_resid = ip->i_ea_len;
		luio.uio_vmspace = vmspace_kernel();
		luio.uio_rw = UIO_WRITE;

		/* XXX: I'm not happy about truncating to zero size */
		if (ip->i_ea_len < dp->di_extsize) {
			if ((error = UFS_WAPBL_BEGIN(vp->v_mount)) != 0) {
				ffs_unlock_ea(vp);
				return error;
			}
			error = ffs_truncate(vp, 0, IO_EXT, cred);
			UFS_WAPBL_END(vp->v_mount);
		}
		error = ffs_extwrite(vp, &luio, IO_EXT | IO_SYNC, cred);
	}
	if (--ip->i_ea_refs == 0) {
		free(ip->i_ea_area, M_TEMP);
		ip->i_ea_area = NULL;
		ip->i_ea_len = 0;
		ip->i_ea_error = 0;
	}
	ffs_unlock_ea(vp);
	return (error);
}

/*
 * Vnode extattr strategy routine for fifos.
 *
 * We need to check for a read or write of the external attributes.
 * Otherwise we just fall through and do the usual thing.
 */
int
ffsext_strategy(void *v)
{
	struct vop_strategy_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct buf *a_bp;
	} */ *ap = v;
	struct vnode *vp;
	daddr_t lbn;

	vp = ap->a_vp;
	lbn = ap->a_bp->b_lblkno;
	if (I_IS_UFS2(VTOI(vp)) && lbn < 0 && lbn >= -UFS_NXADDR)
		return ufs_strategy(ap);
	if (vp->v_type == VFIFO)
		return vn_fifo_bypass(ap);
	panic("spec nodes went here");
}

/*
 * Vnode extattr transaction commit/abort
 */
int
ffs_openextattr(void *v)
{
	struct vop_openextattr_args /* {
		struct vnode *a_vp;
		kauth_cred_t a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct inode *ip = VTOI(ap->a_vp);

	/* Not supported for UFS1 file systems. */
	if (ip->i_ump->um_fstype == UFS1)
		return (EOPNOTSUPP);

#ifdef __FreeBSD__
	if (ap->a_vp->v_type == VCHR || ap->a_vp->v_type == VBLK)
		return (EOPNOTSUPP);
#endif

	return (ffs_open_ea(ap->a_vp, ap->a_cred));
}

/*
 * Vnode extattr transaction commit/abort
 */
int
ffs_closeextattr(void *v)
{
	struct vop_closeextattr_args /* {
		struct vnode *a_vp;
		int a_commit;
		kauth_cred_t a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct inode *ip = VTOI(ap->a_vp);

	/* Not supported for UFS1 file systems. */
	if (ip->i_ump->um_fstype == UFS1)
		return (EOPNOTSUPP);

#ifdef __FreeBSD__
	if (ap->a_vp->v_type == VCHR || ap->a_vp->v_type == VBLK)
		return (EOPNOTSUPP);
#endif

	if (ap->a_commit && (ap->a_vp->v_mount->mnt_flag & MNT_RDONLY))
		return (EROFS);

	return (ffs_close_ea(ap->a_vp, ap->a_commit, ap->a_cred));
}

/*
 * Vnode operation to retrieve a named extended attribute.
 */
int
ffs_getextattr(void *v)
{
	struct vop_getextattr_args /* {
		struct vnode *a_vp;
		int a_attrnamespace;
		const char *a_name;
		struct uio *a_uio;
		size_t *a_size;
		kauth_cred_t a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);

	KASSERT(VOP_ISLOCKED(vp));

	if (ip->i_ump->um_fstype == UFS1) {
#ifdef UFS_EXTATTR
		return ufs_getextattr(ap);
#else
		return EOPNOTSUPP;
#endif
	}

	u_char *eae, *p;
	unsigned easize;
	int error, ealen;

#ifdef __FreeBSD__
	if (ap->a_vp->v_type == VCHR || ap->a_vp->v_type == VBLK)
		return (EOPNOTSUPP);
#endif

	error = extattr_check_cred(ap->a_vp, ap->a_attrnamespace,
	    ap->a_cred, VREAD);
	if (error)
		return (error);

	error = ffs_open_ea(ap->a_vp, ap->a_cred);
	if (error)
		return (error);

	eae = ip->i_ea_area;
	easize = ip->i_ea_len;

	ealen = ffs_findextattr(eae, easize, ap->a_attrnamespace, ap->a_name,
	    NULL, &p);
	if (ealen >= 0) {
		error = 0;
		if (ap->a_size != NULL)
			*ap->a_size = ealen;
		else if (ap->a_uio != NULL)
			error = uiomove(p, ealen, ap->a_uio);
	} else
		error = ENOATTR;

	ffs_close_ea(ap->a_vp, 0, ap->a_cred);
	return (error);
}

/*
 * Vnode operation to set a named attribute.
 */
int
ffs_setextattr(void *v)
{
	struct vop_setextattr_args /* {
		struct vnode *a_vp;
		int a_attrnamespace;
		const char *a_name;
		struct uio *a_uio;
		kauth_cred_t a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	struct fs *fs = ip->i_fs;

	KASSERT(VOP_ISLOCKED(vp) == LK_EXCLUSIVE);
	if (ip->i_ump->um_fstype == UFS1) {
#ifdef UFS_EXTATTR
		return ufs_setextattr(ap);
#else
		return EOPNOTSUPP;
#endif
	}

	struct extattr *eap;
	uint32_t ealength, ul;
	ssize_t ealen;
	int olen, eapad1, eapad2, error, i, easize;
	u_char *eae;
	void *tmp;

	if (ap->a_vp->v_type == VCHR || ap->a_vp->v_type == VBLK)
		return (EOPNOTSUPP);

	if (strlen(ap->a_name) == 0)
		return (EINVAL);

	/* XXX Now unsupported API to delete EAs using NULL uio. */
	if (ap->a_uio == NULL)
		return (EOPNOTSUPP);

	if (ap->a_vp->v_mount->mnt_flag & MNT_RDONLY)
		return (EROFS);

	ealen = ap->a_uio->uio_resid;
	if (ealen < 0 || ealen > lblktosize(fs, UFS_NXADDR))
		return (EINVAL);

	error = extattr_check_cred(ap->a_vp, ap->a_attrnamespace,
	    ap->a_cred, VWRITE);
	if (error) {

		/*
		 * ffs_lock_ea is not needed there, because the vnode
		 * must be exclusively locked.
		 */
		if (ip->i_ea_area != NULL && ip->i_ea_error == 0)
			ip->i_ea_error = error;
		return (error);
	}

	error = ffs_open_ea(ap->a_vp, ap->a_cred);
	if (error)
		return (error);

	ealength = sizeof(uint32_t) + 3 + strlen(ap->a_name);
	eapad1 = roundup2(ealength, 8) - ealength;
	eapad2 = roundup2(ealen, 8) - ealen;
	ealength += eapad1 + ealen + eapad2;

	/*
	 * CEM: rewrites of the same size or smaller could be done in-place
	 * instead.  (We don't acquire any fine-grained locks in here either,
	 * so we could also do bigger writes in-place.)
	 */
	eae = malloc(ip->i_ea_len + ealength, M_TEMP, M_WAITOK);
	bcopy(ip->i_ea_area, eae, ip->i_ea_len);
	easize = ip->i_ea_len;

	olen = ffs_findextattr(eae, easize, ap->a_attrnamespace, ap->a_name,
	    &eap, NULL);
        if (olen == -1) {
		/* new, append at end */
		KASSERT(ALIGNED_TO(eae + easize, struct extattr));
		eap = (struct extattr *)(eae + easize);
		easize += ealength;
	} else {
		ul = eap->ea_length;
		i = (u_char *)EXTATTR_NEXT(eap) - eae;
		if (ul != ealength) {
			bcopy(EXTATTR_NEXT(eap), (u_char *)eap + ealength,
			    easize - i);
			easize += (ealength - ul);
		}
	}
	if (easize > lblktosize(fs, UFS_NXADDR)) {
		free(eae, M_TEMP);
		ffs_close_ea(ap->a_vp, 0, ap->a_cred);
		if (ip->i_ea_area != NULL && ip->i_ea_error == 0)
			ip->i_ea_error = ENOSPC;
		return (ENOSPC);
	}
	eap->ea_length = ealength;
	eap->ea_namespace = ap->a_attrnamespace;
	eap->ea_contentpadlen = eapad2;
	eap->ea_namelength = strlen(ap->a_name);
	memcpy(eap->ea_name, ap->a_name, strlen(ap->a_name));
	bzero(&eap->ea_name[strlen(ap->a_name)], eapad1);
	error = uiomove(EXTATTR_CONTENT(eap), ealen, ap->a_uio);
	if (error) {
		free(eae, M_TEMP);
		ffs_close_ea(ap->a_vp, 0, ap->a_cred);
		if (ip->i_ea_area != NULL && ip->i_ea_error == 0)
			ip->i_ea_error = error;
		return (error);
	}
	bzero((u_char *)EXTATTR_CONTENT(eap) + ealen, eapad2);

	tmp = ip->i_ea_area;
	ip->i_ea_area = eae;
	ip->i_ea_len = easize;
	free(tmp, M_TEMP);
	error = ffs_close_ea(ap->a_vp, 1, ap->a_cred);
	return (error);
}

/*
 * Vnode operation to retrieve extended attributes on a vnode.
 */
int
ffs_listextattr(void *v)
{
	struct vop_listextattr_args /* {
		struct vnode *a_vp;
		int a_attrnamespace;
		struct uio *a_uio;
		size_t *a_size;
		kauth_cred_t a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct inode *ip = VTOI(ap->a_vp);

	if (ip->i_ump->um_fstype == UFS1) {
#ifdef UFS_EXTATTR
		return ufs_listextattr(ap);
#else
		return EOPNOTSUPP;
#endif
	}

	struct extattr *eap, *eaend;
	int error, ealen;

	if (ap->a_vp->v_type == VCHR || ap->a_vp->v_type == VBLK)
		return (EOPNOTSUPP);

	error = extattr_check_cred(ap->a_vp, ap->a_attrnamespace,
	    ap->a_cred, VREAD);
	if (error)
		return (error);

	error = ffs_open_ea(ap->a_vp, ap->a_cred);
	if (error)
		return (error);

	error = 0;
	if (ap->a_size != NULL)
		*ap->a_size = 0;

	KASSERT(ALIGNED_TO(ip->i_ea_area, struct extattr));
	eap = (struct extattr *)ip->i_ea_area;
	eaend = (struct extattr *)(ip->i_ea_area + ip->i_ea_len);
	for (; error == 0 && eap < eaend; eap = EXTATTR_NEXT(eap)) {
		/* make sure this entry is complete */
		if (EXTATTR_NEXT(eap) > eaend)
			break;
		if (eap->ea_namespace != ap->a_attrnamespace)
			continue;

		ealen = eap->ea_namelength;
		if (ap->a_size != NULL)
			*ap->a_size += ealen + 1;
		else if (ap->a_uio != NULL)
			error = uiomove(&eap->ea_namelength, ealen + 1,
			    ap->a_uio);
	}

	ffs_close_ea(ap->a_vp, 0, ap->a_cred);
	return (error);
}

/*
 * Vnode operation to remove a named attribute.
 */
int
ffs_deleteextattr(void *v)
{
	struct vop_deleteextattr_args /* {
		struct vnode *a_vp;
		int a_attrnamespace;
		kauth_cred_t a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);

	if (ip->i_ump->um_fstype == UFS1) {
#ifdef UFS_EXTATTR
		return ufs_deleteextattr(ap);
#else
		return EOPNOTSUPP;
#endif
	}

	struct extattr *eap;
	uint32_t ul;
	int olen, error, i, easize;
	u_char *eae;
	void *tmp;

#ifdef __FreeBSD__
	if (ap->a_vp->v_type == VCHR || ap->a_vp->v_type == VBLK)
		return (EOPNOTSUPP);
#endif

	if (strlen(ap->a_name) == 0)
		return (EINVAL);

	if (ap->a_vp->v_mount->mnt_flag & MNT_RDONLY)
		return (EROFS);

	error = extattr_check_cred(ap->a_vp, ap->a_attrnamespace,
	    ap->a_cred, VWRITE);
	if (error) {
		/*
		 * ffs_lock_ea is not needed there, because the vnode
		 * must be exclusively locked.
		 */
		if (ip->i_ea_area != NULL && ip->i_ea_error == 0)
			ip->i_ea_error = error;
		return (error);
	}

	error = ffs_open_ea(ap->a_vp, ap->a_cred);
	if (error)
		return (error);

	/* CEM: delete could be done in-place instead */
	eae = malloc(ip->i_ea_len, M_TEMP, M_WAITOK);
	bcopy(ip->i_ea_area, eae, ip->i_ea_len);
	easize = ip->i_ea_len;

	olen = ffs_findextattr(eae, easize, ap->a_attrnamespace, ap->a_name,
	    &eap, NULL);
	if (olen == -1) {
		/* delete but nonexistent */
		free(eae, M_TEMP);
		ffs_close_ea(ap->a_vp, 0, ap->a_cred);
		return (ENOATTR);
	}
	ul = eap->ea_length;
	i = (u_char *)EXTATTR_NEXT(eap) - eae;
	bcopy(EXTATTR_NEXT(eap), eap, easize - i);
	easize -= ul;

	tmp = ip->i_ea_area;
	ip->i_ea_area = eae;
	ip->i_ea_len = easize;
	free(tmp, M_TEMP);
	error = ffs_close_ea(ap->a_vp, 1, ap->a_cred);
	return error;
}
