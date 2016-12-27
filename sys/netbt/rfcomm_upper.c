/*	$NetBSD: rfcomm_upper.c,v 1.22 2014/11/16 21:34:27 plunky Exp $	*/

/*-
 * Copyright (c) 2006 Itronix Inc.
 * All rights reserved.
 *
 * Written by Iain Hibbert for Itronix Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Itronix Inc. may not be used to endorse
 *    or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ITRONIX INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ITRONIX INC. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rfcomm_upper.c,v 1.22 2014/11/16 21:34:27 plunky Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/kmem.h>
#include <sys/socketvar.h>
#include <sys/systm.h>

#include <netbt/bluetooth.h>
#include <netbt/hci.h>
#include <netbt/l2cap.h>
#include <netbt/rfcomm.h>

/****************************************************************************
 *
 *	RFCOMM DLC - Upper Protocol API
 *
 * Currently the only 'Port Emulation Entity' is the RFCOMM socket code
 * but it is should be possible to provide a pseudo-device for a direct
 * tty interface.
 */

/*
 * rfcomm_attach_pcb(handle, proto, upper)
 *
 * attach a new RFCOMM DLC to handle, populate with reasonable defaults
 */
int
rfcomm_attach_pcb(struct rfcomm_dlc **handle,
		const struct btproto *proto, void *upper)
{
	struct rfcomm_dlc *dlc;

	KASSERT(handle != NULL);
	KASSERT(proto != NULL);
	KASSERT(upper != NULL);

	dlc = kmem_intr_zalloc(sizeof(struct rfcomm_dlc), KM_NOSLEEP);
	if (dlc == NULL)
		return ENOMEM;

	dlc->rd_state = RFCOMM_DLC_CLOSED;
	dlc->rd_mtu = rfcomm_mtu_default;

	dlc->rd_proto = proto;
	dlc->rd_upper = upper;

	dlc->rd_laddr.bt_len = sizeof(struct sockaddr_bt);
	dlc->rd_laddr.bt_family = AF_BLUETOOTH;
	dlc->rd_laddr.bt_psm = L2CAP_PSM_RFCOMM;

	dlc->rd_raddr.bt_len = sizeof(struct sockaddr_bt);
	dlc->rd_raddr.bt_family = AF_BLUETOOTH;
	dlc->rd_raddr.bt_psm = L2CAP_PSM_RFCOMM;

	dlc->rd_lmodem = RFCOMM_MSC_RTC | RFCOMM_MSC_RTR | RFCOMM_MSC_DV;

	callout_init(&dlc->rd_timeout, 0);
	callout_setfunc(&dlc->rd_timeout, rfcomm_dlc_timeout, dlc);

	*handle = dlc;
	return 0;
}

/*
 * rfcomm_bind_pcb(dlc, sockaddr)
 *
 * bind DLC to local address
 */
int
rfcomm_bind_pcb(struct rfcomm_dlc *dlc, struct sockaddr_bt *addr)
{

	if (dlc->rd_state != RFCOMM_DLC_CLOSED)
		return EINVAL;

	memcpy(&dlc->rd_laddr, addr, sizeof(struct sockaddr_bt));
	return 0;
}

/*
 * rfcomm_sockaddr_pcb(dlc, sockaddr)
 *
 * return local address
 */
int
rfcomm_sockaddr_pcb(struct rfcomm_dlc *dlc, struct sockaddr_bt *addr)
{

	memcpy(addr, &dlc->rd_laddr, sizeof(struct sockaddr_bt));
	return 0;
}

/*
 * rfcomm_connect_pcb(dlc, sockaddr)
 *
 * Initiate connection of RFCOMM DLC to remote address.
 */
int
rfcomm_connect_pcb(struct rfcomm_dlc *dlc, struct sockaddr_bt *dest)
{
	struct rfcomm_session *rs;
	int err = 0;

	if (dlc->rd_state != RFCOMM_DLC_CLOSED)
		return EISCONN;

	memcpy(&dlc->rd_raddr, dest, sizeof(struct sockaddr_bt));

	if (dlc->rd_raddr.bt_channel < RFCOMM_CHANNEL_MIN
	    || dlc->rd_raddr.bt_channel > RFCOMM_CHANNEL_MAX
	    || bdaddr_any(&dlc->rd_raddr.bt_bdaddr))
		return EDESTADDRREQ;

	if (dlc->rd_raddr.bt_psm == L2CAP_PSM_ANY)
		dlc->rd_raddr.bt_psm = L2CAP_PSM_RFCOMM;
	else if (dlc->rd_raddr.bt_psm != L2CAP_PSM_RFCOMM
	    && (dlc->rd_raddr.bt_psm < 0x1001
	    || L2CAP_PSM_INVALID(dlc->rd_raddr.bt_psm)))
		return EINVAL;

	/*
	 * We are allowed only one RFCOMM session between any 2 Bluetooth
	 * devices, so see if there is a session already otherwise create
	 * one and set it connecting.
	 */
	rs = rfcomm_session_lookup(&dlc->rd_laddr, &dlc->rd_raddr);
	if (rs == NULL) {
		rs = rfcomm_session_alloc(&rfcomm_session_active,
						&dlc->rd_laddr);
		if (rs == NULL)
			return ENOMEM;

		rs->rs_flags |= RFCOMM_SESSION_INITIATOR;
		rs->rs_state = RFCOMM_SESSION_WAIT_CONNECT;

		err = l2cap_connect_pcb(rs->rs_l2cap, &dlc->rd_raddr);
		if (err) {
			rfcomm_session_free(rs);
			return err;
		}

		/*
		 * This session will start up automatically when its
		 * L2CAP channel is connected.
		 */
	}

	/* construct DLC */
	dlc->rd_dlci = RFCOMM_MKDLCI(IS_INITIATOR(rs) ? 0:1, dest->bt_channel);
	if (rfcomm_dlc_lookup(rs, dlc->rd_dlci))
		return EBUSY;

	l2cap_sockaddr_pcb(rs->rs_l2cap, &dlc->rd_laddr);

	/*
	 * attach the DLC to the session and start it off
	 */
	dlc->rd_session = rs;
	dlc->rd_state = RFCOMM_DLC_WAIT_SESSION;
	LIST_INSERT_HEAD(&rs->rs_dlcs, dlc, rd_next);

	if (rs->rs_state == RFCOMM_SESSION_OPEN)
		err = rfcomm_dlc_connect(dlc);

	return err;
}

/*
 * rfcomm_peeraddr_pcb(dlc, sockaddr)
 *
 * return remote address
 */
int
rfcomm_peeraddr_pcb(struct rfcomm_dlc *dlc, struct sockaddr_bt *addr)
{

	memcpy(addr, &dlc->rd_raddr, sizeof(struct sockaddr_bt));
	return 0;
}

/*
 * rfcomm_disconnect_pcb(dlc, linger)
 *
 * disconnect RFCOMM DLC
 */
int
rfcomm_disconnect_pcb(struct rfcomm_dlc *dlc, int linger)
{
	struct rfcomm_session *rs = dlc->rd_session;
	int err = 0;

	KASSERT(dlc != NULL);

	switch (dlc->rd_state) {
	case RFCOMM_DLC_CLOSED:
	case RFCOMM_DLC_LISTEN:
		return EINVAL;

	case RFCOMM_DLC_WAIT_SEND_UA:
		err = rfcomm_session_send_frame(rs,
				RFCOMM_FRAME_DM, dlc->rd_dlci);

		/* fall through */
	case RFCOMM_DLC_WAIT_SESSION:
	case RFCOMM_DLC_WAIT_CONNECT:
	case RFCOMM_DLC_WAIT_SEND_SABM:
		rfcomm_dlc_close(dlc, 0);
		break;

	case RFCOMM_DLC_OPEN:
		if (dlc->rd_txbuf != NULL && linger != 0) {
			dlc->rd_flags |= RFCOMM_DLC_SHUTDOWN;
			break;
		}

		/* else fall through */
	case RFCOMM_DLC_WAIT_RECV_UA:
		dlc->rd_state = RFCOMM_DLC_WAIT_DISCONNECT;
		err = rfcomm_session_send_frame(rs, RFCOMM_FRAME_DISC,
							dlc->rd_dlci);
		callout_schedule(&dlc->rd_timeout, rfcomm_ack_timeout * hz);
		break;

	case RFCOMM_DLC_WAIT_DISCONNECT:
		err = EALREADY;
		break;

	default:
		UNKNOWN(dlc->rd_state);
		break;
	}

	return err;
}

/*
 * rfcomm_detach_pcb(handle)
 *
 * detach RFCOMM DLC from handle
 */
void
rfcomm_detach_pcb(struct rfcomm_dlc **handle)
{
	struct rfcomm_dlc *dlc = *handle;

	if (dlc->rd_state != RFCOMM_DLC_CLOSED)
		rfcomm_dlc_close(dlc, 0);

	if (dlc->rd_txbuf != NULL) {
		m_freem(dlc->rd_txbuf);
		dlc->rd_txbuf = NULL;
	}

	dlc->rd_upper = NULL;
	*handle = NULL;

	/*
	 * If callout is invoking we can't free the DLC so
	 * mark it and let the callout release it.
	 */
	if (callout_invoking(&dlc->rd_timeout))
		dlc->rd_flags |= RFCOMM_DLC_DETACH;
	else {
		callout_destroy(&dlc->rd_timeout);
		kmem_intr_free(dlc, sizeof(*dlc));
	}
}

/*
 * rfcomm_listen_pcb(dlc)
 *
 * This DLC is a listener. We look for an existing listening session
 * with a matching address to attach to or else create a new one on
 * the listeners list. If the ANY channel is given, allocate the first
 * available for the session.
 */
int
rfcomm_listen_pcb(struct rfcomm_dlc *dlc)
{
	struct rfcomm_session *rs;
	struct rfcomm_dlc *used;
	struct sockaddr_bt addr;
	int err, channel;

	if (dlc->rd_state != RFCOMM_DLC_CLOSED)
		return EISCONN;

	if (dlc->rd_laddr.bt_channel != RFCOMM_CHANNEL_ANY
	    && (dlc->rd_laddr.bt_channel < RFCOMM_CHANNEL_MIN
	    || dlc->rd_laddr.bt_channel > RFCOMM_CHANNEL_MAX))
		return EADDRNOTAVAIL;

	if (dlc->rd_laddr.bt_psm == L2CAP_PSM_ANY)
		dlc->rd_laddr.bt_psm = L2CAP_PSM_RFCOMM;
	else if (dlc->rd_laddr.bt_psm != L2CAP_PSM_RFCOMM
	    && (dlc->rd_laddr.bt_psm < 0x1001
	    || L2CAP_PSM_INVALID(dlc->rd_laddr.bt_psm)))
		return EADDRNOTAVAIL;

	LIST_FOREACH(rs, &rfcomm_session_listen, rs_next) {
		l2cap_sockaddr_pcb(rs->rs_l2cap, &addr);

		if (addr.bt_psm != dlc->rd_laddr.bt_psm)
			continue;

		if (bdaddr_same(&dlc->rd_laddr.bt_bdaddr, &addr.bt_bdaddr))
			break;
	}

	if (rs == NULL) {
		rs = rfcomm_session_alloc(&rfcomm_session_listen,
						&dlc->rd_laddr);
		if (rs == NULL)
			return ENOMEM;

		rs->rs_state = RFCOMM_SESSION_LISTEN;

		err = l2cap_listen_pcb(rs->rs_l2cap);
		if (err) {
			rfcomm_session_free(rs);
			return err;
		}
	}

	if (dlc->rd_laddr.bt_channel == RFCOMM_CHANNEL_ANY) {
		channel = RFCOMM_CHANNEL_MIN;
		used = LIST_FIRST(&rs->rs_dlcs);

		while (used != NULL) {
			if (used->rd_laddr.bt_channel == channel) {
				if (channel++ == RFCOMM_CHANNEL_MAX)
					return EADDRNOTAVAIL;

				used = LIST_FIRST(&rs->rs_dlcs);
			} else {
				used = LIST_NEXT(used, rd_next);
			}
		}

		dlc->rd_laddr.bt_channel = channel;
	}

	dlc->rd_session = rs;
	dlc->rd_state = RFCOMM_DLC_LISTEN;
	LIST_INSERT_HEAD(&rs->rs_dlcs, dlc, rd_next);

	return 0;
}

/*
 * rfcomm_send_pcb(dlc, mbuf)
 *
 * Output data on DLC. This is streamed data, so we add it
 * to our buffer and start the DLC, which will assemble
 * packets and send them if it can.
 */
int
rfcomm_send_pcb(struct rfcomm_dlc *dlc, struct mbuf *m)
{

	if (dlc->rd_txbuf != NULL) {
		dlc->rd_txbuf->m_pkthdr.len += m->m_pkthdr.len;
		m_cat(dlc->rd_txbuf, m);
	} else {
		dlc->rd_txbuf = m;
	}

	if (dlc->rd_state == RFCOMM_DLC_OPEN)
		rfcomm_dlc_start(dlc);

	return 0;
}

/*
 * rfcomm_rcvd_pcb(dlc, space)
 *
 * Indicate space now available in receive buffer
 *
 * This should be used to give an initial value of the receive buffer
 * size when the DLC is attached and anytime data is cleared from the
 * buffer after that.
 */
int
rfcomm_rcvd_pcb(struct rfcomm_dlc *dlc, size_t space)
{

	KASSERT(dlc != NULL);

	dlc->rd_rxsize = space;

	/*
	 * if we are using credit based flow control, we may
	 * want to send some credits..
	 */
	if (dlc->rd_state == RFCOMM_DLC_OPEN
	    && (dlc->rd_session->rs_flags & RFCOMM_SESSION_CFC))
		rfcomm_dlc_start(dlc);

	return 0;
}

/*
 * rfcomm_setopt(dlc, sopt)
 *
 * set DLC options
 */
int
rfcomm_setopt(struct rfcomm_dlc *dlc, const struct sockopt *sopt)
{
	int mode, err = 0;
	uint16_t mtu;

	switch (sopt->sopt_name) {
	case SO_RFCOMM_MTU:
		err = sockopt_get(sopt, &mtu, sizeof(mtu));
		if (err)
			break;

		if (mtu < RFCOMM_MTU_MIN || mtu > RFCOMM_MTU_MAX)
			err = EINVAL;
		else if (dlc->rd_state == RFCOMM_DLC_CLOSED)
			dlc->rd_mtu = mtu;
		else
			err = EBUSY;

		break;

	case SO_RFCOMM_LM:
		err = sockopt_getint(sopt, &mode);
		if (err)
			break;

		mode &= (RFCOMM_LM_SECURE | RFCOMM_LM_ENCRYPT | RFCOMM_LM_AUTH);

		if (mode & RFCOMM_LM_SECURE)
			mode |= RFCOMM_LM_ENCRYPT;

		if (mode & RFCOMM_LM_ENCRYPT)
			mode |= RFCOMM_LM_AUTH;

		dlc->rd_mode = mode;

		if (dlc->rd_state == RFCOMM_DLC_OPEN)
			err = rfcomm_dlc_setmode(dlc);

		break;

	default:
		err = ENOPROTOOPT;
		break;
	}
	return err;
}

/*
 * rfcomm_getopt(dlc, sopt)
 *
 * get DLC options
 */
int
rfcomm_getopt(struct rfcomm_dlc *dlc, struct sockopt *sopt)
{
	struct rfcomm_fc_info fc;

	switch (sopt->sopt_name) {
	case SO_RFCOMM_MTU:
		return sockopt_set(sopt, &dlc->rd_mtu, sizeof(uint16_t));

	case SO_RFCOMM_FC_INFO:
		memset(&fc, 0, sizeof(fc));
		fc.lmodem = dlc->rd_lmodem;
		fc.rmodem = dlc->rd_rmodem;
		fc.tx_cred = max(dlc->rd_txcred, 0xff);
		fc.rx_cred = max(dlc->rd_rxcred, 0xff);
		if (dlc->rd_session
		    && (dlc->rd_session->rs_flags & RFCOMM_SESSION_CFC))
			fc.cfc = 1;

		return sockopt_set(sopt, &fc, sizeof(fc));

	case SO_RFCOMM_LM:
		return sockopt_setint(sopt, dlc->rd_mode);

	default:
		break;
	}

	return ENOPROTOOPT;
}
