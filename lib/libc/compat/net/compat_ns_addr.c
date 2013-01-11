/*	$NetBSD: compat_ns_addr.c,v 1.3 2012/10/15 22:22:01 msaitoh Exp $	*/

/*
 * Copyright (c) 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * J.Q. Johnson.
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
static char sccsid[] = "@(#)ns_addr.c	8.1 (Berkeley) 6/7/93";
#else
__RCSID("$NetBSD: compat_ns_addr.c,v 1.3 2012/10/15 22:22:01 msaitoh Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/param.h>
#include <compat/include/ns.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void Field(char *, uint8_t *, int);
static void cvtbase(long, int, int[], int, uint8_t [], int);

struct ns_addr 
ns_addr(const char *name)
{
	char separator;
	char *hostname, *socketname, *cp;
	char buf[50];
	static struct ns_addr addr;

	_DIAGASSERT(name != NULL);

	(void)strlcpy(buf, name, sizeof(buf));

	/*
	 * First, figure out what he intends as a field separtor.
	 * Despite the way this routine is written, the prefered
	 * form  2-272.AA001234H.01777, i.e. XDE standard.
	 * Great efforts are made to insure backward compatibility.
	 */
	if ((hostname = strchr(buf, '#')) != NULL)
		separator = '#';
	else {
		hostname = strchr(buf, '.');
		if ((cp = strchr(buf, ':')) &&
		    ((hostname && cp < hostname) || (hostname == 0))) {
			hostname = cp;
			separator = ':';
		} else
			separator = '.';
	}
	if (hostname)
		*hostname++ = 0;

	memset(&addr, '\0', sizeof(addr));
	Field(buf, addr.x_net.c_net, 4);
	if (hostname == 0)
		return (addr);  /* No separator means net only */

	socketname = strchr(hostname, separator);
	if (socketname) {
		*socketname++ = 0;
		Field(socketname, (uint8_t *)(void *)&addr.x_port, 2);
	}

	Field(hostname, addr.x_host.c_host, 6);

	return (addr);
}

static void
Field(char *buf, uint8_t *out, int len)
{
	register char *bp = buf;
	int i, ibase, base16 = 0, base10 = 0;
	unsigned int clen = 0;
	int hb[6], *hp;

	_DIAGASSERT(buf != NULL);
	_DIAGASSERT(out != NULL);

	/*
	 * first try 2-273#2-852-151-014#socket
	 */
	if ((*buf != '-') &&
	    (1 < (i = sscanf(buf, "%d-%d-%d-%d-%d",
			&hb[0], &hb[1], &hb[2], &hb[3], &hb[4])))) {
		cvtbase(1000L, 256, hb, i, out, len);
		return;
	}
	/*
	 * try form 8E1#0.0.AA.0.5E.E6#socket
	 */
	if (1 < (i = sscanf(buf,"%x.%x.%x.%x.%x.%x",
			&hb[0], &hb[1], &hb[2], &hb[3], &hb[4], &hb[5]))) {
		cvtbase(256L, 256, hb, i, out, len);
		return;
	}
	/*
	 * try form 8E1#0:0:AA:0:5E:E6#socket
	 */
	if (1 < (i = sscanf(buf,"%x:%x:%x:%x:%x:%x",
			&hb[0], &hb[1], &hb[2], &hb[3], &hb[4], &hb[5]))) {
		cvtbase(256L, 256, hb, i, out, len);
		return;
	}
	/*
	 * This is REALLY stretching it but there was a
	 * comma notation separting shorts -- definitely non standard
	 */
	if (1 < (i = sscanf(buf,"%x,%x,%x",
			&hb[0], &hb[1], &hb[2]))) {
		hb[0] = htons(hb[0]); hb[1] = htons(hb[1]);
		hb[2] = htons(hb[2]);
		cvtbase(65536L, 256, hb, i, out, len);
		return;
	}

	/* Need to decide if base 10, 16 or 8 */
	while (*bp) switch (*bp++) {

	case '0': case '1': case '2': case '3': case '4': case '5':
	case '6': case '7': case '-':
		break;

	case '8': case '9':
		base10 = 1;
		break;

	case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
	case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
		base16 = 1;
		break;
	
	case 'x': case 'X':
		*--bp = '0';
		base16 = 1;
		break;

	case 'h': case 'H':
		base16 = 1;
		/* FALLTHROUGH */

	default:
		*--bp = 0; /* Ends Loop */
	}
	if (base16) {
		ibase = 4096;
	} else if (base10 == 0 && *buf == '0') {
		ibase = 512;
	} else {
		base10 = 1;
		ibase = 1000;
	}

	for (bp = buf; *bp++; ) clen++;
	if (clen == 0) clen++;
	if (clen > 18) clen = 18;
	i = ((clen - 1) / 3) + 1;
	bp = clen + buf - 3;
	hp = hb + i - 1;

	while (hp > hb) {
		if (base16)
			(void)sscanf(bp, "%3x", hp);
		else if (base10)
			(void)sscanf(bp, "%3d", hp);
		else
			(void)sscanf(bp, "%3o", hp);

		bp[0] = 0;
		hp--;
		bp -= 3;
	}
	if (base16)
		(void)sscanf(buf, "%3x", hp);
	else if (base10)
		(void)sscanf(buf, "%3d", hp);
	else
		(void)sscanf(buf, "%3o", hp);

	cvtbase((long)ibase, 256, hb, i, out, len);
}

static void
cvtbase(long oldbase, int newbase, int input[], int inlen,
	uint8_t result[], int reslen)
{
	int d, e;
	long sum;

	_DIAGASSERT(input != NULL);
	_DIAGASSERT(result != NULL);
	_DIAGASSERT(inlen > 0);

	e = 1;
	while (e > 0 && reslen > 0) {
		d = 0; e = 0; sum = 0;
		/* long division: input=input/newbase */
		while (d < inlen) {
			sum = sum*oldbase + (long) input[d];
			e += (sum > 0);
			input[d++] = (int) (sum / newbase);
			sum %= newbase;
		}
		/* accumulate remainder */
		result[--reslen] = (unsigned char)sum;
	}
	for (d=0; d < reslen; d++)
		result[d] = 0;
}
