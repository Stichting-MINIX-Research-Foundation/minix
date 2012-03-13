/*	$NetBSD: ufs_readwrite.c,v 1.100 2011/11/18 21:18:52 christos Exp $	*/

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
 *	@(#)ufs_readwrite.c	8.11 (Berkeley) 5/8/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(1, "$NetBSD: ufs_readwrite.c,v 1.100 2011/11/18 21:18:52 christos Exp $");

#ifdef LFS_READWRITE
#define	FS			struct lfs
#define	I_FS			i_lfs
#define	READ			lfs_read
#define	READ_S			"lfs_read"
#define	WRITE			lfs_write
#define	WRITE_S			"lfs_write"
#define	fs_bsize		lfs_bsize
#define	fs_bmask		lfs_bmask
#define	UFS_WAPBL_BEGIN(mp)	0
#define	UFS_WAPBL_END(mp)	do { } while (0)
#define	UFS_WAPBL_UPDATE(vp, access, modify, flags)	do { } while (0)
#else
#define	FS			struct fs
#define	I_FS			i_fs
#define	READ			ffs_read
#define	READ_S			"ffs_read"
#define	WRITE			ffs_write
#define	WRITE_S			"ffs_write"
#endif

/*
 * Vnode op for reading.
 */
/* ARGSUSED */
int
READ(void *v)
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
	struct buf *bp;
	FS *fs;
	vsize_t bytelen;
	daddr_t lbn, nextlbn;
	off_t bytesinfile;
	long size, xfersize, blkoffset;
	int error, ioflag;
	bool usepc = false;

	vp = ap->a_vp;
	ip = VTOI(vp);
	ump = ip->i_ump;
	uio = ap->a_uio;
	ioflag = ap->a_ioflag;
	error = 0;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_READ)
		panic("%s: mode", READ_S);

	if (vp->v_type == VLNK) {
		if (ip->i_size < ump->um_maxsymlinklen ||
		    (ump->um_maxsymlinklen == 0 && DIP(ip, blocks) == 0))
			panic("%s: short symlink", READ_S);
	} else if (vp->v_type != VREG && vp->v_type != VDIR)
		panic("%s: type %d", READ_S, vp->v_type);
#endif
	fs = ip->I_FS;
	if ((u_int64_t)uio->uio_offset > ump->um_maxfilesize)
		return (EFBIG);
	if (uio->uio_resid == 0)
		return (0);

#ifndef LFS_READWRITE
	if ((ip->i_flags & (SF_SNAPSHOT | SF_SNAPINVAL)) == SF_SNAPSHOT)
		return ffs_snapshot_read(vp, uio, ioflag);
#endif /* !LFS_READWRITE */

	fstrans_start(vp->v_mount, FSTRANS_SHARED);

	if (uio->uio_offset >= ip->i_size)
		goto out;

#ifdef LFS_READWRITE
	usepc = (vp->v_type == VREG && ip->i_number != LFS_IFILE_INUM);
#else /* !LFS_READWRITE */
	usepc = vp->v_type == VREG;
#endif /* !LFS_READWRITE */
	if (usepc) {
		const int advice = IO_ADV_DECODE(ap->a_ioflag);

		while (uio->uio_resid > 0) {
			if (ioflag & IO_DIRECT) {
				genfs_directio(vp, uio, ioflag);
			}
			bytelen = MIN(ip->i_size - uio->uio_offset,
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
		bytesinfile = ip->i_size - uio->uio_offset;
		if (bytesinfile <= 0)
			break;
		lbn = lblkno(fs, uio->uio_offset);
		nextlbn = lbn + 1;
		size = blksize(fs, ip, lbn);
		blkoffset = blkoff(fs, uio->uio_offset);
		xfersize = MIN(MIN(fs->fs_bsize - blkoffset, uio->uio_resid),
		    bytesinfile);

		if (lblktosize(fs, nextlbn) >= ip->i_size)
			error = bread(vp, lbn, size, NOCRED, 0, &bp);
		else {
			int nextsize = blksize(fs, ip, nextlbn);
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
		if ((ap->a_ioflag & IO_SYNC) == IO_SYNC) {
			error = UFS_WAPBL_BEGIN(vp->v_mount);
			if (error) {
				fstrans_done(vp->v_mount);
				return error;
			}
			error = UFS_UPDATE(vp, NULL, NULL, UPDATE_WAIT);
			UFS_WAPBL_END(vp->v_mount);
		}
	}

	fstrans_done(vp->v_mount);
	return (error);
}

/*
 * Vnode op for writing.
 */
int
WRITE(void *v)
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
	FS *fs;
	struct buf *bp;
	kauth_cred_t cred;
	daddr_t lbn;
	off_t osize, origoff, oldoff, preallocoff, endallocoff, nsize;
	int blkoffset, error, flags, ioflag, resid, size, xfersize;
	int aflag;
	int extended=0;
	vsize_t bytelen;
	bool async;
	bool usepc = false;
#ifdef LFS_READWRITE
	bool need_unreserve = false;
#endif
	struct ufsmount *ump;

	cred = ap->a_cred;
	ioflag = ap->a_ioflag;
	uio = ap->a_uio;
	vp = ap->a_vp;
	ip = VTOI(vp);
	ump = ip->i_ump;

	KASSERT(vp->v_size == ip->i_size);
#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_WRITE)
		panic("%s: mode", WRITE_S);
#endif

	switch (vp->v_type) {
	case VREG:
		if (ioflag & IO_APPEND)
			uio->uio_offset = ip->i_size;
		if ((ip->i_flags & APPEND) && uio->uio_offset != ip->i_size)
			return (EPERM);
		/* FALLTHROUGH */
	case VLNK:
		break;
	case VDIR:
		if ((ioflag & IO_SYNC) == 0)
			panic("%s: nonsync dir write", WRITE_S);
		break;
	default:
		panic("%s: type", WRITE_S);
	}

	fs = ip->I_FS;
	if (uio->uio_offset < 0 ||
	    (u_int64_t)uio->uio_offset + uio->uio_resid > ump->um_maxfilesize)
		return (EFBIG);
#ifdef LFS_READWRITE
	/* Disallow writes to the Ifile, even if noschg flag is removed */
	/* XXX can this go away when the Ifile is no longer in the namespace? */
	if (vp == fs->lfs_ivnode)
		return (EPERM);
#endif
	if (uio->uio_resid == 0)
		return (0);

	fstrans_start(vp->v_mount, FSTRANS_SHARED);

	flags = ioflag & IO_SYNC ? B_SYNC : 0;
	async = vp->v_mount->mnt_flag & MNT_ASYNC;
	origoff = uio->uio_offset;
	resid = uio->uio_resid;
	osize = ip->i_size;
	error = 0;

	usepc = vp->v_type == VREG;

	if ((ioflag & IO_JOURNALLOCKED) == 0) {
		error = UFS_WAPBL_BEGIN(vp->v_mount);
		if (error) {
			fstrans_done(vp->v_mount);
			return error;
		}
	}

#ifdef LFS_READWRITE
	async = true;
	lfs_check(vp, LFS_UNUSED_LBN, 0);
#endif /* !LFS_READWRITE */
	if (!usepc)
		goto bcache;

	preallocoff = round_page(blkroundup(fs, MAX(osize, uio->uio_offset)));
	aflag = ioflag & IO_SYNC ? B_SYNC : 0;
	nsize = MAX(osize, uio->uio_offset + uio->uio_resid);
	endallocoff = nsize - blkoff(fs, nsize);

	/*
	 * if we're increasing the file size, deal with expanding
	 * the fragment if there is one.
	 */

	if (nsize > osize && lblkno(fs, osize) < NDADDR &&
	    lblkno(fs, osize) != lblkno(fs, nsize) &&
	    blkroundup(fs, osize) != osize) {
		off_t eob;

		eob = blkroundup(fs, osize);
		uvm_vnp_setwritesize(vp, eob);
		error = ufs_balloc_range(vp, osize, eob - osize, cred, aflag);
		if (error)
			goto out;
		if (flags & B_SYNC) {
			mutex_enter(vp->v_interlock);
			VOP_PUTPAGES(vp, trunc_page(osize & fs->fs_bmask),
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
		blkoffset = blkoff(fs, uio->uio_offset);
		bytelen = MIN(fs->fs_bsize - blkoffset, uio->uio_resid);
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
		    blkoff(fs, uio->uio_offset) == 0 &&
		    (uio->uio_offset & PAGE_MASK) == 0) {
			vsize_t len;

			len = trunc_page(bytelen);
			len -= blkoff(fs, len);
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

		error = ubc_uiomove(&vp->v_uobj, uio, bytelen,
		    IO_ADV_DECODE(ioflag), ubc_flags | UBC_UNMAP_FLAG(vp));

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

#ifndef LFS_READWRITE
		if (!async && oldoff >> 16 != uio->uio_offset >> 16) {
			mutex_enter(vp->v_interlock);
			error = VOP_PUTPAGES(vp, (oldoff >> 16) << 16,
			    (uio->uio_offset >> 16) << 16,
			    PGO_CLEANIT | PGO_JOURNALLOCKED);
			if (error)
				break;
		}
#endif
	}
	if (error == 0 && ioflag & IO_SYNC) {
		mutex_enter(vp->v_interlock);
		error = VOP_PUTPAGES(vp, trunc_page(origoff & fs->fs_bmask),
		    round_page(blkroundup(fs, uio->uio_offset)),
		    PGO_CLEANIT | PGO_SYNCIO | PGO_JOURNALLOCKED);
	}
	goto out;

 bcache:
	mutex_enter(vp->v_interlock);
	VOP_PUTPAGES(vp, trunc_page(origoff), round_page(origoff + resid),
	    PGO_CLEANIT | PGO_FREE | PGO_SYNCIO | PGO_JOURNALLOCKED);
	while (uio->uio_resid > 0) {
		lbn = lblkno(fs, uio->uio_offset);
		blkoffset = blkoff(fs, uio->uio_offset);
		xfersize = MIN(fs->fs_bsize - blkoffset, uio->uio_resid);
		if (fs->fs_bsize > xfersize)
			flags |= B_CLRBUF;
		else
			flags &= ~B_CLRBUF;

#ifdef LFS_READWRITE
		error = lfs_reserve(fs, vp, NULL,
		    btofsb(fs, (NIADDR + 1) << fs->lfs_bshift));
		if (error)
			break;
		need_unreserve = true;
#endif
		error = UFS_BALLOC(vp, uio->uio_offset, xfersize,
		    ap->a_cred, flags, &bp);

		if (error)
			break;
		if (uio->uio_offset + xfersize > ip->i_size) {
			ip->i_size = uio->uio_offset + xfersize;
			DIP_ASSIGN(ip, size, ip->i_size);
			uvm_vnp_setsize(vp, ip->i_size);
			extended = 1;
		}
		size = blksize(fs, ip, lbn) - bp->b_resid;
		if (xfersize > size)
			xfersize = size;

		error = uiomove((char *)bp->b_data + blkoffset, xfersize, uio);

		/*
		 * if we didn't clear the block and the uiomove failed,
		 * the buf will now contain part of some other file,
		 * so we need to invalidate it.
		 */
		if (error && (flags & B_CLRBUF) == 0) {
			brelse(bp, BC_INVAL);
			break;
		}
#ifdef LFS_READWRITE
		(void)VOP_BWRITE(bp->b_vp, bp);
		lfs_reserve(fs, vp, NULL,
		    -btofsb(fs, (NIADDR + 1) << fs->lfs_bshift));
		need_unreserve = false;
#else
		if (ioflag & IO_SYNC)
			(void)bwrite(bp);
		else if (xfersize + blkoffset == fs->fs_bsize)
			bawrite(bp);
		else
			bdwrite(bp);
#endif
		if (error || xfersize == 0)
			break;
	}
#ifdef LFS_READWRITE
	if (need_unreserve) {
		lfs_reserve(fs, vp, NULL,
		    -btofsb(fs, (NIADDR + 1) << fs->lfs_bshift));
	}
#endif

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
	    kauth_authorize_generic(ap->a_cred, KAUTH_GENERIC_ISSUSER, NULL)) {
		ip->i_mode &= ~(ISUID | ISGID);
		DIP_ASSIGN(ip, mode, ip->i_mode);
	}
	if (resid > uio->uio_resid)
		VN_KNOTE(vp, NOTE_WRITE | (extended ? NOTE_EXTEND : 0));
	if (error) {
		(void) UFS_TRUNCATE(vp, osize, ioflag & IO_SYNC, ap->a_cred);
		uio->uio_offset -= resid - uio->uio_resid;
		uio->uio_resid = resid;
	} else if (resid > uio->uio_resid && (ioflag & IO_SYNC) == IO_SYNC)
		error = UFS_UPDATE(vp, NULL, NULL, UPDATE_WAIT);
	else
		UFS_WAPBL_UPDATE(vp, NULL, NULL, 0);
	KASSERT(vp->v_size == ip->i_size);
	if ((ioflag & IO_JOURNALLOCKED) == 0)
		UFS_WAPBL_END(vp->v_mount);
	fstrans_done(vp->v_mount);

	return (error);
}
