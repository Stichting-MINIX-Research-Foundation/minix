/* $NetBSD: udf_vfsops.c,v 1.71 2015/08/24 08:31:56 hannken Exp $ */

/*
 * Copyright (c) 2006, 2008 Reinoud Zandijk
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
 */

#include <sys/cdefs.h>
#ifndef lint
__KERNEL_RCSID(0, "$NetBSD: udf_vfsops.c,v 1.71 2015/08/24 08:31:56 hannken Exp $");
#endif /* not lint */


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
#include <miscfs/specfs/specdev.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/file.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/dirent.h>
#include <sys/stat.h>
#include <sys/conf.h>
#include <sys/kauth.h>
#include <sys/module.h>

#include <fs/udf/ecma167-udf.h>
#include <fs/udf/udf_mount.h>
#include <sys/dirhash.h>

#include "udf.h"
#include "udf_subr.h"
#include "udf_bswap.h"

MODULE(MODULE_CLASS_VFS, udf, NULL);

#define VTOI(vnode) ((struct udf_node *) vnode->v_data)

/* verbose levels of the udf filingsystem */
int udf_verbose = UDF_DEBUGGING;

/* malloc regions */
MALLOC_JUSTDEFINE(M_UDFMNT,   "UDF mount",	"UDF mount structures");
MALLOC_JUSTDEFINE(M_UDFVOLD,  "UDF volspace",	"UDF volume space descriptors");
MALLOC_JUSTDEFINE(M_UDFTEMP,  "UDF temp",	"UDF scrap space");
struct pool udf_node_pool;

static struct sysctllog *udf_sysctl_log;

/* internal functions */
static int udf_mountfs(struct vnode *, struct mount *, struct lwp *, struct udf_args *);


/* --------------------------------------------------------------------- */

/* predefine vnode-op list descriptor */
extern const struct vnodeopv_desc udf_vnodeop_opv_desc;

const struct vnodeopv_desc * const udf_vnodeopv_descs[] = {
	&udf_vnodeop_opv_desc,
	NULL,
};


/* vfsops descriptor linked in as anchor point for the filingsystem */
struct vfsops udf_vfsops = {
	.vfs_name = MOUNT_UDF,
	.vfs_min_mount_data = sizeof (struct udf_args),
	.vfs_mount = udf_mount,
	.vfs_start = udf_start,
	.vfs_unmount = udf_unmount,
	.vfs_root = udf_root,
	.vfs_quotactl = (void *)eopnotsupp,
	.vfs_statvfs = udf_statvfs,
	.vfs_sync = udf_sync,
	.vfs_vget = udf_vget,
	.vfs_loadvnode = udf_loadvnode,
	.vfs_newvnode = udf_newvnode,
	.vfs_fhtovp = udf_fhtovp,
	.vfs_vptofh = udf_vptofh,
	.vfs_init = udf_init,
	.vfs_reinit = udf_reinit,
	.vfs_done = udf_done,
	.vfs_mountroot = udf_mountroot,
	.vfs_snapshot = udf_snapshot,
	.vfs_extattrctl = vfs_stdextattrctl,
	.vfs_suspendctl = (void *)eopnotsupp,
	.vfs_renamelock_enter = genfs_renamelock_enter,
	.vfs_renamelock_exit = genfs_renamelock_exit,
	.vfs_fsync = (void *)eopnotsupp,
	.vfs_opv_descs = udf_vnodeopv_descs
};

/* --------------------------------------------------------------------- */

/* file system starts here */
void
udf_init(void)
{
	size_t size;

	/* setup memory types */
	malloc_type_attach(M_UDFMNT);
	malloc_type_attach(M_UDFVOLD);
	malloc_type_attach(M_UDFTEMP);

	/* init node pools */
	size = sizeof(struct udf_node);
	pool_init(&udf_node_pool, size, 0, 0, 0,
		"udf_node_pool", NULL, IPL_NONE);
}


void
udf_reinit(void)
{
	/* nothing to do */
}


void
udf_done(void)
{
	/* remove pools */
	pool_destroy(&udf_node_pool);

	malloc_type_detach(M_UDFMNT);
	malloc_type_detach(M_UDFVOLD);
	malloc_type_detach(M_UDFTEMP);
}

/*
 * If running a DEBUG kernel, provide an easy way to set the debug flags when
 * running into a problem.
 */
#define UDF_VERBOSE_SYSCTLOPT        1

static int
udf_modcmd(modcmd_t cmd, void *arg)
{
	const struct sysctlnode *node;
	int error;

	switch (cmd) {
	case MODULE_CMD_INIT:
		error = vfs_attach(&udf_vfsops);
		if (error != 0)
			break;
		/*
		 * XXX the "24" below could be dynamic, thereby eliminating one
		 * more instance of the "number to vfs" mapping problem, but
		 * "24" is the order as taken from sys/mount.h
		 */
		sysctl_createv(&udf_sysctl_log, 0, NULL, &node,
			       CTLFLAG_PERMANENT,
			       CTLTYPE_NODE, "udf",
			       SYSCTL_DESCR("OSTA Universal File System"),
			       NULL, 0, NULL, 0,
			       CTL_VFS, 24, CTL_EOL);
#ifdef DEBUG
		sysctl_createv(&udf_sysctl_log, 0, NULL, &node,
			       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
			       CTLTYPE_INT, "verbose",
			       SYSCTL_DESCR("Bitmask for filesystem debugging"),
			       NULL, 0, &udf_verbose, 0,
			       CTL_VFS, 24, UDF_VERBOSE_SYSCTLOPT, CTL_EOL);
#endif
		break;
	case MODULE_CMD_FINI:
		error = vfs_detach(&udf_vfsops);
		if (error != 0)
			break;
		sysctl_teardown(&udf_sysctl_log);
		break;
	default:
		error = ENOTTY;
		break;
	}

	return (error);
}

/* --------------------------------------------------------------------- */

int
udf_mountroot(void)
{
	return EOPNOTSUPP;
}

/* --------------------------------------------------------------------- */

#define MPFREE(a, lst) \
	if ((a)) free((a), lst);
static void
free_udf_mountinfo(struct mount *mp)
{
	struct udf_mount *ump;
	int i;

	if (!mp)
		return;

	ump = VFSTOUDF(mp);
	if (ump) {
		/* clear our data */
		for (i = 0; i < UDF_ANCHORS; i++)
			MPFREE(ump->anchors[i], M_UDFVOLD);
		MPFREE(ump->primary_vol,      M_UDFVOLD);
		MPFREE(ump->logical_vol,      M_UDFVOLD);
		MPFREE(ump->unallocated,      M_UDFVOLD);
		MPFREE(ump->implementation,   M_UDFVOLD);
		MPFREE(ump->logvol_integrity, M_UDFVOLD);
		for (i = 0; i < UDF_PARTITIONS; i++) {
			MPFREE(ump->partitions[i],        M_UDFVOLD);
			MPFREE(ump->part_unalloc_dscr[i], M_UDFVOLD);
			MPFREE(ump->part_freed_dscr[i],   M_UDFVOLD);
		}
		MPFREE(ump->metadata_unalloc_dscr, M_UDFVOLD);

		MPFREE(ump->fileset_desc,   M_UDFVOLD);
		MPFREE(ump->sparing_table,  M_UDFVOLD);

		MPFREE(ump->la_node_ad_cpy, M_UDFMNT);
		MPFREE(ump->la_pmapping,    M_TEMP);
		MPFREE(ump->la_lmapping,    M_TEMP);

		mutex_destroy(&ump->logvol_mutex);
		mutex_destroy(&ump->allocate_mutex);
		mutex_destroy(&ump->sync_lock);

		MPFREE(ump->vat_table, M_UDFVOLD);

		free(ump, M_UDFMNT);
	}
}
#undef MPFREE

/* --------------------------------------------------------------------- */

/* if the system nodes exist, release them */
static void
udf_release_system_nodes(struct mount *mp)
{
	struct udf_mount *ump = VFSTOUDF(mp);
	int error;

	/* if we haven't even got an ump, dont bother */
	if (!ump)
		return;

	/* VAT partition support */
	if (ump->vat_node)
		vrele(ump->vat_node->vnode);

	/* Metadata partition support */
	if (ump->metadata_node)
		vrele(ump->metadata_node->vnode);
	if (ump->metadatamirror_node)
		vrele(ump->metadatamirror_node->vnode);
	if (ump->metadatabitmap_node)
		vrele(ump->metadatabitmap_node->vnode);

	/* This flush should NOT write anything nor allow any node to remain */
	if ((error = vflush(ump->vfs_mountp, NULLVP, 0)) != 0)
		panic("Failure to flush UDF system vnodes\n");
}


int
udf_mount(struct mount *mp, const char *path,
	  void *data, size_t *data_len)
{
	struct lwp *l = curlwp;
	struct udf_args *args = data;
	struct udf_mount *ump;
	struct vnode *devvp;
	int openflags, accessmode, error;

	DPRINTF(CALL, ("udf_mount called\n"));

	if (args == NULL)
		return EINVAL;
	if (*data_len < sizeof *args)
		return EINVAL;

	if (mp->mnt_flag & MNT_GETARGS) {
		/* request for the mount arguments */
		ump = VFSTOUDF(mp);
		if (ump == NULL)
			return EINVAL;
		*args = ump->mount_args;
		*data_len = sizeof *args;
		return 0;
	}

	/* handle request for updating mount parameters */
	/* TODO can't update my mountpoint yet */
	if (mp->mnt_flag & MNT_UPDATE) {
		return EOPNOTSUPP;
	}

	/* OK, so we are asked to mount the device */

	/* check/translate struct version */
	/* TODO sanity checking other mount arguments */
	if (args->version != 1) {
		printf("mount_udf: unrecognized argument structure version\n");
		return EINVAL;
	}

	/* lookup name to get its vnode */
	error = namei_simple_user(args->fspec,
				NSM_FOLLOW_NOEMULROOT, &devvp);
	if (error)
		return error;

#ifdef DEBUG
	if (udf_verbose & UDF_DEBUG_VOLUMES)
		vprint("UDF mount, trying to mount \n", devvp);
#endif

	/* check if its a block device specified */
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
	accessmode = VREAD;
	if ((mp->mnt_flag & MNT_RDONLY) == 0)
		accessmode |= VWRITE;
	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
	error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_MOUNT,
	    KAUTH_REQ_SYSTEM_MOUNT_DEVICE, mp, devvp, KAUTH_ARG(accessmode));
	VOP_UNLOCK(devvp);
	if (error) {
		vrele(devvp);
		return error;
	}

	/*
	 * Open device and try to mount it!
	 */
	if (mp->mnt_flag & MNT_RDONLY) {
		openflags = FREAD;
	} else {
		openflags = FREAD | FWRITE;
	}
	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
	error = VOP_OPEN(devvp, openflags, FSCRED);
	VOP_UNLOCK(devvp);
	if (error == 0) {
		/* opened ok, try mounting */
		error = udf_mountfs(devvp, mp, l, args);
		if (error) {
			udf_release_system_nodes(mp);
			/* cleanup */
			udf_discstrat_finish(VFSTOUDF(mp));
			free_udf_mountinfo(mp);
			vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
			(void) VOP_CLOSE(devvp, openflags, NOCRED);
			VOP_UNLOCK(devvp);
		}
	}
	if (error) {
		/* devvp is still locked */
		vrele(devvp);
		return error;
	}

	/* register our mountpoint being on this device */
	spec_node_setmountedfs(devvp, mp);

	/* successfully mounted */
	DPRINTF(VOLUMES, ("udf_mount() successfull\n"));

	error = set_statvfs_info(path, UIO_USERSPACE, args->fspec, UIO_USERSPACE,
			mp->mnt_op->vfs_name, mp, l);
	if (error)
		return error;

	/* If we're not opened read-only, open its logical volume */
	if ((mp->mnt_flag & MNT_RDONLY) == 0) {
		if ((error = udf_open_logvol(VFSTOUDF(mp))) != 0) {
			printf( "mount_udf: can't open logical volume for "
				"writing, downgrading access to read-only\n");
			mp->mnt_flag |= MNT_RDONLY;
			/* FIXME we can't return error now on open failure */
			return 0;
		}
	}

	return 0;
}

/* --------------------------------------------------------------------- */

#ifdef DEBUG
static bool
udf_sanity_selector(void *cl, struct vnode *vp)
{

	vprint("", vp);
	if (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) {
		printf("  is locked\n");
	}
	if (vp->v_usecount > 1)
		printf("  more than one usecount %d\n", vp->v_usecount);
	return false;
}

static void
udf_unmount_sanity_check(struct mount *mp)
{
	struct vnode_iterator *marker;

	printf("On unmount, i found the following nodes:\n");
	vfs_vnode_iterator_init(mp, &marker);
	vfs_vnode_iterator_next(marker, udf_sanity_selector, NULL);
	vfs_vnode_iterator_destroy(marker);
}
#endif


int
udf_unmount(struct mount *mp, int mntflags)
{
	struct udf_mount *ump;
	int error, flags, closeflags;

	DPRINTF(CALL, ("udf_umount called\n"));

	ump = VFSTOUDF(mp);
	if (!ump)
		panic("UDF unmount: empty ump\n");

	flags = (mntflags & MNT_FORCE) ? FORCECLOSE : 0;
	/* TODO remove these paranoid functions */
#ifdef DEBUG
	if (udf_verbose & UDF_DEBUG_LOCKING)
		udf_unmount_sanity_check(mp);
#endif

	/*
	 * By specifying SKIPSYSTEM we can skip vnodes marked with VV_SYSTEM.
	 * This hardly documented feature allows us to exempt certain files
	 * from being flushed.
	 */
	if ((error = vflush(mp, NULLVP, flags | SKIPSYSTEM)) != 0)
		return error;

	/* update nodes and wait for completion of writeout of system nodes */
	udf_sync(mp, FSYNC_WAIT, NOCRED);

#ifdef DEBUG
	if (udf_verbose & UDF_DEBUG_LOCKING)
		udf_unmount_sanity_check(mp);
#endif

	/* flush again, to check if we are still busy for something else */
	if ((error = vflush(ump->vfs_mountp, NULLVP, flags | SKIPSYSTEM)) != 0)
		return error;

	DPRINTF(VOLUMES, ("flush OK on unmount\n"));

	/* close logical volume and close session if requested */
	if ((error = udf_close_logvol(ump, mntflags)) != 0)
		return error;

#ifdef DEBUG
	DPRINTF(VOLUMES, ("FINAL sanity check\n"));
	if (udf_verbose & UDF_DEBUG_LOCKING)
		udf_unmount_sanity_check(mp);
#endif

	/* NOTE release system nodes should NOT write anything */
	udf_release_system_nodes(mp);

	/* finalise disc strategy */
	udf_discstrat_finish(ump);

	/* synchronise device caches */
	(void) udf_synchronise_caches(ump);

	/* close device */
	DPRINTF(VOLUMES, ("closing device\n"));
	if (mp->mnt_flag & MNT_RDONLY) {
		closeflags = FREAD;
	} else {
		closeflags = FREAD | FWRITE;
	}

	/* devvp is still locked by us */
	vn_lock(ump->devvp, LK_EXCLUSIVE | LK_RETRY);
	error = VOP_CLOSE(ump->devvp, closeflags, NOCRED);
	if (error)
		printf("Error during closure of device! error %d, "
		       "device might stay locked\n", error);
	DPRINTF(VOLUMES, ("device close ok\n"));

	/* clear our mount reference and release device node */
	spec_node_setmountedfs(ump->devvp, NULL);
	vput(ump->devvp);

	/* free our ump */
	free_udf_mountinfo(mp);

	/* free ump struct references */
	mp->mnt_data = NULL;
	mp->mnt_flag &= ~MNT_LOCAL;

	DPRINTF(VOLUMES, ("Fin unmount\n"));
	return error;
}

/* --------------------------------------------------------------------- */

/*
 * Helper function of udf_mount() that actually mounts the disc.
 */

static int
udf_mountfs(struct vnode *devvp, struct mount *mp,
	    struct lwp *l, struct udf_args *args)
{
	struct udf_mount     *ump;
	uint32_t sector_size, lb_size, bshift;
	uint32_t logvol_integrity;
	int    num_anchors, error;

	/* flush out any old buffers remaining from a previous use. */
	if ((error = vinvalbuf(devvp, V_SAVE, l->l_cred, l, 0, 0)))
		return error;

	/* setup basic mount information */
	mp->mnt_data = NULL;
	mp->mnt_stat.f_fsidx.__fsid_val[0] = (uint32_t) devvp->v_rdev;
	mp->mnt_stat.f_fsidx.__fsid_val[1] = makefstype(MOUNT_UDF);
	mp->mnt_stat.f_fsid = mp->mnt_stat.f_fsidx.__fsid_val[0];
	mp->mnt_stat.f_namemax = UDF_MAXNAMLEN;
	mp->mnt_flag |= MNT_LOCAL;
//	mp->mnt_iflag |= IMNT_MPSAFE;

	/* allocate udf part of mount structure; malloc always succeeds */
	ump = malloc(sizeof(struct udf_mount), M_UDFMNT, M_WAITOK | M_ZERO);

	/* init locks */
	mutex_init(&ump->logvol_mutex, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&ump->allocate_mutex, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&ump->sync_lock, MUTEX_DEFAULT, IPL_NONE);

	/* init rbtree for nodes, ordered by their icb address (long_ad) */
	udf_init_nodes_tree(ump);

	/* set up linkage */
	mp->mnt_data    = ump;
	ump->vfs_mountp = mp;

	/* set up arguments and device */
	ump->mount_args = *args;
	ump->devvp      = devvp;
	if ((error = udf_update_discinfo(ump))) {
		printf("UDF mount: error inspecting fs node\n");
		return error;
	}

	/* inspect sector size */
	sector_size = ump->discinfo.sector_size;
	bshift = 1;
	while ((1 << bshift) < sector_size)
		bshift++;
	if ((1 << bshift) != sector_size) {
		printf("UDF mount: "
		       "hit NetBSD implementation fence on sector size\n");
		return EIO;
	}

	/* temporary check to overcome sectorsize >= 8192 bytes panic */
	if (sector_size >= 8192) {
		printf("UDF mount: "
			"hit implementation limit, sectorsize to big\n");
		return EIO;
	}

	/*
	 * Inspect if we're asked to mount read-write on a non recordable or
	 * closed sequential disc.
	 */
	if ((mp->mnt_flag & MNT_RDONLY) == 0) {
		if ((ump->discinfo.mmc_cur & MMC_CAP_RECORDABLE) == 0) {
			printf("UDF mount: disc is not recordable\n");
			return EROFS;
		}
		if (ump->discinfo.mmc_cur & MMC_CAP_SEQUENTIAL) {
			if (ump->discinfo.disc_state == MMC_STATE_FULL) {
				printf("UDF mount: disc is not appendable\n");
				return EROFS;
			}

			/*
			 * TODO if the last session is closed check if there
			 * is enough space to open/close new session
			 */
		}
		/* double check if we're not mounting a pervious session RW */
		if (args->sessionnr != 0) {
			printf("UDF mount: updating a previous session "
				"not yet allowed\n");
			return EROFS;
		}
	}

	/* initialise bootstrap disc strategy */
	ump->strategy = &udf_strat_bootstrap;
	udf_discstrat_init(ump);

	/* read all anchors to get volume descriptor sequence */
	num_anchors = udf_read_anchors(ump);
	if (num_anchors == 0)
		return EINVAL;

	DPRINTF(VOLUMES, ("Read %d anchors on this disc, session %d\n",
	    num_anchors, args->sessionnr));

	/* read in volume descriptor sequence */
	if ((error = udf_read_vds_space(ump))) {
		printf("UDF mount: error reading volume space\n");
		return error;
	}

	/* close down bootstrap disc strategy */
	udf_discstrat_finish(ump);

	/* check consistency and completeness */
	if ((error = udf_process_vds(ump))) {
		printf( "UDF mount: disc not properly formatted"
			"(bad VDS)\n");
		return error;
	}

	/* switch to new disc strategy */
	KASSERT(ump->strategy != &udf_strat_bootstrap);
	udf_discstrat_init(ump);

	/* initialise late allocation administration space */
	ump->la_lmapping = malloc(sizeof(uint64_t) * UDF_MAX_MAPPINGS,
			M_TEMP, M_WAITOK);
	ump->la_pmapping = malloc(sizeof(uint64_t) * UDF_MAX_MAPPINGS,
			M_TEMP, M_WAITOK);

	/* setup node cleanup extents copy space */
	lb_size = udf_rw32(ump->logical_vol->lb_size);
	ump->la_node_ad_cpy = malloc(lb_size * UDF_MAX_ALLOC_EXTENTS,
		M_UDFMNT, M_WAITOK);
	memset(ump->la_node_ad_cpy, 0, lb_size * UDF_MAX_ALLOC_EXTENTS);

	/* setup rest of mount information */
	mp->mnt_data = ump;

	/* bshift is allways equal to disc sector size */
	mp->mnt_dev_bshift = bshift;
	mp->mnt_fs_bshift  = bshift;

	/* note that the mp info needs to be initialised for reading! */
	/* read vds support tables like VAT, sparable etc. */
	if ((error = udf_read_vds_tables(ump))) {
		printf( "UDF mount: error in format or damaged disc "
			"(VDS tables failing)\n");
		return error;
	}

	/* check if volume integrity is closed otherwise its dirty */
	logvol_integrity = udf_rw32(ump->logvol_integrity->integrity_type);
	if (logvol_integrity != UDF_INTEGRITY_CLOSED) {
		printf("UDF mount: file system is not clean; ");
		printf("please fsck(8)\n");
		return EPERM;
	}

	/* read root directory */
	if ((error = udf_read_rootdirs(ump))) {
		printf( "UDF mount: "
			"disc not properly formatted or damaged disc "
			"(rootdirs failing)\n");
		return error;
	}

	/* success! */
	return 0;
}

/* --------------------------------------------------------------------- */

int
udf_start(struct mount *mp, int flags)
{
	/* do we have to do something here? */
	return 0;
}

/* --------------------------------------------------------------------- */

int
udf_root(struct mount *mp, struct vnode **vpp)
{
	struct vnode *vp;
	struct long_ad *dir_loc;
	struct udf_mount *ump = VFSTOUDF(mp);
	struct udf_node *root_dir;
	int error;

	DPRINTF(CALL, ("udf_root called\n"));

	dir_loc = &ump->fileset_desc->rootdir_icb;
	error = udf_get_node(ump, dir_loc, &root_dir);

	if (!root_dir)
		error = ENOENT;
	if (error)
		return error;

	vp = root_dir->vnode;
	KASSERT(vp->v_vflag & VV_ROOT);

	*vpp = vp;
	return 0;
}

/* --------------------------------------------------------------------- */

int
udf_statvfs(struct mount *mp, struct statvfs *sbp)
{
	struct udf_mount *ump = VFSTOUDF(mp);
	struct logvol_int_desc *lvid;
	struct udf_logvol_info *impl;
	uint64_t freeblks, sizeblks;
	int num_part;

	DPRINTF(CALL, ("udf_statvfs called\n"));
	sbp->f_flag   = mp->mnt_flag;
	sbp->f_bsize  = ump->discinfo.sector_size;
	sbp->f_frsize = ump->discinfo.sector_size;
	sbp->f_iosize = ump->discinfo.sector_size;

	mutex_enter(&ump->allocate_mutex);

	udf_calc_freespace(ump, &sizeblks, &freeblks);

	sbp->f_blocks = sizeblks;
	sbp->f_bfree  = freeblks;
	sbp->f_files  = 0;

	lvid = ump->logvol_integrity;
	num_part = udf_rw32(lvid->num_part);
	impl = (struct udf_logvol_info *) (lvid->tables + 2*num_part);
	if (impl) {
		sbp->f_files  = udf_rw32(impl->num_files);
		sbp->f_files += udf_rw32(impl->num_directories);
	}

	/* XXX read only for now XXX */
	sbp->f_bavail = 0;
	sbp->f_bresvd = 0;

	/* tricky, next only aplies to ffs i think, so set to zero */
	sbp->f_ffree  = 0;
	sbp->f_favail = 0;
	sbp->f_fresvd = 0;

	mutex_exit(&ump->allocate_mutex);

	copy_statvfs_info(sbp, mp);
	return 0;
}

/* --------------------------------------------------------------------- */

/*
 * TODO what about writing out free space maps, lvid etc? only on `waitfor'
 * i.e. explicit syncing by the user?
 */

static int
udf_sync_writeout_system_files(struct udf_mount *ump, int clearflags)
{
	int error;

	/* XXX lock for VAT en bitmaps? */
	/* metadata nodes are written synchronous */
	DPRINTF(CALL, ("udf_sync: syncing metadata\n"));
	if (ump->lvclose & UDF_WRITE_VAT)
		udf_writeout_vat(ump);

	error = 0;
	if (ump->lvclose & UDF_WRITE_PART_BITMAPS) {
		/* writeout metadata spacetable if existing */
		error = udf_write_metadata_partition_spacetable(ump, MNT_WAIT);
		if (error)
			printf( "udf_writeout_system_files : "
				" writeout of metadata space bitmap failed\n");

		/* writeout partition spacetables */
		error = udf_write_physical_partition_spacetables(ump, MNT_WAIT);
		if (error)
			printf( "udf_writeout_system_files : "
				"writeout of space tables failed\n");
		if (!error && clearflags)
			ump->lvclose &= ~UDF_WRITE_PART_BITMAPS;
	}

	return error;
}


int
udf_sync(struct mount *mp, int waitfor, kauth_cred_t cred)
{
	struct udf_mount *ump = VFSTOUDF(mp);

	DPRINTF(CALL, ("udf_sync called\n"));
	/* if called when mounted readonly, just ignore */
	if (mp->mnt_flag & MNT_RDONLY)
		return 0;

	if (ump->syncing && !waitfor) {
		printf("UDF: skipping autosync\n");
		return 0;
	}

	/* get sync lock */
	ump->syncing = 1;

	/* pre-sync */
	udf_do_sync(ump, cred, waitfor);

	if (waitfor == MNT_WAIT)
		udf_sync_writeout_system_files(ump, true);

	DPRINTF(CALL, ("end of udf_sync()\n"));
	ump->syncing = 0;

	return 0;
}

/* --------------------------------------------------------------------- */

/*
 * Get vnode for the file system type specific file id ino for the fs. Its
 * used for reference to files by unique ID and for NFSv3.
 * (optional) TODO lookup why some sources state NFSv3
 */
int
udf_vget(struct mount *mp, ino_t ino,
    struct vnode **vpp)
{
	DPRINTF(NOTIMPL, ("udf_vget called\n"));
	return EOPNOTSUPP;
}

/* --------------------------------------------------------------------- */

/*
 * Lookup vnode for file handle specified
 */
int
udf_fhtovp(struct mount *mp, struct fid *fhp,
    struct vnode **vpp)
{
	DPRINTF(NOTIMPL, ("udf_fhtovp called\n"));
	return EOPNOTSUPP;
}

/* --------------------------------------------------------------------- */

/*
 * Create an unique file handle. Its structure is opaque and won't be used by
 * other subsystems. It should uniquely identify the file in the filingsystem
 * and enough information to know if a file has been removed and/or resources
 * have been recycled.
 */
int
udf_vptofh(struct vnode *vp, struct fid *fid,
    size_t *fh_size)
{
	DPRINTF(NOTIMPL, ("udf_vptofh called\n"));
	return EOPNOTSUPP;
}

/* --------------------------------------------------------------------- */

/*
 * Create a filingsystem snapshot at the specified timestamp. Could be
 * implemented by explicitly creating a new session or with spare room in the
 * integrity descriptor space
 */
int
udf_snapshot(struct mount *mp, struct vnode *vp,
    struct timespec *tm)
{
	DPRINTF(NOTIMPL, ("udf_snapshot called\n"));
	return EOPNOTSUPP;
}

/* --------------------------------------------------------------------- */
