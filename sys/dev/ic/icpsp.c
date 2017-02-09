/*	$NetBSD: icpsp.c,v 1.26 2014/03/07 13:19:26 skrll Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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
__KERNEL_RCSID(0, "$NetBSD: icpsp.c,v 1.26 2014/03/07 13:19:26 skrll Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/queue.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/endian.h>
#include <sys/malloc.h>
#include <sys/scsiio.h>

#include <sys/bswap.h>
#include <sys/bus.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsi_disk.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>
#include <dev/scsipi/scsi_message.h>

#include <dev/ic/icpreg.h>
#include <dev/ic/icpvar.h>

struct icpsp_softc {
	device_t sc_dv;
	struct	scsipi_adapter sc_adapter;
	struct	scsipi_channel sc_channel;
	int	sc_busno;
	int	sc_openings;
};

void	icpsp_attach(device_t, device_t, void *);
void	icpsp_intr(struct icp_ccb *);
int	icpsp_match(device_t, cfdata_t, void *);
void	icpsp_scsipi_request(struct scsipi_channel *, scsipi_adapter_req_t,
			     void *);

void	icpsp_adjqparam(device_t, int);

CFATTACH_DECL_NEW(icpsp, sizeof(struct icpsp_softc),
    icpsp_match, icpsp_attach, NULL, NULL);

static const struct icp_servicecb icpsp_servicecb = {
	icpsp_adjqparam,
};

int
icpsp_match(device_t parent, cfdata_t match,
    void *aux)
{
	struct icp_attach_args *icpa;

	icpa = aux;

	return (icpa->icpa_unit >= ICPA_UNIT_SCSI);
}

void
icpsp_attach(device_t parent, device_t self, void *aux)
{
	struct icp_attach_args *icpa;
	struct icpsp_softc *sc;
	struct icp_softc *icp;

	icpa = (struct icp_attach_args *)aux;
	sc = device_private(self);
	icp = device_private(parent);

	sc->sc_dv = self;
	sc->sc_busno = icpa->icpa_unit - ICPA_UNIT_SCSI;
	sc->sc_openings = icp->icp_openings;
	printf(": physical SCSI channel %d\n", sc->sc_busno);

	icp_register_servicecb(icp, icpa->icpa_unit, &icpsp_servicecb);

	sc->sc_adapter.adapt_dev = sc->sc_dv;
	sc->sc_adapter.adapt_nchannels = 1;
	sc->sc_adapter.adapt_openings = icp->icp_openings;
	sc->sc_adapter.adapt_max_periph = icp->icp_openings;
	sc->sc_adapter.adapt_minphys = minphys;
	sc->sc_adapter.adapt_request = icpsp_scsipi_request;

	sc->sc_channel.chan_adapter = &sc->sc_adapter;
	sc->sc_channel.chan_bustype = &scsi_bustype;
	sc->sc_channel.chan_channel = 0;
	sc->sc_channel.chan_ntargets = ((icp->icp_class & ICP_FC) != 0 ?
	    127 : 16); /* XXX bogus check */
	sc->sc_channel.chan_nluns = 8;
	sc->sc_channel.chan_id = icp->icp_bus_id[sc->sc_busno];
	sc->sc_channel.chan_flags = SCSIPI_CHAN_NOSETTLE;

	config_found(self, &sc->sc_channel, scsiprint);
}

void
icpsp_scsipi_request(struct scsipi_channel *chan, scsipi_adapter_req_t req,
		     void *arg)
{
	struct scsipi_xfer *xs;
	struct scsipi_periph *periph;
	struct icpsp_softc *sc;
	struct icp_rawcmd *rc;
	struct icp_softc *icp;
	struct icp_ccb *ic;
	int rv, flags, s, soff;

	sc = device_private(chan->chan_adapter->adapt_dev);
	icp = device_private(device_parent(sc->sc_dv));

	switch (req) {
	case ADAPTER_REQ_RUN_XFER:
		xs = arg;
		periph = xs->xs_periph;
		flags = xs->xs_control;

		SC_DEBUG(periph, SCSIPI_DB2, ("icpsp_scsi_request run_xfer\n"));

		if ((flags & XS_CTL_RESET) != 0) {
			/* XXX Unimplemented. */
			xs->error = XS_DRIVER_STUFFUP;
			scsipi_done(xs);
			return;
		}

#if defined(ICP_DEBUG) || defined(SCSIDEBUG)
		if (xs->cmdlen > sizeof(rc->rc_cdb))
			panic("%s: CDB too large", device_xname(sc->sc_dv));
#endif

		/*
		 * Allocate a CCB.
		 */
		if (__predict_false((ic = icp_ccb_alloc(icp)) == NULL)) {
			xs->error = XS_RESOURCE_SHORTAGE;
			scsipi_done(xs);
			return;
		}
		rc = &ic->ic_cmd.cmd_packet.rc;
		ic->ic_sg = rc->rc_sg;
		ic->ic_service = ICP_SCSIRAWSERVICE;
		soff = ICP_SCRATCH_SENSE + ic->ic_ident *
		    sizeof(struct scsi_sense_data);

		/*
		 * Build the command.  We don't need to actively prevent
		 * access to array components, since the controller kindly
		 * takes care of that for us.
		 */
		ic->ic_cmd.cmd_opcode = htole16(ICP_WRITE);
		memcpy(rc->rc_cdb, xs->cmd, xs->cmdlen);

		rc->rc_padding0 = 0;
		rc->rc_direction = htole32((flags & XS_CTL_DATA_IN) != 0 ?
		    ICP_DATA_IN : ICP_DATA_OUT);
		rc->rc_mdisc_time = 0;
		rc->rc_mcon_time = 0;
		rc->rc_clen = htole32(xs->cmdlen);
		rc->rc_target = periph->periph_target;
		rc->rc_lun = periph->periph_lun;
		rc->rc_bus = sc->sc_busno;
		rc->rc_priority = 0;
		rc->rc_sense_len = htole32(sizeof(xs->sense.scsi_sense));
		rc->rc_sense_addr =
		    htole32(soff + icp->icp_scr_seg[0].ds_addr);
		rc->rc_padding1 = 0;

		if (xs->datalen != 0) {
			rv = icp_ccb_map(icp, ic, xs->data, xs->datalen,
			   (flags & XS_CTL_DATA_IN) != 0 ? IC_XFER_IN :
			   IC_XFER_OUT);
			if (rv != 0) {
				icp_ccb_free(icp, ic);
				xs->error = XS_DRIVER_STUFFUP;
				scsipi_done(xs);
				return;
			}

			rc->rc_nsgent = htole32(ic->ic_nsgent);
			rc->rc_sdata = ~0;
			rc->rc_sdlen = htole32(xs->datalen);
		} else {
			rc->rc_nsgent = 0;
			rc->rc_sdata = 0;
			rc->rc_sdlen = 0;
		}

		ic->ic_cmdlen = (u_long)ic->ic_sg - (u_long)&ic->ic_cmd +
		    ic->ic_nsgent * sizeof(*ic->ic_sg);

		bus_dmamap_sync(icp->icp_dmat, icp->icp_scr_dmamap, soff,
		    sizeof(xs->sense.scsi_sense), BUS_DMASYNC_PREREAD);

		/*
		 * Fire it off to the controller.
		 */
 		ic->ic_intr = icpsp_intr;
		ic->ic_context = xs;
		ic->ic_dv = sc->sc_dv;

		if ((flags & XS_CTL_POLL) != 0) {
			s = splbio();
			rv = icp_ccb_poll(icp, ic, xs->timeout);
			if (rv != 0) {
				if (xs->datalen != 0)
					icp_ccb_unmap(icp, ic);
				icp_ccb_free(icp, ic);
				xs->error = XS_TIMEOUT;
				scsipi_done(xs);

				/*
				 * XXX We're now in a bad way, because we
				 * don't know how to abort the command.
				 * That shouldn't matter too much, since
				 * polled commands won't be used while the
				 * system is running.
				 */
			}
			splx(s);
		} else
			icp_ccb_enqueue(icp, ic);

		break;

	case ADAPTER_REQ_GROW_RESOURCES:
	case ADAPTER_REQ_SET_XFER_MODE:
		/*
		 * Neither of these cases are supported, and neither of them
		 * is particulatly relevant, since we have an abstract view
		 * of the bus; the controller takes care of all the nitty
		 * gritty.
		 */
		break;
	}
}

void
icpsp_intr(struct icp_ccb *ic)
{
	struct scsipi_xfer *xs;
 	struct icp_softc *icp;
 	int soff;

#ifdef DIAGNOSTIC
	struct icpsp_softc *sc = device_private(ic->ic_dv);
#endif
	xs = ic->ic_context;
	icp = device_private(device_parent(ic->ic_dv));
	soff = ICP_SCRATCH_SENSE + ic->ic_ident *
	    sizeof(struct scsi_sense_data);

	SC_DEBUG(xs->xs_periph, SCSIPI_DB2, ("icpsp_intr\n"));

	bus_dmamap_sync(icp->icp_dmat, icp->icp_scr_dmamap, soff,
	    sizeof(xs->sense.scsi_sense), BUS_DMASYNC_POSTREAD);

	if (ic->ic_status == ICP_S_OK) {
		xs->status = SCSI_OK;
		xs->resid = 0;
	} else if (ic->ic_status != ICP_S_RAW_SCSI || icp->icp_info >= 0x100) {
		xs->error = XS_SELTIMEOUT;
		xs->resid = xs->datalen;
	} else {
		xs->status = icp->icp_info;

		switch (xs->status) {
		case SCSI_OK:
#ifdef DIAGNOSTIC
			printf("%s: error return (%d), but SCSI_OK?\n",
			    device_xname(sc->sc_dv), icp->icp_info);
#endif
			xs->resid = 0;
			break;
		case SCSI_CHECK:
			memcpy(&xs->sense.scsi_sense,
			    (char *)icp->icp_scr + soff,
			    sizeof(xs->sense.scsi_sense));
			xs->error = XS_SENSE;
			/* FALLTHROUGH */
		default:
			/*
			 * XXX Don't know how to get residual count.
			 */
			xs->resid = xs->datalen;
			break;
		}
	}

	if (xs->datalen != 0)
		icp_ccb_unmap(icp, ic);
	icp_ccb_free(icp, ic);
	scsipi_done(xs);
}

void
icpsp_adjqparam(device_t self, int openings)
{
	struct icpsp_softc *sc = device_private(self);
	int s;

	s = splbio();
	sc->sc_adapter.adapt_openings += openings - sc->sc_openings;
	sc->sc_openings = openings;
	splx(s);
}
