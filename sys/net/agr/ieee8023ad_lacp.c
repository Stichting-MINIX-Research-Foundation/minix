/*	$NetBSD: ieee8023ad_lacp.c,v 1.10 2011/07/01 02:46:24 joerg Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: ieee8023ad_lacp.c,v 1.10 2011/07/01 02:46:24 joerg Exp $");

#include <sys/param.h>
#include <sys/callout.h>
#include <sys/mbuf.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h> /* hz */

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <net/agr/if_agrvar_impl.h>
#include <net/agr/if_agrsubr.h>
#include <net/agr/ieee8023_slowprotocols.h>
#include <net/agr/ieee8023_tlv.h>
#include <net/agr/ieee8023ad.h>
#include <net/agr/ieee8023ad_lacp.h>
#include <net/agr/ieee8023ad_lacp_impl.h>
#include <net/agr/ieee8023ad_impl.h>
#include <net/agr/ieee8023ad_lacp_sm.h>
#include <net/agr/ieee8023ad_lacp_debug.h>

static void lacp_fill_actorinfo(struct agr_port *, struct lacp_peerinfo *);

static uint64_t lacp_aggregator_bandwidth(struct lacp_aggregator *);
static void lacp_suppress_distributing(struct lacp_softc *,
    struct lacp_aggregator *);
static void lacp_transit_expire(void *);
static void lacp_select_active_aggregator(struct lacp_softc *);
static uint16_t lacp_compose_key(struct lacp_port *);

/*
 * actor system priority and port priority.
 * XXX should be configurable.
 */

#define	LACP_SYSTEM_PRIO	0x8000
#define	LACP_PORT_PRIO		0x8000

static const struct tlv_template lacp_info_tlv_template[] = {
	{ LACP_TYPE_ACTORINFO,
	    sizeof(struct tlvhdr) + sizeof(struct lacp_peerinfo) },
	{ LACP_TYPE_PARTNERINFO,
	    sizeof(struct tlvhdr) + sizeof(struct lacp_peerinfo) },
	{ LACP_TYPE_COLLECTORINFO,
	    sizeof(struct tlvhdr) + sizeof(struct lacp_collectorinfo) },
	{ 0, 0 },
};

/*
 * ieee8023ad_lacp_input: process lacpdu
 *
 * => called from ether_input.  (ie. at IPL_NET)
 *
 * XXX is it better to defer processing to lower IPL?
 * XXX anyway input rate should be very low...
 */

int
ieee8023ad_lacp_input(struct ifnet *ifp, struct mbuf *m)
{
	struct lacpdu *du;
	struct agr_softc *sc;
	struct agr_port *port;
	struct lacp_port *lp;
	int error = 0;

	port = ifp->if_agrprivate; /* XXX race with agr_remport. */
	if (__predict_false(port->port_flags & AGRPORT_DETACHING)) {
		goto bad;
	}

	sc = AGR_SC_FROM_PORT(port);
	KASSERT(port);

	/* running static config? */
	if (AGR_STATIC(sc)) {
		/* static config, no lacp */
		goto bad;
	}


	if (m->m_pkthdr.len != sizeof(*du)) {
		goto bad;
	}

	if ((m->m_flags & M_MCAST) == 0) {
		goto bad;
	}

	if (m->m_len < sizeof(*du)) {
		m = m_pullup(m, sizeof(*du));
		if (m == NULL) {
			return ENOMEM;
		}
	}

	du = mtod(m, struct lacpdu *);

	if (memcmp(&du->ldu_eh.ether_dhost,
	    &ethermulticastaddr_slowprotocols, ETHER_ADDR_LEN)) {
		goto bad;
	}

	KASSERT(du->ldu_sph.sph_subtype == SLOWPROTOCOLS_SUBTYPE_LACP);

	/*
	 * ignore the version for compatibility with
	 * the future protocol revisions.
	 */

#if 0
	if (du->ldu_sph.sph_version != 1) {
		goto bad;
	}
#endif

	/*
	 * ignore tlv types for compatibility with
	 * the future protocol revisions.
	 */

	if (tlv_check(du, sizeof(*du), &du->ldu_tlv_actor,
	    lacp_info_tlv_template, false)) {
		goto bad;
	}

	AGR_LOCK(sc);
	lp = LACP_PORT(port);

#if defined(LACP_DEBUG)
	if (lacpdebug) {
		LACP_DPRINTF((lp, "lacpdu receive\n"));
		lacp_dump_lacpdu(du);
	}
#endif /* defined(LACP_DEBUG) */
	lacp_sm_rx(lp, du);

	AGR_UNLOCK(sc);

	m_freem(m);

	return error;

bad:
	m_freem(m);
	return EINVAL;
}

static void
lacp_fill_actorinfo(struct agr_port *port, struct lacp_peerinfo *info)
{
	struct lacp_port *lp = LACP_PORT(port);

	info->lip_systemid.lsi_prio = htobe16(LACP_SYSTEM_PRIO);
	memcpy(&info->lip_systemid.lsi_mac,
	    CLLADDR(port->port_ifp->if_sadl), ETHER_ADDR_LEN);
	info->lip_portid.lpi_prio = htobe16(LACP_PORT_PRIO);
	info->lip_portid.lpi_portno = htobe16(port->port_ifp->if_index);
	info->lip_state = lp->lp_state;
}

int
lacp_xmit_lacpdu(struct lacp_port *lp)
{
	struct agr_port *port = lp->lp_agrport;
	struct mbuf *m;
	struct lacpdu *du;
	int error;

	/* running static config? */
	if (AGR_STATIC(AGR_SC_FROM_PORT(port))) {
		/* static config, no lacp transmit */
		return 0;
	}

	KDASSERT(MHLEN >= sizeof(*du));

	m = m_gethdr(M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		return ENOMEM;
	}
	m->m_len = m->m_pkthdr.len = sizeof(*du);

	du = mtod(m, struct lacpdu *);
	memset(du, 0, sizeof(*du));

	memcpy(&du->ldu_eh.ether_dhost, ethermulticastaddr_slowprotocols,
	    ETHER_ADDR_LEN);
	memcpy(&du->ldu_eh.ether_shost, &port->port_origlladdr, ETHER_ADDR_LEN);
	du->ldu_eh.ether_type = htobe16(ETHERTYPE_SLOWPROTOCOLS);

	du->ldu_sph.sph_subtype = SLOWPROTOCOLS_SUBTYPE_LACP;
	du->ldu_sph.sph_version = 1;

	TLV_SET(&du->ldu_tlv_actor, LACP_TYPE_ACTORINFO, sizeof(du->ldu_actor));
	du->ldu_actor = lp->lp_actor;

	TLV_SET(&du->ldu_tlv_partner, LACP_TYPE_PARTNERINFO,
	    sizeof(du->ldu_partner));
	du->ldu_partner = lp->lp_partner;

	TLV_SET(&du->ldu_tlv_collector, LACP_TYPE_COLLECTORINFO,
	    sizeof(du->ldu_collector));
	du->ldu_collector.lci_maxdelay = 0;

#if defined(LACP_DEBUG)
	if (lacpdebug) {
		LACP_DPRINTF((lp, "lacpdu transmit\n"));
		lacp_dump_lacpdu(du);
	}
#endif /* defined(LACP_DEBUG) */

	m->m_flags |= M_MCAST;

	/*
	 * XXX should use higher priority queue.
	 * otherwise network congestion can break aggregation.
	 */

	error = agr_xmit_frame(port->port_ifp, m);
	return error;
}

void
ieee8023ad_lacp_portstate(struct agr_port *port)
{
	struct lacp_port *lp = LACP_PORT(port);
	u_int media = port->port_media;
	uint8_t old_state;
	uint16_t old_key;

	AGR_ASSERT_LOCKED(AGR_SC_FROM_PORT(port));

	LACP_DPRINTF((lp, "media changed 0x%x -> 0x%x\n", lp->lp_media, media));

	old_state = lp->lp_state;
	old_key = lp->lp_key;

	lp->lp_media = media;
	if ((media & IFM_HDX) != 0) {
		lp->lp_state &= ~LACP_STATE_AGGREGATION;
	} else {
		lp->lp_state |= LACP_STATE_AGGREGATION;
	}
	lp->lp_key = lacp_compose_key(lp);

	if (old_state != lp->lp_state || old_key != lp->lp_key) {
		LACP_DPRINTF((lp, "-> UNSELECTED\n"));
		lp->lp_selected = LACP_UNSELECTED;
	}
}

void
ieee8023ad_lacp_porttick(struct agr_softc *sc, struct agr_port *port)
{
	struct lacp_port *lp = LACP_PORT(port);

	AGR_ASSERT_LOCKED(sc);

	lacp_run_timers(lp);

	lacp_select(lp);
	lacp_sm_mux(lp);
	lacp_sm_tx(lp);
	lacp_sm_ptx_tx_schedule(lp);
}

void
lacp_portinit(struct agr_port *port)
{
	struct lacp_port *lp = LACP_PORT(port);
	bool active = true; /* XXX should be configurable */
	bool fast = false; /* XXX should be configurable */

	lp->lp_agrport = port;
	lacp_fill_actorinfo(port, &lp->lp_actor);
	lp->lp_state =
	    (active ? LACP_STATE_ACTIVITY : 0) |
	    (fast ? LACP_STATE_TIMEOUT : 0);
	lp->lp_aggregator = NULL;
	lp->lp_media = port->port_media; /* XXX */
	lp->lp_key = lacp_compose_key(lp);
	lacp_sm_rx_set_expired(lp);
}

void
lacp_portfini(struct agr_port *port)
{
	struct lacp_port *lp = LACP_PORT(port);
	struct lacp_aggregator *la = lp->lp_aggregator;
	int i;

	LACP_DPRINTF((lp, "portfini\n"));

	for (i = 0; i < LACP_NTIMER; i++) {
		LACP_TIMER_DISARM(lp, i);
	}

	if (la == NULL) {
		return;
	}

	lacp_disable_distributing(lp);
	lacp_unselect(lp);
}

/* -------------------- */
void
lacp_disable_collecting(struct lacp_port *lp)
{
	struct agr_port *port = lp->lp_agrport;

	lp->lp_state &= ~LACP_STATE_COLLECTING;
	port->port_flags &= ~AGRPORT_COLLECTING;
}

void
lacp_enable_collecting(struct lacp_port *lp)
{
	struct agr_port *port = lp->lp_agrport;

	lp->lp_state |= LACP_STATE_COLLECTING;
	port->port_flags |= AGRPORT_COLLECTING;
}

void
lacp_disable_distributing(struct lacp_port *lp)
{
	struct agr_port *port = lp->lp_agrport;
	struct lacp_aggregator *la = lp->lp_aggregator;
	struct lacp_softc *lsc = LACP_SOFTC(AGR_SC_FROM_PORT(port));
#if defined(LACP_DEBUG)
	char buf[LACP_LAGIDSTR_MAX+1];
#endif /* defined(LACP_DEBUG) */

	if ((lp->lp_state & LACP_STATE_DISTRIBUTING) == 0) {
		return;
	}

	KASSERT(la);
	KASSERT(!TAILQ_EMPTY(&la->la_ports));
	KASSERT(la->la_nports > 0);
	KASSERT(la->la_refcnt >= la->la_nports);

	LACP_DPRINTF((lp, "disable distributing on aggregator %s, "
	    "nports %d -> %d\n",
	    lacp_format_lagid_aggregator(la, buf, sizeof(buf)),
	    la->la_nports, la->la_nports - 1));

	TAILQ_REMOVE(&la->la_ports, lp, lp_dist_q);
	la->la_nports--;

	lacp_suppress_distributing(lsc, la);

	lp->lp_state &= ~LACP_STATE_DISTRIBUTING;
	port->port_flags &= ~AGRPORT_DISTRIBUTING;

	if (lsc->lsc_active_aggregator == la) {
		lacp_select_active_aggregator(lsc);
	}
}

void
lacp_enable_distributing(struct lacp_port *lp)
{
	struct agr_port *port = lp->lp_agrport;
	struct lacp_aggregator *la = lp->lp_aggregator;
	struct lacp_softc *lsc = LACP_SOFTC(AGR_SC_FROM_PORT(port));
#if defined(LACP_DEBUG)
	char buf[LACP_LAGIDSTR_MAX+1];
#endif /* defined(LACP_DEBUG) */

	if ((lp->lp_state & LACP_STATE_DISTRIBUTING) != 0) {
		return;
	}

	KASSERT(la);

	LACP_DPRINTF((lp, "enable distributing on aggregator %s, "
	    "nports %d -> %d\n",
	    lacp_format_lagid_aggregator(la, buf, sizeof(buf)),
	    la->la_nports, la->la_nports + 1));

	KASSERT(la->la_refcnt > la->la_nports);
	TAILQ_INSERT_HEAD(&la->la_ports, lp, lp_dist_q);
	la->la_nports++;

	lacp_suppress_distributing(lsc, la);

	lp->lp_state |= LACP_STATE_DISTRIBUTING;
	port->port_flags |= AGRPORT_DISTRIBUTING;

	if (lsc->lsc_active_aggregator != la) {
		lacp_select_active_aggregator(lsc);
	}
}

static void
lacp_transit_expire(void *vp)
{
	struct agr_softc *sc = vp;
	struct lacp_softc *lsc = LACP_SOFTC(sc);

	AGR_LOCK(sc);
	LACP_DPRINTF((NULL, "%s\n", __func__));
	lsc->lsc_suppress_distributing = false;
	AGR_UNLOCK(sc);
}

/* -------------------- */
/* XXX */
void
ieee8023ad_portinit(struct agr_port *port)
{
	struct ieee8023ad_port *iport = IEEE8023AD_PORT(port);

	memset(iport, 0, sizeof(*iport));

	lacp_portinit(port);
}

void
ieee8023ad_portfini(struct agr_port *port)
{
	struct agr_softc *sc = AGR_SC_FROM_PORT(port);

	AGR_LOCK(sc);

	lacp_portfini(port);

	AGR_UNLOCK(sc);
}

void
ieee8023ad_ctor(struct agr_softc *sc)
{
	struct ieee8023ad_softc *isc = IEEE8023AD_SOFTC(sc);
	struct lacp_softc *lsc = &isc->isc_lacpsc;

	lsc->lsc_active_aggregator = NULL;
	TAILQ_INIT(&lsc->lsc_aggregators);
	callout_init(&lsc->lsc_transit_callout, 0);
	callout_setfunc(&lsc->lsc_transit_callout, lacp_transit_expire, sc);
}

void
ieee8023ad_dtor(struct agr_softc *sc)
{
	struct ieee8023ad_softc *isc = IEEE8023AD_SOFTC(sc);
	struct lacp_softc *lsc = &isc->isc_lacpsc;

	LACP_DPRINTF((NULL, "%s\n", __func__));

	callout_stop(&lsc->lsc_transit_callout);
	KASSERT(TAILQ_EMPTY(&lsc->lsc_aggregators));
	KASSERT(lsc->lsc_active_aggregator == NULL);
}

/* -------------------- */

struct agr_port *
ieee8023ad_select_tx_port(struct agr_softc *sc, struct mbuf *m)
{
	const struct lacp_softc *lsc = LACP_SOFTC(sc);
	const struct lacp_aggregator *la;
	const struct lacp_port *lp;
	uint32_t hash;
	int nports;

	if (__predict_false(lsc->lsc_suppress_distributing &&
	    !AGR_ROUNDROBIN(sc))) {
		LACP_DPRINTF((NULL, "%s: waiting transit\n", __func__));
		sc->sc_if.if_collisions++; /* XXX abuse */
		return NULL;
	}

	la = lsc->lsc_active_aggregator;
	if (__predict_false(la == NULL)) {
		LACP_DPRINTF((NULL, "%s: no active aggregator\n", __func__));
		return NULL;
	}

	nports = la->la_nports;
	KASSERT(nports > 0);

	if (AGR_ROUNDROBIN(sc)) {
		/* packet ordering rule violation */
		hash = sc->sc_rr_counter++;
	} else {
		hash = (*sc->sc_iftop->iftop_hashmbuf)(sc, m);
	}
	hash %= nports;
	lp = TAILQ_FIRST(&la->la_ports);
	KASSERT(lp != NULL);
	while (hash--) {
		lp = TAILQ_NEXT(lp, lp_dist_q);
		KASSERT(lp != NULL);
	}

	KASSERT((lp->lp_state & LACP_STATE_DISTRIBUTING) != 0);

	return lp->lp_agrport;
}

/*
 * lacp_suppress_distributing: drop transmit packets for a while
 * to preserve packet ordering.
 */

static void
lacp_suppress_distributing(struct lacp_softc *lsc, struct lacp_aggregator *la)
{

	if (lsc->lsc_active_aggregator != la) {
		return;
	}

	LACP_DPRINTF((NULL, "%s\n", __func__));
	lsc->lsc_suppress_distributing = true;
	/* XXX should consider collector max delay */
	callout_schedule(&lsc->lsc_transit_callout,
	    LACP_TRANSIT_DELAY * hz / 1000);
}

/* -------------------- */

int
lacp_compare_peerinfo(const struct lacp_peerinfo *a,
    const struct lacp_peerinfo *b)
{

	return memcmp(a, b, offsetof(struct lacp_peerinfo, lip_state));
}

int
lacp_compare_systemid(const struct lacp_systemid *a,
    const struct lacp_systemid *b)
{

	return memcmp(a, b, sizeof(*a));
}

int
lacp_compare_portid(const struct lacp_portid *a,
    const struct lacp_portid *b)
{

	return memcmp(a, b, sizeof(*a));
}

/* -------------------- */

static uint64_t
lacp_aggregator_bandwidth(struct lacp_aggregator *la)
{
	struct lacp_port *lp;
	uint64_t speed;

	lp = TAILQ_FIRST(&la->la_ports);
	if (lp == NULL) {
		return 0;
	}

	speed = ifmedia_baudrate(lp->lp_media);
	speed *= la->la_nports;
	if (speed == 0) {
		LACP_DPRINTF((lp, "speed 0? media=0x%x nports=%d\n",
		    lp->lp_media, la->la_nports));
	}

	return speed;
}

/*
 * lacp_select_active_aggregator: select an aggregator to be used to transmit
 * packets from agr(4) interface.
 */

static void
lacp_select_active_aggregator(struct lacp_softc *lsc)
{
	struct lacp_aggregator *la;
	struct lacp_aggregator *best_la = NULL;
	uint64_t best_speed = 0;
#if defined(LACP_DEBUG)
	char buf[LACP_LAGIDSTR_MAX+1];
#endif /* defined(LACP_DEBUG) */

	LACP_DPRINTF((NULL, "%s:\n", __func__));

	TAILQ_FOREACH(la, &lsc->lsc_aggregators, la_q) {
		uint64_t speed;

		if (la->la_nports == 0) {
			continue;
		}

		speed = lacp_aggregator_bandwidth(la);
		LACP_DPRINTF((NULL, "%s, speed=%" PRIu64 ", nports=%d\n",
		    lacp_format_lagid_aggregator(la, buf, sizeof(buf)),
		    speed, la->la_nports));
		if (speed > best_speed ||
		    (speed == best_speed &&
		    la == lsc->lsc_active_aggregator)) {
			best_la = la;
			best_speed = speed;
		}
	}

	KASSERT(best_la == NULL || best_la->la_nports > 0);
	KASSERT(best_la == NULL || !TAILQ_EMPTY(&best_la->la_ports));

#if defined(LACP_DEBUG)
	if (lsc->lsc_active_aggregator != best_la) {
		LACP_DPRINTF((NULL, "active aggregator changed\n"));
		LACP_DPRINTF((NULL, "old %s\n",
		    lacp_format_lagid_aggregator(lsc->lsc_active_aggregator,
		    buf, sizeof(buf))));
	} else {
		LACP_DPRINTF((NULL, "active aggregator not changed\n"));
	}
	LACP_DPRINTF((NULL, "new %s\n",
	    lacp_format_lagid_aggregator(best_la, buf, sizeof(buf))));
#endif /* defined(LACP_DEBUG) */

	if (lsc->lsc_active_aggregator != best_la) {
		lsc->lsc_active_aggregator = best_la;
		if (best_la) {
			lacp_suppress_distributing(lsc, best_la);
		}
	}
}

uint16_t
lacp_compose_key(struct lacp_port *lp)
{
	u_int media = lp->lp_media;
	uint16_t key;

	KASSERT(IFM_TYPE(media) == IFM_ETHER);

	if (!(lp->lp_state & LACP_STATE_AGGREGATION)) {

		/*
		 * non-aggregatable links should have unique keys.
		 *
		 * XXX this isn't really unique as if_index is 16 bit.
		 */

		/* bit 0..14:	(some bits of) if_index of this port */
		key = lp->lp_agrport->port_ifp->if_index;
		/* bit 15:	1 */
		key |= 0x8000;
	} else {
		u_int subtype = IFM_SUBTYPE(media);

		KASSERT((media & IFM_HDX) == 0); /* should be handled above */
		KASSERT((subtype & 0x1f) == subtype);

		/* bit 0..4:	IFM_SUBTYPE */
		key = subtype;
		/* bit 5..14:	(some bits of) if_index of agr device */
		key |= 0x7fe0 & ((lp->lp_agrport->port_agrifp->if_index) << 5);
		/* bit 15:	0 */
	}

	return htobe16(key);
}
