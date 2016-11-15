/*	$NetBSD: ieee8023ad_lacp_debug.c,v 1.6 2011/07/17 20:54:52 joerg Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: ieee8023ad_lacp_debug.c,v 1.6 2011/07/17 20:54:52 joerg Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>

#include <net/if.h>
#include <net/if_ether.h>

#include <net/agr/ieee8023_slowprotocols.h>
#include <net/agr/ieee8023_tlv.h>
#include <net/agr/ieee8023ad_lacp.h>
#include <net/agr/ieee8023ad_lacp_impl.h>
#include <net/agr/ieee8023ad_lacp_debug.h>

#if defined(LACP_DEBUG)
int lacpdebug = 0;
#endif /* defined(LACP_DEBUG) */

const char *
lacp_format_mac(const uint8_t *mac, char *buf, size_t buflen)
{

	snprintf(buf, buflen, "%02X-%02X-%02X-%02X-%02X-%02X",
	    (int)mac[0],
	    (int)mac[1],
	    (int)mac[2],
	    (int)mac[3],
	    (int)mac[4],
	    (int)mac[5]);

	return buf;
}

const char *
lacp_format_systemid(const struct lacp_systemid *sysid,
    char *buf, size_t buflen)
{
	char macbuf[LACP_MACSTR_MAX+1];

	snprintf(buf, buflen, "%04X,%s",
	    be16toh(sysid->lsi_prio),
	    lacp_format_mac(sysid->lsi_mac, macbuf, sizeof(macbuf)));

	return buf;
}

const char *
lacp_format_portid(const struct lacp_portid *portid, char *buf, size_t buflen)
{

	snprintf(buf, buflen, "%04X,%04X",
	    be16toh(portid->lpi_prio),
	    be16toh(portid->lpi_portno));

	return buf;
}

const char *
lacp_format_partner(const struct lacp_peerinfo *peer, char *buf, size_t buflen)
{
	char sysid[LACP_SYSTEMIDSTR_MAX+1];
	char portid[LACP_PORTIDSTR_MAX+1];

	snprintf(buf, buflen, "(%s,%04X,%s)",
	    lacp_format_systemid(&peer->lip_systemid, sysid, sizeof(sysid)),
	    be16toh(peer->lip_key),
	    lacp_format_portid(&peer->lip_portid, portid, sizeof(portid)));

	return buf;
}

const char *
lacp_format_lagid(const struct lacp_peerinfo *a,
    const struct lacp_peerinfo *b, char *buf, size_t buflen)
{
	char astr[LACP_PARTNERSTR_MAX+1];
	char bstr[LACP_PARTNERSTR_MAX+1];

#if 0
	/*
	 * there's a convention to display small numbered peer
	 * in the left.
	 */

	if (lacp_compare_peerinfo(a, b) > 0) {
		const struct lacp_peerinfo *t;

		t = a;
		a = b;
		b = t;
	}
#endif

	snprintf(buf, buflen, "[%s,%s]",
	    lacp_format_partner(a, astr, sizeof(astr)),
	    lacp_format_partner(b, bstr, sizeof(bstr)));

	return buf;
}

const char *
lacp_format_lagid_aggregator(const struct lacp_aggregator *la,
    char *buf, size_t buflen)
{

	if (la == NULL) {
		return "(none)";
	}

	return lacp_format_lagid(&la->la_actor, &la->la_partner, buf, buflen);
}

const char *
lacp_format_state(uint8_t state, char *buf, size_t buflen)
{
	static const char lacp_state_bits[] = LACP_STATE_BITS;

	snprintb(buf, buflen, lacp_state_bits, state);

	return buf;
}

void
lacp_dump_lacpdu(const struct lacpdu *du)
{
	char buf[LACP_PARTNERSTR_MAX+1];
	char buf2[LACP_STATESTR_MAX+1];

	printf("actor=%s\n",
	    lacp_format_partner(&du->ldu_actor, buf, sizeof(buf)));
	printf("actor.state=%s\n", 
	    lacp_format_state(du->ldu_actor.lip_state, buf2, sizeof(buf2)));
	printf("partner=%s\n",
	    lacp_format_partner(&du->ldu_partner, buf, sizeof(buf)));
	printf("partner.state=%s\n", 
	    lacp_format_state(du->ldu_partner.lip_state, buf2, sizeof(buf2)));

	printf("maxdelay=%d\n", be16toh(du->ldu_collector.lci_maxdelay));
}

#if defined(LACP_DEBUG)
#include <net/agr/if_agrvar_impl.h>

void
lacp_dprintf(const struct lacp_port *lp, const char *fmt, ...)
{
	va_list va;

	if (lacpdebug == 0) {
		return;
	}

	if (lp) {
		printf("%s: ", lp->lp_agrport->port_ifp->if_xname);
	}

	va_start(va, fmt);
	vprintf(fmt, va);
	va_end(va);
}
#endif
