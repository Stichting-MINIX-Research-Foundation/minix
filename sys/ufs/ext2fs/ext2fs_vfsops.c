/*	$NetBSD: ext2fs_vfsops.c,v 1.193 2015/03/28 19:24:04 maxv Exp $	*/

/*
 * Copyright (c) 1989, 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)ffs_vfsops.c	8.14 (Berkeley) 11/28/94
 * Modified for ext2fs by Manuel Bouyer.
 */

/*
 * Copyright (c) 1997 Manuel Bouyer.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	@(#)ffs_vfsops.c	8.14 (Berkeley) 11/28/94
 * Modified for ext2fs by Manuel Bouyer.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ext2fs_vfsops.c,v 1.193 2015/03/28 19:24:04 maxv Exp $");

#if defined(_KERNEL_OPT)
#include "opt_compat_netbsd.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/socket.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/mbuf.h>
#include <sys/file.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/pool.h>
#include <sys/lock.h>
#include <sys/conf.h>
#include <sys/kauth.h>
#include <sys/module.h>

#include <miscfs/genfs/genfs.h>
#include <miscfs/specfs/specdev.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/ext2fs/ext2fs.h>
#include <ufs/ext2fs/ext2fs_dir.h>
#include <ufs/ext2fs/ext2fs_extern.h>

MODULE(MODULE_CLASS_VFS, ext2fs, "ffs");

int ext2fs_sbupdate(struct ufsmount *, int);
static int ext2fs_sbfill(struct m_ext2fs *, int);

static struct sysctllog *ext2fs_sysctl_log;

extern const struct vnodeopv_desc ext2fs_vnodeop_opv_desc;
extern const struct vnodeopv_desc ext2fs_specop_opv_desc;
extern const struct vnodeopv_desc ext2fs_fifoop_opv_desc;

const struct vnodeopv_desc * const ext2fs_vnodeopv_descs[] = {
	&ext2fs_vnodeop_opv_desc,
	&ext2fs_specop_opv_desc,
	&ext2fs_fifoop_opv_desc,
	NULL,
};

struct vfsops ext2fs_vfsops = {
	.vfs_name = MOUNT_EXT2FS,
	.vfs_min_mount_data = sizeof (struct ufs_args),
	.vfs_mount = ext2fs_mount,
	.vfs_start = ufs_start,
	.vfs_unmount = ext2fs_unmount,
	.vfs_root = ufs_root,
	.vfs_quotactl = ufs_quotactl,
	.vfs_statvfs = ext2fs_statvfs,
	.vfs_sync = ext2fs_sync,
	.vfs_vget = ufs_vget,
	.vfs_loadvnode = ext2fs_loadvnode,
	.vfs_fhtovp = ext2fs_fhtovp,
	.vfs_vptofh = ext2fs_vptofh,
	.vfs_init = ext2fs_init,
	.vfs_reinit = ext2fs_reinit,
	.vfs_done = ext2fs_done,
	.vfs_mountroot = ext2fs_mountroot,
	.vfs_snapshot = (void *)eopnotsupp,
	.vfs_extattrctl = vfs_stdextattrctl,
	.vfs_suspendctl = (void *)eopnotsupp,
	.vfs_renamelock_enter = genfs_renamelock_enter,
	.vfs_renamelock_exit = genfs_renamelock_exit,
	.vfs_fsync = (void *)eopnotsupp,
	.vfs_opv_descs = ext2fs_vnodeopv_descs
};

static const struct genfs_ops ext2fs_genfsops = {
	.gop_size = genfs_size,
	.gop_alloc = ext2fs_gop_alloc,
	.gop_write = genfs_gop_write,
	.gop_markupdate = ufs_gop_markupdate,
};

static const struct ufs_ops ext2fs_ufsops = {
	.uo_itimes = ext2fs_itimes,
	.uo_update = ext2fs_update,
	.uo_bufrd = ext2fs_bufrd,
	.uo_bufwr = ext2fs_bufwr,
};

/* Fill in the inode uid/gid from ext2 halves.  */
void
ext2fs_set_inode_guid(struct inode *ip)
{

	ip->i_gid = ip->i_e2fs_gid;
	ip->i_uid = ip->i_e2fs_uid;
	if (ip->i_e2fs->e2fs.e2fs_rev > E2FS_REV0) {
		ip->i_gid |= ip->i_e2fs_gid_high << 16;
		ip->i_uid |= ip->i_e2fs_uid_high << 16;
	}
}

static int
ext2fs_modcmd(modcmd_t cmd, void *arg)
{
	int error;

	switch (cmd) {
	case MODULE_CMD_INIT:
		error = vfs_attach(&ext2fs_vfsops);
		if (error != 0)
			break;
		sysctl_createv(&ext2fs_sysctl_log, 0, NULL, NULL,
			       CTLFLAG_PERMANENT,
			       CTLTYPE_NODE, "ext2fs",
			       SYSCTL_DESCR("Linux EXT2FS file system"),
			       NULL, 0, NULL, 0,
			       CTL_VFS, 17, CTL_EOL);
		/*
		 * XXX the "17" above could be dynamic, thereby eliminating
		 * one more instance of the "number to vfs" mapping problem,
		 * but "17" is the order as taken from sys/mount.h
		 */
		break;
	case MODULE_CMD_FINI:
		error = vfs_detach(&ext2fs_vfsops);
		if (error != 0)
			break;
		sysctl_teardown(&ext2fs_sysctl_log);
		break;
	default:
		error = ENOTTY;
		break;
	}

	return (error);
}

/*
 * XXX Same structure as FFS inodes?  Should we share a common pool?
 */
struct pool ext2fs_inode_pool;
struct pool ext2fs_dinode_pool;

extern u_long ext2gennumber;

void
ext2fs_init(void)
{

	pool_init(&ext2fs_inode_pool, sizeof(struct inode), 0, 0, 0,
	    "ext2fsinopl", &pool_allocator_nointr, IPL_NONE);
	pool_init(&ext2fs_dinode_pool, sizeof(struct ext2fs_dinode), 0, 0, 0,
	    "ext2dinopl", &pool_allocator_nointr, IPL_NONE);
	ufs_init();
}

void
ext2fs_reinit(void)
{
	ufs_reinit();
}

void
ext2fs_done(void)
{

	ufs_done();
	pool_destroy(&ext2fs_inode_pool);
	pool_destroy(&ext2fs_dinode_pool);
}

/*
 * Called by main() when ext2fs is going to be mounted as root.
 *
 * Name is updated by mount(8) after booting.
 */

int
ext2fs_mountroot(void)
{
	extern struct vnode *rootvp;
	struct m_ext2fs *fs;
	struct mount *mp;
	struct ufsmount *ump;
	int error;

	if (device_class(root_device) != DV_DISK)
		return (ENODEV);

	if ((error = vfs_rootmountalloc(MOUNT_EXT2FS, "root_device", &mp))) {
		vrele(rootvp);
		return (error);
	}

	if ((error = ext2fs_mountfs(rootvp, mp)) != 0) {
		vfs_unbusy(mp, false, NULL);
		vfs_destroy(mp);
		return (error);
	}
	mountlist_append(mp);
	ump = VFSTOUFS(mp);
	fs = ump->um_e2fs;
	memset(fs->e2fs_fsmnt, 0, sizeof(fs->e2fs_fsmnt));
	(void) copystr(mp->mnt_stat.f_mntonname, fs->e2fs_fsmnt,
	    sizeof(fs->e2fs_fsmnt) - 1, 0);
	if (fs->e2fs.e2fs_rev > E2FS_REV0) {
		memset(fs->e2fs.e2fs_fsmnt, 0, sizeof(fs->e2fs.e2fs_fsmnt));
		(void) copystr(mp->mnt_stat.f_mntonname, fs->e2fs.e2fs_fsmnt,
		    sizeof(fs->e2fs.e2fs_fsmnt) - 1, 0);
	}
	(void)ext2fs_statvfs(mp, &mp->mnt_stat);
	vfs_unbusy(mp, false, NULL);
	setrootfstime((time_t)fs->e2fs.e2fs_wtime);
	return (0);
}

/*
 * VFS Operations.
 *
 * mount system call
 */
int
ext2fs_mount(struct mount *mp, const char *path, void *data, size_t *data_len)
{
	struct lwp *l = curlwp;
	struct vnode *devvp;
	struct ufs_args *args = data;
	struct ufsmount *ump = NULL;
	struct m_ext2fs *fs;
	size_t size;
	int error = 0, flags, update;
	mode_t accessmode;

	if (args == NULL)
		return EINVAL;
	if (*data_len < sizeof *args)
		return EINVAL;

	if (mp->mnt_flag & MNT_GETARGS) {
		ump = VFSTOUFS(mp);
		if (ump == NULL)
			return EIO;
		memset(args, 0, sizeof *args);
		args->fspec = NULL;
		*data_len = sizeof *args;
		return 0;
	}

	update = mp->mnt_flag & MNT_UPDATE;

	/* Check arguments */
	if (args->fspec != NULL) {
		/*
		 * Look up the name and verify that it's sane.
		 */
		error = namei_simple_user(args->fspec,
					NSM_FOLLOW_NOEMULROOT, &devvp);
		if (error != 0)
			return (error);

		if (!update) {
			/*
			 * Be sure this is a valid block device
			 */
			if (devvp->v_type != VBLK)
				error = ENOTBLK;
			else if (bdevsw_lookup(devvp->v_rdev) == NULL)
				error = ENXIO;
		} else {
		        /*
			 * Be sure we're still naming the same device
			 * used for our initial mount
			 */
			ump = VFSTOUFS(mp);
			if (devvp != ump->um_devvp) {
				if (devvp->v_rdev != ump->um_devvp->v_rdev)
					error = EINVAL;
				else {
					vrele(devvp);
					devvp = ump->um_devvp;
					vref(devvp);
				}
			}
		}
	} else {
		if (!update) {
			/* New mounts must have a filename for the device */
			return (EINVAL);
		} else {
			ump = VFSTOUFS(mp);
			devvp = ump->um_devvp;
			vref(devvp);
		}
	}

	/*
	 * If mount by non-root, then verify that user has necessary
	 * permissions on the device.
	 *
	 * Permission to update a mount is checked higher, so here we presume
	 * updating the mount is okay (for example, as far as securelevel goes)
	 * which leaves us with the normal check.
	 */
	if (error == 0) {
		accessmode = VREAD;
		if (update ?
		    (mp->mnt_iflag & IMNT_WANTRDWR) != 0 :
		    (mp->mnt_flag & MNT_RDONLY) == 0)
			accessmode |= VWRITE;
		vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
		error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_MOUNT,
		    KAUTH_REQ_SYSTEM_MOUNT_DEVICE, mp, devvp,
		    KAUTH_ARG(accessmode));
		VOP_UNLOCK(devvp);
	}

	if (error) {
		vrele(devvp);
		return (error);
	}

	if (!update) {
		int xflags;

		if (mp->mnt_flag & MNT_RDONLY)
			xflags = FREAD;
		else
			xflags = FREAD|FWRITE;
		vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
		error = VOP_OPEN(devvp, xflags, FSCRED);
		VOP_UNLOCK(devvp);
		if (error)
			goto fail;
		error = ext2fs_mountfs(devvp, mp);
		if (error) {
			vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
			(void)VOP_CLOSE(devvp, xflags, NOCRED);
			VOP_UNLOCK(devvp);
			goto fail;
		}

		ump = VFSTOUFS(mp);
		fs = ump->um_e2fs;
	} else {
		/*
		 * Update the mount.
		 */

		/*
		 * The initial mount got a reference on this
		 * device, so drop the one obtained via
		 * namei(), above.
		 */
		vrele(devvp);

		ump = VFSTOUFS(mp);
		fs = ump->um_e2fs;
		if (fs->e2fs_ronly == 0 && (mp->mnt_flag & MNT_RDONLY)) {
			/*
			 * Changing from r/w to r/o
			 */
			flags = WRITECLOSE;
			if (mp->mnt_flag & MNT_FORCE)
				flags |= FORCECLOSE;
			error = ext2fs_flushfiles(mp, flags);
			if (error == 0 &&
			    ext2fs_cgupdate(ump, MNT_WAIT) == 0 &&
			    (fs->e2fs.e2fs_state & E2FS_ERRORS) == 0) {
				fs->e2fs.e2fs_state = E2FS_ISCLEAN;
				(void) ext2fs_sbupdate(ump, MNT_WAIT);
			}
			if (error)
				return (error);
			fs->e2fs_ronly = 1;
		}

		if (mp->mnt_flag & MNT_RELOAD) {
			error = ext2fs_reload(mp, l->l_cred, l);
			if (error)
				return (error);
		}

		if (fs->e2fs_ronly && (mp->mnt_iflag & IMNT_WANTRDWR)) {
			/*
			 * Changing from read-only to read/write
			 */
			fs->e2fs_ronly = 0;
			if (fs->e2fs.e2fs_state == E2FS_ISCLEAN)
				fs->e2fs.e2fs_state = 0;
			else
				fs->e2fs.e2fs_state = E2FS_ERRORS;
			fs->e2fs_fmod = 1;
		}
		if (args->fspec == NULL)
			return 0;
	}

	error = set_statvfs_info(path, UIO_USERSPACE, args->fspec,
	    UIO_USERSPACE, mp->mnt_op->vfs_name, mp, l);
	(void) copystr(mp->mnt_stat.f_mntonname, fs->e2fs_fsmnt,
	    sizeof(fs->e2fs_fsmnt) - 1, &size);
	memset(fs->e2fs_fsmnt + size, 0, sizeof(fs->e2fs_fsmnt) - size);
	if (fs->e2fs.e2fs_rev > E2FS_REV0) {
		(void) copystr(mp->mnt_stat.f_mntonname, fs->e2fs.e2fs_fsmnt,
		    sizeof(fs->e2fs.e2fs_fsmnt) - 1, &size);
		memset(fs->e2fs.e2fs_fsmnt, 0,
		    sizeof(fs->e2fs.e2fs_fsmnt) - size);
	}
	if (fs->e2fs_fmod != 0) {	/* XXX */
		fs->e2fs_fmod = 0;
		if (fs->e2fs.e2fs_state == 0)
			fs->e2fs.e2fs_wtime = time_second;
		else
			printf("%s: file system not clean; please fsck(8)\n",
				mp->mnt_stat.f_mntfromname);
		(void) ext2fs_cgupdate(ump, MNT_WAIT);
	}
	return (error);

fail:
	vrele(devvp);
	return (error);
}

/*
 * Reload all incore data for a filesystem (used after running fsck on
 * the root filesystem and finding things to fix). The filesystem must
 * be mounted read-only.
 *
 * Things to do to update the mount:
 *	1) invalidate all cached meta-data.
 *	2) re-read superblock from disk.
 *	3) re-read summary information from disk.
 *	4) invalidate all inactive vnodes.
 *	5) invalidate all cached file data.
 *	6) re-read inode data for all active vnodes.
 */
int
ext2fs_reload(struct mount *mp, kauth_cred_t cred, struct lwp *l)
{
	struct vnode *vp, *devvp;
	struct inode *ip;
	struct buf *bp;
	struct m_ext2fs *fs;
	struct ext2fs *newfs;
	int i, error;
	void *cp;
	struct ufsmount *ump;
	struct vnode_iterator *marker;

	if ((mp->mnt_flag & MNT_RDONLY) == 0)
		return (EINVAL);

	ump = VFSTOUFS(mp);
	/*
	 * Step 1: invalidate all cached meta-data.
	 */
	devvp = ump->um_devvp;
	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
	error = vinvalbuf(devvp, 0, cred, l, 0, 0);
	VOP_UNLOCK(devvp);
	if (error)
		panic("ext2fs_reload: dirty1");

	fs = ump->um_e2fs;
	/*
	 * Step 2: re-read superblock from disk. Copy in new superblock, and compute
	 * in-memory values.
	 */
	error = bread(devvp, SBLOCK, SBSIZE, 0, &bp);
	if (error)
		return error;
	newfs = (struct ext2fs *)bp->b_data;
	e2fs_sbload(newfs, &fs->e2fs);

	brelse(bp, 0);

	error = ext2fs_sbfill(fs, (mp->mnt_flag & MNT_RDONLY) != 0);
	if (error)
		return error;

	/*
	 * Step 3: re-read summary information from disk.
	 */
	for (i = 0; i < fs->e2fs_ngdb; i++) {
		error = bread(devvp ,
		    EXT2_FSBTODB(fs, fs->e2fs.e2fs_first_dblock +
		    1 /* superblock */ + i),
		    fs->e2fs_bsize, 0, &bp);
		if (error) {
			return (error);
		}
		e2fs_cgload((struct ext2_gd *)bp->b_data,
		    &fs->e2fs_gd[i * fs->e2fs_bsize / sizeof(struct ext2_gd)],
		    fs->e2fs_bsize);
		brelse(bp, 0);
	}

	vfs_vnode_iterator_init(mp, &marker);
	while ((vp = vfs_vnode_iterator_next(marker, NULL, NULL))) {
		/*
		 * Step 4: invalidate all inactive vnodes.
		 */
		if (vrecycle(vp))
			continue;
		/*
		 * Step 5: invalidate all cached file data.
		 */
		if (vn_lock(vp, LK_EXCLUSIVE)) {
			vrele(vp);
			continue;
		}
		if (vinvalbuf(vp, 0, cred, l, 0, 0))
			panic("ext2fs_reload: dirty2");
		/*
		 * Step 6: re-read inode data for all active vnodes.
		 */
		ip = VTOI(vp);
		error = bread(devvp, EXT2_FSBTODB(fs, ino_to_fsba(fs, ip->i_number)),
		    (int)fs->e2fs_bsize, 0, &bp);
		if (error) {
			vput(vp);
			break;
		}
		cp = (char *)bp->b_data +
		    (ino_to_fsbo(fs, ip->i_number) * EXT2_DINODE_SIZE(fs));
		e2fs_iload((struct ext2fs_dinode *)cp, ip->i_din.e2fs_din);
		ext2fs_set_inode_guid(ip);
		brelse(bp, 0);
		vput(vp);
	}
	vfs_vnode_iterator_destroy(marker);
	return (error);
}

/*
 * Common code for mount and mountroot
 */
int
ext2fs_mountfs(struct vnode *devvp, struct mount *mp)
{
	struct lwp *l = curlwp;
	struct ufsmount *ump;
	struct buf *bp;
	struct ext2fs *fs;
	struct m_ext2fs *m_fs;
	dev_t dev;
	int error, i, ronly;
	kauth_cred_t cred;

	dev = devvp->v_rdev;
	cred = l->l_cred;

	/* Flush out any old buffers remaining from a previous use. */
	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
	error = vinvalbuf(devvp, V_SAVE, cred, l, 0, 0);
	VOP_UNLOCK(devvp);
	if (error)
		return (error);

	ronly = (mp->mnt_flag & MNT_RDONLY) != 0;

	bp = NULL;
	ump = NULL;

	/* Read the superblock from disk, and swap it directly. */
	error = bread(devvp, SBLOCK, SBSIZE, 0, &bp);
	if (error)
		goto out;
	fs = (struct ext2fs *)bp->b_data;
	m_fs = kmem_zalloc(sizeof(struct m_ext2fs), KM_SLEEP);
	e2fs_sbload(fs, &m_fs->e2fs);

	brelse(bp, 0);
	bp = NULL;

	/* Once swapped, validate and fill in the superblock. */
	error = ext2fs_sbfill(m_fs, ronly);
	if (error) {
		kmem_free(m_fs, sizeof(struct m_ext2fs));
		goto out;
	}
	m_fs->e2fs_ronly = ronly;

	ump = kmem_zalloc(sizeof(*ump), KM_SLEEP);
	ump->um_fstype = UFS1;
	ump->um_ops = &ext2fs_ufsops;
	ump->um_e2fs = m_fs;

	if (ronly == 0) {
		if (m_fs->e2fs.e2fs_state == E2FS_ISCLEAN)
			m_fs->e2fs.e2fs_state = 0;
		else
			m_fs->e2fs.e2fs_state = E2FS_ERRORS;
		m_fs->e2fs_fmod = 1;
	}

	/* XXX: should be added in ext2fs_sbfill()? */
	m_fs->e2fs_gd = kmem_alloc(m_fs->e2fs_ngdb * m_fs->e2fs_bsize, KM_SLEEP);
	for (i = 0; i < m_fs->e2fs_ngdb; i++) {
		error = bread(devvp,
		    EXT2_FSBTODB(m_fs, m_fs->e2fs.e2fs_first_dblock +
		    1 /* superblock */ + i),
		    m_fs->e2fs_bsize, 0, &bp);
		if (error) {
			kmem_free(m_fs->e2fs_gd,
			    m_fs->e2fs_ngdb * m_fs->e2fs_bsize);
			goto out;
		}
		e2fs_cgload((struct ext2_gd *)bp->b_data,
		    &m_fs->e2fs_gd[
			i * m_fs->e2fs_bsize / sizeof(struct ext2_gd)],
		    m_fs->e2fs_bsize);
		brelse(bp, 0);
		bp = NULL;
	}

	mp->mnt_data = ump;
	mp->mnt_stat.f_fsidx.__fsid_val[0] = (long)dev;
	mp->mnt_stat.f_fsidx.__fsid_val[1] = makefstype(MOUNT_EXT2FS);
	mp->mnt_stat.f_fsid = mp->mnt_stat.f_fsidx.__fsid_val[0];
	mp->mnt_stat.f_namemax = EXT2FS_MAXNAMLEN;
	mp->mnt_flag |= MNT_LOCAL;
	mp->mnt_dev_bshift = DEV_BSHIFT;	/* XXX */
	mp->mnt_fs_bshift = m_fs->e2fs_bshift;
	mp->mnt_iflag |= IMNT_DTYPE;
	ump->um_flags = 0;
	ump->um_mountp = mp;
	ump->um_dev = dev;
	ump->um_devvp = devvp;
	ump->um_nindir = EXT2_NINDIR(m_fs);
	ump->um_lognindir = ffs(EXT2_NINDIR(m_fs)) - 1;
	ump->um_bptrtodb = m_fs->e2fs_fsbtodb;
	ump->um_seqinc = 1; /* no frags */
	ump->um_maxsymlinklen = EXT2_MAXSYMLINKLEN;
	ump->um_dirblksiz = m_fs->e2fs_bsize;
	ump->um_maxfilesize = ((uint64_t)0x80000000 * m_fs->e2fs_bsize - 1);
	spec_node_setmountedfs(devvp, mp);
	return (0);

out:
	if (bp != NULL)
		brelse(bp, 0);
	if (ump) {
		kmem_free(ump->um_e2fs, sizeof(struct m_ext2fs));
		kmem_free(ump, sizeof(*ump));
		mp->mnt_data = NULL;
	}
	return (error);
}

/*
 * unmount system call
 */
int
ext2fs_unmount(struct mount *mp, int mntflags)
{
	struct ufsmount *ump;
	struct m_ext2fs *fs;
	int error, flags;

	flags = 0;
	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;
	if ((error = ext2fs_flushfiles(mp, flags)) != 0)
		return (error);
	ump = VFSTOUFS(mp);
	fs = ump->um_e2fs;
	if (fs->e2fs_ronly == 0 &&
		ext2fs_cgupdate(ump, MNT_WAIT) == 0 &&
		(fs->e2fs.e2fs_state & E2FS_ERRORS) == 0) {
		fs->e2fs.e2fs_state = E2FS_ISCLEAN;
		(void) ext2fs_sbupdate(ump, MNT_WAIT);
	}
	if (ump->um_devvp->v_type != VBAD)
		spec_node_setmountedfs(ump->um_devvp, NULL);
	vn_lock(ump->um_devvp, LK_EXCLUSIVE | LK_RETRY);
	error = VOP_CLOSE(ump->um_devvp, fs->e2fs_ronly ? FREAD : FREAD|FWRITE,
	    NOCRED);
	vput(ump->um_devvp);
	kmem_free(fs->e2fs_gd, fs->e2fs_ngdb * fs->e2fs_bsize);
	kmem_free(fs, sizeof(*fs));
	kmem_free(ump, sizeof(*ump));
	mp->mnt_data = NULL;
	mp->mnt_flag &= ~MNT_LOCAL;
	return (error);
}

/*
 * Flush out all the files in a filesystem.
 */
int
ext2fs_flushfiles(struct mount *mp, int flags)
{
	extern int doforce;
	int error;

	if (!doforce)
		flags &= ~FORCECLOSE;
	error = vflush(mp, NULLVP, flags);
	return (error);
}

/*
 * Get file system statistics.
 */
int
ext2fs_statvfs(struct mount *mp, struct statvfs *sbp)
{
	struct ufsmount *ump;
	struct m_ext2fs *fs;
	uint32_t overhead, overhead_per_group, ngdb;
	int i, ngroups;

	ump = VFSTOUFS(mp);
	fs = ump->um_e2fs;
	if (fs->e2fs.e2fs_magic != E2FS_MAGIC)
		panic("ext2fs_statvfs");

	/*
	 * Compute the overhead (FS structures)
	 */
	overhead_per_group =
	    1 /* block bitmap */ +
	    1 /* inode bitmap */ +
	    fs->e2fs_itpg;
	overhead = fs->e2fs.e2fs_first_dblock +
	    fs->e2fs_ncg * overhead_per_group;
	if (fs->e2fs.e2fs_rev > E2FS_REV0 &&
	    fs->e2fs.e2fs_features_rocompat & EXT2F_ROCOMPAT_SPARSESUPER) {
		for (i = 0, ngroups = 0; i < fs->e2fs_ncg; i++) {
			if (cg_has_sb(i))
				ngroups++;
		}
	} else {
		ngroups = fs->e2fs_ncg;
	}
	ngdb = fs->e2fs_ngdb;
	if (fs->e2fs.e2fs_rev > E2FS_REV0 &&
	    fs->e2fs.e2fs_features_compat & EXT2F_COMPAT_RESIZE)
		ngdb += fs->e2fs.e2fs_reserved_ngdb;
	overhead += ngroups * (1 /* superblock */ + ngdb);

	sbp->f_bsize = fs->e2fs_bsize;
	sbp->f_frsize = MINBSIZE << fs->e2fs.e2fs_fsize;
	sbp->f_iosize = fs->e2fs_bsize;
	sbp->f_blocks = fs->e2fs.e2fs_bcount - overhead;
	sbp->f_bfree = fs->e2fs.e2fs_fbcount;
	sbp->f_bresvd = fs->e2fs.e2fs_rbcount;
	if (sbp->f_bfree > sbp->f_bresvd)
		sbp->f_bavail = sbp->f_bfree - sbp->f_bresvd;
	else
		sbp->f_bavail = 0;
	sbp->f_files =  fs->e2fs.e2fs_icount;
	sbp->f_ffree = fs->e2fs.e2fs_ficount;
	sbp->f_favail = fs->e2fs.e2fs_ficount;
	sbp->f_fresvd = 0;
	copy_statvfs_info(sbp, mp);
	return (0);
}

static bool
ext2fs_sync_selector(void *cl, struct vnode *vp)
{
	struct inode *ip;

	ip = VTOI(vp);
	/*
	 * Skip the vnode/inode if inaccessible.
	 */
	if (ip == NULL || vp->v_type == VNON)
		return false;

	if (((ip->i_flag &
	      (IN_CHANGE | IN_UPDATE | IN_MODIFIED)) == 0 &&
	     LIST_EMPTY(&vp->v_dirtyblkhd) &&
	     UVM_OBJ_IS_CLEAN(&vp->v_uobj)))
		return false;
	return true;
}

/*
 * Go through the disk queues to initiate sandbagged IO;
 * go through the inodes to write those that have been modified;
 * initiate the writing of the super block if it has been modified.
 *
 * Note: we are always called with the filesystem marked `MPBUSY'.
 */
int
ext2fs_sync(struct mount *mp, int waitfor, kauth_cred_t cred)
{
	struct vnode *vp;
	struct ufsmount *ump = VFSTOUFS(mp);
	struct m_ext2fs *fs;
	struct vnode_iterator *marker;
	int error, allerror = 0;

	fs = ump->um_e2fs;
	if (fs->e2fs_fmod != 0 && fs->e2fs_ronly != 0) {	/* XXX */
		printf("fs = %s\n", fs->e2fs_fsmnt);
		panic("update: rofs mod");
	}

	/*
	 * Write back each (modified) inode.
	 */
	vfs_vnode_iterator_init(mp, &marker);
	while ((vp = vfs_vnode_iterator_next(marker, ext2fs_sync_selector,
	    NULL)))
	{
		error = vn_lock(vp, LK_EXCLUSIVE);
		if (error) {
			vrele(vp);
			continue;
		}
		if (vp->v_type == VREG && waitfor == MNT_LAZY)
			error = ext2fs_update(vp, NULL, NULL, 0);
		else
			error = VOP_FSYNC(vp, cred,
			    waitfor == MNT_WAIT ? FSYNC_WAIT : 0, 0, 0);
		if (error)
			allerror = error;
		vput(vp);
	}
	vfs_vnode_iterator_destroy(marker);
	/*
	 * Force stale file system control information to be flushed.
	 */
	if (waitfor != MNT_LAZY) {
		vn_lock(ump->um_devvp, LK_EXCLUSIVE | LK_RETRY);
		if ((error = VOP_FSYNC(ump->um_devvp, cred,
		    waitfor == MNT_WAIT ? FSYNC_WAIT : 0, 0, 0)) != 0)
			allerror = error;
		VOP_UNLOCK(ump->um_devvp);
	}
	/*
	 * Write back modified superblock.
	 */
	if (fs->e2fs_fmod != 0) {
		fs->e2fs_fmod = 0;
		fs->e2fs.e2fs_wtime = time_second;
		if ((error = ext2fs_cgupdate(ump, waitfor)))
			allerror = error;
	}
	return (allerror);
}

/*
 * Read an inode from disk and initialize this vnode / inode pair.
 * Caller assures no other thread will try to load this inode.
 */
int
ext2fs_loadvnode(struct mount *mp, struct vnode *vp,
    const void *key, size_t key_len, const void **new_key)
{
	ino_t ino;
	struct m_ext2fs *fs;
	struct inode *ip;
	struct ufsmount *ump;
	struct buf *bp;
	dev_t dev;
	int error;
	void *cp;

	KASSERT(key_len == sizeof(ino));
	memcpy(&ino, key, key_len);
	ump = VFSTOUFS(mp);
	dev = ump->um_dev;
	fs = ump->um_e2fs;

	/* Read in the disk contents for the inode, copy into the inode. */
	error = bread(ump->um_devvp, EXT2_FSBTODB(fs, ino_to_fsba(fs, ino)),
	    (int)fs->e2fs_bsize, 0, &bp);
	if (error)
		return error;

	/* Allocate and initialize inode. */
	ip = pool_get(&ext2fs_inode_pool, PR_WAITOK);
	memset(ip, 0, sizeof(struct inode));
	vp->v_tag = VT_EXT2FS;
	vp->v_op = ext2fs_vnodeop_p;
	vp->v_vflag |= VV_LOCKSWORK;
	vp->v_data = ip;
	ip->i_vnode = vp;
	ip->i_ump = ump;
	ip->i_e2fs = fs;
	ip->i_dev = dev;
	ip->i_number = ino;
	ip->i_e2fs_last_lblk = 0;
	ip->i_e2fs_last_blk = 0;

	/* Initialize genfs node. */
	genfs_node_init(vp, &ext2fs_genfsops);

	cp = (char *)bp->b_data + (ino_to_fsbo(fs, ino) * EXT2_DINODE_SIZE(fs));
	ip->i_din.e2fs_din = pool_get(&ext2fs_dinode_pool, PR_WAITOK);
	e2fs_iload((struct ext2fs_dinode *)cp, ip->i_din.e2fs_din);
	ext2fs_set_inode_guid(ip);
	brelse(bp, 0);

	/* If the inode was deleted, reset all fields */
	if (ip->i_e2fs_dtime != 0) {
		ip->i_e2fs_mode = 0;
		(void)ext2fs_setsize(ip, 0);
		(void)ext2fs_setnblock(ip, 0);
		memset(ip->i_e2fs_blocks, 0, sizeof(ip->i_e2fs_blocks));
	}

	/* Initialize the vnode from the inode. */
	ext2fs_vinit(mp, ext2fs_specop_p, ext2fs_fifoop_p, &vp);

	/* Finish inode initialization. */
	ip->i_devvp = ump->um_devvp;
	vref(ip->i_devvp);

	/*
	 * Set up a generation number for this inode if it does not
	 * already have one. This should only happen on old filesystems.
	 */

	if (ip->i_e2fs_gen == 0) {
		if (++ext2gennumber < (u_long)time_second)
			ext2gennumber = time_second;
		ip->i_e2fs_gen = ext2gennumber;
		if ((mp->mnt_flag & MNT_RDONLY) == 0)
			ip->i_flag |= IN_MODIFIED;
	}
	uvm_vnp_setsize(vp, ext2fs_size(ip));
	*new_key = &ip->i_number;
	return 0;
}

/*
 * File handle to vnode
 *
 * Have to be really careful about stale file handles:
 * - check that the inode number is valid
 * - call ext2fs_vget() to get the locked inode
 * - check for an unallocated inode (i_mode == 0)
 */
int
ext2fs_fhtovp(struct mount *mp, struct fid *fhp, struct vnode **vpp)
{
	struct inode *ip;
	struct vnode *nvp;
	int error;
	struct ufid ufh;
	struct m_ext2fs *fs;

	if (fhp->fid_len != sizeof(struct ufid))
		return EINVAL;

	memcpy(&ufh, fhp, sizeof(struct ufid));
	fs = VFSTOUFS(mp)->um_e2fs;
	if ((ufh.ufid_ino < EXT2_FIRSTINO && ufh.ufid_ino != EXT2_ROOTINO) ||
		ufh.ufid_ino >= fs->e2fs_ncg * fs->e2fs.e2fs_ipg)
		return (ESTALE);

	if ((error = VFS_VGET(mp, ufh.ufid_ino, &nvp)) != 0) {
		*vpp = NULLVP;
		return (error);
	}
	ip = VTOI(nvp);
	if (ip->i_e2fs_mode == 0 || ip->i_e2fs_dtime != 0 ||
		ip->i_e2fs_gen != ufh.ufid_gen) {
		vput(nvp);
		*vpp = NULLVP;
		return (ESTALE);
	}
	*vpp = nvp;
	return (0);
}

/*
 * Vnode pointer to File handle
 */
/* ARGSUSED */
int
ext2fs_vptofh(struct vnode *vp, struct fid *fhp, size_t *fh_size)
{
	struct inode *ip;
	struct ufid ufh;

	if (*fh_size < sizeof(struct ufid)) {
		*fh_size = sizeof(struct ufid);
		return E2BIG;
	}
	*fh_size = sizeof(struct ufid);

	ip = VTOI(vp);
	memset(&ufh, 0, sizeof(ufh));
	ufh.ufid_len = sizeof(struct ufid);
	ufh.ufid_ino = ip->i_number;
	ufh.ufid_gen = ip->i_e2fs_gen;
	memcpy(fhp, &ufh, sizeof(ufh));
	return (0);
}

/*
 * Write a superblock and associated information back to disk.
 */
int
ext2fs_sbupdate(struct ufsmount *mp, int waitfor)
{
	struct m_ext2fs *fs = mp->um_e2fs;
	struct buf *bp;
	int error = 0;

	bp = getblk(mp->um_devvp, SBLOCK, SBSIZE, 0, 0);
	e2fs_sbsave(&fs->e2fs, (struct ext2fs*)bp->b_data);
	if (waitfor == MNT_WAIT)
		error = bwrite(bp);
	else
		bawrite(bp);
	return (error);
}

int
ext2fs_cgupdate(struct ufsmount *mp, int waitfor)
{
	struct m_ext2fs *fs = mp->um_e2fs;
	struct buf *bp;
	int i, error = 0, allerror = 0;

	allerror = ext2fs_sbupdate(mp, waitfor);
	for (i = 0; i < fs->e2fs_ngdb; i++) {
		bp = getblk(mp->um_devvp, EXT2_FSBTODB(fs,
		    fs->e2fs.e2fs_first_dblock +
		    1 /* superblock */ + i), fs->e2fs_bsize, 0, 0);
		e2fs_cgsave(&fs->e2fs_gd[
		    i * fs->e2fs_bsize / sizeof(struct ext2_gd)],
		    (struct ext2_gd *)bp->b_data, fs->e2fs_bsize);
		if (waitfor == MNT_WAIT)
			error = bwrite(bp);
		else
			bawrite(bp);
	}

	if (!allerror && error)
		allerror = error;
	return (allerror);
}

/*
 * Fill in the m_fs structure, and validate the fields of the superblock.
 * NOTE: here, the superblock is already swapped.
 */
static int
ext2fs_sbfill(struct m_ext2fs *m_fs, int ronly)
{
	uint32_t u32;
	struct ext2fs *fs = &m_fs->e2fs;

	/*
	 * General sanity checks
	 */
	if (fs->e2fs_magic != E2FS_MAGIC)
		return EINVAL;
	if (fs->e2fs_rev > E2FS_REV1) {
		printf("ext2fs: unsupported revision number: %x\n", fs->e2fs_rev);
		return EINVAL;
	}
	if (fs->e2fs_log_bsize > 2) {
		/* block size = 1024|2048|4096 */
		printf("ext2fs: bad block size: %d\n", fs->e2fs_log_bsize);
		return EINVAL;
	}
	if (fs->e2fs_bpg == 0) {
		printf("ext2fs: zero blocks per group\n");
		return EINVAL;
	}
	if (fs->e2fs_ipg == 0) {
		printf("ext2fs: zero inodes per group\n");
		return EINVAL;
	}

	if (fs->e2fs_first_dblock >= fs->e2fs_bcount) {
		printf("ext2fs: invalid first data block\n");
		return EINVAL;
	}
	if (fs->e2fs_rbcount > fs->e2fs_bcount ||
	    fs->e2fs_fbcount > fs->e2fs_bcount) {
		printf("ext2fs: invalid block count\n");
		return EINVAL;
	}

	/*
	 * Revision-specific checks
	 */
	if (fs->e2fs_rev > E2FS_REV0) {
		char buf[256];
		if (fs->e2fs_first_ino != EXT2_FIRSTINO) {
			printf("ext2fs: unsupported first inode position\n");
			return EINVAL;
		}
		u32 = fs->e2fs_features_incompat & ~EXT2F_INCOMPAT_SUPP;
		if (u32) {
			snprintb(buf, sizeof(buf), EXT2F_INCOMPAT_BITS, u32);
			printf("ext2fs: unsupported incompat features: %s\n", buf);
			return EINVAL;
		}
		u32 = fs->e2fs_features_rocompat & ~EXT2F_ROCOMPAT_SUPP;
		if (!ronly && u32) {
			snprintb(buf, sizeof(buf), EXT2F_ROCOMPAT_BITS, u32);
			printf("ext2fs: unsupported ro-incompat features: %s\n",
			    buf);
			return EROFS;
		}
		if (fs->e2fs_inode_size == 0 || !powerof2(fs->e2fs_inode_size)) {
			printf("ext2fs: bad inode size\n");
			return EINVAL;
		}
	}

	/*
	 * Compute the fields of the superblock
	 */
	u32 = fs->e2fs_bcount - fs->e2fs_first_dblock; /* > 0 */
	m_fs->e2fs_ncg = howmany(u32, fs->e2fs_bpg);
	if (m_fs->e2fs_ncg == 0) {
		printf("ext2fs: invalid number of cylinder groups\n");
		return EINVAL;
	}

	m_fs->e2fs_fsbtodb = fs->e2fs_log_bsize + LOG_MINBSIZE - DEV_BSHIFT;
	m_fs->e2fs_bsize = MINBSIZE << fs->e2fs_log_bsize;
	m_fs->e2fs_bshift = LOG_MINBSIZE + fs->e2fs_log_bsize;
	m_fs->e2fs_qbmask = m_fs->e2fs_bsize - 1;
	m_fs->e2fs_bmask = ~m_fs->e2fs_qbmask;

	if ((u32 = m_fs->e2fs_bsize / sizeof(struct ext2_gd)) == 0) {
		/* Unlikely to happen */
		printf("ext2fs: invalid block size\n");
		return EINVAL;
	}
	m_fs->e2fs_ngdb = howmany(m_fs->e2fs_ncg, u32);
	if (m_fs->e2fs_ngdb == 0) {
		printf("ext2fs: invalid number of group descriptor blocks\n");
		return EINVAL;
	}

	if (m_fs->e2fs_bsize < EXT2_DINODE_SIZE(m_fs)) {
		printf("ext2fs: invalid inode size\n");
		return EINVAL;
	}
	m_fs->e2fs_ipb = m_fs->e2fs_bsize / EXT2_DINODE_SIZE(m_fs);

	m_fs->e2fs_itpg = fs->e2fs_ipg / m_fs->e2fs_ipb;

	return 0;
}
