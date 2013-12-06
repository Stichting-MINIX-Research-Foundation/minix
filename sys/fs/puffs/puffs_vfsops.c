/*	$NetBSD: puffs_vfsops.c,v 1.107 2013/01/16 21:10:14 pooka Exp $	*/

/*
 * Copyright (c) 2005, 2006  Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by the
 * Google Summer of Code program and the Ulla Tuominen Foundation.
 * The Google SoC project was mentored by Bill Studenmund.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: puffs_vfsops.c,v 1.107 2013/01/16 21:10:14 pooka Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/extattr.h>
#include <sys/queue.h>
#include <sys/vnode.h>
#include <sys/dirent.h>
#include <sys/kauth.h>
#include <sys/proc.h>
#include <sys/module.h>
#include <sys/kthread.h>

#include <uvm/uvm.h>

#include <dev/putter/putter_sys.h>

#include <miscfs/genfs/genfs.h>

#include <fs/puffs/puffs_msgif.h>
#include <fs/puffs/puffs_sys.h>

#include <lib/libkern/libkern.h>

#include <nfs/nfsproto.h> /* for fh sizes */

MODULE(MODULE_CLASS_VFS, puffs, "putter");

VFS_PROTOS(puffs_vfsop);

#ifndef PUFFS_PNODEBUCKETS
#define PUFFS_PNODEBUCKETS 256
#endif
#ifndef PUFFS_MAXPNODEBUCKETS
#define PUFFS_MAXPNODEBUCKETS 8192
#endif
int puffs_pnodebuckets_default = PUFFS_PNODEBUCKETS;
int puffs_maxpnodebuckets = PUFFS_MAXPNODEBUCKETS;

#define BUCKETALLOC(a) (sizeof(struct puffs_pnode_hashlist *) * (a))

static struct putter_ops puffs_putter = {
	.pop_getout	= puffs_msgif_getout,
	.pop_releaseout	= puffs_msgif_releaseout,
	.pop_waitcount	= puffs_msgif_waitcount,
	.pop_dispatch	= puffs_msgif_dispatch,
	.pop_close	= puffs_msgif_close,
};

/*
 * Try to ensure data structures used by the puffs protocol
 * do not unexpectedly change.
 */
#if defined(__i386__) && defined(__ELF__)
CTASSERT(sizeof(struct puffs_kargs) == 3928);
CTASSERT(sizeof(struct vattr) == 136);
CTASSERT(sizeof(struct puffs_req) == 44);
#endif

int
puffs_vfsop_mount(struct mount *mp, const char *path, void *data,
	size_t *data_len)
{
	struct puffs_mount *pmp = NULL;
	struct puffs_kargs *args;
	char fstype[_VFS_NAMELEN];
	char *p;
	int error = 0, i;
	pid_t mntpid = curlwp->l_proc->p_pid;

	if (*data_len < sizeof *args)
		return EINVAL;

	if (mp->mnt_flag & MNT_GETARGS) {
		pmp = MPTOPUFFSMP(mp);
		*(struct puffs_kargs *)data = pmp->pmp_args;
		*data_len = sizeof *args;
		return 0;
	}

	/* update is not supported currently */
	if (mp->mnt_flag & MNT_UPDATE)
		return EOPNOTSUPP;

	/*
	 * We need the file system name
	 */
	if (!data)
		return EINVAL;

	args = (struct puffs_kargs *)data;

	if (args->pa_vers != PUFFSVERSION) {
		printf("puffs_mount: development version mismatch: "
		    "kernel %d, lib %d\n", PUFFSVERSION, args->pa_vers);
		error = EINVAL;
		goto out;
	}

	if ((args->pa_flags & ~PUFFS_KFLAG_MASK) != 0) {
		printf("puffs_mount: invalid KFLAGs 0x%x\n", args->pa_flags);
		error = EINVAL;
		goto out;
	}
	if ((args->pa_fhflags & ~PUFFS_FHFLAG_MASK) != 0) {
		printf("puffs_mount: invalid FHFLAGs 0x%x\n", args->pa_fhflags);
		error = EINVAL;
		goto out;
	}

	for (i = 0; i < __arraycount(args->pa_spare); i++) {
		if (args->pa_spare[i] != 0) {
			printf("puffs_mount: pa_spare[%d] = 0x%x\n",
			    i, args->pa_spare[i]);
			error = EINVAL;
			goto out;
		}
	}

	/* use dummy value for passthrough */
	if (args->pa_fhflags & PUFFS_FHFLAG_PASSTHROUGH)
		args->pa_fhsize = sizeof(struct fid);

	/* sanitize file handle length */
	if (PUFFS_TOFHSIZE(args->pa_fhsize) > FHANDLE_SIZE_MAX) {
		printf("puffs_mount: handle size %zu too large\n",
		    args->pa_fhsize);
		error = EINVAL;
		goto out;
	}
	/* sanity check file handle max sizes */
	if (args->pa_fhsize && args->pa_fhflags & PUFFS_FHFLAG_PROTOMASK) {
		size_t kfhsize = PUFFS_TOFHSIZE(args->pa_fhsize);

		if (args->pa_fhflags & PUFFS_FHFLAG_NFSV2) {
			if (NFSX_FHTOOBIG_P(kfhsize, 0)) {
				printf("puffs_mount: fhsize larger than "
				    "NFSv2 max %d\n",
				    PUFFS_FROMFHSIZE(NFSX_V2FH));
				error = EINVAL;
				goto out;
			}
		}

		if (args->pa_fhflags & PUFFS_FHFLAG_NFSV3) {
			if (NFSX_FHTOOBIG_P(kfhsize, 1)) {
				printf("puffs_mount: fhsize larger than "
				    "NFSv3 max %d\n",
				    PUFFS_FROMFHSIZE(NFSX_V3FHMAX));
				error = EINVAL;
				goto out;
			}
		}
	}

	/* don't allow non-printing characters (like my sweet umlauts.. snif) */
	args->pa_typename[sizeof(args->pa_typename)-1] = '\0';
	for (p = args->pa_typename; *p; p++)
		if (*p < ' ' || *p > '~')
			*p = '.';

	args->pa_mntfromname[sizeof(args->pa_mntfromname)-1] = '\0';
	for (p = args->pa_mntfromname; *p; p++)
		if (*p < ' ' || *p > '~')
			*p = '.';

	/* build real name */
	(void)strlcpy(fstype, PUFFS_TYPEPREFIX, sizeof(fstype));
	(void)strlcat(fstype, args->pa_typename, sizeof(fstype));

	/* inform user server if it got the max request size it wanted */
	if (args->pa_maxmsglen == 0 || args->pa_maxmsglen > PUFFS_MSG_MAXSIZE)
		args->pa_maxmsglen = PUFFS_MSG_MAXSIZE;
	else if (args->pa_maxmsglen < 2*PUFFS_MSGSTRUCT_MAX)
		args->pa_maxmsglen = 2*PUFFS_MSGSTRUCT_MAX;

	(void)strlcpy(args->pa_typename, fstype, sizeof(args->pa_typename));

	if (args->pa_nhashbuckets == 0)
		args->pa_nhashbuckets = puffs_pnodebuckets_default;
	if (args->pa_nhashbuckets < 1)
		args->pa_nhashbuckets = 1;
	if (args->pa_nhashbuckets > PUFFS_MAXPNODEBUCKETS) {
		args->pa_nhashbuckets = puffs_maxpnodebuckets;
		printf("puffs_mount: using %d hash buckets. "
		    "adjust puffs_maxpnodebuckets for more\n",
		    puffs_maxpnodebuckets);
	}

	error = set_statvfs_info(path, UIO_USERSPACE, args->pa_mntfromname,
	    UIO_SYSSPACE, fstype, mp, curlwp);
	if (error)
		goto out;
	mp->mnt_stat.f_iosize = DEV_BSIZE;
	mp->mnt_stat.f_namemax = args->pa_svfsb.f_namemax;

	/*
	 * We can't handle the VFS_STATVFS() mount_domount() does
	 * after VFS_MOUNT() because we'd deadlock, so handle it
	 * here already.
	 */
	copy_statvfs_info(&args->pa_svfsb, mp);
	(void)memcpy(&mp->mnt_stat, &args->pa_svfsb, sizeof(mp->mnt_stat));

	KASSERT(curlwp != uvm.pagedaemon_lwp);
	pmp = kmem_zalloc(sizeof(struct puffs_mount), KM_SLEEP);

	mp->mnt_fs_bshift = DEV_BSHIFT;
	mp->mnt_dev_bshift = DEV_BSHIFT;
	mp->mnt_flag &= ~MNT_LOCAL; /* we don't really know, so ... */
	mp->mnt_data = pmp;

#if 0
	/*
	 * XXX: puffs code is MPSAFE.  However, VFS really isn't.
	 * Currently, there is nothing which protects an inode from
	 * reclaim while there are threads inside the file system.
	 * This means that in the event of a server crash, an MPSAFE
	 * mount is likely to end up accessing invalid memory.  For the
	 * non-mpsafe case, the kernel lock, general structure of
	 * puffs and pmp_refcount protect the threads during escape.
	 *
	 * Fixing this will require:
	 *  a) fixing vfs
	 * OR
	 *  b) adding a small sleep to puffs_msgif_close() between
	 *     userdead() and dounmount().
	 *     (well, this isn't really a fix, but would solve
	 *     99.999% of the race conditions).
	 *
	 * Also, in the event of "b", unmount -f should be used,
	 * like with any other file system, sparingly and only when
	 * it is "known" to be safe.
	 */
	mp->mnt_iflags |= IMNT_MPSAFE;
#endif

	pmp->pmp_status = PUFFSTAT_MOUNTING;
	pmp->pmp_mp = mp;
	pmp->pmp_msg_maxsize = args->pa_maxmsglen;
	pmp->pmp_args = *args;

	pmp->pmp_npnodehash = args->pa_nhashbuckets;
	pmp->pmp_pnodehash = kmem_alloc(BUCKETALLOC(pmp->pmp_npnodehash),
	    KM_SLEEP);
	for (i = 0; i < pmp->pmp_npnodehash; i++)
		LIST_INIT(&pmp->pmp_pnodehash[i]);
	LIST_INIT(&pmp->pmp_newcookie);

	/*
	 * Inform the fileops processing code that we have a mountpoint.
	 * If it doesn't know about anyone with our pid/fd having the
	 * device open, punt
	 */
	if ((pmp->pmp_pi
	    = putter_attach(mntpid, args->pa_fd, pmp, &puffs_putter)) == NULL) {
		error = ENOENT;
		goto out;
	}

	/* XXX: check parameters */
	pmp->pmp_root_cookie = args->pa_root_cookie;
	pmp->pmp_root_vtype = args->pa_root_vtype;
	pmp->pmp_root_vsize = args->pa_root_vsize;
	pmp->pmp_root_rdev = args->pa_root_rdev;
	pmp->pmp_docompat = args->pa_time32;

	mutex_init(&pmp->pmp_lock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&pmp->pmp_sopmtx, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&pmp->pmp_msg_waiter_cv, "puffsget");
	cv_init(&pmp->pmp_refcount_cv, "puffsref");
	cv_init(&pmp->pmp_unmounting_cv, "puffsum");
	cv_init(&pmp->pmp_sopcv, "puffsop");
	TAILQ_INIT(&pmp->pmp_msg_touser);
	TAILQ_INIT(&pmp->pmp_msg_replywait);
	TAILQ_INIT(&pmp->pmp_sopfastreqs);
	TAILQ_INIT(&pmp->pmp_sopnodereqs);

	if ((error = kthread_create(PRI_NONE, KTHREAD_MPSAFE, NULL,
	    puffs_sop_thread, pmp, NULL, "puffsop")) != 0)
		goto out;
	pmp->pmp_sopthrcount = 1;

	DPRINTF(("puffs_mount: mount point at %p, puffs specific at %p\n",
	    mp, MPTOPUFFSMP(mp)));

	vfs_getnewfsid(mp);

 out:
	if (error && pmp && pmp->pmp_pi)
		putter_detach(pmp->pmp_pi);
	if (error && pmp && pmp->pmp_pnodehash)
		kmem_free(pmp->pmp_pnodehash, BUCKETALLOC(pmp->pmp_npnodehash));
	if (error && pmp)
		kmem_free(pmp, sizeof(struct puffs_mount));
	return error;
}

int
puffs_vfsop_start(struct mount *mp, int flags)
{
	struct puffs_mount *pmp = MPTOPUFFSMP(mp);

	KASSERT(pmp->pmp_status == PUFFSTAT_MOUNTING);
	pmp->pmp_status = PUFFSTAT_RUNNING;

	return 0;
}

int
puffs_vfsop_unmount(struct mount *mp, int mntflags)
{
	PUFFS_MSG_VARS(vfs, unmount);
	struct puffs_mount *pmp;
	int error, force;

	error = 0;
	force = mntflags & MNT_FORCE;
	pmp = MPTOPUFFSMP(mp);

	DPRINTF(("puffs_unmount: detach filesystem from vfs, current "
	    "status 0x%x\n", pmp->pmp_status));

	/*
	 * flush all the vnodes.  VOP_RECLAIM() takes care that the
	 * root vnode does not get flushed until unmount.  The
	 * userspace root node cookie is stored in the mount
	 * structure, so we can always re-instantiate a root vnode,
	 * should userspace unmount decide it doesn't want to
	 * cooperate.
	 */
	error = vflush(mp, NULLVP, force ? FORCECLOSE : 0);
	if (error)
		goto out;

	/*
	 * If we are not DYING, we should ask userspace's opinion
	 * about the situation
	 */
	mutex_enter(&pmp->pmp_lock);
	if (pmp->pmp_status != PUFFSTAT_DYING) {
		pmp->pmp_unmounting = 1;
		mutex_exit(&pmp->pmp_lock);

		PUFFS_MSG_ALLOC(vfs, unmount);
		puffs_msg_setinfo(park_unmount,
		    PUFFSOP_VFS, PUFFS_VFS_UNMOUNT, NULL);
		unmount_msg->pvfsr_flags = mntflags;

		PUFFS_MSG_ENQUEUEWAIT(pmp, park_unmount, error);
		PUFFS_MSG_RELEASE(unmount);

		error = checkerr(pmp, error, __func__);
		DPRINTF(("puffs_unmount: error %d force %d\n", error, force));

		mutex_enter(&pmp->pmp_lock);
		pmp->pmp_unmounting = 0;
		cv_broadcast(&pmp->pmp_unmounting_cv);
	}

	/*
	 * if userspace cooperated or we really need to die,
	 * screw what userland thinks and just die.
	 */
	if (error == 0 || force) {
		struct puffs_sopreq *psopr;

		/* tell waiters & other resources to go unwait themselves */
		puffs_userdead(pmp);
		putter_detach(pmp->pmp_pi);

		/*
		 * Wait until there are no more users for the mount resource.
		 * Notice that this is hooked against transport_close
		 * and return from touser.  In an ideal world, it would
		 * be hooked against final return from all operations.
		 * But currently it works well enough, since nobody
		 * does weird blocking voodoo after return from touser().
		 */
		while (pmp->pmp_refcount != 0)
			cv_wait(&pmp->pmp_refcount_cv, &pmp->pmp_lock);
		mutex_exit(&pmp->pmp_lock);

		/*
		 * Release kernel thread now that there is nothing
		 * it would be wanting to lock.
		 */
		KASSERT(curlwp != uvm.pagedaemon_lwp);
		psopr = kmem_alloc(sizeof(*psopr), KM_SLEEP);
		psopr->psopr_sopreq = PUFFS_SOPREQSYS_EXIT;
		mutex_enter(&pmp->pmp_sopmtx);
		if (pmp->pmp_sopthrcount == 0) {
			mutex_exit(&pmp->pmp_sopmtx);
			kmem_free(psopr, sizeof(*psopr));
			mutex_enter(&pmp->pmp_sopmtx);
			KASSERT(pmp->pmp_sopthrcount == 0);
		} else {
			TAILQ_INSERT_TAIL(&pmp->pmp_sopfastreqs,
			    psopr, psopr_entries);
			cv_signal(&pmp->pmp_sopcv);
		}
		while (pmp->pmp_sopthrcount > 0)
			cv_wait(&pmp->pmp_sopcv, &pmp->pmp_sopmtx);
		mutex_exit(&pmp->pmp_sopmtx);

		/* free resources now that we hopefully have no waiters left */
		cv_destroy(&pmp->pmp_unmounting_cv);
		cv_destroy(&pmp->pmp_refcount_cv);
		cv_destroy(&pmp->pmp_msg_waiter_cv);
		cv_destroy(&pmp->pmp_sopcv);
		mutex_destroy(&pmp->pmp_lock);
		mutex_destroy(&pmp->pmp_sopmtx);

		kmem_free(pmp->pmp_pnodehash, BUCKETALLOC(pmp->pmp_npnodehash));
		kmem_free(pmp, sizeof(struct puffs_mount));
		error = 0;
	} else {
		mutex_exit(&pmp->pmp_lock);
	}

 out:
	DPRINTF(("puffs_unmount: return %d\n", error));
	return error;
}

/*
 * This doesn't need to travel to userspace
 */
int
puffs_vfsop_root(struct mount *mp, struct vnode **vpp)
{
	struct puffs_mount *pmp = MPTOPUFFSMP(mp);
	int rv;

	rv = puffs_cookie2vnode(pmp, pmp->pmp_root_cookie, 1, 1, vpp);
	KASSERT(rv != PUFFS_NOSUCHCOOKIE);
	return rv;
}

int
puffs_vfsop_statvfs(struct mount *mp, struct statvfs *sbp)
{
	PUFFS_MSG_VARS(vfs, statvfs);
	struct puffs_mount *pmp;
	int error = 0;

	pmp = MPTOPUFFSMP(mp);

	/*
	 * If we are mounting, it means that the userspace counterpart
	 * is calling mount(2), but mount(2) also calls statvfs.  So
	 * requesting statvfs from userspace would mean a deadlock.
	 * Compensate.
	 */
	if (__predict_false(pmp->pmp_status == PUFFSTAT_MOUNTING))
		return EINPROGRESS;

	PUFFS_MSG_ALLOC(vfs, statvfs);
	puffs_msg_setinfo(park_statvfs, PUFFSOP_VFS, PUFFS_VFS_STATVFS, NULL);

	PUFFS_MSG_ENQUEUEWAIT(pmp, park_statvfs, error);
	error = checkerr(pmp, error, __func__);
	statvfs_msg->pvfsr_sb.f_iosize = DEV_BSIZE;

	/*
	 * Try to produce a sensible result even in the event
	 * of userspace error.
	 *
	 * XXX: cache the copy in non-error case
	 */
	if (!error) {
		copy_statvfs_info(&statvfs_msg->pvfsr_sb, mp);
		(void)memcpy(sbp, &statvfs_msg->pvfsr_sb,
		    sizeof(struct statvfs));
	} else {
		copy_statvfs_info(sbp, mp);
	}

	PUFFS_MSG_RELEASE(statvfs);
	return error;
}

static int
pageflush(struct mount *mp, kauth_cred_t cred, int waitfor)
{
	struct puffs_node *pn;
	struct vnode *vp, *mvp;
	int error, rv, fsyncwait;

	error = 0;
	fsyncwait = (waitfor == MNT_WAIT) ? FSYNC_WAIT : 0;

	/* Allocate a marker vnode. */
	mvp = vnalloc(mp);

	/*
	 * Sync all cached data from regular vnodes (which are not
	 * currently locked, see below).  After this we call VFS_SYNC
	 * for the fs server, which should handle data and metadata for
	 * all the nodes it knows to exist.
	 */
	mutex_enter(&mntvnode_lock);
 loop:
	for (vp = TAILQ_FIRST(&mp->mnt_vnodelist); vp; vp = vunmark(mvp)) {
		vmark(mvp, vp);
		if (vp->v_mount != mp || vismarker(vp))
			continue;

		mutex_enter(vp->v_interlock);
		pn = VPTOPP(vp);
		if (vp->v_type != VREG || UVM_OBJ_IS_CLEAN(&vp->v_uobj)) {
			mutex_exit(vp->v_interlock);
			continue;
		}

		mutex_exit(&mntvnode_lock);

		/*
		 * Here we try to get a reference to the vnode and to
		 * lock it.  This is mostly cargo-culted, but I will
		 * offer an explanation to why I believe this might
		 * actually do the right thing.
		 *
		 * If the vnode is a goner, we quite obviously don't need
		 * to sync it.
		 *
		 * If the vnode was busy, we don't need to sync it because
		 * this is never called with MNT_WAIT except from
		 * dounmount(), when we are wait-flushing all the dirty
		 * vnodes through other routes in any case.  So there,
		 * sync() doesn't actually sync.  Happy now?
		 */
		rv = vget(vp, LK_EXCLUSIVE | LK_NOWAIT);
		if (rv) {
			mutex_enter(&mntvnode_lock);
			if (rv == ENOENT) {
				(void)vunmark(mvp);
				goto loop;
			}
			continue;
		}

		/* hmm.. is the FAF thing entirely sensible? */
		if (waitfor == MNT_LAZY) {
			mutex_enter(vp->v_interlock);
			pn->pn_stat |= PNODE_FAF;
			mutex_exit(vp->v_interlock);
		}
		rv = VOP_FSYNC(vp, cred, fsyncwait, 0, 0);
		if (waitfor == MNT_LAZY) {
			mutex_enter(vp->v_interlock);
			pn->pn_stat &= ~PNODE_FAF;
			mutex_exit(vp->v_interlock);
		}
		if (rv)
			error = rv;
		vput(vp);
		mutex_enter(&mntvnode_lock);
	}
	mutex_exit(&mntvnode_lock);
	vnfree(mvp);

	return error;
}

int
puffs_vfsop_sync(struct mount *mp, int waitfor, struct kauth_cred *cred)
{
	PUFFS_MSG_VARS(vfs, sync);
	struct puffs_mount *pmp = MPTOPUFFSMP(mp);
	int error, rv;

	error = pageflush(mp, cred, waitfor);

	/* sync fs */
	PUFFS_MSG_ALLOC(vfs, sync);
	sync_msg->pvfsr_waitfor = waitfor;
	puffs_credcvt(&sync_msg->pvfsr_cred, cred);
	puffs_msg_setinfo(park_sync, PUFFSOP_VFS, PUFFS_VFS_SYNC, NULL);

	PUFFS_MSG_ENQUEUEWAIT(pmp, park_sync, rv);
	rv = checkerr(pmp, rv, __func__);
	if (rv)
		error = rv;

	PUFFS_MSG_RELEASE(sync);
	return error;
}

int
puffs_vfsop_fhtovp(struct mount *mp, struct fid *fhp, struct vnode **vpp)
{
	PUFFS_MSG_VARS(vfs, fhtonode);
	struct puffs_mount *pmp = MPTOPUFFSMP(mp);
	struct vnode *vp;
	void *fhdata;
	size_t argsize, fhlen;
	int error;

	if (pmp->pmp_args.pa_fhsize == 0)
		return EOPNOTSUPP;

	if (pmp->pmp_args.pa_fhflags & PUFFS_FHFLAG_PASSTHROUGH) {
		fhlen = fhp->fid_len;
		fhdata = fhp;
	} else {
		fhlen = PUFFS_FROMFHSIZE(fhp->fid_len);
		fhdata = fhp->fid_data;

		if (pmp->pmp_args.pa_fhflags & PUFFS_FHFLAG_DYNAMIC) {
			if (pmp->pmp_args.pa_fhsize < fhlen)
				return EINVAL;
		} else {
			if (pmp->pmp_args.pa_fhsize != fhlen)
				return EINVAL;
		}
	}

	argsize = sizeof(struct puffs_vfsmsg_fhtonode) + fhlen;
	puffs_msgmem_alloc(argsize, &park_fhtonode, (void *)&fhtonode_msg, 1);
	fhtonode_msg->pvfsr_dsize = fhlen;
	memcpy(fhtonode_msg->pvfsr_data, fhdata, fhlen);
	puffs_msg_setinfo(park_fhtonode, PUFFSOP_VFS, PUFFS_VFS_FHTOVP, NULL);

	PUFFS_MSG_ENQUEUEWAIT(pmp, park_fhtonode, error);
	error = checkerr(pmp, error, __func__);
	if (error)
		goto out;

	error = puffs_cookie2vnode(pmp, fhtonode_msg->pvfsr_fhcookie, 1,1,&vp);
	DPRINTF(("puffs_fhtovp: got cookie %p, existing vnode %p\n",
	    fhtonode_msg->pvfsr_fhcookie, vp));
	if (error == PUFFS_NOSUCHCOOKIE) {
		error = puffs_getvnode(mp, fhtonode_msg->pvfsr_fhcookie,
		    fhtonode_msg->pvfsr_vtype, fhtonode_msg->pvfsr_size,
		    fhtonode_msg->pvfsr_rdev, &vp);
		if (error)
			goto out;
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	} else if (error) {
		goto out;
	}

	*vpp = vp;
 out:
	puffs_msgmem_release(park_fhtonode);
	return error;
}

int
puffs_vfsop_vptofh(struct vnode *vp, struct fid *fhp, size_t *fh_size)
{
	PUFFS_MSG_VARS(vfs, nodetofh);
	struct puffs_mount *pmp = MPTOPUFFSMP(vp->v_mount);
	size_t argsize, fhlen;
	int error;

	if (pmp->pmp_args.pa_fhsize == 0)
		return EOPNOTSUPP;

	/* if file handles are static len, we can test len immediately */
	if (((pmp->pmp_args.pa_fhflags & PUFFS_FHFLAG_DYNAMIC) == 0)
	    && ((pmp->pmp_args.pa_fhflags & PUFFS_FHFLAG_PASSTHROUGH) == 0)
	    && (PUFFS_FROMFHSIZE(*fh_size) < pmp->pmp_args.pa_fhsize)) {
		*fh_size = PUFFS_TOFHSIZE(pmp->pmp_args.pa_fhsize);
		return E2BIG;
	}

	if (pmp->pmp_args.pa_fhflags & PUFFS_FHFLAG_PASSTHROUGH)
		fhlen = *fh_size;
	else
		fhlen = PUFFS_FROMFHSIZE(*fh_size);

	argsize = sizeof(struct puffs_vfsmsg_nodetofh) + fhlen;
	puffs_msgmem_alloc(argsize, &park_nodetofh, (void *)&nodetofh_msg, 1);
	nodetofh_msg->pvfsr_fhcookie = VPTOPNC(vp);
	nodetofh_msg->pvfsr_dsize = fhlen;
	puffs_msg_setinfo(park_nodetofh, PUFFSOP_VFS, PUFFS_VFS_VPTOFH, NULL);

	PUFFS_MSG_ENQUEUEWAIT(pmp, park_nodetofh, error);
	error = checkerr(pmp, error, __func__);

	if (pmp->pmp_args.pa_fhflags & PUFFS_FHFLAG_PASSTHROUGH)
		fhlen = nodetofh_msg->pvfsr_dsize;
	else if (pmp->pmp_args.pa_fhflags & PUFFS_FHFLAG_DYNAMIC)
		fhlen = PUFFS_TOFHSIZE(nodetofh_msg->pvfsr_dsize);
	else
		fhlen = PUFFS_TOFHSIZE(pmp->pmp_args.pa_fhsize);

	if (error) {
		if (error == E2BIG)
			*fh_size = fhlen;
		goto out;
	}

	if (fhlen > FHANDLE_SIZE_MAX) {
		puffs_senderr(pmp, PUFFS_ERR_VPTOFH, E2BIG,
		    "file handle too big", VPTOPNC(vp));
		error = EPROTO;
		goto out;
	}

	if (*fh_size < fhlen) {
		*fh_size = fhlen;
		error = E2BIG;
		goto out;
	}
	*fh_size = fhlen;

	if (fhp) {
		if (pmp->pmp_args.pa_fhflags & PUFFS_FHFLAG_PASSTHROUGH) {
			memcpy(fhp, nodetofh_msg->pvfsr_data, fhlen);
		} else {
			fhp->fid_len = *fh_size;
			memcpy(fhp->fid_data, nodetofh_msg->pvfsr_data,
			    nodetofh_msg->pvfsr_dsize);
		}
	}

 out:
	puffs_msgmem_release(park_nodetofh);
	return error;
}

void
puffs_vfsop_init(void)
{

	/* some checks depend on this */
	KASSERT(VNOVAL == VSIZENOTSET);

	pool_init(&puffs_pnpool, sizeof(struct puffs_node), 0, 0, 0,
	    "puffpnpl", &pool_allocator_nointr, IPL_NONE);
	pool_init(&puffs_vapool, sizeof(struct vattr), 0, 0, 0,
	    "puffvapl", &pool_allocator_nointr, IPL_NONE);
	puffs_msgif_init();
}

void
puffs_vfsop_done(void)
{

	puffs_msgif_destroy();
	pool_destroy(&puffs_pnpool);
	pool_destroy(&puffs_vapool);
}

int
puffs_vfsop_snapshot(struct mount *mp, struct vnode *vp, struct timespec *ts)
{

	return EOPNOTSUPP;
}

int
puffs_vfsop_extattrctl(struct mount *mp, int cmd, struct vnode *vp,
	int attrnamespace, const char *attrname)
{
	PUFFS_MSG_VARS(vfs, extattrctl);
	struct puffs_mount *pmp = MPTOPUFFSMP(mp);
	struct puffs_node *pnp;
	puffs_cookie_t pnc;
	int error, flags;

	if (vp) {
		/* doesn't make sense for puffs servers */
		if (vp->v_mount != mp)
			return EXDEV;
		pnp = vp->v_data;
		pnc = pnp->pn_cookie;
		flags = PUFFS_EXTATTRCTL_HASNODE;
	} else {
		pnp = pnc = NULL;
		flags = 0;
	}

	PUFFS_MSG_ALLOC(vfs, extattrctl);
	extattrctl_msg->pvfsr_cmd = cmd;
	extattrctl_msg->pvfsr_attrnamespace = attrnamespace;
	extattrctl_msg->pvfsr_flags = flags;
	if (attrname) {
		strlcpy(extattrctl_msg->pvfsr_attrname, attrname,
		    sizeof(extattrctl_msg->pvfsr_attrname));
		extattrctl_msg->pvfsr_flags |= PUFFS_EXTATTRCTL_HASATTRNAME;
	}
	puffs_msg_setinfo(park_extattrctl,
	    PUFFSOP_VFS, PUFFS_VFS_EXTATTRCTL, pnc);

	puffs_msg_enqueue(pmp, park_extattrctl);
	if (vp) {
		mutex_enter(&pnp->pn_mtx);
		puffs_referencenode(pnp);
		mutex_exit(&pnp->pn_mtx);
		VOP_UNLOCK(vp);
	}
	error = puffs_msg_wait2(pmp, park_extattrctl, pnp, NULL);
	PUFFS_MSG_RELEASE(extattrctl);
	if (vp) {
		puffs_releasenode(pnp);
	}

	return checkerr(pmp, error, __func__);
}

const struct vnodeopv_desc * const puffs_vnodeopv_descs[] = {
	&puffs_vnodeop_opv_desc,
	&puffs_specop_opv_desc,
	&puffs_fifoop_opv_desc,
	&puffs_msgop_opv_desc,
	NULL,
};

struct vfsops puffs_vfsops = {
	MOUNT_PUFFS,
	sizeof (struct puffs_kargs),
	puffs_vfsop_mount,		/* mount	*/
	puffs_vfsop_start,		/* start	*/
	puffs_vfsop_unmount,		/* unmount	*/
	puffs_vfsop_root,		/* root		*/
	(void *)eopnotsupp,		/* quotactl	*/
	puffs_vfsop_statvfs,		/* statvfs	*/
	puffs_vfsop_sync,		/* sync		*/
	(void *)eopnotsupp,		/* vget		*/
	puffs_vfsop_fhtovp,		/* fhtovp	*/
	puffs_vfsop_vptofh,		/* vptofh	*/
	puffs_vfsop_init,		/* init		*/
	NULL,				/* reinit	*/
	puffs_vfsop_done,		/* done		*/
	NULL,				/* mountroot	*/
	puffs_vfsop_snapshot,		/* snapshot	*/
	puffs_vfsop_extattrctl,		/* extattrctl	*/
	(void *)eopnotsupp,		/* suspendctl	*/
	genfs_renamelock_enter,
	genfs_renamelock_exit,
	(void *)eopnotsupp,
	puffs_vnodeopv_descs,		/* vnodeops	*/
	0,				/* refcount	*/
	{ NULL, NULL }
};

static int
puffs_modcmd(modcmd_t cmd, void *arg)
{

	switch (cmd) {
	case MODULE_CMD_INIT:
		return vfs_attach(&puffs_vfsops);
	case MODULE_CMD_FINI:
		return vfs_detach(&puffs_vfsops);
	default:
		return ENOTTY;
	}
}
