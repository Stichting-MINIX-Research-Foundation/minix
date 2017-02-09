/*	$NetBSD: st.c,v 1.227 2015/08/24 23:13:15 pooka Exp $ */

/*-
 * Copyright (c) 1998, 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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
 * major changes by Julian Elischer (julian@jules.dialix.oz.au) May 1993
 *
 * A lot of rewhacking done by mjacob (mjacob@nas.nasa.gov).
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: st.c,v 1.227 2015/08/24 23:13:15 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_scsi.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/fcntl.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/proc.h>
#include <sys/mtio.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/iostat.h>
#include <sys/sysctl.h>

#include <dev/scsipi/scsi_spc.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsi_tape.h>
#include <dev/scsipi/scsipiconf.h>
#include <dev/scsipi/scsipi_base.h>
#include <dev/scsipi/stvar.h>

/* Defines for device specific stuff */
#define DEF_FIXED_BSIZE  512

#define STMODE(z)	( minor(z)       & 0x03)
#define STDSTY(z)	((minor(z) >> 2) & 0x03)
#define STUNIT(z)	((minor(z) >> 4)       )
#define STNMINOR	16

#define NORMAL_MODE	0
#define NOREW_MODE	1
#define EJECT_MODE	2
#define CTRL_MODE	3

#ifndef		ST_MOUNT_DELAY
#define		ST_MOUNT_DELAY		0
#endif

static dev_type_open(stopen);
static dev_type_close(stclose);
static dev_type_read(stread);
static dev_type_write(stwrite);
static dev_type_ioctl(stioctl);
static dev_type_strategy(ststrategy);
static dev_type_dump(stdump);

const struct bdevsw st_bdevsw = {
	.d_open = stopen,
	.d_close = stclose,
	.d_strategy = ststrategy,
	.d_ioctl = stioctl,
	.d_dump = stdump,
	.d_psize = nosize,
	.d_discard = nodiscard,
	.d_flag = D_TAPE
};

const struct cdevsw st_cdevsw = {
	.d_open = stopen,
	.d_close = stclose,
	.d_read = stread,
	.d_write = stwrite,
	.d_ioctl = stioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_TAPE
};

/*
 * Define various devices that we know mis-behave in some way,
 * and note how they are bad, so we can correct for them
 */

static const struct st_quirk_inquiry_pattern st_quirk_patterns[] = {
	{{T_SEQUENTIAL, T_REMOV,
	 "        ", "                ", "    "}, {0, 0, {
		{ST_Q_FORCE_BLKSIZE, 512, 0},		/* minor 0-3 */
		{ST_Q_FORCE_BLKSIZE, 512, QIC_24},	/* minor 4-7 */
		{ST_Q_FORCE_BLKSIZE, 0, HALFINCH_1600},	/* minor 8-11 */
		{ST_Q_FORCE_BLKSIZE, 0, HALFINCH_6250}	/* minor 12-15 */
	}}},
	{{T_SEQUENTIAL, T_REMOV,
	 "TANDBERG", " TDC 3600       ", ""},     {0, 12, {
		{0, 0, 0},				/* minor 0-3 */
		{ST_Q_FORCE_BLKSIZE, 0, QIC_525},	/* minor 4-7 */
		{0, 0, QIC_150},			/* minor 8-11 */
		{0, 0, QIC_120}				/* minor 12-15 */
	}}},
 	{{T_SEQUENTIAL, T_REMOV,
 	 "TANDBERG", " TDC 3800       ", ""},     {0, 0, {
		{ST_Q_FORCE_BLKSIZE, 512, 0},		/* minor 0-3 */
		{0, 0, QIC_525},			/* minor 4-7 */
		{0, 0, QIC_150},			/* minor 8-11 */
		{0, 0, QIC_120}				/* minor 12-15 */
	}}},
	{{T_SEQUENTIAL, T_REMOV,
	  "TANDBERG", " SLR5 4/8GB     ", ""},     {0, 0, {
		{ST_Q_FORCE_BLKSIZE, 1024, 0},		/* minor 0-3 */
		{0, 0, 0},				/* minor 4-7 */
		{0, 0, 0},				/* minor 8-11 */
		{0, 0, 0}				/* minor 12-15 */
	}}},
	/*
	 * lacking a manual for the 4200, it's not clear what the
	 * specific density codes should be- the device is a 2.5GB
	 * capable QIC drive, those density codes aren't readily
	 * availabel. The 'default' will just have to do.
	 */
 	{{T_SEQUENTIAL, T_REMOV,
 	 "TANDBERG", " TDC 4200       ", ""},     {0, 0, {
		{ST_Q_FORCE_BLKSIZE, 512, 0},		/* minor 0-3 */
		{0, 0, QIC_525},			/* minor 4-7 */
		{0, 0, QIC_150},			/* minor 8-11 */
		{0, 0, QIC_120}				/* minor 12-15 */
	}}},
	/*
	 * At least -005 and -007 need this.  I'll assume they all do unless I
	 * hear otherwise.  - mycroft, 31MAR1994
	 */
	{{T_SEQUENTIAL, T_REMOV,
	 "ARCHIVE ", "VIPER 2525 25462", ""},     {0, 0, {
		{ST_Q_SENSE_HELP, 0, 0},		/* minor 0-3 */
		{ST_Q_SENSE_HELP, 0, QIC_525},		/* minor 4-7 */
		{0, 0, QIC_150},			/* minor 8-11 */
		{0, 0, QIC_120}				/* minor 12-15 */
	}}},
	/*
	 * One user reports that this works for his tape drive.  It probably
	 * needs more work.  - mycroft, 09APR1994
	 */
	{{T_SEQUENTIAL, T_REMOV,
	 "SANKYO  ", "CP525           ", ""},    {0, 0, {
		{ST_Q_FORCE_BLKSIZE, 512, 0},		/* minor 0-3 */
		{ST_Q_FORCE_BLKSIZE, 512, QIC_525},	/* minor 4-7 */
		{0, 0, QIC_150},			/* minor 8-11 */
		{0, 0, QIC_120}				/* minor 12-15 */
	}}},
	{{T_SEQUENTIAL, T_REMOV,
	 "ANRITSU ", "DMT780          ", ""},     {0, 0, {
		{ST_Q_FORCE_BLKSIZE, 512, 0},		/* minor 0-3 */
		{ST_Q_FORCE_BLKSIZE, 512, QIC_525},	/* minor 4-7 */
		{0, 0, QIC_150},			/* minor 8-11 */
		{0, 0, QIC_120}				/* minor 12-15 */
	}}},
	{{T_SEQUENTIAL, T_REMOV,
	 "ARCHIVE ", "VIPER 150  21247", ""},     {ST_Q_ERASE_NOIMM, 12, {
		{ST_Q_SENSE_HELP, 0, 0},		/* minor 0-3 */
		{0, 0, QIC_150},			/* minor 4-7 */
		{0, 0, QIC_120},			/* minor 8-11 */
		{0, 0, QIC_24}				/* minor 12-15 */
	}}},
	{{T_SEQUENTIAL, T_REMOV,
	 "ARCHIVE ", "VIPER 150  21531", ""},     {ST_Q_ERASE_NOIMM, 12, {
		{ST_Q_SENSE_HELP, 0, 0},		/* minor 0-3 */
		{0, 0, QIC_150},			/* minor 4-7 */
		{0, 0, QIC_120},			/* minor 8-11 */
		{0, 0, QIC_24}				/* minor 12-15 */
	}}},
	{{T_SEQUENTIAL, T_REMOV,
	 "WANGTEK ", "5099ES SCSI", ""},          {0, 0, {
		{ST_Q_FORCE_BLKSIZE, 512, 0},		/* minor 0-3 */
		{0, 0, QIC_11},				/* minor 4-7 */
		{0, 0, QIC_24},				/* minor 8-11 */
		{0, 0, QIC_24}				/* minor 12-15 */
	}}},
	{{T_SEQUENTIAL, T_REMOV,
	 "WANGTEK ", "5150ES SCSI", ""},          {0, 0, {
		{ST_Q_FORCE_BLKSIZE, 512, 0},		/* minor 0-3 */
		{0, 0, QIC_24},				/* minor 4-7 */
		{0, 0, QIC_120},			/* minor 8-11 */
		{0, 0, QIC_150}				/* minor 12-15 */
	}}},
	{{T_SEQUENTIAL, T_REMOV,
	 "WANGTEK ", "5525ES SCSI REV7", ""},     {0, 0, {
		{0, 0, 0},				/* minor 0-3 */
		{ST_Q_BLKSIZE, 0, QIC_525},		/* minor 4-7 */
		{0, 0, QIC_150},			/* minor 8-11 */
		{0, 0, QIC_120}				/* minor 12-15 */
	}}},
	{{T_SEQUENTIAL, T_REMOV,
	 "WangDAT ", "Model 1300      ", ""},     {0, 0, {
		{0, 0, 0},				/* minor 0-3 */
		{ST_Q_FORCE_BLKSIZE, 512, DDS},		/* minor 4-7 */
		{ST_Q_FORCE_BLKSIZE, 1024, DDS},	/* minor 8-11 */
		{ST_Q_FORCE_BLKSIZE, 0, DDS}		/* minor 12-15 */
	}}},
	{{T_SEQUENTIAL, T_REMOV,
	 "EXABYTE ", "EXB-8200        ", "263H"}, {0, 5, {
		{0, 0, 0},				/* minor 0-3 */
		{0, 0, 0},				/* minor 4-7 */
		{0, 0, 0},				/* minor 8-11 */
		{0, 0, 0}				/* minor 12-15 */
	}}},
	{{T_SEQUENTIAL, T_REMOV,
	 "STK",      "9490",             ""},
				{ST_Q_FORCE_BLKSIZE, 0, {
		{0, 0, 0},				/* minor 0-3 */
		{0, 0, 0},				/* minor 4-7 */
		{0, 0, 0},				/* minor 8-11 */
		{0, 0, 0}				/* minor 12-15 */
	}}},
	{{T_SEQUENTIAL, T_REMOV,
	 "STK",      "SD-3",             ""},
				{ST_Q_FORCE_BLKSIZE, 0, {
		{0, 0, 0},				/* minor 0-3 */
		{0, 0, 0},				/* minor 4-7 */
		{0, 0, 0},				/* minor 8-11 */
		{0, 0, 0}				/* minor 12-15 */
	}}},
	{{T_SEQUENTIAL, T_REMOV,
	 "IBM",      "03590",            ""},     {ST_Q_IGNORE_LOADS, 0, {
		{0, 0, 0},				/* minor 0-3 */
		{0, 0, 0},				/* minor 4-7 */
		{0, 0, 0},				/* minor 8-11 */
		{0, 0, 0}				/* minor 12-15 */
	}}},
	{{T_SEQUENTIAL, T_REMOV,
	 "HP      ", "T4000s          ", ""},     {ST_Q_UNIMODAL, 0, {
		{0, 0, QIC_3095},			/* minor 0-3 */
		{0, 0, QIC_3095},			/* minor 4-7 */
		{0, 0, QIC_3095},			/* minor 8-11 */
		{0, 0, QIC_3095},			/* minor 12-15 */
	}}},
#if 0
	{{T_SEQUENTIAL, T_REMOV,
	 "EXABYTE ", "EXB-8200        ", ""},     {0, 12, {
		{0, 0, 0},				/* minor 0-3 */
		{0, 0, 0},				/* minor 4-7 */
		{0, 0, 0},				/* minor 8-11 */
		{0, 0, 0}				/* minor 12-15 */
	}}},
#endif
	{{T_SEQUENTIAL, T_REMOV,
	 "TEAC    ", "MT-2ST/N50      ", ""},     {ST_Q_IGNORE_LOADS, 0, {
		{0, 0, 0},			        /* minor 0-3 */
		{0, 0, 0},			        /* minor 4-7 */
		{0, 0, 0},			        /* minor 8-11 */
		{0, 0, 0}			        /* minor 12-15 */
	}}},
	{{T_SEQUENTIAL, T_REMOV,
	 "OnStream", "ADR50 Drive", ""},	  {ST_Q_UNIMODAL, 0, {
		{ST_Q_FORCE_BLKSIZE, 512, 0},	        /* minor 0-3 */
		{ST_Q_FORCE_BLKSIZE, 512, 0},	        /* minor 4-7 */
		{ST_Q_FORCE_BLKSIZE, 512, 0},	        /* minor 8-11 */
		{ST_Q_FORCE_BLKSIZE, 512, 0},	        /* minor 12-15 */
	}}},
	{{T_SEQUENTIAL, T_REMOV,
	 "OnStream DI-30",      "",   "1.0"},  {ST_Q_NOFILEMARKS, 0, {
		{0, 0, 0},                              /* minor 0-3 */
		{0, 0, 0},                              /* minor 4-7 */
		{0, 0, 0},                              /* minor 8-11 */
		{0, 0, 0}                               /* minor 12-15 */
	}}},
	{{T_SEQUENTIAL, T_REMOV,
	 "NCR H621", "0-STD-03-46F880 ", ""},     {ST_Q_NOPREVENT, 0, {
		{0, 0, 0},			       /* minor 0-3 */
		{0, 0, 0},			       /* minor 4-7 */
		{0, 0, 0},			       /* minor 8-11 */
		{0, 0, 0}			       /* minor 12-15 */
	}}},
	{{T_SEQUENTIAL, T_REMOV,
	 "Seagate STT3401A", "hp0atxa", ""},	{0, 0, {
		{ST_Q_FORCE_BLKSIZE, 512, 0},		/* minor 0-3 */
		{ST_Q_FORCE_BLKSIZE, 1024, 0},		/* minor 4-7 */
		{ST_Q_FORCE_BLKSIZE, 512, 0},		/* minor 8-11 */
		{ST_Q_FORCE_BLKSIZE, 512, 0}		/* minor 12-15 */
	}}},
};

#define NOEJECT 0
#define EJECT 1

static void	st_identify_drive(struct st_softc *,
		    struct scsipi_inquiry_pattern *);
static void	st_loadquirks(struct st_softc *);
static int	st_mount_tape(dev_t, int);
static void	st_unmount(struct st_softc *, boolean);
static int	st_decide_mode(struct st_softc *, boolean);
static void	ststart(struct scsipi_periph *);
static void	strestart(void *);
static void	stdone(struct scsipi_xfer *, int);
static int	st_read(struct st_softc *, char *, int, int);
static int	st_space(struct st_softc *, int, u_int, int);
static int	st_write_filemarks(struct st_softc *, int, int);
static int	st_check_eod(struct st_softc *, boolean, int *, int);
static int	st_load(struct st_softc *, u_int, int);
static int	st_rewind(struct st_softc *, u_int, int);
static int	st_interpret_sense(struct scsipi_xfer *);
static int	st_touch_tape(struct st_softc *);
static int	st_erase(struct st_softc *, int full, int flags);
static int	st_rdpos(struct st_softc *, int, uint32_t *);
static int	st_setpos(struct st_softc *, int, uint32_t *);

static const struct scsipi_periphsw st_switch = {
	st_interpret_sense,
	ststart,
	NULL,
	stdone
};

#if defined(ST_ENABLE_EARLYWARN)
#define	ST_INIT_FLAGS	ST_EARLYWARN
#else
#define	ST_INIT_FLAGS	0
#endif

/*
 * The routine called by the low level scsi routine when it discovers
 * A device suitable for this driver
 */
void
stattach(device_t parent, device_t self, void *aux)
{
	struct st_softc *st = device_private(self);
	struct scsipibus_attach_args *sa = aux;
	struct scsipi_periph *periph = sa->sa_periph;

	SC_DEBUG(periph, SCSIPI_DB2, ("stattach: "));
	st->sc_dev = self;

	/* Store information needed to contact our base driver */
	st->sc_periph = periph;
	periph->periph_dev = st->sc_dev;
	periph->periph_switch = &st_switch;

	/* Set initial flags  */
	st->flags = ST_INIT_FLAGS;

	/* Set up the buf queue for this device */
	bufq_alloc(&st->buf_queue, "fcfs", 0);
	callout_init(&st->sc_callout, 0);

	/*
	 * Check if the drive is a known criminal and take
	 * Any steps needed to bring it into line
	 */
	st_identify_drive(st, &sa->sa_inqbuf);
	printf("\n");
	/* Use the subdriver to request information regarding the drive.  */
	printf("%s : %s", device_xname(st->sc_dev), st->quirkdata
	    ? "quirks apply, " : "");
	if (scsipi_test_unit_ready(periph,
	    XS_CTL_DISCOVERY | XS_CTL_SILENT | XS_CTL_IGNORE_MEDIA_CHANGE) ||
	    st->ops(st, ST_OPS_MODESENSE,
	    XS_CTL_DISCOVERY | XS_CTL_SILENT | XS_CTL_IGNORE_MEDIA_CHANGE))
		printf("drive empty\n");
	else {
		printf("density code %d, ", st->media_density);
		if (st->media_blksize > 0)
			printf("%d-byte", st->media_blksize);
		else
			printf("variable");
		printf(" blocks, write-%s\n",
		    (st->flags & ST_READONLY) ? "protected" : "enabled");
	}

	st->stats = iostat_alloc(IOSTAT_TAPE, parent,
	    device_xname(st->sc_dev));

	rnd_attach_source(&st->rnd_source, device_xname(st->sc_dev),
	    RND_TYPE_TAPE, RND_FLAG_DEFAULT);
}

int
stdetach(device_t self, int flags)
{
	struct st_softc *st = device_private(self);
	int s, bmaj, cmaj, mn;

	/* locate the major number */
	bmaj = bdevsw_lookup_major(&st_bdevsw);
	cmaj = cdevsw_lookup_major(&st_cdevsw);

	/* kill any pending restart */
	callout_stop(&st->sc_callout);

	s = splbio();

	/* Kill off any queued buffers. */
	bufq_drain(st->buf_queue);

	bufq_free(st->buf_queue);

	/* Kill off any pending commands. */
	scsipi_kill_pending(st->sc_periph);

	splx(s);

	/* Nuke the vnodes for any open instances */
	mn = STUNIT(device_unit(self));
	vdevgone(bmaj, mn, mn+STNMINOR-1, VBLK);
	vdevgone(cmaj, mn, mn+STNMINOR-1, VCHR);

	iostat_free(st->stats);

	/* Unhook the entropy source. */
	rnd_detach_source(&st->rnd_source);

	return 0;
}

/*
 * Use the inquiry routine in 'scsi_base' to get drive info so we can
 * Further tailor our behaviour.
 */
static void
st_identify_drive(struct st_softc *st, struct scsipi_inquiry_pattern *inqbuf)
{
	const struct st_quirk_inquiry_pattern *finger;
	int priority;

	finger = scsipi_inqmatch(inqbuf,
	    st_quirk_patterns,
	    sizeof(st_quirk_patterns) / sizeof(st_quirk_patterns[0]),
	    sizeof(st_quirk_patterns[0]), &priority);
	if (priority != 0) {
		st->quirkdata = &finger->quirkdata;
		st->drive_quirks = finger->quirkdata.quirks;
		st->quirks = finger->quirkdata.quirks;	/* start value */
		st->page_0_size = finger->quirkdata.page_0_size;
		KASSERT(st->page_0_size <= MAX_PAGE_0_SIZE);
		st_loadquirks(st);
	}
}

/*
 * initialise the subdevices to the default (QUIRK) state.
 * this will remove any setting made by the system operator or previous
 * operations.
 */
static void
st_loadquirks(struct st_softc *st)
{
	const struct	modes *mode;
	struct	modes *mode2;
	int i;

	mode = st->quirkdata->modes;
	mode2 = st->modes;
	for (i = 0; i < 4; i++) {
		memset(mode2, 0, sizeof(struct modes));
		st->modeflags[i] &= ~(BLKSIZE_SET_BY_QUIRK |
		    DENSITY_SET_BY_QUIRK | BLKSIZE_SET_BY_USER |
		    DENSITY_SET_BY_USER);
		if ((mode->quirks | st->drive_quirks) & ST_Q_FORCE_BLKSIZE) {
			mode2->blksize = mode->blksize;
			st->modeflags[i] |= BLKSIZE_SET_BY_QUIRK;
		}
		if (mode->density) {
			mode2->density = mode->density;
			st->modeflags[i] |= DENSITY_SET_BY_QUIRK;
		}
		mode2->quirks |= mode->quirks;
		mode++;
		mode2++;
	}
}

/* open the device. */
static int
stopen(dev_t dev, int flags, int mode, struct lwp *l)
{
	u_int stmode, dsty;
	int error, sflags, unit, tries, ntries;
	struct st_softc *st;
	struct scsipi_periph *periph;
	struct scsipi_adapter *adapt;

	unit = STUNIT(dev);
	st = device_lookup_private(&st_cd, unit);
	if (st == NULL)
		return ENXIO;

	stmode = STMODE(dev);
	dsty = STDSTY(dev);

	periph = st->sc_periph;
	adapt = periph->periph_channel->chan_adapter;

	SC_DEBUG(periph, SCSIPI_DB1,
	    ("open: dev=0x%"PRIx64" (unit %d (of %d))\n", dev, unit,
	    st_cd.cd_ndevs));

	/* Only allow one at a time */
	if (periph->periph_flags & PERIPH_OPEN) {
		aprint_error_dev(st->sc_dev, "already open\n");
		return EBUSY;
	}

	if ((error = scsipi_adapter_addref(adapt)) != 0)
		return error;

	/* clear any latched errors. */
	st->mt_resid = 0;
	st->mt_erreg = 0;
	st->asc = 0;
	st->ascq = 0;

	/*
	 * Catch any unit attention errors. Be silent about this
	 * unless we're already mounted. We ignore media change
	 * if we're in control mode or not mounted yet.
	 */
	if ((st->flags & ST_MOUNTED) == 0 || stmode == CTRL_MODE) {
#ifdef SCSIDEBUG
		sflags = XS_CTL_IGNORE_MEDIA_CHANGE;
#else
		sflags = XS_CTL_SILENT|XS_CTL_IGNORE_MEDIA_CHANGE;
#endif
	} else
		sflags = 0;

	/*
	 * If we're already mounted or we aren't configured for
	 * a mount delay, only try a test unit ready once. Otherwise,
	 * try up to ST_MOUNT_DELAY times with a rest interval of
	 * one second between each try.
	 */
	if ((st->flags & ST_MOUNTED) || ST_MOUNT_DELAY == 0)
		ntries = 1;
	else
		ntries = ST_MOUNT_DELAY;

	for (error = tries = 0; tries < ntries; tries++) {
		int slpintr, oflags;

		/*
		 * If we had no error, or we're opening the control mode
		 * device, we jump out right away.
		 */
		error = scsipi_test_unit_ready(periph, sflags);
		if (error == 0 || stmode == CTRL_MODE)
			break;

		/*
		 * We had an error.
		 *
		 * If we're already mounted or we aren't configured for
		 * a mount delay, or the error isn't a NOT READY error,
		 * skip to the error exit now.
		 */
		if ((st->flags & ST_MOUNTED) || ST_MOUNT_DELAY == 0 ||
		    (st->mt_key != SKEY_NOT_READY)) {
			goto bad;
		}

		/* clear any latched errors. */
		st->mt_resid = 0;
		st->mt_erreg = 0;
		st->asc = 0;
		st->ascq = 0;

		/*
		 * Fake that we have the device open so
		 * we block other apps from getting in.
		 */
		oflags = periph->periph_flags;
		periph->periph_flags |= PERIPH_OPEN;

		slpintr = kpause("stload", true, hz, NULL);

		periph->periph_flags = oflags;	/* restore flags */
		if (slpintr != 0 && slpintr != EWOULDBLOCK) {
			goto bad;
		}
	}


	/*
	 * If the mode is 3 (e.g. minor = 3,7,11,15) then the device has
	 * been opened to set defaults and perform other, usually non-I/O
	 * related, operations. In this case, do a quick check to see
	 * whether the unit actually had a tape loaded (this will be known
	 * as to whether or not we got a NOT READY for the above
	 * unit attention). If a tape is there, go do a mount sequence.
	 */
	if (stmode == CTRL_MODE && st->mt_key == SKEY_NOT_READY) {
		periph->periph_flags |= PERIPH_OPEN;
		return 0;
	}

	/*
	 * If we get this far and had an error set, that means we failed
	 * to pass the 'test unit ready' test for the non-controlmode device,
	 * so we bounce the open.
	 */
	if (error)
		return error;

	/* Else, we're now committed to saying we're open. */
	periph->periph_flags |= PERIPH_OPEN; /* unit attn are now errors */

	/*
	 * If it's a different mode, or if the media has been
	 * invalidated, unmount the tape from the previous
	 * session but continue with open processing
	 */
	if (st->last_dsty != dsty ||
	    (periph->periph_flags & PERIPH_MEDIA_LOADED) == 0)
		st_unmount(st, NOEJECT);

	/*
	 * If we are not mounted, then we should start a new
	 * mount session.
	 */
	if (!(st->flags & ST_MOUNTED)) {
		if ((error = st_mount_tape(dev, flags)) != 0)
			goto bad;
		st->last_dsty = dsty;
	}
	if (!(st->quirks & ST_Q_NOPREVENT)) {
		scsipi_prevent(periph, SPAMR_PREVENT_DT,
		    XS_CTL_IGNORE_ILLEGAL_REQUEST | XS_CTL_IGNORE_NOT_READY);
	}

	SC_DEBUG(periph, SCSIPI_DB2, ("open complete\n"));
	return 0;

bad:
	st_unmount(st, NOEJECT);
	scsipi_adapter_delref(adapt);
	periph->periph_flags &= ~PERIPH_OPEN;
	return error;
}

static int
stclose(dev_t dev, int flags, int mode, struct lwp *l)
{
	int stxx, error = 0;
	struct st_softc *st = device_lookup_private(&st_cd, STUNIT(dev));
	struct scsipi_periph *periph = st->sc_periph;
	struct scsipi_adapter *adapt = periph->periph_channel->chan_adapter;

	SC_DEBUG(st->sc_periph, SCSIPI_DB1, ("closing\n"));

	/*
	 * Make sure that a tape opened in write-only mode will have
	 * file marks written on it when closed, even if not written to.
	 *
	 * This is for SUN compatibility. Actually, the Sun way of
	 * things is to:
	 *
	 *	only write filemarks if there are fmks to be written and
	 *   		- open for write (possibly read/write)
	 *		- the last operation was a write
	 * 	or:
	 *		- opened for wronly
	 *		- no data was written (including filemarks)
	 */

	stxx = st->flags & (ST_WRITTEN | ST_FM_WRITTEN);
	if (((flags & FWRITE) && stxx == ST_WRITTEN) ||
	    ((flags & O_ACCMODE) == FWRITE && stxx == 0)) {
		int nm;
		error = st_check_eod(st, FALSE, &nm, 0);
	}

	/* Allow robots to eject tape if needed.  */
	scsipi_prevent(periph, SPAMR_ALLOW,
	    XS_CTL_IGNORE_ILLEGAL_REQUEST | XS_CTL_IGNORE_NOT_READY);

	switch (STMODE(dev)) {
	case NORMAL_MODE:
		st_unmount(st, NOEJECT);
		break;
	case NOREW_MODE:
	case CTRL_MODE:
		/*
		 * Leave mounted unless media seems to have been removed.
		 *
		 * Otherwise, if we're to terminate a tape with more than one
		 * filemark [ and because we're not rewinding here ], backspace
		 * one filemark so that later appends will see an unbroken
		 * sequence of:
		 *
		 *	file - FMK - file - FMK ... file - FMK FMK (EOM)
		 */
		if ((periph->periph_flags & PERIPH_MEDIA_LOADED) == 0) {
			st_unmount(st, NOEJECT);
		} else if (error == 0) {
			/*
			 * ST_WRITTEN was preserved from above.
			 *
			 * All we need to know here is:
			 *
			 *	Were we writing this tape and was the last
			 *	operation a write?
			 *
			 *	Are there supposed to be 2FM at EOD?
			 *
			 * If both statements are true, then we backspace
			 * one filemark.
			 */
			stxx |= (st->flags & ST_2FM_AT_EOD);
			if ((flags & FWRITE) != 0 &&
			    (stxx == (ST_2FM_AT_EOD|ST_WRITTEN))) {
				error = st_space(st, -1, SP_FILEMARKS, 0);
			}
		}
		break;
	case EJECT_MODE:
		st_unmount(st, EJECT);
		break;
	}

	scsipi_wait_drain(periph);

	scsipi_adapter_delref(adapt);
	periph->periph_flags &= ~PERIPH_OPEN;

	return error;
}

/*
 * Start a new mount session.
 * Copy in all the default parameters from the selected device mode.
 * and try guess any that seem to be defaulted.
 */
static int
st_mount_tape(dev_t dev, int flags)
{
	int unit;
	u_int dsty;
	struct st_softc *st;
	struct scsipi_periph *periph;
	int error = 0;

	unit = STUNIT(dev);
	dsty = STDSTY(dev);
	st = device_lookup_private(&st_cd, unit);
	periph = st->sc_periph;

	if (st->flags & ST_MOUNTED)
		return 0;

	SC_DEBUG(periph, SCSIPI_DB1, ("mounting\n "));
	st->flags |= ST_NEW_MOUNT;
	st->quirks = st->drive_quirks | st->modes[dsty].quirks;
	/*
	 * If the media is new, then make sure we give it a chance to
	 * to do a 'load' instruction.  (We assume it is new.)
	 */
	if ((error = st_load(st, LD_LOAD, XS_CTL_SILENT)) != 0)
		return error;
	/*
	 * Throw another dummy instruction to catch
	 * 'Unit attention' errors. Many drives give
	 * these after doing a Load instruction (with
	 * the MEDIUM MAY HAVE CHANGED asc/ascq).
	 */
	scsipi_test_unit_ready(periph, XS_CTL_SILENT);	/* XXX */

	/*
	 * Some devices can't tell you much until they have been
	 * asked to look at the media. This quirk does this.
	 */
	if (st->quirks & ST_Q_SENSE_HELP)
		if ((error = st_touch_tape(st)) != 0)
			return error;
	/*
	 * Load the physical device parameters
	 * loads: blkmin, blkmax
	 */
	if ((error = st->ops(st, ST_OPS_RBL, 0)) != 0)
		return error;
	/*
	 * Load the media dependent parameters
	 * includes: media_blksize,media_density,numblks
	 * As we have a tape in, it should be reflected here.
	 * If not you may need the "quirk" above.
	 */
	if ((error = st->ops(st, ST_OPS_MODESENSE, 0)) != 0)
		return error;
	/*
	 * If we have gained a permanent density from somewhere,
	 * then use it in preference to the one supplied by
	 * default by the driver.
	 */
	if (st->modeflags[dsty] & (DENSITY_SET_BY_QUIRK | DENSITY_SET_BY_USER))
		st->density = st->modes[dsty].density;
	else
		st->density = st->media_density;
	/*
	 * If we have gained a permanent blocksize
	 * then use it in preference to the one supplied by
	 * default by the driver.
	 */
	st->flags &= ~ST_FIXEDBLOCKS;
	if (st->modeflags[dsty] &
	    (BLKSIZE_SET_BY_QUIRK | BLKSIZE_SET_BY_USER)) {
		st->blksize = st->modes[dsty].blksize;
		if (st->blksize)
			st->flags |= ST_FIXEDBLOCKS;
	} else {
		if ((error = st_decide_mode(st, FALSE)) != 0)
			return error;
	}
	if ((error = st->ops(st, ST_OPS_MODESELECT, 0)) != 0) {
		/* ATAPI will return ENODEV for this, and this may be OK */
		if (error != ENODEV) {
			aprint_error_dev(st->sc_dev,
			    "cannot set selected mode\n");
			return error;
		}
	}
	st->flags &= ~ST_NEW_MOUNT;
	st->flags |= ST_MOUNTED;
	periph->periph_flags |= PERIPH_MEDIA_LOADED;	/* move earlier? */
	st->blkno = st->fileno = (daddr_t) 0;
	return 0;
}

/*
 * End the present mount session.
 * Rewind, and optionally eject the tape.
 * Reset various flags to indicate that all new
 * operations require another mount operation
 */
static void
st_unmount(struct st_softc *st, boolean eject)
{
	struct scsipi_periph *periph = st->sc_periph;
	int nmarks;

	if ((st->flags & ST_MOUNTED) == 0)
		return;
	SC_DEBUG(periph, SCSIPI_DB1, ("unmounting\n"));
	st_check_eod(st, FALSE, &nmarks, XS_CTL_IGNORE_NOT_READY);
	st_rewind(st, 0, XS_CTL_IGNORE_NOT_READY);

	/*
	 * Section 9.3.3 of the SCSI specs states that a device shall return
	 * the density value specified in the last succesfull MODE SELECT
	 * after an unload operation, in case it is not able to
	 * automatically determine the density of the new medium.
	 *
	 * So we instruct the device to use the default density, which will
	 * prevent the use of stale density values (in particular,
	 * in st_touch_tape().
	 */
	st->density = 0;
	if (st->ops(st, ST_OPS_MODESELECT, 0) != 0) {
		aprint_error_dev(st->sc_dev,
		    "WARNING: cannot revert to default density\n");
	}

	if (eject) {
		if (!(st->quirks & ST_Q_NOPREVENT)) {
			scsipi_prevent(periph, SPAMR_ALLOW,
			    XS_CTL_IGNORE_ILLEGAL_REQUEST |
			    XS_CTL_IGNORE_NOT_READY);
		}
		st_load(st, LD_UNLOAD, XS_CTL_IGNORE_NOT_READY);
		st->blkno = st->fileno = (daddr_t) -1;
	} else {
		st->blkno = st->fileno = (daddr_t) 0;
	}
	st->flags &= ~(ST_MOUNTED | ST_NEW_MOUNT);
	periph->periph_flags &= ~PERIPH_MEDIA_LOADED;
}

/*
 * Given all we know about the device, media, mode, 'quirks' and
 * initial operation, make a decision as to how we should be set
 * to run (regarding blocking and EOD marks)
 */
int
st_decide_mode(struct st_softc *st, boolean first_read)
{

	SC_DEBUG(st->sc_periph, SCSIPI_DB2, ("starting block mode decision\n"));

	/*
	 * If the drive can only handle fixed-length blocks and only at
	 * one size, perhaps we should just do that.
	 */
	if (st->blkmin && (st->blkmin == st->blkmax)) {
		st->flags |= ST_FIXEDBLOCKS;
		st->blksize = st->blkmin;
		SC_DEBUG(st->sc_periph, SCSIPI_DB3,
		    ("blkmin == blkmax of %d\n", st->blkmin));
		goto done;
	}
	/*
	 * If the tape density mandates (or even suggests) use of fixed
	 * or variable-length blocks, comply.
	 */
	switch (st->density) {
	case HALFINCH_800:
	case HALFINCH_1600:
	case HALFINCH_6250:
	case DDS:
		st->flags &= ~ST_FIXEDBLOCKS;
		st->blksize = 0;
		SC_DEBUG(st->sc_periph, SCSIPI_DB3,
		    ("density specified variable\n"));
		goto done;
	case QIC_11:
	case QIC_24:
	case QIC_120:
	case QIC_150:
	case QIC_525:
	case QIC_1320:
	case QIC_3095:
	case QIC_3220:
		st->flags |= ST_FIXEDBLOCKS;
		if (st->media_blksize > 0)
			st->blksize = st->media_blksize;
		else
			st->blksize = DEF_FIXED_BSIZE;
		SC_DEBUG(st->sc_periph, SCSIPI_DB3,
		    ("density specified fixed\n"));
		goto done;
	}
	/*
	 * If we're about to read the tape, perhaps we should choose
	 * fixed or variable-length blocks and block size according to
	 * what the drive found on the tape.
	 */
	if (first_read &&
	    (!(st->quirks & ST_Q_BLKSIZE) || (st->media_blksize == 0) ||
	    (st->media_blksize == DEF_FIXED_BSIZE) ||
	    (st->media_blksize == 1024))) {
		if (st->media_blksize > 0)
			st->flags |= ST_FIXEDBLOCKS;
		else
			st->flags &= ~ST_FIXEDBLOCKS;
		st->blksize = st->media_blksize;
		SC_DEBUG(st->sc_periph, SCSIPI_DB3,
		    ("Used media_blksize of %d\n", st->media_blksize));
		goto done;
	}
	/*
	 * We're getting no hints from any direction.  Choose variable-
	 * length blocks arbitrarily.
	 */
	st->flags &= ~ST_FIXEDBLOCKS;
	st->blksize = 0;
	SC_DEBUG(st->sc_periph, SCSIPI_DB3,
	    ("Give up and default to variable mode\n"));

done:
	/*
	 * Decide whether or not to write two file marks to signify end-
	 * of-data.  Make the decision as a function of density.  If
	 * the decision is not to use a second file mark, the SCSI BLANK
	 * CHECK condition code will be recognized as end-of-data when
	 * first read.
	 * (I think this should be a by-product of fixed/variable..julian)
	 */
	switch (st->density) {
/*      case 8 mm:   What is the SCSI density code for 8 mm, anyway? */
	case QIC_11:
	case QIC_24:
	case QIC_120:
	case QIC_150:
	case QIC_525:
	case QIC_1320:
	case QIC_3095:
	case QIC_3220:
		st->flags &= ~ST_2FM_AT_EOD;
		break;
	default:
		st->flags |= ST_2FM_AT_EOD;
	}
	return 0;
}

/*
 * Actually translate the requested transfer into
 * one the physical driver can understand
 * The transfer is described by a buf and will include
 * only one physical transfer.
 */
static void
ststrategy(struct buf *bp)
{
	struct st_softc *st = device_lookup_private(&st_cd, STUNIT(bp->b_dev));
	int s;

	SC_DEBUG(st->sc_periph, SCSIPI_DB1,
	    ("ststrategy %d bytes @ blk %" PRId64 "\n", bp->b_bcount,
	        bp->b_blkno));
	/* If it's a null transfer, return immediately */
	if (bp->b_bcount == 0)
		goto abort;

	/* If offset is negative, error */
	if (bp->b_blkno < 0) {
		bp->b_error = EINVAL;
		goto abort;
	}

	/* Odd sized request on fixed drives are verboten */
	if (st->flags & ST_FIXEDBLOCKS) {
		if (bp->b_bcount % st->blksize) {
			aprint_error_dev(st->sc_dev, "bad request, must be multiple of %d\n",
			    st->blksize);
			bp->b_error = EIO;
			goto abort;
		}
	}
	/* as are out-of-range requests on variable drives. */
	else if (bp->b_bcount < st->blkmin ||
	    (st->blkmax && bp->b_bcount > st->blkmax)) {
		aprint_error_dev(st->sc_dev, "bad request, must be between %d and %d\n",
		    st->blkmin, st->blkmax);
		bp->b_error = EIO;
		goto abort;
	}
	s = splbio();

	/*
	 * Place it in the queue of activities for this tape
	 * at the end (a bit silly because we only have on user..
	 * (but it could fork()))
	 */
	bufq_put(st->buf_queue, bp);

	/*
	 * Tell the device to get going on the transfer if it's
	 * not doing anything, otherwise just wait for completion
	 * (All a bit silly if we're only allowing 1 open but..)
	 */
	ststart(st->sc_periph);

	splx(s);
	return;
abort:
	/*
	 * Reset the residue because we didn't do anything,
	 * and send the buffer back as done.
	 */
	bp->b_resid = bp->b_bcount;
	biodone(bp);
	return;
}

/*
 * ststart looks to see if there is a buf waiting for the device
 * and that the device is not already busy. If both are true,
 * It dequeues the buf and creates a scsi command to perform the
 * transfer required. The transfer request will call scsipi_done
 * on completion, which will in turn call this routine again
 * so that the next queued transfer is performed.
 * The bufs are queued by the strategy routine (ststrategy)
 *
 * This routine is also called after other non-queued requests
 * have been made of the scsi driver, to ensure that the queue
 * continues to be drained.
 * ststart() is called at splbio
 */
static void
ststart(struct scsipi_periph *periph)
{
	struct st_softc *st = device_private(periph->periph_dev);
	struct buf *bp;
	struct scsi_rw_tape cmd;
	struct scsipi_xfer *xs;
	int flags, error __diagused;

	SC_DEBUG(periph, SCSIPI_DB2, ("ststart "));
	/* See if there is a buf to do and we are not already  doing one */
	while (periph->periph_active < periph->periph_openings) {
		/* if a special awaits, let it proceed first */
		if (periph->periph_flags & PERIPH_WAITING) {
			periph->periph_flags &= ~PERIPH_WAITING;
			wakeup((void *)periph);
			return;
		}

		/*
		 * If the device has been unmounted by the user
		 * then throw away all requests until done.
		 */
		if (__predict_false((st->flags & ST_MOUNTED) == 0 ||
		    (periph->periph_flags & PERIPH_MEDIA_LOADED) == 0)) {
			if ((bp = bufq_get(st->buf_queue)) != NULL) {
				/* make sure that one implies the other.. */
				periph->periph_flags &= ~PERIPH_MEDIA_LOADED;
				bp->b_error = EIO;
				bp->b_resid = bp->b_bcount;
				biodone(bp);
				continue;
			} else
				return;
		}

		if ((bp = bufq_peek(st->buf_queue)) == NULL)
			return;

		iostat_busy(st->stats);

		/*
		 * only FIXEDBLOCK devices have pending I/O or space operations.
		 */
		if (st->flags & ST_FIXEDBLOCKS) {
			/*
			 * If we are at a filemark but have not reported it yet
			 * then we should report it now
			 */
			if (st->flags & ST_AT_FILEMARK) {
				if ((bp->b_flags & B_READ) == B_WRITE) {
					/*
					 * Handling of ST_AT_FILEMARK in
					 * st_space will fill in the right file
					 * mark count.
					 * Back up over filemark
					 */
					if (st_space(st, 0, SP_FILEMARKS, 0)) {
						bufq_get(st->buf_queue);
						bp->b_error = EIO;
						bp->b_resid = bp->b_bcount;
						biodone(bp);
						continue;
					}
				} else {
					bufq_get(st->buf_queue);
					bp->b_resid = bp->b_bcount;
					bp->b_error = 0;
					st->flags &= ~ST_AT_FILEMARK;
					biodone(bp);
					continue;	/* seek more work */
				}
			}
		}
		/*
		 * If we are at EOM but have not reported it
		 * yet then we should report it now.
		 */
		if (st->flags & (ST_EOM_PENDING|ST_EIO_PENDING)) {
			bufq_get(st->buf_queue);
			bp->b_resid = bp->b_bcount;
			if (st->flags & ST_EIO_PENDING)
				bp->b_error = EIO;
			st->flags &= ~(ST_EOM_PENDING|ST_EIO_PENDING);
			biodone(bp);
			continue;	/* seek more work */
		}

		/* Fill out the scsi command */
		memset(&cmd, 0, sizeof(cmd));
		flags = XS_CTL_NOSLEEP | XS_CTL_ASYNC;
		if ((bp->b_flags & B_READ) == B_WRITE) {
			cmd.opcode = WRITE;
			st->flags &= ~ST_FM_WRITTEN;
			flags |= XS_CTL_DATA_OUT;
		} else {
			cmd.opcode = READ;
			flags |= XS_CTL_DATA_IN;
		}

		/*
		 * Handle "fixed-block-mode" tape drives by using the
		 * block count instead of the length.
		 */
		if (st->flags & ST_FIXEDBLOCKS) {
			cmd.byte2 |= SRW_FIXED;
			_lto3b(bp->b_bcount / st->blksize, cmd.len);
		} else
			_lto3b(bp->b_bcount, cmd.len);

		/* Clear 'position updated' indicator */
		st->flags &= ~ST_POSUPDATED;

		/* go ask the adapter to do all this for us */
		xs = scsipi_make_xs(periph,
		    (struct scsipi_generic *)&cmd, sizeof(cmd),
		    (u_char *)bp->b_data, bp->b_bcount,
		    0, ST_IO_TIME, bp, flags);
		if (__predict_false(xs == NULL)) {
			/*
			 * out of memory. Keep this buffer in the queue, and
			 * retry later.
			 */
			callout_reset(&st->sc_callout, hz / 2, strestart,
			    periph);
			return;
		}
		/*
		 * need to dequeue the buffer before queuing the command,
		 * because cdstart may be called recursively from the
		 * HBA driver
		 */
#ifdef DIAGNOSTIC
		if (bufq_get(st->buf_queue) != bp)
			panic("ststart(): dequeued wrong buf");
#else
		bufq_get(st->buf_queue);
#endif
		error = scsipi_execute_xs(xs);
		/* with a scsipi_xfer preallocated, scsipi_command can't fail */
		KASSERT(error == 0);
	} /* go back and see if we can cram more work in.. */
}

static void
strestart(void *v)
{
	int s = splbio();
	ststart((struct scsipi_periph *)v);
	splx(s);
}

static void
stdone(struct scsipi_xfer *xs, int error)
{
	struct st_softc *st = device_private(xs->xs_periph->periph_dev);
	struct buf *bp = xs->bp;

	if (bp) {
		bp->b_error = error;
		bp->b_resid = xs->resid;
		/*
		 * buggy device ? A SDLT320 can report an info
		 * field of 0x3de8000 on a Media Error/Write Error
		 * for this CBD: 0x0a 00 00 80 00 00
		 */
		if (bp->b_resid > bp->b_bcount || bp->b_resid < 0)
			bp->b_resid = bp->b_bcount;

		if ((bp->b_flags & B_READ) == B_WRITE)
			st->flags |= ST_WRITTEN;
		else
			st->flags &= ~ST_WRITTEN;

		iostat_unbusy(st->stats, bp->b_bcount,
			     ((bp->b_flags & B_READ) == B_READ));

		rnd_add_uint32(&st->rnd_source, bp->b_blkno);

		if ((st->flags & ST_POSUPDATED) == 0) {
			if (error) {
				st->fileno = st->blkno = -1;
			} else if (st->blkno != -1) {
				if (st->flags & ST_FIXEDBLOCKS)
					st->blkno +=
					    (bp->b_bcount / st->blksize);
				else
					st->blkno++;
			}
		}
		biodone(bp);
	}
}

static int
stread(dev_t dev, struct uio *uio, int iomode)
{
	struct st_softc *st = device_lookup_private(&st_cd, STUNIT(dev));

	return physio(ststrategy, NULL, dev, B_READ,
	    st->sc_periph->periph_channel->chan_adapter->adapt_minphys, uio);
}

static int
stwrite(dev_t dev, struct uio *uio, int iomode)
{
	struct st_softc *st = device_lookup_private(&st_cd, STUNIT(dev));

	return physio(ststrategy, NULL, dev, B_WRITE,
	    st->sc_periph->periph_channel->chan_adapter->adapt_minphys, uio);
}

/*
 * Perform special action on behalf of the user;
 * knows about the internals of this device
 */
static int
stioctl(dev_t dev, u_long cmd, void *arg, int flag, struct lwp *l)
{
	int error = 0;
	int unit;
	int number, nmarks, dsty;
	int flags;
	struct st_softc *st;
	int hold_blksize;
	uint8_t hold_density;
	struct mtop *mt = (struct mtop *) arg;

	/* Find the device that the user is talking about */
	flags = 0;		/* give error messages, act on errors etc. */
	unit = STUNIT(dev);
	dsty = STDSTY(dev);
	st = device_lookup_private(&st_cd, unit);
	hold_blksize = st->blksize;
	hold_density = st->density;

	switch ((u_int)cmd) {
	case MTIOCGET: {
		struct mtget *g = (struct mtget *) arg;
		/*
		 * (to get the current state of READONLY)
		 */
		error = st->ops(st, ST_OPS_MODESENSE, XS_CTL_SILENT);
		if (error) {
			/*
			 * Ignore the error if in control mode;
			 * this is mandated by st(4).
			 */
			if (STMODE(dev) != CTRL_MODE)
				break;
			error = 0;
		}
		SC_DEBUG(st->sc_periph, SCSIPI_DB1, ("[ioctl: get status]\n"));
		memset(g, 0, sizeof(struct mtget));
		g->mt_type = MT_ISAR;	/* Ultrix compat *//*? */
		g->mt_blksiz = st->blksize;
		g->mt_density = st->density;
		g->mt_mblksiz[0] = st->modes[0].blksize;
		g->mt_mblksiz[1] = st->modes[1].blksize;
		g->mt_mblksiz[2] = st->modes[2].blksize;
		g->mt_mblksiz[3] = st->modes[3].blksize;
		g->mt_mdensity[0] = st->modes[0].density;
		g->mt_mdensity[1] = st->modes[1].density;
		g->mt_mdensity[2] = st->modes[2].density;
		g->mt_mdensity[3] = st->modes[3].density;
		g->mt_fileno = st->fileno;
		g->mt_blkno = st->blkno;
		if (st->flags & ST_READONLY)
			g->mt_dsreg |= MT_DS_RDONLY;
		if (st->flags & ST_MOUNTED)
			g->mt_dsreg |= MT_DS_MOUNTED;
		g->mt_resid = st->mt_resid;
		g->mt_erreg = st->mt_erreg;
		/*
		 * clear latched errors.
		 */
		st->mt_resid = 0;
		st->mt_erreg = 0;
		st->asc = 0;
		st->ascq = 0;
		break;
	}
	case MTIOCTOP: {
		SC_DEBUG(st->sc_periph, SCSIPI_DB1,
		    ("[ioctl: op=0x%x count=0x%x]\n", mt->mt_op,
			mt->mt_count));

		/* compat: in U*x it is a short */
		number = mt->mt_count;
		switch ((short) (mt->mt_op)) {
		case MTWEOF:	/* write an end-of-file record */
			error = st_write_filemarks(st, number, flags);
			break;
		case MTBSF:	/* backward space file */
			number = -number;
		case MTFSF:	/* forward space file */
			error = st_check_eod(st, FALSE, &nmarks, flags);
			if (!error)
				error = st_space(st, number - nmarks,
				    SP_FILEMARKS, flags);
			break;
		case MTBSR:	/* backward space record */
			number = -number;
		case MTFSR:	/* forward space record */
			error = st_check_eod(st, true, &nmarks, flags);
			if (!error)
				error = st_space(st, number, SP_BLKS, flags);
			break;
		case MTREW:	/* rewind */
			error = st_rewind(st, 0, flags);
			break;
		case MTOFFL:	/* rewind and put the drive offline */
			st_unmount(st, EJECT);
			break;
		case MTNOP:	/* no operation, sets status only */
			break;
		case MTRETEN:	/* retension the tape */
			error = st_load(st, LD_RETENSION, flags);
			if (!error)
				error = st_load(st, LD_LOAD, flags);
			break;
		case MTEOM:	/* forward space to end of media */
			error = st_check_eod(st, FALSE, &nmarks, flags);
			if (!error)
				error = st_space(st, 1, SP_EOM, flags);
			break;
		case MTCACHE:	/* enable controller cache */
			st->flags &= ~ST_DONTBUFFER;
			goto try_new_value;
		case MTNOCACHE:	/* disable controller cache */
			st->flags |= ST_DONTBUFFER;
			goto try_new_value;
		case MTERASE:	/* erase volume */
			error = st_erase(st, number, flags);
			break;
		case MTSETBSIZ:	/* Set block size for device */
#ifdef	NOTYET
			if (!(st->flags & ST_NEW_MOUNT)) {
				uprintf("re-mount tape before changing "
				    "blocksize");
				error = EINVAL;
				break;
			}
#endif
			if (number == 0)
				st->flags &= ~ST_FIXEDBLOCKS;
			else {
				if ((st->blkmin || st->blkmax) &&
				    (number < st->blkmin ||
				    number > st->blkmax)) {
					error = EINVAL;
					break;
				}
				st->flags |= ST_FIXEDBLOCKS;
			}
			st->blksize = number;
			st->flags |= ST_BLOCK_SET;	/*XXX */
			goto try_new_value;
		case MTSETDNSTY:	/* Set density for device and mode */
			/*
			 * Any number >= 0 and <= 0xff is legal. Numbers
			 * above 0x80 are 'vendor unique'.
			 */
			if (number < 0 || number > 255) {
				error = EINVAL;
				break;
			} else
				st->density = number;
			goto try_new_value;
		case MTCMPRESS:
			error = st->ops(st, (number == 0) ?
			    ST_OPS_CMPRSS_OFF : ST_OPS_CMPRSS_ON,
			    XS_CTL_SILENT);
			break;
		case MTEWARN:
			if (number)
				st->flags |= ST_EARLYWARN;
			else
				st->flags &= ~ST_EARLYWARN;
			break;

		default:
			error = EINVAL;
		}
		break;
	}
	case MTIOCIEOT:
	case MTIOCEEOT:
		break;
	case MTIOCRDSPOS:
		error = st_rdpos(st, 0, (uint32_t *)arg);
		break;
	case MTIOCRDHPOS:
		error = st_rdpos(st, 1, (uint32_t *)arg);
		break;
	case MTIOCSLOCATE:
		error = st_setpos(st, 0, (uint32_t *)arg);
		break;
	case MTIOCHLOCATE:
		error = st_setpos(st, 1, (uint32_t *)arg);
		break;
	default:
		error = scsipi_do_ioctl(st->sc_periph, dev, cmd, arg, flag, l);
		break;
	}
	return error;

try_new_value:
	/*
	 * Check that the mode being asked for is aggreeable to the
	 * drive. If not, put it back the way it was.
	 *
	 * If in control mode, we can make (persistent) mode changes
	 * even if no medium is loaded (see st(4)).
	 */
	if ((STMODE(dev) != CTRL_MODE || (st->flags & ST_MOUNTED) != 0) &&
	    (error = st->ops(st, ST_OPS_MODESELECT, 0)) != 0) {
		/* put it back as it was */
		aprint_error_dev(st->sc_dev, "cannot set selected mode\n");
		st->density = hold_density;
		st->blksize = hold_blksize;
		if (st->blksize)
			st->flags |= ST_FIXEDBLOCKS;
		else
			st->flags &= ~ST_FIXEDBLOCKS;
		return error;
	}
	/*
	 * As the drive liked it, if we are setting a new default,
	 * set it into the structures as such.
	 *
	 * The means for deciding this are not finalised yet- but
	 * if the device was opened in Control Mode, the values
	 * are persistent now across mounts.
	 */
	if (STMODE(dev) == CTRL_MODE) {
		switch ((short) (mt->mt_op)) {
		case MTSETBSIZ:
			st->modes[dsty].blksize = st->blksize;
			st->modeflags[dsty] |= BLKSIZE_SET_BY_USER;
			break;
		case MTSETDNSTY:
			st->modes[dsty].density = st->density;
			st->modeflags[dsty] |= DENSITY_SET_BY_USER;
			break;
		}
	}
	return 0;
}

/* Do a synchronous read. */
static int
st_read(struct st_softc *st, char *bf, int size, int flags)
{
	struct scsi_rw_tape cmd;

	/* If it's a null transfer, return immediatly */
	if (size == 0)
		return 0;
	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = READ;
	if (st->flags & ST_FIXEDBLOCKS) {
		cmd.byte2 |= SRW_FIXED;
		_lto3b(size / (st->blksize ? st->blksize : DEF_FIXED_BSIZE),
		    cmd.len);
	} else
		_lto3b(size, cmd.len);
	return scsipi_command(st->sc_periph,
	    (void *)&cmd, sizeof(cmd), (void *)bf, size, 0, ST_IO_TIME, NULL,
	    flags | XS_CTL_DATA_IN);
}

/* issue an erase command */
static int
st_erase(struct st_softc *st, int full, int flags)
{
	int tmo;
	struct scsi_erase cmd;

	/*
	 * Full erase means set LONG bit in erase command, which asks
	 * the drive to erase the entire unit.  Without this bit, we're
	 * asking the drive to write an erase gap.
	 */
	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = ERASE;
	if (full) {
		cmd.byte2 = SE_LONG;
		tmo = ST_SPC_TIME;
	} else
		tmo = ST_IO_TIME;

	/*
	 * XXX We always do this asynchronously, for now, unless the device
	 * has the ST_Q_ERASE_NOIMM quirk.  How long should we wait if we
	 * want to (eventually) to it synchronously?
	 */
	if ((st->quirks & ST_Q_ERASE_NOIMM) == 0)
		cmd.byte2 |= SE_IMMED;

	return scsipi_command(st->sc_periph, (void *)&cmd, sizeof(cmd), 0, 0,
	    ST_RETRIES, tmo, NULL, flags);
}

/* skip N blocks/filemarks/seq filemarks/eom */
static int
st_space(struct st_softc *st, int number, u_int what, int flags)
{
	struct scsi_space cmd;
	int error;

	switch (what) {
	case SP_BLKS:
		if (st->flags & ST_PER_ACTION) {
			if (number > 0) {
				st->flags &= ~ST_PER_ACTION;
				return EIO;
			} else if (number < 0) {
				if (st->flags & ST_AT_FILEMARK) {
					/*
					 * Handling of ST_AT_FILEMARK
					 * in st_space will fill in the
					 * right file mark count.
					 */
					error = st_space(st, 0, SP_FILEMARKS,
					    flags);
					if (error)
						return error;
				}
				if (st->flags & ST_BLANK_READ) {
					st->flags &= ~ST_BLANK_READ;
					return EIO;
				}
				st->flags &= ~(ST_EIO_PENDING|ST_EOM_PENDING);
			}
		}
		break;
	case SP_FILEMARKS:
		if (st->flags & ST_EIO_PENDING) {
			if (number > 0) {
				/* pretend we just discovered the error */
				st->flags &= ~ST_EIO_PENDING;
				return EIO;
			} else if (number < 0) {
				/* back away from the error */
				st->flags &= ~ST_EIO_PENDING;
			}
		}
		if (st->flags & ST_AT_FILEMARK) {
			st->flags &= ~ST_AT_FILEMARK;
			number--;
		}
		if ((st->flags & ST_BLANK_READ) && (number < 0)) {
			/* back away from unwritten tape */
			st->flags &= ~ST_BLANK_READ;
			number++;	/* XXX dubious */
		}
		break;
	case SP_EOM:
		if (st->flags & ST_EOM_PENDING) {
			/* we're already there */
			st->flags &= ~ST_EOM_PENDING;
			return 0;
		}
		if (st->flags & ST_EIO_PENDING) {
			/* pretend we just discovered the error */
			st->flags &= ~ST_EIO_PENDING;
			return EIO;
		}
		if (st->flags & ST_AT_FILEMARK)
			st->flags &= ~ST_AT_FILEMARK;
		break;
	}
	if (number == 0)
		return 0;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = SPACE;
	cmd.byte2 = what;
	_lto3b(number, cmd.number);

	st->flags &= ~ST_POSUPDATED;
	st->last_ctl_resid = 0;
	error = scsipi_command(st->sc_periph, (void *)&cmd, sizeof(cmd), 0, 0,
	    0, ST_SPC_TIME, NULL, flags);

	if (error == 0 && (st->flags & ST_POSUPDATED) == 0) {
		number = number - st->last_ctl_resid;
		if (what == SP_BLKS) {
			if (st->blkno != -1)
				st->blkno += number;
		} else if (what == SP_FILEMARKS) {
			if (st->fileno != -1) {
				st->fileno += number;
				if (number > 0)
					st->blkno = 0;
				else if (number < 0)
					st->blkno = -1;
			}
		} else if (what == SP_EOM) {
			/* This loses us relative position. */
			st->fileno = st->blkno = -1;
		}
	}
	return error;
}

/*
 * write N filemarks
 */
static int
st_write_filemarks(struct st_softc *st, int number, int flags)
{
	int error;
	struct scsi_write_filemarks cmd;

	/*
	 * It's hard to write a negative number of file marks.
	 * Don't try.
	 */
	if (number < 0)
		return EINVAL;
	switch (number) {
	case 0:		/* really a command to sync the drive's buffers */
		break;
	case 1:
		if (st->flags & ST_FM_WRITTEN)	/* already have one down */
			st->flags &= ~ST_WRITTEN;
		else
			st->flags |= ST_FM_WRITTEN;
		st->flags &= ~ST_PER_ACTION;
		break;
	default:
		st->flags &= ~(ST_PER_ACTION | ST_WRITTEN);
	}

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = WRITE_FILEMARKS;
	if (SCSIPI_BUSTYPE_TYPE(scsipi_periph_bustype(st->sc_periph)) ==
	    SCSIPI_BUSTYPE_ATAPI)
		cmd.byte2 = SR_IMMED;
	/*
	 * The ATAPI Onstream DI-30 doesn't support writing filemarks, but
	 * WRITE_FILEMARKS is still used to flush the buffer
	 */
	if ((st->quirks & ST_Q_NOFILEMARKS) == 0)
		_lto3b(number, cmd.number);

	/* XXX WE NEED TO BE ABLE TO GET A RESIDIUAL XXX */
	error = scsipi_command(st->sc_periph, (void *)&cmd, sizeof(cmd), 0, 0,
	    0, ST_IO_TIME * 4, NULL, flags);
	if (error == 0 && st->fileno != -1)
		st->fileno += number;
	return error;
}

/*
 * Make sure the right number of file marks is on tape if the
 * tape has been written.  If the position argument is true,
 * leave the tape positioned where it was originally.
 *
 * nmarks returns the number of marks to skip (or, if position
 * true, which were skipped) to get back original position.
 */
static int
st_check_eod(struct st_softc *st, boolean position, int *nmarks, int flags)
{
	int error;

	switch (st->flags & (ST_WRITTEN | ST_FM_WRITTEN | ST_2FM_AT_EOD)) {
	default:
		*nmarks = 0;
		return 0;
	case ST_WRITTEN:
	case ST_WRITTEN | ST_FM_WRITTEN | ST_2FM_AT_EOD:
		*nmarks = 1;
		break;
	case ST_WRITTEN | ST_2FM_AT_EOD:
		*nmarks = 2;
	}
	error = st_write_filemarks(st, *nmarks, flags);
	if (position && !error)
		error = st_space(st, -*nmarks, SP_FILEMARKS, flags);
	return error;
}

/* load/unload/retension */
static int
st_load(struct st_softc *st, u_int type, int flags)
{
	int error;
	struct scsi_load cmd;

	if (type != LD_LOAD) {
		int nmarks;

		error = st_check_eod(st, FALSE, &nmarks, flags);
		if (error) {
			aprint_error_dev(st->sc_dev,
			    "failed to write closing filemarks at "
			    "unload, errno=%d\n", error);
			return error;
		}
	}
	if (st->quirks & ST_Q_IGNORE_LOADS) {
		if (type == LD_LOAD)
			/*
			 * If we ignore loads, at least we should try a rewind.
			 */
			return st_rewind(st, 0, flags);
		/* otherwise, we should do what's asked of us */
	}

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = LOAD;
	if (SCSIPI_BUSTYPE_TYPE(scsipi_periph_bustype(st->sc_periph)) ==
	    SCSIPI_BUSTYPE_ATAPI)
		cmd.byte2 = SR_IMMED;
	cmd.how = type;

	error = scsipi_command(st->sc_periph, (void *)&cmd, sizeof(cmd), 0, 0,
	    ST_RETRIES, ST_SPC_TIME, NULL, flags);
	if (error) {
		aprint_error_dev(st->sc_dev, "error %d in st_load (op %d)\n",
		    error, type);
	}
	return error;
}

/* Rewind the device */
static int
st_rewind(struct st_softc *st, u_int immediate, int flags)
{
	struct scsi_rewind cmd;
	int error;
	int nmarks;
	int timeout;

	error = st_check_eod(st, FALSE, &nmarks, flags);
	if (error) {
		aprint_error_dev(st->sc_dev,
		    "failed to write closing filemarks at "
		    "rewind, errno=%d\n", error);
		return error;
	}
	st->flags &= ~ST_PER_ACTION;

	/* If requestor asked for immediate response, set a short timeout */
	timeout = immediate ? ST_CTL_TIME : ST_SPC_TIME;

	/* ATAPI tapes always need immediate to be set */
	if (scsipi_periph_bustype(st->sc_periph) == SCSIPI_BUSTYPE_ATAPI)
		immediate = SR_IMMED;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = REWIND;
	cmd.byte2 = immediate;

	error = scsipi_command(st->sc_periph, (void *)&cmd, sizeof(cmd), 0, 0,
	    ST_RETRIES, timeout, NULL, flags);
	if (error) {
		aprint_error_dev(st->sc_dev, "error %d trying to rewind\n",
		    error);
		/* lost position */
		st->fileno = st->blkno = -1;
	} else
		st->fileno = st->blkno = 0;
	return error;
}

static int
st_rdpos(struct st_softc *st, int hard, uint32_t *blkptr)
{
	int error;
	uint8_t posdata[20];
	struct scsi_tape_read_position cmd;

	/*
	 * We try and flush any buffered writes here if we were writing
	 * and we're trying to get hardware block position. It eats
	 * up performance substantially, but I'm wary of drive firmware.
	 *
	 * I think that *logical* block position is probably okay-
	 * but hardware block position might have to wait for data
	 * to hit media to be valid. Caveat Emptor.
	 */

	if (hard && (st->flags & ST_WRITTEN)) {
		/* First flush any pending writes... */
		error = st_write_filemarks(st, 0, XS_CTL_SILENT);
		/*
		 * The latter case is for 'write protected' tapes
		 * which are too stupid to recognize a zero count
		 * for writing filemarks as a no-op.
		 */
		if (error != 0 && error != EACCES && error != EROFS)
			return error;
	}

	memset(&cmd, 0, sizeof(cmd));
	memset(&posdata, 0, sizeof(posdata));
	cmd.opcode = READ_POSITION;
	if (hard)
		cmd.byte1 = 1;

	error = scsipi_command(st->sc_periph, (void *)&cmd, sizeof(cmd),
	    (void *)&posdata, sizeof(posdata), ST_RETRIES, ST_CTL_TIME, NULL,
	    XS_CTL_SILENT | XS_CTL_DATA_IN);

	if (error == 0) {
#if	0
		printf("posdata:");
		for (hard = 0; hard < sizeof(posdata); hard++)
			printf("%02x ", posdata[hard] & 0xff);
		printf("\n");
#endif
		if (posdata[0] & 0x4)	/* Block Position Unknown */
			error = EINVAL;
		else
			*blkptr = _4btol(&posdata[4]);
	}
	return error;
}

static int
st_setpos(struct st_softc *st, int hard, uint32_t *blkptr)
{
	int error;
	struct scsi_tape_locate cmd;

	/*
	 * We used to try and flush any buffered writes here.
	 * Now we push this onto user applications to either
	 * flush the pending writes themselves (via a zero count
	 * WRITE FILEMARKS command) or they can trust their tape
	 * drive to do this correctly for them.
	 *
	 * There are very ugly performance limitations otherwise.
	 */

	memset(&cmd, 0, sizeof(cmd));
	cmd.opcode = LOCATE;
	if (hard)
		cmd.byte2 = 1 << 2;
	_lto4b(*blkptr, cmd.blkaddr);
	error = scsipi_command(st->sc_periph, (void *)&cmd, sizeof(cmd), 0, 0,
	    ST_RETRIES, ST_SPC_TIME, NULL, 0);
	/*
	 * Note file && block number position now unknown (if
	 * these things ever start being maintained in this driver)
	 */
	st->fileno = st->blkno = -1;
	return error;
}


/*
 * Look at the returned sense and act on the error and determine
 * the unix error number to pass back..., 0 (== report no error),
 * -1 = retry the operation, -2 continue error processing.
 */
static int
st_interpret_sense(struct scsipi_xfer *xs)
{
	struct scsipi_periph *periph = xs->xs_periph;
	struct scsi_sense_data *sense = &xs->sense.scsi_sense;
	struct buf *bp = xs->bp;
	struct st_softc *st = device_private(periph->periph_dev);
	int retval = EJUSTRETURN;
	int doprint = ((xs->xs_control & XS_CTL_SILENT) == 0);
	uint8_t key;
	int32_t info;

	/*
	 * If it isn't a extended or extended/deferred error, let
	 * the generic code handle it.
	 */
	if (SSD_RCODE(sense->response_code) != SSD_RCODE_CURRENT &&
	    SSD_RCODE(sense->response_code) != SSD_RCODE_DEFERRED)
		return retval;

	if (sense->response_code & SSD_RCODE_VALID)
		info = _4btol(sense->info);
	else
		info = (st->flags & ST_FIXEDBLOCKS) ?
		    xs->datalen / st->blksize : xs->datalen;
	key = SSD_SENSE_KEY(sense->flags);
	st->mt_erreg = key;
	st->asc = sense->asc;
	st->ascq = sense->ascq;
	st->mt_resid = (short) info;

	if (key == SKEY_NOT_READY && st->asc == 0x4 && st->ascq == 0x1) {
		/* Not Ready, Logical Unit Is in Process Of Becoming Ready */
		if (!callout_pending(&periph->periph_callout))
			scsipi_periph_freeze(periph, 1);
		callout_reset(&periph->periph_callout,
		    hz, scsipi_periph_timed_thaw, periph);
		return ERESTART;
	}

	/* If the device is not open yet, let generic handle */
	if ((periph->periph_flags & PERIPH_OPEN) == 0)
		return retval;

	xs->resid = info;
	if (st->flags & ST_FIXEDBLOCKS) {
		if (bp) {
			xs->resid *= st->blksize;
			st->last_io_resid = xs->resid;
		} else
			st->last_ctl_resid = xs->resid;
		if (key == SKEY_VOLUME_OVERFLOW) {
			st->flags |= ST_EIO_PENDING;
			if (bp)
				bp->b_resid = xs->resid;
		} else if (sense->flags & SSD_EOM) {
			if ((st->flags & ST_EARLYWARN) == 0)
				st->flags |= ST_EIO_PENDING;
			st->flags |= ST_EOM_PENDING;
			if (bp) {
#if 0
				bp->b_resid = xs->resid;
#else
				/*
				 * Grotesque as it seems, the few times
				 * I've actually seen a non-zero resid,
				 * the tape drive actually lied and had
				 * written all the data!
				 */
				bp->b_resid = 0;
#endif
			}
		}
		if (sense->flags & SSD_FILEMARK) {
			st->flags |= ST_AT_FILEMARK;
			if (bp)
				bp->b_resid = xs->resid;
			if (st->fileno != (daddr_t) -1) {
				st->fileno++;
				st->blkno = 0;
				st->flags |= ST_POSUPDATED;
			}
		}
		if (sense->flags & SSD_ILI) {
			st->flags |= ST_EIO_PENDING;
			if (bp)
				bp->b_resid = xs->resid;
			if (sense->response_code & SSD_RCODE_VALID &&
			    (xs->xs_control & XS_CTL_SILENT) == 0)
				aprint_error_dev(st->sc_dev,
				    "block wrong size, %d blocks residual\n",
				    info);

			/*
			 * This quirk code helps the drive read
			 * the first tape block, regardless of
			 * format.  That is required for these
			 * drives to return proper MODE SENSE
			 * information.
			 */
			if ((st->quirks & ST_Q_SENSE_HELP) &&
			    (periph->periph_flags & PERIPH_MEDIA_LOADED) == 0)
				st->blksize -= 512;
			else if ((st->flags & ST_POSUPDATED) == 0) {
				if (st->blkno != (daddr_t) -1) {
					st->blkno +=
					    (xs->datalen / st->blksize);
					st->flags |= ST_POSUPDATED;
				}
			}
		}
		/*
		 * If data wanted and no data was transferred, do it immediately
		 */
		if (xs->datalen && xs->resid >= xs->datalen) {
			if (st->flags & ST_EIO_PENDING)
				return EIO;
			if (st->flags & ST_AT_FILEMARK) {
				if (bp)
					bp->b_resid = xs->resid;
				return 0;
			}
		}
	} else {		/* must be variable mode */
		if (bp)
			st->last_io_resid = xs->resid;
		else
			st->last_ctl_resid = xs->resid;
		if (sense->flags & SSD_EOM) {
			/*
			 * The current semantics of this
			 * driver requires EOM detection
			 * to return EIO unless early
			 * warning detection is enabled
			 * for variable mode (this is always
			 * on for fixed block mode).
			 */
			if (st->flags & ST_EARLYWARN) {
				st->flags |= ST_EOM_PENDING;
				retval = 0;
			} else {
				retval = EIO;
				/*
				 * If we return an error we can't claim to
				 * have transfered all data.
				 */
				if (xs->resid == 0)
					xs->resid = xs->datalen;
			}

			/*
			 * If it's an unadorned EOM detection,
			 * suppress printing an error.
			 */
			if (key == SKEY_NO_SENSE) {
				doprint = 0;
			}
		} else if (sense->flags & SSD_FILEMARK) {
			retval = 0;
			if (st->fileno != (daddr_t) -1) {
				st->fileno++;
				st->blkno = 0;
				st->flags |= ST_POSUPDATED;
			}
		} else if (sense->flags & SSD_ILI) {
			if (info < 0) {
				/*
				 * The tape record was bigger than the read
				 * we issued.
				 */
				if ((xs->xs_control & XS_CTL_SILENT) == 0) {
					aprint_error_dev(st->sc_dev,
					    "%d-byte tape record too big"
					    " for %d-byte user buffer\n",
					    xs->datalen - info, xs->datalen);
				}
				retval = EIO;
			} else {
				retval = 0;
				if (st->blkno != (daddr_t) -1) {
					st->blkno++;
					st->flags |= ST_POSUPDATED;
				}
			}
		}
		if (bp)
			bp->b_resid = xs->resid;
	}

#ifndef SCSIPI_DEBUG
	if (retval == 0 && key == SKEY_NO_SENSE)
		doprint = 0;
#endif
	if (key == SKEY_BLANK_CHECK) {
		/*
		 * This quirk code helps the drive read the
		 * first tape block, regardless of format.  That
		 * is required for these drives to return proper
		 * MODE SENSE information.
		 */
		if ((st->quirks & ST_Q_SENSE_HELP) &&
		    (periph->periph_flags & PERIPH_MEDIA_LOADED) == 0) {
			/* still starting */
			st->blksize -= 512;
		} else if (!(st->flags & (ST_2FM_AT_EOD | ST_BLANK_READ))) {
			st->flags |= ST_BLANK_READ;
			xs->resid = xs->datalen;
			if (bp) {
				bp->b_resid = xs->resid;
				/* return an EOF */
			}
			retval = 0;
			/* lost position */
			st->fileno = st->blkno = -1;
		}
	}

	/*
	 * If generic sense processing will continue, we should not
	 * print sense info here.
	 */
	if (retval == EJUSTRETURN)
		doprint = 0;

	if (doprint) {
		/* Print verbose sense info if possible */
		if (scsipi_print_sense(xs, 0) != 0)
			return retval;

		/* Print less-verbose sense info */
		scsipi_printaddr(periph);
		printf("Sense Key 0x%02x", key);
		if ((sense->response_code & SSD_RCODE_VALID) != 0) {
			switch (key) {
			case SKEY_NOT_READY:
			case SKEY_ILLEGAL_REQUEST:
			case SKEY_UNIT_ATTENTION:
			case SKEY_DATA_PROTECT:
				break;
			case SKEY_VOLUME_OVERFLOW:
			case SKEY_BLANK_CHECK:
				printf(", requested size: %d (decimal)", info);
				break;
			case SKEY_ABORTED_COMMAND:
				if (xs->xs_retries)
					printf(", retrying");
				printf(", cmd 0x%x, info 0x%x",
				    xs->cmd->opcode, info);
				break;
			default:
				printf(", info = %d (decimal)", info);
			}
		}
		if (sense->extra_len != 0) {
			int n;
			printf(", data =");
			for (n = 0; n < sense->extra_len; n++)
				printf(" %02x", sense->csi[n]);
		}
		printf("\n");
	}
	return retval;
}

/*
 * The quirk here is that the drive returns some value to st_mode_sense
 * incorrectly until the tape has actually passed by the head.
 *
 * The method is to set the drive to large fixed-block state (user-specified
 * density and 1024-byte blocks), then read and rewind to get it to sense the
 * tape.  If that doesn't work, try 512-byte fixed blocks.  If that doesn't
 * work, as a last resort, try variable- length blocks.  The result will be
 * the ability to do an accurate st_mode_sense.
 *
 * We know we can do a rewind because we just did a load, which implies rewind.
 * Rewind seems preferable to space backward if we have a virgin tape.
 *
 * The rest of the code for this quirk is in ILI processing and BLANK CHECK
 * error processing, both part of st_interpret_sense.
 */
static int
st_touch_tape(struct st_softc *st)
{
	char *bf;
	int readsize;
	int error;

	bf = malloc(1024, M_TEMP, M_NOWAIT);
	if (bf == NULL)
		return ENOMEM;

	if ((error = st->ops(st, ST_OPS_MODESENSE, 0)) != 0)
		goto bad;

	/*
	 * If the block size is already known from the
	 * sense data, use it. Else start probing at 1024.
	 */
	if (st->media_blksize > 0)
		st->blksize = st->media_blksize;
	else
		st->blksize = 1024;

	do {
		switch (st->blksize) {
		case 512:
		case 1024:
			readsize = st->blksize;
			st->flags |= ST_FIXEDBLOCKS;
			break;
		default:
			readsize = 1;
			st->flags &= ~ST_FIXEDBLOCKS;
		}
		if ((error = st->ops(st, ST_OPS_MODESELECT, XS_CTL_SILENT))
		    != 0) {
			/*
			 * The device did not agree with the proposed
			 * block size. If we exhausted our options,
			 * return failure, else try another.
			 */
			if (readsize == 1)
				goto bad;
			st->blksize -= 512;
			continue;
		}
		st_read(st, bf, readsize, XS_CTL_SILENT);	/* XXX */
		if ((error = st_rewind(st, 0, 0)) != 0) {
bad:			free(bf, M_TEMP);
			return error;
		}
	} while (readsize != 1 && readsize > st->blksize);

	free(bf, M_TEMP);
	return 0;
}

static int
stdump(dev_t dev, daddr_t blkno, void *va, size_t size)
{
	/* Not implemented. */
	return ENXIO;
}

/*
 * Send a filled out parameter structure to the drive to
 * set it into the desire modes etc.
 */
int
st_mode_select(struct st_softc *st, int flags)
{
	u_int select_len;
	struct select {
		struct scsi_mode_parameter_header_6 header;
		struct scsi_general_block_descriptor blk_desc;
		u_char sense_data[MAX_PAGE_0_SIZE];
	} select;
	struct scsipi_periph *periph = st->sc_periph;

	select_len = sizeof(select.header) + sizeof(select.blk_desc) +
		     st->page_0_size;

	/*
	 * This quirk deals with drives that have only one valid mode
	 * and think this gives them license to reject all mode selects,
	 * even if the selected mode is the one that is supported.
	 */
	if (st->quirks & ST_Q_UNIMODAL) {
		SC_DEBUG(periph, SCSIPI_DB3,
		    ("not setting density 0x%x blksize 0x%x\n",
		    st->density, st->blksize));
		return 0;
	}

	/* Set up for a mode select */
	memset(&select, 0, sizeof(select));
	select.header.blk_desc_len = sizeof(struct
	    scsi_general_block_descriptor);
	select.header.dev_spec &= ~SMH_DSP_BUFF_MODE;
	select.blk_desc.density = st->density;
	if (st->flags & ST_DONTBUFFER)
		select.header.dev_spec |= SMH_DSP_BUFF_MODE_OFF;
	else
		select.header.dev_spec |= SMH_DSP_BUFF_MODE_ON;
	if (st->flags & ST_FIXEDBLOCKS)
		_lto3b(st->blksize, select.blk_desc.blklen);
	if (st->page_0_size)
		memcpy(select.sense_data, st->sense_data, st->page_0_size);

	/* do the command */
	return scsipi_mode_select(periph, 0, &select.header, select_len,
				  flags, ST_RETRIES, ST_CTL_TIME);
}
