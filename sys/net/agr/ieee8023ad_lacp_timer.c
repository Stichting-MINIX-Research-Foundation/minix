/*	$NetBSD: ieee8023ad_lacp_timer.c,v 1.5 2006/10/22 03:39:43 uebayasi Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: ieee8023ad_lacp_timer.c,v 1.5 2006/10/22 03:39:43 uebayasi Exp $");

#include <sys/param.h>
#include <sys/callout.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/if_ether.h>

#include <net/agr/ieee8023_slowprotocols.h>
#include <net/agr/ieee8023_tlv.h>
#include <net/agr/ieee8023ad_lacp.h>
#include <net/agr/ieee8023ad_lacp_impl.h>
#include <net/agr/ieee8023ad_lacp_sm.h>

typedef void (*lacp_timer_func_t)(struct lacp_port *);

static const lacp_timer_func_t lacp_timer_funcs[LACP_NTIMER] = {
	[LACP_TIMER_CURRENT_WHILE] = lacp_sm_rx_timer,
	[LACP_TIMER_PERIODIC] = lacp_sm_ptx_timer,
	[LACP_TIMER_WAIT_WHILE] = lacp_sm_mux_timer,
};

void
lacp_run_timers(struct lacp_port *lp)
{
	int i;

	for (i = 0; i < LACP_NTIMER; i++) {
		KASSERT(lp->lp_timer[i] >= 0);
		if (lp->lp_timer[i] == 0) {
			continue;
		} else if (--lp->lp_timer[i] <= 0) {
			if (lacp_timer_funcs[i]) {
				(*lacp_timer_funcs[i])(lp);
			}
		}
	}
}
