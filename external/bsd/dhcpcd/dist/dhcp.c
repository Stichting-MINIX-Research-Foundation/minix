#include <sys/cdefs.h>
 __RCSID("$NetBSD: dhcp.c,v 1.35 2015/09/04 12:25:01 roy Exp $");

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
#include <sys/socket.h>
#include <sys/stat.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#define __FAVOR_BSD /* Nasty glibc hack so we can use BSD semantics for UDP */
#include <netinet/udp.h>
#undef __FAVOR_BSD

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ELOOP_QUEUE 2
#include "config.h"
#include "arp.h"
#include "common.h"
#include "dhcp.h"
#include "dhcpcd.h"
#include "dhcp-common.h"
#include "duid.h"
#include "eloop.h"
#include "if.h"
#include "ipv4.h"
#include "ipv4ll.h"
#include "script.h"

#define DAD		"Duplicate address detected"
#define DHCP_MIN_LEASE	20

#define IPV4A		ADDRIPV4 | ARRAY
#define IPV4R		ADDRIPV4 | REQUEST

/* We should define a maximum for the NAK exponential backoff */
#define NAKOFF_MAX              60

/* Wait N nanoseconds between sending a RELEASE and dropping the address.
 * This gives the kernel enough time to actually send it. */
#define RELEASE_DELAY_S		0
#define RELEASE_DELAY_NS	10000000

#ifndef IPDEFTTL
#define IPDEFTTL 64 /* RFC1340 */
#endif

struct dhcp_op {
	uint8_t value;
	const char *name;
};

static const struct dhcp_op dhcp_ops[] = {
	{ DHCP_DISCOVER,   "DISCOVER" },
	{ DHCP_OFFER,      "OFFER" },
	{ DHCP_REQUEST,    "REQUEST" },
	{ DHCP_DECLINE,    "DECLINE" },
	{ DHCP_ACK,        "ACK" },
	{ DHCP_NAK,        "NAK" },
	{ DHCP_RELEASE,    "RELEASE" },
	{ DHCP_INFORM,     "INFORM" },
	{ DHCP_FORCERENEW, "DHCP_FORCERENEW"},
	{ 0, NULL }
};

static const char * const dhcp_params[] = {
	"ip_address",
	"subnet_cidr",
	"network_number",
	"filename",
	"server_name",
	NULL
};

struct udp_dhcp_packet
{
	struct ip ip;
	struct udphdr udp;
	struct dhcp_message dhcp;
};

static const size_t udp_dhcp_len = sizeof(struct udp_dhcp_packet);

static int dhcp_open(struct interface *ifp);

void
dhcp_printoptions(const struct dhcpcd_ctx *ctx,
    const struct dhcp_opt *opts, size_t opts_len)
{
	const char * const *p;
	size_t i, j;
	const struct dhcp_opt *opt, *opt2;
	int cols;

	for (p = dhcp_params; *p; p++)
		printf("    %s\n", *p);

	for (i = 0, opt = ctx->dhcp_opts; i < ctx->dhcp_opts_len; i++, opt++) {
		for (j = 0, opt2 = opts; j < opts_len; j++, opt2++)
			if (opt->option == opt2->option)
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

#define get_option_raw(ctx, dhcp, opt) get_option(ctx, dhcp, opt, NULL)
static const uint8_t *
get_option(struct dhcpcd_ctx *ctx,
    const struct dhcp_message *dhcp, unsigned int opt, size_t *len)
{
	const uint8_t *p = dhcp->options;
	const uint8_t *e = p + sizeof(dhcp->options);
	uint8_t l, ol = 0;
	uint8_t o = 0;
	uint8_t overl = 0;
	uint8_t *bp = NULL;
	const uint8_t *op = NULL;
	size_t bl = 0;

	/* Check we have the magic cookie */
	if (dhcp->cookie != htonl(MAGIC_COOKIE)) {
		errno = ENOTSUP;
		return NULL;
	}

	while (p < e) {
		o = *p++;
		switch (o) {
		case DHO_PAD:
			/* No length to read */
			continue;
		case DHO_END:
			if (overl & 1) {
				/* bit 1 set means parse boot file */
				overl = (uint8_t)(overl & ~1);
				p = dhcp->bootfile;
				e = p + sizeof(dhcp->bootfile);
			} else if (overl & 2) {
				/* bit 2 set means parse server name */
				overl = (uint8_t)(overl & ~2);
				p = dhcp->servername;
				e = p + sizeof(dhcp->servername);
			} else
				goto exit;
			/* No length to read */
			continue;
		}

		/* Check we can read the length */
		if (p == e) {
			errno = EINVAL;
			return NULL;
		}
		l = *p++;

		if (o == DHO_OPTIONSOVERLOADED) {
			/* Ensure we only get this option once by setting
			 * the last bit as well as the value.
			 * This is valid because only the first two bits
			 * actually mean anything in RFC2132 Section 9.3 */
			if (l == 1 && !overl)
				overl = 0x80 | p[0];
		}

		if (o == opt) {
			if (op) {
				if (!ctx->opt_buffer) {
					ctx->opt_buffer =
					    malloc(DHCP_OPTION_LEN +
					    BOOTFILE_LEN + SERVERNAME_LEN);
					if (ctx->opt_buffer == NULL)
						return NULL;
				}
				if (!bp)
					bp = ctx->opt_buffer;
				memcpy(bp, op, ol);
				bp += ol;
			}
			ol = l;
			if (p + ol >= e) {
				errno = EINVAL;
				return NULL;
			}
			op = p;
			bl += ol;
		}
		p += l;
	}

exit:
	if (len)
		*len = bl;
	if (bp) {
		memcpy(bp, op, ol);
		return (const uint8_t *)ctx->opt_buffer;
	}
	if (op)
		return op;
	errno = ENOENT;
	return NULL;
}

int
get_option_addr(struct dhcpcd_ctx *ctx,
    struct in_addr *a, const struct dhcp_message *dhcp,
    uint8_t option)
{
	const uint8_t *p;
	size_t len;

	p = get_option(ctx, dhcp, option, &len);
	if (!p || len < (ssize_t)sizeof(a->s_addr))
		return -1;
	memcpy(&a->s_addr, p, sizeof(a->s_addr));
	return 0;
}

static int
get_option_uint32(struct dhcpcd_ctx *ctx,
    uint32_t *i, const struct dhcp_message *dhcp, uint8_t option)
{
	const uint8_t *p;
	size_t len;
	uint32_t d;

	p = get_option(ctx, dhcp, option, &len);
	if (!p || len < (ssize_t)sizeof(d))
		return -1;
	memcpy(&d, p, sizeof(d));
	if (i)
		*i = ntohl(d);
	return 0;
}

static int
get_option_uint16(struct dhcpcd_ctx *ctx,
    uint16_t *i, const struct dhcp_message *dhcp, uint8_t option)
{
	const uint8_t *p;
	size_t len;
	uint16_t d;

	p = get_option(ctx, dhcp, option, &len);
	if (!p || len < (ssize_t)sizeof(d))
		return -1;
	memcpy(&d, p, sizeof(d));
	if (i)
		*i = ntohs(d);
	return 0;
}

static int
get_option_uint8(struct dhcpcd_ctx *ctx,
    uint8_t *i, const struct dhcp_message *dhcp, uint8_t option)
{
	const uint8_t *p;
	size_t len;

	p = get_option(ctx, dhcp, option, &len);
	if (!p || len < (ssize_t)sizeof(*p))
		return -1;
	if (i)
		*i = *(p);
	return 0;
}

ssize_t
decode_rfc3442(char *out, size_t len, const uint8_t *p, size_t pl)
{
	const uint8_t *e;
	size_t bytes = 0, ocets;
	int b;
	uint8_t cidr;
	struct in_addr addr;
	char *o = out;

	/* Minimum is 5 -first is CIDR and a router length of 4 */
	if (pl < 5) {
		errno = EINVAL;
		return -1;
	}

	e = p + pl;
	while (p < e) {
		cidr = *p++;
		if (cidr > 32) {
			errno = EINVAL;
			return -1;
		}
		ocets = (size_t)(cidr + 7) / NBBY;
		if (p + 4 + ocets > e) {
			errno = ERANGE;
			return -1;
		}
		if (!out) {
			p += 4 + ocets;
			bytes += ((4 * 4) * 2) + 4;
			continue;
		}
		if ((((4 * 4) * 2) + 4) > len) {
			errno = ENOBUFS;
			return -1;
		}
		if (o != out) {
			*o++ = ' ';
			len--;
		}
		/* If we have ocets then we have a destination and netmask */
		if (ocets > 0) {
			addr.s_addr = 0;
			memcpy(&addr.s_addr, p, ocets);
			b = snprintf(o, len, "%s/%d", inet_ntoa(addr), cidr);
			p += ocets;
		} else
			b = snprintf(o, len, "0.0.0.0/0");
		o += b;
		len -= (size_t)b;

		/* Finally, snag the router */
		memcpy(&addr.s_addr, p, 4);
		p += 4;
		b = snprintf(o, len, " %s", inet_ntoa(addr));
		o += b;
		len -= (size_t)b;
	}

	if (out)
		return o - out;
	return (ssize_t)bytes;
}

static struct rt_head *
decode_rfc3442_rt(struct dhcpcd_ctx *ctx, const uint8_t *data, size_t dl)
{
	const uint8_t *p = data;
	const uint8_t *e;
	uint8_t cidr;
	size_t ocets;
	struct rt_head *routes;
	struct rt *rt = NULL;

	/* Minimum is 5 -first is CIDR and a router length of 4 */
	if (dl < 5)
		return NULL;

	routes = malloc(sizeof(*routes));
	TAILQ_INIT(routes);
	e = p + dl;
	while (p < e) {
		cidr = *p++;
		if (cidr > 32) {
			ipv4_freeroutes(routes);
			errno = EINVAL;
			return NULL;
		}

		ocets = (size_t)(cidr + 7) / NBBY;
		if (p + 4 + ocets > e) {
			ipv4_freeroutes(routes);
			errno = ERANGE;
			return NULL;
		}

		rt = calloc(1, sizeof(*rt));
		if (rt == NULL) {
			logger(ctx, LOG_ERR, "%s: %m", __func__);
			ipv4_freeroutes(routes);
			return NULL;
		}
		TAILQ_INSERT_TAIL(routes, rt, next);

		/* If we have ocets then we have a destination and netmask */
		if (ocets > 0) {
			memcpy(&rt->dest.s_addr, p, ocets);
			p += ocets;
			rt->net.s_addr = htonl(~0U << (32 - cidr));
		}

		/* Finally, snag the router */
		memcpy(&rt->gate.s_addr, p, 4);
		p += 4;
	}
	return routes;
}

char *
decode_rfc3361(const uint8_t *data, size_t dl)
{
	uint8_t enc;
	size_t l;
	ssize_t r;
	char *sip = NULL;
	struct in_addr addr;
	char *p;

	if (dl < 2) {
		errno = EINVAL;
		return 0;
	}

	enc = *data++;
	dl--;
	switch (enc) {
	case 0:
		if ((r = decode_rfc1035(NULL, 0, data, dl)) > 0) {
			l = (size_t)r;
			sip = malloc(l);
			if (sip == NULL)
				return 0;
			decode_rfc1035(sip, l, data, dl);
		}
		break;
	case 1:
		if (dl == 0 || dl % 4 != 0) {
			errno = EINVAL;
			break;
		}
		addr.s_addr = INADDR_BROADCAST;
		l = ((dl / sizeof(addr.s_addr)) * ((4 * 4) + 1)) + 1;
		sip = p = malloc(l);
		if (sip == NULL)
			return 0;
		while (dl != 0) {
			memcpy(&addr.s_addr, data, sizeof(addr.s_addr));
			data += sizeof(addr.s_addr);
			p += snprintf(p, l - (size_t)(p - sip),
			    "%s ", inet_ntoa(addr));
			dl -= sizeof(addr.s_addr);
		}
		*--p = '\0';
		break;
	default:
		errno = EINVAL;
		return 0;
	}

	return sip;
}

/* Decode an RFC5969 6rd order option into a space
 * separated string. Returns length of string (including
 * terminating zero) or zero on error. */
ssize_t
decode_rfc5969(char *out, size_t len, const uint8_t *p, size_t pl)
{
	uint8_t ipv4masklen, ipv6prefixlen;
	uint8_t ipv6prefix[16];
	uint8_t br[4];
	int i;
	ssize_t b, bytes = 0;

	if (pl < 22) {
		errno = EINVAL;
		return 0;
	}

	ipv4masklen = *p++;
	pl--;
	ipv6prefixlen = *p++;
	pl--;

	for (i = 0; i < 16; i++) {
		ipv6prefix[i] = *p++;
		pl--;
	}
	if (out) {
		b= snprintf(out, len,
		    "%d %d "
		    "%02x%02x:%02x%02x:"
		    "%02x%02x:%02x%02x:"
		    "%02x%02x:%02x%02x:"
		    "%02x%02x:%02x%02x",
		    ipv4masklen, ipv6prefixlen,
		    ipv6prefix[0], ipv6prefix[1], ipv6prefix[2], ipv6prefix[3],
		    ipv6prefix[4], ipv6prefix[5], ipv6prefix[6], ipv6prefix[7],
		    ipv6prefix[8], ipv6prefix[9], ipv6prefix[10],ipv6prefix[11],
		    ipv6prefix[12],ipv6prefix[13],ipv6prefix[14], ipv6prefix[15]
		);

		len -= (size_t)b;
		out += b;
		bytes += b;
	} else {
		bytes += 16 * 2 + 8 + 2 + 1 + 2;
	}

	while (pl >= 4) {
		br[0] = *p++;
		br[1] = *p++;
		br[2] = *p++;
		br[3] = *p++;
		pl -= 4;

		if (out) {
			b= snprintf(out, len, " %d.%d.%d.%d",
			    br[0], br[1], br[2], br[3]);
			len -= (size_t)b;
			out += b;
			bytes += b;
		} else {
			bytes += (4 * 4);
		}
	}

	return bytes;
}

static char *
get_option_string(struct dhcpcd_ctx *ctx,
    const struct dhcp_message *dhcp, uint8_t option)
{
	size_t len;
	const uint8_t *p;
	char *s;

	p = get_option(ctx, dhcp, option, &len);
	if (!p || len == 0 || *p == '\0')
		return NULL;

	s = malloc(sizeof(char) * (len + 1));
	if (s) {
		memcpy(s, p, len);
		s[len] = '\0';
	}
	return s;
}

/* This calculates the netmask that we should use for static routes.
 * This IS different from the calculation used to calculate the netmask
 * for an interface address. */
static uint32_t
route_netmask(uint32_t ip_in)
{
	/* used to be unsigned long - check if error */
	uint32_t p = ntohl(ip_in);
	uint32_t t;

	if (IN_CLASSA(p))
		t = ~IN_CLASSA_NET;
	else {
		if (IN_CLASSB(p))
			t = ~IN_CLASSB_NET;
		else {
			if (IN_CLASSC(p))
				t = ~IN_CLASSC_NET;
			else
				t = 0;
		}
	}

	while (t & p)
		t >>= 1;

	return (htonl(~t));
}

/* We need to obey routing options.
 * If we have a CSR then we only use that.
 * Otherwise we add static routes and then routers. */
static struct rt_head *
get_option_routes(struct interface *ifp, const struct dhcp_message *dhcp)
{
	struct if_options *ifo = ifp->options;
	const uint8_t *p;
	const uint8_t *e;
	struct rt_head *routes = NULL;
	struct rt *route = NULL;
	size_t len;
	const char *csr = "";

	/* If we have CSR's then we MUST use these only */
	if (!has_option_mask(ifo->nomask, DHO_CSR))
		p = get_option(ifp->ctx, dhcp, DHO_CSR, &len);
	else
		p = NULL;
	/* Check for crappy MS option */
	if (!p && !has_option_mask(ifo->nomask, DHO_MSCSR)) {
		p = get_option(ifp->ctx, dhcp, DHO_MSCSR, &len);
		if (p)
			csr = "MS ";
	}
	if (p) {
		routes = decode_rfc3442_rt(ifp->ctx, p, len);
		if (routes) {
			const struct dhcp_state *state;

			state = D_CSTATE(ifp);
			if (!(ifo->options & DHCPCD_CSR_WARNED) &&
			    !(state->added & STATE_FAKE))
			{
				logger(ifp->ctx, LOG_DEBUG,
				    "%s: using %sClassless Static Routes",
				    ifp->name, csr);
				ifo->options |= DHCPCD_CSR_WARNED;
			}
			return routes;
		}
	}

	/* OK, get our static routes first. */
	routes = malloc(sizeof(*routes));
	if (routes == NULL) {
		logger(ifp->ctx, LOG_ERR, "%s: %m", __func__);
		return NULL;
	}
	TAILQ_INIT(routes);
	if (!has_option_mask(ifo->nomask, DHO_STATICROUTE))
		p = get_option(ifp->ctx, dhcp, DHO_STATICROUTE, &len);
	else
		p = NULL;
	/* RFC 2131 Section 5.8 states length MUST be in multiples of 8 */
	if (p && len % 8 == 0) {
		e = p + len;
		while (p < e) {
			if ((route = calloc(1, sizeof(*route))) == NULL) {
				logger(ifp->ctx, LOG_ERR, "%s: %m", __func__);
				ipv4_freeroutes(routes);
				return NULL;
			}
			memcpy(&route->dest.s_addr, p, 4);
			p += 4;
			memcpy(&route->gate.s_addr, p, 4);
			p += 4;
			/* RFC 2131 Section 5.8 states default route is
			 * illegal */
			if (route->dest.s_addr == htonl(INADDR_ANY)) {
				errno = EINVAL;
				free(route);
				continue;
			}
			/* A host route is normally set by having the
			 * gateway match the destination or assigned address */
			if (route->gate.s_addr == route->dest.s_addr ||
			    route->gate.s_addr == dhcp->yiaddr)
			{
				route->gate.s_addr = htonl(INADDR_ANY);
				route->net.s_addr = htonl(INADDR_BROADCAST);
			}
			route->net.s_addr = route_netmask(route->dest.s_addr);
			TAILQ_INSERT_TAIL(routes, route, next);
		}
	}

	/* Now grab our routers */
	if (!has_option_mask(ifo->nomask, DHO_ROUTER))
		p = get_option(ifp->ctx, dhcp, DHO_ROUTER, &len);
	else
		p = NULL;
	if (p) {
		e = p + len;
		while (p < e) {
			if ((route = calloc(1, sizeof(*route))) == NULL) {
				logger(ifp->ctx, LOG_ERR, "%s: %m", __func__);
				ipv4_freeroutes(routes);
				return NULL;
			}
			memcpy(&route->gate.s_addr, p, 4);
			p += 4;
			TAILQ_INSERT_TAIL(routes, route, next);
		}
	}

	return routes;
}

uint16_t
dhcp_get_mtu(const struct interface *ifp)
{
	const struct dhcp_message *dhcp;
	uint16_t mtu;

	if (ifp->options->mtu)
		return (uint16_t)ifp->options->mtu;
	mtu = 0; /* bogus gcc warning */
	if ((dhcp = D_CSTATE(ifp)->new) == NULL ||
	    has_option_mask(ifp->options->nomask, DHO_MTU) ||
	    get_option_uint16(ifp->ctx, &mtu, dhcp, DHO_MTU) == -1)
		return 0;
	return mtu;
}

/* Grab our routers from the DHCP message and apply any MTU value
 * the message contains */
struct rt_head *
dhcp_get_routes(struct interface *ifp)
{
	struct rt_head *routes;
	uint16_t mtu;
	const struct dhcp_message *dhcp;

	dhcp = D_CSTATE(ifp)->new;
	routes = get_option_routes(ifp, dhcp);
	if ((mtu = dhcp_get_mtu(ifp)) != 0) {
		struct rt *rt;

		TAILQ_FOREACH(rt, routes, next) {
			rt->mtu = mtu;
		}
	}
	return routes;
}

#define PUTADDR(_type, _val)						      \
	{								      \
		*p++ = _type;						      \
		*p++ = 4;						      \
		memcpy(p, &_val.s_addr, 4);				      \
		p += 4;							      \
	}

int
dhcp_message_add_addr(struct dhcp_message *dhcp,
    uint8_t type, struct in_addr addr)
{
	uint8_t *p;
	size_t len;

	p = dhcp->options;
	while (*p != DHO_END) {
		p++;
		p += *p + 1;
	}

	len = (size_t)(p - (uint8_t *)dhcp);
	if (len + 6 > sizeof(*dhcp)) {
		errno = ENOMEM;
		return -1;
	}

	PUTADDR(type, addr);
	*p = DHO_END;
	return 0;
}

ssize_t
make_message(struct dhcp_message **message,
    const struct interface *ifp,
    uint8_t type)
{
	struct dhcp_message *dhcp;
	uint8_t *m, *lp, *p, *auth;
	uint8_t *n_params = NULL, auth_len;
	uint32_t ul;
	uint16_t sz;
	size_t len, i;
	const struct dhcp_opt *opt;
	struct if_options *ifo = ifp->options;
	const struct dhcp_state *state = D_CSTATE(ifp);
	const struct dhcp_lease *lease = &state->lease;
	char hbuf[HOSTNAME_MAX_LEN + 1];
	const char *hostname;
	const struct vivco *vivco;

	dhcp = calloc(1, sizeof (*dhcp));
	if (dhcp == NULL)
		return -1;
	m = (uint8_t *)dhcp;
	p = dhcp->options;

	if ((type == DHCP_INFORM || type == DHCP_RELEASE ||
		(type == DHCP_REQUEST &&
		    state->net.s_addr == lease->net.s_addr &&
		    (state->new == NULL ||
			state->new->cookie == htonl(MAGIC_COOKIE)))))
	{
		dhcp->ciaddr = state->addr.s_addr;
		/* In-case we haven't actually configured the address yet */
		if (type == DHCP_INFORM && state->addr.s_addr == 0)
			dhcp->ciaddr = lease->addr.s_addr;
	}

	dhcp->op = DHCP_BOOTREQUEST;
	dhcp->hwtype = (uint8_t)ifp->family;
	switch (ifp->family) {
	case ARPHRD_ETHER:
	case ARPHRD_IEEE802:
		dhcp->hwlen = (uint8_t)ifp->hwlen;
		memcpy(&dhcp->chaddr, &ifp->hwaddr, ifp->hwlen);
		break;
	}

	if (ifo->options & DHCPCD_BROADCAST &&
	    dhcp->ciaddr == 0 &&
	    type != DHCP_DECLINE &&
	    type != DHCP_RELEASE)
		dhcp->flags = htons(BROADCAST_FLAG);

	if (type != DHCP_DECLINE && type != DHCP_RELEASE) {
		struct timespec tv;

		clock_gettime(CLOCK_MONOTONIC, &tv);
		timespecsub(&tv, &state->started, &tv);
		if (tv.tv_sec < 0 || tv.tv_sec > (time_t)UINT16_MAX)
			dhcp->secs = htons((uint16_t)UINT16_MAX);
		else
			dhcp->secs = htons((uint16_t)tv.tv_sec);
	}
	dhcp->xid = htonl(state->xid);
	dhcp->cookie = htonl(MAGIC_COOKIE);

	if (!(ifo->options & DHCPCD_BOOTP)) {
		*p++ = DHO_MESSAGETYPE;
		*p++ = 1;
		*p++ = type;
	}

	if (state->clientid) {
		*p++ = DHO_CLIENTID;
		memcpy(p, state->clientid, (size_t)state->clientid[0] + 1);
		p += state->clientid[0] + 1;
	}

	if (lease->addr.s_addr && lease->cookie == htonl(MAGIC_COOKIE)) {
		if (type == DHCP_DECLINE ||
		    (type == DHCP_REQUEST &&
			lease->addr.s_addr != state->addr.s_addr))
		{
			PUTADDR(DHO_IPADDRESS, lease->addr);
			if (lease->server.s_addr)
				PUTADDR(DHO_SERVERID, lease->server);
		}

		if (type == DHCP_RELEASE) {
			if (lease->server.s_addr)
				PUTADDR(DHO_SERVERID, lease->server);
		}
	}

	if (type == DHCP_DECLINE) {
		*p++ = DHO_MESSAGE;
		len = strlen(DAD);
		*p++ = (uint8_t)len;
		memcpy(p, DAD, len);
		p += len;
	}

	if (type == DHCP_DISCOVER &&
	    !(ifp->ctx->options & DHCPCD_TEST) &&
	    has_option_mask(ifo->requestmask, DHO_RAPIDCOMMIT))
	{
		/* RFC 4039 Section 3 */
		*p++ = DHO_RAPIDCOMMIT;
		*p++ = 0;
	}

	if (type == DHCP_DISCOVER && ifo->options & DHCPCD_REQUEST)
		PUTADDR(DHO_IPADDRESS, ifo->req_addr);

	/* RFC 2563 Auto Configure */
	if (type == DHCP_DISCOVER && ifo->options & DHCPCD_IPV4LL) {
		*p++ = DHO_AUTOCONFIGURE;
		*p++ = 1;
		*p++ = 1;
	}

	if (type == DHCP_DISCOVER ||
	    type == DHCP_INFORM ||
	    type == DHCP_REQUEST)
	{
		if (!(ifo->options & DHCPCD_BOOTP)) {
			int mtu;

			if ((mtu = if_getmtu(ifp)) == -1)
				logger(ifp->ctx, LOG_ERR,
				    "%s: if_getmtu: %m", ifp->name);
			else if (mtu < MTU_MIN) {
				if (if_setmtu(ifp, MTU_MIN) == -1)
					logger(ifp->ctx, LOG_ERR,
					    "%s: if_setmtu: %m", ifp->name);
				mtu = MTU_MIN;
			} else if (mtu > MTU_MAX) {
				/* Even though our MTU could be greater than
				 * MTU_MAX (1500) dhcpcd does not presently
				 * handle DHCP packets any bigger. */
				mtu = MTU_MAX;
			}
			if (mtu != -1) {
				*p++ = DHO_MAXMESSAGESIZE;
				*p++ = 2;
				sz = htons((uint16_t)mtu);
				memcpy(p, &sz, 2);
				p += 2;
			}
		}

		if (ifo->userclass[0]) {
			*p++ = DHO_USERCLASS;
			memcpy(p, ifo->userclass,
			    (size_t)ifo->userclass[0] + 1);
			p += ifo->userclass[0] + 1;
		}

		if (ifo->vendorclassid[0]) {
			*p++ = DHO_VENDORCLASSID;
			memcpy(p, ifo->vendorclassid,
			    (size_t)ifo->vendorclassid[0] + 1);
			p += ifo->vendorclassid[0] + 1;
		}

		if (type != DHCP_INFORM) {
			if (ifo->leasetime != 0) {
				*p++ = DHO_LEASETIME;
				*p++ = 4;
				ul = htonl(ifo->leasetime);
				memcpy(p, &ul, 4);
				p += 4;
			}
		}

		if (ifo->hostname[0] == '\0')
			hostname = get_hostname(hbuf, sizeof(hbuf),
			    ifo->options & DHCPCD_HOSTNAME_SHORT ? 1 : 0);
		else
			hostname = ifo->hostname;

		/*
		 * RFC4702 3.1 States that if we send the Client FQDN option
		 * then we MUST NOT also send the Host Name option.
		 * Technically we could, but that is not RFC conformant and
		 * also seems to break some DHCP server implemetations such as
		 * Windows. On the other hand, ISC dhcpd is just as non RFC
		 * conformant by not accepting a partially qualified FQDN.
		 */
		if (ifo->fqdn != FQDN_DISABLE) {
			/* IETF DHC-FQDN option (81), RFC4702 */
			*p++ = DHO_FQDN;
			lp = p;
			*p++ = 3;
			/*
			 * Flags: 0000NEOS
			 * S: 1 => Client requests Server to update
			 *         a RR in DNS as well as PTR
			 * O: 1 => Server indicates to client that
			 *         DNS has been updated
			 * E: 1 => Name data is DNS format
			 * N: 1 => Client requests Server to not
			 *         update DNS
			 */
			if (hostname)
				*p++ = (uint8_t)((ifo->fqdn & 0x09) | 0x04);
			else
				*p++ = (FQDN_NONE & 0x09) | 0x04;
			*p++ = 0; /* from server for PTR RR */
			*p++ = 0; /* from server for A RR if S=1 */
			if (hostname) {
				i = encode_rfc1035(hostname, p);
				*lp = (uint8_t)(*lp + i);
				p += i;
			}
		} else if (ifo->options & DHCPCD_HOSTNAME && hostname) {
			*p++ = DHO_HOSTNAME;
			len = strlen(hostname);
			*p++ = (uint8_t)len;
			memcpy(p, hostname, len);
			p += len;
		}

		/* vendor is already encoded correctly, so just add it */
		if (ifo->vendor[0]) {
			*p++ = DHO_VENDOR;
			memcpy(p, ifo->vendor, (size_t)ifo->vendor[0] + 1);
			p += ifo->vendor[0] + 1;
		}

		if ((ifo->auth.options & DHCPCD_AUTH_SENDREQUIRE) !=
		    DHCPCD_AUTH_SENDREQUIRE)
		{
			/* We support HMAC-MD5 */
			*p++ = DHO_FORCERENEW_NONCE;
			*p++ = 1;
			*p++ = AUTH_ALG_HMAC_MD5;
		}

		if (ifo->vivco_len) {
			*p++ = DHO_VIVCO;
			lp = p++;
			*lp = sizeof(ul);
			ul = htonl(ifo->vivco_en);
			memcpy(p, &ul, sizeof(ul));
			p += sizeof(ul);
			for (i = 0, vivco = ifo->vivco;
			    i < ifo->vivco_len;
			    i++, vivco++)
			{
				len = (size_t)(p - m) + vivco->len + 1;
				if (len > sizeof(*dhcp))
					goto toobig;
				if (vivco->len + 2 + *lp > 255) {
					logger(ifp->ctx, LOG_ERR,
					    "%s: VIVCO option too big",
					    ifp->name);
					free(dhcp);
					return -1;
				}
				*p++ = (uint8_t)vivco->len;
				memcpy(p, vivco->data, vivco->len);
				p += vivco->len;
				*lp = (uint8_t)(*lp + vivco->len + 1);
			}
		}

		len = (size_t)((p - m) + 3);
		if (len > sizeof(*dhcp))
			goto toobig;
		*p++ = DHO_PARAMETERREQUESTLIST;
		n_params = p;
		*p++ = 0;
		for (i = 0, opt = ifp->ctx->dhcp_opts;
		    i < ifp->ctx->dhcp_opts_len;
		    i++, opt++)
		{
			if (!(opt->type & REQUEST ||
			    has_option_mask(ifo->requestmask, opt->option)))
				continue;
			if (opt->type & NOREQ)
				continue;
			if (type == DHCP_INFORM &&
			    (opt->option == DHO_RENEWALTIME ||
				opt->option == DHO_REBINDTIME))
				continue;
			len = (size_t)((p - m) + 2);
			if (len > sizeof(*dhcp))
				goto toobig;
			*p++ = (uint8_t)opt->option;
		}
		for (i = 0, opt = ifo->dhcp_override;
		    i < ifo->dhcp_override_len;
		    i++, opt++)
		{
			/* Check if added above */
			for (lp = n_params + 1; lp < p; lp++)
				if (*lp == (uint8_t)opt->option)
					break;
			if (lp < p)
				continue;
			if (!(opt->type & REQUEST ||
			    has_option_mask(ifo->requestmask, opt->option)))
				continue;
			if (opt->type & NOREQ)
				continue;
			if (type == DHCP_INFORM &&
			    (opt->option == DHO_RENEWALTIME ||
				opt->option == DHO_REBINDTIME))
				continue;
			len = (size_t)((p - m) + 2);
			if (len > sizeof(*dhcp))
				goto toobig;
			*p++ = (uint8_t)opt->option;
		}
		*n_params = (uint8_t)(p - n_params - 1);
	}

	/* silence GCC */
	auth_len = 0;
	auth = NULL;

	if (ifo->auth.options & DHCPCD_AUTH_SEND) {
		ssize_t alen = dhcp_auth_encode(&ifo->auth,
		    state->auth.token,
		    NULL, 0, 4, type, NULL, 0);
		if (alen != -1 && alen > UINT8_MAX) {
			errno = ERANGE;
			alen = -1;
		}
		if (alen == -1)
			logger(ifp->ctx, LOG_ERR,
			    "%s: dhcp_auth_encode: %m", ifp->name);
		else if (alen != 0) {
			auth_len = (uint8_t)alen;
			len = (size_t)((p + alen) - m);
			if (len > sizeof(*dhcp))
				goto toobig;
			*p++ = DHO_AUTHENTICATION;
			*p++ = auth_len;
			auth = p;
			p += auth_len;
		}
	}

	*p++ = DHO_END;

	/* Pad out to the BOOTP minimum message length.
	 * Some DHCP servers incorrectly require this. */
	while (p - m < BOOTP_MESSAGE_LENTH_MIN)
		*p++ = DHO_PAD;

	len = (size_t)(p - m);
	if (ifo->auth.options & DHCPCD_AUTH_SEND && auth_len != 0)
		dhcp_auth_encode(&ifo->auth, state->auth.token,
		    m, len, 4, type, auth, auth_len);

	*message = dhcp;
	return (ssize_t)len;

toobig:
	logger(ifp->ctx, LOG_ERR, "%s: DHCP messge too big", ifp->name);
	free(dhcp);
	return -1;
}

static ssize_t
write_lease(const struct interface *ifp, const struct dhcp_message *dhcp)
{
	int fd;
	size_t len;
	ssize_t bytes;
	const uint8_t *e, *p;
	uint8_t l;
	uint8_t o = 0;
	const struct dhcp_state *state = D_CSTATE(ifp);

	/* We don't write BOOTP leases */
	if (IS_BOOTP(ifp, dhcp)) {
		unlink(state->leasefile);
		return 0;
	}

	logger(ifp->ctx, LOG_DEBUG, "%s: writing lease `%s'",
	    ifp->name, state->leasefile);

	fd = open(state->leasefile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd == -1)
		return -1;

	/* Only write as much as we need */
	p = dhcp->options;
	e = p + sizeof(dhcp->options);
	len = sizeof(*dhcp);
	while (p < e) {
		o = *p;
		if (o == DHO_END) {
			len = (size_t)(p - (const uint8_t *)dhcp);
			break;
		}
		p++;
		if (o != DHO_PAD) {
			l = *p++;
			p += l;
		}
	}
	bytes = write(fd, dhcp, len);
	close(fd);
	return bytes;
}

static struct dhcp_message *
read_lease(struct interface *ifp)
{
	int fd;
	struct dhcp_message *dhcp;
	struct dhcp_state *state = D_STATE(ifp);
	ssize_t bytes;
	const uint8_t *auth;
	uint8_t type;
	size_t auth_len;

	fd = open(state->leasefile, O_RDONLY);
	if (fd == -1) {
		if (errno != ENOENT)
			logger(ifp->ctx, LOG_ERR, "%s: open `%s': %m",
			    ifp->name, state->leasefile);
		return NULL;
	}
	logger(ifp->ctx, LOG_DEBUG, "%s: reading lease `%s'",
	    ifp->name, state->leasefile);
	dhcp = calloc(1, sizeof(*dhcp));
	if (dhcp == NULL) {
		close(fd);
		return NULL;
	}
	bytes = read(fd, dhcp, sizeof(*dhcp));
	close(fd);
	if (bytes < 0) {
		free(dhcp);
		return NULL;
	}

	/* We may have found a BOOTP server */
	if (get_option_uint8(ifp->ctx, &type, dhcp, DHO_MESSAGETYPE) == -1)
		type = 0;

	/* Authenticate the message */
	auth = get_option(ifp->ctx, dhcp, DHO_AUTHENTICATION, &auth_len);
	if (auth) {
		if (dhcp_auth_validate(&state->auth, &ifp->options->auth,
		    (uint8_t *)dhcp, sizeof(*dhcp), 4, type,
		    auth, auth_len) == NULL)
		{
			logger(ifp->ctx, LOG_DEBUG,
			    "%s: dhcp_auth_validate: %m", ifp->name);
			free(dhcp);
			return NULL;
		}
		if (state->auth.token)
			logger(ifp->ctx, LOG_DEBUG,
			    "%s: validated using 0x%08" PRIu32,
			    ifp->name, state->auth.token->secretid);
		else
			logger(ifp->ctx, LOG_DEBUG,
			    "%s: accepted reconfigure key", ifp->name);
	} else if ((ifp->options->auth.options & DHCPCD_AUTH_SENDREQUIRE) ==
	    DHCPCD_AUTH_SENDREQUIRE)
	{
		logger(ifp->ctx, LOG_ERR,
		    "%s: authentication now required", ifp->name);
		free(dhcp);
		return NULL;
	}

	return dhcp;
}

static const struct dhcp_opt *
dhcp_getoverride(const struct if_options *ifo, unsigned int o)
{
	size_t i;
	const struct dhcp_opt *opt;

	for (i = 0, opt = ifo->dhcp_override;
	    i < ifo->dhcp_override_len;
	    i++, opt++)
	{
		if (opt->option == o)
			return opt;
	}
	return NULL;
}

static const uint8_t *
dhcp_getoption(struct dhcpcd_ctx *ctx,
    size_t *os, unsigned int *code, size_t *len,
    const uint8_t *od, size_t ol, struct dhcp_opt **oopt)
{
	size_t i;
	struct dhcp_opt *opt;

	if (od) {
		if (ol < 2) {
			errno = EINVAL;
			return NULL;
		}
		*os = 2; /* code + len */
		*code = (unsigned int)*od++;
		*len = (size_t)*od++;
		if (*len > ol) {
			errno = EINVAL;
			return NULL;
		}
	}

	for (i = 0, opt = ctx->dhcp_opts; i < ctx->dhcp_opts_len; i++, opt++) {
		if (opt->option == *code) {
			*oopt = opt;
			break;
		}
	}

	return od;
}

ssize_t
dhcp_env(char **env, const char *prefix, const struct dhcp_message *dhcp,
    const struct interface *ifp)
{
	const struct if_options *ifo;
	const uint8_t *p;
	struct in_addr addr;
	struct in_addr net;
	struct in_addr brd;
	struct dhcp_opt *opt, *vo;
	size_t e, i, pl;
	char **ep;
	char cidr[4], safe[(BOOTFILE_LEN * 4) + 1];
	uint8_t overl = 0;
	uint32_t en;

	e = 0;
	ifo = ifp->options;
	get_option_uint8(ifp->ctx, &overl, dhcp, DHO_OPTIONSOVERLOADED);

	if (env == NULL) {
		if (dhcp->yiaddr || dhcp->ciaddr)
			e += 5;
		if (*dhcp->bootfile && !(overl & 1))
			e++;
		if (*dhcp->servername && !(overl & 2))
			e++;
		for (i = 0, opt = ifp->ctx->dhcp_opts;
		    i < ifp->ctx->dhcp_opts_len;
		    i++, opt++)
		{
			if (has_option_mask(ifo->nomask, opt->option))
				continue;
			if (dhcp_getoverride(ifo, opt->option))
				continue;
			p = get_option(ifp->ctx, dhcp, opt->option, &pl);
			if (!p)
				continue;
			e += dhcp_envoption(ifp->ctx, NULL, NULL, ifp->name,
			    opt, dhcp_getoption, p, pl);
		}
		for (i = 0, opt = ifo->dhcp_override;
		    i < ifo->dhcp_override_len;
		    i++, opt++)
		{
			if (has_option_mask(ifo->nomask, opt->option))
				continue;
			p = get_option(ifp->ctx, dhcp, opt->option, &pl);
			if (!p)
				continue;
			e += dhcp_envoption(ifp->ctx, NULL, NULL, ifp->name,
			    opt, dhcp_getoption, p, pl);
		}
		return (ssize_t)e;
	}

	ep = env;
	if (dhcp->yiaddr || dhcp->ciaddr) {
		/* Set some useful variables that we derive from the DHCP
		 * message but are not necessarily in the options */
		addr.s_addr = dhcp->yiaddr ? dhcp->yiaddr : dhcp->ciaddr;
		addvar(ifp->ctx, &ep, prefix, "ip_address", inet_ntoa(addr));
		if (get_option_addr(ifp->ctx, &net,
		    dhcp, DHO_SUBNETMASK) == -1)
		{
			net.s_addr = ipv4_getnetmask(addr.s_addr);
			addvar(ifp->ctx, &ep, prefix,
			    "subnet_mask", inet_ntoa(net));
		}
		snprintf(cidr, sizeof(cidr), "%d", inet_ntocidr(net));
		addvar(ifp->ctx, &ep, prefix, "subnet_cidr", cidr);
		if (get_option_addr(ifp->ctx, &brd,
		    dhcp, DHO_BROADCAST) == -1)
		{
			brd.s_addr = addr.s_addr | ~net.s_addr;
			addvar(ifp->ctx, &ep, prefix,
			    "broadcast_address", inet_ntoa(brd));
		}
		addr.s_addr = dhcp->yiaddr & net.s_addr;
		addvar(ifp->ctx, &ep, prefix,
		    "network_number", inet_ntoa(addr));
	}

	if (*dhcp->bootfile && !(overl & 1)) {
		print_string(safe, sizeof(safe), STRING,
		    dhcp->bootfile, sizeof(dhcp->bootfile));
		addvar(ifp->ctx, &ep, prefix, "filename", safe);
	}
	if (*dhcp->servername && !(overl & 2)) {
		print_string(safe, sizeof(safe), STRING | DOMAIN,
		    dhcp->servername, sizeof(dhcp->servername));
		addvar(ifp->ctx, &ep, prefix, "server_name", safe);
	}

	/* Zero our indexes */
	if (env) {
		for (i = 0, opt = ifp->ctx->dhcp_opts;
		    i < ifp->ctx->dhcp_opts_len;
		    i++, opt++)
			dhcp_zero_index(opt);
		for (i = 0, opt = ifp->options->dhcp_override;
		    i < ifp->options->dhcp_override_len;
		    i++, opt++)
			dhcp_zero_index(opt);
		for (i = 0, opt = ifp->ctx->vivso;
		    i < ifp->ctx->vivso_len;
		    i++, opt++)
			dhcp_zero_index(opt);
	}

	for (i = 0, opt = ifp->ctx->dhcp_opts;
	    i < ifp->ctx->dhcp_opts_len;
	    i++, opt++)
	{
		if (has_option_mask(ifo->nomask, opt->option))
			continue;
		if (dhcp_getoverride(ifo, opt->option))
			continue;
		if ((p = get_option(ifp->ctx, dhcp, opt->option, &pl))) {
			ep += dhcp_envoption(ifp->ctx, ep, prefix, ifp->name,
			    opt, dhcp_getoption, p, pl);
			if (opt->option == DHO_VIVSO &&
			    pl > (int)sizeof(uint32_t))
			{
			        memcpy(&en, p, sizeof(en));
				en = ntohl(en);
				vo = vivso_find(en, ifp);
				if (vo) {
					/* Skip over en + total size */
					p += sizeof(en) + 1;
					pl -= sizeof(en) + 1;
					ep += dhcp_envoption(ifp->ctx,
					    ep, prefix, ifp->name,
					    vo, dhcp_getoption, p, pl);
				}
			}
		}
	}

	for (i = 0, opt = ifo->dhcp_override;
	    i < ifo->dhcp_override_len;
	    i++, opt++)
	{
		if (has_option_mask(ifo->nomask, opt->option))
			continue;
		if ((p = get_option(ifp->ctx, dhcp, opt->option, &pl)))
			ep += dhcp_envoption(ifp->ctx, ep, prefix, ifp->name,
			    opt, dhcp_getoption, p, pl);
	}

	return ep - env;
}

static void
get_lease(struct dhcpcd_ctx *ctx,
    struct dhcp_lease *lease, const struct dhcp_message *dhcp)
{

	lease->cookie = dhcp->cookie;
	/* BOOTP does not set yiaddr for replies when ciaddr is set. */
	if (dhcp->yiaddr)
		lease->addr.s_addr = dhcp->yiaddr;
	else
		lease->addr.s_addr = dhcp->ciaddr;
	if (get_option_addr(ctx, &lease->net, dhcp, DHO_SUBNETMASK) == -1)
		lease->net.s_addr = ipv4_getnetmask(lease->addr.s_addr);
	if (get_option_addr(ctx, &lease->brd, dhcp, DHO_BROADCAST) == -1)
		lease->brd.s_addr = lease->addr.s_addr | ~lease->net.s_addr;
	if (get_option_uint32(ctx, &lease->leasetime, dhcp, DHO_LEASETIME) != 0)
		lease->leasetime = ~0U; /* Default to infinite lease */
	if (get_option_uint32(ctx, &lease->renewaltime,
	    dhcp, DHO_RENEWALTIME) != 0)
		lease->renewaltime = 0;
	if (get_option_uint32(ctx, &lease->rebindtime,
	    dhcp, DHO_REBINDTIME) != 0)
		lease->rebindtime = 0;
	if (get_option_addr(ctx, &lease->server, dhcp, DHO_SERVERID) != 0)
		lease->server.s_addr = INADDR_ANY;
}

static const char *
get_dhcp_op(uint8_t type)
{
	const struct dhcp_op *d;

	for (d = dhcp_ops; d->name; d++)
		if (d->value == type)
			return d->name;
	return NULL;
}

static void
dhcp_fallback(void *arg)
{
	struct interface *iface;

	iface = (struct interface *)arg;
	dhcpcd_selectprofile(iface, iface->options->fallback);
	dhcpcd_startinterface(iface);
}

uint32_t
dhcp_xid(const struct interface *ifp)
{
	uint32_t xid;

	if (ifp->options->options & DHCPCD_XID_HWADDR &&
	    ifp->hwlen >= sizeof(xid))
		/* The lower bits are probably more unique on the network */
		memcpy(&xid, (ifp->hwaddr + ifp->hwlen) - sizeof(xid),
		    sizeof(xid));
	else
		xid = arc4random();

	return xid;
}

void
dhcp_close(struct interface *ifp)
{
	struct dhcp_state *state = D_STATE(ifp);

	if (state == NULL)
		return;

	if (state->raw_fd != -1) {
		eloop_event_delete(ifp->ctx->eloop, state->raw_fd);
		close(state->raw_fd);
		state->raw_fd = -1;
	}

	state->interval = 0;
}

static int
dhcp_openudp(struct interface *ifp)
{
	int s;
	struct sockaddr_in sin;
	int n;
	struct dhcp_state *state;
#ifdef SO_BINDTODEVICE
	struct ifreq ifr;
	char *p;
#endif

	if ((s = xsocket(PF_INET, SOCK_DGRAM, IPPROTO_UDP, O_CLOEXEC)) == -1)
		return -1;

	n = 1;
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &n, sizeof(n)) == -1)
		goto eexit;
#ifdef SO_BINDTODEVICE
	if (ifp) {
		memset(&ifr, 0, sizeof(ifr));
		strlcpy(ifr.ifr_name, ifp->name, sizeof(ifr.ifr_name));
		/* We can only bind to the real device */
		p = strchr(ifr.ifr_name, ':');
		if (p)
			*p = '\0';
		if (setsockopt(s, SOL_SOCKET, SO_BINDTODEVICE, &ifr,
		    sizeof(ifr)) == -1)
		        goto eexit;
	}
#endif
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(DHCP_CLIENT_PORT);
	if (ifp) {
		state = D_STATE(ifp);
		sin.sin_addr.s_addr = state->addr.s_addr;
	} else
		state = NULL; /* appease gcc */
	if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) == -1)
		goto eexit;

	return s;

eexit:
	close(s);
	return -1;
}

static uint16_t
checksum(const void *data, unsigned int len)
{
	const uint8_t *addr = data;
	uint32_t sum = 0;

	while (len > 1) {
		sum += (uint32_t)(addr[0] * 256 + addr[1]);
		addr += 2;
		len -= 2;
	}

	if (len == 1)
		sum += (uint32_t)(*addr * 256);

	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);

	return (uint16_t)~htons((uint16_t)sum);
}

static struct udp_dhcp_packet *
dhcp_makeudppacket(size_t *sz, const uint8_t *data, size_t length,
	struct in_addr source, struct in_addr dest)
{
	struct udp_dhcp_packet *udpp;
	struct ip *ip;
	struct udphdr *udp;

	udpp = calloc(1, sizeof(*udpp));
	if (udpp == NULL)
		return NULL;
	ip = &udpp->ip;
	udp = &udpp->udp;

	/* OK, this is important :)
	 * We copy the data to our packet and then create a small part of the
	 * ip structure and an invalid ip_len (basically udp length).
	 * We then fill the udp structure and put the checksum
	 * of the whole packet into the udp checksum.
	 * Finally we complete the ip structure and ip checksum.
	 * If we don't do the ordering like so then the udp checksum will be
	 * broken, so find another way of doing it! */

	memcpy(&udpp->dhcp, data, length);

	ip->ip_p = IPPROTO_UDP;
	ip->ip_src.s_addr = source.s_addr;
	if (dest.s_addr == 0)
		ip->ip_dst.s_addr = INADDR_BROADCAST;
	else
		ip->ip_dst.s_addr = dest.s_addr;

	udp->uh_sport = htons(DHCP_CLIENT_PORT);
	udp->uh_dport = htons(DHCP_SERVER_PORT);
	udp->uh_ulen = htons((uint16_t)(sizeof(*udp) + length));
	ip->ip_len = udp->uh_ulen;
	udp->uh_sum = checksum(udpp, sizeof(*udpp));

	ip->ip_v = IPVERSION;
	ip->ip_hl = sizeof(*ip) >> 2;
	ip->ip_id = (uint16_t)arc4random_uniform(UINT16_MAX);
	ip->ip_ttl = IPDEFTTL;
	ip->ip_len = htons((uint16_t)(sizeof(*ip) + sizeof(*udp) + length));
	ip->ip_sum = checksum(ip, sizeof(*ip));

	*sz = sizeof(*ip) + sizeof(*udp) + length;
	return udpp;
}

static void
send_message(struct interface *ifp, uint8_t type,
    void (*callback)(void *))
{
	struct dhcp_state *state = D_STATE(ifp);
	struct if_options *ifo = ifp->options;
	struct dhcp_message *dhcp;
	struct udp_dhcp_packet *udp;
	size_t len;
	ssize_t r;
	struct in_addr from, to;
	in_addr_t a = INADDR_ANY;
	struct timespec tv;
	int s;
#ifdef IN_IFF_NOTUSEABLE
	struct ipv4_addr *ia;
#endif

	s = -1;
	if (!callback) {
		/* No carrier? Don't bother sending the packet. */
		if (ifp->carrier == LINK_DOWN)
			return;
		logger(ifp->ctx, LOG_DEBUG, "%s: sending %s with xid 0x%x",
		    ifp->name,
		    ifo->options & DHCPCD_BOOTP ? "BOOTP" : get_dhcp_op(type),
		    state->xid);
	} else {
		if (state->interval == 0)
			state->interval = 4;
		else {
			state->interval *= 2;
			if (state->interval > 64)
				state->interval = 64;
		}
		tv.tv_sec = state->interval + DHCP_RAND_MIN;
		tv.tv_nsec = (suseconds_t)arc4random_uniform(
		    (DHCP_RAND_MAX - DHCP_RAND_MIN) * NSEC_PER_SEC);
		timespecnorm(&tv);
		/* No carrier? Don't bother sending the packet.
		 * However, we do need to advance the timeout. */
		if (ifp->carrier == LINK_DOWN)
			goto fail;
		logger(ifp->ctx, LOG_DEBUG,
		    "%s: sending %s (xid 0x%x), next in %0.1f seconds",
		    ifp->name,
		    ifo->options & DHCPCD_BOOTP ? "BOOTP" : get_dhcp_op(type),
		    state->xid,
		    timespec_to_double(&tv));
	}

	if (dhcp_open(ifp) == -1)
		return;

	if (state->added && !(state->added & STATE_FAKE) &&
	    state->addr.s_addr != INADDR_ANY &&
	    state->new != NULL &&
#ifdef IN_IFF_NOTUSEABLE
	    ((ia = ipv4_iffindaddr(ifp, &state->addr, NULL)) &&
	    !(ia->addr_flags & IN_IFF_NOTUSEABLE)) &&
#endif
	    (state->lease.server.s_addr ||
	    ifp->options->options & DHCPCD_INFORM) &&
	    !IS_BOOTP(ifp, state->new))
	{
		s = dhcp_openudp(ifp);
		if (s == -1) {
			if (errno != EADDRINUSE)
				logger(ifp->ctx, LOG_ERR,
				    "%s: dhcp_openudp: %m", ifp->name);
			/* We cannot renew */
			a = state->addr.s_addr;
			state->addr.s_addr = INADDR_ANY;
		}
	}

	r = make_message(&dhcp, ifp, type);
	if (a != INADDR_ANY)
		state->addr.s_addr = a;
	if (r == -1)
		goto fail;
	len = (size_t)r;
	from.s_addr = dhcp->ciaddr;
	if (s != -1 && from.s_addr != INADDR_ANY)
		to.s_addr = state->lease.server.s_addr;
	else
		to.s_addr = INADDR_ANY;
	if (to.s_addr && to.s_addr != INADDR_BROADCAST) {
		struct sockaddr_in sin;

		memset(&sin, 0, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = to.s_addr;
		sin.sin_port = htons(DHCP_SERVER_PORT);
		r = sendto(s, (uint8_t *)dhcp, len, 0,
		    (struct sockaddr *)&sin, sizeof(sin));
		if (r == -1)
			logger(ifp->ctx, LOG_ERR,
			    "%s: dhcp_sendpacket: %m", ifp->name);
	} else {
		size_t ulen;

		r = 0;
		udp = dhcp_makeudppacket(&ulen, (uint8_t *)dhcp, len, from, to);
		if (udp == NULL) {
			logger(ifp->ctx, LOG_ERR, "dhcp_makeudppacket: %m");
		} else {
			r = if_sendrawpacket(ifp, ETHERTYPE_IP,
			    (uint8_t *)udp, ulen);
			free(udp);
		}
		/* If we failed to send a raw packet this normally means
		 * we don't have the ability to work beneath the IP layer
		 * for this interface.
		 * As such we remove it from consideration without actually
		 * stopping the interface. */
		if (r == -1) {
			logger(ifp->ctx, LOG_ERR,
			    "%s: if_sendrawpacket: %m", ifp->name);
			switch(errno) {
			case ENETDOWN:
			case ENETRESET:
			case ENETUNREACH:
				break;
			default:
				if (!(ifp->ctx->options & DHCPCD_TEST))
					dhcp_drop(ifp, "FAIL");
				dhcp_free(ifp);
				eloop_timeout_delete(ifp->ctx->eloop,
				    NULL, ifp);
				callback = NULL;
			}
		}
	}
	free(dhcp);

fail:
	if (s != -1)
		close(s);

	/* Even if we fail to send a packet we should continue as we are
	 * as our failure timeouts will change out codepath when needed. */
	if (callback)
		eloop_timeout_add_tv(ifp->ctx->eloop, &tv, callback, ifp);
}

static void
send_inform(void *arg)
{

	send_message((struct interface *)arg, DHCP_INFORM, send_inform);
}

static void
send_discover(void *arg)
{

	send_message((struct interface *)arg, DHCP_DISCOVER, send_discover);
}

static void
send_request(void *arg)
{

	send_message((struct interface *)arg, DHCP_REQUEST, send_request);
}

static void
send_renew(void *arg)
{

	send_message((struct interface *)arg, DHCP_REQUEST, send_renew);
}

static void
send_rebind(void *arg)
{

	send_message((struct interface *)arg, DHCP_REQUEST, send_rebind);
}

void
dhcp_discover(void *arg)
{
	struct interface *ifp = arg;
	struct dhcp_state *state = D_STATE(ifp);
	struct if_options *ifo = ifp->options;

	state->state = DHS_DISCOVER;
	state->xid = dhcp_xid(ifp);
	eloop_timeout_delete(ifp->ctx->eloop, NULL, ifp);
	if (ifo->fallback)
		eloop_timeout_add_sec(ifp->ctx->eloop,
		    ifo->reboot, dhcp_fallback, ifp);
	else if (ifo->options & DHCPCD_IPV4LL)
		eloop_timeout_add_sec(ifp->ctx->eloop,
		    ifo->reboot, ipv4ll_start, ifp);
	if (ifo->options & DHCPCD_REQUEST)
		logger(ifp->ctx, LOG_INFO,
		    "%s: soliciting a DHCP lease (requesting %s)",
		    ifp->name, inet_ntoa(ifo->req_addr));
	else
		logger(ifp->ctx, LOG_INFO,
		    "%s: soliciting a %s lease",
		    ifp->name, ifo->options & DHCPCD_BOOTP ? "BOOTP" : "DHCP");
	send_discover(ifp);
}

static void
dhcp_request(void *arg)
{
	struct interface *ifp = arg;
	struct dhcp_state *state = D_STATE(ifp);

	state->state = DHS_REQUEST;
	send_request(ifp);
}

static void
dhcp_expire(void *arg)
{
	struct interface *ifp = arg;
	struct dhcp_state *state = D_STATE(ifp);

	logger(ifp->ctx, LOG_ERR, "%s: DHCP lease expired", ifp->name);
	eloop_timeout_delete(ifp->ctx->eloop, NULL, ifp);
	dhcp_drop(ifp, "EXPIRE");
	unlink(state->leasefile);
	state->interval = 0;
	dhcp_discover(ifp);
}

static void
dhcp_decline(struct interface *ifp)
{

	send_message(ifp, DHCP_DECLINE, NULL);
}

static void
dhcp_renew(void *arg)
{
	struct interface *ifp = arg;
	struct dhcp_state *state = D_STATE(ifp);
	struct dhcp_lease *lease = &state->lease;

	logger(ifp->ctx, LOG_DEBUG, "%s: renewing lease of %s",
	    ifp->name, inet_ntoa(lease->addr));
	logger(ifp->ctx, LOG_DEBUG, "%s: rebind in %"PRIu32" seconds,"
	    " expire in %"PRIu32" seconds",
	    ifp->name, lease->rebindtime - lease->renewaltime,
	    lease->leasetime - lease->renewaltime);
	state->state = DHS_RENEW;
	state->xid = dhcp_xid(ifp);
	send_renew(ifp);
}

static void
dhcp_arp_announced(struct arp_state *astate)
{

	arp_close(astate->iface);
}

static void
dhcp_rebind(void *arg)
{
	struct interface *ifp = arg;
	struct dhcp_state *state = D_STATE(ifp);
	struct dhcp_lease *lease = &state->lease;

	logger(ifp->ctx, LOG_WARNING,
	    "%s: failed to renew DHCP, rebinding", ifp->name);
	logger(ifp->ctx, LOG_DEBUG, "%s: expire in %"PRIu32" seconds",
	    ifp->name, lease->leasetime - lease->rebindtime);
	state->state = DHS_REBIND;
	eloop_timeout_delete(ifp->ctx->eloop, send_renew, ifp);
	state->lease.server.s_addr = 0;
	ifp->options->options &= ~(DHCPCD_CSR_WARNED |
	    DHCPCD_ROUTER_HOST_ROUTE_WARNED);
	send_rebind(ifp);
}

static void
dhcp_arp_probed(struct arp_state *astate)
{
	struct dhcp_state *state;
	struct if_options *ifo;

	state = D_STATE(astate->iface);
	ifo = astate->iface->options;
	if (state->arping_index < ifo->arping_len) {
		/* We didn't find a profile for this
		 * address or hwaddr, so move to the next
		 * arping profile */
		if (++state->arping_index < ifo->arping_len) {
			astate->addr.s_addr =
			    ifo->arping[state->arping_index - 1];
			arp_probe(astate);
		}
		dhcpcd_startinterface(astate->iface);
		return;
	}

	logger(astate->iface->ctx, LOG_DEBUG, "%s: DAD completed for %s",
	    astate->iface->name, inet_ntoa(astate->addr));
	dhcp_bind(astate->iface);
	arp_announce(astate);

	/* Stop IPv4LL now we have a working DHCP address */
	ipv4ll_drop(astate->iface);
}

static void
dhcp_arp_conflicted(struct arp_state *astate, const struct arp_msg *amsg)
{
	struct interface *ifp;
	struct dhcp_state *state;
	struct if_options *ifo;

	ifp = astate->iface;
	ifo = ifp->options;
	state = D_STATE(ifp);
	if (state->arping_index &&
	    state->arping_index <= ifo->arping_len &&
	    amsg &&
	    (amsg->sip.s_addr == ifo->arping[state->arping_index - 1] ||
	    (amsg->sip.s_addr == 0 &&
	    amsg->tip.s_addr == ifo->arping[state->arping_index - 1])))
	{
		char buf[HWADDR_LEN * 3];

		astate->failed.s_addr = ifo->arping[state->arping_index - 1];
		arp_report_conflicted(astate, amsg);
		hwaddr_ntoa(amsg->sha, ifp->hwlen, buf, sizeof(buf));
		if (dhcpcd_selectprofile(ifp, buf) == -1 &&
		    dhcpcd_selectprofile(ifp,
		        inet_ntoa(astate->failed)) == -1)
		{
			/* We didn't find a profile for this
			 * address or hwaddr, so move to the next
			 * arping profile */
			dhcp_arp_probed(astate);
			return;
		}
		dhcp_close(ifp);
		arp_free(astate);
		eloop_timeout_delete(ifp->ctx->eloop, NULL, ifp);
		dhcpcd_startinterface(ifp);
	}

	/* RFC 2131 3.1.5, Client-server interaction
	 * NULL amsg means IN_IFF_DUPLICATED */
	if (amsg == NULL || (state->offer &&
	    (amsg->sip.s_addr == state->offer->yiaddr ||
	    (amsg->sip.s_addr == 0 &&
	    amsg->tip.s_addr == state->offer->yiaddr))))
	{
#ifdef IN_IFF_DUPLICATED
		struct ipv4_addr *ia;
#endif

		if (amsg)
			astate->failed.s_addr = state->offer->yiaddr;
		else
			astate->failed = astate->addr;
		arp_report_conflicted(astate, amsg);
		unlink(state->leasefile);
		if (!(ifp->options->options & DHCPCD_STATIC) &&
		    !state->lease.frominfo)
			dhcp_decline(ifp);
#ifdef IN_IFF_DUPLICATED
		ia = ipv4_iffindaddr(ifp, &astate->addr, NULL);
		if (ia)
			ipv4_deladdr(ifp, &ia->addr, &ia->net, 1);
#endif
		arp_free(astate);
		eloop_timeout_delete(ifp->ctx->eloop, NULL, ifp);
		eloop_timeout_add_sec(ifp->ctx->eloop,
		    DHCP_RAND_MAX, dhcp_discover, ifp);
	}
}

void
dhcp_bind(struct interface *ifp)
{
	struct dhcp_state *state = D_STATE(ifp);
	struct if_options *ifo = ifp->options;
	struct dhcp_lease *lease = &state->lease;

	state->reason = NULL;
	free(state->old);
	state->old = state->new;
	state->new = state->offer;
	state->offer = NULL;
	get_lease(ifp->ctx, lease, state->new);
	if (ifo->options & DHCPCD_STATIC) {
		logger(ifp->ctx, LOG_INFO, "%s: using static address %s/%d",
		    ifp->name, inet_ntoa(lease->addr),
		    inet_ntocidr(lease->net));
		lease->leasetime = ~0U;
		state->reason = "STATIC";
	} else if (ifo->options & DHCPCD_INFORM) {
		if (ifo->req_addr.s_addr != 0)
			lease->addr.s_addr = ifo->req_addr.s_addr;
		else
			lease->addr.s_addr = state->addr.s_addr;
		logger(ifp->ctx, LOG_INFO, "%s: received approval for %s",
		    ifp->name, inet_ntoa(lease->addr));
		lease->leasetime = ~0U;
		state->reason = "INFORM";
	} else {
		if (lease->frominfo)
			state->reason = "TIMEOUT";
		if (lease->leasetime == ~0U) {
			lease->renewaltime =
			    lease->rebindtime =
			    lease->leasetime;
			logger(ifp->ctx, LOG_INFO, "%s: leased %s for infinity",
			    ifp->name, inet_ntoa(lease->addr));
		} else {
			if (lease->leasetime < DHCP_MIN_LEASE) {
				logger(ifp->ctx, LOG_WARNING,
				    "%s: minimum lease is %d seconds",
				    ifp->name, DHCP_MIN_LEASE);
				lease->leasetime = DHCP_MIN_LEASE;
			}
			if (lease->rebindtime == 0)
				lease->rebindtime =
				    (uint32_t)(lease->leasetime * T2);
			else if (lease->rebindtime >= lease->leasetime) {
				lease->rebindtime =
				    (uint32_t)(lease->leasetime * T2);
				logger(ifp->ctx, LOG_WARNING,
				    "%s: rebind time greater than lease "
				    "time, forcing to %"PRIu32" seconds",
				    ifp->name, lease->rebindtime);
			}
			if (lease->renewaltime == 0)
				lease->renewaltime =
				    (uint32_t)(lease->leasetime * T1);
			else if (lease->renewaltime > lease->rebindtime) {
				lease->renewaltime =
				    (uint32_t)(lease->leasetime * T1);
				logger(ifp->ctx, LOG_WARNING,
				    "%s: renewal time greater than rebind "
				    "time, forcing to %"PRIu32" seconds",
				    ifp->name, lease->renewaltime);
			}
			logger(ifp->ctx,
			    lease->addr.s_addr == state->addr.s_addr &&
			    !(state->added & STATE_FAKE) ?
			    LOG_DEBUG : LOG_INFO,
			    "%s: leased %s for %"PRIu32" seconds", ifp->name,
			    inet_ntoa(lease->addr), lease->leasetime);
		}
	}
	if (ifp->ctx->options & DHCPCD_TEST) {
		state->reason = "TEST";
		script_runreason(ifp, state->reason);
		eloop_exit(ifp->ctx->eloop, EXIT_SUCCESS);
		return;
	}
	if (state->reason == NULL) {
		if (state->old && !(state->added & STATE_FAKE)) {
			if (state->old->yiaddr == state->new->yiaddr &&
			    lease->server.s_addr)
				state->reason = "RENEW";
			else
				state->reason = "REBIND";
		} else if (state->state == DHS_REBOOT)
			state->reason = "REBOOT";
		else
			state->reason = "BOUND";
	}
	if (lease->leasetime == ~0U)
		lease->renewaltime = lease->rebindtime = lease->leasetime;
	else {
		eloop_timeout_add_sec(ifp->ctx->eloop,
		    (time_t)lease->renewaltime, dhcp_renew, ifp);
		eloop_timeout_add_sec(ifp->ctx->eloop,
		    (time_t)lease->rebindtime, dhcp_rebind, ifp);
		eloop_timeout_add_sec(ifp->ctx->eloop,
		    (time_t)lease->leasetime, dhcp_expire, ifp);
		logger(ifp->ctx, LOG_DEBUG,
		    "%s: renew in %"PRIu32" seconds, rebind in %"PRIu32
		    " seconds",
		    ifp->name, lease->renewaltime, lease->rebindtime);
	}
	state->state = DHS_BOUND;
	if (!state->lease.frominfo &&
	    !(ifo->options & (DHCPCD_INFORM | DHCPCD_STATIC)))
		if (write_lease(ifp, state->new) == -1)
			logger(ifp->ctx, LOG_ERR,
			    "%s: write_lease: %m", __func__);

	ipv4_applyaddr(ifp);
}

static void
dhcp_timeout(void *arg)
{
	struct interface *ifp = arg;
	struct dhcp_state *state = D_STATE(ifp);

	dhcp_bind(ifp);
	state->interval = 0;
	dhcp_discover(ifp);
}

struct dhcp_message *
dhcp_message_new(const struct in_addr *addr, const struct in_addr *mask)
{
	struct dhcp_message *dhcp;
	uint8_t *p;

	dhcp = calloc(1, sizeof(*dhcp));
	if (dhcp == NULL)
		return NULL;
	dhcp->yiaddr = addr->s_addr;
	dhcp->cookie = htonl(MAGIC_COOKIE);
	p = dhcp->options;
	if (mask && mask->s_addr != INADDR_ANY) {
		*p++ = DHO_SUBNETMASK;
		*p++ = sizeof(mask->s_addr);
		memcpy(p, &mask->s_addr, sizeof(mask->s_addr));
		p+= sizeof(mask->s_addr);
	}
	*p++ = DHO_END;
	return dhcp;
}

static void
dhcp_arp_bind(struct interface *ifp)
{
	const struct dhcp_state *state;
	struct in_addr addr;
	struct ipv4_addr *ia;
	struct arp_state *astate;

	eloop_timeout_delete(ifp->ctx->eloop, NULL, ifp);

	state = D_CSTATE(ifp);
	addr.s_addr = state->offer->yiaddr;
	/* If the interface already has the address configured
	 * then we can't ARP for duplicate detection. */
	ia = ipv4_findaddr(ifp->ctx, &addr);

#ifdef IN_IFF_TENTATIVE
	if (ia == NULL || ia->addr_flags & IN_IFF_NOTUSEABLE) {
		if ((astate = arp_new(ifp, &addr)) != NULL) {
			astate->probed_cb = dhcp_arp_probed;
			astate->conflicted_cb = dhcp_arp_conflicted;
			astate->announced_cb = dhcp_arp_announced;
		}
		if (ia == NULL) {
			struct dhcp_lease l;

			get_lease(ifp->ctx, &l, state->offer);
			/* Add the address now, let the kernel handle DAD. */
			ipv4_addaddr(ifp, &l.addr, &l.net, &l.brd);
		} else
			logger(ifp->ctx, LOG_INFO, "%s: waiting for DAD on %s",
			    ifp->name, inet_ntoa(addr));
		return;
	}
#else
	if (ifp->options->options & DHCPCD_ARP && ia == NULL) {
		struct dhcp_lease l;

		get_lease(ifp->ctx, &l, state->offer);
		logger(ifp->ctx, LOG_INFO, "%s: probing static address %s/%d",
		    ifp->name, inet_ntoa(l.addr), inet_ntocidr(l.net));
		if ((astate = arp_new(ifp, &addr)) != NULL) {
			astate->probed_cb = dhcp_arp_probed;
			astate->conflicted_cb = dhcp_arp_conflicted;
			astate->announced_cb = dhcp_arp_announced;
			/* We need to handle DAD. */
			arp_probe(astate);
		}
		return;
	}
#endif

	dhcp_bind(ifp);
}

static void
dhcp_static(struct interface *ifp)
{
	struct if_options *ifo;
	struct dhcp_state *state;
	struct ipv4_addr *ia;

	state = D_STATE(ifp);
	ifo = ifp->options;

	ia = NULL;
	if (ifo->req_addr.s_addr == INADDR_ANY &&
	    (ia = ipv4_iffindaddr(ifp, NULL, NULL)) == NULL)
	{
		logger(ifp->ctx, LOG_INFO,
		    "%s: waiting for 3rd party to "
		    "configure IP address",
		    ifp->name);
		state->reason = "3RDPARTY";
		script_runreason(ifp, state->reason);
		return;
	}

	state->offer = dhcp_message_new(ia ? &ia->addr : &ifo->req_addr,
	    ia ? &ia->net : &ifo->req_mask);
	if (state->offer)
		dhcp_arp_bind(ifp);
}

void
dhcp_inform(struct interface *ifp)
{
	struct dhcp_state *state;
	struct if_options *ifo;
	struct ipv4_addr *ap;

	state = D_STATE(ifp);
	ifo = ifp->options;
	if (ifp->ctx->options & DHCPCD_TEST) {
		state->addr.s_addr = ifo->req_addr.s_addr;
		state->net.s_addr = ifo->req_mask.s_addr;
	} else {
		if (ifo->req_addr.s_addr == INADDR_ANY) {
			state = D_STATE(ifp);
			ap = ipv4_iffindaddr(ifp, NULL, NULL);
			if (ap == NULL) {
				logger(ifp->ctx, LOG_INFO,
				    "%s: waiting for 3rd party to "
				    "configure IP address",
				    ifp->name);
				state->reason = "3RDPARTY";
				script_runreason(ifp, state->reason);
				return;
			}
			state->offer =
			    dhcp_message_new(&ap->addr, &ap->net);
		} else
			state->offer =
			    dhcp_message_new(&ifo->req_addr, &ifo->req_mask);
		if (state->offer) {
			ifo->options |= DHCPCD_STATIC;
			dhcp_bind(ifp);
			ifo->options &= ~DHCPCD_STATIC;
		}
	}

	state->state = DHS_INFORM;
	state->xid = dhcp_xid(ifp);
	send_inform(ifp);
}

void
dhcp_reboot_newopts(struct interface *ifp, unsigned long long oldopts)
{
	struct if_options *ifo;
	struct dhcp_state *state = D_STATE(ifp);

	if (state == NULL)
		return;
	ifo = ifp->options;
	if ((ifo->options & (DHCPCD_INFORM | DHCPCD_STATIC) &&
		state->addr.s_addr != ifo->req_addr.s_addr) ||
	    (oldopts & (DHCPCD_INFORM | DHCPCD_STATIC) &&
		!(ifo->options & (DHCPCD_INFORM | DHCPCD_STATIC))))
	{
		dhcp_drop(ifp, "EXPIRE");
	}
}

static void
dhcp_reboot(struct interface *ifp)
{
	struct if_options *ifo;
	struct dhcp_state *state = D_STATE(ifp);

	if (state == NULL)
		return;
	ifo = ifp->options;
	state->state = DHS_REBOOT;
	state->interval = 0;

	if (ifo->options & DHCPCD_LINK && ifp->carrier == LINK_DOWN) {
		logger(ifp->ctx, LOG_INFO,
		    "%s: waiting for carrier", ifp->name);
		return;
	}
	if (ifo->options & DHCPCD_STATIC) {
		dhcp_static(ifp);
		return;
	}
	if (ifo->options & DHCPCD_INFORM) {
		logger(ifp->ctx, LOG_INFO, "%s: informing address of %s",
		    ifp->name, inet_ntoa(state->lease.addr));
		dhcp_inform(ifp);
		return;
	}
	if (ifo->reboot == 0 || state->offer == NULL) {
		dhcp_discover(ifp);
		return;
	}
	if (state->offer->cookie == 0)
		return;

	logger(ifp->ctx, LOG_INFO, "%s: rebinding lease of %s",
	    ifp->name, inet_ntoa(state->lease.addr));
	state->xid = dhcp_xid(ifp);
	state->lease.server.s_addr = 0;
	eloop_timeout_delete(ifp->ctx->eloop, NULL, ifp);

	/* Need to add this before dhcp_expire and friends. */
	if (!ifo->fallback && ifo->options & DHCPCD_IPV4LL)
		eloop_timeout_add_sec(ifp->ctx->eloop,
		    ifo->reboot, ipv4ll_start, ifp);

	if (ifo->options & DHCPCD_LASTLEASE && state->lease.frominfo)
		eloop_timeout_add_sec(ifp->ctx->eloop,
		    ifo->reboot, dhcp_timeout, ifp);
	else if (!(ifo->options & DHCPCD_INFORM))
		eloop_timeout_add_sec(ifp->ctx->eloop,
		    ifo->reboot, dhcp_expire, ifp);

	/* Don't bother ARP checking as the server could NAK us first.
	 * Don't call dhcp_request as that would change the state */
	send_request(ifp);
}

void
dhcp_drop(struct interface *ifp, const char *reason)
{
	struct dhcp_state *state;
#ifdef RELEASE_SLOW
	struct timespec ts;
#endif

	state = D_STATE(ifp);
	/* dhcp_start may just have been called and we don't yet have a state
	 * but we do have a timeout, so punt it. */
	if (state == NULL) {
		eloop_timeout_delete(ifp->ctx->eloop, NULL, ifp);
		return;
	}

	if (ifp->options->options & DHCPCD_RELEASE) {
		/* Failure to send the release may cause this function to
		 * re-enter so guard by setting the state. */
		if (state->state == DHS_RELEASE)
			return;
		state->state = DHS_RELEASE;

		unlink(state->leasefile);
		if (ifp->carrier != LINK_DOWN &&
		    state->new != NULL &&
		    state->lease.server.s_addr != INADDR_ANY)
		{
			logger(ifp->ctx, LOG_INFO, "%s: releasing lease of %s",
			    ifp->name, inet_ntoa(state->lease.addr));
			state->xid = dhcp_xid(ifp);
			send_message(ifp, DHCP_RELEASE, NULL);
#ifdef RELEASE_SLOW
			/* Give the packet a chance to go */
			ts.tv_sec = RELEASE_DELAY_S;
			ts.tv_nsec = RELEASE_DELAY_NS;
			nanosleep(&ts, NULL);
#endif
		}
	}

	eloop_timeout_delete(ifp->ctx->eloop, NULL, ifp);
	dhcp_auth_reset(&state->auth);
	dhcp_close(ifp);

	free(state->offer);
	state->offer = NULL;
	free(state->old);
	state->old = state->new;
	state->new = NULL;
	state->reason = reason;
	ipv4_applyaddr(ifp);
	free(state->old);
	state->old = NULL;
	state->lease.addr.s_addr = 0;
	ifp->options->options &= ~(DHCPCD_CSR_WARNED |
	    DHCPCD_ROUTER_HOST_ROUTE_WARNED);
}

static void
log_dhcp1(int lvl, const char *msg,
    const struct interface *ifp, const struct dhcp_message *dhcp,
    const struct in_addr *from, int ad)
{
	const char *tfrom;
	char *a,  sname[sizeof(dhcp->servername) * 4];
	struct in_addr addr;
	int r;

	if (strcmp(msg, "NAK:") == 0) {
		a = get_option_string(ifp->ctx, dhcp, DHO_MESSAGE);
		if (a) {
			char *tmp;
			size_t al, tmpl;

			al = strlen(a);
			tmpl = (al * 4) + 1;
			tmp = malloc(tmpl);
			if (tmp == NULL) {
				logger(ifp->ctx, LOG_ERR, "%s: %m", __func__);
				free(a);
				return;
			}
			print_string(tmp, tmpl, STRING, (uint8_t *)a, al);
			free(a);
			a = tmp;
		}
	} else if (ad && dhcp->yiaddr != 0) {
		addr.s_addr = dhcp->yiaddr;
		a = strdup(inet_ntoa(addr));
		if (a == NULL) {
			logger(ifp->ctx, LOG_ERR, "%s: %m", __func__);
			return;
		}
	} else
		a = NULL;

	tfrom = "from";
	r = get_option_addr(ifp->ctx, &addr, dhcp, DHO_SERVERID);
	if (dhcp->servername[0] && r == 0) {
		print_string(sname, sizeof(sname), STRING,
		    dhcp->servername, strlen((const char *)dhcp->servername));
		if (a == NULL)
			logger(ifp->ctx, lvl, "%s: %s %s %s `%s'",
			    ifp->name, msg, tfrom, inet_ntoa(addr), sname);
		else
			logger(ifp->ctx, lvl, "%s: %s %s %s %s `%s'",
			    ifp->name, msg, a, tfrom, inet_ntoa(addr), sname);
	} else {
		if (r != 0) {
			tfrom = "via";
			addr = *from;
		}
		if (a == NULL)
			logger(ifp->ctx, lvl, "%s: %s %s %s",
			    ifp->name, msg, tfrom, inet_ntoa(addr));
		else
			logger(ifp->ctx, lvl, "%s: %s %s %s %s",
			    ifp->name, msg, a, tfrom, inet_ntoa(addr));
	}
	free(a);
}

static void
log_dhcp(int lvl, const char *msg,
    const struct interface *ifp, const struct dhcp_message *dhcp,
    const struct in_addr *from)
{

	log_dhcp1(lvl, msg, ifp, dhcp, from, 1);
}

static int
blacklisted_ip(const struct if_options *ifo, in_addr_t addr)
{
	size_t i;

	for (i = 0; i < ifo->blacklist_len; i += 2)
		if (ifo->blacklist[i] == (addr & ifo->blacklist[i + 1]))
			return 1;
	return 0;
}

static int
whitelisted_ip(const struct if_options *ifo, in_addr_t addr)
{
	size_t i;

	if (ifo->whitelist_len == 0)
		return -1;
	for (i = 0; i < ifo->whitelist_len; i += 2)
		if (ifo->whitelist[i] == (addr & ifo->whitelist[i + 1]))
			return 1;
	return 0;
}

static void
dhcp_handledhcp(struct interface *ifp, struct dhcp_message **dhcpp,
    const struct in_addr *from)
{
	struct dhcp_state *state = D_STATE(ifp);
	struct if_options *ifo = ifp->options;
	struct dhcp_message *dhcp = *dhcpp;
	struct dhcp_lease *lease = &state->lease;
	uint8_t type, tmp;
	const uint8_t *auth;
	struct in_addr addr;
	unsigned int i;
	size_t auth_len;
	char *msg;
#ifdef IN_IFF_DUPLICATED
	struct ipv4_addr *ia;
#endif

	/* We may have found a BOOTP server */
	if (get_option_uint8(ifp->ctx, &type, dhcp, DHO_MESSAGETYPE) == -1)
		type = 0;
	else if (ifo->options & DHCPCD_BOOTP) {
		logger(ifp->ctx, LOG_DEBUG,
		    "%s: ignoring DHCP reply (excpecting BOOTP)",
		    ifp->name);
		return;
	}

	/* Authenticate the message */
	auth = get_option(ifp->ctx, dhcp, DHO_AUTHENTICATION, &auth_len);
	if (auth) {
		if (dhcp_auth_validate(&state->auth, &ifo->auth,
		    (uint8_t *)*dhcpp, sizeof(**dhcpp), 4, type,
		    auth, auth_len) == NULL)
		{
			logger(ifp->ctx, LOG_DEBUG,
			    "%s: dhcp_auth_validate: %m", ifp->name);
			log_dhcp1(LOG_ERR, "authentication failed",
			    ifp, dhcp, from, 0);
			return;
		}
		if (state->auth.token)
			logger(ifp->ctx, LOG_DEBUG,
			    "%s: validated using 0x%08" PRIu32,
			    ifp->name, state->auth.token->secretid);
		else
			logger(ifp->ctx, LOG_DEBUG,
			    "%s: accepted reconfigure key", ifp->name);
	} else if (ifo->auth.options & DHCPCD_AUTH_SEND) {
		if (ifo->auth.options & DHCPCD_AUTH_REQUIRE) {
			log_dhcp1(LOG_ERR, "no authentication",
			    ifp, dhcp, from, 0);
			return;
		}
		log_dhcp1(LOG_WARNING, "no authentication",
		    ifp, dhcp, from, 0);
	}

	/* RFC 3203 */
	if (type == DHCP_FORCERENEW) {
		if (from->s_addr == INADDR_ANY ||
		    from->s_addr == INADDR_BROADCAST)
		{
			log_dhcp(LOG_ERR, "discarding Force Renew",
			    ifp, dhcp, from);
			return;
		}
		if (auth == NULL) {
			log_dhcp(LOG_ERR, "unauthenticated Force Renew",
			    ifp, dhcp, from);
			if (ifo->auth.options & DHCPCD_AUTH_REQUIRE)
				return;
		}
		if (state->state != DHS_BOUND && state->state != DHS_INFORM) {
			log_dhcp(LOG_DEBUG, "not bound, ignoring Force Renew",
			    ifp, dhcp, from);
			return;
		}
		log_dhcp(LOG_ERR, "Force Renew from", ifp, dhcp, from);
		/* The rebind and expire timings are still the same, we just
		 * enter the renew state early */
		if (state->state == DHS_BOUND) {
			eloop_timeout_delete(ifp->ctx->eloop,
			    dhcp_renew, ifp);
			dhcp_renew(ifp);
		} else {
			eloop_timeout_delete(ifp->ctx->eloop,
			    send_inform, ifp);
			dhcp_inform(ifp);
		}
		return;
	}

	if (state->state == DHS_BOUND) {
		/* Before we supported FORCERENEW we closed off the raw
		 * port so we effectively ignored all messages.
		 * As such we'll not log by default here. */
		//log_dhcp(LOG_DEBUG, "bound, ignoring", iface, dhcp, from);
		return;
	}

	/* Ensure it's the right transaction */
	if (state->xid != ntohl(dhcp->xid)) {
		logger(ifp->ctx, LOG_DEBUG,
		    "%s: wrong xid 0x%x (expecting 0x%x) from %s",
		    ifp->name, ntohl(dhcp->xid), state->xid,
		    inet_ntoa(*from));
		return;
	}
	/* reset the message counter */
	state->interval = 0;

	/* Ensure that no reject options are present */
	for (i = 1; i < 255; i++) {
		if (has_option_mask(ifo->rejectmask, i) &&
		    get_option_uint8(ifp->ctx, &tmp, dhcp, (uint8_t)i) == 0)
		{
			log_dhcp(LOG_WARNING, "reject DHCP", ifp, dhcp, from);
			return;
		}
	}

	if (type == DHCP_NAK) {
		/* For NAK, only check if we require the ServerID */
		if (has_option_mask(ifo->requiremask, DHO_SERVERID) &&
		    get_option_addr(ifp->ctx, &addr, dhcp, DHO_SERVERID) == -1)
		{
			log_dhcp(LOG_WARNING, "reject NAK", ifp, dhcp, from);
			return;
		}

		/* We should restart on a NAK */
		log_dhcp(LOG_WARNING, "NAK:", ifp, dhcp, from);
		if ((msg = get_option_string(ifp->ctx, dhcp, DHO_MESSAGE))) {
			logger(ifp->ctx, LOG_WARNING, "%s: message: %s",
			    ifp->name, msg);
			free(msg);
		}
		if (state->state == DHS_INFORM) /* INFORM should not be NAKed */
			return;
		if (!(ifp->ctx->options & DHCPCD_TEST)) {
			dhcp_drop(ifp, "NAK");
			unlink(state->leasefile);
		}

		/* If we constantly get NAKS then we should slowly back off */
		eloop_timeout_add_sec(ifp->ctx->eloop,
		    state->nakoff, dhcp_discover, ifp);
		if (state->nakoff == 0)
			state->nakoff = 1;
		else {
			state->nakoff *= 2;
			if (state->nakoff > NAKOFF_MAX)
				state->nakoff = NAKOFF_MAX;
		}
		return;
	}

	/* Ensure that all required options are present */
	for (i = 1; i < 255; i++) {
		if (has_option_mask(ifo->requiremask, i) &&
		    get_option_uint8(ifp->ctx, &tmp, dhcp, (uint8_t)i) != 0)
		{
			/* If we are BOOTP, then ignore the need for serverid.
			 * To ignore BOOTP, require dhcp_message_type.
			 * However, nothing really stops BOOTP from providing
			 * DHCP style options as well so the above isn't
			 * always true. */
			if (type == 0 && i == DHO_SERVERID)
				continue;
			log_dhcp(LOG_WARNING, "reject DHCP", ifp, dhcp, from);
			return;
		}
	}

	/* DHCP Auto-Configure, RFC 2563 */
	if (type == DHCP_OFFER && dhcp->yiaddr == 0) {
		log_dhcp(LOG_WARNING, "no address given", ifp, dhcp, from);
		if ((msg = get_option_string(ifp->ctx, dhcp, DHO_MESSAGE))) {
			logger(ifp->ctx, LOG_WARNING,
			    "%s: message: %s", ifp->name, msg);
			free(msg);
		}
		if (state->state == DHS_DISCOVER &&
		    get_option_uint8(ifp->ctx, &tmp, dhcp,
		    DHO_AUTOCONFIGURE) == 0)
		{
			switch (tmp) {
			case 0:
				log_dhcp(LOG_WARNING, "IPv4LL disabled from",
				    ifp, dhcp, from);
				ipv4ll_drop(ifp);
				arp_close(ifp);
				break;
			case 1:
				log_dhcp(LOG_WARNING, "IPv4LL enabled from",
				    ifp, dhcp, from);
				ipv4ll_start(ifp);
				break;
			default:
				logger(ifp->ctx, LOG_ERR,
				    "%s: unknown auto configuration option %d",
				    ifp->name, tmp);
				break;
			}
			eloop_timeout_delete(ifp->ctx->eloop, NULL, ifp);
			eloop_timeout_add_sec(ifp->ctx->eloop,
			    DHCP_MAX, dhcp_discover, ifp);
		}
		return;
	}

	/* Ensure that the address offered is valid */
	if ((type == 0 || type == DHCP_OFFER || type == DHCP_ACK) &&
	    (dhcp->ciaddr == INADDR_ANY || dhcp->ciaddr == INADDR_BROADCAST) &&
	    (dhcp->yiaddr == INADDR_ANY || dhcp->yiaddr == INADDR_BROADCAST))
	{
		log_dhcp(LOG_WARNING, "reject invalid address",
		    ifp, dhcp, from);
		return;
	}

#ifdef IN_IFF_DUPLICATED
	ia = ipv4_iffindaddr(ifp, &lease->addr, NULL);
	if (ia && ia->addr_flags & IN_IFF_DUPLICATED) {
		log_dhcp(LOG_WARNING, "declined duplicate address",
		    ifp, dhcp, from);
		if (type)
			dhcp_decline(ifp);
		ipv4_deladdr(ifp, &ia->addr, &ia->net, 0);
		eloop_timeout_delete(ifp->ctx->eloop, NULL, ifp);
		eloop_timeout_add_sec(ifp->ctx->eloop,
		    DHCP_RAND_MAX, dhcp_discover, ifp);
		return;
	}
#endif

	if ((type == 0 || type == DHCP_OFFER) && state->state == DHS_DISCOVER) {
		lease->frominfo = 0;
		lease->addr.s_addr = dhcp->yiaddr;
		lease->cookie = dhcp->cookie;
		if (type == 0 ||
		    get_option_addr(ifp->ctx,
		    &lease->server, dhcp, DHO_SERVERID) != 0)
			lease->server.s_addr = INADDR_ANY;
		log_dhcp(LOG_INFO, "offered", ifp, dhcp, from);
		free(state->offer);
		state->offer = dhcp;
		*dhcpp = NULL;
		if (ifp->ctx->options & DHCPCD_TEST) {
			free(state->old);
			state->old = state->new;
			state->new = state->offer;
			state->offer = NULL;
			state->reason = "TEST";
			script_runreason(ifp, state->reason);
			eloop_exit(ifp->ctx->eloop, EXIT_SUCCESS);
			return;
		}
		eloop_timeout_delete(ifp->ctx->eloop, send_discover, ifp);
		/* We don't request BOOTP addresses */
		if (type) {
			/* We used to ARP check here, but that seems to be in
			 * violation of RFC2131 where it only describes
			 * DECLINE after REQUEST.
			 * It also seems that some MS DHCP servers actually
			 * ignore DECLINE if no REQUEST, ie we decline a
			 * DISCOVER. */
			dhcp_request(ifp);
			return;
		}
	}

	if (type) {
		if (type == DHCP_OFFER) {
			log_dhcp(LOG_WARNING, "ignoring offer of",
			    ifp, dhcp, from);
			return;
		}

		/* We should only be dealing with acks */
		if (type != DHCP_ACK) {
			log_dhcp(LOG_ERR, "not ACK or OFFER",
			    ifp, dhcp, from);
			return;
		}

		if (!(ifo->options & DHCPCD_INFORM))
			log_dhcp(LOG_DEBUG, "acknowledged", ifp, dhcp, from);
		else
		    ifo->options &= ~DHCPCD_STATIC;
	}

	/* No NAK, so reset the backoff
	 * We don't reset on an OFFER message because the server could
	 * potentially NAK the REQUEST. */
	state->nakoff = 0;

	/* BOOTP could have already assigned this above, so check we still
	 * have a pointer. */
	if (*dhcpp) {
		free(state->offer);
		state->offer = dhcp;
		*dhcpp = NULL;
	}

	lease->frominfo = 0;
	eloop_timeout_delete(ifp->ctx->eloop, NULL, ifp);

	dhcp_arp_bind(ifp);
}

static size_t
get_udp_data(const uint8_t **data, const uint8_t *udp)
{
	struct udp_dhcp_packet p;

	memcpy(&p, udp, sizeof(p));
	*data = udp + offsetof(struct udp_dhcp_packet, dhcp);
	return ntohs(p.ip.ip_len) - sizeof(p.ip) - sizeof(p.udp);
}

static int
valid_udp_packet(const uint8_t *data, size_t data_len, struct in_addr *from,
    int noudpcsum)
{
	struct udp_dhcp_packet p;
	uint16_t bytes, udpsum;

	if (data_len < sizeof(p.ip)) {
		if (from)
			from->s_addr = INADDR_ANY;
		errno = EINVAL;
		return -1;
	}
	memcpy(&p, data, MIN(data_len, sizeof(p)));
	if (from)
		from->s_addr = p.ip.ip_src.s_addr;
	if (data_len > sizeof(p)) {
		errno = EINVAL;
		return -1;
	}
	if (checksum(&p.ip, sizeof(p.ip)) != 0) {
		errno = EINVAL;
		return -1;
	}

	bytes = ntohs(p.ip.ip_len);
	if (data_len < bytes) {
		errno = EINVAL;
		return -1;
	}

	if (noudpcsum == 0) {
		udpsum = p.udp.uh_sum;
		p.udp.uh_sum = 0;
		p.ip.ip_hl = 0;
		p.ip.ip_v = 0;
		p.ip.ip_tos = 0;
		p.ip.ip_len = p.udp.uh_ulen;
		p.ip.ip_id = 0;
		p.ip.ip_off = 0;
		p.ip.ip_ttl = 0;
		p.ip.ip_sum = 0;
		if (udpsum && checksum(&p, bytes) != udpsum) {
			errno = EINVAL;
			return -1;
		}
	}

	return 0;
}

static void
dhcp_handlepacket(void *arg)
{
	struct interface *ifp = arg;
	struct dhcp_message *dhcp = NULL;
	const uint8_t *pp;
	size_t bytes;
	struct in_addr from;
	int i, flags;
	const struct dhcp_state *state = D_CSTATE(ifp);

	/* Need this API due to BPF */
	flags = 0;
	while (!(flags & RAW_EOF)) {
		bytes = (size_t)if_readrawpacket(ifp, ETHERTYPE_IP,
		    ifp->ctx->packet, udp_dhcp_len, &flags);
		if ((ssize_t)bytes == -1) {
			logger(ifp->ctx, LOG_ERR,
			    "%s: dhcp if_readrawpacket: %m", ifp->name);
			dhcp_close(ifp);
			arp_close(ifp);
			break;
		}
		if (valid_udp_packet(ifp->ctx->packet, bytes,
		    &from, flags & RAW_PARTIALCSUM) == -1)
		{
			logger(ifp->ctx, LOG_ERR,
			    "%s: invalid UDP packet from %s",
			    ifp->name, inet_ntoa(from));
			continue;
		}
		i = whitelisted_ip(ifp->options, from.s_addr);
		if (i == 0) {
			logger(ifp->ctx, LOG_WARNING,
			    "%s: non whitelisted DHCP packet from %s",
			    ifp->name, inet_ntoa(from));
			continue;
		} else if (i != 1 &&
		    blacklisted_ip(ifp->options, from.s_addr) == 1)
		{
			logger(ifp->ctx, LOG_WARNING,
			    "%s: blacklisted DHCP packet from %s",
			    ifp->name, inet_ntoa(from));
			continue;
		}
		if (ifp->flags & IFF_POINTOPOINT &&
		    state->dst.s_addr != from.s_addr)
		{
			logger(ifp->ctx, LOG_WARNING,
			    "%s: server %s is not destination",
			    ifp->name, inet_ntoa(from));
		}
		bytes = get_udp_data(&pp, ifp->ctx->packet);
		if (bytes > sizeof(*dhcp)) {
			logger(ifp->ctx, LOG_ERR,
			    "%s: packet greater than DHCP size from %s",
			    ifp->name, inet_ntoa(from));
			continue;
		}
		if (dhcp == NULL) {
		        dhcp = calloc(1, sizeof(*dhcp));
			if (dhcp == NULL) {
				logger(ifp->ctx, LOG_ERR,
				    "%s: calloc: %m", __func__);
				break;
			}
		}
		memcpy(dhcp, pp, bytes);
		if (dhcp->cookie != htonl(MAGIC_COOKIE)) {
			logger(ifp->ctx, LOG_DEBUG, "%s: bogus cookie from %s",
			    ifp->name, inet_ntoa(from));
			continue;
		}
		/* Ensure packet is for us */
		if (ifp->hwlen <= sizeof(dhcp->chaddr) &&
		    memcmp(dhcp->chaddr, ifp->hwaddr, ifp->hwlen))
		{
			char buf[sizeof(dhcp->chaddr) * 3];

			logger(ifp->ctx, LOG_DEBUG,
			    "%s: xid 0x%x is for hwaddr %s",
			    ifp->name, ntohl(dhcp->xid),
			    hwaddr_ntoa(dhcp->chaddr, sizeof(dhcp->chaddr),
				buf, sizeof(buf)));
			continue;
		}
		dhcp_handledhcp(ifp, &dhcp, &from);
		if (state->raw_fd == -1)
			break;
	}
	free(dhcp);
}

static void
dhcp_handleudp(void *arg)
{
	struct dhcpcd_ctx *ctx;
	uint8_t buffer[sizeof(struct dhcp_message)];

	ctx = arg;

	/* Just read what's in the UDP fd and discard it as we always read
	 * from the raw fd */
	if (read(ctx->udp_fd, buffer, sizeof(buffer)) == -1) {
		logger(ctx, LOG_ERR, "%s: %m", __func__);
		eloop_event_delete(ctx->eloop, ctx->udp_fd);
		close(ctx->udp_fd);
		ctx->udp_fd = -1;
	}
}

static int
dhcp_open(struct interface *ifp)
{
	struct dhcp_state *state;

	if (ifp->ctx->packet == NULL) {
		ifp->ctx->packet = malloc(udp_dhcp_len);
		if (ifp->ctx->packet == NULL) {
			logger(ifp->ctx, LOG_ERR, "%s: %m", __func__);
			return -1;
		}
	}

	state = D_STATE(ifp);
	if (state->raw_fd == -1) {
		state->raw_fd = if_openrawsocket(ifp, ETHERTYPE_IP);
		if (state->raw_fd == -1) {
			if (errno == ENOENT) {
				logger(ifp->ctx, LOG_ERR,
				    "%s not found", if_pfname);
				/* May as well disable IPv4 entirely at
				 * this point as we really need it. */
				ifp->options->options &= ~DHCPCD_IPV4;
			} else
				logger(ifp->ctx, LOG_ERR, "%s: %s: %m",
				    __func__, ifp->name);
			return -1;
		}
		eloop_event_add(ifp->ctx->eloop,
		    state->raw_fd, dhcp_handlepacket, ifp, NULL, NULL);
	}
	return 0;
}

int
dhcp_dump(struct interface *ifp)
{
	struct dhcp_state *state;

	ifp->if_data[IF_DATA_DHCP] = state = calloc(1, sizeof(*state));
	if (state == NULL)
		goto eexit;
	state->raw_fd = -1;
	dhcp_set_leasefile(state->leasefile, sizeof(state->leasefile),
	    AF_INET, ifp);
	state->new = read_lease(ifp);
	if (state->new == NULL) {
		logger(ifp->ctx, LOG_ERR, "%s: %s: %m",
		    *ifp->name ? ifp->name : state->leasefile, __func__);
		return -1;
	}
	state->reason = "DUMP";
	return script_runreason(ifp, state->reason);

eexit:
	logger(ifp->ctx, LOG_ERR, "%s: %m", __func__);
	return -1;
}

void
dhcp_free(struct interface *ifp)
{
	struct dhcp_state *state = D_STATE(ifp);
	struct dhcpcd_ctx *ctx;

	dhcp_close(ifp);
	arp_close(ifp);
	if (state) {
		free(state->old);
		free(state->new);
		free(state->offer);
		free(state->clientid);
		free(state);
		ifp->if_data[IF_DATA_DHCP] = NULL;
	}

	ctx = ifp->ctx;
	/* If we don't have any more DHCP enabled interfaces,
	 * close the global socket and release resources */
	if (ctx->ifaces) {
		TAILQ_FOREACH(ifp, ctx->ifaces, next) {
			if (D_STATE(ifp))
				break;
		}
	}
	if (ifp == NULL) {
		if (ctx->udp_fd != -1) {
			eloop_event_delete(ctx->eloop, ctx->udp_fd);
			close(ctx->udp_fd);
			ctx->udp_fd = -1;
		}

		free(ctx->packet);
		free(ctx->opt_buffer);
		ctx->packet = NULL;
		ctx->opt_buffer = NULL;
	}
}

static int
dhcp_init(struct interface *ifp)
{
	struct dhcp_state *state;
	const struct if_options *ifo;
	uint8_t len;
	char buf[(sizeof(ifo->clientid) - 1) * 3];

	state = D_STATE(ifp);
	if (state == NULL) {
		ifp->if_data[IF_DATA_DHCP] = calloc(1, sizeof(*state));
		state = D_STATE(ifp);
		if (state == NULL)
			return -1;
		/* 0 is a valid fd, so init to -1 */
		state->raw_fd = -1;

		/* Now is a good time to find IPv4 routes */
		if_initrt(ifp);
	}

	state->state = DHS_INIT;
	state->reason = "PREINIT";
	state->nakoff = 0;
	dhcp_set_leasefile(state->leasefile, sizeof(state->leasefile),
	    AF_INET, ifp);

	ifo = ifp->options;
	/* We need to drop the leasefile so that dhcp_start
	 * doesn't load it. */
	if (ifo->options & DHCPCD_REQUEST)
		unlink(state->leasefile);

	free(state->clientid);
	state->clientid = NULL;

	if (*ifo->clientid) {
		state->clientid = malloc((size_t)(ifo->clientid[0] + 1));
		if (state->clientid == NULL)
			goto eexit;
		memcpy(state->clientid, ifo->clientid,
		    (size_t)(ifo->clientid[0]) + 1);
	} else if (ifo->options & DHCPCD_CLIENTID) {
		if (ifo->options & DHCPCD_DUID) {
			state->clientid = malloc(ifp->ctx->duid_len + 6);
			if (state->clientid == NULL)
				goto eexit;
			state->clientid[0] =(uint8_t)(ifp->ctx->duid_len + 5);
			state->clientid[1] = 255; /* RFC 4361 */
			memcpy(state->clientid + 2, ifo->iaid, 4);
			memcpy(state->clientid + 6, ifp->ctx->duid,
			    ifp->ctx->duid_len);
		} else {
			len = (uint8_t)(ifp->hwlen + 1);
			state->clientid = malloc((size_t)len + 1);
			if (state->clientid == NULL)
				goto eexit;
			state->clientid[0] = len;
			state->clientid[1] = (uint8_t)ifp->family;
			memcpy(state->clientid + 2, ifp->hwaddr,
			    ifp->hwlen);
		}
	}

	if (ifo->options & DHCPCD_DUID)
		/* Don't bother logging as DUID and IAID are reported
		 * at device start. */
		return 0;

	if (ifo->options & DHCPCD_CLIENTID)
		logger(ifp->ctx, LOG_DEBUG, "%s: using ClientID %s", ifp->name,
		    hwaddr_ntoa(state->clientid + 1, state->clientid[0],
			buf, sizeof(buf)));
	else if (ifp->hwlen)
		logger(ifp->ctx, LOG_DEBUG, "%s: using hwaddr %s", ifp->name,
		    hwaddr_ntoa(ifp->hwaddr, ifp->hwlen, buf, sizeof(buf)));
	return 0;

eexit:
	logger(ifp->ctx, LOG_ERR, "%s: error making ClientID: %m", __func__);
	return -1;
}

static void
dhcp_start1(void *arg)
{
	struct interface *ifp = arg;
	struct if_options *ifo = ifp->options;
	struct dhcp_state *state;
	struct stat st;
	uint32_t l;
	int nolease;

	if (!(ifo->options & DHCPCD_IPV4))
		return;

	/* Listen on *.*.*.*:bootpc so that the kernel never sends an
	 * ICMP port unreachable message back to the DHCP server */
	if (ifp->ctx->udp_fd == -1) {
		ifp->ctx->udp_fd = dhcp_openudp(NULL);
		if (ifp->ctx->udp_fd == -1) {
			/* Don't log an error if some other process
			 * is handling this. */
			if (errno != EADDRINUSE)
				logger(ifp->ctx, LOG_ERR,
				    "%s: dhcp_openudp: %m", __func__);
		} else
			eloop_event_add(ifp->ctx->eloop,
			    ifp->ctx->udp_fd, dhcp_handleudp,
			    ifp->ctx, NULL, NULL);
	}

	if (dhcp_init(ifp) == -1) {
		logger(ifp->ctx, LOG_ERR, "%s: dhcp_init: %m", ifp->name);
		return;
	}

	state = D_STATE(ifp);
	clock_gettime(CLOCK_MONOTONIC, &state->started);
	free(state->offer);
	state->offer = NULL;

	if (state->arping_index < ifo->arping_len) {
		struct arp_state *astate;

		astate = arp_new(ifp, NULL);
		if (astate) {
			astate->probed_cb = dhcp_arp_probed;
			astate->conflicted_cb = dhcp_arp_conflicted;
			dhcp_arp_probed(astate);
		}
		return;
	}

	if (ifo->options & DHCPCD_STATIC) {
		dhcp_static(ifp);
		return;
	}

	if (ifo->options & DHCPCD_DHCP && dhcp_open(ifp) == -1)
		return;

	if (ifo->options & DHCPCD_INFORM) {
		dhcp_inform(ifp);
		return;
	}
	if (ifp->hwlen == 0 && ifo->clientid[0] == '\0') {
		logger(ifp->ctx, LOG_WARNING,
		    "%s: needs a clientid to configure", ifp->name);
		dhcp_drop(ifp, "FAIL");
		eloop_timeout_delete(ifp->ctx->eloop, NULL, ifp);
		return;
	}
	/* We don't want to read the old lease if we NAK an old test */
	nolease = state->offer && ifp->ctx->options & DHCPCD_TEST;
	if (!nolease && ifo->options & DHCPCD_DHCP) {
		state->offer = read_lease(ifp);
		/* Check the saved lease matches the type we want */
		if (state->offer) {
#ifdef IN_IFF_DUPLICATED
			struct in_addr addr;
			struct ipv4_addr *ia;

			addr.s_addr = state->offer->yiaddr;
			ia = ipv4_iffindaddr(ifp, &addr, NULL);
#endif

			if ((IS_BOOTP(ifp, state->offer) &&
			    !(ifo->options & DHCPCD_BOOTP)) ||
#ifdef IN_IFF_DUPLICATED
			    (ia && ia->addr_flags & IN_IFF_DUPLICATED) ||
#endif
			    (!IS_BOOTP(ifp, state->offer) &&
			    ifo->options & DHCPCD_BOOTP))
			{
				free(state->offer);
				state->offer = NULL;
			}
		}
	}
	if (state->offer) {
		get_lease(ifp->ctx, &state->lease, state->offer);
		state->lease.frominfo = 1;
		if (state->new == NULL &&
		    ipv4_iffindaddr(ifp, &state->lease.addr, &state->lease.net))
		{
			/* We still have the IP address from the last lease.
			 * Fake add the address and routes from it so the lease
			 * can be cleaned up. */
			state->new = malloc(sizeof(*state->new));
			if (state->new) {
				memcpy(state->new, state->offer,
				    sizeof(*state->new));
				state->addr = state->lease.addr;
				state->net = state->lease.net;
				state->added |= STATE_ADDED | STATE_FAKE;
				ipv4_buildroutes(ifp->ctx);
			} else
				logger(ifp->ctx, LOG_ERR, "%s: %m", __func__);
		}
		if (state->offer->cookie == 0) {
			if (state->offer->yiaddr == state->addr.s_addr) {
				free(state->offer);
				state->offer = NULL;
			}
		} else if (state->lease.leasetime != ~0U &&
		    stat(state->leasefile, &st) == 0)
		{
			time_t now;

			/* Offset lease times and check expiry */
			now = time(NULL);
			if (now == -1 ||
			    (time_t)state->lease.leasetime < now - st.st_mtime)
			{
				logger(ifp->ctx, LOG_DEBUG,
				    "%s: discarding expired lease", ifp->name);
				free(state->offer);
				state->offer = NULL;
				state->lease.addr.s_addr = 0;
				/* Technically we should discard the lease
				 * as it's expired, just as DHCPv6 addresses
				 * would be by the kernel.
				 * However, this may violate POLA so
				 * we currently leave it be.
				 * If we get a totally different lease from
				 * the DHCP server we'll drop it anyway, as
				 * we will on any other event which would
				 * trigger a lease drop.
				 * This should only happen if dhcpcd stops
				 * running and the lease expires before
				 * dhcpcd starts again. */
#if 0
				if (state->new)
					dhcp_drop(ifp, "EXPIRE");
#endif
			} else {
				l = (uint32_t)(now - st.st_mtime);
				state->lease.leasetime -= l;
				state->lease.renewaltime -= l;
				state->lease.rebindtime -= l;
			}
		}
	}

	if (!(ifo->options & DHCPCD_DHCP)) {
		if (ifo->options & DHCPCD_IPV4LL)
			ipv4ll_start(ifp);
		return;
	}

	if (state->offer == NULL || state->offer->cookie == 0)
		dhcp_discover(ifp);
	else
		dhcp_reboot(ifp);
}

void
dhcp_start(struct interface *ifp)
{
	struct timespec tv;

	if (!(ifp->options->options & DHCPCD_IPV4))
		return;

	/* No point in delaying a static configuration */
	if (ifp->options->options & DHCPCD_STATIC ||
	    !(ifp->options->options & DHCPCD_INITIAL_DELAY))
	{
		dhcp_start1(ifp);
		return;
	}

	tv.tv_sec = DHCP_MIN_DELAY;
	tv.tv_nsec = (suseconds_t)arc4random_uniform(
	    (DHCP_MAX_DELAY - DHCP_MIN_DELAY) * NSEC_PER_SEC);
	timespecnorm(&tv);
	logger(ifp->ctx, LOG_DEBUG,
	    "%s: delaying IPv4 for %0.1f seconds",
	    ifp->name, timespec_to_double(&tv));

	eloop_timeout_add_tv(ifp->ctx->eloop, &tv, dhcp_start1, ifp);
}

void
dhcp_abort(struct interface *ifp)
{

	eloop_timeout_delete(ifp->ctx->eloop, dhcp_start1, ifp);
}

void
dhcp_handleifa(int cmd, struct interface *ifp,
	const struct in_addr *addr,
	const struct in_addr *net,
	const struct in_addr *dst,
	__unused int flags)
{
	struct dhcp_state *state;
	struct if_options *ifo;
	uint8_t i;

	state = D_STATE(ifp);
	if (state == NULL)
		return;

	if (cmd == RTM_DELADDR) {
		if (state->addr.s_addr == addr->s_addr &&
		    state->net.s_addr == net->s_addr)
		{
			logger(ifp->ctx, LOG_INFO,
			    "%s: removing IP address %s/%d",
			    ifp->name, inet_ntoa(state->addr),
			    inet_ntocidr(state->net));
			dhcp_drop(ifp, "EXPIRE");
		}
		return;
	}

	if (cmd != RTM_NEWADDR)
		return;

	ifo = ifp->options;
	if (ifo->options & DHCPCD_INFORM) {
		if (state->state != DHS_INFORM)
			dhcp_inform(ifp);
		return;
	}

	if (!(ifo->options & DHCPCD_STATIC))
		return;
	if (ifo->req_addr.s_addr != INADDR_ANY)
		return;

	free(state->old);
	state->old = state->new;
	state->new = dhcp_message_new(addr, net);
	if (state->new == NULL)
		return;
	state->dst.s_addr = dst ? dst->s_addr : INADDR_ANY;
	if (dst) {
		for (i = 1; i < 255; i++)
			if (i != DHO_ROUTER && has_option_mask(ifo->dstmask,i))
				dhcp_message_add_addr(state->new, i, *dst);
	}
	state->reason = "STATIC";
	ipv4_buildroutes(ifp->ctx);
	script_runreason(ifp, state->reason);
	if (ifo->options & DHCPCD_INFORM) {
		state->state = DHS_INFORM;
		state->xid = dhcp_xid(ifp);
		state->lease.server.s_addr = dst ? dst->s_addr : INADDR_ANY;
		state->addr = *addr;
		state->net = *net;
		dhcp_inform(ifp);
	}
}
