/*	$NetBSD: ip_input.c,v 1.324 2015/08/24 22:21:26 pooka Exp $	*/

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

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
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
 *	@(#)ip_input.c	8.2 (Berkeley) 1/4/94
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ip_input.c,v 1.324 2015/08/24 22:21:26 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#include "opt_compat_netbsd.h"
#include "opt_gateway.h"
#include "opt_ipsec.h"
#include "opt_mrouting.h"
#include "opt_mbuftrace.h"
#include "opt_inet_csum.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cpu.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/pool.h>
#include <sys/sysctl.h>
#include <sys/kauth.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/pktqueue.h>
#include <net/pfil.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_proto.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#include <netinet/ip_private.h>
#include <netinet/ip_icmp.h>
/* just for gif_ttl */
#include <netinet/in_gif.h>
#include "gif.h"
#include <net/if_gre.h>
#include "gre.h"

#ifdef MROUTING
#include <netinet/ip_mroute.h>
#endif
#include <netinet/portalgo.h>

#ifdef IPSEC
#include <netipsec/ipsec.h>
#endif

#ifndef	IPFORWARDING
#ifdef GATEWAY
#define	IPFORWARDING	1	/* forward IP packets not for us */
#else /* GATEWAY */
#define	IPFORWARDING	0	/* don't forward IP packets not for us */
#endif /* GATEWAY */
#endif /* IPFORWARDING */
#ifndef	IPSENDREDIRECTS
#define	IPSENDREDIRECTS	1
#endif
#ifndef IPFORWSRCRT
#define	IPFORWSRCRT	1	/* forward source-routed packets */
#endif
#ifndef IPALLOWSRCRT
#define	IPALLOWSRCRT	1	/* allow source-routed packets */
#endif
#ifndef IPMTUDISC
#define IPMTUDISC	1
#endif
#ifndef IPMTUDISCTIMEOUT
#define IPMTUDISCTIMEOUT (10 * 60)	/* as per RFC 1191 */
#endif

#ifdef COMPAT_50
#include <compat/sys/time.h>
#include <compat/sys/socket.h>
#endif

/*
 * Note: DIRECTED_BROADCAST is handled this way so that previous
 * configuration using this option will Just Work.
 */
#ifndef IPDIRECTEDBCAST
#ifdef DIRECTED_BROADCAST
#define IPDIRECTEDBCAST	1
#else
#define	IPDIRECTEDBCAST	0
#endif /* DIRECTED_BROADCAST */
#endif /* IPDIRECTEDBCAST */
int	ipforwarding = IPFORWARDING;
int	ipsendredirects = IPSENDREDIRECTS;
int	ip_defttl = IPDEFTTL;
int	ip_forwsrcrt = IPFORWSRCRT;
int	ip_directedbcast = IPDIRECTEDBCAST;
int	ip_allowsrcrt = IPALLOWSRCRT;
int	ip_mtudisc = IPMTUDISC;
int	ip_mtudisc_timeout = IPMTUDISCTIMEOUT;
#ifdef DIAGNOSTIC
int	ipprintfs = 0;
#endif

int	ip_do_randomid = 0;

/*
 * XXX - Setting ip_checkinterface mostly implements the receive side of
 * the Strong ES model described in RFC 1122, but since the routing table
 * and transmit implementation do not implement the Strong ES model,
 * setting this to 1 results in an odd hybrid.
 *
 * XXX - ip_checkinterface currently must be disabled if you use ipnat
 * to translate the destination address to another local interface.
 *
 * XXX - ip_checkinterface must be disabled if you add IP aliases
 * to the loopback interface instead of the interface where the
 * packets for those addresses are received.
 */
static int		ip_checkinterface	__read_mostly = 0;

struct rttimer_queue *ip_mtudisc_timeout_q = NULL;

pktqueue_t *		ip_pktq			__read_mostly;
pfil_head_t *		inet_pfil_hook		__read_mostly;
ipid_state_t *		ip_ids			__read_mostly;
percpu_t *		ipstat_percpu		__read_mostly;

static struct route	ipforward_rt		__cacheline_aligned;

uint16_t ip_id;

#ifdef INET_CSUM_COUNTERS
#include <sys/device.h>

struct evcnt ip_hwcsum_bad = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "inet", "hwcsum bad");
struct evcnt ip_hwcsum_ok = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "inet", "hwcsum ok");
struct evcnt ip_swcsum = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "inet", "swcsum");

#define	INET_CSUM_COUNTER_INCR(ev)	(ev)->ev_count++

EVCNT_ATTACH_STATIC(ip_hwcsum_bad);
EVCNT_ATTACH_STATIC(ip_hwcsum_ok);
EVCNT_ATTACH_STATIC(ip_swcsum);

#else

#define	INET_CSUM_COUNTER_INCR(ev)	/* nothing */

#endif /* INET_CSUM_COUNTERS */

/*
 * We need to save the IP options in case a protocol wants to respond
 * to an incoming packet over the same route if the packet got here
 * using IP source routing.  This allows connection establishment and
 * maintenance when the remote end is on a network that is not known
 * to us.
 */

static int	ip_nhops = 0;

static	struct ip_srcrt {
	struct	in_addr dst;			/* final destination */
	char	nop;				/* one NOP to align */
	char	srcopt[IPOPT_OFFSET + 1];	/* OPTVAL, OLEN and OFFSET */
	struct	in_addr route[MAX_IPOPTLEN/sizeof(struct in_addr)];
} ip_srcrt;

static int ip_drainwanted;

struct	sockaddr_in ipaddr = {
	.sin_len = sizeof(ipaddr),
	.sin_family = AF_INET,
};

static void save_rte(u_char *, struct in_addr);

#ifdef MBUFTRACE
struct mowner ip_rx_mowner = MOWNER_INIT("internet", "rx");
struct mowner ip_tx_mowner = MOWNER_INIT("internet", "tx");
#endif

static void		ipintr(void *);
static void		ip_input(struct mbuf *);
static void		ip_forward(struct mbuf *, int);
static bool		ip_dooptions(struct mbuf *);
static struct in_ifaddr *ip_rtaddr(struct in_addr);
static void		sysctl_net_inet_ip_setup(struct sysctllog **);

/* XXX: Not yet enabled. */
#define	SOFTNET_LOCK()		KASSERT(mutex_owned(softnet_lock))
#define	SOFTNET_UNLOCK()	KASSERT(mutex_owned(softnet_lock))

/*
 * IP initialization: fill in IP protocol switch table.
 * All protocols not implemented in kernel go to raw IP protocol handler.
 */
void
ip_init(void)
{
	const struct protosw *pr;

	in_init();
	sysctl_net_inet_ip_setup(NULL);

	pr = pffindproto(PF_INET, IPPROTO_RAW, SOCK_RAW);
	KASSERT(pr != NULL);

	ip_pktq = pktq_create(IFQ_MAXLEN, ipintr, NULL);
	KASSERT(ip_pktq != NULL);

	for (u_int i = 0; i < IPPROTO_MAX; i++) {
		ip_protox[i] = pr - inetsw;
	}
	for (pr = inetdomain.dom_protosw;
	    pr < inetdomain.dom_protoswNPROTOSW; pr++)
		if (pr->pr_domain->dom_family == PF_INET &&
		    pr->pr_protocol && pr->pr_protocol != IPPROTO_RAW)
			ip_protox[pr->pr_protocol] = pr - inetsw;

	ip_reass_init();

	ip_ids = ip_id_init();
	ip_id = time_uptime & 0xfffff;

	ip_mtudisc_timeout_q = rt_timer_queue_create(ip_mtudisc_timeout);
#ifdef GATEWAY
	ipflow_init();
#endif

	/* Register our Packet Filter hook. */
	inet_pfil_hook = pfil_head_create(PFIL_TYPE_AF, (void *)AF_INET);
	KASSERT(inet_pfil_hook != NULL);

#ifdef MBUFTRACE
	MOWNER_ATTACH(&ip_tx_mowner);
	MOWNER_ATTACH(&ip_rx_mowner);
#endif /* MBUFTRACE */

	ipstat_percpu = percpu_alloc(sizeof(uint64_t) * IP_NSTATS);
}

/*
 * IP software interrupt routine.
 */
static void
ipintr(void *arg __unused)
{
	struct mbuf *m;

	KASSERT(cpu_softintr_p());

	mutex_enter(softnet_lock);
	while ((m = pktq_dequeue(ip_pktq)) != NULL) {
		ip_input(m);
	}
	mutex_exit(softnet_lock);
}

/*
 * IP input routine.  Checksum and byte swap header.  If fragmented
 * try to reassemble.  Process options.  Pass to next level.
 */
static void
ip_input(struct mbuf *m)
{
	struct ip *ip = NULL;
	struct in_ifaddr *ia;
	struct ifaddr *ifa;
	int hlen = 0, len;
	int downmatch;
	int checkif;
	int srcrt = 0;
	ifnet_t *ifp;

	KASSERTMSG(cpu_softintr_p(), "ip_input: not in the software "
	    "interrupt handler; synchronization assumptions violated");

	MCLAIM(m, &ip_rx_mowner);
	KASSERT((m->m_flags & M_PKTHDR) != 0);
	ifp = m->m_pkthdr.rcvif;

	/*
	 * If no IP addresses have been set yet but the interfaces
	 * are receiving, can't do anything with incoming packets yet.
	 * Note: we pre-check without locks held.
	 */
	if (!TAILQ_FIRST(&in_ifaddrhead)) {
		goto bad;
	}
	IP_STATINC(IP_STAT_TOTAL);

	/*
	 * If the IP header is not aligned, slurp it up into a new
	 * mbuf with space for link headers, in the event we forward
	 * it.  Otherwise, if it is aligned, make sure the entire
	 * base IP header is in the first mbuf of the chain.
	 */
	if (IP_HDR_ALIGNED_P(mtod(m, void *)) == 0) {
		if ((m = m_copyup(m, sizeof(struct ip),
				  (max_linkhdr + 3) & ~3)) == NULL) {
			/* XXXJRT new stat, please */
			IP_STATINC(IP_STAT_TOOSMALL);
			return;
		}
	} else if (__predict_false(m->m_len < sizeof (struct ip))) {
		if ((m = m_pullup(m, sizeof (struct ip))) == NULL) {
			IP_STATINC(IP_STAT_TOOSMALL);
			return;
		}
	}
	ip = mtod(m, struct ip *);
	if (ip->ip_v != IPVERSION) {
		IP_STATINC(IP_STAT_BADVERS);
		goto bad;
	}
	hlen = ip->ip_hl << 2;
	if (hlen < sizeof(struct ip)) {	/* minimum header length */
		IP_STATINC(IP_STAT_BADHLEN);
		goto bad;
	}
	if (hlen > m->m_len) {
		if ((m = m_pullup(m, hlen)) == NULL) {
			IP_STATINC(IP_STAT_BADHLEN);
			return;
		}
		ip = mtod(m, struct ip *);
	}

	/*
	 * RFC1122: packets with a multicast source address are
	 * not allowed.
	 */
	if (IN_MULTICAST(ip->ip_src.s_addr)) {
		IP_STATINC(IP_STAT_BADADDR);
		goto bad;
	}

	/* 127/8 must not appear on wire - RFC1122 */
	if ((ntohl(ip->ip_dst.s_addr) >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET ||
	    (ntohl(ip->ip_src.s_addr) >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET) {
		if ((ifp->if_flags & IFF_LOOPBACK) == 0) {
			IP_STATINC(IP_STAT_BADADDR);
			goto bad;
		}
	}

	switch (m->m_pkthdr.csum_flags &
		((ifp->if_csum_flags_rx & M_CSUM_IPv4) |
		 M_CSUM_IPv4_BAD)) {
	case M_CSUM_IPv4|M_CSUM_IPv4_BAD:
		INET_CSUM_COUNTER_INCR(&ip_hwcsum_bad);
		goto badcsum;

	case M_CSUM_IPv4:
		/* Checksum was okay. */
		INET_CSUM_COUNTER_INCR(&ip_hwcsum_ok);
		break;

	default:
		/*
		 * Must compute it ourselves.  Maybe skip checksum on
		 * loopback interfaces.
		 */
		if (__predict_true(!(ifp->if_flags & IFF_LOOPBACK) ||
		    ip_do_loopback_cksum)) {
			INET_CSUM_COUNTER_INCR(&ip_swcsum);
			if (in_cksum(m, hlen) != 0)
				goto badcsum;
		}
		break;
	}

	/* Retrieve the packet length. */
	len = ntohs(ip->ip_len);

	/*
	 * Check for additional length bogosity
	 */
	if (len < hlen) {
		IP_STATINC(IP_STAT_BADLEN);
		goto bad;
	}

	/*
	 * Check that the amount of data in the buffers
	 * is as at least much as the IP header would have us expect.
	 * Trim mbufs if longer than we expect.
	 * Drop packet if shorter than we expect.
	 */
	if (m->m_pkthdr.len < len) {
		IP_STATINC(IP_STAT_TOOSHORT);
		goto bad;
	}
	if (m->m_pkthdr.len > len) {
		if (m->m_len == m->m_pkthdr.len) {
			m->m_len = len;
			m->m_pkthdr.len = len;
		} else
			m_adj(m, len - m->m_pkthdr.len);
	}

	/*
	 * Assume that we can create a fast-forward IP flow entry
	 * based on this packet.
	 */
	m->m_flags |= M_CANFASTFWD;

	/*
	 * Run through list of hooks for input packets.  If there are any
	 * filters which require that additional packets in the flow are
	 * not fast-forwarded, they must clear the M_CANFASTFWD flag.
	 * Note that filters must _never_ set this flag, as another filter
	 * in the list may have previously cleared it.
	 */
#if defined(IPSEC)
	if (!ipsec_used || !ipsec_indone(m))
#else
	if (1)
#endif
	{
		struct in_addr odst = ip->ip_dst;
		bool freed;

		SOFTNET_LOCK();
		freed = pfil_run_hooks(inet_pfil_hook, &m, ifp, PFIL_IN) != 0;
		SOFTNET_UNLOCK();
		if (freed || m == NULL) {
			return;
		}
		ip = mtod(m, struct ip *);
		hlen = ip->ip_hl << 2;

		/*
		 * XXX The setting of "srcrt" here is to prevent ip_forward()
		 * from generating ICMP redirects for packets that have
		 * been redirected by a hook back out on to the same LAN that
		 * they came from and is not an indication that the packet
		 * is being inffluenced by source routing options.  This
		 * allows things like
		 * "rdr tlp0 0/0 port 80 -> 1.1.1.200 3128 tcp"
		 * where tlp0 is both on the 1.1.1.0/24 network and is the
		 * default route for hosts on 1.1.1.0/24.  Of course this
		 * also requires a "map tlp0 ..." to complete the story.
		 * One might argue whether or not this kind of network config.
		 * should be supported in this manner...
		 */
		srcrt = (odst.s_addr != ip->ip_dst.s_addr);
	}

#ifdef ALTQ
	/* XXX Temporary until ALTQ is changed to use a pfil hook */
	if (altq_input) {
		SOFTNET_LOCK();
		if ((*altq_input)(m, AF_INET) == 0) {
			/* Packet dropped by traffic conditioner. */
			SOFTNET_UNLOCK();
			return;
		}
		SOFTNET_UNLOCK();
	}
#endif

	/*
	 * Process options and, if not destined for us,
	 * ship it on.  ip_dooptions returns 1 when an
	 * error was detected (causing an icmp message
	 * to be sent and the original packet to be freed).
	 */
	ip_nhops = 0;		/* for source routed packets */
	if (hlen > sizeof (struct ip) && ip_dooptions(m))
		return;

	/*
	 * Enable a consistency check between the destination address
	 * and the arrival interface for a unicast packet (the RFC 1122
	 * strong ES model) if IP forwarding is disabled and the packet
	 * is not locally generated.
	 *
	 * XXX - Checking also should be disabled if the destination
	 * address is ipnat'ed to a different interface.
	 *
	 * XXX - Checking is incompatible with IP aliases added
	 * to the loopback interface instead of the interface where
	 * the packets are received.
	 *
	 * XXX - We need to add a per ifaddr flag for this so that
	 * we get finer grain control.
	 */
	checkif = ip_checkinterface && (ipforwarding == 0) &&
	    (ifp->if_flags & IFF_LOOPBACK) == 0;

	/*
	 * Check our list of addresses, to see if the packet is for us.
	 *
	 * Traditional 4.4BSD did not consult IFF_UP at all.
	 * The behavior here is to treat addresses on !IFF_UP interface
	 * or IN_IFF_NOTREADY addresses as not mine.
	 */
	downmatch = 0;
	LIST_FOREACH(ia, &IN_IFADDR_HASH(ip->ip_dst.s_addr), ia_hash) {
		if (in_hosteq(ia->ia_addr.sin_addr, ip->ip_dst)) {
			if (ia->ia4_flags & IN_IFF_NOTREADY)
				continue;
			if (checkif && ia->ia_ifp != ifp)
				continue;
			if ((ia->ia_ifp->if_flags & IFF_UP) != 0)
				break;
			else
				downmatch++;
		}
	}
	if (ia != NULL)
		goto ours;
	if (ifp->if_flags & IFF_BROADCAST) {
		IFADDR_FOREACH(ifa, ifp) {
			if (ifa->ifa_addr->sa_family != AF_INET)
				continue;
			ia = ifatoia(ifa);
			if (ia->ia4_flags & IN_IFF_NOTREADY)
				continue;
			if (in_hosteq(ip->ip_dst, ia->ia_broadaddr.sin_addr) ||
			    in_hosteq(ip->ip_dst, ia->ia_netbroadcast) ||
			    /*
			     * Look for all-0's host part (old broadcast addr),
			     * either for subnet or net.
			     */
			    ip->ip_dst.s_addr == ia->ia_subnet ||
			    ip->ip_dst.s_addr == ia->ia_net)
				goto ours;
			/*
			 * An interface with IP address zero accepts
			 * all packets that arrive on that interface.
			 */
			if (in_nullhost(ia->ia_addr.sin_addr))
				goto ours;
		}
	}
	if (IN_MULTICAST(ip->ip_dst.s_addr)) {
#ifdef MROUTING
		extern struct socket *ip_mrouter;

		if (ip_mrouter) {
			/*
			 * If we are acting as a multicast router, all
			 * incoming multicast packets are passed to the
			 * kernel-level multicast forwarding function.
			 * The packet is returned (relatively) intact; if
			 * ip_mforward() returns a non-zero value, the packet
			 * must be discarded, else it may be accepted below.
			 *
			 * (The IP ident field is put in the same byte order
			 * as expected when ip_mforward() is called from
			 * ip_output().)
			 */
			SOFTNET_LOCK();
			if (ip_mforward(m, ifp) != 0) {
				SOFTNET_UNLOCK();
				IP_STATINC(IP_STAT_CANTFORWARD);
				m_freem(m);
				return;
			}
			SOFTNET_UNLOCK();

			/*
			 * The process-level routing demon needs to receive
			 * all multicast IGMP packets, whether or not this
			 * host belongs to their destination groups.
			 */
			if (ip->ip_p == IPPROTO_IGMP) {
				goto ours;
			}
			IP_STATINC(IP_STAT_CANTFORWARD);
		}
#endif
		/*
		 * See if we belong to the destination multicast group on the
		 * arrival interface.
		 */
		if (!in_multi_group(ip->ip_dst, ifp, 0)) {
			IP_STATINC(IP_STAT_CANTFORWARD);
			m_freem(m);
			return;
		}
		goto ours;
	}
	if (ip->ip_dst.s_addr == INADDR_BROADCAST ||
	    in_nullhost(ip->ip_dst))
		goto ours;

	/*
	 * Not for us; forward if possible and desirable.
	 */
	if (ipforwarding == 0) {
		IP_STATINC(IP_STAT_CANTFORWARD);
		m_freem(m);
	} else {
		/*
		 * If ip_dst matched any of my address on !IFF_UP interface,
		 * and there's no IFF_UP interface that matches ip_dst,
		 * send icmp unreach.  Forwarding it will result in in-kernel
		 * forwarding loop till TTL goes to 0.
		 */
		if (downmatch) {
			icmp_error(m, ICMP_UNREACH, ICMP_UNREACH_HOST, 0, 0);
			IP_STATINC(IP_STAT_CANTFORWARD);
			return;
		}
#ifdef IPSEC
		/* Perform IPsec, if any. */
		if (ipsec_used) {
			SOFTNET_LOCK();
			if (ipsec4_input(m, IP_FORWARDING |
			    (ip_directedbcast ? IP_ALLOWBROADCAST : 0)) != 0) {
				SOFTNET_UNLOCK();
				goto bad;
			}
			SOFTNET_UNLOCK();
		}
#endif
		ip_forward(m, srcrt);
	}
	return;

ours:
	/*
	 * If offset or IP_MF are set, must reassemble.
	 */
	if (ip->ip_off & ~htons(IP_DF|IP_RF)) {
		/*
		 * Pass to IP reassembly mechanism.
		 */
		if (ip_reass_packet(&m, ip) != 0) {
			/* Failed; invalid fragment(s) or packet. */
			goto bad;
		}
		if (m == NULL) {
			/* More fragments should come; silently return. */
			return;
		}
		/*
		 * Reassembly is done, we have the final packet.
		 * Updated cached data in local variable(s).
		 */
		ip = mtod(m, struct ip *);
		hlen = ip->ip_hl << 2;
	}

#ifdef IPSEC
	/*
	 * Enforce IPsec policy checking if we are seeing last header.
	 * Note that we do not visit this with protocols with PCB layer
	 * code - like UDP/TCP/raw IP.
	 */
	if (ipsec_used &&
	    (inetsw[ip_protox[ip->ip_p]].pr_flags & PR_LASTHDR) != 0) {
		SOFTNET_LOCK();
		if (ipsec4_input(m, 0) != 0) {
			SOFTNET_UNLOCK();
			goto bad;
		}
		SOFTNET_UNLOCK();
	}
#endif

	/*
	 * Switch out to protocol's input routine.
	 */
#if IFA_STATS
	if (ia && ip)
		ia->ia_ifa.ifa_data.ifad_inbytes += ntohs(ip->ip_len);
#endif
	IP_STATINC(IP_STAT_DELIVERED);

	const int off = hlen, nh = ip->ip_p;

	SOFTNET_LOCK();
	(*inetsw[ip_protox[nh]].pr_input)(m, off, nh);
	SOFTNET_UNLOCK();
	return;
bad:
	m_freem(m);
	return;

badcsum:
	IP_STATINC(IP_STAT_BADSUM);
	m_freem(m);
}

/*
 * IP timer processing.
 */
void
ip_slowtimo(void)
{

	mutex_enter(softnet_lock);
	KERNEL_LOCK(1, NULL);

	ip_reass_slowtimo();

	KERNEL_UNLOCK_ONE(NULL);
	mutex_exit(softnet_lock);
}

/*
 * IP drain processing.
 */
void
ip_drain(void)
{

	KERNEL_LOCK(1, NULL);
	ip_reass_drain();
	KERNEL_UNLOCK_ONE(NULL);
}

/*
 * ip_dooptions: perform option processing on a datagram, possibly discarding
 * it if bad options are encountered, or forwarding it if source-routed.
 *
 * => Returns true if packet has been forwarded/freed.
 * => Returns false if the packet should be processed further.
 */
static bool
ip_dooptions(struct mbuf *m)
{
	struct ip *ip = mtod(m, struct ip *);
	u_char *cp, *cp0;
	struct ip_timestamp *ipt;
	struct in_ifaddr *ia;
	int opt, optlen, cnt, off, code, type = ICMP_PARAMPROB, forward = 0;
	struct in_addr dst;
	n_time ntime;

	dst = ip->ip_dst;
	cp = (u_char *)(ip + 1);
	cnt = (ip->ip_hl << 2) - sizeof (struct ip);
	for (; cnt > 0; cnt -= optlen, cp += optlen) {
		opt = cp[IPOPT_OPTVAL];
		if (opt == IPOPT_EOL)
			break;
		if (opt == IPOPT_NOP)
			optlen = 1;
		else {
			if (cnt < IPOPT_OLEN + sizeof(*cp)) {
				code = &cp[IPOPT_OLEN] - (u_char *)ip;
				goto bad;
			}
			optlen = cp[IPOPT_OLEN];
			if (optlen < IPOPT_OLEN + sizeof(*cp) || optlen > cnt) {
				code = &cp[IPOPT_OLEN] - (u_char *)ip;
				goto bad;
			}
		}
		switch (opt) {

		default:
			break;

		/*
		 * Source routing with record.
		 * Find interface with current destination address.
		 * If none on this machine then drop if strictly routed,
		 * or do nothing if loosely routed.
		 * Record interface address and bring up next address
		 * component.  If strictly routed make sure next
		 * address is on directly accessible net.
		 */
		case IPOPT_LSRR:
		case IPOPT_SSRR:
			if (ip_allowsrcrt == 0) {
				type = ICMP_UNREACH;
				code = ICMP_UNREACH_NET_PROHIB;
				goto bad;
			}
			if (optlen < IPOPT_OFFSET + sizeof(*cp)) {
				code = &cp[IPOPT_OLEN] - (u_char *)ip;
				goto bad;
			}
			if ((off = cp[IPOPT_OFFSET]) < IPOPT_MINOFF) {
				code = &cp[IPOPT_OFFSET] - (u_char *)ip;
				goto bad;
			}
			ipaddr.sin_addr = ip->ip_dst;
			ia = ifatoia(ifa_ifwithaddr(sintosa(&ipaddr)));
			if (ia == 0) {
				if (opt == IPOPT_SSRR) {
					type = ICMP_UNREACH;
					code = ICMP_UNREACH_SRCFAIL;
					goto bad;
				}
				/*
				 * Loose routing, and not at next destination
				 * yet; nothing to do except forward.
				 */
				break;
			}
			off--;			/* 0 origin */
			if ((off + sizeof(struct in_addr)) > optlen) {
				/*
				 * End of source route.  Should be for us.
				 */
				save_rte(cp, ip->ip_src);
				break;
			}
			/*
			 * locate outgoing interface
			 */
			memcpy((void *)&ipaddr.sin_addr, (void *)(cp + off),
			    sizeof(ipaddr.sin_addr));
			if (opt == IPOPT_SSRR)
				ia = ifatoia(ifa_ifwithladdr(sintosa(&ipaddr)));
			else
				ia = ip_rtaddr(ipaddr.sin_addr);
			if (ia == 0) {
				type = ICMP_UNREACH;
				code = ICMP_UNREACH_SRCFAIL;
				goto bad;
			}
			ip->ip_dst = ipaddr.sin_addr;
			bcopy((void *)&ia->ia_addr.sin_addr,
			    (void *)(cp + off), sizeof(struct in_addr));
			cp[IPOPT_OFFSET] += sizeof(struct in_addr);
			/*
			 * Let ip_intr's mcast routing check handle mcast pkts
			 */
			forward = !IN_MULTICAST(ip->ip_dst.s_addr);
			break;

		case IPOPT_RR:
			if (optlen < IPOPT_OFFSET + sizeof(*cp)) {
				code = &cp[IPOPT_OLEN] - (u_char *)ip;
				goto bad;
			}
			if ((off = cp[IPOPT_OFFSET]) < IPOPT_MINOFF) {
				code = &cp[IPOPT_OFFSET] - (u_char *)ip;
				goto bad;
			}
			/*
			 * If no space remains, ignore.
			 */
			off--;			/* 0 origin */
			if ((off + sizeof(struct in_addr)) > optlen)
				break;
			memcpy((void *)&ipaddr.sin_addr, (void *)(&ip->ip_dst),
			    sizeof(ipaddr.sin_addr));
			/*
			 * locate outgoing interface; if we're the destination,
			 * use the incoming interface (should be same).
			 */
			if ((ia = ifatoia(ifa_ifwithaddr(sintosa(&ipaddr))))
			    == NULL &&
			    (ia = ip_rtaddr(ipaddr.sin_addr)) == NULL) {
				type = ICMP_UNREACH;
				code = ICMP_UNREACH_HOST;
				goto bad;
			}
			bcopy((void *)&ia->ia_addr.sin_addr,
			    (void *)(cp + off), sizeof(struct in_addr));
			cp[IPOPT_OFFSET] += sizeof(struct in_addr);
			break;

		case IPOPT_TS:
			code = cp - (u_char *)ip;
			ipt = (struct ip_timestamp *)cp;
			if (ipt->ipt_len < 4 || ipt->ipt_len > 40) {
				code = (u_char *)&ipt->ipt_len - (u_char *)ip;
				goto bad;
			}
			if (ipt->ipt_ptr < 5) {
				code = (u_char *)&ipt->ipt_ptr - (u_char *)ip;
				goto bad;
			}
			if (ipt->ipt_ptr > ipt->ipt_len - sizeof (int32_t)) {
				if (++ipt->ipt_oflw == 0) {
					code = (u_char *)&ipt->ipt_ptr -
					    (u_char *)ip;
					goto bad;
				}
				break;
			}
			cp0 = (cp + ipt->ipt_ptr - 1);
			switch (ipt->ipt_flg) {

			case IPOPT_TS_TSONLY:
				break;

			case IPOPT_TS_TSANDADDR:
				if (ipt->ipt_ptr - 1 + sizeof(n_time) +
				    sizeof(struct in_addr) > ipt->ipt_len) {
					code = (u_char *)&ipt->ipt_ptr -
					    (u_char *)ip;
					goto bad;
				}
				ipaddr.sin_addr = dst;
				ia = ifatoia(ifaof_ifpforaddr(sintosa(&ipaddr),
				    m->m_pkthdr.rcvif));
				if (ia == 0)
					continue;
				bcopy(&ia->ia_addr.sin_addr,
				    cp0, sizeof(struct in_addr));
				ipt->ipt_ptr += sizeof(struct in_addr);
				break;

			case IPOPT_TS_PRESPEC:
				if (ipt->ipt_ptr - 1 + sizeof(n_time) +
				    sizeof(struct in_addr) > ipt->ipt_len) {
					code = (u_char *)&ipt->ipt_ptr -
					    (u_char *)ip;
					goto bad;
				}
				memcpy(&ipaddr.sin_addr, cp0,
				    sizeof(struct in_addr));
				if (ifatoia(ifa_ifwithaddr(sintosa(&ipaddr)))
				    == NULL)
					continue;
				ipt->ipt_ptr += sizeof(struct in_addr);
				break;

			default:
				/* XXX can't take &ipt->ipt_flg */
				code = (u_char *)&ipt->ipt_ptr -
				    (u_char *)ip + 1;
				goto bad;
			}
			ntime = iptime();
			cp0 = (u_char *) &ntime; /* XXX grumble, GCC... */
			memmove((char *)cp + ipt->ipt_ptr - 1, cp0,
			    sizeof(n_time));
			ipt->ipt_ptr += sizeof(n_time);
		}
	}
	if (forward) {
		if (ip_forwsrcrt == 0) {
			type = ICMP_UNREACH;
			code = ICMP_UNREACH_SRCFAIL;
			goto bad;
		}
		ip_forward(m, 1);
		return true;
	}
	return false;
bad:
	icmp_error(m, type, code, 0, 0);
	IP_STATINC(IP_STAT_BADOPTIONS);
	return true;
}

/*
 * ip_rtaddr: given address of next destination (final or next hop),
 * return internet address info of interface to be used to get there.
 */
static struct in_ifaddr *
ip_rtaddr(struct in_addr dst)
{
	struct rtentry *rt;
	union {
		struct sockaddr		dst;
		struct sockaddr_in	dst4;
	} u;

	sockaddr_in_init(&u.dst4, &dst, 0);

	SOFTNET_LOCK();
	rt = rtcache_lookup(&ipforward_rt, &u.dst);
	SOFTNET_UNLOCK();
	if (rt == NULL)
		return NULL;

	return ifatoia(rt->rt_ifa);
}

/*
 * save_rte: save incoming source route for use in replies, to be picked
 * up later by ip_srcroute if the receiver is interested.
 */
static void
save_rte(u_char *option, struct in_addr dst)
{
	unsigned olen;

	olen = option[IPOPT_OLEN];
	if (olen > sizeof(ip_srcrt) - (1 + sizeof(dst)))
		return;
	memcpy((void *)ip_srcrt.srcopt, (void *)option, olen);
	ip_nhops = (olen - IPOPT_OFFSET - 1) / sizeof(struct in_addr);
	ip_srcrt.dst = dst;
}

/*
 * Retrieve incoming source route for use in replies,
 * in the same form used by setsockopt.
 * The first hop is placed before the options, will be removed later.
 */
struct mbuf *
ip_srcroute(void)
{
	struct in_addr *p, *q;
	struct mbuf *m;

	if (ip_nhops == 0)
		return NULL;
	m = m_get(M_DONTWAIT, MT_SOOPTS);
	if (m == 0)
		return NULL;

	MCLAIM(m, &inetdomain.dom_mowner);
#define OPTSIZ	(sizeof(ip_srcrt.nop) + sizeof(ip_srcrt.srcopt))

	/* length is (nhops+1)*sizeof(addr) + sizeof(nop + srcrt header) */
	m->m_len = ip_nhops * sizeof(struct in_addr) + sizeof(struct in_addr) +
	    OPTSIZ;

	/*
	 * First save first hop for return route
	 */
	p = &ip_srcrt.route[ip_nhops - 1];
	*(mtod(m, struct in_addr *)) = *p--;

	/*
	 * Copy option fields and padding (nop) to mbuf.
	 */
	ip_srcrt.nop = IPOPT_NOP;
	ip_srcrt.srcopt[IPOPT_OFFSET] = IPOPT_MINOFF;
	memmove(mtod(m, char *) + sizeof(struct in_addr), &ip_srcrt.nop,
	    OPTSIZ);
	q = (struct in_addr *)(mtod(m, char *) +
	    sizeof(struct in_addr) + OPTSIZ);
#undef OPTSIZ
	/*
	 * Record return path as an IP source route,
	 * reversing the path (pointers are now aligned).
	 */
	while (p >= ip_srcrt.route) {
		*q++ = *p--;
	}
	/*
	 * Last hop goes to final destination.
	 */
	*q = ip_srcrt.dst;
	return (m);
}

const int inetctlerrmap[PRC_NCMDS] = {
	[PRC_MSGSIZE] = EMSGSIZE,
	[PRC_HOSTDEAD] = EHOSTDOWN,
	[PRC_HOSTUNREACH] = EHOSTUNREACH,
	[PRC_UNREACH_NET] = EHOSTUNREACH,
	[PRC_UNREACH_HOST] = EHOSTUNREACH,
	[PRC_UNREACH_PROTOCOL] = ECONNREFUSED,
	[PRC_UNREACH_PORT] = ECONNREFUSED,
	[PRC_UNREACH_SRCFAIL] = EHOSTUNREACH,
	[PRC_PARAMPROB] = ENOPROTOOPT,
};

void
ip_fasttimo(void)
{
	if (ip_drainwanted) {
		ip_drain();
		ip_drainwanted = 0;
	}
}

void
ip_drainstub(void)
{
	ip_drainwanted = 1;
}

/*
 * Forward a packet.  If some error occurs return the sender
 * an icmp packet.  Note we can't always generate a meaningful
 * icmp message because icmp doesn't have a large enough repertoire
 * of codes and types.
 *
 * If not forwarding, just drop the packet.  This could be confusing
 * if ipforwarding was zero but some routing protocol was advancing
 * us as a gateway to somewhere.  However, we must let the routing
 * protocol deal with that.
 *
 * The srcrt parameter indicates whether the packet is being forwarded
 * via a source route.
 */
static void
ip_forward(struct mbuf *m, int srcrt)
{
	struct ip *ip = mtod(m, struct ip *);
	struct rtentry *rt;
	int error, type = 0, code = 0, destmtu = 0;
	struct mbuf *mcopy;
	n_long dest;
	union {
		struct sockaddr		dst;
		struct sockaddr_in	dst4;
	} u;
	uint64_t *ips;

	KASSERTMSG(cpu_softintr_p(), "ip_forward: not in the software "
	    "interrupt handler; synchronization assumptions violated");

	/*
	 * We are now in the output path.
	 */
	MCLAIM(m, &ip_tx_mowner);

	/*
	 * Clear any in-bound checksum flags for this packet.
	 */
	m->m_pkthdr.csum_flags = 0;

	dest = 0;
	if (m->m_flags & (M_BCAST|M_MCAST) || in_canforward(ip->ip_dst) == 0) {
		IP_STATINC(IP_STAT_CANTFORWARD);
		m_freem(m);
		return;
	}

	SOFTNET_LOCK();

	if (ip->ip_ttl <= IPTTLDEC) {
		icmp_error(m, ICMP_TIMXCEED, ICMP_TIMXCEED_INTRANS, dest, 0);
		SOFTNET_UNLOCK();
		return;
	}

	sockaddr_in_init(&u.dst4, &ip->ip_dst, 0);

	if ((rt = rtcache_lookup(&ipforward_rt, &u.dst)) == NULL) {
		icmp_error(m, ICMP_UNREACH, ICMP_UNREACH_NET, dest, 0);
		SOFTNET_UNLOCK();
		return;
	}

	/*
	 * Save at most 68 bytes of the packet in case
	 * we need to generate an ICMP message to the src.
	 * Pullup to avoid sharing mbuf cluster between m and mcopy.
	 */
	mcopy = m_copym(m, 0, imin(ntohs(ip->ip_len), 68), M_DONTWAIT);
	if (mcopy)
		mcopy = m_pullup(mcopy, ip->ip_hl << 2);

	ip->ip_ttl -= IPTTLDEC;

	/*
	 * If forwarding packet using same interface that it came in on,
	 * perhaps should send a redirect to sender to shortcut a hop.
	 * Only send redirect if source is sending directly to us,
	 * and if packet was not source routed (or has any options).
	 * Also, don't send redirect if forwarding using a default route
	 * or a route modified by a redirect.
	 */
	if (rt->rt_ifp == m->m_pkthdr.rcvif &&
	    (rt->rt_flags & (RTF_DYNAMIC|RTF_MODIFIED)) == 0 &&
	    !in_nullhost(satocsin(rt_getkey(rt))->sin_addr) &&
	    ipsendredirects && !srcrt) {
		if (rt->rt_ifa &&
		    (ip->ip_src.s_addr & ifatoia(rt->rt_ifa)->ia_subnetmask) ==
		    ifatoia(rt->rt_ifa)->ia_subnet) {
			if (rt->rt_flags & RTF_GATEWAY)
				dest = satosin(rt->rt_gateway)->sin_addr.s_addr;
			else
				dest = ip->ip_dst.s_addr;
			/*
			 * Router requirements says to only send host
			 * redirects.
			 */
			type = ICMP_REDIRECT;
			code = ICMP_REDIRECT_HOST;
		}
	}

	error = ip_output(m, NULL, &ipforward_rt,
	    (IP_FORWARDING | (ip_directedbcast ? IP_ALLOWBROADCAST : 0)),
	    NULL, NULL);

	if (error) {
		IP_STATINC(IP_STAT_CANTFORWARD);
		goto error;
	}

	ips = IP_STAT_GETREF();
	ips[IP_STAT_FORWARD]++;

	if (type) {
		ips[IP_STAT_REDIRECTSENT]++;
		IP_STAT_PUTREF();
		goto redirect;
	}

	IP_STAT_PUTREF();
	if (mcopy) {
#ifdef GATEWAY
		if (mcopy->m_flags & M_CANFASTFWD)
			ipflow_create(&ipforward_rt, mcopy);
#endif
		m_freem(mcopy);
	}

	SOFTNET_UNLOCK();
	return;

redirect:
error:
	if (mcopy == NULL) {
		SOFTNET_UNLOCK();
		return;
	}

	switch (error) {

	case 0:				/* forwarded, but need redirect */
		/* type, code set above */
		break;

	case ENETUNREACH:		/* shouldn't happen, checked above */
	case EHOSTUNREACH:
	case ENETDOWN:
	case EHOSTDOWN:
	default:
		type = ICMP_UNREACH;
		code = ICMP_UNREACH_HOST;
		break;

	case EMSGSIZE:
		type = ICMP_UNREACH;
		code = ICMP_UNREACH_NEEDFRAG;

		if ((rt = rtcache_validate(&ipforward_rt)) != NULL)
			destmtu = rt->rt_ifp->if_mtu;
#ifdef IPSEC
		if (ipsec_used)
			(void)ipsec4_forward(mcopy, &destmtu);
#endif
		IP_STATINC(IP_STAT_CANTFRAG);
		break;

	case ENOBUFS:
		/*
		 * Do not generate ICMP_SOURCEQUENCH as required in RFC 1812,
		 * Requirements for IP Version 4 Routers.  Source quench can
		 * big problem under DoS attacks or if the underlying
		 * interface is rate-limited.
		 */
		if (mcopy)
			m_freem(mcopy);
		SOFTNET_UNLOCK();
		return;
	}
	icmp_error(mcopy, type, code, dest, destmtu);
	SOFTNET_UNLOCK();
}

void
ip_savecontrol(struct inpcb *inp, struct mbuf **mp, struct ip *ip,
    struct mbuf *m)
{
	struct socket *so = inp->inp_socket;
	ifnet_t *ifp = m->m_pkthdr.rcvif;
	int inpflags = inp->inp_flags;

	if (so->so_options & SO_TIMESTAMP
#ifdef SO_OTIMESTAMP
	    || so->so_options & SO_OTIMESTAMP
#endif
	    ) {
		struct timeval tv;

		microtime(&tv);
#ifdef SO_OTIMESTAMP
		if (so->so_options & SO_OTIMESTAMP) {
			struct timeval50 tv50;
			timeval_to_timeval50(&tv, &tv50);
			*mp = sbcreatecontrol((void *) &tv50, sizeof(tv50),
			    SCM_OTIMESTAMP, SOL_SOCKET);
		} else
#endif
		*mp = sbcreatecontrol((void *) &tv, sizeof(tv),
		    SCM_TIMESTAMP, SOL_SOCKET);
		if (*mp)
			mp = &(*mp)->m_next;
	}
	if (inpflags & INP_RECVDSTADDR) {
		*mp = sbcreatecontrol((void *) &ip->ip_dst,
		    sizeof(struct in_addr), IP_RECVDSTADDR, IPPROTO_IP);
		if (*mp)
			mp = &(*mp)->m_next;
	}
	if (inpflags & INP_RECVPKTINFO) {
		struct in_pktinfo ipi;
		ipi.ipi_addr = ip->ip_src;
		ipi.ipi_ifindex = ifp->if_index;
		*mp = sbcreatecontrol((void *) &ipi,
		    sizeof(ipi), IP_RECVPKTINFO, IPPROTO_IP);
		if (*mp)
			mp = &(*mp)->m_next;
	}
	if (inpflags & INP_PKTINFO) {
		struct in_pktinfo ipi;
		ipi.ipi_addr = ip->ip_dst;
		ipi.ipi_ifindex = ifp->if_index;
		*mp = sbcreatecontrol((void *) &ipi,
		    sizeof(ipi), IP_PKTINFO, IPPROTO_IP);
		if (*mp)
			mp = &(*mp)->m_next;
	}
	if (inpflags & INP_RECVIF) {
		struct sockaddr_dl sdl;

		sockaddr_dl_init(&sdl, sizeof(sdl), ifp ?
		    ifp->if_index : 0, 0, NULL, 0, NULL, 0);
		*mp = sbcreatecontrol(&sdl, sdl.sdl_len, IP_RECVIF, IPPROTO_IP);
		if (*mp)
			mp = &(*mp)->m_next;
	}
	if (inpflags & INP_RECVTTL) {
		*mp = sbcreatecontrol((void *) &ip->ip_ttl,
		    sizeof(uint8_t), IP_RECVTTL, IPPROTO_IP);
		if (*mp)
			mp = &(*mp)->m_next;
	}
}

/*
 * sysctl helper routine for net.inet.ip.forwsrcrt.
 */
static int
sysctl_net_inet_ip_forwsrcrt(SYSCTLFN_ARGS)
{
	int error, tmp;
	struct sysctlnode node;

	node = *rnode;
	tmp = ip_forwsrcrt;
	node.sysctl_data = &tmp;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return (error);

	error = kauth_authorize_network(l->l_cred, KAUTH_NETWORK_FORWSRCRT,
	    0, NULL, NULL, NULL);
	if (error)
		return (error);

	ip_forwsrcrt = tmp;

	return (0);
}

/*
 * sysctl helper routine for net.inet.ip.mtudisctimeout.  checks the
 * range of the new value and tweaks timers if it changes.
 */
static int
sysctl_net_inet_ip_pmtudto(SYSCTLFN_ARGS)
{
	int error, tmp;
	struct sysctlnode node;

	node = *rnode;
	tmp = ip_mtudisc_timeout;
	node.sysctl_data = &tmp;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return (error);
	if (tmp < 0)
		return (EINVAL);

	mutex_enter(softnet_lock);

	ip_mtudisc_timeout = tmp;
	rt_timer_queue_change(ip_mtudisc_timeout_q, ip_mtudisc_timeout);

	mutex_exit(softnet_lock);

	return (0);
}

static int
sysctl_net_inet_ip_stats(SYSCTLFN_ARGS)
{

	return (NETSTAT_SYSCTL(ipstat_percpu, IP_NSTATS));
}

static void
sysctl_net_inet_ip_setup(struct sysctllog **clog)
{
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "inet",
		       SYSCTL_DESCR("PF_INET related settings"),
		       NULL, 0, NULL, 0,
		       CTL_NET, PF_INET, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "ip",
		       SYSCTL_DESCR("IPv4 related settings"),
		       NULL, 0, NULL, 0,
		       CTL_NET, PF_INET, IPPROTO_IP, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "forwarding",
		       SYSCTL_DESCR("Enable forwarding of INET datagrams"),
		       NULL, 0, &ipforwarding, 0,
		       CTL_NET, PF_INET, IPPROTO_IP,
		       IPCTL_FORWARDING, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "redirect",
		       SYSCTL_DESCR("Enable sending of ICMP redirect messages"),
		       NULL, 0, &ipsendredirects, 0,
		       CTL_NET, PF_INET, IPPROTO_IP,
		       IPCTL_SENDREDIRECTS, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "ttl",
		       SYSCTL_DESCR("Default TTL for an INET datagram"),
		       NULL, 0, &ip_defttl, 0,
		       CTL_NET, PF_INET, IPPROTO_IP,
		       IPCTL_DEFTTL, CTL_EOL);
#ifdef IPCTL_DEFMTU
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT /* |CTLFLAG_READWRITE? */,
		       CTLTYPE_INT, "mtu",
		       SYSCTL_DESCR("Default MTA for an INET route"),
		       NULL, 0, &ip_mtu, 0,
		       CTL_NET, PF_INET, IPPROTO_IP,
		       IPCTL_DEFMTU, CTL_EOL);
#endif /* IPCTL_DEFMTU */
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "forwsrcrt",
		       SYSCTL_DESCR("Enable forwarding of source-routed "
				    "datagrams"),
		       sysctl_net_inet_ip_forwsrcrt, 0, &ip_forwsrcrt, 0,
		       CTL_NET, PF_INET, IPPROTO_IP,
		       IPCTL_FORWSRCRT, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "directed-broadcast",
		       SYSCTL_DESCR("Enable forwarding of broadcast datagrams"),
		       NULL, 0, &ip_directedbcast, 0,
		       CTL_NET, PF_INET, IPPROTO_IP,
		       IPCTL_DIRECTEDBCAST, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "allowsrcrt",
		       SYSCTL_DESCR("Accept source-routed datagrams"),
		       NULL, 0, &ip_allowsrcrt, 0,
		       CTL_NET, PF_INET, IPPROTO_IP,
		       IPCTL_ALLOWSRCRT, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "mtudisc",
		       SYSCTL_DESCR("Use RFC1191 Path MTU Discovery"),
		       NULL, 0, &ip_mtudisc, 0,
		       CTL_NET, PF_INET, IPPROTO_IP,
		       IPCTL_MTUDISC, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "anonportmin",
		       SYSCTL_DESCR("Lowest ephemeral port number to assign"),
		       sysctl_net_inet_ip_ports, 0, &anonportmin, 0,
		       CTL_NET, PF_INET, IPPROTO_IP,
		       IPCTL_ANONPORTMIN, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "anonportmax",
		       SYSCTL_DESCR("Highest ephemeral port number to assign"),
		       sysctl_net_inet_ip_ports, 0, &anonportmax, 0,
		       CTL_NET, PF_INET, IPPROTO_IP,
		       IPCTL_ANONPORTMAX, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "mtudisctimeout",
		       SYSCTL_DESCR("Lifetime of a Path MTU Discovered route"),
		       sysctl_net_inet_ip_pmtudto, 0, (void *)&ip_mtudisc_timeout, 0,
		       CTL_NET, PF_INET, IPPROTO_IP,
		       IPCTL_MTUDISCTIMEOUT, CTL_EOL);
#if NGIF > 0
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "gifttl",
		       SYSCTL_DESCR("Default TTL for a gif tunnel datagram"),
		       NULL, 0, &ip_gif_ttl, 0,
		       CTL_NET, PF_INET, IPPROTO_IP,
		       IPCTL_GIF_TTL, CTL_EOL);
#endif /* NGIF */
#ifndef IPNOPRIVPORTS
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "lowportmin",
		       SYSCTL_DESCR("Lowest privileged ephemeral port number "
				    "to assign"),
		       sysctl_net_inet_ip_ports, 0, &lowportmin, 0,
		       CTL_NET, PF_INET, IPPROTO_IP,
		       IPCTL_LOWPORTMIN, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "lowportmax",
		       SYSCTL_DESCR("Highest privileged ephemeral port number "
				    "to assign"),
		       sysctl_net_inet_ip_ports, 0, &lowportmax, 0,
		       CTL_NET, PF_INET, IPPROTO_IP,
		       IPCTL_LOWPORTMAX, CTL_EOL);
#endif /* IPNOPRIVPORTS */
#if NGRE > 0
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "grettl",
		       SYSCTL_DESCR("Default TTL for a gre tunnel datagram"),
		       NULL, 0, &ip_gre_ttl, 0,
		       CTL_NET, PF_INET, IPPROTO_IP,
		       IPCTL_GRE_TTL, CTL_EOL);
#endif /* NGRE */
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "checkinterface",
		       SYSCTL_DESCR("Enable receive side of Strong ES model "
				    "from RFC1122"),
		       NULL, 0, &ip_checkinterface, 0,
		       CTL_NET, PF_INET, IPPROTO_IP,
		       IPCTL_CHECKINTERFACE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "random_id",
		       SYSCTL_DESCR("Assign random ip_id values"),
		       NULL, 0, &ip_do_randomid, 0,
		       CTL_NET, PF_INET, IPPROTO_IP,
		       IPCTL_RANDOMID, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "do_loopback_cksum",
		       SYSCTL_DESCR("Perform IP checksum on loopback"),
		       NULL, 0, &ip_do_loopback_cksum, 0,
		       CTL_NET, PF_INET, IPPROTO_IP,
		       IPCTL_LOOPBACKCKSUM, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRUCT, "stats",
		       SYSCTL_DESCR("IP statistics"),
		       sysctl_net_inet_ip_stats, 0, NULL, 0,
		       CTL_NET, PF_INET, IPPROTO_IP, IPCTL_STATS,
		       CTL_EOL);
#if NARP
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "dad_count",
		       SYSCTL_DESCR("Number of Duplicate Address Detection "
				    "probes to send"),
		       NULL, 0, &ip_dad_count, 0,
		       CTL_NET, PF_INET, IPPROTO_IP,
		       IPCTL_DAD_COUNT, CTL_EOL);
#endif

	/* anonportalgo RFC6056 subtree */
	const struct sysctlnode *portalgo_node;
	sysctl_createv(clog, 0, NULL, &portalgo_node,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "anonportalgo",
		       SYSCTL_DESCR("Anonymous Port Algorithm Selection (RFC 6056)"),
	    	       NULL, 0, NULL, 0,
		       CTL_NET, PF_INET, IPPROTO_IP, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &portalgo_node, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRING, "available",
		       SYSCTL_DESCR("available algorithms"),
		       sysctl_portalgo_available, 0, NULL, PORTALGO_MAXLEN,
		       CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &portalgo_node, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_STRING, "selected",
		       SYSCTL_DESCR("selected algorithm"),
		       sysctl_portalgo_selected4, 0, NULL, PORTALGO_MAXLEN,
		       CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &portalgo_node, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_STRUCT, "reserve",
		       SYSCTL_DESCR("bitmap of reserved ports"),
		       sysctl_portalgo_reserve4, 0, NULL, 0,
		       CTL_CREATE, CTL_EOL);
}

void
ip_statinc(u_int stat)
{

	KASSERT(stat < IP_NSTATS);
	IP_STATINC(stat);
}
