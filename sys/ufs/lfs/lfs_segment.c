/*	$NetBSD: lfs_segment.c,v 1.260 2015/10/03 08:28:16 dholland Exp $	*/

/*-
 * Copyright (c) 1999, 2000, 2001, 2002, 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Konrad E. Schroder <perseant@hhhh.org>.
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
 * Copyright (c) 1991, 1993
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
 *	@(#)lfs_segment.c	8.10 (Berkeley) 6/10/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: lfs_segment.c,v 1.260 2015/10/03 08:28:16 dholland Exp $");

#ifdef DEBUG
# define vndebug(vp, str) do {						\
	if (VTOI(vp)->i_flag & IN_CLEANING)				\
		DLOG((DLOG_WVNODE, "not writing ino %d because %s (op %d)\n", \
		     VTOI(vp)->i_number, (str), op));			\
} while(0)
#else
# define vndebug(vp, str)
#endif
#define ivndebug(vp, str) \
	DLOG((DLOG_WVNODE, "ino %d: %s\n", VTOI(vp)->i_number, (str)))

#if defined(_KERNEL_OPT)
#include "opt_ddb.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/kernel.h>
#include <sys/resourcevar.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/kauth.h>
#include <sys/syslog.h>

#include <miscfs/specfs/specdev.h>
#include <miscfs/fifofs/fifo.h>

#include <ufs/lfs/ulfs_inode.h>
#include <ufs/lfs/ulfsmount.h>
#include <ufs/lfs/ulfs_extern.h>

#include <ufs/lfs/lfs.h>
#include <ufs/lfs/lfs_accessors.h>
#include <ufs/lfs/lfs_kernel.h>
#include <ufs/lfs/lfs_extern.h>

#include <uvm/uvm.h>
#include <uvm/uvm_extern.h>

MALLOC_JUSTDEFINE(M_SEGMENT, "LFS segment", "Segment for LFS");

static void lfs_generic_callback(struct buf *, void (*)(struct buf *));
static void lfs_free_aiodone(struct buf *);
static void lfs_super_aiodone(struct buf *);
static void lfs_cluster_aiodone(struct buf *);
static void lfs_cluster_callback(struct buf *);

/*
 * Determine if it's OK to start a partial in this segment, or if we need
 * to go on to a new segment.
 */
#define	LFS_PARTIAL_FITS(fs) \
	(lfs_sb_getfsbpseg(fs) - \
	    (lfs_sb_getoffset(fs) - lfs_sb_getcurseg(fs)) > \
	lfs_sb_getfrag(fs))

/*
 * Figure out whether we should do a checkpoint write or go ahead with
 * an ordinary write.
 */
#define LFS_SHOULD_CHECKPOINT(fs, flags) \
        ((flags & SEGM_CLEAN) == 0 &&					\
	  ((fs->lfs_nactive > LFS_MAX_ACTIVE ||				\
	    (flags & SEGM_CKP) ||					\
	    lfs_sb_getnclean(fs) < LFS_MAX_ACTIVE)))

int	 lfs_match_fake(struct lfs *, struct buf *);
void	 lfs_newseg(struct lfs *);
void	 lfs_supercallback(struct buf *);
void	 lfs_updatemeta(struct segment *);
void	 lfs_writesuper(struct lfs *, daddr_t);
int	 lfs_writevnodes(struct lfs *fs, struct mount *mp,
	    struct segment *sp, int dirops);

static void lfs_shellsort(struct lfs *, struct buf **, union lfs_blocks *,
			  int, int);

int	lfs_allclean_wakeup;		/* Cleaner wakeup address. */
int	lfs_writeindir = 1;		/* whether to flush indir on non-ckp */
int	lfs_clean_vnhead = 0;		/* Allow freeing to head of vn list */
int	lfs_dirvcount = 0;		/* # active dirops */

/* Statistics Counters */
int lfs_dostats = 1;
struct lfs_stats lfs_stats;

/* op values to lfs_writevnodes */
#define	VN_REG		0
#define	VN_DIROP	1
#define	VN_EMPTY	2
#define VN_CLEAN	3

/*
 * XXX KS - Set modification time on the Ifile, so the cleaner can
 * read the fs mod time off of it.  We don't set IN_UPDATE here,
 * since we don't really need this to be flushed to disk (and in any
 * case that wouldn't happen to the Ifile until we checkpoint).
 */
void
lfs_imtime(struct lfs *fs)
{
	struct timespec ts;
	struct inode *ip;

	ASSERT_MAYBE_SEGLOCK(fs);
	vfs_timestamp(&ts);
	ip = VTOI(fs->lfs_ivnode);
	lfs_dino_setmtime(fs, ip->i_din, ts.tv_sec);
	lfs_dino_setmtimensec(fs, ip->i_din, ts.tv_nsec);
}

/*
 * Ifile and meta data blocks are not marked busy, so segment writes MUST be
 * single threaded.  Currently, there are two paths into lfs_segwrite, sync()
 * and getnewbuf().  They both mark the file system busy.  Lfs_vflush()
 * explicitly marks the file system busy.  So lfs_segwrite is safe.  I think.
 */

#define IS_FLUSHING(fs,vp)  ((fs)->lfs_flushvp == (vp))

int
lfs_vflush(struct vnode *vp)
{
	struct inode *ip;
	struct lfs *fs;
	struct segment *sp;
	struct buf *bp, *nbp, *tbp, *tnbp;
	int error;
	int flushed;
	int relock;

	ip = VTOI(vp);
	fs = VFSTOULFS(vp->v_mount)->um_lfs;
	relock = 0;

    top:
	KASSERT(mutex_owned(vp->v_interlock) == false);
	KASSERT(mutex_owned(&lfs_lock) == false);
	KASSERT(mutex_owned(&bufcache_lock) == false);
	ASSERT_NO_SEGLOCK(fs);
	if (ip->i_flag & IN_CLEANING) {
		ivndebug(vp,"vflush/in_cleaning");
		mutex_enter(&lfs_lock);
		LFS_CLR_UINO(ip, IN_CLEANING);
		LFS_SET_UINO(ip, IN_MODIFIED);
		mutex_exit(&lfs_lock);

		/*
		 * Toss any cleaning buffers that have real counterparts
		 * to avoid losing new data.
		 */
		mutex_enter(vp->v_interlock);
		for (bp = LIST_FIRST(&vp->v_dirtyblkhd); bp; bp = nbp) {
			nbp = LIST_NEXT(bp, b_vnbufs);
			if (!LFS_IS_MALLOC_BUF(bp))
				continue;
			/*
			 * Look for pages matching the range covered
			 * by cleaning blocks.  It's okay if more dirty
			 * pages appear, so long as none disappear out
			 * from under us.
			 */
			if (bp->b_lblkno > 0 && vp->v_type == VREG &&
			    vp != fs->lfs_ivnode) {
				struct vm_page *pg;
				voff_t off;

				for (off = lfs_lblktosize(fs, bp->b_lblkno);
				     off < lfs_lblktosize(fs, bp->b_lblkno + 1);
				     off += PAGE_SIZE) {
					pg = uvm_pagelookup(&vp->v_uobj, off);
					if (pg == NULL)
						continue;
					if ((pg->flags & PG_CLEAN) == 0 ||
					    pmap_is_modified(pg)) {
						lfs_sb_addavail(fs,
							lfs_btofsb(fs,
								bp->b_bcount));
						wakeup(&fs->lfs_availsleep);
						mutex_exit(vp->v_interlock);
						lfs_freebuf(fs, bp);
						mutex_enter(vp->v_interlock);
						bp = NULL;
						break;
					}
				}
			}
			for (tbp = LIST_FIRST(&vp->v_dirtyblkhd); tbp;
			    tbp = tnbp)
			{
				tnbp = LIST_NEXT(tbp, b_vnbufs);
				if (tbp->b_vp == bp->b_vp
				   && tbp->b_lblkno == bp->b_lblkno
				   && tbp != bp)
				{
					lfs_sb_addavail(fs, lfs_btofsb(fs,
						bp->b_bcount));
					wakeup(&fs->lfs_availsleep);
					mutex_exit(vp->v_interlock);
					lfs_freebuf(fs, bp);
					mutex_enter(vp->v_interlock);
					bp = NULL;
					break;
				}
			}
		}
	} else {
		mutex_enter(vp->v_interlock);
	}

	/* If the node is being written, wait until that is done */
	while (WRITEINPROG(vp)) {
		ivndebug(vp,"vflush/writeinprog");
		cv_wait(&vp->v_cv, vp->v_interlock);
	}
	error = vdead_check(vp, VDEAD_NOWAIT);
	mutex_exit(vp->v_interlock);

	/* Protect against deadlock in vinvalbuf() */
	lfs_seglock(fs, SEGM_SYNC | ((error != 0) ? SEGM_RECLAIM : 0));
	if (error != 0) {
		fs->lfs_reclino = ip->i_number;
	}

	/* If we're supposed to flush a freed inode, just toss it */
	if (ip->i_lfs_iflags & LFSI_DELETED) {
		DLOG((DLOG_VNODE, "lfs_vflush: ino %d freed, not flushing\n",
		      ip->i_number));
		/* Drain v_numoutput */
		mutex_enter(vp->v_interlock);
		while (vp->v_numoutput > 0) {
			cv_wait(&vp->v_cv, vp->v_interlock);
		}
		KASSERT(vp->v_numoutput == 0);
		mutex_exit(vp->v_interlock);
	
		mutex_enter(&bufcache_lock);
		for (bp = LIST_FIRST(&vp->v_dirtyblkhd); bp; bp = nbp) {
			nbp = LIST_NEXT(bp, b_vnbufs);

			KASSERT((bp->b_flags & B_GATHERED) == 0);
			if (bp->b_oflags & BO_DELWRI) { /* XXX always true? */
				lfs_sb_addavail(fs, lfs_btofsb(fs, bp->b_bcount));
				wakeup(&fs->lfs_availsleep);
			}
			/* Copied from lfs_writeseg */
			if (bp->b_iodone != NULL) {
				mutex_exit(&bufcache_lock);
				biodone(bp);
				mutex_enter(&bufcache_lock);
			} else {
				bremfree(bp);
				LFS_UNLOCK_BUF(bp);
				mutex_enter(vp->v_interlock);
				bp->b_flags &= ~(B_READ | B_GATHERED);
				bp->b_oflags = (bp->b_oflags & ~BO_DELWRI) | BO_DONE;
				bp->b_error = 0;
				reassignbuf(bp, vp);
				mutex_exit(vp->v_interlock);
				brelse(bp, 0);
			}
		}
		mutex_exit(&bufcache_lock);
		LFS_CLR_UINO(ip, IN_CLEANING);
		LFS_CLR_UINO(ip, IN_MODIFIED | IN_ACCESSED);
		ip->i_flag &= ~IN_ALLMOD;
		DLOG((DLOG_VNODE, "lfs_vflush: done not flushing ino %d\n",
		      ip->i_number));
		lfs_segunlock(fs);

		KASSERT(LIST_FIRST(&vp->v_dirtyblkhd) == NULL);

		return 0;
	}

	fs->lfs_flushvp = vp;
	if (LFS_SHOULD_CHECKPOINT(fs, fs->lfs_sp->seg_flags)) {
		error = lfs_segwrite(vp->v_mount, SEGM_CKP | SEGM_SYNC);
		fs->lfs_flushvp = NULL;
		KASSERT(fs->lfs_flushvp_fakevref == 0);
		lfs_segunlock(fs);

		/* Make sure that any pending buffers get written */
		mutex_enter(vp->v_interlock);
		while (vp->v_numoutput > 0) {
			cv_wait(&vp->v_cv, vp->v_interlock);
		}
		KASSERT(LIST_FIRST(&vp->v_dirtyblkhd) == NULL);
		KASSERT(vp->v_numoutput == 0);
		mutex_exit(vp->v_interlock);

		return error;
	}
	sp = fs->lfs_sp;

	flushed = 0;
	if (VPISEMPTY(vp)) {
		lfs_writevnodes(fs, vp->v_mount, sp, VN_EMPTY);
		++flushed;
	} else if ((ip->i_flag & IN_CLEANING) &&
		  (fs->lfs_sp->seg_flags & SEGM_CLEAN)) {
		ivndebug(vp,"vflush/clean");
		lfs_writevnodes(fs, vp->v_mount, sp, VN_CLEAN);
		++flushed;
	} else if (lfs_dostats) {
		if (!VPISEMPTY(vp) || (VTOI(vp)->i_flag & IN_ALLMOD))
			++lfs_stats.vflush_invoked;
		ivndebug(vp,"vflush");
	}

#ifdef DIAGNOSTIC
	if (vp->v_uflag & VU_DIROP) {
		DLOG((DLOG_VNODE, "lfs_vflush: flushing VU_DIROP\n"));
		/* panic("lfs_vflush: VU_DIROP being flushed...this can\'t happen"); */
	}
#endif

	do {
#ifdef DEBUG
		int loopcount = 0;
#endif
		do {
			if (LIST_FIRST(&vp->v_dirtyblkhd) != NULL) {
				relock = lfs_writefile(fs, sp, vp);
				if (relock && vp != fs->lfs_ivnode) {
					/*
					 * Might have to wait for the
					 * cleaner to run; but we're
					 * still not done with this vnode.
					 * XXX we can do better than this.
					 */
					KDASSERT(ip->i_number != LFS_IFILE_INUM);
					lfs_writeinode(fs, sp, ip);
					mutex_enter(&lfs_lock);
					LFS_SET_UINO(ip, IN_MODIFIED);
					mutex_exit(&lfs_lock);
					lfs_writeseg(fs, sp);
					lfs_segunlock(fs);
					lfs_segunlock_relock(fs);
					goto top;
				}
			}
			/*
			 * If we begin a new segment in the middle of writing
			 * the Ifile, it creates an inconsistent checkpoint,
			 * since the Ifile information for the new segment
			 * is not up-to-date.  Take care of this here by
			 * sending the Ifile through again in case there
			 * are newly dirtied blocks.  But wait, there's more!
			 * This second Ifile write could *also* cross a segment
			 * boundary, if the first one was large.  The second
			 * one is guaranteed to be no more than 8 blocks,
			 * though (two segment blocks and supporting indirects)
			 * so the third write *will not* cross the boundary.
			 */
			if (vp == fs->lfs_ivnode) {
				lfs_writefile(fs, sp, vp);
				lfs_writefile(fs, sp, vp);
			}
#ifdef DEBUG
			if (++loopcount > 2)
				log(LOG_NOTICE, "lfs_vflush: looping count=%d\n", loopcount);
#endif
		} while (lfs_writeinode(fs, sp, ip));
	} while (lfs_writeseg(fs, sp) && ip->i_number == LFS_IFILE_INUM);

	if (lfs_dostats) {
		++lfs_stats.nwrites;
		if (sp->seg_flags & SEGM_SYNC)
			++lfs_stats.nsync_writes;
		if (sp->seg_flags & SEGM_CKP)
			++lfs_stats.ncheckpoints;
	}
	/*
	 * If we were called from somewhere that has already held the seglock
	 * (e.g., lfs_markv()), the lfs_segunlock will not wait for
	 * the write to complete because we are still locked.
	 * Since lfs_vflush() must return the vnode with no dirty buffers,
	 * we must explicitly wait, if that is the case.
	 *
	 * We compare the iocount against 1, not 0, because it is
	 * artificially incremented by lfs_seglock().
	 */
	mutex_enter(&lfs_lock);
	if (fs->lfs_seglock > 1) {
		while (fs->lfs_iocount > 1)
			(void)mtsleep(&fs->lfs_iocount, PRIBIO + 1,
				     "lfs_vflush", 0, &lfs_lock);
	}
	mutex_exit(&lfs_lock);

	lfs_segunlock(fs);

	/* Wait for these buffers to be recovered by aiodoned */
	mutex_enter(vp->v_interlock);
	while (vp->v_numoutput > 0) {
		cv_wait(&vp->v_cv, vp->v_interlock);
	}
	KASSERT(LIST_FIRST(&vp->v_dirtyblkhd) == NULL);
	KASSERT(vp->v_numoutput == 0);
	mutex_exit(vp->v_interlock);

	fs->lfs_flushvp = NULL;
	KASSERT(fs->lfs_flushvp_fakevref == 0);

	return (0);
}

struct lfs_writevnodes_ctx {
	int op;
	struct lfs *fs;
};
static bool
lfs_writevnodes_selector(void *cl, struct vnode *vp)
{
	struct lfs_writevnodes_ctx *c = cl;
	struct inode *ip = VTOI(vp);
	int op = c->op;

	if (ip == NULL || vp->v_type == VNON)
		return false;
	if ((op == VN_DIROP && !(vp->v_uflag & VU_DIROP)) ||
	    (op != VN_DIROP && op != VN_CLEAN && (vp->v_uflag & VU_DIROP))) {
		vndebug(vp, "dirop");
		return false;
	}
	if (op == VN_EMPTY && !VPISEMPTY(vp)) {
		vndebug(vp,"empty");
		return false;;
	}
	if (op == VN_CLEAN && ip->i_number != LFS_IFILE_INUM &&
	    vp != c->fs->lfs_flushvp && !(ip->i_flag & IN_CLEANING)) {
		vndebug(vp,"cleaning");
		return false;
	}
	mutex_enter(&lfs_lock);
	if (vp == c->fs->lfs_unlockvp) {
		mutex_exit(&lfs_lock);
		return false;
	}
	mutex_exit(&lfs_lock);

	return true;
}

int
lfs_writevnodes(struct lfs *fs, struct mount *mp, struct segment *sp, int op)
{
	struct inode *ip;
	struct vnode *vp;
	struct vnode_iterator *marker;
	struct lfs_writevnodes_ctx ctx;
	int inodes_written = 0;
	int error = 0;

	/*
	 * XXX This was TAILQ_FOREACH_REVERSE on &mp->mnt_vnodelist.
	 * XXX The rationale is unclear, the initial commit had no information.
	 * XXX If the order really matters we have to sort the vnodes first.
	*/

	ASSERT_SEGLOCK(fs);
	vfs_vnode_iterator_init(mp, &marker);
	ctx.op = op;
	ctx.fs = fs;
	while ((vp = vfs_vnode_iterator_next(marker,
	    lfs_writevnodes_selector, &ctx)) != NULL) {
		ip = VTOI(vp);

		/*
		 * Write the inode/file if dirty and it's not the IFILE.
		 */
		if (((ip->i_flag & IN_ALLMOD) || !VPISEMPTY(vp)) &&
		    ip->i_number != LFS_IFILE_INUM) {
			error = lfs_writefile(fs, sp, vp);
			if (error) {
				vrele(vp);
				if (error == EAGAIN) {
					/*
					 * This error from lfs_putpages
					 * indicates we need to drop
					 * the segment lock and start
					 * over after the cleaner has
					 * had a chance to run.
					 */
					lfs_writeinode(fs, sp, ip);
					lfs_writeseg(fs, sp);
					if (!VPISEMPTY(vp) &&
					    !WRITEINPROG(vp) &&
					    !(ip->i_flag & IN_ALLMOD)) {
						mutex_enter(&lfs_lock);
						LFS_SET_UINO(ip, IN_MODIFIED);
						mutex_exit(&lfs_lock);
					}
					break;
				}
				error = 0; /* XXX not quite right */
				continue;
			}
			
			if (!VPISEMPTY(vp)) {
				if (WRITEINPROG(vp)) {
					ivndebug(vp,"writevnodes/write2");
				} else if (!(ip->i_flag & IN_ALLMOD)) {
					mutex_enter(&lfs_lock);
					LFS_SET_UINO(ip, IN_MODIFIED);
					mutex_exit(&lfs_lock);
				}
			}
			(void) lfs_writeinode(fs, sp, ip);
			inodes_written++;
		}
		vrele(vp);
	}
	vfs_vnode_iterator_destroy(marker);
	return error;
}

/*
 * Do a checkpoint.
 */
int
lfs_segwrite(struct mount *mp, int flags)
{
	struct buf *bp;
	struct inode *ip;
	struct lfs *fs;
	struct segment *sp;
	struct vnode *vp;
	SEGUSE *segusep;
	int do_ckp, did_ckp, error;
	unsigned n, segleft, maxseg, sn, i, curseg;
	int writer_set = 0;
	int dirty;
	int redo;
	SEGSUM *ssp;
	int um_error;

	fs = VFSTOULFS(mp)->um_lfs;
	ASSERT_MAYBE_SEGLOCK(fs);

	if (fs->lfs_ronly)
		return EROFS;

	lfs_imtime(fs);

	/*
	 * Allocate a segment structure and enough space to hold pointers to
	 * the maximum possible number of buffers which can be described in a
	 * single summary block.
	 */
	do_ckp = LFS_SHOULD_CHECKPOINT(fs, flags);

	/* We can't do a partial write and checkpoint at the same time. */
	if (do_ckp)
		flags &= ~SEGM_SINGLE;

	lfs_seglock(fs, flags | (do_ckp ? SEGM_CKP : 0));
	sp = fs->lfs_sp;
	if (sp->seg_flags & (SEGM_CLEAN | SEGM_CKP))
		do_ckp = 1;

	/*
	 * If lfs_flushvp is non-NULL, we are called from lfs_vflush,
	 * in which case we have to flush *all* buffers off of this vnode.
	 * We don't care about other nodes, but write any non-dirop nodes
	 * anyway in anticipation of another getnewvnode().
	 *
	 * If we're cleaning we only write cleaning and ifile blocks, and
	 * no dirops, since otherwise we'd risk corruption in a crash.
	 */
	if (sp->seg_flags & SEGM_CLEAN)
		lfs_writevnodes(fs, mp, sp, VN_CLEAN);
	else if (!(sp->seg_flags & SEGM_FORCE_CKP)) {
		do {
			um_error = lfs_writevnodes(fs, mp, sp, VN_REG);
			if ((sp->seg_flags & SEGM_SINGLE) &&
			    lfs_sb_getcurseg(fs) != fs->lfs_startseg) {
				DLOG((DLOG_SEG, "lfs_segwrite: breaking out of segment write at daddr 0x%jx\n", (uintmax_t)lfs_sb_getoffset(fs)));
				break;
			}

			if (do_ckp || fs->lfs_dirops == 0) {
				if (!writer_set) {
					lfs_writer_enter(fs, "lfs writer");
					writer_set = 1;
				}
				error = lfs_writevnodes(fs, mp, sp, VN_DIROP);
				if (um_error == 0)
					um_error = error;
				/* In case writevnodes errored out */
				lfs_flush_dirops(fs);
				ssp = (SEGSUM *)(sp->segsum);
				lfs_ss_setflags(fs, ssp,
						lfs_ss_getflags(fs, ssp) & ~(SS_CONT));
				lfs_finalize_fs_seguse(fs);
			}
			if (do_ckp && um_error) {
				lfs_segunlock_relock(fs);
				sp = fs->lfs_sp;
			}
		} while (do_ckp && um_error != 0);
	}

	/*
	 * If we are doing a checkpoint, mark everything since the
	 * last checkpoint as no longer ACTIVE.
	 */
	if (do_ckp || fs->lfs_doifile) {
		segleft = lfs_sb_getnseg(fs);
		curseg = 0;
		for (n = 0; n < lfs_sb_getsegtabsz(fs); n++) {
			dirty = 0;
			if (bread(fs->lfs_ivnode, lfs_sb_getcleansz(fs) + n,
			    lfs_sb_getbsize(fs), B_MODIFY, &bp))
				panic("lfs_segwrite: ifile read");
			segusep = (SEGUSE *)bp->b_data;
			maxseg = min(segleft, lfs_sb_getsepb(fs));
			for (i = 0; i < maxseg; i++) {
				sn = curseg + i;
				if (sn != lfs_dtosn(fs, lfs_sb_getcurseg(fs)) &&
				    segusep->su_flags & SEGUSE_ACTIVE) {
					segusep->su_flags &= ~SEGUSE_ACTIVE;
					--fs->lfs_nactive;
					++dirty;
				}
				fs->lfs_suflags[fs->lfs_activesb][sn] =
					segusep->su_flags;
				if (lfs_sb_getversion(fs) > 1)
					++segusep;
				else
					segusep = (SEGUSE *)
						((SEGUSE_V1 *)segusep + 1);
			}

			if (dirty)
				error = LFS_BWRITE_LOG(bp); /* Ifile */
			else
				brelse(bp, 0);
			segleft -= lfs_sb_getsepb(fs);
			curseg += lfs_sb_getsepb(fs);
		}
	}

	KASSERT(LFS_SEGLOCK_HELD(fs));

	did_ckp = 0;
	if (do_ckp || fs->lfs_doifile) {
		vp = fs->lfs_ivnode;
#ifdef DEBUG
		int loopcount = 0;
#endif
		do {
#ifdef DEBUG
			LFS_ENTER_LOG("pretend", __FILE__, __LINE__, 0, 0, curproc->p_pid);
#endif
			mutex_enter(&lfs_lock);
			fs->lfs_flags &= ~LFS_IFDIRTY;
			mutex_exit(&lfs_lock);

			ip = VTOI(vp);

			if (LIST_FIRST(&vp->v_dirtyblkhd) != NULL) {
				/*
				 * Ifile has no pages, so we don't need
				 * to check error return here.
				 */
				lfs_writefile(fs, sp, vp);
				/*
				 * Ensure the Ifile takes the current segment
				 * into account.  See comment in lfs_vflush.
				 */
				lfs_writefile(fs, sp, vp);
				lfs_writefile(fs, sp, vp);
			}

			if (ip->i_flag & IN_ALLMOD)
				++did_ckp;
#if 0
			redo = (do_ckp ? lfs_writeinode(fs, sp, ip) : 0);
#else
			redo = lfs_writeinode(fs, sp, ip);
#endif
			redo += lfs_writeseg(fs, sp);
			mutex_enter(&lfs_lock);
			redo += (fs->lfs_flags & LFS_IFDIRTY);
			mutex_exit(&lfs_lock);
#ifdef DEBUG
			if (++loopcount > 2)
				log(LOG_NOTICE, "lfs_segwrite: looping count=%d\n",
					loopcount);
#endif
		} while (redo && do_ckp);

		/*
		 * Unless we are unmounting, the Ifile may continue to have
		 * dirty blocks even after a checkpoint, due to changes to
		 * inodes' atime.  If we're checkpointing, it's "impossible"
		 * for other parts of the Ifile to be dirty after the loop
		 * above, since we hold the segment lock.
		 */
		mutex_enter(vp->v_interlock);
		if (LIST_EMPTY(&vp->v_dirtyblkhd)) {
			LFS_CLR_UINO(ip, IN_ALLMOD);
		}
#ifdef DIAGNOSTIC
		else if (do_ckp) {
			int do_panic = 0;
			LIST_FOREACH(bp, &vp->v_dirtyblkhd, b_vnbufs) {
				if (bp->b_lblkno < lfs_sb_getcleansz(fs) +
				    lfs_sb_getsegtabsz(fs) &&
				    !(bp->b_flags & B_GATHERED)) {
					printf("ifile lbn %ld still dirty (flags %lx)\n",
						(long)bp->b_lblkno,
						(long)bp->b_flags);
					++do_panic;
				}
			}
			if (do_panic)
				panic("dirty blocks");
		}
#endif
		mutex_exit(vp->v_interlock);
	} else {
		(void) lfs_writeseg(fs, sp);
	}

	/* Note Ifile no longer needs to be written */
	fs->lfs_doifile = 0;
	if (writer_set)
		lfs_writer_leave(fs);

	/*
	 * If we didn't write the Ifile, we didn't really do anything.
	 * That means that (1) there is a checkpoint on disk and (2)
	 * nothing has changed since it was written.
	 *
	 * Take the flags off of the segment so that lfs_segunlock
	 * doesn't have to write the superblock either.
	 */
	if (do_ckp && !did_ckp) {
		sp->seg_flags &= ~SEGM_CKP;
	}

	if (lfs_dostats) {
		++lfs_stats.nwrites;
		if (sp->seg_flags & SEGM_SYNC)
			++lfs_stats.nsync_writes;
		if (sp->seg_flags & SEGM_CKP)
			++lfs_stats.ncheckpoints;
	}
	lfs_segunlock(fs);
	return (0);
}

/*
 * Write the dirty blocks associated with a vnode.
 */
int
lfs_writefile(struct lfs *fs, struct segment *sp, struct vnode *vp)
{
	struct inode *ip;
	int i, frag;
	SEGSUM *ssp;
	int error;

	ASSERT_SEGLOCK(fs);
	error = 0;
	ip = VTOI(vp);

	lfs_acquire_finfo(fs, ip->i_number, ip->i_gen);

	if (vp->v_uflag & VU_DIROP) {
		ssp = (SEGSUM *)sp->segsum;
		lfs_ss_setflags(fs, ssp,
				lfs_ss_getflags(fs, ssp) | (SS_DIROP|SS_CONT));
	}

	if (sp->seg_flags & SEGM_CLEAN) {
		lfs_gather(fs, sp, vp, lfs_match_fake);
		/*
		 * For a file being flushed, we need to write *all* blocks.
		 * This means writing the cleaning blocks first, and then
		 * immediately following with any non-cleaning blocks.
		 * The same is true of the Ifile since checkpoints assume
		 * that all valid Ifile blocks are written.
		 */
		if (IS_FLUSHING(fs, vp) || vp == fs->lfs_ivnode) {
			lfs_gather(fs, sp, vp, lfs_match_data);
			/*
			 * Don't call VOP_PUTPAGES: if we're flushing,
			 * we've already done it, and the Ifile doesn't
			 * use the page cache.
			 */
		}
	} else {
		lfs_gather(fs, sp, vp, lfs_match_data);
		/*
		 * If we're flushing, we've already called VOP_PUTPAGES
		 * so don't do it again.  Otherwise, we want to write
		 * everything we've got.
		 */
		if (!IS_FLUSHING(fs, vp)) {
			mutex_enter(vp->v_interlock);
			error = VOP_PUTPAGES(vp, 0, 0,
				PGO_CLEANIT | PGO_ALLPAGES | PGO_LOCKED);
		}
	}

	/*
	 * It may not be necessary to write the meta-data blocks at this point,
	 * as the roll-forward recovery code should be able to reconstruct the
	 * list.
	 *
	 * We have to write them anyway, though, under two conditions: (1) the
	 * vnode is being flushed (for reuse by vinvalbuf); or (2) we are
	 * checkpointing.
	 *
	 * BUT if we are cleaning, we might have indirect blocks that refer to
	 * new blocks not being written yet, in addition to fragments being
	 * moved out of a cleaned segment.  If that is the case, don't
	 * write the indirect blocks, or the finfo will have a small block
	 * in the middle of it!
	 * XXX in this case isn't the inode size wrong too?
	 */
	frag = 0;
	if (sp->seg_flags & SEGM_CLEAN) {
		for (i = 0; i < ULFS_NDADDR; i++)
			if (ip->i_lfs_fragsize[i] > 0 &&
			    ip->i_lfs_fragsize[i] < lfs_sb_getbsize(fs))
				++frag;
	}
#ifdef DIAGNOSTIC
	if (frag > 1)
		panic("lfs_writefile: more than one fragment!");
#endif
	if (IS_FLUSHING(fs, vp) ||
	    (frag == 0 && (lfs_writeindir || (sp->seg_flags & SEGM_CKP)))) {
		lfs_gather(fs, sp, vp, lfs_match_indir);
		lfs_gather(fs, sp, vp, lfs_match_dindir);
		lfs_gather(fs, sp, vp, lfs_match_tindir);
	}
	lfs_release_finfo(fs);

	return error;
}

/*
 * Update segment accounting to reflect this inode's change of address.
 */
static int
lfs_update_iaddr(struct lfs *fs, struct segment *sp, struct inode *ip, daddr_t ndaddr)
{
	struct buf *bp;
	daddr_t daddr;
	IFILE *ifp;
	SEGUSE *sup;
	ino_t ino;
	int redo_ifile;
	u_int32_t sn;

	redo_ifile = 0;

	/*
	 * If updating the ifile, update the super-block.  Update the disk
	 * address and access times for this inode in the ifile.
	 */
	ino = ip->i_number;
	if (ino == LFS_IFILE_INUM) {
		daddr = lfs_sb_getidaddr(fs);
		lfs_sb_setidaddr(fs, LFS_DBTOFSB(fs, ndaddr));
	} else {
		LFS_IENTRY(ifp, fs, ino, bp);
		daddr = lfs_if_getdaddr(fs, ifp);
		lfs_if_setdaddr(fs, ifp, LFS_DBTOFSB(fs, ndaddr));
		(void)LFS_BWRITE_LOG(bp); /* Ifile */
	}

	/*
	 * If this is the Ifile and lfs_offset is set to the first block
	 * in the segment, dirty the new segment's accounting block
	 * (XXX should already be dirty?) and tell the caller to do it again.
	 */
	if (ip->i_number == LFS_IFILE_INUM) {
		sn = lfs_dtosn(fs, lfs_sb_getoffset(fs));
		if (lfs_sntod(fs, sn) + lfs_btofsb(fs, lfs_sb_getsumsize(fs)) ==
		    lfs_sb_getoffset(fs)) {
			LFS_SEGENTRY(sup, fs, sn, bp);
			KASSERT(bp->b_oflags & BO_DELWRI);
			LFS_WRITESEGENTRY(sup, fs, sn, bp);
			/* fs->lfs_flags |= LFS_IFDIRTY; */
			redo_ifile |= 1;
		}
	}

	/*
	 * The inode's last address should not be in the current partial
	 * segment, except under exceptional circumstances (lfs_writevnodes
	 * had to start over, and in the meantime more blocks were written
	 * to a vnode).	 Both inodes will be accounted to this segment
	 * in lfs_writeseg so we need to subtract the earlier version
	 * here anyway.	 The segment count can temporarily dip below
	 * zero here; keep track of how many duplicates we have in
	 * "dupino" so we don't panic below.
	 */
	if (daddr >= lfs_sb_getlastpseg(fs) && daddr <= lfs_sb_getoffset(fs)) {
		++sp->ndupino;
		DLOG((DLOG_SEG, "lfs_writeinode: last inode addr in current pseg "
		      "(ino %d daddr 0x%llx) ndupino=%d\n", ino,
		      (long long)daddr, sp->ndupino));
	}
	/*
	 * Account the inode: it no longer belongs to its former segment,
	 * though it will not belong to the new segment until that segment
	 * is actually written.
	 */
	if (daddr != LFS_UNUSED_DADDR) {
		u_int32_t oldsn = lfs_dtosn(fs, daddr);
#ifdef DIAGNOSTIC
		int ndupino = (sp->seg_number == oldsn) ? sp->ndupino : 0;
#endif
		LFS_SEGENTRY(sup, fs, oldsn, bp);
#ifdef DIAGNOSTIC
		if (sup->su_nbytes + DINOSIZE(fs) * ndupino < DINOSIZE(fs)) {
			printf("lfs_writeinode: negative bytes "
			       "(segment %" PRIu32 " short by %d, "
			       "oldsn=%" PRIu32 ", cursn=%" PRIu32
			       ", daddr=%" PRId64 ", su_nbytes=%u, "
			       "ndupino=%d)\n",
			       lfs_dtosn(fs, daddr),
			       (int)DINOSIZE(fs) *
				   (1 - sp->ndupino) - sup->su_nbytes,
			       oldsn, sp->seg_number, daddr,
			       (unsigned int)sup->su_nbytes,
			       sp->ndupino);
			panic("lfs_writeinode: negative bytes");
			sup->su_nbytes = DINOSIZE(fs);
		}
#endif
		DLOG((DLOG_SU, "seg %d -= %d for ino %d inode\n",
		      lfs_dtosn(fs, daddr), DINOSIZE(fs), ino));
		sup->su_nbytes -= DINOSIZE(fs);
		redo_ifile |=
			(ino == LFS_IFILE_INUM && !(bp->b_flags & B_GATHERED));
		if (redo_ifile) {
			mutex_enter(&lfs_lock);
			fs->lfs_flags |= LFS_IFDIRTY;
			mutex_exit(&lfs_lock);
			/* Don't double-account */
			lfs_sb_setidaddr(fs, 0x0);
		}
		LFS_WRITESEGENTRY(sup, fs, oldsn, bp); /* Ifile */
	}

	return redo_ifile;
}

int
lfs_writeinode(struct lfs *fs, struct segment *sp, struct inode *ip)
{
	struct buf *bp;
	union lfs_dinode *cdp;
	struct vnode *vp = ITOV(ip);
	daddr_t daddr;
	IINFO *iip;
	int i;
	int redo_ifile = 0;
	int gotblk = 0;
	int count;
	SEGSUM *ssp;

	ASSERT_SEGLOCK(fs);
	if (!(ip->i_flag & IN_ALLMOD) && !(vp->v_uflag & VU_DIROP))
		return (0);

	/* Can't write ifile when writer is not set */
	KASSERT(ip->i_number != LFS_IFILE_INUM || fs->lfs_writer > 0 ||
		(sp->seg_flags & SEGM_CLEAN));

	/*
	 * If this is the Ifile, see if writing it here will generate a
	 * temporary misaccounting.  If it will, do the accounting and write
	 * the blocks, postponing the inode write until the accounting is
	 * solid.
	 */
	count = 0;
	while (vp == fs->lfs_ivnode) {
		int redo = 0;

		if (sp->idp == NULL && sp->ibp == NULL &&
		    (sp->seg_bytes_left < lfs_sb_getibsize(fs) ||
		     sp->sum_bytes_left < sizeof(int32_t))) {
			(void) lfs_writeseg(fs, sp);
			continue;
		}

		/* Look for dirty Ifile blocks */
		LIST_FOREACH(bp, &fs->lfs_ivnode->v_dirtyblkhd, b_vnbufs) {
			if (!(bp->b_flags & B_GATHERED)) {
				redo = 1;
				break;
			}
		}

		if (redo == 0)
			redo = lfs_update_iaddr(fs, sp, ip, 0x0);
		if (redo == 0)
			break;

		if (sp->idp) {
			lfs_dino_setinumber(fs, sp->idp, 0);
			sp->idp = NULL;
		}
		++count;
		if (count > 2)
			log(LOG_NOTICE, "lfs_writeinode: looping count=%d\n", count);
		lfs_writefile(fs, sp, fs->lfs_ivnode);
	}

	/* Allocate a new inode block if necessary. */
	if ((ip->i_number != LFS_IFILE_INUM || sp->idp == NULL) &&
	    sp->ibp == NULL) {
		/* Allocate a new segment if necessary. */
		if (sp->seg_bytes_left < lfs_sb_getibsize(fs) ||
		    sp->sum_bytes_left < sizeof(int32_t))
			(void) lfs_writeseg(fs, sp);

		/* Get next inode block. */
		daddr = lfs_sb_getoffset(fs);
		lfs_sb_addoffset(fs, lfs_btofsb(fs, lfs_sb_getibsize(fs)));
		sp->ibp = *sp->cbpp++ =
			getblk(VTOI(fs->lfs_ivnode)->i_devvp,
			    LFS_FSBTODB(fs, daddr), lfs_sb_getibsize(fs), 0, 0);
		gotblk++;

		/* Zero out inode numbers */
		for (i = 0; i < LFS_INOPB(fs); ++i) {
			union lfs_dinode *tmpdi;

			tmpdi = (union lfs_dinode *)((char *)sp->ibp->b_data +
						     DINOSIZE(fs) * i);
			lfs_dino_setinumber(fs, tmpdi, 0);
		}

		++sp->start_bpp;
		lfs_sb_subavail(fs, lfs_btofsb(fs, lfs_sb_getibsize(fs)));
		/* Set remaining space counters. */
		sp->seg_bytes_left -= lfs_sb_getibsize(fs);
		sp->sum_bytes_left -= sizeof(int32_t);

		/* Store the address in the segment summary. */
		iip = NTH_IINFO(fs, sp->segsum, sp->ninodes / LFS_INOPB(fs));
		lfs_ii_setblock(fs, iip, daddr);
	}

	/* Check VU_DIROP in case there is a new file with no data blocks */
	if (vp->v_uflag & VU_DIROP) {
		ssp = (SEGSUM *)sp->segsum;
		lfs_ss_setflags(fs, ssp,
				lfs_ss_getflags(fs, ssp) | (SS_DIROP|SS_CONT));
	}

	/* Update the inode times and copy the inode onto the inode page. */
	/* XXX kludge --- don't redirty the ifile just to put times on it */
	if (ip->i_number != LFS_IFILE_INUM)
		LFS_ITIMES(ip, NULL, NULL, NULL);

	/*
	 * If this is the Ifile, and we've already written the Ifile in this
	 * partial segment, just overwrite it (it's not on disk yet) and
	 * continue.
	 *
	 * XXX we know that the bp that we get the second time around has
	 * already been gathered.
	 */
	if (ip->i_number == LFS_IFILE_INUM && sp->idp) {
		lfs_copy_dinode(fs, sp->idp, ip->i_din);
		ip->i_lfs_osize = ip->i_size;
		return 0;
	}

	bp = sp->ibp;
	cdp = DINO_IN_BLOCK(fs, bp->b_data, sp->ninodes % LFS_INOPB(fs));
	lfs_copy_dinode(fs, cdp, ip->i_din);

	/*
	 * This inode is on its way to disk; clear its VU_DIROP status when
	 * the write is complete.
	 */
	if (vp->v_uflag & VU_DIROP) {
		if (!(sp->seg_flags & SEGM_CLEAN))
			ip->i_flag |= IN_CDIROP;
		else {
			DLOG((DLOG_DIROP, "lfs_writeinode: not clearing dirop for cleaned ino %d\n", (int)ip->i_number));
		}
	}

	/*
	 * If cleaning, link counts and directory file sizes cannot change,
	 * since those would be directory operations---even if the file
	 * we are writing is marked VU_DIROP we should write the old values.
	 * If we're not cleaning, of course, update the values so we get
	 * current values the next time we clean.
	 */
	if (sp->seg_flags & SEGM_CLEAN) {
		if (vp->v_uflag & VU_DIROP) {
			lfs_dino_setnlink(fs, cdp, ip->i_lfs_odnlink);
			/* if (vp->v_type == VDIR) */
			lfs_dino_setsize(fs, cdp, ip->i_lfs_osize);
		}
	} else {
		ip->i_lfs_odnlink = lfs_dino_getnlink(fs, cdp);
		ip->i_lfs_osize = ip->i_size;
	}
		

	/* We can finish the segment accounting for truncations now */
	lfs_finalize_ino_seguse(fs, ip);

	/*
	 * If we are cleaning, ensure that we don't write UNWRITTEN disk
	 * addresses to disk; possibly change the on-disk record of
	 * the inode size, either by reverting to the previous size
	 * (in the case of cleaning) or by verifying the inode's block
	 * holdings (in the case of files being allocated as they are being
	 * written).
	 * XXX By not writing UNWRITTEN blocks, we are making the lfs_avail
	 * XXX count on disk wrong by the same amount.	We should be
	 * XXX able to "borrow" from lfs_avail and return it after the
	 * XXX Ifile is written.  See also in lfs_writeseg.
	 */

	/* Check file size based on highest allocated block */
	if (((lfs_dino_getmode(fs, ip->i_din) & LFS_IFMT) == LFS_IFREG ||
	     (lfs_dino_getmode(fs, ip->i_din) & LFS_IFMT) == LFS_IFDIR) &&
	    ip->i_size > ((ip->i_lfs_hiblk + 1) << lfs_sb_getbshift(fs))) {
		lfs_dino_setsize(fs, cdp, (ip->i_lfs_hiblk + 1) << lfs_sb_getbshift(fs));
		DLOG((DLOG_SEG, "lfs_writeinode: ino %d size %" PRId64 " -> %"
		      PRId64 "\n", (int)ip->i_number, ip->i_size, lfs_dino_getsize(fs, cdp)));
	}
	if (ip->i_lfs_effnblks != lfs_dino_getblocks(fs, ip->i_din)) {
		DLOG((DLOG_SEG, "lfs_writeinode: cleansing ino %d eff %jd != nblk %d)"
		      " at %jx\n", ip->i_number, (intmax_t)ip->i_lfs_effnblks,
		      lfs_dino_getblocks(fs, ip->i_din), (uintmax_t)lfs_sb_getoffset(fs)));
		for (i=0; i<ULFS_NDADDR; i++) {
			if (lfs_dino_getdb(fs, cdp, i) == UNWRITTEN) {
				DLOG((DLOG_SEG, "lfs_writeinode: wiping UNWRITTEN\n"));
				lfs_dino_setdb(fs, cdp, i, 0);
			}
		}
		for (i=0; i<ULFS_NIADDR; i++) {
			if (lfs_dino_getib(fs, cdp, i) == UNWRITTEN) {
				DLOG((DLOG_SEG, "lfs_writeinode: wiping UNWRITTEN\n"));
				lfs_dino_setib(fs, cdp, i, 0);
			}
		}
	}

#ifdef DIAGNOSTIC
	/*
	 * Check dinode held blocks against dinode size.
	 * This should be identical to the check in lfs_vget().
	 */
	for (i = (lfs_dino_getsize(fs, cdp) + lfs_sb_getbsize(fs) - 1) >> lfs_sb_getbshift(fs);
	     i < ULFS_NDADDR; i++) {
		KASSERT(i >= 0);
		if ((lfs_dino_getmode(fs, cdp) & LFS_IFMT) == LFS_IFLNK)
			continue;
		if (((lfs_dino_getmode(fs, cdp) & LFS_IFMT) == LFS_IFBLK ||
		     (lfs_dino_getmode(fs, cdp) & LFS_IFMT) == LFS_IFCHR) && i == 0)
			continue;
		if (lfs_dino_getdb(fs, cdp, i) != 0) {
# ifdef DEBUG
			lfs_dump_dinode(fs, cdp);
# endif
			panic("writing inconsistent inode");
		}
	}
#endif /* DIAGNOSTIC */

	if (ip->i_flag & IN_CLEANING)
		LFS_CLR_UINO(ip, IN_CLEANING);
	else {
		/* XXX IN_ALLMOD */
		LFS_CLR_UINO(ip, IN_ACCESSED | IN_ACCESS | IN_CHANGE |
			     IN_UPDATE | IN_MODIFY);
		if (ip->i_lfs_effnblks == lfs_dino_getblocks(fs, ip->i_din))
			LFS_CLR_UINO(ip, IN_MODIFIED);
		else {
			DLOG((DLOG_VNODE, "lfs_writeinode: ino %d: real "
			    "blks=%d, eff=%jd\n", ip->i_number,
			    lfs_dino_getblocks(fs, ip->i_din), (intmax_t)ip->i_lfs_effnblks));
		}
	}

	if (ip->i_number == LFS_IFILE_INUM) {
		/* We know sp->idp == NULL */
		sp->idp = DINO_IN_BLOCK(fs, bp, sp->ninodes % LFS_INOPB(fs));

		/* Not dirty any more */
		mutex_enter(&lfs_lock);
		fs->lfs_flags &= ~LFS_IFDIRTY;
		mutex_exit(&lfs_lock);
	}

	if (gotblk) {
		mutex_enter(&bufcache_lock);
		LFS_LOCK_BUF(bp);
		brelsel(bp, 0);
		mutex_exit(&bufcache_lock);
	}

	/* Increment inode count in segment summary block. */

	ssp = (SEGSUM *)sp->segsum;
	lfs_ss_setninos(fs, ssp, lfs_ss_getninos(fs, ssp) + 1);

	/* If this page is full, set flag to allocate a new page. */
	if (++sp->ninodes % LFS_INOPB(fs) == 0)
		sp->ibp = NULL;

	redo_ifile = lfs_update_iaddr(fs, sp, ip, bp->b_blkno);

	KASSERT(redo_ifile == 0);
	return (redo_ifile);
}

int
lfs_gatherblock(struct segment *sp, struct buf *bp, kmutex_t *mptr)
{
	struct lfs *fs;
	int vers;
	int j, blksinblk;

	ASSERT_SEGLOCK(sp->fs);
	/*
	 * If full, finish this segment.  We may be doing I/O, so
	 * release and reacquire the splbio().
	 */
#ifdef DIAGNOSTIC
	if (sp->vp == NULL)
		panic ("lfs_gatherblock: Null vp in segment");
#endif
	fs = sp->fs;
	blksinblk = howmany(bp->b_bcount, lfs_sb_getbsize(fs));
	if (sp->sum_bytes_left < sizeof(int32_t) * blksinblk ||
	    sp->seg_bytes_left < bp->b_bcount) {
		if (mptr)
			mutex_exit(mptr);
		lfs_updatemeta(sp);

		vers = lfs_fi_getversion(fs, sp->fip);
		(void) lfs_writeseg(fs, sp);

		/* Add the current file to the segment summary. */
		lfs_acquire_finfo(fs, VTOI(sp->vp)->i_number, vers);

		if (mptr)
			mutex_enter(mptr);
		return (1);
	}

	if (bp->b_flags & B_GATHERED) {
		DLOG((DLOG_SEG, "lfs_gatherblock: already gathered! Ino %ju,"
		      " lbn %" PRId64 "\n",
		      (uintmax_t)lfs_fi_getino(fs, sp->fip), bp->b_lblkno));
		return (0);
	}

	/* Insert into the buffer list, update the FINFO block. */
	bp->b_flags |= B_GATHERED;

	*sp->cbpp++ = bp;
	for (j = 0; j < blksinblk; j++) {
		unsigned bn;

		bn = lfs_fi_getnblocks(fs, sp->fip);
		lfs_fi_setnblocks(fs, sp->fip, bn+1);
		lfs_fi_setblock(fs, sp->fip, bn, bp->b_lblkno + j);
		/* This block's accounting moves from lfs_favail to lfs_avail */
		lfs_deregister_block(sp->vp, bp->b_lblkno + j);
	}

	sp->sum_bytes_left -= sizeof(int32_t) * blksinblk;
	sp->seg_bytes_left -= bp->b_bcount;
	return (0);
}

int
lfs_gather(struct lfs *fs, struct segment *sp, struct vnode *vp,
    int (*match)(struct lfs *, struct buf *))
{
	struct buf *bp, *nbp;
	int count = 0;

	ASSERT_SEGLOCK(fs);
	if (vp->v_type == VBLK)
		return 0;
	KASSERT(sp->vp == NULL);
	sp->vp = vp;
	mutex_enter(&bufcache_lock);

#ifndef LFS_NO_BACKBUF_HACK
/* This is a hack to see if ordering the blocks in LFS makes a difference. */
# define	BUF_OFFSET	\
	(((char *)&LIST_NEXT(bp, b_vnbufs)) - (char *)bp)
# define	BACK_BUF(BP)	\
	((struct buf *)(((char *)(BP)->b_vnbufs.le_prev) - BUF_OFFSET))
# define	BEG_OF_LIST	\
	((struct buf *)(((char *)&LIST_FIRST(&vp->v_dirtyblkhd)) - BUF_OFFSET))

loop:
	/* Find last buffer. */
	for (bp = LIST_FIRST(&vp->v_dirtyblkhd);
	     bp && LIST_NEXT(bp, b_vnbufs) != NULL;
	     bp = LIST_NEXT(bp, b_vnbufs))
		/* nothing */;
	for (; bp && bp != BEG_OF_LIST; bp = nbp) {
		nbp = BACK_BUF(bp);
#else /* LFS_NO_BACKBUF_HACK */
loop:
	for (bp = LIST_FIRST(&vp->v_dirtyblkhd); bp; bp = nbp) {
		nbp = LIST_NEXT(bp, b_vnbufs);
#endif /* LFS_NO_BACKBUF_HACK */
		if ((bp->b_cflags & BC_BUSY) != 0 ||
		    (bp->b_flags & B_GATHERED) != 0 || !match(fs, bp)) {
#ifdef DEBUG
			if (vp == fs->lfs_ivnode &&
			    (bp->b_cflags & BC_BUSY) != 0 &&
			    (bp->b_flags & B_GATHERED) == 0)
				log(LOG_NOTICE, "lfs_gather: ifile lbn %"
				      PRId64 " busy (%x) at 0x%jx",
				      bp->b_lblkno, bp->b_flags,
				      (uintmax_t)lfs_sb_getoffset(fs));
#endif
			continue;
		}
#ifdef DIAGNOSTIC
# ifdef LFS_USE_B_INVAL
		if ((bp->b_flags & BC_INVAL) != 0 && bp->b_iodone == NULL) {
			DLOG((DLOG_SEG, "lfs_gather: lbn %" PRId64
			      " is BC_INVAL\n", bp->b_lblkno));
			VOP_PRINT(bp->b_vp);
		}
# endif /* LFS_USE_B_INVAL */
		if (!(bp->b_oflags & BO_DELWRI))
			panic("lfs_gather: bp not BO_DELWRI");
		if (!(bp->b_flags & B_LOCKED)) {
			DLOG((DLOG_SEG, "lfs_gather: lbn %" PRId64
			      " blk %" PRId64 " not B_LOCKED\n",
			      bp->b_lblkno,
			      LFS_DBTOFSB(fs, bp->b_blkno)));
			VOP_PRINT(bp->b_vp);
			panic("lfs_gather: bp not B_LOCKED");
		}
#endif
		if (lfs_gatherblock(sp, bp, &bufcache_lock)) {
			goto loop;
		}
		count++;
	}
	mutex_exit(&bufcache_lock);
	lfs_updatemeta(sp);
	KASSERT(sp->vp == vp);
	sp->vp = NULL;
	return count;
}

#if DEBUG
# define DEBUG_OOFF(n) do {						\
	if (ooff == 0) {						\
		DLOG((DLOG_SEG, "lfs_updatemeta[%d]: warning: writing " \
			"ino %d lbn %" PRId64 " at 0x%" PRIx32		\
			", was 0x0 (or %" PRId64 ")\n",			\
			(n), ip->i_number, lbn, ndaddr, daddr));	\
	}								\
} while (0)
#else
# define DEBUG_OOFF(n)
#endif

/*
 * Change the given block's address to ndaddr, finding its previous
 * location using ulfs_bmaparray().
 *
 * Account for this change in the segment table.
 *
 * called with sp == NULL by roll-forwarding code.
 */
void
lfs_update_single(struct lfs *fs, struct segment *sp,
    struct vnode *vp, daddr_t lbn, daddr_t ndaddr, int size)
{
	SEGUSE *sup;
	struct buf *bp;
	struct indir a[ULFS_NIADDR + 2], *ap;
	struct inode *ip;
	daddr_t daddr, ooff;
	int num, error;
	int bb, osize, obb;

	ASSERT_SEGLOCK(fs);
	KASSERT(sp == NULL || sp->vp == vp);
	ip = VTOI(vp);

	error = ulfs_bmaparray(vp, lbn, &daddr, a, &num, NULL, NULL);
	if (error)
		panic("lfs_updatemeta: ulfs_bmaparray returned %d", error);

	KASSERT(daddr <= LFS_MAX_DADDR(fs));
	if (daddr > 0)
		daddr = LFS_DBTOFSB(fs, daddr);

	bb = lfs_numfrags(fs, size);
	switch (num) {
	    case 0:
		    ooff = lfs_dino_getdb(fs, ip->i_din, lbn);
		    DEBUG_OOFF(0);
		    if (ooff == UNWRITTEN)
			    lfs_dino_setblocks(fs, ip->i_din,
				lfs_dino_getblocks(fs, ip->i_din) + bb);
		    else {
			    /* possible fragment truncation or extension */
			    obb = lfs_btofsb(fs, ip->i_lfs_fragsize[lbn]);
			    lfs_dino_setblocks(fs, ip->i_din,
				lfs_dino_getblocks(fs, ip->i_din) + (bb-obb));
		    }
		    lfs_dino_setdb(fs, ip->i_din, lbn, ndaddr);
		    break;
	    case 1:
		    ooff = lfs_dino_getib(fs, ip->i_din, a[0].in_off);
		    DEBUG_OOFF(1);
		    if (ooff == UNWRITTEN)
			    lfs_dino_setblocks(fs, ip->i_din,
				lfs_dino_getblocks(fs, ip->i_din) + bb);
		    lfs_dino_setib(fs, ip->i_din, a[0].in_off, ndaddr);
		    break;
	    default:
		    ap = &a[num - 1];
		    if (bread(vp, ap->in_lbn, lfs_sb_getbsize(fs),
			B_MODIFY, &bp))
			    panic("lfs_updatemeta: bread bno %" PRId64,
				  ap->in_lbn);

		    ooff = lfs_iblock_get(fs, bp->b_data, ap->in_off);
		    DEBUG_OOFF(num);
		    if (ooff == UNWRITTEN)
			    lfs_dino_setblocks(fs, ip->i_din,
				lfs_dino_getblocks(fs, ip->i_din) + bb);
		    lfs_iblock_set(fs, bp->b_data, ap->in_off, ndaddr);
		    (void) VOP_BWRITE(bp->b_vp, bp);
	}

	KASSERT(ooff == 0 || ooff == UNWRITTEN || ooff == daddr);

	/* Update hiblk when extending the file */
	if (lbn > ip->i_lfs_hiblk)
		ip->i_lfs_hiblk = lbn;

	/*
	 * Though we'd rather it couldn't, this *can* happen right now
	 * if cleaning blocks and regular blocks coexist.
	 */
	/* KASSERT(daddr < fs->lfs_lastpseg || daddr > ndaddr); */

	/*
	 * Update segment usage information, based on old size
	 * and location.
	 */
	if (daddr > 0) {
		u_int32_t oldsn = lfs_dtosn(fs, daddr);
#ifdef DIAGNOSTIC
		int ndupino;

		if (sp && sp->seg_number == oldsn) {
			ndupino = sp->ndupino;
		} else {
			ndupino = 0;
		}
#endif
		KASSERT(oldsn < lfs_sb_getnseg(fs));
		if (lbn >= 0 && lbn < ULFS_NDADDR)
			osize = ip->i_lfs_fragsize[lbn];
		else
			osize = lfs_sb_getbsize(fs);
		LFS_SEGENTRY(sup, fs, oldsn, bp);
#ifdef DIAGNOSTIC
		if (sup->su_nbytes + DINOSIZE(fs) * ndupino < osize) {
			printf("lfs_updatemeta: negative bytes "
			       "(segment %" PRIu32 " short by %" PRId64
			       ")\n", lfs_dtosn(fs, daddr),
			       (int64_t)osize -
			       (DINOSIZE(fs) * ndupino + sup->su_nbytes));
			printf("lfs_updatemeta: ino %llu, lbn %" PRId64
			       ", addr = 0x%" PRIx64 "\n",
			       (unsigned long long)ip->i_number, lbn, daddr);
			printf("lfs_updatemeta: ndupino=%d\n", ndupino);
			panic("lfs_updatemeta: negative bytes");
			sup->su_nbytes = osize -
			    DINOSIZE(fs) * ndupino;
		}
#endif
		DLOG((DLOG_SU, "seg %" PRIu32 " -= %d for ino %d lbn %" PRId64
		      " db 0x%" PRIx64 "\n",
		      lfs_dtosn(fs, daddr), osize,
		      ip->i_number, lbn, daddr));
		sup->su_nbytes -= osize;
		if (!(bp->b_flags & B_GATHERED)) {
			mutex_enter(&lfs_lock);
			fs->lfs_flags |= LFS_IFDIRTY;
			mutex_exit(&lfs_lock);
		}
		LFS_WRITESEGENTRY(sup, fs, oldsn, bp);
	}
	/*
	 * Now that this block has a new address, and its old
	 * segment no longer owns it, we can forget about its
	 * old size.
	 */
	if (lbn >= 0 && lbn < ULFS_NDADDR)
		ip->i_lfs_fragsize[lbn] = size;
}

/*
 * Update the metadata that points to the blocks listed in the FINFO
 * array.
 */
void
lfs_updatemeta(struct segment *sp)
{
	struct buf *sbp;
	struct lfs *fs;
	struct vnode *vp;
	daddr_t lbn;
	int i, nblocks, num;
	int __diagused nblocks_orig;
	int bb;
	int bytesleft, size;
	unsigned lastlength;
	union lfs_blocks tmpptr;

	fs = sp->fs;
	vp = sp->vp;
	ASSERT_SEGLOCK(fs);

	/*
	 * This used to be:
	 *
	 *  nblocks = &sp->fip->fi_blocks[sp->fip->fi_nblocks] - sp->start_lbp;
	 *
	 * that is, it allowed for the possibility that start_lbp did
	 * not point to the beginning of the finfo block pointer area.
	 * This particular formulation is six kinds of painful in the
	 * lfs64 world where we have two sizes of block pointer, so
	 * unless/until everything can be cleaned up to not move
	 * start_lbp around but instead use an offset, we do the
	 * following:
	 *    1. Get NEXT_FINFO(sp->fip). This is the same pointer as
	 * &sp->fip->fi_blocks[sp->fip->fi_nblocks], just the wrong
	 * type. (Ugh.)
	 *    2. Cast it to void *, then assign it to a temporary
	 * union lfs_blocks.
	 *    3. Subtract start_lbp from that.
	 *    4. Save the value of nblocks in blocks_orig so we can
	 * assert below that it hasn't changed without repeating this
	 * rubbish.
	 *
	 * XXX.
	 */
	lfs_blocks_fromvoid(fs, &tmpptr, (void *)NEXT_FINFO(fs, sp->fip));
	nblocks = lfs_blocks_sub(fs, &tmpptr, &sp->start_lbp);
	nblocks_orig = nblocks;

	KASSERT(nblocks >= 0);
	KASSERT(vp != NULL);
	if (nblocks == 0)
		return;

	/*
	 * This count may be high due to oversize blocks from lfs_gop_write.
	 * Correct for this. (XXX we should be able to keep track of these.)
	 */
	for (i = 0; i < nblocks; i++) {
		if (sp->start_bpp[i] == NULL) {
			DLOG((DLOG_SEG, "lfs_updatemeta: nblocks = %d, not %d\n", i, nblocks));
			nblocks = i;
			break;
		}
		num = howmany(sp->start_bpp[i]->b_bcount, lfs_sb_getbsize(fs));
		KASSERT(sp->start_bpp[i]->b_lblkno >= 0 || num == 1);
		nblocks -= num - 1;
	}

#if 0
	/* pre-lfs64 assertion */
	KASSERT(vp->v_type == VREG ||
	   nblocks == &sp->fip->fi_blocks[sp->fip->fi_nblocks] - sp->start_lbp);
#else
	KASSERT(vp->v_type == VREG || nblocks == nblocks_orig);
#endif
	KASSERT(nblocks == sp->cbpp - sp->start_bpp);

	/*
	 * Sort the blocks.
	 *
	 * We have to sort even if the blocks come from the
	 * cleaner, because there might be other pending blocks on the
	 * same inode...and if we don't sort, and there are fragments
	 * present, blocks may be written in the wrong place.
	 */
	lfs_shellsort(fs, sp->start_bpp, &sp->start_lbp, nblocks, lfs_sb_getbsize(fs));

	/*
	 * Record the length of the last block in case it's a fragment.
	 * If there are indirect blocks present, they sort last.  An
	 * indirect block will be lfs_bsize and its presence indicates
	 * that you cannot have fragments.
	 *
	 * XXX This last is a lie.  A cleaned fragment can coexist with
	 * XXX a later indirect block.	This will continue to be
	 * XXX true until lfs_markv is fixed to do everything with
	 * XXX fake blocks (including fake inodes and fake indirect blocks).
	 */
	lastlength = ((sp->start_bpp[nblocks - 1]->b_bcount - 1) &
		lfs_sb_getbmask(fs)) + 1;
	lfs_fi_setlastlength(fs, sp->fip, lastlength);

	/*
	 * Assign disk addresses, and update references to the logical
	 * block and the segment usage information.
	 */
	for (i = nblocks; i--; ++sp->start_bpp) {
		sbp = *sp->start_bpp;
		lbn = lfs_blocks_get(fs, &sp->start_lbp, 0);
		KASSERT(sbp->b_lblkno == lbn);

		sbp->b_blkno = LFS_FSBTODB(fs, lfs_sb_getoffset(fs));

		/*
		 * If we write a frag in the wrong place, the cleaner won't
		 * be able to correctly identify its size later, and the
		 * segment will be uncleanable.	 (Even worse, it will assume
		 * that the indirect block that actually ends the list
		 * is of a smaller size!)
		 */
		if ((sbp->b_bcount & lfs_sb_getbmask(fs)) && i != 0)
			panic("lfs_updatemeta: fragment is not last block");

		/*
		 * For each subblock in this possibly oversized block,
		 * update its address on disk.
		 */
		KASSERT(lbn >= 0 || sbp->b_bcount == lfs_sb_getbsize(fs));
		KASSERT(vp == sbp->b_vp);
		for (bytesleft = sbp->b_bcount; bytesleft > 0;
		     bytesleft -= lfs_sb_getbsize(fs)) {
			size = MIN(bytesleft, lfs_sb_getbsize(fs));
			bb = lfs_numfrags(fs, size);
			lbn = lfs_blocks_get(fs, &sp->start_lbp, 0);
			lfs_blocks_inc(fs, &sp->start_lbp);
			lfs_update_single(fs, sp, sp->vp, lbn, lfs_sb_getoffset(fs),
			    size);
			lfs_sb_addoffset(fs, bb);
		}

	}

	/* This inode has been modified */
	LFS_SET_UINO(VTOI(vp), IN_MODIFIED);
}

/*
 * Move lfs_offset to a segment earlier than newsn.
 */
int
lfs_rewind(struct lfs *fs, int newsn)
{
	int sn, osn, isdirty;
	struct buf *bp;
	SEGUSE *sup;

	ASSERT_SEGLOCK(fs);

	osn = lfs_dtosn(fs, lfs_sb_getoffset(fs));
	if (osn < newsn)
		return 0;

	/* lfs_avail eats the remaining space in this segment */
	lfs_sb_subavail(fs, lfs_sb_getfsbpseg(fs) - (lfs_sb_getoffset(fs) - lfs_sb_getcurseg(fs)));

	/* Find a low-numbered segment */
	for (sn = 0; sn < lfs_sb_getnseg(fs); ++sn) {
		LFS_SEGENTRY(sup, fs, sn, bp);
		isdirty = sup->su_flags & SEGUSE_DIRTY;
		brelse(bp, 0);

		if (!isdirty)
			break;
	}
	if (sn == lfs_sb_getnseg(fs))
		panic("lfs_rewind: no clean segments");
	if (newsn >= 0 && sn >= newsn)
		return ENOENT;
	lfs_sb_setnextseg(fs, lfs_sntod(fs, sn));
	lfs_newseg(fs);
	lfs_sb_setoffset(fs, lfs_sb_getcurseg(fs));

	return 0;
}

/*
 * Start a new partial segment.
 *
 * Return 1 when we entered to a new segment.
 * Otherwise, return 0.
 */
int
lfs_initseg(struct lfs *fs)
{
	struct segment *sp = fs->lfs_sp;
	SEGSUM *ssp;
	struct buf *sbp;	/* buffer for SEGSUM */
	int repeat = 0;		/* return value */

	ASSERT_SEGLOCK(fs);
	/* Advance to the next segment. */
	if (!LFS_PARTIAL_FITS(fs)) {
		SEGUSE *sup;
		struct buf *bp;

		/* lfs_avail eats the remaining space */
		lfs_sb_subavail(fs, lfs_sb_getfsbpseg(fs) - (lfs_sb_getoffset(fs) -
						   lfs_sb_getcurseg(fs)));
		/* Wake up any cleaning procs waiting on this file system. */
		lfs_wakeup_cleaner(fs);
		lfs_newseg(fs);
		repeat = 1;
		lfs_sb_setoffset(fs, lfs_sb_getcurseg(fs));

		sp->seg_number = lfs_dtosn(fs, lfs_sb_getcurseg(fs));
		sp->seg_bytes_left = lfs_fsbtob(fs, lfs_sb_getfsbpseg(fs));

		/*
		 * If the segment contains a superblock, update the offset
		 * and summary address to skip over it.
		 */
		LFS_SEGENTRY(sup, fs, sp->seg_number, bp);
		if (sup->su_flags & SEGUSE_SUPERBLOCK) {
			lfs_sb_addoffset(fs, lfs_btofsb(fs, LFS_SBPAD));
			sp->seg_bytes_left -= LFS_SBPAD;
		}
		brelse(bp, 0);
		/* Segment zero could also contain the labelpad */
		if (lfs_sb_getversion(fs) > 1 && sp->seg_number == 0 &&
		    lfs_sb_gets0addr(fs) < lfs_btofsb(fs, LFS_LABELPAD)) {
			lfs_sb_addoffset(fs,
			    lfs_btofsb(fs, LFS_LABELPAD) - lfs_sb_gets0addr(fs));
			sp->seg_bytes_left -=
			    LFS_LABELPAD - lfs_fsbtob(fs, lfs_sb_gets0addr(fs));
		}
	} else {
		sp->seg_number = lfs_dtosn(fs, lfs_sb_getcurseg(fs));
		sp->seg_bytes_left = lfs_fsbtob(fs, lfs_sb_getfsbpseg(fs) -
				      (lfs_sb_getoffset(fs) - lfs_sb_getcurseg(fs)));
	}
	lfs_sb_setlastpseg(fs, lfs_sb_getoffset(fs));

	/* Record first address of this partial segment */
	if (sp->seg_flags & SEGM_CLEAN) {
		fs->lfs_cleanint[fs->lfs_cleanind] = lfs_sb_getoffset(fs);
		if (++fs->lfs_cleanind >= LFS_MAX_CLEANIND) {
			/* "1" is the artificial inc in lfs_seglock */
			mutex_enter(&lfs_lock);
			while (fs->lfs_iocount > 1) {
				mtsleep(&fs->lfs_iocount, PRIBIO + 1,
				    "lfs_initseg", 0, &lfs_lock);
			}
			mutex_exit(&lfs_lock);
			fs->lfs_cleanind = 0;
		}
	}

	sp->fs = fs;
	sp->ibp = NULL;
	sp->idp = NULL;
	sp->ninodes = 0;
	sp->ndupino = 0;

	sp->cbpp = sp->bpp;

	/* Get a new buffer for SEGSUM */
	sbp = lfs_newbuf(fs, VTOI(fs->lfs_ivnode)->i_devvp,
	    LFS_FSBTODB(fs, lfs_sb_getoffset(fs)), lfs_sb_getsumsize(fs), LFS_NB_SUMMARY);

	/* ... and enter it into the buffer list. */
	*sp->cbpp = sbp;
	sp->cbpp++;
	lfs_sb_addoffset(fs, lfs_btofsb(fs, lfs_sb_getsumsize(fs)));

	sp->start_bpp = sp->cbpp;

	/* Set point to SEGSUM, initialize it. */
	ssp = sp->segsum = sbp->b_data;
	memset(ssp, 0, lfs_sb_getsumsize(fs));
	lfs_ss_setnext(fs, ssp, lfs_sb_getnextseg(fs));
	lfs_ss_setnfinfo(fs, ssp, 0);
	lfs_ss_setninos(fs, ssp, 0);
	lfs_ss_setmagic(fs, ssp, SS_MAGIC);

	/* Set pointer to first FINFO, initialize it. */
	sp->fip = SEGSUM_FINFOBASE(fs, sp->segsum);
	lfs_fi_setnblocks(fs, sp->fip, 0);
	lfs_fi_setlastlength(fs, sp->fip, 0);
	lfs_blocks_fromfinfo(fs, &sp->start_lbp, sp->fip);

	sp->seg_bytes_left -= lfs_sb_getsumsize(fs);
	sp->sum_bytes_left = lfs_sb_getsumsize(fs) - SEGSUM_SIZE(fs);

	return (repeat);
}

/*
 * Remove SEGUSE_INVAL from all segments.
 */
void
lfs_unset_inval_all(struct lfs *fs)
{
	SEGUSE *sup;
	struct buf *bp;
	int i;

	for (i = 0; i < lfs_sb_getnseg(fs); i++) {
		LFS_SEGENTRY(sup, fs, i, bp);
		if (sup->su_flags & SEGUSE_INVAL) {
			sup->su_flags &= ~SEGUSE_INVAL;
			LFS_WRITESEGENTRY(sup, fs, i, bp);
		} else
			brelse(bp, 0);
	}
}

/*
 * Return the next segment to write.
 */
void
lfs_newseg(struct lfs *fs)
{
	CLEANERINFO *cip;
	SEGUSE *sup;
	struct buf *bp;
	int curseg, isdirty, sn, skip_inval;

	ASSERT_SEGLOCK(fs);

	/* Honor LFCNWRAPSTOP */
	mutex_enter(&lfs_lock);
	while (lfs_sb_getnextseg(fs) < lfs_sb_getcurseg(fs) && fs->lfs_nowrap) {
		if (fs->lfs_wrappass) {
			log(LOG_NOTICE, "%s: wrappass=%d\n",
				lfs_sb_getfsmnt(fs), fs->lfs_wrappass);
			fs->lfs_wrappass = 0;
			break;
		}
		fs->lfs_wrapstatus = LFS_WRAP_WAITING;
		wakeup(&fs->lfs_nowrap);
		log(LOG_NOTICE, "%s: waiting at log wrap\n", lfs_sb_getfsmnt(fs));
		mtsleep(&fs->lfs_wrappass, PVFS, "newseg", 10 * hz,
			&lfs_lock);
	}
	fs->lfs_wrapstatus = LFS_WRAP_GOING;
	mutex_exit(&lfs_lock);

	LFS_SEGENTRY(sup, fs, lfs_dtosn(fs, lfs_sb_getnextseg(fs)), bp);
	DLOG((DLOG_SU, "lfs_newseg: seg %d := 0 in newseg\n",
	      lfs_dtosn(fs, lfs_sb_getnextseg(fs))));
	sup->su_flags |= SEGUSE_DIRTY | SEGUSE_ACTIVE;
	sup->su_nbytes = 0;
	sup->su_nsums = 0;
	sup->su_ninos = 0;
	LFS_WRITESEGENTRY(sup, fs, lfs_dtosn(fs, lfs_sb_getnextseg(fs)), bp);

	LFS_CLEANERINFO(cip, fs, bp);
	lfs_ci_shiftcleantodirty(fs, cip, 1);
	lfs_sb_setnclean(fs, lfs_ci_getclean(fs, cip));
	LFS_SYNC_CLEANERINFO(cip, fs, bp, 1);

	lfs_sb_setlastseg(fs, lfs_sb_getcurseg(fs));
	lfs_sb_setcurseg(fs, lfs_sb_getnextseg(fs));
	skip_inval = 1;
	for (sn = curseg = lfs_dtosn(fs, lfs_sb_getcurseg(fs)) + lfs_sb_getinterleave(fs);;) {
		sn = (sn + 1) % lfs_sb_getnseg(fs);

		if (sn == curseg) {
			if (skip_inval)
				skip_inval = 0;
			else
				panic("lfs_nextseg: no clean segments");
		}
		LFS_SEGENTRY(sup, fs, sn, bp);
		isdirty = sup->su_flags & (SEGUSE_DIRTY | (skip_inval ? SEGUSE_INVAL : 0));
		/* Check SEGUSE_EMPTY as we go along */
		if (isdirty && sup->su_nbytes == 0 &&
		    !(sup->su_flags & SEGUSE_EMPTY))
			LFS_WRITESEGENTRY(sup, fs, sn, bp);
		else
			brelse(bp, 0);

		if (!isdirty)
			break;
	}
	if (skip_inval == 0)
		lfs_unset_inval_all(fs);

	++fs->lfs_nactive;
	lfs_sb_setnextseg(fs, lfs_sntod(fs, sn));
	if (lfs_dostats) {
		++lfs_stats.segsused;
	}
}

static struct buf *
lfs_newclusterbuf(struct lfs *fs, struct vnode *vp, daddr_t addr,
    int n)
{
	struct lfs_cluster *cl;
	struct buf **bpp, *bp;

	ASSERT_SEGLOCK(fs);
	cl = (struct lfs_cluster *)pool_get(&fs->lfs_clpool, PR_WAITOK);
	bpp = (struct buf **)pool_get(&fs->lfs_bpppool, PR_WAITOK);
	memset(cl, 0, sizeof(*cl));
	cl->fs = fs;
	cl->bpp = bpp;
	cl->bufcount = 0;
	cl->bufsize = 0;

	/* If this segment is being written synchronously, note that */
	if (fs->lfs_sp->seg_flags & SEGM_SYNC) {
		cl->flags |= LFS_CL_SYNC;
		cl->seg = fs->lfs_sp;
		++cl->seg->seg_iocount;
	}

	/* Get an empty buffer header, or maybe one with something on it */
	bp = getiobuf(vp, true);
	bp->b_dev = NODEV;
	bp->b_blkno = bp->b_lblkno = addr;
	bp->b_iodone = lfs_cluster_callback;
	bp->b_private = cl;

	return bp;
}

int
lfs_writeseg(struct lfs *fs, struct segment *sp)
{
	struct buf **bpp, *bp, *cbp, *newbp, *unbusybp;
	SEGUSE *sup;
	SEGSUM *ssp;
	int i;
	int do_again, nblocks, byteoffset;
	size_t el_size;
	struct lfs_cluster *cl;
	u_short ninos;
	struct vnode *devvp;
	char *p = NULL;
	struct vnode *vp;
	int32_t *daddrp;	/* XXX ondisk32 */
	int changed;
	u_int32_t sum;
	size_t sumstart;
#ifdef DEBUG
	FINFO *fip;
	int findex;
#endif

	ASSERT_SEGLOCK(fs);

	ssp = (SEGSUM *)sp->segsum;

	/*
	 * If there are no buffers other than the segment summary to write,
	 * don't do anything.  If we are the end of a dirop sequence, however,
	 * write the empty segment summary anyway, to help out the
	 * roll-forward agent.
	 */
	if ((nblocks = sp->cbpp - sp->bpp) == 1) {
		if ((lfs_ss_getflags(fs, ssp) & (SS_DIROP | SS_CONT)) != SS_DIROP)
			return 0;
	}

	/* Note if partial segment is being written by the cleaner */
	if (sp->seg_flags & SEGM_CLEAN)
		lfs_ss_setflags(fs, ssp, lfs_ss_getflags(fs, ssp) | SS_CLEAN);

	/* Note if we are writing to reclaim */
	if (sp->seg_flags & SEGM_RECLAIM) {
		lfs_ss_setflags(fs, ssp, lfs_ss_getflags(fs, ssp) | SS_RECLAIM);
		lfs_ss_setreclino(fs, ssp, fs->lfs_reclino);
	}

	devvp = VTOI(fs->lfs_ivnode)->i_devvp;

	/* Update the segment usage information. */
	LFS_SEGENTRY(sup, fs, sp->seg_number, bp);

	/* Loop through all blocks, except the segment summary. */
	for (bpp = sp->bpp; ++bpp < sp->cbpp; ) {
		if ((*bpp)->b_vp != devvp) {
			sup->su_nbytes += (*bpp)->b_bcount;
			DLOG((DLOG_SU, "seg %" PRIu32 " += %ld for ino %d"
			      " lbn %" PRId64 " db 0x%" PRIx64 "\n",
			      sp->seg_number, (*bpp)->b_bcount,
			      VTOI((*bpp)->b_vp)->i_number, (*bpp)->b_lblkno,
			      (*bpp)->b_blkno));
		}
	}

#ifdef DEBUG
	/* Check for zero-length and zero-version FINFO entries. */
	fip = SEGSUM_FINFOBASE(fs, ssp);
	for (findex = 0; findex < lfs_ss_getnfinfo(fs, ssp); findex++) {
		KDASSERT(lfs_fi_getnblocks(fs, fip) > 0);
		KDASSERT(lfs_fi_getversion(fs, fip) > 0);
		fip = NEXT_FINFO(fs, fip);
	}
#endif /* DEBUG */

	ninos = (lfs_ss_getninos(fs, ssp) + LFS_INOPB(fs) - 1) / LFS_INOPB(fs);
	DLOG((DLOG_SU, "seg %d += %d for %d inodes\n",
	      sp->seg_number,
	      lfs_ss_getninos(fs, ssp) * DINOSIZE(fs),
	      lfs_ss_getninos(fs, ssp)));
	sup->su_nbytes += lfs_ss_getninos(fs, ssp) * DINOSIZE(fs);
	/* sup->su_nbytes += lfs_sb_getsumsize(fs); */
	if (lfs_sb_getversion(fs) == 1)
		sup->su_olastmod = time_second;
	else
		sup->su_lastmod = time_second;
	sup->su_ninos += ninos;
	++sup->su_nsums;
	lfs_sb_subavail(fs, lfs_btofsb(fs, lfs_sb_getsumsize(fs)));

	do_again = !(bp->b_flags & B_GATHERED);
	LFS_WRITESEGENTRY(sup, fs, sp->seg_number, bp); /* Ifile */

	/*
	 * Mark blocks B_BUSY, to prevent then from being changed between
	 * the checksum computation and the actual write.
	 *
	 * If we are cleaning, check indirect blocks for UNWRITTEN, and if
	 * there are any, replace them with copies that have UNASSIGNED
	 * instead.
	 */
	mutex_enter(&bufcache_lock);
	for (bpp = sp->bpp, i = nblocks - 1; i--;) {
		++bpp;
		bp = *bpp;
		if (bp->b_iodone != NULL) {	 /* UBC or malloced buffer */
			bp->b_cflags |= BC_BUSY;
			continue;
		}

		while (bp->b_cflags & BC_BUSY) {
			DLOG((DLOG_SEG, "lfs_writeseg: avoiding potential"
			      " data summary corruption for ino %d, lbn %"
			      PRId64 "\n",
			      VTOI(bp->b_vp)->i_number, bp->b_lblkno));
			bp->b_cflags |= BC_WANTED;
			cv_wait(&bp->b_busy, &bufcache_lock);
		}
		bp->b_cflags |= BC_BUSY;
		mutex_exit(&bufcache_lock);
		unbusybp = NULL;

		/*
		 * Check and replace indirect block UNWRITTEN bogosity.
		 * XXX See comment in lfs_writefile.
		 */
		if (bp->b_lblkno < 0 && bp->b_vp != devvp && bp->b_vp &&
		   lfs_dino_getblocks(fs, VTOI(bp->b_vp)->i_din) !=
		   VTOI(bp->b_vp)->i_lfs_effnblks) {
			DLOG((DLOG_VNODE, "lfs_writeseg: cleansing ino %d (%jd != %d)\n",
			      VTOI(bp->b_vp)->i_number,
			      (intmax_t)VTOI(bp->b_vp)->i_lfs_effnblks,
			      lfs_dino_getblocks(fs, VTOI(bp->b_vp)->i_din)));
			/* Make a copy we'll make changes to */
			newbp = lfs_newbuf(fs, bp->b_vp, bp->b_lblkno,
					   bp->b_bcount, LFS_NB_IBLOCK);
			newbp->b_blkno = bp->b_blkno;
			memcpy(newbp->b_data, bp->b_data,
			       newbp->b_bcount);

			changed = 0;
			/* XXX ondisk32 */
			for (daddrp = (int32_t *)(newbp->b_data);
			     daddrp < (int32_t *)((char *)newbp->b_data +
						  newbp->b_bcount); daddrp++) {
				if (*daddrp == UNWRITTEN) {
					++changed;
					*daddrp = 0;
				}
			}
			/*
			 * Get rid of the old buffer.  Don't mark it clean,
			 * though, if it still has dirty data on it.
			 */
			if (changed) {
				DLOG((DLOG_SEG, "lfs_writeseg: replacing UNWRITTEN(%d):"
				      " bp = %p newbp = %p\n", changed, bp,
				      newbp));
				*bpp = newbp;
				bp->b_flags &= ~B_GATHERED;
				bp->b_error = 0;
				if (bp->b_iodone != NULL) {
					DLOG((DLOG_SEG, "lfs_writeseg: "
					      "indir bp should not be B_CALL\n"));
					biodone(bp);
					bp = NULL;
				} else {
					/* Still on free list, leave it there */
					unbusybp = bp;
					/*
					 * We have to re-decrement lfs_avail
					 * since this block is going to come
					 * back around to us in the next
					 * segment.
					 */
					lfs_sb_subavail(fs,
					    lfs_btofsb(fs, bp->b_bcount));
				}
			} else {
				lfs_freebuf(fs, newbp);
			}
		}
		mutex_enter(&bufcache_lock);
		if (unbusybp != NULL) {
			unbusybp->b_cflags &= ~BC_BUSY;
			if (unbusybp->b_cflags & BC_WANTED)
				cv_broadcast(&bp->b_busy);
		}
	}
	mutex_exit(&bufcache_lock);

	/*
	 * Compute checksum across data and then across summary; the first
	 * block (the summary block) is skipped.  Set the create time here
	 * so that it's guaranteed to be later than the inode mod times.
	 */
	sum = 0;
	if (lfs_sb_getversion(fs) == 1)
		el_size = sizeof(u_long);
	else
		el_size = sizeof(u_int32_t);
	for (bpp = sp->bpp, i = nblocks - 1; i--; ) {
		++bpp;
		/* Loop through gop_write cluster blocks */
		for (byteoffset = 0; byteoffset < (*bpp)->b_bcount;
		     byteoffset += lfs_sb_getbsize(fs)) {
#ifdef LFS_USE_B_INVAL
			if (((*bpp)->b_cflags & BC_INVAL) != 0 &&
			    (*bpp)->b_iodone != NULL) {
				if (copyin((void *)(*bpp)->b_saveaddr +
					   byteoffset, dp, el_size)) {
					panic("lfs_writeseg: copyin failed [1]:"
						" ino %d blk %" PRId64,
						VTOI((*bpp)->b_vp)->i_number,
						(*bpp)->b_lblkno);
				}
			} else
#endif /* LFS_USE_B_INVAL */
			{
				sum = lfs_cksum_part((char *)
				    (*bpp)->b_data + byteoffset, el_size, sum);
			}
		}
	}
	if (lfs_sb_getversion(fs) == 1)
		lfs_ss_setocreate(fs, ssp, time_second);
	else {
		lfs_ss_setcreate(fs, ssp, time_second);
		lfs_sb_addserial(fs, 1);
		lfs_ss_setserial(fs, ssp, lfs_sb_getserial(fs));
		lfs_ss_setident(fs, ssp, lfs_sb_getident(fs));
	}
	lfs_ss_setdatasum(fs, ssp, lfs_cksum_fold(sum));
	sumstart = lfs_ss_getsumstart(fs);
	lfs_ss_setsumsum(fs, ssp, cksum((char *)ssp + sumstart,
	    lfs_sb_getsumsize(fs) - sumstart));

	mutex_enter(&lfs_lock);
	lfs_sb_subbfree(fs, (lfs_btofsb(fs, ninos * lfs_sb_getibsize(fs)) +
			  lfs_btofsb(fs, lfs_sb_getsumsize(fs))));
	lfs_sb_adddmeta(fs, (lfs_btofsb(fs, ninos * lfs_sb_getibsize(fs)) +
			  lfs_btofsb(fs, lfs_sb_getsumsize(fs))));
	mutex_exit(&lfs_lock);

	/*
	 * When we simply write the blocks we lose a rotation for every block
	 * written.  To avoid this problem, we cluster the buffers into a
	 * chunk and write the chunk.  MAXPHYS is the largest size I/O
	 * devices can handle, use that for the size of the chunks.
	 *
	 * Blocks that are already clusters (from GOP_WRITE), however, we
	 * don't bother to copy into other clusters.
	 */

#define CHUNKSIZE MAXPHYS

	if (devvp == NULL)
		panic("devvp is NULL");
	for (bpp = sp->bpp, i = nblocks; i;) {
		cbp = lfs_newclusterbuf(fs, devvp, (*bpp)->b_blkno, i);
		cl = cbp->b_private;

		cbp->b_flags |= B_ASYNC;
		cbp->b_cflags |= BC_BUSY;
		cbp->b_bcount = 0;

#if defined(DEBUG) && defined(DIAGNOSTIC)
		if (bpp - sp->bpp > (lfs_sb_getsumsize(fs) - SEGSUM_SIZE(fs))
		    / sizeof(int32_t)) {
			panic("lfs_writeseg: real bpp overwrite");
		}
		if (bpp - sp->bpp > lfs_segsize(fs) / lfs_sb_getfsize(fs)) {
			panic("lfs_writeseg: theoretical bpp overwrite");
		}
#endif

		/*
		 * Construct the cluster.
		 */
		mutex_enter(&lfs_lock);
		++fs->lfs_iocount;
		mutex_exit(&lfs_lock);
		while (i && cbp->b_bcount < CHUNKSIZE) {
			bp = *bpp;

			if (bp->b_bcount > (CHUNKSIZE - cbp->b_bcount))
				break;
			if (cbp->b_bcount > 0 && !(cl->flags & LFS_CL_MALLOC))
				break;

			/* Clusters from GOP_WRITE are expedited */
			if (bp->b_bcount > lfs_sb_getbsize(fs)) {
				if (cbp->b_bcount > 0)
					/* Put in its own buffer */
					break;
				else {
					cbp->b_data = bp->b_data;
				}
			} else if (cbp->b_bcount == 0) {
				p = cbp->b_data = lfs_malloc(fs, CHUNKSIZE,
							     LFS_NB_CLUSTER);
				cl->flags |= LFS_CL_MALLOC;
			}
#ifdef DIAGNOSTIC
			if (lfs_dtosn(fs, LFS_DBTOFSB(fs, bp->b_blkno +
					      btodb(bp->b_bcount - 1))) !=
			    sp->seg_number) {
				printf("blk size %d daddr %" PRIx64
				    " not in seg %d\n",
				    bp->b_bcount, bp->b_blkno,
				    sp->seg_number);
				panic("segment overwrite");
			}
#endif

#ifdef LFS_USE_B_INVAL
			/*
			 * Fake buffers from the cleaner are marked as B_INVAL.
			 * We need to copy the data from user space rather than
			 * from the buffer indicated.
			 * XXX == what do I do on an error?
			 */
			if ((bp->b_cflags & BC_INVAL) != 0 &&
			    bp->b_iodone != NULL) {
				if (copyin(bp->b_saveaddr, p, bp->b_bcount))
					panic("lfs_writeseg: "
					    "copyin failed [2]");
			} else
#endif /* LFS_USE_B_INVAL */
			if (cl->flags & LFS_CL_MALLOC) {
				/* copy data into our cluster. */
				memcpy(p, bp->b_data, bp->b_bcount);
				p += bp->b_bcount;
			}

			cbp->b_bcount += bp->b_bcount;
			cl->bufsize += bp->b_bcount;

			bp->b_flags &= ~B_READ;
			bp->b_error = 0;
			cl->bpp[cl->bufcount++] = bp;

			vp = bp->b_vp;
			mutex_enter(&bufcache_lock);
			mutex_enter(vp->v_interlock);
			bp->b_oflags &= ~(BO_DELWRI | BO_DONE);
			reassignbuf(bp, vp);
			vp->v_numoutput++;
			mutex_exit(vp->v_interlock);
			mutex_exit(&bufcache_lock);

			bpp++;
			i--;
		}
		if (fs->lfs_sp->seg_flags & SEGM_SYNC)
			BIO_SETPRIO(cbp, BPRIO_TIMECRITICAL);
		else
			BIO_SETPRIO(cbp, BPRIO_TIMELIMITED);
		mutex_enter(devvp->v_interlock);
		devvp->v_numoutput++;
		mutex_exit(devvp->v_interlock);
		VOP_STRATEGY(devvp, cbp);
		curlwp->l_ru.ru_oublock++;
	}

	if (lfs_dostats) {
		++lfs_stats.psegwrites;
		lfs_stats.blocktot += nblocks - 1;
		if (fs->lfs_sp->seg_flags & SEGM_SYNC)
			++lfs_stats.psyncwrites;
		if (fs->lfs_sp->seg_flags & SEGM_CLEAN) {
			++lfs_stats.pcleanwrites;
			lfs_stats.cleanblocks += nblocks - 1;
		}
	}

	return (lfs_initseg(fs) || do_again);
}

void
lfs_writesuper(struct lfs *fs, daddr_t daddr)
{
	struct buf *bp;
	struct vnode *devvp = VTOI(fs->lfs_ivnode)->i_devvp;
	int s;

	ASSERT_MAYBE_SEGLOCK(fs);
#ifdef DIAGNOSTIC
	if (fs->lfs_is64) {
		KASSERT(fs->lfs_dlfs_u.u_64.dlfs_magic == LFS64_MAGIC);
	} else {
		KASSERT(fs->lfs_dlfs_u.u_32.dlfs_magic == LFS_MAGIC);
	}
#endif
	/*
	 * If we can write one superblock while another is in
	 * progress, we risk not having a complete checkpoint if we crash.
	 * So, block here if a superblock write is in progress.
	 */
	mutex_enter(&lfs_lock);
	s = splbio();
	while (fs->lfs_sbactive) {
		mtsleep(&fs->lfs_sbactive, PRIBIO+1, "lfs sb", 0,
			&lfs_lock);
	}
	fs->lfs_sbactive = daddr;
	splx(s);
	mutex_exit(&lfs_lock);

	/* Set timestamp of this version of the superblock */
	if (lfs_sb_getversion(fs) == 1)
		lfs_sb_setotstamp(fs, time_second);
	lfs_sb_settstamp(fs, time_second);

	/* The next chunk of code relies on this assumption */
	CTASSERT(sizeof(struct dlfs) == sizeof(struct dlfs64));

	/* Checksum the superblock and copy it into a buffer. */
	lfs_sb_setcksum(fs, lfs_sb_cksum(fs));
	bp = lfs_newbuf(fs, devvp,
	    LFS_FSBTODB(fs, daddr), LFS_SBPAD, LFS_NB_SBLOCK);
	memcpy(bp->b_data, &fs->lfs_dlfs_u, sizeof(struct dlfs));
	memset((char *)bp->b_data + sizeof(struct dlfs), 0,
	    LFS_SBPAD - sizeof(struct dlfs));

	bp->b_cflags |= BC_BUSY;
	bp->b_flags = (bp->b_flags & ~B_READ) | B_ASYNC;
	bp->b_oflags &= ~(BO_DONE | BO_DELWRI);
	bp->b_error = 0;
	bp->b_iodone = lfs_supercallback;

	if (fs->lfs_sp != NULL && fs->lfs_sp->seg_flags & SEGM_SYNC)
		BIO_SETPRIO(bp, BPRIO_TIMECRITICAL);
	else
		BIO_SETPRIO(bp, BPRIO_TIMELIMITED);
	curlwp->l_ru.ru_oublock++;

	mutex_enter(devvp->v_interlock);
	devvp->v_numoutput++;
	mutex_exit(devvp->v_interlock);

	mutex_enter(&lfs_lock);
	++fs->lfs_iocount;
	mutex_exit(&lfs_lock);
	VOP_STRATEGY(devvp, bp);
}

/*
 * Logical block number match routines used when traversing the dirty block
 * chain.
 */
int
lfs_match_fake(struct lfs *fs, struct buf *bp)
{

	ASSERT_SEGLOCK(fs);
	return LFS_IS_MALLOC_BUF(bp);
}

#if 0
int
lfs_match_real(struct lfs *fs, struct buf *bp)
{

	ASSERT_SEGLOCK(fs);
	return (lfs_match_data(fs, bp) && !lfs_match_fake(fs, bp));
}
#endif

int
lfs_match_data(struct lfs *fs, struct buf *bp)
{

	ASSERT_SEGLOCK(fs);
	return (bp->b_lblkno >= 0);
}

int
lfs_match_indir(struct lfs *fs, struct buf *bp)
{
	daddr_t lbn;

	ASSERT_SEGLOCK(fs);
	lbn = bp->b_lblkno;
	return (lbn < 0 && (-lbn - ULFS_NDADDR) % LFS_NINDIR(fs) == 0);
}

int
lfs_match_dindir(struct lfs *fs, struct buf *bp)
{
	daddr_t lbn;

	ASSERT_SEGLOCK(fs);
	lbn = bp->b_lblkno;
	return (lbn < 0 && (-lbn - ULFS_NDADDR) % LFS_NINDIR(fs) == 1);
}

int
lfs_match_tindir(struct lfs *fs, struct buf *bp)
{
	daddr_t lbn;

	ASSERT_SEGLOCK(fs);
	lbn = bp->b_lblkno;
	return (lbn < 0 && (-lbn - ULFS_NDADDR) % LFS_NINDIR(fs) == 2);
}

static void
lfs_free_aiodone(struct buf *bp)
{
	struct lfs *fs;

	KERNEL_LOCK(1, curlwp);
	fs = bp->b_private;
	ASSERT_NO_SEGLOCK(fs);
	lfs_freebuf(fs, bp);
	KERNEL_UNLOCK_LAST(curlwp);
}

static void
lfs_super_aiodone(struct buf *bp)
{
	struct lfs *fs;

	KERNEL_LOCK(1, curlwp);
	fs = bp->b_private;
	ASSERT_NO_SEGLOCK(fs);
	mutex_enter(&lfs_lock);
	fs->lfs_sbactive = 0;
	if (--fs->lfs_iocount <= 1)
		wakeup(&fs->lfs_iocount);
	wakeup(&fs->lfs_sbactive);
	mutex_exit(&lfs_lock);
	lfs_freebuf(fs, bp);
	KERNEL_UNLOCK_LAST(curlwp);
}

static void
lfs_cluster_aiodone(struct buf *bp)
{
	struct lfs_cluster *cl;
	struct lfs *fs;
	struct buf *tbp, *fbp;
	struct vnode *vp, *devvp, *ovp;
	struct inode *ip;
	int error;

	KERNEL_LOCK(1, curlwp);

	error = bp->b_error;
	cl = bp->b_private;
	fs = cl->fs;
	devvp = VTOI(fs->lfs_ivnode)->i_devvp;
	ASSERT_NO_SEGLOCK(fs);

	/* Put the pages back, and release the buffer */
	while (cl->bufcount--) {
		tbp = cl->bpp[cl->bufcount];
		KASSERT(tbp->b_cflags & BC_BUSY);
		if (error) {
			tbp->b_error = error;
		}

		/*
		 * We're done with tbp.	 If it has not been re-dirtied since
		 * the cluster was written, free it.  Otherwise, keep it on
		 * the locked list to be written again.
		 */
		vp = tbp->b_vp;

		tbp->b_flags &= ~B_GATHERED;

		LFS_BCLEAN_LOG(fs, tbp);

		mutex_enter(&bufcache_lock);
		if (tbp->b_iodone == NULL) {
			KASSERT(tbp->b_flags & B_LOCKED);
			bremfree(tbp);
			if (vp) {
				mutex_enter(vp->v_interlock);
				reassignbuf(tbp, vp);
				mutex_exit(vp->v_interlock);
			}
			tbp->b_flags |= B_ASYNC; /* for biodone */
		}

		if (((tbp->b_flags | tbp->b_oflags) &
		    (B_LOCKED | BO_DELWRI)) == B_LOCKED)
			LFS_UNLOCK_BUF(tbp);

		if (tbp->b_oflags & BO_DONE) {
			DLOG((DLOG_SEG, "blk %d biodone already (flags %lx)\n",
				cl->bufcount, (long)tbp->b_flags));
		}

		if (tbp->b_iodone != NULL && !LFS_IS_MALLOC_BUF(tbp)) {
			/*
			 * A buffer from the page daemon.
			 * We use the same iodone as it does,
			 * so we must manually disassociate its
			 * buffers from the vp.
			 */
			if ((ovp = tbp->b_vp) != NULL) {
				/* This is just silly */
				mutex_enter(ovp->v_interlock);
				brelvp(tbp);
				mutex_exit(ovp->v_interlock);
				tbp->b_vp = vp;
				tbp->b_objlock = vp->v_interlock;
			}
			/* Put it back the way it was */
			tbp->b_flags |= B_ASYNC;
			/* Master buffers have BC_AGE */
			if (tbp->b_private == tbp)
				tbp->b_cflags |= BC_AGE;
		}
		mutex_exit(&bufcache_lock);

		biodone(tbp);

		/*
		 * If this is the last block for this vnode, but
		 * there are other blocks on its dirty list,
		 * set IN_MODIFIED/IN_CLEANING depending on what
		 * sort of block.  Only do this for our mount point,
		 * not for, e.g., inode blocks that are attached to
		 * the devvp.
		 * XXX KS - Shouldn't we set *both* if both types
		 * of blocks are present (traverse the dirty list?)
		 */
		mutex_enter(vp->v_interlock);
		mutex_enter(&lfs_lock);
		if (vp != devvp && vp->v_numoutput == 0 &&
		    (fbp = LIST_FIRST(&vp->v_dirtyblkhd)) != NULL) {
			ip = VTOI(vp);
			DLOG((DLOG_SEG, "lfs_cluster_aiodone: mark ino %d\n",
			       ip->i_number));
			if (LFS_IS_MALLOC_BUF(fbp))
				LFS_SET_UINO(ip, IN_CLEANING);
			else
				LFS_SET_UINO(ip, IN_MODIFIED);
		}
		cv_broadcast(&vp->v_cv);
		mutex_exit(&lfs_lock);
		mutex_exit(vp->v_interlock);
	}

	/* Fix up the cluster buffer, and release it */
	if (cl->flags & LFS_CL_MALLOC)
		lfs_free(fs, bp->b_data, LFS_NB_CLUSTER);
	putiobuf(bp);

	/* Note i/o done */
	if (cl->flags & LFS_CL_SYNC) {
		if (--cl->seg->seg_iocount == 0)
			wakeup(&cl->seg->seg_iocount);
	}
	mutex_enter(&lfs_lock);
#ifdef DIAGNOSTIC
	if (fs->lfs_iocount == 0)
		panic("lfs_cluster_aiodone: zero iocount");
#endif
	if (--fs->lfs_iocount <= 1)
		wakeup(&fs->lfs_iocount);
	mutex_exit(&lfs_lock);

	KERNEL_UNLOCK_LAST(curlwp);

	pool_put(&fs->lfs_bpppool, cl->bpp);
	cl->bpp = NULL;
	pool_put(&fs->lfs_clpool, cl);
}

static void
lfs_generic_callback(struct buf *bp, void (*aiodone)(struct buf *))
{
	/* reset b_iodone for when this is a single-buf i/o. */
	bp->b_iodone = aiodone;

	workqueue_enqueue(uvm.aiodone_queue, &bp->b_work, NULL);
}

static void
lfs_cluster_callback(struct buf *bp)
{

	lfs_generic_callback(bp, lfs_cluster_aiodone);
}

void
lfs_supercallback(struct buf *bp)
{

	lfs_generic_callback(bp, lfs_super_aiodone);
}

/*
 * The only buffers that are going to hit these functions are the
 * segment write blocks, or the segment summaries, or the superblocks.
 *
 * All of the above are created by lfs_newbuf, and so do not need to be
 * released via brelse.
 */
void
lfs_callback(struct buf *bp)
{

	lfs_generic_callback(bp, lfs_free_aiodone);
}

/*
 * Shellsort (diminishing increment sort) from Data Structures and
 * Algorithms, Aho, Hopcraft and Ullman, 1983 Edition, page 290;
 * see also Knuth Vol. 3, page 84.  The increments are selected from
 * formula (8), page 95.  Roughly O(N^3/2).
 */
/*
 * This is our own private copy of shellsort because we want to sort
 * two parallel arrays (the array of buffer pointers and the array of
 * logical block numbers) simultaneously.  Note that we cast the array
 * of logical block numbers to a unsigned in this routine so that the
 * negative block numbers (meta data blocks) sort AFTER the data blocks.
 */

static void
lfs_shellsort(struct lfs *fs,
	      struct buf **bp_array, union lfs_blocks *lb_array,
	      int nmemb, int size)
{
	static int __rsshell_increments[] = { 4, 1, 0 };
	int incr, *incrp, t1, t2;
	struct buf *bp_temp;

#ifdef DEBUG
	incr = 0;
	for (t1 = 0; t1 < nmemb; t1++) {
		for (t2 = 0; t2 * size < bp_array[t1]->b_bcount; t2++) {
			if (lfs_blocks_get(fs, lb_array, incr++) != bp_array[t1]->b_lblkno + t2) {
				/* dump before panic */
				printf("lfs_shellsort: nmemb=%d, size=%d\n",
				    nmemb, size);
				incr = 0;
				for (t1 = 0; t1 < nmemb; t1++) {
					const struct buf *bp = bp_array[t1];

					printf("bp[%d]: lbn=%" PRIu64 ", size=%"
					    PRIu64 "\n", t1,
					    (uint64_t)bp->b_bcount,
					    (uint64_t)bp->b_lblkno);
					printf("lbns:");
					for (t2 = 0; t2 * size < bp->b_bcount;
					    t2++) {
						printf(" %jd",
						    (intmax_t)lfs_blocks_get(fs, lb_array, incr++));
					}
					printf("\n");
				}
				panic("lfs_shellsort: inconsistent input");
			}
		}
	}
#endif

	for (incrp = __rsshell_increments; (incr = *incrp++) != 0;)
		for (t1 = incr; t1 < nmemb; ++t1)
			for (t2 = t1 - incr; t2 >= 0;)
				if ((u_int64_t)bp_array[t2]->b_lblkno >
				    (u_int64_t)bp_array[t2 + incr]->b_lblkno) {
					bp_temp = bp_array[t2];
					bp_array[t2] = bp_array[t2 + incr];
					bp_array[t2 + incr] = bp_temp;
					t2 -= incr;
				} else
					break;

	/* Reform the list of logical blocks */
	incr = 0;
	for (t1 = 0; t1 < nmemb; t1++) {
		for (t2 = 0; t2 * size < bp_array[t1]->b_bcount; t2++) {
			lfs_blocks_set(fs, lb_array, incr++, 
				       bp_array[t1]->b_lblkno + t2);
		}
	}
}

/*
 * Set up an FINFO entry for a new file.  The fip pointer is assumed to 
 * point at uninitialized space.
 */
void
lfs_acquire_finfo(struct lfs *fs, ino_t ino, int vers)
{
	struct segment *sp = fs->lfs_sp;
	SEGSUM *ssp;

	KASSERT(vers > 0);

	if (sp->seg_bytes_left < lfs_sb_getbsize(fs) ||
	    sp->sum_bytes_left < FINFOSIZE(fs) + LFS_BLKPTRSIZE(fs))
		(void) lfs_writeseg(fs, fs->lfs_sp);
	
	sp->sum_bytes_left -= FINFOSIZE(fs);
	ssp = (SEGSUM *)sp->segsum;
	lfs_ss_setnfinfo(fs, ssp, lfs_ss_getnfinfo(fs, ssp) + 1);
	lfs_fi_setnblocks(fs, sp->fip, 0);
	lfs_fi_setino(fs, sp->fip, ino);
	lfs_fi_setversion(fs, sp->fip, vers);
}

/*
 * Release the FINFO entry, either clearing out an unused entry or
 * advancing us to the next available entry.
 */
void
lfs_release_finfo(struct lfs *fs)
{
	struct segment *sp = fs->lfs_sp;
	SEGSUM *ssp;

	if (lfs_fi_getnblocks(fs, sp->fip) != 0) {
		sp->fip = NEXT_FINFO(fs, sp->fip);
		lfs_blocks_fromfinfo(fs, &sp->start_lbp, sp->fip);
	} else {
		/* XXX shouldn't this update sp->fip? */
		sp->sum_bytes_left += FINFOSIZE(fs);
		ssp = (SEGSUM *)sp->segsum;
		lfs_ss_setnfinfo(fs, ssp, lfs_ss_getnfinfo(fs, ssp) - 1);
	}
}
