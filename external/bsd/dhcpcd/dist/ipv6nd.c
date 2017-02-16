#include <sys/cdefs.h>
 __RCSID("$NetBSD: ipv6nd.c,v 1.26 2015/08/21 10:39:00 roy Exp $");

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

#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ELOOP_QUEUE 3
#include "common.h"
#include "dhcpcd.h"
#include "dhcp6.h"
#include "eloop.h"
#include "if.h"
#include "ipv6.h"
#include "ipv6nd.h"
#include "script.h"

/* Debugging Router Solicitations is a lot of spam, so disable it */
//#define DEBUG_RS

#ifndef ND_OPT_RDNSS
#define ND_OPT_RDNSS			25
struct nd_opt_rdnss {           /* RDNSS option RFC 6106 */
	uint8_t		nd_opt_rdnss_type;
	uint8_t		nd_opt_rdnss_len;
	uint16_t	nd_opt_rdnss_reserved;
	uint32_t	nd_opt_rdnss_lifetime;
        /* followed by list of IP prefixes */
} __packed;
#endif

#ifndef ND_OPT_DNSSL
#define ND_OPT_DNSSL			31
struct nd_opt_dnssl {		/* DNSSL option RFC 6106 */
	uint8_t		nd_opt_dnssl_type;
	uint8_t		nd_opt_dnssl_len;
	uint16_t	nd_opt_dnssl_reserved;
	uint32_t	nd_opt_dnssl_lifetime;
	/* followed by list of DNS servers */
} __packed;
#endif

/* Impossible options, so we can easily add extras */
#define _ND_OPT_PREFIX_ADDR	255 + 1

/* Minimal IPv6 MTU */
#ifndef IPV6_MMTU
#define IPV6_MMTU 1280
#endif

#ifndef ND_RA_FLAG_RTPREF_HIGH
#define ND_RA_FLAG_RTPREF_MASK		0x18
#define ND_RA_FLAG_RTPREF_HIGH		0x08
#define ND_RA_FLAG_RTPREF_MEDIUM	0x00
#define ND_RA_FLAG_RTPREF_LOW		0x18
#define ND_RA_FLAG_RTPREF_RSV		0x10
#endif

/* RTPREF_MEDIUM has to be 0! */
#define RTPREF_HIGH	1
#define RTPREF_MEDIUM	0
#define RTPREF_LOW	(-1)
#define RTPREF_RESERVED	(-2)
#define RTPREF_INVALID	(-3)	/* internal */

#define MIN_RANDOM_FACTOR	500				/* millisecs */
#define MAX_RANDOM_FACTOR	1500				/* millisecs */
#define MIN_RANDOM_FACTOR_U	MIN_RANDOM_FACTOR * 1000	/* usecs */
#define MAX_RANDOM_FACTOR_U	MAX_RANDOM_FACTOR * 1000	/* usecs */

#if BYTE_ORDER == BIG_ENDIAN
#define IPV6_ADDR_INT32_ONE     1
#define IPV6_ADDR_INT16_MLL     0xff02
#elif BYTE_ORDER == LITTLE_ENDIAN
#define IPV6_ADDR_INT32_ONE     0x01000000
#define IPV6_ADDR_INT16_MLL     0x02ff
#endif

/* Debugging Neighbor Solicitations is a lot of spam, so disable it */
//#define DEBUG_NS
//

static void ipv6nd_handledata(void *);

/*
 * Android ships buggy ICMP6 filter headers.
 * Supply our own until they fix their shit.
 * References:
 *     https://android-review.googlesource.com/#/c/58438/
 *     http://code.google.com/p/android/issues/original?id=32621&seq=24
 */
#ifdef __ANDROID__
#undef ICMP6_FILTER_WILLPASS
#undef ICMP6_FILTER_WILLBLOCK
#undef ICMP6_FILTER_SETPASS
#undef ICMP6_FILTER_SETBLOCK
#undef ICMP6_FILTER_SETPASSALL
#undef ICMP6_FILTER_SETBLOCKALL
#define ICMP6_FILTER_WILLPASS(type, filterp) \
	((((filterp)->icmp6_filt[(type) >> 5]) & (1 << ((type) & 31))) == 0)
#define ICMP6_FILTER_WILLBLOCK(type, filterp) \
	((((filterp)->icmp6_filt[(type) >> 5]) & (1 << ((type) & 31))) != 0)
#define ICMP6_FILTER_SETPASS(type, filterp) \
	((((filterp)->icmp6_filt[(type) >> 5]) &= ~(1 << ((type) & 31))))
#define ICMP6_FILTER_SETBLOCK(type, filterp) \
	((((filterp)->icmp6_filt[(type) >> 5]) |=  (1 << ((type) & 31))))
#define ICMP6_FILTER_SETPASSALL(filterp) \
	memset(filterp, 0, sizeof(struct icmp6_filter));
#define ICMP6_FILTER_SETBLOCKALL(filterp) \
	memset(filterp, 0xff, sizeof(struct icmp6_filter));
#endif

/* Support older systems with different defines */
#if !defined(IPV6_RECVHOPLIMIT) && defined(IPV6_HOPLIMIT)
#define IPV6_RECVHOPLIMIT IPV6_HOPLIMIT
#endif
#if !defined(IPV6_RECVPKTINFO) && defined(IPV6_PKTINFO)
#define IPV6_RECVPKTINFO IPV6_PKTINFO
#endif

void
ipv6nd_printoptions(const struct dhcpcd_ctx *ctx,
    const struct dhcp_opt *opts, size_t opts_len)
{
	size_t i, j;
	const struct dhcp_opt *opt, *opt2;
	int cols;

	for (i = 0, opt = ctx->nd_opts;
	    i < ctx->nd_opts_len; i++, opt++)
	{
		for (j = 0, opt2 = opts; j < opts_len; j++, opt2++)
			if (opt2->option == opt->option)
				break;
		if (j == opts_len) {
			cols = printf("%03d %s", opt->option, opt->var);
			dhcp_print_option_encoding(opt, cols);
		}
	}
	for (i = 0, opt = opts; i < opts_len; i++, opt++) {
		cols = printf("%03d %s", opt->option, opt->var);
		dhcp_print_option_encoding(opt, cols);
	}
}

static int
ipv6nd_open(struct dhcpcd_ctx *dctx)
{
	struct ipv6_ctx *ctx;
	int on;
	struct icmp6_filter filt;

	ctx = dctx->ipv6;
	if (ctx->nd_fd != -1)
		return ctx->nd_fd;
	ctx->nd_fd = xsocket(PF_INET6, SOCK_RAW, IPPROTO_ICMPV6,
	    O_NONBLOCK|O_CLOEXEC);
	if (ctx->nd_fd == -1)
		return -1;

	/* RFC4861 4.1 */
	on = 255;
	if (setsockopt(ctx->nd_fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
	    &on, sizeof(on)) == -1)
		goto eexit;

	on = 1;
	if (setsockopt(ctx->nd_fd, IPPROTO_IPV6, IPV6_RECVPKTINFO,
	    &on, sizeof(on)) == -1)
		goto eexit;

	on = 1;
	if (setsockopt(ctx->nd_fd, IPPROTO_IPV6, IPV6_RECVHOPLIMIT,
	    &on, sizeof(on)) == -1)
		goto eexit;

	ICMP6_FILTER_SETBLOCKALL(&filt);
	ICMP6_FILTER_SETPASS(ND_NEIGHBOR_ADVERT, &filt);
	ICMP6_FILTER_SETPASS(ND_ROUTER_ADVERT, &filt);
	if (setsockopt(ctx->nd_fd, IPPROTO_ICMPV6, ICMP6_FILTER,
	    &filt, sizeof(filt)) == -1)
		goto eexit;

	eloop_event_add(dctx->eloop, ctx->nd_fd,
	    ipv6nd_handledata, dctx, NULL, NULL);
	return ctx->nd_fd;

eexit:
	if (ctx->nd_fd != -1) {
		eloop_event_delete(dctx->eloop, ctx->nd_fd);
		close(ctx->nd_fd);
		ctx->nd_fd = -1;
	}
	return -1;
}

static int
ipv6nd_makersprobe(struct interface *ifp)
{
	struct rs_state *state;
	struct nd_router_solicit *rs;
	struct nd_opt_hdr *nd;

	state = RS_STATE(ifp);
	free(state->rs);
	state->rslen = sizeof(*rs) + (size_t)ROUNDUP8(ifp->hwlen + 2);
	state->rs = calloc(1, state->rslen);
	if (state->rs == NULL)
		return -1;
	rs = (struct nd_router_solicit *)(void *)state->rs;
	rs->nd_rs_type = ND_ROUTER_SOLICIT;
	rs->nd_rs_code = 0;
	rs->nd_rs_cksum = 0;
	rs->nd_rs_reserved = 0;
	nd = (struct nd_opt_hdr *)(state->rs + sizeof(*rs));
	nd->nd_opt_type = ND_OPT_SOURCE_LINKADDR;
	nd->nd_opt_len = (uint8_t)((ROUNDUP8(ifp->hwlen + 2)) >> 3);
	memcpy(nd + 1, ifp->hwaddr, ifp->hwlen);
	return 0;
}

static void
ipv6nd_sendrsprobe(void *arg)
{
	struct interface *ifp = arg;
	struct ipv6_ctx *ctx;
	struct rs_state *state;
	struct sockaddr_in6 dst;
	struct cmsghdr *cm;
	struct in6_pktinfo pi;

	if (ipv6_linklocal(ifp) == NULL) {
		logger(ifp->ctx, LOG_DEBUG,
		    "%s: delaying Router Solicitation for LL address",
		    ifp->name);
		ipv6_addlinklocalcallback(ifp, ipv6nd_sendrsprobe, ifp);
		return;
	}

	memset(&dst, 0, sizeof(dst));
	dst.sin6_family = AF_INET6;
#ifdef SIN6_LEN
	dst.sin6_len = sizeof(dst);
#endif
	dst.sin6_scope_id = ifp->index;
	if (inet_pton(AF_INET6, ALLROUTERS, &dst.sin6_addr) != 1) {
		logger(ifp->ctx, LOG_ERR, "%s: %m", __func__);
		return;
	}

	state = RS_STATE(ifp);
	ctx = ifp->ctx->ipv6;
	ctx->sndhdr.msg_name = (void *)&dst;
	ctx->sndhdr.msg_iov[0].iov_base = state->rs;
	ctx->sndhdr.msg_iov[0].iov_len = state->rslen;

	/* Set the outbound interface */
	cm = CMSG_FIRSTHDR(&ctx->sndhdr);
	if (cm == NULL) /* unlikely */
		return;
	cm->cmsg_level = IPPROTO_IPV6;
	cm->cmsg_type = IPV6_PKTINFO;
	cm->cmsg_len = CMSG_LEN(sizeof(pi));
	memset(&pi, 0, sizeof(pi));
	pi.ipi6_ifindex = ifp->index;
	memcpy(CMSG_DATA(cm), &pi, sizeof(pi));

	logger(ifp->ctx, LOG_DEBUG,
	    "%s: sending Router Solicitation", ifp->name);
	if (sendmsg(ctx->nd_fd, &ctx->sndhdr, 0) == -1) {
		logger(ifp->ctx, LOG_ERR,
		    "%s: %s: sendmsg: %m", ifp->name, __func__);
		ipv6nd_drop(ifp);
		ifp->options->options &= ~(DHCPCD_IPV6 | DHCPCD_IPV6RS);
		return;
	}

	if (state->rsprobes++ < MAX_RTR_SOLICITATIONS)
		eloop_timeout_add_sec(ifp->ctx->eloop,
		    RTR_SOLICITATION_INTERVAL, ipv6nd_sendrsprobe, ifp);
	else {
		logger(ifp->ctx, LOG_WARNING,
		    "%s: no IPv6 Routers available", ifp->name);
		ipv6nd_drop(ifp);
		dhcp6_drop(ifp, "EXPIRE6");
	}
}

void
ipv6nd_expire(struct interface *ifp, uint32_t seconds)
{
	struct ra *rap;
	struct timespec now;

	if (ifp->ctx->ipv6 == NULL)
		return;

	clock_gettime(CLOCK_MONOTONIC, &now);

	TAILQ_FOREACH(rap, ifp->ctx->ipv6->ra_routers, next) {
		if (rap->iface == ifp) {
			rap->acquired = now;
			rap->expired = seconds ? 0 : 1;
			if (seconds) {
				struct ipv6_addr *ap;

				rap->lifetime = seconds;
				TAILQ_FOREACH(ap, &rap->addrs, next) {
					if (ap->prefix_vltime) {
						ap->prefix_vltime = seconds;
						ap->prefix_pltime = seconds / 2;
					}
				}
				ipv6_addaddrs(&rap->addrs);
			}
		}
	}
	if (seconds)
		ipv6nd_expirera(ifp);
	else
		ipv6_buildroutes(ifp->ctx);
}

static void
ipv6nd_reachable(struct ra *rap, int flags)
{

	if (flags & IPV6ND_REACHABLE) {
		if (rap->lifetime && rap->expired) {
			logger(rap->iface->ctx, LOG_INFO,
			    "%s: %s is reachable again",
			    rap->iface->name, rap->sfrom);
			rap->expired = 0;
			ipv6_buildroutes(rap->iface->ctx);
			/* XXX Not really an RA */
			script_runreason(rap->iface, "ROUTERADVERT");
		}
	} else {
		if (rap->lifetime && !rap->expired) {
			logger(rap->iface->ctx, LOG_WARNING,
			    "%s: %s is unreachable, expiring it",
			    rap->iface->name, rap->sfrom);
			rap->expired = 1;
			ipv6_buildroutes(rap->iface->ctx);
			/* XXX Not really an RA */
			script_runreason(rap->iface, "ROUTERADVERT");
		}
	}
}

void
ipv6nd_neighbour(struct dhcpcd_ctx *ctx, struct in6_addr *addr, int flags)
{
	struct ra *rap;

	if (ctx->ipv6) {
	        TAILQ_FOREACH(rap, ctx->ipv6->ra_routers, next) {
			if (IN6_ARE_ADDR_EQUAL(&rap->from, addr)) {
				ipv6nd_reachable(rap, flags);
				break;
			}
		}
	}
}

const struct ipv6_addr *
ipv6nd_iffindaddr(const struct interface *ifp, const struct in6_addr *addr,
    short flags)
{
	struct ra *rap;
	struct ipv6_addr *ap;

	if (ifp->ctx->ipv6 == NULL)
		return NULL;

	TAILQ_FOREACH(rap, ifp->ctx->ipv6->ra_routers, next) {
		if (rap->iface != ifp)
			continue;
		TAILQ_FOREACH(ap, &rap->addrs, next) {
			if (ipv6_findaddrmatch(ap, addr, flags))
				return ap;
		}
	}
	return NULL;
}

struct ipv6_addr *
ipv6nd_findaddr(struct dhcpcd_ctx *ctx, const struct in6_addr *addr,
    short flags)
{
	struct ra *rap;
	struct ipv6_addr *ap;

	if (ctx->ipv6 == NULL)
		return NULL;

	TAILQ_FOREACH(rap, ctx->ipv6->ra_routers, next) {
		TAILQ_FOREACH(ap, &rap->addrs, next) {
			if (ipv6_findaddrmatch(ap, addr, flags))
				return ap;
		}
	}
	return NULL;
}

static void
ipv6nd_removefreedrop_ra(struct ra *rap, int remove_ra, int drop_ra)
{

	eloop_timeout_delete(rap->iface->ctx->eloop, NULL, rap->iface);
	eloop_timeout_delete(rap->iface->ctx->eloop, NULL, rap);
	if (remove_ra && !drop_ra)
		TAILQ_REMOVE(rap->iface->ctx->ipv6->ra_routers, rap, next);
	ipv6_freedrop_addrs(&rap->addrs, drop_ra, NULL);
	free(rap->data);
	free(rap);
}

void
ipv6nd_freedrop_ra(struct ra *rap, int drop)
{

	ipv6nd_removefreedrop_ra(rap, 1, drop);
}

ssize_t
ipv6nd_free(struct interface *ifp)
{
	struct rs_state *state;
	struct ra *rap, *ran;
	struct dhcpcd_ctx *ctx;
	ssize_t n;

	state = RS_STATE(ifp);
	if (state == NULL)
		return 0;

	free(state->rs);
	free(state);
	ifp->if_data[IF_DATA_IPV6ND] = NULL;
	n = 0;
	TAILQ_FOREACH_SAFE(rap, ifp->ctx->ipv6->ra_routers, next, ran) {
		if (rap->iface == ifp) {
			ipv6nd_free_ra(rap);
			n++;
		}
	}

	/* If we don't have any more IPv6 enabled interfaces,
	 * close the global socket and release resources */
	ctx = ifp->ctx;
	TAILQ_FOREACH(ifp, ctx->ifaces, next) {
		if (RS_STATE(ifp))
			break;
	}
	if (ifp == NULL) {
		if (ctx->ipv6->nd_fd != -1) {
			eloop_event_delete(ctx->eloop, ctx->ipv6->nd_fd);
			close(ctx->ipv6->nd_fd);
			ctx->ipv6->nd_fd = -1;
		}
	}

	return n;
}

static int
rtpref(struct ra *rap)
{

	switch (rap->flags & ND_RA_FLAG_RTPREF_MASK) {
	case ND_RA_FLAG_RTPREF_HIGH:
		return (RTPREF_HIGH);
	case ND_RA_FLAG_RTPREF_MEDIUM:
	case ND_RA_FLAG_RTPREF_RSV:
		return (RTPREF_MEDIUM);
	case ND_RA_FLAG_RTPREF_LOW:
		return (RTPREF_LOW);
	default:
		logger(rap->iface->ctx, LOG_ERR,
		    "rtpref: impossible RA flag %x", rap->flags);
		return (RTPREF_INVALID);
	}
	/* NOTREACHED */
}

static void
add_router(struct ipv6_ctx *ctx, struct ra *router)
{
	struct ra *rap;

	TAILQ_FOREACH(rap, ctx->ra_routers, next) {
		if (router->iface->metric < rap->iface->metric ||
		    (router->iface->metric == rap->iface->metric &&
		    rtpref(router) > rtpref(rap)))
		{
			TAILQ_INSERT_BEFORE(rap, router, next);
			return;
		}
	}
	TAILQ_INSERT_TAIL(ctx->ra_routers, router, next);
}

static int
ipv6nd_scriptrun(struct ra *rap)
{
	int hasdns, hasaddress, pid;
	struct ipv6_addr *ap;

	hasaddress = 0;
	/* If all addresses have completed DAD run the script */
	TAILQ_FOREACH(ap, &rap->addrs, next) {
		if ((ap->flags & (IPV6_AF_AUTOCONF | IPV6_AF_ADDED)) ==
		    (IPV6_AF_AUTOCONF | IPV6_AF_ADDED))
		{
			hasaddress = 1;
			if (!(ap->flags & IPV6_AF_DADCOMPLETED) &&
			    ipv6_iffindaddr(ap->iface, &ap->addr))
				ap->flags |= IPV6_AF_DADCOMPLETED;
			if ((ap->flags & IPV6_AF_DADCOMPLETED) == 0) {
				logger(ap->iface->ctx, LOG_DEBUG,
				    "%s: waiting for Router Advertisement"
				    " DAD to complete",
				    rap->iface->name);
				return 0;
			}
		}
	}

	/* If we don't require RDNSS then set hasdns = 1 so we fork */
	if (!(rap->iface->options->options & DHCPCD_IPV6RA_REQRDNSS))
		hasdns = 1;
	else {
		hasdns = rap->hasdns;
	}

	script_runreason(rap->iface, "ROUTERADVERT");
	pid = 0;
	if (hasdns && (hasaddress ||
	    !(rap->flags & (ND_RA_FLAG_MANAGED | ND_RA_FLAG_OTHER))))
		pid = dhcpcd_daemonise(rap->iface->ctx);
#if 0
	else if (options & DHCPCD_DAEMONISE &&
	    !(options & DHCPCD_DAEMONISED) && new_data)
		logger(rap->iface->ctx, LOG_WARNING,
		    "%s: did not fork due to an absent"
		    " RDNSS option in the RA",
		    ifp->name);
}
#endif
	return pid;
}

static void
ipv6nd_addaddr(void *arg)
{
	struct ipv6_addr *ap = arg;

	ipv6_addaddr(ap, NULL);
}

int
ipv6nd_dadcompleted(const struct interface *ifp)
{
	const struct ra *rap;
	const struct ipv6_addr *ap;

	TAILQ_FOREACH(rap, ifp->ctx->ipv6->ra_routers, next) {
		if (rap->iface != ifp)
			continue;
		TAILQ_FOREACH(ap, &rap->addrs, next) {
			if (ap->flags & IPV6_AF_AUTOCONF &&
			    ap->flags & IPV6_AF_ADDED &&
			    !(ap->flags & IPV6_AF_DADCOMPLETED))
				return 0;
		}
	}
	return 1;
}

static void
ipv6nd_dadcallback(void *arg)
{
	struct ipv6_addr *ap = arg, *rapap;
	struct interface *ifp;
	struct ra *rap;
	int wascompleted, found;
	struct timespec tv;
	char buf[INET6_ADDRSTRLEN];
	const char *p;
	int dadcounter;

	ifp = ap->iface;
	wascompleted = (ap->flags & IPV6_AF_DADCOMPLETED);
	ap->flags |= IPV6_AF_DADCOMPLETED;
	if (ap->flags & IPV6_AF_DUPLICATED) {
		ap->dadcounter++;
		logger(ifp->ctx, LOG_WARNING, "%s: DAD detected %s",
		    ifp->name, ap->saddr);

		/* Try and make another stable private address.
		 * Because ap->dadcounter is always increamented,
		 * a different address is generated. */
		/* XXX Cache DAD counter per prefix/id/ssid? */
		if (ifp->options->options & DHCPCD_SLAACPRIVATE) {
			if (ap->dadcounter >= IDGEN_RETRIES) {
				logger(ifp->ctx, LOG_ERR,
				    "%s: unable to obtain a"
				    " stable private address",
				    ifp->name);
				goto try_script;
			}
			logger(ifp->ctx, LOG_INFO, "%s: deleting address %s",
				ifp->name, ap->saddr);
			if (if_deladdress6(ap) == -1 &&
			    errno != EADDRNOTAVAIL && errno != ENXIO)
				logger(ifp->ctx, LOG_ERR, "if_deladdress6: %m");
			dadcounter = ap->dadcounter;
			if (ipv6_makestableprivate(&ap->addr,
			    &ap->prefix, ap->prefix_len,
			    ifp, &dadcounter) == -1)
			{
				logger(ifp->ctx, LOG_ERR,
				    "%s: ipv6_makestableprivate: %m",
				    ifp->name);
				return;
			}
			ap->dadcounter = dadcounter;
			ap->flags &= ~(IPV6_AF_ADDED | IPV6_AF_DADCOMPLETED);
			ap->flags |= IPV6_AF_NEW;
			p = inet_ntop(AF_INET6, &ap->addr, buf, sizeof(buf));
			if (p)
				snprintf(ap->saddr,
				    sizeof(ap->saddr),
				    "%s/%d",
				    p, ap->prefix_len);
			else
				ap->saddr[0] = '\0';
			tv.tv_sec = 0;
			tv.tv_nsec = (suseconds_t)
			    arc4random_uniform(IDGEN_DELAY * NSEC_PER_SEC);
			timespecnorm(&tv);
			eloop_timeout_add_tv(ifp->ctx->eloop, &tv,
			    ipv6nd_addaddr, ap);
			return;
		}
	}

try_script:
	if (!wascompleted) {
		TAILQ_FOREACH(rap, ifp->ctx->ipv6->ra_routers, next) {
			if (rap->iface != ifp)
				continue;
			wascompleted = 1;
			found = 0;
			TAILQ_FOREACH(rapap, &rap->addrs, next) {
				if (rapap->flags & IPV6_AF_AUTOCONF &&
				    rapap->flags & IPV6_AF_ADDED &&
				    (rapap->flags & IPV6_AF_DADCOMPLETED) == 0)
				{
					wascompleted = 0;
					break;
				}
				if (rapap == ap)
					found = 1;
			}

			if (wascompleted && found) {
				logger(rap->iface->ctx, LOG_DEBUG,
				    "%s: Router Advertisement DAD completed",
				    rap->iface->name);
				if (ipv6nd_scriptrun(rap))
					return;
			}
		}
	}
}

static int
ipv6nd_has_public_addr(const struct interface *ifp)
{
	const struct ra *rap;
	const struct ipv6_addr *ia;

	TAILQ_FOREACH(rap, ifp->ctx->ipv6->ra_routers, next) {
		if (rap->iface == ifp) {
			TAILQ_FOREACH(ia, &rap->addrs, next) {
				if (ia->flags & IPV6_AF_AUTOCONF &&
				    ipv6_publicaddr(ia))
					return 1;
			}
		}
	}
	return 0;
}

static void
ipv6nd_handlera(struct dhcpcd_ctx *dctx, struct interface *ifp,
    struct icmp6_hdr *icp, size_t len, int hoplimit)
{
	struct ipv6_ctx *ctx = dctx->ipv6;
	size_t i, olen;
	struct nd_router_advert *nd_ra;
	struct nd_opt_prefix_info *pi;
	struct nd_opt_mtu *mtu;
	struct nd_opt_rdnss *rdnss;
	uint32_t mtuv;
	uint8_t *p;
	char buf[INET6_ADDRSTRLEN];
	const char *cbp;
	struct ra *rap;
	struct nd_opt_hdr *ndo;
	struct ipv6_addr *ap;
	struct dhcp_opt *dho;
	uint8_t new_rap, new_data;
#ifdef IPV6_MANAGETEMPADDR
	uint8_t new_ap;
#endif

	if (ifp == NULL) {
#ifdef DEBUG_RS
		logger(dctx, LOG_DEBUG,
		    "RA for unexpected interface from %s", ctx->sfrom);
#endif
		return;
	}

	if (len < sizeof(struct nd_router_advert)) {
		logger(dctx, LOG_ERR,
		    "IPv6 RA packet too short from %s", ctx->sfrom);
		return;
	}

	/* RFC 4861 7.1.2 */
	if (hoplimit != 255) {
		logger(dctx, LOG_ERR,
		    "invalid hoplimit(%d) in RA from %s", hoplimit, ctx->sfrom);
		return;
	}

	if (!IN6_IS_ADDR_LINKLOCAL(&ctx->from.sin6_addr)) {
		logger(dctx, LOG_ERR,
		    "RA from non local address %s", ctx->sfrom);
		return;
	}

	if (!(ifp->options->options & DHCPCD_IPV6RS)) {
#ifdef DEBUG_RS
		logger(ifp->ctx, LOG_DEBUG, "%s: unexpected RA from %s",
		    ifp->name, ctx->sfrom);
#endif
		return;
	}

	/* We could receive a RA before we sent a RS*/
	if (ipv6_linklocal(ifp) == NULL) {
#ifdef DEBUG_RS
		logger(ifp->ctx, LOG_DEBUG,
		    "%s: received RA from %s (no link-local)",
		    ifp->name, ctx->sfrom);
#endif
		return;
	}

	if (ipv6_iffindaddr(ifp, &ctx->from.sin6_addr)) {
		logger(ifp->ctx, LOG_DEBUG,
		    "%s: ignoring RA from ourself %s", ifp->name, ctx->sfrom);
		return;
	}

	TAILQ_FOREACH(rap, ctx->ra_routers, next) {
		if (ifp == rap->iface &&
		    IN6_ARE_ADDR_EQUAL(&rap->from, &ctx->from.sin6_addr))
			break;
	}

	nd_ra = (struct nd_router_advert *)icp;

	/* We don't want to spam the log with the fact we got an RA every
	 * 30 seconds or so, so only spam the log if it's different. */
	if (rap == NULL || (rap->data_len != len ||
	     memcmp(rap->data, (unsigned char *)icp, rap->data_len) != 0))
	{
		if (rap) {
			free(rap->data);
			rap->data_len = 0;
			rap->no_public_warned = 0;
		}
		new_data = 1;
	} else
		new_data = 0;
	if (new_data || ifp->options->options & DHCPCD_DEBUG)
		logger(ifp->ctx, LOG_INFO, "%s: Router Advertisement from %s",
		    ifp->name, ctx->sfrom);

	if (rap == NULL) {
		rap = calloc(1, sizeof(*rap));
		if (rap == NULL) {
			logger(ifp->ctx, LOG_ERR, "%s: %m", __func__);
			return;
		}
		rap->iface = ifp;
		rap->from = ctx->from.sin6_addr;
		strlcpy(rap->sfrom, ctx->sfrom, sizeof(rap->sfrom));
		TAILQ_INIT(&rap->addrs);
		new_rap = 1;
	} else
		new_rap = 0;
	if (rap->data_len == 0) {
		rap->data = malloc(len);
		if (rap->data == NULL) {
			logger(ifp->ctx, LOG_ERR, "%s: %m", __func__);
			if (new_rap)
				free(rap);
			return;
		}
		memcpy(rap->data, icp, len);
		rap->data_len = len;
	}

	clock_gettime(CLOCK_MONOTONIC, &rap->acquired);
	rap->flags = nd_ra->nd_ra_flags_reserved;
	rap->lifetime = ntohs(nd_ra->nd_ra_router_lifetime);
	if (nd_ra->nd_ra_reachable) {
		rap->reachable = ntohl(nd_ra->nd_ra_reachable);
		if (rap->reachable > MAX_REACHABLE_TIME)
			rap->reachable = 0;
	}
	if (nd_ra->nd_ra_retransmit)
		rap->retrans = ntohl(nd_ra->nd_ra_retransmit);
	if (rap->lifetime)
		rap->expired = 0;
	rap->hasdns = 0;

	ipv6_settempstale(ifp);
	TAILQ_FOREACH(ap, &rap->addrs, next) {
		ap->flags |= IPV6_AF_STALE;
	}

	len -= sizeof(struct nd_router_advert);
	p = ((uint8_t *)icp) + sizeof(struct nd_router_advert);
	for (; len > 0; p += olen, len -= olen) {
		if (len < sizeof(struct nd_opt_hdr)) {
			logger(ifp->ctx, LOG_ERR,
			    "%s: short option", ifp->name);
			break;
		}
		ndo = (struct nd_opt_hdr *)p;
		olen = (size_t)ndo->nd_opt_len * 8;
		if (olen == 0) {
			logger(ifp->ctx, LOG_ERR,
			    "%s: zero length option", ifp->name);
			break;
		}
		if (olen > len) {
			logger(ifp->ctx, LOG_ERR,
			    "%s: option length exceeds message", ifp->name);
			break;
		}

		if (has_option_mask(ifp->options->rejectmasknd,
		    ndo->nd_opt_type))
		{
			for (i = 0, dho = dctx->nd_opts;
			    i < dctx->nd_opts_len;
			    i++, dho++)
			{
				if (dho->option == ndo->nd_opt_type)
					break;
			}
			if (dho != NULL)
				logger(ifp->ctx, LOG_WARNING,
				    "%s: reject RA (option %s) from %s",
				    ifp->name, dho->var, ctx->sfrom);
			else
				logger(ifp->ctx, LOG_WARNING,
				    "%s: reject RA (option %d) from %s",
				    ifp->name, ndo->nd_opt_type, ctx->sfrom);
			if (new_rap)
				ipv6nd_removefreedrop_ra(rap, 0, 0);
			else
				ipv6nd_free_ra(rap);
			return;
		}

		if (has_option_mask(ifp->options->nomasknd, ndo->nd_opt_type))
			continue;

		switch (ndo->nd_opt_type) {
		case ND_OPT_PREFIX_INFORMATION:
			pi = (struct nd_opt_prefix_info *)(void *)ndo;
			if (pi->nd_opt_pi_len != 4) {
				logger(ifp->ctx, new_data ? LOG_ERR : LOG_DEBUG,
				    "%s: invalid option len for prefix",
				    ifp->name);
				continue;
			}
			if (pi->nd_opt_pi_prefix_len > 128) {
				logger(ifp->ctx, new_data ? LOG_ERR : LOG_DEBUG,
				    "%s: invalid prefix len",
				    ifp->name);
				continue;
			}
			if (IN6_IS_ADDR_MULTICAST(&pi->nd_opt_pi_prefix) ||
			    IN6_IS_ADDR_LINKLOCAL(&pi->nd_opt_pi_prefix))
			{
				logger(ifp->ctx, new_data ? LOG_ERR : LOG_DEBUG,
				    "%s: invalid prefix in RA", ifp->name);
				continue;
			}
			if (ntohl(pi->nd_opt_pi_preferred_time) >
			    ntohl(pi->nd_opt_pi_valid_time))
			{
				logger(ifp->ctx, new_data ? LOG_ERR : LOG_DEBUG,
				    "%s: pltime > vltime", ifp->name);
				continue;
			}
			TAILQ_FOREACH(ap, &rap->addrs, next)
				if (ap->prefix_len ==pi->nd_opt_pi_prefix_len &&
				    IN6_ARE_ADDR_EQUAL(&ap->prefix,
				    &pi->nd_opt_pi_prefix))
					break;
			if (ap == NULL) {
				if (!(pi->nd_opt_pi_flags_reserved &
				    ND_OPT_PI_FLAG_AUTO) &&
				    !(pi->nd_opt_pi_flags_reserved &
				    ND_OPT_PI_FLAG_ONLINK))
					continue;
				ap = calloc(1, sizeof(*ap));
				if (ap == NULL)
					break;
				ap->iface = rap->iface;
				ap->flags = IPV6_AF_NEW;
				ap->prefix_len = pi->nd_opt_pi_prefix_len;
				ap->prefix = pi->nd_opt_pi_prefix;
				if (pi->nd_opt_pi_flags_reserved &
				    ND_OPT_PI_FLAG_AUTO &&
				    ap->iface->options->options &
				    DHCPCD_IPV6RA_AUTOCONF)
				{
					ap->flags |= IPV6_AF_AUTOCONF;
					ap->dadcounter =
					    ipv6_makeaddr(&ap->addr, ifp,
					    &ap->prefix,
					    pi->nd_opt_pi_prefix_len);
					if (ap->dadcounter == -1) {
						free(ap);
						break;
					}
					cbp = inet_ntop(AF_INET6,
					    &ap->addr,
					    buf, sizeof(buf));
					if (cbp)
						snprintf(ap->saddr,
						    sizeof(ap->saddr),
						    "%s/%d",
						    cbp, ap->prefix_len);
					else
						ap->saddr[0] = '\0';
				} else {
					memset(&ap->addr, 0, sizeof(ap->addr));
					ap->saddr[0] = '\0';
				}
				ap->dadcallback = ipv6nd_dadcallback;
				ap->created = ap->acquired = rap->acquired;
				TAILQ_INSERT_TAIL(&rap->addrs, ap, next);

#ifdef IPV6_MANAGETEMPADDR
				/* New address to dhcpcd RA handling.
				 * If the address already exists and a valid
				 * temporary address also exists then
				 * extend the existing one rather than
				 * create a new one */
				if (ipv6_iffindaddr(ifp, &ap->addr) &&
				    ipv6_settemptime(ap, 0))
					new_ap = 0;
				else
					new_ap = 1;
#endif
			} else {
#ifdef IPV6_MANAGETEMPADDR
				new_ap = 0;
#endif
				ap->flags &= ~IPV6_AF_STALE;
				ap->acquired = rap->acquired;
			}
			if (pi->nd_opt_pi_flags_reserved &
			    ND_OPT_PI_FLAG_ONLINK)
				ap->flags |= IPV6_AF_ONLINK;
			ap->prefix_vltime =
			    ntohl(pi->nd_opt_pi_valid_time);
			ap->prefix_pltime =
			    ntohl(pi->nd_opt_pi_preferred_time);
			ap->nsprobes = 0;

#ifdef IPV6_MANAGETEMPADDR
			/* RFC4941 Section 3.3.3 */
			if (ap->flags & IPV6_AF_AUTOCONF &&
			    ap->iface->options->options & DHCPCD_IPV6RA_OWN &&
			    ip6_use_tempaddr(ap->iface->name))
			{
				if (!new_ap) {
					if (ipv6_settemptime(ap, 1) == NULL)
						new_ap = 1;
				}
				if (new_ap && ap->prefix_pltime) {
					if (ipv6_createtempaddr(ap,
					    &ap->acquired) == NULL)
						logger(ap->iface->ctx, LOG_ERR,
						    "ipv6_createtempaddr: %m");
				}
			}
#endif
			break;

		case ND_OPT_MTU:
			mtu = (struct nd_opt_mtu *)(void *)p;
			mtuv = ntohl(mtu->nd_opt_mtu_mtu);
			if (mtuv < IPV6_MMTU) {
				logger(ifp->ctx, LOG_ERR, "%s: invalid MTU %d",
				    ifp->name, mtuv);
				break;
			}
			rap->mtu = mtuv;
			break;

		case ND_OPT_RDNSS:
			rdnss = (struct nd_opt_rdnss *)(void *)p;
			if (rdnss->nd_opt_rdnss_lifetime &&
			    rdnss->nd_opt_rdnss_len > 1)
				rap->hasdns = 1;

		default:
			continue;
		}
	}

	for (i = 0, dho = dctx->nd_opts;
	    i < dctx->nd_opts_len;
	    i++, dho++)
	{
		if (has_option_mask(ifp->options->requiremasknd,
		    dho->option))
		{
			logger(ifp->ctx, LOG_WARNING,
			    "%s: reject RA (no option %s) from %s",
			    ifp->name, dho->var, ctx->sfrom);
			if (new_rap)
				ipv6nd_removefreedrop_ra(rap, 0, 0);
			else
				ipv6nd_free_ra(rap);
			return;
		}
	}

	if (new_rap)
		add_router(ifp->ctx->ipv6, rap);

	if (!ipv6nd_has_public_addr(rap->iface) &&
	    !(rap->iface->options->options & DHCPCD_IPV6RA_ACCEPT_NOPUBLIC) &&
	    (!(rap->flags & ND_RA_FLAG_MANAGED) ||
	    !dhcp6_has_public_addr(rap->iface)))
	{
		logger(rap->iface->ctx,
		    rap->no_public_warned ? LOG_DEBUG : LOG_WARNING,
		    "%s: ignoring RA from %s"
		    " (no public prefix, no managed address)",
		    rap->iface->name, rap->sfrom);
		rap->no_public_warned = 1;
		goto handle_flag;
	}
	if (ifp->ctx->options & DHCPCD_TEST) {
		script_runreason(ifp, "TEST");
		goto handle_flag;
	}
	ipv6_addaddrs(&rap->addrs);
#ifdef IPV6_MANAGETEMPADDR
	ipv6_addtempaddrs(ifp, &rap->acquired);
#endif

	/* Find any freshly added routes, such as the subnet route.
	 * We do this because we cannot rely on recieving the kernel
	 * notification right now via our link socket. */
	if_initrt6(ifp);

	ipv6_buildroutes(ifp->ctx);
	if (ipv6nd_scriptrun(rap))
		return;

	eloop_timeout_delete(ifp->ctx->eloop, NULL, ifp);
	eloop_timeout_delete(ifp->ctx->eloop, NULL, rap); /* reachable timer */

handle_flag:
	if (!(ifp->options->options & DHCPCD_DHCP6))
		goto nodhcp6;
	if (rap->flags & ND_RA_FLAG_MANAGED) {
		if (new_data && dhcp6_start(ifp, DH6S_INIT) == -1)
			logger(ifp->ctx, LOG_ERR,
			    "dhcp6_start: %s: %m", ifp->name);
	} else if (rap->flags & ND_RA_FLAG_OTHER) {
		if (new_data && dhcp6_start(ifp, DH6S_INFORM) == -1)
			logger(ifp->ctx, LOG_ERR,
			    "dhcp6_start: %s: %m", ifp->name);
	} else {
		if (new_data)
			logger(ifp->ctx, LOG_DEBUG,
			    "%s: No DHCPv6 instruction in RA", ifp->name);
nodhcp6:
		if (ifp->ctx->options & DHCPCD_TEST) {
			eloop_exit(ifp->ctx->eloop, EXIT_SUCCESS);
			return;
		}
	}

	/* Expire should be called last as the rap object could be destroyed */
	ipv6nd_expirera(ifp);
}

/* Run RA's we ignored becuase they had no public addresses
 * This should only be called when DHCPv6 applies a public address */
void
ipv6nd_runignoredra(struct interface *ifp)
{
	struct ra *rap;

	TAILQ_FOREACH(rap, ifp->ctx->ipv6->ra_routers, next) {
		if (rap->iface == ifp &&
		    !rap->expired &&
		    rap->no_public_warned)
		{
			rap->no_public_warned = 0;
			logger(rap->iface->ctx, LOG_INFO,
			    "%s: applying ignored RA from %s",
			    rap->iface->name, rap->sfrom);
			if (ifp->ctx->options & DHCPCD_TEST) {
				script_runreason(ifp, "TEST");
				continue;
			}
			if (ipv6nd_scriptrun(rap))
				return;
			eloop_timeout_delete(ifp->ctx->eloop, NULL, ifp);
			eloop_timeout_delete(ifp->ctx->eloop, NULL, rap);
		}
	}
}

int
ipv6nd_hasra(const struct interface *ifp)
{
	const struct ra *rap;

	if (ifp->ctx->ipv6) {
		TAILQ_FOREACH(rap, ifp->ctx->ipv6->ra_routers, next)
			if (rap->iface == ifp && !rap->expired)
				return 1;
	}
	return 0;
}

int
ipv6nd_hasradhcp(const struct interface *ifp)
{
	const struct ra *rap;

	if (ifp->ctx->ipv6) {
		TAILQ_FOREACH(rap, ifp->ctx->ipv6->ra_routers, next) {
			if (rap->iface == ifp &&
			    !rap->expired &&
			    (rap->flags & (ND_RA_FLAG_MANAGED | ND_RA_FLAG_OTHER)))
				return 1;
		}
	}
	return 0;
}

static const uint8_t *
ipv6nd_getoption(struct dhcpcd_ctx *ctx,
    size_t *os, unsigned int *code, size_t *len,
    const uint8_t *od, size_t ol, struct dhcp_opt **oopt)
{
	const struct nd_opt_hdr *o;
	size_t i;
	struct dhcp_opt *opt;

	if (od) {
		*os = sizeof(*o);
		if (ol < *os) {
			errno = EINVAL;
			return NULL;
		}
		o = (const struct nd_opt_hdr *)od;
		if (o->nd_opt_len > ol) {
			errno = EINVAL;
			return NULL;
		}
		*len = ND_OPTION_LEN(o);
		*code = o->nd_opt_type;
	} else
		o = NULL;

	for (i = 0, opt = ctx->nd_opts;
	    i < ctx->nd_opts_len; i++, opt++)
	{
		if (opt->option == *code) {
			*oopt = opt;
			break;
		}
	}

	if (o)
		return ND_COPTION_DATA(o);
	return NULL;
}

ssize_t
ipv6nd_env(char **env, const char *prefix, const struct interface *ifp)
{
	size_t i, j, n, len;
	struct ra *rap;
	char ndprefix[32], abuf[24];
	struct dhcp_opt *opt;
	const struct nd_opt_hdr *o;
	struct ipv6_addr *ia;
	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC, &now);
	i = n = 0;
	TAILQ_FOREACH(rap, ifp->ctx->ipv6->ra_routers, next) {
		if (rap->iface != ifp)
			continue;
		i++;
		if (prefix != NULL)
			snprintf(ndprefix, sizeof(ndprefix),
			    "%s_nd%zu", prefix, i);
		else
			snprintf(ndprefix, sizeof(ndprefix),
			    "nd%zu", i);
		if (env)
			setvar(rap->iface->ctx, &env[n], ndprefix,
			    "from", rap->sfrom);
		n++;
		if (env)
			setvard(rap->iface->ctx, &env[n], ndprefix,
			    "acquired", (size_t)rap->acquired.tv_sec);
		n++;
		if (env)
			setvard(rap->iface->ctx, &env[n], ndprefix,
			    "now", (size_t)now.tv_sec);
		n++;

		/* Zero our indexes */
		if (env) {
			for (j = 0, opt = rap->iface->ctx->nd_opts;
			    j < rap->iface->ctx->nd_opts_len;
			    j++, opt++)
				dhcp_zero_index(opt);
			for (j = 0, opt = rap->iface->options->nd_override;
			    j < rap->iface->options->nd_override_len;
			    j++, opt++)
				dhcp_zero_index(opt);
		}

		/* Unlike DHCP, ND6 options *may* occur more than once.
		 * There is also no provision for option concatenation
		 * unlike DHCP. */
		len = rap->data_len -
		    ((size_t)((const uint8_t *)ND_CFIRST_OPTION(rap) -
		    rap->data));

		for (o = ND_CFIRST_OPTION(rap);
		    len >= (ssize_t)sizeof(*o);
		    o = ND_CNEXT_OPTION(o))
		{
			if ((size_t)o->nd_opt_len * 8 > len) {
				errno =	EINVAL;
				break;
			}
			len -= (size_t)(o->nd_opt_len * 8);
			if (has_option_mask(rap->iface->options->nomasknd,
			    o->nd_opt_type))
				continue;
			for (j = 0, opt = rap->iface->options->nd_override;
			    j < rap->iface->options->nd_override_len;
			    j++, opt++)
				if (opt->option == o->nd_opt_type)
					break;
			if (j == rap->iface->options->nd_override_len) {
				for (j = 0, opt = rap->iface->ctx->nd_opts;
				    j < rap->iface->ctx->nd_opts_len;
				    j++, opt++)
					if (opt->option == o->nd_opt_type)
						break;
				if (j == rap->iface->ctx->nd_opts_len)
					opt = NULL;
			}
			if (opt) {
				n += dhcp_envoption(rap->iface->ctx,
				    env == NULL ? NULL : &env[n],
				    ndprefix, rap->iface->name,
				    opt, ipv6nd_getoption,
				    ND_COPTION_DATA(o), ND_OPTION_LEN(o));
			}
		}

		/* We need to output the addresses we actually made
		 * from the prefix information options as well. */
		j = 0;
		TAILQ_FOREACH(ia, &rap->addrs, next) {
			if (!(ia->flags & IPV6_AF_AUTOCONF)
#ifdef IPV6_AF_TEMPORARY
			    || ia->flags & IPV6_AF_TEMPORARY
#endif
			    )
				continue;
			j++;
			if (env) {
				snprintf(abuf, sizeof(abuf), "addr%zu", j);
				setvar(rap->iface->ctx, &env[n], ndprefix,
				    abuf, ia->saddr);
			}
			n++;
		}
	}
	return (ssize_t)n;
}

void
ipv6nd_handleifa(struct dhcpcd_ctx *ctx, int cmd, const char *ifname,
    const struct in6_addr *addr, int flags)
{
	struct ra *rap;

	if (ctx->ipv6 == NULL)
		return;
	TAILQ_FOREACH(rap, ctx->ipv6->ra_routers, next) {
		if (strcmp(rap->iface->name, ifname))
			continue;
		ipv6_handleifa_addrs(cmd, &rap->addrs, addr, flags);
	}
}

void
ipv6nd_expirera(void *arg)
{
	struct interface *ifp;
	struct ra *rap, *ran;
	struct timespec now, lt, expire, next;
	uint8_t expired, valid, validone;

	ifp = arg;
	clock_gettime(CLOCK_MONOTONIC, &now);
	expired = 0;
	timespecclear(&next);

	validone = 0;
	TAILQ_FOREACH_SAFE(rap, ifp->ctx->ipv6->ra_routers, next, ran) {
		if (rap->iface != ifp)
			continue;
		valid = 0;
		if (rap->lifetime) {
			lt.tv_sec = (time_t)rap->lifetime;
			lt.tv_nsec = 0;
			timespecadd(&rap->acquired, &lt, &expire);
			if (rap->lifetime == 0 || timespeccmp(&now, &expire, >))
			{
				if (!rap->expired) {
					logger(ifp->ctx, LOG_WARNING,
					    "%s: %s: router expired",
					    ifp->name, rap->sfrom);
					rap->expired = expired = 1;
					rap->lifetime = 0;
				}
			} else {
				valid = 1;
				timespecsub(&expire, &now, &lt);
				if (!timespecisset(&next) ||
				    timespeccmp(&next, &lt, >))
					next = lt;
			}
		}

		/* XXX FixMe!
		 * We need to extract the lifetime from each option and check
		 * if that has expired or not.
		 * If it has, zero the option out in the returned data. */

		/* No valid lifetimes are left on the RA, so we might
		 * as well punt it. */
		if (!valid && TAILQ_FIRST(&rap->addrs) == NULL)
			ipv6nd_free_ra(rap);
		else
			validone = 1;
	}

	if (timespecisset(&next))
		eloop_timeout_add_tv(ifp->ctx->eloop,
		    &next, ipv6nd_expirera, ifp);
	if (expired) {
		ipv6_buildroutes(ifp->ctx);
		script_runreason(ifp, "ROUTERADVERT");
	}

	/* No valid routers? Kill any DHCPv6. */
	if (!validone)
		dhcp6_drop(ifp, "EXPIRE6");
}

void
ipv6nd_drop(struct interface *ifp)
{
	struct ra *rap;
	uint8_t expired = 0;
	TAILQ_HEAD(rahead, ra) rtrs;

	if (ifp->ctx->ipv6 == NULL)
		return;

	eloop_timeout_delete(ifp->ctx->eloop, NULL, ifp);
	TAILQ_INIT(&rtrs);
	TAILQ_FOREACH(rap, ifp->ctx->ipv6->ra_routers, next) {
		if (rap->iface == ifp) {
			rap->expired = expired = 1;
			TAILQ_REMOVE(ifp->ctx->ipv6->ra_routers, rap, next);
			TAILQ_INSERT_TAIL(&rtrs, rap, next);
		}
	}
	if (expired) {
		while ((rap = TAILQ_FIRST(&rtrs))) {
			TAILQ_REMOVE(&rtrs, rap, next);
			ipv6nd_drop_ra(rap);
		}
		ipv6_buildroutes(ifp->ctx);
		if ((ifp->options->options & DHCPCD_NODROP) != DHCPCD_NODROP)
			script_runreason(ifp, "ROUTERADVERT");
	}
}

static void
ipv6nd_handlena(struct dhcpcd_ctx *dctx, struct interface *ifp,
    struct icmp6_hdr *icp, size_t len, int hoplimit)
{
	struct ipv6_ctx *ctx = dctx->ipv6;
	struct nd_neighbor_advert *nd_na;
	struct ra *rap;
	uint32_t is_router, is_solicited;
	char buf[INET6_ADDRSTRLEN];
	const char *taddr;

	if (ifp == NULL) {
#ifdef DEBUG_NS
		logger(ctx, LOG_DEBUG, "NA for unexpected interface from %s",
		    dctx->sfrom);
#endif
		return;
	}

	if ((size_t)len < sizeof(struct nd_neighbor_advert)) {
		logger(ifp->ctx, LOG_ERR, "%s: IPv6 NA too short from %s",
		    ifp->name, ctx->sfrom);
		return;
	}

	/* RFC 4861 7.1.2 */
	if (hoplimit != 255) {
		logger(dctx, LOG_ERR,
		    "invalid hoplimit(%d) in NA from %s", hoplimit, ctx->sfrom);
		return;
	}

	nd_na = (struct nd_neighbor_advert *)icp;
	is_router = nd_na->nd_na_flags_reserved & ND_NA_FLAG_ROUTER;
	is_solicited = nd_na->nd_na_flags_reserved & ND_NA_FLAG_SOLICITED;
	taddr = inet_ntop(AF_INET6, &nd_na->nd_na_target,
	    buf, INET6_ADDRSTRLEN);

	if (IN6_IS_ADDR_MULTICAST(&nd_na->nd_na_target)) {
		logger(ifp->ctx, LOG_ERR, "%s: NA multicast address %s (%s)",
		    ifp->name, taddr, ctx->sfrom);
		return;
	}

	TAILQ_FOREACH(rap, ctx->ra_routers, next) {
		if (rap->iface == ifp &&
		    IN6_ARE_ADDR_EQUAL(&rap->from, &nd_na->nd_na_target))
			break;
	}
	if (rap == NULL) {
#ifdef DEBUG_NS
		logger(ifp->ctx, LOG_DEBUG, "%s: unexpected NA from %s for %s",
		    ifp->name, ctx->sfrom, taddr);
#endif
		return;
	}

#ifdef DEBUG_NS
	logger(ifp->ctx, LOG_DEBUG, "%s: %sNA for %s from %s",
	    ifp->name, is_solicited ? "solicited " : "", taddr, ctx->sfrom);
#endif

	/* Node is no longer a router, so remove it from consideration */
	if (!is_router && !rap->expired) {
		logger(ifp->ctx, LOG_INFO, "%s: %s not a router (%s)",
		    ifp->name, taddr, ctx->sfrom);
		rap->expired = 1;
		ipv6_buildroutes(ifp->ctx);
		script_runreason(ifp, "ROUTERADVERT");
		return;
	}

	if (is_solicited && is_router && rap->lifetime) {
		if (rap->expired) {
			rap->expired = 0;
			logger(ifp->ctx, LOG_INFO, "%s: %s reachable (%s)",
			    ifp->name, taddr, ctx->sfrom);
			ipv6_buildroutes(ifp->ctx);
			script_runreason(rap->iface, "ROUTERADVERT"); /* XXX */
		}
	}
}

static void
ipv6nd_handledata(void *arg)
{
	struct dhcpcd_ctx *dctx;
	struct ipv6_ctx *ctx;
	ssize_t len;
	struct cmsghdr *cm;
	int hoplimit;
	struct in6_pktinfo pkt;
	struct icmp6_hdr *icp;
	struct interface *ifp;

	dctx = arg;
	ctx = dctx->ipv6;
	ctx->rcvhdr.msg_controllen = CMSG_SPACE(sizeof(struct in6_pktinfo)) +
	    CMSG_SPACE(sizeof(int));
	len = recvmsg(ctx->nd_fd, &ctx->rcvhdr, 0);
	if (len == -1) {
		logger(dctx, LOG_ERR, "recvmsg: %m");
		eloop_event_delete(dctx->eloop, ctx->nd_fd);
		close(ctx->nd_fd);
		ctx->nd_fd = -1;
		return;
	}
	ctx->sfrom = inet_ntop(AF_INET6, &ctx->from.sin6_addr,
	    ctx->ntopbuf, INET6_ADDRSTRLEN);
	if ((size_t)len < sizeof(struct icmp6_hdr)) {
		logger(dctx, LOG_ERR, "IPv6 ICMP packet too short from %s",
		    ctx->sfrom);
		return;
	}

	pkt.ipi6_ifindex = 0;
	hoplimit = 0;
	for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(&ctx->rcvhdr);
	     cm;
	     cm = (struct cmsghdr *)CMSG_NXTHDR(&ctx->rcvhdr, cm))
	{
		if (cm->cmsg_level != IPPROTO_IPV6)
			continue;
		switch(cm->cmsg_type) {
		case IPV6_PKTINFO:
			if (cm->cmsg_len == CMSG_LEN(sizeof(pkt)))
				memcpy(&pkt, CMSG_DATA(cm), sizeof(pkt));
			break;
		case IPV6_HOPLIMIT:
			if (cm->cmsg_len == CMSG_LEN(sizeof(int)))
				memcpy(&hoplimit, CMSG_DATA(cm), sizeof(int));
			break;
		}
	}

	if (pkt.ipi6_ifindex == 0) {
		logger(dctx, LOG_ERR,
		    "IPv6 RA/NA did not contain index from %s",
		    ctx->sfrom);
		return;
	}

	TAILQ_FOREACH(ifp, dctx->ifaces, next) {
		if (ifp->index == (unsigned int)pkt.ipi6_ifindex) {
			if (!(ifp->options->options & DHCPCD_IPV6))
				return;
			break;
		}
	}

	icp = (struct icmp6_hdr *)ctx->rcvhdr.msg_iov[0].iov_base;
	if (icp->icmp6_code == 0) {
		switch(icp->icmp6_type) {
			case ND_NEIGHBOR_ADVERT:
				ipv6nd_handlena(dctx, ifp, icp, (size_t)len,
				   hoplimit);
				return;
			case ND_ROUTER_ADVERT:
				ipv6nd_handlera(dctx, ifp, icp, (size_t)len,
				   hoplimit);
				return;
		}
	}

	logger(dctx, LOG_ERR, "invalid IPv6 type %d or code %d from %s",
	    icp->icmp6_type, icp->icmp6_code, ctx->sfrom);
}

static void
ipv6nd_startrs1(void *arg)
{
	struct interface *ifp = arg;
	struct rs_state *state;

	logger(ifp->ctx, LOG_INFO, "%s: soliciting an IPv6 router", ifp->name);
	if (ipv6nd_open(ifp->ctx) == -1) {
		logger(ifp->ctx, LOG_ERR, "%s: ipv6nd_open: %m", __func__);
		return;
	}

	state = RS_STATE(ifp);
	if (state == NULL) {
		ifp->if_data[IF_DATA_IPV6ND] = calloc(1, sizeof(*state));
		state = RS_STATE(ifp);
		if (state == NULL) {
			logger(ifp->ctx, LOG_ERR, "%s: %m", __func__);
			return;
		}
	}

	/* Always make a new probe as the underlying hardware
	 * address could have changed. */
	ipv6nd_makersprobe(ifp);
	if (state->rs == NULL) {
		logger(ifp->ctx, LOG_ERR,
		    "%s: ipv6ns_makersprobe: %m", __func__);
		return;
	}

	state->rsprobes = 0;
	ipv6nd_sendrsprobe(ifp);
}

void
ipv6nd_startrs(struct interface *ifp)
{
	struct timespec tv;

	eloop_timeout_delete(ifp->ctx->eloop, NULL, ifp);
	if (!(ifp->options->options & DHCPCD_INITIAL_DELAY)) {
		ipv6nd_startrs1(ifp);
		return;
	}

	tv.tv_sec = 0;
	tv.tv_nsec = (suseconds_t)arc4random_uniform(
	    MAX_RTR_SOLICITATION_DELAY * NSEC_PER_SEC);
	timespecnorm(&tv);
	logger(ifp->ctx, LOG_DEBUG,
	    "%s: delaying IPv6 router solicitation for %0.1f seconds",
	    ifp->name, timespec_to_double(&tv));
	eloop_timeout_add_tv(ifp->ctx->eloop, &tv, ipv6nd_startrs1, ifp);
	return;
}
