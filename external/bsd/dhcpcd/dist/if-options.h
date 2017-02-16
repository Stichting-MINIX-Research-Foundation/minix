/* $NetBSD: if-options.h,v 1.14 2015/09/04 12:25:01 roy Exp $ */

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

#ifndef IF_OPTIONS_H
#define IF_OPTIONS_H

#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>

#include <getopt.h>
#include <limits.h>
#include <stdint.h>

#include "auth.h"

/* Don't set any optional arguments here so we retain POSIX
 * compatibility with getopt */
#define IF_OPTS "46bc:de:f:gh:i:j:kl:m:no:pqr:s:t:u:v:wxy:z:" \
		"ABC:DEF:GHI:JKLMO:Q:S:TUVW:X:Z:"

#define DEFAULT_TIMEOUT		30
#define DEFAULT_REBOOT		5

#ifndef HOSTNAME_MAX_LEN
#define HOSTNAME_MAX_LEN	250	/* 255 - 3 (FQDN) - 2 (DNS enc) */
#endif
#define VENDORCLASSID_MAX_LEN	255
#define CLIENTID_MAX_LEN	48
#define USERCLASS_MAX_LEN	255
#define VENDOR_MAX_LEN		255

#define DHCPCD_ARP			(1ULL << 0)
#define DHCPCD_RELEASE			(1ULL << 1)
#define DHCPCD_DOMAIN			(1ULL << 2)
#define DHCPCD_GATEWAY			(1ULL << 3)
#define DHCPCD_STATIC			(1ULL << 4)
#define DHCPCD_DEBUG			(1ULL << 5)
#define DHCPCD_LASTLEASE		(1ULL << 7)
#define DHCPCD_INFORM			(1ULL << 8)
#define DHCPCD_REQUEST			(1ULL << 9)
#define DHCPCD_IPV4LL			(1ULL << 10)
#define DHCPCD_DUID			(1ULL << 11)
#define DHCPCD_PERSISTENT		(1ULL << 12)
#define DHCPCD_DAEMONISE		(1ULL << 14)
#define DHCPCD_DAEMONISED		(1ULL << 15)
#define DHCPCD_TEST			(1ULL << 16)
#define DHCPCD_MASTER			(1ULL << 17)
#define DHCPCD_HOSTNAME			(1ULL << 18)
#define DHCPCD_CLIENTID			(1ULL << 19)
#define DHCPCD_LINK			(1ULL << 20)
#define DHCPCD_QUIET			(1ULL << 21)
#define DHCPCD_BACKGROUND		(1ULL << 22)
#define DHCPCD_VENDORRAW		(1ULL << 23)
#define DHCPCD_NOWAITIP			(1ULL << 24) /* To force daemonise */
#define DHCPCD_WAITIP			(1ULL << 25)
#define DHCPCD_SLAACPRIVATE		(1ULL << 26)
#define DHCPCD_CSR_WARNED		(1ULL << 27)
#define DHCPCD_XID_HWADDR		(1ULL << 28)
#define DHCPCD_BROADCAST		(1ULL << 29)
#define DHCPCD_DUMPLEASE		(1ULL << 30)
#define DHCPCD_IPV6RS			(1ULL << 31)
#define DHCPCD_IPV6RA_REQRDNSS		(1ULL << 32)
#define DHCPCD_IPV6RA_OWN		(1ULL << 33)
#define DHCPCD_IPV6RA_OWN_DEFAULT	(1ULL << 34)
#define DHCPCD_IPV4			(1ULL << 35)
#define DHCPCD_FORKED			(1ULL << 36)
#define DHCPCD_IPV6			(1ULL << 37)
#define DHCPCD_STARTED			(1ULL << 38)
#define DHCPCD_NOALIAS			(1ULL << 39)
#define DHCPCD_IA_FORCED		(1ULL << 40)
#define DHCPCD_STOPPING			(1ULL << 41)
#define DHCPCD_DEPARTED			(1ULL << 42)
#define DHCPCD_HOSTNAME_SHORT		(1ULL << 43)
#define DHCPCD_EXITING			(1ULL << 44)
#define DHCPCD_WAITIP4			(1ULL << 45)
#define DHCPCD_WAITIP6			(1ULL << 46)
#define DHCPCD_DEV			(1ULL << 47)
#define DHCPCD_IAID			(1ULL << 48)
#define DHCPCD_DHCP			(1ULL << 49)
#define DHCPCD_DHCP6			(1ULL << 50)
#define DHCPCD_IF_UP			(1ULL << 51)
// unassigned				(1ULL << 52)
// unassinged				(1ULL << 53)
#define DHCPCD_IPV6RA_AUTOCONF		(1ULL << 54)
#define DHCPCD_ROUTER_HOST_ROUTE_WARNED	(1ULL << 55)
#define DHCPCD_IPV6RA_ACCEPT_NOPUBLIC	(1ULL << 56)
#define DHCPCD_BOOTP			(1ULL << 57)
#define DHCPCD_INITIAL_DELAY		(1ULL << 58)

#define DHCPCD_NODROP	(DHCPCD_EXITING | DHCPCD_PERSISTENT)

#define DHCPCD_WAITOPTS	(DHCPCD_WAITIP | DHCPCD_WAITIP4 | DHCPCD_WAITIP6)

#define DHCPCD_WARNINGS	(DHCPCD_CSR_WARNED | \
		DHCPCD_ROUTER_HOST_ROUTE_WARNED)

extern const struct option cf_options[];

struct if_sla {
	char ifname[IF_NAMESIZE];
	uint32_t sla;
	uint8_t prefix_len;
	int8_t sla_set;
};

struct if_ia {
	uint8_t iaid[4];
#ifdef INET6
	uint16_t ia_type;
	uint8_t iaid_set;
	struct in6_addr addr;
	uint8_t prefix_len;
	uint32_t sla_max;
	size_t sla_len;
	struct if_sla *sla;
#endif
};

struct vivco {
	size_t len;
	uint8_t *data;
};

struct if_options {
	time_t mtime;
	uint8_t iaid[4];
	int metric;
	uint8_t requestmask[256 / NBBY];
	uint8_t requiremask[256 / NBBY];
	uint8_t nomask[256 / NBBY];
	uint8_t rejectmask[256 / NBBY];
	uint8_t dstmask[256 / NBBY];
	uint8_t requestmasknd[(UINT16_MAX + 1) / NBBY];
	uint8_t requiremasknd[(UINT16_MAX + 1) / NBBY];
	uint8_t nomasknd[(UINT16_MAX + 1) / NBBY];
	uint8_t rejectmasknd[(UINT16_MAX + 1) / NBBY];
	uint8_t requestmask6[(UINT16_MAX + 1) / NBBY];
	uint8_t requiremask6[(UINT16_MAX + 1) / NBBY];
	uint8_t nomask6[(UINT16_MAX + 1) / NBBY];
	uint8_t rejectmask6[(UINT16_MAX + 1) / NBBY];
	uint32_t leasetime;
	time_t timeout;
	time_t reboot;
	unsigned long long options;

	struct in_addr req_addr;
	struct in_addr req_mask;
	struct rt_head *routes;
	unsigned int mtu;
	char **config;

	char **environ;
	char *script;

	char hostname[HOSTNAME_MAX_LEN + 1]; /* We don't store the length */
	uint8_t fqdn;
	uint8_t vendorclassid[VENDORCLASSID_MAX_LEN + 2];
	uint8_t clientid[CLIENTID_MAX_LEN + 2];
	uint8_t userclass[USERCLASS_MAX_LEN + 2];
	uint8_t vendor[VENDOR_MAX_LEN + 2];

	size_t blacklist_len;
	in_addr_t *blacklist;
	size_t whitelist_len;
	in_addr_t *whitelist;
	size_t arping_len;
	in_addr_t *arping;
	char *fallback;

	struct if_ia *ia;
	size_t ia_len;

	struct dhcp_opt *dhcp_override;
	size_t dhcp_override_len;
	struct dhcp_opt *nd_override;
	size_t nd_override_len;
	struct dhcp_opt *dhcp6_override;
	size_t dhcp6_override_len;
	uint32_t vivco_en;
	struct vivco *vivco;
	size_t vivco_len;
	struct dhcp_opt *vivso_override;
	size_t vivso_override_len;

	struct auth auth;
};

struct if_options *read_config(struct dhcpcd_ctx *,
    const char *, const char *, const char *);
int add_options(struct dhcpcd_ctx *, const char *,
    struct if_options *, int, char **);
void free_dhcp_opt_embenc(struct dhcp_opt *);
void free_options(struct if_options *);

#endif
