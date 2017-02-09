/* $NetBSD: adw.c,v 1.52 2012/10/27 17:18:18 chs Exp $	 */

/*
 * Generic driver for the Advanced Systems Inc. SCSI controllers
 *
 * Copyright (c) 1998, 1999, 2000 The NetBSD Foundation, Inc.
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
__KERNEL_RCSID(0, "$NetBSD: adw.c,v 1.52 2012/10/27 17:18:18 chs Exp $");

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

#include <dev/ic/adwlib.h>
#include <dev/ic/adwmcode.h>
#include <dev/ic/adw.h>

#ifndef DDB
#define	Debugger()	panic("should call debugger here (adw.c)")
#endif				/* ! DDB */

/******************************************************************************/


static int adw_alloc_controls(ADW_SOFTC *);
static int adw_alloc_carriers(ADW_SOFTC *);
static int adw_create_ccbs(ADW_SOFTC *, ADW_CCB *, int);
static void adw_free_ccb(ADW_SOFTC *, ADW_CCB *);
static void adw_reset_ccb(ADW_CCB *);
static int adw_init_ccb(ADW_SOFTC *, ADW_CCB *);
static ADW_CCB *adw_get_ccb(ADW_SOFTC *);
static int adw_queue_ccb(ADW_SOFTC *, ADW_CCB *);

static void adw_scsipi_request(struct scsipi_channel *,
	scsipi_adapter_req_t, void *);
static int adw_build_req(ADW_SOFTC *, ADW_CCB *);
static void adw_build_sglist(ADW_CCB *, ADW_SCSI_REQ_Q *, ADW_SG_BLOCK *);
static void adwminphys(struct buf *);
static void adw_isr_callback(ADW_SOFTC *, ADW_SCSI_REQ_Q *);
static void adw_async_callback(ADW_SOFTC *, u_int8_t);

static void adw_print_info(ADW_SOFTC *, int);

static int adw_poll(ADW_SOFTC *, struct scsipi_xfer *, int);
static void adw_timeout(void *);
static void adw_reset_bus(ADW_SOFTC *);


/******************************************************************************/
/*                       DMA Mapping for Control Blocks                       */
/******************************************************************************/


static int
adw_alloc_controls(ADW_SOFTC *sc)
{
	bus_dma_segment_t seg;
	int             error, rseg;

	/*
         * Allocate the control structure.
         */
	if ((error = bus_dmamem_alloc(sc->sc_dmat, sizeof(struct adw_control),
			   PAGE_SIZE, 0, &seg, 1, &rseg,
			   BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to allocate control structures,"
		       " error = %d\n", error);
		return (error);
	}
	if ((error = bus_dmamem_map(sc->sc_dmat, &seg, rseg,
		   sizeof(struct adw_control), (void **) & sc->sc_control,
				 BUS_DMA_NOWAIT | BUS_DMA_COHERENT)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to map control structures, error = %d\n",
		       error);
		return (error);
	}

	/*
         * Create and load the DMA map used for the control blocks.
         */
	if ((error = bus_dmamap_create(sc->sc_dmat, sizeof(struct adw_control),
			   1, sizeof(struct adw_control), 0, BUS_DMA_NOWAIT,
				       &sc->sc_dmamap_control)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to create control DMA map, error = %d\n",
		       error);
		return (error);
	}
	if ((error = bus_dmamap_load(sc->sc_dmat, sc->sc_dmamap_control,
			   sc->sc_control, sizeof(struct adw_control), NULL,
				     BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to load control DMA map, error = %d\n",
		       error);
		return (error);
	}

	return (0);
}


static int
adw_alloc_carriers(ADW_SOFTC *sc)
{
	bus_dma_segment_t seg;
	int             error, rseg;

	/*
         * Allocate the control structure.
         */
	sc->sc_control->carriers = malloc(sizeof(ADW_CARRIER) * ADW_MAX_CARRIER,
			M_DEVBUF, M_WAITOK);
	if(!sc->sc_control->carriers) {
		aprint_error_dev(sc->sc_dev,
		    "malloc() failed in allocating carrier structures\n");
		return (ENOMEM);
	}

	if ((error = bus_dmamem_alloc(sc->sc_dmat,
			sizeof(ADW_CARRIER) * ADW_MAX_CARRIER,
			0x10, 0, &seg, 1, &rseg, BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to allocate carrier structures,"
		       " error = %d\n", error);
		return (error);
	}
	if ((error = bus_dmamem_map(sc->sc_dmat, &seg, rseg,
			sizeof(ADW_CARRIER) * ADW_MAX_CARRIER,
			(void **) &sc->sc_control->carriers,
			BUS_DMA_NOWAIT | BUS_DMA_COHERENT)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to map carrier structures,"
			" error = %d\n", error);
		return (error);
	}

	/*
         * Create and load the DMA map used for the control blocks.
         */
	if ((error = bus_dmamap_create(sc->sc_dmat,
			sizeof(ADW_CARRIER) * ADW_MAX_CARRIER, 1,
			sizeof(ADW_CARRIER) * ADW_MAX_CARRIER, 0,BUS_DMA_NOWAIT,
			&sc->sc_dmamap_carrier)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to create carriers DMA map,"
			" error = %d\n", error);
		return (error);
	}
	if ((error = bus_dmamap_load(sc->sc_dmat,
			sc->sc_dmamap_carrier, sc->sc_control->carriers,
			sizeof(ADW_CARRIER) * ADW_MAX_CARRIER, NULL,
			BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to load carriers DMA map,"
			" error = %d\n", error);
		return (error);
	}

	return (0);
}


/******************************************************************************/
/*                           Control Blocks routines                          */
/******************************************************************************/


/*
 * Create a set of ccbs and add them to the free list.  Called once
 * by adw_init().  We return the number of CCBs successfully created.
 */
static int
adw_create_ccbs(ADW_SOFTC *sc, ADW_CCB *ccbstore, int count)
{
	ADW_CCB        *ccb;
	int             i, error;

	for (i = 0; i < count; i++) {
		ccb = &ccbstore[i];
		if ((error = adw_init_ccb(sc, ccb)) != 0) {
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
adw_free_ccb(ADW_SOFTC *sc, ADW_CCB *ccb)
{
	int             s;

	s = splbio();

	adw_reset_ccb(ccb);
	TAILQ_INSERT_HEAD(&sc->sc_free_ccb, ccb, chain);

	splx(s);
}


static void
adw_reset_ccb(ADW_CCB *ccb)
{

	ccb->flags = 0;
}


static int
adw_init_ccb(ADW_SOFTC *sc, ADW_CCB *ccb)
{
	int	hashnum, error;

	/*
         * Create the DMA map for this CCB.
         */
	error = bus_dmamap_create(sc->sc_dmat,
				  (ADW_MAX_SG_LIST - 1) * PAGE_SIZE,
			 ADW_MAX_SG_LIST, (ADW_MAX_SG_LIST - 1) * PAGE_SIZE,
		   0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &ccb->dmamap_xfer);
	if (error) {
		aprint_error_dev(sc->sc_dev, "unable to create CCB DMA map, error = %d\n",
		       error);
		return (error);
	}

	/*
	 * put in the phystokv hash table
	 * Never gets taken out.
	 */
	ccb->hashkey = htole32(sc->sc_dmamap_control->dm_segs[0].ds_addr +
	    ADW_CCB_OFF(ccb));
	hashnum = CCB_HASH(ccb->hashkey);
	ccb->nexthash = sc->sc_ccbhash[hashnum];
	sc->sc_ccbhash[hashnum] = ccb;
	adw_reset_ccb(ccb);
	return (0);
}


/*
 * Get a free ccb
 *
 * If there are none, see if we can allocate a new one
 */
static ADW_CCB *
adw_get_ccb(ADW_SOFTC *sc)
{
	ADW_CCB        *ccb = 0;
	int             s;

	s = splbio();

	ccb = sc->sc_free_ccb.tqh_first;
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
ADW_CCB *
adw_ccb_phys_kv(ADW_SOFTC *sc, u_int32_t ccb_phys)
{
	int hashnum = CCB_HASH(ccb_phys);
	ADW_CCB *ccb = sc->sc_ccbhash[hashnum];

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
static int
adw_queue_ccb(ADW_SOFTC *sc, ADW_CCB *ccb)
{
	int		errcode = ADW_SUCCESS;

	TAILQ_INSERT_TAIL(&sc->sc_waiting_ccb, ccb, chain);

	while ((ccb = sc->sc_waiting_ccb.tqh_first) != NULL) {

		TAILQ_REMOVE(&sc->sc_waiting_ccb, ccb, chain);
		errcode = AdwExeScsiQueue(sc, &ccb->scsiq);
		switch(errcode) {
		case ADW_SUCCESS:
			break;

		case ADW_BUSY:
			printf("ADW_BUSY\n");
			return(ADW_BUSY);

		case ADW_ERROR:
			printf("ADW_ERROR\n");
			return(ADW_ERROR);
		}

		TAILQ_INSERT_TAIL(&sc->sc_pending_ccb, ccb, chain);

		if ((ccb->xs->xs_control & XS_CTL_POLL) == 0)
			callout_reset(&ccb->xs->xs_callout,
			    mstohz(ccb->timeout), adw_timeout, ccb);
	}

	return(errcode);
}


/******************************************************************************/
/*                       SCSI layer interfacing routines                      */
/******************************************************************************/


int
adw_init(ADW_SOFTC *sc)
{
	u_int16_t       warn_code;


	sc->cfg.lib_version = (ADW_LIB_VERSION_MAJOR << 8) |
		ADW_LIB_VERSION_MINOR;
	sc->cfg.chip_version =
		ADW_GET_CHIP_VERSION(sc->sc_iot, sc->sc_ioh, sc->bus_type);

	/*
	 * Reset the chip to start and allow register writes.
	 */
	if (ADW_FIND_SIGNATURE(sc->sc_iot, sc->sc_ioh) == 0) {
		panic("adw_init: adw_find_signature failed");
	} else {
		AdwResetChip(sc->sc_iot, sc->sc_ioh);

		warn_code = AdwInitFromEEPROM(sc);

		if (warn_code & ADW_WARN_EEPROM_CHKSUM)
			aprint_error_dev(sc->sc_dev, "Bad checksum found. "
			       "Setting default values\n");
		if (warn_code & ADW_WARN_EEPROM_TERMINATION)
			aprint_error_dev(sc->sc_dev, "Bad bus termination setting."
			       "Using automatic termination.\n");
	}

	sc->isr_callback = (ADW_CALLBACK) adw_isr_callback;
	sc->async_callback = (ADW_CALLBACK) adw_async_callback;

	return 0;
}


void
adw_attach(ADW_SOFTC *sc)
{
	struct scsipi_adapter *adapt = &sc->sc_adapter;
	struct scsipi_channel *chan = &sc->sc_channel;
	int             ncontrols, error;

	TAILQ_INIT(&sc->sc_free_ccb);
	TAILQ_INIT(&sc->sc_waiting_ccb);
	TAILQ_INIT(&sc->sc_pending_ccb);

	/*
         * Allocate the Control Blocks.
         */
	error = adw_alloc_controls(sc);
	if (error)
		return; /* (error) */ ;

	memset(sc->sc_control, 0, sizeof(struct adw_control));

	/*
	 * Create and initialize the Control Blocks.
	 */
	ncontrols = adw_create_ccbs(sc, sc->sc_control->ccbs, ADW_MAX_CCB);
	if (ncontrols == 0) {
		aprint_error_dev(sc->sc_dev, "unable to create Control Blocks\n");
		return; /* (ENOMEM) */ ;
	} else if (ncontrols != ADW_MAX_CCB) {
		aprint_error_dev(sc->sc_dev, "WARNING: only %d of %d Control Blocks"
		       " created\n",
		       ncontrols, ADW_MAX_CCB);
	}

	/*
	 * Create and initialize the Carriers.
	 */
	error = adw_alloc_carriers(sc);
	if (error)
		return; /* (error) */ ;

	/*
	 * Zero's the freeze_device status
	 */
	 memset(sc->sc_freeze_dev, 0, sizeof(sc->sc_freeze_dev));

	/*
	 * Initialize the adapter
	 */
	switch (AdwInitDriver(sc)) {
	case ADW_IERR_BIST_PRE_TEST:
		panic("%s: BIST pre-test error",
		      device_xname(sc->sc_dev));
		break;

	case ADW_IERR_BIST_RAM_TEST:
		panic("%s: BIST RAM test error",
		      device_xname(sc->sc_dev));
		break;

	case ADW_IERR_MCODE_CHKSUM:
		panic("%s: Microcode checksum error",
		      device_xname(sc->sc_dev));
		break;

	case ADW_IERR_ILLEGAL_CONNECTION:
		panic("%s: All three connectors are in use",
		      device_xname(sc->sc_dev));
		break;

	case ADW_IERR_REVERSED_CABLE:
		panic("%s: Cable is reversed",
		      device_xname(sc->sc_dev));
		break;

	case ADW_IERR_HVD_DEVICE:
		panic("%s: HVD attached to LVD connector",
		      device_xname(sc->sc_dev));
		break;

	case ADW_IERR_SINGLE_END_DEVICE:
		panic("%s: single-ended device is attached to"
		      " one of the connectors",
		      device_xname(sc->sc_dev));
		break;

	case ADW_IERR_NO_CARRIER:
		panic("%s: unable to create Carriers",
		      device_xname(sc->sc_dev));
		break;

	case ADW_WARN_BUSRESET_ERROR:
		aprint_error_dev(sc->sc_dev, "WARNING: Bus Reset Error\n");
		break;
	}

	/*
	 * Fill in the scsipi_adapter.
	 */
	memset(adapt, 0, sizeof(*adapt));
	adapt->adapt_dev = sc->sc_dev;
	adapt->adapt_nchannels = 1;
	adapt->adapt_openings = ncontrols;
	adapt->adapt_max_periph = adapt->adapt_openings;
	adapt->adapt_request = adw_scsipi_request;
	adapt->adapt_minphys = adwminphys;

	/*
	 * Fill in the scsipi_channel.
	 */
	memset(chan, 0, sizeof(*chan));
	chan->chan_adapter = adapt;
	chan->chan_bustype = &scsi_bustype;
	chan->chan_channel = 0;
	chan->chan_ntargets = ADW_MAX_TID + 1;
	chan->chan_nluns = 8;
	chan->chan_id = sc->chip_scsi_id;

	config_found(sc->sc_dev, &sc->sc_channel, scsiprint);
}


static void
adwminphys(struct buf *bp)
{

	if (bp->b_bcount > ((ADW_MAX_SG_LIST - 1) * PAGE_SIZE))
		bp->b_bcount = ((ADW_MAX_SG_LIST - 1) * PAGE_SIZE);
	minphys(bp);
}


/*
 * start a scsi operation given the command and the data address.
 * Also needs the unit, target and lu.
 */
static void
adw_scsipi_request(struct scsipi_channel *chan, scsipi_adapter_req_t req,
	void *arg)
{
	struct scsipi_xfer *xs;
	ADW_SOFTC      *sc = device_private(chan->chan_adapter->adapt_dev);
	ADW_CCB        *ccb;
	int            s, retry;

	switch (req) {
	case ADAPTER_REQ_RUN_XFER:
		xs = arg;

		/*
		 * get a ccb to use. If the transfer
		 * is from a buf (possibly from interrupt time)
		 * then we can't allow it to sleep
		 */

		ccb = adw_get_ccb(sc);
#ifdef DIAGNOSTIC
		/*
                 * This should never happen as we track the resources
		 * in the mid-layer.
                 */
		if (ccb == NULL) {
			scsipi_printaddr(xs->xs_periph);
			printf("unable to allocate ccb\n");
			panic("adw_scsipi_request");
		}
#endif

		ccb->xs = xs;
		ccb->timeout = xs->timeout;

		if (adw_build_req(sc, ccb)) {
			s = splbio();
			retry = adw_queue_ccb(sc, ccb);
			splx(s);

			switch(retry) {
			case ADW_BUSY:
				xs->error = XS_RESOURCE_SHORTAGE;
				adw_free_ccb(sc, ccb);
				scsipi_done(xs);
				return;

			case ADW_ERROR:
				xs->error = XS_DRIVER_STUFFUP;
				adw_free_ccb(sc, ccb);
				scsipi_done(xs);
				return;
			}
			if ((xs->xs_control & XS_CTL_POLL) == 0)
				return;
			/*
			 * Not allowed to use interrupts, poll for completion.
			 */
			if (adw_poll(sc, xs, ccb->timeout)) {
				adw_timeout(ccb);
				if (adw_poll(sc, xs, ccb->timeout))
					adw_timeout(ccb);
			}
		}
		return;

	case ADAPTER_REQ_GROW_RESOURCES:
		/* XXX Not supported. */
		return;

	case ADAPTER_REQ_SET_XFER_MODE:
		/* XXX XXX XXX */
		return;
	}
}


/*
 * Build a request structure for the Wide Boards.
 */
static int
adw_build_req(ADW_SOFTC *sc, ADW_CCB *ccb)
{
	struct scsipi_xfer *xs = ccb->xs;
	struct scsipi_periph *periph = xs->xs_periph;
	bus_dma_tag_t   dmat = sc->sc_dmat;
	ADW_SCSI_REQ_Q *scsiqp;
	int             error;

	scsiqp = &ccb->scsiq;
	memset(scsiqp, 0, sizeof(ADW_SCSI_REQ_Q));

	/*
	 * Set the ADW_SCSI_REQ_Q 'ccb_ptr' to point to the
	 * physical CCB structure.
	 */
	scsiqp->ccb_ptr = ccb->hashkey;

	/*
	 * Build the ADW_SCSI_REQ_Q request.
	 */

	/*
	 * Set CDB length and copy it to the request structure.
	 * For wide  boards a CDB length maximum of 16 bytes
	 * is supported.
	 */
	memcpy(&scsiqp->cdb, xs->cmd, ((scsiqp->cdb_len = xs->cmdlen) <= 12)?
			xs->cmdlen : 12 );
	if(xs->cmdlen > 12)
		memcpy(&scsiqp->cdb16, &(xs->cmd[12]), xs->cmdlen - 12);

	scsiqp->target_id = periph->periph_target;
	scsiqp->target_lun = periph->periph_lun;

	scsiqp->vsense_addr = &ccb->scsi_sense;
	scsiqp->sense_addr = htole32(sc->sc_dmamap_control->dm_segs[0].ds_addr +
			ADW_CCB_OFF(ccb) + offsetof(struct adw_ccb, scsi_sense));
	scsiqp->sense_len = sizeof(struct scsi_sense_data);

	/*
	 * Build ADW_SCSI_REQ_Q for a scatter-gather buffer command.
	 */
	if (xs->datalen) {
		/*
                 * Map the DMA transfer.
                 */
#ifdef TFS
		if (xs->xs_control & SCSI_DATA_UIO) {
			error = bus_dmamap_load_uio(dmat,
				ccb->dmamap_xfer, (struct uio *) xs->data,
			        ((flags & XS_CTL_NOSLEEP) ? BUS_DMA_NOWAIT :
			         BUS_DMA_WAITOK) | BUS_DMA_STREAMING |
				 ((flags & XS_CTL_DATA_IN) ? BUS_DMA_READ :
				  BUS_DMA_WRITE));
		} else
#endif		/* TFS */
		{
			error = bus_dmamap_load(dmat,
			      ccb->dmamap_xfer, xs->data, xs->datalen, NULL,
			      ((xs->xs_control & XS_CTL_NOSLEEP) ?
			       BUS_DMA_NOWAIT : BUS_DMA_WAITOK) |
			       BUS_DMA_STREAMING |
			       ((xs->xs_control & XS_CTL_DATA_IN) ?
			        BUS_DMA_READ : BUS_DMA_WRITE));
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
			adw_free_ccb(sc, ccb);
			scsipi_done(xs);
			return(0);
		}

		bus_dmamap_sync(dmat, ccb->dmamap_xfer, 0,
		    ccb->dmamap_xfer->dm_mapsize,
		    (xs->xs_control & XS_CTL_DATA_IN) ?
		    BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE);

		/*
		 * Build scatter-gather list.
		 */
		scsiqp->data_cnt = htole32(xs->datalen);
		scsiqp->vdata_addr = xs->data;
		scsiqp->data_addr = htole32(ccb->dmamap_xfer->dm_segs[0].ds_addr);
		memset(ccb->sg_block, 0,
		    sizeof(ADW_SG_BLOCK) * ADW_NUM_SG_BLOCK);
		adw_build_sglist(ccb, scsiqp, ccb->sg_block);
	} else {
		/*
                 * No data xfer, use non S/G values.
                 */
		scsiqp->data_cnt = 0;
		scsiqp->vdata_addr = 0;
		scsiqp->data_addr = 0;
	}

	return (1);
}


/*
 * Build scatter-gather list for Wide Boards.
 */
static void
adw_build_sglist(ADW_CCB *ccb, ADW_SCSI_REQ_Q *scsiqp, ADW_SG_BLOCK *sg_block)
{
	u_long          sg_block_next_addr;	/* block and its next */
	u_int32_t       sg_block_physical_addr;
	int             i;	/* how many SG entries */
	bus_dma_segment_t *sg_list = &ccb->dmamap_xfer->dm_segs[0];
	int             sg_elem_cnt = ccb->dmamap_xfer->dm_nsegs;


	sg_block_next_addr = (u_long) sg_block;	/* allow math operation */
	sg_block_physical_addr = le32toh(ccb->hashkey) +
	    offsetof(struct adw_ccb, sg_block[0]);
	scsiqp->sg_real_addr = htole32(sg_block_physical_addr);

	/*
	 * If there are more than NO_OF_SG_PER_BLOCK DMA segments (hw sg-list)
	 * then split the request into multiple sg-list blocks.
	 */

	do {
		for (i = 0; i < NO_OF_SG_PER_BLOCK; i++) {
			sg_block->sg_list[i].sg_addr = htole32(sg_list->ds_addr);
			sg_block->sg_list[i].sg_count = htole32(sg_list->ds_len);

			if (--sg_elem_cnt == 0) {
				/* last entry, get out */
				sg_block->sg_cnt = i + 1;
				sg_block->sg_ptr = 0; /* next link = NULL */
				return;
			}
			sg_list++;
		}
		sg_block_next_addr += sizeof(ADW_SG_BLOCK);
		sg_block_physical_addr += sizeof(ADW_SG_BLOCK);

		sg_block->sg_cnt = NO_OF_SG_PER_BLOCK;
		sg_block->sg_ptr = htole32(sg_block_physical_addr);
		sg_block = (ADW_SG_BLOCK *) sg_block_next_addr;	/* virt. addr */
	} while (1);
}


/******************************************************************************/
/*                       Interrupts and TimeOut routines                      */
/******************************************************************************/


int
adw_intr(void *arg)
{
	ADW_SOFTC      *sc = arg;


	if(AdwISR(sc) != ADW_FALSE) {
		return (1);
	}

	return (0);
}


/*
 * Poll a particular unit, looking for a particular xs
 */
static int
adw_poll(ADW_SOFTC *sc, struct scsipi_xfer *xs, int count)
{

	/* timeouts are in msec, so we loop in 1000 usec cycles */
	while (count) {
		adw_intr(sc);
		if (xs->xs_status & XS_STS_DONE)
			return (0);
		delay(1000);	/* only happens in boot so ok */
		count--;
	}
	return (1);
}


static void
adw_timeout(void *arg)
{
	ADW_CCB        *ccb = arg;
	struct scsipi_xfer *xs = ccb->xs;
	struct scsipi_periph *periph = xs->xs_periph;
	ADW_SOFTC      *sc =
	    device_private(periph->periph_channel->chan_adapter->adapt_dev);
	int             s;

	scsipi_printaddr(periph);
	printf("timed out");

	s = splbio();

	if (ccb->flags & CCB_ABORTED) {
	/*
	 * Abort Timed Out
	 *
	 * No more opportunities. Lets try resetting the bus and
	 * reinitialize the host adapter.
	 */
		callout_stop(&xs->xs_callout);
		printf(" AGAIN. Resetting SCSI Bus\n");
		adw_reset_bus(sc);
		splx(s);
		return;
	} else if (ccb->flags & CCB_ABORTING) {
	/*
	 * Abort the operation that has timed out.
	 *
	 * Second opportunity.
	 */
		printf("\n");
		xs->error = XS_TIMEOUT;
		ccb->flags |= CCB_ABORTED;
#if 0
		/*
		 * - XXX - 3.3a microcode is BROKEN!!!
		 *
		 * We cannot abort a CCB, so we can only hope the command
		 * get completed before the next timeout, otherwise a
		 * Bus Reset will arrive inexorably.
		 */
		/*
		 * ADW_ABORT_CCB() makes the board to generate an interrupt
		 *
		 * - XXX - The above assertion MUST be verified (and this
		 *         code changed as well [callout_*()]), when the
		 *         ADW_ABORT_CCB will be working again
		 */
		ADW_ABORT_CCB(sc, ccb);
#endif
		/*
		 * waiting for multishot callout_reset() let's restart it
		 * by hand so the next time a timeout event will occur
		 * we will reset the bus.
		 */
		callout_reset(&xs->xs_callout,
			    mstohz(ccb->timeout), adw_timeout, ccb);
	} else {
	/*
	 * Abort the operation that has timed out.
	 *
	 * First opportunity.
	 */
		printf("\n");
		xs->error = XS_TIMEOUT;
		ccb->flags |= CCB_ABORTING;
#if 0
		/*
		 * - XXX - 3.3a microcode is BROKEN!!!
		 *
		 * We cannot abort a CCB, so we can only hope the command
		 * get completed before the next 2 timeout, otherwise a
		 * Bus Reset will arrive inexorably.
		 */
		/*
		 * ADW_ABORT_CCB() makes the board to generate an interrupt
		 *
		 * - XXX - The above assertion MUST be verified (and this
		 *         code changed as well [callout_*()]), when the
		 *         ADW_ABORT_CCB will be working again
		 */
		ADW_ABORT_CCB(sc, ccb);
#endif
		/*
		 * waiting for multishot callout_reset() let's restart it
		 * by hand so to give a second opportunity to the command
		 * which timed-out.
		 */
		callout_reset(&xs->xs_callout,
			    mstohz(ccb->timeout), adw_timeout, ccb);
	}

	splx(s);
}


static void
adw_reset_bus(ADW_SOFTC *sc)
{
	ADW_CCB	*ccb;
	int	 s;
	struct scsipi_xfer *xs;

	s = splbio();
	AdwResetSCSIBus(sc);
	while((ccb = TAILQ_LAST(&sc->sc_pending_ccb,
			adw_pending_ccb)) != NULL) {
		callout_stop(&ccb->xs->xs_callout);
		TAILQ_REMOVE(&sc->sc_pending_ccb, ccb, chain);
		xs = ccb->xs;
		adw_free_ccb(sc, ccb);
		xs->error = XS_RESOURCE_SHORTAGE;
		scsipi_done(xs);
	}
	splx(s);
}


/******************************************************************************/
/*              Host Adapter and Peripherals Information Routines             */
/******************************************************************************/


static void
adw_print_info(ADW_SOFTC *sc, int tid)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_int16_t wdtr_able, wdtr_done, wdtr;
    	u_int16_t sdtr_able, sdtr_done, sdtr, period;
	static int wdtr_reneg = 0, sdtr_reneg = 0;

	if (tid == 0){
		wdtr_reneg = sdtr_reneg = 0;
	}

	printf("%s: target %d ", device_xname(sc->sc_dev), tid);

	ADW_READ_WORD_LRAM(iot, ioh, ADW_MC_SDTR_ABLE, wdtr_able);
	if(wdtr_able & ADW_TID_TO_TIDMASK(tid)) {
		ADW_READ_WORD_LRAM(iot, ioh, ADW_MC_SDTR_DONE, wdtr_done);
		ADW_READ_WORD_LRAM(iot, ioh, ADW_MC_DEVICE_HSHK_CFG_TABLE +
			(2 * tid), wdtr);
		printf("using %d-bits wide, ", (wdtr & 0x8000)? 16 : 8);
		if((wdtr_done & ADW_TID_TO_TIDMASK(tid)) == 0)
			wdtr_reneg = 1;
	} else {
		printf("wide transfers disabled, ");
	}

	ADW_READ_WORD_LRAM(iot, ioh, ADW_MC_SDTR_ABLE, sdtr_able);
	if(sdtr_able & ADW_TID_TO_TIDMASK(tid)) {
		ADW_READ_WORD_LRAM(iot, ioh, ADW_MC_SDTR_DONE, sdtr_done);
		ADW_READ_WORD_LRAM(iot, ioh, ADW_MC_DEVICE_HSHK_CFG_TABLE +
			(2 * tid), sdtr);
		sdtr &=  ~0x8000;
		if((sdtr & 0x1F) != 0) {
			if((sdtr & 0x1F00) == 0x1100){
				printf("80.0 MHz");
			} else if((sdtr & 0x1F00) == 0x1000){
				printf("40.0 MHz");
			} else {
				/* <= 20.0 MHz */
				period = (((sdtr >> 8) * 25) + 50)/4;
				if(period == 0) {
					/* Should never happen. */
					printf("? MHz");
				} else {
					printf("%d.%d MHz", 250/period,
						ADW_TENTHS(250, period));
				}
			}
			printf(" synchronous transfers\n");
		} else {
			printf("asynchronous transfers\n");
		}
		if((sdtr_done & ADW_TID_TO_TIDMASK(tid)) == 0)
			sdtr_reneg = 1;
	} else {
		printf("synchronous transfers disabled\n");
	}

	if(wdtr_reneg || sdtr_reneg) {
		printf("%s: target %d %s", device_xname(sc->sc_dev), tid,
			(wdtr_reneg)? ((sdtr_reneg)? "wide/sync" : "wide") :
			((sdtr_reneg)? "sync" : "") );
		printf(" renegotiation pending before next command.\n");
	}
}


/******************************************************************************/
/*                        WIDE boards Interrupt callbacks                     */
/******************************************************************************/


/*
 * adw_isr_callback() - Second Level Interrupt Handler called by AdwISR()
 *
 * Interrupt callback function for the Wide SCSI Adv Library.
 *
 * Notice:
 * Interrupts are disabled by the caller (AdwISR() function), and will be
 * enabled at the end of the caller.
 */
static void
adw_isr_callback(ADW_SOFTC *sc, ADW_SCSI_REQ_Q *scsiq)
{
	bus_dma_tag_t   dmat = sc->sc_dmat;
	ADW_CCB        *ccb;
	struct scsipi_xfer *xs;
	struct scsi_sense_data *s1, *s2;


	ccb = adw_ccb_phys_kv(sc, scsiq->ccb_ptr);

	callout_stop(&ccb->xs->xs_callout);

	xs = ccb->xs;

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
	 * 'done_status' contains the command's ending status.
	 * 'host_status' contains the host adapter status.
	 * 'scsi_status' contains the scsi peripheral status.
	 */
	if ((scsiq->host_status == QHSTA_NO_ERROR) &&
	   ((scsiq->done_status == QD_NO_ERROR) ||
	    (scsiq->done_status == QD_WITH_ERROR))) {
		switch (scsiq->scsi_status) {
		case SCSI_STATUS_GOOD:
			if ((scsiq->cdb[0] == INQUIRY) &&
			    (scsiq->target_lun == 0)) {
				adw_print_info(sc, scsiq->target_id);
			}
			xs->error = XS_NOERROR;
			xs->resid = le32toh(scsiq->data_cnt);
			sc->sc_freeze_dev[scsiq->target_id] = 0;
			break;

		case SCSI_STATUS_CHECK_CONDITION:
		case SCSI_STATUS_CMD_TERMINATED:
			s1 = &ccb->scsi_sense;
			s2 = &xs->sense.scsi_sense;
			*s2 = *s1;
			xs->error = XS_SENSE;
			sc->sc_freeze_dev[scsiq->target_id] = 1;
			break;

		default:
			xs->error = XS_BUSY;
			sc->sc_freeze_dev[scsiq->target_id] = 1;
			break;
		}
	} else if (scsiq->done_status == QD_ABORTED_BY_HOST) {
		xs->error = XS_DRIVER_STUFFUP;
	} else {
		switch (scsiq->host_status) {
		case QHSTA_M_SEL_TIMEOUT:
			xs->error = XS_SELTIMEOUT;
			break;

		case QHSTA_M_SXFR_OFF_UFLW:
		case QHSTA_M_SXFR_OFF_OFLW:
		case QHSTA_M_DATA_OVER_RUN:
			aprint_error_dev(sc->sc_dev, "Overrun/Overflow/Underflow condition\n");
			xs->error = XS_DRIVER_STUFFUP;
			break;

		case QHSTA_M_SXFR_DESELECTED:
		case QHSTA_M_UNEXPECTED_BUS_FREE:
			aprint_error_dev(sc->sc_dev, "Unexpected BUS free\n");
			xs->error = XS_DRIVER_STUFFUP;
			break;

		case QHSTA_M_SCSI_BUS_RESET:
		case QHSTA_M_SCSI_BUS_RESET_UNSOL:
			aprint_error_dev(sc->sc_dev, "BUS Reset\n");
			xs->error = XS_DRIVER_STUFFUP;
			break;

		case QHSTA_M_BUS_DEVICE_RESET:
			aprint_error_dev(sc->sc_dev, "Device Reset\n");
			xs->error = XS_DRIVER_STUFFUP;
			break;

		case QHSTA_M_QUEUE_ABORTED:
			aprint_error_dev(sc->sc_dev, "Queue Aborted\n");
			xs->error = XS_DRIVER_STUFFUP;
			break;

		case QHSTA_M_SXFR_SDMA_ERR:
		case QHSTA_M_SXFR_SXFR_PERR:
		case QHSTA_M_RDMA_PERR:
			/*
			 * DMA Error. This should *NEVER* happen!
			 *
			 * Lets try resetting the bus and reinitialize
			 * the host adapter.
			 */
			aprint_error_dev(sc->sc_dev, "DMA Error. Reseting bus\n");
			TAILQ_REMOVE(&sc->sc_pending_ccb, ccb, chain);
			adw_reset_bus(sc);
			xs->error = XS_BUSY;
			goto done;

		case QHSTA_M_WTM_TIMEOUT:
		case QHSTA_M_SXFR_WD_TMO:
			/* The SCSI bus hung in a phase */
			printf("%s: Watch Dog timer expired. Reseting bus\n",
				device_xname(sc->sc_dev));
			TAILQ_REMOVE(&sc->sc_pending_ccb, ccb, chain);
			adw_reset_bus(sc);
			xs->error = XS_BUSY;
			goto done;

		case QHSTA_M_SXFR_XFR_PH_ERR:
			aprint_error_dev(sc->sc_dev, "Transfer Error\n");
			xs->error = XS_DRIVER_STUFFUP;
			break;

		case QHSTA_M_BAD_CMPL_STATUS_IN:
			/* No command complete after a status message */
			printf("%s: Bad Completion Status\n",
				device_xname(sc->sc_dev));
			xs->error = XS_DRIVER_STUFFUP;
			break;

		case QHSTA_M_AUTO_REQ_SENSE_FAIL:
			aprint_error_dev(sc->sc_dev, "Auto Sense Failed\n");
			xs->error = XS_DRIVER_STUFFUP;
			break;

		case QHSTA_M_INVALID_DEVICE:
			aprint_error_dev(sc->sc_dev, "Invalid Device\n");
			xs->error = XS_DRIVER_STUFFUP;
			break;

		case QHSTA_M_NO_AUTO_REQ_SENSE:
			/*
			 * User didn't request sense, but we got a
			 * check condition.
			 */
			aprint_error_dev(sc->sc_dev, "Unexpected Check Condition\n");
			xs->error = XS_DRIVER_STUFFUP;
			break;

		case QHSTA_M_SXFR_UNKNOWN_ERROR:
			aprint_error_dev(sc->sc_dev, "Unknown Error\n");
			xs->error = XS_DRIVER_STUFFUP;
			break;

		default:
			panic("%s: Unhandled Host Status Error %x",
			      device_xname(sc->sc_dev), scsiq->host_status);
		}
	}

	TAILQ_REMOVE(&sc->sc_pending_ccb, ccb, chain);
done:	adw_free_ccb(sc, ccb);
	scsipi_done(xs);
}


/*
 * adw_async_callback() - Adv Library asynchronous event callback function.
 */
static void
adw_async_callback(ADW_SOFTC *sc, u_int8_t code)
{
	switch (code) {
	case ADV_ASYNC_SCSI_BUS_RESET_DET:
		/* The firmware detected a SCSI Bus reset. */
		printf("%s: SCSI Bus reset detected\n", device_xname(sc->sc_dev));
		break;

	case ADV_ASYNC_RDMA_FAILURE:
		/*
		 * Handle RDMA failure by resetting the SCSI Bus and
		 * possibly the chip if it is unresponsive.
		 */
		printf("%s: RDMA failure. Resetting the SCSI Bus and"
				" the adapter\n", device_xname(sc->sc_dev));
		AdwResetSCSIBus(sc);
		break;

	case ADV_HOST_SCSI_BUS_RESET:
		/* Host generated SCSI bus reset occurred. */
		printf("%s: Host generated SCSI bus reset occurred\n",
				device_xname(sc->sc_dev));
		break;

	case ADV_ASYNC_CARRIER_READY_FAILURE:
		/* Carrier Ready failure. */
		printf("%s: Carrier Ready failure!\n", device_xname(sc->sc_dev));
		break;

	default:
		break;
	}
}
