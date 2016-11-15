/*	$NetBSD: in6_cksum.c,v 1.28 2011/04/25 22:05:05 yamt Exp $	*/

/*-
 * Copyright (c) 2008 Joerg Sonnenberger <joerg@NetBSD.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: in6_cksum.c,v 1.28 2011/04/25 22:05:05 yamt Exp $");

#include <sys/param.h>
#include <sys/mbuf.h>
#include <netinet/in.h>
#include <netinet/ip6.h>

/*
 * Checksum of the IPv6 pseudo header.
 *
 * off is supposed to be the skipped IPv6 header, len is the payload size.
 */

int
in6_cksum(struct mbuf *m, u_int8_t nxt, uint32_t off, uint32_t len)
{
	union {
		uint16_t words[16];
		struct {
			struct in6_addr ip6_src;
			struct in6_addr ip6_dst;
		} addrs;
	} u;
	const struct in6_addr *in6_src;
	const struct in6_addr *in6_dst;
	const struct ip6_hdr *ip6;
	uint32_t sum;
	const uint16_t *w;
	const char *cp;

	if (nxt == 0)
		return cpu_in_cksum(m, len, off, 0);

	if (__predict_false(off < sizeof(struct ip6_hdr)))
		panic("in6_cksum: offset too short for IPv6 header");
	if (__predict_false(m->m_len < sizeof(struct ip6_hdr)))
		panic("in6_cksum: mbuf too short for IPv6 header");

	/*
	 * Compute the equivalent of:
	 * struct ip6_hdr_pseudo ip6;
	 *
	 * bzero(sizeof(*ip6));
	 * ip6.ip6ph_nxt = nxt;
	 * ip6.ip6ph_len = htonl(len);
	 * ipv6.ip6ph_src = mtod(m, struct ip6_hdr *)->ip6_src;
	 * in6_clearscope(&ip6->ip6ph_src);
	 * ipv6.ip6ph_dst = mtod(m, struct ip6_hdr *)->ip6_dst;
	 * in6_clearscope(&ip6->ip6ph_dst);
	 * sum = one_add(&ip6);
	 */

#if BYTE_ORDER == LITTLE_ENDIAN
	sum = ((len & 0xffff) + ((len >> 16) & 0xffff) + nxt) << 8;
#else
	sum = (len & 0xffff) + ((len >> 16) & 0xffff) + nxt;
#endif
	cp = mtod(m, const char *);
	w = (const uint16_t *)(cp + offsetof(struct ip6_hdr, ip6_src));
	ip6 = (const void *)cp;
	if (__predict_true((uintptr_t)w % 2 == 0)) {
		in6_src = &ip6->ip6_src;
		in6_dst = &ip6->ip6_dst;
	} else {
		memcpy(&u, &ip6->ip6_src, 32);
		w = u.words;
		in6_src = &u.addrs.ip6_src;
		in6_dst = &u.addrs.ip6_dst;
	}

	sum += w[0];
	if (!IN6_IS_SCOPE_EMBEDDABLE(in6_src))
		sum += w[1];
	sum += w[2];
	sum += w[3];
	sum += w[4];
	sum += w[5];
	sum += w[6];
	sum += w[7];
	w += 8;
	sum += w[0];
	if (!IN6_IS_SCOPE_EMBEDDABLE(in6_dst))
		sum += w[1];
	sum += w[2];
	sum += w[3];
	sum += w[4];
	sum += w[5];
	sum += w[6];
	sum += w[7];

	return cpu_in_cksum(m, len, off, sum);
}
