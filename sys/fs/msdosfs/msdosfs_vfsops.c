/*	$NetBSD: msdosfs_vfsops.c,v 1.118 2015/03/28 19:24:05 maxv Exp $	*/

/*-
 * Copyright (C) 1994, 1995, 1997 Wolfgang Solfrank.
 * Copyright (C) 1994, 1995, 1997 TooLs GmbH.
 * All rights reserved.
 * Original code by Paul Popelka (paulp@uts.amdahl.com) (see below).
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Written by Paul Popelka (paulp@uts.amdahl.com)
 *
 * You can do anything you want with this software, just don't say you wrote
 * it, and don't remove this notice.
 *
 * This software is provided "as is".
 *
 * The author supplies this software to be publicly redistributed on the
 * understanding that the author is not responsible for the correct
 * functioning of this software in any circumstances and is not liable for
 * any damages caused by this software.
 *
 * October 1992
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: msdosfs_vfsops.c,v 1.118 2015/03/28 19:24:05 maxv Exp $");

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
#include <miscfs/genfs/genfs.h>
#include <miscfs/specfs/specdev.h> /* XXX */	/* defines v_rdev */
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/file.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/fstrans.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/dirent.h>
#include <sys/stat.h>
#include <sys/conf.h>
#include <sys/kauth.h>
#include <sys/module.h>

#include <fs/msdosfs/bpb.h>
#include <fs/msdosfs/bootsect.h>
#include <fs/msdosfs/direntry.h>
#include <fs/msdosfs/denode.h>
#include <fs/msdosfs/msdosfsmount.h>
#include <fs/msdosfs/fat.h>

MODULE(MODULE_CLASS_VFS, msdos, NULL);

#ifdef MSDOSFS_DEBUG
#define DPRINTF(fmt, ...) uprintf("%s(): " fmt "\n", __func__, ##__VA_ARGS__)
#else
#define DPRINTF(fmt, ...)
#endif

#define GEMDOSFS_BSIZE	512

#define MSDOSFS_NAMEMAX(pmp) \
	(pmp)->pm_flags & MSDOSFSMNT_LONGNAME ? WIN_MAXLEN : 12

int msdosfs_mountfs(struct vnode *, struct mount *, struct lwp *,
    struct msdosfs_args *);

static int update_mp(struct mount *, struct msdosfs_args *);

MALLOC_JUSTDEFINE(M_MSDOSFSMNT, "MSDOSFS mount", "MSDOS FS mount structure");
MALLOC_JUSTDEFINE(M_MSDOSFSFAT, "MSDOSFS FAT", "MSDOS FS FAT table");
MALLOC_JUSTDEFINE(M_MSDOSFSTMP, "MSDOSFS temp", "MSDOS FS temp. structures");

static struct sysctllog *msdosfs_sysctl_log;

extern const struct vnodeopv_desc msdosfs_vnodeop_opv_desc;

const struct vnodeopv_desc * const msdosfs_vnodeopv_descs[] = {
	&msdosfs_vnodeop_opv_desc,
	NULL,
};

struct vfsops msdosfs_vfsops = {
	.vfs_name = MOUNT_MSDOS,
	.vfs_min_mount_data = sizeof (struct msdosfs_args),
	.vfs_mount = msdosfs_mount,
	.vfs_start = msdosfs_start,
	.vfs_unmount = msdosfs_unmount,
	.vfs_root = msdosfs_root,
	.vfs_quotactl = (void *)eopnotsupp,
	.vfs_statvfs = msdosfs_statvfs,
	.vfs_sync = msdosfs_sync,
	.vfs_vget = msdosfs_vget,
	.vfs_loadvnode = msdosfs_loadvnode,
	.vfs_fhtovp = msdosfs_fhtovp,
	.vfs_vptofh = msdosfs_vptofh,
	.vfs_init = msdosfs_init,
	.vfs_reinit = msdosfs_reinit,
	.vfs_done = msdosfs_done,
	.vfs_mountroot = msdosfs_mountroot,
	.vfs_snapshot = (void *)eopnotsupp,
	.vfs_extattrctl = vfs_stdextattrctl,
	.vfs_suspendctl = msdosfs_suspendctl,
	.vfs_renamelock_enter = genfs_renamelock_enter,
	.vfs_renamelock_exit = genfs_renamelock_exit,
	.vfs_fsync = (void *)eopnotsupp,
	.vfs_opv_descs = msdosfs_vnodeopv_descs
};

static int
msdos_modcmd(modcmd_t cmd, void *arg)
{
	int error;

	switch (cmd) {
	case MODULE_CMD_INIT:
		error = vfs_attach(&msdosfs_vfsops);
		if (error != 0)
			break;
		sysctl_createv(&msdosfs_sysctl_log, 0, NULL, NULL,
			       CTLFLAG_PERMANENT,
			       CTLTYPE_NODE, "msdosfs",
			       SYSCTL_DESCR("MS-DOS file system"),
			       NULL, 0, NULL, 0,
			       CTL_VFS, 4, CTL_EOL);
		/*
		 * XXX the "4" above could be dynamic, thereby eliminating one
		 * more instance of the "number to vfs" mapping problem, but
		 * "4" is the order as taken from sys/mount.h
		 */
		break;
	case MODULE_CMD_FINI:
		error = vfs_detach(&msdosfs_vfsops);
		if (error != 0)
			break;
		sysctl_teardown(&msdosfs_sysctl_log);
		break;
	default:
		error = ENOTTY;
		break;
	}

	return (error);
}

static int
update_mp(struct mount *mp, struct msdosfs_args *argp)
{
	struct msdosfsmount *pmp = VFSTOMSDOSFS(mp);
	int error;

	pmp->pm_gid = argp->gid;
	pmp->pm_uid = argp->uid;
	pmp->pm_mask = argp->mask & ALLPERMS;
	pmp->pm_dirmask = argp->dirmask & ALLPERMS;
	pmp->pm_gmtoff = argp->gmtoff;
	pmp->pm_flags |= argp->flags & MSDOSFSMNT_MNTOPT;

	/*
	 * GEMDOS knows nothing about win95 long filenames
	 */
	if (pmp->pm_flags & MSDOSFSMNT_GEMDOSFS)
		pmp->pm_flags |= MSDOSFSMNT_NOWIN95;

	if (pmp->pm_flags & MSDOSFSMNT_NOWIN95)
		pmp->pm_flags |= MSDOSFSMNT_SHORTNAME;
	else if (!(pmp->pm_flags &
	    (MSDOSFSMNT_SHORTNAME | MSDOSFSMNT_LONGNAME))) {
		struct vnode *rtvp;

		/*
		 * Try to divine whether to support Win'95 long filenames
		 */
		if (FAT32(pmp))
			pmp->pm_flags |= MSDOSFSMNT_LONGNAME;
		else {
			if ((error = msdosfs_root(mp, &rtvp)) != 0)
				return error;
			pmp->pm_flags |= findwin95(VTODE(rtvp))
				? MSDOSFSMNT_LONGNAME
					: MSDOSFSMNT_SHORTNAME;
			vput(rtvp);
		}
	}

	mp->mnt_stat.f_namemax = MSDOSFS_NAMEMAX(pmp);

	return 0;
}

int
msdosfs_mountroot(void)
{
	struct mount *mp;
	struct lwp *l = curlwp;	/* XXX */
	int error;
	struct msdosfs_args args;

	if (device_class(root_device) != DV_DISK)
		return (ENODEV);

	if ((error = vfs_rootmountalloc(MOUNT_MSDOS, "root_device", &mp))) {
		vrele(rootvp);
		return (error);
	}

	args.flags = MSDOSFSMNT_VERSIONED;
	args.uid = 0;
	args.gid = 0;
	args.mask = 0777;
	args.version = MSDOSFSMNT_VERSION;
	args.dirmask = 0777;

	if ((error = msdosfs_mountfs(rootvp, mp, l, &args)) != 0) {
		vfs_unbusy(mp, false, NULL);
		vfs_destroy(mp);
		return (error);
	}

	if ((error = update_mp(mp, &args)) != 0) {
		(void)msdosfs_unmount(mp, 0);
		vfs_unbusy(mp, false, NULL);
		vfs_destroy(mp);
		vrele(rootvp);
		return (error);
	}

	mountlist_append(mp);
	(void)msdosfs_statvfs(mp, &mp->mnt_stat);
	vfs_unbusy(mp, false, NULL);
	return (0);
}

/*
 * mp - path - addr in user space of mount point (ie /usr or whatever)
 * data - addr in user space of mount params including the name of the block
 * special file to treat as a filesystem.
 */
int
msdosfs_mount(struct mount *mp, const char *path, void *data, size_t *data_len)
{
	struct lwp *l = curlwp;
	struct vnode *devvp;	  /* vnode for blk device to mount */
	struct msdosfs_args *args = data; /* holds data from mount request */
	/* msdosfs specific mount control block */
	struct msdosfsmount *pmp = NULL;
	int error, flags;
	mode_t accessmode;

	if (args == NULL)
		return EINVAL;
	if (*data_len < sizeof *args)
		return EINVAL;

	if (mp->mnt_flag & MNT_GETARGS) {
		pmp = VFSTOMSDOSFS(mp);
		if (pmp == NULL)
			return EIO;
		args->fspec = NULL;
		args->uid = pmp->pm_uid;
		args->gid = pmp->pm_gid;
		args->mask = pmp->pm_mask;
		args->flags = pmp->pm_flags;
		args->version = MSDOSFSMNT_VERSION;
		args->dirmask = pmp->pm_dirmask;
		args->gmtoff = pmp->pm_gmtoff;
		*data_len = sizeof *args;
		return 0;
	}

	/*
	 * If not versioned (i.e. using old mount_msdos(8)), fill in
	 * the additional structure items with suitable defaults.
	 */
	if ((args->flags & MSDOSFSMNT_VERSIONED) == 0) {
		args->version = 1;
		args->dirmask = args->mask;
	}

	/*
	 * Reset GMT offset for pre-v3 mount structure args.
	 */
	if (args->version < 3)
		args->gmtoff = 0;

	/*
	 * If updating, check whether changing from read-only to
	 * read/write; if there is no device name, that's all we do.
	 */
	if (mp->mnt_flag & MNT_UPDATE) {
		pmp = VFSTOMSDOSFS(mp);
		error = 0;
		if (!(pmp->pm_flags & MSDOSFSMNT_RONLY) &&
		    (mp->mnt_flag & MNT_RDONLY)) {
			flags = WRITECLOSE;
			if (mp->mnt_flag & MNT_FORCE)
				flags |= FORCECLOSE;
			error = vflush(mp, NULLVP, flags);
		}
		if (!error && (mp->mnt_flag & MNT_RELOAD))
			/* not yet implemented */
			error = EOPNOTSUPP;
		if (error) {
			DPRINTF("vflush %d", error);
			return (error);
		}
		if ((pmp->pm_flags & MSDOSFSMNT_RONLY) &&
		    (mp->mnt_iflag & IMNT_WANTRDWR)) {
			/*
			 * If upgrade to read-write by non-root, then verify
			 * that user has necessary permissions on the device.
			 *
			 * Permission to update a mount is checked higher, so
			 * here we presume updating the mount is okay (for
			 * example, as far as securelevel goes) which leaves us
			 * with the normal check.
			 */
			devvp = pmp->pm_devvp;
			vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
			error = kauth_authorize_system(l->l_cred,
			    KAUTH_SYSTEM_MOUNT, KAUTH_REQ_SYSTEM_MOUNT_DEVICE,
			    mp, devvp, KAUTH_ARG(VREAD | VWRITE));
			VOP_UNLOCK(devvp);
			DPRINTF("KAUTH_REQ_SYSTEM_MOUNT_DEVICE %d", error);
			if (error)
				return (error);

			pmp->pm_flags &= ~MSDOSFSMNT_RONLY;
		}
		if (args->fspec == NULL) {
			DPRINTF("missing fspec");
			return EINVAL;
		}
	}
	/*
	 * Not an update, or updating the name: look up the name
	 * and verify that it refers to a sensible block device.
	 */
	error = namei_simple_user(args->fspec,
				NSM_FOLLOW_NOEMULROOT, &devvp);
	if (error != 0) {
		DPRINTF("namei %d", error);
		return (error);
	}

	if (devvp->v_type != VBLK) {
		DPRINTF("not block");
		vrele(devvp);
		return (ENOTBLK);
	}
	if (bdevsw_lookup(devvp->v_rdev) == NULL) {
		DPRINTF("no block switch");
		vrele(devvp);
		return (ENXIO);
	}
	/*
	 * If mount by non-root, then verify that user has necessary
	 * permissions on the device.
	 */
	accessmode = VREAD;
	if ((mp->mnt_flag & MNT_RDONLY) == 0)
		accessmode |= VWRITE;
	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
	error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_MOUNT,
	    KAUTH_REQ_SYSTEM_MOUNT_DEVICE, mp, devvp, KAUTH_ARG(accessmode));
	VOP_UNLOCK(devvp);
	if (error) {
		DPRINTF("KAUTH_REQ_SYSTEM_MOUNT_DEVICE %d", error);
		vrele(devvp);
		return (error);
	}
	if ((mp->mnt_flag & MNT_UPDATE) == 0) {
		int xflags;

		if (mp->mnt_flag & MNT_RDONLY)
			xflags = FREAD;
		else
			xflags = FREAD|FWRITE;
		vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
		error = VOP_OPEN(devvp, xflags, FSCRED);
		VOP_UNLOCK(devvp);
		if (error) {
			DPRINTF("VOP_OPEN %d", error);
			goto fail;
		}
		error = msdosfs_mountfs(devvp, mp, l, args);
		if (error) {
			DPRINTF("msdosfs_mountfs %d", error);
			vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
			(void) VOP_CLOSE(devvp, xflags, NOCRED);
			VOP_UNLOCK(devvp);
			goto fail;
		}
#ifdef MSDOSFS_DEBUG		/* only needed for the printf below */
		pmp = VFSTOMSDOSFS(mp);
#endif
	} else {
		vrele(devvp);
		if (devvp != pmp->pm_devvp) {
			DPRINTF("devvp %p pmp %p", devvp, pmp->pm_devvp);
			return (EINVAL);	/* needs translation */
		}
	}
	if ((error = update_mp(mp, args)) != 0) {
		msdosfs_unmount(mp, MNT_FORCE);
		DPRINTF("update_mp %d", error);
		return error;
	}

#ifdef MSDOSFS_DEBUG
	printf("msdosfs_mount(): mp %p, pmp %p, inusemap %p\n", mp, pmp, pmp->pm_inusemap);
#endif
	return set_statvfs_info(path, UIO_USERSPACE, args->fspec, UIO_USERSPACE,
	    mp->mnt_op->vfs_name, mp, l);

fail:
	vrele(devvp);
	return (error);
}

int
msdosfs_mountfs(struct vnode *devvp, struct mount *mp, struct lwp *l, struct msdosfs_args *argp)
{
	struct msdosfsmount *pmp;
	struct buf *bp;
	dev_t dev = devvp->v_rdev;
	union bootsector *bsp;
	struct byte_bpb33 *b33;
	struct byte_bpb50 *b50;
	struct byte_bpb710 *b710;
	uint8_t SecPerClust;
	int	ronly, error, BlkPerSec;
	uint64_t psize;
	unsigned secsize;

	/* Flush out any old buffers remaining from a previous use. */
	if ((error = vinvalbuf(devvp, V_SAVE, l->l_cred, l, 0, 0)) != 0)
		return (error);

	ronly = (mp->mnt_flag & MNT_RDONLY) != 0;

	bp  = NULL; /* both used in error_exit */
	pmp = NULL;

	error = fstrans_mount(mp);
	if (error)
		goto error_exit;

	error = getdisksize(devvp, &psize, &secsize);
	if (error) {
		if (argp->flags & MSDOSFSMNT_GEMDOSFS)
			goto error_exit;

		/* ok, so it failed.  we most likely don't need the info */
		secsize = DEV_BSIZE;
		psize = 0;
		error = 0;
	}
	if (secsize < DEV_BSIZE) {
		DPRINTF("Invalid block secsize (%d < DEV_BSIZE)", secsize);
		error = EINVAL;
		goto error_exit;
	}

	if (argp->flags & MSDOSFSMNT_GEMDOSFS) {
		if (secsize != GEMDOSFS_BSIZE) {
			DPRINTF("Invalid block secsize %d for GEMDOS", secsize);
			error = EINVAL;
			goto error_exit;
		}
	}

	/*
	 * Read the boot sector of the filesystem, and then check the
	 * boot signature.  If not a dos boot sector then error out.
	 */
	if (secsize < sizeof(*b50)) {
		DPRINTF("50 bootsec %u\n", secsize);
		error = EINVAL;
		goto error_exit;
	}
	if ((error = bread(devvp, 0, secsize, 0, &bp)) != 0)
		goto error_exit;
	bsp = (union bootsector *)bp->b_data;
	b33 = (struct byte_bpb33 *)bsp->bs33.bsBPB;
	b50 = (struct byte_bpb50 *)bsp->bs50.bsBPB;
	b710 = (struct byte_bpb710 *)bsp->bs710.bsBPB;

	if (!(argp->flags & MSDOSFSMNT_GEMDOSFS)) {
		if (bsp->bs50.bsBootSectSig0 != BOOTSIG0
		    || bsp->bs50.bsBootSectSig1 != BOOTSIG1) {
			DPRINTF("bootsig0 %d bootsig1 %d", 
			    bsp->bs50.bsBootSectSig0,
			    bsp->bs50.bsBootSectSig1);
			error = EINVAL;
			goto error_exit;
		}
	}

	pmp = malloc(sizeof(*pmp), M_MSDOSFSMNT, M_WAITOK|M_ZERO);
	pmp->pm_mountp = mp;

	/*
	 * Compute several useful quantities from the bpb in the
	 * bootsector.  Copy in the dos 5 variant of the bpb then fix up
	 * the fields that are different between dos 5 and dos 3.3.
	 */
	SecPerClust = b50->bpbSecPerClust;
	pmp->pm_BytesPerSec = getushort(b50->bpbBytesPerSec);
	pmp->pm_ResSectors = getushort(b50->bpbResSectors);
	pmp->pm_FATs = b50->bpbFATs;
	pmp->pm_RootDirEnts = getushort(b50->bpbRootDirEnts);
	pmp->pm_Sectors = getushort(b50->bpbSectors);
	pmp->pm_FATsecs = getushort(b50->bpbFATsecs);
	pmp->pm_SecPerTrack = getushort(b50->bpbSecPerTrack);
	pmp->pm_Heads = getushort(b50->bpbHeads);
	pmp->pm_Media = b50->bpbMedia;

	if (pmp->pm_Sectors == 0) {
		pmp->pm_HiddenSects = getulong(b50->bpbHiddenSecs);
		pmp->pm_HugeSectors = getulong(b50->bpbHugeSectors);
	} else {
		if (secsize < sizeof(*b33)) {
			DPRINTF("33 bootsec %u\n", secsize);
			error = EINVAL;
			goto error_exit;
		}
		pmp->pm_HiddenSects = getushort(b33->bpbHiddenSecs);
		pmp->pm_HugeSectors = pmp->pm_Sectors;
	}

	/*
	 * Sanity checks, from the FAT specification:
	 * - sectors per cluster: >= 1, power of 2
	 * - logical sector size: >= 1, power of 2
	 * - cluster size:        <= max FS block size
	 * - number of sectors:   >= 1
	 */
	if ((SecPerClust == 0) || !powerof2(SecPerClust) ||
	    (pmp->pm_BytesPerSec == 0) || !powerof2(pmp->pm_BytesPerSec) ||
	    (SecPerClust * pmp->pm_BytesPerSec > MAXBSIZE) ||
	    (pmp->pm_HugeSectors == 0)) {
		DPRINTF("consistency checks");
		error = EINVAL;
		goto error_exit;
	}

	if (!(argp->flags & MSDOSFSMNT_GEMDOSFS) &&
	    (pmp->pm_SecPerTrack > 63)) {
		DPRINTF("SecPerTrack %d", pmp->pm_SecPerTrack);
		error = EINVAL;
		goto error_exit;
	}

	if (pmp->pm_RootDirEnts == 0) {
		if (secsize < sizeof(*b710)) {
			DPRINTF("710 bootsec %u\n", secsize);
			error = EINVAL;
			goto error_exit;
		}
		unsigned short FSVers = getushort(b710->bpbFSVers);
		unsigned short ExtFlags = getushort(b710->bpbExtFlags);
		/*
		 * Some say that bsBootSectSig[23] must be zero, but
		 * Windows does not require this and some digital cameras
		 * do not set these to zero.  Therefore, do not insist.
		 */
		if (pmp->pm_Sectors || pmp->pm_FATsecs || FSVers) {
			DPRINTF("Sectors %d FATsecs %lu FSVers %d",
			    pmp->pm_Sectors, pmp->pm_FATsecs, FSVers);
			error = EINVAL;
			goto error_exit;
		}
		pmp->pm_fatmask = FAT32_MASK;
		pmp->pm_fatmult = 4;
		pmp->pm_fatdiv = 1;
		pmp->pm_FATsecs = getulong(b710->bpbBigFATsecs);

		/* Mirroring is enabled if the FATMIRROR bit is not set. */
		if ((ExtFlags & FATMIRROR) == 0)
			pmp->pm_flags |= MSDOSFS_FATMIRROR;
		else
			pmp->pm_curfat = ExtFlags & FATNUM;
	} else
		pmp->pm_flags |= MSDOSFS_FATMIRROR;

	if (argp->flags & MSDOSFSMNT_GEMDOSFS) {
		if (FAT32(pmp)) {
			/* GEMDOS doesn't know FAT32. */
			DPRINTF("FAT32 for GEMDOS");
			error = EINVAL;
			goto error_exit;
		}

		/*
		 * Check a few values (could do some more):
		 * - logical sector size: >= block size
		 * - number of sectors:   <= size of partition
		 */
		if ((pmp->pm_BytesPerSec < GEMDOSFS_BSIZE) ||
		    (pmp->pm_HugeSectors *
		     (pmp->pm_BytesPerSec / GEMDOSFS_BSIZE) > psize)) {
			DPRINTF("consistency checks for GEMDOS");
			error = EINVAL;
			goto error_exit;
		}
		/*
		 * XXX - Many parts of the msdosfs driver seem to assume that
		 * the number of bytes per logical sector (BytesPerSec) will
		 * always be the same as the number of bytes per disk block
		 * Let's pretend it is.
		 */
		BlkPerSec = pmp->pm_BytesPerSec / GEMDOSFS_BSIZE;
		pmp->pm_BytesPerSec  = GEMDOSFS_BSIZE;
		pmp->pm_HugeSectors *= BlkPerSec;
		pmp->pm_HiddenSects *= BlkPerSec;
		pmp->pm_ResSectors  *= BlkPerSec;
		pmp->pm_Sectors     *= BlkPerSec;
		pmp->pm_FATsecs     *= BlkPerSec;
		SecPerClust         *= BlkPerSec;
	}

	/* Check that fs has nonzero FAT size */
	if (pmp->pm_FATsecs == 0) {
		DPRINTF("FATsecs is 0");
		error = EINVAL;
		goto error_exit;
	}

	pmp->pm_fatblk = pmp->pm_ResSectors;
	if (FAT32(pmp)) {
		if (secsize < sizeof(*b710)) {
			DPRINTF("710 bootsec %u\n", secsize);
			error = EINVAL;
			goto error_exit;
		}
		pmp->pm_rootdirblk = getulong(b710->bpbRootClust);
		pmp->pm_firstcluster = pmp->pm_fatblk
			+ (pmp->pm_FATs * pmp->pm_FATsecs);
		pmp->pm_fsinfo = getushort(b710->bpbFSInfo);
	} else {
		pmp->pm_rootdirblk = pmp->pm_fatblk +
			(pmp->pm_FATs * pmp->pm_FATsecs);
		pmp->pm_rootdirsize = (pmp->pm_RootDirEnts * sizeof(struct direntry)
				       + pmp->pm_BytesPerSec - 1)
			/ pmp->pm_BytesPerSec;/* in sectors */
		pmp->pm_firstcluster = pmp->pm_rootdirblk + pmp->pm_rootdirsize;
	}

	pmp->pm_nmbrofclusters = (pmp->pm_HugeSectors - pmp->pm_firstcluster) /
	    SecPerClust;
	pmp->pm_maxcluster = pmp->pm_nmbrofclusters + 1;
	pmp->pm_fatsize = pmp->pm_FATsecs * pmp->pm_BytesPerSec;

	if (argp->flags & MSDOSFSMNT_GEMDOSFS) {
		if (pmp->pm_nmbrofclusters <= (0xff0 - 2)) {
			pmp->pm_fatmask = FAT12_MASK;
			pmp->pm_fatmult = 3;
			pmp->pm_fatdiv = 2;
		} else {
			pmp->pm_fatmask = FAT16_MASK;
			pmp->pm_fatmult = 2;
			pmp->pm_fatdiv = 1;
		}
	} else if (pmp->pm_fatmask == 0) {
		if (pmp->pm_maxcluster
		    <= ((CLUST_RSRVD - CLUST_FIRST) & FAT12_MASK)) {
			/*
			 * This will usually be a floppy disk. This size makes
			 * sure that one FAT entry will not be split across
			 * multiple blocks.
			 */
			pmp->pm_fatmask = FAT12_MASK;
			pmp->pm_fatmult = 3;
			pmp->pm_fatdiv = 2;
		} else {
			pmp->pm_fatmask = FAT16_MASK;
			pmp->pm_fatmult = 2;
			pmp->pm_fatdiv = 1;
		}
	}
	if (FAT12(pmp))
		pmp->pm_fatblocksize = 3 * pmp->pm_BytesPerSec;
	else
		pmp->pm_fatblocksize = MAXBSIZE;

	pmp->pm_fatblocksec = pmp->pm_fatblocksize / pmp->pm_BytesPerSec;
	pmp->pm_bnshift = ffs(pmp->pm_BytesPerSec) - 1;

	/*
	 * Compute mask and shift value for isolating cluster relative byte
	 * offsets and cluster numbers from a file offset.
	 */
	pmp->pm_bpcluster = SecPerClust * pmp->pm_BytesPerSec;
	pmp->pm_crbomask = pmp->pm_bpcluster - 1;
	pmp->pm_cnshift = ffs(pmp->pm_bpcluster) - 1;

	/*
	 * Check for valid cluster size
	 * must be a power of 2
	 */
	if (pmp->pm_bpcluster ^ (1 << pmp->pm_cnshift)) {
		DPRINTF("bpcluster %lu cnshift %lu", pmp->pm_bpcluster,
		    pmp->pm_cnshift);
		error = EINVAL;
		goto error_exit;
	}

	/*
	 * Cluster size must be within limit of MAXBSIZE.
	 * Many FAT filesystems will not have clusters larger than
	 * 32KiB due to limits in Windows versions before Vista.
	 */
	if (pmp->pm_bpcluster > MAXBSIZE) {
		DPRINTF("bpcluster %lu > MAXBSIZE %d",
		    pmp->pm_bpcluster, MAXBSIZE);
		error = EINVAL;
		goto error_exit;
	}

	/*
	 * Release the bootsector buffer.
	 */
	brelse(bp, BC_AGE);
	bp = NULL;

	/*
	 * Check FSInfo.
	 */
	if (pmp->pm_fsinfo) {
		struct fsinfo *fp;
		const int rdsz = roundup(sizeof(*fp), pmp->pm_BytesPerSec);

		/*
		 * XXX	If the fsinfo block is stored on media with
		 *	2KB or larger sectors, is the fsinfo structure
		 *	padded at the end or in the middle?
		 */
		if ((error = bread(devvp, de_bn2kb(pmp, pmp->pm_fsinfo),
		    rdsz, 0, &bp)) != 0)
			goto error_exit;
		fp = (struct fsinfo *)bp->b_data;
		if (!memcmp(fp->fsisig1, "RRaA", 4)
		    && !memcmp(fp->fsisig2, "rrAa", 4)
		    && !memcmp(fp->fsisig3, "\0\0\125\252", 4)
		    && !memcmp(fp->fsisig4, "\0\0\125\252", 4))
			pmp->pm_nxtfree = getulong(fp->fsinxtfree);
		else
			pmp->pm_fsinfo = 0;
		brelse(bp, 0);
		bp = NULL;
	}

	/*
	 * Check and validate (or perhaps invalidate?) the fsinfo structure?
	 * XXX
	 */
	if (pmp->pm_fsinfo) {
		if ((pmp->pm_nxtfree == 0xffffffffUL) ||
		    (pmp->pm_nxtfree > pmp->pm_maxcluster))
			pmp->pm_fsinfo = 0;
	}

	/*
	 * Allocate memory for the bitmap of allocated clusters, and then
	 * fill it in.
	 */
	pmp->pm_inusemap = malloc(((pmp->pm_maxcluster + N_INUSEBITS)
				   / N_INUSEBITS)
				  * sizeof(*pmp->pm_inusemap),
				  M_MSDOSFSFAT, M_WAITOK);

	/*
	 * fillinusemap() needs pm_devvp.
	 */
	pmp->pm_dev = dev;
	pmp->pm_devvp = devvp;

	/*
	 * Have the inuse map filled in.
	 */
	if ((error = fillinusemap(pmp)) != 0) {
		DPRINTF("fillinusemap %d", error);
		goto error_exit;
	}

	/*
	 * If they want FAT updates to be synchronous then let them suffer
	 * the performance degradation in exchange for the on disk copy of
	 * the FAT being correct just about all the time.  I suppose this
	 * would be a good thing to turn on if the kernel is still flakey.
	 */
	if (mp->mnt_flag & MNT_SYNCHRONOUS)
		pmp->pm_flags |= MSDOSFSMNT_WAITONFAT;

	/*
	 * Finish up.
	 */
	if (ronly)
		pmp->pm_flags |= MSDOSFSMNT_RONLY;
	else
		pmp->pm_fmod = 1;
	mp->mnt_data = pmp;
	mp->mnt_stat.f_fsidx.__fsid_val[0] = (long)dev;
	mp->mnt_stat.f_fsidx.__fsid_val[1] = makefstype(MOUNT_MSDOS);
	mp->mnt_stat.f_fsid = mp->mnt_stat.f_fsidx.__fsid_val[0];
	mp->mnt_stat.f_namemax = MSDOSFS_NAMEMAX(pmp);
	mp->mnt_flag |= MNT_LOCAL;
	mp->mnt_dev_bshift = pmp->pm_bnshift;
	mp->mnt_fs_bshift = pmp->pm_cnshift;

	/*
	 * If we ever do quotas for DOS filesystems this would be a place
	 * to fill in the info in the msdosfsmount structure. You dolt,
	 * quotas on dos filesystems make no sense because files have no
	 * owners on dos filesystems. of course there is some empty space
	 * in the directory entry where we could put uid's and gid's.
	 */

	spec_node_setmountedfs(devvp, mp);

	return (0);

error_exit:
	fstrans_unmount(mp);
	if (bp)
		brelse(bp, BC_AGE);
	if (pmp) {
		if (pmp->pm_inusemap)
			free(pmp->pm_inusemap, M_MSDOSFSFAT);
		free(pmp, M_MSDOSFSMNT);
		mp->mnt_data = NULL;
	}
	return (error);
}

int
msdosfs_start(struct mount *mp, int flags)
{

	return (0);
}

/*
 * Unmount the filesystem described by mp.
 */
int
msdosfs_unmount(struct mount *mp, int mntflags)
{
	struct msdosfsmount *pmp;
	int error, flags;

	flags = 0;
	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;
	if ((error = vflush(mp, NULLVP, flags)) != 0)
		return (error);
	pmp = VFSTOMSDOSFS(mp);
	if (pmp->pm_devvp->v_type != VBAD)
		spec_node_setmountedfs(pmp->pm_devvp, NULL);
#ifdef MSDOSFS_DEBUG
	{
		struct vnode *vp = pmp->pm_devvp;

		printf("msdosfs_umount(): just before calling VOP_CLOSE()\n");
		printf("flag %08x, usecount %d, writecount %d, holdcnt %d\n",
		    vp->v_vflag | vp->v_iflag | vp->v_uflag, vp->v_usecount,
		    vp->v_writecount, vp->v_holdcnt);
		printf("mount %p, op %p\n",
		    vp->v_mount, vp->v_op);
		printf("freef %p, freeb %p, mount %p\n",
		    vp->v_freelist.tqe_next, vp->v_freelist.tqe_prev,
		    vp->v_mount);
		printf("cleanblkhd %p, dirtyblkhd %p, numoutput %d, type %d\n",
		    vp->v_cleanblkhd.lh_first,
		    vp->v_dirtyblkhd.lh_first,
		    vp->v_numoutput, vp->v_type);
		printf("union %p, tag %d, data[0] %08x, data[1] %08x\n",
		    vp->v_socket, vp->v_tag,
		    ((u_int *)vp->v_data)[0],
		    ((u_int *)vp->v_data)[1]);
	}
#endif
	vn_lock(pmp->pm_devvp, LK_EXCLUSIVE | LK_RETRY);
	(void) VOP_CLOSE(pmp->pm_devvp,
	    pmp->pm_flags & MSDOSFSMNT_RONLY ? FREAD : FREAD|FWRITE, NOCRED);
	vput(pmp->pm_devvp);
	msdosfs_fh_destroy(pmp);
	free(pmp->pm_inusemap, M_MSDOSFSFAT);
	free(pmp, M_MSDOSFSMNT);
	mp->mnt_data = NULL;
	mp->mnt_flag &= ~MNT_LOCAL;
	fstrans_unmount(mp);
	return (0);
}

int
msdosfs_root(struct mount *mp, struct vnode **vpp)
{
	struct msdosfsmount *pmp = VFSTOMSDOSFS(mp);
	int error;

#ifdef MSDOSFS_DEBUG
	printf("msdosfs_root(); mp %p, pmp %p\n", mp, pmp);
#endif
	if ((error = deget(pmp, MSDOSFSROOT, MSDOSFSROOT_OFS, vpp)) != 0)
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
msdosfs_statvfs(struct mount *mp, struct statvfs *sbp)
{
	struct msdosfsmount *pmp;

	pmp = VFSTOMSDOSFS(mp);
	sbp->f_bsize = pmp->pm_bpcluster;
	sbp->f_frsize = sbp->f_bsize;
	sbp->f_iosize = pmp->pm_bpcluster;
	sbp->f_blocks = pmp->pm_nmbrofclusters;
	sbp->f_bfree = pmp->pm_freeclustercount;
	sbp->f_bavail = pmp->pm_freeclustercount;
	sbp->f_bresvd = 0;
	sbp->f_files = pmp->pm_RootDirEnts;			/* XXX */
	sbp->f_ffree = 0;	/* what to put in here? */
	sbp->f_favail = 0;	/* what to put in here? */
	sbp->f_fresvd = 0;
	copy_statvfs_info(sbp, mp);
	return (0);
}

struct msdosfs_sync_ctx {
	int waitfor;
};

static bool
msdosfs_sync_selector(void *cl, struct vnode *vp)
{
	struct msdosfs_sync_ctx *c = cl;
	struct denode *dep;

	dep = VTODE(vp);
	if (c->waitfor == MNT_LAZY || vp->v_type == VNON ||
	    dep == NULL || (((dep->de_flag &
	    (DE_ACCESS | DE_CREATE | DE_UPDATE | DE_MODIFIED)) == 0) &&
	     (LIST_EMPTY(&vp->v_dirtyblkhd) &&
	      UVM_OBJ_IS_CLEAN(&vp->v_uobj))))
		return false;
	return true;
}

int
msdosfs_sync(struct mount *mp, int waitfor, kauth_cred_t cred)
{
	struct vnode *vp;
	struct vnode_iterator *marker;
	struct msdosfsmount *pmp = VFSTOMSDOSFS(mp);
	int error, allerror = 0;
	struct msdosfs_sync_ctx ctx;

	/*
	 * If we ever switch to not updating all of the FATs all the time,
	 * this would be the place to update them from the first one.
	 */
	if (pmp->pm_fmod != 0) {
		if (pmp->pm_flags & MSDOSFSMNT_RONLY)
			panic("msdosfs_sync: rofs mod");
		else {
			/* update FATs here */
		}
	}
	fstrans_start(mp, FSTRANS_SHARED);
	/*
	 * Write back each (modified) denode.
	 */
	vfs_vnode_iterator_init(mp, &marker);
	ctx.waitfor = waitfor;
	while ((vp = vfs_vnode_iterator_next(marker, msdosfs_sync_selector,
	    &ctx)))
	{
		error = vn_lock(vp, LK_EXCLUSIVE);
		if (error) {
			vrele(vp);
			continue;
		}
		if ((error = VOP_FSYNC(vp, cred,
		    waitfor == MNT_WAIT ? FSYNC_WAIT : 0, 0, 0)) != 0)
			allerror = error;
		vput(vp);
	}
	vfs_vnode_iterator_destroy(marker);

	/*
	 * Force stale file system control information to be flushed.
	 */
	if ((error = VOP_FSYNC(pmp->pm_devvp, cred,
	    waitfor == MNT_WAIT ? FSYNC_WAIT : 0, 0, 0)) != 0)
		allerror = error;
	fstrans_done(mp);
	return (allerror);
}

int
msdosfs_fhtovp(struct mount *mp, struct fid *fhp, struct vnode **vpp)
{
	struct msdosfsmount *pmp = VFSTOMSDOSFS(mp);
	struct defid defh;
	uint32_t gen;
	int error;

	if (fhp->fid_len != sizeof(struct defid)) {
		DPRINTF("fid_len %d %zd", fhp->fid_len, sizeof(struct defid));
		return EINVAL;
	}
	memcpy(&defh, fhp, sizeof(defh));
	error = msdosfs_fh_lookup(pmp, defh.defid_dirclust, defh.defid_dirofs,
	    &gen);
	if (error == 0 && gen != defh.defid_gen)
		error = ESTALE;
	if (error) {
		*vpp = NULLVP;
		return error;
	}
	error = deget(pmp, defh.defid_dirclust, defh.defid_dirofs, vpp);
	if (error) {
		DPRINTF("deget %d", error);
		*vpp = NULLVP;
		return error;
	}
	error = vn_lock(*vpp, LK_EXCLUSIVE);
	if (error) {
		vrele(*vpp);
		*vpp = NULLVP;
		return error;
	}
	return 0;
}

int
msdosfs_vptofh(struct vnode *vp, struct fid *fhp, size_t *fh_size)
{
	struct msdosfsmount *pmp = VFSTOMSDOSFS(vp->v_mount);
	struct denode *dep;
	struct defid defh;
	int error;

	if (*fh_size < sizeof(struct defid)) {
		*fh_size = sizeof(struct defid);
		return E2BIG;
	}
	*fh_size = sizeof(struct defid);
	dep = VTODE(vp);
	memset(&defh, 0, sizeof(defh));
	defh.defid_len = sizeof(struct defid);
	defh.defid_dirclust = dep->de_dirclust;
	defh.defid_dirofs = dep->de_diroffset;
	error = msdosfs_fh_enter(pmp, dep->de_dirclust, dep->de_diroffset,
	     &defh.defid_gen);
	if (error == 0)
		memcpy(fhp, &defh, sizeof(defh));
	return error;
}

int
msdosfs_vget(struct mount *mp, ino_t ino,
    struct vnode **vpp)
{

	return (EOPNOTSUPP);
}

int
msdosfs_suspendctl(struct mount *mp, int cmd)
{
	int error;
	struct lwp *l = curlwp;

	switch (cmd) {
	case SUSPEND_SUSPEND:
		if ((error = fstrans_setstate(mp, FSTRANS_SUSPENDING)) != 0)
			return error;
		error = msdosfs_sync(mp, MNT_WAIT, l->l_proc->p_cred);
		if (error == 0)
			error = fstrans_setstate(mp, FSTRANS_SUSPENDED);
		if (error != 0) {
			(void) fstrans_setstate(mp, FSTRANS_NORMAL);
			return error;
		}
		return 0;

	case SUSPEND_RESUME:
		return fstrans_setstate(mp, FSTRANS_NORMAL);

	default:
		return EINVAL;
	}
}
