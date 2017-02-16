/*	$NetBSD: driver.h,v 1.1.1.4 2014/12/10 03:34:28 christos Exp $	*/

/*
 * Copyright (C) 2011  Internet Systems Consortium, Inc. ("ISC")
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

/* Id: driver.h,v 1.4 2011/03/17 09:25:54 fdupont Exp  */

/*
 * This header includes the declarations of entry points.
 */

dlz_dlopen_version_t dlz_version;
dlz_dlopen_create_t dlz_create;
dlz_dlopen_destroy_t dlz_destroy;
dlz_dlopen_findzonedb_t dlz_findzonedb;
dlz_dlopen_lookup_t dlz_lookup;
dlz_dlopen_allowzonexfr_t dlz_allowzonexfr;
dlz_dlopen_allnodes_t dlz_allnodes;
dlz_dlopen_newversion_t dlz_newversion;
dlz_dlopen_closeversion_t dlz_closeversion;
dlz_dlopen_configure_t dlz_configure;
dlz_dlopen_ssumatch_t dlz_ssumatch;
dlz_dlopen_addrdataset_t dlz_addrdataset;
dlz_dlopen_subrdataset_t dlz_subrdataset;
dlz_dlopen_delrdataset_t dlz_delrdataset;
