/*	$NetBSD: sco_upper.c,v 1.16 2014/08/05 07:55:32 rtr Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: sco_upper.c,v 1.16 2014/08/05 07:55:32 rtr Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/socketvar.h>
#include <sys/systm.h>

#include <netbt/bluetooth.h>
#include <netbt/hci.h>
#include <netbt/sco.h>

/****************************************************************************
 *
 *	SCO - Upper Protocol API
 */

struct sco_pcb_list sco_pcb = LIST_HEAD_INITIALIZER(sco_pcb);

/*
 * sco_attach_pcb(handle, proto, upper)
 *
 *	Attach a new instance of SCO pcb to handle
 */
int
sco_attach_pcb(struct sco_pcb **handle,
		const struct btproto *proto, void *upper)
{
	struct sco_pcb *pcb;

	KASSERT(handle != NULL);
	KASSERT(proto != NULL);
	KASSERT(upper != NULL);

	pcb = malloc(sizeof(struct sco_pcb), M_BLUETOOTH,
			M_NOWAIT | M_ZERO);
	if (pcb == NULL)
		return ENOMEM;

	pcb->sp_proto = proto;
	pcb->sp_upper = upper;

	LIST_INSERT_HEAD(&sco_pcb, pcb, sp_next);

	*handle = pcb;
	return 0;
}

/*
 * sco_bind_pcb(pcb, sockaddr)
 *
 *	Bind SCO pcb to local address
 */
int
sco_bind_pcb(struct sco_pcb *pcb, struct sockaddr_bt *addr)
{

	if (pcb->sp_link != NULL || pcb->sp_flags & SP_LISTENING)
		return EINVAL;

	bdaddr_copy(&pcb->sp_laddr, &addr->bt_bdaddr);
	return 0;
}

/*
 * sco_sockaddr_pcb(pcb, sockaddr)
 *
 *	Copy local address of PCB to sockaddr
 */
int
sco_sockaddr_pcb(struct sco_pcb *pcb, struct sockaddr_bt *addr)
{

	memset(addr, 0, sizeof(struct sockaddr_bt));
	addr->bt_len = sizeof(struct sockaddr_bt);
	addr->bt_family = AF_BLUETOOTH;
	bdaddr_copy(&addr->bt_bdaddr, &pcb->sp_laddr);
	return 0;
}

/*
 * sco_connect_pcb(pcb, sockaddr)
 *
 *	Initiate a SCO connection to the destination address.
 */
int
sco_connect_pcb(struct sco_pcb *pcb, struct sockaddr_bt *dest)
{
	hci_add_sco_con_cp cp;
	struct hci_unit *unit;
	struct hci_link *acl, *sco;
	int err;

	if (pcb->sp_flags & SP_LISTENING)
		return EINVAL;

	bdaddr_copy(&pcb->sp_raddr, &dest->bt_bdaddr);

	if (bdaddr_any(&pcb->sp_raddr))
		return EDESTADDRREQ;

	if (bdaddr_any(&pcb->sp_laddr)) {
		err = hci_route_lookup(&pcb->sp_laddr, &pcb->sp_raddr);
		if (err)
			return err;
	}

	unit = hci_unit_lookup(&pcb->sp_laddr);
	if (unit == NULL)
		return ENETDOWN;

	/*
	 * We must have an already open ACL connection before we open the SCO
	 * connection, and since SCO connections dont happen on their own we
	 * will not open one, the application wanting this should have opened
	 * it previously.
	 */
	acl = hci_link_lookup_bdaddr(unit, &pcb->sp_raddr, HCI_LINK_ACL);
	if (acl == NULL || acl->hl_state != HCI_LINK_OPEN)
		return EHOSTUNREACH;

	sco = hci_link_alloc(unit, &pcb->sp_raddr, HCI_LINK_SCO);
	if (sco == NULL)
		return ENOMEM;

	sco->hl_link = hci_acl_open(unit, &pcb->sp_raddr);
	KASSERT(sco->hl_link == acl);

	cp.con_handle = htole16(acl->hl_handle);
	cp.pkt_type = htole16(0x00e0);		/* HV1, HV2, HV3 */
	err = hci_send_cmd(unit, HCI_CMD_ADD_SCO_CON, &cp, sizeof(cp));
	if (err) {
		hci_link_free(sco, err);
		return err;
	}

	sco->hl_sco = pcb;
	pcb->sp_link = sco;

	pcb->sp_mtu = unit->hci_max_sco_size;
	return 0;
}

/*
 * sco_peeraddr_pcb(pcb, sockaddr)
 *
 *	Copy remote address of SCO pcb to sockaddr
 */
int
sco_peeraddr_pcb(struct sco_pcb *pcb, struct sockaddr_bt *addr)
{

	memset(addr, 0, sizeof(struct sockaddr_bt));
	addr->bt_len = sizeof(struct sockaddr_bt);
	addr->bt_family = AF_BLUETOOTH;
	bdaddr_copy(&addr->bt_bdaddr, &pcb->sp_raddr);
	return 0;
}

/*
 * sco_disconnect_pcb(pcb, linger)
 *
 *	Initiate disconnection of connected SCO pcb
 */
int
sco_disconnect_pcb(struct sco_pcb *pcb, int linger)
{
	hci_discon_cp cp;
	struct hci_link *sco;
	int err;

	sco = pcb->sp_link;
	if (sco == NULL)
		return EINVAL;

	cp.con_handle = htole16(sco->hl_handle);
	cp.reason = 0x13;	/* "Remote User Terminated Connection" */

	err = hci_send_cmd(sco->hl_unit, HCI_CMD_DISCONNECT, &cp, sizeof(cp));
	if (err || linger == 0) {
		sco->hl_sco = NULL;
		pcb->sp_link = NULL;
		hci_link_free(sco, err);
	}

	return err;
}

/*
 * sco_detach_pcb(handle)
 *
 *	Detach SCO pcb from handle and clear up
 */
void
sco_detach_pcb(struct sco_pcb **handle)
{
	struct sco_pcb *pcb;

	KASSERT(handle != NULL);
	pcb = *handle;
	*handle = NULL;

	if (pcb->sp_link != NULL) {
		sco_disconnect_pcb(pcb, 0);
		pcb->sp_link = NULL;
	}

	LIST_REMOVE(pcb, sp_next);
	free(pcb, M_BLUETOOTH);
}

/*
 * sco_listen_pcb(pcb)
 *
 *	Mark pcb as a listener.
 */
int
sco_listen_pcb(struct sco_pcb *pcb)
{

	if (pcb->sp_link != NULL)
		return EINVAL;

	pcb->sp_flags |= SP_LISTENING;
	return 0;
}

/*
 * sco_send_pcb(pcb, mbuf)
 *
 *	Send data on SCO pcb.
 *
 * Gross hackage, we just output the packet directly onto the unit queue.
 * This will work fine for one channel per unit, but for more channels it
 * really needs fixing. We set the context so that when the packet is sent,
 * we can drop a record from the socket buffer.
 */
int
sco_send_pcb(struct sco_pcb *pcb, struct mbuf *m)
{
	hci_scodata_hdr_t *hdr;
	int plen;

	if (pcb->sp_link == NULL) {
		m_freem(m);
		return EINVAL;
	}

	plen = m->m_pkthdr.len;
	DPRINTFN(10, "%d bytes\n", plen);

	/*
	 * This is a temporary limitation, as USB devices cannot
	 * handle SCO packet sizes that are not an integer number
	 * of Isochronous frames. See ubt(4)
	 */
	if (plen != pcb->sp_mtu) {
		m_freem(m);
		return EMSGSIZE;
	}

	M_PREPEND(m, sizeof(hci_scodata_hdr_t), M_DONTWAIT);
	if (m == NULL)
		return ENOMEM;

	hdr = mtod(m, hci_scodata_hdr_t *);
	hdr->type = HCI_SCO_DATA_PKT;
	hdr->con_handle = htole16(pcb->sp_link->hl_handle);
	hdr->length = plen;

	pcb->sp_pending++;
	M_SETCTX(m, pcb->sp_link);
	hci_output_sco(pcb->sp_link->hl_unit, m);

	return 0;
}

/*
 * sco_setopt(pcb, sopt)
 *
 *	Set SCO pcb options
 */
int
sco_setopt(struct sco_pcb *pcb, const struct sockopt *sopt)
{
	int err = 0;

	switch (sopt->sopt_name) {
	default:
		err = ENOPROTOOPT;
		break;
	}

	return err;
}

/*
 * sco_getopt(pcb, sopt)
 *
 *	Get SCO pcb options
 */
int
sco_getopt(struct sco_pcb *pcb, struct sockopt *sopt)
{

	switch (sopt->sopt_name) {
	case SO_SCO_MTU:
		return sockopt_set(sopt, &pcb->sp_mtu, sizeof(uint16_t));

	case SO_SCO_HANDLE:
		if (pcb->sp_link)
			return sockopt_set(sopt,
			    &pcb->sp_link->hl_handle, sizeof(uint16_t));

		return ENOTCONN;

	default:
		break;
	}

	return ENOPROTOOPT;
}
