/*	$NetBSD: json.h,v 1.1.1.3 2014/12/10 03:34:44 christos Exp $	*/

/*
 * Copyright (C) 2013  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef ISC_JSON_H
#define ISC_JSON_H 1

/*
 * This file is here mostly to make it easy to add additional libjson header
 * files as needed across all the users of this file.  Rather than place
 * these libjson includes in each file, one include makes it easy to handle
 * the ifdef as well as adding the ability to add additional functions
 * which may be useful.
 */

#ifdef HAVE_JSON
#include <json.h>
#endif

#define ISC_JSON_RENDERCONFIG		0x00000001 /* render config data */
#define ISC_JSON_RENDERSTATS		0x00000002 /* render stats */
#define ISC_JSON_RENDERALL		0x000000ff /* render everything */

#endif /* ISC_JSON_H */
