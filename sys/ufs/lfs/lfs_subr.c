/*	$NetBSD: lfs_subr.c,v 1.86 2015/10/03 08:28:16 dholland Exp $	*/

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
 *	@(#)lfs_subr.c	8.4 (Berkeley) 5/8/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: lfs_subr.c,v 1.86 2015/10/03 08:28:16 dholland Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/buf.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/kauth.h>

#include <ufs/lfs/ulfs_inode.h>
#include <ufs/lfs/lfs.h>
#include <ufs/lfs/lfs_accessors.h>
#include <ufs/lfs/lfs_kernel.h>
#include <ufs/lfs/lfs_extern.h>

#include <uvm/uvm.h>

#ifdef DEBUG
const char *lfs_res_names[LFS_NB_COUNT] = {
	"summary",
	"superblock",
	"file block",
	"cluster",
	"clean",
	"blkiov",
};
#endif

int lfs_res_qty[LFS_NB_COUNT] = {
	LFS_N_SUMMARIES,
	LFS_N_SBLOCKS,
	LFS_N_IBLOCKS,
	LFS_N_CLUSTERS,
	LFS_N_CLEAN,
	LFS_N_BLKIOV,
};

void
lfs_setup_resblks(struct lfs *fs)
{
	int i, j;
	int maxbpp;

	ASSERT_NO_SEGLOCK(fs);
	fs->lfs_resblk = malloc(LFS_N_TOTAL * sizeof(res_t), M_SEGMENT,
				M_WAITOK);
	for (i = 0; i < LFS_N_TOTAL; i++) {
		fs->lfs_resblk[i].inuse = 0;
		fs->lfs_resblk[i].p = NULL;
	}
	for (i = 0; i < LFS_RESHASH_WIDTH; i++)
		LIST_INIT(fs->lfs_reshash + i);

	/*
	 * These types of allocations can be larger than a page,
	 * so we can't use the pool subsystem for them.
	 */
	for (i = 0, j = 0; j < LFS_N_SUMMARIES; j++, i++)
		fs->lfs_resblk[i].size = lfs_sb_getsumsize(fs);
	for (j = 0; j < LFS_N_SBLOCKS; j++, i++)
		fs->lfs_resblk[i].size = LFS_SBPAD;
	for (j = 0; j < LFS_N_IBLOCKS; j++, i++)
		fs->lfs_resblk[i].size = lfs_sb_getbsize(fs);
	for (j = 0; j < LFS_N_CLUSTERS; j++, i++)
		fs->lfs_resblk[i].size = MAXPHYS;
	for (j = 0; j < LFS_N_CLEAN; j++, i++)
		fs->lfs_resblk[i].size = MAXPHYS;
	for (j = 0; j < LFS_N_BLKIOV; j++, i++)
		fs->lfs_resblk[i].size = LFS_MARKV_MAXBLKCNT * sizeof(BLOCK_INFO);

	for (i = 0; i < LFS_N_TOTAL; i++) {
		fs->lfs_resblk[i].p = malloc(fs->lfs_resblk[i].size,
					     M_SEGMENT, M_WAITOK);
	}

	/*
	 * Initialize pools for small types (XXX is BPP small?)
	 */
	pool_init(&fs->lfs_clpool, sizeof(struct lfs_cluster), 0, 0, 0,
		"lfsclpl", &pool_allocator_nointr, IPL_NONE);
	pool_init(&fs->lfs_segpool, sizeof(struct segment), 0, 0, 0,
		"lfssegpool", &pool_allocator_nointr, IPL_NONE);
	/* XXX: should this int32 be 32/64? */
	maxbpp = ((lfs_sb_getsumsize(fs) - SEGSUM_SIZE(fs)) / sizeof(int32_t) + 2);
	maxbpp = MIN(maxbpp, lfs_segsize(fs) / lfs_sb_getfsize(fs) + 2);
	pool_init(&fs->lfs_bpppool, maxbpp * sizeof(struct buf *), 0, 0, 0,
		"lfsbpppl", &pool_allocator_nointr, IPL_NONE);
}

void
lfs_free_resblks(struct lfs *fs)
{
	int i;

	pool_destroy(&fs->lfs_bpppool);
	pool_destroy(&fs->lfs_segpool);
	pool_destroy(&fs->lfs_clpool);

	mutex_enter(&lfs_lock);
	for (i = 0; i < LFS_N_TOTAL; i++) {
		while (fs->lfs_resblk[i].inuse)
			mtsleep(&fs->lfs_resblk, PRIBIO + 1, "lfs_free", 0,
				&lfs_lock);
		if (fs->lfs_resblk[i].p != NULL)
			free(fs->lfs_resblk[i].p, M_SEGMENT);
	}
	free(fs->lfs_resblk, M_SEGMENT);
	mutex_exit(&lfs_lock);
}

static unsigned int
lfs_mhash(void *vp)
{
	return (unsigned int)(((unsigned long)vp) >> 2) % LFS_RESHASH_WIDTH;
}

/*
 * Return memory of the given size for the given purpose, or use one of a
 * number of spare last-resort buffers, if malloc returns NULL.
 */
void *
lfs_malloc(struct lfs *fs, size_t size, int type)
{
	struct lfs_res_blk *re;
	void *r;
	int i, s, start;
	unsigned int h;

	ASSERT_MAYBE_SEGLOCK(fs);
	r = NULL;

	/* If no mem allocated for this type, it just waits */
	if (lfs_res_qty[type] == 0) {
		r = malloc(size, M_SEGMENT, M_WAITOK);
		return r;
	}

	/* Otherwise try a quick malloc, and if it works, great */
	if ((r = malloc(size, M_SEGMENT, M_NOWAIT)) != NULL) {
		return r;
	}

	/*
	 * If malloc returned NULL, we are forced to use one of our
	 * reserve blocks.  We have on hand at least one summary block,
	 * at least one cluster block, at least one superblock,
	 * and several indirect blocks.
	 */

	mutex_enter(&lfs_lock);
	/* skip over blocks of other types */
	for (i = 0, start = 0; i < type; i++)
		start += lfs_res_qty[i];
	while (r == NULL) {
		for (i = 0; i < lfs_res_qty[type]; i++) {
			if (fs->lfs_resblk[start + i].inuse == 0) {
				re = fs->lfs_resblk + start + i;
				re->inuse = 1;
				r = re->p;
				KASSERT(re->size >= size);
				h = lfs_mhash(r);
				s = splbio();
				LIST_INSERT_HEAD(&fs->lfs_reshash[h], re, res);
				splx(s);
				mutex_exit(&lfs_lock);
				return r;
			}
		}
		DLOG((DLOG_MALLOC, "sleeping on %s (%d)\n",
		      lfs_res_names[type], lfs_res_qty[type]));
		mtsleep(&fs->lfs_resblk, PVM, "lfs_malloc", 0,
			&lfs_lock);
		DLOG((DLOG_MALLOC, "done sleeping on %s\n",
		      lfs_res_names[type]));
	}
	/* NOTREACHED */
	mutex_exit(&lfs_lock);
	return r;
}

void
lfs_free(struct lfs *fs, void *p, int type)
{
	int s;
	unsigned int h;
	res_t *re;
#ifdef DEBUG
	int i;
#endif

	ASSERT_MAYBE_SEGLOCK(fs);
	h = lfs_mhash(p);
	mutex_enter(&lfs_lock);
	s = splbio();
	LIST_FOREACH(re, &fs->lfs_reshash[h], res) {
		if (re->p == p) {
			KASSERT(re->inuse == 1);
			LIST_REMOVE(re, res);
			re->inuse = 0;
			wakeup(&fs->lfs_resblk);
			splx(s);
			mutex_exit(&lfs_lock);
			return;
		}
	}
#ifdef DEBUG
	for (i = 0; i < LFS_N_TOTAL; i++) {
		if (fs->lfs_resblk[i].p == p)
			panic("lfs_free: inconsistent reserved block");
	}
#endif
	splx(s);
	mutex_exit(&lfs_lock);
	
	/*
	 * If we didn't find it, free it.
	 */
	free(p, M_SEGMENT);
}

/*
 * lfs_seglock --
 *	Single thread the segment writer.
 */
int
lfs_seglock(struct lfs *fs, unsigned long flags)
{
	struct segment *sp;

	mutex_enter(&lfs_lock);
	if (fs->lfs_seglock) {
		if (fs->lfs_lockpid == curproc->p_pid &&
		    fs->lfs_locklwp == curlwp->l_lid) {
			++fs->lfs_seglock;
			fs->lfs_sp->seg_flags |= flags;
			mutex_exit(&lfs_lock);
			return 0;
		} else if (flags & SEGM_PAGEDAEMON) {
			mutex_exit(&lfs_lock);
			return EWOULDBLOCK;
		} else {
			while (fs->lfs_seglock) {
				(void)mtsleep(&fs->lfs_seglock, PRIBIO + 1,
					"lfs_seglock", 0, &lfs_lock);
			}
		}
	}

	fs->lfs_seglock = 1;
	fs->lfs_lockpid = curproc->p_pid;
	fs->lfs_locklwp = curlwp->l_lid;
	mutex_exit(&lfs_lock);
	fs->lfs_cleanind = 0;

#ifdef DEBUG
	LFS_ENTER_LOG("seglock", __FILE__, __LINE__, 0, flags, curproc->p_pid);
#endif
	/* Drain fragment size changes out */
	rw_enter(&fs->lfs_fraglock, RW_WRITER);

	sp = fs->lfs_sp = pool_get(&fs->lfs_segpool, PR_WAITOK);
	sp->bpp = pool_get(&fs->lfs_bpppool, PR_WAITOK);
	sp->seg_flags = flags;
	sp->vp = NULL;
	sp->seg_iocount = 0;
	(void) lfs_initseg(fs);

	/*
	 * Keep a cumulative count of the outstanding I/O operations.  If the
	 * disk drive catches up with us it could go to zero before we finish,
	 * so we artificially increment it by one until we've scheduled all of
	 * the writes we intend to do.
	 */
	mutex_enter(&lfs_lock);
	++fs->lfs_iocount;
	fs->lfs_startseg = lfs_sb_getcurseg(fs);
	mutex_exit(&lfs_lock);
	return 0;
}

static void lfs_unmark_dirop(struct lfs *);

static void
lfs_unmark_dirop(struct lfs *fs)
{
	struct inode *ip, *nip;
	struct vnode *vp;
	int doit;

	ASSERT_NO_SEGLOCK(fs);
	mutex_enter(&lfs_lock);
	doit = !(fs->lfs_flags & LFS_UNDIROP);
	if (doit)
		fs->lfs_flags |= LFS_UNDIROP;
	if (!doit) {
		mutex_exit(&lfs_lock);
		return;
	}

	for (ip = TAILQ_FIRST(&fs->lfs_dchainhd); ip != NULL; ip = nip) {
		nip = TAILQ_NEXT(ip, i_lfs_dchain);
		vp = ITOV(ip);
		if ((ip->i_flag & (IN_ADIROP | IN_CDIROP)) == IN_CDIROP) {
			--lfs_dirvcount;
			--fs->lfs_dirvcount;
			vp->v_uflag &= ~VU_DIROP;
			TAILQ_REMOVE(&fs->lfs_dchainhd, ip, i_lfs_dchain);
			wakeup(&lfs_dirvcount);
			fs->lfs_unlockvp = vp;
			mutex_exit(&lfs_lock);
			vrele(vp);
			mutex_enter(&lfs_lock);
			fs->lfs_unlockvp = NULL;
			ip->i_flag &= ~IN_CDIROP;
		}
	}

	fs->lfs_flags &= ~LFS_UNDIROP;
	wakeup(&fs->lfs_flags);
	mutex_exit(&lfs_lock);
}

static void
lfs_auto_segclean(struct lfs *fs)
{
	int i, error, s, waited;

	ASSERT_SEGLOCK(fs);
	/*
	 * Now that we've swapped lfs_activesb, but while we still
	 * hold the segment lock, run through the segment list marking
	 * the empty ones clean.
	 * XXX - do we really need to do them all at once?
	 */
	waited = 0;
	for (i = 0; i < lfs_sb_getnseg(fs); i++) {
		if ((fs->lfs_suflags[0][i] &
		     (SEGUSE_ACTIVE | SEGUSE_DIRTY | SEGUSE_EMPTY)) ==
		    (SEGUSE_DIRTY | SEGUSE_EMPTY) &&
		    (fs->lfs_suflags[1][i] &
		     (SEGUSE_ACTIVE | SEGUSE_DIRTY | SEGUSE_EMPTY)) ==
		    (SEGUSE_DIRTY | SEGUSE_EMPTY)) {

			/* Make sure the sb is written before we clean */
			mutex_enter(&lfs_lock);
			s = splbio();
			while (waited == 0 && fs->lfs_sbactive)
				mtsleep(&fs->lfs_sbactive, PRIBIO+1, "lfs asb",
					0, &lfs_lock);
			splx(s);
			mutex_exit(&lfs_lock);
			waited = 1;

			if ((error = lfs_do_segclean(fs, i)) != 0) {
				DLOG((DLOG_CLEAN, "lfs_auto_segclean: lfs_do_segclean returned %d for seg %d\n", error, i));
			}
		}
		fs->lfs_suflags[1 - fs->lfs_activesb][i] =
			fs->lfs_suflags[fs->lfs_activesb][i];
	}
}

/*
 * lfs_segunlock --
 *	Single thread the segment writer.
 */
void
lfs_segunlock(struct lfs *fs)
{
	struct segment *sp;
	unsigned long sync, ckp;
	struct buf *bp;
	int do_unmark_dirop = 0;

	sp = fs->lfs_sp;

	mutex_enter(&lfs_lock);
	KASSERT(LFS_SEGLOCK_HELD(fs));
	if (fs->lfs_seglock == 1) {
		if ((sp->seg_flags & (SEGM_PROT | SEGM_CLEAN)) == 0)
			do_unmark_dirop = 1;
		mutex_exit(&lfs_lock);
		sync = sp->seg_flags & SEGM_SYNC;
		ckp = sp->seg_flags & SEGM_CKP;

		/* We should have a segment summary, and nothing else */
		KASSERT(sp->cbpp == sp->bpp + 1);

		/* Free allocated segment summary */
		lfs_sb_suboffset(fs, lfs_btofsb(fs, lfs_sb_getsumsize(fs)));
		bp = *sp->bpp;
		lfs_freebuf(fs, bp);

		pool_put(&fs->lfs_bpppool, sp->bpp);
		sp->bpp = NULL;

		/*
		 * If we're not sync, we're done with sp, get rid of it.
		 * Otherwise, we keep a local copy around but free
		 * fs->lfs_sp so another process can use it (we have to
		 * wait but they don't have to wait for us).
		 */
		if (!sync)
			pool_put(&fs->lfs_segpool, sp);
		fs->lfs_sp = NULL;

		/*
		 * If the I/O count is non-zero, sleep until it reaches zero.
		 * At the moment, the user's process hangs around so we can
		 * sleep.
		 */
		mutex_enter(&lfs_lock);
		if (--fs->lfs_iocount == 0) {
			LFS_DEBUG_COUNTLOCKED("lfs_segunlock");
		}
		if (fs->lfs_iocount <= 1)
			wakeup(&fs->lfs_iocount);
		mutex_exit(&lfs_lock);
		/*
		 * If we're not checkpointing, we don't have to block
		 * other processes to wait for a synchronous write
		 * to complete.
		 */
		if (!ckp) {
#ifdef DEBUG
			LFS_ENTER_LOG("segunlock_std", __FILE__, __LINE__, 0, 0, curproc->p_pid);
#endif
			mutex_enter(&lfs_lock);
			--fs->lfs_seglock;
			fs->lfs_lockpid = 0;
			fs->lfs_locklwp = 0;
			mutex_exit(&lfs_lock);
			wakeup(&fs->lfs_seglock);
		}
		/*
		 * We let checkpoints happen asynchronously.  That means
		 * that during recovery, we have to roll forward between
		 * the two segments described by the first and second
		 * superblocks to make sure that the checkpoint described
		 * by a superblock completed.
		 */
		mutex_enter(&lfs_lock);
		while (ckp && sync && fs->lfs_iocount) {
			(void)mtsleep(&fs->lfs_iocount, PRIBIO + 1,
				      "lfs_iocount", 0, &lfs_lock);
			DLOG((DLOG_SEG, "sleeping on iocount %x == %d\n", fs, fs->lfs_iocount));
		}
		while (sync && sp->seg_iocount) {
			(void)mtsleep(&sp->seg_iocount, PRIBIO + 1,
				     "seg_iocount", 0, &lfs_lock);
			DLOG((DLOG_SEG, "sleeping on iocount %x == %d\n", sp, sp->seg_iocount));
		}
		mutex_exit(&lfs_lock);
		if (sync)
			pool_put(&fs->lfs_segpool, sp);

		if (ckp) {
			fs->lfs_nactive = 0;
			/* If we *know* everything's on disk, write both sbs */
			/* XXX should wait for this one	 */
			if (sync)
				lfs_writesuper(fs, lfs_sb_getsboff(fs, fs->lfs_activesb));
			lfs_writesuper(fs, lfs_sb_getsboff(fs, 1 - fs->lfs_activesb));
			if (!(fs->lfs_ivnode->v_mount->mnt_iflag & IMNT_UNMOUNT)) {
				lfs_auto_segclean(fs);
				/* If sync, we can clean the remainder too */
				if (sync)
					lfs_auto_segclean(fs);
			}
			fs->lfs_activesb = 1 - fs->lfs_activesb;
#ifdef DEBUG
			LFS_ENTER_LOG("segunlock_ckp", __FILE__, __LINE__, 0, 0, curproc->p_pid);
#endif
			mutex_enter(&lfs_lock);
			--fs->lfs_seglock;
			fs->lfs_lockpid = 0;
			fs->lfs_locklwp = 0;
			mutex_exit(&lfs_lock);
			wakeup(&fs->lfs_seglock);
		}
		/* Reenable fragment size changes */
		rw_exit(&fs->lfs_fraglock);
		if (do_unmark_dirop)
			lfs_unmark_dirop(fs);
	} else if (fs->lfs_seglock == 0) {
		mutex_exit(&lfs_lock);
		panic ("Seglock not held");
	} else {
		--fs->lfs_seglock;
		mutex_exit(&lfs_lock);
	}
}

/*
 * Drain dirops and start writer.
 *
 * No simple_locks are held when we enter and none are held when we return.
 */
int
lfs_writer_enter(struct lfs *fs, const char *wmesg)
{
	int error = 0;

	ASSERT_MAYBE_SEGLOCK(fs);
	mutex_enter(&lfs_lock);

	/* disallow dirops during flush */
	fs->lfs_writer++;

	while (fs->lfs_dirops > 0) {
		++fs->lfs_diropwait;
		error = mtsleep(&fs->lfs_writer, PRIBIO+1, wmesg, 0,
				&lfs_lock);
		--fs->lfs_diropwait;
	}

	if (error)
		fs->lfs_writer--;

	mutex_exit(&lfs_lock);

	return error;
}

void
lfs_writer_leave(struct lfs *fs)
{
	bool dowakeup;

	ASSERT_MAYBE_SEGLOCK(fs);
	mutex_enter(&lfs_lock);
	dowakeup = !(--fs->lfs_writer);
	mutex_exit(&lfs_lock);
	if (dowakeup)
		wakeup(&fs->lfs_dirops);
}

/*
 * Unlock, wait for the cleaner, then relock to where we were before.
 * To be used only at a fairly high level, to address a paucity of free
 * segments propagated back from lfs_gop_write().
 */
void
lfs_segunlock_relock(struct lfs *fs)
{
	int n = fs->lfs_seglock;
	u_int16_t seg_flags;
	CLEANERINFO *cip;
	struct buf *bp;

	if (n == 0)
		return;

	/* Write anything we've already gathered to disk */
	lfs_writeseg(fs, fs->lfs_sp);

	/* Tell cleaner */
	LFS_CLEANERINFO(cip, fs, bp);
	lfs_ci_setflags(fs, cip,
			lfs_ci_getflags(fs, cip) | LFS_CLEANER_MUST_CLEAN);
	LFS_SYNC_CLEANERINFO(cip, fs, bp, 1);

	/* Save segment flags for later */
	seg_flags = fs->lfs_sp->seg_flags;

	fs->lfs_sp->seg_flags |= SEGM_PROT; /* Don't unmark dirop nodes */
	while(fs->lfs_seglock)
		lfs_segunlock(fs);

	/* Wait for the cleaner */
	lfs_wakeup_cleaner(fs);
	mutex_enter(&lfs_lock);
	while (LFS_STARVED_FOR_SEGS(fs))
		mtsleep(&fs->lfs_availsleep, PRIBIO, "relock", 0,
			&lfs_lock);
	mutex_exit(&lfs_lock);

	/* Put the segment lock back the way it was. */
	while(n--)
		lfs_seglock(fs, seg_flags);

	/* Cleaner can relax now */
	LFS_CLEANERINFO(cip, fs, bp);
	lfs_ci_setflags(fs, cip,
			lfs_ci_getflags(fs, cip) & ~LFS_CLEANER_MUST_CLEAN);
	LFS_SYNC_CLEANERINFO(cip, fs, bp, 1);

	return;
}

/*
 * Wake up the cleaner, provided that nowrap is not set.
 */
void
lfs_wakeup_cleaner(struct lfs *fs)
{
	if (fs->lfs_nowrap > 0)
		return;

	wakeup(&fs->lfs_nextsegsleep);
	wakeup(&lfs_allclean_wakeup);
}
