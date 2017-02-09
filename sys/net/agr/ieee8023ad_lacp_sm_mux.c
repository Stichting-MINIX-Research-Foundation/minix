/*	$NetBSD: ieee8023ad_lacp_sm_mux.c,v 1.4 2007/02/21 23:00:07 thorpej Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: ieee8023ad_lacp_sm_mux.c,v 1.4 2007/02/21 23:00:07 thorpej Exp $");

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

/* mux machine */

void
lacp_sm_mux(struct lacp_port *lp)
{
	enum lacp_mux_state new_state;
	bool p_sync =
		    (lp->lp_partner.lip_state & LACP_STATE_SYNC) != 0;
	bool p_collecting =
	    (lp->lp_partner.lip_state & LACP_STATE_COLLECTING) != 0;
	enum lacp_selected selected = lp->lp_selected;
	struct lacp_aggregator *la;

	/* LACP_DPRINTF((lp, "%s: state %d\n", __func__, lp->lp_mux_state)); */

re_eval:
	la = lp->lp_aggregator;
	KASSERT(lp->lp_mux_state == LACP_MUX_DETACHED || la != NULL);
	new_state = lp->lp_mux_state;
	switch (lp->lp_mux_state) {
	case LACP_MUX_DETACHED:
		if (selected != LACP_UNSELECTED) {
			new_state = LACP_MUX_WAITING;
		}
		break;
	case LACP_MUX_WAITING:
		KASSERT(la->la_pending > 0 ||
		    !LACP_TIMER_ISARMED(lp, LACP_TIMER_WAIT_WHILE));
		if (selected == LACP_SELECTED && la->la_pending == 0) {
			new_state = LACP_MUX_ATTACHED;
		} else if (selected == LACP_UNSELECTED) {
			new_state = LACP_MUX_DETACHED;
		}
		break;
	case LACP_MUX_ATTACHED:
		if (selected == LACP_SELECTED && p_sync) {
			new_state = LACP_MUX_COLLECTING;
		} else if (selected != LACP_SELECTED) {
			new_state = LACP_MUX_DETACHED;
		}
		break;
	case LACP_MUX_COLLECTING:
		if (selected == LACP_SELECTED && p_sync && p_collecting) {
			new_state = LACP_MUX_DISTRIBUTING;
		} else if (selected != LACP_SELECTED || !p_sync) {
			new_state = LACP_MUX_ATTACHED;
		}
		break;
	case LACP_MUX_DISTRIBUTING:
		if (selected != LACP_SELECTED || !p_sync || !p_collecting) {
			new_state = LACP_MUX_COLLECTING;
		}
		break;
	default:
		panic("%s: unknown state", __func__);
	}

	if (lp->lp_mux_state == new_state) {
		return;
	}

	switch (new_state) {
	case LACP_MUX_DETACHED:
		lp->lp_state &= ~LACP_STATE_SYNC;
		lacp_disable_distributing(lp);
		lacp_disable_collecting(lp);
		lacp_sm_assert_ntt(lp);
		/* cancel timer */
		if (LACP_TIMER_ISARMED(lp, LACP_TIMER_WAIT_WHILE)) {
			KASSERT(la->la_pending > 0);
			la->la_pending--;
		}
		LACP_TIMER_DISARM(lp, LACP_TIMER_WAIT_WHILE);
		lacp_unselect(lp);
		break;
	case LACP_MUX_WAITING:
		LACP_TIMER_ARM(lp, LACP_TIMER_WAIT_WHILE,
		    LACP_AGGREGATE_WAIT_TIME);
		la->la_pending++;
		break;
	case LACP_MUX_ATTACHED:
		lp->lp_state |= LACP_STATE_SYNC;
		lacp_disable_collecting(lp);
		lacp_sm_assert_ntt(lp);
		break;
	case LACP_MUX_COLLECTING:
		lacp_enable_collecting(lp);
		lp->lp_state |= LACP_STATE_COLLECTING;
		lacp_disable_distributing(lp);
		lacp_sm_assert_ntt(lp);
		break;
	case LACP_MUX_DISTRIBUTING:
		lacp_enable_distributing(lp);
		break;
	default:
		panic("%s: unknown state", __func__);
	}

	LACP_DPRINTF((lp, "mux_state %d -> %d\n", lp->lp_mux_state, new_state));

	lp->lp_mux_state = new_state;
	goto re_eval;
}

void
lacp_sm_mux_timer(struct lacp_port *lp)
{
	struct lacp_aggregator *la = lp->lp_aggregator;
#if defined(LACP_DEBUG)
	char buf[LACP_LAGIDSTR_MAX+1];
#endif

	KASSERT(la);
	KASSERT(la->la_pending > 0);

	LACP_DPRINTF((lp, "%s: aggregator %s, pending %d -> %d\n", __func__,
	    lacp_format_lagid(&la->la_actor, &la->la_partner,
	    buf, sizeof(buf)),
	    la->la_pending, la->la_pending - 1));

	la->la_pending--;
}
