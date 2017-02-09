/*      $NetBSD: ip_etherip.c,v 1.15 2015/08/24 22:21:26 pooka Exp $        */

/*
 *  Copyright (c) 2006, Hans Rosenfeld <rosenfeld@grumpf.hope-2000.org>
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of Hans Rosenfeld nor the names of his
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *  SUCH DAMAGE.
 *
 *
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ip_etherip.c,v 1.15 2015/08/24 22:21:26 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/protosw.h>
#include <sys/kernel.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_var.h>
#include <netinet/ip_etherip.h>

#include <net/if_ether.h>
#include <net/if_media.h>
#include <net/if_etherip.h>
#include <net/bpf.h>

int
ip_etherip_output(struct ifnet *ifp, struct mbuf *m)
{
	struct rtentry *rt;
	struct etherip_softc *sc = (struct etherip_softc*)ifp->if_softc;
	struct sockaddr_in *sin_src, *sin_dst;
	struct ip iphdr;        /* capsule IP header, host byte ordered */
	struct etherip_header eiphdr;
	int proto, error;
	union {
		struct sockaddr		dst;
		struct sockaddr_in	dst4;
	} u;

	sin_src = (struct sockaddr_in *)sc->sc_src;
	sin_dst = (struct sockaddr_in *)sc->sc_dst;

	if (sin_src == NULL || 
	    sin_dst == NULL ||
	    sin_src->sin_family != AF_INET ||
	    sin_dst->sin_family != AF_INET) {
		m_freem(m);
		return EAFNOSUPPORT;
	}

	/* reset broadcast/multicast flags */
	m->m_flags &= ~(M_BCAST|M_MCAST);

	m->m_flags |= M_PKTHDR;
	proto = IPPROTO_ETHERIP;

	/* fill and prepend Ethernet-in-IP header */
	eiphdr.eip_ver = ETHERIP_VERSION & ETHERIP_VER_VERS_MASK;
	eiphdr.eip_pad = 0;
	M_PREPEND(m, sizeof(struct etherip_header), M_DONTWAIT);
	if (m == NULL)
		return ENOBUFS;
	if (M_UNWRITABLE(m, sizeof(struct etherip_header))) {
		m = m_pullup(m, sizeof(struct etherip_header));
		if (m == NULL)
			return ENOBUFS;
	}
	memcpy(mtod(m, struct etherip_header *), &eiphdr, 
	       sizeof(struct etherip_header));

	/* fill new IP header */
	memset(&iphdr, 0, sizeof(struct ip));
	iphdr.ip_src = sin_src->sin_addr;
	/* bidirectional configured tunnel mode */
	if (sin_dst->sin_addr.s_addr != INADDR_ANY)
		iphdr.ip_dst = sin_dst->sin_addr;
	else {
		m_freem(m);
		return ENETUNREACH;
	}
	iphdr.ip_p = proto;
	/* version will be set in ip_output() */
	iphdr.ip_ttl = ETHERIP_TTL;
	iphdr.ip_len = htons(m->m_pkthdr.len + sizeof(struct ip));

	/* prepend new IP header */
	M_PREPEND(m, sizeof(struct ip), M_DONTWAIT);
	if (m == NULL)
		return ENOBUFS;
	if (M_UNWRITABLE(m, sizeof(struct ip)))
		m = m_pullup(m, sizeof(struct ip));
	memcpy(mtod(m, struct ip *), &iphdr, sizeof(struct ip));

	sockaddr_in_init(&u.dst4, &sin_dst->sin_addr, 0);
	if ((rt = rtcache_lookup(&sc->sc_ro, &u.dst)) == NULL) {
		m_freem(m);
		return ENETUNREACH;
	}

	/* if it constitutes infinite encapsulation, punt. */
	if (rt->rt_ifp == ifp) {
		rtcache_free(&sc->sc_ro);
		m_freem(m);
		return ENETUNREACH;     /*XXX*/
	}

	error = ip_output(m, NULL, &sc->sc_ro, 0, NULL, NULL);

	return error;
}

void
ip_etherip_input(struct mbuf *m, ...)
{
	struct etherip_softc *sc;
	const struct ip *ip;
	struct sockaddr_in *src, *dst;
	struct ifnet *ifp = NULL;
	int off, proto, s;
	va_list ap;

	va_start(ap, m);
	off = va_arg(ap, int);
	proto = va_arg(ap, int);
	va_end(ap);

	if (proto != IPPROTO_ETHERIP) {
		m_freem(m);
		ip_statinc(IP_STAT_NOPROTO);
		return;
	}

	ip = mtod(m, const struct ip *);

	/* find device configured for this packets src and dst */
	LIST_FOREACH(sc, &etherip_softc_list, etherip_list) {
		if (!sc->sc_src || !sc->sc_dst)
			continue;

		if (sc->sc_src->sa_family != AF_INET ||
		    sc->sc_dst->sa_family != AF_INET)
			continue;

		src = (struct sockaddr_in *)sc->sc_src;
		dst = (struct sockaddr_in *)sc->sc_dst;

		if (src->sin_addr.s_addr != ip->ip_dst.s_addr ||
		    dst->sin_addr.s_addr != ip->ip_src.s_addr)
			continue;
		
		ifp = &sc->sc_ec.ec_if;
		break;
	}

	/* no matching device found */
	if (!ifp) {
		m_freem(m);
		ip_statinc(IP_STAT_ODROPPED);
		return;
	}

	m_adj(m, off);

	/*
	 * Section 4 of RFC 3378 requires that the EtherIP header of incoming
	 * packets is verified to contain the correct values in the version and
	 * reserved fields, and packets with wrong values be dropped.
	 *
	 * There is some discussion about what exactly the header should look
	 * like, the RFC is not very clear there. To be compatible with broken
	 * implementations, we don't check the header on incoming packets,
	 * relying on the ethernet code to filter out garbage.
	 *
	 * The header we use for sending is compatible with the original
	 * implementation in OpenBSD, which was used in former NetBSD versions
	 * and is used in FreeBSD. One Linux implementation is known to use the
	 * same value.
	 */
	m_adj(m, sizeof(struct etherip_header));
	m = m_pullup(m, sizeof(struct ether_header));
	if (m == NULL) {
		ifp->if_ierrors++;
		return;
	}

	m->m_pkthdr.rcvif = ifp;
	m->m_flags &= ~(M_BCAST|M_MCAST);

	bpf_mtap(ifp, m);

	ifp->if_ipackets++;

	s = splnet();
	(ifp->if_input)(ifp, m);
	splx(s);

	return;
}

