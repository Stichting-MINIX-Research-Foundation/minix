/*	$NetBSD: nsec3_test.c,v 1.1.1.4 2014/12/10 03:34:43 christos Exp $	*/

/*
 * Copyright (C) 2012, 2014  Internet Systems Consortium, Inc. ("ISC")
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

/*! \file */

#include <config.h>

#include <atf-c.h>

#include <unistd.h>

#include <dns/db.h>
#include <dns/nsec3.h>

#include "dnstest.h"

#if defined(OPENSSL) || defined(PKCS11CRYPTO)
/*
 * Helper functions
 */

static void
iteration_test(const char* file, unsigned int expected) {
	isc_result_t result;
	dns_db_t *db = NULL;
	unsigned int iterations;

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_test_loaddb(&db, dns_dbtype_zone, "test", file);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_nsec3_maxiterations(db, NULL, mctx, &iterations);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	ATF_REQUIRE_EQ(iterations, expected);

	dns_db_detach(&db);

	dns_test_end();
}

/*
 * Individual unit tests
 */

ATF_TC(max_iterations);
ATF_TC_HEAD(max_iterations, tc) {
	atf_tc_set_md_var(tc, "descr", "check that appropriate max iterations "
			  " is returned for different key size mixes");
}
ATF_TC_BODY(max_iterations, tc) {

	UNUSED(tc);

	iteration_test("testdata/nsec3/1024.db", 150);
	iteration_test("testdata/nsec3/2048.db", 500);
	iteration_test("testdata/nsec3/4096.db", 2500);
	iteration_test("testdata/nsec3/min-1024.db", 150);
	iteration_test("testdata/nsec3/min-2048.db", 500);
}
#else
ATF_TC(untested);
ATF_TC_HEAD(untested, tc) {
	atf_tc_set_md_var(tc, "descr", "skipping nsec3 test");
}
ATF_TC_BODY(untested, tc) {
	UNUSED(tc);
	atf_tc_skip("DNSSEC not available");
}
#endif

/*
 * Main
 */
ATF_TP_ADD_TCS(tp) {
#if defined(OPENSSL) || defined(PKCS11CRYPTO)
	ATF_TP_ADD_TC(tp, max_iterations);
#else
	ATF_TP_ADD_TC(tp, untested);
#endif

	return (atf_no_error());
}

