/*	$NetBSD: cd.c,v 1.330 2015/04/26 15:15:20 mlelstv Exp $	*/

/*-
 * Copyright (c) 1998, 2001, 2003, 2004, 2005, 2008 The NetBSD Foundation,
 * Inc.  All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
 *
 * MMC framework implemented and contributed to the NetBSD Foundation by
 * Reinoud Zandijk.
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
 * Originally written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems for use under the MACH(2.5) operating system.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 *
 * Ported to run under 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cd.c,v 1.330 2015/04/26 15:15:20 mlelstv Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/cdio.h>
#include <sys/dvdio.h>
#include <sys/scsiio.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/vnode.h>
#include <sys/rndsource.h>

#include <dev/scsipi/scsi_spc.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsipi_cd.h>
#include <dev/scsipi/scsipi_disk.h>	/* rw_big and start_stop come */
#include <dev/scsipi/scsi_all.h>
					/* from there */
#include <dev/scsipi/scsi_disk.h>	/* rw comes from there */
#include <dev/scsipi/scsipiconf.h>
#include <dev/scsipi/scsipi_base.h>
#include <dev/scsipi/cdvar.h>

#include <prop/proplib.h>

#define	CDUNIT(z)			DISKUNIT(z)
#define	CDPART(z)			DISKPART(z)
#define	CDMINOR(unit, part)		DISKMINOR(unit, part)
#define	MAKECDDEV(maj, unit, part)	MAKEDISKDEV(maj, unit, part)

#define MAXTRACK	99
#define CD_BLOCK_OFFSET	150
#define CD_FRAMES	75
#define CD_SECS		60

#define CD_TOC_FORM	0	/* formatted TOC, exposed to userland     */
#define CD_TOC_MSINFO	1	/* multi-session info			  */
#define CD_TOC_RAW	2	/* raw TOC as on disc, unprocessed	  */
#define CD_TOC_PMA	3	/* PMA, used as intermediate (rare use)   */
#define CD_TOC_ATIP	4	/* pressed space of recordable		  */
#define CD_TOC_CDTEXT	5	/* special CD-TEXT, rarely used		  */

#define P5LEN	0x32
#define MS5LEN	(P5LEN + 8 + 2)

struct cd_formatted_toc {
	struct ioc_toc_header header;
	struct cd_toc_entry entries[MAXTRACK+1]; /* One extra for the */
						 /* leadout */
};

struct cdbounce {
	struct buf     *obp;			/* original buf */
	int		doff;			/* byte offset in orig. buf */
	int		soff;			/* byte offset in bounce buf */
	int		resid;			/* residual i/o in orig. buf */
	int		bcount;			/* actual obp bytes in bounce */
};

static void	cdstart(struct scsipi_periph *);
static void	cdrestart(void *);
static void	cdminphys(struct buf *);
static void	cdgetdefaultlabel(struct cd_softc *, struct cd_formatted_toc *,
		    struct disklabel *);
static void	cdgetdisklabel(struct cd_softc *);
static void	cddone(struct scsipi_xfer *, int);
static void	cdbounce(struct buf *);
static int	cd_interpret_sense(struct scsipi_xfer *);
static u_long	cd_size(struct cd_softc *, int);
static int	cd_play(struct cd_softc *, int, int);
static int	cd_play_tracks(struct cd_softc *, struct cd_formatted_toc *,
		    int, int, int, int);
static int	cd_play_msf(struct cd_softc *, int, int, int, int, int, int);
static int	cd_pause(struct cd_softc *, int);
static int	cd_reset(struct cd_softc *);
static int	cd_read_subchannel(struct cd_softc *, int, int, int,
		    struct cd_sub_channel_info *, int, int);
static int	cd_read_toc(struct cd_softc *, int, int, int,
		    struct cd_formatted_toc *, int, int, int);
static int	cd_get_parms(struct cd_softc *, int);
static int	cd_load_toc(struct cd_softc *, int, struct cd_formatted_toc *, int);
static int	cdreadmsaddr(struct cd_softc *, struct cd_formatted_toc *,int *);
static int	cdcachesync(struct scsipi_periph *periph, int flags);

static int	dvd_auth(struct cd_softc *, dvd_authinfo *);
static int	dvd_read_physical(struct cd_softc *, dvd_struct *);
static int	dvd_read_copyright(struct cd_softc *, dvd_struct *);
static int	dvd_read_disckey(struct cd_softc *, dvd_struct *);
static int	dvd_read_bca(struct cd_softc *, dvd_struct *);
static int	dvd_read_manufact(struct cd_softc *, dvd_struct *);
static int	dvd_read_struct(struct cd_softc *, dvd_struct *);

static int	cd_mode_sense(struct cd_softc *, u_int8_t, void *, size_t, int,
		    int, int *);
static int	cd_mode_select(struct cd_softc *, u_int8_t, void *, size_t,
		    int, int);
static int	cd_setchan(struct cd_softc *, int, int, int, int, int);
static int	cd_getvol(struct cd_softc *, struct ioc_vol *, int);
static int	cd_setvol(struct cd_softc *, const struct ioc_vol *, int);
static int	cd_set_pa_immed(struct cd_softc *, int);
static int	cd_load_unload(struct cd_softc *, struct ioc_load_unload *);
static int	cd_setblksize(struct cd_softc *);

static int	cdmatch(device_t, cfdata_t, void *);
static void	cdattach(device_t, device_t, void *);
static int	cddetach(device_t, int);

static int	mmc_getdiscinfo(struct scsipi_periph *, struct mmc_discinfo *);
static int	mmc_gettrackinfo(struct scsipi_periph *, struct mmc_trackinfo *);
static int	mmc_do_op(struct scsipi_periph *, struct mmc_op *);
static int	mmc_setup_writeparams(struct scsipi_periph *, struct mmc_writeparams *);

static void	cd_set_geometry(struct cd_softc *);

CFATTACH_DECL3_NEW(cd, sizeof(struct cd_softc), cdmatch, cdattach, cddetach,
    NULL, NULL, NULL, DVF_DETACH_SHUTDOWN);

extern struct cfdriver cd_cd;

static const struct scsipi_inquiry_pattern cd_patterns[] = {
	{T_CDROM, T_REMOV,
	 "",         "",                 ""},
	{T_WORM, T_REMOV,
	 "",         "",                 ""},
#if 0
	{T_CDROM, T_REMOV, /* more luns */
	 "PIONEER ", "CD-ROM DRM-600  ", ""},
#endif
	{T_DIRECT, T_REMOV,
	 "NEC                 CD-ROM DRIVE:260", "", ""},
};

static dev_type_open(cdopen);
static dev_type_close(cdclose);
static dev_type_read(cdread);
static dev_type_write(cdwrite);
static dev_type_ioctl(cdioctl);
static dev_type_strategy(cdstrategy);
static dev_type_dump(cddump);
static dev_type_size(cdsize);

const struct bdevsw cd_bdevsw = {
	.d_open = cdopen,
	.d_close = cdclose,
	.d_strategy = cdstrategy,
	.d_ioctl = cdioctl,
	.d_dump = cddump,
	.d_psize = cdsize,
	.d_discard = nodiscard,
	.d_flag = D_DISK
};

const struct cdevsw cd_cdevsw = {
	.d_open = cdopen,
	.d_close = cdclose,
	.d_read = cdread,
	.d_write = cdwrite,
	.d_ioctl = cdioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_DISK
};

static struct dkdriver cddkdriver = {
	.d_strategy = cdstrategy,
	.d_minphys = cdminphys
};

static const struct scsipi_periphsw cd_switch = {
	cd_interpret_sense,	/* use our error handler first */
	cdstart,		/* we have a queue, which is started by this */
	NULL,			/* we do not have an async handler */
	cddone,			/* deal with stats at interrupt time */
};

/*
 * The routine called by the low level scsi routine when it discovers
 * A device suitable for this driver
 */
static int
cdmatch(device_t parent, cfdata_t match, void *aux)
{
	struct scsipibus_attach_args *sa = aux;
	int priority;

	(void)scsipi_inqmatch(&sa->sa_inqbuf,
	    cd_patterns, sizeof(cd_patterns) / sizeof(cd_patterns[0]),
	    sizeof(cd_patterns[0]), &priority);

	return (priority);
}

static void
cdattach(device_t parent, device_t self, void *aux)
{
	struct cd_softc *cd = device_private(self);
	struct scsipibus_attach_args *sa = aux;
	struct scsipi_periph *periph = sa->sa_periph;

	SC_DEBUG(periph, SCSIPI_DB2, ("cdattach: "));

	cd->sc_dev = self;

	mutex_init(&cd->sc_lock, MUTEX_DEFAULT, IPL_NONE);

	if (SCSIPI_BUSTYPE_TYPE(scsipi_periph_bustype(sa->sa_periph)) ==
	    SCSIPI_BUSTYPE_SCSI && periph->periph_version == 0)
		cd->flags |= CDF_ANCIENT;

	bufq_alloc(&cd->buf_queue, "disksort", BUFQ_SORT_RAWBLOCK);

	callout_init(&cd->sc_callout, 0);

	/*
	 * Store information needed to contact our base driver
	 */
	cd->sc_periph = periph;

	periph->periph_dev = cd->sc_dev;
	periph->periph_switch = &cd_switch;

	/*
	 * Increase our openings to the maximum-per-periph
	 * supported by the adapter.  This will either be
	 * clamped down or grown by the adapter if necessary.
	 */
	periph->periph_openings =
	    SCSIPI_CHAN_MAX_PERIPH(periph->periph_channel);
	periph->periph_flags |= PERIPH_GROW_OPENINGS;

	/*
	 * Initialize and attach the disk structure.
	 */
	disk_init(&cd->sc_dk, device_xname(cd->sc_dev), &cddkdriver);
	disk_attach(&cd->sc_dk);

	aprint_normal("\n");
	aprint_naive("\n");

	rnd_attach_source(&cd->rnd_source, device_xname(cd->sc_dev),
			  RND_TYPE_DISK, RND_FLAG_DEFAULT);

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

static int
cddetach(device_t self, int flags)
{
	struct cd_softc *cd = device_private(self);
	int s, bmaj, cmaj, i, mn;

	if (cd->sc_dk.dk_openmask != 0 && (flags & DETACH_FORCE) == 0)
		return EBUSY;

	/* locate the major number */
	bmaj = bdevsw_lookup_major(&cd_bdevsw);
	cmaj = cdevsw_lookup_major(&cd_cdevsw);
	/* Nuke the vnodes for any open instances */
	for (i = 0; i < MAXPARTITIONS; i++) {
		mn = CDMINOR(device_unit(self), i);
		vdevgone(bmaj, mn, mn, VBLK);
		vdevgone(cmaj, mn, mn, VCHR);
	}

	/* kill any pending restart */
	callout_stop(&cd->sc_callout);

	s = splbio();

	/* Kill off any queued buffers. */
	bufq_drain(cd->buf_queue);

	bufq_free(cd->buf_queue);

	/* Kill off any pending commands. */
	scsipi_kill_pending(cd->sc_periph);

	splx(s);

	mutex_destroy(&cd->sc_lock);

	/* Detach from the disk list. */
	disk_detach(&cd->sc_dk);
	disk_destroy(&cd->sc_dk);

	/* Unhook the entropy source. */
	rnd_detach_source(&cd->rnd_source);

	return (0);
}

/*
 * open the device. Make sure the partition info is a up-to-date as can be.
 */
static int
cdopen(dev_t dev, int flag, int fmt, struct lwp *l)
{
	struct cd_softc *cd;
	struct scsipi_periph *periph;
	struct scsipi_adapter *adapt;
	int part;
	int error;
	int rawpart;

	cd = device_lookup_private(&cd_cd, CDUNIT(dev));
	if (cd == NULL)
		return (ENXIO);

	periph = cd->sc_periph;
	adapt = periph->periph_channel->chan_adapter;
	part = CDPART(dev);

	SC_DEBUG(periph, SCSIPI_DB1,
	    ("cdopen: dev=0x%"PRIu64" (unit %"PRIu32" (of %d), partition %"PRId32")\n",dev,
	    CDUNIT(dev), cd_cd.cd_ndevs, CDPART(dev)));

	/*
	 * If this is the first open of this device, add a reference
	 * to the adapter.
	 */
	if (cd->sc_dk.dk_openmask == 0 &&
	    (error = scsipi_adapter_addref(adapt)) != 0)
		return (error);

	mutex_enter(&cd->sc_lock);

	rawpart = (part == RAW_PART && fmt == S_IFCHR);
	if ((periph->periph_flags & PERIPH_OPEN) != 0) {
		/*
		 * If any partition is open, but the disk has been invalidated,
		 * disallow further opens.
		 */
		if ((periph->periph_flags & PERIPH_MEDIA_LOADED) == 0 &&
			!rawpart) {
			error = EIO;
			goto bad3;
		}
	} else {
		/* Check that it is still responding and ok. */
		error = scsipi_test_unit_ready(periph,
		    XS_CTL_IGNORE_ILLEGAL_REQUEST | XS_CTL_IGNORE_MEDIA_CHANGE |
		    XS_CTL_SILENT);

		/*
		 * Start the pack spinning if necessary. Always allow the
		 * raw parition to be opened, for raw IOCTLs. Data transfers
		 * will check for SDEV_MEDIA_LOADED.
		 */
		if (error == EIO) {
			int error2;
			int silent;

			if (rawpart)
				silent = XS_CTL_SILENT;
			else
				silent = 0;

			error2 = scsipi_start(periph, SSS_START, silent);
			switch (error2) {
			case 0:
				error = 0;
				break;
			case EIO:
			case EINVAL:
				break;
			default:
				error = error2;
				break;
			}
		}
		if (error) {
			if (rawpart)
				goto out;
			goto bad3;
		}

		periph->periph_flags |= PERIPH_OPEN;

		/* Lock the pack in. */
		error = scsipi_prevent(periph, SPAMR_PREVENT_DT,
		    XS_CTL_IGNORE_ILLEGAL_REQUEST | XS_CTL_IGNORE_MEDIA_CHANGE);
		SC_DEBUG(periph, SCSIPI_DB1,
		    ("cdopen: scsipi_prevent, error=%d\n", error));
		if (error) {
			if (rawpart)
				goto out;
			goto bad;
		}

		if ((periph->periph_flags & PERIPH_MEDIA_LOADED) == 0) {
			/* Load the physical device parameters. */
			if (cd_get_parms(cd, 0) != 0) {
				if (rawpart)
					goto out;
				error = ENXIO;
				goto bad;
			}
			periph->periph_flags |= PERIPH_MEDIA_LOADED;
			SC_DEBUG(periph, SCSIPI_DB3, ("Params loaded "));

			/* Fabricate a disk label. */
			cdgetdisklabel(cd);
			SC_DEBUG(periph, SCSIPI_DB3, ("Disklabel fabricated "));

			cd_set_geometry(cd);
		}
	}

	/* Check that the partition exists. */
	if (part != RAW_PART &&
	    (part >= cd->sc_dk.dk_label->d_npartitions ||
	    cd->sc_dk.dk_label->d_partitions[part].p_fstype == FS_UNUSED)) {
		error = ENXIO;
		goto bad;
	}

out:	/* Insure only one open at a time. */
	switch (fmt) {
	case S_IFCHR:
		cd->sc_dk.dk_copenmask |= (1 << part);
		break;
	case S_IFBLK:
		cd->sc_dk.dk_bopenmask |= (1 << part);
		break;
	}
	cd->sc_dk.dk_openmask =
	    cd->sc_dk.dk_copenmask | cd->sc_dk.dk_bopenmask;

	SC_DEBUG(periph, SCSIPI_DB3, ("open complete\n"));
	mutex_exit(&cd->sc_lock);
	return (0);

bad:
	if (cd->sc_dk.dk_openmask == 0) {
		scsipi_prevent(periph, SPAMR_ALLOW,
		    XS_CTL_IGNORE_ILLEGAL_REQUEST | XS_CTL_IGNORE_MEDIA_CHANGE);
		periph->periph_flags &= ~PERIPH_OPEN;
	}

bad3:
	mutex_exit(&cd->sc_lock);
	if (cd->sc_dk.dk_openmask == 0)
		scsipi_adapter_delref(adapt);
	return (error);
}

/*
 * close the device.. only called if we are the LAST
 * occurence of an open device
 */
static int
cdclose(dev_t dev, int flag, int fmt, struct lwp *l)
{
	struct cd_softc *cd = device_lookup_private(&cd_cd, CDUNIT(dev));
	struct scsipi_periph *periph = cd->sc_periph;
	struct scsipi_adapter *adapt = periph->periph_channel->chan_adapter;
	int part = CDPART(dev);
	int silent = 0;

	if (part == RAW_PART && ((cd->sc_dk.dk_label->d_npartitions == 0) ||
	    (part < cd->sc_dk.dk_label->d_npartitions &&
	    cd->sc_dk.dk_label->d_partitions[part].p_fstype == FS_UNUSED)))
		silent = XS_CTL_SILENT;

	mutex_enter(&cd->sc_lock);

	switch (fmt) {
	case S_IFCHR:
		cd->sc_dk.dk_copenmask &= ~(1 << part);
		break;
	case S_IFBLK:
		cd->sc_dk.dk_bopenmask &= ~(1 << part);
		break;
	}
	cd->sc_dk.dk_openmask =
	    cd->sc_dk.dk_copenmask | cd->sc_dk.dk_bopenmask;

	if (cd->sc_dk.dk_openmask == 0) {
		/* synchronise caches on last close */
		cdcachesync(periph, silent);

		/* drain outstanding calls */
		scsipi_wait_drain(periph);

		scsipi_prevent(periph, SPAMR_ALLOW,
		    XS_CTL_IGNORE_ILLEGAL_REQUEST | XS_CTL_IGNORE_MEDIA_CHANGE |
		    XS_CTL_IGNORE_NOT_READY | silent);
		periph->periph_flags &= ~PERIPH_OPEN;

		scsipi_wait_drain(periph);

		scsipi_adapter_delref(adapt);
	}

	mutex_exit(&cd->sc_lock);
	return (0);
}

/*
 * Actually translate the requested transfer into one the physical driver can
 * understand.  The transfer is described by a buf and will include only one
 * physical transfer.
 */
static void
cdstrategy(struct buf *bp)
{
	struct cd_softc *cd = device_lookup_private(&cd_cd,CDUNIT(bp->b_dev));
	struct disklabel *lp;
	struct scsipi_periph *periph = cd->sc_periph;
	daddr_t blkno;
	int s;

	SC_DEBUG(cd->sc_periph, SCSIPI_DB2, ("cdstrategy "));
	SC_DEBUG(cd->sc_periph, SCSIPI_DB1,
	    ("%d bytes @ blk %" PRId64 "\n", bp->b_bcount, bp->b_blkno));
	/*
	 * If the device has been made invalid, error out
	 * maybe the media changed
	 */
	if ((periph->periph_flags & PERIPH_MEDIA_LOADED) == 0) {
		if (periph->periph_flags & PERIPH_OPEN)
			bp->b_error = EIO;
		else
			bp->b_error = ENODEV;
		goto done;
	}

	lp = cd->sc_dk.dk_label;

	/*
	 * The transfer must be a whole number of blocks, offset must not
	 * be negative.
	 */
	if ((bp->b_bcount % lp->d_secsize) != 0 ||
	    bp->b_blkno < 0 ) {
		bp->b_error = EINVAL;
		goto done;
	}
	/*
	 * If it's a null transfer, return immediately
	 */
	if (bp->b_bcount == 0)
		goto done;

	/*
	 * Do bounds checking, adjust transfer. if error, process.
	 * If end of partition, just return.
	 */
	if (CDPART(bp->b_dev) == RAW_PART) {
		if (bounds_check_with_mediasize(bp, DEV_BSIZE,
		    cd->params.disksize512) <= 0)
			goto done;
	} else {
		if (bounds_check_with_label(&cd->sc_dk, bp,
		    (cd->flags & (CDF_WLABEL|CDF_LABELLING)) != 0) <= 0)
			goto done;
	}

	/*
	 * Now convert the block number to absolute and put it in
	 * terms of the device's logical block size.
	 */
	blkno = bp->b_blkno / (lp->d_secsize / DEV_BSIZE);
	if (CDPART(bp->b_dev) != RAW_PART)
		blkno += lp->d_partitions[CDPART(bp->b_dev)].p_offset;

	bp->b_rawblkno = blkno;

	/*
	 * If the disklabel sector size does not match the device
	 * sector size we may need to do some extra work.
	 */
	if (lp->d_secsize != cd->params.blksize) {

		/*
		 * If the xfer is not a multiple of the device block size
		 * or it is not block aligned, we need to bounce it.
		 */
		if ((bp->b_bcount % cd->params.blksize) != 0 ||
			((blkno * lp->d_secsize) % cd->params.blksize) != 0) {
			struct cdbounce *bounce;
			struct buf *nbp;
			long count;

			if ((bp->b_flags & B_READ) == 0) {

				/* XXXX We don't support bouncing writes. */
				bp->b_error = EACCES;
				goto done;
			}

			bounce = malloc(sizeof(*bounce), M_DEVBUF, M_NOWAIT);
			if (!bounce) {
				/* No memory -- fail the iop. */
				bp->b_error = ENOMEM;
				goto done;
			}

			bounce->obp = bp;
			bounce->resid = bp->b_bcount;
			bounce->doff = 0;
			count = ((blkno * lp->d_secsize) % cd->params.blksize);
			bounce->soff = count;
			count += bp->b_bcount;
			count = roundup(count, cd->params.blksize);
			bounce->bcount = bounce->resid;
			if (count > MAXPHYS) {
				bounce->bcount = MAXPHYS - bounce->soff;
				count = MAXPHYS;
			}

			blkno = ((blkno * lp->d_secsize) / cd->params.blksize);
			nbp = getiobuf(NULL, false);
			if (!nbp) {
				/* No memory -- fail the iop. */
				free(bounce, M_DEVBUF);
				bp->b_error = ENOMEM;
				goto done;
			}
			nbp->b_data = malloc(count, M_DEVBUF, M_NOWAIT);
			if (!nbp->b_data) {
				/* No memory -- fail the iop. */
				free(bounce, M_DEVBUF);
				putiobuf(nbp);
				bp->b_error = ENOMEM;
				goto done;
			}

			/* Set up the IOP to the bounce buffer. */
			nbp->b_error = 0;
			nbp->b_proc = bp->b_proc;
			nbp->b_bcount = count;
			nbp->b_bufsize = count;
			nbp->b_rawblkno = blkno;
			nbp->b_flags = bp->b_flags | B_READ;
			nbp->b_oflags = bp->b_oflags;
			nbp->b_cflags = bp->b_cflags;
			nbp->b_iodone = cdbounce;

			/* store bounce state in b_private and use new buf */
			nbp->b_private = bounce;

			BIO_COPYPRIO(nbp, bp);

			bp = nbp;

		} else {
			/* Xfer is aligned -- just adjust the start block */
			bp->b_rawblkno = (blkno * lp->d_secsize) /
				cd->params.blksize;
		}
	}
	s = splbio();

	/*
	 * Place it in the queue of disk activities for this disk.
	 *
	 * XXX Only do disksort() if the current operating mode does not
	 * XXX include tagged queueing.
	 */
	bufq_put(cd->buf_queue, bp);

	/*
	 * Tell the device to get going on the transfer if it's
	 * not doing anything, otherwise just wait for completion
	 */
	cdstart(cd->sc_periph);

	splx(s);
	return;

done:
	/*
	 * Correctly set the buf to indicate a completed xfer
	 */
	bp->b_resid = bp->b_bcount;
	biodone(bp);
}

/*
 * cdstart looks to see if there is a buf waiting for the device
 * and that the device is not already busy. If both are true,
 * It deques the buf and creates a scsi command to perform the
 * transfer in the buf. The transfer request will call scsipi_done
 * on completion, which will in turn call this routine again
 * so that the next queued transfer is performed.
 * The bufs are queued by the strategy routine (cdstrategy)
 *
 * This routine is also called after other non-queued requests
 * have been made of the scsi driver, to ensure that the queue
 * continues to be drained.
 *
 * must be called at the correct (highish) spl level
 * cdstart() is called at splbio from cdstrategy, cdrestart and scsipi_done
 */
static void
cdstart(struct scsipi_periph *periph)
{
	struct cd_softc *cd = device_private(periph->periph_dev);
	struct buf *bp = 0;
	struct scsipi_rw_10 cmd_big;
	struct scsi_rw_6 cmd_small;
	struct scsipi_generic *cmdp;
	struct scsipi_xfer *xs;
	int flags, nblks, cmdlen, error __diagused;

	SC_DEBUG(periph, SCSIPI_DB2, ("cdstart "));
	/*
	 * Check if the device has room for another command
	 */
	while (periph->periph_active < periph->periph_openings) {
		/*
		 * there is excess capacity, but a special waits
		 * It'll need the adapter as soon as we clear out of the
		 * way and let it run (user level wait).
		 */
		if (periph->periph_flags & PERIPH_WAITING) {
			periph->periph_flags &= ~PERIPH_WAITING;
			wakeup((void *)periph);
			return;
		}

		/*
		 * If the device has become invalid, abort all the
		 * reads and writes until all files have been closed and
		 * re-opened
		 */
		if (__predict_false(
		    (periph->periph_flags & PERIPH_MEDIA_LOADED) == 0)) {
			if ((bp = bufq_get(cd->buf_queue)) != NULL) {
				bp->b_error = EIO;
				bp->b_resid = bp->b_bcount;
				biodone(bp);
				continue;
			} else {
				return;
			}
		}

		/*
		 * See if there is a buf with work for us to do..
		 */
		if ((bp = bufq_peek(cd->buf_queue)) == NULL)
			return;

		/*
		 * We have a buf, now we should make a command.
		 */

		nblks = howmany(bp->b_bcount, cd->params.blksize);

		/*
		 *  Fill out the scsi command.  If the transfer will
		 *  fit in a "small" cdb, use it.
		 */
		if (((bp->b_rawblkno & 0x1fffff) == bp->b_rawblkno) &&
		    ((nblks & 0xff) == nblks) &&
		    !(periph->periph_quirks & PQUIRK_ONLYBIG)) {
			/*
			 * We can fit in a small cdb.
			 */
			memset(&cmd_small, 0, sizeof(cmd_small));
			cmd_small.opcode = (bp->b_flags & B_READ) ?
			    SCSI_READ_6_COMMAND : SCSI_WRITE_6_COMMAND;
			_lto3b(bp->b_rawblkno, cmd_small.addr);
			cmd_small.length = nblks & 0xff;
			cmdlen = sizeof(cmd_small);
			cmdp = (struct scsipi_generic *)&cmd_small;
		} else {
			/*
			 * Need a large cdb.
			 */
			memset(&cmd_big, 0, sizeof(cmd_big));
			cmd_big.opcode = (bp->b_flags & B_READ) ?
			    READ_10 : WRITE_10;
			_lto4b(bp->b_rawblkno, cmd_big.addr);
			_lto2b(nblks, cmd_big.length);
			cmdlen = sizeof(cmd_big);
			cmdp = (struct scsipi_generic *)&cmd_big;
		}

		/* Instrumentation. */
		disk_busy(&cd->sc_dk);

		/*
		 * Figure out what flags to use.
		 */
		flags = XS_CTL_NOSLEEP|XS_CTL_ASYNC|XS_CTL_SIMPLE_TAG;
		if (bp->b_flags & B_READ)
			flags |= XS_CTL_DATA_IN;
		else
			flags |= XS_CTL_DATA_OUT;

		/*
		 * Call the routine that chats with the adapter.
		 * Note: we cannot sleep as we may be an interrupt
		 */
		xs = scsipi_make_xs(periph, cmdp, cmdlen,
		    (u_char *)bp->b_data, bp->b_bcount,
		    CDRETRIES, 30000, bp, flags);
		if (__predict_false(xs == NULL)) {
			/*
			 * out of memory. Keep this buffer in the queue, and
			 * retry later.
			 */
			callout_reset(&cd->sc_callout, hz / 2, cdrestart,
			    periph);
			return;
		}
		/*
		 * need to dequeue the buffer before queuing the command,
		 * because cdstart may be called recursively from the
		 * HBA driver
		 */
#ifdef DIAGNOSTIC
		if (bufq_get(cd->buf_queue) != bp)
			panic("cdstart(): dequeued wrong buf");
#else
		bufq_get(cd->buf_queue);
#endif
		error = scsipi_execute_xs(xs);
		/* with a scsipi_xfer preallocated, scsipi_command can't fail */
		KASSERT(error == 0);
	}
}

static void
cdrestart(void *v)
{
	int s = splbio();
	cdstart((struct scsipi_periph *)v);
	splx(s);
}

static void
cddone(struct scsipi_xfer *xs, int error)
{
	struct cd_softc *cd = device_private(xs->xs_periph->periph_dev);
	struct buf *bp = xs->bp;

	if (bp) {
		/* note, bp->b_resid is NOT initialised */
		bp->b_error = error;
		bp->b_resid = xs->resid;
		if (error) {
			/* on a read/write error bp->b_resid is zero, so fix */
			bp->b_resid = bp->b_bcount;
		}

		disk_unbusy(&cd->sc_dk, bp->b_bcount - bp->b_resid,
		    (bp->b_flags & B_READ));
		rnd_add_uint32(&cd->rnd_source, bp->b_rawblkno);

		biodone(bp);
	}
}

static void
cdbounce(struct buf *bp)
{
	struct cdbounce *bounce = (struct cdbounce *)bp->b_private;
	struct buf *obp = bounce->obp;
	struct cd_softc *cd =
	    device_lookup_private(&cd_cd, CDUNIT(obp->b_dev));
	struct disklabel *lp = cd->sc_dk.dk_label;

	if (bp->b_error != 0) {
		/* EEK propagate the error and free the memory */
		goto done;
	}

	KASSERT(obp->b_flags & B_READ);

	/* copy bounce buffer to final destination */
	memcpy((char *)obp->b_data + bounce->doff,
	    (char *)bp->b_data + bounce->soff, bounce->bcount);

	/* check if we need more I/O, i.e. bounce put us over MAXPHYS */
	KASSERT(bounce->resid >= bounce->bcount);
	bounce->resid -= bounce->bcount;
	if (bounce->resid > 0) {
		struct buf *nbp;
		daddr_t blkno;
		long count;
		int s;

		blkno = obp->b_rawblkno +
		    ((obp->b_bcount - bounce->resid) / lp->d_secsize);
		count = ((blkno * lp->d_secsize) % cd->params.blksize);
		blkno = (blkno * lp->d_secsize) / cd->params.blksize;
		bounce->soff = count;
		bounce->doff += bounce->bcount;
		count += bounce->resid;
		count = roundup(count, cd->params.blksize);
		bounce->bcount = bounce->resid;
		if (count > MAXPHYS) {
			bounce->bcount = MAXPHYS - bounce->soff;
			count = MAXPHYS;
		}

		nbp = getiobuf(NULL, false);
		if (!nbp) {
			/* No memory -- fail the iop. */
			bp->b_error = ENOMEM;
			goto done;
		}

		/* Set up the IOP to the bounce buffer. */
		nbp->b_error = 0;
		nbp->b_proc = obp->b_proc;
		nbp->b_bcount = count;
		nbp->b_bufsize = count;
		nbp->b_data = bp->b_data;
		nbp->b_rawblkno = blkno;
		nbp->b_flags = obp->b_flags | B_READ;
		nbp->b_oflags = obp->b_oflags;
		nbp->b_cflags = obp->b_cflags;
		nbp->b_iodone = cdbounce;

		/* store bounce state in b_private and use new buf */
		nbp->b_private = bounce;

		BIO_COPYPRIO(nbp, obp);

		bp->b_data = NULL;
		putiobuf(bp);

		/* enqueue the request and return */
		s = splbio();
		bufq_put(cd->buf_queue, nbp);
		cdstart(cd->sc_periph);
		splx(s);

		return;
	}

done:
	obp->b_error = bp->b_error;
	obp->b_resid = bp->b_resid;
	free(bp->b_data, M_DEVBUF);
	free(bounce, M_DEVBUF);
	bp->b_data = NULL;
	putiobuf(bp);
	biodone(obp);
}

static int
cd_interpret_sense(struct scsipi_xfer *xs)
{
	struct scsipi_periph *periph = xs->xs_periph;
	struct scsi_sense_data *sense = &xs->sense.scsi_sense;
	int retval = EJUSTRETURN;

	/*
	 * If it isn't a extended or extended/deferred error, let
	 * the generic code handle it.
	 */
	if (SSD_RCODE(sense->response_code) != SSD_RCODE_CURRENT &&
	    SSD_RCODE(sense->response_code) != SSD_RCODE_DEFERRED)
		return (retval);

	/*
	 * If we got a "Unit not ready" (SKEY_NOT_READY) and "Logical Unit
	 * Is In The Process of Becoming Ready" (Sense code 0x04,0x01), then
	 * wait a bit for the drive to spin up
	 */

	if ((SSD_SENSE_KEY(sense->flags) == SKEY_NOT_READY) &&
	    (sense->asc == 0x04) && (sense->ascq == 0x01)) {
		/*
		 * Sleep for 5 seconds to wait for the drive to spin up
		 */

		SC_DEBUG(periph, SCSIPI_DB1, ("Waiting 5 sec for CD "
						"spinup\n"));
		if (!callout_pending(&periph->periph_callout))
			scsipi_periph_freeze(periph, 1);
		callout_reset(&periph->periph_callout,
		    5 * hz, scsipi_periph_timed_thaw, periph);
		retval = ERESTART;
	}

	/*
	 * If we got a "Unit not ready" (SKEY_NOT_READY) and "Logical Unit Not
	 * Ready, Operation In Progress" (Sense code 0x04, 0x07),
	 * then wait for the specified time
	 */
	 
	if ((SSD_SENSE_KEY(sense->flags) == SKEY_NOT_READY) &&
	    (sense->asc == 0x04) && (sense->ascq == 0x07)) {
		/*
		 * we could listen to the delay; but it looks like the skey
		 * data is not always returned.
		 */
		/* cd_delay = _2btol(sense->sks.sks_bytes); */

		/* wait for a half second and get going again */
		if (!callout_pending(&periph->periph_callout))
			scsipi_periph_freeze(periph, 1);
		callout_reset(&periph->periph_callout,
		    hz/2, scsipi_periph_timed_thaw, periph);
		retval = ERESTART;
	}

	/*
	 * If we got a "Unit not ready" (SKEY_NOT_READY) and "Long write in
	 * progress" (Sense code 0x04, 0x08), then wait for the specified
	 * time
	 */
	 
	if ((SSD_SENSE_KEY(sense->flags) == SKEY_NOT_READY) &&
	    (sense->asc == 0x04) && (sense->ascq == 0x08)) {
		/*
		 * long write in process; we could listen to the delay; but it
		 * looks like the skey data is not always returned.
		 */
		/* cd_delay = _2btol(sense->sks.sks_bytes); */

		/* wait for a half second and get going again */
		if (!callout_pending(&periph->periph_callout))
			scsipi_periph_freeze(periph, 1);
		callout_reset(&periph->periph_callout,
		    hz/2, scsipi_periph_timed_thaw, periph);
		retval = ERESTART;
	}

	return (retval);
}

static void
cdminphys(struct buf *bp)
{
	struct cd_softc *cd = device_lookup_private(&cd_cd, CDUNIT(bp->b_dev));
	long xmax;

	/*
	 * If the device is ancient, we want to make sure that
	 * the transfer fits into a 6-byte cdb.
	 *
	 * XXX Note that the SCSI-I spec says that 256-block transfers
	 * are allowed in a 6-byte read/write, and are specified
	 * by settng the "length" to 0.  However, we're conservative
	 * here, allowing only 255-block transfers in case an
	 * ancient device gets confused by length == 0.  A length of 0
	 * in a 10-byte read/write actually means 0 blocks.
	 */
	if (cd->flags & CDF_ANCIENT) {
		xmax = cd->sc_dk.dk_label->d_secsize * 0xff;

		if (bp->b_bcount > xmax)
			bp->b_bcount = xmax;
	}

	(*cd->sc_periph->periph_channel->chan_adapter->adapt_minphys)(bp);
}

static int
cdread(dev_t dev, struct uio *uio, int ioflag)
{
	return (physio(cdstrategy, NULL, dev, B_READ, cdminphys, uio));
}

static int
cdwrite(dev_t dev, struct uio *uio, int ioflag)
{
	return (physio(cdstrategy, NULL, dev, B_WRITE, cdminphys, uio));
}

#if 0	/* XXX Not used */
/*
 * conversion between minute-seconde-frame and logical block address
 * addresses format
 */
static void
lba2msf(u_long lba, u_char *m, u_char *s, u_char *f)
{
	u_long tmp;

	tmp = lba + CD_BLOCK_OFFSET;	/* offset of first logical frame */
	tmp &= 0xffffff;		/* negative lbas use only 24 bits */
	*m = tmp / (CD_SECS * CD_FRAMES);
	tmp %= (CD_SECS * CD_FRAMES);
	*s = tmp / CD_FRAMES;
	*f = tmp % CD_FRAMES;
}
#endif /* XXX Not used */

/*
 * Convert an hour:minute:second:frame address to a logical block adres. In
 * theory the number of secs/minute and number of frames/second could be
 * configured differently in the device  as could the block offset but in
 * practice these values are rock solid and most drives don't even allow
 * theses values to be changed.
 */
static uint32_t
hmsf2lba(uint8_t h, uint8_t m, uint8_t s, uint8_t f)
{
	return (((((uint32_t) h * 60 + m) * CD_SECS) + s) * CD_FRAMES + f)
		- CD_BLOCK_OFFSET;
}

static int
cdreadmsaddr(struct cd_softc *cd, struct cd_formatted_toc *toc, int *addr)
{
	struct scsipi_periph *periph = cd->sc_periph;
	int error;
	struct cd_toc_entry *cte;

	error = cd_read_toc(cd, CD_TOC_FORM, 0, 0, toc,
	    sizeof(struct ioc_toc_header) + sizeof(struct cd_toc_entry),
	    0, 0x40 /* control word for "get MS info" */);

	if (error)
		return (error);

	cte = &toc->entries[0];
	if (periph->periph_quirks & PQUIRK_LITTLETOC) {
		cte->addr.lba = le32toh(cte->addr.lba);
		toc->header.len = le16toh(toc->header.len);
	} else {
		cte->addr.lba = be32toh(cte->addr.lba);
		toc->header.len = be16toh(toc->header.len);
	}

	*addr = (toc->header.len >= 10 && cte->track > 1) ?
		cte->addr.lba : 0;
	return 0;
}

/* synchronise caches code from sd.c, move to scsipi_ioctl.c ? */
static int
cdcachesync(struct scsipi_periph *periph, int flags) {
	struct scsi_synchronize_cache_10 cmd;

	/*
	 * Issue a SYNCHRONIZE CACHE. MMC devices have to issue with address 0
	 * and length 0 as it can't synchronise parts of the disc per spec.
	 * We ignore ILLEGAL REQUEST in the event that the command is not
	 * supported by the device, and poll for completion so that we know
	 * that the cache has actually been flushed.
	 *
	 * XXX should we handle the PQUIRK_NOSYNCCACHE ?
	 */

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = SCSI_SYNCHRONIZE_CACHE_10;

	return (scsipi_command(periph, (void *)&cmd, sizeof(cmd), 0, 0,
	    CDRETRIES, 30000, NULL, flags | XS_CTL_IGNORE_ILLEGAL_REQUEST));
}

static int
do_cdioreadentries(struct cd_softc *cd, struct ioc_read_toc_entry *te,
    struct cd_formatted_toc *toc)
{
	/* READ TOC format 0 command, entries */
	struct ioc_toc_header *th;
	struct cd_toc_entry *cte;
	u_int len = te->data_len;
	int ntracks;
	int error;

	th = &toc->header;

	if (len > sizeof(toc->entries) ||
	    len < sizeof(toc->entries[0]))
		return (EINVAL);
	error = cd_read_toc(cd, CD_TOC_FORM, te->address_format,
	    te->starting_track, toc,
	    sizeof(toc->header) + len,
	    0, 0);
	if (error)
		return (error);
	if (te->address_format == CD_LBA_FORMAT)
		for (ntracks =
		    th->ending_track - th->starting_track + 1;
		    ntracks >= 0; ntracks--) {
			cte = &toc->entries[ntracks];
			cte->addr_type = CD_LBA_FORMAT;
			if (cd->sc_periph->periph_quirks & PQUIRK_LITTLETOC)
				cte->addr.lba = le32toh(cte->addr.lba);
			else
				cte->addr.lba = be32toh(cte->addr.lba);
		}
	if (cd->sc_periph->periph_quirks & PQUIRK_LITTLETOC)
		th->len = le16toh(th->len);
	else
		th->len = be16toh(th->len);
	return 0;
}

/*
 * Perform special action on behalf of the user.
 * Knows about the internals of this device
 */
static int
cdioctl(dev_t dev, u_long cmd, void *addr, int flag, struct lwp *l)
{
	struct cd_softc *cd = device_lookup_private(&cd_cd, CDUNIT(dev));
	struct scsipi_periph *periph = cd->sc_periph;
	struct cd_formatted_toc toc;
	int part = CDPART(dev);
	int error;
	int s;
#ifdef __HAVE_OLD_DISKLABEL
	struct disklabel *newlabel = NULL;
#endif

	SC_DEBUG(cd->sc_periph, SCSIPI_DB2, ("cdioctl 0x%lx ", cmd));

	/*
	 * If the device is not valid, some IOCTLs can still be
	 * handled on the raw partition. Check this here.
	 */
	if ((periph->periph_flags & PERIPH_MEDIA_LOADED) == 0) {
		switch (cmd) {
		case DIOCWLABEL:
		case DIOCLOCK:
		case ODIOCEJECT:
		case DIOCEJECT:
		case DIOCCACHESYNC:
		case DIOCTUR:
		case SCIOCIDENTIFY:
		case OSCIOCIDENTIFY:
		case SCIOCCOMMAND:
		case SCIOCDEBUG:
		case CDIOCGETVOL:
		case CDIOCSETVOL:
		case CDIOCSETMONO:
		case CDIOCSETSTEREO:
		case CDIOCSETMUTE:
		case CDIOCSETLEFT:
		case CDIOCSETRIGHT:
		case CDIOCCLOSE:
		case CDIOCEJECT:
		case CDIOCALLOW:
		case CDIOCPREVENT:
		case CDIOCSETDEBUG:
		case CDIOCCLRDEBUG:
		case CDIOCRESET:
		case SCIOCRESET:
		case CDIOCLOADUNLOAD:
		case DVD_AUTH:
		case DVD_READ_STRUCT:
		case DIOCGSTRATEGY:
		case DIOCSSTRATEGY:
			if (part == RAW_PART)
				break;
		/* FALLTHROUGH */
		default:
			if ((periph->periph_flags & PERIPH_OPEN) == 0)
				return (ENODEV);
			else
				return (EIO);
		}
	}

	error = disk_ioctl(&cd->sc_dk, dev, cmd, addr, flag, l); 
	if (error != EPASSTHROUGH)
		return (error);

	error = 0;
	switch (cmd) {
	case DIOCWDINFO:
	case DIOCSDINFO:
#ifdef __HAVE_OLD_DISKLABEL
	case ODIOCWDINFO:
	case ODIOCSDINFO:
#endif
	{
		struct disklabel *lp;

		if ((flag & FWRITE) == 0)
			return (EBADF);

#ifdef __HAVE_OLD_DISKLABEL
		if (cmd == ODIOCSDINFO || cmd == ODIOCWDINFO) {
			newlabel = malloc(sizeof (*newlabel), M_TEMP,
			    M_WAITOK | M_ZERO);
			if (newlabel == NULL)
				return (EIO);
			memcpy(newlabel, addr, sizeof (struct olddisklabel));
			lp = newlabel;
		} else
#endif
		lp = addr;

		mutex_enter(&cd->sc_lock);
		cd->flags |= CDF_LABELLING;

		error = setdisklabel(cd->sc_dk.dk_label,
		    lp, /*cd->sc_dk.dk_openmask : */0,
		    cd->sc_dk.dk_cpulabel);
		if (error == 0) {
			/* XXX ? */
		}

		cd->flags &= ~CDF_LABELLING;
		mutex_exit(&cd->sc_lock);
#ifdef __HAVE_OLD_DISKLABEL
		if (newlabel != NULL)
			free(newlabel, M_TEMP);
#endif
		return (error);
	}

	case DIOCWLABEL:
		return (EBADF);

	case DIOCGDEFLABEL:
		cdgetdefaultlabel(cd, &toc, addr);
		return (0);

#ifdef __HAVE_OLD_DISKLABEL
	case ODIOCGDEFLABEL:
		newlabel = malloc(sizeof (*newlabel), M_TEMP, M_WAITOK);
		if (newlabel == NULL)
			return (EIO);
		cdgetdefaultlabel(cd, &toc, newlabel);
		if (newlabel->d_npartitions > OLDMAXPARTITIONS)
			error = ENOTTY;
		else
			memcpy(addr, newlabel, sizeof (struct olddisklabel));
		free(newlabel, M_TEMP);
		return error;
#endif

	case DIOCTUR: {
		/* test unit ready */
		error = scsipi_test_unit_ready(cd->sc_periph, XS_CTL_SILENT);
		*((int*)addr) = (error == 0);
		if (error == ENODEV || error == EIO || error == 0)
			return 0;			
		return error;
	}

	case CDIOCPLAYTRACKS: {
		/* PLAY_MSF command */
		struct ioc_play_track *args = addr;

		if ((error = cd_set_pa_immed(cd, 0)) != 0)
			return (error);
		return (cd_play_tracks(cd, &toc, args->start_track,
		    args->start_index, args->end_track, args->end_index));
	}
	case CDIOCPLAYMSF: {
		/* PLAY_MSF command */
		struct ioc_play_msf *args = addr;

		if ((error = cd_set_pa_immed(cd, 0)) != 0)
			return (error);
		return (cd_play_msf(cd, args->start_m, args->start_s,
		    args->start_f, args->end_m, args->end_s, args->end_f));
	}
	case CDIOCPLAYBLOCKS: {
		/* PLAY command */
		struct ioc_play_blocks *args = addr;

		if ((error = cd_set_pa_immed(cd, 0)) != 0)
			return (error);
		return (cd_play(cd, args->blk, args->len));
	}
	case CDIOCREADSUBCHANNEL: {
		/* READ_SUBCHANNEL command */
		struct ioc_read_subchannel *args = addr;
		struct cd_sub_channel_info data;
		u_int len = args->data_len;

		if (len > sizeof(data) ||
		    len < sizeof(struct cd_sub_channel_header))
			return (EINVAL);
		error = cd_read_subchannel(cd, args->address_format,
		    args->data_format, args->track, &data, len, 0);
		if (error)
			return (error);
		len = min(len, _2btol(data.header.data_len) +
		    sizeof(struct cd_sub_channel_header));
		return (copyout(&data, args->data, len));
	}
	case CDIOCREADSUBCHANNEL_BUF: {
		/* As CDIOCREADSUBCHANNEL, but without a 2nd buffer area */
		struct ioc_read_subchannel_buf *args = addr;
		if (args->req.data_len != sizeof args->info)
			return EINVAL;
		return cd_read_subchannel(cd, args->req.address_format,
		    args->req.data_format, args->req.track, &args->info,
		    sizeof(args->info), 0);
	}
	case CDIOREADTOCHEADER: {
		/* READ TOC format 0 command, static header */
		if ((error = cd_read_toc(cd, CD_TOC_FORM, 0, 0,
		    &toc, sizeof(toc.header), 0, 0)) != 0)
			return (error);
		if (cd->sc_periph->periph_quirks & PQUIRK_LITTLETOC)
			toc.header.len = le16toh(toc.header.len);
		else
			toc.header.len = be16toh(toc.header.len);
		memcpy(addr, &toc.header, sizeof(toc.header));
		return (0);
	}
	case CDIOREADTOCENTRYS: {
		struct ioc_read_toc_entry *te = addr;
		error = do_cdioreadentries(cd, te, &toc);
		if (error != 0)
			return error;
		return copyout(toc.entries, te->data, min(te->data_len,
		    toc.header.len - (sizeof(toc.header.starting_track)
			+ sizeof(toc.header.ending_track))));
	}
	case CDIOREADTOCENTRIES_BUF: {
		struct ioc_read_toc_entry_buf *te = addr;
		error = do_cdioreadentries(cd, &te->req, &toc);
		if (error != 0)
			return error;
		memcpy(te->entry, toc.entries, min(te->req.data_len,
		    toc.header.len - (sizeof(toc.header.starting_track)
			+ sizeof(toc.header.ending_track))));
		return 0;
	}
	case CDIOREADMSADDR: {
		/* READ TOC format 0 command, length of first track only */
		int sessno = *(int*)addr;

		if (sessno != 0)
			return (EINVAL);

		return (cdreadmsaddr(cd, &toc, addr));
	}
	case CDIOCSETPATCH: {
		struct ioc_patch *arg = addr;

		return (cd_setchan(cd, arg->patch[0], arg->patch[1],
		    arg->patch[2], arg->patch[3], 0));
	}
	case CDIOCGETVOL: {
		/* MODE SENSE command (AUDIO page) */
		struct ioc_vol *arg = addr;

		return (cd_getvol(cd, arg, 0));
	}
	case CDIOCSETVOL: {
		/* MODE SENSE/MODE SELECT commands (AUDIO page) */
		struct ioc_vol *arg = addr;

		return (cd_setvol(cd, arg, 0));
	}
	case CDIOCSETMONO:
		/* MODE SENSE/MODE SELECT commands (AUDIO page) */
		return (cd_setchan(cd, BOTH_CHANNEL, BOTH_CHANNEL,
		    MUTE_CHANNEL, MUTE_CHANNEL, 0));

	case CDIOCSETSTEREO:
		/* MODE SENSE/MODE SELECT commands (AUDIO page) */
		return (cd_setchan(cd, LEFT_CHANNEL, RIGHT_CHANNEL,
		    MUTE_CHANNEL, MUTE_CHANNEL, 0));

	case CDIOCSETMUTE:
		/* MODE SENSE/MODE SELECT commands (AUDIO page) */
		return (cd_setchan(cd, MUTE_CHANNEL, MUTE_CHANNEL,
		    MUTE_CHANNEL, MUTE_CHANNEL, 0));

	case CDIOCSETLEFT:
		/* MODE SENSE/MODE SELECT commands (AUDIO page) */
		return (cd_setchan(cd, LEFT_CHANNEL, LEFT_CHANNEL,
		    MUTE_CHANNEL, MUTE_CHANNEL, 0));

	case CDIOCSETRIGHT:
		/* MODE SENSE/MODE SELECT commands (AUDIO page) */
		return (cd_setchan(cd, RIGHT_CHANNEL, RIGHT_CHANNEL,
		    MUTE_CHANNEL, MUTE_CHANNEL, 0));

	case CDIOCRESUME:
		/* PAUSE command */
		return (cd_pause(cd, PA_RESUME));
	case CDIOCPAUSE:
		/* PAUSE command */
		return (cd_pause(cd, PA_PAUSE));
	case CDIOCSTART:
		return (scsipi_start(periph, SSS_START, 0));
	case CDIOCSTOP:
		return (scsipi_start(periph, SSS_STOP, 0));
	case CDIOCCLOSE:
		return (scsipi_start(periph, SSS_START|SSS_LOEJ,
		    XS_CTL_IGNORE_NOT_READY | XS_CTL_IGNORE_MEDIA_CHANGE));
	case DIOCEJECT:
		if (*(int *)addr == 0) {
			/*
			 * Don't force eject: check that we are the only
			 * partition open. If so, unlock it.
			 */
			if ((cd->sc_dk.dk_openmask & ~(1 << part)) == 0 &&
			    cd->sc_dk.dk_bopenmask + cd->sc_dk.dk_copenmask ==
			    cd->sc_dk.dk_openmask) {
				error = scsipi_prevent(periph, SPAMR_ALLOW,
				    XS_CTL_IGNORE_NOT_READY);
				if (error)
					return (error);
			} else {
				return (EBUSY);
			}
		}
		/* FALLTHROUGH */
	case CDIOCEJECT: /* FALLTHROUGH */
	case ODIOCEJECT:
		error = scsipi_start(periph, SSS_STOP|SSS_LOEJ, 0);
		if (error == 0) {
			int i;

			/*
			 * We have just successfully ejected the medium,
			 * all partitions cached are meaningless now.
			 * Make sure cdclose() will do silent operations
			 * now by marking all partitions unused.
			 * Before any real access, a new (default-)disk-
			 * label will be generated anyway.
			 */
			for (i = 0; i < cd->sc_dk.dk_label->d_npartitions;
			    i++)
				cd->sc_dk.dk_label->d_partitions[i].p_fstype =
					FS_UNUSED;
		}
		return error;
	case DIOCCACHESYNC:
		/* SYNCHRONISE CACHES command */
		return (cdcachesync(periph, 0));
	case CDIOCALLOW:
		return (scsipi_prevent(periph, SPAMR_ALLOW, 0));
	case CDIOCPREVENT:
		return (scsipi_prevent(periph, SPAMR_PREVENT_DT, 0));
	case DIOCLOCK:
		return (scsipi_prevent(periph,
		    (*(int *)addr) ? SPAMR_PREVENT_DT : SPAMR_ALLOW, 0));
	case CDIOCSETDEBUG:
		cd->sc_periph->periph_dbflags |= (SCSIPI_DB1 | SCSIPI_DB2);
		return (0);
	case CDIOCCLRDEBUG:
		cd->sc_periph->periph_dbflags &= ~(SCSIPI_DB1 | SCSIPI_DB2);
		return (0);
	case CDIOCRESET:
	case SCIOCRESET:
		return (cd_reset(cd));
	case CDIOCLOADUNLOAD:
		/* LOAD_UNLOAD command */
		return (cd_load_unload(cd, addr));
	case DVD_AUTH:
		/* GPCMD_REPORT_KEY or GPCMD_SEND_KEY command */
		return (dvd_auth(cd, addr));
	case DVD_READ_STRUCT:
		/* GPCMD_READ_DVD_STRUCTURE command */
		return (dvd_read_struct(cd, addr));
	case MMCGETDISCINFO:
		/*
		 * GET_CONFIGURATION, READ_DISCINFO, READ_TRACKINFO,
		 * (READ_TOCf2, READ_CD_CAPACITY and GET_CONFIGURATION) commands
		 */
		return mmc_getdiscinfo(periph, (struct mmc_discinfo *) addr);
	case MMCGETTRACKINFO:
		/* READ TOCf2, READ_CD_CAPACITY and READ_TRACKINFO commands */
		return mmc_gettrackinfo(periph, (struct mmc_trackinfo *) addr);
	case MMCOP:
		/*
		 * CLOSE TRACK/SESSION, RESERVE_TRACK, REPAIR_TRACK,
		 * SYNCHRONISE_CACHE commands
		 */
		return mmc_do_op(periph, (struct mmc_op *) addr);
	case MMCSETUPWRITEPARAMS :
		/* MODE SENSE page 5, MODE_SELECT page 5 commands */
		return mmc_setup_writeparams(periph, (struct mmc_writeparams *) addr);
	case DIOCGSTRATEGY:
	    {
		struct disk_strategy *dks = addr;

		s = splbio();
		strlcpy(dks->dks_name, bufq_getstrategyname(cd->buf_queue),
		    sizeof(dks->dks_name));
		splx(s);
		dks->dks_paramlen = 0;

		return 0;
	    }
	case DIOCSSTRATEGY:
	    {
		struct disk_strategy *dks = addr;
		struct bufq_state *new;
		struct bufq_state *old;

		if ((flag & FWRITE) == 0) {
			return EBADF;
		}
		if (dks->dks_param != NULL) {
			return EINVAL;
		}
		dks->dks_name[sizeof(dks->dks_name) - 1] = 0; /* ensure term */
		error = bufq_alloc(&new, dks->dks_name,
		    BUFQ_EXACT|BUFQ_SORT_RAWBLOCK);
		if (error) {
			return error;
		}
		s = splbio();
		old = cd->buf_queue;
		bufq_move(new, old);
		cd->buf_queue = new;
		splx(s);
		bufq_free(old);

		return 0;
	    }
	default:
		if (part != RAW_PART)
			return (ENOTTY);
		return (scsipi_do_ioctl(periph, dev, cmd, addr, flag, l));
	}

#ifdef DIAGNOSTIC
	panic("cdioctl: impossible");
#endif
}

static void
cdgetdefaultlabel(struct cd_softc *cd, struct cd_formatted_toc *toc,
    struct disklabel *lp)
{
	int lastsession;

	memset(lp, 0, sizeof(struct disklabel));

	lp->d_secsize = cd->params.blksize;
	lp->d_ntracks = 1;
	lp->d_nsectors = 100;
	lp->d_ncylinders = (cd->params.disksize / 100) + 1;
	lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;

	switch (SCSIPI_BUSTYPE_TYPE(scsipi_periph_bustype(cd->sc_periph))) {
	case SCSIPI_BUSTYPE_SCSI:
		lp->d_type = DKTYPE_SCSI;
		break;
	case SCSIPI_BUSTYPE_ATAPI:
		lp->d_type = DKTYPE_ATAPI;
		break;
	}
	/*
	 * XXX
	 * We could probe the mode pages to figure out what kind of disc it is.
	 * Is this worthwhile?
	 */
	strncpy(lp->d_typename, "optical media", 16);
	strncpy(lp->d_packname, "fictitious", 16);
	lp->d_secperunit = cd->params.disksize;
	lp->d_rpm = 300;
	lp->d_interleave = 1;
	lp->d_flags = D_REMOVABLE | D_SCSI_MMC;

	if (cdreadmsaddr(cd, toc, &lastsession) != 0)
		lastsession = 0;

	lp->d_partitions[0].p_offset = 0;
	lp->d_partitions[0].p_size = lp->d_secperunit;
	lp->d_partitions[0].p_cdsession = lastsession;
	lp->d_partitions[0].p_fstype = FS_ISO9660;

	lp->d_partitions[RAW_PART].p_offset = 0;
	lp->d_partitions[RAW_PART].p_size = lp->d_secperunit;
	lp->d_partitions[RAW_PART].p_fstype = FS_UDF;

	lp->d_npartitions = RAW_PART + 1;

	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = dkcksum(lp);
}

/*
 * Load the label information on the named device
 * Actually fabricate a disklabel
 *
 * EVENTUALLY take information about different
 * data tracks from the TOC and put it in the disklabel
 */
static void
cdgetdisklabel(struct cd_softc *cd)
{
	struct disklabel *lp = cd->sc_dk.dk_label;
	struct cd_formatted_toc toc;
	const char *errstring;
	int bmajor;

	memset(cd->sc_dk.dk_cpulabel, 0, sizeof(struct cpu_disklabel));

	cdgetdefaultlabel(cd, &toc, lp);

	/*
	 * Call the generic disklabel extraction routine
	 *
	 * bmajor follows ata_raid code
	 */
	bmajor = devsw_name2blk(device_xname(cd->sc_dev), NULL, 0);
	errstring = readdisklabel(MAKECDDEV(bmajor,
	    device_unit(cd->sc_dev), RAW_PART),
	    cdstrategy, lp, cd->sc_dk.dk_cpulabel);

	/* if all went OK, we are passed a NULL error string */
	if (errstring == NULL)
		return;

	/* Reset to default label -- after printing error and the warning */
	aprint_error_dev(cd->sc_dev, "%s\n", errstring);
	memset(cd->sc_dk.dk_cpulabel, 0, sizeof(struct cpu_disklabel));
	cdgetdefaultlabel(cd, &toc, lp);
}

/*
 * Reading a disc's total capacity is apparently a very difficult issue for the
 * SCSI standardisation group. Every disc type seems to have its own
 * (re)invented size request method and modifiers. The failsafe way of
 * determining the total (max) capacity i.e. not the recorded capacity but the
 * total maximum capacity is to request the info on the last track and
 * calculate the last lba.
 *
 * For ROM drives, we go for the CD recorded capacity. For recordable devices
 * we count.
 */
static int
read_cd_capacity(struct scsipi_periph *periph, uint32_t *blksize, u_long *last_lba)
{
	struct scsipi_read_cd_capacity    cap_cmd;
	/*
	 * XXX: see PR 48550 and PR 48754:
	 * the ahcisata(4) driver can not deal with unaligned
	 * data, so align this "a bit"
	 */
	struct scsipi_read_cd_cap_data    cap __aligned(2);
	struct scsipi_read_discinfo       di_cmd;
	struct scsipi_read_discinfo_data  di;
	struct scsipi_read_trackinfo      ti_cmd;
	struct scsipi_read_trackinfo_data ti;
	uint32_t track_start, track_size;
	int error, flags, msb, lsb, last_track;

	/* if the device doesn't grok capacity, return the dummies */
	if (periph->periph_quirks & PQUIRK_NOCAPACITY)
		return 0;

	/* first try read CD capacity for blksize and last recorded lba */
	/* issue the cd capacity request */
	flags = XS_CTL_DATA_IN;
	memset(&cap_cmd, 0, sizeof(cap_cmd));
	memset(&cap, 0, sizeof(cap));
	cap_cmd.opcode = READ_CD_CAPACITY;

	error = scsipi_command(periph,
	    (void *) &cap_cmd, sizeof(cap_cmd),
	    (void *) &cap,     sizeof(cap),
	    CDRETRIES, 30000, NULL, flags);
	if (error)
		return error;

	/* retrieve values and sanity check them */
	*blksize  = _4btol(cap.length);
	*last_lba = _4btol(cap.addr);

	/* blksize is 2048 for CD, but some drives give gibberish */
	if ((*blksize < 512) || ((*blksize & 511) != 0)
	    || (*blksize > 16*1024)) {
		if (*blksize > 16*1024)
			aprint_error("read_cd_capacity: extra large block "
			    "size %u found - limiting to 2kByte\n",
			    *blksize);
		*blksize = 2048;	/* some drives lie ! */
	}

	/* recordables have READ_DISCINFO implemented */
	flags = XS_CTL_DATA_IN | XS_CTL_SILENT;
	memset(&di_cmd, 0, sizeof(di_cmd));
	di_cmd.opcode = READ_DISCINFO;
	_lto2b(READ_DISCINFO_BIGSIZE, di_cmd.data_len);

	error = scsipi_command(periph,
	    (void *) &di_cmd,  sizeof(di_cmd),
	    (void *) &di,      READ_DISCINFO_BIGSIZE,
	    CDRETRIES, 30000, NULL, flags);
	if (error == 0) {
		msb = di.last_track_last_session_msb;
		lsb = di.last_track_last_session_lsb;
		last_track = (msb << 8) | lsb;

		/* request info on last track */
		memset(&ti_cmd, 0, sizeof(ti_cmd));
		ti_cmd.opcode = READ_TRACKINFO;
		ti_cmd.addr_type = 1;			/* on tracknr */
		_lto4b(last_track, ti_cmd.address);	/* tracknr    */
		_lto2b(sizeof(ti), ti_cmd.data_len);

		error = scsipi_command(periph,
		    (void *) &ti_cmd,  sizeof(ti_cmd),
		    (void *) &ti,      sizeof(ti),
		    CDRETRIES, 30000, NULL, flags);
		if (error == 0) {
			track_start = _4btol(ti.track_start);
			track_size  = _4btol(ti.track_size);

			/* overwrite only with a sane value */
			if (track_start + track_size >= 100)
				*last_lba = (u_long) track_start + track_size -1;
		}
	}

	/* sanity check for lba_size */
	if (*last_lba < 100)
		*last_lba = 400000-1;

	return 0;
}

/*
 * Find out from the device what its capacity is
 */
static u_long
cd_size(struct cd_softc *cd, int flags)
{
	uint32_t blksize = 2048;
	u_long last_lba = 0, size;
	int error;

	error = read_cd_capacity(cd->sc_periph, &blksize, &last_lba);
	if (error)
		goto error;

	if (blksize != 2048) {
		if (cd_setblksize(cd) == 0) {
			blksize = 2048;
			error = read_cd_capacity(cd->sc_periph,
			    &blksize, &last_lba);
			if (error)
				goto error;
		}
	}

	size = last_lba + 1;
	cd->params.blksize     = blksize;
	cd->params.disksize    = size;
	cd->params.disksize512 = ((u_int64_t)cd->params.disksize * blksize) / DEV_BSIZE;

	SC_DEBUG(cd->sc_periph, SCSIPI_DB2,
	    ("cd_size: %u %lu\n", blksize, size));

	return size;

error:
	/* something went wrong */
	cd->params.blksize     = 2048;
	cd->params.disksize    = 0;
	cd->params.disksize512 = 0;

	SC_DEBUG(cd->sc_periph, SCSIPI_DB2, ("cd_size: failed\n"));

	return 0;
}

/*
 * Get scsi driver to send a "start playing" command
 */
static int
cd_play(struct cd_softc *cd, int blkno, int nblks)
{
	struct scsipi_play cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = PLAY;
	_lto4b(blkno, cmd.blk_addr);
	_lto2b(nblks, cmd.xfer_len);

	return (scsipi_command(cd->sc_periph, (void *)&cmd, sizeof(cmd), 0, 0,
	    CDRETRIES, 30000, NULL, 0));
}

/*
 * Get scsi driver to send a "start playing" command
 */
static int
cd_play_tracks(struct cd_softc *cd, struct cd_formatted_toc *toc, int strack,
    int sindex, int etrack, int eindex)
{
	int error;

	if (!etrack)
		return (EIO);
	if (strack > etrack)
		return (EINVAL);

	error = cd_load_toc(cd, CD_TOC_FORM, toc, 0);
	if (error)
		return (error);

	if (++etrack > (toc->header.ending_track+1))
		etrack = toc->header.ending_track+1;

	strack -= toc->header.starting_track;
	etrack -= toc->header.starting_track;
	if (strack < 0)
		return (EINVAL);

	return (cd_play_msf(cd, toc->entries[strack].addr.msf.minute,
	    toc->entries[strack].addr.msf.second,
	    toc->entries[strack].addr.msf.frame,
	    toc->entries[etrack].addr.msf.minute,
	    toc->entries[etrack].addr.msf.second,
	    toc->entries[etrack].addr.msf.frame));
}

/*
 * Get scsi driver to send a "play msf" command
 */
static int
cd_play_msf(struct cd_softc *cd, int startm, int starts, int startf, int endm,
    int ends, int endf)
{
	struct scsipi_play_msf cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = PLAY_MSF;
	cmd.start_m = startm;
	cmd.start_s = starts;
	cmd.start_f = startf;
	cmd.end_m = endm;
	cmd.end_s = ends;
	cmd.end_f = endf;

	return (scsipi_command(cd->sc_periph, (void *)&cmd, sizeof(cmd), 0, 0,
	    CDRETRIES, 30000, NULL, 0));
}

/*
 * Get scsi driver to send a "start up" command
 */
static int
cd_pause(struct cd_softc *cd, int go)
{
	struct scsipi_pause cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = PAUSE;
	cmd.resume = go & 0xff;

	return (scsipi_command(cd->sc_periph, (void *)&cmd, sizeof(cmd), 0, 0,
	    CDRETRIES, 30000, NULL, 0));
}

/*
 * Get scsi driver to send a "RESET" command
 */
static int
cd_reset(struct cd_softc *cd)
{

	return (scsipi_command(cd->sc_periph, 0, 0, 0, 0,
	    CDRETRIES, 30000, NULL, XS_CTL_RESET));
}

/*
 * Read subchannel
 */
static int
cd_read_subchannel(struct cd_softc *cd, int mode, int format, int track,
    struct cd_sub_channel_info *data, int len, int flags)
{
	struct scsipi_read_subchannel cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = READ_SUBCHANNEL;
	if (mode == CD_MSF_FORMAT)
		cmd.byte2 |= CD_MSF;
	cmd.byte3 = SRS_SUBQ;
	cmd.subchan_format = format;
	cmd.track = track;
	_lto2b(len, cmd.data_len);

	return (scsipi_command(cd->sc_periph,
	    (void *)&cmd, sizeof(struct scsipi_read_subchannel),
	    (void *)data, len,
	    CDRETRIES, 30000, NULL, flags | XS_CTL_DATA_IN | XS_CTL_SILENT));
}

/*
 * Read table of contents
 */
static int
cd_read_toc(struct cd_softc *cd, int respf, int mode, int start,
    struct cd_formatted_toc *toc, int len, int flags, int control)
{
	struct scsipi_read_toc cmd;
	int ntoc;

	memset(&cmd, 0, sizeof(cmd));
#if 0
	if (len != sizeof(struct ioc_toc_header))
		ntoc = ((len) - sizeof(struct ioc_toc_header)) /
		    sizeof(struct cd_toc_entry);
	else
#endif
	ntoc = len;
	cmd.opcode = READ_TOC;
	if (mode == CD_MSF_FORMAT)
		cmd.addr_mode |= CD_MSF;
	cmd.resp_format = respf;
	cmd.from_track = start;
	_lto2b(ntoc, cmd.data_len);
	cmd.control = control;

	return (scsipi_command(cd->sc_periph,
	    (void *)&cmd, sizeof(cmd), (void *)toc, len, CDRETRIES,
	    30000, NULL, flags | XS_CTL_DATA_IN));
}

static int
cd_load_toc(struct cd_softc *cd, int respf, struct cd_formatted_toc *toc, int flags)
{
	int ntracks, len, error;

	if ((error = cd_read_toc(cd, respf, 0, 0, toc, sizeof(toc->header),
	    flags, 0)) != 0)
		return (error);

	ntracks = toc->header.ending_track - toc->header.starting_track + 1;
	len = (ntracks + 1) * sizeof(struct cd_toc_entry) +
	    sizeof(toc->header);
	if ((error = cd_read_toc(cd, respf, CD_MSF_FORMAT, 0, toc, len,
	    flags, 0)) != 0)
		return (error);
	return (0);
}

/*
 * Get the scsi driver to send a full inquiry to the device and use the
 * results to fill out the disk parameter structure.
 */
static int
cd_get_parms(struct cd_softc *cd, int flags)
{

	/*
	 * give a number of sectors so that sec * trks * cyls
	 * is <= disk_size
	 */
	if (cd_size(cd, flags) == 0)
		return (ENXIO);
	return (0);
}

static int
cdsize(dev_t dev)
{

	/* CD-ROMs are read-only. */
	return (-1);
}

static int
cddump(dev_t dev, daddr_t blkno, void *va, size_t size)
{

	/* Not implemented. */
	return (ENXIO);
}

#define	dvd_copy_key(dst, src)		memcpy((dst), (src), sizeof(dvd_key))
#define	dvd_copy_challenge(dst, src)	memcpy((dst), (src), sizeof(dvd_challenge))

static int
dvd_auth(struct cd_softc *cd, dvd_authinfo *a)
{
	struct scsipi_generic cmd;
	u_int8_t bf[20];
	int error;

	memset(cmd.bytes, 0, 15);
	memset(bf, 0, sizeof(bf));

	switch (a->type) {
	case DVD_LU_SEND_AGID:
		cmd.opcode = GPCMD_REPORT_KEY;
		cmd.bytes[8] = 8;
		cmd.bytes[9] = 0 | (0 << 6);
		error = scsipi_command(cd->sc_periph, &cmd, 12, bf, 8,
		    CDRETRIES, 30000, NULL, XS_CTL_DATA_IN);
		if (error)
			return (error);
		a->lsa.agid = bf[7] >> 6;
		return (0);

	case DVD_LU_SEND_CHALLENGE:
		cmd.opcode = GPCMD_REPORT_KEY;
		cmd.bytes[8] = 16;
		cmd.bytes[9] = 1 | (a->lsc.agid << 6);
		error = scsipi_command(cd->sc_periph, &cmd, 12, bf, 16,
		    CDRETRIES, 30000, NULL, XS_CTL_DATA_IN);
		if (error)
			return (error);
		dvd_copy_challenge(a->lsc.chal, &bf[4]);
		return (0);

	case DVD_LU_SEND_KEY1:
		cmd.opcode = GPCMD_REPORT_KEY;
		cmd.bytes[8] = 12;
		cmd.bytes[9] = 2 | (a->lsk.agid << 6);
		error = scsipi_command(cd->sc_periph, &cmd, 12, bf, 12,
		    CDRETRIES, 30000, NULL, XS_CTL_DATA_IN);
		if (error)
			return (error);
		dvd_copy_key(a->lsk.key, &bf[4]);
		return (0);

	case DVD_LU_SEND_TITLE_KEY:
		cmd.opcode = GPCMD_REPORT_KEY;
		_lto4b(a->lstk.lba, &cmd.bytes[1]);
		cmd.bytes[8] = 12;
		cmd.bytes[9] = 4 | (a->lstk.agid << 6);
		error = scsipi_command(cd->sc_periph, &cmd, 12, bf, 12,
		    CDRETRIES, 30000, NULL, XS_CTL_DATA_IN);
		if (error)
			return (error);
		a->lstk.cpm = (bf[4] >> 7) & 1;
		a->lstk.cp_sec = (bf[4] >> 6) & 1;
		a->lstk.cgms = (bf[4] >> 4) & 3;
		dvd_copy_key(a->lstk.title_key, &bf[5]);
		return (0);

	case DVD_LU_SEND_ASF:
		cmd.opcode = GPCMD_REPORT_KEY;
		cmd.bytes[8] = 8;
		cmd.bytes[9] = 5 | (a->lsasf.agid << 6);
		error = scsipi_command(cd->sc_periph, &cmd, 12, bf, 8,
		    CDRETRIES, 30000, NULL, XS_CTL_DATA_IN);
		if (error)
			return (error);
		a->lsasf.asf = bf[7] & 1;
		return (0);

	case DVD_HOST_SEND_CHALLENGE:
		cmd.opcode = GPCMD_SEND_KEY;
		cmd.bytes[8] = 16;
		cmd.bytes[9] = 1 | (a->hsc.agid << 6);
		bf[1] = 14;
		dvd_copy_challenge(&bf[4], a->hsc.chal);
		error = scsipi_command(cd->sc_periph, &cmd, 12, bf, 16,
		    CDRETRIES, 30000, NULL, XS_CTL_DATA_OUT);
		if (error)
			return (error);
		a->type = DVD_LU_SEND_KEY1;
		return (0);

	case DVD_HOST_SEND_KEY2:
		cmd.opcode = GPCMD_SEND_KEY;
		cmd.bytes[8] = 12;
		cmd.bytes[9] = 3 | (a->hsk.agid << 6);
		bf[1] = 10;
		dvd_copy_key(&bf[4], a->hsk.key);
		error = scsipi_command(cd->sc_periph, &cmd, 12, bf, 12,
		    CDRETRIES, 30000, NULL, XS_CTL_DATA_OUT);
		if (error) {
			a->type = DVD_AUTH_FAILURE;
			return (error);
		}
		a->type = DVD_AUTH_ESTABLISHED;
		return (0);

	case DVD_INVALIDATE_AGID:
		cmd.opcode = GPCMD_REPORT_KEY;
		cmd.bytes[9] = 0x3f | (a->lsa.agid << 6);
		error = scsipi_command(cd->sc_periph, &cmd, 12, bf, 16,
		    CDRETRIES, 30000, NULL, 0);
		if (error)
			return (error);
		return (0);

	case DVD_LU_SEND_RPC_STATE:
		cmd.opcode = GPCMD_REPORT_KEY;
		cmd.bytes[8] = 8;
		cmd.bytes[9] = 8 | (0 << 6);
		error = scsipi_command(cd->sc_periph, &cmd, 12, bf, 8,
		    CDRETRIES, 30000, NULL, XS_CTL_DATA_IN);
		if (error)
			return (error);
		a->lrpcs.type = (bf[4] >> 6) & 3;
		a->lrpcs.vra = (bf[4] >> 3) & 7;
		a->lrpcs.ucca = (bf[4]) & 7;
		a->lrpcs.region_mask = bf[5];
		a->lrpcs.rpc_scheme = bf[6];
		return (0);

	case DVD_HOST_SEND_RPC_STATE:
		cmd.opcode = GPCMD_SEND_KEY;
		cmd.bytes[8] = 8;
		cmd.bytes[9] = 6 | (0 << 6);
		bf[1] = 6;
		bf[4] = a->hrpcs.pdrc;
		error = scsipi_command(cd->sc_periph, &cmd, 12, bf, 8,
		    CDRETRIES, 30000, NULL, XS_CTL_DATA_OUT);
		if (error)
			return (error);
		return (0);

	default:
		return (ENOTTY);
	}
}

static int
dvd_read_physical(struct cd_softc *cd, dvd_struct *s)
{
	struct scsipi_generic cmd;
	u_int8_t bf[4 + 4 * 20], *bufp;
	int error;
	struct dvd_layer *layer;
	int i;

	memset(cmd.bytes, 0, 15);
	memset(bf, 0, sizeof(bf));
	cmd.opcode = GPCMD_READ_DVD_STRUCTURE;
	cmd.bytes[6] = s->type;
	_lto2b(sizeof(bf), &cmd.bytes[7]);

	cmd.bytes[5] = s->physical.layer_num;
	error = scsipi_command(cd->sc_periph, &cmd, 12, bf, sizeof(bf),
	    CDRETRIES, 30000, NULL, XS_CTL_DATA_IN);
	if (error)
		return (error);
	for (i = 0, bufp = &bf[4], layer = &s->physical.layer[0]; i < 4;
	     i++, bufp += 20, layer++) {
		memset(layer, 0, sizeof(*layer));
                layer->book_version = bufp[0] & 0xf;
                layer->book_type = bufp[0] >> 4;
                layer->min_rate = bufp[1] & 0xf;
                layer->disc_size = bufp[1] >> 4;
                layer->layer_type = bufp[2] & 0xf;
                layer->track_path = (bufp[2] >> 4) & 1;
                layer->nlayers = (bufp[2] >> 5) & 3;
                layer->track_density = bufp[3] & 0xf;
                layer->linear_density = bufp[3] >> 4;
                layer->start_sector = _4btol(&bufp[4]);
                layer->end_sector = _4btol(&bufp[8]);
                layer->end_sector_l0 = _4btol(&bufp[12]);
                layer->bca = bufp[16] >> 7;
	}
	return (0);
}

static int
dvd_read_copyright(struct cd_softc *cd, dvd_struct *s)
{
	struct scsipi_generic cmd;
	u_int8_t bf[8];
	int error;

	memset(cmd.bytes, 0, 15);
	memset(bf, 0, sizeof(bf));
	cmd.opcode = GPCMD_READ_DVD_STRUCTURE;
	cmd.bytes[6] = s->type;
	_lto2b(sizeof(bf), &cmd.bytes[7]);

	cmd.bytes[5] = s->copyright.layer_num;
	error = scsipi_command(cd->sc_periph, &cmd, 12, bf, sizeof(bf),
	    CDRETRIES, 30000, NULL, XS_CTL_DATA_IN);
	if (error)
		return (error);
	s->copyright.cpst = bf[4];
	s->copyright.rmi = bf[5];
	return (0);
}

static int
dvd_read_disckey(struct cd_softc *cd, dvd_struct *s)
{
	struct scsipi_generic cmd;
	u_int8_t *bf;
	int error;

	bf = malloc(4 + 2048, M_TEMP, M_WAITOK|M_ZERO);
	if (bf == NULL)
		return EIO;
	memset(cmd.bytes, 0, 15);
	cmd.opcode = GPCMD_READ_DVD_STRUCTURE;
	cmd.bytes[6] = s->type;
	_lto2b(4 + 2048, &cmd.bytes[7]);

	cmd.bytes[9] = s->disckey.agid << 6;
	error = scsipi_command(cd->sc_periph, &cmd, 12, bf, 4 + 2048,
	    CDRETRIES, 30000, NULL, XS_CTL_DATA_IN);
	if (error == 0)
		memcpy(s->disckey.value, &bf[4], 2048);
	free(bf, M_TEMP);
	return error;
}

static int
dvd_read_bca(struct cd_softc *cd, dvd_struct *s)
{
	struct scsipi_generic cmd;
	u_int8_t bf[4 + 188];
	int error;

	memset(cmd.bytes, 0, 15);
	memset(bf, 0, sizeof(bf));
	cmd.opcode = GPCMD_READ_DVD_STRUCTURE;
	cmd.bytes[6] = s->type;
	_lto2b(sizeof(bf), &cmd.bytes[7]);

	error = scsipi_command(cd->sc_periph, &cmd, 12, bf, sizeof(bf),
	    CDRETRIES, 30000, NULL, XS_CTL_DATA_IN);
	if (error)
		return (error);
	s->bca.len = _2btol(&bf[0]);
	if (s->bca.len < 12 || s->bca.len > 188)
		return (EIO);
	memcpy(s->bca.value, &bf[4], s->bca.len);
	return (0);
}

static int
dvd_read_manufact(struct cd_softc *cd, dvd_struct *s)
{
	struct scsipi_generic cmd;
	u_int8_t *bf;
	int error;

	bf = malloc(4 + 2048, M_TEMP, M_WAITOK|M_ZERO);
	if (bf == NULL)
		return (EIO);
	memset(cmd.bytes, 0, 15);
	cmd.opcode = GPCMD_READ_DVD_STRUCTURE;
	cmd.bytes[6] = s->type;
	_lto2b(4 + 2048, &cmd.bytes[7]);

	error = scsipi_command(cd->sc_periph, &cmd, 12, bf, 4 + 2048,
	    CDRETRIES, 30000, NULL, XS_CTL_DATA_IN);
	if (error == 0) {
		s->manufact.len = _2btol(&bf[0]);
		if (s->manufact.len >= 0 && s->manufact.len <= 2048)
			memcpy(s->manufact.value, &bf[4], s->manufact.len);
		else
			error = EIO;
	}
	free(bf, M_TEMP);
	return error;
}

static int
dvd_read_struct(struct cd_softc *cd, dvd_struct *s)
{

	switch (s->type) {
	case DVD_STRUCT_PHYSICAL:
		return (dvd_read_physical(cd, s));
	case DVD_STRUCT_COPYRIGHT:
		return (dvd_read_copyright(cd, s));
	case DVD_STRUCT_DISCKEY:
		return (dvd_read_disckey(cd, s));
	case DVD_STRUCT_BCA:
		return (dvd_read_bca(cd, s));
	case DVD_STRUCT_MANUFACT:
		return (dvd_read_manufact(cd, s));
	default:
		return (EINVAL);
	}
}

static int
cd_mode_sense(struct cd_softc *cd, u_int8_t byte2, void *sense, size_t size,
    int page, int flags, int *big)
{

	if (cd->sc_periph->periph_quirks & PQUIRK_ONLYBIG) {
		*big = 1;
		return scsipi_mode_sense_big(cd->sc_periph, byte2, page, sense,
		    size + sizeof(struct scsi_mode_parameter_header_10),
		    flags, CDRETRIES, 20000);
	} else {
		*big = 0;
		return scsipi_mode_sense(cd->sc_periph, byte2, page, sense,
		    size + sizeof(struct scsi_mode_parameter_header_6),
		    flags, CDRETRIES, 20000);
	}
}

static int
cd_mode_select(struct cd_softc *cd, u_int8_t byte2, void *sense, size_t size,
    int flags, int big)
{

	if (big) {
		struct scsi_mode_parameter_header_10 *header = sense;

		_lto2b(0, header->data_length);
		return scsipi_mode_select_big(cd->sc_periph, byte2, sense,
		    size + sizeof(struct scsi_mode_parameter_header_10),
		    flags, CDRETRIES, 20000);
	} else {
		struct scsi_mode_parameter_header_6 *header = sense;

		header->data_length = 0;
		return scsipi_mode_select(cd->sc_periph, byte2, sense,
		    size + sizeof(struct scsi_mode_parameter_header_6),
		    flags, CDRETRIES, 20000);
	}
}

static int
cd_set_pa_immed(struct cd_softc *cd, int flags)
{
	struct {
		union {
			struct scsi_mode_parameter_header_6 small;
			struct scsi_mode_parameter_header_10 big;
		} header;
		struct cd_audio_page page;
	} data;
	int error;
	uint8_t oflags;
	int big, byte2;
	struct cd_audio_page *page;

	byte2 = SMS_DBD;
try_again:
	if ((error = cd_mode_sense(cd, byte2, &data, sizeof(data.page),
	    AUDIO_PAGE, flags, &big)) != 0) {
		if (byte2 == SMS_DBD) {
			/* Device may not understand DBD; retry without */
			byte2 = 0;
			goto try_again;
		}
		return (error);
	}

	if (big)
		page = (void *)((u_long)&data.header.big +
				sizeof data.header.big +
				_2btol(data.header.big.blk_desc_len));
	else
		page = (void *)((u_long)&data.header.small +
				sizeof data.header.small +
				data.header.small.blk_desc_len);

	oflags = page->flags;
	page->flags &= ~CD_PA_SOTC;
	page->flags |= CD_PA_IMMED;
	if (oflags == page->flags)
		return (0);

	return (cd_mode_select(cd, SMS_PF, &data,
	    sizeof(struct scsi_mode_page_header) + page->pg_length,
	    flags, big));
}

static int
cd_setchan(struct cd_softc *cd, int p0, int p1, int p2, int p3, int flags)
{
	struct {
		union {
			struct scsi_mode_parameter_header_6 small;
			struct scsi_mode_parameter_header_10 big;
		} header;
		struct cd_audio_page page;
	} data;
	int error;
	int big, byte2;
	struct cd_audio_page *page;

	byte2 = SMS_DBD;
try_again:
	if ((error = cd_mode_sense(cd, byte2, &data, sizeof(data.page),
	    AUDIO_PAGE, flags, &big)) != 0) {
		if (byte2 == SMS_DBD) {
			/* Device may not understand DBD; retry without */
			byte2 = 0;
			goto try_again;
		}
		return (error);
	}

	if (big)
		page = (void *)((u_long)&data.header.big +
				sizeof data.header.big +
				_2btol(data.header.big.blk_desc_len));
	else
		page = (void *)((u_long)&data.header.small +
				sizeof data.header.small +
				data.header.small.blk_desc_len);

	page->port[0].channels = p0;
	page->port[1].channels = p1;
	page->port[2].channels = p2;
	page->port[3].channels = p3;

	return (cd_mode_select(cd, SMS_PF, &data,
	    sizeof(struct scsi_mode_page_header) + page->pg_length,
	    flags, big));
}

static int
cd_getvol(struct cd_softc *cd, struct ioc_vol *arg, int flags)
{
	struct {
		union {
			struct scsi_mode_parameter_header_6 small;
			struct scsi_mode_parameter_header_10 big;
		} header;
		struct cd_audio_page page;
	} data;
	int error;
	int big, byte2;
	struct cd_audio_page *page;

	byte2 = SMS_DBD;
try_again:
	if ((error = cd_mode_sense(cd, byte2, &data, sizeof(data.page),
	    AUDIO_PAGE, flags, &big)) != 0) {
		if (byte2 == SMS_DBD) {
			/* Device may not understand DBD; retry without */
			byte2 = 0;
			goto try_again;
		}
		return (error);
	}

	if (big)
		page = (void *)((u_long)&data.header.big +
				sizeof data.header.big +
				_2btol(data.header.big.blk_desc_len));
	else
		page = (void *)((u_long)&data.header.small +
				sizeof data.header.small +
				data.header.small.blk_desc_len);

	arg->vol[0] = page->port[0].volume;
	arg->vol[1] = page->port[1].volume;
	arg->vol[2] = page->port[2].volume;
	arg->vol[3] = page->port[3].volume;

	return (0);
}

static int
cd_setvol(struct cd_softc *cd, const struct ioc_vol *arg, int flags)
{
	struct {
		union {
			struct scsi_mode_parameter_header_6 small;
			struct scsi_mode_parameter_header_10 big;
		} header;
		struct cd_audio_page page;
	} data, mask;
	int error;
	int big, byte2;
	struct cd_audio_page *page, *page2;

	byte2 = SMS_DBD;
try_again:
	if ((error = cd_mode_sense(cd, byte2, &data, sizeof(data.page),
	    AUDIO_PAGE, flags, &big)) != 0) {
		if (byte2 == SMS_DBD) {
			/* Device may not understand DBD; retry without */
			byte2 = 0;
			goto try_again;
		}
		return (error);
	}
	if ((error = cd_mode_sense(cd, byte2, &mask, sizeof(mask.page),
	    AUDIO_PAGE|SMS_PCTRL_CHANGEABLE, flags, &big)) != 0)
		return (error);

	if (big) {
		page = (void *)((u_long)&data.header.big +
				sizeof data.header.big +
				_2btol(data.header.big.blk_desc_len));
		page2 = (void *)((u_long)&mask.header.big +
				sizeof mask.header.big +
				_2btol(mask.header.big.blk_desc_len));
	} else {
		page = (void *)((u_long)&data.header.small +
				sizeof data.header.small +
				data.header.small.blk_desc_len);
		page2 = (void *)((u_long)&mask.header.small +
				sizeof mask.header.small +
				mask.header.small.blk_desc_len);
	}

	page->port[0].volume = arg->vol[0] & page2->port[0].volume;
	page->port[1].volume = arg->vol[1] & page2->port[1].volume;
	page->port[2].volume = arg->vol[2] & page2->port[2].volume;
	page->port[3].volume = arg->vol[3] & page2->port[3].volume;

	page->port[0].channels = CHANNEL_0;
	page->port[1].channels = CHANNEL_1;

	return (cd_mode_select(cd, SMS_PF, &data,
	    sizeof(struct scsi_mode_page_header) + page->pg_length,
	    flags, big));
}

static int
cd_load_unload(struct cd_softc *cd, struct ioc_load_unload *args)
{
	struct scsipi_load_unload cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = LOAD_UNLOAD;
	cmd.options = args->options;    /* ioctl uses MMC values */
	cmd.slot = args->slot;

	return (scsipi_command(cd->sc_periph, (void *)&cmd, sizeof(cmd), 0, 0,
	    CDRETRIES, 200000, NULL, 0));
}

static int
cd_setblksize(struct cd_softc *cd)
{
	struct {
		union {
			struct scsi_mode_parameter_header_6 small;
			struct scsi_mode_parameter_header_10 big;
		} header;
		struct scsi_general_block_descriptor blk_desc;
	} data;
	int error;
	int big, bsize;
	struct scsi_general_block_descriptor *bdesc;

	if ((error = cd_mode_sense(cd, 0, &data, sizeof(data.blk_desc), 0, 0,
	    &big)) != 0)
		return (error);

	if (big) {
		bdesc = (void *)(&data.header.big + 1);
		bsize = _2btol(data.header.big.blk_desc_len);
	} else {
		bdesc = (void *)(&data.header.small + 1);
		bsize = data.header.small.blk_desc_len;
	}

	if (bsize == 0) {
printf("cd_setblksize: trying to change bsize, but no blk_desc\n");
		return (EINVAL);
	}
	if (_3btol(bdesc->blklen) == 2048) {
printf("cd_setblksize: trying to change bsize, but blk_desc is correct\n");
		return (EINVAL);
	}

	_lto3b(2048, bdesc->blklen);

	return (cd_mode_select(cd, SMS_PF, &data, sizeof(data.blk_desc), 0,
	    big));
}


static int
mmc_profile2class(uint16_t mmc_profile)
{
	switch (mmc_profile) {
	case 0x01 : /* SCSI discs */
	case 0x02 :
		/* this can't happen really, cd.c wouldn't have matched */
		return MMC_CLASS_DISC;
	case 0x03 : /* Magneto Optical with sector erase */
	case 0x04 : /* Magneto Optical write once        */
	case 0x05 : /* Advance Storage Magneto Optical   */
		return MMC_CLASS_MO;
	case 0x00 : /* Unknown MMC profile, can also be CD-ROM */
	case 0x08 : /* CD-ROM  */
	case 0x09 : /* CD-R    */
	case 0x0a : /* CD-RW   */
		return MMC_CLASS_CD;
	case 0x10 : /* DVD-ROM */
	case 0x11 : /* DVD-R   */
	case 0x12 : /* DVD-RAM */
	case 0x13 : /* DVD-RW restricted overwrite */
	case 0x14 : /* DVD-RW sequential */
	case 0x1a : /* DVD+RW  */
	case 0x1b : /* DVD+R   */
	case 0x2a : /* DVD+RW Dual layer */
	case 0x2b : /* DVD+R Dual layer */
	case 0x50 : /* HD DVD-ROM */
	case 0x51 : /* HD DVD-R   */
	case 0x52 : /* HD DVD-RW; DVD-RAM like */
		return MMC_CLASS_DVD;
	case 0x40 : /* BD-ROM  */
	case 0x41 : /* BD-R Sequential recording (SRM) */
	case 0x42 : /* BD-R Ramdom Recording (RRM) */
	case 0x43 : /* BD-RE */
		return MMC_CLASS_BD;
	}
	return MMC_CLASS_UNKN;
}


/*
 * Drive/media combination is reflected in a series of features that can
 * either be current or dormant. We try to make sense out of them to create a
 * set of easy to use flags that abstract the device/media capabilities.
 */

static void
mmc_process_feature(struct mmc_discinfo *mmc_discinfo,
		    uint16_t feature, int cur, uint8_t *rpos)
{
	uint32_t blockingnr;
	uint64_t flags;

	if (cur == 1) {
		flags = mmc_discinfo->mmc_cur;
	} else {
		flags = mmc_discinfo->mmc_cap;
	}

	switch (feature) {
	case 0x0010 :	/* random readable feature */
		blockingnr  =  rpos[5] | (rpos[4] << 8);
		if (blockingnr > 1)
			flags |= MMC_CAP_PACKET;

		/* RW error page */
		break;
	case 0x0020 :	/* random writable feature */
		flags |= MMC_CAP_RECORDABLE;
		flags |= MMC_CAP_REWRITABLE;
		blockingnr  =  rpos[9] | (rpos[8] << 8);
		if (blockingnr > 1)
			flags |= MMC_CAP_PACKET;
		break;
	case 0x0021 :	/* incremental streaming write feature */
		flags |= MMC_CAP_RECORDABLE;
		flags |= MMC_CAP_SEQUENTIAL;
		if (cur)
			mmc_discinfo->link_block_penalty = rpos[4];
		if (rpos[2] & 1)
			flags |= MMC_CAP_ZEROLINKBLK;
		break;
	case 0x0022 : 	/* (obsolete) erase support feature */
		flags |= MMC_CAP_RECORDABLE;
		flags |= MMC_CAP_ERASABLE;
		break;
	case 0x0023 :	/* formatting media support feature */
		flags |= MMC_CAP_RECORDABLE;
		flags |= MMC_CAP_FORMATTABLE;
		break;
	case 0x0024 :	/* hardware assised defect management feature */
		flags |= MMC_CAP_HW_DEFECTFREE;
		break;
	case 0x0025 : 	/* write once */
		flags |= MMC_CAP_RECORDABLE;
		break;
	case 0x0026 :	/* restricted overwrite feature */
		flags |= MMC_CAP_RECORDABLE;
		flags |= MMC_CAP_REWRITABLE;
		flags |= MMC_CAP_STRICTOVERWRITE;
		break;
	case 0x0028 :	/* MRW formatted media support feature */
		flags |= MMC_CAP_MRW;
		break;
	case 0x002b :	/* DVD+R read (and opt. write) support */
		flags |= MMC_CAP_SEQUENTIAL;
		if (rpos[0] & 1) /* write support */
			flags |= MMC_CAP_RECORDABLE;
		break;
	case 0x002c :	/* rigid restricted overwrite feature */
		flags |= MMC_CAP_RECORDABLE;
		flags |= MMC_CAP_REWRITABLE;
		flags |= MMC_CAP_STRICTOVERWRITE;
		if (rpos[0] & 1) /* blank bit */
			flags |= MMC_CAP_BLANKABLE;
		break;
	case 0x002d :	/* track at once recording feature */
		flags |= MMC_CAP_RECORDABLE;
		flags |= MMC_CAP_SEQUENTIAL;
		break;
	case 0x002f :	/* DVD-R/-RW write feature */
		flags |= MMC_CAP_RECORDABLE;
		if (rpos[0] & 2) /* DVD-RW bit */
			flags |= MMC_CAP_BLANKABLE;
		break;
	case 0x0038 :	/* BD-R SRM with pseudo overwrite */
		flags |= MMC_CAP_PSEUDOOVERWRITE;
		break;
	default :
		/* ignore */
		break;
	}

	if (cur == 1) {
		mmc_discinfo->mmc_cur = flags;
	} else {
		mmc_discinfo->mmc_cap = flags;
	}
}

static int
mmc_getdiscinfo_cdrom(struct scsipi_periph *periph,
		      struct mmc_discinfo *mmc_discinfo)
{
	struct scsipi_read_toc      gtoc_cmd;
	struct scsipi_toc_header   *toc_hdr;
	struct scsipi_toc_msinfo   *toc_msinfo;
	const uint32_t buffer_size = 1024;
	uint32_t req_size;
	uint8_t  *buffer;
	int error, flags;

	buffer = malloc(buffer_size, M_TEMP, M_WAITOK);
	/*
	 * Fabricate mmc_discinfo for CD-ROM. Some values are really `dont
	 * care' but others might be of interest to programs.
	 */

	mmc_discinfo->disc_state	 = MMC_STATE_FULL;
	mmc_discinfo->last_session_state = MMC_STATE_FULL;
	mmc_discinfo->bg_format_state    = MMC_BGFSTATE_COMPLETED;
	mmc_discinfo->link_block_penalty = 7;	/* not relevant */

	/* get number of sessions and first tracknr in last session */
	flags = XS_CTL_DATA_IN;
	memset(&gtoc_cmd, 0, sizeof(gtoc_cmd));
	gtoc_cmd.opcode      = READ_TOC;
	gtoc_cmd.addr_mode   = CD_MSF;		/* not relevant        */
	gtoc_cmd.resp_format = CD_TOC_MSINFO;	/* multisession info   */
	gtoc_cmd.from_track  = 0;		/* reserved, must be 0 */
	req_size = sizeof(*toc_hdr) + sizeof(*toc_msinfo);
	_lto2b(req_size, gtoc_cmd.data_len);

	error = scsipi_command(periph,
		(void *)&gtoc_cmd, sizeof(gtoc_cmd),
		(void *)buffer,    req_size,
		CDRETRIES, 30000, NULL, flags);
	if (error)
		goto out;
	toc_hdr    = (struct scsipi_toc_header *)  buffer;
	toc_msinfo = (struct scsipi_toc_msinfo *) (buffer + 4);
	mmc_discinfo->num_sessions = toc_hdr->last - toc_hdr->first + 1;
	mmc_discinfo->first_track  = toc_hdr->first;
	mmc_discinfo->first_track_last_session = toc_msinfo->tracknr;

	/* get last track of last session */
	flags = XS_CTL_DATA_IN;
	gtoc_cmd.resp_format  = CD_TOC_FORM;	/* formatted toc */
	req_size = sizeof(*toc_hdr);
	_lto2b(req_size, gtoc_cmd.data_len);

	error = scsipi_command(periph,
		(void *)&gtoc_cmd, sizeof(gtoc_cmd),
		(void *)buffer,    req_size,
		CDRETRIES, 30000, NULL, flags);
	if (error)
		goto out;
	toc_hdr    = (struct scsipi_toc_header *) buffer;
	mmc_discinfo->last_track_last_session = toc_hdr->last;
	mmc_discinfo->num_tracks = toc_hdr->last - toc_hdr->first + 1;

	/* TODO how to handle disc_barcode and disc_id */
	/* done */

out:
	free(buffer, M_TEMP);
	return error;
}

static int
mmc_getdiscinfo_dvdrom(struct scsipi_periph *periph,
		       struct mmc_discinfo *mmc_discinfo)
{
	struct scsipi_read_toc   gtoc_cmd;
	struct scsipi_toc_header toc_hdr;
	uint32_t req_size;
	int error, flags;

	/*
	 * Fabricate mmc_discinfo for DVD-ROM. Some values are really `dont
	 * care' but others might be of interest to programs.
	 */

	mmc_discinfo->disc_state	 = MMC_STATE_FULL;
	mmc_discinfo->last_session_state = MMC_STATE_FULL;
	mmc_discinfo->bg_format_state    = MMC_BGFSTATE_COMPLETED;
	mmc_discinfo->link_block_penalty = 16;	/* not relevant */

	/* get number of sessions and first tracknr in last session */
	flags = XS_CTL_DATA_IN;
	memset(&gtoc_cmd, 0, sizeof(gtoc_cmd));
	gtoc_cmd.opcode      = READ_TOC;
	gtoc_cmd.addr_mode   = 0;		/* LBA                 */
	gtoc_cmd.resp_format = CD_TOC_FORM;	/* multisession info   */
	gtoc_cmd.from_track  = 1;		/* first track         */
	req_size = sizeof(toc_hdr);
	_lto2b(req_size, gtoc_cmd.data_len);

	error = scsipi_command(periph,
		(void *)&gtoc_cmd, sizeof(gtoc_cmd),
		(void *)&toc_hdr,  req_size,
		CDRETRIES, 30000, NULL, flags);
	if (error)
		return error;

	/* DVD-ROM squashes the track/session space */
	mmc_discinfo->num_sessions = toc_hdr.last - toc_hdr.first + 1;
	mmc_discinfo->num_tracks   = mmc_discinfo->num_sessions;
	mmc_discinfo->first_track  = toc_hdr.first;
	mmc_discinfo->first_track_last_session = toc_hdr.last;
	mmc_discinfo->last_track_last_session  = toc_hdr.last;

	/* TODO how to handle disc_barcode and disc_id */
	/* done */
	return 0;
}

static int
mmc_getdiscinfo(struct scsipi_periph *periph,
		struct mmc_discinfo *mmc_discinfo)
{
	struct scsipi_get_configuration   gc_cmd;
	struct scsipi_get_conf_data      *gc;
	struct scsipi_get_conf_feature   *gcf;
	struct scsipi_read_discinfo       di_cmd;
	struct scsipi_read_discinfo_data  di;
	const uint32_t buffer_size = 1024;
	uint32_t feat_tbl_len, pos;
	u_long   last_lba = 0;
	uint8_t  *buffer, *fpos;
	int feature, last_feature, features_len, feature_cur, feature_len;
	int lsb, msb, error, flags;

	feat_tbl_len = buffer_size;

	buffer = malloc(buffer_size, M_TEMP, M_WAITOK);

	/* initialise structure */
	memset(mmc_discinfo, 0, sizeof(struct mmc_discinfo));
	mmc_discinfo->mmc_profile = 0x00;	/* unknown */
	mmc_discinfo->mmc_class   = MMC_CLASS_UNKN;
	mmc_discinfo->mmc_cur     = 0;
	mmc_discinfo->mmc_cap     = 0;
	mmc_discinfo->link_block_penalty = 0;

	/* determine mmc profile and class */
	flags = XS_CTL_DATA_IN;
	memset(&gc_cmd, 0, sizeof(gc_cmd));
	gc_cmd.opcode = GET_CONFIGURATION;
	_lto2b(GET_CONF_NO_FEATURES_LEN, gc_cmd.data_len);

	gc = (struct scsipi_get_conf_data *) buffer;

	error = scsipi_command(periph,
		(void *)&gc_cmd, sizeof(gc_cmd),
		(void *) gc,     GET_CONF_NO_FEATURES_LEN,
		CDRETRIES, 30000, NULL, flags);
	if (error)
		goto out;

	mmc_discinfo->mmc_profile = _2btol(gc->mmc_profile);
	mmc_discinfo->mmc_class = mmc_profile2class(mmc_discinfo->mmc_profile);

	/* assume 2048 sector size unless told otherwise */
	mmc_discinfo->sector_size = 2048;
	error = read_cd_capacity(periph, &mmc_discinfo->sector_size, &last_lba);
	if (error)
		goto out;

	mmc_discinfo->last_possible_lba = (uint32_t) last_lba;

	/* Read in all features to determine device capabilities */
	last_feature = feature = 0;
	do {
		/* determine mmc profile and class */
		flags = XS_CTL_DATA_IN;
		memset(&gc_cmd, 0, sizeof(gc_cmd));
		gc_cmd.opcode = GET_CONFIGURATION;
		_lto2b(last_feature, gc_cmd.start_at_feature);
		_lto2b(feat_tbl_len, gc_cmd.data_len);
		memset(gc, 0, feat_tbl_len);

		error = scsipi_command(periph,
			(void *)&gc_cmd, sizeof(gc_cmd),
			(void *) gc,     feat_tbl_len,
			CDRETRIES, 30000, NULL, flags);
		if (error) {
			/* ieeek... break out of loop... i dunno what to do */
			break;
		}

		features_len = _4btol(gc->data_len);
		if (features_len < 4 || features_len > feat_tbl_len)
			break;

		pos  = 0;
		fpos = &gc->feature_desc[0];
		while (pos < features_len - 4) {
			gcf = (struct scsipi_get_conf_feature *) fpos;

			feature     = _2btol(gcf->featurecode);
			feature_cur = gcf->flags & 1;
			feature_len = gcf->additional_length;

			mmc_process_feature(mmc_discinfo,
					    feature, feature_cur,
					    gcf->feature_dependent);

			last_feature = MAX(last_feature, feature);
#ifdef DIAGNOSTIC
			/* assert((feature_len & 3) == 0); */
			if ((feature_len & 3) != 0) {
				printf("feature %d having length %d\n",
					feature, feature_len);
			}
#endif

			pos  += 4 + feature_len;
			fpos += 4 + feature_len;
		}
		/* unlikely to ever grow past our 1kb buffer */
	} while (features_len >= 0xffff);

	/*
	 * Fixup CD-RW drives that are on crack.
	 *
	 * Some drives report the capability to incrementally write
	 * sequentially on CD-R(W) media...  nice, but this should not be
	 * active for a fixed packet formatted CD-RW media. Other report the
	 * ability of HW_DEFECTFREE even when the media is NOT MRW
	 * formatted....
	 */
	if (mmc_discinfo->mmc_profile == 0x0a) {
		if ((mmc_discinfo->mmc_cur & MMC_CAP_SEQUENTIAL) == 0)
			mmc_discinfo->mmc_cur |= MMC_CAP_STRICTOVERWRITE;
		if (mmc_discinfo->mmc_cur & MMC_CAP_STRICTOVERWRITE)
			mmc_discinfo->mmc_cur &= ~MMC_CAP_SEQUENTIAL;
		if (mmc_discinfo->mmc_cur & MMC_CAP_MRW) {
			mmc_discinfo->mmc_cur &= ~MMC_CAP_SEQUENTIAL;
			mmc_discinfo->mmc_cur &= ~MMC_CAP_STRICTOVERWRITE;
		} else {
			mmc_discinfo->mmc_cur &= ~MMC_CAP_HW_DEFECTFREE;
		}
	}
	if (mmc_discinfo->mmc_profile == 0x09) {
		mmc_discinfo->mmc_cur &= ~MMC_CAP_REWRITABLE;
	}

#ifdef DEBUG
	printf("CD mmc %d, mmc_cur 0x%"PRIx64", mmc_cap 0x%"PRIx64"\n",
		mmc_discinfo->mmc_profile,
	 	mmc_discinfo->mmc_cur, mmc_discinfo->mmc_cap);
#endif

	/* read in disc state and number of sessions and tracks */
	flags = XS_CTL_DATA_IN | XS_CTL_SILENT;
	memset(&di_cmd, 0, sizeof(di_cmd));
	di_cmd.opcode = READ_DISCINFO;
	di_cmd.data_len[1] = READ_DISCINFO_BIGSIZE;

	error = scsipi_command(periph,
		(void *)&di_cmd, sizeof(di_cmd),
		(void *)&di,     READ_DISCINFO_BIGSIZE,
		CDRETRIES, 30000, NULL, flags);

	if (error) {
		/* discinfo call failed, emulate for cd-rom/dvd-rom */
		if (mmc_discinfo->mmc_profile == 0x08) /* CD-ROM */
			return mmc_getdiscinfo_cdrom(periph, mmc_discinfo);
		if (mmc_discinfo->mmc_profile == 0x10) /* DVD-ROM */
			return mmc_getdiscinfo_dvdrom(periph, mmc_discinfo);
		/* CD/DVD drive is violating specs */
		error = EIO;
		goto out;
	}

	/* call went OK */
	mmc_discinfo->disc_state         =  di.disc_state & 3;
	mmc_discinfo->last_session_state = (di.disc_state >> 2) & 3;
	mmc_discinfo->bg_format_state    = (di.disc_state2 & 3);

	lsb = di.num_sessions_lsb;
	msb = di.num_sessions_msb;
	mmc_discinfo->num_sessions = lsb | (msb << 8);

	mmc_discinfo->first_track = di.first_track;
	lsb = di.first_track_last_session_lsb;
	msb = di.first_track_last_session_msb;
	mmc_discinfo->first_track_last_session = lsb | (msb << 8);
	lsb = di.last_track_last_session_lsb;
	msb = di.last_track_last_session_msb;
	mmc_discinfo->last_track_last_session  = lsb | (msb << 8);

	mmc_discinfo->num_tracks = mmc_discinfo->last_track_last_session -
		mmc_discinfo->first_track + 1;

	/* set misc. flags and parameters from this disc info */
	if (di.disc_state  &  16)
		mmc_discinfo->mmc_cur |= MMC_CAP_BLANKABLE;

	if (di.disc_state2 & 128) {
		mmc_discinfo->disc_id = _4btol(di.discid);
		mmc_discinfo->disc_flags |= MMC_DFLAGS_DISCIDVALID;
	}
	if (di.disc_state2 &  64) {
		mmc_discinfo->disc_barcode = _8btol(di.disc_bar_code);
		mmc_discinfo->disc_flags |= MMC_DFLAGS_BARCODEVALID;
	}
	if (di.disc_state2 &  32)
		mmc_discinfo->disc_flags |= MMC_DFLAGS_UNRESTRICTED;

	if (di.disc_state2 &  16) {
		mmc_discinfo->application_code = di.application_code;
		mmc_discinfo->disc_flags |= MMC_DFLAGS_APPCODEVALID;
	}

	/* done */

out:
	free(buffer, M_TEMP);
	return error;
}

static int
mmc_gettrackinfo_cdrom(struct scsipi_periph *periph,
		       struct mmc_trackinfo *trackinfo)
{
	struct scsipi_read_toc            gtoc_cmd;
	struct scsipi_toc_header         *toc_hdr;
	struct scsipi_toc_rawtoc         *rawtoc;
	uint32_t track_start, track_size;
	uint32_t last_recorded, next_writable;
	uint32_t lba, next_track_start, lead_out;
	const uint32_t buffer_size = 4 * 1024;	/* worst case TOC estimate */
	uint8_t *buffer;
	uint8_t track_sessionnr, sessionnr, adr, tno, point;
	uint8_t control, tmin, tsec, tframe, pmin, psec, pframe;
	int size, req_size;
	int error, flags;

	buffer = malloc(buffer_size, M_TEMP, M_WAITOK);

	/*
	 * Emulate read trackinfo for CD-ROM using the raw-TOC.
	 *
	 * Not all information is present and this presents a problem.  Track
	 * starts are known for each track but other values are deducted.
	 *
	 * For a complete overview of `magic' values used here, see the
	 * SCSI/ATAPI MMC documentation. Note that the `magic' values have no
	 * names, they are specified as numbers.
	 */

	/* get raw toc to process, first header to check size */
	flags = XS_CTL_DATA_IN | XS_CTL_SILENT;
	memset(&gtoc_cmd, 0, sizeof(gtoc_cmd));
	gtoc_cmd.opcode      = READ_TOC;
	gtoc_cmd.addr_mode   = CD_MSF;		/* not relevant     */
	gtoc_cmd.resp_format = CD_TOC_RAW;	/* raw toc          */
	gtoc_cmd.from_track  = 1;		/* first session    */
	req_size = sizeof(*toc_hdr);
	_lto2b(req_size, gtoc_cmd.data_len);

	error = scsipi_command(periph,
		(void *)&gtoc_cmd, sizeof(gtoc_cmd),
		(void *)buffer,    req_size,
		CDRETRIES, 30000, NULL, flags);
	if (error)
		goto out;
	toc_hdr = (struct scsipi_toc_header *) buffer;
	if (_2btol(toc_hdr->length) > buffer_size - 2) {
#ifdef DIAGNOSTIC
		printf("increase buffersize in mmc_readtrackinfo_cdrom\n");
#endif
		error = ENOBUFS;
		goto out;
	}

	/* read in complete raw toc */
	req_size = _2btol(toc_hdr->length);
	req_size = 2*((req_size + 1) / 2);	/* for ATAPI */
	_lto2b(req_size, gtoc_cmd.data_len);

	error = scsipi_command(periph,
		(void *)&gtoc_cmd, sizeof(gtoc_cmd),
		(void *)buffer,    req_size,
		CDRETRIES, 30000, NULL, flags);
	if (error)
		goto out;

	toc_hdr = (struct scsipi_toc_header *) buffer;
	rawtoc  = (struct scsipi_toc_rawtoc *) (buffer + 4);

	track_start      = 0;
	track_size       = 0;
	last_recorded    = 0;
	next_writable    = 0;
	flags            = 0;

	next_track_start = 0;
	track_sessionnr  = MAXTRACK;	/* by definition */
	lead_out         = 0;

	size = req_size - sizeof(struct scsipi_toc_header) + 1;
	while (size > 0) {
		/* get track start and session end */
		tno       = rawtoc->tno;
		sessionnr = rawtoc->sessionnr;
		adr       = rawtoc->adrcontrol >> 4;
		control   = rawtoc->adrcontrol & 0xf;
		point     = rawtoc->point;
		tmin      = rawtoc->min;
		tsec      = rawtoc->sec;
		tframe    = rawtoc->frame;
		pmin      = rawtoc->pmin;
		psec      = rawtoc->psec;
		pframe    = rawtoc->pframe;

		if (tno == 0 && sessionnr && adr == 1) {
			lba = hmsf2lba(0, pmin, psec, pframe);
			if (point == trackinfo->tracknr) {
				track_start = lba;
				track_sessionnr = sessionnr;
			}
			if (point == trackinfo->tracknr + 1) {
				/* estimate size */
				track_size = lba - track_start;
				next_track_start = lba;
			}
			if (point == 0xa2) {
				lead_out = lba;
			}
			if (point <= 0x63) {
				/* CD's ok, DVD are glued */
				/* last_tracknr = point; */
			}
			if (sessionnr == track_sessionnr) {
				last_recorded = lead_out;
			}
		}
		if (tno == 0 && sessionnr && adr == 5) {
			lba = hmsf2lba(0, tmin, tsec, tframe);
			if (sessionnr == track_sessionnr) {
				next_writable = lba;
			}
		}

		if ((control & (3<<2)) == 4)		/* 01xxb */
			flags |= MMC_TRACKINFO_DATA;
		if ((control & (1<<2)) == 0) {		/* x0xxb */
			flags |= MMC_TRACKINFO_AUDIO;
			if (control & 1)		/* xxx1b */
				flags |= MMC_TRACKINFO_PRE_EMPH;
		}

		rawtoc++;
		size -= sizeof(struct scsipi_toc_rawtoc);
	}

	/* process found values; some voodoo */
	/* if no tracksize tracknr is the last of the disc */
	if ((track_size == 0) && last_recorded) {
		track_size = last_recorded - track_start;
	}
	/* if last_recorded < tracksize, tracksize is overestimated */
	if (last_recorded) {
		if (last_recorded - track_start <= track_size) {
			track_size = last_recorded - track_start;
			flags |= MMC_TRACKINFO_LRA_VALID;
		}
	}
	/* check if its a the last track of the sector */
	if (next_writable) {
		if (next_track_start > next_writable)
			flags |= MMC_TRACKINFO_NWA_VALID;
	}

	/* no flag set -> no values */
	if ((flags & MMC_TRACKINFO_LRA_VALID) == 0)
		last_recorded = 0;
	if ((flags & MMC_TRACKINFO_NWA_VALID) == 0)
		next_writable = 0;

	/* fill in */
	/* trackinfo->tracknr preserved */
	trackinfo->sessionnr  = track_sessionnr;
	trackinfo->track_mode = 7;	/* data, incremental  */
	trackinfo->data_mode  = 8;	/* 2048 bytes mode1   */

	trackinfo->flags = flags;
	trackinfo->track_start   = track_start;
	trackinfo->next_writable = next_writable;
	trackinfo->free_blocks   = 0;
	trackinfo->packet_size   = 1;
	trackinfo->track_size    = track_size;
	trackinfo->last_recorded = last_recorded;

out:
	free(buffer, M_TEMP);
	return error;

}

static int
mmc_gettrackinfo_dvdrom(struct scsipi_periph *periph, 
			struct mmc_trackinfo *trackinfo)
{
	struct scsipi_read_toc            gtoc_cmd;
	struct scsipi_toc_header         *toc_hdr;
	struct scsipi_toc_formatted      *toc;
	uint32_t tracknr, track_start, track_size;
	uint32_t lba, lead_out;
	const uint32_t buffer_size = 4 * 1024;	/* worst case TOC estimate */
	uint8_t *buffer;
	uint8_t control, last_tracknr;
	int size, req_size;
	int error, flags;

	
	buffer = malloc(buffer_size, M_TEMP, M_WAITOK);
	/*
	 * Emulate read trackinfo for DVD-ROM. We can't use the raw-TOC as the
	 * CD-ROM emulation uses since the specification tells us that no such
	 * thing is defined for DVD's. The reason for this is due to the large
	 * number of tracks and that would clash with the `magic' values. This
	 * suxs.
	 *
	 * Not all information is present and this presents a problem.
	 * Track starts are known for each track but other values are
	 * deducted.
	 */

	/* get formatted toc to process, first header to check size */
	flags = XS_CTL_DATA_IN | XS_CTL_SILENT;
	memset(&gtoc_cmd, 0, sizeof(gtoc_cmd));
	gtoc_cmd.opcode      = READ_TOC;
	gtoc_cmd.addr_mode   = 0;		/* lba's please     */
	gtoc_cmd.resp_format = CD_TOC_FORM;	/* formatted toc    */
	gtoc_cmd.from_track  = 1;		/* first track      */
	req_size = sizeof(*toc_hdr);
	_lto2b(req_size, gtoc_cmd.data_len);

	error = scsipi_command(periph,
		(void *)&gtoc_cmd, sizeof(gtoc_cmd),
		(void *)buffer,    req_size,
		CDRETRIES, 30000, NULL, flags);
	if (error)
		goto out;
	toc_hdr = (struct scsipi_toc_header *) buffer;
	if (_2btol(toc_hdr->length) > buffer_size - 2) {
#ifdef DIAGNOSTIC
		printf("incease buffersize in mmc_readtrackinfo_dvdrom\n");
#endif
		error = ENOBUFS;
		goto out;
	}

	/* read in complete formatted toc */
	req_size = _2btol(toc_hdr->length);
	_lto2b(req_size, gtoc_cmd.data_len);

	error = scsipi_command(periph,
		(void *)&gtoc_cmd, sizeof(gtoc_cmd),
		(void *)buffer,    req_size,
		CDRETRIES, 30000, NULL, flags);
	if (error)
		goto out;

	toc_hdr = (struct scsipi_toc_header *)     buffer;
	toc     = (struct scsipi_toc_formatted *) (buffer + 4);

	/* as in read disc info, all sessions are converted to tracks      */
	/* track 1..  -> offsets, sizes can be (rougly) estimated (16 ECC) */
	/* last track -> we got the size from the lead-out                 */

	tracknr      = 0;
	last_tracknr = toc_hdr->last;
	track_start  = 0;
	track_size   = 0;
	lead_out     = 0;
	flags        = 0;

	size = req_size - sizeof(struct scsipi_toc_header) + 1;
	while (size > 0) {
		/* remember, DVD-ROM: tracknr == sessionnr */
		lba     = _4btol(toc->msf_lba);
		tracknr = toc->tracknr;
		control = toc->adrcontrol & 0xf;

		if (trackinfo->tracknr == tracknr) {
			track_start = lba;
		}
		if (trackinfo->tracknr == tracknr+1) {
			track_size  = lba - track_start;
			track_size -= 16;	/* link block ? */
		}
		if (tracknr == 0xAA) {
			lead_out = lba;
		}

		if ((control & (3<<2)) == 4)		/* 01xxb */
			flags |= MMC_TRACKINFO_DATA;
		if ((control & (1<<2)) == 0) {		/* x0xxb */
			flags |= MMC_TRACKINFO_AUDIO;
			if (control & (1<<3))		/* 10xxb */
				flags |= MMC_TRACKINFO_AUDIO_4CHAN;
			if (control & 1)		/* xxx1b */
				flags |= MMC_TRACKINFO_PRE_EMPH;
		}

		toc++;
		size -= sizeof(struct scsipi_toc_formatted);
	}
	if (trackinfo->tracknr == last_tracknr) {
		track_size = lead_out - track_start;
	}

	/* fill in */
	/* trackinfo->tracknr preserved */
	trackinfo->sessionnr  = trackinfo->tracknr;
	trackinfo->track_mode = 0;	/* unknown */
	trackinfo->data_mode  = 8;	/* 2048 bytes mode1   */

	trackinfo->flags         = flags;
	trackinfo->track_start   = track_start;
	trackinfo->next_writable = 0;
	trackinfo->free_blocks   = 0;
	trackinfo->packet_size   = 16;	/* standard length 16 blocks ECC */
	trackinfo->track_size    = track_size;
	trackinfo->last_recorded = 0;

out:
	free(buffer, M_TEMP);
	return error;
}

static int
mmc_gettrackinfo(struct scsipi_periph *periph, 
		 struct mmc_trackinfo *trackinfo)
{
	struct scsipi_read_trackinfo      ti_cmd;
	struct scsipi_read_trackinfo_data ti;
	struct scsipi_get_configuration   gc_cmd;
	struct scsipi_get_conf_data       gc;
	int error, flags;
	int mmc_profile;

	/* set up SCSI call with track number from trackinfo.tracknr */
	flags = XS_CTL_DATA_IN | XS_CTL_SILENT;
	memset(&ti_cmd, 0, sizeof(ti_cmd));
	ti_cmd.opcode    = READ_TRACKINFO;
	ti_cmd.addr_type = READ_TRACKINFO_ADDR_TRACK;
	ti_cmd.data_len[1] = READ_TRACKINFO_RETURNSIZE;

	/* trackinfo.tracknr contains number of tracks to query */
	_lto4b(trackinfo->tracknr, ti_cmd.address);
	error = scsipi_command(periph,
		(void *)&ti_cmd, sizeof(ti_cmd),
		(void *)&ti,     READ_TRACKINFO_RETURNSIZE,
		CDRETRIES, 30000, NULL, flags);

	if (error) {
		/* trackinfo call failed, emulate for cd-rom/dvd-rom */
		/* first determine mmc profile */
		flags = XS_CTL_DATA_IN;
		memset(&gc_cmd, 0, sizeof(gc_cmd));
		gc_cmd.opcode = GET_CONFIGURATION;
		_lto2b(GET_CONF_NO_FEATURES_LEN, gc_cmd.data_len);

		error = scsipi_command(periph,
			(void *)&gc_cmd, sizeof(gc_cmd),
			(void *)&gc,     GET_CONF_NO_FEATURES_LEN,
			CDRETRIES, 30000, NULL, flags);
		if (error)
			return error;
		mmc_profile = _2btol(gc.mmc_profile);

		/* choose emulation */
		if (mmc_profile == 0x08) /* CD-ROM */
			return mmc_gettrackinfo_cdrom(periph, trackinfo);
		if (mmc_profile == 0x10) /* DVD-ROM */
			return mmc_gettrackinfo_dvdrom(periph, trackinfo);
		/* CD/DVD drive is violating specs */
		return EIO;
	}

	/* (re)initialise structure */
	memset(trackinfo, 0, sizeof(struct mmc_trackinfo));

	/* account for short returns screwing up track and session msb */
	if ((ti.data_len[1] | (ti.data_len[0] << 8)) <= 32) {
		ti.track_msb   = 0;
		ti.session_msb = 0;
	}

	trackinfo->tracknr    = ti.track_lsb   | (ti.track_msb   << 8);
	trackinfo->sessionnr  = ti.session_lsb | (ti.session_msb << 8);
	trackinfo->track_mode = ti.track_info_1 & 0xf;
	trackinfo->data_mode  = ti.track_info_2 & 0xf;

	flags = 0;
	if (ti.track_info_1 & 0x10)
		flags |= MMC_TRACKINFO_COPY;
	if (ti.track_info_1 & 0x20)
		flags |= MMC_TRACKINFO_DAMAGED;
	if (ti.track_info_2 & 0x10)
		flags |= MMC_TRACKINFO_FIXED_PACKET;
	if (ti.track_info_2 & 0x20)
		flags |= MMC_TRACKINFO_INCREMENTAL;
	if (ti.track_info_2 & 0x40)
		flags |= MMC_TRACKINFO_BLANK;
	if (ti.track_info_2 & 0x80)
		flags |= MMC_TRACKINFO_RESERVED;
	if (ti.data_valid   & 0x01)
		flags |= MMC_TRACKINFO_NWA_VALID;
	if (ti.data_valid   & 0x02)
		flags |= MMC_TRACKINFO_LRA_VALID;
	if ((trackinfo->track_mode & (3<<2)) == 4)		/* 01xxb */
		flags |= MMC_TRACKINFO_DATA;
	if ((trackinfo->track_mode & (1<<2)) == 0) {		/* x0xxb */
		flags |= MMC_TRACKINFO_AUDIO;
		if (trackinfo->track_mode & (1<<3))		/* 10xxb */
			flags |= MMC_TRACKINFO_AUDIO_4CHAN;
		if (trackinfo->track_mode & 1)			/* xxx1b */
			flags |= MMC_TRACKINFO_PRE_EMPH;
	}

	trackinfo->flags = flags;
	trackinfo->track_start    = _4btol(ti.track_start);
	trackinfo->next_writable  = _4btol(ti.next_writable);
	trackinfo->free_blocks    = _4btol(ti.free_blocks);
	trackinfo->packet_size    = _4btol(ti.packet_size);
	trackinfo->track_size     = _4btol(ti.track_size);
	trackinfo->last_recorded  = _4btol(ti.last_recorded);

	return 0;
}

static int
mmc_doclose(struct scsipi_periph *periph, int param, int func) {
	struct scsipi_close_tracksession close_cmd;
	int error, flags;

	/* set up SCSI call with track number */
	flags = XS_CTL_DATA_OUT;
	memset(&close_cmd, 0, sizeof(close_cmd));
	close_cmd.opcode    = CLOSE_TRACKSESSION;
	close_cmd.function  = func;
	_lto2b(param, close_cmd.tracksessionnr);

	error = scsipi_command(periph,
		(void *) &close_cmd, sizeof(close_cmd),
		NULL, 0,
		CDRETRIES, 120000, NULL, flags);

	return error;
}

static int
mmc_do_closetrack(struct scsipi_periph *periph, struct mmc_op *mmc_op)
{
	int mmc_profile = mmc_op->mmc_profile;

	switch (mmc_profile) {
	case 0x12 : /* DVD-RAM */
	case 0x1a : /* DVD+RW  */
	case 0x2a : /* DVD+RW Dual layer */
	case 0x42 : /* BD-R Ramdom Recording (RRM) */
	case 0x43 : /* BD-RE */
	case 0x52 : /* HD DVD-RW ; DVD-RAM like */
		return EINVAL;
	}

	return mmc_doclose(periph, mmc_op->tracknr, 1);
}

static int
mmc_do_close_or_finalise(struct scsipi_periph *periph, struct mmc_op *mmc_op)
{
	uint8_t blob[MS5LEN], *page5;
	int mmc_profile = mmc_op->mmc_profile;
	int func, close, flags;
	int error;

	close = (mmc_op->operation == MMC_OP_CLOSESESSION);

	switch (mmc_profile) {
	case 0x09 : /* CD-R       */
	case 0x0a : /* CD-RW      */
		/* Special case : need to update MS field in mode page 5 */
		memset(blob, 0, sizeof(blob));
		page5 = blob+8;

		flags = XS_CTL_DATA_IN;
		error = scsipi_mode_sense_big(periph, SMS_PF, 5,
		    (void *)blob, sizeof(blob), flags, CDRETRIES, 20000);
		if (error)
			return error;

		/* set multi session field when closing a session only */
		page5[3] &= 63;
		if (close)
			page5[3] |= 3 << 6;

		flags = XS_CTL_DATA_OUT;
		error = scsipi_mode_select_big(periph, SMS_PF,
		    (void *)blob, sizeof(blob), flags, CDRETRIES, 20000);
		if (error)
			return error;
		/* and use funtion 2 */
		func = 2;
		break;
	case 0x11 : /* DVD-R (DL) */
	case 0x13 : /* DVD-RW restricted overwrite */
	case 0x14 : /* DVD-RW sequential */
		func = close ? 2 : 3;
		break;
	case 0x1b : /* DVD+R   */
	case 0x2b : /* DVD+R Dual layer */
	case 0x51 : /* HD DVD-R   */
	case 0x41 : /* BD-R Sequential recording (SRM) */
		func = close ? 2 : 6;
		break;
	case 0x12 : /* DVD-RAM */
	case 0x1a : /* DVD+RW  */
	case 0x2a : /* DVD+RW Dual layer */
	case 0x42 : /* BD-R Ramdom Recording (RRM) */
	case 0x43 : /* BD-RE */
	case 0x52 : /* HD DVD-RW; DVD-RAM like */
		return EINVAL;
	default:
		printf("MMC close/finalise passed wrong device type! (%d)\n",
		    mmc_profile);
		return EINVAL;
	}

	return mmc_doclose(periph, mmc_op->sessionnr, func);
}

static int
mmc_do_reserve_track(struct scsipi_periph *periph, struct mmc_op *mmc_op)
{
	struct scsipi_reserve_track reserve_cmd;
	uint32_t extent;
	int error, flags;

	/* TODO make mmc safeguards? */
	extent = mmc_op->extent;
	/* TODO min/max support? */

	/* set up SCSI call with requested space */
	flags = XS_CTL_DATA_OUT;
	memset(&reserve_cmd, 0, sizeof(reserve_cmd));
	reserve_cmd.opcode = RESERVE_TRACK;
	_lto4b(extent, reserve_cmd.reservation_size);

	error = scsipi_command(periph,
		(void *) &reserve_cmd, sizeof(reserve_cmd),
		NULL, 0,
		CDRETRIES, 30000, NULL, flags);

	return error;
}

static int
mmc_do_reserve_track_nwa(struct scsipi_periph *periph, struct mmc_op *mmc_op)
{
	/* XXX assumes that NWA given is valid */
	switch (mmc_op->mmc_profile) {
	case 0x09 : /* CD-R       */
		/* XXX unknown boundary checks XXX */
		if (mmc_op->extent <= 152)
			return EINVAL;
		/* CD-R takes 152 sectors to close track */
		mmc_op->extent -= 152;
		return mmc_do_reserve_track(periph, mmc_op);
	case 0x11 : /* DVD-R (DL) */
	case 0x1b : /* DVD+R   */
	case 0x2b : /* DVD+R Dual layer */
		if (mmc_op->extent % 16)
			return EINVAL;
		/* upto one ECC block of 16 sectors lost */
		mmc_op->extent -= 16;
		return mmc_do_reserve_track(periph, mmc_op);
	case 0x41 : /* BD-R Sequential recording (SRM) */
	case 0x51 : /* HD DVD-R   */
		if (mmc_op->extent % 32)
			return EINVAL;
		/* one ECC block of 32 sectors lost (AFAIK) */
		mmc_op->extent -= 32;
		return mmc_do_reserve_track(periph, mmc_op);
	}

	/* unknown behaviour or invalid disc type */
	return EINVAL;
}

static int
mmc_do_repair_track(struct scsipi_periph *periph, struct mmc_op *mmc_op)
{
	struct scsipi_repair_track repair_cmd;
	int error, flags;

	/* TODO make mmc safeguards? */

	/* set up SCSI call with track number */
	flags = XS_CTL_DATA_OUT;
	memset(&repair_cmd, 0, sizeof(repair_cmd));
	repair_cmd.opcode = REPAIR_TRACK;
	_lto2b(mmc_op->tracknr, repair_cmd.tracknr);

	error = scsipi_command(periph,
		(void *) &repair_cmd, sizeof(repair_cmd),
		NULL, 0,
		CDRETRIES, 30000, NULL, flags);

	return error;
}

static int
mmc_do_op(struct scsipi_periph *periph, struct mmc_op *mmc_op)
{
	/* guard operation value */
	if (mmc_op->operation < 1 || mmc_op->operation > MMC_OP_MAX)
		return EINVAL;

	/* synchronise cache is special since it doesn't rely on mmc_profile */
	if (mmc_op->operation == MMC_OP_SYNCHRONISECACHE)
		return cdcachesync(periph, 0);

	/* zero mmc_profile means unknown disc so operations are not defined */
	if (mmc_op->mmc_profile == 0) {
#ifdef DEBUG
		printf("mmc_do_op called with mmc_profile = 0\n");
#endif
		return EINVAL;
	}

	/* do the operations */
	switch (mmc_op->operation) {
	case MMC_OP_CLOSETRACK   :
		return mmc_do_closetrack(periph, mmc_op);
	case MMC_OP_CLOSESESSION :
	case MMC_OP_FINALISEDISC :
		return mmc_do_close_or_finalise(periph, mmc_op);
	case MMC_OP_RESERVETRACK :
		return mmc_do_reserve_track(periph, mmc_op);
	case MMC_OP_RESERVETRACK_NWA :
		return mmc_do_reserve_track_nwa(periph, mmc_op);
	case MMC_OP_REPAIRTRACK  :
		return mmc_do_repair_track(periph, mmc_op);
	case MMC_OP_UNCLOSELASTSESSION :
		/* TODO unclose last session support */
		return EINVAL;
	default :
		printf("mmc_do_op: unhandled operation %d\n", mmc_op->operation);
	}

	return EINVAL;
}

static int
mmc_setup_writeparams(struct scsipi_periph *periph,
		      struct mmc_writeparams *mmc_writeparams)
{
	struct mmc_trackinfo trackinfo;
	uint8_t blob[MS5LEN];
	uint8_t *page5;
	int flags, error;
	int track_mode, data_mode;

	/* setup mode page 5 for CD only */
	if (mmc_writeparams->mmc_class != MMC_CLASS_CD)
		return 0;

	memset(blob, 0, sizeof(blob));
	page5 = blob+8;

	/* read mode page 5 (with header) */
	flags = XS_CTL_DATA_IN;
	error = scsipi_mode_sense_big(periph, SMS_PF, 5, (void *)blob,
	    sizeof(blob), flags, CDRETRIES, 20000);
	if (error)
		return error;

	/* set page length for reasurance */
	page5[1] = P5LEN;	/* page length */

	/* write type packet/incremental */
	page5[2] &= 0xf0;

	/* set specified mode parameters */
	track_mode = mmc_writeparams->track_mode;
	data_mode  = mmc_writeparams->data_mode;
	if (track_mode <= 0 || track_mode > 15)
		return EINVAL;
	if (data_mode < 1 || data_mode > 2)
		return EINVAL;

	/* if a tracknr is passed, setup according to the track */
	if (mmc_writeparams->tracknr > 0) {
		trackinfo.tracknr = mmc_writeparams->tracknr;
		error = mmc_gettrackinfo(periph, &trackinfo);
		if (error)
			return error;
		if ((trackinfo.flags & MMC_TRACKINFO_BLANK) == 0) {
			track_mode = trackinfo.track_mode;
			data_mode  = trackinfo.data_mode;
		}
		mmc_writeparams->blockingnr = trackinfo.packet_size;
	}

	/* copy track mode and data mode from trackinfo */
	page5[3] &= 16;		/* keep only `Copy' bit */
	page5[3] |= (3 << 6) | track_mode;
	page5[4] &= 0xf0;	/* wipe data block type */
	if (data_mode == 1) {
		/* select ISO mode 1 (CD only) */
		page5[4] |= 8;
		/* select session format normal disc (CD only) */
		page5[8] = 0;
	} else {
		/* select ISO mode 2; XA form 1 (CD only) */
		page5[4] |= 10;
		/* select session format CD-ROM XA disc (CD only) */
		page5[8] = 0x20;
	}
	if (mmc_writeparams->mmc_cur & MMC_CAP_SEQUENTIAL) {
		if (mmc_writeparams->mmc_cur & MMC_CAP_ZEROLINKBLK) {
			/* set BUFE buffer underrun protection */
			page5[2] |= 1<<6;
		}
		/* allow for multi session */
		page5[3] |= 3 << 6;
	} else {
		/* select fixed packets */
		page5[3] |= 1<<5;
		_lto4b(mmc_writeparams->blockingnr, &(page5[10]));
	}

	/* write out updated mode page 5 (with header) */
	flags = XS_CTL_DATA_OUT;
	error = scsipi_mode_select_big(periph, SMS_PF, (void *)blob,
	    sizeof(blob), flags, CDRETRIES, 20000);
	if (error)
		return error;

	return 0;
}

static void
cd_set_geometry(struct cd_softc *cd)
{
	struct disk_geom *dg = &cd->sc_dk.dk_geom;

	memset(dg, 0, sizeof(*dg));

	dg->dg_secperunit = cd->params.disksize;
	dg->dg_secsize = cd->params.blksize;

	disk_set_info(cd->sc_dev, &cd->sc_dk, NULL);
}
