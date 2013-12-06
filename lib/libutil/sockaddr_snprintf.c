/*	$NetBSD: sockaddr_snprintf.c,v 1.10 2013/06/07 17:23:26 christos Exp $	*/

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: sockaddr_snprintf.c,v 1.10 2013/06/07 17:23:26 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <netinet/in.h>
#if !defined(__minix)
#include <netatalk/at.h>
#include <net/if_dl.h>
#endif

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <util.h>
#include <netdb.h>

#if !defined(__minix)
static int
debug_at(char *str, size_t len, const struct sockaddr_at *sat)
{
	return snprintf(str, len, "sat_len=%u, sat_family=%u, sat_port=%u, "
	    "sat_addr.s_net=%u, sat_addr.s_node=%u, "
	    "sat_range.r_netrange.nr_phase=%u, "
	    "sat_range.r_netrange.nr_firstnet=%u, "
	    "sat_range.r_netrange.nr_lastnet=%u",
	    sat->sat_len, sat->sat_family, sat->sat_port,
	    sat->sat_addr.s_net, sat->sat_addr.s_node,
	    sat->sat_range.r_netrange.nr_phase,
	    sat->sat_range.r_netrange.nr_firstnet,
	    sat->sat_range.r_netrange.nr_lastnet);
}

static int
debug_in(char *str, size_t len, const struct sockaddr_in *sin)
{
	return snprintf(str, len, "sin_len=%u, sin_family=%u, sin_port=%u, "
	    "sin_addr.s_addr=%08x",
	    sin->sin_len, sin->sin_family, sin->sin_port,
	    sin->sin_addr.s_addr);
}

static int
debug_in6(char *str, size_t len, const struct sockaddr_in6 *sin6)
{
	const uint8_t *s = sin6->sin6_addr.s6_addr;

	return snprintf(str, len, "sin6_len=%u, sin6_family=%u, sin6_port=%u, "
	    "sin6_flowinfo=%u, "
	    "sin6_addr=%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:"
	    "%02x:%02x:%02x:%02x:%02x:%02x, sin6_scope_id=%u",
	    sin6->sin6_len, sin6->sin6_family, sin6->sin6_port,
	    sin6->sin6_flowinfo, s[0x0], s[0x1], s[0x2], s[0x3], s[0x4], s[0x5],
	    s[0x6], s[0x7], s[0x8], s[0x9], s[0xa], s[0xb], s[0xc], s[0xd],
	    s[0xe], s[0xf], sin6->sin6_scope_id);
}

static int
debug_un(char *str, size_t len, const struct sockaddr_un *sun)
{
	return snprintf(str, len, "sun_len=%u, sun_family=%u, sun_path=%*s",
	    sun->sun_len, sun->sun_family, (int)sizeof(sun->sun_path),
	    sun->sun_path);
}

static int
debug_dl(char *str, size_t len, const struct sockaddr_dl *sdl)
{
	const uint8_t *s = (const void *)sdl->sdl_data;

	return snprintf(str, len, "sdl_len=%u, sdl_family=%u, sdl_index=%u, "
	    "sdl_type=%u, sdl_nlen=%u, sdl_alen=%u, sdl_slen=%u, sdl_data="
	    "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
	    sdl->sdl_len, sdl->sdl_family, sdl->sdl_index,
	    sdl->sdl_type, sdl->sdl_nlen, sdl->sdl_alen, sdl->sdl_slen,
	    s[0x0], s[0x1], s[0x2], s[0x3], s[0x4], s[0x5],
	    s[0x6], s[0x7], s[0x8], s[0x9], s[0xa], s[0xb]);
}
#endif /* !defined(__minix) */

int
sockaddr_snprintf(char * const sbuf, const size_t len, const char * const fmt,
    const struct sockaddr * const sa)
{
	const void *a = NULL;
#if !defined(__minix)
	char abuf[1024], nbuf[1024], *addr = NULL, *w = NULL;
#else
	char abuf[1024], nbuf[1024], *addr = NULL;
#endif /* !defined(__minix) */
	char Abuf[1024], pbuf[32], *name = NULL, *port = NULL;
	char *ebuf = &sbuf[len - 1], *buf = sbuf;
	const char *ptr, *s;
	int p = -1;
#if !defined(__minix)
	const struct sockaddr_at *sat = NULL;
#endif /* !defined(__minix) */
	const struct sockaddr_in *sin4 = NULL;
#if !defined(__minix)
	const struct sockaddr_in6 *sin6 = NULL;
	const struct sockaddr_un *sun = NULL;
	const struct sockaddr_dl *sdl = NULL;
#endif /* !defined(__minix) */
	int na = 1;

#define ADDC(c) do { if (buf < ebuf) *buf++ = c; else buf++; } \
	while (/*CONSTCOND*/0)
#define ADDS(p) do { for (s = p; *s; s++) ADDC(*s); } \
	while (/*CONSTCOND*/0)
#define ADDNA() do { if (na) ADDS("N/A"); } \
	while (/*CONSTCOND*/0)

	switch (sa->sa_family) {
	case AF_UNSPEC:
		goto done;
#if !defined(__minix)
	case AF_APPLETALK:
		sat = ((const struct sockaddr_at *)(const void *)sa);
		p = ntohs(sat->sat_port);
		(void)snprintf(addr = abuf, sizeof(abuf), "%u.%u",
			ntohs(sat->sat_addr.s_net), sat->sat_addr.s_node);
		(void)snprintf(port = pbuf, sizeof(pbuf), "%d", p);
		break;
	case AF_LOCAL:
		sun = ((const struct sockaddr_un *)(const void *)sa);
		(void)strlcpy(addr = abuf, sun->sun_path, SUN_LEN(sun));
		break;
#endif /* !defined(__minix) */
	case AF_INET:
		sin4 = ((const struct sockaddr_in *)(const void *)sa);
		p = ntohs(sin4->sin_port);
		a = &sin4->sin_addr;
		break;
#if !defined(__minix)
	case AF_INET6:
		sin6 = ((const struct sockaddr_in6 *)(const void *)sa);
		p = ntohs(sin6->sin6_port);
		a = &sin6->sin6_addr;
		break;
	case AF_LINK:
		sdl = ((const struct sockaddr_dl *)(const void *)sa);
		(void)strlcpy(addr = abuf, link_ntoa(sdl), sizeof(abuf));
		if ((w = strchr(addr, ':')) != 0) {
			*w++ = '\0';
			addr = w;
		}
		break;
#endif /* !defined(__minix) */
	default:
		errno = EAFNOSUPPORT;
		return -1;
	}

	if (addr == abuf)
		name = addr;

#if !defined(__minix)
	if (a && getnameinfo(sa, (socklen_t)sa->sa_len, addr = abuf,
#else
	if (a && getnameinfo(sa, (socklen_t)len, addr = abuf,
#endif /* !defined(__minix) */
	    (unsigned int)sizeof(abuf), NULL, 0,
	    NI_NUMERICHOST|NI_NUMERICSERV) != 0)
		return -1;

	for (ptr = fmt; *ptr; ptr++) {
		if (*ptr != '%') {
			ADDC(*ptr);
			continue;
		}
	  next_char:
		switch (*++ptr) {
		case '?':
			na = 0;
			goto next_char;
		case 'a':
			ADDS(addr);
			break;
		case 'p':
			if (p != -1) {
				(void)snprintf(nbuf, sizeof(nbuf), "%d", p);
				ADDS(nbuf);
			} else
				ADDNA();
			break;
		case 'f':
			(void)snprintf(nbuf, sizeof(nbuf), "%d", sa->sa_family);
			ADDS(nbuf);
			break;
		case 'l':
#if !defined(__minix)
			(void)snprintf(nbuf, sizeof(nbuf), "%d", sa->sa_len);
#else
			(void)snprintf(nbuf, sizeof(nbuf), "%d", len);
#endif /* !defined(__minix) */
			ADDS(nbuf);
			break;
		case 'A':
			if (name)
				ADDS(name);
			else if (!a)
				ADDNA();
			else {
#if !defined(__minix)
				getnameinfo(sa, (socklen_t)sa->sa_len,
#else
				getnameinfo(sa, (socklen_t)len,
#endif /* !defined(__minix) */
					name = Abuf,
					(unsigned int)sizeof(nbuf), NULL, 0, 0);
				ADDS(name);
			}
			break;
		case 'P':
			if (port)
				ADDS(port);
			else if (p == -1)
				ADDNA();
			else {
#if !defined(__minix)
				getnameinfo(sa, (socklen_t)sa->sa_len, NULL, 0,
#else
				getnameinfo(sa, (socklen_t)len, NULL, 0,
#endif /* !defined(__minix) */
					port = pbuf,
					(unsigned int)sizeof(pbuf), 0);
				ADDS(port);
			}
			break;
#if !defined(__minix)
		case 'I':
			if (sdl && addr != abuf) {
				ADDS(abuf);
			} else {
				ADDNA();
			}
			break;
		case 'F':
			if (sin6) {
				(void)snprintf(nbuf, sizeof(nbuf), "%d",
				    sin6->sin6_flowinfo);
				ADDS(nbuf);
				break;
			} else {
				ADDNA();
			}
			break;
		case 'S':
			if (sin6) {
				(void)snprintf(nbuf, sizeof(nbuf), "%d",
				    sin6->sin6_scope_id);
				ADDS(nbuf);
				break;
			} else {
				ADDNA();
			}
			break;
		case 'R':
			if (sat) {
				const struct netrange *n =
				    &sat->sat_range.r_netrange;
				(void)snprintf(nbuf, sizeof(nbuf),
				    "%d:[%d,%d]", n->nr_phase , n->nr_firstnet,
				    n->nr_lastnet);
				ADDS(nbuf);
			} else {
				ADDNA();
			}
			break;
		case 'D':
			switch (sa->sa_family) {
			case AF_APPLETALK:
				debug_at(nbuf, sizeof(nbuf), sat);
				break;
			case AF_LOCAL:
				debug_un(nbuf, sizeof(nbuf), sun);
				break;
			case AF_INET:
				debug_in(nbuf, sizeof(nbuf), sin4);
				break;
			case AF_INET6:
				debug_in6(nbuf, sizeof(nbuf), sin6);
				break;
			case AF_LINK:
				debug_dl(nbuf, sizeof(nbuf), sdl);
				break;
			default:
				abort();
			}
			ADDS(nbuf);
			break;
#endif /* !defined(__minix) */
		default:
			ADDC('%');
			if (na == 0)
				ADDC('?');
			if (*ptr == '\0')
				goto done;
			/*FALLTHROUGH*/
		case '%':
			ADDC(*ptr);
			break;
		}
		na = 1;
	}
done:
	if (buf < ebuf)
		*buf = '\0';
	else if (len != 0)
		sbuf[len - 1] = '\0';
	return (int)(buf - sbuf);
}
