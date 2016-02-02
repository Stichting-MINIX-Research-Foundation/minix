/* $NetBSD: ffs_quota2.c,v 1.5 2015/02/22 14:12:48 maxv Exp $ */
/*-
  * Copyright (c) 2010 Manuel Bouyer
  * All rights reserved.
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
__KERNEL_RCSID(0, "$NetBSD: ffs_quota2.c,v 1.5 2015/02/22 14:12:48 maxv Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/kauth.h>

#include <ufs/ufs/quota2.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ffs/ffs_extern.h>
#include <ufs/ffs/fs.h>

int
ffs_quota2_mount(struct mount *mp)
{
	struct ufsmount *ump = VFSTOUFS(mp);
	struct fs *fs = ump->um_fs;
	int error;
	struct vnode *vp;
	struct lwp *l = curlwp;

	if ((fs->fs_flags & FS_DOQUOTA2) == 0)
		return 0;

	ump->um_flags |= UFS_QUOTA2;
	ump->umq2_bsize = fs->fs_bsize;
	ump->umq2_bmask = fs->fs_qbmask;
	if (fs->fs_quota_magic != Q2_HEAD_MAGIC) {
		printf("%s: invalid quota magic number\n",
		    mp->mnt_stat.f_mntonname);
		return EINVAL;
	}
	if ((fs->fs_quota_flags & FS_Q2_DO_TYPE(USRQUOTA)) &&
	    fs->fs_quotafile[USRQUOTA] == 0) {
		printf("%s: no user quota inode\n",
		    mp->mnt_stat.f_mntonname); 
		return EINVAL;
	}
	if ((fs->fs_quota_flags & FS_Q2_DO_TYPE(GRPQUOTA)) &&
	    fs->fs_quotafile[GRPQUOTA] == 0) {
		printf("%s: no group quota inode\n",
		    mp->mnt_stat.f_mntonname);
		return EINVAL;
	}

	if (fs->fs_quota_flags & FS_Q2_DO_TYPE(USRQUOTA) &&
	    ump->um_quotas[USRQUOTA] == NULLVP) {
		error = VFS_VGET(mp, fs->fs_quotafile[USRQUOTA], &vp);
		if (error) {
			printf("%s: can't vget() user quota inode: %d\n",
			    mp->mnt_stat.f_mntonname, error);
			return error;
		}
		ump->um_quotas[USRQUOTA] = vp;
		ump->um_cred[USRQUOTA] = l->l_cred;
		mutex_enter(vp->v_interlock);
		vp->v_writecount++;
		mutex_exit(vp->v_interlock);
		VOP_UNLOCK(vp);
	}
	if (fs->fs_quota_flags & FS_Q2_DO_TYPE(GRPQUOTA) &&
	    ump->um_quotas[GRPQUOTA] == NULLVP) {
		error = VFS_VGET(mp, fs->fs_quotafile[GRPQUOTA], &vp);
		if (error) {
			vn_close(ump->um_quotas[USRQUOTA],
			    FREAD|FWRITE, l->l_cred);
			printf("%s: can't vget() group quota inode: %d\n",
			    mp->mnt_stat.f_mntonname, error);
			return error;
		}
		ump->um_quotas[GRPQUOTA] = vp;
		ump->um_cred[GRPQUOTA] = l->l_cred;
		mutex_enter(vp->v_interlock);
		vp->v_vflag |= VV_SYSTEM;
		vp->v_writecount++;
		mutex_exit(vp->v_interlock);
		VOP_UNLOCK(vp);
	}

	mp->mnt_flag |= MNT_QUOTA;
	return 0;
}
