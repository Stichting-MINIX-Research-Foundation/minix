/*	$NetBSD: wt.c,v 1.86 2014/07/25 08:10:37 dholland Exp $	*/

/*
 * Streamer tape driver.
 * Supports Archive and Wangtek compatible QIC-02/QIC-36 boards.
 *
 * Copyright (C) 1993 by:
 *	Sergey Ryzhkov <sir@kiae.su>
 *	Serge Vakulenko <vak@zebub.msk.su>
 *
 * This software is distributed with NO WARRANTIES, not even the implied
 * warranties for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Authors grant any other persons or organisations permission to use
 * or modify this software as long as this message is kept with the software,
 * all derivative works or modified versions.
 *
 * This driver is derived from the old 386bsd Wangtek streamer tape driver,
 * made by Robert Baron at CMU, based on Intel sources.
 */

/*
 * Copyright (c) 1989 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Robert Baron
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 *  Copyright 1988, 1989 by Intel Corporation
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: wt.c,v 1.86 2014/07/25 08:10:37 dholland Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>
#include <sys/mtio.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/lwp.h>
#include <sys/conf.h>

#include <sys/intr.h>
#include <sys/bus.h>
#include <machine/pio.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>
#include <dev/isa/wtreg.h>

/*
 * Uncomment this to enable internal device tracing.
 */
#define WTDBPRINT(x)		/* printf x */

#define WTPRI			(PZERO+10)	/* sleep priority */

#define WT_NPORT		2		/* 2 i/o ports */
#define AV_NPORT		4		/* 4 i/o ports */

enum wttype {
	UNKNOWN = 0,	/* unknown type, driver disabled */
	ARCHIVE,	/* Archive Viper SC499, SC402 etc */
	WANGTEK,	/* Wangtek */
};

static struct wtregs {
	/* controller ports */
	int DATAPORT,	/* data, read only */
	CMDPORT,	/* command, write only */
	STATPORT,	/* status, read only */
	CTLPORT,	/* control, write only */
	SDMAPORT,	/* start DMA */
	RDMAPORT;	/* reset DMA */
	/* status port bits */
	u_char BUSY,	/* not ready bit define */
	NOEXCEP,	/* no exception bit define */
	RESETMASK,	/* to check after reset */
	RESETVAL,	/* state after reset */
	/* control port bits */
	ONLINE,		/* device selected */
	RESET,		/* reset command */
	REQUEST,	/* request command */
	IEN;		/* enable interrupts */
} wtregs = {
	1, 1, 0, 0, 0, 0,
	0x01, 0x02, 0x07, 0x05,
	0x01, 0x02, 0x04, 0x08
}, avregs = {
	0, 0, 1, 1, 2, 3,
	0x40, 0x20, 0xf8, 0x50,
	0, 0x80, 0x40, 0x20
};

struct wt_softc {
	device_t sc_dev;
	void *sc_ih;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	isa_chipset_tag_t	sc_ic;

	callout_t		sc_timer_ch;

	enum wttype type;	/* type of controller */
	int chan;		/* DMA channel number, 1..3 */
	int flags;		/* state of tape drive */
	unsigned dens;		/* tape density */
	int bsize;		/* tape block size */
	void *buf;		/* internal i/o buffer */

	void *dmavaddr;		/* virtual address of DMA i/o buffer */
	size_t dmatotal;	/* size of i/o buffer */
	int dmaflags;		/* i/o direction */
	size_t dmacount;	/* resulting length of DMA i/o */

	u_short error;		/* code for error encountered */
	u_short ercnt;		/* number of error blocks */
	u_short urcnt;		/* number of underruns */

	struct wtregs regs;
};

static dev_type_open(wtopen);
static dev_type_close(wtclose);
static dev_type_read(wtread);
static dev_type_write(wtwrite);
static dev_type_ioctl(wtioctl);
static dev_type_strategy(wtstrategy);
static dev_type_dump(wtdump);
static dev_type_size(wtsize);

const struct bdevsw wt_bdevsw = {
	.d_open = wtopen,
	.d_close = wtclose,
	.d_strategy = wtstrategy,
	.d_ioctl = wtioctl,
	.d_dump = wtdump,
	.d_psize = wtsize,
	.d_discard = nodiscard,
	.d_flag = D_TAPE
};

const struct cdevsw wt_cdevsw = {
	.d_open = wtopen,
	.d_close = wtclose,
	.d_read = wtread,
	.d_write = wtwrite,
	.d_ioctl = wtioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_TAPE
};

static int	wtwait(struct wt_softc *sc, int catch, const char *msg);
static int	wtcmd(struct wt_softc *sc, int cmd);
static int	wtstart(struct wt_softc *sc, int flag, void *vaddr, size_t len);
static void	wtdma(struct wt_softc *sc);
static void	wttimer(void *arg);
static void	wtclock(struct wt_softc *sc);
static int	wtreset(bus_space_tag_t, bus_space_handle_t, struct wtregs *);
static int	wtsense(struct wt_softc *sc, int verbose, int ignore);
static int	wtstatus(struct wt_softc *sc);
static void	wtrewind(struct wt_softc *sc);
static int	wtreadfm(struct wt_softc *sc);
static int	wtwritefm(struct wt_softc *sc);
static u_char	wtsoft(struct wt_softc *sc, int mask, int bits);
static int	wtintr(void *sc);

int	wtprobe(device_t, cfdata_t, void *);
void	wtattach(device_t, device_t, void *);

CFATTACH_DECL_NEW(wt, sizeof(struct wt_softc),
    wtprobe, wtattach, NULL, NULL);

extern struct cfdriver wt_cd;

/*
 * Probe for the presence of the device.
 */
int
wtprobe(device_t parent, cfdata_t match, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	int rv = 0, iosize;

	if (ia->ia_nio < 1)
		return (0);
	if (ia->ia_nirq < 1)
		return (0);
	if (ia->ia_ndrq < 1)
		return (0);

	/* Disallow wildcarded i/o address. */
	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return (0);
	if (ia->ia_irq[0].ir_irq == ISA_UNKNOWN_IRQ)
		return (0);

	if (ia->ia_drq[0].ir_drq < 1 || ia->ia_drq[0].ir_drq > 3) {
		printf("wtprobe: Bad drq=%d, should be 1..3\n",
		    ia->ia_drq[0].ir_drq);
		return (0);
	}

	iosize = AV_NPORT;

	/* Map i/o space */
	if (bus_space_map(iot, ia->ia_io[0].ir_addr, iosize, 0, &ioh))
		return 0;

	/* Try Wangtek. */
	if (wtreset(iot, ioh, &wtregs)) {
		iosize = WT_NPORT; /* XXX misleading */
		rv = 1;
		goto done;
	}

	/* Try Archive. */
	if (wtreset(iot, ioh, &avregs)) {
		iosize = AV_NPORT;
		rv = 1;
		goto done;
	}

done:
	if (rv) {
		ia->ia_nio = 1;
		ia->ia_io[0].ir_size = iosize;

		ia->ia_nirq = 1;
		ia->ia_ndrq = 1;

		ia->ia_niomem = 0;
	}
	bus_space_unmap(iot, ioh, AV_NPORT);
	return rv;
}

/*
 * Device is found, configure it.
 */
void
wtattach(device_t parent, device_t self, void *aux)
{
	struct wt_softc *sc = device_private(self);
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	bus_size_t maxsize;

	sc->sc_dev = self;

	/* Map i/o space */
	if (bus_space_map(iot, ia->ia_io[0].ir_addr, AV_NPORT, 0, &ioh)) {
		printf(": can't map i/o space\n");
		return;
	}

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	sc->sc_ic = ia->ia_ic;

	callout_init(&sc->sc_timer_ch, 0);

	/* Try Wangtek. */
	if (wtreset(iot, ioh, &wtregs)) {
		sc->type = WANGTEK;
		memcpy(&sc->regs, &wtregs, sizeof(sc->regs));
		printf(": type <Wangtek>\n");
		goto ok;
	}

	/* Try Archive. */
	if (wtreset(iot, ioh, &avregs)) {
		sc->type = ARCHIVE;
		memcpy(&sc->regs, &avregs, sizeof(sc->regs));
		printf(": type <Archive>\n");
		/* Reset DMA. */
		bus_space_write_1(iot, ioh, sc->regs.RDMAPORT, 0);
		goto ok;
	}

	/* what happened? */
	aprint_error_dev(self, "lost controller\n");
	return;

ok:
	sc->flags = TPSTART;		/* tape is rewound */
	sc->dens = -1;			/* unknown density */

	sc->chan = ia->ia_drq[0].ir_drq;

	if ((maxsize = isa_dmamaxsize(sc->sc_ic, sc->chan)) < MAXPHYS) {
		aprint_error_dev(sc->sc_dev, "max DMA size %lu is less than required %d\n",
		    (u_long)maxsize, MAXPHYS);
		return;
	}

	if (isa_drq_alloc(sc->sc_ic, sc->chan) != 0) {
		aprint_error_dev(sc->sc_dev, "can't reserve drq %d\n",
		    sc->chan);
		return;
	}

	if (isa_dmamap_create(sc->sc_ic, sc->chan, MAXPHYS,
	    BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW)) {
		aprint_error_dev(sc->sc_dev, "can't set up ISA DMA map\n");
		return;
	}

	sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq[0].ir_irq,
	    IST_EDGE, IPL_BIO, wtintr, sc);
}

static int
wtdump(dev_t dev, daddr_t blkno, void *va,
    size_t size)
{

	/* Not implemented. */
	return ENXIO;
}

static int
wtsize(dev_t dev)
{

	/* Not implemented. */
	return -1;
}

/*
 * Open routine, called on every device open.
 */
static int
wtopen(dev_t dev, int flag, int mode, struct lwp *l)
{
	int unit = minor(dev) & T_UNIT;
	struct wt_softc *sc;
	int error;

	sc = device_lookup_private(&wt_cd, unit);
	if (sc == NULL)
		return (ENXIO);

	/* Check that device is not in use */
	if (sc->flags & TPINUSE)
		return EBUSY;

	/* If the tape is in rewound state, check the status and set density. */
	if (sc->flags & TPSTART) {
		/* If rewind is going on, wait */
		if ((error = wtwait(sc, PCATCH, "wtrew")) != 0)
			return error;

		/* Check the controller status */
		if (!wtsense(sc, 0, (flag & FWRITE) ? 0 : TP_WRP)) {
			/* Bad status, reset the controller. */
			if (!wtreset(sc->sc_iot, sc->sc_ioh, &sc->regs))
				return EIO;
			if (!wtsense(sc, 1, (flag & FWRITE) ? 0 : TP_WRP))
				return EIO;
		}

		/* Set up tape density. */
		if (sc->dens != (minor(dev) & WT_DENSEL)) {
			int d = 0;

			switch (minor(dev) & WT_DENSEL) {
			case WT_DENSDFLT:
			default:
				break;			/* default density */
			case WT_QIC11:
				d = QIC_FMT11;  break;	/* minor 010 */
			case WT_QIC24:
				d = QIC_FMT24;  break;	/* minor 020 */
			case WT_QIC120:
				d = QIC_FMT120; break;	/* minor 030 */
			case WT_QIC150:
				d = QIC_FMT150; break;	/* minor 040 */
			case WT_QIC300:
				d = QIC_FMT300; break;	/* minor 050 */
			case WT_QIC600:
				d = QIC_FMT600; break;	/* minor 060 */
			}
			if (d) {
				/* Change tape density. */
				if (!wtcmd(sc, d))
					return EIO;
				if (!wtsense(sc, 1, TP_WRP | TP_ILL))
					return EIO;

				/* Check the status of the controller. */
				if (sc->error & TP_ILL) {
					aprint_error_dev(sc->sc_dev, "invalid tape density\n");
					return ENODEV;
				}
			}
			sc->dens = minor(dev) & WT_DENSEL;
		}
		sc->flags &= ~TPSTART;
	} else if (sc->dens != (minor(dev) & WT_DENSEL))
		return ENXIO;

	sc->bsize = (minor(dev) & WT_BSIZE) ? 1024 : 512;
	sc->buf = malloc(sc->bsize, M_TEMP, M_WAITOK);

	sc->flags = TPINUSE;
	if (flag & FREAD)
		sc->flags |= TPREAD;
	if (flag & FWRITE)
		sc->flags |= TPWRITE;
	return 0;
}

/*
 * Close routine, called on last device close.
 */
static int
wtclose(dev_t dev, int flags, int mode,
    struct lwp *l)
{
	struct wt_softc *sc;

	sc = device_lookup_private(&wt_cd, minor(dev) & T_UNIT);

	/* If rewind is pending, do nothing */
	if (sc->flags & TPREW)
		goto done;

	/* If seek forward is pending and no rewind on close, do nothing */
	if (sc->flags & TPRMARK) {
		if (minor(dev) & T_NOREWIND)
			goto done;

		/* If read file mark is going on, wait */
		wtwait(sc, 0, "wtrfm");
	}

	if (sc->flags & TPWANY) {
		/* Tape was written.  Write file mark. */
		wtwritefm(sc);
	}

	if ((minor(dev) & T_NOREWIND) == 0) {
		/* Rewind to beginning of tape. */
		/* Don't wait until rewind, though. */
		wtrewind(sc);
		goto done;
	}
	if ((sc->flags & TPRANY) && (sc->flags & (TPVOL | TPWANY)) == 0) {
		/* Space forward to after next file mark if no writing done. */
		/* Don't wait for completion. */
		wtreadfm(sc);
	}

done:
	sc->flags &= TPREW | TPRMARK | TPSTART | TPTIMER;
	free(sc->buf, M_TEMP);
	return 0;
}

/*
 * Ioctl routine.  Compatible with BSD ioctls.
 * Direct QIC-02 commands ERASE and RETENSION added.
 * There are three possible ioctls:
 * ioctl(int fd, MTIOCGET, struct mtget *buf)	-- get status
 * ioctl(int fd, MTIOCTOP, struct mtop *buf)	-- do BSD-like op
 * ioctl(int fd, WTQICMD, int qicop)		-- do QIC op
 */
static int
wtioctl(dev_t dev, unsigned long cmd, void *addr, int flag,
    struct lwp *l)
{
	struct wt_softc *sc;
	int error, count, op;

	sc = device_lookup_private(&wt_cd, minor(dev) & T_UNIT);

	switch (cmd) {
	default:
		return EINVAL;
	case WTQICMD:	/* direct QIC command */
		op = *(int *)addr;
		switch (op) {
		default:
			return EINVAL;
		case QIC_ERASE:		/* erase the whole tape */
			if ((sc->flags & TPWRITE) == 0 || (sc->flags & TPWP))
				return EACCES;
			if ((error = wtwait(sc, PCATCH, "wterase")) != 0)
				return error;
			break;
		case QIC_RETENS:	/* retension the tape */
			if ((error = wtwait(sc, PCATCH, "wtretens")) != 0)
				return error;
			break;
		}
		/* Both ERASE and RETENS operations work like REWIND. */
		/* Simulate the rewind operation here. */
		sc->flags &= ~(TPRO | TPWO | TPVOL);
		if (!wtcmd(sc, op))
			return EIO;
		sc->flags |= TPSTART | TPREW;
		if (op == QIC_ERASE)
			sc->flags |= TPWANY;
		wtclock(sc);
		return 0;
	case MTIOCIEOT:	/* ignore EOT errors */
	case MTIOCEEOT:	/* enable EOT errors */
		return 0;
	case MTIOCGET:
		((struct mtget*)addr)->mt_type =
			sc->type == ARCHIVE ? MT_ISVIPER1 : 0x11;
		((struct mtget*)addr)->mt_dsreg = sc->flags;	/* status */
		((struct mtget*)addr)->mt_erreg = sc->error;	/* errors */
		((struct mtget*)addr)->mt_resid = 0;
		((struct mtget*)addr)->mt_fileno = 0;		/* file */
		((struct mtget*)addr)->mt_blkno = 0;		/* block */
		return 0;
	case MTIOCTOP:
		break;
	}

	switch ((short)((struct mtop*)addr)->mt_op) {
	default:
#if 0
	case MTFSR:	/* forward space record */
	case MTBSR:	/* backward space record */
	case MTBSF:	/* backward space file */
#endif
		return EINVAL;
	case MTNOP:	/* no operation, sets status only */
	case MTCACHE:	/* enable controller cache */
	case MTNOCACHE:	/* disable controller cache */
		return 0;
	case MTREW:	/* rewind */
	case MTOFFL:	/* rewind and put the drive offline */
		if (sc->flags & TPREW)   /* rewind is running */
			return 0;
		if ((error = wtwait(sc, PCATCH, "wtorew")) != 0)
			return error;
		wtrewind(sc);
		return 0;
	case MTFSF:	/* forward space file */
		for (count = ((struct mtop*)addr)->mt_count; count > 0;
		    --count) {
			if ((error = wtwait(sc, PCATCH, "wtorfm")) != 0)
				return error;
			if ((error = wtreadfm(sc)) != 0)
				return error;
		}
		return 0;
	case MTWEOF:	/* write an end-of-file record */
		if ((sc->flags & TPWRITE) == 0 || (sc->flags & TPWP))
			return EACCES;
		if ((error = wtwait(sc, PCATCH, "wtowfm")) != 0)
			return error;
		if ((error = wtwritefm(sc)) != 0)
			return error;
		return 0;
	}

#ifdef DIAGNOSTIC
	panic("wtioctl: impossible");
#endif
}

/*
 * Strategy routine.
 */
static void
wtstrategy(struct buf *bp)
{
	struct wt_softc *sc;
	int s;

	sc = device_lookup_private(&wt_cd, minor(bp->b_dev) & T_UNIT);

	bp->b_resid = bp->b_bcount;

	/* at file marks and end of tape, we just return '0 bytes available' */
	if (sc->flags & TPVOL)
		goto xit;

	if (bp->b_flags & B_READ) {
		/* Check read access and no previous write to this tape. */
		if ((sc->flags & TPREAD) == 0 || (sc->flags & TPWANY))
			goto errxit;

		/* For now, we assume that all data will be copied out */
		/* If read command outstanding, just skip down */
		if ((sc->flags & TPRO) == 0) {
			if (!wtsense(sc, 1, TP_WRP)) {
				/* Clear status. */
				goto errxit;
			}
			if (!wtcmd(sc, QIC_RDDATA)) {
				/* Set read mode. */
				wtsense(sc, 1, TP_WRP);
				goto errxit;
			}
			sc->flags |= TPRO | TPRANY;
		}
	} else {
		/* Check write access and write protection. */
		/* No previous read from this tape allowed. */
		if ((sc->flags & TPWRITE) == 0 || (sc->flags & (TPWP | TPRANY)))
			goto errxit;

		/* If write command outstanding, just skip down */
		if ((sc->flags & TPWO) == 0) {
			if (!wtsense(sc, 1, 0)) {
				/* Clear status. */
				goto errxit;
			}
			if (!wtcmd(sc, QIC_WRTDATA)) {
				/* Set write mode. */
				wtsense(sc, 1, 0);
				goto errxit;
			}
			sc->flags |= TPWO | TPWANY;
		}
	}

	if (bp->b_bcount == 0)
		goto xit;

	sc->flags &= ~TPEXCEP;
	s = splbio();
	if (wtstart(sc, bp->b_flags, bp->b_data, bp->b_bcount)) {
		wtwait(sc, 0, (bp->b_flags & B_READ) ? "wtread" : "wtwrite");
		bp->b_resid -= sc->dmacount;
	}
	splx(s);

	if (sc->flags & TPEXCEP) {
errxit:
		bp->b_error = EIO;
	}
xit:
	biodone(bp);
	return;
}

static int
wtread(dev_t dev, struct uio *uio, int flags)
{

	return (physio(wtstrategy, NULL, dev, B_READ, minphys, uio));
}

static int
wtwrite(dev_t dev, struct uio *uio, int flags)
{

	return (physio(wtstrategy, NULL, dev, B_WRITE, minphys, uio));
}

/*
 * Interrupt routine.
 */
static int
wtintr(void *arg)
{
	struct wt_softc *sc = arg;
	u_char x;

	/* get status */
	x = bus_space_read_1(sc->sc_iot, sc->sc_ioh, sc->regs.STATPORT);
	WTDBPRINT(("wtintr() status=0x%x -- ", x));
	if ((x & (sc->regs.BUSY | sc->regs.NOEXCEP))
	    == (sc->regs.BUSY | sc->regs.NOEXCEP)) {
		WTDBPRINT(("busy\n"));
		return 0;			/* device is busy */
	}

	/*
	 * Check if rewind finished.
	 */
	if (sc->flags & TPREW) {
		WTDBPRINT(((x & (sc->regs.BUSY | sc->regs.NOEXCEP))
			   == (sc->regs.BUSY | sc->regs.NOEXCEP) ?
			   "rewind busy?\n" : "rewind finished\n"));
		sc->flags &= ~TPREW;		/* rewind finished */
		wtsense(sc, 1, TP_WRP);
		wakeup((void *)sc);
		return 1;
	}

	/*
	 * Check if writing/reading of file mark finished.
	 */
	if (sc->flags & (TPRMARK | TPWMARK)) {
		WTDBPRINT(((x & (sc->regs.BUSY | sc->regs.NOEXCEP))
			   == (sc->regs.BUSY | sc->regs.NOEXCEP) ?
			   "marker r/w busy?\n" : "marker r/w finished\n"));
		if ((x & sc->regs.NOEXCEP) == 0)	/* operation failed */
			wtsense(sc, 1, (sc->flags & TPRMARK) ? TP_WRP : 0);
		sc->flags &= ~(TPRMARK | TPWMARK); /* operation finished */
		wakeup((void *)sc);
		return 1;
	}

	/*
	 * Do we started any i/o?  If no, just return.
	 */
	if ((sc->flags & TPACTIVE) == 0) {
		WTDBPRINT(("unexpected interrupt\n"));
		return 0;
	}
	sc->flags &= ~TPACTIVE;
	sc->dmacount += sc->bsize;		/* increment counter */

	/*
	 * Clean up DMA.
	 */
	if ((sc->dmaflags & DMAMODE_READ) &&
	    (sc->dmatotal - sc->dmacount) < sc->bsize) {
		/* If reading short block, copy the internal buffer
		 * to the user memory. */
		isa_dmadone(sc->sc_ic, sc->chan);
		memcpy(sc->dmavaddr, sc->buf, sc->dmatotal - sc->dmacount);
	} else
		isa_dmadone(sc->sc_ic, sc->chan);

	/*
	 * On exception, check for end of file and end of volume.
	 */
	if ((x & sc->regs.NOEXCEP) == 0) {
		WTDBPRINT(("i/o exception\n"));
		wtsense(sc, 1, (sc->dmaflags & DMAMODE_READ) ? TP_WRP : 0);
		if (sc->error & (TP_EOM | TP_FIL))
			sc->flags |= TPVOL;	/* end of file */
		else
			sc->flags |= TPEXCEP;	/* i/o error */
		wakeup((void *)sc);
		return 1;
	}

	if (sc->dmacount < sc->dmatotal) {
		/* Continue I/O. */
		sc->dmavaddr = (char *)sc->dmavaddr + sc->bsize;
		wtdma(sc);
		WTDBPRINT(("continue i/o, %d\n", sc->dmacount));
		return 1;
	}
	if (sc->dmacount > sc->dmatotal)	/* short last block */
		sc->dmacount = sc->dmatotal;
	/* Wake up user level. */
	wakeup((void *)sc);
	WTDBPRINT(("i/o finished, %d\n", sc->dmacount));
	return 1;
}

/* start the rewind operation */
static void
wtrewind(struct wt_softc *sc)
{
	int rwmode = sc->flags & (TPRO | TPWO);

	sc->flags &= ~(TPRO | TPWO | TPVOL);
	/*
	 * Wangtek strictly follows QIC-02 standard:
	 * clearing ONLINE in read/write modes causes rewind.
	 * REWIND command is not allowed in read/write mode
	 * and gives `illegal command' error.
	 */
	if (sc->type == WANGTEK && rwmode) {
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, sc->regs.CTLPORT, 0);
	} else if (!wtcmd(sc, QIC_REWIND))
		return;
	sc->flags |= TPSTART | TPREW;
	wtclock(sc);
}

/*
 * Start the `read marker' operation.
 */
static int
wtreadfm(struct wt_softc *sc)
{

	sc->flags &= ~(TPRO | TPWO | TPVOL);
	if (!wtcmd(sc, QIC_READFM)) {
		wtsense(sc, 1, TP_WRP);
		return EIO;
	}
	sc->flags |= TPRMARK | TPRANY;
	wtclock(sc);
	/* Don't wait for completion here. */
	return 0;
}

/*
 * Write marker to the tape.
 */
static int
wtwritefm(struct wt_softc *sc)
{

	tsleep((void *)wtwritefm, WTPRI, "wtwfm", hz);
	sc->flags &= ~(TPRO | TPWO);
	if (!wtcmd(sc, QIC_WRITEFM)) {
		wtsense(sc, 1, 0);
		return EIO;
	}
	sc->flags |= TPWMARK | TPWANY;
	wtclock(sc);
	return wtwait(sc, 0, "wtwfm");
}

/*
 * While controller status & mask == bits continue waiting.
 */
static u_char
wtsoft(struct wt_softc *sc, int mask, int bits)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_char x;
	int i;


	/* Poll status port, waiting for specified bits. */
	for (i = 0; i < 1000; ++i) {	/* up to 1 msec */
		x = bus_space_read_1(iot, ioh, sc->regs.STATPORT);
		if ((x & mask) != bits)
			return x;
		delay(1);
	}
	for (i = 0; i < 100; ++i) {	/* up to 10 msec */
		x = bus_space_read_1(iot, ioh, sc->regs.STATPORT);
		if ((x & mask) != bits)
			return x;
		delay(100);
	}
	for (;;) {			/* forever */
		x = bus_space_read_1(iot, ioh, sc->regs.STATPORT);
		if ((x & mask) != bits)
			return x;
		tsleep((void *)wtsoft, WTPRI, "wtsoft", 1);
	}
}

/*
 * Execute QIC command.
 */
static int
wtcmd(struct wt_softc *sc, int cmd)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_char x;
	int s;

	WTDBPRINT(("wtcmd() cmd=0x%x\n", cmd));
	s = splbio();
	x = wtsoft(sc, sc->regs.BUSY | sc->regs.NOEXCEP,
		   sc->regs.BUSY | sc->regs.NOEXCEP); /* ready? */
	if ((x & sc->regs.NOEXCEP) == 0) {		/* error */
		splx(s);
		return 0;
	}

	/* output the command */
	bus_space_write_1(iot, ioh, sc->regs.CMDPORT, cmd);

	/* set request */
	bus_space_write_1(iot, ioh, sc->regs.CTLPORT,
			  sc->regs.REQUEST | sc->regs.ONLINE);

	/* wait for ready */
	wtsoft(sc, sc->regs.BUSY, sc->regs.BUSY);

	/* reset request */
	bus_space_write_1(iot, ioh, sc->regs.CTLPORT,
			  sc->regs.IEN | sc->regs.ONLINE);

	/* wait for not ready */
	wtsoft(sc, sc->regs.BUSY, 0);
	splx(s);
	return 1;
}

/* wait for the end of i/o, seeking marker or rewind operation */
static int
wtwait(struct wt_softc *sc, int catch, const char *msg)
{
	int error;

	WTDBPRINT(("wtwait() `%s'\n", msg));
	while (sc->flags & (TPACTIVE | TPREW | TPRMARK | TPWMARK))
		if ((error = tsleep((void *)sc, WTPRI | catch, msg, 0)) != 0)
			return error;
	return 0;
}

/* initialize DMA for the i/o operation */
static void
wtdma(struct wt_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	sc->flags |= TPACTIVE;
	wtclock(sc);

	if (sc->type == ARCHIVE) {
		/* Set DMA. */
		bus_space_write_1(iot, ioh, sc->regs.SDMAPORT, 0);
	}

	if ((sc->dmaflags & DMAMODE_READ) &&
	    (sc->dmatotal - sc->dmacount) < sc->bsize) {
		/* Reading short block; do it through the internal buffer. */
		isa_dmastart(sc->sc_ic, sc->chan, sc->buf,
		    sc->bsize, NULL, sc->dmaflags, BUS_DMA_NOWAIT);
	} else
		isa_dmastart(sc->sc_ic, sc->chan, sc->dmavaddr,
		    sc->bsize, NULL, sc->dmaflags, BUS_DMA_NOWAIT);
}

/* start i/o operation */
static int
wtstart(struct wt_softc *sc, int flag, void *vaddr, size_t len)
{
	u_char x;

	WTDBPRINT(("wtstart()\n"));
	x = wtsoft(sc, sc->regs.BUSY | sc->regs.NOEXCEP,
		   sc->regs.BUSY | sc->regs.NOEXCEP); /* ready? */
	if ((x & sc->regs.NOEXCEP) == 0) {
		sc->flags |= TPEXCEP;	/* error */
		return 0;
	}
	sc->flags &= ~TPEXCEP;		/* clear exception flag */
	sc->dmavaddr = vaddr;
	sc->dmatotal = len;
	sc->dmacount = 0;
	sc->dmaflags = flag & B_READ ? DMAMODE_READ : DMAMODE_WRITE;
	wtdma(sc);
	return 1;
}

/*
 * Start timer.
 */
static void
wtclock(struct wt_softc *sc)
{

	if (sc->flags & TPTIMER)
		return;
	sc->flags |= TPTIMER;
	/*
	 * Some controllers seem to lose DMA interrupts too often.  To make the
	 * tape stream we need 1 tick timeout.
	 */
	callout_reset(&sc->sc_timer_ch, (sc->flags & TPACTIVE) ? 1 : hz,
	    wttimer, sc);
}

/*
 * Simulate an interrupt periodically while i/o is going.
 * This is necessary in case interrupts get eaten due to
 * multiple devices on a single IRQ line.
 */
static void
wttimer(void *arg)
{
	register struct wt_softc *sc = (struct wt_softc *)arg;
	int s;
	u_char status;

	sc->flags &= ~TPTIMER;
	if ((sc->flags & (TPACTIVE | TPREW | TPRMARK | TPWMARK)) == 0)
		return;

	/* If i/o going, simulate interrupt. */
	s = splbio();
	status = bus_space_read_1(sc->sc_iot, sc->sc_ioh, sc->regs.STATPORT);
	if ((status & (sc->regs.BUSY | sc->regs.NOEXCEP))
	    != (sc->regs.BUSY | sc->regs.NOEXCEP)) {
		WTDBPRINT(("wttimer() -- "));
		wtintr(sc);
	}
	splx(s);

	/* Restart timer if i/o pending. */
	if (sc->flags & (TPACTIVE | TPREW | TPRMARK | TPWMARK))
		wtclock(sc);
}

/*
 * Perform QIC-02 and QIC-36 compatible reset sequence.
 */
static int
wtreset(bus_space_tag_t iot, bus_space_handle_t ioh, struct wtregs *regs)
{
	u_char x;
	int i;

	/* send reset */
	bus_space_write_1(iot, ioh, regs->CTLPORT, regs->RESET | regs->ONLINE);
	delay(30);
	/* turn off reset */
	bus_space_write_1(iot, ioh, regs->CTLPORT, regs->ONLINE);
	delay(30);

	/* Read the controller status. */
	x = bus_space_read_1(iot, ioh, regs->STATPORT);
	if (x == 0xff)			/* no port at this address? */
		return 0;

	/* Wait 3 sec for reset to complete. Needed for QIC-36 boards? */
	for (i = 0; i < 3000; ++i) {
		if ((x & regs->BUSY) == 0 || (x & regs->NOEXCEP) == 0)
			break;
		delay(1000);
		x = bus_space_read_1(iot, ioh, regs->STATPORT);
	}
	return (x & regs->RESETMASK) == regs->RESETVAL;
}

/*
 * Get controller status information.  Return 0 if user i/o request should
 * receive an i/o error code.
 */
static int
wtsense(struct wt_softc *sc, int verbose, int ignore)
{
	const char *msg = 0;
	int error;

	WTDBPRINT(("wtsense() ignore=0x%x\n", ignore));
	sc->flags &= ~(TPRO | TPWO);
	if (!wtstatus(sc))
		return 0;
	if ((sc->error & TP_ST0) == 0)
		sc->error &= ~TP_ST0MASK;
	if ((sc->error & TP_ST1) == 0)
		sc->error &= ~TP_ST1MASK;
	sc->error &= ~ignore;	/* ignore certain errors */
	error = sc->error & (TP_FIL | TP_BNL | TP_UDA | TP_EOM | TP_WRP |
	    TP_USL | TP_CNI | TP_MBD | TP_NDT | TP_ILL);
	if (!error)
		return 1;
	if (!verbose)
		return 0;

	/* lifted from tdriver.c from Wangtek */
	if (error & TP_USL)
		msg = "Drive not online";
	else if (error & TP_CNI)
		msg = "No cartridge";
	else if ((error & TP_WRP) && (sc->flags & TPWP) == 0) {
		msg = "Tape is write protected";
		sc->flags |= TPWP;
	} else if (error & TP_FIL)
		msg = 0 /*"Filemark detected"*/;
	else if (error & TP_EOM)
		msg = 0 /*"End of tape"*/;
	else if (error & TP_BNL)
		msg = "Block not located";
	else if (error & TP_UDA)
		msg = "Unrecoverable data error";
	else if (error & TP_NDT)
		msg = "No data detected";
	else if (error & TP_ILL)
		msg = "Illegal command";
	if (msg)
		printf("%s: %s\n", device_xname(sc->sc_dev), msg);
	return 0;
}

/*
 * Get controller status information.
 */
static int
wtstatus(struct wt_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	char *p;
	int s;

	s = splbio();
	wtsoft(sc, sc->regs.BUSY | sc->regs.NOEXCEP,
	       sc->regs.BUSY | sc->regs.NOEXCEP); /* ready? */
	/* send `read status' command */
	bus_space_write_1(iot, ioh, sc->regs.CMDPORT, QIC_RDSTAT);

	/* set request */
	bus_space_write_1(iot, ioh, sc->regs.CTLPORT,
			  sc->regs.REQUEST | sc->regs.ONLINE);

	/* wait for ready */
	wtsoft(sc, sc->regs.BUSY, sc->regs.BUSY);
	/* reset request */
	bus_space_write_1(iot, ioh, sc->regs.CTLPORT, sc->regs.ONLINE);

	/* wait for not ready */
	wtsoft(sc, sc->regs.BUSY, 0);

	p = (char *)&sc->error;
	while (p < (char *)&sc->error + 6) {
		u_char x = wtsoft(sc, sc->regs.BUSY | sc->regs.NOEXCEP,
		    sc->regs.BUSY | sc->regs.NOEXCEP);

		if ((x & sc->regs.NOEXCEP) == 0) {	/* error */
			splx(s);
			return 0;
		}

		/* read status byte */
		*p++ = bus_space_read_1(iot, ioh, sc->regs.DATAPORT);

		/* set request */
		bus_space_write_1(iot, ioh, sc->regs.CTLPORT,
		    sc->regs.REQUEST | sc->regs.ONLINE);

		/* wait for not ready */
		wtsoft(sc, sc->regs.BUSY, 0);

		/* unset request */
		bus_space_write_1(iot, ioh, sc->regs.CTLPORT, sc->regs.ONLINE);
	}
	splx(s);
	return 1;
}
