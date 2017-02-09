/*	$NetBSD: ip6_flow.c,v 1.24 2015/03/23 18:33:17 roy Exp $	*/

/*-
 * Copyright (c) 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by the 3am Software Foundry ("3am").  It was developed by Liam J. Foy
 * <liamjfoy@netbsd.org> and Matt Thomas <matt@netbsd.org>.
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
 *
 * IPv6 version was developed by Liam J. Foy. Original source existed in IPv4
 * format developed by Matt Thomas. Thanks to Joerg Sonnenberger, Matt
 * Thomas and Christos Zoulas. 
 *
 * Thanks to Liverpool John Moores University, especially Dr. David Llewellyn-Jones
 * for providing resources (to test) and Professor Madjid Merabti.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ip6_flow.c,v 1.24 2015/03/23 18:33:17 roy Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/pool.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/pfil.h>

#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/ip6_private.h>

/*
 * IPv6 Fast Forward caches/hashes flows from one source to destination.
 *
 * Upon a successful forward IPv6FF caches and hashes details such as the
 * route, source and destination. Once another packet is received matching
 * the source and destination the packet is forwarded straight onto if_output
 * using the cached details.
 *
 * Example:
 * ether/fddi_input -> ip6flow_fastforward -> if_output
 */

static struct pool ip6flow_pool;

LIST_HEAD(ip6flowhead, ip6flow);

/*
 * We could use IPv4 defines (IPFLOW_HASHBITS) but we'll
 * use our own (possibly for future expansion).
 */
#define	IP6FLOW_TIMER		(5 * PR_SLOWHZ)
#define	IP6FLOW_DEFAULT_HASHSIZE	(1 << IP6FLOW_HASHBITS) 

static struct ip6flowhead *ip6flowtable = NULL;
static struct ip6flowhead ip6flowlist;
static int ip6flow_inuse;

/*
 * Insert an ip6flow into the list.
 */
#define	IP6FLOW_INSERT(bucket, ip6f) \
do { \
	LIST_INSERT_HEAD((bucket), (ip6f), ip6f_hash); \
	LIST_INSERT_HEAD(&ip6flowlist, (ip6f), ip6f_list); \
} while (/*CONSTCOND*/ 0)

/*
 * Remove an ip6flow from the list.
 */
#define	IP6FLOW_REMOVE(ip6f) \
do { \
	LIST_REMOVE((ip6f), ip6f_hash); \
	LIST_REMOVE((ip6f), ip6f_list); \
} while (/*CONSTCOND*/ 0)

#ifndef IP6FLOW_DEFAULT
#define	IP6FLOW_DEFAULT		256
#endif

int ip6_maxflows = IP6FLOW_DEFAULT;
int ip6_hashsize = IP6FLOW_DEFAULT_HASHSIZE;

/*
 * Calculate hash table position.
 */
static size_t 
ip6flow_hash(const struct ip6_hdr *ip6)
{
	size_t hash;
	uint32_t dst_sum, src_sum;
	size_t idx;

	src_sum = ip6->ip6_src.s6_addr32[0] + ip6->ip6_src.s6_addr32[1]
	    + ip6->ip6_src.s6_addr32[2] + ip6->ip6_src.s6_addr32[3];
	dst_sum = ip6->ip6_dst.s6_addr32[0] + ip6->ip6_dst.s6_addr32[1]
	    + ip6->ip6_dst.s6_addr32[2] + ip6->ip6_dst.s6_addr32[3];

	hash = ip6->ip6_flow;

	for (idx = 0; idx < 32; idx += IP6FLOW_HASHBITS)
		hash += (dst_sum >> (32 - idx)) + (src_sum >> idx);

	return hash & (ip6_hashsize-1);
}

/*
 * Check to see if a flow already exists - if so return it.
 */
static struct ip6flow *
ip6flow_lookup(const struct ip6_hdr *ip6)
{
	size_t hash;
	struct ip6flow *ip6f;

	hash = ip6flow_hash(ip6);

	LIST_FOREACH(ip6f, &ip6flowtable[hash], ip6f_hash) {
		if (IN6_ARE_ADDR_EQUAL(&ip6->ip6_dst, &ip6f->ip6f_dst)
		    && IN6_ARE_ADDR_EQUAL(&ip6->ip6_src, &ip6f->ip6f_src)
		    && ip6f->ip6f_flow == ip6->ip6_flow) {
		    	/* A cached flow has been found. */
			return ip6f;
		}
	}

	return NULL;
}

void
ip6flow_poolinit(void)
{

	pool_init(&ip6flow_pool, sizeof(struct ip6flow), 0, 0, 0, "ip6flowpl",
			NULL, IPL_NET);
}

/*
 * Allocate memory and initialise lists. This function is called
 * from ip6_init and called there after to resize the hash table.
 * If a newly sized table cannot be malloc'ed we just continue
 * to use the old one.
 */
int
ip6flow_init(int table_size)
{
	struct ip6flowhead *new_table;
	size_t i;

	new_table = (struct ip6flowhead *)malloc(sizeof(struct ip6flowhead) *
	    table_size, M_RTABLE, M_NOWAIT);

	if (new_table == NULL)
		return 1;

	if (ip6flowtable != NULL)
		free(ip6flowtable, M_RTABLE);

	ip6flowtable = new_table;
	ip6_hashsize = table_size;

	LIST_INIT(&ip6flowlist);
	for (i = 0; i < ip6_hashsize; i++)
		LIST_INIT(&ip6flowtable[i]);

	return 0;
}

/*
 * IPv6 Fast Forward routine. Attempt to forward the packet -
 * if any problems are found return to the main IPv6 input 
 * routine to deal with.
 */
int
ip6flow_fastforward(struct mbuf **mp)
{
	struct ip6flow *ip6f;
	struct ip6_hdr *ip6;
	struct rtentry *rt;
	struct mbuf *m;
	const struct sockaddr *dst;
	int error;

	/*
	 * Are we forwarding packets and have flows?
	 */
	if (!ip6_forwarding || ip6flow_inuse == 0)
		return 0;

	m = *mp;
	/*
	 * At least size of IPv6 Header?
	 */
	if (m->m_len < sizeof(struct ip6_hdr))
		return 0;	
	/*
	 * Was packet received as a link-level multicast or broadcast?
	 * If so, don't try to fast forward.
	 */
	if ((m->m_flags & (M_BCAST|M_MCAST)) != 0)
		return 0;

	if (IP6_HDR_ALIGNED_P(mtod(m, const void *)) == 0) {
		if ((m = m_copyup(m, sizeof(struct ip6_hdr),
				(max_linkhdr + 3) & ~3)) == NULL) {
			return 0;
		}
		*mp = m;
	} else if (__predict_false(m->m_len < sizeof(struct ip6_hdr))) {
		if ((m = m_pullup(m, sizeof(struct ip6_hdr))) == NULL) {
			return 0;
		}
		*mp = m;
	}

	ip6 = mtod(m, struct ip6_hdr *);

	if ((ip6->ip6_vfc & IPV6_VERSION_MASK) != IPV6_VERSION) {
		/* Bad version. */
		return 0;
	}

	/*
	 * If we have a hop-by-hop extension we must process it.
	 * We just leave this up to ip6_input to deal with. 
	 */
	if (ip6->ip6_nxt == IPPROTO_HOPOPTS)
		return 0;

	/*
	 * Attempt to find a flow.
	 */
	if ((ip6f = ip6flow_lookup(ip6)) == NULL) {
		/* No flow found. */
		return 0;
	}

	/*
	 * Route and interface still up?
	 */
	if ((rt = rtcache_validate(&ip6f->ip6f_ro)) == NULL ||
	    (rt->rt_ifp->if_flags & IFF_UP) == 0 ||
	    (rt->rt_flags & RTF_BLACKHOLE) != 0)
		return 0;

	/*
	 * Packet size greater than MTU?
	 */
	if (m->m_pkthdr.len > rt->rt_ifp->if_mtu) {
		/* Return to main IPv6 input function. */
		return 0;
	}

	/*
	 * Clear any in-bound checksum flags for this packet.
	 */
	m->m_pkthdr.csum_flags = 0;

	if (ip6->ip6_hlim <= IPV6_HLIMDEC)
		return 0;

	/* Decrement hop limit (same as TTL) */
	ip6->ip6_hlim -= IPV6_HLIMDEC;

	if (rt->rt_flags & RTF_GATEWAY)
		dst = rt->rt_gateway;
	else
		dst = rtcache_getdst(&ip6f->ip6f_ro);

	PRT_SLOW_ARM(ip6f->ip6f_timer, IP6FLOW_TIMER);

	ip6f->ip6f_uses++;

	KERNEL_LOCK(1, NULL);
	/* Send on its way - straight to the interface output routine. */
	if ((error = (*rt->rt_ifp->if_output)(rt->rt_ifp, m, dst, rt)) != 0) {
		ip6f->ip6f_dropped++;
	} else {
		ip6f->ip6f_forwarded++;
	}
	KERNEL_UNLOCK_ONE(NULL);
	return 1;
}

/*
 * Add the IPv6 flow statistics to the main IPv6 statistics.
 */
static void
ip6flow_addstats(const struct ip6flow *ip6f)
{
	struct rtentry *rt;
	uint64_t *ip6s;

	if ((rt = rtcache_validate(&ip6f->ip6f_ro)) != NULL)
		rt->rt_use += ip6f->ip6f_uses;
	ip6s = IP6_STAT_GETREF();
	ip6s[IP6_STAT_FASTFORWARDFLOWS] = ip6flow_inuse;
	ip6s[IP6_STAT_CANTFORWARD] += ip6f->ip6f_dropped;
	ip6s[IP6_STAT_ODROPPED] += ip6f->ip6f_dropped;
	ip6s[IP6_STAT_TOTAL] += ip6f->ip6f_uses;
	ip6s[IP6_STAT_FORWARD] += ip6f->ip6f_forwarded;
	ip6s[IP6_STAT_FASTFORWARD] += ip6f->ip6f_forwarded;
	IP6_STAT_PUTREF();
}

/*
 * Add statistics and free the flow.
 */
static void
ip6flow_free(struct ip6flow *ip6f)
{
	int s;

	/*
	 * Remove the flow from the hash table (at elevated IPL).
	 * Once it's off the list, we can deal with it at normal
	 * network IPL.
	 */
	s = splnet();
	IP6FLOW_REMOVE(ip6f);
	splx(s);
	ip6flow_inuse--;
	ip6flow_addstats(ip6f);
	rtcache_free(&ip6f->ip6f_ro);
	pool_put(&ip6flow_pool, ip6f);
}

/*
 * Reap one or more flows - ip6flow_reap may remove
 * multiple flows if net.inet6.ip6.maxflows is reduced. 
 */
struct ip6flow *
ip6flow_reap(int just_one)
{
	while (just_one || ip6flow_inuse > ip6_maxflows) {
		struct ip6flow *ip6f, *maybe_ip6f = NULL;
		int s;

		ip6f = LIST_FIRST(&ip6flowlist);
		while (ip6f != NULL) {
			/*
			 * If this no longer points to a valid route -
			 * reclaim it.
			 */
			if (rtcache_validate(&ip6f->ip6f_ro) == NULL)
				goto done;
			/*
			 * choose the one that's been least recently
			 * used or has had the least uses in the
			 * last 1.5 intervals.
			 */
			if (maybe_ip6f == NULL ||
			    ip6f->ip6f_timer < maybe_ip6f->ip6f_timer ||
			    (ip6f->ip6f_timer == maybe_ip6f->ip6f_timer &&
			     ip6f->ip6f_last_uses + ip6f->ip6f_uses <
			         maybe_ip6f->ip6f_last_uses +
			         maybe_ip6f->ip6f_uses))
				maybe_ip6f = ip6f;
			ip6f = LIST_NEXT(ip6f, ip6f_list);
		}
		ip6f = maybe_ip6f;
	    done:
		/*
		 * Remove the entry from the flow table
		 */
		s = splnet();
		IP6FLOW_REMOVE(ip6f);
		splx(s);
		rtcache_free(&ip6f->ip6f_ro);
		if (just_one) {
			ip6flow_addstats(ip6f);
			return ip6f;
		}
		ip6flow_inuse--;
		ip6flow_addstats(ip6f);
		pool_put(&ip6flow_pool, ip6f);
	}
	return NULL;
}

void
ip6flow_slowtimo(void)
{
	struct ip6flow *ip6f, *next_ip6f;

	mutex_enter(softnet_lock);
	KERNEL_LOCK(1, NULL);

	for (ip6f = LIST_FIRST(&ip6flowlist); ip6f != NULL; ip6f = next_ip6f) {
		next_ip6f = LIST_NEXT(ip6f, ip6f_list);
		if (PRT_SLOW_ISEXPIRED(ip6f->ip6f_timer) ||
		    rtcache_validate(&ip6f->ip6f_ro) == NULL) {
			ip6flow_free(ip6f);
		} else {
			ip6f->ip6f_last_uses = ip6f->ip6f_uses;
			ip6flow_addstats(ip6f);
			ip6f->ip6f_uses = 0;
			ip6f->ip6f_dropped = 0;
			ip6f->ip6f_forwarded = 0;
		}
	}

	KERNEL_UNLOCK_ONE(NULL);
	mutex_exit(softnet_lock);
}

/*
 * We have successfully forwarded a packet using the normal
 * IPv6 stack. Now create/update a flow.
 */
void
ip6flow_create(const struct route *ro, struct mbuf *m)
{
	const struct ip6_hdr *ip6;
	struct ip6flow *ip6f;
	size_t hash;
	int s;

	ip6 = mtod(m, const struct ip6_hdr *);

	/*
	 * If IPv6 Fast Forward is disabled, don't create a flow.
	 * It can be disabled by setting net.inet6.ip6.maxflows to 0.
	 *
	 * Don't create a flow for ICMPv6 messages.
	 */
	if (ip6_maxflows == 0 || ip6->ip6_nxt == IPPROTO_IPV6_ICMP)
		return;

	KERNEL_LOCK(1, NULL);

	/*
	 * See if an existing flow exists.  If so:
	 *	- Remove the flow
	 *	- Add flow statistics
	 *	- Free the route
	 *	- Reset statistics
	 *
	 * If a flow doesn't exist allocate a new one if
	 * ip6_maxflows hasn't reached its limit. If it has
	 * been reached, reap some flows.
	 */
	ip6f = ip6flow_lookup(ip6);
	if (ip6f == NULL) {
		if (ip6flow_inuse >= ip6_maxflows) {
			ip6f = ip6flow_reap(1);
		} else {
			ip6f = pool_get(&ip6flow_pool, PR_NOWAIT);
			if (ip6f == NULL)
				goto out;
			ip6flow_inuse++;
		}
		memset(ip6f, 0, sizeof(*ip6f));
	} else {
		s = splnet();
		IP6FLOW_REMOVE(ip6f);
		splx(s);
		ip6flow_addstats(ip6f);
		rtcache_free(&ip6f->ip6f_ro);
		ip6f->ip6f_uses = 0;
		ip6f->ip6f_last_uses = 0;
		ip6f->ip6f_dropped = 0;
		ip6f->ip6f_forwarded = 0;
	}

	/*
	 * Fill in the updated/new details.
	 */
	rtcache_copy(&ip6f->ip6f_ro, ro);
	ip6f->ip6f_dst = ip6->ip6_dst;
	ip6f->ip6f_src = ip6->ip6_src;
	ip6f->ip6f_flow = ip6->ip6_flow;
	PRT_SLOW_ARM(ip6f->ip6f_timer, IP6FLOW_TIMER);

	/*
	 * Insert into the approriate bucket of the flow table.
	 */
	hash = ip6flow_hash(ip6);
	s = splnet();
	IP6FLOW_INSERT(&ip6flowtable[hash], ip6f);
	splx(s);

 out:
	KERNEL_UNLOCK_ONE(NULL);
}

/*
 * Invalidate/remove all flows - if new_size is positive we
 * resize the hash table.
 */
int
ip6flow_invalidate_all(int new_size)
{
	struct ip6flow *ip6f, *next_ip6f;
	int s, error;

	error = 0;
	s = splnet();
	for (ip6f = LIST_FIRST(&ip6flowlist); ip6f != NULL; ip6f = next_ip6f) {
		next_ip6f = LIST_NEXT(ip6f, ip6f_list);
		ip6flow_free(ip6f);
	}

	if (new_size) 
		error = ip6flow_init(new_size);
	splx(s);

	return error;
}
