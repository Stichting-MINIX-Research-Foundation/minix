/*	$NetBSD: nfs_kq.c,v 1.25 2011/10/24 11:43:30 hannken Exp $	*/

/*-
 * Copyright (c) 2002, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jaromir Dolecek.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: nfs_kq.c,v 1.25 2011/10/24 11:43:30 hannken Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/condvar.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/kmem.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/vnode.h>
#include <sys/unistd.h>
#include <sys/file.h>
#include <sys/kthread.h>

#include <uvm/uvm_extern.h>
#include <uvm/uvm.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#include <nfs/nfsnode.h>
#include <nfs/nfs_var.h>

struct kevq {
	SLIST_ENTRY(kevq)	kev_link;
	struct vnode		*vp;
	u_int			usecount;
	u_int			flags;
#define KEVQ_BUSY	0x01	/* currently being processed */
	struct timespec		omtime;	/* old modification time */
	struct timespec		octime;	/* old change time */
	nlink_t			onlink;	/* old number of references to file */
	kcondvar_t		cv;
};
SLIST_HEAD(kevqlist, kevq);

static kmutex_t nfskq_lock;
static struct lwp *nfskq_thread;
static kcondvar_t nfskq_cv;
static struct kevqlist kevlist = SLIST_HEAD_INITIALIZER(kevlist);
static bool nfskq_thread_exit;

void
nfs_kqinit(void)
{

	mutex_init(&nfskq_lock, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&nfskq_cv, "nfskqpw");
}

void
nfs_kqfini(void)
{

	if (nfskq_thread != NULL) {
		mutex_enter(&nfskq_lock);
		nfskq_thread_exit = true;
		cv_broadcast(&nfskq_cv);
		do {
			cv_wait(&nfskq_cv, &nfskq_lock);
		} while (nfskq_thread != NULL);
		mutex_exit(&nfskq_lock);
	}
	mutex_destroy(&nfskq_lock);
	cv_destroy(&nfskq_cv);
}

/*
 * This quite simplistic routine periodically checks for server changes
 * of any of the watched files every NFS_MINATTRTIMO/2 seconds.
 * Only changes in size, modification time, change time and nlinks
 * are being checked, everything else is ignored.
 * The routine only calls VOP_GETATTR() when it's likely it would get
 * some new data, i.e. when the vnode expires from attrcache. This
 * should give same result as periodically running stat(2) from userland,
 * while keeping CPU/network usage low, and still provide proper kevent
 * semantics.
 * The poller thread is created when first vnode is added to watch list,
 * and exits when the watch list is empty. The overhead of thread creation
 * isn't really important, neither speed of attach and detach of knote.
 */
/* ARGSUSED */
static void
nfs_kqpoll(void *arg)
{
	struct kevq *ke;
	struct vattr attr;
	struct lwp *l = curlwp;
	u_quad_t osize;

	mutex_enter(&nfskq_lock);
	while (!nfskq_thread_exit) {
		SLIST_FOREACH(ke, &kevlist, kev_link) {
			/* skip if still in attrcache */
			if (nfs_getattrcache(ke->vp, &attr) != ENOENT)
				continue;

			/*
			 * Mark entry busy, release lock and check
			 * for changes.
			 */
			ke->flags |= KEVQ_BUSY;
			mutex_exit(&nfskq_lock);

			/* save v_size, nfs_getattr() updates it */
			osize = ke->vp->v_size;

			memset(&attr, 0, sizeof(attr));
			vn_lock(ke->vp, LK_SHARED | LK_RETRY);
			(void) VOP_GETATTR(ke->vp, &attr, l->l_cred);
			VOP_UNLOCK(ke->vp);

			/* following is a bit fragile, but about best
			 * we can get */
			if (attr.va_size != osize) {
				int extended = (attr.va_size > osize);
				VN_KNOTE(ke->vp, NOTE_WRITE
					| (extended ? NOTE_EXTEND : 0));
				ke->omtime = attr.va_mtime;
			} else if (attr.va_mtime.tv_sec != ke->omtime.tv_sec
			    || attr.va_mtime.tv_nsec != ke->omtime.tv_nsec) {
				VN_KNOTE(ke->vp, NOTE_WRITE);
				ke->omtime = attr.va_mtime;
			}

			if (attr.va_ctime.tv_sec != ke->octime.tv_sec
			    || attr.va_ctime.tv_nsec != ke->octime.tv_nsec) {
				VN_KNOTE(ke->vp, NOTE_ATTRIB);
				ke->octime = attr.va_ctime;
			}

			if (attr.va_nlink != ke->onlink) {
				VN_KNOTE(ke->vp, NOTE_LINK);
				ke->onlink = attr.va_nlink;
			}

			mutex_enter(&nfskq_lock);
			ke->flags &= ~KEVQ_BUSY;
			cv_signal(&ke->cv);
		}

		if (SLIST_EMPTY(&kevlist)) {
			/* Nothing more to watch, exit */
			nfskq_thread = NULL;
			mutex_exit(&nfskq_lock);
			kthread_exit(0);
		}

		/* wait a while before checking for changes again */
		cv_timedwait(&nfskq_cv, &nfskq_lock,
		    NFS_MINATTRTIMO * hz / 2);
	}
	nfskq_thread = NULL;
	cv_broadcast(&nfskq_cv);
	mutex_exit(&nfskq_lock);
}

static void
filt_nfsdetach(struct knote *kn)
{
	struct vnode *vp = (struct vnode *)kn->kn_hook;
	struct kevq *ke;

	mutex_enter(vp->v_interlock);
	SLIST_REMOVE(&vp->v_klist, kn, knote, kn_selnext);
	mutex_exit(vp->v_interlock);

	/* Remove the vnode from watch list */
	mutex_enter(&nfskq_lock);
	SLIST_FOREACH(ke, &kevlist, kev_link) {
		if (ke->vp == vp) {
			while (ke->flags & KEVQ_BUSY) {
				cv_wait(&ke->cv, &nfskq_lock);
			}

			if (ke->usecount > 1) {
				/* keep, other kevents need this */
				ke->usecount--;
			} else {
				/* last user, g/c */
				cv_destroy(&ke->cv);
				SLIST_REMOVE(&kevlist, ke, kevq, kev_link);
				kmem_free(ke, sizeof(*ke));
			}
			break;
		}
	}
	mutex_exit(&nfskq_lock);
}

static int
filt_nfsread(struct knote *kn, long hint)
{
	struct vnode *vp = (struct vnode *)kn->kn_hook;
	int rv;

	/*
	 * filesystem is gone, so set the EOF flag and schedule
	 * the knote for deletion.
	 */
	switch (hint) {
	case NOTE_REVOKE:
		KASSERT(mutex_owned(vp->v_interlock));
		kn->kn_flags |= (EV_EOF | EV_ONESHOT);
		return (1);
	case 0:
		mutex_enter(vp->v_interlock);
		kn->kn_data = vp->v_size - ((file_t *)kn->kn_obj)->f_offset;
		rv = (kn->kn_data != 0);
		mutex_exit(vp->v_interlock);
		return rv;
	default:
		KASSERT(mutex_owned(vp->v_interlock));
		kn->kn_data = vp->v_size - ((file_t *)kn->kn_obj)->f_offset;
		return (kn->kn_data != 0);
	}
}

static int
filt_nfsvnode(struct knote *kn, long hint)
{
	struct vnode *vp = (struct vnode *)kn->kn_hook;
	int fflags;

	switch (hint) {
	case NOTE_REVOKE:
		KASSERT(mutex_owned(vp->v_interlock));
		kn->kn_flags |= EV_EOF;
		if ((kn->kn_sfflags & hint) != 0)
			kn->kn_fflags |= hint;
		return (1);
	case 0:
		mutex_enter(vp->v_interlock);
		fflags = kn->kn_fflags;
		mutex_exit(vp->v_interlock);
		break;
	default:
		KASSERT(mutex_owned(vp->v_interlock));
		if ((kn->kn_sfflags & hint) != 0)
			kn->kn_fflags |= hint;
		fflags = kn->kn_fflags;
		break;
	}

	return (fflags != 0);
}


static const struct filterops nfsread_filtops =
	{ 1, NULL, filt_nfsdetach, filt_nfsread };
static const struct filterops nfsvnode_filtops =
	{ 1, NULL, filt_nfsdetach, filt_nfsvnode };

int
nfs_kqfilter(void *v)
{
	struct vop_kqfilter_args /* {
		struct vnode	*a_vp;
		struct knote	*a_kn;
	} */ *ap = v;
	struct vnode *vp;
	struct knote *kn;
	struct kevq *ke;
	int error = 0;
	struct vattr attr;
	struct lwp *l = curlwp;

	vp = ap->a_vp;
	kn = ap->a_kn;
	switch (kn->kn_filter) {
	case EVFILT_READ:
		kn->kn_fop = &nfsread_filtops;
		break;
	case EVFILT_VNODE:
		kn->kn_fop = &nfsvnode_filtops;
		break;
	default:
		return (EINVAL);
	}

	/*
	 * Put the vnode to watched list.
	 */

	/*
	 * Fetch current attributes. It's only needed when the vnode
	 * is not watched yet, but we need to do this without lock
	 * held. This is likely cheap due to attrcache, so do it now.
	 */
	memset(&attr, 0, sizeof(attr));
	vn_lock(vp, LK_SHARED | LK_RETRY);
	(void) VOP_GETATTR(vp, &attr, l->l_cred);
	VOP_UNLOCK(vp);

	mutex_enter(&nfskq_lock);

	/* ensure the poller is running */
	if (!nfskq_thread) {
		error = kthread_create(PRI_NONE, 0, NULL, nfs_kqpoll,
		    NULL, &nfskq_thread, "nfskqpoll");
		if (error) {
			mutex_exit(&nfskq_lock);
			return error;
		}
	}

	SLIST_FOREACH(ke, &kevlist, kev_link) {
		if (ke->vp == vp)
			break;
	}

	if (ke) {
		/* already watched, so just bump usecount */
		ke->usecount++;
	} else {
		/* need a new one */
		ke = kmem_alloc(sizeof(*ke), KM_SLEEP);
		ke->vp = vp;
		ke->usecount = 1;
		ke->flags = 0;
		ke->omtime = attr.va_mtime;
		ke->octime = attr.va_ctime;
		ke->onlink = attr.va_nlink;
		cv_init(&ke->cv, "nfskqdet");
		SLIST_INSERT_HEAD(&kevlist, ke, kev_link);
	}

	mutex_enter(vp->v_interlock);
	SLIST_INSERT_HEAD(&vp->v_klist, kn, kn_selnext);
	kn->kn_hook = vp;
	mutex_exit(vp->v_interlock);

	/* kick the poller */
	cv_signal(&nfskq_cv);
	mutex_exit(&nfskq_lock);

	return (error);
}
