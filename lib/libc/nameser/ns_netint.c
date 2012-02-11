/*	$NetBSD: ns_netint.c,v 1.6 2009/04/12 17:07:17 christos Exp $	*/

/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996,1999 by Internet Software Consortium.
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
#ifndef lint
#ifdef notdef
static const char rcsid[] = "Id: ns_netint.c,v 1.3 2005/04/27 04:56:40 sra Exp";
#else
__RCSID("$NetBSD: ns_netint.c,v 1.6 2009/04/12 17:07:17 christos Exp $");
#endif
#endif

/* Import. */

#include "port_before.h"

#include <arpa/nameser.h>

#include "port_after.h"

/* Public. */

u_int16_t
ns_get16(const u_char *src) {
	u_int dst;

	NS_GET16(dst, src);
	return (dst);
}

u_int32_t
ns_get32(const u_char *src) {
	u_long dst;

	NS_GET32(dst, src);
	return (dst);
}

void
ns_put16(u_int16_t src, u_char *dst) {
	NS_PUT16(src, dst);
}

void
ns_put32(u_int32_t src, u_char *dst) {
	NS_PUT32(src, dst);
}

/*! \file */
