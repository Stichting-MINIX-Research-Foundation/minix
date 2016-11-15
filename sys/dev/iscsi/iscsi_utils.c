/*	$NetBSD: iscsi_utils.c,v 1.8 2015/05/30 18:12:09 joerg Exp $	*/

/*-
 * Copyright (c) 2004,2005,2006,2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Wasabi Systems, Inc.
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
#include "iscsi_globals.h"

#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/socketvar.h>
#include <sys/bswap.h>


#ifdef ISCSI_DEBUG

/* debug helper routine */
void
dump(void *buff, int len)
{
	uint8_t *bp = (uint8_t *) buff;
	int i;

	while (len > 0) {
		for (i = min(16, len); i > 0; i--)
			printf("%02x ", *bp++);
		printf("\n");
		len -= 16;
	}
}

#endif

/*****************************************************************************
 * Digest functions
 *****************************************************************************/

/*****************************************************************
 *
 * CRC LOOKUP TABLE
 * ================
 * The following CRC lookup table was generated automagically
 * by the Rocksoft^tm Model CRC Algorithm Table Generation
 * Program V1.0 using the following model parameters:
 *
 *    Width   : 4 bytes.
 *    Poly    : 0x1EDC6F41L
 *    Reverse : TRUE.
 *
 * For more information on the Rocksoft^tm Model CRC Algorithm,
 * see the document titled "A Painless Guide to CRC Error
 * Detection Algorithms" by Ross Williams
 * (ross@guest.adelaide.edu.au.). This document is likely to be
 * in the FTP archive "ftp.adelaide.edu.au/pub/rocksoft".
 *
 *****************************************************************/

STATIC uint32_t crc_table[256] = {
	0x00000000L, 0xF26B8303L, 0xE13B70F7L, 0x1350F3F4L,
	0xC79A971FL, 0x35F1141CL, 0x26A1E7E8L, 0xD4CA64EBL,
	0x8AD958CFL, 0x78B2DBCCL, 0x6BE22838L, 0x9989AB3BL,
	0x4D43CFD0L, 0xBF284CD3L, 0xAC78BF27L, 0x5E133C24L,
	0x105EC76FL, 0xE235446CL, 0xF165B798L, 0x030E349BL,
	0xD7C45070L, 0x25AFD373L, 0x36FF2087L, 0xC494A384L,
	0x9A879FA0L, 0x68EC1CA3L, 0x7BBCEF57L, 0x89D76C54L,
	0x5D1D08BFL, 0xAF768BBCL, 0xBC267848L, 0x4E4DFB4BL,
	0x20BD8EDEL, 0xD2D60DDDL, 0xC186FE29L, 0x33ED7D2AL,
	0xE72719C1L, 0x154C9AC2L, 0x061C6936L, 0xF477EA35L,
	0xAA64D611L, 0x580F5512L, 0x4B5FA6E6L, 0xB93425E5L,
	0x6DFE410EL, 0x9F95C20DL, 0x8CC531F9L, 0x7EAEB2FAL,
	0x30E349B1L, 0xC288CAB2L, 0xD1D83946L, 0x23B3BA45L,
	0xF779DEAEL, 0x05125DADL, 0x1642AE59L, 0xE4292D5AL,
	0xBA3A117EL, 0x4851927DL, 0x5B016189L, 0xA96AE28AL,
	0x7DA08661L, 0x8FCB0562L, 0x9C9BF696L, 0x6EF07595L,
	0x417B1DBCL, 0xB3109EBFL, 0xA0406D4BL, 0x522BEE48L,
	0x86E18AA3L, 0x748A09A0L, 0x67DAFA54L, 0x95B17957L,
	0xCBA24573L, 0x39C9C670L, 0x2A993584L, 0xD8F2B687L,
	0x0C38D26CL, 0xFE53516FL, 0xED03A29BL, 0x1F682198L,
	0x5125DAD3L, 0xA34E59D0L, 0xB01EAA24L, 0x42752927L,
	0x96BF4DCCL, 0x64D4CECFL, 0x77843D3BL, 0x85EFBE38L,
	0xDBFC821CL, 0x2997011FL, 0x3AC7F2EBL, 0xC8AC71E8L,
	0x1C661503L, 0xEE0D9600L, 0xFD5D65F4L, 0x0F36E6F7L,
	0x61C69362L, 0x93AD1061L, 0x80FDE395L, 0x72966096L,
	0xA65C047DL, 0x5437877EL, 0x4767748AL, 0xB50CF789L,
	0xEB1FCBADL, 0x197448AEL, 0x0A24BB5AL, 0xF84F3859L,
	0x2C855CB2L, 0xDEEEDFB1L, 0xCDBE2C45L, 0x3FD5AF46L,
	0x7198540DL, 0x83F3D70EL, 0x90A324FAL, 0x62C8A7F9L,
	0xB602C312L, 0x44694011L, 0x5739B3E5L, 0xA55230E6L,
	0xFB410CC2L, 0x092A8FC1L, 0x1A7A7C35L, 0xE811FF36L,
	0x3CDB9BDDL, 0xCEB018DEL, 0xDDE0EB2AL, 0x2F8B6829L,
	0x82F63B78L, 0x709DB87BL, 0x63CD4B8FL, 0x91A6C88CL,
	0x456CAC67L, 0xB7072F64L, 0xA457DC90L, 0x563C5F93L,
	0x082F63B7L, 0xFA44E0B4L, 0xE9141340L, 0x1B7F9043L,
	0xCFB5F4A8L, 0x3DDE77ABL, 0x2E8E845FL, 0xDCE5075CL,
	0x92A8FC17L, 0x60C37F14L, 0x73938CE0L, 0x81F80FE3L,
	0x55326B08L, 0xA759E80BL, 0xB4091BFFL, 0x466298FCL,
	0x1871A4D8L, 0xEA1A27DBL, 0xF94AD42FL, 0x0B21572CL,
	0xDFEB33C7L, 0x2D80B0C4L, 0x3ED04330L, 0xCCBBC033L,
	0xA24BB5A6L, 0x502036A5L, 0x4370C551L, 0xB11B4652L,
	0x65D122B9L, 0x97BAA1BAL, 0x84EA524EL, 0x7681D14DL,
	0x2892ED69L, 0xDAF96E6AL, 0xC9A99D9EL, 0x3BC21E9DL,
	0xEF087A76L, 0x1D63F975L, 0x0E330A81L, 0xFC588982L,
	0xB21572C9L, 0x407EF1CAL, 0x532E023EL, 0xA145813DL,
	0x758FE5D6L, 0x87E466D5L, 0x94B49521L, 0x66DF1622L,
	0x38CC2A06L, 0xCAA7A905L, 0xD9F75AF1L, 0x2B9CD9F2L,
	0xFF56BD19L, 0x0D3D3E1AL, 0x1E6DCDEEL, 0xEC064EEDL,
	0xC38D26C4L, 0x31E6A5C7L, 0x22B65633L, 0xD0DDD530L,
	0x0417B1DBL, 0xF67C32D8L, 0xE52CC12CL, 0x1747422FL,
	0x49547E0BL, 0xBB3FFD08L, 0xA86F0EFCL, 0x5A048DFFL,
	0x8ECEE914L, 0x7CA56A17L, 0x6FF599E3L, 0x9D9E1AE0L,
	0xD3D3E1ABL, 0x21B862A8L, 0x32E8915CL, 0xC083125FL,
	0x144976B4L, 0xE622F5B7L, 0xF5720643L, 0x07198540L,
	0x590AB964L, 0xAB613A67L, 0xB831C993L, 0x4A5A4A90L,
	0x9E902E7BL, 0x6CFBAD78L, 0x7FAB5E8CL, 0x8DC0DD8FL,
	0xE330A81AL, 0x115B2B19L, 0x020BD8EDL, 0xF0605BEEL,
	0x24AA3F05L, 0xD6C1BC06L, 0xC5914FF2L, 0x37FACCF1L,
	0x69E9F0D5L, 0x9B8273D6L, 0x88D28022L, 0x7AB90321L,
	0xAE7367CAL, 0x5C18E4C9L, 0x4F48173DL, 0xBD23943EL,
	0xF36E6F75L, 0x0105EC76L, 0x12551F82L, 0xE03E9C81L,
	0x34F4F86AL, 0xC69F7B69L, 0xD5CF889DL, 0x27A40B9EL,
	0x79B737BAL, 0x8BDCB4B9L, 0x988C474DL, 0x6AE7C44EL,
	0xBE2DA0A5L, 0x4C4623A6L, 0x5F16D052L, 0xAD7D5351L
};


/*
 * gen_digest:
 *    Generate an iSCSI CRC32C digest over the given data.
 *
 *    Parameters:
 *          buff   The data
 *          len   The length of the data in bytes
 *
 *    Returns:    The digest in network byte order
 */

uint32_t
gen_digest(void *buff, int len)
{
	uint8_t *bp = (uint8_t *) buff;
	uint32_t crc = 0xffffffff;

	while (len--) {
		crc = ((crc >> 8) & 0x00ffffff) ^ crc_table[(crc ^ *bp++) & 0xff];
	}
	return htonl(bswap32(crc ^ 0xffffffff));
}


/*
 * gen_digest_2:
 *    Generate an iSCSI CRC32C digest over the given data, which is split over
 *    two buffers.
 *
 *    Parameters:
 *          buf1, buf2  The data
 *          len1, len2  The length of the data in bytes
 *
 *    Returns:    The digest in network byte order
 */

uint32_t
gen_digest_2(void *buf1, int len1, void *buf2, int len2)
{
	uint8_t *bp = (uint8_t *) buf1;
	uint32_t crc = 0xffffffff;

	while (len1--) {
		crc = ((crc >> 8) & 0x00ffffff) ^ crc_table[(crc ^ *bp++) & 0xff];
	}
	bp = (uint8_t *) buf2;
	while (len2--) {
		crc = ((crc >> 8) & 0x00ffffff) ^ crc_table[(crc ^ *bp++) & 0xff];
	}
	return htonl(bswap32(crc ^ 0xffffffff));
}

/*****************************************************************************
 * CCB management functions
 *****************************************************************************/

/*
 * get_ccb:
 *    Get a CCB for the SCSI operation, waiting if none is available.
 *
 *    Parameter:
 *       sess     The session containing this CCB
 *       waitok   Whether waiting for a CCB is OK
 *
 *    Returns:    The CCB.
 */

ccb_t *
get_ccb(connection_t *conn, bool waitok)
{
	ccb_t *ccb;
	session_t *sess = conn->session;
	int s;

	do {
		s = splbio();
		ccb = TAILQ_FIRST(&sess->ccb_pool);
		if (ccb != NULL)
			TAILQ_REMOVE(&sess->ccb_pool, ccb, chain);
		splx(s);

		DEB(100, ("get_ccb: ccb = %p, waitok = %d\n", ccb, waitok));
		if (ccb == NULL) {
			if (!waitok || conn->terminating) {
				return NULL;
			}
			tsleep(&sess->ccb_pool, PWAIT, "get_ccb", 0);
		}
	} while (ccb == NULL);

	ccb->flags = 0;
	ccb->xs = NULL;
	ccb->temp_data = NULL;
	ccb->text_data = NULL;
	ccb->status = ISCSI_STATUS_SUCCESS;
	ccb->ITT = (ccb->ITT & 0xffffff) | (++sess->itt_id << 24);
	ccb->disp = CCBDISP_NOWAIT;
	ccb->connection = conn;
	conn->usecount++;

	return ccb;
}

/*
 * free_ccb:
 *    Put a CCB back onto the free list.
 *
 *    Parameter:  The CCB.
 */

void
free_ccb(ccb_t *ccb)
{
	session_t *sess = ccb->session;
	pdu_t *pdu;
	int s;

	KASSERT((ccb->flags & CCBF_THROTTLING) == 0);
	KASSERT((ccb->flags & CCBF_WAITQUEUE) == 0);

	ccb->connection->usecount--;
	ccb->connection = NULL;

	ccb->disp = CCBDISP_UNUSED;

	/* free temporary data */
	if (ccb->temp_data != NULL) {
		free(ccb->temp_data, M_TEMP);
	}
	if (ccb->text_data != NULL) {
		free(ccb->text_data, M_TEMP);
	}
	/* free PDU waiting for ACK */
	if ((pdu = ccb->pdu_waiting) != NULL) {
		ccb->pdu_waiting = NULL;
		free_pdu(pdu);
	}

	s = splbio();
	TAILQ_INSERT_TAIL(&sess->ccb_pool, ccb, chain);
	splx(s);

	wakeup(&sess->ccb_pool);
}

/*
 *    create_ccbs
 *       "Create" the pool of CCBs. This doesn't actually create the CCBs
 *       (they are allocated with the session structure), but it links them
 *       into the free-list.
 *
 *    Parameter:  The session owning the CCBs.
 */

void
create_ccbs(session_t *sess)
{
	int i;
	ccb_t *ccb;
	int sid = sess->id << 8;

	/* Note: CCBs are initialized to 0 with connection structure */

	for (i = 0, ccb = sess->ccb; i < CCBS_PER_SESSION; i++, ccb++) {
		ccb->ITT = i | sid;
		ccb->session = sess;

		callout_init(&ccb->timeout, 0);
		callout_setfunc(&ccb->timeout, ccb_timeout, ccb);

		/*DEB (9, ("Create_ccbs: ccb %x itt %x\n", ccb, ccb->ITT)); */
		TAILQ_INSERT_HEAD(&sess->ccb_pool, ccb, chain);
	}
}

/*
 * suspend_ccb:
 *    Put CCB on wait queue
 */
void
suspend_ccb(ccb_t *ccb, bool yes)
{
	connection_t *conn;

	conn = ccb->connection;
	if (yes) {
		KASSERT((ccb->flags & CCBF_THROTTLING) == 0);
		KASSERT((ccb->flags & CCBF_WAITQUEUE) == 0);
		TAILQ_INSERT_TAIL(&conn->ccbs_waiting, ccb, chain);
		ccb->flags |= CCBF_WAITQUEUE;
	} else if (ccb->flags & CCBF_WAITQUEUE) {
		KASSERT((ccb->flags & CCBF_THROTTLING) == 0);
		TAILQ_REMOVE(&conn->ccbs_waiting, ccb, chain);
		ccb->flags &= ~CCBF_WAITQUEUE;
	}
}

/*
 * throttle_ccb:
 *    Put CCB on throttling queue
 */
void
throttle_ccb(ccb_t *ccb, bool yes)
{
	session_t *sess;

	sess = ccb->session;
	if (yes) {
		KASSERT((ccb->flags & CCBF_THROTTLING) == 0);
		KASSERT((ccb->flags & CCBF_WAITQUEUE) == 0);
		TAILQ_INSERT_TAIL(&sess->ccbs_throttled, ccb, chain);
		ccb->flags |= CCBF_THROTTLING;
	} else if (ccb->flags & CCBF_THROTTLING) {
		KASSERT((ccb->flags & CCBF_WAITQUEUE) == 0);
		TAILQ_REMOVE(&sess->ccbs_throttled, ccb, chain);
		ccb->flags &= ~CCBF_THROTTLING;
	}
}


/*
 * wake_ccb:
 *    Wake up (or dispose of) a CCB. Depending on the CCB's disposition,
 *    either wake up the requesting thread, signal SCSIPI that we're done,
 *    or just free the CCB for CCBDISP_FREE.
 *
 *    Parameter:  The CCB to handle and the new status of the CCB
 */

void
wake_ccb(ccb_t *ccb, uint32_t status)
{
	ccb_disp_t disp;
	connection_t *conn;
	int s;

	conn = ccb->connection;

#ifdef ISCSI_DEBUG
	DEBC(conn, 9, ("CCB done, ccb = %p, disp = %d\n",
		ccb, ccb->disp));
#endif

	callout_stop(&ccb->timeout);

	s = splbio();
	disp = ccb->disp;
	if (disp <= CCBDISP_NOWAIT ||
		(disp == CCBDISP_DEFER && conn->state <= ST_WINDING_DOWN)) {
		splx(s);
		return;
	}

	suspend_ccb(ccb, FALSE);
	throttle_ccb(ccb, FALSE);

	/* change the disposition so nobody tries this again */
	ccb->disp = CCBDISP_BUSY;
	ccb->status = status;
	splx(s);

	switch (disp) {
	case CCBDISP_FREE:
		free_ccb(ccb);
		break;

	case CCBDISP_WAIT:
		wakeup(ccb);
		break;

	case CCBDISP_SCSIPI:
		iscsi_done(ccb);
		free_ccb(ccb);
		break;

	case CCBDISP_DEFER:
		break;

	default:
		DEBC(conn, 1, ("CCB done, ccb = %p, invalid disposition %d", ccb, disp));
		free_ccb(ccb);
		break;
	}
}

/*****************************************************************************
 * PDU management functions
 *****************************************************************************/

/*
 * get_pdu:
 *    Get a PDU for the SCSI operation.
 *
 *    Parameter:
 *          conn     The connection this PDU should be associated with
 *          waitok   OK to wait for PDU if TRUE
 *
 *    Returns:    The PDU or NULL if none is available and waitok is FALSE.
 */

pdu_t *
get_pdu(connection_t *conn, bool waitok)
{
	pdu_t *pdu;
	int s;

	do {
		s = splbio();
		pdu = TAILQ_FIRST(&conn->pdu_pool);
		if (pdu != NULL)
			TAILQ_REMOVE(&conn->pdu_pool, pdu, chain);
		splx(s);

		DEB(100, ("get_pdu_c: pdu = %p, waitok = %d\n", pdu, waitok));
		if (pdu == NULL) {
			if (!waitok || conn->terminating)
				return NULL;
			tsleep(&conn->pdu_pool, PWAIT, "get_pdu_c", 0);
		}
	} while (pdu == NULL);

	memset(pdu, 0, sizeof(pdu_t));
	pdu->connection = conn;
	pdu->disp = PDUDISP_FREE;

	return pdu;
}

/*
 * free_pdu:
 *    Put a PDU back onto the free list.
 *
 *    Parameter:  The PDU.
 */

void
free_pdu(pdu_t *pdu)
{
	connection_t *conn = pdu->connection;
	pdu_disp_t pdisp;
	int s;

	if (PDUDISP_UNUSED == (pdisp = pdu->disp))
		return;
	pdu->disp = PDUDISP_UNUSED;

	if (pdu->flags & PDUF_INQUEUE) {
		TAILQ_REMOVE(&conn->pdus_to_send, pdu, send_chain);
		pdu->flags &= ~PDUF_INQUEUE;
	}

	if (pdisp == PDUDISP_SIGNAL)
		wakeup(pdu);

	/* free temporary data in this PDU */
	if (pdu->temp_data)
		free(pdu->temp_data, M_TEMP);

	s = splbio();
	TAILQ_INSERT_TAIL(&conn->pdu_pool, pdu, chain);
	splx(s);

	wakeup(&conn->pdu_pool);
}

/*
 *    create_pdus
 *       "Create" the pool of PDUs. This doesn't actually create the PDUs
 *       (they are allocated with the connection structure), but it links them
 *       into the free-list.
 *
 *    Parameter:  The connection owning the PDUs.
 */

void
create_pdus(connection_t *conn)
{
	int i;
	pdu_t *pdu;

	/* Note: PDUs are initialized to 0 with connection structure */

	for (i = 0, pdu = conn->pdu; i < PDUS_PER_CONNECTION; i++, pdu++) {
		TAILQ_INSERT_HEAD(&conn->pdu_pool, pdu, chain);
	}
}


/*****************************************************************************
 * Serial Number management functions
 *****************************************************************************/

/*
 * init_sernum:
 *    Initialize serial number buffer variables.
 *
 *    Parameter:
 *          buff   The serial number buffer.
 */

void
init_sernum(sernum_buffer_t *buff)
{

	buff->bottom = 0;
	buff->top = 0;
	buff->next_sn = 0;
	buff->ExpSN = 0;
}


/*
 * add_sernum:
 *    Add a received serial number to the buffer.
 *    If the serial number is smaller than the expected one, it is ignored.
 *    If it is larger, all missing serial numbers are added as well.
 *
 *    Parameter:
 *          buff   The serial number buffer.
 *          num   The received serial number
 *
 *    Returns:
 *          0     if the received block is a duplicate
 *          1     if the number is the expected one
 *          >1    if the numer is > the expected value, in this case the
 *                return value is the number of unacknowledged blocks
 *          <0    if the buffer is full (i.e. an excessive number of blocks
 *                is unacknowledged)
 */

int
add_sernum(sernum_buffer_t *buff, uint32_t num)
{
	int i, t, b;
	uint32_t n;
	int32_t diff;

	/*
	 * next_sn is the next expected SN, so normally diff should be 1.
	 */
	n = buff->next_sn;
	diff = (num - n) + 1;

	if (diff <= 0) {
		return 0;				/* ignore if SN is smaller than expected (dup or retransmit) */
	}

	buff->next_sn = num + 1;
	t = buff->top;
	b = buff->bottom;

	for (i = 0; i < diff; i++) {
		buff->sernum[t] = n++;
		buff->ack[t] = false;
		t = (t + 1) % SERNUM_BUFFER_LENGTH;
		if (t == b) {
			DEB(1, ("AddSernum: Buffer Full! num %d, diff %d\n", num, diff));
			return -1;
		}
	}

	buff->top = t;
	DEB(10, ("AddSernum bottom %d [%d], top %d, num %u, diff %d\n",
			 b, buff->sernum[b], buff->top, num, diff));

	return diff;
}


/*
 * ack_sernum:
 *    Mark a received serial number as acknowledged. This does not necessarily
 *    change the associated ExpSN if there are lower serial numbers in the
 *    buffer.
 *
 *    Parameter:
 *          buff   The serial number buffer.
 *          num   The serial number to acknowledge.
 *
 *    Returns:    The value of ExpSN.
 */

uint32_t
ack_sernum(sernum_buffer_t *buff, uint32_t num)
{
	int b = buff->bottom;
	int t = buff->top;

	/* shortcut for most likely case */
	if (t == (b + 1) && num == buff->sernum[b]) {
		/* buffer is now empty, reset top */
		buff->top = b;
	} else if (b != t) {
		for (; b != t; b = (b + 1) % SERNUM_BUFFER_LENGTH) {
			if (!sn_a_lt_b(buff->sernum[b], num))
				break;
		}
		if (num == buff->sernum[b]) {
			if (b == buff->bottom)
				buff->bottom = (b + 1) % SERNUM_BUFFER_LENGTH;
			else
				buff->ack[b] = true;
		}

		for (b = buff->bottom, num = buff->sernum[b] - 1;
			 b != t && buff->ack[b]; b = (b + 1) % SERNUM_BUFFER_LENGTH) {
			num = buff->sernum[b];
		}
	}

	if (!sn_a_lt_b(num, buff->ExpSN))
		buff->ExpSN = num + 1;

	DEB(10, ("AckSernum bottom %d, top %d, num %d ExpSN %d\n",
			 buff->bottom, buff->top, num, buff->ExpSN));

	return buff->ExpSN;
}
