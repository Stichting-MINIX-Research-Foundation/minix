/*	$NetBSD: ulfs_vfsops.c,v 1.11 2015/09/15 15:01:03 dholland Exp $	*/
/*  from NetBSD: ufs_vfsops.c,v 1.52 2013/01/22 09:39:18 dholland Exp  */

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
__KERNEL_RCSID(0, "$NetBSD: ulfs_vfsops.c,v 1.11 2015/09/15 15:01:03 dholland Exp $");

#if defined(_KERNEL_OPT)
#include "opt_lfs.h"
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
#include <sys/quotactl.h>

#include <miscfs/specfs/specdev.h>

#include <ufs/lfs/lfs.h>
#include <ufs/lfs/lfs_accessors.h>
#include <ufs/lfs/ulfs_quotacommon.h>
#include <ufs/lfs/ulfs_inode.h>
#include <ufs/lfs/ulfsmount.h>
#include <ufs/lfs/ulfs_extern.h>
#ifdef LFS_DIRHASH
#include <ufs/lfs/ulfs_dirhash.h>
#endif

/* how many times ulfs_init() was called */
static int ulfs_initcount = 0;

/*
 * Make a filesystem operational.
 * Nothing to do at the moment.
 */
/* ARGSUSED */
int
ulfs_start(struct mount *mp, int flags)
{

	return (0);
}

/*
 * Return the root of a filesystem.
 */
int
ulfs_root(struct mount *mp, struct vnode **vpp)
{
	struct vnode *nvp;
	int error;

	if ((error = VFS_VGET(mp, (ino_t)ULFS_ROOTINO, &nvp)) != 0)
		return (error);
	*vpp = nvp;
	return (0);
}

/*
 * Do operations associated with quotas
 */
int
ulfs_quotactl(struct mount *mp, struct quotactl_args *args)
{

#if !defined(LFS_QUOTA) && !defined(LFS_QUOTA2)
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

	error = lfsquota_handle_cmd(mp, l, args);

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
		error = lfs_qsync(mp);
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
ulfs_fhtovp(struct mount *mp, struct ulfs_ufid *ufhp, struct vnode **vpp)
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
 * Initialize ULFS filesystems, done only once.
 */
void
ulfs_init(void)
{
	if (ulfs_initcount++ > 0)
		return;

#if defined(LFS_QUOTA) || defined(LFS_QUOTA2)
	lfs_dqinit();
#endif
#ifdef LFS_DIRHASH
	ulfsdirhash_init();
#endif
#ifdef LFS_EXTATTR
	ulfs_extattr_init();
#endif
}

void
ulfs_reinit(void)
{

#if defined(LFS_QUOTA) || defined(LFS_QUOTA2)
	lfs_dqreinit();
#endif
}

/*
 * Free ULFS filesystem resources, done only once.
 */
void
ulfs_done(void)
{
	if (--ulfs_initcount > 0)
		return;

#if defined(LFS_QUOTA) || defined(LFS_QUOTA2)
	lfs_dqdone();
#endif
#ifdef LFS_DIRHASH
	ulfsdirhash_done();
#endif
#ifdef LFS_EXTATTR
	ulfs_extattr_done();
#endif
}
