/*	$NetBSD: ieee8023ad_lacp_impl.h,v 1.5 2007/02/21 23:00:06 thorpej Exp $	*/

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

#ifndef _NET_AGR_IEEE8023AD_LACP_IMPL_H_
#define	_NET_AGR_IEEE8023AD_LACP_IMPL_H_

/*
 * IEEE802.3ad LACP
 *
 * implementation details.
 */

#include <sys/queue.h>

#define	LACP_TIMER_CURRENT_WHILE	0
#define	LACP_TIMER_PERIODIC		1
#define	LACP_TIMER_WAIT_WHILE		2
#define	LACP_NTIMER			3

#define	LACP_TIMER_ARM(port, timer, val) \
	(port)->lp_timer[(timer)] = (val)
#define	LACP_TIMER_DISARM(port, timer) \
	(port)->lp_timer[(timer)] = 0
#define	LACP_TIMER_ISARMED(port, timer) \
	((port)->lp_timer[(timer)] > 0)

struct lacp_aggregator {
	TAILQ_ENTRY(lacp_aggregator) la_q;
	int la_refcnt; /* number of ports which selected us */
	int la_nports; /* number of distributing ports  */
	TAILQ_HEAD(, lacp_port) la_ports; /* distributing ports */
	struct lacp_peerinfo la_partner;
	struct lacp_peerinfo la_actor;
	int la_pending; /* number of ports which is waiting wait_while */
};

struct lacp_softc {
	struct lacp_aggregator *lsc_active_aggregator;
	TAILQ_HEAD(, lacp_aggregator) lsc_aggregators;
	bool lsc_suppress_distributing;
	struct callout lsc_transit_callout;
};

#define	LACP_TRANSIT_DELAY	1000	/* in msec */

enum lacp_selected {
	LACP_UNSELECTED,
	LACP_STANDBY,	/* not used in this implementation */
	LACP_SELECTED,
};

enum lacp_mux_state {
	LACP_MUX_DETACHED,
	LACP_MUX_WAITING,
	LACP_MUX_ATTACHED,
	LACP_MUX_COLLECTING,
	LACP_MUX_DISTRIBUTING,
};

struct lacp_port {
	TAILQ_ENTRY(lacp_port) lp_dist_q;
	struct agr_port *lp_agrport;
	struct lacp_peerinfo lp_partner;
	struct lacp_peerinfo lp_actor;
#define	lp_state	lp_actor.lip_state
#define	lp_key		lp_actor.lip_key
	int lp_last_lacpdu_sent;
	enum lacp_mux_state lp_mux_state;
	enum lacp_selected lp_selected;
	int lp_flags;
	u_int lp_media; /* XXX redundant */
	int lp_timer[LACP_NTIMER];

	struct lacp_aggregator *lp_aggregator;
};

#define	LACPPORT_NTT		1	/* need to transmit */

#define	LACP_SOFTC(sc) \
	(&IEEE8023AD_SOFTC(sc)->isc_lacpsc)
#define	LACP_PORT(port) \
	(&IEEE8023AD_PORT(port)->iport_lacpport)

void lacp_run_timers(struct lacp_port *);
void lacp_portinit(struct agr_port *);
void lacp_portfini(struct agr_port *);
int lacp_compare_peerinfo(const struct lacp_peerinfo *,
    const struct lacp_peerinfo *);
int lacp_compare_systemid(const struct lacp_systemid *,
    const struct lacp_systemid *);
int lacp_compare_portid(const struct lacp_portid *,
    const struct lacp_portid *);

void lacp_select(struct lacp_port *);
void lacp_unselect(struct lacp_port *);

void lacp_disable_collecting(struct lacp_port *);
void lacp_enable_collecting(struct lacp_port *);
void lacp_disable_distributing(struct lacp_port *);
void lacp_enable_distributing(struct lacp_port *);

int lacp_xmit_lacpdu(struct lacp_port *);

#endif /* !_NET_AGR_IEEE8023AD_LACP_IMPL_H_ */
