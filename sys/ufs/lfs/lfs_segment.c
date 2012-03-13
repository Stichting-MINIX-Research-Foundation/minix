/*	$NetBSD: lfs_segment.c,v 1.222 2011/07/11 08:27:40 hannken Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: lfs_segment.c,v 1.222 2011/07/11 08:27:40 hannken Exp $");

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

#include <ufs/ufs/inode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/lfs/lfs.h>
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
	((fs)->lfs_fsbpseg - ((fs)->lfs_offset - (fs)->lfs_curseg) > \
	(fs)->lfs_frag)

/*
 * Figure out whether we should do a checkpoint write or go ahead with
 * an ordinary write.
 */
#define LFS_SHOULD_CHECKPOINT(fs, flags) \
        ((flags & SEGM_CLEAN) == 0 &&					\
	  ((fs->lfs_nactive > LFS_MAX_ACTIVE ||				\
	    (flags & SEGM_CKP) ||					\
	    fs->lfs_nclean < LFS_MAX_ACTIVE)))

int	 lfs_match_fake(struct lfs *, struct buf *);
void	 lfs_newseg(struct lfs *);
/* XXX ondisk32 */
void	 lfs_shellsort(struct buf **, int32_t *, int, int);
void	 lfs_supercallback(struct buf *);
void	 lfs_updatemeta(struct segment *);
void	 lfs_writesuper(struct lfs *, daddr_t);
int	 lfs_writevnodes(struct lfs *fs, struct mount *mp,
	    struct segment *sp, int dirops);

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
	ip->i_ffs1_mtime = ts.tv_sec;
	ip->i_ffs1_mtimensec = ts.tv_nsec;
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
	int loopcount;

	ip = VTOI(vp);
	fs = VFSTOUFS(vp->v_mount)->um_lfs;
	relock = 0;

    top:
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

				for (off = lblktosize(fs, bp->b_lblkno);
				     off < lblktosize(fs, bp->b_lblkno + 1);
				     off += PAGE_SIZE) {
					pg = uvm_pagelookup(&vp->v_uobj, off);
					if (pg == NULL)
						continue;
					if ((pg->flags & PG_CLEAN) == 0 ||
					    pmap_is_modified(pg)) {
						fs->lfs_avail += btofsb(fs,
							bp->b_bcount);
						wakeup(&fs->lfs_avail);
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
					fs->lfs_avail += btofsb(fs,
						bp->b_bcount);
					wakeup(&fs->lfs_avail);
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
	mutex_exit(vp->v_interlock);

	/* Protect against VI_XLOCK deadlock in vinvalbuf() */
	lfs_seglock(fs, SEGM_SYNC);

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
				fs->lfs_avail += btofsb(fs, bp->b_bcount);
				wakeup(&fs->lfs_avail);
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
		loopcount = 0;
		do {
			if (LIST_FIRST(&vp->v_dirtyblkhd) != NULL) {
				relock = lfs_writefile(fs, sp, vp);
				if (relock) {
					/*
					 * Might have to wait for the
					 * cleaner to run; but we're
					 * still not done with this vnode.
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

int
lfs_writevnodes(struct lfs *fs, struct mount *mp, struct segment *sp, int op)
{
	struct inode *ip;
	struct vnode *vp;
	int inodes_written = 0, only_cleaning;
	int error = 0;

	ASSERT_SEGLOCK(fs);
 loop:
	/* start at last (newest) vnode. */
	mutex_enter(&mntvnode_lock);
	TAILQ_FOREACH_REVERSE(vp, &mp->mnt_vnodelist, vnodelst, v_mntvnodes) {
		/*
		 * If the vnode that we are about to sync is no longer
		 * associated with this mount point, start over.
		 */
		if (vp->v_mount != mp) {
			DLOG((DLOG_VNODE, "lfs_writevnodes: starting over\n"));
			/*
			 * After this, pages might be busy
			 * due to our own previous putpages.
			 * Start actual segment write here to avoid deadlock.
			 */
			mutex_exit(&mntvnode_lock);
			(void)lfs_writeseg(fs, sp);
			goto loop;
		}

		mutex_enter(vp->v_interlock);
		if (vp->v_type == VNON || vismarker(vp) ||
		    (vp->v_iflag & VI_CLEAN) != 0) {
			mutex_exit(vp->v_interlock);
			continue;
		}

		ip = VTOI(vp);
		if ((op == VN_DIROP && !(vp->v_uflag & VU_DIROP)) ||
		    (op != VN_DIROP && op != VN_CLEAN &&
		    (vp->v_uflag & VU_DIROP))) {
			mutex_exit(vp->v_interlock);
			vndebug(vp,"dirop");
			continue;
		}

		if (op == VN_EMPTY && !VPISEMPTY(vp)) {
			mutex_exit(vp->v_interlock);
			vndebug(vp,"empty");
			continue;
		}

		if (op == VN_CLEAN && ip->i_number != LFS_IFILE_INUM
		   && vp != fs->lfs_flushvp
		   && !(ip->i_flag & IN_CLEANING)) {
			mutex_exit(vp->v_interlock);
			vndebug(vp,"cleaning");
			continue;
		}

		mutex_exit(&mntvnode_lock);
		if (lfs_vref(vp)) {
			vndebug(vp,"vref");
			mutex_enter(&mntvnode_lock);
			continue;
		}

		only_cleaning = 0;
		/*
		 * Write the inode/file if dirty and it's not the IFILE.
		 */
		if ((ip->i_flag & IN_ALLMOD) || !VPISEMPTY(vp)) {
			only_cleaning =
			    ((ip->i_flag & IN_ALLMOD) == IN_CLEANING);

			if (ip->i_number != LFS_IFILE_INUM) {
				error = lfs_writefile(fs, sp, vp);
				if (error) {
					lfs_vunref(vp);
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
						mutex_enter(&mntvnode_lock);
						break;
					}
					error = 0; /* XXX not quite right */
					mutex_enter(&mntvnode_lock);
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
		}

		if (lfs_clean_vnhead && only_cleaning)
			lfs_vunref_head(vp);
		else
			lfs_vunref(vp);

		mutex_enter(&mntvnode_lock);
	}
	mutex_exit(&mntvnode_lock);
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
	int um_error;
	int loopcount;

	fs = VFSTOUFS(mp)->um_lfs;
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
				((SEGSUM *)(sp->segsum))->ss_flags &= ~(SS_CONT);
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
		segleft = fs->lfs_nseg;
		curseg = 0;
		for (n = 0; n < fs->lfs_segtabsz; n++) {
			dirty = 0;
			if (bread(fs->lfs_ivnode, fs->lfs_cleansz + n,
			    fs->lfs_bsize, NOCRED, B_MODIFY, &bp))
				panic("lfs_segwrite: ifile read");
			segusep = (SEGUSE *)bp->b_data;
			maxseg = min(segleft, fs->lfs_sepb);
			for (i = 0; i < maxseg; i++) {
				sn = curseg + i;
				if (sn != dtosn(fs, fs->lfs_curseg) &&
				    segusep->su_flags & SEGUSE_ACTIVE) {
					segusep->su_flags &= ~SEGUSE_ACTIVE;
					--fs->lfs_nactive;
					++dirty;
				}
				fs->lfs_suflags[fs->lfs_activesb][sn] =
					segusep->su_flags;
				if (fs->lfs_version > 1)
					++segusep;
				else
					segusep = (SEGUSE *)
						((SEGUSE_V1 *)segusep + 1);
			}

			if (dirty)
				error = LFS_BWRITE_LOG(bp); /* Ifile */
			else
				brelse(bp, 0);
			segleft -= fs->lfs_sepb;
			curseg += fs->lfs_sepb;
		}
	}

	KASSERT(LFS_SEGLOCK_HELD(fs));

	did_ckp = 0;
	if (do_ckp || fs->lfs_doifile) {
		vp = fs->lfs_ivnode;
		vn_lock(vp, LK_EXCLUSIVE);
		loopcount = 0;
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
				if (bp->b_lblkno < fs->lfs_cleansz +
				    fs->lfs_segtabsz &&
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
		VOP_UNLOCK(vp);
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
	struct finfo *fip;
	struct inode *ip;
	int i, frag;
	int error;

	ASSERT_SEGLOCK(fs);
	error = 0;
	ip = VTOI(vp);

	fip = sp->fip;
	lfs_acquire_finfo(fs, ip->i_number, ip->i_gen);

	if (vp->v_uflag & VU_DIROP)
		((SEGSUM *)(sp->segsum))->ss_flags |= (SS_DIROP|SS_CONT);

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
		for (i = 0; i < NDADDR; i++)
			if (ip->i_lfs_fragsize[i] > 0 &&
			    ip->i_lfs_fragsize[i] < fs->lfs_bsize)
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
	fip = sp->fip;
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
	int redo_ifile, error;
	u_int32_t sn;

	redo_ifile = 0;

	/*
	 * If updating the ifile, update the super-block.  Update the disk
	 * address and access times for this inode in the ifile.
	 */
	ino = ip->i_number;
	if (ino == LFS_IFILE_INUM) {
		daddr = fs->lfs_idaddr;
		fs->lfs_idaddr = dbtofsb(fs, ndaddr);
	} else {
		LFS_IENTRY(ifp, fs, ino, bp);
		daddr = ifp->if_daddr;
		ifp->if_daddr = dbtofsb(fs, ndaddr);
		error = LFS_BWRITE_LOG(bp); /* Ifile */
	}

	/*
	 * If this is the Ifile and lfs_offset is set to the first block
	 * in the segment, dirty the new segment's accounting block
	 * (XXX should already be dirty?) and tell the caller to do it again.
	 */
	if (ip->i_number == LFS_IFILE_INUM) {
		sn = dtosn(fs, fs->lfs_offset);
		if (sntod(fs, sn) + btofsb(fs, fs->lfs_sumsize) ==
		    fs->lfs_offset) {
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
	if (daddr >= fs->lfs_lastpseg && daddr <= fs->lfs_offset) {
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
		u_int32_t oldsn = dtosn(fs, daddr);
#ifdef DIAGNOSTIC
		int ndupino = (sp->seg_number == oldsn) ? sp->ndupino : 0;
#endif
		LFS_SEGENTRY(sup, fs, oldsn, bp);
#ifdef DIAGNOSTIC
		if (sup->su_nbytes +
		    sizeof (struct ufs1_dinode) * ndupino
		      < sizeof (struct ufs1_dinode)) {
			printf("lfs_writeinode: negative bytes "
			       "(segment %" PRIu32 " short by %d, "
			       "oldsn=%" PRIu32 ", cursn=%" PRIu32
			       ", daddr=%" PRId64 ", su_nbytes=%u, "
			       "ndupino=%d)\n",
			       dtosn(fs, daddr),
			       (int)sizeof (struct ufs1_dinode) *
				   (1 - sp->ndupino) - sup->su_nbytes,
			       oldsn, sp->seg_number, daddr,
			       (unsigned int)sup->su_nbytes,
			       sp->ndupino);
			panic("lfs_writeinode: negative bytes");
			sup->su_nbytes = sizeof (struct ufs1_dinode);
		}
#endif
		DLOG((DLOG_SU, "seg %d -= %d for ino %d inode\n",
		      dtosn(fs, daddr), sizeof (struct ufs1_dinode), ino));
		sup->su_nbytes -= sizeof (struct ufs1_dinode);
		redo_ifile |=
			(ino == LFS_IFILE_INUM && !(bp->b_flags & B_GATHERED));
		if (redo_ifile) {
			mutex_enter(&lfs_lock);
			fs->lfs_flags |= LFS_IFDIRTY;
			mutex_exit(&lfs_lock);
			/* Don't double-account */
			fs->lfs_idaddr = 0x0;
		}
		LFS_WRITESEGENTRY(sup, fs, oldsn, bp); /* Ifile */
	}

	return redo_ifile;
}

int
lfs_writeinode(struct lfs *fs, struct segment *sp, struct inode *ip)
{
	struct buf *bp;
	struct ufs1_dinode *cdp;
	daddr_t daddr;
	int32_t *daddrp;	/* XXX ondisk32 */
	int i, ndx;
	int redo_ifile = 0;
	int gotblk = 0;
	int count;

	ASSERT_SEGLOCK(fs);
	if (!(ip->i_flag & IN_ALLMOD))
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
	while (ip->i_number == LFS_IFILE_INUM) {
		int redo = 0;

		if (sp->idp == NULL && sp->ibp == NULL &&
		    (sp->seg_bytes_left < fs->lfs_ibsize ||
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
			sp->idp->di_inumber = 0;
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
		if (sp->seg_bytes_left < fs->lfs_ibsize ||
		    sp->sum_bytes_left < sizeof(int32_t))
			(void) lfs_writeseg(fs, sp);

		/* Get next inode block. */
		daddr = fs->lfs_offset;
		fs->lfs_offset += btofsb(fs, fs->lfs_ibsize);
		sp->ibp = *sp->cbpp++ =
			getblk(VTOI(fs->lfs_ivnode)->i_devvp,
			    fsbtodb(fs, daddr), fs->lfs_ibsize, 0, 0);
		gotblk++;

		/* Zero out inode numbers */
		for (i = 0; i < INOPB(fs); ++i)
			((struct ufs1_dinode *)sp->ibp->b_data)[i].di_inumber =
			    0;

		++sp->start_bpp;
		fs->lfs_avail -= btofsb(fs, fs->lfs_ibsize);
		/* Set remaining space counters. */
		sp->seg_bytes_left -= fs->lfs_ibsize;
		sp->sum_bytes_left -= sizeof(int32_t);
		ndx = fs->lfs_sumsize / sizeof(int32_t) -
			sp->ninodes / INOPB(fs) - 1;
		((int32_t *)(sp->segsum))[ndx] = daddr;
	}

	/* Check VU_DIROP in case there is a new file with no data blocks */
	if (ITOV(ip)->v_uflag & VU_DIROP)
		((SEGSUM *)(sp->segsum))->ss_flags |= (SS_DIROP|SS_CONT);

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
		*(sp->idp) = *ip->i_din.ffs1_din;
		ip->i_lfs_osize = ip->i_size;
		return 0;
	}

	bp = sp->ibp;
	cdp = ((struct ufs1_dinode *)bp->b_data) + (sp->ninodes % INOPB(fs));
	*cdp = *ip->i_din.ffs1_din;

	/*
	 * If cleaning, link counts and directory file sizes cannot change,
	 * since those would be directory operations---even if the file
	 * we are writing is marked VU_DIROP we should write the old values.
	 * If we're not cleaning, of course, update the values so we get
	 * current values the next time we clean.
	 */
	if (sp->seg_flags & SEGM_CLEAN) {
		if (ITOV(ip)->v_uflag & VU_DIROP) {
			cdp->di_nlink = ip->i_lfs_odnlink;
			/* if (ITOV(ip)->v_type == VDIR) */
			cdp->di_size = ip->i_lfs_osize;
		}
	} else {
		ip->i_lfs_odnlink = cdp->di_nlink;
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
	if (((ip->i_ffs1_mode & IFMT) == IFREG ||
	     (ip->i_ffs1_mode & IFMT) == IFDIR) &&
	    ip->i_size > ((ip->i_lfs_hiblk + 1) << fs->lfs_bshift)) {
		cdp->di_size = (ip->i_lfs_hiblk + 1) << fs->lfs_bshift;
		DLOG((DLOG_SEG, "lfs_writeinode: ino %d size %" PRId64 " -> %"
		      PRId64 "\n", (int)ip->i_number, ip->i_size, cdp->di_size));
	}
	if (ip->i_lfs_effnblks != ip->i_ffs1_blocks) {
		DLOG((DLOG_SEG, "lfs_writeinode: cleansing ino %d eff %d != nblk %d)"
		      " at %x\n", ip->i_number, ip->i_lfs_effnblks,
		      ip->i_ffs1_blocks, fs->lfs_offset));
		for (daddrp = cdp->di_db; daddrp < cdp->di_ib + NIADDR;
		     daddrp++) {
			if (*daddrp == UNWRITTEN) {
				DLOG((DLOG_SEG, "lfs_writeinode: wiping UNWRITTEN\n"));
				*daddrp = 0;
			}
		}
	}

#ifdef DIAGNOSTIC
	/*
	 * Check dinode held blocks against dinode size.
	 * This should be identical to the check in lfs_vget().
	 */
	for (i = (cdp->di_size + fs->lfs_bsize - 1) >> fs->lfs_bshift;
	     i < NDADDR; i++) {
		KASSERT(i >= 0);
		if ((cdp->di_mode & IFMT) == IFLNK)
			continue;
		if (((cdp->di_mode & IFMT) == IFBLK ||
		     (cdp->di_mode & IFMT) == IFCHR) && i == 0)
			continue;
		if (cdp->di_db[i] != 0) {
# ifdef DEBUG
			lfs_dump_dinode(cdp);
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
		if (ip->i_lfs_effnblks == ip->i_ffs1_blocks)
			LFS_CLR_UINO(ip, IN_MODIFIED);
		else {
			DLOG((DLOG_VNODE, "lfs_writeinode: ino %d: real "
			    "blks=%d, eff=%d\n", ip->i_number,
			    ip->i_ffs1_blocks, ip->i_lfs_effnblks));
		}
	}

	if (ip->i_number == LFS_IFILE_INUM) {
		/* We know sp->idp == NULL */
		sp->idp = ((struct ufs1_dinode *)bp->b_data) +
			(sp->ninodes % INOPB(fs));

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
	++((SEGSUM *)(sp->segsum))->ss_ninos;

	/* If this page is full, set flag to allocate a new page. */
	if (++sp->ninodes % INOPB(fs) == 0)
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
	blksinblk = howmany(bp->b_bcount, fs->lfs_bsize);
	if (sp->sum_bytes_left < sizeof(int32_t) * blksinblk ||
	    sp->seg_bytes_left < bp->b_bcount) {
		if (mptr)
			mutex_exit(mptr);
		lfs_updatemeta(sp);

		vers = sp->fip->fi_version;
		(void) lfs_writeseg(fs, sp);

		/* Add the current file to the segment summary. */
		lfs_acquire_finfo(fs, VTOI(sp->vp)->i_number, vers);

		if (mptr)
			mutex_enter(mptr);
		return (1);
	}

	if (bp->b_flags & B_GATHERED) {
		DLOG((DLOG_SEG, "lfs_gatherblock: already gathered! Ino %d,"
		      " lbn %" PRId64 "\n",
		      sp->fip->fi_ino, bp->b_lblkno));
		return (0);
	}

	/* Insert into the buffer list, update the FINFO block. */
	bp->b_flags |= B_GATHERED;

	*sp->cbpp++ = bp;
	for (j = 0; j < blksinblk; j++) {
		sp->fip->fi_blocks[sp->fip->fi_nblocks++] = bp->b_lblkno + j;
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
				      PRId64 " busy (%x) at 0x%x",
				      bp->b_lblkno, bp->b_flags,
				      (unsigned)fs->lfs_offset);
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
			      dbtofsb(fs, bp->b_blkno)));
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
 * location using ufs_bmaparray().
 *
 * Account for this change in the segment table.
 *
 * called with sp == NULL by roll-forwarding code.
 */
void
lfs_update_single(struct lfs *fs, struct segment *sp,
    struct vnode *vp, daddr_t lbn, int32_t ndaddr, int size)
{
	SEGUSE *sup;
	struct buf *bp;
	struct indir a[NIADDR + 2], *ap;
	struct inode *ip;
	daddr_t daddr, ooff;
	int num, error;
	int bb, osize, obb;

	ASSERT_SEGLOCK(fs);
	KASSERT(sp == NULL || sp->vp == vp);
	ip = VTOI(vp);

	error = ufs_bmaparray(vp, lbn, &daddr, a, &num, NULL, NULL);
	if (error)
		panic("lfs_updatemeta: ufs_bmaparray returned %d", error);

	daddr = (daddr_t)((int32_t)daddr); /* XXX ondisk32 */
	KASSERT(daddr <= LFS_MAX_DADDR);
	if (daddr > 0)
		daddr = dbtofsb(fs, daddr);

	bb = numfrags(fs, size);
	switch (num) {
	    case 0:
		    ooff = ip->i_ffs1_db[lbn];
		    DEBUG_OOFF(0);
		    if (ooff == UNWRITTEN)
			    ip->i_ffs1_blocks += bb;
		    else {
			    /* possible fragment truncation or extension */
			    obb = btofsb(fs, ip->i_lfs_fragsize[lbn]);
			    ip->i_ffs1_blocks += (bb - obb);
		    }
		    ip->i_ffs1_db[lbn] = ndaddr;
		    break;
	    case 1:
		    ooff = ip->i_ffs1_ib[a[0].in_off];
		    DEBUG_OOFF(1);
		    if (ooff == UNWRITTEN)
			    ip->i_ffs1_blocks += bb;
		    ip->i_ffs1_ib[a[0].in_off] = ndaddr;
		    break;
	    default:
		    ap = &a[num - 1];
		    if (bread(vp, ap->in_lbn, fs->lfs_bsize, NOCRED,
			B_MODIFY, &bp))
			    panic("lfs_updatemeta: bread bno %" PRId64,
				  ap->in_lbn);

		    /* XXX ondisk32 */
		    ooff = ((int32_t *)bp->b_data)[ap->in_off];
		    DEBUG_OOFF(num);
		    if (ooff == UNWRITTEN)
			    ip->i_ffs1_blocks += bb;
		    /* XXX ondisk32 */
		    ((int32_t *)bp->b_data)[ap->in_off] = ndaddr;
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
		u_int32_t oldsn = dtosn(fs, daddr);
#ifdef DIAGNOSTIC
		int ndupino;

		if (sp && sp->seg_number == oldsn) {
			ndupino = sp->ndupino;
		} else {
			ndupino = 0;
		}
#endif
		KASSERT(oldsn < fs->lfs_nseg);
		if (lbn >= 0 && lbn < NDADDR)
			osize = ip->i_lfs_fragsize[lbn];
		else
			osize = fs->lfs_bsize;
		LFS_SEGENTRY(sup, fs, oldsn, bp);
#ifdef DIAGNOSTIC
		if (sup->su_nbytes + sizeof (struct ufs1_dinode) * ndupino
		    < osize) {
			printf("lfs_updatemeta: negative bytes "
			       "(segment %" PRIu32 " short by %" PRId64
			       ")\n", dtosn(fs, daddr),
			       (int64_t)osize -
			       (sizeof (struct ufs1_dinode) * ndupino +
				sup->su_nbytes));
			printf("lfs_updatemeta: ino %llu, lbn %" PRId64
			       ", addr = 0x%" PRIx64 "\n",
			       (unsigned long long)ip->i_number, lbn, daddr);
			printf("lfs_updatemeta: ndupino=%d\n", ndupino);
			panic("lfs_updatemeta: negative bytes");
			sup->su_nbytes = osize -
			    sizeof (struct ufs1_dinode) * ndupino;
		}
#endif
		DLOG((DLOG_SU, "seg %" PRIu32 " -= %d for ino %d lbn %" PRId64
		      " db 0x%" PRIx64 "\n",
		      dtosn(fs, daddr), osize,
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
	if (lbn >= 0 && lbn < NDADDR)
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
	int bb;
	int bytesleft, size;

	ASSERT_SEGLOCK(sp->fs);
	vp = sp->vp;
	nblocks = &sp->fip->fi_blocks[sp->fip->fi_nblocks] - sp->start_lbp;
	KASSERT(nblocks >= 0);
	KASSERT(vp != NULL);
	if (nblocks == 0)
		return;

	/*
	 * This count may be high due to oversize blocks from lfs_gop_write.
	 * Correct for this. (XXX we should be able to keep track of these.)
	 */
	fs = sp->fs;
	for (i = 0; i < nblocks; i++) {
		if (sp->start_bpp[i] == NULL) {
			DLOG((DLOG_SEG, "lfs_updatemeta: nblocks = %d, not %d\n", i, nblocks));
			nblocks = i;
			break;
		}
		num = howmany(sp->start_bpp[i]->b_bcount, fs->lfs_bsize);
		KASSERT(sp->start_bpp[i]->b_lblkno >= 0 || num == 1);
		nblocks -= num - 1;
	}

	KASSERT(vp->v_type == VREG ||
	   nblocks == &sp->fip->fi_blocks[sp->fip->fi_nblocks] - sp->start_lbp);
	KASSERT(nblocks == sp->cbpp - sp->start_bpp);

	/*
	 * Sort the blocks.
	 *
	 * We have to sort even if the blocks come from the
	 * cleaner, because there might be other pending blocks on the
	 * same inode...and if we don't sort, and there are fragments
	 * present, blocks may be written in the wrong place.
	 */
	lfs_shellsort(sp->start_bpp, sp->start_lbp, nblocks, fs->lfs_bsize);

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
	sp->fip->fi_lastlength = ((sp->start_bpp[nblocks - 1]->b_bcount - 1) &
		fs->lfs_bmask) + 1;

	/*
	 * Assign disk addresses, and update references to the logical
	 * block and the segment usage information.
	 */
	for (i = nblocks; i--; ++sp->start_bpp) {
		sbp = *sp->start_bpp;
		lbn = *sp->start_lbp;
		KASSERT(sbp->b_lblkno == lbn);

		sbp->b_blkno = fsbtodb(fs, fs->lfs_offset);

		/*
		 * If we write a frag in the wrong place, the cleaner won't
		 * be able to correctly identify its size later, and the
		 * segment will be uncleanable.	 (Even worse, it will assume
		 * that the indirect block that actually ends the list
		 * is of a smaller size!)
		 */
		if ((sbp->b_bcount & fs->lfs_bmask) && i != 0)
			panic("lfs_updatemeta: fragment is not last block");

		/*
		 * For each subblock in this possibly oversized block,
		 * update its address on disk.
		 */
		KASSERT(lbn >= 0 || sbp->b_bcount == fs->lfs_bsize);
		KASSERT(vp == sbp->b_vp);
		for (bytesleft = sbp->b_bcount; bytesleft > 0;
		     bytesleft -= fs->lfs_bsize) {
			size = MIN(bytesleft, fs->lfs_bsize);
			bb = numfrags(fs, size);
			lbn = *sp->start_lbp++;
			lfs_update_single(fs, sp, sp->vp, lbn, fs->lfs_offset,
			    size);
			fs->lfs_offset += bb;
		}

	}

	/* This inode has been modified */
	LFS_SET_UINO(VTOI(vp), IN_MODIFIED);
}

/*
 * Move lfs_offset to a segment earlier than sn.
 */
int
lfs_rewind(struct lfs *fs, int newsn)
{
	int sn, osn, isdirty;
	struct buf *bp;
	SEGUSE *sup;

	ASSERT_SEGLOCK(fs);

	osn = dtosn(fs, fs->lfs_offset);
	if (osn < newsn)
		return 0;

	/* lfs_avail eats the remaining space in this segment */
	fs->lfs_avail -= fs->lfs_fsbpseg - (fs->lfs_offset - fs->lfs_curseg);

	/* Find a low-numbered segment */
	for (sn = 0; sn < fs->lfs_nseg; ++sn) {
		LFS_SEGENTRY(sup, fs, sn, bp);
		isdirty = sup->su_flags & SEGUSE_DIRTY;
		brelse(bp, 0);

		if (!isdirty)
			break;
	}
	if (sn == fs->lfs_nseg)
		panic("lfs_rewind: no clean segments");
	if (newsn >= 0 && sn >= newsn)
		return ENOENT;
	fs->lfs_nextseg = sn;
	lfs_newseg(fs);
	fs->lfs_offset = fs->lfs_curseg;

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
		fs->lfs_avail -= fs->lfs_fsbpseg - (fs->lfs_offset -
						   fs->lfs_curseg);
		/* Wake up any cleaning procs waiting on this file system. */
		lfs_wakeup_cleaner(fs);
		lfs_newseg(fs);
		repeat = 1;
		fs->lfs_offset = fs->lfs_curseg;

		sp->seg_number = dtosn(fs, fs->lfs_curseg);
		sp->seg_bytes_left = fsbtob(fs, fs->lfs_fsbpseg);

		/*
		 * If the segment contains a superblock, update the offset
		 * and summary address to skip over it.
		 */
		LFS_SEGENTRY(sup, fs, sp->seg_number, bp);
		if (sup->su_flags & SEGUSE_SUPERBLOCK) {
			fs->lfs_offset += btofsb(fs, LFS_SBPAD);
			sp->seg_bytes_left -= LFS_SBPAD;
		}
		brelse(bp, 0);
		/* Segment zero could also contain the labelpad */
		if (fs->lfs_version > 1 && sp->seg_number == 0 &&
		    fs->lfs_start < btofsb(fs, LFS_LABELPAD)) {
			fs->lfs_offset +=
			    btofsb(fs, LFS_LABELPAD) - fs->lfs_start;
			sp->seg_bytes_left -=
			    LFS_LABELPAD - fsbtob(fs, fs->lfs_start);
		}
	} else {
		sp->seg_number = dtosn(fs, fs->lfs_curseg);
		sp->seg_bytes_left = fsbtob(fs, fs->lfs_fsbpseg -
				      (fs->lfs_offset - fs->lfs_curseg));
	}
	fs->lfs_lastpseg = fs->lfs_offset;

	/* Record first address of this partial segment */
	if (sp->seg_flags & SEGM_CLEAN) {
		fs->lfs_cleanint[fs->lfs_cleanind] = fs->lfs_offset;
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
	    fsbtodb(fs, fs->lfs_offset), fs->lfs_sumsize, LFS_NB_SUMMARY);

	/* ... and enter it into the buffer list. */
	*sp->cbpp = sbp;
	sp->cbpp++;
	fs->lfs_offset += btofsb(fs, fs->lfs_sumsize);

	sp->start_bpp = sp->cbpp;

	/* Set point to SEGSUM, initialize it. */
	ssp = sp->segsum = sbp->b_data;
	memset(ssp, 0, fs->lfs_sumsize);
	ssp->ss_next = fs->lfs_nextseg;
	ssp->ss_nfinfo = ssp->ss_ninos = 0;
	ssp->ss_magic = SS_MAGIC;

	/* Set pointer to first FINFO, initialize it. */
	sp->fip = (struct finfo *)((char *)sp->segsum + SEGSUM_SIZE(fs));
	sp->fip->fi_nblocks = 0;
	sp->start_lbp = &sp->fip->fi_blocks[0];
	sp->fip->fi_lastlength = 0;

	sp->seg_bytes_left -= fs->lfs_sumsize;
	sp->sum_bytes_left = fs->lfs_sumsize - SEGSUM_SIZE(fs);

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

	for (i = 0; i < fs->lfs_nseg; i++) {
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
	while (fs->lfs_nextseg < fs->lfs_curseg && fs->lfs_nowrap) {
		if (fs->lfs_wrappass) {
			log(LOG_NOTICE, "%s: wrappass=%d\n",
				fs->lfs_fsmnt, fs->lfs_wrappass);
			fs->lfs_wrappass = 0;
			break;
		}
		fs->lfs_wrapstatus = LFS_WRAP_WAITING;
		wakeup(&fs->lfs_nowrap);
		log(LOG_NOTICE, "%s: waiting at log wrap\n", fs->lfs_fsmnt);
		mtsleep(&fs->lfs_wrappass, PVFS, "newseg", 10 * hz,
			&lfs_lock);
	}
	fs->lfs_wrapstatus = LFS_WRAP_GOING;
	mutex_exit(&lfs_lock);

	LFS_SEGENTRY(sup, fs, dtosn(fs, fs->lfs_nextseg), bp);
	DLOG((DLOG_SU, "lfs_newseg: seg %d := 0 in newseg\n",
	      dtosn(fs, fs->lfs_nextseg)));
	sup->su_flags |= SEGUSE_DIRTY | SEGUSE_ACTIVE;
	sup->su_nbytes = 0;
	sup->su_nsums = 0;
	sup->su_ninos = 0;
	LFS_WRITESEGENTRY(sup, fs, dtosn(fs, fs->lfs_nextseg), bp);

	LFS_CLEANERINFO(cip, fs, bp);
	--cip->clean;
	++cip->dirty;
	fs->lfs_nclean = cip->clean;
	LFS_SYNC_CLEANERINFO(cip, fs, bp, 1);

	fs->lfs_lastseg = fs->lfs_curseg;
	fs->lfs_curseg = fs->lfs_nextseg;
	skip_inval = 1;
	for (sn = curseg = dtosn(fs, fs->lfs_curseg) + fs->lfs_interleave;;) {
		sn = (sn + 1) % fs->lfs_nseg;

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
	fs->lfs_nextseg = sntod(fs, sn);
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
		if ((ssp->ss_flags & (SS_DIROP | SS_CONT)) != SS_DIROP)
			return 0;
	}

	/* Note if partial segment is being written by the cleaner */
	if (sp->seg_flags & SEGM_CLEAN)
		ssp->ss_flags |= SS_CLEAN;

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
	fip = (struct finfo *)((char *)ssp + SEGSUM_SIZE(fs));
	for (findex = 0; findex < ssp->ss_nfinfo; findex++) {
		KDASSERT(fip->fi_nblocks > 0);
		KDASSERT(fip->fi_version > 0);
		fip = (FINFO *)((char *)fip + FINFOSIZE +
			sizeof(int32_t) * fip->fi_nblocks);
	}
#endif /* DEBUG */

	ninos = (ssp->ss_ninos + INOPB(fs) - 1) / INOPB(fs);
	DLOG((DLOG_SU, "seg %d += %d for %d inodes\n",
	      sp->seg_number, ssp->ss_ninos * sizeof (struct ufs1_dinode),
	      ssp->ss_ninos));
	sup->su_nbytes += ssp->ss_ninos * sizeof (struct ufs1_dinode);
	/* sup->su_nbytes += fs->lfs_sumsize; */
	if (fs->lfs_version == 1)
		sup->su_olastmod = time_second;
	else
		sup->su_lastmod = time_second;
	sup->su_ninos += ninos;
	++sup->su_nsums;
	fs->lfs_avail -= btofsb(fs, fs->lfs_sumsize);

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
		   VTOI(bp->b_vp)->i_ffs1_blocks !=
		   VTOI(bp->b_vp)->i_lfs_effnblks) {
			DLOG((DLOG_VNODE, "lfs_writeseg: cleansing ino %d (%d != %d)\n",
			      VTOI(bp->b_vp)->i_number,
			      VTOI(bp->b_vp)->i_lfs_effnblks,
			      VTOI(bp->b_vp)->i_ffs1_blocks));
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
					fs->lfs_avail -=
					    btofsb(fs, bp->b_bcount);
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
	if (fs->lfs_version == 1)
		el_size = sizeof(u_long);
	else
		el_size = sizeof(u_int32_t);
	for (bpp = sp->bpp, i = nblocks - 1; i--; ) {
		++bpp;
		/* Loop through gop_write cluster blocks */
		for (byteoffset = 0; byteoffset < (*bpp)->b_bcount;
		     byteoffset += fs->lfs_bsize) {
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
	if (fs->lfs_version == 1)
		ssp->ss_ocreate = time_second;
	else {
		ssp->ss_create = time_second;
		ssp->ss_serial = ++fs->lfs_serial;
		ssp->ss_ident  = fs->lfs_ident;
	}
	ssp->ss_datasum = lfs_cksum_fold(sum);
	ssp->ss_sumsum = cksum(&ssp->ss_datasum,
	    fs->lfs_sumsize - sizeof(ssp->ss_sumsum));

	mutex_enter(&lfs_lock);
	fs->lfs_bfree -= (btofsb(fs, ninos * fs->lfs_ibsize) +
			  btofsb(fs, fs->lfs_sumsize));
	fs->lfs_dmeta += (btofsb(fs, ninos * fs->lfs_ibsize) +
			  btofsb(fs, fs->lfs_sumsize));
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
		if (bpp - sp->bpp > (fs->lfs_sumsize - SEGSUM_SIZE(fs))
		    / sizeof(int32_t)) {
			panic("lfs_writeseg: real bpp overwrite");
		}
		if (bpp - sp->bpp > segsize(fs) / fs->lfs_fsize) {
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
			if (bp->b_bcount > fs->lfs_bsize) {
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
			if (dtosn(fs, dbtofsb(fs, bp->b_blkno +
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
	KASSERT(fs->lfs_magic == LFS_MAGIC);
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
	if (fs->lfs_version == 1)
		fs->lfs_otstamp = time_second;
	fs->lfs_tstamp = time_second;

	/* Checksum the superblock and copy it into a buffer. */
	fs->lfs_cksum = lfs_sb_cksum(&(fs->lfs_dlfs));
	bp = lfs_newbuf(fs, devvp,
	    fsbtodb(fs, daddr), LFS_SBPAD, LFS_NB_SBLOCK);
	memset((char *)bp->b_data + sizeof(struct dlfs), 0,
	    LFS_SBPAD - sizeof(struct dlfs));
	*(struct dlfs *)bp->b_data = fs->lfs_dlfs;

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
	return (lbn < 0 && (-lbn - NDADDR) % NINDIR(fs) == 0);
}

int
lfs_match_dindir(struct lfs *fs, struct buf *bp)
{
	daddr_t lbn;

	ASSERT_SEGLOCK(fs);
	lbn = bp->b_lblkno;
	return (lbn < 0 && (-lbn - NDADDR) % NINDIR(fs) == 1);
}

int
lfs_match_tindir(struct lfs *fs, struct buf *bp)
{
	daddr_t lbn;

	ASSERT_SEGLOCK(fs);
	lbn = bp->b_lblkno;
	return (lbn < 0 && (-lbn - NDADDR) % NINDIR(fs) == 2);
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
		mutex_enter(&lfs_lock);
		mutex_enter(vp->v_interlock);
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
		mutex_exit(vp->v_interlock);
		mutex_exit(&lfs_lock);
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

void
lfs_shellsort(struct buf **bp_array, int32_t *lb_array, int nmemb, int size)
{
	static int __rsshell_increments[] = { 4, 1, 0 };
	int incr, *incrp, t1, t2;
	struct buf *bp_temp;

#ifdef DEBUG
	incr = 0;
	for (t1 = 0; t1 < nmemb; t1++) {
		for (t2 = 0; t2 * size < bp_array[t1]->b_bcount; t2++) {
			if (lb_array[incr++] != bp_array[t1]->b_lblkno + t2) {
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
						printf(" %" PRId32,
						    lb_array[incr++]);
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
				if ((u_int32_t)bp_array[t2]->b_lblkno >
				    (u_int32_t)bp_array[t2 + incr]->b_lblkno) {
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
			lb_array[incr++] = bp_array[t1]->b_lblkno + t2;
		}
	}
}

/*
 * Call vget with LK_NOWAIT.  If we are the one who holds VI_XLOCK,
 * however, we must press on.  Just fake success in that case.
 */
int
lfs_vref(struct vnode *vp)
{
	int error;
	struct lfs *fs;

	KASSERT(mutex_owned(vp->v_interlock));

	fs = VTOI(vp)->i_lfs;

	ASSERT_MAYBE_SEGLOCK(fs);

	/*
	 * If we return 1 here during a flush, we risk vinvalbuf() not
	 * being able to flush all of the pages from this vnode, which
	 * will cause it to panic.  So, return 0 if a flush is in progress.
	 */
	error = vget(vp, LK_NOWAIT);
	if (error == EBUSY && IS_FLUSHING(VTOI(vp)->i_lfs, vp)) {
		++fs->lfs_flushvp_fakevref;
		return 0;
	}
	return error;
}

/*
 * This is vrele except that we do not want to VOP_INACTIVE this vnode. We
 * inline vrele here to avoid the vn_lock and VOP_INACTIVE call at the end.
 */
void
lfs_vunref(struct vnode *vp)
{
	struct lfs *fs;

	fs = VTOI(vp)->i_lfs;
	ASSERT_MAYBE_SEGLOCK(fs);

	/*
	 * Analogous to lfs_vref, if the node is flushing, fake it.
	 */
	if (IS_FLUSHING(fs, vp) && fs->lfs_flushvp_fakevref) {
		--fs->lfs_flushvp_fakevref;
		return;
	}

	/* does not call inactive */
	mutex_enter(vp->v_interlock);
	vrelel(vp, 0);
}

/*
 * We use this when we have vnodes that were loaded in solely for cleaning.
 * There is no reason to believe that these vnodes will be referenced again
 * soon, since the cleaning process is unrelated to normal filesystem
 * activity.  Putting cleaned vnodes at the tail of the list has the effect
 * of flushing the vnode LRU.  So, put vnodes that were loaded only for
 * cleaning at the head of the list, instead.
 */
void
lfs_vunref_head(struct vnode *vp)
{

	ASSERT_SEGLOCK(VTOI(vp)->i_lfs);

	/* does not call inactive, inserts non-held vnode at head of freelist */
	mutex_enter(vp->v_interlock);
	vrelel(vp, 0);
}


/*
 * Set up an FINFO entry for a new file.  The fip pointer is assumed to 
 * point at uninitialized space.
 */
void
lfs_acquire_finfo(struct lfs *fs, ino_t ino, int vers)
{
	struct segment *sp = fs->lfs_sp;

	KASSERT(vers > 0);

	if (sp->seg_bytes_left < fs->lfs_bsize ||
	    sp->sum_bytes_left < sizeof(struct finfo))
		(void) lfs_writeseg(fs, fs->lfs_sp);
	
	sp->sum_bytes_left -= FINFOSIZE;
	++((SEGSUM *)(sp->segsum))->ss_nfinfo;
	sp->fip->fi_nblocks = 0;
	sp->fip->fi_ino = ino;
	sp->fip->fi_version = vers;
}

/*
 * Release the FINFO entry, either clearing out an unused entry or
 * advancing us to the next available entry.
 */
void
lfs_release_finfo(struct lfs *fs)
{
	struct segment *sp = fs->lfs_sp;

	if (sp->fip->fi_nblocks != 0) {
		sp->fip = (FINFO*)((char *)sp->fip + FINFOSIZE +
			sizeof(int32_t) * sp->fip->fi_nblocks);
		sp->start_lbp = &sp->fip->fi_blocks[0];
	} else {
		sp->sum_bytes_left += FINFOSIZE;
		--((SEGSUM *)(sp->segsum))->ss_nfinfo;
	}
}
