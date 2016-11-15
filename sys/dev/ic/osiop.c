/*	$NetBSD: osiop.c,v 1.40 2013/12/16 15:49:25 mrg Exp $	*/

/*-
 * Copyright (c) 2001 Izumi Tsutsui.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Van Jacobson of Lawrence Berkeley Laboratory.
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
 *	@(#)siop.c	7.5 (Berkeley) 5/4/91
 */

/*
 * Copyright (c) 1994 Michael L. Hitch
 *
 * This code is derived from software contributed to Berkeley by
 * Van Jacobson of Lawrence Berkeley Laboratory.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	@(#)siop.c	7.5 (Berkeley) 5/4/91
 */

/*
 * MI NCR53C710 scsi adaptor driver; based on arch/amiga/dev/siop.c:
 *	NetBSD: siop.c,v 1.43 1999/09/30 22:59:53 thorpej Exp
 *
 * bus_space/bus_dma'fied by Izumi Tsutsui <tsutsui@NetBSD.org>
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: osiop.c,v 1.40 2013/12/16 15:49:25 mrg Exp $");

/* #define OSIOP_DEBUG */

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/kernel.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>
#include <dev/scsipi/scsi_message.h>

#include <sys/cpu.h>
#include <sys/bus.h>

#include <dev/ic/osiopreg.h>
#include <dev/ic/osiopvar.h>

/* 53C710 script */
#include <dev/microcode/siop/osiop.out>

static void osiop_minphys(struct buf *);
static void osiop_scsipi_request(struct scsipi_channel *, scsipi_adapter_req_t,
    void *);
static void osiop_poll(struct osiop_softc *, struct osiop_acb *);
static void osiop_sched(struct osiop_softc *);
static void osiop_scsidone(struct osiop_acb *, int);
static void osiop_abort(struct osiop_softc *, const char *);
static void osiop_init(struct osiop_softc *);
static void osiop_reset(struct osiop_softc *);
static void osiop_resetbus(struct osiop_softc *);
static void osiop_start(struct osiop_softc *);
static int osiop_checkintr(struct osiop_softc *, uint8_t, uint8_t, uint8_t,
    int *);
static void osiop_select(struct osiop_softc *);
static void osiop_update_xfer_mode(struct osiop_softc *, int);
static void scsi_period_to_osiop(struct osiop_softc *, int);
static void osiop_timeout(void *);

int osiop_reset_delay = 250;	/* delay after reset, in milliseconds */

#ifdef OSIOP_DEBUG
#define DEBUG_DMA	0x01
#define DEBUG_INT	0x02
#define DEBUG_PHASE	0x04
#define DEBUG_UNEXCEPT	0x08
#define DEBUG_DISC	0x10
#define DEBUG_CMD	0x20
#define DEBUG_ALL	0xff
int osiop_debug = 0; /*DEBUG_ALL;*/

int osiopsync_debug = 0;
int osiopdma_hits = 1;
int osiopstarts = 0;
int osiopints = 0;
int osiopphmm = 0;
int osiop_trix = 0;
#define OSIOP_TRACE_SIZE	128
#define OSIOP_TRACE(a,b,c,d)	do {				\
	osiop_trbuf[osiop_trix + 0] = (a);			\
	osiop_trbuf[osiop_trix + 1] = (b);			\
	osiop_trbuf[osiop_trix + 2] = (c);			\
	osiop_trbuf[osiop_trix + 3] = (d);			\
	osiop_trix = (osiop_trix + 4) & (OSIOP_TRACE_SIZE - 1);	\
} while (0)
uint8_t osiop_trbuf[OSIOP_TRACE_SIZE];
void osiop_dump_trace(void);
void osiop_dump_acb(struct osiop_acb *);
void osiop_dump(struct osiop_softc *);
#else
#define OSIOP_TRACE(a,b,c,d)
#endif

void
osiop_attach(struct osiop_softc *sc)
{
	struct osiop_acb *acb;
	bus_dma_segment_t seg;
	int nseg;
	int i, err;

	/*
	 * Allocate and map DMA-safe memory for the script.
	 */
	err = bus_dmamem_alloc(sc->sc_dmat, PAGE_SIZE, PAGE_SIZE, 0,
	    &seg, 1, &nseg, BUS_DMA_NOWAIT);
	if (err) {
		aprint_error(": failed to allocate script memory, err=%d\n",
		    err);
		return;
	}
	err = bus_dmamem_map(sc->sc_dmat, &seg, nseg, PAGE_SIZE,
	    (void **)&sc->sc_script, BUS_DMA_NOWAIT | BUS_DMA_COHERENT);
	if (err) {
		aprint_error(": failed to map script memory, err=%d\n", err);
		return;
	}
	err = bus_dmamap_create(sc->sc_dmat, PAGE_SIZE, 1, PAGE_SIZE, 0,
	    BUS_DMA_NOWAIT, &sc->sc_scrdma);
	if (err) {
		aprint_error(": failed to create script map, err=%d\n", err);
		return;
	}
	err = bus_dmamap_load(sc->sc_dmat, sc->sc_scrdma,
	    sc->sc_script, PAGE_SIZE, NULL, BUS_DMA_NOWAIT);
	if (err) {
		aprint_error(": failed to load script map, err=%d\n", err);
		return;
	}

	/*
	 * Copy and sync script
	 */
	memcpy(sc->sc_script, osiop_script, sizeof(osiop_script));
	bus_dmamap_sync(sc->sc_dmat, sc->sc_scrdma, 0, sizeof(osiop_script),
	    BUS_DMASYNC_PREWRITE);

	/*
	 * Allocate and map DMA-safe memory for the script data structure.
	 */
	err = bus_dmamem_alloc(sc->sc_dmat,
	    sizeof(struct osiop_ds) * OSIOP_NACB, PAGE_SIZE, 0,
	    &seg, 1, &nseg, BUS_DMA_NOWAIT);
	if (err) {
		aprint_error(": failed to allocate ds memory, err=%d\n", err);
		return;
	}
	err = bus_dmamem_map(sc->sc_dmat, &seg, nseg,
	    sizeof(struct osiop_ds) * OSIOP_NACB, (void **)&sc->sc_ds,
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT);
	if (err) {
		aprint_error(": failed to map ds memory, err=%d\n", err);
		return;
	}
	err = bus_dmamap_create(sc->sc_dmat,
	    sizeof(struct osiop_ds) * OSIOP_NACB, 1,
	    sizeof(struct osiop_ds) * OSIOP_NACB, 0,
	    BUS_DMA_NOWAIT, &sc->sc_dsdma);
	if (err) {
		aprint_error(": failed to create ds map, err=%d\n", err);
		return;
	}
	err = bus_dmamap_load(sc->sc_dmat, sc->sc_dsdma, sc->sc_ds,
	    sizeof(struct osiop_ds) * OSIOP_NACB, NULL, BUS_DMA_NOWAIT);
	if (err) {
		aprint_error(": failed to load ds map, err=%d\n", err);
		return;
	}

	acb = malloc(sizeof(struct osiop_acb) * OSIOP_NACB,
	    M_DEVBUF, M_NOWAIT|M_ZERO);
	if (acb == NULL) {
		aprint_error(": can't allocate memory for acb\n");
		return;
	}
	sc->sc_acb = acb;
	sc->sc_cfflags = device_cfdata(sc->sc_dev)->cf_flags;
	sc->sc_nexus = NULL;
	sc->sc_active = 0;
	memset(sc->sc_tinfo, 0, sizeof(sc->sc_tinfo));

	/* Initialize command block queue */
	TAILQ_INIT(&sc->ready_list);
	TAILQ_INIT(&sc->nexus_list);
	TAILQ_INIT(&sc->free_list);

	/* Initialize each command block */
	for (i = 0; i < OSIOP_NACB; i++) {
		bus_addr_t dsa;

		err = bus_dmamap_create(sc->sc_dmat, OSIOP_MAX_XFER, OSIOP_NSG,
		    OSIOP_MAX_XFER, 0, BUS_DMA_NOWAIT, &acb->datadma);
		if (err) {
			aprint_error(": failed to create datadma map, err=%d\n",
			    err);
			return;
		}

		acb->sc = sc;
		acb->ds = &sc->sc_ds[i];
		acb->dsoffset = sizeof(struct osiop_ds) * i;

		dsa = sc->sc_dsdma->dm_segs[0].ds_addr + acb->dsoffset;
		acb->ds->id.addr = dsa + OSIOP_DSIDOFF;
		acb->ds->cmd.addr = dsa + OSIOP_DSCMDOFF;
		acb->ds->status.count = 1;
		acb->ds->status.addr = dsa + OSIOP_DSSTATOFF;
		acb->ds->msg.count = 1;
		acb->ds->msg.addr = dsa + OSIOP_DSMSGOFF;
		acb->ds->msgin.count = 1;
		acb->ds->msgin.addr = dsa + OSIOP_DSMSGINOFF;
		acb->ds->extmsg.count = 1;
		acb->ds->extmsg.addr = dsa + OSIOP_DSEXTMSGOFF;
		acb->ds->synmsg.count = 3;
		acb->ds->synmsg.addr = dsa + OSIOP_DSSYNMSGOFF;
		TAILQ_INSERT_TAIL(&sc->free_list, acb, chain);

		acb++;
	}

	aprint_normal(": NCR53C710 rev %d, %dMHz, SCSI ID %d\n",
	    osiop_read_1(sc, OSIOP_CTEST8) >> 4, sc->sc_clock_freq, sc->sc_id);

	/*
	 * Initialize all
	 */
	osiop_init(sc);

	/*
	 * Fill in the adapter.
	 */
	sc->sc_adapter.adapt_dev = sc->sc_dev;
	sc->sc_adapter.adapt_nchannels = 1;
	sc->sc_adapter.adapt_openings = OSIOP_NACB;
	sc->sc_adapter.adapt_max_periph = 1;
	sc->sc_adapter.adapt_ioctl = NULL;
	sc->sc_adapter.adapt_minphys = osiop_minphys;
	sc->sc_adapter.adapt_request = osiop_scsipi_request;

	/*
	 * Fill in the channel.
	 */
	sc->sc_channel.chan_adapter = &sc->sc_adapter;
	sc->sc_channel.chan_bustype = &scsi_bustype;
	sc->sc_channel.chan_channel = 0;
	sc->sc_channel.chan_ntargets = OSIOP_NTGT;
	sc->sc_channel.chan_nluns = 8;
	sc->sc_channel.chan_id = sc->sc_id;

	/*
	 * Now try to attach all the sub devices.
	 */
	config_found(sc->sc_dev, &sc->sc_channel, scsiprint);
}

/*
 * default minphys routine for osiop based controllers
 */
void
osiop_minphys(struct buf *bp)
{

	if (bp->b_bcount > OSIOP_MAX_XFER)
		bp->b_bcount = OSIOP_MAX_XFER;
	minphys(bp);
}

/*
 * used by specific osiop controller
 *
 */
void
osiop_scsipi_request(struct scsipi_channel *chan, scsipi_adapter_req_t req,
    void *arg)
{
	struct scsipi_xfer *xs;
	struct osiop_acb *acb;
	struct osiop_softc *sc;
	int err, flags, s;

	sc = device_private(chan->chan_adapter->adapt_dev);

	switch (req) {
	case ADAPTER_REQ_RUN_XFER:
		xs = arg;
		flags = xs->xs_control;

		/* XXXX ?? */
		if (flags & XS_CTL_DATA_UIO)
			panic("osiop: scsi data uio requested");

		/* XXXX ?? */
		if (sc->sc_nexus && flags & XS_CTL_POLL)
#if 0
			panic("osiop_scsicmd: busy");
#else
			printf("osiop_scsicmd: busy\n");
#endif

		s = splbio();
		acb = TAILQ_FIRST(&sc->free_list);
		if (acb != NULL) {
			TAILQ_REMOVE(&sc->free_list, acb, chain);
		}
#ifdef DIAGNOSTIC
		else {
			scsipi_printaddr(xs->xs_periph);
			printf("unable to allocate acb\n");
			panic("osiop_scsipi_request");
		}
#endif

		acb->status = ACB_S_READY;
		acb->xs = xs;

		/* Setup DMA map for data buffer */
		if (xs->xs_control & (XS_CTL_DATA_IN | XS_CTL_DATA_OUT)) {
			err = bus_dmamap_load(sc->sc_dmat, acb->datadma,
			    xs->data, xs->datalen, NULL,
			    BUS_DMA_NOWAIT | BUS_DMA_STREAMING |
			    ((xs->xs_control & XS_CTL_DATA_IN) ?
			     BUS_DMA_READ : BUS_DMA_WRITE));
			if (err) {
				printf("%s: unable to load data DMA map: %d\n",
				    device_xname(sc->sc_dev), err);
				xs->error = XS_DRIVER_STUFFUP;
				scsipi_done(xs);
				TAILQ_INSERT_TAIL(&sc->free_list, acb, chain);
				splx(s);
				return;
			}
		}

		acb->cmdlen = xs->cmdlen;
		acb->datalen = xs->datalen;
#ifdef OSIOP_DEBUG
		acb->data = xs->data;
#endif

		TAILQ_INSERT_TAIL(&sc->ready_list, acb, chain);

		if (sc->sc_nexus == NULL)
			osiop_sched(sc);

		splx(s);

		if (flags & XS_CTL_POLL || sc->sc_flags & OSIOP_NODMA)
			osiop_poll(sc, acb);
		return;

	case ADAPTER_REQ_GROW_RESOURCES:
		return;

	case ADAPTER_REQ_SET_XFER_MODE:
		{
			struct osiop_tinfo *ti;
			struct scsipi_xfer_mode *xm = arg;

			ti = &sc->sc_tinfo[xm->xm_target];

			if ((xm->xm_mode & PERIPH_CAP_SYNC) != 0 &&
			    (ti->flags & TI_NOSYNC) == 0)
				ti->state = NEG_INIT;

			/*
			 * If we're not going to negotiate, send the
			 * notification now, since it won't happen later.
			 */
			if (ti->state == NEG_DONE)
				osiop_update_xfer_mode(sc, xm->xm_target);

			return;
		}
	}
}

void
osiop_poll(struct osiop_softc *sc, struct osiop_acb *acb)
{
	struct scsipi_xfer *xs = acb->xs;
	int status, i, s, to;
	uint8_t istat, dstat, sstat0;

	s = splbio();
	to = xs->timeout / 1000;
	if (!TAILQ_EMPTY(&sc->nexus_list))
		printf("%s: osiop_poll called with disconnected device\n",
		    device_xname(sc->sc_dev));
	for (;;) {
		i = 1000;
		while (((istat = osiop_read_1(sc, OSIOP_ISTAT)) &
		    (OSIOP_ISTAT_SIP | OSIOP_ISTAT_DIP)) == 0) {
			if (i <= 0) {
#ifdef OSIOP_DEBUG
				printf("waiting: tgt %d cmd %02x sbcl %02x"
				    " dsp %x (+%lx) dcmd %x"
				    " ds %p timeout %d\n",
				    xs->xs_periph->periph_target,
				    xs->cmd->opcode,
				    osiop_read_1(sc, OSIOP_SBCL),
				    osiop_read_4(sc, OSIOP_DSP),
				    osiop_read_4(sc, OSIOP_DSP) -
				        sc->sc_scrdma->dm_segs[0].ds_addr,
				    osiop_read_1(sc, OSIOP_DCMD),
				    acb->ds, acb->xs->timeout);
#endif
				i = 1000;
				to--;
				if (to <= 0) {
					osiop_reset(sc);
					splx(s);
					return;
				}
			}
			delay(1000);
			i--;
		}
		sstat0 = osiop_read_1(sc, OSIOP_SSTAT0);
		delay(25);
		dstat = osiop_read_1(sc, OSIOP_DSTAT);
		if (osiop_checkintr(sc, istat, dstat, sstat0, &status)) {
			if (acb != sc->sc_nexus)
				printf("%s: osiop_poll disconnected device"
				    " completed\n", device_xname(sc->sc_dev));
			else if ((sc->sc_flags & OSIOP_INTDEFER) == 0) {
				sc->sc_flags &= ~OSIOP_INTSOFF;
				osiop_write_1(sc, OSIOP_SIEN, sc->sc_sien);
				osiop_write_1(sc, OSIOP_DIEN, sc->sc_dien);
			}
			osiop_scsidone(sc->sc_nexus, status);
		}

		if (xs->xs_status & XS_STS_DONE)
			break;
	}

	splx(s);
	return;
}

/*
 * start next command that's ready
 */
void
osiop_sched(struct osiop_softc *sc)
{
	struct scsipi_periph *periph;
	struct osiop_acb *acb;
	int i;

#ifdef OSIOP_DEBUG
	if (sc->sc_nexus != NULL) {
		printf("%s: osiop_sched- nexus %p/%d ready %p/%d\n",
		    device_xname(sc->sc_dev), sc->sc_nexus,
		    sc->sc_nexus->xs->xs_periph->periph_target,
		    TAILQ_FIRST(&sc->ready_list),
		    TAILQ_FIRST(&sc->ready_list)->xs->xs_periph->periph_target);
		return;
	}
#endif
	TAILQ_FOREACH(acb, &sc->ready_list, chain) {
		periph = acb->xs->xs_periph;
		i = periph->periph_target;
		if ((sc->sc_tinfo[i].lubusy & (1 << periph->periph_lun)) == 0) {
			struct osiop_tinfo *ti;

			TAILQ_REMOVE(&sc->ready_list, acb, chain);
			sc->sc_nexus = acb;
			ti = &sc->sc_tinfo[i];
			ti->lubusy |= (1 << periph->periph_lun);
			break;
		}
	}

	if (acb == NULL) {
#ifdef OSIOP_DEBUG
		printf("%s: osiop_sched didn't find ready command\n",
		    device_xname(sc->sc_dev));
#endif
		return;
	}

	if (acb->xs->xs_control & XS_CTL_RESET)
		osiop_reset(sc);

	sc->sc_active++;
	osiop_select(sc);
}

void
osiop_scsidone(struct osiop_acb *acb, int status)
{
	struct scsipi_xfer *xs;
	struct scsipi_periph *periph;
	struct osiop_softc *sc;
	int dosched = 0;

#ifdef DIAGNOSTIC
	if (acb == NULL || acb->xs == NULL) {
		printf("osiop_scsidone: NULL acb or scsipi_xfer\n");
#if defined(OSIOP_DEBUG) && defined(DDB)
		Debugger();
#endif
		return;
	}
#endif
	xs = acb->xs;
	sc = acb->sc;
	periph = xs->xs_periph;

#ifdef OSIOP_DEBUG
	if (acb->status != ACB_S_DONE)
		printf("%s: acb not done (status %d)\n",
		    device_xname(sc->sc_dev), acb->status);
#endif

	xs->status = status;

	switch (status) {
	case SCSI_OK:
		xs->error = XS_NOERROR;
		break;
	case SCSI_BUSY:
		xs->error = XS_BUSY;
		break;
	case SCSI_CHECK:
		xs->error = XS_BUSY;
		break;
	case SCSI_OSIOP_NOCHECK:
		/*
		 * don't check status, xs->error is already valid
		 */
		break;
	case SCSI_OSIOP_NOSTATUS:
		/*
		 * the status byte was not updated, cmd was
		 * aborted
		 */
		xs->error = XS_SELTIMEOUT;
		break;
	default:
#ifdef OSIOP_DEBUG
		printf("%s: osiop_scsidone: unknown status code (0x%02x)\n",
		    device_xname(sc->sc_dev), status);
#endif
		xs->error = XS_DRIVER_STUFFUP;
		break;
	}

	if (xs->xs_control & (XS_CTL_DATA_IN | XS_CTL_DATA_OUT)) {
		bus_dmamap_sync(sc->sc_dmat, acb->datadma, 0, acb->datalen,
		    (xs->xs_control & XS_CTL_DATA_IN) ?
		    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, acb->datadma);
	}

	/*
	 * Remove the ACB from whatever queue it's on.  We have to do a bit of
	 * a hack to figure out which queue it's on.  Note that it is *not*
	 * necessary to cdr down the ready queue, but we must cdr down the
	 * nexus queue and see if it's there, so we can mark the unit as no
	 * longer busy.  This code is sickening, but it works.
	 */
	if (acb == sc->sc_nexus) {
		sc->sc_nexus = NULL;
		sc->sc_tinfo[periph->periph_target].lubusy &=
		    ~(1 << periph->periph_lun);
		if (!TAILQ_EMPTY(&sc->ready_list))
			dosched = 1;	/* start next command */
		sc->sc_active--;
		OSIOP_TRACE('d', 'a', status, 0);
	} else if (sc->ready_list.tqh_last == &TAILQ_NEXT(acb, chain)) {
		TAILQ_REMOVE(&sc->ready_list, acb, chain);
		OSIOP_TRACE('d', 'r', status, 0);
	} else {
		struct osiop_acb *acb2;
		TAILQ_FOREACH(acb2, &sc->nexus_list, chain) {
			if (acb2 == acb) {
				TAILQ_REMOVE(&sc->nexus_list, acb, chain);
				sc->sc_tinfo[periph->periph_target].lubusy &=
				    ~(1 << periph->periph_lun);
				sc->sc_active--;
				break;
			}
		}
		if (acb2 == NULL) {
			if (TAILQ_NEXT(acb, chain) != NULL) {
				TAILQ_REMOVE(&sc->ready_list, acb, chain);
				sc->sc_active--;
			} else {
				printf("%s: can't find matching acb\n",
				    device_xname(sc->sc_dev));
#ifdef DDB
#if 0
				Debugger();
#endif
#endif
			}
		}
		OSIOP_TRACE('d', 'n', status, 0);
	}
	/* Put it on the free list. */
	acb->status = ACB_S_FREE;
	TAILQ_INSERT_TAIL(&sc->free_list, acb, chain);
	sc->sc_tinfo[periph->periph_target].cmds++;

	callout_stop(&xs->xs_callout);
	xs->resid = 0;
	scsipi_done(xs);

	if (dosched && sc->sc_nexus == NULL)
		osiop_sched(sc);
}

void
osiop_abort(struct osiop_softc *sc, const char *where)
{

	printf("%s: abort %s: dstat %02x, sstat0 %02x sbcl %02x\n",
	    device_xname(sc->sc_dev), where,
	    osiop_read_1(sc, OSIOP_DSTAT),
	    osiop_read_1(sc, OSIOP_SSTAT0),
	    osiop_read_1(sc, OSIOP_SBCL));

	/* XXX XXX XXX */
	if (sc->sc_active > 0) {
		sc->sc_active = 0;
	}
}

void
osiop_init(struct osiop_softc *sc)
{
	int i, inhibit_sync, inhibit_disc;

	sc->sc_tcp[1] = 1000 / sc->sc_clock_freq;
	sc->sc_tcp[2] = 1500 / sc->sc_clock_freq;
	sc->sc_tcp[3] = 2000 / sc->sc_clock_freq;
	sc->sc_minsync = sc->sc_tcp[1];		/* in 4ns units */

	if (sc->sc_minsync < 25)
		sc->sc_minsync = 25;

	if (sc->sc_clock_freq <= 25) {
		sc->sc_dcntl |= OSIOP_DCNTL_CF_1;	/* SCLK/1 */
		sc->sc_tcp[0] = sc->sc_tcp[1];
	} else if (sc->sc_clock_freq <= 37) {
		sc->sc_dcntl |= OSIOP_DCNTL_CF_1_5;	/* SCLK/1.5 */
		sc->sc_tcp[0] = sc->sc_tcp[2];
	} else if (sc->sc_clock_freq <= 50) {
		sc->sc_dcntl |= OSIOP_DCNTL_CF_2;	/* SCLK/2 */
		sc->sc_tcp[0] = sc->sc_tcp[3];
	} else {
		sc->sc_dcntl |= OSIOP_DCNTL_CF_3;	/* SCLK/3 */
		sc->sc_tcp[0] = 3000 / sc->sc_clock_freq;
	}

	if ((sc->sc_cfflags & 0x10000) != 0) {
		sc->sc_flags |= OSIOP_NODMA;
#ifdef OSIOP_DEBUG
		printf("%s: DMA disabled; use polling\n",
		    device_xname(sc->sc_dev));
#endif
	}

	inhibit_sync = (sc->sc_cfflags & 0xff00) >> 8;	/* XXX */
	inhibit_disc =  sc->sc_cfflags & 0x00ff;	/* XXX */
#ifdef OSIOP_DEBUG
	if (inhibit_sync != 0)
		printf("%s: Inhibiting synchronous transfer: 0x%02x\n",
		    device_xname(sc->sc_dev), inhibit_sync);
	if (inhibit_disc != 0)
		printf("%s: Inhibiting disconnect: 0x%02x\n",
		    device_xname(sc->sc_dev), inhibit_disc);
#endif
	for (i = 0; i < OSIOP_NTGT; i++) {
		if (inhibit_sync & (1 << i))
			sc->sc_tinfo[i].flags |= TI_NOSYNC;
		if (inhibit_disc & (1 << i))
			sc->sc_tinfo[i].flags |= TI_NODISC;
	}

	osiop_resetbus(sc);
	osiop_reset(sc);
}

void
osiop_reset(struct osiop_softc *sc)
{
	struct osiop_acb *acb;
	int i, s;
	uint8_t stat;

#ifdef OSIOP_DEBUG
	printf("%s: resetting chip\n", device_xname(sc->sc_dev));
#endif
	if (sc->sc_flags & OSIOP_ALIVE)
		osiop_abort(sc, "reset");

	s = splbio();

	/*
	 * Reset the chip
	 * XXX - is this really needed?
	 */

	/* abort current script */
	osiop_write_1(sc, OSIOP_ISTAT,
	    osiop_read_1(sc, OSIOP_ISTAT) | OSIOP_ISTAT_ABRT);
	/* reset chip */
	osiop_write_1(sc, OSIOP_ISTAT,
	    osiop_read_1(sc, OSIOP_ISTAT) | OSIOP_ISTAT_RST);
	delay(100);
	osiop_write_1(sc, OSIOP_ISTAT,
	    osiop_read_1(sc, OSIOP_ISTAT) & ~OSIOP_ISTAT_RST);
	delay(100);

	/*
	 * Set up various chip parameters
	 */
	osiop_write_1(sc, OSIOP_SCNTL0,
	    OSIOP_ARB_FULL | OSIOP_SCNTL0_EPC | OSIOP_SCNTL0_EPG);
	osiop_write_1(sc, OSIOP_SCNTL1, OSIOP_SCNTL1_ESR);
	osiop_write_1(sc, OSIOP_DCNTL, sc->sc_dcntl);
	osiop_write_1(sc, OSIOP_DMODE, OSIOP_DMODE_BL4);
	/* don't enable interrupts yet */
	osiop_write_1(sc, OSIOP_SIEN, 0x00);
	osiop_write_1(sc, OSIOP_DIEN, 0x00);
	osiop_write_1(sc, OSIOP_SCID, OSIOP_SCID_VALUE(sc->sc_id));
	osiop_write_1(sc, OSIOP_DWT, 0x00);
	osiop_write_1(sc, OSIOP_CTEST0, osiop_read_1(sc, OSIOP_CTEST0)
	    | OSIOP_CTEST0_BTD | OSIOP_CTEST0_EAN);
	osiop_write_1(sc, OSIOP_CTEST7,
	    osiop_read_1(sc, OSIOP_CTEST7) | sc->sc_ctest7);
	osiop_write_1(sc, OSIOP_CTEST4,
	    osiop_read_1(sc, OSIOP_CTEST4) | sc->sc_ctest4);

	/* will need to re-negotiate sync xfers */
	for (i = 0; i < OSIOP_NTGT; i++) {
		sc->sc_tinfo[i].state = NEG_INIT;
		sc->sc_tinfo[i].period = 0;
		sc->sc_tinfo[i].offset = 0;
	}

	stat = osiop_read_1(sc, OSIOP_ISTAT);
	if (stat & OSIOP_ISTAT_SIP)
		osiop_read_1(sc, OSIOP_SSTAT0);
	delay(25);
	if (stat & OSIOP_ISTAT_DIP)
		osiop_read_1(sc, OSIOP_DSTAT);

	splx(s);

	delay(osiop_reset_delay * 1000);

	if (sc->sc_nexus != NULL) {
		sc->sc_nexus->xs->error =
		    (sc->sc_nexus->flags & ACB_F_TIMEOUT) ?
		    XS_TIMEOUT : XS_RESET;
		sc->sc_nexus->status = ACB_S_DONE;
		sc->sc_nexus->flags = 0;
		osiop_scsidone(sc->sc_nexus, SCSI_OSIOP_NOCHECK);
	}
	while ((acb = TAILQ_FIRST(&sc->nexus_list)) != NULL) {
		acb->xs->error = (acb->flags & ACB_F_TIMEOUT) ?
		    XS_TIMEOUT : XS_RESET;
		acb->status = ACB_S_DONE;
		acb->flags = 0;
		osiop_scsidone(acb, SCSI_OSIOP_NOCHECK);
	}

	sc->sc_flags &= ~(OSIOP_INTDEFER | OSIOP_INTSOFF);
	/* enable SCSI and DMA interrupts */
	sc->sc_sien = OSIOP_SIEN_M_A | OSIOP_SIEN_STO | /*OSIOP_SIEN_SEL |*/
	    OSIOP_SIEN_SGE | OSIOP_SIEN_UDC | OSIOP_SIEN_RST | OSIOP_SIEN_PAR;
	sc->sc_dien = OSIOP_DIEN_BF | OSIOP_DIEN_ABRT | OSIOP_DIEN_SIR |
	    /*OSIOP_DIEN_WTD |*/ OSIOP_DIEN_IID;
	osiop_write_1(sc, OSIOP_SIEN, sc->sc_sien);
	osiop_write_1(sc, OSIOP_DIEN, sc->sc_dien);
}

void
osiop_resetbus(struct osiop_softc *sc)
{

	osiop_write_1(sc, OSIOP_SIEN, 0);
	osiop_write_1(sc, OSIOP_SCNTL1,
	    osiop_read_1(sc, OSIOP_SCNTL1) | OSIOP_SCNTL1_RST);
	delay(25);
	osiop_write_1(sc, OSIOP_SCNTL1,
	    osiop_read_1(sc, OSIOP_SCNTL1) & ~OSIOP_SCNTL1_RST);
}

/*
 * Setup Data Storage for 53C710 and start SCRIPTS processing
 */

void
osiop_start(struct osiop_softc *sc)
{
	struct osiop_acb *acb = sc->sc_nexus;
	struct osiop_ds *ds = acb->ds;
	struct scsipi_xfer *xs = acb->xs;
	bus_dmamap_t dsdma = sc->sc_dsdma, datadma = acb->datadma;
	struct osiop_tinfo *ti;
	int target = xs->xs_periph->periph_target;
	int lun = xs->xs_periph->periph_lun;
	int disconnect, i;

#ifdef OSIOP_DEBUG
	if (osiop_debug & DEBUG_DISC &&
	    osiop_read_1(sc, OSIOP_SBCL) & OSIOP_BSY) {
		printf("ACK! osiop was busy: script %p dsa %p active %d\n",
		    sc->sc_script, acb->ds, sc->sc_active);
		printf("istat %02x sfbr %02x lcrc %02x sien %02x dien %02x\n",
		    osiop_read_1(sc, OSIOP_ISTAT),
		    osiop_read_1(sc, OSIOP_SFBR),
		    osiop_read_1(sc, OSIOP_LCRC),
		    osiop_read_1(sc, OSIOP_SIEN),
		    osiop_read_1(sc, OSIOP_DIEN));
#ifdef DDB
#if 0
		Debugger();
#endif
#endif
	}
#endif

#ifdef OSIOP_DEBUG
	if (acb->status != ACB_S_READY)
		panic("osiop_start: non-ready cmd in acb");
#endif

	acb->intstat = 0;

	/* Copy SCSI command to DMA buffer */
	memcpy(ds->scsipi_cmd, xs->cmd, acb->cmdlen);
	ds->cmd.count = acb->cmdlen;

	ti = &sc->sc_tinfo[target];
	ds->scsi_addr = ((1 << 16) << target) | (ti->sxfer << 8);

	disconnect = (xs->xs_control & XS_CTL_REQSENSE) == 0 &&
	    (ti->flags & TI_NODISC) == 0;

	ds->msgout[0] = MSG_IDENTIFY(lun, disconnect);
	ds->id.count = 1;
	ds->stat[0] = SCSI_OSIOP_NOSTATUS;	/* set invalid status */
	ds->msgbuf[0] = ds->msgbuf[1] = MSG_INVALID;
	memset(&ds->data, 0, sizeof(ds->data));

	/*
	 * Negotiate wide is the initial negotiation state;  since the 53c710
	 * doesn't do wide transfers, just begin the synchronous transfer
	 * negotiation here.
	 */
	if (ti->state == NEG_INIT) {
		if ((ti->flags & TI_NOSYNC) != 0) {
			ti->state = NEG_DONE;
			ti->sbcl = 0;
			ti->sxfer = 0;
			ti->period = 0;
			ti->offset = 0;
			osiop_update_xfer_mode(sc, target);
#ifdef OSIOP_DEBUG
			if (osiopsync_debug)
				printf("Forcing target %d asynchronous\n",
				    target);
#endif
		} else {
			ds->msgbuf[2] = MSG_INVALID;
			ds->msgout[1] = MSG_EXTENDED;
			ds->msgout[2] = MSG_EXT_SDTR_LEN;
			ds->msgout[3] = MSG_EXT_SDTR;
			ds->msgout[4] = sc->sc_minsync;
			ds->msgout[5] = OSIOP_MAX_OFFSET;
			ds->id.count = MSG_EXT_SDTR_LEN + 3;
			ti->state = NEG_WAITS;
#ifdef OSIOP_DEBUG
			if (osiopsync_debug)
				printf("Sending sync request to target %d\n",
				    target);
#endif
		}
	}

	acb->curaddr = 0;
	acb->curlen = 0;

	/*
	 * Build physical DMA addresses for scatter/gather I/O
	 */
	if (xs->xs_control & (XS_CTL_DATA_IN | XS_CTL_DATA_OUT)) {
		for (i = 0; i < datadma->dm_nsegs; i++) {
			ds->data[i].count = datadma->dm_segs[i].ds_len;
			ds->data[i].addr  = datadma->dm_segs[i].ds_addr;
		}
		/* sync xfer data buffer */
		bus_dmamap_sync(sc->sc_dmat, acb->datadma,
		    0, acb->datalen, (xs->xs_control & XS_CTL_DATA_IN) ?
		    BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE);
	}

	/* sync script data structure */
	bus_dmamap_sync(sc->sc_dmat, dsdma,
	    acb->dsoffset, sizeof(struct osiop_ds),
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	acb->status = ACB_S_ACTIVE;

	/* handle timeout */
	if ((xs->xs_control & XS_CTL_POLL) == 0) {
		int timeout = mstohz(acb->xs->timeout);
		/* start expire timer */
		if (timeout == 0)
			timeout = 1;
		callout_reset(&xs->xs_callout, timeout,
		    osiop_timeout, acb);
	}
#ifdef OSIOP_DEBUG
	if (osiop_debug & DEBUG_DISC &&
	    osiop_read_1(sc, OSIOP_SBCL) & OSIOP_BSY) {
		printf("ACK! osiop was busy at start: "
		    "script %p dsa %p active %d\n",
		    sc->sc_script, acb->ds, sc->sc_active);
#ifdef DDB
#if 0
		Debugger();
#endif
#endif
	}
#endif
	if (TAILQ_EMPTY(&sc->nexus_list)) {
		if (osiop_read_1(sc, OSIOP_ISTAT) & OSIOP_ISTAT_CON)
			printf("%s: osiop_select while connected?\n",
			    device_xname(sc->sc_dev));
		osiop_write_4(sc, OSIOP_TEMP, 0);
		osiop_write_1(sc, OSIOP_SBCL, ti->sbcl);
		osiop_write_4(sc, OSIOP_DSA,
		    dsdma->dm_segs[0].ds_addr + acb->dsoffset);
		osiop_write_4(sc, OSIOP_DSP,
		    sc->sc_scrdma->dm_segs[0].ds_addr + Ent_scripts);
		OSIOP_TRACE('s', 1, 0, 0);
	} else {
		if ((osiop_read_1(sc, OSIOP_ISTAT) & OSIOP_ISTAT_CON) == 0) {
			osiop_write_1(sc, OSIOP_ISTAT, OSIOP_ISTAT_SIGP);
			OSIOP_TRACE('s', 2, 0, 0);
		} else {
			OSIOP_TRACE('s', 3,
			    osiop_read_1(sc, OSIOP_ISTAT), 0);
		}
	}
#ifdef OSIOP_DEBUG
	osiopstarts++;
#endif
}

/*
 * Process a DMA or SCSI interrupt from the 53C710 SIOP
 */

int
osiop_checkintr(struct osiop_softc *sc, uint8_t istat, uint8_t dstat,
    uint8_t sstat0, int *status)
{
	struct osiop_acb *acb = sc->sc_nexus;
	struct osiop_ds *ds = NULL;	/* XXX */
	bus_dmamap_t dsdma = sc->sc_dsdma;
	bus_addr_t scraddr = sc->sc_scrdma->dm_segs[0].ds_addr;
	int target = 0;
	int dfifo, dbc, intcode, sstat1;

	dfifo = osiop_read_1(sc, OSIOP_DFIFO);
	dbc = osiop_read_4(sc, OSIOP_DBC) & 0x00ffffff;
	sstat1 = osiop_read_1(sc, OSIOP_SSTAT1);
	osiop_write_1(sc, OSIOP_CTEST8,
	    osiop_read_1(sc, OSIOP_CTEST8) | OSIOP_CTEST8_CLF);
	while ((osiop_read_1(sc, OSIOP_CTEST1) & OSIOP_CTEST1_FMT) !=
	    OSIOP_CTEST1_FMT)
		;
	osiop_write_1(sc, OSIOP_CTEST8,
	    osiop_read_1(sc, OSIOP_CTEST8) & ~OSIOP_CTEST8_CLF);
	intcode = osiop_read_4(sc, OSIOP_DSPS);
#ifdef OSIOP_DEBUG
	osiopints++;
	if (osiop_read_4(sc, OSIOP_DSP) != 0 &&
	    (osiop_read_4(sc, OSIOP_DSP) < scraddr ||
	    osiop_read_4(sc, OSIOP_DSP) >= scraddr + sizeof(osiop_script))) {
		printf("%s: dsp not within script dsp %x scripts %lx:%lx",
		    device_xname(sc->sc_dev),
		    osiop_read_4(sc, OSIOP_DSP),
		    scraddr, scraddr + sizeof(osiop_script));
		printf(" istat %x dstat %x sstat0 %x\n", istat, dstat, sstat0);
#ifdef DDB
		Debugger();
#endif
	}
#endif
	OSIOP_TRACE('i', dstat, istat, (istat & OSIOP_ISTAT_DIP) ?
	    intcode & 0xff : sstat0);

	if (acb != NULL) { /* XXX */
		ds = acb->ds;
		bus_dmamap_sync(sc->sc_dmat, dsdma,
		    acb->dsoffset, sizeof(struct osiop_ds),
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
#ifdef OSIOP_DEBUG
		if (acb->status != ACB_S_ACTIVE)
			printf("osiop_checkintr: acb not active (status %d)\n",
			    acb->status);
#endif
	}


	if (dstat & OSIOP_DSTAT_SIR && intcode == A_ok) {
		/* Normal completion status, or check condition */
		struct osiop_tinfo *ti;

		if (acb == NULL) {
			printf("%s: COMPLETE with no active command?\n",
			    device_xname(sc->sc_dev));
			goto bad_phase;
		}
#ifdef OSIOP_DEBUG
		if (osiop_read_4(sc, OSIOP_DSA) !=
		    dsdma->dm_segs[0].ds_addr + acb->dsoffset) {
			printf("osiop: invalid dsa: %x %lx\n",
			    osiop_read_4(sc, OSIOP_DSA),
			    dsdma->dm_segs[0].ds_addr + acb->dsoffset);
			panic("*** osiop DSA invalid ***");
		}
#endif
		target = acb->xs->xs_periph->periph_target;
		ti = &sc->sc_tinfo[target];
		if (ti->state == NEG_WAITS) {
			if (ds->msgbuf[1] == MSG_INVALID)
				printf("%s: target %d ignored sync request\n",
				    device_xname(sc->sc_dev), target);
			else if (ds->msgbuf[1] == MSG_MESSAGE_REJECT)
				printf("%s: target %d rejected sync request\n",
				    device_xname(sc->sc_dev), target);
			ti->period = 0;
			ti->offset = 0;
			osiop_update_xfer_mode(sc, target);
			ti->state = NEG_DONE;
		}
#ifdef OSIOP_DEBUG
		if (osiop_read_1(sc, OSIOP_SBCL) & OSIOP_BSY) {
#if 0
			printf("ACK! osiop was busy at end: "
			    "script %p dsa %p\n", &osiop_script, ds);
#ifdef DDB
			Debugger();
#endif
#endif
		}
		if (ds->msgbuf[0] != MSG_CMDCOMPLETE)
			printf("%s: message was not COMMAND COMPLETE: %02x\n",
			    device_xname(sc->sc_dev), ds->msgbuf[0]);
#endif
		if (!TAILQ_EMPTY(&sc->nexus_list))
			osiop_write_1(sc, OSIOP_DCNTL,
			    osiop_read_1(sc, OSIOP_DCNTL) | OSIOP_DCNTL_STD);
		*status = ds->stat[0];
		acb->status = ACB_S_DONE;
		return (1);
	}
	if (dstat & OSIOP_DSTAT_SIR && intcode == A_int_syncmsg) {
		if (acb == NULL) {
			printf("%s: sync message with no active command?\n",
			    device_xname(sc->sc_dev));
			goto bad_phase;
		}
		target = acb->xs->xs_periph->periph_target;
		if (ds->msgbuf[1] == MSG_EXTENDED &&
		    ds->msgbuf[2] == MSG_EXT_SDTR_LEN &&
		    ds->msgbuf[3] == MSG_EXT_SDTR) {
			struct osiop_tinfo *ti = &sc->sc_tinfo[target];
#ifdef OSIOP_DEBUG
			if (osiopsync_debug)
				printf("sync msg in: "
				    "%02x %02x %02x %02x %02x %02x\n",
				    ds->msgbuf[0], ds->msgbuf[1],
				    ds->msgbuf[2], ds->msgbuf[3],
				    ds->msgbuf[4], ds->msgbuf[5]);
#endif
			ti->period = ds->msgbuf[4];
			ti->offset = ds->msgbuf[5];
			ti->sxfer = 0;
			ti->sbcl = 0;
			if (ds->msgbuf[5] != 0)
				scsi_period_to_osiop(sc, target);
			osiop_update_xfer_mode(sc, target);

			bus_dmamap_sync(sc->sc_dmat, dsdma,
			    acb->dsoffset, sizeof(struct osiop_ds),
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			osiop_write_1(sc, OSIOP_SXFER, ti->sxfer);
			osiop_write_1(sc, OSIOP_SBCL, ti->sbcl);
			if (ti->state == NEG_WAITS) {
				ti->state = NEG_DONE;
				osiop_write_4(sc, OSIOP_DSP,
				    scraddr + Ent_clear_ack);
				return (0);
			}
			osiop_write_1(sc, OSIOP_DCNTL,
			    osiop_read_1(sc, OSIOP_DCNTL) | OSIOP_DCNTL_STD);
			ti->state = NEG_DONE;
			return (0);
		}
		/* XXX - not SDTR message */
	}
	if (sstat0 & OSIOP_SSTAT0_M_A) {
		/* Phase mismatch */
#ifdef OSIOP_DEBUG
		osiopphmm++;
#endif
		if (acb == NULL) {
			printf("%s: Phase mismatch with no active command?\n",
			    device_xname(sc->sc_dev));
			goto bad_phase;
		}
		if (acb->datalen > 0) {
			int adjust = (dfifo - (dbc & 0x7f)) & 0x7f;
			if (sstat1 & OSIOP_SSTAT1_ORF)
				adjust++;
			if (sstat1 & OSIOP_SSTAT1_OLF)
				adjust++;
			acb->curaddr = osiop_read_4(sc, OSIOP_DNAD) - adjust;
			acb->curlen = dbc + adjust;
#ifdef OSIOP_DEBUG
			if (osiop_debug & DEBUG_DISC) {
				printf("Phase mismatch: curaddr %lx "
				    "curlen %lx dfifo %x dbc %x sstat1 %x "
				    "adjust %x sbcl %x starts %d acb %p\n",
				    acb->curaddr, acb->curlen, dfifo,
				    dbc, sstat1, adjust,
				    osiop_read_1(sc, OSIOP_SBCL),
				    osiopstarts, acb);
				if (ds->data[1].count != 0) {
					int i;
					for (i = 0; ds->data[i].count != 0; i++)
						printf("chain[%d] "
						    "addr %x len %x\n", i,
						    ds->data[i].addr,
						    ds->data[i].count);
				}
				bus_dmamap_sync(sc->sc_dmat, dsdma,
				    acb->dsoffset, sizeof(struct osiop_ds),
				    BUS_DMASYNC_PREREAD |
				    BUS_DMASYNC_PREWRITE);
			}
#endif
		}
#ifdef OSIOP_DEBUG
		OSIOP_TRACE('m', osiop_read_1(sc, OSIOP_SBCL),
		    osiop_read_4(sc, OSIOP_DSP) >> 8,
		    osiop_read_4(sc, OSIOP_DSP));
		if (osiop_debug & DEBUG_PHASE)
			printf("Phase mismatch: %x dsp +%lx dcmd %x\n",
			    osiop_read_1(sc, OSIOP_SBCL),
			    osiop_read_4(sc, OSIOP_DSP) - scraddr,
			    osiop_read_4(sc, OSIOP_DBC));
#endif
		if ((osiop_read_1(sc, OSIOP_SBCL) & OSIOP_REQ) == 0) {
			printf("Phase mismatch: "
			    "REQ not asserted! %02x dsp %x\n",
			    osiop_read_1(sc, OSIOP_SBCL),
			    osiop_read_4(sc, OSIOP_DSP));
#if defined(OSIOP_DEBUG) && defined(DDB)
			/*Debugger(); XXX is*/
#endif
		}
		switch (OSIOP_PHASE(osiop_read_1(sc, OSIOP_SBCL))) {
		case DATA_OUT_PHASE:
		case DATA_IN_PHASE:
		case STATUS_PHASE:
		case COMMAND_PHASE:
		case MSG_IN_PHASE:
		case MSG_OUT_PHASE:
			osiop_write_4(sc, OSIOP_DSP, scraddr + Ent_switch);
			break;
		default:
			printf("%s: invalid phase\n", device_xname(sc->sc_dev));
			goto bad_phase;
		}
		return (0);
	}
	if (sstat0 & OSIOP_SSTAT0_STO) {
		/* Select timed out */
		if (acb == NULL) {
			printf("%s: Select timeout with no active command?\n",
			    device_xname(sc->sc_dev));
			goto bad_phase;
		}
#ifdef OSIOP_DEBUG
		if (osiop_read_1(sc, OSIOP_SBCL) & OSIOP_BSY) {
			printf("ACK! osiop was busy at timeout: "
			    "script %p dsa %lx\n", sc->sc_script,
			    dsdma->dm_segs[0].ds_addr + acb->dsoffset);
			printf(" sbcl %x sdid %x "
			    "istat %x dstat %x sstat0 %x\n",
			    osiop_read_1(sc, OSIOP_SBCL),
			    osiop_read_1(sc, OSIOP_SDID),
			    istat, dstat, sstat0);
			if ((osiop_read_1(sc, OSIOP_SBCL) & OSIOP_BSY) == 0) {
				printf("Yikes, it's not busy now!\n");
#if 0
				*status = SCSI_OSIOP_NOSTATUS;
				if (!TAILQ_EMPTY(&sc->nexus_list))
					osiop_write_4(sc, OSIOP_DSP,
					    scraddr + Ent_wait_reselect);
				return (1);
#endif
			}
#if 0
			osiop_write_1(sc, OSIOP_DCNTL,
			    osiop_read_1(sc, OSIOP_DCNTL) | OSIOP_DCNTL_STD);
#endif
#ifdef DDB
			Debugger();
#endif
			return (0);
		}
#endif
		acb->status = ACB_S_DONE;
		*status = SCSI_OSIOP_NOSTATUS;
		acb->xs->error = XS_SELTIMEOUT;
		if (!TAILQ_EMPTY(&sc->nexus_list))
			osiop_write_4(sc, OSIOP_DSP,
			    scraddr + Ent_wait_reselect);
		return (1);
	}
	if (acb != NULL)
		target = acb->xs->xs_periph->periph_target;
	else
		target = sc->sc_id;
	if (sstat0 & OSIOP_SSTAT0_UDC) {
		printf("%s: target %d disconnected unexpectedly",
		    device_xname(sc->sc_dev), target);
		if (acb == NULL)
			printf("with no active command?");
		printf("\n");
#if 0
		osiop_abort(sc, "osiop_chkintr");
#endif
		*status = SCSI_CHECK;
		if (!TAILQ_EMPTY(&sc->nexus_list))
			osiop_write_4(sc, OSIOP_DSP,
			    scraddr + Ent_wait_reselect);
		return (acb != NULL);
	}
	if (dstat & OSIOP_DSTAT_SIR &&
	    (intcode == A_int_disc || intcode == A_int_disc_wodp)) {
		/* Disconnect */
		if (acb == NULL) {
			printf("%s: Disconnect with no active command?\n",
			    device_xname(sc->sc_dev));
			return (0);
		}
#ifdef OSIOP_DEBUG
		if (osiop_debug & DEBUG_DISC) {
			printf("%s: ID %02x disconnected TEMP %x (+%lx) "
			    "curaddr %lx curlen %lx buf %x len %x dfifo %x "
			    "dbc %x sstat1 %x starts %d acb %p\n",
			    device_xname(sc->sc_dev), 1 << target,
			    osiop_read_4(sc, OSIOP_TEMP),
			    (osiop_read_4(sc, OSIOP_TEMP) != 0) ?
			        osiop_read_4(sc, OSIOP_TEMP) - scraddr : 0,
			    acb->curaddr, acb->curlen,
			    ds->data[0].addr, ds->data[0].count,
			    dfifo, dbc, sstat1, osiopstarts, acb);
			bus_dmamap_sync(sc->sc_dmat, dsdma,
			    acb->dsoffset, sizeof(struct osiop_ds),
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		}
#endif
		/*
		 * XXXX need to update curaddr/curlen to reflect
		 * current data transferred.  If device disconnected in
		 * the middle of a DMA block, they should already be set
		 * by the phase change interrupt.  If the disconnect
		 * occurs on a DMA block boundary, we have to figure out
		 * which DMA block it was.
		 */
		if (acb->datalen > 0 &&
		    osiop_read_4(sc, OSIOP_TEMP) != 0) {
			long n = osiop_read_4(sc, OSIOP_TEMP) - scraddr;

			if (acb->curlen != 0 &&
			    acb->curlen != ds->data[0].count)
				printf("%s: curaddr/curlen already set? "
				    "n %lx iob %lx/%lx chain[0] %x/%x\n",
				    device_xname(sc->sc_dev), n,
				    acb->curaddr, acb->curlen,
				    ds->data[0].addr, ds->data[0].count);
			if (n < Ent_datain)
				n = (n - Ent_dataout) / 16;
			else
				n = (n - Ent_datain) / 16;
			if (n < 0 || n >= OSIOP_NSG)
				printf("TEMP invalid %ld\n", n);
			else {
				acb->curaddr = ds->data[n].addr;
				acb->curlen = ds->data[n].count;
			}
#ifdef OSIOP_DEBUG
			if (osiop_debug & DEBUG_DISC) {
				printf("%s: TEMP offset %ld",
				    device_xname(sc->sc_dev), n);
				printf(" curaddr %lx curlen %lx\n",
				    acb->curaddr, acb->curlen);
			}
#endif
		}
		/*
		 * If data transfer was interrupted by disconnect, curaddr
		 * and curlen should reflect the point of interruption.
		 * Adjust the DMA chain so that the data transfer begins
		 * at the appropriate place upon reselection.
		 * XXX This should only be done on save data pointer message?
		 */
		if (acb->curlen > 0) {
			int i, j;

#ifdef OSIOP_DEBUG
			if (osiop_debug & DEBUG_DISC)
				printf("%s: adjusting DMA chain\n",
				    device_xname(sc->sc_dev));
			if (intcode == A_int_disc_wodp)
				printf("%s: ID %02x disconnected "
				    "without Save Data Pointers\n",
				    device_xname(sc->sc_dev), 1 << target);
#endif
			for (i = 0; i < OSIOP_NSG; i++) {
				if (ds->data[i].count == 0)
					break;
				if (acb->curaddr >= ds->data[i].addr &&
				    acb->curaddr <
				    (ds->data[i].addr + ds->data[i].count))
					break;
			}
			if (i >= OSIOP_NSG || ds->data[i].count == 0) {
				printf("couldn't find saved data pointer: "
				    "curaddr %lx curlen %lx i %d\n",
				    acb->curaddr, acb->curlen, i);
#ifdef DDB
				Debugger();
#endif
			}
#ifdef OSIOP_DEBUG
			if (osiop_debug & DEBUG_DISC)
				printf(" chain[0]: %x/%x -> %lx/%lx\n",
				    ds->data[0].addr, ds->data[0].count,
				    acb->curaddr, acb->curlen);
#endif
			ds->data[0].addr = acb->curaddr;
			ds->data[0].count = acb->curlen;
			for (j = 1, i = i + 1;
			    i < OSIOP_NSG && ds->data[i].count > 0;
			    i++, j++) {
#ifdef OSIOP_DEBUG
				if (osiop_debug & DEBUG_DISC)
					printf("  chain[%d]: %x/%x -> %x/%x\n",
					    j, ds->data[j].addr,
					    ds->data[j].count,
					    ds->data[i].addr,
					    ds->data[i].count);
#endif
				ds->data[j].addr  = ds->data[i].addr;
				ds->data[j].count = ds->data[i].count;
			}
			if (j < OSIOP_NSG) {
				ds->data[j].addr  = 0;
				ds->data[j].count = 0;
			}
			bus_dmamap_sync(sc->sc_dmat, dsdma,
			    acb->dsoffset, sizeof(struct osiop_ds),
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		}
		sc->sc_tinfo[target].dconns++;
		/*
		 * add nexus to waiting list
		 * clear nexus
		 * try to start another command for another target/lun
		 */
		acb->intstat = sc->sc_flags & OSIOP_INTSOFF;
		TAILQ_INSERT_TAIL(&sc->nexus_list, acb, chain);
		sc->sc_nexus = NULL;		/* no current device */
		osiop_write_4(sc, OSIOP_DSP, scraddr + Ent_wait_reselect);
		/* XXXX start another command ? */
		if (!TAILQ_EMPTY(&sc->ready_list))
			osiop_sched(sc);
		return (0);
	}
	if (dstat & OSIOP_DSTAT_SIR && intcode == A_int_reconnect) {
		int reselid = ffs(osiop_read_4(sc, OSIOP_SCRATCH) & 0xff) - 1;
		int reselun = osiop_read_1(sc, OSIOP_SFBR) & 0x07;
#ifdef OSIOP_DEBUG
		uint8_t resmsg;
#endif

		/* Reconnect */
		/* XXXX save current SBCL */
		sc->sc_sstat1 = osiop_read_1(sc, OSIOP_SBCL);
#ifdef OSIOP_DEBUG
		if (osiop_debug & DEBUG_DISC)
			printf("%s: target ID %02x reselected dsps %x\n",
			    device_xname(sc->sc_dev), reselid, intcode);
		resmsg = osiop_read_1(sc, OSIOP_SFBR);
		if (!MSG_ISIDENTIFY(resmsg))
			printf("%s: Reselect message in was not identify: "
			    "%02x\n", device_xname(sc->sc_dev), resmsg);
#endif
		if (sc->sc_nexus != NULL) {
			struct scsipi_periph *periph =
			    sc->sc_nexus->xs->xs_periph;
#ifdef OSIOP_DEBUG
			if (osiop_debug & DEBUG_DISC)
				printf("%s: reselect ID %02x w/active\n",
				    device_xname(sc->sc_dev), reselid);
#endif
			TAILQ_INSERT_HEAD(&sc->ready_list,
			    sc->sc_nexus, chain);
			sc->sc_tinfo[periph->periph_target].lubusy
			    &= ~(1 << periph->periph_lun);
			sc->sc_active--;
		}
		/*
		 * locate acb of reselecting device
		 * set sc->sc_nexus to acb
		 */
		TAILQ_FOREACH(acb, &sc->nexus_list, chain) {
			struct scsipi_periph *periph = acb->xs->xs_periph;
			if (reselid != periph->periph_target ||
			    reselun != periph->periph_lun) {
				continue;
			}
			TAILQ_REMOVE(&sc->nexus_list, acb, chain);
			sc->sc_nexus = acb;
			sc->sc_flags |= acb->intstat;
			acb->intstat = 0;
			osiop_write_4(sc, OSIOP_DSA,
			    dsdma->dm_segs[0].ds_addr + acb->dsoffset);
			osiop_write_1(sc, OSIOP_SXFER,
			    sc->sc_tinfo[reselid].sxfer);
			osiop_write_1(sc, OSIOP_SBCL,
			    sc->sc_tinfo[reselid].sbcl);
			break;
		}
		if (acb == NULL) {
			printf("%s: target ID %02x reselect nexus_list %p\n",
			    device_xname(sc->sc_dev), reselid,
			    TAILQ_FIRST(&sc->nexus_list));
			panic("unable to find reselecting device");
		}

		osiop_write_4(sc, OSIOP_TEMP, 0);
		osiop_write_1(sc, OSIOP_DCNTL,
		    osiop_read_1(sc, OSIOP_DCNTL) | OSIOP_DCNTL_STD);
		return (0);
	}
	if (dstat & OSIOP_DSTAT_SIR && intcode == A_int_connect) {
#ifdef OSIOP_DEBUG
		uint8_t ctest2 = osiop_read_1(sc, OSIOP_CTEST2);

		/* reselect was interrupted (by Sig_P or select) */
		if (osiop_debug & DEBUG_DISC ||
		    (ctest2 & OSIOP_CTEST2_SIGP) == 0)
			printf("%s: reselect interrupted (Sig_P?) "
			    "scntl1 %x ctest2 %x sfbr %x istat %x/%x\n",
			    device_xname(sc->sc_dev),
			    osiop_read_1(sc, OSIOP_SCNTL1), ctest2,
			    osiop_read_1(sc, OSIOP_SFBR), istat,
			    osiop_read_1(sc, OSIOP_ISTAT));
#endif
		/* XXX assumes it was not select */
		if (sc->sc_nexus == NULL) {
#ifdef OSIOP_DEBUG
			printf("%s: reselect interrupted, sc_nexus == NULL\n",
			    device_xname(sc->sc_dev));
#if 0
			osiop_dump(sc);
#ifdef DDB
			Debugger();
#endif
#endif
#endif
			osiop_write_1(sc, OSIOP_DCNTL,
			    osiop_read_1(sc, OSIOP_DCNTL) | OSIOP_DCNTL_STD);
			return (0);
		}
		target = sc->sc_nexus->xs->xs_periph->periph_target;
		osiop_write_4(sc, OSIOP_TEMP, 0);
		osiop_write_4(sc, OSIOP_DSA,
		    dsdma->dm_segs[0].ds_addr + sc->sc_nexus->dsoffset);
		osiop_write_1(sc, OSIOP_SXFER, sc->sc_tinfo[target].sxfer);
		osiop_write_1(sc, OSIOP_SBCL, sc->sc_tinfo[target].sbcl);
		osiop_write_4(sc, OSIOP_DSP, scraddr + Ent_scripts);
		return (0);
	}
	if (dstat & OSIOP_DSTAT_SIR && intcode == A_int_msgin) {
		/* Unrecognized message in byte */
		if (acb == NULL) {
			printf("%s: Bad message-in with no active command?\n",
			    device_xname(sc->sc_dev));
			goto bad_phase;
		}
		printf("%s: Unrecognized message in data "
		    "sfbr %x msg %x sbcl %x\n", device_xname(sc->sc_dev),
		    osiop_read_1(sc, OSIOP_SFBR), ds->msgbuf[1],
		    osiop_read_1(sc, OSIOP_SBCL));
		/* what should be done here? */
		osiop_write_4(sc, OSIOP_DSP, scraddr + Ent_clear_ack);
		bus_dmamap_sync(sc->sc_dmat, dsdma,
		    acb->dsoffset, sizeof(struct osiop_ds),
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		return (0);
	}
	if (dstat & OSIOP_DSTAT_SIR && intcode == A_int_status) {
		/* Status phase wasn't followed by message in phase? */
		printf("%s: Status phase not followed by message in phase? "
		    "sbcl %x sbdl %x\n", device_xname(sc->sc_dev),
		    osiop_read_1(sc, OSIOP_SBCL),
		    osiop_read_1(sc, OSIOP_SBDL));
		if (osiop_read_1(sc, OSIOP_SBCL) == 0xa7) {
			/* It is now, just continue the script? */
			osiop_write_1(sc, OSIOP_DCNTL,
			    osiop_read_1(sc, OSIOP_DCNTL) | OSIOP_DCNTL_STD);
			return (0);
		}
	}
	if (dstat & OSIOP_DSTAT_SIR && sstat0 == 0) {
		printf("OSIOP interrupt: %x sts %x msg %x %x sbcl %x\n",
		    intcode, ds->stat[0], ds->msgbuf[0], ds->msgbuf[1],
		    osiop_read_1(sc, OSIOP_SBCL));
		osiop_reset(sc);
		*status = SCSI_OSIOP_NOSTATUS;
		return (0);	/* osiop_reset has cleaned up */
	}
	if (sstat0 & OSIOP_SSTAT0_SGE)
		printf("%s: SCSI Gross Error\n", device_xname(sc->sc_dev));
	if (sstat0 & OSIOP_SSTAT0_PAR)
		printf("%s: Parity Error\n", device_xname(sc->sc_dev));
	if (dstat & OSIOP_DSTAT_IID)
		printf("%s: Invalid instruction detected\n",
		    device_xname(sc->sc_dev));
 bad_phase:
	/*
	 * temporary panic for unhandled conditions
	 * displays various things about the 53C710 status and registers
	 * then panics.
	 * XXXX need to clean this up to print out the info, reset, and continue
	 */
	printf("osiop_chkintr: target %x ds %p\n", target, ds);
	printf("scripts %lx ds %lx dsp %x dcmd %x\n", scraddr,
	    acb ? sc->sc_dsdma->dm_segs[0].ds_addr + acb->dsoffset : 0,
	    osiop_read_4(sc, OSIOP_DSP),
	    osiop_read_4(sc, OSIOP_DBC));
	printf("osiop_chkintr: istat %x dstat %x sstat0 %x "
	    "dsps %x dsa %x sbcl %x sts %x msg %x %x sfbr %x\n",
	    istat, dstat, sstat0, intcode,
	    osiop_read_4(sc, OSIOP_DSA),
	    osiop_read_1(sc, OSIOP_SBCL),
	    ds ? ds->stat[0] : 0,
	    ds ? ds->msgbuf[0] : 0,
	    ds ? ds->msgbuf[1] : 0,
	    osiop_read_1(sc, OSIOP_SFBR));
#ifdef OSIOP_DEBUG
	if (osiop_debug & DEBUG_DMA)
		panic("osiop_chkintr: **** temp ****");
#ifdef DDB
	Debugger();
#endif
#endif
	osiop_reset(sc);	/* hard reset */
	*status = SCSI_OSIOP_NOSTATUS;
	if (acb != NULL)
		acb->status = ACB_S_DONE;
	return (0);		/* osiop_reset cleaned up */
}

void
osiop_select(struct osiop_softc *sc)
{
	struct osiop_acb *acb = sc->sc_nexus;

#ifdef OSIOP_DEBUG
	if (osiop_debug & DEBUG_CMD)
		printf("%s: select ", device_xname(sc->sc_dev));
#endif

	if (acb->xs->xs_control & XS_CTL_POLL || sc->sc_flags & OSIOP_NODMA) {
		sc->sc_flags |= OSIOP_INTSOFF;
		sc->sc_flags &= ~OSIOP_INTDEFER;
		if ((osiop_read_1(sc, OSIOP_ISTAT) & OSIOP_ISTAT_CON) == 0) {
			osiop_write_1(sc, OSIOP_SIEN, 0);
			osiop_write_1(sc, OSIOP_DIEN, 0);
		}
#if 0
	} else if ((sc->sc_flags & OSIOP_INTDEFER) == 0) {
		sc->sc_flags &= ~OSIOP_INTSOFF;
		if ((osiop_read_1(sc, OSIOP_ISTAT) & OSIOP_ISTAT_CON) == 0) {
			osiop_write_1(sc, OSIOP_SIEN, sc->sc_sien);
			osiop_write_1(sc, OSIOP_DIEN, sc->sc_dien);
		}
#endif
	}
#ifdef OSIOP_DEBUG
	if (osiop_debug & DEBUG_CMD)
		printf("osiop_select: target %x cmd %02x ds %p\n",
		    acb->xs->xs_periph->periph_target,
		    acb->xs->cmd->opcode, sc->sc_nexus->ds);
#endif

	osiop_start(sc);

	return;
}

/*
 * 53C710 interrupt handler
 */

void
osiop_intr(struct osiop_softc *sc)
{
	int status, s;
	uint8_t istat, dstat, sstat0;

	s = splbio();

	istat = sc->sc_istat;
	if ((istat & (OSIOP_ISTAT_SIP | OSIOP_ISTAT_DIP)) == 0) {
		splx(s);
		return;
	}

	/* Got a valid interrupt on this device; set by MD handler */
	dstat = sc->sc_dstat;
	sstat0 = sc->sc_sstat0;
	sc->sc_istat = 0;
#ifdef OSIOP_DEBUG
	if (!sc->sc_active) {
		/* XXX needs sync */
		printf("%s: spurious interrupt? "
		    "istat %x dstat %x sstat0 %x nexus %p status %x\n",
		    device_xname(sc->sc_dev),
		    istat, dstat, sstat0, sc->sc_nexus,
		    (sc->sc_nexus != NULL) ? sc->sc_nexus->ds->stat[0] : 0);
	}
#endif

#ifdef OSIOP_DEBUG
	if (osiop_debug & (DEBUG_INT|DEBUG_CMD)) {
		/* XXX needs sync */
		printf("%s: intr istat %x dstat %x sstat0 %x dsps %x "
		    "sbcl %x dsp %x dcmd %x sts %x msg %x\n",
		    device_xname(sc->sc_dev),
		    istat, dstat, sstat0,
		    osiop_read_4(sc, OSIOP_DSPS),
		    osiop_read_1(sc, OSIOP_SBCL),
		    osiop_read_4(sc, OSIOP_DSP),
		    osiop_read_4(sc, OSIOP_DBC),
		    (sc->sc_nexus != NULL) ? sc->sc_nexus->ds->stat[0] : 0,
		    (sc->sc_nexus != NULL) ? sc->sc_nexus->ds->msgbuf[0] : 0);
	}
#endif
	if (sc->sc_flags & OSIOP_INTDEFER) {
		sc->sc_flags &= ~(OSIOP_INTDEFER | OSIOP_INTSOFF);
		osiop_write_1(sc, OSIOP_SIEN, sc->sc_sien);
		osiop_write_1(sc, OSIOP_DIEN, sc->sc_dien);
	}
	if (osiop_checkintr(sc, istat, dstat, sstat0, &status)) {
#if 0
		if (status == SCSI_OSIOP_NOSTATUS)
			printf("osiop_intr: no valid status \n");
#endif
		if ((sc->sc_flags & (OSIOP_INTSOFF | OSIOP_INTDEFER)) !=
		    OSIOP_INTSOFF) {
#if 0
			if (osiop_read_1(sc, OSIOP_SBCL) & OSIOP_BSY) {
				struct scsipi_periph *periph;

				periph = sc->sc_nexus->xs->xs_periph;
				printf("%s: SCSI bus busy at completion"
				    " targ %d sbcl %02x sfbr %x lcrc "
				    "%02x dsp +%x\n", device_xname(sc->sc_dev),
				    periph->periphtarget,
				    osiop_read_1(sc, OSIOP_SBCL),
				    osiop_read_1(sc, OSIOP_SFBR),
				    osiop_read_1(sc, OSIOP_LCRC),
				    osiop_read_4(sc, OSIOP_DSP) -
				        sc->sc_scrdma->dm_segs[0].ds_addr);
			}
#endif
			osiop_scsidone(sc->sc_nexus, status);
		}
	}
	splx(s);
}

void
osiop_update_xfer_mode(struct osiop_softc *sc, int target)
{
	struct osiop_tinfo *tinfo = &sc->sc_tinfo[target];
	struct scsipi_xfer_mode xm;

	xm.xm_target = target;
	xm.xm_mode = 0;
	xm.xm_period = 0;
	xm.xm_offset = 0;

	if (tinfo->period) {
		xm.xm_mode |= PERIPH_CAP_SYNC;
		xm.xm_period = tinfo->period;
		xm.xm_offset = tinfo->offset;
	}

	scsipi_async_event(&sc->sc_channel, ASYNC_EVENT_XFER_MODE, &xm);
}

void
scsi_period_to_osiop(struct osiop_softc *sc, int target)
{
	int period, offset, sxfer, sbcl;

	period = sc->sc_tinfo[target].period;
	offset = sc->sc_tinfo[target].offset;
	for (sbcl = 1; sbcl < 4; sbcl++) {
		sxfer = (period * 4 - 1) / sc->sc_tcp[sbcl] - 3;
		if (sxfer >= 0 && sxfer <= 7)
			break;
	}
	if (sbcl > 3) {
		printf("osiop sync: unable to compute sync params "
		    "for period %d ns\n", period * 4);
		/*
		 * XXX need to pick a value we can do and renegotiate
		 */
		sxfer = sbcl = 0;
	} else {
		sxfer = (sxfer << 4) | ((offset <= OSIOP_MAX_OFFSET) ?
		    offset : OSIOP_MAX_OFFSET);
#ifdef DEBUG_SYNC
		printf("osiop sync: params for period %dns: sxfer %x sbcl %x",
		    period * 4, sxfer, sbcl);
		printf(" actual period %dns\n",
		    sc->sc_tcp[sbcl] * ((sxfer >> 4) + 4));
#endif
	}
	sc->sc_tinfo[target].sxfer = sxfer;
	sc->sc_tinfo[target].sbcl = sbcl;
#ifdef DEBUG_SYNC
	printf("osiop sync: osiop_sxfr %02x, osiop_sbcl %02x\n", sxfer, sbcl);
#endif
}

void
osiop_timeout(void *arg)
{
	struct osiop_acb *acb = arg;
	struct scsipi_xfer *xs = acb->xs;
	struct osiop_softc *sc = acb->sc;
	int s;

	scsipi_printaddr(xs->xs_periph);
	printf("command timeout\n");

	s = splbio();
	/* reset the scsi bus */
	osiop_resetbus(sc);

	/* deactivate callout */
	callout_stop(&xs->xs_callout);
	acb->flags |= ACB_F_TIMEOUT;
	osiop_reset(sc);
	splx(s);
	return;
}

#ifdef OSIOP_DEBUG

#if OSIOP_TRACE_SIZE
void
osiop_dump_trace(void)
{
	int i;

	printf("osiop trace: next index %d\n", osiop_trix);
	i = osiop_trix;
	do {
		printf("%3d: '%c' %02x %02x %02x\n", i,
		    osiop_trbuf[i], osiop_trbuf[i + 1],
		    osiop_trbuf[i + 2], osiop_trbuf[i + 3]);
		i = (i + 4) & (OSIOP_TRACE_SIZE - 1);
	} while (i != osiop_trix);
}
#endif

void
osiop_dump_acb(struct osiop_acb *acb)
{
	uint8_t *b;
	int i;

	printf("acb@%p ", acb);
	if (acb->xs == NULL) {
		printf("<unused>\n");
		return;
	}

	b = (uint8_t *)&acb->xs->cmd;
	printf("(%d:%d) status %2x cmdlen %2ld cmd ",
	    acb->xs->xs_periph->periph_target,
	    acb->xs->xs_periph->periph_lun, acb->status, acb->cmdlen);
	for (i = acb->cmdlen; i > 0; i--)
		printf(" %02x", *b++);
	printf("\n");
	printf("  xs: %p data %p:%04x ", acb->xs, acb->xs->data,
	    acb->xs->datalen);
	printf("va %p:%lx ", acb->data, acb->datalen);
	printf("cur %lx:%lx\n", acb->curaddr, acb->curlen);
}

void
osiop_dump(struct osiop_softc *sc)
{
	struct osiop_acb *acb;
	int i, s;

	s = splbio();
#if OSIOP_TRACE_SIZE
	osiop_dump_trace();
#endif
	printf("%s@%p istat %02x\n",
	    device_xname(sc->sc_dev), sc, osiop_read_1(sc, OSIOP_ISTAT));
	if ((acb = TAILQ_FIRST(&sc->free_list)) != NULL) {
		printf("Free list:\n");
		while (acb) {
			osiop_dump_acb(acb);
			acb = TAILQ_NEXT(acb, chain);
		}
	}
	if ((acb = TAILQ_FIRST(&sc->ready_list)) != NULL) {
		printf("Ready list:\n");
		while (acb) {
			osiop_dump_acb(acb);
			acb = TAILQ_NEXT(acb, chain);
		}
	}
	if ((acb = TAILQ_FIRST(&sc->nexus_list)) != NULL) {
		printf("Nexus list:\n");
		while (acb) {
			osiop_dump_acb(acb);
			acb = TAILQ_NEXT(acb, chain);
		}
	}
	if (sc->sc_nexus) {
		printf("Nexus:\n");
		osiop_dump_acb(sc->sc_nexus);
	}
	for (i = 0; i < OSIOP_NTGT; i++) {
		if (sc->sc_tinfo[i].cmds > 2) {
			printf("tgt %d: cmds %d disc %d lubusy %x\n",
			    i, sc->sc_tinfo[i].cmds,
			    sc->sc_tinfo[i].dconns,
			    sc->sc_tinfo[i].lubusy);
		}
	}
	splx(s);
}
#endif
