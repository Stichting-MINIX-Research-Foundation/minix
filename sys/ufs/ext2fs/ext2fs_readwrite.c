/*	$NetBSD: ext2fs_readwrite.c,v 1.58 2011/11/18 21:18:51 christos Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: ext2fs_readwrite.c,v 1.58 2011/11/18 21:18:51 christos Exp $");

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
#include <sys/malloc.h>
#include <sys/signalvar.h>
#include <sys/kauth.h>

#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>
#include <ufs/ext2fs/ext2fs.h>
#include <ufs/ext2fs/ext2fs_extern.h>


#define doclusterread 0 /* XXX underway */
#define doclusterwrite 0

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
	struct m_ext2fs *fs;
	struct buf *bp;
	struct ufsmount *ump;
	vsize_t bytelen;
	daddr_t lbn, nextlbn;
	off_t bytesinfile;
	long size, xfersize, blkoffset;
	int error;

	vp = ap->a_vp;
	ip = VTOI(vp);
	ump = ip->i_ump;
	uio = ap->a_uio;
	error = 0;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_READ)
		panic("%s: mode", "ext2fs_read");

	if (vp->v_type == VLNK) {
		if (ext2fs_size(ip) < ump->um_maxsymlinklen ||
		    (ump->um_maxsymlinklen == 0 && ip->i_e2fs_nblock == 0))
			panic("%s: short symlink", "ext2fs_read");
	} else if (vp->v_type != VREG && vp->v_type != VDIR)
		panic("%s: type %d", "ext2fs_read", vp->v_type);
#endif
	fs = ip->i_e2fs;
	if ((uint64_t)uio->uio_offset > ump->um_maxfilesize)
		return (EFBIG);
	if (uio->uio_resid == 0)
		return (0);
	if (uio->uio_offset >= ext2fs_size(ip))
		goto out;

	if (vp->v_type == VREG) {
		const int advice = IO_ADV_DECODE(ap->a_ioflag);

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
		goto out;
	}

	for (error = 0, bp = NULL; uio->uio_resid > 0; bp = NULL) {
		bytesinfile = ext2fs_size(ip) - uio->uio_offset;
		if (bytesinfile <= 0)
			break;
		lbn = lblkno(fs, uio->uio_offset);
		nextlbn = lbn + 1;
		size = fs->e2fs_bsize;
		blkoffset = blkoff(fs, uio->uio_offset);
		xfersize = fs->e2fs_bsize - blkoffset;
		if (uio->uio_resid < xfersize)
			xfersize = uio->uio_resid;
		if (bytesinfile < xfersize)
			xfersize = bytesinfile;

		if (lblktosize(fs, nextlbn) >= ext2fs_size(ip))
			error = bread(vp, lbn, size, NOCRED, 0, &bp);
		else {
			int nextsize = fs->e2fs_bsize;
			error = breadn(vp, lbn,
				size, &nextlbn, &nextsize, 1, NOCRED, 0, &bp);
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
	if (!(vp->v_mount->mnt_flag & MNT_NOATIME)) {
		ip->i_flag |= IN_ACCESS;
		if ((ap->a_ioflag & IO_SYNC) == IO_SYNC)
			error = ext2fs_update(vp, NULL, NULL, UPDATE_WAIT);
	}
	return (error);
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
	struct buf *bp;
	struct ufsmount *ump;
	daddr_t lbn;
	off_t osize;
	int blkoffset, error, flags, ioflag, resid, xfersize;
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

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_WRITE)
		panic("%s: mode", "ext2fs_write");
#endif

	switch (vp->v_type) {
	case VREG:
		if (ioflag & IO_APPEND)
			uio->uio_offset = ext2fs_size(ip);
		if ((ip->i_e2fs_flags & EXT2_APPEND) &&
		    uio->uio_offset != ext2fs_size(ip))
			return (EPERM);
		/* FALLTHROUGH */
	case VLNK:
		break;
	case VDIR:
		if ((ioflag & IO_SYNC) == 0)
			panic("%s: nonsync dir write", "ext2fs_write");
		break;
	default:
		panic("%s: type", "ext2fs_write");
	}

	fs = ip->i_e2fs;
	if (uio->uio_offset < 0 ||
	    (uint64_t)uio->uio_offset + uio->uio_resid > ump->um_maxfilesize)
		return (EFBIG);
	if (uio->uio_resid == 0)
		return (0);

	async = vp->v_mount->mnt_flag & MNT_ASYNC;
	resid = uio->uio_resid;
	osize = ext2fs_size(ip);

	if (vp->v_type == VREG) {
		while (uio->uio_resid > 0) {
			oldoff = uio->uio_offset;
			blkoffset = blkoff(fs, uio->uio_offset);
			bytelen = MIN(fs->e2fs_bsize - blkoffset,
			    uio->uio_resid);

			if (vp->v_size < oldoff + bytelen) {
				uvm_vnp_setwritesize(vp, oldoff + bytelen);
			}
			error = ufs_balloc_range(vp, uio->uio_offset,
			    bytelen, ap->a_cred, 0);
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
				    (uio->uio_offset >> 16) << 16, PGO_CLEANIT);
			}
		}
		if (error == 0 && ioflag & IO_SYNC) {
			mutex_enter(vp->v_interlock);
			error = VOP_PUTPAGES(vp, trunc_page(oldoff),
			    round_page(blkroundup(fs, uio->uio_offset)),
			    PGO_CLEANIT | PGO_SYNCIO);
		}

		goto out;
	}

	flags = ioflag & IO_SYNC ? B_SYNC : 0;
	for (error = 0; uio->uio_resid > 0;) {
		lbn = lblkno(fs, uio->uio_offset);
		blkoffset = blkoff(fs, uio->uio_offset);
		xfersize = MIN(fs->e2fs_bsize - blkoffset, uio->uio_resid);
		if (xfersize < fs->e2fs_bsize)
			flags |= B_CLRBUF;
		else
			flags &= ~B_CLRBUF;
		error = ext2fs_balloc(ip,
		    lbn, blkoffset + xfersize, ap->a_cred, &bp, flags);
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

	/*
	 * If we successfully wrote any data, and we are not the superuser
	 * we clear the setuid and setgid bits as a precaution against
	 * tampering.
	 */

out:
	ip->i_flag |= IN_CHANGE | IN_UPDATE;
	if (vp->v_mount->mnt_flag & MNT_RELATIME)
		ip->i_flag |= IN_ACCESS;
	if (resid > uio->uio_resid && ap->a_cred &&
	    kauth_authorize_generic(ap->a_cred, KAUTH_GENERIC_ISSUSER, NULL))
		ip->i_e2fs_mode &= ~(ISUID | ISGID);
	if (resid > uio->uio_resid)
		VN_KNOTE(vp, NOTE_WRITE | (extended ? NOTE_EXTEND : 0));
	if (error) {
		(void) ext2fs_truncate(vp, osize, ioflag & IO_SYNC, ap->a_cred);
		uio->uio_offset -= resid - uio->uio_resid;
		uio->uio_resid = resid;
	} else if (resid > uio->uio_resid && (ioflag & IO_SYNC) == IO_SYNC)
		error = ext2fs_update(vp, NULL, NULL, UPDATE_WAIT);
	KASSERT(vp->v_size == ext2fs_size(ip));
	return (error);
}
