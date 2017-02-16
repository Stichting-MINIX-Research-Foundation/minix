#include <sys/cdefs.h>
 __RCSID("$NetBSD: dhcp6.c,v 1.15 2015/08/21 10:39:00 roy Exp $");

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

/* TODO: We should decline dupliate addresses detected */

#include <sys/stat.h>
#include <sys/utsname.h>

#include <netinet/in.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define ELOOP_QUEUE 4
#include "config.h"
#include "common.h"
#include "dhcp.h"
#include "dhcp6.h"
#include "duid.h"
#include "eloop.h"
#include "if.h"
#include "if-options.h"
#include "ipv6nd.h"
#include "script.h"

#ifndef __UNCONST
#define __UNCONST(a) ((void *)(unsigned long)(const void *)(a))
#endif

/* DHCPCD Project has been assigned an IANA PEN of 40712 */
#define DHCPCD_IANA_PEN 40712

/* Unsure if I want this */
//#define VENDOR_SPLIT

/* Support older systems with different defines */
#if !defined(IPV6_RECVPKTINFO) && defined(IPV6_PKTINFO)
#define IPV6_RECVPKTINFO IPV6_PKTINFO
#endif

struct dhcp6_op {
	uint16_t type;
	const char *name;
};

static const struct dhcp6_op dhcp6_ops[] = {
	{ DHCP6_SOLICIT, "SOLICIT6" },
	{ DHCP6_ADVERTISE, "ADVERTISE6" },
	{ DHCP6_REQUEST, "REQUEST6" },
	{ DHCP6_REPLY, "REPLY6" },
	{ DHCP6_RENEW, "RENEW6" },
	{ DHCP6_REBIND, "REBIND6" },
	{ DHCP6_CONFIRM, "CONFIRM6" },
	{ DHCP6_INFORMATION_REQ, "INFORM6" },
	{ DHCP6_RELEASE, "RELEASE6" },
	{ DHCP6_RECONFIGURE, "RECONFIURE6" },
	{ 0, NULL }
};

struct dhcp_compat {
	uint8_t dhcp_opt;
	uint16_t dhcp6_opt;
};

const struct dhcp_compat dhcp_compats[] = {
	{ DHO_DNSSERVER,	D6_OPTION_DNS_SERVERS },
	{ DHO_HOSTNAME,		D6_OPTION_FQDN },
	{ DHO_DNSDOMAIN,	D6_OPTION_FQDN },
	{ DHO_NISSERVER,	D6_OPTION_NIS_SERVERS },
	{ DHO_NTPSERVER,	D6_OPTION_SNTP_SERVERS },
	{ DHO_RAPIDCOMMIT,	D6_OPTION_RAPID_COMMIT },
	{ DHO_FQDN,		D6_OPTION_FQDN },
	{ DHO_VIVCO,		D6_OPTION_VENDOR_CLASS },
	{ DHO_VIVSO,		D6_OPTION_VENDOR_OPTS },
	{ DHO_DNSSEARCH,	D6_OPTION_DOMAIN_LIST },
	{ 0, 0 }
};

static const char * const dhcp6_statuses[] = {
	"Success",
	"Unspecified Failure",
	"No Addresses Available",
	"No Binding",
	"Not On Link",
	"Use Multicast"
};

struct dhcp6_ia_addr {
	struct in6_addr addr;
	uint32_t pltime;
	uint32_t vltime;
} __packed;

struct dhcp6_pd_addr {
	uint32_t pltime;
	uint32_t vltime;
	uint8_t prefix_len;
	struct in6_addr prefix;
} __packed;

void
dhcp6_printoptions(const struct dhcpcd_ctx *ctx,
    const struct dhcp_opt *opts, size_t opts_len)
{
	size_t i, j;
	const struct dhcp_opt *opt, *opt2;
	int cols;

	for (i = 0, opt = ctx->dhcp6_opts;
	    i < ctx->dhcp6_opts_len; i++, opt++)
	{
		for (j = 0, opt2 = opts; j < opts_len; j++, opt2++)
			if (opt2->option == opt->option)
				break;
		if (j == opts_len) {
			cols = printf("%05d %s", opt->option, opt->var);
			dhcp_print_option_encoding(opt, cols);
		}
	}
	for (i = 0, opt = opts; i < opts_len; i++, opt++) {
		cols = printf("%05d %s", opt->option, opt->var);
		dhcp_print_option_encoding(opt, cols);
	}
}

static size_t
dhcp6_makevendor(struct dhcp6_option *o, const struct interface *ifp)
{
	const struct if_options *ifo;
	size_t len, i;
	uint8_t *p;
	uint16_t u16;
	uint32_t u32;
	ssize_t vlen;
	const struct vivco *vivco;
	char vendor[VENDORCLASSID_MAX_LEN];

	ifo = ifp->options;
	len = sizeof(uint32_t); /* IANA PEN */
	if (ifo->vivco_en) {
		for (i = 0, vivco = ifo->vivco;
		    i < ifo->vivco_len;
		    i++, vivco++)
			len += sizeof(uint16_t) + vivco->len;
		vlen = 0; /* silence bogus gcc warning */
	} else {
		vlen = dhcp_vendor(vendor, sizeof(vendor));
		if (vlen == -1)
			vlen = 0;
		else
			len += sizeof(uint16_t) + (size_t)vlen;
	}

	if (len > UINT16_MAX) {
		logger(ifp->ctx, LOG_ERR,
		    "%s: DHCPv6 Vendor Class too big", ifp->name);
		return 0;
	}

	if (o) {
		o->code = htons(D6_OPTION_VENDOR_CLASS);
		o->len = htons((uint16_t)len);
		p = D6_OPTION_DATA(o);
		u32 = htonl(ifo->vivco_en ? ifo->vivco_en : DHCPCD_IANA_PEN);
		memcpy(p, &u32, sizeof(u32));
		p += sizeof(u32);
		if (ifo->vivco_en) {
			for (i = 0, vivco = ifo->vivco;
			    i < ifo->vivco_len;
			    i++, vivco++)
			{
				u16 = htons((uint16_t)vivco->len);
				memcpy(p, &u16, sizeof(u16));
				p += sizeof(u16);
				memcpy(p, vivco->data, vivco->len);
				p += vivco->len;
			}
		} else if (vlen) {
			u16 = htons((uint16_t)vlen);
			memcpy(p, &u16, sizeof(u16));
			p += sizeof(u16);
			memcpy(p, vendor, (size_t)vlen);
		}
	}

	return len;
}

static const struct dhcp6_option *
dhcp6_findoption(uint16_t code, const uint8_t *d, size_t len)
{
	const struct dhcp6_option *o;
	size_t ol;

	code = htons(code);
	for (o = (const struct dhcp6_option *)d;
	    len >= sizeof(*o);
	    o = D6_CNEXT_OPTION(o))
	{
		ol = sizeof(*o) + ntohs(o->len);
		if (ol > len) {
			errno = EINVAL;
			return NULL;
		}
		if (o->code == code)
			return o;
		len -= ol;
	}

	errno = ESRCH;
	return NULL;
}

static const uint8_t *
dhcp6_getoption(struct dhcpcd_ctx *ctx,
    size_t *os, unsigned int *code, size_t *len,
    const uint8_t *od, size_t ol, struct dhcp_opt **oopt)
{
	const struct dhcp6_option *o;
	size_t i;
	struct dhcp_opt *opt;

	if (od) {
		*os = sizeof(*o);
		if (ol < *os) {
			errno = EINVAL;
			return NULL;
		}
		o = (const struct dhcp6_option *)od;
		*len = ntohs(o->len);
		if (*len > ol) {
			errno = EINVAL;
			return NULL;
		}
		*code = ntohs(o->code);
	} else
		o = NULL;

	for (i = 0, opt = ctx->dhcp6_opts;
	    i < ctx->dhcp6_opts_len; i++, opt++)
	{
		if (opt->option == *code) {
			*oopt = opt;
			break;
		}
	}

	if (o)
		return D6_COPTION_DATA(o);
	return NULL;
}

static const struct dhcp6_option *
dhcp6_getmoption(uint16_t code, const struct dhcp6_message *m, size_t len)
{

	if (len < sizeof(*m)) {
		errno = EINVAL;
		return NULL;
	}
	len -= sizeof(*m);
	return dhcp6_findoption(code,
	    (const uint8_t *)D6_CFIRST_OPTION(m), len);
}

static int
dhcp6_updateelapsed(struct interface *ifp, struct dhcp6_message *m, size_t len)
{
	struct dhcp6_state *state;
	const struct dhcp6_option *co;
	struct dhcp6_option *o;
	struct timespec tv;
	time_t hsec;
	uint16_t u16;

	co = dhcp6_getmoption(D6_OPTION_ELAPSED, m, len);
	if (co == NULL)
		return -1;

	o = __UNCONST(co);
	state = D6_STATE(ifp);
	clock_gettime(CLOCK_MONOTONIC, &tv);
	if (state->RTC == 0) {
		/* An RTC of zero means we're the first message
		 * out of the door, so the elapsed time is zero. */
		state->started = tv;
		hsec = 0;
	} else {
		timespecsub(&tv, &state->started, &tv);
		/* Elapsed time is measured in centiseconds.
		 * We need to be sure it will not potentially overflow. */
		if (tv.tv_sec >= (UINT16_MAX / CSEC_PER_SEC) + 1)
			hsec = UINT16_MAX;
		else {
			hsec = (tv.tv_sec * CSEC_PER_SEC) +
			    (tv.tv_nsec / NSEC_PER_CSEC);
			if (hsec > UINT16_MAX)
				hsec = UINT16_MAX;
		}
	}
	u16 = htons((uint16_t)hsec);
	memcpy(D6_OPTION_DATA(o), &u16, sizeof(u16));
	return 0;
}

static void
dhcp6_newxid(const struct interface *ifp, struct dhcp6_message *m)
{
	uint32_t xid;

	if (ifp->options->options & DHCPCD_XID_HWADDR &&
	    ifp->hwlen >= sizeof(xid))
		/* The lower bits are probably more unique on the network */
		memcpy(&xid, (ifp->hwaddr + ifp->hwlen) - sizeof(xid),
		    sizeof(xid));
	else
		xid = arc4random();

	m->xid[0] = (xid >> 16) & 0xff;
	m->xid[1] = (xid >> 8) & 0xff;
	m->xid[2] = xid & 0xff;
}

static const struct if_sla *
dhcp6_findselfsla(struct interface *ifp, const uint8_t *iaid)
{
	size_t i, j;

	for (i = 0; i < ifp->options->ia_len; i++) {
		if (iaid == NULL ||
		    memcmp(&ifp->options->ia[i].iaid, iaid,
		    sizeof(ifp->options->ia[i].iaid)) == 0)
		{
			for (j = 0; j < ifp->options->ia[i].sla_len; j++) {
				if (strcmp(ifp->options->ia[i].sla[j].ifname,
				    ifp->name) == 0)
					return &ifp->options->ia[i].sla[j];
			}
		}
	}
	return NULL;
}


#ifndef ffs32
static int
ffs32(uint32_t n)
{
	int v;

	if (!n)
		return 0;

	v = 1;
	if ((n & 0x0000FFFFU) == 0) {
		n >>= 16;
		v += 16;
	}
	if ((n & 0x000000FFU) == 0) {
		n >>= 8;
		v += 8;
	}
	if ((n & 0x0000000FU) == 0) {
		n >>= 4;
		v += 4;
	}
	if ((n & 0x00000003U) == 0) {
		n >>= 2;
		v += 2;
	}
	if ((n & 0x00000001U) == 0)
		v += 1;

	return v;
}
#endif

static int
dhcp6_delegateaddr(struct in6_addr *addr, struct interface *ifp,
    const struct ipv6_addr *prefix, const struct if_sla *sla, struct if_ia *ia)
{
	struct dhcp6_state *state;
	struct if_sla asla;
	char sabuf[INET6_ADDRSTRLEN];
	const char *sa;

	state = D6_STATE(ifp);
	if (state == NULL) {
		ifp->if_data[IF_DATA_DHCP6] = calloc(1, sizeof(*state));
		state = D6_STATE(ifp);
		if (state == NULL) {
			logger(ifp->ctx, LOG_ERR, "%s: %m", __func__);
			return -1;
		}

		TAILQ_INIT(&state->addrs);
		state->state = DH6S_DELEGATED;
		state->reason = "DELEGATED6";
	}

	if (sla == NULL || sla->sla_set == 0) {
		asla.sla = ifp->index;
		asla.prefix_len = 0;
		sla = &asla;
	} else if (sla->prefix_len == 0) {
		asla.sla = sla->sla;
		if (asla.sla == 0)
			asla.prefix_len = prefix->prefix_len;
		else
			asla.prefix_len = 0;
		sla = &asla;
	}
	if (sla->prefix_len == 0) {
		uint32_t sla_max;
		int bits;

		if (ia->sla_max == 0) {
			const struct interface *ifi;

			sla_max = 0;
			TAILQ_FOREACH(ifi, ifp->ctx->ifaces, next) {
				if (ifi != ifp && ifi->index > sla_max)
					sla_max = ifi->index;
			}
		} else
			sla_max = ia->sla_max;

		bits = ffs32(sla_max);

		if (prefix->prefix_len + bits > UINT8_MAX)
			asla.prefix_len = UINT8_MAX;
		else {
			asla.prefix_len = (uint8_t)(prefix->prefix_len + bits);

			/* Make a 64 prefix by default, as this maks SLAAC
			 * possible. Otherwise round up to the nearest octet. */
			if (asla.prefix_len <= 64)
				asla.prefix_len = 64;
			else
				asla.prefix_len = (uint8_t)ROUNDUP8(asla.prefix_len);

		}

#define BIT(n) (1l << (n))
#define BIT_MASK(len) (BIT(len) - 1)
		if (ia->sla_max == 0)
			/* Work out the real sla_max from our bits used */
			ia->sla_max = (uint32_t)BIT_MASK(asla.prefix_len -
			    prefix->prefix_len);
	}

	if (ipv6_userprefix(&prefix->prefix, prefix->prefix_len,
		sla->sla, addr, sla->prefix_len) == -1)
	{
		sa = inet_ntop(AF_INET6, &prefix->prefix,
		    sabuf, sizeof(sabuf));
		logger(ifp->ctx, LOG_ERR,
		    "%s: invalid prefix %s/%d + %d/%d: %m",
		    ifp->name, sa, prefix->prefix_len,
		    sla->sla, sla->prefix_len);
		return -1;
	}

	if (prefix->prefix_exclude_len &&
	    IN6_ARE_ADDR_EQUAL(addr, &prefix->prefix_exclude))
	{
		sa = inet_ntop(AF_INET6, &prefix->prefix_exclude,
		    sabuf, sizeof(sabuf));
		logger(ifp->ctx, LOG_ERR,
		    "%s: cannot delegate excluded prefix %s/%d",
		    ifp->name, sa, prefix->prefix_exclude_len);
		return -1;
	}

	return sla->prefix_len;
}

int
dhcp6_has_public_addr(const struct interface *ifp)
{
	const struct dhcp6_state *state = D6_CSTATE(ifp);
	const struct ipv6_addr *ia;

	if (state == NULL)
		return 0;
	TAILQ_FOREACH(ia, &state->addrs, next) {
		if (ipv6_publicaddr(ia))
			return 1;
	}
	return 0;
}

static int
dhcp6_makemessage(struct interface *ifp)
{
	struct dhcp6_state *state;
	struct dhcp6_message *m;
	struct dhcp6_option *o, *so, *eo;
	const struct dhcp6_option *si, *unicast;
	size_t l, n, len, ml;
	uint8_t u8, type;
	uint16_t u16, n_options, auth_len;
	struct if_options *ifo;
	const struct dhcp_opt *opt, *opt2;
	uint8_t IA, *p;
	const uint8_t *pp;
	uint32_t u32;
	const struct ipv6_addr *ap;
	char hbuf[HOSTNAME_MAX_LEN + 1];
	const char *hostname;
	int fqdn;
	struct dhcp6_ia_addr *iap;
	struct dhcp6_pd_addr *pdp;

	state = D6_STATE(ifp);
	if (state->send) {
		free(state->send);
		state->send = NULL;
	}

	ifo = ifp->options;
	fqdn = ifo->fqdn;

	if (fqdn == FQDN_DISABLE && ifo->options & DHCPCD_HOSTNAME) {
		/* We're sending the DHCPv4 hostname option, so send FQDN as
		 * DHCPv6 has no FQDN option and DHCPv4 must not send
		 * hostname and FQDN according to RFC4702 */
		fqdn = FQDN_BOTH;
	}
	if (fqdn != FQDN_DISABLE) {
		if (ifo->hostname[0] == '\0')
			hostname = get_hostname(hbuf, sizeof(hbuf),
			    ifo->options & DHCPCD_HOSTNAME_SHORT ? 1 : 0);
		else
			hostname = ifo->hostname;
	} else
		hostname = NULL; /* appearse gcc */

	/* Work out option size first */
	n_options = 0;
	len = 0;
	si = NULL;
	if (state->state != DH6S_RELEASE) {
		for (l = 0, opt = ifp->ctx->dhcp6_opts;
		    l < ifp->ctx->dhcp6_opts_len;
		    l++, opt++)
		{
			for (n = 0, opt2 = ifo->dhcp6_override;
			    n < ifo->dhcp6_override_len;
			    n++, opt2++)
			{
				if (opt->option == opt2->option)
					break;
			}
			if (n < ifo->dhcp6_override_len)
			    continue;
			if (!(opt->type & NOREQ) &&
			    (opt->type & REQUEST ||
			    has_option_mask(ifo->requestmask6, opt->option)))
			{
				n_options++;
				len += sizeof(u16);
			}
		}
		for (l = 0, opt = ifo->dhcp6_override;
		    l < ifo->dhcp6_override_len;
		    l++, opt++)
		{
			if (!(opt->type & NOREQ) &&
			    (opt->type & REQUEST ||
			    has_option_mask(ifo->requestmask6, opt->option)))
			{
				n_options++;
				len += sizeof(u16);
			}
		}
		if (dhcp6_findselfsla(ifp, NULL)) {
			n_options++;
			len += sizeof(u16);
		}
		if (len)
			len += sizeof(*o);

		if (fqdn != FQDN_DISABLE)
			len += sizeof(*o) + 1 + encode_rfc1035(hostname, NULL);

		if ((ifo->auth.options & DHCPCD_AUTH_SENDREQUIRE) !=
		    DHCPCD_AUTH_SENDREQUIRE)
			len += sizeof(*o); /* Reconfigure Accept */
	}

	len += sizeof(*state->send);
	len += sizeof(*o) + ifp->ctx->duid_len;
	len += sizeof(*o) + sizeof(uint16_t); /* elapsed */
	len += sizeof(*o) + dhcp6_makevendor(NULL, ifp);

	/* IA */
	m = NULL;
	ml = 0;
	switch(state->state) {
	case DH6S_REQUEST:
		m = state->recv;
		ml = state->recv_len;
		/* FALLTHROUGH */
	case DH6S_RELEASE:
		/* FALLTHROUGH */
	case DH6S_RENEW:
		if (m == NULL) {
			m = state->new;
			ml = state->new_len;
		}
		si = dhcp6_getmoption(D6_OPTION_SERVERID, m, ml);
		if (si == NULL) {
			errno = ESRCH;
			return -1;
		}
		len += sizeof(*si) + ntohs(si->len);
		/* FALLTHROUGH */
	case DH6S_REBIND:
		/* FALLTHROUGH */
	case DH6S_CONFIRM:
		/* FALLTHROUGH */
	case DH6S_DISCOVER:
		if (m == NULL) {
			m = state->new;
			ml = state->new_len;
		}
		TAILQ_FOREACH(ap, &state->addrs, next) {
			if (ap->prefix_vltime == 0 &&
			    !(ap->flags & IPV6_AF_REQUEST))
				continue;
			if (ap->ia_type == D6_OPTION_IA_PD) {
				len += sizeof(*o) + sizeof(u8) +
				    sizeof(u32) + sizeof(u32) +
				    sizeof(ap->prefix);
				if (ap->prefix_exclude_len)
					len += sizeof(*o) + 1 +
					    (uint8_t)((ap->prefix_exclude_len -
					    ap->prefix_len - 1) / NBBY) + 1;
			} else
				len += sizeof(*o) + sizeof(ap->addr) +
				    sizeof(u32) + sizeof(u32);
		}
		/* FALLTHROUGH */
	case DH6S_INIT:
		len += ifo->ia_len * (sizeof(*o) + (sizeof(u32) * 3));
		IA = 1;
		break;
	default:
		IA = 0;
	}

	if (state->state == DH6S_DISCOVER &&
	    !(ifp->ctx->options & DHCPCD_TEST) &&
	    has_option_mask(ifo->requestmask6, D6_OPTION_RAPID_COMMIT))
		len += sizeof(*o);

	if (m == NULL) {
		m = state->new;
		ml = state->new_len;
	}
	unicast = NULL;
	/* Depending on state, get the unicast address */
	switch(state->state) {
		break;
	case DH6S_INIT: /* FALLTHROUGH */
	case DH6S_DISCOVER:
		type = DHCP6_SOLICIT;
		break;
	case DH6S_REQUEST:
		type = DHCP6_REQUEST;
		unicast = dhcp6_getmoption(D6_OPTION_UNICAST, m, ml);
		break;
	case DH6S_CONFIRM:
		type = DHCP6_CONFIRM;
		break;
	case DH6S_REBIND:
		type = DHCP6_REBIND;
		break;
	case DH6S_RENEW:
		type = DHCP6_RENEW;
		unicast = dhcp6_getmoption(D6_OPTION_UNICAST, m, ml);
		break;
	case DH6S_INFORM:
		type = DHCP6_INFORMATION_REQ;
		break;
	case DH6S_RELEASE:
		type = DHCP6_RELEASE;
		unicast = dhcp6_getmoption(D6_OPTION_UNICAST, m, ml);
		break;
	default:
		errno = EINVAL;
		return -1;
	}

	auth_len = 0;
	if (ifo->auth.options & DHCPCD_AUTH_SEND) {
		ssize_t alen = dhcp_auth_encode(&ifo->auth,
		    state->auth.token, NULL, 0, 6, type, NULL, 0);
		if (alen != -1 && alen > UINT16_MAX) {
			errno = ERANGE;
			alen = -1;
		}
		if (alen == -1)
			logger(ifp->ctx, LOG_ERR,
			    "%s: dhcp_auth_encode: %m", ifp->name);
		else if (alen != 0) {
			auth_len = (uint16_t)alen;
			len += sizeof(*o) + auth_len;
		}
	}

	state->send = malloc(len);
	if (state->send == NULL)
		return -1;

	state->send_len = len;
	state->send->type = type;

	/* If we found a unicast option, copy it to our state for sending */
	if (unicast && ntohs(unicast->len) == sizeof(state->unicast))
		memcpy(&state->unicast, D6_COPTION_DATA(unicast),
		    sizeof(state->unicast));
	else
		state->unicast = in6addr_any;

	dhcp6_newxid(ifp, state->send);

	o = D6_FIRST_OPTION(state->send);
	o->code = htons(D6_OPTION_CLIENTID);
	o->len = htons((uint16_t)ifp->ctx->duid_len);
	memcpy(D6_OPTION_DATA(o), ifp->ctx->duid, ifp->ctx->duid_len);

	if (si) {
		o = D6_NEXT_OPTION(o);
		memcpy(o, si, sizeof(*si) + ntohs(si->len));
	}

	o = D6_NEXT_OPTION(o);
	o->code = htons(D6_OPTION_ELAPSED);
	o->len = htons(sizeof(uint16_t));
	p = D6_OPTION_DATA(o);
	memset(p, 0, sizeof(uint16_t));

	o = D6_NEXT_OPTION(o);
	dhcp6_makevendor(o, ifp);

	if (state->state == DH6S_DISCOVER &&
	    !(ifp->ctx->options & DHCPCD_TEST) &&
	    has_option_mask(ifo->requestmask6, D6_OPTION_RAPID_COMMIT))
	{
		o = D6_NEXT_OPTION(o);
		o->code = htons(D6_OPTION_RAPID_COMMIT);
		o->len = 0;
	}

	for (l = 0; IA && l < ifo->ia_len; l++) {
		o = D6_NEXT_OPTION(o);
		o->code = htons(ifo->ia[l].ia_type);
		o->len = htons(sizeof(u32) + sizeof(u32) + sizeof(u32));
		p = D6_OPTION_DATA(o);
		memcpy(p, ifo->ia[l].iaid, sizeof(u32));
		p += sizeof(u32);
		memset(p, 0, sizeof(u32) + sizeof(u32));
		TAILQ_FOREACH(ap, &state->addrs, next) {
			if (ap->prefix_vltime == 0 &&
			    !(ap->flags & IPV6_AF_REQUEST))
				continue;
			if (memcmp(ifo->ia[l].iaid, ap->iaid, sizeof(u32)))
				continue;
			so = D6_NEXT_OPTION(o);
			if (ap->ia_type == D6_OPTION_IA_PD) {
				so->code = htons(D6_OPTION_IAPREFIX);
				so->len = htons(sizeof(ap->prefix) +
				    sizeof(u32) + sizeof(u32) + sizeof(u8));
				pdp = (struct dhcp6_pd_addr *)
				    D6_OPTION_DATA(so);
				pdp->pltime = htonl(ap->prefix_pltime);
				pdp->vltime = htonl(ap->prefix_vltime);
				pdp->prefix_len = ap->prefix_len;
				pdp->prefix = ap->prefix;

				/* RFC6603 Section 4.2 */
				if (ap->prefix_exclude_len) {
					n = (size_t)((ap->prefix_exclude_len -
					    ap->prefix_len - 1) / NBBY) + 1;
					eo = D6_NEXT_OPTION(so);
					eo->code = htons(D6_OPTION_PD_EXCLUDE);
					eo->len = (uint16_t)(n + 1);
					p = D6_OPTION_DATA(eo);
					*p++ = (uint8_t)ap->prefix_exclude_len;
					pp = ap->prefix_exclude.s6_addr;
					pp += (size_t)((ap->prefix_len - 1) / NBBY)
					    + (n - 1);
					u8 = ap->prefix_len % NBBY;
					if (u8)
						n--;
					while (n-- > 0)
						*p++ = *pp--;
					if (u8)
						*p = (uint8_t)(*pp << u8);
					u16 = (uint16_t)(ntohs(so->len) +
					    sizeof(*eo) + eo->len);
					so->len = htons(u16);
					eo->len = htons(eo->len);
				}

				u16 = (uint16_t)(ntohs(o->len) + sizeof(*so)
				    + ntohs(so->len));
				o->len = htons(u16);
			} else {
				so->code = htons(D6_OPTION_IA_ADDR);
				so->len = sizeof(ap->addr) +
				    sizeof(u32) + sizeof(u32);
				iap = (struct dhcp6_ia_addr *)
				    D6_OPTION_DATA(so);
				iap->addr = ap->addr;
				iap->pltime = htonl(ap->prefix_pltime);
				iap->vltime = htonl(ap->prefix_vltime);
				u16 = (uint16_t)(ntohs(o->len) + sizeof(*so)
				    + so->len);
				so->len = htons(so->len);
				o->len = htons(u16);
			}
		}
	}

	if (state->send->type != DHCP6_RELEASE) {
		if (fqdn != FQDN_DISABLE) {
			o = D6_NEXT_OPTION(o);
			o->code = htons(D6_OPTION_FQDN);
			p = D6_OPTION_DATA(o);
			switch (fqdn) {
			case FQDN_BOTH:
				*p = D6_FQDN_BOTH;
				break;
			case FQDN_PTR:
				*p = D6_FQDN_PTR;
				break;
			default:
				*p = D6_FQDN_NONE;
				break;
			}
			l = encode_rfc1035(hostname, p + 1);
			if (l == 0)
				*p = D6_FQDN_NONE;
			o->len = htons((uint16_t)(l + 1));
		}

		if ((ifo->auth.options & DHCPCD_AUTH_SENDREQUIRE) !=
		    DHCPCD_AUTH_SENDREQUIRE)
		{
			o = D6_NEXT_OPTION(o);
			o->code = htons(D6_OPTION_RECONF_ACCEPT);
			o->len = 0;
		}

		if (n_options) {
			o = D6_NEXT_OPTION(o);
			o->code = htons(D6_OPTION_ORO);
			o->len = 0;
			p = D6_OPTION_DATA(o);
			for (l = 0, opt = ifp->ctx->dhcp6_opts;
			    l < ifp->ctx->dhcp6_opts_len;
			    l++, opt++)
			{
				for (n = 0, opt2 = ifo->dhcp6_override;
				    n < ifo->dhcp6_override_len;
				    n++, opt2++)
				{
					if (opt->option == opt2->option)
						break;
				}
				if (n < ifo->dhcp6_override_len)
				    continue;
				if (!(opt->type & NOREQ) &&
				    (opt->type & REQUEST ||
				    has_option_mask(ifo->requestmask6,
				        opt->option)))
				{
					u16 = htons((uint16_t)opt->option);
					memcpy(p, &u16, sizeof(u16));
					p += sizeof(u16);
					o->len = (uint16_t)(o->len + sizeof(u16));
				}
			}
			for (l = 0, opt = ifo->dhcp6_override;
			    l < ifo->dhcp6_override_len;
			    l++, opt++)
			{
				if (!(opt->type & NOREQ) &&
				    (opt->type & REQUEST ||
				    has_option_mask(ifo->requestmask6,
				        opt->option)))
				{
					u16 = htons((uint16_t)opt->option);
					memcpy(p, &u16, sizeof(u16));
					p += sizeof(u16);
					o->len = (uint16_t)(o->len + sizeof(u16));
				}
			}
			if (dhcp6_findselfsla(ifp, NULL)) {
				u16 = htons(D6_OPTION_PD_EXCLUDE);
				memcpy(p, &u16, sizeof(u16));
				o->len = (uint16_t)(o->len + sizeof(u16));
			}
			o->len = htons(o->len);
		}
	}

	/* This has to be the last option */
	if (ifo->auth.options & DHCPCD_AUTH_SEND && auth_len != 0) {
		o = D6_NEXT_OPTION(o);
		o->code = htons(D6_OPTION_AUTH);
		o->len = htons((uint16_t)auth_len);
		/* data will be filled at send message time */
	}

	return 0;
}

static const char *
dhcp6_get_op(uint16_t type)
{
	const struct dhcp6_op *d;

	for (d = dhcp6_ops; d->name; d++)
		if (d->type == type)
			return d->name;
	return NULL;
}

static void
dhcp6_freedrop_addrs(struct interface *ifp, int drop,
    const struct interface *ifd)
{
	struct dhcp6_state *state;

	state = D6_STATE(ifp);
	if (state) {
		ipv6_freedrop_addrs(&state->addrs, drop, ifd);
		if (drop)
			ipv6_buildroutes(ifp->ctx);
	}
}

static void dhcp6_delete_delegates(struct interface *ifp)
{
	struct interface *ifp0;

	if (ifp->ctx->ifaces) {
		TAILQ_FOREACH(ifp0, ifp->ctx->ifaces, next) {
			if (ifp0 != ifp)
				dhcp6_freedrop_addrs(ifp0, 1, ifp);
		}
	}
}

static ssize_t
dhcp6_update_auth(struct interface *ifp, struct dhcp6_message *m, size_t len)
{
	struct dhcp6_state *state;
	const struct dhcp6_option *co;
	struct dhcp6_option *o;

	co = dhcp6_getmoption(D6_OPTION_AUTH, m, len);
	if (co == NULL)
		return -1;

	o = __UNCONST(co);
	state = D6_STATE(ifp);

	return dhcp_auth_encode(&ifp->options->auth, state->auth.token,
	    (uint8_t *)state->send, state->send_len,
	    6, state->send->type,
	    D6_OPTION_DATA(o), ntohs(o->len));
}

static int
dhcp6_sendmessage(struct interface *ifp, void (*callback)(void *))
{
	struct dhcp6_state *state;
	struct ipv6_ctx *ctx;
	struct sockaddr_in6 dst;
	struct cmsghdr *cm;
	struct in6_pktinfo pi;
	struct timespec RTprev;
	double rnd;
	time_t ms;
	uint8_t neg;
	const char *broad_uni;
	const struct in6_addr alldhcp = IN6ADDR_LINKLOCAL_ALLDHCP_INIT;

	if (!callback && ifp->carrier == LINK_DOWN)
		return 0;

	memset(&dst, 0, sizeof(dst));
	dst.sin6_family = AF_INET6;
	dst.sin6_port = htons(DHCP6_SERVER_PORT);
#ifdef SIN6_LEN
	dst.sin6_len = sizeof(dst);
#endif

	state = D6_STATE(ifp);
	/* We need to ensure we have sufficient scope to unicast the address */
	/* XXX FIXME: We should check any added addresses we have like from
	 * a Router Advertisement */
	if (IN6_IS_ADDR_UNSPECIFIED(&state->unicast) ||
	    (state->state == DH6S_REQUEST &&
	    (!IN6_IS_ADDR_LINKLOCAL(&state->unicast) || !ipv6_linklocal(ifp))))
	{
		dst.sin6_addr = alldhcp;
		broad_uni = "broadcasting";
	} else {
		dst.sin6_addr = state->unicast;
		broad_uni = "unicasting";
	}

	if (!callback)
		logger(ifp->ctx, LOG_DEBUG,
		    "%s: %s %s with xid 0x%02x%02x%02x",
		    ifp->name,
		    broad_uni,
		    dhcp6_get_op(state->send->type),
		    state->send->xid[0],
		    state->send->xid[1],
		    state->send->xid[2]);
	else {
		if (state->IMD &&
		    !(ifp->options->options & DHCPCD_INITIAL_DELAY))
			state->IMD = 0;
		if (state->IMD) {
			/* Some buggy PPP servers close the link too early
			 * after sending an invalid status in their reply
			 * which means this host won't see it.
			 * 1 second grace seems to be the sweet spot. */
			if (ifp->flags & IFF_POINTOPOINT)
				state->RT.tv_sec = 1;
			else
				state->RT.tv_sec = 0;
			state->RT.tv_nsec = (suseconds_t)arc4random_uniform(
			    (uint32_t)(state->IMD * NSEC_PER_SEC));
			timespecnorm(&state->RT);
			broad_uni = "delaying";
			goto logsend;
		}
		if (state->RTC == 0) {
			RTprev.tv_sec = state->IRT;
			RTprev.tv_nsec = 0;
			state->RT.tv_sec = RTprev.tv_sec;
			state->RT.tv_nsec = 0;
		} else {
			RTprev = state->RT;
			timespecadd(&state->RT, &state->RT, &state->RT);
		}

		rnd = DHCP6_RAND_MIN;
		rnd += (suseconds_t)arc4random_uniform(
		    DHCP6_RAND_MAX - DHCP6_RAND_MIN);
		rnd /= MSEC_PER_SEC;
		neg = (rnd < 0.0);
		if (neg)
			rnd = -rnd;
		ts_to_ms(ms, &RTprev);
		ms = (time_t)((double)ms * rnd);
		ms_to_ts(&RTprev, ms);
		if (neg)
			timespecsub(&state->RT, &RTprev, &state->RT);
		else
			timespecadd(&state->RT, &RTprev, &state->RT);

		if (state->MRT != 0 && state->RT.tv_sec > state->MRT) {
			RTprev.tv_sec = state->MRT;
			RTprev.tv_nsec = 0;
			state->RT.tv_sec = state->MRT;
			state->RT.tv_nsec = 0;
			ts_to_ms(ms, &RTprev);
			ms = (time_t)((double)ms * rnd);
			ms_to_ts(&RTprev, ms);
			if (neg)
				timespecsub(&state->RT, &RTprev, &state->RT);
			else
				timespecadd(&state->RT, &RTprev, &state->RT);
		}

logsend:
		if (ifp->carrier != LINK_DOWN)
			logger(ifp->ctx, LOG_DEBUG,
			    "%s: %s %s (xid 0x%02x%02x%02x),"
			    " next in %0.1f seconds",
			    ifp->name,
			    broad_uni,
			    dhcp6_get_op(state->send->type),
			    state->send->xid[0],
			    state->send->xid[1],
			    state->send->xid[2],
			    timespec_to_double(&state->RT));

		/* This sometimes happens when we delegate to this interface
		 * AND run DHCPv6 on it normally. */
		assert(timespec_to_double(&state->RT) != 0);

		/* Wait the initial delay */
		if (state->IMD != 0) {
			state->IMD = 0;
			eloop_timeout_add_tv(ifp->ctx->eloop,
			    &state->RT, callback, ifp);
			return 0;
		}
	}

	if (ifp->carrier == LINK_DOWN)
		return 0;

	/* Update the elapsed time */
	dhcp6_updateelapsed(ifp, state->send, state->send_len);
	if (ifp->options->auth.options & DHCPCD_AUTH_SEND &&
	    dhcp6_update_auth(ifp, state->send, state->send_len) == -1)
	{
		logger(ifp->ctx, LOG_ERR,
		    "%s: dhcp6_updateauth: %m", ifp->name);
		if (errno != ESRCH)
			return -1;
	}

	ctx = ifp->ctx->ipv6;
	dst.sin6_scope_id = ifp->index;
	ctx->sndhdr.msg_name = (void *)&dst;
	ctx->sndhdr.msg_iov[0].iov_base = state->send;
	ctx->sndhdr.msg_iov[0].iov_len = state->send_len;

	/* Set the outbound interface */
	cm = CMSG_FIRSTHDR(&ctx->sndhdr);
	if (cm == NULL) /* unlikely */
		return -1;
	cm->cmsg_level = IPPROTO_IPV6;
	cm->cmsg_type = IPV6_PKTINFO;
	cm->cmsg_len = CMSG_LEN(sizeof(pi));
	memset(&pi, 0, sizeof(pi));
	pi.ipi6_ifindex = ifp->index;
	memcpy(CMSG_DATA(cm), &pi, sizeof(pi));

	if (sendmsg(ctx->dhcp_fd, &ctx->sndhdr, 0) == -1) {
		logger(ifp->ctx, LOG_ERR,
		    "%s: %s: sendmsg: %m", ifp->name, __func__);
		ifp->options->options &= ~DHCPCD_IPV6;
		dhcp6_drop(ifp, "EXPIRE6");
		return -1;
	}

	state->RTC++;
	if (callback) {
		if (state->MRC == 0 || state->RTC < state->MRC)
			eloop_timeout_add_tv(ifp->ctx->eloop,
			    &state->RT, callback, ifp);
		else if (state->MRC != 0 && state->MRCcallback)
			eloop_timeout_add_tv(ifp->ctx->eloop,
			    &state->RT, state->MRCcallback, ifp);
		else
			logger(ifp->ctx, LOG_WARNING,
			    "%s: sent %d times with no reply",
			    ifp->name, state->RTC);
	}
	return 0;
}

static void
dhcp6_sendinform(void *arg)
{

	dhcp6_sendmessage(arg, dhcp6_sendinform);
}

static void
dhcp6_senddiscover(void *arg)
{

	dhcp6_sendmessage(arg, dhcp6_senddiscover);
}

static void
dhcp6_sendrequest(void *arg)
{

	dhcp6_sendmessage(arg, dhcp6_sendrequest);
}

static void
dhcp6_sendrebind(void *arg)
{

	dhcp6_sendmessage(arg, dhcp6_sendrebind);
}

static void
dhcp6_sendrenew(void *arg)
{

	dhcp6_sendmessage(arg, dhcp6_sendrenew);
}

static void
dhcp6_sendconfirm(void *arg)
{

	dhcp6_sendmessage(arg, dhcp6_sendconfirm);
}

static void
dhcp6_sendrelease(void *arg)
{

	dhcp6_sendmessage(arg, dhcp6_sendrelease);
}

static void
dhcp6_startrenew(void *arg)
{
	struct interface *ifp;
	struct dhcp6_state *state;

	ifp = arg;
	state = D6_STATE(ifp);
	state->state = DH6S_RENEW;
	state->RTC = 0;
	state->IRT = REN_TIMEOUT;
	state->MRT = REN_MAX_RT;
	state->MRC = 0;

	if (dhcp6_makemessage(ifp) == -1)
		logger(ifp->ctx, LOG_ERR,
		    "%s: dhcp6_makemessage: %m", ifp->name);
	else
		dhcp6_sendrenew(ifp);
}

int
dhcp6_dadcompleted(const struct interface *ifp)
{
	const struct dhcp6_state *state;
	const struct ipv6_addr *ap;

	state = D6_CSTATE(ifp);
	TAILQ_FOREACH(ap, &state->addrs, next) {
		if (ap->flags & IPV6_AF_ADDED &&
		    !(ap->flags & IPV6_AF_DADCOMPLETED))
			return 0;
	}
	return 1;
}

static void
dhcp6_dadcallback(void *arg)
{
	struct ipv6_addr *ap = arg;
	struct interface *ifp;
	struct dhcp6_state *state;
	int wascompleted, valid;

	wascompleted = (ap->flags & IPV6_AF_DADCOMPLETED);
	ap->flags |= IPV6_AF_DADCOMPLETED;
	if (ap->flags & IPV6_AF_DUPLICATED)
		/* XXX FIXME
		 * We should decline the address */
		logger(ap->iface->ctx, LOG_WARNING, "%s: DAD detected %s",
		    ap->iface->name, ap->saddr);

	if (!wascompleted) {
		ifp = ap->iface;
		state = D6_STATE(ifp);
		if (state->state == DH6S_BOUND ||
		    state->state == DH6S_DELEGATED)
		{
			struct ipv6_addr *ap2;

			valid = (ap->delegating_iface == NULL);
			TAILQ_FOREACH(ap2, &state->addrs, next) {
				if (ap2->flags & IPV6_AF_ADDED &&
				    !(ap2->flags & IPV6_AF_DADCOMPLETED))
				{
					wascompleted = 1;
					break;
				}
			}
			if (!wascompleted) {
				logger(ap->iface->ctx, LOG_DEBUG,
				    "%s: DHCPv6 DAD completed", ifp->name);
				script_runreason(ifp,
				    ap->delegating_iface ?
				    "DELEGATED6" : state->reason);
				if (valid)
					dhcpcd_daemonise(ifp->ctx);
			}
		}
	}
}

static void
dhcp6_addrequestedaddrs(struct interface *ifp)
{
	struct dhcp6_state *state;
	size_t i;
	struct if_ia *ia;
	struct ipv6_addr *a;
	char iabuf[INET6_ADDRSTRLEN];
	const char *iap;

	state = D6_STATE(ifp);
	/* Add any requested prefixes / addresses */
	for (i = 0; i < ifp->options->ia_len; i++) {
		ia = &ifp->options->ia[i];
		if (!((ia->ia_type == D6_OPTION_IA_PD && ia->prefix_len) ||
		    !IN6_IS_ADDR_UNSPECIFIED(&ia->addr)))
			continue;
		a = calloc(1, sizeof(*a));
		if (a == NULL) {
			logger(ifp->ctx, LOG_ERR, "%s: %m", __func__);
			return;
		}
		a->flags = IPV6_AF_REQUEST;
		a->iface = ifp;
		a->dadcallback = dhcp6_dadcallback;
		memcpy(&a->iaid, &ia->iaid, sizeof(a->iaid));
		a->ia_type = ia->ia_type;
		//a->prefix_pltime = 0;
		//a->prefix_vltime = 0;

		if (ia->ia_type == D6_OPTION_IA_PD) {
			memcpy(&a->prefix, &ia->addr, sizeof(a->addr));
			a->prefix_len = ia->prefix_len;
			iap = inet_ntop(AF_INET6, &a->prefix,
			    iabuf, sizeof(iabuf));
		} else {
			memcpy(&a->addr, &ia->addr, sizeof(a->addr));
			/*
			 * RFC 5942 Section 5
			 * We cannot assume any prefix length, nor tie the
			 * address to an existing one as it could expire
			 * before the address.
			 * As such we just give it a 128 prefix.
			 */
			a->prefix_len = 128;
			ipv6_makeprefix(&a->prefix, &a->addr, a->prefix_len);
			iap = inet_ntop(AF_INET6, &a->addr,
			    iabuf, sizeof(iabuf));
		}
		snprintf(a->saddr, sizeof(a->saddr),
		    "%s/%d", iap, a->prefix_len);
		TAILQ_INSERT_TAIL(&state->addrs, a, next);
	}
}

static void
dhcp6_startdiscover(void *arg)
{
	struct interface *ifp;
	struct dhcp6_state *state;

	ifp = arg;
	dhcp6_delete_delegates(ifp);
	logger(ifp->ctx, LOG_INFO, "%s: soliciting a DHCPv6 lease", ifp->name);
	state = D6_STATE(ifp);
	state->state = DH6S_DISCOVER;
	state->RTC = 0;
	state->IMD = SOL_MAX_DELAY;
	state->IRT = SOL_TIMEOUT;
	state->MRT = state->sol_max_rt;
	state->MRC = 0;

	eloop_timeout_delete(ifp->ctx->eloop, NULL, ifp);
	free(state->new);
	state->new = NULL;
	state->new_len = 0;

	dhcp6_freedrop_addrs(ifp, 0, NULL);
	unlink(state->leasefile);

	dhcp6_addrequestedaddrs(ifp);

	if (dhcp6_makemessage(ifp) == -1)
		logger(ifp->ctx, LOG_ERR,
		    "%s: dhcp6_makemessage: %m", ifp->name);
	else
		dhcp6_senddiscover(ifp);
}

static void
dhcp6_failconfirm(void *arg)
{
	struct interface *ifp;

	ifp = arg;
	logger(ifp->ctx, LOG_ERR,
	    "%s: failed to confirm prior address", ifp->name);
	/* Section 18.1.2 says that we SHOULD use the last known
	 * IP address(s) and lifetimes if we didn't get a reply.
	 * I disagree with this. */
	dhcp6_startdiscover(ifp);
}

static void
dhcp6_failrequest(void *arg)
{
	struct interface *ifp;

	ifp = arg;
	logger(ifp->ctx, LOG_ERR, "%s: failed to request address", ifp->name);
	/* Section 18.1.1 says that client local policy dictates
	 * what happens if a REQUEST fails.
	 * Of the possible scenarios listed, moving back to the
	 * DISCOVER phase makes more sense for us. */
	dhcp6_startdiscover(ifp);
}

static void
dhcp6_failrebind(void *arg)
{
	struct interface *ifp;

	ifp = arg;
	logger(ifp->ctx, LOG_ERR,
	    "%s: failed to rebind prior delegation", ifp->name);
	dhcp6_delete_delegates(ifp);
	/* Section 18.1.2 says that we SHOULD use the last known
	 * IP address(s) and lifetimes if we didn't get a reply.
	 * I disagree with this. */
	dhcp6_startdiscover(ifp);
}


static int
dhcp6_hasprefixdelegation(struct interface *ifp)
{
	size_t i;
	uint16_t t;

	t = 0;
	for (i = 0; i < ifp->options->ia_len; i++) {
		if (t && t != ifp->options->ia[i].ia_type) {
			if (t == D6_OPTION_IA_PD ||
			    ifp->options->ia[i].ia_type == D6_OPTION_IA_PD)
				return 2;
		}
		t = ifp->options->ia[i].ia_type;
	}
	return t == D6_OPTION_IA_PD ? 1 : 0;
}

static void
dhcp6_startrebind(void *arg)
{
	struct interface *ifp;
	struct dhcp6_state *state;
	int pd;

	ifp = arg;
	eloop_timeout_delete(ifp->ctx->eloop, dhcp6_sendrenew, ifp);
	state = D6_STATE(ifp);
	if (state->state == DH6S_RENEW)
		logger(ifp->ctx, LOG_WARNING,
		    "%s: failed to renew DHCPv6, rebinding", ifp->name);
	else
		logger(ifp->ctx, LOG_INFO,
		    "%s: rebinding prior DHCPv6 lease", ifp->name);
	state->state = DH6S_REBIND;
	state->RTC = 0;
	state->MRC = 0;

	/* RFC 3633 section 12.1 */
	pd = dhcp6_hasprefixdelegation(ifp);
	if (pd) {
		state->IMD = CNF_MAX_DELAY;
		state->IRT = CNF_TIMEOUT;
		state->MRT = CNF_MAX_RT;
	} else {
		state->IRT = REB_TIMEOUT;
		state->MRT = REB_MAX_RT;
	}

	if (dhcp6_makemessage(ifp) == -1)
		logger(ifp->ctx, LOG_ERR,
		    "%s: dhcp6_makemessage: %m", ifp->name);
	else
		dhcp6_sendrebind(ifp);

	/* RFC 3633 section 12.1 */
	if (pd)
		eloop_timeout_add_sec(ifp->ctx->eloop,
		    CNF_MAX_RD, dhcp6_failrebind, ifp);
}


static void
dhcp6_startrequest(struct interface *ifp)
{
	struct dhcp6_state *state;

	eloop_timeout_delete(ifp->ctx->eloop, dhcp6_senddiscover, ifp);
	state = D6_STATE(ifp);
	state->state = DH6S_REQUEST;
	state->RTC = 0;
	state->IRT = REQ_TIMEOUT;
	state->MRT = REQ_MAX_RT;
	state->MRC = REQ_MAX_RC;
	state->MRCcallback = dhcp6_failrequest;

	if (dhcp6_makemessage(ifp) == -1) {
		logger(ifp->ctx, LOG_ERR,
		    "%s: dhcp6_makemessage: %m", ifp->name);
		return;
	}

	dhcp6_sendrequest(ifp);
}

static void
dhcp6_startconfirm(struct interface *ifp)
{
	struct dhcp6_state *state;

	state = D6_STATE(ifp);
	state->state = DH6S_CONFIRM;
	state->RTC = 0;
	state->IMD = CNF_MAX_DELAY;
	state->IRT = CNF_TIMEOUT;
	state->MRT = CNF_MAX_RT;
	state->MRC = 0;

	logger(ifp->ctx, LOG_INFO,
	    "%s: confirming prior DHCPv6 lease", ifp->name);
	if (dhcp6_makemessage(ifp) == -1) {
		logger(ifp->ctx, LOG_ERR,
		    "%s: dhcp6_makemessage: %m", ifp->name);
		return;
	}
	dhcp6_sendconfirm(ifp);
	eloop_timeout_add_sec(ifp->ctx->eloop,
	    CNF_MAX_RD, dhcp6_failconfirm, ifp);
}

static void
dhcp6_startinform(void *arg)
{
	struct interface *ifp;
	struct dhcp6_state *state;

	ifp = arg;
	state = D6_STATE(ifp);
	if (state->new == NULL || ifp->options->options & DHCPCD_DEBUG)
		logger(ifp->ctx, LOG_INFO,
		    "%s: requesting DHCPv6 information", ifp->name);
	state->state = DH6S_INFORM;
	state->RTC = 0;
	state->IMD = INF_MAX_DELAY;
	state->IRT = INF_TIMEOUT;
	state->MRT = state->inf_max_rt;
	state->MRC = 0;

	if (dhcp6_makemessage(ifp) == -1)
		logger(ifp->ctx, LOG_ERR,
		    "%s: dhcp6_makemessage: %m", ifp->name);
	else
		dhcp6_sendinform(ifp);
}

static void
dhcp6_startexpire(void *arg)
{
	struct interface *ifp;

	ifp = arg;
	eloop_timeout_delete(ifp->ctx->eloop, dhcp6_sendrebind, ifp);

	logger(ifp->ctx, LOG_ERR, "%s: DHCPv6 lease expired", ifp->name);
	dhcp6_freedrop_addrs(ifp, 1, NULL);
	dhcp6_delete_delegates(ifp);
	script_runreason(ifp, "EXPIRE6");
	if (ipv6nd_hasradhcp(ifp) || dhcp6_hasprefixdelegation(ifp))
		dhcp6_startdiscover(ifp);
	else
		logger(ifp->ctx, LOG_WARNING,
		    "%s: no advertising IPv6 router wants DHCP", ifp->name);
}

static void
dhcp6_finishrelease(void *arg)
{
	struct interface *ifp;
	struct dhcp6_state *state;

	ifp = (struct interface *)arg;
	state = D6_STATE(ifp);
	state->state = DH6S_RELEASED;
	dhcp6_drop(ifp, "RELEASE6");
}

static void
dhcp6_startrelease(struct interface *ifp)
{
	struct dhcp6_state *state;

	state = D6_STATE(ifp);
	if (state->state != DH6S_BOUND)
		return;

	state->state = DH6S_RELEASE;
	state->RTC = 0;
	state->IRT = REL_TIMEOUT;
	state->MRT = 0;
	/* MRC of REL_MAX_RC is optional in RFC 3315 18.1.6 */
#if 0
	state->MRC = REL_MAX_RC;
	state->MRCcallback = dhcp6_finishrelease;
#else
	state->MRC = 0;
	state->MRCcallback = NULL;
#endif

	if (dhcp6_makemessage(ifp) == -1)
		logger(ifp->ctx, LOG_ERR,
		    "%s: dhcp6_makemessage: %m", ifp->name);
	else {
		dhcp6_sendrelease(ifp);
		dhcp6_finishrelease(ifp);
	}
}

static int
dhcp6_checkstatusok(const struct interface *ifp,
    const struct dhcp6_message *m, const uint8_t *p, size_t len)
{
	const struct dhcp6_option *o;
	uint16_t code;
	char *status;

	if (p)
		o = dhcp6_findoption(D6_OPTION_STATUS_CODE, p, len);
	else
		o = dhcp6_getmoption(D6_OPTION_STATUS_CODE, m, len);
	if (o == NULL) {
		//logger(ifp->ctx, LOG_DEBUG, "%s: no status", ifp->name);
		return 0;
	}

	len = ntohs(o->len);
	if (len < sizeof(code)) {
		logger(ifp->ctx, LOG_ERR, "%s: status truncated", ifp->name);
		return -1;
	}

	p = D6_COPTION_DATA(o);
	memcpy(&code, p, sizeof(code));
	code = ntohs(code);
	if (code == D6_STATUS_OK)
		return 1;

	len -= sizeof(code);

	if (len == 0) {
		if (code < sizeof(dhcp6_statuses) / sizeof(char *)) {
			p = (const uint8_t *)dhcp6_statuses[code];
			len = strlen((const char *)p);
		} else
			p = NULL;
	} else
		p += sizeof(code);

	status = malloc(len + 1);
	if (status == NULL) {
		logger(ifp->ctx, LOG_ERR, "%s: %m", __func__);
		return -1;
	}
	if (p)
		memcpy(status, p, len);
	status[len] = '\0';
	logger(ifp->ctx, LOG_ERR, "%s: DHCPv6 REPLY: %s", ifp->name, status);
	free(status);
	return -1;
}

const struct ipv6_addr *
dhcp6_iffindaddr(const struct interface *ifp, const struct in6_addr *addr,
    short flags)
{
	const struct dhcp6_state *state;
	const struct ipv6_addr *ap;

	if ((state = D6_STATE(ifp)) != NULL) {
		TAILQ_FOREACH(ap, &state->addrs, next) {
			if (ipv6_findaddrmatch(ap, addr, flags))
				return ap;
		}
	}
	return NULL;
}

struct ipv6_addr *
dhcp6_findaddr(struct dhcpcd_ctx *ctx, const struct in6_addr *addr,
    short flags)
{
	struct interface *ifp;
	struct ipv6_addr *ap;
	struct dhcp6_state *state;

	TAILQ_FOREACH(ifp, ctx->ifaces, next) {
		if ((state = D6_STATE(ifp)) != NULL) {
			TAILQ_FOREACH(ap, &state->addrs, next) {
				if (ipv6_findaddrmatch(ap, addr, flags))
					return ap;
			}
		}
	}
	return NULL;
}

static int
dhcp6_findna(struct interface *ifp, uint16_t ot, const uint8_t *iaid,
    const uint8_t *d, size_t l, const struct timespec *acquired)
{
	struct dhcp6_state *state;
	const struct dhcp6_option *o;
	struct ipv6_addr *a;
	char iabuf[INET6_ADDRSTRLEN];
	const char *ia;
	int i;
	uint32_t u32;
	size_t off;
	const struct dhcp6_ia_addr *iap;

	i = 0;
	state = D6_STATE(ifp);
	while ((o = dhcp6_findoption(D6_OPTION_IA_ADDR, d, l))) {
		off = (size_t)((const uint8_t *)o - d);
		l -= off;
		d += off;
		u32 = ntohs(o->len);
		l -= sizeof(*o) + u32;
		d += sizeof(*o) + u32;
		if (u32 < 24) {
			errno = EINVAL;
			logger(ifp->ctx, LOG_ERR,
			    "%s: IA Address option truncated", ifp->name);
			continue;
		}
		iap = (const struct dhcp6_ia_addr *)D6_COPTION_DATA(o);
		TAILQ_FOREACH(a, &state->addrs, next) {
			if (ipv6_findaddrmatch(a, &iap->addr, 0))
				break;
		}
		if (a == NULL) {
			a = calloc(1, sizeof(*a));
			if (a == NULL) {
				logger(ifp->ctx, LOG_ERR, "%s: %m", __func__);
				break;
			}
			a->iface = ifp;
			a->flags = IPV6_AF_NEW | IPV6_AF_ONLINK;
			a->dadcallback = dhcp6_dadcallback;
			a->ia_type = ot;
			memcpy(a->iaid, iaid, sizeof(a->iaid));
			a->addr = iap->addr;
			a->created = *acquired;

			/*
			 * RFC 5942 Section 5
			 * We cannot assume any prefix length, nor tie the
			 * address to an existing one as it could expire
			 * before the address.
			 * As such we just give it a 128 prefix.
			 */
			a->prefix_len = 128;
			ipv6_makeprefix(&a->prefix, &a->addr, a->prefix_len);
			ia = inet_ntop(AF_INET6, &a->addr,
			    iabuf, sizeof(iabuf));
			snprintf(a->saddr, sizeof(a->saddr),
			    "%s/%d", ia, a->prefix_len);

			TAILQ_INSERT_TAIL(&state->addrs, a, next);
		} else {
			if (!(a->flags & IPV6_AF_ONLINK))
				a->flags |= IPV6_AF_ONLINK | IPV6_AF_NEW;
			a->flags &= ~IPV6_AF_STALE;
		}
		a->acquired = *acquired;
		a->prefix_pltime = ntohl(iap->pltime);
		u32 = ntohl(iap->vltime);
		if (a->prefix_vltime != u32) {
			a->flags |= IPV6_AF_NEW;
			a->prefix_vltime = u32;
		}
		if (a->prefix_pltime && a->prefix_pltime < state->lowpl)
		    state->lowpl = a->prefix_pltime;
		if (a->prefix_vltime && a->prefix_vltime > state->expire)
		    state->expire = a->prefix_vltime;
		i++;
	}
	return i;
}

static int
dhcp6_findpd(struct interface *ifp, const uint8_t *iaid,
    const uint8_t *d, size_t l, const struct timespec *acquired)
{
	struct dhcp6_state *state;
	const struct dhcp6_option *o, *ex;
	const uint8_t *p, *op;
	struct ipv6_addr *a;
	char iabuf[INET6_ADDRSTRLEN];
	const char *ia;
	int i;
	uint8_t u8, *pw;
	size_t off;
	uint16_t ol;
	const struct dhcp6_pd_addr *pdp;

	i = 0;
	state = D6_STATE(ifp);
	while ((o = dhcp6_findoption(D6_OPTION_IAPREFIX, d, l))) {
		off = (size_t)((const uint8_t *)o - d);
		l -= off;
		d += off;
		ol = ntohs(o->len);
		l -= sizeof(*o) + ol;
		d += sizeof(*o) + ol;
		if (ol < sizeof(*pdp)) {
			errno = EINVAL;
			logger(ifp->ctx, LOG_ERR,
			    "%s: IA Prefix option truncated", ifp->name);
			continue;
		}

		pdp = (const struct dhcp6_pd_addr *)D6_COPTION_DATA(o);
		TAILQ_FOREACH(a, &state->addrs, next) {
			if (IN6_ARE_ADDR_EQUAL(&a->prefix, &pdp->prefix))
				break;
		}
		if (a == NULL) {
			a = calloc(1, sizeof(*a));
			if (a == NULL) {
				logger(ifp->ctx, LOG_ERR, "%s: %m", __func__);
				break;
			}
			a->iface = ifp;
			a->flags = IPV6_AF_NEW | IPV6_AF_DELEGATEDPFX;
			a->created = *acquired;
			a->dadcallback = dhcp6_dadcallback;
			a->ia_type = D6_OPTION_IA_PD;
			memcpy(a->iaid, iaid, sizeof(a->iaid));
			a->prefix = pdp->prefix;
			a->prefix_len = pdp->prefix_len;
			ia = inet_ntop(AF_INET6, &a->prefix,
			    iabuf, sizeof(iabuf));
			snprintf(a->saddr, sizeof(a->saddr),
			    "%s/%d", ia, a->prefix_len);
			TAILQ_INSERT_TAIL(&state->addrs, a, next);
		} else {
			if (!(a->flags & IPV6_AF_DELEGATEDPFX))
				a->flags |= IPV6_AF_NEW | IPV6_AF_DELEGATEDPFX;
			a->flags &= ~(IPV6_AF_STALE | IPV6_AF_REQUEST);
			if (a->prefix_vltime != ntohl(pdp->vltime))
				a->flags |= IPV6_AF_NEW;
		}

		a->acquired = *acquired;
		a->prefix_pltime = ntohl(pdp->pltime);
		a->prefix_vltime = ntohl(pdp->vltime);

		if (a->prefix_pltime && a->prefix_pltime < state->lowpl)
			state->lowpl = a->prefix_pltime;
		if (a->prefix_vltime && a->prefix_vltime > state->expire)
			state->expire = a->prefix_vltime;
		i++;

		p = D6_COPTION_DATA(o) + sizeof(pdp);
		ol = (uint16_t)(ol - sizeof(pdp));
		ex = dhcp6_findoption(D6_OPTION_PD_EXCLUDE, p, ol);
		a->prefix_exclude_len = 0;
		memset(&a->prefix_exclude, 0, sizeof(a->prefix_exclude));
#if 0
		if (ex == NULL) {
			struct dhcp6_option *w;
			uint8_t *wp;

			w = calloc(1, 128);
			w->len = htons(2);
			wp = D6_OPTION_DATA(w);
			*wp++ = 64;
			*wp++ = 0x78;
			ex = w;
		}
#endif
		if (ex == NULL)
			continue;
		ol = ntohs(ex->len);
		if (ol < 2) {
			logger(ifp->ctx, LOG_ERR,
			    "%s: truncated PD Exclude", ifp->name);
			continue;
		}
		op = D6_COPTION_DATA(ex);
		a->prefix_exclude_len = *op++;
		ol--;
		if (((a->prefix_exclude_len - a->prefix_len - 1) / NBBY) + 1
		    != ol)
		{
			logger(ifp->ctx, LOG_ERR,
			    "%s: PD Exclude length mismatch", ifp->name);
			a->prefix_exclude_len = 0;
			continue;
		}
		u8 = a->prefix_len % NBBY;
		memcpy(&a->prefix_exclude, &a->prefix,
		    sizeof(a->prefix_exclude));
		if (u8)
			ol--;
		pw = a->prefix_exclude.s6_addr +
		    (a->prefix_exclude_len / NBBY) - 1;
		while (ol-- > 0)
			*pw-- = *op++;
		if (u8)
			*pw = (uint8_t)(*pw | (*op >> u8));
	}
	return i;
}

static int
dhcp6_findia(struct interface *ifp, const struct dhcp6_message *m, size_t l,
    const char *sfrom, const struct timespec *acquired)
{
	struct dhcp6_state *state;
	const struct if_options *ifo;
	const struct dhcp6_option *o;
	const uint8_t *p;
	int i, e;
	size_t j;
	uint32_t u32, renew, rebind;
	uint16_t code, ol;
	uint8_t iaid[4];
	char buf[sizeof(iaid) * 3];
	struct ipv6_addr *ap, *nap;

	if (l < sizeof(*m)) {
		/* Should be impossible with guards at packet in
		 * and reading leases */
		errno = EINVAL;
		return -1;
	}

	ifo = ifp->options;
	i = e = 0;
	state = D6_STATE(ifp);
	TAILQ_FOREACH(ap, &state->addrs, next) {
		ap->flags |= IPV6_AF_STALE;
	}
	l -= sizeof(*m);
	for (o = D6_CFIRST_OPTION(m); l > sizeof(*o); o = D6_CNEXT_OPTION(o)) {
		ol = ntohs(o->len);
		if (sizeof(*o) + ol > l) {
			errno = EINVAL;
			logger(ifp->ctx, LOG_ERR,
			    "%s: option overflow", ifp->name);
			break;
		}
		l -= sizeof(*o) + ol;

		code = ntohs(o->code);
		switch(code) {
		case D6_OPTION_IA_TA:
			u32 = 4;
			break;
		case D6_OPTION_IA_NA:
		case D6_OPTION_IA_PD:
			u32 = 12;
			break;
		default:
			continue;
		}
		if (ol < u32) {
			errno = EINVAL;
			logger(ifp->ctx, LOG_ERR,
			    "%s: IA option truncated", ifp->name);
			continue;
		}

		p = D6_COPTION_DATA(o);
		memcpy(iaid, p, sizeof(iaid));
		p += sizeof(iaid);
		ol = (uint16_t)(ol - sizeof(iaid));

		for (j = 0; j < ifo->ia_len; j++) {
			if (memcmp(&ifo->ia[j].iaid, iaid, sizeof(iaid)) == 0)
				break;
		}
		if (j == ifo->ia_len &&
		    !(ifo->ia_len == 0 && ifp->ctx->options & DHCPCD_DUMPLEASE))
		{
			logger(ifp->ctx, LOG_DEBUG,
			    "%s: ignoring unrequested IAID %s",
			    ifp->name,
			    hwaddr_ntoa(iaid, sizeof(iaid), buf, sizeof(buf)));
			continue;
		}
		if ( j < ifo->ia_len && ifo->ia[j].ia_type != code) {
			logger(ifp->ctx, LOG_ERR,
			    "%s: IAID %s: option type mismatch",
			    ifp->name,
			    hwaddr_ntoa(iaid, sizeof(iaid), buf, sizeof(buf)));
			continue;
		}

		if (code != D6_OPTION_IA_TA) {
			memcpy(&u32, p, sizeof(u32));
			renew = ntohl(u32);
			p += sizeof(u32);
			ol = (uint16_t)(ol - sizeof(u32));
			memcpy(&u32, p, sizeof(u32));
			rebind = ntohl(u32);
			p += sizeof(u32);
			ol = (uint16_t)(ol - sizeof(u32));
		} else
			renew = rebind = 0; /* appease gcc */
		if (dhcp6_checkstatusok(ifp, NULL, p, ol) == -1) {
			e = 1;
			continue;
		}
		if (code == D6_OPTION_IA_PD) {
			if (dhcp6_findpd(ifp, iaid, p, ol, acquired) == 0) {
				logger(ifp->ctx, LOG_WARNING,
				    "%s: %s: DHCPv6 REPLY missing Prefix",
				    ifp->name, sfrom);
				continue;
			}
		} else {
			if (dhcp6_findna(ifp, code, iaid, p, ol, acquired) == 0)
			{
				logger(ifp->ctx, LOG_WARNING,
				    "%s: %s: DHCPv6 REPLY missing IA Address",
				    ifp->name, sfrom);
				continue;
			}
		}
		if (code != D6_OPTION_IA_TA) {
			if (renew > rebind && rebind > 0) {
				if (sfrom)
				    logger(ifp->ctx, LOG_WARNING,
					"%s: T1 (%d) > T2 (%d) from %s",
					ifp->name, renew, rebind, sfrom);
				renew = 0;
				rebind = 0;
			}
			if (renew != 0 &&
			    (renew < state->renew || state->renew == 0))
				state->renew = renew;
			if (rebind != 0 &&
			    (rebind < state->rebind || state->rebind == 0))
				state->rebind = rebind;
		}
		i++;
	}
	TAILQ_FOREACH_SAFE(ap, &state->addrs, next, nap) {
		if (ap->flags & IPV6_AF_STALE) {
			eloop_q_timeout_delete(ifp->ctx->eloop, 0, NULL, ap);
			if (ap->flags & IPV6_AF_REQUEST) {
				ap->prefix_vltime = ap->prefix_pltime = 0;
			} else {
				TAILQ_REMOVE(&state->addrs, ap, next);
				free(ap);
			}
		}
	}
	if (i == 0 && e)
		return -1;
	return i;
}

static int
dhcp6_validatelease(struct interface *ifp,
    const struct dhcp6_message *m, size_t len,
    const char *sfrom, const struct timespec *acquired)
{
	struct dhcp6_state *state;
	int nia;
	struct timespec aq;

	if (len <= sizeof(*m)) {
		logger(ifp->ctx, LOG_ERR,
		    "%s: DHCPv6 lease truncated", ifp->name);
		return -1;
	}

	state = D6_STATE(ifp);
	if (dhcp6_checkstatusok(ifp, m, NULL, len) == -1)
		return -1;

	state->renew = state->rebind = state->expire = 0;
	state->lowpl = ND6_INFINITE_LIFETIME;
	if (!acquired) {
		clock_gettime(CLOCK_MONOTONIC, &aq);
		acquired = &aq;
	}
	nia = dhcp6_findia(ifp, m, len, sfrom, acquired);
	if (nia == 0) {
		logger(ifp->ctx, LOG_ERR,
		    "%s: no useable IA found in lease", ifp->name);
		return -1;
	}
	return nia;
}

static ssize_t
dhcp6_writelease(const struct interface *ifp)
{
	const struct dhcp6_state *state;
	int fd;
	ssize_t bytes;

	state = D6_CSTATE(ifp);
	logger(ifp->ctx, LOG_DEBUG,
	    "%s: writing lease `%s'", ifp->name, state->leasefile);

	fd = open(state->leasefile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd == -1) {
		logger(ifp->ctx, LOG_ERR, "%s: dhcp6_writelease: %m", ifp->name);
		return -1;
	}
	bytes = write(fd, state->new, state->new_len);
	close(fd);
	return bytes;
}

static int
dhcp6_readlease(struct interface *ifp, int validate)
{
	struct dhcp6_state *state;
	struct stat st;
	int fd;
	ssize_t bytes;
	const struct dhcp6_option *o;
	struct timespec acquired;
	time_t now;
	int retval;

	state = D6_STATE(ifp);
	if (stat(state->leasefile, &st) == -1)
		return -1;
	logger(ifp->ctx, LOG_DEBUG, "%s: reading lease `%s'",
	    ifp->name, state->leasefile);
	if (st.st_size > UINT32_MAX) {
		errno = E2BIG;
		return -1;
	}
	if ((fd = open(state->leasefile, O_RDONLY)) == -1)
		return -1;
	if ((state->new = malloc((size_t)st.st_size)) == NULL)
		return -1;
	retval = -1;
	state->new_len = (size_t)st.st_size;
	bytes = read(fd, state->new, state->new_len);
	close(fd);
	if (bytes != (ssize_t)state->new_len)
		goto ex;

	/* If not validating IA's and if they have expired,
	 * skip to the auth check. */
	if (!validate) {
		fd = 0;
		goto auth;
	}

	if ((now = time(NULL)) == -1)
		goto ex;

	clock_gettime(CLOCK_MONOTONIC, &acquired);
	acquired.tv_sec -= now - st.st_mtime;

	/* Check to see if the lease is still valid */
	fd = dhcp6_validatelease(ifp, state->new, state->new_len, NULL,
	    &acquired);
	if (fd == -1)
		goto ex;

	if (!(ifp->ctx->options & DHCPCD_DUMPLEASE) &&
	    state->expire != ND6_INFINITE_LIFETIME)
	{
		if ((time_t)state->expire < now - st.st_mtime) {
			logger(ifp->ctx,
			    LOG_DEBUG,"%s: discarding expired lease",
			    ifp->name);
			retval = 0;
			goto ex;
		}
	}

auth:

	retval = 0;
	/* Authenticate the message */
	o = dhcp6_getmoption(D6_OPTION_AUTH, state->new, state->new_len);
	if (o) {
		if (dhcp_auth_validate(&state->auth, &ifp->options->auth,
		    (uint8_t *)state->new, state->new_len, 6, state->new->type,
		    D6_COPTION_DATA(o), ntohs(o->len)) == NULL)
		{
			logger(ifp->ctx, LOG_DEBUG,
			    "%s: dhcp_auth_validate: %m", ifp->name);
			logger(ifp->ctx, LOG_ERR,
			    "%s: authentication failed", ifp->name);
			goto ex;
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
		goto ex;
	}

	return fd;

ex:
	dhcp6_freedrop_addrs(ifp, 0, NULL);
	free(state->new);
	state->new = NULL;
	state->new_len = 0;
	if (!(ifp->ctx->options & DHCPCD_DUMPLEASE))
		unlink(state->leasefile);
	return retval;
}

static void
dhcp6_startinit(struct interface *ifp)
{
	struct dhcp6_state *state;
	int r;
	uint8_t has_ta, has_non_ta;
	size_t i;

	state = D6_STATE(ifp);
	state->state = DH6S_INIT;
	state->expire = ND6_INFINITE_LIFETIME;
	state->lowpl = ND6_INFINITE_LIFETIME;

	dhcp6_addrequestedaddrs(ifp);
	has_ta = has_non_ta = 0;
	for (i = 0; i < ifp->options->ia_len; i++) {
		switch (ifp->options->ia[i].ia_type) {
		case D6_OPTION_IA_TA:
			has_ta = 1;
			break;
		default:
			has_non_ta = 1;
		}
	}

	if (!(ifp->ctx->options & DHCPCD_TEST) &&
	    !(has_ta && !has_non_ta) &&
	    ifp->options->reboot != 0)
	{
		r = dhcp6_readlease(ifp, 1);
		if (r == -1) {
			if (errno != ENOENT)
				logger(ifp->ctx, LOG_ERR,
				    "%s: dhcp6_readlease: %s: %m",
				    ifp->name, state->leasefile);
		} else if (r != 0) {
			/* RFC 3633 section 12.1 */
			if (dhcp6_hasprefixdelegation(ifp))
				dhcp6_startrebind(ifp);
			else
				dhcp6_startconfirm(ifp);
			return;
		}
	}
	dhcp6_startdiscover(ifp);
}

static struct ipv6_addr *
dhcp6_ifdelegateaddr(struct interface *ifp, struct ipv6_addr *prefix,
    const struct if_sla *sla, struct if_ia *ia, struct interface *ifs)
{
	struct dhcp6_state *state;
	struct in6_addr addr;
	struct ipv6_addr *a, *ap, *apn;
	char sabuf[INET6_ADDRSTRLEN];
	const char *sa;
	int pfxlen;

	/* RFC6603 Section 4.2 */
	if (strcmp(ifp->name, ifs->name) == 0) {
		if (prefix->prefix_exclude_len == 0) {
			/* Don't spam the log automatically */
			if (sla)
				logger(ifp->ctx, LOG_WARNING,
				    "%s: DHCPv6 server does not support "
				    "OPTION_PD_EXCLUDE",
				    ifp->name);
			return NULL;
		}
		pfxlen = prefix->prefix_exclude_len;
		memcpy(&addr, &prefix->prefix_exclude, sizeof(addr));
	} else if ((pfxlen = dhcp6_delegateaddr(&addr, ifp, prefix,
	    sla, ia)) == -1)
		return NULL;


	a = calloc(1, sizeof(*a));
	if (a == NULL) {
		logger(ifp->ctx, LOG_ERR, "%s: %m", __func__);
		return NULL;
	}
	a->iface = ifp;
	a->flags = IPV6_AF_NEW | IPV6_AF_ONLINK;
	a->dadcallback = dhcp6_dadcallback;
	a->delegating_iface = ifs;
	memcpy(&a->iaid, &prefix->iaid, sizeof(a->iaid));
	a->created = a->acquired = prefix->acquired;
	a->prefix_pltime = prefix->prefix_pltime;
	a->prefix_vltime = prefix->prefix_vltime;
	a->prefix = addr;
	a->prefix_len = (uint8_t)pfxlen;

	/* Wang a 1 at the end as the prefix could be >64
	 * making SLAAC impossible. */
	a->addr = a->prefix;
	a->addr.s6_addr[sizeof(a->addr.s6_addr) - 1] =
	    (uint8_t)(a->addr.s6_addr[sizeof(a->addr.s6_addr) - 1] + 1);

	state = D6_STATE(ifp);
	/* Remove any exiting address */
	TAILQ_FOREACH_SAFE(ap, &state->addrs, next, apn) {
		if (IN6_ARE_ADDR_EQUAL(&ap->addr, &a->addr)) {
			TAILQ_REMOVE(&state->addrs, ap, next);
			/* Keep our flags */
			a->flags |= ap->flags;
			a->flags &= ~IPV6_AF_NEW;
			a->created = ap->created;
			ipv6_freeaddr(ap);
		}
	}

	sa = inet_ntop(AF_INET6, &a->addr, sabuf, sizeof(sabuf));
	snprintf(a->saddr, sizeof(a->saddr), "%s/%d", sa, a->prefix_len);
	TAILQ_INSERT_TAIL(&state->addrs, a, next);
	return a;
}

static void
dhcp6_script_try_run(struct interface *ifp, int delegated)
{
	struct dhcp6_state *state;
	struct ipv6_addr *ap;
	int completed;

	state = D6_STATE(ifp);
	completed = 1;
	/* If all addresses have completed DAD run the script */
	TAILQ_FOREACH(ap, &state->addrs, next) {
		if (!(ap->flags & IPV6_AF_ADDED))
			continue;
		if (ap->flags & IPV6_AF_ONLINK) {
			if (!(ap->flags & IPV6_AF_DADCOMPLETED) &&
			    ipv6_iffindaddr(ap->iface, &ap->addr))
				ap->flags |= IPV6_AF_DADCOMPLETED;
			if ((ap->flags & IPV6_AF_DADCOMPLETED) == 0 &&
			    ((delegated && ap->delegating_iface) ||
			    (!delegated && !ap->delegating_iface)))
			{
				completed = 0;
				break;
			}
		}
	}
	if (completed) {
		script_runreason(ifp, delegated ? "DELEGATED6" : state->reason);
		if (!delegated)
			dhcpcd_daemonise(ifp->ctx);
	} else
		logger(ifp->ctx, LOG_DEBUG,
		    "%s: waiting for DHCPv6 DAD to complete", ifp->name);
}

static void
dhcp6_delegate_prefix(struct interface *ifp)
{
	struct if_options *ifo;
	struct dhcp6_state *state, *ifd_state;
	struct ipv6_addr *ap;
	size_t i, j, k;
	struct if_ia *ia;
	struct if_sla *sla;
	struct interface *ifd;
	uint8_t carrier_warned, abrt;

	ifo = ifp->options;
	state = D6_STATE(ifp);

	/* Try to load configured interfaces for delegation that do not exist */
	for (i = 0; i < ifo->ia_len; i++) {
		ia = &ifo->ia[i];
		for (j = 0; j < ia->sla_len; j++) {
			sla = &ia->sla[j];
			for (k = 0; k < i; j++)
				if (strcmp(sla->ifname, ia->sla[j].ifname) == 0)
					break;
			if (j >= i &&
			    if_find(ifp->ctx->ifaces, sla->ifname) == NULL)
			{
				logger(ifp->ctx, LOG_INFO,
				    "%s: loading for delegation", sla->ifname);
				if (dhcpcd_handleinterface(ifp->ctx, 2,
				    sla->ifname) == -1)
					logger(ifp->ctx, LOG_ERR,
					    "%s: interface does not exist"
					    " for delegation",
					    sla->ifname);
			}
		}
	}

	TAILQ_FOREACH(ifd, ifp->ctx->ifaces, next) {
		k = 0;
		carrier_warned = abrt = 0;
		TAILQ_FOREACH(ap, &state->addrs, next) {
			if (!(ap->flags & IPV6_AF_DELEGATEDPFX))
				continue;
			if (ap->flags & IPV6_AF_NEW) {
				ap->flags &= ~IPV6_AF_NEW;
				logger(ifp->ctx, LOG_DEBUG,
				    "%s: delegated prefix %s",
				    ifp->name, ap->saddr);
			}
			for (i = 0; i < ifo->ia_len; i++) {
				ia = &ifo->ia[i];
				if (memcmp(ia->iaid, ap->iaid,
				    sizeof(ia->iaid)))
					continue;
				if (ia->sla_len == 0) {
					/* no SLA configured, so lets
					 * automate it */
					if (ifd->carrier != LINK_UP) {
						logger(ifp->ctx, LOG_DEBUG,
						    "%s: has no carrier, cannot"
						    " delegate addresses",
						    ifd->name);
						carrier_warned = 1;
						break;
					}
					if (dhcp6_ifdelegateaddr(ifd, ap,
					    NULL, ia, ifp))
						k++;
				}
				for (j = 0; j < ia->sla_len; j++) {
					sla = &ia->sla[j];
					if (sla->sla_set && sla->sla == 0)
						ap->flags |=
						    IPV6_AF_DELEGATEDZERO;
					if (strcmp(ifd->name, sla->ifname))
						continue;
					if (ifd->carrier != LINK_UP) {
						logger(ifp->ctx, LOG_DEBUG,
						    "%s: has no carrier, cannot"
						    " delegate addresses",
						    ifd->name);
						carrier_warned = 1;
						break;
					}
					if (dhcp6_ifdelegateaddr(ifd, ap,
					    sla, ia, ifp))
						k++;
				}
				if (carrier_warned ||abrt)
					break;
			}
			if (carrier_warned || abrt)
				break;
		}
		if (k && !carrier_warned) {
			ifd_state = D6_STATE(ifd);
			ipv6_addaddrs(&ifd_state->addrs);
			if_initrt6(ifd);
			dhcp6_script_try_run(ifd, 1);
		}
	}
}

static void
dhcp6_find_delegates1(void *arg)
{

	dhcp6_find_delegates(arg);
}

size_t
dhcp6_find_delegates(struct interface *ifp)
{
	struct if_options *ifo;
	struct dhcp6_state *state;
	struct ipv6_addr *ap;
	size_t i, j, k;
	struct if_ia *ia;
	struct if_sla *sla;
	struct interface *ifd;

	k = 0;
	TAILQ_FOREACH(ifd, ifp->ctx->ifaces, next) {
		ifo = ifd->options;
		state = D6_STATE(ifd);
		if (state == NULL || state->state != DH6S_BOUND)
			continue;
		TAILQ_FOREACH(ap, &state->addrs, next) {
			if (!(ap->flags & IPV6_AF_DELEGATEDPFX))
				continue;
			for (i = 0; i < ifo->ia_len; i++) {
				ia = &ifo->ia[i];
				if (memcmp(ia->iaid, ap->iaid,
				    sizeof(ia->iaid)))
					continue;
				for (j = 0; j < ia->sla_len; j++) {
					sla = &ia->sla[j];
					if (strcmp(ifp->name, sla->ifname))
						continue;
					if (ipv6_linklocal(ifp) == NULL) {
						logger(ifp->ctx, LOG_DEBUG,
						    "%s: delaying adding"
						    " delegated addresses for"
						    " LL address",
						    ifp->name);
						ipv6_addlinklocalcallback(ifp,
						    dhcp6_find_delegates1, ifp);
						return 1;
					}
					if (dhcp6_ifdelegateaddr(ifp, ap,
					    sla, ia, ifd))
					    k++;
				}
			}
		}
	}

	if (k) {
		logger(ifp->ctx, LOG_INFO,
		    "%s: adding delegated prefixes", ifp->name);
		state = D6_STATE(ifp);
		state->state = DH6S_DELEGATED;
		ipv6_addaddrs(&state->addrs);
		if_initrt6(ifp);
		ipv6_buildroutes(ifp->ctx);
		dhcp6_script_try_run(ifp, 1);
	}
	return k;
}

/* ARGSUSED */
static void
dhcp6_handledata(void *arg)
{
	struct dhcpcd_ctx *dctx;
	struct ipv6_ctx *ctx;
	size_t i, len;
	ssize_t bytes;
	struct cmsghdr *cm;
	struct in6_pktinfo pkt;
	struct interface *ifp;
	const char *op;
	struct dhcp6_message *r;
	struct dhcp6_state *state;
	const struct dhcp6_option *o, *auth;
	const struct dhcp_opt *opt;
	const struct if_options *ifo;
	struct ipv6_addr *ap;
	uint8_t has_new;
	int error;
	uint32_t u32;

	dctx = arg;
	ctx = dctx->ipv6;
	ctx->rcvhdr.msg_controllen = CMSG_SPACE(sizeof(struct in6_pktinfo));
	bytes = recvmsg(ctx->dhcp_fd, &ctx->rcvhdr, 0);
	if (bytes == -1) {
		logger(dctx, LOG_ERR, "%s: recvmsg: %m", __func__);
		close(ctx->dhcp_fd);
		eloop_event_delete(dctx->eloop, ctx->dhcp_fd);
		ctx->dhcp_fd = -1;
		return;
	}
	len = (size_t)bytes;
	ctx->sfrom = inet_ntop(AF_INET6, &ctx->from.sin6_addr,
	    ctx->ntopbuf, sizeof(ctx->ntopbuf));
	if (len < sizeof(struct dhcp6_message)) {
		logger(dctx, LOG_ERR,
		    "DHCPv6 packet too short from %s", ctx->sfrom);
		return;
	}

	pkt.ipi6_ifindex = 0;
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
		}
	}
	if (pkt.ipi6_ifindex == 0) {
		logger(dctx, LOG_ERR,
		    "DHCPv6 reply did not contain index from %s", ctx->sfrom);
		return;
	}

	TAILQ_FOREACH(ifp, dctx->ifaces, next) {
		if (ifp->index == (unsigned int)pkt.ipi6_ifindex)
			break;
	}
	if (ifp == NULL) {
		logger(dctx, LOG_DEBUG,
		    "DHCPv6 reply for unexpected interface from %s",
		    ctx->sfrom);
		return;
	}

	state = D6_STATE(ifp);
	if (state == NULL || state->send == NULL) {
		logger(ifp->ctx, LOG_DEBUG,
		    "%s: DHCPv6 reply received but not running", ifp->name);
		return;
	}

	r = (struct dhcp6_message *)ctx->rcvhdr.msg_iov[0].iov_base;
	/* We're already bound and this message is for another machine */
	/* XXX DELEGATED? */
	if (r->type != DHCP6_RECONFIGURE &&
	    (state->state == DH6S_BOUND || state->state == DH6S_INFORMED)) 
	{
		logger(ifp->ctx, LOG_DEBUG,
		    "%s: DHCPv6 reply received but already bound", ifp->name);
		return;
	}

	if (r->type != DHCP6_RECONFIGURE &&
	    (r->xid[0] != state->send->xid[0] ||
	    r->xid[1] != state->send->xid[1] ||
	    r->xid[2] != state->send->xid[2]))
	{
		logger(dctx, LOG_DEBUG,
		    "%s: wrong xid 0x%02x%02x%02x"
		    " (expecting 0x%02x%02x%02x) from %s",
		    ifp->name,
		    r->xid[0], r->xid[1], r->xid[2],
		    state->send->xid[0], state->send->xid[1],
		    state->send->xid[2],
		    ctx->sfrom);
		return;
	}

	if (dhcp6_getmoption(D6_OPTION_SERVERID, r, len) == NULL) {
		logger(ifp->ctx, LOG_DEBUG, "%s: no DHCPv6 server ID from %s",
		    ifp->name, ctx->sfrom);
		return;
	}

	o = dhcp6_getmoption(D6_OPTION_CLIENTID, r, len);
	if (o == NULL || ntohs(o->len) != dctx->duid_len ||
	    memcmp(D6_COPTION_DATA(o), dctx->duid, dctx->duid_len) != 0)
	{
		logger(ifp->ctx, LOG_DEBUG, "%s: incorrect client ID from %s",
		    ifp->name, ctx->sfrom);
		return;
	}

	ifo = ifp->options;
	for (i = 0, opt = dctx->dhcp6_opts;
	    i < dctx->dhcp6_opts_len;
	    i++, opt++)
	{
		if (has_option_mask(ifo->requiremask6, opt->option) &&
		    dhcp6_getmoption((uint16_t)opt->option, r, len) == NULL)
		{
			logger(ifp->ctx, LOG_WARNING,
			    "%s: reject DHCPv6 (no option %s) from %s",
			    ifp->name, opt->var, ctx->sfrom);
			return;
		}
		if (has_option_mask(ifo->rejectmask6, opt->option) &&
		    dhcp6_getmoption((uint16_t)opt->option, r, len))
		{
			logger(ifp->ctx, LOG_WARNING,
			    "%s: reject DHCPv6 (option %s) from %s",
			    ifp->name, opt->var, ctx->sfrom);
			return;
		}
	}

	/* Authenticate the message */
	auth = dhcp6_getmoption(D6_OPTION_AUTH, r, len);
	if (auth) {
		if (dhcp_auth_validate(&state->auth, &ifo->auth,
		    (uint8_t *)r, len, 6, r->type,
		    D6_COPTION_DATA(auth), ntohs(auth->len)) == NULL)
		{
			logger(ifp->ctx, LOG_DEBUG, "dhcp_auth_validate: %m");
			logger(ifp->ctx, LOG_ERR,
			    "%s: authentication failed from %s",
			    ifp->name, ctx->sfrom);
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
			logger(ifp->ctx, LOG_ERR,
			    "%s: no authentication from %s",
			    ifp->name, ctx->sfrom);
			return;
		}
		logger(ifp->ctx, LOG_WARNING,
		    "%s: no authentication from %s", ifp->name, ctx->sfrom);
	}

	op = dhcp6_get_op(r->type);
	switch(r->type) {
	case DHCP6_REPLY:
		switch(state->state) {
		case DH6S_INFORM:
			if (dhcp6_checkstatusok(ifp, r, NULL, len) == -1)
				return;
			/* RFC4242 */
			o = dhcp6_getmoption(D6_OPTION_INFO_REFRESH_TIME,
			    r, len);
			if (o == NULL || ntohs(o->len) != sizeof(u32))
				state->renew = IRT_DEFAULT;
			else {
				memcpy(&u32, D6_COPTION_DATA(o), sizeof(u32));
				state->renew = ntohl(u32);
				if (state->renew < IRT_MINIMUM)
					state->renew = IRT_MINIMUM;
			}
			break;
		case DH6S_CONFIRM:
			error = dhcp6_checkstatusok(ifp, r, NULL, len);
			/* If we got an OK status the chances are that we
			 * didn't get the IA's returned, so preserve them
			 * from our saved response */
			if (error == 1)
				goto recv;
			if (error == -1 ||
			    dhcp6_validatelease(ifp, r, len,
			    ctx->sfrom, NULL) == -1)
			{
				dhcp6_startdiscover(ifp);
				return;
			}
			break;
		case DH6S_DISCOVER:
			if (has_option_mask(ifo->requestmask6,
			    D6_OPTION_RAPID_COMMIT) &&
			    dhcp6_getmoption(D6_OPTION_RAPID_COMMIT, r, len))
				state->state = DH6S_REQUEST;
			else
				op = NULL;
		case DH6S_REQUEST: /* FALLTHROUGH */
		case DH6S_RENEW: /* FALLTHROUGH */
		case DH6S_REBIND:
			if (dhcp6_validatelease(ifp, r, len,
			    ctx->sfrom, NULL) == -1)
			{
				/* PD doesn't use CONFIRM, so REBIND could
				 * throw up an invalid prefix if we
				 * changed link */
				if (dhcp6_hasprefixdelegation(ifp))
					dhcp6_startdiscover(ifp);
				return;
			}
			break;
		default:
			op = NULL;
		}
		break;
	case DHCP6_ADVERTISE:
		if (state->state != DH6S_DISCOVER) {
			op = NULL;
			break;
		}
		/* RFC7083 */
		o = dhcp6_getmoption(D6_OPTION_SOL_MAX_RT, r, len);
		if (o && ntohs(o->len) >= sizeof(u32)) {
			memcpy(&u32, D6_COPTION_DATA(o), sizeof(u32));
			u32 = ntohl(u32);
			if (u32 >= 60 && u32 <= 86400) {
				logger(ifp->ctx, LOG_DEBUG,
				    "%s: SOL_MAX_RT %llu -> %d", ifp->name,
				    (unsigned long long)state->sol_max_rt, u32);
				state->sol_max_rt = (time_t)u32;
			} else
				logger(ifp->ctx, LOG_ERR,
				    "%s: invalid SOL_MAX_RT %d",
				    ifp->name, u32);
		}
		o = dhcp6_getmoption(D6_OPTION_INF_MAX_RT, r, len);
		if (o && ntohs(o->len) >= sizeof(u32)) {
			memcpy(&u32, D6_COPTION_DATA(o), sizeof(u32));
			u32 = ntohl(u32);
			if (u32 >= 60 && u32 <= 86400) {
				logger(ifp->ctx, LOG_DEBUG,
				    "%s: INF_MAX_RT %llu -> %d",
				    ifp->name,
				    (unsigned long long)state->inf_max_rt, u32);
				state->inf_max_rt = (time_t)u32;
			} else
				logger(ifp->ctx, LOG_ERR,
				    "%s: invalid INF_MAX_RT %d",
				    ifp->name, u32);
		}
		if (dhcp6_validatelease(ifp, r, len, ctx->sfrom, NULL) == -1)
			return;
		break;
	case DHCP6_RECONFIGURE:
		if (auth == NULL) {
			logger(ifp->ctx, LOG_ERR,
			    "%s: unauthenticated %s from %s",
			    ifp->name, op, ctx->sfrom);
			if (ifo->auth.options & DHCPCD_AUTH_REQUIRE)
				return;
		}
		logger(ifp->ctx, LOG_INFO, "%s: %s from %s",
		    ifp->name, op, ctx->sfrom);
		o = dhcp6_getmoption(D6_OPTION_RECONF_MSG, r, len);
		if (o == NULL) {
			logger(ifp->ctx, LOG_ERR,
			    "%s: missing Reconfigure Message option",
			    ifp->name);
			return;
		}
		if (ntohs(o->len) != 1) {
			logger(ifp->ctx, LOG_ERR,
			    "%s: missing Reconfigure Message type", ifp->name);
			return;
		}
		switch(*D6_COPTION_DATA(o)) {
		case DHCP6_RENEW:
			if (state->state != DH6S_BOUND) {
				logger(ifp->ctx, LOG_ERR,
				    "%s: not bound, ignoring %s",
				    ifp->name, op);
				return;
			}
			eloop_timeout_delete(ifp->ctx->eloop,
			    dhcp6_startrenew, ifp);
			dhcp6_startrenew(ifp);
			break;
		case DHCP6_INFORMATION_REQ:
			if (state->state != DH6S_INFORMED) {
				logger(ifp->ctx, LOG_ERR,
				    "%s: not informed, ignoring %s",
				    ifp->name, op);
				return;
			}
			eloop_timeout_delete(ifp->ctx->eloop,
			    dhcp6_sendinform, ifp);
			dhcp6_startinform(ifp);
			break;
		default:
			logger(ifp->ctx, LOG_ERR,
			    "%s: unsupported %s type %d",
			    ifp->name, op, *D6_COPTION_DATA(o));
			break;
		}
		return;
	default:
		logger(ifp->ctx, LOG_ERR, "%s: invalid DHCP6 type %s (%d)",
		    ifp->name, op, r->type);
		return;
	}
	if (op == NULL) {
		logger(ifp->ctx, LOG_WARNING,
		    "%s: invalid state for DHCP6 type %s (%d)",
		    ifp->name, op, r->type);
		return;
	}

	if (state->recv_len < (size_t)len) {
		free(state->recv);
		state->recv = malloc(len);
		if (state->recv == NULL) {
			logger(ifp->ctx, LOG_ERR,
			    "%s: malloc recv: %m", ifp->name);
			return;
		}
	}
	memcpy(state->recv, r, len);
	state->recv_len = len;

	switch(r->type) {
	case DHCP6_ADVERTISE:
		if (state->state == DH6S_REQUEST) /* rapid commit */
			break;
		ap = TAILQ_FIRST(&state->addrs);
		logger(ifp->ctx, LOG_INFO, "%s: ADV %s from %s",
		    ifp->name, ap->saddr, ctx->sfrom);
		if (ifp->ctx->options & DHCPCD_TEST)
			break;
		dhcp6_startrequest(ifp);
		return;
	}

recv:
	has_new = 0;
	TAILQ_FOREACH(ap, &state->addrs, next) {
		if (ap->flags & IPV6_AF_NEW) {
			has_new = 1;
			break;
		}
	}
	logger(ifp->ctx, has_new ? LOG_INFO : LOG_DEBUG,
	    "%s: %s received from %s", ifp->name, op, ctx->sfrom);

	state->reason = NULL;
	eloop_timeout_delete(ifp->ctx->eloop, NULL, ifp);
	switch(state->state) {
	case DH6S_INFORM:
		state->rebind = 0;
		state->expire = ND6_INFINITE_LIFETIME;
		state->lowpl = ND6_INFINITE_LIFETIME;
		state->reason = "INFORM6";
		break;
	case DH6S_REQUEST:
		if (state->reason == NULL)
			state->reason = "BOUND6";
		/* FALLTHROUGH */
	case DH6S_RENEW:
		if (state->reason == NULL)
			state->reason = "RENEW6";
		/* FALLTHROUGH */
	case DH6S_REBIND:
		if (state->reason == NULL)
			state->reason = "REBIND6";
		/* FALLTHROUGH */
	case DH6S_CONFIRM:
		if (state->reason == NULL)
			state->reason = "REBOOT6";
		if (state->renew == 0) {
			if (state->expire == ND6_INFINITE_LIFETIME)
				state->renew = ND6_INFINITE_LIFETIME;
			else if (state->lowpl != ND6_INFINITE_LIFETIME)
				state->renew = (uint32_t)(state->lowpl * 0.5);
		}
		if (state->rebind == 0) {
			if (state->expire == ND6_INFINITE_LIFETIME)
				state->rebind = ND6_INFINITE_LIFETIME;
			else if (state->lowpl != ND6_INFINITE_LIFETIME)
				state->rebind = (uint32_t)(state->lowpl * 0.8);
		}
		break;
	default:
		state->reason = "UNKNOWN6";
		break;
	}

	if (state->state != DH6S_CONFIRM) {
		free(state->old);
		state->old = state->new;
		state->old_len = state->new_len;
		state->new = state->recv;
		state->new_len = state->recv_len;
		state->recv = NULL;
		state->recv_len = 0;
	}

	if (ifp->ctx->options & DHCPCD_TEST)
		script_runreason(ifp, "TEST");
	else {
		if (state->state == DH6S_INFORM)
			state->state = DH6S_INFORMED;
		else
			state->state = DH6S_BOUND;
		if (state->renew && state->renew != ND6_INFINITE_LIFETIME)
			eloop_timeout_add_sec(ifp->ctx->eloop,
			    (time_t)state->renew,
			    state->state == DH6S_INFORMED ?
			    dhcp6_startinform : dhcp6_startrenew, ifp);
		if (state->rebind && state->rebind != ND6_INFINITE_LIFETIME)
			eloop_timeout_add_sec(ifp->ctx->eloop,
			    (time_t)state->rebind, dhcp6_startrebind, ifp);
		if (state->expire != ND6_INFINITE_LIFETIME)
			eloop_timeout_add_sec(ifp->ctx->eloop,
			    (time_t)state->expire, dhcp6_startexpire, ifp);

		ipv6nd_runignoredra(ifp);
		ipv6_addaddrs(&state->addrs);
		dhcp6_delegate_prefix(ifp);

		if (state->state == DH6S_INFORMED)
			logger(ifp->ctx, has_new ? LOG_INFO : LOG_DEBUG,
			    "%s: refresh in %"PRIu32" seconds",
			    ifp->name, state->renew);
		else if (state->renew || state->rebind)
			logger(ifp->ctx, has_new ? LOG_INFO : LOG_DEBUG,
			    "%s: renew in %"PRIu32" seconds,"
			    " rebind in %"PRIu32" seconds",
			    ifp->name, state->renew, state->rebind);
		else if (state->expire == 0)
			logger(ifp->ctx, has_new ? LOG_INFO : LOG_DEBUG,
			    "%s: will expire", ifp->name);
		if_initrt6(ifp);
		ipv6_buildroutes(ifp->ctx);
		dhcp6_writelease(ifp);
		dhcp6_script_try_run(ifp, 0);
	}

	if (ifp->ctx->options & DHCPCD_TEST ||
	    (ifp->options->options & DHCPCD_INFORM &&
	    !(ifp->ctx->options & DHCPCD_MASTER)))
	{
		eloop_exit(ifp->ctx->eloop, EXIT_SUCCESS);
	}
}

static int
dhcp6_open(struct dhcpcd_ctx *dctx)
{
	struct ipv6_ctx *ctx;
	struct sockaddr_in6 sa;
	int n;

	memset(&sa, 0, sizeof(sa));
	sa.sin6_family = AF_INET6;
	sa.sin6_port = htons(DHCP6_CLIENT_PORT);
#ifdef BSD
	sa.sin6_len = sizeof(sa);
#endif

	ctx = dctx->ipv6;
	ctx->dhcp_fd = xsocket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP,
	    O_NONBLOCK|O_CLOEXEC);
	if (ctx->dhcp_fd == -1)
		return -1;

	n = 1;
	if (setsockopt(ctx->dhcp_fd, SOL_SOCKET, SO_REUSEADDR,
	    &n, sizeof(n)) == -1)
		goto errexit;

	n = 1;
	if (setsockopt(ctx->dhcp_fd, SOL_SOCKET, SO_BROADCAST,
	    &n, sizeof(n)) == -1)
		goto errexit;

#ifdef SO_REUSEPORT
	n = 1;
	if (setsockopt(ctx->dhcp_fd, SOL_SOCKET, SO_REUSEPORT,
	    &n, sizeof(n)) == -1)
		logger(dctx, LOG_WARNING, "setsockopt: SO_REUSEPORT: %m");
#endif

	if (bind(ctx->dhcp_fd, (struct sockaddr *)&sa, sizeof(sa)) == -1)
		goto errexit;

	n = 1;
	if (setsockopt(ctx->dhcp_fd, IPPROTO_IPV6, IPV6_RECVPKTINFO,
	    &n, sizeof(n)) == -1)
		goto errexit;

	eloop_event_add(dctx->eloop, ctx->dhcp_fd,
	    dhcp6_handledata, dctx, NULL, NULL);
	return 0;

errexit:
	close(ctx->dhcp_fd);
	ctx->dhcp_fd = -1;
	return -1;
}

static void
dhcp6_start1(void *arg)
{
	struct interface *ifp = arg;
	struct if_options *ifo = ifp->options;
	struct dhcp6_state *state;
	size_t i;
	const struct dhcp_compat *dhc;

	state = D6_STATE(ifp);
	/* If no DHCPv6 options are configured,
	   match configured DHCPv4 options to DHCPv6 equivalents. */
	for (i = 0; i < sizeof(ifo->requestmask6); i++) {
		if (ifo->requestmask6[i] != '\0')
			break;
	}
	if (i == sizeof(ifo->requestmask6)) {
		for (dhc = dhcp_compats; dhc->dhcp_opt; dhc++) {
			if (has_option_mask(ifo->requestmask, dhc->dhcp_opt))
				add_option_mask(ifo->requestmask6,
				    dhc->dhcp6_opt);
		}
		if (ifo->fqdn != FQDN_DISABLE ||
		    ifo->options & DHCPCD_HOSTNAME)
			add_option_mask(ifo->requestmask6, D6_OPTION_FQDN);
	}

	/* Rapid commit won't work with Prefix Delegation Exclusion */
	if (dhcp6_findselfsla(ifp, NULL))
		del_option_mask(ifo->requestmask6, D6_OPTION_RAPID_COMMIT);

	if (state->state == DH6S_INFORM) {
		add_option_mask(ifo->requestmask6, D6_OPTION_INFO_REFRESH_TIME);
		dhcp6_startinform(ifp);
	} else {
		del_option_mask(ifo->requestmask6, D6_OPTION_INFO_REFRESH_TIME);
		dhcp6_startinit(ifp);
	}
}

int
dhcp6_start(struct interface *ifp, enum DH6S init_state)
{
	struct dhcp6_state *state;

	state = D6_STATE(ifp);
	if (state) {
		if (state->state == DH6S_INFORMED &&
		    init_state == DH6S_INFORM)
		{
			dhcp6_startinform(ifp);
			return 0;
		}
		if (init_state == DH6S_INIT &&
		    ifp->options->options & DHCPCD_DHCP6 &&
		    (state->state == DH6S_INFORM ||
		    state->state == DH6S_INFORMED ||
		    state->state == DH6S_DELEGATED))
		{
			/* Change from stateless to stateful */
			goto gogogo;
		}
		/* We're already running DHCP6 */
		/* XXX: What if the managed flag vanishes from all RA? */
		return 0;
	}

	if (!(ifp->options->options & DHCPCD_DHCP6))
		return 0;

	if (ifp->ctx->ipv6->dhcp_fd == -1 && dhcp6_open(ifp->ctx) == -1)
		return -1;

	ifp->if_data[IF_DATA_DHCP6] = calloc(1, sizeof(*state));
	state = D6_STATE(ifp);
	if (state == NULL)
		return -1;

	state->sol_max_rt = SOL_MAX_RT;
	state->inf_max_rt = INF_MAX_RT;
	TAILQ_INIT(&state->addrs);

gogogo:
	state->state = init_state;
	dhcp_set_leasefile(state->leasefile, sizeof(state->leasefile),
	    AF_INET6, ifp);
	if (ipv6_linklocal(ifp) == NULL) {
		logger(ifp->ctx, LOG_DEBUG,
		    "%s: delaying DHCPv6 soliciation for LL address",
		    ifp->name);
		ipv6_addlinklocalcallback(ifp, dhcp6_start1, ifp);
		return 0;
	}

	dhcp6_start1(ifp);
	return 0;
}

void
dhcp6_reboot(struct interface *ifp)
{
	struct dhcp6_state *state;

	state = D6_STATE(ifp);
	if (state) {
		switch (state->state) {
		case DH6S_BOUND:
			dhcp6_startrebind(ifp);
			break;
		case DH6S_INFORMED:
			dhcp6_startinform(ifp);
			break;
		default:
			dhcp6_startdiscover(ifp);
			break;
		}
	}
}

static void
dhcp6_freedrop(struct interface *ifp, int drop, const char *reason)
{
	struct dhcp6_state *state;
	struct dhcpcd_ctx *ctx;
	unsigned long long options;
	int dropdele;

	/*
	 * As the interface is going away from dhcpcd we need to
	 * remove the delegated addresses, otherwise we lose track
	 * of which interface is delegating as we remeber it by pointer.
	 * So if we need to change this behaviour, we need to change
	 * how we remember which interface delegated.
	 *
	 * XXX The below is no longer true due to the change of the
	 * default IAID, but do PPP links have stable ethernet
	 * addresses?
	 *
	 * To make it more interesting, on some OS's with PPP links
	 * there is no guarantee the delegating interface will have
	 * the same name or index so think very hard before changing
	 * this.
	 */
	if (ifp->options)
		options = ifp->options->options;
	else
		options = 0;
	dropdele = (options & (DHCPCD_STOPPING | DHCPCD_RELEASE) &&
	    (options & DHCPCD_NODROP) != DHCPCD_NODROP);

	if (ifp->ctx->eloop)
		eloop_timeout_delete(ifp->ctx->eloop, NULL, ifp);

	if (dropdele)
		dhcp6_delete_delegates(ifp);

	state = D6_STATE(ifp);
	if (state) {
		/* Failure to send the release may cause this function to
		 * re-enter */
		if (state->state == DH6S_RELEASE) {
			dhcp6_finishrelease(ifp);
			return;
		}

		if (drop && options & DHCPCD_RELEASE) {
			if (ifp->carrier == LINK_UP &&
			    state->state != DH6S_RELEASED)
			{
				dhcp6_startrelease(ifp);
				return;
			}
			unlink(state->leasefile);
		}
		dhcp6_freedrop_addrs(ifp, drop, NULL);
		free(state->old);
		state->old = state->new;
		state->old_len = state->new_len;
		state->new = NULL;
		state->new_len = 0;
		if (drop && state->old &&
		    (options & DHCPCD_NODROP) != DHCPCD_NODROP)
		{
			if (reason == NULL)
				reason = "STOP6";
			script_runreason(ifp, reason);
		}
		free(state->old);
		free(state->send);
		free(state->recv);
		free(state);
		ifp->if_data[IF_DATA_DHCP6] = NULL;
	}

	/* If we don't have any more DHCP6 enabled interfaces,
	 * close the global socket and release resources */
	ctx = ifp->ctx;
	if (ctx->ifaces) {
		TAILQ_FOREACH(ifp, ctx->ifaces, next) {
			if (D6_STATE(ifp))
				break;
		}
	}
	if (ifp == NULL && ctx->ipv6) {
		if (ctx->ipv6->dhcp_fd != -1) {
			eloop_event_delete(ctx->eloop, ctx->ipv6->dhcp_fd);
			close(ctx->ipv6->dhcp_fd);
			ctx->ipv6->dhcp_fd = -1;
		}
	}
}

void
dhcp6_drop(struct interface *ifp, const char *reason)
{

	dhcp6_freedrop(ifp, 1, reason);
}

void
dhcp6_free(struct interface *ifp)
{

	dhcp6_freedrop(ifp, 0, NULL);
}

void
dhcp6_handleifa(struct dhcpcd_ctx *ctx, int cmd, const char *ifname,
    const struct in6_addr *addr, int flags)
{
	struct interface *ifp;
	struct dhcp6_state *state;

	if (ctx->ifaces == NULL)
		return;

	TAILQ_FOREACH(ifp, ctx->ifaces, next) {
		state = D6_STATE(ifp);
		if (state == NULL || strcmp(ifp->name, ifname))
			continue;
		ipv6_handleifa_addrs(cmd, &state->addrs, addr, flags);
	}

}

ssize_t
dhcp6_env(char **env, const char *prefix, const struct interface *ifp,
    const struct dhcp6_message *m, size_t len)
{
	const struct if_options *ifo;
	struct dhcp_opt *opt, *vo;
	const struct dhcp6_option *o;
	size_t i, n;
	uint16_t ol, oc;
	char *pfx;
	uint32_t en;
	const struct dhcpcd_ctx *ctx;
	const struct dhcp6_state *state;
	const struct ipv6_addr *ap;
	char *v, *val;

	n = 0;
	if (m == NULL)
		goto delegated;

	if (len < sizeof(*m)) {
		/* Should be impossible with guards at packet in
		 * and reading leases */
		errno = EINVAL;
		return -1;
	}

	ifo = ifp->options;
	ctx = ifp->ctx;

	/* Zero our indexes */
	if (env) {
		for (i = 0, opt = ctx->dhcp6_opts;
		    i < ctx->dhcp6_opts_len;
		    i++, opt++)
			dhcp_zero_index(opt);
		for (i = 0, opt = ifp->options->dhcp6_override;
		    i < ifp->options->dhcp6_override_len;
		    i++, opt++)
			dhcp_zero_index(opt);
		for (i = 0, opt = ctx->vivso;
		    i < ctx->vivso_len;
		    i++, opt++)
			dhcp_zero_index(opt);
		i = strlen(prefix) + strlen("_dhcp6") + 1;
		pfx = malloc(i);
		if (pfx == NULL) {
			logger(ifp->ctx, LOG_ERR, "%s: %m", __func__);
			return -1;
		}
		snprintf(pfx, i, "%s_dhcp6", prefix);
	} else
		pfx = NULL;

	/* Unlike DHCP, DHCPv6 options *may* occur more than once.
	 * There is also no provision for option concatenation unlike DHCP. */
	for (o = D6_CFIRST_OPTION(m);
	    len > (ssize_t)sizeof(*o);
	    o = D6_CNEXT_OPTION(o))
	{
		ol = ntohs(o->len);
		if (sizeof(*o) + ol > len) {
			errno =	EINVAL;
			break;
		}
		len -= sizeof(*o) + ol;
		oc = ntohs(o->code);
		if (has_option_mask(ifo->nomask6, oc))
			continue;
		for (i = 0, opt = ifo->dhcp6_override;
		    i < ifo->dhcp6_override_len;
		    i++, opt++)
			if (opt->option == oc)
				break;
		if (i == ifo->dhcp6_override_len &&
		    oc == D6_OPTION_VENDOR_OPTS &&
		    ol > sizeof(en))
		{
			memcpy(&en, D6_COPTION_DATA(o), sizeof(en));
			en = ntohl(en);
			vo = vivso_find(en, ifp);
		} else
			vo = NULL;
		if (i == ifo->dhcp6_override_len) {
			for (i = 0, opt = ctx->dhcp6_opts;
			    i < ctx->dhcp6_opts_len;
			    i++, opt++)
				if (opt->option == oc)
					break;
			if (i == ctx->dhcp6_opts_len)
				opt = NULL;
		}
		if (opt) {
			n += dhcp_envoption(ifp->ctx,
			    env == NULL ? NULL : &env[n],
			    pfx, ifp->name,
			    opt, dhcp6_getoption, D6_COPTION_DATA(o), ol);
		}
		if (vo) {
			n += dhcp_envoption(ifp->ctx,
			    env == NULL ? NULL : &env[n],
			    pfx, ifp->name,
			    vo, dhcp6_getoption,
			    D6_COPTION_DATA(o) + sizeof(en),
			    ol - sizeof(en));
		}
	}
	free(pfx);

delegated:
        /* Needed for Delegated Prefixes */
	state = D6_CSTATE(ifp);
	i = 0;
	TAILQ_FOREACH(ap, &state->addrs, next) {
		if (ap->delegating_iface) {
			i += strlen(ap->saddr) + 1;
		}
	}
	if (env && i) {
		i += strlen(prefix) + strlen("_delegated_dhcp6_prefix=");
                v = val = env[n] = malloc(i);
		if (v == NULL) {
			logger(ifp->ctx, LOG_ERR, "%s: %m", __func__);
			return -1;
		}
		v += snprintf(val, i, "%s_delegated_dhcp6_prefix=", prefix);
		TAILQ_FOREACH(ap, &state->addrs, next) {
			if (ap->delegating_iface) {
				/* Can't use stpcpy(3) due to "security" */
				const char *sap = ap->saddr;

				do
					*v++ = *sap;
				while (*++sap != '\0');
				*v++ = ' ';
			}
		}
		*--v = '\0';
        }
	if (i)
		n++;

	return (ssize_t)n;
}

int
dhcp6_dump(struct interface *ifp)
{
	struct dhcp6_state *state;

	ifp->if_data[IF_DATA_DHCP6] = state = calloc(1, sizeof(*state));
	if (state == NULL) {
		logger(ifp->ctx, LOG_ERR, "%s: %m", __func__);
		return -1;
	}
	TAILQ_INIT(&state->addrs);
	dhcp_set_leasefile(state->leasefile, sizeof(state->leasefile),
	    AF_INET6, ifp);
	if (dhcp6_readlease(ifp, 0) == -1) {
		logger(ifp->ctx, LOG_ERR, "%s: %s: %m",
		    *ifp->name ? ifp->name : state->leasefile, __func__);
		return -1;
	}
	state->reason = "DUMP6";
	return script_runreason(ifp, state->reason);
}
