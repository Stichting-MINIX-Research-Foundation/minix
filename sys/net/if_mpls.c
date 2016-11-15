/*	$NetBSD: if_mpls.c,v 1.19 2015/08/24 22:21:26 pooka Exp $ */

/*
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Mihai Chelaru <kefren@NetBSD.org>
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_mpls.c,v 1.19 2015/08/24 22:21:26 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#include "opt_mpls.h"
#endif

#include <sys/param.h>

#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/sysctl.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#endif

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/in6_var.h>
#include <netinet6/ip6_var.h>
#endif

#include <netmpls/mpls.h>
#include <netmpls/mpls_var.h>

#include "if_mpls.h"

#include "ioconf.h"

#define TRIM_LABEL do { \
	m_adj(m, sizeof(union mpls_shim)); \
	if (m->m_len < sizeof(union mpls_shim) && \
	    (m = m_pullup(m, sizeof(union mpls_shim))) == NULL) \
		goto done; \
	dst.smpls_addr.s_addr = ntohl(mtod(m, union mpls_shim *)->s_addr); \
	} while (/* CONSTCOND */ 0)


static int mpls_clone_create(struct if_clone *, int);
static int mpls_clone_destroy(struct ifnet *);

static struct if_clone mpls_if_cloner =
	IF_CLONE_INITIALIZER("mpls", mpls_clone_create, mpls_clone_destroy);


static void mpls_input(struct ifnet *, struct mbuf *);
static int mpls_output(struct ifnet *, struct mbuf *, const struct sockaddr *,
	struct rtentry *);
static int mpls_ioctl(struct ifnet *, u_long, void *);
static int mpls_send_frame(struct mbuf *, struct ifnet *, struct rtentry *);
static int mpls_lse(struct mbuf *);

#ifdef INET
static int mpls_unlabel_inet(struct mbuf *);
static struct mbuf *mpls_label_inet(struct mbuf *, union mpls_shim *, uint);
#endif

#ifdef INET6
static int mpls_unlabel_inet6(struct mbuf *);
static struct mbuf *mpls_label_inet6(struct mbuf *, union mpls_shim *, uint);
#endif

static struct mbuf *mpls_prepend_shim(struct mbuf *, union mpls_shim *);

extern int mpls_defttl, mpls_mapttl_inet, mpls_mapttl_inet6, mpls_icmp_respond,
	mpls_forwarding, mpls_frame_accept, mpls_mapprec_inet, mpls_mapclass_inet6,
	mpls_rfc4182;

/* ARGSUSED */
void
ifmplsattach(int count)
{
	if_clone_attach(&mpls_if_cloner);
}

static int
mpls_clone_create(struct if_clone *ifc, int unit)
{
	struct mpls_softc *sc;

	sc = malloc(sizeof(*sc), M_DEVBUF, M_WAITOK | M_ZERO);

	if_initname(&sc->sc_if, ifc->ifc_name, unit);
	sc->sc_if.if_softc = sc;
	sc->sc_if.if_type = IFT_MPLS;
	sc->sc_if.if_addrlen = 0;
	sc->sc_if.if_hdrlen = sizeof(union mpls_shim);
	sc->sc_if.if_dlt = DLT_NULL;
	sc->sc_if.if_mtu = 1500;
	sc->sc_if.if_flags = 0;
	sc->sc_if.if_input = mpls_input;
	sc->sc_if.if_output = mpls_output;
	sc->sc_if.if_ioctl = mpls_ioctl;

	if_attach(&sc->sc_if);
	if_alloc_sadl(&sc->sc_if);
	bpf_attach(&sc->sc_if, DLT_NULL, sizeof(uint32_t));
	return 0;
}

static int
mpls_clone_destroy(struct ifnet *ifp)
{
	int s;

	bpf_detach(ifp);

	s = splnet();
	if_detach(ifp);
	splx(s);

	free(ifp->if_softc, M_DEVBUF);
	return 0;
}

static void
mpls_input(struct ifnet *ifp, struct mbuf *m)
{
#if 0
	/*
	 * TODO - kefren
	 * I'd love to unshim the packet, guess family
	 * and pass it to bpf
	 */
	bpf_mtap_af(ifp, AF_MPLS, m);
#endif

	mpls_lse(m);
}

void
mplsintr(void)
{
	struct mbuf *m;
	int s;

	while (!IF_IS_EMPTY(&mplsintrq)) {
		s = splnet();
		IF_DEQUEUE(&mplsintrq, m);
		splx(s);

		if (!m)
			return;

		if (((m->m_flags & M_PKTHDR) == 0) ||
		    (m->m_pkthdr.rcvif == 0))
			panic("mplsintr(): no pkthdr or rcvif");

#ifdef MBUFTRACE
		m_claimm(m, &mpls_owner);
#endif
		mpls_input(m->m_pkthdr.rcvif, m);
	}
}

/*
 * prepend shim and deliver
 */
static int
mpls_output(struct ifnet *ifp, struct mbuf *m, const struct sockaddr *dst, struct rtentry *rt)
{
	union mpls_shim mh, *pms;
	struct rtentry *rt1;
	int err;
	uint psize = sizeof(struct sockaddr_mpls);

	KASSERT(KERNEL_LOCKED_P());

	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING)) {
		m_freem(m);
		return ENETDOWN;
	}

	if (rt_gettag(rt) == NULL || rt_gettag(rt)->sa_family != AF_MPLS) {
		m_freem(m);
		return EINVAL;
	}

	bpf_mtap_af(ifp, dst->sa_family, m);

	memset(&mh, 0, sizeof(mh));
	mh.s_addr = MPLS_GETSADDR(rt);
	mh.shim.bos = 1;
	mh.shim.exp = 0;
	mh.shim.ttl = mpls_defttl;

	pms = &((struct sockaddr_mpls*)rt_gettag(rt))->smpls_addr;

	while (psize <= rt_gettag(rt)->sa_len - sizeof(mh)) {
		pms++;
		if (mh.shim.label != MPLS_LABEL_IMPLNULL &&
		    ((m = mpls_prepend_shim(m, &mh)) == NULL))
			return ENOBUFS;
		memset(&mh, 0, sizeof(mh));
		mh.s_addr = ntohl(pms->s_addr);
		mh.shim.bos = mh.shim.exp = 0;
		mh.shim.ttl = mpls_defttl;
		psize += sizeof(mh);
	}

	switch(dst->sa_family) {
#ifdef INET
	case AF_INET:
		m = mpls_label_inet(m, &mh, psize - sizeof(struct sockaddr_mpls));
		break;
#endif
#ifdef INET6
	case AF_INET6:
		m = mpls_label_inet6(m, &mh, psize - sizeof(struct sockaddr_mpls));
		break;
#endif
	default:
		m = mpls_prepend_shim(m, &mh);
		break;
	}

	if (m == NULL) {
		IF_DROP(&ifp->if_snd);
		ifp->if_oerrors++;
		return ENOBUFS;
	}

	ifp->if_opackets++;
	ifp->if_obytes += m->m_pkthdr.len;

	if ((rt1=rtalloc1(rt->rt_gateway, 1)) == NULL) {
		m_freem(m);
		return EHOSTUNREACH;
	}

	err = mpls_send_frame(m, rt1->rt_ifp, rt);
	rtfree(rt1);
	return err;
}

static int
mpls_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	int error = 0, s = splnet();
	struct ifreq *ifr = data;

	switch(cmd) {
	case SIOCINITIFADDR:
		ifp->if_flags |= IFF_UP | IFF_RUNNING;
		break;
	case SIOCSIFMTU:
		if (ifr != NULL && ifr->ifr_mtu < 576) {
			error = EINVAL;
			break;
		}
		/* FALLTHROUGH */
	case SIOCGIFMTU:
		if ((error = ifioctl_common(ifp, cmd, data)) == ENETRESET)
			error = 0;
		break;
	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, cmd, data)) != 0)
			break;
		if (ifp->if_flags & IFF_UP)
			ifp->if_flags |= IFF_RUNNING;
		break;
	default:
		error = ifioctl_common(ifp, cmd, data);
		break;
	}
	splx(s);
	return error;
}

/*
 * MPLS Label Switch Engine
 */
static int
mpls_lse(struct mbuf *m)
{
	struct sockaddr_mpls dst;
	union mpls_shim tshim, *htag;
	struct rtentry *rt = NULL;
	int error = ENOBUFS;
	uint psize = sizeof(struct sockaddr_mpls);
	bool push_back_alert = false;

	if (m->m_len < sizeof(union mpls_shim) &&
	    (m = m_pullup(m, sizeof(union mpls_shim))) == NULL)
		goto done;

	dst.smpls_len = sizeof(struct sockaddr_mpls);
	dst.smpls_family = AF_MPLS;
	dst.smpls_addr.s_addr = ntohl(mtod(m, union mpls_shim *)->s_addr);

	/* Check if we're accepting MPLS Frames */
	error = EINVAL;
	if (!mpls_frame_accept)
		goto done;

	/* TTL decrement */
	if ((m = mpls_ttl_dec(m)) == NULL)
		goto done;

	/* RFC 4182 */
	if (mpls_rfc4182 != 0)
		while((dst.smpls_addr.shim.label == MPLS_LABEL_IPV4NULL ||
		    dst.smpls_addr.shim.label == MPLS_LABEL_IPV6NULL) &&
		    __predict_false(dst.smpls_addr.shim.bos == 0))
			TRIM_LABEL;

	/* RFC 3032 Section 2.1 Page 4 */
	if (__predict_false(dst.smpls_addr.shim.label == MPLS_LABEL_RTALERT) &&
	    dst.smpls_addr.shim.bos == 0) {
		TRIM_LABEL;
		push_back_alert = true;
	}

	if (dst.smpls_addr.shim.label <= MPLS_LABEL_RESMAX) {
		/* Don't swap reserved labels */
		switch (dst.smpls_addr.shim.label) {
#ifdef INET
		case MPLS_LABEL_IPV4NULL:
			/* Pop shim and push mbuf to IP stack */
			if (dst.smpls_addr.shim.bos)
				error = mpls_unlabel_inet(m);
			break;
#endif
#ifdef INET6
		case MPLS_LABEL_IPV6NULL:
			/* Pop shim and push mbuf to IPv6 stack */
			if (dst.smpls_addr.shim.bos)
				error = mpls_unlabel_inet6(m);
			break;
#endif
		case MPLS_LABEL_RTALERT:	/* Yeah, I'm all alerted */
		case MPLS_LABEL_IMPLNULL:	/* This is logical only */
		default:			/* Rest are not allowed */
			break;
		}
		goto done;
	}

	/* Check if we should do MPLS forwarding */
	error = EHOSTUNREACH;
	if (!mpls_forwarding)
		goto done;

	/* Get a route to dst */
	dst.smpls_addr.shim.ttl =
	    dst.smpls_addr.shim.bos =
	    dst.smpls_addr.shim.exp = 0;
	dst.smpls_addr.s_addr = htonl(dst.smpls_addr.s_addr);
	if ((rt = rtalloc1((const struct sockaddr*)&dst, 1)) == NULL)
		goto done;

	/* MPLS packet with no MPLS tagged route ? */
	if ((rt->rt_flags & RTF_GATEWAY) == 0 ||
	     rt_gettag(rt) == NULL ||
	     rt_gettag(rt)->sa_family != AF_MPLS)
		goto done;

	tshim.s_addr = MPLS_GETSADDR(rt);

	/* Swap labels */
	if ((m->m_len < sizeof(union mpls_shim)) &&
	    (m = m_pullup(m, sizeof(union mpls_shim))) == 0) {
		error = ENOBUFS;
		goto done;
	}

	/* Replace only the label */
	htag = mtod(m, union mpls_shim *);
	htag->s_addr = ntohl(htag->s_addr);
	htag->shim.label = tshim.shim.label;
	htag->s_addr = htonl(htag->s_addr);

	/* check if there is anything more to prepend */
	htag = &((struct sockaddr_mpls*)rt_gettag(rt))->smpls_addr;
	while (psize <= rt_gettag(rt)->sa_len - sizeof(tshim)) {
		htag++;
		memset(&tshim, 0, sizeof(tshim));
		tshim.s_addr = ntohl(htag->s_addr);
		tshim.shim.bos = tshim.shim.exp = 0;
		tshim.shim.ttl = mpls_defttl;
		if (tshim.shim.label != MPLS_LABEL_IMPLNULL &&
		    ((m = mpls_prepend_shim(m, &tshim)) == NULL))
			return ENOBUFS;
		psize += sizeof(tshim);
	}

	if (__predict_false(push_back_alert == true)) {
		/* re-add the router alert label */
		memset(&tshim, 0, sizeof(tshim));
		tshim.s_addr = MPLS_LABEL_RTALERT;
		tshim.shim.bos = tshim.shim.exp = 0;
		tshim.shim.ttl = mpls_defttl;
		if ((m = mpls_prepend_shim(m, &tshim)) == NULL)
			return ENOBUFS;
	}

	error = mpls_send_frame(m, rt->rt_ifp, rt);

done:
	if (error != 0 && m != NULL)
		m_freem(m);
	if (rt != NULL)
		rtfree(rt);

	return error;
}

static int
mpls_send_frame(struct mbuf *m, struct ifnet *ifp, struct rtentry *rt)
{
	union mpls_shim msh;
	int ret;

	if ((rt->rt_flags & RTF_GATEWAY) == 0)
		return EHOSTUNREACH;

	rt->rt_use++;

	msh.s_addr = MPLS_GETSADDR(rt);
	if (msh.shim.label == MPLS_LABEL_IMPLNULL ||
	    (m->m_flags & (M_MCAST | M_BCAST))) {
		m_adj(m, sizeof(union mpls_shim));
		m->m_pkthdr.csum_flags = 0;
	}

	switch(ifp->if_type) {
	/* only these are supported for now */
	case IFT_ETHER:
	case IFT_TUNNEL:
	case IFT_LOOP:
#ifdef INET
		ret = ip_hresolv_output(ifp, m, rt->rt_gateway, rt);
#else
		KERNEL_LOCK(1, NULL);
		ret =  (*ifp->if_output)(ifp, m, rt->rt_gateway, rt);
		KERNEL_UNLOCK_ONE(NULL);
#endif
		return ret;
		break;
	default:
		return ENETUNREACH;
	}
	return 0;
}



#ifdef INET
static int
mpls_unlabel_inet(struct mbuf *m)
{
	struct ip *iph;
	union mpls_shim *ms;
	int iphlen;

	if (mpls_mapttl_inet || mpls_mapprec_inet) {

		/* get shim info */
		ms = mtod(m, union mpls_shim *);
		ms->s_addr = ntohl(ms->s_addr);

		/* and get rid of it */
		m_adj(m, sizeof(union mpls_shim));

		/* get ip header */
		if (m->m_len < sizeof (struct ip) &&
		    (m = m_pullup(m, sizeof(struct ip))) == NULL)
			return ENOBUFS;
		iph = mtod(m, struct ip *);
		iphlen = iph->ip_hl << 2;

		/* get it all */
		if (m->m_len < iphlen) {
			if ((m = m_pullup(m, iphlen)) == NULL)
				return ENOBUFS;
			iph = mtod(m, struct ip *);
		}

		/* check ipsum */
		if (in_cksum(m, iphlen) != 0) {
			m_freem(m);
			return EINVAL;
		}

		/* set IP ttl from MPLS ttl */
		if (mpls_mapttl_inet)
			iph->ip_ttl = ms->shim.ttl;

		/* set IP Precedence from MPLS Exp */
		if (mpls_mapprec_inet) {
			iph->ip_tos = (iph->ip_tos << 3) >> 3;
			iph->ip_tos |= ms->shim.exp << 5;
		}

		/* reset ipsum because we modified TTL and TOS */
		iph->ip_sum = 0;
		iph->ip_sum = in_cksum(m, iphlen);
	} else
		m_adj(m, sizeof(union mpls_shim));

	/* Put it on IP queue */
	if (__predict_false(!pktq_enqueue(ip_pktq, m, 0))) {
		m_freem(m);
		return ENOBUFS;
	}
	return 0;
}

/*
 * Prepend MPLS label
 */
static struct mbuf *
mpls_label_inet(struct mbuf *m, union mpls_shim *ms, uint offset)
{
	struct ip iphdr;

	if (mpls_mapttl_inet || mpls_mapprec_inet) {
		if ((m->m_len < sizeof(struct ip)) &&
		    (m = m_pullup(m, offset + sizeof(struct ip))) == 0)
			return NULL; /* XXX */
		m_copydata(m, offset, sizeof(struct ip), &iphdr);

		/* Map TTL */
		if (mpls_mapttl_inet)
			ms->shim.ttl = iphdr.ip_ttl;

		/* Copy IP precedence to EXP */
		if (mpls_mapprec_inet)
			ms->shim.exp = ((u_int8_t)iphdr.ip_tos) >> 5;
	}

	if ((m = mpls_prepend_shim(m, ms)) == NULL)
		return NULL;

	return m;
}

#endif	/* INET */

#ifdef INET6

static int
mpls_unlabel_inet6(struct mbuf *m)
{
	struct ip6_hdr *ip6hdr;
	union mpls_shim ms;

	/* TODO: mapclass */
	if (mpls_mapttl_inet6) {
		ms.s_addr = ntohl(mtod(m, union mpls_shim *)->s_addr);
		m_adj(m, sizeof(union mpls_shim));

		if (m->m_len < sizeof (struct ip6_hdr) &&
		    (m = m_pullup(m, sizeof(struct ip6_hdr))) == 0)
			return ENOBUFS;
		ip6hdr = mtod(m, struct ip6_hdr *);

		/* Because we just decremented this in mpls_lse */
		ip6hdr->ip6_hlim = ms.shim.ttl + 1;
	} else
		m_adj(m, sizeof(union mpls_shim));

	/* Put it back on IPv6 queue. */
	if (__predict_false(!pktq_enqueue(ip6_pktq, m, 0))) {
		m_freem(m);
		return ENOBUFS;
	}
	return 0;
}

static struct mbuf *
mpls_label_inet6(struct mbuf *m, union mpls_shim *ms, uint offset)
{
	struct ip6_hdr ip6h;

	if (mpls_mapttl_inet6 || mpls_mapclass_inet6) {
		if (m->m_len < sizeof(struct ip6_hdr) &&
		    (m = m_pullup(m, offset + sizeof(struct ip6_hdr))) == 0)
			return NULL;
		m_copydata(m, offset, sizeof(struct ip6_hdr), &ip6h);

		if (mpls_mapttl_inet6)
			ms->shim.ttl = ip6h.ip6_hlim;

		if (mpls_mapclass_inet6)
			ms->shim.exp = ip6h.ip6_vfc << 1 >> 5;
	}

	if ((m = mpls_prepend_shim(m, ms)) == NULL)
		return NULL;

	return m;
}

#endif	/* INET6 */

static struct mbuf *
mpls_prepend_shim(struct mbuf *m, union mpls_shim *ms) 
{
	union mpls_shim *shim; 
 
	M_PREPEND(m, sizeof(*ms), M_DONTWAIT);
	if (m == NULL)
		return NULL;

	if (m->m_len < sizeof(union mpls_shim) &&
	    (m = m_pullup(m, sizeof(union mpls_shim))) == 0)
		return NULL;

	shim = mtod(m, union mpls_shim *);

	memcpy(shim, ms, sizeof(*shim));
	shim->s_addr = htonl(shim->s_addr);

	return m;
}
