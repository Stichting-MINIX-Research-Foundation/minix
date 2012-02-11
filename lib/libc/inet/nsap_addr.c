/*	$NetBSD: nsap_addr.c,v 1.6 2009/04/12 17:07:17 christos Exp $	*/

/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996-1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static const char rcsid[] = "Id: nsap_addr.c,v 1.5 2005/07/28 06:51:48 marka Exp";
#else
__RCSID("$NetBSD: nsap_addr.c,v 1.6 2009/04/12 17:07:17 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "port_before.h"

#include "namespace.h"
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <assert.h>
#include <ctype.h>
#include <resolv.h>
#include <resolv_mt.h>

#include "port_after.h"

#ifdef __weak_alias
__weak_alias(inet_nsap_addr,_inet_nsap_addr)
__weak_alias(inet_nsap_ntoa,_inet_nsap_ntoa)
#endif

static char
xtob(int c) {
	return (c - (((c >= '0') && (c <= '9')) ? '0' : '7'));
}

u_int
inet_nsap_addr(const char *ascii, u_char *binary, int maxlen) {
	u_char c, nib;
	u_int len = 0;

	_DIAGASSERT(ascii != NULL);
	_DIAGASSERT(binary != NULL);

	if (ascii[0] != '0' || (ascii[1] != 'x' && ascii[1] != 'X'))
		return (0);
	ascii += 2;

	while ((c = *ascii++) != '\0' && len < (u_int)maxlen) {
		if (c == '.' || c == '+' || c == '/')
			continue;
		if (!isascii(c))
			return (0);
		if (islower(c))
			c = toupper(c);
		if (isxdigit(c)) {
			nib = xtob(c);
			c = *ascii++;
			if (c != '\0') {
				c = toupper(c);
				if (isxdigit(c)) {
					*binary++ = (nib << 4) | xtob(c);
					len++;
				} else
					return (0);
			}
			else
				return (0);
		}
		else
			return (0);
	}
	return (len);
}

char *
inet_nsap_ntoa(int binlen, const u_char *binary, char *ascii) {
	int nib;
	int i;
	char *tmpbuf = inet_nsap_ntoa_tmpbuf;
	char *start;

	_DIAGASSERT(binary != NULL);

	if (ascii)
		start = ascii;
	else {
		ascii = tmpbuf;
		start = tmpbuf;
	}

	*ascii++ = '0';
	*ascii++ = 'x';

	if (binlen > 255)
		binlen = 255;

	for (i = 0; i < binlen; i++) {
		nib = (u_int32_t)*binary >> 4;
		*ascii++ = nib + (nib < 10 ? '0' : '7');
		nib = *binary++ & 0x0f;
		*ascii++ = nib + (nib < 10 ? '0' : '7');
		if (((i % 2) == 0 && (i + 1) < binlen))
			*ascii++ = '.';
	}
	*ascii = '\0';
	return (start);
}

/*! \file */
