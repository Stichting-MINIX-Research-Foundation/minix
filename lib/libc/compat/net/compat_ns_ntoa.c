/*	$NetBSD: compat_ns_ntoa.c,v 1.2 2012/03/20 17:05:59 matt Exp $	*/

/*
 * Copyright (c) 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
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
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)ns_ntoa.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: compat_ns_ntoa.c,v 1.2 2012/03/20 17:05:59 matt Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <compat/include/ns.h>

#include <assert.h>
#include <stdio.h>

static char *spectHex(char *);

char *
ns_ntoa(struct ns_addr addr)
{
	static char obuf[40];
	union { union ns_net net_e; uint32_t long_e; } net;
	uint16_t port = htons(addr.x_port);
	char *cp;
	char *cp2;
	uint8_t *up = addr.x_host.c_host;
	uint8_t *uplim = up + 6;

	net.net_e = addr.x_net;
	sprintf(obuf, "%x", ntohl(net.long_e));
	cp = spectHex(obuf);
	cp2 = cp + 1;
	while (up < uplim && *up==0)
		up++;
	if (up == uplim) {
		if (port) {
			sprintf(cp, ".0");
			cp += 2;
		}
	} else {
		sprintf(cp, ".%x", *up++);
		while (up < uplim) {
			while (*cp) cp++;
			sprintf(cp, "%02x", *up++);
		}
		cp = spectHex(cp2);
	}
	if (port) {
		sprintf(cp, ".%x", port);
		spectHex(cp + 1);
	}
	return (obuf);
}

static char *
spectHex(char *p0)
{
	int ok = 0;
	int nonzero = 0;
	char *p = p0;

	_DIAGASSERT(p0 != NULL);

	for (; *p; p++)
		switch (*p) {
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
			*p += ('A' - 'a');
			/* FALLTHROUGH */
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
			ok = 1;
			/* FALLTHROUGH */
		case '1': case '2': case '3': case '4': case '5':
		case '6': case '7': case '8': case '9':
			nonzero = 1;
		}
	if (nonzero && !ok) { *p++ = 'H'; *p = 0; }
	return (p);
}
