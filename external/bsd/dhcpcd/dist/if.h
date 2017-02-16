/* $NetBSD: if.h,v 1.12 2015/08/21 10:39:00 roy Exp $ */

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

#ifndef INTERFACE_H
#define INTERFACE_H

#include <net/if.h>
#ifdef __FreeBSD__
#include <net/if_var.h>
#endif
#include <net/route.h>		/* for RTM_ADD et all */
#include <netinet/in.h>
#ifdef BSD
#include <netinet/in_var.h>	/* for IN_IFF_TENTATIVE et all */
#endif

#if defined(__minix)
/*
 * These flags are used for IPv4 autoconfiguration (RFC 3927).  The MINIX 3
 * TCP/IP service does not support IPv4 autoconfiguration, because lwIP's
 * AUTOIP implementation is all-or-nothing by nature: either it implements the
 * whole thing fully itself, or no support is present at all.  dhcpcd(8) needs
 * a more hybrid implementation if at all.  It appears that by undefining the
 * following flags, dhcpcd(8) will assume that no support is present for them
 * in the operating system, and do everything itself instead, which is exactly
 * what we want.
 */
#undef IN_IFF_TENTATIVE
#undef IN_IFF_DUPLICATED
#undef IN_IFF_DETACHED
#undef IN_IFF_TRYTENTATIVE
#undef IN_IFF_NOTREADY
#endif /* defined(__minix) */

/* Some systems have route metrics.
 * OpenBSD route priority is not this. */
#ifndef HAVE_ROUTE_METRIC
# if defined(__linux__)
#  define HAVE_ROUTE_METRIC 1
# endif
#endif

/* Some systems have in-built IPv4 DAD.
 * However, we need them to do DAD at carrier up as well. */
#ifdef IN_IFF_TENTATIVE
#  ifdef __NetBSD__
#    define NOCARRIER_PRESERVE_IP
#  endif
#endif

#include "config.h"
#include "dhcpcd.h"
#include "ipv4.h"
#include "ipv6.h"

#define EUI64_ADDR_LEN			8
#define INFINIBAND_ADDR_LEN		20

/* Linux 2.4 doesn't define this */
#ifndef ARPHRD_IEEE1394
#  define ARPHRD_IEEE1394		24
#endif

/* The BSD's don't define this yet */
#ifndef ARPHRD_INFINIBAND
#  define ARPHRD_INFINIBAND		32
#endif

/* Work out if we have a private address or not
 * 10/8
 * 172.16/12
 * 192.168/16
 */
#ifndef IN_PRIVATE
# define IN_PRIVATE(addr) (((addr & IN_CLASSA_NET) == 0x0a000000) ||	      \
	    ((addr & 0xfff00000)    == 0xac100000) ||			      \
	    ((addr & IN_CLASSB_NET) == 0xc0a80000))
#endif

#define RAW_EOF			1 << 0
#define RAW_PARTIALCSUM		2 << 0

int if_setflag(struct interface *ifp, short flag);
#define if_up(ifp) if_setflag((ifp), (IFF_UP | IFF_RUNNING))
struct if_head *if_discover(struct dhcpcd_ctx *, int, char * const *);
struct interface *if_find(struct if_head *, const char *);
struct interface *if_findindex(struct if_head *, unsigned int);
void if_sortinterfaces(struct dhcpcd_ctx *);
void if_free(struct interface *);
int if_domtu(const struct interface *, short int);
#define if_getmtu(ifp) if_domtu((ifp), 0)
#define if_setmtu(ifp, mtu) if_domtu((ifp), (mtu))
int if_carrier(struct interface *);

/* The below functions are provided by if-KERNEL.c */
int if_conf(struct interface *);
int if_init(struct interface *);
int if_getssid(struct interface *);
int if_vimaster(const struct dhcpcd_ctx *ctx, const char *);
int if_opensockets(struct dhcpcd_ctx *);
int if_openlinksocket(void);
int if_managelink(struct dhcpcd_ctx *);

/* dhcpcd uses the same routing flags as BSD.
 * If the platform doesn't use these flags,
 * map them in the platform interace file. */
#ifndef RTM_ADD
#define RTM_ADD		0x1	/* Add Route */
#define RTM_DELETE	0x2	/* Delete Route */
#define RTM_CHANGE	0x3	/* Change Metrics or flags */
#define RTM_GET		0x4	/* Report Metrics */
#endif

#ifdef INET
extern const char *if_pfname;
int if_openrawsocket(struct interface *, uint16_t);
ssize_t if_sendrawpacket(const struct interface *,
    uint16_t, const void *, size_t);
ssize_t if_readrawpacket(struct interface *, uint16_t, void *, size_t, int *);

int if_address(const struct interface *,
    const struct in_addr *, const struct in_addr *,
    const struct in_addr *, int);
#define if_addaddress(ifp, addr, net, brd)	\
	if_address(ifp, addr, net, brd, 1)
#define if_deladdress(ifp, addr, net)		\
	if_address(ifp, addr, net, NULL, -1)

int if_addrflags(const struct in_addr *, const struct interface *);

int if_route(unsigned char, const struct rt *rt);
int if_initrt(struct interface *);
#endif

#ifdef INET6
int if_checkipv6(struct dhcpcd_ctx *ctx, const struct interface *, int);
#ifdef IPV6_MANAGETEMPADDR
int ip6_use_tempaddr(const char *ifname);
int ip6_temp_preferred_lifetime(const char *ifname);
int ip6_temp_valid_lifetime(const char *ifname);
#else
#define ip6_use_tempaddr(a) (0)
#endif

int if_address6(const struct ipv6_addr *, int);
#define if_addaddress6(a) if_address6(a, 1)
#define if_deladdress6(a) if_address6(a, -1)

int if_addrflags6(const struct in6_addr *, const struct interface *);
int if_getlifetime6(struct ipv6_addr *);

int if_route6(unsigned char, const struct rt6 *rt);
int if_initrt6(struct interface *);
#else
#define if_checkipv6(a, b, c) (-1)
#endif

int if_machinearch(char *, size_t);
int xsocket(int, int, int, int);
#endif
