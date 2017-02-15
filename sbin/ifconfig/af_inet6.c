/*	$NetBSD: af_inet6.c,v 1.33 2015/05/12 14:05:29 roy Exp $	*/

/*
 * Copyright (c) 1983, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: af_inet6.c,v 1.33 2015/05/12 14:05:29 roy Exp $");
#endif /* not lint */

#include <sys/param.h> 
#include <sys/ioctl.h> 
#include <sys/socket.h>

#include <net/if.h> 
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet6/nd6.h>

#include <err.h>
#include <errno.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <util.h>

#include "env.h"
#include "extern.h"
#include "parse.h"
#include "extern.h"
#include "af_inetany.h"
#include "prog_ops.h"

static void in6_constructor(void) __attribute__((constructor));
static void in6_alias(const char *, prop_dictionary_t, prop_dictionary_t,
    struct in6_ifreq *);
static void in6_commit_address(prop_dictionary_t, prop_dictionary_t);

static int setia6eui64_impl(prop_dictionary_t, struct in6_aliasreq *);
static int setia6flags_impl(prop_dictionary_t, struct in6_aliasreq *);
static int setia6pltime_impl(prop_dictionary_t, struct in6_aliasreq *);
static int setia6vltime_impl(prop_dictionary_t, struct in6_aliasreq *);

static int setia6lifetime(prop_dictionary_t, int64_t, time_t *, uint32_t *);

static void in6_status(prop_dictionary_t, prop_dictionary_t, bool);
static bool in6_addr_tentative(struct ifaddrs *ifa);

static struct usage_func usage;
static cmdloop_branch_t branch[2];

static const struct kwinst ia6flagskw[] = {
	  IFKW("anycast",	IN6_IFF_ANYCAST)
	, IFKW("deprecated",	IN6_IFF_DEPRECATED)
};

static struct pinteger parse_pltime = PINTEGER_INITIALIZER(&parse_pltime,
    "pltime", 0, NULL, "pltime", &command_root.pb_parser);

static struct pinteger parse_vltime = PINTEGER_INITIALIZER(&parse_vltime,
    "vltime", 0, NULL, "vltime", &command_root.pb_parser);

static const struct kwinst inet6kw[] = {
	  {.k_word = "pltime", .k_nextparser = &parse_pltime.pi_parser}
	, {.k_word = "vltime", .k_nextparser = &parse_vltime.pi_parser}
	, {.k_word = "eui64", .k_key = "eui64", .k_type = KW_T_BOOL,
	   .k_bool = true, .k_nextparser = &command_root.pb_parser}
};

struct pkw ia6flags = PKW_INITIALIZER(&ia6flags, "ia6flags", NULL,
    "ia6flag", ia6flagskw, __arraycount(ia6flagskw), &command_root.pb_parser);
struct pkw inet6 = PKW_INITIALIZER(&inet6, "IPv6 keywords", NULL,
    NULL, inet6kw, __arraycount(inet6kw), NULL);

static struct afswtch in6af = {
	.af_name = "inet6", .af_af = AF_INET6, .af_status = in6_status,
	.af_addr_commit = in6_commit_address,
	.af_addr_tentative = in6_addr_tentative
};

static int
prefix(void *val, int size)
{
	u_char *pname = (u_char *)val;
	int byte, bit, plen = 0;

	for (byte = 0; byte < size; byte++, plen += 8)
		if (pname[byte] != 0xff)
			break;
	if (byte == size)
		return (plen);
	for (bit = 7; bit != 0; bit--, plen++)
		if (!(pname[byte] & (1 << bit)))
			break;
	for (; bit != 0; bit--)
		if (pname[byte] & (1 << bit))
			return(0);
	byte++;
	for (; byte < size; byte++)
		if (pname[byte])
			return(0);
	return (plen);
}

int
setia6flags_impl(prop_dictionary_t env, struct in6_aliasreq *ifra)
{
	int64_t ia6flag;

	if (!prop_dictionary_get_int64(env, "ia6flag", &ia6flag)) {
		errno = ENOENT;
		return -1;
	}

	if (ia6flag < 0) {
		ia6flag = -ia6flag;
		ifra->ifra_flags &= ~ia6flag;
	} else
		ifra->ifra_flags |= ia6flag;
	return 0;
}

int
setia6pltime_impl(prop_dictionary_t env, struct in6_aliasreq *ifra)
{
	int64_t pltime;

	if (!prop_dictionary_get_int64(env, "pltime", &pltime)) {
		errno = ENOENT;
		return -1;
	}

	return setia6lifetime(env, pltime,
	    &ifra->ifra_lifetime.ia6t_preferred,
	    &ifra->ifra_lifetime.ia6t_pltime);
}

int
setia6vltime_impl(prop_dictionary_t env, struct in6_aliasreq *ifra)
{
	int64_t vltime;

	if (!prop_dictionary_get_int64(env, "vltime", &vltime)) {
		errno = ENOENT;
		return -1;
	}

	return setia6lifetime(env, vltime,
		&ifra->ifra_lifetime.ia6t_expire,
		&ifra->ifra_lifetime.ia6t_vltime);
}

static int
setia6lifetime(prop_dictionary_t env, int64_t val, time_t *timep,
    uint32_t *ivalp)
{
	time_t t;
	int af;

	if ((af = getaf(env)) == -1 || af != AF_INET6) {
		errx(EXIT_FAILURE,
		    "inet6 address lifetime not allowed for the AF");
	}

	t = time(NULL);
	*timep = t + val;
	*ivalp = val;
	return 0;
}

int
setia6eui64_impl(prop_dictionary_t env, struct in6_aliasreq *ifra)
{
	char buf[2][80];
	struct ifaddrs *ifap, *ifa;
	const struct sockaddr_in6 *sin6 = NULL;
	const struct in6_addr *lladdr = NULL;
	struct in6_addr *in6;
	const char *ifname;
	bool doit = false;
	int af;

	if (!prop_dictionary_get_bool(env, "eui64", &doit) || !doit) {
		errno = ENOENT;
		return -1;
	}

	if ((ifname = getifname(env)) == NULL)
		return -1;

	af = getaf(env);
	if (af != AF_INET6) {
		errx(EXIT_FAILURE,
		    "eui64 address modifier not allowed for the AF");
	}
 	in6 = &ifra->ifra_addr.sin6_addr;
	if (memcmp(&in6addr_any.s6_addr[8], &in6->s6_addr[8], 8) != 0) {
		union {
			struct sockaddr_in6 sin6;
			struct sockaddr sa;
		} any = {.sin6 = {.sin6_family = AF_INET6}};
		memcpy(&any.sin6.sin6_addr, &in6addr_any,
		    sizeof(any.sin6.sin6_addr));
		(void)sockaddr_snprintf(buf[0], sizeof(buf[0]), "%a%%S",
		    &any.sa);
		(void)sockaddr_snprintf(buf[1], sizeof(buf[1]), "%a%%S",
		    (const struct sockaddr *)&ifra->ifra_addr);
		errx(EXIT_FAILURE, "interface index is already filled, %s | %s",
		    buf[0], buf[1]);
	}
	if (getifaddrs(&ifap) != 0)
		err(EXIT_FAILURE, "getifaddrs");
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family == AF_INET6 &&
		    strcmp(ifa->ifa_name, ifname) == 0) {
			sin6 = (const struct sockaddr_in6 *)ifa->ifa_addr;
			if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
				lladdr = &sin6->sin6_addr;
				break;
			}
		}
	}
	if (lladdr == NULL)
		errx(EXIT_FAILURE, "could not determine link local address"); 

 	memcpy(&in6->s6_addr[8], &lladdr->s6_addr[8], 8);

	freeifaddrs(ifap);
	return 0;
}

/* XXX not really an alias */
void
in6_alias(const char *ifname, prop_dictionary_t env, prop_dictionary_t oenv,
    struct in6_ifreq *creq)
{
	struct in6_ifreq ifr6;
	struct sockaddr_in6 *sin6;
	char hbuf[NI_MAXHOST];
	u_int32_t scopeid;
	int s;
	const int niflag = Nflag ? 0 : NI_NUMERICHOST;
	unsigned short flags;

	/* Get the non-alias address for this interface. */
	if ((s = getsock(AF_INET6)) == -1) {
		if (errno == EAFNOSUPPORT)
			return;
		err(EXIT_FAILURE, "socket");
	}

	sin6 = &creq->ifr_addr;

	inet6_getscopeid(sin6, INET6_IS_ADDR_LINKLOCAL);
	scopeid = sin6->sin6_scope_id;
	if (getnameinfo((const struct sockaddr *)sin6, sin6->sin6_len,
			hbuf, sizeof(hbuf), NULL, 0, niflag))
		strlcpy(hbuf, "", sizeof(hbuf));	/* some message? */
	printf("\tinet6 %s", hbuf);

	if (getifflags(env, oenv, &flags) == -1)
		err(EXIT_FAILURE, "%s: getifflags", __func__);

	if (flags & IFF_POINTOPOINT) {
		ifr6 = *creq;
		if (prog_ioctl(s, SIOCGIFDSTADDR_IN6, &ifr6) == -1) {
			if (errno != EADDRNOTAVAIL)
				warn("SIOCGIFDSTADDR_IN6");
			memset(&ifr6.ifr_addr, 0, sizeof(ifr6.ifr_addr));
			ifr6.ifr_addr.sin6_family = AF_INET6;
			ifr6.ifr_addr.sin6_len = sizeof(struct sockaddr_in6);
		}
		sin6 = &ifr6.ifr_addr;
		inet6_getscopeid(sin6, INET6_IS_ADDR_LINKLOCAL);
		hbuf[0] = '\0';
		if (getnameinfo((struct sockaddr *)sin6, sin6->sin6_len,
				hbuf, sizeof(hbuf), NULL, 0, niflag))
			strlcpy(hbuf, "", sizeof(hbuf)); /* some message? */
		printf(" -> %s", hbuf);
	}

	ifr6 = *creq;
	if (prog_ioctl(s, SIOCGIFNETMASK_IN6, &ifr6) == -1) {
		if (errno != EADDRNOTAVAIL)
			warn("SIOCGIFNETMASK_IN6");
	} else {
		sin6 = &ifr6.ifr_addr;
		printf(" prefixlen %d", prefix(&sin6->sin6_addr,
					       sizeof(struct in6_addr)));
	}

	ifr6 = *creq;
	if (prog_ioctl(s, SIOCGIFAFLAG_IN6, &ifr6) == -1) {
		if (errno != EADDRNOTAVAIL)
			warn("SIOCGIFAFLAG_IN6");
	} else {
		if (ifr6.ifr_ifru.ifru_flags6 & IN6_IFF_ANYCAST)
			printf(" anycast");
		if (ifr6.ifr_ifru.ifru_flags6 & IN6_IFF_TENTATIVE)
			printf(" tentative");
		if (ifr6.ifr_ifru.ifru_flags6 & IN6_IFF_DUPLICATED)
			printf(" duplicated");
		if (ifr6.ifr_ifru.ifru_flags6 & IN6_IFF_DETACHED)
			printf(" detached");
		if (ifr6.ifr_ifru.ifru_flags6 & IN6_IFF_DEPRECATED)
			printf(" deprecated");
		if (ifr6.ifr_ifru.ifru_flags6 & IN6_IFF_AUTOCONF)
			printf(" autoconf");
		if (ifr6.ifr_ifru.ifru_flags6 & IN6_IFF_TEMPORARY)
			printf(" temporary");
	}

	if (scopeid)
		printf(" scopeid 0x%x", scopeid);

	if (get_flag('L')) {
		struct in6_addrlifetime *lifetime;
		ifr6 = *creq;
		lifetime = &ifr6.ifr_ifru.ifru_lifetime;
		if (prog_ioctl(s, SIOCGIFALIFETIME_IN6, &ifr6) == -1) {
			if (errno != EADDRNOTAVAIL)
				warn("SIOCGIFALIFETIME_IN6");
		} else if (lifetime->ia6t_preferred || lifetime->ia6t_expire) {
			time_t t = time(NULL);
			printf(" pltime ");
			if (lifetime->ia6t_preferred) {
				printf("%lu",
				    (unsigned long)(lifetime->ia6t_preferred -
				        MIN(t, lifetime->ia6t_preferred)));
			} else
				printf("infty");

			printf(" vltime ");
			if (lifetime->ia6t_expire) {
				printf("%lu",
				    (unsigned long)(lifetime->ia6t_expire -
				        MIN(t, lifetime->ia6t_expire)));
			} else
				printf("infty");
		}
	}
}

static void
in6_status(prop_dictionary_t env, prop_dictionary_t oenv, bool force)
{
	struct ifaddrs *ifap, *ifa;
	struct in6_ifreq ifr;
	const char *ifname;
	bool printprefs = false;

	if ((ifname = getifname(env)) == NULL)
		err(EXIT_FAILURE, "%s: getifname", __func__);

	if (getifaddrs(&ifap) != 0)
		err(EXIT_FAILURE, "getifaddrs");
	printprefs = ifa_any_preferences(ifname, ifap, AF_INET6);
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (strcmp(ifname, ifa->ifa_name) != 0)
			continue;
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		if (sizeof(ifr.ifr_addr) < ifa->ifa_addr->sa_len)
			continue;

		memset(&ifr, 0, sizeof(ifr));
		estrlcpy(ifr.ifr_name, ifa->ifa_name, sizeof(ifr.ifr_name));
		memcpy(&ifr.ifr_addr, ifa->ifa_addr, ifa->ifa_addr->sa_len);
		in6_alias(ifname, env, oenv, &ifr);
		if (printprefs)
			ifa_print_preference(ifa->ifa_name, ifa->ifa_addr);
		printf("\n");
	}
	freeifaddrs(ifap);
}

static int
in6_pre_aifaddr(prop_dictionary_t env, const struct afparam *param)
{
	struct in6_aliasreq *ifra = param->req.buf;

	setia6eui64_impl(env, ifra);
	setia6vltime_impl(env, ifra);
	setia6pltime_impl(env, ifra);
	setia6flags_impl(env, ifra);
	inet6_putscopeid(&ifra->ifra_addr, INET6_IS_ADDR_LINKLOCAL);
	inet6_putscopeid(&ifra->ifra_dstaddr, INET6_IS_ADDR_LINKLOCAL);

	return 0;
}

static void
in6_commit_address(prop_dictionary_t env, prop_dictionary_t oenv)
{
	struct in6_ifreq in6_ifr = {
		.ifr_addr = {
			.sin6_family = AF_INET6,
			.sin6_len = sizeof(in6_ifr.ifr_addr),
			.sin6_addr = {
				.s6_addr =
				    {0xff, 0xff, 0xff, 0xff,
				     0xff, 0xff, 0xff, 0xff}
			}
		}
	};
	static struct sockaddr_in6 in6_defmask = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(in6_defmask),
		.sin6_addr = {
			.s6_addr = {0xff, 0xff, 0xff, 0xff,
			            0xff, 0xff, 0xff, 0xff}
		}
	};

	struct in6_aliasreq in6_ifra = {
		.ifra_prefixmask = {
			.sin6_family = AF_INET6,
			.sin6_len = sizeof(in6_ifra.ifra_prefixmask),
			.sin6_addr = {
				.s6_addr =
				    {0xff, 0xff, 0xff, 0xff,
				     0xff, 0xff, 0xff, 0xff}}},
		.ifra_lifetime = {
			  .ia6t_pltime = ND6_INFINITE_LIFETIME
			, .ia6t_vltime = ND6_INFINITE_LIFETIME
		}
	};
	struct afparam in6param = {
		  .req = BUFPARAM(in6_ifra)
		, .dgreq = BUFPARAM(in6_ifr)
		, .name = {
			{.buf = in6_ifr.ifr_name,
			 .buflen = sizeof(in6_ifr.ifr_name)},
			{.buf = in6_ifra.ifra_name,
			 .buflen = sizeof(in6_ifra.ifra_name)}
		  }
		, .dgaddr = BUFPARAM(in6_ifr.ifr_addr)
		, .addr = BUFPARAM(in6_ifra.ifra_addr)
		, .dst = BUFPARAM(in6_ifra.ifra_dstaddr)
		, .brd = BUFPARAM(in6_ifra.ifra_broadaddr)
		, .mask = BUFPARAM(in6_ifra.ifra_prefixmask)
		, .aifaddr = IFADDR_PARAM(SIOCAIFADDR_IN6)
		, .difaddr = IFADDR_PARAM(SIOCDIFADDR_IN6)
		, .gifaddr = IFADDR_PARAM(SIOCGIFADDR_IN6)
		, .defmask = BUFPARAM(in6_defmask)
		, .pre_aifaddr = in6_pre_aifaddr
	};
	commit_address(env, oenv, &in6param);
}

static bool
in6_addr_tentative(struct ifaddrs *ifa)
{
	int s;
	struct in6_ifreq ifr;

	if ((s = getsock(AF_INET6)) == -1)
		err(EXIT_FAILURE, "%s: getsock", __func__);
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifa->ifa_name, sizeof(ifr.ifr_name));
	ifr.ifr_addr = *(struct sockaddr_in6 *)ifa->ifa_addr;
	if (prog_ioctl(s, SIOCGIFAFLAG_IN6, &ifr) == -1)
		err(EXIT_FAILURE, "SIOCGIFAFLAG_IN6");
	return ifr.ifr_ifru.ifru_flags6 & IN6_IFF_TENTATIVE ? true : false;
}

static void
in6_usage(prop_dictionary_t env)
{
	fprintf(stderr,
	    "\t[ anycast | -anycast ] [ deprecated | -deprecated ]\n"
	    "\t[ pltime n ] [ vltime n ] "
	    "[ eui64 ]\n");
}

static void
in6_constructor(void)
{
	if (register_flag('L') != 0)
		err(EXIT_FAILURE, __func__);
	register_family(&in6af);
	usage_func_init(&usage, in6_usage);
	register_usage(&usage);
	cmdloop_branch_init(&branch[0], &ia6flags.pk_parser);
	cmdloop_branch_init(&branch[1], &inet6.pk_parser);
	register_cmdloop_branch(&branch[0]);
	register_cmdloop_branch(&branch[1]);
}
