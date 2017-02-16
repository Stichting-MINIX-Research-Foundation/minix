/*	$NetBSD: load_bal_unittest.c,v 1.1.1.2 2014/07/12 11:58:16 spz Exp $	*/
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

#include "dhcpd.h"

#include <atf-c.h>

/*
 * Test the load balancing code.  
 *
 * The two main variables are:
 * packet => the "packet" being processed
 * state  => the "state" of the failover peer
 * We only fill in the fields necessary for our testing
 * packet->raw->secs   => amount of time the client has been trying
 * packet->raw->hlen   => the length of the mac address of the client
 * packet->raw->chaddr => the mac address of the client
 * To simplify the tests the mac address will be only 1 byte long and
 * not really matter.  Instead the hba will be all 1s and the tests
 * will use the primary/secondary flag to change the expected result.
 *
 * state->i_am => primary or secondary
 * state->load_balance_max_secs => maxixum time for a client to be trying
 *                                 before the other peer responds
 *                                 set to 5 for these tests
 * state->hba = array of hash buckets assigning the hash to primary or secondary
 *              set to all ones (all primary) for theses tests
 */

ATF_TC(load_balance);

ATF_TC_HEAD(load_balance, tc)
{
	atf_tc_set_md_var(tc, "descr", "This test case checks that "
			  "load balancing works.");
}

ATF_TC_BODY(load_balance, tc)
{
	struct packet packet;
	struct dhcp_packet raw;
	dhcp_failover_state_t pstate, sstate;
	u_int8_t hba[256];

	memset(&packet, 0, sizeof(struct packet));
	memset(&raw, 0, sizeof(struct dhcp_packet));
	packet.raw = &raw;
	raw.hlen = 1;
	raw.chaddr[0] = 14;

	memset(hba, 0xFF, 256);

	/* primary state */
	memset(&pstate, 0, sizeof(dhcp_failover_state_t));
	pstate.i_am = primary;
	pstate.load_balance_max_secs = 5;
	pstate.hba = hba;

	/* secondary state, we can reuse the hba as it doesn't change */
	memset(&sstate, 0, sizeof(dhcp_failover_state_t));
	sstate.i_am = secondary;
	sstate.load_balance_max_secs = 5;
	sstate.hba = hba;

	/* Basic check, primary accepted, secondary not */
	raw.secs = htons(0);
	if (load_balance_mine(&packet, &pstate) != 1) {
		atf_tc_fail("ERROR: primary not accepted %s:%d", MDL);
	}

	if (load_balance_mine(&packet, &sstate) != 0) {
		atf_tc_fail("ERROR: secondary accepted %s:%d", MDL);
	}
	

	/* Timeout not exceeded, primary accepted, secondary not */
	raw.secs = htons(2);
	if (load_balance_mine(&packet, &pstate) != 1) {
		atf_tc_fail("ERROR: primary not accepted %s:%d", MDL);
	}

	if (load_balance_mine(&packet, &sstate) != 0) {
		atf_tc_fail("ERROR: secondary accepted %s:%d", MDL);
	}
	
	/* Timeout exceeded, both accepted */
	raw.secs = htons(6);
	if (load_balance_mine(&packet, &pstate) != 1) {
		atf_tc_fail("ERROR: primary not accepted %s:%d", MDL);
	}

	if (load_balance_mine(&packet, &sstate) != 1) {
		atf_tc_fail("ERROR: secondary not accepted %s:%d", MDL);
	}

	/* Timeout exeeded with a large value, both accepted */
	raw.secs = htons(257);
	if (load_balance_mine(&packet, &pstate) != 1) {
		atf_tc_fail("ERROR: primary not accepted %s:%d", MDL);
	}

	if (load_balance_mine(&packet, &sstate) != 1) {
		atf_tc_fail("ERROR: secondary not accepted %s:%d", MDL);
	}
	
}

ATF_TC(load_balance_swap);

ATF_TC_HEAD(load_balance_swap, tc)
{
	atf_tc_set_md_var(tc, "descr", "This test case checks that "
			  "load balancing works with byteswapping.");
}

ATF_TC_BODY(load_balance_swap, tc)
{
#if defined(SECS_BYTEORDER)
	struct packet packet;
	struct dhcp_packet raw;
	dhcp_failover_state_t pstate, sstate;
	u_int8_t hba[256];

	memset(&packet, 0, sizeof(struct packet));
	memset(&raw, 0, sizeof(struct dhcp_packet));
	packet.raw = &raw;
	raw.hlen = 1;
	raw.chaddr[0] = 14;

	memset(hba, 0xFF, 256);

	/* primary state */
	memset(&pstate, 0, sizeof(dhcp_failover_state_t));
	pstate.i_am = primary;
	pstate.load_balance_max_secs = 5;
	pstate.hba = hba;

	/* secondary state, we can reuse the hba as it doesn't change */
	memset(&sstate, 0, sizeof(dhcp_failover_state_t));
	sstate.i_am = secondary;
	sstate.load_balance_max_secs = 5;
	sstate.hba = hba;

	/* Small byteswapped timeout, primary accepted, secondary not*/
	raw.secs = htons(256);
	if (load_balance_mine(&packet, &pstate) != 1) {
		atf_tc_fail("ERROR: primary not accepted %s:%d", MDL);
	}

	if (load_balance_mine(&packet, &sstate) != 0) {
		atf_tc_fail("ERROR: secondary accepted %s:%d", MDL);
	}

	/* Large byteswapped timeout, both accepted*/
	raw.secs = htons(256 * 6);
	if (load_balance_mine(&packet, &pstate) != 1) {
		atf_tc_fail("ERROR: primary not accepted %s:%d", MDL);
	}

	if (load_balance_mine(&packet, &sstate) != 1) {
		atf_tc_fail("ERROR: secondary not accepted %s:%d", MDL);
	}
	
#else
	atf_tc_skip("SECS_BYTEORDER not defined");
#endif
}


ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, load_balance);
	ATF_TP_ADD_TC(tp, load_balance_swap);

	return (atf_no_error());
}
