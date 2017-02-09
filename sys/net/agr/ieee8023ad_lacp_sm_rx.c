/*	$NetBSD: ieee8023ad_lacp_sm_rx.c,v 1.4 2007/02/21 23:00:07 thorpej Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: ieee8023ad_lacp_sm_rx.c,v 1.4 2007/02/21 23:00:07 thorpej Exp $");

#include <sys/param.h>
#include <sys/callout.h>
#include <sys/mbuf.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/if_ether.h>

#include <net/agr/ieee8023_slowprotocols.h>
#include <net/agr/ieee8023_tlv.h>
#include <net/agr/ieee8023ad_lacp.h>
#include <net/agr/ieee8023ad_lacp_impl.h>
#include <net/agr/ieee8023ad_lacp_sm.h>
#include <net/agr/ieee8023ad_lacp_debug.h>

/* receive machine */

static void lacp_sm_rx_update_ntt(struct lacp_port *, const struct lacpdu *);
static void lacp_sm_rx_record_pdu(struct lacp_port *, const struct lacpdu *);
static void lacp_sm_rx_update_selected(struct lacp_port *, const struct lacpdu *);

static void lacp_sm_rx_record_default(struct lacp_port *);
static void lacp_sm_rx_update_default_selected(struct lacp_port *);

static void lacp_sm_rx_update_selected_from_peerinfo(struct lacp_port *,
    const struct lacp_peerinfo *);

/*
 * partner administration variables.
 * XXX should be configurable.
 */

static const struct lacp_peerinfo lacp_partner_admin = {
	.lip_systemid = { .lsi_prio = 0xffff },
	.lip_portid = { .lpi_prio = 0xffff },
#if 1
	/* optimistic */
	.lip_state = LACP_STATE_SYNC | LACP_STATE_AGGREGATION |
	    LACP_STATE_COLLECTING | LACP_STATE_DISTRIBUTING,
#else
	/* pessimistic */
	.lip_state = 0,
#endif
};

void
lacp_sm_rx(struct lacp_port *lp, const struct lacpdu *du)
{
	int timeout;

	/*
	 * check LACP_DISABLED first
	 */

	if (!(lp->lp_state & LACP_STATE_AGGREGATION)) {
		return;
	}

	/*
	 * check loopback condition.
	 */

	if (!lacp_compare_systemid(&du->ldu_actor.lip_systemid,
	    &lp->lp_actor.lip_systemid)) {
		return;
	}

	/*
	 * EXPIRED, DEFAULTED, CURRENT -> CURRENT
	 */

	lacp_sm_rx_update_selected(lp, du);
	lacp_sm_rx_update_ntt(lp, du);
	lacp_sm_rx_record_pdu(lp, du);

	timeout = (lp->lp_state & LACP_STATE_TIMEOUT) ?
	    LACP_SHORT_TIMEOUT_TIME : LACP_LONG_TIMEOUT_TIME;
	LACP_TIMER_ARM(lp, LACP_TIMER_CURRENT_WHILE, timeout);

	lp->lp_state &= ~LACP_STATE_EXPIRED;

	/*
	 * kick transmit machine without waiting the next tick.
	 */

	lacp_sm_tx(lp);
}

void
lacp_sm_rx_set_expired(struct lacp_port *lp)
{

	lp->lp_partner.lip_state &= ~LACP_STATE_SYNC;
	lp->lp_partner.lip_state |= LACP_STATE_TIMEOUT;
	LACP_TIMER_ARM(lp, LACP_TIMER_CURRENT_WHILE, LACP_SHORT_TIMEOUT_TIME);
	lp->lp_state |= LACP_STATE_EXPIRED;
}

void
lacp_sm_rx_timer(struct lacp_port *lp)
{

	if ((lp->lp_state & LACP_STATE_EXPIRED) == 0) {
		/* CURRENT -> EXPIRED */
		LACP_DPRINTF((lp, "%s: CURRENT -> EXPIRED\n", __func__));
		lacp_sm_rx_set_expired(lp);
	} else {
		/* EXPIRED -> DEFAULTED */
		LACP_DPRINTF((lp, "%s: EXPIRED -> DEFAULTED\n", __func__));
		lacp_sm_rx_update_default_selected(lp);
		lacp_sm_rx_record_default(lp);
		lp->lp_state &= ~LACP_STATE_EXPIRED;
	}
}

static void
lacp_sm_rx_record_pdu(struct lacp_port *lp, const struct lacpdu *du)
{
	bool active;
	uint8_t oldpstate;
#if defined(LACP_DEBUG)
	char buf[LACP_STATESTR_MAX+1];
#endif

	/* LACP_DPRINTF((lp, "%s\n", __func__)); */

	oldpstate = lp->lp_partner.lip_state;

	active = (du->ldu_actor.lip_state & LACP_STATE_ACTIVITY)
	    || ((lp->lp_state & LACP_STATE_ACTIVITY) &&
	    (du->ldu_partner.lip_state & LACP_STATE_ACTIVITY));

	lp->lp_partner = du->ldu_actor;
	if (active &&
	    ((LACP_STATE_EQ(lp->lp_state, du->ldu_partner.lip_state,
	    LACP_STATE_AGGREGATION) &&
	    !lacp_compare_peerinfo(&lp->lp_actor, &du->ldu_partner))
	    || (du->ldu_partner.lip_state & LACP_STATE_AGGREGATION) == 0)) {
		/* nothing */
	} else {
		lp->lp_partner.lip_state &= ~LACP_STATE_SYNC;
	}

	lp->lp_state &= ~LACP_STATE_DEFAULTED;

	LACP_DPRINTF((lp, "old pstate %s\n",
	    lacp_format_state(oldpstate, buf, sizeof(buf))));
	LACP_DPRINTF((lp, "new pstate %s\n",
	    lacp_format_state(lp->lp_partner.lip_state, buf, sizeof(buf))));

	lacp_sm_ptx_update_timeout(lp, oldpstate);
}

static void
lacp_sm_rx_update_ntt(struct lacp_port *lp, const struct lacpdu *du)
{

	/* LACP_DPRINTF((lp, "%s\n", __func__)); */

	if (lacp_compare_peerinfo(&lp->lp_actor, &du->ldu_partner) ||
	    !LACP_STATE_EQ(lp->lp_state, du->ldu_partner.lip_state,
	    LACP_STATE_ACTIVITY | LACP_STATE_SYNC | LACP_STATE_AGGREGATION)) {
		LACP_DPRINTF((lp, "%s: assert ntt\n", __func__));
		lacp_sm_assert_ntt(lp);
	}
}

static void
lacp_sm_rx_record_default(struct lacp_port *lp)
{
	uint8_t oldpstate;

	/* LACP_DPRINTF((lp, "%s\n", __func__)); */

	oldpstate = lp->lp_partner.lip_state;
	lp->lp_partner = lacp_partner_admin;
	lp->lp_state |= LACP_STATE_DEFAULTED;
	lacp_sm_ptx_update_timeout(lp, oldpstate);
}

static void
lacp_sm_rx_update_selected_from_peerinfo(struct lacp_port *lp,
    const struct lacp_peerinfo *info)
{

	/* LACP_DPRINTF((lp, "%s\n", __func__)); */

	if (lacp_compare_peerinfo(&lp->lp_partner, info) ||
	    !LACP_STATE_EQ(lp->lp_partner.lip_state, info->lip_state,
	    LACP_STATE_AGGREGATION)) {
		lp->lp_selected = LACP_UNSELECTED;
		/* mux machine will clean up lp->lp_aggregator */
	}
}

static void
lacp_sm_rx_update_selected(struct lacp_port *lp, const struct lacpdu *du)
{

	/* LACP_DPRINTF((lp, "%s\n", __func__)); */

	lacp_sm_rx_update_selected_from_peerinfo(lp, &du->ldu_actor);
}

static void
lacp_sm_rx_update_default_selected(struct lacp_port *lp)
{

	/* LACP_DPRINTF((lp, "%s\n", __func__)); */

	lacp_sm_rx_update_selected_from_peerinfo(lp, &lacp_partner_admin);
}
