#include <sys/cdefs.h>
 __RCSID("$NetBSD: if.c,v 1.16 2015/09/04 12:25:01 roy Exp $");

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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#ifdef __FreeBSD__ /* Needed so that including netinet6/in6_var.h works */
#  include <net/if_var.h>
#endif
#ifdef AF_LINK
#  include <net/if_dl.h>
#  include <net/if_types.h>
#  include <netinet/in_var.h>
#endif
#ifdef AF_PACKET
#  include <netpacket/packet.h>
#endif
#ifdef SIOCGIFMEDIA
#  include <net/if_media.h>
#endif
#include <net/route.h>

#include <ctype.h>
#include <errno.h>
#include <ifaddrs.h>
#include <fnmatch.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "config.h"
#include "common.h"
#include "dev.h"
#include "dhcp.h"
#include "dhcp6.h"
#include "if.h"
#include "if-options.h"
#include "ipv4.h"
#include "ipv4ll.h"
#include "ipv6nd.h"

void
if_free(struct interface *ifp)
{

	if (ifp == NULL)
		return;
	ipv4ll_free(ifp);
	dhcp_free(ifp);
	ipv4_free(ifp);
	dhcp6_free(ifp);
	ipv6nd_free(ifp);
	ipv6_free(ifp);
	free_options(ifp->options);
	free(ifp);
}

int
if_opensockets(struct dhcpcd_ctx *ctx)
{

	if ((ctx->link_fd = if_openlinksocket()) == -1)
		return -1;

	ctx->pf_inet_fd = xsocket(PF_INET, SOCK_DGRAM, 0, O_CLOEXEC);
	if (ctx->pf_inet_fd == -1)
		return -1;

#if defined(INET6) && defined(BSD)
	ctx->pf_inet6_fd = xsocket(PF_INET6, SOCK_DGRAM, 0, O_CLOEXEC);
	if (ctx->pf_inet6_fd == -1)
		return -1;
#endif

#ifdef IFLR_ACTIVE
	ctx->pf_link_fd = xsocket(PF_LINK, SOCK_DGRAM, 0, O_CLOEXEC);
	if (ctx->pf_link_fd == -1)
		return -1;
#endif

	return 0;
}

int
if_carrier(struct interface *ifp)
{
	int r;
	struct ifreq ifr;
#ifdef SIOCGIFMEDIA
	struct ifmediareq ifmr;
#endif

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifp->name, sizeof(ifr.ifr_name));
	if (ioctl(ifp->ctx->pf_inet_fd, SIOCGIFFLAGS, &ifr) == -1)
		return LINK_UNKNOWN;
	ifp->flags = (unsigned int)ifr.ifr_flags;

#ifdef SIOCGIFMEDIA
	memset(&ifmr, 0, sizeof(ifmr));
	strlcpy(ifmr.ifm_name, ifp->name, sizeof(ifmr.ifm_name));
	if (ioctl(ifp->ctx->pf_inet_fd, SIOCGIFMEDIA, &ifmr) != -1 &&
	    ifmr.ifm_status & IFM_AVALID)
		r = (ifmr.ifm_status & IFM_ACTIVE) ? LINK_UP : LINK_DOWN;
	else
		r = ifr.ifr_flags & IFF_RUNNING ? LINK_UP : LINK_UNKNOWN;
#else
	r = ifr.ifr_flags & IFF_RUNNING ? LINK_UP : LINK_DOWN;
#endif
	return r;
}

int
if_setflag(struct interface *ifp, short flag)
{
	struct ifreq ifr;
	int r;

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifp->name, sizeof(ifr.ifr_name));
	r = -1;
	if (ioctl(ifp->ctx->pf_inet_fd, SIOCGIFFLAGS, &ifr) == 0) {
		if (flag == 0 || (ifr.ifr_flags & flag) == flag)
			r = 0;
		else {
			ifr.ifr_flags |= flag;
			if (ioctl(ifp->ctx->pf_inet_fd, SIOCSIFFLAGS, &ifr) ==0)
				r = 0;
		}
		ifp->flags = (unsigned int)ifr.ifr_flags;
	}
	return r;
}

static int
if_hasconf(struct dhcpcd_ctx *ctx, const char *ifname)
{
	int i;

	for (i = 0; i < ctx->ifcc; i++) {
		if (strcmp(ctx->ifcv[i], ifname) == 0)
			return 1;
	}
	return 0;
}

static void if_learnaddrs1(struct dhcpcd_ctx *ctx, struct if_head *ifs,
    struct ifaddrs *ifaddrs)
{
	struct ifaddrs *ifa;
	struct interface *ifp;
#ifdef INET
	const struct sockaddr_in *addr, *net, *dst;
#endif
#ifdef INET6
	struct sockaddr_in6 *sin6, *net6;
#endif
	int ifa_flags;


	for (ifa = ifaddrs; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL)
			continue;
		if ((ifp = if_find(ifs, ifa->ifa_name)) == NULL)
			continue;
		switch(ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			addr = (const struct sockaddr_in *)
			    (void *)ifa->ifa_addr;
			net = (const struct sockaddr_in *)
			    (void *)ifa->ifa_netmask;
			if (ifa->ifa_flags & IFF_POINTOPOINT)
				dst = (const struct sockaddr_in *)
				    (void *)ifa->ifa_dstaddr;
			else
				dst = NULL;
			ifa_flags = if_addrflags(&addr->sin_addr, ifp);
			ipv4_handleifa(ctx, RTM_NEWADDR, ifs, ifa->ifa_name,
				&addr->sin_addr,
				&net->sin_addr,
				dst ? &dst->sin_addr : NULL, ifa_flags);
			break;
#endif
#ifdef INET6
		case AF_INET6:
			sin6 = (struct sockaddr_in6 *)(void *)ifa->ifa_addr;
			net6 = (struct sockaddr_in6 *)(void *)ifa->ifa_netmask;
#ifdef __KAME__
			if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr))
				/* Remove the scope from the address */
				sin6->sin6_addr.s6_addr[2] =
				    sin6->sin6_addr.s6_addr[3] = '\0';
#endif
			ifa_flags = if_addrflags6(&sin6->sin6_addr, ifp);
			if (ifa_flags != -1)
				ipv6_handleifa(ctx, RTM_NEWADDR, ifs,
				    ifa->ifa_name,
				    &sin6->sin6_addr,
				    ipv6_prefixlen(&net6->sin6_addr),
				    ifa_flags);
			break;
#endif
		}
	}
}

struct if_head *
if_discover(struct dhcpcd_ctx *ctx, int argc, char * const *argv)
{
	struct ifaddrs *ifaddrs, *ifa;
	char *p;
	int i;
	struct if_head *ifs;
	struct interface *ifp;
#ifdef __linux__
	char ifn[IF_NAMESIZE];
#endif
#ifdef AF_LINK
	const struct sockaddr_dl *sdl;
#ifdef SIOCGIFPRIORITY
	struct ifreq ifr;
#endif
#ifdef IFLR_ACTIVE
	struct if_laddrreq iflr;
#endif

#ifdef IFLR_ACTIVE
	memset(&iflr, 0, sizeof(iflr));
#endif
#elif AF_PACKET
	const struct sockaddr_ll *sll;
#endif

	if (getifaddrs(&ifaddrs) == -1)
		return NULL;
	ifs = malloc(sizeof(*ifs));
	if (ifs == NULL)
		return NULL;
	TAILQ_INIT(ifs);

	for (ifa = ifaddrs; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr != NULL) {
#ifdef AF_LINK
			if (ifa->ifa_addr->sa_family != AF_LINK)
				continue;
#elif AF_PACKET
			if (ifa->ifa_addr->sa_family != AF_PACKET)
				continue;
#endif
		}

		/* It's possible for an interface to have >1 AF_LINK.
		 * For our purposes, we use the first one. */
		TAILQ_FOREACH(ifp, ifs, next) {
			if (strcmp(ifp->name, ifa->ifa_name) == 0)
				break;
		}
		if (ifp)
			continue;

		if (argc > 0) {
			for (i = 0; i < argc; i++) {
#ifdef __linux__
				/* Check the real interface name */
				strlcpy(ifn, argv[i], sizeof(ifn));
				p = strchr(ifn, ':');
				if (p)
					*p = '\0';
				if (strcmp(ifn, ifa->ifa_name) == 0)
					break;
#else
				if (strcmp(argv[i], ifa->ifa_name) == 0)
					break;
#endif
			}
			if (i == argc)
				continue;
			p = argv[i];
		} else {
			p = ifa->ifa_name;
#ifdef __linux__
			strlcpy(ifn, ifa->ifa_name, sizeof(ifn));
#endif
			/* -1 means we're discovering against a specific
			 * interface, but we still need the below rules
			 * to apply. */
			if (argc == -1 && strcmp(argv[0], ifa->ifa_name) != 0)
				continue;
		}
		for (i = 0; i < ctx->ifdc; i++)
			if (!fnmatch(ctx->ifdv[i], p, 0))
				break;
		if (i < ctx->ifdc)
			continue;
		for (i = 0; i < ctx->ifac; i++)
			if (!fnmatch(ctx->ifav[i], p, 0))
				break;
		if (ctx->ifac && i == ctx->ifac)
			continue;

		/* Ensure that the interface name has settled */
		if (!dev_initialized(ctx, p))
			continue;

		/* Don't allow loopback or pointopoint unless explicit */
		if (ifa->ifa_flags & (IFF_LOOPBACK | IFF_POINTOPOINT)) {
			if ((argc == 0 || argc == -1) &&
			    ctx->ifac == 0 && !if_hasconf(ctx, p))
				continue;
		}

		if (if_vimaster(ctx, p) == 1) {
			logger(ctx, argc ? LOG_ERR : LOG_DEBUG,
			    "%s: is a Virtual Interface Master, skipping", p);
			continue;
		}

		ifp = calloc(1, sizeof(*ifp));
		if (ifp == NULL) {
			logger(ctx, LOG_ERR, "%s: %m", __func__);
			break;
		}
		ifp->ctx = ctx;
#ifdef __linux__
		strlcpy(ifp->name, ifn, sizeof(ifp->name));
		strlcpy(ifp->alias, p, sizeof(ifp->alias));
#else
		strlcpy(ifp->name, p, sizeof(ifp->name));
#endif
		ifp->flags = ifa->ifa_flags;
		ifp->carrier = if_carrier(ifp);

		if (ifa->ifa_addr != NULL) {
#ifdef AF_LINK
			sdl = (const struct sockaddr_dl *)(void *)ifa->ifa_addr;

#ifdef IFLR_ACTIVE
			/* We need to check for active address */
			strlcpy(iflr.iflr_name, ifp->name,
			    sizeof(iflr.iflr_name));
			memcpy(&iflr.addr, ifa->ifa_addr,
			    MIN(ifa->ifa_addr->sa_len, sizeof(iflr.addr)));
			iflr.flags = IFLR_PREFIX;
			iflr.prefixlen = (unsigned int)sdl->sdl_alen * NBBY;
			if (ioctl(ctx->pf_link_fd, SIOCGLIFADDR, &iflr) == -1 ||
			    !(iflr.flags & IFLR_ACTIVE))
			{
				if_free(ifp);
				continue;
			}
#endif

			ifp->index = sdl->sdl_index;
			switch(sdl->sdl_type) {
#ifdef IFT_BRIDGE
			case IFT_BRIDGE: /* FALLTHROUGH */
#endif
#ifdef IFT_PPP
			case IFT_PPP: /* FALLTHROUGH */
#endif
#ifdef IFT_PROPVIRTUAL
			case IFT_PROPVIRTUAL: /* FALLTHROUGH */
#endif
#if defined(IFT_BRIDGE) || defined(IFT_PPP) || defined(IFT_PROPVIRTUAL)
				/* Don't allow unless explicit */
				if ((argc == 0 || argc == -1) &&
				    ctx->ifac == 0 &&
				    !if_hasconf(ctx, ifp->name))
				{
					logger(ifp->ctx, LOG_DEBUG,
					    "%s: ignoring due to"
					    " interface type and"
					    " no config",
					    ifp->name);
					if_free(ifp);
					continue;
				}
				/* FALLTHROUGH */
#endif
#ifdef IFT_L2VLAN
			case IFT_L2VLAN: /* FALLTHROUGH */
#endif
#ifdef IFT_L3IPVLAN
			case IFT_L3IPVLAN: /* FALLTHROUGH */
#endif
			case IFT_ETHER:
				ifp->family = ARPHRD_ETHER;
				break;
#ifdef IFT_IEEE1394
			case IFT_IEEE1394:
				ifp->family = ARPHRD_IEEE1394;
				break;
#endif
#ifdef IFT_INFINIBAND
			case IFT_INFINIBAND:
				ifp->family = ARPHRD_INFINIBAND;
				break;
#endif
			default:
				/* Don't allow unless explicit */
				if ((argc == 0 || argc == -1) &&
				    ctx->ifac == 0 &&
				    !if_hasconf(ctx, ifp->name))
				{
					if_free(ifp);
					continue;
				}
				logger(ifp->ctx, LOG_WARNING,
				    "%s: unsupported interface type %.2x",
				    ifp->name, sdl->sdl_type);
				/* Pretend it's ethernet */
				ifp->family = ARPHRD_ETHER;
				break;
			}
			ifp->hwlen = sdl->sdl_alen;
#ifndef CLLADDR
#  define CLLADDR(s) ((const char *)((s)->sdl_data + (s)->sdl_nlen))
#endif
			memcpy(ifp->hwaddr, CLLADDR(sdl), ifp->hwlen);
#elif AF_PACKET
			sll = (const struct sockaddr_ll *)(void *)ifa->ifa_addr;
			ifp->index = (unsigned int)sll->sll_ifindex;
			ifp->family = sll->sll_hatype;
			ifp->hwlen = sll->sll_halen;
			if (ifp->hwlen != 0)
				memcpy(ifp->hwaddr, sll->sll_addr, ifp->hwlen);
#endif
		}
#ifdef __linux__
		/* PPP addresses on Linux don't have hardware addresses */
		else
			ifp->index = if_nametoindex(ifp->name);
#endif

		/* We only work on ethernet by default */
		if (ifp->family != ARPHRD_ETHER) {
			if ((argc == 0 || argc == -1) &&
			    ctx->ifac == 0 && !if_hasconf(ctx, ifp->name))
			{
				if_free(ifp);
				continue;
			}
			switch (ifp->family) {
			case ARPHRD_IEEE1394:
			case ARPHRD_INFINIBAND:
#ifdef ARPHRD_LOOPBACK
			case ARPHRD_LOOPBACK:
#endif
#ifdef ARPHRD_PPP
			case ARPHRD_PPP:
#endif
				/* We don't warn for supported families */
				break;

/* IFT already checked */
#ifndef AF_LINK
			default:
				logger(ifp->ctx, LOG_WARNING,
				    "%s: unsupported interface family %.2x",
				    ifp->name, ifp->family);
				break;
#endif
			}
		}

		if (!(ctx->options & (DHCPCD_DUMPLEASE | DHCPCD_TEST))) {
			/* Handle any platform init for the interface */
			if (if_init(ifp) == -1) {
				logger(ifp->ctx, LOG_ERR, "%s: if_init: %m", p);
				if_free(ifp);
				continue;
			}

			/* Ensure that the MTU is big enough for DHCP */
			if (if_getmtu(ifp) < MTU_MIN &&
			    if_setmtu(ifp, MTU_MIN) == -1)
			{
				logger(ifp->ctx, LOG_ERR,
				    "%s: if_setmtu: %m", p);
				if_free(ifp);
				continue;
			}
		}

#ifdef SIOCGIFPRIORITY
		/* Respect the interface priority */
		memset(&ifr, 0, sizeof(ifr));
		strlcpy(ifr.ifr_name, ifp->name, sizeof(ifr.ifr_name));
		if (ioctl(ctx->pf_inet_fd, SIOCGIFPRIORITY, &ifr) == 0)
			ifp->metric = ifr.ifr_metric;
#else
		/* We reserve the 100 range for virtual interfaces, if and when
		 * we can work them out. */
		ifp->metric = 200 + ifp->index;
		if (if_getssid(ifp) != -1) {
			ifp->wireless = 1;
			ifp->metric += 100;
		}
#endif

		TAILQ_INSERT_TAIL(ifs, ifp, next);
	}

	if_learnaddrs1(ctx, ifs, ifaddrs);
	freeifaddrs(ifaddrs);

	return ifs;
}

static struct interface *
if_findindexname(struct if_head *ifaces, unsigned int idx, const char *name)
{

	if (ifaces != NULL) {
		struct interface *ifp;

		TAILQ_FOREACH(ifp, ifaces, next) {
			if ((name && strcmp(ifp->name, name) == 0) ||
#ifdef __linux__
			    (name && strcmp(ifp->alias, name) == 0) ||
#endif
			    (!name && ifp->index == idx))
				return ifp;
		}
	}

	errno = ESRCH;
	return NULL;
}

struct interface *
if_find(struct if_head *ifaces, const char *name)
{

	return if_findindexname(ifaces, 0, name);
}

struct interface *
if_findindex(struct if_head *ifaces, unsigned int idx)
{

	return if_findindexname(ifaces, idx, NULL);
}

int
if_domtu(const struct interface *ifp, short int mtu)
{
	int r;
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifp->name, sizeof(ifr.ifr_name));
	ifr.ifr_mtu = mtu;
	r = ioctl(ifp->ctx->pf_inet_fd, mtu ? SIOCSIFMTU : SIOCGIFMTU, &ifr);
	if (r == -1)
		return -1;
	return ifr.ifr_mtu;
}

/* Interface comparer for working out ordering. */
static int
if_cmp(const struct interface *si, const struct interface *ti)
{
#ifdef INET
	int r;
#endif

	/* Check carrier status first */
	if (si->carrier > ti->carrier)
		return -1;
	if (si->carrier < ti->carrier)
		return 1;

	if (D_STATE_RUNNING(si) && !D_STATE_RUNNING(ti))
		return -1;
	if (!D_STATE_RUNNING(si) && D_STATE_RUNNING(ti))
		return 1;
	if (RS_STATE_RUNNING(si) && !RS_STATE_RUNNING(ti))
		return -1;
	if (!RS_STATE_RUNNING(si) && RS_STATE_RUNNING(ti))
		return 1;
	if (D6_STATE_RUNNING(si) && !D6_STATE_RUNNING(ti))
		return -1;
	if (!D6_STATE_RUNNING(si) && D6_STATE_RUNNING(ti))
		return 1;

#ifdef INET
	/* Special attention needed here due to states and IPv4LL. */
	if ((r = ipv4_ifcmp(si, ti)) != 0)
		return r;
#endif

	/* Finally, metric */
	if (si->metric < ti->metric)
		return -1;
	if (si->metric > ti->metric)
		return 1;
	return 0;
}

/* Sort the interfaces into a preferred order - best first, worst last. */
void
if_sortinterfaces(struct dhcpcd_ctx *ctx)
{
	struct if_head sorted;
	struct interface *ifp, *ift;

	if (ctx->ifaces == NULL ||
	    (ifp = TAILQ_FIRST(ctx->ifaces)) == NULL ||
	    TAILQ_NEXT(ifp, next) == NULL)
		return;

	TAILQ_INIT(&sorted);
	TAILQ_REMOVE(ctx->ifaces, ifp, next);
	TAILQ_INSERT_HEAD(&sorted, ifp, next);
	while ((ifp = TAILQ_FIRST(ctx->ifaces))) {
		TAILQ_REMOVE(ctx->ifaces, ifp, next);
		TAILQ_FOREACH(ift, &sorted, next) {
			if (if_cmp(ifp, ift) == -1) {
				TAILQ_INSERT_BEFORE(ift, ifp, next);
				break;
			}
		}
		if (ift == NULL)
			TAILQ_INSERT_TAIL(&sorted, ifp, next);
	}
	TAILQ_CONCAT(ctx->ifaces, &sorted, next);
}

int
xsocket(int domain, int type, int protocol, int flags)
{
#ifdef SOCK_CLOEXEC
	if (flags & O_CLOEXEC)
		type |= SOCK_CLOEXEC;
	if (flags & O_NONBLOCK)
		type |= SOCK_NONBLOCK;

	return socket(domain, type, protocol);
#else
	int s, xflags;

	if ((s = socket(domain, type, protocol)) == -1)
		return -1;
	if ((flags & O_CLOEXEC) && (xflags = fcntl(s, F_GETFD, 0)) == -1 ||
	    fcntl(s, F_SETFD, xflags | FD_CLOEXEC) == -1)
		goto out;
	if ((flags & O_NONBLOCK) && (xflags = fcntl(s, F_GETFL, 0)) == -1 ||
	    fcntl(s, F_SETFL, xflags | O_NONBLOCK) == -1)
		goto out;
	return s;
out:
	close(s);
	return -1;
#endif
}
