/*	$NetBSD: iscsi_rcv.c,v 1.10 2015/05/30 18:09:31 joerg Exp $	*/

/*-
 * Copyright (c) 2004,2005,2006,2011 The NetBSD Foundation, Inc.
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

#include <sys/file.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

/*****************************************************************************/

/*
 * my_soo_read:
 *    Replacement for soo_read with flag handling.
 *
 *    Parameter:
 *          conn     The connection
 *          u        The uio descriptor
 *          flags    Read flags
 *
 *    Returns:    0 on success, else 1
 */

STATIC int
my_soo_read(connection_t *conn, struct uio *u, int flags)
{
	struct socket *so = conn->sock->f_socket;
	int ret;
#ifdef ISCSI_DEBUG
	size_t resid = u->uio_resid;
#endif

	DEBC(conn, 99, ("soo_read req: %zu\n", resid));

	ret = soreceive(so, NULL, u, NULL, NULL, &flags);

	if (ret || (flags != MSG_DONTWAIT && u->uio_resid)) {
		DEBC(conn, 1, ("Read failed (ret: %d, req: %zu, out: %zu)\n", ret, resid,
				u->uio_resid));
		handle_connection_error(conn, ISCSI_STATUS_SOCKET_ERROR,
								RECOVER_CONNECTION);
		return 1;
	}
	return 0;
}


/*
 * try_resynch_receive:
 *    Skip over everything in the socket's receive buffer, in the hope of
 *    ending up at the start of a new PDU.
 *
 *    Parameter:
 *          conn     The connection
 */

STATIC void
try_resynch_receive(connection_t *conn)
{
	uint8_t buffer[64];
	struct uio uio;
	struct iovec io_vec;
	int rc;

	uio.uio_rw = UIO_READ;
	UIO_SETUP_SYSSPACE(&uio);

	do {
		io_vec.iov_base = buffer;
		uio.uio_iov = &io_vec;
		uio.uio_iovcnt = 1;
		uio.uio_resid = io_vec.iov_len = sizeof(buffer);

		rc = my_soo_read(conn, &uio, MSG_DONTWAIT);
		DEBC(conn, 9, ("try_resynch_receive: rc = %d, resid = %zu\n",
				rc, uio.uio_resid));
	} while (!rc && !uio.uio_resid);
}


/*
 * ccb_from_itt
 *    Translate ITT into CCB pointer.
 *
 *    Parameter:
 *          conn     The connection
 *          itt      The Initiator Task Tag
 *
 *    Returns:
 *          Pointer to CCB, or NULL if ITT is not a valid CCB index.
 */

STATIC ccb_t *
ccb_from_itt(connection_t *conn, uint32_t itt)
{
	ccb_t *ccb;
	int cidx;

	cidx = itt & 0xff;
	if (cidx >= CCBS_PER_SESSION) {
		return NULL;
	}
	ccb = &conn->session->ccb[cidx];
	if (ccb->ITT != itt || ccb->disp <= CCBDISP_BUSY) {
		return NULL;
	}
	return ccb;
}


/*
 * read_pdu_data:
 *    Initialize the uio structure for receiving everything after the
 *    header, including data (if present), and padding. Read the data.
 *
 *    Parameter:
 *          pdu      The PDU
 *          data     Pointer to data (may be NULL for auto-allocation)
 *          offset   The offset into the data pointer
 *
 *    Returns:     0 on success
 *                 1 if an error occurs during read
 *                -1 if the data digest was incorrect (PDU must be ignored)
 */

STATIC int
read_pdu_data(pdu_t *pdu, uint8_t *data, uint32_t offset)
{
	static uint8_t pad_bytes[4];
	uint32_t len, digest;
	struct uio *uio;
	int i, pad;
	connection_t *conn = pdu->connection;

	DEBOUT(("read_pdu_data: data segment length = %d\n",
		ntoh3(pdu->pdu.DataSegmentLength)));
	if (!(len = ntoh3(pdu->pdu.DataSegmentLength))) {
		return 0;
	}
	pad = len & 0x03;
	if (pad) {
		pad = 4 - pad;
	}
	assert((data != NULL) || (offset == 0));

	if (data == NULL) {
		/*
		 * NOTE: Always allocate 2 extra bytes when reading temp data,
		 * since temp data is mostly used for received text, and we can
		 * make sure there's a double zero at the end of the data to mark EOF.
		 */
		if ((data = (uint8_t *) malloc(len + 2, M_TEMP, M_WAITOK)) == NULL) {
			DEBOUT(("ran out of mem on receive\n"));
			handle_connection_error(pdu->connection,
				ISCSI_STATUS_NO_RESOURCES, LOGOUT_SESSION);
			return 1;
		}
		pdu->temp_data = data;
		pdu->temp_data_len = len;
	}

	pdu->io_vec[0].iov_base = data + offset;
	pdu->io_vec[0].iov_len = len;

	uio = &pdu->uio;

	uio->uio_iov = pdu->io_vec;
	uio->uio_iovcnt = 1;
	uio->uio_rw = UIO_READ;
	uio->uio_resid = len;
	UIO_SETUP_SYSSPACE(uio);

	if (pad) {
		uio->uio_iovcnt++;
		uio->uio_iov[1].iov_base = pad_bytes;
		uio->uio_iov[1].iov_len = pad;
		uio->uio_resid += pad;
	}

	if (conn->DataDigest) {
		i = uio->uio_iovcnt++;
		pdu->io_vec[i].iov_base = &pdu->data_digest;
		pdu->io_vec[i].iov_len = 4;
		uio->uio_resid += 4;
	}

	/* get the data */
	if (my_soo_read(conn, &pdu->uio, MSG_WAITALL) != 0) {
		return 1;
	}
	if (conn->DataDigest) {
		digest = gen_digest_2(data, len, pad_bytes, pad);

		if (digest != pdu->data_digest) {
			DEBOUT(("Data Digest Error: comp = %08x, rx = %08x\n",
					digest, pdu->data_digest));
			switch (pdu->pdu.Opcode & OPCODE_MASK) {
			case TOP_SCSI_Response:
			case TOP_Text_Response:
				send_snack(pdu->connection, pdu, NULL, SNACK_STATUS_NAK);
				break;

			case TOP_SCSI_Data_in:
				send_snack(pdu->connection, pdu, NULL, SNACK_DATA_NAK);
				break;

			default:
				/* ignore all others */
				break;
			}
			return -1;
		}
	}
	return 0;
}


/*
 * collect_text_data
 *    Handle text continuation in login and text response PDUs
 *
 *    Parameter:
 *          pdu      The received PDU
 *          req_CCB  The CCB associated with the original request
 *
 *    Returns:    -1    if continue flag is set
 *                0     if text is complete
 *                +1    if an error occurred (out of resources)
 */
STATIC int
collect_text_data(pdu_t *pdu, ccb_t *req_ccb)
{

	if (req_ccb->text_data) {
		int nlen;
		uint8_t *newp;

		nlen = req_ccb->text_len + pdu->temp_data_len;
		/* Note: allocate extra 2 bytes for text terminator */
		if ((newp = malloc(nlen + 2, M_TEMP, M_WAITOK)) == NULL) {
			DEBOUT(("Collect Text Data: Out of Memory, ccb = %p\n", req_ccb));
			req_ccb->status = ISCSI_STATUS_NO_RESOURCES;
			/* XXX where is CCB freed? */
			return 1;
		}
		memcpy(newp, req_ccb->text_data, req_ccb->text_len);
		memcpy(&newp[req_ccb->text_len], pdu->temp_data, pdu->temp_data_len);

		free(req_ccb->text_data, M_TEMP);
		free(pdu->temp_data, M_TEMP);

		req_ccb->text_data = NULL;
		pdu->temp_data = newp;
		pdu->temp_data_len = nlen;
	}

	if (pdu->pdu.Flags & FLAG_CONTINUE) {
		req_ccb->text_data = pdu->temp_data;
		req_ccb->text_len = pdu->temp_data_len;
		pdu->temp_data = NULL;

		acknowledge_text(req_ccb->connection, pdu, req_ccb);
		return -1;
	}
	return 0;
}


/*
 * check_StatSN
 *    Check received vs. expected StatSN
 *
 *    Parameter:
 *          conn     The connection
 *          nw_sn    The received StatSN in network byte order
 *          ack      Acknowledge this SN if TRUE
 */

STATIC int
check_StatSN(connection_t *conn, uint32_t nw_sn, bool ack)
{
	int rc;
	uint32_t sn = ntohl(nw_sn);

	rc = add_sernum(&conn->StatSN_buf, sn);

	if (ack)
		ack_sernum(&conn->StatSN_buf, sn);

	if (rc != 1) {
		if (rc == 0) {
			DEBOUT(("Duplicate PDU, ExpSN %d, Recvd: %d\n",
				conn->StatSN_buf.ExpSN, sn));
			return -1;
		}

		if (rc < 0) {
			DEBOUT(("Excessive outstanding Status PDUs, ExpSN %d, Recvd: %d\n",
					conn->StatSN_buf.ExpSN, sn));
			handle_connection_error(conn, ISCSI_STATUS_PDUS_LOST,
									RECOVER_CONNECTION);
			return rc;
		}

		DEBOUT(("Missing Status PDUs: First %d, num: %d\n",
				conn->StatSN_buf.ExpSN, rc - 1));
		if (conn->state == ST_FULL_FEATURE &&
			conn->session->ErrorRecoveryLevel) {
			snack_missing(conn, NULL, SNACK_STATUS_NAK,
						  conn->StatSN_buf.ExpSN, rc - 1);
		} else {
			DEBOUT(("StatSN killing connection (State = %d, "
					"ErrorRecoveryLevel = %d)\n",
					conn->state, conn->session->ErrorRecoveryLevel));
			handle_connection_error(conn, ISCSI_STATUS_PDUS_LOST,
									RECOVER_CONNECTION);
			return -1;
		}
	}
	return 0;
}


/*
 * check_CmdSN
 *    Check received vs. expected CmdSN
 *
 *    Parameter:
 *          conn     The connection
 *          nw_sn    The received ExpCmdSN in network byte order
 */

STATIC void
check_CmdSN(connection_t *conn, uint32_t nw_sn)
{
	uint32_t sn = ntohl(nw_sn);
	ccb_t *ccb, *nxt;

	TAILQ_FOREACH_SAFE(ccb, &conn->ccbs_waiting, chain, nxt) {
		DEBC(conn, 10,
			("CheckCmdSN - CmdSN=%d, ExpCmdSn=%d, waiting=%p, flags=%x\n",
			ccb->CmdSN, sn, ccb->pdu_waiting, ccb->flags));
		if (ccb->pdu_waiting != NULL && ccb->CmdSN > sn &&
			!(ccb->flags & CCBF_GOT_RSP)) {
			DEBC(conn, 1, ("CheckCmdSN resending - CmdSN=%d, ExpCmdSn=%d\n",
						   ccb->CmdSN, sn));

			ccb->total_tries++;

			if (++ccb->num_timeouts > MAX_CCB_TIMEOUTS ||
				ccb->total_tries > MAX_CCB_TRIES) {
				handle_connection_error(conn, ISCSI_STATUS_TIMEOUT,
					(ccb->total_tries <= MAX_CCB_TRIES) ? RECOVER_CONNECTION
														: LOGOUT_CONNECTION);
				break;
			} else {
				resend_pdu(ccb);
			}
		}
	}
}


/*
 * receive_login_pdu
 *    Handle receipt of a login response PDU.
 *
 *    Parameter:
 *          conn     The connection
 *          pdu      The PDU
 *          req_CCB  The CCB associated with the original request (if any)
 */

STATIC int
receive_login_pdu(connection_t *conn, pdu_t *pdu, ccb_t *req_ccb)
{
	int rc;

	DEBC(conn, 9, ("Received Login Response PDU, op=%x, flags=%x, sn=%u\n",
			pdu->pdu.Opcode, pdu->pdu.Flags,
			ntohl(pdu->pdu.p.login_rsp.StatSN)));

	if (req_ccb == NULL) {
		/* Duplicate?? */
		DEBOUT(("Received duplicate login response (no associated CCB)\n"));
		return -1;
	}

	if (pdu->pdu.p.login_rsp.StatusClass) {
		DEBC(conn, 1, ("Login problem - Class = %x, Detail = %x\n",
				pdu->pdu.p.login_rsp.StatusClass,
				pdu->pdu.p.login_rsp.StatusDetail));
		wake_ccb(req_ccb, ISCSI_STATUS_LOGIN_FAILED);
		return 0;
	}

	if (!conn->StatSN_buf.next_sn) {
		conn->StatSN_buf.next_sn = conn->StatSN_buf.ExpSN =
			ntohl(pdu->pdu.p.login_rsp.StatSN) + 1;
	} else if (check_StatSN(conn, pdu->pdu.p.login_rsp.StatSN, TRUE))
		return -1;

	if (pdu->temp_data_len) {
		if ((rc = collect_text_data(pdu, req_ccb)) != 0)
			return max(rc, 0);
	}

	negotiate_login(conn, pdu, req_ccb);

	/* negotiate_login will decide whether login is complete or not */
	return 0;
}


/*
 * receive_text_response_pdu
 *    Handle receipt of a text response PDU.
 *
 *    Parameter:
 *          conn     The connection
 *          pdu      The PDU
 *          req_CCB  The CCB associated with the original request (if any)
 */

STATIC int
receive_text_response_pdu(connection_t *conn, pdu_t *pdu, ccb_t *req_ccb)
{
	int rc;

	DEBC(conn, 9, ("Received Text Response PDU, op=%x, flags=%x\n",
			pdu->pdu.Opcode, pdu->pdu.Flags));

	if (check_StatSN(conn, pdu->pdu.p.text_rsp.StatSN, TRUE)) {
		return -1;
	}
	if (req_ccb == NULL) {
		DEBOUT(("Received unsolicited text response\n"));
		handle_connection_error(conn, ISCSI_STATUS_TARGET_ERROR,
							LOGOUT_CONNECTION);
		return -1;
	}

	if (req_ccb->pdu_waiting != NULL) {
		callout_schedule(&req_ccb->timeout, COMMAND_TIMEOUT);
		req_ccb->num_timeouts = 0;
	}

	if ((rc = collect_text_data(pdu, req_ccb)) != 0) {
		return max(0, rc);
	}
	negotiate_text(conn, pdu, req_ccb);

	return 0;
}


/*
 * receive_logout_pdu
 *    Handle receipt of a logout response PDU.
 *
 *    Parameter:
 *          conn     The connection
 *          pdu      The PDU
 *          req_CCB  The CCB associated with the original request (if any)
 */

STATIC int
receive_logout_pdu(connection_t *conn, pdu_t *pdu, ccb_t *req_ccb)
{
	bool otherconn;
	uint8_t response;
	uint32_t status;

	otherconn = (req_ccb != NULL) ? (req_ccb->flags & CCBF_OTHERCONN) != 0 : 1;
	response = pdu->pdu.OpcodeSpecific [0];
	DEBC(conn, 1,
		("Received Logout PDU - CCB = %p, otherconn=%d, response=%d\n",
		req_ccb, otherconn, response));

	if (req_ccb == NULL)
		return 0;

	if (otherconn && check_StatSN(conn, pdu->pdu.p.logout_rsp.StatSN, TRUE))
		return -1;

	switch (response) {
	case 0:
		status = ISCSI_STATUS_SUCCESS;
		break;
	case 1:
		status = ISCSI_STATUS_LOGOUT_CID_NOT_FOUND;
		break;
	case 2:
		status = ISCSI_STATUS_LOGOUT_RECOVERY_NS;
		break;
	default:
		status = ISCSI_STATUS_LOGOUT_ERROR;
		break;
	}

	if (conn->session->ErrorRecoveryLevel >= 2 && response != 1) {
		connection_t *refconn = (otherconn) ? req_ccb->par : conn;

		refconn->Time2Wait = ntohs(pdu->pdu.p.logout_rsp.Time2Wait);
		refconn->Time2Retain = ntohs(pdu->pdu.p.logout_rsp.Time2Retain);
	}

	wake_ccb(req_ccb, status);

	if (!otherconn && conn->state == ST_LOGOUT_SENT) {
		conn->terminating = ISCSI_STATUS_LOGOUT;
		conn->state = ST_SETTLING;
		conn->loggedout = (response) ? LOGOUT_FAILED : LOGOUT_SUCCESS;

		callout_stop(&conn->timeout);

		/* let send thread take over next step of cleanup */
		wakeup(&conn->pdus_to_send);
	}

	return !otherconn;
}


/*
 * receive_data_in_pdu
 *    Handle receipt of a data in PDU.
 *
 *    Parameter:
 *          conn     The connection
 *          pdu      The PDU
 *          req_CCB  The CCB associated with the original request (if any)
 */

STATIC int
receive_data_in_pdu(connection_t *conn, pdu_t *pdu, ccb_t *req_ccb)
{
	uint32_t dsl, sn;
	bool done;
	int rc;

	dsl = ntoh3(pdu->pdu.DataSegmentLength);

	if (req_ccb == NULL || !req_ccb->data_in || !req_ccb->data_len) {
		DEBOUT(("Received Data In, but req_ccb not waiting for it, ignored\n"));
		return 0;
	}
	req_ccb->flags |= CCBF_GOT_RSP;

	if (req_ccb->pdu_waiting != NULL) {
		callout_schedule(&req_ccb->timeout, COMMAND_TIMEOUT);
		req_ccb->num_timeouts = 0;
	}

	sn = ntohl(pdu->pdu.p.data_in.DataSN);

	if ((rc = add_sernum(&req_ccb->DataSN_buf, sn)) != 1) {
		if (!rc) {
			return -1;
		}
		if (rc < 0) {
			DEBOUT(("Excessive outstanding Data PDUs\n"));
			handle_connection_error(req_ccb->connection,
				ISCSI_STATUS_PDUS_LOST, LOGOUT_CONNECTION);
			return -1;
		}
		DEBOUT(("Missing Data PDUs: First %d, num: %d\n",
				req_ccb->DataSN_buf.ExpSN, rc - 1));

		if (conn->state == ST_FULL_FEATURE &&
			conn->session->ErrorRecoveryLevel) {
			snack_missing(req_ccb->connection, req_ccb,
				SNACK_DATA_NAK, req_ccb->DataSN_buf.ExpSN,
				rc - 1);
		} else {
			DEBOUT(("Killing connection (State=%d, ErrorRecoveryLevel=%d)\n",
					conn->state, conn->session->ErrorRecoveryLevel));
			handle_connection_error(conn, ISCSI_STATUS_PDUS_LOST,
						LOGOUT_CONNECTION);
			return -1;
		}
	}

	ack_sernum(&req_ccb->DataSN_buf, sn);

	req_ccb->xfer_len += dsl;

	if ((pdu->pdu.Flags & FLAG_ACK) && conn->session->ErrorRecoveryLevel)
		send_snack(conn, pdu, req_ccb, SNACK_DATA_ACK);

	done = sn_empty(&req_ccb->DataSN_buf);

	if (pdu->pdu.Flags & FLAG_STATUS) {
		DEBC(conn, 10, ("Rx Data In Complete, done = %d\n", done));

		req_ccb->flags |= CCBF_COMPLETE;
		/* successful transfer, reset recover count */
		conn->recover = 0;

		if (done)
			wake_ccb(req_ccb, ISCSI_STATUS_SUCCESS);
		if (check_StatSN(conn, pdu->pdu.p.data_in.StatSN, done))
			return -1;

	} else if (done && (req_ccb->flags & CCBF_COMPLETE)) {
		wake_ccb(req_ccb, ISCSI_STATUS_SUCCESS);
	}
	/* else wait for command response */

	return 0;
}


/*
 * receive_r2t_pdu
 *    Handle receipt of a R2T PDU.
 *
 *    Parameter:
 *          conn     The connection
 *          pdu      The PDU
 *          req_CCB  The CCB associated with the original request (if any)
 */

STATIC int
receive_r2t_pdu(connection_t *conn, pdu_t *pdu, ccb_t *req_ccb)
{

	DEBC(conn, 10, ("Received R2T PDU - CCB = %p\n", req_ccb));

	if (req_ccb != NULL) {
		if (req_ccb->pdu_waiting != NULL) {
			callout_schedule(&req_ccb->timeout, COMMAND_TIMEOUT);
			req_ccb->num_timeouts = 0;
		}
		send_data_out(conn, pdu, req_ccb, CCBDISP_NOWAIT, TRUE);
	}

	return 0;
}


/*
 * receive_command_response_pdu
 *    Handle receipt of a command response PDU.
 *
 *    Parameter:
 *          conn     The connection
 *          pdu      The PDU
 *          req_CCB  The CCB associated with the original request (if any)
 */

STATIC int
receive_command_response_pdu(connection_t *conn, pdu_t *pdu, ccb_t *req_ccb)
{
	int len, rc;
	bool done;
	uint32_t status;

	/* Read any provided data */
	if (pdu->temp_data_len && req_ccb != NULL && req_ccb->sense_len_req) {
		len = min(req_ccb->sense_len_req,
				  ntohs(*((uint16_t *) pdu->temp_data)));
		memcpy(req_ccb->sense_ptr, ((uint16_t *) pdu->temp_data) + 1,
					len);
		req_ccb->sense_len_got = len;
	}

	if (req_ccb == NULL) {
		/* Assume duplicate... */
		DEBOUT(("Possibly duplicate command response (no associated CCB)\n"));
		return -1;
	}

	if (req_ccb->pdu_waiting != NULL) {
		callout_schedule(&req_ccb->timeout, COMMAND_TIMEOUT);
		req_ccb->num_timeouts = 0;
	}

	req_ccb->flags |= CCBF_COMPLETE;
	conn->recover = 0;	/* successful transfer, reset recover count */

	if (pdu->pdu.OpcodeSpecific[0]) {	/* Response */
		status = ISCSI_STATUS_TARGET_FAILURE;
	} else {
		switch (pdu->pdu.OpcodeSpecific[1]) {	/* Status */
		case 0x00:
			status = ISCSI_STATUS_SUCCESS;
			break;

		case 0x02:
			status = ISCSI_STATUS_CHECK_CONDITION;
			break;

		case 0x08:
			status = ISCSI_STATUS_TARGET_BUSY;
			break;

		default:
			status = ISCSI_STATUS_TARGET_ERROR;
			break;
		}
	}

	if (pdu->pdu.Flags & (FLAG_OVERFLOW | FLAG_UNDERFLOW))
		req_ccb->residual = ntohl(pdu->pdu.p.response.ResidualCount);

	done = status || sn_empty(&req_ccb->DataSN_buf);

	DEBC(conn, 10, ("Rx Command Response rsp = %x, status = %x\n",
			pdu->pdu.OpcodeSpecific[0], pdu->pdu.OpcodeSpecific[1]));

	rc = check_StatSN(conn, pdu->pdu.p.response.StatSN, done);

	if (done)
		wake_ccb(req_ccb, status);

	return rc;
}


/*
 * receive_asynch_pdu
 *    Handle receipt of an asynchronous message PDU.
 *
 *    Parameter:
 *          conn     The connection
 *          pdu      The PDU
 */

STATIC int
receive_asynch_pdu(connection_t *conn, pdu_t *pdu)
{

	DEBOUT(("Received Asynch PDU, Event %d\n", pdu->pdu.p.asynch.AsyncEvent));

	switch (pdu->pdu.p.asynch.AsyncEvent) {
	case 0:		   		/* SCSI Asynch event. Don't know what to do with it... */
		break;

	case 1:		   		/* Target requests logout. */
		if (conn->session->active_connections > 1) {
			kill_connection(conn, ISCSI_STATUS_TARGET_LOGOUT,
						LOGOUT_CONNECTION, FALSE);
		} else {
			kill_session(conn->session, ISCSI_STATUS_TARGET_LOGOUT,
						 LOGOUT_SESSION, FALSE);
		}
		break;

	case 2:				/* Target is dropping connection */
		conn = find_connection(conn->session,
					ntohs(pdu->pdu.p.asynch.Parameter1));
		if (conn != NULL) {
			conn->Time2Wait = ntohs(pdu->pdu.p.asynch.Parameter2);
			conn->Time2Retain = ntohs(pdu->pdu.p.asynch.Parameter3);
			kill_connection(conn, ISCSI_STATUS_TARGET_DROP,
					NO_LOGOUT, TRUE);
		}
		break;

	case 3:				/* Target is dropping all connections of session */
		conn->session->DefaultTime2Wait = ntohs(pdu->pdu.p.asynch.Parameter2);
		conn->session->DefaultTime2Retain = ntohs(pdu->pdu.p.asynch.Parameter3);
		kill_session(conn->session, ISCSI_STATUS_TARGET_DROP, NO_LOGOUT, TRUE);
		break;

	case 4:				/* Target requests parameter negotiation */
		start_text_negotiation(conn);
		break;

	default:
		/* ignore */
		break;
	}
	return 0;
}


/*
 * receive_reject_pdu
 *    Handle receipt of a reject PDU.
 *
 *    Parameter:
 *          conn     The connection
 *          pdu      The PDU
 */

STATIC int
receive_reject_pdu(connection_t *conn, pdu_t *pdu)
{
	pdu_header_t *hpdu;
	ccb_t *req_ccb;
	uint32_t status;

	DEBOUT(("Received Reject PDU, reason = %x, data_len = %d\n",
			pdu->pdu.OpcodeSpecific[0], pdu->temp_data_len));

	if (pdu->temp_data_len >= BHS_SIZE) {
		hpdu = (pdu_header_t *) pdu->temp_data;
		req_ccb = ccb_from_itt(conn, hpdu->InitiatorTaskTag);

		DEBC(conn, 9, ("Reject PDU ITT (ccb)= %x (%p)\n",
				hpdu->InitiatorTaskTag, req_ccb));
		if (!req_ccb) {
			return 0;
		}
		switch (pdu->pdu.OpcodeSpecific[0]) {
		case REJECT_DIGEST_ERROR:
			/* don't retransmit data out */
			if ((hpdu->Opcode & OPCODE_MASK) == IOP_SCSI_Data_out)
				return 0;
			resend_pdu(req_ccb);
			return 0;

		case REJECT_IMMED_COMMAND:
		case REJECT_LONG_OPERATION:
			resend_pdu(req_ccb);
			return 0;

		case REJECT_SNACK:
		case REJECT_PROTOCOL_ERROR:
			status = ISCSI_STATUS_PROTOCOL_ERROR;
			break;

		case REJECT_CMD_NOT_SUPPORTED:
			status = ISCSI_STATUS_CMD_NOT_SUPPORTED;
			break;

		case REJECT_INVALID_PDU_FIELD:
			status = ISCSI_STATUS_PDU_ERROR;
			break;

		default:
			status = ISCSI_STATUS_GENERAL_ERROR;
			break;
		}

		wake_ccb(req_ccb, status);
		handle_connection_error(conn, ISCSI_STATUS_PROTOCOL_ERROR,
							LOGOUT_CONNECTION);
	}
	return 0;
}


/*
 * receive_task_management_pdu
 *    Handle receipt of a task management PDU.
 *
 *    Parameter:
 *          conn     The connection
 *          pdu      The PDU
 *          req_CCB  The CCB associated with the original request (if any)
 */

STATIC int
receive_task_management_pdu(connection_t *conn, pdu_t *pdu, ccb_t *req_ccb)
{
	uint32_t status;

	DEBC(conn, 2, ("Received Task Management PDU, response %d, req_ccb %p\n",
			pdu->pdu.OpcodeSpecific[0], req_ccb));

	if (req_ccb != NULL) {
		switch (pdu->pdu.OpcodeSpecific[0]) {	/* Response */
		case 0:
			status = ISCSI_STATUS_SUCCESS;
			break;
		case 1:
			status = ISCSI_STATUS_TASK_NOT_FOUND;
			break;
		case 2:
			status = ISCSI_STATUS_LUN_NOT_FOUND;
			break;
		case 3:
			status = ISCSI_STATUS_TASK_ALLEGIANT;
			break;
		case 4:
			status = ISCSI_STATUS_CANT_REASSIGN;
			break;
		case 5:
			status = ISCSI_STATUS_FUNCTION_UNSUPPORTED;
			break;
		case 6:
			status = ISCSI_STATUS_FUNCTION_NOT_AUTHORIZED;
			break;
		case 255:
			status = ISCSI_STATUS_FUNCTION_REJECTED;
			break;
		default:
			status = ISCSI_STATUS_UNKNOWN_REASON;
			break;
		}
		wake_ccb(req_ccb, status);
	}

	check_StatSN(conn, pdu->pdu.p.task_rsp.StatSN, TRUE);

	return 0;
}


/*
 * receive_nop_in_pdu
 *    Handle receipt of a Nop-In PDU.
 *
 *    Parameter:
 *          conn     The connection
 *          pdu      The PDU
 *          req_CCB  The CCB associated with the original request (if any)
 */

STATIC int
receive_nop_in_pdu(connection_t *conn, pdu_t *pdu, ccb_t *req_ccb)
{
	DEBC(conn, 10,
		("Received NOP-In PDU, req_ccb=%p, ITT=%x, TTT=%x, StatSN=%x\n",
		req_ccb, pdu->pdu.InitiatorTaskTag,
		pdu->pdu.p.nop_in.TargetTransferTag,
		ntohl(pdu->pdu.p.nop_in.StatSN)));

	if (pdu->pdu.InitiatorTaskTag == 0xffffffff) {
		/* this is a target ping - respond with a pong */
		if (pdu->pdu.p.nop_in.TargetTransferTag != 0xffffffff)
			send_nop_out(conn, pdu);

		/*
		   Any receive resets the connection timeout, but we got a ping, which
		   means that it's likely the other side was waiting for something to
		   happen on the connection. If we aren't idle, send a ping right
		   away to synch counters (don't synch on this ping because other
		   PDUs may be on the way).
		 */
		if (TAILQ_FIRST(&conn->ccbs_waiting) != NULL)
			send_nop_out(conn, NULL);
	} else if (req_ccb != NULL) {
		/* this is a solicited ping, check CmdSN for lost commands */
		/* and advance StatSN */
		check_CmdSN(conn, pdu->pdu.p.nop_in.ExpCmdSN);

		wake_ccb(req_ccb, ISCSI_STATUS_SUCCESS);

		check_StatSN(conn, pdu->pdu.p.nop_in.StatSN, TRUE);
	}

	return 0;
}


/*
 * receive_pdu
 *    Get parameters, call the appropriate handler for a received PDU.
 *
 *    Parameter:
 *          conn     The connection
 *          pdu      The PDU
 *
 *    Returns:    0 on success, nonzero if the connection is broken.
 */

STATIC int
receive_pdu(connection_t *conn, pdu_t *pdu)
{
	ccb_t *req_ccb;
	ccb_list_t waiting;
	int rc, s;
	uint32_t MaxCmdSN, digest;
	session_t *sess = conn->session;

	if (conn->HeaderDigest) {
		digest = gen_digest(&pdu->pdu, BHS_SIZE);
		if (digest != pdu->pdu.HeaderDigest) {
			DEBOUT(("Header Digest Error: comp = %08x, rx = %08x\n",
					digest, pdu->pdu.HeaderDigest));
			/* try to skip to next PDU */
			try_resynch_receive(conn);
			free_pdu(pdu);
			return 0;
		}
	}

	req_ccb = ccb_from_itt(conn, pdu->pdu.InitiatorTaskTag);

	if (req_ccb != NULL && req_ccb->data_in && req_ccb->data_len &&
		(pdu->pdu.Opcode & OPCODE_MASK) == TOP_SCSI_Data_in) {
		uint32_t dsl, offset;

		dsl = ntoh3(pdu->pdu.DataSegmentLength);
		offset = ntohl(pdu->pdu.p.data_in.BufferOffset);

		if ((offset + dsl) > req_ccb->data_len) {
			DEBOUT(("Received more data than requested (len %d, offset %d)\n",
					dsl, offset));
			handle_connection_error(conn, ISCSI_STATUS_TARGET_ERROR, NO_LOGOUT);
			return 1;
		}
		DEBC(conn, 10,
			("Received Data in PDU - CCB = %p, Datalen = %d, Offset = %d\n",
			req_ccb, dsl, offset));

		rc = read_pdu_data(pdu, req_ccb->data_ptr, offset);
	} else {
		rc = read_pdu_data(pdu, NULL, 0);
	}
	if (!rc && (conn->state <= ST_WINDING_DOWN ||
		(pdu->pdu.Opcode & OPCODE_MASK) == TOP_Logout_Response)) {

		switch (pdu->pdu.Opcode & OPCODE_MASK) {
		case TOP_NOP_In:
			rc = receive_nop_in_pdu(conn, pdu, req_ccb);
			break;

		case TOP_SCSI_Response:
			rc = receive_command_response_pdu(conn, pdu, req_ccb);
			break;

		case TOP_SCSI_Task_Management:
			rc = receive_task_management_pdu(conn, pdu, req_ccb);
			break;

		case TOP_Login_Response:
			rc = receive_login_pdu(conn, pdu, req_ccb);
			break;

		case TOP_Text_Response:
			rc = receive_text_response_pdu(conn, pdu, req_ccb);
			break;

		case TOP_SCSI_Data_in:
			rc = receive_data_in_pdu(conn, pdu, req_ccb);
			break;

		case TOP_Logout_Response:
			rc = receive_logout_pdu(conn, pdu, req_ccb);
			break;

		case TOP_R2T:
			rc = receive_r2t_pdu(conn, pdu, req_ccb);
			break;

		case TOP_Asynchronous_Message:
			rc = receive_asynch_pdu(conn, pdu);
			break;

		case TOP_Reject:
			rc = receive_reject_pdu(conn, pdu);
			break;

		default:
			DEBOUT(("Received Invalid Opcode %x\n", pdu->pdu.Opcode));
			try_resynch_receive(conn);
			rc = -1;
			break;
		}
	}

	free_pdu(pdu);
	if (rc)
		return rc;

	/* MaxCmdSN and ExpCmdSN are in the same place in all received PDUs */
	sess->ExpCmdSN = max(sess->ExpCmdSN, ntohl(pdu->pdu.p.nop_in.ExpCmdSN));
	MaxCmdSN = ntohl(pdu->pdu.p.nop_in.MaxCmdSN);

	/* received a valid frame, reset timeout */
	if ((pdu->pdu.Opcode & OPCODE_MASK) == TOP_NOP_In &&
	    TAILQ_EMPTY(&conn->ccbs_waiting))
		callout_schedule(&conn->timeout, conn->idle_timeout_val);
	else
		callout_schedule(&conn->timeout, CONNECTION_TIMEOUT);
	conn->num_timeouts = 0;

	/*
	 * Un-throttle - wakeup all CCBs waiting for MaxCmdSN to increase.
	 * We have to handle wait/nowait CCBs a bit differently.
	 */
	if (MaxCmdSN != sess->MaxCmdSN) {
		sess->MaxCmdSN = MaxCmdSN;
		if (TAILQ_FIRST(&sess->ccbs_throttled) == NULL)
			return 0;

		DEBC(conn, 1, ("Unthrottling - MaxCmdSN = %d\n", MaxCmdSN));

		s = splbio();
		TAILQ_INIT(&waiting);
		while ((req_ccb = TAILQ_FIRST(&sess->ccbs_throttled)) != NULL) {
			throttle_ccb(req_ccb, FALSE);
			TAILQ_INSERT_TAIL(&waiting, req_ccb, chain);
		}
		splx(s);

		while ((req_ccb = TAILQ_FIRST(&waiting)) != NULL) {
			TAILQ_REMOVE(&waiting, req_ccb, chain);

			DEBC(conn, 1, ("Unthrottling - ccb = %p, disp = %d\n",
					req_ccb, req_ccb->disp));

			if (req_ccb->flags & CCBF_WAITING)
				wakeup(req_ccb);
			else
				send_command(req_ccb, req_ccb->disp, FALSE, FALSE);
		}
	}

	return 0;
}

/*****************************************************************************/

/*
 * iscsi_receive_thread
 *    Per connection thread handling receive data.
 *
 *    Parameter:
 *          conn     The connection
 */

void
iscsi_rcv_thread(void *par)
{
	connection_t *conn = (connection_t *) par;
	pdu_t *pdu;
	size_t hlen;

	do {
		while (!conn->terminating) {
			pdu = get_pdu(conn, TRUE);
			pdu->uio.uio_iov = pdu->io_vec;
			UIO_SETUP_SYSSPACE(&pdu->uio);
			pdu->uio.uio_iovcnt = 1;
			pdu->uio.uio_rw = UIO_READ;

			pdu->io_vec[0].iov_base = &pdu->pdu;
			hlen = (conn->HeaderDigest) ? BHS_SIZE + 4 : BHS_SIZE;
			pdu->io_vec[0].iov_len = hlen;
			pdu->uio.uio_resid = hlen;

			DEBC(conn, 99, ("Receive thread waiting for data\n"));
			if (my_soo_read(conn, &pdu->uio, MSG_WAITALL)) {
				free_pdu(pdu);
				break;
			}
			/* Check again for header digest */
			/* (it may have changed during the wait) */
			if (hlen == BHS_SIZE && conn->HeaderDigest) {
				pdu->uio.uio_iov = pdu->io_vec;
				pdu->uio.uio_iovcnt = 1;
				pdu->io_vec[0].iov_base = &pdu->pdu.HeaderDigest;
				pdu->io_vec[0].iov_len = 4;
				pdu->uio.uio_resid = 4;
				if (my_soo_read(conn, &pdu->uio, MSG_WAITALL)) {
					free_pdu(pdu);
					break;
				}
			}

			if (receive_pdu(conn, pdu) > 0) {
				break;
			}
		}
		if (!conn->destroy) {
			tsleep(conn, PRIBIO, "conn_idle", 30 * hz);
		}
	} while (!conn->destroy);

	conn->rcvproc = NULL;
	DEBC(conn, 5, ("Receive thread exits\n"));
	kthread_exit(0);
}
