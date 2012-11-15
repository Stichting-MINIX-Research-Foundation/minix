/*	$NetBSD: sockaddr_snprintf.c,v 1.9 2008/04/28 20:23:03 martin Exp $	*/

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
__RCSID("$NetBSD: sockaddr_snprintf.c,v 1.9 2008/04/28 20:23:03 martin Exp $");
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <netinet/in.h>
#ifndef __minix
#include <netatalk/at.h>
#include <net/if_dl.h>
#endif

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <util.h>
#include <netdb.h>

int
sockaddr_snprintf(char * const sbuf, const size_t len, const char * const fmt,
    const struct sockaddr * const sa)
{
	const void *a = NULL;
#ifndef __minix
	char abuf[1024], nbuf[1024], *addr = NULL, *w = NULL;
#else
	char abuf[1024], nbuf[1024], *addr = NULL;
#endif
	char Abuf[1024], pbuf[32], *name = NULL, *port = NULL;
	char *ebuf = &sbuf[len - 1], *buf = sbuf;
	const char *ptr, *s;
	int p = -1;
#ifndef __minix
	const struct sockaddr_at *sat = NULL;
#endif
	const struct sockaddr_in *sin4 = NULL;
#ifndef __minix
	const struct sockaddr_in6 *sin6 = NULL;
	const struct sockaddr_un *sun = NULL;
	const struct sockaddr_dl *sdl = NULL;
#endif
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
#ifndef __minix
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
#endif
	case AF_INET:
		sin4 = ((const struct sockaddr_in *)(const void *)sa);
		p = ntohs(sin4->sin_port);
		a = &sin4->sin_addr;
		break;
#ifndef __minix
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
#endif
	default:
		errno = EAFNOSUPPORT;
		return -1;
	}

	if (addr == abuf)
		name = addr;

#ifndef __minix
	if (a && getnameinfo(sa, (socklen_t)sa->sa_len, addr = abuf,
#else
	if (a && getnameinfo(sa, (socklen_t)len, addr = abuf,
#endif
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
#ifndef __minix
			(void)snprintf(nbuf, sizeof(nbuf), "%d", sa->sa_len);
#else
			(void)snprintf(nbuf, sizeof(nbuf), "%d", len);
#endif
			ADDS(nbuf);
			break;
		case 'A':
			if (name)
				ADDS(name);
			else if (!a)
				ADDNA();
			else {
#ifndef __minix
				getnameinfo(sa, (socklen_t)sa->sa_len,
#else
				getnameinfo(sa, (socklen_t)len,
#endif
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
#ifndef __minix
				getnameinfo(sa, (socklen_t)sa->sa_len, NULL, 0,
#else
				getnameinfo(sa, (socklen_t)len, NULL, 0,
#endif
					port = pbuf,
					(unsigned int)sizeof(pbuf), 0);
				ADDS(port);
			}
			break;
#ifndef __minix
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
#endif
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
