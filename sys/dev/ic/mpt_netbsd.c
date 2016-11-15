/*	$NetBSD: mpt_netbsd.c,v 1.32 2015/07/22 08:33:51 hannken Exp $	*/

/*
 * Copyright (c) 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 2000, 2001 by Greg Ansley
 * Partially derived from Matt Jacob's ISP driver.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */
/*
 * Additional Copyright (c) 2002 by Matthew Jacob under same license.
 */

/*
 * mpt_netbsd.c:
 *
 * NetBSD-specific routines for LSI Fusion adapters.  Includes some
 * bus_dma glue, and SCSIPI glue.
 *
 * Adapted from the FreeBSD "mpt" driver by Jason R. Thorpe for
 * Wasabi Systems, Inc.
 *
 * Additional contributions by Garrett D'Amore on behalf of TELES AG.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mpt_netbsd.c,v 1.32 2015/07/22 08:33:51 hannken Exp $");

#include "bio.h"

#include <dev/ic/mpt.h>			/* pulls in all headers */
#include <sys/scsiio.h>

#if NBIO > 0
#include <dev/biovar.h>
#endif

static int	mpt_poll(mpt_softc_t *, struct scsipi_xfer *, int);
static void	mpt_timeout(void *);
static void	mpt_restart(mpt_softc_t *, request_t *);
static void	mpt_done(mpt_softc_t *, uint32_t);
static int	mpt_drain_queue(mpt_softc_t *);
static void	mpt_run_xfer(mpt_softc_t *, struct scsipi_xfer *);
static void	mpt_set_xfer_mode(mpt_softc_t *, struct scsipi_xfer_mode *);
static void	mpt_get_xfer_mode(mpt_softc_t *, struct scsipi_periph *);
static void	mpt_ctlop(mpt_softc_t *, void *vmsg, uint32_t);
static void	mpt_event_notify_reply(mpt_softc_t *, MSG_EVENT_NOTIFY_REPLY *);
static void  mpt_bus_reset(mpt_softc_t *);

static void	mpt_scsipi_request(struct scsipi_channel *,
		    scsipi_adapter_req_t, void *);
static void	mpt_minphys(struct buf *);
static int 	mpt_ioctl(struct scsipi_channel *, u_long, void *, int,
	struct proc *);

#if NBIO > 0
static bool	mpt_is_raid(mpt_softc_t *);
static int	mpt_bio_ioctl(device_t, u_long, void *);
static int	mpt_bio_ioctl_inq(mpt_softc_t *, struct bioc_inq *);
static int	mpt_bio_ioctl_vol(mpt_softc_t *, struct bioc_vol *);
static int	mpt_bio_ioctl_disk(mpt_softc_t *, struct bioc_disk *);
static int	mpt_bio_ioctl_disk_novol(mpt_softc_t *, struct bioc_disk *);
static int	mpt_bio_ioctl_setstate(mpt_softc_t *, struct bioc_setstate *);
#endif

void
mpt_scsipi_attach(mpt_softc_t *mpt)
{
	struct scsipi_adapter *adapt = &mpt->sc_adapter;
	struct scsipi_channel *chan = &mpt->sc_channel;
	int maxq;

	mpt->bus = 0;		/* XXX ?? */

	maxq = (mpt->mpt_global_credits < MPT_MAX_REQUESTS(mpt)) ?
	    mpt->mpt_global_credits : MPT_MAX_REQUESTS(mpt);

	/* Fill in the scsipi_adapter. */
	memset(adapt, 0, sizeof(*adapt));
	adapt->adapt_dev = mpt->sc_dev;
	adapt->adapt_nchannels = 1;
	adapt->adapt_openings = maxq - 2;	/* Reserve 2 for driver use*/
	adapt->adapt_max_periph = maxq - 2;
	adapt->adapt_request = mpt_scsipi_request;
	adapt->adapt_minphys = mpt_minphys;
	adapt->adapt_ioctl = mpt_ioctl;

	/* Fill in the scsipi_channel. */
	memset(chan, 0, sizeof(*chan));
	chan->chan_adapter = adapt;
	if (mpt->is_sas) {
		chan->chan_bustype = &scsi_sas_bustype;
	} else if (mpt->is_fc) {
		chan->chan_bustype = &scsi_fc_bustype;
	} else {
		chan->chan_bustype = &scsi_bustype;
	}
	chan->chan_channel = 0;
	chan->chan_flags = 0;
	chan->chan_nluns = 8;
	chan->chan_ntargets = mpt->mpt_max_devices;
	chan->chan_id = mpt->mpt_ini_id;

	/*
	* Save the output of the config so we can rescan the bus in case of 
	* errors
	*/
	mpt->sc_scsibus_dv = config_found(mpt->sc_dev, &mpt->sc_channel, 
	scsiprint);

#if NBIO > 0
	if (mpt_is_raid(mpt)) {
		if (bio_register(mpt->sc_dev, mpt_bio_ioctl) != 0)
			panic("%s: controller registration failed",
			    device_xname(mpt->sc_dev));
	}
#endif
}

int
mpt_dma_mem_alloc(mpt_softc_t *mpt)
{
	bus_dma_segment_t reply_seg, request_seg;
	int reply_rseg, request_rseg;
	bus_addr_t pptr, end;
	char *vptr;
	size_t len;
	int error, i;

	/* Check if we have already allocated the reply memory. */
	if (mpt->reply != NULL)
		return (0);

	/*
	 * Allocate the request pool.  This isn't really DMA'd memory,
	 * but it's a convenient place to do it.
	 */
	len = sizeof(request_t) * MPT_MAX_REQUESTS(mpt);
	mpt->request_pool = malloc(len, M_DEVBUF, M_WAITOK | M_ZERO);
	if (mpt->request_pool == NULL) {
		aprint_error_dev(mpt->sc_dev, "unable to allocate request pool\n");
		return (ENOMEM);
	}

	/*
	 * Allocate DMA resources for reply buffers.
	 */
	error = bus_dmamem_alloc(mpt->sc_dmat, PAGE_SIZE, PAGE_SIZE, 0,
	    &reply_seg, 1, &reply_rseg, 0);
	if (error) {
		aprint_error_dev(mpt->sc_dev, "unable to allocate reply area, error = %d\n",
		    error);
		goto fail_0;
	}

	error = bus_dmamem_map(mpt->sc_dmat, &reply_seg, reply_rseg, PAGE_SIZE,
	    (void **) &mpt->reply, BUS_DMA_COHERENT/*XXX*/);
	if (error) {
		aprint_error_dev(mpt->sc_dev, "unable to map reply area, error = %d\n",
		    error);
		goto fail_1;
	}

	error = bus_dmamap_create(mpt->sc_dmat, PAGE_SIZE, 1, PAGE_SIZE,
	    0, 0, &mpt->reply_dmap);
	if (error) {
		aprint_error_dev(mpt->sc_dev, "unable to create reply DMA map, error = %d\n",
		    error);
		goto fail_2;
	}

	error = bus_dmamap_load(mpt->sc_dmat, mpt->reply_dmap, mpt->reply,
	    PAGE_SIZE, NULL, 0);
	if (error) {
		aprint_error_dev(mpt->sc_dev, "unable to load reply DMA map, error = %d\n",
		    error);
		goto fail_3;
	}
	mpt->reply_phys = mpt->reply_dmap->dm_segs[0].ds_addr;

	/*
	 * Allocate DMA resources for request buffers.
	 */
	error = bus_dmamem_alloc(mpt->sc_dmat, MPT_REQ_MEM_SIZE(mpt),
	    PAGE_SIZE, 0, &request_seg, 1, &request_rseg, 0);
	if (error) {
		aprint_error_dev(mpt->sc_dev, "unable to allocate request area, "
		    "error = %d\n", error);
		goto fail_4;
	}

	error = bus_dmamem_map(mpt->sc_dmat, &request_seg, request_rseg,
	    MPT_REQ_MEM_SIZE(mpt), (void **) &mpt->request, 0);
	if (error) {
		aprint_error_dev(mpt->sc_dev, "unable to map request area, error = %d\n",
		    error);
		goto fail_5;
	}

	error = bus_dmamap_create(mpt->sc_dmat, MPT_REQ_MEM_SIZE(mpt), 1,
	    MPT_REQ_MEM_SIZE(mpt), 0, 0, &mpt->request_dmap);
	if (error) {
		aprint_error_dev(mpt->sc_dev, "unable to create request DMA map, "
		    "error = %d\n", error);
		goto fail_6;
	}

	error = bus_dmamap_load(mpt->sc_dmat, mpt->request_dmap, mpt->request,
	    MPT_REQ_MEM_SIZE(mpt), NULL, 0);
	if (error) {
		aprint_error_dev(mpt->sc_dev, "unable to load request DMA map, error = %d\n",
		    error);
		goto fail_7;
	}
	mpt->request_phys = mpt->request_dmap->dm_segs[0].ds_addr;

	pptr = mpt->request_phys;
	vptr = (void *) mpt->request;
	end = pptr + MPT_REQ_MEM_SIZE(mpt);

	for (i = 0; pptr < end; i++) {
		request_t *req = &mpt->request_pool[i];
		req->index = i;

		/* Store location of Request Data */
		req->req_pbuf = pptr;
		req->req_vbuf = vptr;

		pptr += MPT_REQUEST_AREA;
		vptr += MPT_REQUEST_AREA;

		req->sense_pbuf = (pptr - MPT_SENSE_SIZE);
		req->sense_vbuf = (vptr - MPT_SENSE_SIZE);

		error = bus_dmamap_create(mpt->sc_dmat, MAXPHYS,
		    MPT_SGL_MAX, MAXPHYS, 0, 0, &req->dmap);
		if (error) {
			aprint_error_dev(mpt->sc_dev, "unable to create req %d DMA map, "
			    "error = %d\n", i, error);
			goto fail_8;
		}
	}

	return (0);

 fail_8:
	for (--i; i >= 0; i--) {
		request_t *req = &mpt->request_pool[i];
		if (req->dmap != NULL)
			bus_dmamap_destroy(mpt->sc_dmat, req->dmap);
	}
	bus_dmamap_unload(mpt->sc_dmat, mpt->request_dmap);
 fail_7:
	bus_dmamap_destroy(mpt->sc_dmat, mpt->request_dmap);
 fail_6:
	bus_dmamem_unmap(mpt->sc_dmat, (void *)mpt->request, PAGE_SIZE);
 fail_5:
	bus_dmamem_free(mpt->sc_dmat, &request_seg, request_rseg);
 fail_4:
	bus_dmamap_unload(mpt->sc_dmat, mpt->reply_dmap);
 fail_3:
	bus_dmamap_destroy(mpt->sc_dmat, mpt->reply_dmap);
 fail_2:
	bus_dmamem_unmap(mpt->sc_dmat, (void *)mpt->reply, PAGE_SIZE);
 fail_1:
	bus_dmamem_free(mpt->sc_dmat, &reply_seg, reply_rseg);
 fail_0:
	free(mpt->request_pool, M_DEVBUF);

	mpt->reply = NULL;
	mpt->request = NULL;
	mpt->request_pool = NULL;

	return (error);
}

int
mpt_intr(void *arg)
{
	mpt_softc_t *mpt = arg;
	int nrepl = 0;

	if ((mpt_read(mpt, MPT_OFFSET_INTR_STATUS) & MPT_INTR_REPLY_READY) == 0)
		return (0);

	nrepl = mpt_drain_queue(mpt);
	return (nrepl != 0);
}

void
mpt_prt(mpt_softc_t *mpt, const char *fmt, ...)
{
	va_list ap;

	printf("%s: ", device_xname(mpt->sc_dev));
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");
}

static int
mpt_poll(mpt_softc_t *mpt, struct scsipi_xfer *xs, int count)
{

	/* Timeouts are in msec, so we loop in 1000usec cycles */
	while (count) {
		mpt_intr(mpt);
		if (xs->xs_status & XS_STS_DONE)
			return (0);
		delay(1000);		/* only happens in boot, so ok */
		count--;
	}
	return (1);
}

static void
mpt_timeout(void *arg)
{
	request_t *req = arg;
	struct scsipi_xfer *xs;
	struct scsipi_periph *periph;
	mpt_softc_t *mpt;
 	uint32_t oseq;
	int s, nrepl = 0;
 
	if (req->xfer  == NULL) {
		printf("mpt_timeout: NULL xfer for request index 0x%x, sequenc 0x%x\n",
		req->index, req->sequence);
		return;
	}
	xs = req->xfer;
	periph = xs->xs_periph;
	mpt = device_private(periph->periph_channel->chan_adapter->adapt_dev);
	scsipi_printaddr(periph);
	printf("command timeout\n");

	s = splbio();

	oseq = req->sequence;
	mpt->timeouts++;
	if (mpt_intr(mpt)) {
		if (req->sequence != oseq) {
			mpt->success++;
			mpt_prt(mpt, "recovered from command timeout");
			splx(s);
			return;
		}
	}

	/*
	 * Ensure the IOC is really done giving us data since it appears it can
	 * sometimes fail to give us interrupts under heavy load.
	 */
	nrepl = mpt_drain_queue(mpt);
	if (nrepl ) {
		mpt_prt(mpt, "mpt_timeout: recovered %d commands",nrepl);
	}

	if (req->sequence != oseq) {
		mpt->success++;
		splx(s);
		return;
	}

	mpt_prt(mpt,
	    "timeout on request index = 0x%x, seq = 0x%08x",
	    req->index, req->sequence);
	mpt_check_doorbell(mpt);
	mpt_prt(mpt, "Status 0x%08x, Mask 0x%08x, Doorbell 0x%08x",
	    mpt_read(mpt, MPT_OFFSET_INTR_STATUS),
	    mpt_read(mpt, MPT_OFFSET_INTR_MASK),
	    mpt_read(mpt, MPT_OFFSET_DOORBELL));
	mpt_prt(mpt, "request state: %s", mpt_req_state(req->debug));
	if (mpt->verbose > 1)
		mpt_print_scsi_io_request((MSG_SCSI_IO_REQUEST *)req->req_vbuf);

	xs->error = XS_TIMEOUT;
	splx(s);
	mpt_restart(mpt, req);
}

static void
mpt_restart(mpt_softc_t *mpt, request_t *req0)
{
	int i, s, nreq;
	request_t *req;
	struct scsipi_xfer *xs;

	/* first, reset the IOC, leaving stopped so all requests are idle */
	if (mpt_soft_reset(mpt) != MPT_OK) {
		mpt_prt(mpt, "soft reset failed");
		/* 
		* Don't try a hard reset since this mangles the PCI 
		* configuration registers.
		*/
		return;
	}

	/* Freeze the channel so scsipi doesn't queue more commands. */
	scsipi_channel_freeze(&mpt->sc_channel, 1);

	/* Return all pending requests to scsipi and de-allocate them. */
	s = splbio();
	nreq = 0;
	for (i = 0; i < MPT_MAX_REQUESTS(mpt); i++) {
		req = &mpt->request_pool[i];
		xs = req->xfer;
		if (xs != NULL) {
			if (xs->datalen != 0)
				bus_dmamap_unload(mpt->sc_dmat, req->dmap);
			req->xfer = NULL;
			callout_stop(&xs->xs_callout);
			if (req != req0) {
				nreq++;
				xs->error = XS_REQUEUE;
			}
			scsipi_done(xs);
			/*
			* Don't need to mpt_free_request() since mpt_init() 
			* below will free all requests anyway.
			*/
			mpt_free_request(mpt, req);
		}
	}
	splx(s);
	if (nreq > 0)
		mpt_prt(mpt, "re-queued %d requests", nreq);

	/* Re-initialize the IOC (which restarts it). */
	if (mpt_init(mpt, MPT_DB_INIT_HOST) == 0)
		mpt_prt(mpt, "restart succeeded");
	/* else error message already printed */

	/* Thaw the channel, causing scsipi to re-queue the commands. */
	scsipi_channel_thaw(&mpt->sc_channel, 1);
}

static int 
mpt_drain_queue(mpt_softc_t *mpt)
{
	int nrepl = 0;
	uint32_t reply;

	reply = mpt_pop_reply_queue(mpt);
	while (reply != MPT_REPLY_EMPTY) {
		nrepl++;
		if (mpt->verbose > 1) {
			if ((reply & MPT_CONTEXT_REPLY) != 0) {
				/* Address reply; IOC has something to say */
				mpt_print_reply(MPT_REPLY_PTOV(mpt, reply));
			} else {
				/* Context reply; all went well */
				mpt_prt(mpt, "context %u reply OK", reply);
			}
		}
		mpt_done(mpt, reply);
		reply = mpt_pop_reply_queue(mpt);
	}
	return (nrepl);
}

static void
mpt_done(mpt_softc_t *mpt, uint32_t reply)
{
	struct scsipi_xfer *xs = NULL;
	struct scsipi_periph *periph;
	int index;
	request_t *req;
	MSG_REQUEST_HEADER *mpt_req;
	MSG_SCSI_IO_REPLY *mpt_reply;
	int restart = 0; /* nonzero if we need to restart the IOC*/

	if (__predict_true((reply & MPT_CONTEXT_REPLY) == 0)) {
		/* context reply (ok) */
		mpt_reply = NULL;
		index = reply & MPT_CONTEXT_MASK;
	} else {
		/* address reply (error) */

		/* XXX BUS_DMASYNC_POSTREAD XXX */
		mpt_reply = MPT_REPLY_PTOV(mpt, reply);
		if (mpt_reply != NULL) {
			if (mpt->verbose > 1) {
				uint32_t *pReply = (uint32_t *) mpt_reply;

				mpt_prt(mpt, "Address Reply (index %u):",
				    le32toh(mpt_reply->MsgContext) & 0xffff);
				mpt_prt(mpt, "%08x %08x %08x %08x", pReply[0],
				    pReply[1], pReply[2], pReply[3]);
				mpt_prt(mpt, "%08x %08x %08x %08x", pReply[4],
				    pReply[5], pReply[6], pReply[7]);
				mpt_prt(mpt, "%08x %08x %08x %08x", pReply[8],
				    pReply[9], pReply[10], pReply[11]);
			}
			index = le32toh(mpt_reply->MsgContext);
		} else
			index = reply & MPT_CONTEXT_MASK;
	}

	/*
	 * Address reply with MessageContext high bit set.
	 * This is most likely a notify message, so we try
	 * to process it, then free it.
	 */
	if (__predict_false((index & 0x80000000) != 0)) {
		if (mpt_reply != NULL)
			mpt_ctlop(mpt, mpt_reply, reply);
		else
			mpt_prt(mpt, "%s: index 0x%x, NULL reply", __func__,
			    index);
		return;
	}

	/* Did we end up with a valid index into the table? */
	if (__predict_false(index < 0 || index >= MPT_MAX_REQUESTS(mpt))) {
		mpt_prt(mpt, "%s: invalid index (0x%x) in reply", __func__,
		    index);
		return;
	}

	req = &mpt->request_pool[index];

	/* Make sure memory hasn't been trashed. */
	if (__predict_false(req->index != index)) {
		mpt_prt(mpt, "%s: corrupted request_t (0x%x)", __func__,
		    index);
		return;
	}

	MPT_SYNC_REQ(mpt, req, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
	mpt_req = req->req_vbuf;

	/* Short cut for task management replies; nothing more for us to do. */
	if (__predict_false(mpt_req->Function == MPI_FUNCTION_SCSI_TASK_MGMT)) {
		if (mpt->verbose > 1)
			mpt_prt(mpt, "%s: TASK MGMT", __func__);
		KASSERT(req == mpt->mngt_req);
		mpt->mngt_req = NULL;
		goto done;
	}

	if (__predict_false(mpt_req->Function == MPI_FUNCTION_PORT_ENABLE))
		goto done;

	/*
	 * At this point, it had better be a SCSI I/O command, but don't
	 * crash if it isn't.
	 */
	if (__predict_false(mpt_req->Function !=
			    MPI_FUNCTION_SCSI_IO_REQUEST)) {
		if (mpt->verbose > 1)
			mpt_prt(mpt, "%s: unknown Function 0x%x (0x%x)",
			    __func__, mpt_req->Function, index);
		goto done;
	}

	/* Recover scsipi_xfer from the request structure. */
	xs = req->xfer;

	/* Can't have a SCSI command without a scsipi_xfer. */
	if (__predict_false(xs == NULL)) {
		mpt_prt(mpt,
		    "%s: no scsipi_xfer, index = 0x%x, seq = 0x%08x", __func__,
		    req->index, req->sequence);
		mpt_prt(mpt, "request state: %s", mpt_req_state(req->debug));
		mpt_prt(mpt, "mpt_request:");
		mpt_print_scsi_io_request((MSG_SCSI_IO_REQUEST *)req->req_vbuf);

		if (mpt_reply != NULL) {
			mpt_prt(mpt, "mpt_reply:");
			mpt_print_reply(mpt_reply);
		} else {
			mpt_prt(mpt, "context reply: 0x%08x", reply);
		}
		goto done;
	}

	callout_stop(&xs->xs_callout);

	periph = xs->xs_periph;

	/*
	 * If we were a data transfer, unload the map that described
	 * the data buffer.
	 */
	if (__predict_true(xs->datalen != 0)) {
		bus_dmamap_sync(mpt->sc_dmat, req->dmap, 0,
		    req->dmap->dm_mapsize,
		    (xs->xs_control & XS_CTL_DATA_IN) ? BUS_DMASYNC_POSTREAD
						      : BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(mpt->sc_dmat, req->dmap);
	}

	if (__predict_true(mpt_reply == NULL)) {
		/*
		 * Context reply; report that the command was
		 * successful!
		 *
		 * Also report the xfer mode, if necessary.
		 */
		if (__predict_false(mpt->mpt_report_xfer_mode != 0)) {
			if ((mpt->mpt_report_xfer_mode &
			     (1 << periph->periph_target)) != 0)
				mpt_get_xfer_mode(mpt, periph);
		}
		xs->error = XS_NOERROR;
		xs->status = SCSI_OK;
		xs->resid = 0;
		mpt_free_request(mpt, req);
		scsipi_done(xs);
		return;
	}

	xs->status = mpt_reply->SCSIStatus;
	switch (le16toh(mpt_reply->IOCStatus) & MPI_IOCSTATUS_MASK) {
	case MPI_IOCSTATUS_SCSI_DATA_OVERRUN:
		xs->error = XS_DRIVER_STUFFUP;
		mpt_prt(mpt, "%s: IOC overrun!", __func__);
		break;

	case MPI_IOCSTATUS_SCSI_DATA_UNDERRUN:
		/*
		 * Yikes!  Tagged queue full comes through this path!
		 *
		 * So we'll change it to a status error and anything
		 * that returns status should probably be a status
		 * error as well.
		 */
		xs->resid = xs->datalen - le32toh(mpt_reply->TransferCount);
		if (mpt_reply->SCSIState &
		    MPI_SCSI_STATE_NO_SCSI_STATUS) {
			xs->error = XS_DRIVER_STUFFUP;
			break;
		}
		/* FALLTHROUGH */
	case MPI_IOCSTATUS_SUCCESS:
	case MPI_IOCSTATUS_SCSI_RECOVERED_ERROR:
		switch (xs->status) {
		case SCSI_OK:
			/* Report the xfer mode, if necessary. */
			if ((mpt->mpt_report_xfer_mode &
			     (1 << periph->periph_target)) != 0)
				mpt_get_xfer_mode(mpt, periph);
			xs->resid = 0;
			break;

		case SCSI_CHECK:
			xs->error = XS_SENSE;
			break;

		case SCSI_BUSY:
		case SCSI_QUEUE_FULL:
			xs->error = XS_BUSY;
			break;

		default:
			scsipi_printaddr(periph);
			printf("invalid status code %d\n", xs->status);
			xs->error = XS_DRIVER_STUFFUP;
			break;
		}
		break;

	case MPI_IOCSTATUS_BUSY:
	case MPI_IOCSTATUS_INSUFFICIENT_RESOURCES:
		xs->error = XS_RESOURCE_SHORTAGE;
		break;

	case MPI_IOCSTATUS_SCSI_INVALID_BUS:
	case MPI_IOCSTATUS_SCSI_INVALID_TARGETID:
	case MPI_IOCSTATUS_SCSI_DEVICE_NOT_THERE:
		xs->error = XS_SELTIMEOUT;
		break;

	case MPI_IOCSTATUS_SCSI_RESIDUAL_MISMATCH:
		xs->error = XS_DRIVER_STUFFUP;
		mpt_prt(mpt, "%s: IOC SCSI residual mismatch!", __func__);
		restart = 1;
		break;

	case MPI_IOCSTATUS_SCSI_TASK_TERMINATED:
		/* XXX What should we do here? */
		mpt_prt(mpt, "%s: IOC SCSI task terminated!", __func__);
		restart = 1;
		break;

	case MPI_IOCSTATUS_SCSI_TASK_MGMT_FAILED:
		/* XXX */
		xs->error = XS_DRIVER_STUFFUP;
		mpt_prt(mpt, "%s: IOC SCSI task failed!", __func__);
		restart = 1;
		break;

	case MPI_IOCSTATUS_SCSI_IOC_TERMINATED:
		/* XXX */
		xs->error = XS_DRIVER_STUFFUP;
		mpt_prt(mpt, "%s: IOC task terminated!", __func__);
		restart = 1;
		break;

	case MPI_IOCSTATUS_SCSI_EXT_TERMINATED:
		/* XXX This is a bus-reset */
		xs->error = XS_DRIVER_STUFFUP;
		mpt_prt(mpt, "%s: IOC SCSI bus reset!", __func__);
		restart = 1;
		break;

	case MPI_IOCSTATUS_SCSI_PROTOCOL_ERROR:
		/*
		 * FreeBSD and Linux indicate this is a phase error between
		 * the IOC and the drive itself. When this happens, the IOC
		 * becomes unhappy and stops processing all transactions.  
		 * Call mpt_timeout which knows how to get the IOC back
		 * on its feet.
		 */
		 mpt_prt(mpt, "%s: IOC indicates protocol error -- "
		     "recovering...", __func__);
		xs->error = XS_TIMEOUT;
		restart = 1;

		break;

	default:
		/* XXX unrecognized HBA error */
		xs->error = XS_DRIVER_STUFFUP;
		mpt_prt(mpt, "%s: IOC returned unknown code: 0x%x", __func__,
		    le16toh(mpt_reply->IOCStatus));
		restart = 1;
		break;
	}

	if (mpt_reply != NULL) {
		if (mpt_reply->SCSIState & MPI_SCSI_STATE_AUTOSENSE_VALID) {
			memcpy(&xs->sense.scsi_sense, req->sense_vbuf,
			    sizeof(xs->sense.scsi_sense));
		} else if (mpt_reply->SCSIState &
		    MPI_SCSI_STATE_AUTOSENSE_FAILED) {
			/*
			 * This will cause the scsipi layer to issue
			 * a REQUEST SENSE.
			 */
			if (xs->status == SCSI_CHECK)
				xs->error = XS_BUSY;
		}
	}

 done:
	if (mpt_reply != NULL && le16toh(mpt_reply->IOCStatus) & 
	MPI_IOCSTATUS_FLAG_LOG_INFO_AVAILABLE) {
		mpt_prt(mpt, "%s: IOC has error - logging...\n", __func__);
		mpt_ctlop(mpt, mpt_reply, reply);
	}

	/* If IOC done with this request, free it up. */
	if (mpt_reply == NULL || (mpt_reply->MsgFlags & 0x80) == 0)
		mpt_free_request(mpt, req);

	/* If address reply, give the buffer back to the IOC. */
	if (mpt_reply != NULL)
		mpt_free_reply(mpt, (reply << 1));

	if (xs != NULL)
		scsipi_done(xs);

	if (restart) {
		mpt_prt(mpt, "%s: IOC fatal error: restarting...", __func__);
		mpt_restart(mpt, NULL);
	}
}

static void
mpt_run_xfer(mpt_softc_t *mpt, struct scsipi_xfer *xs)
{
	struct scsipi_periph *periph = xs->xs_periph;
	request_t *req;
	MSG_SCSI_IO_REQUEST *mpt_req;
	int error, s;

	s = splbio();
	req = mpt_get_request(mpt);
	if (__predict_false(req == NULL)) {
		/* This should happen very infrequently. */
		xs->error = XS_RESOURCE_SHORTAGE;
		scsipi_done(xs);
		splx(s);
		return;
	}
	splx(s);

	/* Link the req and the scsipi_xfer. */
	req->xfer = xs;

	/* Now we build the command for the IOC */
	mpt_req = req->req_vbuf;
	memset(mpt_req, 0, sizeof(*mpt_req));

	mpt_req->Function = MPI_FUNCTION_SCSI_IO_REQUEST;
	mpt_req->Bus = mpt->bus;

	mpt_req->SenseBufferLength =
	    (sizeof(xs->sense.scsi_sense) < MPT_SENSE_SIZE) ?
	    sizeof(xs->sense.scsi_sense) : MPT_SENSE_SIZE;

	/*
	 * We use the message context to find the request structure when
	 * we get the command completion interrupt from the IOC.
	 */
	mpt_req->MsgContext = htole32(req->index);

	/* Which physical device to do the I/O on. */
	mpt_req->TargetID = periph->periph_target;
	mpt_req->LUN[1] = periph->periph_lun;

	/* Set the direction of the transfer. */
	if (xs->xs_control & XS_CTL_DATA_IN)
		mpt_req->Control = MPI_SCSIIO_CONTROL_READ;
	else if (xs->xs_control & XS_CTL_DATA_OUT)
		mpt_req->Control = MPI_SCSIIO_CONTROL_WRITE;
	else
		mpt_req->Control = MPI_SCSIIO_CONTROL_NODATATRANSFER;

	/* Set the queue behavior. */
	if (__predict_true((!mpt->is_scsi) ||
			   (mpt->mpt_tag_enable &
			    (1 << periph->periph_target)))) {
		switch (XS_CTL_TAGTYPE(xs)) {
		case XS_CTL_HEAD_TAG:
			mpt_req->Control |= MPI_SCSIIO_CONTROL_HEADOFQ;
			break;

#if 0	/* XXX */
		case XS_CTL_ACA_TAG:
			mpt_req->Control |= MPI_SCSIIO_CONTROL_ACAQ;
			break;
#endif

		case XS_CTL_ORDERED_TAG:
			mpt_req->Control |= MPI_SCSIIO_CONTROL_ORDEREDQ;
			break;

		case XS_CTL_SIMPLE_TAG:
			mpt_req->Control |= MPI_SCSIIO_CONTROL_SIMPLEQ;
			break;

		default:
			if (mpt->is_scsi)
				mpt_req->Control |= MPI_SCSIIO_CONTROL_UNTAGGED;
			else
				mpt_req->Control |= MPI_SCSIIO_CONTROL_SIMPLEQ;
			break;
		}
	} else
		mpt_req->Control |= MPI_SCSIIO_CONTROL_UNTAGGED;

	if (__predict_false(mpt->is_scsi &&
			    (mpt->mpt_disc_enable &
			     (1 << periph->periph_target)) == 0))
		mpt_req->Control |= MPI_SCSIIO_CONTROL_NO_DISCONNECT;

	mpt_req->Control = htole32(mpt_req->Control);

	/* Copy the SCSI command block into place. */
	memcpy(mpt_req->CDB, xs->cmd, xs->cmdlen);

	mpt_req->CDBLength = xs->cmdlen;
	mpt_req->DataLength = htole32(xs->datalen);
	mpt_req->SenseBufferLowAddr = htole32(req->sense_pbuf);

	/*
	 * Map the DMA transfer.
	 */
	if (xs->datalen) {
		SGE_SIMPLE32 *se;

		error = bus_dmamap_load(mpt->sc_dmat, req->dmap, xs->data,
		    xs->datalen, NULL,
		    ((xs->xs_control & XS_CTL_NOSLEEP) ? BUS_DMA_NOWAIT
						       : BUS_DMA_WAITOK) |
		    BUS_DMA_STREAMING |
		    ((xs->xs_control & XS_CTL_DATA_IN) ? BUS_DMA_READ
						       : BUS_DMA_WRITE));
		switch (error) {
		case 0:
			break;

		case ENOMEM:
		case EAGAIN:
			xs->error = XS_RESOURCE_SHORTAGE;
			goto out_bad;

		default:
			xs->error = XS_DRIVER_STUFFUP;
			mpt_prt(mpt, "error %d loading DMA map", error);
 out_bad:
			s = splbio();
			mpt_free_request(mpt, req);
			scsipi_done(xs);
			splx(s);
			return;
		}

		if (req->dmap->dm_nsegs > MPT_NSGL_FIRST(mpt)) {
			int seg, i, nleft = req->dmap->dm_nsegs;
			uint32_t flags;
			SGE_CHAIN32 *ce;

			seg = 0;
			flags = MPI_SGE_FLAGS_SIMPLE_ELEMENT;
			if (xs->xs_control & XS_CTL_DATA_OUT)
				flags |= MPI_SGE_FLAGS_HOST_TO_IOC;

			se = (SGE_SIMPLE32 *) &mpt_req->SGL;
			for (i = 0; i < MPT_NSGL_FIRST(mpt) - 1;
			     i++, se++, seg++) {
				uint32_t tf;

				memset(se, 0, sizeof(*se));
				se->Address =
				    htole32(req->dmap->dm_segs[seg].ds_addr);
				MPI_pSGE_SET_LENGTH(se,
				    req->dmap->dm_segs[seg].ds_len);
				tf = flags;
				if (i == MPT_NSGL_FIRST(mpt) - 2)
					tf |= MPI_SGE_FLAGS_LAST_ELEMENT;
				MPI_pSGE_SET_FLAGS(se, tf);
				se->FlagsLength = htole32(se->FlagsLength);
				nleft--;
			}

			/*
			 * Tell the IOC where to find the first chain element.
			 */
			mpt_req->ChainOffset =
			    ((char *)se - (char *)mpt_req) >> 2;

			/*
			 * Until we're finished with all segments...
			 */
			while (nleft) {
				int ntodo;

				/*
				 * Construct the chain element that points to
				 * the next segment.
				 */
				ce = (SGE_CHAIN32 *) se++;
				if (nleft > MPT_NSGL(mpt)) {
					ntodo = MPT_NSGL(mpt) - 1;
					ce->NextChainOffset = (MPT_RQSL(mpt) -
					    sizeof(SGE_SIMPLE32)) >> 2;
					ce->Length = htole16(MPT_NSGL(mpt)
						* sizeof(SGE_SIMPLE32));
				} else {
					ntodo = nleft;
					ce->NextChainOffset = 0;
					ce->Length = htole16(ntodo
						* sizeof(SGE_SIMPLE32));
				}
				ce->Address = htole32(req->req_pbuf +
				    ((char *)se - (char *)mpt_req));
				ce->Flags = MPI_SGE_FLAGS_CHAIN_ELEMENT;
				for (i = 0; i < ntodo; i++, se++, seg++) {
					uint32_t tf;

					memset(se, 0, sizeof(*se));
					se->Address = htole32(
					    req->dmap->dm_segs[seg].ds_addr);
					MPI_pSGE_SET_LENGTH(se,
					    req->dmap->dm_segs[seg].ds_len);
					tf = flags;
					if (i == ntodo - 1) {
						tf |=
						    MPI_SGE_FLAGS_LAST_ELEMENT;
						if (ce->NextChainOffset == 0) {
							tf |=
						    MPI_SGE_FLAGS_END_OF_LIST |
						    MPI_SGE_FLAGS_END_OF_BUFFER;
						}
					}
					MPI_pSGE_SET_FLAGS(se, tf);
					se->FlagsLength =
					    htole32(se->FlagsLength);
					nleft--;
				}
			}
			bus_dmamap_sync(mpt->sc_dmat, req->dmap, 0,
			    req->dmap->dm_mapsize,
			    (xs->xs_control & XS_CTL_DATA_IN) ?
			    				BUS_DMASYNC_PREREAD
						      : BUS_DMASYNC_PREWRITE);
		} else {
			int i;
			uint32_t flags;

			flags = MPI_SGE_FLAGS_SIMPLE_ELEMENT;
			if (xs->xs_control & XS_CTL_DATA_OUT)
				flags |= MPI_SGE_FLAGS_HOST_TO_IOC;

			/* Copy the segments into our SG list. */
			se = (SGE_SIMPLE32 *) &mpt_req->SGL;
			for (i = 0; i < req->dmap->dm_nsegs;
			     i++, se++) {
				uint32_t tf;

				memset(se, 0, sizeof(*se));
				se->Address =
				    htole32(req->dmap->dm_segs[i].ds_addr);
				MPI_pSGE_SET_LENGTH(se,
				    req->dmap->dm_segs[i].ds_len);
				tf = flags;
				if (i == req->dmap->dm_nsegs - 1) {
					tf |=
					    MPI_SGE_FLAGS_LAST_ELEMENT |
					    MPI_SGE_FLAGS_END_OF_BUFFER |
					    MPI_SGE_FLAGS_END_OF_LIST;
				}
				MPI_pSGE_SET_FLAGS(se, tf);
				se->FlagsLength = htole32(se->FlagsLength);
			}
			bus_dmamap_sync(mpt->sc_dmat, req->dmap, 0,
			    req->dmap->dm_mapsize,
			    (xs->xs_control & XS_CTL_DATA_IN) ?
			    				BUS_DMASYNC_PREREAD
						      : BUS_DMASYNC_PREWRITE);
		}
	} else {
		/*
		 * No data to transfer; just make a single simple SGL
		 * with zero length.
		 */
		SGE_SIMPLE32 *se = (SGE_SIMPLE32 *) &mpt_req->SGL;
		memset(se, 0, sizeof(*se));
		MPI_pSGE_SET_FLAGS(se,
		    (MPI_SGE_FLAGS_LAST_ELEMENT | MPI_SGE_FLAGS_END_OF_BUFFER |
		     MPI_SGE_FLAGS_SIMPLE_ELEMENT | MPI_SGE_FLAGS_END_OF_LIST));
		se->FlagsLength = htole32(se->FlagsLength);
	}

	if (mpt->verbose > 1)
		mpt_print_scsi_io_request(mpt_req);

		if (xs->timeout == 0) {
			mpt_prt(mpt, "mpt_run_xfer: no timeout specified for request: 0x%x\n",
			req->index);
			xs->timeout = 500;
		}

	s = splbio();
	if (__predict_true((xs->xs_control & XS_CTL_POLL) == 0))
		callout_reset(&xs->xs_callout,
		    mstohz(xs->timeout), mpt_timeout, req);
	mpt_send_cmd(mpt, req);
	splx(s);

	if (__predict_true((xs->xs_control & XS_CTL_POLL) == 0))
		return;

	/*
	 * If we can't use interrupts, poll on completion.
	 */
	if (mpt_poll(mpt, xs, xs->timeout))
		mpt_timeout(req);
}

static void
mpt_set_xfer_mode(mpt_softc_t *mpt, struct scsipi_xfer_mode *xm)
{
	fCONFIG_PAGE_SCSI_DEVICE_1 tmp;

	/*
	 * Always allow disconnect; we don't have a way to disable
	 * it right now, in any case.
	 */
	mpt->mpt_disc_enable |= (1 << xm->xm_target);

	if (xm->xm_mode & PERIPH_CAP_TQING)
		mpt->mpt_tag_enable |= (1 << xm->xm_target);
	else
		mpt->mpt_tag_enable &= ~(1 << xm->xm_target);

	if (mpt->is_scsi) {
		/*
		 * SCSI transport settings only make any sense for
		 * SCSI
		 */

		tmp = mpt->mpt_dev_page1[xm->xm_target];

		/*
		 * Set the wide/narrow parameter for the target.
		 */
		if (xm->xm_mode & PERIPH_CAP_WIDE16)
			tmp.RequestedParameters |= MPI_SCSIDEVPAGE1_RP_WIDE;
		else
			tmp.RequestedParameters &= ~MPI_SCSIDEVPAGE1_RP_WIDE;

		/*
		 * Set the synchronous parameters for the target.
		 *
		 * XXX If we request sync transfers, we just go ahead and
		 * XXX request the maximum available.  We need finer control
		 * XXX in order to implement Domain Validation.
		 */
		tmp.RequestedParameters &= ~(MPI_SCSIDEVPAGE1_RP_MIN_SYNC_PERIOD_MASK |
		    MPI_SCSIDEVPAGE1_RP_MAX_SYNC_OFFSET_MASK |
		    MPI_SCSIDEVPAGE1_RP_DT | MPI_SCSIDEVPAGE1_RP_QAS |
		    MPI_SCSIDEVPAGE1_RP_IU);
		if (xm->xm_mode & PERIPH_CAP_SYNC) {
			int factor, offset, np;

			factor = (mpt->mpt_port_page0.Capabilities >> 8) & 0xff;
			offset = (mpt->mpt_port_page0.Capabilities >> 16) & 0xff;
			np = 0;
			if (factor < 0x9) {
				/* Ultra320 */
				np |= MPI_SCSIDEVPAGE1_RP_QAS | MPI_SCSIDEVPAGE1_RP_IU;
			}
			if (factor < 0xa) {
				/* at least Ultra160 */
				np |= MPI_SCSIDEVPAGE1_RP_DT;
			}
			np |= (factor << 8) | (offset << 16);
			tmp.RequestedParameters |= np;
		}

		host2mpt_config_page_scsi_device_1(&tmp);
		if (mpt_write_cfg_page(mpt, xm->xm_target, &tmp.Header)) {
			mpt_prt(mpt, "unable to write Device Page 1");
			return;
		}

		if (mpt_read_cfg_page(mpt, xm->xm_target, &tmp.Header)) {
			mpt_prt(mpt, "unable to read back Device Page 1");
			return;
		}

		mpt2host_config_page_scsi_device_1(&tmp);
		mpt->mpt_dev_page1[xm->xm_target] = tmp;
		if (mpt->verbose > 1) {
			mpt_prt(mpt,
			    "SPI Target %d Page 1: RequestedParameters %x Config %x",
			    xm->xm_target,
			    mpt->mpt_dev_page1[xm->xm_target].RequestedParameters,
			    mpt->mpt_dev_page1[xm->xm_target].Configuration);
		}
	}

	/*
	 * Make a note that we should perform an async callback at the
	 * end of the next successful command completion to report the
	 * negotiated transfer mode.
	 */
	mpt->mpt_report_xfer_mode |= (1 << xm->xm_target);
}

static void
mpt_get_xfer_mode(mpt_softc_t *mpt, struct scsipi_periph *periph)
{
	fCONFIG_PAGE_SCSI_DEVICE_0 tmp;
	struct scsipi_xfer_mode xm;
	int period, offset;

	tmp = mpt->mpt_dev_page0[periph->periph_target];
	host2mpt_config_page_scsi_device_0(&tmp);
	if (mpt_read_cfg_page(mpt, periph->periph_target, &tmp.Header)) {
		mpt_prt(mpt, "unable to read Device Page 0");
		return;
	}
	mpt2host_config_page_scsi_device_0(&tmp);

	if (mpt->verbose > 1) {
		mpt_prt(mpt,
		    "SPI Tgt %d Page 0: NParms %x Information %x",
		    periph->periph_target,
		    tmp.NegotiatedParameters, tmp.Information);
	}

	xm.xm_target = periph->periph_target;
	xm.xm_mode = 0;

	if (tmp.NegotiatedParameters & MPI_SCSIDEVPAGE0_NP_WIDE)
		xm.xm_mode |= PERIPH_CAP_WIDE16;

	period = (tmp.NegotiatedParameters >> 8) & 0xff;
	offset = (tmp.NegotiatedParameters >> 16) & 0xff;
	if (offset) {
		xm.xm_period = period;
		xm.xm_offset = offset;
		xm.xm_mode |= PERIPH_CAP_SYNC;
	}

	/*
	 * Tagged queueing is all controlled by us; there is no
	 * other setting to query.
	 */
	if (mpt->mpt_tag_enable & (1 << periph->periph_target))
		xm.xm_mode |= PERIPH_CAP_TQING;

	/*
	 * We're going to deliver the async event, so clear the marker.
	 */
	mpt->mpt_report_xfer_mode &= ~(1 << periph->periph_target);

	scsipi_async_event(&mpt->sc_channel, ASYNC_EVENT_XFER_MODE, &xm);
}

static void
mpt_ctlop(mpt_softc_t *mpt, void *vmsg, uint32_t reply)
{
	MSG_DEFAULT_REPLY *dmsg = vmsg;

	switch (dmsg->Function) {
	case MPI_FUNCTION_EVENT_NOTIFICATION:
		mpt_event_notify_reply(mpt, vmsg);
		mpt_free_reply(mpt, (reply << 1));
		break;

	case MPI_FUNCTION_EVENT_ACK:
	    {
		MSG_EVENT_ACK_REPLY *msg = vmsg;
		int index = le32toh(msg->MsgContext) & ~0x80000000;
		mpt_free_reply(mpt, (reply << 1));
		if (index >= 0 && index < MPT_MAX_REQUESTS(mpt)) {
			request_t *req = &mpt->request_pool[index];
			mpt_free_request(mpt, req);
		}
		break;
	    }

	case MPI_FUNCTION_PORT_ENABLE:
	    {
		MSG_PORT_ENABLE_REPLY *msg = vmsg;
		int index = le32toh(msg->MsgContext) & ~0x80000000;
		if (mpt->verbose > 1)
			mpt_prt(mpt, "enable port reply index %d", index);
		if (index >= 0 && index < MPT_MAX_REQUESTS(mpt)) {
			request_t *req = &mpt->request_pool[index];
			req->debug = REQ_DONE;
		}
		mpt_free_reply(mpt, (reply << 1));
		break;
	    }

	case MPI_FUNCTION_CONFIG:
	    {
		MSG_CONFIG_REPLY *msg = vmsg;
		int index = le32toh(msg->MsgContext) & ~0x80000000;
		if (index >= 0 && index < MPT_MAX_REQUESTS(mpt)) {
			request_t *req = &mpt->request_pool[index];
			req->debug = REQ_DONE;
			req->sequence = reply;
		} else
			mpt_free_reply(mpt, (reply << 1));
		break;
	    }

	default:
		mpt_prt(mpt, "unknown ctlop: 0x%x", dmsg->Function);
	}
}

static void
mpt_event_notify_reply(mpt_softc_t *mpt, MSG_EVENT_NOTIFY_REPLY *msg)
{

	switch (le32toh(msg->Event)) {
	case MPI_EVENT_LOG_DATA:
	    {
		int i;

		/* Some error occurrerd that the Fusion wants logged. */
		mpt_prt(mpt, "EvtLogData: IOCLogInfo: 0x%08x", msg->IOCLogInfo);
		mpt_prt(mpt, "EvtLogData: Event Data:");
		for (i = 0; i < msg->EventDataLength; i++) {
			if ((i % 4) == 0)
				printf("%s:\t", device_xname(mpt->sc_dev));
			printf("0x%08x%c", msg->Data[i],
			    ((i % 4) == 3) ? '\n' : ' ');
		}
		if ((i % 4) != 0)
			printf("\n");
		break;
	    }

	case MPI_EVENT_UNIT_ATTENTION:
		mpt_prt(mpt, "Unit Attn: Bus 0x%02x Target 0x%02x",
		    (msg->Data[0] >> 8) & 0xff, msg->Data[0] & 0xff);
		break;

	case MPI_EVENT_IOC_BUS_RESET:
		/* We generated a bus reset. */
		mpt_prt(mpt, "IOC Bus Reset Port %d",
		    (msg->Data[0] >> 8) & 0xff);
		break;

	case MPI_EVENT_EXT_BUS_RESET:
		/* Someone else generated a bus reset. */
		mpt_prt(mpt, "External Bus Reset");
		/*
		 * These replies don't return EventData like the MPI
		 * spec says they do.
		 */
		/* XXX Send an async event? */
		break;

	case MPI_EVENT_RESCAN:
		/*
		 * In general, thise means a device has been added
		 * to the loop.
		 */
		mpt_prt(mpt, "Rescan Port %d", (msg->Data[0] >> 8) & 0xff);
		/* XXX Send an async event? */
		break;

	case MPI_EVENT_LINK_STATUS_CHANGE:
		mpt_prt(mpt, "Port %d: Link state %s",
		    (msg->Data[1] >> 8) & 0xff,
		    (msg->Data[0] & 0xff) == 0 ? "Failed" : "Active");
		break;

	case MPI_EVENT_LOOP_STATE_CHANGE:
		switch ((msg->Data[0] >> 16) & 0xff) {
		case 0x01:
			mpt_prt(mpt,
			    "Port %d: FC Link Event: LIP(%02x,%02x) "
			    "(Loop Initialization)",
			    (msg->Data[1] >> 8) & 0xff,
			    (msg->Data[0] >> 8) & 0xff,
			    (msg->Data[0]     ) & 0xff);
			switch ((msg->Data[0] >> 8) & 0xff) {
			case 0xf7:
				if ((msg->Data[0] & 0xff) == 0xf7)
					mpt_prt(mpt, "\tDevice needs AL_PA");
				else
					mpt_prt(mpt, "\tDevice %02x doesn't "
					    "like FC performance",
					    msg->Data[0] & 0xff);
				break;

			case 0xf8:
				if ((msg->Data[0] & 0xff) == 0xf7)
					mpt_prt(mpt, "\tDevice detected loop "
					    "failure before acquiring AL_PA");
				else
					mpt_prt(mpt, "\tDevice %02x detected "
					    "loop failure",
					    msg->Data[0] & 0xff);
				break;

			default:
				mpt_prt(mpt, "\tDevice %02x requests that "
				    "device %02x reset itself",
				    msg->Data[0] & 0xff,
				    (msg->Data[0] >> 8) & 0xff);
				break;
			}
			break;

		case 0x02:
			mpt_prt(mpt, "Port %d: FC Link Event: LPE(%02x,%02x) "
			    "(Loop Port Enable)",
			    (msg->Data[1] >> 8) & 0xff,
			    (msg->Data[0] >> 8) & 0xff,
			    (msg->Data[0]     ) & 0xff);
			break;

		case 0x03:
			mpt_prt(mpt, "Port %d: FC Link Event: LPB(%02x,%02x) "
			    "(Loop Port Bypass)",
			    (msg->Data[1] >> 8) & 0xff,
			    (msg->Data[0] >> 8) & 0xff,
			    (msg->Data[0]     ) & 0xff);
			break;

		default:
			mpt_prt(mpt, "Port %d: FC Link Event: "
			    "Unknown event (%02x %02x %02x)",
			    (msg->Data[1] >>  8) & 0xff,
			    (msg->Data[0] >> 16) & 0xff,
			    (msg->Data[0] >>  8) & 0xff,
			    (msg->Data[0]      ) & 0xff);
			break;
		}
		break;

	case MPI_EVENT_LOGOUT:
		mpt_prt(mpt, "Port %d: FC Logout: N_PortID: %02x",
		    (msg->Data[1] >> 8) & 0xff, msg->Data[0]);
		break;

	case MPI_EVENT_EVENT_CHANGE:
		/*
		 * This is just an acknowledgement of our
		 * mpt_send_event_request().
		 */
		break;

	case MPI_EVENT_SAS_PHY_LINK_STATUS:
		switch ((msg->Data[0] >> 12) & 0x0f) {
		case 0x00:
			mpt_prt(mpt, "Phy %d: Link Status Unknown",
			    msg->Data[0] & 0xff);
			break;
		case 0x01:
			mpt_prt(mpt, "Phy %d: Link Disabled",
			    msg->Data[0] & 0xff);
			break;
		case 0x02:
			mpt_prt(mpt, "Phy %d: Failed Speed Negotiation",
			    msg->Data[0] & 0xff);
			break;
		case 0x03:
			mpt_prt(mpt, "Phy %d: SATA OOB Complete",
			    msg->Data[0] & 0xff);
			break;
		case 0x08:
			mpt_prt(mpt, "Phy %d: Link Rate 1.5 Gbps",
			    msg->Data[0] & 0xff);
			break;
		case 0x09:
			mpt_prt(mpt, "Phy %d: Link Rate 3.0 Gbps",
			    msg->Data[0] & 0xff);
			break;
		default:
			mpt_prt(mpt, "Phy %d: SAS Phy Link Status Event: "
			    "Unknown event (%0x)",
			    msg->Data[0] & 0xff, (msg->Data[0] >> 8) & 0xff);
		}
		break;

	case MPI_EVENT_SAS_DEVICE_STATUS_CHANGE:
	case MPI_EVENT_SAS_DISCOVERY:
		/* ignore these events for now */
		break;

	case MPI_EVENT_QUEUE_FULL:
		/* This can get a little chatty */
		if (mpt->verbose > 0)
			mpt_prt(mpt, "Queue Full Event");
		break;

	default:
		mpt_prt(mpt, "Unknown async event: 0x%x", msg->Event);
		break;
	}

	if (msg->AckRequired) {
		MSG_EVENT_ACK *ackp;
		request_t *req;

		if ((req = mpt_get_request(mpt)) == NULL) {
			/* XXX XXX XXX XXXJRT */
			panic("mpt_event_notify_reply: unable to allocate "
			    "request structure");
		}

		ackp = (MSG_EVENT_ACK *) req->req_vbuf;
		memset(ackp, 0, sizeof(*ackp));
		ackp->Function = MPI_FUNCTION_EVENT_ACK;
		ackp->Event = msg->Event;
		ackp->EventContext = msg->EventContext;
		ackp->MsgContext = htole32(req->index | 0x80000000);
		mpt_check_doorbell(mpt);
		mpt_send_cmd(mpt, req);
	}
}

static void
mpt_bus_reset(mpt_softc_t *mpt)
{
	request_t *req;
	MSG_SCSI_TASK_MGMT *mngt_req;
	int s;

	s = splbio();
	if (mpt->mngt_req) {
		/* request already queued; can't do more */
		splx(s);
		return;
	}
	req = mpt_get_request(mpt);
	if (__predict_false(req == NULL)) {
		mpt_prt(mpt, "no mngt request\n");
		splx(s);
		return;
	}
	mpt->mngt_req = req;
	splx(s);
	mngt_req = req->req_vbuf;
	memset(mngt_req, 0, sizeof(*mngt_req));
	mngt_req->Function = MPI_FUNCTION_SCSI_TASK_MGMT;
	mngt_req->Bus = mpt->bus;
	mngt_req->TargetID = 0;
	mngt_req->ChainOffset = 0;
	mngt_req->TaskType = MPI_SCSITASKMGMT_TASKTYPE_RESET_BUS;
	mngt_req->Reserved1 = 0;
	mngt_req->MsgFlags =
	    mpt->is_fc ? MPI_SCSITASKMGMT_MSGFLAGS_LIP_RESET_OPTION : 0;
	mngt_req->MsgContext = req->index;
	mngt_req->TaskMsgContext = 0;
	s = splbio();
	mpt_send_handshake_cmd(mpt, sizeof(*mngt_req), mngt_req);
	splx(s);
}

/*****************************************************************************
 * SCSI interface routines
 *****************************************************************************/

static void
mpt_scsipi_request(struct scsipi_channel *chan, scsipi_adapter_req_t req,
    void *arg)
{
	struct scsipi_adapter *adapt = chan->chan_adapter;
	mpt_softc_t *mpt = device_private(adapt->adapt_dev);

	switch (req) {
	case ADAPTER_REQ_RUN_XFER:
		mpt_run_xfer(mpt, (struct scsipi_xfer *) arg);
		return;

	case ADAPTER_REQ_GROW_RESOURCES:
		/* Not supported. */
		return;

	case ADAPTER_REQ_SET_XFER_MODE:
		mpt_set_xfer_mode(mpt, (struct scsipi_xfer_mode *) arg);
		return;
	}
}

static void
mpt_minphys(struct buf *bp)
{

/*
 * Subtract one from the SGL limit, since we need an extra one to handle
 * an non-page-aligned transfer.
 */
#define	MPT_MAX_XFER	((MPT_SGL_MAX - 1) * PAGE_SIZE)

	if (bp->b_bcount > MPT_MAX_XFER)
		bp->b_bcount = MPT_MAX_XFER;
	minphys(bp);
}

static int
mpt_ioctl(struct scsipi_channel *chan, u_long cmd, void *arg,
    int flag, struct proc *p)
{
	mpt_softc_t *mpt;
	int s;

	mpt = device_private(chan->chan_adapter->adapt_dev);
	switch (cmd) {
	case SCBUSIORESET:
		mpt_bus_reset(mpt);
		s = splbio();
		mpt_intr(mpt);
		splx(s);
		return(0);
	default:
		return (ENOTTY);
	}
}

#if NBIO > 0
static fCONFIG_PAGE_IOC_2 *
mpt_get_cfg_page_ioc2(mpt_softc_t *mpt)
{
	fCONFIG_PAGE_HEADER hdr;
	fCONFIG_PAGE_IOC_2 *ioc2;
	int rv;

	rv = mpt_read_cfg_header(mpt, MPI_CONFIG_PAGETYPE_IOC, 2, 0, &hdr);
	if (rv)
		return NULL;

	ioc2 = malloc(hdr.PageLength * 4, M_DEVBUF, M_WAITOK | M_ZERO);
	if (ioc2 == NULL)
		return NULL;

	memcpy(ioc2, &hdr, sizeof(hdr));

	rv = mpt_read_cfg_page(mpt, 0, &ioc2->Header);
	if (rv)
		goto fail;
	mpt2host_config_page_ioc_2(ioc2);

	return ioc2;

fail:
	free(ioc2, M_DEVBUF);
	return NULL;
}

static fCONFIG_PAGE_IOC_3 *
mpt_get_cfg_page_ioc3(mpt_softc_t *mpt)
{
	fCONFIG_PAGE_HEADER hdr;
	fCONFIG_PAGE_IOC_3 *ioc3;
	int rv;

	rv = mpt_read_cfg_header(mpt, MPI_CONFIG_PAGETYPE_IOC, 3, 0, &hdr);
	if (rv)
		return NULL;

	ioc3 = malloc(hdr.PageLength * 4, M_DEVBUF, M_WAITOK | M_ZERO);
	if (ioc3 == NULL)
		return NULL;

	memcpy(ioc3, &hdr, sizeof(hdr));

	rv = mpt_read_cfg_page(mpt, 0, &ioc3->Header);
	if (rv)
		goto fail;

	return ioc3;

fail:
	free(ioc3, M_DEVBUF);
	return NULL;
}


static fCONFIG_PAGE_RAID_VOL_0 *
mpt_get_cfg_page_raid_vol0(mpt_softc_t *mpt, int address)
{
	fCONFIG_PAGE_HEADER hdr;
	fCONFIG_PAGE_RAID_VOL_0 *rvol0;
	int rv;

	rv = mpt_read_cfg_header(mpt, MPI_CONFIG_PAGETYPE_RAID_VOLUME, 0,
	    address, &hdr);
	if (rv)
		return NULL;

	rvol0 = malloc(hdr.PageLength * 4, M_DEVBUF, M_WAITOK | M_ZERO);
	if (rvol0 == NULL)
		return NULL;

	memcpy(rvol0, &hdr, sizeof(hdr));

	rv = mpt_read_cfg_page(mpt, address, &rvol0->Header);
	if (rv)
		goto fail;
	mpt2host_config_page_raid_vol_0(rvol0);

	return rvol0;

fail:
	free(rvol0, M_DEVBUF);
	return NULL;
}

static fCONFIG_PAGE_RAID_PHYS_DISK_0 *
mpt_get_cfg_page_raid_phys_disk0(mpt_softc_t *mpt, int address)
{
	fCONFIG_PAGE_HEADER hdr;
	fCONFIG_PAGE_RAID_PHYS_DISK_0 *physdisk0;
	int rv;

	rv = mpt_read_cfg_header(mpt, MPI_CONFIG_PAGETYPE_RAID_PHYSDISK, 0,
	    address, &hdr);
	if (rv)
		return NULL;

	physdisk0 = malloc(hdr.PageLength * 4, M_DEVBUF, M_WAITOK | M_ZERO);
	if (physdisk0 == NULL)
		return NULL;

	memcpy(physdisk0, &hdr, sizeof(hdr));

	rv = mpt_read_cfg_page(mpt, address, &physdisk0->Header);
	if (rv)
		goto fail;
	mpt2host_config_page_raid_phys_disk_0(physdisk0);

	return physdisk0;

fail:
	free(physdisk0, M_DEVBUF);
	return NULL;
}

static bool
mpt_is_raid(mpt_softc_t *mpt)
{
	fCONFIG_PAGE_IOC_2 *ioc2;
	bool is_raid = false;

	ioc2 = mpt_get_cfg_page_ioc2(mpt);
	if (ioc2 == NULL)
		return false;

	if (ioc2->CapabilitiesFlags != 0xdeadbeef) {
		is_raid = !!(ioc2->CapabilitiesFlags &
				(MPI_IOCPAGE2_CAP_FLAGS_IS_SUPPORT|
				 MPI_IOCPAGE2_CAP_FLAGS_IME_SUPPORT|
				 MPI_IOCPAGE2_CAP_FLAGS_IM_SUPPORT));
	}

	free(ioc2, M_DEVBUF);

	return is_raid;
}

static int
mpt_bio_ioctl(device_t dev, u_long cmd, void *addr)
{
	mpt_softc_t *mpt = device_private(dev);
	int error, s;

	KERNEL_LOCK(1, curlwp);
	s = splbio();

	switch (cmd) {
	case BIOCINQ:
		error = mpt_bio_ioctl_inq(mpt, addr);
		break;
	case BIOCVOL:
		error = mpt_bio_ioctl_vol(mpt, addr);
		break;
	case BIOCDISK_NOVOL:
		error = mpt_bio_ioctl_disk_novol(mpt, addr);
		break;
	case BIOCDISK:
		error = mpt_bio_ioctl_disk(mpt, addr);
		break;
	case BIOCSETSTATE:
		error = mpt_bio_ioctl_setstate(mpt, addr);
		break;
	default:
		error = EINVAL;
		break;
	}

	splx(s);
	KERNEL_UNLOCK_ONE(curlwp);

	return error;
}

static int
mpt_bio_ioctl_inq(mpt_softc_t *mpt, struct bioc_inq *bi)
{	
	fCONFIG_PAGE_IOC_2 *ioc2;
	fCONFIG_PAGE_IOC_3 *ioc3;

	ioc2 = mpt_get_cfg_page_ioc2(mpt);
	if (ioc2 == NULL)
		return EIO;
	ioc3 = mpt_get_cfg_page_ioc3(mpt);
	if (ioc3 == NULL) {
		free(ioc2, M_DEVBUF);
		return EIO;
	}

	strlcpy(bi->bi_dev, device_xname(mpt->sc_dev), sizeof(bi->bi_dev));
	bi->bi_novol = ioc2->NumActiveVolumes;
	bi->bi_nodisk = ioc3->NumPhysDisks;

	free(ioc2, M_DEVBUF);
	free(ioc3, M_DEVBUF);

	return 0;
}

static int
mpt_bio_ioctl_vol(mpt_softc_t *mpt, struct bioc_vol *bv)
{
	fCONFIG_PAGE_IOC_2 *ioc2 = NULL;
	fCONFIG_PAGE_IOC_2_RAID_VOL *ioc2rvol;
	fCONFIG_PAGE_RAID_VOL_0 *rvol0 = NULL;
	struct scsipi_periph *periph;
	struct scsipi_inquiry_data inqbuf;
	char vendor[9], product[17], revision[5];
	int address;

	ioc2 = mpt_get_cfg_page_ioc2(mpt);
	if (ioc2 == NULL)
		return EIO;

	if (bv->bv_volid < 0 || bv->bv_volid >= ioc2->NumActiveVolumes)
		goto fail;

	ioc2rvol = &ioc2->RaidVolume[bv->bv_volid];
	address = ioc2rvol->VolumeID | (ioc2rvol->VolumeBus << 8);

	rvol0 = mpt_get_cfg_page_raid_vol0(mpt, address);
	if (rvol0 == NULL)
		goto fail;

	bv->bv_dev[0] = '\0';
	bv->bv_vendor[0] = '\0';

	periph = scsipi_lookup_periph(&mpt->sc_channel, ioc2rvol->VolumeBus, 0);
	if (periph != NULL) {
		if (periph->periph_dev != NULL) {
			snprintf(bv->bv_dev, sizeof(bv->bv_dev), "%s",
			    device_xname(periph->periph_dev));
		}
		memset(&inqbuf, 0, sizeof(inqbuf));
		if (scsipi_inquire(periph, &inqbuf,
		    XS_CTL_DISCOVERY | XS_CTL_SILENT) == 0) {
			scsipi_strvis(vendor, sizeof(vendor),
			    inqbuf.vendor, sizeof(inqbuf.vendor));
			scsipi_strvis(product, sizeof(product),
			    inqbuf.product, sizeof(inqbuf.product));
			scsipi_strvis(revision, sizeof(revision),
			    inqbuf.revision, sizeof(inqbuf.revision));

			snprintf(bv->bv_vendor, sizeof(bv->bv_vendor),
			    "%s %s %s", vendor, product, revision);
		}
	
		snprintf(bv->bv_dev, sizeof(bv->bv_dev), "%s",
		    device_xname(periph->periph_dev));
	}
	bv->bv_nodisk = rvol0->NumPhysDisks;
	bv->bv_size = (uint64_t)rvol0->MaxLBA * 512;
	bv->bv_stripe_size = rvol0->StripeSize;
	bv->bv_percent = -1;
	bv->bv_seconds = 0;

	switch (rvol0->VolumeStatus.State) {
	case MPI_RAIDVOL0_STATUS_STATE_OPTIMAL:
		bv->bv_status = BIOC_SVONLINE;
		break;
	case MPI_RAIDVOL0_STATUS_STATE_DEGRADED:
		bv->bv_status = BIOC_SVDEGRADED;
		break;
	case MPI_RAIDVOL0_STATUS_STATE_FAILED:
		bv->bv_status = BIOC_SVOFFLINE;
		break;
	default:
		bv->bv_status = BIOC_SVINVALID;
		break;
	}

	switch (ioc2rvol->VolumeType) {
	case MPI_RAID_VOL_TYPE_IS:
		bv->bv_level = 0;
		break;
	case MPI_RAID_VOL_TYPE_IME:
	case MPI_RAID_VOL_TYPE_IM:
		bv->bv_level = 1;
		break;
	default:
		bv->bv_level = -1;
		break;
	}

	free(ioc2, M_DEVBUF);
	free(rvol0, M_DEVBUF);

	return 0;

fail:
	if (ioc2) free(ioc2, M_DEVBUF);
	if (rvol0) free(rvol0, M_DEVBUF);
	return EINVAL;
}

static void
mpt_bio_ioctl_disk_common(mpt_softc_t *mpt, struct bioc_disk *bd,
    int address)
{
	fCONFIG_PAGE_RAID_PHYS_DISK_0 *phys = NULL;
	char vendor_id[9], product_id[17], product_rev_level[5];

	phys = mpt_get_cfg_page_raid_phys_disk0(mpt, address);
	if (phys == NULL)
		return;

	scsipi_strvis(vendor_id, sizeof(vendor_id),
	    phys->InquiryData.VendorID, sizeof(phys->InquiryData.VendorID));
	scsipi_strvis(product_id, sizeof(product_id),
	    phys->InquiryData.ProductID, sizeof(phys->InquiryData.ProductID));
	scsipi_strvis(product_rev_level, sizeof(product_rev_level),
	    phys->InquiryData.ProductRevLevel,
	    sizeof(phys->InquiryData.ProductRevLevel));

	snprintf(bd->bd_vendor, sizeof(bd->bd_vendor), "%s %s %s",
	    vendor_id, product_id, product_rev_level);
	strlcpy(bd->bd_serial, phys->InquiryData.Info, sizeof(bd->bd_serial));
	bd->bd_procdev[0] = '\0';
	bd->bd_channel = phys->PhysDiskBus;
	bd->bd_target = phys->PhysDiskID;
	bd->bd_lun = 0;
	bd->bd_size = (uint64_t)phys->MaxLBA * 512;

	switch (phys->PhysDiskStatus.State) {
	case MPI_PHYSDISK0_STATUS_ONLINE:
		bd->bd_status = BIOC_SDONLINE;
		break;
	case MPI_PHYSDISK0_STATUS_MISSING:
	case MPI_PHYSDISK0_STATUS_FAILED:
		bd->bd_status = BIOC_SDFAILED;
		break;
	case MPI_PHYSDISK0_STATUS_OFFLINE_REQUESTED:
	case MPI_PHYSDISK0_STATUS_FAILED_REQUESTED:
	case MPI_PHYSDISK0_STATUS_OTHER_OFFLINE:
		bd->bd_status = BIOC_SDOFFLINE;
		break;
	case MPI_PHYSDISK0_STATUS_INITIALIZING:
		bd->bd_status = BIOC_SDSCRUB;
		break;
	case MPI_PHYSDISK0_STATUS_NOT_COMPATIBLE:
	default:
		bd->bd_status = BIOC_SDINVALID;
		break;
	}

	free(phys, M_DEVBUF);
}

static int
mpt_bio_ioctl_disk_novol(mpt_softc_t *mpt, struct bioc_disk *bd)
{
	fCONFIG_PAGE_IOC_2 *ioc2 = NULL;
	fCONFIG_PAGE_IOC_3 *ioc3 = NULL;
	fCONFIG_PAGE_RAID_VOL_0 *rvol0 = NULL;
	fCONFIG_PAGE_IOC_2_RAID_VOL *ioc2rvol;
	int address, v, d;

	ioc2 = mpt_get_cfg_page_ioc2(mpt);
	if (ioc2 == NULL)
		return EIO;
	ioc3 = mpt_get_cfg_page_ioc3(mpt);
	if (ioc3 == NULL) {
		free(ioc2, M_DEVBUF);
		return EIO;
	}

	if (bd->bd_diskid < 0 || bd->bd_diskid >= ioc3->NumPhysDisks)
		goto fail;

	address = ioc3->PhysDisk[bd->bd_diskid].PhysDiskNum;

	mpt_bio_ioctl_disk_common(mpt, bd, address);

	bd->bd_disknovol = true;
	for (v = 0; bd->bd_disknovol && v < ioc2->NumActiveVolumes; v++) {
		ioc2rvol = &ioc2->RaidVolume[v];
		address = ioc2rvol->VolumeID | (ioc2rvol->VolumeBus << 8);

		rvol0 = mpt_get_cfg_page_raid_vol0(mpt, address);
		if (rvol0 == NULL)
			continue;

		for (d = 0; d < rvol0->NumPhysDisks; d++) {
			if (rvol0->PhysDisk[d].PhysDiskNum ==
			    ioc3->PhysDisk[bd->bd_diskid].PhysDiskNum) {
				bd->bd_disknovol = false;
				bd->bd_volid = v;
				break;
			}
		}
		free(rvol0, M_DEVBUF);
	}

	free(ioc3, M_DEVBUF);
	free(ioc2, M_DEVBUF);

	return 0;

fail:
	if (ioc3) free(ioc3, M_DEVBUF);
	if (ioc2) free(ioc2, M_DEVBUF);
	return EINVAL;
}


static int
mpt_bio_ioctl_disk(mpt_softc_t *mpt, struct bioc_disk *bd)
{
	fCONFIG_PAGE_IOC_2 *ioc2 = NULL;
	fCONFIG_PAGE_RAID_VOL_0 *rvol0 = NULL;
	fCONFIG_PAGE_IOC_2_RAID_VOL *ioc2rvol;
	int address;

	ioc2 = mpt_get_cfg_page_ioc2(mpt);
	if (ioc2 == NULL)
		return EIO;

	if (bd->bd_volid < 0 || bd->bd_volid >= ioc2->NumActiveVolumes)
		goto fail;

	ioc2rvol = &ioc2->RaidVolume[bd->bd_volid];
	address = ioc2rvol->VolumeID | (ioc2rvol->VolumeBus << 8);

	rvol0 = mpt_get_cfg_page_raid_vol0(mpt, address);
	if (rvol0 == NULL)
		goto fail;

	if (bd->bd_diskid < 0 || bd->bd_diskid >= rvol0->NumPhysDisks)
		goto fail;

	address = rvol0->PhysDisk[bd->bd_diskid].PhysDiskNum;

	mpt_bio_ioctl_disk_common(mpt, bd, address);

	free(ioc2, M_DEVBUF);

	return 0;

fail:
	if (ioc2) free(ioc2, M_DEVBUF);
	return EINVAL;
}

static int
mpt_bio_ioctl_setstate(mpt_softc_t *mpt, struct bioc_setstate *bs)
{
	return ENOTTY;
}
#endif

