/*	$NetBSD: util.c,v 1.17 2013/10/19 00:35:30 christos Exp $	*/

/*-
 * Copyright (c) 2008 David Young.  All rights reserved.
 *
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

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: util.c,v 1.17 2013/10/19 00:35:30 christos Exp $");
#endif /* not lint */

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>

#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <netinet/in.h>		/* XXX */

#include "env.h"
#include "extern.h"
#include "util.h"
#include "prog_ops.h"

int
getsock(int naf)
{
	static int oaf = -1, s;

	if (oaf == naf || (oaf != -1 && naf == AF_UNSPEC))
		return s;

	if (oaf != -1)
		prog_close(s);

	if (naf == AF_UNSPEC)
		naf = AF_INET;

	s = prog_socket(naf, SOCK_DGRAM, 0);
	if (s == -1)
		oaf = -1;
	else
		oaf = naf;
	return s;
}

const char *
get_string(const char *val, const char *sep, u_int8_t *buf, int *lenp,
    bool hexok)
{
	int len;
	bool hexstr;
	u_int8_t *p;

	len = *lenp;
	p = buf;
	hexstr = hexok && val[0] == '0' && tolower((u_char)val[1]) == 'x';
	if (hexstr)
		val += 2;
	for (;;) {
		if (*val == '\0')
			break;
		if (sep != NULL && strchr(sep, *val) != NULL) {
			val++;
			break;
		}
		if (hexstr) {
			if (!isxdigit((u_char)val[0]) ||
			    !isxdigit((u_char)val[1])) {
				warnx("bad hexadecimal digits");
				return NULL;
			}
		}
		if (p >= buf + len) {
			if (hexstr)
				warnx("hexadecimal digits too long");
			else
				warnx("strings too long");
			return NULL;
		}
		if (hexstr) {
#define	tohex(x)	(isdigit(x) ? (x) - '0' : tolower(x) - 'a' + 10)
			*p++ = (tohex((u_char)val[0]) << 4) |
			    tohex((u_char)val[1]);
#undef tohex
			val += 2;
		} else
			*p++ = *val++;
	}
	len = p - buf;
	if (len < *lenp)
		memset(p, 0, *lenp - len);
	*lenp = len;
	return val;
}

void
print_string(const u_int8_t *buf, int len)
{
	int i;
	bool hasspc;

	i = 0;
	hasspc = false;
	if (len < 2 || buf[0] != '0' || tolower(buf[1]) != 'x') {
		for (; i < len; i++) {
			if (!isprint(buf[i]))
				break;
			if (isspace(buf[i]))
				hasspc = true;
		}
	}
	if (i == len) {
		if (hasspc || len == 0)
			printf("\"%.*s\"", len, buf);
		else
			printf("%.*s", len, buf);
	} else {
		printf("0x");
		for (i = 0; i < len; i++)
			printf("%02x", buf[i]);
	}
}

struct paddr_prefix *
prefixlen_to_mask(int af, int plen)
{
	union {
		struct sockaddr sa; 
		struct sockaddr_in sin; 
		struct sockaddr_in6 sin6; 
	} u;
	struct paddr_prefix *pfx;
	size_t addrlen;
	uint8_t *addr;
	int nbit;

	memset(&u, 0, sizeof(u));

	switch (af) {
	case AF_INET:
		addrlen = sizeof(u.sin.sin_addr);
		addr = (uint8_t *)&u.sin.sin_addr;
		u.sa.sa_len = sizeof(u.sin);
		break;
	case AF_INET6:
		addrlen = sizeof(u.sin6.sin6_addr);
		addr = (uint8_t *)&u.sin6.sin6_addr;
		u.sa.sa_len = sizeof(u.sin6);
		break;
	default:
		errno = EINVAL;
		return NULL;
	}
	u.sa.sa_family = af;

	if (plen < 0 || (size_t)plen > addrlen * NBBY) {
		errno = EINVAL;
		return NULL;
	}

	if (plen == 0)
		plen = addrlen * NBBY;

	memset(addr, 0xff, (plen + NBBY - 1) / NBBY);

	nbit = plen % NBBY;
	if (nbit != 0)
		addr[plen / NBBY] &= ~((uint8_t)0xff >> nbit);
	pfx = malloc(offsetof(struct paddr_prefix, pfx_addr) + u.sa.sa_len);
	if (pfx == NULL)
		return NULL;
	pfx->pfx_len = plen;
	memcpy(&pfx->pfx_addr, &u.sa, u.sa.sa_len);

	return pfx;
}

int
direct_ioctl(prop_dictionary_t env, unsigned long cmd, void *data)
{
	const char *ifname;
	int s;

	if ((s = getsock(AF_UNSPEC)) == -1)
		err(EXIT_FAILURE, "getsock");

	if ((ifname = getifname(env)) == NULL)
		err(EXIT_FAILURE, "getifname");

	estrlcpy(data, ifname, IFNAMSIZ);

	return prog_ioctl(s, cmd, data);
}

int
indirect_ioctl(prop_dictionary_t env, unsigned long cmd, void *data)
{
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));

	ifr.ifr_data = data;

	return direct_ioctl(env, cmd, &ifr);
}

void
print_link_addresses(prop_dictionary_t env, bool print_active_only)
{
	char hbuf[NI_MAXHOST];
	const char *ifname;
	int s;
	struct ifaddrs *ifa, *ifap;
	const struct sockaddr_dl *sdl;
	struct if_laddrreq iflr;

	if ((ifname = getifname(env)) == NULL)
		err(EXIT_FAILURE, "%s: getifname", __func__);

	if ((s = getsock(AF_LINK)) == -1)
		err(EXIT_FAILURE, "%s: getsock", __func__);

	if (getifaddrs(&ifap) == -1)
		err(EXIT_FAILURE, "%s: getifaddrs", __func__);

	memset(&iflr, 0, sizeof(iflr));

	strlcpy(iflr.iflr_name, ifname, sizeof(iflr.iflr_name));

	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		if (strcmp(ifname, ifa->ifa_name) != 0)
			continue;
		if (ifa->ifa_addr->sa_family != AF_LINK)
			continue;

		sdl = satocsdl(ifa->ifa_addr);

		memcpy(&iflr.addr, ifa->ifa_addr, MIN(ifa->ifa_addr->sa_len,
		    sizeof(iflr.addr)));
		iflr.flags = IFLR_PREFIX;
		iflr.prefixlen = sdl->sdl_alen * NBBY;

		if (prog_ioctl(s, SIOCGLIFADDR, &iflr) == -1)
			err(EXIT_FAILURE, "%s: ioctl", __func__);

		if (((iflr.flags & IFLR_ACTIVE) != 0) != print_active_only)
			continue;

		if (getnameinfo(ifa->ifa_addr, ifa->ifa_addr->sa_len,
			hbuf, sizeof(hbuf), NULL, 0,
			Nflag ? 0 : NI_NUMERICHOST) == 0 &&
		    hbuf[0] != '\0') {
			printf("\t%s %s\n",
			    print_active_only ? "address:" : "link", hbuf);
		}
	}
	freeifaddrs(ifap);
}

int16_t
ifa_get_preference(const char *ifname, const struct sockaddr *sa)
{
	struct if_addrprefreq ifap;
	int s;

	if ((s = getsock(sa->sa_family)) == -1) {
		if (errno == EPROTONOSUPPORT)
			return 0;
		err(EXIT_FAILURE, "socket");
	}
	memset(&ifap, 0, sizeof(ifap));
	estrlcpy(ifap.ifap_name, ifname, sizeof(ifap.ifap_name));
	memcpy(&ifap.ifap_addr, sa, MIN(sizeof(ifap.ifap_addr), sa->sa_len));
	if (prog_ioctl(s, SIOCGIFADDRPREF, &ifap) == -1) {
		if (errno == EADDRNOTAVAIL || errno == EAFNOSUPPORT)
			return 0;
		warn("SIOCGIFADDRPREF");
	}
	return ifap.ifap_preference;
}

void
ifa_print_preference(const char *ifname, const struct sockaddr *sa)
{
	int16_t preference;

	if (lflag)
		return;

	preference = ifa_get_preference(ifname, sa);
	printf(" preference %" PRId16, preference);
}

bool
ifa_any_preferences(const char *ifname, struct ifaddrs *ifap, int family)
{
	struct ifaddrs *ifa;

	/* Print address preference numbers if any address has a non-zero
	 * preference assigned.
	 */
	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		if (strcmp(ifname, ifa->ifa_name) != 0)
			continue;
		if (ifa->ifa_addr->sa_family != family)
			continue;
		if (ifa_get_preference(ifa->ifa_name, ifa->ifa_addr) != 0)
			return true;
	}
	return false;
}
