/*	$NetBSD: if.c,v 1.23 2015/06/05 15:41:59 roy Exp $	*/
/*	$KAME: if.c,v 1.36 2004/11/30 22:32:01 suz Exp $	*/

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

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_types.h>
#include <ifaddrs.h>
#ifdef __FreeBSD__
#include <net/ethernet.h>
#else
#include <net/if_ether.h>
#endif
#include <net/route.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <netinet/icmp6.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "rtadvd.h"
#include "if.h"

#ifndef RT_ROUNDUP
#define RT_ROUNDUP(a)							       \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define RT_ADVANCE(x, n) (x += RT_ROUNDUP((n)->sa_len))
#endif

static void
get_rtaddrs(int addrs, struct sockaddr *sa, struct sockaddr **rti_info)
{
	int i;
	
	for (i = 0; i < RTAX_MAX; i++) {
		if (addrs & (1 << i)) {
			rti_info[i] = sa;
			RT_ADVANCE(sa, sa);
		}
		else
			rti_info[i] = NULL;
	}
}

struct sockaddr_dl *
if_nametosdl(const char *name)
{
	struct ifaddrs *ifap, *ifa;
	struct sockaddr_dl *sdl;

	if (getifaddrs(&ifap) != 0)
		return (NULL);

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (strcmp(ifa->ifa_name, name) != 0)
			continue;
		if (ifa->ifa_addr->sa_family != AF_LINK)
			continue;

		sdl = malloc(ifa->ifa_addr->sa_len);
		if (!sdl)
			continue;	/*XXX*/

		memcpy(sdl, ifa->ifa_addr, ifa->ifa_addr->sa_len);
		freeifaddrs(ifap);
		return (sdl);
	}

	freeifaddrs(ifap);
	return (NULL);
}

int
if_getmtu(const char *name)
{
	struct ifreq ifr;
	int s, mtu;

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
		return 0;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_addr.sa_family = AF_INET6;
	strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCGIFMTU, &ifr) != -1)
		mtu = ifr.ifr_mtu;
	else
		mtu = 0;
	close(s);

	return mtu;
}

/* give interface index and its old flags, then new flags returned */
int
if_getflags(int ifindex, int oifflags)
{
	struct ifreq ifr;
	int s;

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		syslog(LOG_ERR, "<%s> socket: %m", __func__);
		return (oifflags & ~IFF_UP);
	}

	memset(&ifr, 0, sizeof(ifr));
	if_indextoname(ifindex, ifr.ifr_name);
	if (ioctl(s, SIOCGIFFLAGS, &ifr) < 0) {
		syslog(LOG_ERR, "<%s> ioctl:SIOCGIFFLAGS: failed for %s",
		       __func__, ifr.ifr_name);
		close(s);
		return (oifflags & ~IFF_UP);
	}
	close(s);
	return (ifr.ifr_flags);
}

#define ROUNDUP8(a) (1 + (((a) - 1) | 7))
int
lladdropt_length(struct sockaddr_dl *sdl)
{
	switch (sdl->sdl_type) {
	case IFT_ETHER:
	case IFT_FDDI:
		return(ROUNDUP8(ETHER_ADDR_LEN + 2));
	default:
		return(0);
	}
}

void
lladdropt_fill(struct sockaddr_dl *sdl, struct nd_opt_hdr *ndopt)
{
	char *addr;

	ndopt->nd_opt_type = ND_OPT_SOURCE_LINKADDR; /* fixed */

	switch (sdl->sdl_type) {
	case IFT_ETHER:
	case IFT_FDDI:
		ndopt->nd_opt_len = (ROUNDUP8(ETHER_ADDR_LEN + 2)) >> 3;
		addr = (char *)(ndopt + 1);
		memcpy(addr, LLADDR(sdl), ETHER_ADDR_LEN);
		break;
	default:
		syslog(LOG_ERR, "<%s> unsupported link type(%d)",
		    __func__, sdl->sdl_type);
		exit(1);
	}

	return;
}

#define FILTER_MATCH(type, filter) ((0x1 << type) & filter)
#define SIN6(s) ((struct sockaddr_in6 *)(s))
#define SDL(s) ((struct sockaddr_dl *)(s))
char *
get_next_msg(char *buf, char *lim, int ifindex, size_t *lenp, int filter)
{
	struct rt_msghdr *rtm;
	struct ifa_msghdr *ifam;
	struct sockaddr *sa, *dst, *gw, *ifa, *rti_info[RTAX_MAX];

	*lenp = 0;
	for (rtm = (struct rt_msghdr *)buf;
	     rtm < (struct rt_msghdr *)lim;
	     rtm = (struct rt_msghdr *)(((char *)rtm) + rtm->rtm_msglen)) {
		/* just for safety */
		if (!rtm->rtm_msglen) {
			syslog(LOG_WARNING, "<%s> rtm_msglen is 0 "
				"(buf=%p lim=%p rtm=%p)", __func__,
				buf, lim, rtm);
			break;
		}
		if (FILTER_MATCH(rtm->rtm_type, filter) == 0) {
			continue;
		}

		switch (rtm->rtm_type) {
		case RTM_GET:
		case RTM_ADD:
		case RTM_DELETE:
			/* address related checks */
			sa = (struct sockaddr *)(rtm + 1);
			get_rtaddrs(rtm->rtm_addrs, sa, rti_info);
			if ((dst = rti_info[RTAX_DST]) == NULL ||
			    dst->sa_family != AF_INET6)
				continue;

			if (IN6_IS_ADDR_LINKLOCAL(&SIN6(dst)->sin6_addr) ||
			    IN6_IS_ADDR_MULTICAST(&SIN6(dst)->sin6_addr))
				continue;

			if ((gw = rti_info[RTAX_GATEWAY]) == NULL ||
			    gw->sa_family != AF_LINK)
				continue;
			if (ifindex && SDL(gw)->sdl_index != ifindex)
				continue;

			if (rti_info[RTAX_NETMASK] == NULL)
				continue;

			/* found */
			*lenp = rtm->rtm_msglen;
			return (char *)rtm;
			/* NOTREACHED */
		case RTM_NEWADDR:
		case RTM_DELADDR:
			ifam = (struct ifa_msghdr *)rtm;

			/* address related checks */
			sa = (struct sockaddr *)(ifam + 1);
			get_rtaddrs(ifam->ifam_addrs, sa, rti_info);
			if ((ifa = rti_info[RTAX_IFA]) == NULL ||
			    (ifa->sa_family != AF_INET &&
			     ifa->sa_family != AF_INET6))
				continue;

			if (ifa->sa_family == AF_INET6 &&
			    (IN6_IS_ADDR_LINKLOCAL(&SIN6(ifa)->sin6_addr) ||
			     IN6_IS_ADDR_MULTICAST(&SIN6(ifa)->sin6_addr)))
				continue;

			if (ifindex && ifam->ifam_index != ifindex)
				continue;

			/* found */
			*lenp = ifam->ifam_msglen;
			return (char *)rtm;
			/* NOTREACHED */
#ifdef RTM_IFANNOUNCE
		case RTM_IFANNOUNCE:
#endif
		case RTM_IFINFO:
			/* found */
			*lenp = rtm->rtm_msglen;
			return (char *)rtm;
			/* NOTREACHED */
		}
	}

	return (char *)rtm;
}
#undef FILTER_MATCH

struct in6_addr *
get_addr(char *buf)
{
	struct rt_msghdr *rtm = (struct rt_msghdr *)buf;
	struct sockaddr *sa, *rti_info[RTAX_MAX];

	sa = (struct sockaddr *)(rtm + 1);
	get_rtaddrs(rtm->rtm_addrs, sa, rti_info);

	return(&SIN6(rti_info[RTAX_DST])->sin6_addr);
}

int
get_rtm_ifindex(char *buf)
{
	struct rt_msghdr *rtm = (struct rt_msghdr *)buf;
	struct sockaddr *sa, *rti_info[RTAX_MAX];

	sa = (struct sockaddr *)(rtm + 1);
	get_rtaddrs(rtm->rtm_addrs, sa, rti_info);

	return(((struct sockaddr_dl *)rti_info[RTAX_GATEWAY])->sdl_index);
}

int
get_ifm_ifindex(char *buf)
{
	struct if_msghdr *ifm = (struct if_msghdr *)buf;

	return ((int)ifm->ifm_index);
}

int
get_ifam_ifindex(char *buf)
{
	struct ifa_msghdr *ifam = (struct ifa_msghdr *)buf;

	return ((int)ifam->ifam_index);
}

int
get_ifm_flags(char *buf)
{
	struct if_msghdr *ifm = (struct if_msghdr *)buf;

	return (ifm->ifm_flags);
}

#ifdef RTM_IFANNOUNCE
int
get_ifan_ifindex(char *buf)
{
	struct if_announcemsghdr *ifan = (struct if_announcemsghdr *)buf;

	return ((int)ifan->ifan_index);
}

int
get_ifan_what(char *buf)
{
	struct if_announcemsghdr *ifan = (struct if_announcemsghdr *)buf;

	return ((int)ifan->ifan_what);
}
#endif

int
get_prefixlen(char *buf)
{
	struct rt_msghdr *rtm = (struct rt_msghdr *)buf;
	struct sockaddr *sa, *rti_info[RTAX_MAX];
	unsigned char *p, *lim;
	
	sa = (struct sockaddr *)(rtm + 1);
	get_rtaddrs(rtm->rtm_addrs, sa, rti_info);
	sa = rti_info[RTAX_NETMASK];

	p = (unsigned char *)(&SIN6(sa)->sin6_addr);
	lim = (unsigned char *)sa + sa->sa_len;
	return prefixlen(p, lim);
}

int
prefixlen(const unsigned char *p, const unsigned char *lim)
{
	int masklen;

	for (masklen = 0; p < lim; p++) {
		switch (*p) {
		case 0xff:
			masklen += 8;
			break;
		case 0xfe:
			masklen += 7;
			break;
		case 0xfc:
			masklen += 6;
			break;
		case 0xf8:
			masklen += 5;
			break;
		case 0xf0:
			masklen += 4;
			break;
		case 0xe0:
			masklen += 3;
			break;
		case 0xc0:
			masklen += 2;
			break;
		case 0x80:
			masklen += 1;
			break;
		case 0x00:
			break;
		default:
			return(-1);
		}
	}

	return(masklen);
}

int
rtmsg_type(char *buf)
{
	struct rt_msghdr *rtm = (struct rt_msghdr *)buf;

	return(rtm->rtm_type);
}

int
rtmsg_len(char *buf)
{
	struct rt_msghdr *rtm = (struct rt_msghdr *)buf;

	return(rtm->rtm_msglen);
}
