/*	$NetBSD: npf_sendpkt.c,v 1.15 2014/07/20 00:37:41 rmind Exp $	*/

/*-
 * Copyright (c) 2010-2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This material is based upon work partially supported by The
 * NetBSD Foundation under a contract with Mindaugas Rasiukevicius.
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

/*
 * NPF module for packet construction routines.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: npf_sendpkt.c,v 1.15 2014/07/20 00:37:41 rmind Exp $");

#include <sys/param.h>
#include <sys/types.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet6/ip6_var.h>
#include <sys/mbuf.h>

#include "npf_impl.h"

#define	DEFAULT_IP_TTL		(ip_defttl)

#ifndef INET6
#define	in6_cksum(...)		0
#define	ip6_output(...)		0
#define	icmp6_error(m, ...)	m_freem(m)
#endif

/*
 * npf_return_tcp: return a TCP reset (RST) packet.
 */
static int
npf_return_tcp(npf_cache_t *npc)
{
	struct mbuf *m;
	struct ip *ip = NULL;
	struct ip6_hdr *ip6 = NULL;
	struct tcphdr *oth, *th;
	tcp_seq seq, ack;
	int tcpdlen, len;
	uint32_t win;

	/* Fetch relevant data. */
	KASSERT(npf_iscached(npc, NPC_IP46));
	KASSERT(npf_iscached(npc, NPC_LAYER4));
	tcpdlen = npf_tcpsaw(npc, &seq, &ack, &win);
	oth = npc->npc_l4.tcp;

	if (oth->th_flags & TH_RST) {
		return 0;
	}

	/* Create and setup a network buffer. */
	if (npf_iscached(npc, NPC_IP4)) {
		len = sizeof(struct ip) + sizeof(struct tcphdr);
	} else if (npf_iscached(npc, NPC_IP6)) {
		len = sizeof(struct ip6_hdr) + sizeof(struct tcphdr);
	} else {
		return EINVAL;
	}

	m = m_gethdr(M_DONTWAIT, MT_HEADER);
	if (m == NULL) {
		return ENOMEM;
	}
	m->m_data += max_linkhdr;
	m->m_len = len;
	m->m_pkthdr.len = len;

	if (npf_iscached(npc, NPC_IP4)) {
		struct ip *oip = npc->npc_ip.v4;

		ip = mtod(m, struct ip *);
		memset(ip, 0, len);

		/*
		 * First, partially fill IPv4 header for TCP checksum.
		 * Note: IP length contains TCP header length.
		 */
		ip->ip_p = IPPROTO_TCP;
		ip->ip_src.s_addr = oip->ip_dst.s_addr;
		ip->ip_dst.s_addr = oip->ip_src.s_addr;
		ip->ip_len = htons(sizeof(struct tcphdr));

		th = (struct tcphdr *)(ip + 1);
	} else {
		struct ip6_hdr *oip = npc->npc_ip.v6;

		KASSERT(npf_iscached(npc, NPC_IP6));
		ip6 = mtod(m, struct ip6_hdr *);
		memset(ip6, 0, len);

		ip6->ip6_nxt = IPPROTO_TCP;
		ip6->ip6_hlim = IPV6_DEFHLIM;
		memcpy(&ip6->ip6_src, &oip->ip6_dst, sizeof(struct in6_addr));
		memcpy(&ip6->ip6_dst, &oip->ip6_src, sizeof(struct in6_addr));
		ip6->ip6_plen = htons(len);
		ip6->ip6_vfc = IPV6_VERSION;

		th = (struct tcphdr *)(ip6 + 1);
	}

	/*
	 * Construct TCP header and compute the checksum.
	 */
	th->th_sport = oth->th_dport;
	th->th_dport = oth->th_sport;
	th->th_seq = htonl(ack);
	if (oth->th_flags & TH_SYN) {
		tcpdlen++;
	}
	th->th_ack = htonl(seq + tcpdlen);
	th->th_off = sizeof(struct tcphdr) >> 2;
	th->th_flags = TH_ACK | TH_RST;

	if (npf_iscached(npc, NPC_IP4)) {
		th->th_sum = in_cksum(m, len);

		/*
		 * Second, fill the rest of IPv4 header and correct IP length.
		 */
		ip->ip_v = IPVERSION;
		ip->ip_hl = sizeof(struct ip) >> 2;
		ip->ip_tos = IPTOS_LOWDELAY;
		ip->ip_len = htons(len);
		ip->ip_ttl = DEFAULT_IP_TTL;
	} else {
		KASSERT(npf_iscached(npc, NPC_IP6));
		th->th_sum = in6_cksum(m, IPPROTO_TCP, sizeof(struct ip6_hdr),
		    sizeof(struct tcphdr));
	}

	/* Pass to IP layer. */
	if (npf_iscached(npc, NPC_IP4)) {
		return ip_output(m, NULL, NULL, IP_FORWARDING, NULL, NULL);
	}
	return ip6_output(m, NULL, NULL, IPV6_FORWARDING, NULL, NULL, NULL);
}

/*
 * npf_return_icmp: return an ICMP error.
 */
static int
npf_return_icmp(const npf_cache_t *npc)
{
	struct mbuf *m = nbuf_head_mbuf(npc->npc_nbuf);

	if (npf_iscached(npc, NPC_IP4)) {
		icmp_error(m, ICMP_UNREACH, ICMP_UNREACH_ADMIN_PROHIBIT, 0, 0);
		return 0;
	} else if (npf_iscached(npc, NPC_IP6)) {
		icmp6_error(m, ICMP6_DST_UNREACH, ICMP6_DST_UNREACH_ADMIN, 0);
		return 0;
	}
	return EINVAL;
}

/*
 * npf_return_block: return TCP reset or ICMP host unreachable packet.
 *
 * => Returns true if the buffer was consumed (freed) and false otherwise.
 */
bool
npf_return_block(npf_cache_t *npc, const int retfl)
{
	if (!npf_iscached(npc, NPC_IP46) || !npf_iscached(npc, NPC_LAYER4)) {
		return false;
	}
	switch (npc->npc_proto) {
	case IPPROTO_TCP:
		if (retfl & NPF_RULE_RETRST) {
			(void)npf_return_tcp(npc);
		}
		break;
	case IPPROTO_UDP:
		if (retfl & NPF_RULE_RETICMP)
			if (npf_return_icmp(npc) == 0)
				return true;
		break;
	}
	return false;
}
