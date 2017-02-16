/*	$NetBSD: mdb6_unittest.c,v 1.1.1.2 2014/07/12 11:58:16 spz Exp $	*/
/*
 * Copyright (C) 2007-2012 by Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and distribute this software for any
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

#include "config.h"

#include <sys/types.h>
#include <time.h>
#include <netinet/in.h>

#include <stdarg.h>
#include "dhcpd.h"
#include "omapip/omapip.h"
#include "omapip/hash.h"
#include <isc/md5.h>

#include <atf-c.h>

#include <stdlib.h>

void build_prefix6(struct in6_addr *pref, const struct in6_addr *net_start_pref,
                   int pool_bits, int pref_bits,
                   const struct data_string *input);

/*
 * Basic iaaddr manipulation.
 * Verify construction and referencing of an iaaddr.
 */

ATF_TC(iaaddr_basic);
ATF_TC_HEAD(iaaddr_basic, tc)
{
    atf_tc_set_md_var(tc, "descr", "This test case checks that basic "
                      "IAADDR manipulation is possible.");
}
ATF_TC_BODY(iaaddr_basic, tc)
{
    struct iasubopt *iaaddr;
    struct iasubopt *iaaddr_copy;

    /* set up dhcp globals */
    dhcp_context_create(DHCP_CONTEXT_PRE_DB | DHCP_CONTEXT_POST_DB,
			NULL, NULL);

    /* and other common arguments */
    iaaddr = NULL;
    iaaddr_copy = NULL;

    /* tests */
    if (iasubopt_allocate(&iaaddr, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: iasubopt_allocate() %s:%d", MDL);
    }
    if (iaaddr->state != FTS_FREE) {
        atf_tc_fail("ERROR: bad state %s:%d", MDL);
    }
    if (iaaddr->heap_index != -1) {
        atf_tc_fail("ERROR: bad heap_index %s:%d", MDL);
    }
    if (iasubopt_reference(&iaaddr_copy, iaaddr, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: iasubopt_reference() %s:%d", MDL);
    }
    if (iasubopt_dereference(&iaaddr, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: iasubopt_reference() %s:%d", MDL);
    }
    if (iasubopt_dereference(&iaaddr_copy, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: iasubopt_reference() %s:%d", MDL);
    }
}

/*
 * Basic iaaddr sanity checks.
 * Verify that the iaaddr code does some sanity checking.
 */

ATF_TC(iaaddr_negative);
ATF_TC_HEAD(iaaddr_negative, tc)
{
    atf_tc_set_md_var(tc, "descr", "This test case checks that IAADDR "
                      "option code can handle various negative scenarios.");
}
ATF_TC_BODY(iaaddr_negative, tc)
{
    struct iasubopt *iaaddr;
    struct iasubopt *iaaddr_copy;

    /* set up dhcp globals */
    dhcp_context_create(DHCP_CONTEXT_PRE_DB | DHCP_CONTEXT_POST_DB,
			NULL, NULL);

    /* tests */
    /* bogus allocate arguments */
    if (iasubopt_allocate(NULL, MDL) != DHCP_R_INVALIDARG) {
        atf_tc_fail("ERROR: iasubopt_allocate() %s:%d", MDL);
    }
    iaaddr = (struct iasubopt *)1;
    if (iasubopt_allocate(&iaaddr, MDL) != DHCP_R_INVALIDARG) {
        atf_tc_fail("ERROR: iasubopt_allocate() %s:%d", MDL);
    }

    /* bogus reference arguments */
    iaaddr = NULL;
    if (iasubopt_allocate(&iaaddr, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: iasubopt_allocate() %s:%d", MDL);
    }
    if (iasubopt_reference(NULL, iaaddr, MDL) != DHCP_R_INVALIDARG) {
        atf_tc_fail("ERROR: iasubopt_reference() %s:%d", MDL);
    }
    iaaddr_copy = (struct iasubopt *)1;
    if (iasubopt_reference(&iaaddr_copy, iaaddr,
                           MDL) != DHCP_R_INVALIDARG) {
        atf_tc_fail("ERROR: iasubopt_reference() %s:%d", MDL);
    }
    iaaddr_copy = NULL;
    if (iasubopt_reference(&iaaddr_copy, NULL, MDL) != DHCP_R_INVALIDARG) {
        atf_tc_fail("ERROR: iasubopt_reference() %s:%d", MDL);
    }
    if (iasubopt_dereference(&iaaddr, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: iasubopt_reference() %s:%d", MDL);
    }

    /* bogus dereference arguments */
    if (iasubopt_dereference(NULL, MDL) != DHCP_R_INVALIDARG) {
        atf_tc_fail("ERROR: iasubopt_dereference() %s:%d", MDL);
    }
    iaaddr = NULL;
    if (iasubopt_dereference(&iaaddr, MDL) != DHCP_R_INVALIDARG) {
        atf_tc_fail("ERROR: iasubopt_dereference() %s:%d", MDL);
    }
}

/*
 * Basic ia_na manipulation.
 */

ATF_TC(ia_na_basic);
ATF_TC_HEAD(ia_na_basic, tc)
{
    atf_tc_set_md_var(tc, "descr", "This test case checks that IA_NA code can "
                      "handle various basic scenarios.");
}
ATF_TC_BODY(ia_na_basic, tc)
{
    uint32_t iaid;
    struct ia_xx *ia_na;
    struct ia_xx *ia_na_copy;
    struct iasubopt *iaaddr;

    /* set up dhcp globals */
    dhcp_context_create(DHCP_CONTEXT_PRE_DB | DHCP_CONTEXT_POST_DB,
			NULL, NULL);

    /* and other common arguments */
    iaid = 666;
    ia_na = NULL;
    ia_na_copy = NULL;
    iaaddr = NULL;

    /* tests */
    if (ia_allocate(&ia_na, iaid, "TestDUID", 8, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: ia_allocate() %s:%d", MDL);
    }
    if (memcmp(ia_na->iaid_duid.data, &iaid, sizeof(iaid)) != 0) {
        atf_tc_fail("ERROR: bad IAID_DUID %s:%d", MDL);
    }
    if (memcmp(ia_na->iaid_duid.data+sizeof(iaid), "TestDUID", 8) != 0) {
        atf_tc_fail("ERROR: bad IAID_DUID %s:%d", MDL);
    }
    if (ia_na->num_iasubopt != 0) {
        atf_tc_fail("ERROR: bad num_iasubopt %s:%d", MDL);
    }
    if (ia_reference(&ia_na_copy, ia_na, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: ia_reference() %s:%d", MDL);
    }
    if (iasubopt_allocate(&iaaddr, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: iasubopt_allocate() %s:%d", MDL);
    }
    if (ia_add_iasubopt(ia_na, iaaddr, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: ia_add_iasubopt() %s:%d", MDL);
    }
    ia_remove_iasubopt(ia_na, iaaddr, MDL);
    if (iasubopt_dereference(&iaaddr, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: iasubopt_reference() %s:%d", MDL);
    }
    if (ia_dereference(&ia_na, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: ia_dereference() %s:%d", MDL);
    }
    if (ia_dereference(&ia_na_copy, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: ia_dereference() %s:%d", MDL);
    }
}

/*
 * Lots of iaaddr in our ia_na.
 * Create many iaaddrs and attach them to an ia_na
 * then clean up by removing them one at a time and
 * all at once by dereferencing the ia_na.
 */

ATF_TC(ia_na_manyaddrs);
ATF_TC_HEAD(ia_na_manyaddrs, tc)
{
    atf_tc_set_md_var(tc, "descr", "This test case checks that IA_NA can "
                      "handle lots of addresses.");
}
ATF_TC_BODY(ia_na_manyaddrs, tc)
{
    uint32_t iaid;
    struct ia_xx *ia_na;
    struct iasubopt *iaaddr;
    int i;

    /* set up dhcp globals */
    dhcp_context_create(DHCP_CONTEXT_PRE_DB | DHCP_CONTEXT_POST_DB,
			NULL, NULL);

    /* tests */
    /* lots of iaaddr that we delete */
    iaid = 666;
    ia_na = NULL;
    if (ia_allocate(&ia_na, iaid, "TestDUID", 8, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: ia_allocate() %s:%d", MDL);
    }
    for (i=0; i<100; i++) {
        iaaddr = NULL;
        if (iasubopt_allocate(&iaaddr, MDL) != ISC_R_SUCCESS) {
            atf_tc_fail("ERROR: iasubopt_allocate() %s:%d", MDL);
        }
        if (ia_add_iasubopt(ia_na, iaaddr, MDL) != ISC_R_SUCCESS) {
            atf_tc_fail("ERROR: ia_add_iasubopt() %s:%d", MDL);
        }
        if (iasubopt_dereference(&iaaddr, MDL) != ISC_R_SUCCESS) {
            atf_tc_fail("ERROR: iasubopt_reference() %s:%d", MDL);
        }
    }

#if 0
    for (i=0; i<100; i++) {
        iaaddr = ia_na->iasubopt[random() % ia_na->num_iasubopt];
        ia_remove_iasubopt(ia_na, iaaddr, MDL);
        /* TODO: valgrind reports problem here: Invalid read of size 8
         * Address 0x51e6258 is 56 bytes inside a block of size 88 free'd */
    }
#endif
    if (ia_dereference(&ia_na, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: ia_dereference() %s:%d", MDL);
    }

    /* lots of iaaddr, let dereference cleanup */
    iaid = 666;
    ia_na = NULL;
    if (ia_allocate(&ia_na, iaid, "TestDUID", 8, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: ia_allocate() %s:%d", MDL);
    }
    for (i=0; i<100; i++) {
        iaaddr = NULL;
        if (iasubopt_allocate(&iaaddr, MDL) != ISC_R_SUCCESS) {
            atf_tc_fail("ERROR: iasubopt_allocate() %s:%d", MDL);
        }
        if (ia_add_iasubopt(ia_na, iaaddr, MDL) != ISC_R_SUCCESS) {
            atf_tc_fail("ERROR: ia_add_iasubopt() %s:%d", MDL);
        }
        if (iasubopt_dereference(&iaaddr, MDL) != ISC_R_SUCCESS) {
            atf_tc_fail("ERROR: iasubopt_reference() %s:%d", MDL);
        }
    }
    if (ia_dereference(&ia_na, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: ia_dereference() %s:%d", MDL);
    }
}

/*
 * Basic ia_na sanity checks.
 * Verify that the ia_na code does some sanity checking.
 */

ATF_TC(ia_na_negative);
ATF_TC_HEAD(ia_na_negative, tc)
{
    atf_tc_set_md_var(tc, "descr", "This test case checks that IA_NA option "
                      "code can handle various negative scenarios.");
}
ATF_TC_BODY(ia_na_negative, tc)
{
    uint32_t iaid;
    struct ia_xx *ia_na;
    struct ia_xx *ia_na_copy;

    /* set up dhcp globals */
    dhcp_context_create(DHCP_CONTEXT_PRE_DB | DHCP_CONTEXT_POST_DB,
			NULL, NULL);

    /* tests */
    /* bogus allocate arguments */
    if (ia_allocate(NULL, 123, "", 0, MDL) != DHCP_R_INVALIDARG) {
        atf_tc_fail("ERROR: ia_allocate() %s:%d", MDL);
    }
    ia_na = (struct ia_xx *)1;
    if (ia_allocate(&ia_na, 456, "", 0, MDL) != DHCP_R_INVALIDARG) {
        atf_tc_fail("ERROR: ia_allocate() %s:%d", MDL);
    }

    /* bogus reference arguments */
    iaid = 666;
    ia_na = NULL;
    if (ia_allocate(&ia_na, iaid, "TestDUID", 8, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: ia_allocate() %s:%d", MDL);
    }
    if (ia_reference(NULL, ia_na, MDL) != DHCP_R_INVALIDARG) {
        atf_tc_fail("ERROR: ia_reference() %s:%d", MDL);
    }
    ia_na_copy = (struct ia_xx *)1;
    if (ia_reference(&ia_na_copy, ia_na, MDL) != DHCP_R_INVALIDARG) {
        atf_tc_fail("ERROR: ia_reference() %s:%d", MDL);
    }
    ia_na_copy = NULL;
    if (ia_reference(&ia_na_copy, NULL, MDL) != DHCP_R_INVALIDARG) {
        atf_tc_fail("ERROR: ia_reference() %s:%d", MDL);
    }
    if (ia_dereference(&ia_na, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: ia_dereference() %s:%d", MDL);
    }

    /* bogus dereference arguments */
    if (ia_dereference(NULL, MDL) != DHCP_R_INVALIDARG) {
        atf_tc_fail("ERROR: ia_dereference() %s:%d", MDL);
    }

    /* bogus remove */
    iaid = 666;
    ia_na = NULL;
    if (ia_allocate(&ia_na, iaid, "TestDUID", 8, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: ia_allocate() %s:%d", MDL);
    }
    ia_remove_iasubopt(ia_na, NULL, MDL);
    if (ia_dereference(&ia_na, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: ia_dereference() %s:%d", MDL);
    }
}

/*
 * Basic ipv6_pool manipulation.
 * Verify that basic pool operations work properly.
 * The operations include creating a pool and creating,
 * renewing, expiring, releasing and declining addresses.
 */

ATF_TC(ipv6_pool_basic);
ATF_TC_HEAD(ipv6_pool_basic, tc)
{
    atf_tc_set_md_var(tc, "descr", "This test case checks that IPv6 pool "
                      "manipulation is possible.");
}
ATF_TC_BODY(ipv6_pool_basic, tc)
{
    struct iasubopt *iaaddr;
    struct in6_addr addr;
    struct ipv6_pool *pool;
    struct ipv6_pool *pool_copy;
    char addr_buf[INET6_ADDRSTRLEN];
    char *uid;
    struct data_string ds;
    struct iasubopt *expired_iaaddr;
    unsigned int attempts;

    /* set up dhcp globals */
    dhcp_context_create(DHCP_CONTEXT_PRE_DB | DHCP_CONTEXT_POST_DB,
			NULL, NULL);

    /* and other common arguments */
    inet_pton(AF_INET6, "1:2:3:4::", &addr);

    uid = "client0";
    memset(&ds, 0, sizeof(ds));
    ds.len = strlen(uid);
    if (!buffer_allocate(&ds.buffer, ds.len, MDL)) {
        atf_tc_fail("Out of memory");
    }
    ds.data = ds.buffer->data;
    memcpy((char *)ds.data, uid, ds.len);

    /* tests */
    /* allocate, reference */
    pool = NULL;
    if (ipv6_pool_allocate(&pool, D6O_IA_NA, &addr,
                           64, 128, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: ipv6_pool_allocate() %s:%d", MDL);
    }
    if (pool->num_active != 0) {
        atf_tc_fail("ERROR: bad num_active %s:%d", MDL);
    }
    if (pool->bits != 64) {
        atf_tc_fail("ERROR: bad bits %s:%d", MDL);
    }
    inet_ntop(AF_INET6, &pool->start_addr, addr_buf, sizeof(addr_buf));
    if (strcmp(inet_ntop(AF_INET6, &pool->start_addr, addr_buf,
                         sizeof(addr_buf)), "1:2:3:4::") != 0) {
        atf_tc_fail("ERROR: bad start_addr %s:%d", MDL);
    }
    pool_copy = NULL;
    if (ipv6_pool_reference(&pool_copy, pool, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: ipv6_pool_reference() %s:%d", MDL);
    }

    /* create_lease6, renew_lease6, expire_lease6 */
    iaaddr = NULL;
    if (create_lease6(pool, &iaaddr,
                      &attempts, &ds, 1) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: create_lease6() %s:%d", MDL);
    }
    if (pool->num_inactive != 1) {
        atf_tc_fail("ERROR: bad num_inactive %s:%d", MDL);
    }
    if (renew_lease6(pool, iaaddr) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: renew_lease6() %s:%d", MDL);
    }
    if (pool->num_active != 1) {
        atf_tc_fail("ERROR: bad num_active %s:%d", MDL);
    }
    expired_iaaddr = NULL;
    if (expire_lease6(&expired_iaaddr, pool, 0) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: expire_lease6() %s:%d", MDL);
    }
    if (expired_iaaddr != NULL) {
        atf_tc_fail("ERROR: should not have expired a lease %s:%d", MDL);
    }
    if (pool->num_active != 1) {
        atf_tc_fail("ERROR: bad num_active %s:%d", MDL);
    }
    if (expire_lease6(&expired_iaaddr, pool, 1000) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: expire_lease6() %s:%d", MDL);
    }
    if (expired_iaaddr == NULL) {
        atf_tc_fail("ERROR: should have expired a lease %s:%d", MDL);
    }
    if (iasubopt_dereference(&expired_iaaddr, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: iasubopt_dereference() %s:%d", MDL);
    }
    if (pool->num_active != 0) {
        atf_tc_fail("ERROR: bad num_active %s:%d", MDL);
    }
    if (iasubopt_dereference(&iaaddr, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: iasubopt_dereference() %s:%d", MDL);
    }

    /* release_lease6, decline_lease6 */
    if (create_lease6(pool, &iaaddr, &attempts,
              &ds, 1) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: create_lease6() %s:%d", MDL);
    }
    if (renew_lease6(pool, iaaddr) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: renew_lease6() %s:%d", MDL);
    }
    if (pool->num_active != 1) {
        atf_tc_fail("ERROR: bad num_active %s:%d", MDL);
    }
    if (release_lease6(pool, iaaddr) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: decline_lease6() %s:%d", MDL);
    }
    if (pool->num_active != 0) {
        atf_tc_fail("ERROR: bad num_active %s:%d", MDL);
    }
    if (iasubopt_dereference(&iaaddr, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: iasubopt_dereference() %s:%d", MDL);
    }
    if (create_lease6(pool, &iaaddr, &attempts,
              &ds, 1) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: create_lease6() %s:%d", MDL);
    }
    if (renew_lease6(pool, iaaddr) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: renew_lease6() %s:%d", MDL);
    }
    if (pool->num_active != 1) {
        atf_tc_fail("ERROR: bad num_active %s:%d", MDL);
    }
    if (decline_lease6(pool, iaaddr) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: decline_lease6() %s:%d", MDL);
    }
    if (pool->num_active != 1) {
        atf_tc_fail("ERROR: bad num_active %s:%d", MDL);
    }
    if (iasubopt_dereference(&iaaddr, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: iasubopt_dereference() %s:%d", MDL);
    }

    /* dereference */
    if (ipv6_pool_dereference(&pool, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: ipv6_pool_reference() %s:%d", MDL);
    }
    if (ipv6_pool_dereference(&pool_copy, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: ipv6_pool_reference() %s:%d", MDL);
    }
}

/*
 * Basic ipv6_pool sanity checks.
 * Verify that the ipv6_pool code does some sanity checking.
 */

ATF_TC(ipv6_pool_negative);
ATF_TC_HEAD(ipv6_pool_negative, tc)
{
    atf_tc_set_md_var(tc, "descr", "This test case checks that IPv6 pool "
                      "can handle negative cases.");
}
ATF_TC_BODY(ipv6_pool_negative, tc)
{
    struct in6_addr addr;
    struct ipv6_pool *pool;
    struct ipv6_pool *pool_copy;

    /* set up dhcp globals */
    dhcp_context_create(DHCP_CONTEXT_PRE_DB | DHCP_CONTEXT_POST_DB,
			NULL, NULL);

    /* and other common arguments */
    inet_pton(AF_INET6, "1:2:3:4::", &addr);

    /* tests */
    if (ipv6_pool_allocate(NULL, D6O_IA_NA, &addr,
                           64, 128, MDL) != DHCP_R_INVALIDARG) {
        atf_tc_fail("ERROR: ipv6_pool_allocate() %s:%d", MDL);
    }
    pool = (struct ipv6_pool *)1;
    if (ipv6_pool_allocate(&pool, D6O_IA_NA, &addr,
                           64, 128, MDL) != DHCP_R_INVALIDARG) {
        atf_tc_fail("ERROR: ipv6_pool_allocate() %s:%d", MDL);
    }
    if (ipv6_pool_reference(NULL, pool, MDL) != DHCP_R_INVALIDARG) {
        atf_tc_fail("ERROR: ipv6_pool_reference() %s:%d", MDL);
    }
    pool_copy = (struct ipv6_pool *)1;
    if (ipv6_pool_reference(&pool_copy, pool, MDL) != DHCP_R_INVALIDARG) {
        atf_tc_fail("ERROR: ipv6_pool_reference() %s:%d", MDL);
    }
    pool_copy = NULL;
    if (ipv6_pool_reference(&pool_copy, NULL, MDL) != DHCP_R_INVALIDARG) {
        atf_tc_fail("ERROR: ipv6_pool_reference() %s:%d", MDL);
    }
    if (ipv6_pool_dereference(NULL, MDL) != DHCP_R_INVALIDARG) {
        atf_tc_fail("ERROR: ipv6_pool_dereference() %s:%d", MDL);
    }
    if (ipv6_pool_dereference(&pool_copy, MDL) != DHCP_R_INVALIDARG) {
        atf_tc_fail("ERROR: ipv6_pool_dereference() %s:%d", MDL);
    }
}


/*
 * Order of expiration.
 * Add several addresses to a pool and check that
 * they expire in the proper order.
 */

ATF_TC(expire_order);
ATF_TC_HEAD(expire_order, tc)
{
    atf_tc_set_md_var(tc, "descr", "This test case checks that order "
                      "of lease expiration is handled properly.");
}
ATF_TC_BODY(expire_order, tc)
{
    struct iasubopt *iaaddr;
    struct ipv6_pool *pool;
    struct in6_addr addr;
        int i;
    char *uid;
    struct data_string ds;
    struct iasubopt *expired_iaaddr;
    unsigned int attempts;

    /* set up dhcp globals */
    dhcp_context_create(DHCP_CONTEXT_PRE_DB | DHCP_CONTEXT_POST_DB,
			NULL, NULL);

    /* and other common arguments */
    inet_pton(AF_INET6, "1:2:3:4::", &addr);

    uid = "client0";
    memset(&ds, 0, sizeof(ds));
    ds.len = strlen(uid);
    if (!buffer_allocate(&ds.buffer, ds.len, MDL)) {
        atf_tc_fail("Out of memory");
    }
    ds.data = ds.buffer->data;
    memcpy((char *)ds.data, uid, ds.len);

    iaaddr = NULL;
    expired_iaaddr = NULL;

    /* tests */
    pool = NULL;
    if (ipv6_pool_allocate(&pool, D6O_IA_NA, &addr,
                           64, 128, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: ipv6_pool_allocate() %s:%d", MDL);
    }

    for (i=10; i<100; i+=10) {
        if (create_lease6(pool, &iaaddr, &attempts,
                  &ds, i) != ISC_R_SUCCESS) {
            atf_tc_fail("ERROR: create_lease6() %s:%d", MDL);
                }
        if (renew_lease6(pool, iaaddr) != ISC_R_SUCCESS) {
            atf_tc_fail("ERROR: renew_lease6() %s:%d", MDL);
                }
        if (iasubopt_dereference(&iaaddr, MDL) != ISC_R_SUCCESS) {
            atf_tc_fail("ERROR: iasubopt_dereference() %s:%d", MDL);
                }
        if (pool->num_active != (i / 10)) {
            atf_tc_fail("ERROR: bad num_active %s:%d", MDL);
                }
    }
    if (pool->num_active != 9) {
        atf_tc_fail("ERROR: bad num_active %s:%d", MDL);
    }

    for (i=10; i<100; i+=10) {
        if (expire_lease6(&expired_iaaddr,
                  pool, 1000) != ISC_R_SUCCESS) {
            atf_tc_fail("ERROR: expire_lease6() %s:%d", MDL);
                }
        if (expired_iaaddr == NULL) {
            atf_tc_fail("ERROR: should have expired a lease %s:%d",
                   MDL);
                }
        if (pool->num_active != (9 - (i / 10))) {
            atf_tc_fail("ERROR: bad num_active %s:%d", MDL);
                }
        if (expired_iaaddr->hard_lifetime_end_time != i) {
            atf_tc_fail("ERROR: bad hard_lifetime_end_time %s:%d",
                   MDL);
                }
        if (iasubopt_dereference(&expired_iaaddr, MDL) !=
                ISC_R_SUCCESS) {
            atf_tc_fail("ERROR: iasubopt_dereference() %s:%d", MDL);
                }
    }
    if (pool->num_active != 0) {
        atf_tc_fail("ERROR: bad num_active %s:%d", MDL);
    }
    expired_iaaddr = NULL;
    if (expire_lease6(&expired_iaaddr, pool, 1000) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: expire_lease6() %s:%d", MDL);
    }
    if (ipv6_pool_dereference(&pool, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: ipv6_pool_dereference() %s:%d", MDL);
    }
}

/*
 * Reduce the expiration period of a lease.
 * This test reduces the expiration period of
 * a lease to verify we process reductions
 * properly.
 */
ATF_TC(expire_order_reduce);
ATF_TC_HEAD(expire_order_reduce, tc)
{
    atf_tc_set_md_var(tc, "descr", "This test case checks that reducing "
                      "the expiration time of a lease works properly.");
}
ATF_TC_BODY(expire_order_reduce, tc)
{
    struct iasubopt *iaaddr1, *iaaddr2;
    struct ipv6_pool *pool;
    struct in6_addr addr;
    char *uid;
    struct data_string ds;
    struct iasubopt *expired_iaaddr;
    unsigned int attempts;

    /* set up dhcp globals */
    dhcp_context_create(DHCP_CONTEXT_PRE_DB | DHCP_CONTEXT_POST_DB,
			NULL, NULL);

    /* and other common arguments */
    inet_pton(AF_INET6, "1:2:3:4::", &addr);

    uid = "client0";
    memset(&ds, 0, sizeof(ds));
    ds.len = strlen(uid);
    if (!buffer_allocate(&ds.buffer, ds.len, MDL)) {
        atf_tc_fail("Out of memory");
    }
    ds.data = ds.buffer->data;
    memcpy((char *)ds.data, uid, ds.len);

    pool = NULL;
    iaaddr1 = NULL;
    iaaddr2 = NULL;
    expired_iaaddr = NULL;

    /*
     * Add two leases iaaddr1 with expire time of 200
     * and iaaddr2 with expire time of 300.  Then update
     * iaaddr2 to expire in 100 instead.  This should cause
     * iaaddr2 to move with the hash list.
     */
    /* create pool and add iaaddr1 and iaaddr2 */
    if (ipv6_pool_allocate(&pool, D6O_IA_NA, &addr,
                           64, 128, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: ipv6_pool_allocate() %s:%d", MDL);
    }
    if (create_lease6(pool, &iaaddr1, &attempts, &ds, 200) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: create_lease6() %s:%d", MDL);
    }
    if (renew_lease6(pool, iaaddr1) != ISC_R_SUCCESS) {
            atf_tc_fail("ERROR: renew_lease6() %s:%d", MDL);
    }
    if (create_lease6(pool, &iaaddr2, &attempts, &ds, 300) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: create_lease6() %s:%d", MDL);
    }
    if (renew_lease6(pool, iaaddr2) != ISC_R_SUCCESS) {
            atf_tc_fail("ERROR: renew_lease6() %s:%d", MDL);
    }

    /* verify pool */
    if (pool->num_active != 2) {
        atf_tc_fail("ERROR: bad num_active %s:%d", MDL);
    }

    /* reduce lease for iaaddr2 */
    iaaddr2->soft_lifetime_end_time = 100;
    if (renew_lease6(pool, iaaddr2) != ISC_R_SUCCESS) {
            atf_tc_fail("ERROR: renew_lease6() %s:%d", MDL);
    }

    /* expire a lease, it should be iaaddr2 with an expire time of 100 */
    if (expire_lease6(&expired_iaaddr, pool, 1000) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: expire_lease6() %s:%d", MDL);
    }
    if (expired_iaaddr == NULL) {
        atf_tc_fail("ERROR: should have expired a lease %s:%d", MDL);
    }
    if (expired_iaaddr != iaaddr2) {
        atf_tc_fail("Error: incorrect lease expired %s:%d", MDL);
    }
    if (expired_iaaddr->hard_lifetime_end_time != 100) {
        atf_tc_fail("ERROR: bad hard_lifetime_end_time %s:%d", MDL);
    }
    if (iasubopt_dereference(&expired_iaaddr, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: iasubopt_dereference() %s:%d", MDL);
    }

    /* expire a lease, it should be iaaddr1 with an expire time of 200 */
    if (expire_lease6(&expired_iaaddr, pool, 1000) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: expire_lease6() %s:%d", MDL);
    }
    if (expired_iaaddr == NULL) {
        atf_tc_fail("ERROR: should have expired a lease %s:%d", MDL);
    }
    if (expired_iaaddr != iaaddr1) {
        atf_tc_fail("Error: incorrect lease expired %s:%d", MDL);
    }
    if (expired_iaaddr->hard_lifetime_end_time != 200) {
        atf_tc_fail("ERROR: bad hard_lifetime_end_time %s:%d", MDL);
    }
    if (iasubopt_dereference(&expired_iaaddr, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: iasubopt_dereference() %s:%d", MDL);
    }

    /* cleanup */
    if (iasubopt_dereference(&iaaddr1, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: iasubopt_dereference() %s:%d", MDL);
    }
    if (iasubopt_dereference(&iaaddr2, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: iasubopt_dereference() %s:%d", MDL);
    }
    if (ipv6_pool_dereference(&pool, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: ipv6_pool_dereference() %s:%d", MDL);
    }
}

/*
 * Small pool.
 * check that a small pool behaves properly.
 */

ATF_TC(small_pool);
ATF_TC_HEAD(small_pool, tc)
{
    atf_tc_set_md_var(tc, "descr", "This test case checks that small pool "
                      "is handled properly.");
}
ATF_TC_BODY(small_pool, tc)
{
    struct in6_addr addr;
    struct ipv6_pool *pool;
    struct iasubopt *iaaddr;
    char *uid;
    struct data_string ds;
    unsigned int attempts;

    /* set up dhcp globals */
    dhcp_context_create(DHCP_CONTEXT_PRE_DB | DHCP_CONTEXT_POST_DB,
			NULL, NULL);

    /* and other common arguments */
    inet_pton(AF_INET6, "1:2:3:4::", &addr);
    addr.s6_addr[14] = 0x81;

    uid = "client0";
    memset(&ds, 0, sizeof(ds));
    ds.len = strlen(uid);
    if (!buffer_allocate(&ds.buffer, ds.len, MDL)) {
        atf_tc_fail("Out of memory");
    }
    ds.data = ds.buffer->data;
    memcpy((char *)ds.data, uid, ds.len);

    pool = NULL;
    iaaddr = NULL;

    /* tests */
    if (ipv6_pool_allocate(&pool, D6O_IA_NA, &addr,
                           127, 128, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: ipv6_pool_allocate() %s:%d", MDL);
    }

    if (create_lease6(pool, &iaaddr, &attempts,
              &ds, 42) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: create_lease6() %s:%d", MDL);
    }
    if (renew_lease6(pool, iaaddr) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: renew_lease6() %s:%d", MDL);
    }
    if (iasubopt_dereference(&iaaddr, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: iasubopt_dereference() %s:%d", MDL);
    }
    if (create_lease6(pool, &iaaddr, &attempts,
              &ds, 11) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: create_lease6() %s:%d", MDL);
    }
    if (renew_lease6(pool, iaaddr) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: renew_lease6() %s:%d", MDL);
    }
    if (iasubopt_dereference(&iaaddr, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: iasubopt_dereference() %s:%d", MDL);
    }
    if (create_lease6(pool, &iaaddr, &attempts,
              &ds, 11) != ISC_R_NORESOURCES) {
        atf_tc_fail("ERROR: create_lease6() %s:%d", MDL);
    }
    if (ipv6_pool_dereference(&pool, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: ipv6_pool_dereference() %s:%d", MDL);
    }
}

/*
 * Address to pool mapping.
 * Verify that we find the proper pool for an address
 * or don't find a pool if we don't have one for the given
 * address.
 */
ATF_TC(many_pools);
ATF_TC_HEAD(many_pools, tc)
{
    atf_tc_set_md_var(tc, "descr", "This test case checks that functions "
                      "across all pools are working correctly.");
}
ATF_TC_BODY(many_pools, tc)
{
    struct in6_addr addr;
    struct ipv6_pool *pool;

    /* set up dhcp globals */
    dhcp_context_create(DHCP_CONTEXT_PRE_DB | DHCP_CONTEXT_POST_DB,
			NULL, NULL);

    /* and other common arguments */
    inet_pton(AF_INET6, "1:2:3:4::", &addr);

    /* tests */

    pool = NULL;
    if (ipv6_pool_allocate(&pool, D6O_IA_NA, &addr,
                           64, 128, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: ipv6_pool_allocate() %s:%d", MDL);
    }
    if (add_ipv6_pool(pool) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: add_ipv6_pool() %s:%d", MDL);
    }
    if (ipv6_pool_dereference(&pool, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: ipv6_pool_dereference() %s:%d", MDL);
    }
    pool = NULL;
    if (find_ipv6_pool(&pool, D6O_IA_NA, &addr) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: find_ipv6_pool() %s:%d", MDL);
    }
    if (ipv6_pool_dereference(&pool, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: ipv6_pool_dereference() %s:%d", MDL);
    }
    inet_pton(AF_INET6, "1:2:3:4:ffff:ffff:ffff:ffff", &addr);
    pool = NULL;
    if (find_ipv6_pool(&pool, D6O_IA_NA, &addr) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: find_ipv6_pool() %s:%d", MDL);
    }
    if (ipv6_pool_dereference(&pool, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: ipv6_pool_dereference() %s:%d", MDL);
    }
    inet_pton(AF_INET6, "1:2:3:5::", &addr);
    pool = NULL;
    if (find_ipv6_pool(&pool, D6O_IA_NA, &addr) != ISC_R_NOTFOUND) {
        atf_tc_fail("ERROR: find_ipv6_pool() %s:%d", MDL);
    }
    inet_pton(AF_INET6, "1:2:3:3:ffff:ffff:ffff:ffff", &addr);
    pool = NULL;
    if (find_ipv6_pool(&pool, D6O_IA_NA, &addr) != ISC_R_NOTFOUND) {
        atf_tc_fail("ERROR: find_ipv6_pool() %s:%d", MDL);
    }

/*  iaid = 666;
    ia_na = NULL;
    if (ia_allocate(&ia_na, iaid, "TestDUID", 8, MDL) != ISC_R_SUCCESS) {
        atf_tc_fail("ERROR: ia_allocate() %s:%d", MDL);
    }*/

    {
        struct in6_addr r;
        struct data_string ds;
        u_char data[16];
        char buf[64];
        int i, j;

        memset(&ds, 0, sizeof(ds));
        memset(data, 0xaa, sizeof(data));
        ds.len = 16;
        ds.data = data;

        inet_pton(AF_INET6, "3ffe:501:ffff:100::", &addr);
        for (i = 32; i < 42; i++)
            for (j = i + 1; j < 49; j++) {
                memset(&r, 0, sizeof(r));
                memset(buf, 0, 64);
                build_prefix6(&r, &addr, i, j, &ds);
                inet_ntop(AF_INET6, &r, buf, 64);
                printf("%d,%d-> %s/%d\n", i, j, buf, j);
            }
    }
}

ATF_TP_ADD_TCS(tp)
{
    ATF_TP_ADD_TC(tp, iaaddr_basic);
    ATF_TP_ADD_TC(tp, iaaddr_negative);
    ATF_TP_ADD_TC(tp, ia_na_basic);
    ATF_TP_ADD_TC(tp, ia_na_manyaddrs);
    ATF_TP_ADD_TC(tp, ia_na_negative);
    ATF_TP_ADD_TC(tp, ipv6_pool_basic);
    ATF_TP_ADD_TC(tp, ipv6_pool_negative);
    ATF_TP_ADD_TC(tp, expire_order);
    ATF_TP_ADD_TC(tp, expire_order_reduce);
    ATF_TP_ADD_TC(tp, small_pool);
    ATF_TP_ADD_TC(tp, many_pools);

    return (atf_no_error());
}
