/*	$NetBSD: in_gif.c,v 1.65 2015/08/24 22:21:26 pooka Exp $	*/
/*	$KAME: in_gif.c,v 1.66 2001/07/29 04:46:09 itojun Exp $	*/

/*
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
__KERNEL_RCSID(0, "$NetBSD: in_gif.c,v 1.65 2015/08/24 22:21:26 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
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
#include <netinet/in_gif.h>
#include <netinet/in_var.h>
#include <netinet/ip_encap.h>
#include <netinet/ip_ecn.h>

#ifdef INET6
#include <netinet/ip6.h>
#endif

#include <net/if_gif.h>

#include "gif.h"

#include <net/net_osdep.h>

static int gif_validate4(const struct ip *, struct gif_softc *,
	struct ifnet *);

#if NGIF > 0
int ip_gif_ttl = GIF_TTL;
#else
int ip_gif_ttl = 0;
#endif

const struct protosw in_gif_protosw = {
	.pr_type	= SOCK_RAW,
	.pr_domain	= &inetdomain,
	.pr_protocol	= 0 /* IPPROTO_IPV[46] */,
	.pr_flags	= PR_ATOMIC|PR_ADDR,
	.pr_input	= in_gif_input,
	.pr_output	= rip_output,
	.pr_ctlinput	= NULL,
	.pr_ctloutput	= rip_ctloutput,
	.pr_usrreqs	= &rip_usrreqs,
};

int
in_gif_output(struct ifnet *ifp, int family, struct mbuf *m)
{
	struct rtentry *rt;
	struct gif_softc *sc = ifp->if_softc;
	struct sockaddr_in *sin_src = (struct sockaddr_in *)sc->gif_psrc;
	struct sockaddr_in *sin_dst = (struct sockaddr_in *)sc->gif_pdst;
	struct ip iphdr;	/* capsule IP header, host byte ordered */
	int proto, error;
	u_int8_t tos;
	union {
		struct sockaddr		dst;
		struct sockaddr_in	dst4;
	} u;

	if (sin_src == NULL || sin_dst == NULL ||
	    sin_src->sin_family != AF_INET ||
	    sin_dst->sin_family != AF_INET) {
		m_freem(m);
		return EAFNOSUPPORT;
	}

	switch (family) {
#ifdef INET
	case AF_INET:
	    {
		const struct ip *ip;

		proto = IPPROTO_IPV4;
		if (m->m_len < sizeof(*ip)) {
			m = m_pullup(m, sizeof(*ip));
			if (m == NULL)
				return ENOBUFS;
		}
		ip = mtod(m, const struct ip *);
		tos = ip->ip_tos;
		break;
	    }
#endif /* INET */
#ifdef INET6
	case AF_INET6:
	    {
		const struct ip6_hdr *ip6;
		proto = IPPROTO_IPV6;
		if (m->m_len < sizeof(*ip6)) {
			m = m_pullup(m, sizeof(*ip6));
			if (m == NULL)
				return ENOBUFS;
		}
		ip6 = mtod(m, const struct ip6_hdr *);
		tos = (ntohl(ip6->ip6_flow) >> 20) & 0xff;
		break;
	    }
#endif /* INET6 */
	default:
#ifdef DEBUG
		printf("in_gif_output: warning: unknown family %d passed\n",
			family);
#endif
		m_freem(m);
		return EAFNOSUPPORT;
	}

	memset(&iphdr, 0, sizeof(iphdr));
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
	iphdr.ip_ttl = ip_gif_ttl;
	iphdr.ip_len = htons(m->m_pkthdr.len + sizeof(struct ip));
	if (ifp->if_flags & IFF_LINK1)
		ip_ecn_ingress(ECN_ALLOWED, &iphdr.ip_tos, &tos);
	else
		ip_ecn_ingress(ECN_NOCARE, &iphdr.ip_tos, &tos);

	/* prepend new IP header */
	M_PREPEND(m, sizeof(struct ip), M_DONTWAIT);
	/* XXX Is m_pullup really necessary after M_PREPEND? */
	if (m != NULL && M_UNWRITABLE(m, sizeof(struct ip)))
		m = m_pullup(m, sizeof(struct ip));
	if (m == NULL)
		return ENOBUFS;
	bcopy(&iphdr, mtod(m, struct ip *), sizeof(struct ip));

	sockaddr_in_init(&u.dst4, &sin_dst->sin_addr, 0);
	if ((rt = rtcache_lookup(&sc->gif_ro, &u.dst)) == NULL) {
		m_freem(m);
		return ENETUNREACH;
	}

	/* If the route constitutes infinite encapsulation, punt. */
	if (rt->rt_ifp == ifp) {
		rtcache_free(&sc->gif_ro);
		m_freem(m);
		return ENETUNREACH;	/*XXX*/
	}

	error = ip_output(m, NULL, &sc->gif_ro, 0, NULL, NULL);
	return (error);
}

void
in_gif_input(struct mbuf *m, ...)
{
	int off, proto;
	struct ifnet *gifp = NULL;
	const struct ip *ip;
	va_list ap;
	int af;
	u_int8_t otos;

	va_start(ap, m);
	off = va_arg(ap, int);
	proto = va_arg(ap, int);
	va_end(ap);

	ip = mtod(m, const struct ip *);

	gifp = (struct ifnet *)encap_getarg(m);

	if (gifp == NULL || (gifp->if_flags & IFF_UP) == 0) {
		m_freem(m);
		ip_statinc(IP_STAT_NOGIF);
		return;
	}
#ifndef GIF_ENCAPCHECK
	if (!gif_validate4(ip, gifp->if_softc, m->m_pkthdr.rcvif)) {
		m_freem(m);
		ip_statinc(IP_STAT_NOGIF);
		return;
	}
#endif

	otos = ip->ip_tos;
	m_adj(m, off);

	switch (proto) {
#ifdef INET
	case IPPROTO_IPV4:
	    {
		struct ip *xip;
		af = AF_INET;
		if (M_UNWRITABLE(m, sizeof(*xip))) {
			if ((m = m_pullup(m, sizeof(*xip))) == NULL)
				return;
		}
		xip = mtod(m, struct ip *);
		if (gifp->if_flags & IFF_LINK1)
			ip_ecn_egress(ECN_ALLOWED, &otos, &xip->ip_tos);
		else
			ip_ecn_egress(ECN_NOCARE, &otos, &xip->ip_tos);
		break;
	    }
#endif
#ifdef INET6
	case IPPROTO_IPV6:
	    {
		struct ip6_hdr *ip6;
		u_int8_t itos;
		af = AF_INET6;
		if (M_UNWRITABLE(m, sizeof(*ip6))) {
			if ((m = m_pullup(m, sizeof(*ip6))) == NULL)
				return;
		}
		ip6 = mtod(m, struct ip6_hdr *);
		itos = (ntohl(ip6->ip6_flow) >> 20) & 0xff;
		if (gifp->if_flags & IFF_LINK1)
			ip_ecn_egress(ECN_ALLOWED, &otos, &itos);
		else
			ip_ecn_egress(ECN_NOCARE, &otos, &itos);
		ip6->ip6_flow &= ~htonl(0xff << 20);
		ip6->ip6_flow |= htonl((u_int32_t)itos << 20);
		break;
	    }
#endif /* INET6 */
	default:
		ip_statinc(IP_STAT_NOGIF);
		m_freem(m);
		return;
	}
	gif_input(m, af, gifp);
	return;
}

/*
 * validate outer address.
 */
static int
gif_validate4(const struct ip *ip, struct gif_softc *sc, struct ifnet *ifp)
{
	struct sockaddr_in *src, *dst;
	struct in_ifaddr *ia4;

	src = (struct sockaddr_in *)sc->gif_psrc;
	dst = (struct sockaddr_in *)sc->gif_pdst;

	/* check for address match */
	if (src->sin_addr.s_addr != ip->ip_dst.s_addr ||
	    dst->sin_addr.s_addr != ip->ip_src.s_addr)
		return 0;

	/* martian filters on outer source - NOT done in ip_input! */
	if (IN_MULTICAST(ip->ip_src.s_addr))
		return 0;
	switch ((ntohl(ip->ip_src.s_addr) & 0xff000000) >> 24) {
	case 0: case 127: case 255:
		return 0;
	}
	/* reject packets with broadcast on source */
	TAILQ_FOREACH(ia4, &in_ifaddrhead, ia_list) {
		if ((ia4->ia_ifa.ifa_ifp->if_flags & IFF_BROADCAST) == 0)
			continue;
		if (ip->ip_src.s_addr == ia4->ia_broadaddr.sin_addr.s_addr)
			return 0;
	}

	/* ingress filters on outer source */
	if ((sc->gif_if.if_flags & IFF_LINK2) == 0 && ifp) {
		union {
			struct sockaddr sa;
			struct sockaddr_in sin;
		} u;
		struct rtentry *rt;

		sockaddr_in_init(&u.sin, &ip->ip_src, 0);
		rt = rtalloc1(&u.sa, 0);
		if (rt == NULL || rt->rt_ifp != ifp) {
#if 0
			log(LOG_WARNING, "%s: packet from 0x%x dropped "
			    "due to ingress filter\n", if_name(&sc->gif_if),
			    (u_int32_t)ntohl(u.sin.sin_addr.s_addr));
#endif
			if (rt != NULL)
				rtfree(rt);
			return 0;
		}
		rtfree(rt);
	}

	return 32 * 2;
}

#ifdef GIF_ENCAPCHECK
/*
 * we know that we are in IFF_UP, outer address available, and outer family
 * matched the physical addr family.  see gif_encapcheck().
 */
int
gif_encapcheck4(struct mbuf *m, int off, int proto, void *arg)
{
	struct ip ip;
	struct gif_softc *sc;
	struct ifnet *ifp;

	/* sanity check done in caller */
	sc = arg;

	m_copydata(m, 0, sizeof(ip), &ip);
	ifp = ((m->m_flags & M_PKTHDR) != 0) ? m->m_pkthdr.rcvif : NULL;

	return gif_validate4(&ip, sc, ifp);
}
#endif

int
in_gif_attach(struct gif_softc *sc)
{
#ifndef GIF_ENCAPCHECK
	struct sockaddr_in mask4;

	memset(&mask4, 0, sizeof(mask4));
	mask4.sin_len = sizeof(struct sockaddr_in);
	mask4.sin_addr.s_addr = ~0;

	if (!sc->gif_psrc || !sc->gif_pdst)
		return EINVAL;
	sc->encap_cookie4 = encap_attach(AF_INET, -1, sc->gif_psrc,
	    (struct sockaddr *)&mask4, sc->gif_pdst, (struct sockaddr *)&mask4,
	    (const struct protosw *)&in_gif_protosw, sc);
#else
	sc->encap_cookie4 = encap_attach_func(AF_INET, -1, gif_encapcheck,
	    &in_gif_protosw, sc);
#endif
	if (sc->encap_cookie4 == NULL)
		return EEXIST;
	return 0;
}

int
in_gif_detach(struct gif_softc *sc)
{
	int error;

	error = encap_detach(sc->encap_cookie4);
	if (error == 0)
		sc->encap_cookie4 = NULL;

	rtcache_free(&sc->gif_ro);

	return error;
}
