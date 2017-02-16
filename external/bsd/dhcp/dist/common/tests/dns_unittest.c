/*	$NetBSD: dns_unittest.c,v 1.1.1.1 2014/07/12 11:57:48 spz Exp $	*/
/*
 * Copyright (C) 2013 Internet Systems Consortium, Inc. ("ISC")
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
#include "dhcpd.h"

/*
 * This file provides unit tests for the dns and ddns code.
 * Currently this is limited to verifying the dhcid code is
 * working properly.  In time we may be able to expand the
 * tests to cover other areas.
 *
 * The tests for the interim txt records comapre to previous
 * internally generated values.
 *
 * The tests for the standard dhcid records compare to values
 * from rfc 4701
 */

char *name_1 = "chi6.example.com"; 
u_int8_t clid_1[] = {0x00, 0x01, 0x00, 0x06, 0x41, 0x2d, 0xf1, 0x66, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
u_int8_t std_result_1[] = {0x00, 0x02, 0x01, 0x63, 0x6f, 0xc0, 0xb8, 0x27, 0x1c,
			  0x82, 0x82, 0x5b, 0xb1, 0xac, 0x5c, 0x41, 0xcf, 0x53,
			  0x51, 0xaa, 0x69, 0xb4, 0xfe, 0xbd, 0x94, 0xe8, 0xf1,
			  0x7c, 0xdb, 0x95, 0x00, 0x0d, 0xa4, 0x8c, 0x40};
char *int_result_1 = "\"02abf8cd3753dc1847be40858becd77865";

char *name_2 = "chi.example.com";
u_int8_t clid_2[] = {0x01, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c};
u_int8_t std_result_2[] = {0x00, 0x01, 0x01, 0x39, 0x20, 0xfe, 0x5d, 0x1d, 0xce,
			  0xb3, 0xfd, 0x0b, 0xa3, 0x37, 0x97, 0x56, 0xa7, 0x0d,
			  0x73, 0xb1, 0x70, 0x09, 0xf4, 0x1d, 0x58, 0xbd, 0xdb,
			  0xfc, 0xd6, 0xa2, 0x50, 0x39, 0x56, 0xd8, 0xda};
char *int_result_2 = "\"31934ffa9344a3ab86c380505a671e5113";

char *name_3 = "client.example.com";
u_int8_t clid_3[] = {0x01, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
u_int8_t std_result_3[] = {0x00, 0x00, 0x01, 0xc4, 0xb9, 0xa5, 0xb2, 0x49, 0x65,
			  0x13, 0x43, 0x15, 0x8d, 0xde, 0x7b, 0xcc, 0x77, 0x16,
			  0x98, 0x41, 0xf7, 0xa4, 0x24, 0x3a, 0x57, 0x2b, 0x5c,
			  0x28, 0x3f, 0xff, 0xed, 0xeb, 0x3f, 0x75, 0xe6};
char *int_result_3 = "\"0046b6cacea62dc1d4567b068175d1f808";

void call_get_std_dhcid(int test, int type,
			u_int8_t *clid, unsigned clidlen,
			char *name, unsigned namelen,
			u_int8_t *dhcid, unsigned dhcid_len)
{
  dhcp_ddns_cb_t ddns_cb;
  struct data_string *id;

  memset(&ddns_cb, 0, sizeof(ddns_cb));
  ddns_cb.dhcid_class = dns_rdatatype_dhcid;;

  id = &ddns_cb.fwd_name;
  if (!buffer_allocate(&id->buffer, namelen, MDL))
    atf_tc_fail("Unable to allocate buffer for std test %d", test);
  id->data = id->buffer->data;
  memcpy(id->buffer->data, name, namelen);
  id->len = namelen;

    if (get_dhcid(&ddns_cb, type, clid, clidlen) != 1) {
        atf_tc_fail("Unable to get std dhcid for %d", test);
    } else if (ddns_cb.dhcid_class != dns_rdatatype_dhcid) {
        atf_tc_fail("Wrong class for std dhcid for %d", test);
    } else if (ddns_cb.dhcid.len != dhcid_len) {
        atf_tc_fail("Wrong length for std dhcid for %d", test);
    } else if (memcmp(ddns_cb.dhcid.data, dhcid, dhcid_len) != 0) {
        atf_tc_fail("Wrong digest for std dhcid for %d", test);
    }

    /* clean up  */
    data_string_forget(&ddns_cb.dhcid, MDL);

    return;
}
ATF_TC(standard_dhcid);

ATF_TC_HEAD(standard_dhcid, tc)
{
    atf_tc_set_md_var(tc, "descr", "Verify standard dhcid construction.");
}


ATF_TC_BODY(standard_dhcid, tc)
{

  call_get_std_dhcid(1, 2, clid_1, sizeof(clid_1),
		     name_1, strlen(name_1),
		     std_result_1, 35);


  call_get_std_dhcid(2, 1, clid_2, sizeof(clid_2),
		     name_2, strlen(name_2),
		     std_result_2, 35);


  call_get_std_dhcid(3, 0, clid_3, sizeof(clid_3),
		     name_3, strlen(name_3),
		     std_result_3, 35);
}

void call_get_int_dhcid(int test, int type,
			u_int8_t *clid, unsigned clidlen,
			char *dhcid, unsigned dhcid_len)
{
  dhcp_ddns_cb_t ddns_cb;

  memset(&ddns_cb, 0, sizeof(ddns_cb));
  ddns_cb.dhcid_class = dns_rdatatype_txt;;

    if (get_dhcid(&ddns_cb, type, clid, clidlen) != 1) {
        atf_tc_fail("Unable to get txt dhcid for %d", test);
    } else if (ddns_cb.dhcid_class != dns_rdatatype_txt) {
        atf_tc_fail("Wrong class for txt dhcid for %d", test);
    } else if (ddns_cb.dhcid.len != dhcid_len) {
        atf_tc_fail("Wrong length for txt dhcid for %d", test);
    } else if (memcmp(ddns_cb.dhcid.data, dhcid, dhcid_len) != 0) {
        atf_tc_fail("Wrong digest for txt dhcid for %d", test);
    }

    /* clean up  */
    data_string_forget(&ddns_cb.dhcid, MDL);

    return;
}

ATF_TC(interim_dhcid);

ATF_TC_HEAD(interim_dhcid, tc)
{
    atf_tc_set_md_var(tc, "descr", "Verify interim dhcid construction.");
}

ATF_TC_BODY(interim_dhcid, tc)
{

  call_get_int_dhcid(1, 2, clid_1, sizeof(clid_1),
		     int_result_1, 35);


  call_get_int_dhcid(2, DHO_DHCP_CLIENT_IDENTIFIER,
		     clid_2, sizeof(clid_2),
		     int_result_2, 35);


  call_get_int_dhcid(3, 0, clid_3, sizeof(clid_3),
		     int_result_3, 35);

}

/* This macro defines main() method that will call specified
   test cases. tp and simple_test_case names can be whatever you want
   as long as it is a valid variable identifier. */
ATF_TP_ADD_TCS(tp)
{
    ATF_TP_ADD_TC(tp, interim_dhcid);
    ATF_TP_ADD_TC(tp, standard_dhcid);

    return (atf_no_error());
}
