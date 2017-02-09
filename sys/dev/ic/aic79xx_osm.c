/*	$NetBSD: aic79xx_osm.c,v 1.32 2013/10/17 21:24:24 christos Exp $	*/

/*
 * Bus independent NetBSD shim for the aic7xxx based adaptec SCSI controllers
 *
 * Copyright (c) 1994-2002 Justin T. Gibbs.
 * Copyright (c) 2001-2002 Adaptec Inc.
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
 * //depot/aic7xxx/freebsd/dev/aic7xxx/aic79xx_osm.c#26 $
 *
 * $FreeBSD: src/sys/dev/aic7xxx/aic79xx_osm.c,v 1.11 2003/05/04 00:20:07 gibbs Exp $
 */
/*
 * Ported from FreeBSD by Pascal Renauld, Network Storage Solutions, Inc.
 * - April 2003
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: aic79xx_osm.c,v 1.32 2013/10/17 21:24:24 christos Exp $");

#include <dev/ic/aic79xx_osm.h>
#include <dev/ic/aic79xx_inline.h>

#ifndef AHD_TMODE_ENABLE
#define AHD_TMODE_ENABLE 0
#endif

static int	ahd_ioctl(struct scsipi_channel *channel, u_long cmd,
			  void *addr, int flag, struct proc *p);
static void	ahd_action(struct scsipi_channel *chan,
			   scsipi_adapter_req_t req, void *arg);
static void	ahd_execute_scb(void *arg, bus_dma_segment_t *dm_segs,
				int nsegments);
static int	ahd_poll(struct ahd_softc *ahd, int wait);
static void	ahd_setup_data(struct ahd_softc *ahd, struct scsipi_xfer *xs,
			       struct scb *scb);

#if NOT_YET
static void	ahd_set_recoveryscb(struct ahd_softc *ahd, struct scb *scb);
#endif

static bool	ahd_pmf_suspend(device_t, const pmf_qual_t *);
static bool	ahd_pmf_resume(device_t, const pmf_qual_t *);
static bool	ahd_pmf_shutdown(device_t, int);

/*
 * Attach all the sub-devices we can find
 */
int
ahd_attach(struct ahd_softc *ahd)
{
	int	s;
	char	ahd_info[256];

	ahd_controller_info(ahd, ahd_info, sizeof(ahd_info));
	printf("%s: %s\n", ahd_name(ahd), ahd_info);

	ahd_lock(ahd, &s);

	ahd->sc_adapter.adapt_dev = ahd->sc_dev;
	ahd->sc_adapter.adapt_nchannels = 1;

	ahd->sc_adapter.adapt_openings = ahd->scb_data.numscbs - 1;
	ahd->sc_adapter.adapt_max_periph = 32;

	ahd->sc_adapter.adapt_ioctl = ahd_ioctl;
	ahd->sc_adapter.adapt_minphys = ahd_minphys;
	ahd->sc_adapter.adapt_request = ahd_action;

	ahd->sc_channel.chan_adapter = &ahd->sc_adapter;
	ahd->sc_channel.chan_bustype = &scsi_bustype;
	ahd->sc_channel.chan_channel = 0;
	ahd->sc_channel.chan_ntargets = AHD_NUM_TARGETS;
	ahd->sc_channel.chan_nluns = 8 /*AHD_NUM_LUNS*/;
	ahd->sc_channel.chan_id = ahd->our_id;
	ahd->sc_channel.chan_flags |= SCSIPI_CHAN_CANGROW;

	ahd->sc_child = config_found(ahd->sc_dev, &ahd->sc_channel, scsiprint);

	ahd_intr_enable(ahd, TRUE);

	if (ahd->flags & AHD_RESET_BUS_A)
		ahd_reset_channel(ahd, 'A', TRUE);

	if (!pmf_device_register1(ahd->sc_dev,
	    ahd_pmf_suspend, ahd_pmf_resume, ahd_pmf_shutdown))
		aprint_error_dev(ahd->sc_dev,
		    "couldn't establish power handler\n");

	ahd_unlock(ahd, &s);

	return (1);
}

static bool
ahd_pmf_suspend(device_t dev, const pmf_qual_t *qual)
{
	struct ahd_softc *sc = device_private(dev);
#if 0
	return (ahd_suspend(sc) == 0);
#else
	ahd_shutdown(sc);
	return true;
#endif
}

static bool
ahd_pmf_resume(device_t dev, const pmf_qual_t *qual)
{
#if 0
	struct ahd_softc *sc = device_private(dev);

	return (ahd_resume(sc) == 0);
#else
	return true;
#endif
}

static bool
ahd_pmf_shutdown(device_t dev, int howto)
{
	struct ahd_softc *sc = device_private(dev);

	/* Disable all interrupt sources by resetting the controller */
	ahd_shutdown(sc);

	return true;
}

static int
ahd_ioctl(struct scsipi_channel *channel, u_long cmd,
	  void *addr, int flag, struct proc *p)
{
	struct ahd_softc *ahd;
	int s, ret = ENOTTY;

	ahd = device_private(channel->chan_adapter->adapt_dev);

	switch (cmd) {
	case SCBUSIORESET:
		s = splbio();
		ahd_reset_channel(ahd, channel->chan_channel == 1 ? 'B' : 'A', TRUE);
		splx(s);
		ret = 0;
		break;
	default:
		break;
	}

	return ret;
}

/*
 * Catch an interrupt from the adapter
 */
void
ahd_platform_intr(void *arg)
{
	struct	ahd_softc *ahd;

	ahd = arg;

	printf("%s; ahd_platform_intr\n", ahd_name(ahd));

	ahd_intr(ahd);
}

/*
 * We have an scb which has been processed by the
 * adaptor, now we look to see how the operation * went.
 */
void
ahd_done(struct ahd_softc *ahd, struct scb *scb)
{
	struct scsipi_xfer	*xs;
	struct scsipi_periph	*periph;
	int			s;

	LIST_REMOVE(scb, pending_links);

	xs = scb->xs;
	periph = xs->xs_periph;

	callout_stop(&scb->xs->xs_callout);

	if (xs->datalen) {
		int op;

		if (xs->xs_control & XS_CTL_DATA_IN)
			op = BUS_DMASYNC_POSTREAD;
		else
			op = BUS_DMASYNC_POSTWRITE;

		bus_dmamap_sync(ahd->parent_dmat, scb->dmamap, 0,
				scb->dmamap->dm_mapsize, op);
		bus_dmamap_unload(ahd->parent_dmat, scb->dmamap);
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
		LIST_FOREACH(list_scb, &ahd->pending_scbs, pending_links) {
			struct scsipi_xfer	*txs = list_scb->xs;

			if (!(txs->xs_control & XS_CTL_POLL)) {
				callout_reset(&txs->xs_callout,
				    (txs->timeout > 1000000) ?
				    (txs->timeout / 1000) * hz :
				    (txs->timeout * hz) / 1000,
				    ahd_timeout, list_scb);
			}
		}

		if (ahd_get_transaction_status(scb) != XS_NOERROR)
			ahd_set_transaction_status(scb, XS_TIMEOUT);
		scsipi_printaddr(xs->xs_periph);
		printf("%s: no longer in timeout, status = %x\n",
		       ahd_name(ahd), xs->status);
	}

	if (xs->error != XS_NOERROR) {
		/* Don't clobber any existing error state */
	} else if ((xs->status == SCSI_STATUS_BUSY) ||
		   (xs->status == SCSI_STATUS_QUEUE_FULL)) {
		ahd_set_transaction_status(scb, XS_BUSY);
		printf("%s: drive (ID %d, LUN %d) queue full (SCB 0x%x)\n",
		       ahd_name(ahd), SCB_GET_TARGET(ahd,scb), SCB_GET_LUN(scb), SCB_GET_TAG(scb));
	} else if ((scb->flags & SCB_SENSE) != 0) {
		/*
		 * We performed autosense retrieval.
		 *
		 * zero the sense data before having
		 * the drive fill it.  The SCSI spec mandates
		 * that any untransferred data should be
		 * assumed to be zero.  Complete the 'bounce'
		 * of sense information through buffers accessible
		 * via bus-space by copying it into the clients
		 * csio.
		 */
		memset(&xs->sense.scsi_sense, 0, sizeof(xs->sense.scsi_sense));
		memcpy(&xs->sense.scsi_sense, ahd_get_sense_buf(ahd, scb),
		       sizeof(struct scsi_sense_data));

		ahd_set_transaction_status(scb, XS_SENSE);
	} else if ((scb->flags & SCB_PKT_SENSE) != 0) {
		struct scsi_status_iu_header *siu;
		u_int sense_len;
#ifdef AHD_DEBUG
		int i;
#endif
		/*
		 * Copy only the sense data into the provided buffer.
		 */
		siu = (struct scsi_status_iu_header *)scb->sense_data;
		sense_len = MIN(scsi_4btoul(siu->sense_length),
				sizeof(xs->sense.scsi_sense));
		memset(&xs->sense.scsi_sense, 0, sizeof(xs->sense.scsi_sense));
		memcpy(&xs->sense.scsi_sense,
		       scb->sense_data + SIU_SENSE_OFFSET(siu), sense_len);
#ifdef AHD_DEBUG
		printf("Copied %d bytes of sense data offset %d:", sense_len,
		       SIU_SENSE_OFFSET(siu));
		for (i = 0; i < sense_len; i++)
			printf(" 0x%x", ((uint8_t *)&xs->sense.scsi_sense)[i]);
		printf("\n");
#endif
		ahd_set_transaction_status(scb, XS_SENSE);
	}

	if (scb->flags & SCB_FREEZE_QUEUE) {
		scsipi_periph_thaw(periph, 1);
		scb->flags &= ~SCB_FREEZE_QUEUE;
	}

	if (scb->flags & SCB_REQUEUE)
		ahd_set_transaction_status(scb, XS_REQUEUE);

	ahd_lock(ahd, &s);
	ahd_free_scb(ahd, scb);
	ahd_unlock(ahd, &s);

	scsipi_done(xs);
}

static void
ahd_action(struct scsipi_channel *chan, scsipi_adapter_req_t req, void *arg)
{
	struct ahd_softc *ahd;
	struct ahd_initiator_tinfo *tinfo;
	struct ahd_tmode_tstate *tstate;

	ahd = device_private(chan->chan_adapter->adapt_dev);

	switch(req) {

	case ADAPTER_REQ_RUN_XFER:
	  {
		struct scsipi_xfer *xs;
		struct scsipi_periph *periph;
		struct scb *scb;
		struct hardware_scb *hscb;
		u_int target_id;
		u_int our_id;
		u_int col_idx;
		char channel;
		int s;

		xs = arg;
		periph = xs->xs_periph;

		SC_DEBUG(periph, SCSIPI_DB3, ("ahd_action\n"));

		target_id = periph->periph_target;
		our_id = ahd->our_id;
		channel = (chan->chan_channel == 1) ? 'B' : 'A';

		/*
		 * get an scb to use.
		 */
		ahd_lock(ahd, &s);
		tinfo = ahd_fetch_transinfo(ahd, channel, our_id,
					    target_id, &tstate);

		if (xs->xs_tag_type != 0 ||
		    (tinfo->curr.ppr_options & MSG_EXT_PPR_IU_REQ) != 0)
			col_idx = AHD_NEVER_COL_IDX;
		else
			col_idx = AHD_BUILD_COL_IDX(target_id,
			    periph->periph_lun);

		if ((scb = ahd_get_scb(ahd, col_idx)) == NULL) {
			xs->error = XS_RESOURCE_SHORTAGE;
			ahd_unlock(ahd, &s);
			scsipi_done(xs);
			return;
		}
		ahd_unlock(ahd, &s);

		hscb = scb->hscb;

		SC_DEBUG(periph, SCSIPI_DB3, ("start scb(%p)\n", scb));
		scb->xs = xs;

		/*
		 * Put all the arguments for the xfer in the scb
		 */
		hscb->control = 0;
		hscb->scsiid = BUILD_SCSIID(ahd, sim, target_id, our_id);
		hscb->lun = periph->periph_lun;
		if (xs->xs_control & XS_CTL_RESET) {
			hscb->cdb_len = 0;
			scb->flags |= SCB_DEVICE_RESET;
			hscb->control |= MK_MESSAGE;
			hscb->task_management = SIU_TASKMGMT_LUN_RESET;
			ahd_execute_scb(scb, NULL, 0);
		} else {
			hscb->task_management = 0;
		}

		ahd_setup_data(ahd, xs, scb);
		break;
	  }

	case ADAPTER_REQ_GROW_RESOURCES:
#ifdef AHC_DEBUG
		printf("%s: ADAPTER_REQ_GROW_RESOURCES\n", ahd_name(ahd));
#endif
		chan->chan_adapter->adapt_openings += ahd_alloc_scbs(ahd);
		if (ahd->scb_data.numscbs >= AHD_SCB_MAX_ALLOC)
			chan->chan_flags &= ~SCSIPI_CHAN_CANGROW;
		break;

	case ADAPTER_REQ_SET_XFER_MODE:
	    {
		struct scsipi_xfer_mode *xm = arg;
		struct ahd_devinfo devinfo;
		int target_id, our_id, first;
		u_int width;
		int s;
		char channel;
		u_int ppr_options = 0, period, offset;
		uint16_t old_autoneg;

		target_id = xm->xm_target;
		our_id = chan->chan_id;
		channel = 'A';
		s = splbio();
		tinfo = ahd_fetch_transinfo(ahd, channel, our_id, target_id,
		    &tstate);
		ahd_compile_devinfo(&devinfo, our_id, target_id,
		    0, channel, ROLE_INITIATOR);

		old_autoneg = tstate->auto_negotiate;

		/*
		 * XXX since the period and offset are not provided here,
		 * fake things by forcing a renegotiation using the user
		 * settings if this is called for the first time (i.e.
		 * during probe). Also, cap various values at the user
		 * values, assuming that the user set it up that way.
		 */
		if (ahd->inited_target[target_id] == 0) {
			period = tinfo->user.period;
			offset = tinfo->user.offset;
			ppr_options = tinfo->user.ppr_options;
			width = tinfo->user.width;
			tstate->tagenable |=
			    (ahd->user_tagenable & devinfo.target_mask);
			tstate->discenable |=
			    (ahd->user_discenable & devinfo.target_mask);
			ahd->inited_target[target_id] = 1;
			first = 1;
		} else
			first = 0;

		if (xm->xm_mode & (PERIPH_CAP_WIDE16 | PERIPH_CAP_DT))
			width = MSG_EXT_WDTR_BUS_16_BIT;
		else
			width = MSG_EXT_WDTR_BUS_8_BIT;

		ahd_validate_width(ahd, NULL, &width, ROLE_UNKNOWN);
		if (width > tinfo->user.width)
			width = tinfo->user.width;
		ahd_set_width(ahd, &devinfo, width, AHD_TRANS_GOAL, FALSE);

		if (!(xm->xm_mode & (PERIPH_CAP_SYNC | PERIPH_CAP_DT))) {
			period = 0;
			offset = 0;
			ppr_options = 0;
		}

		if ((xm->xm_mode & PERIPH_CAP_DT) &&
		    (tinfo->user.ppr_options & MSG_EXT_PPR_DT_REQ))
			ppr_options |= MSG_EXT_PPR_DT_REQ;
		else
			ppr_options &= ~MSG_EXT_PPR_DT_REQ;

		if ((tstate->discenable & devinfo.target_mask) == 0 ||
		    (tstate->tagenable & devinfo.target_mask) == 0)
			ppr_options &= ~MSG_EXT_PPR_IU_REQ;

		if ((xm->xm_mode & PERIPH_CAP_TQING) &&
		    (ahd->user_tagenable & devinfo.target_mask))
			tstate->tagenable |= devinfo.target_mask;
		else
			tstate->tagenable &= ~devinfo.target_mask;

		ahd_find_syncrate(ahd, &period, &ppr_options, AHD_SYNCRATE_MAX);
		ahd_validate_offset(ahd, NULL, period, &offset,
		    MSG_EXT_WDTR_BUS_8_BIT, ROLE_UNKNOWN);
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

		ahd_set_syncrate(ahd, &devinfo, period, offset,
		    ppr_options, AHD_TRANS_GOAL, FALSE);

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
ahd_execute_scb(void *arg, bus_dma_segment_t *dm_segs, int nsegments)
{
	struct scb *scb;
	struct scsipi_xfer *xs;
	struct ahd_softc *ahd;
	struct ahd_initiator_tinfo *tinfo;
	struct ahd_tmode_tstate *tstate;
	u_int  mask;
	int    s;

	scb = arg;
	xs = scb->xs;
	xs->error = 0;
	xs->status = 0;
	xs->xs_status = 0;
	ahd = device_private(
	    xs->xs_periph->periph_channel->chan_adapter->adapt_dev);

	scb->sg_count = 0;
	if (nsegments != 0) {
		void *sg;
		int op;
		u_int i;

		ahd_setup_data_scb(ahd, scb);

		/* Copy the segments into our SG list */
		for (i = nsegments, sg = scb->sg_list; i > 0; i--) {

			sg = ahd_sg_setup(ahd, scb, sg, dm_segs->ds_addr,
					  dm_segs->ds_len,
					  /*last*/i == 1);
			dm_segs++;
		}

		if (xs->xs_control & XS_CTL_DATA_IN)
			op = BUS_DMASYNC_PREREAD;
		else
			op = BUS_DMASYNC_PREWRITE;

		bus_dmamap_sync(ahd->parent_dmat, scb->dmamap, 0,
				scb->dmamap->dm_mapsize, op);
	}

	ahd_lock(ahd, &s);

	/*
	 * Last time we need to check if this SCB needs to
	 * be aborted.
	 */
	if (ahd_get_scsi_status(scb) == XS_STS_DONE) {
		if (nsegments != 0)
			bus_dmamap_unload(ahd->parent_dmat,
					  scb->dmamap);
		ahd_free_scb(ahd, scb);
		ahd_unlock(ahd, &s);
		return;
	}

	tinfo = ahd_fetch_transinfo(ahd, SCSIID_CHANNEL(ahd, scb->hscb->scsiid),
				    SCSIID_OUR_ID(scb->hscb->scsiid),
				    SCSIID_TARGET(ahd, scb->hscb->scsiid),
				    &tstate);

	mask = SCB_GET_TARGET_MASK(ahd, scb);

	if ((tstate->discenable & mask) != 0)
		scb->hscb->control |= DISCENB;

	if ((tstate->tagenable & mask) != 0)
		scb->hscb->control |= xs->xs_tag_type|TAG_ENB;

	if ((tinfo->curr.ppr_options & MSG_EXT_PPR_IU) != 0) {
		scb->flags |= SCB_PACKETIZED;
		if (scb->hscb->task_management != 0)
			scb->hscb->control &= ~MK_MESSAGE;
	}

#if 0	/* This looks like it makes sense at first, but it can loop */
	if ((xs->xs_control & XS_CTL_DISCOVERY) &&
	    (tinfo->goal.width != 0
	     || tinfo->goal.period != 0
	     || tinfo->goal.ppr_options != 0)) {
		scb->flags |= SCB_NEGOTIATE;
		scb->hscb->control |= MK_MESSAGE;
	} else
#endif
	if ((tstate->auto_negotiate & mask) != 0) {
		scb->flags |= SCB_AUTO_NEGOTIATE;
		scb->hscb->control |= MK_MESSAGE;
	}

	LIST_INSERT_HEAD(&ahd->pending_scbs, scb, pending_links);

	scb->flags |= SCB_ACTIVE;

	if (!(xs->xs_control & XS_CTL_POLL)) {
		callout_reset(&scb->xs->xs_callout, xs->timeout > 1000000 ?
			      (xs->timeout / 1000) * hz : (xs->timeout * hz) / 1000,
			      ahd_timeout, scb);
	}

	if ((scb->flags & SCB_TARGET_IMMEDIATE) != 0) {
		/* Define a mapping from our tag to the SCB. */
		ahd->scb_data.scbindex[SCB_GET_TAG(scb)] = scb;
		ahd_pause(ahd);
		ahd_set_scbptr(ahd, SCB_GET_TAG(scb));
		ahd_outb(ahd, RETURN_1, CONT_MSG_LOOP_TARG);
		ahd_unpause(ahd);
	} else {
		ahd_queue_scb(ahd, scb);
	}

	if (!(xs->xs_control & XS_CTL_POLL)) {
		ahd_unlock(ahd, &s);
		return;
	}
	/*
	 * If we can't use interrupts, poll for completion
	 */
	SC_DEBUG(xs->xs_periph, SCSIPI_DB3, ("cmd_poll\n"));
	do {
		if (ahd_poll(ahd, xs->timeout)) {
			if (!(xs->xs_control & XS_CTL_SILENT))
				printf("cmd fail\n");
			ahd_timeout(scb);
			break;
		}
	} while (!(xs->xs_status & XS_STS_DONE));

	ahd_unlock(ahd, &s);
}

static int
ahd_poll(struct ahd_softc *ahd, int wait)
{

	while (--wait) {
		DELAY(1000);
		if (ahd_inb(ahd, INTSTAT) & INT_PEND)
			break;
	}

	if (wait == 0) {
		printf("%s: board is not responding\n", ahd_name(ahd));
		return (EIO);
	}

	ahd_intr(ahd);
	return (0);
}


static void
ahd_setup_data(struct ahd_softc *ahd, struct scsipi_xfer *xs,
	       struct scb *scb)
{
	struct hardware_scb *hscb;

	hscb = scb->hscb;
	xs->resid = xs->status = 0;

	hscb->cdb_len = xs->cmdlen;
	if (hscb->cdb_len > MAX_CDB_LEN) {
		int s;
		/*
		 * Should CAM start to support CDB sizes
		 * greater than 16 bytes, we could use
		 * the sense buffer to store the CDB.
		 */
		ahd_set_transaction_status(scb,
					   XS_DRIVER_STUFFUP);

		ahd_lock(ahd, &s);
		ahd_free_scb(ahd, scb);
		ahd_unlock(ahd, &s);
		scsipi_done(xs);
	}
	memcpy(hscb->shared_data.idata.cdb, xs->cmd, hscb->cdb_len);

	/* Only use S/G if there is a transfer */
	if (xs->datalen) {
		int error;

		error = bus_dmamap_load(ahd->parent_dmat,
					scb->dmamap, xs->data,
					xs->datalen, NULL,
					((xs->xs_control & XS_CTL_NOSLEEP) ?
					 BUS_DMA_NOWAIT : BUS_DMA_WAITOK) |
					BUS_DMA_STREAMING |
					((xs->xs_control & XS_CTL_DATA_IN) ?
					 BUS_DMA_READ : BUS_DMA_WRITE));
		if (error) {
#ifdef AHD_DEBUG
			printf("%s: in ahd_setup_data(): bus_dmamap_load() "
			       "= %d\n",
			       ahd_name(ahd), error);
#endif
			xs->error = XS_RESOURCE_SHORTAGE;
			scsipi_done(xs);
			return;
		}
		ahd_execute_scb(scb,
				scb->dmamap->dm_segs,
				scb->dmamap->dm_nsegs);
	} else {
		ahd_execute_scb(scb, NULL, 0);
	}
}

void
ahd_timeout(void *arg)
{
	struct	scb	  *scb;
	struct	ahd_softc *ahd;
	int		   s;

	scb = arg;
	ahd = scb->ahd_softc;

	printf("%s: ahd_timeout\n", ahd_name(ahd));

	ahd_lock(ahd, &s);

	ahd_pause_and_flushwork(ahd);
	(void)ahd_save_modes(ahd);
#if 0
	ahd_set_modes(ahd, AHD_MODE_SCSI, AHD_MODE_SCSI);
	ahd_outb(ahd, SCSISIGO, ACKO);
	printf("set ACK\n");
	ahd_outb(ahd, SCSISIGO, 0);
	printf("clearing Ack\n");
	ahd_restore_modes(ahd, saved_modes);
#endif
	if ((scb->flags & SCB_ACTIVE) == 0) {
		/* Previous timeout took care of me already */
		printf("%s: Timedout SCB already complete. "
		       "Interrupts may not be functioning.\n", ahd_name(ahd));
		ahd_unpause(ahd);
		ahd_unlock(ahd, &s);
		return;
	}

	ahd_print_path(ahd, scb);
	printf("SCB 0x%x - timed out\n", SCB_GET_TAG(scb));
	ahd_dump_card_state(ahd);
	ahd_reset_channel(ahd, SIM_CHANNEL(ahd, sim),
			  /*initiate reset*/TRUE);
	ahd_unlock(ahd, &s);
	return;
}

int
ahd_platform_alloc(struct ahd_softc *ahd, void *platform_arg)
{
	ahd->platform_data = malloc(sizeof(struct ahd_platform_data), M_DEVBUF,
				    M_NOWAIT /*| M_ZERO*/);
	if (ahd->platform_data == NULL)
		return (ENOMEM);

	memset(ahd->platform_data, 0, sizeof(struct ahd_platform_data));

	return (0);
}

void
ahd_platform_free(struct ahd_softc *ahd)
{
	free(ahd->platform_data, M_DEVBUF);
}

int
ahd_softc_comp(struct ahd_softc *lahd, struct ahd_softc *rahd)
{
	/* We don't sort softcs under NetBSD so report equal always */
	return (0);
}

int
ahd_detach(struct ahd_softc *ahd, int flags)
{
	int rv = 0;

	if (ahd->sc_child != NULL)
		rv = config_detach(ahd->sc_child, flags);

	pmf_device_deregister(ahd->sc_dev);

	ahd_free(ahd);

	return rv;
}

void
ahd_platform_set_tags(struct ahd_softc *ahd,
		      struct ahd_devinfo *devinfo, ahd_queue_alg alg)
{
	struct ahd_tmode_tstate *tstate;

	ahd_fetch_transinfo(ahd, devinfo->channel, devinfo->our_scsiid,
			    devinfo->target, &tstate);

	if (alg != AHD_QUEUE_NONE)
		tstate->tagenable |= devinfo->target_mask;
	else
		tstate->tagenable &= ~devinfo->target_mask;
}

void
ahd_send_async(struct ahd_softc *ahd, char channel, u_int target, u_int lun,
	       ac_code code, void *opt_arg)
{
	struct ahd_tmode_tstate *tstate;
	struct ahd_initiator_tinfo *tinfo;
	struct ahd_devinfo devinfo;
	struct scsipi_channel *chan;
	struct scsipi_xfer_mode xm;

#ifdef DIAGNOSTIC
	if (channel != 'A')
		panic("ahd_send_async: not channel A");
#endif
	chan = &ahd->sc_channel;
	switch (code) {
	case AC_TRANSFER_NEG:
		tinfo = ahd_fetch_transinfo(ahd, channel, ahd->our_id, target,
			    &tstate);
		ahd_compile_devinfo(&devinfo, ahd->our_id, target, lun,
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
		if (tinfo->goal.ppr_options & MSG_EXT_PPR_DT_REQ)
			xm.xm_mode |= PERIPH_CAP_DT;
		if (tinfo->curr.width == MSG_EXT_WDTR_BUS_16_BIT)
			xm.xm_mode |= PERIPH_CAP_WIDE16;
		if (tinfo->curr.period)
			xm.xm_mode |= PERIPH_CAP_SYNC;
		if (tstate->tagenable & devinfo.target_mask)
			xm.xm_mode |= PERIPH_CAP_TQING;
		scsipi_async_event(chan, ASYNC_EVENT_XFER_MODE, &xm);
		break;
	case AC_BUS_RESET:
		scsipi_async_event(chan, ASYNC_EVENT_RESET, NULL);
	case AC_SENT_BDR:
	default:
		break;
	}
}
