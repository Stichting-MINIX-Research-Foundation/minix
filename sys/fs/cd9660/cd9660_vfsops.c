/*	$NetBSD: cd9660_vfsops.c,v 1.90 2015/03/28 19:24:05 maxv Exp $	*/

/*-
 * Copyright (c) 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley
 * by Pace Willisson (pace@blitz.com).  The Rock Ridge Extension
 * Support code is derived from software contributed to Berkeley
 * by Atsushi Murai (amurai@spec.co.jp).
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
 *	@(#)cd9660_vfsops.c	8.18 (Berkeley) 5/22/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cd9660_vfsops.c,v 1.90 2015/03/28 19:24:05 maxv Exp $");

#if defined(_KERNEL_OPT)
#include "opt_compat_netbsd.h"
#endif

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <miscfs/genfs/genfs.h>
#include <miscfs/specfs/specdev.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/file.h>
#include <sys/disklabel.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/cdio.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/stat.h>
#include <sys/conf.h>
#include <sys/dirent.h>
#include <sys/kauth.h>
#include <sys/module.h>

#include <fs/cd9660/iso.h>
#include <fs/cd9660/cd9660_extern.h>
#include <fs/cd9660/iso_rrip.h>
#include <fs/cd9660/cd9660_node.h>
#include <fs/cd9660/cd9660_mount.h>

MODULE(MODULE_CLASS_VFS, cd9660, NULL);

MALLOC_JUSTDEFINE(M_ISOFSMNT, "ISOFS mount", "ISOFS mount structure");

static struct sysctllog *cd9660_sysctl_log;

extern const struct vnodeopv_desc cd9660_vnodeop_opv_desc;
extern const struct vnodeopv_desc cd9660_specop_opv_desc;
extern const struct vnodeopv_desc cd9660_fifoop_opv_desc;

const struct vnodeopv_desc * const cd9660_vnodeopv_descs[] = {
	&cd9660_vnodeop_opv_desc,
	&cd9660_specop_opv_desc,
	&cd9660_fifoop_opv_desc,
	NULL,
};

struct vfsops cd9660_vfsops = {
	.vfs_name = MOUNT_CD9660,
	.vfs_min_mount_data = sizeof (struct iso_args),
	.vfs_mount = cd9660_mount,
	.vfs_start = cd9660_start,
	.vfs_unmount = cd9660_unmount,
	.vfs_root = cd9660_root,
	.vfs_quotactl = (void *)eopnotsupp,
	.vfs_statvfs = cd9660_statvfs,
	.vfs_sync = cd9660_sync,
	.vfs_vget = cd9660_vget,
	.vfs_loadvnode = cd9660_loadvnode,
	.vfs_fhtovp = cd9660_fhtovp,
	.vfs_vptofh = cd9660_vptofh,
	.vfs_init = cd9660_init,
	.vfs_reinit = cd9660_reinit,
	.vfs_done = cd9660_done,
	.vfs_mountroot = cd9660_mountroot,
	.vfs_snapshot = (void *)eopnotsupp,
	.vfs_extattrctl = vfs_stdextattrctl,
	.vfs_suspendctl = (void *)eopnotsupp,
	.vfs_renamelock_enter = genfs_renamelock_enter,
	.vfs_renamelock_exit = genfs_renamelock_exit,
	.vfs_fsync = (void *)eopnotsupp,
	.vfs_opv_descs = cd9660_vnodeopv_descs
};

static const struct genfs_ops cd9660_genfsops = {
	.gop_size = genfs_size,
};

/*
 * Called by vfs_mountroot when iso is going to be mounted as root.
 *
 * Name is updated by mount(8) after booting.
 */
static int iso_makemp(struct iso_mnt *isomp, struct buf *bp, int *ea_len);
static int iso_mountfs(struct vnode *devvp, struct mount *mp,
		struct lwp *l, struct iso_args *argp);

static int
cd9660_modcmd(modcmd_t cmd, void *arg)
{
	int error;

	switch (cmd) {
	case MODULE_CMD_INIT:
		error = vfs_attach(&cd9660_vfsops);
		if (error != 0)
			break;
		sysctl_createv(&cd9660_sysctl_log, 0, NULL, NULL,
			       CTLFLAG_PERMANENT, CTLTYPE_NODE, "cd9660",
			       SYSCTL_DESCR("ISO-9660 file system"),
			       NULL, 0, NULL, 0,
			       CTL_VFS, 14, CTL_EOL);
		sysctl_createv(&cd9660_sysctl_log, 0, NULL, NULL,
			       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
			       CTLTYPE_INT, "utf8_joliet",
			       SYSCTL_DESCR("Encode Joliet filenames to UTF-8"),
			       NULL, 0, &cd9660_utf8_joliet, 0,
			       CTL_VFS, 14, CD9660_UTF8_JOLIET, CTL_EOL);
		/*
		 * XXX the "14" above could be dynamic, thereby eliminating
		 * one more instance of the "number to vfs" mapping problem,
		 * but "14" is the order as taken from sys/mount.h
		 */
		break;
	case MODULE_CMD_FINI:
		error = vfs_detach(&cd9660_vfsops);
		if (error != 0)
			break;
		sysctl_teardown(&cd9660_sysctl_log);
		break;
	default:
		error = ENOTTY;
		break;
	}

	return (error);
}

int
cd9660_mountroot(void)
{
	struct mount *mp;
	struct lwp *l = curlwp;
	int error;
	struct iso_args args;

	if (device_class(root_device) != DV_DISK)
		return (ENODEV);

	if ((error = vfs_rootmountalloc(MOUNT_CD9660, "root_device", &mp))
			!= 0) {
		vrele(rootvp);
		return (error);
	}

	args.flags = ISOFSMNT_ROOT;
	if ((error = iso_mountfs(rootvp, mp, l, &args)) != 0) {
		vfs_unbusy(mp, false, NULL);
		vfs_destroy(mp);
		return (error);
	}
	mountlist_append(mp);
	(void)cd9660_statvfs(mp, &mp->mnt_stat);
	vfs_unbusy(mp, false, NULL);
	return (0);
}

/*
 * VFS Operations.
 *
 * mount system call
 */
int
cd9660_mount(struct mount *mp, const char *path, void *data, size_t *data_len)
{
	struct lwp *l = curlwp;
	struct vnode *devvp;
	struct iso_args *args = data;
	int error;
	struct iso_mnt *imp = VFSTOISOFS(mp);

	if (args == NULL)
		return EINVAL;
	if (*data_len < sizeof *args)
		return EINVAL;

	if (mp->mnt_flag & MNT_GETARGS) {
		if (imp == NULL)
			return EIO;
		args->fspec = NULL;
		args->flags = imp->im_flags;
		*data_len = sizeof (*args);
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
	if (error) {
		goto fail;
	}
	if ((mp->mnt_flag & MNT_UPDATE) == 0) {
		error = VOP_OPEN(devvp, FREAD, FSCRED);
		if (error)
			goto fail;
		VOP_UNLOCK(devvp);
		error = iso_mountfs(devvp, mp, l, args);
		vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
		if (error) {
			(void)VOP_CLOSE(devvp, FREAD, NOCRED);
			goto fail;
		}
		VOP_UNLOCK(devvp);
		/* reference to devvp is donated through iso_mountfs */
	} else {
		if (devvp != imp->im_devvp &&
		    devvp->v_rdev != imp->im_devvp->v_rdev) {
			error = EINVAL;		/* needs translation */
			goto fail;
		}
		VOP_UNLOCK(devvp);
		vrele(devvp);
	}
	return set_statvfs_info(path, UIO_USERSPACE, args->fspec, UIO_USERSPACE,
	    mp->mnt_op->vfs_name, mp, l);

fail:
	VOP_UNLOCK(devvp);
	vrele(devvp);
	return (error);
}

/*
 * Make a mount point from a volume descriptor
 */
static int
iso_makemp(struct iso_mnt *isomp, struct buf *bp, int *ea_len)
{
	struct iso_primary_descriptor *pri;
	int logical_block_size;
	struct iso_directory_record *rootp;

	pri = (struct iso_primary_descriptor *)bp->b_data;

	logical_block_size = isonum_723 (pri->logical_block_size);

	if (logical_block_size < DEV_BSIZE || logical_block_size > MAXBSIZE
	    || (logical_block_size & (logical_block_size - 1)) != 0)
		return -1;

	rootp = (struct iso_directory_record *)pri->root_directory_record;

	isomp->logical_block_size = logical_block_size;
	isomp->volume_space_size = isonum_733 (pri->volume_space_size);
	memcpy(isomp->root, rootp, sizeof(isomp->root));
	isomp->root_extent = isonum_733 (rootp->extent);
	isomp->root_size = isonum_733 (rootp->size);
	isomp->im_joliet_level = 0;

	isomp->im_bmask = logical_block_size - 1;
	isomp->im_bshift = 0;
	while ((1 << isomp->im_bshift) < isomp->logical_block_size)
		isomp->im_bshift++;

	if (ea_len != NULL)
		*ea_len = isonum_711(rootp->ext_attr_length);

	return 0;
}

/*
 * Common code for mount and mountroot
 */
static int
iso_mountfs(struct vnode *devvp, struct mount *mp, struct lwp *l,
	struct iso_args *argp)
{
	struct iso_mnt *isomp = (struct iso_mnt *)0;
	struct buf *bp = NULL, *pribp = NULL, *supbp = NULL;
	dev_t dev = devvp->v_rdev;
	int error = EINVAL;
	int ronly = (mp->mnt_flag & MNT_RDONLY) != 0;
	int iso_bsize;
	int iso_blknum;
	int joliet_level;
	struct iso_volume_descriptor *vdp;
	struct iso_supplementary_descriptor *sup;
	int sess = 0;
	int ext_attr_length;
	struct disklabel label;

	if (!ronly)
		return EROFS;

	/* Flush out any old buffers remaining from a previous use. */
	if ((error = vinvalbuf(devvp, V_SAVE, l->l_cred, l, 0, 0)) != 0)
		return (error);

	/* This is the "logical sector size".  The standard says this
	 * should be 2048 or the physical sector size on the device,
	 * whichever is greater.  For now, we'll just use a constant.
	 */
	iso_bsize = ISO_DEFAULT_BLOCK_SIZE;

	error = VOP_IOCTL(devvp, DIOCGDINFO, &label, FREAD, FSCRED);
	if (!error) {
		/* XXX more sanity checks? */
		sess = label.d_partitions[DISKPART(dev)].p_cdsession;
	} else {
		/* fallback to old method */
		error = VOP_IOCTL(devvp, CDIOREADMSADDR, &sess, 0, FSCRED);
		if (error)
			sess = 0;	/* never mind */
	}
#ifdef ISO_DEBUG
	printf("isofs: session offset (part %"PRId32") %d\n", DISKPART(dev), sess);
#endif

	for (iso_blknum = 16; iso_blknum < 100; iso_blknum++) {
		if ((error = bread(devvp, (iso_blknum+sess) * btodb(iso_bsize),
				   iso_bsize, 0, &bp)) != 0)
			goto out;

		vdp = (struct iso_volume_descriptor *)bp->b_data;
		if (memcmp(vdp->id, ISO_STANDARD_ID, sizeof(vdp->id)) != 0) {
			error = EINVAL;
			goto out;
		}

		switch (isonum_711(vdp->type)) {
		case ISO_VD_PRIMARY:
			if (pribp == NULL) {
				pribp = bp;
				bp = NULL;
			}
			break;

		case ISO_VD_SUPPLEMENTARY:
			if (supbp == NULL) {
				supbp = bp;
				bp = NULL;
			}
			break;

		default:
			break;
		}

		if (isonum_711 (vdp->type) == ISO_VD_END) {
			brelse(bp, 0);
			bp = NULL;
			break;
		}

		if (bp != NULL) {
			brelse(bp, 0);
			bp = NULL;
		}
	}

	if (pribp == NULL) {
		error = EINVAL;
		goto out;
	}

	isomp = malloc(sizeof *isomp, M_ISOFSMNT, M_WAITOK);
	memset(isomp, 0, sizeof *isomp);
	if (iso_makemp(isomp, pribp, &ext_attr_length) == -1) {
		error = EINVAL;
		goto out;
	}

	isomp->volume_space_size += sess;

	brelse(pribp, BC_AGE);
	pribp = NULL;

	mp->mnt_data = isomp;
	mp->mnt_stat.f_fsidx.__fsid_val[0] = (long)dev;
	mp->mnt_stat.f_fsidx.__fsid_val[1] = makefstype(MOUNT_CD9660);
	mp->mnt_stat.f_fsid = mp->mnt_stat.f_fsidx.__fsid_val[0];
	mp->mnt_stat.f_namemax = ISO_MAXNAMLEN;
	mp->mnt_flag |= MNT_LOCAL;
	mp->mnt_iflag |= IMNT_MPSAFE;
	mp->mnt_dev_bshift = iso_bsize;
	mp->mnt_fs_bshift = isomp->im_bshift;
	isomp->im_mountp = mp;
	isomp->im_dev = dev;
	isomp->im_devvp = devvp;

	/* Check the Rock Ridge Extension support */
	if (!(argp->flags & ISOFSMNT_NORRIP)) {
		struct iso_directory_record *rootp;

		if ((error = bread(isomp->im_devvp,
				   (isomp->root_extent + ext_attr_length) <<
				   (isomp->im_bshift - DEV_BSHIFT),
				   isomp->logical_block_size,
				   0, &bp)) != 0)
		    goto out;

		rootp = (struct iso_directory_record *)bp->b_data;

		if ((isomp->rr_skip = cd9660_rrip_offset(rootp,isomp)) < 0) {
		    argp->flags  |= ISOFSMNT_NORRIP;
		} else {
		    argp->flags  &= ~ISOFSMNT_GENS;
		}

		/*
		 * The contents are valid,
		 * but they will get reread as part of another vnode, so...
		 */
		brelse(bp, BC_AGE);
		bp = NULL;
	}
	isomp->im_flags = argp->flags & (ISOFSMNT_NORRIP | ISOFSMNT_GENS |
		 ISOFSMNT_EXTATT | ISOFSMNT_NOJOLIET | ISOFSMNT_RRCASEINS);

	if (isomp->im_flags & ISOFSMNT_GENS)
		isomp->iso_ftype = ISO_FTYPE_9660;
	else if (isomp->im_flags & ISOFSMNT_NORRIP) {
		isomp->iso_ftype = ISO_FTYPE_DEFAULT;
		if (argp->flags & ISOFSMNT_NOCASETRANS)
			isomp->im_flags |= ISOFSMNT_NOCASETRANS;
	} else
		isomp->iso_ftype = ISO_FTYPE_RRIP;

	/* Check the Joliet Extension support */
	if ((argp->flags & ISOFSMNT_NORRIP) != 0 &&
	    (argp->flags & ISOFSMNT_NOJOLIET) == 0 &&
	    supbp != NULL) {
		joliet_level = 0;
		sup = (struct iso_supplementary_descriptor *)supbp->b_data;

		if ((isonum_711(sup->flags) & 1) == 0) {
			if (memcmp(sup->escape, "%/@", 3) == 0)
				joliet_level = 1;
			if (memcmp(sup->escape, "%/C", 3) == 0)
				joliet_level = 2;
			if (memcmp(sup->escape, "%/E", 3) == 0)
				joliet_level = 3;
		}
		if (joliet_level != 0) {
			if (iso_makemp(isomp, supbp, NULL) == -1) {
				error = EINVAL;
				goto out;
			}
			isomp->im_joliet_level = joliet_level;
		}
	}

	if (supbp != NULL) {
		brelse(supbp, 0);
		supbp = NULL;
	}

	spec_node_setmountedfs(devvp, mp);

	return 0;
out:
	if (bp)
		brelse(bp, 0);
	if (pribp)
		brelse(pribp, 0);
	if (supbp)
		brelse(supbp, 0);
	if (isomp) {
		free(isomp, M_ISOFSMNT);
		mp->mnt_data = NULL;
	}
	return error;
}

/*
 * Make a filesystem operational.
 * Nothing to do at the moment.
 */
/* ARGSUSED */
int
cd9660_start(struct mount *mp, int flags)
{
	return 0;
}

/*
 * unmount system call
 */
int
cd9660_unmount(struct mount *mp, int mntflags)
{
	struct iso_mnt *isomp;
	int error, flags = 0;

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;
	if ((error = vflush(mp, NULLVP, flags)) != 0)
		return (error);

	isomp = VFSTOISOFS(mp);

	if (isomp->im_devvp->v_type != VBAD)
		spec_node_setmountedfs(isomp->im_devvp, NULL);

	vn_lock(isomp->im_devvp, LK_EXCLUSIVE | LK_RETRY);
	error = VOP_CLOSE(isomp->im_devvp, FREAD, NOCRED);
	vput(isomp->im_devvp);
	free(isomp, M_ISOFSMNT);
	mp->mnt_data = NULL;
	mp->mnt_flag &= ~MNT_LOCAL;
	return (error);
}

/*
 * Return root of a filesystem
 */
int
cd9660_root(struct mount *mp, struct vnode **vpp)
{
	struct iso_mnt *imp = VFSTOISOFS(mp);
	struct iso_directory_record *dp =
	    (struct iso_directory_record *)imp->root;
	ino_t ino = isodirino(dp, imp);

	return cd9660_vget(mp, ino, vpp);
}

/*
 * Get file system statistics.
 */
int
cd9660_statvfs(struct mount *mp, struct statvfs *sbp)
{
	struct iso_mnt *isomp;

	isomp = VFSTOISOFS(mp);

	sbp->f_bsize = isomp->logical_block_size;
	sbp->f_frsize = sbp->f_bsize;
	sbp->f_iosize = sbp->f_bsize;	/* XXX */
	sbp->f_blocks = isomp->volume_space_size;
	sbp->f_bfree = 0; /* total free blocks */
	sbp->f_bavail = 0; /* blocks free for non superuser */
	sbp->f_bresvd = 0; /* total reserved blocks */
	sbp->f_files =  0; /* total files */
	sbp->f_ffree = 0; /* free file nodes */
	sbp->f_favail = 0; /* free file nodes for non superuser */
	sbp->f_fresvd = 0; /* reserved file nodes */
	copy_statvfs_info(sbp, mp);
	/* Use the first spare for flags: */
	sbp->f_spare[0] = isomp->im_flags;
	return 0;
}

/* ARGSUSED */
int
cd9660_sync(struct mount *mp, int waitfor, kauth_cred_t cred)
{
	return 0;
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
	ushort	ifid_len;
	ushort	ifid_pad;
	ino_t	ifid_ino;
#ifdef	ISOFS_DBG
	u_long	ifid_start;
#endif
};

/* ARGSUSED */
int
cd9660_fhtovp(struct mount *mp, struct fid *fhp, struct vnode **vpp)
{
	struct ifid ifh;
	struct iso_node *ip;
	struct vnode *nvp;
	int error;

	if (fhp->fid_len != sizeof(ifh))
		return EINVAL;

	memcpy(&ifh, fhp, sizeof(ifh));
#ifdef	ISOFS_DBG
	printf("fhtovp: ino %"PRIu64", start %lu\n",
	    ifh.ifid_ino, ifh.ifid_start);
#endif

	if ((error = VFS_VGET(mp, ifh.ifid_ino, &nvp)) != 0) {
		*vpp = NULLVP;
		return (error);
	}
	ip = VTOI(nvp);
	if (ip->inode.iso_mode == 0) {
		vput(nvp);
		*vpp = NULLVP;
		return (ESTALE);
	}
	*vpp = nvp;
	return (0);
}

int
cd9660_vget(struct mount *mp, ino_t ino, struct vnode **vpp)
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

int
cd9660_loadvnode(struct mount *mp, struct vnode *vp,
    const void *key, size_t key_len, const void **new_key)
{
	struct iso_mnt *imp;
	struct iso_node *ip;
	struct iso_directory_record *isodir;
	struct buf *bp;
	dev_t dev;
	ino_t ino;
	int lbn, off;
	int error;

	KASSERT(key_len == sizeof(ino));
	memcpy(&ino, key, key_len);
	imp = VFSTOISOFS(mp);
	dev = imp->im_dev;

	ip = pool_get(&cd9660_node_pool, PR_WAITOK);

	memset(ip, 0, sizeof(struct iso_node));
	ip->i_vnode = vp;
	ip->i_dev = dev;
	ip->i_number = ino;
	ip->i_mnt = imp;
	ip->i_devvp = imp->im_devvp;

	lbn = cd9660_lblkno(imp, ino);
	if (lbn >= imp->volume_space_size) {
		pool_put(&cd9660_node_pool, ip);
		printf("fhtovp: lbn exceed volume space %d\n", lbn);
		return (ESTALE);
	}

	off = cd9660_blkoff(imp, ino);
	if (off + ISO_DIRECTORY_RECORD_SIZE > imp->logical_block_size) {
		pool_put(&cd9660_node_pool, ip);
		printf("fhtovp: crosses block boundary %d\n",
		    off + ISO_DIRECTORY_RECORD_SIZE);
		return (ESTALE);
	}

	error = bread(imp->im_devvp,
		      lbn << (imp->im_bshift - DEV_BSHIFT),
		      imp->logical_block_size, 0, &bp);
	if (error) {
		pool_put(&cd9660_node_pool, ip);
		printf("fhtovp: bread error %d\n",error);
		return (error);
	}
	isodir = (struct iso_directory_record *)((char *)bp->b_data + off);

	if (off + isonum_711(isodir->length) > imp->logical_block_size) {
		pool_put(&cd9660_node_pool, ip);
		brelse(bp, 0);
		printf("fhtovp: directory crosses block boundary %d[off=%d/len=%d]\n",
		    off +isonum_711(isodir->length), off,
		    isonum_711(isodir->length));
		return (ESTALE);
	}

#if 0
	if (isonum_733(isodir->extent) +
	    isonum_711(isodir->ext_attr_length) != ifhp->ifid_start) {
		pool_put(&cd9660_node_pool, ip);
		if (bp != 0)
			brelse(bp, 0);
		printf("fhtovp: file start miss %d vs %d\n",
		    isonum_733(isodir->extent) + isonum_711(isodir->ext_attr_length),
		    ifhp->ifid_start);
		return (ESTALE);
	}
#endif

	ip->iso_extent = isonum_733(isodir->extent);
	ip->i_size = isonum_733(isodir->size);
	ip->iso_start = isonum_711(isodir->ext_attr_length) + ip->iso_extent;

	vp->v_tag = VT_ISOFS;
	vp->v_op = cd9660_vnodeop_p;
	vp->v_data = ip;
	genfs_node_init(vp, &cd9660_genfsops);

	/*
	 * Setup time stamp, attribute
	 */
	switch (imp->iso_ftype) {
	default:	/* ISO_FTYPE_9660 */
	    {
		struct buf *bp2;
		if ((imp->im_flags & ISOFSMNT_EXTATT)
		    && (off = isonum_711(isodir->ext_attr_length)))
			cd9660_blkatoff(vp, (off_t)-(off << imp->im_bshift),
			    NULL, &bp2);
		else
			bp2 = NULL;
		cd9660_defattr(isodir, ip, bp2);
		cd9660_deftstamp(isodir, ip, bp2);
		if (bp2)
			brelse(bp2, 0);
		break;
	    }
	case ISO_FTYPE_RRIP:
		cd9660_rrip_analyze(isodir, ip, imp);
		break;
	}

	brelse(bp, 0);

	/*
	 * Initialize the associated vnode
	 */
	switch (vp->v_type = IFTOVT(ip->inode.iso_mode)) {
	case VFIFO:
		vp->v_op = cd9660_fifoop_p;
		break;
	case VCHR:
	case VBLK:
		/*
		 * if device, look at device number table for translation
		 */
		vp->v_op = cd9660_specop_p;
		spec_node_init(vp, ip->inode.iso_rdev);
		break;
	case VLNK:
	case VNON:
	case VSOCK:
	case VDIR:
	case VBAD:
		break;
	case VREG:
		uvm_vnp_setsize(vp, ip->i_size);
		break;
	}

	if (vp->v_type != VREG)
		uvm_vnp_setsize(vp, 0);

	if (ip->iso_extent == imp->root_extent)
		vp->v_vflag |= VV_ROOT;

	/*
	 * XXX need generation number?
	 */

	*new_key = &ip->i_number;
	return 0;
}

/*
 * Vnode pointer to File handle
 */
/* ARGSUSED */
int
cd9660_vptofh(struct vnode *vp, struct fid *fhp, size_t *fh_size)
{
	struct iso_node *ip = VTOI(vp);
	struct ifid ifh;

	if (*fh_size < sizeof(struct ifid)) {
		*fh_size = sizeof(struct ifid);
		return E2BIG;
	}
	*fh_size = sizeof(struct ifid);

	memset(&ifh, 0, sizeof(ifh));
	ifh.ifid_len = sizeof(struct ifid);
	ifh.ifid_ino = ip->i_number;
#ifdef	ISOFS_DBG
	ifh.ifid_start = ip->iso_start;
#endif
	memcpy(fhp, &ifh, sizeof(ifh));

#ifdef	ISOFS_DBG
	printf("vptofh: ino %"PRIu64", start %lu\n",
	    ifh.ifid_ino,ifh.ifid_start);
#endif
	return 0;
}
