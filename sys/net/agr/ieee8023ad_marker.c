/*	$NetBSD: ieee8023ad_marker.c,v 1.4 2007/02/22 06:20:16 thorpej Exp $	*/

/*-
 * Copyright (c)2005 YAMAMOTO Takashi,
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ieee8023ad_marker.c,v 1.4 2007/02/22 06:20:16 thorpej Exp $");

#include <sys/param.h>
#include <sys/callout.h>
#include <sys/mbuf.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/if_ether.h>

#include <net/agr/if_agrvar_impl.h>
#include <net/agr/ieee8023_slowprotocols.h>
#include <net/agr/ieee8023_tlv.h>
#include <net/agr/ieee8023ad.h>
#include <net/agr/ieee8023ad_marker.h>

static const struct tlv_template marker_info_tlv_template[] = {
	{ MARKER_TYPE_INFO, 16 },
	{ 0, 0 },
};

static const struct tlv_template marker_response_tlv_template[] = {
	{ MARKER_TYPE_RESPONSE, 16 },
	{ 0, 0 },
};

int
ieee8023ad_marker_input(struct ifnet *ifp, struct mbuf *m)
{
	struct markerdu *mdu;
	struct agr_port *port;
	int error = 0;

	port = ifp->if_agrprivate; /* XXX race with agr_remport. */
	KASSERT(port);
	if (__predict_false(port->port_flags & AGRPORT_DETACHING)) {
		goto bad;
	}

	if (m->m_pkthdr.len != sizeof(*mdu)) {
		goto bad;
	}

	if ((m->m_flags & M_MCAST) == 0) {
		goto bad;
	}

	if (m->m_len < sizeof(*mdu)) {
		m = m_pullup(m, sizeof(*mdu));
		if (m == NULL) {
			return ENOMEM;
		}
	}

	mdu = mtod(m, struct markerdu *);

	if (memcmp(&mdu->mdu_eh.ether_dhost,
	    &ethermulticastaddr_slowprotocols, ETHER_ADDR_LEN)) {
		goto bad;
	}

	KASSERT(mdu->mdu_sph.sph_subtype == SLOWPROTOCOLS_SUBTYPE_MARKER);
	if (mdu->mdu_sph.sph_version != 1) {
		goto bad;
	}

	switch (mdu->mdu_tlv.tlv_type) {
	case MARKER_TYPE_INFO:
		if (tlv_check(mdu, sizeof(*mdu), &mdu->mdu_tlv,
		    marker_info_tlv_template, true)) {
			goto bad;
		}
		mdu->mdu_tlv.tlv_type = MARKER_TYPE_RESPONSE;
		memcpy(&mdu->mdu_eh.ether_dhost,
		    &ethermulticastaddr_slowprotocols, ETHER_ADDR_LEN);
		memcpy(&mdu->mdu_eh.ether_shost,
		    &port->port_origlladdr, ETHER_ADDR_LEN);
		error = agr_xmit_frame(ifp, m);
		break;

	case MARKER_TYPE_RESPONSE:
		if (tlv_check(mdu, sizeof(*mdu), &mdu->mdu_tlv,
		    marker_response_tlv_template, true)) {
			goto bad;
		}
		/*
		 * we are not interested in responses as
		 * we don't have a marker sender.
		 */
		/* FALLTHROUGH */
	default:
		goto bad;
	}

	return error;

bad:
	m_freem(m);
	return EINVAL;
}
