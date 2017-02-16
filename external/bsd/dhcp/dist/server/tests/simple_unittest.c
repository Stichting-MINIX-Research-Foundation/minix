/*	$NetBSD: simple_unittest.c,v 1.1.1.2 2014/07/12 11:58:17 spz Exp $	*/
/*
 * Copyright (C) 2012 Internet Systems Consortium, Inc. ("ISC")
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

/* That is an example ATF test case, tailored to ISC DHCP sources.
   For detailed description with examples, see man 3 atf-c-api. */

/* this macro defines a name of a test case. Typical test case constists
   of an initial test declaration (ATF_TC()) followed by 3 phases:

   - Initialization: ATF_TC_HEAD()
   - Main body: ATF_TC_BODY()
   - Cleanup: ATF_TC_CLEANUP()

 In many cases initialization or cleanup are not needed. Use
 ATF_TC_WITHOUT_HEAD() or ATF_TC_WITH_CLEANUP() as needed. */
ATF_TC(simple_test_case);


ATF_TC_HEAD(simple_test_case, tc)
{
    atf_tc_set_md_var(tc, "descr", "This test case is a simple DHCP test.");
}
ATF_TC_BODY(simple_test_case, tc)
{
    int condition = 1;
    int this_is_linux = 1;
    /* Failing condition will fail the test, but the code
       itself will continue */
    ATF_CHECK( 2 > 1 );

    /* assert style check. Test will abort if the condition is not met. */
    ATF_REQUIRE( 5 > 4 );

    ATF_CHECK_EQ(4, 2 + 2); /* Non-fatal test. */
    ATF_REQUIRE_EQ(4, 2 + 2); /* Fatal test. */

    /* tests can also explicitly report test result */
    if (!condition) {
        atf_tc_fail("Condition not met!"); /* Explicit failure. */
    }

    if (!this_is_linux) {
        atf_tc_skip("Skipping test. This Linux-only test.");
    }

    if (condition && this_is_linux) {
        /* no extra comments for pass needed. It just passed. */
        atf_tc_pass();
    }

}

/* This macro defines main() method that will call specified
   test cases. tp and simple_test_case names can be whatever you want
   as long as it is a valid variable identifier. */
ATF_TP_ADD_TCS(tp)
{
    ATF_TP_ADD_TC(tp, simple_test_case);

    return (atf_no_error());
}
