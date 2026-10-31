/*	$NetBSD: ffs_inode.c,v 1.131 2020/07/31 04:07:30 chs Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Wasabi Systems, Inc.
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

/*
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
 *	@(#)ffs_inode.c	8.13 (Berkeley) 4/21/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ffs_inode.c,v 1.131 2020/07/31 04:07:30 chs Exp $");

#if defined(_KERNEL_OPT)
#include "opt_ffs.h"
#include "opt_quota.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/file.h>
#include <sys/fstrans.h>
#include <sys/kauth.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/trace.h>
#include <sys/vnode.h>
#include <sys/wapbl.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>
#include <ufs/ufs/ufs_bswap.h>
#include <ufs/ufs/ufs_wapbl.h>

#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>

static int ffs_indirtrunc(struct inode *, daddr_t, daddr_t, daddr_t, int,
			  int64_t *);

/*
 * Update the access, modified, and inode change times as specified
 * by the IN_ACCESS, IN_UPDATE, and IN_CHANGE flags respectively.
 * The IN_MODIFIED flag is used to specify that the inode needs to be
 * updated but that the times have already been set. The access
 * and modified times are taken from the second and third parameters;
 * the inode change time is always taken from the current time. If
 * UPDATE_WAIT flag is set, or UPDATE_DIROP is set then wait for the
 * disk write of the inode to complete.
 */

int
ffs_update(struct vnode *vp, const struct timespec *acc,
    const struct timespec *mod, int updflags)
{
	struct fs *fs;
	struct buf *bp;
	struct inode *ip;
	int error;
	void *cp;
	int waitfor, flags;

	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return (0);
	ip = VTOI(vp);
	FFS_ITIMES(ip, acc, mod, NULL);
	if (updflags & UPDATE_CLOSE)
		flags = ip->i_flag & (IN_MODIFIED | IN_ACCESSED);
	else
		flags = ip->i_flag & IN_MODIFIED;
	if (flags == 0)
		return (0);
	fs = ip->i_fs;

	if ((flags & IN_MODIFIED) != 0 &&
	    (vp->v_mount->mnt_flag & MNT_ASYNC) == 0) {
		waitfor = updflags & UPDATE_WAIT;
		if ((updflags & UPDATE_DIROP) != 0)
			waitfor |= UPDATE_WAIT;
	} else
		waitfor = 0;

	/*
	 * Ensure that uid and gid are correct. This is a temporary
	 * fix until fsck has been changed to do the update.
	 */
	if (fs->fs_magic == FS_UFS1_MAGIC &&			/* XXX */
	    fs->fs_old_inodefmt < FS_44INODEFMT) {		/* XXX */
		ip->i_ffs1_ouid = ip->i_uid;	/* XXX */
		ip->i_ffs1_ogid = ip->i_gid;	/* XXX */
	}							/* XXX */
	error = bread(ip->i_devvp,
		      FFS_FSBTODB(fs, ino_to_fsba(fs, ip->i_number)),
		      (int)fs->fs_bsize, B_MODIFY, &bp);
	if (error) {
		return (error);
	}
	ip->i_flag &= ~(IN_MODIFIED | IN_ACCESSED);
	/* Keep unlinked inode list up to date */
	KDASSERTMSG(DIP(ip, nlink) == ip->i_nlink,
	    "DIP(ip, nlink) [%d] == ip->i_nlink [%d]",
	    DIP(ip, nlink), ip->i_nlink);
	if (ip->i_mode) {
		if (ip->i_nlink > 0) {
			UFS_WAPBL_UNREGISTER_INODE(ip->i_ump->um_mountp,
			    ip->i_number, ip->i_mode);
		} else {
			UFS_WAPBL_REGISTER_INODE(ip->i_ump->um_mountp,
			    ip->i_number, ip->i_mode);
		}
	}
	if (fs->fs_magic == FS_UFS1_MAGIC) {
		cp = (char *)bp->b_data +
		    (ino_to_fsbo(fs, ip->i_number) * DINODE1_SIZE);
#ifdef FFS_EI
		if (UFS_FSNEEDSWAP(fs))
			ffs_dinode1_swap(ip->i_din.ffs1_din,
			    (struct ufs1_dinode *)cp);
		else
#endif
			memcpy(cp, ip->i_din.ffs1_din, DINODE1_SIZE);
	} else {
		cp = (char *)bp->b_data +
		    (ino_to_fsbo(fs, ip->i_number) * DINODE2_SIZE);
#ifdef FFS_EI
		if (UFS_FSNEEDSWAP(fs))
			ffs_dinode2_swap(ip->i_din.ffs2_din,
			    (struct ufs2_dinode *)cp);
		else
#endif
			memcpy(cp, ip->i_din.ffs2_din, DINODE2_SIZE);
	}
	if (waitfor) {
		return (bwrite(bp));
	} else {
		bdwrite(bp);
		return (0);
	}
}

#define	SINGLE	0	/* index of single indirect block */
#define	DOUBLE	1	/* index of double indirect block */
#define	TRIPLE	2	/* index of triple indirect block */
/*
 * Truncate the inode oip to at most length size, freeing the
 * disk blocks.
 */
int
ffs_truncate(struct vnode *ovp, off_t length, int ioflag, kauth_cred_t cred)
{
	daddr_t lastblock;
	struct inode *oip = VTOI(ovp);
	struct mount *omp = ovp->v_mount;
	daddr_t bn, lastiblock[UFS_NIADDR], indir_lbn[UFS_NIADDR];
	daddr_t blks[UFS_NDADDR + UFS_NIADDR], oldblks[UFS_NDADDR + UFS_NIADDR];
	struct fs *fs;
	int extblocks;
	int offset, pgoffset, level;
	int64_t blocksreleased = 0, datablocks;
	int i, aflag, nblocks;
	int error, allerror = 0;
	off_t osize;
	int sync;
	struct ufsmount *ump = oip->i_ump;
	void *dcookie;
	long bsize;
	bool wapbl = omp->mnt_wapbl != NULL;

	UFS_WAPBL_JLOCK_ASSERT(ump->um_mountp);

	if (ovp->v_type == VCHR || ovp->v_type == VBLK ||
	    ovp->v_type == VFIFO || ovp->v_type == VSOCK) {
		KASSERT(oip->i_size == 0);
		return 0;
	}

	if (length < 0)
		return (EINVAL);

	/*
	 * Historically clients did not have to specify which data
	 * they were truncating. So, if not specified, we assume
	 * traditional behavior, e.g., just the normal data.
	 */
	if ((ioflag & (IO_EXT | IO_NORMAL)) == 0)
		ioflag |= IO_NORMAL;

	fs = oip->i_fs;
#define i_din2 i_din.ffs2_din
	extblocks = 0;
	datablocks = DIP(oip, blocks);
	if (fs->fs_magic == FS_UFS2_MAGIC && oip->i_din2->di_extsize > 0) {
		extblocks = btodb(ffs_fragroundup(fs, oip->i_din2->di_extsize));
		datablocks -= extblocks;
	}
	if ((ioflag & IO_EXT) && extblocks > 0) {
		if (length != 0)
			panic("ffs_truncate: partial trunc of extdata");
		{
#ifdef QUOTA
			(void) chkdq(oip, -extblocks, NOCRED, FORCE);
#endif
			osize = oip->i_din2->di_extsize;
			oip->i_din2->di_blocks -= extblocks;
			oip->i_din2->di_extsize = 0;
			for (i = 0; i < UFS_NXADDR; i++) {
				binvalbuf(ovp, -1 - i);
				oldblks[i] = oip->i_din2->di_extb[i];
				oip->i_din2->di_extb[i] = 0;
			}
			oip->i_flag |= IN_CHANGE;
			if ((error = ffs_update(ovp, NULL, NULL, 0)))
				return (error);
			for (i = 0; i < UFS_NXADDR; i++) {
				if (oldblks[i] == 0)
					continue;
				bsize = ffs_sblksize(fs, osize, i);
				if (wapbl) {
					error = UFS_WAPBL_REGISTER_DEALLOCATION(omp,
					    FFS_FSBTODB(fs, oldblks[i]), bsize, NULL);
					if (error)
						return error;
				} else 
					ffs_blkfree(fs, oip->i_devvp, oldblks[i],
					    bsize, oip->i_number);
			}
			extblocks = 0;
		}
	}
	if ((ioflag & IO_NORMAL) == 0)
		return (0);
	if (ovp->v_type == VLNK &&
	    (oip->i_size < ump->um_maxsymlinklen ||
	     (ump->um_maxsymlinklen == 0 && datablocks == 0))) {
		KDASSERT(length == 0);
		memset(SHORTLINK(oip), 0, (size_t)oip->i_size);
		oip->i_size = 0;
		DIP_ASSIGN(oip, size, 0);
		oip->i_flag |= IN_CHANGE | IN_UPDATE;
		return (ffs_update(ovp, NULL, NULL, 0));
	}
	if (oip->i_size == length) {
		/* still do a uvm_vnp_setsize() as writesize may be larger */
		uvm_vnp_setsize(ovp, length);
		oip->i_flag |= IN_CHANGE | IN_UPDATE;
		return (ffs_update(ovp, NULL, NULL, 0));
	}
	if (length > ump->um_maxfilesize)
		return (EFBIG);

	if ((oip->i_flags & SF_SNAPSHOT) != 0)
		ffs_snapremove(ovp);

	osize = oip->i_size;
	aflag = ioflag & IO_SYNC ? B_SYNC : 0;

	/*
	 * Lengthen the size of the file. We must ensure that the
	 * last byte of the file is allocated. Since the smallest
	 * value of osize is 0, length will be at least 1.
	 */

	if (osize < length) {
		if (ffs_lblkno(fs, osize) < UFS_NDADDR &&
		    ffs_lblkno(fs, osize) != ffs_lblkno(fs, length) &&
		    ffs_blkroundup(fs, osize) != osize) {
			off_t eob;

			eob = ffs_blkroundup(fs, osize);
			uvm_vnp_setwritesize(ovp, eob);
			error = ufs_balloc_range(ovp, osize, eob - osize,
			    cred, aflag);
			if (error) {
				(void) ffs_truncate(ovp, osize,
				    ioflag & IO_SYNC, cred);
				return error;
			}
			if (ioflag & IO_SYNC) {
				rw_enter(ovp->v_uobj.vmobjlock, RW_WRITER);
				VOP_PUTPAGES(ovp,
				    trunc_page(osize & fs->fs_bmask),
				    round_page(eob), PGO_CLEANIT | PGO_SYNCIO |
				    PGO_JOURNALLOCKED);
			}
		}
		uvm_vnp_setwritesize(ovp, length);
		error = ufs_balloc_range(ovp, length - 1, 1, cred, aflag);
		if (error) {
			(void) ffs_truncate(ovp, osize, ioflag & IO_SYNC, cred);
			return (error);
		}
		uvm_vnp_setsize(ovp, length);
		oip->i_flag |= IN_CHANGE | IN_UPDATE;
		KASSERT(ovp->v_size == oip->i_size);
		return (ffs_update(ovp, NULL, NULL, 0));
	}

	/*
	 * When truncating a regular file down to a non-block-aligned size,
	 * we must zero the part of last block which is past the new EOF.
	 * We must synchronously flush the zeroed pages to disk
	 * since the new pages will be invalidated as soon as we
	 * inform the VM system of the new, smaller size.
	 * We must do this before acquiring the GLOCK, since fetching
	 * the pages will acquire the GLOCK internally.
	 * So there is a window where another thread could see a whole
	 * zeroed page past EOF, but that's life.
	 */

	offset = ffs_blkoff(fs, length);
	pgoffset = length & PAGE_MASK;
	if (ovp->v_type == VREG && (pgoffset != 0 || offset != 0) &&
	    osize > length) {
		daddr_t lbn;
		voff_t eoz;
		int size;

		if (offset != 0) {
			error = ufs_balloc_range(ovp, length - 1, 1, cred,
			    aflag);
			if (error)
				return error;
		}
		lbn = ffs_lblkno(fs, length);
		size = ffs_blksize(fs, oip, lbn);
		eoz = MIN(MAX(ffs_lblktosize(fs, lbn) + size, round_page(pgoffset)),
		    osize);
		ubc_zerorange(&ovp->v_uobj, length, eoz - length,
		    UBC_VNODE_FLAGS(ovp));
		if (round_page(eoz) > round_page(length)) {
			rw_enter(ovp->v_uobj.vmobjlock, RW_WRITER);
			error = VOP_PUTPAGES(ovp, round_page(length),
			    round_page(eoz),
			    PGO_CLEANIT | PGO_DEACTIVATE | PGO_JOURNALLOCKED |
			    ((ioflag & IO_SYNC) ? PGO_SYNCIO : 0));
			if (error)
				return error;
		}
	}

	genfs_node_wrlock(ovp);
	oip->i_size = length;
	DIP_ASSIGN(oip, size, length);
	uvm_vnp_setsize(ovp, length);
	/*
	 * Calculate index into inode's block list of
	 * last direct and indirect blocks (if any)
	 * which we want to keep.  Lastblock is -1 when
	 * the file is truncated to 0.
	 */
	lastblock = ffs_lblkno(fs, length + fs->fs_bsize - 1) - 1;
	lastiblock[SINGLE] = lastblock - UFS_NDADDR;
	lastiblock[DOUBLE] = lastiblock[SINGLE] - FFS_NINDIR(fs);
	lastiblock[TRIPLE] = lastiblock[DOUBLE] - FFS_NINDIR(fs) * FFS_NINDIR(fs);
	nblocks = btodb(fs->fs_bsize);
	/*
	 * Update file and block pointers on disk before we start freeing
	 * blocks.  If we crash before free'ing blocks below, the blocks
	 * will be returned to the free list.  lastiblock values are also
	 * normalized to -1 for calls to ffs_indirtrunc below.
	 */
	sync = 0;
	for (level = TRIPLE; level >= SINGLE; level--) {
		blks[UFS_NDADDR + level] = DIP(oip, ib[level]);
		if (lastiblock[level] < 0 && blks[UFS_NDADDR + level] != 0) {
			sync = 1;
			DIP_ASSIGN(oip, ib[level], 0);
			lastiblock[level] = -1;
		}
	}
	for (i = 0; i < UFS_NDADDR; i++) {
		blks[i] = DIP(oip, db[i]);
		if (i > lastblock && blks[i] != 0) {
			sync = 1;
			DIP_ASSIGN(oip, db[i], 0);
		}
	}
	oip->i_flag |= IN_CHANGE | IN_UPDATE;
	if (sync) {
		error = ffs_update(ovp, NULL, NULL, UPDATE_WAIT);
		if (error && !allerror)
			allerror = error;
	}

	/*
	 * Having written the new inode to disk, save its new configuration
	 * and put back the old block pointers long enough to process them.
	 * Note that we save the new block configuration so we can check it
	 * when we are done.
	 */
	for (i = 0; i < UFS_NDADDR; i++) {
		bn = DIP(oip, db[i]);
		DIP_ASSIGN(oip, db[i], blks[i]);
		blks[i] = bn;
	}
	for (i = 0; i < UFS_NIADDR; i++) {
		bn = DIP(oip, ib[i]);
		DIP_ASSIGN(oip, ib[i], blks[UFS_NDADDR + i]);
		blks[UFS_NDADDR + i] = bn;
	}

	oip->i_size = osize;
	DIP_ASSIGN(oip, size, osize);
	error = vtruncbuf(ovp, lastblock + 1, 0, 0);
	if (error && !allerror)
		allerror = error;

	/*
	 * Indirect blocks first.
	 */
	indir_lbn[SINGLE] = -UFS_NDADDR;
	indir_lbn[DOUBLE] = indir_lbn[SINGLE] - FFS_NINDIR(fs) - 1;
	indir_lbn[TRIPLE] = indir_lbn[DOUBLE] - FFS_NINDIR(fs) * FFS_NINDIR(fs) - 1;
	for (level = TRIPLE; level >= SINGLE; level--) {
		bn = ffs_getib(fs, oip, level);
		if (bn != 0) {
			if (lastiblock[level] < 0 &&
			    oip->i_ump->um_mountp->mnt_wapbl) {
				error = UFS_WAPBL_REGISTER_DEALLOCATION(
				    oip->i_ump->um_mountp,
				    FFS_FSBTODB(fs, bn), fs->fs_bsize,
				    &dcookie);
				if (error)
					goto out;
			} else {
				dcookie = NULL;
			}
			    
			error = ffs_indirtrunc(oip, indir_lbn[level],
			    FFS_FSBTODB(fs, bn), lastiblock[level], level,
			    &blocksreleased);
			if (error) {
				if (dcookie) {
					UFS_WAPBL_UNREGISTER_DEALLOCATION(
					    oip->i_ump->um_mountp, dcookie);
				}
				goto out;
			}

			if (lastiblock[level] < 0) {
				if (!dcookie)
					ffs_blkfree(fs, oip->i_devvp, bn,
					    fs->fs_bsize, oip->i_number);
				DIP_ASSIGN(oip, ib[level], 0);
				blocksreleased += nblocks;
			}
		}
		if (lastiblock[level] >= 0)
			goto done;
	}

	/*
	 * All whole direct blocks or frags.
	 */
	for (i = UFS_NDADDR - 1; i > lastblock; i--) {
		bn = ffs_getdb(fs, oip, i);
		if (bn == 0)
			continue;

		bsize = ffs_blksize(fs, oip, i);
		if ((oip->i_ump->um_mountp->mnt_wapbl) &&
		    (ovp->v_type != VREG)) {
			error = UFS_WAPBL_REGISTER_DEALLOCATION(
			    oip->i_ump->um_mountp,
			    FFS_FSBTODB(fs, bn), bsize, NULL);
			if (error)
				goto out;
		} else
			ffs_blkfree(fs, oip->i_devvp, bn, bsize, oip->i_number);
		DIP_ASSIGN(oip, db[i], 0);
		blocksreleased += btodb(bsize);
	}
	if (lastblock < 0)
		goto done;

	/*
	 * Finally, look for a change in size of the
	 * last direct block; release any frags.
	 */
	bn = ffs_getdb(fs, oip, lastblock);
	if (bn != 0) {
		long oldspace, newspace;

		/*
		 * Calculate amount of space we're giving
		 * back as old block size minus new block size.
		 */
		oldspace = ffs_blksize(fs, oip, lastblock);
		oip->i_size = length;
		DIP_ASSIGN(oip, size, length);
		newspace = ffs_blksize(fs, oip, lastblock);
		if (newspace == 0)
			panic("itrunc: newspace");
		if (oldspace - newspace > 0) {
			/*
			 * Block number of space to be free'd is
			 * the old block # plus the number of frags
			 * required for the storage we're keeping.
			 */
			bn += ffs_numfrags(fs, newspace);
			if ((oip->i_ump->um_mountp->mnt_wapbl) &&
			    (ovp->v_type != VREG)) {
				error = UFS_WAPBL_REGISTER_DEALLOCATION(
				    oip->i_ump->um_mountp, FFS_FSBTODB(fs, bn),
				    oldspace - newspace, NULL);
				if (error)
					goto out;
			} else
				ffs_blkfree(fs, oip->i_devvp, bn,
				    oldspace - newspace, oip->i_number);
			blocksreleased += btodb(oldspace - newspace);
		}
	}

done:
	for (level = SINGLE; level <= TRIPLE; level++)
		KASSERTMSG((blks[UFS_NDADDR + level] == DIP(oip, ib[level])),
		    "itrunc1 blk mismatch: %jx != %jx",
		    (uintmax_t)blks[UFS_NDADDR + level],
		    (uintmax_t)DIP(oip, ib[level]));
	for (i = 0; i < UFS_NDADDR; i++)
		KASSERTMSG((blks[i] == DIP(oip, db[i])),
		    "itrunc2 blk mismatch: %jx != %jx",
		    (uintmax_t)blks[i], (uintmax_t)DIP(oip, db[i]));
	KASSERTMSG((length != 0 || extblocks || LIST_EMPTY(&ovp->v_cleanblkhd)),
	    "itrunc3: zero length and nonempty cleanblkhd");
	KASSERTMSG((length != 0 || extblocks || LIST_EMPTY(&ovp->v_dirtyblkhd)),
	    "itrunc3: zero length and nonempty dirtyblkhd");

out:
	/*
	 * Set length back to old size if deallocation failed. Some indirect
	 * blocks were deallocated creating a hole, but that is okay.
	 */
	if (error == EAGAIN) {
		if (!allerror)
			allerror = error;
		length = osize;
		uvm_vnp_setsize(ovp, length);
	}

	/*
	 * Put back the real size.
	 */
	oip->i_size = length;
	DIP_ASSIGN(oip, size, length);
	DIP_ADD(oip, blocks, -blocksreleased);
	genfs_node_unlock(ovp);
	oip->i_flag |= IN_CHANGE;
	UFS_WAPBL_UPDATE(ovp, NULL, NULL, 0);
#if defined(QUOTA) || defined(QUOTA2)
	(void) chkdq(oip, -blocksreleased, NOCRED, 0);
#endif
	KASSERT(ovp->v_type != VREG || ovp->v_size == oip->i_size);
	return (allerror);
}

/*
 * Release blocks associated with the inode ip and stored in the indirect
 * block bn.  Blocks are free'd in LIFO order up to (but not including)
 * lastbn.  If level is greater than SINGLE, the block is an indirect block
 * and recursive calls to indirtrunc must be used to cleanse other indirect
 * blocks.
 *
 * NB: triple indirect blocks are untested.
 */
static int
ffs_indirtrunc(struct inode *ip, daddr_t lbn, daddr_t dbn, daddr_t lastbn,
    int level, int64_t *countp)
{
	int i;
	struct buf *bp;
	struct fs *fs = ip->i_fs;
	int32_t *bap1 = NULL;
	int64_t *bap2 = NULL;
	struct vnode *vp;
	daddr_t nb, nlbn, last;
	char *copy = NULL;
	int64_t factor;
	int64_t nblocks;
	int error = 0, allerror = 0;
	const int needswap = UFS_FSNEEDSWAP(fs);
	const int wapbl = (ip->i_ump->um_mountp->mnt_wapbl != NULL);
	void *dcookie;

#define RBAP(ip, i) (((ip)->i_ump->um_fstype == UFS1) ? \
	    ufs_rw32(bap1[i], needswap) : ufs_rw64(bap2[i], needswap))
#define BAP_ASSIGN(ip, i, value)					\
	do {								\
		if ((ip)->i_ump->um_fstype == UFS1)			\
			bap1[i] = (value);				\
		else							\
			bap2[i] = (value);				\
	} while(0)

	/*
	 * Calculate index in current block of last
	 * block to be kept.  -1 indicates the entire
	 * block so we need not calculate the index.
	 */
	factor = 1;
	for (i = SINGLE; i < level; i++)
		factor *= FFS_NINDIR(fs);
	last = lastbn;
	if (lastbn > 0)
		last /= factor;
	nblocks = btodb(fs->fs_bsize);
	/*
	 * Get buffer of block pointers, zero those entries corresponding
	 * to blocks to be free'd, and update on disk copy first.  Since
	 * double(triple) indirect before single(double) indirect, calls
	 * to bmap on these blocks will fail.  However, we already have
	 * the on disk address, so we have to set the b_blkno field
	 * explicitly instead of letting bread do everything for us.
	 */
	vp = ITOV(ip);
	error = ffs_getblk(vp, lbn, FFS_NOBLK, fs->fs_bsize, false, &bp);
	if (error)
		return error;

	if (bp->b_oflags & (BO_DONE | BO_DELWRI)) {
		/* Braces must be here in case trace evaluates to nothing. */
		trace(TR_BREADHIT, pack(vp, fs->fs_bsize), lbn);
	} else {
		trace(TR_BREADMISS, pack(vp, fs->fs_bsize), lbn);
		curlwp->l_ru.ru_inblock++;	/* pay for read */
		bp->b_flags |= B_READ;
		bp->b_flags &= ~B_COWDONE;	/* we change blkno below */
		if (bp->b_bcount > bp->b_bufsize)
			panic("ffs_indirtrunc: bad buffer size");
		bp->b_blkno = dbn;
		BIO_SETPRIO(bp, BPRIO_TIMECRITICAL);
		VOP_STRATEGY(vp, bp);
		error = biowait(bp);
		if (error == 0)
			error = fscow_run(bp, true);
	}
	if (error) {
		brelse(bp, 0);
		return error;
	}

	/*
	 * Clear reference to blocks to be removed on disk, before actually
	 * reclaiming them, so that fsck is more likely to be able to recover
	 * the filesystem if system goes down during the truncate process.
	 * This assumes the truncate process would not fail, contrary
	 * to the wapbl case.
	 */
	if (ip->i_ump->um_fstype == UFS1)
		bap1 = (int32_t *)bp->b_data;
	else
		bap2 = (int64_t *)bp->b_data;
	if (lastbn >= 0 && !wapbl) {
		copy = kmem_alloc(fs->fs_bsize, KM_SLEEP);
		memcpy((void *)copy, bp->b_data, (u_int)fs->fs_bsize);
		for (i = last + 1; i < FFS_NINDIR(fs); i++)
			BAP_ASSIGN(ip, i, 0);
		error = bwrite(bp);
		if (error)
			allerror = error;

		if (ip->i_ump->um_fstype == UFS1)
			bap1 = (int32_t *)copy;
		else
			bap2 = (int64_t *)copy;
	}

	/*
	 * Recursively free totally unused blocks.
	 */
	for (i = FFS_NINDIR(fs) - 1, nlbn = lbn + 1 - i * factor; i > last;
	    i--, nlbn += factor) {
		nb = RBAP(ip, i);
		if (nb == 0)
			continue;

		if ((ip->i_ump->um_mountp->mnt_wapbl) &&
		    ((level > SINGLE) || (ITOV(ip)->v_type != VREG))) {
			error = UFS_WAPBL_REGISTER_DEALLOCATION(
			    ip->i_ump->um_mountp,
			    FFS_FSBTODB(fs, nb), fs->fs_bsize,
			    &dcookie);
			if (error)
				goto out;
		} else {
			dcookie = NULL;
		}

		if (level > SINGLE) {
			error = ffs_indirtrunc(ip, nlbn, FFS_FSBTODB(fs, nb),
					       (daddr_t)-1, level - 1, countp);
			if (error) {
				if (dcookie) {
					UFS_WAPBL_UNREGISTER_DEALLOCATION(
					    ip->i_ump->um_mountp, dcookie);
				}

				goto out;
			}
		}

		if (!dcookie)
			ffs_blkfree(fs, ip->i_devvp, nb, fs->fs_bsize,
			    ip->i_number);

		BAP_ASSIGN(ip, i, 0);
		*countp += nblocks;
	}

	/*
	 * Recursively free blocks on the now last partial indirect block.
	 */
	if (level > SINGLE && lastbn >= 0) {
		last = lastbn % factor;
		nb = RBAP(ip, i);
		if (nb != 0) {
			error = ffs_indirtrunc(ip, nlbn, FFS_FSBTODB(fs, nb),
					       last, level - 1, countp);
			if (error)
				goto out;
		}
	}

out:
 	if (error && !allerror)
 		allerror = error;

 	if (copy != NULL) {
 		kmem_free(copy, fs->fs_bsize);
 	} else if (lastbn < 0 && error == 0) {
		/* all freed, release without writing back */
		brelse(bp, BC_INVAL);
	} else if (wapbl) {
 		/* only partially freed, write the updated block */
 		error = bwrite(bp);
 		if (!allerror)
 			allerror = error;
	}

	return (allerror);
}

void
ffs_itimes(struct inode *ip, const struct timespec *acc,
    const struct timespec *mod, const struct timespec *cre)
{
	struct timespec now;

	if (!(ip->i_flag & (IN_ACCESS | IN_CHANGE | IN_UPDATE | IN_MODIFY))) {
		return;
	}

	vfs_timestamp(&now);
	if (ip->i_flag & IN_ACCESS) {
		if (acc == NULL)
			acc = &now;
		DIP_ASSIGN(ip, atime, acc->tv_sec);
		DIP_ASSIGN(ip, atimensec, acc->tv_nsec);
	}
	if (ip->i_flag & (IN_UPDATE | IN_MODIFY)) {
		if ((ip->i_flags & SF_SNAPSHOT) == 0) {
			if (mod == NULL)
				mod = &now;
			DIP_ASSIGN(ip, mtime, mod->tv_sec);
			DIP_ASSIGN(ip, mtimensec, mod->tv_nsec);
		}
		ip->i_modrev++;
	}
	if (ip->i_flag & (IN_CHANGE | IN_MODIFY)) {
		if (cre == NULL)
			cre = &now;
		DIP_ASSIGN(ip, ctime, cre->tv_sec);
		DIP_ASSIGN(ip, ctimensec, cre->tv_nsec);
	}
	if (ip->i_flag & (IN_ACCESS | IN_MODIFY))
		ip->i_flag |= IN_ACCESSED;
	if (ip->i_flag & (IN_UPDATE | IN_CHANGE))
		ip->i_flag |= IN_MODIFIED;
	ip->i_flag &= ~(IN_ACCESS | IN_CHANGE | IN_UPDATE | IN_MODIFY);
}
