/*	$NetBSD: minires.h,v 1.1.1.2 2014/07/12 11:57:56 spz Exp $	*/
/*
 * Copyright (c) 2004,2007-2009 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 2001-2003 by Internet Software Consortium
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
 *
 *   Internet Systems Consortium, Inc.
 *   950 Charter Street
 *   Redwood City, CA 94063
 *   <info@isc.org>
 *   http://www.isc.org/
 */
#ifndef MINIRES_H
#define MINIRES_H

#include "cdefs.h"
#include "osdep.h"

/*
 * Based on the Dynamic DNS reference implementation by Viraj Bais
 * <viraj_bais@ccm.fm.intel.com>
 */

int MRns_name_compress(const char *, u_char *, size_t, const unsigned char **,
		       const unsigned char **);
int MRns_name_unpack(const unsigned char *, const unsigned char *,
		     const unsigned char *, unsigned char *, size_t);
int MRns_name_pack (const unsigned char *, unsigned char *,
		    unsigned, const unsigned char **, const unsigned char **);
int MRns_name_ntop(const unsigned char *, char *, size_t);
int MRns_name_pton(const char *, u_char *, size_t);

#endif /* MINIRES_H */
