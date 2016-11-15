/*	$NetBSD: ip6_input.c,v 1.152 2015/08/24 22:21:27 pooka Exp $	*/
/*	$KAME: ip6_input.c,v 1.188 2001/03/29 05:34:31 itojun Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: ip6_input.c,v 1.152 2015/08/24 22:21:27 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_gateway.h"
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"
#include "opt_compat_netbsd.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/cprng.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/pktqueue.h>
#include <net/pfil.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#ifdef INET
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>
#endif /* INET */
#include <netinet/ip6.h>
#include <netinet/portalgo.h>
#include <netinet6/in6_var.h>
#include <netinet6/ip6_var.h>
#include <netinet6/ip6_private.h>
#include <netinet6/in6_pcb.h>
#include <netinet/icmp6.h>
#include <netinet6/scope6_var.h>
#include <netinet6/in6_ifattach.h>
#include <netinet6/nd6.h>

#ifdef IPSEC
#include <netipsec/ipsec.h>
#include <netipsec/ipsec6.h>
#include <netipsec/key.h>
#endif /* IPSEC */

#ifdef COMPAT_50
#include <compat/sys/time.h>
#include <compat/sys/socket.h>
#endif

#include <netinet6/ip6protosw.h>

#include "faith.h"
#include "gif.h"

#if NGIF > 0
#include <netinet6/in6_gif.h>
#endif

#include <net/net_osdep.h>

extern struct domain inet6domain;

u_char ip6_protox[IPPROTO_MAX];
struct in6_ifaddr *in6_ifaddr;
pktqueue_t *ip6_pktq __read_mostly;

extern callout_t in6_tmpaddrtimer_ch;

int ip6_forward_srcrt;			/* XXX */
int ip6_sourcecheck;			/* XXX */
int ip6_sourcecheck_interval;		/* XXX */

pfil_head_t *inet6_pfil_hook;

percpu_t *ip6stat_percpu;

static void ip6_init2(void *);
static void ip6intr(void *);
static struct m_tag *ip6_setdstifaddr(struct mbuf *, const struct in6_ifaddr *);

static int ip6_process_hopopts(struct mbuf *, u_int8_t *, int, u_int32_t *,
	u_int32_t *);
static struct mbuf *ip6_pullexthdr(struct mbuf *, size_t, int);
static void sysctl_net_inet6_ip6_setup(struct sysctllog **);

/*
 * IP6 initialization: fill in IP6 protocol switch table.
 * All protocols not implemented in kernel go to raw IP6 protocol handler.
 */
void
ip6_init(void)
{
	const struct ip6protosw *pr;
	int i;

	sysctl_net_inet6_ip6_setup(NULL);
	pr = (const struct ip6protosw *)pffindproto(PF_INET6, IPPROTO_RAW, SOCK_RAW);
	if (pr == 0)
		panic("ip6_init");
	for (i = 0; i < IPPROTO_MAX; i++)
		ip6_protox[i] = pr - inet6sw;
	for (pr = (const struct ip6protosw *)inet6domain.dom_protosw;
	    pr < (const struct ip6protosw *)inet6domain.dom_protoswNPROTOSW; pr++)
		if (pr->pr_domain->dom_family == PF_INET6 &&
		    pr->pr_protocol && pr->pr_protocol != IPPROTO_RAW)
			ip6_protox[pr->pr_protocol] = pr - inet6sw;

	ip6_pktq = pktq_create(IFQ_MAXLEN, ip6intr, NULL);
	KASSERT(ip6_pktq != NULL);

	scope6_init();
	addrsel_policy_init();
	nd6_init();
	frag6_init();
	ip6_desync_factor = cprng_fast32() % MAX_TEMP_DESYNC_FACTOR;

	ip6_init2(NULL);
#ifdef GATEWAY
	ip6flow_init(ip6_hashsize);
#endif
	/* Register our Packet Filter hook. */
	inet6_pfil_hook = pfil_head_create(PFIL_TYPE_AF, (void *)AF_INET6);
	KASSERT(inet6_pfil_hook != NULL);

	ip6stat_percpu = percpu_alloc(sizeof(uint64_t) * IP6_NSTATS);
}

static void
ip6_init2(void *dummy)
{

	/* nd6_timer_init */
	callout_init(&nd6_timer_ch, CALLOUT_MPSAFE);
	callout_reset(&nd6_timer_ch, hz, nd6_timer, NULL);

	/* timer for regeneranation of temporary addresses randomize ID */
	callout_init(&in6_tmpaddrtimer_ch, CALLOUT_MPSAFE);
	callout_reset(&in6_tmpaddrtimer_ch,
		      (ip6_temp_preferred_lifetime - ip6_desync_factor -
		       ip6_temp_regen_advance) * hz,
		      in6_tmpaddrtimer, NULL);
}

/*
 * IP6 input interrupt handling. Just pass the packet to ip6_input.
 */
static void
ip6intr(void *arg __unused)
{
	struct mbuf *m;

	mutex_enter(softnet_lock);
	while ((m = pktq_dequeue(ip6_pktq)) != NULL) {
		const ifnet_t *ifp = m->m_pkthdr.rcvif;

		/*
		 * Drop the packet if IPv6 is disabled on the interface.
		 */
		if ((ND_IFINFO(ifp)->flags & ND6_IFF_IFDISABLED)) {
			m_freem(m);
			continue;
		}
		ip6_input(m);
	}
	mutex_exit(softnet_lock);
}

extern struct	route ip6_forward_rt;

void
ip6_input(struct mbuf *m)
{
	struct ip6_hdr *ip6;
	int hit, off = sizeof(struct ip6_hdr), nest;
	u_int32_t plen;
	u_int32_t rtalert = ~0;
	int nxt, ours = 0, rh_present = 0;
	struct ifnet *deliverifp = NULL;
	int srcrt = 0;
	const struct rtentry *rt;
	union {
		struct sockaddr		dst;
		struct sockaddr_in6	dst6;
	} u;

	/*
	 * make sure we don't have onion peering information into m_tag.
	 */
	ip6_delaux(m);

	/*
	 * mbuf statistics
	 */
	if (m->m_flags & M_EXT) {
		if (m->m_next)
			IP6_STATINC(IP6_STAT_MEXT2M);
		else
			IP6_STATINC(IP6_STAT_MEXT1);
	} else {
#define M2MMAX	32
		if (m->m_next) {
			if (m->m_flags & M_LOOP) {
			/*XXX*/	IP6_STATINC(IP6_STAT_M2M + lo0ifp->if_index);
			} else if (m->m_pkthdr.rcvif->if_index < M2MMAX) {
				IP6_STATINC(IP6_STAT_M2M +
					    m->m_pkthdr.rcvif->if_index);
			} else
				IP6_STATINC(IP6_STAT_M2M);
		} else
			IP6_STATINC(IP6_STAT_M1);
#undef M2MMAX
	}

	in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_receive);
	IP6_STATINC(IP6_STAT_TOTAL);

	/*
	 * If the IPv6 header is not aligned, slurp it up into a new
	 * mbuf with space for link headers, in the event we forward
	 * it.  Otherwise, if it is aligned, make sure the entire base
	 * IPv6 header is in the first mbuf of the chain.
	 */
	if (IP6_HDR_ALIGNED_P(mtod(m, void *)) == 0) {
		struct ifnet *inifp = m->m_pkthdr.rcvif;
		if ((m = m_copyup(m, sizeof(struct ip6_hdr),
				  (max_linkhdr + 3) & ~3)) == NULL) {
			/* XXXJRT new stat, please */
			IP6_STATINC(IP6_STAT_TOOSMALL);
			in6_ifstat_inc(inifp, ifs6_in_hdrerr);
			return;
		}
	} else if (__predict_false(m->m_len < sizeof(struct ip6_hdr))) {
		struct ifnet *inifp = m->m_pkthdr.rcvif;
		if ((m = m_pullup(m, sizeof(struct ip6_hdr))) == NULL) {
			IP6_STATINC(IP6_STAT_TOOSMALL);
			in6_ifstat_inc(inifp, ifs6_in_hdrerr);
			return;
		}
	}

	ip6 = mtod(m, struct ip6_hdr *);

	if ((ip6->ip6_vfc & IPV6_VERSION_MASK) != IPV6_VERSION) {
		IP6_STATINC(IP6_STAT_BADVERS);
		in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_hdrerr);
		goto bad;
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
	/*
	 * let ipfilter look at packet on the wire,
	 * not the decapsulated packet.
	 */
#if defined(IPSEC)
	if (!ipsec_used || !ipsec_indone(m))
#else
	if (1)
#endif
	{
		struct in6_addr odst;

		odst = ip6->ip6_dst;
		if (pfil_run_hooks(inet6_pfil_hook, &m, m->m_pkthdr.rcvif,
				   PFIL_IN) != 0)
			return;
		if (m == NULL)
			return;
		ip6 = mtod(m, struct ip6_hdr *);
		srcrt = !IN6_ARE_ADDR_EQUAL(&odst, &ip6->ip6_dst);
	}

	IP6_STATINC(IP6_STAT_NXTHIST + ip6->ip6_nxt);

#ifdef ALTQ
	if (altq_input != NULL && (*altq_input)(m, AF_INET6) == 0) {
		/* packet is dropped by traffic conditioner */
		return;
	}
#endif

	/*
	 * Check against address spoofing/corruption.
	 */
	if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_src) ||
	    IN6_IS_ADDR_UNSPECIFIED(&ip6->ip6_dst)) {
		/*
		 * XXX: "badscope" is not very suitable for a multicast source.
		 */
		IP6_STATINC(IP6_STAT_BADSCOPE);
		in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_addrerr);
		goto bad;
	}
	/*
	 * The following check is not documented in specs.  A malicious
	 * party may be able to use IPv4 mapped addr to confuse tcp/udp stack
	 * and bypass security checks (act as if it was from 127.0.0.1 by using
	 * IPv6 src ::ffff:127.0.0.1).  Be cautious.
	 *
	 * This check chokes if we are in an SIIT cloud.  As none of BSDs
	 * support IPv4-less kernel compilation, we cannot support SIIT
	 * environment at all.  So, it makes more sense for us to reject any
	 * malicious packets for non-SIIT environment, than try to do a
	 * partial support for SIIT environment.
	 */
	if (IN6_IS_ADDR_V4MAPPED(&ip6->ip6_src) ||
	    IN6_IS_ADDR_V4MAPPED(&ip6->ip6_dst)) {
		IP6_STATINC(IP6_STAT_BADSCOPE);
		in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_addrerr);
		goto bad;
	}
#if 0
	/*
	 * Reject packets with IPv4 compatible addresses (auto tunnel).
	 *
	 * The code forbids auto tunnel relay case in RFC1933 (the check is
	 * stronger than RFC1933).  We may want to re-enable it if mech-xx
	 * is revised to forbid relaying case.
	 */
	if (IN6_IS_ADDR_V4COMPAT(&ip6->ip6_src) ||
	    IN6_IS_ADDR_V4COMPAT(&ip6->ip6_dst)) {
		IP6_STATINC(IP6_STAT_BADSCOPE);
		in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_addrerr);
		goto bad;
	}
#endif

	/*
	 * Disambiguate address scope zones (if there is ambiguity).
	 * We first make sure that the original source or destination address
	 * is not in our internal form for scoped addresses.  Such addresses
	 * are not necessarily invalid spec-wise, but we cannot accept them due
	 * to the usage conflict.
	 * in6_setscope() then also checks and rejects the cases where src or
	 * dst are the loopback address and the receiving interface
	 * is not loopback. 
	 */
	if (__predict_false(
	    m_makewritable(&m, 0, sizeof(struct ip6_hdr), M_DONTWAIT)))
		goto bad;
	ip6 = mtod(m, struct ip6_hdr *);
	if (in6_clearscope(&ip6->ip6_src) || in6_clearscope(&ip6->ip6_dst)) {
		IP6_STATINC(IP6_STAT_BADSCOPE);	/* XXX */
		goto bad;
	}
	if (in6_setscope(&ip6->ip6_src, m->m_pkthdr.rcvif, NULL) ||
	    in6_setscope(&ip6->ip6_dst, m->m_pkthdr.rcvif, NULL)) {
		IP6_STATINC(IP6_STAT_BADSCOPE);
		goto bad;
	}

	/*
	 * Multicast check
	 */
	if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst)) {
	  	struct	in6_multi *in6m = 0;

		in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_mcast);
		/*
		 * See if we belong to the destination multicast group on the
		 * arrival interface.
		 */
		IN6_LOOKUP_MULTI(ip6->ip6_dst, m->m_pkthdr.rcvif, in6m);
		if (in6m)
			ours = 1;
		else if (!ip6_mrouter) {
			uint64_t *ip6s = IP6_STAT_GETREF();
			ip6s[IP6_STAT_NOTMEMBER]++;
			ip6s[IP6_STAT_CANTFORWARD]++;
			IP6_STAT_PUTREF();
			in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_discard);
			goto bad;
		}
		deliverifp = m->m_pkthdr.rcvif;
		goto hbhcheck;
	}

	sockaddr_in6_init(&u.dst6, &ip6->ip6_dst, 0, 0, 0);

	/*
	 *  Unicast check
	 */
	rt = rtcache_lookup2(&ip6_forward_rt, &u.dst, 1, &hit);
	if (hit)
		IP6_STATINC(IP6_STAT_FORWARD_CACHEHIT);
	else
		IP6_STATINC(IP6_STAT_FORWARD_CACHEMISS);

#define rt6_getkey(__rt) satocsin6(rt_getkey(__rt))

	/*
	 * Accept the packet if the forwarding interface to the destination
	 * according to the routing table is the loopback interface,
	 * unless the associated route has a gateway.
	 * Note that this approach causes to accept a packet if there is a
	 * route to the loopback interface for the destination of the packet.
	 * But we think it's even useful in some situations, e.g. when using
	 * a special daemon which wants to intercept the packet.
	 */
	if (rt != NULL &&
	    (rt->rt_flags & (RTF_HOST|RTF_GATEWAY)) == RTF_HOST &&
	    !(rt->rt_flags & RTF_CLONED) &&
#if 0
	    /*
	     * The check below is redundant since the comparison of
	     * the destination and the key of the rtentry has
	     * already done through looking up the routing table.
	     */
	    IN6_ARE_ADDR_EQUAL(&ip6->ip6_dst, &rt6_getkey(rt)->sin6_addr) &&
#endif
	    rt->rt_ifp->if_type == IFT_LOOP) {
		struct in6_ifaddr *ia6 = (struct in6_ifaddr *)rt->rt_ifa;
		if (ia6->ia6_flags & IN6_IFF_ANYCAST)
			m->m_flags |= M_ANYCAST6;
		/*
		 * packets to a tentative, duplicated, or somehow invalid
		 * address must not be accepted.
		 */
		if (!(ia6->ia6_flags & IN6_IFF_NOTREADY)) {
			/* this address is ready */
			ours = 1;
			deliverifp = ia6->ia_ifp;	/* correct? */
			goto hbhcheck;
		} else {
			/* address is not ready, so discard the packet. */
			nd6log((LOG_INFO,
			    "ip6_input: packet to an unready address %s->%s\n",
			    ip6_sprintf(&ip6->ip6_src),
			    ip6_sprintf(&ip6->ip6_dst)));

			goto bad;
		}
	}

	/*
	 * FAITH (Firewall Aided Internet Translator)
	 */
#if defined(NFAITH) && 0 < NFAITH
	if (ip6_keepfaith) {
		if (rt != NULL && rt->rt_ifp != NULL &&
		    rt->rt_ifp->if_type == IFT_FAITH) {
			/* XXX do we need more sanity checks? */
			ours = 1;
			deliverifp = rt->rt_ifp; /* faith */
			goto hbhcheck;
		}
	}
#endif

#if 0
    {
	/*
	 * Last resort: check in6_ifaddr for incoming interface.
	 * The code is here until I update the "goto ours hack" code above
	 * working right.
	 */
	struct ifaddr *ifa;
	IFADDR_FOREACH(ifa, m->m_pkthdr.rcvif) {
		if (ifa->ifa_addr == NULL)
			continue;	/* just for safety */
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		if (IN6_ARE_ADDR_EQUAL(IFA_IN6(ifa), &ip6->ip6_dst)) {
			ours = 1;
			deliverifp = ifa->ifa_ifp;
			goto hbhcheck;
		}
	}
    }
#endif

	/*
	 * Now there is no reason to process the packet if it's not our own
	 * and we're not a router.
	 */
	if (!ip6_forwarding) {
		IP6_STATINC(IP6_STAT_CANTFORWARD);
		in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_discard);
		goto bad;
	}

  hbhcheck:
	/*
	 * record address information into m_tag, if we don't have one yet.
	 * note that we are unable to record it, if the address is not listed
	 * as our interface address (e.g. multicast addresses, addresses
	 * within FAITH prefixes and such).
	 */
	if (deliverifp && ip6_getdstifaddr(m) == NULL) {
		struct in6_ifaddr *ia6;

		ia6 = in6_ifawithifp(deliverifp, &ip6->ip6_dst);
		if (ia6 != NULL && ip6_setdstifaddr(m, ia6) == NULL) {
			/*
			 * XXX maybe we should drop the packet here,
			 * as we could not provide enough information
			 * to the upper layers.
			 */
		}
	}

	/*
	 * Process Hop-by-Hop options header if it's contained.
	 * m may be modified in ip6_hopopts_input().
	 * If a JumboPayload option is included, plen will also be modified.
	 */
	plen = (u_int32_t)ntohs(ip6->ip6_plen);
	if (ip6->ip6_nxt == IPPROTO_HOPOPTS) {
		struct ip6_hbh *hbh;

		if (ip6_hopopts_input(&plen, &rtalert, &m, &off)) {
#if 0	/*touches NULL pointer*/
			in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_discard);
#endif
			return;	/* m have already been freed */
		}

		/* adjust pointer */
		ip6 = mtod(m, struct ip6_hdr *);

		/*
		 * if the payload length field is 0 and the next header field
		 * indicates Hop-by-Hop Options header, then a Jumbo Payload
		 * option MUST be included.
		 */
		if (ip6->ip6_plen == 0 && plen == 0) {
			/*
			 * Note that if a valid jumbo payload option is
			 * contained, ip6_hopopts_input() must set a valid
			 * (non-zero) payload length to the variable plen.
			 */
			IP6_STATINC(IP6_STAT_BADOPTIONS);
			in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_discard);
			in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_hdrerr);
			icmp6_error(m, ICMP6_PARAM_PROB,
				    ICMP6_PARAMPROB_HEADER,
				    (char *)&ip6->ip6_plen - (char *)ip6);
			return;
		}
		IP6_EXTHDR_GET(hbh, struct ip6_hbh *, m, sizeof(struct ip6_hdr),
			sizeof(struct ip6_hbh));
		if (hbh == NULL) {
			IP6_STATINC(IP6_STAT_TOOSHORT);
			return;
		}
		KASSERT(IP6_HDR_ALIGNED_P(hbh));
		nxt = hbh->ip6h_nxt;

		/*
		 * accept the packet if a router alert option is included
		 * and we act as an IPv6 router.
		 */
		if (rtalert != ~0 && ip6_forwarding)
			ours = 1;
	} else
		nxt = ip6->ip6_nxt;

	/*
	 * Check that the amount of data in the buffers
	 * is as at least much as the IPv6 header would have us expect.
	 * Trim mbufs if longer than we expect.
	 * Drop packet if shorter than we expect.
	 */
	if (m->m_pkthdr.len - sizeof(struct ip6_hdr) < plen) {
		IP6_STATINC(IP6_STAT_TOOSHORT);
		in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_truncated);
		goto bad;
	}
	if (m->m_pkthdr.len > sizeof(struct ip6_hdr) + plen) {
		if (m->m_len == m->m_pkthdr.len) {
			m->m_len = sizeof(struct ip6_hdr) + plen;
			m->m_pkthdr.len = sizeof(struct ip6_hdr) + plen;
		} else
			m_adj(m, sizeof(struct ip6_hdr) + plen - m->m_pkthdr.len);
	}

	/*
	 * Forward if desirable.
	 */
	if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst)) {
		/*
		 * If we are acting as a multicast router, all
		 * incoming multicast packets are passed to the
		 * kernel-level multicast forwarding function.
		 * The packet is returned (relatively) intact; if
		 * ip6_mforward() returns a non-zero value, the packet
		 * must be discarded, else it may be accepted below.
		 */
		if (ip6_mrouter && ip6_mforward(ip6, m->m_pkthdr.rcvif, m)) {
			IP6_STATINC(IP6_STAT_CANTFORWARD);
			m_freem(m);
			return;
		}
		if (!ours) {
			m_freem(m);
			return;
		}
	} else if (!ours) {
		ip6_forward(m, srcrt);
		return;
	}

	ip6 = mtod(m, struct ip6_hdr *);

	/*
	 * Malicious party may be able to use IPv4 mapped addr to confuse
	 * tcp/udp stack and bypass security checks (act as if it was from
	 * 127.0.0.1 by using IPv6 src ::ffff:127.0.0.1).  Be cautious.
	 *
	 * For SIIT end node behavior, you may want to disable the check.
	 * However, you will  become vulnerable to attacks using IPv4 mapped
	 * source.
	 */
	if (IN6_IS_ADDR_V4MAPPED(&ip6->ip6_src) ||
	    IN6_IS_ADDR_V4MAPPED(&ip6->ip6_dst)) {
		IP6_STATINC(IP6_STAT_BADSCOPE);
		in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_addrerr);
		goto bad;
	}

	/*
	 * Tell launch routine the next header
	 */
#ifdef IFA_STATS
	if (deliverifp != NULL) {
		struct in6_ifaddr *ia6;
		ia6 = in6_ifawithifp(deliverifp, &ip6->ip6_dst);
		if (ia6)
			ia6->ia_ifa.ifa_data.ifad_inbytes += m->m_pkthdr.len;
	}
#endif
	IP6_STATINC(IP6_STAT_DELIVERED);
	in6_ifstat_inc(deliverifp, ifs6_in_deliver);
	nest = 0;

	rh_present = 0;
	while (nxt != IPPROTO_DONE) {
		if (ip6_hdrnestlimit && (++nest > ip6_hdrnestlimit)) {
			IP6_STATINC(IP6_STAT_TOOMANYHDR);
			in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_hdrerr);
			goto bad;
		}

		/*
		 * protection against faulty packet - there should be
		 * more sanity checks in header chain processing.
		 */
		if (m->m_pkthdr.len < off) {
			IP6_STATINC(IP6_STAT_TOOSHORT);
			in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_truncated);
			goto bad;
		}

		if (nxt == IPPROTO_ROUTING) {
			if (rh_present++) {
				in6_ifstat_inc(m->m_pkthdr.rcvif,
				    ifs6_in_hdrerr);
				IP6_STATINC(IP6_STAT_BADOPTIONS);
				goto bad;
			}
		}

#ifdef IPSEC
		if (ipsec_used) {
			/*
			 * enforce IPsec policy checking if we are seeing last
			 * header. note that we do not visit this with
			 * protocols with pcb layer code - like udp/tcp/raw ip.
			 */
			if ((inet6sw[ip_protox[nxt]].pr_flags
			    & PR_LASTHDR) != 0) {
				int error = ipsec6_input(m);
				if (error)
					goto bad;
			}
		}
#endif /* IPSEC */

		nxt = (*inet6sw[ip6_protox[nxt]].pr_input)(&m, &off, nxt);
	}
	return;
 bad:
	m_freem(m);
}

/*
 * set/grab in6_ifaddr correspond to IPv6 destination address.
 */
static struct m_tag *
ip6_setdstifaddr(struct mbuf *m, const struct in6_ifaddr *ia)
{
	struct m_tag *mtag;
	struct ip6aux *ip6a;

	mtag = ip6_addaux(m);
	if (mtag == NULL)
		return NULL;

	ip6a = (struct ip6aux *)(mtag + 1);
	if (in6_setscope(&ip6a->ip6a_src, ia->ia_ifp, &ip6a->ip6a_scope_id)) {
		IP6_STATINC(IP6_STAT_BADSCOPE);
		return NULL;
	}

	ip6a->ip6a_src = ia->ia_addr.sin6_addr;
	ip6a->ip6a_flags = ia->ia6_flags;
	return mtag;
}

const struct ip6aux *
ip6_getdstifaddr(struct mbuf *m)
{
	struct m_tag *mtag;

	mtag = ip6_findaux(m);
	if (mtag != NULL)
		return (struct ip6aux *)(mtag + 1);
	else
		return NULL;
}

/*
 * Hop-by-Hop options header processing. If a valid jumbo payload option is
 * included, the real payload length will be stored in plenp.
 *
 * rtalertp - XXX: should be stored more smart way
 */
int
ip6_hopopts_input(u_int32_t *plenp, u_int32_t *rtalertp, 
	struct mbuf **mp, int *offp)
{
	struct mbuf *m = *mp;
	int off = *offp, hbhlen;
	struct ip6_hbh *hbh;

	/* validation of the length of the header */
	IP6_EXTHDR_GET(hbh, struct ip6_hbh *, m,
		sizeof(struct ip6_hdr), sizeof(struct ip6_hbh));
	if (hbh == NULL) {
		IP6_STATINC(IP6_STAT_TOOSHORT);
		return -1;
	}
	hbhlen = (hbh->ip6h_len + 1) << 3;
	IP6_EXTHDR_GET(hbh, struct ip6_hbh *, m, sizeof(struct ip6_hdr),
		hbhlen);
	if (hbh == NULL) {
		IP6_STATINC(IP6_STAT_TOOSHORT);
		return -1;
	}
	KASSERT(IP6_HDR_ALIGNED_P(hbh));
	off += hbhlen;
	hbhlen -= sizeof(struct ip6_hbh);

	if (ip6_process_hopopts(m, (u_int8_t *)hbh + sizeof(struct ip6_hbh),
				hbhlen, rtalertp, plenp) < 0)
		return (-1);

	*offp = off;
	*mp = m;
	return (0);
}

/*
 * Search header for all Hop-by-hop options and process each option.
 * This function is separate from ip6_hopopts_input() in order to
 * handle a case where the sending node itself process its hop-by-hop
 * options header. In such a case, the function is called from ip6_output().
 *
 * The function assumes that hbh header is located right after the IPv6 header
 * (RFC2460 p7), opthead is pointer into data content in m, and opthead to
 * opthead + hbhlen is located in continuous memory region.
 */
static int
ip6_process_hopopts(struct mbuf *m, u_int8_t *opthead, int hbhlen, 
	u_int32_t *rtalertp, u_int32_t *plenp)
{
	struct ip6_hdr *ip6;
	int optlen = 0;
	u_int8_t *opt = opthead;
	u_int16_t rtalert_val;
	u_int32_t jumboplen;
	const int erroff = sizeof(struct ip6_hdr) + sizeof(struct ip6_hbh);

	for (; hbhlen > 0; hbhlen -= optlen, opt += optlen) {
		switch (*opt) {
		case IP6OPT_PAD1:
			optlen = 1;
			break;
		case IP6OPT_PADN:
			if (hbhlen < IP6OPT_MINLEN) {
				IP6_STATINC(IP6_STAT_TOOSMALL);
				goto bad;
			}
			optlen = *(opt + 1) + 2;
			break;
		case IP6OPT_RTALERT:
			/* XXX may need check for alignment */
			if (hbhlen < IP6OPT_RTALERT_LEN) {
				IP6_STATINC(IP6_STAT_TOOSMALL);
				goto bad;
			}
			if (*(opt + 1) != IP6OPT_RTALERT_LEN - 2) {
				/* XXX stat */
				icmp6_error(m, ICMP6_PARAM_PROB,
				    ICMP6_PARAMPROB_HEADER,
				    erroff + opt + 1 - opthead);
				return (-1);
			}
			optlen = IP6OPT_RTALERT_LEN;
			memcpy((void *)&rtalert_val, (void *)(opt + 2), 2);
			*rtalertp = ntohs(rtalert_val);
			break;
		case IP6OPT_JUMBO:
			/* XXX may need check for alignment */
			if (hbhlen < IP6OPT_JUMBO_LEN) {
				IP6_STATINC(IP6_STAT_TOOSMALL);
				goto bad;
			}
			if (*(opt + 1) != IP6OPT_JUMBO_LEN - 2) {
				/* XXX stat */
				icmp6_error(m, ICMP6_PARAM_PROB,
				    ICMP6_PARAMPROB_HEADER,
				    erroff + opt + 1 - opthead);
				return (-1);
			}
			optlen = IP6OPT_JUMBO_LEN;

			/*
			 * IPv6 packets that have non 0 payload length
			 * must not contain a jumbo payload option.
			 */
			ip6 = mtod(m, struct ip6_hdr *);
			if (ip6->ip6_plen) {
				IP6_STATINC(IP6_STAT_BADOPTIONS);
				icmp6_error(m, ICMP6_PARAM_PROB,
				    ICMP6_PARAMPROB_HEADER,
				    erroff + opt - opthead);
				return (-1);
			}

			/*
			 * We may see jumbolen in unaligned location, so
			 * we'd need to perform bcopy().
			 */
			memcpy(&jumboplen, opt + 2, sizeof(jumboplen));
			jumboplen = (u_int32_t)htonl(jumboplen);

#if 1
			/*
			 * if there are multiple jumbo payload options,
			 * *plenp will be non-zero and the packet will be
			 * rejected.
			 * the behavior may need some debate in ipngwg -
			 * multiple options does not make sense, however,
			 * there's no explicit mention in specification.
			 */
			if (*plenp != 0) {
				IP6_STATINC(IP6_STAT_BADOPTIONS);
				icmp6_error(m, ICMP6_PARAM_PROB,
				    ICMP6_PARAMPROB_HEADER,
				    erroff + opt + 2 - opthead);
				return (-1);
			}
#endif

			/*
			 * jumbo payload length must be larger than 65535.
			 */
			if (jumboplen <= IPV6_MAXPACKET) {
				IP6_STATINC(IP6_STAT_BADOPTIONS);
				icmp6_error(m, ICMP6_PARAM_PROB,
				    ICMP6_PARAMPROB_HEADER,
				    erroff + opt + 2 - opthead);
				return (-1);
			}
			*plenp = jumboplen;

			break;
		default:		/* unknown option */
			if (hbhlen < IP6OPT_MINLEN) {
				IP6_STATINC(IP6_STAT_TOOSMALL);
				goto bad;
			}
			optlen = ip6_unknown_opt(opt, m,
			    erroff + opt - opthead);
			if (optlen == -1)
				return (-1);
			optlen += 2;
			break;
		}
	}

	return (0);

  bad:
	m_freem(m);
	return (-1);
}

/*
 * Unknown option processing.
 * The third argument `off' is the offset from the IPv6 header to the option,
 * which is necessary if the IPv6 header the and option header and IPv6 header
 * is not continuous in order to return an ICMPv6 error.
 */
int
ip6_unknown_opt(u_int8_t *optp, struct mbuf *m, int off)
{
	struct ip6_hdr *ip6;

	switch (IP6OPT_TYPE(*optp)) {
	case IP6OPT_TYPE_SKIP: /* ignore the option */
		return ((int)*(optp + 1));
	case IP6OPT_TYPE_DISCARD:	/* silently discard */
		m_freem(m);
		return (-1);
	case IP6OPT_TYPE_FORCEICMP: /* send ICMP even if multicasted */
		IP6_STATINC(IP6_STAT_BADOPTIONS);
		icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_OPTION, off);
		return (-1);
	case IP6OPT_TYPE_ICMP: /* send ICMP if not multicasted */
		IP6_STATINC(IP6_STAT_BADOPTIONS);
		ip6 = mtod(m, struct ip6_hdr *);
		if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst) ||
		    (m->m_flags & (M_BCAST|M_MCAST)))
			m_freem(m);
		else
			icmp6_error(m, ICMP6_PARAM_PROB,
				    ICMP6_PARAMPROB_OPTION, off);
		return (-1);
	}

	m_freem(m);		/* XXX: NOTREACHED */
	return (-1);
}

/*
 * Create the "control" list for this pcb.
 *
 * The routine will be called from upper layer handlers like tcp6_input().
 * Thus the routine assumes that the caller (tcp6_input) have already
 * called IP6_EXTHDR_CHECK() and all the extension headers are located in the
 * very first mbuf on the mbuf chain.
 * We may want to add some infinite loop prevention or sanity checks for safety.
 * (This applies only when you are using KAME mbuf chain restriction, i.e.
 * you are using IP6_EXTHDR_CHECK() not m_pulldown())
 */
void
ip6_savecontrol(struct in6pcb *in6p, struct mbuf **mp, 
	struct ip6_hdr *ip6, struct mbuf *m)
{
#ifdef RFC2292
#define IS2292(x, y)	((in6p->in6p_flags & IN6P_RFC2292) ? (x) : (y))
#else
#define IS2292(x, y)	(y)
#endif

	if (in6p->in6p_socket->so_options & SO_TIMESTAMP
#ifdef SO_OTIMESTAMP
	    || in6p->in6p_socket->so_options & SO_OTIMESTAMP
#endif
	) {
		struct timeval tv;

		microtime(&tv);
#ifdef SO_OTIMESTAMP
		if (in6p->in6p_socket->so_options & SO_OTIMESTAMP) {
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

	/* some OSes call this logic with IPv4 packet, for SO_TIMESTAMP */
	if ((ip6->ip6_vfc & IPV6_VERSION_MASK) != IPV6_VERSION)
		return;

	/* RFC 2292 sec. 5 */
	if ((in6p->in6p_flags & IN6P_PKTINFO) != 0) {
		struct in6_pktinfo pi6;

		memcpy(&pi6.ipi6_addr, &ip6->ip6_dst, sizeof(struct in6_addr));
		in6_clearscope(&pi6.ipi6_addr);	/* XXX */
		pi6.ipi6_ifindex = m->m_pkthdr.rcvif ?
		    m->m_pkthdr.rcvif->if_index : 0;
		*mp = sbcreatecontrol((void *) &pi6,
		    sizeof(struct in6_pktinfo),
		    IS2292(IPV6_2292PKTINFO, IPV6_PKTINFO), IPPROTO_IPV6);
		if (*mp)
			mp = &(*mp)->m_next;
	}

	if (in6p->in6p_flags & IN6P_HOPLIMIT) {
		int hlim = ip6->ip6_hlim & 0xff;

		*mp = sbcreatecontrol((void *) &hlim, sizeof(int),
		    IS2292(IPV6_2292HOPLIMIT, IPV6_HOPLIMIT), IPPROTO_IPV6);
		if (*mp)
			mp = &(*mp)->m_next;
	}

	if ((in6p->in6p_flags & IN6P_TCLASS) != 0) {
		u_int32_t flowinfo;
		int tclass;

		flowinfo = (u_int32_t)ntohl(ip6->ip6_flow & IPV6_FLOWINFO_MASK);
		flowinfo >>= 20;

		tclass = flowinfo & 0xff;
		*mp = sbcreatecontrol((void *)&tclass, sizeof(tclass),
		    IPV6_TCLASS, IPPROTO_IPV6);

		if (*mp)
			mp = &(*mp)->m_next;
	}

	/*
	 * IPV6_HOPOPTS socket option.  Recall that we required super-user
	 * privilege for the option (see ip6_ctloutput), but it might be too
	 * strict, since there might be some hop-by-hop options which can be
	 * returned to normal user.
	 * See also RFC3542 section 8 (or RFC2292 section 6).
	 */
	if ((in6p->in6p_flags & IN6P_HOPOPTS) != 0) {
		/*
		 * Check if a hop-by-hop options header is contatined in the
		 * received packet, and if so, store the options as ancillary
		 * data. Note that a hop-by-hop options header must be
		 * just after the IPv6 header, which fact is assured through
		 * the IPv6 input processing.
		 */
		struct ip6_hdr *xip6 = mtod(m, struct ip6_hdr *);
		if (xip6->ip6_nxt == IPPROTO_HOPOPTS) {
			struct ip6_hbh *hbh;
			int hbhlen;
			struct mbuf *ext;

			ext = ip6_pullexthdr(m, sizeof(struct ip6_hdr),
			    xip6->ip6_nxt);
			if (ext == NULL) {
				IP6_STATINC(IP6_STAT_TOOSHORT);
				return;
			}
			hbh = mtod(ext, struct ip6_hbh *);
			hbhlen = (hbh->ip6h_len + 1) << 3;
			if (hbhlen != ext->m_len) {
				m_freem(ext);
				IP6_STATINC(IP6_STAT_TOOSHORT);
				return;
			}

			/*
			 * XXX: We copy whole the header even if a jumbo
			 * payload option is included, which option is to
			 * be removed before returning in the RFC 2292.
			 * Note: this constraint is removed in RFC3542.
			 */
			*mp = sbcreatecontrol((void *)hbh, hbhlen,
			    IS2292(IPV6_2292HOPOPTS, IPV6_HOPOPTS),
			    IPPROTO_IPV6);
			if (*mp)
				mp = &(*mp)->m_next;
			m_freem(ext);
		}
	}

	/* IPV6_DSTOPTS and IPV6_RTHDR socket options */
	if (in6p->in6p_flags & (IN6P_DSTOPTS | IN6P_RTHDR)) {
		struct ip6_hdr *xip6 = mtod(m, struct ip6_hdr *);
		int nxt = xip6->ip6_nxt, off = sizeof(struct ip6_hdr);

		/*
		 * Search for destination options headers or routing
		 * header(s) through the header chain, and stores each
		 * header as ancillary data.
		 * Note that the order of the headers remains in
		 * the chain of ancillary data.
		 */
		for (;;) {	/* is explicit loop prevention necessary? */
			struct ip6_ext *ip6e = NULL;
			int elen;
			struct mbuf *ext = NULL;

			/*
			 * if it is not an extension header, don't try to
			 * pull it from the chain.
			 */
			switch (nxt) {
			case IPPROTO_DSTOPTS:
			case IPPROTO_ROUTING:
			case IPPROTO_HOPOPTS:
			case IPPROTO_AH: /* is it possible? */
				break;
			default:
				goto loopend;
			}

			ext = ip6_pullexthdr(m, off, nxt);
			if (ext == NULL) {
				IP6_STATINC(IP6_STAT_TOOSHORT);
				return;
			}
			ip6e = mtod(ext, struct ip6_ext *);
			if (nxt == IPPROTO_AH)
				elen = (ip6e->ip6e_len + 2) << 2;
			else
				elen = (ip6e->ip6e_len + 1) << 3;
			if (elen != ext->m_len) {
				m_freem(ext);
				IP6_STATINC(IP6_STAT_TOOSHORT);
				return;
			}
			KASSERT(IP6_HDR_ALIGNED_P(ip6e));

			switch (nxt) {
			case IPPROTO_DSTOPTS:
				if (!(in6p->in6p_flags & IN6P_DSTOPTS))
					break;

				*mp = sbcreatecontrol((void *)ip6e, elen,
				    IS2292(IPV6_2292DSTOPTS, IPV6_DSTOPTS),
				    IPPROTO_IPV6);
				if (*mp)
					mp = &(*mp)->m_next;
				break;

			case IPPROTO_ROUTING:
				if (!(in6p->in6p_flags & IN6P_RTHDR))
					break;

				*mp = sbcreatecontrol((void *)ip6e, elen,
				    IS2292(IPV6_2292RTHDR, IPV6_RTHDR),
				    IPPROTO_IPV6);
				if (*mp)
					mp = &(*mp)->m_next;
				break;

			case IPPROTO_HOPOPTS:
			case IPPROTO_AH: /* is it possible? */
				break;

			default:
				/*
			 	 * other cases have been filtered in the above.
				 * none will visit this case.  here we supply
				 * the code just in case (nxt overwritten or
				 * other cases).
				 */
				m_freem(ext);
				goto loopend;

			}

			/* proceed with the next header. */
			off += elen;
			nxt = ip6e->ip6e_nxt;
			ip6e = NULL;
			m_freem(ext);
			ext = NULL;
		}
	  loopend:
	  	;
	}
}
#undef IS2292


void
ip6_notify_pmtu(struct in6pcb *in6p, const struct sockaddr_in6 *dst,
    uint32_t *mtu)
{
	struct socket *so;
	struct mbuf *m_mtu;
	struct ip6_mtuinfo mtuctl;

	so = in6p->in6p_socket;

	if (mtu == NULL)
		return;

#ifdef DIAGNOSTIC
	if (so == NULL)		/* I believe this is impossible */
		panic("ip6_notify_pmtu: socket is NULL");
#endif

	memset(&mtuctl, 0, sizeof(mtuctl));	/* zero-clear for safety */
	mtuctl.ip6m_mtu = *mtu;
	mtuctl.ip6m_addr = *dst;
	if (sa6_recoverscope(&mtuctl.ip6m_addr))
		return;

	if ((m_mtu = sbcreatecontrol((void *)&mtuctl, sizeof(mtuctl),
	    IPV6_PATHMTU, IPPROTO_IPV6)) == NULL)
		return;

	if (sbappendaddr(&so->so_rcv, (const struct sockaddr *)dst, NULL, m_mtu)
	    == 0) {
		m_freem(m_mtu);
		/* XXX: should count statistics */
	} else
		sorwakeup(so);

	return;
}

/*
 * pull single extension header from mbuf chain.  returns single mbuf that
 * contains the result, or NULL on error.
 */
static struct mbuf *
ip6_pullexthdr(struct mbuf *m, size_t off, int nxt)
{
	struct ip6_ext ip6e;
	size_t elen;
	struct mbuf *n;

#ifdef DIAGNOSTIC
	switch (nxt) {
	case IPPROTO_DSTOPTS:
	case IPPROTO_ROUTING:
	case IPPROTO_HOPOPTS:
	case IPPROTO_AH: /* is it possible? */
		break;
	default:
		printf("ip6_pullexthdr: invalid nxt=%d\n", nxt);
	}
#endif

	m_copydata(m, off, sizeof(ip6e), (void *)&ip6e);
	if (nxt == IPPROTO_AH)
		elen = (ip6e.ip6e_len + 2) << 2;
	else
		elen = (ip6e.ip6e_len + 1) << 3;

	MGET(n, M_DONTWAIT, MT_DATA);
	if (n && elen >= MLEN) {
		MCLGET(n, M_DONTWAIT);
		if ((n->m_flags & M_EXT) == 0) {
			m_free(n);
			n = NULL;
		}
	}
	if (!n)
		return NULL;

	n->m_len = 0;
	if (elen >= M_TRAILINGSPACE(n)) {
		m_free(n);
		return NULL;
	}

	m_copydata(m, off, elen, mtod(n, void *));
	n->m_len = elen;
	return n;
}

/*
 * Get pointer to the previous header followed by the header
 * currently processed.
 * XXX: This function supposes that
 *	M includes all headers,
 *	the next header field and the header length field of each header
 *	are valid, and
 *	the sum of each header length equals to OFF.
 * Because of these assumptions, this function must be called very
 * carefully. Moreover, it will not be used in the near future when
 * we develop `neater' mechanism to process extension headers.
 */
u_int8_t *
ip6_get_prevhdr(struct mbuf *m, int off)
{
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);

	if (off == sizeof(struct ip6_hdr))
		return (&ip6->ip6_nxt);
	else {
		int len, nxt;
		struct ip6_ext *ip6e = NULL;

		nxt = ip6->ip6_nxt;
		len = sizeof(struct ip6_hdr);
		while (len < off) {
			ip6e = (struct ip6_ext *)(mtod(m, char *) + len);

			switch (nxt) {
			case IPPROTO_FRAGMENT:
				len += sizeof(struct ip6_frag);
				break;
			case IPPROTO_AH:
				len += (ip6e->ip6e_len + 2) << 2;
				break;
			default:
				len += (ip6e->ip6e_len + 1) << 3;
				break;
			}
			nxt = ip6e->ip6e_nxt;
		}
		if (ip6e)
			return (&ip6e->ip6e_nxt);
		else
			return NULL;
	}
}

/*
 * get next header offset.  m will be retained.
 */
int
ip6_nexthdr(struct mbuf *m, int off, int proto, int *nxtp)
{
	struct ip6_hdr ip6;
	struct ip6_ext ip6e;
	struct ip6_frag fh;

	/* just in case */
	if (m == NULL)
		panic("ip6_nexthdr: m == NULL");
	if ((m->m_flags & M_PKTHDR) == 0 || m->m_pkthdr.len < off)
		return -1;

	switch (proto) {
	case IPPROTO_IPV6:
		/* do not chase beyond intermediate IPv6 headers */
		if (off != 0)
			return -1;
		if (m->m_pkthdr.len < off + sizeof(ip6))
			return -1;
		m_copydata(m, off, sizeof(ip6), (void *)&ip6);
		if (nxtp)
			*nxtp = ip6.ip6_nxt;
		off += sizeof(ip6);
		return off;

	case IPPROTO_FRAGMENT:
		/*
		 * terminate parsing if it is not the first fragment,
		 * it does not make sense to parse through it.
		 */
		if (m->m_pkthdr.len < off + sizeof(fh))
			return -1;
		m_copydata(m, off, sizeof(fh), (void *)&fh);
		if ((fh.ip6f_offlg & IP6F_OFF_MASK) != 0)
			return -1;
		if (nxtp)
			*nxtp = fh.ip6f_nxt;
		off += sizeof(struct ip6_frag);
		return off;

	case IPPROTO_AH:
		if (m->m_pkthdr.len < off + sizeof(ip6e))
			return -1;
		m_copydata(m, off, sizeof(ip6e), (void *)&ip6e);
		if (nxtp)
			*nxtp = ip6e.ip6e_nxt;
		off += (ip6e.ip6e_len + 2) << 2;
		if (m->m_pkthdr.len < off)
			return -1;
		return off;

	case IPPROTO_HOPOPTS:
	case IPPROTO_ROUTING:
	case IPPROTO_DSTOPTS:
		if (m->m_pkthdr.len < off + sizeof(ip6e))
			return -1;
		m_copydata(m, off, sizeof(ip6e), (void *)&ip6e);
		if (nxtp)
			*nxtp = ip6e.ip6e_nxt;
		off += (ip6e.ip6e_len + 1) << 3;
		if (m->m_pkthdr.len < off)
			return -1;
		return off;

	case IPPROTO_NONE:
	case IPPROTO_ESP:
	case IPPROTO_IPCOMP:
		/* give up */
		return -1;

	default:
		return -1;
	}
}

/*
 * get offset for the last header in the chain.  m will be kept untainted.
 */
int
ip6_lasthdr(struct mbuf *m, int off, int proto, int *nxtp)
{
	int newoff;
	int nxt;

	if (!nxtp) {
		nxt = -1;
		nxtp = &nxt;
	}
	for (;;) {
		newoff = ip6_nexthdr(m, off, proto, nxtp);
		if (newoff < 0)
			return off;
		else if (newoff < off)
			return -1;	/* invalid */
		else if (newoff == off)
			return newoff;

		off = newoff;
		proto = *nxtp;
	}
}

struct m_tag *
ip6_addaux(struct mbuf *m)
{
	struct m_tag *mtag;

	mtag = m_tag_find(m, PACKET_TAG_INET6, NULL);
	if (!mtag) {
		mtag = m_tag_get(PACKET_TAG_INET6, sizeof(struct ip6aux),
		    M_NOWAIT);
		if (mtag) {
			m_tag_prepend(m, mtag);
			memset(mtag + 1, 0, sizeof(struct ip6aux));
		}
	}
	return mtag;
}

struct m_tag *
ip6_findaux(struct mbuf *m)
{
	struct m_tag *mtag;

	mtag = m_tag_find(m, PACKET_TAG_INET6, NULL);
	return mtag;
}

void
ip6_delaux(struct mbuf *m)
{
	struct m_tag *mtag;

	mtag = m_tag_find(m, PACKET_TAG_INET6, NULL);
	if (mtag)
		m_tag_delete(m, mtag);
}

#ifdef GATEWAY
/* 
 * sysctl helper routine for net.inet.ip6.maxflows. Since
 * we could reduce this value, call ip6flow_reap();
 */
static int
sysctl_net_inet6_ip6_maxflows(SYSCTLFN_ARGS)
{  
	int error;
  
	error = sysctl_lookup(SYSCTLFN_CALL(rnode));
	if (error || newp == NULL)
		return (error);
 
	mutex_enter(softnet_lock);
	KERNEL_LOCK(1, NULL);

	ip6flow_reap(0);

	KERNEL_UNLOCK_ONE(NULL);
	mutex_exit(softnet_lock);
 
	return (0);
}

static int
sysctl_net_inet6_ip6_hashsize(SYSCTLFN_ARGS)
{  
	int error, tmp;
	struct sysctlnode node;

	node = *rnode;
	tmp = ip6_hashsize;
	node.sysctl_data = &tmp;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return (error);

	if ((tmp & (tmp - 1)) == 0 && tmp != 0) {
		/*
		 * Can only fail due to malloc()
		 */
		mutex_enter(softnet_lock);
		KERNEL_LOCK(1, NULL);

		error = ip6flow_invalidate_all(tmp);

		KERNEL_UNLOCK_ONE(NULL);
		mutex_exit(softnet_lock);
	} else {
		/*
		 * EINVAL if not a power of 2
		 */
		error = EINVAL;
	}	

	return error;
}
#endif /* GATEWAY */

/*
 * System control for IP6
 */

const u_char inet6ctlerrmap[PRC_NCMDS] = {
	0,		0,		0,		0,
	0,		EMSGSIZE,	EHOSTDOWN,	EHOSTUNREACH,
	EHOSTUNREACH,	EHOSTUNREACH,	ECONNREFUSED,	ECONNREFUSED,
	EMSGSIZE,	EHOSTUNREACH,	0,		0,
	0,		0,		0,		0,
	ENOPROTOOPT
};

static int
sysctl_net_inet6_ip6_stats(SYSCTLFN_ARGS)
{

	return (NETSTAT_SYSCTL(ip6stat_percpu, IP6_NSTATS));
}

static void
sysctl_net_inet6_ip6_setup(struct sysctllog **clog)
{
#ifdef RFC2292
#define IS2292(x, y)	((in6p->in6p_flags & IN6P_RFC2292) ? (x) : (y))
#else
#define IS2292(x, y)	(y)
#endif

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "inet6",
		       SYSCTL_DESCR("PF_INET6 related settings"),
		       NULL, 0, NULL, 0,
		       CTL_NET, PF_INET6, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "ip6",
		       SYSCTL_DESCR("IPv6 related settings"),
		       NULL, 0, NULL, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "forwarding",
		       SYSCTL_DESCR("Enable forwarding of INET6 datagrams"),
		       NULL, 0, &ip6_forwarding, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       IPV6CTL_FORWARDING, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "redirect",
		       SYSCTL_DESCR("Enable sending of ICMPv6 redirect messages"),
		       NULL, 0, &ip6_sendredirects, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       IPV6CTL_SENDREDIRECTS, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "hlim",
		       SYSCTL_DESCR("Hop limit for an INET6 datagram"),
		       NULL, 0, &ip6_defhlim, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       IPV6CTL_DEFHLIM, CTL_EOL);
#ifdef notyet
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "mtu", NULL,
		       NULL, 0, &, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       IPV6CTL_DEFMTU, CTL_EOL);
#endif
#ifdef __no_idea__
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "forwsrcrt", NULL,
		       NULL, 0, &?, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       IPV6CTL_FORWSRCRT, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_STRUCT, "mrtstats", NULL,
		       NULL, 0, &?, sizeof(?),
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       IPV6CTL_MRTSTATS, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_?, "mrtproto", NULL,
		       NULL, 0, &?, sizeof(?),
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       IPV6CTL_MRTPROTO, CTL_EOL);
#endif
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "maxfragpackets",
		       SYSCTL_DESCR("Maximum number of fragments to buffer "
				    "for reassembly"),
		       NULL, 0, &ip6_maxfragpackets, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       IPV6CTL_MAXFRAGPACKETS, CTL_EOL);
#ifdef __no_idea__
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "sourcecheck", NULL,
		       NULL, 0, &?, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       IPV6CTL_SOURCECHECK, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "sourcecheck_logint", NULL,
		       NULL, 0, &?, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       IPV6CTL_SOURCECHECK_LOGINT, CTL_EOL);
#endif
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "accept_rtadv",
		       SYSCTL_DESCR("Accept router advertisements"),
		       NULL, 0, &ip6_accept_rtadv, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       IPV6CTL_ACCEPT_RTADV, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "rtadv_maxroutes",
		       SYSCTL_DESCR("Maximum number of routes accepted via router advertisements"),
		       NULL, 0, &ip6_rtadv_maxroutes, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       IPV6CTL_RTADV_MAXROUTES, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_INT, "rtadv_numroutes",
		       SYSCTL_DESCR("Current number of routes accepted via router advertisements"),
		       NULL, 0, &nd6_numroutes, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       IPV6CTL_RTADV_NUMROUTES, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "keepfaith",
		       SYSCTL_DESCR("Activate faith interface"),
		       NULL, 0, &ip6_keepfaith, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       IPV6CTL_KEEPFAITH, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "log_interval",
		       SYSCTL_DESCR("Minumum interval between logging "
				    "unroutable packets"),
		       NULL, 0, &ip6_log_interval, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       IPV6CTL_LOG_INTERVAL, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "hdrnestlimit",
		       SYSCTL_DESCR("Maximum number of nested IPv6 headers"),
		       NULL, 0, &ip6_hdrnestlimit, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       IPV6CTL_HDRNESTLIMIT, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "dad_count",
		       SYSCTL_DESCR("Number of Duplicate Address Detection "
				    "probes to send"),
		       NULL, 0, &ip6_dad_count, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       IPV6CTL_DAD_COUNT, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "auto_flowlabel",
		       SYSCTL_DESCR("Assign random IPv6 flow labels"),
		       NULL, 0, &ip6_auto_flowlabel, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       IPV6CTL_AUTO_FLOWLABEL, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "defmcasthlim",
		       SYSCTL_DESCR("Default multicast hop limit"),
		       NULL, 0, &ip6_defmcasthlim, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       IPV6CTL_DEFMCASTHLIM, CTL_EOL);
#if NGIF > 0
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "gifhlim",
		       SYSCTL_DESCR("Default hop limit for a gif tunnel datagram"),
		       NULL, 0, &ip6_gif_hlim, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       IPV6CTL_GIF_HLIM, CTL_EOL);
#endif /* NGIF */
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRING, "kame_version",
		       SYSCTL_DESCR("KAME Version"),
		       NULL, 0, __UNCONST(__KAME_VERSION), 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       IPV6CTL_KAME_VERSION, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "use_deprecated",
		       SYSCTL_DESCR("Allow use of deprecated addresses as "
				    "source addresses"),
		       NULL, 0, &ip6_use_deprecated, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       IPV6CTL_USE_DEPRECATED, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "rr_prune", NULL,
		       NULL, 0, &ip6_rr_prune, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       IPV6CTL_RR_PRUNE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT
#ifndef INET6_BINDV6ONLY
		       |CTLFLAG_READWRITE,
#endif
		       CTLTYPE_INT, "v6only",
		       SYSCTL_DESCR("Disallow PF_INET6 sockets from connecting "
				    "to PF_INET sockets"),
		       NULL, 0, &ip6_v6only, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       IPV6CTL_V6ONLY, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "auto_linklocal",
		       SYSCTL_DESCR("Default value of per-interface flag for "
		                    "adding an IPv6 link-local address to "
				    "interfaces when attached"),
		       NULL, 0, &ip6_auto_linklocal, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       IPV6CTL_AUTO_LINKLOCAL, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "anonportmin",
		       SYSCTL_DESCR("Lowest ephemeral port number to assign"),
		       sysctl_net_inet_ip_ports, 0, &ip6_anonportmin, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       IPV6CTL_ANONPORTMIN, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "anonportmax",
		       SYSCTL_DESCR("Highest ephemeral port number to assign"),
		       sysctl_net_inet_ip_ports, 0, &ip6_anonportmax, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       IPV6CTL_ANONPORTMAX, CTL_EOL);
#ifndef IPNOPRIVPORTS
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "lowportmin",
		       SYSCTL_DESCR("Lowest privileged ephemeral port number "
				    "to assign"),
		       sysctl_net_inet_ip_ports, 0, &ip6_lowportmin, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       IPV6CTL_LOWPORTMIN, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "lowportmax",
		       SYSCTL_DESCR("Highest privileged ephemeral port number "
				    "to assign"),
		       sysctl_net_inet_ip_ports, 0, &ip6_lowportmax, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       IPV6CTL_LOWPORTMAX, CTL_EOL);
#endif /* IPNOPRIVPORTS */
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "use_tempaddr",
		       SYSCTL_DESCR("Use temporary address"),
		       NULL, 0, &ip6_use_tempaddr, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "prefer_tempaddr",
		       SYSCTL_DESCR("Prefer temporary address as source "
		                    "address"),
		       NULL, 0, &ip6_prefer_tempaddr, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "temppltime",
		       SYSCTL_DESCR("preferred lifetime of a temporary address"),
		       NULL, 0, &ip6_temp_preferred_lifetime, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "tempvltime",
		       SYSCTL_DESCR("valid lifetime of a temporary address"),
		       NULL, 0, &ip6_temp_valid_lifetime, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "maxfrags",
		       SYSCTL_DESCR("Maximum fragments in reassembly queue"),
		       NULL, 0, &ip6_maxfrags, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       IPV6CTL_MAXFRAGS, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRUCT, "stats",
		       SYSCTL_DESCR("IPv6 statistics"),
		       sysctl_net_inet6_ip6_stats, 0, NULL, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       IPV6CTL_STATS, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "use_defaultzone",
		       SYSCTL_DESCR("Whether to use the default scope zones"),
		       NULL, 0, &ip6_use_defzone, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       IPV6CTL_USE_DEFAULTZONE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "mcast_pmtu",
		       SYSCTL_DESCR("Enable pMTU discovery for multicast packet"),
		       NULL, 0, &ip6_mcast_pmtu, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       CTL_CREATE, CTL_EOL);
#ifdef GATEWAY 
	sysctl_createv(clog, 0, NULL, NULL,
			CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
			CTLTYPE_INT, "maxflows",
			SYSCTL_DESCR("Number of flows for fast forwarding (IPv6)"),
			sysctl_net_inet6_ip6_maxflows, 0, &ip6_maxflows, 0,
			CTL_NET, PF_INET6, IPPROTO_IPV6,
			CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
			CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
			CTLTYPE_INT, "hashsize",
			SYSCTL_DESCR("Size of hash table for fast forwarding (IPv6)"),
			sysctl_net_inet6_ip6_hashsize, 0, &ip6_hashsize, 0,
			CTL_NET, PF_INET6, IPPROTO_IPV6,
			CTL_CREATE, CTL_EOL);
#endif
	/* anonportalgo RFC6056 subtree */
	const struct sysctlnode *portalgo_node;
	sysctl_createv(clog, 0, NULL, &portalgo_node,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "anonportalgo",
		       SYSCTL_DESCR("Anonymous port algorithm selection (RFC 6056)"),
	    	       NULL, 0, NULL, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6, CTL_CREATE, CTL_EOL);
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
	               sysctl_portalgo_selected6, 0, NULL, PORTALGO_MAXLEN,
		       CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &portalgo_node, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_STRUCT, "reserve",
		       SYSCTL_DESCR("bitmap of reserved ports"),
		       sysctl_portalgo_reserve6, 0, NULL, 0,
		       CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "neighborgcthresh",
		       SYSCTL_DESCR("Maximum number of entries in neighbor"
			" cache"),
		       NULL, 1, &ip6_neighborgcthresh, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "maxifprefixes",
		       SYSCTL_DESCR("Maximum number of prefixes created by"
			   " route advertisement per interface"),
		       NULL, 1, &ip6_maxifprefixes, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "maxifdefrouters",
		       SYSCTL_DESCR("Maximum number of default routers created"
			   " by route advertisement per interface"),
		       NULL, 1, &ip6_maxifdefrouters, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "maxdynroutes",
		       SYSCTL_DESCR("Maximum number of routes created via"
			   " redirect"),
		       NULL, 1, &ip6_maxdynroutes, 0,
		       CTL_NET, PF_INET6, IPPROTO_IPV6,
		       CTL_CREATE, CTL_EOL);
}

void
ip6_statinc(u_int stat)
{

	KASSERT(stat < IP6_NSTATS);
	IP6_STATINC(stat);
}
