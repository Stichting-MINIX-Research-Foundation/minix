/*	$NetBSD: in6_print.c,v 1.1 2014/12/02 19:36:58 christos Exp $	*/

/*-
 * Copyright (c) 2014 The NetBSD Foundation, Inc.
 * All rights reserved.
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

#include <sys/types.h>
#ifdef _KERNEL
__KERNEL_RCSID(0, "$NetBSD: in6_print.c,v 1.1 2014/12/02 19:36:58 christos Exp $");
#include <sys/systm.h>
#else
__RCSID("$NetBSD: in6_print.c,v 1.1 2014/12/02 19:36:58 christos Exp $");
#include <stdio.h>
#define s6_addr32 __u6_addr.__u6_addr32
static const uint8_t hexdigits[] = "0123456789abcdef";
#endif

#include <netinet/in.h>

int
in6_print(char *buf, size_t len, const struct in6_addr *ia6)
{
	int i;
	char *bp;
	char *cp, *ecp;
	const uint16_t *a;
	const uint8_t *d;
	int dcolon = 0;

	if (IN6_IS_ADDR_V4MAPPED(ia6)) {
		char buf4[INET_ADDRSTRLEN];
		struct in_addr ia = { .s_addr = ia6->s6_addr32[3] };
		in_print(buf4, sizeof(buf4), &ia);
		return snprintf(buf, len, "::ffff:%s", buf4);
	}

#define ADDC(c) do { \
		if (cp >= ecp) {\
			cp++; \
		} else \
			*cp++ = (char)(c); \
	} while (/*CONSTCOND*/0)
#define ADDX(v) do { \
		uint8_t n = hexdigits[(v)]; \
		ADDC(n); \
		if (cp == bp && n == '0') \
			cp--; \
	} while (/*CONSTCOND*/0)

	cp = buf;
	ecp = buf + len;
	a = (const uint16_t *)ia6;
	for (i = 0; i < 8; i++) {
		if (dcolon == 1) {
			if (*a == 0) {
				if (i == 7)
					ADDC(':');
				a++;
				continue;
			} else
				dcolon = 2;
		}
		if (*a == 0) {
			if (dcolon == 0 && *(a + 1) == 0) {
				if (i == 0)
					ADDC(':');
				ADDC(':');
				dcolon = 1;
			} else {
				ADDC('0');
				ADDC(':');
			}
			a++;
			continue;
		}
		d = (const u_char *)a;
		bp = cp + 1;

		ADDX((u_int)*d >> 4);
		ADDX(*d & 0xf);
		d++;
		ADDX((u_int)*d >> 4);
		ADDX(*d & 0xf);
		ADDC(':');
		a++;
	}
	if (cp > buf)
		--cp;
	if (ecp > buf) {
		if (cp < ecp)
			*cp = '\0';
		else
			*--ecp = '\0';
	}
	return (int)(cp - buf);
}

int
sin6_print(char *buf, size_t len, const void *v)
{
	const struct sockaddr_in6 *sin6 = v;
	const struct in6_addr *ia6 = &sin6->sin6_addr;
	char abuf[INET6_ADDRSTRLEN];

	if (!sin6->sin6_port)
		return in6_print(buf, len, ia6);
	in6_print(abuf, sizeof(abuf), ia6);
	return snprintf(buf, len, "[%s]:%hu", abuf, ntohs(sin6->sin6_port));
}
