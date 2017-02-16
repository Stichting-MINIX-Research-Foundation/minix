/* $NetBSD: dhcp6.h,v 1.10 2015/07/09 10:15:34 roy Exp $ */

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

#ifndef DHCP6_H
#define DHCP6_H

#include "dhcpcd.h"

#define IN6ADDR_LINKLOCAL_ALLDHCP_INIT \
	{{{ 0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02 }}}

/* UDP port numbers for DHCP */
#define DHCP6_CLIENT_PORT	546
#define DHCP6_SERVER_PORT	547

/* DHCP message type */
#define DHCP6_SOLICIT		1
#define DHCP6_ADVERTISE		2
#define DHCP6_REQUEST		3
#define DHCP6_CONFIRM		4
#define DHCP6_RENEW		5
#define DHCP6_REBIND		6
#define DHCP6_REPLY		7
#define DHCP6_RELEASE		8
#define DHCP6_DECLINE		9
#define DHCP6_RECONFIGURE	10
#define DHCP6_INFORMATION_REQ	11
#define DHCP6_RELAY_FLOW	12
#define DHCP6_RELAY_REPL	13
#define DHCP6_RECONFIGURE_REQ	18
#define DHCP6_RECONFIGURE_REPLY	19

#define D6_OPTION_CLIENTID		1
#define D6_OPTION_SERVERID		2
#define D6_OPTION_IA_NA			3
#define D6_OPTION_IA_TA			4
#define D6_OPTION_ORO			6
#define D6_OPTION_IA_ADDR		5
#define D6_OPTION_PREFERENCE		7
#define D6_OPTION_ELAPSED		8
#define D6_OPTION_AUTH			11
#define D6_OPTION_UNICAST		12
#define D6_OPTION_STATUS_CODE		13
#define D6_OPTION_RAPID_COMMIT		14
#define D6_OPTION_VENDOR_CLASS		16
#define D6_OPTION_VENDOR_OPTS		17
#define D6_OPTION_INTERFACE_ID		18
#define D6_OPTION_RECONF_MSG		19
#define D6_OPTION_RECONF_ACCEPT		20
#define D6_OPTION_SIP_SERVERS_NAME	21
#define D6_OPTION_SIP_SERVERS_ADDRESS	22
#define D6_OPTION_DNS_SERVERS		23
#define D6_OPTION_DOMAIN_LIST		24
#define D6_OPTION_IA_PD			25
#define D6_OPTION_IAPREFIX		26
#define D6_OPTION_NIS_SERVERS		27
#define D6_OPTION_NISP_SERVERS		28
#define D6_OPTION_NIS_DOMAIN_NAME	29
#define D6_OPTION_NISP_DOMAIN_NAME	30
#define D6_OPTION_SNTP_SERVERS		31
#define D6_OPTION_INFO_REFRESH_TIME	32
#define D6_OPTION_BCMS_SERVER_D		33
#define D6_OPTION_BCMS_SERVER_A		34
#define D6_OPTION_FQDN			39
#define D6_OPTION_POSIX_TIMEZONE	41
#define D6_OPTION_TZDB_TIMEZONE		42
#define D6_OPTION_PD_EXCLUDE		67
#define D6_OPTION_SOL_MAX_RT		82
#define D6_OPTION_INF_MAX_RT		83

#define D6_FQDN_PTR	0x00
#define D6_FQDN_BOTH	0x01
#define D6_FQDN_NONE	0x04

#include "dhcp.h"
#include "ipv6.h"

struct dhcp6_message {
	uint8_t type;
	uint8_t xid[3];
	/* followed by options */
} __packed;

struct dhcp6_option {
	uint16_t code;
	uint16_t len;
	/* followed by data */
} __packed;

#define D6_STATUS_OK		0
#define D6_STATUS_FAIL		1
#define D6_STATUS_NOADDR	2
#define D6_STATUS_NOBINDING	3
#define D6_STATUS_NOTONLINK	4
#define D6_STATUS_USEMULTICAST	5

#define SOL_MAX_DELAY		1
#define SOL_TIMEOUT		1
#define SOL_MAX_RT		3600 /* RFC7083 */
#define REQ_TIMEOUT		1
#define REQ_MAX_RT		30
#define REQ_MAX_RC		10
#define CNF_MAX_DELAY		1
#define CNF_TIMEOUT		1
#define CNF_MAX_RT		4
#define CNF_MAX_RD		10
#define REN_TIMEOUT		10
#define REN_MAX_RT		600
#define REB_TIMEOUT		10
#define REB_MAX_RT		600
#define INF_MAX_DELAY		1
#define INF_TIMEOUT		1
#define INF_MAX_RT		3600 /* RFC7083 */
#define REL_TIMEOUT		1
#define REL_MAX_RC		5
#define DEC_TIMEOUT		1
#define DEC_MAX_RC		5
#define REC_TIMEOUT		2
#define REC_MAX_RC		8
#define HOP_COUNT_LIMIT		32

/* RFC4242 3.1 */
#define IRT_DEFAULT		86400
#define IRT_MINIMUM		600

#define DHCP6_RAND_MIN		-100
#define DHCP6_RAND_MAX		100

enum DH6S {
	DH6S_INIT,
	DH6S_DISCOVER,
	DH6S_REQUEST,
	DH6S_BOUND,
	DH6S_RENEW,
	DH6S_REBIND,
	DH6S_CONFIRM,
	DH6S_INFORM,
	DH6S_INFORMED,
	DH6S_RENEW_REQUESTED,
	DH6S_PROBE,
	DH6S_DELEGATED,
	DH6S_RELEASE,
	DH6S_RELEASED
};

struct dhcp6_state {
	enum DH6S state;
	struct timespec started;

	/* Message retransmission timings */
	struct timespec RT;
	unsigned int IMD;
	unsigned int RTC;
	time_t IRT;
	unsigned int MRC;
	time_t MRT;
	void (*MRCcallback)(void *);
	time_t sol_max_rt;
	time_t inf_max_rt;

	struct dhcp6_message *send;
	size_t send_len;
	struct dhcp6_message *recv;
	size_t recv_len;
	struct dhcp6_message *new;
	size_t new_len;
	struct dhcp6_message *old;
	size_t old_len;

	uint32_t renew;
	uint32_t rebind;
	uint32_t expire;
	struct in6_addr unicast;
	struct ipv6_addrhead addrs;
	uint32_t lowpl;
	/* The +3 is for the possible .pd extension for prefix delegation */
	char leasefile[sizeof(LEASEFILE6) + IF_NAMESIZE + (IF_SSIDSIZE * 4) +3];
	const char *reason;

	struct authstate auth;
};

#define D6_STATE(ifp)							       \
	((struct dhcp6_state *)(ifp)->if_data[IF_DATA_DHCP6])
#define D6_CSTATE(ifp)							       \
	((const struct dhcp6_state *)(ifp)->if_data[IF_DATA_DHCP6])
#define D6_STATE_RUNNING(ifp)						       \
	(D6_CSTATE((ifp)) &&						       \
	D6_CSTATE((ifp))->reason && dhcp6_dadcompleted((ifp)))

#define D6_FIRST_OPTION(m)						       \
    ((struct dhcp6_option *)						       \
        ((uint8_t *)(m) + sizeof(struct dhcp6_message)))
#define D6_NEXT_OPTION(o)						       \
    ((struct dhcp6_option *)						       \
        (((uint8_t *)o) + sizeof(struct dhcp6_option) + ntohs((o)->len)))
#define D6_OPTION_DATA(o)						       \
    ((uint8_t *)(o) + sizeof(struct dhcp6_option))
#define D6_CFIRST_OPTION(m)						       \
    ((const struct dhcp6_option *)					       \
        ((const uint8_t *)(m) + sizeof(struct dhcp6_message)))
#define D6_CNEXT_OPTION(o)						       \
    ((const struct dhcp6_option *)					       \
        (((const uint8_t *)o) + sizeof(struct dhcp6_option) + ntohs((o)->len)))
#define D6_COPTION_DATA(o)						       \
    ((const uint8_t *)(o) + sizeof(struct dhcp6_option))

#ifdef INET6
void dhcp6_printoptions(const struct dhcpcd_ctx *,
    const struct dhcp_opt *, size_t);
const struct ipv6_addr *dhcp6_iffindaddr(const struct interface *ifp,
    const struct in6_addr *addr, short flags);
struct ipv6_addr *dhcp6_findaddr(struct dhcpcd_ctx *, const struct in6_addr *,
    short);
size_t dhcp6_find_delegates(struct interface *);
int dhcp6_has_public_addr(const struct interface *);
int dhcp6_start(struct interface *, enum DH6S);
void dhcp6_reboot(struct interface *);
ssize_t dhcp6_env(char **, const char *, const struct interface *,
    const struct dhcp6_message *, size_t);
void dhcp6_free(struct interface *);
void dhcp6_handleifa(struct dhcpcd_ctx *, int, const char *,
    const struct in6_addr *addr, int);
int dhcp6_dadcompleted(const struct interface *);
void dhcp6_drop(struct interface *, const char *);
int dhcp6_dump(struct interface *);
#else
#define dhcp6_find_delegates(a) {}
#define dhcp6_start(a, b) (0)
#define dhcp6_reboot(a) {}
#define dhcp6_env(a, b, c, d, e) {}
#define dhcp6_free(a) {}
#define dhcp6_dadcompleted(a) (0)
#define dhcp6_drop(a, b) {}
#define dhcp6_dump(a) (-1)
#endif

#endif
