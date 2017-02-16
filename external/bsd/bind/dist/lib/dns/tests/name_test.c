/*	$NetBSD: name_test.c,v 1.1.1.1 2015/07/08 15:38:04 christos Exp $	*/

/*
 * Copyright (C) 2014  Internet Systems Consortium, Inc. ("ISC")
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

#include <dns/name.h>
#include <dns/fixedname.h>
#include "dnstest.h"

/*
 * Individual unit tests
 */

ATF_TC(fullcompare);
ATF_TC_HEAD(fullcompare, tc) {
	atf_tc_set_md_var(tc, "descr", "dns_name_fullcompare test");
}
ATF_TC_BODY(fullcompare, tc) {
	dns_fixedname_t fixed1;
	dns_fixedname_t fixed2;
	dns_name_t *name1;
	dns_name_t *name2;
	dns_namereln_t relation;
	int i;
	isc_result_t result;
	struct {
		const char *name1;
		const char *name2;
		dns_namereln_t relation;
		int order;
		unsigned int nlabels;
	} data[] = {
		/* relative */
		{ "", "", dns_namereln_equal, 0, 0 },
		{ "foo", "", dns_namereln_subdomain, 1, 0 },
		{ "", "foo", dns_namereln_contains, -1, 0 },
		{ "foo", "bar", dns_namereln_none, 4, 0 },
		{ "bar", "foo", dns_namereln_none, -4, 0 },
		{ "bar.foo", "foo", dns_namereln_subdomain, 1, 1 },
		{ "foo", "bar.foo", dns_namereln_contains, -1, 1 },
		{ "baz.bar.foo", "bar.foo", dns_namereln_subdomain, 1, 2 },
		{ "bar.foo", "baz.bar.foo", dns_namereln_contains, -1, 2 },
		{ "foo.example", "bar.example", dns_namereln_commonancestor,
		  4, 1 },

		/* absolute */
		{ ".", ".", dns_namereln_equal, 0, 1 },
		{ "foo.", "bar.", dns_namereln_commonancestor, 4, 1 },
		{ "bar.", "foo.", dns_namereln_commonancestor, -4, 1 },
		{ "foo.example.", "bar.example.", dns_namereln_commonancestor,
		  4, 2 },
		{ "bar.foo.", "foo.", dns_namereln_subdomain, 1, 2 },
		{ "foo.", "bar.foo.", dns_namereln_contains, -1, 2 },
		{ "baz.bar.foo.", "bar.foo.", dns_namereln_subdomain, 1, 3 },
		{ "bar.foo.", "baz.bar.foo.", dns_namereln_contains, -1, 3 },
		{ NULL, NULL, dns_namereln_none, 0, 0 }
	};

	UNUSED(tc);

	dns_fixedname_init(&fixed1);
	name1 = dns_fixedname_name(&fixed1);
	dns_fixedname_init(&fixed2);
	name2 = dns_fixedname_name(&fixed2);
	for (i = 0; data[i].name1 != NULL; i++) {
		int order = 3000;
		unsigned int nlabels = 3000;

		if (data[i].name1[0] == 0) {
			dns_fixedname_init(&fixed1);
		} else {
			result = dns_name_fromstring2(name1, data[i].name1,
						      NULL, 0, NULL);
			ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
		}
		if (data[i].name2[0] == 0) {
			dns_fixedname_init(&fixed2);
		} else {
			result = dns_name_fromstring2(name2, data[i].name2,
						      NULL, 0, NULL);
			ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
		}
		relation = dns_name_fullcompare(name1, name1, &order, &nlabels);
		ATF_REQUIRE_EQ(relation, dns_namereln_equal);
		ATF_REQUIRE_EQ(order, 0);
		ATF_REQUIRE_EQ(nlabels, name1->labels);

		/* Some random initializer */
		order = 3001;
		nlabels = 3001;

		relation = dns_name_fullcompare(name1, name2, &order, &nlabels);
		ATF_REQUIRE_EQ(relation, data[i].relation);
		ATF_REQUIRE_EQ(order, data[i].order);
		ATF_REQUIRE_EQ(nlabels, data[i].nlabels);
	}
}

/*
 * Main
 */
ATF_TP_ADD_TCS(tp) {
	ATF_TP_ADD_TC(tp, fullcompare);

	return (atf_no_error());
}

