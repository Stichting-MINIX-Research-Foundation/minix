/*	$NetBSD: in6_offload.h,v 1.7 2011/04/25 22:20:59 yamt Exp $	*/

/*-
 * Copyright (c)2005, 2006 YAMAMOTO Takashi,
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

/*
 * subroutines to do software-only equivalent of h/w offloading.
 */

#if !defined(_NETINET6_IN6_OFFLOAD_H_)
#define	_NETINET6_IN6_OFFLOAD_H_

int tcp6_segment(struct mbuf *, int (*)(void *, struct mbuf *), void *);
int ip6_tso_output(struct ifnet *, struct ifnet *, struct mbuf *,
    const struct sockaddr_in6 *, struct rtentry *);
void ip6_undefer_csum(struct mbuf *, size_t, int);

extern int tcp_do_loopback_cksum; /* do TCP checksum on loopback? */
extern int udp_do_loopback_cksum; /* do UDP checksum on loopback? */

#define	IN6_LOOPBACK_NEED_CHECKSUM(csum_flags) \
	((((csum_flags) & M_CSUM_UDPv6) != 0 && udp_do_loopback_cksum) || \
	(((csum_flags) & M_CSUM_TCPv6) != 0 && tcp_do_loopback_cksum))

#define	IN6_NEED_CHECKSUM(ifp, csum_flags) \
	(__predict_true(((ifp)->if_flags & IFF_LOOPBACK) == 0 || \
	IN6_LOOPBACK_NEED_CHECKSUM(csum_flags)))

#endif /* !defined(_NETINET6_IN6_OFFLOAD_H_) */
