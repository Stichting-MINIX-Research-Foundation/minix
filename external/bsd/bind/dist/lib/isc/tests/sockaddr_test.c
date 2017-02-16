/*	$NetBSD: sockaddr_test.c,v 1.1.1.3 2014/12/10 03:34:44 christos Exp $	*/

/*
 * Copyright (C) 2012  Internet Systems Consortium, Inc. ("ISC")
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

#include <isc/sockaddr.h>
#include <isc/print.h>

#include "isctest.h"

/*
 * Individual unit tests
 */

ATF_TC(sockaddr_hash);
ATF_TC_HEAD(sockaddr_hash, tc) {
	atf_tc_set_md_var(tc, "descr", "sockaddr hash");
}
ATF_TC_BODY(sockaddr_hash, tc) {
	isc_result_t result;
	isc_sockaddr_t addr;
	struct in_addr in;
	struct in6_addr in6;
	unsigned int h1, h2, h3, h4;
	int ret;

	UNUSED(tc);

	result = isc_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	in.s_addr = inet_addr("127.0.0.1");
	isc_sockaddr_fromin(&addr, &in, 1);
	h1 = isc_sockaddr_hash(&addr, ISC_TRUE);
	h2 = isc_sockaddr_hash(&addr, ISC_FALSE);
	ATF_CHECK(h1 != h2);

	ret = inet_pton(AF_INET6, "::ffff:127.0.0.1", &in6);
	ATF_CHECK(ret == 1);
	isc_sockaddr_fromin6(&addr, &in6, 1);
	h3 = isc_sockaddr_hash(&addr, ISC_TRUE);
	h4 = isc_sockaddr_hash(&addr, ISC_FALSE);
	ATF_CHECK(h1 == h3);
	ATF_CHECK(h2 == h4);

	isc_test_end();
}

/*
 * Main
 */
ATF_TP_ADD_TCS(tp) {
	ATF_TP_ADD_TC(tp, sockaddr_hash);

	return (atf_no_error());
}

