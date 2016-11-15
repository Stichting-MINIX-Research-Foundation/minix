/*	$NetBSD: ieee8023ad_lacp_sm_tx.c,v 1.3 2005/12/11 12:24:54 christos Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: ieee8023ad_lacp_sm_tx.c,v 1.3 2005/12/11 12:24:54 christos Exp $");

#include <sys/param.h>
#include <sys/callout.h>
#include <sys/mbuf.h>
#include <sys/systm.h>
#include <sys/kernel.h>	/* hardclock_ticks */

#include <net/if.h>
#include <net/if_ether.h>

#include <net/agr/ieee8023_slowprotocols.h>
#include <net/agr/ieee8023_tlv.h>
#include <net/agr/ieee8023ad_lacp.h>
#include <net/agr/ieee8023ad_lacp_impl.h>
#include <net/agr/ieee8023ad_lacp_sm.h>
#include <net/agr/ieee8023ad_lacp_debug.h>

/* transmit machine */

void
lacp_sm_tx(struct lacp_port *lp)
{
	int error;
	int now;

	if (!(lp->lp_state & LACP_STATE_AGGREGATION)
#if 1
	    || (!(lp->lp_state & LACP_STATE_ACTIVITY)
	    && !(lp->lp_partner.lip_state & LACP_STATE_ACTIVITY))
#endif
	    ) {
		lp->lp_flags &= ~LACPPORT_NTT;
	}

	if (!(lp->lp_flags & LACPPORT_NTT)) {
		return;
	}

	/* rate limit */
	now = hardclock_ticks;
	if ((unsigned int)(now - lp->lp_last_lacpdu_sent) <=
	    LACP_FAST_PERIODIC_TIME * hz / 3) {
		return;
	}
	lp->lp_last_lacpdu_sent = now;

	error = lacp_xmit_lacpdu(lp);

	if (error == 0) {
		lp->lp_flags &= ~LACPPORT_NTT;
	} else {
		LACP_DPRINTF((lp, "lacpdu transmit failure, error %d\n",
		    error));
	}
}

void
lacp_sm_assert_ntt(struct lacp_port *lp)
{

	lp->lp_flags |= LACPPORT_NTT;
}
