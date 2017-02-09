/*	$NetBSD: nd6_rtr.c,v 1.104 2015/10/05 04:15:42 ozaki-r Exp $	*/
/*	$KAME: nd6_rtr.c,v 1.95 2001/02/07 08:09:47 itojun Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: nd6_rtr.c,v 1.104 2015/10/05 04:15:42 ozaki-r Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/cprng.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/radix.h>

#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <netinet6/in6_ifattach.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/nd6.h>
#include <netinet/icmp6.h>
#include <netinet6/icmp6_private.h>
#include <netinet6/scope6_var.h>

#include <net/net_osdep.h>

static int rtpref(struct nd_defrouter *);
static struct nd_defrouter *defrtrlist_update(struct nd_defrouter *);
static int prelist_update(struct nd_prefixctl *, struct nd_defrouter *,
    struct mbuf *, int);
static struct in6_ifaddr *in6_ifadd(struct nd_prefixctl *, int);
static struct nd_pfxrouter *pfxrtr_lookup(struct nd_prefix *,
	struct nd_defrouter *);
static void pfxrtr_add(struct nd_prefix *, struct nd_defrouter *);
static void pfxrtr_del(struct nd_pfxrouter *);
static struct nd_pfxrouter *find_pfxlist_reachable_router
	(struct nd_prefix *);
static void defrouter_delreq(struct nd_defrouter *);

static int in6_init_prefix_ltimes(struct nd_prefix *);
static void in6_init_address_ltimes(struct nd_prefix *,
	struct in6_addrlifetime *);
static void purge_detached(struct ifnet *);

static int rt6_deleteroute(struct rtentry *, void *);

extern int nd6_recalc_reachtm_interval;

static struct ifnet *nd6_defifp;
int nd6_defifindex;

int ip6_use_tempaddr = 0;

int ip6_desync_factor;
u_int32_t ip6_temp_preferred_lifetime = DEF_TEMP_PREFERRED_LIFETIME;
u_int32_t ip6_temp_valid_lifetime = DEF_TEMP_VALID_LIFETIME;
int ip6_temp_regen_advance = TEMPADDR_REGEN_ADVANCE;

int nd6_numroutes = 0;

/* RTPREF_MEDIUM has to be 0! */
#define RTPREF_HIGH	1
#define RTPREF_MEDIUM	0
#define RTPREF_LOW	(-1)
#define RTPREF_RESERVED	(-2)
#define RTPREF_INVALID	(-3)	/* internal */

static inline bool
nd6_is_llinfo_probreach(struct nd_defrouter *dr)
{
	struct rtentry *rt = NULL;
	struct llinfo_nd6 *ln = NULL;

	rt = nd6_lookup(&dr->rtaddr, 0, dr->ifp);
	if (rt == NULL)
		return false;
	ln = (struct llinfo_nd6 *)rt->rt_llinfo;
	rtfree(rt);
	if (ln == NULL || !ND6_IS_LLINFO_PROBREACH(ln))
		return false;

	return true;
}

/*
 * Receive Router Solicitation Message - just for routers.
 * Router solicitation/advertisement is mostly managed by a userland program
 * (rtadvd) so here we have no function like nd6_ra_output().
 *
 * Based on RFC 2461
 */
void
nd6_rs_input(struct mbuf *m, int off, int icmp6len)
{
	struct ifnet *ifp = m->m_pkthdr.rcvif;
	struct nd_ifinfo *ndi = ND_IFINFO(ifp);
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	struct nd_router_solicit *nd_rs;
	struct in6_addr saddr6 = ip6->ip6_src;
	char *lladdr = NULL;
	int lladdrlen = 0;
	union nd_opts ndopts;

	/* If I'm not a router, ignore it. */
	if (nd6_accepts_rtadv(ndi) || !ip6_forwarding)
		goto freeit;

	/* Sanity checks */
	if (ip6->ip6_hlim != 255) {
		nd6log((LOG_ERR,
		    "nd6_rs_input: invalid hlim (%d) from %s to %s on %s\n",
		    ip6->ip6_hlim, ip6_sprintf(&ip6->ip6_src),
		    ip6_sprintf(&ip6->ip6_dst), if_name(ifp)));
		goto bad;
	}

	/*
	 * Don't update the neighbor cache, if src = ::.
	 * This indicates that the src has no IP address assigned yet.
	 */
	if (IN6_IS_ADDR_UNSPECIFIED(&saddr6))
		goto freeit;

	IP6_EXTHDR_GET(nd_rs, struct nd_router_solicit *, m, off, icmp6len);
	if (nd_rs == NULL) {
		ICMP6_STATINC(ICMP6_STAT_TOOSHORT);
		return;
	}

	icmp6len -= sizeof(*nd_rs);
	nd6_option_init(nd_rs + 1, icmp6len, &ndopts);
	if (nd6_options(&ndopts) < 0) {
		nd6log((LOG_INFO,
		    "nd6_rs_input: invalid ND option, ignored\n"));
		/* nd6_options have incremented stats */
		goto freeit;
	}

	if (ndopts.nd_opts_src_lladdr) {
		lladdr = (char *)(ndopts.nd_opts_src_lladdr + 1);
		lladdrlen = ndopts.nd_opts_src_lladdr->nd_opt_len << 3;
	}

	if (lladdr && ((ifp->if_addrlen + 2 + 7) & ~7) != lladdrlen) {
		nd6log((LOG_INFO,
		    "nd6_rs_input: lladdrlen mismatch for %s "
		    "(if %d, RS packet %d)\n",
		    ip6_sprintf(&saddr6), ifp->if_addrlen, lladdrlen - 2));
		goto bad;
	}

	nd6_cache_lladdr(ifp, &saddr6, lladdr, lladdrlen, ND_ROUTER_SOLICIT, 0);

 freeit:
	m_freem(m);
	return;

 bad:
	ICMP6_STATINC(ICMP6_STAT_BADRS);
	m_freem(m);
}

/*
 * Receive Router Advertisement Message.
 *
 * Based on RFC 2461
 * TODO: on-link bit on prefix information
 * TODO: ND_RA_FLAG_{OTHER,MANAGED} processing
 */
void
nd6_ra_input(struct mbuf *m, int off, int icmp6len)
{
	struct ifnet *ifp = m->m_pkthdr.rcvif;
	struct nd_ifinfo *ndi = ND_IFINFO(ifp);
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	struct nd_router_advert *nd_ra;
	struct in6_addr saddr6 = ip6->ip6_src;
#if 0
	struct in6_addr daddr6 = ip6->ip6_dst;
	int flags; /* = nd_ra->nd_ra_flags_reserved; */
	int is_managed = ((flags & ND_RA_FLAG_MANAGED) != 0);
	int is_other = ((flags & ND_RA_FLAG_OTHER) != 0);
#endif
	int mcast = 0;
	union nd_opts ndopts;
	struct nd_defrouter *dr;

	/*
	 * We only accept RAs when
	 * the system-wide variable allows the acceptance, and the
	 * per-interface variable allows RAs on the receiving interface.
	 */
	if (!nd6_accepts_rtadv(ndi))
		goto freeit;

	if (ip6->ip6_hlim != 255) {
		nd6log((LOG_ERR,
		    "nd6_ra_input: invalid hlim (%d) from %s to %s on %s\n",
		    ip6->ip6_hlim, ip6_sprintf(&ip6->ip6_src),
		    ip6_sprintf(&ip6->ip6_dst), if_name(ifp)));
		goto bad;
	}

	if (!IN6_IS_ADDR_LINKLOCAL(&saddr6)) {
		nd6log((LOG_ERR,
		    "nd6_ra_input: src %s is not link-local\n",
		    ip6_sprintf(&saddr6)));
		goto bad;
	}

	IP6_EXTHDR_GET(nd_ra, struct nd_router_advert *, m, off, icmp6len);
	if (nd_ra == NULL) {
		ICMP6_STATINC(ICMP6_STAT_TOOSHORT);
		return;
	}

	icmp6len -= sizeof(*nd_ra);
	nd6_option_init(nd_ra + 1, icmp6len, &ndopts);
	if (nd6_options(&ndopts) < 0) {
		nd6log((LOG_INFO,
		    "nd6_ra_input: invalid ND option, ignored\n"));
		/* nd6_options have incremented stats */
		goto freeit;
	}

    {
	struct nd_defrouter drtr;
	u_int32_t advreachable = nd_ra->nd_ra_reachable;

	/* remember if this is a multicasted advertisement */
	if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst))
		mcast = 1;

	memset(&drtr, 0, sizeof(drtr));
	drtr.rtaddr = saddr6;
	drtr.flags  = nd_ra->nd_ra_flags_reserved;
	drtr.rtlifetime = ntohs(nd_ra->nd_ra_router_lifetime);
	drtr.expire = time_uptime + drtr.rtlifetime;
	drtr.ifp = ifp;
	/* unspecified or not? (RFC 2461 6.3.4) */
	if (advreachable) {
		NTOHL(advreachable);
		if (advreachable <= MAX_REACHABLE_TIME &&
		    ndi->basereachable != advreachable) {
			ndi->basereachable = advreachable;
			ndi->reachable = ND_COMPUTE_RTIME(ndi->basereachable);
			ndi->recalctm = nd6_recalc_reachtm_interval; /* reset */
		}
	}
	if (nd_ra->nd_ra_retransmit)
		ndi->retrans = ntohl(nd_ra->nd_ra_retransmit);
	if (nd_ra->nd_ra_curhoplimit) {
		if (ndi->chlim < nd_ra->nd_ra_curhoplimit)
			ndi->chlim = nd_ra->nd_ra_curhoplimit;
		else if (ndi->chlim != nd_ra->nd_ra_curhoplimit)
			log(LOG_ERR, "nd_ra_input: lower CurHopLimit sent from "
			   "%s on %s (current=%d, received=%d), ignored\n",
			   ip6_sprintf(&ip6->ip6_src),
			   if_name(ifp), ndi->chlim, nd_ra->nd_ra_curhoplimit);
	}
	dr = defrtrlist_update(&drtr);
    }

	/*
	 * prefix
	 */
	if (ndopts.nd_opts_pi) {
		struct nd_opt_hdr *pt;
		struct nd_opt_prefix_info *pi = NULL;
		struct nd_prefixctl prc;

		for (pt = (struct nd_opt_hdr *)ndopts.nd_opts_pi;
		     pt <= (struct nd_opt_hdr *)ndopts.nd_opts_pi_end;
		     pt = (struct nd_opt_hdr *)((char *)pt +
						(pt->nd_opt_len << 3))) {
			if (pt->nd_opt_type != ND_OPT_PREFIX_INFORMATION)
				continue;
			pi = (struct nd_opt_prefix_info *)pt;

			if (pi->nd_opt_pi_len != 4) {
				nd6log((LOG_INFO,
				    "nd6_ra_input: invalid option "
				    "len %d for prefix information option, "
				    "ignored\n", pi->nd_opt_pi_len));
				continue;
			}

			if (128 < pi->nd_opt_pi_prefix_len) {
				nd6log((LOG_INFO,
				    "nd6_ra_input: invalid prefix "
				    "len %d for prefix information option, "
				    "ignored\n", pi->nd_opt_pi_prefix_len));
				continue;
			}

			if (IN6_IS_ADDR_MULTICAST(&pi->nd_opt_pi_prefix)
			 || IN6_IS_ADDR_LINKLOCAL(&pi->nd_opt_pi_prefix)) {
				nd6log((LOG_INFO,
				    "nd6_ra_input: invalid prefix "
				    "%s, ignored\n",
				    ip6_sprintf(&pi->nd_opt_pi_prefix)));
				continue;
			}

			memset(&prc, 0, sizeof(prc));
			sockaddr_in6_init(&prc.ndprc_prefix,
			    &pi->nd_opt_pi_prefix, 0, 0, 0);
			prc.ndprc_ifp = (struct ifnet *)m->m_pkthdr.rcvif;

			prc.ndprc_raf_onlink = (pi->nd_opt_pi_flags_reserved &
			    ND_OPT_PI_FLAG_ONLINK) ? 1 : 0;
			prc.ndprc_raf_auto = (pi->nd_opt_pi_flags_reserved &
			    ND_OPT_PI_FLAG_AUTO) ? 1 : 0;
			prc.ndprc_plen = pi->nd_opt_pi_prefix_len;
			prc.ndprc_vltime = ntohl(pi->nd_opt_pi_valid_time);
			prc.ndprc_pltime = ntohl(pi->nd_opt_pi_preferred_time);

			(void)prelist_update(&prc, dr, m, mcast);
		}
	}

	/*
	 * MTU
	 */
	if (ndopts.nd_opts_mtu && ndopts.nd_opts_mtu->nd_opt_mtu_len == 1) {
		u_long mtu;
		u_long maxmtu;

		mtu = ntohl(ndopts.nd_opts_mtu->nd_opt_mtu_mtu);

		/* lower bound */
		if (mtu < IPV6_MMTU) {
			nd6log((LOG_INFO, "nd6_ra_input: bogus mtu option "
			    "mtu=%lu sent from %s, ignoring\n",
			    mtu, ip6_sprintf(&ip6->ip6_src)));
			goto skip;
		}

		/* upper bound */
		maxmtu = (ndi->maxmtu && ndi->maxmtu < ifp->if_mtu)
		    ? ndi->maxmtu : ifp->if_mtu;
		if (mtu <= maxmtu) {
			int change = (ndi->linkmtu != mtu);

			ndi->linkmtu = mtu;
			if (change) /* in6_maxmtu may change */
				in6_setmaxmtu();
		} else {
			nd6log((LOG_INFO, "nd6_ra_input: bogus mtu "
			    "mtu=%lu sent from %s; "
			    "exceeds maxmtu %lu, ignoring\n",
			    mtu, ip6_sprintf(&ip6->ip6_src), maxmtu));
		}
	}

 skip:

	/*
	 * Source link layer address
	 */
    {
	char *lladdr = NULL;
	int lladdrlen = 0;

	if (ndopts.nd_opts_src_lladdr) {
		lladdr = (char *)(ndopts.nd_opts_src_lladdr + 1);
		lladdrlen = ndopts.nd_opts_src_lladdr->nd_opt_len << 3;
	}

	if (lladdr && ((ifp->if_addrlen + 2 + 7) & ~7) != lladdrlen) {
		nd6log((LOG_INFO,
		    "nd6_ra_input: lladdrlen mismatch for %s "
		    "(if %d, RA packet %d)\n", ip6_sprintf(&saddr6),
		    ifp->if_addrlen, lladdrlen - 2));
		goto bad;
	}

	nd6_cache_lladdr(ifp, &saddr6, lladdr, lladdrlen, ND_ROUTER_ADVERT, 0);

	/*
	 * Installing a link-layer address might change the state of the
	 * router's neighbor cache, which might also affect our on-link
	 * detection of adveritsed prefixes.
	 */
	pfxlist_onlink_check();
    }

 freeit:
	m_freem(m);
	return;

 bad:
	ICMP6_STATINC(ICMP6_STAT_BADRA);
	m_freem(m);
}

/*
 * default router list processing sub routines
 */
void
defrouter_addreq(struct nd_defrouter *newdr)
{
	union {
		struct sockaddr_in6 sin6;
		struct sockaddr sa;
	} def, mask, gate;
	int s;
	int error;

	memset(&def, 0, sizeof(def));
	memset(&mask, 0, sizeof(mask));
	memset(&gate, 0,sizeof(gate)); /* for safety */

	def.sin6.sin6_len = mask.sin6.sin6_len = gate.sin6.sin6_len =
	    sizeof(struct sockaddr_in6);
	def.sin6.sin6_family = mask.sin6.sin6_family = gate.sin6.sin6_family = AF_INET6;
	gate.sin6.sin6_addr = newdr->rtaddr;
#ifndef SCOPEDROUTING
	gate.sin6.sin6_scope_id = 0;	/* XXX */
#endif

	s = splsoftnet();
	error = rtrequest_newmsg(RTM_ADD, &def.sa, &gate.sa, &mask.sa,
	    RTF_GATEWAY);
	if (error == 0) {
		nd6_numroutes++;
		newdr->installed = 1;
	}
	splx(s);
	return;
}

struct nd_defrouter *
defrouter_lookup(const struct in6_addr *addr, struct ifnet *ifp)
{
	struct nd_defrouter *dr;

	TAILQ_FOREACH(dr, &nd_defrouter, dr_entry) {
		if (dr->ifp == ifp && IN6_ARE_ADDR_EQUAL(addr, &dr->rtaddr))
			break;
	}

	return dr;		/* search failed */
}

void
defrtrlist_del(struct nd_defrouter *dr, struct in6_ifextra *ext)
{
	struct nd_defrouter *deldr = NULL;
	struct nd_prefix *pr;
	struct nd_ifinfo *ndi;

	if (ext == NULL)
		ext = dr->ifp->if_afdata[AF_INET6];

	/* detach already in progress, can not do anything */
	if (ext == NULL)
		return;

	ndi = ext->nd_ifinfo;

	/*
	 * Flush all the routing table entries that use the router
	 * as a next hop.
	 */
	/* XXX: better condition? */
	if (!ip6_forwarding && nd6_accepts_rtadv(ndi))
		rt6_flush(&dr->rtaddr, dr->ifp);

	if (dr->installed) {
		deldr = dr;
		defrouter_delreq(dr);
	}
	TAILQ_REMOVE(&nd_defrouter, dr, dr_entry);

	/*
	 * Also delete all the pointers to the router in each prefix lists.
	 */
	LIST_FOREACH(pr, &nd_prefix, ndpr_entry) {
		struct nd_pfxrouter *pfxrtr;
		if ((pfxrtr = pfxrtr_lookup(pr, dr)) != NULL)
			pfxrtr_del(pfxrtr);
	}
	pfxlist_onlink_check();

	/*
	 * If the router is the primary one, choose a new one.
	 * Note that defrouter_select() will remove the current gateway
	 * from the routing table.
	 */
	if (deldr)
		defrouter_select();

	ext->ndefrouters--;
	if (ext->ndefrouters < 0) {
		log(LOG_WARNING, "defrtrlist_del: negative count on %s\n",
		    dr->ifp->if_xname);
	}

	free(dr, M_IP6NDP);
}

/*
 * Remove the default route for a given router.
 * This is just a subroutine function for defrouter_select(), and should
 * not be called from anywhere else.
 */
static void
defrouter_delreq(struct nd_defrouter *dr)
{
	union {
		struct sockaddr_in6 sin6;
		struct sockaddr sa;
	} def, mask, gw;
	int error;

#ifdef DIAGNOSTIC
	if (dr == NULL)
		panic("dr == NULL in defrouter_delreq");
#endif

	memset(&def, 0, sizeof(def));
	memset(&mask, 0, sizeof(mask));
	memset(&gw, 0, sizeof(gw));	/* for safety */

	def.sin6.sin6_len = mask.sin6.sin6_len = gw.sin6.sin6_len =
	    sizeof(struct sockaddr_in6);
	def.sin6.sin6_family = mask.sin6.sin6_family = gw.sin6.sin6_family = AF_INET6;
	gw.sin6.sin6_addr = dr->rtaddr;
#ifndef SCOPEDROUTING
	gw.sin6.sin6_scope_id = 0;	/* XXX */
#endif

	error = rtrequest_newmsg(RTM_DELETE, &def.sa, &gw.sa, &mask.sa,
	    RTF_GATEWAY);
	if (error == 0)
		nd6_numroutes--;

	dr->installed = 0;
}

/*
 * remove all default routes from default router list
 */
void
defrouter_reset(void)
{
	struct nd_defrouter *dr;

	for (dr = TAILQ_FIRST(&nd_defrouter); dr;
	     dr = TAILQ_NEXT(dr, dr_entry))
		defrouter_delreq(dr);

	/*
	 * XXX should we also nuke any default routers in the kernel, by
	 * going through them by rtalloc1()?
	 */
}

/*
 * Default Router Selection according to Section 6.3.6 of RFC 2461 and
 * draft-ietf-ipngwg-router-selection:
 * 1) Routers that are reachable or probably reachable should be preferred.
 *    If we have more than one (probably) reachable router, prefer ones
 *    with the highest router preference.
 * 2) When no routers on the list are known to be reachable or
 *    probably reachable, routers SHOULD be selected in a round-robin
 *    fashion, regardless of router preference values.
 * 3) If the Default Router List is empty, assume that all
 *    destinations are on-link.
 *
 * We assume nd_defrouter is sorted by router preference value.
 * Since the code below covers both with and without router preference cases,
 * we do not need to classify the cases by ifdef.
 *
 * At this moment, we do not try to install more than one default router,
 * even when the multipath routing is available, because we're not sure about
 * the benefits for stub hosts comparing to the risk of making the code
 * complicated and the possibility of introducing bugs.
 */
void
defrouter_select(void)
{
	struct nd_ifinfo *ndi;
	int s = splsoftnet();
	struct nd_defrouter *dr, *selected_dr = NULL, *installed_dr = NULL;

	/*
	 * This function should be called only when acting as an autoconfigured
	 * host.  Although the remaining part of this function is not effective
	 * if the node is not an autoconfigured host, we explicitly exclude
	 * such cases here for safety.
	 */
	if (ip6_forwarding) {
		nd6log((LOG_WARNING,
		    "defrouter_select: called unexpectedly (forwarding=%d, "
		    "accept_rtadv=%d)\n", ip6_forwarding, ip6_accept_rtadv));
		splx(s);
		return;
	}

	/*
	 * Let's handle easy case (3) first:
	 * If default router list is empty, there's nothing to be done.
	 */
	if (!TAILQ_FIRST(&nd_defrouter)) {
		splx(s);
		return;
	}

	/*
	 * Search for a (probably) reachable router from the list.
	 * We just pick up the first reachable one (if any), assuming that
	 * the ordering rule of the list described in defrtrlist_update().
	 */
	for (dr = TAILQ_FIRST(&nd_defrouter); dr;
	     dr = TAILQ_NEXT(dr, dr_entry)) {
		ndi = ND_IFINFO(dr->ifp);
		if (nd6_accepts_rtadv(ndi))
			continue;

		if (selected_dr == NULL &&
		    nd6_is_llinfo_probreach(dr))
			selected_dr = dr;

		if (dr->installed && !installed_dr)
			installed_dr = dr;
		else if (dr->installed && installed_dr) {
			/* this should not happen.  warn for diagnosis. */
			log(LOG_ERR, "defrouter_select: more than one router"
			    " is installed\n");
		}
	}
	/*
	 * If none of the default routers was found to be reachable,
	 * round-robin the list regardless of preference.
	 * Otherwise, if we have an installed router, check if the selected
	 * (reachable) router should really be preferred to the installed one.
	 * We only prefer the new router when the old one is not reachable
	 * or when the new one has a really higher preference value.
	 */
	if (selected_dr == NULL) {
		if (installed_dr == NULL || !TAILQ_NEXT(installed_dr, dr_entry))
			selected_dr = TAILQ_FIRST(&nd_defrouter);
		else
			selected_dr = TAILQ_NEXT(installed_dr, dr_entry);
	} else if (installed_dr &&
	    nd6_is_llinfo_probreach(installed_dr) &&
	    rtpref(selected_dr) <= rtpref(installed_dr)) {
		selected_dr = installed_dr;
	}

	/*
	 * If the selected router is different than the installed one,
	 * remove the installed router and install the selected one.
	 * Note that the selected router is never NULL here.
	 */
	if (installed_dr != selected_dr) {
		if (installed_dr)
			defrouter_delreq(installed_dr);
		defrouter_addreq(selected_dr);
	}

	splx(s);
	return;
}

/*
 * for default router selection
 * regards router-preference field as a 2-bit signed integer
 */
static int
rtpref(struct nd_defrouter *dr)
{
	switch (dr->flags & ND_RA_FLAG_RTPREF_MASK) {
	case ND_RA_FLAG_RTPREF_HIGH:
		return (RTPREF_HIGH);
	case ND_RA_FLAG_RTPREF_MEDIUM:
	case ND_RA_FLAG_RTPREF_RSV:
		return (RTPREF_MEDIUM);
	case ND_RA_FLAG_RTPREF_LOW:
		return (RTPREF_LOW);
	default:
		/*
		 * This case should never happen.  If it did, it would mean a
		 * serious bug of kernel internal.  We thus always bark here.
		 * Or, can we even panic?
		 */
		log(LOG_ERR, "rtpref: impossible RA flag %x\n", dr->flags);
		return (RTPREF_INVALID);
	}
	/* NOTREACHED */
}

static struct nd_defrouter *
defrtrlist_update(struct nd_defrouter *newdr)
{
	struct nd_defrouter *dr, *n;
	struct in6_ifextra *ext = newdr->ifp->if_afdata[AF_INET6];
	int s = splsoftnet();

	if ((dr = defrouter_lookup(&newdr->rtaddr, newdr->ifp)) != NULL) {
		/* entry exists */
		if (newdr->rtlifetime == 0) {
			defrtrlist_del(dr, ext);
			dr = NULL;
		} else {
			int oldpref = rtpref(dr);

			/* override */
			dr->flags = newdr->flags; /* xxx flag check */
			dr->rtlifetime = newdr->rtlifetime;
			dr->expire = newdr->expire;

			/*
			 * If the preference does not change, there's no need
			 * to sort the entries.
			 */
			if (rtpref(newdr) == oldpref) {
				splx(s);
				return (dr);
			}

			/*
			 * preferred router may be changed, so relocate
			 * this router.
			 * XXX: calling TAILQ_REMOVE directly is a bad manner.
			 * However, since defrtrlist_del() has many side
			 * effects, we intentionally do so here.
			 * defrouter_select() below will handle routing
			 * changes later.
			 */
			TAILQ_REMOVE(&nd_defrouter, dr, dr_entry);
			n = dr;
			goto insert;
		}
		splx(s);
		return (dr);
	}

	if (ip6_maxifdefrouters >= 0 &&
	    ext->ndefrouters >= ip6_maxifdefrouters) {
		splx(s);
		return (NULL);
	}

	/* entry does not exist */
	if (newdr->rtlifetime == 0) {
		splx(s);
		return (NULL);
	}

	if (ip6_rtadv_maxroutes <= nd6_numroutes) {
		ICMP6_STATINC(ICMP6_STAT_DROPPED_RAROUTE);
		splx(s);
		return (NULL);
	}

	n = (struct nd_defrouter *)malloc(sizeof(*n), M_IP6NDP, M_NOWAIT);
	if (n == NULL) {
		splx(s);
		return (NULL);
	}
	memset(n, 0, sizeof(*n));
	*n = *newdr;

insert:
	/*
	 * Insert the new router in the Default Router List;
	 * The Default Router List should be in the descending order
	 * of router-preferece.  Routers with the same preference are
	 * sorted in the arriving time order.
	 */

	/* insert at the end of the group */
	for (dr = TAILQ_FIRST(&nd_defrouter); dr;
	     dr = TAILQ_NEXT(dr, dr_entry)) {
		if (rtpref(n) > rtpref(dr))
			break;
	}
	if (dr)
		TAILQ_INSERT_BEFORE(dr, n, dr_entry);
	else
		TAILQ_INSERT_TAIL(&nd_defrouter, n, dr_entry);

	defrouter_select();

	ext->ndefrouters++;

	splx(s);

	return (n);
}

static struct nd_pfxrouter *
pfxrtr_lookup(struct nd_prefix *pr, struct nd_defrouter *dr)
{
	struct nd_pfxrouter *search;

	LIST_FOREACH(search, &pr->ndpr_advrtrs, pfr_entry) {
		if (search->router == dr)
			break;
	}

	return (search);
}

static void
pfxrtr_add(struct nd_prefix *pr, struct nd_defrouter *dr)
{
	struct nd_pfxrouter *newpfr;

	newpfr = malloc(sizeof(*newpfr), M_IP6NDP, M_NOWAIT|M_ZERO);
	if (newpfr == NULL)
		return;
	newpfr->router = dr;

	LIST_INSERT_HEAD(&pr->ndpr_advrtrs, newpfr, pfr_entry);

	pfxlist_onlink_check();
}

static void
pfxrtr_del(struct nd_pfxrouter *pfr)
{
	LIST_REMOVE(pfr, pfr_entry);
	free(pfr, M_IP6NDP);
}

struct nd_prefix *
nd6_prefix_lookup(struct nd_prefixctl *key)
{
	struct nd_prefix *search;

	LIST_FOREACH(search, &nd_prefix, ndpr_entry) {
		if (key->ndprc_ifp == search->ndpr_ifp &&
		    key->ndprc_plen == search->ndpr_plen &&
		    in6_are_prefix_equal(&key->ndprc_prefix.sin6_addr,
		    &search->ndpr_prefix.sin6_addr, key->ndprc_plen)) {
			break;
		}
	}

	return (search);
}

static void
purge_detached(struct ifnet *ifp)
{
	struct nd_prefix *pr, *pr_next;
	struct in6_ifaddr *ia;
	struct ifaddr *ifa, *ifa_next;

	for (pr = nd_prefix.lh_first; pr; pr = pr_next) {
		pr_next = pr->ndpr_next;

		/*
		 * This function is called when we need to make more room for
		 * new prefixes rather than keeping old, possibly stale ones.
		 * Detached prefixes would be a good candidate; if all routers
		 * that advertised the prefix expired, the prefix is also
		 * probably stale.
		 */
		if (pr->ndpr_ifp != ifp ||
		    IN6_IS_ADDR_LINKLOCAL(&pr->ndpr_prefix.sin6_addr) ||
		    ((pr->ndpr_stateflags & NDPRF_DETACHED) == 0 &&
		    !LIST_EMPTY(&pr->ndpr_advrtrs)))
			continue;

		IFADDR_FOREACH_SAFE(ifa, ifp, ifa_next) {
			if (ifa->ifa_addr->sa_family != AF_INET6)
				continue;
			ia = (struct in6_ifaddr *)ifa;
			if ((ia->ia6_flags & IN6_IFF_AUTOCONF) ==
			    IN6_IFF_AUTOCONF && ia->ia6_ndpr == pr) {
				in6_purgeaddr(ifa);
			}
		}
		if (pr->ndpr_refcnt == 0)
			prelist_remove(pr);
	}
}
int
nd6_prelist_add(struct nd_prefixctl *prc, struct nd_defrouter *dr, 
	struct nd_prefix **newp)
{
	struct nd_prefix *newpr = NULL;
	int i, s;
	int error;
	struct in6_ifextra *ext = prc->ndprc_ifp->if_afdata[AF_INET6];

	if (ip6_maxifprefixes >= 0) { 
		if (ext->nprefixes >= ip6_maxifprefixes / 2) 
			purge_detached(prc->ndprc_ifp);
		if (ext->nprefixes >= ip6_maxifprefixes)
			return ENOMEM;
	}

	error = 0;
	newpr = malloc(sizeof(*newpr), M_IP6NDP, M_NOWAIT|M_ZERO);
	if (newpr == NULL)
		return ENOMEM;
	newpr->ndpr_ifp = prc->ndprc_ifp;
	newpr->ndpr_prefix = prc->ndprc_prefix;
	newpr->ndpr_plen = prc->ndprc_plen;
	newpr->ndpr_vltime = prc->ndprc_vltime;
	newpr->ndpr_pltime = prc->ndprc_pltime;
	newpr->ndpr_flags = prc->ndprc_flags;
	if ((error = in6_init_prefix_ltimes(newpr)) != 0) {
		free(newpr, M_IP6NDP);
		return(error);
	}
	newpr->ndpr_lastupdate = time_uptime;
	if (newp != NULL)
		*newp = newpr;

	/* initialization */
	LIST_INIT(&newpr->ndpr_advrtrs);
	in6_prefixlen2mask(&newpr->ndpr_mask, newpr->ndpr_plen);
	/* make prefix in the canonical form */
	for (i = 0; i < 4; i++) {
		newpr->ndpr_prefix.sin6_addr.s6_addr32[i] &=
		    newpr->ndpr_mask.s6_addr32[i];
	}

	s = splsoftnet();
	/* link ndpr_entry to nd_prefix list */
	LIST_INSERT_HEAD(&nd_prefix, newpr, ndpr_entry);
	splx(s);

	/* ND_OPT_PI_FLAG_ONLINK processing */
	if (newpr->ndpr_raf_onlink) {
		int e;

		if ((e = nd6_prefix_onlink(newpr)) != 0) {
			nd6log((LOG_ERR, "nd6_prelist_add: failed to make "
			    "the prefix %s/%d on-link on %s (errno=%d)\n",
			    ip6_sprintf(&prc->ndprc_prefix.sin6_addr),
			    prc->ndprc_plen, if_name(prc->ndprc_ifp), e));
			/* proceed anyway. XXX: is it correct? */
		}
	}

	if (dr)
		pfxrtr_add(newpr, dr);

	ext->nprefixes++;

	return 0;
}

void
prelist_remove(struct nd_prefix *pr)
{
	struct nd_pfxrouter *pfr, *next;
	int e, s;
	struct in6_ifextra *ext = pr->ndpr_ifp->if_afdata[AF_INET6];

	/* make sure to invalidate the prefix until it is really freed. */
	pr->ndpr_vltime = 0;
	pr->ndpr_pltime = 0;
#if 0
	/*
	 * Though these flags are now meaningless, we'd rather keep the value
	 * not to confuse users when executing "ndp -p".
	 */
	pr->ndpr_raf_onlink = 0;
	pr->ndpr_raf_auto = 0;
#endif
	if ((pr->ndpr_stateflags & NDPRF_ONLINK) != 0 &&
	    (e = nd6_prefix_offlink(pr)) != 0) {
		nd6log((LOG_ERR, "prelist_remove: failed to make %s/%d offlink "
		    "on %s, errno=%d\n",
		    ip6_sprintf(&pr->ndpr_prefix.sin6_addr),
		    pr->ndpr_plen, if_name(pr->ndpr_ifp), e));
		/* what should we do? */
	}

	if (pr->ndpr_refcnt > 0)
		return;		/* notice here? */

	s = splsoftnet();
	/* unlink ndpr_entry from nd_prefix list */
	LIST_REMOVE(pr, ndpr_entry);

	/* free list of routers that adversed the prefix */
	for (pfr = LIST_FIRST(&pr->ndpr_advrtrs); pfr != NULL; pfr = next) {
		next = LIST_NEXT(pfr, pfr_entry);

		free(pfr, M_IP6NDP);
	}

	if (ext) {
		ext->nprefixes--;
		if (ext->nprefixes < 0) {
			log(LOG_WARNING, "prelist_remove: negative count on "
			    "%s\n", pr->ndpr_ifp->if_xname);
		}
	}
	splx(s);

	free(pr, M_IP6NDP);

	pfxlist_onlink_check();
}

static int
prelist_update(struct nd_prefixctl *newprc,
	struct nd_defrouter *dr, /* may be NULL */
	struct mbuf *m, 
	int mcast)
{
	struct in6_ifaddr *ia6 = NULL, *ia6_match = NULL;
	struct ifaddr *ifa;
	struct ifnet *ifp = newprc->ndprc_ifp;
	struct nd_prefix *pr;
	int s = splsoftnet();
	int error = 0;
	int auth;
	struct in6_addrlifetime lt6_tmp;

	auth = 0;
	if (m) {
		/*
		 * Authenticity for NA consists authentication for
		 * both IP header and IP datagrams, doesn't it ?
		 */
#if defined(M_AUTHIPHDR) && defined(M_AUTHIPDGM)
		auth = (m->m_flags & M_AUTHIPHDR
		     && m->m_flags & M_AUTHIPDGM) ? 1 : 0;
#endif
	}

	if ((pr = nd6_prefix_lookup(newprc)) != NULL) {
		/*
		 * nd6_prefix_lookup() ensures that pr and newprc have the same
		 * prefix on a same interface.
		 */

		/*
		 * Update prefix information.  Note that the on-link (L) bit
		 * and the autonomous (A) bit should NOT be changed from 1
		 * to 0.
		 */
		if (newprc->ndprc_raf_onlink == 1)
			pr->ndpr_raf_onlink = 1;
		if (newprc->ndprc_raf_auto == 1)
			pr->ndpr_raf_auto = 1;
		if (newprc->ndprc_raf_onlink) {
			pr->ndpr_vltime = newprc->ndprc_vltime;
			pr->ndpr_pltime = newprc->ndprc_pltime;
			(void)in6_init_prefix_ltimes(pr); /* XXX error case? */
			pr->ndpr_lastupdate = time_uptime;
		}

		if (newprc->ndprc_raf_onlink &&
		    (pr->ndpr_stateflags & NDPRF_ONLINK) == 0) {
			int e;

			if ((e = nd6_prefix_onlink(pr)) != 0) {
				nd6log((LOG_ERR,
				    "prelist_update: failed to make "
				    "the prefix %s/%d on-link on %s "
				    "(errno=%d)\n",
				    ip6_sprintf(&pr->ndpr_prefix.sin6_addr),
				    pr->ndpr_plen, if_name(pr->ndpr_ifp), e));
				/* proceed anyway. XXX: is it correct? */
			}
		}

		if (dr && pfxrtr_lookup(pr, dr) == NULL)
			pfxrtr_add(pr, dr);
	} else {
		struct nd_prefix *newpr = NULL;

		if (newprc->ndprc_vltime == 0)
			goto end;
		if (newprc->ndprc_raf_onlink == 0 && newprc->ndprc_raf_auto == 0)
			goto end;

		if (ip6_rtadv_maxroutes <= nd6_numroutes) {
			ICMP6_STATINC(ICMP6_STAT_DROPPED_RAROUTE);
			goto end;
		}

		error = nd6_prelist_add(newprc, dr, &newpr);
		if (error != 0 || newpr == NULL) {
			nd6log((LOG_NOTICE, "prelist_update: "
			    "nd6_prelist_add failed for %s/%d on %s "
			    "errno=%d, returnpr=%p\n",
			    ip6_sprintf(&newprc->ndprc_prefix.sin6_addr),
			    newprc->ndprc_plen, if_name(newprc->ndprc_ifp),
			    error, newpr));
			goto end; /* we should just give up in this case. */
		}

		/*
		 * XXX: from the ND point of view, we can ignore a prefix
		 * with the on-link bit being zero.  However, we need a
		 * prefix structure for references from autoconfigured
		 * addresses.  Thus, we explicitly make sure that the prefix
		 * itself expires now.
		 */
		if (newpr->ndpr_raf_onlink == 0) {
			newpr->ndpr_vltime = 0;
			newpr->ndpr_pltime = 0;
			in6_init_prefix_ltimes(newpr);
		}

		pr = newpr;
	}

	/*
	 * Address autoconfiguration based on Section 5.5.3 of RFC 2462.
	 * Note that pr must be non NULL at this point.
	 */

	/* 5.5.3 (a). Ignore the prefix without the A bit set. */
	if (!newprc->ndprc_raf_auto)
		goto end;

	/*
	 * 5.5.3 (b). the link-local prefix should have been ignored in
	 * nd6_ra_input.
	 */

	/* 5.5.3 (c). Consistency check on lifetimes: pltime <= vltime. */
	if (newprc->ndprc_pltime > newprc->ndprc_vltime) {
		error = EINVAL;	/* XXX: won't be used */
		goto end;
	}

	/*
	 * 5.5.3 (d).  If the prefix advertised is not equal to the prefix of
	 * an address configured by stateless autoconfiguration already in the
	 * list of addresses associated with the interface, and the Valid
	 * Lifetime is not 0, form an address.  We first check if we have
	 * a matching prefix.
	 * Note: we apply a clarification in rfc2462bis-02 here.  We only
	 * consider autoconfigured addresses while RFC2462 simply said
	 * "address".
	 */
	IFADDR_FOREACH(ifa, ifp) {
		struct in6_ifaddr *ifa6;
		u_int32_t remaininglifetime;

		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;

		ifa6 = (struct in6_ifaddr *)ifa;

		/*
		 * We only consider autoconfigured addresses as per rfc2462bis.
		 */
		if (!(ifa6->ia6_flags & IN6_IFF_AUTOCONF))
			continue;

		/*
		 * Spec is not clear here, but I believe we should concentrate
		 * on unicast (i.e. not anycast) addresses.
		 * XXX: other ia6_flags? detached or duplicated?
		 */
		if ((ifa6->ia6_flags & IN6_IFF_ANYCAST) != 0)
			continue;

		/*
		 * Ignore the address if it is not associated with a prefix
		 * or is associated with a prefix that is different from this
		 * one.  (pr is never NULL here)
		 */
		if (ifa6->ia6_ndpr != pr)
			continue;

		if (ia6_match == NULL) /* remember the first one */
			ia6_match = ifa6;

		/*
		 * An already autoconfigured address matched.  Now that we
		 * are sure there is at least one matched address, we can
		 * proceed to 5.5.3. (e): update the lifetimes according to the
		 * "two hours" rule and the privacy extension.
		 * We apply some clarifications in rfc2462bis:
		 * - use remaininglifetime instead of storedlifetime as a
		 *   variable name
		 * - remove the dead code in the "two-hour" rule
		 */
#define TWOHOUR		(120*60)
		lt6_tmp = ifa6->ia6_lifetime;
		if (lt6_tmp.ia6t_vltime == ND6_INFINITE_LIFETIME)
			remaininglifetime = ND6_INFINITE_LIFETIME;
		else if (time_uptime - ifa6->ia6_updatetime >
			 lt6_tmp.ia6t_vltime) {
			/*
			 * The case of "invalid" address.  We should usually
			 * not see this case.
			 */
			remaininglifetime = 0;
		} else
			remaininglifetime = lt6_tmp.ia6t_vltime -
			    (time_uptime - ifa6->ia6_updatetime);

		/* when not updating, keep the current stored lifetime. */
		lt6_tmp.ia6t_vltime = remaininglifetime;

		if (TWOHOUR < newprc->ndprc_vltime ||
		    remaininglifetime < newprc->ndprc_vltime) {
			lt6_tmp.ia6t_vltime = newprc->ndprc_vltime;
		} else if (remaininglifetime <= TWOHOUR) {
			if (auth)
				lt6_tmp.ia6t_vltime = newprc->ndprc_vltime;
		} else {
			/*
			 * newprc->ndprc_vltime <= TWOHOUR &&
			 * TWOHOUR < remaininglifetime
			 */
			lt6_tmp.ia6t_vltime = TWOHOUR;
		}

		/* The 2 hour rule is not imposed for preferred lifetime. */
		lt6_tmp.ia6t_pltime = newprc->ndprc_pltime;

		in6_init_address_ltimes(pr, &lt6_tmp);

		/*
		 * We need to treat lifetimes for temporary addresses
		 * differently, according to
		 * draft-ietf-ipv6-privacy-addrs-v2-01.txt 3.3 (1);
		 * we only update the lifetimes when they are in the maximum
		 * intervals.
		 */
		if ((ifa6->ia6_flags & IN6_IFF_TEMPORARY) != 0) {
			u_int32_t maxvltime, maxpltime;

			if (ip6_temp_valid_lifetime >
			    (u_int32_t)((time_uptime - ifa6->ia6_createtime) +
			    ip6_desync_factor)) {
				maxvltime = ip6_temp_valid_lifetime -
				    (time_uptime - ifa6->ia6_createtime) -
				    ip6_desync_factor;
			} else
				maxvltime = 0;
			if (ip6_temp_preferred_lifetime >
			    (u_int32_t)((time_uptime - ifa6->ia6_createtime) +
			    ip6_desync_factor)) {
				maxpltime = ip6_temp_preferred_lifetime -
				    (time_uptime - ifa6->ia6_createtime) -
				    ip6_desync_factor;
			} else
				maxpltime = 0;

			if (lt6_tmp.ia6t_vltime == ND6_INFINITE_LIFETIME ||
			    lt6_tmp.ia6t_vltime > maxvltime) {
				lt6_tmp.ia6t_vltime = maxvltime;
			}
			if (lt6_tmp.ia6t_pltime == ND6_INFINITE_LIFETIME ||
			    lt6_tmp.ia6t_pltime > maxpltime) {
				lt6_tmp.ia6t_pltime = maxpltime;
			}
		}

		ifa6->ia6_lifetime = lt6_tmp;
		ifa6->ia6_updatetime = time_uptime;
	}
	if (ia6_match == NULL && newprc->ndprc_vltime) {
		int ifidlen;

		/*
		 * 5.5.3 (d) (continued)
		 * No address matched and the valid lifetime is non-zero.
		 * Create a new address.
		 */

		/*
		 * Prefix Length check:
		 * If the sum of the prefix length and interface identifier
		 * length does not equal 128 bits, the Prefix Information
		 * option MUST be ignored.  The length of the interface
		 * identifier is defined in a separate link-type specific
		 * document.
		 */
		ifidlen = in6_if2idlen(ifp);
		if (ifidlen < 0) {
			/* this should not happen, so we always log it. */
			log(LOG_ERR, "prelist_update: IFID undefined (%s)\n",
			    if_name(ifp));
			goto end;
		}
		if (ifidlen + pr->ndpr_plen != 128) {
			nd6log((LOG_INFO,
			    "prelist_update: invalid prefixlen "
			    "%d for %s, ignored\n",
			    pr->ndpr_plen, if_name(ifp)));
			goto end;
		}

		if ((ia6 = in6_ifadd(newprc, mcast)) != NULL) {
			/*
			 * note that we should use pr (not newprc) for reference.
			 */
			pr->ndpr_refcnt++;
			ia6->ia6_ndpr = pr;

			/*
			 * draft-ietf-ipngwg-temp-addresses-v2-00 3.3 (2).
			 * When a new public address is created as described
			 * in RFC2462, also create a new temporary address.
			 *
			 * draft-ietf-ipngwg-temp-addresses-v2-00 3.5.
			 * When an interface connects to a new link, a new
			 * randomized interface identifier should be generated
			 * immediately together with a new set of temporary
			 * addresses.  Thus, we specifiy 1 as the 2nd arg of
			 * in6_tmpifadd().
			 */
			if (ip6_use_tempaddr) {
				int e;
				if ((e = in6_tmpifadd(ia6, 1, 1)) != 0) {
					nd6log((LOG_NOTICE, "prelist_update: "
					    "failed to create a temporary "
					    "address, errno=%d\n",
					    e));
				}
			}

			/*
			 * A newly added address might affect the status
			 * of other addresses, so we check and update it.
			 * XXX: what if address duplication happens?
			 */
			pfxlist_onlink_check();
		} else {
			/* just set an error. do not bark here. */
			error = EADDRNOTAVAIL; /* XXX: might be unused. */
		}
	}

 end:
	splx(s);
	return error;
}

/*
 * A supplement function used in the on-link detection below;
 * detect if a given prefix has a (probably) reachable advertising router.
 * XXX: lengthy function name...
 */
static struct nd_pfxrouter *
find_pfxlist_reachable_router(struct nd_prefix *pr)
{
	struct nd_pfxrouter *pfxrtr;

	for (pfxrtr = LIST_FIRST(&pr->ndpr_advrtrs); pfxrtr;
	     pfxrtr = LIST_NEXT(pfxrtr, pfr_entry)) {
		if (pfxrtr->router->ifp->if_flags & IFF_UP &&
		    pfxrtr->router->ifp->if_link_state != LINK_STATE_DOWN &&
		    nd6_is_llinfo_probreach(pfxrtr->router))
			break;	/* found */
	}

	return (pfxrtr);
}

/*
 * Check if each prefix in the prefix list has at least one available router
 * that advertised the prefix (a router is "available" if its neighbor cache
 * entry is reachable or probably reachable).
 * If the check fails, the prefix may be off-link, because, for example,
 * we have moved from the network but the lifetime of the prefix has not
 * expired yet.  So we should not use the prefix if there is another prefix
 * that has an available router.
 * But, if there is no prefix that has an available router, we still regards
 * all the prefixes as on-link.  This is because we can't tell if all the
 * routers are simply dead or if we really moved from the network and there
 * is no router around us.
 */
void
pfxlist_onlink_check(void)
{
	struct nd_prefix *pr;
	struct in6_ifaddr *ifa;
	struct nd_defrouter *dr;
	struct nd_pfxrouter *pfxrtr = NULL;

	/*
	 * Check if there is a prefix that has a reachable advertising
	 * router.
	 */
	LIST_FOREACH(pr, &nd_prefix, ndpr_entry) {
		if (pr->ndpr_raf_onlink && find_pfxlist_reachable_router(pr))
			break;
	}
	/*
	 * If we have no such prefix, check whether we still have a router
	 * that does not advertise any prefixes.
	 */
	if (pr == NULL) {
		TAILQ_FOREACH(dr, &nd_defrouter, dr_entry) {
			struct nd_prefix *pr0;

			LIST_FOREACH(pr0, &nd_prefix, ndpr_entry) {
				if ((pfxrtr = pfxrtr_lookup(pr0, dr)) != NULL)
					break;
			}
			if (pfxrtr)
				break;
		}
	}
	if (pr != NULL || (TAILQ_FIRST(&nd_defrouter) && !pfxrtr)) {
		/*
		 * There is at least one prefix that has a reachable router,
		 * or at least a router which probably does not advertise
		 * any prefixes.  The latter would be the case when we move
		 * to a new link where we have a router that does not provide
		 * prefixes and we configure an address by hand.
		 * Detach prefixes which have no reachable advertising
		 * router, and attach other prefixes.
		 */
		LIST_FOREACH(pr, &nd_prefix, ndpr_entry) {
			/* XXX: a link-local prefix should never be detached */
			if (IN6_IS_ADDR_LINKLOCAL(&pr->ndpr_prefix.sin6_addr))
				continue;

			/*
			 * we aren't interested in prefixes without the L bit
			 * set.
			 */
			if (pr->ndpr_raf_onlink == 0)
				continue;

			if ((pr->ndpr_stateflags & NDPRF_DETACHED) == 0 &&
			    find_pfxlist_reachable_router(pr) == NULL)
				pr->ndpr_stateflags |= NDPRF_DETACHED;
			if ((pr->ndpr_stateflags & NDPRF_DETACHED) != 0 &&
			    find_pfxlist_reachable_router(pr) != 0)
				pr->ndpr_stateflags &= ~NDPRF_DETACHED;
		}
	} else {
		/* there is no prefix that has a reachable router */
		LIST_FOREACH(pr, &nd_prefix, ndpr_entry) {
			if (IN6_IS_ADDR_LINKLOCAL(&pr->ndpr_prefix.sin6_addr))
				continue;

			if (pr->ndpr_raf_onlink == 0)
				continue;

			if ((pr->ndpr_stateflags & NDPRF_DETACHED) != 0)
				pr->ndpr_stateflags &= ~NDPRF_DETACHED;
		}
	}

	/*
	 * Remove each interface route associated with a (just) detached
	 * prefix, and reinstall the interface route for a (just) attached
	 * prefix.  Note that all attempt of reinstallation does not
	 * necessarily success, when a same prefix is shared among multiple
	 * interfaces.  Such cases will be handled in nd6_prefix_onlink,
	 * so we don't have to care about them.
	 */
	LIST_FOREACH(pr, &nd_prefix, ndpr_entry) {
		int e;

		if (IN6_IS_ADDR_LINKLOCAL(&pr->ndpr_prefix.sin6_addr))
			continue;

		if (pr->ndpr_raf_onlink == 0)
			continue;

		if ((pr->ndpr_stateflags & NDPRF_DETACHED) != 0 &&
		    (pr->ndpr_stateflags & NDPRF_ONLINK) != 0) {
			if ((e = nd6_prefix_offlink(pr)) != 0) {
				nd6log((LOG_ERR,
				    "pfxlist_onlink_check: failed to "
				    "make %s/%d offlink, errno=%d\n",
				    ip6_sprintf(&pr->ndpr_prefix.sin6_addr),
				    pr->ndpr_plen, e));
			}
		}
		if ((pr->ndpr_stateflags & NDPRF_DETACHED) == 0 &&
		    (pr->ndpr_stateflags & NDPRF_ONLINK) == 0 &&
		    pr->ndpr_raf_onlink) {
			if ((e = nd6_prefix_onlink(pr)) != 0) {
				nd6log((LOG_ERR,
				    "pfxlist_onlink_check: failed to "
				    "make %s/%d onlink, errno=%d\n",
				    ip6_sprintf(&pr->ndpr_prefix.sin6_addr),
				    pr->ndpr_plen, e));
			}
		}
	}

	/*
	 * Changes on the prefix status might affect address status as well.
	 * Make sure that all addresses derived from an attached prefix are
	 * attached, and that all addresses derived from a detached prefix are
	 * detached.  Note, however, that a manually configured address should
	 * always be attached.
	 * The precise detection logic is same as the one for prefixes.
	 */
	for (ifa = in6_ifaddr; ifa; ifa = ifa->ia_next) {
		if (!(ifa->ia6_flags & IN6_IFF_AUTOCONF))
			continue;

		if (ifa->ia6_ndpr == NULL) {
			/*
			 * This can happen when we first configure the address
			 * (i.e. the address exists, but the prefix does not).
			 * XXX: complicated relationships...
			 */
			continue;
		}

		if (find_pfxlist_reachable_router(ifa->ia6_ndpr))
			break;
	}
	if (ifa) {
		for (ifa = in6_ifaddr; ifa; ifa = ifa->ia_next) {
			if ((ifa->ia6_flags & IN6_IFF_AUTOCONF) == 0)
				continue;

			if (ifa->ia6_ndpr == NULL) /* XXX: see above. */
				continue;

			if (find_pfxlist_reachable_router(ifa->ia6_ndpr)) {
				if (ifa->ia6_flags & IN6_IFF_DETACHED) {
					ifa->ia6_flags &= ~IN6_IFF_DETACHED;
					ifa->ia6_flags |= IN6_IFF_TENTATIVE;
					nd6_dad_start((struct ifaddr *)ifa,
					    0);
					/* We will notify the routing socket
					 * of the DAD result, so no need to
					 * here */
				}
			} else {
				if ((ifa->ia6_flags & IN6_IFF_DETACHED) == 0) {
					ifa->ia6_flags |= IN6_IFF_DETACHED;
					rt_newaddrmsg(RTM_NEWADDR,
					    (struct ifaddr *)ifa, 0, NULL);
				}
			}
		}
	}
	else {
		for (ifa = in6_ifaddr; ifa; ifa = ifa->ia_next) {
			if ((ifa->ia6_flags & IN6_IFF_AUTOCONF) == 0)
				continue;

			if (ifa->ia6_flags & IN6_IFF_DETACHED) {
				ifa->ia6_flags &= ~IN6_IFF_DETACHED;
				ifa->ia6_flags |= IN6_IFF_TENTATIVE;
				/* Do we need a delay in this case? */
				nd6_dad_start((struct ifaddr *)ifa, 0);
			}
		}
	}
}

int
nd6_prefix_onlink(struct nd_prefix *pr)
{
	struct ifaddr *ifa;
	struct ifnet *ifp = pr->ndpr_ifp;
	struct sockaddr_in6 mask6;
	struct nd_prefix *opr;
	u_long rtflags;
	int error = 0;

	/* sanity check */
	if ((pr->ndpr_stateflags & NDPRF_ONLINK) != 0) {
		nd6log((LOG_ERR,
		    "nd6_prefix_onlink: %s/%d is already on-link\n",
		    ip6_sprintf(&pr->ndpr_prefix.sin6_addr), pr->ndpr_plen));
		return (EEXIST);
	}

	/*
	 * Add the interface route associated with the prefix.  Before
	 * installing the route, check if there's the same prefix on another
	 * interface, and the prefix has already installed the interface route.
	 * Although such a configuration is expected to be rare, we explicitly
	 * allow it.
	 */
	LIST_FOREACH(opr, &nd_prefix, ndpr_entry) {
		if (opr == pr)
			continue;

		if ((opr->ndpr_stateflags & NDPRF_ONLINK) == 0)
			continue;

		if (opr->ndpr_plen == pr->ndpr_plen &&
		    in6_are_prefix_equal(&pr->ndpr_prefix.sin6_addr,
		    &opr->ndpr_prefix.sin6_addr, pr->ndpr_plen))
			return (0);
	}

	/*
	 * We prefer link-local addresses as the associated interface address.
	 */
	/* search for a link-local addr */
	ifa = (struct ifaddr *)in6ifa_ifpforlinklocal(ifp,
	    IN6_IFF_NOTREADY | IN6_IFF_ANYCAST);
	if (ifa == NULL) {
		/* XXX: freebsd does not have ifa_ifwithaf */
		IFADDR_FOREACH(ifa, ifp) {
			if (ifa->ifa_addr->sa_family == AF_INET6)
				break;
		}
		/* should we care about ia6_flags? */
	}
	if (ifa == NULL) {
		/*
		 * This can still happen, when, for example, we receive an RA
		 * containing a prefix with the L bit set and the A bit clear,
		 * after removing all IPv6 addresses on the receiving
		 * interface.  This should, of course, be rare though.
		 */
		nd6log((LOG_NOTICE,
		    "nd6_prefix_onlink: failed to find any ifaddr"
		    " to add route for a prefix(%s/%d) on %s\n",
		    ip6_sprintf(&pr->ndpr_prefix.sin6_addr),
		    pr->ndpr_plen, if_name(ifp)));
		return (0);
	}

	/*
	 * in6_ifinit() sets nd6_rtrequest to ifa_rtrequest for all ifaddrs.
	 * ifa->ifa_rtrequest = nd6_rtrequest;
	 */
	memset(&mask6, 0, sizeof(mask6));
	mask6.sin6_family = AF_INET6;
	mask6.sin6_len = sizeof(mask6);
	mask6.sin6_addr = pr->ndpr_mask;
	/* rtrequest() will probably set RTF_UP, but we're not sure. */
	rtflags = ifa->ifa_flags | RTF_UP;
	if (nd6_need_cache(ifp)) {
		/* explicitly set in case ifa_flags does not set the flag. */
		rtflags |= RTF_CLONING;
	} else {
		/*
		 * explicitly clear the cloning bit in case ifa_flags sets it.
		 */
		rtflags &= ~RTF_CLONING;
	}
	error = rtrequest_newmsg(RTM_ADD, (struct sockaddr *)&pr->ndpr_prefix,
	    ifa->ifa_addr, (struct sockaddr *)&mask6, rtflags);
	if (error == 0) {
		nd6_numroutes++;
		pr->ndpr_stateflags |= NDPRF_ONLINK;
	} else {
		nd6log((LOG_ERR, "nd6_prefix_onlink: failed to add route for a"
		    " prefix (%s/%d) on %s, gw=%s, mask=%s, flags=%lx "
		    "errno = %d\n",
		    ip6_sprintf(&pr->ndpr_prefix.sin6_addr),
		    pr->ndpr_plen, if_name(ifp),
		    ip6_sprintf(&((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr),
		    ip6_sprintf(&mask6.sin6_addr), rtflags, error));
	}

	return (error);
}

int
nd6_prefix_offlink(struct nd_prefix *pr)
{
	int error = 0;
	struct ifnet *ifp = pr->ndpr_ifp;
	struct nd_prefix *opr;
	struct sockaddr_in6 sa6, mask6;

	/* sanity check */
	if ((pr->ndpr_stateflags & NDPRF_ONLINK) == 0) {
		nd6log((LOG_ERR,
		    "nd6_prefix_offlink: %s/%d is already off-link\n",
		    ip6_sprintf(&pr->ndpr_prefix.sin6_addr), pr->ndpr_plen));
		return (EEXIST);
	}

	sockaddr_in6_init(&sa6, &pr->ndpr_prefix.sin6_addr, 0, 0, 0);
	sockaddr_in6_init(&mask6, &pr->ndpr_mask, 0, 0, 0);
	error = rtrequest_newmsg(RTM_DELETE, (struct sockaddr *)&sa6, NULL,
	    (struct sockaddr *)&mask6, 0);
	if (error == 0) {
		pr->ndpr_stateflags &= ~NDPRF_ONLINK;
		nd6_numroutes--;

		/*
		 * There might be the same prefix on another interface,
		 * the prefix which could not be on-link just because we have
		 * the interface route (see comments in nd6_prefix_onlink).
		 * If there's one, try to make the prefix on-link on the
		 * interface.
		 */
		LIST_FOREACH(opr, &nd_prefix, ndpr_entry) {
			if (opr == pr)
				continue;

			if ((opr->ndpr_stateflags & NDPRF_ONLINK) != 0)
				continue;

			/*
			 * KAME specific: detached prefixes should not be
			 * on-link.
			 */
			if ((opr->ndpr_stateflags & NDPRF_DETACHED) != 0)
				continue;

			if (opr->ndpr_plen == pr->ndpr_plen &&
			    in6_are_prefix_equal(&pr->ndpr_prefix.sin6_addr,
			    &opr->ndpr_prefix.sin6_addr, pr->ndpr_plen)) {
				int e;

				if ((e = nd6_prefix_onlink(opr)) != 0) {
					nd6log((LOG_ERR,
					    "nd6_prefix_offlink: failed to "
					    "recover a prefix %s/%d from %s "
					    "to %s (errno = %d)\n",
					    ip6_sprintf(&opr->ndpr_prefix.sin6_addr),
					    opr->ndpr_plen, if_name(ifp),
					    if_name(opr->ndpr_ifp), e));
				}
			}
		}
	} else {
		/* XXX: can we still set the NDPRF_ONLINK flag? */
		nd6log((LOG_ERR,
		    "nd6_prefix_offlink: failed to delete route: "
		    "%s/%d on %s (errno = %d)\n",
		    ip6_sprintf(&sa6.sin6_addr), pr->ndpr_plen, if_name(ifp),
		    error));
	}

	return error;
}

static struct in6_ifaddr *
in6_ifadd(struct nd_prefixctl *prc, int mcast)
{
	struct ifnet *ifp = prc->ndprc_ifp;
	struct ifaddr *ifa;
	struct in6_aliasreq ifra;
	struct in6_ifaddr *ia, *ib;
	int error, plen0;
	struct in6_addr mask;
	int prefixlen = prc->ndprc_plen;
	int updateflags;

	in6_prefixlen2mask(&mask, prefixlen);

	/*
	 * find a link-local address (will be interface ID).
	 * Is it really mandatory? Theoretically, a global or a site-local
	 * address can be configured without a link-local address, if we
	 * have a unique interface identifier...
	 *
	 * it is not mandatory to have a link-local address, we can generate
	 * interface identifier on the fly.  we do this because:
	 * (1) it should be the easiest way to find interface identifier.
	 * (2) RFC2462 5.4 suggesting the use of the same interface identifier
	 * for multiple addresses on a single interface, and possible shortcut
	 * of DAD.  we omitted DAD for this reason in the past.
	 * (3) a user can prevent autoconfiguration of global address
	 * by removing link-local address by hand (this is partly because we
	 * don't have other way to control the use of IPv6 on an interface.
	 * this has been our design choice - cf. NRL's "ifconfig auto").
	 * (4) it is easier to manage when an interface has addresses
	 * with the same interface identifier, than to have multiple addresses
	 * with different interface identifiers.
	 */
	ifa = (struct ifaddr *)in6ifa_ifpforlinklocal(ifp, 0); /* 0 is OK? */
	if (ifa)
		ib = (struct in6_ifaddr *)ifa;
	else
		return NULL;

#if 0 /* don't care link local addr state, and always do DAD */
	/* if link-local address is not eligible, do not autoconfigure. */
	if (((struct in6_ifaddr *)ifa)->ia6_flags & IN6_IFF_NOTREADY) {
		printf("in6_ifadd: link-local address not ready\n");
		return NULL;
	}
#endif

	/* prefixlen + ifidlen must be equal to 128 */
	plen0 = in6_mask2len(&ib->ia_prefixmask.sin6_addr, NULL);
	if (prefixlen != plen0) {
		nd6log((LOG_INFO, "in6_ifadd: wrong prefixlen for %s "
		    "(prefix=%d ifid=%d)\n",
		    if_name(ifp), prefixlen, 128 - plen0));
		return NULL;
	}

	/* make ifaddr */

	memset(&ifra, 0, sizeof(ifra));
	/*
	 * in6_update_ifa() does not use ifra_name, but we accurately set it
	 * for safety.
	 */
	strncpy(ifra.ifra_name, if_name(ifp), sizeof(ifra.ifra_name));
	sockaddr_in6_init(&ifra.ifra_addr, &prc->ndprc_prefix.sin6_addr, 0, 0, 0);
	/* prefix */
	ifra.ifra_addr.sin6_addr.s6_addr32[0] &= mask.s6_addr32[0];
	ifra.ifra_addr.sin6_addr.s6_addr32[1] &= mask.s6_addr32[1];
	ifra.ifra_addr.sin6_addr.s6_addr32[2] &= mask.s6_addr32[2];
	ifra.ifra_addr.sin6_addr.s6_addr32[3] &= mask.s6_addr32[3];

	/* interface ID */
	ifra.ifra_addr.sin6_addr.s6_addr32[0] |=
	    (ib->ia_addr.sin6_addr.s6_addr32[0] & ~mask.s6_addr32[0]);
	ifra.ifra_addr.sin6_addr.s6_addr32[1] |=
	    (ib->ia_addr.sin6_addr.s6_addr32[1] & ~mask.s6_addr32[1]);
	ifra.ifra_addr.sin6_addr.s6_addr32[2] |=
	    (ib->ia_addr.sin6_addr.s6_addr32[2] & ~mask.s6_addr32[2]);
	ifra.ifra_addr.sin6_addr.s6_addr32[3] |=
	    (ib->ia_addr.sin6_addr.s6_addr32[3] & ~mask.s6_addr32[3]);

	/* new prefix mask. */
	sockaddr_in6_init(&ifra.ifra_prefixmask, &mask, 0, 0, 0);

	/* lifetimes */
	ifra.ifra_lifetime.ia6t_vltime = prc->ndprc_vltime;
	ifra.ifra_lifetime.ia6t_pltime = prc->ndprc_pltime;

	/* XXX: scope zone ID? */

	ifra.ifra_flags |= IN6_IFF_AUTOCONF; /* obey autoconf */

	/*
	 * Make sure that we do not have this address already.  This should
	 * usually not happen, but we can still see this case, e.g., if we
	 * have manually configured the exact address to be configured.
	 */
	if (in6ifa_ifpwithaddr(ifp, &ifra.ifra_addr.sin6_addr) != NULL) {
		/* this should be rare enough to make an explicit log */
		log(LOG_INFO, "in6_ifadd: %s is already configured\n",
		    ip6_sprintf(&ifra.ifra_addr.sin6_addr));
		return (NULL);
	}

	/*
	 * Allocate ifaddr structure, link into chain, etc.
	 * If we are going to create a new address upon receiving a multicasted
	 * RA, we need to impose a random delay before starting DAD.
	 * [draft-ietf-ipv6-rfc2462bis-02.txt, Section 5.4.2]
	 */
	updateflags = 0;
	if (mcast)
		updateflags |= IN6_IFAUPDATE_DADDELAY;
	if ((error = in6_update_ifa(ifp, &ifra, NULL, updateflags)) != 0) {
		nd6log((LOG_ERR,
		    "in6_ifadd: failed to make ifaddr %s on %s (errno=%d)\n",
		    ip6_sprintf(&ifra.ifra_addr.sin6_addr), if_name(ifp),
		    error));
		return (NULL);	/* ifaddr must not have been allocated. */
	}

	ia = in6ifa_ifpwithaddr(ifp, &ifra.ifra_addr.sin6_addr);

	return (ia);		/* this is always non-NULL */
}

int
in6_tmpifadd(
	const struct in6_ifaddr *ia0, /* corresponding public address */
	int forcegen, 
	int dad_delay)
{
	struct ifnet *ifp = ia0->ia_ifa.ifa_ifp;
	struct in6_ifaddr *newia, *ia;
	struct in6_aliasreq ifra;
	int i, error;
	int trylimit = 3;	/* XXX: adhoc value */
	int updateflags;
	u_int32_t randid[2];
	u_int32_t vltime0, pltime0;

	memset(&ifra, 0, sizeof(ifra));
	strncpy(ifra.ifra_name, if_name(ifp), sizeof(ifra.ifra_name));
	ifra.ifra_addr = ia0->ia_addr;
	/* copy prefix mask */
	ifra.ifra_prefixmask = ia0->ia_prefixmask;
	/* clear the old IFID */
	for (i = 0; i < 4; i++) {
		ifra.ifra_addr.sin6_addr.s6_addr32[i] &=
		    ifra.ifra_prefixmask.sin6_addr.s6_addr32[i];
	}

  again:
	if (in6_get_tmpifid(ifp, (u_int8_t *)randid,
	    (const u_int8_t *)&ia0->ia_addr.sin6_addr.s6_addr[8], forcegen)) {
		nd6log((LOG_NOTICE, "in6_tmpifadd: failed to find a good "
		    "random IFID\n"));
		return (EINVAL);
	}
	ifra.ifra_addr.sin6_addr.s6_addr32[2] |=
	    (randid[0] & ~(ifra.ifra_prefixmask.sin6_addr.s6_addr32[2]));
	ifra.ifra_addr.sin6_addr.s6_addr32[3] |=
	    (randid[1] & ~(ifra.ifra_prefixmask.sin6_addr.s6_addr32[3]));

	/*
	 * in6_get_tmpifid() quite likely provided a unique interface ID.
	 * However, we may still have a chance to see collision, because
	 * there may be a time lag between generation of the ID and generation
	 * of the address.  So, we'll do one more sanity check.
	 */
	for (ia = in6_ifaddr; ia; ia = ia->ia_next) {
		if (IN6_ARE_ADDR_EQUAL(&ia->ia_addr.sin6_addr,
		    &ifra.ifra_addr.sin6_addr)) {
			if (trylimit-- == 0) {
				/*
				 * Give up.  Something strange should have
				 * happened.
				 */
				nd6log((LOG_NOTICE, "in6_tmpifadd: failed to "
				    "find a unique random IFID\n"));
				return (EEXIST);
			}
			forcegen = 1;
			goto again;
		}
	}

	/*
	 * The Valid Lifetime is the lower of the Valid Lifetime of the
         * public address or TEMP_VALID_LIFETIME.
	 * The Preferred Lifetime is the lower of the Preferred Lifetime
         * of the public address or TEMP_PREFERRED_LIFETIME -
         * DESYNC_FACTOR.
	 */
	if (ia0->ia6_lifetime.ia6t_vltime != ND6_INFINITE_LIFETIME) {
		vltime0 = IFA6_IS_INVALID(ia0) ? 0 :
		    (ia0->ia6_lifetime.ia6t_vltime -
		    (time_uptime - ia0->ia6_updatetime));
		if (vltime0 > ip6_temp_valid_lifetime)
			vltime0 = ip6_temp_valid_lifetime;
	} else
		vltime0 = ip6_temp_valid_lifetime;
	if (ia0->ia6_lifetime.ia6t_pltime != ND6_INFINITE_LIFETIME) {
		pltime0 = IFA6_IS_DEPRECATED(ia0) ? 0 :
		    (ia0->ia6_lifetime.ia6t_pltime -
		    (time_uptime - ia0->ia6_updatetime));
		if (pltime0 > ip6_temp_preferred_lifetime - ip6_desync_factor){
			pltime0 = ip6_temp_preferred_lifetime -
			    ip6_desync_factor;
		}
	} else
		pltime0 = ip6_temp_preferred_lifetime - ip6_desync_factor;
	ifra.ifra_lifetime.ia6t_vltime = vltime0;
	ifra.ifra_lifetime.ia6t_pltime = pltime0;

	/*
	 * A temporary address is created only if this calculated Preferred
	 * Lifetime is greater than REGEN_ADVANCE time units.
	 */
	if (ifra.ifra_lifetime.ia6t_pltime <= ip6_temp_regen_advance)
		return (0);

	/* XXX: scope zone ID? */

	ifra.ifra_flags |= (IN6_IFF_AUTOCONF|IN6_IFF_TEMPORARY);

	/* allocate ifaddr structure, link into chain, etc. */
	updateflags = 0;
	if (dad_delay)
		updateflags |= IN6_IFAUPDATE_DADDELAY;
	if ((error = in6_update_ifa(ifp, &ifra, NULL, updateflags)) != 0)
		return (error);

	newia = in6ifa_ifpwithaddr(ifp, &ifra.ifra_addr.sin6_addr);
	if (newia == NULL) {	/* XXX: can it happen? */
		nd6log((LOG_ERR,
		    "in6_tmpifadd: ifa update succeeded, but we got "
		    "no ifaddr\n"));
		return (EINVAL); /* XXX */
	}
	newia->ia6_ndpr = ia0->ia6_ndpr;
	newia->ia6_ndpr->ndpr_refcnt++;

	/*
	 * A newly added address might affect the status of other addresses.
	 * XXX: when the temporary address is generated with a new public
	 * address, the onlink check is redundant.  However, it would be safe
	 * to do the check explicitly everywhere a new address is generated,
	 * and, in fact, we surely need the check when we create a new
	 * temporary address due to deprecation of an old temporary address.
	 */
	pfxlist_onlink_check();

	return (0);
}

static int
in6_init_prefix_ltimes(struct nd_prefix *ndpr)
{

	/* check if preferred lifetime > valid lifetime.  RFC2462 5.5.3 (c) */
	if (ndpr->ndpr_pltime > ndpr->ndpr_vltime) {
		nd6log((LOG_INFO, "in6_init_prefix_ltimes: preferred lifetime"
		    "(%d) is greater than valid lifetime(%d)\n",
		    (u_int)ndpr->ndpr_pltime, (u_int)ndpr->ndpr_vltime));
		return (EINVAL);
	}
	if (ndpr->ndpr_pltime == ND6_INFINITE_LIFETIME)
		ndpr->ndpr_preferred = 0;
	else
		ndpr->ndpr_preferred = time_uptime + ndpr->ndpr_pltime;
	if (ndpr->ndpr_vltime == ND6_INFINITE_LIFETIME)
		ndpr->ndpr_expire = 0;
	else
		ndpr->ndpr_expire = time_uptime + ndpr->ndpr_vltime;

	return 0;
}

static void
in6_init_address_ltimes(struct nd_prefix *newpr,
    struct in6_addrlifetime *lt6)
{

	/* Valid lifetime must not be updated unless explicitly specified. */
	/* init ia6t_expire */
	if (lt6->ia6t_vltime == ND6_INFINITE_LIFETIME)
		lt6->ia6t_expire = 0;
	else {
		lt6->ia6t_expire = time_uptime;
		lt6->ia6t_expire += lt6->ia6t_vltime;
	}

	/* init ia6t_preferred */
	if (lt6->ia6t_pltime == ND6_INFINITE_LIFETIME)
		lt6->ia6t_preferred = 0;
	else {
		lt6->ia6t_preferred = time_uptime;
		lt6->ia6t_preferred += lt6->ia6t_pltime;
	}
}

/*
 * Delete all the routing table entries that use the specified gateway.
 * XXX: this function causes search through all entries of routing table, so
 * it shouldn't be called when acting as a router.
 */
void
rt6_flush(struct in6_addr *gateway, struct ifnet *ifp)
{
	int s = splsoftnet();

	/* We'll care only link-local addresses */
	if (!IN6_IS_ADDR_LINKLOCAL(gateway)) {
		splx(s);
		return;
	}

	rt_walktree(AF_INET6, rt6_deleteroute, (void *)gateway);
	splx(s);
}

static int
rt6_deleteroute(struct rtentry *rt, void *arg)
{
	struct in6_addr *gate = (struct in6_addr *)arg;

	if (rt->rt_gateway == NULL || rt->rt_gateway->sa_family != AF_INET6)
		return (0);

	if (!IN6_ARE_ADDR_EQUAL(gate, &satosin6(rt->rt_gateway)->sin6_addr))
		return (0);

	/*
	 * Do not delete a static route.
	 * XXX: this seems to be a bit ad-hoc. Should we consider the
	 * 'cloned' bit instead?
	 */
	if ((rt->rt_flags & RTF_STATIC) != 0)
		return (0);

	/*
	 * We delete only host route. This means, in particular, we don't
	 * delete default route.
	 */
	if ((rt->rt_flags & RTF_HOST) == 0)
		return (0);

	return (rtrequest(RTM_DELETE, rt_getkey(rt), rt->rt_gateway,
	    rt_mask(rt), rt->rt_flags, NULL));
}

int
nd6_setdefaultiface(int ifindex)
{
	ifnet_t *ifp;
	int error = 0;

	if ((ifp = if_byindex(ifindex)) == NULL) {
		return EINVAL;
	}
	if (nd6_defifindex != ifindex) {
		nd6_defifindex = ifindex;
		nd6_defifp = nd6_defifindex > 0 ? ifp : NULL;

		/*
		 * Our current implementation assumes one-to-one maping between
		 * interfaces and links, so it would be natural to use the
		 * default interface as the default link.
		 */
		scope6_setdefault(nd6_defifp);
	}

	return (error);
}
