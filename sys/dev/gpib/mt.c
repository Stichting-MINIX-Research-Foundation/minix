/*	$NetBSD: mt.c,v 1.29 2014/07/25 08:10:36 dholland Exp $ */

/*-
 * Copyright (c) 1996-2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: rd.c 1.44 92/12/26$
 *
 *	@(#)rd.c	8.2 (Berkeley) 5/19/94
 */

/*
 * Magnetic tape driver (HP7974a, HP7978a/b, HP7979a, HP7980a, HP7980xc)
 * Original version contributed by Mt. Xinu.
 * Modified for 4.4BSD by Mark Davies and Andrew Vignaux, Department of
 * Computer Science, Victoria University of Wellington
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mt.c,v 1.29 2014/07/25 08:10:36 dholland Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/ioctl.h>
#include <sys/mtio.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/kernel.h>
#include <sys/tprintf.h>
#include <sys/device.h>
#include <sys/conf.h>

#include <dev/gpib/gpibvar.h>
#include <dev/gpib/cs80busvar.h>

#include <dev/gpib/mtreg.h>

#ifdef DEBUG
int	mtdebug = 0;
#define MDB_ANY		0xff
#define MDB_FOLLOW	0x01
#define	DPRINTF(mask, str)	if (mtdebug & (mask)) printf str
#else
#define	DPRINTF(mask, str)	/* nothing */
#endif

struct	mt_softc {
	device_t sc_dev;

	gpib_chipset_tag_t sc_ic;
	gpib_handle_t sc_hdl;

	int	sc_slave;	/* GPIB slave address (0-6) */
	short	sc_flags;	/* see below */
	u_char	sc_lastdsj;	/* place for DSJ in mtreaddsj() */
	u_char	sc_lastecmd;	/* place for End Command in mtreaddsj() */
	short	sc_recvtimeo;	/* count of gpibsend timeouts to prevent hang */
	short	sc_statindex;	/* index for next sc_stat when MTF_STATTIMEO */
	struct	mt_stat sc_stat;/* status bytes last read from device */
	short	sc_density;	/* current density of tape (mtio.h format) */
	short	sc_type;	/* tape drive model (hardware IDs) */
	tpr_t	sc_ttyp;
	struct bufq_state *sc_tab;/* buf queue */
	int	sc_active;
	struct buf sc_bufstore;	/* XXX buffer storage */

	struct	callout sc_start_ch;
	struct	callout sc_intr_ch;
};

#define	MTUNIT(x)	(minor(x) & 0x03)

#define B_CMD		B_DEVPRIVATE	/* command buf instead of data */
#define	b_cmd		b_blkno		/* blkno holds cmd when B_CMD */

int	mtmatch(device_t, cfdata_t, void *);
void	mtattach(device_t, device_t, void *);

CFATTACH_DECL_NEW(mt, sizeof(struct mt_softc),
	mtmatch, mtattach, NULL, NULL);

int	mtlookup(int, int, int);
void	mtustart(struct mt_softc *);
int	mtreaddsj(struct mt_softc *, int);
int	mtcommand(dev_t, int, int);

void	mtintr_callout(void *);
void	mtstart_callout(void *);

void	mtcallback(void *, int);
void	mtstart(struct mt_softc *);
void	mtintr(struct mt_softc  *);

dev_type_open(mtopen);
dev_type_close(mtclose);
dev_type_read(mtread);
dev_type_write(mtwrite);
dev_type_ioctl(mtioctl);
dev_type_strategy(mtstrategy);

const struct bdevsw mt_bdevsw = {
	.d_open = mtopen,
	.d_close = mtclose,
	.d_strategy = mtstrategy,
	.d_ioctl = mtioctl,
	.d_dump = nodump,
	.d_psize = nosize,
	.d_discard = nodiscard,
	.d_flag = D_TAPE
};

const struct cdevsw mt_cdevsw = {
	.d_open = mtopen,
	.d_close = mtclose,
	.d_read = mtread,
	.d_write = mtwrite,
	.d_ioctl = mtioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_TAPE
};


extern struct cfdriver mt_cd;

struct	mtinfo {
	u_short	hwid;
	const char	*desc;
} mtinfo[] = {
	{ MT7978ID,	"7978"	},
	{ MT7979AID,	"7979A"	},
	{ MT7980ID,	"7980"	},
	{ MT7974AID,	"7974A"	},
};
int	nmtinfo = sizeof(mtinfo) / sizeof(mtinfo[0]);


int
mtlookup(int id, int slave, int punit)
{
	int i;

	for (i = 0; i < nmtinfo; i++)
		if (mtinfo[i].hwid == id)
			break;
	if (i == nmtinfo)
		return (-1);
	return (0);
}

int
mtmatch(device_t parent, cfdata_t match, void *aux)
{
	struct cs80bus_attach_args *ca = aux;

	ca->ca_punit = 0;
	return (mtlookup(ca->ca_id, ca->ca_slave, ca->ca_punit) == 0);
}

void
mtattach(device_t parent, device_t self, void *aux)
{
	struct mt_softc *sc = device_private(self);
	struct cs80bus_attach_args *ca = aux;
	int type;

	sc->sc_ic = ca->ca_ic;
	sc->sc_slave = ca->ca_slave;

	if ((type = mtlookup(ca->ca_id, ca->ca_slave, ca->ca_punit)) < 0)
		return;

	printf(": %s tape\n", mtinfo[type].desc);

	sc->sc_type = type;
	sc->sc_flags = MTF_EXISTS;

	bufq_alloc(&sc->sc_tab, "fcfs", 0);
	callout_init(&sc->sc_start_ch, 0);
	callout_init(&sc->sc_intr_ch, 0);

	if (gpibregister(sc->sc_ic, sc->sc_slave, mtcallback, sc,
	    &sc->sc_hdl)) {
		aprint_error_dev(sc->sc_dev, "can't register callback\n");
		return;
	}
}

/*
 * Perform a read of "Device Status Jump" register and update the
 * status if necessary.  If status is read, the given "ecmd" is also
 * performed, unless "ecmd" is zero.  Returns DSJ value, -1 on failure
 * and -2 on "temporary" failure.
 */
int
mtreaddsj(struct mt_softc *sc, int ecmd)
{
	int retval;

	if (sc->sc_flags & MTF_STATTIMEO)
		goto getstats;
	retval = gpibrecv(sc->sc_ic,
	    (sc->sc_flags & MTF_DSJTIMEO) ? -1 : sc->sc_slave,
	    MTT_DSJ, &(sc->sc_lastdsj), 1);
	sc->sc_flags &= ~MTF_DSJTIMEO;
	if (retval != 1) {
		DPRINTF(MDB_ANY, ("%s can't gpibrecv DSJ",
		    device_xname(sc->sc_dev)));
		if (sc->sc_recvtimeo == 0)
			sc->sc_recvtimeo = hz;
		if (--sc->sc_recvtimeo == 0)
			return (-1);
		if (retval == 0)
			sc->sc_flags |= MTF_DSJTIMEO;
		return (-2);
	}
	sc->sc_recvtimeo = 0;
	sc->sc_statindex = 0;
	DPRINTF(MDB_ANY, ("%s readdsj: 0x%x", device_xname(sc->sc_dev),
	    sc->sc_lastdsj));
	sc->sc_lastecmd = ecmd;
	switch (sc->sc_lastdsj) {
	    case 0:
		if (ecmd & MTE_DSJ_FORCE)
			break;
		return (0);

	    case 2:
		sc->sc_lastecmd = MTE_COMPLETE;
	    case 1:
		break;

	    default:
		printf("%s readdsj: DSJ 0x%x\n", device_xname(sc->sc_dev),
		    sc->sc_lastdsj);
		return (-1);
	}

getstats:
	retval = gpibrecv(sc->sc_ic,
	    (sc->sc_flags & MTF_STATCONT) ? -1 : sc->sc_slave, MTT_STAT,
	     ((char *)&(sc->sc_stat)) + sc->sc_statindex,
	    sizeof(sc->sc_stat) - sc->sc_statindex);
	sc->sc_flags &= ~(MTF_STATTIMEO | MTF_STATCONT);
	if (retval != sizeof(sc->sc_stat) - sc->sc_statindex) {
		if (sc->sc_recvtimeo == 0)
			sc->sc_recvtimeo = hz;
		if (--sc->sc_recvtimeo != 0) {
			if (retval >= 0) {
				sc->sc_statindex += retval;
				sc->sc_flags |= MTF_STATCONT;
			}
			sc->sc_flags |= MTF_STATTIMEO;
			return (-2);
		}
		printf("%s readdsj: can't read status", device_xname(sc->sc_dev));
		return (-1);
	}
	sc->sc_recvtimeo = 0;
	sc->sc_statindex = 0;
	DPRINTF(MDB_ANY, ("%s readdsj: status is %x %x %x %x %x %x",
	    device_xname(sc->sc_dev),
	    sc->sc_stat1, sc->sc_stat2, sc->sc_stat3,
	    sc->sc_stat4, sc->sc_stat5, sc->sc_stat6));
	if (sc->sc_lastecmd)
		(void) gpibsend(sc->sc_ic, sc->sc_slave,
		    MTL_ECMD, &(sc->sc_lastecmd), 1);
	return ((int) sc->sc_lastdsj);
}

int
mtopen(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct mt_softc *sc;
	int req_den;
	int error;

	sc = device_lookup_private(&mt_cd, MTUNIT(dev));
	if (sc == NULL || (sc->sc_flags & MTF_EXISTS) == 0)
		return (ENXIO);

	if (sc->sc_flags & MTF_OPEN)
		return (EBUSY);

	DPRINTF(MDB_ANY, ("%s open: flags 0x%x", device_xname(sc->sc_dev),
	    sc->sc_flags));

	sc->sc_flags |= MTF_OPEN;
	sc->sc_ttyp = tprintf_open(l->l_proc);
	if ((sc->sc_flags & MTF_ALIVE) == 0) {
		error = mtcommand(dev, MTRESET, 0);
		if (error != 0 || (sc->sc_flags & MTF_ALIVE) == 0)
			goto errout;
		if ((sc->sc_stat1 & (SR1_BOT | SR1_ONLINE)) == SR1_ONLINE)
			(void) mtcommand(dev, MTREW, 0);
	}
	for (;;) {
		if ((error = mtcommand(dev, MTNOP, 0)) != 0)
			goto errout;
		if (!(sc->sc_flags & MTF_REW))
			break;
		error = kpause("mt", true, hz, NULL);
		if (error != 0 && error != EWOULDBLOCK) {
			error = EINTR;
			goto errout;
		}
	}
	if ((flag & FWRITE) && (sc->sc_stat1 & SR1_RO)) {
		error = EROFS;
		goto errout;
	}
	if (!(sc->sc_stat1 & SR1_ONLINE)) {
		uprintf("%s: not online\n", device_xname(sc->sc_dev));
		error = EIO;
		goto errout;
	}
	/*
	 * Select density:
	 *  - find out what density the drive is set to
	 *	(i.e. the density of the current tape)
	 *  - if we are going to write
	 *    - if we're not at the beginning of the tape
	 *      - complain if we want to change densities
	 *    - otherwise, select the mtcommand to set the density
	 *
	 * If the drive doesn't support it then don't change the recorded
	 * density.
	 *
	 * The original MOREbsd code had these additional conditions
	 * for the mid-tape change
	 *
	 *	req_den != T_BADBPI &&
	 *	sc->sc_density != T_6250BPI
	 *
	 * which suggests that it would be possible to write multiple
	 * densities if req_den == T_BAD_BPI or the current tape
	 * density was 6250.  Testing of our 7980 suggests that the
	 * device cannot change densities mid-tape.
	 *
	 * ajv@comp.vuw.ac.nz
	 */
	sc->sc_density = (sc->sc_stat2 & SR2_6250) ? T_6250BPI : (
			 (sc->sc_stat3 & SR3_1600) ? T_1600BPI : (
			 (sc->sc_stat3 & SR3_800) ? T_800BPI : -1));
	req_den = (dev & T_DENSEL);

	if (flag & FWRITE) {
		if (!(sc->sc_stat1 & SR1_BOT)) {
			if (sc->sc_density != req_den) {
				uprintf("%s: can't change density mid-tape\n",
				    device_xname(sc->sc_dev));
				error = EIO;
				goto errout;
			}
		}
		else {
			int mtset_density =
			    (req_den == T_800BPI  ? MTSET800BPI : (
			     req_den == T_1600BPI ? MTSET1600BPI : (
			     req_den == T_6250BPI ? MTSET6250BPI : (
			     sc->sc_type == MT7980ID
						  ? MTSET6250DC
						  : MTSET6250BPI))));
			if (mtcommand(dev, mtset_density, 0) == 0)
				sc->sc_density = req_den;
		}
	}
	return (0);
errout:
	sc->sc_flags &= ~MTF_OPEN;
	return (error);
}

int
mtclose(dev_t dev, int flag, int fmt, struct lwp *l)
{
	struct mt_softc *sc;

	sc = device_lookup_private(&mt_cd, MTUNIT(dev));
	if (sc == NULL)
		return (ENXIO);

	if (sc->sc_flags & MTF_WRT) {
		(void) mtcommand(dev, MTWEOF, 2);
		(void) mtcommand(dev, MTBSF, 0);
	}
	if ((minor(dev) & T_NOREWIND) == 0)
		(void) mtcommand(dev, MTREW, 0);
	sc->sc_flags &= ~MTF_OPEN;
	tprintf_close(sc->sc_ttyp);
	return (0);
}

int
mtcommand(dev_t dev, int cmd, int cnt)
{
	struct mt_softc *sc;
	struct buf *bp;
	int error = 0;

	sc = device_lookup_private(&mt_cd, MTUNIT(dev));
	bp = &sc->sc_bufstore;

	if (bp->b_cflags & BC_BUSY)
		return (EBUSY);

	bp->b_cmd = cmd;
	bp->b_dev = dev;
	bp->b_objlock = &buffer_lock;
	do {
		bp->b_cflags = BC_BUSY;
		bp->b_flags = B_CMD;
		bp->b_oflags = 0;
		mtstrategy(bp);
		biowait(bp);
		if (bp->b_error != 0) {
			error = (int) (unsigned) bp->b_error;
			break;
		}
	} while (--cnt > 0);
#if 0
	bp->b_cflags = 0 /*&= ~BC_BUSY*/;
#else
	bp->b_cflags &= ~BC_BUSY;
#endif
	return (error);
}

/*
 * Only thing to check here is for legal record lengths (writes only).
 */
void
mtstrategy(struct buf *bp)
{
	struct mt_softc *sc;
	int s;

	sc = device_lookup_private(&mt_cd, MTUNIT(bp->b_dev));

	DPRINTF(MDB_ANY, ("%s strategy", device_xname(sc->sc_dev)));

	if ((bp->b_flags & (B_CMD | B_READ)) == 0) {
#define WRITE_BITS_IGNORED	8
#if 0
		if (bp->b_bcount & ((1 << WRITE_BITS_IGNORED) - 1)) {
			tprintf(sc->sc_ttyp,
				"%s: write record must be multiple of %d\n",
				device_xname(sc->sc_dev), 1 << WRITE_BITS_IGNORED);
			goto error;
		}
#endif
		s = 16 * 1024;
		if (sc->sc_stat2 & SR2_LONGREC) {
			switch (sc->sc_density) {
			    case T_1600BPI:
				s = 32 * 1024;
				break;

			    case T_6250BPI:
			    case T_BADBPI:
				s = 60 * 1024;
				break;
			}
		}
		if (bp->b_bcount > s) {
			tprintf(sc->sc_ttyp,
				"%s: write record (%d) too big: limit (%d)\n",
				device_xname(sc->sc_dev), bp->b_bcount, s);
#if 0 /* XXX see above */
	    error:
#endif
			bp->b_error = EIO;
			biodone(bp);
			return;
		}
	}
	s = splbio();
	bufq_put(sc->sc_tab, bp);
	if (sc->sc_active == 0) {
		sc->sc_active = 1;
		mtustart(sc);
	}
	splx(s);
}

void
mtustart(struct mt_softc *sc)
{

	DPRINTF(MDB_ANY, ("%s ustart", device_xname(sc->sc_dev)));
	if (gpibrequest(sc->sc_ic, sc->sc_hdl))
		mtstart(sc);
}

void
mtcallback(void *v, int action)
{
	struct mt_softc *sc = v;

	DPRINTF(MDB_FOLLOW, ("mtcallback: v=%p, action=%d\n", v, action));

	switch (action) {
	case GPIBCBF_START:
		mtstart(sc);
		break;
	case GPIBCBF_INTR:
		mtintr(sc);
		break;
#ifdef DEBUG
	default:
		printf("mtcallback: unknown action %d\n", action);
		break;
#endif
	}
}

void
mtintr_callout(void *arg)
{
	struct mt_softc *sc = arg;
	int s = splbio();

	gpibppclear(sc->sc_ic);
	mtintr(sc);
	splx(s);
}

void
mtstart_callout(void *arg)
{
	int s = splbio();

	mtstart((struct mt_softc *)arg);
	splx(s);
}

void
mtstart(struct mt_softc *sc)
{
	struct buf *bp;
	short	cmdcount = 1;
	u_char	cmdbuf[2];

	DPRINTF(MDB_ANY, ("%s start", device_xname(sc->sc_dev)));
	sc->sc_flags &= ~MTF_WRT;
	bp = bufq_peek(sc->sc_tab);
	if ((sc->sc_flags & MTF_ALIVE) == 0 &&
	    ((bp->b_flags & B_CMD) == 0 || bp->b_cmd != MTRESET))
		goto fatalerror;

	if (sc->sc_flags & MTF_REW) {
		if (!gpibpptest(sc->sc_ic, sc->sc_slave))
			goto stillrew;
		switch (mtreaddsj(sc, MTE_DSJ_FORCE|MTE_COMPLETE|MTE_IDLE)) {
		    case 0:
		    case 1:
		stillrew:
			if ((sc->sc_stat1 & SR1_BOT) ||
			    !(sc->sc_stat1 & SR1_ONLINE)) {
				sc->sc_flags &= ~MTF_REW;
				break;
			}
		    case -2:
			/*
			 * -2 means "timeout" reading DSJ, which is probably
			 * temporary.  This is considered OK when doing a NOP,
			 * but not otherwise.
			 */
			if (sc->sc_flags & (MTF_DSJTIMEO | MTF_STATTIMEO)) {
				callout_reset(&sc->sc_start_ch, hz >> 5,
				    mtstart_callout, sc);
				return;
			}
		    case 2:
			if (bp->b_cmd != MTNOP || !(bp->b_flags & B_CMD)) {
				bp->b_error = EBUSY;
				goto done;
			}
			goto done;

		    default:
			goto fatalerror;
		}
	}
	if (bp->b_flags & B_CMD) {
		if (sc->sc_flags & MTF_PASTEOT) {
			switch(bp->b_cmd) {
			    case MTFSF:
			    case MTWEOF:
			    case MTFSR:
				bp->b_error = ENOSPC;
				goto done;

			    case MTBSF:
			    case MTOFFL:
			    case MTBSR:
			    case MTREW:
				sc->sc_flags &= ~(MTF_PASTEOT | MTF_ATEOT);
				break;
			}
		}
		switch(bp->b_cmd) {
		    case MTFSF:
			if (sc->sc_flags & MTF_HITEOF)
				goto done;
			cmdbuf[0] = MTTC_FSF;
			break;

		    case MTBSF:
			if (sc->sc_flags & MTF_HITBOF)
				goto done;
			cmdbuf[0] = MTTC_BSF;
			break;

		    case MTOFFL:
			sc->sc_flags |= MTF_REW;
			cmdbuf[0] = MTTC_REWOFF;
			break;

		    case MTWEOF:
			cmdbuf[0] = MTTC_WFM;
			break;

		    case MTBSR:
			cmdbuf[0] = MTTC_BSR;
			break;

		    case MTFSR:
			cmdbuf[0] = MTTC_FSR;
			break;

		    case MTREW:
			sc->sc_flags |= MTF_REW;
			cmdbuf[0] = MTTC_REW;
			break;

		    case MTNOP:
			/*
			 * NOP is supposed to set status bits.
			 * Force readdsj to do it.
			 */
			switch (mtreaddsj(sc,
			  MTE_DSJ_FORCE | MTE_COMPLETE | MTE_IDLE)) {
			    default:
				goto done;

			    case -1:
				/*
				 * If this fails, perform a device clear
				 * to fix any protocol problems and (most
				 * likely) get the status.
				 */
				bp->b_cmd = MTRESET;
				break;

			    case -2:
				callout_reset(&sc->sc_start_ch, hz >> 5,
				    mtstart_callout, sc);
				return;
			}

		    case MTRESET:
			/*
			 * 1) selected device clear (send with "-2" secondary)
			 * 2) set timeout, then wait for "service request"
			 * 3) interrupt will read DSJ (and END COMPLETE-IDLE)
			 */
			if (gpibsend(sc->sc_ic, sc->sc_slave, -2, NULL, 0)){
				aprint_error_dev(sc->sc_dev, "can't reset");
				goto fatalerror;
			}
			callout_reset(&sc->sc_intr_ch, 4*hz, mtintr_callout,
			    sc);
			gpibawait(sc->sc_ic);
			return;

		    case MTSET800BPI:
			cmdbuf[0] = MTTC_800;
			break;

		    case MTSET1600BPI:
			cmdbuf[0] = MTTC_1600;
			break;

		    case MTSET6250BPI:
			cmdbuf[0] = MTTC_6250;
			break;

		    case MTSET6250DC:
			cmdbuf[0] = MTTC_DC6250;
			break;
		}
	} else {
		if (sc->sc_flags & MTF_PASTEOT) {
			bp->b_error = ENOSPC;
			goto done;
		}
		if (bp->b_flags & B_READ) {
			sc->sc_flags |= MTF_IO;
			cmdbuf[0] = MTTC_READ;
		} else {
			sc->sc_flags |= MTF_WRT | MTF_IO;
			cmdbuf[0] = MTTC_WRITE;
			cmdbuf[1] = (bp->b_bcount +((1 << WRITE_BITS_IGNORED) - 1)) >> WRITE_BITS_IGNORED;
			cmdcount = 2;
		}
	}
	if (gpibsend(sc->sc_ic, sc->sc_slave, MTL_TCMD, cmdbuf, cmdcount)
	    == cmdcount) {
		if (sc->sc_flags & MTF_REW)
			goto done;
		gpibawait(sc->sc_ic);
		return;
	}
fatalerror:
	/*
	 * If anything fails, the drive is probably hosed, so mark it not
	 * "ALIVE" (but it EXISTS and is OPEN or we wouldn't be here, and
	 * if, last we heard, it was REWinding, remember that).
	 */
	sc->sc_flags &= MTF_EXISTS | MTF_OPEN | MTF_REW;
	bp->b_error = EIO;
done:
	sc->sc_flags &= ~(MTF_HITEOF | MTF_HITBOF);
	(void)bufq_get(sc->sc_tab);
	biodone(bp);
	gpibrelease(sc->sc_ic, sc->sc_hdl);
	if ((bp = bufq_peek(sc->sc_tab)) == NULL)
		sc->sc_active = 0;
	else
		mtustart(sc);
}

void
mtintr(struct mt_softc *sc)
{
	struct buf *bp;
	int slave, dir, i;
	u_char cmdbuf[4];

	slave = sc->sc_slave;

	bp = bufq_peek(sc->sc_tab);
	if (bp == NULL) {
		printf("%s intr: bp == NULL", device_xname(sc->sc_dev));
		return;
	}

	DPRINTF(MDB_ANY, ("%s intr", device_xname(sc->sc_dev)));

	/*
	 * Some operation completed.  Read status bytes and report errors.
	 * Clear EOF flags here `cause they're set once on specific conditions
	 * below when a command succeeds.
	 * A DSJ of 2 always means keep waiting.  If the command was READ
	 * (and we're in data DMA phase) stop data transfer first.
	 */
	sc->sc_flags &= ~(MTF_HITEOF | MTF_HITBOF);
	if ((bp->b_flags & (B_CMD|B_READ)) == B_READ &&
	    !(sc->sc_flags & (MTF_IO | MTF_STATTIMEO | MTF_DSJTIMEO))){
		cmdbuf[0] = MTE_STOP;
		(void) gpibsend(sc->sc_ic, slave, MTL_ECMD,cmdbuf,1);
	}
	switch (mtreaddsj(sc, 0)) {
	    case 0:
		break;

	    case 1:
		/*
		 * If we're in the middle of a READ/WRITE and have yet to
		 * start the data transfer, a DSJ of one should terminate it.
		 */
		sc->sc_flags &= ~MTF_IO;
		break;

	    case 2:
		(void) gpibawait(sc->sc_ic);
		return;

	    case -2:
		/*
		 * -2 means that the drive failed to respond quickly enough
		 * to the request for DSJ.  It's probably just "busy" figuring
		 * it out and will know in a little bit...
		 */
		callout_reset(&sc->sc_intr_ch, hz >> 5, mtintr_callout, sc);
		return;

	    default:
		printf("%s intr: can't get drive stat", device_xname(sc->sc_dev));
		goto error;
	}
	if (sc->sc_stat1 & (SR1_ERR | SR1_REJECT)) {
		i = sc->sc_stat4 & SR4_ERCLMASK;
		printf("%s: %s error, retry %d, SR2/3 %x/%x, code %d",
			device_xname(sc->sc_dev), i == SR4_DEVICE ? "device" :
			(i == SR4_PROTOCOL ? "protocol" :
			(i == SR4_SELFTEST ? "selftest" : "unknown")),
			sc->sc_stat4 & SR4_RETRYMASK, sc->sc_stat2,
			sc->sc_stat3, sc->sc_stat5);

		if ((bp->b_flags & B_CMD) && bp->b_cmd == MTRESET)
			callout_stop(&sc->sc_intr_ch);
		if (sc->sc_stat3 & SR3_POWERUP)
			sc->sc_flags &= MTF_OPEN | MTF_EXISTS;
		goto error;
	}
	/*
	 * Report and clear any soft errors.
	 */
	if (sc->sc_stat1 & SR1_SOFTERR) {
		printf("%s: soft error, retry %d\n", device_xname(sc->sc_dev),
		    sc->sc_stat4 & SR4_RETRYMASK);
		sc->sc_stat1 &= ~SR1_SOFTERR;
	}
	/*
	 * We've initiated a read or write, but haven't actually started to
	 * DMA the data yet.  At this point, the drive's ready.
	 */
	if (sc->sc_flags & MTF_IO) {
		sc->sc_flags &= ~MTF_IO;
		dir = (bp->b_flags & B_READ ? GPIB_READ : GPIB_WRITE);
		gpibxfer(sc->sc_ic, slave,
		    dir == GPIB_READ ? MTT_READ : MTL_WRITE,
		    bp->b_data, bp->b_bcount, dir, dir == GPIB_READ);
		return;
	}
	/*
	 * Check for End Of Tape - we're allowed to hit EOT and then write (or
	 * read) one more record.  If we get here and have not already hit EOT,
	 * return ENOSPC to inform the process that it's hit it.  If we get
	 * here and HAVE already hit EOT, don't allow any more operations that
	 * move the tape forward.
	 */
	if (sc->sc_stat1 & SR1_EOT) {
		if (sc->sc_flags & MTF_ATEOT)
			sc->sc_flags |= MTF_PASTEOT;
		else {
			bp->b_error = ENOSPC;
			sc->sc_flags |= MTF_ATEOT;
		}
	}
	/*
	 * If a motion command was being executed, check for Tape Marks.
	 * If we were doing data, make sure we got the right amount, and
	 * check for hitting tape marks on reads.
	 */
	if (bp->b_flags & B_CMD) {
		if (sc->sc_stat1 & SR1_EOF) {
			if (bp->b_cmd == MTFSR)
				sc->sc_flags |= MTF_HITEOF;
			if (bp->b_cmd == MTBSR)
				sc->sc_flags |= MTF_HITBOF;
		}
		if (bp->b_cmd == MTRESET) {
			callout_stop(&sc->sc_intr_ch);
			sc->sc_flags |= MTF_ALIVE;
		}
	} else {
		i = gpibrecv(sc->sc_ic, slave, MTT_BCNT, cmdbuf, 2);
		if (i != 2) {
			aprint_error_dev(sc->sc_dev, "intr: can't get xfer length\n");
			goto error;
		}
		i = (int) *((u_short *) cmdbuf);
		if (i <= bp->b_bcount) {
			if (i == 0)
				sc->sc_flags |= MTF_HITEOF;
			bp->b_resid = bp->b_bcount - i;
			DPRINTF(MDB_ANY, ("%s intr: bcount %d, resid %d",
			    device_xname(sc->sc_dev),
			    bp->b_bcount, bp->b_resid));
		} else {
			tprintf(sc->sc_ttyp,
				"%s: record (%d) larger than wanted (%d)\n",
				device_xname(sc->sc_dev), i, bp->b_bcount);
error:
			sc->sc_flags &= ~MTF_IO;
			bp->b_error = EIO;
		}
	}
	/*
	 * The operation is completely done.
	 * Let the drive know with an END command.
	 */
	cmdbuf[0] = MTE_COMPLETE | MTE_IDLE;
	(void) gpibsend(sc->sc_ic, slave, MTL_ECMD, cmdbuf, 1);
	bp->b_flags &= ~B_CMD;
	(void)bufq_get(sc->sc_tab);
	biodone(bp);
	gpibrelease(sc->sc_ic, sc->sc_hdl);
	if (bufq_peek(sc->sc_tab) == NULL)
		sc->sc_active = 0;
	else
		mtustart(sc);
}

int
mtread(dev_t dev, struct uio *uio, int flags)
{
	return (physio(mtstrategy, NULL, dev, B_READ, minphys, uio));
}

int
mtwrite(dev_t dev, struct uio *uio, int flags)
{
	return (physio(mtstrategy, NULL, dev, B_WRITE, minphys, uio));
}

int
mtioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct mtop *op;
	int cnt;

	switch (cmd) {
	    case MTIOCTOP:
		op = (struct mtop *)data;
		switch(op->mt_op) {
		    case MTWEOF:
		    case MTFSF:
		    case MTBSR:
		    case MTBSF:
		    case MTFSR:
			cnt = op->mt_count;
			break;

		    case MTOFFL:
		    case MTREW:
		    case MTNOP:
			cnt = 0;
			break;

		    default:
			return (EINVAL);
		}
		return (mtcommand(dev, op->mt_op, cnt));

	    case MTIOCGET:
		break;

	    default:
		return (EINVAL);
	}
	return (0);
}
