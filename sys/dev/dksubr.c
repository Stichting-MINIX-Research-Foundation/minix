/* $NetBSD: dksubr.c,v 1.76 2015/08/28 17:41:49 mlelstv Exp $ */

/*-
 * Copyright (c) 1996, 1997, 1998, 1999, 2002, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe and Roland C. Dowdeswell.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dksubr.c,v 1.76 2015/08/28 17:41:49 mlelstv Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/stat.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/vnode.h>
#include <sys/fcntl.h>
#include <sys/namei.h>
#include <sys/module.h>
#include <sys/syslog.h>

#include <dev/dkvar.h>
#include <miscfs/specfs/specdev.h> /* for v_rdev */

int	dkdebug = 0;

#ifdef DEBUG
#define DKDB_FOLLOW	0x1
#define DKDB_INIT	0x2
#define DKDB_VNODE	0x4

#define IFDEBUG(x,y)		if (dkdebug & (x)) y
#define DPRINTF(x,y)		IFDEBUG(x, printf y)
#define DPRINTF_FOLLOW(y)	DPRINTF(DKDB_FOLLOW, y)
#else
#define IFDEBUG(x,y)
#define DPRINTF(x,y)
#define DPRINTF_FOLLOW(y)
#endif

static int dk_subr_modcmd(modcmd_t, void *);

#define DKLABELDEV(dev)	\
	(MAKEDISKDEV(major((dev)), DISKUNIT((dev)), RAW_PART))

static void	dk_makedisklabel(struct dk_softc *);
static int	dk_translate(struct dk_softc *, struct buf *);
static void	dk_done1(struct dk_softc *, struct buf *, bool);

void
dk_init(struct dk_softc *dksc, device_t dev, int dtype)
{

	memset(dksc, 0x0, sizeof(*dksc));
	dksc->sc_dtype = dtype;
	dksc->sc_dev = dev;

	strlcpy(dksc->sc_xname, device_xname(dev), DK_XNAME_SIZE);
	dksc->sc_dkdev.dk_name = dksc->sc_xname;
}

void
dk_attach(struct dk_softc *dksc)
{
	mutex_init(&dksc->sc_iolock, MUTEX_DEFAULT, IPL_VM);
	dksc->sc_flags |= DKF_INITED;
#ifdef DIAGNOSTIC
	dksc->sc_flags |= DKF_WARNLABEL | DKF_LABELSANITY;
#endif

	/* Attach the device into the rnd source list. */
	rnd_attach_source(&dksc->sc_rnd_source, dksc->sc_xname,
	    RND_TYPE_DISK, RND_FLAG_DEFAULT);
}

void
dk_detach(struct dk_softc *dksc)
{
	/* Unhook the entropy source. */
	rnd_detach_source(&dksc->sc_rnd_source);

	dksc->sc_flags &= ~DKF_INITED;
	mutex_destroy(&dksc->sc_iolock);
}

/* ARGSUSED */
int
dk_open(struct dk_softc *dksc, dev_t dev,
    int flags, int fmt, struct lwp *l)
{
	struct	disklabel *lp = dksc->sc_dkdev.dk_label;
	int	part = DISKPART(dev);
	int	pmask = 1 << part;
	int	ret = 0;
	struct disk *dk = &dksc->sc_dkdev;

	DPRINTF_FOLLOW(("dk_open(%s, %p, 0x%"PRIx64", 0x%x)\n",
	    dksc->sc_xname, dksc, dev, flags));

	mutex_enter(&dk->dk_openlock);

	/*
	 * If there are wedges, and this is not RAW_PART, then we
	 * need to fail.
	 */
	if (dk->dk_nwedges != 0 && part != RAW_PART) {
		ret = EBUSY;
		goto done;
	}

	/*
	 * If we're init'ed and there are no other open partitions then
	 * update the in-core disklabel.
	 */
	if ((dksc->sc_flags & DKF_INITED)) {
		if ((dksc->sc_flags & DKF_VLABEL) == 0) {
			dksc->sc_flags |= DKF_VLABEL;
			dk_getdisklabel(dksc, dev);
		}
	}

	/* Fail if we can't find the partition. */
	if (part != RAW_PART &&
	    ((dksc->sc_flags & DKF_VLABEL) == 0 ||
	     part >= lp->d_npartitions ||
	     lp->d_partitions[part].p_fstype == FS_UNUSED)) {
		ret = ENXIO;
		goto done;
	}

	/* Mark our unit as open. */
	switch (fmt) {
	case S_IFCHR:
		dk->dk_copenmask |= pmask;
		break;
	case S_IFBLK:
		dk->dk_bopenmask |= pmask;
		break;
	}

	dk->dk_openmask = dk->dk_copenmask | dk->dk_bopenmask;

done:
	mutex_exit(&dk->dk_openlock);
	return ret;
}

/* ARGSUSED */
int
dk_close(struct dk_softc *dksc, dev_t dev,
    int flags, int fmt, struct lwp *l)
{
	const struct dkdriver *dkd = dksc->sc_dkdev.dk_driver;
	int	part = DISKPART(dev);
	int	pmask = 1 << part;
	struct disk *dk = &dksc->sc_dkdev;

	DPRINTF_FOLLOW(("dk_close(%s, %p, 0x%"PRIx64", 0x%x)\n",
	    dksc->sc_xname, dksc, dev, flags));

	mutex_enter(&dk->dk_openlock);

	switch (fmt) {
	case S_IFCHR:
		dk->dk_copenmask &= ~pmask;
		break;
	case S_IFBLK:
		dk->dk_bopenmask &= ~pmask;
		break;
	}
	dk->dk_openmask = dk->dk_copenmask | dk->dk_bopenmask;

	if (dk->dk_openmask == 0) {
		if (dkd->d_lastclose != NULL)
			(*dkd->d_lastclose)(dksc->sc_dev);
		if ((dksc->sc_flags & DKF_KLABEL) == 0)
			dksc->sc_flags &= ~DKF_VLABEL;
	}

	mutex_exit(&dk->dk_openlock);
	return 0;
}

static int
dk_translate(struct dk_softc *dksc, struct buf *bp)
{
	int	part;
	int	wlabel;
	daddr_t	blkno;
	struct disklabel *lp;
	struct disk *dk;
	uint64_t numsecs;
	unsigned secsize;

	lp = dksc->sc_dkdev.dk_label;
	dk = &dksc->sc_dkdev;

	part = DISKPART(bp->b_dev);
	numsecs = dk->dk_geom.dg_secperunit;
	secsize = dk->dk_geom.dg_secsize;

	/*
	 * The transfer must be a whole number of blocks and the offset must
	 * not be negative.
	 */
	if ((bp->b_bcount % secsize) != 0 || bp->b_blkno < 0) {
		bp->b_error = EINVAL;
		goto done;
	}

	/* If there is nothing to do, then we are done */
	if (bp->b_bcount == 0)
		goto done;

	wlabel = dksc->sc_flags & (DKF_WLABEL|DKF_LABELLING);
	if (part == RAW_PART) {
		if (bounds_check_with_mediasize(bp, DEV_BSIZE, numsecs) <= 0)
			goto done;
	} else {
		if (bounds_check_with_label(&dksc->sc_dkdev, bp, wlabel) <= 0)
			goto done;
	}

	/*
	 * Convert the block number to absolute and put it in terms
	 * of the device's logical block size.
	 */
	if (secsize >= DEV_BSIZE)
		blkno = bp->b_blkno / (secsize / DEV_BSIZE);
	else
		blkno = bp->b_blkno * (DEV_BSIZE / secsize);

	if (part != RAW_PART)
		blkno += lp->d_partitions[DISKPART(bp->b_dev)].p_offset;
	bp->b_rawblkno = blkno;

	return -1;

done:
	bp->b_resid = bp->b_bcount;
	return bp->b_error;
}

void
dk_strategy(struct dk_softc *dksc, struct buf *bp)
{
	int error;

	DPRINTF_FOLLOW(("dk_strategy(%s, %p, %p)\n",
	    dksc->sc_xname, dksc, bp));

	if (!(dksc->sc_flags & DKF_INITED)) {
		DPRINTF_FOLLOW(("dk_strategy: not inited\n"));
		bp->b_error  = ENXIO;
		biodone(bp);
		return;
	}

	error = dk_translate(dksc, bp);
	if (error >= 0) {
		biodone(bp);
		return;
	}

	/*
	 * Queue buffer and start unit
	 */
	dk_start(dksc, bp);
}

void
dk_start(struct dk_softc *dksc, struct buf *bp)
{
	const struct dkdriver *dkd = dksc->sc_dkdev.dk_driver;
	int error;

	mutex_enter(&dksc->sc_iolock);

	if (bp != NULL)
		bufq_put(dksc->sc_bufq, bp);

	if (dksc->sc_busy)
		goto done;
	dksc->sc_busy = true;

	/*
	 * Peeking at the buffer queue and committing the operation
	 * only after success isn't atomic.
	 *
	 * So when a diskstart fails, the buffer is saved
	 * and tried again before the next buffer is fetched.
	 * dk_drain() handles flushing of a saved buffer.
	 *
	 * This keeps order of I/O operations, unlike bufq_put.
	 */

	bp = dksc->sc_deferred;
	dksc->sc_deferred = NULL;

	if (bp == NULL)
		bp = bufq_get(dksc->sc_bufq);

	while (bp != NULL) {

		disk_busy(&dksc->sc_dkdev);
		mutex_exit(&dksc->sc_iolock);
		error = dkd->d_diskstart(dksc->sc_dev, bp);
		mutex_enter(&dksc->sc_iolock);
		if (error == EAGAIN) {
			dksc->sc_deferred = bp;
			disk_unbusy(&dksc->sc_dkdev, 0, (bp->b_flags & B_READ));
			break;
		}

		if (error != 0) {
			bp->b_error = error;
			bp->b_resid = bp->b_bcount;
			dk_done1(dksc, bp, false);
		}

		bp = bufq_get(dksc->sc_bufq);
	}

	dksc->sc_busy = false;
done:
	mutex_exit(&dksc->sc_iolock);
}

static void
dk_done1(struct dk_softc *dksc, struct buf *bp, bool lock)
{
	struct disk *dk = &dksc->sc_dkdev;

	if (bp->b_error != 0) {
		struct cfdriver *cd = device_cfdriver(dksc->sc_dev);

		diskerr(bp, cd->cd_name, "error", LOG_PRINTF, 0,
			dk->dk_label);
		printf("\n");
	}

	if (lock)
		mutex_enter(&dksc->sc_iolock);
	disk_unbusy(dk, bp->b_bcount - bp->b_resid, (bp->b_flags & B_READ));
	if (lock)
		mutex_exit(&dksc->sc_iolock);

	rnd_add_uint32(&dksc->sc_rnd_source, bp->b_rawblkno);

	biodone(bp);
}

void
dk_done(struct dk_softc *dksc, struct buf *bp)
{
	dk_done1(dksc, bp, true);
}

void
dk_drain(struct dk_softc *dksc)
{
	struct buf *bp;

	mutex_enter(&dksc->sc_iolock);
	bp = dksc->sc_deferred;
	if (bp != NULL) {
		bp->b_error = EIO;
		bp->b_resid = bp->b_bcount;
		biodone(bp); 
	}
	bufq_drain(dksc->sc_bufq);
	mutex_exit(&dksc->sc_iolock);
}

int
dk_discard(struct dk_softc *dksc, dev_t dev, off_t pos, off_t len)
{
	const struct dkdriver *dkd = dksc->sc_dkdev.dk_driver;
	unsigned secsize = dksc->sc_dkdev.dk_geom.dg_secsize;
	struct buf tmp, *bp = &tmp;
	int error;

	DPRINTF_FOLLOW(("dk_discard(%s, %p, 0x"PRIx64", %jd, %jd)\n",
	    dksc->sc_xname, dksc, (intmax_t)pos, (intmax_t)len));

	if (!(dksc->sc_flags & DKF_INITED)) {
		DPRINTF_FOLLOW(("dk_discard: not inited\n"));
		return ENXIO;
	}

	if (secsize == 0 || (pos % secsize) != 0)
		return EINVAL;

	/* enough data to please the bounds checking code */
	bp->b_dev = dev;
	bp->b_blkno = (daddr_t)(pos / secsize);
	bp->b_bcount = len;
	bp->b_flags = B_WRITE;

	error = dk_translate(dksc, bp);
	if (error >= 0)
		return error;

	error = dkd->d_discard(dksc->sc_dev,
		(off_t)bp->b_rawblkno * secsize,
		(off_t)bp->b_bcount);

	return error;
}

int
dk_size(struct dk_softc *dksc, dev_t dev)
{
	const struct dkdriver *dkd = dksc->sc_dkdev.dk_driver;
	struct	disklabel *lp;
	int	is_open;
	int	part;
	int	size;

	if ((dksc->sc_flags & DKF_INITED) == 0)
		return -1;

	part = DISKPART(dev);
	is_open = dksc->sc_dkdev.dk_openmask & (1 << part);

	if (!is_open && dkd->d_open(dev, 0, S_IFBLK, curlwp))
		return -1;

	lp = dksc->sc_dkdev.dk_label;
	if (lp->d_partitions[part].p_fstype != FS_SWAP)
		size = -1;
	else
		size = lp->d_partitions[part].p_size *
		    (lp->d_secsize / DEV_BSIZE);

	if (!is_open && dkd->d_close(dev, 0, S_IFBLK, curlwp))
		return -1;

	return size;
}

int
dk_ioctl(struct dk_softc *dksc, dev_t dev,
	    u_long cmd, void *data, int flag, struct lwp *l)
{
	const struct dkdriver *dkd = dksc->sc_dkdev.dk_driver;
	struct	disklabel *lp;
	struct	disk *dk = &dksc->sc_dkdev;
#ifdef __HAVE_OLD_DISKLABEL
	struct	disklabel newlabel;
#endif
	int	error;

	DPRINTF_FOLLOW(("dk_ioctl(%s, %p, 0x%"PRIx64", 0x%lx)\n",
	    dksc->sc_xname, dksc, dev, cmd));

	/* ensure that the pseudo disk is open for writes for these commands */
	switch (cmd) {
	case DIOCSDINFO:
	case DIOCWDINFO:
#ifdef __HAVE_OLD_DISKLABEL
	case ODIOCSDINFO:
	case ODIOCWDINFO:
#endif
	case DIOCKLABEL:
	case DIOCWLABEL:
	case DIOCAWEDGE:
	case DIOCDWEDGE:
	case DIOCSSTRATEGY:
		if ((flag & FWRITE) == 0)
			return EBADF;
	}

	/* ensure that the pseudo-disk is initialized for these */
	switch (cmd) {
	case DIOCGDINFO:
	case DIOCSDINFO:
	case DIOCWDINFO:
	case DIOCGPART:
	case DIOCKLABEL:
	case DIOCWLABEL:
	case DIOCGDEFLABEL:
	case DIOCAWEDGE:
	case DIOCDWEDGE:
	case DIOCLWEDGES:
	case DIOCMWEDGES:
	case DIOCCACHESYNC:
#ifdef __HAVE_OLD_DISKLABEL
	case ODIOCGDINFO:
	case ODIOCSDINFO:
	case ODIOCWDINFO:
	case ODIOCGDEFLABEL:
#endif
		if ((dksc->sc_flags & DKF_INITED) == 0)
			return ENXIO;
	}

	error = disk_ioctl(dk, dev, cmd, data, flag, l);
	if (error != EPASSTHROUGH)
		return error;
	else
		error = 0;

	switch (cmd) {
	case DIOCWDINFO:
	case DIOCSDINFO:
#ifdef __HAVE_OLD_DISKLABEL
	case ODIOCWDINFO:
	case ODIOCSDINFO:
#endif
#ifdef __HAVE_OLD_DISKLABEL
		if (cmd == ODIOCSDINFO || cmd == ODIOCWDINFO) {
			memset(&newlabel, 0, sizeof newlabel);
			memcpy(&newlabel, data, sizeof (struct olddisklabel));
			lp = &newlabel;
		} else
#endif
		lp = (struct disklabel *)data;

		mutex_enter(&dk->dk_openlock);
		dksc->sc_flags |= DKF_LABELLING;

		error = setdisklabel(dksc->sc_dkdev.dk_label,
		    lp, 0, dksc->sc_dkdev.dk_cpulabel);
		if (error == 0) {
			if (cmd == DIOCWDINFO
#ifdef __HAVE_OLD_DISKLABEL
			    || cmd == ODIOCWDINFO
#endif
			   )
				error = writedisklabel(DKLABELDEV(dev),
				    dkd->d_strategy, dksc->sc_dkdev.dk_label,
				    dksc->sc_dkdev.dk_cpulabel);
		}

		dksc->sc_flags &= ~DKF_LABELLING;
		mutex_exit(&dk->dk_openlock);
		break;

	case DIOCKLABEL:
		if (*(int *)data != 0)
			dksc->sc_flags |= DKF_KLABEL;
		else
			dksc->sc_flags &= ~DKF_KLABEL;
		break;

	case DIOCWLABEL:
		if (*(int *)data != 0)
			dksc->sc_flags |= DKF_WLABEL;
		else
			dksc->sc_flags &= ~DKF_WLABEL;
		break;

	case DIOCGDEFLABEL:
		dk_getdefaultlabel(dksc, (struct disklabel *)data);
		break;

#ifdef __HAVE_OLD_DISKLABEL
	case ODIOCGDEFLABEL:
		dk_getdefaultlabel(dksc, &newlabel);
		if (newlabel.d_npartitions > OLDMAXPARTITIONS)
			return ENOTTY;
		memcpy(data, &newlabel, sizeof (struct olddisklabel));
		break;
#endif

	case DIOCGSTRATEGY:
	    {
		struct disk_strategy *dks = (void *)data;

		mutex_enter(&dksc->sc_iolock);
		strlcpy(dks->dks_name, bufq_getstrategyname(dksc->sc_bufq),
		    sizeof(dks->dks_name));
		mutex_exit(&dksc->sc_iolock);
		dks->dks_paramlen = 0;

		return 0;
	    }

	case DIOCSSTRATEGY:
	    {
		struct disk_strategy *dks = (void *)data;
		struct bufq_state *new;
		struct bufq_state *old;

		if (dks->dks_param != NULL) {
			return EINVAL;
		}
		dks->dks_name[sizeof(dks->dks_name) - 1] = 0; /* ensure term */
		error = bufq_alloc(&new, dks->dks_name,
		    BUFQ_EXACT|BUFQ_SORT_RAWBLOCK);
		if (error) {
			return error;
		}
		mutex_enter(&dksc->sc_iolock);
		old = dksc->sc_bufq;
		bufq_move(new, old);
		dksc->sc_bufq = new;
		mutex_exit(&dksc->sc_iolock);
		bufq_free(old);

		return 0;
	    }

	default:
		error = ENOTTY;
	}

	return error;
}

/*
 * dk_dump dumps all of physical memory into the partition specified.
 * This requires substantially more framework than {s,w}ddump, and hence
 * is probably much more fragile.
 *
 */

#define DKF_READYFORDUMP	(DKF_INITED|DKF_TAKEDUMP)
#define DKFF_READYFORDUMP(x)	(((x) & DKF_READYFORDUMP) == DKF_READYFORDUMP)
static volatile int	dk_dumping = 0;

/* ARGSUSED */
int
dk_dump(struct dk_softc *dksc, dev_t dev,
    daddr_t blkno, void *vav, size_t size)
{
	const struct dkdriver *dkd = dksc->sc_dkdev.dk_driver;
	char *va = vav;
	struct disklabel *lp;
	int part, towrt, nsects, sectoff, maxblkcnt, nblk;
	int maxxfer, rv = 0;

	/*
	 * ensure that we consider this device to be safe for dumping,
	 * and that the device is configured.
	 */
	if (!DKFF_READYFORDUMP(dksc->sc_flags))
		return ENXIO;

	/* ensure that we are not already dumping */
	if (dk_dumping)
		return EFAULT;
	dk_dumping = 1;

	if (dkd->d_dumpblocks == NULL)
		return ENXIO;

	/* device specific max transfer size */
	maxxfer = MAXPHYS;
	if (dkd->d_iosize != NULL)
		(*dkd->d_iosize)(dksc->sc_dev, &maxxfer);

	/* Convert to disk sectors.  Request must be a multiple of size. */
	part = DISKPART(dev);
	lp = dksc->sc_dkdev.dk_label;
	if ((size % lp->d_secsize) != 0)
		return (EFAULT);
	towrt = size / lp->d_secsize;
	blkno = dbtob(blkno) / lp->d_secsize;   /* blkno in secsize units */

	nsects = lp->d_partitions[part].p_size;
	sectoff = lp->d_partitions[part].p_offset;

	/* Check transfer bounds against partition size. */
	if ((blkno < 0) || ((blkno + towrt) > nsects))
		return (EINVAL);

	/* Offset block number to start of partition. */
	blkno += sectoff;

	/* Start dumping and return when done. */
	maxblkcnt = howmany(maxxfer, lp->d_secsize);
	while (towrt > 0) {
		nblk = min(maxblkcnt, towrt);

		if ((rv = (*dkd->d_dumpblocks)(dksc->sc_dev, va, blkno, nblk)) != 0)
			return (rv);

		towrt -= nblk;
		blkno += nblk;
		va += nblk * lp->d_secsize;
	}

	dk_dumping = 0;

	return 0;
}

/* ARGSUSED */
void
dk_getdefaultlabel(struct dk_softc *dksc, struct disklabel *lp)
{
	struct disk_geom *dg = &dksc->sc_dkdev.dk_geom;

	memset(lp, 0, sizeof(*lp));

	if (dg->dg_secperunit > UINT32_MAX)
		lp->d_secperunit = UINT32_MAX;
	else
		lp->d_secperunit = dg->dg_secperunit;
	lp->d_secsize = dg->dg_secsize;
	lp->d_nsectors = dg->dg_nsectors;
	lp->d_ntracks = dg->dg_ntracks;
	lp->d_ncylinders = dg->dg_ncylinders;
	lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;

	strlcpy(lp->d_typename, dksc->sc_xname, sizeof(lp->d_typename));
	lp->d_type = dksc->sc_dtype;
	strlcpy(lp->d_packname, "fictitious", sizeof(lp->d_packname));
	lp->d_rpm = 3600;
	lp->d_interleave = 1;
	lp->d_flags = 0;

	lp->d_partitions[RAW_PART].p_offset = 0;
	lp->d_partitions[RAW_PART].p_size = lp->d_secperunit;
	lp->d_partitions[RAW_PART].p_fstype = FS_UNUSED;
	lp->d_npartitions = RAW_PART + 1;

	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = dkcksum(dksc->sc_dkdev.dk_label);
}

/* ARGSUSED */
void
dk_getdisklabel(struct dk_softc *dksc, dev_t dev)
{
	const struct dkdriver *dkd = dksc->sc_dkdev.dk_driver;
	struct	 disklabel *lp = dksc->sc_dkdev.dk_label;
	struct	 cpu_disklabel *clp = dksc->sc_dkdev.dk_cpulabel;
	struct   disk_geom *dg = &dksc->sc_dkdev.dk_geom;
	struct	 partition *pp;
	int	 i;
	const char	*errstring;

	memset(clp, 0x0, sizeof(*clp));
	dk_getdefaultlabel(dksc, lp);
	errstring = readdisklabel(DKLABELDEV(dev), dkd->d_strategy,
	    dksc->sc_dkdev.dk_label, dksc->sc_dkdev.dk_cpulabel);
	if (errstring) {
		dk_makedisklabel(dksc);
		if (dksc->sc_flags & DKF_WARNLABEL)
			printf("%s: %s\n", dksc->sc_xname, errstring);
		return;
	}

	if ((dksc->sc_flags & DKF_LABELSANITY) == 0)
		return;

	/* Sanity check */
	if (lp->d_secperunit < UINT32_MAX ?
		lp->d_secperunit != dg->dg_secperunit :
		lp->d_secperunit > dg->dg_secperunit)
		printf("WARNING: %s: total sector size in disklabel (%ju) "
		    "!= the size of %s (%ju)\n", dksc->sc_xname,
		    (uintmax_t)lp->d_secperunit, dksc->sc_xname,
		    (uintmax_t)dg->dg_secperunit);

	for (i=0; i < lp->d_npartitions; i++) {
		pp = &lp->d_partitions[i];
		if (pp->p_offset + pp->p_size > dg->dg_secperunit)
			printf("WARNING: %s: end of partition `%c' exceeds "
			    "the size of %s (%ju)\n", dksc->sc_xname,
			    'a' + i, dksc->sc_xname,
			    (uintmax_t)dg->dg_secperunit);
	}
}

/* ARGSUSED */
static void
dk_makedisklabel(struct dk_softc *dksc)
{
	struct	disklabel *lp = dksc->sc_dkdev.dk_label;

	lp->d_partitions[RAW_PART].p_fstype = FS_BSDFFS;
	strlcpy(lp->d_packname, "default label", sizeof(lp->d_packname));
	lp->d_checksum = dkcksum(lp);
}

/* This function is taken from ccd.c:1.76  --rcd */

/*
 * XXX this function looks too generic for dksubr.c, shouldn't we
 *     put it somewhere better?
 */

/*
 * Lookup the provided name in the filesystem.  If the file exists,
 * is a valid block device, and isn't being used by anyone else,
 * set *vpp to the file's vnode.
 */
int
dk_lookup(struct pathbuf *pb, struct lwp *l, struct vnode **vpp)
{
	struct nameidata nd;
	struct vnode *vp;
	int     error;

	if (l == NULL)
		return ESRCH;	/* Is ESRCH the best choice? */

	NDINIT(&nd, LOOKUP, FOLLOW, pb);
	if ((error = vn_open(&nd, FREAD | FWRITE, 0)) != 0) {
		DPRINTF((DKDB_FOLLOW|DKDB_INIT),
		    ("dk_lookup: vn_open error = %d\n", error));
		return error;
	}

	vp = nd.ni_vp;
	if (vp->v_type != VBLK) {
		error = ENOTBLK;
		goto out;
	}

	/* Reopen as anonymous vnode to protect against forced unmount. */
	if ((error = bdevvp(vp->v_rdev, vpp)) != 0)
		goto out;
	VOP_UNLOCK(vp);
	if ((error = vn_close(vp, FREAD | FWRITE, l->l_cred)) != 0) {
		vrele(*vpp);
		return error;
	}
	if ((error = VOP_OPEN(*vpp, FREAD | FWRITE, l->l_cred)) != 0) {
		vrele(*vpp);
		return error;
	}
	mutex_enter((*vpp)->v_interlock);
	(*vpp)->v_writecount++;
	mutex_exit((*vpp)->v_interlock);

	IFDEBUG(DKDB_VNODE, vprint("dk_lookup: vnode info", *vpp));

	return 0;
out:
	VOP_UNLOCK(vp);
	(void) vn_close(vp, FREAD | FWRITE, l->l_cred);
	return error;
}

MODULE(MODULE_CLASS_MISC, dk_subr, NULL);

static int
dk_subr_modcmd(modcmd_t cmd, void *arg)
{
	switch (cmd) {
	case MODULE_CMD_INIT:
	case MODULE_CMD_FINI:
		return 0;
	case MODULE_CMD_STAT:
	case MODULE_CMD_AUTOUNLOAD:
	default:
		return ENOTTY;
	}
}
