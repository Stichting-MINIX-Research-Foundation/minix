/*	$NetBSD: ed_mca.c,v 1.64 2015/04/26 15:15:20 mlelstv Exp $	*/

/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jaromir Dolecek.
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
 * Disk drive goo for MCA ESDI controller driver.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ed_mca.c,v 1.64 2015/04/26 15:15:20 mlelstv Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/syslog.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/rndsource.h>

#include <sys/intr.h>
#include <sys/bus.h>

#include <dev/mca/mcavar.h>

#include <dev/mca/edcreg.h>
#include <dev/mca/edvar.h>
#include <dev/mca/edcvar.h>

/* #define ATADEBUG */

#ifdef ATADEBUG
#define ATADEBUG_PRINT(args, level)  printf args
#else
#define ATADEBUG_PRINT(args, level)
#endif

#define	EDLABELDEV(dev) (MAKEDISKDEV(major(dev), DISKUNIT(dev), RAW_PART))

static int     ed_mca_probe  (device_t, cfdata_t, void *);
static void    ed_mca_attach (device_t, device_t, void *);

CFATTACH_DECL_NEW(ed_mca, sizeof(struct ed_softc),
    ed_mca_probe, ed_mca_attach, NULL, NULL);

extern struct cfdriver ed_cd;

static int	ed_get_params(struct ed_softc *, int *);
static void	edgetdisklabel(dev_t, struct ed_softc *);
static void	edgetdefaultlabel(struct ed_softc *, struct disklabel *);

dev_type_open(edmcaopen);
dev_type_close(edmcaclose);
dev_type_read(edmcaread);
dev_type_write(edmcawrite);
dev_type_ioctl(edmcaioctl);
dev_type_strategy(edmcastrategy);
dev_type_dump(edmcadump);
dev_type_size(edmcasize);

const struct bdevsw ed_bdevsw = {
	.d_open = edmcaopen,
	.d_close = edmcaclose,
	.d_strategy = edmcastrategy,
	.d_ioctl = edmcaioctl,
	.d_dump = edmcadump,
	.d_psize = edmcasize,
	.d_discard = nodiscard,
	.d_flag = D_DISK
};

const struct cdevsw ed_cdevsw = {
	.d_open = edmcaopen,
	.d_close = edmcaclose,
	.d_read = edmcaread,
	.d_write = edmcawrite,
	.d_ioctl = edmcaioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_DISK
};

static struct dkdriver eddkdriver = {
	.d_strategy = edmcastrategy,
	.d_minphys = minphys
};

/*
 * Just check if it's possible to identify the disk.
 */
static int
ed_mca_probe(device_t parent, cfdata_t cf, void *aux)
{
	struct edc_mca_softc *sc = device_private(parent);
	struct ed_attach_args *eda = aux;
	u_int16_t cmd_args[2];
	int found = 1;

	/*
	 * Get Device Configuration (09).
	 */
	cmd_args[0] = 14;	/* Options: 00s110, s: 0=Physical 1=Pseudo */
	cmd_args[1] = 0;
	if (edc_run_cmd(sc, CMD_GET_DEV_CONF, eda->edc_drive, cmd_args, 2, 1))
		found = 0;

	return (found);
}

static void
ed_mca_attach(device_t parent, device_t self, void *aux)
{
	struct ed_softc *ed = device_private(self);
	struct edc_mca_softc *sc = device_private(parent);
	struct ed_attach_args *eda = aux;
	char pbuf[8];
	int drv_flags;

	ed->sc_dev = self;
	ed->edc_softc = sc;
	ed->sc_devno  = eda->edc_drive;
	edc_add_disk(sc, ed);

	bufq_alloc(&ed->sc_q, "disksort", BUFQ_SORT_RAWBLOCK);
	mutex_init(&ed->sc_q_lock, MUTEX_DEFAULT, IPL_VM);

	if (ed_get_params(ed, &drv_flags)) {
		printf(": IDENTIFY failed, no disk found\n");
		return;
	}

	format_bytes(pbuf, sizeof(pbuf),
		(u_int64_t) ed->sc_capacity * DEV_BSIZE);
	printf(": %s, %u cyl, %u head, %u sec, 512 bytes/sect x %u sectors\n",
		pbuf,
		ed->cyl, ed->heads, ed->sectors,
		ed->sc_capacity);

	printf("%s: %u spares/cyl, %s, %s, %s, %s, %s\n",
		device_xname(ed->sc_dev), ed->spares,
		(drv_flags & (1 << 0)) ? "NoRetries" : "Retries",
		(drv_flags & (1 << 1)) ? "Removable" : "Fixed",
		(drv_flags & (1 << 2)) ? "SkewedFormat" : "NoSkew",
		(drv_flags & (1 << 3)) ? "ZeroDefect" : "Defects",
		(drv_flags & (1 << 4)) ? "InvalidSecondary" : "SecondaryOK"
		);

	/*
	 * Initialize and attach the disk structure.
	 */
	disk_init(&ed->sc_dk, device_xname(ed->sc_dev), &eddkdriver);
	disk_attach(&ed->sc_dk);
	rnd_attach_source(&ed->rnd_source, device_xname(ed->sc_dev),
			  RND_TYPE_DISK, RND_FLAG_DEFAULT);

	ed->sc_flags |= EDF_INIT;

	/*
	 * XXX We should try to discovery wedges here, but
	 * XXX that would mean being able to do I/O.  Should
	 * XXX use config_defer() here.
	 */
}

/*
 * Read/write routine for a buffer.  Validates the arguments and schedules the
 * transfer.  Does not wait for the transfer to complete.
 */
void
edmcastrategy(struct buf *bp)
{
	struct ed_softc *ed;
	struct disklabel *lp;
	daddr_t blkno;

	ed = device_lookup_private(&ed_cd, DISKUNIT(bp->b_dev));
	lp = ed->sc_dk.dk_label;

	ATADEBUG_PRINT(("edmcastrategy (%s)\n", device_xname(ed->sc_dev)),
	    DEBUG_XFERS);

	/* Valid request?  */
	if (bp->b_blkno < 0 ||
	    (bp->b_bcount % lp->d_secsize) != 0 ||
	    (bp->b_bcount / lp->d_secsize) >= (1 << NBBY)) {
		bp->b_error = EINVAL;
		goto done;
	}

	/* If device invalidated (e.g. media change, door open), error. */
	if ((ed->sc_flags & WDF_LOADED) == 0) {
		bp->b_error = EIO;
		goto done;
	}

	/* If it's a null transfer, return immediately. */
	if (bp->b_bcount == 0)
		goto done;

	/*
	 * Do bounds checking, adjust transfer. if error, process.
	 * If end of partition, just return.
	 */
	if (DISKPART(bp->b_dev) != RAW_PART &&
	    bounds_check_with_label(&ed->sc_dk, bp,
	    (ed->sc_flags & (WDF_WLABEL|WDF_LABELLING)) != 0) <= 0)
		goto done;

	/*
	 * Now convert the block number to absolute and put it in
	 * terms of the device's logical block size.
	 */
	if (lp->d_secsize >= DEV_BSIZE)
		blkno = bp->b_blkno / (lp->d_secsize / DEV_BSIZE);
	else
		blkno = bp->b_blkno * (DEV_BSIZE / lp->d_secsize);

	if (DISKPART(bp->b_dev) != RAW_PART)
		blkno += lp->d_partitions[DISKPART(bp->b_dev)].p_offset;

	bp->b_rawblkno = blkno;

	/* Queue transfer on drive, activate drive and controller if idle. */
	mutex_enter(&ed->sc_q_lock);
	bufq_put(ed->sc_q, bp);
	mutex_exit(&ed->sc_q_lock);

	/* Ring the worker thread */
	wakeup(ed->edc_softc);

	return;
done:
	/* Toss transfer; we're done early. */
	bp->b_resid = bp->b_bcount;
	biodone(bp);
}

int
edmcaread(dev_t dev, struct uio *uio, int flags)
{
	ATADEBUG_PRINT(("edread\n"), DEBUG_XFERS);
	return (physio(edmcastrategy, NULL, dev, B_READ, minphys, uio));
}

int
edmcawrite(dev_t dev, struct uio *uio, int flags)
{
	ATADEBUG_PRINT(("edwrite\n"), DEBUG_XFERS);
	return (physio(edmcastrategy, NULL, dev, B_WRITE, minphys, uio));
}

int
edmcaopen(dev_t dev, int flag, int fmt, struct lwp *l)
{
	struct ed_softc *wd;
	int part, error;

	ATADEBUG_PRINT(("edopen\n"), DEBUG_FUNCS);
	wd = device_lookup_private(&ed_cd, DISKUNIT(dev));
	if (wd == NULL || (wd->sc_flags & EDF_INIT) == 0)
		return (ENXIO);

	part = DISKPART(dev);

	mutex_enter(&wd->sc_dk.dk_openlock);

	/*
	 * If there are wedges, and this is not RAW_PART, then we
	 * need to fail.
	 */
	if (wd->sc_dk.dk_nwedges != 0 && part != RAW_PART) {
		error = EBUSY;
		goto bad1;
	}

	if (wd->sc_dk.dk_openmask != 0) {
		/*
		 * If any partition is open, but the disk has been invalidated,
		 * disallow further opens.
		 */
		if ((wd->sc_flags & WDF_LOADED) == 0) {
			error = EIO;
			goto bad1;
		}
	} else {
		if ((wd->sc_flags & WDF_LOADED) == 0) {
			int s;

			wd->sc_flags |= WDF_LOADED;

			/* Load the physical device parameters. */
			s = splbio();
			ed_get_params(wd, NULL);
			splx(s);

			/* Load the partition info if not already loaded. */
			edgetdisklabel(dev, wd);
		}
	}

	/* Check that the partition exists. */
	if (part != RAW_PART &&
	    (part >= wd->sc_dk.dk_label->d_npartitions ||
	     wd->sc_dk.dk_label->d_partitions[part].p_fstype == FS_UNUSED)) {
		error = ENXIO;
		goto bad1;
	}

	/* Insure only one open at a time. */
	switch (fmt) {
	case S_IFCHR:
		wd->sc_dk.dk_copenmask |= (1 << part);
		break;
	case S_IFBLK:
		wd->sc_dk.dk_bopenmask |= (1 << part);
		break;
	}
	wd->sc_dk.dk_openmask =
	    wd->sc_dk.dk_copenmask | wd->sc_dk.dk_bopenmask;

	error = 0;
 bad1:
	mutex_exit(&wd->sc_dk.dk_openlock);
	return (error);
}

int
edmcaclose(dev_t dev, int flag, int fmt, struct lwp *l)
{
	struct ed_softc *wd = device_lookup_private(&ed_cd, DISKUNIT(dev));
	int part = DISKPART(dev);

	ATADEBUG_PRINT(("edmcaclose\n"), DEBUG_FUNCS);

	mutex_enter(&wd->sc_dk.dk_openlock);

	switch (fmt) {
	case S_IFCHR:
		wd->sc_dk.dk_copenmask &= ~(1 << part);
		break;
	case S_IFBLK:
		wd->sc_dk.dk_bopenmask &= ~(1 << part);
		break;
	}
	wd->sc_dk.dk_openmask =
	    wd->sc_dk.dk_copenmask | wd->sc_dk.dk_bopenmask;

	if (wd->sc_dk.dk_openmask == 0) {
#if 0
		wd_flushcache(wd, AT_WAIT);
#endif
		/* XXXX Must wait for I/O to complete! */

		if (! (wd->sc_flags & WDF_KLABEL))
			wd->sc_flags &= ~WDF_LOADED;
	}

	mutex_exit(&wd->sc_dk.dk_openlock);

	return 0;
}

static void
edgetdefaultlabel(struct ed_softc *ed, struct disklabel *lp)
{
	ATADEBUG_PRINT(("edgetdefaultlabel\n"), DEBUG_FUNCS);
	memset(lp, 0, sizeof(struct disklabel));

	lp->d_secsize = DEV_BSIZE;
	lp->d_ntracks = ed->heads;
	lp->d_nsectors = ed->sectors;
	lp->d_ncylinders = ed->cyl;
	lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;

	lp->d_type = DKTYPE_ESDI;

	strncpy(lp->d_typename, "ESDI", 16);
	strncpy(lp->d_packname, "fictitious", 16);
	lp->d_secperunit = ed->sc_capacity;
	lp->d_rpm = 3600;
	lp->d_interleave = 1;
	lp->d_flags = 0;

	lp->d_partitions[RAW_PART].p_offset = 0;
	lp->d_partitions[RAW_PART].p_size =
	lp->d_secperunit * (lp->d_secsize / DEV_BSIZE);
	lp->d_partitions[RAW_PART].p_fstype = FS_UNUSED;
	lp->d_npartitions = RAW_PART + 1;

	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = dkcksum(lp);
}

/*
 * Fabricate a default disk label, and try to read the correct one.
 */
static void
edgetdisklabel(dev_t dev, struct ed_softc *ed)
{
	struct disklabel *lp = ed->sc_dk.dk_label;
	const char *errstring;

	ATADEBUG_PRINT(("edgetdisklabel\n"), DEBUG_FUNCS);

	memset(ed->sc_dk.dk_cpulabel, 0, sizeof(struct cpu_disklabel));

	edgetdefaultlabel(ed, lp);

	errstring = readdisklabel(
	    EDLABELDEV(dev), edmcastrategy, lp, ed->sc_dk.dk_cpulabel);
	if (errstring) {
		/*
		 * This probably happened because the drive's default
		 * geometry doesn't match the DOS geometry.  We
		 * assume the DOS geometry is now in the label and try
		 * again.  XXX This is a kluge.
		 */
#if 0
		if (wd->drvp->state > RECAL)
			wd->drvp->drive_flags |= ATA_DRIVE_RESET;
#endif
		errstring = readdisklabel(EDLABELDEV(dev),
			edmcastrategy, lp, ed->sc_dk.dk_cpulabel);
	}
	if (errstring) {
		printf("%s: %s\n", device_xname(ed->sc_dev), errstring);
		return;
	}
}

int
edmcaioctl(dev_t dev, u_long xfer, void *addr, int flag, struct lwp *l)
{
	struct ed_softc *ed = device_lookup_private(&ed_cd, DISKUNIT(dev));
	int error;

	ATADEBUG_PRINT(("edioctl\n"), DEBUG_FUNCS);

	if ((ed->sc_flags & WDF_LOADED) == 0)
		return EIO;

        error = disk_ioctl(&ed->sc_dk, dev, xfer, addr, flag, l);
	if (error != EPASSTHROUGH)
		return error;

	switch (xfer) {
	case DIOCWDINFO:
	case DIOCSDINFO:
	{
		struct disklabel *lp;

		lp = (struct disklabel *)addr;

		if ((flag & FWRITE) == 0)
			return EBADF;

		mutex_enter(&ed->sc_dk.dk_openlock);
		ed->sc_flags |= WDF_LABELLING;

		error = setdisklabel(ed->sc_dk.dk_label,
		    lp, /*wd->sc_dk.dk_openmask : */0,
		    ed->sc_dk.dk_cpulabel);
		if (error == 0) {
#if 0
			if (wd->drvp->state > RECAL)
				wd->drvp->drive_flags |= ATA_DRIVE_RESET;
#endif
			if (xfer == DIOCWDINFO)
				error = writedisklabel(EDLABELDEV(dev),
				    edmcastrategy, ed->sc_dk.dk_label,
				    ed->sc_dk.dk_cpulabel);
		}

		ed->sc_flags &= ~WDF_LABELLING;
		mutex_exit(&ed->sc_dk.dk_openlock);
		return (error);
	}

	case DIOCKLABEL:
		if (*(int *)addr)
			ed->sc_flags |= WDF_KLABEL;
		else
			ed->sc_flags &= ~WDF_KLABEL;
		return 0;

	case DIOCWLABEL:
		if ((flag & FWRITE) == 0)
			return EBADF;
		if (*(int *)addr)
			ed->sc_flags |= WDF_WLABEL;
		else
			ed->sc_flags &= ~WDF_WLABEL;
		return 0;

	case DIOCGDEFLABEL:
		edgetdefaultlabel(ed, (struct disklabel *)addr);
		return 0;

#if 0
	case DIOCWFORMAT:
		if ((flag & FWRITE) == 0)
			return EBADF;
		{
		register struct format_op *fop;
		struct iovec aiov;
		struct uio auio;

		fop = (struct format_op *)addr;
		aiov.iov_base = fop->df_buf;
		aiov.iov_len = fop->df_count;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_resid = fop->df_count;
		auio.uio_segflg = 0;
		auio.uio_offset =
			fop->df_startblk * wd->sc_dk.dk_label->d_secsize;
		auio.uio_lwp = l;
		error = physio(wdformat, NULL, dev, B_WRITE, minphys,
		    &auio);
		fop->df_count -= auio.uio_resid;
		fop->df_reg[0] = wdc->sc_status;
		fop->df_reg[1] = wdc->sc_error;
		return error;
		}
#endif

	default:
		return ENOTTY;
	}

#ifdef DIAGNOSTIC
	panic("edioctl: impossible");
#endif
}

int
edmcasize(dev_t dev)
{
	struct ed_softc *wd;
	int part, omask;
	int size;

	ATADEBUG_PRINT(("edsize\n"), DEBUG_FUNCS);

	wd = device_lookup_private(&ed_cd, DISKUNIT(dev));
	if (wd == NULL)
		return (-1);

	part = DISKPART(dev);
	omask = wd->sc_dk.dk_openmask & (1 << part);

	if (omask == 0 && edmcaopen(dev, 0, S_IFBLK, NULL) != 0)
		return (-1);
	if (wd->sc_dk.dk_label->d_partitions[part].p_fstype != FS_SWAP)
		size = -1;
	else
		size = wd->sc_dk.dk_label->d_partitions[part].p_size *
		    (wd->sc_dk.dk_label->d_secsize / DEV_BSIZE);
	if (omask == 0 && edmcaclose(dev, 0, S_IFBLK, NULL) != 0)
		return (-1);
	return (size);
}

/* #define WD_DUMP_NOT_TRUSTED if you just want to watch */
static int eddoingadump = 0;
static int eddumprecalibrated = 0;
static int eddumpmulti = 1;

/*
 * Dump core after a system crash.
 */
int
edmcadump(dev_t dev, daddr_t blkno, void *va, size_t size)
{
	struct ed_softc *ed;	/* disk unit to do the I/O */
	struct disklabel *lp;   /* disk's disklabel */
	int part;
	int nblks;	/* total number of sectors left to write */
	int error;

	/* Check if recursive dump; if so, punt. */
	if (eddoingadump)
		return EFAULT;
	eddoingadump = 1;

	ed = device_lookup_private(&ed_cd, DISKUNIT(dev));
	if (ed == NULL)
		return (ENXIO);

	part = DISKPART(dev);

	/* Make sure it was initialized. */
	if ((ed->sc_flags & EDF_INIT) == 0)
		return ENXIO;

	/* Convert to disk sectors.  Request must be a multiple of size. */
	lp = ed->sc_dk.dk_label;
	if ((size % lp->d_secsize) != 0)
		return EFAULT;
	nblks = size / lp->d_secsize;
	blkno = blkno / (lp->d_secsize / DEV_BSIZE);

	/* Check transfer bounds against partition size. */
	if ((blkno < 0) || ((blkno + nblks) > lp->d_partitions[part].p_size))
		return EINVAL;

	/* Offset block number to start of partition. */
	blkno += lp->d_partitions[part].p_offset;

	/* Recalibrate, if first dump transfer. */
	if (eddumprecalibrated == 0) {
		eddumprecalibrated = 1;
		eddumpmulti = 8;
#if 0
		wd->drvp->state = RESET;
#endif
	}

	while (nblks > 0) {
		error = edc_bio(ed->edc_softc, ed, va, blkno,
			min(nblks, eddumpmulti) * lp->d_secsize, 0, 1);
		if (error)
			return (error);

		/* update block count */
		nblks -= min(nblks, eddumpmulti);
		blkno += min(nblks, eddumpmulti);
		va = (char *)va + min(nblks, eddumpmulti) * lp->d_secsize;
	}

	eddoingadump = 0;
	return (0);
}

static int
ed_get_params(struct ed_softc *ed, int *drv_flags)
{
	u_int16_t cmd_args[2];

	/*
	 * Get Device Configuration (09).
	 */
	cmd_args[0] = 14;	/* Options: 00s110, s: 0=Physical 1=Pseudo */
	cmd_args[1] = 0;
	if (edc_run_cmd(ed->edc_softc, CMD_GET_DEV_CONF, ed->sc_devno,
	    cmd_args, 2, 1))
		return (1);

	ed->spares = ed->sense_data[1] >> 8;
	if (drv_flags)
		*drv_flags = ed->sense_data[1] & 0x1f;
	ed->rba = ed->sense_data[2] | (ed->sense_data[3] << 16);
	/* Instead of using:
		ed->cyl = ed->sense_data[4];
		ed->heads = ed->sense_data[5] & 0xff;
		ed->sectors = ed->sense_data[5] >> 8;
	 * we fabricate the numbers from RBA count, so that
	 * number of sectors is 32 and heads 64. This seems
	 * to be necessary for integrated ESDI controller.
	 */
	ed->sectors = 32;
	ed->heads = 64;
	ed->cyl = ed->rba / (ed->heads * ed->sectors);
	ed->sc_capacity = ed->rba;

	return (0);
}
