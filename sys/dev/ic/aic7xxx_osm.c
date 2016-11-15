/*	$NetBSD: aic7xxx_osm.c,v 1.37 2010/02/24 22:37:57 dyoung Exp $	*/

/*
 * Bus independent FreeBSD shim for the aic7xxx based adaptec SCSI controllers
 *
 * Copyright (c) 1994-2001 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU Public License ("GPL").
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * //depot/aic7xxx/freebsd/dev/aic7xxx/aic7xxx_osm.c#12 $
 *
 * $FreeBSD: src/sys/dev/aic7xxx/aic7xxx_osm.c,v 1.31 2002/11/30 19:08:58 scottl Exp $
 */
/*
 * Ported from FreeBSD by Pascal Renauld, Network Storage Solutions, Inc. - April 2003
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: aic7xxx_osm.c,v 1.37 2010/02/24 22:37:57 dyoung Exp $");

#include <dev/ic/aic7xxx_osm.h>
#include <dev/ic/aic7xxx_inline.h>

#ifndef AHC_TMODE_ENABLE
#define AHC_TMODE_ENABLE 0
#endif


static void	ahc_action(struct scsipi_channel *chan,
			   scsipi_adapter_req_t req, void *arg);
static void	ahc_execute_scb(void *arg, bus_dma_segment_t *dm_segs,
				int nsegments);
static int	ahc_poll(struct ahc_softc *ahc, int wait);
static void	ahc_setup_data(struct ahc_softc *ahc,
			       struct scsipi_xfer *xs, struct scb *scb);
static void	ahc_set_recoveryscb(struct ahc_softc *ahc, struct scb *scb);
static int	ahc_ioctl(struct scsipi_channel *channel, u_long cmd,
			  void *addr, int flag, struct proc *p);

static bool	ahc_pmf_suspend(device_t, const pmf_qual_t *);
static bool	ahc_pmf_resume(device_t, const pmf_qual_t *);
static bool	ahc_pmf_shutdown(device_t, int);


/*
 * Attach all the sub-devices we can find
 */
int
ahc_attach(struct ahc_softc *ahc)
{
	u_long 	s;
	int i;
	char ahc_info[256];

	LIST_INIT(&ahc->pending_scbs);
	for (i = 0; i < AHC_NUM_TARGETS; i++)
		TAILQ_INIT(&ahc->untagged_queues[i]);

	ahc_lock(ahc, &s);

	ahc->sc_adapter.adapt_dev = ahc->sc_dev;
	ahc->sc_adapter.adapt_nchannels = (ahc->features & AHC_TWIN) ? 2 : 1;

	ahc->sc_adapter.adapt_openings = ahc->scb_data->numscbs - 1;
	ahc->sc_adapter.adapt_max_periph = 16;

	ahc->sc_adapter.adapt_ioctl = ahc_ioctl;
	ahc->sc_adapter.adapt_minphys = ahc_minphys;
	ahc->sc_adapter.adapt_request = ahc_action;

	ahc->sc_channel.chan_adapter = &ahc->sc_adapter;
	ahc->sc_channel.chan_bustype = &scsi_bustype;
	ahc->sc_channel.chan_channel = 0;
	ahc->sc_channel.chan_ntargets = (ahc->features & AHC_WIDE) ? 16 : 8;
	ahc->sc_channel.chan_nluns = 8 /*AHC_NUM_LUNS*/;
	ahc->sc_channel.chan_id = ahc->our_id;
	ahc->sc_channel.chan_flags |= SCSIPI_CHAN_CANGROW;

	if (ahc->features & AHC_TWIN) {
		ahc->sc_channel_b = ahc->sc_channel;
		ahc->sc_channel_b.chan_id = ahc->our_id_b;
		ahc->sc_channel_b.chan_channel = 1;
	}

	ahc_controller_info(ahc, ahc_info, sizeof(ahc_info));
	printf("%s: %s\n", device_xname(ahc->sc_dev), ahc_info);

	if ((ahc->flags & AHC_PRIMARY_CHANNEL) == 0) {
		ahc->sc_child = config_found(ahc->sc_dev,
		    &ahc->sc_channel, scsiprint);
		if (ahc->features & AHC_TWIN)
			ahc->sc_child_b = config_found(ahc->sc_dev,
			    &ahc->sc_channel_b, scsiprint);
	} else {
		if (ahc->features & AHC_TWIN)
			ahc->sc_child = config_found(ahc->sc_dev,
			    &ahc->sc_channel_b, scsiprint);
		ahc->sc_child_b = config_found(ahc->sc_dev,
		    &ahc->sc_channel, scsiprint);
	}

	ahc_intr_enable(ahc, TRUE);

	if (ahc->flags & AHC_RESET_BUS_A)
		ahc_reset_channel(ahc, 'A', TRUE);
	if ((ahc->features & AHC_TWIN) && ahc->flags & AHC_RESET_BUS_B)
		ahc_reset_channel(ahc, 'B', TRUE);

	if (!pmf_device_register1(ahc->sc_dev,
	    ahc_pmf_suspend, ahc_pmf_resume, ahc_pmf_shutdown))
		aprint_error_dev(ahc->sc_dev,
		    "couldn't establish power handler\n");

	ahc_unlock(ahc, &s);
	return (1);
}

/*
 * XXX we should call the real suspend and resume functions here
 *     but pmf(9) stuff on cardbus backend is untested yet
 */

static bool
ahc_pmf_suspend(device_t dev, const pmf_qual_t *qual)
{
	struct ahc_softc *sc = device_private(dev);
#if 0
	return (ahc_suspend(sc) == 0);
#else
	ahc_shutdown(sc);
	return true;
#endif
}

static bool
ahc_pmf_resume(device_t dev, const pmf_qual_t *qual)
{
#if 0
	struct ahc_softc *sc = device_private(dev);

	return (ahc_resume(sc) == 0);
#else
	return true;
#endif
}

static bool
ahc_pmf_shutdown(device_t dev, int howto)
{
	struct ahc_softc *sc = device_private(dev);

	/* Disable all interrupt sources by resetting the controller */
	ahc_shutdown(sc);

	return true;
}

/*
 * Catch an interrupt from the adapter
 */
void
ahc_platform_intr(void *arg)
{
	struct	ahc_softc *ahc;

	ahc = arg;
	ahc_intr(ahc);
}

/*
 * We have an scb which has been processed by the
 * adaptor, now we look to see how the operation
 * went.
 */
void
ahc_done(struct ahc_softc *ahc, struct scb *scb)
{
	struct scsipi_xfer *xs;
	struct scsipi_periph *periph;
	u_long s;

	xs = scb->xs;
	periph = xs->xs_periph;
	LIST_REMOVE(scb, pending_links);
	if ((scb->flags & SCB_UNTAGGEDQ) != 0) {
		struct scb_tailq *untagged_q;
		int target_offset;

		target_offset = SCB_GET_TARGET_OFFSET(ahc, scb);
		untagged_q = &ahc->untagged_queues[target_offset];
		TAILQ_REMOVE(untagged_q, scb, links.tqe);
		scb->flags &= ~SCB_UNTAGGEDQ;
		ahc_run_untagged_queue(ahc, untagged_q);
	}

	callout_stop(&scb->xs->xs_callout);

	if (xs->datalen) {
		int op;

		if (xs->xs_control & XS_CTL_DATA_IN)
			op = BUS_DMASYNC_POSTREAD;
		else
			op = BUS_DMASYNC_POSTWRITE;
		bus_dmamap_sync(ahc->parent_dmat, scb->dmamap, 0,
				scb->dmamap->dm_mapsize, op);
		bus_dmamap_unload(ahc->parent_dmat, scb->dmamap);
	}

	/*
	 * If the recovery SCB completes, we have to be
	 * out of our timeout.
	 */
	if ((scb->flags & SCB_RECOVERY_SCB) != 0) {
		struct	scb *list_scb;

		/*
		 * We were able to complete the command successfully,
		 * so reinstate the timeouts for all other pending
		 * commands.
		 */
		LIST_FOREACH(list_scb, &ahc->pending_scbs, pending_links) {
			if (!(list_scb->xs->xs_control & XS_CTL_POLL)) {
				callout_reset(&list_scb->xs->xs_callout,
				    (list_scb->xs->timeout > 1000000) ?
				    (list_scb->xs->timeout / 1000) * hz :
				    (list_scb->xs->timeout * hz) / 1000,
				    ahc_timeout, list_scb);
			}
		}

		if (ahc_get_transaction_status(scb) == CAM_BDR_SENT
		 || ahc_get_transaction_status(scb) == CAM_REQ_ABORTED)
			ahc_set_transaction_status(scb, CAM_CMD_TIMEOUT);
		scsipi_printaddr(xs->xs_periph);
		printf("%s: no longer in timeout, status = %x\n",
		       ahc_name(ahc), xs->status);
	}

	/* Don't clobber any existing error state */
	if (xs->error != XS_NOERROR) {
	  /* Don't clobber any existing error state */
	} else if ((scb->flags & SCB_SENSE) != 0) {
		/*
		 * We performed autosense retrieval.
		 *
		 * Zero any sense not transferred by the
		 * device.  The SCSI spec mandates that any
		 * untransferred data should be assumed to be
		 * zero.  Complete the 'bounce' of sense information
		 * through buffers accessible via bus-space by
		 * copying it into the clients csio.
		 */
		memset(&xs->sense.scsi_sense, 0, sizeof(xs->sense.scsi_sense));
		memcpy(&xs->sense.scsi_sense,
		       ahc_get_sense_buf(ahc, scb),
		       sizeof(xs->sense.scsi_sense));
		xs->error = XS_SENSE;
	}
	if (scb->flags & SCB_FREEZE_QUEUE) {
		scsipi_periph_thaw(periph, 1);
		scb->flags &= ~SCB_FREEZE_QUEUE;
	}

	ahc_lock(ahc, &s);
	ahc_free_scb(ahc, scb);
	ahc_unlock(ahc, &s);

	scsipi_done(xs);
}

static int
ahc_ioctl(struct scsipi_channel *channel, u_long cmd, void *addr,
    int flag, struct proc *p)
{
	struct ahc_softc *ahc;
	int s, ret = ENOTTY;

	ahc = device_private(channel->chan_adapter->adapt_dev);

	switch (cmd) {
	case SCBUSIORESET:
		s = splbio();
		ahc_reset_channel(ahc, channel->chan_channel == 1 ? 'B' : 'A',
		    TRUE);
		splx(s);
		ret = 0;
		break;
	default:
		break;
	}

	return ret;
}

static void
ahc_action(struct scsipi_channel *chan, scsipi_adapter_req_t req, void *arg)
{
	struct ahc_softc *ahc;
	int s;
	struct ahc_initiator_tinfo *tinfo;
	struct ahc_tmode_tstate *tstate;

	ahc  = device_private(chan->chan_adapter->adapt_dev);

	switch (req) {

	case ADAPTER_REQ_RUN_XFER:
	  {
		struct scsipi_xfer *xs;
		struct scsipi_periph *periph;
		struct scb *scb;
		struct hardware_scb *hscb;
		u_int target_id;
		u_int our_id;
		u_long ss;

		xs = arg;
		periph = xs->xs_periph;

		target_id = periph->periph_target;
		our_id = ahc->our_id;

		SC_DEBUG(xs->xs_periph, SCSIPI_DB3, ("ahc_action\n"));

		/*
		 * get an scb to use.
		 */
		ahc_lock(ahc, &ss);
		if ((scb = ahc_get_scb(ahc)) == NULL) {
			xs->error = XS_RESOURCE_SHORTAGE;
			ahc_unlock(ahc, &ss);
			scsipi_done(xs);
			return;
		}
		ahc_unlock(ahc, &ss);

		hscb = scb->hscb;

		SC_DEBUG(periph, SCSIPI_DB3, ("start scb(%p)\n", scb));
		scb->xs = xs;

		/*
		 * Put all the arguments for the xfer in the scb
		 */
		hscb->control = 0;
		hscb->scsiid = BUILD_SCSIID(ahc, 0, target_id, our_id);
		hscb->lun = periph->periph_lun;
		if (xs->xs_control & XS_CTL_RESET) {
			hscb->cdb_len = 0;
			scb->flags |= SCB_DEVICE_RESET;
			hscb->control |= MK_MESSAGE;
			ahc_execute_scb(scb, NULL, 0);
		}

		ahc_setup_data(ahc, xs, scb);

		break;
	  }
	case ADAPTER_REQ_GROW_RESOURCES:
#ifdef AHC_DEBUG
		printf("%s: ADAPTER_REQ_GROW_RESOURCES\n", ahc_name(ahc));
#endif
		chan->chan_adapter->adapt_openings += ahc_alloc_scbs(ahc);
		if (ahc->scb_data->numscbs >= AHC_SCB_MAX_ALLOC)
			chan->chan_flags &= ~SCSIPI_CHAN_CANGROW;
		return;

	case ADAPTER_REQ_SET_XFER_MODE:
	    {
		struct scsipi_xfer_mode *xm = arg;
		struct ahc_devinfo devinfo;
		int target_id, our_id, first;
		u_int width;
		char channel;
		u_int ppr_options = 0, period, offset;
		struct ahc_syncrate *syncrate;
		uint16_t old_autoneg;

		target_id = xm->xm_target;
		our_id = chan->chan_id;
		channel = (chan->chan_channel == 1) ? 'B' : 'A';
		s = splbio();
		tinfo = ahc_fetch_transinfo(ahc, channel, our_id, target_id,
		    &tstate);
		ahc_compile_devinfo(&devinfo, our_id, target_id,
		    0, channel, ROLE_INITIATOR);

		old_autoneg = tstate->auto_negotiate;

		/*
		 * XXX since the period and offset are not provided here,
		 * fake things by forcing a renegotiation using the user
		 * settings if this is called for the first time (i.e.
		 * during probe). Also, cap various values at the user
		 * values, assuming that the user set it up that way.
		 */
		if (ahc->inited_target[target_id] == 0) {
			period = tinfo->user.period;
			offset = tinfo->user.offset;
			ppr_options = tinfo->user.ppr_options;
			width = tinfo->user.width;
			tstate->tagenable |=
			    (ahc->user_tagenable & devinfo.target_mask);
			tstate->discenable |=
			    (ahc->user_discenable & devinfo.target_mask);
			ahc->inited_target[target_id] = 1;
			first = 1;
		} else
			first = 0;

		if (xm->xm_mode & (PERIPH_CAP_WIDE16 | PERIPH_CAP_DT))
			width = MSG_EXT_WDTR_BUS_16_BIT;
		else
			width = MSG_EXT_WDTR_BUS_8_BIT;

		ahc_validate_width(ahc, NULL, &width, ROLE_UNKNOWN);
		if (width > tinfo->user.width)
			width = tinfo->user.width;
		ahc_set_width(ahc, &devinfo, width, AHC_TRANS_GOAL, FALSE);

		if (!(xm->xm_mode & (PERIPH_CAP_SYNC | PERIPH_CAP_DT))) {
			period = 0;
			offset = 0;
			ppr_options = 0;
		}

		if ((xm->xm_mode & PERIPH_CAP_DT) &&
		    (ppr_options & MSG_EXT_PPR_DT_REQ))
			ppr_options |= MSG_EXT_PPR_DT_REQ;
		else
			ppr_options &= ~MSG_EXT_PPR_DT_REQ;
		if ((tstate->discenable & devinfo.target_mask) == 0 ||
		    (tstate->tagenable & devinfo.target_mask) == 0)
			ppr_options &= ~MSG_EXT_PPR_IU_REQ;

		if ((xm->xm_mode & PERIPH_CAP_TQING) &&
		    (ahc->user_tagenable & devinfo.target_mask))
			tstate->tagenable |= devinfo.target_mask;
		else
			tstate->tagenable &= ~devinfo.target_mask;

		syncrate = ahc_find_syncrate(ahc, &period, &ppr_options,
		    AHC_SYNCRATE_MAX);
		ahc_validate_offset(ahc, NULL, syncrate, &offset,
		    width, ROLE_UNKNOWN);

		if (offset == 0) {
			period = 0;
			ppr_options = 0;
		}

		if (ppr_options != 0
		    && tinfo->user.transport_version >= 3) {
			tinfo->goal.transport_version =
			    tinfo->user.transport_version;
			tinfo->curr.transport_version =
			    tinfo->user.transport_version;
		}

		ahc_set_syncrate(ahc, &devinfo, syncrate, period, offset,
		    ppr_options, AHC_TRANS_GOAL, FALSE);

		/*
		 * If this is the first request, and no negotiation is
		 * needed, just confirm the state to the scsipi layer,
		 * so that it can print a message.
		 */
		if (old_autoneg == tstate->auto_negotiate && first) {
			xm->xm_mode = 0;
			xm->xm_period = tinfo->curr.period;
			xm->xm_offset = tinfo->curr.offset;
			if (tinfo->curr.width == MSG_EXT_WDTR_BUS_16_BIT)
				xm->xm_mode |= PERIPH_CAP_WIDE16;
			if (tinfo->curr.period)
				xm->xm_mode |= PERIPH_CAP_SYNC;
			if (tstate->tagenable & devinfo.target_mask)
				xm->xm_mode |= PERIPH_CAP_TQING;
			if (tinfo->curr.ppr_options & MSG_EXT_PPR_DT_REQ)
				xm->xm_mode |= PERIPH_CAP_DT;
			scsipi_async_event(chan, ASYNC_EVENT_XFER_MODE, xm);
		}
		splx(s);
	    }
	}

	return;
}

static void
ahc_execute_scb(void *arg, bus_dma_segment_t *dm_segs, int nsegments)
{
	struct	scb *scb;
	struct scsipi_xfer *xs;
	struct	ahc_softc *ahc;
	struct	ahc_initiator_tinfo *tinfo;
	struct	ahc_tmode_tstate *tstate;

	u_int	mask;
	u_long	s;

	scb = (struct scb *)arg;
	xs = scb->xs;
	xs->error = 0;
	xs->status = 0;
	xs->xs_status = 0;
	ahc = device_private(
	    xs->xs_periph->periph_channel->chan_adapter->adapt_dev);

	if (nsegments != 0) {
		struct ahc_dma_seg *sg;
		bus_dma_segment_t *end_seg;
		int op;

		end_seg = dm_segs + nsegments;

		/* Copy the segments into our SG list */
		sg = scb->sg_list;
		while (dm_segs < end_seg) {
			uint32_t len;

			sg->addr = ahc_htole32(dm_segs->ds_addr);
			len = dm_segs->ds_len
			    | ((dm_segs->ds_addr >> 8) & AHC_SG_HIGH_ADDR_MASK);
			sg->len = ahc_htole32(len);
			sg++;
			dm_segs++;
		}

		/*
		 * Note where to find the SG entries in bus space.
		 * We also set the full residual flag which the
		 * sequencer will clear as soon as a data transfer
		 * occurs.
		 */
		scb->hscb->sgptr = ahc_htole32(scb->sg_list_phys|SG_FULL_RESID);

		if (xs->xs_control & XS_CTL_DATA_IN)
			op = BUS_DMASYNC_PREREAD;
		else
			op = BUS_DMASYNC_PREWRITE;

		bus_dmamap_sync(ahc->parent_dmat, scb->dmamap, 0,
				scb->dmamap->dm_mapsize, op);

		sg--;
		sg->len |= ahc_htole32(AHC_DMA_LAST_SEG);

		/* Copy the first SG into the "current" data pointer area */
		scb->hscb->dataptr = scb->sg_list->addr;
		scb->hscb->datacnt = scb->sg_list->len;
	} else {
		scb->hscb->sgptr = ahc_htole32(SG_LIST_NULL);
		scb->hscb->dataptr = 0;
		scb->hscb->datacnt = 0;
	}

	scb->sg_count = nsegments;

	ahc_lock(ahc, &s);

	/*
	 * Last time we need to check if this SCB needs to
	 * be aborted.
	 */
	if (xs->xs_status & XS_STS_DONE) {
		if (nsegments != 0)
			bus_dmamap_unload(ahc->buffer_dmat, scb->dmamap);
		ahc_free_scb(ahc, scb);
		ahc_unlock(ahc, &s);
		scsipi_done(xs);
		return;
	}

	tinfo = ahc_fetch_transinfo(ahc, ahc->channel,
				    SCSIID_OUR_ID(scb->hscb->scsiid),
				    SCSIID_TARGET(ahc, scb->hscb->scsiid),
				    &tstate);

	mask = SCB_GET_TARGET_MASK(ahc, scb);
	scb->hscb->scsirate = tinfo->scsirate;
	scb->hscb->scsioffset = tinfo->curr.offset;

	if ((tstate->ultraenb & mask) != 0)
		scb->hscb->control |= ULTRAENB;

	if ((tstate->discenable & mask) != 0)
		scb->hscb->control |= DISCENB;

	if (xs->xs_tag_type)
		scb->hscb->control |= xs->xs_tag_type;

#if 1	/* This looks like it makes sense at first, but it can loop */
	if ((xs->xs_control & XS_CTL_DISCOVERY) && (tinfo->goal.width == 0
	     && tinfo->goal.offset == 0
	     && tinfo->goal.ppr_options == 0)) {
		scb->flags |= SCB_NEGOTIATE;
		scb->hscb->control |= MK_MESSAGE;
	} else
#endif
	if ((tstate->auto_negotiate & mask) != 0) {
		scb->flags |= SCB_AUTO_NEGOTIATE;
		scb->hscb->control |= MK_MESSAGE;
	}

	LIST_INSERT_HEAD(&ahc->pending_scbs, scb, pending_links);

	if (!(xs->xs_control & XS_CTL_POLL)) {
		callout_reset(&scb->xs->xs_callout, xs->timeout > 1000000 ?
			      (xs->timeout / 1000) * hz : (xs->timeout * hz) / 1000,
			      ahc_timeout, scb);
	}

	/*
	 * We only allow one untagged transaction
	 * per target in the initiator role unless
	 * we are storing a full busy target *lun*
	 * table in SCB space.
	 */
	if ((scb->hscb->control & (TARGET_SCB|TAG_ENB)) == 0
	    && (ahc->flags & AHC_SCB_BTT) == 0) {
		struct scb_tailq *untagged_q;
		int target_offset;

		target_offset = SCB_GET_TARGET_OFFSET(ahc, scb);
		untagged_q = &(ahc->untagged_queues[target_offset]);
		TAILQ_INSERT_TAIL(untagged_q, scb, links.tqe);
		scb->flags |= SCB_UNTAGGEDQ;
		if (TAILQ_FIRST(untagged_q) != scb) {
			ahc_unlock(ahc, &s);
			return;
		}
	}
	scb->flags |= SCB_ACTIVE;

	if ((scb->flags & SCB_TARGET_IMMEDIATE) != 0) {
		/* Define a mapping from our tag to the SCB. */
		ahc->scb_data->scbindex[scb->hscb->tag] = scb;
		ahc_pause(ahc);
		if ((ahc->flags & AHC_PAGESCBS) == 0)
			ahc_outb(ahc, SCBPTR, scb->hscb->tag);
		ahc_outb(ahc, TARG_IMMEDIATE_SCB, scb->hscb->tag);
		ahc_unpause(ahc);
	} else {
		ahc_queue_scb(ahc, scb);
	}

	if (!(xs->xs_control & XS_CTL_POLL)) {
		ahc_unlock(ahc, &s);
		return;
	}

	/*
	 * If we can't use interrupts, poll for completion
	 */

	SC_DEBUG(xs->xs_periph, SCSIPI_DB3, ("cmd_poll\n"));
	do {
		if (ahc_poll(ahc, xs->timeout)) {
			if (!(xs->xs_control & XS_CTL_SILENT))
				printf("cmd fail\n");
			ahc_timeout(scb);
			break;
		}
	} while (!(xs->xs_status & XS_STS_DONE));
	ahc_unlock(ahc, &s);

	return;
}

static int
ahc_poll(struct ahc_softc *ahc, int wait)
{
	while (--wait) {
		DELAY(1000);
		if (ahc_inb(ahc, INTSTAT) & INT_PEND)
			break;
	}

	if (wait == 0) {
		printf("%s: board is not responding\n", ahc_name(ahc));
		return (EIO);
	}

	ahc_intr(ahc);
	return (0);
}

static void
ahc_setup_data(struct ahc_softc *ahc, struct scsipi_xfer *xs,
	       struct scb *scb)
{
	struct hardware_scb *hscb;

	hscb = scb->hscb;
	xs->resid = xs->status = 0;

	hscb->cdb_len = xs->cmdlen;
	if (hscb->cdb_len > sizeof(hscb->cdb32)) {
		u_long s;

		ahc_set_transaction_status(scb, CAM_REQ_INVALID);
		ahc_lock(ahc, &s);
		ahc_free_scb(ahc, scb);
		ahc_unlock(ahc, &s);
		scsipi_done(xs);
		return;
	}

	if (hscb->cdb_len > 12) {
		memcpy(hscb->cdb32, xs->cmd, hscb->cdb_len);
		scb->flags |= SCB_CDB32_PTR;
	} else {
		memcpy(hscb->shared_data.cdb, xs->cmd, hscb->cdb_len);
	}

	/* Only use S/G if there is a transfer */
	if (xs->datalen) {
		int error;

		error = bus_dmamap_load(ahc->parent_dmat,
					scb->dmamap, xs->data,
					xs->datalen, NULL,
					((xs->xs_control & XS_CTL_NOSLEEP) ?
					 BUS_DMA_NOWAIT : BUS_DMA_WAITOK) |
					BUS_DMA_STREAMING |
					((xs->xs_control & XS_CTL_DATA_IN) ?
					 BUS_DMA_READ : BUS_DMA_WRITE));
		if (error) {
#ifdef AHC_DEBUG
			printf("%s: in ahc_setup_data(): bus_dmamap_load() "
			       "= %d\n",
			       ahc_name(ahc), error);
#endif
			xs->error = XS_RESOURCE_SHORTAGE;
			scsipi_done(xs);
			return;
		}
		ahc_execute_scb(scb,
				scb->dmamap->dm_segs,
				scb->dmamap->dm_nsegs);
	} else {
		ahc_execute_scb(scb, NULL, 0);
	}
}

static void
ahc_set_recoveryscb(struct ahc_softc *ahc, struct scb *scb) {

	if ((scb->flags & SCB_RECOVERY_SCB) == 0) {
		struct scb *list_scb;

		scb->flags |= SCB_RECOVERY_SCB;

		/*
		 * Take all queued, but not sent SCBs out of the equation.
		 * Also ensure that no new CCBs are queued to us while we
		 * try to fix this problem.
		 */
		scsipi_channel_freeze(&ahc->sc_channel, 1);
		if (ahc->features & AHC_TWIN)
			scsipi_channel_freeze(&ahc->sc_channel_b, 1);

		/*
		 * Go through all of our pending SCBs and remove
		 * any scheduled timeouts for them.  We will reschedule
		 * them after we've successfully fixed this problem.
		 */
		LIST_FOREACH(list_scb, &ahc->pending_scbs, pending_links) {
			callout_stop(&list_scb->xs->xs_callout);
		}
	}
}

void
ahc_timeout(void *arg)
{
	struct	scb *scb;
	struct	ahc_softc *ahc;
	u_long	s;
	int	found;
	u_int	last_phase;
	int	target;
	int	lun;
	int	i;
	char	channel;

	scb = arg;
	ahc = scb->ahc_softc;

	ahc_lock(ahc, &s);

	ahc_pause_and_flushwork(ahc);

	if ((scb->flags & SCB_ACTIVE) == 0) {
		/* Previous timeout took care of me already */
		printf("%s: Timedout SCB already complete. "
		       "Interrupts may not be functioning.\n", ahc_name(ahc));
		ahc_unpause(ahc);
		ahc_unlock(ahc, &s);
		return;
	}

	target = SCB_GET_TARGET(ahc, scb);
	channel = SCB_GET_CHANNEL(ahc, scb);
	lun = SCB_GET_LUN(scb);

	ahc_print_path(ahc, scb);
	printf("SCB 0x%x - timed out\n", scb->hscb->tag);
	ahc_dump_card_state(ahc);
	last_phase = ahc_inb(ahc, LASTPHASE);
	if (scb->sg_count > 0) {
		for (i = 0; i < scb->sg_count; i++) {
			printf("sg[%d] - Addr 0x%x : Length %d\n",
			       i,
			       scb->sg_list[i].addr,
			       scb->sg_list[i].len & AHC_SG_LEN_MASK);
		}
	}
	if (scb->flags & (SCB_DEVICE_RESET|SCB_ABORT)) {
		/*
		 * Been down this road before.
		 * Do a full bus reset.
		 */
bus_reset:
		ahc_set_transaction_status(scb, CAM_CMD_TIMEOUT);
		found = ahc_reset_channel(ahc, channel, /*Initiate Reset*/TRUE);
		printf("%s: Issued Channel %c Bus Reset. "
		       "%d SCBs aborted\n", ahc_name(ahc), channel, found);
	} else {
		/*
		 * If we are a target, transition to bus free and report
		 * the timeout.
		 *
		 * The target/initiator that is holding up the bus may not
		 * be the same as the one that triggered this timeout
		 * (different commands have different timeout lengths).
		 * If the bus is idle and we are acting as the initiator
		 * for this request, queue a BDR message to the timed out
		 * target.  Otherwise, if the timed out transaction is
		 * active:
		 *   Initiator transaction:
		 *	Stuff the message buffer with a BDR message and assert
		 *	ATN in the hopes that the target will let go of the bus
		 *	and go to the mesgout phase.  If this fails, we'll
		 *	get another timeout 2 seconds later which will attempt
		 *	a bus reset.
		 *
		 *   Target transaction:
		 *	Transition to BUS FREE and report the error.
		 *	It's good to be the target!
		 */
		u_int active_scb_index;
		u_int saved_scbptr;

		saved_scbptr = ahc_inb(ahc, SCBPTR);
		active_scb_index = ahc_inb(ahc, SCB_TAG);

		if ((ahc_inb(ahc, SEQ_FLAGS) & NOT_IDENTIFIED) == 0
		  && (active_scb_index < ahc->scb_data->numscbs)) {
			struct scb *active_scb;

			/*
			 * If the active SCB is not us, assume that
			 * the active SCB has a longer timeout than
			 * the timedout SCB, and wait for the active
			 * SCB to timeout.
			 */
			active_scb = ahc_lookup_scb(ahc, active_scb_index);
			if (active_scb != scb) {
				uint64_t newtimeout;

				ahc_print_path(ahc, scb);
				printf("Other SCB Timeout%s",
				       (scb->flags & SCB_OTHERTCL_TIMEOUT) != 0
				       ? " again\n" : "\n");
				scb->flags |= SCB_OTHERTCL_TIMEOUT;
				newtimeout = MAX(active_scb->xs->timeout,
						 scb->xs->timeout);
				callout_reset(&scb->xs->xs_callout,
				    newtimeout > 1000000 ?
				    (newtimeout / 1000) * hz :
				    (newtimeout * hz) / 1000,
				    ahc_timeout, scb);
				ahc_unpause(ahc);
				ahc_unlock(ahc, &s);
				return;
			}

			/* It's us */
			if ((scb->flags & SCB_TARGET_SCB) != 0) {

				/*
				 * Send back any queued up transactions
				 * and properly record the error condition.
				 */
				ahc_abort_scbs(ahc, SCB_GET_TARGET(ahc, scb),
					       SCB_GET_CHANNEL(ahc, scb),
					       SCB_GET_LUN(scb),
					       scb->hscb->tag,
					       ROLE_TARGET,
					       CAM_CMD_TIMEOUT);

				/* Will clear us from the bus */
				ahc_restart(ahc);
				ahc_unlock(ahc, &s);
				return;
			}

			ahc_set_recoveryscb(ahc, active_scb);
			ahc_outb(ahc, MSG_OUT, HOST_MSG);
			ahc_outb(ahc, SCSISIGO, last_phase|ATNO);
			ahc_print_path(ahc, active_scb);
			printf("BDR message in message buffer\n");
			active_scb->flags |= SCB_DEVICE_RESET;
			callout_reset(&active_scb->xs->xs_callout,
				      2 * hz, ahc_timeout, active_scb);
			ahc_unpause(ahc);
		} else {
			int disconnected;

			/* XXX Shouldn't panic.  Just punt instead? */
			if ((scb->flags & SCB_TARGET_SCB) != 0)
				panic("Timed-out target SCB but bus idle");

			if (last_phase != P_BUSFREE
			 && (ahc_inb(ahc, SSTAT0) & TARGET) != 0) {
				/* XXX What happened to the SCB? */
				/* Hung target selection.  Goto busfree */
				printf("%s: Hung target selection\n",
				       ahc_name(ahc));
				ahc_restart(ahc);
				ahc_unlock(ahc, &s);
				return;
			}

			if (ahc_search_qinfifo(ahc, target, channel, lun,
					       scb->hscb->tag, ROLE_INITIATOR,
					       /*status*/0, SEARCH_COUNT) > 0) {
				disconnected = FALSE;
			} else {
				disconnected = TRUE;
			}

			if (disconnected) {

				ahc_set_recoveryscb(ahc, scb);
				/*
				 * Actually re-queue this SCB in an attempt
				 * to select the device before it reconnects.
				 * In either case (selection or reselection),
				 * we will now issue a target reset to the
				 * timed-out device.
				 *
				 * Set the MK_MESSAGE control bit indicating
				 * that we desire to send a message.  We
				 * also set the disconnected flag since
				 * in the paging case there is no guarantee
				 * that our SCB control byte matches the
				 * version on the card.  We don't want the
				 * sequencer to abort the command thinking
				 * an unsolicited reselection occurred.
				 */
				scb->hscb->control |= MK_MESSAGE|DISCONNECTED;
				scb->flags |= SCB_DEVICE_RESET;

				/*
				 * Remove any cached copy of this SCB in the
				 * disconnected list in preparation for the
				 * queuing of our abort SCB.  We use the
				 * same element in the SCB, SCB_NEXT, for
				 * both the qinfifo and the disconnected list.
				 */
				ahc_search_disc_list(ahc, target, channel,
						     lun, scb->hscb->tag,
						     /*stop_on_first*/TRUE,
						     /*remove*/TRUE,
						     /*save_state*/FALSE);

				/*
				 * In the non-paging case, the sequencer will
				 * never re-reference the in-core SCB.
				 * To make sure we are notified during
				 * reslection, set the MK_MESSAGE flag in
				 * the card's copy of the SCB.
				 */
				if ((ahc->flags & AHC_PAGESCBS) == 0) {
					ahc_outb(ahc, SCBPTR, scb->hscb->tag);
					ahc_outb(ahc, SCB_CONTROL,
						 ahc_inb(ahc, SCB_CONTROL)
						| MK_MESSAGE);
				}

				/*
				 * Clear out any entries in the QINFIFO first
				 * so we are the next SCB for this target
				 * to run.
				 */
				ahc_search_qinfifo(ahc,
						   SCB_GET_TARGET(ahc, scb),
						   channel, SCB_GET_LUN(scb),
						   SCB_LIST_NULL,
						   ROLE_INITIATOR,
						   CAM_REQUEUE_REQ,
						   SEARCH_COMPLETE);
				ahc_print_path(ahc, scb);
				printf("Queuing a BDR SCB\n");
				ahc_qinfifo_requeue_tail(ahc, scb);
				ahc_outb(ahc, SCBPTR, saved_scbptr);
				callout_reset(&scb->xs->xs_callout, 2 * hz,
					      ahc_timeout, scb);
				ahc_unpause(ahc);
			} else {
				/* Go "immediatly" to the bus reset */
				/* This shouldn't happen */
				ahc_set_recoveryscb(ahc, scb);
				ahc_print_path(ahc, scb);
				printf("SCB %d: Immediate reset.  "
					"Flags = 0x%x\n", scb->hscb->tag,
					scb->flags);
				goto bus_reset;
			}
		}
	}
	ahc_unlock(ahc, &s);
}

void
ahc_platform_set_tags(struct ahc_softc *ahc,
		      struct ahc_devinfo *devinfo, int enable)
{
	struct ahc_tmode_tstate *tstate;

	ahc_fetch_transinfo(ahc, devinfo->channel, devinfo->our_scsiid,
			    devinfo->target, &tstate);

	if (enable)
		tstate->tagenable |= devinfo->target_mask;
	else
		tstate->tagenable &= ~devinfo->target_mask;
}

int
ahc_platform_alloc(struct ahc_softc *ahc, void *platform_arg)
{
	if (sizeof(struct ahc_platform_data) == 0)
		return 0;
	ahc->platform_data = malloc(sizeof(struct ahc_platform_data), M_DEVBUF,
				    M_NOWAIT);
	if (ahc->platform_data == NULL)
		return (ENOMEM);
	return (0);
}

void
ahc_platform_free(struct ahc_softc *ahc)
{
	if (sizeof(struct ahc_platform_data) == 0)
		return;
	free(ahc->platform_data, M_DEVBUF);
}

int
ahc_softc_comp(struct ahc_softc *lahc, struct ahc_softc *rahc)
{
	return (0);
}

int
ahc_detach(struct ahc_softc *ahc, int flags)
{
	int rv = 0;

	ahc_intr_enable(ahc, FALSE);
	if (ahc->sc_child != NULL)
		rv = config_detach(ahc->sc_child, flags);
	if (rv == 0 && ahc->sc_child_b != NULL)
		rv = config_detach(ahc->sc_child_b, flags);

	pmf_device_deregister(ahc->sc_dev);
	ahc_free(ahc);

	return (rv);
}


void
ahc_send_async(struct ahc_softc *ahc, char channel, u_int target, u_int lun,
	       ac_code code, void *opt_arg)
{
	struct ahc_tmode_tstate *tstate;
	struct ahc_initiator_tinfo *tinfo;
	struct ahc_devinfo devinfo;
	struct scsipi_channel *chan;
	struct scsipi_xfer_mode xm;

	chan = channel == 'B' ? &ahc->sc_channel_b : &ahc->sc_channel;
	switch (code) {
	case AC_TRANSFER_NEG:
		tinfo = ahc_fetch_transinfo(ahc, channel, ahc->our_id, target,
			    &tstate);
		ahc_compile_devinfo(&devinfo, ahc->our_id, target, lun,
		    channel, ROLE_UNKNOWN);
		/*
		 * Don't bother if negotiating. XXX?
		 */
		if (tinfo->curr.period != tinfo->goal.period
		    || tinfo->curr.width != tinfo->goal.width
		    || tinfo->curr.offset != tinfo->goal.offset
		    || tinfo->curr.ppr_options != tinfo->goal.ppr_options)
			break;
		xm.xm_target = target;
		xm.xm_mode = 0;
		xm.xm_period = tinfo->curr.period;
		xm.xm_offset = tinfo->curr.offset;
		if (tinfo->curr.width == MSG_EXT_WDTR_BUS_16_BIT)
			xm.xm_mode |= PERIPH_CAP_WIDE16;
		if (tinfo->curr.period)
			xm.xm_mode |= PERIPH_CAP_SYNC;
		if (tstate->tagenable & devinfo.target_mask)
			xm.xm_mode |= PERIPH_CAP_TQING;
		if (tinfo->curr.ppr_options & MSG_EXT_PPR_DT_REQ)
			xm.xm_mode |= PERIPH_CAP_DT;
		scsipi_async_event(chan, ASYNC_EVENT_XFER_MODE, &xm);
		break;
	case AC_BUS_RESET:
		scsipi_async_event(chan, ASYNC_EVENT_RESET, NULL);
	case AC_SENT_BDR:
	default:
		break;
	}
}
