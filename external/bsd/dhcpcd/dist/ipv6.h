/* $NetBSD: ipv6.h,v 1.14 2015/07/09 10:15:34 roy Exp $ */

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

#ifndef IPV6_H
#define IPV6_H

#include <sys/uio.h>
#include <netinet/in.h>

#ifndef __linux__
#  ifndef __QNX__
#    include <sys/endian.h>
#  endif
#  include <net/if.h>
#  ifdef __FreeBSD__ /* Needed so that including netinet6/in6_var.h works */
#    include <net/if_var.h>
#  endif
#  ifndef __sun
#    include <netinet6/in6_var.h>
#  endif
#endif

#include "config.h"
#include "dhcpcd.h"

#define ALLROUTERS "ff02::2"

#define ROUNDUP8(a)  (1 + (((a) - 1) |  7))
#define ROUNDUP16(a) (1 + (((a) - 1) | 16))

#define EUI64_GBIT		0x01
#define EUI64_UBIT		0x02
#define EUI64_TO_IFID(in6)	do {(in6)->s6_addr[8] ^= EUI64_UBIT; } while (0)
#define EUI64_GROUP(in6)	((in6)->s6_addr[8] & EUI64_GBIT)

#ifndef ND6_INFINITE_LIFETIME
#  define ND6_INFINITE_LIFETIME		((uint32_t)~0)
#endif

/* RFC4941 constants */
#define TEMP_VALID_LIFETIME	604800	/* 1 week */
#define TEMP_PREFERRED_LIFETIME	86400	/* 1 day */
#define REGEN_ADVANCE		5	/* seconds */
#define MAX_DESYNC_FACTOR	600	/* 10 minutes */

#define TEMP_IDGEN_RETRIES	3
#define GEN_TEMPID_RETRY_MAX	5

/* RFC7217 constants */
#define IDGEN_RETRIES	3
#define IDGEN_DELAY	1 /* second */

/*
 * BSD kernels don't inform userland of DAD results.
 * See the discussion here:
 *    http://mail-index.netbsd.org/tech-net/2013/03/15/msg004019.html
 */
#ifndef __linux__
/* We guard here to avoid breaking a compile on linux ppc-64 headers */
#  include <sys/param.h>
#endif
#ifdef BSD
#  define IPV6_POLLADDRFLAG
#endif

/* This was fixed in NetBSD */
#if defined(__NetBSD_Version__) && __NetBSD_Version__ >= 699002000
#  undef IPV6_POLLADDRFLAG
#endif

/* Linux-3.18 can manage temporary addresses even with RA
 * processing disabled. */
//#undef IFA_F_MANAGETEMPADDR
#if defined(__linux__) && defined(IFA_F_MANAGETEMPADDR)
#define IPV6_MANAGETEMPADDR
#endif

/* Some BSDs do not allow userland to set temporary addresses. */
#if defined(BSD) && defined(IN6_IFF_TEMPORARY)
#define IPV6_MANAGETEMPADDR
#endif

struct ipv6_addr {
	TAILQ_ENTRY(ipv6_addr) next;
	struct interface *iface;
	struct in6_addr prefix;
	uint8_t prefix_len;
	uint32_t prefix_vltime;
	uint32_t prefix_pltime;
	struct timespec created;
	struct timespec acquired;
	struct in6_addr addr;
	int addr_flags;
	short flags;
	char saddr[INET6_ADDRSTRLEN];
	uint8_t iaid[4];
	uint16_t ia_type;
	struct interface *delegating_iface;
	uint8_t prefix_exclude_len;
	struct in6_addr prefix_exclude;

	void (*dadcallback)(void *);
	int dadcounter;
	uint8_t *ns;
	size_t nslen;
	int nsprobes;
};
TAILQ_HEAD(ipv6_addrhead, ipv6_addr);

#define IPV6_AF_ONLINK		0x0001
#define	IPV6_AF_NEW		0x0002
#define IPV6_AF_STALE		0x0004
#define IPV6_AF_ADDED		0x0008
#define IPV6_AF_AUTOCONF	0x0010
#define IPV6_AF_DUPLICATED	0x0020
#define IPV6_AF_DADCOMPLETED	0x0040
#define IPV6_AF_DELEGATED	0x0080
#define IPV6_AF_DELEGATEDPFX	0x0100
#define IPV6_AF_DELEGATEDZERO	0x0200
#define IPV6_AF_REQUEST		0x0400
#ifdef IPV6_MANAGETEMPADDR
#define IPV6_AF_TEMPORARY	0X0800
#endif

struct rt6 {
	TAILQ_ENTRY(rt6) next;
	struct in6_addr dest;
	struct in6_addr net;
	struct in6_addr gate;
	const struct interface *iface;
	unsigned int flags;
#ifdef HAVE_ROUTE_METRIC
	unsigned int metric;
#endif
	unsigned int mtu;
};
TAILQ_HEAD(rt6_head, rt6);

struct ll_callback {
	TAILQ_ENTRY(ll_callback) next;
	void (*callback)(void *);
	void *arg;
};
TAILQ_HEAD(ll_callback_head, ll_callback);

struct ipv6_state {
	struct ipv6_addrhead addrs;
	struct ll_callback_head ll_callbacks;

#ifdef IPV6_MANAGETEMPADDR
	time_t desync_factor;
	uint8_t randomseed0[8]; /* upper 64 bits of MD5 digest */
	uint8_t randomseed1[8]; /* lower 64 bits */
	uint8_t randomid[8];
#endif
};

#define IPV6_STATE(ifp)							       \
	((struct ipv6_state *)(ifp)->if_data[IF_DATA_IPV6])
#define IPV6_CSTATE(ifp)						       \
	((const struct ipv6_state *)(ifp)->if_data[IF_DATA_IPV6])

/* dhcpcd requires CMSG_SPACE to evaluate to a compile time constant. */
#ifdef __QNX__
#undef CMSG_SPACE
#endif

#ifndef ALIGNBYTES
#define ALIGNBYTES (sizeof(int) - 1)
#endif
#ifndef ALIGN
#define	ALIGN(p) (((unsigned int)(p) + ALIGNBYTES) & ~ALIGNBYTES)
#endif
#ifndef CMSG_SPACE
#define	CMSG_SPACE(len)	(ALIGN(sizeof(struct cmsghdr)) + ALIGN(len))
#endif

#define IP6BUFLEN	(CMSG_SPACE(sizeof(struct in6_pktinfo)) + \
			CMSG_SPACE(sizeof(int)))


#ifdef INET6
struct ipv6_ctx {
	struct sockaddr_in6 from;
	struct msghdr sndhdr;
	struct iovec sndiov[2];
	unsigned char sndbuf[CMSG_SPACE(sizeof(struct in6_pktinfo))];
	struct msghdr rcvhdr;
	struct iovec rcviov[2];
	unsigned char rcvbuf[IP6BUFLEN];
	unsigned char ansbuf[1500];
	char ntopbuf[INET6_ADDRSTRLEN];
	const char *sfrom;

	int nd_fd;
	struct ra_head *ra_routers;
	struct rt6_head *routes;

	struct rt6_head kroutes;

	int dhcp_fd;
};

struct ipv6_ctx *ipv6_init(struct dhcpcd_ctx *);
ssize_t ipv6_printaddr(char *, size_t, const uint8_t *, const char *);
int ipv6_makestableprivate(struct in6_addr *addr,
    const struct in6_addr *prefix, int prefix_len,
    const struct interface *ifp, int *dad_counter);
int ipv6_makeaddr(struct in6_addr *, const struct interface *,
    const struct in6_addr *, int);
int ipv6_makeprefix(struct in6_addr *, const struct in6_addr *, int);
int ipv6_mask(struct in6_addr *, int);
uint8_t ipv6_prefixlen(const struct in6_addr *);
int ipv6_userprefix( const struct in6_addr *, short prefix_len,
    uint64_t user_number, struct in6_addr *result, short result_len);
void ipv6_checkaddrflags(void *);
int ipv6_addaddr(struct ipv6_addr *, const struct timespec *);
ssize_t ipv6_addaddrs(struct ipv6_addrhead *addrs);
void ipv6_freedrop_addrs(struct ipv6_addrhead *, int,
    const struct interface *);
void ipv6_handleifa(struct dhcpcd_ctx *ctx, int, struct if_head *,
    const char *, const struct in6_addr *, uint8_t, int);
int ipv6_handleifa_addrs(int, struct ipv6_addrhead *,
    const struct in6_addr *, int);
int ipv6_publicaddr(const struct ipv6_addr *);
const struct ipv6_addr *ipv6_iffindaddr(const struct interface *,
    const struct in6_addr *);
int ipv6_hasaddr(const struct interface *);
int ipv6_findaddrmatch(const struct ipv6_addr *, const struct in6_addr *,
    short);
struct ipv6_addr *ipv6_findaddr(struct dhcpcd_ctx *,
    const struct in6_addr *, short);
#define ipv6_linklocal(ifp) ipv6_iffindaddr((ifp), NULL)
int ipv6_addlinklocalcallback(struct interface *, void (*)(void *), void *);
void ipv6_freeaddr(struct ipv6_addr *);
void ipv6_freedrop(struct interface *, int);
#define ipv6_free(ifp) ipv6_freedrop((ifp), 0)
#define ipv6_drop(ifp) ipv6_freedrop((ifp), 2)

#ifdef IPV6_MANAGETEMPADDR
void ipv6_gentempifid(struct interface *);
void ipv6_settempstale(struct interface *);
struct ipv6_addr *ipv6_createtempaddr(struct ipv6_addr *,
    const struct timespec *);
struct ipv6_addr *ipv6_settemptime(struct ipv6_addr *, int);
void ipv6_addtempaddrs(struct interface *, const struct timespec *);
#else
#define ipv6_gentempifid(a) {}
#define ipv6_settempstale(a) {}
#endif

int ipv6_start(struct interface *);
void ipv6_ctxfree(struct dhcpcd_ctx *);
int ipv6_handlert(struct dhcpcd_ctx *, int cmd, struct rt6 *);
void ipv6_freerts(struct rt6_head *);
void ipv6_buildroutes(struct dhcpcd_ctx *);

#else
#define ipv6_init(a) (NULL)
#define ipv6_start(a) (-1)
#define ipv6_hasaddr(a) (0)
#define ipv6_free_ll_callbacks(a) {}
#define ipv6_free(a) {}
#define ipv6_drop(a) {}
#define ipv6_ctxfree(a) {}
#define ipv6_gentempifid(a) {}
#endif

#endif
