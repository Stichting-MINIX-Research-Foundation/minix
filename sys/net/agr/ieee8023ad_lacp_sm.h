/*	$NetBSD: ieee8023ad_lacp_sm.h,v 1.2 2005/12/10 23:21:39 elad Exp $	*/

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

#ifndef _NET_AGR_IEEE8023AD_LACP_SM_H_
#define	_NET_AGR_IEEE8023AD_LACP_SM_H_

#define	LACP_STATE_EQ(s1, s2, mask)	\
	((((s1) ^ (s2)) & (mask)) == 0)

/* receive machine */

void lacp_sm_rx(struct lacp_port *, const struct lacpdu *);
void lacp_sm_rx_timer(struct lacp_port *);
void lacp_sm_rx_set_expired(struct lacp_port *);

/* mux machine */

void lacp_sm_mux(struct lacp_port *);
void lacp_sm_mux_timer(struct lacp_port *);

/* periodic transmit machine */

void lacp_sm_ptx_update_timeout(struct lacp_port *, uint8_t);
void lacp_sm_ptx_tx_schedule(struct lacp_port *);
void lacp_sm_ptx_timer(struct lacp_port *);

/* transmit machine */

void lacp_sm_tx(struct lacp_port *);
void lacp_sm_assert_ntt(struct lacp_port *);

#endif /* !_NET_AGR_IEEE8023AD_LACP_SM_H_ */
