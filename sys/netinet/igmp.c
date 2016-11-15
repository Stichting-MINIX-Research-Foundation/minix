/*	$NetBSD: igmp.c,v 1.56 2015/08/24 22:21:26 pooka Exp $	*/

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
 * Internet Group Management Protocol (IGMP) routines.
 *
 * Written by Steve Deering, Stanford, May 1988.
 * Modified by Rosen Sharma, Stanford, Aug 1994.
 * Modified by Bill Fenner, Xerox PARC, Feb 1995.
 *
 * MULTICAST Revision: 1.3
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: igmp.c,v 1.56 2015/08/24 22:21:26 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_mrouting.h"
#endif

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/systm.h>
#include <sys/cprng.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/route.h>
#include <net/net_stats.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/igmp.h>
#include <netinet/igmp_var.h>

/*
 * Per-interface router version information.
 */
typedef struct router_info {
	LIST_ENTRY(router_info) rti_link;
	ifnet_t *	rti_ifp;
	int		rti_type;	/* type of router on this interface */
	int		rti_age;	/* time since last v1 query */
} router_info_t;

/*
 * The router-info list and the timer flag are protected by in_multilock.
 *
 * Lock order:
 *
 *	softnet_lock ->
 *		in_multilock
 */
static struct pool	igmp_rti_pool		__cacheline_aligned;
static LIST_HEAD(, router_info)	rti_head	__cacheline_aligned;
static int		igmp_timers_on		__cacheline_aligned;
static percpu_t *	igmpstat_percpu		__read_mostly;

#define	IGMP_STATINC(x)		_NET_STATINC(igmpstat_percpu, x)

static void		igmp_sendpkt(struct in_multi *, int);
static int		rti_fill(struct in_multi *);
static router_info_t *	rti_find(struct ifnet *);
static void		rti_delete(struct ifnet *);
static void		sysctl_net_inet_igmp_setup(struct sysctllog **);

/*
 * rti_fill: associate router information with the given multicast group;
 * if there is no router information for the interface, then create it.
 */
static int
rti_fill(struct in_multi *inm)
{
	router_info_t *rti;

	KASSERT(in_multi_lock_held());

	LIST_FOREACH(rti, &rti_head, rti_link) {
		if (rti->rti_ifp == inm->inm_ifp) {
			inm->inm_rti = rti;
			return rti->rti_type == IGMP_v1_ROUTER ?
			    IGMP_v1_HOST_MEMBERSHIP_REPORT :
			    IGMP_v2_HOST_MEMBERSHIP_REPORT;
		}
	}
	rti = pool_get(&igmp_rti_pool, PR_NOWAIT);
	if (rti == NULL) {
		return 0;
	}
	rti->rti_ifp = inm->inm_ifp;
	rti->rti_type = IGMP_v2_ROUTER;
	LIST_INSERT_HEAD(&rti_head, rti, rti_link);
	inm->inm_rti = rti;
	return IGMP_v2_HOST_MEMBERSHIP_REPORT;
}

/*
 * rti_find: lookup or create router information for the given interface.
 */
static router_info_t *
rti_find(ifnet_t *ifp)
{
	router_info_t *rti;

	KASSERT(in_multi_lock_held());

	LIST_FOREACH(rti, &rti_head, rti_link) {
		if (rti->rti_ifp == ifp)
			return rti;
	}
	rti = pool_get(&igmp_rti_pool, PR_NOWAIT);
	if (rti == NULL) {
		return NULL;
	}
	rti->rti_ifp = ifp;
	rti->rti_type = IGMP_v2_ROUTER;
	LIST_INSERT_HEAD(&rti_head, rti, rti_link);
	return rti;
}

/*
 * rti_delete: remove and free the router information entry for the
 * given interface.
 */
static void
rti_delete(ifnet_t *ifp)
{
	router_info_t *rti;

	KASSERT(in_multi_lock_held());

	LIST_FOREACH(rti, &rti_head, rti_link) {
		if (rti->rti_ifp == ifp) {
			LIST_REMOVE(rti, rti_link);
			pool_put(&igmp_rti_pool, rti);
			break;
		}
	}
}

void
igmp_init(void)
{
	pool_init(&igmp_rti_pool, sizeof(router_info_t), 0, 0, 0,
	    "igmppl", NULL, IPL_SOFTNET);
	igmpstat_percpu = percpu_alloc(sizeof(uint64_t) * IGMP_NSTATS);
	sysctl_net_inet_igmp_setup(NULL);
	LIST_INIT(&rti_head);
}

void
igmp_input(struct mbuf *m, ...)
{
	ifnet_t *ifp = m->m_pkthdr.rcvif;
	struct ip *ip = mtod(m, struct ip *);
	struct igmp *igmp;
	u_int minlen, timer;
	struct in_multi *inm;
	struct in_ifaddr *ia;
	int proto, ip_len, iphlen;
	va_list ap;

	va_start(ap, m);
	iphlen = va_arg(ap, int);
	proto = va_arg(ap, int);
	va_end(ap);

	IGMP_STATINC(IGMP_STAT_RCV_TOTAL);

	/*
	 * Validate lengths
	 */
	minlen = iphlen + IGMP_MINLEN;
	ip_len = ntohs(ip->ip_len);
	if (ip_len < minlen) {
		IGMP_STATINC(IGMP_STAT_RCV_TOOSHORT);
		m_freem(m);
		return;
	}
	if (((m->m_flags & M_EXT) && (ip->ip_src.s_addr & IN_CLASSA_NET) == 0)
	    || m->m_len < minlen) {
		if ((m = m_pullup(m, minlen)) == NULL) {
			IGMP_STATINC(IGMP_STAT_RCV_TOOSHORT);
			return;
		}
		ip = mtod(m, struct ip *);
	}

	/*
	 * Validate checksum
	 */
	m->m_data += iphlen;
	m->m_len -= iphlen;
	igmp = mtod(m, struct igmp *);
	/* No need to assert alignment here. */
	if (in_cksum(m, ip_len - iphlen)) {
		IGMP_STATINC(IGMP_STAT_RCV_BADSUM);
		m_freem(m);
		return;
	}
	m->m_data -= iphlen;
	m->m_len += iphlen;

	switch (igmp->igmp_type) {

	case IGMP_HOST_MEMBERSHIP_QUERY:
		IGMP_STATINC(IGMP_STAT_RCV_QUERIES);

		if (ifp->if_flags & IFF_LOOPBACK)
			break;

		if (igmp->igmp_code == 0) {
			struct in_multistep step;
			router_info_t *rti;

			if (ip->ip_dst.s_addr != INADDR_ALLHOSTS_GROUP) {
				IGMP_STATINC(IGMP_STAT_RCV_BADQUERIES);
				m_freem(m);
				return;
			}

			in_multi_lock(RW_WRITER);
			rti = rti_find(ifp);
			if (rti == NULL) {
				in_multi_unlock();
				break;
			}
			rti->rti_type = IGMP_v1_ROUTER;
			rti->rti_age = 0;

			/*
			 * Start the timers in all of our membership records
			 * for the interface on which the query arrived,
			 * except those that are already running and those
			 * that belong to a "local" group (224.0.0.X).
			 */

			inm = in_first_multi(&step);
			while (inm != NULL) {
				if (inm->inm_ifp == ifp &&
				    inm->inm_timer == 0 &&
				    !IN_LOCAL_GROUP(inm->inm_addr.s_addr)) {
					inm->inm_state = IGMP_DELAYING_MEMBER;
					inm->inm_timer = IGMP_RANDOM_DELAY(
					    IGMP_MAX_HOST_REPORT_DELAY * PR_FASTHZ);
					igmp_timers_on = true;
				}
				inm = in_next_multi(&step);
			}
			in_multi_unlock();
		} else {
			struct in_multistep step;

			if (!IN_MULTICAST(ip->ip_dst.s_addr)) {
				IGMP_STATINC(IGMP_STAT_RCV_BADQUERIES);
				m_freem(m);
				return;
			}

			timer = igmp->igmp_code * PR_FASTHZ / IGMP_TIMER_SCALE;
			if (timer == 0)
				timer = 1;

			/*
			 * Start the timers in all of our membership records
			 * for the interface on which the query arrived,
			 * except those that are already running and those
			 * that belong to a "local" group (224.0.0.X).  For
			 * timers already running, check if they need to be
			 * reset.
			 */
			in_multi_lock(RW_WRITER);
			inm = in_first_multi(&step);
			while (inm != NULL) {
				if (inm->inm_ifp == ifp &&
				    !IN_LOCAL_GROUP(inm->inm_addr.s_addr) &&
				    (ip->ip_dst.s_addr == INADDR_ALLHOSTS_GROUP ||
				     in_hosteq(ip->ip_dst, inm->inm_addr))) {
					switch (inm->inm_state) {
					case IGMP_DELAYING_MEMBER:
						if (inm->inm_timer <= timer)
							break;
						/* FALLTHROUGH */
					case IGMP_IDLE_MEMBER:
					case IGMP_LAZY_MEMBER:
					case IGMP_AWAKENING_MEMBER:
						inm->inm_state =
						    IGMP_DELAYING_MEMBER;
						inm->inm_timer =
						    IGMP_RANDOM_DELAY(timer);
						igmp_timers_on = true;
						break;
					case IGMP_SLEEPING_MEMBER:
						inm->inm_state =
						    IGMP_AWAKENING_MEMBER;
						break;
					}
				}
				inm = in_next_multi(&step);
			}
			in_multi_unlock();
		}

		break;

	case IGMP_v1_HOST_MEMBERSHIP_REPORT:
		IGMP_STATINC(IGMP_STAT_RCV_REPORTS);

		if (ifp->if_flags & IFF_LOOPBACK)
			break;

		if (!IN_MULTICAST(igmp->igmp_group.s_addr) ||
		    !in_hosteq(igmp->igmp_group, ip->ip_dst)) {
			IGMP_STATINC(IGMP_STAT_RCV_BADREPORTS);
			m_freem(m);
			return;
		}

		/*
		 * KLUDGE: if the IP source address of the report has an
		 * unspecified (i.e., zero) subnet number, as is allowed for
		 * a booting host, replace it with the correct subnet number
		 * so that a process-level multicast routing daemon can
		 * determine which subnet it arrived from.  This is necessary
		 * to compensate for the lack of any way for a process to
		 * determine the arrival interface of an incoming packet.
		 */
		if ((ip->ip_src.s_addr & IN_CLASSA_NET) == 0) {
			IFP_TO_IA(ifp, ia);		/* XXX */
			if (ia)
				ip->ip_src.s_addr = ia->ia_subnet;
		}

		/*
		 * If we belong to the group being reported, stop
		 * our timer for that group.
		 */
		in_multi_lock(RW_WRITER);
		inm = in_lookup_multi(igmp->igmp_group, ifp);
		if (inm != NULL) {
			inm->inm_timer = 0;
			IGMP_STATINC(IGMP_STAT_RCV_OURREPORTS);

			switch (inm->inm_state) {
			case IGMP_IDLE_MEMBER:
			case IGMP_LAZY_MEMBER:
			case IGMP_AWAKENING_MEMBER:
			case IGMP_SLEEPING_MEMBER:
				inm->inm_state = IGMP_SLEEPING_MEMBER;
				break;
			case IGMP_DELAYING_MEMBER:
				if (inm->inm_rti->rti_type == IGMP_v1_ROUTER)
					inm->inm_state = IGMP_LAZY_MEMBER;
				else
					inm->inm_state = IGMP_SLEEPING_MEMBER;
				break;
			}
		}
		in_multi_unlock();
		break;

	case IGMP_v2_HOST_MEMBERSHIP_REPORT:
#ifdef MROUTING
		/*
		 * Make sure we don't hear our own membership report.  Fast
		 * leave requires knowing that we are the only member of a
		 * group.
		 */
		IFP_TO_IA(ifp, ia);			/* XXX */
		if (ia && in_hosteq(ip->ip_src, ia->ia_addr.sin_addr))
			break;
#endif

		IGMP_STATINC(IGMP_STAT_RCV_REPORTS);

		if (ifp->if_flags & IFF_LOOPBACK)
			break;

		if (!IN_MULTICAST(igmp->igmp_group.s_addr) ||
		    !in_hosteq(igmp->igmp_group, ip->ip_dst)) {
			IGMP_STATINC(IGMP_STAT_RCV_BADREPORTS);
			m_freem(m);
			return;
		}

		/*
		 * KLUDGE: if the IP source address of the report has an
		 * unspecified (i.e., zero) subnet number, as is allowed for
		 * a booting host, replace it with the correct subnet number
		 * so that a process-level multicast routing daemon can
		 * determine which subnet it arrived from.  This is necessary
		 * to compensate for the lack of any way for a process to
		 * determine the arrival interface of an incoming packet.
		 */
		if ((ip->ip_src.s_addr & IN_CLASSA_NET) == 0) {
#ifndef MROUTING
			IFP_TO_IA(ifp, ia);		/* XXX */
#endif
			if (ia)
				ip->ip_src.s_addr = ia->ia_subnet;
		}

		/*
		 * If we belong to the group being reported, stop
		 * our timer for that group.
		 */
		in_multi_lock(RW_WRITER);
		inm = in_lookup_multi(igmp->igmp_group, ifp);
		if (inm != NULL) {
			inm->inm_timer = 0;
			IGMP_STATINC(IGMP_STAT_RCV_OURREPORTS);

			switch (inm->inm_state) {
			case IGMP_DELAYING_MEMBER:
			case IGMP_IDLE_MEMBER:
			case IGMP_AWAKENING_MEMBER:
				inm->inm_state = IGMP_LAZY_MEMBER;
				break;
			case IGMP_LAZY_MEMBER:
			case IGMP_SLEEPING_MEMBER:
				break;
			}
		}
		in_multi_unlock();
		break;

	}

	/*
	 * Pass all valid IGMP packets up to any process(es) listening
	 * on a raw IGMP socket.
	 */
	rip_input(m, iphlen, proto);
	return;
}

int
igmp_joingroup(struct in_multi *inm)
{
	KASSERT(in_multi_lock_held());
	inm->inm_state = IGMP_IDLE_MEMBER;

	if (!IN_LOCAL_GROUP(inm->inm_addr.s_addr) &&
	    (inm->inm_ifp->if_flags & IFF_LOOPBACK) == 0) {
		int report_type;

		report_type = rti_fill(inm);
		if (report_type == 0) {
			return ENOMEM;
		}
		igmp_sendpkt(inm, report_type);
		inm->inm_state = IGMP_DELAYING_MEMBER;
		inm->inm_timer = IGMP_RANDOM_DELAY(
		    IGMP_MAX_HOST_REPORT_DELAY * PR_FASTHZ);
		igmp_timers_on = true;
	} else
		inm->inm_timer = 0;

	return 0;
}

void
igmp_leavegroup(struct in_multi *inm)
{
	KASSERT(in_multi_lock_held());

	switch (inm->inm_state) {
	case IGMP_DELAYING_MEMBER:
	case IGMP_IDLE_MEMBER:
		if (!IN_LOCAL_GROUP(inm->inm_addr.s_addr) &&
		    (inm->inm_ifp->if_flags & IFF_LOOPBACK) == 0)
			if (inm->inm_rti->rti_type != IGMP_v1_ROUTER)
				igmp_sendpkt(inm, IGMP_HOST_LEAVE_MESSAGE);
		break;
	case IGMP_LAZY_MEMBER:
	case IGMP_AWAKENING_MEMBER:
	case IGMP_SLEEPING_MEMBER:
		break;
	}
}

void
igmp_fasttimo(void)
{
	struct in_multi *inm;
	struct in_multistep step;

	/*
	 * Quick check to see if any work needs to be done, in order
	 * to minimize the overhead of fasttimo processing.
	 */
	if (!igmp_timers_on) {
		return;
	}

	/* XXX: Needed for ip_output(). */
	mutex_enter(softnet_lock);

	in_multi_lock(RW_WRITER);
	igmp_timers_on = false;
	inm = in_first_multi(&step);
	while (inm != NULL) {
		if (inm->inm_timer == 0) {
			/* do nothing */
		} else if (--inm->inm_timer == 0) {
			if (inm->inm_state == IGMP_DELAYING_MEMBER) {
				if (inm->inm_rti->rti_type == IGMP_v1_ROUTER)
					igmp_sendpkt(inm,
					    IGMP_v1_HOST_MEMBERSHIP_REPORT);
				else
					igmp_sendpkt(inm,
					    IGMP_v2_HOST_MEMBERSHIP_REPORT);
				inm->inm_state = IGMP_IDLE_MEMBER;
			}
		} else {
			igmp_timers_on = true;
		}
		inm = in_next_multi(&step);
	}
	in_multi_unlock();
	mutex_exit(softnet_lock);
}

void
igmp_slowtimo(void)
{
	router_info_t *rti;

	in_multi_lock(RW_WRITER);
	LIST_FOREACH(rti, &rti_head, rti_link) {
		if (rti->rti_type == IGMP_v1_ROUTER &&
		    ++rti->rti_age >= IGMP_AGE_THRESHOLD) {
			rti->rti_type = IGMP_v2_ROUTER;
		}
	}
	in_multi_unlock();
}

/*
 * igmp_sendpkt: construct an IGMP packet, given the multicast structure
 * and the type, and send the datagram.
 */
static void
igmp_sendpkt(struct in_multi *inm, int type)
{
	struct mbuf *m;
	struct igmp *igmp;
	struct ip *ip;
	struct ip_moptions imo;

	KASSERT(in_multi_lock_held());

	MGETHDR(m, M_DONTWAIT, MT_HEADER);
	if (m == NULL)
		return;

	/*
	 * Assume max_linkhdr + sizeof(struct ip) + IGMP_MINLEN
	 * is smaller than mbuf size returned by MGETHDR.
	 */
	m->m_data += max_linkhdr;
	m->m_len = sizeof(struct ip) + IGMP_MINLEN;
	m->m_pkthdr.len = sizeof(struct ip) + IGMP_MINLEN;

	ip = mtod(m, struct ip *);
	ip->ip_tos = 0;
	ip->ip_len = htons(sizeof(struct ip) + IGMP_MINLEN);
	ip->ip_off = htons(0);
	ip->ip_p = IPPROTO_IGMP;
	ip->ip_src = zeroin_addr;
	ip->ip_dst = inm->inm_addr;

	m->m_data += sizeof(struct ip);
	m->m_len -= sizeof(struct ip);
	igmp = mtod(m, struct igmp *);
	igmp->igmp_type = type;
	igmp->igmp_code = 0;
	igmp->igmp_group = inm->inm_addr;
	igmp->igmp_cksum = 0;
	igmp->igmp_cksum = in_cksum(m, IGMP_MINLEN);
	m->m_data -= sizeof(struct ip);
	m->m_len += sizeof(struct ip);

	imo.imo_multicast_ifp = inm->inm_ifp;
	imo.imo_multicast_ttl = 1;
#ifdef RSVP_ISI
	imo.imo_multicast_vif = -1;
#endif
	/*
	 * Request loopback of the report if we are acting as a multicast
	 * router, so that the process-level routing demon can hear it.
	 */
#ifdef MROUTING
	extern struct socket *ip_mrouter;
	imo.imo_multicast_loop = (ip_mrouter != NULL);
#else
	imo.imo_multicast_loop = 0;
#endif

	/*
	 * Note: IP_IGMP_MCAST indicates that in_multilock is held.
	 * The caller must still acquire softnet_lock for ip_output().
	 */
	KASSERT(mutex_owned(softnet_lock));
	ip_output(m, NULL, NULL, IP_IGMP_MCAST, &imo, NULL);
	IGMP_STATINC(IGMP_STAT_SND_REPORTS);
}

void
igmp_purgeif(ifnet_t *ifp)
{
	in_multi_lock(RW_WRITER);
	rti_delete(ifp);
	in_multi_unlock();
}

static int
sysctl_net_inet_igmp_stats(SYSCTLFN_ARGS)
{
	return NETSTAT_SYSCTL(igmpstat_percpu, IGMP_NSTATS);
}

static void
sysctl_net_inet_igmp_setup(struct sysctllog **clog)
{
	sysctl_createv(clog, 0, NULL, NULL,
			CTLFLAG_PERMANENT,
			CTLTYPE_NODE, "inet", NULL,
			NULL, 0, NULL, 0,
			CTL_NET, PF_INET, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
			CTLFLAG_PERMANENT,
			CTLTYPE_NODE, "igmp",
			SYSCTL_DESCR("Internet Group Management Protocol"),
			NULL, 0, NULL, 0,
			CTL_NET, PF_INET, IPPROTO_IGMP, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
			CTLFLAG_PERMANENT,
			CTLTYPE_STRUCT, "stats",
			SYSCTL_DESCR("IGMP statistics"),
			sysctl_net_inet_igmp_stats, 0, NULL, 0,
			CTL_NET, PF_INET, IPPROTO_IGMP, CTL_CREATE, CTL_EOL);
}
