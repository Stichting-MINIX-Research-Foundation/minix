/*	$NetBSD: ieee8023ad_impl.h,v 1.2 2005/12/10 23:21:39 elad Exp $	*/

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

#ifndef _NET_AGR_IEEE8023AD_INT_H_
#define	_NET_AGR_IEEE8023AD_INT_H_

struct ieee8023ad_softc {
	struct lacp_softc isc_lacpsc;
	struct callout isc_callout;
};

struct ieee8023ad_port {
	struct lacp_port iport_lacpport;
};

#define	IEEE8023AD_SOFTC(sc)	\
	((struct ieee8023ad_softc *)(sc)->sc_iftprivate)
#define	IEEE8023AD_PORT(port)	\
	((struct ieee8023ad_port *)(port)->port_iftprivate)

struct agr_softc;

void ieee8023ad_ctor(struct agr_softc *);
void ieee8023ad_dtor(struct agr_softc *);
void ieee8023ad_portinit(struct agr_port *);
void ieee8023ad_portfini(struct agr_port *);
struct agr_port *ieee8023ad_select_tx_port(struct agr_softc *, struct mbuf *);
void ieee8023ad_lacp_porttick(struct agr_softc *, struct agr_port *);
void ieee8023ad_lacp_portstate(struct agr_port *);

#endif /* !_NET_AGR_IEEE8023AD_INT_H_ */
