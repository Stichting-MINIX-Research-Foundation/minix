#include <sys/cdefs.h>
 __RCSID("$NetBSD: if-options.c,v 1.27 2015/09/04 12:25:01 roy Exp $");

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

#define _WITH_GETLINE /* Stop FreeBSD bitching */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <grp.h>
#include <inttypes.h>
#include <limits.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "config.h"
#include "common.h"
#include "dhcp.h"
#include "dhcp6.h"
#include "dhcpcd-embedded.h"
#include "if.h"
#include "if-options.h"
#include "ipv4.h"

/* These options only make sense in the config file, so don't use any
   valid short options for them */
#define O_BASE			MAX('z', 'Z') + 1
#define O_ARPING		O_BASE + 1
#define O_FALLBACK		O_BASE + 2
#define O_DESTINATION		O_BASE + 3
#define O_IPV6RS		O_BASE + 4
#define O_NOIPV6RS		O_BASE + 5
#define O_IPV6RA_FORK		O_BASE + 6
#define O_IPV6RA_OWN		O_BASE + 7
#define O_IPV6RA_OWN_D		O_BASE + 8
#define O_NOALIAS		O_BASE + 9
#define O_IA_NA			O_BASE + 10
#define O_IA_TA			O_BASE + 11
#define O_IA_PD			O_BASE + 12
#define O_HOSTNAME_SHORT	O_BASE + 13
#define O_DEV			O_BASE + 14
#define O_NODEV			O_BASE + 15
#define O_NOIPV4		O_BASE + 16
#define O_NOIPV6		O_BASE + 17
#define O_IAID			O_BASE + 18
#define O_DEFINE		O_BASE + 19
#define O_DEFINE6		O_BASE + 20
#define O_EMBED			O_BASE + 21
#define O_ENCAP			O_BASE + 22
#define O_VENDOPT		O_BASE + 23
#define O_VENDCLASS		O_BASE + 24
#define O_AUTHPROTOCOL		O_BASE + 25
#define O_AUTHTOKEN		O_BASE + 26
#define O_AUTHNOTREQUIRED	O_BASE + 27
#define O_NODHCP		O_BASE + 28
#define O_NODHCP6		O_BASE + 29
#define O_DHCP			O_BASE + 30
#define O_DHCP6			O_BASE + 31
#define O_IPV4			O_BASE + 32
#define O_IPV6			O_BASE + 33
#define O_CONTROLGRP		O_BASE + 34
#define O_SLAAC			O_BASE + 35
#define O_GATEWAY		O_BASE + 36
#define O_NOUP			O_BASE + 37
#define O_IPV6RA_AUTOCONF	O_BASE + 38
#define O_IPV6RA_NOAUTOCONF	O_BASE + 39
#define O_REJECT		O_BASE + 40
#define O_IPV6RA_ACCEPT_NOPUBLIC	O_BASE + 41
#define O_BOOTP			O_BASE + 42
#define O_DEFINEND		O_BASE + 43
#define O_NODELAY		O_BASE + 44

const struct option cf_options[] = {
	{"background",      no_argument,       NULL, 'b'},
	{"script",          required_argument, NULL, 'c'},
	{"debug",           no_argument,       NULL, 'd'},
	{"env",             required_argument, NULL, 'e'},
	{"config",          required_argument, NULL, 'f'},
	{"reconfigure",     no_argument,       NULL, 'g'},
	{"hostname",        optional_argument, NULL, 'h'},
	{"vendorclassid",   optional_argument, NULL, 'i'},
	{"logfile",         required_argument, NULL, 'j'},
	{"release",         no_argument,       NULL, 'k'},
	{"leasetime",       required_argument, NULL, 'l'},
	{"metric",          required_argument, NULL, 'm'},
	{"rebind",          no_argument,       NULL, 'n'},
	{"option",          required_argument, NULL, 'o'},
	{"persistent",      no_argument,       NULL, 'p'},
	{"quiet",           no_argument,       NULL, 'q'},
	{"request",         optional_argument, NULL, 'r'},
	{"inform",          optional_argument, NULL, 's'},
	{"timeout",         required_argument, NULL, 't'},
	{"userclass",       required_argument, NULL, 'u'},
	{"vendor",          required_argument, NULL, 'v'},
	{"waitip",          optional_argument, NULL, 'w'},
	{"exit",            no_argument,       NULL, 'x'},
	{"allowinterfaces", required_argument, NULL, 'z'},
	{"reboot",          required_argument, NULL, 'y'},
	{"noarp",           no_argument,       NULL, 'A'},
	{"nobackground",    no_argument,       NULL, 'B'},
	{"nohook",          required_argument, NULL, 'C'},
	{"duid",            no_argument,       NULL, 'D'},
	{"lastlease",       no_argument,       NULL, 'E'},
	{"fqdn",            optional_argument, NULL, 'F'},
	{"nogateway",       no_argument,       NULL, 'G'},
	{"xidhwaddr",       no_argument,       NULL, 'H'},
	{"clientid",        optional_argument, NULL, 'I'},
	{"broadcast",       no_argument,       NULL, 'J'},
	{"nolink",          no_argument,       NULL, 'K'},
	{"noipv4ll",        no_argument,       NULL, 'L'},
	{"master",          no_argument,       NULL, 'M'},
	{"nooption",        optional_argument, NULL, 'O'},
	{"require",         required_argument, NULL, 'Q'},
	{"static",          required_argument, NULL, 'S'},
	{"test",            no_argument,       NULL, 'T'},
	{"dumplease",       no_argument,       NULL, 'U'},
	{"variables",       no_argument,       NULL, 'V'},
	{"whitelist",       required_argument, NULL, 'W'},
	{"blacklist",       required_argument, NULL, 'X'},
	{"denyinterfaces",  required_argument, NULL, 'Z'},
	{"arping",          required_argument, NULL, O_ARPING},
	{"destination",     required_argument, NULL, O_DESTINATION},
	{"fallback",        required_argument, NULL, O_FALLBACK},
	{"ipv6rs",          no_argument,       NULL, O_IPV6RS},
	{"noipv6rs",        no_argument,       NULL, O_NOIPV6RS},
	{"ipv6ra_autoconf", no_argument,       NULL, O_IPV6RA_AUTOCONF},
	{"ipv6ra_noautoconf", no_argument,     NULL, O_IPV6RA_NOAUTOCONF},
	{"ipv6ra_fork",     no_argument,       NULL, O_IPV6RA_FORK},
	{"ipv6ra_own",      no_argument,       NULL, O_IPV6RA_OWN},
	{"ipv6ra_own_default", no_argument,    NULL, O_IPV6RA_OWN_D},
	{"ipv6ra_accept_nopublic", no_argument, NULL, O_IPV6RA_ACCEPT_NOPUBLIC},
	{"ipv4only",        no_argument,       NULL, '4'},
	{"ipv6only",        no_argument,       NULL, '6'},
	{"ipv4",            no_argument,       NULL, O_IPV4},
	{"noipv4",          no_argument,       NULL, O_NOIPV4},
	{"ipv6",            no_argument,       NULL, O_IPV6},
	{"noipv6",          no_argument,       NULL, O_NOIPV6},
	{"noalias",         no_argument,       NULL, O_NOALIAS},
	{"iaid",            required_argument, NULL, O_IAID},
	{"ia_na",           no_argument,       NULL, O_IA_NA},
	{"ia_ta",           no_argument,       NULL, O_IA_TA},
	{"ia_pd",           no_argument,       NULL, O_IA_PD},
	{"hostname_short",  no_argument,       NULL, O_HOSTNAME_SHORT},
	{"dev",             required_argument, NULL, O_DEV},
	{"nodev",           no_argument,       NULL, O_NODEV},
	{"define",          required_argument, NULL, O_DEFINE},
	{"definend",        required_argument, NULL, O_DEFINEND},
	{"define6",         required_argument, NULL, O_DEFINE6},
	{"embed",           required_argument, NULL, O_EMBED},
	{"encap",           required_argument, NULL, O_ENCAP},
	{"vendopt",         required_argument, NULL, O_VENDOPT},
	{"vendclass",       required_argument, NULL, O_VENDCLASS},
	{"authprotocol",    required_argument, NULL, O_AUTHPROTOCOL},
	{"authtoken",       required_argument, NULL, O_AUTHTOKEN},
	{"noauthrequired",  no_argument,       NULL, O_AUTHNOTREQUIRED},
	{"dhcp",            no_argument,       NULL, O_DHCP},
	{"nodhcp",          no_argument,       NULL, O_NODHCP},
	{"dhcp6",           no_argument,       NULL, O_DHCP6},
	{"nodhcp6",         no_argument,       NULL, O_NODHCP6},
	{"controlgroup",    required_argument, NULL, O_CONTROLGRP},
	{"slaac",           required_argument, NULL, O_SLAAC},
	{"gateway",         no_argument,       NULL, O_GATEWAY},
	{"reject",          required_argument, NULL, O_REJECT},
	{"bootp",           no_argument,       NULL, O_BOOTP},
	{"nodelay",         no_argument,       NULL, O_NODELAY},
	{"noup",            no_argument,       NULL, O_NOUP},
	{NULL,              0,                 NULL, '\0'}
};

static char *
add_environ(struct dhcpcd_ctx *ctx, struct if_options *ifo,
    const char *value, int uniq)
{
	char **newlist;
	char **lst = ifo->environ;
	size_t i = 0, l, lv;
	char *match = NULL, *p, *n;

	match = strdup(value);
	if (match == NULL) {
		logger(ctx, LOG_ERR, "%s: %m", __func__);
		return NULL;
	}
	p = strchr(match, '=');
	if (p == NULL) {
		logger(ctx, LOG_ERR, "%s: no assignment: %s", __func__, value);
		free(match);
		return NULL;
	}
	*p++ = '\0';
	l = strlen(match);

	while (lst && lst[i]) {
		if (match && strncmp(lst[i], match, l) == 0) {
			if (uniq) {
				n = strdup(value);
				if (n == NULL) {
					logger(ctx, LOG_ERR,
					    "%s: %m", __func__);
					free(match);
					return NULL;
				}
				free(lst[i]);
				lst[i] = n;
			} else {
				/* Append a space and the value to it */
				l = strlen(lst[i]);
				lv = strlen(p);
				n = realloc(lst[i], l + lv + 2);
				if (n == NULL) {
					logger(ctx, LOG_ERR,
					    "%s: %m", __func__);
					free(match);
					return NULL;
				}
				lst[i] = n;
				lst[i][l] = ' ';
				memcpy(lst[i] + l + 1, p, lv);
				lst[i][l + lv + 1] = '\0';
			}
			free(match);
			return lst[i];
		}
		i++;
	}

	free(match);
	n = strdup(value);
	if (n == NULL) {
		logger(ctx, LOG_ERR, "%s: %m", __func__);
		return NULL;
	}
	newlist = realloc(lst, sizeof(char *) * (i + 2));
	if (newlist == NULL) {
		logger(ctx, LOG_ERR, "%s: %m", __func__);
		free(n);
		return NULL;
	}
	newlist[i] = n;
	newlist[i + 1] = NULL;
	ifo->environ = newlist;
	return newlist[i];
}

#define parse_string(buf, len, arg) parse_string_hwaddr(buf, len, arg, 0)
static ssize_t
parse_string_hwaddr(char *sbuf, size_t slen, const char *str, int clid)
{
	size_t l;
	const char *p;
	int i, punt_last = 0;
	char c[4];

	/* If surrounded by quotes then it's a string */
	if (*str == '"') {
		str++;
		l = strlen(str);
		p = str + l - 1;
		if (*p == '"')
			punt_last = 1;
	} else {
		l = (size_t)hwaddr_aton(NULL, str);
		if ((ssize_t) l != -1 && l > 1) {
			if (l > slen) {
				errno = ENOBUFS;
				return -1;
			}
			hwaddr_aton((uint8_t *)sbuf, str);
			return (ssize_t)l;
		}
	}

	/* Process escapes */
	l = 0;
	/* If processing a string on the clientid, first byte should be
	 * 0 to indicate a non hardware type */
	if (clid && *str) {
		if (sbuf)
			*sbuf++ = 0;
		l++;
	}
	c[3] = '\0';
	while (*str) {
		if (++l > slen && sbuf) {
			errno = ENOBUFS;
			return -1;
		}
		if (*str == '\\') {
			str++;
			switch(*str) {
			case '\0':
				break;
			case 'b':
				if (sbuf)
					*sbuf++ = '\b';
				str++;
				break;
			case 'n':
				if (sbuf)
					*sbuf++ = '\n';
				str++;
				break;
			case 'r':
				if (sbuf)
					*sbuf++ = '\r';
				str++;
				break;
			case 't':
				if (sbuf)
					*sbuf++ = '\t';
				str++;
				break;
			case 'x':
				/* Grab a hex code */
				c[1] = '\0';
				for (i = 0; i < 2; i++) {
					if (isxdigit((unsigned char)*str) == 0)
						break;
					c[i] = *str++;
				}
				if (c[1] != '\0' && sbuf) {
					c[2] = '\0';
					*sbuf++ = (char)strtol(c, NULL, 16);
				} else
					l--;
				break;
			case '0':
				/* Grab an octal code */
				c[2] = '\0';
				for (i = 0; i < 3; i++) {
					if (*str < '0' || *str > '7')
						break;
					c[i] = *str++;
				}
				if (c[2] != '\0' && sbuf) {
					i = (int)strtol(c, NULL, 8);
					if (i > 255)
						i = 255;
					*sbuf ++= (char)i;
				} else
					l--;
				break;
			default:
				if (sbuf)
					*sbuf++ = *str;
				str++;
				break;
			}
		} else {
			if (sbuf)
				*sbuf++ = *str;
			str++;
		}
	}
	if (punt_last) {
		if (sbuf)
			*--sbuf = '\0';
		l--;
	}
	return (ssize_t)l;
}

static int
parse_iaid1(uint8_t *iaid, const char *arg, size_t len, int n)
{
	int e;
	uint32_t narg;
	ssize_t s;

	narg = (uint32_t)strtou(arg, NULL, 0, 0, UINT32_MAX, &e);
	if (e == 0) {
		if (n)
			narg = htonl(narg);
		memcpy(iaid, &narg, sizeof(narg));
		return 0;
	}

	if ((s = parse_string((char *)iaid, len, arg)) < 1)
		return -1;
	if (s < 4)
		iaid[3] = '\0';
	if (s < 3)
		iaid[2] = '\0';
	if (s < 2)
		iaid[1] = '\0';
	return 0;
}

static int
parse_iaid(uint8_t *iaid, const char *arg, size_t len)
{

	return parse_iaid1(iaid, arg, len, 1);
}

static int
parse_uint32(uint32_t *i, const char *arg)
{

	return parse_iaid1((uint8_t *)i, arg, sizeof(uint32_t), 0);
}

static char **
splitv(struct dhcpcd_ctx *ctx, int *argc, char **argv, const char *arg)
{
	char **n, **v = argv;
	char *o = strdup(arg), *p, *t, *nt;

	if (o == NULL) {
		logger(ctx, LOG_ERR, "%s: %m", __func__);
		return v;
	}
	p = o;
	while ((t = strsep(&p, ", "))) {
		nt = strdup(t);
		if (nt == NULL) {
			logger(ctx, LOG_ERR, "%s: %m", __func__);
			free(o);
			return v;
		}
		n = realloc(v, sizeof(char *) * ((size_t)(*argc) + 1));
		if (n == NULL) {
			logger(ctx, LOG_ERR, "%s: %m", __func__);
			free(o);
			free(nt);
			return v;
		}
		v = n;
		v[(*argc)++] = nt;
	}
	free(o);
	return v;
}

#ifdef INET
static int
parse_addr(struct dhcpcd_ctx *ctx,
    struct in_addr *addr, struct in_addr *net, const char *arg)
{
	char *p;

	if (arg == NULL || *arg == '\0') {
		if (addr != NULL)
			addr->s_addr = 0;
		if (net != NULL)
			net->s_addr = 0;
		return 0;
	}
	if ((p = strchr(arg, '/')) != NULL) {
		int e;
		intmax_t i;

		*p++ = '\0';
		i = strtoi(p, NULL, 10, 0, 32, &e);
		if (e != 0 ||
		    (net != NULL && inet_cidrtoaddr((int)i, net) != 0))
		{
			logger(ctx, LOG_ERR, "`%s' is not a valid CIDR", p);
			return -1;
		}
	}

	if (addr != NULL && inet_aton(arg, addr) == 0) {
		logger(ctx, LOG_ERR, "`%s' is not a valid IP address", arg);
		return -1;
	}
	if (p != NULL)
		*--p = '/';
	else if (net != NULL && addr != NULL)
		net->s_addr = ipv4_getnetmask(addr->s_addr);
	return 0;
}
#else
static int
parse_addr(struct dhcpcd_ctx *ctx,
    __unused struct in_addr *addr, __unused struct in_addr *net,
    __unused const char *arg)
{

	logger(ctx, LOG_ERR, "No IPv4 support");
	return -1;
}
#endif

static const char *
set_option_space(struct dhcpcd_ctx *ctx,
    const char *arg,
    const struct dhcp_opt **d, size_t *dl,
    const struct dhcp_opt **od, size_t *odl,
    struct if_options *ifo,
    uint8_t *request[], uint8_t *require[], uint8_t *no[], uint8_t *reject[])
{

#if !defined(INET) && !defined(INET6)
	UNUSED(ctx);
#endif

#ifdef INET6
	if (strncmp(arg, "nd_", strlen("nd_")) == 0) {
		*d = ctx->nd_opts;
		*dl = ctx->nd_opts_len;
		*od = ifo->nd_override;
		*odl = ifo->nd_override_len;
		*request = ifo->requestmasknd;
		*require = ifo->requiremasknd;
		*no = ifo->nomasknd;
		*reject = ifo->rejectmasknd;
		return arg + strlen("nd_");
	}

	if (strncmp(arg, "dhcp6_", strlen("dhcp6_")) == 0) {
		*d = ctx->dhcp6_opts;
		*dl = ctx->dhcp6_opts_len;
		*od = ifo->dhcp6_override;
		*odl = ifo->dhcp6_override_len;
		*request = ifo->requestmask6;
		*require = ifo->requiremask6;
		*no = ifo->nomask6;
		*reject = ifo->rejectmask6;
		return arg + strlen("dhcp6_");
	}
#endif

#ifdef INET
	*d = ctx->dhcp_opts;
	*dl = ctx->dhcp_opts_len;
	*od = ifo->dhcp_override;
	*odl = ifo->dhcp_override_len;
#else
	*d = NULL;
	*dl = 0;
	*od = NULL;
	*odl = 0;
#endif
	*request = ifo->requestmask;
	*require = ifo->requiremask;
	*no = ifo->nomask;
	*reject = ifo->rejectmask;
	return arg;
}

void
free_dhcp_opt_embenc(struct dhcp_opt *opt)
{
	size_t i;
	struct dhcp_opt *o;

	free(opt->var);

	for (i = 0, o = opt->embopts; i < opt->embopts_len; i++, o++)
		free_dhcp_opt_embenc(o);
	free(opt->embopts);
	opt->embopts_len = 0;
	opt->embopts = NULL;

	for (i = 0, o = opt->encopts; i < opt->encopts_len; i++, o++)
		free_dhcp_opt_embenc(o);
	free(opt->encopts);
	opt->encopts_len = 0;
	opt->encopts = NULL;
}

static char *
strwhite(const char *s)
{

	if (s == NULL)
		return NULL;
	while (*s != ' ' && *s != '\t') {
		if (*s == '\0')
			return NULL;
		s++;
	}
	return UNCONST(s);
}

static char *
strskipwhite(const char *s)
{

	if (s == NULL)
		return NULL;
	while (*s == ' ' || *s == '\t') {
		if (*s == '\0')
			return NULL;
		s++;
	}
	return UNCONST(s);
}

/* Find the end pointer of a string. */
static char *
strend(const char *s)
{

	s = strskipwhite(s);
	if (s == NULL)
		return NULL;
	if (*s != '"')
		return strchr(s, ' ');
	s++;
	for (; *s != '"' ; s++) {
		if (*s == '\0')
			return NULL;
		if (*s == '\\') {
			if (*(++s) == '\0')
				return NULL;
		}
	}
	return UNCONST(++s);
}

static int
parse_option(struct dhcpcd_ctx *ctx, const char *ifname, struct if_options *ifo,
    int opt, const char *arg, struct dhcp_opt **ldop, struct dhcp_opt **edop)
{
	int e, i, t;
	long l;
	unsigned long u;
	char *p = NULL, *bp, *fp, *np, **nconf;
	ssize_t s;
	struct in_addr addr, addr2;
	in_addr_t *naddr;
	struct rt *rt;
	const struct dhcp_opt *d, *od;
	uint8_t *request, *require, *no, *reject;
	struct dhcp_opt **dop, *ndop;
	size_t *dop_len, dl, odl;
	struct vivco *vivco;
	struct token *token;
	struct group *grp;
#ifdef _REENTRANT
	struct group grpbuf;
#endif
#ifdef INET6
	size_t sl;
	struct if_ia *ia;
	uint8_t iaid[4];
	struct if_sla *sla, *slap;
#endif

	dop = NULL;
	dop_len = NULL;
#ifdef INET6
	i = 0;
#endif
	switch(opt) {
	case 'f': /* FALLTHROUGH */
	case 'g': /* FALLTHROUGH */
	case 'n': /* FALLTHROUGH */
	case 'x': /* FALLTHROUGH */
	case 'T': /* FALLTHROUGH */
	case 'U': /* FALLTHROUGH */
	case 'V': /* We need to handle non interface options */
		break;
	case 'b':
		ifo->options |= DHCPCD_BACKGROUND;
		break;
	case 'c':
		free(ifo->script);
		ifo->script = strdup(arg);
		if (ifo->script == NULL)
			logger(ctx, LOG_ERR, "%s: %m", __func__);
		break;
	case 'd':
		ifo->options |= DHCPCD_DEBUG;
		break;
	case 'e':
		add_environ(ctx, ifo, arg, 1);
		break;
	case 'h':
		if (!arg) {
			ifo->options |= DHCPCD_HOSTNAME;
			break;
		}
		s = parse_string(ifo->hostname, HOSTNAME_MAX_LEN, arg);
		if (s == -1) {
			logger(ctx, LOG_ERR, "hostname: %m");
			return -1;
		}
		if (s != 0 && ifo->hostname[0] == '.') {
			logger(ctx, LOG_ERR, "hostname cannot begin with .");
			return -1;
		}
		ifo->hostname[s] = '\0';
		if (ifo->hostname[0] == '\0')
			ifo->options &= ~DHCPCD_HOSTNAME;
		else
			ifo->options |= DHCPCD_HOSTNAME;
		break;
	case 'i':
		if (arg)
			s = parse_string((char *)ifo->vendorclassid + 1,
			    VENDORCLASSID_MAX_LEN, arg);
		else
			s = 0;
		if (s == -1) {
			logger(ctx, LOG_ERR, "vendorclassid: %m");
			return -1;
		}
		*ifo->vendorclassid = (uint8_t)s;
		break;
	case 'j':
		/* per interface logging is not supported
		 * don't want to overide the commandline */
		if (ifname == NULL && ctx->logfile == NULL) {
			logger_close(ctx);
			ctx->logfile = strdup(arg);
			logger_open(ctx);
		}
		break;
	case 'k':
		ifo->options |= DHCPCD_RELEASE;
		break;
	case 'l':
		ifo->leasetime = (uint32_t)strtou(arg, NULL,
		    0, 0, UINT32_MAX, &e);
		if (e) {
			logger(ctx, LOG_ERR, "failed to convert leasetime %s", arg);
			return -1;
		}
		break;
	case 'm':
		ifo->metric = (int)strtoi(arg, NULL, 0, 0, INT32_MAX, &e);
		if (e) {
			logger(ctx, LOG_ERR, "failed to convert metric %s", arg);
			return -1;
		}
		break;
	case 'o':
		arg = set_option_space(ctx, arg, &d, &dl, &od, &odl, ifo,
		    &request, &require, &no, &reject);
		if (make_option_mask(d, dl, od, odl, request, arg, 1) != 0 ||
		    make_option_mask(d, dl, od, odl, no, arg, -1) != 0 ||
		    make_option_mask(d, dl, od, odl, reject, arg, -1) != 0)
		{
			logger(ctx, LOG_ERR, "unknown option `%s'", arg);
			return -1;
		}
		break;
	case O_REJECT:
		arg = set_option_space(ctx, arg, &d, &dl, &od, &odl, ifo,
		    &request, &require, &no, &reject);
		if (make_option_mask(d, dl, od, odl, reject, arg, 1) != 0 ||
		    make_option_mask(d, dl, od, odl, request, arg, -1) != 0 ||
		    make_option_mask(d, dl, od, odl, require, arg, -1) != 0)
		{
			logger(ctx, LOG_ERR, "unknown option `%s'", arg);
			return -1;
		}
		break;
	case 'p':
		ifo->options |= DHCPCD_PERSISTENT;
		break;
	case 'q':
		ifo->options |= DHCPCD_QUIET;
		break;
	case 'r':
		if (parse_addr(ctx, &ifo->req_addr, NULL, arg) != 0)
			return -1;
		ifo->options |= DHCPCD_REQUEST;
		ifo->req_mask.s_addr = 0;
		break;
	case 's':
		if (ifo->options & DHCPCD_IPV6 &&
		    !(ifo->options & DHCPCD_IPV4))
		{
			ifo->options |= DHCPCD_INFORM;
			break;
		}
		if (arg && *arg != '\0') {
			if (parse_addr(ctx,
			    &ifo->req_addr, &ifo->req_mask, arg) != 0)
				return -1;
		} else {
			ifo->req_addr.s_addr = 0;
			ifo->req_mask.s_addr = 0;
		}
		ifo->options |= DHCPCD_INFORM | DHCPCD_PERSISTENT;
		ifo->options &= ~DHCPCD_STATIC;
		break;
	case 't':
		ifo->timeout = (time_t)strtoi(arg, NULL, 0, 0, INT32_MAX, &e);
		if (e) {
			logger(ctx, LOG_ERR, "failed to convert timeout");
			return -1;
		}
		break;
	case 'u':
		s = USERCLASS_MAX_LEN - ifo->userclass[0] - 1;
		s = parse_string((char *)ifo->userclass +
		    ifo->userclass[0] + 2, (size_t)s, arg);
		if (s == -1) {
			logger(ctx, LOG_ERR, "userclass: %m");
			return -1;
		}
		if (s != 0) {
			ifo->userclass[ifo->userclass[0] + 1] = (uint8_t)s;
			ifo->userclass[0] = (uint8_t)(ifo->userclass[0] + s +1);
		}
		break;
	case 'v':
		p = strchr(arg, ',');
		if (!p || !p[1]) {
			logger(ctx, LOG_ERR, "invalid vendor format: %s", arg);
			return -1;
		}

		/* If vendor starts with , then it is not encapsulated */
		if (p == arg) {
			arg++;
			s = parse_string((char *)ifo->vendor + 1,
			    VENDOR_MAX_LEN, arg);
			if (s == -1) {
				logger(ctx, LOG_ERR, "vendor: %m");
				return -1;
			}
			ifo->vendor[0] = (uint8_t)s;
			ifo->options |= DHCPCD_VENDORRAW;
			break;
		}

		/* Encapsulated vendor options */
		if (ifo->options & DHCPCD_VENDORRAW) {
			ifo->options &= ~DHCPCD_VENDORRAW;
			ifo->vendor[0] = 0;
		}

		/* Strip and preserve the comma */
		*p = '\0';
		i = (int)strtoi(arg, NULL, 0, 1, 254, &e);
		*p = ',';
		if (e) {
			logger(ctx, LOG_ERR, "vendor option should be between"
			    " 1 and 254 inclusive");
			return -1;
		}

		arg = p + 1;
		s = VENDOR_MAX_LEN - ifo->vendor[0] - 2;
		if (inet_aton(arg, &addr) == 1) {
			if (s < 6) {
				s = -1;
				errno = ENOBUFS;
			} else {
				memcpy(ifo->vendor + ifo->vendor[0] + 3,
				    &addr.s_addr, sizeof(addr.s_addr));
				s = sizeof(addr.s_addr);
			}
		} else {
			s = parse_string((char *)ifo->vendor +
			    ifo->vendor[0] + 3, (size_t)s, arg);
		}
		if (s == -1) {
			logger(ctx, LOG_ERR, "vendor: %m");
			return -1;
		}
		if (s != 0) {
			ifo->vendor[ifo->vendor[0] + 1] = (uint8_t)i;
			ifo->vendor[ifo->vendor[0] + 2] = (uint8_t)s;
			ifo->vendor[0] = (uint8_t)(ifo->vendor[0] + s + 2);
		}
		break;
	case 'w':
		ifo->options |= DHCPCD_WAITIP;
		if (arg != NULL && arg[0] != '\0') {
			if (arg[0] == '4' || arg[1] == '4')
				ifo->options |= DHCPCD_WAITIP4;
			if (arg[0] == '6' || arg[1] == '6')
				ifo->options |= DHCPCD_WAITIP6;
		}
		break;
	case 'y':
		ifo->reboot = (time_t)strtoi(arg, NULL, 0, 0, UINT32_MAX, &e);
		if (e) {
			logger(ctx, LOG_ERR, "failed to convert reboot %s", arg);
			return -1;
		}
		break;
	case 'z':
		if (ifname == NULL)
			ctx->ifav = splitv(ctx, &ctx->ifac, ctx->ifav, arg);
		break;
	case 'A':
		ifo->options &= ~DHCPCD_ARP;
		/* IPv4LL requires ARP */
		ifo->options &= ~DHCPCD_IPV4LL;
		break;
	case 'B':
		ifo->options &= ~DHCPCD_DAEMONISE;
		break;
	case 'C':
		/* Commas to spaces for shell */
		while ((p = strchr(arg, ',')))
			*p = ' ';
		dl = strlen("skip_hooks=") + strlen(arg) + 1;
		p = malloc(sizeof(char) * dl);
		if (p == NULL) {
			logger(ctx, LOG_ERR, "%s: %m", __func__);
			return -1;
		}
		snprintf(p, dl, "skip_hooks=%s", arg);
		add_environ(ctx, ifo, p, 0);
		free(p);
		break;
	case 'D':
		ifo->options |= DHCPCD_CLIENTID | DHCPCD_DUID;
		break;
	case 'E':
		ifo->options |= DHCPCD_LASTLEASE;
		break;
	case 'F':
		if (!arg) {
			ifo->fqdn = FQDN_BOTH;
			break;
		}
		if (strcmp(arg, "none") == 0)
			ifo->fqdn = FQDN_NONE;
		else if (strcmp(arg, "ptr") == 0)
			ifo->fqdn = FQDN_PTR;
		else if (strcmp(arg, "both") == 0)
			ifo->fqdn = FQDN_BOTH;
		else if (strcmp(arg, "disable") == 0)
			ifo->fqdn = FQDN_DISABLE;
		else {
			logger(ctx, LOG_ERR, "invalid value `%s' for FQDN", arg);
			return -1;
		}
		break;
	case 'G':
		ifo->options &= ~DHCPCD_GATEWAY;
		break;
	case 'H':
		ifo->options |= DHCPCD_XID_HWADDR;
		break;
	case 'I':
		/* Strings have a type of 0 */;
		ifo->clientid[1] = 0;
		if (arg)
			s = parse_string_hwaddr((char *)ifo->clientid + 1,
			    CLIENTID_MAX_LEN, arg, 1);
		else
			s = 0;
		if (s == -1) {
			logger(ctx, LOG_ERR, "clientid: %m");
			return -1;
		}
		ifo->options |= DHCPCD_CLIENTID;
		ifo->clientid[0] = (uint8_t)s;
		break;
	case 'J':
		ifo->options |= DHCPCD_BROADCAST;
		break;
	case 'K':
		ifo->options &= ~DHCPCD_LINK;
		break;
	case 'L':
		ifo->options &= ~DHCPCD_IPV4LL;
		break;
	case 'M':
		ifo->options |= DHCPCD_MASTER;
		break;
	case 'O':
		arg = set_option_space(ctx, arg, &d, &dl, &od, &odl, ifo,
		    &request, &require, &no, &reject);
		if (make_option_mask(d, dl, od, odl, request, arg, -1) != 0 ||
		    make_option_mask(d, dl, od, odl, require, arg, -1) != 0 ||
		    make_option_mask(d, dl, od, odl, no, arg, 1) != 0)
		{
			logger(ctx, LOG_ERR, "unknown option `%s'", arg);
			return -1;
		}
		break;
	case 'Q':
		arg = set_option_space(ctx, arg, &d, &dl, &od, &odl, ifo,
		    &request, &require, &no, &reject);
		if (make_option_mask(d, dl, od, odl, require, arg, 1) != 0 ||
		    make_option_mask(d, dl, od, odl, request, arg, 1) != 0 ||
		    make_option_mask(d, dl, od, odl, no, arg, -1) != 0 ||
		    make_option_mask(d, dl, od, odl, reject, arg, -1) != 0)
		{
			logger(ctx, LOG_ERR, "unknown option `%s'", arg);
			return -1;
		}
		break;
	case 'S':
		p = strchr(arg, '=');
		if (p == NULL) {
			logger(ctx, LOG_ERR, "static assignment required");
			return -1;
		}
		p++;
		if (strncmp(arg, "ip_address=", strlen("ip_address=")) == 0) {
			if (parse_addr(ctx, &ifo->req_addr,
			    ifo->req_mask.s_addr == 0 ? &ifo->req_mask : NULL,
			    p) != 0)
				return -1;

			ifo->options |= DHCPCD_STATIC;
			ifo->options &= ~DHCPCD_INFORM;
		} else if (strncmp(arg, "subnet_mask=",
		    strlen("subnet_mask=")) == 0)
		{
			if (parse_addr(ctx, &ifo->req_mask, NULL, p) != 0)
				return -1;
		} else if (strncmp(arg, "routes=", strlen("routes=")) == 0 ||
		    strncmp(arg, "static_routes=",
		        strlen("static_routes=")) == 0 ||
		    strncmp(arg, "classless_static_routes=",
		        strlen("classless_static_routes=")) == 0 ||
		    strncmp(arg, "ms_classless_static_routes=",
		        strlen("ms_classless_static_routes=")) == 0)
		{
			fp = np = strwhite(p);
			if (np == NULL) {
				logger(ctx, LOG_ERR,
				    "all routes need a gateway");
				return -1;
			}
			*np++ = '\0';
			np = strskipwhite(np);
			if (ifo->routes == NULL) {
				ifo->routes = malloc(sizeof(*ifo->routes));
				if (ifo->routes == NULL) {
					logger(ctx, LOG_ERR,
					    "%s: %m", __func__);
					return -1;
				}
				TAILQ_INIT(ifo->routes);
			}
			rt = calloc(1, sizeof(*rt));
			if (rt == NULL) {
				logger(ctx, LOG_ERR, "%s: %m", __func__);
				*fp = ' ';
				return -1;
			}
			if (parse_addr(ctx, &rt->dest, &rt->net, p) == -1 ||
			    parse_addr(ctx, &rt->gate, NULL, np) == -1)
			{
				free(rt);
				*fp = ' ';
				return -1;
			}
			TAILQ_INSERT_TAIL(ifo->routes, rt, next);
			*fp = ' ';
		} else if (strncmp(arg, "routers=", strlen("routers=")) == 0) {
			if (ifo->routes == NULL) {
				ifo->routes = malloc(sizeof(*ifo->routes));
				if (ifo->routes == NULL) {
					logger(ctx, LOG_ERR,
					    "%s: %m", __func__);
					return -1;
				}
				TAILQ_INIT(ifo->routes);
			}
			rt = calloc(1, sizeof(*rt));
			if (rt == NULL) {
				logger(ctx, LOG_ERR, "%s: %m", __func__);
				return -1;
			}
			rt->dest.s_addr = INADDR_ANY;
			rt->net.s_addr = INADDR_ANY;
			if (parse_addr(ctx, &rt->gate, NULL, p) == -1) {
				free(rt);
				return -1;
			}
			TAILQ_INSERT_TAIL(ifo->routes, rt, next);
		} else if (strncmp(arg, "interface_mtu=",
		    strlen("interface_mtu=")) == 0 ||
		    strncmp(arg, "mtu=", strlen("mtu=")) == 0)
		{
			ifo->mtu = (unsigned int)strtou(p, NULL, 0,
			    MTU_MIN, MTU_MAX, &e);
			if (e) {
				logger(ctx, LOG_ERR, "invalid MTU %s", p);
				return -1;
			}
		} else {
			dl = 0;
			if (ifo->config != NULL) {
				while (ifo->config[dl] != NULL) {
					if (strncmp(ifo->config[dl], arg,
						(size_t)(p - arg)) == 0)
					{
						p = strdup(arg);
						if (p == NULL) {
							logger(ctx, LOG_ERR,
							    "%s: %m", __func__);
							return -1;
						}
						free(ifo->config[dl]);
						ifo->config[dl] = p;
						return 1;
					}
					dl++;
				}
			}
			p = strdup(arg);
			if (p == NULL) {
				logger(ctx, LOG_ERR, "%s: %m", __func__);
				return -1;
			}
			nconf = realloc(ifo->config, sizeof(char *) * (dl + 2));
			if (nconf == NULL) {
				logger(ctx, LOG_ERR, "%s: %m", __func__);
				return -1;
			}
			ifo->config = nconf;
			ifo->config[dl] = p;
			ifo->config[dl + 1] = NULL;
		}
		break;
	case 'W':
		if (parse_addr(ctx, &addr, &addr2, arg) != 0)
			return -1;
		if (strchr(arg, '/') == NULL)
			addr2.s_addr = INADDR_BROADCAST;
		naddr = realloc(ifo->whitelist,
		    sizeof(in_addr_t) * (ifo->whitelist_len + 2));
		if (naddr == NULL) {
			logger(ctx, LOG_ERR, "%s: %m", __func__);
			return -1;
		}
		ifo->whitelist = naddr;
		ifo->whitelist[ifo->whitelist_len++] = addr.s_addr;
		ifo->whitelist[ifo->whitelist_len++] = addr2.s_addr;
		break;
	case 'X':
		if (parse_addr(ctx, &addr, &addr2, arg) != 0)
			return -1;
		if (strchr(arg, '/') == NULL)
			addr2.s_addr = INADDR_BROADCAST;
		naddr = realloc(ifo->blacklist,
		    sizeof(in_addr_t) * (ifo->blacklist_len + 2));
		if (naddr == NULL) {
			logger(ctx, LOG_ERR, "%s: %m", __func__);
			return -1;
		}
		ifo->blacklist = naddr;
		ifo->blacklist[ifo->blacklist_len++] = addr.s_addr;
		ifo->blacklist[ifo->blacklist_len++] = addr2.s_addr;
		break;
	case 'Z':
		if (ifname == NULL)
			ctx->ifdv = splitv(ctx, &ctx->ifdc, ctx->ifdv, arg);
		break;
	case '4':
		ifo->options &= ~DHCPCD_IPV6;
		ifo->options |= DHCPCD_IPV4;
		break;
	case '6':
		ifo->options &= ~DHCPCD_IPV4;
		ifo->options |= DHCPCD_IPV6;
		break;
	case O_IPV4:
		ifo->options |= DHCPCD_IPV4;
		break;
	case O_NOIPV4:
		ifo->options &= ~DHCPCD_IPV4;
		break;
	case O_IPV6:
		ifo->options |= DHCPCD_IPV6;
		break;
	case O_NOIPV6:
		ifo->options &= ~DHCPCD_IPV6;
		break;
#ifdef INET
	case O_ARPING:
		while (arg && *arg != '\0') {
			fp = strwhite(arg);
			if (fp)
				*fp++ = '\0';
			if (parse_addr(ctx, &addr, NULL, arg) != 0)
				return -1;
			naddr = realloc(ifo->arping,
			    sizeof(in_addr_t) * (ifo->arping_len + 1));
			if (naddr == NULL) {
				logger(ctx, LOG_ERR, "%s: %m", __func__);
				return -1;
			}
			ifo->arping = naddr;
			ifo->arping[ifo->arping_len++] = addr.s_addr;
			arg = strskipwhite(fp);
		}
		break;
	case O_DESTINATION:
		arg = set_option_space(ctx, arg, &d, &dl, &od, &odl, ifo,
		    &request, &require, &no, &reject);
		if (make_option_mask(d, dl, od, odl,
		    ifo->dstmask, arg, 2) != 0)
		{
			if (errno == EINVAL)
				logger(ctx, LOG_ERR, "option `%s' does not take"
				    " an IPv4 address", arg);
			else
				logger(ctx, LOG_ERR, "unknown option `%s'", arg);
			return -1;
		}
		break;
	case O_FALLBACK:
		free(ifo->fallback);
		ifo->fallback = strdup(arg);
		if (ifo->fallback == NULL) {
			logger(ctx, LOG_ERR, "%s: %m", __func__);
			return -1;
		}
		break;
#endif
	case O_IAID:
		if (ifname == NULL) {
			logger(ctx, LOG_ERR,
			    "IAID must belong in an interface block");
			return -1;
		}
		if (parse_iaid(ifo->iaid, arg, sizeof(ifo->iaid)) == -1) {
			logger(ctx, LOG_ERR, "invalid IAID %s", arg);
			return -1;
		}
		ifo->options |= DHCPCD_IAID;
		break;
	case O_IPV6RS:
		ifo->options |= DHCPCD_IPV6RS;
		break;
	case O_NOIPV6RS:
		ifo->options &= ~DHCPCD_IPV6RS;
		break;
	case O_IPV6RA_FORK:
		ifo->options &= ~DHCPCD_IPV6RA_REQRDNSS;
		break;
	case O_IPV6RA_OWN:
		ifo->options |= DHCPCD_IPV6RA_OWN;
		break;
	case O_IPV6RA_OWN_D:
		ifo->options |= DHCPCD_IPV6RA_OWN_DEFAULT;
		break;
	case O_IPV6RA_ACCEPT_NOPUBLIC:
		ifo->options |= DHCPCD_IPV6RA_ACCEPT_NOPUBLIC;
		break;
	case O_IPV6RA_AUTOCONF:
		ifo->options |= DHCPCD_IPV6RA_AUTOCONF;
		break;
	case O_IPV6RA_NOAUTOCONF:
		ifo->options &= ~DHCPCD_IPV6RA_AUTOCONF;
		break;
	case O_NOALIAS:
		ifo->options |= DHCPCD_NOALIAS;
		break;
#ifdef INET6
	case O_IA_NA:
		i = D6_OPTION_IA_NA;
		/* FALLTHROUGH */
	case O_IA_TA:
		if (i == 0)
			i = D6_OPTION_IA_TA;
		/* FALLTHROUGH */
	case O_IA_PD:
		if (i == 0) {
			if (ifname == NULL) {
				logger(ctx, LOG_ERR,
				    "IA PD must belong in an interface block");
				return -1;
			}
			i = D6_OPTION_IA_PD;
		}
		if (ifname == NULL && arg) {
			logger(ctx, LOG_ERR,
			    "IA with IAID must belong in an interface block");
			return -1;
		}
		ifo->options |= DHCPCD_IA_FORCED;
		fp = strwhite(arg);
		if (fp) {
			*fp++ = '\0';
			fp = strskipwhite(fp);
		}
		if (arg) {
			p = strchr(arg, '/');
			if (p)
				*p++ = '\0';
			if (parse_iaid(iaid, arg, sizeof(iaid)) == -1) {
				logger(ctx, LOG_ERR, "invalid IAID: %s", arg);
				return -1;
			}
		}
		ia = NULL;
		for (sl = 0; sl < ifo->ia_len; sl++) {
			if ((arg == NULL && !ifo->ia[sl].iaid_set) ||
			    (ifo->ia[sl].iaid_set &&
			    ifo->ia[sl].iaid[0] == iaid[0] &&
			    ifo->ia[sl].iaid[1] == iaid[1] &&
			    ifo->ia[sl].iaid[2] == iaid[2] &&
			    ifo->ia[sl].iaid[3] == iaid[3]))
			{
			        ia = &ifo->ia[sl];
				break;
			}
		}
		if (ia && ia->ia_type != (uint16_t)i) {
			logger(ctx, LOG_ERR, "Cannot mix IA for the same IAID");
			break;
		}
		if (ia == NULL) {
			ia = realloc(ifo->ia,
			    sizeof(*ifo->ia) * (ifo->ia_len + 1));
			if (ia == NULL) {
				logger(ctx, LOG_ERR, "%s: %m", __func__);
				return -1;
			}
			ifo->ia = ia;
			ia = &ifo->ia[ifo->ia_len++];
			ia->ia_type = (uint16_t)i;
			if (arg) {
				ia->iaid[0] = iaid[0];
				ia->iaid[1] = iaid[1];
				ia->iaid[2] = iaid[2];
				ia->iaid[3] = iaid[3];
				ia->iaid_set = 1;
			} else
				ia->iaid_set = 0;
			if (!ia->iaid_set ||
			    p == NULL ||
			    ia->ia_type == D6_OPTION_IA_TA)
			{
				memset(&ia->addr, 0, sizeof(ia->addr));
				ia->prefix_len = 0;
			} else {
				arg = p;
				p = strchr(arg, '/');
				if (p)
					*p++ = '\0';
				if (inet_pton(AF_INET6, arg, &ia->addr) == -1) {
					logger(ctx, LOG_ERR, "%s: %m", arg);
					memset(&ia->addr, 0, sizeof(ia->addr));
				}
				if (p && ia->ia_type == D6_OPTION_IA_PD) {
					i = (int)strtoi(p, NULL, 0, 8, 120, &e);
					if (e) {
						logger(ctx, LOG_ERR,
						    "%s: failed to convert"
						    " prefix len",
						    p);
						ia->prefix_len = 0;
					} else
						ia->prefix_len = (uint8_t)i;
				}
			}
			ia->sla_max = 0;
			ia->sla_len = 0;
			ia->sla = NULL;
		}
		if (ia->ia_type != D6_OPTION_IA_PD)
			break;
		for (p = fp; p; p = fp) {
			fp = strwhite(p);
			if (fp) {
				*fp++ = '\0';
				fp = strskipwhite(fp);
			}
			sla = realloc(ia->sla,
			    sizeof(*ia->sla) * (ia->sla_len + 1));
			if (sla == NULL) {
				logger(ctx, LOG_ERR, "%s: %m", __func__);
				return -1;
			}
			ia->sla = sla;
			sla = &ia->sla[ia->sla_len++];
			np = strchr(p, '/');
			if (np)
				*np++ = '\0';
			if (strlcpy(sla->ifname, p,
			    sizeof(sla->ifname)) >= sizeof(sla->ifname))
			{
				logger(ctx, LOG_ERR, "%s: interface name too long",
				    arg);
				goto err_sla;
			}
			p = np;
			if (p) {
				np = strchr(p, '/');
				if (np)
					*np++ = '\0';
				if (*p == '\0')
					sla->sla_set = 0;
				else {
					sla->sla = (uint32_t)strtou(p, NULL,
					    0, 0, UINT32_MAX, &e);
					sla->sla_set = 1;
					if (e) {
						logger(ctx, LOG_ERR,
						    "%s: failed to convert sla",
						    ifname);
						goto err_sla;
					}
				}
				if (np) {
					sla->prefix_len = (uint8_t)strtoi(np,
					    NULL, 0, 0, 128, &e);
					if (e) {
						logger(ctx, LOG_ERR, "%s: failed to "
						    "convert prefix len",
						    ifname);
						goto err_sla;
					}
				} else
					sla->prefix_len = 0;
			} else {
				sla->sla_set = 0;
				sla->prefix_len = 0;
			}
			/* Sanity check */
			for (sl = 0; sl < ia->sla_len - 1; sl++) {
				slap = &ia->sla[sl];
				if (slap->sla_set != sla->sla_set) {
					logger(ctx, LOG_WARNING,
					    "%s: cannot mix automatic "
					    "and fixed SLA",
					    sla->ifname);
					goto err_sla;
				}
				if (sla->sla_set == 0 &&
				    strcmp(slap->ifname, sla->ifname) == 0)
				{
					logger(ctx, LOG_WARNING,
					    "%s: cannot specify the "
					    "same interface twice with "
					    "an automatic SLA",
					    sla->ifname);
					goto err_sla;
				}
				if (slap->sla == 0 || sla->sla == 0) {
					logger(ctx, LOG_ERR, "%s: cannot"
					    " assign multiple prefixes"
					    " with a SLA of 0",
					    ifname);
					goto err_sla;
				}
			}
			if (sla->sla_set && sla->sla > ia->sla_max)
				ia->sla_max = sla->sla;
		}
		break;
err_sla:
		ia->sla_len--;
		return -1;
#endif
	case O_HOSTNAME_SHORT:
		ifo->options |= DHCPCD_HOSTNAME | DHCPCD_HOSTNAME_SHORT;
		break;
	case O_DEV:
#ifdef PLUGIN_DEV
		if (ctx->dev_load)
			free(ctx->dev_load);
		ctx->dev_load = strdup(arg);
#endif
		break;
	case O_NODEV:
		ifo->options &= ~DHCPCD_DEV;
		break;
	case O_DEFINE:
		dop = &ifo->dhcp_override;
		dop_len = &ifo->dhcp_override_len;
		/* FALLTHROUGH */
	case O_DEFINEND:
		if (dop == NULL) {
			dop = &ifo->nd_override;
			dop_len = &ifo->nd_override_len;
		}
		/* FALLTHROUGH */
	case O_DEFINE6:
		if (dop == NULL) {
			dop = &ifo->dhcp6_override;
			dop_len = &ifo->dhcp6_override_len;
		}
		/* FALLTHROUGH */
	case O_VENDOPT:
		if (dop == NULL) {
			dop = &ifo->vivso_override;
			dop_len = &ifo->vivso_override_len;
		}
		*edop = *ldop = NULL;
		/* FALLTHROUGH */
	case O_EMBED:
		if (dop == NULL) {
			if (*edop) {
				dop = &(*edop)->embopts;
				dop_len = &(*edop)->embopts_len;
			} else if (ldop) {
				dop = &(*ldop)->embopts;
				dop_len = &(*ldop)->embopts_len;
			} else {
				logger(ctx, LOG_ERR,
				    "embed must be after a define or encap");
				return -1;
			}
		}
		/* FALLTHROUGH */
	case O_ENCAP:
		if (dop == NULL) {
			if (*ldop == NULL) {
				logger(ctx, LOG_ERR, "encap must be after a define");
				return -1;
			}
			dop = &(*ldop)->encopts;
			dop_len = &(*ldop)->encopts_len;
		}

		/* Shared code for define, define6, embed and encap */

		/* code */
		if (opt == O_EMBED) /* Embedded options don't have codes */
			u = 0;
		else {
			fp = strwhite(arg);
			if (fp == NULL) {
				logger(ctx, LOG_ERR, "invalid syntax: %s", arg);
				return -1;
			}
			*fp++ = '\0';
			u = (uint32_t)strtou(arg, NULL, 0, 0, UINT32_MAX, &e);
			if (e) {
				logger(ctx, LOG_ERR, "invalid code: %s", arg);
				return -1;
			}
			arg = strskipwhite(fp);
			if (arg == NULL) {
				logger(ctx, LOG_ERR, "invalid syntax");
				return -1;
			}
		}
		/* type */
		fp = strwhite(arg);
		if (fp)
			*fp++ = '\0';
		np = strchr(arg, ':');
		/* length */
		if (np) {
			*np++ = '\0';
			bp = NULL; /* No bitflag */
			l = (long)strtou(np, NULL, 0, 0, LONG_MAX, &e);
			if (e) {
				logger(ctx,LOG_ERR, "failed to convert length");
				return -1;
			}
		} else {
			l = 0;
			bp = strchr(arg, '='); /* bitflag assignment */
			if (bp)
				*bp++ = '\0';
		}
		t = 0;
		if (strcasecmp(arg, "request") == 0) {
			t |= REQUEST;
			arg = strskipwhite(fp);
			fp = strwhite(arg);
			if (fp == NULL) {
				logger(ctx, LOG_ERR, "incomplete request type");
				return -1;
			}
			*fp++ = '\0';
		} else if (strcasecmp(arg, "norequest") == 0) {
			t |= NOREQ;
			arg = strskipwhite(fp);
			fp = strwhite(arg);
			if (fp == NULL) {
				logger(ctx, LOG_ERR, "incomplete request type");
				return -1;
			}
			*fp++ = '\0';
		}
		if (strcasecmp(arg, "index") == 0) {
			t |= INDEX;
			arg = strskipwhite(fp);
			fp = strwhite(arg);
			if (fp == NULL) {
				logger(ctx, LOG_ERR, "incomplete index type");
				return -1;
			}
			*fp++ = '\0';
		}
		if (strcasecmp(arg, "array") == 0) {
			t |= ARRAY;
			arg = strskipwhite(fp);
			fp = strwhite(arg);
			if (fp == NULL) {
				logger(ctx, LOG_ERR, "incomplete array type");
				return -1;
			}
			*fp++ = '\0';
		}
		if (strcasecmp(arg, "ipaddress") == 0)
			t |= ADDRIPV4;
		else if (strcasecmp(arg, "ip6address") == 0)
			t |= ADDRIPV6;
		else if (strcasecmp(arg, "string") == 0)
			t |= STRING;
		else if (strcasecmp(arg, "byte") == 0)
			t |= UINT8;
		else if (strcasecmp(arg, "bitflags") == 0)
			t |= BITFLAG;
		else if (strcasecmp(arg, "uint16") == 0)
			t |= UINT16;
		else if (strcasecmp(arg, "int16") == 0)
			t |= SINT16;
		else if (strcasecmp(arg, "uint32") == 0)
			t |= UINT32;
		else if (strcasecmp(arg, "int32") == 0)
			t |= SINT32;
		else if (strcasecmp(arg, "flag") == 0)
			t |= FLAG;
		else if (strcasecmp(arg, "raw") == 0)
			t |= STRING | RAW;
		else if (strcasecmp(arg, "ascii") == 0)
			t |= STRING | ASCII;
		else if (strcasecmp(arg, "domain") == 0)
			t |= STRING | DOMAIN | RFC1035;
		else if (strcasecmp(arg, "dname") == 0)
			t |= STRING | DOMAIN;
		else if (strcasecmp(arg, "binhex") == 0)
			t |= STRING | BINHEX;
		else if (strcasecmp(arg, "embed") == 0)
			t |= EMBED;
		else if (strcasecmp(arg, "encap") == 0)
			t |= ENCAP;
		else if (strcasecmp(arg, "rfc3361") ==0)
			t |= STRING | RFC3361;
		else if (strcasecmp(arg, "rfc3442") ==0)
			t |= STRING | RFC3442;
		else if (strcasecmp(arg, "rfc5969") == 0)
			t |= STRING | RFC5969;
		else if (strcasecmp(arg, "option") == 0)
			t |= OPTION;
		else {
			logger(ctx, LOG_ERR, "unknown type: %s", arg);
			return -1;
		}
		if (l && !(t & (STRING | BINHEX))) {
			logger(ctx, LOG_WARNING,
			    "ignoring length for type `%s'", arg);
			l = 0;
		}
		if (t & ARRAY && t & (STRING | BINHEX) &&
		    !(t & (RFC1035 | DOMAIN)))
		{
			logger(ctx, LOG_WARNING, "ignoring array for strings");
			t &= ~ARRAY;
		}
		if (t & BITFLAG) {
			if (bp == NULL)
				logger(ctx, LOG_WARNING,
				    "missing bitflag assignment");
		}
		/* variable */
		if (!fp) {
			if (!(t & OPTION)) {
			        logger(ctx, LOG_ERR,
				    "type %s requires a variable name", arg);
				return -1;
			}
			np = NULL;
		} else {
			arg = strskipwhite(fp);
			fp = strwhite(arg);
			if (fp)
				*fp++ = '\0';
			if (strcasecmp(arg, "reserved")) {
				np = strdup(arg);
				if (np == NULL) {
					logger(ctx, LOG_ERR,
					    "%s: %m", __func__);
					return -1;
				}
			} else {
				np = NULL;
				t |= RESERVED;
			}
		}
		if (opt != O_EMBED) {
			for (dl = 0, ndop = *dop; dl < *dop_len; dl++, ndop++)
			{
				/* type 0 seems freshly malloced struct
				 * for us to use */
				if (ndop->option == u || ndop->type == 0)
					break;
			}
			if (dl == *dop_len)
				ndop = NULL;
		} else
			ndop = NULL;
		if (ndop == NULL) {
			if ((ndop = realloc(*dop,
			    sizeof(**dop) * ((*dop_len) + 1))) == NULL)
			{
				logger(ctx, LOG_ERR, "%s: %m", __func__);
				free(np);
				return -1;
			}
			*dop = ndop;
			ndop = &(*dop)[(*dop_len)++];
			ndop->embopts = NULL;
			ndop->embopts_len = 0;
			ndop->encopts = NULL;
			ndop->encopts_len = 0;
		} else
			free_dhcp_opt_embenc(ndop);
		ndop->option = (uint32_t)u; /* could have been 0 */
		ndop->type = t;
		ndop->len = (size_t)l;
		ndop->var = np;
		if (bp) {
			dl = strlen(bp);
			memcpy(ndop->bitflags, bp, dl);
			memset(ndop->bitflags + dl, 0,
			    sizeof(ndop->bitflags) - dl);
		} else
			memset(ndop->bitflags, 0, sizeof(ndop->bitflags));
		/* Save the define for embed and encap options */
		switch (opt) {
		case O_DEFINE:
		case O_DEFINEND:
		case O_DEFINE6:
		case O_VENDOPT:
			*ldop = ndop;
			break;
		case O_ENCAP:
			*edop = ndop;
			break;
		}
		break;
	case O_VENDCLASS:
		fp = strwhite(arg);
		if (fp)
			*fp++ = '\0';
		u = (uint32_t)strtou(arg, NULL, 0, 0, UINT32_MAX, &e);
		if (e) {
			logger(ctx, LOG_ERR, "invalid code: %s", arg);
			return -1;
		}
		if (fp) {
			s = parse_string(NULL, 0, fp);
			if (s == -1) {
				logger(ctx, LOG_ERR, "%s: %m", __func__);
				return -1;
			}
			dl = (size_t)s;
			if (dl + (sizeof(uint16_t) * 2) > UINT16_MAX) {
				logger(ctx, LOG_ERR, "vendor class is too big");
				return -1;
			}
			np = malloc(dl);
			if (np == NULL) {
				logger(ctx, LOG_ERR, "%s: %m", __func__);
				return -1;
			}
			parse_string(np, dl, fp);
		} else {
			dl = 0;
			np = NULL;
		}
		vivco = realloc(ifo->vivco, sizeof(*ifo->vivco) *
		    (ifo->vivco_len + 1));
		if (vivco == NULL) {
			logger(ctx, LOG_ERR, "%s: %m", __func__);
			return -1;
		}
		ifo->vivco = vivco;
		ifo->vivco_en = (uint32_t)u;
		vivco = &ifo->vivco[ifo->vivco_len++];
		vivco->len = dl;
		vivco->data = (uint8_t *)np;
		break;
	case O_AUTHPROTOCOL:
		fp = strwhite(arg);
		if (fp)
			*fp++ = '\0';
		if (strcasecmp(arg, "token") == 0)
			ifo->auth.protocol = AUTH_PROTO_TOKEN;
		else if (strcasecmp(arg, "delayed") == 0)
			ifo->auth.protocol = AUTH_PROTO_DELAYED;
		else if (strcasecmp(arg, "delayedrealm") == 0)
			ifo->auth.protocol = AUTH_PROTO_DELAYEDREALM;
		else {
			logger(ctx, LOG_ERR, "%s: unsupported protocol", arg);
			return -1;
		}
		arg = strskipwhite(fp);
		fp = strwhite(arg);
		if (arg == NULL) {
			ifo->auth.options |= DHCPCD_AUTH_SEND;
			ifo->auth.algorithm = AUTH_ALG_HMAC_MD5;
			ifo->auth.rdm = AUTH_RDM_MONOTONIC;
			break;
		}
		if (fp)
			*fp++ = '\0';
		if (strcasecmp(arg, "hmacmd5") == 0 ||
		    strcasecmp(arg, "hmac-md5") == 0)
			ifo->auth.algorithm = AUTH_ALG_HMAC_MD5;
		else {
			logger(ctx, LOG_ERR, "%s: unsupported algorithm", arg);
			return 1;
		}
		arg = fp;
		if (arg == NULL) {
			ifo->auth.options |= DHCPCD_AUTH_SEND;
			ifo->auth.rdm = AUTH_RDM_MONOTONIC;
			break;
		}
		if (strcasecmp(arg, "monocounter") == 0) {
			ifo->auth.rdm = AUTH_RDM_MONOTONIC;
			ifo->auth.options |= DHCPCD_AUTH_RDM_COUNTER;
		} else if (strcasecmp(arg, "monotonic") ==0 ||
		    strcasecmp(arg, "monotime") == 0)
			ifo->auth.rdm = AUTH_RDM_MONOTONIC;
		else {
			logger(ctx, LOG_ERR, "%s: unsupported RDM", arg);
			return -1;
		}
		ifo->auth.options |= DHCPCD_AUTH_SEND;
		break;
	case O_AUTHTOKEN:
		fp = strwhite(arg);
		if (fp == NULL) {
			logger(ctx, LOG_ERR, "authtoken requires a realm");
			return -1;
		}
		*fp++ = '\0';
		token = malloc(sizeof(*token));
		if (token == NULL) {
			logger(ctx, LOG_ERR, "%s: %m", __func__);
			free(token);
			return -1;
		}
		if (parse_uint32(&token->secretid, arg) == -1) {
			logger(ctx, LOG_ERR, "%s: not a number", arg);
			free(token);
			return -1;
		}
		arg = fp;
		fp = strend(arg);
		if (fp == NULL) {
			logger(ctx, LOG_ERR, "authtoken requies an a key");
			free(token);
			return -1;
		}
		*fp++ = '\0';
		s = parse_string(NULL, 0, arg);
		if (s == -1) {
			logger(ctx, LOG_ERR, "realm_len: %m");
			free(token);
			return -1;
		}
		if (s) {
			token->realm_len = (size_t)s;
			token->realm = malloc(token->realm_len);
			if (token->realm == NULL) {
				free(token);
				logger(ctx, LOG_ERR, "%s: %m", __func__);
				return -1;
			}
			parse_string((char *)token->realm, token->realm_len,
			    arg);
		} else {
			token->realm_len = 0;
			token->realm = NULL;
		}
		arg = fp;
		fp = strend(arg);
		if (fp == NULL) {
			logger(ctx, LOG_ERR, "authtoken requies an an expiry date");
			free(token->realm);
			free(token);
			return -1;
		}
		*fp++ = '\0';
		if (*arg == '"') {
			arg++;
			np = strchr(arg, '"');
			if (np)
				*np = '\0';
		}
		if (strcmp(arg, "0") == 0 || strcasecmp(arg, "forever") == 0)
			token->expire =0;
		else {
			struct tm tm;

			memset(&tm, 0, sizeof(tm));
			if (strptime(arg, "%Y-%m-%d %H:%M", &tm) == NULL) {
				logger(ctx, LOG_ERR, "%s: invalid date time", arg);
				free(token->realm);
				free(token);
				return -1;
			}
			if ((token->expire = mktime(&tm)) == (time_t)-1) {
				logger(ctx, LOG_ERR, "%s: mktime: %m", __func__);
				free(token->realm);
				free(token);
				return -1;
			}
		}
		arg = fp;
		s = parse_string(NULL, 0, arg);
		if (s == -1 || s == 0) {
			logger(ctx, LOG_ERR, s == -1 ? "token_len: %m" : 
			    "authtoken needs a key");
			free(token->realm);
			free(token);
			return -1;
		}
		token->key_len = (size_t)s;
		token->key = malloc(token->key_len);
		parse_string((char *)token->key, token->key_len, arg);
		TAILQ_INSERT_TAIL(&ifo->auth.tokens, token, next);
		break;
	case O_AUTHNOTREQUIRED:
		ifo->auth.options &= ~DHCPCD_AUTH_REQUIRE;
		break;
	case O_DHCP:
		ifo->options |= DHCPCD_DHCP | DHCPCD_IPV4;
		break;
	case O_NODHCP:
		ifo->options &= ~DHCPCD_DHCP;
		break;
	case O_DHCP6:
		ifo->options |= DHCPCD_DHCP6 | DHCPCD_IPV6;
		break;
	case O_NODHCP6:
		ifo->options &= ~DHCPCD_DHCP6;
		break;
	case O_CONTROLGRP:
#ifdef _REENTRANT
		l = sysconf(_SC_GETGR_R_SIZE_MAX);
		if (l == -1)
			dl = 1024;
		else
			dl = (size_t)l;
		p = malloc(dl);
		if (p == NULL) {
			logger(ctx, LOG_ERR, "%s: malloc: %m", __func__);
			return -1;
		}
		while ((i = getgrnam_r(arg, &grpbuf, p, (size_t)l, &grp)) ==
		    ERANGE)
		{
			size_t nl = dl * 2;
			if (nl < dl) {
				logger(ctx, LOG_ERR, "control_group: out of buffer");
				free(p);
				return -1;
			}
			dl = nl;
			np = realloc(p, dl);
			if (np == NULL) {
				logger(ctx, LOG_ERR, "control_group: realloc: %m");
				free(p);
				return -1;
			}
			p = np;
		}
		if (i != 0) {
			errno = i;
			logger(ctx, LOG_ERR, "getgrnam_r: %m");
			free(p);
			return -1;
		}
		if (grp == NULL) {
			logger(ctx, LOG_ERR, "controlgroup: %s: not found", arg);
			free(p);
			return -1;
		}
		ctx->control_group = grp->gr_gid;
		free(p);
#else
		grp = getgrnam(arg);
		if (grp == NULL) {
			logger(ctx, LOG_ERR, "controlgroup: %s: not found", arg);
			return -1;
		}
		ctx->control_group = grp->gr_gid;
#endif
		break;
	case O_GATEWAY:
		ifo->options |= DHCPCD_GATEWAY;
		break;
	case O_NOUP:
		ifo->options &= ~DHCPCD_IF_UP;
		break;
	case O_SLAAC:
		if (strcmp(arg, "private") == 0 ||
		    strcmp(arg, "stableprivate") == 0 ||
		    strcmp(arg, "stable") == 0)
			ifo->options |= DHCPCD_SLAACPRIVATE;
		else
			ifo->options &= ~DHCPCD_SLAACPRIVATE;
		break;
	case O_BOOTP:
		ifo->options |= DHCPCD_BOOTP;
		break;
	case O_NODELAY:
		ifo->options &= ~DHCPCD_INITIAL_DELAY;
		break;
	default:
		return 0;
	}

	return 1;
}

static int
parse_config_line(struct dhcpcd_ctx *ctx, const char *ifname,
    struct if_options *ifo, const char *opt, char *line,
    struct dhcp_opt **ldop, struct dhcp_opt **edop)
{
	unsigned int i;

	for (i = 0; i < sizeof(cf_options) / sizeof(cf_options[0]); i++) {
		if (!cf_options[i].name ||
		    strcmp(cf_options[i].name, opt) != 0)
			continue;

		if (cf_options[i].has_arg == required_argument && !line) {
			fprintf(stderr,
			    PACKAGE ": option requires an argument -- %s\n",
			    opt);
			return -1;
		}

		return parse_option(ctx, ifname, ifo, cf_options[i].val, line,
		    ldop, edop);
	}

	logger(ctx, LOG_ERR, "unknown option: %s", opt);
	return -1;
}

static void
finish_config(struct if_options *ifo)
{

	/* Terminate the encapsulated options */
	if (ifo->vendor[0] && !(ifo->options & DHCPCD_VENDORRAW)) {
		ifo->vendor[0]++;
		ifo->vendor[ifo->vendor[0]] = DHO_END;
		/* We are called twice.
		 * This should be fixed, but in the meantime, this
		 * guard should suffice */
		ifo->options |= DHCPCD_VENDORRAW;
	}
}

/* Handy routine to read very long lines in text files.
 * This means we read the whole line and avoid any nasty buffer overflows.
 * We strip leading space and avoid comment lines, making the code that calls
 * us smaller. */
static char *
get_line(char ** __restrict buf, size_t * __restrict buflen,
    FILE * __restrict fp)
{
	char *p;
	ssize_t bytes;

	do {
		bytes = getline(buf, buflen, fp);
		if (bytes == -1)
			return NULL;
		for (p = *buf; *p == ' ' || *p == '\t'; p++)
			;
	} while (*p == '\0' || *p == '\n' || *p == '#' || *p == ';');
	if ((*buf)[--bytes] == '\n')
		(*buf)[bytes] = '\0';
	return p;
}

struct if_options *
read_config(struct dhcpcd_ctx *ctx,
    const char *ifname, const char *ssid, const char *profile)
{
	struct if_options *ifo;
	FILE *fp;
	struct stat sb;
	char *line, *buf, *option, *p;
	size_t buflen;
	ssize_t vlen;
	int skip, have_profile, new_block, had_block;
#ifndef EMBEDDED_CONFIG
	const char * const *e;
	size_t ol;
#endif
#if !defined(INET) || !defined(INET6)
	size_t i;
	struct dhcp_opt *opt;
#endif
	struct dhcp_opt *ldop, *edop;

	/* Seed our default options */
	ifo = calloc(1, sizeof(*ifo));
	if (ifo == NULL) {
		logger(ctx, LOG_ERR, "%s: %m", __func__);
		return NULL;
	}
	ifo->options |= DHCPCD_DAEMONISE | DHCPCD_LINK | DHCPCD_INITIAL_DELAY;
	ifo->options |= DHCPCD_IF_UP;
#ifdef PLUGIN_DEV
	ifo->options |= DHCPCD_DEV;
#endif
#ifdef INET
	ifo->options |= DHCPCD_IPV4 | DHCPCD_DHCP | DHCPCD_IPV4LL;
	ifo->options |= DHCPCD_GATEWAY | DHCPCD_ARP;
#endif
#ifdef INET6
	ifo->options |= DHCPCD_IPV6 | DHCPCD_IPV6RS;
	ifo->options |= DHCPCD_IPV6RA_AUTOCONF | DHCPCD_IPV6RA_REQRDNSS;
	ifo->options |= DHCPCD_DHCP6;
#endif
	ifo->timeout = DEFAULT_TIMEOUT;
	ifo->reboot = DEFAULT_REBOOT;
	ifo->metric = -1;
	ifo->auth.options |= DHCPCD_AUTH_REQUIRE;
	TAILQ_INIT(&ifo->auth.tokens);

	vlen = dhcp_vendor((char *)ifo->vendorclassid + 1,
	            sizeof(ifo->vendorclassid) - 1);
	ifo->vendorclassid[0] = (uint8_t)(vlen == -1 ? 0 : vlen);

	buf = NULL;
	buflen = 0;

	/* Parse our embedded options file */
	if (ifname == NULL) {
		/* Space for initial estimates */
#if defined(INET) && defined(INITDEFINES)
		ifo->dhcp_override =
		    calloc(INITDEFINES, sizeof(*ifo->dhcp_override));
		if (ifo->dhcp_override == NULL)
			logger(ctx, LOG_ERR, "%s: %m", __func__);
		else
			ifo->dhcp_override_len = INITDEFINES;
#endif

#if defined(INET6) && defined(INITDEFINENDS)
		ifo->nd_override =
		    calloc(INITDEFINENDS, sizeof(*ifo->nd_override));
		if (ifo->nd_override == NULL)
			logger(ctx, LOG_ERR, "%s: %m", __func__);
		else
			ifo->nd_override_len = INITDEFINENDS;
#endif
#if defined(INET6) && defined(INITDEFINE6S)
		ifo->dhcp6_override =
		    calloc(INITDEFINE6S, sizeof(*ifo->dhcp6_override));
		if (ifo->dhcp6_override == NULL)
			logger(ctx, LOG_ERR, "%s: %m", __func__);
		else
			ifo->dhcp6_override_len = INITDEFINE6S;
#endif

		/* Now load our embedded config */
#ifdef EMBEDDED_CONFIG
		fp = fopen(EMBEDDED_CONFIG, "r");
		if (fp == NULL)
			logger(ctx, LOG_ERR, "fopen `%s': %m", EMBEDDED_CONFIG);

		while (fp && (line = get_line(&buf, &buflen, fp))) {
#else
		buflen = 80;
		buf = malloc(buflen);
		if (buf == NULL) {
			logger(ctx, LOG_ERR, "%s: %m", __func__);
			return NULL;
		}
		ldop = edop = NULL;
		for (e = dhcpcd_embedded_conf; *e; e++) {
			ol = strlen(*e) + 1;
			if (ol > buflen) {
				buflen = ol;
				buf = realloc(buf, buflen);
				if (buf == NULL) {
					logger(ctx, LOG_ERR, "%s: %m", __func__);
					free(buf);
					return NULL;
				}
			}
			memcpy(buf, *e, ol);
			line = buf;
#endif
			option = strsep(&line, " \t");
			if (line)
				line = strskipwhite(line);
			/* Trim trailing whitespace */
			if (line && *line) {
				p = line + strlen(line) - 1;
				while (p != line &&
				    (*p == ' ' || *p == '\t') &&
				    *(p - 1) != '\\')
					*p-- = '\0';
			}
			parse_config_line(ctx, NULL, ifo, option, line,
			    &ldop, &edop);

		}

#ifdef EMBEDDED_CONFIG
		if (fp)
			fclose(fp);
#endif
#ifdef INET
		ctx->dhcp_opts = ifo->dhcp_override;
		ctx->dhcp_opts_len = ifo->dhcp_override_len;
#else
		for (i = 0, opt = ifo->dhcp_override;
		    i < ifo->dhcp_override_len;
		    i++, opt++)
			free_dhcp_opt_embenc(opt);
		free(ifo->dhcp_override);
#endif
		ifo->dhcp_override = NULL;
		ifo->dhcp_override_len = 0;

#ifdef INET6
		ctx->nd_opts = ifo->nd_override;
		ctx->nd_opts_len = ifo->nd_override_len;
		ctx->dhcp6_opts = ifo->dhcp6_override;
		ctx->dhcp6_opts_len = ifo->dhcp6_override_len;
#else
		for (i = 0, opt = ifo->nd_override;
		    i < ifo->nd_override_len;
		    i++, opt++)
			free_dhcp_opt_embenc(opt);
		free(ifo->nd_override);
		for (i = 0, opt = ifo->dhcp6_override;
		    i < ifo->dhcp6_override_len;
		    i++, opt++)
			free_dhcp_opt_embenc(opt);
		free(ifo->dhcp6_override);
#endif
		ifo->nd_override = NULL;
		ifo->nd_override_len = 0;
		ifo->dhcp6_override = NULL;
		ifo->dhcp6_override_len = 0;

		ctx->vivso = ifo->vivso_override;
		ctx->vivso_len = ifo->vivso_override_len;
		ifo->vivso_override = NULL;
		ifo->vivso_override_len = 0;
	}

	/* Parse our options file */
	fp = fopen(ctx->cffile, "r");
	if (fp == NULL) {
		if (strcmp(ctx->cffile, CONFIG))
			logger(ctx, LOG_ERR, "fopen `%s': %m", ctx->cffile);
		free(buf);
		return ifo;
	}
	if (stat(ctx->cffile, &sb) == 0)
		ifo->mtime = sb.st_mtime;

	ldop = edop = NULL;
	skip = have_profile = new_block = 0;
	had_block = ifname == NULL ? 1 : 0;
	while ((line = get_line(&buf, &buflen, fp))) {
		option = strsep(&line, " \t");
		if (line)
			line = strskipwhite(line);
		/* Trim trailing whitespace */
		if (line && *line) {
			p = line + strlen(line) - 1;
			while (p != line &&
			    (*p == ' ' || *p == '\t') &&
			    *(p - 1) != '\\')
				*p-- = '\0';
		}
		if (skip == 0 && new_block) {
			had_block = 1;
			new_block = 0;
			ifo->options &= ~DHCPCD_WAITOPTS;
		}
		/* Start of an interface block, skip if not ours */
		if (strcmp(option, "interface") == 0) {
			char **n;

			new_block = 1;
			if (ifname && line && strcmp(line, ifname) == 0)
				skip = 0;
			else
				skip = 1;
			if (ifname)
				continue;

			n = realloc(ctx->ifcv,
			    sizeof(char *) * ((size_t)ctx->ifcc + 1));
			if (n == NULL) {
				logger(ctx, LOG_ERR, "%s: %m", __func__);
				continue;
			}
			ctx->ifcv = n;
			ctx->ifcv[ctx->ifcc] = strdup(line);
			if (ctx->ifcv[ctx->ifcc] == NULL) {
				logger(ctx, LOG_ERR, "%s: %m", __func__);
				continue;
			}
			ctx->ifcc++;
			logger(ctx, LOG_DEBUG, "allowing interface %s",
			    ctx->ifcv[ctx->ifcc - 1]);
			continue;
		}
		/* Start of an ssid block, skip if not ours */
		if (strcmp(option, "ssid") == 0) {
			new_block = 1;
			if (ssid && line && strcmp(line, ssid) == 0)
				skip = 0;
			else
				skip = 1;
			continue;
		}
		/* Start of a profile block, skip if not ours */
		if (strcmp(option, "profile") == 0) {
			new_block = 1;
			if (profile && line && strcmp(line, profile) == 0) {
				skip = 0;
				have_profile = 1;
			} else
				skip = 1;
			continue;
		}
		/* Skip arping if we have selected a profile but not parsing
		 * one. */
		if (profile && !have_profile && strcmp(option, "arping") == 0)
			continue;
		if (skip)
			continue;
		parse_config_line(ctx, ifname, ifo, option, line, &ldop, &edop);
	}
	fclose(fp);
	free(buf);

	if (profile && !have_profile) {
		free_options(ifo);
		errno = ENOENT;
		return NULL;
	}

	if (!had_block)
		ifo->options &= ~DHCPCD_WAITOPTS;
	finish_config(ifo);
	return ifo;
}

int
add_options(struct dhcpcd_ctx *ctx, const char *ifname,
    struct if_options *ifo, int argc, char **argv)
{
	int oi, opt, r;
	unsigned long long wait_opts;

	if (argc == 0)
		return 1;

	optind = 0;
	r = 1;
	/* Don't apply the command line wait options to each interface,
	 * only use the dhcpcd.conf entry for that. */
	if (ifname != NULL)
		wait_opts = ifo->options & DHCPCD_WAITOPTS;
	while ((opt = getopt_long(argc, argv, IF_OPTS, cf_options, &oi)) != -1)
	{
		r = parse_option(ctx, ifname, ifo, opt, optarg, NULL, NULL);
		if (r != 1)
			break;
	}
	if (ifname != NULL) {
		ifo->options &= ~DHCPCD_WAITOPTS;
		ifo->options |= wait_opts;
	}

	finish_config(ifo);
	return r;
}

void
free_options(struct if_options *ifo)
{
	size_t i;
	struct dhcp_opt *opt;
	struct vivco *vo;
	struct token *token;

	if (ifo) {
		if (ifo->environ) {
			i = 0;
			while (ifo->environ[i])
				free(ifo->environ[i++]);
			free(ifo->environ);
		}
		if (ifo->config) {
			i = 0;
			while (ifo->config[i])
				free(ifo->config[i++]);
			free(ifo->config);
		}
		ipv4_freeroutes(ifo->routes);
		free(ifo->script);
		free(ifo->arping);
		free(ifo->blacklist);
		free(ifo->fallback);

		for (opt = ifo->dhcp_override;
		    ifo->dhcp_override_len > 0;
		    opt++, ifo->dhcp_override_len--)
			free_dhcp_opt_embenc(opt);
		free(ifo->dhcp_override);
		for (opt = ifo->nd_override;
		    ifo->nd_override_len > 0;
		    opt++, ifo->nd_override_len--)
			free_dhcp_opt_embenc(opt);
		free(ifo->nd_override);
		for (opt = ifo->dhcp6_override;
		    ifo->dhcp6_override_len > 0;
		    opt++, ifo->dhcp6_override_len--)
			free_dhcp_opt_embenc(opt);
		free(ifo->dhcp6_override);
		for (vo = ifo->vivco;
		    ifo->vivco_len > 0;
		    vo++, ifo->vivco_len--)
			free(vo->data);
		free(ifo->vivco);
		for (opt = ifo->vivso_override;
		    ifo->vivso_override_len > 0;
		    opt++, ifo->vivso_override_len--)
			free_dhcp_opt_embenc(opt);
		free(ifo->vivso_override);

#ifdef INET6
		for (; ifo->ia_len > 0; ifo->ia_len--)
			free(ifo->ia[ifo->ia_len - 1].sla);
#endif
		free(ifo->ia);

		while ((token = TAILQ_FIRST(&ifo->auth.tokens))) {
			TAILQ_REMOVE(&ifo->auth.tokens, token, next);
			if (token->realm_len)
				free(token->realm);
			free(token->key);
			free(token);
		}
		free(ifo);
	}
}
