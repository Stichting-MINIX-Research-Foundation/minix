/*	$NetBSD: convert.h,v 1.1.1.3 2014/07/12 11:57:57 spz Exp $	*/
/* convert.h

   Safe copying of integers into and out of a non-aligned memory buffer. */

/*
 * Copyright (c) 2004,2009,2014 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996-2003 by Internet Software Consortium
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
 *   https://www.isc.org/
 *
 */

#ifndef OMAPI_CONVERT_H
#define OMAPI_CONVERT_H

u_int32_t getULong (const unsigned char *);
int32_t getLong (const unsigned char *);
u_int32_t getUShort (const unsigned char *);
int32_t getShort (const unsigned char *);
u_int32_t getUChar (const unsigned char *);
void putULong (unsigned char *, u_int32_t);
void putLong (unsigned char *, int32_t);
void putUShort (unsigned char *, u_int32_t);
void putShort (unsigned char *, int32_t);
void putUChar (unsigned char *, u_int32_t);
int converted_length (const unsigned char *, unsigned int, unsigned int);
int binary_to_ascii (unsigned char *, const unsigned char *,
		     unsigned int, unsigned int);

#endif /* OMAPI_CONVERT_H */
