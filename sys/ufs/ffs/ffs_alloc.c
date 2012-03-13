/*	$NetBSD: ffs_alloc.c,v 1.130 2011/11/28 08:05:07 tls Exp $	*/

/*-
 * Copyright (c) 2008, 2009 The NetBSD Foundation, Inc.
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
 *	@(#)ffs_alloc.c	8.19 (Berkeley) 7/13/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ffs_alloc.c,v 1.130 2011/11/28 08:05:07 tls Exp $");

#if defined(_KERNEL_OPT)
#include "opt_ffs.h"
#include "opt_quota.h"
#include "opt_uvm_page_trkown.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/cprng.h>
#include <sys/fstrans.h>
#include <sys/kauth.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/syslog.h>
#include <sys/vnode.h>
#include <sys/wapbl.h>

#include <miscfs/specfs/specdev.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufs_extern.h>
#include <ufs/ufs/ufs_bswap.h>
#include <ufs/ufs/ufs_wapbl.h>

#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>

#ifdef UVM_PAGE_TRKOWN
#include <uvm/uvm.h>
#endif

static daddr_t ffs_alloccg(struct inode *, int, daddr_t, int, int);
static daddr_t ffs_alloccgblk(struct inode *, struct buf *, daddr_t, int);
static ino_t ffs_dirpref(struct inode *);
static daddr_t ffs_fragextend(struct inode *, int, daddr_t, int, int);
static void ffs_fserr(struct fs *, u_int, const char *);
static daddr_t ffs_hashalloc(struct inode *, int, daddr_t, int, int,
    daddr_t (*)(struct inode *, int, daddr_t, int, int));
static daddr_t ffs_nodealloccg(struct inode *, int, daddr_t, int, int);
static int32_t ffs_mapsearch(struct fs *, struct cg *,
				      daddr_t, int);
static void ffs_blkfree_common(struct ufsmount *, struct fs *, dev_t, struct buf *,
    daddr_t, long, bool);
static void ffs_freefile_common(struct ufsmount *, struct fs *, dev_t, struct buf *, ino_t,
    int, bool);

/* if 1, changes in optimalization strategy are logged */
int ffs_log_changeopt = 0;

/* in ffs_tables.c */
extern const int inside[], around[];
extern const u_char * const fragtbl[];

/* Basic consistency check for block allocations */
static int
ffs_check_bad_allocation(const char *func, struct fs *fs, daddr_t bno,
    long size, dev_t dev, ino_t inum)
{
	if ((u_int)size > fs->fs_bsize || fragoff(fs, size) != 0 ||
	    fragnum(fs, bno) + numfrags(fs, size) > fs->fs_frag) {
		printf("dev = 0x%llx, bno = %" PRId64 " bsize = %d, "
		    "size = %ld, fs = %s\n",
		    (long long)dev, bno, fs->fs_bsize, size, fs->fs_fsmnt);
		panic("%s: bad size", func);
	}

	if (bno >= fs->fs_size) {
		printf("bad block %" PRId64 ", ino %llu\n", bno,
		    (unsigned long long)inum);
		ffs_fserr(fs, inum, "bad block");
		return EINVAL;
	}
	return 0;
}

/*
 * Allocate a block in the file system.
 *
 * The size of the requested block is given, which must be some
 * multiple of fs_fsize and <= fs_bsize.
 * A preference may be optionally specified. If a preference is given
 * the following hierarchy is used to allocate a block:
 *   1) allocate the requested block.
 *   2) allocate a rotationally optimal block in the same cylinder.
 *   3) allocate a block in the same cylinder group.
 *   4) quadradically rehash into other cylinder groups, until an
 *      available block is located.
 * If no block preference is given the following hierarchy is used
 * to allocate a block:
 *   1) allocate a block in the cylinder group that contains the
 *      inode for the file.
 *   2) quadradically rehash into other cylinder groups, until an
 *      available block is located.
 *
 * => called with um_lock held
 * => releases um_lock before returning
 */
int
ffs_alloc(struct inode *ip, daddr_t lbn, daddr_t bpref, int size, int flags,
    kauth_cred_t cred, daddr_t *bnp)
{
	struct ufsmount *ump;
	struct fs *fs;
	daddr_t bno;
	int cg;
#if defined(QUOTA) || defined(QUOTA2)
	int error;
#endif

	fs = ip->i_fs;
	ump = ip->i_ump;

	KASSERT(mutex_owned(&ump->um_lock));

#ifdef UVM_PAGE_TRKOWN

	/*
	 * Sanity-check that allocations within the file size
	 * do not allow other threads to read the stale contents
	 * of newly allocated blocks.
	 * Usually pages will exist to cover the new allocation.
	 * There is an optimization in ffs_write() where we skip
	 * creating pages if several conditions are met:
	 *  - the file must not be mapped (in any user address space).
	 *  - the write must cover whole pages and whole blocks.
	 * If those conditions are not met then pages must exist and
	 * be locked by the current thread.
	 */

	if (ITOV(ip)->v_type == VREG &&
	    lblktosize(fs, (voff_t)lbn) < round_page(ITOV(ip)->v_size)) {
		struct vm_page *pg;
		struct vnode *vp = ITOV(ip);
		struct uvm_object *uobj = &vp->v_uobj;
		voff_t off = trunc_page(lblktosize(fs, lbn));
		voff_t endoff = round_page(lblktosize(fs, lbn) + size);

		mutex_enter(uobj->vmobjlock);
		while (off < endoff) {
			pg = uvm_pagelookup(uobj, off);
			KASSERT((pg == NULL && (vp->v_vflag & VV_MAPPED) == 0 &&
				 (size & PAGE_MASK) == 0 && 
				 blkoff(fs, size) == 0) ||
				(pg != NULL && pg->owner == curproc->p_pid &&
				 pg->lowner == curlwp->l_lid));
			off += PAGE_SIZE;
		}
		mutex_exit(uobj->vmobjlock);
	}
#endif

	*bnp = 0;
#ifdef DIAGNOSTIC
	if ((u_int)size > fs->fs_bsize || fragoff(fs, size) != 0) {
		printf("dev = 0x%llx, bsize = %d, size = %d, fs = %s\n",
		    (unsigned long long)ip->i_dev, fs->fs_bsize, size,
		    fs->fs_fsmnt);
		panic("ffs_alloc: bad size");
	}
	if (cred == NOCRED)
		panic("ffs_alloc: missing credential");
#endif /* DIAGNOSTIC */
	if (size == fs->fs_bsize && fs->fs_cstotal.cs_nbfree == 0)
		goto nospace;
	if (freespace(fs, fs->fs_minfree) <= 0 &&
	    kauth_authorize_system(cred, KAUTH_SYSTEM_FS_RESERVEDSPACE, 0, NULL,
	    NULL, NULL) != 0)
		goto nospace;
#if defined(QUOTA) || defined(QUOTA2)
	mutex_exit(&ump->um_lock);
	if ((error = chkdq(ip, btodb(size), cred, 0)) != 0)
		return (error);
	mutex_enter(&ump->um_lock);
#endif

	if (bpref >= fs->fs_size)
		bpref = 0;
	if (bpref == 0)
		cg = ino_to_cg(fs, ip->i_number);
	else
		cg = dtog(fs, bpref);
	bno = ffs_hashalloc(ip, cg, bpref, size, flags, ffs_alloccg);
	if (bno > 0) {
		DIP_ADD(ip, blocks, btodb(size));
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		*bnp = bno;
		return (0);
	}
#if defined(QUOTA) || defined(QUOTA2)
	/*
	 * Restore user's disk quota because allocation failed.
	 */
	(void) chkdq(ip, -btodb(size), cred, FORCE);
#endif
	if (flags & B_CONTIG) {
		/*
		 * XXX ump->um_lock handling is "suspect" at best.
		 * For the case where ffs_hashalloc() fails early
		 * in the B_CONTIG case we reach here with um_lock
		 * already unlocked, so we can't release it again
		 * like in the normal error path.  See kern/39206.
		 *
		 *
		 * Fail silently - it's up to our caller to report
		 * errors.
		 */
		return (ENOSPC);
	}
nospace:
	mutex_exit(&ump->um_lock);
	ffs_fserr(fs, kauth_cred_geteuid(cred), "file system full");
	uprintf("\n%s: write failed, file system is full\n", fs->fs_fsmnt);
	return (ENOSPC);
}

/*
 * Reallocate a fragment to a bigger size
 *
 * The number and size of the old block is given, and a preference
 * and new size is also specified. The allocator attempts to extend
 * the original block. Failing that, the regular block allocator is
 * invoked to get an appropriate block.
 *
 * => called with um_lock held
 * => return with um_lock released
 */
int
ffs_realloccg(struct inode *ip, daddr_t lbprev, daddr_t bpref, int osize,
    int nsize, kauth_cred_t cred, struct buf **bpp, daddr_t *blknop)
{
	struct ufsmount *ump;
	struct fs *fs;
	struct buf *bp;
	int cg, request, error;
	daddr_t bprev, bno;

	fs = ip->i_fs;
	ump = ip->i_ump;

	KASSERT(mutex_owned(&ump->um_lock));

#ifdef UVM_PAGE_TRKOWN

	/*
	 * Sanity-check that allocations within the file size
	 * do not allow other threads to read the stale contents
	 * of newly allocated blocks.
	 * Unlike in ffs_alloc(), here pages must always exist
	 * for such allocations, because only the last block of a file
	 * can be a fragment and ffs_write() will reallocate the
	 * fragment to the new size using ufs_balloc_range(),
	 * which always creates pages to cover blocks it allocates.
	 */

	if (ITOV(ip)->v_type == VREG) {
		struct vm_page *pg;
		struct uvm_object *uobj = &ITOV(ip)->v_uobj;
		voff_t off = trunc_page(lblktosize(fs, lbprev));
		voff_t endoff = round_page(lblktosize(fs, lbprev) + osize);

		mutex_enter(uobj->vmobjlock);
		while (off < endoff) {
			pg = uvm_pagelookup(uobj, off);
			KASSERT(pg->owner == curproc->p_pid &&
				pg->lowner == curlwp->l_lid);
			off += PAGE_SIZE;
		}
		mutex_exit(uobj->vmobjlock);
	}
#endif

#ifdef DIAGNOSTIC
	if ((u_int)osize > fs->fs_bsize || fragoff(fs, osize) != 0 ||
	    (u_int)nsize > fs->fs_bsize || fragoff(fs, nsize) != 0) {
		printf(
		    "dev = 0x%llx, bsize = %d, osize = %d, nsize = %d, fs = %s\n",
		    (unsigned long long)ip->i_dev, fs->fs_bsize, osize, nsize,
		    fs->fs_fsmnt);
		panic("ffs_realloccg: bad size");
	}
	if (cred == NOCRED)
		panic("ffs_realloccg: missing credential");
#endif /* DIAGNOSTIC */
	if (freespace(fs, fs->fs_minfree) <= 0 &&
	    kauth_authorize_system(cred, KAUTH_SYSTEM_FS_RESERVEDSPACE, 0, NULL,
	    NULL, NULL) != 0) {
		mutex_exit(&ump->um_lock);
		goto nospace;
	}
	if (fs->fs_magic == FS_UFS2_MAGIC)
		bprev = ufs_rw64(ip->i_ffs2_db[lbprev], UFS_FSNEEDSWAP(fs));
	else
		bprev = ufs_rw32(ip->i_ffs1_db[lbprev], UFS_FSNEEDSWAP(fs));

	if (bprev == 0) {
		printf("dev = 0x%llx, bsize = %d, bprev = %" PRId64 ", fs = %s\n",
		    (unsigned long long)ip->i_dev, fs->fs_bsize, bprev,
		    fs->fs_fsmnt);
		panic("ffs_realloccg: bad bprev");
	}
	mutex_exit(&ump->um_lock);

	/*
	 * Allocate the extra space in the buffer.
	 */
	if (bpp != NULL &&
	    (error = bread(ITOV(ip), lbprev, osize, NOCRED, 0, &bp)) != 0) {
		brelse(bp, 0);
		return (error);
	}
#if defined(QUOTA) || defined(QUOTA2)
	if ((error = chkdq(ip, btodb(nsize - osize), cred, 0)) != 0) {
		if (bpp != NULL) {
			brelse(bp, 0);
		}
		return (error);
	}
#endif
	/*
	 * Check for extension in the existing location.
	 */
	cg = dtog(fs, bprev);
	mutex_enter(&ump->um_lock);
	if ((bno = ffs_fragextend(ip, cg, bprev, osize, nsize)) != 0) {
		DIP_ADD(ip, blocks, btodb(nsize - osize));
		ip->i_flag |= IN_CHANGE | IN_UPDATE;

		if (bpp != NULL) {
			if (bp->b_blkno != fsbtodb(fs, bno))
				panic("bad blockno");
			allocbuf(bp, nsize, 1);
			memset((char *)bp->b_data + osize, 0, nsize - osize);
			mutex_enter(bp->b_objlock);
			KASSERT(!cv_has_waiters(&bp->b_done));
			bp->b_oflags |= BO_DONE;
			mutex_exit(bp->b_objlock);
			*bpp = bp;
		}
		if (blknop != NULL) {
			*blknop = bno;
		}
		return (0);
	}
	/*
	 * Allocate a new disk location.
	 */
	if (bpref >= fs->fs_size)
		bpref = 0;
	switch ((int)fs->fs_optim) {
	case FS_OPTSPACE:
		/*
		 * Allocate an exact sized fragment. Although this makes
		 * best use of space, we will waste time relocating it if
		 * the file continues to grow. If the fragmentation is
		 * less than half of the minimum free reserve, we choose
		 * to begin optimizing for time.
		 */
		request = nsize;
		if (fs->fs_minfree < 5 ||
		    fs->fs_cstotal.cs_nffree >
		    fs->fs_dsize * fs->fs_minfree / (2 * 100))
			break;

		if (ffs_log_changeopt) {
			log(LOG_NOTICE,
				"%s: optimization changed from SPACE to TIME\n",
				fs->fs_fsmnt);
		}

		fs->fs_optim = FS_OPTTIME;
		break;
	case FS_OPTTIME:
		/*
		 * At this point we have discovered a file that is trying to
		 * grow a small fragment to a larger fragment. To save time,
		 * we allocate a full sized block, then free the unused portion.
		 * If the file continues to grow, the `ffs_fragextend' call
		 * above will be able to grow it in place without further
		 * copying. If aberrant programs cause disk fragmentation to
		 * grow within 2% of the free reserve, we choose to begin
		 * optimizing for space.
		 */
		request = fs->fs_bsize;
		if (fs->fs_cstotal.cs_nffree <
		    fs->fs_dsize * (fs->fs_minfree - 2) / 100)
			break;

		if (ffs_log_changeopt) {
			log(LOG_NOTICE,
				"%s: optimization changed from TIME to SPACE\n",
				fs->fs_fsmnt);
		}

		fs->fs_optim = FS_OPTSPACE;
		break;
	default:
		printf("dev = 0x%llx, optim = %d, fs = %s\n",
		    (unsigned long long)ip->i_dev, fs->fs_optim, fs->fs_fsmnt);
		panic("ffs_realloccg: bad optim");
		/* NOTREACHED */
	}
	bno = ffs_hashalloc(ip, cg, bpref, request, 0, ffs_alloccg);
	if (bno > 0) {
		if ((ip->i_ump->um_mountp->mnt_wapbl) &&
		    (ITOV(ip)->v_type != VREG)) {
			UFS_WAPBL_REGISTER_DEALLOCATION(
			    ip->i_ump->um_mountp, fsbtodb(fs, bprev),
			    osize);
		} else {
			ffs_blkfree(fs, ip->i_devvp, bprev, (long)osize,
			    ip->i_number);
		}
		if (nsize < request) {
			if ((ip->i_ump->um_mountp->mnt_wapbl) &&
			    (ITOV(ip)->v_type != VREG)) {
				UFS_WAPBL_REGISTER_DEALLOCATION(
				    ip->i_ump->um_mountp,
				    fsbtodb(fs, (bno + numfrags(fs, nsize))),
				    request - nsize);
			} else
				ffs_blkfree(fs, ip->i_devvp,
				    bno + numfrags(fs, nsize),
				    (long)(request - nsize), ip->i_number);
		}
		DIP_ADD(ip, blocks, btodb(nsize - osize));
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		if (bpp != NULL) {
			bp->b_blkno = fsbtodb(fs, bno);
			allocbuf(bp, nsize, 1);
			memset((char *)bp->b_data + osize, 0, (u_int)nsize - osize);
			mutex_enter(bp->b_objlock);
			KASSERT(!cv_has_waiters(&bp->b_done));
			bp->b_oflags |= BO_DONE;
			mutex_exit(bp->b_objlock);
			*bpp = bp;
		}
		if (blknop != NULL) {
			*blknop = bno;
		}
		return (0);
	}
	mutex_exit(&ump->um_lock);

#if defined(QUOTA) || defined(QUOTA2)
	/*
	 * Restore user's disk quota because allocation failed.
	 */
	(void) chkdq(ip, -btodb(nsize - osize), cred, FORCE);
#endif
	if (bpp != NULL) {
		brelse(bp, 0);
	}

nospace:
	/*
	 * no space available
	 */
	ffs_fserr(fs, kauth_cred_geteuid(cred), "file system full");
	uprintf("\n%s: write failed, file system is full\n", fs->fs_fsmnt);
	return (ENOSPC);
}

/*
 * Allocate an inode in the file system.
 *
 * If allocating a directory, use ffs_dirpref to select the inode.
 * If allocating in a directory, the following hierarchy is followed:
 *   1) allocate the preferred inode.
 *   2) allocate an inode in the same cylinder group.
 *   3) quadradically rehash into other cylinder groups, until an
 *      available inode is located.
 * If no inode preference is given the following hierarchy is used
 * to allocate an inode:
 *   1) allocate an inode in cylinder group 0.
 *   2) quadradically rehash into other cylinder groups, until an
 *      available inode is located.
 *
 * => um_lock not held upon entry or return
 */
int
ffs_valloc(struct vnode *pvp, int mode, kauth_cred_t cred,
    struct vnode **vpp)
{
	struct ufsmount *ump;
	struct inode *pip;
	struct fs *fs;
	struct inode *ip;
	struct timespec ts;
	ino_t ino, ipref;
	int cg, error;

	UFS_WAPBL_JUNLOCK_ASSERT(pvp->v_mount);

	*vpp = NULL;
	pip = VTOI(pvp);
	fs = pip->i_fs;
	ump = pip->i_ump;

	error = UFS_WAPBL_BEGIN(pvp->v_mount);
	if (error) {
		return error;
	}
	mutex_enter(&ump->um_lock);
	if (fs->fs_cstotal.cs_nifree == 0)
		goto noinodes;

	if ((mode & IFMT) == IFDIR)
		ipref = ffs_dirpref(pip);
	else
		ipref = pip->i_number;
	if (ipref >= fs->fs_ncg * fs->fs_ipg)
		ipref = 0;
	cg = ino_to_cg(fs, ipref);
	/*
	 * Track number of dirs created one after another
	 * in a same cg without intervening by files.
	 */
	if ((mode & IFMT) == IFDIR) {
		if (fs->fs_contigdirs[cg] < 255)
			fs->fs_contigdirs[cg]++;
	} else {
		if (fs->fs_contigdirs[cg] > 0)
			fs->fs_contigdirs[cg]--;
	}
	ino = (ino_t)ffs_hashalloc(pip, cg, ipref, mode, 0, ffs_nodealloccg);
	if (ino == 0)
		goto noinodes;
	UFS_WAPBL_END(pvp->v_mount);
	error = VFS_VGET(pvp->v_mount, ino, vpp);
	if (error) {
		int err;
		err = UFS_WAPBL_BEGIN(pvp->v_mount);
		if (err == 0)
			ffs_vfree(pvp, ino, mode);
		if (err == 0)
			UFS_WAPBL_END(pvp->v_mount);
		return (error);
	}
	KASSERT((*vpp)->v_type == VNON);
	ip = VTOI(*vpp);
	if (ip->i_mode) {
#if 0
		printf("mode = 0%o, inum = %d, fs = %s\n",
		    ip->i_mode, ip->i_number, fs->fs_fsmnt);
#else
		printf("dmode %x mode %x dgen %x gen %x\n",
		    DIP(ip, mode), ip->i_mode,
		    DIP(ip, gen), ip->i_gen);
		printf("size %llx blocks %llx\n",
		    (long long)DIP(ip, size), (long long)DIP(ip, blocks));
		printf("ino %llu ipref %llu\n", (unsigned long long)ino,
		    (unsigned long long)ipref);
#if 0
		error = bread(ump->um_devvp, fsbtodb(fs, ino_to_fsba(fs, ino)),
		    (int)fs->fs_bsize, NOCRED, 0, &bp);
#endif

#endif
		panic("ffs_valloc: dup alloc");
	}
	if (DIP(ip, blocks)) {				/* XXX */
		printf("free inode %s/%llu had %" PRId64 " blocks\n",
		    fs->fs_fsmnt, (unsigned long long)ino, DIP(ip, blocks));
		DIP_ASSIGN(ip, blocks, 0);
	}
	ip->i_flag &= ~IN_SPACECOUNTED;
	ip->i_flags = 0;
	DIP_ASSIGN(ip, flags, 0);
	/*
	 * Set up a new generation number for this inode.
	 */
	ip->i_gen++;
	DIP_ASSIGN(ip, gen, ip->i_gen);
	if (fs->fs_magic == FS_UFS2_MAGIC) {
		vfs_timestamp(&ts);
		ip->i_ffs2_birthtime = ts.tv_sec;
		ip->i_ffs2_birthnsec = ts.tv_nsec;
	}
	return (0);
noinodes:
	mutex_exit(&ump->um_lock);
	UFS_WAPBL_END(pvp->v_mount);
	ffs_fserr(fs, kauth_cred_geteuid(cred), "out of inodes");
	uprintf("\n%s: create/symlink failed, no inodes free\n", fs->fs_fsmnt);
	return (ENOSPC);
}

/*
 * Find a cylinder group in which to place a directory.
 *
 * The policy implemented by this algorithm is to allocate a
 * directory inode in the same cylinder group as its parent
 * directory, but also to reserve space for its files inodes
 * and data. Restrict the number of directories which may be
 * allocated one after another in the same cylinder group
 * without intervening allocation of files.
 *
 * If we allocate a first level directory then force allocation
 * in another cylinder group.
 */
static ino_t
ffs_dirpref(struct inode *pip)
{
	register struct fs *fs;
	int cg, prefcg;
	int64_t dirsize, cgsize, curdsz;
	int avgifree, avgbfree, avgndir;
	int minifree, minbfree, maxndir;
	int mincg, minndir;
	int maxcontigdirs;

	KASSERT(mutex_owned(&pip->i_ump->um_lock));

	fs = pip->i_fs;

	avgifree = fs->fs_cstotal.cs_nifree / fs->fs_ncg;
	avgbfree = fs->fs_cstotal.cs_nbfree / fs->fs_ncg;
	avgndir = fs->fs_cstotal.cs_ndir / fs->fs_ncg;

	/*
	 * Force allocation in another cg if creating a first level dir.
	 */
	if (ITOV(pip)->v_vflag & VV_ROOT) {
		prefcg = random() % fs->fs_ncg;
		mincg = prefcg;
		minndir = fs->fs_ipg;
		for (cg = prefcg; cg < fs->fs_ncg; cg++)
			if (fs->fs_cs(fs, cg).cs_ndir < minndir &&
			    fs->fs_cs(fs, cg).cs_nifree >= avgifree &&
			    fs->fs_cs(fs, cg).cs_nbfree >= avgbfree) {
				mincg = cg;
				minndir = fs->fs_cs(fs, cg).cs_ndir;
			}
		for (cg = 0; cg < prefcg; cg++)
			if (fs->fs_cs(fs, cg).cs_ndir < minndir &&
			    fs->fs_cs(fs, cg).cs_nifree >= avgifree &&
			    fs->fs_cs(fs, cg).cs_nbfree >= avgbfree) {
				mincg = cg;
				minndir = fs->fs_cs(fs, cg).cs_ndir;
			}
		return ((ino_t)(fs->fs_ipg * mincg));
	}

	/*
	 * Count various limits which used for
	 * optimal allocation of a directory inode.
	 */
	maxndir = min(avgndir + fs->fs_ipg / 16, fs->fs_ipg);
	minifree = avgifree - fs->fs_ipg / 4;
	if (minifree < 0)
		minifree = 0;
	minbfree = avgbfree - fragstoblks(fs, fs->fs_fpg) / 4;
	if (minbfree < 0)
		minbfree = 0;
	cgsize = (int64_t)fs->fs_fsize * fs->fs_fpg;
	dirsize = (int64_t)fs->fs_avgfilesize * fs->fs_avgfpdir;
	if (avgndir != 0) {
		curdsz = (cgsize - (int64_t)avgbfree * fs->fs_bsize) / avgndir;
		if (dirsize < curdsz)
			dirsize = curdsz;
	}
	if (cgsize < dirsize * 255)
		maxcontigdirs = cgsize / dirsize;
	else
		maxcontigdirs = 255;
	if (fs->fs_avgfpdir > 0)
		maxcontigdirs = min(maxcontigdirs,
				    fs->fs_ipg / fs->fs_avgfpdir);
	if (maxcontigdirs == 0)
		maxcontigdirs = 1;

	/*
	 * Limit number of dirs in one cg and reserve space for
	 * regular files, but only if we have no deficit in
	 * inodes or space.
	 */
	prefcg = ino_to_cg(fs, pip->i_number);
	for (cg = prefcg; cg < fs->fs_ncg; cg++)
		if (fs->fs_cs(fs, cg).cs_ndir < maxndir &&
		    fs->fs_cs(fs, cg).cs_nifree >= minifree &&
	    	    fs->fs_cs(fs, cg).cs_nbfree >= minbfree) {
			if (fs->fs_contigdirs[cg] < maxcontigdirs)
				return ((ino_t)(fs->fs_ipg * cg));
		}
	for (cg = 0; cg < prefcg; cg++)
		if (fs->fs_cs(fs, cg).cs_ndir < maxndir &&
		    fs->fs_cs(fs, cg).cs_nifree >= minifree &&
	    	    fs->fs_cs(fs, cg).cs_nbfree >= minbfree) {
			if (fs->fs_contigdirs[cg] < maxcontigdirs)
				return ((ino_t)(fs->fs_ipg * cg));
		}
	/*
	 * This is a backstop when we are deficient in space.
	 */
	for (cg = prefcg; cg < fs->fs_ncg; cg++)
		if (fs->fs_cs(fs, cg).cs_nifree >= avgifree)
			return ((ino_t)(fs->fs_ipg * cg));
	for (cg = 0; cg < prefcg; cg++)
		if (fs->fs_cs(fs, cg).cs_nifree >= avgifree)
			break;
	return ((ino_t)(fs->fs_ipg * cg));
}

/*
 * Select the desired position for the next block in a file.  The file is
 * logically divided into sections. The first section is composed of the
 * direct blocks. Each additional section contains fs_maxbpg blocks.
 *
 * If no blocks have been allocated in the first section, the policy is to
 * request a block in the same cylinder group as the inode that describes
 * the file. If no blocks have been allocated in any other section, the
 * policy is to place the section in a cylinder group with a greater than
 * average number of free blocks.  An appropriate cylinder group is found
 * by using a rotor that sweeps the cylinder groups. When a new group of
 * blocks is needed, the sweep begins in the cylinder group following the
 * cylinder group from which the previous allocation was made. The sweep
 * continues until a cylinder group with greater than the average number
 * of free blocks is found. If the allocation is for the first block in an
 * indirect block, the information on the previous allocation is unavailable;
 * here a best guess is made based upon the logical block number being
 * allocated.
 *
 * If a section is already partially allocated, the policy is to
 * contiguously allocate fs_maxcontig blocks.  The end of one of these
 * contiguous blocks and the beginning of the next is laid out
 * contigously if possible.
 *
 * => um_lock held on entry and exit
 */
daddr_t
ffs_blkpref_ufs1(struct inode *ip, daddr_t lbn, int indx, int flags,
    int32_t *bap /* XXX ondisk32 */)
{
	struct fs *fs;
	int cg;
	int avgbfree, startcg;

	KASSERT(mutex_owned(&ip->i_ump->um_lock));

	fs = ip->i_fs;

	/*
	 * If allocating a contiguous file with B_CONTIG, use the hints
	 * in the inode extentions to return the desired block.
	 *
	 * For metadata (indirect blocks) return the address of where
	 * the first indirect block resides - we'll scan for the next
	 * available slot if we need to allocate more than one indirect
	 * block.  For data, return the address of the actual block
	 * relative to the address of the first data block.
	 */
	if (flags & B_CONTIG) {
		KASSERT(ip->i_ffs_first_data_blk != 0);
		KASSERT(ip->i_ffs_first_indir_blk != 0);
		if (flags & B_METAONLY)
			return ip->i_ffs_first_indir_blk;
		else
			return ip->i_ffs_first_data_blk + blkstofrags(fs, lbn);
	}

	if (indx % fs->fs_maxbpg == 0 || bap[indx - 1] == 0) {
		if (lbn < NDADDR + NINDIR(fs)) {
			cg = ino_to_cg(fs, ip->i_number);
			return (cgbase(fs, cg) + fs->fs_frag);
		}
		/*
		 * Find a cylinder with greater than average number of
		 * unused data blocks.
		 */
		if (indx == 0 || bap[indx - 1] == 0)
			startcg =
			    ino_to_cg(fs, ip->i_number) + lbn / fs->fs_maxbpg;
		else
			startcg = dtog(fs,
				ufs_rw32(bap[indx - 1], UFS_FSNEEDSWAP(fs)) + 1);
		startcg %= fs->fs_ncg;
		avgbfree = fs->fs_cstotal.cs_nbfree / fs->fs_ncg;
		for (cg = startcg; cg < fs->fs_ncg; cg++)
			if (fs->fs_cs(fs, cg).cs_nbfree >= avgbfree) {
				return (cgbase(fs, cg) + fs->fs_frag);
			}
		for (cg = 0; cg < startcg; cg++)
			if (fs->fs_cs(fs, cg).cs_nbfree >= avgbfree) {
				return (cgbase(fs, cg) + fs->fs_frag);
			}
		return (0);
	}
	/*
	 * We just always try to lay things out contiguously.
	 */
	return ufs_rw32(bap[indx - 1], UFS_FSNEEDSWAP(fs)) + fs->fs_frag;
}

daddr_t
ffs_blkpref_ufs2(struct inode *ip, daddr_t lbn, int indx, int flags,
    int64_t *bap)
{
	struct fs *fs;
	int cg;
	int avgbfree, startcg;

	KASSERT(mutex_owned(&ip->i_ump->um_lock));

	fs = ip->i_fs;

	/*
	 * If allocating a contiguous file with B_CONTIG, use the hints
	 * in the inode extentions to return the desired block.
	 *
	 * For metadata (indirect blocks) return the address of where
	 * the first indirect block resides - we'll scan for the next
	 * available slot if we need to allocate more than one indirect
	 * block.  For data, return the address of the actual block
	 * relative to the address of the first data block.
	 */
	if (flags & B_CONTIG) {
		KASSERT(ip->i_ffs_first_data_blk != 0);
		KASSERT(ip->i_ffs_first_indir_blk != 0);
		if (flags & B_METAONLY)
			return ip->i_ffs_first_indir_blk;
		else
			return ip->i_ffs_first_data_blk + blkstofrags(fs, lbn);
	}

	if (indx % fs->fs_maxbpg == 0 || bap[indx - 1] == 0) {
		if (lbn < NDADDR + NINDIR(fs)) {
			cg = ino_to_cg(fs, ip->i_number);
			return (cgbase(fs, cg) + fs->fs_frag);
		}
		/*
		 * Find a cylinder with greater than average number of
		 * unused data blocks.
		 */
		if (indx == 0 || bap[indx - 1] == 0)
			startcg =
			    ino_to_cg(fs, ip->i_number) + lbn / fs->fs_maxbpg;
		else
			startcg = dtog(fs,
				ufs_rw64(bap[indx - 1], UFS_FSNEEDSWAP(fs)) + 1);
		startcg %= fs->fs_ncg;
		avgbfree = fs->fs_cstotal.cs_nbfree / fs->fs_ncg;
		for (cg = startcg; cg < fs->fs_ncg; cg++)
			if (fs->fs_cs(fs, cg).cs_nbfree >= avgbfree) {
				return (cgbase(fs, cg) + fs->fs_frag);
			}
		for (cg = 0; cg < startcg; cg++)
			if (fs->fs_cs(fs, cg).cs_nbfree >= avgbfree) {
				return (cgbase(fs, cg) + fs->fs_frag);
			}
		return (0);
	}
	/*
	 * We just always try to lay things out contiguously.
	 */
	return ufs_rw64(bap[indx - 1], UFS_FSNEEDSWAP(fs)) + fs->fs_frag;
}


/*
 * Implement the cylinder overflow algorithm.
 *
 * The policy implemented by this algorithm is:
 *   1) allocate the block in its requested cylinder group.
 *   2) quadradically rehash on the cylinder group number.
 *   3) brute force search for a free block.
 *
 * => called with um_lock held
 * => returns with um_lock released on success, held on failure
 *    (*allocator releases lock on success, retains lock on failure)
 */
/*VARARGS5*/
static daddr_t
ffs_hashalloc(struct inode *ip, int cg, daddr_t pref,
    int size /* size for data blocks, mode for inodes */,
    int flags, daddr_t (*allocator)(struct inode *, int, daddr_t, int, int))
{
	struct fs *fs;
	daddr_t result;
	int i, icg = cg;

	fs = ip->i_fs;
	/*
	 * 1: preferred cylinder group
	 */
	result = (*allocator)(ip, cg, pref, size, flags);
	if (result)
		return (result);

	if (flags & B_CONTIG)
		return (result);
	/*
	 * 2: quadratic rehash
	 */
	for (i = 1; i < fs->fs_ncg; i *= 2) {
		cg += i;
		if (cg >= fs->fs_ncg)
			cg -= fs->fs_ncg;
		result = (*allocator)(ip, cg, 0, size, flags);
		if (result)
			return (result);
	}
	/*
	 * 3: brute force search
	 * Note that we start at i == 2, since 0 was checked initially,
	 * and 1 is always checked in the quadratic rehash.
	 */
	cg = (icg + 2) % fs->fs_ncg;
	for (i = 2; i < fs->fs_ncg; i++) {
		result = (*allocator)(ip, cg, 0, size, flags);
		if (result)
			return (result);
		cg++;
		if (cg == fs->fs_ncg)
			cg = 0;
	}
	return (0);
}

/*
 * Determine whether a fragment can be extended.
 *
 * Check to see if the necessary fragments are available, and
 * if they are, allocate them.
 *
 * => called with um_lock held
 * => returns with um_lock released on success, held on failure
 */
static daddr_t
ffs_fragextend(struct inode *ip, int cg, daddr_t bprev, int osize, int nsize)
{
	struct ufsmount *ump;
	struct fs *fs;
	struct cg *cgp;
	struct buf *bp;
	daddr_t bno;
	int frags, bbase;
	int i, error;
	u_int8_t *blksfree;

	fs = ip->i_fs;
	ump = ip->i_ump;

	KASSERT(mutex_owned(&ump->um_lock));

	if (fs->fs_cs(fs, cg).cs_nffree < numfrags(fs, nsize - osize))
		return (0);
	frags = numfrags(fs, nsize);
	bbase = fragnum(fs, bprev);
	if (bbase > fragnum(fs, (bprev + frags - 1))) {
		/* cannot extend across a block boundary */
		return (0);
	}
	mutex_exit(&ump->um_lock);
	error = bread(ip->i_devvp, fsbtodb(fs, cgtod(fs, cg)),
		(int)fs->fs_cgsize, NOCRED, B_MODIFY, &bp);
	if (error)
		goto fail;
	cgp = (struct cg *)bp->b_data;
	if (!cg_chkmagic(cgp, UFS_FSNEEDSWAP(fs)))
		goto fail;
	cgp->cg_old_time = ufs_rw32(time_second, UFS_FSNEEDSWAP(fs));
	if ((fs->fs_magic != FS_UFS1_MAGIC) ||
	    (fs->fs_old_flags & FS_FLAGS_UPDATED))
		cgp->cg_time = ufs_rw64(time_second, UFS_FSNEEDSWAP(fs));
	bno = dtogd(fs, bprev);
	blksfree = cg_blksfree(cgp, UFS_FSNEEDSWAP(fs));
	for (i = numfrags(fs, osize); i < frags; i++)
		if (isclr(blksfree, bno + i))
			goto fail;
	/*
	 * the current fragment can be extended
	 * deduct the count on fragment being extended into
	 * increase the count on the remaining fragment (if any)
	 * allocate the extended piece
	 */
	for (i = frags; i < fs->fs_frag - bbase; i++)
		if (isclr(blksfree, bno + i))
			break;
	ufs_add32(cgp->cg_frsum[i - numfrags(fs, osize)], -1, UFS_FSNEEDSWAP(fs));
	if (i != frags)
		ufs_add32(cgp->cg_frsum[i - frags], 1, UFS_FSNEEDSWAP(fs));
	mutex_enter(&ump->um_lock);
	for (i = numfrags(fs, osize); i < frags; i++) {
		clrbit(blksfree, bno + i);
		ufs_add32(cgp->cg_cs.cs_nffree, -1, UFS_FSNEEDSWAP(fs));
		fs->fs_cstotal.cs_nffree--;
		fs->fs_cs(fs, cg).cs_nffree--;
	}
	fs->fs_fmod = 1;
	ACTIVECG_CLR(fs, cg);
	mutex_exit(&ump->um_lock);
	bdwrite(bp);
	return (bprev);

 fail:
 	brelse(bp, 0);
 	mutex_enter(&ump->um_lock);
 	return (0);
}

/*
 * Determine whether a block can be allocated.
 *
 * Check to see if a block of the appropriate size is available,
 * and if it is, allocate it.
 */
static daddr_t
ffs_alloccg(struct inode *ip, int cg, daddr_t bpref, int size, int flags)
{
	struct ufsmount *ump;
	struct fs *fs = ip->i_fs;
	struct cg *cgp;
	struct buf *bp;
	int32_t bno;
	daddr_t blkno;
	int error, frags, allocsiz, i;
	u_int8_t *blksfree;
#ifdef FFS_EI
	const int needswap = UFS_FSNEEDSWAP(fs);
#endif

	ump = ip->i_ump;

	KASSERT(mutex_owned(&ump->um_lock));

	if (fs->fs_cs(fs, cg).cs_nbfree == 0 && size == fs->fs_bsize)
		return (0);
	mutex_exit(&ump->um_lock);
	error = bread(ip->i_devvp, fsbtodb(fs, cgtod(fs, cg)),
		(int)fs->fs_cgsize, NOCRED, B_MODIFY, &bp);
	if (error)
		goto fail;
	cgp = (struct cg *)bp->b_data;
	if (!cg_chkmagic(cgp, needswap) ||
	    (cgp->cg_cs.cs_nbfree == 0 && size == fs->fs_bsize))
		goto fail;
	cgp->cg_old_time = ufs_rw32(time_second, needswap);
	if ((fs->fs_magic != FS_UFS1_MAGIC) ||
	    (fs->fs_old_flags & FS_FLAGS_UPDATED))
		cgp->cg_time = ufs_rw64(time_second, needswap);
	if (size == fs->fs_bsize) {
		mutex_enter(&ump->um_lock);
		blkno = ffs_alloccgblk(ip, bp, bpref, flags);
		ACTIVECG_CLR(fs, cg);
		mutex_exit(&ump->um_lock);
		bdwrite(bp);
		return (blkno);
	}
	/*
	 * check to see if any fragments are already available
	 * allocsiz is the size which will be allocated, hacking
	 * it down to a smaller size if necessary
	 */
	blksfree = cg_blksfree(cgp, needswap);
	frags = numfrags(fs, size);
	for (allocsiz = frags; allocsiz < fs->fs_frag; allocsiz++)
		if (cgp->cg_frsum[allocsiz] != 0)
			break;
	if (allocsiz == fs->fs_frag) {
		/*
		 * no fragments were available, so a block will be
		 * allocated, and hacked up
		 */
		if (cgp->cg_cs.cs_nbfree == 0)
			goto fail;
		mutex_enter(&ump->um_lock);
		blkno = ffs_alloccgblk(ip, bp, bpref, flags);
		bno = dtogd(fs, blkno);
		for (i = frags; i < fs->fs_frag; i++)
			setbit(blksfree, bno + i);
		i = fs->fs_frag - frags;
		ufs_add32(cgp->cg_cs.cs_nffree, i, needswap);
		fs->fs_cstotal.cs_nffree += i;
		fs->fs_cs(fs, cg).cs_nffree += i;
		fs->fs_fmod = 1;
		ufs_add32(cgp->cg_frsum[i], 1, needswap);
		ACTIVECG_CLR(fs, cg);
		mutex_exit(&ump->um_lock);
		bdwrite(bp);
		return (blkno);
	}
	bno = ffs_mapsearch(fs, cgp, bpref, allocsiz);
#if 0
	/*
	 * XXX fvdl mapsearch will panic, and never return -1
	 *          also: returning NULL as daddr_t ?
	 */
	if (bno < 0)
		goto fail;
#endif
	for (i = 0; i < frags; i++)
		clrbit(blksfree, bno + i);
	mutex_enter(&ump->um_lock);
	ufs_add32(cgp->cg_cs.cs_nffree, -frags, needswap);
	fs->fs_cstotal.cs_nffree -= frags;
	fs->fs_cs(fs, cg).cs_nffree -= frags;
	fs->fs_fmod = 1;
	ufs_add32(cgp->cg_frsum[allocsiz], -1, needswap);
	if (frags != allocsiz)
		ufs_add32(cgp->cg_frsum[allocsiz - frags], 1, needswap);
	blkno = cgbase(fs, cg) + bno;
	ACTIVECG_CLR(fs, cg);
	mutex_exit(&ump->um_lock);
	bdwrite(bp);
	return blkno;

 fail:
 	brelse(bp, 0);
 	mutex_enter(&ump->um_lock);
 	return (0);
}

/*
 * Allocate a block in a cylinder group.
 *
 * This algorithm implements the following policy:
 *   1) allocate the requested block.
 *   2) allocate a rotationally optimal block in the same cylinder.
 *   3) allocate the next available block on the block rotor for the
 *      specified cylinder group.
 * Note that this routine only allocates fs_bsize blocks; these
 * blocks may be fragmented by the routine that allocates them.
 */
static daddr_t
ffs_alloccgblk(struct inode *ip, struct buf *bp, daddr_t bpref, int flags)
{
	struct ufsmount *ump;
	struct fs *fs = ip->i_fs;
	struct cg *cgp;
	int cg;
	daddr_t blkno;
	int32_t bno;
	u_int8_t *blksfree;
#ifdef FFS_EI
	const int needswap = UFS_FSNEEDSWAP(fs);
#endif

	ump = ip->i_ump;

	KASSERT(mutex_owned(&ump->um_lock));

	cgp = (struct cg *)bp->b_data;
	blksfree = cg_blksfree(cgp, needswap);
	if (bpref == 0 || dtog(fs, bpref) != ufs_rw32(cgp->cg_cgx, needswap)) {
		bpref = ufs_rw32(cgp->cg_rotor, needswap);
	} else {
		bpref = blknum(fs, bpref);
		bno = dtogd(fs, bpref);
		/*
		 * if the requested block is available, use it
		 */
		if (ffs_isblock(fs, blksfree, fragstoblks(fs, bno)))
			goto gotit;
		/*
		 * if the requested data block isn't available and we are
		 * trying to allocate a contiguous file, return an error.
		 */
		if ((flags & (B_CONTIG | B_METAONLY)) == B_CONTIG)
			return (0);
	}

	/*
	 * Take the next available block in this cylinder group.
	 */
	bno = ffs_mapsearch(fs, cgp, bpref, (int)fs->fs_frag);
	if (bno < 0)
		return (0);
	cgp->cg_rotor = ufs_rw32(bno, needswap);
gotit:
	blkno = fragstoblks(fs, bno);
	ffs_clrblock(fs, blksfree, blkno);
	ffs_clusteracct(fs, cgp, blkno, -1);
	ufs_add32(cgp->cg_cs.cs_nbfree, -1, needswap);
	fs->fs_cstotal.cs_nbfree--;
	fs->fs_cs(fs, ufs_rw32(cgp->cg_cgx, needswap)).cs_nbfree--;
	if ((fs->fs_magic == FS_UFS1_MAGIC) &&
	    ((fs->fs_old_flags & FS_FLAGS_UPDATED) == 0)) {
		int cylno;
		cylno = old_cbtocylno(fs, bno);
		KASSERT(cylno >= 0);
		KASSERT(cylno < fs->fs_old_ncyl);
		KASSERT(old_cbtorpos(fs, bno) >= 0);
		KASSERT(fs->fs_old_nrpos == 0 || old_cbtorpos(fs, bno) < fs->fs_old_nrpos);
		ufs_add16(old_cg_blks(fs, cgp, cylno, needswap)[old_cbtorpos(fs, bno)], -1,
		    needswap);
		ufs_add32(old_cg_blktot(cgp, needswap)[cylno], -1, needswap);
	}
	fs->fs_fmod = 1;
	cg = ufs_rw32(cgp->cg_cgx, needswap);
	blkno = cgbase(fs, cg) + bno;
	return (blkno);
}

/*
 * Determine whether an inode can be allocated.
 *
 * Check to see if an inode is available, and if it is,
 * allocate it using the following policy:
 *   1) allocate the requested inode.
 *   2) allocate the next available inode after the requested
 *      inode in the specified cylinder group.
 */
static daddr_t
ffs_nodealloccg(struct inode *ip, int cg, daddr_t ipref, int mode, int flags)
{
	struct ufsmount *ump = ip->i_ump;
	struct fs *fs = ip->i_fs;
	struct cg *cgp;
	struct buf *bp, *ibp;
	u_int8_t *inosused;
	int error, start, len, loc, map, i;
	int32_t initediblk;
	daddr_t nalloc;
	struct ufs2_dinode *dp2;
#ifdef FFS_EI
	const int needswap = UFS_FSNEEDSWAP(fs);
#endif

	KASSERT(mutex_owned(&ump->um_lock));
	UFS_WAPBL_JLOCK_ASSERT(ip->i_ump->um_mountp);

	if (fs->fs_cs(fs, cg).cs_nifree == 0)
		return (0);
	mutex_exit(&ump->um_lock);
	ibp = NULL;
	initediblk = -1;
retry:
	error = bread(ip->i_devvp, fsbtodb(fs, cgtod(fs, cg)),
		(int)fs->fs_cgsize, NOCRED, B_MODIFY, &bp);
	if (error)
		goto fail;
	cgp = (struct cg *)bp->b_data;
	if (!cg_chkmagic(cgp, needswap) || cgp->cg_cs.cs_nifree == 0)
		goto fail;

	if (ibp != NULL &&
	    initediblk != ufs_rw32(cgp->cg_initediblk, needswap)) {
		/* Another thread allocated more inodes so we retry the test. */
		brelse(ibp, 0);
		ibp = NULL;
	}
	/*
	 * Check to see if we need to initialize more inodes.
	 */
	if (fs->fs_magic == FS_UFS2_MAGIC && ibp == NULL) {
		initediblk = ufs_rw32(cgp->cg_initediblk, needswap);
		nalloc = fs->fs_ipg - ufs_rw32(cgp->cg_cs.cs_nifree, needswap);
		if (nalloc + INOPB(fs) > initediblk &&
		    initediblk < ufs_rw32(cgp->cg_niblk, needswap)) {
			/*
			 * We have to release the cg buffer here to prevent
			 * a deadlock when reading the inode block will
			 * run a copy-on-write that might use this cg.
			 */
			brelse(bp, 0);
			bp = NULL;
			error = ffs_getblk(ip->i_devvp, fsbtodb(fs,
			    ino_to_fsba(fs, cg * fs->fs_ipg + initediblk)),
			    FFS_NOBLK, fs->fs_bsize, false, &ibp);
			if (error)
				goto fail;
			goto retry;
		}
	}

	cgp->cg_old_time = ufs_rw32(time_second, needswap);
	if ((fs->fs_magic != FS_UFS1_MAGIC) ||
	    (fs->fs_old_flags & FS_FLAGS_UPDATED))
		cgp->cg_time = ufs_rw64(time_second, needswap);
	inosused = cg_inosused(cgp, needswap);
	if (ipref) {
		ipref %= fs->fs_ipg;
		if (isclr(inosused, ipref))
			goto gotit;
	}
	start = ufs_rw32(cgp->cg_irotor, needswap) / NBBY;
	len = howmany(fs->fs_ipg - ufs_rw32(cgp->cg_irotor, needswap),
		NBBY);
	loc = skpc(0xff, len, &inosused[start]);
	if (loc == 0) {
		len = start + 1;
		start = 0;
		loc = skpc(0xff, len, &inosused[0]);
		if (loc == 0) {
			printf("cg = %d, irotor = %d, fs = %s\n",
			    cg, ufs_rw32(cgp->cg_irotor, needswap),
				fs->fs_fsmnt);
			panic("ffs_nodealloccg: map corrupted");
			/* NOTREACHED */
		}
	}
	i = start + len - loc;
	map = inosused[i] ^ 0xff;
	if (map == 0) {
		printf("fs = %s\n", fs->fs_fsmnt);
		panic("ffs_nodealloccg: block not in map");
	}
	ipref = i * NBBY + ffs(map) - 1;
	cgp->cg_irotor = ufs_rw32(ipref, needswap);
gotit:
	UFS_WAPBL_REGISTER_INODE(ip->i_ump->um_mountp, cg * fs->fs_ipg + ipref,
	    mode);
	/*
	 * Check to see if we need to initialize more inodes.
	 */
	if (ibp != NULL) {
		KASSERT(initediblk == ufs_rw32(cgp->cg_initediblk, needswap));
		memset(ibp->b_data, 0, fs->fs_bsize);
		dp2 = (struct ufs2_dinode *)(ibp->b_data);
		for (i = 0; i < INOPB(fs); i++) {
			/*
			 * Don't bother to swap, it's supposed to be
			 * random, after all.
			 */
			dp2->di_gen = (cprng_fast32() & INT32_MAX) / 2 + 1;
			dp2++;
		}
		initediblk += INOPB(fs);
		cgp->cg_initediblk = ufs_rw32(initediblk, needswap);
	}

	mutex_enter(&ump->um_lock);
	ACTIVECG_CLR(fs, cg);
	setbit(inosused, ipref);
	ufs_add32(cgp->cg_cs.cs_nifree, -1, needswap);
	fs->fs_cstotal.cs_nifree--;
	fs->fs_cs(fs, cg).cs_nifree--;
	fs->fs_fmod = 1;
	if ((mode & IFMT) == IFDIR) {
		ufs_add32(cgp->cg_cs.cs_ndir, 1, needswap);
		fs->fs_cstotal.cs_ndir++;
		fs->fs_cs(fs, cg).cs_ndir++;
	}
	mutex_exit(&ump->um_lock);
	if (ibp != NULL) {
		bwrite(bp);
		bawrite(ibp);
	} else
		bdwrite(bp);
	return (cg * fs->fs_ipg + ipref);
 fail:
	if (bp != NULL)
		brelse(bp, 0);
	if (ibp != NULL)
		brelse(ibp, 0);
	mutex_enter(&ump->um_lock);
	return (0);
}

/*
 * Allocate a block or fragment.
 *
 * The specified block or fragment is removed from the
 * free map, possibly fragmenting a block in the process.
 *
 * This implementation should mirror fs_blkfree
 *
 * => um_lock not held on entry or exit
 */
int
ffs_blkalloc(struct inode *ip, daddr_t bno, long size)
{
	int error;

	error = ffs_check_bad_allocation(__func__, ip->i_fs, bno, size,
	    ip->i_dev, ip->i_uid);
	if (error)
		return error;

	return ffs_blkalloc_ump(ip->i_ump, bno, size);
}

int
ffs_blkalloc_ump(struct ufsmount *ump, daddr_t bno, long size)
{
	struct fs *fs = ump->um_fs;
	struct cg *cgp;
	struct buf *bp;
	int32_t fragno, cgbno;
	int i, error, cg, blk, frags, bbase;
	u_int8_t *blksfree;
	const int needswap = UFS_FSNEEDSWAP(fs);

	KASSERT((u_int)size <= fs->fs_bsize && fragoff(fs, size) == 0 &&
	    fragnum(fs, bno) + numfrags(fs, size) <= fs->fs_frag);
	KASSERT(bno < fs->fs_size);

	cg = dtog(fs, bno);
	error = bread(ump->um_devvp, fsbtodb(fs, cgtod(fs, cg)),
		(int)fs->fs_cgsize, NOCRED, B_MODIFY, &bp);
	if (error) {
		brelse(bp, 0);
		return error;
	}
	cgp = (struct cg *)bp->b_data;
	if (!cg_chkmagic(cgp, needswap)) {
		brelse(bp, 0);
		return EIO;
	}
	cgp->cg_old_time = ufs_rw32(time_second, needswap);
	cgp->cg_time = ufs_rw64(time_second, needswap);
	cgbno = dtogd(fs, bno);
	blksfree = cg_blksfree(cgp, needswap);

	mutex_enter(&ump->um_lock);
	if (size == fs->fs_bsize) {
		fragno = fragstoblks(fs, cgbno);
		if (!ffs_isblock(fs, blksfree, fragno)) {
			mutex_exit(&ump->um_lock);
			brelse(bp, 0);
			return EBUSY;
		}
		ffs_clrblock(fs, blksfree, fragno);
		ffs_clusteracct(fs, cgp, fragno, -1);
		ufs_add32(cgp->cg_cs.cs_nbfree, -1, needswap);
		fs->fs_cstotal.cs_nbfree--;
		fs->fs_cs(fs, cg).cs_nbfree--;
	} else {
		bbase = cgbno - fragnum(fs, cgbno);

		frags = numfrags(fs, size);
		for (i = 0; i < frags; i++) {
			if (isclr(blksfree, cgbno + i)) {
				mutex_exit(&ump->um_lock);
				brelse(bp, 0);
				return EBUSY;
			}
		}
		/*
		 * if a complete block is being split, account for it
		 */
		fragno = fragstoblks(fs, bbase);
		if (ffs_isblock(fs, blksfree, fragno)) {
			ufs_add32(cgp->cg_cs.cs_nffree, fs->fs_frag, needswap);
			fs->fs_cstotal.cs_nffree += fs->fs_frag;
			fs->fs_cs(fs, cg).cs_nffree += fs->fs_frag;
			ffs_clusteracct(fs, cgp, fragno, -1);
			ufs_add32(cgp->cg_cs.cs_nbfree, -1, needswap);
			fs->fs_cstotal.cs_nbfree--;
			fs->fs_cs(fs, cg).cs_nbfree--;
		}
		/*
		 * decrement the counts associated with the old frags
		 */
		blk = blkmap(fs, blksfree, bbase);
		ffs_fragacct(fs, blk, cgp->cg_frsum, -1, needswap);
		/*
		 * allocate the fragment
		 */
		for (i = 0; i < frags; i++) {
			clrbit(blksfree, cgbno + i);
		}
		ufs_add32(cgp->cg_cs.cs_nffree, -i, needswap);
		fs->fs_cstotal.cs_nffree -= i;
		fs->fs_cs(fs, cg).cs_nffree -= i;
		/*
		 * add back in counts associated with the new frags
		 */
		blk = blkmap(fs, blksfree, bbase);
		ffs_fragacct(fs, blk, cgp->cg_frsum, 1, needswap);
	}
	fs->fs_fmod = 1;
	ACTIVECG_CLR(fs, cg);
	mutex_exit(&ump->um_lock);
	bdwrite(bp);
	return 0;
}

/*
 * Free a block or fragment.
 *
 * The specified block or fragment is placed back in the
 * free map. If a fragment is deallocated, a possible
 * block reassembly is checked.
 *
 * => um_lock not held on entry or exit
 */
void
ffs_blkfree(struct fs *fs, struct vnode *devvp, daddr_t bno, long size,
    ino_t inum)
{
	struct cg *cgp;
	struct buf *bp;
	struct ufsmount *ump;
	daddr_t cgblkno;
	int error, cg;
	dev_t dev;
	const bool devvp_is_snapshot = (devvp->v_type != VBLK);
#ifdef FFS_EI
	const int needswap = UFS_FSNEEDSWAP(fs);
#endif

	KASSERT(!devvp_is_snapshot);

	cg = dtog(fs, bno);
	dev = devvp->v_rdev;
	ump = VFSTOUFS(devvp->v_specmountpoint);
	KASSERT(fs == ump->um_fs);
	cgblkno = fsbtodb(fs, cgtod(fs, cg));
	if (ffs_snapblkfree(fs, devvp, bno, size, inum))
		return;

	error = ffs_check_bad_allocation(__func__, fs, bno, size, dev, inum);
	if (error)
		return;

	error = bread(devvp, cgblkno, (int)fs->fs_cgsize,
	    NOCRED, B_MODIFY, &bp);
	if (error) {
		brelse(bp, 0);
		return;
	}
	cgp = (struct cg *)bp->b_data;
	if (!cg_chkmagic(cgp, needswap)) {
		brelse(bp, 0);
		return;
	}

	ffs_blkfree_common(ump, fs, dev, bp, bno, size, devvp_is_snapshot);

	bdwrite(bp);
}

/*
 * Free a block or fragment from a snapshot cg copy.
 *
 * The specified block or fragment is placed back in the
 * free map. If a fragment is deallocated, a possible
 * block reassembly is checked.
 *
 * => um_lock not held on entry or exit
 */
void
ffs_blkfree_snap(struct fs *fs, struct vnode *devvp, daddr_t bno, long size,
    ino_t inum)
{
	struct cg *cgp;
	struct buf *bp;
	struct ufsmount *ump;
	daddr_t cgblkno;
	int error, cg;
	dev_t dev;
	const bool devvp_is_snapshot = (devvp->v_type != VBLK);
#ifdef FFS_EI
	const int needswap = UFS_FSNEEDSWAP(fs);
#endif

	KASSERT(devvp_is_snapshot);

	cg = dtog(fs, bno);
	dev = VTOI(devvp)->i_devvp->v_rdev;
	ump = VFSTOUFS(devvp->v_mount);
	cgblkno = fragstoblks(fs, cgtod(fs, cg));

	error = ffs_check_bad_allocation(__func__, fs, bno, size, dev, inum);
	if (error)
		return;

	error = bread(devvp, cgblkno, (int)fs->fs_cgsize,
	    NOCRED, B_MODIFY, &bp);
	if (error) {
		brelse(bp, 0);
		return;
	}
	cgp = (struct cg *)bp->b_data;
	if (!cg_chkmagic(cgp, needswap)) {
		brelse(bp, 0);
		return;
	}

	ffs_blkfree_common(ump, fs, dev, bp, bno, size, devvp_is_snapshot);

	bdwrite(bp);
}

static void
ffs_blkfree_common(struct ufsmount *ump, struct fs *fs, dev_t dev,
    struct buf *bp, daddr_t bno, long size, bool devvp_is_snapshot)
{
	struct cg *cgp;
	int32_t fragno, cgbno;
	int i, cg, blk, frags, bbase;
	u_int8_t *blksfree;
	const int needswap = UFS_FSNEEDSWAP(fs);

	cg = dtog(fs, bno);
	cgp = (struct cg *)bp->b_data;
	cgp->cg_old_time = ufs_rw32(time_second, needswap);
	if ((fs->fs_magic != FS_UFS1_MAGIC) ||
	    (fs->fs_old_flags & FS_FLAGS_UPDATED))
		cgp->cg_time = ufs_rw64(time_second, needswap);
	cgbno = dtogd(fs, bno);
	blksfree = cg_blksfree(cgp, needswap);
	mutex_enter(&ump->um_lock);
	if (size == fs->fs_bsize) {
		fragno = fragstoblks(fs, cgbno);
		if (!ffs_isfreeblock(fs, blksfree, fragno)) {
			if (devvp_is_snapshot) {
				mutex_exit(&ump->um_lock);
				return;
			}
			printf("dev = 0x%llx, block = %" PRId64 ", fs = %s\n",
			    (unsigned long long)dev, bno, fs->fs_fsmnt);
			panic("blkfree: freeing free block");
		}
		ffs_setblock(fs, blksfree, fragno);
		ffs_clusteracct(fs, cgp, fragno, 1);
		ufs_add32(cgp->cg_cs.cs_nbfree, 1, needswap);
		fs->fs_cstotal.cs_nbfree++;
		fs->fs_cs(fs, cg).cs_nbfree++;
		if ((fs->fs_magic == FS_UFS1_MAGIC) &&
		    ((fs->fs_old_flags & FS_FLAGS_UPDATED) == 0)) {
			i = old_cbtocylno(fs, cgbno);
			KASSERT(i >= 0);
			KASSERT(i < fs->fs_old_ncyl);
			KASSERT(old_cbtorpos(fs, cgbno) >= 0);
			KASSERT(fs->fs_old_nrpos == 0 || old_cbtorpos(fs, cgbno) < fs->fs_old_nrpos);
			ufs_add16(old_cg_blks(fs, cgp, i, needswap)[old_cbtorpos(fs, cgbno)], 1,
			    needswap);
			ufs_add32(old_cg_blktot(cgp, needswap)[i], 1, needswap);
		}
	} else {
		bbase = cgbno - fragnum(fs, cgbno);
		/*
		 * decrement the counts associated with the old frags
		 */
		blk = blkmap(fs, blksfree, bbase);
		ffs_fragacct(fs, blk, cgp->cg_frsum, -1, needswap);
		/*
		 * deallocate the fragment
		 */
		frags = numfrags(fs, size);
		for (i = 0; i < frags; i++) {
			if (isset(blksfree, cgbno + i)) {
				printf("dev = 0x%llx, block = %" PRId64
				       ", fs = %s\n",
				    (unsigned long long)dev, bno + i,
				    fs->fs_fsmnt);
				panic("blkfree: freeing free frag");
			}
			setbit(blksfree, cgbno + i);
		}
		ufs_add32(cgp->cg_cs.cs_nffree, i, needswap);
		fs->fs_cstotal.cs_nffree += i;
		fs->fs_cs(fs, cg).cs_nffree += i;
		/*
		 * add back in counts associated with the new frags
		 */
		blk = blkmap(fs, blksfree, bbase);
		ffs_fragacct(fs, blk, cgp->cg_frsum, 1, needswap);
		/*
		 * if a complete block has been reassembled, account for it
		 */
		fragno = fragstoblks(fs, bbase);
		if (ffs_isblock(fs, blksfree, fragno)) {
			ufs_add32(cgp->cg_cs.cs_nffree, -fs->fs_frag, needswap);
			fs->fs_cstotal.cs_nffree -= fs->fs_frag;
			fs->fs_cs(fs, cg).cs_nffree -= fs->fs_frag;
			ffs_clusteracct(fs, cgp, fragno, 1);
			ufs_add32(cgp->cg_cs.cs_nbfree, 1, needswap);
			fs->fs_cstotal.cs_nbfree++;
			fs->fs_cs(fs, cg).cs_nbfree++;
			if ((fs->fs_magic == FS_UFS1_MAGIC) &&
			    ((fs->fs_old_flags & FS_FLAGS_UPDATED) == 0)) {
				i = old_cbtocylno(fs, bbase);
				KASSERT(i >= 0);
				KASSERT(i < fs->fs_old_ncyl);
				KASSERT(old_cbtorpos(fs, bbase) >= 0);
				KASSERT(fs->fs_old_nrpos == 0 || old_cbtorpos(fs, bbase) < fs->fs_old_nrpos);
				ufs_add16(old_cg_blks(fs, cgp, i, needswap)[old_cbtorpos(fs,
				    bbase)], 1, needswap);
				ufs_add32(old_cg_blktot(cgp, needswap)[i], 1, needswap);
			}
		}
	}
	fs->fs_fmod = 1;
	ACTIVECG_CLR(fs, cg);
	mutex_exit(&ump->um_lock);
}

/*
 * Free an inode.
 */
int
ffs_vfree(struct vnode *vp, ino_t ino, int mode)
{

	return ffs_freefile(vp->v_mount, ino, mode);
}

/*
 * Do the actual free operation.
 * The specified inode is placed back in the free map.
 *
 * => um_lock not held on entry or exit
 */
int
ffs_freefile(struct mount *mp, ino_t ino, int mode)
{
	struct ufsmount *ump = VFSTOUFS(mp);
	struct fs *fs = ump->um_fs;
	struct vnode *devvp;
	struct cg *cgp;
	struct buf *bp;
	int error, cg;
	daddr_t cgbno;
	dev_t dev;
#ifdef FFS_EI
	const int needswap = UFS_FSNEEDSWAP(fs);
#endif

	cg = ino_to_cg(fs, ino);
	devvp = ump->um_devvp;
	dev = devvp->v_rdev;
	cgbno = fsbtodb(fs, cgtod(fs, cg));

	if ((u_int)ino >= fs->fs_ipg * fs->fs_ncg)
		panic("ifree: range: dev = 0x%llx, ino = %llu, fs = %s",
		    (long long)dev, (unsigned long long)ino, fs->fs_fsmnt);
	error = bread(devvp, cgbno, (int)fs->fs_cgsize,
	    NOCRED, B_MODIFY, &bp);
	if (error) {
		brelse(bp, 0);
		return (error);
	}
	cgp = (struct cg *)bp->b_data;
	if (!cg_chkmagic(cgp, needswap)) {
		brelse(bp, 0);
		return (0);
	}

	ffs_freefile_common(ump, fs, dev, bp, ino, mode, false);

	bdwrite(bp);

	return 0;
}

int
ffs_freefile_snap(struct fs *fs, struct vnode *devvp, ino_t ino, int mode)
{
	struct ufsmount *ump;
	struct cg *cgp;
	struct buf *bp;
	int error, cg;
	daddr_t cgbno;
	dev_t dev;
#ifdef FFS_EI
	const int needswap = UFS_FSNEEDSWAP(fs);
#endif

	KASSERT(devvp->v_type != VBLK);

	cg = ino_to_cg(fs, ino);
	dev = VTOI(devvp)->i_devvp->v_rdev;
	ump = VFSTOUFS(devvp->v_mount);
	cgbno = fragstoblks(fs, cgtod(fs, cg));
	if ((u_int)ino >= fs->fs_ipg * fs->fs_ncg)
		panic("ifree: range: dev = 0x%llx, ino = %llu, fs = %s",
		    (unsigned long long)dev, (unsigned long long)ino,
		    fs->fs_fsmnt);
	error = bread(devvp, cgbno, (int)fs->fs_cgsize,
	    NOCRED, B_MODIFY, &bp);
	if (error) {
		brelse(bp, 0);
		return (error);
	}
	cgp = (struct cg *)bp->b_data;
	if (!cg_chkmagic(cgp, needswap)) {
		brelse(bp, 0);
		return (0);
	}
	ffs_freefile_common(ump, fs, dev, bp, ino, mode, true);

	bdwrite(bp);

	return 0;
}

static void
ffs_freefile_common(struct ufsmount *ump, struct fs *fs, dev_t dev,
    struct buf *bp, ino_t ino, int mode, bool devvp_is_snapshot)
{
	int cg;
	struct cg *cgp;
	u_int8_t *inosused;
#ifdef FFS_EI
	const int needswap = UFS_FSNEEDSWAP(fs);
#endif

	cg = ino_to_cg(fs, ino);
	cgp = (struct cg *)bp->b_data;
	cgp->cg_old_time = ufs_rw32(time_second, needswap);
	if ((fs->fs_magic != FS_UFS1_MAGIC) ||
	    (fs->fs_old_flags & FS_FLAGS_UPDATED))
		cgp->cg_time = ufs_rw64(time_second, needswap);
	inosused = cg_inosused(cgp, needswap);
	ino %= fs->fs_ipg;
	if (isclr(inosused, ino)) {
		printf("ifree: dev = 0x%llx, ino = %llu, fs = %s\n",
		    (unsigned long long)dev, (unsigned long long)ino +
		    cg * fs->fs_ipg, fs->fs_fsmnt);
		if (fs->fs_ronly == 0)
			panic("ifree: freeing free inode");
	}
	clrbit(inosused, ino);
	if (!devvp_is_snapshot)
		UFS_WAPBL_UNREGISTER_INODE(ump->um_mountp,
		    ino + cg * fs->fs_ipg, mode);
	if (ino < ufs_rw32(cgp->cg_irotor, needswap))
		cgp->cg_irotor = ufs_rw32(ino, needswap);
	ufs_add32(cgp->cg_cs.cs_nifree, 1, needswap);
	mutex_enter(&ump->um_lock);
	fs->fs_cstotal.cs_nifree++;
	fs->fs_cs(fs, cg).cs_nifree++;
	if ((mode & IFMT) == IFDIR) {
		ufs_add32(cgp->cg_cs.cs_ndir, -1, needswap);
		fs->fs_cstotal.cs_ndir--;
		fs->fs_cs(fs, cg).cs_ndir--;
	}
	fs->fs_fmod = 1;
	ACTIVECG_CLR(fs, cg);
	mutex_exit(&ump->um_lock);
}

/*
 * Check to see if a file is free.
 */
int
ffs_checkfreefile(struct fs *fs, struct vnode *devvp, ino_t ino)
{
	struct cg *cgp;
	struct buf *bp;
	daddr_t cgbno;
	int ret, cg;
	u_int8_t *inosused;
	const bool devvp_is_snapshot = (devvp->v_type != VBLK);

	KASSERT(devvp_is_snapshot);

	cg = ino_to_cg(fs, ino);
	if (devvp_is_snapshot)
		cgbno = fragstoblks(fs, cgtod(fs, cg));
	else
		cgbno = fsbtodb(fs, cgtod(fs, cg));
	if ((u_int)ino >= fs->fs_ipg * fs->fs_ncg)
		return 1;
	if (bread(devvp, cgbno, (int)fs->fs_cgsize, NOCRED, 0, &bp)) {
		brelse(bp, 0);
		return 1;
	}
	cgp = (struct cg *)bp->b_data;
	if (!cg_chkmagic(cgp, UFS_FSNEEDSWAP(fs))) {
		brelse(bp, 0);
		return 1;
	}
	inosused = cg_inosused(cgp, UFS_FSNEEDSWAP(fs));
	ino %= fs->fs_ipg;
	ret = isclr(inosused, ino);
	brelse(bp, 0);
	return ret;
}

/*
 * Find a block of the specified size in the specified cylinder group.
 *
 * It is a panic if a request is made to find a block if none are
 * available.
 */
static int32_t
ffs_mapsearch(struct fs *fs, struct cg *cgp, daddr_t bpref, int allocsiz)
{
	int32_t bno;
	int start, len, loc, i;
	int blk, field, subfield, pos;
	int ostart, olen;
	u_int8_t *blksfree;
#ifdef FFS_EI
	const int needswap = UFS_FSNEEDSWAP(fs);
#endif

	/* KASSERT(mutex_owned(&ump->um_lock)); */

	/*
	 * find the fragment by searching through the free block
	 * map for an appropriate bit pattern
	 */
	if (bpref)
		start = dtogd(fs, bpref) / NBBY;
	else
		start = ufs_rw32(cgp->cg_frotor, needswap) / NBBY;
	blksfree = cg_blksfree(cgp, needswap);
	len = howmany(fs->fs_fpg, NBBY) - start;
	ostart = start;
	olen = len;
	loc = scanc((u_int)len,
		(const u_char *)&blksfree[start],
		(const u_char *)fragtbl[fs->fs_frag],
		(1 << (allocsiz - 1 + (fs->fs_frag & (NBBY - 1)))));
	if (loc == 0) {
		len = start + 1;
		start = 0;
		loc = scanc((u_int)len,
			(const u_char *)&blksfree[0],
			(const u_char *)fragtbl[fs->fs_frag],
			(1 << (allocsiz - 1 + (fs->fs_frag & (NBBY - 1)))));
		if (loc == 0) {
			printf("start = %d, len = %d, fs = %s\n",
			    ostart, olen, fs->fs_fsmnt);
			printf("offset=%d %ld\n",
				ufs_rw32(cgp->cg_freeoff, needswap),
				(long)blksfree - (long)cgp);
			printf("cg %d\n", cgp->cg_cgx);
			panic("ffs_alloccg: map corrupted");
			/* NOTREACHED */
		}
	}
	bno = (start + len - loc) * NBBY;
	cgp->cg_frotor = ufs_rw32(bno, needswap);
	/*
	 * found the byte in the map
	 * sift through the bits to find the selected frag
	 */
	for (i = bno + NBBY; bno < i; bno += fs->fs_frag) {
		blk = blkmap(fs, blksfree, bno);
		blk <<= 1;
		field = around[allocsiz];
		subfield = inside[allocsiz];
		for (pos = 0; pos <= fs->fs_frag - allocsiz; pos++) {
			if ((blk & field) == subfield)
				return (bno + pos);
			field <<= 1;
			subfield <<= 1;
		}
	}
	printf("bno = %d, fs = %s\n", bno, fs->fs_fsmnt);
	panic("ffs_alloccg: block not in map");
	/* return (-1); */
}

/*
 * Fserr prints the name of a file system with an error diagnostic.
 *
 * The form of the error message is:
 *	fs: error message
 */
static void
ffs_fserr(struct fs *fs, u_int uid, const char *cp)
{

	log(LOG_ERR, "uid %d, pid %d, command %s, on %s: %s\n",
	    uid, curproc->p_pid, curproc->p_comm, fs->fs_fsmnt, cp);
}
