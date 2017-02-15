/*	$NetBSD: af_inet.c,v 1.17 2015/05/12 14:05:29 roy Exp $	*/

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
__RCSID("$NetBSD: af_inet.c,v 1.17 2015/05/12 14:05:29 roy Exp $");
#endif /* not lint */

#include <sys/param.h> 
#include <sys/ioctl.h> 
#include <sys/socket.h>

#include <net/if.h> 
#include <netinet/in.h>
#include <netinet/in_var.h>

#include <arpa/inet.h>

#include <assert.h>
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
#include "af_inetany.h"
#include "prog_ops.h"

static void in_constructor(void) __attribute__((constructor));
static void in_status(prop_dictionary_t, prop_dictionary_t, bool);
static void in_commit_address(prop_dictionary_t, prop_dictionary_t);
static bool in_addr_tentative(struct ifaddrs *ifa);
static void in_alias(const char *, prop_dictionary_t, prop_dictionary_t,
    struct in_aliasreq *);

static struct afswtch af = {
	.af_name = "inet", .af_af = AF_INET, .af_status = in_status,
	.af_addr_commit = in_commit_address,
	.af_addr_tentative = in_addr_tentative
};

static void
in_alias(const char *ifname, prop_dictionary_t env, prop_dictionary_t oenv,
    struct in_aliasreq *creq)
{
	struct ifreq ifr;
	bool alias;
	int s;
	unsigned short flags;
	struct in_aliasreq in_addreq;
	const struct sockaddr_in * const asin = &in_addreq.ifra_addr;
	const struct sockaddr_in * const dsin = &in_addreq.ifra_dstaddr;
	const struct sockaddr_in * const bsin = &in_addreq.ifra_broadaddr;
	char hbuf[NI_MAXHOST];
	const int niflag = Nflag ? 0 : NI_NUMERICHOST;

	if (lflag)
		return;

	alias = true;

	/* Get the non-alias address for this interface. */
	if ((s = getsock(AF_INET)) == -1) {
		if (errno == EAFNOSUPPORT)
			return;
		err(EXIT_FAILURE, "socket");
	}
	memset(&ifr, 0, sizeof(ifr));
	estrlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	if (prog_ioctl(s, SIOCGIFADDR, &ifr) == -1) {
		if (errno == EADDRNOTAVAIL || errno == EAFNOSUPPORT)
			return;
		warn("SIOCGIFADDR");
	}
	/* If creq and ifr are the same address, this is not an alias. */
	if (memcmp(&ifr.ifr_addr, &creq->ifra_addr, sizeof(ifr.ifr_addr)) == 0)
		alias = false;
	in_addreq = *creq;
	if (prog_ioctl(s, SIOCGIFALIAS, &in_addreq) == -1) {
		if (errno == EADDRNOTAVAIL || errno == EAFNOSUPPORT) {
			return;
		} else
			warn("SIOCGIFALIAS");
	}

	if (getnameinfo((const struct sockaddr *)asin, asin->sin_len,
			hbuf, sizeof(hbuf), NULL, 0, niflag))
		strlcpy(hbuf, "", sizeof(hbuf));	/* some message? */
	printf("\tinet %s%s", alias ? "alias " : "", hbuf);

	if (getifflags(env, oenv, &flags) == -1)
		err(EXIT_FAILURE, "%s: getifflags", __func__);

	if (flags & IFF_POINTOPOINT) {
		if (getnameinfo((const struct sockaddr *)dsin, dsin->sin_len,
				hbuf, sizeof(hbuf), NULL, 0, niflag))
			strlcpy(hbuf, "", sizeof(hbuf)); /* some message? */
		printf(" -> %s", hbuf);
	}

	printf(" netmask 0x%x", ntohl(in_addreq.ifra_mask.sin_addr.s_addr));

	if (flags & IFF_BROADCAST) {
		if (getnameinfo((const struct sockaddr *)bsin, bsin->sin_len,
				hbuf, sizeof(hbuf), NULL, 0, niflag))
			strlcpy(hbuf, "", sizeof(hbuf)); /* some message? */
		printf(" broadcast %s", hbuf);
	}

#ifdef IN_IFF_TENTATIVE
	memcpy(&ifr.ifr_addr, &creq->ifra_addr, creq->ifra_addr.sin_len);
	if (prog_ioctl(s, SIOCGIFAFLAG_IN, &ifr) == -1) {
		if (errno != EADDRNOTAVAIL)
			warn("SIOCGIFAFLAG_IN");
	} else {
		if (ifr.ifr_addrflags & IN_IFF_TENTATIVE)
			printf(" tentative");
		if (ifr.ifr_addrflags & IN_IFF_DUPLICATED)
			printf(" duplicated");
		if (ifr.ifr_addrflags & IN_IFF_DETACHED)
			printf(" detached");
	}
#endif
}

static void
in_status(prop_dictionary_t env, prop_dictionary_t oenv, bool force)
{
	struct ifaddrs *ifap, *ifa;
	struct in_aliasreq ifra;
	bool printprefs = false;
	const char *ifname;

	if ((ifname = getifname(env)) == NULL)
		err(EXIT_FAILURE, "%s: getifname", __func__);

	if (getifaddrs(&ifap) != 0)
		err(EXIT_FAILURE, "getifaddrs");

	printprefs = ifa_any_preferences(ifname, ifap, AF_INET);

	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		if (strcmp(ifname, ifa->ifa_name) != 0)
			continue;
		if (ifa->ifa_addr->sa_family != AF_INET)
			continue;
		if (sizeof(ifra.ifra_addr) < ifa->ifa_addr->sa_len)
			continue;

		memset(&ifra, 0, sizeof(ifra));
		estrlcpy(ifra.ifra_name, ifa->ifa_name, sizeof(ifra.ifra_name));
		memcpy(&ifra.ifra_addr, ifa->ifa_addr, ifa->ifa_addr->sa_len);
		in_alias(ifa->ifa_name, env, oenv, &ifra);
		if (printprefs)
			ifa_print_preference(ifa->ifa_name, ifa->ifa_addr);
		printf("\n");
	}
	freeifaddrs(ifap);
}

static void
in_commit_address(prop_dictionary_t env, prop_dictionary_t oenv)
{
	struct ifreq in_ifr;
	struct in_aliasreq in_ifra;
	struct afparam inparam = {
		  .req = BUFPARAM(in_ifra)
		, .dgreq = BUFPARAM(in_ifr)
		, .name = {
			  {.buf = in_ifr.ifr_name,
			   .buflen = sizeof(in_ifr.ifr_name)}
			, {.buf = in_ifra.ifra_name,
			   .buflen = sizeof(in_ifra.ifra_name)}
		  }
		, .dgaddr = BUFPARAM(in_ifr.ifr_addr)
		, .addr = BUFPARAM(in_ifra.ifra_addr)
		, .dst = BUFPARAM(in_ifra.ifra_dstaddr)
		, .brd = BUFPARAM(in_ifra.ifra_broadaddr)
		, .mask = BUFPARAM(in_ifra.ifra_mask)
		, .aifaddr = IFADDR_PARAM(SIOCAIFADDR)
		, .difaddr = IFADDR_PARAM(SIOCDIFADDR)
		, .gifaddr = IFADDR_PARAM(SIOCGIFADDR)
		, .defmask = {.buf = NULL, .buflen = 0}
	};
	memset(&in_ifr, 0, sizeof(in_ifr));
	memset(&in_ifra, 0, sizeof(in_ifra));
	commit_address(env, oenv, &inparam);
}

static bool
in_addr_tentative(struct ifaddrs *ifa)
{
#ifdef IN_IFF_TENTATIVE
	int s;
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifa->ifa_name, sizeof(ifr.ifr_name));
	ifr.ifr_addr = *ifa->ifa_addr;
	if ((s = getsock(AF_INET)) == -1)
		err(EXIT_FAILURE, "%s: getsock", __func__);
	if (prog_ioctl(s, SIOCGIFAFLAG_IN, &ifr) == -1)
		err(EXIT_FAILURE, "SIOCGIFAFLAG_IN");
	return ifr.ifr_addrflags & IN_IFF_TENTATIVE ? true : false;
#else
	return false;
#endif
}

static void
in_constructor(void)
{
	register_family(&af);
}
