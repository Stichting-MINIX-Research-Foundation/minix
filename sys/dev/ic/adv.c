/*	$NetBSD: adv.c,v 1.46 2012/10/27 17:18:18 chs Exp $	*/

/*
 * Generic driver for the Advanced Systems Inc. Narrow SCSI controllers
 *
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Author: Baldassare Dante Profeta <dante@mclink.it>
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
__KERNEL_RCSID(0, "$NetBSD: adv.c,v 1.46 2012/10/27 17:18:18 chs Exp $");

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

#include <dev/ic/advlib.h>
#include <dev/ic/adv.h>

#ifndef DDB
#define	Debugger()	panic("should call debugger here (adv.c)")
#endif /* ! DDB */


/* #define ASC_DEBUG */

/******************************************************************************/


static int adv_alloc_control_data(ASC_SOFTC *);
static void adv_free_control_data(ASC_SOFTC *);
static int adv_create_ccbs(ASC_SOFTC *, ADV_CCB *, int);
static void adv_free_ccb(ASC_SOFTC *, ADV_CCB *);
static void adv_reset_ccb(ADV_CCB *);
static int adv_init_ccb(ASC_SOFTC *, ADV_CCB *);
static ADV_CCB *adv_get_ccb(ASC_SOFTC *);
static void adv_queue_ccb(ASC_SOFTC *, ADV_CCB *);
static void adv_start_ccbs(ASC_SOFTC *);


static void adv_scsipi_request(struct scsipi_channel *,
	scsipi_adapter_req_t, void *);
static void advminphys(struct buf *);
static void adv_narrow_isr_callback(ASC_SOFTC *, ASC_QDONE_INFO *);

static int adv_poll(ASC_SOFTC *, struct scsipi_xfer *, int);
static void adv_timeout(void *);
static void adv_watchdog(void *);


/******************************************************************************/

#define ADV_ABORT_TIMEOUT       2000	/* time to wait for abort (mSec) */
#define ADV_WATCH_TIMEOUT       1000	/* time to wait for watchdog (mSec) */

/******************************************************************************/
/*                             Control Blocks routines                        */
/******************************************************************************/


static int
adv_alloc_control_data(ASC_SOFTC *sc)
{
	int error;

	/*
 	* Allocate the control blocks.
	 */
	if ((error = bus_dmamem_alloc(sc->sc_dmat, sizeof(struct adv_control),
			   PAGE_SIZE, 0, &sc->sc_control_seg, 1,
			   &sc->sc_control_nsegs, BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to allocate control structures,"
		       " error = %d\n", error);
		return (error);
	}
	if ((error = bus_dmamem_map(sc->sc_dmat, &sc->sc_control_seg,
			   sc->sc_control_nsegs, sizeof(struct adv_control),
			   (void **) & sc->sc_control,
			   BUS_DMA_NOWAIT | BUS_DMA_COHERENT)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to map control structures, error = %d\n",
		       error);
		return (error);
	}
	/*
	 * Create and load the DMA map used for the control blocks.
	 */
	if ((error = bus_dmamap_create(sc->sc_dmat, sizeof(struct adv_control),
			   1, sizeof(struct adv_control), 0, BUS_DMA_NOWAIT,
				       &sc->sc_dmamap_control)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to create control DMA map, error = %d\n",
		       error);
		return (error);
	}
	if ((error = bus_dmamap_load(sc->sc_dmat, sc->sc_dmamap_control,
			   sc->sc_control, sizeof(struct adv_control), NULL,
				     BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to load control DMA map, error = %d\n",
		       error);
		return (error);
	}

	/*
	 * Initialize the overrun_buf address.
	 */
	sc->overrun_buf = sc->sc_dmamap_control->dm_segs[0].ds_addr +
	    offsetof(struct adv_control, overrun_buf);

	return (0);
}

static void
adv_free_control_data(ASC_SOFTC *sc)
{

	bus_dmamap_unload(sc->sc_dmat, sc->sc_dmamap_control);
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_dmamap_control);
	sc->sc_dmamap_control = NULL;

	bus_dmamem_unmap(sc->sc_dmat, (void *) sc->sc_control,
	    sizeof(struct adv_control));
	bus_dmamem_free(sc->sc_dmat, &sc->sc_control_seg,
	    sc->sc_control_nsegs);
}

/*
 * Create a set of ccbs and add them to the free list.  Called once
 * by adv_init().  We return the number of CCBs successfully created.
 */
static int
adv_create_ccbs(ASC_SOFTC *sc, ADV_CCB *ccbstore, int count)
{
	ADV_CCB        *ccb;
	int             i, error;

	memset(ccbstore, 0, sizeof(ADV_CCB) * count);
	for (i = 0; i < count; i++) {
		ccb = &ccbstore[i];
		if ((error = adv_init_ccb(sc, ccb)) != 0) {
			aprint_error_dev(sc->sc_dev, "unable to initialize ccb, error = %d\n",
			       error);
			return (i);
		}
		TAILQ_INSERT_TAIL(&sc->sc_free_ccb, ccb, chain);
	}

	return (i);
}


/*
 * A ccb is put onto the free list.
 */
static void
adv_free_ccb(ASC_SOFTC *sc, ADV_CCB *ccb)
{
	int             s;

	s = splbio();
	adv_reset_ccb(ccb);
	TAILQ_INSERT_HEAD(&sc->sc_free_ccb, ccb, chain);
	splx(s);
}


static void
adv_reset_ccb(ADV_CCB *ccb)
{

	ccb->flags = 0;
}


static int
adv_init_ccb(ASC_SOFTC *sc, ADV_CCB *ccb)
{
	int	hashnum, error;

	callout_init(&ccb->ccb_watchdog, 0);

	/*
	 * Create the DMA map for this CCB.
	 */
	error = bus_dmamap_create(sc->sc_dmat,
				  (ASC_MAX_SG_LIST - 1) * PAGE_SIZE,
			 ASC_MAX_SG_LIST, (ASC_MAX_SG_LIST - 1) * PAGE_SIZE,
		   0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &ccb->dmamap_xfer);
	if (error) {
		aprint_error_dev(sc->sc_dev, "unable to create DMA map, error = %d\n",
		       error);
		return (error);
	}

	/*
	 * put in the phystokv hash table
	 * Never gets taken out.
	 */
	ccb->hashkey = sc->sc_dmamap_control->dm_segs[0].ds_addr +
	    ADV_CCB_OFF(ccb);
	hashnum = CCB_HASH(ccb->hashkey);
	ccb->nexthash = sc->sc_ccbhash[hashnum];
	sc->sc_ccbhash[hashnum] = ccb;

	adv_reset_ccb(ccb);
	return (0);
}


/*
 * Get a free ccb
 *
 * If there are none, see if we can allocate a new one
 */
static ADV_CCB *
adv_get_ccb(ASC_SOFTC *sc)
{
	ADV_CCB        *ccb = 0;
	int             s;

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
ADV_CCB *
adv_ccb_phys_kv(ASC_SOFTC *sc, u_long ccb_phys)
{
	int hashnum = CCB_HASH(ccb_phys);
	ADV_CCB *ccb = sc->sc_ccbhash[hashnum];

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
adv_queue_ccb(ASC_SOFTC *sc, ADV_CCB *ccb)
{

	TAILQ_INSERT_TAIL(&sc->sc_waiting_ccb, ccb, chain);

	adv_start_ccbs(sc);
}


static void
adv_start_ccbs(ASC_SOFTC *sc)
{
	ADV_CCB        *ccb;

	while ((ccb = sc->sc_waiting_ccb.tqh_first) != NULL) {
		if (ccb->flags & CCB_WATCHDOG)
			callout_stop(&ccb->ccb_watchdog);

		if (AscExeScsiQueue(sc, &ccb->scsiq) == ASC_BUSY) {
			ccb->flags |= CCB_WATCHDOG;
			callout_reset(&ccb->ccb_watchdog,
			    (ADV_WATCH_TIMEOUT * hz) / 1000,
			    adv_watchdog, ccb);
			break;
		}
		TAILQ_REMOVE(&sc->sc_waiting_ccb, ccb, chain);

		if ((ccb->xs->xs_control & XS_CTL_POLL) == 0)
			callout_reset(&ccb->xs->xs_callout,
			    mstohz(ccb->timeout), adv_timeout, ccb);
	}
}


/******************************************************************************/
/*                         SCSI layer interfacing routines                    */
/******************************************************************************/


int
adv_init(ASC_SOFTC *sc)
{
	int             warn;

	if (!AscFindSignature(sc->sc_iot, sc->sc_ioh)) {
		aprint_error("adv_init: failed to find signature\n");
		return (1);
	}

	/*
	 * Read the board configuration
	 */
	AscInitASC_SOFTC(sc);
	warn = AscInitFromEEP(sc);
	if (warn) {
		aprint_error_dev(sc->sc_dev, "-get: ");
		switch (warn) {
		case -1:
			aprint_normal("Chip is not halted\n");
			break;

		case -2:
			aprint_normal("Couldn't get MicroCode Start"
			       " address\n");
			break;

		case ASC_WARN_IO_PORT_ROTATE:
			aprint_normal("I/O port address modified\n");
			break;

		case ASC_WARN_AUTO_CONFIG:
			aprint_normal("I/O port increment switch enabled\n");
			break;

		case ASC_WARN_EEPROM_CHKSUM:
			aprint_normal("EEPROM checksum error\n");
			break;

		case ASC_WARN_IRQ_MODIFIED:
			aprint_normal("IRQ modified\n");
			break;

		case ASC_WARN_CMD_QNG_CONFLICT:
			aprint_normal("tag queuing enabled w/o disconnects\n");
			break;

		default:
			aprint_normal("unknown warning %d\n", warn);
		}
	}
	if (sc->scsi_reset_wait > ASC_MAX_SCSI_RESET_WAIT)
		sc->scsi_reset_wait = ASC_MAX_SCSI_RESET_WAIT;

	/*
	 * Modify the board configuration
	 */
	warn = AscInitFromASC_SOFTC(sc);
	if (warn) {
		aprint_error_dev(sc->sc_dev, "-set: ");
		switch (warn) {
		case ASC_WARN_CMD_QNG_CONFLICT:
			aprint_normal("tag queuing enabled w/o disconnects\n");
			break;

		case ASC_WARN_AUTO_CONFIG:
			aprint_normal("I/O port increment switch enabled\n");
			break;

		default:
			aprint_normal("unknown warning %d\n", warn);
		}
	}
	sc->isr_callback = (ASC_CALLBACK) adv_narrow_isr_callback;

	return (0);
}


void
adv_attach(ASC_SOFTC *sc)
{
	struct scsipi_adapter *adapt = &sc->sc_adapter;
	struct scsipi_channel *chan = &sc->sc_channel;
	int             i, error;

	/*
	 * Initialize board RISC chip and enable interrupts.
	 */
	switch (AscInitDriver(sc)) {
	case 0:
		/* AllOK */
		break;

	case 1:
		panic("%s: bad signature", device_xname(sc->sc_dev));
		break;

	case 2:
		panic("%s: unable to load MicroCode",
		      device_xname(sc->sc_dev));
		break;

	case 3:
		panic("%s: unable to initialize MicroCode",
		      device_xname(sc->sc_dev));
		break;

	default:
		panic("%s: unable to initialize board RISC chip",
		      device_xname(sc->sc_dev));
	}

	/*
	 * Fill in the scsipi_adapter.
	 */
	memset(adapt, 0, sizeof(*adapt));
	adapt->adapt_dev = sc->sc_dev;
	adapt->adapt_nchannels = 1;
	/* adapt_openings initialized below */
	/* adapt_max_periph initialized below */
	adapt->adapt_request = adv_scsipi_request;
	adapt->adapt_minphys = advminphys;

	/*
	 * Fill in the scsipi_channel.
	 */
	memset(chan, 0, sizeof(*chan));
	chan->chan_adapter = adapt;
	chan->chan_bustype = &scsi_bustype;
	chan->chan_channel = 0;
	chan->chan_ntargets = 8;
	chan->chan_nluns = 8;
	chan->chan_id = sc->chip_scsi_id;

	TAILQ_INIT(&sc->sc_free_ccb);
	TAILQ_INIT(&sc->sc_waiting_ccb);

	/*
	 * Allocate the Control Blocks and the overrun buffer.
	 */
	error = adv_alloc_control_data(sc);
	if (error)
		return; /* (error) */

	/*
	 * Create and initialize the Control Blocks.
	 */
	i = adv_create_ccbs(sc, sc->sc_control->ccbs, ADV_MAX_CCB);
	if (i == 0) {
		aprint_error_dev(sc->sc_dev, "unable to create control blocks\n");
		return; /* (ENOMEM) */ ;
	} else if (i != ADV_MAX_CCB) {
		aprint_error_dev(sc->sc_dev, 
		    "WARNING: only %d of %d control blocks created\n",
		    i, ADV_MAX_CCB);
	}

	adapt->adapt_openings = i;
	adapt->adapt_max_periph = adapt->adapt_openings;

	sc->sc_child = config_found(sc->sc_dev, chan, scsiprint);
}

int
adv_detach(ASC_SOFTC *sc, int flags)
{
	int rv = 0;

	if (sc->sc_child != NULL)
		rv = config_detach(sc->sc_child, flags);

	adv_free_control_data(sc);

	return (rv);
}

static void
advminphys(struct buf *bp)
{

	if (bp->b_bcount > ((ASC_MAX_SG_LIST - 1) * PAGE_SIZE))
		bp->b_bcount = ((ASC_MAX_SG_LIST - 1) * PAGE_SIZE);
	minphys(bp);
}


/*
 * start a scsi operation given the command and the data address.  Also needs
 * the unit, target and lu.
 */

static void
adv_scsipi_request(struct scsipi_channel *chan, scsipi_adapter_req_t req, void *arg)
{
 	struct scsipi_xfer *xs;
 	struct scsipi_periph *periph;
 	ASC_SOFTC      *sc = device_private(chan->chan_adapter->adapt_dev);
 	bus_dma_tag_t   dmat = sc->sc_dmat;
 	ADV_CCB        *ccb;
 	int             s, flags, error, nsegs;

 	switch (req) {
 	case ADAPTER_REQ_RUN_XFER:
 		xs = arg;
 		periph = xs->xs_periph;
 		flags = xs->xs_control;

 		/*
 		 * Get a CCB to use.
 		 */
 		ccb = adv_get_ccb(sc);
#ifdef DIAGNOSTIC
 		/*
 		 * This should never happen as we track the resources
 		 * in the mid-layer.
 		 */
 		if (ccb == NULL) {
 			scsipi_printaddr(periph);
 			printf("unable to allocate ccb\n");
 			panic("adv_scsipi_request");
 		}
#endif

 		ccb->xs = xs;
 		ccb->timeout = xs->timeout;

 		/*
 		 * Build up the request
 		 */
 		memset(&ccb->scsiq, 0, sizeof(ASC_SCSI_Q));

 		ccb->scsiq.q2.ccb_ptr =
 		    sc->sc_dmamap_control->dm_segs[0].ds_addr +
 		    ADV_CCB_OFF(ccb);

 		ccb->scsiq.cdbptr = &xs->cmd->opcode;
 		ccb->scsiq.q2.cdb_len = xs->cmdlen;
 		ccb->scsiq.q1.target_id =
 		    ASC_TID_TO_TARGET_ID(periph->periph_target);
 		ccb->scsiq.q1.target_lun = periph->periph_lun;
 		ccb->scsiq.q2.target_ix =
 		    ASC_TIDLUN_TO_IX(periph->periph_target,
 		    periph->periph_lun);
 		ccb->scsiq.q1.sense_addr =
 		    sc->sc_dmamap_control->dm_segs[0].ds_addr +
 		    ADV_CCB_OFF(ccb) + offsetof(struct adv_ccb, scsi_sense);
 		ccb->scsiq.q1.sense_len = sizeof(struct scsi_sense_data);

 		/*
 		 * If there are any outstanding requests for the current
 		 * target, then every 255th request send an ORDERED request.
 		 * This heuristic tries to retain the benefit of request
 		 * sorting while preventing request starvation. 255 is the
 		 * max number of tags or pending commands a device may have
 		 * outstanding.
 		 */
 		sc->reqcnt[periph->periph_target]++;
 		if (((sc->reqcnt[periph->periph_target] > 0) &&
 		    (sc->reqcnt[periph->periph_target] % 255) == 0) ||
		    xs->bp == NULL || (xs->bp->b_flags & B_ASYNC) == 0) {
 			ccb->scsiq.q2.tag_code = M2_QTAG_MSG_ORDERED;
 		} else {
 			ccb->scsiq.q2.tag_code = M2_QTAG_MSG_SIMPLE;
 		}

 		if (xs->datalen) {
 			/*
 			 * Map the DMA transfer.
 			 */
#ifdef TFS
 			if (flags & SCSI_DATA_UIO) {
 				error = bus_dmamap_load_uio(dmat,
 				    ccb->dmamap_xfer, (struct uio *) xs->data,
				    ((flags & XS_CTL_NOSLEEP) ? BUS_DMA_NOWAIT :
				     BUS_DMA_WAITOK) | BUS_DMA_STREAMING |
				     ((flags & XS_CTL_DATA_IN) ? BUS_DMA_READ :
				      BUS_DMA_WRITE));
 			} else
#endif /* TFS */
 			{
 				error = bus_dmamap_load(dmat, ccb->dmamap_xfer,
 				    xs->data, xs->datalen, NULL,
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
					aprint_error_dev(sc->sc_dev, "adv_scsi_cmd, more than %d"
					    " DMA segments\n",
					    ASC_MAX_SG_LIST);
				} else {
					aprint_error_dev(sc->sc_dev, "adv_scsi_cmd, error %d"
					    " loading DMA map\n",
					    error);
				}

out_bad:
 				adv_free_ccb(sc, ccb);
 				scsipi_done(xs);
 				return;
 			}
 			bus_dmamap_sync(dmat, ccb->dmamap_xfer, 0,
 			    ccb->dmamap_xfer->dm_mapsize,
 			    (flags & XS_CTL_DATA_IN) ?
 			     BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE);

 			memset(&ccb->sghead, 0, sizeof(ASC_SG_HEAD));

 			for (nsegs = 0;
 			     nsegs < ccb->dmamap_xfer->dm_nsegs; nsegs++) {
 				ccb->sghead.sg_list[nsegs].addr =
 				    ccb->dmamap_xfer->dm_segs[nsegs].ds_addr;
 				ccb->sghead.sg_list[nsegs].bytes =
 				    ccb->dmamap_xfer->dm_segs[nsegs].ds_len;
 			}

 			ccb->sghead.entry_cnt = ccb->scsiq.q1.sg_queue_cnt =
 			    ccb->dmamap_xfer->dm_nsegs;

 			ccb->scsiq.q1.cntl |= ASC_QC_SG_HEAD;
 			ccb->scsiq.sg_head = &ccb->sghead;
 			ccb->scsiq.q1.data_addr = 0;
 			ccb->scsiq.q1.data_cnt = 0;
 		} else {
 			/*
 			 * No data xfer, use non S/G values.
 			 */
 			ccb->scsiq.q1.data_addr = 0;
 			ccb->scsiq.q1.data_cnt = 0;
 		}

#ifdef ASC_DEBUG
 		printf("id = %d, lun = %d, cmd = %d, ccb = 0x%lX\n",
 		    periph->periph_target,
 		    periph->periph_lun, xs->cmd->opcode,
 		    (unsigned long)ccb);
#endif
 		s = splbio();
 		adv_queue_ccb(sc, ccb);
 		splx(s);

 		if ((flags & XS_CTL_POLL) == 0)
 			return;

 		/* Not allowed to use interrupts, poll for completion. */
 		if (adv_poll(sc, xs, ccb->timeout)) {
 			adv_timeout(ccb);
 			if (adv_poll(sc, xs, ccb->timeout))
 				adv_timeout(ccb);
 		}
 		return;

 	case ADAPTER_REQ_GROW_RESOURCES:
 		/* XXX Not supported. */
 		return;

 	case ADAPTER_REQ_SET_XFER_MODE:
 	    {
 		/*
 		 * We can't really set the mode, but we know how to
 		 * query what the firmware negotiated.
 		 */
 		struct scsipi_xfer_mode *xm = arg;
 		u_int8_t sdtr_data;
 		ASC_SCSI_BIT_ID_TYPE tid_bit;

 		tid_bit = ASC_TIX_TO_TARGET_ID(xm->xm_target);

 		xm->xm_mode = 0;
 		xm->xm_period = 0;
 		xm->xm_offset = 0;

 		if (sc->init_sdtr & tid_bit) {
 			xm->xm_mode |= PERIPH_CAP_SYNC;
 			sdtr_data = sc->sdtr_data[xm->xm_target];
 			xm->xm_period =
 			    sc->sdtr_period_tbl[(sdtr_data >> 4) &
 			    (sc->max_sdtr_index - 1)];
 			xm->xm_offset = sdtr_data & ASC_SYN_MAX_OFFSET;
 		}

 		if (sc->use_tagged_qng & tid_bit)
 			xm->xm_mode |= PERIPH_CAP_TQING;

 		scsipi_async_event(chan, ASYNC_EVENT_XFER_MODE, xm);
 		return;
 	    }
 	}
}

int
adv_intr(void *arg)
{
	ASC_SOFTC      *sc = arg;

#ifdef ASC_DEBUG
	int int_pend = FALSE;

	if(ASC_IS_INT_PENDING(sc->sc_iot, sc->sc_ioh))
	{
		int_pend = TRUE;
		printf("ISR - ");
	}
#endif
	AscISR(sc);
#ifdef ASC_DEBUG
	if(int_pend)
		printf("\n");
#endif

	return (1);
}


/*
 * Poll a particular unit, looking for a particular xs
 */
static int
adv_poll(ASC_SOFTC *sc, struct scsipi_xfer *xs, int count)
{

	/* timeouts are in msec, so we loop in 1000 usec cycles */
	while (count) {
		adv_intr(sc);
		if (xs->xs_status & XS_STS_DONE)
			return (0);
		delay(1000);	/* only happens in boot so ok */
		count--;
	}
	return (1);
}


static void
adv_timeout(void *arg)
{
	ADV_CCB        *ccb = arg;
	struct scsipi_xfer *xs = ccb->xs;
	struct scsipi_periph *periph = xs->xs_periph;
	ASC_SOFTC      *sc =
	    device_private(periph->periph_channel->chan_adapter->adapt_dev);
	int             s;

	scsipi_printaddr(periph);
	printf("timed out");

	s = splbio();

	/*
	 * If it has been through before, then a previous abort has failed,
	 * don't try abort again, reset the bus instead.
	 */
	if (ccb->flags & CCB_ABORT) {
		/* abort timed out */
		printf(" AGAIN. Resetting Bus\n");
		/* Lets try resetting the bus! */
		if (AscResetBus(sc) == ASC_ERROR) {
			ccb->timeout = sc->scsi_reset_wait;
			adv_queue_ccb(sc, ccb);
		}
	} else {
		/* abort the operation that has timed out */
		printf("\n");
		AscAbortCCB(sc, ccb);
		ccb->xs->error = XS_TIMEOUT;
		ccb->timeout = ADV_ABORT_TIMEOUT;
		ccb->flags |= CCB_ABORT;
		adv_queue_ccb(sc, ccb);
	}

	splx(s);
}


static void
adv_watchdog(void *arg)
{
	ADV_CCB        *ccb = arg;
	struct scsipi_xfer *xs = ccb->xs;
	struct scsipi_periph *periph = xs->xs_periph;
	ASC_SOFTC      *sc =
	    device_private(periph->periph_channel->chan_adapter->adapt_dev);
	int             s;

	s = splbio();

	ccb->flags &= ~CCB_WATCHDOG;
	adv_start_ccbs(sc);

	splx(s);
}


/******************************************************************************/
/*                      NARROW boards Interrupt callbacks                     */
/******************************************************************************/


/*
 * adv_narrow_isr_callback() - Second Level Interrupt Handler called by AscISR()
 *
 * Interrupt callback function for the Narrow SCSI Asc Library.
 */
static void
adv_narrow_isr_callback(ASC_SOFTC *sc, ASC_QDONE_INFO *qdonep)
{
	bus_dma_tag_t   dmat = sc->sc_dmat;
	ADV_CCB        *ccb;
	struct scsipi_xfer *xs;
	struct scsi_sense_data *s1, *s2;


	ccb = adv_ccb_phys_kv(sc, qdonep->d2.ccb_ptr);
	xs = ccb->xs;

#ifdef ASC_DEBUG
	printf(" - ccb=0x%lx, id=%d, lun=%d, cmd=%d, ",
			(unsigned long)ccb,
			xs->xs_periph->periph_target,
			xs->xs_periph->periph_lun, xs->cmd->opcode);
#endif
	callout_stop(&ccb->xs->xs_callout);

	/*
	 * If we were a data transfer, unload the map that described
	 * the data buffer.
	 */
	if (xs->datalen) {
		bus_dmamap_sync(dmat, ccb->dmamap_xfer, 0,
				ccb->dmamap_xfer->dm_mapsize,
			 (xs->xs_control & XS_CTL_DATA_IN) ?
			 BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(dmat, ccb->dmamap_xfer);
	}
	if ((ccb->flags & CCB_ALLOC) == 0) {
		aprint_error_dev(sc->sc_dev, "exiting ccb not allocated!\n");
		Debugger();
		return;
	}
	/*
	 * 'qdonep' contains the command's ending status.
	 */
#ifdef ASC_DEBUG
	printf("d_s=%d, h_s=%d", qdonep->d3.done_stat, qdonep->d3.host_stat);
#endif
	switch (qdonep->d3.done_stat) {
	case ASC_QD_NO_ERROR:
		switch (qdonep->d3.host_stat) {
		case ASC_QHSTA_NO_ERROR:
			xs->error = XS_NOERROR;
			xs->resid = 0;
			break;

		default:
			/* QHSTA error occurred */
			xs->error = XS_DRIVER_STUFFUP;
			break;
		}

		/*
	         * If an INQUIRY command completed successfully, then call
	         * the AscInquiryHandling() function to patch bugged boards.
	         */
		if ((xs->cmd->opcode == SCSICMD_Inquiry) &&
		    (xs->xs_periph->periph_lun == 0) &&
		    (xs->datalen - qdonep->remain_bytes) >= 8) {
			AscInquiryHandling(sc,
				      xs->xs_periph->periph_target & 0x7,
					   (ASC_SCSI_INQUIRY *) xs->data);
		}
		break;

	case ASC_QD_WITH_ERROR:
		switch (qdonep->d3.host_stat) {
		case ASC_QHSTA_NO_ERROR:
			if (qdonep->d3.scsi_stat == SS_CHK_CONDITION) {
				s1 = &ccb->scsi_sense;
				s2 = &xs->sense.scsi_sense;
				*s2 = *s1;
				xs->error = XS_SENSE;
			} else {
				xs->error = XS_DRIVER_STUFFUP;
			}
			break;

		case ASC_QHSTA_M_SEL_TIMEOUT:
			xs->error = XS_SELTIMEOUT;
			break;

		default:
			/* QHSTA error occurred */
			xs->error = XS_DRIVER_STUFFUP;
			break;
		}
		break;

	case ASC_QD_ABORTED_BY_HOST:
	default:
		xs->error = XS_DRIVER_STUFFUP;
		break;
	}


	adv_free_ccb(sc, ccb);
	scsipi_done(xs);
}
