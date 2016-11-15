/*	$NetBSD: npf_alg_icmp.c,v 1.23 2014/07/20 00:37:41 rmind Exp $	*/

/*-
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
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
 * NPF ALG for ICMP and traceroute translations.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: npf_alg_icmp.c,v 1.23 2014/07/20 00:37:41 rmind Exp $");

#include <sys/param.h>
#include <sys/module.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include <net/pfil.h>

#include "npf_impl.h"
#include "npf_conn.h"

MODULE(MODULE_CLASS_MISC, npf_alg_icmp, "npf");

/*
 * Traceroute criteria.
 *
 * IANA assigned base port: 33434.  However, common practice is to increase
 * the port, thus monitor [33434-33484] range.  Additional filter is low TTL.
 */

#define	TR_BASE_PORT	33434
#define	TR_PORT_RANGE	33484
#define	TR_MAX_TTL	48

static npf_alg_t *	alg_icmp	__read_mostly;

/*
 * npfa_icmp_match: matching inspector determines ALG case and associates
 * our ALG with the NAT entry.
 */
static bool
npfa_icmp_match(npf_cache_t *npc, npf_nat_t *nt, int di)
{
	const int proto = npc->npc_proto;
	const struct ip *ip = npc->npc_ip.v4;
	in_port_t dport;

	KASSERT(npf_iscached(npc, NPC_IP46));
	KASSERT(npf_iscached(npc, NPC_LAYER4));

	/* Check for low TTL.  Also, we support outbound NAT only. */
	if (ip->ip_ttl > TR_MAX_TTL || di != PFIL_OUT) {
		return false;
	}

	switch (proto) {
	case IPPROTO_TCP: {
		const struct tcphdr *th = npc->npc_l4.tcp;
		dport = ntohs(th->th_dport);
		break;
	}
	case IPPROTO_UDP: {
		const struct udphdr *uh = npc->npc_l4.udp;
		dport = ntohs(uh->uh_dport);
		break;
	}
	case IPPROTO_ICMP:
	case IPPROTO_ICMPV6:
		/* Just to pass the test below. */
		dport = TR_BASE_PORT;
		break;
	default:
		return false;
	}

	/* Handle TCP/UDP traceroute - check for port range. */
	if (dport < TR_BASE_PORT || dport > TR_PORT_RANGE) {
		return false;
	}

	/* Associate ALG with translation entry. */
	npf_nat_setalg(nt, alg_icmp, 0);
	return true;
}

/*
 * npfa_icmp{4,6}_inspect: retrieve unique identifiers - either ICMP query
 * ID or TCP/UDP ports of the original packet, which is embedded.
 */

static bool
npfa_icmp4_inspect(const int type, npf_cache_t *npc)
{
	nbuf_t *nbuf = npc->npc_nbuf;
	u_int offby;

	/* Per RFC 792. */
	switch (type) {
	case ICMP_UNREACH:
	case ICMP_SOURCEQUENCH:
	case ICMP_REDIRECT:
	case ICMP_TIMXCEED:
	case ICMP_PARAMPROB:
		if (npc == NULL) {
			return false;
		}
		/* Should contain original IP header. */
		if (!nbuf_advance(nbuf, offsetof(struct icmp, icmp_ip), 0)) {
			return false;
		}
		return (npf_cache_all(npc) & NPC_LAYER4) != 0;

	case ICMP_ECHOREPLY:
	case ICMP_ECHO:
	case ICMP_TSTAMP:
	case ICMP_TSTAMPREPLY:
	case ICMP_IREQ:
	case ICMP_IREQREPLY:
		/* Should contain ICMP query ID - ensure. */
		offby = offsetof(struct icmp, icmp_id);
		if (!nbuf_advance(nbuf, offby, sizeof(uint16_t))) {
			return false;
		}
		npc->npc_info |= NPC_ICMP_ID;
		return true;
	default:
		break;
	}
	return false;
}

static bool
npfa_icmp6_inspect(const int type, npf_cache_t *npc)
{
	nbuf_t *nbuf = npc->npc_nbuf;
	u_int offby;

	/* Per RFC 4443. */
	switch (type) {
	case ICMP6_DST_UNREACH:
	case ICMP6_PACKET_TOO_BIG:
	case ICMP6_TIME_EXCEEDED:
	case ICMP6_PARAM_PROB:
		if (npc == NULL) {
			return false;
		}
		/* Should contain original IP header. */
		if (!nbuf_advance(nbuf, sizeof(struct icmp6_hdr), 0)) {
			return false;
		}
		return (npf_cache_all(npc) & NPC_LAYER4) != 0;

	case ICMP6_ECHO_REQUEST:
	case ICMP6_ECHO_REPLY:
		/* Should contain ICMP query ID - ensure. */
		offby = offsetof(struct icmp6_hdr, icmp6_id);
		if (!nbuf_advance(nbuf, offby, sizeof(uint16_t))) {
			return false;
		}
		npc->npc_info |= NPC_ICMP_ID;
		return true;
	default:
		break;
	}
	return false;
}

/*
 * npfa_icmp_inspect: ALG ICMP inspector.
 *
 * => Returns true if "enpc" is filled.
 */
static bool
npfa_icmp_inspect(npf_cache_t *npc, npf_cache_t *enpc)
{
	nbuf_t *nbuf = npc->npc_nbuf;
	bool ret;

	KASSERT(npf_iscached(npc, NPC_IP46));
	KASSERT(npf_iscached(npc, NPC_ICMP));

	/* Advance to ICMP header. */
	nbuf_reset(nbuf);
	if (!nbuf_advance(nbuf, npc->npc_hlen, 0)) {
		return false;
	}
	enpc->npc_nbuf = nbuf;
	enpc->npc_info = 0;

	/*
	 * Inspect the ICMP packet.  The relevant data might be in the
	 * embedded packet.  Fill the "enpc" cache, if so.
	 */
	if (npf_iscached(npc, NPC_IP4)) {
		const struct icmp *ic = npc->npc_l4.icmp;
		ret = npfa_icmp4_inspect(ic->icmp_type, enpc);
	} else if (npf_iscached(npc, NPC_IP6)) {
		const struct icmp6_hdr *ic6 = npc->npc_l4.icmp6;
		ret = npfa_icmp6_inspect(ic6->icmp6_type, enpc);
	} else {
		ret = false;
	}
	if (!ret) {
		return false;
	}

	/* ICMP ID is the original packet, just indicate it. */
	if (npf_iscached(enpc, NPC_ICMP_ID)) {
		npc->npc_info |= NPC_ICMP_ID;
		return false;
	}

	/* Indicate that embedded packet is in the cache. */
	return true;
}

static npf_conn_t *
npfa_icmp_conn(npf_cache_t *npc, int di)
{
	npf_cache_t enpc;

	/* Inspect ICMP packet for an embedded packet. */
	if (!npf_iscached(npc, NPC_ICMP))
		return NULL;
	if (!npfa_icmp_inspect(npc, &enpc))
		return NULL;

	/*
	 * Invert the identifiers of the embedded packet.
	 * If it is ICMP, then ensure ICMP ID.
	 */
	union l4 {
		struct tcphdr th;
		struct udphdr uh;
	} l4;
	bool ret, forw;

	#define	SWAP(type, x, y) { type tmp = x; x = y; y = tmp; }
	SWAP(npf_addr_t *, enpc.npc_ips[NPF_SRC], enpc.npc_ips[NPF_DST]);

	switch (enpc.npc_proto) {
	case IPPROTO_TCP:
		l4.th.th_sport = enpc.npc_l4.tcp->th_dport;
		l4.th.th_dport = enpc.npc_l4.tcp->th_sport;
		enpc.npc_l4.tcp = &l4.th;
		break;
	case IPPROTO_UDP:
		l4.uh.uh_sport = enpc.npc_l4.udp->uh_dport;
		l4.uh.uh_dport = enpc.npc_l4.udp->uh_sport;
		enpc.npc_l4.udp = &l4.uh;
		break;
	case IPPROTO_ICMP: {
		const struct icmp *ic = enpc.npc_l4.icmp;
		ret = npfa_icmp4_inspect(ic->icmp_type, &enpc);
		if (!ret || !npf_iscached(&enpc, NPC_ICMP_ID))
			return false;
		break;
	}
	case IPPROTO_ICMPV6: {
		const struct icmp6_hdr *ic6 = enpc.npc_l4.icmp6;
		ret = npfa_icmp6_inspect(ic6->icmp6_type, &enpc);
		if (!ret || !npf_iscached(&enpc, NPC_ICMP_ID))
			return false;
		break;
	}
	default:
		return false;
	}

	/* Lookup a connection using the embedded packet. */
	return npf_conn_lookup(&enpc, di, &forw);
}

/*
 * npfa_icmp_nat: ALG translator - rewrites IP address in the IP header
 * which is embedded in ICMP packet.  Note: backwards stream only.
 */
static bool
npfa_icmp_nat(npf_cache_t *npc, npf_nat_t *nt, bool forw)
{
	const u_int which = NPF_SRC;
	npf_cache_t enpc;

	if (forw || !npf_iscached(npc, NPC_ICMP))
		return false;
	if (!npfa_icmp_inspect(npc, &enpc))
		return false;

	KASSERT(npf_iscached(&enpc, NPC_IP46));
	KASSERT(npf_iscached(&enpc, NPC_LAYER4));

	/*
	 * ICMP: fetch the current checksum we are going to fixup.
	 */
	struct icmp *ic = npc->npc_l4.icmp;
	uint16_t cksum = ic->icmp_cksum;

	CTASSERT(offsetof(struct icmp, icmp_cksum) ==
	    offsetof(struct icmp6_hdr, icmp6_cksum));

	/*
	 * Fetch the IP and port in the _embedded_ packet.  Also, fetch
	 * the IPv4 and TCP/UDP checksums before they are rewritten.
	 * Calculate the part of the ICMP checksum fixup.
	 */
	const int proto = enpc.npc_proto;
	uint16_t ipcksum = 0, l4cksum = 0;
	npf_addr_t *addr;
	in_port_t port;

	npf_nat_getorig(nt, &addr, &port);

	if (npf_iscached(&enpc, NPC_IP4)) {
		const struct ip *eip = enpc.npc_ip.v4;
		ipcksum = eip->ip_sum;
	}
	cksum = npf_addr_cksum(cksum, enpc.npc_alen, enpc.npc_ips[which], addr);

	switch (proto) {
	case IPPROTO_TCP: {
		const struct tcphdr *th = enpc.npc_l4.tcp;
		cksum = npf_fixup16_cksum(cksum, th->th_sport, port);
		l4cksum = th->th_sum;
		break;
	}
	case IPPROTO_UDP: {
		const struct udphdr *uh = enpc.npc_l4.udp;
		cksum = npf_fixup16_cksum(cksum, uh->uh_sport, port);
		l4cksum = uh->uh_sum;
		break;
	}
	case IPPROTO_ICMP:
	case IPPROTO_ICMPV6:
		break;
	default:
		return false;
	}

	/*
	 * Translate the embedded packet.  The following changes will
	 * be performed by npf_napt_rwr():
	 *
	 *	1) Rewrite the IP address and, if not ICMP, port.
	 *	2) Rewrite the TCP/UDP checksum (if not ICMP).
	 *	3) Rewrite the IPv4 checksum for (1) and (2).
	 *
	 * XXX: Assumes NPF_NATOUT (source address/port).  Currently,
	 * npfa_icmp_match() matches only for the PFIL_OUT traffic.
	 */
	if (npf_napt_rwr(&enpc, which, addr, port)) {
		return false;
	}

	/*
	 * Finally, finish the ICMP checksum fixup: include the checksum
	 * changes in the embedded packet.
	 */
	if (npf_iscached(&enpc, NPC_IP4)) {
		const struct ip *eip = enpc.npc_ip.v4;
		cksum = npf_fixup16_cksum(cksum, ipcksum, eip->ip_sum);
	}
	switch (proto) {
	case IPPROTO_TCP: {
		const struct tcphdr *th = enpc.npc_l4.tcp;
		cksum = npf_fixup16_cksum(cksum, l4cksum, th->th_sum);
		break;
	}
	case IPPROTO_UDP:
		if (l4cksum) {
			const struct udphdr *uh = enpc.npc_l4.udp;
			cksum = npf_fixup16_cksum(cksum, l4cksum, uh->uh_sum);
		}
		break;
	}
	ic->icmp_cksum = cksum;
	return true;
}

/*
 * npf_alg_icmp_{init,fini,modcmd}: ICMP ALG initialization, destruction
 * and module interface.
 */

static int
npf_alg_icmp_init(void)
{
	static const npfa_funcs_t icmp = {
		.match		= npfa_icmp_match,
		.translate	= npfa_icmp_nat,
		.inspect	= npfa_icmp_conn,
	};
	alg_icmp = npf_alg_register("icmp", &icmp);
	return alg_icmp ? 0 : ENOMEM;
}

static int
npf_alg_icmp_fini(void)
{
	KASSERT(alg_icmp != NULL);
	return npf_alg_unregister(alg_icmp);
}

static int
npf_alg_icmp_modcmd(modcmd_t cmd, void *arg)
{
	switch (cmd) {
	case MODULE_CMD_INIT:
		return npf_alg_icmp_init();
	case MODULE_CMD_FINI:
		return npf_alg_icmp_fini();
	case MODULE_CMD_AUTOUNLOAD:
		return EBUSY;
	default:
		return ENOTTY;
	}
	return 0;
}
