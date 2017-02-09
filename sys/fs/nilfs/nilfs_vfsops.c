/* $NetBSD: nilfs_vfsops.c,v 1.21 2015/03/28 19:24:05 maxv Exp $ */

/*
 * Copyright (c) 2008, 2009 Reinoud Zandijk
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
__KERNEL_RCSID(0, "$NetBSD: nilfs_vfsops.c,v 1.21 2015/03/28 19:24:05 maxv Exp $");
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

#include <fs/nilfs/nilfs_mount.h>
#include <sys/dirhash.h>


#include "nilfs.h"
#include "nilfs_subr.h"
#include "nilfs_bswap.h"

MODULE(MODULE_CLASS_VFS, nilfs, NULL);

#define VTOI(vnode) ((struct nilfs_node *) vnode->v_data)

/* verbose levels of the nilfs filingsystem */
int nilfs_verbose = NILFS_DEBUGGING;

/* malloc regions */
MALLOC_JUSTDEFINE(M_NILFSMNT,   "NILFS mount",	"NILFS mount structures");
MALLOC_JUSTDEFINE(M_NILFSTEMP,  "NILFS temp",	"NILFS scrap space");
struct pool nilfs_node_pool;

/* globals */
struct _nilfs_devices nilfs_devices;
static struct sysctllog *nilfs_sysctl_log;

/* supported functions predefined */
VFS_PROTOS(nilfs);


/* --------------------------------------------------------------------- */

/*
 * Genfs interfacing
 *
 * static const struct genfs_ops nilfs_genfsops = {
 * 	.gop_size = genfs_size,
 * 		size of transfers
 * 	.gop_alloc = nilfs_gop_alloc,
 * 		allocate len bytes at offset
 * 	.gop_write = genfs_gop_write,
 * 		putpages interface code
 * 	.gop_markupdate = nilfs_gop_markupdate,
 * 		set update/modify flags etc.
 * }
 */

/*
 * Callback from genfs to allocate len bytes at offset off; only called when
 * filling up gaps in the allocation.
 */
static int
nilfs_gop_alloc(struct vnode *vp, off_t off,
    off_t len, int flags, kauth_cred_t cred)
{
	DPRINTF(NOTIMPL, ("nilfs_gop_alloc not implemented\n"));
	DPRINTF(ALLOC, ("nilfs_gop_alloc called for %"PRIu64" bytes\n", len));

	return 0;
}


/*
 * callback from genfs to update our flags
 */
static void
nilfs_gop_markupdate(struct vnode *vp, int flags)
{
	struct nilfs_node *nilfs_node = VTOI(vp);
	u_long mask = 0;

	if ((flags & GOP_UPDATE_ACCESSED) != 0) {
		mask = IN_ACCESS;
	}
	if ((flags & GOP_UPDATE_MODIFIED) != 0) {
		if (vp->v_type == VREG) {
			mask |= IN_CHANGE | IN_UPDATE;
		} else {
			mask |= IN_MODIFY;
		}
	}
	if (mask) {
		nilfs_node->i_flags |= mask;
	}
}


static const struct genfs_ops nilfs_genfsops = {
	.gop_size = genfs_size,
	.gop_alloc = nilfs_gop_alloc,
	.gop_write = genfs_gop_write_rwmap,
	.gop_markupdate = nilfs_gop_markupdate,
};

/* --------------------------------------------------------------------- */

/* predefine vnode-op list descriptor */
extern const struct vnodeopv_desc nilfs_vnodeop_opv_desc;

const struct vnodeopv_desc * const nilfs_vnodeopv_descs[] = {
	&nilfs_vnodeop_opv_desc,
	NULL,
};


/* vfsops descriptor linked in as anchor point for the filingsystem */
struct vfsops nilfs_vfsops = {
	.vfs_name = MOUNT_NILFS,
	.vfs_min_mount_data = sizeof (struct nilfs_args),
	.vfs_mount = nilfs_mount,
	.vfs_start = nilfs_start,
	.vfs_unmount = nilfs_unmount,
	.vfs_root = nilfs_root,
	.vfs_quotactl = (void *)eopnotsupp,
	.vfs_statvfs = nilfs_statvfs,
	.vfs_sync = nilfs_sync,
	.vfs_vget = nilfs_vget,
	.vfs_loadvnode = nilfs_loadvnode,
	.vfs_fhtovp = nilfs_fhtovp,
	.vfs_vptofh = nilfs_vptofh,
	.vfs_init = nilfs_init,
	.vfs_reinit = nilfs_reinit,
	.vfs_done = nilfs_done,
	.vfs_mountroot = nilfs_mountroot,
	.vfs_snapshot = nilfs_snapshot,
	.vfs_extattrctl = vfs_stdextattrctl,
	.vfs_suspendctl = (void *)eopnotsupp,
	.vfs_renamelock_enter = genfs_renamelock_enter,
	.vfs_renamelock_exit = genfs_renamelock_exit,
	.vfs_fsync = (void *)eopnotsupp,
	.vfs_opv_descs = nilfs_vnodeopv_descs
};

/* --------------------------------------------------------------------- */

/* file system starts here */
void
nilfs_init(void)
{
	size_t size;

	/* setup memory types */
	malloc_type_attach(M_NILFSMNT);
	malloc_type_attach(M_NILFSTEMP);

	/* init device lists */
	SLIST_INIT(&nilfs_devices);

	/* init node pools */
	size = sizeof(struct nilfs_node);
	pool_init(&nilfs_node_pool, size, 0, 0, 0,
		"nilfs_node_pool", NULL, IPL_NONE);
}


void
nilfs_reinit(void)
{
	/* nothing to do */
}


void
nilfs_done(void)
{
	/* remove pools */
	pool_destroy(&nilfs_node_pool);

	malloc_type_detach(M_NILFSMNT);
	malloc_type_detach(M_NILFSTEMP);
}

/*
 * If running a DEBUG kernel, provide an easy way to set the debug flags when
 * running into a problem.
 */
#define NILFS_VERBOSE_SYSCTLOPT        1

static int
nilfs_modcmd(modcmd_t cmd, void *arg)
{
	const struct sysctlnode *node;
	int error;

	switch (cmd) {
	case MODULE_CMD_INIT:
		error = vfs_attach(&nilfs_vfsops);
		if (error != 0)
			break;
		/*
		 * XXX the "30" below could be dynamic, thereby eliminating one
		 * more instance of the "number to vfs" mapping problem, but
		 * "30" is the order as taken from sys/mount.h
		 */
		sysctl_createv(&nilfs_sysctl_log, 0, NULL, &node,
			       CTLFLAG_PERMANENT,
			       CTLTYPE_NODE, "nilfs",
			       SYSCTL_DESCR("NTT's NILFSv2"),
			       NULL, 0, NULL, 0,
			       CTL_VFS, 30, CTL_EOL);
#ifdef DEBUG
		sysctl_createv(&nilfs_sysctl_log, 0, NULL, &node,
			       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
			       CTLTYPE_INT, "verbose",
			       SYSCTL_DESCR("Bitmask for filesystem debugging"),
			       NULL, 0, &nilfs_verbose, 0,
			       CTL_VFS, 30, NILFS_VERBOSE_SYSCTLOPT, CTL_EOL);
#endif
		break;
	case MODULE_CMD_FINI:
		error = vfs_detach(&nilfs_vfsops);
		if (error != 0)
			break;
		sysctl_teardown(&nilfs_sysctl_log);
		break;
	default:
		error = ENOTTY;
		break;
	}

	return (error);
}

/* --------------------------------------------------------------------- */

int
nilfs_mountroot(void)
{
	return EOPNOTSUPP;
}

/* --------------------------------------------------------------------- */

/* system nodes */
static int
nilfs_create_system_nodes(struct nilfs_device *nilfsdev)
{
	int error;

	error = nilfs_get_node_raw(nilfsdev, NULL, NILFS_DAT_INO,
		&nilfsdev->super_root.sr_dat, &nilfsdev->dat_node);
	if (error)
		goto errorout;

	error = nilfs_get_node_raw(nilfsdev, NULL, NILFS_CPFILE_INO,
		&nilfsdev->super_root.sr_cpfile, &nilfsdev->cp_node);
	if (error)
		goto errorout;

	error = nilfs_get_node_raw(nilfsdev, NULL, NILFS_SUFILE_INO,
		&nilfsdev->super_root.sr_sufile, &nilfsdev->su_node);
	if (error)
		goto errorout;

	return 0;
errorout:
	nilfs_dispose_node(&nilfsdev->dat_node);
	nilfs_dispose_node(&nilfsdev->cp_node);
	nilfs_dispose_node(&nilfsdev->su_node);

	return error;
}


static void
nilfs_release_system_nodes(struct nilfs_device *nilfsdev)
{
	if (!nilfsdev)
		return;
	if (nilfsdev->refcnt > 0)
		return;

	nilfs_dispose_node(&nilfsdev->dat_node);
	nilfs_dispose_node(&nilfsdev->cp_node);
	nilfs_dispose_node(&nilfsdev->su_node);
}


/* --------------------------------------------------------------------- */

static int
nilfs_check_superblock_crc(struct nilfs_super_block *super)
{
	uint32_t super_crc, comp_crc;

	/* check super block magic */
	if (nilfs_rw16(super->s_magic) != NILFS_SUPER_MAGIC)
		return 0;

	/* preserve crc */
	super_crc  = nilfs_rw32(super->s_sum);

	/* calculate */
	super->s_sum = 0;
	comp_crc = crc32_le(nilfs_rw32(super->s_crc_seed),
		(uint8_t *) super, nilfs_rw16(super->s_bytes));

	/* restore */
	super->s_sum = nilfs_rw32(super_crc);

	/* check CRC */
	return (super_crc == comp_crc);
}



static int
nilfs_read_superblock(struct nilfs_device *nilfsdev)
{
	struct nilfs_super_block *super, tmp_super;
	struct buf *bp;
	uint64_t sb1off, sb2off;
	uint64_t last_cno1, last_cno2;
	uint64_t dev_blk;
	int dev_bsize, dev_blks;
	int sb1ok, sb2ok, swp;
	int error;

	sb1off = NILFS_SB_OFFSET_BYTES;
	sb2off = NILFS_SB2_OFFSET_BYTES(nilfsdev->devsize);

	dev_bsize = 1 << nilfsdev->devvp->v_mount->mnt_fs_bshift;

	/* read our superblock regardless of backing device blocksize */
	dev_blk   = 0;
	dev_blks  = (sb1off + dev_bsize -1)/dev_bsize;
	error = bread(nilfsdev->devvp, dev_blk, dev_blks * dev_bsize, 0, &bp);
	if (error) {
		return error;
	}

	/* copy read-in super block at the offset */
	super = &nilfsdev->super;
	memcpy(super, (uint8_t *) bp->b_data + NILFS_SB_OFFSET_BYTES,
		sizeof(struct nilfs_super_block));
	brelse(bp, BC_AGE);

	/* read our 2nd superblock regardless of backing device blocksize */
	dev_blk   = sb2off / dev_bsize;
	dev_blks  = 2;		/* assumption max one dev_bsize */
	error = bread(nilfsdev->devvp, dev_blk, dev_blks * dev_bsize, 0, &bp);
	if (error) {
		return error;
	}

	/* copy read-in superblock2 at the offset */
	super = &nilfsdev->super2;
	memcpy(super, (uint8_t *) bp->b_data + NILFS_SB_OFFSET_BYTES,
		sizeof(struct nilfs_super_block));
	brelse(bp, BC_AGE);

	sb1ok = nilfs_check_superblock_crc(&nilfsdev->super);
	sb2ok = nilfs_check_superblock_crc(&nilfsdev->super2);

	last_cno1 = nilfs_rw64(nilfsdev->super.s_last_cno);
	last_cno2 = nilfs_rw64(nilfsdev->super2.s_last_cno);
	swp = sb2ok && (last_cno2 > last_cno1);

	if (swp) {
		printf("nilfs warning: broken superblock, using spare\n");
		tmp_super = nilfsdev->super2;
		nilfsdev->super2 = nilfsdev->super;	/* why preserve? */
		nilfsdev->super  = tmp_super;
	}

	if (!sb1ok && !sb2ok) {
		printf("nilfs: no valid superblocks found\n");
		return EINVAL;
	}

	return 0;
}


/* XXX NOTHING from the system nodes should need to be written here */
static void
nilfs_unmount_base(struct nilfs_device *nilfsdev)
{
	int error __diagused;

	if (!nilfsdev)
		return;

	/* remove all our information */
	error = vinvalbuf(nilfsdev->devvp, 0, FSCRED, curlwp, 0, 0);
	KASSERT(error == 0);

	/* release the device's system nodes */
	nilfs_release_system_nodes(nilfsdev);

	/* TODO writeout super_block? */
}


static int
nilfs_mount_base(struct nilfs_device *nilfsdev,
		struct mount *mp, struct nilfs_args *args)
{
	struct lwp *l = curlwp;
	uint64_t last_pseg, last_cno, last_seq;
	uint32_t log_blocksize;
	int error;

	/* flush out any old buffers remaining from a previous use. */
	if ((error = vinvalbuf(nilfsdev->devvp, V_SAVE, l->l_cred, l, 0, 0)))
		return error;

	/* read in our superblock */
	error = nilfs_read_superblock(nilfsdev);
	if (error) {
		printf("nilfs_mount: can't read in super block : %d\n", error);
		return error;
	}

	/* get our blocksize */
	log_blocksize = nilfs_rw32(nilfsdev->super.s_log_block_size);
	nilfsdev->blocksize   = (uint64_t) 1 << (log_blocksize + 10);
	/* TODO check superblock's blocksize limits */

	/* calculate dat structure parameters */
	nilfs_calc_mdt_consts(nilfsdev, &nilfsdev->dat_mdt,
			nilfs_rw16(nilfsdev->super.s_dat_entry_size));
	nilfs_calc_mdt_consts(nilfsdev, &nilfsdev->ifile_mdt,
			nilfs_rw16(nilfsdev->super.s_inode_size));

	DPRINTF(VOLUMES, ("nilfs_mount: accepted super block\n"));

	/* search for the super root and roll forward when needed */
	nilfs_search_super_root(nilfsdev);

	nilfsdev->mount_state = nilfs_rw16(nilfsdev->super.s_state);
	if (nilfsdev->mount_state != NILFS_VALID_FS) {
		printf("FS is seriously damaged, needs repairing\n");
		printf("aborting mount\n");
		return EINVAL;
	}

	/*
	 * FS should be ok now. The superblock and the last segsum could be
	 * updated from the repair so extract running values again.
	 */
	last_pseg = nilfs_rw64(nilfsdev->super.s_last_pseg); /*blknr */
	last_cno  = nilfs_rw64(nilfsdev->super.s_last_cno);
	last_seq  = nilfs_rw64(nilfsdev->super.s_last_seq);

	nilfsdev->last_seg_seq = last_seq;
	nilfsdev->last_seg_num = nilfs_get_segnum_of_block(nilfsdev, last_pseg);
	nilfsdev->next_seg_num = nilfs_get_segnum_of_block(nilfsdev,
		nilfs_rw64(nilfsdev->last_segsum.ss_next));
	nilfsdev->last_cno     = last_cno;

	DPRINTF(VOLUMES, ("nilfs_mount: accepted super root\n"));

	/* create system vnodes for DAT, CP and SEGSUM */
	error = nilfs_create_system_nodes(nilfsdev);
	if (error)
		nilfs_unmount_base(nilfsdev);
	return error;
}


static void
nilfs_unmount_device(struct nilfs_device *nilfsdev)
{
	int error;

	/* is there anything? */
	if (nilfsdev == NULL)
		return;

	/* remove the device only if we're the last reference */
	nilfsdev->refcnt--;
	if (nilfsdev->refcnt >= 1)
		return;

	/* unmount our base */
	nilfs_unmount_base(nilfsdev);

	/* remove from our device list */
	SLIST_REMOVE(&nilfs_devices, nilfsdev, nilfs_device, next_device);

	/* close device */
	DPRINTF(VOLUMES, ("closing device\n"));

	/* remove our mount reference before closing device */
	spec_node_setmountedfs(nilfsdev->devvp, NULL);

	/* devvp is still locked by us */
	vn_lock(nilfsdev->devvp, LK_EXCLUSIVE | LK_RETRY);
	error = VOP_CLOSE(nilfsdev->devvp, FREAD | FWRITE, NOCRED);
	if (error)
		printf("Error during closure of device! error %d, "
		       "device might stay locked\n", error);
	DPRINTF(VOLUMES, ("device close ok\n"));

	/* clear our mount reference and release device node */
	vput(nilfsdev->devvp);

	/* free our device info */
	cv_destroy(&nilfsdev->sync_cv);
	free(nilfsdev, M_NILFSMNT);
}


static int
nilfs_check_mounts(struct nilfs_device *nilfsdev, struct mount *mp,
	struct nilfs_args *args)
{
	struct nilfs_mount  *ump;
	uint64_t last_cno;

	/* no double-mounting of the same checkpoint */
	STAILQ_FOREACH(ump, &nilfsdev->mounts, next_mount) {
		if (ump->mount_args.cpno == args->cpno)
			return EBUSY;
	}

	/* allow readonly mounts without questioning here */
	if (mp->mnt_flag & MNT_RDONLY)
		return 0;

	/* readwrite mount you want */
	STAILQ_FOREACH(ump, &nilfsdev->mounts, next_mount) {
		/* only one RW mount on this device! */
		if ((ump->vfs_mountp->mnt_flag & MNT_RDONLY)==0)
			return EROFS;
		/* RDONLY on last mountpoint is device busy */
		last_cno = nilfs_rw64(ump->nilfsdev->super.s_last_cno);
		if (ump->mount_args.cpno == last_cno)
			return EBUSY;
	}

	/* OK for now */
	return 0;
}


static int
nilfs_mount_device(struct vnode *devvp, struct mount *mp, struct nilfs_args *args,
	struct nilfs_device **nilfsdev_p)
{
	uint64_t psize;
	unsigned secsize;
	struct nilfs_device *nilfsdev;
	struct lwp *l = curlwp;
	int openflags, accessmode, error;

	DPRINTF(VOLUMES, ("Mounting NILFS device\n"));

	/* lookup device in our nilfs_mountpoints */
	*nilfsdev_p = NULL;
	SLIST_FOREACH(nilfsdev, &nilfs_devices, next_device)
		if (nilfsdev->devvp == devvp)
			break;

	if (nilfsdev) {
		DPRINTF(VOLUMES, ("device already mounted\n"));
		error = nilfs_check_mounts(nilfsdev, mp, args);
		if (error)
			return error;
		nilfsdev->refcnt++;
		*nilfsdev_p = nilfsdev;
		return 0;
	}

	DPRINTF(VOLUMES, ("no previous mounts on this device, mounting device\n"));

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
	 * Open device read-write; TODO how about upgrading later when needed?
	 */
	openflags = FREAD | FWRITE;
	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
	error = VOP_OPEN(devvp, openflags, FSCRED);
	VOP_UNLOCK(devvp);
	if (error) {
		vrele(devvp);
		return error;
	}

	/* opened ok, try mounting */
	nilfsdev = malloc(sizeof(*nilfsdev), M_NILFSMNT, M_WAITOK | M_ZERO);

	/* initialise */
	nilfsdev->refcnt        = 1;
	nilfsdev->devvp         = devvp;
	nilfsdev->uncomitted_bl = 0;
	cv_init(&nilfsdev->sync_cv, "nilfssyn");
	STAILQ_INIT(&nilfsdev->mounts);

	/* register nilfs_device in list */
	SLIST_INSERT_HEAD(&nilfs_devices, nilfsdev, next_device);

	/* get our device's size */
	error = getdisksize(devvp, &psize, &secsize);
	if (error) {
		/* remove all our information */
		nilfs_unmount_device(nilfsdev);
		return EINVAL;
	}

	nilfsdev->devsize = psize * secsize;

	/* connect to the head for most recent files XXX really pass mp and args? */
	error = nilfs_mount_base(nilfsdev, mp, args);
	if (error) {
		/* remove all our information */
		nilfs_unmount_device(nilfsdev);
		return EINVAL;
	}

	*nilfsdev_p = nilfsdev;
	DPRINTF(VOLUMES, ("NILFS device mounted ok\n"));

	return 0;
}


static int
nilfs_mount_checkpoint(struct nilfs_mount *ump)
{
	struct nilfs_cpfile_header *cphdr;
	struct nilfs_checkpoint *cp;
	struct nilfs_inode  ifile_inode;
	struct nilfs_node  *cp_node;
	struct buf *bp;
	uint64_t ncp, nsn, fcpno, blocknr, last_cno;
	uint32_t off, dlen;
	int cp_per_block, error;

	DPRINTF(VOLUMES, ("mount_nilfs: trying to mount checkpoint number "
		"%"PRIu64"\n", ump->mount_args.cpno));

	cp_node = ump->nilfsdev->cp_node;

	/* get cpfile header from 1st block of cp file */
	error = nilfs_bread(cp_node, 0, 0, &bp);
	if (error)
		return error;
	cphdr = (struct nilfs_cpfile_header *) bp->b_data;
	ncp = nilfs_rw64(cphdr->ch_ncheckpoints);
	nsn = nilfs_rw64(cphdr->ch_nsnapshots);

	brelse(bp, BC_AGE);

	DPRINTF(VOLUMES, ("mount_nilfs: checkpoint header read in\n"));
	DPRINTF(VOLUMES, ("\tNumber of checkpoints %"PRIu64"\n", ncp));
	DPRINTF(VOLUMES, ("\tNumber of snapshots   %"PRIu64"\n", nsn));
#ifndef NILFS_DEBUG
	__USE(ncp);
	__USE(nsn);
#endif

	/* read in our specified checkpoint */
	dlen = nilfs_rw16(ump->nilfsdev->super.s_checkpoint_size);
	cp_per_block = ump->nilfsdev->blocksize / dlen;

	fcpno = ump->mount_args.cpno + NILFS_CPFILE_FIRST_CHECKPOINT_OFFSET -1;
	blocknr =  fcpno / cp_per_block;
	off     = (fcpno % cp_per_block) * dlen;

	error = nilfs_bread(cp_node, blocknr, 0, &bp);
	if (error) {
		printf("mount_nilfs: couldn't read cp block %"PRIu64"\n",
			fcpno);
		return EINVAL;
	}

	/* needs to be a valid checkpoint */
	cp = (struct nilfs_checkpoint *) ((uint8_t *) bp->b_data + off);
	if (cp->cp_flags & NILFS_CHECKPOINT_INVALID) {
		printf("mount_nilfs: checkpoint marked invalid\n");
		brelse(bp, BC_AGE);
		return EINVAL;
	}

	/* is this really the checkpoint we want? */
	if (nilfs_rw64(cp->cp_cno) != ump->mount_args.cpno) {
		printf("mount_nilfs: checkpoint file corrupt? "
			"expected cpno %"PRIu64", found cpno %"PRIu64"\n",
			ump->mount_args.cpno, nilfs_rw64(cp->cp_cno));
		brelse(bp, BC_AGE);
		return EINVAL;
	}

	/* check if its a snapshot ! */
	last_cno = nilfs_rw64(ump->nilfsdev->super.s_last_cno);
	if (ump->mount_args.cpno != last_cno) {
		/* only allow snapshots if not mounting on the last cp */
		if ((cp->cp_flags & NILFS_CHECKPOINT_SNAPSHOT) == 0) {
			printf( "mount_nilfs: checkpoint %"PRIu64" is not a "
				"snapshot\n", ump->mount_args.cpno);
			brelse(bp, BC_AGE);
			return EINVAL;
		}
	}

	ifile_inode = cp->cp_ifile_inode;
	brelse(bp, BC_AGE);

	/* get ifile inode */
	error = nilfs_get_node_raw(ump->nilfsdev, NULL, NILFS_IFILE_INO,
		&ifile_inode, &ump->ifile_node);
	if (error) {
		printf("mount_nilfs: can't read ifile node\n");
		return EINVAL;
	}

	/* get root node? */

	return 0;
}


static int
nilfs_stop_writing(struct nilfs_mount *ump)
{
	/* readonly mounts won't write */
	if (ump->vfs_mountp->mnt_flag & MNT_RDONLY)
		return 0;

	DPRINTF(CALL, ("nilfs_stop_writing called for RW mount\n"));

	/* TODO writeout super_block? */
	/* XXX no support for writing yet anyway */
	return 0;
}


/* --------------------------------------------------------------------- */



#define MPFREE(a, lst) \
	if ((a)) free((a), lst);
static void
free_nilfs_mountinfo(struct mount *mp)
{
	struct nilfs_mount *ump = VFSTONILFS(mp);

	if (ump == NULL)
		return;

	MPFREE(ump, M_NILFSMNT);
}
#undef MPFREE

int
nilfs_mount(struct mount *mp, const char *path,
	  void *data, size_t *data_len)
{
	struct nilfs_args   *args = data;
	struct nilfs_device *nilfsdev;
	struct nilfs_mount  *ump;
	struct vnode *devvp;
	int error;

	DPRINTF(VFSCALL, ("nilfs_mount called\n"));

	if (args == NULL)
		return EINVAL;
	if (*data_len < sizeof *args)
		return EINVAL;

	if (mp->mnt_flag & MNT_GETARGS) {
		/* request for the mount arguments */
		ump = VFSTONILFS(mp);
		if (ump == NULL)
			return EINVAL;
		*args = ump->mount_args;
		*data_len = sizeof *args;
		return 0;
	}

	/* check/translate struct version */
	if (args->version != 1) {
		printf("mount_nilfs: unrecognized argument structure version\n");
		return EINVAL;
	}
	/* TODO sanity checking other mount arguments */

	/* handle request for updating mount parameters */
	if (mp->mnt_flag & MNT_UPDATE) {
		/* TODO can't update my mountpoint yet */
		return EOPNOTSUPP;
	}

	/* lookup name to get its vnode */
	error = namei_simple_user(args->fspec, NSM_FOLLOW_NOEMULROOT, &devvp);
	if (error)
		return error;

#ifdef DEBUG
	if (nilfs_verbose & NILFS_DEBUG_VOLUMES)
		vprint("NILFS mount, trying to mount \n", devvp);
#endif

	error = nilfs_mount_device(devvp, mp, args, &nilfsdev);
	if (error)
		return error;

	/*
	 * Create a nilfs_mount on the specified checkpoint. Note that only
	 * ONE RW mount point can exist and it needs to have the highest
	 * checkpoint nr. If mounting RW and its not on the last checkpoint we
	 * need to invalidate all checkpoints that follow!!! This is an
	 * advanced option.
	 */

	/* setup basic mountpoint structure */
	mp->mnt_data = NULL;
	mp->mnt_stat.f_fsidx.__fsid_val[0] = (uint32_t) devvp->v_rdev;
	mp->mnt_stat.f_fsidx.__fsid_val[1] = makefstype(MOUNT_NILFS);
	mp->mnt_stat.f_fsid = mp->mnt_stat.f_fsidx.__fsid_val[0];
	mp->mnt_stat.f_namemax = NILFS_NAME_LEN;
	mp->mnt_flag  |= MNT_LOCAL;

	/* XXX can't enable MPSAFE yet since genfs barfs on bad CV */
	// mp->mnt_iflag |= IMNT_MPSAFE;

	/* set our dev and fs units */
	mp->mnt_dev_bshift = nilfs_rw32(nilfsdev->super.s_log_block_size) + 10;
	mp->mnt_fs_bshift  = mp->mnt_dev_bshift;

	/* allocate nilfs part of mount structure; malloc always succeeds */
	ump = malloc(sizeof(struct nilfs_mount), M_NILFSMNT, M_WAITOK | M_ZERO);

	/* set up linkage */
	mp->mnt_data    =  ump;
	ump->vfs_mountp =  mp;
	ump->nilfsdev   =  nilfsdev;

#if 0
#ifndef NILFS_READWRITE
	/* force read-only for now */
	if ((mp->mnt_flag & MNT_RDONLY) == 0) {
		printf( "Enable kernel/module option NILFS_READWRITE for "
			"writing, downgrading access to read-only\n");
		mp->mnt_flag |= MNT_RDONLY;
	}
#endif
#endif

	/* DONT register our nilfs mountpoint on our vfs mountpoint */
	spec_node_setmountedfs(devvp, NULL);
#if 0
	if (spec_node_getmountedfs(devvp) == NULL)
		spec_node_setmountedfs(devvp, mp);
	if ((mp->mnt_flag & MNT_RDONLY) == 0)
		spec_node_setmountedfs(devvp, mp);
#endif

	/* add our mountpoint */
	STAILQ_INSERT_TAIL(&nilfsdev->mounts, ump, next_mount);

	/* get our selected checkpoint */
	if (args->cpno == 0)
		args->cpno = nilfsdev->last_cno;
	args->cpno = MIN(args->cpno, nilfsdev->last_cno);

	/* setting up other parameters */
	ump->mount_args = *args;
	error = nilfs_mount_checkpoint(ump);
	if (error) {
		nilfs_unmount(mp, MNT_FORCE);
		return error;
	}

	/* set VFS info */
	error = set_statvfs_info(path, UIO_USERSPACE, args->fspec, UIO_USERSPACE,
			mp->mnt_op->vfs_name, mp, curlwp);
	if (error) {
		nilfs_unmount(mp, MNT_FORCE);
		return error;
	}

	/* successfully mounted */
	DPRINTF(VOLUMES, ("nilfs_mount() successfull\n"));

	return 0;
}

/* --------------------------------------------------------------------- */


/* remove our mountpoint and if its the last reference, remove our device */
int
nilfs_unmount(struct mount *mp, int mntflags)
{
	struct nilfs_device *nilfsdev;
	struct nilfs_mount  *ump;
	int error, flags;

	DPRINTF(VFSCALL, ("nilfs_umount called\n"));

	ump = VFSTONILFS(mp);
	if (!ump)
		panic("NILFS unmount: empty ump\n");
	nilfsdev = ump->nilfsdev;

	/*
	 * Flush all nodes associated to this mountpoint.
	 */
	flags = (mntflags & MNT_FORCE) ? FORCECLOSE : 0;
	if ((error = vflush(mp, NULLVP, flags)) != 0)
		return error;

	/* if we're the write mount, we ought to close the writing session */
	error = nilfs_stop_writing(ump);
	if (error)
		return error;

	nilfs_dispose_node(&ump->ifile_node);

	/* remove our mount point */
	STAILQ_REMOVE(&nilfsdev->mounts, ump, nilfs_mount, next_mount);
	free_nilfs_mountinfo(mp);

	/* free ump struct references */
	mp->mnt_data = NULL;
	mp->mnt_flag &= ~MNT_LOCAL;

	/* unmount the device itself when we're the last one */
	nilfs_unmount_device(nilfsdev);

	DPRINTF(VOLUMES, ("Fin unmount\n"));
	return error;
}

/* --------------------------------------------------------------------- */

int
nilfs_start(struct mount *mp, int flags)
{
	/* do we have to do something here? */
	return 0;
}

/* --------------------------------------------------------------------- */

int
nilfs_root(struct mount *mp, struct vnode **vpp)
{
	uint64_t ino = NILFS_ROOT_INO;
	int error;

	DPRINTF(NODE, ("nilfs_root called\n"));

	error = vcache_get(mp, &ino, sizeof(ino), vpp);
	if (error == 0) {
		error = vn_lock(*vpp, LK_EXCLUSIVE);
		if (error) {
			vrele(*vpp);
			*vpp = NULL;
			return error;
		}
	}
	KASSERT(error != 0 || ((*vpp)->v_vflag & VV_ROOT));

	DPRINTF(NODE, ("nilfs_root finished\n"));
	return error;
}

/* --------------------------------------------------------------------- */

int
nilfs_statvfs(struct mount *mp, struct statvfs *sbp)
{
	struct nilfs_mount *ump = VFSTONILFS(mp);
	uint32_t blocksize;

	DPRINTF(VFSCALL, ("nilfs_statvfs called\n"));

	blocksize = ump->nilfsdev->blocksize;
	sbp->f_flag   = mp->mnt_flag;
	sbp->f_bsize  = blocksize;
	sbp->f_frsize = blocksize;
	sbp->f_iosize = blocksize;

	copy_statvfs_info(sbp, mp);
	return 0;
}

/* --------------------------------------------------------------------- */

int
nilfs_sync(struct mount *mp, int waitfor, kauth_cred_t cred)
{
//	struct nilfs_mount *ump = VFSTONILFS(mp);

	DPRINTF(VFSCALL, ("nilfs_sync called\n"));
	/* if called when mounted readonly, just ignore */
	if (mp->mnt_flag & MNT_RDONLY)
		return 0;

	DPRINTF(VFSCALL, ("end of nilfs_sync()\n"));

	return 0;
}

/* --------------------------------------------------------------------- */

/*
 * Get vnode for the file system type specific file id ino for the fs. Its
 * used for reference to files by unique ID and for NFSv3.
 * (optional) TODO lookup why some sources state NFSv3
 */
int
nilfs_vget(struct mount *mp, ino_t ino,
    struct vnode **vpp)
{
	DPRINTF(NOTIMPL, ("nilfs_vget called\n"));
	return EOPNOTSUPP;
}

/* --------------------------------------------------------------------- */

/*
 * Read an inode from disk and initialize this vnode / inode pair.
 * Caller assures no other thread will try to load this inode.
 */
int
nilfs_loadvnode(struct mount *mp, struct vnode *vp,
    const void *key, size_t key_len, const void **new_key)
{
	uint64_t ino;
	struct nilfs_device *nilfsdev;
	struct nilfs_inode   inode, *entry;
	struct nilfs_node *node;
	struct nilfs_mount *ump;
	struct buf *bp;
	uint64_t ivblocknr;
	uint32_t entry_in_block;
	int error;
	extern int (**nilfs_vnodeop_p)(void *);

	KASSERT(key_len == sizeof(ino));
	memcpy(&ino, key, key_len);

	ump = VFSTONILFS(mp);

	/* create new inode; XXX check could be handier */
	if ((ino < NILFS_USER_INO) && (ino != NILFS_ROOT_INO)) {
		printf("nilfs_get_node: system ino %"PRIu64" not in mount "
			"point!\n", ino);
		return ENOENT;
	}

	/* lookup inode in the ifile */
	DPRINTF(NODE, ("lookup ino %"PRIu64"\n", ino));

	/* lookup inode structure in mountpoints ifile */
	nilfsdev = ump->nilfsdev;
	nilfs_mdt_trans(&nilfsdev->ifile_mdt, ino, &ivblocknr, &entry_in_block);

	error = nilfs_bread(ump->ifile_node, ivblocknr, 0, &bp);
	if (error)
		return ENOENT;

	/* get inode entry */
	entry =  (struct nilfs_inode *) bp->b_data + entry_in_block;
	inode = *entry;
	brelse(bp, BC_AGE);

	/* get node */
	error = nilfs_get_node_raw(ump->nilfsdev, ump, ino, &inode, &node);
	if (error)
		return error;

	vp->v_type = IFTOVT(inode.i_mode);
	switch (vp->v_type) {
	case VREG:
	case VDIR:
	case VLNK:
		break;
	/* other types not yet supported. */
	default:
		vp->v_type = VNON;
		nilfs_dispose_node(&node);
		return ENXIO;
	}

	vp->v_tag = VT_NILFS;
	vp->v_op = nilfs_vnodeop_p;
	vp->v_data = node;
	node->vnode = vp;

	/* initialise genfs */
	genfs_node_init(vp, &nilfs_genfsops);

	/* check if we're fetching the root */
	if (ino == NILFS_ROOT_INO)
		vp->v_vflag |= VV_ROOT;

	uvm_vnp_setsize(vp, nilfs_rw64(inode.i_size));
	*new_key = &node->ino;

	return 0;

}

/* --------------------------------------------------------------------- */

/*
 * Lookup vnode for file handle specified
 */
int
nilfs_fhtovp(struct mount *mp, struct fid *fhp,
    struct vnode **vpp)
{
	DPRINTF(NOTIMPL, ("nilfs_fhtovp called\n"));
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
nilfs_vptofh(struct vnode *vp, struct fid *fid,
    size_t *fh_size)
{
	DPRINTF(NOTIMPL, ("nilfs_vptofh called\n"));
	return EOPNOTSUPP;
}

/* --------------------------------------------------------------------- */

/*
 * Create a file system snapshot at the specified timestamp.
 */
int
nilfs_snapshot(struct mount *mp, struct vnode *vp,
    struct timespec *tm)
{
	DPRINTF(NOTIMPL, ("nilfs_snapshot called\n"));
	return EOPNOTSUPP;
}

/* --------------------------------------------------------------------- */
