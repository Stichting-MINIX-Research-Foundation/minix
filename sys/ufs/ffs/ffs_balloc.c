/*	$NetBSD: ffs_balloc.c,v 1.54 2011/04/23 07:36:02 hannken Exp $	*/

/*
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Marshall
 * Kirk McKusick and Network Associates Laboratories, the Security
 * Research Division of Network Associates, Inc. under DARPA/SPAWAR
 * contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA CHATS
 * research program
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
 *	@(#)ffs_balloc.c	8.8 (Berkeley) 6/16/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ffs_balloc.c,v 1.54 2011/04/23 07:36:02 hannken Exp $");

#if defined(_KERNEL_OPT)
#include "opt_quota.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/kauth.h>
#include <sys/fstrans.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufs_extern.h>
#include <ufs/ufs/ufs_bswap.h>

#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>

#include <uvm/uvm.h>

static int ffs_balloc_ufs1(struct vnode *, off_t, int, kauth_cred_t, int,
    struct buf **);
static int ffs_balloc_ufs2(struct vnode *, off_t, int, kauth_cred_t, int,
    struct buf **);

/*
 * Balloc defines the structure of file system storage
 * by allocating the physical blocks on a device given
 * the inode and the logical block number in a file.
 */

int
ffs_balloc(struct vnode *vp, off_t off, int size, kauth_cred_t cred, int flags,
    struct buf **bpp)
{
	int error;

	if (VTOI(vp)->i_fs->fs_magic == FS_UFS2_MAGIC)
		error = ffs_balloc_ufs2(vp, off, size, cred, flags, bpp);
	else
		error = ffs_balloc_ufs1(vp, off, size, cred, flags, bpp);

	if (error == 0 && bpp != NULL && (error = fscow_run(*bpp, false)) != 0)
		brelse(*bpp, 0);

	return error;
}

static int
ffs_balloc_ufs1(struct vnode *vp, off_t off, int size, kauth_cred_t cred,
    int flags, struct buf **bpp)
{
	daddr_t lbn, lastlbn;
	struct buf *bp, *nbp;
	struct inode *ip = VTOI(vp);
	struct fs *fs = ip->i_fs;
	struct ufsmount *ump = ip->i_ump;
	struct indir indirs[NIADDR + 2];
	daddr_t newb, pref, nb;
	int32_t *bap;	/* XXX ondisk32 */
	int deallocated, osize, nsize, num, i, error;
	int32_t *blkp, *allocblk, allociblk[NIADDR + 1];
	int32_t *allocib;
	int unwindidx = -1;
#ifdef FFS_EI
	const int needswap = UFS_FSNEEDSWAP(fs);
#endif
	UVMHIST_FUNC("ffs_balloc"); UVMHIST_CALLED(ubchist);

	lbn = lblkno(fs, off);
	size = blkoff(fs, off) + size;
	if (size > fs->fs_bsize)
		panic("ffs_balloc: blk too big");
	if (bpp != NULL) {
		*bpp = NULL;
	}
	UVMHIST_LOG(ubchist, "vp %p lbn 0x%x size 0x%x", vp, lbn, size,0);

	if (lbn < 0)
		return (EFBIG);

	/*
	 * If the next write will extend the file into a new block,
	 * and the file is currently composed of a fragment
	 * this fragment has to be extended to be a full block.
	 */

	lastlbn = lblkno(fs, ip->i_size);
	if (lastlbn < NDADDR && lastlbn < lbn) {
		nb = lastlbn;
		osize = blksize(fs, ip, nb);
		if (osize < fs->fs_bsize && osize > 0) {
			mutex_enter(&ump->um_lock);
			error = ffs_realloccg(ip, nb,
				    ffs_blkpref_ufs1(ip, lastlbn, nb, flags,
					&ip->i_ffs1_db[0]),
				    osize, (int)fs->fs_bsize, cred, bpp, &newb);
			if (error)
				return (error);
			ip->i_size = lblktosize(fs, nb + 1);
			ip->i_ffs1_size = ip->i_size;
			uvm_vnp_setsize(vp, ip->i_ffs1_size);
			ip->i_ffs1_db[nb] = ufs_rw32((u_int32_t)newb, needswap);
			ip->i_flag |= IN_CHANGE | IN_UPDATE;
			if (bpp && *bpp) {
				if (flags & B_SYNC)
					bwrite(*bpp);
				else
					bawrite(*bpp);
			}
		}
	}

	/*
	 * The first NDADDR blocks are direct blocks
	 */

	if (lbn < NDADDR) {
		nb = ufs_rw32(ip->i_ffs1_db[lbn], needswap);
		if (nb != 0 && ip->i_size >= lblktosize(fs, lbn + 1)) {

			/*
			 * The block is an already-allocated direct block
			 * and the file already extends past this block,
			 * thus this must be a whole block.
			 * Just read the block (if requested).
			 */

			if (bpp != NULL) {
				error = bread(vp, lbn, fs->fs_bsize, NOCRED,
					      B_MODIFY, bpp);
				if (error) {
					brelse(*bpp, 0);
					return (error);
				}
			}
			return (0);
		}
		if (nb != 0) {

			/*
			 * Consider need to reallocate a fragment.
			 */

			osize = fragroundup(fs, blkoff(fs, ip->i_size));
			nsize = fragroundup(fs, size);
			if (nsize <= osize) {

				/*
				 * The existing block is already
				 * at least as big as we want.
				 * Just read the block (if requested).
				 */

				if (bpp != NULL) {
					error = bread(vp, lbn, osize, NOCRED,
						      B_MODIFY, bpp);
					if (error) {
						brelse(*bpp, 0);
						return (error);
					}
				}
				return 0;
			} else {

				/*
				 * The existing block is smaller than we want,
				 * grow it.
				 */
				mutex_enter(&ump->um_lock);
				error = ffs_realloccg(ip, lbn,
				    ffs_blkpref_ufs1(ip, lbn, (int)lbn, flags,
					&ip->i_ffs1_db[0]),
				    osize, nsize, cred, bpp, &newb);
				if (error)
					return (error);
			}
		} else {

			/*
			 * the block was not previously allocated,
			 * allocate a new block or fragment.
			 */

			if (ip->i_size < lblktosize(fs, lbn + 1))
				nsize = fragroundup(fs, size);
			else
				nsize = fs->fs_bsize;
			mutex_enter(&ump->um_lock);
			error = ffs_alloc(ip, lbn,
			    ffs_blkpref_ufs1(ip, lbn, (int)lbn, flags,
				&ip->i_ffs1_db[0]),
			    nsize, flags, cred, &newb);
			if (error)
				return (error);
			if (bpp != NULL) {
				error = ffs_getblk(vp, lbn, fsbtodb(fs, newb),
				    nsize, (flags & B_CLRBUF) != 0, bpp);
				if (error)
					return error;
			}
		}
		ip->i_ffs1_db[lbn] = ufs_rw32((u_int32_t)newb, needswap);
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		return (0);
	}

	/*
	 * Determine the number of levels of indirection.
	 */

	pref = 0;
	if ((error = ufs_getlbns(vp, lbn, indirs, &num)) != 0)
		return (error);

	/*
	 * Fetch the first indirect block allocating if necessary.
	 */

	--num;
	nb = ufs_rw32(ip->i_ffs1_ib[indirs[0].in_off], needswap);
	allocib = NULL;
	allocblk = allociblk;
	if (nb == 0) {
		mutex_enter(&ump->um_lock);
		pref = ffs_blkpref_ufs1(ip, lbn, 0, flags | B_METAONLY, NULL);
		error = ffs_alloc(ip, lbn, pref, (int)fs->fs_bsize,
		    flags | B_METAONLY, cred, &newb);
		if (error)
			goto fail;
		nb = newb;
		*allocblk++ = nb;
		error = ffs_getblk(vp, indirs[1].in_lbn, fsbtodb(fs, nb),
		    fs->fs_bsize, true, &bp);
		if (error)
			goto fail;
		/*
		 * Write synchronously so that indirect blocks
		 * never point at garbage.
		 */
		if ((error = bwrite(bp)) != 0)
			goto fail;
		unwindidx = 0;
		allocib = &ip->i_ffs1_ib[indirs[0].in_off];
		*allocib = ufs_rw32(nb, needswap);
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
	}

	/*
	 * Fetch through the indirect blocks, allocating as necessary.
	 */

	for (i = 1;;) {
		error = bread(vp,
		    indirs[i].in_lbn, (int)fs->fs_bsize, NOCRED, 0, &bp);
		if (error) {
			brelse(bp, 0);
			goto fail;
		}
		bap = (int32_t *)bp->b_data;	/* XXX ondisk32 */
		nb = ufs_rw32(bap[indirs[i].in_off], needswap);
		if (i == num)
			break;
		i++;
		if (nb != 0) {
			brelse(bp, 0);
			continue;
		}
		if (fscow_run(bp, true) != 0) {
			brelse(bp, 0);
			goto fail;
		}
		mutex_enter(&ump->um_lock);
		/* Try to keep snapshot indirect blocks contiguous. */
		if (i == num && (ip->i_flags & SF_SNAPSHOT) != 0)
			pref = ffs_blkpref_ufs1(ip, lbn, indirs[i-1].in_off,
			    flags | B_METAONLY, &bap[0]);
		if (pref == 0)
			pref = ffs_blkpref_ufs1(ip, lbn, 0, flags | B_METAONLY,
			    NULL);
		error = ffs_alloc(ip, lbn, pref, (int)fs->fs_bsize,
		    flags | B_METAONLY, cred, &newb);
		if (error) {
			brelse(bp, 0);
			goto fail;
		}
		nb = newb;
		*allocblk++ = nb;
		error = ffs_getblk(vp, indirs[i].in_lbn, fsbtodb(fs, nb),
		    fs->fs_bsize, true, &nbp);
		if (error) {
			brelse(bp, 0);
			goto fail;
		}
		/*
		 * Write synchronously so that indirect blocks
		 * never point at garbage.
		 */
		if ((error = bwrite(nbp)) != 0) {
			brelse(bp, 0);
			goto fail;
		}
		if (unwindidx < 0)
			unwindidx = i - 1;
		bap[indirs[i - 1].in_off] = ufs_rw32(nb, needswap);

		/*
		 * If required, write synchronously, otherwise use
		 * delayed write.
		 */

		if (flags & B_SYNC) {
			bwrite(bp);
		} else {
			bdwrite(bp);
		}
	}

	if (flags & B_METAONLY) {
		KASSERT(bpp != NULL);
		*bpp = bp;
		return (0);
	}

	/*
	 * Get the data block, allocating if necessary.
	 */

	if (nb == 0) {
		if (fscow_run(bp, true) != 0) {
			brelse(bp, 0);
			goto fail;
		}
		mutex_enter(&ump->um_lock);
		pref = ffs_blkpref_ufs1(ip, lbn, indirs[num].in_off, flags,
		    &bap[0]);
		error = ffs_alloc(ip, lbn, pref, (int)fs->fs_bsize, flags, cred,
		    &newb);
		if (error) {
			brelse(bp, 0);
			goto fail;
		}
		nb = newb;
		*allocblk++ = nb;
		if (bpp != NULL) {
			error = ffs_getblk(vp, lbn, fsbtodb(fs, nb),
			    fs->fs_bsize, (flags & B_CLRBUF) != 0, bpp);
			if (error) {
				brelse(bp, 0);
				goto fail;
			}
		}
		bap[indirs[num].in_off] = ufs_rw32(nb, needswap);
		if (allocib == NULL && unwindidx < 0) {
			unwindidx = i - 1;
		}

		/*
		 * If required, write synchronously, otherwise use
		 * delayed write.
		 */

		if (flags & B_SYNC) {
			bwrite(bp);
		} else {
			bdwrite(bp);
		}
		return (0);
	}
	brelse(bp, 0);
	if (bpp != NULL) {
		if (flags & B_CLRBUF) {
			error = bread(vp, lbn, (int)fs->fs_bsize,
			    NOCRED, B_MODIFY, &nbp);
			if (error) {
				brelse(nbp, 0);
				goto fail;
			}
		} else {
			error = ffs_getblk(vp, lbn, fsbtodb(fs, nb),
			    fs->fs_bsize, true, &nbp);
			if (error)
				goto fail;
		}
		*bpp = nbp;
	}
	return (0);

fail:
	/*
	 * If we have failed part way through block allocation, we
	 * have to deallocate any indirect blocks that we have allocated.
	 */

	if (unwindidx >= 0) {

		/*
		 * First write out any buffers we've created to resolve their
		 * softdeps.  This must be done in reverse order of creation
		 * so that we resolve the dependencies in one pass.
		 * Write the cylinder group buffers for these buffers too.
		 */

		for (i = num; i >= unwindidx; i--) {
			if (i == 0) {
				break;
			}
			if (ffs_getblk(vp, indirs[i].in_lbn, FFS_NOBLK,
			    fs->fs_bsize, false, &bp) != 0)
				continue;
			if (bp->b_oflags & BO_DELWRI) {
				nb = fsbtodb(fs, cgtod(fs, dtog(fs,
				    dbtofsb(fs, bp->b_blkno))));
				bwrite(bp);
				if (ffs_getblk(ip->i_devvp, nb, FFS_NOBLK,
				    fs->fs_cgsize, false, &bp) != 0)
					continue;
				if (bp->b_oflags & BO_DELWRI) {
					bwrite(bp);
				} else {
					brelse(bp, BC_INVAL);
				}
			} else {
				brelse(bp, BC_INVAL);
			}
		}

		/*
		 * Undo the partial allocation.
		 */
		if (unwindidx == 0) {
			*allocib = 0;
			ip->i_flag |= IN_CHANGE | IN_UPDATE;
		} else {
			int r;

			r = bread(vp, indirs[unwindidx].in_lbn,
			    (int)fs->fs_bsize, NOCRED, 0, &bp);
			if (r) {
				panic("Could not unwind indirect block, error %d", r);
				brelse(bp, 0);
			} else {
				bap = (int32_t *)bp->b_data; /* XXX ondisk32 */
				bap[indirs[unwindidx].in_off] = 0;
				bwrite(bp);
			}
		}
		for (i = unwindidx + 1; i <= num; i++) {
			if (ffs_getblk(vp, indirs[i].in_lbn, FFS_NOBLK,
			    fs->fs_bsize, false, &bp) == 0)
				brelse(bp, BC_INVAL);
		}
	}
	for (deallocated = 0, blkp = allociblk; blkp < allocblk; blkp++) {
		ffs_blkfree(fs, ip->i_devvp, *blkp, fs->fs_bsize, ip->i_number);
		deallocated += fs->fs_bsize;
	}
	if (deallocated) {
#if defined(QUOTA) || defined(QUOTA2)
		/*
		 * Restore user's disk quota because allocation failed.
		 */
		(void)chkdq(ip, -btodb(deallocated), cred, FORCE);
#endif
		ip->i_ffs1_blocks -= btodb(deallocated);
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
	}
	return (error);
}

static int
ffs_balloc_ufs2(struct vnode *vp, off_t off, int size, kauth_cred_t cred,
    int flags, struct buf **bpp)
{
	daddr_t lbn, lastlbn;
	struct buf *bp, *nbp;
	struct inode *ip = VTOI(vp);
	struct fs *fs = ip->i_fs;
	struct ufsmount *ump = ip->i_ump;
	struct indir indirs[NIADDR + 2];
	daddr_t newb, pref, nb;
	int64_t *bap;
	int deallocated, osize, nsize, num, i, error;
	daddr_t *blkp, *allocblk, allociblk[NIADDR + 1];
	int64_t *allocib;
	int unwindidx = -1;
#ifdef FFS_EI
	const int needswap = UFS_FSNEEDSWAP(fs);
#endif
	UVMHIST_FUNC("ffs_balloc"); UVMHIST_CALLED(ubchist);

	lbn = lblkno(fs, off);
	size = blkoff(fs, off) + size;
	if (size > fs->fs_bsize)
		panic("ffs_balloc: blk too big");
	if (bpp != NULL) {
		*bpp = NULL;
	}
	UVMHIST_LOG(ubchist, "vp %p lbn 0x%x size 0x%x", vp, lbn, size,0);

	if (lbn < 0)
		return (EFBIG);

#ifdef notyet
	/*
	 * Check for allocating external data.
	 */
	if (flags & IO_EXT) {
		if (lbn >= NXADDR)
			return (EFBIG);
		/*
		 * If the next write will extend the data into a new block,
		 * and the data is currently composed of a fragment
		 * this fragment has to be extended to be a full block.
		 */
		lastlbn = lblkno(fs, dp->di_extsize);
		if (lastlbn < lbn) {
			nb = lastlbn;
			osize = sblksize(fs, dp->di_extsize, nb);
			if (osize < fs->fs_bsize && osize > 0) {
				mutex_enter(&ump->um_lock);
				error = ffs_realloccg(ip, -1 - nb,
				    dp->di_extb[nb],
				    ffs_blkpref_ufs2(ip, lastlbn, (int)nb,
					flags, &dp->di_extb[0]),
				    osize,
				    (int)fs->fs_bsize, cred, &bp);
				if (error)
					return (error);
				dp->di_extsize = smalllblktosize(fs, nb + 1);
				dp->di_extb[nb] = dbtofsb(fs, bp->b_blkno);
				bp->b_xflags |= BX_ALTDATA;
				ip->i_flag |= IN_CHANGE | IN_UPDATE;
				if (flags & IO_SYNC)
					bwrite(bp);
				else
					bawrite(bp);
			}
		}
		/*
		 * All blocks are direct blocks
		 */
		if (flags & BA_METAONLY)
			panic("ffs_balloc_ufs2: BA_METAONLY for ext block");
		nb = dp->di_extb[lbn];
		if (nb != 0 && dp->di_extsize >= smalllblktosize(fs, lbn + 1)) {
			error = bread(vp, -1 - lbn, fs->fs_bsize,
			    NOCRED, 0, &bp);
			if (error) {
				brelse(bp, 0);
				return (error);
			}
			mutex_enter(&bp->b_interlock);
			bp->b_blkno = fsbtodb(fs, nb);
			bp->b_xflags |= BX_ALTDATA;
			mutex_exit(&bp->b_interlock);
			*bpp = bp;
			return (0);
		}
		if (nb != 0) {
			/*
			 * Consider need to reallocate a fragment.
			 */
			osize = fragroundup(fs, blkoff(fs, dp->di_extsize));
			nsize = fragroundup(fs, size);
			if (nsize <= osize) {
				error = bread(vp, -1 - lbn, osize,
				    NOCRED, 0, &bp);
				if (error) {
					brelse(bp, 0);
					return (error);
				}
				mutex_enter(&bp->b_interlock);
				bp->b_blkno = fsbtodb(fs, nb);
				bp->b_xflags |= BX_ALTDATA;
				mutex_exit(&bp->b_interlock);
			} else {
				mutex_enter(&ump->um_lock);
				error = ffs_realloccg(ip, -1 - lbn,
				    dp->di_extb[lbn],
				    ffs_blkpref_ufs2(ip, lbn, (int)lbn, flags,
				        &dp->di_extb[0]),
				    osize, nsize, cred, &bp);
				if (error)
					return (error);
				bp->b_xflags |= BX_ALTDATA;
			}
		} else {
			if (dp->di_extsize < smalllblktosize(fs, lbn + 1))
				nsize = fragroundup(fs, size);
			else
				nsize = fs->fs_bsize;
			mutex_enter(&ump->um_lock);
			error = ffs_alloc(ip, lbn,
			   ffs_blkpref_ufs2(ip, lbn, (int)lbn, flags,
			       &dp->di_extb[0]),
			   nsize, flags, cred, &newb);
			if (error)
				return (error);
			error = ffs_getblk(vp, -1 - lbn, fsbtodb(fs, newb),
			    nsize, (flags & BA_CLRBUF) != 0, &bp);
			if (error)
				return error;
			bp->b_xflags |= BX_ALTDATA;
		}
		dp->di_extb[lbn] = dbtofsb(fs, bp->b_blkno);
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		*bpp = bp;
		return (0);
	}
#endif
	/*
	 * If the next write will extend the file into a new block,
	 * and the file is currently composed of a fragment
	 * this fragment has to be extended to be a full block.
	 */

	lastlbn = lblkno(fs, ip->i_size);
	if (lastlbn < NDADDR && lastlbn < lbn) {
		nb = lastlbn;
		osize = blksize(fs, ip, nb);
		if (osize < fs->fs_bsize && osize > 0) {
			mutex_enter(&ump->um_lock);
			error = ffs_realloccg(ip, nb,
				    ffs_blkpref_ufs2(ip, lastlbn, nb, flags,
					&ip->i_ffs2_db[0]),
				    osize, (int)fs->fs_bsize, cred, bpp, &newb);
			if (error)
				return (error);
			ip->i_size = lblktosize(fs, nb + 1);
			ip->i_ffs2_size = ip->i_size;
			uvm_vnp_setsize(vp, ip->i_size);
			ip->i_ffs2_db[nb] = ufs_rw64(newb, needswap);
			ip->i_flag |= IN_CHANGE | IN_UPDATE;
			if (bpp) {
				if (flags & B_SYNC)
					bwrite(*bpp);
				else
					bawrite(*bpp);
			}
		}
	}

	/*
	 * The first NDADDR blocks are direct blocks
	 */

	if (lbn < NDADDR) {
		nb = ufs_rw64(ip->i_ffs2_db[lbn], needswap);
		if (nb != 0 && ip->i_size >= lblktosize(fs, lbn + 1)) {

			/*
			 * The block is an already-allocated direct block
			 * and the file already extends past this block,
			 * thus this must be a whole block.
			 * Just read the block (if requested).
			 */

			if (bpp != NULL) {
				error = bread(vp, lbn, fs->fs_bsize, NOCRED,
					      B_MODIFY, bpp);
				if (error) {
					brelse(*bpp, 0);
					return (error);
				}
			}
			return (0);
		}
		if (nb != 0) {

			/*
			 * Consider need to reallocate a fragment.
			 */

			osize = fragroundup(fs, blkoff(fs, ip->i_size));
			nsize = fragroundup(fs, size);
			if (nsize <= osize) {

				/*
				 * The existing block is already
				 * at least as big as we want.
				 * Just read the block (if requested).
				 */

				if (bpp != NULL) {
					error = bread(vp, lbn, osize, NOCRED,
						      B_MODIFY, bpp);
					if (error) {
						brelse(*bpp, 0);
						return (error);
					}
				}
				return 0;
			} else {

				/*
				 * The existing block is smaller than we want,
				 * grow it.
				 */
				mutex_enter(&ump->um_lock);
				error = ffs_realloccg(ip, lbn,
				    ffs_blkpref_ufs2(ip, lbn, (int)lbn, flags,
					&ip->i_ffs2_db[0]),
				    osize, nsize, cred, bpp, &newb);
				if (error)
					return (error);
			}
		} else {

			/*
			 * the block was not previously allocated,
			 * allocate a new block or fragment.
			 */

			if (ip->i_size < lblktosize(fs, lbn + 1))
				nsize = fragroundup(fs, size);
			else
				nsize = fs->fs_bsize;
			mutex_enter(&ump->um_lock);
			error = ffs_alloc(ip, lbn,
			    ffs_blkpref_ufs2(ip, lbn, (int)lbn, flags,
				&ip->i_ffs2_db[0]),
			    nsize, flags, cred, &newb);
			if (error)
				return (error);
			if (bpp != NULL) {
				error = ffs_getblk(vp, lbn, fsbtodb(fs, newb),
				    nsize, (flags & B_CLRBUF) != 0, bpp);
				if (error)
					return error;
			}
		}
		ip->i_ffs2_db[lbn] = ufs_rw64(newb, needswap);
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		return (0);
	}

	/*
	 * Determine the number of levels of indirection.
	 */

	pref = 0;
	if ((error = ufs_getlbns(vp, lbn, indirs, &num)) != 0)
		return (error);

	/*
	 * Fetch the first indirect block allocating if necessary.
	 */

	--num;
	nb = ufs_rw64(ip->i_ffs2_ib[indirs[0].in_off], needswap);
	allocib = NULL;
	allocblk = allociblk;
	if (nb == 0) {
		mutex_enter(&ump->um_lock);
		pref = ffs_blkpref_ufs2(ip, lbn, 0, flags | B_METAONLY, NULL);
		error = ffs_alloc(ip, lbn, pref, (int)fs->fs_bsize,
		    flags | B_METAONLY, cred, &newb);
		if (error)
			goto fail;
		nb = newb;
		*allocblk++ = nb;
		error = ffs_getblk(vp, indirs[1].in_lbn, fsbtodb(fs, nb),
		    fs->fs_bsize, true, &bp);
		if (error)
			goto fail;
		/*
		 * Write synchronously so that indirect blocks
		 * never point at garbage.
		 */
		if ((error = bwrite(bp)) != 0)
			goto fail;
		unwindidx = 0;
		allocib = &ip->i_ffs2_ib[indirs[0].in_off];
		*allocib = ufs_rw64(nb, needswap);
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
	}

	/*
	 * Fetch through the indirect blocks, allocating as necessary.
	 */

	for (i = 1;;) {
		error = bread(vp,
		    indirs[i].in_lbn, (int)fs->fs_bsize, NOCRED, 0, &bp);
		if (error) {
			brelse(bp, 0);
			goto fail;
		}
		bap = (int64_t *)bp->b_data;
		nb = ufs_rw64(bap[indirs[i].in_off], needswap);
		if (i == num)
			break;
		i++;
		if (nb != 0) {
			brelse(bp, 0);
			continue;
		}
		if (fscow_run(bp, true) != 0) {
			brelse(bp, 0);
			goto fail;
		}
		mutex_enter(&ump->um_lock);
		/* Try to keep snapshot indirect blocks contiguous. */
		if (i == num && (ip->i_flags & SF_SNAPSHOT) != 0)
			pref = ffs_blkpref_ufs2(ip, lbn, indirs[i-1].in_off,
			    flags | B_METAONLY, &bap[0]);
		if (pref == 0)
			pref = ffs_blkpref_ufs2(ip, lbn, 0, flags | B_METAONLY,
			    NULL);
		error = ffs_alloc(ip, lbn, pref, (int)fs->fs_bsize,
		    flags | B_METAONLY, cred, &newb);
		if (error) {
			brelse(bp, 0);
			goto fail;
		}
		nb = newb;
		*allocblk++ = nb;
		error = ffs_getblk(vp, indirs[i].in_lbn, fsbtodb(fs, nb),
		    fs->fs_bsize, true, &nbp);
		if (error) {
			brelse(bp, 0);
			goto fail;
		}
		/*
		 * Write synchronously so that indirect blocks
		 * never point at garbage.
		 */
		if ((error = bwrite(nbp)) != 0) {
			brelse(bp, 0);
			goto fail;
		}
		if (unwindidx < 0)
			unwindidx = i - 1;
		bap[indirs[i - 1].in_off] = ufs_rw64(nb, needswap);

		/*
		 * If required, write synchronously, otherwise use
		 * delayed write.
		 */

		if (flags & B_SYNC) {
			bwrite(bp);
		} else {
			bdwrite(bp);
		}
	}

	if (flags & B_METAONLY) {
		KASSERT(bpp != NULL);
		*bpp = bp;
		return (0);
	}

	/*
	 * Get the data block, allocating if necessary.
	 */

	if (nb == 0) {
		if (fscow_run(bp, true) != 0) {
			brelse(bp, 0);
			goto fail;
		}
		mutex_enter(&ump->um_lock);
		pref = ffs_blkpref_ufs2(ip, lbn, indirs[num].in_off, flags,
		    &bap[0]);
		error = ffs_alloc(ip, lbn, pref, (int)fs->fs_bsize, flags, cred,
		    &newb);
		if (error) {
			brelse(bp, 0);
			goto fail;
		}
		nb = newb;
		*allocblk++ = nb;
		if (bpp != NULL) {
			error = ffs_getblk(vp, lbn, fsbtodb(fs, nb),
			    fs->fs_bsize, (flags & B_CLRBUF) != 0, bpp);
			if (error) {
				brelse(bp, 0);
				goto fail;
			}
		}
		bap[indirs[num].in_off] = ufs_rw64(nb, needswap);
		if (allocib == NULL && unwindidx < 0) {
			unwindidx = i - 1;
		}

		/*
		 * If required, write synchronously, otherwise use
		 * delayed write.
		 */

		if (flags & B_SYNC) {
			bwrite(bp);
		} else {
			bdwrite(bp);
		}
		return (0);
	}
	brelse(bp, 0);
	if (bpp != NULL) {
		if (flags & B_CLRBUF) {
			error = bread(vp, lbn, (int)fs->fs_bsize,
			    NOCRED, B_MODIFY, &nbp);
			if (error) {
				brelse(nbp, 0);
				goto fail;
			}
		} else {
			error = ffs_getblk(vp, lbn, fsbtodb(fs, nb),
			    fs->fs_bsize, true, &nbp);
			if (error)
				goto fail;
		}
		*bpp = nbp;
	}
	return (0);

fail:
	/*
	 * If we have failed part way through block allocation, we
	 * have to deallocate any indirect blocks that we have allocated.
	 */

	if (unwindidx >= 0) {

		/*
		 * First write out any buffers we've created to resolve their
		 * softdeps.  This must be done in reverse order of creation
		 * so that we resolve the dependencies in one pass.
		 * Write the cylinder group buffers for these buffers too.
		 */

		for (i = num; i >= unwindidx; i--) {
			if (i == 0) {
				break;
			}
			if (ffs_getblk(vp, indirs[i].in_lbn, FFS_NOBLK,
			    fs->fs_bsize, false, &bp) != 0)
				continue;
			if (bp->b_oflags & BO_DELWRI) {
				nb = fsbtodb(fs, cgtod(fs, dtog(fs,
				    dbtofsb(fs, bp->b_blkno))));
				bwrite(bp);
				if (ffs_getblk(ip->i_devvp, nb, FFS_NOBLK,
				    fs->fs_cgsize, false, &bp) != 0)
					continue;
				if (bp->b_oflags & BO_DELWRI) {
					bwrite(bp);
				} else {
					brelse(bp, BC_INVAL);
				}
			} else {
				brelse(bp, BC_INVAL);
			}
		}

		/*
		 * Now that any dependencies that we created have been
		 * resolved, we can undo the partial allocation.
		 */

		if (unwindidx == 0) {
			*allocib = 0;
			ip->i_flag |= IN_CHANGE | IN_UPDATE;
		} else {
			int r;

			r = bread(vp, indirs[unwindidx].in_lbn,
			    (int)fs->fs_bsize, NOCRED, 0, &bp);
			if (r) {
				panic("Could not unwind indirect block, error %d", r);
				brelse(bp, 0);
			} else {
				bap = (int64_t *)bp->b_data;
				bap[indirs[unwindidx].in_off] = 0;
				bwrite(bp);
			}
		}
		for (i = unwindidx + 1; i <= num; i++) {
			if (ffs_getblk(vp, indirs[i].in_lbn, FFS_NOBLK,
			    fs->fs_bsize, false, &bp) == 0)
				brelse(bp, BC_INVAL);
		}
	}
	for (deallocated = 0, blkp = allociblk; blkp < allocblk; blkp++) {
		ffs_blkfree(fs, ip->i_devvp, *blkp, fs->fs_bsize, ip->i_number);
		deallocated += fs->fs_bsize;
	}
	if (deallocated) {
#if defined(QUOTA) || defined(QUOTA2)
		/*
		 * Restore user's disk quota because allocation failed.
		 */
		(void)chkdq(ip, -btodb(deallocated), cred, FORCE);
#endif
		ip->i_ffs2_blocks -= btodb(deallocated);
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
	}

	return (error);
}
