/*	$NetBSD: vfs_syscalls_20.c,v 1.39 2015/07/24 13:02:52 maxv Exp $	*/

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
 *	@(#)vfs_syscalls.c	8.42 (Berkeley) 7/31/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: vfs_syscalls_20.c,v 1.39 2015/07/24 13:02:52 maxv Exp $");

#ifdef _KERNEL_OPT
#include "opt_compat_netbsd.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/dirent.h>
#include <sys/sysctl.h>
#include <sys/syscallargs.h>
#include <sys/kauth.h>

#include <compat/sys/mount.h>

#define MOUNTNO_NONE	0
#define MOUNTNO_UFS	1		/* UNIX "Fast" Filesystem */
#define MOUNTNO_NFS	2		/* Network Filesystem */
#define MOUNTNO_MFS	3		/* Memory Filesystem */
#define MOUNTNO_MSDOS	4		/* MSDOS Filesystem */
#define MOUNTNO_CD9660	5		/* iso9660 cdrom */
#define MOUNTNO_FDESC	6		/* /dev/fd filesystem */
#define MOUNTNO_KERNFS	7		/* kernel variable filesystem */
#define MOUNTNO_DEVFS	8		/* device node filesystem */
#define MOUNTNO_AFS	9		/* AFS 3.x */
static const struct {
	const char *name;
	const int value;
} nv[] = {
	{ MOUNT_UFS, MOUNTNO_UFS },
	{ MOUNT_NFS, MOUNTNO_NFS },
	{ MOUNT_MFS, MOUNTNO_MFS },
	{ MOUNT_MSDOS, MOUNTNO_MSDOS },
	{ MOUNT_CD9660, MOUNTNO_CD9660 },
	{ MOUNT_FDESC, MOUNTNO_FDESC },
	{ MOUNT_KERNFS, MOUNTNO_KERNFS },
	{ MOUNT_AFS, MOUNTNO_AFS },
};

static int
vfs2fs(struct statfs12 *bfs, const struct statvfs *fs)
{
	struct statfs12 ofs;
	int i;
	ofs.f_type = 0;
	ofs.f_oflags = (short)fs->f_flag;

	for (i = 0; i < sizeof(nv) / sizeof(nv[0]); i++) {
		if (strcmp(nv[i].name, fs->f_fstypename) == 0) {
			ofs.f_type = nv[i].value;
			break;
		}
	}
#define CLAMP(a)	(long)(((a) & ~LONG_MAX) ? LONG_MAX : (a))
	ofs.f_bsize = CLAMP(fs->f_frsize);
	ofs.f_iosize = CLAMP(fs->f_iosize);
	ofs.f_blocks = CLAMP(fs->f_blocks);
	ofs.f_bfree = CLAMP(fs->f_bfree);
	if (fs->f_bfree > fs->f_bresvd)
		ofs.f_bavail = CLAMP(fs->f_bfree - fs->f_bresvd);
	else
		ofs.f_bavail = -CLAMP(fs->f_bresvd - fs->f_bfree);
	ofs.f_files = CLAMP(fs->f_files);
	ofs.f_ffree = CLAMP(fs->f_ffree);
	ofs.f_fsid = fs->f_fsidx;
	ofs.f_owner = fs->f_owner;
	ofs.f_flags = (long)fs->f_flag;
	ofs.f_syncwrites = CLAMP(fs->f_syncwrites);
	ofs.f_asyncwrites = CLAMP(fs->f_asyncwrites);
	(void)strncpy(ofs.f_fstypename, fs->f_fstypename,
	    sizeof(ofs.f_fstypename));
	(void)strncpy(ofs.f_mntonname, fs->f_mntonname,
	    sizeof(ofs.f_mntonname));
	(void)strncpy(ofs.f_mntfromname, fs->f_mntfromname,
	    sizeof(ofs.f_mntfromname));

	return copyout(&ofs, bfs, sizeof(ofs));
}

/*
 * Get filesystem statistics.
 */
/* ARGSUSED */
int
compat_20_sys_statfs(struct lwp *l, const struct compat_20_sys_statfs_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(struct statfs12 *) buf;
	} */
	struct mount *mp;
	struct statvfs *sbuf;
	int error;
	struct vnode *vp;

	error = namei_simple_user(SCARG(uap, path),
			NSM_FOLLOW_TRYEMULROOT, &vp);
	if (error != 0)
		return error;

	mp = vp->v_mount;

	sbuf = malloc(sizeof(*sbuf), M_TEMP, M_WAITOK);
	if ((error = dostatvfs(mp, sbuf, l, 0, 1)) != 0)
		goto done;

	error = vfs2fs(SCARG(uap, buf), sbuf);
done:
	vrele(vp);
	free(sbuf, M_TEMP);
	return error;
}

/*
 * Get filesystem statistics.
 */
/* ARGSUSED */
int
compat_20_sys_fstatfs(struct lwp *l, const struct compat_20_sys_fstatfs_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(struct statfs12 *) buf;
	} */
	struct file *fp;
	struct mount *mp;
	struct statvfs *sbuf;
	int error;

	/* fd_getvnode() will use the descriptor for us */
	if ((error = fd_getvnode(SCARG(uap, fd), &fp)) != 0)
		return (error);
	mp = fp->f_vnode->v_mount;
	sbuf = malloc(sizeof(*sbuf), M_TEMP, M_WAITOK);
	if ((error = dostatvfs(mp, sbuf, l, 0, 1)) != 0)
		goto out;
	error = vfs2fs(SCARG(uap, buf), sbuf);
 out:
	fd_putfile(SCARG(uap, fd));
	free(sbuf, M_TEMP);
	return error;
}


/*
 * Get statistics on all filesystems.
 */
int
compat_20_sys_getfsstat(struct lwp *l, const struct compat_20_sys_getfsstat_args *uap, register_t *retval)
{
	/* {
		syscallarg(struct statfs12 *) buf;
		syscallarg(long) bufsize;
		syscallarg(int) flags;
	} */
	int root = 0;
	struct mount *mp, *nmp;
	struct statvfs *sbuf;
	struct statfs12 *sfsp;
	size_t count, maxcount;
	int error = 0;

	sbuf = malloc(sizeof(*sbuf), M_TEMP, M_WAITOK);
	maxcount = (size_t)SCARG(uap, bufsize) / sizeof(struct statfs12);
	sfsp = SCARG(uap, buf);
	mutex_enter(&mountlist_lock);
	count = 0;
	for (mp = TAILQ_FIRST(&mountlist); mp != NULL; mp = nmp) {
		if (vfs_busy(mp, &nmp)) {
			continue;
		}
		if (sfsp && count < maxcount) {
			error = dostatvfs(mp, sbuf, l, SCARG(uap, flags), 0);
			if (error) {
				vfs_unbusy(mp, false, &nmp);
				continue;
			}
			error = vfs2fs(sfsp, sbuf);
			if (error) {
				vfs_unbusy(mp, false, NULL);
				goto out;
			}
			sfsp++;
			root |= strcmp(sbuf->f_mntonname, "/") == 0;
		}
		count++;
		vfs_unbusy(mp, false, &nmp);
	}
	mutex_exit(&mountlist_lock);
	if (root == 0 && l->l_proc->p_cwdi->cwdi_rdir) {
		/*
		 * fake a root entry
		 */
		if ((error = dostatvfs(l->l_proc->p_cwdi->cwdi_rdir->v_mount,
				       sbuf, l, SCARG(uap, flags), 1)) != 0)
			goto out;
		if (sfsp)
			error = vfs2fs(sfsp, sbuf);
		count++;
	}
	if (sfsp && count > maxcount)
		*retval = maxcount;
	else
		*retval = count;
out:
	free(sbuf, M_TEMP);
	return error;
}

int
compat_20_sys_fhstatfs(struct lwp *l, const struct compat_20_sys_fhstatfs_args *uap, register_t *retval)
{
	/* {
		syscallarg(const struct compat_30_fhandle *) fhp;
		syscallarg(struct statfs12 *) buf;
	} */
	struct statvfs *sbuf;
	struct compat_30_fhandle fh;
	struct mount *mp;
	struct vnode *vp;
	int error;

	/*
	 * Must be super user
	 */
	if ((error = kauth_authorize_system(l->l_cred,
	    KAUTH_SYSTEM_FILEHANDLE, 0, NULL, NULL, NULL)))
		return (error);

	if ((error = copyin(SCARG(uap, fhp), &fh, sizeof(fh))) != 0)
		return (error);

	if ((mp = vfs_getvfs(&fh.fh_fsid)) == NULL)
		return (ESTALE);
	if ((error = VFS_FHTOVP(mp, (struct fid*)&fh.fh_fid, &vp)))
		return (error);
	mp = vp->v_mount;
	VOP_UNLOCK(vp);
	sbuf = malloc(sizeof(*sbuf), M_TEMP, M_WAITOK);
	if ((error = VFS_STATVFS(mp, sbuf)) != 0)
		goto out;
	error = vfs2fs(SCARG(uap, buf), sbuf);
out:
	vrele(vp);
	free(sbuf, M_TEMP);
	return error;
}
