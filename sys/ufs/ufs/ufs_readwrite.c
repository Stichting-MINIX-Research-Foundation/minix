/*	$NetBSD: ufs_readwrite.c,v 1.120 2015/04/12 22:48:38 riastradh Exp $	*/

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
__KERNEL_RCSID(1, "$NetBSD: ufs_readwrite.c,v 1.120 2015/04/12 22:48:38 riastradh Exp $");

#ifdef LFS_READWRITE
#define	FS			struct lfs
#define	I_FS			i_lfs
#define	READ			lfs_read
#define	READ_S			"lfs_read"
#define	WRITE			lfs_write
#define	WRITE_S			"lfs_write"
#define	BUFRD			lfs_bufrd
#define	BUFWR			lfs_bufwr
#define	fs_bsize		lfs_bsize
#define	fs_bmask		lfs_bmask
#define	UFS_WAPBL_BEGIN(mp)	0
#define	UFS_WAPBL_END(mp)	do { } while (0)
#define	UFS_WAPBL_UPDATE(vp, access, modify, flags)	do { } while (0)
#define ufs_blkoff		lfs_blkoff
#define ufs_blksize		lfs_blksize
#define ufs_lblkno		lfs_lblkno
#define ufs_lblktosize		lfs_lblktosize
#define ufs_blkroundup		lfs_blkroundup
#else
#define	FS			struct fs
#define	I_FS			i_fs
#define	READ			ffs_read
#define	READ_S			"ffs_read"
#define	WRITE			ffs_write
#define	WRITE_S			"ffs_write"
#define	BUFRD			ffs_bufrd
#define	BUFWR			ffs_bufwr
#define ufs_blkoff		ffs_blkoff
#define ufs_blksize		ffs_blksize
#define ufs_lblkno		ffs_lblkno
#define ufs_lblktosize		ffs_lblktosize
#define ufs_blkroundup		ffs_blkroundup
#endif

static int	ufs_post_read_update(struct vnode *, int, int);
static int	ufs_post_write_update(struct vnode *, struct uio *, int,
		    kauth_cred_t, off_t, int, int, int);

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
	vsize_t bytelen;
	int error, ioflag, advice;

	vp = ap->a_vp;
	ip = VTOI(vp);
	ump = ip->i_ump;
	uio = ap->a_uio;
	ioflag = ap->a_ioflag;
	error = 0;

	KASSERT(uio->uio_rw == UIO_READ);
	KASSERT(vp->v_type == VREG || vp->v_type == VDIR);

	/* XXX Eliminate me by refusing directory reads from userland.  */
	if (vp->v_type == VDIR)
		return BUFRD(vp, uio, ioflag, ap->a_cred);
#ifdef LFS_READWRITE
	/* XXX Eliminate me by using ufs_bufio in lfs.  */
	if (vp->v_type == VREG && ip->i_number == LFS_IFILE_INUM)
		return BUFRD(vp, uio, ioflag, ap->a_cred);
#endif
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

	KASSERT(vp->v_type == VREG);
	advice = IO_ADV_DECODE(ap->a_ioflag);
	while (uio->uio_resid > 0) {
		if (ioflag & IO_DIRECT) {
			genfs_directio(vp, uio, ioflag);
		}
		bytelen = MIN(ip->i_size - uio->uio_offset, uio->uio_resid);
		if (bytelen == 0)
			break;
		error = ubc_uiomove(&vp->v_uobj, uio, bytelen, advice,
		    UBC_READ | UBC_PARTIALOK | UBC_UNMAP_FLAG(vp));
		if (error)
			break;
	}

 out:
	error = ufs_post_read_update(vp, ap->a_ioflag, error);
	fstrans_done(vp->v_mount);
	return (error);
}

/*
 * UFS op for reading via the buffer cache
 */
int
BUFRD(struct vnode *vp, struct uio *uio, int ioflag, kauth_cred_t cred)
{
	struct inode *ip;
	struct ufsmount *ump;
	FS *fs;
	struct buf *bp;
	daddr_t lbn, nextlbn;
	off_t bytesinfile;
	long size, xfersize, blkoffset;
	int error;

	KASSERT(VOP_ISLOCKED(vp));
	KASSERT(vp->v_type == VDIR || vp->v_type == VLNK);
	KASSERT(uio->uio_rw == UIO_READ);

	ip = VTOI(vp);
	ump = ip->i_ump;
	fs = ip->I_FS;
	error = 0;

	KASSERT(vp->v_type != VLNK || ip->i_size >= ump->um_maxsymlinklen);
	KASSERT(vp->v_type != VLNK || ump->um_maxsymlinklen != 0 ||
	    DIP(ip, blocks) == 0);

	if (uio->uio_offset > ump->um_maxfilesize)
		return EFBIG;
	if (uio->uio_resid == 0)
		return 0;

#ifndef LFS_READWRITE
	KASSERT(!ISSET(ip->i_flags, (SF_SNAPSHOT | SF_SNAPINVAL)));
#endif

	fstrans_start(vp->v_mount, FSTRANS_SHARED);

	if (uio->uio_offset >= ip->i_size)
		goto out;

	for (error = 0, bp = NULL; uio->uio_resid > 0; bp = NULL) {
		bytesinfile = ip->i_size - uio->uio_offset;
		if (bytesinfile <= 0)
			break;
		lbn = ufs_lblkno(fs, uio->uio_offset);
		nextlbn = lbn + 1;
		size = ufs_blksize(fs, ip, lbn);
		blkoffset = ufs_blkoff(fs, uio->uio_offset);
		xfersize = MIN(MIN(fs->fs_bsize - blkoffset, uio->uio_resid),
		    bytesinfile);

		if (ufs_lblktosize(fs, nextlbn) >= ip->i_size)
			error = bread(vp, lbn, size, 0, &bp);
		else {
			int nextsize = ufs_blksize(fs, ip, nextlbn);
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
	error = ufs_post_read_update(vp, ioflag, error);
	fstrans_done(vp->v_mount);
	return (error);
}

static int
ufs_post_read_update(struct vnode *vp, int ioflag, int oerror)
{
	struct inode *ip = VTOI(vp);
	int error = oerror;

	if (!(vp->v_mount->mnt_flag & MNT_NOATIME)) {
		ip->i_flag |= IN_ACCESS;
		if ((ioflag & IO_SYNC) == IO_SYNC) {
			error = UFS_WAPBL_BEGIN(vp->v_mount);
			if (error)
				goto out;
			error = UFS_UPDATE(vp, NULL, NULL, UPDATE_WAIT);
			UFS_WAPBL_END(vp->v_mount);
		}
	}

out:
	/* Read error overrides any inode update error.  */
	if (oerror)
		error = oerror;
	return error;
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
	ump = ip->i_ump;

	KASSERT(vp->v_size == ip->i_size);
	KASSERT(uio->uio_rw == UIO_WRITE);
	KASSERT(vp->v_type == VREG);
	KASSERT(!ISSET(ioflag, IO_JOURNALLOCKED));
	UFS_WAPBL_JUNLOCK_ASSERT(vp->v_mount);

	if (ioflag & IO_APPEND)
		uio->uio_offset = ip->i_size;
	if ((ip->i_flags & APPEND) && uio->uio_offset != ip->i_size)
		return (EPERM);

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

	KASSERT(vp->v_type == VREG);

	/*
	 * XXX The entire write operation must occur in a single WAPBL
	 * transaction because it may allocate disk blocks, if
	 * appending or filling holes, which is allowed to happen only
	 * if the write fully succeeds.
	 *
	 * If ubc_uiomove fails in the middle with EFAULT, we can clean
	 * up at the end with UFS_TRUNCATE.  But if the power fails in
	 * the middle, there would be nobody to deallocate the blocks,
	 * without an fsck to globally analyze the file system.
	 *
	 * If the increasingly inaccurately named WAPBL were augmented
	 * with rollback records for block allocations, then we could
	 * split this into multiple transactions and commit the
	 * allocations in the last one.
	 *
	 * But WAPBL doesn't have that notion now, so we'll have to
	 * live with gigantic transactions and WAPBL tentacles in
	 * genfs_getpages/putpages to cope with the possibility that
	 * the transaction may or may not be locked on entry to the
	 * page cache.
	 *
	 * And even if we added that notion to WAPBL, it wouldn't help
	 * us get rid of the tentacles in genfs_getpages/putpages
	 * because we'd have to interoperate with old implementations
	 * that assume they can replay the log without fsck.
	 */
	error = UFS_WAPBL_BEGIN(vp->v_mount);
	if (error) {
		fstrans_done(vp->v_mount);
		return error;
	}

#ifdef LFS_READWRITE
	async = true;
	lfs_availwait(fs, btofsb(fs, uio->uio_resid));
	lfs_check(vp, LFS_UNUSED_LBN, 0);
#endif /* !LFS_READWRITE */

	preallocoff = round_page(ufs_blkroundup(fs, MAX(osize, uio->uio_offset)));
	aflag = ioflag & IO_SYNC ? B_SYNC : 0;
	nsize = MAX(osize, uio->uio_offset + uio->uio_resid);
	endallocoff = nsize - ufs_blkoff(fs, nsize);

	/*
	 * if we're increasing the file size, deal with expanding
	 * the fragment if there is one.
	 */

	if (nsize > osize && ufs_lblkno(fs, osize) < UFS_NDADDR &&
	    ufs_lblkno(fs, osize) != ufs_lblkno(fs, nsize) &&
	    ufs_blkroundup(fs, osize) != osize) {
		off_t eob;

		eob = ufs_blkroundup(fs, osize);
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
		blkoffset = ufs_blkoff(fs, uio->uio_offset);
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
		    ufs_blkoff(fs, uio->uio_offset) == 0 &&
		    (uio->uio_offset & PAGE_MASK) == 0) {
			vsize_t len;

			len = trunc_page(bytelen);
			len -= ufs_blkoff(fs, len);
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
			    PGO_CLEANIT | PGO_JOURNALLOCKED | PGO_LAZY);
			if (error)
				break;
		}
#endif
	}
	if (error == 0 && ioflag & IO_SYNC) {
		mutex_enter(vp->v_interlock);
		error = VOP_PUTPAGES(vp, trunc_page(origoff & fs->fs_bmask),
		    round_page(ufs_blkroundup(fs, uio->uio_offset)),
		    PGO_CLEANIT | PGO_SYNCIO | PGO_JOURNALLOCKED);
	}

out:
	error = ufs_post_write_update(vp, uio, ioflag, cred, osize, resid,
	    extended, error);
	UFS_WAPBL_END(vp->v_mount);
	fstrans_done(vp->v_mount);

	return (error);
}

/*
 * UFS op for writing via the buffer cache
 */
int
BUFWR(struct vnode *vp, struct uio *uio, int ioflag, kauth_cred_t cred)
{
	struct inode *ip;
	struct ufsmount *ump;
	FS *fs;
	int flags;
	struct buf *bp;
	off_t osize;
	int resid, xfersize, size, blkoffset;
	daddr_t lbn;
	int extended=0;
	int error;
#ifdef LFS_READWRITE
	bool need_unreserve = false;
#endif

	KASSERT(ISSET(ioflag, IO_NODELOCKED));
	KASSERT(VOP_ISLOCKED(vp) == LK_EXCLUSIVE);
	KASSERT(vp->v_type == VDIR || vp->v_type == VLNK);
	KASSERT(vp->v_type != VDIR || ISSET(ioflag, IO_SYNC));
	KASSERT(uio->uio_rw == UIO_WRITE);
	KASSERT(ISSET(ioflag, IO_JOURNALLOCKED));
	UFS_WAPBL_JLOCK_ASSERT(vp->v_mount);

	ip = VTOI(vp);
	ump = ip->i_ump;
	fs = ip->I_FS;

	KASSERT(vp->v_size == ip->i_size);

	if (uio->uio_offset < 0 ||
	    uio->uio_resid > ump->um_maxfilesize ||
	    uio->uio_offset > (ump->um_maxfilesize - uio->uio_resid))
		return EFBIG;
#ifdef LFS_READWRITE
	KASSERT(vp != fs->lfs_ivnode);
#endif
	if (uio->uio_resid == 0)
		return 0;

	fstrans_start(vp->v_mount, FSTRANS_SHARED);

	flags = ioflag & IO_SYNC ? B_SYNC : 0;
	resid = uio->uio_resid;
	osize = ip->i_size;
	error = 0;

	KASSERT(vp->v_type != VREG);

#ifdef LFS_READWRITE
	lfs_availwait(fs, btofsb(fs, uio->uio_resid));
	lfs_check(vp, LFS_UNUSED_LBN, 0);
#endif /* !LFS_READWRITE */

	/* XXX Should never have pages cached here.  */
	KASSERT(vp->v_uobj.uo_npages == 0);
	while (uio->uio_resid > 0) {
		lbn = ufs_lblkno(fs, uio->uio_offset);
		blkoffset = ufs_blkoff(fs, uio->uio_offset);
		xfersize = MIN(fs->fs_bsize - blkoffset, uio->uio_resid);
		if (fs->fs_bsize > xfersize)
			flags |= B_CLRBUF;
		else
			flags &= ~B_CLRBUF;

#ifdef LFS_READWRITE
		error = lfs_reserve(fs, vp, NULL,
		    btofsb(fs, (UFS_NIADDR + 1) << fs->lfs_bshift));
		if (error)
			break;
		need_unreserve = true;
#endif
		error = UFS_BALLOC(vp, uio->uio_offset, xfersize, cred, flags,
		    &bp);

		if (error)
			break;
		if (uio->uio_offset + xfersize > ip->i_size) {
			ip->i_size = uio->uio_offset + xfersize;
			DIP_ASSIGN(ip, size, ip->i_size);
			uvm_vnp_setsize(vp, ip->i_size);
			extended = 1;
		}
		size = ufs_blksize(fs, ip, lbn) - bp->b_resid;
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
		    -btofsb(fs, (UFS_NIADDR + 1) << fs->lfs_bshift));
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
		    -btofsb(fs, (UFS_NIADDR + 1) << fs->lfs_bshift));
	}
#endif

	error = ufs_post_write_update(vp, uio, ioflag, cred, osize, resid,
	    extended, error);
	fstrans_done(vp->v_mount);

	return (error);
}

static int
ufs_post_write_update(struct vnode *vp, struct uio *uio, int ioflag,
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
		if (ip->i_mode & ISUID) {
			if (kauth_authorize_vnode(cred,
			    KAUTH_VNODE_RETAIN_SUID, vp, NULL, EPERM) != 0) {
				ip->i_mode &= ~ISUID;
				DIP_ASSIGN(ip, mode, ip->i_mode);
			}
		}

		if (ip->i_mode & ISGID) {
			if (kauth_authorize_vnode(cred,
			    KAUTH_VNODE_RETAIN_SGID, vp, NULL, EPERM) != 0) {
				ip->i_mode &= ~ISGID;
				DIP_ASSIGN(ip, mode, ip->i_mode);
			}
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
		(void) UFS_TRUNCATE(vp, osize, ioflag & IO_SYNC, cred);
		uio->uio_offset -= resid - uio->uio_resid;
		uio->uio_resid = resid;
	} else if (resid > uio->uio_resid && (ioflag & IO_SYNC) == IO_SYNC)
		error = UFS_UPDATE(vp, NULL, NULL, UPDATE_WAIT);
	else
		UFS_WAPBL_UPDATE(vp, NULL, NULL, 0);

	/* Make sure the vnode uvm size matches the inode file size.  */
	KASSERT(vp->v_size == ip->i_size);

	/* Write error overrides any inode update error.  */
	if (oerror)
		error = oerror;
	return error;
}
