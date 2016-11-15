/*	$NetBSD: mpt.c,v 1.17 2014/09/27 16:14:16 jmcneill Exp $	*/

/*
 * Copyright (c) 2000, 2001 by Greg Ansley
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
/*-
 * Copyright (c) 2002, 2006 by Matthew Jacob
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon including
 *    a substantially similar Disclaimer requirement for further binary
 *    redistribution.
 * 3. Neither the names of the above listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF THE COPYRIGHT
 * OWNER OR CONTRIBUTOR IS ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Support from Chris Ellsworth in order to make SAS adapters work
 * is gratefully acknowledged.
 *
 *
 * Support from LSI-Logic has also gone a great deal toward making this a
 * workable subsystem and is gratefully acknowledged.
 */
/*-
 * Copyright (c) 2004, Avid Technology, Inc. and its contributors.
 * Copyright (c) 2005, WHEEL Sp. z o.o.
 * Copyright (c) 2004, 2005 Justin T. Gibbs
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon including
 *    a substantially similar Disclaimer requirement for further binary
 *    redistribution.
 * 3. Neither the names of the above listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF THE COPYRIGHT
 * OWNER OR CONTRIBUTOR IS ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * mpt.c:
 *
 * Generic routines for LSI Fusion adapters.
 *
 * Adapted from the FreeBSD "mpt" driver by Jason R. Thorpe for
 * Wasabi Systems, Inc.
 *
 * Additional contributions by Garrett D'Amore on behalf of TELES AG.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mpt.c,v 1.17 2014/09/27 16:14:16 jmcneill Exp $");

#include <dev/ic/mpt.h>

#define MPT_MAX_TRYS 3
#define MPT_MAX_WAIT 300000

static int maxwait_ack = 0;
static int maxwait_int = 0;
static int maxwait_state = 0;

static inline u_int32_t
mpt_rd_db(mpt_softc_t *mpt)
{
	return mpt_read(mpt, MPT_OFFSET_DOORBELL);
}

static inline u_int32_t
mpt_rd_intr(mpt_softc_t *mpt)
{
	return mpt_read(mpt, MPT_OFFSET_INTR_STATUS);
}

/* Busy wait for a door bell to be read by IOC */
static int
mpt_wait_db_ack(mpt_softc_t *mpt)
{
	int i;
	for (i=0; i < MPT_MAX_WAIT; i++) {
		if (!MPT_DB_IS_BUSY(mpt_rd_intr(mpt))) {
			maxwait_ack = i > maxwait_ack ? i : maxwait_ack;
			return MPT_OK;
		}

		DELAY(100);
	}
	return MPT_FAIL;
}

/* Busy wait for a door bell interrupt */
static int
mpt_wait_db_int(mpt_softc_t *mpt)
{
	int i;
	for (i=0; i < MPT_MAX_WAIT; i++) {
		if (MPT_DB_INTR(mpt_rd_intr(mpt))) {
			maxwait_int = i > maxwait_int ? i : maxwait_int;
			return MPT_OK;
		}
		DELAY(100);
	}
	return MPT_FAIL;
}

/* Wait for IOC to transition to a give state */
void
mpt_check_doorbell(mpt_softc_t *mpt)
{
	u_int32_t db = mpt_rd_db(mpt);
	if (MPT_STATE(db) != MPT_DB_STATE_RUNNING) {
		mpt_prt(mpt, "Device not running");
		mpt_print_db(db);
	}
}

/* Wait for IOC to transition to a give state */
static int
mpt_wait_state(mpt_softc_t *mpt, enum DB_STATE_BITS state)
{
	int i;

	for (i = 0; i < MPT_MAX_WAIT; i++) {
		u_int32_t db = mpt_rd_db(mpt);
		if (MPT_STATE(db) == state) {
			maxwait_state = i > maxwait_state ? i : maxwait_state;
			return (MPT_OK);
		}
		DELAY(100);
	}
	return (MPT_FAIL);
}


/* Issue the reset COMMAND to the IOC */
int
mpt_soft_reset(mpt_softc_t *mpt)
{
	if (mpt->verbose) {
		mpt_prt(mpt, "soft reset");
	}

	/* Have to use hard reset if we are not in Running state */
	if (MPT_STATE(mpt_rd_db(mpt)) != MPT_DB_STATE_RUNNING) {
		mpt_prt(mpt, "soft reset failed: device not running");
		return MPT_FAIL;
	}

	/* If door bell is in use we don't have a chance of getting
	 * a word in since the IOC probably crashed in message
	 * processing. So don't waste our time.
	 */
	if (MPT_DB_IS_IN_USE(mpt_rd_db(mpt))) {
		mpt_prt(mpt, "soft reset failed: doorbell wedged");
		return MPT_FAIL;
	}

	/* Send the reset request to the IOC */
	mpt_write(mpt, MPT_OFFSET_DOORBELL,
	    MPI_FUNCTION_IOC_MESSAGE_UNIT_RESET << MPI_DOORBELL_FUNCTION_SHIFT);
	if (mpt_wait_db_ack(mpt) != MPT_OK) {
		mpt_prt(mpt, "soft reset failed: ack timeout");
		return MPT_FAIL;
	}

	/* Wait for the IOC to reload and come out of reset state */
	if (mpt_wait_state(mpt, MPT_DB_STATE_READY) != MPT_OK) {
		mpt_prt(mpt, "soft reset failed: device did not start running");
		return MPT_FAIL;
	}

	return MPT_OK;
}

/* This is a magic diagnostic reset that resets all the ARM
 * processors in the chip.
 */
void
mpt_hard_reset(mpt_softc_t *mpt)
{
	if (mpt->verbose) {
		mpt_prt(mpt, "hard reset");
	}
	mpt_write(mpt, MPT_OFFSET_SEQUENCE, 0xff);

	/* Enable diagnostic registers */
	mpt_write(mpt, MPT_OFFSET_SEQUENCE, MPT_DIAG_SEQUENCE_1);
	mpt_write(mpt, MPT_OFFSET_SEQUENCE, MPT_DIAG_SEQUENCE_2);
	mpt_write(mpt, MPT_OFFSET_SEQUENCE, MPT_DIAG_SEQUENCE_3);
	mpt_write(mpt, MPT_OFFSET_SEQUENCE, MPT_DIAG_SEQUENCE_4);
	mpt_write(mpt, MPT_OFFSET_SEQUENCE, MPT_DIAG_SEQUENCE_5);

	/* Diag. port is now active so we can now hit the reset bit */
	mpt_write(mpt, MPT_OFFSET_DIAGNOSTIC, MPT_DIAG_RESET_IOC);

	DELAY(10000);

	/* Disable Diagnostic Register */
	mpt_write(mpt, MPT_OFFSET_SEQUENCE, 0xFF);
}

/*
 * Reset the IOC when needed. Try software command first then if needed
 * poke at the magic diagnostic reset. Note that a hard reset resets
 * *both* IOCs on dual function chips (FC929 && LSI1030) as well as
 * fouls up the PCI configuration registers.
 */
int
mpt_reset(mpt_softc_t *mpt)
{
	int ret;

	/* Try a soft reset */
	if ((ret = mpt_soft_reset(mpt)) != MPT_OK) {
		/* Failed; do a hard reset */
		mpt_hard_reset(mpt);

		/* Wait for the IOC to reload and come out of reset state */
		ret = mpt_wait_state(mpt, MPT_DB_STATE_READY);
		if (ret != MPT_OK) {
			mpt_prt(mpt, "failed to reset device");
		}
	}

	return ret;
}

/* Return a command buffer to the free queue */
void
mpt_free_request(mpt_softc_t *mpt, request_t *req)
{
	if (req == NULL || req != &mpt->request_pool[req->index]) {
		panic("mpt_free_request bad req ptr\n");
		return;
	}
	req->sequence = 0;
	req->xfer = NULL;
	req->debug = REQ_FREE;
	SLIST_INSERT_HEAD(&mpt->request_free_list, req, link);
}

/* Get a command buffer from the free queue */
request_t *
mpt_get_request(mpt_softc_t *mpt)
{
	request_t *req;
	req = SLIST_FIRST(&mpt->request_free_list);
	if (req != NULL) {
		if (req != &mpt->request_pool[req->index]) {
			panic("mpt_get_request: corrupted request free list\n");
		}
		if (req->xfer != NULL) {
			panic("mpt_get_request: corrupted request free list (xfer)\n");
		}
		SLIST_REMOVE_HEAD(&mpt->request_free_list, link);
		req->debug = REQ_IN_PROGRESS;
	}
	return req;
}

/* Pass the command to the IOC */
void
mpt_send_cmd(mpt_softc_t *mpt, request_t *req)
{
	req->sequence = mpt->sequence++;
	if (mpt->verbose > 1) {
		u_int32_t *pReq;
		pReq = req->req_vbuf;
		mpt_prt(mpt, "Send Request %d (0x%x):",
		    req->index, req->req_pbuf);
		mpt_prt(mpt, "%08x %08x %08x %08x",
		    pReq[0], pReq[1], pReq[2], pReq[3]);
		mpt_prt(mpt, "%08x %08x %08x %08x",
		    pReq[4], pReq[5], pReq[6], pReq[7]);
		mpt_prt(mpt, "%08x %08x %08x %08x",
		    pReq[8], pReq[9], pReq[10], pReq[11]);
		mpt_prt(mpt, "%08x %08x %08x %08x",
		    pReq[12], pReq[13], pReq[14], pReq[15]);
	}
	MPT_SYNC_REQ(mpt, req, BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	req->debug = REQ_ON_CHIP;
	mpt_write(mpt, MPT_OFFSET_REQUEST_Q, (u_int32_t) req->req_pbuf);
}

/*
 * Give the reply buffer back to the IOC after we have
 * finished processing it.
 */
void
mpt_free_reply(mpt_softc_t *mpt, u_int32_t ptr)
{
     mpt_write(mpt, MPT_OFFSET_REPLY_Q, ptr);
}

/* Get a reply from the IOC */
u_int32_t
mpt_pop_reply_queue(mpt_softc_t *mpt)
{
     return mpt_read(mpt, MPT_OFFSET_REPLY_Q);
}

/*
 * Send a command to the IOC via the handshake register.
 *
 * Only done at initialization time and for certain unusual
 * commands such as device/bus reset as specified by LSI.
 */
int
mpt_send_handshake_cmd(mpt_softc_t *mpt, size_t len, void *cmd)
{
	int i;
	u_int32_t data, *data32;

	/* Check condition of the IOC */
	data = mpt_rd_db(mpt);
	if (((MPT_STATE(data) != MPT_DB_STATE_READY)	&&
	     (MPT_STATE(data) != MPT_DB_STATE_RUNNING)	&&
	     (MPT_STATE(data) != MPT_DB_STATE_FAULT))	||
	    (  MPT_DB_IS_IN_USE(data) )) {
		mpt_prt(mpt, "handshake aborted due to invalid doorbell state");
		mpt_print_db(data);
		return(EBUSY);
	}

	/* We move things in 32 bit chunks */
	len = (len + 3) >> 2;
	data32 = cmd;

	/* Clear any left over pending doorbell interrupts */
	if (MPT_DB_INTR(mpt_rd_intr(mpt)))
		mpt_write(mpt, MPT_OFFSET_INTR_STATUS, 0);

	/*
	 * Tell the handshake reg. we are going to send a command
         * and how long it is going to be.
	 */
	data = (MPI_FUNCTION_HANDSHAKE << MPI_DOORBELL_FUNCTION_SHIFT) |
	    (len << MPI_DOORBELL_ADD_DWORDS_SHIFT);
	mpt_write(mpt, MPT_OFFSET_DOORBELL, data);

	/* Wait for the chip to notice */
	if (mpt_wait_db_int(mpt) != MPT_OK) {
		mpt_prt(mpt, "mpt_send_handshake_cmd timeout1");
		return ETIMEDOUT;
	}

	/* Clear the interrupt */
	mpt_write(mpt, MPT_OFFSET_INTR_STATUS, 0);

	if (mpt_wait_db_ack(mpt) != MPT_OK) {
		mpt_prt(mpt, "mpt_send_handshake_cmd timeout2");
		return ETIMEDOUT;
	}

	/* Send the command */
	for (i = 0; i < len; i++) {
		mpt_write(mpt, MPT_OFFSET_DOORBELL, htole32(*data32++));
		if (mpt_wait_db_ack(mpt) != MPT_OK) {
			mpt_prt(mpt,
			    "mpt_send_handshake_cmd timeout! index = %d", i);
			return ETIMEDOUT;
		}
	}
	return MPT_OK;
}

/* Get the response from the handshake register */
int
mpt_recv_handshake_reply(mpt_softc_t *mpt, size_t reply_len, void *reply)
{
	int left, reply_left;
	u_int16_t *data16;
	MSG_DEFAULT_REPLY *hdr;

	/* We move things out in 16 bit chunks */
	reply_len >>= 1;
	data16 = (u_int16_t *)reply;

	hdr = (MSG_DEFAULT_REPLY *)reply;

	/* Get first word */
	if (mpt_wait_db_int(mpt) != MPT_OK) {
		mpt_prt(mpt, "mpt_recv_handshake_cmd timeout1");
		return ETIMEDOUT;
	}
	*data16++ = le16toh(mpt_read(mpt, MPT_OFFSET_DOORBELL) &
			    MPT_DB_DATA_MASK);
	mpt_write(mpt, MPT_OFFSET_INTR_STATUS, 0);

	/* Get Second Word */
	if (mpt_wait_db_int(mpt) != MPT_OK) {
		mpt_prt(mpt, "mpt_recv_handshake_cmd timeout2");
		return ETIMEDOUT;
	}
	*data16++ = le16toh(mpt_read(mpt, MPT_OFFSET_DOORBELL) &
			    MPT_DB_DATA_MASK);
	mpt_write(mpt, MPT_OFFSET_INTR_STATUS, 0);

	/* With the second word, we can now look at the length */
	if (mpt->verbose > 1 && ((reply_len >> 1) != hdr->MsgLength)) {
		mpt_prt(mpt, "reply length does not match message length: "
			"got 0x%02x, expected 0x%02x",
			hdr->MsgLength << 2, reply_len << 1);
	}

	/* Get rest of the reply; but don't overflow the provided buffer */
	left = (hdr->MsgLength << 1) - 2;
	reply_left =  reply_len - 2;
	while (left--) {
		u_int16_t datum;

		if (mpt_wait_db_int(mpt) != MPT_OK) {
			mpt_prt(mpt, "mpt_recv_handshake_cmd timeout3");
			return ETIMEDOUT;
		}
		datum = mpt_read(mpt, MPT_OFFSET_DOORBELL);

		if (reply_left-- > 0)
			*data16++ = le16toh(datum & MPT_DB_DATA_MASK);

		mpt_write(mpt, MPT_OFFSET_INTR_STATUS, 0);
	}

	/* One more wait & clear at the end */
	if (mpt_wait_db_int(mpt) != MPT_OK) {
		mpt_prt(mpt, "mpt_recv_handshake_cmd timeout4");
		return ETIMEDOUT;
	}
	mpt_write(mpt, MPT_OFFSET_INTR_STATUS, 0);

	if ((hdr->IOCStatus & MPI_IOCSTATUS_MASK) != MPI_IOCSTATUS_SUCCESS) {
		if (mpt->verbose > 1)
			mpt_print_reply(hdr);
		return (MPT_FAIL | hdr->IOCStatus);
	}

	return (0);
}

static int
mpt_get_iocfacts(mpt_softc_t *mpt, MSG_IOC_FACTS_REPLY *freplp)
{
	MSG_IOC_FACTS f_req;
	int error;

	memset(&f_req, 0, sizeof f_req);
	f_req.Function = MPI_FUNCTION_IOC_FACTS;
	f_req.MsgContext = htole32(0x12071942);
	error = mpt_send_handshake_cmd(mpt, sizeof f_req, &f_req);
	if (error)
		return(error);
	error = mpt_recv_handshake_reply(mpt, sizeof (*freplp), freplp);
	return (error);
}

static int
mpt_get_portfacts(mpt_softc_t *mpt, MSG_PORT_FACTS_REPLY *freplp)
{
	MSG_PORT_FACTS f_req;
	int error;

	/* XXX: Only getting PORT FACTS for Port 0 */
	memset(&f_req, 0, sizeof f_req);
	f_req.Function = MPI_FUNCTION_PORT_FACTS;
	f_req.MsgContext =  htole32(0x12071943);
	error = mpt_send_handshake_cmd(mpt, sizeof f_req, &f_req);
	if (error)
		return(error);
	error = mpt_recv_handshake_reply(mpt, sizeof (*freplp), freplp);
	return (error);
}

/*
 * Send the initialization request. This is where we specify how many
 * SCSI busses and how many devices per bus we wish to emulate.
 * This is also the command that specifies the max size of the reply
 * frames from the IOC that we will be allocating.
 */
static int
mpt_send_ioc_init(mpt_softc_t *mpt, u_int32_t who)
{
	int error = 0;
	MSG_IOC_INIT init;
	MSG_IOC_INIT_REPLY reply;

	memset(&init, 0, sizeof init);
	init.WhoInit = who;
	init.Function = MPI_FUNCTION_IOC_INIT;
	init.MaxDevices = mpt->mpt_max_devices;
	init.MaxBuses = 1;
	init.ReplyFrameSize = htole16(MPT_REPLY_SIZE);
	init.MsgContext = htole32(0x12071941);

	if ((error = mpt_send_handshake_cmd(mpt, sizeof init, &init)) != 0) {
		return(error);
	}

	error = mpt_recv_handshake_reply(mpt, sizeof reply, &reply);
	return (error);
}


/*
 * Utiltity routine to read configuration headers and pages
 */

int
mpt_read_cfg_header(mpt_softc_t *mpt, int PageType, int PageNumber,
    int PageAddress, fCONFIG_PAGE_HEADER *rslt)
{
	int count;
	request_t *req;
	MSG_CONFIG *cfgp;
	MSG_CONFIG_REPLY *reply;

	req = mpt_get_request(mpt);

	cfgp = req->req_vbuf;
	memset(cfgp, 0, sizeof *cfgp);

	cfgp->Action = MPI_CONFIG_ACTION_PAGE_HEADER;
	cfgp->Function = MPI_FUNCTION_CONFIG;
	cfgp->Header.PageNumber = (U8) PageNumber;
	cfgp->Header.PageType = (U8) PageType;
	cfgp->PageAddress = htole32(PageAddress);
	MPI_pSGE_SET_FLAGS(((SGE_SIMPLE32 *) &cfgp->PageBufferSGE),
	    (MPI_SGE_FLAGS_LAST_ELEMENT | MPI_SGE_FLAGS_END_OF_BUFFER |
	    MPI_SGE_FLAGS_SIMPLE_ELEMENT | MPI_SGE_FLAGS_END_OF_LIST));
	cfgp->MsgContext = htole32(req->index | 0x80000000);

	mpt_check_doorbell(mpt);
	mpt_send_cmd(mpt, req);
	count = 0;
	do {
		DELAY(500);
		mpt_intr(mpt);
		if (++count == 1000) {
			mpt_prt(mpt, "read_cfg_header timed out");
			return (-1);
		}
	} while (req->debug == REQ_ON_CHIP);

	reply = (MSG_CONFIG_REPLY *) MPT_REPLY_PTOV(mpt, req->sequence);
        if ((reply->IOCStatus & MPI_IOCSTATUS_MASK) != MPI_IOCSTATUS_SUCCESS) {
		mpt_prt(mpt, "mpt_read_cfg_header: Config Info Status %x",
		    reply->IOCStatus);
		mpt_free_reply(mpt, (req->sequence << 1));
		return (-1);
	}
	memcpy(rslt, &reply->Header, sizeof (fCONFIG_PAGE_HEADER));
	mpt_free_reply(mpt, (req->sequence << 1));
	mpt_free_request(mpt, req);
	return (0);
}

#define	CFG_DATA_OFF	128

int
mpt_read_cfg_page(mpt_softc_t *mpt, int PageAddress, fCONFIG_PAGE_HEADER *hdr)
{
	int count;
	request_t *req;
	SGE_SIMPLE32 *se;
	MSG_CONFIG *cfgp;
	size_t amt;
	MSG_CONFIG_REPLY *reply;

	req = mpt_get_request(mpt);

	cfgp = req->req_vbuf;
	memset(cfgp, 0, MPT_REQUEST_AREA);
	cfgp->Action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;
	cfgp->Function = MPI_FUNCTION_CONFIG;
	cfgp->Header = *hdr;
 	amt = (cfgp->Header.PageLength * sizeof (u_int32_t));
	cfgp->Header.PageType &= MPI_CONFIG_PAGETYPE_MASK;
	cfgp->PageAddress = htole32(PageAddress);
	se = (SGE_SIMPLE32 *) &cfgp->PageBufferSGE;
	se->Address = htole32(req->req_pbuf + CFG_DATA_OFF);
	MPI_pSGE_SET_LENGTH(se, amt);
	MPI_pSGE_SET_FLAGS(se, (MPI_SGE_FLAGS_SIMPLE_ELEMENT |
	    MPI_SGE_FLAGS_LAST_ELEMENT | MPI_SGE_FLAGS_END_OF_BUFFER |
	    MPI_SGE_FLAGS_END_OF_LIST));
	se->FlagsLength = htole32(se->FlagsLength);

	cfgp->MsgContext = htole32(req->index | 0x80000000);

	mpt_check_doorbell(mpt);
	mpt_send_cmd(mpt, req);
	count = 0;
	do {
		DELAY(500);
		mpt_intr(mpt);
		if (++count == 1000) {
			mpt_prt(mpt, "read_cfg_page timed out");
			return (-1);
		}
	} while (req->debug == REQ_ON_CHIP);

	reply = (MSG_CONFIG_REPLY *) MPT_REPLY_PTOV(mpt, req->sequence);
        if ((reply->IOCStatus & MPI_IOCSTATUS_MASK) != MPI_IOCSTATUS_SUCCESS) {
		mpt_prt(mpt, "mpt_read_cfg_page: Config Info Status %x",
		    reply->IOCStatus);
		mpt_free_reply(mpt, (req->sequence << 1));
		return (-1);
	}
	mpt_free_reply(mpt, (req->sequence << 1));
#if 0 /* XXXJRT */
	bus_dmamap_sync(mpt->request_dmat, mpt->request_dmap,
	    BUS_DMASYNC_POSTREAD);
#endif
	if (cfgp->Header.PageType == MPI_CONFIG_PAGETYPE_SCSI_PORT &&
	    cfgp->Header.PageNumber == 0) {
		amt = sizeof (fCONFIG_PAGE_SCSI_PORT_0);
	} else if (cfgp->Header.PageType == MPI_CONFIG_PAGETYPE_SCSI_PORT &&
	    cfgp->Header.PageNumber == 1) {
		amt = sizeof (fCONFIG_PAGE_SCSI_PORT_1);
	} else if (cfgp->Header.PageType == MPI_CONFIG_PAGETYPE_SCSI_PORT &&
	    cfgp->Header.PageNumber == 2) {
		amt = sizeof (fCONFIG_PAGE_SCSI_PORT_2);
	} else if (cfgp->Header.PageType == MPI_CONFIG_PAGETYPE_SCSI_DEVICE  &&
	    cfgp->Header.PageNumber == 0) {
		amt = sizeof (fCONFIG_PAGE_SCSI_DEVICE_0);
	} else if (cfgp->Header.PageType == MPI_CONFIG_PAGETYPE_SCSI_DEVICE  &&
	    cfgp->Header.PageNumber == 1) {
		amt = sizeof (fCONFIG_PAGE_SCSI_DEVICE_1);
	}
	memcpy(hdr, (char *)req->req_vbuf + CFG_DATA_OFF, amt);
	mpt_free_request(mpt, req);
	return (0);
}

int
mpt_write_cfg_page(mpt_softc_t *mpt, int PageAddress, fCONFIG_PAGE_HEADER *hdr)
{
	int count, hdr_attr;
	request_t *req;
	SGE_SIMPLE32 *se;
	MSG_CONFIG *cfgp;
	size_t amt;
	MSG_CONFIG_REPLY *reply;

	req = mpt_get_request(mpt);

	cfgp = req->req_vbuf;
	memset(cfgp, 0, sizeof *cfgp);

	hdr_attr = hdr->PageType & MPI_CONFIG_PAGEATTR_MASK;
	if (hdr_attr != MPI_CONFIG_PAGEATTR_CHANGEABLE &&
	    hdr_attr != MPI_CONFIG_PAGEATTR_PERSISTENT) {
		mpt_prt(mpt, "page type 0x%x not changeable",
		    hdr->PageType & MPI_CONFIG_PAGETYPE_MASK);
		return (-1);
	}
	hdr->PageType &= MPI_CONFIG_PAGETYPE_MASK;

	cfgp->Action = MPI_CONFIG_ACTION_PAGE_WRITE_CURRENT;
	cfgp->Function = MPI_FUNCTION_CONFIG;
	cfgp->Header = *hdr;
 	amt = (cfgp->Header.PageLength * sizeof (u_int32_t));
	cfgp->PageAddress = htole32(PageAddress);

	se = (SGE_SIMPLE32 *) &cfgp->PageBufferSGE;
	se->Address = htole32(req->req_pbuf + CFG_DATA_OFF);
	MPI_pSGE_SET_LENGTH(se, amt);
	MPI_pSGE_SET_FLAGS(se, (MPI_SGE_FLAGS_SIMPLE_ELEMENT |
	    MPI_SGE_FLAGS_LAST_ELEMENT | MPI_SGE_FLAGS_END_OF_BUFFER |
	    MPI_SGE_FLAGS_END_OF_LIST | MPI_SGE_FLAGS_HOST_TO_IOC));
	se->FlagsLength = htole32(se->FlagsLength);

	cfgp->MsgContext = htole32(req->index | 0x80000000);

	if (cfgp->Header.PageType == MPI_CONFIG_PAGETYPE_SCSI_PORT &&
	    cfgp->Header.PageNumber == 0) {
		amt = sizeof (fCONFIG_PAGE_SCSI_PORT_0);
	} else if (cfgp->Header.PageType == MPI_CONFIG_PAGETYPE_SCSI_PORT &&
	    cfgp->Header.PageNumber == 1) {
		amt = sizeof (fCONFIG_PAGE_SCSI_PORT_1);
	} else if (cfgp->Header.PageType == MPI_CONFIG_PAGETYPE_SCSI_PORT &&
	    cfgp->Header.PageNumber == 2) {
		amt = sizeof (fCONFIG_PAGE_SCSI_PORT_2);
	} else if (cfgp->Header.PageType == MPI_CONFIG_PAGETYPE_SCSI_DEVICE  &&
	    cfgp->Header.PageNumber == 0) {
		amt = sizeof (fCONFIG_PAGE_SCSI_DEVICE_0);
	} else if (cfgp->Header.PageType == MPI_CONFIG_PAGETYPE_SCSI_DEVICE  &&
	    cfgp->Header.PageNumber == 1) {
		amt = sizeof (fCONFIG_PAGE_SCSI_DEVICE_1);
	}
	memcpy((char *)req->req_vbuf + CFG_DATA_OFF, hdr, amt);
	/* Restore stripped out attributes */
	hdr->PageType |= hdr_attr;

	mpt_check_doorbell(mpt);
	mpt_send_cmd(mpt, req);
	count = 0;
	do {
		DELAY(500);
		mpt_intr(mpt);
		if (++count == 1000) {
			hdr->PageType |= hdr_attr;
			mpt_prt(mpt, "mpt_write_cfg_page timed out");
			return (-1);
		}
	} while (req->debug == REQ_ON_CHIP);

	reply = (MSG_CONFIG_REPLY *) MPT_REPLY_PTOV(mpt, req->sequence);
        if ((reply->IOCStatus & MPI_IOCSTATUS_MASK) != MPI_IOCSTATUS_SUCCESS) {
		mpt_prt(mpt, "mpt_write_cfg_page: Config Info Status %x",
		    le16toh(reply->IOCStatus));
		mpt_free_reply(mpt, (req->sequence << 1));
		return (-1);
	}
	mpt_free_reply(mpt, (req->sequence << 1));

	mpt_free_request(mpt, req);
	return (0);
}

/*
 * Read SCSI configuration information
 */
static int
mpt_read_config_info_spi(mpt_softc_t *mpt)
{
	int rv, i;

	rv = mpt_read_cfg_header(mpt, MPI_CONFIG_PAGETYPE_SCSI_PORT, 0,
	    0, &mpt->mpt_port_page0.Header);
	if (rv) {
		return (-1);
	}
	if (mpt->verbose > 1) {
		mpt_prt(mpt, "SPI Port Page 0 Header: %x %x %x %x",
		    mpt->mpt_port_page0.Header.PageVersion,
		    mpt->mpt_port_page0.Header.PageLength,
		    mpt->mpt_port_page0.Header.PageNumber,
		    mpt->mpt_port_page0.Header.PageType);
	}

	rv = mpt_read_cfg_header(mpt, MPI_CONFIG_PAGETYPE_SCSI_PORT, 1,
	    0, &mpt->mpt_port_page1.Header);
	if (rv) {
		return (-1);
	}
	if (mpt->verbose > 1) {
		mpt_prt(mpt, "SPI Port Page 1 Header: %x %x %x %x",
		    mpt->mpt_port_page1.Header.PageVersion,
		    mpt->mpt_port_page1.Header.PageLength,
		    mpt->mpt_port_page1.Header.PageNumber,
		    mpt->mpt_port_page1.Header.PageType);
	}

	rv = mpt_read_cfg_header(mpt, MPI_CONFIG_PAGETYPE_SCSI_PORT, 2,
	    0, &mpt->mpt_port_page2.Header);
	if (rv) {
		return (-1);
	}

	if (mpt->verbose > 1) {
		mpt_prt(mpt, "SPI Port Page 2 Header: %x %x %x %x",
		    mpt->mpt_port_page1.Header.PageVersion,
		    mpt->mpt_port_page1.Header.PageLength,
		    mpt->mpt_port_page1.Header.PageNumber,
		    mpt->mpt_port_page1.Header.PageType);
	}

	for (i = 0; i < 16; i++) {
		rv = mpt_read_cfg_header(mpt, MPI_CONFIG_PAGETYPE_SCSI_DEVICE,
		    0, i, &mpt->mpt_dev_page0[i].Header);
		if (rv) {
			return (-1);
		}
		if (mpt->verbose > 1) {
			mpt_prt(mpt,
			    "SPI Target %d Device Page 0 Header: %x %x %x %x",
			    i, mpt->mpt_dev_page0[i].Header.PageVersion,
			    mpt->mpt_dev_page0[i].Header.PageLength,
			    mpt->mpt_dev_page0[i].Header.PageNumber,
			    mpt->mpt_dev_page0[i].Header.PageType);
		}

		rv = mpt_read_cfg_header(mpt, MPI_CONFIG_PAGETYPE_SCSI_DEVICE,
		    1, i, &mpt->mpt_dev_page1[i].Header);
		if (rv) {
			return (-1);
		}
		if (mpt->verbose > 1) {
			mpt_prt(mpt,
			    "SPI Target %d Device Page 1 Header: %x %x %x %x",
			    i, mpt->mpt_dev_page1[i].Header.PageVersion,
			    mpt->mpt_dev_page1[i].Header.PageLength,
			    mpt->mpt_dev_page1[i].Header.PageNumber,
			    mpt->mpt_dev_page1[i].Header.PageType);
		}
	}

	/*
	 * At this point, we don't *have* to fail. As long as we have
	 * valid config header information, we can (barely) lurch
	 * along.
	 */

	rv = mpt_read_cfg_page(mpt, 0, &mpt->mpt_port_page0.Header);
	mpt2host_config_page_scsi_port_0(&mpt->mpt_port_page0);
	if (rv) {
		mpt_prt(mpt, "failed to read SPI Port Page 0");
	} else if (mpt->verbose > 1) {
		mpt_prt(mpt,
		    "SPI Port Page 0: Capabilities %x PhysicalInterface %x",
		    mpt->mpt_port_page0.Capabilities,
		    mpt->mpt_port_page0.PhysicalInterface);
	}

	rv = mpt_read_cfg_page(mpt, 0, &mpt->mpt_port_page1.Header);
	mpt2host_config_page_scsi_port_1(&mpt->mpt_port_page1);
	if (rv) {
		mpt_prt(mpt, "failed to read SPI Port Page 1");
	} else if (mpt->verbose > 1) {
		mpt_prt(mpt,
		    "SPI Port Page 1: Configuration %x OnBusTimerValue %x",
		    mpt->mpt_port_page1.Configuration,
		    mpt->mpt_port_page1.OnBusTimerValue);
	}

	rv = mpt_read_cfg_page(mpt, 0, &mpt->mpt_port_page2.Header);
	mpt2host_config_page_scsi_port_2(&mpt->mpt_port_page2);
	if (rv) {
		mpt_prt(mpt, "failed to read SPI Port Page 2");
	} else if (mpt->verbose > 1) {
		mpt_prt(mpt,
		    "SPI Port Page 2: Flags %x Settings %x",
		    mpt->mpt_port_page2.PortFlags,
		    mpt->mpt_port_page2.PortSettings);
		for (i = 0; i < 1; i++) {
			mpt_prt(mpt,
		  	    "SPI Port Page 2 Tgt %d: timo %x SF %x Flags %x",
			    i, mpt->mpt_port_page2.DeviceSettings[i].Timeout,
			    mpt->mpt_port_page2.DeviceSettings[i].SyncFactor,
			    mpt->mpt_port_page2.DeviceSettings[i].DeviceFlags);
		}
	}

	for (i = 0; i < 16; i++) {
		rv = mpt_read_cfg_page(mpt, i, &mpt->mpt_dev_page0[i].Header);
		mpt2host_config_page_scsi_device_0(&mpt->mpt_dev_page0[i]);
		if (rv) {
			mpt_prt(mpt, "cannot read SPI Tgt %d Device Page 0", i);
			continue;
		}
		if (mpt->verbose > 1) {
			mpt_prt(mpt,
			    "SPI Tgt %d Page 0: NParms %x Information %x",
			    i, mpt->mpt_dev_page0[i].NegotiatedParameters,
			    mpt->mpt_dev_page0[i].Information);
		}
		rv = mpt_read_cfg_page(mpt, i, &mpt->mpt_dev_page1[i].Header);
		mpt2host_config_page_scsi_device_1(&mpt->mpt_dev_page1[i]);
		if (rv) {
			mpt_prt(mpt, "cannot read SPI Tgt %d Device Page 1", i);
			continue;
		}
		if (mpt->verbose > 1) {
			mpt_prt(mpt,
			    "SPI Tgt %d Page 1: RParms %x Configuration %x",
			    i, mpt->mpt_dev_page1[i].RequestedParameters,
			    mpt->mpt_dev_page1[i].Configuration);
		}
	}
	return (0);
}

/*
 * Validate SPI configuration information.
 *
 * In particular, validate SPI Port Page 1.
 */
static int
mpt_set_initial_config_spi(mpt_softc_t *mpt)
{
	int i, pp1val = ((1 << mpt->mpt_ini_id) << 16) | mpt->mpt_ini_id;

	mpt->mpt_disc_enable = 0xff;
	mpt->mpt_tag_enable = 0;

	if (mpt->mpt_port_page1.Configuration != pp1val) {
		fCONFIG_PAGE_SCSI_PORT_1 tmp;

		mpt_prt(mpt,
		    "SPI Port Page 1 Config value bad (%x)- should be %x",
		    mpt->mpt_port_page1.Configuration, pp1val);
		tmp = mpt->mpt_port_page1;
		tmp.Configuration = pp1val;
		host2mpt_config_page_scsi_port_1(&tmp);
		if (mpt_write_cfg_page(mpt, 0, &tmp.Header)) {
			return (-1);
		}
		if (mpt_read_cfg_page(mpt, 0, &tmp.Header)) {
			return (-1);
		}
		mpt2host_config_page_scsi_port_1(&tmp);
		if (tmp.Configuration != pp1val) {
			mpt_prt(mpt,
			    "failed to reset SPI Port Page 1 Config value");
			return (-1);
		}
		mpt->mpt_port_page1 = tmp;
	}

	i = 0;
	for (i = 0; i < 16; i++) {
		fCONFIG_PAGE_SCSI_DEVICE_1 tmp;

		tmp = mpt->mpt_dev_page1[i];
		tmp.RequestedParameters = 0;
		tmp.Configuration = 0;
		if (mpt->verbose > 1) {
			mpt_prt(mpt,
			    "Set Tgt %d SPI DevicePage 1 values to %x 0 %x",
			    i, tmp.RequestedParameters, tmp.Configuration);
		}
		host2mpt_config_page_scsi_device_1(&tmp);
		if (mpt_write_cfg_page(mpt, i, &tmp.Header)) {
			return (-1);
		}
		if (mpt_read_cfg_page(mpt, i, &tmp.Header)) {
			return (-1);
		}
		mpt2host_config_page_scsi_device_1(&tmp);
		mpt->mpt_dev_page1[i] = tmp;
		if (mpt->verbose > 1) {
			mpt_prt(mpt,
		 	    "SPI Tgt %d Page 1: RParm %x Configuration %x", i,
			    mpt->mpt_dev_page1[i].RequestedParameters,
			    mpt->mpt_dev_page1[i].Configuration);
		}
	}
	return (0);
}

/*
 * Enable IOC port
 */
static int
mpt_send_port_enable(mpt_softc_t *mpt, int port)
{
	int count;
	request_t *req;
	MSG_PORT_ENABLE *enable_req;

	req = mpt_get_request(mpt);

	enable_req = req->req_vbuf;
	memset(enable_req, 0, sizeof *enable_req);

	enable_req->Function   = MPI_FUNCTION_PORT_ENABLE;
	enable_req->MsgContext = htole32(req->index | 0x80000000);
	enable_req->PortNumber = port;

	mpt_check_doorbell(mpt);
	if (mpt->verbose > 1) {
		mpt_prt(mpt, "enabling port %d", port);
	}
	mpt_send_cmd(mpt, req);

	count = 0;
	do {
		DELAY(500);
		mpt_intr(mpt);
		if (++count == 100000) {
			mpt_prt(mpt, "port enable timed out");
			return (-1);
		}
	} while (req->debug == REQ_ON_CHIP);
	mpt_free_request(mpt, req);
	return (0);
}

/*
 * Enable/Disable asynchronous event reporting.
 *
 * NB: this is the first command we send via shared memory
 * instead of the handshake register.
 */
static int
mpt_send_event_request(mpt_softc_t *mpt, int onoff)
{
	request_t *req;
	MSG_EVENT_NOTIFY *enable_req;

	req = mpt_get_request(mpt);

	enable_req = req->req_vbuf;
	memset(enable_req, 0, sizeof *enable_req);

	enable_req->Function   = MPI_FUNCTION_EVENT_NOTIFICATION;
	enable_req->MsgContext = htole32(req->index | 0x80000000);
	enable_req->Switch     = onoff;

	mpt_check_doorbell(mpt);
	if (mpt->verbose > 1) {
		mpt_prt(mpt, "%sabling async events", onoff? "en" : "dis");
	}
	mpt_send_cmd(mpt, req);

	return (0);
}

/*
 * Un-mask the interrupts on the chip.
 */
void
mpt_enable_ints(mpt_softc_t *mpt)
{
	/* Unmask every thing except door bell int */
	mpt_write(mpt, MPT_OFFSET_INTR_MASK, MPT_INTR_DB_MASK);
}

/*
 * Mask the interrupts on the chip.
 */
void
mpt_disable_ints(mpt_softc_t *mpt)
{
	/* Mask all interrupts */
	mpt_write(mpt, MPT_OFFSET_INTR_MASK,
	    MPT_INTR_REPLY_MASK | MPT_INTR_DB_MASK);
}

/* (Re)Initialize the chip for use */
int
mpt_hw_init(mpt_softc_t *mpt)
{
	u_int32_t	db;
	int		try;

	/*
	 * Start by making sure we're not at FAULT or RESET state
	 */
	for (try = 0; try < MPT_MAX_TRYS; try++) {

		db = mpt_rd_db(mpt);

		switch (MPT_STATE(db)) {
		case MPT_DB_STATE_READY:
			return (0);

		default:
			/* if peer has already reset us, don't do it again! */
			if (MPT_WHO(db) == MPT_DB_INIT_PCIPEER)
				return (0);
			/*FALLTHRU*/
		case MPT_DB_STATE_RESET:
		case MPT_DB_STATE_FAULT:
			if (mpt_reset(mpt) != MPT_OK) {
				DELAY(10000);
				continue;
			}
			break;
		}
	}
	return (EIO);
}

int
mpt_init(mpt_softc_t *mpt, u_int32_t who)
{
        int try;
        MSG_IOC_FACTS_REPLY facts;
        MSG_PORT_FACTS_REPLY pfp;
        prop_dictionary_t dict;
        uint32_t ini_id;
        uint32_t pptr;
        int val;

	/* Put all request buffers (back) on the free list */
        SLIST_INIT(&mpt->request_free_list);
	for (val = 0; val < MPT_MAX_REQUESTS(mpt); val++) {
		mpt_free_request(mpt, &mpt->request_pool[val]);
	}

	if (mpt->verbose > 1) {
		mpt_prt(mpt, "doorbell req = %s",
		    mpt_ioc_diag(mpt_read(mpt, MPT_OFFSET_DOORBELL)));
	}

	/*
	 * Start by making sure we're not at FAULT or RESET state
	 */
	if (mpt_hw_init(mpt) != 0)
		return (EIO);

	dict = device_properties(mpt->sc_dev);

	for (try = 0; try < MPT_MAX_TRYS; try++) {
		/*
		 * No need to reset if the IOC is already in the READY state.
		 */

		if (mpt_get_iocfacts(mpt, &facts) != MPT_OK) {
			mpt_prt(mpt, "mpt_get_iocfacts failed");
			continue;
		}
		mpt2host_iocfacts_reply(&facts);

		if (mpt->verbose > 1) {
			mpt_prt(mpt,
			    "IOCFACTS: GlobalCredits=%d BlockSize=%u "
			    "Request Frame Size %u", facts.GlobalCredits,
			    facts.BlockSize, facts.RequestFrameSize);
		}
		mpt->mpt_max_devices = facts.MaxDevices;
		mpt->mpt_global_credits = facts.GlobalCredits;
		mpt->request_frame_size = facts.RequestFrameSize;

		if (mpt_get_portfacts(mpt, &pfp) != MPT_OK) {
			mpt_prt(mpt, "mpt_get_portfacts failed");
			continue;
		}
		mpt2host_portfacts_reply(&pfp);

		if (mpt->verbose > 1) {
			mpt_prt(mpt,
			    "PORTFACTS: Type %x PFlags %x IID %d MaxDev %d",
			    pfp.PortType, pfp.ProtocolFlags, pfp.PortSCSIID,
			    pfp.MaxDevices);
		}

		if (!(pfp.ProtocolFlags & MPI_PORTFACTS_PROTOCOL_INITIATOR)) {
			mpt_prt(mpt, "initiator role unsupported");
			return (ENXIO);
		}

		switch (pfp.PortType) {
		case MPI_PORTFACTS_PORTTYPE_FC:
			mpt->is_fc = 1;
			mpt->mpt_max_devices = 255;
			break;
		case MPI_PORTFACTS_PORTTYPE_SCSI:
			mpt->is_scsi = 1;
			/* some SPI controllers (VMWare, Sun) lie */
			mpt->mpt_max_devices = 16;
			break;
		case MPI_PORTFACTS_PORTTYPE_SAS:
			mpt->is_sas = 1;
			break;
		default:
			mpt_prt(mpt, "Unsupported Port Type (%x)",
			    pfp.PortType);
			return (ENXIO);
		}

		if (!mpt->is_sas && !mpt->is_fc &&
		    prop_dictionary_get_uint32(dict, "scsi-initiator-id", &ini_id))
			mpt->mpt_ini_id = ini_id;
		else
			mpt->mpt_ini_id = pfp.PortSCSIID;

		if (mpt_send_ioc_init(mpt, who) != MPT_OK) {
			mpt_prt(mpt, "mpt_send_ioc_init failed");
			continue;
		}

		if (mpt->verbose > 1) {
			mpt_prt(mpt, "mpt_send_ioc_init ok");
		}

		if (mpt_wait_state(mpt, MPT_DB_STATE_RUNNING) != MPT_OK) {
			mpt_prt(mpt, "IOC failed to go to run state");
			continue;
		}
		if (mpt->verbose > 1) {
			mpt_prt(mpt, "IOC now at RUNSTATE");
		}

		/*
		 * Give it reply buffers
		 *
		 * Do *not* except global credits.
		 */
		for (val = 0, pptr = mpt->reply_phys;
		    (pptr + MPT_REPLY_SIZE) < (mpt->reply_phys + PAGE_SIZE);
		     pptr += MPT_REPLY_SIZE) {
			mpt_free_reply(mpt, pptr);
			if (++val == mpt->mpt_global_credits - 1)
				break;
		}

		/*
		 * Enable asynchronous event reporting
		 */
		mpt_send_event_request(mpt, 1);


		/*
		 * Read set up initial configuration information
		 * (SPI only for now)
		 */

		if (mpt->is_scsi) {
			if (mpt_read_config_info_spi(mpt)) {
				return (EIO);
			}
			if (mpt_set_initial_config_spi(mpt)) {
				return (EIO);
			}
		}

		/*
		 * Now enable the port
		 */
		if (mpt_send_port_enable(mpt, 0) != MPT_OK) {
			mpt_prt(mpt, "failed to enable port 0");
			continue;
		}

		if (mpt->verbose > 1) {
			mpt_prt(mpt, "enabled port 0");
		}

		/* Everything worked */
		break;
	}

	if (try >= MPT_MAX_TRYS) {
		mpt_prt(mpt, "failed to initialize IOC");
		return (EIO);
	}

	if (mpt->verbose > 1) {
		mpt_prt(mpt, "enabling interrupts");
	}

	mpt_enable_ints(mpt);
	return (0);
}

/*
 * Endian Conversion Functions- only used on Big Endian machines
 */
#if	_BYTE_ORDER == _BIG_ENDIAN
void
mpt2host_sge_simple_union(SGE_SIMPLE_UNION *sge)
{

	MPT_2_HOST32(sge, FlagsLength);
	MPT_2_HOST32(sge, _u.Address64.Low);
	MPT_2_HOST32(sge, _u.Address64.High);
}

void
mpt2host_iocfacts_reply(MSG_IOC_FACTS_REPLY *rp)
{

	MPT_2_HOST16(rp, MsgVersion);
#if 0
	MPT_2_HOST16(rp, HeaderVersion);
#endif
	MPT_2_HOST32(rp, MsgContext);
	MPT_2_HOST16(rp, IOCExceptions);
	MPT_2_HOST16(rp, IOCStatus);
	MPT_2_HOST32(rp, IOCLogInfo);
	MPT_2_HOST16(rp, ReplyQueueDepth);
	MPT_2_HOST16(rp, RequestFrameSize);
	MPT_2_HOST16(rp, Reserved_0101_FWVersion);
	MPT_2_HOST16(rp, ProductID);
	MPT_2_HOST32(rp, CurrentHostMfaHighAddr);
	MPT_2_HOST16(rp, GlobalCredits);
	MPT_2_HOST32(rp, CurrentSenseBufferHighAddr);
	MPT_2_HOST16(rp, CurReplyFrameSize);
	MPT_2_HOST32(rp, FWImageSize);
#if 0
	MPT_2_HOST32(rp, IOCCapabilities);
#endif
	MPT_2_HOST32(rp, FWVersion.Word);
#if 0
	MPT_2_HOST16(rp, HighPriorityQueueDepth);
	MPT_2_HOST16(rp, Reserved2);
	mpt2host_sge_simple_union(&rp->HostPageBufferSGE);
	MPT_2_HOST32(rp, ReplyFifoHostSignalingAddr);
#endif
}

void
mpt2host_portfacts_reply(MSG_PORT_FACTS_REPLY *pfp)
{

	MPT_2_HOST16(pfp, Reserved);
	MPT_2_HOST16(pfp, Reserved1);
	MPT_2_HOST32(pfp, MsgContext);
	MPT_2_HOST16(pfp, Reserved2);
	MPT_2_HOST16(pfp, IOCStatus);
	MPT_2_HOST32(pfp, IOCLogInfo);
	MPT_2_HOST16(pfp, MaxDevices);
	MPT_2_HOST16(pfp, PortSCSIID);
	MPT_2_HOST16(pfp, ProtocolFlags);
	MPT_2_HOST16(pfp, MaxPostedCmdBuffers);
	MPT_2_HOST16(pfp, MaxPersistentIDs);
	MPT_2_HOST16(pfp, MaxLanBuckets);
	MPT_2_HOST16(pfp, Reserved4);
	MPT_2_HOST32(pfp, Reserved5);
}

void
mpt2host_config_page_scsi_port_0(fCONFIG_PAGE_SCSI_PORT_0 *sp0)
{

	MPT_2_HOST32(sp0, Capabilities);
	MPT_2_HOST32(sp0, PhysicalInterface);
}

void
mpt2host_config_page_scsi_port_1(fCONFIG_PAGE_SCSI_PORT_1 *sp1)
{

	MPT_2_HOST32(sp1, Configuration);
	MPT_2_HOST32(sp1, OnBusTimerValue);
#if 0
	MPT_2_HOST16(sp1, IDConfig);
#endif
}

void
host2mpt_config_page_scsi_port_1(fCONFIG_PAGE_SCSI_PORT_1 *sp1)
{

	HOST_2_MPT32(sp1, Configuration);
	HOST_2_MPT32(sp1, OnBusTimerValue);
#if 0
	HOST_2_MPT16(sp1, IDConfig);
#endif
}

void
mpt2host_config_page_scsi_port_2(fCONFIG_PAGE_SCSI_PORT_2 *sp2)
{
	int i;

	MPT_2_HOST32(sp2, PortFlags);
	MPT_2_HOST32(sp2, PortSettings);
	for (i = 0; i < sizeof(sp2->DeviceSettings) /
	    sizeof(*sp2->DeviceSettings); i++) {
		MPT_2_HOST16(sp2, DeviceSettings[i].DeviceFlags);
	}
}

void
mpt2host_config_page_scsi_device_0(fCONFIG_PAGE_SCSI_DEVICE_0 *sd0)
{

	MPT_2_HOST32(sd0, NegotiatedParameters);
	MPT_2_HOST32(sd0, Information);
}

void
host2mpt_config_page_scsi_device_0(fCONFIG_PAGE_SCSI_DEVICE_0 *sd0)
{

	HOST_2_MPT32(sd0, NegotiatedParameters);
	HOST_2_MPT32(sd0, Information);
}

void
mpt2host_config_page_scsi_device_1(fCONFIG_PAGE_SCSI_DEVICE_1 *sd1)
{

	MPT_2_HOST32(sd1, RequestedParameters);
	MPT_2_HOST32(sd1, Reserved);
	MPT_2_HOST32(sd1, Configuration);
}

void
host2mpt_config_page_scsi_device_1(fCONFIG_PAGE_SCSI_DEVICE_1 *sd1)
{

	HOST_2_MPT32(sd1, RequestedParameters);
	HOST_2_MPT32(sd1, Reserved);
	HOST_2_MPT32(sd1, Configuration);
}

void
mpt2host_config_page_fc_port_0(fCONFIG_PAGE_FC_PORT_0 *fp0)
{

	MPT_2_HOST32(fp0, Flags);
	MPT_2_HOST32(fp0, PortIdentifier);
	MPT_2_HOST32(fp0, WWNN.Low);
	MPT_2_HOST32(fp0, WWNN.High);
	MPT_2_HOST32(fp0, WWPN.Low);
	MPT_2_HOST32(fp0, WWPN.High);
	MPT_2_HOST32(fp0, SupportedServiceClass);
	MPT_2_HOST32(fp0, SupportedSpeeds);
	MPT_2_HOST32(fp0, CurrentSpeed);
	MPT_2_HOST32(fp0, MaxFrameSize);
	MPT_2_HOST32(fp0, FabricWWNN.Low);
	MPT_2_HOST32(fp0, FabricWWNN.High);
	MPT_2_HOST32(fp0, FabricWWPN.Low);
	MPT_2_HOST32(fp0, FabricWWPN.High);
	MPT_2_HOST32(fp0, DiscoveredPortsCount);
	MPT_2_HOST32(fp0, MaxInitiators);
}

void
mpt2host_config_page_fc_port_1(fCONFIG_PAGE_FC_PORT_1 *fp1)
{

	MPT_2_HOST32(fp1, Flags);
	MPT_2_HOST32(fp1, NoSEEPROMWWNN.Low);
	MPT_2_HOST32(fp1, NoSEEPROMWWNN.High);
	MPT_2_HOST32(fp1, NoSEEPROMWWPN.Low);
	MPT_2_HOST32(fp1, NoSEEPROMWWPN.High);
}

void
host2mpt_config_page_fc_port_1(fCONFIG_PAGE_FC_PORT_1 *fp1)
{

	HOST_2_MPT32(fp1, Flags);
	HOST_2_MPT32(fp1, NoSEEPROMWWNN.Low);
	HOST_2_MPT32(fp1, NoSEEPROMWWNN.High);
	HOST_2_MPT32(fp1, NoSEEPROMWWPN.Low);
	HOST_2_MPT32(fp1, NoSEEPROMWWPN.High);
}

void
mpt2host_config_page_raid_vol_0(fCONFIG_PAGE_RAID_VOL_0 *volp)
{
	int i;

	MPT_2_HOST16(volp, VolumeStatus.Reserved);
	MPT_2_HOST16(volp, VolumeSettings.Settings);
	MPT_2_HOST32(volp, MaxLBA);
#if 0
	MPT_2_HOST32(volp, MaxLBAHigh);
#endif
	MPT_2_HOST32(volp, StripeSize);
	MPT_2_HOST32(volp, Reserved2);
	MPT_2_HOST32(volp, Reserved3);
	for (i = 0; i < MPI_RAID_VOL_PAGE_0_PHYSDISK_MAX; i++) {
		MPT_2_HOST16(volp, PhysDisk[i].Reserved);
	}
}

void
mpt2host_config_page_raid_phys_disk_0(fCONFIG_PAGE_RAID_PHYS_DISK_0 *rpd0)
{

	MPT_2_HOST32(rpd0, Reserved1);
	MPT_2_HOST16(rpd0, PhysDiskStatus.Reserved);
	MPT_2_HOST32(rpd0, MaxLBA);
	MPT_2_HOST16(rpd0, ErrorData.Reserved);
	MPT_2_HOST16(rpd0, ErrorData.ErrorCount);
	MPT_2_HOST16(rpd0, ErrorData.SmartCount);
}

void
mpt2host_config_page_ioc_2(fCONFIG_PAGE_IOC_2 *ioc2)
{
	MPT_2_HOST32(ioc2, CapabilitiesFlags);
}

#endif
