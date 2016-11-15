/*	$NetBSD: fss.c,v 1.93 2015/08/20 14:40:17 christos Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Juergen Hannken-Illjes.
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
 * File system snapshot disk driver.
 *
 * Block/character interface to the snapshot of a mounted file system.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: fss.c,v 1.93 2015/08/20 14:40:17 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/disklabel.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/conf.h>
#include <sys/kthread.h>
#include <sys/fstrans.h>
#include <sys/vfs_syscalls.h>		/* For do_sys_unlink(). */

#include <miscfs/specfs/specdev.h>

#include <dev/fssvar.h>

#include <uvm/uvm.h>

#include "ioconf.h"

dev_type_open(fss_open);
dev_type_close(fss_close);
dev_type_read(fss_read);
dev_type_write(fss_write);
dev_type_ioctl(fss_ioctl);
dev_type_strategy(fss_strategy);
dev_type_dump(fss_dump);
dev_type_size(fss_size);

static void fss_unmount_hook(struct mount *);
static int fss_copy_on_write(void *, struct buf *, bool);
static inline void fss_error(struct fss_softc *, const char *);
static int fss_create_files(struct fss_softc *, struct fss_set *,
    off_t *, struct lwp *);
static int fss_create_snapshot(struct fss_softc *, struct fss_set *,
    struct lwp *);
static int fss_delete_snapshot(struct fss_softc *, struct lwp *);
static int fss_softc_alloc(struct fss_softc *);
static void fss_softc_free(struct fss_softc *);
static int fss_read_cluster(struct fss_softc *, u_int32_t);
static void fss_bs_thread(void *);
static int fss_bs_io(struct fss_softc *, fss_io_type,
    u_int32_t, off_t, int, void *);
static u_int32_t *fss_bs_indir(struct fss_softc *, u_int32_t);

static kmutex_t fss_device_lock;	/* Protect all units. */
static int fss_num_attached = 0;	/* Number of attached devices. */
static struct vfs_hooks fss_vfs_hooks = {
	.vh_unmount = fss_unmount_hook
};

const struct bdevsw fss_bdevsw = {
	.d_open = fss_open,
	.d_close = fss_close,
	.d_strategy = fss_strategy,
	.d_ioctl = fss_ioctl,
	.d_dump = fss_dump,
	.d_psize = fss_size,
	.d_discard = nodiscard,
	.d_flag = D_DISK | D_MPSAFE
};

const struct cdevsw fss_cdevsw = {
	.d_open = fss_open,
	.d_close = fss_close,
	.d_read = fss_read,
	.d_write = fss_write,
	.d_ioctl = fss_ioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_DISK | D_MPSAFE
};

static int fss_match(device_t, cfdata_t, void *);
static void fss_attach(device_t, device_t, void *);
static int fss_detach(device_t, int);

CFATTACH_DECL_NEW(fss, sizeof(struct fss_softc),
    fss_match, fss_attach, fss_detach, NULL);
extern struct cfdriver fss_cd;

void
fssattach(int num)
{

	mutex_init(&fss_device_lock, MUTEX_DEFAULT, IPL_NONE);
	if (config_cfattach_attach(fss_cd.cd_name, &fss_ca))
		aprint_error("%s: unable to register\n", fss_cd.cd_name);
}

static int
fss_match(device_t self, cfdata_t cfdata, void *aux)
{
	return 1;
}

static void
fss_attach(device_t parent, device_t self, void *aux)
{
	struct fss_softc *sc = device_private(self);

	sc->sc_dev = self;
	sc->sc_bdev = NODEV;
	mutex_init(&sc->sc_slock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&sc->sc_work_cv, "fssbs");
	cv_init(&sc->sc_cache_cv, "cowwait");
	bufq_alloc(&sc->sc_bufq, "fcfs", 0);
	sc->sc_dkdev = malloc(sizeof(*sc->sc_dkdev), M_DEVBUF, M_WAITOK);
	sc->sc_dkdev->dk_info = NULL;
	disk_init(sc->sc_dkdev, device_xname(self), NULL);
	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");

	if (fss_num_attached++ == 0)
		vfs_hooks_attach(&fss_vfs_hooks);
}

static int
fss_detach(device_t self, int flags)
{
	struct fss_softc *sc = device_private(self);

	if (sc->sc_flags & FSS_ACTIVE)
		return EBUSY;

	if (--fss_num_attached == 0)
		vfs_hooks_detach(&fss_vfs_hooks);

	pmf_device_deregister(self);
	mutex_destroy(&sc->sc_slock);
	mutex_destroy(&sc->sc_lock);
	cv_destroy(&sc->sc_work_cv);
	cv_destroy(&sc->sc_cache_cv);
	bufq_drain(sc->sc_bufq);
	bufq_free(sc->sc_bufq);
	disk_destroy(sc->sc_dkdev);
	free(sc->sc_dkdev, M_DEVBUF);

	return 0;
}

int
fss_open(dev_t dev, int flags, int mode, struct lwp *l)
{
	int mflag;
	cfdata_t cf;
	struct fss_softc *sc;

	mflag = (mode == S_IFCHR ? FSS_CDEV_OPEN : FSS_BDEV_OPEN);

	mutex_enter(&fss_device_lock);

	sc = device_lookup_private(&fss_cd, minor(dev));
	if (sc == NULL) {
		cf = malloc(sizeof(*cf), M_DEVBUF, M_WAITOK);
		cf->cf_name = fss_cd.cd_name;
		cf->cf_atname = fss_cd.cd_name;
		cf->cf_unit = minor(dev);
		cf->cf_fstate = FSTATE_STAR;
		sc = device_private(config_attach_pseudo(cf));
		if (sc == NULL) {
			mutex_exit(&fss_device_lock);
			return ENOMEM;
		}
	}

	mutex_enter(&sc->sc_slock);

	sc->sc_flags |= mflag;

	mutex_exit(&sc->sc_slock);
	mutex_exit(&fss_device_lock);

	return 0;
}

int
fss_close(dev_t dev, int flags, int mode, struct lwp *l)
{
	int mflag, error;
	cfdata_t cf;
	struct fss_softc *sc = device_lookup_private(&fss_cd, minor(dev));

	mflag = (mode == S_IFCHR ? FSS_CDEV_OPEN : FSS_BDEV_OPEN);
	error = 0;

	mutex_enter(&fss_device_lock);
restart:
	mutex_enter(&sc->sc_slock);
	if ((sc->sc_flags & (FSS_CDEV_OPEN|FSS_BDEV_OPEN)) != mflag) {
		sc->sc_flags &= ~mflag;
		mutex_exit(&sc->sc_slock);
		mutex_exit(&fss_device_lock);
		return 0;
	}
	if ((sc->sc_flags & FSS_ACTIVE) != 0 &&
	    (sc->sc_uflags & FSS_UNCONFIG_ON_CLOSE) != 0) {
		sc->sc_uflags &= ~FSS_UNCONFIG_ON_CLOSE;
		mutex_exit(&sc->sc_slock);
		error = fss_ioctl(dev, FSSIOCCLR, NULL, FWRITE, l);
		goto restart;
	}
	if ((sc->sc_flags & FSS_ACTIVE) != 0) {
		mutex_exit(&sc->sc_slock);
		mutex_exit(&fss_device_lock);
		return error;
	}

	KASSERT((sc->sc_flags & FSS_ACTIVE) == 0);
	KASSERT((sc->sc_flags & (FSS_CDEV_OPEN|FSS_BDEV_OPEN)) == mflag);
	mutex_exit(&sc->sc_slock);
	cf = device_cfdata(sc->sc_dev);
	error = config_detach(sc->sc_dev, DETACH_QUIET);
	if (! error)
		free(cf, M_DEVBUF);
	mutex_exit(&fss_device_lock);

	return error;
}

void
fss_strategy(struct buf *bp)
{
	const bool write = ((bp->b_flags & B_READ) != B_READ);
	struct fss_softc *sc = device_lookup_private(&fss_cd, minor(bp->b_dev));

	mutex_enter(&sc->sc_slock);

	if (write || !FSS_ISVALID(sc)) {

		mutex_exit(&sc->sc_slock);

		bp->b_error = (write ? EROFS : ENXIO);
		bp->b_resid = bp->b_bcount;
		biodone(bp);
		return;
	}

	bp->b_rawblkno = bp->b_blkno;
	bufq_put(sc->sc_bufq, bp);
	cv_signal(&sc->sc_work_cv);

	mutex_exit(&sc->sc_slock);
}

int
fss_read(dev_t dev, struct uio *uio, int flags)
{
	return physio(fss_strategy, NULL, dev, B_READ, minphys, uio);
}

int
fss_write(dev_t dev, struct uio *uio, int flags)
{
	return physio(fss_strategy, NULL, dev, B_WRITE, minphys, uio);
}

int
fss_ioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	int error;
	struct fss_softc *sc = device_lookup_private(&fss_cd, minor(dev));
	struct fss_set _fss;
	struct fss_set *fss = (struct fss_set *)data;
	struct fss_set50 *fss50 = (struct fss_set50 *)data;
	struct fss_get *fsg = (struct fss_get *)data;
#ifndef _LP64
	struct fss_get50 *fsg50 = (struct fss_get50 *)data;
#endif

	switch (cmd) {
	case FSSIOCSET50:
		fss = &_fss;
		fss->fss_mount = fss50->fss_mount;
		fss->fss_bstore = fss50->fss_bstore;
		fss->fss_csize = fss50->fss_csize;
		fss->fss_flags = 0;
		/* Fall through */
	case FSSIOCSET:
		mutex_enter(&sc->sc_lock);
		if ((flag & FWRITE) == 0)
			error = EPERM;
		else if ((sc->sc_flags & FSS_ACTIVE) != 0)
			error = EBUSY;
		else
			error = fss_create_snapshot(sc, fss, l);
		if (error == 0)
			sc->sc_uflags = fss->fss_flags;
		mutex_exit(&sc->sc_lock);
		break;

	case FSSIOCCLR:
		mutex_enter(&sc->sc_lock);
		if ((flag & FWRITE) == 0)
			error = EPERM;
		else if ((sc->sc_flags & FSS_ACTIVE) == 0)
			error = ENXIO;
		else
			error = fss_delete_snapshot(sc, l);
		mutex_exit(&sc->sc_lock);
		break;

#ifndef _LP64
	case FSSIOCGET50:
		mutex_enter(&sc->sc_lock);
		switch (sc->sc_flags & (FSS_PERSISTENT | FSS_ACTIVE)) {
		case FSS_ACTIVE:
			memcpy(fsg50->fsg_mount, sc->sc_mntname, MNAMELEN);
			fsg50->fsg_csize = FSS_CLSIZE(sc);
			timeval_to_timeval50(&sc->sc_time, &fsg50->fsg_time);
			fsg50->fsg_mount_size = sc->sc_clcount;
			fsg50->fsg_bs_size = sc->sc_clnext;
			error = 0;
			break;
		case FSS_PERSISTENT | FSS_ACTIVE:
			memcpy(fsg50->fsg_mount, sc->sc_mntname, MNAMELEN);
			fsg50->fsg_csize = 0;
			timeval_to_timeval50(&sc->sc_time, &fsg50->fsg_time);
			fsg50->fsg_mount_size = 0;
			fsg50->fsg_bs_size = 0;
			error = 0;
			break;
		default:
			error = ENXIO;
			break;
		}
		mutex_exit(&sc->sc_lock);
		break;
#endif /* _LP64 */

	case FSSIOCGET:
		mutex_enter(&sc->sc_lock);
		switch (sc->sc_flags & (FSS_PERSISTENT | FSS_ACTIVE)) {
		case FSS_ACTIVE:
			memcpy(fsg->fsg_mount, sc->sc_mntname, MNAMELEN);
			fsg->fsg_csize = FSS_CLSIZE(sc);
			fsg->fsg_time = sc->sc_time;
			fsg->fsg_mount_size = sc->sc_clcount;
			fsg->fsg_bs_size = sc->sc_clnext;
			error = 0;
			break;
		case FSS_PERSISTENT | FSS_ACTIVE:
			memcpy(fsg->fsg_mount, sc->sc_mntname, MNAMELEN);
			fsg->fsg_csize = 0;
			fsg->fsg_time = sc->sc_time;
			fsg->fsg_mount_size = 0;
			fsg->fsg_bs_size = 0;
			error = 0;
			break;
		default:
			error = ENXIO;
			break;
		}
		mutex_exit(&sc->sc_lock);
		break;

	case FSSIOFSET:
		mutex_enter(&sc->sc_slock);
		sc->sc_uflags = *(int *)data;
		mutex_exit(&sc->sc_slock);
		error = 0;
		break;

	case FSSIOFGET:
		mutex_enter(&sc->sc_slock);
		*(int *)data = sc->sc_uflags;
		mutex_exit(&sc->sc_slock);
		error = 0;
		break;

	default:
		error = EINVAL;
		break;
	}

	return error;
}

int
fss_size(dev_t dev)
{
	return -1;
}

int
fss_dump(dev_t dev, daddr_t blkno, void *va,
    size_t size)
{
	return EROFS;
}

/*
 * An error occurred reading or writing the snapshot or backing store.
 * If it is the first error log to console.
 * The caller holds the mutex.
 */
static inline void
fss_error(struct fss_softc *sc, const char *msg)
{

	if ((sc->sc_flags & (FSS_ACTIVE|FSS_ERROR)) == FSS_ACTIVE)
		aprint_error_dev(sc->sc_dev, "snapshot invalid: %s\n", msg);
	if ((sc->sc_flags & FSS_ACTIVE) == FSS_ACTIVE)
		sc->sc_flags |= FSS_ERROR;
}

/*
 * Allocate the variable sized parts of the softc and
 * fork the kernel thread.
 *
 * The fields sc_clcount, sc_clshift, sc_cache_size and sc_indir_size
 * must be initialized.
 */
static int
fss_softc_alloc(struct fss_softc *sc)
{
	int i, error;

	if ((sc->sc_flags & FSS_PERSISTENT) == 0) {
		sc->sc_copied =
		    kmem_zalloc(howmany(sc->sc_clcount, NBBY), KM_SLEEP);
		if (sc->sc_copied == NULL)
			return(ENOMEM);

		sc->sc_cache = kmem_alloc(sc->sc_cache_size *
		    sizeof(struct fss_cache), KM_SLEEP);
		if (sc->sc_cache == NULL)
			return(ENOMEM);

		for (i = 0; i < sc->sc_cache_size; i++) {
			sc->sc_cache[i].fc_type = FSS_CACHE_FREE;
			sc->sc_cache[i].fc_data =
			    kmem_alloc(FSS_CLSIZE(sc), KM_SLEEP);
			if (sc->sc_cache[i].fc_data == NULL)
				return(ENOMEM);
			cv_init(&sc->sc_cache[i].fc_state_cv, "cowwait1");
		}

		sc->sc_indir_valid =
		    kmem_zalloc(howmany(sc->sc_indir_size, NBBY), KM_SLEEP);
		if (sc->sc_indir_valid == NULL)
			return(ENOMEM);

		sc->sc_indir_data = kmem_zalloc(FSS_CLSIZE(sc), KM_SLEEP);
		if (sc->sc_indir_data == NULL)
			return(ENOMEM);
	} else {
		sc->sc_copied = NULL;
		sc->sc_cache = NULL;
		sc->sc_indir_valid = NULL;
		sc->sc_indir_data = NULL;
	}

	sc->sc_flags |= FSS_BS_THREAD;
	if ((error = kthread_create(PRI_BIO, KTHREAD_MUSTJOIN, NULL,
	    fss_bs_thread, sc, &sc->sc_bs_lwp,
	    "%s", device_xname(sc->sc_dev))) != 0) {
		sc->sc_flags &= ~FSS_BS_THREAD;
		return error;
	}

	disk_attach(sc->sc_dkdev);

	return 0;
}

/*
 * Free the variable sized parts of the softc.
 */
static void
fss_softc_free(struct fss_softc *sc)
{
	int i;

	if ((sc->sc_flags & FSS_BS_THREAD) != 0) {
		mutex_enter(&sc->sc_slock);
		sc->sc_flags &= ~FSS_BS_THREAD;
		cv_signal(&sc->sc_work_cv);
		mutex_exit(&sc->sc_slock);
		kthread_join(sc->sc_bs_lwp);

		disk_detach(sc->sc_dkdev);
	}

	if (sc->sc_copied != NULL)
		kmem_free(sc->sc_copied, howmany(sc->sc_clcount, NBBY));
	sc->sc_copied = NULL;

	if (sc->sc_cache != NULL) {
		for (i = 0; i < sc->sc_cache_size; i++)
			if (sc->sc_cache[i].fc_data != NULL) {
				cv_destroy(&sc->sc_cache[i].fc_state_cv);
				kmem_free(sc->sc_cache[i].fc_data,
				    FSS_CLSIZE(sc));
			}
		kmem_free(sc->sc_cache,
		    sc->sc_cache_size*sizeof(struct fss_cache));
	}
	sc->sc_cache = NULL;

	if (sc->sc_indir_valid != NULL)
		kmem_free(sc->sc_indir_valid, howmany(sc->sc_indir_size, NBBY));
	sc->sc_indir_valid = NULL;

	if (sc->sc_indir_data != NULL)
		kmem_free(sc->sc_indir_data, FSS_CLSIZE(sc));
	sc->sc_indir_data = NULL;
}

/*
 * Set all active snapshots on this file system into ERROR state.
 */
static void
fss_unmount_hook(struct mount *mp)
{
	int i;
	struct fss_softc *sc;

	mutex_enter(&fss_device_lock);
	for (i = 0; i < fss_cd.cd_ndevs; i++) {
		if ((sc = device_lookup_private(&fss_cd, i)) == NULL)
			continue;
		mutex_enter(&sc->sc_slock);
		if ((sc->sc_flags & FSS_ACTIVE) != 0 &&
		    sc->sc_mount == mp)
			fss_error(sc, "forced unmount");
		mutex_exit(&sc->sc_slock);
	}
	mutex_exit(&fss_device_lock);
}

/*
 * A buffer is written to the snapshotted block device. Copy to
 * backing store if needed.
 */
static int
fss_copy_on_write(void *v, struct buf *bp, bool data_valid)
{
	int error;
	u_int32_t cl, ch, c;
	struct fss_softc *sc = v;

	mutex_enter(&sc->sc_slock);
	if (!FSS_ISVALID(sc)) {
		mutex_exit(&sc->sc_slock);
		return 0;
	}

	cl = FSS_BTOCL(sc, dbtob(bp->b_blkno));
	ch = FSS_BTOCL(sc, dbtob(bp->b_blkno)+bp->b_bcount-1);
	error = 0;
	if (curlwp == uvm.pagedaemon_lwp) {
		for (c = cl; c <= ch; c++)
			if (isclr(sc->sc_copied, c)) {
				error = ENOMEM;
				break;
			}
	}
	mutex_exit(&sc->sc_slock);

	if (error == 0)
		for (c = cl; c <= ch; c++) {
			error = fss_read_cluster(sc, c);
			if (error)
				break;
		}

	return error;
}

/*
 * Lookup and open needed files.
 *
 * For file system internal snapshot initializes sc_mntname, sc_mount,
 * sc_bs_vp and sc_time.
 *
 * Otherwise returns dev and size of the underlying block device.
 * Initializes sc_mntname, sc_mount, sc_bdev, sc_bs_vp and sc_mount
 */
static int
fss_create_files(struct fss_softc *sc, struct fss_set *fss,
    off_t *bsize, struct lwp *l)
{
	int error, bits, fsbsize;
	uint64_t numsec;
	unsigned int secsize;
	struct timespec ts;
	/* nd -> nd2 to reduce mistakes while updating only some namei calls */
	struct pathbuf *pb2;
	struct nameidata nd2;
	struct vnode *vp;

	/*
	 * Get the mounted file system.
	 */

	error = namei_simple_user(fss->fss_mount,
				NSM_FOLLOW_NOEMULROOT, &vp);
	if (error != 0)
		return error;

	if ((vp->v_vflag & VV_ROOT) != VV_ROOT) {
		vrele(vp);
		return EINVAL;
	}

	sc->sc_mount = vp->v_mount;
	memcpy(sc->sc_mntname, sc->sc_mount->mnt_stat.f_mntonname, MNAMELEN);

	vrele(vp);

	/*
	 * Check for file system internal snapshot.
	 */

	error = namei_simple_user(fss->fss_bstore,
				NSM_FOLLOW_NOEMULROOT, &vp);
	if (error != 0)
		return error;

	if (vp->v_type == VREG && vp->v_mount == sc->sc_mount) {
		sc->sc_flags |= FSS_PERSISTENT;
		sc->sc_bs_vp = vp;

		fsbsize = sc->sc_bs_vp->v_mount->mnt_stat.f_iosize;
		bits = sizeof(sc->sc_bs_bshift)*NBBY;
		for (sc->sc_bs_bshift = 1; sc->sc_bs_bshift < bits;
		    sc->sc_bs_bshift++)
			if (FSS_FSBSIZE(sc) == fsbsize)
				break;
		if (sc->sc_bs_bshift >= bits)
			return EINVAL;

		sc->sc_bs_bmask = FSS_FSBSIZE(sc)-1;
		sc->sc_clshift = 0;

		if ((fss->fss_flags & FSS_UNLINK_ON_CREATE) != 0) {
			error = do_sys_unlink(fss->fss_bstore, UIO_USERSPACE);
			if (error)
				return error;
		}
		error = vn_lock(vp, LK_EXCLUSIVE);
		if (error != 0)
			return error;
		error = VFS_SNAPSHOT(sc->sc_mount, sc->sc_bs_vp, &ts);
		TIMESPEC_TO_TIMEVAL(&sc->sc_time, &ts);

		VOP_UNLOCK(sc->sc_bs_vp);

		return error;
	}
	vrele(vp);

	/*
	 * Get the block device it is mounted on and its size.
	 */

	error = spec_node_lookup_by_mount(sc->sc_mount, &vp);
	if (error)
		return error;
	sc->sc_bdev = vp->v_rdev;

	error = getdisksize(vp, &numsec, &secsize);
	vrele(vp);
	if (error)
		return error;

	*bsize = (off_t)numsec*secsize;

	/*
	 * Get the backing store
	 */

	error = pathbuf_copyin(fss->fss_bstore, &pb2);
	if (error) {
 		return error;
	}
	NDINIT(&nd2, LOOKUP, FOLLOW, pb2);
	if ((error = vn_open(&nd2, FREAD|FWRITE, 0)) != 0) {
		pathbuf_destroy(pb2);
		return error;
	}
	VOP_UNLOCK(nd2.ni_vp);

	sc->sc_bs_vp = nd2.ni_vp;

	if (nd2.ni_vp->v_type != VREG && nd2.ni_vp->v_type != VCHR) {
		pathbuf_destroy(pb2);
		return EINVAL;
	}
	pathbuf_destroy(pb2);

	if ((fss->fss_flags & FSS_UNLINK_ON_CREATE) != 0) {
		error = do_sys_unlink(fss->fss_bstore, UIO_USERSPACE);
		if (error)
			return error;
	}
	if (sc->sc_bs_vp->v_type == VREG) {
		fsbsize = sc->sc_bs_vp->v_mount->mnt_stat.f_iosize;
		if (fsbsize & (fsbsize-1))	/* No power of two */
			return EINVAL;
		for (sc->sc_bs_bshift = 1; sc->sc_bs_bshift < 32;
		    sc->sc_bs_bshift++)
			if (FSS_FSBSIZE(sc) == fsbsize)
				break;
		if (sc->sc_bs_bshift >= 32)
			return EINVAL;
		sc->sc_bs_bmask = FSS_FSBSIZE(sc)-1;
	} else {
		sc->sc_bs_bshift = DEV_BSHIFT;
		sc->sc_bs_bmask = FSS_FSBSIZE(sc)-1;
	}

	return 0;
}

/*
 * Create a snapshot.
 */
static int
fss_create_snapshot(struct fss_softc *sc, struct fss_set *fss, struct lwp *l)
{
	int len, error;
	u_int32_t csize;
	off_t bsize;

	bsize = 0;	/* XXX gcc */

	/*
	 * Open needed files.
	 */
	if ((error = fss_create_files(sc, fss, &bsize, l)) != 0)
		goto bad;

	if (sc->sc_flags & FSS_PERSISTENT) {
		fss_softc_alloc(sc);
		sc->sc_flags |= FSS_ACTIVE;
		return 0;
	}

	/*
	 * Set cluster size. Must be a power of two and
	 * a multiple of backing store block size.
	 */
	if (fss->fss_csize <= 0)
		csize = MAXPHYS;
	else
		csize = fss->fss_csize;
	if (bsize/csize > FSS_CLUSTER_MAX)
		csize = bsize/FSS_CLUSTER_MAX+1;

	for (sc->sc_clshift = sc->sc_bs_bshift; sc->sc_clshift < 32;
	    sc->sc_clshift++)
		if (FSS_CLSIZE(sc) >= csize)
			break;
	if (sc->sc_clshift >= 32) {
		error = EINVAL;
		goto bad;
	}
	sc->sc_clmask = FSS_CLSIZE(sc)-1;

	/*
	 * Set number of cache slots.
	 */
	if (FSS_CLSIZE(sc) <= 8192)
		sc->sc_cache_size = 32;
	else if (FSS_CLSIZE(sc) <= 65536)
		sc->sc_cache_size = 8;
	else
		sc->sc_cache_size = 4;

	/*
	 * Set number of clusters and size of last cluster.
	 */
	sc->sc_clcount = FSS_BTOCL(sc, bsize-1)+1;
	sc->sc_clresid = FSS_CLOFF(sc, bsize-1)+1;

	/*
	 * Set size of indirect table.
	 */
	len = sc->sc_clcount*sizeof(u_int32_t);
	sc->sc_indir_size = FSS_BTOCL(sc, len)+1;
	sc->sc_clnext = sc->sc_indir_size;
	sc->sc_indir_cur = 0;

	if ((error = fss_softc_alloc(sc)) != 0)
		goto bad;

	/*
	 * Activate the snapshot.
	 */

	if ((error = vfs_suspend(sc->sc_mount, 0)) != 0)
		goto bad;

	microtime(&sc->sc_time);

	error = fscow_establish(sc->sc_mount, fss_copy_on_write, sc);
	if (error == 0)
		sc->sc_flags |= FSS_ACTIVE;

	vfs_resume(sc->sc_mount);

	if (error != 0)
		goto bad;

	aprint_debug_dev(sc->sc_dev, "%s snapshot active\n", sc->sc_mntname);
	aprint_debug_dev(sc->sc_dev,
	    "%u clusters of %u, %u cache slots, %u indir clusters\n",
	    sc->sc_clcount, FSS_CLSIZE(sc),
	    sc->sc_cache_size, sc->sc_indir_size);

	return 0;

bad:
	fss_softc_free(sc);
	if (sc->sc_bs_vp != NULL) {
		if (sc->sc_flags & FSS_PERSISTENT)
			vrele(sc->sc_bs_vp);
		else
			vn_close(sc->sc_bs_vp, FREAD|FWRITE, l->l_cred);
	}
	sc->sc_bs_vp = NULL;

	return error;
}

/*
 * Delete a snapshot.
 */
static int
fss_delete_snapshot(struct fss_softc *sc, struct lwp *l)
{

	if ((sc->sc_flags & FSS_PERSISTENT) == 0)
		fscow_disestablish(sc->sc_mount, fss_copy_on_write, sc);

	mutex_enter(&sc->sc_slock);
	sc->sc_flags &= ~(FSS_ACTIVE|FSS_ERROR);
	sc->sc_mount = NULL;
	sc->sc_bdev = NODEV;
	mutex_exit(&sc->sc_slock);

	fss_softc_free(sc);
	if (sc->sc_flags & FSS_PERSISTENT)
		vrele(sc->sc_bs_vp);
	else
		vn_close(sc->sc_bs_vp, FREAD|FWRITE, l->l_cred);
	sc->sc_bs_vp = NULL;
	sc->sc_flags &= ~FSS_PERSISTENT;

	return 0;
}

/*
 * Read a cluster from the snapshotted block device to the cache.
 */
static int
fss_read_cluster(struct fss_softc *sc, u_int32_t cl)
{
	int error, todo, offset, len;
	daddr_t dblk;
	struct buf *bp, *mbp;
	struct fss_cache *scp, *scl;

	/*
	 * Get a free cache slot.
	 */
	scl = sc->sc_cache+sc->sc_cache_size;

	mutex_enter(&sc->sc_slock);

restart:
	if (isset(sc->sc_copied, cl) || !FSS_ISVALID(sc)) {
		mutex_exit(&sc->sc_slock);
		return 0;
	}

	for (scp = sc->sc_cache; scp < scl; scp++)
		if (scp->fc_cluster == cl) {
			if (scp->fc_type == FSS_CACHE_VALID) {
				mutex_exit(&sc->sc_slock);
				return 0;
			} else if (scp->fc_type == FSS_CACHE_BUSY) {
				cv_wait(&scp->fc_state_cv, &sc->sc_slock);
				goto restart;
			}
		}

	for (scp = sc->sc_cache; scp < scl; scp++)
		if (scp->fc_type == FSS_CACHE_FREE) {
			scp->fc_type = FSS_CACHE_BUSY;
			scp->fc_cluster = cl;
			break;
		}
	if (scp >= scl) {
		cv_wait(&sc->sc_cache_cv, &sc->sc_slock);
		goto restart;
	}

	mutex_exit(&sc->sc_slock);

	/*
	 * Start the read.
	 */
	dblk = btodb(FSS_CLTOB(sc, cl));
	if (cl == sc->sc_clcount-1) {
		todo = sc->sc_clresid;
		memset((char *)scp->fc_data + todo, 0, FSS_CLSIZE(sc) - todo);
	} else
		todo = FSS_CLSIZE(sc);
	offset = 0;
	mbp = getiobuf(NULL, true);
	mbp->b_bufsize = todo;
	mbp->b_data = scp->fc_data;
	mbp->b_resid = mbp->b_bcount = todo;
	mbp->b_flags = B_READ;
	mbp->b_cflags = BC_BUSY;
	mbp->b_dev = sc->sc_bdev;
	while (todo > 0) {
		len = todo;
		if (len > MAXPHYS)
			len = MAXPHYS;
		if (btodb(FSS_CLTOB(sc, cl)) == dblk && len == todo)
			bp = mbp;
		else {
			bp = getiobuf(NULL, true);
			nestiobuf_setup(mbp, bp, offset, len);
		}
		bp->b_lblkno = 0;
		bp->b_blkno = dblk;
		bdev_strategy(bp);
		dblk += btodb(len);
		offset += len;
		todo -= len;
	}
	error = biowait(mbp);
	putiobuf(mbp);

	mutex_enter(&sc->sc_slock);
	scp->fc_type = (error ? FSS_CACHE_FREE : FSS_CACHE_VALID);
	cv_broadcast(&scp->fc_state_cv);
	if (error == 0) {
		setbit(sc->sc_copied, scp->fc_cluster);
		cv_signal(&sc->sc_work_cv);
	}
	mutex_exit(&sc->sc_slock);

	return error;
}

/*
 * Read/write clusters from/to backing store.
 * For persistent snapshots must be called with cl == 0. off is the
 * offset into the snapshot.
 */
static int
fss_bs_io(struct fss_softc *sc, fss_io_type rw,
    u_int32_t cl, off_t off, int len, void *data)
{
	int error;

	off += FSS_CLTOB(sc, cl);

	vn_lock(sc->sc_bs_vp, LK_EXCLUSIVE|LK_RETRY);

	error = vn_rdwr((rw == FSS_READ ? UIO_READ : UIO_WRITE), sc->sc_bs_vp,
	    data, len, off, UIO_SYSSPACE,
	    IO_ADV_ENCODE(POSIX_FADV_NOREUSE) | IO_NODELOCKED,
	    sc->sc_bs_lwp->l_cred, NULL, NULL);
	if (error == 0) {
		mutex_enter(sc->sc_bs_vp->v_interlock);
		error = VOP_PUTPAGES(sc->sc_bs_vp, trunc_page(off),
		    round_page(off+len), PGO_CLEANIT | PGO_FREE | PGO_SYNCIO);
	}

	VOP_UNLOCK(sc->sc_bs_vp);

	return error;
}

/*
 * Get a pointer to the indirect slot for this cluster.
 */
static u_int32_t *
fss_bs_indir(struct fss_softc *sc, u_int32_t cl)
{
	u_int32_t icl;
	int ioff;

	icl = cl/(FSS_CLSIZE(sc)/sizeof(u_int32_t));
	ioff = cl%(FSS_CLSIZE(sc)/sizeof(u_int32_t));

	if (sc->sc_indir_cur == icl)
		return &sc->sc_indir_data[ioff];

	if (sc->sc_indir_dirty) {
		if (fss_bs_io(sc, FSS_WRITE, sc->sc_indir_cur, 0,
		    FSS_CLSIZE(sc), (void *)sc->sc_indir_data) != 0)
			return NULL;
		setbit(sc->sc_indir_valid, sc->sc_indir_cur);
	}

	sc->sc_indir_dirty = 0;
	sc->sc_indir_cur = icl;

	if (isset(sc->sc_indir_valid, sc->sc_indir_cur)) {
		if (fss_bs_io(sc, FSS_READ, sc->sc_indir_cur, 0,
		    FSS_CLSIZE(sc), (void *)sc->sc_indir_data) != 0)
			return NULL;
	} else
		memset(sc->sc_indir_data, 0, FSS_CLSIZE(sc));

	return &sc->sc_indir_data[ioff];
}

/*
 * The kernel thread (one for every active snapshot).
 *
 * After wakeup it cleans the cache and runs the I/O requests.
 */
static void
fss_bs_thread(void *arg)
{
	bool thread_idle, is_valid;
	int error, i, todo, len, crotor, is_read;
	long off;
	char *addr;
	u_int32_t c, cl, ch, *indirp;
	struct buf *bp, *nbp;
	struct fss_softc *sc;
	struct fss_cache *scp, *scl;

	sc = arg;
	scl = sc->sc_cache+sc->sc_cache_size;
	crotor = 0;
	thread_idle = false;

	mutex_enter(&sc->sc_slock);

	for (;;) {
		if (thread_idle)
			cv_wait(&sc->sc_work_cv, &sc->sc_slock);
		thread_idle = true;
		if ((sc->sc_flags & FSS_BS_THREAD) == 0) {
			mutex_exit(&sc->sc_slock);
			kthread_exit(0);
		}

		/*
		 * Process I/O requests (persistent)
		 */

		if (sc->sc_flags & FSS_PERSISTENT) {
			if ((bp = bufq_get(sc->sc_bufq)) == NULL)
				continue;
			is_valid = FSS_ISVALID(sc);
			is_read = (bp->b_flags & B_READ);
			thread_idle = false;
			mutex_exit(&sc->sc_slock);

			if (is_valid) {
				disk_busy(sc->sc_dkdev);
				error = fss_bs_io(sc, FSS_READ, 0,
				    dbtob(bp->b_blkno), bp->b_bcount,
				    bp->b_data);
				disk_unbusy(sc->sc_dkdev,
				    (error ? 0 : bp->b_bcount), is_read);
			} else
				error = ENXIO;

			bp->b_error = error;
			bp->b_resid = (error ? bp->b_bcount : 0);
			biodone(bp);

			mutex_enter(&sc->sc_slock);
			continue;
		}

		/*
		 * Clean the cache
		 */
		for (i = 0; i < sc->sc_cache_size; i++) {
			crotor = (crotor + 1) % sc->sc_cache_size;
			scp = sc->sc_cache + crotor;
			if (scp->fc_type != FSS_CACHE_VALID)
				continue;
			mutex_exit(&sc->sc_slock);

			thread_idle = false;
			indirp = fss_bs_indir(sc, scp->fc_cluster);
			if (indirp != NULL) {
				error = fss_bs_io(sc, FSS_WRITE, sc->sc_clnext,
				    0, FSS_CLSIZE(sc), scp->fc_data);
			} else
				error = EIO;

			mutex_enter(&sc->sc_slock);
			if (error == 0) {
				*indirp = sc->sc_clnext++;
				sc->sc_indir_dirty = 1;
			} else
				fss_error(sc, "write error on backing store");

			scp->fc_type = FSS_CACHE_FREE;
			cv_broadcast(&sc->sc_cache_cv);
			break;
		}

		/*
		 * Process I/O requests
		 */
		if ((bp = bufq_get(sc->sc_bufq)) == NULL)
			continue;
		is_valid = FSS_ISVALID(sc);
		is_read = (bp->b_flags & B_READ);
		thread_idle = false;

		if (!is_valid) {
			mutex_exit(&sc->sc_slock);

			bp->b_error = ENXIO;
			bp->b_resid = bp->b_bcount;
			biodone(bp);

			mutex_enter(&sc->sc_slock);
			continue;
		}

		disk_busy(sc->sc_dkdev);

		/*
		 * First read from the snapshotted block device unless
		 * this request is completely covered by backing store.
		 */

		cl = FSS_BTOCL(sc, dbtob(bp->b_blkno));
		off = FSS_CLOFF(sc, dbtob(bp->b_blkno));
		ch = FSS_BTOCL(sc, dbtob(bp->b_blkno)+bp->b_bcount-1);
		error = 0;
		bp->b_resid = 0;
		bp->b_error = 0;
		for (c = cl; c <= ch; c++) {
			if (isset(sc->sc_copied, c))
				continue;
			mutex_exit(&sc->sc_slock);

			/* Not on backing store, read from device. */
			nbp = getiobuf(NULL, true);
			nbp->b_flags = B_READ;
			nbp->b_resid = nbp->b_bcount = bp->b_bcount;
			nbp->b_bufsize = bp->b_bcount;
			nbp->b_data = bp->b_data;
			nbp->b_blkno = bp->b_blkno;
			nbp->b_lblkno = 0;
			nbp->b_dev = sc->sc_bdev;
			SET(nbp->b_cflags, BC_BUSY);	/* mark buffer busy */

			bdev_strategy(nbp);

			error = biowait(nbp);
			if (error != 0) {
				bp->b_resid = bp->b_bcount;
				bp->b_error = nbp->b_error;
				disk_unbusy(sc->sc_dkdev, 0, is_read);
				biodone(bp);
			}
			putiobuf(nbp);

			mutex_enter(&sc->sc_slock);
			break;
		}
		if (error)
			continue;

		/*
		 * Replace those parts that have been saved to backing store.
		 */

		addr = bp->b_data;
		todo = bp->b_bcount;
		for (c = cl; c <= ch; c++, off = 0, todo -= len, addr += len) {
			len = FSS_CLSIZE(sc)-off;
			if (len > todo)
				len = todo;
			if (isclr(sc->sc_copied, c))
				continue;
			mutex_exit(&sc->sc_slock);

			indirp = fss_bs_indir(sc, c);
			if (indirp == NULL || *indirp == 0) {
				/*
				 * Not on backing store. Either in cache
				 * or hole in the snapshotted block device.
				 */

				mutex_enter(&sc->sc_slock);
				for (scp = sc->sc_cache; scp < scl; scp++)
					if (scp->fc_type == FSS_CACHE_VALID &&
					    scp->fc_cluster == c)
						break;
				if (scp < scl)
					memcpy(addr, (char *)scp->fc_data+off,
					    len);
				else
					memset(addr, 0, len);
				continue;
			}

			/*
			 * Read from backing store.
			 */
			error =
			    fss_bs_io(sc, FSS_READ, *indirp, off, len, addr);

			mutex_enter(&sc->sc_slock);
			if (error) {
				bp->b_resid = bp->b_bcount;
				bp->b_error = error;
				break;
			}
		}
		mutex_exit(&sc->sc_slock);

		disk_unbusy(sc->sc_dkdev, (error ? 0 : bp->b_bcount), is_read);
		biodone(bp);

		mutex_enter(&sc->sc_slock);
	}
}

#ifdef _MODULE

#include <sys/module.h>

MODULE(MODULE_CLASS_DRIVER, fss, NULL);
CFDRIVER_DECL(fss, DV_DISK, NULL);

static int
fss_modcmd(modcmd_t cmd, void *arg)
{
	devmajor_t bmajor = -1, cmajor = -1;
	int error = 0;

	switch (cmd) {
	case MODULE_CMD_INIT:
		mutex_init(&fss_device_lock, MUTEX_DEFAULT, IPL_NONE);
		error = config_cfdriver_attach(&fss_cd);
		if (error) {
			mutex_destroy(&fss_device_lock);
			break;
		}
		error = config_cfattach_attach(fss_cd.cd_name, &fss_ca);
		if (error) {
			config_cfdriver_detach(&fss_cd);
			mutex_destroy(&fss_device_lock);
			break;
		}
		error = devsw_attach(fss_cd.cd_name,
		    &fss_bdevsw, &bmajor, &fss_cdevsw, &cmajor);
		if (error == EEXIST)
			error = 0;
		if (error) {
			config_cfattach_detach(fss_cd.cd_name, &fss_ca);
			config_cfdriver_detach(&fss_cd);
			mutex_destroy(&fss_device_lock);
			break;
		}
		break;

	case MODULE_CMD_FINI:
		error = config_cfattach_detach(fss_cd.cd_name, &fss_ca);
		if (error)
			break;
		config_cfdriver_detach(&fss_cd);
		devsw_detach(&fss_bdevsw, &fss_cdevsw);
		mutex_destroy(&fss_device_lock);
		break;

	default:
		error = ENOTTY;
		break;
	}

	return error;
}

#endif /* _MODULE */
