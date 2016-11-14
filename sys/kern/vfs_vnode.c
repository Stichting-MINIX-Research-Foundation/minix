/*	$NetBSD: vfs_vnode.c,v 1.45 2015/07/12 08:11:28 hannken Exp $	*/

/*-
 * Copyright (c) 1997-2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center, by Charles M. Hannum, and by Andrew Doran.
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

/*
 * The vnode cache subsystem.
 *
 * Life-cycle
 *
 *	Normally, there are two points where new vnodes are created:
 *	VOP_CREATE(9) and VOP_LOOKUP(9).  The life-cycle of a vnode
 *	starts in one of the following ways:
 *
 *	- Allocation, via vcache_get(9) or vcache_new(9).
 *	- Reclamation of inactive vnode, via vget(9).
 *
 *	Recycle from a free list, via getnewvnode(9) -> getcleanvnode(9)
 *	was another, traditional way.  Currently, only the draining thread
 *	recycles the vnodes.  This behaviour might be revisited.
 *
 *	The life-cycle ends when the last reference is dropped, usually
 *	in VOP_REMOVE(9).  In such case, VOP_INACTIVE(9) is called to inform
 *	the file system that vnode is inactive.  Via this call, file system
 *	indicates whether vnode can be recycled (usually, it checks its own
 *	references, e.g. count of links, whether the file was removed).
 *
 *	Depending on indication, vnode can be put into a free list (cache),
 *	or cleaned via vclean(9), which calls VOP_RECLAIM(9) to disassociate
 *	underlying file system from the vnode, and finally destroyed.
 *
 * Reference counting
 *
 *	Vnode is considered active, if reference count (vnode_t::v_usecount)
 *	is non-zero.  It is maintained using: vref(9) and vrele(9), as well
 *	as vput(9), routines.  Common points holding references are e.g.
 *	file openings, current working directory, mount points, etc.  
 *
 * Note on v_usecount and its locking
 *
 *	At nearly all points it is known that v_usecount could be zero,
 *	the vnode_t::v_interlock will be held.  To change v_usecount away
 *	from zero, the interlock must be held.  To change from a non-zero
 *	value to zero, again the interlock must be held.
 *
 *	Changing the usecount from a non-zero value to a non-zero value can
 *	safely be done using atomic operations, without the interlock held.
 *
 *	Note: if VI_CLEAN is set, vnode_t::v_interlock will be released while
 *	mntvnode_lock is still held.
 *
 *	See PR 41374.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: vfs_vnode.c,v 1.45 2015/07/12 08:11:28 hannken Exp $");

#define _VFS_VNODE_PRIVATE

#include <sys/param.h>
#include <sys/kernel.h>

#include <sys/atomic.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/hash.h>
#include <sys/kauth.h>
#include <sys/kmem.h>
#include <sys/kthread.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/syscallargs.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/wapbl.h>
#include <sys/fstrans.h>

#include <uvm/uvm.h>
#include <uvm/uvm_readahead.h>

/* Flags to vrelel. */
#define	VRELEL_ASYNC_RELE	0x0001	/* Always defer to vrele thread. */
#define	VRELEL_CHANGING_SET	0x0002	/* VI_CHANGING set by caller. */

struct vcache_key {
	struct mount *vk_mount;
	const void *vk_key;
	size_t vk_key_len;
};
struct vcache_node {
	SLIST_ENTRY(vcache_node) vn_hash;
	struct vnode *vn_vnode;
	struct vcache_key vn_key;
};

u_int			numvnodes		__cacheline_aligned;

static pool_cache_t	vnode_cache		__read_mostly;

/*
 * There are two free lists: one is for vnodes which have no buffer/page
 * references and one for those which do (i.e. v_holdcnt is non-zero).
 * Vnode recycling mechanism first attempts to look into the former list.
 */
static kmutex_t		vnode_free_list_lock	__cacheline_aligned;
static vnodelst_t	vnode_free_list		__cacheline_aligned;
static vnodelst_t	vnode_hold_list		__cacheline_aligned;
static kcondvar_t	vdrain_cv		__cacheline_aligned;

static vnodelst_t	vrele_list		__cacheline_aligned;
static kmutex_t		vrele_lock		__cacheline_aligned;
static kcondvar_t	vrele_cv		__cacheline_aligned;
static lwp_t *		vrele_lwp		__cacheline_aligned;
static int		vrele_pending		__cacheline_aligned;
static int		vrele_gen		__cacheline_aligned;

SLIST_HEAD(hashhead, vcache_node);
static struct {
	kmutex_t	lock;
	u_long		hashmask;
	struct hashhead	*hashtab;
	pool_cache_t	pool;
}			vcache			__cacheline_aligned;

static int		cleanvnode(void);
static void		vcache_init(void);
static void		vcache_reinit(void);
static void		vclean(vnode_t *);
static void		vrelel(vnode_t *, int);
static void		vdrain_thread(void *);
static void		vrele_thread(void *);
static void		vnpanic(vnode_t *, const char *, ...)
    __printflike(2, 3);
static void		vwait(vnode_t *, int);

/* Routines having to do with the management of the vnode table. */
extern struct mount	*dead_rootmount;
extern int		(**dead_vnodeop_p)(void *);
extern struct vfsops	dead_vfsops;

void
vfs_vnode_sysinit(void)
{
	int error __diagused;

	vnode_cache = pool_cache_init(sizeof(vnode_t), 0, 0, 0, "vnodepl",
	    NULL, IPL_NONE, NULL, NULL, NULL);
	KASSERT(vnode_cache != NULL);

	dead_rootmount = vfs_mountalloc(&dead_vfsops, NULL);
	KASSERT(dead_rootmount != NULL);
	dead_rootmount->mnt_iflag = IMNT_MPSAFE;

	mutex_init(&vnode_free_list_lock, MUTEX_DEFAULT, IPL_NONE);
	TAILQ_INIT(&vnode_free_list);
	TAILQ_INIT(&vnode_hold_list);
	TAILQ_INIT(&vrele_list);

	vcache_init();

	mutex_init(&vrele_lock, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&vdrain_cv, "vdrain");
	cv_init(&vrele_cv, "vrele");
	error = kthread_create(PRI_VM, KTHREAD_MPSAFE, NULL, vdrain_thread,
	    NULL, NULL, "vdrain");
	KASSERT(error == 0);
	error = kthread_create(PRI_VM, KTHREAD_MPSAFE, NULL, vrele_thread,
	    NULL, &vrele_lwp, "vrele");
	KASSERT(error == 0);
}

/*
 * Allocate a new, uninitialized vnode.  If 'mp' is non-NULL, this is a
 * marker vnode.
 */
vnode_t *
vnalloc(struct mount *mp)
{
	vnode_t *vp;

	vp = pool_cache_get(vnode_cache, PR_WAITOK);
	KASSERT(vp != NULL);

	memset(vp, 0, sizeof(*vp));
	uvm_obj_init(&vp->v_uobj, &uvm_vnodeops, true, 0);
	cv_init(&vp->v_cv, "vnode");
	/*
	 * Done by memset() above.
	 *	LIST_INIT(&vp->v_nclist);
	 *	LIST_INIT(&vp->v_dnclist);
	 */

	if (mp != NULL) {
		vp->v_mount = mp;
		vp->v_type = VBAD;
		vp->v_iflag = VI_MARKER;
		return vp;
	}

	mutex_enter(&vnode_free_list_lock);
	numvnodes++;
	if (numvnodes > desiredvnodes + desiredvnodes / 10)
		cv_signal(&vdrain_cv);
	mutex_exit(&vnode_free_list_lock);

	rw_init(&vp->v_lock);
	vp->v_usecount = 1;
	vp->v_type = VNON;
	vp->v_size = vp->v_writesize = VSIZENOTSET;

	return vp;
}

/*
 * Free an unused, unreferenced vnode.
 */
void
vnfree(vnode_t *vp)
{

	KASSERT(vp->v_usecount == 0);

	if ((vp->v_iflag & VI_MARKER) == 0) {
		rw_destroy(&vp->v_lock);
		mutex_enter(&vnode_free_list_lock);
		numvnodes--;
		mutex_exit(&vnode_free_list_lock);
	}

	uvm_obj_destroy(&vp->v_uobj, true);
	cv_destroy(&vp->v_cv);
	pool_cache_put(vnode_cache, vp);
}

/*
 * cleanvnode: grab a vnode from freelist, clean and free it.
 *
 * => Releases vnode_free_list_lock.
 */
static int
cleanvnode(void)
{
	vnode_t *vp;
	vnodelst_t *listhd;
	struct mount *mp;

	KASSERT(mutex_owned(&vnode_free_list_lock));

	listhd = &vnode_free_list;
try_nextlist:
	TAILQ_FOREACH(vp, listhd, v_freelist) {
		/*
		 * It's safe to test v_usecount and v_iflag
		 * without holding the interlock here, since
		 * these vnodes should never appear on the
		 * lists.
		 */
		KASSERT(vp->v_usecount == 0);
		KASSERT((vp->v_iflag & VI_CLEAN) == 0);
		KASSERT(vp->v_freelisthd == listhd);

		if (!mutex_tryenter(vp->v_interlock))
			continue;
		if ((vp->v_iflag & VI_XLOCK) != 0) {
			mutex_exit(vp->v_interlock);
			continue;
		}
		mp = vp->v_mount;
		if (fstrans_start_nowait(mp, FSTRANS_SHARED) != 0) {
			mutex_exit(vp->v_interlock);
			continue;
		}
		break;
	}

	if (vp == NULL) {
		if (listhd == &vnode_free_list) {
			listhd = &vnode_hold_list;
			goto try_nextlist;
		}
		mutex_exit(&vnode_free_list_lock);
		return EBUSY;
	}

	/* Remove it from the freelist. */
	TAILQ_REMOVE(listhd, vp, v_freelist);
	vp->v_freelisthd = NULL;
	mutex_exit(&vnode_free_list_lock);

	KASSERT(vp->v_usecount == 0);

	/*
	 * The vnode is still associated with a file system, so we must
	 * clean it out before freeing it.  We need to add a reference
	 * before doing this.
	 */
	vp->v_usecount = 1;
	KASSERT((vp->v_iflag & VI_CHANGING) == 0);
	vp->v_iflag |= VI_CHANGING;
	vclean(vp);
	vrelel(vp, VRELEL_CHANGING_SET);
	fstrans_done(mp);

	return 0;
}

/*
 * Helper thread to keep the number of vnodes below desiredvnodes.
 */
static void
vdrain_thread(void *cookie)
{
	int error;

	mutex_enter(&vnode_free_list_lock);

	for (;;) {
		cv_timedwait(&vdrain_cv, &vnode_free_list_lock, hz);
		while (numvnodes > desiredvnodes) {
			error = cleanvnode();
			if (error)
				kpause("vndsbusy", false, hz, NULL);
			mutex_enter(&vnode_free_list_lock);
			if (error)
				break;
		}
	}
}

/*
 * Remove a vnode from its freelist.
 */
void
vremfree(vnode_t *vp)
{

	KASSERT(mutex_owned(vp->v_interlock));
	KASSERT(vp->v_usecount == 0);

	/*
	 * Note that the reference count must not change until
	 * the vnode is removed.
	 */
	mutex_enter(&vnode_free_list_lock);
	if (vp->v_holdcnt > 0) {
		KASSERT(vp->v_freelisthd == &vnode_hold_list);
	} else {
		KASSERT(vp->v_freelisthd == &vnode_free_list);
	}
	TAILQ_REMOVE(vp->v_freelisthd, vp, v_freelist);
	vp->v_freelisthd = NULL;
	mutex_exit(&vnode_free_list_lock);
}

/*
 * vget: get a particular vnode from the free list, increment its reference
 * count and lock it.
 *
 * => Should be called with v_interlock held.
 *
 * If VI_CHANGING is set, the vnode may be eliminated in vgone()/vclean().
 * In that case, we cannot grab the vnode, so the process is awakened when
 * the transition is completed, and an error returned to indicate that the
 * vnode is no longer usable.
 */
int
vget(vnode_t *vp, int flags, bool waitok)
{
	int error = 0;

	KASSERT((vp->v_iflag & VI_MARKER) == 0);
	KASSERT(mutex_owned(vp->v_interlock));
	KASSERT((flags & ~LK_NOWAIT) == 0);
	KASSERT(waitok == ((flags & LK_NOWAIT) == 0));

	/*
	 * Before adding a reference, we must remove the vnode
	 * from its freelist.
	 */
	if (vp->v_usecount == 0) {
		vremfree(vp);
		vp->v_usecount = 1;
	} else {
		atomic_inc_uint(&vp->v_usecount);
	}

	/*
	 * If the vnode is in the process of changing state we wait
	 * for the change to complete and take care not to return
	 * a clean vnode.
	 */
	if ((vp->v_iflag & VI_CHANGING) != 0) {
		if ((flags & LK_NOWAIT) != 0) {
			vrelel(vp, 0);
			return EBUSY;
		}
		vwait(vp, VI_CHANGING);
		if ((vp->v_iflag & VI_CLEAN) != 0) {
			vrelel(vp, 0);
			return ENOENT;
		}
	}

	/*
	 * Ok, we got it in good shape.
	 */
	KASSERT((vp->v_iflag & VI_CLEAN) == 0);
	mutex_exit(vp->v_interlock);
	return error;
}

/*
 * vput: unlock and release the reference.
 */
void
vput(vnode_t *vp)
{

	KASSERT((vp->v_iflag & VI_MARKER) == 0);

	VOP_UNLOCK(vp);
	vrele(vp);
}

/*
 * Try to drop reference on a vnode.  Abort if we are releasing the
 * last reference.  Note: this _must_ succeed if not the last reference.
 */
static inline bool
vtryrele(vnode_t *vp)
{
	u_int use, next;

	for (use = vp->v_usecount;; use = next) {
		if (use == 1) {
			return false;
		}
		KASSERT(use > 1);
		next = atomic_cas_uint(&vp->v_usecount, use, use - 1);
		if (__predict_true(next == use)) {
			return true;
		}
	}
}

/*
 * Vnode release.  If reference count drops to zero, call inactive
 * routine and either return to freelist or free to the pool.
 */
static void
vrelel(vnode_t *vp, int flags)
{
	bool recycle, defer;
	int error;

	KASSERT(mutex_owned(vp->v_interlock));
	KASSERT((vp->v_iflag & VI_MARKER) == 0);
	KASSERT(vp->v_freelisthd == NULL);

	if (__predict_false(vp->v_op == dead_vnodeop_p &&
	    (vp->v_iflag & (VI_CLEAN|VI_XLOCK)) == 0)) {
		vnpanic(vp, "dead but not clean");
	}

	/*
	 * If not the last reference, just drop the reference count
	 * and unlock.
	 */
	if (vtryrele(vp)) {
		if ((flags & VRELEL_CHANGING_SET) != 0) {
			KASSERT((vp->v_iflag & VI_CHANGING) != 0);
			vp->v_iflag &= ~VI_CHANGING;
			cv_broadcast(&vp->v_cv);
		}
		mutex_exit(vp->v_interlock);
		return;
	}
	if (vp->v_usecount <= 0 || vp->v_writecount != 0) {
		vnpanic(vp, "%s: bad ref count", __func__);
	}

	KASSERT((vp->v_iflag & VI_XLOCK) == 0);

#ifdef DIAGNOSTIC
	if ((vp->v_type == VBLK || vp->v_type == VCHR) &&
	    vp->v_specnode != NULL && vp->v_specnode->sn_opencnt != 0) {
		vprint("vrelel: missing VOP_CLOSE()", vp);
	}
#endif

	/*
	 * If not clean, deactivate the vnode, but preserve
	 * our reference across the call to VOP_INACTIVE().
	 */
	if ((vp->v_iflag & VI_CLEAN) == 0) {
		recycle = false;

		/*
		 * XXX This ugly block can be largely eliminated if
		 * locking is pushed down into the file systems.
		 *
		 * Defer vnode release to vrele_thread if caller
		 * requests it explicitly or is the pagedaemon.
		 */
		if ((curlwp == uvm.pagedaemon_lwp) ||
		    (flags & VRELEL_ASYNC_RELE) != 0) {
			defer = true;
		} else if (curlwp == vrele_lwp) {
			/*
			 * We have to try harder.
			 */
			mutex_exit(vp->v_interlock);
			error = vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
			KASSERT(error == 0);
			mutex_enter(vp->v_interlock);
			defer = false;
		} else {
			/* If we can't acquire the lock, then defer. */
			mutex_exit(vp->v_interlock);
			error = vn_lock(vp,
			    LK_EXCLUSIVE | LK_RETRY | LK_NOWAIT);
			defer = (error != 0);
			mutex_enter(vp->v_interlock);
		}

		KASSERT(mutex_owned(vp->v_interlock));
		KASSERT(! (curlwp == vrele_lwp && defer));

		if (defer) {
			/*
			 * Defer reclaim to the kthread; it's not safe to
			 * clean it here.  We donate it our last reference.
			 */
			if ((flags & VRELEL_CHANGING_SET) != 0) {
				KASSERT((vp->v_iflag & VI_CHANGING) != 0);
				vp->v_iflag &= ~VI_CHANGING;
				cv_broadcast(&vp->v_cv);
			}
			mutex_enter(&vrele_lock);
			TAILQ_INSERT_TAIL(&vrele_list, vp, v_freelist);
			if (++vrele_pending > (desiredvnodes >> 8))
				cv_signal(&vrele_cv); 
			mutex_exit(&vrele_lock);
			mutex_exit(vp->v_interlock);
			return;
		}

		/*
		 * If the node got another reference while we
		 * released the interlock, don't try to inactivate it yet.
		 */
		if (__predict_false(vtryrele(vp))) {
			VOP_UNLOCK(vp);
			if ((flags & VRELEL_CHANGING_SET) != 0) {
				KASSERT((vp->v_iflag & VI_CHANGING) != 0);
				vp->v_iflag &= ~VI_CHANGING;
				cv_broadcast(&vp->v_cv);
			}
			mutex_exit(vp->v_interlock);
			return;
		}

		if ((flags & VRELEL_CHANGING_SET) == 0) {
			KASSERT((vp->v_iflag & VI_CHANGING) == 0);
			vp->v_iflag |= VI_CHANGING;
		}
		mutex_exit(vp->v_interlock);

		/*
		 * The vnode can gain another reference while being
		 * deactivated.  If VOP_INACTIVE() indicates that
		 * the described file has been deleted, then recycle
		 * the vnode irrespective of additional references.
		 * Another thread may be waiting to re-use the on-disk
		 * inode.
		 *
		 * Note that VOP_INACTIVE() will drop the vnode lock.
		 */
		VOP_INACTIVE(vp, &recycle);
		mutex_enter(vp->v_interlock);
		if (!recycle) {
			if (vtryrele(vp)) {
				KASSERT((vp->v_iflag & VI_CHANGING) != 0);
				vp->v_iflag &= ~VI_CHANGING;
				cv_broadcast(&vp->v_cv);
				mutex_exit(vp->v_interlock);
				return;
			}
		}

		/* Take care of space accounting. */
		if (vp->v_iflag & VI_EXECMAP) {
			atomic_add_int(&uvmexp.execpages,
			    -vp->v_uobj.uo_npages);
			atomic_add_int(&uvmexp.filepages,
			    vp->v_uobj.uo_npages);
		}
		vp->v_iflag &= ~(VI_TEXT|VI_EXECMAP|VI_WRMAP);
		vp->v_vflag &= ~VV_MAPPED;

		/*
		 * Recycle the vnode if the file is now unused (unlinked),
		 * otherwise just free it.
		 */
		if (recycle) {
			vclean(vp);
		}
		KASSERT(vp->v_usecount > 0);
	} else { /* vnode was already clean */
		if ((flags & VRELEL_CHANGING_SET) == 0) {
			KASSERT((vp->v_iflag & VI_CHANGING) == 0);
			vp->v_iflag |= VI_CHANGING;
		}
	}

	if (atomic_dec_uint_nv(&vp->v_usecount) != 0) {
		/* Gained another reference while being reclaimed. */
		KASSERT((vp->v_iflag & VI_CHANGING) != 0);
		vp->v_iflag &= ~VI_CHANGING;
		cv_broadcast(&vp->v_cv);
		mutex_exit(vp->v_interlock);
		return;
	}

	if ((vp->v_iflag & VI_CLEAN) != 0) {
		/*
		 * It's clean so destroy it.  It isn't referenced
		 * anywhere since it has been reclaimed.
		 */
		KASSERT(vp->v_holdcnt == 0);
		KASSERT(vp->v_writecount == 0);
		mutex_exit(vp->v_interlock);
		vfs_insmntque(vp, NULL);
		if (vp->v_type == VBLK || vp->v_type == VCHR) {
			spec_node_destroy(vp);
		}
		vnfree(vp);
	} else {
		/*
		 * Otherwise, put it back onto the freelist.  It
		 * can't be destroyed while still associated with
		 * a file system.
		 */
		mutex_enter(&vnode_free_list_lock);
		if (vp->v_holdcnt > 0) {
			vp->v_freelisthd = &vnode_hold_list;
		} else {
			vp->v_freelisthd = &vnode_free_list;
		}
		TAILQ_INSERT_TAIL(vp->v_freelisthd, vp, v_freelist);
		mutex_exit(&vnode_free_list_lock);
		KASSERT((vp->v_iflag & VI_CHANGING) != 0);
		vp->v_iflag &= ~VI_CHANGING;
		cv_broadcast(&vp->v_cv);
		mutex_exit(vp->v_interlock);
	}
}

void
vrele(vnode_t *vp)
{

	KASSERT((vp->v_iflag & VI_MARKER) == 0);

	if (vtryrele(vp)) {
		return;
	}
	mutex_enter(vp->v_interlock);
	vrelel(vp, 0);
}

/*
 * Asynchronous vnode release, vnode is released in different context.
 */
void
vrele_async(vnode_t *vp)
{

	KASSERT((vp->v_iflag & VI_MARKER) == 0);

	if (vtryrele(vp)) {
		return;
	}
	mutex_enter(vp->v_interlock);
	vrelel(vp, VRELEL_ASYNC_RELE);
}

static void
vrele_thread(void *cookie)
{
	vnodelst_t skip_list;
	vnode_t *vp;
	struct mount *mp;

	TAILQ_INIT(&skip_list);

	mutex_enter(&vrele_lock);
	for (;;) {
		while (TAILQ_EMPTY(&vrele_list)) {
			vrele_gen++;
			cv_broadcast(&vrele_cv);
			cv_timedwait(&vrele_cv, &vrele_lock, hz);
			TAILQ_CONCAT(&vrele_list, &skip_list, v_freelist);
		}
		vp = TAILQ_FIRST(&vrele_list);
		mp = vp->v_mount;
		TAILQ_REMOVE(&vrele_list, vp, v_freelist);
		if (fstrans_start_nowait(mp, FSTRANS_LAZY) != 0) {
			TAILQ_INSERT_TAIL(&skip_list, vp, v_freelist);
			continue;
		}
		vrele_pending--;
		mutex_exit(&vrele_lock);

		/*
		 * If not the last reference, then ignore the vnode
		 * and look for more work.
		 */
		mutex_enter(vp->v_interlock);
		vrelel(vp, 0);
		fstrans_done(mp);
		mutex_enter(&vrele_lock);
	}
}

void
vrele_flush(void)
{
	int gen;

	mutex_enter(&vrele_lock);
	gen = vrele_gen;
	while (vrele_pending && gen == vrele_gen) {
		cv_broadcast(&vrele_cv);
		cv_wait(&vrele_cv, &vrele_lock);
	}
	mutex_exit(&vrele_lock);
}

/*
 * Vnode reference, where a reference is already held by some other
 * object (for example, a file structure).
 */
void
vref(vnode_t *vp)
{

	KASSERT((vp->v_iflag & VI_MARKER) == 0);
	KASSERT(vp->v_usecount != 0);

	atomic_inc_uint(&vp->v_usecount);
}

/*
 * Page or buffer structure gets a reference.
 * Called with v_interlock held.
 */
void
vholdl(vnode_t *vp)
{

	KASSERT(mutex_owned(vp->v_interlock));
	KASSERT((vp->v_iflag & VI_MARKER) == 0);

	if (vp->v_holdcnt++ == 0 && vp->v_usecount == 0) {
		mutex_enter(&vnode_free_list_lock);
		KASSERT(vp->v_freelisthd == &vnode_free_list);
		TAILQ_REMOVE(vp->v_freelisthd, vp, v_freelist);
		vp->v_freelisthd = &vnode_hold_list;
		TAILQ_INSERT_TAIL(vp->v_freelisthd, vp, v_freelist);
		mutex_exit(&vnode_free_list_lock);
	}
}

/*
 * Page or buffer structure frees a reference.
 * Called with v_interlock held.
 */
void
holdrelel(vnode_t *vp)
{

	KASSERT(mutex_owned(vp->v_interlock));
	KASSERT((vp->v_iflag & VI_MARKER) == 0);

	if (vp->v_holdcnt <= 0) {
		vnpanic(vp, "%s: holdcnt vp %p", __func__, vp);
	}

	vp->v_holdcnt--;
	if (vp->v_holdcnt == 0 && vp->v_usecount == 0) {
		mutex_enter(&vnode_free_list_lock);
		KASSERT(vp->v_freelisthd == &vnode_hold_list);
		TAILQ_REMOVE(vp->v_freelisthd, vp, v_freelist);
		vp->v_freelisthd = &vnode_free_list;
		TAILQ_INSERT_TAIL(vp->v_freelisthd, vp, v_freelist);
		mutex_exit(&vnode_free_list_lock);
	}
}

/*
 * Disassociate the underlying file system from a vnode.
 *
 * Must be called with the interlock held, and will return with it held.
 */
static void
vclean(vnode_t *vp)
{
	lwp_t *l = curlwp;
	bool recycle, active;
	int error;

	KASSERT(mutex_owned(vp->v_interlock));
	KASSERT((vp->v_iflag & VI_MARKER) == 0);
	KASSERT(vp->v_usecount != 0);

	/* If already clean, nothing to do. */
	if ((vp->v_iflag & VI_CLEAN) != 0) {
		return;
	}

	active = (vp->v_usecount > 1);
	mutex_exit(vp->v_interlock);

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);

	/*
	 * Prevent the vnode from being recycled or brought into use
	 * while we clean it out.
	 */
	mutex_enter(vp->v_interlock);
	KASSERT((vp->v_iflag & (VI_XLOCK | VI_CLEAN)) == 0);
	vp->v_iflag |= VI_XLOCK;
	if (vp->v_iflag & VI_EXECMAP) {
		atomic_add_int(&uvmexp.execpages, -vp->v_uobj.uo_npages);
		atomic_add_int(&uvmexp.filepages, vp->v_uobj.uo_npages);
	}
	vp->v_iflag &= ~(VI_TEXT|VI_EXECMAP);
	mutex_exit(vp->v_interlock);

	/*
	 * Clean out any cached data associated with the vnode.
	 * If purging an active vnode, it must be closed and
	 * deactivated before being reclaimed. Note that the
	 * VOP_INACTIVE will unlock the vnode.
	 */
	error = vinvalbuf(vp, V_SAVE, NOCRED, l, 0, 0);
	if (error != 0) {
		if (wapbl_vphaswapbl(vp))
			WAPBL_DISCARD(wapbl_vptomp(vp));
		error = vinvalbuf(vp, 0, NOCRED, l, 0, 0);
	}
	KASSERT(error == 0);
	KASSERT((vp->v_iflag & VI_ONWORKLST) == 0);
	if (active && (vp->v_type == VBLK || vp->v_type == VCHR)) {
		 spec_node_revoke(vp);
	}
	if (active) {
		VOP_INACTIVE(vp, &recycle);
	} else {
		/*
		 * Any other processes trying to obtain this lock must first
		 * wait for VI_XLOCK to clear, then call the new lock operation.
		 */
		VOP_UNLOCK(vp);
	}

	/* Disassociate the underlying file system from the vnode. */
	if (VOP_RECLAIM(vp)) {
		vnpanic(vp, "%s: cannot reclaim", __func__);
	}

	KASSERT(vp->v_data == NULL);
	KASSERT(vp->v_uobj.uo_npages == 0);

	if (vp->v_type == VREG && vp->v_ractx != NULL) {
		uvm_ra_freectx(vp->v_ractx);
		vp->v_ractx = NULL;
	}

	/* Purge name cache. */
	cache_purge(vp);

	/* Move to dead mount. */
	vp->v_vflag &= ~VV_ROOT;
	atomic_inc_uint(&dead_rootmount->mnt_refcnt);
	vfs_insmntque(vp, dead_rootmount);

	/* Done with purge, notify sleepers of the grim news. */
	mutex_enter(vp->v_interlock);
	vp->v_op = dead_vnodeop_p;
	vp->v_vflag |= VV_LOCKSWORK;
	vp->v_iflag |= VI_CLEAN;
	vp->v_tag = VT_NON;
	KNOTE(&vp->v_klist, NOTE_REVOKE);
	vp->v_iflag &= ~VI_XLOCK;
	cv_broadcast(&vp->v_cv);

	KASSERT((vp->v_iflag & VI_ONWORKLST) == 0);
}

/*
 * Recycle an unused vnode if caller holds the last reference.
 */
bool
vrecycle(vnode_t *vp)
{

	mutex_enter(vp->v_interlock);

	KASSERT((vp->v_iflag & VI_MARKER) == 0);

	if (vp->v_usecount != 1) {
		mutex_exit(vp->v_interlock);
		return false;
	}
	if ((vp->v_iflag & VI_CHANGING) != 0)
		vwait(vp, VI_CHANGING);
	if (vp->v_usecount != 1) {
		mutex_exit(vp->v_interlock);
		return false;
	} else if ((vp->v_iflag & VI_CLEAN) != 0) {
		mutex_exit(vp->v_interlock);
		return true;
	}
	vp->v_iflag |= VI_CHANGING;
	vclean(vp);
	vrelel(vp, VRELEL_CHANGING_SET);
	return true;
}

/*
 * Eliminate all activity associated with the requested vnode
 * and with all vnodes aliased to the requested vnode.
 */
void
vrevoke(vnode_t *vp)
{
	vnode_t *vq;
	enum vtype type;
	dev_t dev;

	KASSERT(vp->v_usecount > 0);

	mutex_enter(vp->v_interlock);
	if ((vp->v_iflag & VI_CLEAN) != 0) {
		mutex_exit(vp->v_interlock);
		return;
	} else if (vp->v_type != VBLK && vp->v_type != VCHR) {
		atomic_inc_uint(&vp->v_usecount);
		mutex_exit(vp->v_interlock);
		vgone(vp);
		return;
	} else {
		dev = vp->v_rdev;
		type = vp->v_type;
		mutex_exit(vp->v_interlock);
	}

	while (spec_node_lookup_by_dev(type, dev, &vq) == 0) {
		vgone(vq);
	}
}

/*
 * Eliminate all activity associated with a vnode in preparation for
 * reuse.  Drops a reference from the vnode.
 */
void
vgone(vnode_t *vp)
{

	mutex_enter(vp->v_interlock);
	if ((vp->v_iflag & VI_CHANGING) != 0)
		vwait(vp, VI_CHANGING);
	vp->v_iflag |= VI_CHANGING;
	vclean(vp);
	vrelel(vp, VRELEL_CHANGING_SET);
}

static inline uint32_t
vcache_hash(const struct vcache_key *key)
{
	uint32_t hash = HASH32_BUF_INIT;

	hash = hash32_buf(&key->vk_mount, sizeof(struct mount *), hash);
	hash = hash32_buf(key->vk_key, key->vk_key_len, hash);
	return hash;
}

static void
vcache_init(void)
{

	vcache.pool = pool_cache_init(sizeof(struct vcache_node), 0, 0, 0,
	    "vcachepl", NULL, IPL_NONE, NULL, NULL, NULL);
	KASSERT(vcache.pool != NULL);
	mutex_init(&vcache.lock, MUTEX_DEFAULT, IPL_NONE);
	vcache.hashtab = hashinit(desiredvnodes, HASH_SLIST, true,
	    &vcache.hashmask);
}

static void
vcache_reinit(void)
{
	int i;
	uint32_t hash;
	u_long oldmask, newmask;
	struct hashhead *oldtab, *newtab;
	struct vcache_node *node;

	newtab = hashinit(desiredvnodes, HASH_SLIST, true, &newmask);
	mutex_enter(&vcache.lock);
	oldtab = vcache.hashtab;
	oldmask = vcache.hashmask;
	vcache.hashtab = newtab;
	vcache.hashmask = newmask;
	for (i = 0; i <= oldmask; i++) {
		while ((node = SLIST_FIRST(&oldtab[i])) != NULL) {
			SLIST_REMOVE(&oldtab[i], node, vcache_node, vn_hash);
			hash = vcache_hash(&node->vn_key);
			SLIST_INSERT_HEAD(&newtab[hash & vcache.hashmask],
			    node, vn_hash);
		}
	}
	mutex_exit(&vcache.lock);
	hashdone(oldtab, HASH_SLIST, oldmask);
}

static inline struct vcache_node *
vcache_hash_lookup(const struct vcache_key *key, uint32_t hash)
{
	struct hashhead *hashp;
	struct vcache_node *node;

	KASSERT(mutex_owned(&vcache.lock));

	hashp = &vcache.hashtab[hash & vcache.hashmask];
	SLIST_FOREACH(node, hashp, vn_hash) {
		if (key->vk_mount != node->vn_key.vk_mount)
			continue;
		if (key->vk_key_len != node->vn_key.vk_key_len)
			continue;
		if (memcmp(key->vk_key, node->vn_key.vk_key, key->vk_key_len))
			continue;
		return node;
	}
	return NULL;
}

/*
 * Get a vnode / fs node pair by key and return it referenced through vpp.
 */
int
vcache_get(struct mount *mp, const void *key, size_t key_len,
    struct vnode **vpp)
{
	int error;
	uint32_t hash;
	const void *new_key;
	struct vnode *vp;
	struct vcache_key vcache_key;
	struct vcache_node *node, *new_node;

	new_key = NULL;
	*vpp = NULL;

	vcache_key.vk_mount = mp;
	vcache_key.vk_key = key;
	vcache_key.vk_key_len = key_len;
	hash = vcache_hash(&vcache_key);

again:
	mutex_enter(&vcache.lock);
	node = vcache_hash_lookup(&vcache_key, hash);

	/* If found, take a reference or retry. */
	if (__predict_true(node != NULL && node->vn_vnode != NULL)) {
		vp = node->vn_vnode;
		mutex_enter(vp->v_interlock);
		mutex_exit(&vcache.lock);
		error = vget(vp, 0, true /* wait */);
		if (error == ENOENT)
			goto again;
		if (error == 0)
			*vpp = vp;
		KASSERT((error != 0) == (*vpp == NULL));
		return error;
	}

	/* If another thread loads this node, wait and retry. */
	if (node != NULL) {
		KASSERT(node->vn_vnode == NULL);
		mutex_exit(&vcache.lock);
		kpause("vcache", false, mstohz(20), NULL);
		goto again;
	}
	mutex_exit(&vcache.lock);

	/* Allocate and initialize a new vcache / vnode pair. */
	error = vfs_busy(mp, NULL);
	if (error)
		return error;
	new_node = pool_cache_get(vcache.pool, PR_WAITOK);
	new_node->vn_vnode = NULL;
	new_node->vn_key = vcache_key;
	vp = vnalloc(NULL);
	mutex_enter(&vcache.lock);
	node = vcache_hash_lookup(&vcache_key, hash);
	if (node == NULL) {
		SLIST_INSERT_HEAD(&vcache.hashtab[hash & vcache.hashmask],
		    new_node, vn_hash);
		node = new_node;
	}
	mutex_exit(&vcache.lock);

	/* If another thread beat us inserting this node, retry. */
	if (node != new_node) {
		pool_cache_put(vcache.pool, new_node);
		KASSERT(vp->v_usecount == 1);
		vp->v_usecount = 0;
		vnfree(vp);
		vfs_unbusy(mp, false, NULL);
		goto again;
	}

	/* Load the fs node.  Exclusive as new_node->vn_vnode is NULL. */
	vp->v_iflag |= VI_CHANGING;
	error = VFS_LOADVNODE(mp, vp, key, key_len, &new_key);
	if (error) {
		mutex_enter(&vcache.lock);
		SLIST_REMOVE(&vcache.hashtab[hash & vcache.hashmask],
		    new_node, vcache_node, vn_hash);
		mutex_exit(&vcache.lock);
		pool_cache_put(vcache.pool, new_node);
		KASSERT(vp->v_usecount == 1);
		vp->v_usecount = 0;
		vnfree(vp);
		vfs_unbusy(mp, false, NULL);
		KASSERT(*vpp == NULL);
		return error;
	}
	KASSERT(new_key != NULL);
	KASSERT(memcmp(key, new_key, key_len) == 0);
	KASSERT(vp->v_op != NULL);
	vfs_insmntque(vp, mp);
	if ((mp->mnt_iflag & IMNT_MPSAFE) != 0)
		vp->v_vflag |= VV_MPSAFE;
	vfs_unbusy(mp, true, NULL);

	/* Finished loading, finalize node. */
	mutex_enter(&vcache.lock);
	new_node->vn_key.vk_key = new_key;
	new_node->vn_vnode = vp;
	mutex_exit(&vcache.lock);
	mutex_enter(vp->v_interlock);
	vp->v_iflag &= ~VI_CHANGING;
	cv_broadcast(&vp->v_cv);
	mutex_exit(vp->v_interlock);
	*vpp = vp;
	return 0;
}

/*
 * Create a new vnode / fs node pair and return it referenced through vpp.
 */
int
vcache_new(struct mount *mp, struct vnode *dvp, struct vattr *vap,
    kauth_cred_t cred, struct vnode **vpp)
{
	int error;
	uint32_t hash;
	struct vnode *vp;
	struct vcache_node *new_node;
	struct vcache_node *old_node __diagused;

	*vpp = NULL;

	/* Allocate and initialize a new vcache / vnode pair. */
	error = vfs_busy(mp, NULL);
	if (error)
		return error;
	new_node = pool_cache_get(vcache.pool, PR_WAITOK);
	new_node->vn_key.vk_mount = mp;
	new_node->vn_vnode = NULL;
	vp = vnalloc(NULL);

	/* Create and load the fs node. */
	vp->v_iflag |= VI_CHANGING;
	error = VFS_NEWVNODE(mp, dvp, vp, vap, cred,
	    &new_node->vn_key.vk_key_len, &new_node->vn_key.vk_key);
	if (error) {
		pool_cache_put(vcache.pool, new_node);
		KASSERT(vp->v_usecount == 1);
		vp->v_usecount = 0;
		vnfree(vp);
		vfs_unbusy(mp, false, NULL);
		KASSERT(*vpp == NULL);
		return error;
	}
	KASSERT(new_node->vn_key.vk_key != NULL);
	KASSERT(vp->v_op != NULL);
	hash = vcache_hash(&new_node->vn_key);

	/* Wait for previous instance to be reclaimed, then insert new node. */
	mutex_enter(&vcache.lock);
	while ((old_node = vcache_hash_lookup(&new_node->vn_key, hash))) {
#ifdef DIAGNOSTIC
		if (old_node->vn_vnode != NULL)
			mutex_enter(old_node->vn_vnode->v_interlock);
		KASSERT(old_node->vn_vnode == NULL ||
		    (old_node->vn_vnode->v_iflag & (VI_XLOCK | VI_CLEAN)) != 0);
		if (old_node->vn_vnode != NULL)
			mutex_exit(old_node->vn_vnode->v_interlock);
#endif
		mutex_exit(&vcache.lock);
		kpause("vcache", false, mstohz(20), NULL);
		mutex_enter(&vcache.lock);
	}
	SLIST_INSERT_HEAD(&vcache.hashtab[hash & vcache.hashmask],
	    new_node, vn_hash);
	mutex_exit(&vcache.lock);
	vfs_insmntque(vp, mp);
	if ((mp->mnt_iflag & IMNT_MPSAFE) != 0)
		vp->v_vflag |= VV_MPSAFE;
	vfs_unbusy(mp, true, NULL);

	/* Finished loading, finalize node. */
	mutex_enter(&vcache.lock);
	new_node->vn_vnode = vp;
	mutex_exit(&vcache.lock);
	mutex_enter(vp->v_interlock);
	vp->v_iflag &= ~VI_CHANGING;
	cv_broadcast(&vp->v_cv);
	mutex_exit(vp->v_interlock);
	*vpp = vp;
	return 0;
}

/*
 * Prepare key change: lock old and new cache node.
 * Return an error if the new node already exists.
 */
int
vcache_rekey_enter(struct mount *mp, struct vnode *vp,
    const void *old_key, size_t old_key_len,
    const void *new_key, size_t new_key_len)
{
	uint32_t old_hash, new_hash;
	struct vcache_key old_vcache_key, new_vcache_key;
	struct vcache_node *node, *new_node;

	old_vcache_key.vk_mount = mp;
	old_vcache_key.vk_key = old_key;
	old_vcache_key.vk_key_len = old_key_len;
	old_hash = vcache_hash(&old_vcache_key);

	new_vcache_key.vk_mount = mp;
	new_vcache_key.vk_key = new_key;
	new_vcache_key.vk_key_len = new_key_len;
	new_hash = vcache_hash(&new_vcache_key);

	new_node = pool_cache_get(vcache.pool, PR_WAITOK);
	new_node->vn_vnode = NULL;
	new_node->vn_key = new_vcache_key;

	mutex_enter(&vcache.lock);
	node = vcache_hash_lookup(&new_vcache_key, new_hash);
	if (node != NULL) {
		mutex_exit(&vcache.lock);
		pool_cache_put(vcache.pool, new_node);
		return EEXIST;
	}
	SLIST_INSERT_HEAD(&vcache.hashtab[new_hash & vcache.hashmask],
	    new_node, vn_hash);
	node = vcache_hash_lookup(&old_vcache_key, old_hash);
	KASSERT(node != NULL);
	KASSERT(node->vn_vnode == vp);
	node->vn_vnode = NULL;
	node->vn_key = old_vcache_key;
	mutex_exit(&vcache.lock);
	return 0;
}

/*
 * Key change complete: remove old node and unlock new node.
 */
void
vcache_rekey_exit(struct mount *mp, struct vnode *vp,
    const void *old_key, size_t old_key_len,
    const void *new_key, size_t new_key_len)
{
	uint32_t old_hash, new_hash;
	struct vcache_key old_vcache_key, new_vcache_key;
	struct vcache_node *node;

	old_vcache_key.vk_mount = mp;
	old_vcache_key.vk_key = old_key;
	old_vcache_key.vk_key_len = old_key_len;
	old_hash = vcache_hash(&old_vcache_key);

	new_vcache_key.vk_mount = mp;
	new_vcache_key.vk_key = new_key;
	new_vcache_key.vk_key_len = new_key_len;
	new_hash = vcache_hash(&new_vcache_key);

	mutex_enter(&vcache.lock);
	node = vcache_hash_lookup(&new_vcache_key, new_hash);
	KASSERT(node != NULL && node->vn_vnode == NULL);
	KASSERT(node->vn_key.vk_key_len == new_key_len);
	node->vn_vnode = vp;
	node->vn_key = new_vcache_key;
	node = vcache_hash_lookup(&old_vcache_key, old_hash);
	KASSERT(node != NULL);
	KASSERT(node->vn_vnode == NULL);
	SLIST_REMOVE(&vcache.hashtab[old_hash & vcache.hashmask],
	    node, vcache_node, vn_hash);
	mutex_exit(&vcache.lock);
	pool_cache_put(vcache.pool, node);
}

/*
 * Remove a vnode / fs node pair from the cache.
 */
void
vcache_remove(struct mount *mp, const void *key, size_t key_len)
{
	uint32_t hash;
	struct vcache_key vcache_key;
	struct vcache_node *node;

	vcache_key.vk_mount = mp;
	vcache_key.vk_key = key;
	vcache_key.vk_key_len = key_len;
	hash = vcache_hash(&vcache_key);

	mutex_enter(&vcache.lock);
	node = vcache_hash_lookup(&vcache_key, hash);
	KASSERT(node != NULL);
	SLIST_REMOVE(&vcache.hashtab[hash & vcache.hashmask],
	    node, vcache_node, vn_hash);
	mutex_exit(&vcache.lock);
	pool_cache_put(vcache.pool, node);
}

/*
 * Update outstanding I/O count and do wakeup if requested.
 */
void
vwakeup(struct buf *bp)
{
	vnode_t *vp;

	if ((vp = bp->b_vp) == NULL)
		return;

	KASSERT(bp->b_objlock == vp->v_interlock);
	KASSERT(mutex_owned(bp->b_objlock));

	if (--vp->v_numoutput < 0)
		vnpanic(vp, "%s: neg numoutput, vp %p", __func__, vp);
	if (vp->v_numoutput == 0)
		cv_broadcast(&vp->v_cv);
}

/*
 * Test a vnode for being or becoming dead.  Returns one of:
 * EBUSY:  vnode is becoming dead, with "flags == VDEAD_NOWAIT" only.
 * ENOENT: vnode is dead.
 * 0:      otherwise.
 *
 * Whenever this function returns a non-zero value all future
 * calls will also return a non-zero value.
 */
int
vdead_check(struct vnode *vp, int flags)
{

	KASSERT(mutex_owned(vp->v_interlock));
	if (ISSET(vp->v_iflag, VI_XLOCK)) {
		if (ISSET(flags, VDEAD_NOWAIT))
			return EBUSY;
		vwait(vp, VI_XLOCK);
		KASSERT(ISSET(vp->v_iflag, VI_CLEAN));
	}
	if (ISSET(vp->v_iflag, VI_CLEAN))
		return ENOENT;
	return 0;
}

/*
 * Wait for a vnode (typically with VI_XLOCK set) to be cleaned or
 * recycled.
 */
static void
vwait(vnode_t *vp, int flags)
{

	KASSERT(mutex_owned(vp->v_interlock));
	KASSERT(vp->v_usecount != 0);

	while ((vp->v_iflag & flags) != 0)
		cv_wait(&vp->v_cv, vp->v_interlock);
}

int
vfs_drainvnodes(long target)
{
	int error;

	mutex_enter(&vnode_free_list_lock);

	while (numvnodes > target) {
		error = cleanvnode();
		if (error != 0)
			return error;
		mutex_enter(&vnode_free_list_lock);
	}

	mutex_exit(&vnode_free_list_lock);

	vcache_reinit();

	return 0;
}

void
vnpanic(vnode_t *vp, const char *fmt, ...)
{
	va_list ap;

#ifdef DIAGNOSTIC
	vprint(NULL, vp);
#endif
	va_start(ap, fmt);
	vpanic(fmt, ap);
	va_end(ap);
}
