/*	$NetBSD: fdesc_vfsops.c,v 1.91 2014/11/09 18:08:06 maxv Exp $	*/

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
 *	@(#)fdesc_vfsops.c	8.10 (Berkeley) 5/14/95
 *
 * #Id: fdesc_vfsops.c,v 1.9 1993/04/06 15:28:33 jsp Exp #
 */

/*
 * /dev/fd Filesystem
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: fdesc_vfsops.c,v 1.91 2014/11/09 18:08:06 maxv Exp $");

#if defined(_KERNEL_OPT)
#include "opt_compat_netbsd.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/filedesc.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/dirent.h>
#include <sys/namei.h>
#include <sys/kauth.h>
#include <sys/module.h>

#include <miscfs/genfs/genfs.h>
#include <miscfs/fdesc/fdesc.h>

MODULE(MODULE_CLASS_VFS, fdesc, NULL);

VFS_PROTOS(fdesc);

static struct sysctllog *fdesc_sysctl_log;

/*
 * Mount the per-process file descriptors (/dev/fd)
 */
int
fdesc_mount(struct mount *mp, const char *path, void *data, size_t *data_len)
{
	struct lwp *l = curlwp;
	int error = 0, ix;
	struct vnode *rvp;

	if (mp->mnt_flag & MNT_GETARGS) {
		*data_len = 0;
		return 0;
	}
	/*
	 * Update is a no-op
	 */
	if (mp->mnt_flag & MNT_UPDATE)
		return (EOPNOTSUPP);

	ix = FD_ROOT;
	error = vcache_get(mp, &ix, sizeof(ix), &rvp);
	if (error)
		return error;

	mp->mnt_stat.f_namemax = FDESC_MAXNAMLEN;
	mp->mnt_flag |= MNT_LOCAL;
	mp->mnt_data = rvp;
	vfs_getnewfsid(mp);

	error = set_statvfs_info(path, UIO_USERSPACE, "fdesc", UIO_SYSSPACE,
	    mp->mnt_op->vfs_name, mp, l);
	return error;
}

int
fdesc_start(struct mount *mp, int flags)
{
	return (0);
}

int
fdesc_unmount(struct mount *mp, int mntflags)
{
	int error;
	int flags = 0;
	struct vnode *rtvp = mp->mnt_data;

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	if (rtvp->v_usecount > 1 && (mntflags & MNT_FORCE) == 0)
		return (EBUSY);
	if ((error = vflush(mp, rtvp, flags)) != 0)
		return (error);

	/*
	 * Blow it away for future re-use
	 */
	vgone(rtvp);
	mp->mnt_data = NULL;

	return (0);
}

int
fdesc_root(struct mount *mp, struct vnode **vpp)
{
	struct vnode *vp;

	/*
	 * Return locked reference to root.
	 */
	vp = mp->mnt_data;
	vref(vp);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	*vpp = vp;
	return (0);
}

/*ARGSUSED*/
int
fdesc_sync(struct mount *mp, int waitfor,
    kauth_cred_t uc)
{

	return (0);
}

/*
 * Fdesc flat namespace lookup.
 * Currently unsupported.
 */
int
fdesc_vget(struct mount *mp, ino_t ino,
    struct vnode **vpp)
{

	return (EOPNOTSUPP);
}

int
fdesc_loadvnode(struct mount *mp, struct vnode *vp,
    const void *key, size_t key_len, const void **new_key)
{
	int ix;
	struct fdescnode *fd;

	KASSERT(key_len == sizeof(ix));
	memcpy(&ix, key, key_len);

	fd = kmem_alloc(sizeof(struct fdescnode), KM_SLEEP);
	fd->fd_fd = -1;
	fd->fd_link = NULL;
	fd->fd_ix = ix;
	fd->fd_vnode = vp;
	vp->v_tag = VT_FDESC;
	vp->v_op = fdesc_vnodeop_p;
	vp->v_data = fd;
	switch (ix) {
	case FD_ROOT:
		fd->fd_type = Froot;
		vp->v_type = VDIR;
		vp->v_vflag |= VV_ROOT;
		break;
	case FD_DEVFD:
		fd->fd_type = Fdevfd;
		vp->v_type = VDIR;
		break;
	case FD_CTTY:
		fd->fd_type = Fctty;
		vp->v_type = VCHR;
		break;
	case FD_STDIN:
		fd->fd_type = Flink;
		fd->fd_link = "fd/0";
		vp->v_type = VLNK;
		break;
	case FD_STDOUT:
		fd->fd_type = Flink;
		fd->fd_link = "fd/1";
		vp->v_type = VLNK;
		break;
	case FD_STDERR:
		fd->fd_type = Flink;
		fd->fd_link = "fd/2";
		vp->v_type = VLNK;
		break;
	default:
		KASSERT(ix >= FD_DESC);
		fd->fd_type = Fdesc;
		fd->fd_fd = ix - FD_DESC;
		vp->v_type = VNON;
		break;
	}
	uvm_vnp_setsize(vp, 0);
	*new_key = &fd->fd_ix;

	return 0;
}

extern const struct vnodeopv_desc fdesc_vnodeop_opv_desc;

const struct vnodeopv_desc * const fdesc_vnodeopv_descs[] = {
	&fdesc_vnodeop_opv_desc,
	NULL,
};

struct vfsops fdesc_vfsops = {
	.vfs_name = MOUNT_FDESC,
	.vfs_min_mount_data = 0,
	.vfs_mount = fdesc_mount,
	.vfs_start = fdesc_start,
	.vfs_unmount = fdesc_unmount,
	.vfs_root = fdesc_root,
	.vfs_quotactl = (void *)eopnotsupp,
	.vfs_statvfs = genfs_statvfs,
	.vfs_sync = fdesc_sync,
	.vfs_vget = fdesc_vget,
	.vfs_loadvnode = fdesc_loadvnode,
	.vfs_fhtovp = (void *)eopnotsupp,
	.vfs_vptofh = (void *)eopnotsupp,
	.vfs_init = fdesc_init,
	.vfs_done = fdesc_done,
	.vfs_snapshot = (void *)eopnotsupp,
	.vfs_extattrctl = vfs_stdextattrctl,
	.vfs_suspendctl = (void *)eopnotsupp,
	.vfs_renamelock_enter = genfs_renamelock_enter,
	.vfs_renamelock_exit = genfs_renamelock_exit,
	.vfs_fsync = (void *)eopnotsupp,
	.vfs_opv_descs = fdesc_vnodeopv_descs
};

static int
fdesc_modcmd(modcmd_t cmd, void *arg)
{
	int error;

	switch (cmd) {
	case MODULE_CMD_INIT:
		error = vfs_attach(&fdesc_vfsops);
		if (error != 0)
			break;
		sysctl_createv(&fdesc_sysctl_log, 0, NULL, NULL,
			       CTLFLAG_PERMANENT,
			       CTLTYPE_NODE, "fdesc",
			       SYSCTL_DESCR("File-descriptor file system"),
			       NULL, 0, NULL, 0,
			       CTL_VFS, 7, CTL_EOL);
		/*
		 * XXX the "7" above could be dynamic, thereby eliminating one
		 * more instance of the "number to vfs" mapping problem, but
		 * "7" is the order as taken from sys/mount.h
		 */
		break;
	case MODULE_CMD_FINI:
		error = vfs_detach(&fdesc_vfsops);
		if (error != 0)
			break;
		sysctl_teardown(&fdesc_sysctl_log);
		break;
	default:
		error = ENOTTY;
		break;
	}

	return (error);
}
