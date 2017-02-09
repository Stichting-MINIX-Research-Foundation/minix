/*	$NetBSD: kernfs_vfsops.c,v 1.95 2014/07/20 13:58:04 hannken Exp $	*/

/*
 * Copyright (c) 1992, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
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
 *	@(#)kernfs_vfsops.c	8.10 (Berkeley) 5/14/95
 */

/*
 * Kernel params Filesystem
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kernfs_vfsops.c,v 1.95 2014/07/20 13:58:04 hannken Exp $");

#ifdef _KERNEL_OPT
#include "opt_compat_netbsd.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/dirent.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/kauth.h>
#include <sys/module.h>

#include <miscfs/genfs/genfs.h>
#include <miscfs/specfs/specdev.h>
#include <miscfs/kernfs/kernfs.h>

MODULE(MODULE_CLASS_VFS, kernfs, NULL);

MALLOC_JUSTDEFINE(M_KERNFSMNT, "kernfs mount", "kernfs mount structures");

dev_t rrootdev = NODEV;
kmutex_t kfs_lock;

VFS_PROTOS(kernfs);

void	kernfs_get_rrootdev(void);

static struct sysctllog *kernfs_sysctl_log;

void
kernfs_init(void)
{

	malloc_type_attach(M_KERNFSMNT);
	mutex_init(&kfs_lock, MUTEX_DEFAULT, IPL_NONE);
}

void
kernfs_reinit(void)
{

}

void
kernfs_done(void)
{

	mutex_destroy(&kfs_lock);
	malloc_type_detach(M_KERNFSMNT);
}

void
kernfs_get_rrootdev(void)
{
	static int tried = 0;

	if (tried) {
		/* Already did it once. */
		return;
	}
	tried = 1;

	if (rootdev == NODEV)
		return;
	rrootdev = devsw_blk2chr(rootdev);
	if (rrootdev != NODEV)
		return;
	rrootdev = NODEV;
	printf("kernfs_get_rrootdev: no raw root device\n");
}

/*
 * Mount the Kernel params filesystem
 */
int
kernfs_mount(struct mount *mp, const char *path, void *data, size_t *data_len)
{
	struct lwp *l = curlwp;
	int error = 0;
	struct kernfs_mount *fmp;

	if (UIO_MX & (UIO_MX - 1)) {
		log(LOG_ERR, "kernfs: invalid directory entry size");
		return (EINVAL);
	}

	if (mp->mnt_flag & MNT_GETARGS) {
		*data_len = 0;
		return 0;
	}
	/*
	 * Update is a no-op
	 */
	if (mp->mnt_flag & MNT_UPDATE)
		return (EOPNOTSUPP);

	fmp = malloc(sizeof(struct kernfs_mount), M_KERNFSMNT, M_WAITOK|M_ZERO);
	TAILQ_INIT(&fmp->nodelist);

	mp->mnt_stat.f_namemax = KERNFS_MAXNAMLEN;
	mp->mnt_flag |= MNT_LOCAL;
	mp->mnt_data = fmp;
	vfs_getnewfsid(mp);

	if ((error = set_statvfs_info(path, UIO_USERSPACE, "kernfs",
	    UIO_SYSSPACE, mp->mnt_op->vfs_name, mp, l)) != 0) {
		free(fmp, M_KERNFSMNT);
		return error;
	}

	kernfs_get_rrootdev();
	return 0;
}

int
kernfs_start(struct mount *mp, int flags)
{

	return (0);
}

int
kernfs_unmount(struct mount *mp, int mntflags)
{
	int error;
	int flags = 0;

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	if ((error = vflush(mp, 0, flags)) != 0)
		return (error);

	/*
	 * Finally, throw away the kernfs_mount structure
	 */
	free(mp->mnt_data, M_KERNFSMNT);
	mp->mnt_data = NULL;
	return (0);
}

int
kernfs_root(struct mount *mp, struct vnode **vpp)
{
	const struct kern_target *root_target = &kern_targets[0];
	int error;

	/* setup "." */
	error = vcache_get(mp, &root_target, sizeof(root_target), vpp);
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

/*ARGSUSED*/
int
kernfs_sync(struct mount *mp, int waitfor,
    kauth_cred_t uc)
{

	return (0);
}

/*
 * Kernfs flat namespace lookup.
 * Currently unsupported.
 */
int
kernfs_vget(struct mount *mp, ino_t ino,
    struct vnode **vpp)
{

	return (EOPNOTSUPP);
}

int
kernfs_loadvnode(struct mount *mp, struct vnode *vp,
    const void *key, size_t key_len, const void **new_key)
{
	const struct kern_target *kt;
	struct kernfs_node *kfs, *kfsp;
	long *cookie;

	KASSERT(key_len == sizeof(kt));
	memcpy(&kt, key, key_len);

	kfs = kmem_zalloc(sizeof(struct kernfs_node), KM_SLEEP);
	cookie = &(VFSTOKERNFS(mp)->fileno_cookie);
	mutex_enter(&kfs_lock);
again:
	TAILQ_FOREACH(kfsp, &VFSTOKERNFS(mp)->nodelist, kfs_list) {
		if (kfsp->kfs_cookie == *cookie) {
			(*cookie) ++;
			goto again;
		}
		if (TAILQ_NEXT(kfsp, kfs_list)) {
			if (kfsp->kfs_cookie < *cookie &&
			    *cookie < TAILQ_NEXT(kfsp, kfs_list)->kfs_cookie)
				break;
			if (kfsp->kfs_cookie + 1 <
			    TAILQ_NEXT(kfsp, kfs_list)->kfs_cookie) {
				*cookie = kfsp->kfs_cookie + 1;
				break;
			}
		}
	}

	kfs->kfs_cookie = *cookie;

	if (kfsp)
		TAILQ_INSERT_AFTER(&VFSTOKERNFS(mp)->nodelist, kfsp, kfs,
		    kfs_list);
	else
		TAILQ_INSERT_TAIL(&VFSTOKERNFS(mp)->nodelist, kfs, kfs_list);

	kfs->kfs_type = kt->kt_tag;
	kfs->kfs_vnode = vp;
	kfs->kfs_fileno = KERNFS_FILENO(kt, kt->kt_tag, kfs->kfs_cookie);
	kfs->kfs_kt = kt;
	kfs->kfs_mode = kt->kt_mode;
	vp->v_tag = VT_KERNFS;
	vp->v_op = kernfs_vnodeop_p;
	vp->v_data = kfs;
	vp->v_type = kt->kt_vtype;
	mutex_exit(&kfs_lock);

	if (kt->kt_tag == KFSkern)
		vp->v_vflag = VV_ROOT;

	if (kt->kt_tag == KFSdevice) {
		spec_node_init(vp, *(dev_t *)kt->kt_data);
	}

	uvm_vnp_setsize(vp, 0);

	*new_key = &kfs->kfs_kt;
	return 0;
}

extern const struct vnodeopv_desc kernfs_vnodeop_opv_desc;

const struct vnodeopv_desc * const kernfs_vnodeopv_descs[] = {
	&kernfs_vnodeop_opv_desc,
	NULL,
};

struct vfsops kernfs_vfsops = {
	.vfs_name = MOUNT_KERNFS,
	.vfs_min_mount_data = 0,
	.vfs_mount = kernfs_mount,
	.vfs_start = kernfs_start,
	.vfs_unmount = kernfs_unmount,
	.vfs_root = kernfs_root,
	.vfs_quotactl = (void *)eopnotsupp,
	.vfs_statvfs = genfs_statvfs,
	.vfs_sync = kernfs_sync,
	.vfs_vget = kernfs_vget,
	.vfs_loadvnode = kernfs_loadvnode,
	.vfs_fhtovp = (void *)eopnotsupp,
	.vfs_vptofh = (void *)eopnotsupp,
	.vfs_init = kernfs_init,
	.vfs_reinit = kernfs_reinit,
	.vfs_done = kernfs_done,
	.vfs_snapshot = (void *)eopnotsupp,
	.vfs_extattrctl = vfs_stdextattrctl,
	.vfs_suspendctl = (void *)eopnotsupp,
	.vfs_renamelock_enter = genfs_renamelock_enter,
	.vfs_renamelock_exit = genfs_renamelock_exit,
	.vfs_fsync = (void *)eopnotsupp,
	.vfs_opv_descs = kernfs_vnodeopv_descs
};

static int
kernfs_modcmd(modcmd_t cmd, void *arg)
{
	int error;

	switch (cmd) {
	case MODULE_CMD_INIT:
		error = vfs_attach(&kernfs_vfsops);
		if (error != 0)
			break;
		sysctl_createv(&kernfs_sysctl_log, 0, NULL, NULL,
			       CTLFLAG_PERMANENT,
			       CTLTYPE_NODE, "kernfs",
			       SYSCTL_DESCR("/kern file system"),
			       NULL, 0, NULL, 0,
			       CTL_VFS, 11, CTL_EOL);
		/*
		 * XXX the "11" above could be dynamic, thereby eliminating one
		 * more instance of the "number to vfs" mapping problem, but
		 * "11" is the order as taken from sys/mount.h
		 */
		break;
	case MODULE_CMD_FINI:
		error = vfs_detach(&kernfs_vfsops);
		if (error != 0)
			break;
		sysctl_teardown(&kernfs_sysctl_log);
		break;
	default:
		error = ENOTTY;
		break;
	}

	return (error);
}
