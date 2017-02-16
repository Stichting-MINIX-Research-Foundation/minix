/*	$NetBSD: atomic.h,v 1.6 2014/12/10 04:38:00 christos Exp $	*/

/*
 * Copyright (C) 2005, 2007  Internet Systems Consortium, Inc. ("ISC")
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

/* Id: atomic.h,v 1.4 2007/06/19 23:47:18 tbox Exp  */

#ifndef ISC_ATOMIC_H
#define ISC_ATOMIC_H 1

#include <isc/platform.h>
#include <isc/types.h>

/* XXX: These are not atomic! */
/*
 * This routine atomically increments the value stored in 'p' by 'val', and
 * returns the previous value.
 */
static __inline isc_int32_t
isc_atomic_xadd(isc_int32_t *p, isc_int32_t val) {
	isc_int32_t prev = *p;

	*p += val;

	return prev;
}

/*
 * This routine atomically stores the value 'val' in 'p'.
 */
static __inline void
isc_atomic_store(isc_int32_t *p, isc_int32_t val) {
	*p = val;
}

/*
 * This routine atomically replaces the value in 'p' with 'val', if the
 * original value is equal to 'cmpval'.  The original value is returned in any
 * case.
 */
static __inline isc_int32_t
isc_atomic_cmpxchg(isc_int32_t *p, isc_int32_t cmpval, isc_int32_t val) {
	isc_int32_t prev = *p;

	if (*p == cmpval)
		*p = val;

	return prev;
}

#endif /* ISC_ATOMIC_H */
