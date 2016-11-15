/*	$NetBSD: mcd.c,v 1.115 2015/04/26 15:15:20 mlelstv Exp $	*/

/*
 * Copyright (c) 1993, 1994, 1995 Charles M. Hannum.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Charles M. Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Copyright 1993 by Holger Veit (data part)
 * Copyright 1993 by Brian Moore (audio part)
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This software was developed by Holger Veit and Brian Moore
 *      for use with "386BSD" and similar operating systems.
 *    "Similar operating systems" includes mainly non-profit oriented
 *    systems for research and education, including but not restricted to
 *    "NetBSD", "FreeBSD", "Mach" (by CMU).
 * 4. Neither the name of the developer(s) nor the name "386BSD"
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER(S) BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*static char COPYRIGHT[] = "mcd-driver (C)1993 by H.Veit & B.Moore";*/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mcd.c,v 1.115 2015/04/26 15:15:20 mlelstv Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <sys/cdio.h>
#include <sys/errno.h>
#include <sys/disklabel.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/mutex.h>
#include <sys/cpu.h>
#include <sys/intr.h>
#include <sys/bus.h>

#include <dev/isa/isavar.h>
#include <dev/isa/mcdreg.h>

#ifndef MCDDEBUG
#define MCD_TRACE(fmt,...)
#else
#define MCD_TRACE(fmt,...)	{if (sc->debug) {printf("%s: st=%02x: ", device_xname(sc->sc_dev), sc->status); printf(fmt,__VA_ARGS__);}}
#endif

#define	MCDPART(dev)	DISKPART(dev)
#define	MCDUNIT(dev)	DISKUNIT(dev)

/* toc */
#define MCD_MAXTOCS	104	/* from the Linux driver */

/* control promiscuous match */
#include "opt_mcd_promisc.h"

#ifdef MCD_PROMISC
int mcd_promisc = 1;
#else
int mcd_promisc = 0;
#endif

struct mcd_mbx {
	int		retry, count;
	struct buf	*bp;
	daddr_t		blkno;
	int		nblk;
	int		sz;
	u_long		skip;
	int		state;
#define	MCD_S_IDLE	0
#define MCD_S_BEGIN	1
#define MCD_S_WAITMODE	2
#define MCD_S_WAITREAD	3
	int		mode;
};

struct mcd_softc {
	device_t sc_dev;
	struct	disk sc_dk;
	kmutex_t sc_lock;
	void *sc_ih;

	callout_t sc_pintr_ch;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	int	irq, drq;

	const char	*type;
	int	flags;
#define	MCDF_WLABEL	0x04	/* label is writable */
#define	MCDF_LABELLING	0x08	/* writing label */
#define	MCDF_LOADED	0x10	/* parameters loaded */
	short	status;
	short	audio_status;
	int	blksize;
	u_long	disksize;
	struct	mcd_volinfo volinfo;
	union	mcd_qchninfo toc[MCD_MAXTOCS];
	struct	mcd_command lastpb;
	struct	mcd_mbx mbx;
	int	lastmode;
#define	MCD_MD_UNKNOWN	-1
	int	lastupc;
#define	MCD_UPC_UNKNOWN	-1
	struct bufq_state *buf_queue;
	int	active;
	u_char	readcmd;
	u_char	debug;
	u_char	probe;
};

static int bcd2bin(bcd_t);
static bcd_t bin2bcd(int);
static void hsg2msf(int, bcd_t *);
static daddr_t msf2hsg(bcd_t *, int);

int mcd_playtracks(struct mcd_softc *, struct ioc_play_track *);
int mcd_playmsf(struct mcd_softc *, struct ioc_play_msf *);
int mcd_playblocks(struct mcd_softc *, struct ioc_play_blocks *);
int mcd_stop(struct mcd_softc *);
int mcd_eject(struct mcd_softc *);
int mcd_read_subchannel(struct mcd_softc *, struct ioc_read_subchannel *,
    struct cd_sub_channel_info *);
int mcd_pause(struct mcd_softc *);
int mcd_resume(struct mcd_softc *);
int mcd_toc_header(struct mcd_softc *, struct ioc_toc_header *);
int mcd_toc_entries(struct mcd_softc *, struct ioc_read_toc_entry *,
	struct cd_toc_entry *, int *);

int mcd_getreply(struct mcd_softc *);
int mcd_getstat(struct mcd_softc *);
int mcd_getresult(struct mcd_softc *, struct mcd_result *);
void mcd_setflags(struct mcd_softc *);
int mcd_get(struct mcd_softc *, char *, int);
int mcd_send(struct mcd_softc *, struct mcd_mbox *, int);
int mcdintr(void *);
void mcd_soft_reset(struct mcd_softc *);
int mcd_hard_reset(struct mcd_softc *);
int mcd_setmode(struct mcd_softc *, int);
int mcd_setupc(struct mcd_softc *, int);
int mcd_read_toc(struct mcd_softc *);
int mcd_getqchan(struct mcd_softc *, union mcd_qchninfo *, int);
int mcd_setlock(struct mcd_softc *, int);

int mcd_find(bus_space_tag_t, bus_space_handle_t, struct mcd_softc *);
int mcdprobe(device_t, cfdata_t, void *);
void mcdattach(device_t, device_t, void *);

CFATTACH_DECL_NEW(mcd, sizeof(struct mcd_softc),
    mcdprobe, mcdattach, NULL, NULL);

extern struct cfdriver mcd_cd;

dev_type_open(mcdopen);
dev_type_close(mcdclose);
dev_type_read(mcdread);
dev_type_write(mcdwrite);
dev_type_ioctl(mcdioctl);
dev_type_strategy(mcdstrategy);
dev_type_dump(mcddump);
dev_type_size(mcdsize);

const struct bdevsw mcd_bdevsw = {
	.d_open = mcdopen,
	.d_close = mcdclose,
	.d_strategy = mcdstrategy,
	.d_ioctl = mcdioctl,
	.d_dump = mcddump,
	.d_psize = mcdsize,
	.d_discard = nodiscard,
	.d_flag = D_DISK
};

const struct cdevsw mcd_cdevsw = {
	.d_open = mcdopen,
	.d_close = mcdclose,
	.d_read = mcdread,
	.d_write = mcdwrite,
	.d_ioctl = mcdioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_DISK
};

void	mcdgetdefaultlabel(struct mcd_softc *, struct disklabel *);
void	mcdgetdisklabel(struct mcd_softc *);
int	mcd_get_parms(struct mcd_softc *);
void	mcdstart(struct mcd_softc *);
void	mcd_pseudointr(void *);

struct dkdriver mcddkdriver = {
	.d_strategy = mcdstrategy
};

#define MCD_RETRIES	3
#define MCD_RDRETRIES	3

/* several delays */
#define RDELAY_WAITMODE	300
#define RDELAY_WAITREAD	800

#define	DELAY_GRANULARITY	25	/* 25us */
#define DELAY_GETREPLY		100000	/* 100000 * 25us */

void
mcdattach(device_t parent, device_t self, void *aux)
{
	struct mcd_softc *sc = device_private(self);
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	struct mcd_mbox mbx;

	/* Map i/o space */
	if (bus_space_map(iot, ia->ia_io[0].ir_addr, MCD_NPORT, 0, &ioh)) {
		printf(": can't map i/o space\n");
		return;
	}

	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_NONE);

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;

	sc->probe = 0;
	sc->debug = 0;

	if (!mcd_find(iot, ioh, sc)) {
		printf(": mcd_find failed\n");
		return;
	}

	bufq_alloc(&sc->buf_queue, "disksort", BUFQ_SORT_RAWBLOCK);
	callout_init(&sc->sc_pintr_ch, 0);

	/*
	 * Initialize and attach the disk structure.
	 */
	disk_init(&sc->sc_dk, device_xname(sc->sc_dev), &mcddkdriver);
	disk_attach(&sc->sc_dk);

	printf(": model %s\n", sc->type != 0 ? sc->type : "unknown");

	(void) mcd_setlock(sc, MCD_LK_UNLOCK);

	mbx.cmd.opcode = MCD_CMDCONFIGDRIVE;
	mbx.cmd.length = sizeof(mbx.cmd.data.config) - 1;
	mbx.cmd.data.config.subcommand = MCD_CF_IRQENABLE;
	mbx.cmd.data.config.data1 = 0x01;
	mbx.res.length = 0;
	(void) mcd_send(sc, &mbx, 0);

	mcd_soft_reset(sc);

	sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq[0].ir_irq,
	    IST_EDGE, IPL_BIO, mcdintr, sc);
}

int
mcdopen(dev_t dev, int flag, int fmt, struct lwp *l)
{
	int error, part;
	struct mcd_softc *sc;

	sc = device_lookup_private(&mcd_cd, MCDUNIT(dev));
	if (sc == NULL)
		return ENXIO;

	mutex_enter(&sc->sc_lock);

	if (sc->sc_dk.dk_openmask != 0) {
		/*
		 * If any partition is open, but the disk has been invalidated,
		 * disallow further opens.
		 */
		if ((sc->flags & MCDF_LOADED) == 0) {
			error = EIO;
			goto bad3;
		}
	} else {
		/*
		 * Lock the drawer.  This will also notice any pending disk
		 * change or door open indicator and clear the MCDF_LOADED bit
		 * if necessary.
		 */
		(void) mcd_setlock(sc, MCD_LK_LOCK);

		if ((sc->flags & MCDF_LOADED) == 0) {
			/* Partially reset the state. */
			sc->lastmode = MCD_MD_UNKNOWN;
			sc->lastupc = MCD_UPC_UNKNOWN;

			sc->flags |= MCDF_LOADED;

			/* Set the mode, causing the disk to spin up. */
			if ((error = mcd_setmode(sc, MCD_MD_COOKED)) != 0)
				goto bad2;

			/* Load the physical device parameters. */
			if (mcd_get_parms(sc) != 0) {
				error = ENXIO;
				goto bad2;
			}

			/* Read the table of contents. */
			if ((error = mcd_read_toc(sc)) != 0)
				goto bad2;

			/* Fabricate a disk label. */
			mcdgetdisklabel(sc);
		}
	}

	part = MCDPART(dev);

	MCD_TRACE("open: partition=%d disksize=%ld blksize=%d\n", part,
	    sc->disksize, sc->blksize);

	/* Check that the partition exists. */
	if (part != RAW_PART &&
	    (part >= sc->sc_dk.dk_label->d_npartitions ||
	     sc->sc_dk.dk_label->d_partitions[part].p_fstype == FS_UNUSED)) {
		error = ENXIO;
		goto bad;
	}

	/* Insure only one open at a time. */
	switch (fmt) {
	case S_IFCHR:
		sc->sc_dk.dk_copenmask |= (1 << part);
		break;
	case S_IFBLK:
		sc->sc_dk.dk_bopenmask |= (1 << part);
		break;
	}
	sc->sc_dk.dk_openmask = sc->sc_dk.dk_copenmask | sc->sc_dk.dk_bopenmask;

	mutex_exit(&sc->sc_lock);
	return 0;

bad2:
	sc->flags &= ~MCDF_LOADED;

bad:
	if (sc->sc_dk.dk_openmask == 0) {
#if 0
		(void) mcd_setmode(sc, MCD_MD_SLEEP);
#endif
		(void) mcd_setlock(sc, MCD_LK_UNLOCK);
	}

bad3:
	mutex_exit(&sc->sc_lock);
	return error;
}

int
mcdclose(dev_t dev, int flag, int fmt, struct lwp *l)
{
	struct mcd_softc *sc = device_lookup_private(&mcd_cd, MCDUNIT(dev));
	int part = MCDPART(dev);
	
	MCD_TRACE("close: partition=%d\n", part);

	mutex_enter(&sc->sc_lock);

	switch (fmt) {
	case S_IFCHR:
		sc->sc_dk.dk_copenmask &= ~(1 << part);
		break;
	case S_IFBLK:
		sc->sc_dk.dk_bopenmask &= ~(1 << part);
		break;
	}
	sc->sc_dk.dk_openmask = sc->sc_dk.dk_copenmask | sc->sc_dk.dk_bopenmask;

	if (sc->sc_dk.dk_openmask == 0) {
		/* XXXX Must wait for I/O to complete! */

#if 0
		(void) mcd_setmode(sc, MCD_MD_SLEEP);
#endif
		(void) mcd_setlock(sc, MCD_LK_UNLOCK);
	}

	mutex_exit(&sc->sc_lock);
	return 0;
}

void
mcdstrategy(struct buf *bp)
{
	struct mcd_softc *sc;
	struct disklabel *lp;
	daddr_t blkno;
	int s;

	sc = device_lookup_private(&mcd_cd, MCDUNIT(bp->b_dev));
	lp = sc->sc_dk.dk_label;

	/* Test validity. */
	MCD_TRACE("strategy: buf=0x%p blkno=%d bcount=%d\n", bp,
	    (int) bp->b_blkno, bp->b_bcount);
	if (bp->b_blkno < 0 ||
	    (bp->b_bcount % sc->blksize) != 0) {
		printf("%s: strategy: blkno = %" PRId64 " bcount = %d\n",
		    device_xname(sc->sc_dev), bp->b_blkno, bp->b_bcount);
		bp->b_error = EINVAL;
		goto done;
	}

	/* If device invalidated (e.g. media change, door open), error. */
	if ((sc->flags & MCDF_LOADED) == 0) {
		MCD_TRACE("strategy: drive not valid%s", "\n");
		bp->b_error = EIO;
		goto done;
	}

	/* No data to read. */
	if (bp->b_bcount == 0)
		goto done;

	/*
	 * Do bounds checking, adjust transfer. if error, process.
	 * If end of partition, just return.
	 */
	if (MCDPART(bp->b_dev) != RAW_PART &&
	    bounds_check_with_label(&sc->sc_dk, bp,
	    (sc->flags & (MCDF_WLABEL|MCDF_LABELLING)) != 0) <= 0)
		goto done;

	/*
	 * Now convert the block number to absolute and put it in
	 * terms of the device's logical block size.
	 */
	blkno = bp->b_blkno / (lp->d_secsize / DEV_BSIZE);
	if (MCDPART(bp->b_dev) != RAW_PART)
		blkno += lp->d_partitions[MCDPART(bp->b_dev)].p_offset;

	bp->b_rawblkno = blkno;

	/* Queue it. */
	s = splbio();
	bufq_put(sc->buf_queue, bp);
	splx(s);
	if (!sc->active)
		mcdstart(sc);
	return;

done:
	bp->b_resid = bp->b_bcount;
	biodone(bp);
}

void
mcdstart(struct mcd_softc *sc)
{
	struct buf *bp;
	int s;

loop:
	s = splbio();

	if ((bp = bufq_get(sc->buf_queue)) == NULL) {
		/* Nothing to do. */
		sc->active = 0;
		splx(s);
		return;
	}

	/* Block found to process. */
	MCD_TRACE("start: found block bp=0x%p\n", bp);
	splx(s);

	/* Changed media? */
	if ((sc->flags & MCDF_LOADED) == 0) {
		MCD_TRACE("start: drive not valid%s", "\n");
		bp->b_error = EIO;
		biodone(bp);
		goto loop;
	}

	sc->active = 1;

	/* Instrumentation. */
	s = splbio();
	disk_busy(&sc->sc_dk);
	splx(s);

	sc->mbx.retry = MCD_RDRETRIES;
	sc->mbx.bp = bp;
	sc->mbx.blkno = bp->b_rawblkno;
	sc->mbx.nblk = bp->b_bcount / sc->blksize;
	sc->mbx.sz = sc->blksize;
	sc->mbx.skip = 0;
	sc->mbx.state = MCD_S_BEGIN;
	sc->mbx.mode = MCD_MD_COOKED;

	s = splbio();
	(void) mcdintr(sc);
	splx(s);
}

int
mcdread(dev_t dev, struct uio *uio, int flags)
{

	return (physio(mcdstrategy, NULL, dev, B_READ, minphys, uio));
}

int
mcdwrite(dev_t dev, struct uio *uio, int flags)
{

	return (physio(mcdstrategy, NULL, dev, B_WRITE, minphys, uio));
}

int
mcdioctl(dev_t dev, u_long cmd, void *addr, int flag, struct lwp *l)
{
	struct mcd_softc *sc = device_lookup_private(&mcd_cd, MCDUNIT(dev));
	int error;
	int part;
#ifdef __HAVE_OLD_DISKLABEL
	struct disklabel newlabel;
#endif
	
	MCD_TRACE("ioctl: cmd=0x%lx\n", cmd);

	if ((sc->flags & MCDF_LOADED) == 0)
		return EIO;

	error = disk_ioctl(&sc->sc_dk, dev, cmd, addr, flag, l);
	if (error != EPASSTHROUGH)
		return error;

	part = MCDPART(dev);
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
			return EBADF;

#ifdef __HAVE_OLD_DISKLABEL
		if (cmd == ODIOCSDINFO || cmd == ODIOCWDINFO) {
			memset(&newlabel, 0, sizeof newlabel);
			memcpy(&newlabel, addr, sizeof (struct olddisklabel));
			lp = &newlabel;
		} else
#endif
		lp = addr;

		mutex_enter(&sc->sc_lock);
		sc->flags |= MCDF_LABELLING;

		error = setdisklabel(sc->sc_dk.dk_label,
		    lp, /*sc->sc_dk.dk_openmask : */0,
		    sc->sc_dk.dk_cpulabel);
		if (error == 0) {
		}

		sc->flags &= ~MCDF_LABELLING;
		mutex_exit(&sc->sc_lock);
		return error;
	}

	case DIOCWLABEL:
		return EBADF;

	case DIOCGDEFLABEL:
		mcdgetdefaultlabel(sc, addr);
		return 0;

#ifdef __HAVE_OLD_DISKLABEL
	case ODIOCGDEFLABEL:
		mcdgetdefaultlabel(sc, &newlabel);
		if (newlabel.d_npartitions > OLDMAXPARTITIONS)
			return ENOTTY;
		memcpy(addr, &newlabel, sizeof (struct olddisklabel));
		return 0;
#endif

	case CDIOCPLAYTRACKS:
		return mcd_playtracks(sc, addr);
	case CDIOCPLAYMSF:
		return mcd_playmsf(sc, addr);
	case CDIOCPLAYBLOCKS:
		return mcd_playblocks(sc, addr);
	case CDIOCREADSUBCHANNEL: {
		struct cd_sub_channel_info info;
		error = mcd_read_subchannel(sc, addr, &info);
		if (error != 0) {
			struct ioc_read_subchannel *ch = addr;
			error = copyout(&info, ch->data, ch->data_len);
		}
		return error;
	}
	case CDIOCREADSUBCHANNEL_BUF:
		return mcd_read_subchannel(sc, addr,
		    &((struct ioc_read_subchannel_buf *)addr)->info);
	case CDIOREADTOCHEADER:
		return mcd_toc_header(sc, addr);
	case CDIOREADTOCENTRYS: {
		struct cd_toc_entry entries[MCD_MAXTOCS];
		struct ioc_read_toc_entry *te = addr;
		int count;
		if (te->data_len > sizeof entries)
			return EINVAL;
		error = mcd_toc_entries(sc, te, entries, &count);
		if (error == 0)
			/* Copy the data back. */
			error = copyout(entries, te->data, min(te->data_len,
					count * sizeof(struct cd_toc_entry)));
		return error;
	}
	case CDIOREADTOCENTRIES_BUF: {
		struct ioc_read_toc_entry_buf *te = addr;
		int count;
		if (te->req.data_len > sizeof te->entry)
			return EINVAL;
		return mcd_toc_entries(sc, &te->req, te->entry, &count);
	}
	case CDIOCSETPATCH:
	case CDIOCGETVOL:
	case CDIOCSETVOL:
	case CDIOCSETMONO:
	case CDIOCSETSTEREO:
	case CDIOCSETMUTE:
	case CDIOCSETLEFT:
	case CDIOCSETRIGHT:
		return EINVAL;
	case CDIOCRESUME:
		return mcd_resume(sc);
	case CDIOCPAUSE:
		return mcd_pause(sc);
	case CDIOCSTART:
		return EINVAL;
	case CDIOCSTOP:
		return mcd_stop(sc);
	case DIOCEJECT:
		if (*(int *)addr == 0) {
			/*
			 * Don't force eject: check that we are the only
			 * partition open. If so, unlock it.
			 */
			if ((sc->sc_dk.dk_openmask & ~(1 << part)) == 0 &&
			    sc->sc_dk.dk_bopenmask + sc->sc_dk.dk_copenmask ==
			    sc->sc_dk.dk_openmask) {
				error = mcd_setlock(sc, MCD_LK_UNLOCK);
				if (error)
					return (error);
			} else {
				return (EBUSY);
			}
		}
		/* FALLTHROUGH */
	case CDIOCEJECT: /* FALLTHROUGH */
	case ODIOCEJECT:
		return mcd_eject(sc);
	case CDIOCALLOW:
		return mcd_setlock(sc, MCD_LK_UNLOCK);
	case CDIOCPREVENT:
		return mcd_setlock(sc, MCD_LK_LOCK);
	case DIOCLOCK:
		return mcd_setlock(sc,
		    (*(int *)addr) ? MCD_LK_LOCK : MCD_LK_UNLOCK);
	case CDIOCSETDEBUG:
		sc->debug = 1;
		return 0;
	case CDIOCCLRDEBUG:
		sc->debug = 0;
		return 0;
	case CDIOCRESET:
		return mcd_hard_reset(sc);

	default:
		return ENOTTY;
	}

#ifdef DIAGNOSTIC
	panic("mcdioctl: impossible");
#endif
}

void
mcdgetdefaultlabel(struct mcd_softc *sc, struct disklabel *lp)
{

	memset(lp, 0, sizeof(struct disklabel));

	lp->d_secsize = sc->blksize;
	lp->d_ntracks = 1;
	lp->d_nsectors = 100;
	lp->d_ncylinders = (sc->disksize / 100) + 1;
	lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;

	strncpy(lp->d_typename, "Mitsumi CD-ROM", 16);
	lp->d_type = 0;	/* XXX */
	strncpy(lp->d_packname, "fictitious", 16);
	lp->d_secperunit = sc->disksize;
	lp->d_rpm = 300;
	lp->d_interleave = 1;
	lp->d_flags = D_REMOVABLE;

	lp->d_partitions[0].p_offset = 0;
	lp->d_partitions[0].p_size =
	    lp->d_secperunit * (lp->d_secsize / DEV_BSIZE);
	lp->d_partitions[0].p_fstype = FS_ISO9660;
	lp->d_partitions[RAW_PART].p_offset = 0;
	lp->d_partitions[RAW_PART].p_size =
	    lp->d_secperunit * (lp->d_secsize / DEV_BSIZE);
	lp->d_partitions[RAW_PART].p_fstype = FS_ISO9660;
	lp->d_npartitions = RAW_PART + 1;

	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = dkcksum(lp);
}

/*
 * This could have been taken from scsi/cd.c, but it is not clear
 * whether the scsi cd driver is linked in.
 */
void
mcdgetdisklabel(struct mcd_softc *sc)
{
	struct disklabel *lp = sc->sc_dk.dk_label;

	memset(sc->sc_dk.dk_cpulabel, 0, sizeof(struct cpu_disklabel));

	mcdgetdefaultlabel(sc, lp);
}

int
mcd_get_parms(struct mcd_softc *sc)
{
	struct mcd_mbox mbx;
	daddr_t size;
	int error;

	/* Send volume info command. */
	mbx.cmd.opcode = MCD_CMDGETVOLINFO;
	mbx.cmd.length = 0;
	mbx.res.length = sizeof(mbx.res.data.volinfo);
	if ((error = mcd_send(sc, &mbx, 1)) != 0)
		return error;

	if (mbx.res.data.volinfo.trk_low == 0x00 &&
	    mbx.res.data.volinfo.trk_high == 0x00)
		return EINVAL;

	/* Volinfo is OK. */
	sc->volinfo = mbx.res.data.volinfo;
	sc->blksize = MCD_BLKSIZE_COOKED;
	size = msf2hsg(sc->volinfo.vol_msf, 0);
	sc->disksize = size * (MCD_BLKSIZE_COOKED / DEV_BSIZE);
	return 0;
}

int
mcdsize(dev_t dev)
{

	/* CD-ROMs are read-only. */
	return -1;
}

int
mcddump(dev_t dev, daddr_t blkno, void *va,
    size_t size)
{

	/* Not implemented. */
	return ENXIO;
}

/*
 * Find the board and fill in the softc.
 */
int
mcd_find(bus_space_tag_t iot, bus_space_handle_t ioh, struct mcd_softc *sc)
{
	int i;
	struct mcd_mbox mbx;

        sc->sc_iot = iot;
	sc->sc_ioh = ioh;

	/* Send a reset. */
	bus_space_write_1(iot, ioh, MCD_RESET, 0);
	delay(1000000);
	/* Get any pending status and throw away. */
	for (i = 10; i; i--)
		bus_space_read_1(iot, ioh, MCD_STATUS);
	delay(1000);

	/* Send get status command. */
	mbx.cmd.opcode = MCD_CMDGETSTAT;
	mbx.cmd.length = 0;
	mbx.res.length = 0;
	if (mcd_send(sc, &mbx, 0) != 0)
		return 0;

	/* Get info about the drive. */
	mbx.cmd.opcode = MCD_CMDCONTINFO;
	mbx.cmd.length = 0;
	mbx.res.length = sizeof(mbx.res.data.continfo);
	if (mcd_send(sc, &mbx, 0) != 0)
		return 0;

	/*
	 * The following is code which is not guaranteed to work for all
	 * drives, because the meaning of the expected 'M' is not clear
	 * (M_itsumi is an obvious assumption, but I don't trust that).
	 * Also, the original hack had a bogus condition that always
	 * returned true.
	 *
	 * Note:  Which models support interrupts?  >=LU005S?
	 */
	sc->readcmd = MCD_CMDREADSINGLESPEED;
	switch (mbx.res.data.continfo.code) {
	case 'M':
		if (mbx.res.data.continfo.version <= 2)
			sc->type = "LU002S";
		else if (mbx.res.data.continfo.version <= 5)
			sc->type = "LU005S";
		else
			sc->type = "LU006S";
		break;
	case 'F':
		sc->type = "FX001";
		break;
	case 'D':
		sc->type = "FX001D";
		sc->readcmd = MCD_CMDREADDOUBLESPEED;
		break;
	default:
		/*
		 * mcd_send() says the  response looked OK but the
		 * drive type is unknown. If mcd_promisc,  match anyway.
		 */
		if (mcd_promisc != 0)
			return 0;

#ifdef MCDDEBUG
		printf("%s: unrecognized drive version %c%02x; will try to use it anyway\n",
		    device_xname(sc->sc_dev),
		    mbx.res.data.continfo.code, mbx.res.data.continfo.version);
#endif
		sc->type = 0;
		break;
	}

	return 1;

}

int
mcdprobe(device_t parent, cfdata_t match, void *aux)
{
	struct isa_attach_args *ia = aux;
	struct mcd_softc sc;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	int rv;

	if (ia->ia_nio < 1)
		return (0);
	if (ia->ia_nirq < 1)
		return (0);

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	/* Disallow wildcarded i/o address. */
	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return (0);
	if (ia->ia_irq[0].ir_irq == ISA_UNKNOWN_IRQ)
		return (0);

	/* Map i/o space */
	if (bus_space_map(iot, ia->ia_io[0].ir_addr, MCD_NPORT, 0, &ioh))
		return 0;

	sc.debug = 0;
	sc.probe = 1;

	rv = mcd_find(iot, ioh, &sc);

	bus_space_unmap(iot, ioh, MCD_NPORT);

	if (rv)	{
		ia->ia_nio = 1;
		ia->ia_io[0].ir_size = MCD_NPORT;

		ia->ia_nirq = 1;

		ia->ia_niomem = 0;
		ia->ia_ndrq = 0;
	}

	return (rv);
}

int
mcd_getreply(struct mcd_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int i;

	/* Wait until xfer port senses data ready. */
	for (i = DELAY_GETREPLY; i; i--) {
		if ((bus_space_read_1(iot, ioh, MCD_XFER) &
		    MCD_XF_STATUSUNAVAIL) == 0)
			break;
		delay(DELAY_GRANULARITY);
	}
	if (!i)
		return -1;

	/* Get the data. */
	return bus_space_read_1(iot, ioh, MCD_STATUS);
}

int
mcd_getstat(struct mcd_softc *sc)
{
	struct mcd_mbox mbx;

	mbx.cmd.opcode = MCD_CMDGETSTAT;
	mbx.cmd.length = 0;
	mbx.res.length = 0;
	return mcd_send(sc, &mbx, 1);
}

int
mcd_getresult(struct mcd_softc *sc, struct mcd_result *res)
{
	int i, x;

	if (sc->debug)
		printf("%s: mcd_getresult: %d", device_xname(sc->sc_dev),
		    res->length);

	if ((x = mcd_getreply(sc)) < 0) {
		if (sc->debug)
			printf(" timeout\n");
		else if (!sc->probe)
			printf("%s: timeout in getresult\n", device_xname(sc->sc_dev));
		return EIO;
	}
	if (sc->debug)
		printf(" %02x", (u_int)x);
	sc->status = x;
	mcd_setflags(sc);

	if ((sc->status & MCD_ST_CMDCHECK) != 0)
		return EINVAL;

	for (i = 0; i < res->length; i++) {
		if ((x = mcd_getreply(sc)) < 0) {
			if (sc->debug)
				printf(" timeout\n");
			else
				printf("%s: timeout in getresult\n", device_xname(sc->sc_dev));
			return EIO;
		}
		if (sc->debug)
			printf(" %02x", (u_int)x);
		res->data.raw.data[i] = x;
	}

	if (sc->debug)
		printf(" succeeded\n");

#ifdef MCDDEBUG
	delay(10);
	while ((bus_space_read_1(sc->sc_iot, sc->sc_ioh, MCD_XFER) &
	    MCD_XF_STATUSUNAVAIL) == 0) {
		x = bus_space_read_1(sc->sc_iot, sc->sc_ioh, MCD_STATUS);
		printf("%s: got extra byte %02x during getstatus\n",
		    device_xname(sc->sc_dev), (u_int)x);
		delay(10);
	}
#endif

	return 0;
}

void
mcd_setflags(struct mcd_softc *sc)
{

	/* Check flags. */
	if ((sc->flags & MCDF_LOADED) != 0 &&
	    (sc->status & (MCD_ST_DSKCHNG | MCD_ST_DSKIN | MCD_ST_DOOROPEN)) !=
	    MCD_ST_DSKIN) {
		if ((sc->status & MCD_ST_DOOROPEN) != 0)
			printf("%s: door open\n", device_xname(sc->sc_dev));
		else if ((sc->status & MCD_ST_DSKIN) == 0)
			printf("%s: no disk present\n", device_xname(sc->sc_dev));
		else if ((sc->status & MCD_ST_DSKCHNG) != 0)
			printf("%s: media change\n", device_xname(sc->sc_dev));
		sc->flags &= ~MCDF_LOADED;
	}

	if ((sc->status & MCD_ST_AUDIOBSY) != 0)
		sc->audio_status = CD_AS_PLAY_IN_PROGRESS;
	else if (sc->audio_status == CD_AS_PLAY_IN_PROGRESS ||
		 sc->audio_status == CD_AS_AUDIO_INVALID)
		sc->audio_status = CD_AS_PLAY_COMPLETED;
}

int
mcd_send(struct mcd_softc *sc, struct mcd_mbox *mbx, int diskin)
{
	int retry, i, error;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	if (sc->debug) {
		printf("%s: mcd_send: %d %02x", device_xname(sc->sc_dev),
		    mbx->cmd.length, (u_int)mbx->cmd.opcode);
		for (i = 0; i < mbx->cmd.length; i++)
			printf(" %02x", (u_int)mbx->cmd.data.raw.data[i]);
		printf("\n");
	}

	for (retry = MCD_RETRIES; retry; retry--) {
		bus_space_write_1(iot, ioh, MCD_COMMAND, mbx->cmd.opcode);
		for (i = 0; i < mbx->cmd.length; i++)
			bus_space_write_1(iot, ioh, MCD_COMMAND, mbx->cmd.data.raw.data[i]);
		if ((error = mcd_getresult(sc, &mbx->res)) == 0)
			break;
		if (error == EINVAL)
			return error;
	}
	if (!retry)
		return error;
	if (diskin && (sc->flags & MCDF_LOADED) == 0)
		return EIO;

	return 0;
}

static int
bcd2bin(bcd_t b)
{

	return (b >> 4) * 10 + (b & 15);
}

static bcd_t
bin2bcd(int b)
{

	return ((b / 10) << 4) | (b % 10);
}

static void
hsg2msf(int hsg, bcd_t *msf)
{

	hsg += 150;
	F_msf(msf) = bin2bcd(hsg % 75);
	hsg /= 75;
	S_msf(msf) = bin2bcd(hsg % 60);
	hsg /= 60;
	M_msf(msf) = bin2bcd(hsg);
}

static daddr_t
msf2hsg(bcd_t *msf, int relative)
{
	daddr_t blkno;

	blkno = bcd2bin(M_msf(msf)) * 75 * 60 +
		bcd2bin(S_msf(msf)) * 75 +
		bcd2bin(F_msf(msf));
	if (!relative)
		blkno -= 150;
	return blkno;
}

void
mcd_pseudointr(void *v)
{
	struct mcd_softc *sc = v;
	int s;

	s = splbio();
	(void) mcdintr(sc);
	splx(s);
}

/*
 * State machine to process read requests.
 * Initialize with MCD_S_BEGIN: calculate sizes, and set mode
 * MCD_S_WAITMODE: waits for status reply from set mode, set read command
 * MCD_S_WAITREAD: wait for read ready, read data.
 */
int
mcdintr(void *arg)
{
	struct mcd_softc *sc = arg;
	struct mcd_mbx *mbx = &sc->mbx;
	struct buf *bp = mbx->bp;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	int i;
	u_char x;
	bcd_t msf[3];

	switch (mbx->state) {
	case MCD_S_IDLE:
		return 0;

	case MCD_S_BEGIN:
	tryagain:
		if (mbx->mode == sc->lastmode)
			goto firstblock;

		sc->lastmode = MCD_MD_UNKNOWN;
		bus_space_write_1(iot, ioh, MCD_COMMAND, MCD_CMDSETMODE);
		bus_space_write_1(iot, ioh, MCD_COMMAND, mbx->mode);

		mbx->count = RDELAY_WAITMODE;
		mbx->state = MCD_S_WAITMODE;

	case MCD_S_WAITMODE:
		callout_stop(&sc->sc_pintr_ch);
		for (i = 20; i; i--) {
			x = bus_space_read_1(iot, ioh, MCD_XFER);
			if ((x & MCD_XF_STATUSUNAVAIL) == 0)
				break;
			delay(50);
		}
		if (i == 0)
			goto hold;
		sc->status = bus_space_read_1(iot, ioh, MCD_STATUS);
		mcd_setflags(sc);
		if ((sc->flags & MCDF_LOADED) == 0)
			goto changed;
		MCD_TRACE("doread: got WAITMODE delay=%d\n",
		    RDELAY_WAITMODE - mbx->count);

		sc->lastmode = mbx->mode;

	firstblock:
		MCD_TRACE("doread: read blkno=%d for bp=0x%p\n", 
		    (int) mbx->blkno, bp);

		/* Build parameter block. */
		hsg2msf(mbx->blkno, msf);

		/* Send the read command. */
		bus_space_write_1(iot, ioh, MCD_COMMAND, sc->readcmd);
		bus_space_write_1(iot, ioh, MCD_COMMAND, msf[0]);
		bus_space_write_1(iot, ioh, MCD_COMMAND, msf[1]);
		bus_space_write_1(iot, ioh, MCD_COMMAND, msf[2]);
		bus_space_write_1(iot, ioh, MCD_COMMAND, 0);
		bus_space_write_1(iot, ioh, MCD_COMMAND, 0);
		bus_space_write_1(iot, ioh, MCD_COMMAND, mbx->nblk);

		mbx->count = RDELAY_WAITREAD;
		mbx->state = MCD_S_WAITREAD;

	case MCD_S_WAITREAD:
		callout_stop(&sc->sc_pintr_ch);
	nextblock:
	loop:
		for (i = 20; i; i--) {
			x = bus_space_read_1(iot, ioh, MCD_XFER);
			if ((x & MCD_XF_DATAUNAVAIL) == 0)
				goto gotblock;
			if ((x & MCD_XF_STATUSUNAVAIL) == 0)
				break;
			delay(50);
		}
		if (i == 0)
			goto hold;
		sc->status = bus_space_read_1(iot, ioh, MCD_STATUS);
		mcd_setflags(sc);
		if ((sc->flags & MCDF_LOADED) == 0)
			goto changed;
#if 0
		printf("%s: got status byte %02x during read\n",
		    device_xname(sc->sc_dev), (u_int)sc->status);
#endif
		goto loop;

	gotblock:
		MCD_TRACE("doread: got data delay=%d\n",
		    RDELAY_WAITREAD - mbx->count);

		/* Data is ready. */
		bus_space_write_1(iot, ioh, MCD_CTL2, 0x04);	/* XXX */
		bus_space_read_multi_1(iot, ioh, MCD_RDATA,
		    (char *)bp->b_data + mbx->skip, mbx->sz);
		bus_space_write_1(iot, ioh, MCD_CTL2, 0x0c);	/* XXX */
		mbx->blkno += 1;
		mbx->skip += mbx->sz;
		if (--mbx->nblk > 0)
			goto nextblock;

		mbx->state = MCD_S_IDLE;

		/* Return buffer. */
		bp->b_resid = 0;
		disk_unbusy(&sc->sc_dk, bp->b_bcount, (bp->b_flags & B_READ));
		biodone(bp);

		mcdstart(sc);
		return 1;

	hold:
		if (mbx->count-- < 0) {
			printf("%s: timeout in state %d",
			    device_xname(sc->sc_dev), mbx->state);
			goto readerr;
		}

#if 0
		printf("%s: sleep in state %d\n", device_xname(sc->sc_dev),
		    mbx->state);
#endif
		callout_reset(&sc->sc_pintr_ch, hz / 100,
		    mcd_pseudointr, sc);
		return -1;
	}

readerr:
	if (mbx->retry-- > 0) {
		printf("; retrying\n");
		goto tryagain;
	} else
		printf("; giving up\n");

changed:
	/* Invalidate the buffer. */
	bp->b_error = EIO;
	bp->b_resid = bp->b_bcount - mbx->skip;
	disk_unbusy(&sc->sc_dk, (bp->b_bcount - bp->b_resid),
	    (bp->b_flags & B_READ));
	biodone(bp);

	mcdstart(sc);
	return -1;

#ifdef notyet
	printf("%s: unit timeout; resetting\n", device_xname(sc->sc_dev));
	bus_space_write_1(iot, ioh, MCD_RESET, MCD_CMDRESET);
	delay(300000);
	(void) mcd_getstat(sc, 1);
	(void) mcd_getstat(sc, 1);
	/*sc->status &= ~MCD_ST_DSKCHNG; */
	sc->debug = 1; /* preventive set debug mode */
#endif
}

void
mcd_soft_reset(struct mcd_softc *sc)
{

	sc->debug = 0;
	sc->flags = 0;
	sc->lastmode = MCD_MD_UNKNOWN;
	sc->lastupc = MCD_UPC_UNKNOWN;
	sc->audio_status = CD_AS_AUDIO_INVALID;
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, MCD_CTL2, 0x0c); /* XXX */
}

int
mcd_hard_reset(struct mcd_softc *sc)
{
	struct mcd_mbox mbx;

	mcd_soft_reset(sc);

	mbx.cmd.opcode = MCD_CMDRESET;
	mbx.cmd.length = 0;
	mbx.res.length = 0;
	return mcd_send(sc, &mbx, 0);
}

int
mcd_setmode(struct mcd_softc *sc, int mode)
{
	struct mcd_mbox mbx;
	int error;

	if (sc->lastmode == mode)
		return 0;
	if (sc->debug)
		printf("%s: setting mode to %d\n", device_xname(sc->sc_dev), mode);
	sc->lastmode = MCD_MD_UNKNOWN;

	mbx.cmd.opcode = MCD_CMDSETMODE;
	mbx.cmd.length = sizeof(mbx.cmd.data.datamode);
	mbx.cmd.data.datamode.mode = mode;
	mbx.res.length = 0;
	if ((error = mcd_send(sc, &mbx, 1)) != 0)
		return error;

	sc->lastmode = mode;
	return 0;
}

int
mcd_setupc(struct mcd_softc *sc, int upc)
{
	struct mcd_mbox mbx;
	int error;

	if (sc->lastupc == upc)
		return 0;
	if (sc->debug)
		printf("%s: setting upc to %d\n", device_xname(sc->sc_dev), upc);
	sc->lastupc = MCD_UPC_UNKNOWN;

	mbx.cmd.opcode = MCD_CMDCONFIGDRIVE;
	mbx.cmd.length = sizeof(mbx.cmd.data.config) - 1;
	mbx.cmd.data.config.subcommand = MCD_CF_READUPC;
	mbx.cmd.data.config.data1 = upc;
	mbx.res.length = 0;
	if ((error = mcd_send(sc, &mbx, 1)) != 0)
		return error;

	sc->lastupc = upc;
	return 0;
}

int
mcd_toc_header(struct mcd_softc *sc, struct ioc_toc_header *th)
{

	if (sc->debug)
		printf("%s: mcd_toc_header: reading toc header\n",
		    device_xname(sc->sc_dev));

	th->len = msf2hsg(sc->volinfo.vol_msf, 0);
	th->starting_track = bcd2bin(sc->volinfo.trk_low);
	th->ending_track = bcd2bin(sc->volinfo.trk_high);

	return 0;
}

int
mcd_read_toc(struct mcd_softc *sc)
{
	struct ioc_toc_header th;
	union mcd_qchninfo q;
	int error, trk, idx, retry;

	if ((error = mcd_toc_header(sc, &th)) != 0)
		return error;

	if ((error = mcd_stop(sc)) != 0)
		return error;

	if (sc->debug)
		printf("%s: read_toc: reading qchannel info\n",
		    device_xname(sc->sc_dev));

	for (trk = th.starting_track; trk <= th.ending_track; trk++)
		sc->toc[trk].toc.idx_no = 0x00;
	trk = th.ending_track - th.starting_track + 1;
	for (retry = 300; retry && trk > 0; retry--) {
		if (mcd_getqchan(sc, &q, CD_TRACK_INFO) != 0)
			break;
		if (q.toc.trk_no != 0x00 || q.toc.idx_no == 0x00)
			continue;
		idx = bcd2bin(q.toc.idx_no);
		if (idx < MCD_MAXTOCS &&
		    sc->toc[idx].toc.idx_no == 0x00) {
			sc->toc[idx] = q;
			trk--;
		}
	}

	/* Inform the drive that we're finished so it turns off the light. */
	if ((error = mcd_setmode(sc, MCD_MD_COOKED)) != 0)
		return error;

	if (trk != 0)
		return EINVAL;

	/* Add a fake last+1 for mcd_playtracks(). */
	idx = th.ending_track + 1;
	sc->toc[idx].toc.control = sc->toc[idx-1].toc.control;
	sc->toc[idx].toc.addr_type = sc->toc[idx-1].toc.addr_type;
	sc->toc[idx].toc.trk_no = 0x00;
	sc->toc[idx].toc.idx_no = 0xaa;
	sc->toc[idx].toc.absolute_pos[0] = sc->volinfo.vol_msf[0];
	sc->toc[idx].toc.absolute_pos[1] = sc->volinfo.vol_msf[1];
	sc->toc[idx].toc.absolute_pos[2] = sc->volinfo.vol_msf[2];

	return 0;
}

int
mcd_toc_entries(struct mcd_softc *sc, struct ioc_read_toc_entry *te, struct cd_toc_entry *entries, int *count)
{
	int len = te->data_len;
	struct ioc_toc_header header;
	u_char trk;
	daddr_t lba;
	int error, n;

	if (len < sizeof(struct cd_toc_entry))
		return EINVAL;
	if (te->address_format != CD_MSF_FORMAT &&
	    te->address_format != CD_LBA_FORMAT)
		return EINVAL;

	/* Copy the TOC header. */
	if ((error = mcd_toc_header(sc, &header)) != 0)
		return error;

	/* Verify starting track. */
	trk = te->starting_track;
	if (trk == 0x00)
		trk = header.starting_track;
	else if (trk == 0xaa)
		trk = header.ending_track + 1;
	else if (trk < header.starting_track ||
		 trk > header.ending_track + 1)
		return EINVAL;

	/* Copy the TOC data. */
	for (n = 0; trk <= header.ending_track + 1; n++, trk++) {
		if (n * sizeof entries[0] > len)
			break;
		if (sc->toc[trk].toc.idx_no == 0x00)
			continue;
		entries[n].control = sc->toc[trk].toc.control;
		entries[n].addr_type = sc->toc[trk].toc.addr_type;
		entries[n].track = bcd2bin(sc->toc[trk].toc.idx_no);
		switch (te->address_format) {
		case CD_MSF_FORMAT:
			entries[n].addr.addr[0] = 0;
			entries[n].addr.addr[1] = bcd2bin(sc->toc[trk].toc.absolute_pos[0]);
			entries[n].addr.addr[2] = bcd2bin(sc->toc[trk].toc.absolute_pos[1]);
			entries[n].addr.addr[3] = bcd2bin(sc->toc[trk].toc.absolute_pos[2]);
			break;
		case CD_LBA_FORMAT:
			lba = msf2hsg(sc->toc[trk].toc.absolute_pos, 0);
			entries[n].addr.addr[0] = lba >> 24;
			entries[n].addr.addr[1] = lba >> 16;
			entries[n].addr.addr[2] = lba >> 8;
			entries[n].addr.addr[3] = lba;
			break;
		}
	}

	*count = n;
	return 0;
}

int
mcd_stop(struct mcd_softc *sc)
{
	struct mcd_mbox mbx;
	int error;

	if (sc->debug)
		printf("%s: mcd_stop: stopping play\n", device_xname(sc->sc_dev));

	mbx.cmd.opcode = MCD_CMDSTOPAUDIO;
	mbx.cmd.length = 0;
	mbx.res.length = 0;
	if ((error = mcd_send(sc, &mbx, 1)) != 0)
		return error;

	sc->audio_status = CD_AS_PLAY_COMPLETED;
	return 0;
}

int
mcd_getqchan(struct mcd_softc *sc, union mcd_qchninfo *q, int qchn)
{
	struct mcd_mbox mbx;
	int error;

	if (qchn == CD_TRACK_INFO) {
		if ((error = mcd_setmode(sc, MCD_MD_TOC)) != 0)
			return error;
	} else {
		if ((error = mcd_setmode(sc, MCD_MD_COOKED)) != 0)
			return error;
	}
	if (qchn == CD_MEDIA_CATALOG) {
		if ((error = mcd_setupc(sc, MCD_UPC_ENABLE)) != 0)
			return error;
	} else {
		if ((error = mcd_setupc(sc, MCD_UPC_DISABLE)) != 0)
			return error;
	}

	mbx.cmd.opcode = MCD_CMDGETQCHN;
	mbx.cmd.length = 0;
	mbx.res.length = sizeof(mbx.res.data.qchninfo);
	if ((error = mcd_send(sc, &mbx, 1)) != 0)
		return error;

	*q = mbx.res.data.qchninfo;
	return 0;
}

int
mcd_read_subchannel(struct mcd_softc *sc, struct ioc_read_subchannel *ch, struct cd_sub_channel_info *info)
{
	int len = ch->data_len;
	union mcd_qchninfo q;
	daddr_t lba;
	int error;

	if (sc->debug)
		printf("%s: subchan: af=%d df=%d\n", device_xname(sc->sc_dev),
		    ch->address_format, ch->data_format);

	if (len > sizeof(*info) || len < sizeof(info->header))
		return EINVAL;
	if (ch->address_format != CD_MSF_FORMAT &&
	    ch->address_format != CD_LBA_FORMAT)
		return EINVAL;
	if (ch->data_format != CD_CURRENT_POSITION &&
	    ch->data_format != CD_MEDIA_CATALOG)
		return EINVAL;

	if ((error = mcd_getqchan(sc, &q, ch->data_format)) != 0)
		return error;

	info->header.audio_status = sc->audio_status;
	info->what.media_catalog.data_format = ch->data_format;

	switch (ch->data_format) {
	case CD_MEDIA_CATALOG:
		info->what.media_catalog.mc_valid = 1;
#if 0
		info->what.media_catalog.mc_number =
#endif
		break;

	case CD_CURRENT_POSITION:
		info->what.position.track_number = bcd2bin(q.current.trk_no);
		info->what.position.index_number = bcd2bin(q.current.idx_no);
		switch (ch->address_format) {
		case CD_MSF_FORMAT:
			info->what.position.reladdr.addr[0] = 0;
			info->what.position.reladdr.addr[1] = bcd2bin(q.current.relative_pos[0]);
			info->what.position.reladdr.addr[2] = bcd2bin(q.current.relative_pos[1]);
			info->what.position.reladdr.addr[3] = bcd2bin(q.current.relative_pos[2]);
			info->what.position.absaddr.addr[0] = 0;
			info->what.position.absaddr.addr[1] = bcd2bin(q.current.absolute_pos[0]);
			info->what.position.absaddr.addr[2] = bcd2bin(q.current.absolute_pos[1]);
			info->what.position.absaddr.addr[3] = bcd2bin(q.current.absolute_pos[2]);
			break;
		case CD_LBA_FORMAT:
			lba = msf2hsg(q.current.relative_pos, 1);
			/*
			 * Pre-gap has index number of 0, and decreasing MSF
			 * address.  Must be converted to negative LBA, per
			 * SCSI spec.
			 */
			if (info->what.position.index_number == 0x00)
				lba = -lba;
			info->what.position.reladdr.addr[0] = lba >> 24;
			info->what.position.reladdr.addr[1] = lba >> 16;
			info->what.position.reladdr.addr[2] = lba >> 8;
			info->what.position.reladdr.addr[3] = lba;
			lba = msf2hsg(q.current.absolute_pos, 0);
			info->what.position.absaddr.addr[0] = lba >> 24;
			info->what.position.absaddr.addr[1] = lba >> 16;
			info->what.position.absaddr.addr[2] = lba >> 8;
			info->what.position.absaddr.addr[3] = lba;
			break;
		}
		break;
	}

	return 0;
}

int
mcd_playtracks(struct mcd_softc *sc, struct ioc_play_track *p)
{
	struct mcd_mbox mbx;
	int a = p->start_track;
	int z = p->end_track;
	int error;

	if (sc->debug)
		printf("%s: playtracks: from %d:%d to %d:%d\n",
		    device_xname(sc->sc_dev),
		    a, p->start_index, z, p->end_index);

	if (a < bcd2bin(sc->volinfo.trk_low) ||
	    a > bcd2bin(sc->volinfo.trk_high) ||
	    a > z ||
	    z < bcd2bin(sc->volinfo.trk_low) ||
	    z > bcd2bin(sc->volinfo.trk_high))
		return EINVAL;

	if ((error = mcd_setmode(sc, MCD_MD_COOKED)) != 0)
		return error;

	mbx.cmd.opcode = MCD_CMDREADSINGLESPEED;
	mbx.cmd.length = sizeof(mbx.cmd.data.play);
	mbx.cmd.data.play.start_msf[0] = sc->toc[a].toc.absolute_pos[0];
	mbx.cmd.data.play.start_msf[1] = sc->toc[a].toc.absolute_pos[1];
	mbx.cmd.data.play.start_msf[2] = sc->toc[a].toc.absolute_pos[2];
	mbx.cmd.data.play.end_msf[0] = sc->toc[z+1].toc.absolute_pos[0];
	mbx.cmd.data.play.end_msf[1] = sc->toc[z+1].toc.absolute_pos[1];
	mbx.cmd.data.play.end_msf[2] = sc->toc[z+1].toc.absolute_pos[2];
	sc->lastpb = mbx.cmd;
	mbx.res.length = 0;
	return mcd_send(sc, &mbx, 1);
}

int
mcd_playmsf(struct mcd_softc *sc, struct ioc_play_msf *p)
{
	struct mcd_mbox mbx;
	int error;

	if (sc->debug)
		printf("%s: playmsf: from %d:%d.%d to %d:%d.%d\n",
		    device_xname(sc->sc_dev),
		    p->start_m, p->start_s, p->start_f,
		    p->end_m, p->end_s, p->end_f);

	if ((p->start_m * 60 * 75 + p->start_s * 75 + p->start_f) >=
	    (p->end_m * 60 * 75 + p->end_s * 75 + p->end_f))
		return EINVAL;

	if ((error = mcd_setmode(sc, MCD_MD_COOKED)) != 0)
		return error;

	mbx.cmd.opcode = MCD_CMDREADSINGLESPEED;
	mbx.cmd.length = sizeof(mbx.cmd.data.play);
	mbx.cmd.data.play.start_msf[0] = bin2bcd(p->start_m);
	mbx.cmd.data.play.start_msf[1] = bin2bcd(p->start_s);
	mbx.cmd.data.play.start_msf[2] = bin2bcd(p->start_f);
	mbx.cmd.data.play.end_msf[0] = bin2bcd(p->end_m);
	mbx.cmd.data.play.end_msf[1] = bin2bcd(p->end_s);
	mbx.cmd.data.play.end_msf[2] = bin2bcd(p->end_f);
	sc->lastpb = mbx.cmd;
	mbx.res.length = 0;
	return mcd_send(sc, &mbx, 1);
}

int
mcd_playblocks(struct mcd_softc *sc, struct ioc_play_blocks *p)
{
	struct mcd_mbox mbx;
	int error;

	if (sc->debug)
		printf("%s: playblocks: blkno %d length %d\n",
		    device_xname(sc->sc_dev), p->blk, p->len);

	if (p->blk > sc->disksize || p->len > sc->disksize ||
	    (p->blk + p->len) > sc->disksize)
		return 0;

	if ((error = mcd_setmode(sc, MCD_MD_COOKED)) != 0)
		return error;

	mbx.cmd.opcode = MCD_CMDREADSINGLESPEED;
	mbx.cmd.length = sizeof(mbx.cmd.data.play);
	hsg2msf(p->blk, mbx.cmd.data.play.start_msf);
	hsg2msf(p->blk + p->len, mbx.cmd.data.play.end_msf);
	sc->lastpb = mbx.cmd;
	mbx.res.length = 0;
	return mcd_send(sc, &mbx, 1);
}

int
mcd_pause(struct mcd_softc *sc)
{
	union mcd_qchninfo q;
	int error;

	/* Verify current status. */
	if (sc->audio_status != CD_AS_PLAY_IN_PROGRESS)	{
		printf("%s: pause: attempted when not playing\n",
		    device_xname(sc->sc_dev));
		return EINVAL;
	}

	/* Get the current position. */
	if ((error = mcd_getqchan(sc, &q, CD_CURRENT_POSITION)) != 0)
		return error;

	/* Copy it into lastpb. */
	sc->lastpb.data.seek.start_msf[0] = q.current.absolute_pos[0];
	sc->lastpb.data.seek.start_msf[1] = q.current.absolute_pos[1];
	sc->lastpb.data.seek.start_msf[2] = q.current.absolute_pos[2];

	/* Stop playing. */
	if ((error = mcd_stop(sc)) != 0)
		return error;

	/* Set the proper status and exit. */
	sc->audio_status = CD_AS_PLAY_PAUSED;
	return 0;
}

int
mcd_resume(struct mcd_softc *sc)
{
	struct mcd_mbox mbx;
	int error;

	if (sc->audio_status != CD_AS_PLAY_PAUSED)
		return EINVAL;

	if ((error = mcd_setmode(sc, MCD_MD_COOKED)) != 0)
		return error;

	mbx.cmd = sc->lastpb;
	mbx.res.length = 0;
	return mcd_send(sc, &mbx, 1);
}

int
mcd_eject(struct mcd_softc *sc)
{
	struct mcd_mbox mbx;

	mbx.cmd.opcode = MCD_CMDEJECTDISK;
	mbx.cmd.length = 0;
	mbx.res.length = 0;
	return mcd_send(sc, &mbx, 0);
}

int
mcd_setlock(struct mcd_softc *sc, int mode)
{
	struct mcd_mbox mbx;

	mbx.cmd.opcode = MCD_CMDSETLOCK;
	mbx.cmd.length = sizeof(mbx.cmd.data.lockmode);
	mbx.cmd.data.lockmode.mode = mode;
	mbx.res.length = 0;
	return mcd_send(sc, &mbx, 1);
}
