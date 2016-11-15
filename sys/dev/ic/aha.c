/*	$NetBSD: aha.c,v 1.61 2010/11/13 13:52:00 uebayasi Exp $	*/

/*-
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum and by Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center.
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
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: aha.c,v 1.61 2010/11/13 13:52:00 uebayasi Exp $");

#include "opt_ddb.h"

#undef AHADIAG

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/ic/ahareg.h>
#include <dev/ic/ahavar.h>

#ifndef DDB
#define Debugger() panic("should call debugger here (aha1542.c)")
#endif /* ! DDB */

#define	AHA_MAXXFER	((AHA_NSEG - 1) << PGSHIFT)

#ifdef AHADEBUG
int	aha_debug = 1;
#endif /* AHADEBUG */

static int	aha_cmd(bus_space_tag_t, bus_space_handle_t,
			struct aha_softc *, int, u_char *, int, u_char *);
static void	aha_finish_ccbs(struct aha_softc *);
static void	aha_free_ccb(struct aha_softc *, struct aha_ccb *);
static int	aha_init_ccb(struct aha_softc *, struct aha_ccb *);
static struct aha_ccb *aha_get_ccb(struct aha_softc *);
static struct aha_ccb *aha_ccb_phys_kv(struct aha_softc *, u_long);
static void	aha_queue_ccb(struct aha_softc *, struct aha_ccb *);
static void	aha_collect_mbo(struct aha_softc *);
static void	aha_start_ccbs(struct aha_softc *);
static void	aha_done(struct aha_softc *, struct aha_ccb *);
static int	aha_init(struct aha_softc *);
static void	aha_inquire_setup_information(struct aha_softc *);
static void	ahaminphys(struct buf *);
static void	aha_scsipi_request(struct scsipi_channel *,
				   scsipi_adapter_req_t, void *);
static int	aha_poll(struct aha_softc *, struct scsipi_xfer *, int);
static void	aha_timeout(void *arg);
static int	aha_create_ccbs(struct aha_softc *, struct aha_ccb *, int);

#define AHA_RESET_TIMEOUT	2000	/* time to wait for reset (mSec) */
#define	AHA_ABORT_TIMEOUT	2000	/* time to wait for abort (mSec) */

/*
 * aha_cmd(iot, ioh, sc, icnt, ibuf, ocnt, obuf)
 *
 * Activate Adapter command
 *    icnt:   number of args (outbound bytes including opcode)
 *    ibuf:   argument buffer
 *    ocnt:   number of expected returned bytes
 *    obuf:   result buffer
 *    wait:   number of seconds to wait for response
 *
 * Performs an adapter command through the ports.  Not to be confused with a
 * scsi command, which is read in via the DMA; one of the adapter commands
 * tells it to read in a scsi command.
 */
static int
aha_cmd(bus_space_tag_t iot, bus_space_handle_t ioh, struct aha_softc *sc,
    int icnt, u_char *ibuf, int ocnt, u_char *obuf)
{
	const char *name;
	int i;
	int wait;
	u_char sts;
	u_char opcode = ibuf[0];

	if (sc != NULL)
		name = device_xname(sc->sc_dev);
	else
		name = "(aha probe)";

	/*
	 * Calculate a reasonable timeout for the command.
	 */
	switch (opcode) {
	case AHA_INQUIRE_DEVICES:
		wait = 90 * 20000;
		break;
	default:
		wait = 1 * 20000;
		break;
	}

	/*
	 * Wait for the adapter to go idle, unless it's one of
	 * the commands which don't need this
	 */
	if (opcode != AHA_MBO_INTR_EN) {
		for (i = 20000; i; i--) {	/* 1 sec? */
			sts = bus_space_read_1(iot, ioh, AHA_STAT_PORT);
			if (sts & AHA_STAT_IDLE)
				break;
			delay(50);
		}
		if (!i) {
			printf("%s: aha_cmd, host not idle(0x%x)\n",
			    name, sts);
			return (1);
		}
	}
	/*
	 * Now that it is idle, if we expect output, preflush the
	 * queue feeding to us.
	 */
	if (ocnt) {
		while ((bus_space_read_1(iot, ioh, AHA_STAT_PORT)) & AHA_STAT_DF)
			bus_space_read_1(iot, ioh, AHA_DATA_PORT);
	}
	/*
	 * Output the command and the number of arguments given
	 * for each byte, first check the port is empty.
	 */
	while (icnt--) {
		for (i = wait; i; i--) {
			sts = bus_space_read_1(iot, ioh, AHA_STAT_PORT);
			if (!(sts & AHA_STAT_CDF))
				break;
			delay(50);
		}
		if (!i) {
			if (opcode != AHA_INQUIRE_REVISION)
				printf("%s: aha_cmd, cmd/data port full\n", name);
			bus_space_write_1(iot, ioh, AHA_CTRL_PORT, AHA_CTRL_SRST);
			return (1);
		}
		bus_space_write_1(iot, ioh, AHA_CMD_PORT, *ibuf++);
	}
	/*
	 * If we expect input, loop that many times, each time,
	 * looking for the data register to have valid data
	 */
	while (ocnt--) {
		for (i = wait; i; i--) {
			sts = bus_space_read_1(iot, ioh, AHA_STAT_PORT);
			if (sts & AHA_STAT_DF)
				break;
			delay(50);
		}
		if (!i) {
			if (opcode != AHA_INQUIRE_REVISION)
				printf("%s: aha_cmd, cmd/data port empty %d\n",
				    name, ocnt);
			bus_space_write_1(iot, ioh, AHA_CTRL_PORT, AHA_CTRL_SRST);
			return (1);
		}
		*obuf++ = bus_space_read_1(iot, ioh, AHA_DATA_PORT);
	}
	/*
	 * Wait for the board to report a finished instruction.
	 * We may get an extra interrupt for the HACC signal, but this is
	 * unimportant.
	 */
	if (opcode != AHA_MBO_INTR_EN) {
		for (i = 20000; i; i--) {	/* 1 sec? */
			sts = bus_space_read_1(iot, ioh, AHA_INTR_PORT);
			/* XXX Need to save this in the interrupt handler? */
			if (sts & AHA_INTR_HACC)
				break;
			delay(50);
		}
		if (!i) {
			printf("%s: aha_cmd, host not finished(0x%x)\n",
			    name, sts);
			return (1);
		}
	}
	bus_space_write_1(iot, ioh, AHA_CTRL_PORT, AHA_CTRL_IRST);
	return (0);
}

void
aha_attach(struct aha_softc *sc, struct aha_probe_data *apd)
{
	struct scsipi_adapter *adapt = &sc->sc_adapter;
	struct scsipi_channel *chan = &sc->sc_channel;

	TAILQ_INIT(&sc->sc_free_ccb);
	TAILQ_INIT(&sc->sc_waiting_ccb);

	/*
	 * Fill in the scsipi_adapter.
	 */
	memset(adapt, 0, sizeof(*adapt));
	adapt->adapt_dev = sc->sc_dev;
	adapt->adapt_nchannels = 1;
	/* adapt_openings initialized below */
	/* adapt_max_periph initialized below */
	adapt->adapt_request = aha_scsipi_request;
	adapt->adapt_minphys = ahaminphys;

	/*
	 * Fill in the scsipi_channel.
	 */
	memset(chan, 0, sizeof(*chan));
	chan->chan_adapter = adapt;
	chan->chan_bustype = &scsi_bustype;
	chan->chan_channel = 0;
	chan->chan_ntargets = 8;
	chan->chan_nluns = 8;
	chan->chan_id = apd->sc_scsi_dev;

	aha_inquire_setup_information(sc);
	if (aha_init(sc) != 0) {
		/* Error during initialization! */
		return;
	}

	/*
	 * ask the adapter what subunits are present
	 */
	config_found(sc->sc_dev, &sc->sc_channel, scsiprint);
}

static void
aha_finish_ccbs(struct aha_softc *sc)
{
	struct aha_mbx_in *wmbi;
	struct aha_ccb *ccb;
	int i;

	wmbi = wmbx->tmbi;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap_control,
	    AHA_MBI_OFF(wmbi), sizeof(struct aha_mbx_in),
	    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

	if (wmbi->stat == AHA_MBI_FREE) {
		for (i = 0; i < AHA_MBX_SIZE; i++) {
			if (wmbi->stat != AHA_MBI_FREE) {
				printf("%s: mbi not in round-robin order\n",
				    device_xname(sc->sc_dev));
				goto AGAIN;
			}
			aha_nextmbx(wmbi, wmbx, mbi);
			bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap_control,
			    AHA_MBI_OFF(wmbi), sizeof(struct aha_mbx_in),
			    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
		}
#ifdef AHADIAGnot
		printf("%s: mbi interrupt with no full mailboxes\n",
		    device_xname(sc->sc_dev));
#endif
		return;
	}

AGAIN:
	do {
		ccb = aha_ccb_phys_kv(sc, phystol(wmbi->ccb_addr));
		if (!ccb) {
			printf("%s: bad mbi ccb pointer; skipping\n",
			    device_xname(sc->sc_dev));
			goto next;
		}

		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap_control,
		    AHA_CCB_OFF(ccb), sizeof(struct aha_ccb),
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

#ifdef AHADEBUG
		if (aha_debug) {
			u_char *cp = ccb->scsi_cmd;
			printf("op=%x %x %x %x %x %x\n",
			    cp[0], cp[1], cp[2], cp[3], cp[4], cp[5]);
			printf("stat %x for mbi addr = %p, ",
			    wmbi->stat, wmbi);
			printf("ccb addr = %p\n", ccb);
		}
#endif /* AHADEBUG */

		switch (wmbi->stat) {
		case AHA_MBI_OK:
		case AHA_MBI_ERROR:
			if ((ccb->flags & CCB_ABORT) != 0) {
				/*
				 * If we already started an abort, wait for it
				 * to complete before clearing the CCB.  We
				 * could instead just clear CCB_SENDING, but
				 * what if the mailbox was already received?
				 * The worst that happens here is that we clear
				 * the CCB a bit later than we need to.  BFD.
				 */
				goto next;
			}
			break;

		case AHA_MBI_ABORT:
		case AHA_MBI_UNKNOWN:
			/*
			 * Even if the CCB wasn't found, we clear it anyway.
			 * See preceding comment.
			 */
			break;

		default:
			printf("%s: bad mbi status %02x; skipping\n",
			    device_xname(sc->sc_dev), wmbi->stat);
			goto next;
		}

		callout_stop(&ccb->xs->xs_callout);
		aha_done(sc, ccb);

	next:
		wmbi->stat = AHA_MBI_FREE;
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap_control,
		    AHA_MBI_OFF(wmbi), sizeof(struct aha_mbx_in),
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
		aha_nextmbx(wmbi, wmbx, mbi);
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap_control,
		    AHA_MBI_OFF(wmbi), sizeof(struct aha_mbx_in),
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
	} while (wmbi->stat != AHA_MBI_FREE);

	wmbx->tmbi = wmbi;
}

/*
 * Catch an interrupt from the adaptor
 */
int
aha_intr(void *arg)
{
	struct aha_softc *sc = arg;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_char sts;

#ifdef AHADEBUG
	printf("%s: aha_intr ", device_xname(sc->sc_dev));
#endif /*AHADEBUG */

	/*
	 * First acknowledge the interrupt, Then if it's not telling about
	 * a completed operation just return.
	 */
	sts = bus_space_read_1(iot, ioh, AHA_INTR_PORT);
	if ((sts & AHA_INTR_ANYINTR) == 0)
		return (0);
	bus_space_write_1(iot, ioh, AHA_CTRL_PORT, AHA_CTRL_IRST);

#ifdef AHADIAG
	/* Make sure we clear CCB_SENDING before finishing a CCB. */
	aha_collect_mbo(sc);
#endif

	/* Mail box out empty? */
	if (sts & AHA_INTR_MBOA) {
		struct aha_toggle toggle;

		toggle.cmd.opcode = AHA_MBO_INTR_EN;
		toggle.cmd.enable = 0;
		aha_cmd(iot, ioh, sc,
		    sizeof(toggle.cmd), (u_char *)&toggle.cmd,
		    0, (u_char *)0);
		aha_start_ccbs(sc);
	}

	/* Mail box in full? */
	if (sts & AHA_INTR_MBIF)
		aha_finish_ccbs(sc);

	return (1);
}

static inline void
aha_reset_ccb(struct aha_softc *sc, struct aha_ccb *ccb)
{

	ccb->flags = 0;
}

/*
 * A ccb is put onto the free list.
 */
static void
aha_free_ccb(struct aha_softc *sc, struct aha_ccb *ccb)
{
	int s;

	s = splbio();
	aha_reset_ccb(sc, ccb);
	TAILQ_INSERT_HEAD(&sc->sc_free_ccb, ccb, chain);
	splx(s);
}

static int
aha_init_ccb(struct aha_softc *sc, struct aha_ccb *ccb)
{
	bus_dma_tag_t dmat = sc->sc_dmat;
	int hashnum, error;

	/*
	 * Create the DMA map for this CCB.
	 */
	error = bus_dmamap_create(dmat, AHA_MAXXFER, AHA_NSEG, AHA_MAXXFER,
	    0, BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW, &ccb->dmamap_xfer);
	if (error) {
		aprint_error_dev(sc->sc_dev,
		    "unable to create ccb DMA map, error = %d\n",
		    error);
		return (error);
	}

	/*
	 * put in the phystokv hash table
	 * Never gets taken out.
	 */
	ccb->hashkey = sc->sc_dmamap_control->dm_segs[0].ds_addr +
	    AHA_CCB_OFF(ccb);
	hashnum = CCB_HASH(ccb->hashkey);
	ccb->nexthash = sc->sc_ccbhash[hashnum];
	sc->sc_ccbhash[hashnum] = ccb;
	aha_reset_ccb(sc, ccb);
	return (0);
}

/*
 * Create a set of ccbs and add them to the free list.  Called once
 * by aha_init().  We return the number of CCBs successfully created.
 */
static int
aha_create_ccbs(struct aha_softc *sc, struct aha_ccb *ccbstore, int count)
{
	struct aha_ccb *ccb;
	int i, error;

	memset(ccbstore, 0, sizeof(struct aha_ccb) * count);
	for (i = 0; i < count; i++) {
		ccb = &ccbstore[i];
		if ((error = aha_init_ccb(sc, ccb)) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "unable to initialize ccb, error = %d\n",
			    error);
			goto out;
		}
		TAILQ_INSERT_TAIL(&sc->sc_free_ccb, ccb, chain);
	}
 out:
	return (i);
}

/*
 * Get a free ccb
 *
 * If there are none, see if we can allocate a new one.  If so, put it in
 * the hash table too otherwise either return an error or sleep.
 */
struct aha_ccb *
aha_get_ccb(struct aha_softc *sc)
{
	struct aha_ccb *ccb;
	int s;

	s = splbio();
	ccb = TAILQ_FIRST(&sc->sc_free_ccb);
	if (ccb != NULL) {
		TAILQ_REMOVE(&sc->sc_free_ccb, ccb, chain);
		ccb->flags |= CCB_ALLOC;
	}
	splx(s);
	return (ccb);
}

/*
 * Given a physical address, find the ccb that it corresponds to.
 */
static struct aha_ccb *
aha_ccb_phys_kv(struct aha_softc *sc, u_long ccb_phys)
{
	int hashnum = CCB_HASH(ccb_phys);
	struct aha_ccb *ccb = sc->sc_ccbhash[hashnum];

	while (ccb) {
		if (ccb->hashkey == ccb_phys)
			break;
		ccb = ccb->nexthash;
	}
	return (ccb);
}

/*
 * Queue a CCB to be sent to the controller, and send it if possible.
 */
static void
aha_queue_ccb(struct aha_softc *sc, struct aha_ccb *ccb)
{

	TAILQ_INSERT_TAIL(&sc->sc_waiting_ccb, ccb, chain);
	aha_start_ccbs(sc);
}

/*
 * Garbage collect mailboxes that are no longer in use.
 */
static void
aha_collect_mbo(struct aha_softc *sc)
{
	struct aha_mbx_out *wmbo;	/* Mail Box Out pointer */
#ifdef AHADIAG
	struct aha_ccb *ccb;
#endif

	wmbo = wmbx->cmbo;

	while (sc->sc_mbofull > 0) {
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap_control,
		    AHA_MBO_OFF(wmbo), sizeof(struct aha_mbx_out),
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
		if (wmbo->cmd != AHA_MBO_FREE)
			break;

#ifdef AHADIAG
		ccb = aha_ccb_phys_kv(sc, phystol(wmbo->ccb_addr));
		ccb->flags &= ~CCB_SENDING;
#endif

		--sc->sc_mbofull;
		aha_nextmbx(wmbo, wmbx, mbo);
	}

	wmbx->cmbo = wmbo;
}

/*
 * Send as many CCBs as we have empty mailboxes for.
 */
static void
aha_start_ccbs(struct aha_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct aha_mbx_out *wmbo;	/* Mail Box Out pointer */
	struct aha_ccb *ccb;

	wmbo = wmbx->tmbo;

	while ((ccb = sc->sc_waiting_ccb.tqh_first) != NULL) {
		if (sc->sc_mbofull >= AHA_MBX_SIZE) {
			aha_collect_mbo(sc);
			if (sc->sc_mbofull >= AHA_MBX_SIZE) {
				struct aha_toggle toggle;

				toggle.cmd.opcode = AHA_MBO_INTR_EN;
				toggle.cmd.enable = 1;
				aha_cmd(iot, ioh, sc,
				    sizeof(toggle.cmd), (u_char *)&toggle.cmd,
				    0, (u_char *)0);
				break;
			}
		}

		TAILQ_REMOVE(&sc->sc_waiting_ccb, ccb, chain);
#ifdef AHADIAG
		ccb->flags |= CCB_SENDING;
#endif

		/* Link ccb to mbo. */
		ltophys(sc->sc_dmamap_control->dm_segs[0].ds_addr +
		    AHA_CCB_OFF(ccb), wmbo->ccb_addr);
		if (ccb->flags & CCB_ABORT)
			wmbo->cmd = AHA_MBO_ABORT;
		else
			wmbo->cmd = AHA_MBO_START;

		 bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap_control,
		     AHA_MBO_OFF(wmbo), sizeof(struct aha_mbx_out),
		     BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/* Tell the card to poll immediately. */
		bus_space_write_1(iot, ioh, AHA_CMD_PORT, AHA_START_SCSI);

		if ((ccb->xs->xs_control & XS_CTL_POLL) == 0)
			callout_reset(&ccb->xs->xs_callout,
			    mstohz(ccb->timeout), aha_timeout, ccb);

		++sc->sc_mbofull;
		aha_nextmbx(wmbo, wmbx, mbo);
	}

	wmbx->tmbo = wmbo;
}

/*
 * We have a ccb which has been processed by the
 * adaptor, now we look to see how the operation
 * went. Wake up the owner if waiting
 */
static void
aha_done(struct aha_softc *sc, struct aha_ccb *ccb)
{
	bus_dma_tag_t dmat = sc->sc_dmat;
	struct scsi_sense_data *s1, *s2;
	struct scsipi_xfer *xs = ccb->xs;

	SC_DEBUG(xs->xs_periph, SCSIPI_DB2, ("aha_done\n"));

	/*
	 * If we were a data transfer, unload the map that described
	 * the data buffer.
	 */
	if (xs->datalen) {
		bus_dmamap_sync(dmat, ccb->dmamap_xfer, 0,
		    ccb->dmamap_xfer->dm_mapsize,
		    (xs->xs_control & XS_CTL_DATA_IN) ? BUS_DMASYNC_POSTREAD :
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(dmat, ccb->dmamap_xfer);
	}

	/*
	 * Otherwise, put the results of the operation
	 * into the xfer and call whoever started it
	 */
#ifdef AHADIAG
	if (ccb->flags & CCB_SENDING) {
		printf("%s: exiting ccb still in transit!\n",
		    device_xname(sc->sc_dev));
		Debugger();
		return;
	}
#endif
	if ((ccb->flags & CCB_ALLOC) == 0) {
		printf("%s: exiting ccb not allocated!\n",
		    device_xname(sc->sc_dev));
		Debugger();
		return;
	}
	if (xs->error == XS_NOERROR) {
		if (ccb->host_stat != AHA_OK) {
			switch (ccb->host_stat) {
			case AHA_SEL_TIMEOUT:	/* No response */
				xs->error = XS_SELTIMEOUT;
				break;
			default:	/* Other scsi protocol messes */
				printf("%s: host_stat %x\n",
				    device_xname(sc->sc_dev), ccb->host_stat);
				xs->error = XS_DRIVER_STUFFUP;
				break;
			}
		} else if (ccb->target_stat != SCSI_OK) {
			switch (ccb->target_stat) {
			case SCSI_CHECK:
				s1 = (struct scsi_sense_data *) (((char *) (&ccb->scsi_cmd)) +
				    ccb->scsi_cmd_length);
				s2 = &xs->sense.scsi_sense;
				*s2 = *s1;
				xs->error = XS_SENSE;
				break;
			case SCSI_BUSY:
				xs->error = XS_BUSY;
				break;
			default:
				printf("%s: target_stat %x\n",
				    device_xname(sc->sc_dev), ccb->target_stat);
				xs->error = XS_DRIVER_STUFFUP;
				break;
			}
		} else
			xs->resid = 0;
	}
	aha_free_ccb(sc, ccb);
	scsipi_done(xs);
}

/*
 * Find the board and find its irq/drq
 */
int
aha_find(bus_space_tag_t iot, bus_space_handle_t ioh, struct aha_probe_data *sc)
{
	int i;
	u_char sts;
	struct aha_config config;
	int irq, drq;

	/*
	 * assume invalid status means the board is not present.
	 */

	sts = bus_space_read_1(iot, ioh, AHA_STAT_PORT);
	if (sts == 0)
		return (0);
	if ((sts & (AHA_STAT_STST|AHA_STAT_RSVD|AHA_STAT_CDF)) != 0)
		return (0);
	sts = bus_space_read_1(iot, ioh, AHA_INTR_PORT);
	if ((sts & AHA_INTR_RSVD) != 0)
		return (0);

	/*
	 * reset board, If it doesn't respond, assume
	 * that it's not there.. good for the probe
	 */

	bus_space_write_1(iot, ioh, AHA_CTRL_PORT, AHA_CTRL_HRST | AHA_CTRL_SRST);

	delay(100);
	for (i = AHA_RESET_TIMEOUT; i; i--) {
		sts = bus_space_read_1(iot, ioh, AHA_STAT_PORT);
		if (sts == (AHA_STAT_IDLE | AHA_STAT_INIT))
			break;
		delay(1000);	/* calibrated in msec */
	}
	if (!i) {
#ifdef AHADEBUG
		if (aha_debug)
			printf("aha_find: No answer from adaptec board\n");
#endif /* AHADEBUG */
		return (0);
	}

	/*
	 * setup DMA channel from jumpers and save int
	 * level
	 */
	delay(1000);		/* for Bustek 545 */
	config.cmd.opcode = AHA_INQUIRE_CONFIG;
	aha_cmd(iot, ioh, (struct aha_softc *)0,
	    sizeof(config.cmd), (u_char *)&config.cmd,
	    sizeof(config.reply), (u_char *)&config.reply);
	switch (config.reply.chan) {
	case EISADMA:
		drq = -1;
		break;
	case CHAN0:
		drq = 0;
		break;
	case CHAN5:
		drq = 5;
		break;
	case CHAN6:
		drq = 6;
		break;
	case CHAN7:
		drq = 7;
		break;
	default:
		printf("aha_find: illegal drq setting %x\n", config.reply.chan);
		return (0);
	}

	switch (config.reply.intr) {
	case INT9:
		irq = 9;
		break;
	case INT10:
		irq = 10;
		break;
	case INT11:
		irq = 11;
		break;
	case INT12:
		irq = 12;
		break;
	case INT14:
		irq = 14;
		break;
	case INT15:
		irq = 15;
		break;
	default:
		printf("aha_find: illegal irq setting %x\n", config.reply.intr);
		return (0);
	}

	if (sc) {
		sc->sc_irq = irq;
		sc->sc_drq = drq;
		sc->sc_scsi_dev = config.reply.scsi_dev;
	}

	return (1);
}

/*
 * Start the board, ready for normal operation
 */
static int
aha_init(struct aha_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	bus_dma_segment_t seg;
	struct aha_devices devices;
	struct aha_setup setup;
	struct aha_mailbox mailbox;
	int error, i, j, initial_ccbs, rseg;

	/*
	 * XXX
	 * If we are a 1542C or later, disable the extended BIOS so that the
	 * mailbox interface is unlocked.
	 * No need to check the extended BIOS flags as some of the
	 * extensions that cause us problems are not flagged in that byte.
	 */
	if (!strncmp(sc->sc_model, "1542C", 5)) {
		struct aha_extbios extbios;
		struct aha_unlock unlock;

		printf("%s: unlocking mailbox interface\n",
		    device_xname(sc->sc_dev));
		extbios.cmd.opcode = AHA_EXT_BIOS;
		aha_cmd(iot, ioh, sc,
		    sizeof(extbios.cmd), (u_char *)&extbios.cmd,
		    sizeof(extbios.reply), (u_char *)&extbios.reply);

#ifdef AHADEBUG
		printf("%s: flags=%02x, mailboxlock=%02x\n",
		    device_xname(sc->sc_dev),
		    extbios.reply.flags, extbios.reply.mailboxlock);
#endif /* AHADEBUG */

		unlock.cmd.opcode = AHA_MBX_ENABLE;
		unlock.cmd.junk = 0;
		unlock.cmd.magic = extbios.reply.mailboxlock;
		aha_cmd(iot, ioh, sc,
		    sizeof(unlock.cmd), (u_char *)&unlock.cmd,
		    0, (u_char *)0);
	}

#if 0
	/*
	 * Change the bus on/off times to not clash with other DMA users.
	 */
	aha_cmd(iot, ioh, 1, 0, 0, 0, AHA_BUS_ON_TIME_SET, 7);
	aha_cmd(iot, ioh, 1, 0, 0, 0, AHA_BUS_OFF_TIME_SET, 4);
#endif

	/* Inquire Installed Devices (to force synchronous negotiation). */
	devices.cmd.opcode = AHA_INQUIRE_DEVICES;
	aha_cmd(iot, ioh, sc,
	    sizeof(devices.cmd), (u_char *)&devices.cmd,
	    sizeof(devices.reply), (u_char *)&devices.reply);

	/* Count installed units */
	initial_ccbs = 0;
	for (i = 0; i < 8; i++) {
		for (j = 0; j < 8; j++) {
			if (((devices.reply.lun_map[i] >> j) & 1) == 1)
				initial_ccbs += 1;
		}
	}
	initial_ccbs *= 2;
	if (initial_ccbs > AHA_CCB_MAX)
		initial_ccbs = AHA_CCB_MAX;
	if (initial_ccbs == 0)	/* yes, this can happen */
		initial_ccbs = 2;

	/* Obtain setup information from. */
	setup.cmd.opcode = AHA_INQUIRE_SETUP;
	setup.cmd.len = sizeof(setup.reply);
	aha_cmd(iot, ioh, sc,
	    sizeof(setup.cmd), (u_char *)&setup.cmd,
	    sizeof(setup.reply), (u_char *)&setup.reply);

	printf("%s: %s, %s\n",
	    device_xname(sc->sc_dev),
	    setup.reply.sync_neg ? "sync" : "async",
	    setup.reply.parity ? "parity" : "no parity");

	for (i = 0; i < 8; i++) {
		if (!setup.reply.sync[i].valid ||
		    (!setup.reply.sync[i].offset && !setup.reply.sync[i].period))
			continue;
		printf("%s targ %d: sync, offset %d, period %dnsec\n",
		    device_xname(sc->sc_dev), i,
		    setup.reply.sync[i].offset, setup.reply.sync[i].period * 50 + 200);
	}

	/*
	 * Allocate the mailbox and control blocks.
	 */
	if ((error = bus_dmamem_alloc(sc->sc_dmat, sizeof(struct aha_control),
	    PAGE_SIZE, 0, &seg, 1, &rseg, BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to allocate control structures, "
		    "error = %d\n", error);
		return (error);
	}
	if ((error = bus_dmamem_map(sc->sc_dmat, &seg, rseg,
	    sizeof(struct aha_control), (void **)&sc->sc_control,
	    BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to map control structures, error = %d\n", error);
		return (error);
	}

	/*
	 * Create and load the DMA map used for the mailbox and
	 * control blocks.
	 */
	if ((error = bus_dmamap_create(sc->sc_dmat, sizeof(struct aha_control),
	    1, sizeof(struct aha_control), 0, BUS_DMA_NOWAIT,
	    &sc->sc_dmamap_control)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to create control DMA map, error = %d\n",
		    error);
		return (error);
	}
	if ((error = bus_dmamap_load(sc->sc_dmat, sc->sc_dmamap_control,
	    sc->sc_control, sizeof(struct aha_control), NULL,
	    BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to load control DMA map, error = %d\n",
		    error);
		return (error);
	}

	/*
	 * Initialize the control blocks.
	 */
	i = aha_create_ccbs(sc, sc->sc_control->ac_ccbs, initial_ccbs);
	if (i == 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to create control blocks\n");
		return (ENOMEM);
	} else if (i != initial_ccbs) {
		printf("%s: WARNING: only %d of %d control blocks created\n",
		    device_xname(sc->sc_dev), i, initial_ccbs);
	}

	sc->sc_adapter.adapt_openings = i;
	sc->sc_adapter.adapt_max_periph = sc->sc_adapter.adapt_openings;

	/*
	 * Set up initial mail box for round-robin operation.
	 */
	for (i = 0; i < AHA_MBX_SIZE; i++) {
		wmbx->mbo[i].cmd = AHA_MBO_FREE;
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap_control,
		    AHA_MBO_OFF(&wmbx->mbo[i]), sizeof(struct aha_mbx_out),
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
		wmbx->mbi[i].stat = AHA_MBI_FREE;
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap_control,
		    AHA_MBI_OFF(&wmbx->mbi[i]), sizeof(struct aha_mbx_in),
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	}
	wmbx->cmbo = wmbx->tmbo = &wmbx->mbo[0];
	wmbx->tmbi = &wmbx->mbi[0];
	sc->sc_mbofull = 0;

	/* Initialize mail box. */
	mailbox.cmd.opcode = AHA_MBX_INIT;
	mailbox.cmd.nmbx = AHA_MBX_SIZE;
	ltophys(sc->sc_dmamap_control->dm_segs[0].ds_addr +
	    offsetof(struct aha_control, ac_mbx), mailbox.cmd.addr);
	aha_cmd(iot, ioh, sc,
	    sizeof(mailbox.cmd), (u_char *)&mailbox.cmd,
	    0, (u_char *)0);
	return (0);
}

static void
aha_inquire_setup_information(struct aha_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct aha_revision revision;
	u_char sts;
	int i;
	char *p;

	strcpy(sc->sc_model, "unknown");

	/*
	 * Assume we have a board at this stage, do an adapter inquire
	 * to find out what type of controller it is.  If the command
	 * fails, we assume it's either a crusty board or an old 1542
	 * clone, and skip the board-specific stuff.
	 */
	revision.cmd.opcode = AHA_INQUIRE_REVISION;
	if (aha_cmd(iot, ioh, sc,
	    sizeof(revision.cmd), (u_char *)&revision.cmd,
	    sizeof(revision.reply), (u_char *)&revision.reply)) {
		/*
		 * aha_cmd() already started the reset.  It's not clear we
		 * even need to bother here.
		 */
		for (i = AHA_RESET_TIMEOUT; i; i--) {
			sts = bus_space_read_1(iot, ioh, AHA_STAT_PORT);
			if (sts == (AHA_STAT_IDLE | AHA_STAT_INIT))
				break;
			delay(1000);
		}
		if (!i) {
#ifdef AHADEBUG
			printf("aha_init: soft reset failed\n");
#endif /* AHADEBUG */
			return;
		}
#ifdef AHADEBUG
		printf("aha_init: inquire command failed\n");
#endif /* AHADEBUG */
		goto noinquire;
	}

#ifdef AHADEBUG
	printf("%s: inquire %x, %x, %x, %x\n",
	    device_xname(sc->sc_dev),
	    revision.reply.boardid, revision.reply.spec_opts,
	    revision.reply.revision_1, revision.reply.revision_2);
#endif /* AHADEBUG */

	switch (revision.reply.boardid) {
	case BOARD_1540_16HEAD_BIOS:
	case BOARD_1540_64HEAD_BIOS:
	case BOARD_1540:
		strcpy(sc->sc_model, "1540");
		break;
	case BOARD_1542:
		strcpy(sc->sc_model, "1540A/1542A/1542B");
		break;
	case BOARD_1640:
		strcpy(sc->sc_model, "1640");
		break;
	case BOARD_1740:
		strcpy(sc->sc_model, "1740");
		break;
	case BOARD_1542C:
		strcpy(sc->sc_model, "1542C");
		break;
	case BOARD_1542CF:
		strcpy(sc->sc_model, "1542CF");
		break;
	case BOARD_1542CP:
		strcpy(sc->sc_model, "1542CP");
		break;
	}

	p = sc->sc_firmware;
	*p++ = revision.reply.revision_1;
	*p++ = '.';
	*p++ = revision.reply.revision_2;
	*p = '\0';

noinquire:
	printf("%s: model AHA-%s, firmware %s\n",
	       device_xname(sc->sc_dev),
	       sc->sc_model, sc->sc_firmware);
}

static void
ahaminphys(struct buf *bp)
{

	if (bp->b_bcount > AHA_MAXXFER)
		bp->b_bcount = AHA_MAXXFER;
	minphys(bp);
}

/*
 * start a scsi operation given the command and the data address. Also needs
 * the unit, target and lu.
 */

static void
aha_scsipi_request(struct scsipi_channel *chan, scsipi_adapter_req_t req,
    void *arg)
{
	struct scsipi_xfer *xs;
	struct scsipi_periph *periph;
	struct aha_softc *sc = device_private(chan->chan_adapter->adapt_dev);
	bus_dma_tag_t dmat = sc->sc_dmat;
	struct aha_ccb *ccb;
	int error, seg, flags, s;


	switch (req) {
	case ADAPTER_REQ_RUN_XFER:
		xs = arg;
		periph = xs->xs_periph;
		flags = xs->xs_control;

		SC_DEBUG(periph, SCSIPI_DB2, ("aha_scsipi_request\n"));

		/* Get a CCB to use. */
		ccb = aha_get_ccb(sc);
#ifdef DIAGNOSTIC
		/*
		 * This should never happen as we track the resources
		 * in the mid-layer.
		 */
		if (ccb == NULL) {
			scsipi_printaddr(periph);
			printf("unable to allocate ccb\n");
			panic("aha_scsipi_request");
		}
#endif

		ccb->xs = xs;
		ccb->timeout = xs->timeout;

		/*
		 * Put all the arguments for the xfer in the ccb
		 */
		if (flags & XS_CTL_RESET) {
			ccb->opcode = AHA_RESET_CCB;
			ccb->scsi_cmd_length = 0;
		} else {
			/* can't use S/G if zero length */
			if (xs->cmdlen > sizeof(ccb->scsi_cmd)) {
				printf("%s: cmdlen %d too large for CCB\n",
				    device_xname(sc->sc_dev), xs->cmdlen);
				xs->error = XS_DRIVER_STUFFUP;
				goto out_bad;
			}
			ccb->opcode = (xs->datalen ? AHA_INIT_SCAT_GATH_CCB
						   : AHA_INITIATOR_CCB);
			memcpy(&ccb->scsi_cmd, xs->cmd,
			    ccb->scsi_cmd_length = xs->cmdlen);
		}

		if (xs->datalen) {
			/*
			 * Map the DMA transfer.
			 */
#ifdef TFS
			if (flags & XS_CTL_DATA_UIO) {
				error = bus_dmamap_load_uio(dmat,
				    ccb->dmamap_xfer, (struct uio *)xs->data,
				    ((flags & XS_CTL_NOSLEEP) ? BUS_DMA_NOWAIT :
				     BUS_DMA_WAITOK) | BUS_DMA_STREAMING |
				     ((flags & XS_CTL_DATA_IN) ? BUS_DMA_READ :
				      BUS_DMA_WRITE));
			} else
#endif
			{
				error = bus_dmamap_load(dmat,
				    ccb->dmamap_xfer, xs->data, xs->datalen,
				    NULL,
				    ((flags & XS_CTL_NOSLEEP) ? BUS_DMA_NOWAIT :
				     BUS_DMA_WAITOK) | BUS_DMA_STREAMING |
				     ((flags & XS_CTL_DATA_IN) ? BUS_DMA_READ :
				      BUS_DMA_WRITE));
			}

			switch (error) {
			case 0:
				break;

			case ENOMEM:
			case EAGAIN:
				xs->error = XS_RESOURCE_SHORTAGE;
				goto out_bad;

			default:
				xs->error = XS_DRIVER_STUFFUP;
				if (error == EFBIG) {
					printf("%s: aha_scsi_cmd, more than %d"
					    " DMA segments\n",
					    device_xname(sc->sc_dev), AHA_NSEG);
				} else {
					aprint_error_dev(sc->sc_dev,
					    "error %d loading DMA map\n",
					    error);
				}
out_bad:
				aha_free_ccb(sc, ccb);
				scsipi_done(xs);
				return;
			}

			bus_dmamap_sync(dmat, ccb->dmamap_xfer, 0,
			    ccb->dmamap_xfer->dm_mapsize,
			    (flags & XS_CTL_DATA_IN) ? BUS_DMASYNC_PREREAD :
			    BUS_DMASYNC_PREWRITE);

			/*
			 * Load the hardware scatter/gather map with the
			 * contents of the DMA map.
			 */
			for (seg = 0; seg < ccb->dmamap_xfer->dm_nsegs; seg++) {
				ltophys(ccb->dmamap_xfer->dm_segs[seg].ds_addr,
				    ccb->scat_gath[seg].seg_addr);
				ltophys(ccb->dmamap_xfer->dm_segs[seg].ds_len,
				    ccb->scat_gath[seg].seg_len);
			}

			ltophys(sc->sc_dmamap_control->dm_segs[0].ds_addr +
			    AHA_CCB_OFF(ccb) +
			    offsetof(struct aha_ccb, scat_gath),
			    ccb->data_addr);
			ltophys(ccb->dmamap_xfer->dm_nsegs *
			    sizeof(struct aha_scat_gath), ccb->data_length);
		} else {
			/*
			 * No data xfer, use non S/G values.
			 */
			ltophys(0, ccb->data_addr);
			ltophys(0, ccb->data_length);
		}

		ccb->data_out = 0;
		ccb->data_in = 0;
		ccb->target = periph->periph_target;
		ccb->lun = periph->periph_lun;
		ccb->req_sense_length = sizeof(ccb->scsi_sense);
		ccb->host_stat = 0x00;
		ccb->target_stat = 0x00;
		ccb->link_id = 0;
		ltophys(0, ccb->link_addr);

		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap_control,
		    AHA_CCB_OFF(ccb), sizeof(struct aha_ccb),
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		s = splbio();
		aha_queue_ccb(sc, ccb);
		splx(s);

		SC_DEBUG(periph, SCSIPI_DB3, ("cmd_sent\n"));
		if ((flags & XS_CTL_POLL) == 0)
			return;

		/* Not allowed to use interrupts, poll for completion. */
		if (aha_poll(sc, xs, ccb->timeout)) {
			aha_timeout(ccb);
			if (aha_poll(sc, xs, ccb->timeout))
				aha_timeout(ccb);
		}
		return;

	case ADAPTER_REQ_GROW_RESOURCES:
		/* XXX Not supported. */
		return;

	case ADAPTER_REQ_SET_XFER_MODE:
		/*
		 * Can't really do this on the Adaptec; it has
		 * its own config mechanism, but we do know how
		 * to query what the firmware negotiated.
		 */
		/* XXX XXX XXX */
		return;
	}
}

/*
 * Poll a particular unit, looking for a particular xs
 */
static int
aha_poll(struct aha_softc *sc, struct scsipi_xfer *xs, int count)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	/* timeouts are in msec, so we loop in 1000 usec cycles */
	while (count) {
		/*
		 * If we had interrupts enabled, would we
		 * have got an interrupt?
		 */
		if (bus_space_read_1(iot, ioh, AHA_INTR_PORT) & AHA_INTR_ANYINTR)
			aha_intr(sc);
		if (xs->xs_status & XS_STS_DONE)
			return (0);
		delay(1000);	/* only happens in boot so ok */
		count--;
	}
	return (1);
}

static void
aha_timeout(void *arg)
{
	struct aha_ccb *ccb = arg;
	struct scsipi_xfer *xs = ccb->xs;
	struct scsipi_periph *periph = xs->xs_periph;
	struct aha_softc *sc =
	    device_private(periph->periph_channel->chan_adapter->adapt_dev);
	int s;

	scsipi_printaddr(periph);
	printf("timed out");

	s = splbio();

#ifdef AHADIAG
	/*
	 * If The ccb's mbx is not free, then the board has gone south?
	 */
	aha_collect_mbo(sc);
	if (ccb->flags & CCB_SENDING) {
		aprint_error_dev(sc->sc_dev, "not taking commands!\n");
		Debugger();
	}
#endif

	/*
	 * If it has been through before, then
	 * a previous abort has failed, don't
	 * try abort again
	 */
	if (ccb->flags & CCB_ABORT) {
		/* abort timed out */
		printf(" AGAIN\n");
		/* XXX Must reset! */
	} else {
		/* abort the operation that has timed out */
		printf("\n");
		ccb->xs->error = XS_TIMEOUT;
		ccb->timeout = AHA_ABORT_TIMEOUT;
		ccb->flags |= CCB_ABORT;
		aha_queue_ccb(sc, ccb);
	}

	splx(s);
}
