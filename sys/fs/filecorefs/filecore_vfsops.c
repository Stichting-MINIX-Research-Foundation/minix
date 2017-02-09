/*	$NetBSD: filecore_vfsops.c,v 1.78 2015/03/28 19:24:05 maxv Exp $	*/

/*-
 * Copyright (c) 1994 The Regents of the University of California.
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
 *	filecore_vfsops.c	1.1	1998/6/26
 */

/*-
 * Copyright (c) 1998 Andrew McMurry
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	filecore_vfsops.c	1.1	1998/6/26
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: filecore_vfsops.c,v 1.78 2015/03/28 19:24:05 maxv Exp $");

#if defined(_KERNEL_OPT)
#include "opt_compat_netbsd.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <miscfs/genfs/genfs.h>
#include <miscfs/specfs/specdev.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/file.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/pool.h>
#include <sys/conf.h>
#include <sys/sysctl.h>
#include <sys/kauth.h>
#include <sys/module.h>

#include <fs/filecorefs/filecore.h>
#include <fs/filecorefs/filecore_extern.h>
#include <fs/filecorefs/filecore_node.h>
#include <fs/filecorefs/filecore_mount.h>

MODULE(MODULE_CLASS_VFS, filecore, NULL);

static struct sysctllog *filecore_sysctl_log;

extern const struct vnodeopv_desc filecore_vnodeop_opv_desc;

const struct vnodeopv_desc * const filecore_vnodeopv_descs[] = {
	&filecore_vnodeop_opv_desc,
	NULL,
};

struct vfsops filecore_vfsops = {
	.vfs_name = MOUNT_FILECORE,
	.vfs_min_mount_data = sizeof (struct filecore_args),
	.vfs_mount = filecore_mount,
	.vfs_start = filecore_start,
	.vfs_unmount = filecore_unmount,
	.vfs_root = filecore_root,
	.vfs_quotactl = (void *)eopnotsupp,
	.vfs_statvfs = filecore_statvfs,
	.vfs_sync = filecore_sync,
	.vfs_vget = filecore_vget,
	.vfs_loadvnode = filecore_loadvnode,
	.vfs_fhtovp = filecore_fhtovp,
	.vfs_vptofh = filecore_vptofh,
	.vfs_init = filecore_init,
	.vfs_reinit = filecore_reinit,
	.vfs_done = filecore_done,
	.vfs_snapshot = (void *)eopnotsupp,
	.vfs_extattrctl = vfs_stdextattrctl,
	.vfs_suspendctl = (void *)eopnotsupp,
	.vfs_renamelock_enter = genfs_renamelock_enter,
	.vfs_renamelock_exit = genfs_renamelock_exit,
	.vfs_fsync = (void *)eopnotsupp,
	.vfs_opv_descs = filecore_vnodeopv_descs
};

static int
filecore_modcmd(modcmd_t cmd, void *arg)
{
	int error;

	switch (cmd) {
	case MODULE_CMD_INIT:
		error = vfs_attach(&filecore_vfsops);
		if (error != 0)
			break;
		sysctl_createv(&filecore_sysctl_log, 0, NULL, NULL,
			       CTLFLAG_PERMANENT,
			       CTLTYPE_NODE, "filecore",
			       SYSCTL_DESCR("Acorn FILECORE file system"),
			       NULL, 0, NULL, 0,
			       CTL_VFS, 19, CTL_EOL);
		/*
		 * XXX the "19" above could be dynamic, thereby eliminating
		 * one more instance of the "number to vfs" mapping problem,
		 * but "19" is the order as taken from sys/mount.h
		 */
		break;
	case MODULE_CMD_FINI:
		error = vfs_detach(&filecore_vfsops);
		if (error != 0)
			break;
		sysctl_teardown(&filecore_sysctl_log);	
		break;
	default:
		error = ENOTTY;
		break;
	}

	return (error);
}

/*
 * Called by vfs_mountroot when iso is going to be mounted as root.
 *
 * Name is updated by mount(8) after booting.
 */

static int filecore_mountfs(struct vnode *devvp, struct mount *mp,
		struct lwp *l, struct filecore_args *argp);

#if 0
int
filecore_mountroot(void)
{
	struct mount *mp;
	extern struct vnode *rootvp;
	struct proc *p = curproc;	/* XXX */
	int error;
	struct filecore_args args;

	if (device_class(root_device) != DV_DISK)
		return (ENODEV);

	/*
	 * Get vnodes for swapdev and rootdev.
	 */
	if (bdevvp(rootdev, &rootvp))
		panic("filecore_mountroot: can't setup rootvp");

	if ((error = vfs_rootmountalloc(MOUNT_FILECORE, "root_device", &mp)) != 0)
		return (error);

	args.flags = FILECOREMNT_ROOT;
	if ((error = filecore_mountfs(rootvp, mp, p, &args)) != 0) {
		vfs_unbusy(mp, false, NULL);
		vfs_destroy(mp);
		return (error);
	}
	mountlist_append(mp);
	(void)filecore_statvfs(mp, &mp->mnt_stat, p);
	vfs_unbusy(mp, false, NULL);
	return (0);
}
#endif

/*
 * VFS Operations.
 *
 * mount system call
 */
int
filecore_mount(struct mount *mp, const char *path, void *data, size_t *data_len)
{
	struct lwp *l = curlwp;
	struct vnode *devvp;
	struct filecore_args *args = data;
	int error;
	struct filecore_mnt *fcmp = NULL;

	if (args == NULL)
		return EINVAL;
	if (*data_len < sizeof *args)
		return EINVAL;

	if (mp->mnt_flag & MNT_GETARGS) {
		fcmp = VFSTOFILECORE(mp);
		if (fcmp == NULL)
			return EIO;
		args->flags = fcmp->fc_mntflags;
		args->uid = fcmp->fc_uid;
		args->gid = fcmp->fc_gid;
		args->fspec = NULL;
		*data_len = sizeof *args;
		return 0;
	}

	if ((mp->mnt_flag & MNT_RDONLY) == 0)
		return (EROFS);

	if ((mp->mnt_flag & MNT_UPDATE) && args->fspec == NULL)
		return EINVAL;

	/*
	 * Not an update, or updating the name: look up the name
	 * and verify that it refers to a sensible block device.
	 */
	error = namei_simple_user(args->fspec,
				NSM_FOLLOW_NOEMULROOT, &devvp);
	if (error != 0)
		return (error);

	if (devvp->v_type != VBLK) {
		vrele(devvp);
		return ENOTBLK;
	}
	if (bdevsw_lookup(devvp->v_rdev) == NULL) {
		vrele(devvp);
		return ENXIO;
	}
	/*
	 * If mount by non-root, then verify that user has necessary
	 * permissions on the device.
	 */
	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
	error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_MOUNT,
	    KAUTH_REQ_SYSTEM_MOUNT_DEVICE, mp, devvp, KAUTH_ARG(VREAD));
	VOP_UNLOCK(devvp);
	if (error) {
		vrele(devvp);
		return (error);
	}
	if ((mp->mnt_flag & MNT_UPDATE) == 0)
		error = filecore_mountfs(devvp, mp, l, args);
	else {
		fcmp = VFSTOFILECORE(mp);
		if (devvp != fcmp->fc_devvp)
			error = EINVAL;	/* needs translation */
		else
			vrele(devvp);
	}
	if (error) {
		vrele(devvp);
		return error;
	}
	fcmp = VFSTOFILECORE(mp);
	return set_statvfs_info(path, UIO_USERSPACE, args->fspec, UIO_USERSPACE,
	    mp->mnt_op->vfs_name, mp, l);
}

/*
 * Common code for mount and mountroot
 */
static int
filecore_mountfs(struct vnode *devvp, struct mount *mp, struct lwp *l, struct filecore_args *argp)
{
	struct filecore_mnt *fcmp = (struct filecore_mnt *)0;
	struct buf *bp = NULL;
	dev_t dev = devvp->v_rdev;
	int error = EINVAL;
	int ronly = (mp->mnt_flag & MNT_RDONLY) != 0;
	struct filecore_disc_record *fcdr;
	unsigned map;
	unsigned log2secsize;

	if (!ronly)
		return EROFS;

	if ((error = vinvalbuf(devvp, V_SAVE, l->l_cred, l, 0, 0)) != 0)
		return (error);

	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
	error = VOP_OPEN(devvp, ronly ? FREAD : FREAD|FWRITE, FSCRED);
	VOP_UNLOCK(devvp);
	if (error)
		return error;

	/* Read the filecore boot block to check FS validity and to find the map */
	error = bread(devvp, FILECORE_BOOTBLOCK_BLKN,
			   FILECORE_BOOTBLOCK_SIZE, 0, &bp);
#ifdef FILECORE_DEBUG_BR
		printf("bread(%p, %x, %d, CRED, %p)=%d\n", devvp,
		       FILECORE_BOOTBLOCK_BLKN, FILECORE_BOOTBLOCK_SIZE,
		       bp, error);
#endif
	if (error != 0)
		goto out;

	KASSERT(bp != NULL);
	if (filecore_bbchecksum(bp->b_data) != 0) {
		error = EINVAL;
		goto out;
	}
	fcdr = (struct filecore_disc_record *)((char *)(bp->b_data) +
	    FILECORE_BB_DISCREC);
	map = ((((8 << fcdr->log2secsize) - fcdr->zone_spare)
	    * (fcdr->nzones / 2) - 8 * FILECORE_DISCREC_SIZE)
	    << fcdr->log2bpmb) >> fcdr->log2secsize;
	log2secsize = fcdr->log2secsize;
#ifdef FILECORE_DEBUG_BR
	printf("brelse(%p) vf1\n", bp);
#endif
	brelse(bp, BC_AGE);
	bp = NULL;

	/* Read the bootblock in the map */
	error = bread(devvp, map, 1 << log2secsize, 0, &bp);
#ifdef FILECORE_DEBUG_BR
		printf("bread(%p, %x, %d, CRED, %p)=%d\n", devvp,
		       map, 1 << log2secsize, bp, error);
#endif
	if (error != 0)
		goto out;
       	fcdr = (struct filecore_disc_record *)((char *)(bp->b_data) + 4);
	fcmp = kmem_zalloc(sizeof(*fcmp), KM_SLEEP);
	if (fcdr->log2bpmb > fcdr->log2secsize)
		fcmp->log2bsize = fcdr->log2bpmb;
	else	fcmp->log2bsize = fcdr->log2secsize;
	fcmp->blksize = 1 << fcmp->log2bsize;
	memcpy(&fcmp->drec, fcdr, sizeof(*fcdr));
	fcmp->map = map;
	fcmp->idspz = ((8 << fcdr->log2secsize) - fcdr->zone_spare)
	    / (fcdr->idlen + 1);
	fcmp->mask = (1 << fcdr->idlen) - 1;
	if (fcdr->big_flag & 1) {
		fcmp->nblks = ((((u_int64_t)fcdr->disc_size_2) << 32)
		    + fcdr->disc_size) / fcmp->blksize;
	} else {
		fcmp->nblks=fcdr->disc_size / fcmp->blksize;
	}

#ifdef FILECORE_DEBUG_BR
	printf("brelse(%p) vf2\n", bp);
#endif
	brelse(bp, BC_AGE);
	bp = NULL;

	mp->mnt_data = fcmp;
	mp->mnt_stat.f_fsidx.__fsid_val[0] = (long)dev;
	mp->mnt_stat.f_fsidx.__fsid_val[1] = makefstype(MOUNT_FILECORE);
	mp->mnt_stat.f_fsid = mp->mnt_stat.f_fsidx.__fsid_val[0];
	mp->mnt_stat.f_namemax = 10;
	mp->mnt_flag |= MNT_LOCAL;
	mp->mnt_dev_bshift = fcdr->log2secsize;
	mp->mnt_fs_bshift = fcmp->log2bsize;

	fcmp->fc_mountp = mp;
	fcmp->fc_dev = dev;
	fcmp->fc_devvp = devvp;
	fcmp->fc_mntflags = argp->flags;
	if (argp->flags & FILECOREMNT_USEUID) {
		fcmp->fc_uid = kauth_cred_getuid(l->l_cred);
		fcmp->fc_gid = kauth_cred_getgid(l->l_cred);
	} else {
		fcmp->fc_uid = argp->uid;
		fcmp->fc_gid = argp->gid;
	}

	return 0;
out:
	if (bp) {
#ifdef FILECORE_DEBUG_BR
		printf("brelse(%p) vf3\n", bp);
#endif
		brelse(bp, 0);
	}
	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
	(void)VOP_CLOSE(devvp, ronly ? FREAD : FREAD|FWRITE, NOCRED);
	VOP_UNLOCK(devvp);
	return error;
}

/*
 * Make a filesystem operational.
 * Nothing to do at the moment.
 */
/* ARGSUSED */
int
filecore_start(struct mount *mp, int flags)
{
	return 0;
}

/*
 * unmount system call
 */
int
filecore_unmount(struct mount *mp, int mntflags)
{
	struct filecore_mnt *fcmp;
	int error, flags = 0;

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;
	if ((error = vflush(mp, NULLVP, flags)) != 0)
		return (error);

	fcmp = VFSTOFILECORE(mp);

	if (fcmp->fc_devvp->v_type != VBAD)
		spec_node_setmountedfs(fcmp->fc_devvp, NULL);
	vn_lock(fcmp->fc_devvp, LK_EXCLUSIVE | LK_RETRY);
	error = VOP_CLOSE(fcmp->fc_devvp, FREAD, NOCRED);
	vput(fcmp->fc_devvp);
	kmem_free(fcmp, sizeof(*fcmp));
	mp->mnt_data = NULL;
	mp->mnt_flag &= ~MNT_LOCAL;
	return (error);
}

/*
 * Return root of a filesystem
 */
int
filecore_root(struct mount *mp, struct vnode **vpp)
{
	struct vnode *nvp;
        int error;

        if ((error = VFS_VGET(mp, FILECORE_ROOTINO, &nvp)) != 0)
                return (error);
        *vpp = nvp;
        return (0);
}

/*
 * Get file system statistics.
 */
int
filecore_statvfs(struct mount *mp, struct statvfs *sbp)
{
	struct filecore_mnt *fcmp = VFSTOFILECORE(mp);

	sbp->f_bsize = fcmp->blksize;
	sbp->f_frsize = sbp->f_bsize; /* XXX */
	sbp->f_iosize = sbp->f_bsize;	/* XXX */
	sbp->f_blocks = fcmp->nblks;
	sbp->f_bfree = 0; /* total free blocks */
	sbp->f_bavail = 0; /* blocks free for non superuser */
	sbp->f_bresvd = 0; /* reserved blocks */
	sbp->f_files =  0; /* total files */
	sbp->f_ffree = 0; /* free file nodes for non superuser */
	sbp->f_favail = 0; /* free file nodes */
	sbp->f_fresvd = 0; /* reserved file nodes */
	copy_statvfs_info(sbp, mp);
	return 0;
}

/* ARGSUSED */
int
filecore_sync(struct mount *mp, int waitfor, kauth_cred_t cred)
{
	return (0);
}

/*
 * File handle to vnode
 *
 * Have to be really careful about stale file handles:
 * - check that the inode number is in range
 * - call iget() to get the locked inode
 * - check for an unallocated inode (i_mode == 0)
 * - check that the generation number matches
 */

struct ifid {
	ushort		ifid_len;
	ushort		ifid_pad;
	u_int32_t	ifid_ino;
};

/* ARGSUSED */
int
filecore_fhtovp(struct mount *mp, struct fid *fhp, struct vnode **vpp)
{
	struct ifid ifh;
	struct vnode *nvp;
	struct filecore_node *ip;
	int error;

	if (fhp->fid_len != sizeof(struct ifid))
		return EINVAL;

	memcpy(&ifh, fhp, sizeof(ifh));
	if ((error = VFS_VGET(mp, ifh.ifid_ino, &nvp)) != 0) {
		*vpp = NULLVP;
		return (error);
	}
        ip = VTOI(nvp);
        if (filecore_staleinode(ip)) {
                vput(nvp);
                *vpp = NULLVP;
                return (ESTALE);
        }
	*vpp = nvp;
	return (0);
}

/* This looks complicated. Look at other vgets as well as the iso9660 one.
 *
 * The filecore inode number is made up of 1 byte directory entry index and
 * 3 bytes of the internal disc address of the directory. On a read-only
 * filesystem this is unique. For a read-write version we may not be able to
 * do vget, see msdosfs.
 */

int
filecore_vget(struct mount *mp, ino_t ino, struct vnode **vpp)
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
 * Vnode pointer to File handle
 */
/* ARGSUSED */
int
filecore_vptofh(struct vnode *vp, struct fid *fhp, size_t *fh_size)
{
	struct filecore_node *ip = VTOI(vp);
	struct ifid ifh;

	if (*fh_size < sizeof(struct ifid)) {
		*fh_size = sizeof(struct ifid);
		return E2BIG;
	}
	memset(&ifh, 0, sizeof(ifh));
	ifh.ifid_len = sizeof(struct ifid);
	ifh.ifid_ino = ip->i_number;
	memcpy(fhp, &ifh, sizeof(ifh));
       	return 0;
}
