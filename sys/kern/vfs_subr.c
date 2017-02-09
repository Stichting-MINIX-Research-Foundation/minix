/*	$NetBSD: vfs_subr.c,v 1.448 2015/08/24 22:50:32 pooka Exp $	*/

/*-
 * Copyright (c) 1997, 1998, 2004, 2005, 2007, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center, by Charles M. Hannum, by Andrew Doran,
 * by Marshall Kirk McKusick and Greg Ganger at the University of Michigan.
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
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)vfs_subr.c	8.13 (Berkeley) 4/18/94
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: vfs_subr.c,v 1.448 2015/08/24 22:50:32 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_ddb.h"
#include "opt_compat_netbsd.h"
#include "opt_compat_43.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/dirent.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/namei.h>
#include <sys/buf.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/syscallargs.h>
#include <sys/kauth.h>
#include <sys/module.h>

#include <miscfs/genfs/genfs.h>
#include <miscfs/specfs/specdev.h>
#include <uvm/uvm_ddb.h>

const enum vtype iftovt_tab[16] = {
	VNON, VFIFO, VCHR, VNON, VDIR, VNON, VBLK, VNON,
	VREG, VNON, VLNK, VNON, VSOCK, VNON, VNON, VBAD,
};
const int	vttoif_tab[9] = {
	0, S_IFREG, S_IFDIR, S_IFBLK, S_IFCHR, S_IFLNK,
	S_IFSOCK, S_IFIFO, S_IFMT,
};

/*
 * Insq/Remq for the vnode usage lists.
 */
#define	bufinsvn(bp, dp)	LIST_INSERT_HEAD(dp, bp, b_vnbufs)
#define	bufremvn(bp) {							\
	LIST_REMOVE(bp, b_vnbufs);					\
	(bp)->b_vnbufs.le_next = NOLIST;				\
}

int doforce = 1;		/* 1 => permit forcible unmounting */
int prtactive = 0;		/* 1 => print out reclaim of active vnodes */

extern struct mount *dead_rootmount;

/*
 * Local declarations.
 */

static void vn_initialize_syncerd(void);

/*
 * Initialize the vnode management data structures.
 */
void
vntblinit(void)
{

	vn_initialize_syncerd();
	vfs_mount_sysinit();
	vfs_vnode_sysinit();
}

/*
 * Flush out and invalidate all buffers associated with a vnode.
 * Called with the underlying vnode locked, which should prevent new dirty
 * buffers from being queued.
 */
int
vinvalbuf(struct vnode *vp, int flags, kauth_cred_t cred, struct lwp *l,
	  bool catch_p, int slptimeo)
{
	struct buf *bp, *nbp;
	int error;
	int flushflags = PGO_ALLPAGES | PGO_FREE | PGO_SYNCIO |
	    (flags & V_SAVE ? PGO_CLEANIT | PGO_RECLAIM : 0);

	/* XXXUBC this doesn't look at flags or slp* */
	mutex_enter(vp->v_interlock);
	error = VOP_PUTPAGES(vp, 0, 0, flushflags);
	if (error) {
		return error;
	}

	if (flags & V_SAVE) {
		error = VOP_FSYNC(vp, cred, FSYNC_WAIT|FSYNC_RECLAIM, 0, 0);
		if (error)
		        return (error);
		KASSERT(LIST_EMPTY(&vp->v_dirtyblkhd));
	}

	mutex_enter(&bufcache_lock);
restart:
	for (bp = LIST_FIRST(&vp->v_dirtyblkhd); bp; bp = nbp) {
		KASSERT(bp->b_vp == vp);
		nbp = LIST_NEXT(bp, b_vnbufs);
		error = bbusy(bp, catch_p, slptimeo, NULL);
		if (error != 0) {
			if (error == EPASSTHROUGH)
				goto restart;
			mutex_exit(&bufcache_lock);
			return (error);
		}
		brelsel(bp, BC_INVAL | BC_VFLUSH);
	}

	for (bp = LIST_FIRST(&vp->v_cleanblkhd); bp; bp = nbp) {
		KASSERT(bp->b_vp == vp);
		nbp = LIST_NEXT(bp, b_vnbufs);
		error = bbusy(bp, catch_p, slptimeo, NULL);
		if (error != 0) {
			if (error == EPASSTHROUGH)
				goto restart;
			mutex_exit(&bufcache_lock);
			return (error);
		}
		/*
		 * XXX Since there are no node locks for NFS, I believe
		 * there is a slight chance that a delayed write will
		 * occur while sleeping just above, so check for it.
		 */
		if ((bp->b_oflags & BO_DELWRI) && (flags & V_SAVE)) {
#ifdef DEBUG
			printf("buffer still DELWRI\n");
#endif
			bp->b_cflags |= BC_BUSY | BC_VFLUSH;
			mutex_exit(&bufcache_lock);
			VOP_BWRITE(bp->b_vp, bp);
			mutex_enter(&bufcache_lock);
			goto restart;
		}
		brelsel(bp, BC_INVAL | BC_VFLUSH);
	}

#ifdef DIAGNOSTIC
	if (!LIST_EMPTY(&vp->v_cleanblkhd) || !LIST_EMPTY(&vp->v_dirtyblkhd))
		panic("vinvalbuf: flush failed, vp %p", vp);
#endif

	mutex_exit(&bufcache_lock);

	return (0);
}

/*
 * Destroy any in core blocks past the truncation length.
 * Called with the underlying vnode locked, which should prevent new dirty
 * buffers from being queued.
 */
int
vtruncbuf(struct vnode *vp, daddr_t lbn, bool catch_p, int slptimeo)
{
	struct buf *bp, *nbp;
	int error;
	voff_t off;

	off = round_page((voff_t)lbn << vp->v_mount->mnt_fs_bshift);
	mutex_enter(vp->v_interlock);
	error = VOP_PUTPAGES(vp, off, 0, PGO_FREE | PGO_SYNCIO);
	if (error) {
		return error;
	}

	mutex_enter(&bufcache_lock);
restart:
	for (bp = LIST_FIRST(&vp->v_dirtyblkhd); bp; bp = nbp) {
		KASSERT(bp->b_vp == vp);
		nbp = LIST_NEXT(bp, b_vnbufs);
		if (bp->b_lblkno < lbn)
			continue;
		error = bbusy(bp, catch_p, slptimeo, NULL);
		if (error != 0) {
			if (error == EPASSTHROUGH)
				goto restart;
			mutex_exit(&bufcache_lock);
			return (error);
		}
		brelsel(bp, BC_INVAL | BC_VFLUSH);
	}

	for (bp = LIST_FIRST(&vp->v_cleanblkhd); bp; bp = nbp) {
		KASSERT(bp->b_vp == vp);
		nbp = LIST_NEXT(bp, b_vnbufs);
		if (bp->b_lblkno < lbn)
			continue;
		error = bbusy(bp, catch_p, slptimeo, NULL);
		if (error != 0) {
			if (error == EPASSTHROUGH)
				goto restart;
			mutex_exit(&bufcache_lock);
			return (error);
		}
		brelsel(bp, BC_INVAL | BC_VFLUSH);
	}
	mutex_exit(&bufcache_lock);

	return (0);
}

/*
 * Flush all dirty buffers from a vnode.
 * Called with the underlying vnode locked, which should prevent new dirty
 * buffers from being queued.
 */
int
vflushbuf(struct vnode *vp, int flags)
{
	struct buf *bp, *nbp;
	int error, pflags;
	bool dirty, sync;

	sync = (flags & FSYNC_WAIT) != 0;
	pflags = PGO_CLEANIT | PGO_ALLPAGES |
		(sync ? PGO_SYNCIO : 0) |
		((flags & FSYNC_LAZY) ? PGO_LAZY : 0);
	mutex_enter(vp->v_interlock);
	(void) VOP_PUTPAGES(vp, 0, 0, pflags);

loop:
	mutex_enter(&bufcache_lock);
	for (bp = LIST_FIRST(&vp->v_dirtyblkhd); bp; bp = nbp) {
		KASSERT(bp->b_vp == vp);
		nbp = LIST_NEXT(bp, b_vnbufs);
		if ((bp->b_cflags & BC_BUSY))
			continue;
		if ((bp->b_oflags & BO_DELWRI) == 0)
			panic("vflushbuf: not dirty, bp %p", bp);
		bp->b_cflags |= BC_BUSY | BC_VFLUSH;
		mutex_exit(&bufcache_lock);
		/*
		 * Wait for I/O associated with indirect blocks to complete,
		 * since there is no way to quickly wait for them below.
		 */
		if (bp->b_vp == vp || !sync)
			(void) bawrite(bp);
		else {
			error = bwrite(bp);
			if (error)
				return error;
		}
		goto loop;
	}
	mutex_exit(&bufcache_lock);

	if (!sync)
		return 0;

	mutex_enter(vp->v_interlock);
	while (vp->v_numoutput != 0)
		cv_wait(&vp->v_cv, vp->v_interlock);
	dirty = !LIST_EMPTY(&vp->v_dirtyblkhd);
	mutex_exit(vp->v_interlock);

	if (dirty) {
		vprint("vflushbuf: dirty", vp);
		goto loop;
	}

	return 0;
}

/*
 * Create a vnode for a block device.
 * Used for root filesystem and swap areas.
 * Also used for memory file system special devices.
 */
int
bdevvp(dev_t dev, vnode_t **vpp)
{
	struct vattr va;

	vattr_null(&va);
	va.va_type = VBLK;
	va.va_rdev = dev;

	return vcache_new(dead_rootmount, NULL, &va, NOCRED, vpp);
}

/*
 * Create a vnode for a character device.
 * Used for kernfs and some console handling.
 */
int
cdevvp(dev_t dev, vnode_t **vpp)
{
	struct vattr va;

	vattr_null(&va);
	va.va_type = VCHR;
	va.va_rdev = dev;

	return vcache_new(dead_rootmount, NULL, &va, NOCRED, vpp);
}

/*
 * Associate a buffer with a vnode.  There must already be a hold on
 * the vnode.
 */
void
bgetvp(struct vnode *vp, struct buf *bp)
{

	KASSERT(bp->b_vp == NULL);
	KASSERT(bp->b_objlock == &buffer_lock);
	KASSERT(mutex_owned(vp->v_interlock));
	KASSERT(mutex_owned(&bufcache_lock));
	KASSERT((bp->b_cflags & BC_BUSY) != 0);
	KASSERT(!cv_has_waiters(&bp->b_done));

	vholdl(vp);
	bp->b_vp = vp;
	if (vp->v_type == VBLK || vp->v_type == VCHR)
		bp->b_dev = vp->v_rdev;
	else
		bp->b_dev = NODEV;

	/*
	 * Insert onto list for new vnode.
	 */
	bufinsvn(bp, &vp->v_cleanblkhd);
	bp->b_objlock = vp->v_interlock;
}

/*
 * Disassociate a buffer from a vnode.
 */
void
brelvp(struct buf *bp)
{
	struct vnode *vp = bp->b_vp;

	KASSERT(vp != NULL);
	KASSERT(bp->b_objlock == vp->v_interlock);
	KASSERT(mutex_owned(vp->v_interlock));
	KASSERT(mutex_owned(&bufcache_lock));
	KASSERT((bp->b_cflags & BC_BUSY) != 0);
	KASSERT(!cv_has_waiters(&bp->b_done));

	/*
	 * Delete from old vnode list, if on one.
	 */
	if (LIST_NEXT(bp, b_vnbufs) != NOLIST)
		bufremvn(bp);

	if (vp->v_uobj.uo_npages == 0 && (vp->v_iflag & VI_ONWORKLST) &&
	    LIST_FIRST(&vp->v_dirtyblkhd) == NULL) {
		vp->v_iflag &= ~VI_WRMAPDIRTY;
		vn_syncer_remove_from_worklist(vp);
	}

	bp->b_objlock = &buffer_lock;
	bp->b_vp = NULL;
	holdrelel(vp);
}

/*
 * Reassign a buffer from one vnode list to another.
 * The list reassignment must be within the same vnode.
 * Used to assign file specific control information
 * (indirect blocks) to the list to which they belong.
 */
void
reassignbuf(struct buf *bp, struct vnode *vp)
{
	struct buflists *listheadp;
	int delayx;

	KASSERT(mutex_owned(&bufcache_lock));
	KASSERT(bp->b_objlock == vp->v_interlock);
	KASSERT(mutex_owned(vp->v_interlock));
	KASSERT((bp->b_cflags & BC_BUSY) != 0);

	/*
	 * Delete from old vnode list, if on one.
	 */
	if (LIST_NEXT(bp, b_vnbufs) != NOLIST)
		bufremvn(bp);

	/*
	 * If dirty, put on list of dirty buffers;
	 * otherwise insert onto list of clean buffers.
	 */
	if ((bp->b_oflags & BO_DELWRI) == 0) {
		listheadp = &vp->v_cleanblkhd;
		if (vp->v_uobj.uo_npages == 0 &&
		    (vp->v_iflag & VI_ONWORKLST) &&
		    LIST_FIRST(&vp->v_dirtyblkhd) == NULL) {
			vp->v_iflag &= ~VI_WRMAPDIRTY;
			vn_syncer_remove_from_worklist(vp);
		}
	} else {
		listheadp = &vp->v_dirtyblkhd;
		if ((vp->v_iflag & VI_ONWORKLST) == 0) {
			switch (vp->v_type) {
			case VDIR:
				delayx = dirdelay;
				break;
			case VBLK:
				if (spec_node_getmountedfs(vp) != NULL) {
					delayx = metadelay;
					break;
				}
				/* fall through */
			default:
				delayx = filedelay;
				break;
			}
			if (!vp->v_mount ||
			    (vp->v_mount->mnt_flag & MNT_ASYNC) == 0)
				vn_syncer_add_to_worklist(vp, delayx);
		}
	}
	bufinsvn(bp, listheadp);
}

/*
 * Lookup a vnode by device number and return it referenced.
 */
int
vfinddev(dev_t dev, enum vtype type, vnode_t **vpp)
{

	return (spec_node_lookup_by_dev(type, dev, vpp) == 0);
}

/*
 * Revoke all the vnodes corresponding to the specified minor number
 * range (endpoints inclusive) of the specified major.
 */
void
vdevgone(int maj, int minl, int minh, enum vtype type)
{
	vnode_t *vp;
	dev_t dev;
	int mn;

	for (mn = minl; mn <= minh; mn++) {
		dev = makedev(maj, mn);
		while (spec_node_lookup_by_dev(type, dev, &vp) == 0) {
			VOP_REVOKE(vp, REVOKEALL);
			vrele(vp);
		}
	}
}

/*
 * The filesystem synchronizer mechanism - syncer.
 *
 * It is useful to delay writes of file data and filesystem metadata for
 * a certain amount of time so that quickly created and deleted files need
 * not waste disk bandwidth being created and removed.  To implement this,
 * vnodes are appended to a "workitem" queue.
 *
 * Most pending metadata should not wait for more than ten seconds.  Thus,
 * mounted on block devices are delayed only about a half the time that file
 * data is delayed.  Similarly, directory updates are more critical, so are
 * only delayed about a third the time that file data is delayed.
 *
 * There are SYNCER_MAXDELAY queues that are processed in a round-robin
 * manner at a rate of one each second (driven off the filesystem syner
 * thread). The syncer_delayno variable indicates the next queue that is
 * to be processed.  Items that need to be processed soon are placed in
 * this queue:
 *
 *	syncer_workitem_pending[syncer_delayno]
 *
 * A delay of e.g. fifteen seconds is done by placing the request fifteen
 * entries later in the queue:
 *
 *	syncer_workitem_pending[(syncer_delayno + 15) & syncer_mask]
 *
 * Flag VI_ONWORKLST indicates that vnode is added into the queue.
 */

#define SYNCER_MAXDELAY		32

typedef TAILQ_HEAD(synclist, vnode) synclist_t;

static void	vn_syncer_add1(struct vnode *, int);
static void	sysctl_vfs_syncfs_setup(struct sysctllog **);

/*
 * Defines and variables for the syncer process.
 */
int syncer_maxdelay = SYNCER_MAXDELAY;	/* maximum delay time */
time_t syncdelay = 30;			/* max time to delay syncing data */
time_t filedelay = 30;			/* time to delay syncing files */
time_t dirdelay  = 15;			/* time to delay syncing directories */
time_t metadelay = 10;			/* time to delay syncing metadata */
time_t lockdelay = 1;			/* time to delay if locking fails */

kmutex_t		syncer_mutex;	/* used to freeze syncer, long term */
static kmutex_t		syncer_data_lock; /* short term lock on data structs */

static int		syncer_delayno = 0;
static long		syncer_last;
static synclist_t *	syncer_workitem_pending;

static void
vn_initialize_syncerd(void)
{
	int i;

	syncer_last = SYNCER_MAXDELAY + 2;

	sysctl_vfs_syncfs_setup(NULL);

	syncer_workitem_pending =
	    kmem_alloc(syncer_last * sizeof (struct synclist), KM_SLEEP);

	for (i = 0; i < syncer_last; i++)
		TAILQ_INIT(&syncer_workitem_pending[i]);

	mutex_init(&syncer_mutex, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&syncer_data_lock, MUTEX_DEFAULT, IPL_NONE);
}

/*
 * Return delay factor appropriate for the given file system.   For
 * WAPBL we use the sync vnode to burst out metadata updates: sync
 * those file systems more frequently.
 */
static inline int
sync_delay(struct mount *mp)
{

	return mp->mnt_wapbl != NULL ? metadelay : syncdelay;
}

/*
 * Compute the next slot index from delay.
 */
static inline int
sync_delay_slot(int delayx)
{

	if (delayx > syncer_maxdelay - 2)
		delayx = syncer_maxdelay - 2;
	return (syncer_delayno + delayx) % syncer_last;
}

/*
 * Add an item to the syncer work queue.
 */
static void
vn_syncer_add1(struct vnode *vp, int delayx)
{
	synclist_t *slp;

	KASSERT(mutex_owned(&syncer_data_lock));

	if (vp->v_iflag & VI_ONWORKLST) {
		/*
		 * Remove in order to adjust the position of the vnode.
		 * Note: called from sched_sync(), which will not hold
		 * interlock, therefore we cannot modify v_iflag here.
		 */
		slp = &syncer_workitem_pending[vp->v_synclist_slot];
		TAILQ_REMOVE(slp, vp, v_synclist);
	} else {
		KASSERT(mutex_owned(vp->v_interlock));
		vp->v_iflag |= VI_ONWORKLST;
	}

	vp->v_synclist_slot = sync_delay_slot(delayx);

	slp = &syncer_workitem_pending[vp->v_synclist_slot];
	TAILQ_INSERT_TAIL(slp, vp, v_synclist);
}

void
vn_syncer_add_to_worklist(struct vnode *vp, int delayx)
{

	KASSERT(mutex_owned(vp->v_interlock));

	mutex_enter(&syncer_data_lock);
	vn_syncer_add1(vp, delayx);
	mutex_exit(&syncer_data_lock);
}

/*
 * Remove an item from the syncer work queue.
 */
void
vn_syncer_remove_from_worklist(struct vnode *vp)
{
	synclist_t *slp;

	KASSERT(mutex_owned(vp->v_interlock));

	mutex_enter(&syncer_data_lock);
	if (vp->v_iflag & VI_ONWORKLST) {
		vp->v_iflag &= ~VI_ONWORKLST;
		slp = &syncer_workitem_pending[vp->v_synclist_slot];
		TAILQ_REMOVE(slp, vp, v_synclist);
	}
	mutex_exit(&syncer_data_lock);
}

/*
 * Add this mount point to the syncer.
 */
void
vfs_syncer_add_to_worklist(struct mount *mp)
{
	static int start, incr, next;
	int vdelay;

	KASSERT(mutex_owned(&mp->mnt_updating));
	KASSERT((mp->mnt_iflag & IMNT_ONWORKLIST) == 0);

	/*
	 * We attempt to scatter the mount points on the list
	 * so that they will go off at evenly distributed times
	 * even if all the filesystems are mounted at once.
	 */

	next += incr;
	if (next == 0 || next > syncer_maxdelay) {
		start /= 2;
		incr /= 2;
		if (start == 0) {
			start = syncer_maxdelay / 2;
			incr = syncer_maxdelay;
		}
		next = start;
	}
	mp->mnt_iflag |= IMNT_ONWORKLIST;
	vdelay = sync_delay(mp);
	mp->mnt_synclist_slot = vdelay > 0 ? next % vdelay : 0;
}

/*
 * Remove the mount point from the syncer.
 */
void
vfs_syncer_remove_from_worklist(struct mount *mp)
{

	KASSERT(mutex_owned(&mp->mnt_updating));
	KASSERT((mp->mnt_iflag & IMNT_ONWORKLIST) != 0);

	mp->mnt_iflag &= ~IMNT_ONWORKLIST;
}

/*
 * Try lazy sync, return true on success.
 */
static bool
lazy_sync_vnode(struct vnode *vp)
{
	bool synced;

	KASSERT(mutex_owned(&syncer_data_lock));

	synced = false;
	/* We are locking in the wrong direction. */
	if (mutex_tryenter(vp->v_interlock)) {
		mutex_exit(&syncer_data_lock);
		if (vget(vp, LK_NOWAIT, false /* !wait */) == 0) {
			if (vn_lock(vp, LK_EXCLUSIVE | LK_NOWAIT) == 0) {
				synced = true;
				(void) VOP_FSYNC(vp, curlwp->l_cred,
				    FSYNC_LAZY, 0, 0);
				vput(vp);
			} else
				vrele(vp);
		}
		mutex_enter(&syncer_data_lock);
	}
	return synced;
}

/*
 * System filesystem synchronizer daemon.
 */
void
sched_sync(void *arg)
{
	synclist_t *slp;
	struct vnode *vp;
	struct mount *mp, *nmp;
	time_t starttime;
	bool synced;

	for (;;) {
		mutex_enter(&syncer_mutex);

		starttime = time_second;

		/*
		 * Sync mounts whose dirty time has expired.
		 */
		mutex_enter(&mountlist_lock);
		for (mp = TAILQ_FIRST(&mountlist); mp != NULL; mp = nmp) {
			if ((mp->mnt_iflag & IMNT_ONWORKLIST) == 0 ||
			    mp->mnt_synclist_slot != syncer_delayno) {
				nmp = TAILQ_NEXT(mp, mnt_list);
				continue;
			}
			mp->mnt_synclist_slot = sync_delay_slot(sync_delay(mp));
			if (vfs_busy(mp, &nmp))
				continue;
			VFS_SYNC(mp, MNT_LAZY, curlwp->l_cred);
			vfs_unbusy(mp, false, &nmp);
		}
		mutex_exit(&mountlist_lock);

		mutex_enter(&syncer_data_lock);

		/*
		 * Push files whose dirty time has expired.
		 */
		slp = &syncer_workitem_pending[syncer_delayno];
		syncer_delayno += 1;
		if (syncer_delayno >= syncer_last)
			syncer_delayno = 0;

		while ((vp = TAILQ_FIRST(slp)) != NULL) {
			synced = lazy_sync_vnode(vp);

			/*
			 * XXX The vnode may have been recycled, in which
			 * case it may have a new identity.
			 */
			if (TAILQ_FIRST(slp) == vp) {
				/*
				 * Put us back on the worklist.  The worklist
				 * routine will remove us from our current
				 * position and then add us back in at a later
				 * position.
				 *
				 * Try again sooner rather than later if
				 * we were unable to lock the vnode.  Lock
				 * failure should not prevent us from doing
				 * the sync "soon".
				 *
				 * If we locked it yet arrive here, it's
				 * likely that lazy sync is in progress and
				 * so the vnode still has dirty metadata. 
				 * syncdelay is mainly to get this vnode out
				 * of the way so we do not consider it again
				 * "soon" in this loop, so the delay time is
				 * not critical as long as it is not "soon". 
				 * While write-back strategy is the file
				 * system's domain, we expect write-back to
				 * occur no later than syncdelay seconds
				 * into the future.
				 */
				vn_syncer_add1(vp,
				    synced ? syncdelay : lockdelay);
			}
		}
		mutex_exit(&syncer_mutex);

		/*
		 * If it has taken us less than a second to process the
		 * current work, then wait.  Otherwise start right over
		 * again.  We can still lose time if any single round
		 * takes more than two seconds, but it does not really
		 * matter as we are just trying to generally pace the
		 * filesystem activity.
		 */
		if (time_second == starttime) {
			kpause("syncer", false, hz, &syncer_data_lock);
		}
		mutex_exit(&syncer_data_lock);
	}
}

static void
sysctl_vfs_syncfs_setup(struct sysctllog **clog)
{
	const struct sysctlnode *rnode, *cnode;

	sysctl_createv(clog, 0, NULL, &rnode,
			CTLFLAG_PERMANENT,
			CTLTYPE_NODE, "sync",
			SYSCTL_DESCR("syncer options"),
			NULL, 0, NULL, 0,
			CTL_VFS, CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, &rnode, &cnode,
			CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
			CTLTYPE_QUAD, "delay",
			SYSCTL_DESCR("max time to delay syncing data"),
			NULL, 0, &syncdelay, 0,
			CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, &rnode, &cnode,
			CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
			CTLTYPE_QUAD, "filedelay",
			SYSCTL_DESCR("time to delay syncing files"),
			NULL, 0, &filedelay, 0,
			CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, &rnode, &cnode,
			CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
			CTLTYPE_QUAD, "dirdelay",
			SYSCTL_DESCR("time to delay syncing directories"),
			NULL, 0, &dirdelay, 0,
			CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, &rnode, &cnode,
			CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
			CTLTYPE_QUAD, "metadelay",
			SYSCTL_DESCR("time to delay syncing metadata"),
			NULL, 0, &metadelay, 0,
			CTL_CREATE, CTL_EOL);
}

/*
 * sysctl helper routine to return list of supported fstypes
 */
int
sysctl_vfs_generic_fstypes(SYSCTLFN_ARGS)
{
	char bf[sizeof(((struct statvfs *)NULL)->f_fstypename)];
	char *where = oldp;
	struct vfsops *v;
	size_t needed, left, slen;
	int error, first;

	if (newp != NULL)
		return (EPERM);
	if (namelen != 0)
		return (EINVAL);

	first = 1;
	error = 0;
	needed = 0;
	left = *oldlenp;

	sysctl_unlock();
	mutex_enter(&vfs_list_lock);
	LIST_FOREACH(v, &vfs_list, vfs_list) {
		if (where == NULL)
			needed += strlen(v->vfs_name) + 1;
		else {
			memset(bf, 0, sizeof(bf));
			if (first) {
				strncpy(bf, v->vfs_name, sizeof(bf));
				first = 0;
			} else {
				bf[0] = ' ';
				strncpy(bf + 1, v->vfs_name, sizeof(bf) - 1);
			}
			bf[sizeof(bf)-1] = '\0';
			slen = strlen(bf);
			if (left < slen + 1)
				break;
			v->vfs_refcount++;
			mutex_exit(&vfs_list_lock);
			/* +1 to copy out the trailing NUL byte */
			error = copyout(bf, where, slen + 1);
			mutex_enter(&vfs_list_lock);
			v->vfs_refcount--;
			if (error)
				break;
			where += slen;
			needed += slen;
			left -= slen;
		}
	}
	mutex_exit(&vfs_list_lock);
	sysctl_relock();
	*oldlenp = needed;
	return (error);
}

int kinfo_vdebug = 1;
int kinfo_vgetfailed;

#define KINFO_VNODESLOP	10

/*
 * Dump vnode list (via sysctl).
 * Copyout address of vnode followed by vnode.
 */
int
sysctl_kern_vnode(SYSCTLFN_ARGS)
{
	char *where = oldp;
	size_t *sizep = oldlenp;
	struct mount *mp, *nmp;
	vnode_t *vp, vbuf;
	struct vnode_iterator *marker;
	char *bp = where;
	char *ewhere;
	int error;

	if (namelen != 0)
		return (EOPNOTSUPP);
	if (newp != NULL)
		return (EPERM);

#define VPTRSZ	sizeof(vnode_t *)
#define VNODESZ	sizeof(vnode_t)
	if (where == NULL) {
		*sizep = (numvnodes + KINFO_VNODESLOP) * (VPTRSZ + VNODESZ);
		return (0);
	}
	ewhere = where + *sizep;

	sysctl_unlock();
	mutex_enter(&mountlist_lock);
	for (mp = TAILQ_FIRST(&mountlist); mp != NULL; mp = nmp) {
		if (vfs_busy(mp, &nmp)) {
			continue;
		}
		vfs_vnode_iterator_init(mp, &marker);
		while ((vp = vfs_vnode_iterator_next(marker, NULL, NULL))) {
			if (bp + VPTRSZ + VNODESZ > ewhere) {
				vrele(vp);
				vfs_vnode_iterator_destroy(marker);
				vfs_unbusy(mp, false, NULL);
				sysctl_relock();
				*sizep = bp - where;
				return (ENOMEM);
			}
			memcpy(&vbuf, vp, VNODESZ);
			if ((error = copyout(&vp, bp, VPTRSZ)) ||
			    (error = copyout(&vbuf, bp + VPTRSZ, VNODESZ))) {
				vrele(vp);
				vfs_vnode_iterator_destroy(marker);
				vfs_unbusy(mp, false, NULL);
				sysctl_relock();
				return (error);
			}
			vrele(vp);
			bp += VPTRSZ + VNODESZ;
		}
		vfs_vnode_iterator_destroy(marker);
		vfs_unbusy(mp, false, &nmp);
	}
	mutex_exit(&mountlist_lock);
	sysctl_relock();

	*sizep = bp - where;
	return (0);
}

/*
 * Set vnode attributes to VNOVAL
 */
void
vattr_null(struct vattr *vap)
{

	memset(vap, 0, sizeof(*vap));

	vap->va_type = VNON;

	/*
	 * Assign individually so that it is safe even if size and
	 * sign of each member are varied.
	 */
	vap->va_mode = VNOVAL;
	vap->va_nlink = VNOVAL;
	vap->va_uid = VNOVAL;
	vap->va_gid = VNOVAL;
	vap->va_fsid = VNOVAL;
	vap->va_fileid = VNOVAL;
	vap->va_size = VNOVAL;
	vap->va_blocksize = VNOVAL;
	vap->va_atime.tv_sec =
	    vap->va_mtime.tv_sec =
	    vap->va_ctime.tv_sec =
	    vap->va_birthtime.tv_sec = VNOVAL;
	vap->va_atime.tv_nsec =
	    vap->va_mtime.tv_nsec =
	    vap->va_ctime.tv_nsec =
	    vap->va_birthtime.tv_nsec = VNOVAL;
	vap->va_gen = VNOVAL;
	vap->va_flags = VNOVAL;
	vap->va_rdev = VNOVAL;
	vap->va_bytes = VNOVAL;
}

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))
#define ARRAY_PRINT(idx, arr) \
    ((unsigned int)(idx) < ARRAY_SIZE(arr) ? (arr)[(idx)] : "UNKNOWN")

const char * const vnode_tags[] = { VNODE_TAGS };
const char * const vnode_types[] = { VNODE_TYPES };
const char vnode_flagbits[] = VNODE_FLAGBITS;

/*
 * Print out a description of a vnode.
 */
void
vprint(const char *label, struct vnode *vp)
{
	char bf[96];
	int flag;

	flag = vp->v_iflag | vp->v_vflag | vp->v_uflag;
	snprintb(bf, sizeof(bf), vnode_flagbits, flag);

	if (label != NULL)
		printf("%s: ", label);
	printf("vnode @ %p, flags (%s)\n\ttag %s(%d), type %s(%d), "
	    "usecount %d, writecount %d, holdcount %d\n"
	    "\tfreelisthd %p, mount %p, data %p lock %p\n",
	    vp, bf, ARRAY_PRINT(vp->v_tag, vnode_tags), vp->v_tag,
	    ARRAY_PRINT(vp->v_type, vnode_types), vp->v_type,
	    vp->v_usecount, vp->v_writecount, vp->v_holdcnt,
	    vp->v_freelisthd, vp->v_mount, vp->v_data, &vp->v_lock);
	if (vp->v_data != NULL) {
		printf("\t");
		VOP_PRINT(vp);
	}
}

/* Deprecated. Kept for KPI compatibility. */
int
vaccess(enum vtype type, mode_t file_mode, uid_t uid, gid_t gid,
    mode_t acc_mode, kauth_cred_t cred)
{

#ifdef DIAGNOSTIC
	printf("vaccess: deprecated interface used.\n");
#endif /* DIAGNOSTIC */

	return kauth_authorize_vnode(cred, KAUTH_ACCESS_ACTION(acc_mode,
	    type, file_mode), NULL /* This may panic. */, NULL,
	    genfs_can_access(type, file_mode, uid, gid, acc_mode, cred));
}

/*
 * Given a file system name, look up the vfsops for that
 * file system, or return NULL if file system isn't present
 * in the kernel.
 */
struct vfsops *
vfs_getopsbyname(const char *name)
{
	struct vfsops *v;

	mutex_enter(&vfs_list_lock);
	LIST_FOREACH(v, &vfs_list, vfs_list) {
		if (strcmp(v->vfs_name, name) == 0)
			break;
	}
	if (v != NULL)
		v->vfs_refcount++;
	mutex_exit(&vfs_list_lock);

	return (v);
}

void
copy_statvfs_info(struct statvfs *sbp, const struct mount *mp)
{
	const struct statvfs *mbp;

	if (sbp == (mbp = &mp->mnt_stat))
		return;

	(void)memcpy(&sbp->f_fsidx, &mbp->f_fsidx, sizeof(sbp->f_fsidx));
	sbp->f_fsid = mbp->f_fsid;
	sbp->f_owner = mbp->f_owner;
	sbp->f_flag = mbp->f_flag;
	sbp->f_syncwrites = mbp->f_syncwrites;
	sbp->f_asyncwrites = mbp->f_asyncwrites;
	sbp->f_syncreads = mbp->f_syncreads;
	sbp->f_asyncreads = mbp->f_asyncreads;
	(void)memcpy(sbp->f_spare, mbp->f_spare, sizeof(mbp->f_spare));
	(void)memcpy(sbp->f_fstypename, mbp->f_fstypename,
	    sizeof(sbp->f_fstypename));
	(void)memcpy(sbp->f_mntonname, mbp->f_mntonname,
	    sizeof(sbp->f_mntonname));
	(void)memcpy(sbp->f_mntfromname, mp->mnt_stat.f_mntfromname,
	    sizeof(sbp->f_mntfromname));
	sbp->f_namemax = mbp->f_namemax;
}

int
set_statvfs_info(const char *onp, int ukon, const char *fromp, int ukfrom,
    const char *vfsname, struct mount *mp, struct lwp *l)
{
	int error;
	size_t size;
	struct statvfs *sfs = &mp->mnt_stat;
	int (*fun)(const void *, void *, size_t, size_t *);

	(void)strlcpy(mp->mnt_stat.f_fstypename, vfsname,
	    sizeof(mp->mnt_stat.f_fstypename));

	if (onp) {
		struct cwdinfo *cwdi = l->l_proc->p_cwdi;
		fun = (ukon == UIO_SYSSPACE) ? copystr : copyinstr;
		if (cwdi->cwdi_rdir != NULL) {
			size_t len;
			char *bp;
			char *path = PNBUF_GET();

			bp = path + MAXPATHLEN;
			*--bp = '\0';
			rw_enter(&cwdi->cwdi_lock, RW_READER);
			error = getcwd_common(cwdi->cwdi_rdir, rootvnode, &bp,
			    path, MAXPATHLEN / 2, 0, l);
			rw_exit(&cwdi->cwdi_lock);
			if (error) {
				PNBUF_PUT(path);
				return error;
			}

			len = strlen(bp);
			if (len > sizeof(sfs->f_mntonname) - 1)
				len = sizeof(sfs->f_mntonname) - 1;
			(void)strncpy(sfs->f_mntonname, bp, len);
			PNBUF_PUT(path);

			if (len < sizeof(sfs->f_mntonname) - 1) {
				error = (*fun)(onp, &sfs->f_mntonname[len],
				    sizeof(sfs->f_mntonname) - len - 1, &size);
				if (error)
					return error;
				size += len;
			} else {
				size = len;
			}
		} else {
			error = (*fun)(onp, &sfs->f_mntonname,
			    sizeof(sfs->f_mntonname) - 1, &size);
			if (error)
				return error;
		}
		(void)memset(sfs->f_mntonname + size, 0,
		    sizeof(sfs->f_mntonname) - size);
	}

	if (fromp) {
		fun = (ukfrom == UIO_SYSSPACE) ? copystr : copyinstr;
		error = (*fun)(fromp, sfs->f_mntfromname,
		    sizeof(sfs->f_mntfromname) - 1, &size);
		if (error)
			return error;
		(void)memset(sfs->f_mntfromname + size, 0,
		    sizeof(sfs->f_mntfromname) - size);
	}
	return 0;
}

void
vfs_timestamp(struct timespec *ts)
{

	nanotime(ts);
}

time_t	rootfstime;			/* recorded root fs time, if known */
void
setrootfstime(time_t t)
{
	rootfstime = t;
}

static const uint8_t vttodt_tab[ ] = {
	[VNON]	=	DT_UNKNOWN,
	[VREG]	=	DT_REG,
	[VDIR]	=	DT_DIR,
	[VBLK]	=	DT_BLK,
	[VCHR]	=	DT_CHR,
	[VLNK]	=	DT_LNK,
	[VSOCK]	=	DT_SOCK,
	[VFIFO]	=	DT_FIFO,
	[VBAD]	=	DT_UNKNOWN
};

uint8_t
vtype2dt(enum vtype vt)
{

	CTASSERT(VBAD == __arraycount(vttodt_tab) - 1);
	return vttodt_tab[vt];
}

int
VFS_MOUNT(struct mount *mp, const char *a, void *b, size_t *c)
{
	int error;

	KERNEL_LOCK(1, NULL);
	error = (*(mp->mnt_op->vfs_mount))(mp, a, b, c);
	KERNEL_UNLOCK_ONE(NULL);

	return error;
}
	
int
VFS_START(struct mount *mp, int a)
{
	int error;

	if ((mp->mnt_iflag & IMNT_MPSAFE) == 0) {
		KERNEL_LOCK(1, NULL);
	}
	error = (*(mp->mnt_op->vfs_start))(mp, a);
	if ((mp->mnt_iflag & IMNT_MPSAFE) == 0) {
		KERNEL_UNLOCK_ONE(NULL);
	}

	return error;
}
	
int
VFS_UNMOUNT(struct mount *mp, int a)
{
	int error;

	KERNEL_LOCK(1, NULL);
	error = (*(mp->mnt_op->vfs_unmount))(mp, a);
	KERNEL_UNLOCK_ONE(NULL);

	return error;
}

int
VFS_ROOT(struct mount *mp, struct vnode **a)
{
	int error;

	if ((mp->mnt_iflag & IMNT_MPSAFE) == 0) {
		KERNEL_LOCK(1, NULL);
	}
	error = (*(mp->mnt_op->vfs_root))(mp, a);
	if ((mp->mnt_iflag & IMNT_MPSAFE) == 0) {
		KERNEL_UNLOCK_ONE(NULL);
	}

	return error;
}

int
VFS_QUOTACTL(struct mount *mp, struct quotactl_args *args)
{
	int error;

	if ((mp->mnt_iflag & IMNT_MPSAFE) == 0) {
		KERNEL_LOCK(1, NULL);
	}
	error = (*(mp->mnt_op->vfs_quotactl))(mp, args);
	if ((mp->mnt_iflag & IMNT_MPSAFE) == 0) {
		KERNEL_UNLOCK_ONE(NULL);
	}

	return error;
}

int
VFS_STATVFS(struct mount *mp, struct statvfs *a)
{
	int error;

	if ((mp->mnt_iflag & IMNT_MPSAFE) == 0) {
		KERNEL_LOCK(1, NULL);
	}
	error = (*(mp->mnt_op->vfs_statvfs))(mp, a);
	if ((mp->mnt_iflag & IMNT_MPSAFE) == 0) {
		KERNEL_UNLOCK_ONE(NULL);
	}

	return error;
}

int
VFS_SYNC(struct mount *mp, int a, struct kauth_cred *b)
{
	int error;

	if ((mp->mnt_iflag & IMNT_MPSAFE) == 0) {
		KERNEL_LOCK(1, NULL);
	}
	error = (*(mp->mnt_op->vfs_sync))(mp, a, b);
	if ((mp->mnt_iflag & IMNT_MPSAFE) == 0) {
		KERNEL_UNLOCK_ONE(NULL);
	}

	return error;
}

int
VFS_FHTOVP(struct mount *mp, struct fid *a, struct vnode **b)
{
	int error;

	if ((mp->mnt_iflag & IMNT_MPSAFE) == 0) {
		KERNEL_LOCK(1, NULL);
	}
	error = (*(mp->mnt_op->vfs_fhtovp))(mp, a, b);
	if ((mp->mnt_iflag & IMNT_MPSAFE) == 0) {
		KERNEL_UNLOCK_ONE(NULL);
	}

	return error;
}

int
VFS_VPTOFH(struct vnode *vp, struct fid *a, size_t *b)
{
	int error;

	if ((vp->v_vflag & VV_MPSAFE) == 0) {
		KERNEL_LOCK(1, NULL);
	}
	error = (*(vp->v_mount->mnt_op->vfs_vptofh))(vp, a, b);
	if ((vp->v_vflag & VV_MPSAFE) == 0) {
		KERNEL_UNLOCK_ONE(NULL);
	}

	return error;
}

int
VFS_SNAPSHOT(struct mount *mp, struct vnode *a, struct timespec *b)
{
	int error;

	if ((mp->mnt_iflag & IMNT_MPSAFE) == 0) {
		KERNEL_LOCK(1, NULL);
	}
	error = (*(mp->mnt_op->vfs_snapshot))(mp, a, b);
	if ((mp->mnt_iflag & IMNT_MPSAFE) == 0) {
		KERNEL_UNLOCK_ONE(NULL);
	}

	return error;
}

int
VFS_EXTATTRCTL(struct mount *mp, int a, struct vnode *b, int c, const char *d)
{
	int error;

	KERNEL_LOCK(1, NULL);		/* XXXSMP check ffs */
	error = (*(mp->mnt_op->vfs_extattrctl))(mp, a, b, c, d);
	KERNEL_UNLOCK_ONE(NULL);	/* XXX */

	return error;
}

int
VFS_SUSPENDCTL(struct mount *mp, int a)
{
	int error;

	if ((mp->mnt_iflag & IMNT_MPSAFE) == 0) {
		KERNEL_LOCK(1, NULL);
	}
	error = (*(mp->mnt_op->vfs_suspendctl))(mp, a);
	if ((mp->mnt_iflag & IMNT_MPSAFE) == 0) {
		KERNEL_UNLOCK_ONE(NULL);
	}

	return error;
}

#if defined(DDB) || defined(DEBUGPRINT)
static const char buf_flagbits[] = BUF_FLAGBITS;

void
vfs_buf_print(struct buf *bp, int full, void (*pr)(const char *, ...))
{
	char bf[1024];

	(*pr)("  vp %p lblkno 0x%"PRIx64" blkno 0x%"PRIx64" rawblkno 0x%"
	    PRIx64 " dev 0x%x\n",
	    bp->b_vp, bp->b_lblkno, bp->b_blkno, bp->b_rawblkno, bp->b_dev);

	snprintb(bf, sizeof(bf),
	    buf_flagbits, bp->b_flags | bp->b_oflags | bp->b_cflags);
	(*pr)("  error %d flags 0x%s\n", bp->b_error, bf);

	(*pr)("  bufsize 0x%lx bcount 0x%lx resid 0x%lx\n",
		  bp->b_bufsize, bp->b_bcount, bp->b_resid);
	(*pr)("  data %p saveaddr %p\n",
		  bp->b_data, bp->b_saveaddr);
	(*pr)("  iodone %p objlock %p\n", bp->b_iodone, bp->b_objlock);
}

void
vfs_vnode_print(struct vnode *vp, int full, void (*pr)(const char *, ...))
{
	char bf[256];

	uvm_object_printit(&vp->v_uobj, full, pr);
	snprintb(bf, sizeof(bf),
	    vnode_flagbits, vp->v_iflag | vp->v_vflag | vp->v_uflag);
	(*pr)("\nVNODE flags %s\n", bf);
	(*pr)("mp %p numoutput %d size 0x%llx writesize 0x%llx\n",
	      vp->v_mount, vp->v_numoutput, vp->v_size, vp->v_writesize);

	(*pr)("data %p writecount %ld holdcnt %ld\n",
	      vp->v_data, vp->v_writecount, vp->v_holdcnt);

	(*pr)("tag %s(%d) type %s(%d) mount %p typedata %p\n",
	      ARRAY_PRINT(vp->v_tag, vnode_tags), vp->v_tag,
	      ARRAY_PRINT(vp->v_type, vnode_types), vp->v_type,
	      vp->v_mount, vp->v_mountedhere);

	(*pr)("v_lock %p\n", &vp->v_lock);

	if (full) {
		struct buf *bp;

		(*pr)("clean bufs:\n");
		LIST_FOREACH(bp, &vp->v_cleanblkhd, b_vnbufs) {
			(*pr)(" bp %p\n", bp);
			vfs_buf_print(bp, full, pr);
		}

		(*pr)("dirty bufs:\n");
		LIST_FOREACH(bp, &vp->v_dirtyblkhd, b_vnbufs) {
			(*pr)(" bp %p\n", bp);
			vfs_buf_print(bp, full, pr);
		}
	}
}

void
vfs_mount_print(struct mount *mp, int full, void (*pr)(const char *, ...))
{
	char sbuf[256];

	(*pr)("vnodecovered = %p data = %p\n",
			mp->mnt_vnodecovered,mp->mnt_data);

	(*pr)("fs_bshift %d dev_bshift = %d\n",
			mp->mnt_fs_bshift,mp->mnt_dev_bshift);

	snprintb(sbuf, sizeof(sbuf), __MNT_FLAG_BITS, mp->mnt_flag);
	(*pr)("flag = %s\n", sbuf);

	snprintb(sbuf, sizeof(sbuf), __IMNT_FLAG_BITS, mp->mnt_iflag);
	(*pr)("iflag = %s\n", sbuf);

	(*pr)("refcnt = %d unmounting @ %p updating @ %p\n", mp->mnt_refcnt,
	    &mp->mnt_unmounting, &mp->mnt_updating);

	(*pr)("statvfs cache:\n");
	(*pr)("\tbsize = %lu\n",mp->mnt_stat.f_bsize);
	(*pr)("\tfrsize = %lu\n",mp->mnt_stat.f_frsize);
	(*pr)("\tiosize = %lu\n",mp->mnt_stat.f_iosize);

	(*pr)("\tblocks = %"PRIu64"\n",mp->mnt_stat.f_blocks);
	(*pr)("\tbfree = %"PRIu64"\n",mp->mnt_stat.f_bfree);
	(*pr)("\tbavail = %"PRIu64"\n",mp->mnt_stat.f_bavail);
	(*pr)("\tbresvd = %"PRIu64"\n",mp->mnt_stat.f_bresvd);

	(*pr)("\tfiles = %"PRIu64"\n",mp->mnt_stat.f_files);
	(*pr)("\tffree = %"PRIu64"\n",mp->mnt_stat.f_ffree);
	(*pr)("\tfavail = %"PRIu64"\n",mp->mnt_stat.f_favail);
	(*pr)("\tfresvd = %"PRIu64"\n",mp->mnt_stat.f_fresvd);

	(*pr)("\tf_fsidx = { 0x%"PRIx32", 0x%"PRIx32" }\n",
			mp->mnt_stat.f_fsidx.__fsid_val[0],
			mp->mnt_stat.f_fsidx.__fsid_val[1]);

	(*pr)("\towner = %"PRIu32"\n",mp->mnt_stat.f_owner);
	(*pr)("\tnamemax = %lu\n",mp->mnt_stat.f_namemax);

	snprintb(sbuf, sizeof(sbuf), __MNT_FLAG_BITS, mp->mnt_stat.f_flag);

	(*pr)("\tflag = %s\n",sbuf);
	(*pr)("\tsyncwrites = %" PRIu64 "\n",mp->mnt_stat.f_syncwrites);
	(*pr)("\tasyncwrites = %" PRIu64 "\n",mp->mnt_stat.f_asyncwrites);
	(*pr)("\tsyncreads = %" PRIu64 "\n",mp->mnt_stat.f_syncreads);
	(*pr)("\tasyncreads = %" PRIu64 "\n",mp->mnt_stat.f_asyncreads);
	(*pr)("\tfstypename = %s\n",mp->mnt_stat.f_fstypename);
	(*pr)("\tmntonname = %s\n",mp->mnt_stat.f_mntonname);
	(*pr)("\tmntfromname = %s\n",mp->mnt_stat.f_mntfromname);

	{
		int cnt = 0;
		struct vnode *vp;
		(*pr)("locked vnodes =");
		TAILQ_FOREACH(vp, &mp->mnt_vnodelist, v_mntvnodes) {
			if (VOP_ISLOCKED(vp)) {
				if ((++cnt % 6) == 0) {
					(*pr)(" %p,\n\t", vp);
				} else {
					(*pr)(" %p,", vp);
				}
			}
		}
		(*pr)("\n");
	}

	if (full) {
		int cnt = 0;
		struct vnode *vp;
		(*pr)("all vnodes =");
		TAILQ_FOREACH(vp, &mp->mnt_vnodelist, v_mntvnodes) {
			if (!TAILQ_NEXT(vp, v_mntvnodes)) {
				(*pr)(" %p", vp);
			} else if ((++cnt % 6) == 0) {
				(*pr)(" %p,\n\t", vp);
			} else {
				(*pr)(" %p,", vp);
			}
		}
		(*pr)("\n", vp);
	}
}

/*
 * List all of the locked vnodes in the system.
 */
void printlockedvnodes(void);

void
printlockedvnodes(void)
{
	struct mount *mp, *nmp;
	struct vnode *vp;

	printf("Locked vnodes\n");
	mutex_enter(&mountlist_lock);
	for (mp = TAILQ_FIRST(&mountlist); mp != NULL; mp = nmp) {
		if (vfs_busy(mp, &nmp)) {
			continue;
		}
		TAILQ_FOREACH(vp, &mp->mnt_vnodelist, v_mntvnodes) {
			if (VOP_ISLOCKED(vp))
				vprint(NULL, vp);
		}
		mutex_enter(&mountlist_lock);
		vfs_unbusy(mp, false, &nmp);
	}
	mutex_exit(&mountlist_lock);
}

#endif /* DDB || DEBUGPRINT */
