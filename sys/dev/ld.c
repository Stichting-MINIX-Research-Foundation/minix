/*	$NetBSD: ld.c,v 1.93 2015/08/28 17:41:49 mlelstv Exp $	*/

/*-
 * Copyright (c) 1998, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran and Charles M. Hannum.
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
 * Disk driver for use by RAID controllers.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ld.c,v 1.93 2015/08/28 17:41:49 mlelstv Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/queue.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/endian.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/dkio.h>
#include <sys/stat.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/syslog.h>
#include <sys/mutex.h>

#include <dev/ldvar.h>

#include <prop/proplib.h>

static void	ldminphys(struct buf *bp);
static bool	ld_suspend(device_t, const pmf_qual_t *);
static bool	ld_shutdown(device_t, int);
static int	ld_diskstart(device_t, struct buf *bp);
static void	ld_iosize(device_t, int *);
static int	ld_dumpblocks(device_t, void *, daddr_t, int);
static void	ld_fake_geometry(struct ld_softc *);
static void	ld_set_geometry(struct ld_softc *);
static void	ld_config_interrupts (device_t);
static int	ld_lastclose(device_t);
static int	ld_discard(device_t, off_t, off_t);

extern struct	cfdriver ld_cd;

static dev_type_open(ldopen);
static dev_type_close(ldclose);
static dev_type_read(ldread);
static dev_type_write(ldwrite);
static dev_type_ioctl(ldioctl);
static dev_type_strategy(ldstrategy);
static dev_type_dump(lddump);
static dev_type_size(ldsize);
static dev_type_discard(lddiscard);

const struct bdevsw ld_bdevsw = {
	.d_open = ldopen,
	.d_close = ldclose,
	.d_strategy = ldstrategy,
	.d_ioctl = ldioctl,
	.d_dump = lddump,
	.d_psize = ldsize,
	.d_discard = lddiscard,
	.d_flag = D_DISK | D_MPSAFE
};

const struct cdevsw ld_cdevsw = {
	.d_open = ldopen,
	.d_close = ldclose,
	.d_read = ldread,
	.d_write = ldwrite,
	.d_ioctl = ldioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = lddiscard,
	.d_flag = D_DISK | D_MPSAFE
};

static struct	dkdriver lddkdriver = {
	.d_open = ldopen,
	.d_close = ldclose,
	.d_strategy = ldstrategy,
	.d_iosize = ld_iosize,
	.d_minphys  = ldminphys,
	.d_diskstart = ld_diskstart,
	.d_dumpblocks = ld_dumpblocks,
	.d_lastclose = ld_lastclose,
	.d_discard = ld_discard
};

void
ldattach(struct ld_softc *sc)
{
	device_t self = sc->sc_dv;
	struct dk_softc *dksc = &sc->sc_dksc;

	mutex_init(&sc->sc_mutex, MUTEX_DEFAULT, IPL_VM);
	cv_init(&sc->sc_drain, "lddrain");

	if ((sc->sc_flags & LDF_ENABLED) == 0) {
		return;
	}

	/* Initialise dk and disk structure. */
	dk_init(dksc, self, DKTYPE_LD);
	disk_init(&dksc->sc_dkdev, dksc->sc_xname, &lddkdriver);

	if (sc->sc_maxxfer > MAXPHYS)
		sc->sc_maxxfer = MAXPHYS;

	/* Build synthetic geometry if necessary. */
	if (sc->sc_nheads == 0 || sc->sc_nsectors == 0 ||
	    sc->sc_ncylinders == 0)
	    ld_fake_geometry(sc);

	sc->sc_disksize512 = sc->sc_secperunit * sc->sc_secsize / DEV_BSIZE;

	/* Attach dk and disk subsystems */
	dk_attach(dksc);
	disk_attach(&dksc->sc_dkdev);
	ld_set_geometry(sc);

	bufq_alloc(&dksc->sc_bufq, BUFQ_DISK_DEFAULT_STRAT, BUFQ_SORT_RAWBLOCK);

	/* Register with PMF */
	if (!pmf_device_register1(dksc->sc_dev, ld_suspend, NULL, ld_shutdown))
		aprint_error_dev(dksc->sc_dev,
		    "couldn't establish power handler\n");

	/* Discover wedges on this disk. */
	config_interrupts(sc->sc_dv, ld_config_interrupts);
}

int
ldadjqparam(struct ld_softc *sc, int xmax)
{

	mutex_enter(&sc->sc_mutex);
	sc->sc_maxqueuecnt = xmax;
	mutex_exit(&sc->sc_mutex);

	return (0);
}

int
ldbegindetach(struct ld_softc *sc, int flags)
{
	struct dk_softc *dksc = &sc->sc_dksc;
	int rv = 0;

	if ((sc->sc_flags & LDF_ENABLED) == 0)
		return (0);

	rv = disk_begindetach(&dksc->sc_dkdev, ld_lastclose, dksc->sc_dev, flags);

	if (rv != 0)
		return rv;

	mutex_enter(&sc->sc_mutex);
	sc->sc_maxqueuecnt = 0;

	while (sc->sc_queuecnt > 0) {
		sc->sc_flags |= LDF_DRAIN;
		cv_wait(&sc->sc_drain, &sc->sc_mutex);
	}
	mutex_exit(&sc->sc_mutex);

	return (rv);
}

void
ldenddetach(struct ld_softc *sc)
{
	struct dk_softc *dksc = &sc->sc_dksc;
	int bmaj, cmaj, i, mn;

	if ((sc->sc_flags & LDF_ENABLED) == 0)
		return;

	mutex_enter(&sc->sc_mutex);

	/* Wait for commands queued with the hardware to complete. */
	if (sc->sc_queuecnt != 0) {
		if (cv_timedwait(&sc->sc_drain, &sc->sc_mutex, 30 * hz))
			printf("%s: not drained\n", dksc->sc_xname);
	}
	mutex_exit(&sc->sc_mutex);

	/* Kill off any queued buffers. */
	dk_drain(dksc);
	bufq_free(dksc->sc_bufq);

	/* Locate the major numbers. */
	bmaj = bdevsw_lookup_major(&ld_bdevsw);
	cmaj = cdevsw_lookup_major(&ld_cdevsw);

	/* Nuke the vnodes for any open instances. */
	for (i = 0; i < MAXPARTITIONS; i++) {
		mn = DISKMINOR(device_unit(dksc->sc_dev), i);
		vdevgone(bmaj, mn, mn, VBLK);
		vdevgone(cmaj, mn, mn, VCHR);
	}

	/* Delete all of our wedges. */
	dkwedge_delall(&dksc->sc_dkdev);

	/* Detach from the disk list. */
	disk_detach(&dksc->sc_dkdev);
	disk_destroy(&dksc->sc_dkdev);

	dk_detach(dksc);

	/* Deregister with PMF */
	pmf_device_deregister(dksc->sc_dev);

	/*
	 * XXX We can't really flush the cache here, beceause the
	 * XXX device may already be non-existent from the controller's
	 * XXX perspective.
	 */
#if 0
	/* Flush the device's cache. */
	if (sc->sc_flush != NULL)
		if ((*sc->sc_flush)(sc, 0) != 0)
			device_printf(dksc->sc_dev, "unable to flush cache\n");
#endif
	cv_destroy(&sc->sc_drain);
	mutex_destroy(&sc->sc_mutex);
}

/* ARGSUSED */
static bool
ld_suspend(device_t dev, const pmf_qual_t *qual)
{
	return ld_shutdown(dev, 0);
}

/* ARGSUSED */
static bool
ld_shutdown(device_t dev, int flags)
{
	struct ld_softc *sc = device_private(dev);
	struct dk_softc *dksc = &sc->sc_dksc;

	if (sc->sc_flush != NULL && (*sc->sc_flush)(sc, LDFL_POLL) != 0) {
		device_printf(dksc->sc_dev, "unable to flush cache\n");
		return false;
	}

	return true;
}

/* ARGSUSED */
static int
ldopen(dev_t dev, int flags, int fmt, struct lwp *l)
{
	struct ld_softc *sc;
	struct dk_softc *dksc;
	int unit;

	unit = DISKUNIT(dev);
	if ((sc = device_lookup_private(&ld_cd, unit)) == NULL)
		return (ENXIO);
	dksc = &sc->sc_dksc;

	return dk_open(dksc, dev, flags, fmt, l);
}

static int
ld_lastclose(device_t self)
{
	struct ld_softc *sc = device_private(self);

	if (sc->sc_flush != NULL && (*sc->sc_flush)(sc, 0) != 0)
		device_printf(self, "unable to flush cache\n");

	return 0;
}

/* ARGSUSED */
static int
ldclose(dev_t dev, int flags, int fmt, struct lwp *l)
{
	struct ld_softc *sc;
	struct dk_softc *dksc;
	int unit;

	unit = DISKUNIT(dev);
	sc = device_lookup_private(&ld_cd, unit);
	dksc = &sc->sc_dksc;

	return dk_close(dksc, dev, flags, fmt, l);
}

/* ARGSUSED */
static int
ldread(dev_t dev, struct uio *uio, int ioflag)
{

	return (physio(ldstrategy, NULL, dev, B_READ, ldminphys, uio));
}

/* ARGSUSED */
static int
ldwrite(dev_t dev, struct uio *uio, int ioflag)
{

	return (physio(ldstrategy, NULL, dev, B_WRITE, ldminphys, uio));
}

/* ARGSUSED */
static int
ldioctl(dev_t dev, u_long cmd, void *addr, int32_t flag, struct lwp *l)
{
	struct ld_softc *sc;
	struct dk_softc *dksc;
	int unit, error;

	unit = DISKUNIT(dev);
	sc = device_lookup_private(&ld_cd, unit);
	dksc = &sc->sc_dksc;

	error = disk_ioctl(&dksc->sc_dkdev, dev, cmd, addr, flag, l);
	if (error != EPASSTHROUGH)
		return (error);

	error = dk_ioctl(dksc, dev, cmd, addr, flag, l);
	if (error != EPASSTHROUGH)
		return (error);

	error = 0;

	switch (cmd) {
	case DIOCCACHESYNC:
		/*
		 * XXX Do we really need to care about having a writable
		 * file descriptor here?
		 */
		if ((flag & FWRITE) == 0)
			error = EBADF;
		else if (sc->sc_flush)
			error = (*sc->sc_flush)(sc, 0);
		else
			error = 0;	/* XXX Error out instead? */
		break;
	default:
		error = ENOTTY;
		break;
	}

	return (error);
}

static void
ldstrategy(struct buf *bp)
{
	struct ld_softc *sc;
	struct dk_softc *dksc;
	int unit;

	unit = DISKUNIT(bp->b_dev);
	sc = device_lookup_private(&ld_cd, unit);
	dksc = &sc->sc_dksc;

	return dk_strategy(dksc, bp);
}

static int
ld_diskstart(device_t dev, struct buf *bp)
{
	struct ld_softc *sc = device_private(dev);
	int error;

	if (sc->sc_queuecnt >= sc->sc_maxqueuecnt)
		return EAGAIN;

	mutex_enter(&sc->sc_mutex);

	if (sc->sc_queuecnt >= sc->sc_maxqueuecnt)
		error = EAGAIN;
	else {
		error = (*sc->sc_start)(sc, bp);
		if (error == 0)
			sc->sc_queuecnt++;
	}

	mutex_exit(&sc->sc_mutex);

	return error;
}

void
lddone(struct ld_softc *sc, struct buf *bp)
{
	struct dk_softc *dksc = &sc->sc_dksc;

	dk_done(dksc, bp);

	mutex_enter(&sc->sc_mutex);
	if (--sc->sc_queuecnt <= sc->sc_maxqueuecnt) {
		if ((sc->sc_flags & LDF_DRAIN) != 0) {
			sc->sc_flags &= ~LDF_DRAIN;
			cv_broadcast(&sc->sc_drain);
		}
		mutex_exit(&sc->sc_mutex);
		dk_start(dksc, NULL);
	} else
		mutex_exit(&sc->sc_mutex);
}

static int
ldsize(dev_t dev)
{
	struct ld_softc *sc;
	struct dk_softc *dksc;
	int unit;

	unit = DISKUNIT(dev);
	if ((sc = device_lookup_private(&ld_cd, unit)) == NULL)
		return (ENODEV);
	dksc = &sc->sc_dksc;

	if ((sc->sc_flags & LDF_ENABLED) == 0)
		return (ENODEV);

	return dk_size(dksc, dev);
}

/*
 * Take a dump.
 */
static int
lddump(dev_t dev, daddr_t blkno, void *va, size_t size)
{
	struct ld_softc *sc;
	struct dk_softc *dksc;
	int unit;

	unit = DISKUNIT(dev);
	if ((sc = device_lookup_private(&ld_cd, unit)) == NULL)
		return (ENXIO);
	dksc = &sc->sc_dksc;

	if ((sc->sc_flags & LDF_ENABLED) == 0)
		return (ENODEV);

	return dk_dump(dksc, dev, blkno, va, size);
}

static int
ld_dumpblocks(device_t dev, void *va, daddr_t blkno, int nblk)
{
	struct ld_softc *sc = device_private(dev);

	if (sc->sc_dump == NULL)
		return (ENODEV);

	return (*sc->sc_dump)(sc, va, blkno, nblk);
}

/*
 * Adjust the size of a transfer.
 */
static void
ldminphys(struct buf *bp)
{
	int unit;
	struct ld_softc *sc;

	unit = DISKUNIT(bp->b_dev);
	sc = device_lookup_private(&ld_cd, unit);

	ld_iosize(sc->sc_dv, &bp->b_bcount);
	minphys(bp);
}

static void
ld_iosize(device_t d, int *countp)
{
	struct ld_softc *sc = device_private(d);

	if (*countp > sc->sc_maxxfer)
		*countp = sc->sc_maxxfer;
}

static void
ld_fake_geometry(struct ld_softc *sc)
{
	uint64_t ncyl;

	if (sc->sc_secperunit <= 528 * 2048)		/* 528MB */
		sc->sc_nheads = 16;
	else if (sc->sc_secperunit <= 1024 * 2048)	/* 1GB */
		sc->sc_nheads = 32;
	else if (sc->sc_secperunit <= 21504 * 2048)	/* 21GB */
		sc->sc_nheads = 64;
	else if (sc->sc_secperunit <= 43008 * 2048)	/* 42GB */
		sc->sc_nheads = 128;
	else
		sc->sc_nheads = 255;

	sc->sc_nsectors = 63;
	sc->sc_ncylinders = INT_MAX;
	ncyl = sc->sc_secperunit /
	    (sc->sc_nheads * sc->sc_nsectors);
	if (ncyl < INT_MAX)
		sc->sc_ncylinders = (int)ncyl;
}

static void
ld_set_geometry(struct ld_softc *sc)
{
	struct dk_softc *dksc = &sc->sc_dksc;
	struct disk_geom *dg = &dksc->sc_dkdev.dk_geom;
	char tbuf[9];

	format_bytes(tbuf, sizeof(tbuf), sc->sc_secperunit *
	    sc->sc_secsize);
	aprint_normal_dev(dksc->sc_dev, "%s, %d cyl, %d head, %d sec, "
	    "%d bytes/sect x %"PRIu64" sectors\n",
	    tbuf, sc->sc_ncylinders, sc->sc_nheads,
	    sc->sc_nsectors, sc->sc_secsize, sc->sc_secperunit);

	memset(dg, 0, sizeof(*dg));
	dg->dg_secperunit = sc->sc_secperunit;
	dg->dg_secsize = sc->sc_secsize;
	dg->dg_nsectors = sc->sc_nsectors;
	dg->dg_ntracks = sc->sc_nheads;
	dg->dg_ncylinders = sc->sc_ncylinders;

	disk_set_info(dksc->sc_dev, &dksc->sc_dkdev, NULL);
}

static void
ld_config_interrupts(device_t d)
{
	struct ld_softc *sc = device_private(d);
	struct dk_softc *dksc = &sc->sc_dksc;

	dkwedge_discover(&dksc->sc_dkdev);
}

static int
ld_discard(device_t dev, off_t pos, off_t len)
{
	struct ld_softc *sc = device_private(dev);

	if (sc->sc_discard == NULL)
		return (ENODEV);

	return (*sc->sc_discard)(sc, pos, len);
}

static int
lddiscard(dev_t dev, off_t pos, off_t len)
{
	struct ld_softc *sc;
	struct dk_softc *dksc;
	int unit;

	unit = DISKUNIT(dev);
	sc = device_lookup_private(&ld_cd, unit);
	dksc = &sc->sc_dksc;

	return dk_discard(dksc, dev, pos, len);
}
