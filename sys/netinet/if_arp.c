/*	$NetBSD: if_arp.c,v 1.184 2015/10/08 08:17:37 roy Exp $	*/

/*-
 * Copyright (c) 1998, 2000, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Public Access Networks Corporation ("Panix").  It was developed under
 * contract to Panix by Eric Haszlakiewicz and Thor Lancelot Simon.
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
 * Copyright (c) 1982, 1986, 1988, 1993
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
 *
 *	@(#)if_ether.c	8.2 (Berkeley) 9/26/94
 */

/*
 * Ethernet address resolution protocol.
 * TODO:
 *	add "inuse/lock" bit (or ref. count) along with valid bit
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_arp.c,v 1.184 2015/10/08 08:17:37 roy Exp $");

#ifdef _KERNEL_OPT
#include "opt_ddb.h"
#include "opt_inet.h"
#endif

#ifdef INET

#include "bridge.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/timetc.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/sysctl.h>
#include <sys/socketvar.h>
#include <sys/percpu.h>
#include <sys/cprng.h>
#include <sys/kmem.h>

#include <net/ethertypes.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_token.h>
#include <net/if_types.h>
#include <net/if_ether.h>
#include <net/if_llatbl.h>
#include <net/net_osdep.h>
#include <net/route.h>
#include <net/net_stats.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_inarp.h>

#include "arcnet.h"
#if NARCNET > 0
#include <net/if_arc.h>
#endif
#include "fddi.h"
#if NFDDI > 0
#include <net/if_fddi.h>
#endif
#include "token.h"
#include "carp.h"
#if NCARP > 0
#include <netinet/ip_carp.h>
#endif

#define SIN(s) ((struct sockaddr_in *)s)
#define SRP(s) ((struct sockaddr_inarp *)s)

/*
 * ARP trailer negotiation.  Trailer protocol is not IP specific,
 * but ARP request/response use IP addresses.
 */
#define ETHERTYPE_IPTRAILERS ETHERTYPE_TRAIL

/* timer values */
static int	arpt_keep = (20*60);	/* once resolved, good for 20 more minutes */
static int	arpt_down = 20;		/* once declared down, don't send for 20 secs */
static int	arp_maxhold = 1;	/* number of packets to hold per ARP entry */
#define	rt_expire rt_rmx.rmx_expire
#define	rt_pksent rt_rmx.rmx_pksent

int		ip_dad_count = PROBE_NUM;
#ifdef ARP_DEBUG
static int	arp_debug = 1;
#else
static int	arp_debug = 0;
#endif
#define arplog(x)	do { if (arp_debug) log x; } while (/*CONSTCOND*/ 0)

static	void arp_init(void);

static	struct sockaddr *arp_setgate(struct rtentry *, struct sockaddr *,
	    const struct sockaddr *);
static	void arptfree(struct rtentry *);
static	void arptimer(void *);
static	struct llentry *arplookup(struct ifnet *, struct mbuf *,
	    const struct in_addr *, int, int, int, struct rtentry *);
static	void in_arpinput(struct mbuf *);
static	void in_revarpinput(struct mbuf *);
static	void revarprequest(struct ifnet *);

static	void arp_drainstub(void);

static void arp_dad_timer(struct ifaddr *);
static void arp_dad_start(struct ifaddr *);
static void arp_dad_stop(struct ifaddr *);
static void arp_dad_duplicated(struct ifaddr *);

struct	ifqueue arpintrq = {
	.ifq_head = NULL,
	.ifq_tail = NULL,
	.ifq_len = 0,
	.ifq_maxlen = 50,
	.ifq_drops = 0,
};
static int	arp_inuse, arp_allocated;
static int	arp_maxtries = 5;
static int	useloopback = 1;	/* use loopback interface for local traffic */

static percpu_t *arpstat_percpu;

#define	ARP_STAT_GETREF()	_NET_STAT_GETREF(arpstat_percpu)
#define	ARP_STAT_PUTREF()	_NET_STAT_PUTREF(arpstat_percpu)

#define	ARP_STATINC(x)		_NET_STATINC(arpstat_percpu, x)
#define	ARP_STATADD(x, v)	_NET_STATADD(arpstat_percpu, x, v)

/* revarp state */
static struct	in_addr myip, srv_ip;
static int	myip_initialized = 0;
static int	revarp_in_progress = 0;
static struct	ifnet *myip_ifp = NULL;

#ifdef DDB
static void db_print_sa(const struct sockaddr *);
static void db_print_ifa(struct ifaddr *);
static void db_print_llinfo(void *);
static int db_show_rtentry(struct rtentry *, void *);
#endif

static int arp_drainwanted;

static int log_movements = 1;
static int log_permanent_modify = 1;
static int log_wrong_iface = 1;
static int log_unknown_network = 1;

/*
 * this should be elsewhere.
 */

static char *
lla_snprintf(u_int8_t *, int);

static char *
lla_snprintf(u_int8_t *adrp, int len)
{
#define NUMBUFS 3
	static char buf[NUMBUFS][16*3];
	static int bnum = 0;

	int i;
	char *p;

	p = buf[bnum];

	*p++ = hexdigits[(*adrp)>>4];
	*p++ = hexdigits[(*adrp++)&0xf];

	for (i=1; i<len && i<16; i++) {
		*p++ = ':';
		*p++ = hexdigits[(*adrp)>>4];
		*p++ = hexdigits[(*adrp++)&0xf];
	}

	*p = 0;
	p = buf[bnum];
	bnum = (bnum + 1) % NUMBUFS;
	return p;
}

DOMAIN_DEFINE(arpdomain);	/* forward declare and add to link set */

static void
arp_fasttimo(void)
{
	if (arp_drainwanted) {
		arp_drain();
		arp_drainwanted = 0;
	}
}

const struct protosw arpsw[] = {
	{ .pr_type = 0,
	  .pr_domain = &arpdomain,
	  .pr_protocol = 0,
	  .pr_flags = 0,
	  .pr_input = 0,
	  .pr_output = 0,
	  .pr_ctlinput = 0,
	  .pr_ctloutput = 0,
	  .pr_usrreqs = 0,
	  .pr_init = arp_init,
	  .pr_fasttimo = arp_fasttimo,
	  .pr_slowtimo = 0,
	  .pr_drain = arp_drainstub,
	}
};

struct domain arpdomain = {
	.dom_family = PF_ARP,
	.dom_name = "arp",
	.dom_protosw = arpsw,
	.dom_protoswNPROTOSW = &arpsw[__arraycount(arpsw)],
};

static void sysctl_net_inet_arp_setup(struct sysctllog **);

void
arp_init(void)
{

	sysctl_net_inet_arp_setup(NULL);
	arpstat_percpu = percpu_alloc(sizeof(uint64_t) * ARP_NSTATS);
}

static void
arp_drainstub(void)
{
	arp_drainwanted = 1;
}

/*
 * ARP protocol drain routine.  Called when memory is in short supply.
 * Called at splvm();  don't acquire softnet_lock as can be called from
 * hardware interrupt handlers.
 */
void
arp_drain(void)
{

	lltable_drain(AF_INET);
}

static void
arptimer(void *arg)
{
	struct llentry *lle = arg;
	struct ifnet *ifp;

	mutex_enter(softnet_lock);

	if (lle == NULL)
		goto out;

	if (lle->la_flags & LLE_STATIC)
		goto out;

	LLE_WLOCK(lle);
	if (callout_pending(&lle->la_timer)) {
		/*
		 * Here we are a bit odd here in the treatment of
		 * active/pending. If the pending bit is set, it got
		 * rescheduled before I ran. The active
		 * bit we ignore, since if it was stopped
		 * in ll_tablefree() and was currently running
		 * it would have return 0 so the code would
		 * not have deleted it since the callout could
		 * not be stopped so we want to go through
		 * with the delete here now. If the callout
		 * was restarted, the pending bit will be back on and
		 * we just want to bail since the callout_reset would
		 * return 1 and our reference would have been removed
		 * by arpresolve() below.
		 */
		LLE_WUNLOCK(lle);
		goto out;
	}
	ifp = lle->lle_tbl->llt_ifp;

	callout_stop(&lle->la_timer);

	/* XXX: LOR avoidance. We still have ref on lle. */
	LLE_WUNLOCK(lle);

	if (lle->la_rt != NULL) {
		/* We have to call arptfree w/o IF_AFDATA_LOCK */
		arptfree(lle->la_rt);
		lle->la_rt = NULL;
	}

	IF_AFDATA_LOCK(ifp);
	LLE_WLOCK(lle);

	/* Guard against race with other llentry_free(). */
	if (lle->la_flags & LLE_LINKED) {
		size_t pkts_dropped;

		LLE_REMREF(lle);
		pkts_dropped = llentry_free(lle);
		ARP_STATADD(ARP_STAT_DFRDROPPED, pkts_dropped);
	} else {
		LLE_FREE_LOCKED(lle);
	}

	IF_AFDATA_UNLOCK(ifp);

out:
	mutex_exit(softnet_lock);
}

/*
 * We set the gateway for RTF_CLONING routes to a "prototype"
 * link-layer sockaddr whose interface type (if_type) and interface
 * index (if_index) fields are prepared.
 */
static struct sockaddr *
arp_setgate(struct rtentry *rt, struct sockaddr *gate,
    const struct sockaddr *netmask)
{
	const struct ifnet *ifp = rt->rt_ifp;
	uint8_t namelen = strlen(ifp->if_xname);
	uint8_t addrlen = ifp->if_addrlen;

	/*
	 * XXX: If this is a manually added route to interface
	 * such as older version of routed or gated might provide,
	 * restore cloning bit.
	 */
	if ((rt->rt_flags & RTF_HOST) == 0 && netmask != NULL &&
	    satocsin(netmask)->sin_addr.s_addr != 0xffffffff)
		rt->rt_flags |= RTF_CLONING;
	if (rt->rt_flags & RTF_CLONING ||
	    ((rt->rt_flags & (RTF_LLINFO | RTF_LOCAL)) && !rt->rt_llinfo))
	{
		union {
			struct sockaddr sa;
			struct sockaddr_storage ss;
			struct sockaddr_dl sdl;
		} u;
		/*
		 * Case 1: This route should come from a route to iface.
		 */
		sockaddr_dl_init(&u.sdl, sizeof(u.ss),
		    ifp->if_index, ifp->if_type, NULL, namelen, NULL, addrlen);
		rt_setgate(rt, &u.sa);
		gate = rt->rt_gateway;
	}
	return gate;
}

/*
 * Parallel to llc_rtrequest.
 */
void
arp_rtrequest(int req, struct rtentry *rt, const struct rt_addrinfo *info)
{
	struct sockaddr *gate = rt->rt_gateway;
	struct llentry *la = NULL;
	struct in_ifaddr *ia;
	struct ifaddr *ifa;
	struct ifnet *ifp = rt->rt_ifp;
	int flags = 0;

	if (req == RTM_LLINFO_UPD) {
		struct in_addr *in;

		if ((ifa = info->rti_ifa) == NULL)
			return;

		in = &ifatoia(ifa)->ia_addr.sin_addr;

		if (ifatoia(ifa)->ia4_flags &
		    (IN_IFF_NOTREADY | IN_IFF_DETACHED))
		{
			arplog((LOG_DEBUG, "arp_request: %s not ready\n",
			   in_fmtaddr(*in)));
			return;
		}

		arprequest(ifa->ifa_ifp, in, in,
		    CLLADDR(ifa->ifa_ifp->if_sadl));
		return;
	}

	if ((rt->rt_flags & RTF_GATEWAY) != 0) {
		if (req != RTM_ADD)
			return;

		/*
		 * linklayers with particular link MTU limitation.
		 */
		switch(ifp->if_type) {
#if NFDDI > 0
		case IFT_FDDI:
			if (ifp->if_mtu > FDDIIPMTU)
				rt->rt_rmx.rmx_mtu = FDDIIPMTU;
			break;
#endif
#if NARCNET > 0
		case IFT_ARCNET:
		    {
			int arcipifmtu;

			if (ifp->if_flags & IFF_LINK0)
				arcipifmtu = arc_ipmtu;
			else
				arcipifmtu = ARCMTU;
			if (ifp->if_mtu > arcipifmtu)
				rt->rt_rmx.rmx_mtu = arcipifmtu;
			break;
		    }
#endif
		}
		return;
	}

	IF_AFDATA_RLOCK(ifp);
	la = lla_lookup(LLTABLE(ifp), flags, rt_getkey(rt));
	IF_AFDATA_RUNLOCK(ifp);

	switch (req) {
	case RTM_SETGATE:
		gate = arp_setgate(rt, gate, info->rti_info[RTAX_NETMASK]);
		break;
	case RTM_ADD:
		gate = arp_setgate(rt, gate, info->rti_info[RTAX_NETMASK]);
		if (rt->rt_flags & RTF_CLONING ||
		    ((rt->rt_flags & (RTF_LLINFO | RTF_LOCAL)) && !la))
		{
			/*
			 * Give this route an expiration time, even though
			 * it's a "permanent" route, so that routes cloned
			 * from it do not need their expiration time set.
			 */
			KASSERT(time_uptime != 0);
			rt->rt_expire = time_uptime;
			/*
			 * linklayers with particular link MTU limitation.
			 */
			switch (ifp->if_type) {
#if NFDDI > 0
			case IFT_FDDI:
				if ((rt->rt_rmx.rmx_locks & RTV_MTU) == 0 &&
				    (rt->rt_rmx.rmx_mtu > FDDIIPMTU ||
				     (rt->rt_rmx.rmx_mtu == 0 &&
				      ifp->if_mtu > FDDIIPMTU)))
					rt->rt_rmx.rmx_mtu = FDDIIPMTU;
				break;
#endif
#if NARCNET > 0
			case IFT_ARCNET:
			    {
				int arcipifmtu;
				if (ifp->if_flags & IFF_LINK0)
					arcipifmtu = arc_ipmtu;
				else
					arcipifmtu = ARCMTU;

				if ((rt->rt_rmx.rmx_locks & RTV_MTU) == 0 &&
				    (rt->rt_rmx.rmx_mtu > arcipifmtu ||
				     (rt->rt_rmx.rmx_mtu == 0 &&
				      ifp->if_mtu > arcipifmtu)))
					rt->rt_rmx.rmx_mtu = arcipifmtu;
				break;
			    }
#endif
			}
			if (rt->rt_flags & RTF_CLONING)
				break;
		}
		/* Announce a new entry if requested. */
		if (rt->rt_flags & RTF_ANNOUNCE) {
			INADDR_TO_IA(satocsin(rt_getkey(rt))->sin_addr, ia);
			while (ia && ia->ia_ifp != ifp)
				NEXT_IA_WITH_SAME_ADDR(ia);
			if (ia == NULL ||
			    ia->ia4_flags & (IN_IFF_NOTREADY | IN_IFF_DETACHED))
				;
			else
				arprequest(ifp,
				    &satocsin(rt_getkey(rt))->sin_addr,
				    &satocsin(rt_getkey(rt))->sin_addr,
				    CLLADDR(satocsdl(gate)));
		}
		/*FALLTHROUGH*/
	case RTM_RESOLVE:
		if (gate->sa_family != AF_LINK ||
		    gate->sa_len < sockaddr_dl_measure(0, ifp->if_addrlen)) {
			log(LOG_DEBUG, "arp_rtrequest: bad gateway value\n");
			break;
		}

		satosdl(gate)->sdl_type = ifp->if_type;
		satosdl(gate)->sdl_index = ifp->if_index;
		if (la != NULL)
			break; /* This happens on a route change */

		/* If the route is for a broadcast address mark it as such.
		 * This way we can avoid an expensive call to in_broadcast()
		 * in ip_output() most of the time (because the route passed
		 * to ip_output() is almost always a host route). */
		if (rt->rt_flags & RTF_HOST &&
		    !(rt->rt_flags & RTF_BROADCAST) &&
		    in_broadcast(satocsin(rt_getkey(rt))->sin_addr, rt->rt_ifp))
			rt->rt_flags |= RTF_BROADCAST;
		/* There is little point in resolving the broadcast address */
		if (rt->rt_flags & RTF_BROADCAST)
			break;

		INADDR_TO_IA(satocsin(rt_getkey(rt))->sin_addr, ia);
		while (ia && ia->ia_ifp != ifp)
			NEXT_IA_WITH_SAME_ADDR(ia);
		if (ia) {
			/*
			 * This test used to be
			 *	if (lo0ifp->if_flags & IFF_UP)
			 * It allowed local traffic to be forced through
			 * the hardware by configuring the loopback down.
			 * However, it causes problems during network
			 * configuration for boards that can't receive
			 * packets they send.  It is now necessary to clear
			 * "useloopback" and remove the route to force
			 * traffic out to the hardware.
			 *
			 * In 4.4BSD, the above "if" statement checked
			 * rt->rt_ifa against rt_getkey(rt).  It was changed
			 * to the current form so that we can provide a
			 * better support for multiple IPv4 addresses on a
			 * interface.
			 */
			rt->rt_expire = 0;
			if (sockaddr_dl_init(satosdl(gate), gate->sa_len,
			    ifp->if_index, ifp->if_type, NULL, 0,
			    CLLADDR(ifp->if_sadl), ifp->if_addrlen) == NULL) {
				panic("%s(%s): sockaddr_dl_init cannot fail",
				    __func__, ifp->if_xname);
			}
			if (useloopback) {
				ifp = rt->rt_ifp = lo0ifp;
				rt->rt_rmx.rmx_mtu = 0;
			}
			rt->rt_flags |= RTF_LOCAL;
			/*
			 * make sure to set rt->rt_ifa to the interface
			 * address we are using, otherwise we will have trouble
			 * with source address selection.
			 */
			ifa = &ia->ia_ifa;
			if (ifa != rt->rt_ifa)
				rt_replace_ifa(rt, ifa);
		}

		/*
		 * Case 2:  This route may come from cloning, or a manual route
		 * add with a LL address.
		 */
		flags = LLE_EXCLUSIVE;
		if ((rt->rt_flags & RTF_CLONED) == 0)
			flags |= LLE_IFADDR;

		IF_AFDATA_WLOCK(ifp);
		la = lla_create(LLTABLE(ifp), flags, rt_getkey(rt));
		IF_AFDATA_WUNLOCK(ifp);

		if (la == NULL) {
			log(LOG_DEBUG, "%s: lla_create failed\n",
			    __func__);
			rt->rt_llinfo = NULL;
			break;
		}
		rt->rt_llinfo = la;
		switch (ifp->if_type) {
#if NTOKEN > 0
		case IFT_ISO88025:
			la->la_opaque = kmem_alloc(sizeof(struct token_rif),
			    KM_SLEEP);
			break;
#endif /* NTOKEN > 0 */
		default:
			break;
		}
		la->la_rt = rt;
		rt->rt_refcnt++;
		rt->rt_flags |= RTF_LLINFO;
		arp_inuse++, arp_allocated++;

		LLE_WUNLOCK(la);
		la = NULL;

		break;

	case RTM_DELETE:
		if (la == NULL)
			break;
		arp_inuse--;
		rt->rt_llinfo = NULL;
		rt->rt_flags &= ~RTF_LLINFO;

		LLE_RUNLOCK(la);

		flags |= LLE_EXCLUSIVE;
		IF_AFDATA_WLOCK(ifp);

		la = lla_lookup(LLTABLE(ifp), flags, rt_getkey(rt));
		/* This shouldn't happen */
		if (la == NULL) {
			IF_AFDATA_WUNLOCK(ifp);
			break;
		}

		if (la->la_opaque != NULL) {
			switch (ifp->if_type) {
#if NTOKEN > 0
			case IFT_ISO88025:
				kmem_free(la->la_opaque,
				    sizeof(struct token_rif));
				break;
#endif /* NTOKEN > 0 */
			default:
				break;
			}
		}

		if (la->la_rt != NULL) {
			/*
			 * Don't rtfree (may actually free objects) here.
			 * Leave it to rtrequest1.
			 */
			la->la_rt->rt_refcnt--;
			la->la_rt = NULL;
		}
		llentry_free(la);

		IF_AFDATA_WUNLOCK(ifp);
		la = NULL;
	}

	if (la != NULL) {
		if (flags & LLE_EXCLUSIVE)
			LLE_WUNLOCK(la);
		else
			LLE_RUNLOCK(la);
	}
}

/*
 * Broadcast an ARP request. Caller specifies:
 *	- arp header source ip address
 *	- arp header target ip address
 *	- arp header source ethernet address
 */
void
arprequest(struct ifnet *ifp,
    const struct in_addr *sip, const struct in_addr *tip,
    const u_int8_t *enaddr)
{
	struct mbuf *m;
	struct arphdr *ah;
	struct sockaddr sa;
	uint64_t *arps;

	KASSERT(sip != NULL);
	KASSERT(tip != NULL);
	KASSERT(enaddr != NULL);

	if ((m = m_gethdr(M_DONTWAIT, MT_DATA)) == NULL)
		return;
	MCLAIM(m, &arpdomain.dom_mowner);
	switch (ifp->if_type) {
	case IFT_IEEE1394:
		m->m_len = sizeof(*ah) + 2 * sizeof(struct in_addr) +
		    ifp->if_addrlen;
		break;
	default:
		m->m_len = sizeof(*ah) + 2 * sizeof(struct in_addr) +
		    2 * ifp->if_addrlen;
		break;
	}
	m->m_pkthdr.len = m->m_len;
	MH_ALIGN(m, m->m_len);
	ah = mtod(m, struct arphdr *);
	memset(ah, 0, m->m_len);
	switch (ifp->if_type) {
	case IFT_IEEE1394:	/* RFC2734 */
		/* fill it now for ar_tpa computation */
		ah->ar_hrd = htons(ARPHRD_IEEE1394);
		break;
	default:
		/* ifp->if_output will fill ar_hrd */
		break;
	}
	ah->ar_pro = htons(ETHERTYPE_IP);
	ah->ar_hln = ifp->if_addrlen;		/* hardware address length */
	ah->ar_pln = sizeof(struct in_addr);	/* protocol address length */
	ah->ar_op = htons(ARPOP_REQUEST);
	memcpy(ar_sha(ah), enaddr, ah->ar_hln);
	memcpy(ar_spa(ah), sip, ah->ar_pln);
	memcpy(ar_tpa(ah), tip, ah->ar_pln);
	sa.sa_family = AF_ARP;
	sa.sa_len = 2;
	m->m_flags |= M_BCAST;
	arps = ARP_STAT_GETREF();
	arps[ARP_STAT_SNDTOTAL]++;
	arps[ARP_STAT_SENDREQUEST]++;
	ARP_STAT_PUTREF();
	(*ifp->if_output)(ifp, m, &sa, NULL);
}

/*
 * Resolve an IP address into an ethernet address.  If success,
 * desten is filled in.  If there is no entry in arptab,
 * set one up and broadcast a request for the IP address.
 * Hold onto this mbuf and resend it once the address
 * is finally resolved.  A return value of 1 indicates
 * that desten has been filled in and the packet should be sent
 * normally; a 0 return indicates that the packet has been
 * taken over here, either now or for later transmission.
 */
int
arpresolve(struct ifnet *ifp, struct rtentry *rt, struct mbuf *m,
    const struct sockaddr *dst, u_char *desten)
{
	struct llentry *la;
	const struct sockaddr_dl *sdl;
	int renew;
	int flags = 0;
	int error;

	KASSERT(m != NULL);

	la = arplookup(ifp, m, &satocsin(dst)->sin_addr, 1, 0, 0, rt);
	if (la != NULL)
		rt = la->la_rt;

	if (la == NULL || rt == NULL) {
		ARP_STATINC(ARP_STAT_ALLOCFAIL);
		log(LOG_DEBUG,
		    "arpresolve: can't allocate llinfo on %s for %s\n",
		    ifp->if_xname, in_fmtaddr(satocsin(dst)->sin_addr));
		m_freem(m);
		if (la != NULL)
			LLE_RUNLOCK(la);
		return 0;
	}
	sdl = satocsdl(rt->rt_gateway);
	/*
	 * Check the address family and length is valid, the address
	 * is resolved; otherwise, try to resolve.
	 */
	if ((rt->rt_expire == 0 || rt->rt_expire > time_uptime) &&
	    sdl->sdl_family == AF_LINK && sdl->sdl_alen != 0) {
		memcpy(desten, CLLADDR(sdl),
		    min(sdl->sdl_alen, ifp->if_addrlen));
		rt->rt_pksent = time_uptime; /* Time for last pkt sent */
		LLE_RUNLOCK(la);
		return 1;
	}

	/*
	 * Re-send the ARP request when appropriate.
	 */
#ifdef	DIAGNOSTIC
	if (rt->rt_expire == 0) {
		/* This should never happen. (Should it? -gwr) */
		printf("arpresolve: unresolved and rt_expire == 0\n");
		/* Set expiration time to now (expired). */
		rt->rt_expire = time_uptime;
	}
#endif

retry:
	if (la == NULL) {
		IF_AFDATA_RLOCK(ifp);
		la = lla_lookup(LLTABLE(ifp), flags, dst);
		IF_AFDATA_RUNLOCK(ifp);
	}

#ifdef IFF_STATICARP /* FreeBSD */
#define _IFF_NOARP (IFF_NOARP | IFF_STATICARP)
#else
#define _IFF_NOARP IFF_NOARP
#endif
	if ((la == NULL) && ((flags & LLE_EXCLUSIVE) == 0)
	    && ((ifp->if_flags & _IFF_NOARP) == 0))
	{
		flags |= LLE_EXCLUSIVE;
		IF_AFDATA_WLOCK(ifp);
		la = lla_create(LLTABLE(ifp), flags, dst);
		IF_AFDATA_WUNLOCK(ifp);

		if (la == NULL) {
			log(LOG_DEBUG,
			    "%s: failed to create llentry for %s on %s\n",
			    __func__, inet_ntoa(satocsin(dst)->sin_addr),
			    ifp->if_xname);
		}
	}
#undef _IFF_NOARP

	if (la == NULL) {
		m_freem(m);
		return 0;
	}

	if ((la->la_flags & LLE_VALID) &&
	    ((la->la_flags & LLE_STATIC) || la->la_expire > time_uptime)) {
		memcpy(desten, CLLADDR(sdl),
		    min(sdl->sdl_alen, ifp->if_addrlen));
		renew = 0;
		/*
		 * If entry has an expiry time and it is approaching,
		 * see if we need to send an ARP request within this
		 * arpt_down interval.
		 */
		if (!(la->la_flags & LLE_STATIC) &&
		    time_uptime + la->la_preempt > la->la_expire) {
			renew = 1;
			la->la_preempt--;
		}

		if (flags & LLE_EXCLUSIVE)
			LLE_WUNLOCK(la);
		else
			LLE_RUNLOCK(la);

		if (renew == 1) {
			const u_int8_t *enaddr =
#if NCARP > 0
			    (rt->rt_ifp->if_type == IFT_CARP) ?
			    CLLADDR(rt->rt_ifp->if_sadl):
#endif
			    CLLADDR(ifp->if_sadl);
			arprequest(ifp, &satocsin(rt->rt_ifa->ifa_addr)->sin_addr,
			    &satocsin(dst)->sin_addr, enaddr);
		}

		return 1;
	}

	if (la->la_flags & LLE_STATIC) {   /* should not happen! */
		log(LOG_DEBUG, "arpresolve: ouch, empty static llinfo for %s\n",
		    inet_ntoa(satocsin(dst)->sin_addr));
		m_freem(m);
		error = EINVAL;
		goto done;
	}

	renew = (la->la_asked == 0 || la->la_expire != time_uptime);
	if (renew && (flags & LLE_EXCLUSIVE) == 0) {
		flags |= LLE_EXCLUSIVE;
		LLE_RUNLOCK(la);
		la = NULL;
		goto retry;
	}

	/*
	 * There is an arptab entry, but no ethernet address
	 * response yet.  Add the mbuf to the list, dropping
	 * the oldest packet if we have exceeded the system
	 * setting.
	 */
	LLE_WLOCK_ASSERT(la);
	if (la->la_numheld >= arp_maxhold) {
		if (la->la_hold != NULL) {
			struct mbuf *next = la->la_hold->m_nextpkt;
			m_freem(la->la_hold);
			la->la_hold = next;
			la->la_numheld--;
			ARP_STATINC(ARP_STAT_DFRDROPPED);
		}
	}
	if (la->la_hold != NULL) {
		struct mbuf *curr = la->la_hold;
		while (curr->m_nextpkt != NULL)
			curr = curr->m_nextpkt;
		curr->m_nextpkt = m;
	} else
		la->la_hold = m;
	la->la_numheld++;
	if (renew == 0 && (flags & LLE_EXCLUSIVE)) {
		flags &= ~LLE_EXCLUSIVE;
		LLE_DOWNGRADE(la);
	}

	/*
	 * Return EWOULDBLOCK if we have tried less than arp_maxtries. It
	 * will be masked by ether_output(). Return EHOSTDOWN/EHOSTUNREACH
	 * if we have already sent arp_maxtries ARP requests. Retransmit the
	 * ARP request, but not faster than one request per second.
	 */
	if (la->la_asked < arp_maxtries)
		error = EWOULDBLOCK;	/* First request. */
	else
		error = (rt->rt_flags & RTF_GATEWAY) ?
		    EHOSTUNREACH : EHOSTDOWN;

	if (renew) {
		const u_int8_t *enaddr =
#if NCARP > 0
		    (rt->rt_ifp->if_type == IFT_CARP) ?
		    CLLADDR(rt->rt_ifp->if_sadl):
#endif
		    CLLADDR(ifp->if_sadl);
		LLE_ADDREF(la);
		la->la_expire = time_uptime;
		callout_reset(&la->la_timer, hz * arpt_down,
		    arptimer, la);
		la->la_asked++;
		if (flags & LLE_EXCLUSIVE)
			LLE_WUNLOCK(la);
		else
			LLE_RUNLOCK(la);
		arprequest(ifp, &satocsin(rt->rt_ifa->ifa_addr)->sin_addr,
		    &satocsin(dst)->sin_addr, enaddr);
		return error == 0;
	}
done:
	if (flags & LLE_EXCLUSIVE)
		LLE_WUNLOCK(la);
	else
		LLE_RUNLOCK(la);
	return error == 0;
}

/*
 * Common length and type checks are done here,
 * then the protocol-specific routine is called.
 */
void
arpintr(void)
{
	struct mbuf *m;
	struct arphdr *ar;
	int s;
	int arplen;

	mutex_enter(softnet_lock);
	KERNEL_LOCK(1, NULL);
	while (arpintrq.ifq_head) {
		s = splnet();
		IF_DEQUEUE(&arpintrq, m);
		splx(s);
		if (m == NULL || (m->m_flags & M_PKTHDR) == 0)
			panic("arpintr");

		MCLAIM(m, &arpdomain.dom_mowner);
		ARP_STATINC(ARP_STAT_RCVTOTAL);

		/*
		 * First, make sure we have at least struct arphdr.
		 */
		if (m->m_len < sizeof(struct arphdr) ||
		    (ar = mtod(m, struct arphdr *)) == NULL)
			goto badlen;

		switch (m->m_pkthdr.rcvif->if_type) {
		case IFT_IEEE1394:
			arplen = sizeof(struct arphdr) +
			    ar->ar_hln + 2 * ar->ar_pln;
			break;
		default:
			arplen = sizeof(struct arphdr) +
			    2 * ar->ar_hln + 2 * ar->ar_pln;
			break;
		}

		if (/* XXX ntohs(ar->ar_hrd) == ARPHRD_ETHER && */
		    m->m_len >= arplen)
			switch (ntohs(ar->ar_pro)) {
			case ETHERTYPE_IP:
			case ETHERTYPE_IPTRAILERS:
				in_arpinput(m);
				continue;
			default:
				ARP_STATINC(ARP_STAT_RCVBADPROTO);
			}
		else {
badlen:
			ARP_STATINC(ARP_STAT_RCVBADLEN);
		}
		m_freem(m);
	}
	KERNEL_UNLOCK_ONE(NULL);
	mutex_exit(softnet_lock);
}

/*
 * ARP for Internet protocols on 10 Mb/s Ethernet.
 * Algorithm is that given in RFC 826.
 * In addition, a sanity check is performed on the sender
 * protocol address, to catch impersonators.
 * We no longer handle negotiations for use of trailer protocol:
 * Formerly, ARP replied for protocol type ETHERTYPE_TRAIL sent
 * along with IP replies if we wanted trailers sent to us,
 * and also sent them in response to IP replies.
 * This allowed either end to announce the desire to receive
 * trailer packets.
 * We no longer reply to requests for ETHERTYPE_TRAIL protocol either,
 * but formerly didn't normally send requests.
 */
static void
in_arpinput(struct mbuf *m)
{
	struct arphdr *ah;
	struct ifnet *ifp = m->m_pkthdr.rcvif;
	struct llentry *la = NULL;
	struct rtentry  *rt = NULL;
	struct in_ifaddr *ia;
#if NBRIDGE > 0
	struct in_ifaddr *bridge_ia = NULL;
#endif
#if NCARP > 0
	u_int32_t count = 0, index = 0;
#endif
	struct sockaddr_dl *sdl = NULL;
	struct sockaddr sa;
	struct in_addr isaddr, itaddr, myaddr;
	int op;
	void *tha;
	uint64_t *arps;

	if (__predict_false(m_makewritable(&m, 0, m->m_pkthdr.len, M_DONTWAIT)))
		goto out;
	ah = mtod(m, struct arphdr *);
	op = ntohs(ah->ar_op);

	/*
	 * Fix up ah->ar_hrd if necessary, before using ar_tha() or
	 * ar_tpa().
	 */
	switch (ifp->if_type) {
	case IFT_IEEE1394:
		if (ntohs(ah->ar_hrd) == ARPHRD_IEEE1394)
			;
		else {
			/* XXX this is to make sure we compute ar_tha right */
			/* XXX check ar_hrd more strictly? */
			ah->ar_hrd = htons(ARPHRD_IEEE1394);
		}
		break;
	default:
		/* XXX check ar_hrd? */
		break;
	}

	memcpy(&isaddr, ar_spa(ah), sizeof (isaddr));
	memcpy(&itaddr, ar_tpa(ah), sizeof (itaddr));

	if (m->m_flags & (M_BCAST|M_MCAST))
		ARP_STATINC(ARP_STAT_RCVMCAST);


	/*
	 * Search for a matching interface address
	 * or any address on the interface to use
	 * as a dummy address in the rest of this function
	 */
	
	INADDR_TO_IA(itaddr, ia);
	while (ia != NULL) {
#if NCARP > 0
		if (ia->ia_ifp->if_type == IFT_CARP &&
		    ((ia->ia_ifp->if_flags & (IFF_UP|IFF_RUNNING)) ==
		    (IFF_UP|IFF_RUNNING))) {
			index++;
			if (ia->ia_ifp == m->m_pkthdr.rcvif &&
			    carp_iamatch(ia, ar_sha(ah),
			    &count, index)) {
				break;
				}
		} else
#endif
			    if (ia->ia_ifp == m->m_pkthdr.rcvif)
				break;
#if NBRIDGE > 0
		/*
		 * If the interface we received the packet on
		 * is part of a bridge, check to see if we need
		 * to "bridge" the packet to ourselves at this
		 * layer.  Note we still prefer a perfect match,
		 * but allow this weaker match if necessary.
		 */
		if (m->m_pkthdr.rcvif->if_bridge != NULL &&
		    m->m_pkthdr.rcvif->if_bridge == ia->ia_ifp->if_bridge)
			bridge_ia = ia;
#endif /* NBRIDGE > 0 */

		NEXT_IA_WITH_SAME_ADDR(ia);
	}

#if NBRIDGE > 0
	if (ia == NULL && bridge_ia != NULL) {
		ia = bridge_ia;
		ifp = bridge_ia->ia_ifp;
	}
#endif

	if (ia == NULL) {
		INADDR_TO_IA(isaddr, ia);
		while ((ia != NULL) && ia->ia_ifp != m->m_pkthdr.rcvif)
			NEXT_IA_WITH_SAME_ADDR(ia);

		if (ia == NULL) {
			IFP_TO_IA(ifp, ia);
			if (ia == NULL) {
				ARP_STATINC(ARP_STAT_RCVNOINT);
				goto out;
			}
		}
	}

	myaddr = ia->ia_addr.sin_addr;

	/* XXX checks for bridge case? */
	if (!memcmp(ar_sha(ah), CLLADDR(ifp->if_sadl), ifp->if_addrlen)) {
		ARP_STATINC(ARP_STAT_RCVLOCALSHA);
		goto out;	/* it's from me, ignore it. */
	}

	/* XXX checks for bridge case? */
	if (!memcmp(ar_sha(ah), ifp->if_broadcastaddr, ifp->if_addrlen)) {
		ARP_STATINC(ARP_STAT_RCVBCASTSHA);
		log(LOG_ERR,
		    "%s: arp: link address is broadcast for IP address %s!\n",
		    ifp->if_xname, in_fmtaddr(isaddr));
		goto out;
	}

	/*
	 * If the source IP address is zero, this is an RFC 5227 ARP probe
	 */
	if (in_nullhost(isaddr))
		ARP_STATINC(ARP_STAT_RCVZEROSPA);
	else if (in_hosteq(isaddr, myaddr))
		ARP_STATINC(ARP_STAT_RCVLOCALSPA);

	if (in_nullhost(itaddr))
		ARP_STATINC(ARP_STAT_RCVZEROTPA);

	/* DAD check, RFC 5227 2.1.1, Probe Details */
	if (in_hosteq(isaddr, myaddr) ||
	    (in_nullhost(isaddr) && in_hosteq(itaddr, myaddr)))
	{
		/* If our address is tentative, mark it as duplicated */
		if (ia->ia4_flags & IN_IFF_TENTATIVE)
			arp_dad_duplicated((struct ifaddr *)ia);
		/* If our address is unuseable, don't reply */
		if (ia->ia4_flags & (IN_IFF_NOTREADY | IN_IFF_DETACHED))
			goto out;
	}

	/*
	 * If the target IP address is zero, ignore the packet.
	 * This prevents the code below from tring to answer
	 * when we are using IP address zero (booting).
	 */
	if (in_nullhost(itaddr))
		goto out;

	if (in_nullhost(isaddr))
		goto reply;

	if (in_hosteq(isaddr, myaddr)) {
		log(LOG_ERR,
		   "duplicate IP address %s sent from link address %s\n",
		   in_fmtaddr(isaddr), lla_snprintf(ar_sha(ah), ah->ar_hln));
		itaddr = myaddr;
		goto reply;
	}

	la = arplookup(ifp, m, &isaddr, in_hosteq(itaddr, myaddr), 0, 1, NULL);
	if (la != NULL) {
		rt = la->la_rt;
		if (rt != NULL)
			sdl = satosdl(rt->rt_gateway);
	}
	if (sdl != NULL) {
		if (sdl->sdl_alen &&
		    memcmp(ar_sha(ah), CLLADDR(sdl), sdl->sdl_alen)) {
			if (rt->rt_flags & RTF_STATIC) {
				ARP_STATINC(ARP_STAT_RCVOVERPERM);
				if (!log_permanent_modify)
					goto out;
				log(LOG_INFO,
				    "%s tried to overwrite permanent arp info"
				    " for %s\n",
				    lla_snprintf(ar_sha(ah), ah->ar_hln),
				    in_fmtaddr(isaddr));
				goto out;
			} else if (rt->rt_ifp != ifp) {
				ARP_STATINC(ARP_STAT_RCVOVERINT);
				if (!log_wrong_iface)
					goto out;
				log(LOG_INFO,
				    "%s on %s tried to overwrite "
				    "arp info for %s on %s\n",
				    lla_snprintf(ar_sha(ah), ah->ar_hln),
				    ifp->if_xname, in_fmtaddr(isaddr),
				    rt->rt_ifp->if_xname);
				    goto out;
			} else {
				ARP_STATINC(ARP_STAT_RCVOVER);
				if (log_movements)
					log(LOG_INFO, "arp info overwritten "
					    "for %s by %s\n",
					    in_fmtaddr(isaddr),
					    lla_snprintf(ar_sha(ah),
					    ah->ar_hln));
			}
		}
		/*
		 * sanity check for the address length.
		 * XXX this does not work for protocols with variable address
		 * length. -is
		 */
		if (sdl->sdl_alen &&
		    sdl->sdl_alen != ah->ar_hln) {
			ARP_STATINC(ARP_STAT_RCVLENCHG);
			log(LOG_WARNING,
			    "arp from %s: new addr len %d, was %d\n",
			    in_fmtaddr(isaddr), ah->ar_hln, sdl->sdl_alen);
		}
		if (ifp->if_addrlen != ah->ar_hln) {
			ARP_STATINC(ARP_STAT_RCVBADLEN);
			log(LOG_WARNING,
			    "arp from %s: addr len: new %d, i/f %d (ignored)\n",
			    in_fmtaddr(isaddr), ah->ar_hln,
			    ifp->if_addrlen);
			goto reply;
		}
#if NTOKEN > 0
		/*
		 * XXX uses m_data and assumes the complete answer including
		 * XXX token-ring headers is in the same buf
		 */
		if (ifp->if_type == IFT_ISO88025) {
			struct token_header *trh;

			trh = (struct token_header *)M_TRHSTART(m);
			if (trh->token_shost[0] & TOKEN_RI_PRESENT) {
				struct token_rif	*rif;
				size_t	riflen;

				rif = TOKEN_RIF(trh);
				riflen = (ntohs(rif->tr_rcf) &
				    TOKEN_RCF_LEN_MASK) >> 8;

				if (riflen > 2 &&
				    riflen < sizeof(struct token_rif) &&
				    (riflen & 1) == 0) {
					rif->tr_rcf ^= htons(TOKEN_RCF_DIRECTION);
					rif->tr_rcf &= htons(~TOKEN_RCF_BROADCAST_MASK);
					memcpy(TOKEN_RIF(la), rif, riflen);
				}
			}
		}
#endif /* NTOKEN > 0 */
		(void)sockaddr_dl_setaddr(sdl, sdl->sdl_len, ar_sha(ah),
		    ah->ar_hln);
		if (rt->rt_expire) {
			rt->rt_expire = time_uptime + arpt_keep;

			KASSERT((la->la_flags & LLE_STATIC) == 0);
			LLE_ADDREF(la);
			callout_reset(&la->la_timer, hz * arpt_keep, arptimer, la);
		}
		rt->rt_flags &= ~RTF_REJECT;
		la->la_asked = 0;

		if (la->la_hold != NULL) {
			int n = la->la_numheld;
			struct mbuf *m_hold, *m_hold_next;

			m_hold = la->la_hold;
			la->la_hold = NULL;
			la->la_numheld = 0;
			/*
			 * We have to unlock here because if_output would call
			 * arpresolve
			 */
			LLE_WUNLOCK(la);
			ARP_STATADD(ARP_STAT_DFRSENT, n);
			for (; m_hold != NULL; m_hold = m_hold_next) {
				m_hold_next = m_hold->m_nextpkt;
				m_hold->m_nextpkt = NULL;
				(*ifp->if_output)(ifp, m_hold, rt_getkey(rt), rt);
			}
		} else
			LLE_WUNLOCK(la);
		la = NULL;
	}
reply:
	if (la != NULL) {
		LLE_WUNLOCK(la);
		la = NULL;
	}
	if (op != ARPOP_REQUEST) {
		if (op == ARPOP_REPLY)
			ARP_STATINC(ARP_STAT_RCVREPLY);
		goto out;
	}
	ARP_STATINC(ARP_STAT_RCVREQUEST);
	if (in_hosteq(itaddr, myaddr)) {
		/* If our address is unuseable, don't reply */
		if (ia->ia4_flags & (IN_IFF_NOTREADY | IN_IFF_DETACHED))
			goto out;
		/* I am the target */
		tha = ar_tha(ah);
		if (tha)
			memcpy(tha, ar_sha(ah), ah->ar_hln);
		memcpy(ar_sha(ah), CLLADDR(ifp->if_sadl), ah->ar_hln);
	} else {
		la = arplookup(ifp, m, &itaddr, 0, SIN_PROXY, 0, NULL);
		if (la == NULL)
			goto out;
		rt = la->la_rt;
		LLE_RUNLOCK(la);
		la = NULL;
		if (rt->rt_ifp->if_type == IFT_CARP &&
		    m->m_pkthdr.rcvif->if_type != IFT_CARP)
			goto out;
		tha = ar_tha(ah);
		if (tha)
			memcpy(tha, ar_sha(ah), ah->ar_hln);
		sdl = satosdl(rt->rt_gateway);
		memcpy(ar_sha(ah), CLLADDR(sdl), ah->ar_hln);
	}

	memcpy(ar_tpa(ah), ar_spa(ah), ah->ar_pln);
	memcpy(ar_spa(ah), &itaddr, ah->ar_pln);
	ah->ar_op = htons(ARPOP_REPLY);
	ah->ar_pro = htons(ETHERTYPE_IP); /* let's be sure! */
	switch (ifp->if_type) {
	case IFT_IEEE1394:
		/*
		 * ieee1394 arp reply is broadcast
		 */
		m->m_flags &= ~M_MCAST;
		m->m_flags |= M_BCAST;
		m->m_len = sizeof(*ah) + (2 * ah->ar_pln) + ah->ar_hln;
		break;

	default:
		m->m_flags &= ~(M_BCAST|M_MCAST); /* never reply by broadcast */
		m->m_len = sizeof(*ah) + (2 * ah->ar_pln) + (2 * ah->ar_hln);
		break;
	}
	m->m_pkthdr.len = m->m_len;
	sa.sa_family = AF_ARP;
	sa.sa_len = 2;
	arps = ARP_STAT_GETREF();
	arps[ARP_STAT_SNDTOTAL]++;
	arps[ARP_STAT_SNDREPLY]++;
	ARP_STAT_PUTREF();
	(*ifp->if_output)(ifp, m, &sa, NULL);
	return;

out:
	if (la != NULL)
		LLE_WUNLOCK(la);
	m_freem(m);
}

/*
 * Free an arp entry.
 */
static void arptfree(struct rtentry *rt)
{

	rtrequest(RTM_DELETE, rt_getkey(rt), NULL, rt_mask(rt), 0, NULL);
	rtfree(rt);
}

/*
 * Lookup or enter a new address in arptab.
 */
static struct llentry *
arplookup(struct ifnet *ifp, struct mbuf *m, const struct in_addr *addr,
    int create, int proxy, int wlock, struct rtentry *rt0)
{
	struct arphdr *ah;
	struct rtentry *rt;
	struct sockaddr_inarp sin;
	const char *why = NULL;

	ah = mtod(m, struct arphdr *);
	if (rt0 == NULL) {
		memset(&sin, 0, sizeof(sin));
		sin.sin_len = sizeof(sin);
		sin.sin_family = AF_INET;
		sin.sin_addr = *addr;
		sin.sin_other = proxy ? SIN_PROXY : 0;
		rt = rtalloc1(sintosa(&sin), create);
		if (rt == NULL)
			return NULL;
		rt->rt_refcnt--;
	} else
		rt = rt0;

#define	IS_LLINFO(__rt)							  \
	(((__rt)->rt_flags & (RTF_GATEWAY | RTF_LLINFO)) == RTF_LLINFO && \
	 (__rt)->rt_gateway->sa_family == AF_LINK)


	if (IS_LLINFO(rt)) {
		struct llentry *la;
		int flags = wlock ? LLE_EXCLUSIVE : 0;

		IF_AFDATA_RLOCK(ifp);
		la = lla_lookup(LLTABLE(ifp), flags, rt_getkey(rt));
		IF_AFDATA_RUNLOCK(ifp);

		if (la == NULL && create) {
			IF_AFDATA_WLOCK(ifp);
			la = lla_create(LLTABLE(ifp), flags, rt_getkey(rt));
			IF_AFDATA_WUNLOCK(ifp);
		}

		return la;
	}

	if (create) {
		if (rt->rt_flags & RTF_GATEWAY) {
			if (log_unknown_network)
				why = "host is not on local network";
		} else if ((rt->rt_flags & RTF_LLINFO) == 0) {
			ARP_STATINC(ARP_STAT_ALLOCFAIL);
			why = "could not allocate llinfo";
		} else
			why = "gateway route is not ours";
		if (why) {
			log(LOG_DEBUG, "arplookup: unable to enter address"
			    " for %s@%s on %s (%s)\n", in_fmtaddr(*addr),
			    lla_snprintf(ar_sha(ah), ah->ar_hln),
			    (ifp) ? ifp->if_xname : "null", why);
		}
		if ((rt->rt_flags & RTF_CLONED) != 0) {
			rtrequest(RTM_DELETE, rt_getkey(rt),
		    	    rt->rt_gateway, rt_mask(rt), rt->rt_flags, NULL);
		}
	}
	return NULL;
}

int
arpioctl(u_long cmd, void *data)
{

	return EOPNOTSUPP;
}

void
arp_ifinit(struct ifnet *ifp, struct ifaddr *ifa)
{
	struct in_addr *ip;
	struct in_ifaddr *ia = (struct in_ifaddr *)ifa;

	/*
	 * Warn the user if another station has this IP address,
	 * but only if the interface IP address is not zero.
	 */
	ip = &IA_SIN(ifa)->sin_addr;
	if (!in_nullhost(*ip) &&
	    (ia->ia4_flags & (IN_IFF_NOTREADY | IN_IFF_DETACHED)) == 0)
		arprequest(ifp, ip, ip, CLLADDR(ifp->if_sadl));

	ifa->ifa_rtrequest = arp_rtrequest;
	ifa->ifa_flags |= RTF_CLONING;

	/* ARP will handle DAD for this address. */
	if (ia->ia4_flags & IN_IFF_TRYTENTATIVE) {
		ia->ia4_flags |= IN_IFF_TENTATIVE;
		ia->ia_dad_start = arp_dad_start;
		ia->ia_dad_stop = arp_dad_stop;
	}
}

TAILQ_HEAD(dadq_head, dadq);
struct dadq {
	TAILQ_ENTRY(dadq) dad_list;
	struct ifaddr *dad_ifa;
	int dad_count;		/* max ARP to send */
	int dad_arp_tcount;	/* # of trials to send ARP */
	int dad_arp_ocount;	/* ARP sent so far */
	int dad_arp_announce;	/* max ARP announcements */
	int dad_arp_acount;	/* # of announcements */
	struct callout dad_timer_ch;
};
MALLOC_JUSTDEFINE(M_IPARP, "ARP DAD", "ARP DAD Structure");

static struct dadq_head dadq;
static int dad_init = 0;
static int dad_maxtry = 15;     /* max # of *tries* to transmit DAD packet */

static struct dadq *
arp_dad_find(struct ifaddr *ifa)
{
	struct dadq *dp;

	TAILQ_FOREACH(dp, &dadq, dad_list) {
		if (dp->dad_ifa == ifa)
			return dp;
	}
	return NULL;
}

static void
arp_dad_starttimer(struct dadq *dp, int ticks)
{

	callout_reset(&dp->dad_timer_ch, ticks,
	    (void (*)(void *))arp_dad_timer, (void *)dp->dad_ifa);
}

static void
arp_dad_stoptimer(struct dadq *dp)
{

	callout_stop(&dp->dad_timer_ch);
}

static void
arp_dad_output(struct dadq *dp, struct ifaddr *ifa)
{
	struct in_ifaddr *ia = (struct in_ifaddr *)ifa;
	struct ifnet *ifp = ifa->ifa_ifp;
	struct in_addr sip;

	dp->dad_arp_tcount++;
	if ((ifp->if_flags & IFF_UP) == 0)
		return;
	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return;

	dp->dad_arp_tcount = 0;
	dp->dad_arp_ocount++;

	memset(&sip, 0, sizeof(sip));
	arprequest(ifa->ifa_ifp, &sip, &ia->ia_addr.sin_addr,
	    CLLADDR(ifa->ifa_ifp->if_sadl));
}

/*
 * Start Duplicate Address Detection (DAD) for specified interface address.
 */
static void
arp_dad_start(struct ifaddr *ifa)
{
	struct in_ifaddr *ia = (struct in_ifaddr *)ifa;
	struct dadq *dp;

	if (!dad_init) {
		TAILQ_INIT(&dadq);
		dad_init++;
	}

	/*
	 * If we don't need DAD, don't do it.
	 * - DAD is disabled (ip_dad_count == 0)
	 */
	if (!(ia->ia4_flags & IN_IFF_TENTATIVE)) {
		log(LOG_DEBUG,
			"arp_dad_start: called with non-tentative address "
			"%s(%s)\n",
			in_fmtaddr(ia->ia_addr.sin_addr),
			ifa->ifa_ifp ? if_name(ifa->ifa_ifp) : "???");
		return;
	}
	if (!ip_dad_count) {
		struct in_addr *ip = &IA_SIN(ifa)->sin_addr;

		ia->ia4_flags &= ~IN_IFF_TENTATIVE;
		rt_newaddrmsg(RTM_NEWADDR, ifa, 0, NULL);
		arprequest(ifa->ifa_ifp, ip, ip,
		    CLLADDR(ifa->ifa_ifp->if_sadl));
		return;
	}
	if (ifa->ifa_ifp == NULL)
		panic("arp_dad_start: ifa->ifa_ifp == NULL");
	if (!(ifa->ifa_ifp->if_flags & IFF_UP))
		return;
	if (arp_dad_find(ifa) != NULL) {
		/* DAD already in progress */
		return;
	}

	dp = malloc(sizeof(*dp), M_IPARP, M_NOWAIT);
	if (dp == NULL) {
		log(LOG_ERR, "arp_dad_start: memory allocation failed for "
			"%s(%s)\n",
			in_fmtaddr(ia->ia_addr.sin_addr),
			ifa->ifa_ifp ? if_name(ifa->ifa_ifp) : "???");
		return;
	}
	memset(dp, 0, sizeof(*dp));
	callout_init(&dp->dad_timer_ch, CALLOUT_MPSAFE);
	TAILQ_INSERT_TAIL(&dadq, (struct dadq *)dp, dad_list);

	arplog((LOG_DEBUG, "%s: starting DAD for %s\n", if_name(ifa->ifa_ifp),
	    in_fmtaddr(ia->ia_addr.sin_addr)));

	/*
	 * Send ARP packet for DAD, ip_dad_count times.
	 * Note that we must delay the first transmission.
	 */
	dp->dad_ifa = ifa;
	ifaref(ifa);	/* just for safety */
	dp->dad_count = ip_dad_count;
	dp->dad_arp_announce = 0; /* Will be set when starting to announce */
	dp->dad_arp_acount = dp->dad_arp_ocount = dp->dad_arp_tcount = 0;

	arp_dad_starttimer(dp, cprng_fast32() % (PROBE_WAIT * hz));
}

/*
 * terminate DAD unconditionally.  used for address removals.
 */
static void
arp_dad_stop(struct ifaddr *ifa)
{
	struct dadq *dp;

	if (!dad_init)
		return;
	dp = arp_dad_find(ifa);
	if (dp == NULL) {
		/* DAD wasn't started yet */
		return;
	}

	arp_dad_stoptimer(dp);

	TAILQ_REMOVE(&dadq, dp, dad_list);
	free(dp, M_IPARP);
	dp = NULL;
	ifafree(ifa);
}

static void
arp_dad_timer(struct ifaddr *ifa)
{
	struct in_ifaddr *ia = (struct in_ifaddr *)ifa;
	struct dadq *dp;
	struct in_addr *ip;

	mutex_enter(softnet_lock);
	KERNEL_LOCK(1, NULL);

	/* Sanity check */
	if (ia == NULL) {
		log(LOG_ERR, "arp_dad_timer: called with null parameter\n");
		goto done;
	}
	dp = arp_dad_find(ifa);
	if (dp == NULL) {
		log(LOG_ERR, "arp_dad_timer: DAD structure not found\n");
		goto done;
	}
	if (ia->ia4_flags & IN_IFF_DUPLICATED) {
		log(LOG_ERR, "nd4_dad_timer: called with duplicate address "
			"%s(%s)\n",
			in_fmtaddr(ia->ia_addr.sin_addr),
			ifa->ifa_ifp ? if_name(ifa->ifa_ifp) : "???");
		goto done;
	}
	if ((ia->ia4_flags & IN_IFF_TENTATIVE) == 0 && dp->dad_arp_acount == 0){
		log(LOG_ERR, "arp_dad_timer: called with non-tentative address "
			"%s(%s)\n",
			in_fmtaddr(ia->ia_addr.sin_addr),
			ifa->ifa_ifp ? if_name(ifa->ifa_ifp) : "???");
		goto done;
	}

	/* timeouted with IFF_{RUNNING,UP} check */
	if (dp->dad_arp_tcount > dad_maxtry) {
		arplog((LOG_INFO, "%s: could not run DAD, driver problem?\n",
			if_name(ifa->ifa_ifp)));

		TAILQ_REMOVE(&dadq, dp, dad_list);
		free(dp, M_IPARP);
		dp = NULL;
		ifafree(ifa);
		goto done;
	}

	/* Need more checks? */
	if (dp->dad_arp_ocount < dp->dad_count) {
		int adelay;

		/*
		 * We have more ARP to go.  Send ARP packet for DAD.
		 */
		arp_dad_output(dp, ifa);
		if (dp->dad_arp_ocount < dp->dad_count)
			adelay = (PROBE_MIN * hz) +
			    (cprng_fast32() %
			    ((PROBE_MAX * hz) - (PROBE_MIN * hz)));
		else
			adelay = ANNOUNCE_WAIT * hz;
		arp_dad_starttimer(dp, adelay);
		goto done;
	} else if (dp->dad_arp_acount == 0) {
		/*
		 * We are done with DAD.
		 * No duplicate address found.
		 */
		ia->ia4_flags &= ~IN_IFF_TENTATIVE;
		rt_newaddrmsg(RTM_NEWADDR, ifa, 0, NULL);
		arplog((LOG_DEBUG,
		    "%s: DAD complete for %s - no duplicates found\n",
		    if_name(ifa->ifa_ifp),
		    in_fmtaddr(ia->ia_addr.sin_addr)));
		dp->dad_arp_announce = ANNOUNCE_NUM;
		goto announce;
	} else if (dp->dad_arp_acount < dp->dad_arp_announce) {
announce:
		/*
		 * Announce the address.
		 */
		ip = &IA_SIN(ifa)->sin_addr;
		arprequest(ifa->ifa_ifp, ip, ip,
		    CLLADDR(ifa->ifa_ifp->if_sadl));
		dp->dad_arp_acount++;
		if (dp->dad_arp_acount < dp->dad_arp_announce) {
			arp_dad_starttimer(dp, ANNOUNCE_INTERVAL * hz);
			goto done;
		}
		arplog((LOG_DEBUG,
		    "%s: ARP announcement complete for %s\n",
		    if_name(ifa->ifa_ifp),
		    in_fmtaddr(ia->ia_addr.sin_addr)));
	}

	TAILQ_REMOVE(&dadq, dp, dad_list);
	free(dp, M_IPARP);
	dp = NULL;
	ifafree(ifa);

done:
	KERNEL_UNLOCK_ONE(NULL);
	mutex_exit(softnet_lock);
}

static void
arp_dad_duplicated(struct ifaddr *ifa)
{
	struct in_ifaddr *ia = (struct in_ifaddr *)ifa;
	struct ifnet *ifp;
	struct dadq *dp;

	dp = arp_dad_find(ifa);
	if (dp == NULL) {
		log(LOG_ERR, "arp_dad_duplicated: DAD structure not found\n");
		return;
	}

	ifp = ifa->ifa_ifp;
	log(LOG_ERR, "%s: DAD detected duplicate IPv4 address %s: "
	    "ARP out=%d\n",
	    if_name(ifp), in_fmtaddr(ia->ia_addr.sin_addr),
	    dp->dad_arp_ocount);

	ia->ia4_flags &= ~IN_IFF_TENTATIVE;
	ia->ia4_flags |= IN_IFF_DUPLICATED;

	/* We are done with DAD, with duplicated address found. (failure) */
	arp_dad_stoptimer(dp);

	/* Inform the routing socket that DAD has completed */
	rt_newaddrmsg(RTM_NEWADDR, ifa, 0, NULL);

	TAILQ_REMOVE(&dadq, dp, dad_list);
	free(dp, M_IPARP);
	dp = NULL;
	ifafree(ifa);
}

/*
 * Called from 10 Mb/s Ethernet interrupt handlers
 * when ether packet type ETHERTYPE_REVARP
 * is received.  Common length and type checks are done here,
 * then the protocol-specific routine is called.
 */
void
revarpinput(struct mbuf *m)
{
	struct arphdr *ar;

	if (m->m_len < sizeof(struct arphdr))
		goto out;
	ar = mtod(m, struct arphdr *);
#if 0 /* XXX I don't think we need this... and it will prevent other LL */
	if (ntohs(ar->ar_hrd) != ARPHRD_ETHER)
		goto out;
#endif
	if (m->m_len < sizeof(struct arphdr) + 2 * (ar->ar_hln + ar->ar_pln))
		goto out;
	switch (ntohs(ar->ar_pro)) {
	case ETHERTYPE_IP:
	case ETHERTYPE_IPTRAILERS:
		in_revarpinput(m);
		return;

	default:
		break;
	}
out:
	m_freem(m);
}

/*
 * RARP for Internet protocols on 10 Mb/s Ethernet.
 * Algorithm is that given in RFC 903.
 * We are only using for bootstrap purposes to get an ip address for one of
 * our interfaces.  Thus we support no user-interface.
 *
 * Since the contents of the RARP reply are specific to the interface that
 * sent the request, this code must ensure that they are properly associated.
 *
 * Note: also supports ARP via RARP packets, per the RFC.
 */
void
in_revarpinput(struct mbuf *m)
{
	struct ifnet *ifp;
	struct arphdr *ah;
	void *tha;
	int op;

	ah = mtod(m, struct arphdr *);
	op = ntohs(ah->ar_op);

	switch (m->m_pkthdr.rcvif->if_type) {
	case IFT_IEEE1394:
		/* ARP without target hardware address is not supported */
		goto out;
	default:
		break;
	}

	switch (op) {
	case ARPOP_REQUEST:
	case ARPOP_REPLY:	/* per RFC */
		in_arpinput(m);
		return;
	case ARPOP_REVREPLY:
		break;
	case ARPOP_REVREQUEST:	/* handled by rarpd(8) */
	default:
		goto out;
	}
	if (!revarp_in_progress)
		goto out;
	ifp = m->m_pkthdr.rcvif;
	if (ifp != myip_ifp) /* !same interface */
		goto out;
	if (myip_initialized)
		goto wake;
	tha = ar_tha(ah);
	if (tha == NULL)
		goto out;
	if (memcmp(tha, CLLADDR(ifp->if_sadl), ifp->if_sadl->sdl_alen))
		goto out;
	memcpy(&srv_ip, ar_spa(ah), sizeof(srv_ip));
	memcpy(&myip, ar_tpa(ah), sizeof(myip));
	myip_initialized = 1;
wake:	/* Do wakeup every time in case it was missed. */
	wakeup((void *)&myip);

out:
	m_freem(m);
}

/*
 * Send a RARP request for the ip address of the specified interface.
 * The request should be RFC 903-compliant.
 */
void
revarprequest(struct ifnet *ifp)
{
	struct sockaddr sa;
	struct mbuf *m;
	struct arphdr *ah;
	void *tha;

	if ((m = m_gethdr(M_DONTWAIT, MT_DATA)) == NULL)
		return;
	MCLAIM(m, &arpdomain.dom_mowner);
	m->m_len = sizeof(*ah) + 2*sizeof(struct in_addr) +
	    2*ifp->if_addrlen;
	m->m_pkthdr.len = m->m_len;
	MH_ALIGN(m, m->m_len);
	ah = mtod(m, struct arphdr *);
	memset(ah, 0, m->m_len);
	ah->ar_pro = htons(ETHERTYPE_IP);
	ah->ar_hln = ifp->if_addrlen;		/* hardware address length */
	ah->ar_pln = sizeof(struct in_addr);	/* protocol address length */
	ah->ar_op = htons(ARPOP_REVREQUEST);

	memcpy(ar_sha(ah), CLLADDR(ifp->if_sadl), ah->ar_hln);
	tha = ar_tha(ah);
	if (tha == NULL) {
		m_free(m);
		return;
	}
	memcpy(tha, CLLADDR(ifp->if_sadl), ah->ar_hln);

	sa.sa_family = AF_ARP;
	sa.sa_len = 2;
	m->m_flags |= M_BCAST;

	KERNEL_LOCK(1, NULL);
	(*ifp->if_output)(ifp, m, &sa, NULL);
	KERNEL_UNLOCK_ONE(NULL);
}

/*
 * RARP for the ip address of the specified interface, but also
 * save the ip address of the server that sent the answer.
 * Timeout if no response is received.
 */
int
revarpwhoarewe(struct ifnet *ifp, struct in_addr *serv_in,
    struct in_addr *clnt_in)
{
	int result, count = 20;

	myip_initialized = 0;
	myip_ifp = ifp;

	revarp_in_progress = 1;
	while (count--) {
		revarprequest(ifp);
		result = tsleep((void *)&myip, PSOCK, "revarp", hz/2);
		if (result != EWOULDBLOCK)
			break;
	}
	revarp_in_progress = 0;

	if (!myip_initialized)
		return ENETUNREACH;

	memcpy(serv_in, &srv_ip, sizeof(*serv_in));
	memcpy(clnt_in, &myip, sizeof(*clnt_in));
	return 0;
}



#ifdef DDB

#include <machine/db_machdep.h>
#include <ddb/db_interface.h>
#include <ddb/db_output.h>

static void
db_print_sa(const struct sockaddr *sa)
{
	int len;
	const u_char *p;

	if (sa == NULL) {
		db_printf("[NULL]");
		return;
	}

	p = (const u_char *)sa;
	len = sa->sa_len;
	db_printf("[");
	while (len > 0) {
		db_printf("%d", *p);
		p++; len--;
		if (len) db_printf(",");
	}
	db_printf("]\n");
}

static void
db_print_ifa(struct ifaddr *ifa)
{
	if (ifa == NULL)
		return;
	db_printf("  ifa_addr=");
	db_print_sa(ifa->ifa_addr);
	db_printf("  ifa_dsta=");
	db_print_sa(ifa->ifa_dstaddr);
	db_printf("  ifa_mask=");
	db_print_sa(ifa->ifa_netmask);
	db_printf("  flags=0x%x,refcnt=%d,metric=%d\n",
			  ifa->ifa_flags,
			  ifa->ifa_refcnt,
			  ifa->ifa_metric);
}

static void
db_print_llinfo(void *li)
{
	struct llinfo_arp *la;

	if (li == NULL)
		return;
	la = (struct llinfo_arp *)li;
	db_printf("  la_rt=%p la_hold=%p, la_asked=0x%lx\n",
			  la->la_rt, la->la_hold, la->la_asked);
}

/*
 * Function to pass to rt_walktree().
 * Return non-zero error to abort walk.
 */
static int
db_show_rtentry(struct rtentry *rt, void *w)
{
	db_printf("rtentry=%p", rt);

	db_printf(" flags=0x%x refcnt=%d use=%"PRId64" expire=%"PRId64"\n",
			  rt->rt_flags, rt->rt_refcnt,
			  rt->rt_use, (uint64_t)rt->rt_expire);

	db_printf(" key="); db_print_sa(rt_getkey(rt));
	db_printf(" mask="); db_print_sa(rt_mask(rt));
	db_printf(" gw="); db_print_sa(rt->rt_gateway);

	db_printf(" ifp=%p ", rt->rt_ifp);
	if (rt->rt_ifp)
		db_printf("(%s)", rt->rt_ifp->if_xname);
	else
		db_printf("(NULL)");

	db_printf(" ifa=%p\n", rt->rt_ifa);
	db_print_ifa(rt->rt_ifa);

	db_printf(" gwroute=%p llinfo=%p\n",
			  rt->rt_gwroute, rt->rt_llinfo);
	db_print_llinfo(rt->rt_llinfo);

	return 0;
}

/*
 * Function to print all the route trees.
 * Use this from ddb:  "show arptab"
 */
void
db_show_arptab(db_expr_t addr, bool have_addr,
    db_expr_t count, const char *modif)
{
	rt_walktree(AF_INET, db_show_rtentry, NULL);
}
#endif

void
arp_stat_add(int type, uint64_t count)
{
	ARP_STATADD(type, count);
}

static int
sysctl_net_inet_arp_stats(SYSCTLFN_ARGS)
{

	return NETSTAT_SYSCTL(arpstat_percpu, ARP_NSTATS);
}

static void
sysctl_net_inet_arp_setup(struct sysctllog **clog)
{
	const struct sysctlnode *node;

	sysctl_createv(clog, 0, NULL, NULL,
			CTLFLAG_PERMANENT,
			CTLTYPE_NODE, "inet", NULL,
			NULL, 0, NULL, 0,
			CTL_NET, PF_INET, CTL_EOL);
	sysctl_createv(clog, 0, NULL, &node,
			CTLFLAG_PERMANENT,
			CTLTYPE_NODE, "arp",
			SYSCTL_DESCR("Address Resolution Protocol"),
			NULL, 0, NULL, 0,
			CTL_NET, PF_INET, CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
			CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
			CTLTYPE_INT, "keep",
			SYSCTL_DESCR("Valid ARP entry lifetime in seconds"),
			NULL, 0, &arpt_keep, 0,
			CTL_NET,PF_INET, node->sysctl_num, CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
			CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
			CTLTYPE_INT, "down",
			SYSCTL_DESCR("Failed ARP entry lifetime in seconds"),
			NULL, 0, &arpt_down, 0,
			CTL_NET,PF_INET, node->sysctl_num, CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
			CTLFLAG_PERMANENT,
			CTLTYPE_STRUCT, "stats",
			SYSCTL_DESCR("ARP statistics"),
			sysctl_net_inet_arp_stats, 0, NULL, 0,
			CTL_NET,PF_INET, node->sysctl_num, CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
			CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
			CTLTYPE_INT, "log_movements",
			SYSCTL_DESCR("log ARP replies from MACs different than"
			    " the one in the cache"),
			NULL, 0, &log_movements, 0,
			CTL_NET,PF_INET, node->sysctl_num, CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
			CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
			CTLTYPE_INT, "log_permanent_modify",
			SYSCTL_DESCR("log ARP replies from MACs different than"
			    " the one in the permanent arp entry"),
			NULL, 0, &log_permanent_modify, 0,
			CTL_NET,PF_INET, node->sysctl_num, CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
			CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
			CTLTYPE_INT, "log_wrong_iface",
			SYSCTL_DESCR("log ARP packets arriving on the wrong"
			    " interface"),
			NULL, 0, &log_wrong_iface, 0,
			CTL_NET,PF_INET, node->sysctl_num, CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
			CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
			CTLTYPE_INT, "log_unknown_network",
			SYSCTL_DESCR("log ARP packets from non-local network"),
			NULL, 0, &log_unknown_network, 0,
			CTL_NET,PF_INET, node->sysctl_num, CTL_CREATE, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "debug",
		       SYSCTL_DESCR("Enable ARP DAD debug output"),
		       NULL, 0, &arp_debug, 0,
		       CTL_NET, PF_INET, node->sysctl_num, CTL_CREATE, CTL_EOL);
}

#endif /* INET */
