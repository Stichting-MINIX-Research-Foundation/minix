/*	$NetBSD: if_stf.c,v 1.82 2015/08/24 22:21:26 pooka Exp $	*/
/*	$KAME: if_stf.c,v 1.62 2001/06/07 22:32:16 itojun Exp $ */

/*
 * Copyright (C) 2000 WIDE Project.
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

/*
 * 6to4 interface, based on RFC3056.
 *
 * 6to4 interface is NOT capable of link-layer (I mean, IPv4) multicasting.
 * There is no address mapping defined from IPv6 multicast address to IPv4
 * address.  Therefore, we do not have IFF_MULTICAST on the interface.
 *
 * Due to the lack of address mapping for link-local addresses, we cannot
 * throw packets toward link-local addresses (fe80::x).  Also, we cannot throw
 * packets to link-local multicast addresses (ff02::x).
 *
 * Here are interesting symptoms due to the lack of link-local address:
 *
 * Unicast routing exchange:
 * - RIPng: Impossible.  Uses link-local multicast packet toward ff02::9,
 *   and link-local addresses as nexthop.
 * - OSPFv6: Impossible.  OSPFv6 assumes that there's link-local address
 *   assigned to the link, and makes use of them.  Also, HELLO packets use
 *   link-local multicast addresses (ff02::5 and ff02::6).
 * - BGP4+: Maybe.  You can only use global address as nexthop, and global
 *   address as TCP endpoint address.
 *
 * Multicast routing protocols:
 * - PIM: Hello packet cannot be used to discover adjacent PIM routers.
 *   Adjacent PIM routers must be configured manually (is it really spec-wise
 *   correct thing to do?).
 *
 * ICMPv6:
 * - Redirects cannot be used due to the lack of link-local address.
 *
 * stf interface does not have, and will not need, a link-local address.
 * It seems to have no real benefit and does not help the above symptoms much.
 * Even if we assign link-locals to interface, we cannot really
 * use link-local unicast/multicast on top of 6to4 cloud (since there's no
 * encapsulation defined for link-local address), and the above analysis does
 * not change.  RFC3056 does not mandate the assignment of link-local address
 * either.
 *
 * 6to4 interface has security issues.  Refer to
 * http://playground.iijlab.net/i-d/draft-itojun-ipv6-transition-abuse-00.txt
 * for details.  The code tries to filter out some of malicious packets.
 * Note that there is no way to be 100% secure.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_stf.c,v 1.82 2015/08/24 22:21:26 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#endif

#ifndef INET6
	#error "pseudo-device stf requires options INET6"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/queue.h>
#include <sys/syslog.h>

#include <sys/cpu.h>

#include <net/if.h>
#include <net/route.h>
#include <net/netisr.h>
#include <net/if_types.h>
#include <net/if_stf.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_var.h>

#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_gif.h>
#include <netinet6/in6_var.h>
#include <netinet/ip_ecn.h>

#include <netinet/ip_encap.h>

#include <net/net_osdep.h>

#include "stf.h"
#include "gif.h"	/*XXX*/

#include <net/bpf.h>

#if NGIF > 0
#include <net/if_gif.h>
#endif

#include "ioconf.h"

#define IN6_IS_ADDR_6TO4(x)	(ntohs((x)->s6_addr16[0]) == 0x2002)
#define GET_V4(x)	((const struct in_addr *)(&(x)->s6_addr16[1]))

struct stf_softc {
	struct ifnet	sc_if;	   /* common area */
	struct route	sc_ro;
	const struct encaptab *encap_cookie;
	LIST_ENTRY(stf_softc) sc_list;
};

static LIST_HEAD(, stf_softc) stf_softc_list;

static int	stf_clone_create(struct if_clone *, int);
static int	stf_clone_destroy(struct ifnet *);

struct if_clone stf_cloner =
    IF_CLONE_INITIALIZER("stf", stf_clone_create, stf_clone_destroy);

#if NGIF > 0
extern int ip_gif_ttl;	/*XXX*/
#else
static int ip_gif_ttl = 40;	/*XXX*/
#endif

extern struct domain inetdomain;

static const struct protosw in_stf_protosw =
{
	.pr_type	= SOCK_RAW,
	.pr_domain	= &inetdomain,
	.pr_protocol	= IPPROTO_IPV6,
	.pr_flags	= PR_ATOMIC|PR_ADDR,
	.pr_input	= in_stf_input,
	.pr_output	= rip_output,
	.pr_ctlinput	= NULL,
	.pr_ctloutput	= rip_ctloutput,
	.pr_usrreqs	= &rip_usrreqs,
};

static int stf_encapcheck(struct mbuf *, int, int, void *);
static struct in6_ifaddr *stf_getsrcifa6(struct ifnet *);
static int stf_output(struct ifnet *, struct mbuf *, const struct sockaddr *,
	struct rtentry *);
static int isrfc1918addr(const struct in_addr *);
static int stf_checkaddr4(struct stf_softc *, const struct in_addr *,
	struct ifnet *);
static int stf_checkaddr6(struct stf_softc *, const struct in6_addr *,
	struct ifnet *);
static void stf_rtrequest(int, struct rtentry *, const struct rt_addrinfo *);
static int stf_ioctl(struct ifnet *, u_long, void *);

/* ARGSUSED */
void
stfattach(int count)
{

	LIST_INIT(&stf_softc_list);
	if_clone_attach(&stf_cloner);
}

static int
stf_clone_create(struct if_clone *ifc, int unit)
{
	struct stf_softc *sc;

	if (LIST_FIRST(&stf_softc_list) != NULL) {
		/* Only one stf interface is allowed. */
		return (EEXIST);
	}

	sc = malloc(sizeof(struct stf_softc), M_DEVBUF, M_WAIT|M_ZERO);

	if_initname(&sc->sc_if, ifc->ifc_name, unit);

	sc->encap_cookie = encap_attach_func(AF_INET, IPPROTO_IPV6,
	    stf_encapcheck, &in_stf_protosw, sc);
	if (sc->encap_cookie == NULL) {
		printf("%s: unable to attach encap\n", if_name(&sc->sc_if));
		free(sc, M_DEVBUF);
		return (EIO);	/* XXX */
	}

	sc->sc_if.if_mtu    = STF_MTU;
	sc->sc_if.if_flags  = 0;
	sc->sc_if.if_ioctl  = stf_ioctl;
	sc->sc_if.if_output = stf_output;
	sc->sc_if.if_type   = IFT_STF;
	sc->sc_if.if_dlt    = DLT_NULL;
	if_attach(&sc->sc_if);
	if_alloc_sadl(&sc->sc_if);
	bpf_attach(&sc->sc_if, DLT_NULL, sizeof(u_int));
	LIST_INSERT_HEAD(&stf_softc_list, sc, sc_list);
	return (0);
}

static int
stf_clone_destroy(struct ifnet *ifp)
{
	struct stf_softc *sc = (void *) ifp;

	LIST_REMOVE(sc, sc_list);
	encap_detach(sc->encap_cookie);
	bpf_detach(ifp);
	if_detach(ifp);
	rtcache_free(&sc->sc_ro);
	free(sc, M_DEVBUF);

	return (0);
}

static int
stf_encapcheck(struct mbuf *m, int off, int proto, void *arg)
{
	struct ip ip;
	struct in6_ifaddr *ia6;
	struct stf_softc *sc;
	struct in_addr a, b;

	sc = (struct stf_softc *)arg;
	if (sc == NULL)
		return 0;

	if ((sc->sc_if.if_flags & IFF_UP) == 0)
		return 0;

	/* IFF_LINK0 means "no decapsulation" */
	if ((sc->sc_if.if_flags & IFF_LINK0) != 0)
		return 0;

	if (proto != IPPROTO_IPV6)
		return 0;

	m_copydata(m, 0, sizeof(ip), (void *)&ip);

	if (ip.ip_v != 4)
		return 0;

	ia6 = stf_getsrcifa6(&sc->sc_if);
	if (ia6 == NULL)
		return 0;

	/*
	 * check if IPv4 dst matches the IPv4 address derived from the
	 * local 6to4 address.
	 * success on: dst = 10.1.1.1, ia6->ia_addr = 2002:0a01:0101:...
	 */
	if (memcmp(GET_V4(&ia6->ia_addr.sin6_addr), &ip.ip_dst,
	    sizeof(ip.ip_dst)) != 0)
		return 0;

	/*
	 * check if IPv4 src matches the IPv4 address derived from the
	 * local 6to4 address masked by prefixmask.
	 * success on: src = 10.1.1.1, ia6->ia_addr = 2002:0a00:.../24
	 * fail on: src = 10.1.1.1, ia6->ia_addr = 2002:0b00:.../24
	 */
	memset(&a, 0, sizeof(a));
	a.s_addr = GET_V4(&ia6->ia_addr.sin6_addr)->s_addr;
	a.s_addr &= GET_V4(&ia6->ia_prefixmask.sin6_addr)->s_addr;
	b = ip.ip_src;
	b.s_addr &= GET_V4(&ia6->ia_prefixmask.sin6_addr)->s_addr;
	if (a.s_addr != b.s_addr)
		return 0;

	/* stf interface makes single side match only */
	return 32;
}

static struct in6_ifaddr *
stf_getsrcifa6(struct ifnet *ifp)
{
	struct ifaddr *ifa;
	struct in_ifaddr *ia4;
	struct sockaddr_in6 *sin6;
	struct in_addr in;

	IFADDR_FOREACH(ifa, ifp)
	{
		if (ifa->ifa_addr == NULL)
			continue;
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		sin6 = (struct sockaddr_in6 *)ifa->ifa_addr;
		if (!IN6_IS_ADDR_6TO4(&sin6->sin6_addr))
			continue;

		memcpy(&in, GET_V4(&sin6->sin6_addr), sizeof(in));
		INADDR_TO_IA(in, ia4);
		if (ia4 == NULL)
			continue;

		return (struct in6_ifaddr *)ifa;
	}

	return NULL;
}

static int
stf_output(struct ifnet *ifp, struct mbuf *m, const struct sockaddr *dst,
    struct rtentry *rt0)
{
	struct rtentry *rt;
	struct stf_softc *sc;
	const struct sockaddr_in6 *dst6;
	const struct in_addr *in4;
	uint8_t tos;
	struct ip *ip;
	struct ip6_hdr *ip6;
	struct in6_ifaddr *ia6;
	union {
		struct sockaddr		dst;
		struct sockaddr_in	dst4;
	} u;

	sc = (struct stf_softc*)ifp;
	dst6 = (const struct sockaddr_in6 *)dst;

	/* just in case */
	if ((ifp->if_flags & IFF_UP) == 0) {
		m_freem(m);
		return ENETDOWN;
	}

	/*
	 * If we don't have an ip4 address that match my inner ip6 address,
	 * we shouldn't generate output.  Without this check, we'll end up
	 * using wrong IPv4 source.
	 */
	ia6 = stf_getsrcifa6(ifp);
	if (ia6 == NULL) {
		m_freem(m);
		ifp->if_oerrors++;
		return ENETDOWN;
	}

	if (m->m_len < sizeof(*ip6)) {
		m = m_pullup(m, sizeof(*ip6));
		if (m == NULL) {
			ifp->if_oerrors++;
			return ENOBUFS;
		}
	}
	ip6 = mtod(m, struct ip6_hdr *);
	tos = (ntohl(ip6->ip6_flow) >> 20) & 0xff;

	/*
	 * Pickup the right outer dst addr from the list of candidates.
	 * ip6_dst has priority as it may be able to give us shorter IPv4 hops.
	 */
	if (IN6_IS_ADDR_6TO4(&ip6->ip6_dst))
		in4 = GET_V4(&ip6->ip6_dst);
	else if (IN6_IS_ADDR_6TO4(&dst6->sin6_addr))
		in4 = GET_V4(&dst6->sin6_addr);
	else {
		m_freem(m);
		ifp->if_oerrors++;
		return ENETUNREACH;
	}

	bpf_mtap_af(ifp, AF_INET6, m);

	M_PREPEND(m, sizeof(struct ip), M_DONTWAIT);
	if (m && m->m_len < sizeof(struct ip))
		m = m_pullup(m, sizeof(struct ip));
	if (m == NULL) {
		ifp->if_oerrors++;
		return ENOBUFS;
	}
	ip = mtod(m, struct ip *);

	memset(ip, 0, sizeof(*ip));

	bcopy(GET_V4(&((struct sockaddr_in6 *)&ia6->ia_addr)->sin6_addr),
	    &ip->ip_src, sizeof(ip->ip_src));
	memcpy(&ip->ip_dst, in4, sizeof(ip->ip_dst));
	ip->ip_p = IPPROTO_IPV6;
	ip->ip_ttl = ip_gif_ttl;	/*XXX*/
	ip->ip_len = htons(m->m_pkthdr.len);
	if (ifp->if_flags & IFF_LINK1)
		ip_ecn_ingress(ECN_ALLOWED, &ip->ip_tos, &tos);
	else
		ip_ecn_ingress(ECN_NOCARE, &ip->ip_tos, &tos);

	sockaddr_in_init(&u.dst4, &ip->ip_dst, 0);
	if ((rt = rtcache_lookup(&sc->sc_ro, &u.dst)) == NULL) {
		m_freem(m);
		ifp->if_oerrors++;
		return ENETUNREACH;
	}

	/* If the route constitutes infinite encapsulation, punt. */
	if (rt->rt_ifp == ifp) {
		rtcache_free(&sc->sc_ro);
		m_freem(m);
		ifp->if_oerrors++;
		return ENETUNREACH;
	}

	ifp->if_opackets++;
	ifp->if_obytes += m->m_pkthdr.len - sizeof(struct ip);
	return ip_output(m, NULL, &sc->sc_ro, 0, NULL, NULL);
}

static int
isrfc1918addr(const struct in_addr *in)
{
	/*
	 * returns 1 if private address range:
	 * 10.0.0.0/8 172.16.0.0/12 192.168.0.0/16
	 */
	if ((ntohl(in->s_addr) & 0xff000000) >> 24 == 10 ||
	    (ntohl(in->s_addr) & 0xfff00000) >> 16 == 172 * 256 + 16 ||
	    (ntohl(in->s_addr) & 0xffff0000) >> 16 == 192 * 256 + 168)
		return 1;

	return 0;
}

static int
stf_checkaddr4(struct stf_softc *sc, const struct in_addr *in,
    struct ifnet *inifp /*incoming interface*/)
{
	struct in_ifaddr *ia4;

	/*
	 * reject packets with the following address:
	 * 224.0.0.0/4 0.0.0.0/8 127.0.0.0/8 255.0.0.0/8
	 */
	if (IN_MULTICAST(in->s_addr))
		return -1;
	switch ((ntohl(in->s_addr) & 0xff000000) >> 24) {
	case 0: case 127: case 255:
		return -1;
	}

	/*
	 * reject packets with private address range.
	 * (requirement from RFC3056 section 2 1st paragraph)
	 */
	if (isrfc1918addr(in))
		return -1;

	/*
	 * reject packet with IPv4 link-local (169.254.0.0/16),
	 * as suggested in draft-savola-v6ops-6to4-security-00.txt
	 */
	if (((ntohl(in->s_addr) & 0xff000000) >> 24) == 169 &&
	    ((ntohl(in->s_addr) & 0x00ff0000) >> 16) == 254)
		return -1;

	/*
	 * reject packets with broadcast
	 */
	TAILQ_FOREACH(ia4, &in_ifaddrhead, ia_list)
	{
		if ((ia4->ia_ifa.ifa_ifp->if_flags & IFF_BROADCAST) == 0)
			continue;
		if (in->s_addr == ia4->ia_broadaddr.sin_addr.s_addr)
			return -1;
	}

	/*
	 * perform ingress filter
	 */
	if (sc && (sc->sc_if.if_flags & IFF_LINK2) == 0 && inifp) {
		struct sockaddr_in sin;
		struct rtentry *rt;

		memset(&sin, 0, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_len = sizeof(struct sockaddr_in);
		sin.sin_addr = *in;
		rt = rtalloc1((struct sockaddr *)&sin, 0);
		if (!rt || rt->rt_ifp != inifp) {
#if 0
			log(LOG_WARNING, "%s: packet from 0x%x dropped "
			    "due to ingress filter\n", if_name(&sc->sc_if),
			    (uint32_t)ntohl(sin.sin_addr.s_addr));
#endif
			if (rt)
				rtfree(rt);
			return -1;
		}
		rtfree(rt);
	}

	return 0;
}

static int
stf_checkaddr6(struct stf_softc *sc, const struct in6_addr *in6,
    struct ifnet *inifp /*incoming interface*/)
{

	/*
	 * check 6to4 addresses
	 */
	if (IN6_IS_ADDR_6TO4(in6))
		return stf_checkaddr4(sc, GET_V4(in6), inifp);

	/*
	 * reject anything that look suspicious.  the test is implemented
	 * in ip6_input too, but we check here as well to
	 * (1) reject bad packets earlier, and
	 * (2) to be safe against future ip6_input change.
	 */
	if (IN6_IS_ADDR_V4COMPAT(in6) || IN6_IS_ADDR_V4MAPPED(in6))
		return -1;

	/*
	 * reject link-local and site-local unicast
	 * as suggested in draft-savola-v6ops-6to4-security-00.txt
	 */
	if (IN6_IS_ADDR_LINKLOCAL(in6) || IN6_IS_ADDR_SITELOCAL(in6))
		return -1;

	/*
	 * reject node-local and link-local multicast
	 * as suggested in draft-savola-v6ops-6to4-security-00.txt
	 */
	if (IN6_IS_ADDR_MC_NODELOCAL(in6) || IN6_IS_ADDR_MC_LINKLOCAL(in6))
		return -1;

	return 0;
}

void
in_stf_input(struct mbuf *m, ...)
{
	int s, off, proto;
	struct stf_softc *sc;
	struct ip *ip;
	struct ip6_hdr *ip6;
	uint8_t otos, itos;
	struct ifnet *ifp;
	size_t pktlen;
	va_list ap;

	va_start(ap, m);
	off = va_arg(ap, int);
	proto = va_arg(ap, int);
	va_end(ap);

	if (proto != IPPROTO_IPV6) {
		m_freem(m);
		return;
	}

	ip = mtod(m, struct ip *);

	sc = (struct stf_softc *)encap_getarg(m);

	if (sc == NULL || (sc->sc_if.if_flags & IFF_UP) == 0) {
		m_freem(m);
		return;
	}

	ifp = &sc->sc_if;

	/*
	 * perform sanity check against outer src/dst.
	 * for source, perform ingress filter as well.
	 */
	if (stf_checkaddr4(sc, &ip->ip_dst, NULL) < 0 ||
	    stf_checkaddr4(sc, &ip->ip_src, m->m_pkthdr.rcvif) < 0) {
		m_freem(m);
		return;
	}

	otos = ip->ip_tos;
	m_adj(m, off);

	if (m->m_len < sizeof(*ip6)) {
		m = m_pullup(m, sizeof(*ip6));
		if (!m)
			return;
	}
	ip6 = mtod(m, struct ip6_hdr *);

	/*
	 * perform sanity check against inner src/dst.
	 * for source, perform ingress filter as well.
	 */
	if (stf_checkaddr6(sc, &ip6->ip6_dst, NULL) < 0 ||
	    stf_checkaddr6(sc, &ip6->ip6_src, m->m_pkthdr.rcvif) < 0) {
		m_freem(m);
		return;
	}

	itos = (ntohl(ip6->ip6_flow) >> 20) & 0xff;
	if ((ifp->if_flags & IFF_LINK1) != 0)
		ip_ecn_egress(ECN_ALLOWED, &otos, &itos);
	else
		ip_ecn_egress(ECN_NOCARE, &otos, &itos);
	ip6->ip6_flow &= ~htonl(0xff << 20);
	ip6->ip6_flow |= htonl((uint32_t)itos << 20);

	pktlen = m->m_pkthdr.len;
	m->m_pkthdr.rcvif = ifp;

	bpf_mtap_af(ifp, AF_INET6, m);

	/*
	 * Put the packet to the network layer input queue according to the
	 * specified address family.
	 * See net/if_gif.c for possible issues with packet processing
	 * reorder due to extra queueing.
	 */

	s = splnet();
	if (__predict_true(pktq_enqueue(ip6_pktq, m, 0))) {
		ifp->if_ipackets++;
		ifp->if_ibytes += pktlen;
	} else {
		m_freem(m);
	}
	splx(s);
}

/* ARGSUSED */
static void
stf_rtrequest(int cmd, struct rtentry *rt,
    const struct rt_addrinfo *info)
{
	if (rt != NULL) {
		struct stf_softc *sc;

		sc = LIST_FIRST(&stf_softc_list);
		rt->rt_rmx.rmx_mtu = (sc != NULL) ? sc->sc_if.if_mtu : STF_MTU;
	}
}

static int
stf_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct ifaddr		*ifa;
	struct ifreq		*ifr = data;
	struct sockaddr_in6	*sin6;
	int			error;

	error = 0;
	switch (cmd) {
	case SIOCINITIFADDR:
		ifa = (struct ifaddr *)data;
		if (ifa == NULL || ifa->ifa_addr->sa_family != AF_INET6) {
			error = EAFNOSUPPORT;
			break;
		}
		sin6 = (struct sockaddr_in6 *)ifa->ifa_addr;
		if (IN6_IS_ADDR_6TO4(&sin6->sin6_addr) &&
		    !isrfc1918addr(GET_V4(&sin6->sin6_addr))) {
			ifa->ifa_rtrequest = stf_rtrequest;
			ifp->if_flags |= IFF_UP;
		} else
			error = EINVAL;
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifr != NULL &&
		    ifreq_getaddr(cmd, ifr)->sa_family == AF_INET6)
			;
		else
			error = EAFNOSUPPORT;
		break;

	case SIOCSIFMTU:
		if (ifr->ifr_mtu < STF_MTU_MIN || ifr->ifr_mtu > STF_MTU_MAX)
			return EINVAL;
		else if ((error = ifioctl_common(ifp, cmd, data)) == ENETRESET)
			error = 0;
		break;

	default:
		error = ifioctl_common(ifp, cmd, data);
		break;
	}

	return error;
}
