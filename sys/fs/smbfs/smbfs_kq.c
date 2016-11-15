/*	$NetBSD: smbfs_kq.c,v 1.26 2013/10/17 21:04:44 christos Exp $	*/

/*-
 * Copyright (c) 2003, 2008 The NetBSD Foundation, Inc.
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
__KERNEL_RCSID(0, "$NetBSD: smbfs_kq.c,v 1.26 2013/10/17 21:04:44 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/fcntl.h>
#include <sys/mount.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <sys/lockf.h>
#include <sys/kmem.h>
#include <sys/kthread.h>
#include <sys/file.h>
#include <sys/dirent.h>
#include <sys/mallocvar.h>

#include <machine/limits.h>

#include <uvm/uvm.h>
#include <uvm/uvm_extern.h>

#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_rq.h>

#include <fs/smbfs/smbfs.h>
#include <fs/smbfs/smbfs_node.h>
#include <fs/smbfs/smbfs_subr.h>

#include <miscfs/genfs/genfs.h>

/*
 * The maximum of outstanding SMB requests is 65536, since the
 * message id is 16bit. Don't consume all. If there is more
 * than 30k directory notify requests, fall back to polling mode.
 */
#define DNOTIFY_MAX	30000

struct kevq {
	SLIST_ENTRY(kevq)	kev_link;	/* link on kevlist */
	SLIST_ENTRY(kevq)	k_link;		/* link on poll/dn list */

	struct vnode		*vp;
	u_int			usecount;
	u_int			flags;
#define KEVQ_BUSY	0x01	/* currently being processed */
#define KEVQ_WANT	0x02	/* want to change this entry */
#define KEVQ_DNOT	0x04	/* kevent using NT directory change notify */
	struct timespec		omtime;	/* old modification time */
	struct timespec		octime;	/* old change time */
	nlink_t			onlink;	/* old number of references to file */
	struct smb_rq		*rq;	/* request structure */
};

static struct lwp *smbkql;		/* the kevent handler */
static struct smb_cred smbkq_scred;

static kmutex_t smbkq_lock;
					/* guard access to k*evlist */
static SLIST_HEAD(, kevq) kevlist = SLIST_HEAD_INITIALIZER(kevlist);
static SLIST_HEAD(, kevq) kplist = SLIST_HEAD_INITIALIZER(kplist);
static SLIST_HEAD(, kevq) kdnlist = SLIST_HEAD_INITIALIZER(kdnlist);

static int dnot_num = 0;		/* number of active dir notifications */
static u_int32_t kevs;

static void smbfskq_dirnotify(void *);

/*
 * This routine periodically checks server for change
 * of any of the watched files every SMBFS_MINATTRTIME/2 seconds.
 * Only changes in size, modification time, change time and nlinks
 * are being checked, everything else is ignored.
 * Directory events are watched via NT DIRECTORY CHANGE NOTIFY
 * if the server supports it.
 *
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
smbfs_kqpoll(void *arg)
{
	struct kevq *ke;
	struct vattr attr;
	int error = 0;
	struct lwp *l;
	u_quad_t osize;
	int needwake;

	l = curlwp;

	mutex_enter(&smbkq_lock);
	for(;;) {
		/* check all entries on poll list for changes */
		SLIST_FOREACH(ke, &kplist, k_link) {
			/* skip if still in attrcache */
			if (smbfs_attr_cachelookup(ke->vp, &attr) != ENOENT)
				continue;

			/*
			 * Mark entry busy, release lock and check
			 * for changes.
			 */
			ke->flags |= KEVQ_BUSY;
			mutex_exit(&smbkq_lock);

			/* save v_size, smbfs_getattr() updates it */
			osize = ke->vp->v_size;

			vn_lock(ke->vp, LK_SHARED | LK_RETRY);
			error = VOP_GETATTR(ke->vp, &attr, l->l_cred);
			VOP_UNLOCK(ke->vp);
			if (error) {
				/* relock and proceed with next */
				mutex_enter(&smbkq_lock);
				continue;
			}

			/* following is a bit fragile, but about best
			 * we can get */
			if (ke->vp->v_type != VDIR && attr.va_size != osize) {
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

			mutex_enter(&smbkq_lock);
			ke->flags &= ~KEVQ_BUSY;
			if (ke->flags & KEVQ_WANT) {
				ke->flags &= ~KEVQ_WANT;
				wakeup(ke);
			}
		}

		/* Exit if there are no more kevents to watch for */
		if (kevs == 0) {
			smbkql = NULL;
			break;
		}

		/* only wake periodically if poll list is nonempty */
		needwake = !SLIST_EMPTY(&kplist);

		/* wait a while before checking for changes again */
		if (SLIST_EMPTY(&kdnlist)) {
			error = mtsleep(smbkql, PSOCK, "smbkqidl",
				needwake ? (SMBFS_ATTRTIMO * hz / 2) : 0,
				&smbkq_lock);
		}

		if (!error) {
			/* woken up, check if any pending notifications */
			while (!SLIST_EMPTY(&kdnlist)) {
				int s, hint;

				s = splnet();
				ke = SLIST_FIRST(&kdnlist);
				SLIST_REMOVE_HEAD(&kdnlist, k_link);
				SLIST_NEXT(ke, k_link) = NULL;
				splx(s);

				/* drop lock while processing */
				mutex_exit(&smbkq_lock);

				/*
				 * Skip fetch if not yet setup.
				 */
				if (__predict_false(ke->rq == NULL))
					goto notifyrq;

				error = smbfs_smb_nt_dirnotify_fetch(ke->rq,
				    &hint);
				ke->rq = NULL;	/* rq deallocated by now */
				if (error) {
					/*
					 * if there is error, switch to
					 * polling for this one
					 */
					ke->flags &= KEVQ_DNOT;
					SLIST_INSERT_HEAD(&kplist, ke, k_link);
					continue;
				}

				VN_KNOTE(ke->vp, hint);

			notifyrq:
				/* reissue the notify request */
				(void) smbfs_smb_nt_dirnotify_setup(
				    VTOSMB(ke->vp),
				    &ke->rq, &smbkq_scred,
				    smbfskq_dirnotify, ke);

				/* reacquire the lock */
				mutex_enter(&smbkq_lock);
			}
		}
	}
	mutex_exit(&smbkq_lock);

	kthread_exit(0);
}

static void
smbfskq_dirnotify(void *arg)
{
	struct kevq *ke = arg;

	if (SLIST_NEXT(ke, k_link)) {
		/* already on notify list */
		return;
	}

	SLIST_INSERT_HEAD(&kdnlist, ke, k_link);
	wakeup(smbkql);
}

static void
filt_smbfsdetach(struct knote *kn)
{
	struct kevq *ke = (struct kevq *)kn->kn_hook;
	struct vnode *vp = ke->vp;
	struct smb_rq *rq = NULL;

	mutex_enter(vp->v_interlock);
	SLIST_REMOVE(&ke->vp->v_klist, kn, knote, kn_selnext);
	mutex_exit(vp->v_interlock);

	/* Remove the vnode from watch list */
	mutex_enter(&smbkq_lock);

	/* the handler does something to it, wait */
	while (ke->flags & KEVQ_BUSY) {
		ke->flags |= KEVQ_WANT;
		mtsleep(ke, PSOCK, "smbkqdw", 0, &smbkq_lock);
	}

	if (ke->usecount > 1) {
		/* keep, other kevents need this */
		ke->usecount--;
	} else {
		/* last user, g/c */
		if (ke->flags & KEVQ_DNOT) {
			dnot_num--;
			rq = ke->rq;

			/* If on dirnotify list, remove */
			if (SLIST_NEXT(ke, k_link))
				SLIST_REMOVE(&kdnlist, ke, kevq, k_link);
		} else
			SLIST_REMOVE(&kplist, ke, kevq, k_link);
		SLIST_REMOVE(&kevlist, ke, kevq, kev_link);
		kmem_free(ke, sizeof(*ke));
	}
	kevs--;

	mutex_exit(&smbkq_lock);

	/* If there was request still pending, cancel it now */
	if (rq) {
		smb_iod_removerq(rq);

		/*
		 * Explicitly cancel the request, so that server can
		 * free directory change notify resources.
		 */
		smbfs_smb_ntcancel(SSTOCP(rq->sr_share), rq->sr_mid,
		    &smbkq_scred);

		/* Free */
		smb_rq_done(rq);
	}
}

static int
filt_smbfsread(struct knote *kn, long hint)
{
	struct kevq *ke = (struct kevq *)kn->kn_hook;
	struct vnode *vp = ke->vp;
	int rv;

	if (hint == NOTE_REVOKE) {
		/*
		 * filesystem is gone, so set the EOF flag and schedule
		 * the knote for deletion.
		 */
		KASSERT(mutex_owned(vp->v_interlock));
		kn->kn_flags |= (EV_EOF | EV_ONESHOT);
		return (1);
	}

	/* There is no size info for directories */
	if (hint == 0) {
		mutex_enter(vp->v_interlock);
	} else {
		KASSERT(mutex_owned(vp->v_interlock));
	}
	if (vp->v_type == VDIR) {
		/*
		 * This is kind of hackish, since we need to
		 * set the flag when we are called with the hint
		 * to make confirming call from kern_event.c
		 * succeed too, but need to unset it afterwards
		 * so that the directory wouldn't stay flagged
		 * as changed.
		 * XXX perhaps just fail for directories?
		 */
		if (hint & NOTE_WRITE) {
			kn->kn_fflags |= NOTE_WRITE;
			rv = (1 * sizeof(struct dirent));
		} else if (hint == 0 && (kn->kn_fflags & NOTE_WRITE)) {
			kn->kn_fflags &= ~NOTE_WRITE;
			rv = (1 * sizeof(struct dirent));
		} else
			rv = 0;
	} else {
		kn->kn_data = vp->v_size - ((file_t *)kn->kn_obj)->f_offset;
       		rv = (kn->kn_data != 0);
	}
	if (hint == 0) {
		mutex_enter(vp->v_interlock);
	}

	return rv;
}

static int
filt_smbfsvnode(struct knote *kn, long hint)
{
	struct kevq *ke = (struct kevq *)kn->kn_hook;
	struct vnode *vp = ke->vp;

	switch (hint) {
	case NOTE_REVOKE:
		KASSERT(mutex_owned(vp->v_interlock));
		kn->kn_flags |= EV_EOF;
		if ((kn->kn_sfflags & hint) != 0)
			kn->kn_fflags |= hint;
		return (1);
	case 0:
		mutex_enter(vp->v_interlock);
		mutex_exit(vp->v_interlock);
		break;
	default:
		KASSERT(mutex_owned(vp->v_interlock));
		if ((kn->kn_sfflags & hint) != 0)
			kn->kn_fflags |= hint;
		break;
	}

	return (kn->kn_fflags != 0);
}

static const struct filterops smbfsread_filtops =
	{ 1, NULL, filt_smbfsdetach, filt_smbfsread };
static const struct filterops smbfsvnode_filtops =
	{ 1, NULL, filt_smbfsdetach, filt_smbfsvnode };

int
smbfs_kqfilter(void *v)
{
	struct vop_kqfilter_args /* {
		struct vnode	*a_vp;
		struct knote	*a_kn;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct knote *kn = ap->a_kn;
	struct kevq *ke, *ken;
	int error = 0;
	struct vattr attr;
	struct lwp *l = curlwp;	/* XXX */
	int dnot;
	struct smb_vc *vcp = SSTOVC(VTOSMB(vp)->n_mount->sm_share);
	static bool again;

	switch (kn->kn_filter) {
	case EVFILT_READ:
		kn->kn_fop = &smbfsread_filtops;
		break;
	case EVFILT_VNODE:
		kn->kn_fop = &smbfsvnode_filtops;
		break;
	default:
		return (EINVAL);
	}

	/* Find out if we can use directory change notify for this file */
	dnot = (vp->v_type == VDIR
	    && (SMB_CAPS(vcp) & SMB_CAP_NT_SMBS)
	    && dnot_num < DNOTIFY_MAX);

	/*
	 * Put the vnode to watched list.
	 */
	kevs++;

	/*
	 * Fetch current attributes. It's only needed when the vnode
	 * is not watched yet, but we need to do this without lock
	 * held. This is likely cheap due to attrcache, so do it now.
	 */
	memset(&attr, 0, sizeof(attr));
	vn_lock(vp, LK_SHARED | LK_RETRY);
	(void) VOP_GETATTR(vp, &attr, l->l_cred);
	VOP_UNLOCK(vp);

	/* ensure the handler is running */
	/* XXX this is unreliable. */
	if (!again) {
		mutex_init(&smbkq_lock, MUTEX_DEFAULT, IPL_NONE);
	}	
	if (!smbkql) {
		/* XXX very fishy */
		error = kthread_create(PRI_NONE, 0, NULL, smbfs_kqpoll,
		    NULL, &smbkql, "smbkq");
		smb_makescred(&smbkq_scred, smbkql, smbkql->l_cred);
		if (error) {
			kevs--;
			return (error);
		}
	}

	/*
	 * Allocate new kev. It's more probable it will be needed,
	 * and the malloc is cheaper than scanning possibly
	 * large kevlist list second time after malloc.
	 */
	ken = kmem_alloc(sizeof(*ken), KM_SLEEP);

	/* Check the list and insert new entry */
	mutex_enter(&smbkq_lock);
	SLIST_FOREACH(ke, &kevlist, kev_link) {
		if (ke->vp == vp)
			break;
	}

	if (ke) {
		/* already watched, so just bump usecount */
		ke->usecount++;
		kmem_free(ken, sizeof(*ken));
	} else {
		/* need a new one */
		memset(ken, 0, sizeof(*ken));
		ke = ken;
		ke->vp = vp;
		ke->usecount = 1;
		ke->flags = (dnot) ? KEVQ_DNOT : 0;
		ke->omtime = attr.va_mtime;
		ke->octime = attr.va_ctime;
		ke->onlink = attr.va_nlink;

		if (dnot) {
			int s;

			/*
			 * Add kevent to list of 'need attend' kevnets.
			 * The handler will pick it up and setup request
			 * appropriately.
			 */
			s = splnet();
			SLIST_INSERT_HEAD(&kdnlist, ke, k_link);
			splx(s);
			dnot_num++;
		} else {
			/* add to poll list */
			SLIST_INSERT_HEAD(&kplist, ke, k_link);
		}

		SLIST_INSERT_HEAD(&kevlist, ke, kev_link);

		/* kick the handler */
		wakeup(smbkql);
	}

	mutex_enter(vp->v_interlock);
	SLIST_INSERT_HEAD(&vp->v_klist, kn, kn_selnext);
	kn->kn_hook = ke;
	mutex_exit(vp->v_interlock);

	mutex_exit(&smbkq_lock);

	return (0);
}
