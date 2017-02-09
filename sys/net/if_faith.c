/*	$NetBSD: if_faith.c,v 1.51 2015/08/20 14:40:19 christos Exp $	*/
/*	$KAME: if_faith.c,v 1.21 2001/02/20 07:59:26 itojun Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * derived from
 *	@(#)if_loop.c	8.1 (Berkeley) 6/10/93
 * Id: if_loop.c,v 1.22 1996/06/19 16:24:10 wollman Exp
 */

/*
 * IPv6-to-IPv4 TCP relay capturing interface
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_faith.c,v 1.51 2015/08/20 14:40:19 christos Exp $");

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/queue.h>

#include <sys/cpu.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/bpf.h>
#include <net/if_faith.h>

#ifdef	INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#endif
#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#endif


#include <net/net_osdep.h>

#include "ioconf.h"

static int	faithioctl(struct ifnet *, u_long, void *);
static int	faithoutput(struct ifnet *, struct mbuf *,
		            const struct sockaddr *, struct rtentry *);
static void	faithrtrequest(int, struct rtentry *,
		               const struct rt_addrinfo *);

static int	faith_clone_create(struct if_clone *, int);
static int	faith_clone_destroy(struct ifnet *);

static struct if_clone faith_cloner =
    IF_CLONE_INITIALIZER("faith", faith_clone_create, faith_clone_destroy);

#define	FAITHMTU	1500

/* ARGSUSED */
void
faithattach(int count)
{

	if_clone_attach(&faith_cloner);
}

static int
faith_clone_create(struct if_clone *ifc, int unit)
{
	struct ifnet *ifp;

	ifp = if_alloc(IFT_FAITH);

	if_initname(ifp, ifc->ifc_name, unit);

	ifp->if_mtu = FAITHMTU;
	/* Change to BROADCAST experimentaly to announce its prefix. */
	ifp->if_flags = /* IFF_LOOPBACK */ IFF_BROADCAST | IFF_MULTICAST;
	ifp->if_ioctl = faithioctl;
	ifp->if_output = faithoutput;
	ifp->if_type = IFT_FAITH;
	ifp->if_hdrlen = 0;
	ifp->if_addrlen = 0;
	ifp->if_dlt = DLT_NULL;
	if_attach(ifp);
	if_alloc_sadl(ifp);
	bpf_attach(ifp, DLT_NULL, sizeof(u_int));
	return (0);
}

int
faith_clone_destroy(struct ifnet *ifp)
{

	bpf_detach(ifp);
	if_detach(ifp);
	if_free(ifp);

	return (0);
}

static int
faithoutput(struct ifnet *ifp, struct mbuf *m, const struct sockaddr *dst,
    struct rtentry *rt)
{
	pktqueue_t *pktq;
	size_t pktlen;
	int s, error;
	uint32_t af;

	if ((m->m_flags & M_PKTHDR) == 0)
		panic("faithoutput no HDR");
	af = dst->sa_family;
	/* BPF write needs to be handled specially */
	if (af == AF_UNSPEC) {
		af = *(mtod(m, int *));
		m_adj(m, sizeof(int));
	}

	bpf_mtap_af(ifp, af, m);

	if (rt && rt->rt_flags & (RTF_REJECT|RTF_BLACKHOLE)) {
		m_freem(m);
		return (rt->rt_flags & RTF_BLACKHOLE ? 0 :
		        rt->rt_flags & RTF_HOST ? EHOSTUNREACH : ENETUNREACH);
	}
	pktlen = m->m_pkthdr.len;
	ifp->if_opackets++;
	ifp->if_obytes += pktlen;
	switch (af) {
#ifdef INET
	case AF_INET:
		pktq = ip_pktq;
		break;
#endif
#ifdef INET6
	case AF_INET6:
		pktq = ip6_pktq;
		break;
#endif
	default:
		m_freem(m);
		return EAFNOSUPPORT;
	}

	/* XXX do we need more sanity checks? */
	KASSERT(pktq != NULL);
	m->m_pkthdr.rcvif = ifp;

	s = splnet();
	if (__predict_true(pktq_enqueue(pktq, m, 0))) {
		ifp->if_ipackets++;
		ifp->if_ibytes += pktlen;
		error = 0;
	} else {
		m_freem(m);
		error = ENOBUFS;
	}
	splx(s);

	return error;
}

/* ARGSUSED */
static void
faithrtrequest(int cmd, struct rtentry *rt,
    const struct rt_addrinfo *info)
{
	if (rt)
		rt->rt_rmx.rmx_mtu = rt->rt_ifp->if_mtu; /* for ISO */
}

/*
 * Process an ioctl request.
 */
/* ARGSUSED */
static int
faithioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct ifaddr *ifa;
	struct ifreq *ifr = (struct ifreq *)data;
	int error = 0;

	switch (cmd) {

	case SIOCINITIFADDR:
		ifp->if_flags |= IFF_UP | IFF_RUNNING;
		ifa = (struct ifaddr *)data;
		ifa->ifa_rtrequest = faithrtrequest;
		/*
		 * Everything else is done at a higher level.
		 */
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifr == 0) {
			error = EAFNOSUPPORT;		/* XXX */
			break;
		}
		switch (ifr->ifr_addr.sa_family) {
#ifdef INET
		case AF_INET:
			break;
#endif
#ifdef INET6
		case AF_INET6:
			break;
#endif

		default:
			error = EAFNOSUPPORT;
			break;
		}
		break;

	default:
		if ((error = ifioctl_common(ifp, cmd, data)) == ENETRESET)
			error = 0;
		break;
	}
	return (error);
}

#ifdef INET6
/*
 * XXX could be slow
 * XXX could be layer violation to call sys/net from sys/netinet6
 */
int
faithprefix(struct in6_addr *in6)
{
	struct rtentry *rt;
	struct sockaddr_in6 sin6;
	int ret;

	if (ip6_keepfaith == 0)
		return 0;

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	sin6.sin6_addr = *in6;
	rt = rtalloc1((struct sockaddr *)&sin6, 0);
	if (rt && rt->rt_ifp && rt->rt_ifp->if_type == IFT_FAITH &&
	    (rt->rt_ifp->if_flags & IFF_UP) != 0)
		ret = 1;
	else
		ret = 0;
	if (rt)
		rtfree(rt);
	return ret;
}
#endif
