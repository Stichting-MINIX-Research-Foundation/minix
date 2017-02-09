/*	$NetBSD: iscsi_send.c,v 1.14 2015/05/30 18:09:31 joerg Exp $	*/

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
#include <sys/filedesc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

/*#define LUN_1  1 */

/*****************************************************************************/

/*
 * my_soo_write:
 *    Replacement for soo_write with flag handling.
 *
 *    Parameter:
 *          conn     The connection
 *          u        The uio descriptor
 *
 *    Returns:    0 on success, else EIO.
 */

STATIC int
my_soo_write(connection_t *conn, struct uio *u)
{
	struct socket *so = conn->sock->f_socket;
	size_t resid = u->uio_resid;
	int ret;

	assert(resid != 0);

	ret = sosend(so, NULL, u, NULL, NULL, 0, conn->threadobj);

	DEB(99, ("soo_write done: len = %zu\n", u->uio_resid));

	if (ret != 0 || u->uio_resid) {
		DEBC(conn, 0, ("Write failed sock %p (ret: %d, req: %zu, resid: %zu)\n",
			conn->sock, ret, resid, u->uio_resid));
		handle_connection_error(conn, ISCSI_STATUS_SOCKET_ERROR, NO_LOGOUT);
		return EIO;
	}
	return 0;
}

/*****************************************************************************/

/*
 * assign_connection:
 *    This function returns the connection to use for the next transaction.
 *
 *    Parameter:  The session
 *
 *    Returns:    The connection
 */

connection_t *
assign_connection(session_t *session, bool waitok)
{
	connection_t *conn, *next;

	do {
		if ((conn = session->mru_connection) == NULL) {
			return NULL;
		}
		next = conn;
		do {
			next = TAILQ_NEXT(next, connections);
			if (next == NULL) {
				next = TAILQ_FIRST(&session->conn_list);
			}
		} while (next != NULL && next != conn &&
				 next->state != ST_FULL_FEATURE);

		if (next->state != ST_FULL_FEATURE) {
			if (waitok) {
				tsleep(session, PRIBIO, "iscsi_assign_connection", 0);
				next = TAILQ_FIRST(&session->conn_list);
			} else {
				return NULL;
			}
		} else {
			session->mru_connection = next;
		}
	} while (next != NULL && next->state != ST_FULL_FEATURE);

	return next;
}


/*
 * reassign_tasks:
 *    Reassign pending commands to one of the still existing connections
 *    of a session.
 *
 *    Parameter:
 *          oldconn		The terminating connection
 */

STATIC void
reassign_tasks(connection_t *oldconn)
{
	session_t *sess = oldconn->session;
	connection_t *conn;
	ccb_t *ccb;
	pdu_t *pdu = NULL;
	pdu_t *opdu;
	int no_tm = 1;
	int rc = 1;
	int s;

	if ((conn = assign_connection(sess, FALSE)) == NULL) {
		DEB(1, ("Reassign_tasks of Session %d, connection %d failed, "
			    "no active connection\n",
			    sess->id, oldconn->id));
		/* XXX here we need to abort the waiting CCBs */
		return;
	}

	if (sess->ErrorRecoveryLevel >= 2) {
		if (oldconn->loggedout == NOT_LOGGED_OUT) {
			oldconn->loggedout = LOGOUT_SENT;
			no_tm = send_logout(conn, oldconn, RECOVER_CONNECTION, TRUE);
			oldconn->loggedout = (rc) ? LOGOUT_FAILED : LOGOUT_SUCCESS;
			if (!oldconn->Time2Retain) {
				DEBC(conn, 1, ("Time2Retain is zero, setting no_tm\n"));
				no_tm = 1;
			}
		} else if (oldconn->loggedout == LOGOUT_SUCCESS) {
			no_tm = 0;
		}
		if (!no_tm && oldconn->Time2Wait) {
			DEBC(conn, 1, ("Time2Wait=%d, hz=%d, waiting...\n",
						   oldconn->Time2Wait, hz));
			tsleep(&no_tm, PRIBIO, "Time2Wait", oldconn->Time2Wait * hz);
		}
	}

	DEBC(conn, 1, ("Reassign_tasks: Session %d, conn %d -> conn %d, no_tm=%d\n",
		sess->id, oldconn->id, conn->id, no_tm));


	/* XXX reassign waiting CCBs to new connection */

	while ((ccb = TAILQ_FIRST(&oldconn->ccbs_waiting)) != NULL) {
		/* Copy PDU contents (PDUs are bound to connection) */
		if ((pdu = get_pdu(conn, TRUE)) == NULL) {
			break;
		}

		/* adjust CCB and clone PDU for new connection */
		TAILQ_REMOVE(&oldconn->ccbs_waiting, ccb, chain);

		opdu = ccb->pdu_waiting;
		*pdu = *opdu;

		/* restore overwritten back ptr */
		pdu->connection = conn;

		/* fixup saved UIO and IOVEC (regular one will be overwritten anyway) */
		pdu->save_uio.uio_iov = pdu->io_vec;
		pdu->save_iovec [0].iov_base = &pdu->pdu;

		if (conn->DataDigest && pdu->save_uio.uio_iovcnt > 1) {
			if (pdu->save_iovec [2].iov_base == NULL) {
				pdu->save_iovec [2].iov_base = &pdu->data_digest;
				pdu->save_uio.uio_iovcnt = 3;
			} else {
				pdu->save_iovec [3].iov_base = &pdu->data_digest;
				pdu->save_uio.uio_iovcnt = 4;
			}
		}
		pdu->save_iovec [0].iov_len =
			(conn->HeaderDigest) ? BHS_SIZE + 4 : BHS_SIZE;


		/* link new PDU into old CCB */
		ccb->pdu_waiting = pdu;
		/* link new CCB into new connection */
		ccb->connection = conn;
		/* reset timeouts */
		ccb->num_timeouts = 0;

		/* fixup reference counts */
		oldconn->usecount--;
		conn->usecount++;

		DEBC(conn, 1, ("CCB %p: Copied PDU %p to %p\n",
					   ccb, opdu, pdu));

		/* kill temp pointer that is now referenced by the new PDU */
		opdu->temp_data = NULL;

		/* and free the old PDU */
		free_pdu(opdu);

		/* put ready CCB into waiting list of new connection */
		s = splbio();
		suspend_ccb(ccb, TRUE);
		splx(s);
	}

	if (pdu == NULL) {
		DEBC(conn, 1, ("Error while copying PDUs in reassign_tasks!\n"));
		/* give up recovering, the other connection is screwed up as well... */
		while ((ccb = TAILQ_FIRST(&oldconn->ccbs_waiting)) != NULL) {
			wake_ccb(ccb, oldconn->terminating);
		}
		/* XXX some CCBs might have been moved to new connection, but how is the
		 * new connection handled or killed ? */
		return;
	}

	TAILQ_FOREACH(ccb, &conn->ccbs_waiting, chain) {
		if (!no_tm) {
			rc = send_task_management(conn, ccb, NULL, TASK_REASSIGN);
		}
		/* if we get an error on reassign, restart the original request */
		if (no_tm || rc) {
			if (ccb->CmdSN < sess->ExpCmdSN) {
				pdu = ccb->pdu_waiting;

				/* update CmdSN */
				DEBC(conn, 1, ("Resend Updating CmdSN - old %d, new %d\n",
					   ccb->CmdSN, sess->CmdSN));
				ccb->CmdSN = sess->CmdSN;
				if (!(pdu->pdu.Opcode & OP_IMMEDIATE)) {
					sess->CmdSN++;
				}
				pdu->pdu.p.command.CmdSN = htonl(ccb->CmdSN);
			}
			resend_pdu(ccb);
		} else {
			callout_schedule(&ccb->timeout, COMMAND_TIMEOUT);
		}
		DEBC(conn, 1, ("Reassign ccb %p, no_tm=%d, rc=%d\n",
					   ccb, no_tm, rc));
	}
}


/*
 * iscsi_send_thread:
 *    This thread services the send queue, writing the PDUs to the socket.
 *    It also handles the cleanup when the connection is terminated.
 *
 *    Parameter:
 *          par		The connection this thread services
 */

void
iscsi_send_thread(void *par)
{
	connection_t *conn = (connection_t *) par;
	session_t *sess;
	ccb_t *ccb, *nccb;
	pdu_t *pdu;
	struct file *fp;
	int s;

	sess = conn->session;
	/* so cleanup thread knows there's someone left */
	iscsi_num_send_threads++;

	do {
		while (!conn->terminating) {
			s = splbio();
			while (!conn->terminating &&
				(pdu = TAILQ_FIRST(&conn->pdus_to_send)) != NULL) {
				TAILQ_REMOVE(&conn->pdus_to_send, pdu, send_chain);
				pdu->flags &= ~PDUF_INQUEUE;
				splx(s);

#ifdef ISCSI_DEBUG
				if (!pdu->uio.uio_resid) {
					DEBOUT(("uio.resid = 0 in iscsi_send_thread! pdu=%p\n",
							pdu));
					assert(pdu->uio.uio_resid != 0);
				}
#endif
				/*DEB (99,("Send thread woke up, pdu = %x)\n", (int)pdu)); */

				/* update ExpStatSN here to avoid discontinuities */
				/* and delays in updating target */
				pdu->pdu.p.command.ExpStatSN = htonl(conn->StatSN_buf.ExpSN);

				if (conn->HeaderDigest)
					pdu->pdu.HeaderDigest = gen_digest(&pdu->pdu, BHS_SIZE);
				my_soo_write(conn, &pdu->uio);

				if (pdu->disp <= PDUDISP_FREE) {
					free_pdu(pdu);
				} else {
					pdu->flags &= ~PDUF_BUSY;
				}
				s = splbio();
			}

			/*DEB (99,("Send thread done, waiting (conn->terminating = %d)\n", */
			/* 		conn->terminating)); */

			if (!conn->terminating) {
				tsleep(&conn->pdus_to_send, PRIBIO,
						"iscsisend", 0);
			}

			splx(s);
		}

		/* ------------------------------------------------------------------------
		 *    Here this thread takes over cleanup of the terminating connection.
		 * ------------------------------------------------------------------------
		 */
		callout_stop(&conn->timeout);
		conn->idle_timeout_val = CONNECTION_IDLE_TIMEOUT;

		fp = conn->sock;

		/*
		 * We shutdown the socket here to force the receive
		 * thread to wake up
		 */
		DEBC(conn, 1, ("Closing Socket %p\n", conn->sock));
		solock(fp->f_socket);
		soshutdown(fp->f_socket, SHUT_RDWR);
		sounlock(fp->f_socket);

		/* wake up any non-reassignable waiting CCBs */
		TAILQ_FOREACH_SAFE(ccb, &conn->ccbs_waiting, chain, nccb) {
			if (!(ccb->flags & CCBF_REASSIGN) || ccb->pdu_waiting == NULL) {
				DEBC(conn, 1, ("Terminating CCB %p (t=%p)\n",
					ccb,&ccb->timeout));
				wake_ccb(ccb, conn->terminating);
			} else {
				callout_stop(&ccb->timeout);
				ccb->num_timeouts = 0;
			}
		}

		/* clean out anything left in send queue */
		while ((pdu = TAILQ_FIRST(&conn->pdus_to_send)) != NULL) {
			TAILQ_REMOVE(&conn->pdus_to_send, pdu, send_chain);
			pdu->flags &= ~(PDUF_INQUEUE | PDUF_BUSY);
			/* if it's not attached to a waiting CCB, free it */
			if (pdu->owner == NULL ||
			    pdu->owner->pdu_waiting != pdu) {
				free_pdu(pdu);
			}
		}

		/* If there's another connection available, transfer pending tasks */
		if (sess->active_connections &&
			TAILQ_FIRST(&conn->ccbs_waiting) != NULL) {
			DEBC(conn, 1, ("Reassign Tasks\n"));
			reassign_tasks(conn);
		} else if (!conn->destroy && conn->Time2Wait) {
			DEBC(conn, 1, ("Time2Wait\n"));
			tsleep(&s, PRIBIO, "Time2Wait", conn->Time2Wait * hz);
			DEBC(conn, 1, ("Time2Wait\n"));
		}
		/* notify event handlers of connection shutdown */
		DEBC(conn, 1, ("%s\n", (conn->destroy) ? "TERMINATED" : "RECOVER"));
		add_event((conn->destroy) ? ISCSI_CONNECTION_TERMINATED
								  : ISCSI_RECOVER_CONNECTION,
				  sess->id, conn->id, conn->terminating);

		DEBC(conn, 1, ("Waiting for conn_idle\n"));
		if (!conn->destroy)
			tsleep(conn, PRIBIO, "conn_idle", 30 * hz);
		DEBC(conn, 1, ("Waited for conn_idle, destroy = %d\n", conn->destroy));

	} while (!conn->destroy);

	/* wake up anyone waiting for a PDU */
	wakeup(&conn->pdu_pool);

	/* wake up any waiting CCBs */
	while ((ccb = TAILQ_FIRST(&conn->ccbs_waiting)) != NULL) {
		wake_ccb(ccb, conn->terminating);
		/* NOTE: wake_ccb will remove the CCB from the queue */
	}

	s = splbio();
	if (conn->in_session) {
		conn->in_session = FALSE;
		TAILQ_REMOVE(&sess->conn_list, conn, connections);
		sess->mru_connection = TAILQ_FIRST(&sess->conn_list);
	}

	TAILQ_INSERT_TAIL(&iscsi_cleanupc_list, conn, connections);
	splx(s);

	wakeup(&iscsi_cleanupc_list);

	conn->sendproc = NULL;
	DEBC(conn, 1, ("Send thread exits\n"));
	iscsi_num_send_threads--;
	kthread_exit(0);
}


/*
 * send_pdu:
 *    Enqueue a PDU to be sent, and handle its disposition as well as
 *    the disposition of its associated CCB.
 *
 *    Parameter:
 *          ccb      The associated CCB. May be NULL if cdisp is CCBDISP_NOWAIT
 *                   and pdisp is not PDUDISP_WAIT
 *          cdisp    The CCB's disposition
 *          pdu      The PDU
 *          pdisp    The PDU's disposition
 */

STATIC void
send_pdu(ccb_t *ccb, pdu_t *pdu, ccb_disp_t cdisp, pdu_disp_t pdisp)
{
	connection_t *conn = pdu->connection;
	ccb_disp_t prev_cdisp = 0;
	int s;

	if (ccb != NULL) {
		prev_cdisp = ccb->disp;
		pdu->pdu.InitiatorTaskTag = ccb->ITT;
		pdu->owner = ccb;
		if (cdisp != CCBDISP_NOWAIT)
			ccb->disp = cdisp;
	}

	pdu->disp = pdisp;

	DEBC(conn, 10, ("Send_pdu: ccb=%p, pcd=%d, cdsp=%d, pdu=%p, pdsp=%d\n",
			ccb, prev_cdisp, cdisp, pdu, pdisp));

	s = splbio();
	if (pdisp == PDUDISP_WAIT) {
		ccb->pdu_waiting = pdu;

		/* save UIO and IOVEC for retransmit */
		pdu->save_uio = pdu->uio;
		memcpy(pdu->save_iovec, pdu->io_vec, sizeof(pdu->save_iovec));

		pdu->flags |= PDUF_BUSY;
	}
	/* Enqueue for sending */
	pdu->flags |= PDUF_INQUEUE;

	if (pdu->flags & PDUF_PRIORITY)
		TAILQ_INSERT_HEAD(&conn->pdus_to_send, pdu, send_chain);
	else
		TAILQ_INSERT_TAIL(&conn->pdus_to_send, pdu, send_chain);

	wakeup(&conn->pdus_to_send);

	if (cdisp != CCBDISP_NOWAIT) {
		callout_schedule(&ccb->timeout, COMMAND_TIMEOUT);

		if (prev_cdisp <= CCBDISP_NOWAIT)
			suspend_ccb(ccb, TRUE);

		if (cdisp == CCBDISP_WAIT)
			tsleep(ccb, PWAIT, "sendpdu", 0);
	}
	splx(s);
}


/*
 * resend_pdu:
 *    Re-Enqueue a PDU that has apparently gotten lost.
 *
 *    Parameter:
 *          ccb      The associated CCB.
 */

void
resend_pdu(ccb_t *ccb)
{
	connection_t *conn = ccb->connection;
	pdu_t *pdu = ccb->pdu_waiting;
	int s;

	s = splbio ();
	if (pdu == NULL || (pdu->flags & PDUF_BUSY)) {
		splx (s);
		return;
	}
	pdu->flags |= PDUF_BUSY;
	splx (s);

	/* restore UIO and IOVEC */
	pdu->uio = pdu->save_uio;
	memcpy(pdu->io_vec, pdu->save_iovec, sizeof(pdu->io_vec));

	DEBC(conn, 8, ("ReSend_pdu ccb=%p, pdu=%p\n", ccb, pdu));

	s = splbio ();

	/* Enqueue for sending */
	pdu->flags |= PDUF_INQUEUE;

	if (pdu->flags & PDUF_PRIORITY) {
		TAILQ_INSERT_HEAD(&conn->pdus_to_send, pdu, send_chain);
	} else {
		TAILQ_INSERT_TAIL(&conn->pdus_to_send, pdu, send_chain);
	}
	callout_schedule(&ccb->timeout, COMMAND_TIMEOUT);
	splx (s);

	wakeup(&conn->pdus_to_send);
}


/*
 * setup_tx_uio:
 *    Initialize the uio structure for sending, including header,
 *    data (if present), padding, and Data Digest.
 *    Header Digest is generated in send thread.
 *
 *    Parameter:
 *          pdu      The PDU
 *          dsl      The Data Segment Length
 *          data     The data pointer
 *          read     TRUE if this is a read operation
 */

STATIC void
setup_tx_uio(pdu_t *pdu, uint32_t dsl, void *data, bool read)
{
	static uint8_t pad_bytes[4] = { 0 };
	struct uio *uio;
	int i, pad, hlen;
	connection_t *conn = pdu->connection;

	DEB(99, ("SetupTxUio: dlen = %d, dptr: %p, read: %d\n",
			 dsl, data, read));

	if (!read && dsl) {
		hton3(dsl, pdu->pdu.DataSegmentLength);
	}
	hlen = (conn->HeaderDigest) ? BHS_SIZE + 4 : BHS_SIZE;

	pdu->io_vec[0].iov_base = &pdu->pdu;
	pdu->io_vec[0].iov_len = hlen;

	uio = &pdu->uio;

	uio->uio_iov = pdu->io_vec;
	uio->uio_iovcnt = 1;
	uio->uio_rw = UIO_WRITE;
	uio->uio_resid = hlen;
	UIO_SETUP_SYSSPACE(uio);

	if (!read && dsl) {
		uio->uio_iovcnt++;
		pdu->io_vec[1].iov_base = data;
		pdu->io_vec[1].iov_len = dsl;
		uio->uio_resid += dsl;

		/* Pad to next multiple of 4 */
		pad = uio->uio_resid & 0x03;
		if (pad) {
			i = uio->uio_iovcnt++;
			pad = 4 - pad;
			pdu->io_vec[i].iov_base = pad_bytes;
			pdu->io_vec[i].iov_len = pad;
			uio->uio_resid += pad;
		}

		if (conn->DataDigest) {
			pdu->data_digest = gen_digest_2(data, dsl, pad_bytes, pad);
			i = uio->uio_iovcnt++;
			pdu->io_vec[i].iov_base = &pdu->data_digest;
			pdu->io_vec[i].iov_len = 4;
			uio->uio_resid += 4;
		}
	}
}

/*
 * init_login_pdu:
 *    Initialize the login PDU.
 *
 *    Parameter:
 *          conn     The connection
 *          pdu      The PDU
 */

STATIC void
init_login_pdu(connection_t *conn, pdu_t *ppdu, bool next)
{
	pdu_header_t *pdu = &ppdu->pdu;
	login_isid_t *isid = (login_isid_t *) & pdu->LUN;
	uint8_t c_phase;

	pdu->Opcode = IOP_Login_Request | OP_IMMEDIATE;

	if (next) {
		c_phase = (pdu->Flags >> CSG_SHIFT) & SG_MASK;
		pdu->Flags = FLAG_TRANSIT | (c_phase << CSG_SHIFT) |
					 NEXT_PHASE(c_phase);
	}

	memcpy(isid, &iscsi_InitiatorISID, 6);
	isid->TSIH = conn->session->TSIH;

	pdu->p.login_req.CID = htons(conn->id);
	pdu->p.login_req.CmdSN = htonl(conn->session->CmdSN);
}


/*
 * negotiate_login:
 *    Control login negotiation.
 *
 *    Parameter:
 *          conn     The connection
 *          rx_pdu   The received login response PDU
 *          tx_ccb   The originally sent login CCB
 */

void
negotiate_login(connection_t *conn, pdu_t *rx_pdu, ccb_t *tx_ccb)
{
	int rc;
	bool next = TRUE;
	pdu_t *tx_pdu;
	uint8_t c_phase;

	if (rx_pdu->pdu.Flags & FLAG_TRANSIT)
		c_phase = rx_pdu->pdu.Flags & SG_MASK;
	else
		c_phase = (rx_pdu->pdu.Flags >> CSG_SHIFT) & SG_MASK;

	DEB(99, ("NegotiateLogin: Flags=%x Phase=%x\n",
			 rx_pdu->pdu.Flags, c_phase));

	if (c_phase == SG_FULL_FEATURE_PHASE) {
		session_t *sess = conn->session;

		if (!sess->TSIH)
			sess->TSIH = ((login_isid_t *) &rx_pdu->pdu.LUN)->TSIH;

		if (rx_pdu->temp_data != NULL)
			assemble_negotiation_parameters(conn, tx_ccb, rx_pdu, NULL);

		/* negotiated values are now valid */
		set_negotiated_parameters(tx_ccb);

		DEBC(conn, 5, ("Login Successful!\n"));
		wake_ccb(tx_ccb, ISCSI_STATUS_SUCCESS);
		return;
	}

	tx_pdu = get_pdu(conn, TRUE);
	if (tx_pdu == NULL)
		return;

	tx_pdu->pdu.Flags = c_phase << CSG_SHIFT;

	switch (c_phase) {
	case SG_SECURITY_NEGOTIATION:
		rc = assemble_security_parameters(conn, tx_ccb, rx_pdu, tx_pdu);
		if (rc < 0)
			next = FALSE;
		break;

	case SG_LOGIN_OPERATIONAL_NEGOTIATION:
		rc = assemble_negotiation_parameters(conn, tx_ccb, rx_pdu, tx_pdu);
		break;

	default:
		DEBOUT(("Invalid phase %x in negotiate_login\n", c_phase));
		rc = ISCSI_STATUS_TARGET_ERROR;
		break;
	}

	if (rc > 0) {
		wake_ccb(tx_ccb, rc);
		free_pdu(tx_pdu);
	} else {
		init_login_pdu(conn, tx_pdu, next);
		setup_tx_uio(tx_pdu, tx_pdu->temp_data_len, tx_pdu->temp_data, FALSE);
		send_pdu(tx_ccb, tx_pdu, CCBDISP_NOWAIT, PDUDISP_FREE);
	}
}


/*
 * init_text_pdu:
 *    Initialize the text PDU.
 *
 *    Parameter:
 *          conn     The connection
 *          ppdu     The transmit PDU
 *          rx_pdu   The received PDU if this is an unsolicited negotiation
 */

STATIC void
init_text_pdu(connection_t *conn, pdu_t *ppdu, pdu_t *rx_pdu)
{
	pdu_header_t *pdu = &ppdu->pdu;

	pdu->Opcode = IOP_Text_Request | OP_IMMEDIATE;
	pdu->Flags = FLAG_FINAL;

	if (rx_pdu != NULL) {
		pdu->p.text_req.TargetTransferTag =
			rx_pdu->pdu.p.text_rsp.TargetTransferTag;
		pdu->LUN = rx_pdu->pdu.LUN;
	} else
		pdu->p.text_req.TargetTransferTag = 0xffffffff;

	pdu->p.text_req.CmdSN = htonl(conn->session->CmdSN);
}


/*
 * acknowledge_text:
 *    Acknowledge a continued login or text response.
 *
 *    Parameter:
 *          conn     The connection
 *          rx_pdu   The received login/text response PDU
 *          tx_ccb   The originally sent login/text request CCB
 */

void
acknowledge_text(connection_t *conn, pdu_t *rx_pdu, ccb_t *tx_ccb)
{
	pdu_t *tx_pdu;

	tx_pdu = get_pdu(conn, TRUE);
	if (tx_pdu == NULL)
		return;

	if (rx_pdu != NULL &&
		(rx_pdu->pdu.Opcode & OPCODE_MASK) == IOP_Login_Request)
		init_login_pdu(conn, tx_pdu, FALSE);
	else
		init_text_pdu(conn, tx_pdu, rx_pdu);

	setup_tx_uio(tx_pdu, 0, NULL, FALSE);
	send_pdu(tx_ccb, tx_pdu, CCBDISP_NOWAIT, PDUDISP_FREE);
}


/*
 * start_text_negotiation:
 *    Handle target request to negotiate (via asynch event)
 *
 *    Parameter:
 *          conn     The connection
 */

void
start_text_negotiation(connection_t *conn)
{
	pdu_t *pdu;
	ccb_t *ccb;

	ccb = get_ccb(conn, TRUE);
	if (ccb == NULL)
		return;
	pdu = get_pdu(conn, TRUE);
	if (pdu == NULL) {
		free_ccb(ccb);
		return;
	}

	if (init_text_parameters(conn, ccb)) {
		free_ccb(ccb);
		free_pdu(pdu);
		return;
	}

	init_text_pdu(conn, pdu, NULL);
	setup_tx_uio(pdu, 0, NULL, FALSE);
	send_pdu(ccb, pdu, CCBDISP_FREE, PDUDISP_WAIT);
}


/*
 * negotiate_text:
 *    Handle received text negotiation.
 *
 *    Parameter:
 *          conn     The connection
 *          rx_pdu   The received text response PDU
 *          tx_ccb   The original CCB
 */

void
negotiate_text(connection_t *conn, pdu_t *rx_pdu, ccb_t *tx_ccb)
{
	int rc;
	pdu_t *tx_pdu;

	if (tx_ccb->flags & CCBF_SENDTARGET) {
		if (!(rx_pdu->pdu.Flags & FLAG_FINAL)) {
			handle_connection_error(conn, ISCSI_STATUS_PROTOCOL_ERROR,
									LOGOUT_CONNECTION);
			return;
		}
		/* transfer ownership of text to CCB */
		tx_ccb->text_data = rx_pdu->temp_data;
		tx_ccb->text_len = rx_pdu->temp_data_len;
		rx_pdu->temp_data = NULL;
		wake_ccb(tx_ccb, ISCSI_STATUS_SUCCESS);
	} else {
		if (!(rx_pdu->pdu.Flags & FLAG_FINAL))
			tx_pdu = get_pdu(conn, TRUE);
		else
			tx_pdu = NULL;

		rc = assemble_negotiation_parameters(conn, tx_ccb, rx_pdu, tx_pdu);
		if (rc) {
			if (tx_pdu != NULL)
				free_pdu(tx_pdu);

			handle_connection_error(conn, rc, LOGOUT_CONNECTION);
		} else if (tx_pdu != NULL) {
			init_text_pdu(conn, tx_pdu, rx_pdu);
			setup_tx_uio(tx_pdu, tx_pdu->temp_data_len, tx_pdu->temp_data,
						 FALSE);
			send_pdu(tx_ccb, tx_pdu, CCBDISP_NOWAIT, PDUDISP_FREE);
		} else {
			set_negotiated_parameters(tx_ccb);
			wake_ccb(tx_ccb, ISCSI_STATUS_SUCCESS);
		}
	}
}


/*
 * send_send_targets:
 *    Send out a SendTargets text request.
 *    The result is stored in the fields in the session structure.
 *
 *    Parameter:
 *          session  The session
 *          key      The text key to use
 *
 *    Returns:    0 on success, else an error code.
 */

int
send_send_targets(session_t *session, uint8_t *key)
{
	ccb_t *ccb;
	pdu_t *pdu;
	int rc = 0;
	connection_t *conn;

	DEB(9, ("Send_send_targets\n"));

	conn = assign_connection(session, TRUE);
	if (conn == NULL || conn->terminating || conn->state != ST_FULL_FEATURE)
		return (conn != NULL && conn->terminating) ? conn->terminating
			: ISCSI_STATUS_CONNECTION_FAILED;

	ccb = get_ccb(conn, TRUE);
	if (ccb == NULL)
		return conn->terminating;
	pdu = get_pdu(conn, TRUE);
	if (pdu == NULL) {
		free_ccb(ccb);
		return conn->terminating;
	}

	ccb->flags |= CCBF_SENDTARGET;

	if ((rc = assemble_send_targets(pdu, key)) != 0) {
		free_ccb(ccb);
		free_pdu(pdu);
		return rc;
	}

	init_text_pdu(conn, pdu, NULL);

	setup_tx_uio(pdu, pdu->temp_data_len, pdu->temp_data, FALSE);
	send_pdu(ccb, pdu, CCBDISP_WAIT, PDUDISP_WAIT);

	rc = ccb->status;
	if (!rc) {
		/* transfer ownership of data */
		session->target_list = ccb->text_data;
		session->target_list_len = ccb->text_len;
		ccb->text_data = NULL;
	}
	free_ccb(ccb);
	return rc;
}


/*
 * send_nop_out:
 *    Send nop out request.
 *
 *    Parameter:
 *          conn     The connection
 *          rx_pdu   The received Nop-In PDU
 *
 *    Returns:    0 on success, else an error code.
 */

int
send_nop_out(connection_t *conn, pdu_t *rx_pdu)
{
	ccb_t *ccb;
	pdu_t *ppdu;
	pdu_header_t *pdu;

	DEBC(conn, 10, ("Send NOP_Out rx_pdu=%p\n", rx_pdu));

	if (rx_pdu != NULL) {
		ccb = NULL;
		ppdu = get_pdu(conn, TRUE);
		if (ppdu == NULL)
			return 1;
	} else {
		ccb = get_ccb(conn, FALSE);
		if (ccb == NULL) {
			DEBOUT(("Can't get CCB in send_nop_out\n"));
			return 1;
		}
		ppdu = get_pdu(conn, FALSE);
		if (ppdu == NULL) {
			free_ccb(ccb);
			DEBOUT(("Can't get PDU in send_nop_out\n"));
			return 1;
		}
	}

	pdu = &ppdu->pdu;
	pdu->Flags = FLAG_FINAL;
	pdu->Opcode = IOP_NOP_Out | OP_IMMEDIATE;

	if (rx_pdu != NULL) {
		pdu->p.nop_out.TargetTransferTag =
			rx_pdu->pdu.p.nop_in.TargetTransferTag;
		pdu->InitiatorTaskTag = rx_pdu->pdu.InitiatorTaskTag;
		pdu->p.nop_out.CmdSN = htonl(conn->session->CmdSN);
		pdu->LUN = rx_pdu->pdu.LUN;
	} else {
		pdu->p.nop_out.TargetTransferTag = 0xffffffff;
		ccb->CmdSN = ccb->session->CmdSN;
		pdu->p.nop_out.CmdSN = htonl(ccb->CmdSN);
	}

	setup_tx_uio(ppdu, 0, NULL, FALSE);
	send_pdu(ccb, ppdu, (rx_pdu != NULL) ? CCBDISP_NOWAIT : CCBDISP_FREE,
			 PDUDISP_FREE);
	return 0;
}


/*
 * snack_missing:
 *    Send SNACK request for missing data.
 *
 *    Parameter:
 *          conn     The connection
 *          ccb      The task's CCB (for Data NAK only)
 *          type     The SNACK type
 *          BegRun   The BegRun field
 *          RunLength   The RunLength field
 */

void
snack_missing(connection_t *conn, ccb_t *ccb, uint8_t type,
			  uint32_t BegRun, uint32_t RunLength)
{
	pdu_t *ppdu;
	pdu_header_t *pdu;

	ppdu = get_pdu(conn, TRUE);
	if (ppdu == NULL)
		return;
	pdu = &ppdu->pdu;
	pdu->Opcode = IOP_SNACK_Request;
	pdu->Flags = FLAG_FINAL | type;

	pdu->InitiatorTaskTag = (type == SNACK_DATA_NAK) ? ccb->ITT : 0xffffffff;
	pdu->p.snack.TargetTransferTag = 0xffffffff;
	pdu->p.snack.BegRun = htonl(BegRun);
	pdu->p.snack.RunLength = htonl(RunLength);

	ppdu->flags = PDUF_PRIORITY;

	setup_tx_uio(ppdu, 0, NULL, FALSE);
	send_pdu(NULL, ppdu, CCBDISP_NOWAIT, PDUDISP_FREE);
}


/*
 * send_snack:
 *    Send SNACK request.
 *
 *    Parameter:
 *          conn     The connection
 *          rx_pdu   The received data in PDU
 *          tx_ccb   The original command CCB (required for Data ACK only)
 *          type     The SNACK type
 *
 *    Returns:    0 on success, else an error code.
 */

void
send_snack(connection_t *conn, pdu_t *rx_pdu, ccb_t *tx_ccb, uint8_t type)
{
	pdu_t *ppdu;
	pdu_header_t *pdu;

	ppdu = get_pdu(conn, TRUE);
	if (ppdu == NULL)
		return;
	pdu = &ppdu->pdu;
	pdu->Opcode = IOP_SNACK_Request;
	pdu->Flags = FLAG_FINAL | type;

	switch (type) {
	case SNACK_DATA_NAK:
		pdu->InitiatorTaskTag = rx_pdu->pdu.InitiatorTaskTag;
		pdu->p.snack.TargetTransferTag = 0xffffffff;
		pdu->p.snack.BegRun = rx_pdu->pdu.p.data_in.DataSN;
		pdu->p.snack.RunLength = htonl(1);
		break;

	case SNACK_STATUS_NAK:
		pdu->InitiatorTaskTag = 0xffffffff;
		pdu->p.snack.TargetTransferTag = 0xffffffff;
		pdu->p.snack.BegRun = rx_pdu->pdu.p.response.StatSN;
		pdu->p.snack.RunLength = htonl(1);
		break;

	case SNACK_DATA_ACK:
		pdu->InitiatorTaskTag = 0xffffffff;
		pdu->p.snack.TargetTransferTag =
			rx_pdu->pdu.p.data_in.TargetTransferTag;
		pdu->p.snack.BegRun = tx_ccb->DataSN_buf.ExpSN;
		pdu->p.snack.RunLength = 0;
		break;

	default:
		DEBOUT(("Invalid type %d in send_snack\n", type));
		return;
	}

	pdu->LUN = rx_pdu->pdu.LUN;

	ppdu->flags = PDUF_PRIORITY;

	setup_tx_uio(ppdu, 0, NULL, FALSE);
	send_pdu(NULL, ppdu, CCBDISP_NOWAIT, PDUDISP_FREE);
}


/*
 * send_login:
 *    Send login request.
 *
 *    Parameter:
 *          conn     The connection
 *          par      The login parameters (for negotiation)
 *
 *    Returns:       0 on success, else an error code.
 */

int
send_login(connection_t *conn)
{
	ccb_t *ccb;
	pdu_t *pdu;
	int rc;

	DEBC(conn, 9, ("Send_login\n"));
	ccb = get_ccb(conn, TRUE);
	/* only if terminating (which couldn't possibly happen here, but...) */
	if (ccb == NULL)
		return conn->terminating;
	pdu = get_pdu(conn, TRUE);
	if (pdu == NULL) {
		free_ccb(ccb);
		return conn->terminating;
	}

	if ((rc = assemble_login_parameters(conn, ccb, pdu)) <= 0) {
		init_login_pdu(conn, pdu, !rc);
		setup_tx_uio(pdu, pdu->temp_data_len, pdu->temp_data, FALSE);
		send_pdu(ccb, pdu, CCBDISP_WAIT, PDUDISP_FREE);
		rc = ccb->status;
	} else {
		free_pdu(pdu);
	}
	free_ccb(ccb);
	return rc;
}


/*
 * send_logout:
 *    Send logout request.
 *	  NOTE: This function does not wait for the logout to complete.
 *
 *    Parameter:
 *          conn	The connection
 *			refconn	The referenced connection
 *			reason	The reason code
 *			wait	Wait for completion if TRUE
 *
 *    Returns:       0 on success (logout sent), else an error code.
 */

int
send_logout(connection_t *conn, connection_t *refconn, int reason,
			bool wait)
{
	ccb_t *ccb;
	pdu_t *ppdu;
	pdu_header_t *pdu;

	DEBC(conn, 5, ("Send_logout\n"));
	ccb = get_ccb(conn, TRUE);
	/* can only happen if terminating... */
	if (ccb == NULL)
		return conn->terminating;
	ppdu = get_pdu(conn, TRUE);
	if (ppdu == NULL) {
		free_ccb(ccb);
		return conn->terminating;
	}

	pdu = &ppdu->pdu;
	pdu->Opcode = IOP_Logout_Request | OP_IMMEDIATE;

	pdu->Flags = FLAG_FINAL | reason;
	ccb->CmdSN = conn->session->CmdSN;
	pdu->p.logout_req.CmdSN = htonl(ccb->CmdSN);
	if (reason > 0)
		pdu->p.logout_req.CID = htons(refconn->id);

	ccb->par = refconn;
	if (refconn != conn) {
		ccb->flags |= CCBF_OTHERCONN;
	} else {
		conn->state = ST_LOGOUT_SENT;
		conn->loggedout = LOGOUT_SENT;
	}

	setup_tx_uio(ppdu, 0, NULL, FALSE);

	send_pdu(ccb, ppdu, (wait) ? CCBDISP_WAIT : CCBDISP_FREE, PDUDISP_FREE);

	if (wait) {
		int rc = ccb->status;
		free_ccb (ccb);
		return rc;
	}
	return 0;
}


/*
 * send_task_management:
 *    Send task management request.
 *
 *    Parameter:
 *          conn     The connection
 *          ref_ccb  The referenced command (NULL if none)
 *          xs       The scsipi command structure (NULL if not a scsipi request)
 *          function The function code
 *
 *    Returns:       0 on success, else an error code.
 */

int
send_task_management(connection_t *conn, ccb_t *ref_ccb, struct scsipi_xfer *xs,
					 int function)
{
	ccb_t *ccb;
	pdu_t *ppdu;
	pdu_header_t *pdu;

	DEBC(conn, 5, ("Send_task_management, ref_ccb=%p, func = %d\n",
			ref_ccb, function));

	if (function == TASK_REASSIGN && conn->session->ErrorRecoveryLevel < 2)
		return ISCSI_STATUS_CANT_REASSIGN;

	ccb = get_ccb(conn, xs == NULL);
	/* can only happen if terminating... */
	if (ccb == NULL)
		return conn->terminating;
	ppdu = get_pdu(conn, TRUE);
	if (ppdu == NULL) {
		free_ccb(ccb);
		return conn->terminating;
	}

	ccb->xs = xs;

	pdu = &ppdu->pdu;
	pdu->Opcode = IOP_SCSI_Task_Management | OP_IMMEDIATE;
	pdu->Flags = FLAG_FINAL | function;

	ccb->CmdSN = conn->session->CmdSN;
	pdu->p.task_req.CmdSN = htonl(ccb->CmdSN);

	if (ref_ccb != NULL) {
		pdu->p.task_req.ReferencedTaskTag = ref_ccb->ITT;
		pdu->p.task_req.RefCmdSN = htonl(ref_ccb->CmdSN);
		pdu->p.task_req.ExpDataSN = htonl(ref_ccb->DataSN_buf.ExpSN);
	} else
		pdu->p.task_req.ReferencedTaskTag = 0xffffffff;

	ppdu->flags |= PDUF_PRIORITY;

	setup_tx_uio(ppdu, 0, NULL, FALSE);
	send_pdu(ccb, ppdu, (xs) ? CCBDISP_SCSIPI : CCBDISP_WAIT, PDUDISP_FREE);

	if (xs == NULL) {
		int rc = ccb->status;
		free_ccb(ccb);
		return rc;
	}
	return 0;
}


/*
 * send_data_out:
 *    Send data to target in response to an R2T or as unsolicited data.
 *
 *    Parameter:
 *          conn     The connection
 *          rx_pdu   The received R2T PDU (NULL if unsolicited)
 *          tx_ccb   The originally sent command CCB
 *          waitok   Whether it's OK to wait for an available PDU or not
 */

int
send_data_out(connection_t *conn, pdu_t *rx_pdu, ccb_t *tx_ccb,
			  ccb_disp_t disp, bool waitok)
{
	pdu_header_t *pdu;
	uint32_t totlen, len, offs, sn;
	pdu_t *tx_pdu;

	assert(conn->max_transfer != 0);

	if (rx_pdu) {
		offs = ntohl(rx_pdu->pdu.p.r2t.BufferOffset);
		totlen = ntohl(rx_pdu->pdu.p.r2t.DesiredDataTransferLength);
	} else {
		offs = conn->max_firstimmed;
		totlen = min(conn->max_firstdata - offs, tx_ccb->data_len - offs);
	}
	sn = 0;

	while (totlen) {
		len = min(totlen, conn->max_transfer);

		tx_pdu = get_pdu(conn, waitok);
		if (tx_pdu == NULL) {
			DEBOUT(("No PDU in send_data_out\n"));

			tx_ccb->disp = disp;
			tx_ccb->status = ISCSI_STATUS_NO_RESOURCES;
			handle_connection_error(conn, ISCSI_STATUS_NO_RESOURCES, NO_LOGOUT);

			return ISCSI_STATUS_NO_RESOURCES;
		}

		totlen -= len;
		pdu = &tx_pdu->pdu;
		pdu->Opcode = IOP_SCSI_Data_out;
		if (!totlen)
			pdu->Flags = FLAG_FINAL;

		if (rx_pdu != NULL)
			pdu->p.data_out.TargetTransferTag =
				rx_pdu->pdu.p.r2t.TargetTransferTag;
		else
			pdu->p.data_out.TargetTransferTag = 0xffffffff;
		pdu->p.data_out.BufferOffset = htonl(offs);
		pdu->p.data_out.DataSN = htonl(sn);

		DEBC(conn, 10, ("Send DataOut: DataSN %d, len %d offs %x totlen %d\n",
				sn, len, offs, totlen));

		setup_tx_uio(tx_pdu, len, tx_ccb->data_ptr + offs, FALSE);

		send_pdu(tx_ccb, tx_pdu, (totlen) ? CCBDISP_NOWAIT : disp, PDUDISP_FREE);

		sn++;
		offs += len;
	}
	return 0;
}


/*
 * send_command:
 *    Send a SCSI command request.
 *
 *    Parameter:
 *          CCB      The CCB
 *          disp     The CCB disposition
 */

void
send_command(ccb_t *ccb, ccb_disp_t disp, bool waitok, bool immed)
{
	uint32_t totlen, len;
	connection_t *conn = ccb->connection;
	session_t *sess = ccb->session;
	pdu_t *ppdu;
	pdu_header_t *pdu;
	int s;

	s = splbio();
	while (/*CONSTCOND*/ISCSI_THROTTLING_ENABLED &&
	    /*CONSTCOND*/!ISCSI_SERVER_TRUSTED &&
	    !sn_a_le_b(sess->CmdSN, sess->MaxCmdSN)) {

		ccb->disp = disp;
		if (waitok)
			ccb->flags |= CCBF_WAITING;
		throttle_ccb(ccb, TRUE);

		if (!waitok) {
			splx(s);
			return;
		}

		tsleep(ccb, PWAIT, "waitMaxCmd", 0);

		throttle_ccb(ccb, FALSE);
		ccb->flags &= ~CCBF_WAITING;
	}
	splx(s);
	ppdu = get_pdu(conn, FALSE);
	if (ppdu == NULL) {
		wake_ccb(ccb, ISCSI_STATUS_NO_RESOURCES);
		return;
	}

	totlen = len = ccb->data_len;

	pdu = &ppdu->pdu;
	pdu->LUN = htonq(ccb->lun);
	memcpy(pdu->p.command.SCSI_CDB, ccb->cmd, ccb->cmdlen);
	pdu->Opcode = IOP_SCSI_Command;
	if (immed)
		pdu->Opcode |= OP_IMMEDIATE;
	pdu->p.command.ExpectedDataTransferLength = htonl(totlen);

	if (totlen) {
		if (ccb->data_in) {
			pdu->Flags = FLAG_READ;
			totlen = 0;
		} else {
			pdu->Flags = FLAG_WRITE;
			/* immediate data we can send */
			len = min(totlen, conn->max_firstimmed);

			/* can we send more unsolicited data ? */
			totlen = conn->max_firstdata ? totlen - len : 0;
		}
	}

	if (!totlen)
		pdu->Flags |= FLAG_FINAL;

	if (ccb->data_in)
		init_sernum(&ccb->DataSN_buf);

	ccb->sense_len_got = 0;
	ccb->xfer_len = 0;
	ccb->residual = 0;
	ccb->flags |= CCBF_REASSIGN;

	s = splbio();
	ccb->CmdSN = sess->CmdSN;
	if (!immed)
		sess->CmdSN++;
	splx(s);

	pdu->p.command.CmdSN = htonl(ccb->CmdSN);

	DEBC(conn, 10, ("Send Command: CmdSN %d, data_in %d, len %d, totlen %d\n",
			ccb->CmdSN, ccb->data_in, len, totlen));

	setup_tx_uio(ppdu, len, ccb->data_ptr, ccb->data_in);
	send_pdu(ccb, ppdu, (totlen) ? CCBDISP_DEFER : disp, PDUDISP_WAIT);

	if (totlen)
		send_data_out(conn, NULL, ccb, disp, waitok);
}


/*
 * send_run_xfer:
 *    Handle a SCSI command transfer request from scsipi.
 *
 *    Parameter:
 *          session  The session
 *          xs       The transfer parameters
 */

void
send_run_xfer(session_t *session, struct scsipi_xfer *xs)
{
	ccb_t *ccb;
	connection_t *conn;
	bool waitok;

	waitok = !(xs->xs_control & XS_CTL_NOSLEEP);

	DEB(10, ("RunXfer: flags=%x, data=%p, datalen=%d, resid=%d, cmdlen=%d, "
			"waitok=%d\n", xs->xs_control, xs->data, xs->datalen,
			xs->resid, xs->cmdlen, waitok));

	conn = assign_connection(session, waitok);

	if (conn == NULL || conn->terminating || conn->state != ST_FULL_FEATURE) {
		xs->error = XS_SELTIMEOUT;
		DEBC(conn, 10, ("run_xfer on dead connection\n"));
		scsipi_done(xs);
		return;
	}

	if (xs->xs_control & XS_CTL_RESET) {
		if (send_task_management(conn, NULL, xs, TARGET_WARM_RESET)) {
			xs->error = XS_SELTIMEOUT;
			scsipi_done(xs);
		}
		return;
	}

	ccb = get_ccb(conn, waitok);
	if (ccb == NULL) {
		xs->error = XS_BUSY;
		xs->status = SCSI_QUEUE_FULL;
		DEBC(conn, 0, ("No CCB in run_xfer\n"));
		scsipi_done(xs);
		return;
	}
	/* copy parameters into CCB for easier access */
	ccb->xs = xs;

	ccb->data_in = (xs->xs_control & XS_CTL_DATA_IN) != 0;
	ccb->data_len = (uint32_t) xs->datalen;
	ccb->data_ptr = xs->data;

	ccb->sense_len_req = sizeof(xs->sense.scsi_sense);
	ccb->sense_ptr = &xs->sense;

	ccb->lun = ((uint64_t) (uint8_t) xs->xs_periph->periph_lun) << 48;
	ccb->cmd = (uint8_t *) xs->cmd;
	ccb->cmdlen = xs->cmdlen;
	DEB(10, ("RunXfer: Periph_lun = %d, cmd[1] = %x, cmdlen = %d\n",
			xs->xs_periph->periph_lun, ccb->cmd[1], xs->cmdlen));

#ifdef LUN_1
	ccb->lun += 0x1000000000000LL;
	ccb->cmd[1] += 0x10;
#endif
	send_command(ccb, CCBDISP_SCSIPI, waitok, FALSE);
}


#ifndef ISCSI_MINIMAL
/*
 * send_io_command:
 *    Handle a SCSI io command request from user space.
 *
 *    Parameter:
 *          session 	The session
 *          lun		    The LUN to use
 *          req			The SCSI request block
 *			immed		Immediate command if TRUE
 *			conn_id		Assign to this connection ID if nonzero
 */

int
send_io_command(session_t *session, uint64_t lun, scsireq_t *req,
				bool immed, uint32_t conn_id)
{
	ccb_t *ccb;
	connection_t *conn;
	int rc;

	DEB(9, ("IoCommand: lun=%x, datalen=%d, cmdlen=%d, immed=%d, cid=%d\n",
			(int) lun, (int) req->datalen, (int) req->cmdlen, immed, conn_id));

	conn = (conn_id) ? find_connection(session, conn_id)
					 : assign_connection(session, TRUE);

	if (conn == NULL || conn->terminating || conn->state != ST_FULL_FEATURE) {
		DEBOUT(("io_command on dead connection (state = %d)\n",
				(conn != NULL) ? conn->state : -1));
		return ISCSI_STATUS_INVALID_CONNECTION_ID;
	}

	ccb = get_ccb(conn, TRUE);
	if (ccb == NULL) {
		DEBOUT(("No CCB in io_command\n"));
		return ISCSI_STATUS_NO_RESOURCES;
	}

	ccb->data_in = (req->flags & SCCMD_READ) != 0;
	ccb->data_len = (uint32_t) req->datalen;
	ccb->data_ptr = req->databuf;

	ccb->sense_len_req = req->senselen;
	ccb->sense_ptr = &req->sense;

	ccb->lun = lun;
	ccb->cmd = (uint8_t *) req->cmd;
	ccb->cmdlen = req->cmdlen;
	DEBC(conn, 10, ("IoCommand: cmd[1] = %x, cmdlen = %d\n",
			 ccb->cmd[1], ccb->cmdlen));

	send_command(ccb, CCBDISP_WAIT, TRUE, immed);

	rc = ccb->status;

	req->senselen_used = ccb->sense_len_got;
	req->datalen_used = req->datalen - ccb->residual;

	free_ccb(ccb);

	return rc;
}
#endif


/*****************************************************************************
 * Timeout handlers
 *****************************************************************************/
/*
 * connection_timeout:
 *    Handle prolonged silence on a connection by checking whether
 *    it's still alive.
 *    This has the side effect of discovering missing status or lost commands
 *    before those time out.
 *
 *    Parameter:
 *          par      The connection
 */

void
connection_timeout(void *par)
{
	connection_t *conn = (connection_t *) par;

	if (++conn->num_timeouts > MAX_CONN_TIMEOUTS)
		handle_connection_error(conn, ISCSI_STATUS_TIMEOUT, NO_LOGOUT);
	else {
		if (conn->state == ST_FULL_FEATURE)
			send_nop_out(conn, NULL);

		callout_schedule(&conn->timeout, CONNECTION_TIMEOUT);
	}
}

/*
 * ccb_timeout:
 *    Handle timeout of a sent command.
 *
 *    Parameter:
 *          par      The CCB
 */

void
ccb_timeout(void *par)
{
	ccb_t *ccb = (ccb_t *) par;
	connection_t *conn = ccb->connection;

	ccb->total_tries++;

	if (++ccb->num_timeouts > MAX_CCB_TIMEOUTS ||
		ccb->total_tries > MAX_CCB_TRIES ||
		ccb->disp <= CCBDISP_FREE ||
		!ccb->session->ErrorRecoveryLevel) {

		wake_ccb(ccb, ISCSI_STATUS_TIMEOUT);
		handle_connection_error(conn, ISCSI_STATUS_TIMEOUT, RECOVER_CONNECTION);
	} else {
		if (ccb->data_in && ccb->xfer_len < ccb->data_len) {
			/* request resend of all missing data */
			snack_missing(conn, ccb, SNACK_DATA_NAK, 0, 0);
		} else {
			/* request resend of all missing status */
			snack_missing(conn, NULL, SNACK_STATUS_NAK, 0, 0);
		}
		callout_schedule(&ccb->timeout, COMMAND_TIMEOUT);
	}
}
