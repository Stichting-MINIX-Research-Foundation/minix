/*	$NetBSD: in6_offload.c,v 1.6 2011/04/25 22:07:57 yamt Exp $	*/

/*-
 * Copyright (c)2006 YAMAMOTO Takashi,
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
__KERNEL_RCSID(0, "$NetBSD: in6_offload.c,v 1.6 2011/04/25 22:07:57 yamt Exp $");

#include <sys/param.h>
#include <sys/mbuf.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>
#include <netinet6/in6_offload.h>

struct ip6_tso_output_args {
	struct ifnet *ifp;
	struct ifnet *origifp;
	const struct sockaddr_in6 *dst;
	struct rtentry *rt;
};

static int ip6_tso_output_callback(void *, struct mbuf *);

static int
ip6_tso_output_callback(void *vp, struct mbuf *m)
{
	struct ip6_tso_output_args *args = vp;

	return nd6_output(args->ifp, args->origifp, m, args->dst, args->rt);
}

int
ip6_tso_output(struct ifnet *ifp, struct ifnet *origifp, struct mbuf *m,
    const struct sockaddr_in6 *dst, struct rtentry *rt)
{
	struct ip6_tso_output_args args;

	args.ifp = ifp;
	args.origifp = origifp;
	args.dst = dst;
	args.rt = rt;

	return tcp6_segment(m, ip6_tso_output_callback, &args);
}

/*
 * tcp6_segment: handle M_CSUM_TSOv6 by software.
 *
 * => always consume m.
 * => call output_func with output_arg for each segments.
 */

int
tcp6_segment(struct mbuf *m, int (*output_func)(void *, struct mbuf *),
    void *output_arg)
{
	int mss;
	int iphlen;
	int thlen;
	int hlen;
	int len;
	struct ip6_hdr *iph;
	struct tcphdr *th;
	uint32_t tcpseq;
	struct mbuf *hdr = NULL;
	struct mbuf *t;
	int error = 0;

	KASSERT((m->m_flags & M_PKTHDR) != 0);
	KASSERT((m->m_pkthdr.csum_flags & M_CSUM_TSOv6) != 0);

	m->m_pkthdr.csum_flags = 0;

	len = m->m_pkthdr.len;
	KASSERT(len >= sizeof(*iph) + sizeof(*th));

	if (m->m_len < sizeof(*iph)) {
		m = m_pullup(m, sizeof(*iph));
		if (m == NULL) {
			error = ENOMEM;
			goto quit;
		}
	}
	iph = mtod(m, struct ip6_hdr *);
	iphlen = sizeof(*iph);
	KASSERT((iph->ip6_vfc & IPV6_VERSION_MASK) == IPV6_VERSION);
	KASSERT(iph->ip6_nxt == IPPROTO_TCP);

	hlen = iphlen + sizeof(*th);
	if (m->m_len < hlen) {
		m = m_pullup(m, hlen);
		if (m == NULL) {
			error = ENOMEM;
			goto quit;
		}
	}
	th = (void *)(mtod(m, char *) + iphlen);
	tcpseq = ntohl(th->th_seq);
	thlen = th->th_off * 4;
	hlen = iphlen + thlen;

	mss = m->m_pkthdr.segsz;
	KASSERT(mss != 0);
	KASSERT(len > hlen);

	t = m_split(m, hlen, M_NOWAIT);
	if (t == NULL) {
		error = ENOMEM;
		goto quit;
	}
	hdr = m;
	m = t;
	len -= hlen;
	KASSERT(len % mss == 0);
	while (len > 0) {
		struct mbuf *n;

		n = m_dup(hdr, 0, hlen, M_NOWAIT);
		if (n == NULL) {
			error = ENOMEM;
			goto quit;
		}
		KASSERT(n->m_len == hlen); /* XXX */

		t = m_split(m, mss, M_NOWAIT);
		if (t == NULL) {
			m_freem(n);
			error = ENOMEM;
			goto quit;
		}
		m_cat(n, m);
		m = t;

		KASSERT(n->m_len >= hlen); /* XXX */

		n->m_pkthdr.len = hlen + mss;
		iph = mtod(n, struct ip6_hdr *);
		KASSERT((iph->ip6_vfc & IPV6_VERSION_MASK) == IPV6_VERSION);
		iph->ip6_plen = htons(thlen + mss);
		th = (void *)(mtod(n, char *) + iphlen);
		th->th_seq = htonl(tcpseq);
		th->th_sum = 0;
		th->th_sum = in6_cksum(n, IPPROTO_TCP, iphlen, thlen + mss);

		error = (*output_func)(output_arg, n);
		if (error) {
			goto quit;
		}

		tcpseq += mss;
		len -= mss;
	}

quit:
	if (hdr != NULL) {
		m_freem(hdr);
	}
	if (m != NULL) {
		m_freem(m);
	}

	return error;
}

void
ip6_undefer_csum(struct mbuf *m, size_t hdrlen, int csum_flags)
{
	const size_t ip6_plen_offset =
	    hdrlen + offsetof(struct ip6_hdr, ip6_plen);
	size_t l4hdroff;
	size_t l4offset;
	uint16_t plen;
	uint16_t csum;

	KASSERT(m->m_flags & M_PKTHDR);
	KASSERT((m->m_pkthdr.csum_flags & csum_flags) == csum_flags);
	KASSERT(csum_flags == M_CSUM_UDPv6 || csum_flags == M_CSUM_TCPv6);

	if (__predict_true(hdrlen + sizeof(struct ip6_hdr) <= m->m_len)) {
		plen = *(uint16_t *)(mtod(m, char *) + ip6_plen_offset);
	} else {
		m_copydata(m, ip6_plen_offset, sizeof(plen), &plen);
	}
	plen = ntohs(plen);

	l4hdroff = M_CSUM_DATA_IPv6_HL(m->m_pkthdr.csum_data);
	l4offset = hdrlen + l4hdroff;
	csum = in6_cksum(m, 0, l4offset, plen - l4hdroff);

	if (csum == 0 && (csum_flags & M_CSUM_UDPv6) != 0)
		csum = 0xffff;

	l4offset += M_CSUM_DATA_IPv6_OFFSET(m->m_pkthdr.csum_data);

	if (__predict_true((l4offset + sizeof(uint16_t)) <= m->m_len)) {
		*(uint16_t *)(mtod(m, char *) + l4offset) = csum;
	} else {
		m_copyback(m, l4offset, sizeof(csum), (void *) &csum);
	}

	m->m_pkthdr.csum_flags ^= csum_flags;
}
