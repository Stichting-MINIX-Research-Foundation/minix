/*	$NetBSD: hash_unittest.c,v 1.1.1.3 2014/07/12 11:58:16 spz Exp $	*/
/*
 * Copyright (c) 2012 by Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *   Internet Systems Consortium, Inc.
 *   950 Charter Street
 *   Redwood City, CA 94063
 *   <info@isc.org>
 *   https://www.isc.org/
 *
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: hash_unittest.c,v 1.1.1.3 2014/07/12 11:58:16 spz Exp $");

#include "config.h"
#include <atf-c.h>
#include <omapip/omapip_p.h>
#include "dhcpd.h"

/*
 * The following structures are kept here for reference only. As hash functions
 * are somewhat convoluted, they are copied here for the reference. Original
 * location is specified. Keep in mind that it may change over time:
 *
 * copied from server/omapi.c:49 *
 * omapi_object_type_t *dhcp_type_lease;
 * omapi_object_type_t *dhcp_type_pool;
 * omapi_object_type_t *dhcp_type_class;
 * omapi_object_type_t *dhcp_type_subclass;
 * omapi_object_type_t *dhcp_type_host;
 *
 * copied from server/salloc.c:138
 * OMAPI_OBJECT_ALLOC (lease, struct lease, dhcp_type_lease)
 * OMAPI_OBJECT_ALLOC (class, struct class, dhcp_type_class)
 * OMAPI_OBJECT_ALLOC (subclass, struct class, dhcp_type_subclass)
 * OMAPI_OBJECT_ALLOC (pool, struct pool, dhcp_type_pool)
 * OMAPI_OBJECT_ALLOC (host, struct host_decl, dhcp_type_host)
 *
 * copied from server/mdb.c:2686
 * HASH_FUNCTIONS(lease_ip, const unsigned char *, struct lease, lease_ip_hash_t,
 *                lease_reference, lease_dereference, do_ip4_hash)
 * HASH_FUNCTIONS(lease_id, const unsigned char *, struct lease, lease_id_hash_t,
 *                lease_reference, lease_dereference, do_id_hash)
 * HASH_FUNCTIONS (host, const unsigned char *, struct host_decl, host_hash_t,
 *                 host_reference, host_dereference, do_string_hash)
 * HASH_FUNCTIONS (class, const char *, struct class, class_hash_t,
 *                 class_reference, class_dereference, do_string_hash)
 *
 * copied from server/mdb.c:46
 * host_hash_t *host_hw_addr_hash;
 * host_hash_t *host_uid_hash;
 * host_hash_t *host_name_hash;
 * lease_id_hash_t *lease_uid_hash;
 * lease_ip_hash_t *lease_ip_addr_hash;
 * lease_id_hash_t *lease_hw_addr_hash;
 */

/**
 *  @brief sets client-id field in host declaration
 *
 *  @param host pointer to host declaration
 *  @param uid pointer to client-id data
 *  @param uid_len length of the client-id data
 *
 *  @return 1 if successful, 0 otherwise
 */
int lease_set_clientid(struct host_decl *host, const unsigned char *uid, int uid_len) {

    /* clean-up this mess and set client-identifier in a sane way */
    int real_len = uid_len;
    if (uid_len == 0) {
        real_len = strlen((const char *)uid) + 1;
    }

    memset(&host->client_identifier, 0, sizeof(host->client_identifier));
    host->client_identifier.len = uid_len;
    if (!buffer_allocate(&host->client_identifier.buffer, real_len, MDL)) {
        return 0;
    }
    host->client_identifier.data = host->client_identifier.buffer->data;
    memcpy((char *)host->client_identifier.data, uid, real_len);

    return 1;
}

/// @brief executes uid hash test for specified client-ids (2 hosts)
///
/// Creates two host structures, adds first host to the uid hash,
/// then adds second host to the hash, then removes first host,
/// then removed the second. Many checks are performed during all
/// operations.
///
/// @param clientid1 client-id of the first host
/// @param clientid1_len client-id1 length (may be 0 for strings)
/// @param clientid2 client-id of the second host
/// @param clientid2_len client-id2 length (may be 0 for strings)
void lease_hash_test_2hosts(unsigned char clientid1[], size_t clientid1_len,
                            unsigned char clientid2[], size_t clientid2_len) {

    printf("Checking hash operation for 2 hosts: clientid1-len=%lu"
           "clientid2-len=%lu\n", (unsigned long) clientid1_len,
           (unsigned long) clientid2_len);

    dhcp_db_objects_setup ();
    dhcp_common_objects_setup ();

    /* check that there is actually zero hosts in the hash */
    /* @todo: host_hash_for_each() */

    struct host_decl *host1 = 0, *host2 = 0;
    struct host_decl *check = 0;

    /* === step 1: allocate hosts === */
    ATF_CHECK_MSG(host_allocate(&host1, MDL) == ISC_R_SUCCESS,
                  "Failed to allocate host");
    ATF_CHECK_MSG(host_allocate(&host2, MDL) == ISC_R_SUCCESS,
                  "Failed to allocate host");

    ATF_CHECK_MSG(host_new_hash(&host_uid_hash, HOST_HASH_SIZE, MDL) != 0,
                  "Unable to create new hash");

    ATF_CHECK_MSG(buffer_allocate(&host1->client_identifier.buffer,
                                  clientid1_len, MDL) != 0,
                  "Can't allocate uid buffer for host1");

    ATF_CHECK_MSG(buffer_allocate(&host2->client_identifier.buffer,
                                  clientid2_len, MDL) != 0,
                  "Can't allocate uid buffer for host2");

    /* setting up host1->client_identifier is actually not needed */
    /*
    ATF_CHECK_MSG(lease_set_clientid(host1, clientid1, actual1_len) != 0,
                  "Failed to set client-id for host1");

    ATF_CHECK_MSG(lease_set_clientid(host2, clientid2, actual2_len) != 0,
                  "Failed to set client-id for host2");
    */

    ATF_CHECK_MSG(host1->refcnt == 1, "Invalid refcnt for host1");
    ATF_CHECK_MSG(host2->refcnt == 1, "Invalid refcnt for host2");

    /* verify that our hosts are not in the hash yet */
    ATF_CHECK_MSG(host_hash_lookup(&check, host_uid_hash, clientid1,
                                   clientid1_len, MDL) == 0,
                   "Host1 is not supposed to be in the uid_hash.");

    ATF_CHECK_MSG(!check, "Host1 is not supposed to be in the uid_hash.");

    ATF_CHECK_MSG(host_hash_lookup(&check, host_uid_hash, clientid2,
                                   clientid2_len, MDL) == 0,
                  "Host2 is not supposed to be in the uid_hash.");
    ATF_CHECK_MSG(!check, "Host2 is not supposed to be in the uid_hash.");


    /* === step 2: add first host to the hash === */
    host_hash_add(host_uid_hash, clientid1, clientid1_len, host1, MDL);

    /* 2 pointers expected: ours (host1) and the one stored in hash */
    ATF_CHECK_MSG(host1->refcnt == 2, "Invalid refcnt for host1");
    /* 1 pointer expected: just ours (host2) */
    ATF_CHECK_MSG(host2->refcnt == 1, "Invalid refcnt for host2");

    /* verify that host1 is really in the hash and the we can find it */
    ATF_CHECK_MSG(host_hash_lookup(&check, host_uid_hash, clientid1,
                                   clientid1_len, MDL),
                  "Host1 was supposed to be in the uid_hash.");
    ATF_CHECK_MSG(check, "Host1 was supposed to be in the uid_hash.");

    /* Hey! That's not the host we were looking for! */
    ATF_CHECK_MSG(check == host1, "Wrong host returned by host_hash_lookup");

    /* 3 pointers: host1, (stored in hash), check */
    ATF_CHECK_MSG(host1->refcnt == 3, "Invalid refcnt for host1");

    /* reference count should be increased because we not have a pointer */

    host_dereference(&check, MDL); /* we don't need it now */

    ATF_CHECK_MSG(check == NULL, "check pointer is supposed to be NULL");

    /* 2 pointers: host1, (stored in hash) */
    ATF_CHECK_MSG(host1->refcnt == 2, "Invalid refcnt for host1");

    /* verify that host2 is not in the hash */
    ATF_CHECK_MSG(host_hash_lookup(&check, host_uid_hash, clientid2,
                                   clientid2_len, MDL) == 0,
                  "Host2 was not supposed to be in the uid_hash[2].");
    ATF_CHECK_MSG(check == NULL, "Host2 was not supposed to be in the hash.");


    /* === step 3: add second hot to the hash === */
    host_hash_add(host_uid_hash, clientid2, clientid2_len, host2, MDL);

    /* 2 pointers expected: ours (host1) and the one stored in hash */
    ATF_CHECK_MSG(host2->refcnt == 2, "Invalid refcnt for host2");

    ATF_CHECK_MSG(host_hash_lookup(&check, host_uid_hash, clientid2,
                                   clientid2_len, MDL),
                  "Host2 was supposed to be in the uid_hash.");
    ATF_CHECK_MSG(check, "Host2 was supposed to be in the uid_hash.");

    /* Hey! That's not the host we were looking for! */
    ATF_CHECK_MSG(check == host2, "Wrong host returned by host_hash_lookup");

    /* 3 pointers: host1, (stored in hash), check */
    ATF_CHECK_MSG(host2->refcnt == 3, "Invalid refcnt for host1");

    host_dereference(&check, MDL); /* we don't need it now */

    /* now we have 2 hosts in the hash */

    /* verify that host1 is still in the hash and the we can find it */
    ATF_CHECK_MSG(host_hash_lookup(&check, host_uid_hash, clientid1,
                                   clientid1_len, MDL),
                  "Host1 was supposed to be in the uid_hash.");
    ATF_CHECK_MSG(check, "Host1 was supposed to be in the uid_hash.");

    /* Hey! That's not the host we were looking for! */
    ATF_CHECK_MSG(check == host1, "Wrong host returned by host_hash_lookup");

    /* 3 pointers: host1, (stored in hash), check */
    ATF_CHECK_MSG(host1->refcnt == 3, "Invalid refcnt for host1");

    host_dereference(&check, MDL); /* we don't need it now */


    /**
     * @todo check that there is actually two hosts in the hash.
     * Use host_hash_for_each() for that.
     */

    /* === step 4: remove first host from the hash === */

    /* delete host from hash */
    host_hash_delete(host_uid_hash, clientid1, clientid1_len, MDL);

    ATF_CHECK_MSG(host1->refcnt == 1, "Invalid refcnt for host1");
    ATF_CHECK_MSG(host2->refcnt == 2, "Invalid refcnt for host2");

    /* verify that host1 is no longer in the hash */
    ATF_CHECK_MSG(host_hash_lookup(&check, host_uid_hash, clientid1,
                                   clientid1_len, MDL) == 0,
                   "Host1 is not supposed to be in the uid_hash.");
    ATF_CHECK_MSG(!check, "Host1 is not supposed to be in the uid_hash.");

    /* host2 should be still there, though */
    ATF_CHECK_MSG(host_hash_lookup(&check, host_uid_hash, clientid2,
                                   clientid2_len, MDL),
                   "Host2 was supposed to still be in the uid_hash.");
    host_dereference(&check, MDL);

    /* === step 5: remove second host from the hash === */
    host_hash_delete(host_uid_hash, clientid2, clientid2_len, MDL);

    ATF_CHECK_MSG(host1->refcnt == 1, "Invalid refcnt for host1");
    ATF_CHECK_MSG(host2->refcnt == 1, "Invalid refcnt for host2");

    ATF_CHECK_MSG(host_hash_lookup(&check, host_uid_hash, clientid2,
                                   clientid2_len, MDL) == 0,
                   "Host2 was not supposed to be in the uid_hash anymore.");

    host_dereference(&host1, MDL);
    host_dereference(&host2, MDL);

    /*
     * No easy way to check if the host object were actually released.
     * We could run it in valgrind and check for memory leaks.
     */

#if defined (DEBUG_MEMORY_LEAKAGE) && defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
    /* @todo: Should be called in cleanup */
    free_everything ();
#endif
}

/// @brief executes uid hash test for specified client-ids (3 hosts)
///
/// Creates three host structures, adds first host to the uid hash,
/// then adds second host to the hash, then removes first host,
/// then removed the second. Many checks are performed during all
/// operations.
///
/// @param clientid1 client-id of the first host
/// @param clientid1_len client-id1 length (may be 0 for strings)
/// @param clientid2 client-id of the second host
/// @param clientid2_len client-id2 length (may be 0 for strings)
/// @param clientid3 client-id of the second host
/// @param clientid3_len client-id2 length (may be 0 for strings)
void lease_hash_test_3hosts(unsigned char clientid1[], size_t clientid1_len,
                            unsigned char clientid2[], size_t clientid2_len,
                            unsigned char clientid3[], size_t clientid3_len) {

    printf("Checking hash operation for 3 hosts: clientid1-len=%lu"
           " clientid2-len=%lu clientid3-len=%lu\n",
           (unsigned long) clientid1_len, (unsigned long) clientid2_len,
           (unsigned long) clientid3_len);

    dhcp_db_objects_setup ();
    dhcp_common_objects_setup ();

    /* check that there is actually zero hosts in the hash */
    /* @todo: host_hash_for_each() */

    struct host_decl *host1 = 0, *host2 = 0, *host3 = 0;
    struct host_decl *check = 0;

    /* === step 1: allocate hosts === */
    ATF_CHECK_MSG(host_allocate(&host1, MDL) == ISC_R_SUCCESS,
                  "Failed to allocate host");
    ATF_CHECK_MSG(host_allocate(&host2, MDL) == ISC_R_SUCCESS,
                  "Failed to allocate host");
    ATF_CHECK_MSG(host_allocate(&host3, MDL) == ISC_R_SUCCESS,
                  "Failed to allocate host");

    ATF_CHECK_MSG(host_new_hash(&host_uid_hash, HOST_HASH_SIZE, MDL) != 0,
                  "Unable to create new hash");

    ATF_CHECK_MSG(buffer_allocate(&host1->client_identifier.buffer,
                                  clientid1_len, MDL) != 0,
                  "Can't allocate uid buffer for host1");
    ATF_CHECK_MSG(buffer_allocate(&host2->client_identifier.buffer,
                                  clientid2_len, MDL) != 0,
                  "Can't allocate uid buffer for host2");
    ATF_CHECK_MSG(buffer_allocate(&host3->client_identifier.buffer,
                                  clientid3_len, MDL) != 0,
                  "Can't allocate uid buffer for host3");

    /* verify that our hosts are not in the hash yet */
    ATF_CHECK_MSG(host_hash_lookup(&check, host_uid_hash, clientid1,
                                   clientid1_len, MDL) == 0,
                   "Host1 is not supposed to be in the uid_hash.");

    ATF_CHECK_MSG(!check, "Host1 is not supposed to be in the uid_hash.");

    ATF_CHECK_MSG(host_hash_lookup(&check, host_uid_hash, clientid2,
                                   clientid2_len, MDL) == 0,
                  "Host2 is not supposed to be in the uid_hash.");
    ATF_CHECK_MSG(!check, "Host2 is not supposed to be in the uid_hash.");

    ATF_CHECK_MSG(host_hash_lookup(&check, host_uid_hash, clientid3,
                                   clientid3_len, MDL) == 0,
                  "Host3 is not supposed to be in the uid_hash.");
    ATF_CHECK_MSG(!check, "Host3 is not supposed to be in the uid_hash.");

    /* === step 2: add hosts to the hash === */
    host_hash_add(host_uid_hash, clientid1, clientid1_len, host1, MDL);
    host_hash_add(host_uid_hash, clientid2, clientid2_len, host2, MDL);
    host_hash_add(host_uid_hash, clientid3, clientid3_len, host3, MDL);

    ATF_CHECK_MSG(host_hash_lookup(&check, host_uid_hash, clientid1,
                                   clientid1_len, MDL),
                  "Host1 was supposed to be in the uid_hash.");
    /* Hey! That's not the host we were looking for! */
    ATF_CHECK_MSG(check == host1, "Wrong host returned by host_hash_lookup");
    host_dereference(&check, MDL); /* we don't need it now */

    ATF_CHECK_MSG(host_hash_lookup(&check, host_uid_hash, clientid2,
                                   clientid2_len, MDL),
                  "Host2 was supposed to be in the uid_hash.");
    ATF_CHECK_MSG(check, "Host2 was supposed to be in the uid_hash.");
    /* Hey! That's not the host we were looking for! */
    ATF_CHECK_MSG(check == host2, "Wrong host returned by host_hash_lookup");
    host_dereference(&check, MDL); /* we don't need it now */

    ATF_CHECK_MSG(host_hash_lookup(&check, host_uid_hash, clientid3,
                                   clientid3_len, MDL),
                  "Host3 was supposed to be in the uid_hash.");
    ATF_CHECK_MSG(check, "Host3 was supposed to be in the uid_hash.");
    /* Hey! That's not the host we were looking for! */
    ATF_CHECK_MSG(check == host3, "Wrong host returned by host_hash_lookup");
    host_dereference(&check, MDL); /* we don't need it now */

    /* === step 4: remove first host from the hash === */

    /* delete host from hash */
    host_hash_delete(host_uid_hash, clientid1, clientid1_len, MDL);

    /* verify that host1 is no longer in the hash */
    ATF_CHECK_MSG(host_hash_lookup(&check, host_uid_hash, clientid1,
                                   clientid1_len, MDL) == 0,
                   "Host1 is not supposed to be in the uid_hash.");
    ATF_CHECK_MSG(!check, "Host1 is not supposed to be in the uid_hash.");

    /* host2 and host3 should be still there, though */
    ATF_CHECK_MSG(host_hash_lookup(&check, host_uid_hash, clientid2,
                                   clientid2_len, MDL),
                   "Host2 was supposed to still be in the uid_hash.");
    host_dereference(&check, MDL);
    ATF_CHECK_MSG(host_hash_lookup(&check, host_uid_hash, clientid3,
                                   clientid3_len, MDL),
                   "Host3 was supposed to still be in the uid_hash.");
    host_dereference(&check, MDL);

    /* === step 5: remove second host from the hash === */
    host_hash_delete(host_uid_hash, clientid2, clientid2_len, MDL);

    ATF_CHECK_MSG(host_hash_lookup(&check, host_uid_hash, clientid2,
                                   clientid2_len, MDL) == 0,
                   "Host2 was not supposed to be in the uid_hash anymore.");
    ATF_CHECK_MSG(host_hash_lookup(&check, host_uid_hash, clientid3,
                                   clientid3_len, MDL),
                   "Host3 was supposed to still be in the uid_hash.");
    host_dereference(&check, MDL);

    /* === step 6: remove the last (third) host from the hash === */
    host_hash_delete(host_uid_hash, clientid3, clientid3_len, MDL);

    ATF_CHECK_MSG(host_hash_lookup(&check, host_uid_hash, clientid3,
                                   clientid3_len, MDL) == 0,
                   "Host3 was not supposed to be in the uid_hash anymore.");
    host_dereference(&check, MDL);


    host_dereference(&host1, MDL);
    host_dereference(&host2, MDL);
    host_dereference(&host3, MDL);

    /*
     * No easy way to check if the host object were actually released.
     * We could run it in valgrind and check for memory leaks.
     */

#if defined (DEBUG_MEMORY_LEAKAGE) && defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
    /* @todo: Should be called in cleanup */
    free_everything ();
#endif
}

ATF_TC(lease_hash_basic_2hosts);

ATF_TC_HEAD(lease_hash_basic_2hosts, tc) {
    atf_tc_set_md_var(tc, "descr", "Basic lease hash tests");
    /*
     * The following functions are tested:
     * host_allocate(), host_new_hash(), buffer_allocate(), host_hash_lookup()
     * host_hash_add(), host_hash_delete()
     */
}

ATF_TC_BODY(lease_hash_basic_2hosts, tc) {

    unsigned char clientid1[] = { 0x1, 0x2, 0x3 };
    unsigned char clientid2[] = { 0xff, 0xfe };

    lease_hash_test_2hosts(clientid1, sizeof(clientid1),
                           clientid2, sizeof(clientid2));
}


ATF_TC(lease_hash_string_2hosts);

ATF_TC_HEAD(lease_hash_string_2hosts, tc) {
    atf_tc_set_md_var(tc, "descr", "string-based lease hash tests");
    /*
     * The following functions are tested:
     * host_allocate(), host_new_hash(), buffer_allocate(), host_hash_lookup()
     * host_hash_add(), host_hash_delete()
     */
}

ATF_TC_BODY(lease_hash_string_2hosts, tc) {

    unsigned char clientid1[] = "Alice";
    unsigned char clientid2[] = "Bob";

    lease_hash_test_2hosts(clientid1, 0, clientid2, 0);
}


ATF_TC(lease_hash_negative1);

ATF_TC_HEAD(lease_hash_negative1, tc) {
    atf_tc_set_md_var(tc, "descr", "Negative tests for lease hash");
}

ATF_TC_BODY(lease_hash_negative1, tc) {

    unsigned char clientid1[] = { 0x1 };
    unsigned char clientid2[] = { 0x0 };

    lease_hash_test_2hosts(clientid1, 0, clientid2, 1);
}



ATF_TC(lease_hash_string_3hosts);
ATF_TC_HEAD(lease_hash_string_3hosts, tc) {
    atf_tc_set_md_var(tc, "descr", "string-based lease hash tests");
    /*
     * The following functions are tested:
     * host_allocate(), host_new_hash(), buffer_allocate(), host_hash_lookup()
     * host_hash_add(), host_hash_delete()
     */
}
ATF_TC_BODY(lease_hash_string_3hosts, tc) {

    unsigned char clientid1[] = "Alice";
    unsigned char clientid2[] = "Bob";
    unsigned char clientid3[] = "Charlie";

    lease_hash_test_3hosts(clientid1, 0, clientid2, 0, clientid3, 0);
}


ATF_TC(lease_hash_basic_3hosts);
ATF_TC_HEAD(lease_hash_basic_3hosts, tc) {
    atf_tc_set_md_var(tc, "descr", "Basic lease hash tests");
    /*
     * The following functions are tested:
     * host_allocate(), host_new_hash(), buffer_allocate(), host_hash_lookup()
     * host_hash_add(), host_hash_delete()
     */
}
ATF_TC_BODY(lease_hash_basic_3hosts, tc) {

    unsigned char clientid1[] = { 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9 };
    unsigned char clientid2[] = { 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8 };
    unsigned char clientid3[] = { 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7 };

    lease_hash_test_3hosts(clientid1, sizeof(clientid1),
                           clientid2, sizeof(clientid2),
                           clientid3, sizeof(clientid3));
}

#if 0
/* This test is disabled as we solved the issue by prohibiting
   the code from using an improper client id earlier and restoring
   the hash code to its previous state.  As we may choose to
   redo the hash code again this test hasn't been deleted.
*/   
/* this test is a direct reproduction of 29851 issue */
ATF_TC(uid_hash_rt29851);

ATF_TC_HEAD(uid_hash_rt29851, tc) {
    atf_tc_set_md_var(tc, "descr", "Uid hash tests");

    /*
     * this test should last less than millisecond. If its execution
     *  is longer than 3 second, we hit infinite loop.
     */
    atf_tc_set_md_var(tc, "timeout", "3");
}

ATF_TC_BODY(uid_hash_rt29851, tc) {

    unsigned char clientid1[] = { 0x0 };
    unsigned char clientid2[] = { 0x0 };
    unsigned char clientid3[] = { 0x0 };

    int clientid1_len = 1;
    int clientid2_len = 1;
    int clientid3_len = 0;

    struct lease *lease1 = 0, *lease2 = 0, *lease3 = 0;

    dhcp_db_objects_setup ();
    dhcp_common_objects_setup ();

    ATF_CHECK(lease_id_new_hash(&lease_uid_hash, LEASE_HASH_SIZE, MDL));

    ATF_CHECK(lease_allocate (&lease1, MDL) == ISC_R_SUCCESS);
    ATF_CHECK(lease_allocate (&lease2, MDL) == ISC_R_SUCCESS);
    ATF_CHECK(lease_allocate (&lease3, MDL) == ISC_R_SUCCESS);

    lease1->uid = clientid1;
    lease2->uid = clientid2;
    lease3->uid = clientid3;

    lease1->uid_len = clientid1_len;
    lease2->uid_len = clientid2_len;
    lease3->uid_len = clientid3_len;

    uid_hash_add(lease1);
    /* uid_hash_delete(lease2); // not necessary for actual issue repro */
    uid_hash_add(lease3);

    /* lease2->uid_len = 0;     // not necessary for actual issue repro */
    /* uid_hash_delete(lease2); // not necessary for actual issue repro */
    /* uid_hash_delete(lease3); // not necessary for actual issue repro */
    uid_hash_delete(lease1);

    /* lease2->uid_len = 1;     // not necessary for actual issue repro */
    uid_hash_add(lease1);
    uid_hash_delete(lease2);
}
#endif

ATF_TP_ADD_TCS(tp) {
    ATF_TP_ADD_TC(tp, lease_hash_basic_2hosts);
    ATF_TP_ADD_TC(tp, lease_hash_basic_3hosts);
    ATF_TP_ADD_TC(tp, lease_hash_string_2hosts);
    ATF_TP_ADD_TC(tp, lease_hash_string_3hosts);
    ATF_TP_ADD_TC(tp, lease_hash_negative1);
#if 0 /* see comment in function */
    ATF_TP_ADD_TC(tp, uid_hash_rt29851);
#endif
    return (atf_no_error());
}
