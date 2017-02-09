/*	$NetBSD: tcp_congctl.h,v 1.7 2013/11/12 09:02:05 kefren Exp $	*/

/*
 * Copyright (c) 2006 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Rui Paulo.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _NETINET_TCP_CONGCTL_H
#define _NETINET_TCP_CONGCTL_H

#define TCPCC_MAXLEN 12

struct tcp_congctlent {
	TAILQ_ENTRY(tcp_congctlent) congctl_ent;
	const struct tcp_congctl *congctl_ctl;
	unsigned int congctl_refcnt;
	char congctl_name[TCPCC_MAXLEN];
};
/*
 * Congestion control function table.
 */
struct tcp_congctl {
	/*
	 * fast_retransmit: called on tcprexmtthresh'th dup ACKs.
	 * this actually retransmits packets by calling tcp_output()
	 * if appropriate.
	 * returns 0 if entering fast recovery.  otherwise returns non-0.
	 */
	int  (*fast_retransmit)(struct tcpcb *, const struct tcphdr *);

	/*
	 * slow_retransmit: called on RTO to adjust parameters like cwnd.
	 */
	void (*slow_retransmit)(struct tcpcb *);

	/*
	 * fast_retransmit_newack: called when new data is acked.
	 * ie. when advancing SND.UNA
	 * not called if TCP_SACK_ENABLED.
	 */
	void (*fast_retransmit_newack)(struct tcpcb *, const struct tcphdr *);

	/*
	 * newack: called when new data is acked.  ie. when advancing SND.UNA
	 * it's called before updating tp->snd_una.
	 */
	void (*newack)(struct tcpcb *, const struct tcphdr *);

	/*
	 * cong_exp: called when congestion is detected.  eg. by ECN
	 */
	void (*cong_exp)(struct tcpcb *);
};

extern const struct tcp_congctl tcp_reno_ctl;
extern const struct tcp_congctl tcp_newreno_ctl;
extern const struct tcp_congctl tcp_cubic_ctl;

/* currently selected global congestion control */
extern char tcp_congctl_global_name[TCPCC_MAXLEN];

/* available global congestion control algorithms */
extern char tcp_congctl_avail[10 * TCPCC_MAXLEN];

void   tcp_congctl_init(void);
int    tcp_congctl_register(const char *, const struct tcp_congctl *);
int    tcp_congctl_unregister(const char *);
int    tcp_congctl_select(struct tcpcb *, const char *);
void   tcp_congctl_release(struct tcpcb *);
const char *
       tcp_congctl_bystruct(const struct tcp_congctl *);

#endif /* _NETINET_TCP_CONGCTL_H */
