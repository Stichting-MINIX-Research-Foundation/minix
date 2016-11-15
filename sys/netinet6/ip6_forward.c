/*	$NetBSD: ip6_forward.c,v 1.78 2015/08/24 22:21:27 pooka Exp $	*/
/*	$KAME: ip6_forward.c,v 1.109 2002/09/11 08:10:17 sakane Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: ip6_forward.c,v 1.78 2015/08/24 22:21:27 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_gateway.h"
#include "opt_ipsec.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/route.h>
#include <net/pfil.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/ip6_private.h>
#include <netinet6/scope6_var.h>
#include <netinet/icmp6.h>
#include <netinet6/nd6.h>

#ifdef IPSEC
#include <netipsec/ipsec.h>
#include <netipsec/ipsec6.h>
#include <netipsec/key.h>
#include <netipsec/xform.h>
#endif /* IPSEC */

#include <net/net_osdep.h>

struct	route ip6_forward_rt;

extern pfil_head_t *inet6_pfil_hook;	/* XXX */

static void __printflike(4, 5)
ip6_cantforward(const struct ip6_hdr *ip6, const struct ifnet *srcifp,
    const struct ifnet *dstifp, const char *fmt, ...)
{
	char sbuf[INET6_ADDRSTRLEN], dbuf[INET6_ADDRSTRLEN];
	char reason[256];
	va_list ap;
	uint64_t *ip6s;

	/* update statistics */
	ip6s = IP6_STAT_GETREF();
	ip6s[IP6_STAT_CANTFORWARD]++;
	if (dstifp)
		ip6s[IP6_STAT_BADSCOPE]++;
	IP6_STAT_PUTREF();

	if (dstifp)
		in6_ifstat_inc(dstifp, ifs6_in_discard);

	if (ip6_log_time + ip6_log_interval >= time_uptime)
		return;
	ip6_log_time = time_uptime;

	va_start(ap, fmt);
	vsnprintf(reason, sizeof(reason), fmt, ap);
	va_end(ap);

	log(LOG_DEBUG, "Cannot forward from %s@%s to %s@%s nxt %d (%s)\n",
	    IN6_PRINT(sbuf, &ip6->ip6_src), srcifp ? if_name(srcifp) : "?",
	    IN6_PRINT(dbuf, &ip6->ip6_dst), dstifp ? if_name(dstifp) : "?",
	    ip6->ip6_nxt, reason);
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
 */

void
ip6_forward(struct mbuf *m, int srcrt)
{
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	const struct sockaddr_in6 *dst;
	struct rtentry *rt;
	int error = 0, type = 0, code = 0;
	struct mbuf *mcopy = NULL;
	struct ifnet *origifp;	/* maybe unnecessary */
	uint32_t inzone, outzone;
	struct in6_addr src_in6, dst_in6;
#ifdef IPSEC
	int needipsec = 0;
	struct secpolicy *sp = NULL;
#endif

	/*
	 * Clear any in-bound checksum flags for this packet.
	 */
	m->m_pkthdr.csum_flags = 0;

	/*
	 * Do not forward packets to multicast destination (should be handled
	 * by ip6_mforward().
	 * Do not forward packets with unspecified source.  It was discussed
	 * in July 2000, on ipngwg mailing list.
	 */
	if ((m->m_flags & (M_BCAST|M_MCAST)) != 0 ||
	    IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst) ||
	    IN6_IS_ADDR_UNSPECIFIED(&ip6->ip6_src)) {
		ip6_cantforward(ip6, m->m_pkthdr.rcvif, NULL,
		    ((m->m_flags & (M_BCAST|M_MCAST)) != 0) ? "bcast/mcast" :
		    IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst) ? "mcast/dst" :
		    "unspec/src");
		m_freem(m);
		return;
	}

	if (ip6->ip6_hlim <= IPV6_HLIMDEC) {
		/* XXX in6_ifstat_inc(rt->rt_ifp, ifs6_in_discard) */
		icmp6_error(m, ICMP6_TIME_EXCEEDED,
				ICMP6_TIME_EXCEED_TRANSIT, 0);
		return;
	}
	ip6->ip6_hlim -= IPV6_HLIMDEC;

	/*
	 * Save at most ICMPV6_PLD_MAXLEN (= the min IPv6 MTU -
	 * size of IPv6 + ICMPv6 headers) bytes of the packet in case
	 * we need to generate an ICMP6 message to the src.
	 * Thanks to M_EXT, in most cases copy will not occur.
	 *
	 * It is important to save it before IPsec processing as IPsec
	 * processing may modify the mbuf.
	 */
	mcopy = m_copy(m, 0, imin(m->m_pkthdr.len, ICMPV6_PLD_MAXLEN));

#ifdef IPSEC
	if (ipsec_used) {
		/* Check the security policy (SP) for the packet */

		sp = ipsec6_check_policy(m, NULL, 0, &needipsec, &error);
		if (error != 0) {
			/*
			 * Hack: -EINVAL is used to signal that a packet
			 * should be silently discarded.  This is typically
			 * because we asked key management for an SA and
			 * it was delayed (e.g. kicked up to IKE).
			 */
			if (error == -EINVAL)
				error = 0;
			goto freecopy;
		}
	}
#endif /* IPSEC */

	if (srcrt) {
		union {
			struct sockaddr		dst;
			struct sockaddr_in6	dst6;
		} u;

		sockaddr_in6_init(&u.dst6, &ip6->ip6_dst, 0, 0, 0);
		if ((rt = rtcache_lookup(&ip6_forward_rt, &u.dst)) == NULL) {
			IP6_STATINC(IP6_STAT_NOROUTE);
			/* XXX in6_ifstat_inc(rt->rt_ifp, ifs6_in_noroute) */
			if (mcopy) {
				icmp6_error(mcopy, ICMP6_DST_UNREACH,
					    ICMP6_DST_UNREACH_NOROUTE, 0);
			}
			m_freem(m);
			return;
		}
	} else if ((rt = rtcache_validate(&ip6_forward_rt)) == NULL &&
	           (rt = rtcache_update(&ip6_forward_rt, 1)) == NULL) {
		/*
		 * rtcache_getdst(ip6_forward_rt)->sin6_addr was equal to
		 * ip6->ip6_dst
		 */
		IP6_STATINC(IP6_STAT_NOROUTE);
		/* XXX in6_ifstat_inc(rt->rt_ifp, ifs6_in_noroute) */
		if (mcopy) {
			icmp6_error(mcopy, ICMP6_DST_UNREACH,
			    ICMP6_DST_UNREACH_NOROUTE, 0);
		}
		m_freem(m);
		return;
	}
	dst = satocsin6(rtcache_getdst(&ip6_forward_rt));

	/*
	 * Source scope check: if a packet can't be delivered to its
	 * destination for the reason that the destination is beyond the scope
	 * of the source address, discard the packet and return an icmp6
	 * destination unreachable error with Code 2 (beyond scope of source
	 * address).  We use a local copy of ip6_src, since in6_setscope()
	 * will possibly modify its first argument.
	 * [draft-ietf-ipngwg-icmp-v3-07, Section 3.1]
	 */
	src_in6 = ip6->ip6_src;
	inzone = outzone = ~0;
	if (in6_setscope(&src_in6, rt->rt_ifp, &outzone) != 0 ||
	    in6_setscope(&src_in6, m->m_pkthdr.rcvif, &inzone) != 0 ||
	    inzone != outzone) {
		ip6_cantforward(ip6, m->m_pkthdr.rcvif, rt->rt_ifp,
		    "src[%s] inzone %d outzone %d", 
		    in6_getscopename(&ip6->ip6_src), inzone, outzone);
		if (mcopy)
			icmp6_error(mcopy, ICMP6_DST_UNREACH,
				    ICMP6_DST_UNREACH_BEYONDSCOPE, 0);
		m_freem(m);
		return;
	}

#ifdef IPSEC
	/*
	 * If we need to encapsulate the packet, do it here
	 * ipsec6_proces_packet will send the packet using ip6_output 
	 */
	if (needipsec) {
		int s = splsoftnet();
		error = ipsec6_process_packet(m, sp->req);
		splx(s);
		if (mcopy)
			goto freecopy;
	}
#endif   

	/*
	 * Destination scope check: if a packet is going to break the scope
	 * zone of packet's destination address, discard it.  This case should
	 * usually be prevented by appropriately-configured routing table, but
	 * we need an explicit check because we may mistakenly forward the
	 * packet to a different zone by (e.g.) a default route.
	 */
	dst_in6 = ip6->ip6_dst;
	inzone = outzone = ~0;
	if (in6_setscope(&dst_in6, m->m_pkthdr.rcvif, &inzone) != 0 ||
	    in6_setscope(&dst_in6, rt->rt_ifp, &outzone) != 0 ||
	    inzone != outzone) {
		ip6_cantforward(ip6, m->m_pkthdr.rcvif, rt->rt_ifp,
		    "dst[%s] inzone %d outzone %d",
		    in6_getscopename(&ip6->ip6_dst), inzone, outzone);
		if (mcopy)
			icmp6_error(mcopy, ICMP6_DST_UNREACH,
				    ICMP6_DST_UNREACH_BEYONDSCOPE, 0);
		m_freem(m);
		return;
	}

	if (m->m_pkthdr.len > IN6_LINKMTU(rt->rt_ifp)) {
		in6_ifstat_inc(rt->rt_ifp, ifs6_in_toobig);
		if (mcopy) {
			u_long mtu;

			mtu = IN6_LINKMTU(rt->rt_ifp);
			icmp6_error(mcopy, ICMP6_PACKET_TOO_BIG, 0, mtu);
		}
		m_freem(m);
		return;
	}

	if (rt->rt_flags & RTF_GATEWAY)
		dst = (struct sockaddr_in6 *)rt->rt_gateway;

	/*
	 * If we are to forward the packet using the same interface
	 * as one we got the packet from, perhaps we should send a redirect
	 * to sender to shortcut a hop.
	 * Only send redirect if source is sending directly to us,
	 * and if packet was not source routed (or has any options).
	 * Also, don't send redirect if forwarding using a route
	 * modified by a redirect.
	 */
	if (rt->rt_ifp == m->m_pkthdr.rcvif && !srcrt && ip6_sendredirects &&
	    (rt->rt_flags & (RTF_DYNAMIC|RTF_MODIFIED)) == 0) {
		if ((rt->rt_ifp->if_flags & IFF_POINTOPOINT) &&
		    nd6_is_addr_neighbor(
		        satocsin6(rtcache_getdst(&ip6_forward_rt)),
			rt->rt_ifp)) {
			/*
			 * If the incoming interface is equal to the outgoing
			 * one, the link attached to the interface is
			 * point-to-point, and the IPv6 destination is
			 * regarded as on-link on the link, then it will be
			 * highly probable that the destination address does
			 * not exist on the link and that the packet is going
			 * to loop.  Thus, we immediately drop the packet and
			 * send an ICMPv6 error message.
			 * For other routing loops, we dare to let the packet
			 * go to the loop, so that a remote diagnosing host
			 * can detect the loop by traceroute.
			 * type/code is based on suggestion by Rich Draves.
			 * not sure if it is the best pick.
			 */
			icmp6_error(mcopy, ICMP6_DST_UNREACH,
				    ICMP6_DST_UNREACH_ADDR, 0);
			m_freem(m);
			return;
		}
		type = ND_REDIRECT;
	}

	/*
	 * Fake scoped addresses. Note that even link-local source or
	 * destinaion can appear, if the originating node just sends the
	 * packet to us (without address resolution for the destination).
	 * Since both icmp6_error and icmp6_redirect_output fill the embedded
	 * link identifiers, we can do this stuff after making a copy for
	 * returning an error.
	 */
	if ((rt->rt_ifp->if_flags & IFF_LOOPBACK) != 0) {
		/*
		 * See corresponding comments in ip6_output.
		 * XXX: but is it possible that ip6_forward() sends a packet
		 *      to a loopback interface? I don't think so, and thus
		 *      I bark here. (jinmei@kame.net)
		 * XXX: it is common to route invalid packets to loopback.
		 *	also, the codepath will be visited on use of ::1 in
		 *	rthdr. (itojun)
		 */
#if 1
		if (0)
#else
		if ((rt->rt_flags & (RTF_BLACKHOLE|RTF_REJECT)) == 0)
#endif
		{
			printf("ip6_forward: outgoing interface is loopback. "
			       "src %s, dst %s, nxt %d, rcvif %s, outif %s\n",
			       ip6_sprintf(&ip6->ip6_src),
			       ip6_sprintf(&ip6->ip6_dst),
			       ip6->ip6_nxt, if_name(m->m_pkthdr.rcvif),
			       if_name(rt->rt_ifp));
		}

		/* we can just use rcvif in forwarding. */
		origifp = m->m_pkthdr.rcvif;
	}
	else
		origifp = rt->rt_ifp;
	/*
	 * clear embedded scope identifiers if necessary.
	 * in6_clearscope will touch the addresses only when necessary.
	 */
	in6_clearscope(&ip6->ip6_src);
	in6_clearscope(&ip6->ip6_dst);

	/*
	 * Run through list of hooks for output packets.
	 */
	if ((error = pfil_run_hooks(inet6_pfil_hook, &m, rt->rt_ifp,
	    PFIL_OUT)) != 0)
		goto senderr;
	if (m == NULL)
		goto freecopy;
	ip6 = mtod(m, struct ip6_hdr *);

	error = nd6_output(rt->rt_ifp, origifp, m, dst, rt);
	if (error) {
		in6_ifstat_inc(rt->rt_ifp, ifs6_out_discard);
		IP6_STATINC(IP6_STAT_CANTFORWARD);
	} else {
		IP6_STATINC(IP6_STAT_FORWARD);
		in6_ifstat_inc(rt->rt_ifp, ifs6_out_forward);
		if (type)
			IP6_STATINC(IP6_STAT_REDIRECTSENT);
		else {
#ifdef GATEWAY
			if (m->m_flags & M_CANFASTFWD)
				ip6flow_create(&ip6_forward_rt, m);
#endif
			if (mcopy)
				goto freecopy;
		}
	}

 senderr:
	if (mcopy == NULL)
		return;
	switch (error) {
	case 0:
		if (type == ND_REDIRECT) {
			icmp6_redirect_output(mcopy, rt);
			return;
		}
		goto freecopy;

	case EMSGSIZE:
		/* xxx MTU is constant in PPP? */
		goto freecopy;

	case ENOBUFS:
		/* Tell source to slow down like source quench in IP? */
		goto freecopy;

	case ENETUNREACH:	/* shouldn't happen, checked above */
	case EHOSTUNREACH:
	case ENETDOWN:
	case EHOSTDOWN:
	default:
		type = ICMP6_DST_UNREACH;
		code = ICMP6_DST_UNREACH_ADDR;
		break;
	}
	icmp6_error(mcopy, type, code, 0);
	return;

 freecopy:
	m_freem(mcopy);
	return;
}
