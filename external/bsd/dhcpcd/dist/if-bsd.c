#include <sys/cdefs.h>
 __RCSID("$NetBSD: if-bsd.c,v 1.24 2015/08/21 13:24:47 roy Exp $");

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
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/utsname.h>

#include <arpa/inet.h>
#include <net/bpf.h>
#include <net/if.h>
#include <net/if_dl.h>
#ifdef __FreeBSD__ /* Needed so that including netinet6/in6_var.h works */
#  include <net/if_var.h>
#endif
#include <net/if_media.h>
#include <net/route.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>
#ifdef __DragonFly__
#  include <netproto/802_11/ieee80211_ioctl.h>
#elif __APPLE__
  /* FIXME: Add apple includes so we can work out SSID */
#else
#  include <net80211/ieee80211.h>
#  include <net80211/ieee80211_ioctl.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <paths.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(OpenBSD) && OpenBSD >= 201411
/* OpenBSD dropped the global setting from sysctl but left the #define
 * which causes a EPERM error when trying to use it.
 * I think both the error and keeping the define are wrong, so we #undef it. */
#undef IPV6CTL_ACCEPT_RTADV
#endif

#include "config.h"
#include "common.h"
#include "dhcp.h"
#include "if.h"
#include "if-options.h"
#include "ipv4.h"
#include "ipv4ll.h"
#include "ipv6.h"
#include "ipv6nd.h"

#include "bpf-filter.h"

#ifndef RT_ROUNDUP
#define RT_ROUNDUP(a)							      \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define RT_ADVANCE(x, n) (x += RT_ROUNDUP((n)->sa_len))
#endif

#define COPYOUT(sin, sa) do {						      \
	if ((sa) && ((sa)->sa_family == AF_INET || (sa)->sa_family == 255))   \
		(sin) = ((struct sockaddr_in*)(void *)(sa))->sin_addr;	      \
	} while (0)

#define COPYOUT6(sin, sa) do {						      \
	if ((sa) && ((sa)->sa_family == AF_INET6 || (sa)->sa_family == 255))  \
		(sin) = ((struct sockaddr_in6*)(void *)(sa))->sin6_addr;      \
	} while (0)

#ifndef CLLADDR
#  define CLLADDR(s) ((const char *)((s)->sdl_data + (s)->sdl_nlen))
#endif

int
if_init(__unused struct interface *iface)
{
	/* BSD promotes secondary address by default */
	return 0;
}

int
if_conf(__unused struct interface *iface)
{
	/* No extra checks needed on BSD */
	return 0;
}

int
if_openlinksocket(void)
{

	return xsocket(PF_ROUTE, SOCK_RAW, 0, O_NONBLOCK|O_CLOEXEC);
}

#if defined(INET) || defined(INET6)
static void
if_linkaddr(struct sockaddr_dl *sdl, const struct interface *ifp)
{

	memset(sdl, 0, sizeof(*sdl));
	sdl->sdl_family = AF_LINK;
	sdl->sdl_len = sizeof(*sdl);
	sdl->sdl_nlen = sdl->sdl_alen = sdl->sdl_slen = 0;
	sdl->sdl_index = (unsigned short)ifp->index;
}
#endif

static int
if_getssid1(int s, const char *ifname, uint8_t *ssid)
{
	int retval = -1;
#if defined(SIOCG80211NWID)
	struct ifreq ifr;
	struct ieee80211_nwid nwid;
#elif defined(IEEE80211_IOC_SSID)
	struct ieee80211req ireq;
	char nwid[IEEE80211_NWID_LEN + 1];
#endif

#if defined(SIOCG80211NWID) /* NetBSD */
	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	memset(&nwid, 0, sizeof(nwid));
	ifr.ifr_data = (void *)&nwid;
	if (ioctl(s, SIOCG80211NWID, &ifr) == 0) {
		if (ssid == NULL)
			retval = nwid.i_len;
		else if (nwid.i_len > IF_SSIDSIZE) {
			errno = ENOBUFS;
			retval = -1;
		} else {
			retval = nwid.i_len;
			memcpy(ssid, nwid.i_nwid, nwid.i_len);
			ssid[nwid.i_len] = '\0';
		}
	}
#elif defined(IEEE80211_IOC_SSID) /* FreeBSD */
	memset(&ireq, 0, sizeof(ireq));
	strlcpy(ireq.i_name, ifname, sizeof(ireq.i_name));
	ireq.i_type = IEEE80211_IOC_SSID;
	ireq.i_val = -1;
	memset(nwid, 0, sizeof(nwid));
	ireq.i_data = &nwid;
	if (ioctl(s, SIOCG80211, &ireq) == 0) {
		if (ssid == NULL)
			retval = ireq.i_len;
		else if (ireq.i_len > IF_SSIDSIZE) {
			errno = ENOBUFS;
			retval = -1;
		} else  {
			retval = ireq.i_len;
			memcpy(ssid, nwid, ireq.i_len);
			ssid[ireq.i_len] = '\0';
		}
	}
#endif

	return retval;
}

int
if_getssid(struct interface *ifp)
{
	int r;

	r = if_getssid1(ifp->ctx->pf_inet_fd, ifp->name, ifp->ssid);
	if (r != -1)
		ifp->ssid_len = (unsigned int)r;
	return r;
}

/*
 * FreeBSD allows for Virtual Access Points
 * We need to check if the interface is a Virtual Interface Master
 * and if so, don't use it.
 * This check is made by virtue of being a IEEE80211 device but
 * returning the SSID gives an error.
 */
int
if_vimaster(const struct dhcpcd_ctx *ctx, const char *ifname)
{
	int r;
	struct ifmediareq ifmr;

	memset(&ifmr, 0, sizeof(ifmr));
	strlcpy(ifmr.ifm_name, ifname, sizeof(ifmr.ifm_name));
	r = ioctl(ctx->pf_inet_fd, SIOCGIFMEDIA, &ifmr);
	if (r == -1)
		return -1;
	if (ifmr.ifm_status & IFM_AVALID &&
	    IFM_TYPE(ifmr.ifm_active) == IFM_IEEE80211)
	{
		if (if_getssid1(ctx->pf_inet_fd, ifname, NULL) == -1)
			return 1;
	}
	return 0;
}

static void
get_addrs(int type, char *cp, struct sockaddr **sa)
{
	int i;

	for (i = 0; i < RTAX_MAX; i++) {
		if (type & (1 << i)) {
			sa[i] = (struct sockaddr *)cp;
			RT_ADVANCE(cp, sa[i]);
		} else
			sa[i] = NULL;
	}
}

#if defined(INET) || defined(INET6)
static struct interface *
if_findsdl(struct dhcpcd_ctx *ctx, struct sockaddr_dl *sdl)
{

	if (sdl->sdl_nlen) {
		char ifname[IF_NAMESIZE];
		memcpy(ifname, sdl->sdl_data, sdl->sdl_nlen);
		ifname[sdl->sdl_nlen] = '\0';
		return if_find(ctx->ifaces, ifname);
	}
	return NULL;
}
#endif

#ifdef INET
const char *if_pfname = "Berkley Packet Filter";

int
if_openrawsocket(struct interface *ifp, uint16_t protocol)
{
	struct ipv4_state *state;
	int fd = -1;
	struct ifreq ifr;
	int ibuf_len = 0;
	size_t buf_len;
	struct bpf_version pv;
	struct bpf_program pf;
#ifdef BIOCIMMEDIATE
	int flags;
#endif
#ifdef _PATH_BPF
	fd = open(_PATH_BPF, O_RDWR | O_CLOEXEC | O_NONBLOCK);
#else
	char device[32];
	int n = 0;

	do {
		snprintf(device, sizeof(device), "/dev/bpf%d", n++);
		fd = open(device, O_RDWR | O_CLOEXEC | O_NONBLOCK);
	} while (fd == -1 && errno == EBUSY);
#endif

	if (fd == -1)
		return -1;

	state = IPV4_STATE(ifp);
	memset(&pv, 0, sizeof(pv));
	if (ioctl(fd, BIOCVERSION, &pv) == -1)
		goto eexit;
	if (pv.bv_major != BPF_MAJOR_VERSION ||
	    pv.bv_minor < BPF_MINOR_VERSION) {
		logger(ifp->ctx, LOG_ERR, "BPF version mismatch - recompile");
		goto eexit;
	}

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifp->name, sizeof(ifr.ifr_name));
	if (ioctl(fd, BIOCSETIF, &ifr) == -1)
		goto eexit;

	/* Get the required BPF buffer length from the kernel. */
	if (ioctl(fd, BIOCGBLEN, &ibuf_len) == -1)
		goto eexit;
	buf_len = (size_t)ibuf_len;
	if (state->buffer_size != buf_len) {
		free(state->buffer);
		state->buffer = malloc(buf_len);
		if (state->buffer == NULL)
			goto eexit;
		state->buffer_size = buf_len;
		state->buffer_len = state->buffer_pos = 0;
	}

#ifdef BIOCIMMEDIATE
	flags = 1;
	if (ioctl(fd, BIOCIMMEDIATE, &flags) == -1)
		goto eexit;
#endif

	/* Install the DHCP filter */
	memset(&pf, 0, sizeof(pf));
	if (protocol == ETHERTYPE_ARP) {
		pf.bf_insns = UNCONST(arp_bpf_filter);
		pf.bf_len = arp_bpf_filter_len;
	} else {
		pf.bf_insns = UNCONST(dhcp_bpf_filter);
		pf.bf_len = dhcp_bpf_filter_len;
	}
	if (ioctl(fd, BIOCSETF, &pf) == -1)
		goto eexit;

	return fd;

eexit:
	free(state->buffer);
	state->buffer = NULL;
	close(fd);
	return -1;
}

ssize_t
if_sendrawpacket(const struct interface *ifp, uint16_t protocol,
    const void *data, size_t len)
{
	struct iovec iov[2];
	struct ether_header hw;
	int fd;

	memset(&hw, 0, ETHER_HDR_LEN);
	memset(&hw.ether_dhost, 0xff, ETHER_ADDR_LEN);
	hw.ether_type = htons(protocol);
	iov[0].iov_base = &hw;
	iov[0].iov_len = ETHER_HDR_LEN;
	iov[1].iov_base = UNCONST(data);
	iov[1].iov_len = len;
	fd = ipv4_protocol_fd(ifp, protocol);
	return writev(fd, iov, 2);
}

/* BPF requires that we read the entire buffer.
 * So we pass the buffer in the API so we can loop on >1 packet. */
ssize_t
if_readrawpacket(struct interface *ifp, uint16_t protocol,
    void *data, size_t len, int *flags)
{
	int fd;
	struct bpf_hdr packet;
	ssize_t bytes;
	const unsigned char *payload;
	struct ipv4_state *state;

	state = IPV4_STATE(ifp);
	fd = ipv4_protocol_fd(ifp, protocol);

	*flags = 0;
	for (;;) {
		if (state->buffer_len == 0) {
			bytes = read(fd, state->buffer, state->buffer_size);
			if (bytes == -1 || bytes == 0)
				return bytes;
			state->buffer_len = (size_t)bytes;
			state->buffer_pos = 0;
		}
		bytes = -1;
		memcpy(&packet, state->buffer + state->buffer_pos,
		    sizeof(packet));
		if (packet.bh_caplen != packet.bh_datalen)
			goto next; /* Incomplete packet, drop. */
		if (state->buffer_pos + packet.bh_caplen + packet.bh_hdrlen >
		    state->buffer_len)
			goto next; /* Packet beyond buffer, drop. */
		payload = state->buffer + state->buffer_pos +
		    packet.bh_hdrlen + ETHER_HDR_LEN;
		bytes = (ssize_t)packet.bh_caplen - ETHER_HDR_LEN;
		if ((size_t)bytes > len)
			bytes = (ssize_t)len;
		memcpy(data, payload, (size_t)bytes);
next:
		state->buffer_pos += BPF_WORDALIGN(packet.bh_hdrlen +
		    packet.bh_caplen);
		if (state->buffer_pos >= state->buffer_len) {
			state->buffer_len = state->buffer_pos = 0;
			*flags |= RAW_EOF;
		}
		if (bytes != -1)
			return bytes;
	}
}

int
if_address(const struct interface *ifp, const struct in_addr *address,
    const struct in_addr *netmask, const struct in_addr *broadcast,
    int action)
{
	int r;
	struct in_aliasreq ifra;

	memset(&ifra, 0, sizeof(ifra));
	strlcpy(ifra.ifra_name, ifp->name, sizeof(ifra.ifra_name));

#define ADDADDR(var, addr) do {						      \
		(var)->sin_family = AF_INET;				      \
		(var)->sin_len = sizeof(*(var));			      \
		(var)->sin_addr = *(addr);				      \
	} while (/*CONSTCOND*/0)
	ADDADDR(&ifra.ifra_addr, address);
	ADDADDR(&ifra.ifra_mask, netmask);
	if (action >= 0 && broadcast)
		ADDADDR(&ifra.ifra_broadaddr, broadcast);
#undef ADDADDR

	r = ioctl(ifp->ctx->pf_inet_fd,
	    action < 0 ? SIOCDIFADDR : SIOCAIFADDR, &ifra);
	return r;
}

static int
if_copyrt(struct dhcpcd_ctx *ctx, struct rt *rt, struct rt_msghdr *rtm)
{
	char *cp;
	struct sockaddr *sa, *rti_info[RTAX_MAX];

	cp = (char *)(void *)(rtm + 1);
	sa = (struct sockaddr *)(void *)cp;
	if (sa->sa_family != AF_INET)
		return -1;
	if (~rtm->rtm_addrs & (RTA_DST | RTA_GATEWAY))
		return -1;
#ifdef RTF_CLONED
	if (rtm->rtm_flags & RTF_CLONED)
		return -1;
#endif
#ifdef RTF_LOCAL
	if (rtm->rtm_flags & RTF_LOCAL)
		return -1;
#endif
#ifdef RTF_BROADCAST
	if (rtm->rtm_flags & RTF_BROADCAST)
		return -1;
#endif

	get_addrs(rtm->rtm_addrs, cp, rti_info);
	memset(rt, 0, sizeof(*rt));
	rt->flags = (unsigned int)rtm->rtm_flags;
	COPYOUT(rt->dest, rti_info[RTAX_DST]);
	if (rtm->rtm_addrs & RTA_NETMASK)
		COPYOUT(rt->net, rti_info[RTAX_NETMASK]);
	else
		rt->net.s_addr = INADDR_BROADCAST;
	COPYOUT(rt->gate, rti_info[RTAX_GATEWAY]);
	COPYOUT(rt->src, rti_info[RTAX_IFA]);

	if (rtm->rtm_inits & RTV_MTU)
		rt->mtu = (unsigned int)rtm->rtm_rmx.rmx_mtu;

	if (rtm->rtm_index)
		rt->iface = if_findindex(ctx->ifaces, rtm->rtm_index);
	else if (rtm->rtm_addrs & RTA_IFP) {
		struct sockaddr_dl *sdl;

		sdl = (struct sockaddr_dl *)(void *)rti_info[RTAX_IFP];
		rt->iface = if_findsdl(ctx, sdl);
	}

	/* If we don't have an interface and it's a host route, it maybe
	 * to a local ip via the loopback interface. */
	if (rt->iface == NULL &&
	    !(~rtm->rtm_flags & (RTF_HOST | RTF_GATEWAY)))
	{
		struct ipv4_addr *ia;

		if ((ia = ipv4_findaddr(ctx, &rt->dest)))
			rt->iface = ia->iface;
	}

	return 0;
}

int
if_route(unsigned char cmd, const struct rt *rt)
{
	const struct dhcp_state *state;
	const struct ipv4ll_state *istate;
	union sockunion {
		struct sockaddr sa;
		struct sockaddr_in sin;
		struct sockaddr_dl sdl;
	} su;
	struct rtm
	{
		struct rt_msghdr hdr;
		char buffer[sizeof(su) * RTAX_MAX];
	} rtm;
	char *bp = rtm.buffer;
	size_t l;

#define ADDSU {								      \
		l = RT_ROUNDUP(su.sa.sa_len);				      \
		memcpy(bp, &su, l);					      \
		bp += l;						      \
	}
#define ADDADDR(addr) {							      \
		memset(&su, 0, sizeof(su));				      \
		su.sin.sin_family = AF_INET;				      \
		su.sin.sin_len = sizeof(su.sin);			      \
		(&su.sin)->sin_addr = *(addr);				      \
		ADDSU;							      \
	}

#if defined(__minix)
	/*
	 * It seems that when dhcpcd(8) is invoked on a single interface, which
	 * is what netconf(8) configures interfaces to do right now, dhcpcd
	 * sometimes tries to remove routes for interfaces it does not manage.
	 * These routes therefore do not have an associated "iface", causing
	 * dhcpcd to crash on a NULL pointer dereference in this function.
	 * This quick hack prevents it from attempting to perform such
	 * dereferences.  The affected scenario seems to be mainly that dhcpcd
	 * is unable to replace a preexisting default route because of this,
	 * but arguably if it didn't set it, it shouldn't remove it either..
	 * Better solutions will have to come from upstream.
	 */
	if (rt->iface == NULL) {
		errno = EINVAL;
		return -1;
	}
#endif /* defined(__minix) */

	if (cmd != RTM_DELETE) {
		state = D_CSTATE(rt->iface);
		istate = IPV4LL_CSTATE(rt->iface);
	} else {
		/* appease GCC */
		state = NULL;
		istate = NULL;
	}
	memset(&rtm, 0, sizeof(rtm));
	rtm.hdr.rtm_version = RTM_VERSION;
	rtm.hdr.rtm_seq = 1;
	rtm.hdr.rtm_type = cmd;
	rtm.hdr.rtm_addrs = RTA_DST;
	if (cmd == RTM_ADD || cmd == RTM_CHANGE)
		rtm.hdr.rtm_addrs |= RTA_GATEWAY;
	rtm.hdr.rtm_flags = RTF_UP;
#ifdef RTF_PINNED
	if (cmd != RTM_ADD)
		rtm.hdr.rtm_flags |= RTF_PINNED;
#endif

	if (cmd != RTM_DELETE) {
		rtm.hdr.rtm_addrs |= RTA_IFA | RTA_IFP;
		/* None interface subnet routes are static. */
		if ((rt->gate.s_addr != INADDR_ANY ||
		    rt->net.s_addr != state->net.s_addr ||
		    rt->dest.s_addr !=
		    (state->addr.s_addr & state->net.s_addr)) &&
		    (istate == NULL ||
		    rt->dest.s_addr !=
		    (istate->addr.s_addr & inaddr_llmask.s_addr) ||
		    rt->net.s_addr != inaddr_llmask.s_addr))
			rtm.hdr.rtm_flags |= RTF_STATIC;
		else {
#ifdef RTF_CLONING
			rtm.hdr.rtm_flags |= RTF_CLONING;
#endif
#ifdef RTP_CONNECTED
			rtm.hdr.rtm_priority = RTP_CONNECTED;
#endif
		}
	}
	if (rt->net.s_addr == htonl(INADDR_BROADCAST) &&
	    rt->gate.s_addr == htonl(INADDR_ANY))
	{
#ifdef RTF_CLONING
		/* We add a cloning network route for a single host.
		 * Traffic to the host will generate a cloned route and the
		 * hardware address will resolve correctly.
		 * It might be more correct to use RTF_HOST instead of
		 * RTF_CLONING, and that does work, but some OS generate
		 * an arp warning diagnostic which we don't want to do. */
		rtm.hdr.rtm_flags |= RTF_CLONING;
		rtm.hdr.rtm_addrs |= RTA_NETMASK;
#else
		rtm.hdr.rtm_flags |= RTF_HOST;
#endif
	} else if (rt->gate.s_addr == htonl(INADDR_LOOPBACK) &&
	    rt->net.s_addr == htonl(INADDR_BROADCAST))
	{
		rtm.hdr.rtm_flags |= RTF_HOST | RTF_GATEWAY;
		/* Going via lo0 so remove the interface flags */
		if (cmd == RTM_ADD)
			rtm.hdr.rtm_addrs &= ~(RTA_IFA | RTA_IFP);
	} else {
		rtm.hdr.rtm_addrs |= RTA_NETMASK;
		if (rtm.hdr.rtm_flags & RTF_STATIC)
			rtm.hdr.rtm_flags |= RTF_GATEWAY;
	}
	if ((cmd == RTM_ADD || cmd == RTM_CHANGE) &&
	    !(rtm.hdr.rtm_flags & RTF_GATEWAY))
		rtm.hdr.rtm_addrs |= RTA_IFA | RTA_IFP;

	ADDADDR(&rt->dest);
	if (rtm.hdr.rtm_addrs & RTA_GATEWAY) {
#ifdef RTF_CLONING
		if ((rtm.hdr.rtm_flags & (RTF_HOST | RTF_CLONING) &&
#else
		if ((rtm.hdr.rtm_flags & RTF_HOST &&
#endif
		    rt->gate.s_addr != htonl(INADDR_LOOPBACK)) ||
		    !(rtm.hdr.rtm_flags & RTF_STATIC))
		{
			if_linkaddr(&su.sdl, rt->iface);
			ADDSU;
		} else
			ADDADDR(&rt->gate);
	}

	if (rtm.hdr.rtm_addrs & RTA_NETMASK)
		ADDADDR(&rt->net);

	if ((cmd == RTM_ADD || cmd == RTM_CHANGE) &&
	    (rtm.hdr.rtm_addrs & (RTA_IFP | RTA_IFA)))
	{
		rtm.hdr.rtm_index = (unsigned short)rt->iface->index;
		if (rtm.hdr.rtm_addrs & RTA_IFP) {
			if_linkaddr(&su.sdl, rt->iface);
			ADDSU;
		}

		if (rtm.hdr.rtm_addrs & RTA_IFA)
			ADDADDR(istate == NULL ? &state->addr : &istate->addr);

		if (rt->mtu) {
			rtm.hdr.rtm_inits |= RTV_MTU;
			rtm.hdr.rtm_rmx.rmx_mtu = rt->mtu;
		}
	}

#undef ADDADDR
#undef ADDSU

	rtm.hdr.rtm_msglen = (unsigned short)(bp - (char *)&rtm);
	return write(rt->iface->ctx->link_fd,
	    &rtm, rtm.hdr.rtm_msglen) == -1 ? -1 : 0;
}

int
if_initrt(struct interface *ifp)
{
	struct rt_msghdr *rtm;
	int mib[6];
	size_t needed;
	char *buf, *p, *end;
	struct rt rt;

	ipv4_freerts(ifp->ctx->ipv4_kroutes);

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;
	mib[4] = NET_RT_DUMP;
	mib[5] = 0;

	if (sysctl(mib, 6, NULL, &needed, NULL, 0) == -1)
		return -1;
	if (needed == 0)
		return 0;
	if ((buf = malloc(needed)) == NULL)
		return -1;
	if (sysctl(mib, 6, buf, &needed, NULL, 0) == -1)
		return -1;

	end = buf + needed;
	for (p = buf; p < end; p += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)(void *)p;
		if (if_copyrt(ifp->ctx, &rt, rtm) == 0)
			ipv4_handlert(ifp->ctx, RTM_ADD, &rt);
	}
	free(buf);
	return 0;
}

#ifdef SIOCGIFAFLAG_IN
int
if_addrflags(const struct in_addr *addr, const struct interface *ifp)
{
	struct ifreq ifr;
	struct sockaddr_in *sin;

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifp->name, sizeof(ifr.ifr_name));
	sin = (struct sockaddr_in *)(void *)&ifr.ifr_addr;
	sin->sin_family = AF_INET;
	sin->sin_addr = *addr;
	if (ioctl(ifp->ctx->pf_inet_fd, SIOCGIFAFLAG_IN, &ifr) == -1)
		return -1;
	return ifr.ifr_addrflags;
}
#else
int
if_addrflags(__unused const struct in_addr *addr,
    __unused const struct interface *ifp)
{

	errno = ENOTSUP;
	return 0;
}
#endif
#endif /* INET */

#ifdef INET6
static void
ifa_scope(struct sockaddr_in6 *sin, unsigned int ifindex)
{

#ifdef __KAME__
	/* KAME based systems want to store the scope inside the sin6_addr
	 * for link local addreses */
	if (IN6_IS_ADDR_LINKLOCAL(&sin->sin6_addr)) {
		uint16_t scope = htons((uint16_t)ifindex);
		memcpy(&sin->sin6_addr.s6_addr[2], &scope,
		    sizeof(scope));
	}
	sin->sin6_scope_id = 0;
#else
	if (IN6_IS_ADDR_LINKLOCAL(&sin->sin6_addr))
		sin->sin6_scope_id = ifindex;
	else
		sin->sin6_scope_id = 0;
#endif
}

#ifdef __KAME__
#define DESCOPE(ia6) do {						      \
	if (IN6_IS_ADDR_LINKLOCAL((ia6)))				      \
		(ia6)->s6_addr[2] = (ia6)->s6_addr[3] = '\0';		      \
	} while (/*CONSTCOND */0)
#else
#define DESCOPE(ia6)
#endif

int
if_address6(const struct ipv6_addr *ia, int action)
{
	struct in6_aliasreq ifa;
	struct in6_addr mask;

	memset(&ifa, 0, sizeof(ifa));
	strlcpy(ifa.ifra_name, ia->iface->name, sizeof(ifa.ifra_name));
	/*
	 * We should not set IN6_IFF_TENTATIVE as the kernel should be
	 * able to work out if it's a new address or not.
	 *
	 * We should set IN6_IFF_AUTOCONF, but the kernel won't let us.
	 * This is probably a safety measure, but still it's not entirely right
	 * either.
	 */
#if 0
	if (ia->autoconf)
		ifa.ifra_flags |= IN6_IFF_AUTOCONF;
#endif
#if defined(__minix)
	/*
	 * On MINIX 3, do set the IN6_IFF_AUTOCONF flag: this tells the TCP/IP
	 * service that the address does not come with an implied subnet.
	 */
	if (ia->flags & IPV6_AF_AUTOCONF)
		ifa.ifra_flags |= IN6_IFF_AUTOCONF;
#endif
#ifdef IPV6_MANAGETEMPADDR /* XXX typo fix on MINIX3: "MANGE" -> "MANAGE" */
	if (ia->flags & IPV6_AF_TEMPORARY)
		ifa.ifra_flags |= IN6_IFF_TEMPORARY;
#endif

#define ADDADDR(v, addr) {						      \
		(v)->sin6_family = AF_INET6;				      \
		(v)->sin6_len = sizeof(*v);				      \
		(v)->sin6_addr = *(addr);				      \
	}

	ADDADDR(&ifa.ifra_addr, &ia->addr);
	ifa_scope(&ifa.ifra_addr, ia->iface->index);
	ipv6_mask(&mask, ia->prefix_len);
	ADDADDR(&ifa.ifra_prefixmask, &mask);
	ifa.ifra_lifetime.ia6t_vltime = ia->prefix_vltime;
	ifa.ifra_lifetime.ia6t_pltime = ia->prefix_pltime;
#undef ADDADDR

	return ioctl(ia->iface->ctx->pf_inet6_fd,
	    action < 0 ? SIOCDIFADDR_IN6 : SIOCAIFADDR_IN6, &ifa);
}


static int
if_copyrt6(struct dhcpcd_ctx *ctx, struct rt6 *rt, struct rt_msghdr *rtm)
{
	char *cp;
	struct sockaddr *sa, *rti_info[RTAX_MAX];

	cp = (char *)(void *)(rtm + 1);
	sa = (struct sockaddr *)(void *)cp;
	if (sa->sa_family != AF_INET6)
		return -1;
	if (~rtm->rtm_addrs & (RTA_DST | RTA_GATEWAY))
		return -1;
#ifdef RTF_CLONED
	if (rtm->rtm_flags & (RTF_CLONED | RTF_HOST))
		return -1;
#else
	if (rtm->rtm_flags & RTF_HOST)
		return -1;
#endif
#ifdef RTF_LOCAL
	if (rtm->rtm_flags & RTF_LOCAL)
		return -1;
#endif

	get_addrs(rtm->rtm_addrs, cp, rti_info);
	memset(rt, 0, sizeof(*rt));
	rt->flags = (unsigned int)rtm->rtm_flags;
	COPYOUT6(rt->dest, rti_info[RTAX_DST]);
	if (rtm->rtm_addrs & RTA_NETMASK) {
		/*
		 * We need to zero out the struct beyond sin6_len and
		 * ensure it's valid.
		 * I have no idea what the invalid data is for, could be
		 * a kernel bug or actually used for something.
		 * Either way it needs to be zeroed out.
		 */
		struct sockaddr_in6 *sin6;
		size_t e, i, len = 0, final = 0;

		sin6 = (struct sockaddr_in6 *)(void *)rti_info[RTAX_NETMASK];
		rt->net = sin6->sin6_addr;
		e = sin6->sin6_len - offsetof(struct sockaddr_in6, sin6_addr);
		if (e > sizeof(struct in6_addr))
			e = sizeof(struct in6_addr);
		for (i = 0; i < e; i++) {
			switch (rt->net.s6_addr[i] & 0xff) {
			case 0xff:
				/* We don't really want the length,
				 * just that it's valid */
				len++;
				break;
			case 0xfe:
			case 0xfc:
			case 0xf8:
			case 0xf0:
			case 0xe0:
			case 0xc0:
			case 0x80:
				len++;
				final = 1;
				break;
			default:
				rt->net.s6_addr[i] = 0x00;
				final = 1;
				break;
			}
			if (final)
				break;
		}
		if (len == 0)
			i = 0;
		while (i < sizeof(rt->net.s6_addr))
			rt->net.s6_addr[i++] = 0x00;
	} else
		ipv6_mask(&rt->net, 128);
	COPYOUT6(rt->gate, rti_info[RTAX_GATEWAY]);

	if (rtm->rtm_inits & RTV_MTU)
		rt->mtu = (unsigned int)rtm->rtm_rmx.rmx_mtu;

	if (rtm->rtm_index)
		rt->iface = if_findindex(ctx->ifaces, rtm->rtm_index);
	else if (rtm->rtm_addrs & RTA_IFP) {
		struct sockaddr_dl *sdl;

		sdl = (struct sockaddr_dl *)(void *)rti_info[RTAX_IFP];
		rt->iface = if_findsdl(ctx, sdl);
	}
	/* If we don't have an interface and it's a host route, it maybe
	 * to a local ip via the loopback interface. */
	if (rt->iface == NULL &&
	    !(~rtm->rtm_flags & (RTF_HOST | RTF_GATEWAY)))
	{
		struct ipv6_addr *ia;

		if ((ia = ipv6_findaddr(ctx, &rt->dest, 0)))
			rt->iface = ia->iface;
	}

	return 0;
}

int
if_route6(unsigned char cmd, const struct rt6 *rt)
{
	union sockunion {
		struct sockaddr sa;
		struct sockaddr_in6 sin;
		struct sockaddr_dl sdl;
	} su;
	struct rtm
	{
		struct rt_msghdr hdr;
		char buffer[sizeof(su) * RTAX_MAX];
	} rtm;
	char *bp = rtm.buffer;
	size_t l;

#define ADDSU {								      \
		l = RT_ROUNDUP(su.sa.sa_len);				      \
		memcpy(bp, &su, l);					      \
		bp += l;						      \
	}
#define ADDADDRS(addr, scope) {						      \
		memset(&su, 0, sizeof(su));				      \
		su.sin.sin6_family = AF_INET6;				      \
		su.sin.sin6_len = sizeof(su.sin);			      \
		(&su.sin)->sin6_addr = *addr;				      \
		if (scope)						      \
			ifa_scope(&su.sin, scope);			      \
		ADDSU;							      \
	}
#define ADDADDR(addr) ADDADDRS(addr, 0)

	memset(&rtm, 0, sizeof(rtm));
	rtm.hdr.rtm_version = RTM_VERSION;
	rtm.hdr.rtm_seq = 1;
	rtm.hdr.rtm_type = cmd;
	rtm.hdr.rtm_flags = RTF_UP | (int)rt->flags;
#ifdef RTF_PINNED
	if (rtm.hdr.rtm_type != RTM_ADD)
		rtm.hdr.rtm_flags |= RTF_PINNED;
#endif
	rtm.hdr.rtm_addrs = RTA_DST | RTA_NETMASK;
	/* None interface subnet routes are static. */
	if (IN6_IS_ADDR_UNSPECIFIED(&rt->gate)) {
#ifdef RTF_CLONING
		rtm.hdr.rtm_flags |= RTF_CLONING;
#endif
#ifdef RTP_CONNECTED
		rtm.hdr.rtm_priority = RTP_CONNECTED;
#endif
	} else
		rtm.hdr.rtm_flags |= RTF_GATEWAY | RTF_STATIC;

	if (cmd == RTM_ADD)
		rtm.hdr.rtm_addrs |= RTA_GATEWAY;
	if (cmd == RTM_ADD && !(rtm.hdr.rtm_flags & RTF_REJECT))
		rtm.hdr.rtm_addrs |= RTA_IFP | RTA_IFA;

	ADDADDR(&rt->dest);
	if (rtm.hdr.rtm_addrs & RTA_GATEWAY) {
		if (IN6_IS_ADDR_UNSPECIFIED(&rt->gate)) {
			if_linkaddr(&su.sdl, rt->iface);
			ADDSU;
		} else {
			ADDADDRS(&rt->gate, rt->iface->index);
		}
	}

	if (rtm.hdr.rtm_addrs & RTA_NETMASK)
		ADDADDR(&rt->net);

	if ((cmd == RTM_ADD || cmd == RTM_CHANGE) &&
	    (rtm.hdr.rtm_addrs & (RTA_IFP | RTA_IFA)))
	{
		rtm.hdr.rtm_index = (unsigned short)rt->iface->index;
		if (rtm.hdr.rtm_addrs & RTA_IFP) {
			if_linkaddr(&su.sdl, rt->iface);
			ADDSU;
		}

		if (rtm.hdr.rtm_addrs & RTA_IFA) {
			const struct ipv6_addr *lla;

			lla = ipv6_linklocal(rt->iface);
			if (lla == NULL) /* unlikely */
					return -1;
			ADDADDRS(&lla->addr, rt->iface->index);
		}

		if (rt->mtu) {
			rtm.hdr.rtm_inits |= RTV_MTU;
			rtm.hdr.rtm_rmx.rmx_mtu = rt->mtu;
		}
	}

#undef ADDADDR
#undef ADDSU

	rtm.hdr.rtm_msglen = (unsigned short)(bp - (char *)&rtm);
	return write(rt->iface->ctx->link_fd,
	    &rtm, rtm.hdr.rtm_msglen) == -1 ? -1 : 0;
}

int
if_initrt6(struct interface *ifp)
{
	struct rt_msghdr *rtm;
	int mib[6];
	size_t needed;
	char *buf, *p, *end;
	struct rt6 rt;

	ipv6_freerts(&ifp->ctx->ipv6->kroutes);

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET6;
	mib[4] = NET_RT_DUMP;
	mib[5] = 0;

	if (sysctl(mib, 6, NULL, &needed, NULL, 0) == -1)
		return -1;
	if (needed == 0)
		return 0;
	if ((buf = malloc(needed)) == NULL)
		return -1;
	if (sysctl(mib, 6, buf, &needed, NULL, 0) == -1)
		return -1;

	end = buf + needed;
	for (p = buf; p < end; p += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)(void *)p;
		if (if_copyrt6(ifp->ctx, &rt, rtm) == 0)
			ipv6_handlert(ifp->ctx, RTM_ADD, &rt);
	}
	free(buf);
	return 0;
}

int
if_addrflags6(const struct in6_addr *addr, const struct interface *ifp)
{
	int flags;
	struct in6_ifreq ifr6;

	memset(&ifr6, 0, sizeof(ifr6));
	strlcpy(ifr6.ifr_name, ifp->name, sizeof(ifr6.ifr_name));
	ifr6.ifr_addr.sin6_family = AF_INET6;
	ifr6.ifr_addr.sin6_addr = *addr;
	ifa_scope(&ifr6.ifr_addr, ifp->index);
	if (ioctl(ifp->ctx->pf_inet6_fd, SIOCGIFAFLAG_IN6, &ifr6) != -1)
		flags = ifr6.ifr_ifru.ifru_flags6;
	else
		flags = -1;
	return flags;
}

int
if_getlifetime6(struct ipv6_addr *ia)
{
	struct in6_ifreq ifr6;
	time_t t;
	struct in6_addrlifetime *lifetime;

	memset(&ifr6, 0, sizeof(ifr6));
	strlcpy(ifr6.ifr_name, ia->iface->name, sizeof(ifr6.ifr_name));
	ifr6.ifr_addr.sin6_family = AF_INET6;
	ifr6.ifr_addr.sin6_addr = ia->addr;
	ifa_scope(&ifr6.ifr_addr, ia->iface->index);
	if (ioctl(ia->iface->ctx->pf_inet6_fd,
	    SIOCGIFALIFETIME_IN6, &ifr6) == -1)
		return -1;

	t = time(NULL);
	lifetime = &ifr6.ifr_ifru.ifru_lifetime;

	if (lifetime->ia6t_preferred)
		ia->prefix_pltime = (uint32_t)(lifetime->ia6t_preferred -
		    MIN(t, lifetime->ia6t_preferred));
	else
		ia->prefix_pltime = ND6_INFINITE_LIFETIME;
	if (lifetime->ia6t_expire) {
		ia->prefix_vltime = (uint32_t)(lifetime->ia6t_expire -
		    MIN(t, lifetime->ia6t_expire));
		/* Calculate the created time */
		clock_gettime(CLOCK_MONOTONIC, &ia->created);
		ia->created.tv_sec -= lifetime->ia6t_vltime - ia->prefix_vltime;
	} else
		ia->prefix_vltime = ND6_INFINITE_LIFETIME;
	return 0;
}
#endif

int
if_managelink(struct dhcpcd_ctx *ctx)
{
	/* route and ifwatchd like a msg buf size of 2048 */
	char msg[2048], *p, *e, *cp;
	ssize_t bytes;
	struct rt_msghdr *rtm;
	struct if_announcemsghdr *ifan;
	struct if_msghdr *ifm;
	struct ifa_msghdr *ifam;
	struct sockaddr *sa, *rti_info[RTAX_MAX];
	int len;
	struct sockaddr_dl sdl;
	struct interface *ifp;
#ifdef INET
	struct rt rt;
#endif
#ifdef INET6
	struct rt6 rt6;
	struct in6_addr ia6, net6;
	struct sockaddr_in6 *sin6;
#endif
#if (defined(INET) && defined(IN_IFF_TENTATIVE)) || defined(INET6)
	int ifa_flags;
#elif defined(__minix)
	int ifa_flags;	/* compilation fix for USE_INET6=no */
#endif

	if ((bytes = read(ctx->link_fd, msg, sizeof(msg))) == -1)
		return -1;
	e = msg + bytes;
	for (p = msg; p < e; p += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)(void *)p;
		// Ignore messages generated by us
		if (rtm->rtm_pid == getpid())
			break;
		switch(rtm->rtm_type) {
#ifdef RTM_IFANNOUNCE
		case RTM_IFANNOUNCE:
			ifan = (struct if_announcemsghdr *)(void *)p;
			switch(ifan->ifan_what) {
			case IFAN_ARRIVAL:
				dhcpcd_handleinterface(ctx, 1,
				    ifan->ifan_name);
				break;
			case IFAN_DEPARTURE:
				dhcpcd_handleinterface(ctx, -1,
				    ifan->ifan_name);
				break;
			}
			break;
#endif
		case RTM_IFINFO:
			ifm = (struct if_msghdr *)(void *)p;
			ifp = if_findindex(ctx->ifaces, ifm->ifm_index);
			if (ifp == NULL)
				break;
			switch (ifm->ifm_data.ifi_link_state) {
			case LINK_STATE_DOWN:
				len = LINK_DOWN;
				break;
			case LINK_STATE_UP:
				len = LINK_UP;
				break;
			default:
				/* handle_carrier will re-load
				 * the interface flags and check for
				 * IFF_RUNNING as some drivers that
				 * don't handle link state also don't
				 * set IFF_RUNNING when this routing
				 * message is generated.
				 * As such, it is a race ...*/
				len = LINK_UNKNOWN;
				break;
			}
			dhcpcd_handlecarrier(ctx, len,
			    (unsigned int)ifm->ifm_flags, ifp->name);
			break;
		case RTM_ADD:
		case RTM_CHANGE:
		case RTM_DELETE:
			cp = (char *)(void *)(rtm + 1);
			sa = (struct sockaddr *)(void *)cp;
			switch (sa->sa_family) {
#ifdef INET
			case AF_INET:
				if (if_copyrt(ctx, &rt, rtm) == 0)
					ipv4_handlert(ctx, rtm->rtm_type, &rt);
				break;
#endif
#ifdef INET6
			case AF_INET6:
				if (~rtm->rtm_addrs & (RTA_DST | RTA_GATEWAY))
					break;
				/*
				 * BSD caches host routes in the
				 * routing table.
				 * As such, we should be notified of
				 * reachability by its existance
				 * with a hardware address
				 */
				if (rtm->rtm_flags & (RTF_HOST)) {
					get_addrs(rtm->rtm_addrs, cp, rti_info);
					COPYOUT6(ia6, rti_info[RTAX_DST]);
					DESCOPE(&ia6);
					if (rti_info[RTAX_GATEWAY]->sa_family
					    == AF_LINK)
						memcpy(&sdl,
						    rti_info[RTAX_GATEWAY],
						    sizeof(sdl));
					else
						sdl.sdl_alen = 0;
					ipv6nd_neighbour(ctx, &ia6,
					    rtm->rtm_type != RTM_DELETE &&
					    sdl.sdl_alen ?
					    IPV6ND_REACHABLE : 0);
					break;
				}

				if (if_copyrt6(ctx, &rt6, rtm) == 0)
					ipv6_handlert(ctx, rtm->rtm_type, &rt6);
				break;
#endif
			}
			break;
#ifdef RTM_CHGADDR
		case RTM_CHGADDR:	/* FALLTHROUGH */
#endif
		case RTM_DELADDR:	/* FALLTHROUGH */
		case RTM_NEWADDR:
			ifam = (struct ifa_msghdr *)(void *)p;
			ifp = if_findindex(ctx->ifaces, ifam->ifam_index);
			if (ifp == NULL)
				break;
			cp = (char *)(void *)(ifam + 1);
			get_addrs(ifam->ifam_addrs, cp, rti_info);
			if (rti_info[RTAX_IFA] == NULL)
				break;
			switch (rti_info[RTAX_IFA]->sa_family) {
			case AF_LINK:
#ifdef RTM_CHGADDR
				if (rtm->rtm_type != RTM_CHGADDR)
					break;
#else
				if (rtm->rtm_type != RTM_NEWADDR)
					break;
#endif
				memcpy(&sdl, rti_info[RTAX_IFA],
				    rti_info[RTAX_IFA]->sa_len);
				dhcpcd_handlehwaddr(ctx, ifp->name,
				    (const unsigned char*)CLLADDR(&sdl),
				    sdl.sdl_alen);
				break;
#ifdef INET
			case AF_INET:
			case 255: /* FIXME: Why 255? */
				COPYOUT(rt.dest, rti_info[RTAX_IFA]);
				COPYOUT(rt.net, rti_info[RTAX_NETMASK]);
				COPYOUT(rt.gate, rti_info[RTAX_BRD]);
				if (rtm->rtm_type == RTM_NEWADDR) {
					ifa_flags = if_addrflags(&rt.dest, ifp);
					if (ifa_flags == -1)
						break;
				} else
					ifa_flags = 0;
				ipv4_handleifa(ctx, rtm->rtm_type,
				    NULL, ifp->name,
				    &rt.dest, &rt.net, &rt.gate, ifa_flags);
				break;
#endif
#ifdef INET6
			case AF_INET6:
				sin6 = (struct sockaddr_in6*)(void *)
				    rti_info[RTAX_IFA];
				ia6 = sin6->sin6_addr;
				DESCOPE(&ia6);
				sin6 = (struct sockaddr_in6*)(void *)
				    rti_info[RTAX_NETMASK];
				net6 = sin6->sin6_addr;
				DESCOPE(&net6);
				if (rtm->rtm_type == RTM_NEWADDR) {
					ifa_flags = if_addrflags6(&ia6, ifp);
					if (ifa_flags == -1)
						break;
				} else
					ifa_flags = 0;
				ipv6_handleifa(ctx, rtm->rtm_type, NULL,
				    ifp->name, &ia6, ipv6_prefixlen(&net6),
				    ifa_flags);
				break;
#endif
			}
			break;
		}
	}
	return 0;
}

#ifndef SYS_NMLN	/* OSX */
#  define SYS_NMLN 256
#endif
#ifndef HW_MACHINE_ARCH
#  ifdef HW_MODEL	/* OpenBSD */
#    define HW_MACHINE_ARCH HW_MODEL
#  endif
#endif
int
if_machinearch(char *str, size_t len)
{
	int mib[2] = { CTL_HW, HW_MACHINE_ARCH };
	char march[SYS_NMLN];
	size_t marchlen = sizeof(march);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    march, &marchlen, NULL, 0) != 0)
		return -1;
	return snprintf(str, len, ":%s", march);
}

#ifdef INET6
#ifdef IPV6CTL_ACCEPT_RTADV
#define get_inet6_sysctl(code) inet6_sysctl(code, 0, 0)
#define set_inet6_sysctl(code, val) inet6_sysctl(code, val, 1)
static int
inet6_sysctl(int code, int val, int action)
{
	int mib[] = { CTL_NET, PF_INET6, IPPROTO_IPV6, 0 };
	size_t size;

	mib[3] = code;
	size = sizeof(val);
	if (action) {
		if (sysctl(mib, sizeof(mib)/sizeof(mib[0]),
		    NULL, 0, &val, size) == -1)
			return -1;
		return 0;
	}
	if (sysctl(mib, sizeof(mib)/sizeof(mib[0]), &val, &size, NULL, 0) == -1)
		return -1;
	return val;
}
#endif

#ifdef IPV6_MANAGETEMPADDR
#ifndef IPV6CTL_TEMPVLTIME
#define get_inet6_sysctlbyname(code) inet6_sysctlbyname(code, 0, 0)
#define set_inet6_sysctlbyname(code, val) inet6_sysctlbyname(code, val, 1)
static int
inet6_sysctlbyname(const char *name, int val, int action)
{
	size_t size;

	size = sizeof(val);
	if (action) {
		if (sysctlbyname(name, NULL, 0, &val, size) == -1)
			return -1;
		return 0;
	}
	if (sysctlbyname(name, &val, &size, NULL, 0) == -1)
		return -1;
	return val;
}
#endif

int
ip6_use_tempaddr(__unused const char *ifname)
{
	int val;

#ifdef IPV6CTL_USETEMPADDR
	val = get_inet6_sysctl(IPV6CTL_USETEMPADDR);
#else
	val = get_inet6_sysctlbyname("net.inet6.ip6.use_tempaddr");
#endif
	return val == -1 ? 0 : val;
}

int
ip6_temp_preferred_lifetime(__unused const char *ifname)
{
	int val;

#ifdef IPV6CTL_TEMPPLTIME
	val = get_inet6_sysctl(IPV6CTL_TEMPPLTIME);
#else
	val = get_inet6_sysctlbyname("net.inet6.ip6.temppltime");
#endif
	return val < 0 ? TEMP_PREFERRED_LIFETIME : val;
}

int
ip6_temp_valid_lifetime(__unused const char *ifname)
{
	int val;

#ifdef IPV6CTL_TEMPVLTIME
	val = get_inet6_sysctl(IPV6CTL_TEMPVLTIME);
#else
	val = get_inet6_sysctlbyname("net.inet6.ip6.tempvltime");
#endif
	return val < 0 ? TEMP_VALID_LIFETIME : val;
}
#endif

#define del_if_nd6_flag(s, ifname, flag) if_nd6_flag((s), (ifp), (flag), -1)
#define get_if_nd6_flag(s, ifname, flag) if_nd6_flag((s), (ifp), (flag),  0)
#define set_if_nd6_flag(s, ifname, flag) if_nd6_flag((s), (ifp), (flag),  1)
static int
if_nd6_flag(int s, const struct interface *ifp, unsigned int flag, int set)
{
	struct in6_ndireq nd;
	unsigned int oflags;

	memset(&nd, 0, sizeof(nd));
	strlcpy(nd.ifname, ifp->name, sizeof(nd.ifname));
	if (ioctl(s, SIOCGIFINFO_IN6, &nd) == -1)
		return -1;
	if (set == 0)
		return nd.ndi.flags & flag ? 1 : 0;

	oflags = nd.ndi.flags;
	if (set == -1)
		nd.ndi.flags &= ~flag;
	else
		nd.ndi.flags |= flag;
	if (oflags == nd.ndi.flags)
		return 0;
	return ioctl(s, SIOCSIFINFO_FLAGS, &nd);
}

static int
if_raflush(int s)
{
	char dummy[IFNAMSIZ + 8];

	strlcpy(dummy, "lo0", sizeof(dummy));
	if (ioctl(s, SIOCSRTRFLUSH_IN6, (void *)&dummy) == -1 ||
	    ioctl(s, SIOCSPFXFLUSH_IN6, (void *)&dummy) == -1)
		return -1;
	return 0;
}

#ifdef SIOCIFAFATTACH
static int
af_attach(int s, const struct interface *ifp, int af)
{
	struct if_afreq ifar;

	strlcpy(ifar.ifar_name, ifp->name, sizeof(ifar.ifar_name));
	ifar.ifar_af = af;
	return ioctl(s, SIOCIFAFATTACH, (void *)&ifar);
}
#endif

#ifdef SIOCGIFXFLAGS
static int
set_ifxflags(int s, const struct interface *ifp, int own)
{
	struct ifreq ifr;
	int flags;

#ifndef IFXF_NOINET6
	/* No point in removing the no inet6 flag if it doesn't
	 * exist and we're not owning inet6. */
	if (! own)
		return 0;
#endif

	strlcpy(ifr.ifr_name, ifp->name, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCGIFXFLAGS, (void *)&ifr) == -1)
		return -1;
	flags = ifr.ifr_flags;
#ifdef IFXF_NOINET6
	flags &= ~IFXF_NOINET6;
#endif
	if (own)
		flags &= ~IFXF_AUTOCONF6;
	if (ifr.ifr_flags == flags)
		return 0;
	ifr.ifr_flags = flags;
	return ioctl(s, SIOCSIFXFLAGS, (void *)&ifr);
}
#endif

static int
_if_checkipv6(int s, struct dhcpcd_ctx *ctx,
    const struct interface *ifp, int own)
{
	int ra;

	if (ifp) {
#ifdef ND6_IFF_OVERRIDE_RTADV
		int override;
#endif

#ifdef ND6_IFF_IFDISABLED
		if (del_if_nd6_flag(s, ifp, ND6_IFF_IFDISABLED) == -1) {
			logger(ifp->ctx, LOG_ERR,
			    "%s: del_if_nd6_flag: ND6_IFF_IFDISABLED: %m",
			    ifp->name);
			return -1;
		}
#endif

#ifdef ND6_IFF_PERFORMNUD
		if (set_if_nd6_flag(s, ifp, ND6_IFF_PERFORMNUD) == -1) {
			logger(ifp->ctx, LOG_ERR,
			    "%s: set_if_nd6_flag: ND6_IFF_PERFORMNUD: %m",
			    ifp->name);
			return -1;
		}
#endif

#ifdef ND6_IFF_AUTO_LINKLOCAL
		if (own) {
			int all;

			all = get_if_nd6_flag(s, ifp, ND6_IFF_AUTO_LINKLOCAL);
			if (all == -1)
				logger(ifp->ctx, LOG_ERR,
				    "%s: get_if_nd6_flag: "
				    "ND6_IFF_AUTO_LINKLOCAL: %m",
				    ifp->name);
			else if (all != 0) {
				logger(ifp->ctx, LOG_DEBUG,
				    "%s: disabling Kernel IPv6 "
				    "auto link-local support",
				    ifp->name);
				if (del_if_nd6_flag(s, ifp,
				    ND6_IFF_AUTO_LINKLOCAL) == -1)
				{
					logger(ifp->ctx, LOG_ERR,
					    "%s: del_if_nd6_flag: "
					    "ND6_IFF_AUTO_LINKLOCAL: %m",
					    ifp->name);
					return -1;
				}
			}
		}
#endif

#ifdef SIOCIFAFATTACH
		if (af_attach(s, ifp, AF_INET6) == -1) {
			logger(ifp->ctx, LOG_ERR,
			    "%s: af_attach: %m", ifp->name);
			return 1;
		}
#endif

#ifdef SIOCGIFXFLAGS
		if (set_ifxflags(s, ifp, own) == -1) {
			logger(ifp->ctx, LOG_ERR,
			    "%s: set_ifxflags: %m", ifp->name);
			return -1;
		}
#endif

#ifdef ND6_IFF_OVERRIDE_RTADV
		override = get_if_nd6_flag(s, ifp, ND6_IFF_OVERRIDE_RTADV);
		if (override == -1)
			logger(ifp->ctx, LOG_ERR,
			    "%s: get_if_nd6_flag: ND6_IFF_OVERRIDE_RTADV: %m",
			    ifp->name);
		else if (override == 0 && own) {
			if (set_if_nd6_flag(s, ifp, ND6_IFF_OVERRIDE_RTADV)
			    == -1)
				logger(ifp->ctx, LOG_ERR,
				    "%s: set_if_nd6_flag: "
				    "ND6_IFF_OVERRIDE_RTADV: %m",
				    ifp->name);
			else
				override = 1;
		}
#endif

#ifdef ND6_IFF_ACCEPT_RTADV
		ra = get_if_nd6_flag(s, ifp, ND6_IFF_ACCEPT_RTADV);
		if (ra == -1)
			logger(ifp->ctx, LOG_ERR,
			    "%s: get_if_nd6_flag: ND6_IFF_ACCEPT_RTADV: %m",
			    ifp->name);
		else if (ra != 0 && own) {
			logger(ifp->ctx, LOG_DEBUG,
			    "%s: disabling Kernel IPv6 RA support",
			    ifp->name);
			if (del_if_nd6_flag(s, ifp, ND6_IFF_ACCEPT_RTADV)
			    == -1)
				logger(ifp->ctx, LOG_ERR,
				    "%s: del_if_nd6_flag: "
				    "ND6_IFF_ACCEPT_RTADV: %m",
				    ifp->name);
			else
				ra = 0;
		} else if (ra == 0 && !own)
			logger(ifp->ctx, LOG_WARNING,
			    "%s: IPv6 kernel autoconf disabled", ifp->name);
#ifdef ND6_IFF_OVERRIDE_RTADV
		if (override == 0 && ra)
			return ctx->ra_global;
#endif
		return ra;
#else
		return ctx->ra_global;
#endif
	}

#ifdef IPV6CTL_ACCEPT_RTADV
	ra = get_inet6_sysctl(IPV6CTL_ACCEPT_RTADV);
	if (ra == -1)
		/* The sysctl probably doesn't exist, but this isn't an
		 * error as such so just log it and continue */
		logger(ifp->ctx, errno == ENOENT ? LOG_DEBUG : LOG_WARNING,
		    "IPV6CTL_ACCEPT_RTADV: %m");
	else if (ra != 0 && own) {
		logger(ifp->ctx, LOG_DEBUG, "disabling Kernel IPv6 RA support");
		if (set_inet6_sysctl(IPV6CTL_ACCEPT_RTADV, 0) == -1) {
			logger(ifp->ctx, LOG_ERR, "IPV6CTL_ACCEPT_RTADV: %m");
			return ra;
		}
		ra = 0;
#else
	ra = 0;
	if (own) {
#endif
		/* Flush the kernel knowledge of advertised routers
		 * and prefixes so the kernel does not expire prefixes
		 * and default routes we are trying to own. */
		if (if_raflush(s) == -1)
			logger(ctx, LOG_WARNING, "if_raflush: %m");
	}

	ctx->ra_global = ra;
	return ra;
}

int
if_checkipv6(struct dhcpcd_ctx *ctx, const struct interface *ifp, int own)
{

	return _if_checkipv6(ctx->pf_inet6_fd, ctx, ifp, own);
}
#endif
