/*	$NetBSD: spec_vnops.c,v 1.153 2015/07/01 08:13:52 hannken Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
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
 *	@(#)spec_vnops.c	8.15 (Berkeley) 7/14/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: spec_vnops.c,v 1.153 2015/07/01 08:13:52 hannken Exp $");

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/file.h>
#include <sys/disklabel.h>
#include <sys/lockf.h>
#include <sys/tty.h>
#include <sys/kauth.h>
#include <sys/fstrans.h>
#include <sys/module.h>

#include <miscfs/genfs/genfs.h>
#include <miscfs/specfs/specdev.h>

/* symbolic sleep message strings for devices */
const char	devopn[] = "devopn";
const char	devio[] = "devio";
const char	devwait[] = "devwait";
const char	devin[] = "devin";
const char	devout[] = "devout";
const char	devioc[] = "devioc";
const char	devcls[] = "devcls";

#define	SPECHSZ	64
#if	((SPECHSZ&(SPECHSZ-1)) == 0)
#define	SPECHASH(rdev)	(((rdev>>5)+(rdev))&(SPECHSZ-1))
#else
#define	SPECHASH(rdev)	(((unsigned)((rdev>>5)+(rdev)))%SPECHSZ)
#endif

static vnode_t	*specfs_hash[SPECHSZ];
extern struct mount *dead_rootmount;

/*
 * This vnode operations vector is used for special device nodes
 * created from whole cloth by the kernel.  For the ops vector for
 * vnodes built from special devices found in a filesystem, see (e.g)
 * ffs_specop_entries[] in ffs_vnops.c or the equivalent for other
 * filesystems.
 */

int (**spec_vnodeop_p)(void *);
const struct vnodeopv_entry_desc spec_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, spec_lookup },		/* lookup */
	{ &vop_create_desc, spec_create },		/* create */
	{ &vop_mknod_desc, spec_mknod },		/* mknod */
	{ &vop_open_desc, spec_open },			/* open */
	{ &vop_close_desc, spec_close },		/* close */
	{ &vop_access_desc, spec_access },		/* access */
	{ &vop_getattr_desc, spec_getattr },		/* getattr */
	{ &vop_setattr_desc, spec_setattr },		/* setattr */
	{ &vop_read_desc, spec_read },			/* read */
	{ &vop_write_desc, spec_write },		/* write */
	{ &vop_fallocate_desc, spec_fallocate },	/* fallocate */
	{ &vop_fdiscard_desc, spec_fdiscard },		/* fdiscard */
	{ &vop_fcntl_desc, spec_fcntl },		/* fcntl */
	{ &vop_ioctl_desc, spec_ioctl },		/* ioctl */
	{ &vop_poll_desc, spec_poll },			/* poll */
	{ &vop_kqfilter_desc, spec_kqfilter },		/* kqfilter */
	{ &vop_revoke_desc, spec_revoke },		/* revoke */
	{ &vop_mmap_desc, spec_mmap },			/* mmap */
	{ &vop_fsync_desc, spec_fsync },		/* fsync */
	{ &vop_seek_desc, spec_seek },			/* seek */
	{ &vop_remove_desc, spec_remove },		/* remove */
	{ &vop_link_desc, spec_link },			/* link */
	{ &vop_rename_desc, spec_rename },		/* rename */
	{ &vop_mkdir_desc, spec_mkdir },		/* mkdir */
	{ &vop_rmdir_desc, spec_rmdir },		/* rmdir */
	{ &vop_symlink_desc, spec_symlink },		/* symlink */
	{ &vop_readdir_desc, spec_readdir },		/* readdir */
	{ &vop_readlink_desc, spec_readlink },		/* readlink */
	{ &vop_abortop_desc, spec_abortop },		/* abortop */
	{ &vop_inactive_desc, spec_inactive },		/* inactive */
	{ &vop_reclaim_desc, spec_reclaim },		/* reclaim */
	{ &vop_lock_desc, spec_lock },			/* lock */
	{ &vop_unlock_desc, spec_unlock },		/* unlock */
	{ &vop_bmap_desc, spec_bmap },			/* bmap */
	{ &vop_strategy_desc, spec_strategy },		/* strategy */
	{ &vop_print_desc, spec_print },		/* print */
	{ &vop_islocked_desc, spec_islocked },		/* islocked */
	{ &vop_pathconf_desc, spec_pathconf },		/* pathconf */
	{ &vop_advlock_desc, spec_advlock },		/* advlock */
	{ &vop_bwrite_desc, spec_bwrite },		/* bwrite */
	{ &vop_getpages_desc, spec_getpages },		/* getpages */
	{ &vop_putpages_desc, spec_putpages },		/* putpages */
	{ NULL, NULL }
};
const struct vnodeopv_desc spec_vnodeop_opv_desc =
	{ &spec_vnodeop_p, spec_vnodeop_entries };

static kauth_listener_t rawio_listener;

/* Returns true if vnode is /dev/mem or /dev/kmem. */
bool
iskmemvp(struct vnode *vp)
{
	return ((vp->v_type == VCHR) && iskmemdev(vp->v_rdev));
}

/*
 * Returns true if dev is /dev/mem or /dev/kmem.
 */
int
iskmemdev(dev_t dev)
{
	/* mem_no is emitted by config(8) to generated devsw.c */
	extern const int mem_no;

	/* minor 14 is /dev/io on i386 with COMPAT_10 */
	return (major(dev) == mem_no && (minor(dev) < 2 || minor(dev) == 14));
}

static int
rawio_listener_cb(kauth_cred_t cred, kauth_action_t action, void *cookie,
    void *arg0, void *arg1, void *arg2, void *arg3)
{
	int result;

	result = KAUTH_RESULT_DEFER;

	if ((action != KAUTH_DEVICE_RAWIO_SPEC) &&
	    (action != KAUTH_DEVICE_RAWIO_PASSTHRU))
		return result;

	/* Access is mandated by permissions. */
	result = KAUTH_RESULT_ALLOW;

	return result;
}

void
spec_init(void)
{

	rawio_listener = kauth_listen_scope(KAUTH_SCOPE_DEVICE,
	    rawio_listener_cb, NULL);
}

/*
 * Initialize a vnode that represents a device.
 */
void
spec_node_init(vnode_t *vp, dev_t rdev)
{
	specnode_t *sn;
	specdev_t *sd;
	vnode_t *vp2;
	vnode_t **vpp;

	KASSERT(vp->v_type == VBLK || vp->v_type == VCHR);
	KASSERT(vp->v_specnode == NULL);

	/*
	 * Search the hash table for this device.  If known, add a
	 * reference to the device structure.  If not known, create
	 * a new entry to represent the device.  In all cases add
	 * the vnode to the hash table.
	 */
	sn = kmem_alloc(sizeof(*sn), KM_SLEEP);
	if (sn == NULL) {
		/* XXX */
		panic("spec_node_init: unable to allocate memory");
	}
	sd = kmem_alloc(sizeof(*sd), KM_SLEEP);
	if (sd == NULL) {
		/* XXX */
		panic("spec_node_init: unable to allocate memory");
	}
	mutex_enter(&device_lock);
	vpp = &specfs_hash[SPECHASH(rdev)];
	for (vp2 = *vpp; vp2 != NULL; vp2 = vp2->v_specnext) {
		KASSERT(vp2->v_specnode != NULL);
		if (rdev == vp2->v_rdev && vp->v_type == vp2->v_type) {
			break;
		}
	}
	if (vp2 == NULL) {
		/* No existing record, create a new one. */
		sd->sd_rdev = rdev;
		sd->sd_mountpoint = NULL;
		sd->sd_lockf = NULL;
		sd->sd_refcnt = 1;
		sd->sd_opencnt = 0;
		sd->sd_bdevvp = NULL;
		sn->sn_dev = sd;
		sd = NULL;
	} else {
		/* Use the existing record. */
		sn->sn_dev = vp2->v_specnode->sn_dev;
		sn->sn_dev->sd_refcnt++;
	}
	/* Insert vnode into the hash chain. */
	sn->sn_opencnt = 0;
	sn->sn_rdev = rdev;
	sn->sn_gone = false;
	vp->v_specnode = sn;
	vp->v_specnext = *vpp;
	*vpp = vp;
	mutex_exit(&device_lock);

	/* Free the record we allocated if unused. */
	if (sd != NULL) {
		kmem_free(sd, sizeof(*sd));
	}
}

/*
 * Lookup a vnode by device number and return it referenced.
 */
int
spec_node_lookup_by_dev(enum vtype type, dev_t dev, vnode_t **vpp)
{
	int error;
	vnode_t *vp;

	mutex_enter(&device_lock);
	for (vp = specfs_hash[SPECHASH(dev)]; vp; vp = vp->v_specnext) {
		if (type == vp->v_type && dev == vp->v_rdev) {
			mutex_enter(vp->v_interlock);
			/* If clean or being cleaned, then ignore it. */
			if (vdead_check(vp, VDEAD_NOWAIT) == 0)
				break;
			mutex_exit(vp->v_interlock);
		}
	}
	KASSERT(vp == NULL || mutex_owned(vp->v_interlock));
	if (vp == NULL) {
		mutex_exit(&device_lock);
		return ENOENT;
	}
	/*
	 * If it is an opened block device return the opened vnode.
	 */
	if (type == VBLK && vp->v_specnode->sn_dev->sd_bdevvp != NULL) {
		mutex_exit(vp->v_interlock);
		vp = vp->v_specnode->sn_dev->sd_bdevvp;
		mutex_enter(vp->v_interlock);
	}
	mutex_exit(&device_lock);
	error = vget(vp, 0, true /* wait */);
	if (error != 0)
		return error;
	*vpp = vp;

	return 0;
}

/*
 * Lookup a vnode by file system mounted on and return it referenced.
 */
int
spec_node_lookup_by_mount(struct mount *mp, vnode_t **vpp)
{
	int i, error;
	vnode_t *vp, *vq;

	mutex_enter(&device_lock);
	for (i = 0, vq = NULL; i < SPECHSZ && vq == NULL; i++) {
		for (vp = specfs_hash[i]; vp; vp = vp->v_specnext) {
			if (vp->v_type != VBLK)
				continue;
			vq = vp->v_specnode->sn_dev->sd_bdevvp;
			if (vq != NULL &&
			    vq->v_specnode->sn_dev->sd_mountpoint == mp)
				break;
			vq = NULL;
		}
	}
	if (vq == NULL) {
		mutex_exit(&device_lock);
		return ENOENT;
	}
	mutex_enter(vq->v_interlock);
	mutex_exit(&device_lock);
	error = vget(vq, 0, true /* wait */);
	if (error != 0)
		return error;
	*vpp = vq;

	return 0;

}

/*
 * Get the file system mounted on this block device.
 */
struct mount *
spec_node_getmountedfs(vnode_t *devvp)
{
	struct mount *mp;

	KASSERT(devvp->v_type == VBLK);
	mp = devvp->v_specnode->sn_dev->sd_mountpoint;

	return mp;
}

/*
 * Set the file system mounted on this block device.
 */
void
spec_node_setmountedfs(vnode_t *devvp, struct mount *mp)
{

	KASSERT(devvp->v_type == VBLK);
	KASSERT(devvp->v_specnode->sn_dev->sd_mountpoint == NULL || mp == NULL);
	devvp->v_specnode->sn_dev->sd_mountpoint = mp;
}

/*
 * A vnode representing a special device is going away.  Close
 * the device if the vnode holds it open.
 */
void
spec_node_revoke(vnode_t *vp)
{
	specnode_t *sn;
	specdev_t *sd;

	sn = vp->v_specnode;
	sd = sn->sn_dev;

	KASSERT(vp->v_type == VBLK || vp->v_type == VCHR);
	KASSERT(vp->v_specnode != NULL);
	KASSERT(sn->sn_gone == false);

	mutex_enter(&device_lock);
	KASSERT(sn->sn_opencnt <= sd->sd_opencnt);
	if (sn->sn_opencnt != 0) {
		sd->sd_opencnt -= (sn->sn_opencnt - 1);
		sn->sn_opencnt = 1;
		sn->sn_gone = true;
		mutex_exit(&device_lock);

		VOP_CLOSE(vp, FNONBLOCK, NOCRED);

		mutex_enter(&device_lock);
		KASSERT(sn->sn_opencnt == 0);
	}
	mutex_exit(&device_lock);
}

/*
 * A vnode representing a special device is being recycled.
 * Destroy the specfs component.
 */
void
spec_node_destroy(vnode_t *vp)
{
	specnode_t *sn;
	specdev_t *sd;
	vnode_t **vpp, *vp2;
	int refcnt;

	sn = vp->v_specnode;
	sd = sn->sn_dev;

	KASSERT(vp->v_type == VBLK || vp->v_type == VCHR);
	KASSERT(vp->v_specnode != NULL);
	KASSERT(sn->sn_opencnt == 0);

	mutex_enter(&device_lock);
	/* Remove from the hash and destroy the node. */
	vpp = &specfs_hash[SPECHASH(vp->v_rdev)];
	for (vp2 = *vpp;; vp2 = vp2->v_specnext) {
		if (vp2 == NULL) {
			panic("spec_node_destroy: corrupt hash");
		}
		if (vp2 == vp) {
			KASSERT(vp == *vpp);
			*vpp = vp->v_specnext;
			break;
		}
		if (vp2->v_specnext == vp) {
			vp2->v_specnext = vp->v_specnext;
			break;
		}
	}
	sn = vp->v_specnode;
	vp->v_specnode = NULL;
	refcnt = sd->sd_refcnt--;
	KASSERT(refcnt > 0);
	mutex_exit(&device_lock);

	/* If the device is no longer in use, destroy our record. */
	if (refcnt == 1) {
		KASSERT(sd->sd_opencnt == 0);
		KASSERT(sd->sd_bdevvp == NULL);
		kmem_free(sd, sizeof(*sd));
	}
	kmem_free(sn, sizeof(*sn));
}

/*
 * Trivial lookup routine that always fails.
 */
int
spec_lookup(void *v)
{
	struct vop_lookup_v2_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap = v;

	*ap->a_vpp = NULL;
	return (ENOTDIR);
}

/*
 * Open a special file.
 */
/* ARGSUSED */
int
spec_open(void *v)
{
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct lwp *l;
	struct vnode *vp;
	dev_t dev;
	int error;
	struct partinfo pi;
	enum kauth_device_req req;
	specnode_t *sn;
	specdev_t *sd;

	u_int gen;
	const char *name;
	
	l = curlwp;
	vp = ap->a_vp;
	dev = vp->v_rdev;
	sn = vp->v_specnode;
	sd = sn->sn_dev;
	name = NULL;
	gen = 0;
	
	/*
	 * Don't allow open if fs is mounted -nodev.
	 */
	if (vp->v_mount && (vp->v_mount->mnt_flag & MNT_NODEV))
		return (ENXIO);

	switch (ap->a_mode & (FREAD | FWRITE)) {
	case FREAD | FWRITE:
		req = KAUTH_REQ_DEVICE_RAWIO_SPEC_RW;
		break;
	case FWRITE:
		req = KAUTH_REQ_DEVICE_RAWIO_SPEC_WRITE;
		break;
	default:
		req = KAUTH_REQ_DEVICE_RAWIO_SPEC_READ;
		break;
	}

	switch (vp->v_type) {
	case VCHR:
		error = kauth_authorize_device_spec(ap->a_cred, req, vp);
		if (error != 0)
			return (error);

		/*
		 * Character devices can accept opens from multiple
		 * vnodes.
		 */
		mutex_enter(&device_lock);
		if (sn->sn_gone) {
			mutex_exit(&device_lock);
			return (EBADF);
		}
		sd->sd_opencnt++;
		sn->sn_opencnt++;
		mutex_exit(&device_lock);
		if (cdev_type(dev) == D_TTY)
			vp->v_vflag |= VV_ISTTY;
		VOP_UNLOCK(vp);
		do {
			const struct cdevsw *cdev;

			gen = module_gen;
			error = cdev_open(dev, ap->a_mode, S_IFCHR, l);
			if (error != ENXIO)
				break;
			
			/* Check if we already have a valid driver */
			mutex_enter(&device_lock);
			cdev = cdevsw_lookup(dev);
			mutex_exit(&device_lock);
			if (cdev != NULL)
				break;

			/* Get device name from devsw_conv array */
			if ((name = cdevsw_getname(major(dev))) == NULL)
				break;
			
			/* Try to autoload device module */
			(void) module_autoload(name, MODULE_CLASS_DRIVER);
		} while (gen != module_gen);

		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
		break;

	case VBLK:
		error = kauth_authorize_device_spec(ap->a_cred, req, vp);
		if (error != 0)
			return (error);

		/*
		 * For block devices, permit only one open.  The buffer
		 * cache cannot remain self-consistent with multiple
		 * vnodes holding a block device open.
		 */
		mutex_enter(&device_lock);
		if (sn->sn_gone) {
			mutex_exit(&device_lock);
			return (EBADF);
		}
		if (sd->sd_opencnt != 0) {
			mutex_exit(&device_lock);
			return EBUSY;
		}
		sn->sn_opencnt = 1;
		sd->sd_opencnt = 1;
		sd->sd_bdevvp = vp;
		mutex_exit(&device_lock);
		do {
			const struct bdevsw *bdev;

			gen = module_gen;
			error = bdev_open(dev, ap->a_mode, S_IFBLK, l);
			if (error != ENXIO)
				break;

			/* Check if we already have a valid driver */
			mutex_enter(&device_lock);
			bdev = bdevsw_lookup(dev);
			mutex_exit(&device_lock);
			if (bdev != NULL)
				break;

			/* Get device name from devsw_conv array */
			if ((name = bdevsw_getname(major(dev))) == NULL)
				break;

			VOP_UNLOCK(vp);

                        /* Try to autoload device module */
			(void) module_autoload(name, MODULE_CLASS_DRIVER);
			
			vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
		} while (gen != module_gen);

		break;

	case VNON:
	case VLNK:
	case VDIR:
	case VREG:
	case VBAD:
	case VFIFO:
	case VSOCK:
	default:
		return 0;
	}

	mutex_enter(&device_lock);
	if (sn->sn_gone) {
		if (error == 0)
			error = EBADF;
	} else if (error != 0) {
		sd->sd_opencnt--;
		sn->sn_opencnt--;
		if (vp->v_type == VBLK)
			sd->sd_bdevvp = NULL;

	}
	mutex_exit(&device_lock);

	if (cdev_type(dev) != D_DISK || error != 0)
		return error;

	if (vp->v_type == VCHR)
		error = cdev_ioctl(vp->v_rdev, DIOCGPART, &pi, FREAD, curlwp);
	else
		error = bdev_ioctl(vp->v_rdev, DIOCGPART, &pi, FREAD, curlwp);
	if (error == 0)
		uvm_vnp_setsize(vp,
		    (voff_t)pi.disklab->d_secsize * pi.part->p_size);
	return 0;
}

/*
 * Vnode op for read
 */
/* ARGSUSED */
int
spec_read(void *v)
{
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
 	struct lwp *l = curlwp;
	struct buf *bp;
	daddr_t bn;
	int bsize, bscale;
	struct partinfo dpart;
	int n, on;
	int error = 0;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_READ)
		panic("spec_read mode");
	if (&uio->uio_vmspace->vm_map != kernel_map &&
	    uio->uio_vmspace != curproc->p_vmspace)
		panic("spec_read proc");
#endif
	if (uio->uio_resid == 0)
		return (0);

	switch (vp->v_type) {

	case VCHR:
		VOP_UNLOCK(vp);
		error = cdev_read(vp->v_rdev, uio, ap->a_ioflag);
		vn_lock(vp, LK_SHARED | LK_RETRY);
		return (error);

	case VBLK:
		KASSERT(vp == vp->v_specnode->sn_dev->sd_bdevvp);
		if (uio->uio_offset < 0)
			return (EINVAL);
		bsize = BLKDEV_IOSIZE;

		/*
		 * dholland 20130616: XXX this logic should not be
		 * here. It is here because the old buffer cache
		 * demands that all accesses to the same blocks need
		 * to be the same size; but it only works for FFS and
		 * nowadays I think it'll fail silently if the size
		 * info in the disklabel is wrong. (Or missing.) The
		 * buffer cache needs to be smarter; or failing that
		 * we need a reliable way here to get the right block
		 * size; or a reliable way to guarantee that (a) the
		 * fs is not mounted when we get here and (b) any
		 * buffers generated here will get purged when the fs
		 * does get mounted.
		 */
		if (bdev_ioctl(vp->v_rdev, DIOCGPART, &dpart, FREAD, l) == 0) {
			if (dpart.part->p_fstype == FS_BSDFFS &&
			    dpart.part->p_frag != 0 && dpart.part->p_fsize != 0)
				bsize = dpart.part->p_frag *
				    dpart.part->p_fsize;
		}

		bscale = bsize >> DEV_BSHIFT;
		do {
			bn = (uio->uio_offset >> DEV_BSHIFT) &~ (bscale - 1);
			on = uio->uio_offset % bsize;
			n = min((unsigned)(bsize - on), uio->uio_resid);
			error = bread(vp, bn, bsize, 0, &bp);
			if (error) {
				return (error);
			}
			n = min(n, bsize - bp->b_resid);
			error = uiomove((char *)bp->b_data + on, n, uio);
			brelse(bp, 0);
		} while (error == 0 && uio->uio_resid > 0 && n != 0);
		return (error);

	default:
		panic("spec_read type");
	}
	/* NOTREACHED */
}

/*
 * Vnode op for write
 */
/* ARGSUSED */
int
spec_write(void *v)
{
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	struct lwp *l = curlwp;
	struct buf *bp;
	daddr_t bn;
	int bsize, bscale;
	struct partinfo dpart;
	int n, on;
	int error = 0;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_WRITE)
		panic("spec_write mode");
	if (&uio->uio_vmspace->vm_map != kernel_map &&
	    uio->uio_vmspace != curproc->p_vmspace)
		panic("spec_write proc");
#endif

	switch (vp->v_type) {

	case VCHR:
		VOP_UNLOCK(vp);
		error = cdev_write(vp->v_rdev, uio, ap->a_ioflag);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
		return (error);

	case VBLK:
		KASSERT(vp == vp->v_specnode->sn_dev->sd_bdevvp);
		if (uio->uio_resid == 0)
			return (0);
		if (uio->uio_offset < 0)
			return (EINVAL);
		bsize = BLKDEV_IOSIZE;
		if (bdev_ioctl(vp->v_rdev, DIOCGPART, &dpart, FREAD, l) == 0) {
			if (dpart.part->p_fstype == FS_BSDFFS &&
			    dpart.part->p_frag != 0 && dpart.part->p_fsize != 0)
				bsize = dpart.part->p_frag *
				    dpart.part->p_fsize;
		}
		bscale = bsize >> DEV_BSHIFT;
		do {
			bn = (uio->uio_offset >> DEV_BSHIFT) &~ (bscale - 1);
			on = uio->uio_offset % bsize;
			n = min((unsigned)(bsize - on), uio->uio_resid);
			if (n == bsize)
				bp = getblk(vp, bn, bsize, 0, 0);
			else
				error = bread(vp, bn, bsize, B_MODIFY, &bp);
			if (error) {
				return (error);
			}
			n = min(n, bsize - bp->b_resid);
			error = uiomove((char *)bp->b_data + on, n, uio);
			if (error)
				brelse(bp, 0);
			else {
				if (n + on == bsize)
					bawrite(bp);
				else
					bdwrite(bp);
				error = bp->b_error;
			}
		} while (error == 0 && uio->uio_resid > 0 && n != 0);
		return (error);

	default:
		panic("spec_write type");
	}
	/* NOTREACHED */
}

/*
 * fdiscard, which on disk devices becomes TRIM.
 */
int
spec_fdiscard(void *v)
{
	struct vop_fdiscard_args /* {
		struct vnode *a_vp;
		off_t a_pos;
		off_t a_len;
	} */ *ap = v;
	struct vnode *vp;
	dev_t dev;

	vp = ap->a_vp;
	dev = NODEV;

	mutex_enter(vp->v_interlock);
	if (vdead_check(vp, VDEAD_NOWAIT) == 0 && vp->v_specnode != NULL) {
		dev = vp->v_rdev;
	}
	mutex_exit(vp->v_interlock);

	if (dev == NODEV) {
		return ENXIO;
	}

	switch (vp->v_type) {
	    case VCHR:
		// this is not stored for character devices
		//KASSERT(vp == vp->v_specnode->sn_dev->sd_cdevvp);
		return cdev_discard(dev, ap->a_pos, ap->a_len);
	    case VBLK:
		KASSERT(vp == vp->v_specnode->sn_dev->sd_bdevvp);
		return bdev_discard(dev, ap->a_pos, ap->a_len);
	    default:
		panic("spec_fdiscard: not a device\n");
	}
}

/*
 * Device ioctl operation.
 */
/* ARGSUSED */
int
spec_ioctl(void *v)
{
	struct vop_ioctl_args /* {
		struct vnode *a_vp;
		u_long a_command;
		void  *a_data;
		int  a_fflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp;
	dev_t dev;

	/*
	 * Extract all the info we need from the vnode, taking care to
	 * avoid a race with VOP_REVOKE().
	 */

	vp = ap->a_vp;
	dev = NODEV;
	mutex_enter(vp->v_interlock);
	if (vdead_check(vp, VDEAD_NOWAIT) == 0 && vp->v_specnode) {
		dev = vp->v_rdev;
	}
	mutex_exit(vp->v_interlock);
	if (dev == NODEV) {
		return ENXIO;
	}

	switch (vp->v_type) {

	case VCHR:
		return cdev_ioctl(dev, ap->a_command, ap->a_data,
		    ap->a_fflag, curlwp);

	case VBLK:
		KASSERT(vp == vp->v_specnode->sn_dev->sd_bdevvp);
		return bdev_ioctl(dev, ap->a_command, ap->a_data,
		   ap->a_fflag, curlwp);

	default:
		panic("spec_ioctl");
		/* NOTREACHED */
	}
}

/* ARGSUSED */
int
spec_poll(void *v)
{
	struct vop_poll_args /* {
		struct vnode *a_vp;
		int a_events;
	} */ *ap = v;
	struct vnode *vp;
	dev_t dev;

	/*
	 * Extract all the info we need from the vnode, taking care to
	 * avoid a race with VOP_REVOKE().
	 */

	vp = ap->a_vp;
	dev = NODEV;
	mutex_enter(vp->v_interlock);
	if (vdead_check(vp, VDEAD_NOWAIT) == 0 && vp->v_specnode) {
		dev = vp->v_rdev;
	}
	mutex_exit(vp->v_interlock);
	if (dev == NODEV) {
		return POLLERR;
	}

	switch (vp->v_type) {

	case VCHR:
		return cdev_poll(dev, ap->a_events, curlwp);

	default:
		return (genfs_poll(v));
	}
}

/* ARGSUSED */
int
spec_kqfilter(void *v)
{
	struct vop_kqfilter_args /* {
		struct vnode	*a_vp;
		struct proc	*a_kn;
	} */ *ap = v;
	dev_t dev;

	switch (ap->a_vp->v_type) {

	case VCHR:
		dev = ap->a_vp->v_rdev;
		return cdev_kqfilter(dev, ap->a_kn);
	default:
		/*
		 * Block devices don't support kqfilter, and refuse it
		 * for any other files (like those vflush()ed) too.
		 */
		return (EOPNOTSUPP);
	}
}

/*
 * Allow mapping of only D_DISK.  This is called only for VBLK.
 */
int
spec_mmap(void *v)
{
	struct vop_mmap_args /* {
		struct vnode *a_vp;
		vm_prot_t a_prot;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	KASSERT(vp->v_type == VBLK);
	if (bdev_type(vp->v_rdev) != D_DISK)
		return EINVAL;

	return 0;
}

/*
 * Synch buffers associated with a block device
 */
/* ARGSUSED */
int
spec_fsync(void *v)
{
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		kauth_cred_t a_cred;
		int  a_flags;
		off_t offlo;
		off_t offhi;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct mount *mp;
	int error;

	if (vp->v_type == VBLK) {
		if ((mp = spec_node_getmountedfs(vp)) != NULL) {
			error = VFS_FSYNC(mp, vp, ap->a_flags);
			if (error != EOPNOTSUPP)
				return error;
		}
		return vflushbuf(vp, ap->a_flags);
	}
	return (0);
}

/*
 * Just call the device strategy routine
 */
int
spec_strategy(void *v)
{
	struct vop_strategy_args /* {
		struct vnode *a_vp;
		struct buf *a_bp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct buf *bp = ap->a_bp;
	int error;

	KASSERT(vp == vp->v_specnode->sn_dev->sd_bdevvp);

	error = 0;
	bp->b_dev = vp->v_rdev;

	if (!(bp->b_flags & B_READ))
		error = fscow_run(bp, false);

	if (error) {
		bp->b_error = error;
		bp->b_resid = bp->b_bcount;
		biodone(bp);
		return (error);
	}

	bdev_strategy(bp);

	return (0);
}

int
spec_inactive(void *v)
{
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		struct bool *a_recycle;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	KASSERT(vp->v_mount == dead_rootmount);
	*ap->a_recycle = true;
	VOP_UNLOCK(vp);
	return 0;
}

int
spec_reclaim(void *v)
{
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	KASSERT(vp->v_mount == dead_rootmount);
	vcache_remove(vp->v_mount, &vp->v_interlock, sizeof(vp->v_interlock));
	return 0;
}

/*
 * This is a noop, simply returning what one has been given.
 */
int
spec_bmap(void *v)
{
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
	} */ *ap = v;

	if (ap->a_vpp != NULL)
		*ap->a_vpp = ap->a_vp;
	if (ap->a_bnp != NULL)
		*ap->a_bnp = ap->a_bn;
	if (ap->a_runp != NULL)
		*ap->a_runp = (MAXBSIZE >> DEV_BSHIFT) - 1;
	return (0);
}

/*
 * Device close routine
 */
/* ARGSUSED */
int
spec_close(void *v)
{
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct session *sess;
	dev_t dev = vp->v_rdev;
	int flags = ap->a_fflag;
	int mode, error, count;
	specnode_t *sn;
	specdev_t *sd;

	mutex_enter(vp->v_interlock);
	sn = vp->v_specnode;
	sd = sn->sn_dev;
	/*
	 * If we're going away soon, make this non-blocking.
	 * Also ensures that we won't wedge in vn_lock below.
	 */
	if (vdead_check(vp, VDEAD_NOWAIT) != 0)
		flags |= FNONBLOCK;
	mutex_exit(vp->v_interlock);

	switch (vp->v_type) {

	case VCHR:
		/*
		 * Hack: a tty device that is a controlling terminal
		 * has a reference from the session structure.  We
		 * cannot easily tell that a character device is a
		 * controlling terminal, unless it is the closing
		 * process' controlling terminal.  In that case, if the
		 * open count is 1 release the reference from the
		 * session.  Also, remove the link from the tty back to
		 * the session and pgrp.
		 *
		 * XXX V. fishy.
		 */
		mutex_enter(proc_lock);
		sess = curlwp->l_proc->p_session;
		if (sn->sn_opencnt == 1 && vp == sess->s_ttyvp) {
			mutex_spin_enter(&tty_lock);
			sess->s_ttyvp = NULL;
			if (sess->s_ttyp->t_session != NULL) {
				sess->s_ttyp->t_pgrp = NULL;
				sess->s_ttyp->t_session = NULL;
				mutex_spin_exit(&tty_lock);
				/* Releases proc_lock. */
				proc_sessrele(sess);
			} else {
				mutex_spin_exit(&tty_lock);
				if (sess->s_ttyp->t_pgrp != NULL)
					panic("spec_close: spurious pgrp ref");
				mutex_exit(proc_lock);
			}
			vrele(vp);
		} else
			mutex_exit(proc_lock);

		/*
		 * If the vnode is locked, then we are in the midst
		 * of forcably closing the device, otherwise we only
		 * close on last reference.
		 */
		mode = S_IFCHR;
		break;

	case VBLK:
		KASSERT(vp == vp->v_specnode->sn_dev->sd_bdevvp);
		/*
		 * On last close of a block device (that isn't mounted)
		 * we must invalidate any in core blocks, so that
		 * we can, for instance, change floppy disks.
		 */
		error = vinvalbuf(vp, V_SAVE, ap->a_cred, curlwp, 0, 0);
		if (error)
			return (error);
		/*
		 * We do not want to really close the device if it
		 * is still in use unless we are trying to close it
		 * forcibly. Since every use (buffer, vnode, swap, cmap)
		 * holds a reference to the vnode, and because we mark
		 * any other vnodes that alias this device, when the
		 * sum of the reference counts on all the aliased
		 * vnodes descends to one, we are on last close.
		 */
		mode = S_IFBLK;
		break;

	default:
		panic("spec_close: not special");
	}

	mutex_enter(&device_lock);
	sn->sn_opencnt--;
	count = --sd->sd_opencnt;
	if (vp->v_type == VBLK)
		sd->sd_bdevvp = NULL;
	mutex_exit(&device_lock);

	if (count != 0)
		return 0;

	/*
	 * If we're able to block, release the vnode lock & reacquire. We
	 * might end up sleeping for someone else who wants our queues. They
	 * won't get them if we hold the vnode locked.
	 */
	if (!(flags & FNONBLOCK))
		VOP_UNLOCK(vp);

	if (vp->v_type == VBLK)
		error = bdev_close(dev, flags, mode, curlwp);
	else
		error = cdev_close(dev, flags, mode, curlwp);

	if (!(flags & FNONBLOCK))
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);

	return (error);
}

/*
 * Print out the contents of a special device vnode.
 */
int
spec_print(void *v)
{
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap = v;

	printf("dev %llu, %llu\n", (unsigned long long)major(ap->a_vp->v_rdev),
	    (unsigned long long)minor(ap->a_vp->v_rdev));
	return 0;
}

/*
 * Return POSIX pathconf information applicable to special devices.
 */
int
spec_pathconf(void *v)
{
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		register_t *a_retval;
	} */ *ap = v;

	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = LINK_MAX;
		return (0);
	case _PC_MAX_CANON:
		*ap->a_retval = MAX_CANON;
		return (0);
	case _PC_MAX_INPUT:
		*ap->a_retval = MAX_INPUT;
		return (0);
	case _PC_PIPE_BUF:
		*ap->a_retval = PIPE_BUF;
		return (0);
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		return (0);
	case _PC_VDISABLE:
		*ap->a_retval = _POSIX_VDISABLE;
		return (0);
	case _PC_SYNC_IO:
		*ap->a_retval = 1;
		return (0);
	default:
		return (EINVAL);
	}
	/* NOTREACHED */
}

/*
 * Advisory record locking support.
 */
int
spec_advlock(void *v)
{
	struct vop_advlock_args /* {
		struct vnode *a_vp;
		void *a_id;
		int a_op;
		struct flock *a_fl;
		int a_flags;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	return lf_advlock(ap, &vp->v_speclockf, (off_t)0);
}
