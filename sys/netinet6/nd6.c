/*	$NetBSD: nd6.c,v 1.177 2015/09/11 10:33:32 roy Exp $	*/
/*	$KAME: nd6.c,v 1.279 2002/06/08 11:16:51 itojun Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: nd6.c,v 1.177 2015/09/11 10:33:32 roy Exp $");

#ifdef _KERNEL_OPT
#include "opt_net_mpsafe.h"
#endif

#include "bridge.h"
#include "carp.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sockio.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/protosw.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/queue.h>
#include <sys/cprng.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/if_ether.h>
#include <net/if_fddi.h>
#include <net/if_arc.h>

#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/scope6_var.h>
#include <netinet6/nd6.h>
#include <netinet6/in6_ifattach.h>
#include <netinet/icmp6.h>
#include <netinet6/icmp6_private.h>

#include <net/net_osdep.h>

#define ND6_SLOWTIMER_INTERVAL (60 * 60) /* 1 hour */
#define ND6_RECALC_REACHTM_INTERVAL (60 * 120) /* 2 hours */

/* timer values */
int	nd6_prune	= 1;	/* walk list every 1 seconds */
int	nd6_delay	= 5;	/* delay first probe time 5 second */
int	nd6_umaxtries	= 3;	/* maximum unicast query */
int	nd6_mmaxtries	= 3;	/* maximum multicast query */
int	nd6_useloopback = 1;	/* use loopback interface for local traffic */
int	nd6_gctimer	= (60 * 60 * 24); /* 1 day: garbage collection timer */

/* preventing too many loops in ND option parsing */
int nd6_maxndopt = 10;	/* max # of ND options allowed */

int nd6_maxnudhint = 0;	/* max # of subsequent upper layer hints */

int nd6_maxqueuelen = 1; /* max # of packets cached in unresolved ND entries */

#ifdef ND6_DEBUG
int nd6_debug = 1;
#else
int nd6_debug = 0;
#endif

/* for debugging? */
static int nd6_inuse, nd6_allocated;

struct llinfo_nd6 llinfo_nd6 = {
	.ln_prev = &llinfo_nd6,
	.ln_next = &llinfo_nd6,
};
struct nd_drhead nd_defrouter;
struct nd_prhead nd_prefix = { 0 };

int nd6_recalc_reachtm_interval = ND6_RECALC_REACHTM_INTERVAL;
static const struct sockaddr_in6 all1_sa = {
	  .sin6_family = AF_INET6
	, .sin6_len = sizeof(struct sockaddr_in6)
	, .sin6_addr = {.s6_addr = {0xff, 0xff, 0xff, 0xff,
				    0xff, 0xff, 0xff, 0xff,
				    0xff, 0xff, 0xff, 0xff,
				    0xff, 0xff, 0xff, 0xff}}
};

static void nd6_setmtu0(struct ifnet *, struct nd_ifinfo *);
static void nd6_slowtimo(void *);
static int regen_tmpaddr(struct in6_ifaddr *);
static struct llinfo_nd6 *nd6_free(struct rtentry *, int);
static void nd6_llinfo_timer(void *);
static void clear_llinfo_pqueue(struct llinfo_nd6 *);

callout_t nd6_slowtimo_ch;
callout_t nd6_timer_ch;
extern callout_t in6_tmpaddrtimer_ch;

static int fill_drlist(void *, size_t *, size_t);
static int fill_prlist(void *, size_t *, size_t);

MALLOC_DEFINE(M_IP6NDP, "NDP", "IPv6 Neighbour Discovery");

#define LN_DEQUEUE(ln) do { \
	(ln)->ln_next->ln_prev = (ln)->ln_prev; \
	(ln)->ln_prev->ln_next = (ln)->ln_next; \
	} while (/*CONSTCOND*/0)
#define LN_INSERTHEAD(ln) do { \
	(ln)->ln_next = llinfo_nd6.ln_next; \
	llinfo_nd6.ln_next = (ln); \
	(ln)->ln_prev = &llinfo_nd6; \
	(ln)->ln_next->ln_prev = (ln); \
	} while (/*CONSTCOND*/0)
void
nd6_init(void)
{
	static int nd6_init_done = 0;

	if (nd6_init_done) {
		log(LOG_NOTICE, "nd6_init called more than once(ignored)\n");
		return;
	}

	/* initialization of the default router list */
	TAILQ_INIT(&nd_defrouter);

	nd6_init_done = 1;

	callout_init(&nd6_slowtimo_ch, CALLOUT_MPSAFE);
	callout_init(&nd6_timer_ch, CALLOUT_MPSAFE);

	/* start timer */
	callout_reset(&nd6_slowtimo_ch, ND6_SLOWTIMER_INTERVAL * hz,
	    nd6_slowtimo, NULL);
}

struct nd_ifinfo *
nd6_ifattach(struct ifnet *ifp)
{
	struct nd_ifinfo *nd;

	nd = (struct nd_ifinfo *)malloc(sizeof(*nd), M_IP6NDP, M_WAITOK|M_ZERO);

	nd->initialized = 1;

	nd->chlim = IPV6_DEFHLIM;
	nd->basereachable = REACHABLE_TIME;
	nd->reachable = ND_COMPUTE_RTIME(nd->basereachable);
	nd->retrans = RETRANS_TIMER;

	nd->flags = ND6_IFF_PERFORMNUD | ND6_IFF_ACCEPT_RTADV;

	/* A loopback interface always has ND6_IFF_AUTO_LINKLOCAL.
	 * A bridge interface should not have ND6_IFF_AUTO_LINKLOCAL
	 * because one of its members should. */
	if ((ip6_auto_linklocal && ifp->if_type != IFT_BRIDGE) ||
	    (ifp->if_flags & IFF_LOOPBACK))
		nd->flags |= ND6_IFF_AUTO_LINKLOCAL;

	/* A loopback interface does not need to accept RTADV.
	 * A bridge interface should not accept RTADV
	 * because one of its members should. */
	if (ip6_accept_rtadv &&
	    !(ifp->if_flags & IFF_LOOPBACK) &&
	    !(ifp->if_type != IFT_BRIDGE))
		nd->flags |= ND6_IFF_ACCEPT_RTADV;

	/* XXX: we cannot call nd6_setmtu since ifp is not fully initialized */
	nd6_setmtu0(ifp, nd);

	return nd;
}

void
nd6_ifdetach(struct ifnet *ifp, struct in6_ifextra *ext)
{

	nd6_purge(ifp, ext);
	free(ext->nd_ifinfo, M_IP6NDP);
}

void
nd6_setmtu(struct ifnet *ifp)
{
	nd6_setmtu0(ifp, ND_IFINFO(ifp));
}

void
nd6_setmtu0(struct ifnet *ifp, struct nd_ifinfo *ndi)
{
	u_int32_t omaxmtu;

	omaxmtu = ndi->maxmtu;

	switch (ifp->if_type) {
	case IFT_ARCNET:
		ndi->maxmtu = MIN(ARC_PHDS_MAXMTU, ifp->if_mtu); /* RFC2497 */
		break;
	case IFT_FDDI:
		ndi->maxmtu = MIN(FDDIIPMTU, ifp->if_mtu);
		break;
	default:
		ndi->maxmtu = ifp->if_mtu;
		break;
	}

	/*
	 * Decreasing the interface MTU under IPV6 minimum MTU may cause
	 * undesirable situation.  We thus notify the operator of the change
	 * explicitly.  The check for omaxmtu is necessary to restrict the
	 * log to the case of changing the MTU, not initializing it.
	 */
	if (omaxmtu >= IPV6_MMTU && ndi->maxmtu < IPV6_MMTU) {
		log(LOG_NOTICE, "nd6_setmtu0: new link MTU on %s (%lu) is too"
		    " small for IPv6 which needs %lu\n",
		    if_name(ifp), (unsigned long)ndi->maxmtu, (unsigned long)
		    IPV6_MMTU);
	}

	if (ndi->maxmtu > in6_maxmtu)
		in6_setmaxmtu(); /* check all interfaces just in case */
}

void
nd6_option_init(void *opt, int icmp6len, union nd_opts *ndopts)
{

	memset(ndopts, 0, sizeof(*ndopts));
	ndopts->nd_opts_search = (struct nd_opt_hdr *)opt;
	ndopts->nd_opts_last
		= (struct nd_opt_hdr *)(((u_char *)opt) + icmp6len);

	if (icmp6len == 0) {
		ndopts->nd_opts_done = 1;
		ndopts->nd_opts_search = NULL;
	}
}

/*
 * Take one ND option.
 */
struct nd_opt_hdr *
nd6_option(union nd_opts *ndopts)
{
	struct nd_opt_hdr *nd_opt;
	int olen;

	KASSERT(ndopts != NULL);
	KASSERT(ndopts->nd_opts_last != NULL);

	if (ndopts->nd_opts_search == NULL)
		return NULL;
	if (ndopts->nd_opts_done)
		return NULL;

	nd_opt = ndopts->nd_opts_search;

	/* make sure nd_opt_len is inside the buffer */
	if ((void *)&nd_opt->nd_opt_len >= (void *)ndopts->nd_opts_last) {
		memset(ndopts, 0, sizeof(*ndopts));
		return NULL;
	}

	olen = nd_opt->nd_opt_len << 3;
	if (olen == 0) {
		/*
		 * Message validation requires that all included
		 * options have a length that is greater than zero.
		 */
		memset(ndopts, 0, sizeof(*ndopts));
		return NULL;
	}

	ndopts->nd_opts_search = (struct nd_opt_hdr *)((char *)nd_opt + olen);
	if (ndopts->nd_opts_search > ndopts->nd_opts_last) {
		/* option overruns the end of buffer, invalid */
		memset(ndopts, 0, sizeof(*ndopts));
		return NULL;
	} else if (ndopts->nd_opts_search == ndopts->nd_opts_last) {
		/* reached the end of options chain */
		ndopts->nd_opts_done = 1;
		ndopts->nd_opts_search = NULL;
	}
	return nd_opt;
}

/*
 * Parse multiple ND options.
 * This function is much easier to use, for ND routines that do not need
 * multiple options of the same type.
 */
int
nd6_options(union nd_opts *ndopts)
{
	struct nd_opt_hdr *nd_opt;
	int i = 0;

	KASSERT(ndopts != NULL);
	KASSERT(ndopts->nd_opts_last != NULL);

	if (ndopts->nd_opts_search == NULL)
		return 0;
 
	while (1) {
		nd_opt = nd6_option(ndopts);
		if (nd_opt == NULL && ndopts->nd_opts_last == NULL) {
			/*
			 * Message validation requires that all included
			 * options have a length that is greater than zero.
			 */
			ICMP6_STATINC(ICMP6_STAT_ND_BADOPT);
			memset(ndopts, 0, sizeof(*ndopts));
			return -1;
		}

		if (nd_opt == NULL)
			goto skip1;

		switch (nd_opt->nd_opt_type) {
		case ND_OPT_SOURCE_LINKADDR:
		case ND_OPT_TARGET_LINKADDR:
		case ND_OPT_MTU:
		case ND_OPT_REDIRECTED_HEADER:
			if (ndopts->nd_opt_array[nd_opt->nd_opt_type]) {
				nd6log((LOG_INFO,
				    "duplicated ND6 option found (type=%d)\n",
				    nd_opt->nd_opt_type));
				/* XXX bark? */
			} else {
				ndopts->nd_opt_array[nd_opt->nd_opt_type]
					= nd_opt;
			}
			break;
		case ND_OPT_PREFIX_INFORMATION:
			if (ndopts->nd_opt_array[nd_opt->nd_opt_type] == 0) {
				ndopts->nd_opt_array[nd_opt->nd_opt_type]
					= nd_opt;
			}
			ndopts->nd_opts_pi_end =
				(struct nd_opt_prefix_info *)nd_opt;
			break;
		default:
			/*
			 * Unknown options must be silently ignored,
			 * to accommodate future extension to the protocol.
			 */
			nd6log((LOG_DEBUG,
			    "nd6_options: unsupported option %d - "
			    "option ignored\n", nd_opt->nd_opt_type));
		}

skip1:
		i++;
		if (i > nd6_maxndopt) {
			ICMP6_STATINC(ICMP6_STAT_ND_TOOMANYOPT);
			nd6log((LOG_INFO, "too many loop in nd opt\n"));
			break;
		}

		if (ndopts->nd_opts_done)
			break;
	}

	return 0;
}

/*
 * ND6 timer routine to handle ND6 entries
 */
void
nd6_llinfo_settimer(struct llinfo_nd6 *ln, long xtick)
{
	int s;

	s = splsoftnet();

	if (xtick < 0) {
		ln->ln_expire = 0;
		ln->ln_ntick = 0;
		callout_stop(&ln->ln_timer_ch);
	} else {
		ln->ln_expire = time_uptime + xtick / hz;
		if (xtick > INT_MAX) {
			ln->ln_ntick = xtick - INT_MAX;
			callout_reset(&ln->ln_timer_ch, INT_MAX,
			    nd6_llinfo_timer, ln);
		} else {
			ln->ln_ntick = 0;
			callout_reset(&ln->ln_timer_ch, xtick,
			    nd6_llinfo_timer, ln);
		}
	}

	splx(s);
}

static void
nd6_llinfo_timer(void *arg)
{
	struct llinfo_nd6 *ln;
	struct rtentry *rt;
	const struct sockaddr_in6 *dst;
	struct ifnet *ifp;
	struct nd_ifinfo *ndi = NULL;

	mutex_enter(softnet_lock);
	KERNEL_LOCK(1, NULL);

	ln = (struct llinfo_nd6 *)arg;

	if (ln->ln_ntick > 0) {
		nd6_llinfo_settimer(ln, ln->ln_ntick);
		KERNEL_UNLOCK_ONE(NULL);
		mutex_exit(softnet_lock);
		return;
	}

	rt = ln->ln_rt;
	KASSERT(rt != NULL);
	ifp = rt->rt_ifp;
	KASSERT(ifp != NULL);

	ndi = ND_IFINFO(ifp);
	dst = satocsin6(rt_getkey(rt));

	/* sanity check */
	if (rt->rt_llinfo && (struct llinfo_nd6 *)rt->rt_llinfo != ln)
		panic("rt_llinfo(%p) is not equal to ln(%p)",
		      rt->rt_llinfo, ln);
	if (!dst)
		panic("dst=0 in nd6_timer(ln=%p)", ln);

	switch (ln->ln_state) {
	case ND6_LLINFO_INCOMPLETE:
		if (ln->ln_asked < nd6_mmaxtries) {
			ln->ln_asked++;
			nd6_llinfo_settimer(ln, (long)ndi->retrans * hz / 1000);
			nd6_ns_output(ifp, NULL, &dst->sin6_addr, ln, 0);
		} else {
			struct mbuf *m = ln->ln_hold;
			if (m) {
				struct mbuf *m0;

				/*
				 * assuming every packet in ln_hold has
				 * the same IP header
				 */
				m0 = m->m_nextpkt;
				m->m_nextpkt = NULL;
				icmp6_error2(m, ICMP6_DST_UNREACH,
				    ICMP6_DST_UNREACH_ADDR, 0, rt->rt_ifp);

				ln->ln_hold = m0;
				clear_llinfo_pqueue(ln);
 			}
			(void)nd6_free(rt, 0);
			ln = NULL;
		}
		break;
	case ND6_LLINFO_REACHABLE:
		if (!ND6_LLINFO_PERMANENT(ln)) {
			ln->ln_state = ND6_LLINFO_STALE;
			nd6_llinfo_settimer(ln, (long)nd6_gctimer * hz);
		}
		break;

	case ND6_LLINFO_PURGE:
	case ND6_LLINFO_STALE:
		/* Garbage Collection(RFC 2461 5.3) */
		if (!ND6_LLINFO_PERMANENT(ln)) {
			(void)nd6_free(rt, 1);
			ln = NULL;
		}
		break;

	case ND6_LLINFO_DELAY:
		if (ndi && (ndi->flags & ND6_IFF_PERFORMNUD) != 0) {
			/* We need NUD */
			ln->ln_asked = 1;
			ln->ln_state = ND6_LLINFO_PROBE;
			nd6_llinfo_settimer(ln, (long)ndi->retrans * hz / 1000);
			nd6_ns_output(ifp, &dst->sin6_addr,
			    &dst->sin6_addr, ln, 0);
		} else {
			ln->ln_state = ND6_LLINFO_STALE; /* XXX */
			nd6_llinfo_settimer(ln, (long)nd6_gctimer * hz);
		}
		break;
	case ND6_LLINFO_PROBE:
		if (ln->ln_asked < nd6_umaxtries) {
			ln->ln_asked++;
			nd6_llinfo_settimer(ln, (long)ndi->retrans * hz / 1000);
			nd6_ns_output(ifp, &dst->sin6_addr,
			    &dst->sin6_addr, ln, 0);
		} else {
			(void)nd6_free(rt, 0);
			ln = NULL;
		}
		break;
	}

	KERNEL_UNLOCK_ONE(NULL);
	mutex_exit(softnet_lock);
}

/*
 * ND6 timer routine to expire default route list and prefix list
 */
void
nd6_timer(void *ignored_arg)
{
	struct nd_defrouter *next_dr, *dr;
	struct nd_prefix *next_pr, *pr;
	struct in6_ifaddr *ia6, *nia6;

	callout_reset(&nd6_timer_ch, nd6_prune * hz,
	    nd6_timer, NULL);

	mutex_enter(softnet_lock);
	KERNEL_LOCK(1, NULL);

	/* expire default router list */
	
	TAILQ_FOREACH_SAFE(dr, &nd_defrouter, dr_entry, next_dr) {
		if (dr->expire && dr->expire < time_uptime) {
			defrtrlist_del(dr, NULL);
		}
	}

	/*
	 * expire interface addresses.
	 * in the past the loop was inside prefix expiry processing.
	 * However, from a stricter speci-confrmance standpoint, we should
	 * rather separate address lifetimes and prefix lifetimes.
	 */
  addrloop:
	for (ia6 = in6_ifaddr; ia6; ia6 = nia6) {
		nia6 = ia6->ia_next;
		/* check address lifetime */
		if (IFA6_IS_INVALID(ia6)) {
			int regen = 0;

			/*
			 * If the expiring address is temporary, try
			 * regenerating a new one.  This would be useful when
			 * we suspended a laptop PC, then turned it on after a
			 * period that could invalidate all temporary
			 * addresses.  Although we may have to restart the
			 * loop (see below), it must be after purging the
			 * address.  Otherwise, we'd see an infinite loop of
			 * regeneration.
			 */
			if (ip6_use_tempaddr &&
			    (ia6->ia6_flags & IN6_IFF_TEMPORARY) != 0) {
				if (regen_tmpaddr(ia6) == 0)
					regen = 1;
			}

 			in6_purgeaddr(&ia6->ia_ifa);

			if (regen)
				goto addrloop; /* XXX: see below */
		} else if (IFA6_IS_DEPRECATED(ia6)) {
			int oldflags = ia6->ia6_flags;

			if ((oldflags & IN6_IFF_DEPRECATED) == 0) {
				ia6->ia6_flags |= IN6_IFF_DEPRECATED;
				rt_newaddrmsg(RTM_NEWADDR,
				    (struct ifaddr *)ia6, 0, NULL);
			}

			/*
			 * If a temporary address has just become deprecated,
			 * regenerate a new one if possible.
			 */
			if (ip6_use_tempaddr &&
			    (ia6->ia6_flags & IN6_IFF_TEMPORARY) != 0 &&
			    (oldflags & IN6_IFF_DEPRECATED) == 0) {

				if (regen_tmpaddr(ia6) == 0) {
					/*
					 * A new temporary address is
					 * generated.
					 * XXX: this means the address chain
					 * has changed while we are still in
					 * the loop.  Although the change
					 * would not cause disaster (because
					 * it's not a deletion, but an
					 * addition,) we'd rather restart the
					 * loop just for safety.  Or does this
					 * significantly reduce performance??
					 */
					goto addrloop;
				}
			}
		} else {
			/*
			 * A new RA might have made a deprecated address
			 * preferred.
			 */
			if (ia6->ia6_flags & IN6_IFF_DEPRECATED) {
				ia6->ia6_flags &= ~IN6_IFF_DEPRECATED;
				rt_newaddrmsg(RTM_NEWADDR,
				    (struct ifaddr *)ia6, 0, NULL);
			}
		}
	}

	/* expire prefix list */
	LIST_FOREACH_SAFE(pr, &nd_prefix, ndpr_entry, next_pr) {
		/*
		 * check prefix lifetime.
		 * since pltime is just for autoconf, pltime processing for
		 * prefix is not necessary.
		 */
		if (pr->ndpr_vltime != ND6_INFINITE_LIFETIME &&
		    time_uptime - pr->ndpr_lastupdate > pr->ndpr_vltime) {

			/*
			 * address expiration and prefix expiration are
			 * separate.  NEVER perform in6_purgeaddr here.
			 */

			prelist_remove(pr);
		}
	}

	KERNEL_UNLOCK_ONE(NULL);
	mutex_exit(softnet_lock);
}

/* ia6: deprecated/invalidated temporary address */
static int
regen_tmpaddr(struct in6_ifaddr *ia6)
{
	struct ifaddr *ifa;
	struct ifnet *ifp;
	struct in6_ifaddr *public_ifa6 = NULL;

	ifp = ia6->ia_ifa.ifa_ifp;
	IFADDR_FOREACH(ifa, ifp) {
		struct in6_ifaddr *it6;

		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;

		it6 = (struct in6_ifaddr *)ifa;

		/* ignore no autoconf addresses. */
		if ((it6->ia6_flags & IN6_IFF_AUTOCONF) == 0)
			continue;

		/* ignore autoconf addresses with different prefixes. */
		if (it6->ia6_ndpr == NULL || it6->ia6_ndpr != ia6->ia6_ndpr)
			continue;

		/*
		 * Now we are looking at an autoconf address with the same
		 * prefix as ours.  If the address is temporary and is still
		 * preferred, do not create another one.  It would be rare, but
		 * could happen, for example, when we resume a laptop PC after
		 * a long period.
		 */
		if ((it6->ia6_flags & IN6_IFF_TEMPORARY) != 0 &&
		    !IFA6_IS_DEPRECATED(it6)) {
			public_ifa6 = NULL;
			break;
		}

		/*
		 * This is a public autoconf address that has the same prefix
		 * as ours.  If it is preferred, keep it.  We can't break the
		 * loop here, because there may be a still-preferred temporary
		 * address with the prefix.
		 */
		if (!IFA6_IS_DEPRECATED(it6))
		    public_ifa6 = it6;
	}

	if (public_ifa6 != NULL) {
		int e;

		/*
		 * Random factor is introduced in the preferred lifetime, so
		 * we do not need additional delay (3rd arg to in6_tmpifadd).
		 */
		if ((e = in6_tmpifadd(public_ifa6, 0, 0)) != 0) {
			log(LOG_NOTICE, "regen_tmpaddr: failed to create a new"
			    " tmp addr, errno=%d\n", e);
			return -1;
		}
		return 0;
	}

	return -1;
}

bool
nd6_accepts_rtadv(const struct nd_ifinfo *ndi)
{
	switch (ndi->flags & (ND6_IFF_ACCEPT_RTADV|ND6_IFF_OVERRIDE_RTADV)) {
	case ND6_IFF_OVERRIDE_RTADV|ND6_IFF_ACCEPT_RTADV:
		return true;
	case ND6_IFF_ACCEPT_RTADV:
		return ip6_accept_rtadv != 0;
	case ND6_IFF_OVERRIDE_RTADV:
	case 0:
	default:
		return false;
	}
}

/*
 * Nuke neighbor cache/prefix/default router management table, right before
 * ifp goes away.
 */
void
nd6_purge(struct ifnet *ifp, struct in6_ifextra *ext)
{
	struct llinfo_nd6 *ln, *nln;
	struct nd_defrouter *dr, *ndr;
	struct nd_prefix *pr, *npr;

	/*
	 * During detach, the ND info might be already removed, but
	 * then is explitly passed as argument.
	 * Otherwise get it from ifp->if_afdata.
	 */
	if (ext == NULL)
		ext = ifp->if_afdata[AF_INET6];
	if (ext == NULL)
		return;

	/*
	 * Nuke default router list entries toward ifp.
	 * We defer removal of default router list entries that is installed
	 * in the routing table, in order to keep additional side effects as
	 * small as possible.
	 */
	TAILQ_FOREACH_SAFE(dr, &nd_defrouter, dr_entry, ndr) {
		if (dr->installed)
			continue;

		if (dr->ifp == ifp) {
			KASSERT(ext != NULL);
			defrtrlist_del(dr, ext);
		}
	}

	TAILQ_FOREACH_SAFE(dr, &nd_defrouter, dr_entry, ndr) {
		if (!dr->installed)
			continue;

		if (dr->ifp == ifp) {
			KASSERT(ext != NULL);
			defrtrlist_del(dr, ext);
		}
	}

	/* Nuke prefix list entries toward ifp */
	LIST_FOREACH_SAFE(pr, &nd_prefix, ndpr_entry, npr) {
		if (pr->ndpr_ifp == ifp) {
			/*
			 * Because if_detach() does *not* release prefixes
			 * while purging addresses the reference count will
			 * still be above zero. We therefore reset it to
			 * make sure that the prefix really gets purged.
			 */
			pr->ndpr_refcnt = 0;
			/*
			 * Previously, pr->ndpr_addr is removed as well,
			 * but I strongly believe we don't have to do it.
			 * nd6_purge() is only called from in6_ifdetach(),
			 * which removes all the associated interface addresses
			 * by itself.
			 * (jinmei@kame.net 20010129)
			 */
			prelist_remove(pr);
		}
	}

	/* cancel default outgoing interface setting */
	if (nd6_defifindex == ifp->if_index)
		nd6_setdefaultiface(0);

	/* XXX: too restrictive? */
	if (!ip6_forwarding && ifp->if_afdata[AF_INET6]) {
		struct nd_ifinfo *ndi = ND_IFINFO(ifp);
		if (ndi && nd6_accepts_rtadv(ndi)) {
			/* refresh default router list */
			defrouter_select();
		}
	}

	/*
	 * Nuke neighbor cache entries for the ifp.
	 * Note that rt->rt_ifp may not be the same as ifp,
	 * due to KAME goto ours hack.  See RTM_RESOLVE case in
	 * nd6_rtrequest(), and ip6_input().
	 */
	ln = llinfo_nd6.ln_next;
	while (ln != NULL && ln != &llinfo_nd6) {
		struct rtentry *rt;
		const struct sockaddr_dl *sdl;

		nln = ln->ln_next;
		rt = ln->ln_rt;
		if (rt && rt->rt_gateway &&
		    rt->rt_gateway->sa_family == AF_LINK) {
			sdl = satocsdl(rt->rt_gateway);
			if (sdl->sdl_index == ifp->if_index)
				nln = nd6_free(rt, 0);
		}
		ln = nln;
	}
}

static struct rtentry *
nd6_lookup1(const struct in6_addr *addr6, int create, struct ifnet *ifp,
    int cloning)
{
	struct rtentry *rt;
	struct sockaddr_in6 sin6;

	sockaddr_in6_init(&sin6, addr6, 0, 0, 0);
	rt = rtalloc1((struct sockaddr *)&sin6, create);
	if (rt != NULL && (rt->rt_flags & RTF_LLINFO) == 0) {
		/*
		 * This is the case for the default route.
		 * If we want to create a neighbor cache for the address, we
		 * should free the route for the destination and allocate an
		 * interface route.
		 */
		if (create) {
			rtfree(rt);
			rt = NULL;
		}
	}
	if (rt != NULL)
		;
	else if (create && ifp) {
		int e;

		/*
		 * If no route is available and create is set,
		 * we allocate a host route for the destination
		 * and treat it like an interface route.
		 * This hack is necessary for a neighbor which can't
		 * be covered by our own prefix.
		 */
		struct ifaddr *ifa =
		    ifaof_ifpforaddr((struct sockaddr *)&sin6, ifp);
		if (ifa == NULL)
			return NULL;

		/*
		 * Create a new route.  RTF_LLINFO is necessary
		 * to create a Neighbor Cache entry for the
		 * destination in nd6_rtrequest which will be
		 * called in rtrequest via ifa->ifa_rtrequest.
		 */
		if ((e = rtrequest(RTM_ADD, (const struct sockaddr *)&sin6,
		    ifa->ifa_addr, (const struct sockaddr *)&all1_sa,
		    (ifa->ifa_flags | RTF_HOST | RTF_LLINFO) &
		    ~RTF_CLONING, &rt)) != 0) {
#if 0
			log(LOG_ERR,
			    "nd6_lookup: failed to add route for a "
			    "neighbor(%s), errno=%d\n",
			    ip6_sprintf(addr6), e);
#endif
			return NULL;
		}
		if (rt == NULL)
			return NULL;
		if (rt->rt_llinfo) {
			struct llinfo_nd6 *ln =
			    (struct llinfo_nd6 *)rt->rt_llinfo;
			ln->ln_state = ND6_LLINFO_NOSTATE;
		}
	} else
		return NULL;

	/*
	 * Check for a cloning route to match the address.
	 * This should only be set from in6_is_addr_neighbor so we avoid
	 * a potentially expensive second call to rtalloc1.
	 */
	if (cloning &&
	    rt->rt_flags & (RTF_CLONING | RTF_CLONED) &&
	    (rt->rt_ifp == ifp
#if NBRIDGE > 0
	    || rt->rt_ifp->if_bridge == ifp->if_bridge
#endif
#if NCARP > 0
	    || (ifp->if_type == IFT_CARP && rt->rt_ifp == ifp->if_carpdev) ||
	    (rt->rt_ifp->if_type == IFT_CARP && rt->rt_ifp->if_carpdev == ifp)||
	    (ifp->if_type == IFT_CARP && rt->rt_ifp->if_type == IFT_CARP &&
	    rt->rt_ifp->if_carpdev == ifp->if_carpdev)
#endif
	    ))
		return rt;

	/*
	 * Validation for the entry.
	 * Note that the check for rt_llinfo is necessary because a cloned
	 * route from a parent route that has the L flag (e.g. the default
	 * route to a p2p interface) may have the flag, too, while the
	 * destination is not actually a neighbor.
	 * XXX: we can't use rt->rt_ifp to check for the interface, since
	 *      it might be the loopback interface if the entry is for our
	 *      own address on a non-loopback interface. Instead, we should
	 *      use rt->rt_ifa->ifa_ifp, which would specify the REAL
	 *	interface.
	 * Note also that ifa_ifp and ifp may differ when we connect two
	 * interfaces to a same link, install a link prefix to an interface,
	 * and try to install a neighbor cache on an interface that does not
	 * have a route to the prefix.
	 */
	if ((rt->rt_flags & RTF_GATEWAY) || (rt->rt_flags & RTF_LLINFO) == 0 ||
	    rt->rt_gateway->sa_family != AF_LINK || rt->rt_llinfo == NULL ||
	    (ifp && rt->rt_ifa->ifa_ifp != ifp)) {
		if (create) {
			nd6log((LOG_DEBUG,
			    "nd6_lookup: failed to lookup %s (if = %s)\n",
			    ip6_sprintf(addr6),
			    ifp ? if_name(ifp) : "unspec"));
		}
		rtfree(rt);
		return NULL;
	}
	return rt;
}

struct rtentry *
nd6_lookup(const struct in6_addr *addr6, int create, struct ifnet *ifp)
{

	return nd6_lookup1(addr6, create, ifp, 0);
}

/*
 * Detect if a given IPv6 address identifies a neighbor on a given link.
 * XXX: should take care of the destination of a p2p link?
 */
int
nd6_is_addr_neighbor(const struct sockaddr_in6 *addr, struct ifnet *ifp)
{
	struct nd_prefix *pr;
	struct rtentry *rt;

	/*
	 * A link-local address is always a neighbor.
	 * XXX: a link does not necessarily specify a single interface.
	 */
	if (IN6_IS_ADDR_LINKLOCAL(&addr->sin6_addr)) {
		struct sockaddr_in6 sin6_copy;
		u_int32_t zone;

		/*
		 * We need sin6_copy since sa6_recoverscope() may modify the
		 * content (XXX).
		 */
		sin6_copy = *addr;
		if (sa6_recoverscope(&sin6_copy))
			return 0; /* XXX: should be impossible */
		if (in6_setscope(&sin6_copy.sin6_addr, ifp, &zone))
			return 0;
		if (sin6_copy.sin6_scope_id == zone)
			return 1;
		else
			return 0;
	}

	/*
	 * If the address matches one of our on-link prefixes, it should be a
	 * neighbor.
	 */
	LIST_FOREACH(pr, &nd_prefix, ndpr_entry) {
		if (pr->ndpr_ifp != ifp)
			continue;

		if (!(pr->ndpr_stateflags & NDPRF_ONLINK))
			continue;

		if (IN6_ARE_MASKED_ADDR_EQUAL(&pr->ndpr_prefix.sin6_addr,
		    &addr->sin6_addr, &pr->ndpr_mask))
			return 1;
	}

	/*
	 * If the default router list is empty, all addresses are regarded
	 * as on-link, and thus, as a neighbor.
	 * XXX: we restrict the condition to hosts, because routers usually do
	 * not have the "default router list".
	 */
	if (!ip6_forwarding && TAILQ_FIRST(&nd_defrouter) == NULL &&
	    nd6_defifindex == ifp->if_index) {
		return 1;
	}

	/*
	 * Even if the address matches none of our addresses, it might match
	 * a cloning route or be in the neighbor cache.
	 */
	rt = nd6_lookup1(&addr->sin6_addr, 0, ifp, 1);
	if (rt != NULL) {
		rtfree(rt);
		return 1;
	}

	return 0;
}

/*
 * Free an nd6 llinfo entry.
 * Since the function would cause significant changes in the kernel, DO NOT
 * make it global, unless you have a strong reason for the change, and are sure
 * that the change is safe.
 */
static struct llinfo_nd6 *
nd6_free(struct rtentry *rt, int gc)
{
	struct llinfo_nd6 *ln = (struct llinfo_nd6 *)rt->rt_llinfo, *next;
	struct in6_addr in6 = satocsin6(rt_getkey(rt))->sin6_addr;
	struct nd_defrouter *dr;
	int error;

	/*
	 * we used to have pfctlinput(PRC_HOSTDEAD) here.
	 * even though it is not harmful, it was not really necessary.
	 */

	/* cancel timer */
	nd6_llinfo_settimer(ln, -1);

	if (!ip6_forwarding) {
		int s;
		s = splsoftnet();
		dr = defrouter_lookup(&satocsin6(rt_getkey(rt))->sin6_addr,
		    rt->rt_ifp);

		if (dr != NULL && dr->expire &&
		    ln->ln_state == ND6_LLINFO_STALE && gc) {
			/*
			 * If the reason for the deletion is just garbage
			 * collection, and the neighbor is an active default
			 * router, do not delete it.  Instead, reset the GC
			 * timer using the router's lifetime.
			 * Simply deleting the entry would affect default
			 * router selection, which is not necessarily a good
			 * thing, especially when we're using router preference
			 * values.
			 * XXX: the check for ln_state would be redundant,
			 *      but we intentionally keep it just in case.
			 */
			if (dr->expire > time_uptime)
				nd6_llinfo_settimer(ln,
				    (dr->expire - time_uptime) * hz);
			else
				nd6_llinfo_settimer(ln, (long)nd6_gctimer * hz);
			splx(s);
			return ln->ln_next;
		}

		if (ln->ln_router || dr) {
			/*
			 * rt6_flush must be called whether or not the neighbor
			 * is in the Default Router List.
			 * See a corresponding comment in nd6_na_input().
			 */
			rt6_flush(&in6, rt->rt_ifp);
		}

		if (dr) {
			/*
			 * Unreachablity of a router might affect the default
			 * router selection and on-link detection of advertised
			 * prefixes.
			 */

			/*
			 * Temporarily fake the state to choose a new default
			 * router and to perform on-link determination of
			 * prefixes correctly.
			 * Below the state will be set correctly,
			 * or the entry itself will be deleted.
			 */
			ln->ln_state = ND6_LLINFO_INCOMPLETE;

			/*
			 * Since defrouter_select() does not affect the
			 * on-link determination and MIP6 needs the check
			 * before the default router selection, we perform
			 * the check now.
			 */
			pfxlist_onlink_check();

			/*
			 * refresh default router list
			 */
			defrouter_select();
		}
		splx(s);
	}

	/*
	 * Before deleting the entry, remember the next entry as the
	 * return value.  We need this because pfxlist_onlink_check() above
	 * might have freed other entries (particularly the old next entry) as
	 * a side effect (XXX).
	 */
	next = ln->ln_next;

	/*
	 * Detach the route from the routing tree and the list of neighbor
	 * caches, and disable the route entry not to be used in already
	 * cached routes.
	 */
	error = rtrequest_newmsg(RTM_DELETE, rt_getkey(rt), NULL,
	    rt_mask(rt), 0);
	if (error != 0) {
		/* XXX need error message? */;
	}

	return next;
}

/*
 * Upper-layer reachability hint for Neighbor Unreachability Detection.
 *
 * XXX cost-effective methods?
 */
void
nd6_nud_hint(struct rtentry *rt)
{
	struct llinfo_nd6 *ln;

	if (rt == NULL)
		return;

	if ((rt->rt_flags & RTF_GATEWAY) != 0 ||
	    (rt->rt_flags & RTF_LLINFO) == 0 ||
	    !rt->rt_llinfo || !rt->rt_gateway ||
	    rt->rt_gateway->sa_family != AF_LINK) {
		/* This is not a host route. */
		return;
	}

	ln = (struct llinfo_nd6 *)rt->rt_llinfo;
	if (ln->ln_state < ND6_LLINFO_REACHABLE)
		return;

	/*
	 * if we get upper-layer reachability confirmation many times,
	 * it is possible we have false information.
	 */
	ln->ln_byhint++;
	if (ln->ln_byhint > nd6_maxnudhint)
		return;

	ln->ln_state = ND6_LLINFO_REACHABLE;
	if (!ND6_LLINFO_PERMANENT(ln)) {
		nd6_llinfo_settimer(ln,
		    (long)ND_IFINFO(rt->rt_ifp)->reachable * hz);
	}

	return;
}

void
nd6_rtrequest(int req, struct rtentry *rt, const struct rt_addrinfo *info)
{
	struct sockaddr *gate = rt->rt_gateway;
	struct llinfo_nd6 *ln = (struct llinfo_nd6 *)rt->rt_llinfo;
	struct ifnet *ifp = rt->rt_ifp;
	uint8_t namelen = strlen(ifp->if_xname), addrlen = ifp->if_addrlen;
	struct ifaddr *ifa;

	RT_DPRINTF("rt_getkey(rt) = %p\n", rt_getkey(rt));

	if (req == RTM_LLINFO_UPD) {
		int rc;
		struct in6_addr *in6;
		struct in6_addr in6_all;
		int anycast;

		if ((ifa = info->rti_ifa) == NULL)
			return;

		in6 = &ifatoia6(ifa)->ia_addr.sin6_addr;
		anycast = ifatoia6(ifa)->ia6_flags & IN6_IFF_ANYCAST;

		in6_all = in6addr_linklocal_allnodes;
		if ((rc = in6_setscope(&in6_all, ifa->ifa_ifp, NULL)) != 0) {
			log(LOG_ERR, "%s: failed to set scope %s "
			    "(errno=%d)\n", __func__, if_name(ifp), rc);
			return;
		}

		/* XXX don't set Override for proxy addresses */
		nd6_na_output(ifa->ifa_ifp, &in6_all, in6,
		    (anycast ? 0 : ND_NA_FLAG_OVERRIDE)
#if 0
		    | (ip6_forwarding ? ND_NA_FLAG_ROUTER : 0)
#endif
		    , 1, NULL);
		return;
	}

	if ((rt->rt_flags & RTF_GATEWAY) != 0)
		return;

	if (nd6_need_cache(ifp) == 0 && (rt->rt_flags & RTF_HOST) == 0) {
		RT_DPRINTF("rt_getkey(rt) = %p\n", rt_getkey(rt));
		/*
		 * This is probably an interface direct route for a link
		 * which does not need neighbor caches (e.g. fe80::%lo0/64).
		 * We do not need special treatment below for such a route.
		 * Moreover, the RTF_LLINFO flag which would be set below
		 * would annoy the ndp(8) command.
		 */
		return;
	}

	if (req == RTM_RESOLVE &&
	    (nd6_need_cache(ifp) == 0 || /* stf case */
	     !nd6_is_addr_neighbor(satocsin6(rt_getkey(rt)), ifp))) {
		RT_DPRINTF("rt_getkey(rt) = %p\n", rt_getkey(rt));
		/*
		 * FreeBSD and BSD/OS often make a cloned host route based
		 * on a less-specific route (e.g. the default route).
		 * If the less specific route does not have a "gateway"
		 * (this is the case when the route just goes to a p2p or an
		 * stf interface), we'll mistakenly make a neighbor cache for
		 * the host route, and will see strange neighbor solicitation
		 * for the corresponding destination.  In order to avoid the
		 * confusion, we check if the destination of the route is
		 * a neighbor in terms of neighbor discovery, and stop the
		 * process if not.  Additionally, we remove the LLINFO flag
		 * so that ndp(8) will not try to get the neighbor information
		 * of the destination.
		 */
		rt->rt_flags &= ~RTF_LLINFO;
		return;
	}

	switch (req) {
	case RTM_ADD:
		RT_DPRINTF("rt_getkey(rt) = %p\n", rt_getkey(rt));
		/*
		 * There is no backward compatibility :)
		 *
		 * if ((rt->rt_flags & RTF_HOST) == 0 &&
		 *     SIN(rt_mask(rt))->sin_addr.s_addr != 0xffffffff)
		 *	   rt->rt_flags |= RTF_CLONING;
		 */
		if ((rt->rt_flags & RTF_CLONING) ||
		    ((rt->rt_flags & RTF_LLINFO) && ln == NULL)) {
			union {
				struct sockaddr sa;
				struct sockaddr_dl sdl;
				struct sockaddr_storage ss;
			} u;
			/*
			 * Case 1: This route should come from a route to
			 * interface (RTF_CLONING case) or the route should be
			 * treated as on-link but is currently not
			 * (RTF_LLINFO && ln == NULL case).
			 */
			if (sockaddr_dl_init(&u.sdl, sizeof(u.ss),
			    ifp->if_index, ifp->if_type,
			    NULL, namelen, NULL, addrlen) == NULL) {
				printf("%s.%d: sockaddr_dl_init(, %zu, ) "
				    "failed on %s\n", __func__, __LINE__,
				    sizeof(u.ss), if_name(ifp));
			}
			rt_setgate(rt, &u.sa);
			gate = rt->rt_gateway;
			RT_DPRINTF("rt_getkey(rt) = %p\n", rt_getkey(rt));
			if (ln != NULL)
				nd6_llinfo_settimer(ln, 0);
			RT_DPRINTF("rt_getkey(rt) = %p\n", rt_getkey(rt));
			if ((rt->rt_flags & RTF_CLONING) != 0)
				break;
		}
		RT_DPRINTF("rt_getkey(rt) = %p\n", rt_getkey(rt));
		/*
		 * In IPv4 code, we try to annonuce new RTF_ANNOUNCE entry here.
		 * We don't do that here since llinfo is not ready yet.
		 *
		 * There are also couple of other things to be discussed:
		 * - unsolicited NA code needs improvement beforehand
		 * - RFC2461 says we MAY send multicast unsolicited NA
		 *   (7.2.6 paragraph 4), however, it also says that we
		 *   SHOULD provide a mechanism to prevent multicast NA storm.
		 *   we don't have anything like it right now.
		 *   note that the mechanism needs a mutual agreement
		 *   between proxies, which means that we need to implement
		 *   a new protocol, or a new kludge.
		 * - from RFC2461 6.2.4, host MUST NOT send an unsolicited NA.
		 *   we need to check ip6forwarding before sending it.
		 *   (or should we allow proxy ND configuration only for
		 *   routers?  there's no mention about proxy ND from hosts)
		 */
#if 0
		/* XXX it does not work */
		if (rt->rt_flags & RTF_ANNOUNCE)
			nd6_na_output(ifp,
			      &satocsin6(rt_getkey(rt))->sin6_addr,
			      &satocsin6(rt_getkey(rt))->sin6_addr,
			      ip6_forwarding ? ND_NA_FLAG_ROUTER : 0,
			      1, NULL);
#endif
		/* FALLTHROUGH */
	case RTM_RESOLVE:
		if ((ifp->if_flags & (IFF_POINTOPOINT | IFF_LOOPBACK)) == 0) {
			RT_DPRINTF("rt_getkey(rt) = %p\n", rt_getkey(rt));
			/*
			 * Address resolution isn't necessary for a point to
			 * point link, so we can skip this test for a p2p link.
			 */
			if (gate->sa_family != AF_LINK ||
			    gate->sa_len <
			    sockaddr_dl_measure(namelen, addrlen)) {
				log(LOG_DEBUG,
				    "nd6_rtrequest: bad gateway value: %s\n",
				    if_name(ifp));
				break;
			}
			satosdl(gate)->sdl_type = ifp->if_type;
			satosdl(gate)->sdl_index = ifp->if_index;
			RT_DPRINTF("rt_getkey(rt) = %p\n", rt_getkey(rt));
		}
		if (ln != NULL)
			break;	/* This happens on a route change */
		RT_DPRINTF("rt_getkey(rt) = %p\n", rt_getkey(rt));
		/*
		 * Case 2: This route may come from cloning, or a manual route
		 * add with a LL address.
		 */
		R_Malloc(ln, struct llinfo_nd6 *, sizeof(*ln));
		rt->rt_llinfo = ln;
		RT_DPRINTF("rt_getkey(rt) = %p\n", rt_getkey(rt));
		if (ln == NULL) {
			log(LOG_DEBUG, "nd6_rtrequest: malloc failed\n");
			break;
		}
		RT_DPRINTF("rt_getkey(rt) = %p\n", rt_getkey(rt));
		nd6_inuse++;
		nd6_allocated++;
		memset(ln, 0, sizeof(*ln));
		ln->ln_rt = rt;
		callout_init(&ln->ln_timer_ch, CALLOUT_MPSAFE);
		/* this is required for "ndp" command. - shin */
		if (req == RTM_ADD) {
		        /*
			 * gate should have some valid AF_LINK entry,
			 * and ln->ln_expire should have some lifetime
			 * which is specified by ndp command.
			 */
			ln->ln_state = ND6_LLINFO_REACHABLE;
			ln->ln_byhint = 0;
		} else {
		        /*
			 * When req == RTM_RESOLVE, rt is created and
			 * initialized in rtrequest(), so rt_expire is 0.
			 */
			ln->ln_state = ND6_LLINFO_NOSTATE;
			nd6_llinfo_settimer(ln, 0);
		}
		RT_DPRINTF("rt_getkey(rt) = %p\n", rt_getkey(rt));
		rt->rt_flags |= RTF_LLINFO;
		ln->ln_next = llinfo_nd6.ln_next;
		llinfo_nd6.ln_next = ln;
		ln->ln_prev = &llinfo_nd6;
		ln->ln_next->ln_prev = ln;

		/*
		 * If we have too many cache entries, initiate immediate
		 * purging for some "less recently used" entries.  Note that
		 * we cannot directly call nd6_free() here because it would
		 * cause re-entering rtable related routines triggering an LOR
		 * problem for FreeBSD.
		 */
		if (ip6_neighborgcthresh >= 0 &&
		    nd6_inuse >= ip6_neighborgcthresh) {
			int i;

			for (i = 0; i < 10 && llinfo_nd6.ln_prev != ln; i++) {
				struct llinfo_nd6 *ln_end = llinfo_nd6.ln_prev;

				/* Move this entry to the head */
				LN_DEQUEUE(ln_end);
				LN_INSERTHEAD(ln_end);

				if (ND6_LLINFO_PERMANENT(ln_end))
					continue;

				if (ln_end->ln_state > ND6_LLINFO_INCOMPLETE)
					ln_end->ln_state = ND6_LLINFO_STALE;
				else
					ln_end->ln_state = ND6_LLINFO_PURGE;
				nd6_llinfo_settimer(ln_end, 0);
			}
		}

		RT_DPRINTF("rt_getkey(rt) = %p\n", rt_getkey(rt));
		/*
		 * check if rt_getkey(rt) is an address assigned
		 * to the interface.
		 */
		ifa = (struct ifaddr *)in6ifa_ifpwithaddr(ifp,
		    &satocsin6(rt_getkey(rt))->sin6_addr);
		RT_DPRINTF("rt_getkey(rt) = %p\n", rt_getkey(rt));
		if (ifa != NULL) {
			const void *mac;
			nd6_llinfo_settimer(ln, -1);
			ln->ln_state = ND6_LLINFO_REACHABLE;
			ln->ln_byhint = 0;
			if ((mac = nd6_ifptomac(ifp)) != NULL) {
				/* XXX check for error */
				if (sockaddr_dl_setaddr(satosdl(gate),
				    gate->sa_len, mac,
				    ifp->if_addrlen) == NULL) {
					printf("%s.%d: "
					    "sockaddr_dl_setaddr(, %d, ) "
					    "failed on %s\n", __func__,
					    __LINE__, gate->sa_len,
					    if_name(ifp));
				}
			}
			if (nd6_useloopback) {
				ifp = rt->rt_ifp = lo0ifp;	/* XXX */
				/*
				 * Make sure rt_ifa be equal to the ifaddr
				 * corresponding to the address.
				 * We need this because when we refer
				 * rt_ifa->ia6_flags in ip6_input, we assume
				 * that the rt_ifa points to the address instead
				 * of the loopback address.
				 */
				if (ifa != rt->rt_ifa)
					rt_replace_ifa(rt, ifa);
				rt->rt_rmx.rmx_mtu = 0;
				rt->rt_flags &= ~RTF_CLONED;
			}
			rt->rt_flags |= RTF_LOCAL;
		} else if (rt->rt_flags & RTF_ANNOUNCE) {
			nd6_llinfo_settimer(ln, -1);
			ln->ln_state = ND6_LLINFO_REACHABLE;
			ln->ln_byhint = 0;

			/* join solicited node multicast for proxy ND */
			if (ifp->if_flags & IFF_MULTICAST) {
				struct in6_addr llsol;
				int error;

				llsol = satocsin6(rt_getkey(rt))->sin6_addr;
				llsol.s6_addr32[0] = htonl(0xff020000);
				llsol.s6_addr32[1] = 0;
				llsol.s6_addr32[2] = htonl(1);
				llsol.s6_addr8[12] = 0xff;
				if (in6_setscope(&llsol, ifp, NULL))
					break;
				if (!in6_addmulti(&llsol, ifp, &error, 0)) {
					nd6log((LOG_ERR, "%s: failed to join "
					    "%s (errno=%d)\n", if_name(ifp),
					    ip6_sprintf(&llsol), error));
				}
			}
		}
		break;

	case RTM_DELETE:
		if (ln == NULL)
			break;
		/* leave from solicited node multicast for proxy ND */
		if ((rt->rt_flags & RTF_ANNOUNCE) != 0 &&
		    (ifp->if_flags & IFF_MULTICAST) != 0) {
			struct in6_addr llsol;
			struct in6_multi *in6m;

			llsol = satocsin6(rt_getkey(rt))->sin6_addr;
			llsol.s6_addr32[0] = htonl(0xff020000);
			llsol.s6_addr32[1] = 0;
			llsol.s6_addr32[2] = htonl(1);
			llsol.s6_addr8[12] = 0xff;
			if (in6_setscope(&llsol, ifp, NULL) == 0) {
				IN6_LOOKUP_MULTI(llsol, ifp, in6m);
				if (in6m)
					in6_delmulti(in6m);
			}
		}
		nd6_inuse--;
		ln->ln_next->ln_prev = ln->ln_prev;
		ln->ln_prev->ln_next = ln->ln_next;
		ln->ln_prev = NULL;
		nd6_llinfo_settimer(ln, -1);
		rt->rt_llinfo = 0;
		rt->rt_flags &= ~RTF_LLINFO;
		clear_llinfo_pqueue(ln);
		Free(ln);
	}
}

int
nd6_ioctl(u_long cmd, void *data, struct ifnet *ifp)
{
	struct in6_drlist *drl = (struct in6_drlist *)data;
	struct in6_oprlist *oprl = (struct in6_oprlist *)data;
	struct in6_ndireq *ndi = (struct in6_ndireq *)data;
	struct in6_nbrinfo *nbi = (struct in6_nbrinfo *)data;
	struct in6_ndifreq *ndif = (struct in6_ndifreq *)data;
	struct nd_defrouter *dr;
	struct nd_prefix *pr;
	int i = 0, error = 0;
	int s;

	switch (cmd) {
	case SIOCGDRLST_IN6:
		/*
		 * obsolete API, use sysctl under net.inet6.icmp6
		 */
		memset(drl, 0, sizeof(*drl));
		s = splsoftnet();
		TAILQ_FOREACH(dr, &nd_defrouter, dr_entry) {
			if (i >= DRLSTSIZ)
				break;
			drl->defrouter[i].rtaddr = dr->rtaddr;
			in6_clearscope(&drl->defrouter[i].rtaddr);

			drl->defrouter[i].flags = dr->flags;
			drl->defrouter[i].rtlifetime = dr->rtlifetime;
			drl->defrouter[i].expire = dr->expire ?
			    time_mono_to_wall(dr->expire) : 0;
			drl->defrouter[i].if_index = dr->ifp->if_index;
			i++;
		}
		splx(s);
		break;
	case SIOCGPRLST_IN6:
		/*
		 * obsolete API, use sysctl under net.inet6.icmp6
		 *
		 * XXX the structure in6_prlist was changed in backward-
		 * incompatible manner.  in6_oprlist is used for SIOCGPRLST_IN6,
		 * in6_prlist is used for nd6_sysctl() - fill_prlist().
		 */
		/*
		 * XXX meaning of fields, especialy "raflags", is very
		 * differnet between RA prefix list and RR/static prefix list.
		 * how about separating ioctls into two?
		 */
		memset(oprl, 0, sizeof(*oprl));
		s = splsoftnet();
		LIST_FOREACH(pr, &nd_prefix, ndpr_entry) {
			struct nd_pfxrouter *pfr;
			int j;

			if (i >= PRLSTSIZ)
				break;
			oprl->prefix[i].prefix = pr->ndpr_prefix.sin6_addr;
			oprl->prefix[i].raflags = pr->ndpr_raf;
			oprl->prefix[i].prefixlen = pr->ndpr_plen;
			oprl->prefix[i].vltime = pr->ndpr_vltime;
			oprl->prefix[i].pltime = pr->ndpr_pltime;
			oprl->prefix[i].if_index = pr->ndpr_ifp->if_index;
			if (pr->ndpr_vltime == ND6_INFINITE_LIFETIME)
				oprl->prefix[i].expire = 0;
			else {
				time_t maxexpire;

				/* XXX: we assume time_t is signed. */
				maxexpire = (-1) &
				    ~((time_t)1 <<
				    ((sizeof(maxexpire) * 8) - 1));
				if (pr->ndpr_vltime <
				    maxexpire - pr->ndpr_lastupdate) {
					time_t expire;
					expire = pr->ndpr_lastupdate +
					    pr->ndpr_vltime;
					oprl->prefix[i].expire = expire ?
					    time_mono_to_wall(expire) : 0;
				} else
					oprl->prefix[i].expire = maxexpire;
			}

			j = 0;
			LIST_FOREACH(pfr, &pr->ndpr_advrtrs, pfr_entry) {
				if (j < DRLSTSIZ) {
#define RTRADDR oprl->prefix[i].advrtr[j]
					RTRADDR = pfr->router->rtaddr;
					in6_clearscope(&RTRADDR);
#undef RTRADDR
				}
				j++;
			}
			oprl->prefix[i].advrtrs = j;
			oprl->prefix[i].origin = PR_ORIG_RA;

			i++;
		}
		splx(s);

		break;
	case OSIOCGIFINFO_IN6:
#define ND	ndi->ndi
		/* XXX: old ndp(8) assumes a positive value for linkmtu. */
		memset(&ND, 0, sizeof(ND));
		ND.linkmtu = IN6_LINKMTU(ifp);
		ND.maxmtu = ND_IFINFO(ifp)->maxmtu;
		ND.basereachable = ND_IFINFO(ifp)->basereachable;
		ND.reachable = ND_IFINFO(ifp)->reachable;
		ND.retrans = ND_IFINFO(ifp)->retrans;
		ND.flags = ND_IFINFO(ifp)->flags;
		ND.recalctm = ND_IFINFO(ifp)->recalctm;
		ND.chlim = ND_IFINFO(ifp)->chlim;
		break;
	case SIOCGIFINFO_IN6:
		ND = *ND_IFINFO(ifp);
		break;
	case SIOCSIFINFO_IN6:
		/* 
		 * used to change host variables from userland.
		 * intented for a use on router to reflect RA configurations.
		 */
		/* 0 means 'unspecified' */
		if (ND.linkmtu != 0) {
			if (ND.linkmtu < IPV6_MMTU ||
			    ND.linkmtu > IN6_LINKMTU(ifp)) {
				error = EINVAL;
				break;
			}
			ND_IFINFO(ifp)->linkmtu = ND.linkmtu;
		}

		if (ND.basereachable != 0) {
			int obasereachable = ND_IFINFO(ifp)->basereachable;

			ND_IFINFO(ifp)->basereachable = ND.basereachable;
			if (ND.basereachable != obasereachable)
				ND_IFINFO(ifp)->reachable =
				    ND_COMPUTE_RTIME(ND.basereachable);
		}
		if (ND.retrans != 0)
			ND_IFINFO(ifp)->retrans = ND.retrans;
		if (ND.chlim != 0)
			ND_IFINFO(ifp)->chlim = ND.chlim;
		/* FALLTHROUGH */
	case SIOCSIFINFO_FLAGS:
	{
		struct ifaddr *ifa;
		struct in6_ifaddr *ia;

		if ((ND_IFINFO(ifp)->flags & ND6_IFF_IFDISABLED) &&
		    !(ND.flags & ND6_IFF_IFDISABLED))
		{
			/*
			 * If the interface is marked as ND6_IFF_IFDISABLED and
			 * has a link-local address with IN6_IFF_DUPLICATED,
			 * do not clear ND6_IFF_IFDISABLED.
			 * See RFC 4862, section 5.4.5.
			 */
			int duplicated_linklocal = 0;

			IFADDR_FOREACH(ifa, ifp) {
				if (ifa->ifa_addr->sa_family != AF_INET6)
					continue;
				ia = (struct in6_ifaddr *)ifa;
				if ((ia->ia6_flags & IN6_IFF_DUPLICATED) &&
				    IN6_IS_ADDR_LINKLOCAL(IA6_IN6(ia)))
				{
					duplicated_linklocal = 1;
					break;
				}
			}

			if (duplicated_linklocal) {
				ND.flags |= ND6_IFF_IFDISABLED;
				log(LOG_ERR, "Cannot enable an interface"
				    " with a link-local address marked"
				    " duplicate.\n");
			} else {
				ND_IFINFO(ifp)->flags &= ~ND6_IFF_IFDISABLED;
				if (ifp->if_flags & IFF_UP)
					in6_if_up(ifp);
			}
		} else if (!(ND_IFINFO(ifp)->flags & ND6_IFF_IFDISABLED) &&
		    (ND.flags & ND6_IFF_IFDISABLED))
		{
			/* Mark all IPv6 addresses as tentative. */

			ND_IFINFO(ifp)->flags |= ND6_IFF_IFDISABLED;
			IFADDR_FOREACH(ifa, ifp) {
				if (ifa->ifa_addr->sa_family != AF_INET6)
					continue;
				nd6_dad_stop(ifa);
				ia = (struct in6_ifaddr *)ifa;
				ia->ia6_flags |= IN6_IFF_TENTATIVE;
			}
		}

		if (ND.flags & ND6_IFF_AUTO_LINKLOCAL) {
			if (!(ND_IFINFO(ifp)->flags & ND6_IFF_AUTO_LINKLOCAL)) {
				/* auto_linklocal 0->1 transition */

				ND_IFINFO(ifp)->flags |= ND6_IFF_AUTO_LINKLOCAL;
				in6_ifattach(ifp, NULL);
			} else if (!(ND.flags & ND6_IFF_IFDISABLED) &&
			    ifp->if_flags & IFF_UP)
			{
				/*
				 * When the IF already has
				 * ND6_IFF_AUTO_LINKLOCAL, no link-local
				 * address is assigned, and IFF_UP, try to
				 * assign one.
				 */
				 int haslinklocal = 0;

				 IFADDR_FOREACH(ifa, ifp) {
					if (ifa->ifa_addr->sa_family !=AF_INET6)
						continue;
					ia = (struct in6_ifaddr *)ifa;
					if (IN6_IS_ADDR_LINKLOCAL(IA6_IN6(ia))){
						haslinklocal = 1;
						break;
					}
				 }
				 if (!haslinklocal)
					in6_ifattach(ifp, NULL);
			}
		}
	}
		ND_IFINFO(ifp)->flags = ND.flags;
		break;
#undef ND
	case SIOCSNDFLUSH_IN6:	/* XXX: the ioctl name is confusing... */
		/* sync kernel routing table with the default router list */
		defrouter_reset();
		defrouter_select();
		break;
	case SIOCSPFXFLUSH_IN6:
	{
		/* flush all the prefix advertised by routers */
		struct nd_prefix *pfx, *next;

		s = splsoftnet();
		LIST_FOREACH_SAFE(pfx, &nd_prefix, ndpr_entry, next) {
			struct in6_ifaddr *ia, *ia_next;

			if (IN6_IS_ADDR_LINKLOCAL(&pfx->ndpr_prefix.sin6_addr))
				continue; /* XXX */

			/* do we really have to remove addresses as well? */
			for (ia = in6_ifaddr; ia; ia = ia_next) {
				/* ia might be removed.  keep the next ptr. */
				ia_next = ia->ia_next;

				if ((ia->ia6_flags & IN6_IFF_AUTOCONF) == 0)
					continue;

				if (ia->ia6_ndpr == pfx)
					in6_purgeaddr(&ia->ia_ifa);
			}
			prelist_remove(pfx);
		}
		splx(s);
		break;
	}
	case SIOCSRTRFLUSH_IN6:
	{
		/* flush all the default routers */
		struct nd_defrouter *drtr, *next;

		s = splsoftnet();
		defrouter_reset();
		TAILQ_FOREACH_SAFE(drtr, &nd_defrouter, dr_entry, next) {
			defrtrlist_del(drtr, NULL);
		}
		defrouter_select();
		splx(s);
		break;
	}
	case SIOCGNBRINFO_IN6:
	{
		struct llinfo_nd6 *ln;
		struct in6_addr nb_addr = nbi->addr; /* make local for safety */
		struct rtentry *rt;

		if ((error = in6_setscope(&nb_addr, ifp, NULL)) != 0)
			return error;

		s = splsoftnet();
		rt = nd6_lookup(&nb_addr, 0, ifp);
		if (rt == NULL) {
			error = EINVAL;
			splx(s);
			break;
		}

		ln = (struct llinfo_nd6 *)rt->rt_llinfo;
		rtfree(rt);
		if (ln == NULL) {
			error = EINVAL;
			splx(s);
			break;
		}
		nbi->state = ln->ln_state;
		nbi->asked = ln->ln_asked;
		nbi->isrouter = ln->ln_router;
		nbi->expire = ln->ln_expire ?
		    time_mono_to_wall(ln->ln_expire) : 0;
		splx(s);

		break;
	}
	case SIOCGDEFIFACE_IN6:	/* XXX: should be implemented as a sysctl? */
		ndif->ifindex = nd6_defifindex;
		break;
	case SIOCSDEFIFACE_IN6:	/* XXX: should be implemented as a sysctl? */
		return nd6_setdefaultiface(ndif->ifindex);
	}
	return error;
}

void
nd6_llinfo_release_pkts(struct llinfo_nd6 *ln, struct ifnet *ifp,
    struct rtentry *rt)
{
	struct mbuf *m_hold, *m_hold_next;

	for (m_hold = ln->ln_hold, ln->ln_hold = NULL;
	     m_hold != NULL;
	     m_hold = m_hold_next) {
		m_hold_next = m_hold->m_nextpkt;
		m_hold->m_nextpkt = NULL;

		/*
		 * we assume ifp is not a p2p here, so
		 * just set the 2nd argument as the 
		 * 1st one.
		 */
		nd6_output(ifp, ifp, m_hold, satocsin6(rt_getkey(rt)), rt);
	}
}

/*
 * Create neighbor cache entry and cache link-layer address,
 * on reception of inbound ND6 packets.  (RS/RA/NS/redirect)
 */
void
nd6_cache_lladdr(
    struct ifnet *ifp,
    struct in6_addr *from,
    char *lladdr,
    int lladdrlen,
    int type,	/* ICMP6 type */
    int code	/* type dependent information */
)
{
	struct nd_ifinfo *ndi = ND_IFINFO(ifp);
	struct rtentry *rt = NULL;
	struct llinfo_nd6 *ln = NULL;
	int is_newentry;
	struct sockaddr_dl *sdl = NULL;
	int do_update;
	int olladdr;
	int llchange;
	int newstate = 0;

	KASSERT(ifp != NULL);
	KASSERT(from != NULL);

	/* nothing must be updated for unspecified address */
	if (IN6_IS_ADDR_UNSPECIFIED(from))
		return;

	/*
	 * Validation about ifp->if_addrlen and lladdrlen must be done in
	 * the caller.
	 *
	 * XXX If the link does not have link-layer adderss, what should
	 * we do? (ifp->if_addrlen == 0)
	 * Spec says nothing in sections for RA, RS and NA.  There's small
	 * description on it in NS section (RFC 2461 7.2.3).
	 */

	rt = nd6_lookup(from, 0, ifp);
	if (rt == NULL) {
#if 0
		/* nothing must be done if there's no lladdr */
		if (!lladdr || !lladdrlen)
			return NULL;
#endif

		rt = nd6_lookup(from, 1, ifp);
		is_newentry = 1;
	} else {
		/* do nothing if static ndp is set */
		if (rt->rt_flags & RTF_STATIC) {
			rtfree(rt);
			return;
		}
		is_newentry = 0;
	}

	if (rt == NULL)
		return;
	if ((rt->rt_flags & (RTF_GATEWAY | RTF_LLINFO)) != RTF_LLINFO) {
fail:
		(void)nd6_free(rt, 0);
		rtfree(rt);
		return;
	}
	ln = (struct llinfo_nd6 *)rt->rt_llinfo;
	if (ln == NULL)
		goto fail;
	if (rt->rt_gateway == NULL)
		goto fail;
	if (rt->rt_gateway->sa_family != AF_LINK)
		goto fail;
	sdl = satosdl(rt->rt_gateway);

	olladdr = (sdl->sdl_alen) ? 1 : 0;
	if (olladdr && lladdr) {
		if (memcmp(lladdr, CLLADDR(sdl), ifp->if_addrlen))
			llchange = 1;
		else
			llchange = 0;
	} else
		llchange = 0;

	/*
	 * newentry olladdr  lladdr  llchange	(*=record)
	 *	0	n	n	--	(1)
	 *	0	y	n	--	(2)
	 *	0	n	y	--	(3) * STALE
	 *	0	y	y	n	(4) *
	 *	0	y	y	y	(5) * STALE
	 *	1	--	n	--	(6)   NOSTATE(= PASSIVE)
	 *	1	--	y	--	(7) * STALE
	 */

	if (lladdr) {		/* (3-5) and (7) */
		/*
		 * Record source link-layer address
		 * XXX is it dependent to ifp->if_type?
		 */
		/* XXX check for error */
		if (sockaddr_dl_setaddr(sdl, sdl->sdl_len, lladdr,
		    ifp->if_addrlen) == NULL) {
			printf("%s.%d: sockaddr_dl_setaddr(, %d, ) "
			    "failed on %s\n", __func__, __LINE__,
			    sdl->sdl_len, if_name(ifp));
		}
	}

	if (!is_newentry) {
		if ((!olladdr && lladdr) ||		/* (3) */
		    (olladdr && lladdr && llchange)) {	/* (5) */
			do_update = 1;
			newstate = ND6_LLINFO_STALE;
		} else					/* (1-2,4) */
			do_update = 0;
	} else {
		do_update = 1;
		if (lladdr == NULL)			/* (6) */
			newstate = ND6_LLINFO_NOSTATE;
		else					/* (7) */
			newstate = ND6_LLINFO_STALE;
	}

	if (do_update) {
		/*
		 * Update the state of the neighbor cache.
		 */
		ln->ln_state = newstate;

		if (ln->ln_state == ND6_LLINFO_STALE) {
			/*
			 * XXX: since nd6_output() below will cause
			 * state tansition to DELAY and reset the timer,
			 * we must set the timer now, although it is actually
			 * meaningless.
			 */
			nd6_llinfo_settimer(ln, (long)nd6_gctimer * hz);

			nd6_llinfo_release_pkts(ln, ifp, rt);
		} else if (ln->ln_state == ND6_LLINFO_INCOMPLETE) {
			/* probe right away */
			nd6_llinfo_settimer((void *)ln, 0);
		}
	}

	/*
	 * ICMP6 type dependent behavior.
	 *
	 * NS: clear IsRouter if new entry
	 * RS: clear IsRouter
	 * RA: set IsRouter if there's lladdr
	 * redir: clear IsRouter if new entry
	 *
	 * RA case, (1):
	 * The spec says that we must set IsRouter in the following cases:
	 * - If lladdr exist, set IsRouter.  This means (1-5).
	 * - If it is old entry (!newentry), set IsRouter.  This means (7).
	 * So, based on the spec, in (1-5) and (7) cases we must set IsRouter.
	 * A quetion arises for (1) case.  (1) case has no lladdr in the
	 * neighbor cache, this is similar to (6).
	 * This case is rare but we figured that we MUST NOT set IsRouter.
	 *
	 * newentry olladdr  lladdr  llchange	    NS  RS  RA	redir
	 *							D R
	 *	0	n	n	--	(1)	c   ?     s
	 *	0	y	n	--	(2)	c   s     s
	 *	0	n	y	--	(3)	c   s     s
	 *	0	y	y	n	(4)	c   s     s
	 *	0	y	y	y	(5)	c   s     s
	 *	1	--	n	--	(6) c	c 	c s
	 *	1	--	y	--	(7) c	c   s	c s
	 *
	 *					(c=clear s=set)
	 */
	switch (type & 0xff) {
	case ND_NEIGHBOR_SOLICIT:
		/*
		 * New entry must have is_router flag cleared.
		 */
		if (is_newentry)	/* (6-7) */
			ln->ln_router = 0;
		break;
	case ND_REDIRECT:
		/*
		 * If the icmp is a redirect to a better router, always set the
		 * is_router flag.  Otherwise, if the entry is newly created,
		 * clear the flag.  [RFC 2461, sec 8.3]
		 */
		if (code == ND_REDIRECT_ROUTER)
			ln->ln_router = 1;
		else if (is_newentry) /* (6-7) */
			ln->ln_router = 0;
		break;
	case ND_ROUTER_SOLICIT:
		/*
		 * is_router flag must always be cleared.
		 */
		ln->ln_router = 0;
		break;
	case ND_ROUTER_ADVERT:
		/*
		 * Mark an entry with lladdr as a router.
		 */
		if ((!is_newentry && (olladdr || lladdr)) ||	/* (2-5) */
		    (is_newentry && lladdr)) {			/* (7) */
			ln->ln_router = 1;
		}
		break;
	}

	if (do_update)
		rt_newmsg(RTM_CHANGE, rt);  /* tell user process */

	/*
	 * When the link-layer address of a router changes, select the
	 * best router again.  In particular, when the neighbor entry is newly
	 * created, it might affect the selection policy.
	 * Question: can we restrict the first condition to the "is_newentry"
	 * case?
	 * XXX: when we hear an RA from a new router with the link-layer
	 * address option, defrouter_select() is called twice, since
	 * defrtrlist_update called the function as well.  However, I believe
	 * we can compromise the overhead, since it only happens the first
	 * time.
	 * XXX: although defrouter_select() should not have a bad effect
	 * for those are not autoconfigured hosts, we explicitly avoid such
	 * cases for safety.
	 */
	if (do_update && ln->ln_router && !ip6_forwarding &&
	    nd6_accepts_rtadv(ndi))
		defrouter_select();

	rtfree(rt);
}

static void
nd6_slowtimo(void *ignored_arg)
{
	struct nd_ifinfo *nd6if;
	struct ifnet *ifp;

	mutex_enter(softnet_lock);
	KERNEL_LOCK(1, NULL);
	callout_reset(&nd6_slowtimo_ch, ND6_SLOWTIMER_INTERVAL * hz,
	    nd6_slowtimo, NULL);
	IFNET_FOREACH(ifp) {
		nd6if = ND_IFINFO(ifp);
		if (nd6if->basereachable && /* already initialized */
		    (nd6if->recalctm -= ND6_SLOWTIMER_INTERVAL) <= 0) {
			/*
			 * Since reachable time rarely changes by router
			 * advertisements, we SHOULD insure that a new random
			 * value gets recomputed at least once every few hours.
			 * (RFC 2461, 6.3.4)
			 */
			nd6if->recalctm = nd6_recalc_reachtm_interval;
			nd6if->reachable = ND_COMPUTE_RTIME(nd6if->basereachable);
		}
	}
	KERNEL_UNLOCK_ONE(NULL);
	mutex_exit(softnet_lock);
}

/*
 * Next hop determination.  This routine was derived from ether_output.
 */
static int
nd6_determine_nexthop(struct ifnet *ifp, const struct sockaddr_in6 *dst,
    struct rtentry *rt00, struct rtentry **ret_rt, bool *sendpkt)
{
	struct rtentry *rt, *rt0;
	struct rtentry *gwrt;
	struct sockaddr_in6 *gw6;

#define RTFREE_IF_NEEDED(_rt) \
	if ((_rt) != NULL && (_rt) != rt00) \
		rtfree((_rt));

	KASSERT(rt00 != NULL);

	rt = rt0 = rt00;

	if ((rt->rt_flags & RTF_UP) == 0) {
		rt0 = rt = rtalloc1(sin6tocsa(dst), 1);
		if (rt == NULL)
			goto hostunreach;
		if (rt->rt_ifp != ifp)
			goto hostunreach;
	}

	if ((rt->rt_flags & RTF_GATEWAY) == 0)
		goto out;

	gw6 = (struct sockaddr_in6 *)rt->rt_gateway;

	/*
	 * We skip link-layer address resolution and NUD
	 * if the gateway is not a neighbor from ND point
	 * of view, regardless of the value of nd_ifinfo.flags.
	 * The second condition is a bit tricky; we skip
	 * if the gateway is our own address, which is
	 * sometimes used to install a route to a p2p link.
	 */
	if (!nd6_is_addr_neighbor(gw6, ifp) ||
	    in6ifa_ifpwithaddr(ifp, &gw6->sin6_addr)) {
		/*
		 * We allow this kind of tricky route only
		 * when the outgoing interface is p2p.
		 * XXX: we may need a more generic rule here.
		 */
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0)
			goto hostunreach;

		*sendpkt = true;
		goto out;
	}

	/* Try to use a cached nexthop route (gwroute) if exists */
	gwrt = rt_get_gwroute(rt);
	if (gwrt == NULL)
		goto lookup;

	RTFREE_IF_NEEDED(rt);
	rt = gwrt;
	if ((rt->rt_flags & RTF_UP) == 0) {
		RTFREE_IF_NEEDED(rt);
		rt = rt0;
	lookup:
		/* Look up a nexthop route */
		gwrt = rtalloc1(rt->rt_gateway, 1);
		rt_set_gwroute(rt, gwrt);
		RTFREE_IF_NEEDED(rt);
		rt = gwrt;
		if (rt == NULL)
			goto hostunreach;
		/* the "G" test below also prevents rt == rt0 */
		if ((rt->rt_flags & RTF_GATEWAY) ||
		    (rt->rt_ifp != ifp)) {
			if (rt0->rt_gwroute != NULL)
				rtfree(rt0->rt_gwroute);
			rt0->rt_gwroute = NULL;
			goto hostunreach;
		}
	}

out:
	*ret_rt = rt;
	return 0;

hostunreach:
	RTFREE_IF_NEEDED(rt);

	return EHOSTUNREACH;
#undef RTFREE_IF_NEEDED
}

#define senderr(e) { error = (e); goto bad;}
int
nd6_output(struct ifnet *ifp, struct ifnet *origifp, struct mbuf *m0,
    const struct sockaddr_in6 *dst, struct rtentry *rt0)
{
	struct mbuf *m = m0;
	struct rtentry *rt = rt0;
	struct llinfo_nd6 *ln = NULL;
	int error = 0;

#define RTFREE_IF_NEEDED(_rt) \
	if ((_rt) != NULL && (_rt) != rt0) \
		rtfree((_rt));

	if (IN6_IS_ADDR_MULTICAST(&dst->sin6_addr))
		goto sendpkt;

	if (nd6_need_cache(ifp) == 0)
		goto sendpkt;

	if (rt) {
		struct rtentry *nexthop = NULL;
		bool sendpkt = false;

		error = nd6_determine_nexthop(ifp, dst, rt, &nexthop, &sendpkt);
		if (error != 0)
			senderr(error);
		rt = nexthop;
		if (sendpkt)
			goto sendpkt;
	}

	/*
	 * Address resolution or Neighbor Unreachability Detection
	 * for the next hop.
	 * At this point, the destination of the packet must be a unicast
	 * or an anycast address(i.e. not a multicast).
	 */

	/* Look up the neighbor cache for the nexthop */
	if (rt != NULL && (rt->rt_flags & RTF_LLINFO) != 0)
		ln = (struct llinfo_nd6 *)rt->rt_llinfo;
	else {
		/*
		 * Since nd6_is_addr_neighbor() internally calls nd6_lookup(),
		 * the condition below is not very efficient.  But we believe
		 * it is tolerable, because this should be a rare case.
		 */
		if (nd6_is_addr_neighbor(dst, ifp)) {
			RTFREE_IF_NEEDED(rt);
			rt = nd6_lookup(&dst->sin6_addr, 1, ifp);
			if (rt != NULL)
				ln = (struct llinfo_nd6 *)rt->rt_llinfo;
		}
	}
	if (ln == NULL || rt == NULL) {
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0 &&
		    !(ND_IFINFO(ifp)->flags & ND6_IFF_PERFORMNUD)) {
			log(LOG_DEBUG,
			    "nd6_output: can't allocate llinfo for %s "
			    "(ln=%p, rt=%p)\n",
			    ip6_sprintf(&dst->sin6_addr), ln, rt);
			senderr(EIO);	/* XXX: good error? */
		}

		goto sendpkt;	/* send anyway */
	}

	/*
	 * Move this entry to the head of the queue so that it is less likely
	 * for this entry to be a target of forced garbage collection (see
	 * nd6_rtrequest()).
	 */
	LN_DEQUEUE(ln);
	LN_INSERTHEAD(ln);

	/* We don't have to do link-layer address resolution on a p2p link. */
	if ((ifp->if_flags & IFF_POINTOPOINT) != 0 &&
	    ln->ln_state < ND6_LLINFO_REACHABLE) {
		ln->ln_state = ND6_LLINFO_STALE;
		nd6_llinfo_settimer(ln, (long)nd6_gctimer * hz);
	}

	/*
	 * The first time we send a packet to a neighbor whose entry is
	 * STALE, we have to change the state to DELAY and a sets a timer to
	 * expire in DELAY_FIRST_PROBE_TIME seconds to ensure do
	 * neighbor unreachability detection on expiration.
	 * (RFC 2461 7.3.3)
	 */
	if (ln->ln_state == ND6_LLINFO_STALE) {
		ln->ln_asked = 0;
		ln->ln_state = ND6_LLINFO_DELAY;
		nd6_llinfo_settimer(ln, (long)nd6_delay * hz);
	}

	/*
	 * If the neighbor cache entry has a state other than INCOMPLETE
	 * (i.e. its link-layer address is already resolved), just
	 * send the packet.
	 */
	if (ln->ln_state > ND6_LLINFO_INCOMPLETE)
		goto sendpkt;

	/*
	 * There is a neighbor cache entry, but no ethernet address
	 * response yet.  Append this latest packet to the end of the
	 * packet queue in the mbuf, unless the number of the packet
	 * does not exceed nd6_maxqueuelen.  When it exceeds nd6_maxqueuelen,
	 * the oldest packet in the queue will be removed.
	 */
	if (ln->ln_state == ND6_LLINFO_NOSTATE)
		ln->ln_state = ND6_LLINFO_INCOMPLETE;
	if (ln->ln_hold) {
		struct mbuf *m_hold;
		int i;

		i = 0;
		for (m_hold = ln->ln_hold; m_hold; m_hold = m_hold->m_nextpkt) {
			i++;
			if (m_hold->m_nextpkt == NULL) {
				m_hold->m_nextpkt = m;
				break;
			}
		}
		while (i >= nd6_maxqueuelen) {
			m_hold = ln->ln_hold;
			ln->ln_hold = ln->ln_hold->m_nextpkt;
			m_freem(m_hold);
			i--;
		}
	} else {
		ln->ln_hold = m;
	}

	/*
	 * If there has been no NS for the neighbor after entering the
	 * INCOMPLETE state, send the first solicitation.
	 */
	if (!ND6_LLINFO_PERMANENT(ln) && ln->ln_asked == 0) {
		ln->ln_asked++;
		nd6_llinfo_settimer(ln,
		    (long)ND_IFINFO(ifp)->retrans * hz / 1000);
		nd6_ns_output(ifp, NULL, &dst->sin6_addr, ln, 0);
	}
	error = 0;
	goto exit;

  sendpkt:
	/* discard the packet if IPv6 operation is disabled on the interface */
	if ((ND_IFINFO(ifp)->flags & ND6_IFF_IFDISABLED)) {
		error = ENETDOWN; /* better error? */
		goto bad;
	}

#ifndef NET_MPSAFE
	KERNEL_LOCK(1, NULL);
#endif
	if ((ifp->if_flags & IFF_LOOPBACK) != 0)
		error = (*ifp->if_output)(origifp, m, sin6tocsa(dst), rt);
	else
		error = (*ifp->if_output)(ifp, m, sin6tocsa(dst), rt);
#ifndef NET_MPSAFE
	KERNEL_UNLOCK_ONE(NULL);
#endif
	goto exit;

  bad:
	if (m != NULL)
		m_freem(m);
  exit:
	RTFREE_IF_NEEDED(rt);

	return error;
#undef RTFREE_IF_NEEDED
}
#undef senderr

int
nd6_need_cache(struct ifnet *ifp)
{
	/*
	 * XXX: we currently do not make neighbor cache on any interface
	 * other than ARCnet, Ethernet, FDDI and GIF.
	 *
	 * RFC2893 says:
	 * - unidirectional tunnels needs no ND
	 */
	switch (ifp->if_type) {
	case IFT_ARCNET:
	case IFT_ETHER:
	case IFT_FDDI:
	case IFT_IEEE1394:
	case IFT_CARP:
	case IFT_GIF:		/* XXX need more cases? */
	case IFT_PPP:
	case IFT_TUNNEL:
		return 1;
	default:
		return 0;
	}
}

int
nd6_storelladdr(const struct ifnet *ifp, const struct rtentry *rt,
    struct mbuf *m, const struct sockaddr *dst, uint8_t *lldst,
    size_t dstsize)
{
	const struct sockaddr_dl *sdl;

	if (m->m_flags & M_MCAST) {
		switch (ifp->if_type) {
		case IFT_ETHER:
		case IFT_FDDI:
			ETHER_MAP_IPV6_MULTICAST(&satocsin6(dst)->sin6_addr,
			    lldst);
			return 1;
		case IFT_IEEE1394:
			memcpy(lldst, ifp->if_broadcastaddr,
			    MIN(dstsize, ifp->if_addrlen));
			return 1;
		case IFT_ARCNET:
			*lldst = 0;
			return 1;
		default:
			m_freem(m);
			return 0;
		}
	}

	if (rt == NULL) {
		/* this could happen, if we could not allocate memory */
		m_freem(m);
		return 0;
	}
	if (rt->rt_gateway->sa_family != AF_LINK) {
		char gbuf[256];
		char dbuf[LINK_ADDRSTRLEN];
		sockaddr_format(rt->rt_gateway, gbuf, sizeof(gbuf));
		printf("%s: bad gateway address type %s for dst %s"
		    " through interface %s\n", __func__, gbuf, 
		    IN6_PRINT(dbuf, &satocsin6(dst)->sin6_addr),
		    if_name(ifp));
		m_freem(m);
		return 0;
	}
	sdl = satocsdl(rt->rt_gateway);
	if (sdl->sdl_alen == 0 || sdl->sdl_alen > dstsize) {
		char sbuf[INET6_ADDRSTRLEN];
		char dbuf[LINK_ADDRSTRLEN];
		/* this should be impossible, but we bark here for debugging */
		printf("%s: sdl_alen == %" PRIu8 ", if=%s, dst=%s, sdl=%s\n",
		    __func__, sdl->sdl_alen, if_name(ifp),
		    IN6_PRINT(sbuf, &satocsin6(dst)->sin6_addr),
		    DL_PRINT(dbuf, &sdl->sdl_addr));
		m_freem(m);
		return 0;
	}

	memcpy(lldst, CLLADDR(sdl), MIN(dstsize, sdl->sdl_alen));
	return 1;
}

static void 
clear_llinfo_pqueue(struct llinfo_nd6 *ln)
{
	struct mbuf *m_hold, *m_hold_next;

	for (m_hold = ln->ln_hold; m_hold; m_hold = m_hold_next) {
		m_hold_next = m_hold->m_nextpkt;
		m_hold->m_nextpkt = NULL;
		m_freem(m_hold);
	}

	ln->ln_hold = NULL;
	return;
}
 
int
nd6_sysctl(
    int name,
    void *oldp,	/* syscall arg, need copyout */
    size_t *oldlenp,
    void *newp,	/* syscall arg, need copyin */
    size_t newlen
)
{
	void *p;
	size_t ol;
	int error;

	error = 0;

	if (newp)
		return EPERM;
	if (oldp && !oldlenp)
		return EINVAL;
	ol = oldlenp ? *oldlenp : 0;

	if (oldp) {
		p = malloc(*oldlenp, M_TEMP, M_WAITOK);
		if (p == NULL)
			return ENOMEM;
	} else
		p = NULL;
	switch (name) {
	case ICMPV6CTL_ND6_DRLIST:
		error = fill_drlist(p, oldlenp, ol);
		if (!error && p != NULL && oldp != NULL)
			error = copyout(p, oldp, *oldlenp);
		break;

	case ICMPV6CTL_ND6_PRLIST:
		error = fill_prlist(p, oldlenp, ol);
		if (!error && p != NULL && oldp != NULL)
			error = copyout(p, oldp, *oldlenp);
		break;

	case ICMPV6CTL_ND6_MAXQLEN:
		break;

	default:
		error = ENOPROTOOPT;
		break;
	}
	if (p)
		free(p, M_TEMP);

	return error;
}

static int
fill_drlist(void *oldp, size_t *oldlenp, size_t ol)
{
	int error = 0, s;
	struct in6_defrouter *d = NULL, *de = NULL;
	struct nd_defrouter *dr;
	size_t l;

	s = splsoftnet();

	if (oldp) {
		d = (struct in6_defrouter *)oldp;
		de = (struct in6_defrouter *)((char *)oldp + *oldlenp);
	}
	l = 0;

	TAILQ_FOREACH(dr, &nd_defrouter, dr_entry) {

		if (oldp && d + 1 <= de) {
			memset(d, 0, sizeof(*d));
			sockaddr_in6_init(&d->rtaddr, &dr->rtaddr, 0, 0, 0);
			if (sa6_recoverscope(&d->rtaddr)) {
				log(LOG_ERR,
				    "scope error in router list (%s)\n",
				    ip6_sprintf(&d->rtaddr.sin6_addr));
				/* XXX: press on... */
			}
			d->flags = dr->flags;
			d->rtlifetime = dr->rtlifetime;
			d->expire = dr->expire ?
			    time_mono_to_wall(dr->expire) : 0;
			d->if_index = dr->ifp->if_index;
		}

		l += sizeof(*d);
		if (d)
			d++;
	}

	if (oldp) {
		if (l > ol)
			error = ENOMEM;
	}
	if (oldlenp)
		*oldlenp = l;	/* (void *)d - (void *)oldp */

	splx(s);

	return error;
}

static int
fill_prlist(void *oldp, size_t *oldlenp, size_t ol)
{
	int error = 0, s;
	struct nd_prefix *pr;
	uint8_t *p = NULL, *ps = NULL;
	uint8_t *pe = NULL;
	size_t l;

	s = splsoftnet();

	if (oldp) {
		ps = p = (uint8_t*)oldp;
		pe = (uint8_t*)oldp + *oldlenp;
	}
	l = 0;

	LIST_FOREACH(pr, &nd_prefix, ndpr_entry) {
		u_short advrtrs;
		struct sockaddr_in6 sin6;
		struct nd_pfxrouter *pfr;
		struct in6_prefix pfx;

		if (oldp && p + sizeof(struct in6_prefix) <= pe)
		{
			memset(&pfx, 0, sizeof(pfx));
			ps = p;
			pfx.prefix = pr->ndpr_prefix;

			if (sa6_recoverscope(&pfx.prefix)) {
				log(LOG_ERR,
				    "scope error in prefix list (%s)\n",
				    ip6_sprintf(&pfx.prefix.sin6_addr));
				/* XXX: press on... */
			}
			pfx.raflags = pr->ndpr_raf;
			pfx.prefixlen = pr->ndpr_plen;
			pfx.vltime = pr->ndpr_vltime;
			pfx.pltime = pr->ndpr_pltime;
			pfx.if_index = pr->ndpr_ifp->if_index;
			if (pr->ndpr_vltime == ND6_INFINITE_LIFETIME)
				pfx.expire = 0;
			else {
				time_t maxexpire;

				/* XXX: we assume time_t is signed. */
				maxexpire = (-1) &
				    ~((time_t)1 <<
				    ((sizeof(maxexpire) * 8) - 1));
				if (pr->ndpr_vltime <
				    maxexpire - pr->ndpr_lastupdate) {
					pfx.expire = pr->ndpr_lastupdate +
						pr->ndpr_vltime;
				} else
					pfx.expire = maxexpire;
			}
			pfx.refcnt = pr->ndpr_refcnt;
			pfx.flags = pr->ndpr_stateflags;
			pfx.origin = PR_ORIG_RA;

			p += sizeof(pfx); l += sizeof(pfx);

			advrtrs = 0;
			LIST_FOREACH(pfr, &pr->ndpr_advrtrs, pfr_entry) {
				if (p + sizeof(sin6) > pe) {
					advrtrs++;
					continue;
				}

				sockaddr_in6_init(&sin6, &pfr->router->rtaddr,
				    0, 0, 0);
				if (sa6_recoverscope(&sin6)) {
					log(LOG_ERR,
					    "scope error in "
					    "prefix list (%s)\n",
					    ip6_sprintf(&pfr->router->rtaddr));
				}
				advrtrs++;
				memcpy(p, &sin6, sizeof(sin6));
				p += sizeof(sin6);
				l += sizeof(sin6);
			}
			pfx.advrtrs = advrtrs;
			memcpy(ps, &pfx, sizeof(pfx));
		}
		else {
			l += sizeof(pfx);
			advrtrs = 0;
			LIST_FOREACH(pfr, &pr->ndpr_advrtrs, pfr_entry) {
				advrtrs++;
				l += sizeof(sin6);
			}
		}
	}

	if (oldp) {
		*oldlenp = l;	/* (void *)d - (void *)oldp */
		if (l > ol)
			error = ENOMEM;
	} else
		*oldlenp = l;

	splx(s);

	return error;
}
