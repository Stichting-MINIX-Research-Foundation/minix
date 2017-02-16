/*	$NetBSD: atomic.h,v 1.3 2014/12/10 04:38:01 christos Exp $	*/

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

/* Id */

#ifndef ISC_ATOMIC_H
#define ISC_ATOMIC_H 1

#include <config.h>
#include <isc/platform.h>
#include <isc/types.h>

/*
 * This routine atomically increments the value stored in 'p' by 'val', and
 * returns the previous value.
 */
#ifdef ISC_PLATFORM_HAVEXADD
static __inline isc_int32_t
isc_atomic_xadd(isc_int32_t *p, isc_int32_t val) {
	return (isc_int32_t) _InterlockedExchangeAdd((long *)p, (long)val);
}
#endif

#ifdef ISC_PLATFORM_HAVEXADDQ
static __inline isc_int64_t
isc_atomic_xaddq(isc_int64_t *p, isc_int64_t val) {
	return (isc_int64_t) _InterlockedExchangeAdd64((__int64 *)p,
						       (__int64) val);
}
#endif

/*
 * This routine atomically stores the value 'val' in 'p'.
 */
#ifdef ISC_PLATFORM_HAVEATOMICSTORE
static __inline void
isc_atomic_store(isc_int32_t *p, isc_int32_t val) {
	(void) _InterlockedExchange((long *)p, (long)val);
}
#endif

/*
 * This routine atomically replaces the value in 'p' with 'val', if the
 * original value is equal to 'cmpval'.  The original value is returned in any
 * case.
 */
#ifdef ISC_PLATFORM_HAVECMPXCHG
static __inline isc_int32_t
isc_atomic_cmpxchg(isc_int32_t *p, isc_int32_t cmpval, isc_int32_t val) {
	/* beware: swap arguments */
	return (isc_int32_t) _InterlockedCompareExchange((long *)p,
							 (long)val,
							 (long)cmpval);
}
#endif

#endif /* ISC_ATOMIC_H */
