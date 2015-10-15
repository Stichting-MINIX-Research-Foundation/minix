/*	$NetBSD: ext2fs_readwrite.c,v 1.74 2015/03/28 19:24:04 maxv Exp $	*/

/*-
 * Copyright (c) 1993
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
 *	@(#)ufs_readwrite.c	8.8 (Berkeley) 8/4/94
 * Modified for ext2fs by Manuel Bouyer.
 */

/*-
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
 *	@(#)ufs_readwrite.c	8.8 (Berkeley) 8/4/94
 * Modified for ext2fs by Manuel Bouyer.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ext2fs_readwrite.c,v 1.74 2015/03/28 19:24:04 maxv Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/resourcevar.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/signalvar.h>
#include <sys/kauth.h>

#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>
#include <ufs/ext2fs/ext2fs.h>
#include <ufs/ext2fs/ext2fs_extern.h>

static int	ext2fs_post_read_update(struct vnode *, int, int);
static int	ext2fs_post_write_update(struct vnode *, struct uio *, int,
		    kauth_cred_t, off_t, int, int, int);

/*
 * Vnode op for reading.
 */
/* ARGSUSED */
int
ext2fs_read(void *v)
{
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp;
	struct inode *ip;
	struct uio *uio;
	struct ufsmount *ump;
	vsize_t bytelen;
	int advice;
	int error;

	vp = ap->a_vp;
	ip = VTOI(vp);
	ump = ip->i_ump;
	uio = ap->a_uio;
	error = 0;

	KASSERT(uio->uio_rw == UIO_READ);
	KASSERT(vp->v_type == VREG || vp->v_type == VDIR);

	/* XXX Eliminate me by refusing directory reads from userland.  */
	if (vp->v_type == VDIR)
		return ext2fs_bufrd(vp, uio, ap->a_ioflag, ap->a_cred);

	if ((uint64_t)uio->uio_offset > ump->um_maxfilesize)
		return (EFBIG);
	if (uio->uio_resid == 0)
		return (0);
	if (uio->uio_offset >= ext2fs_size(ip))
		goto out;

	KASSERT(vp->v_type == VREG);
	advice = IO_ADV_DECODE(ap->a_ioflag);
	while (uio->uio_resid > 0) {
		bytelen = MIN(ext2fs_size(ip) - uio->uio_offset,
			    uio->uio_resid);
		if (bytelen == 0)
			break;

		error = ubc_uiomove(&vp->v_uobj, uio, bytelen, advice,
		    UBC_READ | UBC_PARTIALOK | UBC_UNMAP_FLAG(vp));
		if (error)
			break;
	}

out:
	error = ext2fs_post_read_update(vp, ap->a_ioflag, error);
	return (error);
}

/*
 * UFS op for reading via the buffer cache
 */
int
ext2fs_bufrd(struct vnode *vp, struct uio *uio, int ioflag, kauth_cred_t cred)
{
	struct inode *ip;
	struct ufsmount *ump;
	struct m_ext2fs *fs;
	struct buf *bp;
	off_t bytesinfile;
	daddr_t lbn, nextlbn;
	long size, xfersize, blkoffset;
	int error;

	KASSERT(uio->uio_rw == UIO_READ);
	KASSERT(VOP_ISLOCKED(vp));
	KASSERT(vp->v_type == VDIR || vp->v_type == VLNK);

	ip = VTOI(vp);
	ump = ip->i_ump;
	fs = ip->i_e2fs;
	error = 0;

	KASSERT(vp->v_type != VLNK ||
	    ext2fs_size(ip) >= ump->um_maxsymlinklen);
	KASSERT(vp->v_type != VLNK || ump->um_maxsymlinklen != 0 ||
	    ext2fs_nblock(ip) != 0);

	if (uio->uio_offset > ump->um_maxfilesize)
		return EFBIG;
	if (uio->uio_resid == 0)
		return 0;
	if (uio->uio_offset >= ext2fs_size(ip))
		goto out;

	for (error = 0, bp = NULL; uio->uio_resid > 0; bp = NULL) {
		bytesinfile = ext2fs_size(ip) - uio->uio_offset;
		if (bytesinfile <= 0)
			break;
		lbn = ext2_lblkno(fs, uio->uio_offset);
		nextlbn = lbn + 1;
		size = fs->e2fs_bsize;
		blkoffset = ext2_blkoff(fs, uio->uio_offset);
		xfersize = fs->e2fs_bsize - blkoffset;
		if (uio->uio_resid < xfersize)
			xfersize = uio->uio_resid;
		if (bytesinfile < xfersize)
			xfersize = bytesinfile;

		if (ext2_lblktosize(fs, nextlbn) >= ext2fs_size(ip))
			error = bread(vp, lbn, size, 0, &bp);
		else {
			int nextsize = fs->e2fs_bsize;
			error = breadn(vp, lbn,
				size, &nextlbn, &nextsize, 1, 0, &bp);
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
		error = uiomove((char *)bp->b_data + blkoffset, xfersize, uio);
		if (error)
			break;
		brelse(bp, 0);
	}
	if (bp != NULL)
		brelse(bp, 0);

out:
	error = ext2fs_post_read_update(vp, ioflag, error);
	return (error);
}

static int
ext2fs_post_read_update(struct vnode *vp, int ioflag, int oerror)
{
	struct inode *ip = VTOI(vp);
	int error = oerror;

	if (!(vp->v_mount->mnt_flag & MNT_NOATIME)) {
		ip->i_flag |= IN_ACCESS;
		if ((ioflag & IO_SYNC) == IO_SYNC)
			error = ext2fs_update(vp, NULL, NULL, UPDATE_WAIT);
	}

	/* Read error overrides any inode update error.  */
	if (oerror)
		error = oerror;
	return error;
}

/*
 * Vnode op for writing.
 */
int
ext2fs_write(void *v)
{
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp;
	struct uio *uio;
	struct inode *ip;
	struct m_ext2fs *fs;
	struct ufsmount *ump;
	off_t osize;
	int blkoffset, error, ioflag, resid;
	vsize_t bytelen;
	off_t oldoff = 0;					/* XXX */
	bool async;
	int extended = 0;
	int advice;

	ioflag = ap->a_ioflag;
	advice = IO_ADV_DECODE(ioflag);
	uio = ap->a_uio;
	vp = ap->a_vp;
	ip = VTOI(vp);
	ump = ip->i_ump;
	error = 0;

	KASSERT(uio->uio_rw == UIO_WRITE);
	KASSERT(vp->v_type == VREG);

	if (ioflag & IO_APPEND)
		uio->uio_offset = ext2fs_size(ip);
	if ((ip->i_e2fs_flags & EXT2_APPEND) &&
	    uio->uio_offset != ext2fs_size(ip))
		return (EPERM);

	fs = ip->i_e2fs;
	if (uio->uio_offset < 0 ||
	    (uint64_t)uio->uio_offset + uio->uio_resid > ump->um_maxfilesize)
		return (EFBIG);
	if (uio->uio_resid == 0)
		return (0);

	async = vp->v_mount->mnt_flag & MNT_ASYNC;
	resid = uio->uio_resid;
	osize = ext2fs_size(ip);

	KASSERT(vp->v_type == VREG);
	while (uio->uio_resid > 0) {
		oldoff = uio->uio_offset;
		blkoffset = ext2_blkoff(fs, uio->uio_offset);
		bytelen = MIN(fs->e2fs_bsize - blkoffset, uio->uio_resid);

		if (vp->v_size < oldoff + bytelen) {
			uvm_vnp_setwritesize(vp, oldoff + bytelen);
		}
		error = ufs_balloc_range(vp, uio->uio_offset, bytelen,
		    ap->a_cred, 0);
		if (error)
			break;
		error = ubc_uiomove(&vp->v_uobj, uio, bytelen, advice,
		    UBC_WRITE | UBC_UNMAP_FLAG(vp));
		if (error)
			break;

		/*
		 * update UVM's notion of the size now that we've
		 * copied the data into the vnode's pages.
		 */

		if (vp->v_size < uio->uio_offset) {
			uvm_vnp_setsize(vp, uio->uio_offset);
			extended = 1;
		}

		/*
		 * flush what we just wrote if necessary.
		 * XXXUBC simplistic async flushing.
		 */

		if (!async && oldoff >> 16 != uio->uio_offset >> 16) {
			mutex_enter(vp->v_interlock);
			error = VOP_PUTPAGES(vp, (oldoff >> 16) << 16,
			    (uio->uio_offset >> 16) << 16,
			    PGO_CLEANIT | PGO_LAZY);
		}
	}
	if (error == 0 && ioflag & IO_SYNC) {
		mutex_enter(vp->v_interlock);
		error = VOP_PUTPAGES(vp, trunc_page(oldoff),
		    round_page(ext2_blkroundup(fs, uio->uio_offset)),
		    PGO_CLEANIT | PGO_SYNCIO);
	}

	error = ext2fs_post_write_update(vp, uio, ioflag, ap->a_cred, osize,
	    resid, extended, error);
	return (error);
}

/*
 * UFS op for writing via the buffer cache
 */
int
ext2fs_bufwr(struct vnode *vp, struct uio *uio, int ioflag, kauth_cred_t cred)
{
	struct inode *ip;
	struct ufsmount *ump;
	struct m_ext2fs *fs;
	struct buf *bp;
	int flags;
	off_t osize;
	daddr_t lbn;
	int resid, blkoffset, xfersize;
	int extended = 0;
	int error;

	KASSERT(VOP_ISLOCKED(vp) == LK_EXCLUSIVE);
	KASSERT(vp->v_type == VDIR || vp->v_type == VLNK);
	KASSERT(vp->v_type != VDIR || ISSET(ioflag, IO_SYNC));
	KASSERT(uio->uio_rw == UIO_WRITE);

	ip = VTOI(vp);
	ump = ip->i_ump;
	fs = ip->i_e2fs;
	error = 0;

	if (uio->uio_offset < 0 ||
	    uio->uio_resid > ump->um_maxfilesize ||
	    uio->uio_offset > (ump->um_maxfilesize - uio->uio_resid))
		return EFBIG;
	if (uio->uio_resid == 0)
		return 0;

	flags = ioflag & IO_SYNC ? B_SYNC : 0;
	resid = uio->uio_resid;
	osize = ext2fs_size(ip);

	for (error = 0; uio->uio_resid > 0;) {
		lbn = ext2_lblkno(fs, uio->uio_offset);
		blkoffset = ext2_blkoff(fs, uio->uio_offset);
		xfersize = MIN(fs->e2fs_bsize - blkoffset, uio->uio_resid);
		if (xfersize < fs->e2fs_bsize)
			flags |= B_CLRBUF;
		else
			flags &= ~B_CLRBUF;
		error = ext2fs_balloc(ip, lbn, blkoffset + xfersize, cred, &bp,
		    flags);
		if (error)
			break;
		if (ext2fs_size(ip) < uio->uio_offset + xfersize) {
			error = ext2fs_setsize(ip, uio->uio_offset + xfersize);
			if (error)
				break;
		}
		error = uiomove((char *)bp->b_data + blkoffset, xfersize, uio);

		/*
		 * update UVM's notion of the size now that we've
		 * copied the data into the vnode's pages.
		 */

		if (vp->v_size < uio->uio_offset) {
			uvm_vnp_setsize(vp, uio->uio_offset);
			extended = 1;
		}

		if (ioflag & IO_SYNC)
			(void)bwrite(bp);
		else if (xfersize + blkoffset == fs->e2fs_bsize)
			bawrite(bp);
		else
			bdwrite(bp);
		if (error || xfersize == 0)
			break;
	}

	error = ext2fs_post_write_update(vp, uio, ioflag, cred, osize, resid,
	    extended, error);
	return (error);
}

static int
ext2fs_post_write_update(struct vnode *vp, struct uio *uio, int ioflag,
    kauth_cred_t cred, off_t osize, int resid, int extended, int oerror)
{
	struct inode *ip = VTOI(vp);
	int error = oerror;

	/* Trigger ctime and mtime updates, and atime if MNT_RELATIME.  */
	ip->i_flag |= IN_CHANGE | IN_UPDATE;
	if (vp->v_mount->mnt_flag & MNT_RELATIME)
		ip->i_flag |= IN_ACCESS;

	/*
	 * If we successfully wrote any data and we are not the superuser,
	 * we clear the setuid and setgid bits as a precaution against
	 * tampering.
	 */
	if (resid > uio->uio_resid && cred) {
		if (ip->i_e2fs_mode & ISUID) {
			if (kauth_authorize_vnode(cred,
			    KAUTH_VNODE_RETAIN_SUID, vp, NULL, EPERM) != 0)
				ip->i_e2fs_mode &= ISUID;
		}

		if (ip->i_e2fs_mode & ISGID) {
			if (kauth_authorize_vnode(cred,
			    KAUTH_VNODE_RETAIN_SGID, vp, NULL, EPERM) != 0)
				ip->i_e2fs_mode &= ~ISGID;
		}
	}

	/* If we successfully wrote anything, notify kevent listeners.  */
	if (resid > uio->uio_resid)
		VN_KNOTE(vp, NOTE_WRITE | (extended ? NOTE_EXTEND : 0));

	/*
	 * Update the size on disk: truncate back to original size on
	 * error, or reflect the new size on success.
	 */
	if (error) {
		(void) ext2fs_truncate(vp, osize, ioflag & IO_SYNC, cred);
		uio->uio_offset -= resid - uio->uio_resid;
		uio->uio_resid = resid;
	} else if (resid > uio->uio_resid && (ioflag & IO_SYNC) == IO_SYNC)
		error = ext2fs_update(vp, NULL, NULL, UPDATE_WAIT);

	/* Make sure the vnode uvm size matches the inode file size.  */
	KASSERT(vp->v_size == ext2fs_size(ip));

	/* Write error overrides any inode update error.  */
	if (oerror)
		error = oerror;
	return error;
}
