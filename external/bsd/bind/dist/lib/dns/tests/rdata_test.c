/*	$NetBSD: rdata_test.c,v 1.4 2014/12/10 04:37:59 christos Exp $	*/

/*
 * Copyright (C) 2012, 2013  Internet Systems Consortium, Inc. ("ISC")
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

#include <isc/types.h>

#include <dns/rdata.h>

#include "dnstest.h"


/*
 * Individual unit tests
 */

/* Successful load test */
ATF_TC(hip);
ATF_TC_HEAD(hip, tc) {
	atf_tc_set_md_var(tc, "descr", "that a oversized HIP record will "
				       "be rejected");
}
ATF_TC_BODY(hip, tc) {
	unsigned char hipwire[DNS_RDATA_MAXLENGTH] = {
				    0x01, 0x00, 0x00, 0x01, 0x00, 0x00,
				    0x04, 0x41, 0x42, 0x43, 0x44, 0x00 };
	unsigned char buf[1024*1024];
	isc_buffer_t source, target;
	dns_rdata_t rdata;
	dns_decompress_t dctx;
	isc_result_t result;
	size_t i;

	UNUSED(tc);

	/*
	 * Fill the rest of input buffer with compression pointers.
	 */
	for (i = 12; i < sizeof(hipwire) - 2; i += 2) {
		hipwire[i] = 0xc0;
		hipwire[i+1] = 0x06;
	}

	isc_buffer_init(&source, hipwire, sizeof(hipwire));
	isc_buffer_add(&source, sizeof(hipwire));
	isc_buffer_setactive(&source, i);
	isc_buffer_init(&target, buf, sizeof(buf));
	dns_rdata_init(&rdata);
	dns_decompress_init(&dctx, -1, DNS_DECOMPRESS_ANY);
	result = dns_rdata_fromwire(&rdata, dns_rdataclass_in,
				    dns_rdatatype_hip, &source, &dctx,
				    0, &target);
	dns_decompress_invalidate(&dctx);
	ATF_REQUIRE_EQ(result, DNS_R_FORMERR);
}

ATF_TC(edns_client_subnet);
ATF_TC_HEAD(edns_client_subnet, tc) {
	atf_tc_set_md_var(tc, "descr",
			  "check EDNS client subnet option parsing");
}
ATF_TC_BODY(edns_client_subnet, tc) {
	struct {
		unsigned char data[64];
		size_t len;
		isc_boolean_t ok;
	} test_data[] = {
		{
			/* option code with no content */
			{ 0x00, 0x08, 0x0, 0x00 }, 4, ISC_FALSE
		},
		{
			/* Option code family 0, source 0, scope 0 */
			{
			  0x00, 0x08, 0x00, 0x04,
			  0x00, 0x00, 0x00, 0x00
			},
			8, ISC_TRUE
		},
		{
			/* Option code family 1 (ipv4), source 0, scope 0 */
			{
			  0x00, 0x08, 0x00, 0x04,
			  0x00, 0x01, 0x00, 0x00
			},
			8, ISC_TRUE
		},
		{
			/* Option code family 2 (ipv6) , source 0, scope 0 */
			{
			  0x00, 0x08, 0x00, 0x04,
			  0x00, 0x02, 0x00, 0x00
			},
			8, ISC_TRUE
		},
		{
			/* extra octet */
			{
			  0x00, 0x08, 0x00, 0x05,
			  0x00, 0x00, 0x00, 0x00,
			  0x00
			},
			9, ISC_FALSE
		},
		{
			/* source too long for IPv4 */
			{
			  0x00, 0x08, 0x00,    8,
			  0x00, 0x01,   33, 0x00,
			  0x00, 0x00, 0x00, 0x00
			},
			12, ISC_FALSE
		},
		{
			/* source too long for IPv6 */
			{
			  0x00, 0x08, 0x00,   20,
			  0x00, 0x02,  129, 0x00,
			  0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00,
			},
			24, ISC_FALSE
		},
		{
			/* scope too long for IPv4 */
			{
			  0x00, 0x08, 0x00,    8,
			  0x00, 0x01, 0x00,   33,
			  0x00, 0x00, 0x00, 0x00
			},
			12, ISC_FALSE
		},
		{
			/* scope too long for IPv6 */
			{
			  0x00, 0x08, 0x00,   20,
			  0x00, 0x02, 0x00,  129,
			  0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00,
			},
			24, ISC_FALSE
		},
		{
			/* length too short for source generic */
			{
			  0x00, 0x08, 0x00,    5,
			  0x00, 0x00,   17, 0x00,
			  0x00, 0x00,
			},
			19, ISC_FALSE
		},
		{
			/* length too short for source ipv4 */
			{
			  0x00, 0x08, 0x00,    7,
			  0x00, 0x01,   32, 0x00,
			  0x00, 0x00, 0x00, 0x00
			},
			11, ISC_FALSE
		},
		{
			/* length too short for source ipv6 */
			{
			  0x00, 0x08, 0x00,   19,
			  0x00, 0x02,  128, 0x00,
			  0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00,
			},
			23, ISC_FALSE
		},
		{
			/* sentinal */
			{ 0x00 }, 0, ISC_FALSE
		}
	};
	unsigned char buf[1024*1024];
	isc_buffer_t source, target;
	dns_rdata_t rdata;
	dns_decompress_t dctx;
	isc_result_t result;
	size_t i;

	UNUSED(tc);

	for (i = 0; test_data[i].len != 0; i++) {
		isc_buffer_init(&source, test_data[i].data, test_data[i].len);
		isc_buffer_add(&source, test_data[i].len);
		isc_buffer_setactive(&source, test_data[i].len);
		isc_buffer_init(&target, buf, sizeof(buf));
		dns_rdata_init(&rdata);
		dns_decompress_init(&dctx, -1, DNS_DECOMPRESS_ANY);
		result = dns_rdata_fromwire(&rdata, dns_rdataclass_in,
					    dns_rdatatype_opt, &source,
					    &dctx, 0, &target);
		dns_decompress_invalidate(&dctx);
		if (test_data[i].ok)
			ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
		else
			ATF_REQUIRE(result != ISC_R_SUCCESS);
	}
}

ATF_TC(wks);
ATF_TC_HEAD(wks, tc) {
	atf_tc_set_md_var(tc, "descr", "wks to/from struct");
}
ATF_TC_BODY(wks, tc) {
	struct {
		unsigned char data[64];
		size_t len;
		isc_boolean_t ok;
	} test_data[] = {
		{
			/* too short */
			{ 0x00, 0x08, 0x0, 0x00 }, 4, ISC_FALSE
		},
		{
			/* minimal TCP */
			{ 0x00, 0x08, 0x0, 0x00, 6 }, 5, ISC_TRUE
		},
		{
			/* minimal UDP */
			{ 0x00, 0x08, 0x0, 0x00, 17 }, 5, ISC_TRUE
		},
		{
			/* minimal other */
			{ 0x00, 0x08, 0x0, 0x00, 1 }, 5, ISC_TRUE
		},
		{
			/* sentinal */
			{ 0x00 }, 0, ISC_FALSE
		}
	};
	unsigned char buf1[1024];
	unsigned char buf2[1024];
	isc_buffer_t source, target1, target2;
	dns_rdata_t rdata;
	dns_decompress_t dctx;
	isc_result_t result;
	size_t i;
	dns_rdata_in_wks_t wks;

	UNUSED(tc);

	for (i = 0; test_data[i].len != 0; i++) {
		isc_buffer_init(&source, test_data[i].data, test_data[i].len);
		isc_buffer_add(&source, test_data[i].len);
		isc_buffer_setactive(&source, test_data[i].len);
		isc_buffer_init(&target1, buf1, sizeof(buf1));
		dns_rdata_init(&rdata);
		dns_decompress_init(&dctx, -1, DNS_DECOMPRESS_ANY);
		result = dns_rdata_fromwire(&rdata, dns_rdataclass_in,
					    dns_rdatatype_wks, &source,
					    &dctx, 0, &target1);
		dns_decompress_invalidate(&dctx);
		if (test_data[i].ok)
			ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
		else
			ATF_REQUIRE(result != ISC_R_SUCCESS);
		if (result != ISC_R_SUCCESS)
			continue;
		result = dns_rdata_tostruct(&rdata, &wks, NULL);
		ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
		isc_buffer_init(&target2, buf2, sizeof(buf2));
		dns_rdata_reset(&rdata);
		result = dns_rdata_fromstruct(&rdata, dns_rdataclass_in,
					      dns_rdatatype_wks, &wks,
					      &target2);
		ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
		ATF_REQUIRE_EQ(isc_buffer_usedlength(&target2),
						     test_data[i].len);
		ATF_REQUIRE_EQ(memcmp(buf2, test_data[i].data,
				      test_data[i].len), 0);
	}
}

ATF_TC(isdn);
ATF_TC_HEAD(isdn, tc) {
	atf_tc_set_md_var(tc, "descr", "isdn to/from struct");
}
ATF_TC_BODY(isdn, tc) {
	struct {
		unsigned char data[64];
		size_t len;
		isc_boolean_t ok;
	} test_data[] = {
		{
			/* "" */
			{ 0x00 }, 1, ISC_TRUE
		},
		{
			/* "\001" */
			{ 0x1, 0x01 }, 2, ISC_TRUE
		},
		{
			/* "\001" "" */
			{ 0x1, 0x01, 0x00 }, 3, ISC_TRUE
		},
		{
			/* "\000" "\001" */
			{ 0x1, 0x01, 0x01, 0x01 }, 4, ISC_TRUE
		},
		{
			/* sentinal */
			{ 0x00 }, 0, ISC_FALSE
		}
	};
	unsigned char buf1[1024];
	unsigned char buf2[1024];
	isc_buffer_t source, target1, target2;
	dns_rdata_t rdata;
	dns_decompress_t dctx;
	isc_result_t result;
	size_t i;
	dns_rdata_isdn_t isdn;

	UNUSED(tc);

	for (i = 0; test_data[i].len != 0; i++) {
		isc_buffer_init(&source, test_data[i].data, test_data[i].len);
		isc_buffer_add(&source, test_data[i].len);
		isc_buffer_setactive(&source, test_data[i].len);
		isc_buffer_init(&target1, buf1, sizeof(buf1));
		dns_rdata_init(&rdata);
		dns_decompress_init(&dctx, -1, DNS_DECOMPRESS_ANY);
		result = dns_rdata_fromwire(&rdata, dns_rdataclass_in,
					    dns_rdatatype_isdn, &source,
					    &dctx, 0, &target1);
		dns_decompress_invalidate(&dctx);
		if (test_data[i].ok)
			ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
		else
			ATF_REQUIRE(result != ISC_R_SUCCESS);
		if (result != ISC_R_SUCCESS)
			continue;
		result = dns_rdata_tostruct(&rdata, &isdn, NULL);
		ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
		isc_buffer_init(&target2, buf2, sizeof(buf2));
		dns_rdata_reset(&rdata);
		result = dns_rdata_fromstruct(&rdata, dns_rdataclass_in,
					      dns_rdatatype_isdn, &isdn,
					      &target2);
		ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
		ATF_REQUIRE_EQ(isc_buffer_usedlength(&target2),
						     test_data[i].len);
		ATF_REQUIRE_EQ(memcmp(buf2, test_data[i].data,
				      test_data[i].len), 0);
	}
}

/*
 * Main
 */
ATF_TP_ADD_TCS(tp) {
	ATF_TP_ADD_TC(tp, hip);
	ATF_TP_ADD_TC(tp, edns_client_subnet);
	ATF_TP_ADD_TC(tp, wks);
	ATF_TP_ADD_TC(tp, isdn);

	return (atf_no_error());
}

