#include <sys/cdefs.h>
 __RCSID("$NetBSD: ipv4.c,v 1.17 2015/08/21 10:39:00 roy Exp $");

/*
 * dhcpcd - DHCP client daemon
 * Copyright (c) 2006-2015 Roy Marples <roy@marples.name>
 * All rights reserved

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/socket.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "arp.h"
#include "common.h"
#include "dhcpcd.h"
#include "dhcp.h"
#include "if.h"
#include "if-options.h"
#include "ipv4.h"
#include "ipv4ll.h"
#include "script.h"

#define IPV4_LOOPBACK_ROUTE
#if defined(__linux__) || (defined(BSD) && defined(RTF_LOCAL))
/* Linux has had loopback routes in the local table since 2.2 */
#undef IPV4_LOOPBACK_ROUTE
#endif

uint8_t
inet_ntocidr(struct in_addr address)
{
	uint8_t cidr = 0;
	uint32_t mask = htonl(address.s_addr);

	while (mask) {
		cidr++;
		mask <<= 1;
	}
	return cidr;
}

int
inet_cidrtoaddr(int cidr, struct in_addr *addr)
{
	int ocets;

	if (cidr < 1 || cidr > 32) {
		errno = EINVAL;
		return -1;
	}
	ocets = (cidr + 7) / NBBY;

	addr->s_addr = 0;
	if (ocets > 0) {
		memset(&addr->s_addr, 255, (size_t)ocets - 1);
		memset((unsigned char *)&addr->s_addr + (ocets - 1),
		    (256 - (1 << (32 - cidr) % NBBY)), 1);
	}

	return 0;
}

uint32_t
ipv4_getnetmask(uint32_t addr)
{
	uint32_t dst;

	if (addr == 0)
		return 0;

	dst = htonl(addr);
	if (IN_CLASSA(dst))
		return ntohl(IN_CLASSA_NET);
	if (IN_CLASSB(dst))
		return ntohl(IN_CLASSB_NET);
	if (IN_CLASSC(dst))
		return ntohl(IN_CLASSC_NET);

	return 0;
}

struct ipv4_addr *
ipv4_iffindaddr(struct interface *ifp,
    const struct in_addr *addr, const struct in_addr *net)
{
	struct ipv4_state *state;
	struct ipv4_addr *ap;

	state = IPV4_STATE(ifp);
	if (state) {
		TAILQ_FOREACH(ap, &state->addrs, next) {
			if ((addr == NULL || ap->addr.s_addr == addr->s_addr) &&
			    (net == NULL || ap->net.s_addr == net->s_addr))
				return ap;
		}
	}
	return NULL;
}

struct ipv4_addr *
ipv4_iffindlladdr(struct interface *ifp)
{
	struct ipv4_state *state;
	struct ipv4_addr *ap;

	state = IPV4_STATE(ifp);
	if (state) {
		TAILQ_FOREACH(ap, &state->addrs, next) {
			if (IN_LINKLOCAL(htonl(ap->addr.s_addr)))
				return ap;
		}
	}
	return NULL;
}

struct ipv4_addr *
ipv4_findaddr(struct dhcpcd_ctx *ctx, const struct in_addr *addr)
{
	struct interface *ifp;
	struct ipv4_addr *ap;

	TAILQ_FOREACH(ifp, ctx->ifaces, next) {
		ap = ipv4_iffindaddr(ifp, addr, NULL);
		if (ap)
			return ap;
	}
	return NULL;
}

int
ipv4_hasaddr(const struct interface *ifp)
{
	const struct dhcp_state *dstate;
	const struct ipv4ll_state *istate;

	dstate = D_CSTATE(ifp);
	istate = IPV4LL_CSTATE(ifp);
	return ((dstate &&
	    dstate->added == STATE_ADDED &&
	    dstate->addr.s_addr != INADDR_ANY) ||
	    (istate && istate->addr.s_addr != INADDR_ANY));
}

void
ipv4_freeroutes(struct rt_head *rts)
{

	if (rts) {
		ipv4_freerts(rts);
		free(rts);
	}
}

int
ipv4_init(struct dhcpcd_ctx *ctx)
{

	if (ctx->ipv4_routes == NULL) {
		ctx->ipv4_routes = malloc(sizeof(*ctx->ipv4_routes));
		if (ctx->ipv4_routes == NULL)
			return -1;
		TAILQ_INIT(ctx->ipv4_routes);
	}
	if (ctx->ipv4_kroutes == NULL) {
		ctx->ipv4_kroutes = malloc(sizeof(*ctx->ipv4_kroutes));
		if (ctx->ipv4_kroutes == NULL)
			return -1;
		TAILQ_INIT(ctx->ipv4_kroutes);
	}
	return 0;
}

int
ipv4_protocol_fd(const struct interface *ifp, uint16_t protocol)
{

	if (protocol == ETHERTYPE_ARP) {
		const struct iarp_state *istate;

		istate = ARP_CSTATE(ifp);
		assert(istate != NULL);
		return istate->fd;
	} else {
		const struct dhcp_state *dstate;

		dstate = D_CSTATE(ifp);
		assert(dstate != NULL);
		return dstate->raw_fd;
	}
}

/* Interface comparer for working out ordering. */
int
ipv4_ifcmp(const struct interface *si, const struct interface *ti)
{
	const struct dhcp_state *sis, *tis;

	sis = D_CSTATE(si);
	tis = D_CSTATE(ti);
	if (sis && !tis)
		return -1;
	if (!sis && tis)
		return 1;
	if (!sis && !tis)
		return 0;
	/* If one has a lease and the other not, it takes precedence. */
	if (sis->new && !tis->new)
		return -1;
	if (!sis->new && tis->new)
		return 1;
	/* Always prefer proper leases */
	if (!(sis->added & STATE_FAKE) && (sis->added & STATE_FAKE))
		return -1;
	if ((sis->added & STATE_FAKE) && !(sis->added & STATE_FAKE))
		return 1;
	/* If we are either, they neither have a lease, or they both have.
	 * We need to check for IPv4LL and make it non-preferred. */
	if (sis->new && tis->new) {
		int sill = (sis->new->cookie == htonl(MAGIC_COOKIE));
		int till = (tis->new->cookie == htonl(MAGIC_COOKIE));
		if (sill && !till)
			return -1;
		if (!sill && till)
			return 1;
	}
	return 0;
}

static struct rt *
find_route(struct rt_head *rts, const struct rt *r, const struct rt *srt)
{
	struct rt *rt;

	if (rts == NULL)
		return NULL;
	TAILQ_FOREACH(rt, rts, next) {
		if (rt->dest.s_addr == r->dest.s_addr &&
#ifdef HAVE_ROUTE_METRIC
		    (srt || (r->iface == NULL || rt->iface == NULL ||
		    rt->iface->metric == r->iface->metric)) &&
#endif
                    (!srt || srt != rt) &&
		    rt->net.s_addr == r->net.s_addr)
			return rt;
	}
	return NULL;
}

static void
desc_route(const char *cmd, const struct rt *rt)
{
	char addr[sizeof("000.000.000.000") + 1];
	struct dhcpcd_ctx *ctx = rt->iface ? rt->iface->ctx : NULL;
	const char *ifname = rt->iface ? rt->iface->name : NULL;

	strlcpy(addr, inet_ntoa(rt->dest), sizeof(addr));
	if (rt->net.s_addr == htonl(INADDR_BROADCAST) &&
	    rt->gate.s_addr == htonl(INADDR_ANY))
		logger(ctx, LOG_INFO, "%s: %s host route to %s",
		    ifname, cmd, addr);
	else if (rt->net.s_addr == htonl(INADDR_BROADCAST))
		logger(ctx, LOG_INFO, "%s: %s host route to %s via %s",
		    ifname, cmd, addr, inet_ntoa(rt->gate));
	else if (rt->dest.s_addr == htonl(INADDR_ANY) &&
	    rt->net.s_addr == htonl(INADDR_ANY) &&
	    rt->gate.s_addr == htonl(INADDR_ANY))
		logger(ctx, LOG_INFO, "%s: %s default route",
		    ifname, cmd);
	else if (rt->gate.s_addr == htonl(INADDR_ANY))
		logger(ctx, LOG_INFO, "%s: %s route to %s/%d",
		    ifname, cmd, addr, inet_ntocidr(rt->net));
	else if (rt->dest.s_addr == htonl(INADDR_ANY) &&
	    rt->net.s_addr == htonl(INADDR_ANY))
		logger(ctx, LOG_INFO, "%s: %s default route via %s",
		    ifname, cmd, inet_ntoa(rt->gate));
	else
		logger(ctx, LOG_INFO, "%s: %s route to %s/%d via %s",
		    ifname, cmd, addr, inet_ntocidr(rt->net),
		    inet_ntoa(rt->gate));
}

static struct rt *
ipv4_findrt(struct dhcpcd_ctx *ctx, const struct rt *rt, int flags)
{
	struct rt *r;

	if (ctx->ipv4_kroutes == NULL)
		return NULL;
	TAILQ_FOREACH(r, ctx->ipv4_kroutes, next) {
		if (rt->dest.s_addr == r->dest.s_addr &&
#ifdef HAVE_ROUTE_METRIC
		    rt->iface == r->iface &&
		    (!flags || rt->metric == r->metric) &&
#else
		    (!flags || rt->iface == r->iface) &&
#endif
		    rt->net.s_addr == r->net.s_addr)
			return r;
	}
	return NULL;
}

void
ipv4_freerts(struct rt_head *routes)
{
	struct rt *rt;

	while ((rt = TAILQ_FIRST(routes))) {
		TAILQ_REMOVE(routes, rt, next);
		free(rt);
	}
}

/* If something other than dhcpcd removes a route,
 * we need to remove it from our internal table. */
int
ipv4_handlert(struct dhcpcd_ctx *ctx, int cmd, struct rt *rt)
{
	struct rt *f;

	if (ctx->ipv4_kroutes == NULL)
		return 0;

	f = ipv4_findrt(ctx, rt, 1);
	switch (cmd) {
	case RTM_ADD:
		if (f == NULL) {
			if ((f = malloc(sizeof(*f))) == NULL)
				return -1;
			*f = *rt;
			TAILQ_INSERT_TAIL(ctx->ipv4_kroutes, f, next);
		}
		break;
	case RTM_DELETE:
		if (f) {
			TAILQ_REMOVE(ctx->ipv4_kroutes, f, next);
			free(f);
		}

		/* If we manage the route, remove it */
		if ((f = find_route(ctx->ipv4_routes, rt, NULL))) {
			desc_route("removing", f);
			TAILQ_REMOVE(ctx->ipv4_routes, f, next);
			free(f);
		}
		break;
	}
	return 0;
}

#define n_route(a)	 nc_route(NULL, a)
#define c_route(a, b)	 nc_route(a, b)
static int
nc_route(struct rt *ort, struct rt *nrt)
{
	int change;

	/* Don't set default routes if not asked to */
	if (nrt->dest.s_addr == 0 &&
	    nrt->net.s_addr == 0 &&
	    !(nrt->iface->options->options & DHCPCD_GATEWAY))
		return -1;

	desc_route(ort == NULL ? "adding" : "changing", nrt);

	change = 0;
	if (ort == NULL) {
		ort = ipv4_findrt(nrt->iface->ctx, nrt, 0);
		if (ort &&
		    ((ort->flags & RTF_REJECT && nrt->flags & RTF_REJECT) ||
		     (ort->iface == nrt->iface &&
#ifdef HAVE_ROUTE_METRIC
		    ort->metric == nrt->metric &&
#endif
		    ort->gate.s_addr == nrt->gate.s_addr)))
		{
			if (ort->mtu == nrt->mtu)
				return 0;
			change = 1;
		}
	} else if (ort->state & STATE_FAKE && !(nrt->state & STATE_FAKE) &&
	    ort->iface == nrt->iface &&
#ifdef HAVE_ROUTE_METRIC
	    ort->metric == nrt->metric &&
#endif
	    ort->dest.s_addr == nrt->dest.s_addr &&
	    ort->net.s_addr ==  nrt->net.s_addr &&
	    ort->gate.s_addr == nrt->gate.s_addr)
	{
		if (ort->mtu == nrt->mtu)
			return 0;
		change = 1;
	}

#ifdef RTF_CLONING
	/* BSD can set routes to be cloning routes.
	 * Cloned routes inherit the parent flags.
	 * As such, we need to delete and re-add the route to flush children
	 * to correct the flags. */
	if (change && ort != NULL && ort->flags & RTF_CLONING)
		change = 0;
#endif

	if (change) {
		if (if_route(RTM_CHANGE, nrt) == 0)
			return 0;
		if (errno != ESRCH)
			logger(nrt->iface->ctx, LOG_ERR, "if_route (CHG): %m");
	}

#ifdef HAVE_ROUTE_METRIC
	/* With route metrics, we can safely add the new route before
	 * deleting the old route. */
	if (if_route(RTM_ADD, nrt) == 0) {
		if (ort && if_route(RTM_DELETE, ort) == -1 && errno != ESRCH)
			logger(nrt->iface->ctx, LOG_ERR, "if_route (DEL): %m");
		return 0;
	}

	/* If the kernel claims the route exists we need to rip out the
	 * old one first. */
	if (errno != EEXIST || ort == NULL)
		goto logerr;
#endif

	/* No route metrics, we need to delete the old route before
	 * adding the new one. */
	if (ort && if_route(RTM_DELETE, ort) == -1 && errno != ESRCH)
		logger(nrt->iface->ctx, LOG_ERR, "if_route (DEL): %m");
	if (if_route(RTM_ADD, nrt) == 0)
		return 0;
#ifdef HAVE_ROUTE_METRIC
logerr:
#endif
	logger(nrt->iface->ctx, LOG_ERR, "if_route (ADD): %m");
	return -1;
}

static int
d_route(struct rt *rt)
{
	int retval;

	desc_route("deleting", rt);
	retval = if_route(RTM_DELETE, rt);
	if (retval != 0 && errno != ENOENT && errno != ESRCH)
		logger(rt->iface->ctx, LOG_ERR,
		    "%s: if_delroute: %m", rt->iface->name);
	return retval;
}

static struct rt_head *
add_subnet_route(struct rt_head *rt, const struct interface *ifp)
{
	const struct dhcp_state *s;
	struct rt *r;

	if (rt == NULL) /* earlier malloc failed */
		return NULL;

	s = D_CSTATE(ifp);
	/* Don't create a subnet route for these addresses */
	if (s->net.s_addr == INADDR_ANY)
		return rt;
#ifndef BSD
	/* BSD adds a route in this instance */
	if (s->net.s_addr == INADDR_BROADCAST)
		return rt;
#endif

	if ((r = calloc(1, sizeof(*r))) == NULL) {
		logger(ifp->ctx, LOG_ERR, "%s: %m", __func__);
		ipv4_freeroutes(rt);
		return NULL;
	}
	r->dest.s_addr = s->addr.s_addr & s->net.s_addr;
	r->net.s_addr = s->net.s_addr;
	r->gate.s_addr = INADDR_ANY;
	r->mtu = dhcp_get_mtu(ifp);
	r->src = s->addr;

	TAILQ_INSERT_HEAD(rt, r, next);
	return rt;
}

#ifdef IPV4_LOOPBACK_ROUTE
static struct rt_head *
add_loopback_route(struct rt_head *rt, const struct interface *ifp)
{
	struct rt *r;
	const struct dhcp_state *s;

	if (rt == NULL) /* earlier malloc failed */
		return NULL;

	s = D_CSTATE(ifp);
	if (s->addr.s_addr == INADDR_ANY)
		return rt;

	if ((r = calloc(1, sizeof(*r))) == NULL) {
		logger(ifp->ctx, LOG_ERR, "%s: %m", __func__);
		ipv4_freeroutes(rt);
		return NULL;
	}
	r->dest = s->addr;
	r->net.s_addr = INADDR_BROADCAST;
	r->gate.s_addr = htonl(INADDR_LOOPBACK);
	r->mtu = dhcp_get_mtu(ifp);
	r->src = s->addr;
	TAILQ_INSERT_HEAD(rt, r, next);
	return rt;
}
#endif

static struct rt_head *
get_routes(struct interface *ifp)
{
	struct rt_head *nrt;
	struct rt *rt, *r = NULL;
	const struct dhcp_state *state;

	if (ifp->options->routes && TAILQ_FIRST(ifp->options->routes)) {
		if ((nrt = malloc(sizeof(*nrt))) == NULL)
			return NULL;
		TAILQ_INIT(nrt);
		TAILQ_FOREACH(rt, ifp->options->routes, next) {
			if (rt->gate.s_addr == 0)
				break;
			if ((r = calloc(1, sizeof(*r))) == NULL) {
				logger(ifp->ctx, LOG_ERR, "%s: %m", __func__);
				ipv4_freeroutes(nrt);
				return NULL;
			}
			memcpy(r, rt, sizeof(*r));
			TAILQ_INSERT_TAIL(nrt, r, next);
		}
	} else
		nrt = dhcp_get_routes(ifp);

	/* Copy our address as the source address */
	if (nrt) {
		state = D_CSTATE(ifp);
		TAILQ_FOREACH(rt, nrt, next) {
			rt->src = state->addr;
		}
	}

	return nrt;
}

static struct rt_head *
add_destination_route(struct rt_head *rt, const struct interface *ifp)
{
	struct rt *r;
	const struct dhcp_state *state;

	if (rt == NULL || /* failed a malloc earlier probably */
	    !(ifp->flags & IFF_POINTOPOINT) ||
	    !has_option_mask(ifp->options->dstmask, DHO_ROUTER) ||
	    (state = D_CSTATE(ifp)) == NULL)
		return rt;

	if ((r = calloc(1, sizeof(*r))) == NULL) {
		logger(ifp->ctx, LOG_ERR, "%s: %m", __func__);
		ipv4_freeroutes(rt);
		return NULL;
	}
	r->dest.s_addr = INADDR_ANY;
	r->net.s_addr = INADDR_ANY;
	r->gate.s_addr = state->dst.s_addr;
	r->mtu = dhcp_get_mtu(ifp);
	r->src = state->addr;
	TAILQ_INSERT_HEAD(rt, r, next);
	return rt;
}

/* We should check to ensure the routers are on the same subnet
 * OR supply a host route. If not, warn and add a host route. */
static struct rt_head *
add_router_host_route(struct rt_head *rt, const struct interface *ifp)
{
	struct rt *rtp, *rtn;
	const char *cp, *cp2, *cp3, *cplim;
	struct if_options *ifo;
	const struct dhcp_state *state;

	if (rt == NULL) /* earlier malloc failed */
		return NULL;

	TAILQ_FOREACH(rtp, rt, next) {
		if (rtp->dest.s_addr != INADDR_ANY)
			continue;
		/* Scan for a route to match */
		TAILQ_FOREACH(rtn, rt, next) {
			if (rtn == rtp)
				break;
			/* match host */
			if (rtn->dest.s_addr == rtp->gate.s_addr)
				break;
			/* match subnet */
			cp = (const char *)&rtp->gate.s_addr;
			cp2 = (const char *)&rtn->dest.s_addr;
			cp3 = (const char *)&rtn->net.s_addr;
			cplim = cp3 + sizeof(rtn->net.s_addr);
			while (cp3 < cplim) {
				if ((*cp++ ^ *cp2++) & *cp3++)
					break;
			}
			if (cp3 == cplim)
				break;
		}
		if (rtn != rtp)
			continue;
		if ((state = D_CSTATE(ifp)) == NULL)
			continue;
		ifo = ifp->options;
		if (ifp->flags & IFF_NOARP) {
			if (!(ifo->options & DHCPCD_ROUTER_HOST_ROUTE_WARNED) &&
			    !(state->added & STATE_FAKE))
			{
				ifo->options |= DHCPCD_ROUTER_HOST_ROUTE_WARNED;
				logger(ifp->ctx, LOG_WARNING,
				    "%s: forcing router %s through interface",
				    ifp->name, inet_ntoa(rtp->gate));
			}
			rtp->gate.s_addr = 0;
			continue;
		}
		if (!(ifo->options & DHCPCD_ROUTER_HOST_ROUTE_WARNED) &&
		    !(state->added & STATE_FAKE))
		{
			ifo->options |= DHCPCD_ROUTER_HOST_ROUTE_WARNED;
			logger(ifp->ctx, LOG_WARNING,
			    "%s: router %s requires a host route",
			    ifp->name, inet_ntoa(rtp->gate));
		}
		if ((rtn = calloc(1, sizeof(*rtn))) == NULL) {
			logger(ifp->ctx, LOG_ERR, "%s: %m", __func__);
			ipv4_freeroutes(rt);
			return NULL;
		}
		rtn->dest.s_addr = rtp->gate.s_addr;
		rtn->net.s_addr = htonl(INADDR_BROADCAST);
		rtn->gate.s_addr = htonl(INADDR_ANY);
		rtn->mtu = dhcp_get_mtu(ifp);
		rtn->src = state->addr;
		TAILQ_INSERT_BEFORE(rtp, rtn, next);
	}
	return rt;
}

static int
ipv4_doroute(struct rt *rt, struct rt_head *nrs)
{
	const struct dhcp_state *state;
	struct rt *or;

	state = D_CSTATE(rt->iface);
	rt->state = state->added & STATE_FAKE;
#ifdef HAVE_ROUTE_METRIC
	rt->metric = rt->iface->metric;
#endif
	/* Is this route already in our table? */
	if ((find_route(nrs, rt, NULL)) != NULL)
		return 0;
	/* Do we already manage it? */
	if ((or = find_route(rt->iface->ctx->ipv4_routes, rt, NULL))) {
		if (state->added & STATE_FAKE)
			return 0;
		if (or->state & STATE_FAKE ||
		    or->iface != rt->iface ||
#ifdef HAVE_ROUTE_METRIC
		    rt->metric != or->metric ||
#endif
		    rt->src.s_addr != or->src.s_addr ||
		    rt->gate.s_addr != or->gate.s_addr ||
		    rt->mtu != or->mtu)
		{
			if (c_route(or, rt) != 0)
				return 0;
		}
		TAILQ_REMOVE(rt->iface->ctx->ipv4_routes, or, next);
		free(or);
	} else {
		if (state->added & STATE_FAKE) {
			if ((or = ipv4_findrt(rt->iface->ctx, rt, 1)) == NULL)
				return 0;
			rt->iface = or->iface;
			rt->gate.s_addr = or->gate.s_addr;
#ifdef HAVE_ROUTE_METRIC
			rt->metric = or->metric;
#endif
			rt->mtu = or->mtu;
			rt->flags = or->flags;
		} else {
			if (n_route(rt) != 0)
				return 0;
		}
	}
	return 1;
}

void
ipv4_buildroutes(struct dhcpcd_ctx *ctx)
{
	struct rt_head *nrs, *dnr;
	struct rt *rt, *rtn;
	struct interface *ifp;
	const struct dhcp_state *state;
	int has_default;

	/* We need to have the interfaces in the correct order to ensure
	 * our routes are managed correctly. */
	if_sortinterfaces(ctx);

	if ((nrs = malloc(sizeof(*nrs))) == NULL) {
		logger(ctx, LOG_ERR, "%s: %m", __func__);
		return;
	}
	TAILQ_INIT(nrs);

	has_default = 0;
	TAILQ_FOREACH(ifp, ctx->ifaces, next) {
		state = D_CSTATE(ifp);
		if (state != NULL && state->new != NULL && state->added) {
			dnr = get_routes(ifp);
			dnr = add_subnet_route(dnr, ifp);
		} else
			dnr = NULL;
		if ((rt = ipv4ll_subnet_route(ifp)) != NULL) {
			if (dnr == NULL) {
				if ((dnr = malloc(sizeof(*dnr))) == NULL) {
					logger(ifp->ctx, LOG_ERR,
					    "%s: malloc %m", __func__);
					continue;
				}
				TAILQ_INIT(dnr);
			}
			TAILQ_INSERT_HEAD(dnr, rt, next);
		}
		if (dnr == NULL)
			continue;
#ifdef IPV4_LOOPBACK_ROUTE
		dnr = add_loopback_route(dnr, ifp);
#endif
		if (ifp->options->options & DHCPCD_GATEWAY) {
			dnr = add_router_host_route(dnr, ifp);
			dnr = add_destination_route(dnr, ifp);
		}
		if (dnr == NULL)
			continue;
		TAILQ_FOREACH_SAFE(rt, dnr, next, rtn) {
			rt->iface = ifp;
			if (ipv4_doroute(rt, nrs) == 1) {
				TAILQ_REMOVE(dnr, rt, next);
				TAILQ_INSERT_TAIL(nrs, rt, next);
				if (rt->dest.s_addr == INADDR_ANY)
					has_default = 1;
			}
		}
		ipv4_freeroutes(dnr);
	}

	/* If we don't manage a default route, grab one without a
	 * gateway for any IPv4LL enabled interfaces. */
	if (!has_default) {
		TAILQ_FOREACH(ifp, ctx->ifaces, next) {
			if ((rt = ipv4ll_default_route(ifp)) != NULL) {
				if (ipv4_doroute(rt, nrs) == 1)
					TAILQ_INSERT_TAIL(nrs, rt, next);
				else
					free(rt);
			}
		}
	}

	/* Remove old routes we used to manage */
	if (ctx->ipv4_routes) {
		TAILQ_FOREACH(rt, ctx->ipv4_routes, next) {
			if (find_route(nrs, rt, NULL) == NULL &&
			    (rt->iface->options->options &
			    (DHCPCD_EXITING | DHCPCD_PERSISTENT)) !=
			    (DHCPCD_EXITING | DHCPCD_PERSISTENT))
				d_route(rt);
		}
	}
	ipv4_freeroutes(ctx->ipv4_routes);
	ctx->ipv4_routes = nrs;
}

int
ipv4_deladdr(struct interface *ifp,
    const struct in_addr *addr, const struct in_addr *net, int keeparp)
{
	struct dhcp_state *dstate;
	int r;
	struct ipv4_state *state;
	struct ipv4_addr *ap;
	struct arp_state *astate;

	logger(ifp->ctx, LOG_DEBUG, "%s: deleting IP address %s/%d",
	    ifp->name, inet_ntoa(*addr), inet_ntocidr(*net));

	r = if_deladdress(ifp, addr, net);
	if (r == -1 && errno != EADDRNOTAVAIL && errno != ENXIO &&
	    errno != ENODEV)
		logger(ifp->ctx, LOG_ERR, "%s: %s: %m", ifp->name, __func__);

	if (!keeparp && (astate = arp_find(ifp, addr)) != NULL)
		arp_free(astate);

	state = IPV4_STATE(ifp);
	TAILQ_FOREACH(ap, &state->addrs, next) {
		if (ap->addr.s_addr == addr->s_addr &&
		    ap->net.s_addr == net->s_addr)
		{
			TAILQ_REMOVE(&state->addrs, ap, next);
			free(ap);
			break;
		}
	}

	/* Have to do this last incase the function arguments
	 * were these very pointers. */
	dstate = D_STATE(ifp);
	if (dstate &&
	    dstate->addr.s_addr == addr->s_addr &&
	    dstate->net.s_addr == net->s_addr)
	{
		dstate->added = 0;
		dstate->addr.s_addr = 0;
		dstate->net.s_addr = 0;
	}
	return r;
}

static int
delete_address(struct interface *ifp)
{
	int r;
	struct if_options *ifo;
	struct dhcp_state *state;

	state = D_STATE(ifp);
	ifo = ifp->options;
	if (ifo->options & DHCPCD_INFORM ||
	    (ifo->options & DHCPCD_STATIC && ifo->req_addr.s_addr == 0))
		return 0;
	r = ipv4_deladdr(ifp, &state->addr, &state->net, 0);
	return r;
}

struct ipv4_state *
ipv4_getstate(struct interface *ifp)
{
	struct ipv4_state *state;

	state = IPV4_STATE(ifp);
	if (state == NULL) {
	        ifp->if_data[IF_DATA_IPV4] = malloc(sizeof(*state));
		state = IPV4_STATE(ifp);
		if (state == NULL) {
			logger(ifp->ctx, LOG_ERR, "%s: %m", __func__);
			return NULL;
		}
		TAILQ_INIT(&state->addrs);
		TAILQ_INIT(&state->routes);
#ifdef BSD
		state->buffer_size = state->buffer_len = state->buffer_pos = 0;
		state->buffer = NULL;
#endif
	}
	return state;
}

struct ipv4_addr *
ipv4_addaddr(struct interface *ifp, const struct in_addr *addr,
    const struct in_addr *mask, const struct in_addr *bcast)
{
	struct ipv4_state *state;
	struct ipv4_addr *ia;

	if ((state = ipv4_getstate(ifp)) == NULL) {
		logger(ifp->ctx, LOG_ERR, "%s: ipv4_getstate: %m", __func__);
		return NULL;
	}
	if (ifp->options->options & DHCPCD_NOALIAS) {
		struct ipv4_addr *ian;

		TAILQ_FOREACH_SAFE(ia, &state->addrs, next, ian) {
			if (ia->addr.s_addr != addr->s_addr)
				ipv4_deladdr(ifp, &ia->addr, &ia->net, 0);
		}
	}

	if ((ia = malloc(sizeof(*ia))) == NULL) {
		logger(ifp->ctx, LOG_ERR, "%s: %m", __func__);
		return NULL;
	}

	logger(ifp->ctx, LOG_DEBUG, "%s: adding IP address %s/%d",
	    ifp->name, inet_ntoa(*addr), inet_ntocidr(*mask));
	if (if_addaddress(ifp, addr, mask, bcast) == -1) {
		if (errno != EEXIST)
			logger(ifp->ctx, LOG_ERR, "%s: if_addaddress: %m",
			    __func__);
		free(ia);
		return NULL;
	}

	ia->iface = ifp;
	ia->addr = *addr;
	ia->net = *mask;
#ifdef IN_IFF_TENTATIVE
	ia->addr_flags = IN_IFF_TENTATIVE;
#endif
	TAILQ_INSERT_TAIL(&state->addrs, ia, next);
	return ia;
}

static int
ipv4_daddaddr(struct interface *ifp, const struct dhcp_lease *lease)
{
	struct dhcp_state *state;

	if (ipv4_addaddr(ifp, &lease->addr, &lease->net, &lease->brd) == NULL)
		return -1;

	state = D_STATE(ifp);
	state->added = STATE_ADDED;

	state->addr.s_addr = lease->addr.s_addr;
	state->net.s_addr = lease->net.s_addr;

	return 0;
}

int
ipv4_preferanother(struct interface *ifp)
{
	struct dhcp_state *state = D_STATE(ifp), *nstate;
	struct interface *ifn;
	int preferred;

	if (state == NULL)
		return 0;

	preferred = 0;
	if (!state->added)
		goto out;

	TAILQ_FOREACH(ifn, ifp->ctx->ifaces, next) {
		if (ifn == ifp)
			break; /* We are already the most preferred */
		nstate = D_STATE(ifn);
		if (nstate && !nstate->added &&
		    nstate->lease.addr.s_addr == state->addr.s_addr)
		{
			preferred = 1;
			delete_address(ifp);
			if (ifn->options->options & DHCPCD_ARP)
				dhcp_bind(ifn);
			else {
				ipv4_daddaddr(ifn, &nstate->lease);
				nstate->added = STATE_ADDED;
			}
			break;
		}
	}

out:
	ipv4_buildroutes(ifp->ctx);
	return preferred;
}

void
ipv4_applyaddr(void *arg)
{
	struct interface *ifp = arg, *ifn;
	struct dhcp_state *state = D_STATE(ifp), *nstate;
	struct dhcp_message *dhcp;
	struct dhcp_lease *lease;
	struct if_options *ifo = ifp->options;
	struct ipv4_addr *ap;
	int r;

	if (state == NULL)
		return;
	dhcp = state->new;
	lease = &state->lease;

	if_sortinterfaces(ifp->ctx);
	if (dhcp == NULL) {
		if ((ifo->options & (DHCPCD_EXITING | DHCPCD_PERSISTENT)) !=
		    (DHCPCD_EXITING | DHCPCD_PERSISTENT))
		{
			if (state->added && !ipv4_preferanother(ifp)) {
				delete_address(ifp);
				ipv4_buildroutes(ifp->ctx);
			}
			script_runreason(ifp, state->reason);
		} else
			ipv4_buildroutes(ifp->ctx);
		return;
	}

	/* Ensure only one interface has the address */
	r = 0;
	TAILQ_FOREACH(ifn, ifp->ctx->ifaces, next) {
		if (ifn == ifp) {
			r = 1; /* past ourselves */
			continue;
		}
		nstate = D_STATE(ifn);
		if (nstate && nstate->added &&
		    nstate->addr.s_addr == lease->addr.s_addr)
		{
			if (r == 0) {
				logger(ifp->ctx, LOG_INFO,
				    "%s: preferring %s on %s",
				    ifp->name,
				    inet_ntoa(lease->addr),
				    ifn->name);
				return;
			}
			logger(ifp->ctx, LOG_INFO, "%s: preferring %s on %s",
			    ifn->name,
			    inet_ntoa(lease->addr),
			    ifp->name);
			ipv4_deladdr(ifn, &nstate->addr, &nstate->net, 0);
			break;
		}
	}

	/* Does another interface already have the address from a prior boot? */
	if (ifn == NULL) {
		TAILQ_FOREACH(ifn, ifp->ctx->ifaces, next) {
			if (ifn == ifp)
				continue;
			ap = ipv4_iffindaddr(ifn, &lease->addr, NULL);
			if (ap)
				ipv4_deladdr(ifn, &ap->addr, &ap->net, 0);
		}
	}

	/* If the netmask is different, delete the addresss */
	ap = ipv4_iffindaddr(ifp, &lease->addr, NULL);
	if (ap && ap->net.s_addr != lease->net.s_addr)
		ipv4_deladdr(ifp, &ap->addr, &ap->net, 0);

	if (ipv4_iffindaddr(ifp, &lease->addr, &lease->net))
		logger(ifp->ctx, LOG_DEBUG,
		    "%s: IP address %s/%d already exists",
		    ifp->name, inet_ntoa(lease->addr),
		    inet_ntocidr(lease->net));
	else {
		r = ipv4_daddaddr(ifp, lease);
		if (r == -1 && errno != EEXIST)
			return;
	}

#ifdef IN_IFF_NOTUSEABLE
	ap = ipv4_iffindaddr(ifp, &lease->addr, NULL);
	if (ap == NULL) {
		logger(ifp->ctx, LOG_ERR, "%s: added address vanished",
		    ifp->name);
		return;
	} else if (ap->addr_flags & IN_IFF_NOTUSEABLE)
		return;
#endif

	/* Delete the old address if different */
	if (state->addr.s_addr != lease->addr.s_addr &&
	    state->addr.s_addr != 0 &&
	    ipv4_iffindaddr(ifp, &lease->addr, NULL))
		delete_address(ifp);

	state->added = STATE_ADDED;
	state->addr.s_addr = lease->addr.s_addr;
	state->net.s_addr = lease->net.s_addr;

	/* Find any freshly added routes, such as the subnet route.
	 * We do this because we cannot rely on recieving the kernel
	 * notification right now via our link socket. */
	if_initrt(ifp);
	ipv4_buildroutes(ifp->ctx);
	script_runreason(ifp, state->reason);

	dhcpcd_daemonise(ifp->ctx);
}

void
ipv4_handleifa(struct dhcpcd_ctx *ctx,
    int cmd, struct if_head *ifs, const char *ifname,
    const struct in_addr *addr, const struct in_addr *net,
    const struct in_addr *dst, int flags)
{
	struct interface *ifp;
	struct ipv4_state *state;
	struct ipv4_addr *ap;

	if (ifs == NULL)
		ifs = ctx->ifaces;
	if (ifs == NULL) {
		errno = ESRCH;
		return;
	}
	if (addr->s_addr == INADDR_ANY) {
		errno = EINVAL;
		return;
	}
	if ((ifp = if_find(ifs, ifname)) == NULL)
		return;
	if ((state = ipv4_getstate(ifp)) == NULL) {
		errno = ENOENT;
		return;
	}

	ap = ipv4_iffindaddr(ifp, addr, net);
	if (cmd == RTM_NEWADDR) {
		if (ap == NULL) {
			if ((ap = malloc(sizeof(*ap))) == NULL) {
				logger(ifp->ctx, LOG_ERR, "%s: %m", __func__);
				return;
			}
			ap->iface = ifp;
			ap->addr = *addr;
			ap->net = *net;
			if (dst)
				ap->dst.s_addr = dst->s_addr;
			else
				ap->dst.s_addr = INADDR_ANY;
			TAILQ_INSERT_TAIL(&state->addrs, ap, next);
		}
		ap->addr_flags = flags;
	} else if (cmd == RTM_DELADDR) {
		if (ap) {
			TAILQ_REMOVE(&state->addrs, ap, next);
			free(ap);
		}
	}

	dhcp_handleifa(cmd, ifp, addr, net, dst, flags);
	arp_handleifa(cmd, ifp, addr, flags);
}

void
ipv4_free(struct interface *ifp)
{
	struct ipv4_state *state;
	struct ipv4_addr *addr;

	if (ifp) {
		state = IPV4_STATE(ifp);
		if (state) {
		        while ((addr = TAILQ_FIRST(&state->addrs))) {
				TAILQ_REMOVE(&state->addrs, addr, next);
				free(addr);
			}
			ipv4_freerts(&state->routes);
#ifdef BSD
			free(state->buffer);
#endif
			free(state);
		}
	}
}

void
ipv4_ctxfree(struct dhcpcd_ctx *ctx)
{

	ipv4_freeroutes(ctx->ipv4_routes);
	ipv4_freeroutes(ctx->ipv4_kroutes);
}
