/*	$NetBSD: print_test.c,v 1.1.1.4 2015/07/08 15:38:05 christos Exp $	*/

/*
 * Copyright (C) 2014, 2015  Internet Systems Consortium, Inc. ("ISC")
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

#include <config.h>

#include <atf-c.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Workout if we need to force the inclusion of print.c so we can test
 * it on all platforms even if we don't include it in libisc.
 */
#include <isc/platform.h>
#if !defined(ISC_PLATFORM_NEEDVSNPRINTF) && !defined(ISC_PLATFORM_NEEDSPRINTF)
#define ISC__PRINT_SOURCE
#include "../print.c"
#else
#if !defined(ISC_PLATFORM_NEEDVSNPRINTF) || !defined(ISC_PLATFORM_NEEDSPRINTF)
#define ISC__PRINT_SOURCE
#endif
#include <isc/print.h>
#include <isc/types.h>
#include <isc/util.h>
#endif

ATF_TC(snprintf);
ATF_TC_HEAD(snprintf, tc) {
	atf_tc_set_md_var(tc, "descr", "snprintf implementation");
}
ATF_TC_BODY(snprintf, tc) {
	char buf[10000];
	isc_uint64_t ll = 8589934592ULL;
	int n;

	UNUSED(tc);

	/*
	 * 4294967296 <= 8589934592 < 1000000000^2 to verify fix for
	 * RT#36505.
	 */

	memset(buf, 0xff, sizeof(buf));
	n = isc_print_snprintf(buf, sizeof(buf), "%qu", ll);
	ATF_CHECK_EQ(n, 10);
	ATF_CHECK_STREQ(buf, "8589934592");

	memset(buf, 0xff, sizeof(buf));
	n = isc_print_snprintf(buf, sizeof(buf), "%llu", ll);
	ATF_CHECK_EQ(n, 10);
	ATF_CHECK_STREQ(buf, "8589934592");
}

/*
 * Main
 */
ATF_TP_ADD_TCS(tp) {
	ATF_TP_ADD_TC(tp, snprintf);
	return (atf_no_error());
}
