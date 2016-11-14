/*	$NetBSD: vfs_syscalls.c,v 1.500 2015/07/24 13:02:52 maxv Exp $	*/

/*-
 * Copyright (c) 2008, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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
 *	@(#)vfs_syscalls.c	8.42 (Berkeley) 7/31/95
 */

/*
 * Virtual File System System Calls
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: vfs_syscalls.c,v 1.500 2015/07/24 13:02:52 maxv Exp $");

#ifdef _KERNEL_OPT
#include "opt_fileassoc.h"
#include "veriexec.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/kmem.h>
#include <sys/dirent.h>
#include <sys/sysctl.h>
#include <sys/syscallargs.h>
#include <sys/vfs_syscalls.h>
#include <sys/quota.h>
#include <sys/quotactl.h>
#include <sys/ktrace.h>
#ifdef FILEASSOC
#include <sys/fileassoc.h>
#endif /* FILEASSOC */
#include <sys/extattr.h>
#include <sys/verified_exec.h>
#include <sys/kauth.h>
#include <sys/atomic.h>
#include <sys/module.h>
#include <sys/buf.h>

#include <miscfs/genfs/genfs.h>
#include <miscfs/specfs/specdev.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#include <nfs/nfs_var.h>

/* XXX this shouldn't be here */
#ifndef OFF_T_MAX
#define OFF_T_MAX __type_max(off_t)
#endif

static int change_flags(struct vnode *, u_long, struct lwp *);
static int change_mode(struct vnode *, int, struct lwp *);
static int change_owner(struct vnode *, uid_t, gid_t, struct lwp *, int);
static int do_sys_openat(lwp_t *, int, const char *, int, int, int *);
static int do_sys_mkdirat(struct lwp *l, int, const char *, mode_t,
    enum uio_seg);
static int do_sys_mkfifoat(struct lwp *, int, const char *, mode_t);
static int do_sys_symlinkat(struct lwp *, const char *, int, const char *,
    enum uio_seg);
static int do_sys_renameat(struct lwp *l, int, const char *, int, const char *,
    enum uio_seg, int);
static int do_sys_readlinkat(struct lwp *, int, const char *, char *,
    size_t, register_t *);
static int do_sys_unlinkat(struct lwp *, int, const char *, int, enum uio_seg);

static int fd_nameiat(struct lwp *, int, struct nameidata *);
static int fd_nameiat_simple_user(struct lwp *, int, const char *,
    namei_simple_flags_t, struct vnode **);


/*
 * This table is used to maintain compatibility with 4.3BSD
 * and NetBSD 0.9 mount syscalls - and possibly other systems.
 * Note, the order is important!
 *
 * Do not modify this table. It should only contain filesystems
 * supported by NetBSD 0.9 and 4.3BSD.
 */
const char * const mountcompatnames[] = {
	NULL,		/* 0 = MOUNT_NONE */
	MOUNT_FFS,	/* 1 = MOUNT_UFS */
	MOUNT_NFS,	/* 2 */
	MOUNT_MFS,	/* 3 */
	MOUNT_MSDOS,	/* 4 */
	MOUNT_CD9660,	/* 5 = MOUNT_ISOFS */
	MOUNT_FDESC,	/* 6 */
	MOUNT_KERNFS,	/* 7 */
	NULL,		/* 8 = MOUNT_DEVFS */
	MOUNT_AFS,	/* 9 */
};

const int nmountcompatnames = __arraycount(mountcompatnames);

static int 
fd_nameiat(struct lwp *l, int fdat, struct nameidata *ndp)
{
	file_t *dfp;
	int error;

	if (fdat != AT_FDCWD) {
		if ((error = fd_getvnode(fdat, &dfp)) != 0)
			goto out;

		NDAT(ndp, dfp->f_vnode);
	}

	error = namei(ndp);

	if (fdat != AT_FDCWD)
		fd_putfile(fdat);
out:
	return error;	
}

static int
fd_nameiat_simple_user(struct lwp *l, int fdat, const char *path,
    namei_simple_flags_t sflags, struct vnode **vp_ret)
{
	file_t *dfp;
	struct vnode *dvp;
	int error;

	if (fdat != AT_FDCWD) {
		if ((error = fd_getvnode(fdat, &dfp)) != 0)
			goto out;

		dvp = dfp->f_vnode;
	} else {
		dvp = NULL;
	}

	error = nameiat_simple_user(dvp, path, sflags, vp_ret);

	if (fdat != AT_FDCWD)
		fd_putfile(fdat);
out:
	return error;	
}

static int
open_setfp(struct lwp *l, file_t *fp, struct vnode *vp, int indx, int flags)
{
	int error;

	fp->f_flag = flags & FMASK;
	fp->f_type = DTYPE_VNODE;
	fp->f_ops = &vnops;
	fp->f_vnode = vp;

	if (flags & (O_EXLOCK | O_SHLOCK)) {
		struct flock lf;
		int type;

		lf.l_whence = SEEK_SET;
		lf.l_start = 0;
		lf.l_len = 0;
		if (flags & O_EXLOCK)
			lf.l_type = F_WRLCK;
		else
			lf.l_type = F_RDLCK;
		type = F_FLOCK;
		if ((flags & FNONBLOCK) == 0)
			type |= F_WAIT;
		VOP_UNLOCK(vp);
		error = VOP_ADVLOCK(vp, fp, F_SETLK, &lf, type);
		if (error) {
			(void) vn_close(vp, fp->f_flag, fp->f_cred);
			fd_abort(l->l_proc, fp, indx);
			return error;
		}
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
		atomic_or_uint(&fp->f_flag, FHASLOCK);
	}
	if (flags & O_CLOEXEC)
		fd_set_exclose(l, indx, true);
	return 0;
}

static int
mount_update(struct lwp *l, struct vnode *vp, const char *path, int flags,
    void *data, size_t *data_len)
{
	struct mount *mp;
	int error = 0, saved_flags;

	mp = vp->v_mount;
	saved_flags = mp->mnt_flag;

	/* We can operate only on VV_ROOT nodes. */
	if ((vp->v_vflag & VV_ROOT) == 0) {
		error = EINVAL;
		goto out;
	}

	/*
	 * We only allow the filesystem to be reloaded if it
	 * is currently mounted read-only.  Additionally, we
	 * prevent read-write to read-only downgrades.
	 */
	if ((flags & (MNT_RELOAD | MNT_RDONLY)) != 0 &&
	    (mp->mnt_flag & MNT_RDONLY) == 0 &&
	    (mp->mnt_iflag & IMNT_CAN_RWTORO) == 0) {
		error = EOPNOTSUPP;	/* Needs translation */
		goto out;
	}

	error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_MOUNT,
	    KAUTH_REQ_SYSTEM_MOUNT_UPDATE, mp, KAUTH_ARG(flags), data);
	if (error)
		goto out;

	if (vfs_busy(mp, NULL)) {
		error = EPERM;
		goto out;
	}

	mutex_enter(&mp->mnt_updating);

	mp->mnt_flag &= ~MNT_OP_FLAGS;
	mp->mnt_flag |= flags & MNT_OP_FLAGS;

	/*
	 * Set the mount level flags.
	 */
	if (flags & MNT_RDONLY)
		mp->mnt_flag |= MNT_RDONLY;
	else if (mp->mnt_flag & MNT_RDONLY)
		mp->mnt_iflag |= IMNT_WANTRDWR;
	mp->mnt_flag &= ~MNT_BASIC_FLAGS;
	mp->mnt_flag |= flags & MNT_BASIC_FLAGS;
	error = VFS_MOUNT(mp, path, data, data_len);

	if (error && data != NULL) {
		int error2;

		/*
		 * Update failed; let's try and see if it was an
		 * export request.  For compat with 3.0 and earlier.
		 */
		error2 = vfs_hooks_reexport(mp, path, data);

		/*
		 * Only update error code if the export request was
		 * understood but some problem occurred while
		 * processing it.
		 */
		if (error2 != EJUSTRETURN)
			error = error2;
	}

	if (mp->mnt_iflag & IMNT_WANTRDWR)
		mp->mnt_flag &= ~MNT_RDONLY;
	if (error)
		mp->mnt_flag = saved_flags;
	mp->mnt_flag &= ~MNT_OP_FLAGS;
	mp->mnt_iflag &= ~IMNT_WANTRDWR;
	if ((mp->mnt_flag & (MNT_RDONLY | MNT_ASYNC)) == 0) {
		if ((mp->mnt_iflag & IMNT_ONWORKLIST) == 0)
			vfs_syncer_add_to_worklist(mp);
	} else {
		if ((mp->mnt_iflag & IMNT_ONWORKLIST) != 0)
			vfs_syncer_remove_from_worklist(mp);
	}
	mutex_exit(&mp->mnt_updating);
	vfs_unbusy(mp, false, NULL);

	if ((error == 0) && !(saved_flags & MNT_EXTATTR) && 
	    (flags & MNT_EXTATTR)) {
		if (VFS_EXTATTRCTL(mp, EXTATTR_CMD_START, 
				   NULL, 0, NULL) != 0) {
			printf("%s: failed to start extattr, error = %d",
			       mp->mnt_stat.f_mntonname, error);
			mp->mnt_flag &= ~MNT_EXTATTR;
		}
	}

	if ((error == 0) && (saved_flags & MNT_EXTATTR) && 
	    !(flags & MNT_EXTATTR)) {
		if (VFS_EXTATTRCTL(mp, EXTATTR_CMD_STOP, 
				   NULL, 0, NULL) != 0) {
			printf("%s: failed to stop extattr, error = %d",
			       mp->mnt_stat.f_mntonname, error);
			mp->mnt_flag |= MNT_RDONLY;
		}
	}
 out:
	return (error);
}

static int
mount_get_vfsops(const char *fstype, struct vfsops **vfsops)
{
	char fstypename[sizeof(((struct statvfs *)NULL)->f_fstypename)];
	int error;

	/* Copy file-system type from userspace.  */
	error = copyinstr(fstype, fstypename, sizeof(fstypename), NULL);
	if (error) {
		/*
		 * Historically, filesystem types were identified by numbers.
		 * If we get an integer for the filesystem type instead of a
		 * string, we check to see if it matches one of the historic
		 * filesystem types.
		 */
		u_long fsindex = (u_long)fstype;
		if (fsindex >= nmountcompatnames ||
		    mountcompatnames[fsindex] == NULL)
			return ENODEV;
		strlcpy(fstypename, mountcompatnames[fsindex],
		    sizeof(fstypename));
	}

	/* Accept `ufs' as an alias for `ffs', for compatibility. */
	if (strcmp(fstypename, "ufs") == 0)
		fstypename[0] = 'f';

	if ((*vfsops = vfs_getopsbyname(fstypename)) != NULL)
		return 0;

	/* If we can autoload a vfs module, try again */
	(void)module_autoload(fstypename, MODULE_CLASS_VFS);

	if ((*vfsops = vfs_getopsbyname(fstypename)) != NULL)
		return 0;

	return ENODEV;
}

static int
mount_getargs(struct lwp *l, struct vnode *vp, const char *path, int flags,
    void *data, size_t *data_len)
{
	struct mount *mp;
	int error;

	/* If MNT_GETARGS is specified, it should be the only flag. */
	if (flags & ~MNT_GETARGS)
		return EINVAL;

	mp = vp->v_mount;

	/* XXX: probably some notion of "can see" here if we want isolation. */ 
	error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_MOUNT,
	    KAUTH_REQ_SYSTEM_MOUNT_GET, mp, data, NULL);
	if (error)
		return error;

	if ((vp->v_vflag & VV_ROOT) == 0)
		return EINVAL;

	if (vfs_busy(mp, NULL))
		return EPERM;

	mutex_enter(&mp->mnt_updating);
	mp->mnt_flag &= ~MNT_OP_FLAGS;
	mp->mnt_flag |= MNT_GETARGS;
	error = VFS_MOUNT(mp, path, data, data_len);
	mp->mnt_flag &= ~MNT_OP_FLAGS;
	mutex_exit(&mp->mnt_updating);

	vfs_unbusy(mp, false, NULL);
	return (error);
}

int
sys___mount50(struct lwp *l, const struct sys___mount50_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) type;
		syscallarg(const char *) path;
		syscallarg(int) flags;
		syscallarg(void *) data;
		syscallarg(size_t) data_len;
	} */

	return do_sys_mount(l, NULL, SCARG(uap, type), SCARG(uap, path),
	    SCARG(uap, flags), SCARG(uap, data), UIO_USERSPACE,
	    SCARG(uap, data_len), retval);
}

int
do_sys_mount(struct lwp *l, struct vfsops *vfsops, const char *type,
    const char *path, int flags, void *data, enum uio_seg data_seg,
    size_t data_len, register_t *retval)
{
	struct vnode *vp;
	void *data_buf = data;
	bool vfsopsrele = false;
	size_t alloc_sz = 0;
	int error;

	/* XXX: The calling convention of this routine is totally bizarre */
	if (vfsops)
		vfsopsrele = true;

	/*
	 * Get vnode to be covered
	 */
	error = namei_simple_user(path, NSM_FOLLOW_TRYEMULROOT, &vp);
	if (error != 0) {
		vp = NULL;
		goto done;
	}

	if (vfsops == NULL) {
		if (flags & (MNT_GETARGS | MNT_UPDATE)) {
			vfsops = vp->v_mount->mnt_op;
		} else {
			/* 'type' is userspace */
			error = mount_get_vfsops(type, &vfsops);
			if (error != 0)
				goto done;
			vfsopsrele = true;
		}
	}

	/*
	 * We allow data to be NULL, even for userspace. Some fs's don't need
	 * it. The others will handle NULL.
	 */
	if (data != NULL && data_seg == UIO_USERSPACE) {
		if (data_len == 0) {
			/* No length supplied, use default for filesystem */
			data_len = vfsops->vfs_min_mount_data;

			/*
			 * Hopefully a longer buffer won't make copyin() fail.
			 * For compatibility with 3.0 and earlier.
			 */
			if (flags & MNT_UPDATE
			    && data_len < sizeof (struct mnt_export_args30))
				data_len = sizeof (struct mnt_export_args30);
		}
		if ((data_len == 0) || (data_len > VFS_MAX_MOUNT_DATA)) {
			error = EINVAL;
			goto done;
		}
		alloc_sz = data_len;
		data_buf = kmem_alloc(alloc_sz, KM_SLEEP);

		/* NFS needs the buffer even for mnt_getargs .... */
		error = copyin(data, data_buf, data_len);
		if (error != 0)
			goto done;
	}

	if (flags & MNT_GETARGS) {
		if (data_len == 0) {
			error = EINVAL;
			goto done;
		}
		error = mount_getargs(l, vp, path, flags, data_buf, &data_len);
		if (error != 0)
			goto done;
		if (data_seg == UIO_USERSPACE)
			error = copyout(data_buf, data, data_len);
		*retval = data_len;
	} else if (flags & MNT_UPDATE) {
		error = mount_update(l, vp, path, flags, data_buf, &data_len);
	} else {
		/* Locking is handled internally in mount_domount(). */
		KASSERT(vfsopsrele == true);
		error = mount_domount(l, &vp, vfsops, path, flags, data_buf,
		    &data_len);
		vfsopsrele = false;
	}

    done:
	if (vfsopsrele)
		vfs_delref(vfsops);
    	if (vp != NULL) {
	    	vrele(vp);
	}
	if (data_buf != data)
		kmem_free(data_buf, alloc_sz);
	return (error);
}

/*
 * Unmount a file system.
 *
 * Note: unmount takes a path to the vnode mounted on as argument,
 * not special file (as before).
 */
/* ARGSUSED */
int
sys_unmount(struct lwp *l, const struct sys_unmount_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(int) flags;
	} */
	struct vnode *vp;
	struct mount *mp;
	int error;
	struct pathbuf *pb;
	struct nameidata nd;

	error = pathbuf_copyin(SCARG(uap, path), &pb);
	if (error) {
		return error;
	}

	NDINIT(&nd, LOOKUP, NOFOLLOW | LOCKLEAF | TRYEMULROOT, pb);
	if ((error = namei(&nd)) != 0) {
		pathbuf_destroy(pb);
		return error;
	}
	vp = nd.ni_vp;
	pathbuf_destroy(pb);

	mp = vp->v_mount;
	atomic_inc_uint(&mp->mnt_refcnt);
	VOP_UNLOCK(vp);

	error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_MOUNT,
	    KAUTH_REQ_SYSTEM_MOUNT_UNMOUNT, mp, NULL, NULL);
	if (error) {
		vrele(vp);
		vfs_destroy(mp);
		return (error);
	}

	/*
	 * Don't allow unmounting the root file system.
	 */
	if (mp->mnt_flag & MNT_ROOTFS) {
		vrele(vp);
		vfs_destroy(mp);
		return (EINVAL);
	}

	/*
	 * Must be the root of the filesystem
	 */
	if ((vp->v_vflag & VV_ROOT) == 0) {
		vrele(vp);
		vfs_destroy(mp);
		return (EINVAL);
	}

	vrele(vp);
	error = dounmount(mp, SCARG(uap, flags), l);
	vfs_destroy(mp);
	return error;
}

/*
 * Sync each mounted filesystem.
 */
#ifdef DEBUG
int syncprt = 0;
struct ctldebug debug0 = { "syncprt", &syncprt };
#endif

void
do_sys_sync(struct lwp *l)
{
	struct mount *mp, *nmp;
	int asyncflag;

	mutex_enter(&mountlist_lock);
	for (mp = TAILQ_FIRST(&mountlist); mp != NULL; mp = nmp) {
		if (vfs_busy(mp, &nmp)) {
			continue;
		}
		mutex_enter(&mp->mnt_updating);
		if ((mp->mnt_flag & MNT_RDONLY) == 0) {
			asyncflag = mp->mnt_flag & MNT_ASYNC;
			mp->mnt_flag &= ~MNT_ASYNC;
			VFS_SYNC(mp, MNT_NOWAIT, l->l_cred);
			if (asyncflag)
				 mp->mnt_flag |= MNT_ASYNC;
		}
		mutex_exit(&mp->mnt_updating);
		vfs_unbusy(mp, false, &nmp);
	}
	mutex_exit(&mountlist_lock);
#ifdef DEBUG
	if (syncprt)
		vfs_bufstats();
#endif /* DEBUG */
}

/* ARGSUSED */
int
sys_sync(struct lwp *l, const void *v, register_t *retval)
{
	do_sys_sync(l);
	return (0);
}


/*
 * Access or change filesystem quotas.
 *
 * (this is really 14 different calls bundled into one)
 */

static int
do_sys_quotactl_stat(struct mount *mp, struct quotastat *info_u)
{
	struct quotastat info_k;
	int error;

	/* ensure any padding bytes are cleared */
	memset(&info_k, 0, sizeof(info_k));

	error = vfs_quotactl_stat(mp, &info_k);
	if (error) {
		return error;
	}

	return copyout(&info_k, info_u, sizeof(info_k));
}

static int
do_sys_quotactl_idtypestat(struct mount *mp, int idtype,
    struct quotaidtypestat *info_u)
{
	struct quotaidtypestat info_k;
	int error;

	/* ensure any padding bytes are cleared */
	memset(&info_k, 0, sizeof(info_k));

	error = vfs_quotactl_idtypestat(mp, idtype, &info_k);
	if (error) {
		return error;
	}

	return copyout(&info_k, info_u, sizeof(info_k));
}

static int
do_sys_quotactl_objtypestat(struct mount *mp, int objtype,
    struct quotaobjtypestat *info_u)
{
	struct quotaobjtypestat info_k;
	int error;

	/* ensure any padding bytes are cleared */
	memset(&info_k, 0, sizeof(info_k));

	error = vfs_quotactl_objtypestat(mp, objtype, &info_k);
	if (error) {
		return error;
	}

	return copyout(&info_k, info_u, sizeof(info_k));
}

static int
do_sys_quotactl_get(struct mount *mp, const struct quotakey *key_u,
    struct quotaval *val_u)
{
	struct quotakey key_k;
	struct quotaval val_k;
	int error;

	/* ensure any padding bytes are cleared */
	memset(&val_k, 0, sizeof(val_k));

	error = copyin(key_u, &key_k, sizeof(key_k));
	if (error) {
		return error;
	}

	error = vfs_quotactl_get(mp, &key_k, &val_k);
	if (error) {
		return error;
	}

	return copyout(&val_k, val_u, sizeof(val_k));
}

static int
do_sys_quotactl_put(struct mount *mp, const struct quotakey *key_u,
    const struct quotaval *val_u)
{
	struct quotakey key_k;
	struct quotaval val_k;
	int error;

	error = copyin(key_u, &key_k, sizeof(key_k));
	if (error) {
		return error;
	}

	error = copyin(val_u, &val_k, sizeof(val_k));
	if (error) {
		return error;
	}

	return vfs_quotactl_put(mp, &key_k, &val_k);
}

static int
do_sys_quotactl_del(struct mount *mp, const struct quotakey *key_u)
{
	struct quotakey key_k;
	int error;

	error = copyin(key_u, &key_k, sizeof(key_k));
	if (error) {
		return error;
	}

	return vfs_quotactl_del(mp, &key_k);
}

static int
do_sys_quotactl_cursoropen(struct mount *mp, struct quotakcursor *cursor_u)
{
	struct quotakcursor cursor_k;
	int error;

	/* ensure any padding bytes are cleared */
	memset(&cursor_k, 0, sizeof(cursor_k));

	error = vfs_quotactl_cursoropen(mp, &cursor_k);
	if (error) {
		return error;
	}

	return copyout(&cursor_k, cursor_u, sizeof(cursor_k));
}

static int
do_sys_quotactl_cursorclose(struct mount *mp, struct quotakcursor *cursor_u)
{
	struct quotakcursor cursor_k;
	int error;

	error = copyin(cursor_u, &cursor_k, sizeof(cursor_k));
	if (error) {
		return error;
	}

	return vfs_quotactl_cursorclose(mp, &cursor_k);
}

static int
do_sys_quotactl_cursorskipidtype(struct mount *mp,
    struct quotakcursor *cursor_u, int idtype)
{
	struct quotakcursor cursor_k;
	int error;

	error = copyin(cursor_u, &cursor_k, sizeof(cursor_k));
	if (error) {
		return error;
	}

	error = vfs_quotactl_cursorskipidtype(mp, &cursor_k, idtype);
	if (error) {
		return error;
	}

	return copyout(&cursor_k, cursor_u, sizeof(cursor_k));
}

static int
do_sys_quotactl_cursorget(struct mount *mp, struct quotakcursor *cursor_u,
    struct quotakey *keys_u, struct quotaval *vals_u, unsigned maxnum,
    unsigned *ret_u)
{
#define CGET_STACK_MAX 8
	struct quotakcursor cursor_k;
	struct quotakey stackkeys[CGET_STACK_MAX];
	struct quotaval stackvals[CGET_STACK_MAX];
	struct quotakey *keys_k;
	struct quotaval *vals_k;
	unsigned ret_k;
	int error;

	if (maxnum > 128) {
		maxnum = 128;
	}

	error = copyin(cursor_u, &cursor_k, sizeof(cursor_k));
	if (error) {
		return error;
	}

	if (maxnum <= CGET_STACK_MAX) {
		keys_k = stackkeys;
		vals_k = stackvals;
		/* ensure any padding bytes are cleared */
		memset(keys_k, 0, maxnum * sizeof(keys_k[0]));
		memset(vals_k, 0, maxnum * sizeof(vals_k[0]));
	} else {
		keys_k = kmem_zalloc(maxnum * sizeof(keys_k[0]), KM_SLEEP);
		vals_k = kmem_zalloc(maxnum * sizeof(vals_k[0]), KM_SLEEP);
	}

	error = vfs_quotactl_cursorget(mp, &cursor_k, keys_k, vals_k, maxnum,
				       &ret_k);
	if (error) {
		goto fail;
	}

	error = copyout(keys_k, keys_u, ret_k * sizeof(keys_k[0]));
	if (error) {
		goto fail;
	}

	error = copyout(vals_k, vals_u, ret_k * sizeof(vals_k[0]));
	if (error) {
		goto fail;
	}

	error = copyout(&ret_k, ret_u, sizeof(ret_k));
	if (error) {
		goto fail;
	}

	/* do last to maximize the chance of being able to recover a failure */
	error = copyout(&cursor_k, cursor_u, sizeof(cursor_k));

fail:
	if (keys_k != stackkeys) {
		kmem_free(keys_k, maxnum * sizeof(keys_k[0]));
	}
	if (vals_k != stackvals) {
		kmem_free(vals_k, maxnum * sizeof(vals_k[0]));
	}
	return error;
}

static int
do_sys_quotactl_cursoratend(struct mount *mp, struct quotakcursor *cursor_u,
    int *ret_u)
{
	struct quotakcursor cursor_k;
	int ret_k;
	int error;

	error = copyin(cursor_u, &cursor_k, sizeof(cursor_k));
	if (error) {
		return error;
	}

	error = vfs_quotactl_cursoratend(mp, &cursor_k, &ret_k);
	if (error) {
		return error;
	}

	error = copyout(&ret_k, ret_u, sizeof(ret_k));
	if (error) {
		return error;
	}

	return copyout(&cursor_k, cursor_u, sizeof(cursor_k));
}

static int
do_sys_quotactl_cursorrewind(struct mount *mp, struct quotakcursor *cursor_u)
{
	struct quotakcursor cursor_k;
	int error;

	error = copyin(cursor_u, &cursor_k, sizeof(cursor_k));
	if (error) {
		return error;
	}

	error = vfs_quotactl_cursorrewind(mp, &cursor_k);
	if (error) {
		return error;
	}

	return copyout(&cursor_k, cursor_u, sizeof(cursor_k));
}

static int
do_sys_quotactl_quotaon(struct mount *mp, int idtype, const char *path_u)
{
	char *path_k;
	int error;

	/* XXX this should probably be a struct pathbuf */
	path_k = PNBUF_GET();
	error = copyin(path_u, path_k, PATH_MAX);
	if (error) {
		PNBUF_PUT(path_k);
		return error;
	}

	error = vfs_quotactl_quotaon(mp, idtype, path_k);

	PNBUF_PUT(path_k);
	return error;
}

static int
do_sys_quotactl_quotaoff(struct mount *mp, int idtype)
{
	return vfs_quotactl_quotaoff(mp, idtype);
}

int
do_sys_quotactl(const char *path_u, const struct quotactl_args *args)
{
	struct mount *mp;
	struct vnode *vp;
	int error;

	error = namei_simple_user(path_u, NSM_FOLLOW_TRYEMULROOT, &vp);
	if (error != 0)
		return (error);
	mp = vp->v_mount;

	switch (args->qc_op) {
	    case QUOTACTL_STAT:
		error = do_sys_quotactl_stat(mp, args->u.stat.qc_info);
		break;
	    case QUOTACTL_IDTYPESTAT:
		error = do_sys_quotactl_idtypestat(mp,
				args->u.idtypestat.qc_idtype,
				args->u.idtypestat.qc_info);
		break;
	    case QUOTACTL_OBJTYPESTAT:
		error = do_sys_quotactl_objtypestat(mp,
				args->u.objtypestat.qc_objtype,
				args->u.objtypestat.qc_info);
		break;
	    case QUOTACTL_GET:
		error = do_sys_quotactl_get(mp,
				args->u.get.qc_key,
				args->u.get.qc_val);
		break;
	    case QUOTACTL_PUT:
		error = do_sys_quotactl_put(mp,
				args->u.put.qc_key,
				args->u.put.qc_val);
		break;
	    case QUOTACTL_DEL:
		error = do_sys_quotactl_del(mp, args->u.del.qc_key);
		break;
	    case QUOTACTL_CURSOROPEN:
		error = do_sys_quotactl_cursoropen(mp,
				args->u.cursoropen.qc_cursor);
		break;
	    case QUOTACTL_CURSORCLOSE:
		error = do_sys_quotactl_cursorclose(mp,
				args->u.cursorclose.qc_cursor);
		break;
	    case QUOTACTL_CURSORSKIPIDTYPE:
		error = do_sys_quotactl_cursorskipidtype(mp,
				args->u.cursorskipidtype.qc_cursor,
				args->u.cursorskipidtype.qc_idtype);
		break;
	    case QUOTACTL_CURSORGET:
		error = do_sys_quotactl_cursorget(mp,
				args->u.cursorget.qc_cursor,
				args->u.cursorget.qc_keys,
				args->u.cursorget.qc_vals,
				args->u.cursorget.qc_maxnum,
				args->u.cursorget.qc_ret);
		break;
	    case QUOTACTL_CURSORATEND:
		error = do_sys_quotactl_cursoratend(mp,
				args->u.cursoratend.qc_cursor,
				args->u.cursoratend.qc_ret);
		break;
	    case QUOTACTL_CURSORREWIND:
		error = do_sys_quotactl_cursorrewind(mp,
				args->u.cursorrewind.qc_cursor);
		break;
	    case QUOTACTL_QUOTAON:
		error = do_sys_quotactl_quotaon(mp,
				args->u.quotaon.qc_idtype,
				args->u.quotaon.qc_quotafile);
		break;
	    case QUOTACTL_QUOTAOFF:
		error = do_sys_quotactl_quotaoff(mp,
				args->u.quotaoff.qc_idtype);
		break;
	    default:
		error = EINVAL;
		break;
	}

	vrele(vp);
	return error;
}

/* ARGSUSED */
int
sys___quotactl(struct lwp *l, const struct sys___quotactl_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(struct quotactl_args *) args;
	} */
	struct quotactl_args args;
	int error;

	error = copyin(SCARG(uap, args), &args, sizeof(args));
	if (error) {
		return error;
	}

	return do_sys_quotactl(SCARG(uap, path), &args);
}

int
dostatvfs(struct mount *mp, struct statvfs *sp, struct lwp *l, int flags,
    int root)
{
	struct cwdinfo *cwdi = l->l_proc->p_cwdi;
	int error = 0;

	/*
	 * If MNT_NOWAIT or MNT_LAZY is specified, do not
	 * refresh the fsstat cache. MNT_WAIT or MNT_LAZY
	 * overrides MNT_NOWAIT.
	 */
	if (flags == MNT_NOWAIT	|| flags == MNT_LAZY ||
	    (flags != MNT_WAIT && flags != 0)) {
		memcpy(sp, &mp->mnt_stat, sizeof(*sp));
		goto done;
	}

	/* Get the filesystem stats now */
	memset(sp, 0, sizeof(*sp));
	if ((error = VFS_STATVFS(mp, sp)) != 0) {
		return error;
	}

	if (cwdi->cwdi_rdir == NULL)
		(void)memcpy(&mp->mnt_stat, sp, sizeof(mp->mnt_stat));
done:
	if (cwdi->cwdi_rdir != NULL) {
		size_t len;
		char *bp;
		char c;
		char *path = PNBUF_GET();

		bp = path + MAXPATHLEN;
		*--bp = '\0';
		rw_enter(&cwdi->cwdi_lock, RW_READER);
		error = getcwd_common(cwdi->cwdi_rdir, rootvnode, &bp, path,
		    MAXPATHLEN / 2, 0, l);
		rw_exit(&cwdi->cwdi_lock);
		if (error) {
			PNBUF_PUT(path);
			return error;
		}
		len = strlen(bp);
		if (len != 1) {
			/*
			 * for mount points that are below our root, we can see
			 * them, so we fix up the pathname and return them. The
			 * rest we cannot see, so we don't allow viewing the
			 * data.
			 */
			if (strncmp(bp, sp->f_mntonname, len) == 0 &&
			    ((c = sp->f_mntonname[len]) == '/' || c == '\0')) {
				(void)strlcpy(sp->f_mntonname,
				    c == '\0' ? "/" : &sp->f_mntonname[len],
				    sizeof(sp->f_mntonname));
			} else {
				if (root)
					(void)strlcpy(sp->f_mntonname, "/",
					    sizeof(sp->f_mntonname));
				else
					error = EPERM;
			}
		}
		PNBUF_PUT(path);
	}
	sp->f_flag = mp->mnt_flag & MNT_VISFLAGMASK;
	return error;
}

/*
 * Get filesystem statistics by path.
 */
int
do_sys_pstatvfs(struct lwp *l, const char *path, int flags, struct statvfs *sb)
{
	struct mount *mp;
	int error;
	struct vnode *vp;

	error = namei_simple_user(path, NSM_FOLLOW_TRYEMULROOT, &vp);
	if (error != 0)
		return error;
	mp = vp->v_mount;
	error = dostatvfs(mp, sb, l, flags, 1);
	vrele(vp);
	return error;
}

/* ARGSUSED */
int
sys_statvfs1(struct lwp *l, const struct sys_statvfs1_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(struct statvfs *) buf;
		syscallarg(int) flags;
	} */
	struct statvfs *sb;
	int error;

	sb = STATVFSBUF_GET();
	error = do_sys_pstatvfs(l, SCARG(uap, path), SCARG(uap, flags), sb);
	if (error == 0)
		error = copyout(sb, SCARG(uap, buf), sizeof(*sb));
	STATVFSBUF_PUT(sb);
	return error;
}

/*
 * Get filesystem statistics by fd.
 */
int
do_sys_fstatvfs(struct lwp *l, int fd, int flags, struct statvfs *sb)
{
	file_t *fp;
	struct mount *mp;
	int error;

	/* fd_getvnode() will use the descriptor for us */
	if ((error = fd_getvnode(fd, &fp)) != 0)
		return (error);
	mp = fp->f_vnode->v_mount;
	error = dostatvfs(mp, sb, curlwp, flags, 1);
	fd_putfile(fd);
	return error;
}

/* ARGSUSED */
int
sys_fstatvfs1(struct lwp *l, const struct sys_fstatvfs1_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(struct statvfs *) buf;
		syscallarg(int) flags;
	} */
	struct statvfs *sb;
	int error;

	sb = STATVFSBUF_GET();
	error = do_sys_fstatvfs(l, SCARG(uap, fd), SCARG(uap, flags), sb);
	if (error == 0)
		error = copyout(sb, SCARG(uap, buf), sizeof(*sb));
	STATVFSBUF_PUT(sb);
	return error;
}


/*
 * Get statistics on all filesystems.
 */
int
do_sys_getvfsstat(struct lwp *l, void *sfsp, size_t bufsize, int flags,
    int (*copyfn)(const void *, void *, size_t), size_t entry_sz,
    register_t *retval)
{
	int root = 0;
	struct proc *p = l->l_proc;
	struct mount *mp, *nmp;
	struct statvfs *sb;
	size_t count, maxcount;
	int error = 0;

	sb = STATVFSBUF_GET();
	maxcount = bufsize / entry_sz;
	mutex_enter(&mountlist_lock);
	count = 0;
	for (mp = TAILQ_FIRST(&mountlist); mp != NULL; mp = nmp) {
		if (vfs_busy(mp, &nmp)) {
			continue;
		}
		if (sfsp && count < maxcount) {
			error = dostatvfs(mp, sb, l, flags, 0);
			if (error) {
				vfs_unbusy(mp, false, &nmp);
				error = 0;
				continue;
			}
			error = copyfn(sb, sfsp, entry_sz);
			if (error) {
				vfs_unbusy(mp, false, NULL);
				goto out;
			}
			sfsp = (char *)sfsp + entry_sz;
			root |= strcmp(sb->f_mntonname, "/") == 0;
		}
		count++;
		vfs_unbusy(mp, false, &nmp);
	}
	mutex_exit(&mountlist_lock);

	if (root == 0 && p->p_cwdi->cwdi_rdir) {
		/*
		 * fake a root entry
		 */
		error = dostatvfs(p->p_cwdi->cwdi_rdir->v_mount,
		    sb, l, flags, 1);
		if (error != 0)
			goto out;
		if (sfsp) {
			error = copyfn(sb, sfsp, entry_sz);
			if (error != 0)
				goto out;
		}
		count++;
	}
	if (sfsp && count > maxcount)
		*retval = maxcount;
	else
		*retval = count;
out:
	STATVFSBUF_PUT(sb);
	return error;
}

int
sys_getvfsstat(struct lwp *l, const struct sys_getvfsstat_args *uap, register_t *retval)
{
	/* {
		syscallarg(struct statvfs *) buf;
		syscallarg(size_t) bufsize;
		syscallarg(int) flags;
	} */

	return do_sys_getvfsstat(l, SCARG(uap, buf), SCARG(uap, bufsize),
	    SCARG(uap, flags), copyout, sizeof (struct statvfs), retval);
}

/*
 * Change current working directory to a given file descriptor.
 */
/* ARGSUSED */
int
sys_fchdir(struct lwp *l, const struct sys_fchdir_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fd;
	} */
	struct proc *p = l->l_proc;
	struct cwdinfo *cwdi;
	struct vnode *vp, *tdp;
	struct mount *mp;
	file_t *fp;
	int error, fd;

	/* fd_getvnode() will use the descriptor for us */
	fd = SCARG(uap, fd);
	if ((error = fd_getvnode(fd, &fp)) != 0)
		return (error);
	vp = fp->f_vnode;

	vref(vp);
	vn_lock(vp,  LK_EXCLUSIVE | LK_RETRY);
	if (vp->v_type != VDIR)
		error = ENOTDIR;
	else
		error = VOP_ACCESS(vp, VEXEC, l->l_cred);
	if (error) {
		vput(vp);
		goto out;
	}
	while ((mp = vp->v_mountedhere) != NULL) {
		error = vfs_busy(mp, NULL);
		vput(vp);
		if (error != 0)
			goto out;
		error = VFS_ROOT(mp, &tdp);
		vfs_unbusy(mp, false, NULL);
		if (error)
			goto out;
		vp = tdp;
	}
	VOP_UNLOCK(vp);

	/*
	 * Disallow changing to a directory not under the process's
	 * current root directory (if there is one).
	 */
	cwdi = p->p_cwdi;
	rw_enter(&cwdi->cwdi_lock, RW_WRITER);
	if (cwdi->cwdi_rdir && !vn_isunder(vp, NULL, l)) {
		vrele(vp);
		error = EPERM;	/* operation not permitted */
	} else {
		vrele(cwdi->cwdi_cdir);
		cwdi->cwdi_cdir = vp;
	}
	rw_exit(&cwdi->cwdi_lock);

 out:
	fd_putfile(fd);
	return (error);
}

/*
 * Change this process's notion of the root directory to a given file
 * descriptor.
 */
int
sys_fchroot(struct lwp *l, const struct sys_fchroot_args *uap, register_t *retval)
{
	struct proc *p = l->l_proc;
	struct vnode	*vp;
	file_t	*fp;
	int		 error, fd = SCARG(uap, fd);

	if ((error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_CHROOT,
 	    KAUTH_REQ_SYSTEM_CHROOT_FCHROOT, NULL, NULL, NULL)) != 0)
		return error;
	/* fd_getvnode() will use the descriptor for us */
	if ((error = fd_getvnode(fd, &fp)) != 0)
		return error;
	vp = fp->f_vnode;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	if (vp->v_type != VDIR)
		error = ENOTDIR;
	else
		error = VOP_ACCESS(vp, VEXEC, l->l_cred);
	VOP_UNLOCK(vp);
	if (error)
		goto out;
	vref(vp);

	change_root(p->p_cwdi, vp, l);

 out:
	fd_putfile(fd);
	return (error);
}

/*
 * Change current working directory (``.'').
 */
/* ARGSUSED */
int
sys_chdir(struct lwp *l, const struct sys_chdir_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
	} */
	struct proc *p = l->l_proc;
	struct cwdinfo *cwdi;
	int error;
	struct vnode *vp;

	if ((error = chdir_lookup(SCARG(uap, path), UIO_USERSPACE,
				  &vp, l)) != 0)
		return (error);
	cwdi = p->p_cwdi;
	rw_enter(&cwdi->cwdi_lock, RW_WRITER);
	vrele(cwdi->cwdi_cdir);
	cwdi->cwdi_cdir = vp;
	rw_exit(&cwdi->cwdi_lock);
	return (0);
}

/*
 * Change notion of root (``/'') directory.
 */
/* ARGSUSED */
int
sys_chroot(struct lwp *l, const struct sys_chroot_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
	} */
	struct proc *p = l->l_proc;
	int error;
	struct vnode *vp;

	if ((error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_CHROOT,
	    KAUTH_REQ_SYSTEM_CHROOT_CHROOT, NULL, NULL, NULL)) != 0)
		return (error);
	if ((error = chdir_lookup(SCARG(uap, path), UIO_USERSPACE,
				  &vp, l)) != 0)
		return (error);

	change_root(p->p_cwdi, vp, l);

	return (0);
}

/*
 * Common routine for chroot and fchroot.
 * NB: callers need to properly authorize the change root operation.
 */
void
change_root(struct cwdinfo *cwdi, struct vnode *vp, struct lwp *l)
{
	struct proc *p = l->l_proc;
	kauth_cred_t ncred;

	ncred = kauth_cred_alloc();

	rw_enter(&cwdi->cwdi_lock, RW_WRITER);
	if (cwdi->cwdi_rdir != NULL)
		vrele(cwdi->cwdi_rdir);
	cwdi->cwdi_rdir = vp;

	/*
	 * Prevent escaping from chroot by putting the root under
	 * the working directory.  Silently chdir to / if we aren't
	 * already there.
	 */
	if (!vn_isunder(cwdi->cwdi_cdir, vp, l)) {
		/*
		 * XXX would be more failsafe to change directory to a
		 * deadfs node here instead
		 */
		vrele(cwdi->cwdi_cdir);
		vref(vp);
		cwdi->cwdi_cdir = vp;
	}
	rw_exit(&cwdi->cwdi_lock);

	/* Get a write lock on the process credential. */
	proc_crmod_enter();

	kauth_cred_clone(p->p_cred, ncred);
	kauth_proc_chroot(ncred, p->p_cwdi);

	/* Broadcast our credentials to the process and other LWPs. */
 	proc_crmod_leave(ncred, p->p_cred, true);
}

/*
 * Common routine for chroot and chdir.
 * XXX "where" should be enum uio_seg
 */
int
chdir_lookup(const char *path, int where, struct vnode **vpp, struct lwp *l)
{
	struct pathbuf *pb;
	struct nameidata nd;
	int error;

	error = pathbuf_maybe_copyin(path, where, &pb);
	if (error) {
		return error;
	}
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF | TRYEMULROOT, pb);
	if ((error = namei(&nd)) != 0) {
		pathbuf_destroy(pb);
		return error;
	}
	*vpp = nd.ni_vp;
	pathbuf_destroy(pb);

	if ((*vpp)->v_type != VDIR)
		error = ENOTDIR;
	else
		error = VOP_ACCESS(*vpp, VEXEC, l->l_cred);

	if (error)
		vput(*vpp);
	else
		VOP_UNLOCK(*vpp);
	return (error);
}

/*
 * Internals of sys_open - path has already been converted into a pathbuf
 * (so we can easily reuse this function from other parts of the kernel,
 * like posix_spawn post-processing).
 */
int
do_open(lwp_t *l, struct vnode *dvp, struct pathbuf *pb, int open_flags, 
	int open_mode, int *fd)
{
	struct proc *p = l->l_proc;
	struct cwdinfo *cwdi = p->p_cwdi;
	file_t *fp;
	struct vnode *vp;
	int flags, cmode;
	int indx, error;
	struct nameidata nd;

	if (open_flags & O_SEARCH) {
		open_flags &= ~(int)O_SEARCH;
	}

	flags = FFLAGS(open_flags);
	if ((flags & (FREAD | FWRITE)) == 0)
		return EINVAL;

	if ((error = fd_allocfile(&fp, &indx)) != 0) {
		return error;
	}

	/* We're going to read cwdi->cwdi_cmask unlocked here. */
	cmode = ((open_mode &~ cwdi->cwdi_cmask) & ALLPERMS) &~ S_ISTXT;
	NDINIT(&nd, LOOKUP, FOLLOW | TRYEMULROOT, pb);
	if (dvp != NULL)
		NDAT(&nd, dvp);
	
	l->l_dupfd = -indx - 1;			/* XXX check for fdopen */
	if ((error = vn_open(&nd, flags, cmode)) != 0) {
		fd_abort(p, fp, indx);
		if ((error == EDUPFD || error == EMOVEFD) &&
		    l->l_dupfd >= 0 &&			/* XXX from fdopen */
		    (error =
			fd_dupopen(l->l_dupfd, &indx, flags, error)) == 0) {
			*fd = indx;
			return 0;
		}
		if (error == ERESTART)
			error = EINTR;
		return error;
	}

	l->l_dupfd = 0;
	vp = nd.ni_vp;

	if ((error = open_setfp(l, fp, vp, indx, flags)))
		return error;

	VOP_UNLOCK(vp);
	*fd = indx;
	fd_affix(p, fp, indx);
	return 0;
}

int
fd_open(const char *path, int open_flags, int open_mode, int *fd)
{
	struct pathbuf *pb;
	int error, oflags;

	oflags = FFLAGS(open_flags);
	if ((oflags & (FREAD | FWRITE)) == 0)
		return EINVAL;

	pb = pathbuf_create(path);
	if (pb == NULL)
		return ENOMEM;

	error = do_open(curlwp, NULL, pb, open_flags, open_mode, fd);
	pathbuf_destroy(pb);

	return error;
}

/*
 * Check permissions, allocate an open file structure,
 * and call the device open routine if any.
 */
static int
do_sys_openat(lwp_t *l, int fdat, const char *path, int flags,
    int mode, int *fd)
{
	file_t *dfp = NULL;
	struct vnode *dvp = NULL;
	struct pathbuf *pb;
	int error;

#ifdef COMPAT_10	/* XXX: and perhaps later */
	if (path == NULL) {
		pb = pathbuf_create(".");
		if (pb == NULL)
			return ENOMEM;
	} else
#endif
	{
		error = pathbuf_copyin(path, &pb);
		if (error)
			return error;
	}

	if (fdat != AT_FDCWD) {
		/* fd_getvnode() will use the descriptor for us */
		if ((error = fd_getvnode(fdat, &dfp)) != 0)
			goto out;

		dvp = dfp->f_vnode;
	}

	error = do_open(l, dvp, pb, flags, mode, fd);

	if (dfp != NULL)
		fd_putfile(fdat);
out:
	pathbuf_destroy(pb);
	return error;
}

int
sys_open(struct lwp *l, const struct sys_open_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(int) flags;
		syscallarg(int) mode;
	} */
	int error;
	int fd;

	error = do_sys_openat(l, AT_FDCWD, SCARG(uap, path),
			      SCARG(uap, flags), SCARG(uap, mode), &fd);

	if (error == 0)
		*retval = fd;

	return error;
}

int
sys_openat(struct lwp *l, const struct sys_openat_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(const char *) path;
		syscallarg(int) oflags;
		syscallarg(int) mode;
	} */
	int error;
	int fd;

	error = do_sys_openat(l, SCARG(uap, fd), SCARG(uap, path),
			      SCARG(uap, oflags), SCARG(uap, mode), &fd);

	if (error == 0)
		*retval = fd;

	return error;
}

static void
vfs__fhfree(fhandle_t *fhp)
{
	size_t fhsize;

	fhsize = FHANDLE_SIZE(fhp);
	kmem_free(fhp, fhsize);
}

/*
 * vfs_composefh: compose a filehandle.
 */

int
vfs_composefh(struct vnode *vp, fhandle_t *fhp, size_t *fh_size)
{
	struct mount *mp;
	struct fid *fidp;
	int error;
	size_t needfhsize;
	size_t fidsize;

	mp = vp->v_mount;
	fidp = NULL;
	if (*fh_size < FHANDLE_SIZE_MIN) {
		fidsize = 0;
	} else {
		fidsize = *fh_size - offsetof(fhandle_t, fh_fid);
		if (fhp != NULL) {
			memset(fhp, 0, *fh_size);
			fhp->fh_fsid = mp->mnt_stat.f_fsidx;
			fidp = &fhp->fh_fid;
		}
	}
	error = VFS_VPTOFH(vp, fidp, &fidsize);
	needfhsize = FHANDLE_SIZE_FROM_FILEID_SIZE(fidsize);
	if (error == 0 && *fh_size < needfhsize) {
		error = E2BIG;
	}
	*fh_size = needfhsize;
	return error;
}

int
vfs_composefh_alloc(struct vnode *vp, fhandle_t **fhpp)
{
	struct mount *mp;
	fhandle_t *fhp;
	size_t fhsize;
	size_t fidsize;
	int error;

	mp = vp->v_mount;
	fidsize = 0;
	error = VFS_VPTOFH(vp, NULL, &fidsize);
	KASSERT(error != 0);
	if (error != E2BIG) {
		goto out;
	}
	fhsize = FHANDLE_SIZE_FROM_FILEID_SIZE(fidsize);
	fhp = kmem_zalloc(fhsize, KM_SLEEP);
	if (fhp == NULL) {
		error = ENOMEM;
		goto out;
	}
	fhp->fh_fsid = mp->mnt_stat.f_fsidx;
	error = VFS_VPTOFH(vp, &fhp->fh_fid, &fidsize);
	if (error == 0) {
		KASSERT((FHANDLE_SIZE(fhp) == fhsize &&
		    FHANDLE_FILEID(fhp)->fid_len == fidsize));
		*fhpp = fhp;
	} else {
		kmem_free(fhp, fhsize);
	}
out:
	return error;
}

void
vfs_composefh_free(fhandle_t *fhp)
{

	vfs__fhfree(fhp);
}

/*
 * vfs_fhtovp: lookup a vnode by a filehandle.
 */

int
vfs_fhtovp(fhandle_t *fhp, struct vnode **vpp)
{
	struct mount *mp;
	int error;

	*vpp = NULL;
	mp = vfs_getvfs(FHANDLE_FSID(fhp));
	if (mp == NULL) {
		error = ESTALE;
		goto out;
	}
	if (mp->mnt_op->vfs_fhtovp == NULL) {
		error = EOPNOTSUPP;
		goto out;
	}
	error = VFS_FHTOVP(mp, FHANDLE_FILEID(fhp), vpp);
out:
	return error;
}

/*
 * vfs_copyinfh_alloc: allocate and copyin a filehandle, given
 * the needed size.
 */

int
vfs_copyinfh_alloc(const void *ufhp, size_t fhsize, fhandle_t **fhpp)
{
	fhandle_t *fhp;
	int error;

	if (fhsize > FHANDLE_SIZE_MAX) {
		return EINVAL;
	}
	if (fhsize < FHANDLE_SIZE_MIN) {
		return EINVAL;
	}
again:
	fhp = kmem_alloc(fhsize, KM_SLEEP);
	if (fhp == NULL) {
		return ENOMEM;
	}
	error = copyin(ufhp, fhp, fhsize);
	if (error == 0) {
		/* XXX this check shouldn't be here */
		if (FHANDLE_SIZE(fhp) == fhsize) {
			*fhpp = fhp;
			return 0;
		} else if (fhsize == NFSX_V2FH && FHANDLE_SIZE(fhp) < fhsize) {
			/*
			 * a kludge for nfsv2 padded handles.
			 */
			size_t sz;

			sz = FHANDLE_SIZE(fhp);
			kmem_free(fhp, fhsize);
			fhsize = sz;
			goto again;
		} else {
			/*
			 * userland told us wrong size.
			 */
		    	error = EINVAL;
		}
	}
	kmem_free(fhp, fhsize);
	return error;
}

void
vfs_copyinfh_free(fhandle_t *fhp)
{

	vfs__fhfree(fhp);
}

/*
 * Get file handle system call
 */
int
sys___getfh30(struct lwp *l, const struct sys___getfh30_args *uap, register_t *retval)
{
	/* {
		syscallarg(char *) fname;
		syscallarg(fhandle_t *) fhp;
		syscallarg(size_t *) fh_size;
	} */
	struct vnode *vp;
	fhandle_t *fh;
	int error;
	struct pathbuf *pb;
	struct nameidata nd;
	size_t sz;
	size_t usz;

	/*
	 * Must be super user
	 */
	error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_FILEHANDLE,
	    0, NULL, NULL, NULL);
	if (error)
		return (error);

	error = pathbuf_copyin(SCARG(uap, fname), &pb);
	if (error) {
		return error;
	}
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF | TRYEMULROOT, pb);
	error = namei(&nd);
	if (error) {
		pathbuf_destroy(pb);
		return error;
	}
	vp = nd.ni_vp;
	pathbuf_destroy(pb);

	error = vfs_composefh_alloc(vp, &fh);
	vput(vp);
	if (error != 0) {
		return error;
	}
	error = copyin(SCARG(uap, fh_size), &usz, sizeof(size_t));
	if (error != 0) {
		goto out;
	}
	sz = FHANDLE_SIZE(fh);
	error = copyout(&sz, SCARG(uap, fh_size), sizeof(size_t));
	if (error != 0) {
		goto out;
	}
	if (usz >= sz) {
		error = copyout(fh, SCARG(uap, fhp), sz);
	} else {
		error = E2BIG;
	}
out:
	vfs_composefh_free(fh);
	return (error);
}

/*
 * Open a file given a file handle.
 *
 * Check permissions, allocate an open file structure,
 * and call the device open routine if any.
 */

int
dofhopen(struct lwp *l, const void *ufhp, size_t fhsize, int oflags,
    register_t *retval)
{
	file_t *fp;
	struct vnode *vp = NULL;
	kauth_cred_t cred = l->l_cred;
	file_t *nfp;
	int indx, error;
	struct vattr va;
	fhandle_t *fh;
	int flags;
	proc_t *p;

	p = curproc;

	/*
	 * Must be super user
	 */
	if ((error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_FILEHANDLE,
	    0, NULL, NULL, NULL)))
		return (error);

	if (oflags & O_SEARCH) {
		oflags &= ~(int)O_SEARCH;
	}

	flags = FFLAGS(oflags);
	if ((flags & (FREAD | FWRITE)) == 0)
		return (EINVAL);
	if ((flags & O_CREAT))
		return (EINVAL);
	if ((error = fd_allocfile(&nfp, &indx)) != 0)
		return (error);
	fp = nfp;
	error = vfs_copyinfh_alloc(ufhp, fhsize, &fh);
	if (error != 0) {
		goto bad;
	}
	error = vfs_fhtovp(fh, &vp);
	vfs_copyinfh_free(fh);
	if (error != 0) {
		goto bad;
	}

	/* Now do an effective vn_open */

	if (vp->v_type == VSOCK) {
		error = EOPNOTSUPP;
		goto bad;
	}
	error = vn_openchk(vp, cred, flags);
	if (error != 0)
		goto bad;
	if (flags & O_TRUNC) {
		VOP_UNLOCK(vp);			/* XXX */
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);   /* XXX */
		vattr_null(&va);
		va.va_size = 0;
		error = VOP_SETATTR(vp, &va, cred);
		if (error)
			goto bad;
	}
	if ((error = VOP_OPEN(vp, flags, cred)) != 0)
		goto bad;
	if (flags & FWRITE) {
		mutex_enter(vp->v_interlock);
		vp->v_writecount++;
		mutex_exit(vp->v_interlock);
	}

	/* done with modified vn_open, now finish what sys_open does. */
	if ((error = open_setfp(l, fp, vp, indx, flags)))
		return error;

	VOP_UNLOCK(vp);
	*retval = indx;
	fd_affix(p, fp, indx);
	return (0);

bad:
	fd_abort(p, fp, indx);
	if (vp != NULL)
		vput(vp);
	return (error);
}

int
sys___fhopen40(struct lwp *l, const struct sys___fhopen40_args *uap, register_t *retval)
{
	/* {
		syscallarg(const void *) fhp;
		syscallarg(size_t) fh_size;
		syscallarg(int) flags;
	} */

	return dofhopen(l, SCARG(uap, fhp), SCARG(uap, fh_size),
	    SCARG(uap, flags), retval);
}

int
do_fhstat(struct lwp *l, const void *ufhp, size_t fhsize, struct stat *sb)
{
	int error;
	fhandle_t *fh;
	struct vnode *vp;

	/*
	 * Must be super user
	 */
	if ((error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_FILEHANDLE,
	    0, NULL, NULL, NULL)))
		return (error);

	error = vfs_copyinfh_alloc(ufhp, fhsize, &fh);
	if (error != 0)
		return error;

	error = vfs_fhtovp(fh, &vp);
	vfs_copyinfh_free(fh);
	if (error != 0)
		return error;

	error = vn_stat(vp, sb);
	vput(vp);
	return error;
}


/* ARGSUSED */
int
sys___fhstat50(struct lwp *l, const struct sys___fhstat50_args *uap, register_t *retval)
{
	/* {
		syscallarg(const void *) fhp;
		syscallarg(size_t) fh_size;
		syscallarg(struct stat *) sb;
	} */
	struct stat sb;
	int error;

	error = do_fhstat(l, SCARG(uap, fhp), SCARG(uap, fh_size), &sb);
	if (error)
		return error;
	return copyout(&sb, SCARG(uap, sb), sizeof(sb));
}

int
do_fhstatvfs(struct lwp *l, const void *ufhp, size_t fhsize, struct statvfs *sb,
    int flags)
{
	fhandle_t *fh;
	struct mount *mp;
	struct vnode *vp;
	int error;

	/*
	 * Must be super user
	 */
	if ((error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_FILEHANDLE,
	    0, NULL, NULL, NULL)))
		return error;

	error = vfs_copyinfh_alloc(ufhp, fhsize, &fh);
	if (error != 0)
		return error;

	error = vfs_fhtovp(fh, &vp);
	vfs_copyinfh_free(fh);
	if (error != 0)
		return error;

	mp = vp->v_mount;
	error = dostatvfs(mp, sb, l, flags, 1);
	vput(vp);
	return error;
}

/* ARGSUSED */
int
sys___fhstatvfs140(struct lwp *l, const struct sys___fhstatvfs140_args *uap, register_t *retval)
{
	/* {
		syscallarg(const void *) fhp;
		syscallarg(size_t) fh_size;
		syscallarg(struct statvfs *) buf;
		syscallarg(int)	flags;
	} */
	struct statvfs *sb = STATVFSBUF_GET();
	int error;

	error = do_fhstatvfs(l, SCARG(uap, fhp), SCARG(uap, fh_size), sb,
	    SCARG(uap, flags));
	if (error == 0)
		error = copyout(sb, SCARG(uap, buf), sizeof(*sb));
	STATVFSBUF_PUT(sb);
	return error;
}

/*
 * Create a special file.
 */
/* ARGSUSED */
int
sys___mknod50(struct lwp *l, const struct sys___mknod50_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(mode_t) mode;
		syscallarg(dev_t) dev;
	} */
	return do_sys_mknodat(l, AT_FDCWD, SCARG(uap, path), SCARG(uap, mode),
	    SCARG(uap, dev), retval, UIO_USERSPACE);
}

int
sys_mknodat(struct lwp *l, const struct sys_mknodat_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(const char *) path;
		syscallarg(mode_t) mode;
		syscallarg(int) pad;
		syscallarg(dev_t) dev;
	} */

	return do_sys_mknodat(l, SCARG(uap, fd), SCARG(uap, path), 
	    SCARG(uap, mode), SCARG(uap, dev), retval, UIO_USERSPACE);
}

int
do_sys_mknod(struct lwp *l, const char *pathname, mode_t mode, dev_t dev,
    register_t *retval, enum uio_seg seg)
{
	return do_sys_mknodat(l, AT_FDCWD, pathname, mode, dev, retval, seg);
}

int
do_sys_mknodat(struct lwp *l, int fdat, const char *pathname, mode_t mode,
    dev_t dev, register_t *retval, enum uio_seg seg)
{
	struct proc *p = l->l_proc;
	struct vnode *vp;
	struct vattr vattr;
	int error, optype;
	struct pathbuf *pb;
	struct nameidata nd;
	const char *pathstring;

	if ((error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_MKNOD,
	    0, NULL, NULL, NULL)) != 0)
		return (error);

	optype = VOP_MKNOD_DESCOFFSET;

	error = pathbuf_maybe_copyin(pathname, seg, &pb);
	if (error) {
		return error;
	}
	pathstring = pathbuf_stringcopy_get(pb);
	if (pathstring == NULL) {
		pathbuf_destroy(pb);
		return ENOMEM;
	}

	NDINIT(&nd, CREATE, LOCKPARENT | TRYEMULROOT, pb);

	if ((error = fd_nameiat(l, fdat, &nd)) != 0)
		goto out;
	vp = nd.ni_vp;

	if (vp != NULL)
		error = EEXIST;
	else {
		vattr_null(&vattr);
		/* We will read cwdi->cwdi_cmask unlocked. */
		vattr.va_mode = (mode & ALLPERMS) &~ p->p_cwdi->cwdi_cmask;
		vattr.va_rdev = dev;

		switch (mode & S_IFMT) {
		case S_IFMT:	/* used by badsect to flag bad sectors */
			vattr.va_type = VBAD;
			break;
		case S_IFCHR:
			vattr.va_type = VCHR;
			break;
		case S_IFBLK:
			vattr.va_type = VBLK;
			break;
		case S_IFWHT:
			optype = VOP_WHITEOUT_DESCOFFSET;
			break;
		case S_IFREG:
#if NVERIEXEC > 0
			error = veriexec_openchk(l, nd.ni_vp, pathstring,
			    O_CREAT);
#endif /* NVERIEXEC > 0 */
			vattr.va_type = VREG;
			vattr.va_rdev = VNOVAL;
			optype = VOP_CREATE_DESCOFFSET;
			break;
		default:
			error = EINVAL;
			break;
		}
	}
	if (error == 0 && optype == VOP_MKNOD_DESCOFFSET
	    && vattr.va_rdev == VNOVAL)
		error = EINVAL;
	if (!error) {
		switch (optype) {
		case VOP_WHITEOUT_DESCOFFSET:
			error = VOP_WHITEOUT(nd.ni_dvp, &nd.ni_cnd, CREATE);
			if (error)
				VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
			vput(nd.ni_dvp);
			break;

		case VOP_MKNOD_DESCOFFSET:
			error = VOP_MKNOD(nd.ni_dvp, &nd.ni_vp,
						&nd.ni_cnd, &vattr);
			if (error == 0)
				vrele(nd.ni_vp);
			vput(nd.ni_dvp);
			break;

		case VOP_CREATE_DESCOFFSET:
			error = VOP_CREATE(nd.ni_dvp, &nd.ni_vp,
						&nd.ni_cnd, &vattr);
			if (error == 0)
				vrele(nd.ni_vp);
			vput(nd.ni_dvp);
			break;
		}
	} else {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		if (vp)
			vrele(vp);
	}
out:
	pathbuf_stringcopy_put(pb, pathstring);
	pathbuf_destroy(pb);
	return (error);
}

/*
 * Create a named pipe.
 */
/* ARGSUSED */
int
sys_mkfifo(struct lwp *l, const struct sys_mkfifo_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(int) mode;
	} */
	return do_sys_mkfifoat(l, AT_FDCWD, SCARG(uap, path), SCARG(uap, mode));
}

int
sys_mkfifoat(struct lwp *l, const struct sys_mkfifoat_args *uap, 
    register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(const char *) path;
		syscallarg(int) mode;
	} */

	return do_sys_mkfifoat(l, SCARG(uap, fd), SCARG(uap, path), 
	    SCARG(uap, mode));
}

static int
do_sys_mkfifoat(struct lwp *l, int fdat, const char *path, mode_t mode)
{
	struct proc *p = l->l_proc;
	struct vattr vattr;
	int error;
	struct pathbuf *pb;
	struct nameidata nd;

	error = pathbuf_copyin(path, &pb);
	if (error) {
		return error;
	}
	NDINIT(&nd, CREATE, LOCKPARENT | TRYEMULROOT, pb);

	if ((error = fd_nameiat(l, fdat, &nd)) != 0) {
		pathbuf_destroy(pb);
		return error;
	}
	if (nd.ni_vp != NULL) {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vrele(nd.ni_vp);
		pathbuf_destroy(pb);
		return (EEXIST);
	}
	vattr_null(&vattr);
	vattr.va_type = VFIFO;
	/* We will read cwdi->cwdi_cmask unlocked. */
	vattr.va_mode = (mode & ALLPERMS) &~ p->p_cwdi->cwdi_cmask;
	error = VOP_MKNOD(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &vattr);
	if (error == 0)
		vrele(nd.ni_vp);
	vput(nd.ni_dvp);
	pathbuf_destroy(pb);
	return (error);
}

/*
 * Make a hard file link.
 */
/* ARGSUSED */
int
do_sys_linkat(struct lwp *l, int fdpath, const char *path, int fdlink,
    const char *link, int follow, register_t *retval) 
{
	struct vnode *vp;
	struct pathbuf *linkpb;
	struct nameidata nd;
	namei_simple_flags_t ns_flags;
	int error;

	if (follow & AT_SYMLINK_FOLLOW)
		ns_flags = NSM_FOLLOW_TRYEMULROOT;
	else
		ns_flags = NSM_NOFOLLOW_TRYEMULROOT;

	error = fd_nameiat_simple_user(l, fdpath, path, ns_flags, &vp);
	if (error != 0)
		return (error);
	error = pathbuf_copyin(link, &linkpb);
	if (error) {
		goto out1;
	}
	NDINIT(&nd, CREATE, LOCKPARENT | TRYEMULROOT, linkpb);
	if ((error = fd_nameiat(l, fdlink, &nd)) != 0)
		goto out2;
	if (nd.ni_vp) {
		error = EEXIST;
		goto abortop;
	}
	/* Prevent hard links on directories. */
	if (vp->v_type == VDIR) {
		error = EPERM;
		goto abortop;
	}
	/* Prevent cross-mount operation. */
	if (nd.ni_dvp->v_mount != vp->v_mount) {
		error = EXDEV;
		goto abortop;
	}
	error = VOP_LINK(nd.ni_dvp, vp, &nd.ni_cnd);
	VOP_UNLOCK(nd.ni_dvp);
	vrele(nd.ni_dvp);
out2:
	pathbuf_destroy(linkpb);
out1:
	vrele(vp);
	return (error);
abortop:
	VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
	if (nd.ni_dvp == nd.ni_vp)
		vrele(nd.ni_dvp);
	else
		vput(nd.ni_dvp);
	if (nd.ni_vp != NULL)
		vrele(nd.ni_vp);
	goto out2;
}

int
sys_link(struct lwp *l, const struct sys_link_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(const char *) link;
	} */
	const char *path = SCARG(uap, path);
	const char *link = SCARG(uap, link);

	return do_sys_linkat(l, AT_FDCWD, path, AT_FDCWD, link,
	    AT_SYMLINK_FOLLOW, retval);
}

int
sys_linkat(struct lwp *l, const struct sys_linkat_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(int) fd1;
		syscallarg(const char *) name1;
		syscallarg(int) fd2;
		syscallarg(const char *) name2;
		syscallarg(int) flags;
	} */
	int fd1 = SCARG(uap, fd1);
	const char *name1 = SCARG(uap, name1);
	int fd2 = SCARG(uap, fd2);
	const char *name2 = SCARG(uap, name2);
	int follow;

	follow = SCARG(uap, flags) & AT_SYMLINK_FOLLOW;

	return do_sys_linkat(l, fd1, name1, fd2, name2, follow, retval);
}


int
do_sys_symlink(const char *patharg, const char *link, enum uio_seg seg)
{
	return do_sys_symlinkat(NULL, patharg, AT_FDCWD, link, seg);
}

static int
do_sys_symlinkat(struct lwp *l, const char *patharg, int fdat,
    const char *link, enum uio_seg seg)
{
	struct proc *p = curproc;
	struct vattr vattr;
	char *path;
	int error;
	struct pathbuf *linkpb;
	struct nameidata nd;

	KASSERT(l != NULL || fdat == AT_FDCWD);

	path = PNBUF_GET();
	if (seg == UIO_USERSPACE) {
		if ((error = copyinstr(patharg, path, MAXPATHLEN, NULL)) != 0)
			goto out1;
		if ((error = pathbuf_copyin(link, &linkpb)) != 0)
			goto out1;
	} else {
		KASSERT(strlen(patharg) < MAXPATHLEN);
		strcpy(path, patharg);
		linkpb = pathbuf_create(link);
		if (linkpb == NULL) {
			error = ENOMEM;
			goto out1;
		}
	}
	ktrkuser("symlink-target", path, strlen(path));

	NDINIT(&nd, CREATE, LOCKPARENT | TRYEMULROOT, linkpb);
	if ((error = fd_nameiat(l, fdat, &nd)) != 0)
		goto out2;
	if (nd.ni_vp) {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vrele(nd.ni_vp);
		error = EEXIST;
		goto out2;
	}
	vattr_null(&vattr);
	vattr.va_type = VLNK;
	/* We will read cwdi->cwdi_cmask unlocked. */
	vattr.va_mode = ACCESSPERMS &~ p->p_cwdi->cwdi_cmask;
	error = VOP_SYMLINK(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &vattr, path);
	if (error == 0)
		vrele(nd.ni_vp);
	vput(nd.ni_dvp);
out2:
	pathbuf_destroy(linkpb);
out1:
	PNBUF_PUT(path);
	return (error);
}

/*
 * Make a symbolic link.
 */
/* ARGSUSED */
int
sys_symlink(struct lwp *l, const struct sys_symlink_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(const char *) link;
	} */

	return do_sys_symlinkat(l, SCARG(uap, path), AT_FDCWD, SCARG(uap, link),
	    UIO_USERSPACE);
}

int
sys_symlinkat(struct lwp *l, const struct sys_symlinkat_args *uap, 
    register_t *retval)
{
	/* {
		syscallarg(const char *) path1;
		syscallarg(int) fd;
		syscallarg(const char *) path2;
	} */

	return do_sys_symlinkat(l, SCARG(uap, path1), SCARG(uap, fd),
	    SCARG(uap, path2), UIO_USERSPACE);
}

/*
 * Delete a whiteout from the filesystem.
 */
/* ARGSUSED */
int
sys_undelete(struct lwp *l, const struct sys_undelete_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
	} */
	int error;
	struct pathbuf *pb;
	struct nameidata nd;

	error = pathbuf_copyin(SCARG(uap, path), &pb);
	if (error) {
		return error;
	}

	NDINIT(&nd, DELETE, LOCKPARENT | DOWHITEOUT | TRYEMULROOT, pb);
	error = namei(&nd);
	if (error) {
		pathbuf_destroy(pb);
		return (error);
	}

	if (nd.ni_vp != NULLVP || !(nd.ni_cnd.cn_flags & ISWHITEOUT)) {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		if (nd.ni_vp)
			vrele(nd.ni_vp);
		pathbuf_destroy(pb);
		return (EEXIST);
	}
	if ((error = VOP_WHITEOUT(nd.ni_dvp, &nd.ni_cnd, DELETE)) != 0)
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
	vput(nd.ni_dvp);
	pathbuf_destroy(pb);
	return (error);
}

/*
 * Delete a name from the filesystem.
 */
/* ARGSUSED */
int
sys_unlink(struct lwp *l, const struct sys_unlink_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
	} */

	return do_sys_unlinkat(l, AT_FDCWD, SCARG(uap, path), 0, UIO_USERSPACE);
}

int
sys_unlinkat(struct lwp *l, const struct sys_unlinkat_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(const char *) path;
		syscallarg(int) flag;
	} */

	return do_sys_unlinkat(l, SCARG(uap, fd), SCARG(uap, path),
	    SCARG(uap, flag), UIO_USERSPACE);
}

int
do_sys_unlink(const char *arg, enum uio_seg seg)
{
	return do_sys_unlinkat(NULL, AT_FDCWD, arg, 0, seg);
}

static int
do_sys_unlinkat(struct lwp *l, int fdat, const char *arg, int flags,
    enum uio_seg seg)
{
	struct vnode *vp;
	int error;
	struct pathbuf *pb;
	struct nameidata nd;
	const char *pathstring;

	KASSERT(l != NULL || fdat == AT_FDCWD);

	error = pathbuf_maybe_copyin(arg, seg, &pb);
	if (error) {
		return error;
	}
	pathstring = pathbuf_stringcopy_get(pb);
	if (pathstring == NULL) {
		pathbuf_destroy(pb);
		return ENOMEM;
	}

	NDINIT(&nd, DELETE, LOCKPARENT | LOCKLEAF | TRYEMULROOT, pb);
	if ((error = fd_nameiat(l, fdat, &nd)) != 0)
		goto out;
	vp = nd.ni_vp;

	/*
	 * The root of a mounted filesystem cannot be deleted.
	 */
	if ((vp->v_vflag & VV_ROOT) != 0) {
		error = EBUSY;
		goto abort;
	}

	if ((vp->v_type == VDIR) && (vp->v_mountedhere != NULL)) {
		error = EBUSY;
		goto abort;
	}

	/*
	 * No rmdir "." please.
	 */
	if (nd.ni_dvp == vp) {
		error = EINVAL;
		goto abort;
	}

	/*
	 * AT_REMOVEDIR is required to remove a directory
	 */
	if (vp->v_type == VDIR) {
		if (!(flags & AT_REMOVEDIR)) {
			error = EPERM;
			goto abort;
		} else {
			error = VOP_RMDIR(nd.ni_dvp, nd.ni_vp, &nd.ni_cnd);
			goto out;
		}
	}

	/*
	 * Starting here we only deal with non directories.
	 */
	if (flags & AT_REMOVEDIR) {
		error = ENOTDIR;
		goto abort;
	}

#if NVERIEXEC > 0
	/* Handle remove requests for veriexec entries. */
	if ((error = veriexec_removechk(curlwp, nd.ni_vp, pathstring)) != 0) {
		goto abort;
	}
#endif /* NVERIEXEC > 0 */
	
#ifdef FILEASSOC
	(void)fileassoc_file_delete(vp);
#endif /* FILEASSOC */
	error = VOP_REMOVE(nd.ni_dvp, nd.ni_vp, &nd.ni_cnd);
	goto out;

abort:
	VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
	if (nd.ni_dvp == vp)
		vrele(nd.ni_dvp);
	else
		vput(nd.ni_dvp);
	vput(vp);

out:
	pathbuf_stringcopy_put(pb, pathstring);
	pathbuf_destroy(pb);
	return (error);
}

/*
 * Reposition read/write file offset.
 */
int
sys_lseek(struct lwp *l, const struct sys_lseek_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(int) pad;
		syscallarg(off_t) offset;
		syscallarg(int) whence;
	} */
	kauth_cred_t cred = l->l_cred;
	file_t *fp;
	struct vnode *vp;
	struct vattr vattr;
	off_t newoff;
	int error, fd;

	fd = SCARG(uap, fd);

	if ((fp = fd_getfile(fd)) == NULL)
		return (EBADF);

	vp = fp->f_vnode;
	if (fp->f_type != DTYPE_VNODE || vp->v_type == VFIFO) {
		error = ESPIPE;
		goto out;
	}

	switch (SCARG(uap, whence)) {
	case SEEK_CUR:
		newoff = fp->f_offset + SCARG(uap, offset);
		break;
	case SEEK_END:
		vn_lock(vp, LK_SHARED | LK_RETRY);
		error = VOP_GETATTR(vp, &vattr, cred);
		VOP_UNLOCK(vp);
		if (error) {
			goto out;
		}
		newoff = SCARG(uap, offset) + vattr.va_size;
		break;
	case SEEK_SET:
		newoff = SCARG(uap, offset);
		break;
	default:
		error = EINVAL;
		goto out;
	}
	if ((error = VOP_SEEK(vp, fp->f_offset, newoff, cred)) == 0) {
		*(off_t *)retval = fp->f_offset = newoff;
	}
 out:
 	fd_putfile(fd);
	return (error);
}

/*
 * Positional read system call.
 */
int
sys_pread(struct lwp *l, const struct sys_pread_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(void *) buf;
		syscallarg(size_t) nbyte;
		syscallarg(off_t) offset;
	} */
	file_t *fp;
	struct vnode *vp;
	off_t offset;
	int error, fd = SCARG(uap, fd);

	if ((fp = fd_getfile(fd)) == NULL)
		return (EBADF);

	if ((fp->f_flag & FREAD) == 0) {
		fd_putfile(fd);
		return (EBADF);
	}

	vp = fp->f_vnode;
	if (fp->f_type != DTYPE_VNODE || vp->v_type == VFIFO) {
		error = ESPIPE;
		goto out;
	}

	offset = SCARG(uap, offset);

	/*
	 * XXX This works because no file systems actually
	 * XXX take any action on the seek operation.
	 */
	if ((error = VOP_SEEK(vp, fp->f_offset, offset, fp->f_cred)) != 0)
		goto out;

	/* dofileread() will unuse the descriptor for us */
	return (dofileread(fd, fp, SCARG(uap, buf), SCARG(uap, nbyte),
	    &offset, 0, retval));

 out:
	fd_putfile(fd);
	return (error);
}

/*
 * Positional scatter read system call.
 */
int
sys_preadv(struct lwp *l, const struct sys_preadv_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(const struct iovec *) iovp;
		syscallarg(int) iovcnt;
		syscallarg(off_t) offset;
	} */
	off_t offset = SCARG(uap, offset);

	return do_filereadv(SCARG(uap, fd), SCARG(uap, iovp),
	    SCARG(uap, iovcnt), &offset, 0, retval);
}

/*
 * Positional write system call.
 */
int
sys_pwrite(struct lwp *l, const struct sys_pwrite_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(const void *) buf;
		syscallarg(size_t) nbyte;
		syscallarg(off_t) offset;
	} */
	file_t *fp;
	struct vnode *vp;
	off_t offset;
	int error, fd = SCARG(uap, fd);

	if ((fp = fd_getfile(fd)) == NULL)
		return (EBADF);

	if ((fp->f_flag & FWRITE) == 0) {
		fd_putfile(fd);
		return (EBADF);
	}

	vp = fp->f_vnode;
	if (fp->f_type != DTYPE_VNODE || vp->v_type == VFIFO) {
		error = ESPIPE;
		goto out;
	}

	offset = SCARG(uap, offset);

	/*
	 * XXX This works because no file systems actually
	 * XXX take any action on the seek operation.
	 */
	if ((error = VOP_SEEK(vp, fp->f_offset, offset, fp->f_cred)) != 0)
		goto out;

	/* dofilewrite() will unuse the descriptor for us */
	return (dofilewrite(fd, fp, SCARG(uap, buf), SCARG(uap, nbyte),
	    &offset, 0, retval));

 out:
	fd_putfile(fd);
	return (error);
}

/*
 * Positional gather write system call.
 */
int
sys_pwritev(struct lwp *l, const struct sys_pwritev_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(const struct iovec *) iovp;
		syscallarg(int) iovcnt;
		syscallarg(off_t) offset;
	} */
	off_t offset = SCARG(uap, offset);

	return do_filewritev(SCARG(uap, fd), SCARG(uap, iovp),
	    SCARG(uap, iovcnt), &offset, 0, retval);
}

/*
 * Check access permissions.
 */
int
sys_access(struct lwp *l, const struct sys_access_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(int) flags;
	} */

	return do_sys_accessat(l, AT_FDCWD, SCARG(uap, path),
	     SCARG(uap, flags), 0);
}

int
do_sys_accessat(struct lwp *l, int fdat, const char *path,
    int mode, int flags)
{
	kauth_cred_t cred;
	struct vnode *vp;
	int error, nd_flag, vmode;
	struct pathbuf *pb;
	struct nameidata nd;

	CTASSERT(F_OK == 0);
	if ((mode & ~(R_OK | W_OK | X_OK)) != 0) {
		/* nonsense mode */
		return EINVAL;
	}

	nd_flag = FOLLOW | LOCKLEAF | TRYEMULROOT;
	if (flags & AT_SYMLINK_NOFOLLOW)
		nd_flag &= ~FOLLOW;

	error = pathbuf_copyin(path, &pb);
	if (error) 
		return error;

	NDINIT(&nd, LOOKUP, nd_flag, pb);

	/* Override default credentials */
	cred = kauth_cred_dup(l->l_cred);
	if (!(flags & AT_EACCESS)) {
		kauth_cred_seteuid(cred, kauth_cred_getuid(l->l_cred));
		kauth_cred_setegid(cred, kauth_cred_getgid(l->l_cred));
	}
	nd.ni_cnd.cn_cred = cred;

	if ((error = fd_nameiat(l, fdat, &nd)) != 0) {
		pathbuf_destroy(pb);
		goto out;
	}
	vp = nd.ni_vp;
	pathbuf_destroy(pb);

	/* Flags == 0 means only check for existence. */
	if (mode) {
		vmode = 0;
		if (mode & R_OK)
			vmode |= VREAD;
		if (mode & W_OK)
			vmode |= VWRITE;
		if (mode & X_OK)
			vmode |= VEXEC;

		error = VOP_ACCESS(vp, vmode, cred);
		if (!error && (vmode & VWRITE))
			error = vn_writechk(vp);
	}
	vput(vp);
out:
	kauth_cred_free(cred);
	return (error);
}

int
sys_faccessat(struct lwp *l, const struct sys_faccessat_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(const char *) path;
		syscallarg(int) amode;
		syscallarg(int) flag;
	} */

	return do_sys_accessat(l, SCARG(uap, fd), SCARG(uap, path),
	     SCARG(uap, amode), SCARG(uap, flag));
}

/*
 * Common code for all sys_stat functions, including compat versions.
 */
int
do_sys_stat(const char *userpath, unsigned int nd_flag,
    struct stat *sb)
{
	return do_sys_statat(NULL, AT_FDCWD, userpath, nd_flag, sb);
}

int
do_sys_statat(struct lwp *l, int fdat, const char *userpath,
    unsigned int nd_flag, struct stat *sb) 
{
	int error;
	struct pathbuf *pb;
	struct nameidata nd;

	KASSERT(l != NULL || fdat == AT_FDCWD);

	error = pathbuf_copyin(userpath, &pb);
	if (error) {
		return error;
	}

	NDINIT(&nd, LOOKUP, nd_flag | LOCKLEAF | TRYEMULROOT, pb);

	error = fd_nameiat(l, fdat, &nd);
	if (error != 0) {
		pathbuf_destroy(pb);
		return error;
	}
	error = vn_stat(nd.ni_vp, sb);
	vput(nd.ni_vp);
	pathbuf_destroy(pb);
	return error;
}

/*
 * Get file status; this version follows links.
 */
/* ARGSUSED */
int
sys___stat50(struct lwp *l, const struct sys___stat50_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(struct stat *) ub;
	} */
	struct stat sb;
	int error;

	error = do_sys_statat(l, AT_FDCWD, SCARG(uap, path), FOLLOW, &sb);
	if (error)
		return error;
	return copyout(&sb, SCARG(uap, ub), sizeof(sb));
}

/*
 * Get file status; this version does not follow links.
 */
/* ARGSUSED */
int
sys___lstat50(struct lwp *l, const struct sys___lstat50_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(struct stat *) ub;
	} */
	struct stat sb;
	int error;

	error = do_sys_statat(l, AT_FDCWD, SCARG(uap, path), NOFOLLOW, &sb);
	if (error)
		return error;
	return copyout(&sb, SCARG(uap, ub), sizeof(sb));
}

int
sys_fstatat(struct lwp *l, const struct sys_fstatat_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(const char *) path;
		syscallarg(struct stat *) buf;
		syscallarg(int) flag;
	} */
	unsigned int nd_flag;
	struct stat sb;
	int error;

	if (SCARG(uap, flag) & AT_SYMLINK_NOFOLLOW)
		nd_flag = NOFOLLOW;
	else
		nd_flag = FOLLOW;

	error = do_sys_statat(l, SCARG(uap, fd), SCARG(uap, path), nd_flag, 
	    &sb);
	if (error)
		return error;
	return copyout(&sb, SCARG(uap, buf), sizeof(sb));
}

/*
 * Get configurable pathname variables.
 */
/* ARGSUSED */
int
sys_pathconf(struct lwp *l, const struct sys_pathconf_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(int) name;
	} */
	int error;
	struct pathbuf *pb;
	struct nameidata nd;

	error = pathbuf_copyin(SCARG(uap, path), &pb);
	if (error) {
		return error;
	}
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF | TRYEMULROOT, pb);
	if ((error = namei(&nd)) != 0) {
		pathbuf_destroy(pb);
		return (error);
	}
	error = VOP_PATHCONF(nd.ni_vp, SCARG(uap, name), retval);
	vput(nd.ni_vp);
	pathbuf_destroy(pb);
	return (error);
}

/*
 * Return target name of a symbolic link.
 */
/* ARGSUSED */
int
sys_readlink(struct lwp *l, const struct sys_readlink_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(char *) buf;
		syscallarg(size_t) count;
	} */
	return do_sys_readlinkat(l, AT_FDCWD, SCARG(uap, path),
	    SCARG(uap, buf), SCARG(uap, count), retval);
}

static int
do_sys_readlinkat(struct lwp *l, int fdat, const char *path, char *buf,
    size_t count, register_t *retval)
{
	struct vnode *vp;
	struct iovec aiov;
	struct uio auio;
	int error;
	struct pathbuf *pb;
	struct nameidata nd;

	error = pathbuf_copyin(path, &pb);
	if (error) {
		return error;
	}
	NDINIT(&nd, LOOKUP, NOFOLLOW | LOCKLEAF | TRYEMULROOT, pb);
	if ((error = fd_nameiat(l, fdat, &nd)) != 0) {
		pathbuf_destroy(pb);
		return error;
	}
	vp = nd.ni_vp;
	pathbuf_destroy(pb);
	if (vp->v_type != VLNK)
		error = EINVAL;
	else if (!(vp->v_mount->mnt_flag & MNT_SYMPERM) ||
	    (error = VOP_ACCESS(vp, VREAD, l->l_cred)) == 0) {
		aiov.iov_base = buf;
		aiov.iov_len = count;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = 0;
		auio.uio_rw = UIO_READ;
		KASSERT(l == curlwp);
		auio.uio_vmspace = l->l_proc->p_vmspace;
		auio.uio_resid = count;
		if ((error = VOP_READLINK(vp, &auio, l->l_cred)) == 0)
			*retval = count - auio.uio_resid;
	}
	vput(vp);
	return (error);
}

int
sys_readlinkat(struct lwp *l, const struct sys_readlinkat_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(const char *) path;
		syscallarg(char *) buf;
		syscallarg(size_t) bufsize;
	} */

	return do_sys_readlinkat(l, SCARG(uap, fd), SCARG(uap, path),
	    SCARG(uap, buf), SCARG(uap, bufsize), retval);
}

/*
 * Change flags of a file given a path name.
 */
/* ARGSUSED */
int
sys_chflags(struct lwp *l, const struct sys_chflags_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(u_long) flags;
	} */
	struct vnode *vp;
	int error;

	error = namei_simple_user(SCARG(uap, path),
				NSM_FOLLOW_TRYEMULROOT, &vp);
	if (error != 0)
		return (error);
	error = change_flags(vp, SCARG(uap, flags), l);
	vput(vp);
	return (error);
}

/*
 * Change flags of a file given a file descriptor.
 */
/* ARGSUSED */
int
sys_fchflags(struct lwp *l, const struct sys_fchflags_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(u_long) flags;
	} */
	struct vnode *vp;
	file_t *fp;
	int error;

	/* fd_getvnode() will use the descriptor for us */
	if ((error = fd_getvnode(SCARG(uap, fd), &fp)) != 0)
		return (error);
	vp = fp->f_vnode;
	error = change_flags(vp, SCARG(uap, flags), l);
	VOP_UNLOCK(vp);
	fd_putfile(SCARG(uap, fd));
	return (error);
}

/*
 * Change flags of a file given a path name; this version does
 * not follow links.
 */
int
sys_lchflags(struct lwp *l, const struct sys_lchflags_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(u_long) flags;
	} */
	struct vnode *vp;
	int error;

	error = namei_simple_user(SCARG(uap, path),
				NSM_NOFOLLOW_TRYEMULROOT, &vp);
	if (error != 0)
		return (error);
	error = change_flags(vp, SCARG(uap, flags), l);
	vput(vp);
	return (error);
}

/*
 * Common routine to change flags of a file.
 */
int
change_flags(struct vnode *vp, u_long flags, struct lwp *l)
{
	struct vattr vattr;
	int error;

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);

	vattr_null(&vattr);
	vattr.va_flags = flags;
	error = VOP_SETATTR(vp, &vattr, l->l_cred);

	return (error);
}

/*
 * Change mode of a file given path name; this version follows links.
 */
/* ARGSUSED */
int
sys_chmod(struct lwp *l, const struct sys_chmod_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(int) mode;
	} */
	return do_sys_chmodat(l, AT_FDCWD, SCARG(uap, path),
			      SCARG(uap, mode), 0);
}

int
do_sys_chmodat(struct lwp *l, int fdat, const char *path, int mode, int flags)
{
	int error;
	struct vnode *vp;
	namei_simple_flags_t ns_flag;

	if (flags & AT_SYMLINK_NOFOLLOW)
		ns_flag = NSM_NOFOLLOW_TRYEMULROOT;
	else
		ns_flag = NSM_FOLLOW_TRYEMULROOT;

	error = fd_nameiat_simple_user(l, fdat, path, ns_flag, &vp);
	if (error != 0)
		return error;

	error = change_mode(vp, mode, l);

	vrele(vp);

	return (error);
}

/*
 * Change mode of a file given a file descriptor.
 */
/* ARGSUSED */
int
sys_fchmod(struct lwp *l, const struct sys_fchmod_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(int) mode;
	} */
	file_t *fp;
	int error;

	/* fd_getvnode() will use the descriptor for us */
	if ((error = fd_getvnode(SCARG(uap, fd), &fp)) != 0)
		return (error);
	error = change_mode(fp->f_vnode, SCARG(uap, mode), l);
	fd_putfile(SCARG(uap, fd));
	return (error);
}

int
sys_fchmodat(struct lwp *l, const struct sys_fchmodat_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(const char *) path;
		syscallarg(int) mode;
		syscallarg(int) flag;
	} */

	return do_sys_chmodat(l, SCARG(uap, fd), SCARG(uap, path),
			      SCARG(uap, mode), SCARG(uap, flag));
}

/*
 * Change mode of a file given path name; this version does not follow links.
 */
/* ARGSUSED */
int
sys_lchmod(struct lwp *l, const struct sys_lchmod_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(int) mode;
	} */
	int error;
	struct vnode *vp;

	error = namei_simple_user(SCARG(uap, path),
				NSM_NOFOLLOW_TRYEMULROOT, &vp);
	if (error != 0)
		return (error);

	error = change_mode(vp, SCARG(uap, mode), l);

	vrele(vp);
	return (error);
}

/*
 * Common routine to set mode given a vnode.
 */
static int
change_mode(struct vnode *vp, int mode, struct lwp *l)
{
	struct vattr vattr;
	int error;

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	vattr_null(&vattr);
	vattr.va_mode = mode & ALLPERMS;
	error = VOP_SETATTR(vp, &vattr, l->l_cred);
	VOP_UNLOCK(vp);
	return (error);
}

/*
 * Set ownership given a path name; this version follows links.
 */
/* ARGSUSED */
int
sys_chown(struct lwp *l, const struct sys_chown_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(uid_t) uid;
		syscallarg(gid_t) gid;
	} */
	return do_sys_chownat(l, AT_FDCWD, SCARG(uap, path), SCARG(uap,uid),
			      SCARG(uap, gid), 0);
}

int
do_sys_chownat(struct lwp *l, int fdat, const char *path, uid_t uid,
   gid_t gid, int flags)
{
	int error;
	struct vnode *vp;
	namei_simple_flags_t ns_flag;

	if (flags & AT_SYMLINK_NOFOLLOW)
		ns_flag = NSM_NOFOLLOW_TRYEMULROOT;
	else
		ns_flag = NSM_FOLLOW_TRYEMULROOT;

	error = fd_nameiat_simple_user(l, fdat, path, ns_flag, &vp);
	if (error != 0)
		return error;

	error = change_owner(vp, uid, gid, l, 0);

	vrele(vp);

	return (error);
}

/*
 * Set ownership given a path name; this version follows links.
 * Provides POSIX semantics.
 */
/* ARGSUSED */
int
sys___posix_chown(struct lwp *l, const struct sys___posix_chown_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(uid_t) uid;
		syscallarg(gid_t) gid;
	} */
	int error;
	struct vnode *vp;

	error = namei_simple_user(SCARG(uap, path),
				NSM_FOLLOW_TRYEMULROOT, &vp);
	if (error != 0)
		return (error);

	error = change_owner(vp, SCARG(uap, uid), SCARG(uap, gid), l, 1);

	vrele(vp);
	return (error);
}

/*
 * Set ownership given a file descriptor.
 */
/* ARGSUSED */
int
sys_fchown(struct lwp *l, const struct sys_fchown_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(uid_t) uid;
		syscallarg(gid_t) gid;
	} */
	int error;
	file_t *fp;

	/* fd_getvnode() will use the descriptor for us */
	if ((error = fd_getvnode(SCARG(uap, fd), &fp)) != 0)
		return (error);
	error = change_owner(fp->f_vnode, SCARG(uap, uid), SCARG(uap, gid),
	    l, 0);
	fd_putfile(SCARG(uap, fd));
	return (error);
}

int
sys_fchownat(struct lwp *l, const struct sys_fchownat_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(const char *) path;
		syscallarg(uid_t) owner;
		syscallarg(gid_t) group;
		syscallarg(int) flag;
	} */

	return do_sys_chownat(l, SCARG(uap, fd), SCARG(uap, path),
			      SCARG(uap, owner), SCARG(uap, group),
			      SCARG(uap, flag));
}

/*
 * Set ownership given a file descriptor, providing POSIX/XPG semantics.
 */
/* ARGSUSED */
int
sys___posix_fchown(struct lwp *l, const struct sys___posix_fchown_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(uid_t) uid;
		syscallarg(gid_t) gid;
	} */
	int error;
	file_t *fp;

	/* fd_getvnode() will use the descriptor for us */
	if ((error = fd_getvnode(SCARG(uap, fd), &fp)) != 0)
		return (error);
	error = change_owner(fp->f_vnode, SCARG(uap, uid), SCARG(uap, gid),
	    l, 1);
	fd_putfile(SCARG(uap, fd));
	return (error);
}

/*
 * Set ownership given a path name; this version does not follow links.
 */
/* ARGSUSED */
int
sys_lchown(struct lwp *l, const struct sys_lchown_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(uid_t) uid;
		syscallarg(gid_t) gid;
	} */
	int error;
	struct vnode *vp;

	error = namei_simple_user(SCARG(uap, path),
				NSM_NOFOLLOW_TRYEMULROOT, &vp);
	if (error != 0)
		return (error);

	error = change_owner(vp, SCARG(uap, uid), SCARG(uap, gid), l, 0);

	vrele(vp);
	return (error);
}

/*
 * Set ownership given a path name; this version does not follow links.
 * Provides POSIX/XPG semantics.
 */
/* ARGSUSED */
int
sys___posix_lchown(struct lwp *l, const struct sys___posix_lchown_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(uid_t) uid;
		syscallarg(gid_t) gid;
	} */
	int error;
	struct vnode *vp;

	error = namei_simple_user(SCARG(uap, path),
				NSM_NOFOLLOW_TRYEMULROOT, &vp);
	if (error != 0)
		return (error);

	error = change_owner(vp, SCARG(uap, uid), SCARG(uap, gid), l, 1);

	vrele(vp);
	return (error);
}

/*
 * Common routine to set ownership given a vnode.
 */
static int
change_owner(struct vnode *vp, uid_t uid, gid_t gid, struct lwp *l,
    int posix_semantics)
{
	struct vattr vattr;
	mode_t newmode;
	int error;

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	if ((error = VOP_GETATTR(vp, &vattr, l->l_cred)) != 0)
		goto out;

#define CHANGED(x) ((int)(x) != -1)
	newmode = vattr.va_mode;
	if (posix_semantics) {
		/*
		 * POSIX/XPG semantics: if the caller is not the super-user,
		 * clear set-user-id and set-group-id bits.  Both POSIX and
		 * the XPG consider the behaviour for calls by the super-user
		 * implementation-defined; we leave the set-user-id and set-
		 * group-id settings intact in that case.
		 */
		if (vattr.va_mode & S_ISUID) {
			if (kauth_authorize_vnode(l->l_cred,
			    KAUTH_VNODE_RETAIN_SUID, vp, NULL, EPERM) != 0)
				newmode &= ~S_ISUID;
		}
		if (vattr.va_mode & S_ISGID) {
			if (kauth_authorize_vnode(l->l_cred,
			    KAUTH_VNODE_RETAIN_SGID, vp, NULL, EPERM) != 0)
				newmode &= ~S_ISGID;
		}
	} else {
		/*
		 * NetBSD semantics: when changing owner and/or group,
		 * clear the respective bit(s).
		 */
		if (CHANGED(uid))
			newmode &= ~S_ISUID;
		if (CHANGED(gid))
			newmode &= ~S_ISGID;
	}
	/* Update va_mode iff altered. */
	if (vattr.va_mode == newmode)
		newmode = VNOVAL;

	vattr_null(&vattr);
	vattr.va_uid = CHANGED(uid) ? uid : (uid_t)VNOVAL;
	vattr.va_gid = CHANGED(gid) ? gid : (gid_t)VNOVAL;
	vattr.va_mode = newmode;
	error = VOP_SETATTR(vp, &vattr, l->l_cred);
#undef CHANGED

out:
	VOP_UNLOCK(vp);
	return (error);
}

/*
 * Set the access and modification times given a path name; this
 * version follows links.
 */
/* ARGSUSED */
int
sys___utimes50(struct lwp *l, const struct sys___utimes50_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(const struct timeval *) tptr;
	} */

	return do_sys_utimes(l, NULL, SCARG(uap, path), FOLLOW,
	    SCARG(uap, tptr), UIO_USERSPACE);
}

/*
 * Set the access and modification times given a file descriptor.
 */
/* ARGSUSED */
int
sys___futimes50(struct lwp *l, const struct sys___futimes50_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(const struct timeval *) tptr;
	} */
	int error;
	file_t *fp;

	/* fd_getvnode() will use the descriptor for us */
	if ((error = fd_getvnode(SCARG(uap, fd), &fp)) != 0)
		return (error);
	error = do_sys_utimes(l, fp->f_vnode, NULL, 0, SCARG(uap, tptr),
	    UIO_USERSPACE);
	fd_putfile(SCARG(uap, fd));
	return (error);
}

int
sys_futimens(struct lwp *l, const struct sys_futimens_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(const struct timespec *) tptr;
	} */
	int error;
	file_t *fp;

	/* fd_getvnode() will use the descriptor for us */
	if ((error = fd_getvnode(SCARG(uap, fd), &fp)) != 0)
		return (error);
	error = do_sys_utimensat(l, AT_FDCWD, fp->f_vnode, NULL, 0,
	    SCARG(uap, tptr), UIO_USERSPACE);
	fd_putfile(SCARG(uap, fd));
	return (error);
}

/*
 * Set the access and modification times given a path name; this
 * version does not follow links.
 */
int
sys___lutimes50(struct lwp *l, const struct sys___lutimes50_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(const struct timeval *) tptr;
	} */

	return do_sys_utimes(l, NULL, SCARG(uap, path), NOFOLLOW,
	    SCARG(uap, tptr), UIO_USERSPACE);
}

int
sys_utimensat(struct lwp *l, const struct sys_utimensat_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(const char *) path;
		syscallarg(const struct timespec *) tptr;
		syscallarg(int) flag;
	} */
	int follow;
	const struct timespec *tptr;
	int error;

	tptr = SCARG(uap, tptr);
	follow = (SCARG(uap, flag) & AT_SYMLINK_NOFOLLOW) ? NOFOLLOW : FOLLOW;

	error = do_sys_utimensat(l, SCARG(uap, fd), NULL, 
	    SCARG(uap, path), follow, tptr, UIO_USERSPACE);

	return error;
}

/*
 * Common routine to set access and modification times given a vnode.
 */
int
do_sys_utimens(struct lwp *l, struct vnode *vp, const char *path, int flag,
    const struct timespec *tptr, enum uio_seg seg)
{
	return do_sys_utimensat(l, AT_FDCWD, vp, path, flag, tptr, seg);
}

int
do_sys_utimensat(struct lwp *l, int fdat, struct vnode *vp,
    const char *path, int flag, const struct timespec *tptr, enum uio_seg seg)
{
	struct vattr vattr;
	int error, dorele = 0;
	namei_simple_flags_t sflags;
	bool vanull, setbirthtime;
	struct timespec ts[2];

	KASSERT(l != NULL || fdat == AT_FDCWD);

	/* 
	 * I have checked all callers and they pass either FOLLOW,
	 * NOFOLLOW, or 0 (when they don't pass a path), and NOFOLLOW
	 * is 0. More to the point, they don't pass anything else.
	 * Let's keep it that way at least until the namei interfaces
	 * are fully sanitized.
	 */
	KASSERT(flag == NOFOLLOW || flag == FOLLOW);
	sflags = (flag == FOLLOW) ? 
		NSM_FOLLOW_TRYEMULROOT : NSM_NOFOLLOW_TRYEMULROOT;

	if (tptr == NULL) {
		vanull = true;
		nanotime(&ts[0]);
		ts[1] = ts[0];
	} else {
		vanull = false;
		if (seg != UIO_SYSSPACE) {
			error = copyin(tptr, ts, sizeof (ts));
			if (error != 0)
				return error;
		} else {
			ts[0] = tptr[0];
			ts[1] = tptr[1];
		}
	}

	if (ts[0].tv_nsec == UTIME_NOW) {
		nanotime(&ts[0]);
		if (ts[1].tv_nsec == UTIME_NOW) {
			vanull = true;
			ts[1] = ts[0];
		}
	} else if (ts[1].tv_nsec == UTIME_NOW)
		nanotime(&ts[1]);

	if (vp == NULL) {
		/* note: SEG describes TPTR, not PATH; PATH is always user */
		error = fd_nameiat_simple_user(l, fdat, path, sflags, &vp);
		if (error != 0)
			return error;
		dorele = 1;
	}

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	setbirthtime = (VOP_GETATTR(vp, &vattr, l->l_cred) == 0 &&
	    timespeccmp(&ts[1], &vattr.va_birthtime, <));
	vattr_null(&vattr);

	if (ts[0].tv_nsec != UTIME_OMIT)
		vattr.va_atime = ts[0];

	if (ts[1].tv_nsec != UTIME_OMIT) {
		vattr.va_mtime = ts[1];
		if (setbirthtime)
			vattr.va_birthtime = ts[1];
	}

	if (vanull)
		vattr.va_vaflags |= VA_UTIMES_NULL;
	error = VOP_SETATTR(vp, &vattr, l->l_cred);
	VOP_UNLOCK(vp);

	if (dorele != 0)
		vrele(vp);

	return error;
}

int
do_sys_utimes(struct lwp *l, struct vnode *vp, const char *path, int flag,
    const struct timeval *tptr, enum uio_seg seg)
{
	struct timespec ts[2];
	struct timespec *tsptr = NULL;
	int error;
	
	if (tptr != NULL) {
		struct timeval tv[2];

		if (seg != UIO_SYSSPACE) {
			error = copyin(tptr, tv, sizeof (tv));
			if (error != 0)
				return error;
			tptr = tv;
		}

		if ((tv[0].tv_usec == UTIME_NOW) || 
		    (tv[0].tv_usec == UTIME_OMIT))
			ts[0].tv_nsec = tv[0].tv_usec;
		else
			TIMEVAL_TO_TIMESPEC(&tptr[0], &ts[0]);

		if ((tv[1].tv_usec == UTIME_NOW) || 
		    (tv[1].tv_usec == UTIME_OMIT))
			ts[1].tv_nsec = tv[1].tv_usec;
		else
			TIMEVAL_TO_TIMESPEC(&tptr[1], &ts[1]);

		tsptr = &ts[0];	
	}

	return do_sys_utimens(l, vp, path, flag, tsptr, UIO_SYSSPACE);
}

/*
 * Truncate a file given its path name.
 */
/* ARGSUSED */
int
sys_truncate(struct lwp *l, const struct sys_truncate_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(int) pad;
		syscallarg(off_t) length;
	} */
	struct vnode *vp;
	struct vattr vattr;
	int error;

	if (SCARG(uap, length) < 0)
		return EINVAL;

	error = namei_simple_user(SCARG(uap, path),
				NSM_FOLLOW_TRYEMULROOT, &vp);
	if (error != 0)
		return (error);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	if (vp->v_type == VDIR)
		error = EISDIR;
	else if ((error = vn_writechk(vp)) == 0 &&
	    (error = VOP_ACCESS(vp, VWRITE, l->l_cred)) == 0) {
		vattr_null(&vattr);
		vattr.va_size = SCARG(uap, length);
		error = VOP_SETATTR(vp, &vattr, l->l_cred);
	}
	vput(vp);
	return (error);
}

/*
 * Truncate a file given a file descriptor.
 */
/* ARGSUSED */
int
sys_ftruncate(struct lwp *l, const struct sys_ftruncate_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(int) pad;
		syscallarg(off_t) length;
	} */
	struct vattr vattr;
	struct vnode *vp;
	file_t *fp;
	int error;

	if (SCARG(uap, length) < 0)
		return EINVAL;

	/* fd_getvnode() will use the descriptor for us */
	if ((error = fd_getvnode(SCARG(uap, fd), &fp)) != 0)
		return (error);
	if ((fp->f_flag & FWRITE) == 0) {
		error = EINVAL;
		goto out;
	}
	vp = fp->f_vnode;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	if (vp->v_type == VDIR)
		error = EISDIR;
	else if ((error = vn_writechk(vp)) == 0) {
		vattr_null(&vattr);
		vattr.va_size = SCARG(uap, length);
		error = VOP_SETATTR(vp, &vattr, fp->f_cred);
	}
	VOP_UNLOCK(vp);
 out:
	fd_putfile(SCARG(uap, fd));
	return (error);
}

/*
 * Sync an open file.
 */
/* ARGSUSED */
int
sys_fsync(struct lwp *l, const struct sys_fsync_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fd;
	} */
	struct vnode *vp;
	file_t *fp;
	int error;

	/* fd_getvnode() will use the descriptor for us */
	if ((error = fd_getvnode(SCARG(uap, fd), &fp)) != 0)
		return (error);
	vp = fp->f_vnode;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	error = VOP_FSYNC(vp, fp->f_cred, FSYNC_WAIT, 0, 0);
	VOP_UNLOCK(vp);
	fd_putfile(SCARG(uap, fd));
	return (error);
}

/*
 * Sync a range of file data.  API modeled after that found in AIX.
 *
 * FDATASYNC indicates that we need only save enough metadata to be able
 * to re-read the written data.  Note we duplicate AIX's requirement that
 * the file be open for writing.
 */
/* ARGSUSED */
int
sys_fsync_range(struct lwp *l, const struct sys_fsync_range_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(int) flags;
		syscallarg(off_t) start;
		syscallarg(off_t) length;
	} */
	struct vnode *vp;
	file_t *fp;
	int flags, nflags;
	off_t s, e, len;
	int error;

	/* fd_getvnode() will use the descriptor for us */
	if ((error = fd_getvnode(SCARG(uap, fd), &fp)) != 0)
		return (error);

	if ((fp->f_flag & FWRITE) == 0) {
		error = EBADF;
		goto out;
	}

	flags = SCARG(uap, flags);
	if (((flags & (FDATASYNC | FFILESYNC)) == 0) ||
	    ((~flags & (FDATASYNC | FFILESYNC)) == 0)) {
		error = EINVAL;
		goto out;
	}
	/* Now set up the flags for value(s) to pass to VOP_FSYNC() */
	if (flags & FDATASYNC)
		nflags = FSYNC_DATAONLY | FSYNC_WAIT;
	else
		nflags = FSYNC_WAIT;
	if (flags & FDISKSYNC)
		nflags |= FSYNC_CACHE;

	len = SCARG(uap, length);
	/* If length == 0, we do the whole file, and s = e = 0 will do that */
	if (len) {
		s = SCARG(uap, start);
		e = s + len;
		if (e < s) {
			error = EINVAL;
			goto out;
		}
	} else {
		e = 0;
		s = 0;
	}

	vp = fp->f_vnode;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	error = VOP_FSYNC(vp, fp->f_cred, nflags, s, e);
	VOP_UNLOCK(vp);
out:
	fd_putfile(SCARG(uap, fd));
	return (error);
}

/*
 * Sync the data of an open file.
 */
/* ARGSUSED */
int
sys_fdatasync(struct lwp *l, const struct sys_fdatasync_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fd;
	} */
	struct vnode *vp;
	file_t *fp;
	int error;

	/* fd_getvnode() will use the descriptor for us */
	if ((error = fd_getvnode(SCARG(uap, fd), &fp)) != 0)
		return (error);
	if ((fp->f_flag & FWRITE) == 0) {
		fd_putfile(SCARG(uap, fd));
		return (EBADF);
	}
	vp = fp->f_vnode;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	error = VOP_FSYNC(vp, fp->f_cred, FSYNC_WAIT|FSYNC_DATAONLY, 0, 0);
	VOP_UNLOCK(vp);
	fd_putfile(SCARG(uap, fd));
	return (error);
}

/*
 * Rename files, (standard) BSD semantics frontend.
 */
/* ARGSUSED */
int
sys_rename(struct lwp *l, const struct sys_rename_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) from;
		syscallarg(const char *) to;
	} */

	return (do_sys_renameat(l, AT_FDCWD, SCARG(uap, from), AT_FDCWD, 
	    SCARG(uap, to), UIO_USERSPACE, 0));
}

int
sys_renameat(struct lwp *l, const struct sys_renameat_args *uap, 
    register_t *retval)
{
	/* {
		syscallarg(int) fromfd;
		syscallarg(const char *) from;
		syscallarg(int) tofd;
		syscallarg(const char *) to;
	} */

	return (do_sys_renameat(l, SCARG(uap, fromfd), SCARG(uap, from),
	    SCARG(uap, tofd), SCARG(uap, to), UIO_USERSPACE, 0));
}

/*
 * Rename files, POSIX semantics frontend.
 */
/* ARGSUSED */
int
sys___posix_rename(struct lwp *l, const struct sys___posix_rename_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) from;
		syscallarg(const char *) to;
	} */

	return (do_sys_renameat(l, AT_FDCWD, SCARG(uap, from), AT_FDCWD,
	    SCARG(uap, to), UIO_USERSPACE, 1));
}

/*
 * Rename files.  Source and destination must either both be directories,
 * or both not be directories.  If target is a directory, it must be empty.
 * If `from' and `to' refer to the same object, the value of the `retain'
 * argument is used to determine whether `from' will be
 *
 * (retain == 0)	deleted unless `from' and `to' refer to the same
 *			object in the file system's name space (BSD).
 * (retain == 1)	always retained (POSIX).
 *
 * XXX Synchronize with nfsrv_rename in nfs_serv.c.
 */
int
do_sys_rename(const char *from, const char *to, enum uio_seg seg, int retain)
{
	return do_sys_renameat(NULL, AT_FDCWD, from, AT_FDCWD, to, seg, retain);
}

static int
do_sys_renameat(struct lwp *l, int fromfd, const char *from, int tofd,
    const char *to, enum uio_seg seg, int retain)
{
	struct pathbuf *fpb, *tpb;
	struct nameidata fnd, tnd;
	struct vnode *fdvp, *fvp;
	struct vnode *tdvp, *tvp;
	struct mount *mp, *tmp;
	int error;

	KASSERT(l != NULL || (fromfd == AT_FDCWD && tofd == AT_FDCWD));

	error = pathbuf_maybe_copyin(from, seg, &fpb);
	if (error)
		goto out0;
	KASSERT(fpb != NULL);

	error = pathbuf_maybe_copyin(to, seg, &tpb);
	if (error)
		goto out1;
	KASSERT(tpb != NULL);

	/*
	 * Lookup from.
	 *
	 * XXX LOCKPARENT is wrong because we don't actually want it
	 * locked yet, but (a) namei is insane, and (b) VOP_RENAME is
	 * insane, so for the time being we need to leave it like this.
	 */
	NDINIT(&fnd, DELETE, (LOCKPARENT | TRYEMULROOT), fpb);
	if ((error = fd_nameiat(l, fromfd, &fnd)) != 0)
		goto out2;

	/*
	 * Pull out the important results of the lookup, fdvp and fvp.
	 * Of course, fvp is bogus because we're about to unlock fdvp.
	 */
	fdvp = fnd.ni_dvp;
	fvp = fnd.ni_vp;
	KASSERT(fdvp != NULL);
	KASSERT(fvp != NULL);
	KASSERT((fdvp == fvp) || (VOP_ISLOCKED(fdvp) == LK_EXCLUSIVE));

	/*
	 * Make sure neither fdvp nor fvp is locked.
	 */
	if (fdvp != fvp)
		VOP_UNLOCK(fdvp);
	/* XXX KASSERT(VOP_ISLOCKED(fdvp) != LK_EXCLUSIVE); */
	/* XXX KASSERT(VOP_ISLOCKED(fvp) != LK_EXCLUSIVE); */

	/*
	 * Reject renaming `.' and `..'.  Can't do this until after
	 * namei because we need namei's parsing to find the final
	 * component name.  (namei should just leave us with the final
	 * component name and not look it up itself, but anyway...)
	 *
	 * This was here before because we used to relookup from
	 * instead of to and relookup requires the caller to check
	 * this, but now file systems may depend on this check, so we
	 * must retain it until the file systems are all rototilled.
	 */
	if (((fnd.ni_cnd.cn_namelen == 1) &&
		(fnd.ni_cnd.cn_nameptr[0] == '.')) ||
	    ((fnd.ni_cnd.cn_namelen == 2) &&
		(fnd.ni_cnd.cn_nameptr[0] == '.') &&
		(fnd.ni_cnd.cn_nameptr[1] == '.'))) {
		error = EINVAL;	/* XXX EISDIR?  */
		goto abort0;
	}

	/*
	 * Lookup to.
	 *
	 * XXX LOCKPARENT is wrong, but...insanity, &c.  Also, using
	 * fvp here to decide whether to add CREATEDIR is a load of
	 * bollocks because fvp might be the wrong node by now, since
	 * fdvp is unlocked.
	 *
	 * XXX Why not pass CREATEDIR always?
	 */
	NDINIT(&tnd, RENAME,
	    (LOCKPARENT | NOCACHE | TRYEMULROOT |
		((fvp->v_type == VDIR)? CREATEDIR : 0)),
	    tpb);
	if ((error = fd_nameiat(l, tofd, &tnd)) != 0)
		goto abort0;

	/*
	 * Pull out the important results of the lookup, tdvp and tvp.
	 * Of course, tvp is bogus because we're about to unlock tdvp.
	 */
	tdvp = tnd.ni_dvp;
	tvp = tnd.ni_vp;
	KASSERT(tdvp != NULL);
	KASSERT((tdvp == tvp) || (VOP_ISLOCKED(tdvp) == LK_EXCLUSIVE));

	/*
	 * Make sure neither tdvp nor tvp is locked.
	 */
	if (tdvp != tvp)
		VOP_UNLOCK(tdvp);
	/* XXX KASSERT(VOP_ISLOCKED(tdvp) != LK_EXCLUSIVE); */
	/* XXX KASSERT((tvp == NULL) || (VOP_ISLOCKED(tvp) != LK_EXCLUSIVE)); */

	/*
	 * Reject renaming onto `.' or `..'.  relookup is unhappy with
	 * these, which is why we must do this here.  Once upon a time
	 * we relooked up from instead of to, and consequently didn't
	 * need this check, but now that we relookup to instead of
	 * from, we need this; and we shall need it forever forward
	 * until the VOP_RENAME protocol changes, because file systems
	 * will no doubt begin to depend on this check.
	 */
	if ((tnd.ni_cnd.cn_namelen == 1) && (tnd.ni_cnd.cn_nameptr[0] == '.')) {
		error = EISDIR;
		goto abort1;
	}
	if ((tnd.ni_cnd.cn_namelen == 2) &&
	    (tnd.ni_cnd.cn_nameptr[0] == '.') &&
	    (tnd.ni_cnd.cn_nameptr[1] == '.')) {
		error = EINVAL;
		goto abort1;
	}

	/*
	 * Get the mount point.  If the file system has been unmounted,
	 * which it may be because we're not holding any vnode locks,
	 * then v_mount will be NULL.  We're not really supposed to
	 * read v_mount without holding the vnode lock, but since we
	 * have fdvp referenced, if fdvp->v_mount changes then at worst
	 * it will be set to NULL, not changed to another mount point.
	 * And, of course, since it is up to the file system to
	 * determine the real lock order, we can't lock both fdvp and
	 * tdvp at the same time.
	 */
	mp = fdvp->v_mount;
	if (mp == NULL) {
		error = ENOENT;
		goto abort1;
	}

	/*
	 * Make sure the mount points match.  Again, although we don't
	 * hold any vnode locks, the v_mount fields may change -- but
	 * at worst they will change to NULL, so this will never become
	 * a cross-device rename, because we hold vnode references.
	 *
	 * XXX Because nothing is locked and the compiler may reorder
	 * things here, unmounting the file system at an inopportune
	 * moment may cause rename to fail with ENXDEV when it really
	 * should fail with ENOENT.
	 */
	tmp = tdvp->v_mount;
	if (tmp == NULL) {
		error = ENOENT;
		goto abort1;
	}

	if (mp != tmp) {
		error = EXDEV;
		goto abort1;
	}

	/*
	 * Take the vfs rename lock to avoid cross-directory screw cases.
	 * Nothing is locked currently, so taking this lock is safe.
	 */
	error = VFS_RENAMELOCK_ENTER(mp);
	if (error)
		goto abort1;

	/*
	 * Now fdvp, fvp, tdvp, and (if nonnull) tvp are referenced,
	 * and nothing is locked except for the vfs rename lock.
	 *
	 * The next step is a little rain dance to conform to the
	 * insane lock protocol, even though it does nothing to ward
	 * off race conditions.
	 *
	 * We need tdvp and tvp to be locked.  However, because we have
	 * unlocked tdvp in order to hold no locks while we take the
	 * vfs rename lock, tvp may be wrong here, and we can't safely
	 * lock it even if the sensible file systems will just unlock
	 * it straight away.  Consequently, we must lock tdvp and then
	 * relookup tvp to get it locked.
	 *
	 * Finally, because the VOP_RENAME protocol is brain-damaged
	 * and various file systems insanely depend on the semantics of
	 * this brain damage, the lookup of to must be the last lookup
	 * before VOP_RENAME.
	 */
	vn_lock(tdvp, LK_EXCLUSIVE | LK_RETRY);
	error = relookup(tdvp, &tnd.ni_vp, &tnd.ni_cnd, 0);
	if (error)
		goto abort2;

	/*
	 * Drop the old tvp and pick up the new one -- which might be
	 * the same, but that doesn't matter to us.  After this, tdvp
	 * and tvp should both be locked.
	 */
	if (tvp != NULL)
		vrele(tvp);
	tvp = tnd.ni_vp;
	KASSERT(VOP_ISLOCKED(tdvp) == LK_EXCLUSIVE);
	KASSERT((tvp == NULL) || (VOP_ISLOCKED(tvp) == LK_EXCLUSIVE));

	/*
	 * The old do_sys_rename had various consistency checks here
	 * involving fvp and tvp.  fvp is bogus already here, and tvp
	 * will become bogus soon in any sensible file system, so the
	 * only purpose in putting these checks here is to give lip
	 * service to these screw cases and to acknowledge that they
	 * exist, not actually to handle them, but here you go
	 * anyway...
	 */

	/*
	 * Acknowledge that directories and non-directories aren't
	 * suposed to mix.
	 */
	if (tvp != NULL) {
		if ((fvp->v_type == VDIR) && (tvp->v_type != VDIR)) {
			error = ENOTDIR;
			goto abort3;
		} else if ((fvp->v_type != VDIR) && (tvp->v_type == VDIR)) {
			error = EISDIR;
			goto abort3;
		}
	}

	/*
	 * Acknowledge some random screw case, among the dozens that
	 * might arise.
	 */
	if (fvp == tdvp) {
		error = EINVAL;
		goto abort3;
	}

	/*
	 * Acknowledge that POSIX has a wacky screw case.
	 *
	 * XXX Eventually the retain flag needs to be passed on to
	 * VOP_RENAME.
	 */
	if (fvp == tvp) {
		if (retain) {
			error = 0;
			goto abort3;
		} else if ((fdvp == tdvp) &&
		    (fnd.ni_cnd.cn_namelen == tnd.ni_cnd.cn_namelen) &&
		    (0 == memcmp(fnd.ni_cnd.cn_nameptr, tnd.ni_cnd.cn_nameptr,
			fnd.ni_cnd.cn_namelen))) {
			error = 0;
			goto abort3;
		}
	}

	/*
	 * Make sure veriexec can screw us up.  (But a race can screw
	 * up veriexec, of course -- remember, fvp and (soon) tvp are
	 * bogus.)
	 */
#if NVERIEXEC > 0
	{
		char *f1, *f2;
		size_t f1_len;
		size_t f2_len;

		f1_len = fnd.ni_cnd.cn_namelen + 1;
		f1 = kmem_alloc(f1_len, KM_SLEEP);
		strlcpy(f1, fnd.ni_cnd.cn_nameptr, f1_len);

		f2_len = tnd.ni_cnd.cn_namelen + 1;
		f2 = kmem_alloc(f2_len, KM_SLEEP);
		strlcpy(f2, tnd.ni_cnd.cn_nameptr, f2_len);

		error = veriexec_renamechk(curlwp, fvp, f1, tvp, f2);

		kmem_free(f1, f1_len);
		kmem_free(f2, f2_len);

		if (error)
			goto abort3;
	}
#endif /* NVERIEXEC > 0 */

	/*
	 * All ready.  Incant the rename vop.
	 */
	/* XXX KASSERT(VOP_ISLOCKED(fdvp) != LK_EXCLUSIVE); */
	/* XXX KASSERT(VOP_ISLOCKED(fvp) != LK_EXCLUSIVE); */
	KASSERT(VOP_ISLOCKED(tdvp) == LK_EXCLUSIVE);
	KASSERT((tvp == NULL) || (VOP_ISLOCKED(tvp) == LK_EXCLUSIVE));
	error = VOP_RENAME(fdvp, fvp, &fnd.ni_cnd, tdvp, tvp, &tnd.ni_cnd);

	/*
	 * VOP_RENAME releases fdvp, fvp, tdvp, and tvp, and unlocks
	 * tdvp and tvp.  But we can't assert any of that.
	 */
	/* XXX KASSERT(VOP_ISLOCKED(fdvp) != LK_EXCLUSIVE); */
	/* XXX KASSERT(VOP_ISLOCKED(fvp) != LK_EXCLUSIVE); */
	/* XXX KASSERT(VOP_ISLOCKED(tdvp) != LK_EXCLUSIVE); */
	/* XXX KASSERT((tvp == NULL) || (VOP_ISLOCKED(tvp) != LK_EXCLUSIVE)); */

	/*
	 * So all we have left to do is to drop the rename lock and
	 * destroy the pathbufs.
	 */
	VFS_RENAMELOCK_EXIT(mp);
	goto out2;

abort3:	if ((tvp != NULL) && (tvp != tdvp))
		VOP_UNLOCK(tvp);
abort2:	VOP_UNLOCK(tdvp);
	VFS_RENAMELOCK_EXIT(mp);
abort1:	VOP_ABORTOP(tdvp, &tnd.ni_cnd);
	vrele(tdvp);
	if (tvp != NULL)
		vrele(tvp);
abort0:	VOP_ABORTOP(fdvp, &fnd.ni_cnd);
	vrele(fdvp);
	vrele(fvp);
out2:	pathbuf_destroy(tpb);
out1:	pathbuf_destroy(fpb);
out0:	return error;
}

/*
 * Make a directory file.
 */
/* ARGSUSED */
int
sys_mkdir(struct lwp *l, const struct sys_mkdir_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(int) mode;
	} */

	return do_sys_mkdirat(l, AT_FDCWD, SCARG(uap, path),
	    SCARG(uap, mode), UIO_USERSPACE);
}

int
sys_mkdirat(struct lwp *l, const struct sys_mkdirat_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(const char *) path;
		syscallarg(int) mode;
	} */

	return do_sys_mkdirat(l, SCARG(uap, fd), SCARG(uap, path),
	    SCARG(uap, mode), UIO_USERSPACE);
}


int
do_sys_mkdir(const char *path, mode_t mode, enum uio_seg seg)
{
	return do_sys_mkdirat(NULL, AT_FDCWD, path, mode, UIO_USERSPACE);
}

static int
do_sys_mkdirat(struct lwp *l, int fdat, const char *path, mode_t mode,
    enum uio_seg seg)
{
	struct proc *p = curlwp->l_proc;
	struct vnode *vp;
	struct vattr vattr;
	int error;
	struct pathbuf *pb;
	struct nameidata nd;

	KASSERT(l != NULL || fdat == AT_FDCWD);

	/* XXX bollocks, should pass in a pathbuf */
	error = pathbuf_maybe_copyin(path, seg, &pb);
	if (error) {
		return error;
	}

	NDINIT(&nd, CREATE, LOCKPARENT | CREATEDIR | TRYEMULROOT, pb);

	if ((error = fd_nameiat(l, fdat, &nd)) != 0) {
		pathbuf_destroy(pb);
		return (error);
	}
	vp = nd.ni_vp;
	if (vp != NULL) {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vrele(vp);
		pathbuf_destroy(pb);
		return (EEXIST);
	}
	vattr_null(&vattr);
	vattr.va_type = VDIR;
	/* We will read cwdi->cwdi_cmask unlocked. */
	vattr.va_mode = (mode & ACCESSPERMS) &~ p->p_cwdi->cwdi_cmask;
	error = VOP_MKDIR(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &vattr);
	if (!error)
		vrele(nd.ni_vp);
	vput(nd.ni_dvp);
	pathbuf_destroy(pb);
	return (error);
}

/*
 * Remove a directory file.
 */
/* ARGSUSED */
int
sys_rmdir(struct lwp *l, const struct sys_rmdir_args *uap, register_t *retval)
{
	return do_sys_unlinkat(l, AT_FDCWD, SCARG(uap, path),
	    AT_REMOVEDIR, UIO_USERSPACE);
}

/*
 * Read a block of directory entries in a file system independent format.
 */
int
sys___getdents30(struct lwp *l, const struct sys___getdents30_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(char *) buf;
		syscallarg(size_t) count;
	} */
	file_t *fp;
	int error, done;

	/* fd_getvnode() will use the descriptor for us */
	if ((error = fd_getvnode(SCARG(uap, fd), &fp)) != 0)
		return (error);
	if ((fp->f_flag & FREAD) == 0) {
		error = EBADF;
		goto out;
	}
	error = vn_readdir(fp, SCARG(uap, buf), UIO_USERSPACE,
			SCARG(uap, count), &done, l, 0, 0);
	ktrgenio(SCARG(uap, fd), UIO_READ, SCARG(uap, buf), done, error);
	*retval = done;
 out:
	fd_putfile(SCARG(uap, fd));
	return (error);
}

/*
 * Set the mode mask for creation of filesystem nodes.
 */
int
sys_umask(struct lwp *l, const struct sys_umask_args *uap, register_t *retval)
{
	/* {
		syscallarg(mode_t) newmask;
	} */
	struct proc *p = l->l_proc;
	struct cwdinfo *cwdi;

	/*
	 * cwdi->cwdi_cmask will be read unlocked elsewhere.  What's
	 * important is that we serialize changes to the mask.  The
	 * rw_exit() will issue a write memory barrier on our behalf,
	 * and force the changes out to other CPUs (as it must use an
	 * atomic operation, draining the local CPU's store buffers).
	 */
	cwdi = p->p_cwdi;
	rw_enter(&cwdi->cwdi_lock, RW_WRITER);
	*retval = cwdi->cwdi_cmask;
	cwdi->cwdi_cmask = SCARG(uap, newmask) & ALLPERMS;
	rw_exit(&cwdi->cwdi_lock);

	return (0);
}

int
dorevoke(struct vnode *vp, kauth_cred_t cred)
{
	struct vattr vattr;
	int error, fs_decision;

	vn_lock(vp, LK_SHARED | LK_RETRY);
	error = VOP_GETATTR(vp, &vattr, cred);
	VOP_UNLOCK(vp);
	if (error != 0)
		return error;
	fs_decision = (kauth_cred_geteuid(cred) == vattr.va_uid) ? 0 : EPERM;
	error = kauth_authorize_vnode(cred, KAUTH_VNODE_REVOKE, vp, NULL,
	    fs_decision);
	if (!error)
		VOP_REVOKE(vp, REVOKEALL);
	return (error);
}

/*
 * Void all references to file by ripping underlying filesystem
 * away from vnode.
 */
/* ARGSUSED */
int
sys_revoke(struct lwp *l, const struct sys_revoke_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
	} */
	struct vnode *vp;
	int error;

	error = namei_simple_user(SCARG(uap, path),
				NSM_FOLLOW_TRYEMULROOT, &vp);
	if (error != 0)
		return (error);
	error = dorevoke(vp, l->l_cred);
	vrele(vp);
	return (error);
}

/*
 * Allocate backing store for a file, filling a hole without having to
 * explicitly write anything out.
 */
/* ARGSUSED */
int
sys_posix_fallocate(struct lwp *l, const struct sys_posix_fallocate_args *uap,
		register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(off_t) pos;
		syscallarg(off_t) len;
	} */
	int fd;
	off_t pos, len;
	struct file *fp;
	struct vnode *vp;
	int error;

	fd = SCARG(uap, fd);
	pos = SCARG(uap, pos);
	len = SCARG(uap, len);
	
	if (pos < 0 || len < 0 || len > OFF_T_MAX - pos) {
		*retval = EINVAL;
		return 0;
	}
	
	error = fd_getvnode(fd, &fp);
	if (error) {
		*retval = error;
		return 0;
	}
	if ((fp->f_flag & FWRITE) == 0) {
		error = EBADF;
		goto fail;
	}
	vp = fp->f_vnode;

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	if (vp->v_type == VDIR) {
		error = EISDIR;
	} else {
		error = VOP_FALLOCATE(vp, pos, len);
	}
	VOP_UNLOCK(vp);

fail:
	fd_putfile(fd);
	*retval = error;
	return 0;
}

/*
 * Deallocate backing store for a file, creating a hole. Also used for
 * invoking TRIM on disks.
 */
/* ARGSUSED */
int
sys_fdiscard(struct lwp *l, const struct sys_fdiscard_args *uap,
		register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(off_t) pos;
		syscallarg(off_t) len;
	} */
	int fd;
	off_t pos, len;
	struct file *fp;
	struct vnode *vp;
	int error;

	fd = SCARG(uap, fd);
	pos = SCARG(uap, pos);
	len = SCARG(uap, len);

	if (pos < 0 || len < 0 || len > OFF_T_MAX - pos) {
		return EINVAL;
	}
	
	error = fd_getvnode(fd, &fp);
	if (error) {
		return error;
	}
	if ((fp->f_flag & FWRITE) == 0) {
		error = EBADF;
		goto fail;
	}
	vp = fp->f_vnode;

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	if (vp->v_type == VDIR) {
		error = EISDIR;
	} else {
		error = VOP_FDISCARD(vp, pos, len);
	}
	VOP_UNLOCK(vp);

fail:
	fd_putfile(fd);
	return error;
}
