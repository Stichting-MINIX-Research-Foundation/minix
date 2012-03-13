/*	$NetBSD: chfs_vfsops.c,v 1.2 2011/11/24 21:09:37 agc Exp $	*/

/*-
 * Copyright (c) 2010 Department of Software Engineering,
 *		      University of Szeged, Hungary
 * Copyright (C) 2010 Tamas Toth <ttoth@inf.u-szeged.hu>
 * Copyright (C) 2010 Adam Hoka <ahoka@NetBSD.org>
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by the Department of Software Engineering, University of Szeged, Hungary
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/module.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/buf.h>
//XXX needed just for debugging
#include <sys/fstrans.h>
#include <sys/sleepq.h>
#include <sys/lockdebug.h>
#include <sys/ktrace.h>

#include <uvm/uvm.h>
#include <uvm/uvm_pager.h>
#include <ufs/ufs/dir.h>
//#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufs_extern.h>
#include <miscfs/genfs/genfs.h>
#include <miscfs/genfs/genfs_node.h>
#include <miscfs/specfs/specdev.h>
//#include </root/xipffs/netbsd.chfs/chfs.h>
//#include </root/xipffs/netbsd.chfs/chfs_args.h>
#include "chfs.h"
#include "chfs_args.h"

MODULE(MODULE_CLASS_VFS, chfs, "flash");

/* --------------------------------------------------------------------- */
/* functions */

static int chfs_mount(struct mount *, const char *, void *, size_t *);
static int chfs_unmount(struct mount *, int);
static int chfs_root(struct mount *, struct vnode **);
static int chfs_vget(struct mount *, ino_t, struct vnode **);
static int chfs_fhtovp(struct mount *, struct fid *, struct vnode **);
static int chfs_vptofh(struct vnode *, struct fid *, size_t *);
static int chfs_start(struct mount *, int);
static int chfs_statvfs(struct mount *, struct statvfs *);
static int chfs_sync(struct mount *, int, kauth_cred_t);
static void chfs_init(void);
static void chfs_reinit(void);
static void chfs_done(void);
static int chfs_snapshot(struct mount *, struct vnode *,
    struct timespec *);

/* --------------------------------------------------------------------- */
/* structures */

int
chfs_gop_alloc(struct vnode *vp, off_t off, off_t len,  int flags,
    kauth_cred_t cred)
{
	return (0);
}

const struct genfs_ops chfs_genfsops = {
	.gop_size = genfs_size,
	.gop_alloc = chfs_gop_alloc,
	.gop_write = genfs_gop_write,
	.gop_markupdate = ufs_gop_markupdate,
};

/*
static const struct ufs_ops chfs_ufsops = {
	.uo_itimes = chfs_itimes,
	.uo_update = chfs_update,
};
*/

struct pool chfs_inode_pool;

/* for looking up the major for flash */
extern const struct cdevsw flash_cdevsw;

/* --------------------------------------------------------------------- */

static int
chfs_mount(struct mount *mp,
    const char *path, void *data, size_t *data_len)
{
	struct lwp *l = curlwp;
	struct nameidata nd;
	struct pathbuf *pb;
	struct vnode *devvp = NULL;
	struct ufs_args *args = data;
	struct ufsmount *ump = NULL;
	struct chfs_mount *chmp;
	int err = 0;
	int xflags;

	dbg("mount()\n");

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

	if (mp->mnt_flag & MNT_UPDATE) {
		/* XXX: There is no support yet to update file system
		 * settings.  Should be added. */

		return ENODEV;
	}

	if (args->fspec != NULL) {
		err = pathbuf_copyin(args->fspec, &pb);
		if (err) {
			return err;
		}
		/*
		 * Look up the name and verify that it's sane.
		 */
		NDINIT(&nd, LOOKUP, FOLLOW, pb);
		if ((err = namei(&nd)) != 0 )
			return (err);
		devvp = nd.ni_vp;

		/*
		 * Be sure this is a valid block device
		 */
		if (devvp->v_type != VBLK)
			err = ENOTBLK;
		else if (bdevsw_lookup(devvp->v_rdev) == NULL)
			err = ENXIO;
	}

	if (err) {
		vrele(devvp);
		return (err);
	}

	if (mp->mnt_flag & MNT_RDONLY)
		xflags = FREAD;
	else
		xflags = FREAD|FWRITE;

	err = VOP_OPEN(devvp, xflags, FSCRED);
	if (err)
		goto fail;


	err = chfs_mountfs(devvp, mp);
	if (err) {
		vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
		(void)VOP_CLOSE(devvp, xflags, NOCRED);
		VOP_UNLOCK(devvp);
		goto fail;
	}
	ump = VFSTOUFS(mp);
	chmp = ump->um_chfs;

	vfs_getnewfsid(mp);
	chmp->chm_fsmp = mp;

	return set_statvfs_info(path,
	    UIO_USERSPACE, args->fspec,
	    UIO_USERSPACE, mp->mnt_op->vfs_name, mp, l);

fail:
	vrele(devvp);
	return (err);
}


int
chfs_mountfs(struct vnode *devvp, struct mount *mp)
{
	struct lwp *l = curlwp;
	struct proc *p;
	kauth_cred_t cred;
	devmajor_t flash_major;
	dev_t dev;
	struct ufsmount* ump = NULL;
	struct chfs_mount* chmp;
	struct vnode *vp;
	int err = 0;

	dbg("mountfs()\n");

	dev = devvp->v_rdev;
	p = l ? l->l_proc : NULL;
	cred = l ? l->l_cred : NOCRED;

	/* Flush out any old buffers remaining from a previous use. */
	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
	err = vinvalbuf(devvp, V_SAVE, cred, l, 0, 0);
	VOP_UNLOCK(devvp);
	if (err)
		return (err);

	flash_major = cdevsw_lookup_major(&flash_cdevsw);

	if (devvp->v_type != VBLK)
		err = ENOTBLK;
	else if (bdevsw_lookup(dev) == NULL)
		err = ENXIO;
	else if (major(dev) != flash_major) {
		dbg("major(dev): %d, flash_major: %d\n",
		    major(dev), flash_major);
		err = ENODEV;
	}
	if (err) {
		vrele(devvp);
		return (err);
	}

	ump = malloc(sizeof(*ump), M_UFSMNT, M_WAITOK);
	memset(ump, 0, sizeof(*ump));
	ump->um_fstype = UFS1;
	//ump->um_ops = &chfs_ufsops;
	ump->um_chfs = malloc(sizeof(struct chfs_mount),
	    M_UFSMNT, M_WAITOK);
	memset(ump->um_chfs, 0, sizeof(struct chfs_mount));

	mutex_init(&ump->um_lock, MUTEX_DEFAULT, IPL_NONE);

	/* Get superblock and set flash device number */
	chmp = ump->um_chfs;
	if (!chmp)
		return ENOMEM;

	chmp->chm_ebh = kmem_alloc(sizeof(struct chfs_ebh), KM_SLEEP);

	dbg("[]opening flash: %u\n", (unsigned int)devvp->v_rdev);
	err = ebh_open(chmp->chm_ebh, devvp->v_rdev);
	if (err) {
		dbg("error while opening flash\n");
		kmem_free(chmp->chm_ebh, sizeof(struct chfs_ebh));
		free(chmp, M_UFSMNT);
		return err;
	}

	//TODO check flash sizes

	chmp->chm_gbl_version = 0;
	chmp->chm_vnocache_hash = chfs_vnocache_hash_init();

	chmp->chm_blocks = kmem_zalloc(chmp->chm_ebh->peb_nr *
	    sizeof(struct chfs_eraseblock), KM_SLEEP);

	if (!chmp->chm_blocks) {
		kmem_free(chmp->chm_ebh, chmp->chm_ebh->peb_nr *
		    sizeof(struct chfs_eraseblock));
		ebh_close(chmp->chm_ebh);
		free(chmp, M_UFSMNT);
		return ENOMEM;
	}

	mutex_init(&chmp->chm_lock_mountfields, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&chmp->chm_lock_sizes, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&chmp->chm_lock_vnocache, MUTEX_DEFAULT, IPL_NONE);

	//XXX
	chmp->chm_fs_bmask = -4096;
	chmp->chm_fs_bsize = 4096;
	chmp->chm_fs_qbmask = 4095;
	chmp->chm_fs_bshift = 12;
	chmp->chm_fs_fmask = -2048;
	chmp->chm_fs_qfmask = 2047;

	chmp->chm_wbuf_pagesize = chmp->chm_ebh->flash_if->page_size;
	dbg("wbuf size: %zu\n", chmp->chm_wbuf_pagesize);
	chmp->chm_wbuf = kmem_alloc(chmp->chm_wbuf_pagesize, KM_SLEEP);
	rw_init(&chmp->chm_lock_wbuf);

	//init queues
	TAILQ_INIT(&chmp->chm_free_queue);
	TAILQ_INIT(&chmp->chm_clean_queue);
	TAILQ_INIT(&chmp->chm_dirty_queue);
	TAILQ_INIT(&chmp->chm_very_dirty_queue);
	TAILQ_INIT(&chmp->chm_erasable_pending_wbuf_queue);
	TAILQ_INIT(&chmp->chm_erase_pending_queue);

	chfs_calc_trigger_levels(chmp);

	chmp->chm_nr_free_blocks = 0;
	chmp->chm_nr_erasable_blocks = 0;
	chmp->chm_max_vno = 2;
	chmp->chm_checked_vno = 2;
	chmp->chm_unchecked_size = 0;
	chmp->chm_used_size = 0;
	chmp->chm_dirty_size = 0;
	chmp->chm_wasted_size = 0;
	chmp->chm_free_size = chmp->chm_ebh->eb_size * chmp->chm_ebh->peb_nr;
	err = chfs_build_filesystem(chmp);

	if (err) {
		chfs_vnocache_hash_destroy(chmp->chm_vnocache_hash);
		kmem_free(chmp->chm_ebh, chmp->chm_ebh->peb_nr *
		    sizeof(struct chfs_eraseblock));
		ebh_close(chmp->chm_ebh);
		free(chmp, M_UFSMNT);
		return EIO;
	}

	mp->mnt_data = ump;
	mp->mnt_stat.f_fsidx.__fsid_val[0] = (long)dev;
	mp->mnt_stat.f_fsidx.__fsid_val[1] = makefstype(MOUNT_CHFS);
	mp->mnt_stat.f_fsid = mp->mnt_stat.f_fsidx.__fsid_val[0];
	mp->mnt_stat.f_namemax = MAXNAMLEN;
	mp->mnt_flag |= MNT_LOCAL;
	mp->mnt_fs_bshift = PAGE_SHIFT;
	mp->mnt_dev_bshift = DEV_BSHIFT;
	mp->mnt_iflag |= IMNT_MPSAFE;
	ump->um_flags = 0;
	ump->um_mountp = mp;
	ump->um_dev = dev;
	ump->um_devvp = devvp;
	ump->um_maxfilesize = 1048512 * 1024;
	/*TODO fill these fields
	  ump->um_nindir =
	  ump->um_lognindir =
	  ump->um_bptrtodb =
	  ump->um_seqinc =
	  ump->um_maxsymlinklen =
	  ump->um_dirblksiz =
	  ump->um_maxfilesize =
	*/

	/*
	 * Allocate the root vnode.
	 */
	err = VFS_VGET(mp, CHFS_ROOTINO, &vp);
	if (err) {
		dbg("error: %d while allocating root node\n", err);
		return err;
	}
	vput(vp);

	chfs_gc_thread_start(chmp);
	mutex_enter(&chmp->chm_lock_mountfields);
	chfs_gc_trigger(chmp);
	mutex_exit(&chmp->chm_lock_mountfields);

	devvp->v_specmountpoint = mp;
	return 0;
}

/* --------------------------------------------------------------------- */

/* ARGSUSED2 */
static int
chfs_unmount(struct mount *mp, int mntflags)
{
	int flags = 0, i = 0;
	struct ufsmount *ump;
	struct chfs_mount *chmp;
//	struct chfs_vnode_cache *vc, *next;

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	dbg("[START]\n");

	ump = VFSTOUFS(mp);
	chmp = ump->um_chfs;

	chfs_gc_thread_stop(chmp);

	(void)vflush(mp, NULLVP, flags);

	if (chmp->chm_wbuf_len) {
		mutex_enter(&chmp->chm_lock_mountfields);
		chfs_flush_pending_wbuf(chmp);
		mutex_exit(&chmp->chm_lock_mountfields);
	}

	for (i = 0; i < chmp->chm_ebh->peb_nr; i++) {
		chfs_free_node_refs(&chmp->chm_blocks[i]);
	}

	chfs_vnocache_hash_destroy(chmp->chm_vnocache_hash);

	ebh_close(chmp->chm_ebh);

	rw_destroy(&chmp->chm_lock_wbuf);
	mutex_destroy(&chmp->chm_lock_vnocache);
	mutex_destroy(&chmp->chm_lock_sizes);
	mutex_destroy(&chmp->chm_lock_mountfields);

	if (ump->um_devvp->v_type != VBAD) {
		ump->um_devvp->v_specmountpoint = NULL;
	}
	vn_lock(ump->um_devvp, LK_EXCLUSIVE | LK_RETRY);
	(void)VOP_CLOSE(ump->um_devvp, FREAD|FWRITE, NOCRED);
	vput(ump->um_devvp);

	mutex_destroy(&ump->um_lock);

	//free(ump->um_chfs, M_UFSMNT);
	free(ump, M_UFSMNT);
	mp->mnt_data = NULL;
	mp->mnt_flag &= ~MNT_LOCAL;
	dbg("[END]\n");
	return (0);
}

/* --------------------------------------------------------------------- */

static int
chfs_root(struct mount *mp, struct vnode **vpp)
{
	struct vnode *vp;
	int error;

	if ((error = VFS_VGET(mp, (ino_t)ROOTINO, &vp)) != 0)
		return error;
	*vpp = vp;
	return 0;
}

/* --------------------------------------------------------------------- */

extern rb_tree_ops_t frag_rbtree_ops;

static int
chfs_vget(struct mount *mp, ino_t ino, struct vnode **vpp)
{
	struct chfs_mount *chmp;
	struct chfs_inode *ip;
	struct ufsmount *ump;
	struct vnode *vp;
	dev_t dev;
	int error;
	struct chfs_vnode_cache* chvc = NULL;
	struct chfs_node_ref* nref = NULL;
	struct buf *bp;

	dbg("vget() | ino: %llu\n", (unsigned long long)ino);

	ump = VFSTOUFS(mp);
	dev = ump->um_dev;
retry:
	if (!vpp) {
		vpp = kmem_alloc(sizeof(struct vnode*), KM_SLEEP);
	}

	if ((*vpp = chfs_ihashget(dev, ino, LK_EXCLUSIVE)) != NULL) {
		return 0;
	}

	/* Allocate a new vnode/inode. */
	if ((error = getnewvnode(VT_CHFS,
		    mp, chfs_vnodeop_p, NULL, &vp)) != 0) {
		*vpp = NULL;
		return (error);
	}
	ip = pool_get(&chfs_inode_pool, PR_WAITOK);

	mutex_enter(&chfs_hashlock);
	if ((*vpp = chfs_ihashget(dev, ino, LK_EXCLUSIVE)) != NULL) {
		mutex_exit(&chfs_hashlock);
		ungetnewvnode(vp);
		pool_put(&chfs_inode_pool, ip);
		goto retry;
	}

	vp->v_vflag |= VV_LOCKSWORK;

	memset(ip, 0, sizeof(*ip));
	vp->v_data = ip;
	ip->vp = vp;
	ip->ump = ump;
	ip->chmp = chmp = ump->um_chfs;
	ip->dev = dev;
	ip->ino = ino;
	vp->v_mount = mp;
	genfs_node_init(vp, &chfs_genfsops);

	rb_tree_init(&ip->fragtree, &frag_rbtree_ops);
	//mutex_init(&ip->inode_lock, MUTEX_DEFAULT, IPL_NONE);

	chfs_ihashins(ip);
	mutex_exit(&chfs_hashlock);

	// set root inode
	if (ino == CHFS_ROOTINO) {
		dbg("SETROOT\n");
		vp->v_vflag |= VV_ROOT;
		vp->v_type = VDIR;
		ip->mode = IFMT | IEXEC | IWRITE | IREAD;
		ip->iflag |= (IN_ACCESS | IN_CHANGE | IN_UPDATE);
		chfs_update(vp, NULL, NULL, UPDATE_WAIT);
//		ip->dents = NULL; XXXTAILQ
		TAILQ_INIT(&ip->dents);
		chfs_set_vnode_size(vp, 512);
	}

	// set vnode cache
	mutex_enter(&chmp->chm_lock_vnocache);
	chvc = chfs_vnode_cache_get(chmp, ino);
	mutex_exit(&chmp->chm_lock_vnocache);
	if (!chvc) {
		dbg("!chvc\n");
		/* XXX, we cant alloc under a lock, refactor this! */
		chvc = chfs_vnode_cache_alloc(ino);
		mutex_enter(&chmp->chm_lock_vnocache);
		if (ino == CHFS_ROOTINO) {
			chvc->nlink = 2;
			chvc->pvno = CHFS_ROOTINO;
			chfs_vnode_cache_set_state(chmp,
			    chvc, VNO_STATE_CHECKEDABSENT);
		}
		chfs_vnode_cache_add(chmp, chvc);
		mutex_exit(&chmp->chm_lock_vnocache);

		ip->chvc = chvc;
		TAILQ_INIT(&ip->dents);
	} else {
		dbg("chvc\n");
		ip->chvc = chvc;
		// if we have a vnode cache, the node is already on flash, so read it
		if (ino == CHFS_ROOTINO) {
			chvc->pvno = CHFS_ROOTINO;
			TAILQ_INIT(&chvc->scan_dirents);
		} else {
			chfs_readvnode(mp, ino, &vp);
		}

		mutex_enter(&chmp->chm_lock_mountfields);
		// init type specific things
		switch (vp->v_type) {
		case VDIR:
			nref = chvc->dirents;
			while (nref &&
			    (struct chfs_vnode_cache *)nref != chvc) {
				chfs_readdirent(mp, nref, ip);
				nref = nref->nref_next;
			}
			chfs_set_vnode_size(vp, 512);
			break;
		case VREG:
		case VSOCK:
			//build the fragtree of the vnode
			dbg("read_inode_internal | ino: %llu\n",
				(unsigned long long)ip->ino);
			error = chfs_read_inode(chmp, ip);
			if (error) {
				vput(vp);
				*vpp = NULL;
				mutex_exit(&chmp->chm_lock_mountfields);
				return (error);
			}
			break;
		case VLNK:
			//build the fragtree of the vnode
			dbg("read_inode_internal | ino: %llu\n",
				(unsigned long long)ip->ino);
			error = chfs_read_inode_internal(chmp, ip);
			if (error) {
				vput(vp);
				*vpp = NULL;
				mutex_exit(&chmp->chm_lock_mountfields);
				return (error);
			}

			dbg("size: %llu\n", (unsigned long long)ip->size);
			bp = getiobuf(vp, true);
			bp->b_blkno = 0;
			bp->b_bufsize = bp->b_resid =
			    bp->b_bcount = ip->size;
			bp->b_data = kmem_alloc(ip->size, KM_SLEEP);
			chfs_read_data(chmp, vp, bp);
			if (!ip->target)
				ip->target = kmem_alloc(ip->size,
				    KM_SLEEP);
			memcpy(ip->target, bp->b_data, ip->size);
			kmem_free(bp->b_data, ip->size);
			putiobuf(bp);

			break;
		case VCHR:
		case VBLK:
		case VFIFO:
			//build the fragtree of the vnode
			dbg("read_inode_internal | ino: %llu\n",
				(unsigned long long)ip->ino);
			error = chfs_read_inode_internal(chmp, ip);
			if (error) {
				vput(vp);
				*vpp = NULL;
				mutex_exit(&chmp->chm_lock_mountfields);
				return (error);
			}

			bp = getiobuf(vp, true);
			bp->b_blkno = 0;
			bp->b_bufsize = bp->b_resid =
			    bp->b_bcount = sizeof(dev_t);
			bp->b_data = kmem_alloc(sizeof(dev_t), KM_SLEEP);
			chfs_read_data(chmp, vp, bp);
			memcpy(&ip->rdev,
			    bp->b_data, sizeof(dev_t));
			kmem_free(bp->b_data, sizeof(dev_t));
			putiobuf(bp);
			if (vp->v_type == VFIFO)
				vp->v_op = chfs_fifoop_p;
			else {
				vp->v_op = chfs_specop_p;
				spec_node_init(vp, ip->rdev);
			}

		    break;
		case VNON:
		case VBAD:
			break;
		}
		mutex_exit(&chmp->chm_lock_mountfields);

	}

	/* finish inode initalization */
	ip->devvp = ump->um_devvp;
	vref(ip->devvp);

	uvm_vnp_setsize(vp, ip->size);
	*vpp = vp;

	return 0;
}

/* --------------------------------------------------------------------- */

static int
chfs_fhtovp(struct mount *mp, struct fid *fhp, struct vnode **vpp)
{
	return ENODEV;
}

/* --------------------------------------------------------------------- */

static int
chfs_vptofh(struct vnode *vp, struct fid *fhp, size_t *fh_size)
{
	return ENODEV;
}

/* --------------------------------------------------------------------- */

static int
chfs_start(struct mount *mp, int flags)
{
	return 0;
}

/* --------------------------------------------------------------------- */

/* ARGSUSED2 */
static int
chfs_statvfs(struct mount *mp, struct statvfs *sbp)
{
 	struct chfs_mount *chmp;
	struct ufsmount *ump;
	dbg("statvfs\n");

	ump = VFSTOUFS(mp);
	chmp = ump->um_chfs;

	sbp->f_flag   = mp->mnt_flag;
	sbp->f_bsize  = chmp->chm_ebh->eb_size;
	sbp->f_frsize = chmp->chm_ebh->eb_size;
	sbp->f_iosize = chmp->chm_ebh->eb_size;

	sbp->f_blocks = chmp->chm_ebh->peb_nr;
	sbp->f_files  = 0;
	sbp->f_bavail = chmp->chm_nr_free_blocks - chmp->chm_resv_blocks_write;
#if 0
	printf("chmp->chm_nr_free_blocks: %jd\n",
	    (intmax_t )chmp->chm_nr_free_blocks);
	printf("chmp->chm_resv_blocks_write: %jd\n",
	    (intmax_t) chmp->chm_resv_blocks_write);
	printf("chmp->chm_ebh->peb_nr: %jd\n",
	    (intmax_t) chmp->chm_ebh->peb_nr);
#endif

	sbp->f_bfree = chmp->chm_nr_free_blocks;
	sbp->f_bresvd = chmp->chm_resv_blocks_write;

	/* FFS specific */
	sbp->f_ffree  = 0;
	sbp->f_favail = 0;
	sbp->f_fresvd = 0;

	copy_statvfs_info(sbp, mp);

	return 0;
}

/* --------------------------------------------------------------------- */

/* ARGSUSED0 */
static int
chfs_sync(struct mount *mp, int waitfor,
    kauth_cred_t uc)
{
	return 0;
}

/* --------------------------------------------------------------------- */

static void
chfs_init(void)
{
	chfs_alloc_pool_caches();
	chfs_ihashinit();
	pool_init(&chfs_inode_pool, sizeof(struct chfs_inode), 0, 0, 0,
	    "chfsinopl", &pool_allocator_nointr, IPL_NONE);
	ufs_init();
}

/* --------------------------------------------------------------------- */

static void
chfs_reinit(void)
{
	chfs_ihashreinit();
	ufs_reinit();
}

/* --------------------------------------------------------------------- */

static void
chfs_done(void)
{
	ufs_done();
	chfs_ihashdone();
	pool_destroy(&chfs_inode_pool);
	chfs_destroy_pool_caches();
}

/* --------------------------------------------------------------------- */

static int
chfs_snapshot(struct mount *mp, struct vnode *vp,
    struct timespec *ctime)
{
	return ENODEV;
}

/* --------------------------------------------------------------------- */

/*
 * chfs vfs operations.
 */

extern const struct vnodeopv_desc chfs_fifoop_opv_desc;
extern const struct vnodeopv_desc chfs_specop_opv_desc;
extern const struct vnodeopv_desc chfs_vnodeop_opv_desc;

const struct vnodeopv_desc * const chfs_vnodeopv_descs[] = {
	&chfs_fifoop_opv_desc,
	&chfs_specop_opv_desc,
	&chfs_vnodeop_opv_desc,
	NULL,
};

struct vfsops chfs_vfsops = {
	MOUNT_CHFS,			/* vfs_name */
	sizeof (struct chfs_args),
	chfs_mount,			/* vfs_mount */
	chfs_start,			/* vfs_start */
	chfs_unmount,		/* vfs_unmount */
	chfs_root,			/* vfs_root */
	ufs_quotactl,			/* vfs_quotactl */
	chfs_statvfs,		/* vfs_statvfs */
	chfs_sync,			/* vfs_sync */
	chfs_vget,			/* vfs_vget */
	chfs_fhtovp,		/* vfs_fhtovp */
	chfs_vptofh,		/* vfs_vptofh */
	chfs_init,			/* vfs_init */
	chfs_reinit,		/* vfs_reinit */
	chfs_done,			/* vfs_done */
	NULL,				/* vfs_mountroot */
	chfs_snapshot,		/* vfs_snapshot */
	vfs_stdextattrctl,		/* vfs_extattrctl */
	(void *)eopnotsupp,		/* vfs_suspendctl */
	genfs_renamelock_enter,
	genfs_renamelock_exit,
	(void *)eopnotsupp,
	chfs_vnodeopv_descs,
	0,				/* vfs_refcount */
	{ NULL, NULL },
};

static int
chfs_modcmd(modcmd_t cmd, void *arg)
{
	switch (cmd) {
	case MODULE_CMD_INIT:
		return vfs_attach(&chfs_vfsops);
	case MODULE_CMD_FINI:
		return vfs_detach(&chfs_vfsops);
	default:
		return ENOTTY;
	}
}
