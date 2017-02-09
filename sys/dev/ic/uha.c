/*	$NetBSD: uha.c,v 1.46 2012/10/27 17:18:23 chs Exp $	*/

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
 * Ported for use with the UltraStor 14f by Gary Close (gclose@wvnvms.wvnet.edu)
 * Slight fixes to timeouts to run with the 34F
 * Thanks to Julian Elischer for advice and help with this port.
 *
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
 * commenced: Sun Sep 27 18:14:01 PDT 1992
 * slight mod to make work with 34F as well: Wed Jun  2 18:05:48 WST 1993
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uha.c,v 1.46 2012/10/27 17:18:23 chs Exp $");

#undef UHADEBUG
#ifdef DDB
#define	integrate
#else
#define	integrate	static inline
#endif

#include <sys/param.h>
#include <sys/systm.h>
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

#include <dev/ic/uhareg.h>
#include <dev/ic/uhavar.h>

#ifndef	DDB
#define Debugger() panic("should call debugger here (uha.c)")
#endif /* ! DDB */

#define	UHA_MAXXFER	((UHA_NSEG - 1) << PGSHIFT)

integrate void uha_reset_mscp(struct uha_softc *, struct uha_mscp *);
void uha_free_mscp(struct uha_softc *, struct uha_mscp *);
integrate int uha_init_mscp(struct uha_softc *, struct uha_mscp *);
struct uha_mscp *uha_get_mscp(struct uha_softc *);
void uhaminphys(struct buf *);
void uha_scsipi_request(struct scsipi_channel *, scsipi_adapter_req_t, void *);
int uha_create_mscps(struct uha_softc *, struct uha_mscp *, int);

#define	UHA_ABORT_TIMEOUT	2000	/* time to wait for abort (mSec) */

/*
 * Attach all the sub-devices we can find
 */
void
uha_attach(struct uha_softc *sc, struct uha_probe_data *upd)
{
	struct scsipi_adapter *adapt = &sc->sc_adapter;
	struct scsipi_channel *chan = &sc->sc_channel;
	bus_dma_segment_t seg;
	int i, error, rseg;

	TAILQ_INIT(&sc->sc_free_mscp);

	(sc->init)(sc);

	/*
	 * Fill in the scsipi_adapter.
	 */
	memset(adapt, 0, sizeof(*adapt));
	adapt->adapt_dev = sc->sc_dev;
	adapt->adapt_nchannels = 1;
	/* adapt_openings initialized below */
	/* adapt_max_periph initialized below */
	adapt->adapt_request = uha_scsipi_request;
	adapt->adapt_minphys = uhaminphys;

	/*
	 * Fill in the scsipi_channel.
	 */
	memset(chan, 0, sizeof(*chan));
	chan->chan_adapter = adapt;
	chan->chan_bustype = &scsi_bustype;
	chan->chan_channel = 0;
	chan->chan_ntargets = 8;
	chan->chan_nluns = 8;
	chan->chan_id = upd->sc_scsi_dev;

#define	MSCPSIZE	(UHA_MSCP_MAX * sizeof(struct uha_mscp))

	/*
	 * Allocate the MSCPs.
	 */
	if ((error = bus_dmamem_alloc(sc->sc_dmat, MSCPSIZE,
	    PAGE_SIZE, 0, &seg, 1, &rseg, BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to allocate mscps, error = %d\n",
		    error);
		return;
	}
	if ((error = bus_dmamem_map(sc->sc_dmat, &seg, rseg,
	    MSCPSIZE, (void **)&sc->sc_mscps,
	    BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to map mscps, error = %d\n",
		    error);
		return;
	}

	/*
	 * Create and load the DMA map used for the mscps.
	 */
	if ((error = bus_dmamap_create(sc->sc_dmat, MSCPSIZE,
	    1, MSCPSIZE, 0, BUS_DMA_NOWAIT | sc->sc_dmaflags,
	    &sc->sc_dmamap_mscp)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to create mscp DMA map, error = %d\n",
		    error);
		return;
	}
	if ((error = bus_dmamap_load(sc->sc_dmat, sc->sc_dmamap_mscp,
	    sc->sc_mscps, MSCPSIZE, NULL, BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to load mscp DMA map, error = %d\n",
		    error);
		return;
	}

#undef MSCPSIZE

	/*
	 * Initialize the mscps.
	 */
	i = uha_create_mscps(sc, sc->sc_mscps, UHA_MSCP_MAX);
	if (i == 0) {
		aprint_error_dev(sc->sc_dev, "unable to create mscps\n");
		return;
	} else if (i != UHA_MSCP_MAX) {
		aprint_error_dev(sc->sc_dev, "WARNING: only %d of %d mscps created\n",
		    i, UHA_MSCP_MAX);
	}

	adapt->adapt_openings = i;
	adapt->adapt_max_periph = adapt->adapt_openings;

	/*
	 * ask the adapter what subunits are present
	 */
	config_found(sc->sc_dev, &sc->sc_channel, scsiprint);
}

integrate void
uha_reset_mscp(struct uha_softc *sc, struct uha_mscp *mscp)
{

	mscp->flags = 0;
}

/*
 * A mscp (and hence a mbx-out) is put onto the free list.
 */
void
uha_free_mscp(struct uha_softc *sc, struct uha_mscp *mscp)
{
	int s;

	s = splbio();
	uha_reset_mscp(sc, mscp);
	TAILQ_INSERT_HEAD(&sc->sc_free_mscp, mscp, chain);
	splx(s);
}

integrate int
uha_init_mscp(struct uha_softc *sc, struct uha_mscp *mscp)
{
	bus_dma_tag_t dmat = sc->sc_dmat;
	int hashnum, error;

	/*
	 * Create the DMA map for this MSCP.
	 */
	error = bus_dmamap_create(dmat, UHA_MAXXFER, UHA_NSEG, UHA_MAXXFER,
	    0, BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW | sc->sc_dmaflags,
	    &mscp->dmamap_xfer);
	if (error) {
		aprint_error_dev(sc->sc_dev, "can't create mscp DMA map, error = %d\n",
		    error);
		return (error);
	}

	/*
	 * put in the phystokv hash table
	 * Never gets taken out.
	 */
	mscp->hashkey = sc->sc_dmamap_mscp->dm_segs[0].ds_addr +
	    UHA_MSCP_OFF(mscp);
	hashnum = MSCP_HASH(mscp->hashkey);
	mscp->nexthash = sc->sc_mscphash[hashnum];
	sc->sc_mscphash[hashnum] = mscp;
	uha_reset_mscp(sc, mscp);
	return (0);
}

/*
 * Create a set of MSCPs and add them to the free list.
 */
int
uha_create_mscps(struct uha_softc *sc, struct uha_mscp *mscpstore, int count)
{
	struct uha_mscp *mscp;
	int i, error;

	memset(mscpstore, 0, sizeof(struct uha_mscp) * count);
	for (i = 0; i < count; i++) {
		mscp = &mscpstore[i];
		if ((error = uha_init_mscp(sc, mscp)) != 0) {
			aprint_error_dev(sc->sc_dev, "unable to initialize mscp, error = %d\n",
			    error);
			goto out;
		}
		TAILQ_INSERT_TAIL(&sc->sc_free_mscp, mscp, chain);
	}
 out:
	return (i);
}

/*
 * Get a free mscp
 *
 * If there are none, see if we can allocate a new one.  If so, put it in the
 * hash table too otherwise either return an error or sleep.
 */
struct uha_mscp *
uha_get_mscp(struct uha_softc *sc)
{
	struct uha_mscp *mscp;
	int s;

	s = splbio();
	mscp = TAILQ_FIRST(&sc->sc_free_mscp);
	if (mscp != NULL) {
		TAILQ_REMOVE(&sc->sc_free_mscp, mscp, chain);
		mscp->flags |= MSCP_ALLOC;
	}
	splx(s);
	return (mscp);
}

/*
 * given a physical address, find the mscp that it corresponds to.
 */
struct uha_mscp *
uha_mscp_phys_kv(struct uha_softc *sc, u_long mscp_phys)
{
	int hashnum = MSCP_HASH(mscp_phys);
	struct uha_mscp *mscp = sc->sc_mscphash[hashnum];

	while (mscp) {
		if (mscp->hashkey == mscp_phys)
			break;
		mscp = mscp->nexthash;
	}
	return (mscp);
}

/*
 * We have a mscp which has been processed by the adaptor, now we look to see
 * how the operation went.
 */
void
uha_done(struct uha_softc *sc, struct uha_mscp *mscp)
{
	bus_dma_tag_t dmat = sc->sc_dmat;
	struct scsi_sense_data *s1, *s2;
	struct scsipi_xfer *xs = mscp->xs;

	SC_DEBUG(xs->xs_periph, SCSIPI_DB2, ("uha_done\n"));

	bus_dmamap_sync(dmat, sc->sc_dmamap_mscp,
	    UHA_MSCP_OFF(mscp), sizeof(struct uha_mscp),
	    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

	/*
	 * If we were a data transfer, unload the map that described
	 * the data buffer.
	 */
	if (xs->datalen) {
		bus_dmamap_sync(dmat, mscp->dmamap_xfer, 0,
		    mscp->dmamap_xfer->dm_mapsize,
		    (xs->xs_control & XS_CTL_DATA_IN) ? BUS_DMASYNC_POSTREAD :
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(dmat, mscp->dmamap_xfer);
	}

	/*
	 * Otherwise, put the results of the operation
	 * into the xfer and call whoever started it
	 */
	if ((mscp->flags & MSCP_ALLOC) == 0) {
		aprint_error_dev(sc->sc_dev, "exiting ccb not allocated!\n");
		Debugger();
		return;
	}
	if (xs->error == XS_NOERROR) {
		if (mscp->host_stat != UHA_NO_ERR) {
			switch (mscp->host_stat) {
			case UHA_SBUS_TIMEOUT:		/* No response */
				xs->error = XS_SELTIMEOUT;
				break;
			default:	/* Other scsi protocol messes */
				aprint_error_dev(sc->sc_dev, "host_stat %x\n",
				    mscp->host_stat);
				xs->error = XS_DRIVER_STUFFUP;
			}
		} else if (mscp->target_stat != SCSI_OK) {
			switch (mscp->target_stat) {
			case SCSI_CHECK:
				s1 = &mscp->mscp_sense;
				s2 = &xs->sense.scsi_sense;
				*s2 = *s1;
				xs->error = XS_SENSE;
				break;
			case SCSI_BUSY:
				xs->error = XS_BUSY;
				break;
			default:
				aprint_error_dev(sc->sc_dev, "target_stat %x\n",
				    mscp->target_stat);
				xs->error = XS_DRIVER_STUFFUP;
			}
		} else
			xs->resid = 0;
	}
	uha_free_mscp(sc, mscp);
	scsipi_done(xs);
}

void
uhaminphys(struct buf *bp)
{

	if (bp->b_bcount > UHA_MAXXFER)
		bp->b_bcount = UHA_MAXXFER;
	minphys(bp);
}

/*
 * start a scsi operation given the command and the data address.  Also
 * needs the unit, target and lu.
 */

void
uha_scsipi_request(struct scsipi_channel *chan, scsipi_adapter_req_t req, void *arg)
{
	struct scsipi_xfer *xs;
	struct scsipi_periph *periph;
	struct uha_softc *sc = device_private(chan->chan_adapter->adapt_dev);
	bus_dma_tag_t dmat = sc->sc_dmat;
	struct uha_mscp *mscp;
	int error, seg, flags, s;


	switch (req) {
	case ADAPTER_REQ_RUN_XFER:
		xs = arg;
		periph = xs->xs_periph;
		flags = xs->xs_control;

		SC_DEBUG(periph, SCSIPI_DB2, ("uha_scsipi_request\n"));

		/* Get an MSCP to use. */
		mscp = uha_get_mscp(sc);
#ifdef DIAGNOSTIC
		/*
		 * This should never happen as we track the resources
		 * in the mid-layer.
		 */
		if (mscp == NULL) {
			scsipi_printaddr(periph);
			printf("unable to allocate mscp\n");
			panic("uha_scsipi_request");
		}
#endif

		mscp->xs = xs;
		mscp->timeout = xs->timeout;

		/*
		 * Put all the arguments for the xfer in the mscp
		 */
		if (flags & XS_CTL_RESET) {
			mscp->opcode = UHA_SDR;
			mscp->ca = 0x01;
		} else {
			if (xs->cmdlen > sizeof(mscp->scsi_cmd)) {
				aprint_error_dev(sc->sc_dev, "cmdlen %d too large for MSCP\n",
				    xs->cmdlen);
				xs->error = XS_DRIVER_STUFFUP;
				goto out_bad;
			}
			mscp->opcode = UHA_TSP;
			/* XXX Not for tapes. */
			mscp->ca = 0x01;
			memcpy(&mscp->scsi_cmd, xs->cmd, mscp->scsi_cmd_length);
		}
		mscp->xdir = UHA_SDET;
		mscp->dcn = 0x00;
		mscp->chan = 0x00;
		mscp->target = periph->periph_target;
		mscp->lun = periph->periph_lun;
		mscp->scsi_cmd_length = xs->cmdlen;
		mscp->sense_ptr = sc->sc_dmamap_mscp->dm_segs[0].ds_addr +
		    UHA_MSCP_OFF(mscp) + offsetof(struct uha_mscp, mscp_sense);
		mscp->req_sense_length = sizeof(mscp->mscp_sense);
		mscp->host_stat = 0x00;
		mscp->target_stat = 0x00;

		if (xs->datalen) {
			seg = 0;
#ifdef	TFS
			if (flags & SCSI_DATA_UIO) {
				error = bus_dmamap_load_uio(dmat,
				    mscp->dmamap_xfer, (struct uio *)xs->data,
				    ((flags & XS_CTL_NOSLEEP) ? BUS_DMA_NOWAIT :
				     BUS_DMA_WAITOK) | BUS_DMA_STREAMING |
				     ((flags & XS_CTL_DATA_IN) ? BUS_DMA_READ :
				      BUS_DMA_WRITE));
			} else
#endif /*TFS */
			{
				error = bus_dmamap_load(dmat,
				    mscp->dmamap_xfer, xs->data, xs->datalen,
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
				aprint_error_dev(sc->sc_dev, "error %d loading DMA map\n",
				    error);
 out_bad:
				uha_free_mscp(sc, mscp);
				scsipi_done(xs);
				return;
			}

			bus_dmamap_sync(dmat, mscp->dmamap_xfer, 0,
			    mscp->dmamap_xfer->dm_mapsize,
			    (flags & XS_CTL_DATA_IN) ? BUS_DMASYNC_PREREAD :
			    BUS_DMASYNC_PREWRITE);

			/*
			 * Load the hardware scatter/gather map with the
			 * contents of the DMA map.
			 */
			for (seg = 0;
			     seg < mscp->dmamap_xfer->dm_nsegs; seg++) {
				mscp->uha_dma[seg].seg_addr =
				    mscp->dmamap_xfer->dm_segs[seg].ds_addr;
				mscp->uha_dma[seg].seg_len =
				    mscp->dmamap_xfer->dm_segs[seg].ds_len;
			}

			mscp->data_addr =
			    sc->sc_dmamap_mscp->dm_segs[0].ds_addr +
			    UHA_MSCP_OFF(mscp) + offsetof(struct uha_mscp,
			    uha_dma);
			mscp->data_length = xs->datalen;
			mscp->sgth = 0x01;
			mscp->sg_num = seg;
		} else {		/* No data xfer, use non S/G values */
			mscp->data_addr = (physaddr)0;
			mscp->data_length = 0;
			mscp->sgth = 0x00;
			mscp->sg_num = 0;
		}
		mscp->link_id = 0;
		mscp->link_addr = (physaddr)0;

		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap_mscp,
		    UHA_MSCP_OFF(mscp), sizeof(struct uha_mscp),
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		s = splbio();
		(sc->start_mbox)(sc, mscp);
		splx(s);

		if ((flags & XS_CTL_POLL) == 0)
			return;

		/*
		 * If we can't use interrupts, poll on completion
		 */
		if ((sc->poll)(sc, xs, mscp->timeout)) {
			uha_timeout(mscp);
			if ((sc->poll)(sc, xs, mscp->timeout))
				uha_timeout(mscp);
		}
		return;

	case ADAPTER_REQ_GROW_RESOURCES:
		/* XXX Not supported. */
		return;

	case ADAPTER_REQ_SET_XFER_MODE:
		/*
		 * We can't really do this (the UltraStor controllers
		 * have their own config).
		 *
		 * XXX How do we query the config?
		 */
		return;
	}
}
void
uha_timeout(void *arg)
{
	struct uha_mscp *mscp = arg;
	struct scsipi_xfer *xs = mscp->xs;
	struct scsipi_periph *periph = xs->xs_periph;
	struct uha_softc *sc =
	    device_private(periph->periph_channel->chan_adapter->adapt_dev);
	int s;

	scsipi_printaddr(periph);
	printf("timed out");

	s = splbio();

	if (mscp->flags & MSCP_ABORT) {
		/* abort timed out */
		printf(" AGAIN\n");
		/* XXX Must reset! */
	} else {
		/* abort the operation that has timed out */
		printf("\n");
		mscp->xs->error = XS_TIMEOUT;
		mscp->timeout = UHA_ABORT_TIMEOUT;
		mscp->flags |= MSCP_ABORT;
		(sc->start_mbox)(sc, mscp);
	}

	splx(s);
}
