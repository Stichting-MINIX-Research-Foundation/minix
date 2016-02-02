/*	$NetBSD: ufs_vfsops.c,v 1.54 2015/03/17 09:39:29 hannken Exp $	*/

/*
 * Copyright (c) 1991, 1993, 1994
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
 *	@(#)ufs_vfsops.c	8.8 (Berkeley) 5/20/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ufs_vfsops.c,v 1.54 2015/03/17 09:39:29 hannken Exp $");

#if defined(_KERNEL_OPT)
#include "opt_ffs.h"
#include "opt_quota.h"
#endif

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/kmem.h>
#include <sys/kauth.h>

#include <miscfs/specfs/specdev.h>

#include <sys/quotactl.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>
#ifdef UFS_DIRHASH
#include <ufs/ufs/dirhash.h>
#endif

/* how many times ufs_init() was called */
static int ufs_initcount = 0;

pool_cache_t ufs_direct_cache;

/*
 * Make a filesystem operational.
 * Nothing to do at the moment.
 */
/* ARGSUSED */
int
ufs_start(struct mount *mp, int flags)
{

	return (0);
}

/*
 * Return the root of a filesystem.
 */
int
ufs_root(struct mount *mp, struct vnode **vpp)
{
	struct vnode *nvp;
	int error;

	if ((error = VFS_VGET(mp, (ino_t)UFS_ROOTINO, &nvp)) != 0)
		return (error);
	*vpp = nvp;
	return (0);
}

/*
 * Look up and return a vnode/inode pair by inode number.
 */
int
ufs_vget(struct mount *mp, ino_t ino, struct vnode **vpp)
{
	int error;

	error = vcache_get(mp, &ino, sizeof(ino), vpp);
	if (error)
		return error;
	error = vn_lock(*vpp, LK_EXCLUSIVE);
	if (error) {
		vrele(*vpp);
		*vpp = NULL;
		return error;
	}
	return 0;
}

/*
 * Do operations associated with quotas
 */
int
ufs_quotactl(struct mount *mp, struct quotactl_args *args)
{

#if !defined(QUOTA) && !defined(QUOTA2)
	(void) mp;
	(void) args;
	return (EOPNOTSUPP);
#else
	struct lwp *l = curlwp;
	int error;

	/* Mark the mount busy, as we're passing it to kauth(9). */
	error = vfs_busy(mp, NULL);
	if (error) {
		return (error);
	}
	mutex_enter(&mp->mnt_updating);

	error = quota_handle_cmd(mp, l, args);

	mutex_exit(&mp->mnt_updating);
	vfs_unbusy(mp, false, NULL);
	return (error);
#endif
}
	
#if 0
	switch (cmd) {
	case Q_SYNC:
		break;

	case Q_GETQUOTA:
		/* The user can always query about his own quota. */
		if (uid == kauth_cred_getuid(l->l_cred))
			break;

		error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_FS_QUOTA,
		    KAUTH_REQ_SYSTEM_FS_QUOTA_GET, mp, KAUTH_ARG(uid), NULL);

		break;

	case Q_QUOTAON:
	case Q_QUOTAOFF:
		error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_FS_QUOTA,
		    KAUTH_REQ_SYSTEM_FS_QUOTA_ONOFF, mp, NULL, NULL);

		break;

	case Q_SETQUOTA:
	case Q_SETUSE:
		error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_FS_QUOTA,
		    KAUTH_REQ_SYSTEM_FS_QUOTA_MANAGE, mp, KAUTH_ARG(uid), NULL);

		break;

	default:
		error = EINVAL;
		break;
	}

	type = cmds & SUBCMDMASK;
	if (!error) {
		/* Only check if there was no error above. */
		if ((u_int)type >= MAXQUOTAS)
			error = EINVAL;
	}

	if (error) {
		vfs_unbusy(mp, false, NULL);
		return (error);
	}

	mutex_enter(&mp->mnt_updating);
	switch (cmd) {

	case Q_QUOTAON:
		error = quotaon(l, mp, type, arg);
		break;

	case Q_QUOTAOFF:
		error = quotaoff(l, mp, type);
		break;

	case Q_SETQUOTA:
		error = setquota(mp, uid, type, arg);
		break;

	case Q_SETUSE:
		error = setuse(mp, uid, type, arg);
		break;

	case Q_GETQUOTA:
		error = getquota(mp, uid, type, arg);
		break;

	case Q_SYNC:
		error = qsync(mp);
		break;

	default:
		error = EINVAL;
	}
	mutex_exit(&mp->mnt_updating);
	vfs_unbusy(mp, false, NULL);
	return (error);
#endif

/*
 * This is the generic part of fhtovp called after the underlying
 * filesystem has validated the file handle.
 */
int
ufs_fhtovp(struct mount *mp, struct ufid *ufhp, struct vnode **vpp)
{
	struct vnode *nvp;
	struct inode *ip;
	int error;

	if ((error = VFS_VGET(mp, ufhp->ufid_ino, &nvp)) != 0) {
		if (error == ENOENT)
			error = ESTALE;
		*vpp = NULLVP;
		return (error);
	}
	ip = VTOI(nvp);
	KASSERT(ip != NULL);
	if (ip->i_mode == 0 || ip->i_gen != ufhp->ufid_gen) {
		vput(nvp);
		*vpp = NULLVP;
		return (ESTALE);
	}
	*vpp = nvp;
	return (0);
}

/*
 * Initialize UFS filesystems, done only once.
 */
void
ufs_init(void)
{
	if (ufs_initcount++ > 0)
		return;

	ufs_direct_cache = pool_cache_init(sizeof(struct direct), 0, 0, 0,
	    "ufsdir", NULL, IPL_NONE, NULL, NULL, NULL);

#if defined(QUOTA) || defined(QUOTA2)
	dqinit();
#endif
#ifdef UFS_DIRHASH
	ufsdirhash_init();
#endif
#ifdef UFS_EXTATTR
	ufs_extattr_init();
#endif
}

void
ufs_reinit(void)
{
#if defined(QUOTA) || defined(QUOTA2)
	dqreinit();
#endif
}

/*
 * Free UFS filesystem resources, done only once.
 */
void
ufs_done(void)
{
	if (--ufs_initcount > 0)
		return;

#if defined(QUOTA) || defined(QUOTA2)
	dqdone();
#endif
	pool_cache_destroy(ufs_direct_cache);
#ifdef UFS_DIRHASH
	ufsdirhash_done();
#endif
#ifdef UFS_EXTATTR
	ufs_extattr_done();
#endif
}
