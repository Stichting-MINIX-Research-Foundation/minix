/*	$NetBSD: dhc6.c,v 1.6 2014/07/12 12:09:37 spz Exp $	*/
/* dhc6.c - DHCPv6 client routines. */

/*
 * Copyright (c) 2012-2013 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 2006-2010 by Internet Systems Consortium, Inc. ("ISC")
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
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: dhc6.c,v 1.6 2014/07/12 12:09:37 spz Exp $");

#include "dhcpd.h"

#ifdef DHCPv6

struct sockaddr_in6 DHCPv6DestAddr;

/*
 * Option definition structures that are used by the software - declared
 * here once and assigned at startup to save lookups.
 */
struct option *clientid_option = NULL;
struct option *elapsed_option = NULL;
struct option *ia_na_option = NULL;
struct option *ia_ta_option = NULL;
struct option *ia_pd_option = NULL;
struct option *iaaddr_option = NULL;
struct option *iaprefix_option = NULL;
struct option *oro_option = NULL;
struct option *irt_option = NULL;

static struct dhc6_lease *dhc6_dup_lease(struct dhc6_lease *lease,
					 const char *file, int line);
static struct dhc6_ia *dhc6_dup_ia(struct dhc6_ia *ia,
				   const char *file, int line);
static struct dhc6_addr *dhc6_dup_addr(struct dhc6_addr *addr,
				       const char *file, int line);
static void dhc6_ia_destroy(struct dhc6_ia **src, const char *file, int line);
static isc_result_t dhc6_parse_ia_na(struct dhc6_ia **pia,
				     struct packet *packet,
				     struct option_state *options);
static isc_result_t dhc6_parse_ia_ta(struct dhc6_ia **pia,
				     struct packet *packet,
				     struct option_state *options);
static isc_result_t dhc6_parse_ia_pd(struct dhc6_ia **pia,
				     struct packet *packet,
				     struct option_state *options);
static isc_result_t dhc6_parse_addrs(struct dhc6_addr **paddr,
				     struct packet *packet,
				     struct option_state *options);
static isc_result_t dhc6_parse_prefixes(struct dhc6_addr **ppref,
					struct packet *packet,
					struct option_state *options);
static struct dhc6_ia *find_ia(struct dhc6_ia *head,
			       u_int16_t type, const char *id);
static struct dhc6_addr *find_addr(struct dhc6_addr *head,
				   struct iaddr *address);
static struct dhc6_addr *find_pref(struct dhc6_addr *head,
				   struct iaddr *prefix, u_int8_t plen);
void init_handler(struct packet *packet, struct client_state *client);
void info_request_handler(struct packet *packet, struct client_state *client);
void rapid_commit_handler(struct packet *packet, struct client_state *client);
void do_init6(void *input);
void do_info_request6(void *input);
void do_confirm6(void *input);
void reply_handler(struct packet *packet, struct client_state *client);
static isc_result_t dhc6_add_ia_na(struct client_state *client,
				   struct data_string *packet,
				   struct dhc6_lease *lease,
				   u_int8_t message);
static isc_result_t dhc6_add_ia_ta(struct client_state *client,
				   struct data_string *packet,
				   struct dhc6_lease *lease,
				   u_int8_t message);
static isc_result_t dhc6_add_ia_pd(struct client_state *client,
				   struct data_string *packet,
				   struct dhc6_lease *lease,
				   u_int8_t message);
static isc_boolean_t stopping_finished(void);
static void dhc6_merge_lease(struct dhc6_lease *src, struct dhc6_lease *dst);
void do_select6(void *input);
void do_refresh6(void *input);
static void do_release6(void *input);
static void start_bound(struct client_state *client);
static void start_informed(struct client_state *client);
void informed_handler(struct packet *packet, struct client_state *client);
void bound_handler(struct packet *packet, struct client_state *client);
void start_renew6(void *input);
void start_rebind6(void *input);
void do_depref(void *input);
void do_expire(void *input);
static void make_client6_options(struct client_state *client,
				 struct option_state **op,
				 struct dhc6_lease *lease, u_int8_t message);
static void script_write_params6(struct client_state *client,
				 const char *prefix,
				 struct option_state *options);
static void script_write_requested6(struct client_state *client);
static isc_boolean_t active_prefix(struct client_state *client);

static int check_timing6(struct client_state *client, u_int8_t msg_type, 
		         char *msg_str, struct dhc6_lease *lease,
		         struct data_string *ds);

extern int onetry;
extern int stateless;

/*
 * Assign DHCPv6 port numbers as a client.
 */
void
dhcpv6_client_assignments(void)
{
	struct servent *ent;
	unsigned code;

	if (path_dhclient_pid == NULL)
		path_dhclient_pid = _PATH_DHCLIENT6_PID;
	if (path_dhclient_db == NULL)
		path_dhclient_db = _PATH_DHCLIENT6_DB;

	if (local_port == 0) {
		ent = getservbyname("dhcpv6-client", "udp");
		if (ent == NULL)
			local_port = htons(546);
		else
			local_port = ent->s_port;
	}

	if (remote_port == 0) {
		ent = getservbyname("dhcpv6-server", "udp");
		if (ent == NULL)
			remote_port = htons(547);
		else
			remote_port = ent->s_port;
	}

	memset(&DHCPv6DestAddr, 0, sizeof(DHCPv6DestAddr));
	DHCPv6DestAddr.sin6_family = AF_INET6;
	DHCPv6DestAddr.sin6_port = remote_port;
	if (inet_pton(AF_INET6, All_DHCP_Relay_Agents_and_Servers,
		      &DHCPv6DestAddr.sin6_addr) <= 0) {
		log_fatal("Bad address %s", All_DHCP_Relay_Agents_and_Servers);
	}

	code = D6O_CLIENTID;
	if (!option_code_hash_lookup(&clientid_option,
				     dhcpv6_universe.code_hash, &code, 0, MDL))
		log_fatal("Unable to find the CLIENTID option definition.");

	code = D6O_ELAPSED_TIME;
	if (!option_code_hash_lookup(&elapsed_option,
				     dhcpv6_universe.code_hash, &code, 0, MDL))
		log_fatal("Unable to find the ELAPSED_TIME option definition.");

	code = D6O_IA_NA;
	if (!option_code_hash_lookup(&ia_na_option, dhcpv6_universe.code_hash,
				     &code, 0, MDL))
		log_fatal("Unable to find the IA_NA option definition.");

	code = D6O_IA_TA;
	if (!option_code_hash_lookup(&ia_ta_option, dhcpv6_universe.code_hash,
				     &code, 0, MDL))
		log_fatal("Unable to find the IA_TA option definition.");

	code = D6O_IA_PD;
	if (!option_code_hash_lookup(&ia_pd_option, dhcpv6_universe.code_hash,
				     &code, 0, MDL))
		log_fatal("Unable to find the IA_PD option definition.");

	code = D6O_IAADDR;
	if (!option_code_hash_lookup(&iaaddr_option, dhcpv6_universe.code_hash,
				     &code, 0, MDL))
		log_fatal("Unable to find the IAADDR option definition.");

	code = D6O_IAPREFIX;
	if (!option_code_hash_lookup(&iaprefix_option,
				     dhcpv6_universe.code_hash,
				     &code, 0, MDL))
		log_fatal("Unable to find the IAPREFIX option definition.");

	code = D6O_ORO;
	if (!option_code_hash_lookup(&oro_option, dhcpv6_universe.code_hash,
				     &code, 0, MDL))
		log_fatal("Unable to find the ORO option definition.");

	code = D6O_INFORMATION_REFRESH_TIME;
	if (!option_code_hash_lookup(&irt_option, dhcpv6_universe.code_hash,
				     &code, 0, MDL))
		log_fatal("Unable to find the IRT option definition.");

#ifndef __CYGWIN32__ /* XXX */
	endservent();
#endif
}

/*
 * Instead of implementing RFC3315 RAND (section 14) as a float "between"
 * -0.1 and 0.1 non-inclusive, we implement it as an integer.
 *
 * The result is expected to follow this table:
 *
 *		split range answer
 *		    - ERROR -		      base <= 0
 *		0	1   0..0	 1 <= base <= 10
 *		1	3  -1..1	11 <= base <= 20
 *		2	5  -2..2	21 <= base <= 30
 *		3	7  -3..3	31 <= base <= 40
 *		...
 *
 * XXX: For this to make sense, we really need to do timing on a
 * XXX: usec scale...we currently can assume zero for any value less than
 * XXX: 11, which are very common in early stages of transmission for most
 * XXX: messages.
 */
static TIME
dhc6_rand(TIME base)
{
	TIME rval;
	TIME range;
	TIME split;

	/*
	 * A zero or less timeout is a bad thing...we don't want to
	 * DHCP-flood anyone.
	 */
	if (base <= 0)
		log_fatal("Impossible condition at %s:%d.", MDL);

	/*
	 * The first thing we do is count how many random integers we want
	 * in either direction (best thought of as the maximum negative
	 * integer, as we will subtract this potentially from a random 0).
	 */
	split = (base - 1) / 10;

	/* Don't bother with the rest of the math if we know we'll get 0. */
	if (split == 0)
		return 0;

	/*
	 * Then we count the total number of integers in this set.  This
	 * is twice the number of integers in positive and negative
	 * directions, plus zero (-1, 0, 1 is 3, -2..2 adds 2 to 5, so forth).
	 */
	range = (split * 2) + 1;

	/* Take a random number from [0..(range-1)]. */
	rval = random();
	rval %= range;

	/* Offset it to uncover potential negative values. */
	rval -= split;

	return rval;
}

/* Initialize message exchange timers (set RT from Initial-RT). */
static void
dhc6_retrans_init(struct client_state *client)
{
	int xid;

	/* Initialize timers. */
	client->txcount = 0;
	client->RT = client->IRT + dhc6_rand(client->IRT);

	/* Generate a new random 24-bit transaction ID for this exchange. */

#if (RAND_MAX >= 0x00ffffff)
	xid = random();
#elif (RAND_MAX >= 0x0000ffff)
	xid = (random() << 16) ^ random();
#elif (RAND_MAX >= 0x000000ff)
	xid = (random() << 16) ^ (random() << 8) ^ random();
#else
# error "Random number generator of less than 8 bits not supported."
#endif

	client->dhcpv6_transaction_id[0] = (xid >> 16) & 0xff;
	client->dhcpv6_transaction_id[1] = (xid >>  8) & 0xff;
	client->dhcpv6_transaction_id[2] =  xid        & 0xff;
}

/* Advance the DHCPv6 retransmission state once. */
static void
dhc6_retrans_advance(struct client_state *client)
{
	struct timeval elapsed;

	/* elapsed = cur - start */
	elapsed.tv_sec = cur_tv.tv_sec - client->start_time.tv_sec;
	elapsed.tv_usec = cur_tv.tv_usec - client->start_time.tv_usec;
	if (elapsed.tv_usec < 0) {
		elapsed.tv_sec -= 1;
		elapsed.tv_usec += 1000000;
	}
	/* retrans_advance is called after consuming client->RT. */
	/* elapsed += RT */
	elapsed.tv_sec += client->RT / 100;
	elapsed.tv_usec += (client->RT % 100) * 10000;
	if (elapsed.tv_usec >= 1000000) {
		elapsed.tv_sec += 1;
		elapsed.tv_usec -= 1000000;
	}

	/*
	 * RT for each subsequent message transmission is based on the previous
	 * value of RT:
	 *
	 *    RT = 2*RTprev + RAND*RTprev
	 */
	client->RT += client->RT + dhc6_rand(client->RT);

	/*
	 * MRT specifies an upper bound on the value of RT (disregarding the
	 * randomization added by the use of RAND).  If MRT has a value of 0,
	 * there is no upper limit on the value of RT.  Otherwise:
	 *
	 *    if (RT > MRT)
	 *       RT = MRT + RAND*MRT
	 */
	if ((client->MRT != 0) && (client->RT > client->MRT))
		client->RT = client->MRT + dhc6_rand(client->MRT);

	/*
	 * Further, if there's an MRD, we should wake up upon reaching
	 * the MRD rather than at some point after it.
	 */
	if (client->MRD == 0) {
		/* Done. */
		client->txcount++;
		return;
	}
	/* elapsed += client->RT */
	elapsed.tv_sec += client->RT / 100;
	elapsed.tv_usec += (client->RT % 100) * 10000;
	if (elapsed.tv_usec >= 1000000) {
		elapsed.tv_sec += 1;
		elapsed.tv_usec -= 1000000;
	}
	if (elapsed.tv_sec >= client->MRD) {
		/*
		 * wake at RT + cur = start + MRD
		 */
		client->RT = client->MRD +
			(client->start_time.tv_sec - cur_tv.tv_sec);
		client->RT = client->RT * 100 +
			(client->start_time.tv_usec - cur_tv.tv_usec) / 10000;
	}
	client->txcount++;
}

/* Quick validation of DHCPv6 ADVERTISE packet contents. */
static int
valid_reply(struct packet *packet, struct client_state *client)
{
	struct data_string sid, cid;
	struct option_cache *oc;
	int rval = ISC_TRUE;

	memset(&sid, 0, sizeof(sid));
	memset(&cid, 0, sizeof(cid));

	if (!lookup_option(&dhcpv6_universe, packet->options, D6O_SERVERID)) {
		log_error("Response without a server identifier received.");
		rval = ISC_FALSE;
	}

	oc = lookup_option(&dhcpv6_universe, packet->options, D6O_CLIENTID);
	if (!oc ||
	    !evaluate_option_cache(&sid, packet, NULL, client, packet->options,
				   client->sent_options, &global_scope, oc,
				   MDL)) {
		log_error("Response without a client identifier.");
		rval = ISC_FALSE;
	}

	oc = lookup_option(&dhcpv6_universe, client->sent_options,
			   D6O_CLIENTID);
	if (!oc ||
	    !evaluate_option_cache(&cid, packet, NULL, client,
				   client->sent_options, NULL, &global_scope,
				   oc, MDL)) {
		log_error("Local client identifier is missing!");
		rval = ISC_FALSE;
	}

	if (sid.len == 0 ||
	    sid.len != cid.len ||
	    memcmp(sid.data, cid.data, sid.len)) {
		log_error("Advertise with matching transaction ID, but "
			  "mismatching client id.");
		rval = ISC_FALSE;
	}

	return rval;
}

/*
 * Create a complete copy of a DHCPv6 lease structure.
 */
static struct dhc6_lease *
dhc6_dup_lease(struct dhc6_lease *lease, const char *file, int line)
{
	struct dhc6_lease *copy;
	struct dhc6_ia **insert_ia, *ia;

	copy = dmalloc(sizeof(*copy), file, line);
	if (copy == NULL) {
		log_error("Out of memory for v6 lease structure.");
		return NULL;
	}

	data_string_copy(&copy->server_id, &lease->server_id, file, line);
	copy->pref = lease->pref;

	memcpy(copy->dhcpv6_transaction_id, lease->dhcpv6_transaction_id,
	       sizeof(copy->dhcpv6_transaction_id));

	option_state_reference(&copy->options, lease->options, file, line);

	insert_ia = &copy->bindings;
	for (ia = lease->bindings ; ia != NULL ; ia = ia->next) {
		*insert_ia = dhc6_dup_ia(ia, file, line);

		if (*insert_ia == NULL) {
			dhc6_lease_destroy(&copy, file, line);
			return NULL;
		}

		insert_ia = &(*insert_ia)->next;
	}

	return copy;
}

/*
 * Duplicate an IA structure.
 */
static struct dhc6_ia *
dhc6_dup_ia(struct dhc6_ia *ia, const char *file, int line)
{
	struct dhc6_ia *copy;
	struct dhc6_addr **insert_addr, *addr;

	copy = dmalloc(sizeof(*ia), file, line);

	memcpy(copy->iaid, ia->iaid, sizeof(copy->iaid));

	copy->ia_type = ia->ia_type;
	copy->starts = ia->starts;
	copy->renew = ia->renew;
	copy->rebind = ia->rebind;

	insert_addr = &copy->addrs;
	for (addr = ia->addrs ; addr != NULL ; addr = addr->next) {
		*insert_addr = dhc6_dup_addr(addr, file, line);

		if (*insert_addr == NULL) {
			dhc6_ia_destroy(&copy, file, line);
			return NULL;
		}

		insert_addr = &(*insert_addr)->next;
	}

	if (ia->options != NULL)
		option_state_reference(&copy->options, ia->options,
				       file, line);

	return copy;
}

/*
 * Duplicate an IAADDR or IAPREFIX structure.
 */
static struct dhc6_addr *
dhc6_dup_addr(struct dhc6_addr *addr, const char *file, int line)
{
	struct dhc6_addr *copy;

	copy = dmalloc(sizeof(*addr), file, line);

	if (copy == NULL)
		return NULL;

	memcpy(&copy->address, &addr->address, sizeof(copy->address));

	copy->plen = addr->plen;
	copy->flags = addr->flags;
	copy->starts = addr->starts;
	copy->preferred_life = addr->preferred_life;
	copy->max_life = addr->max_life;

	if (addr->options != NULL)
		option_state_reference(&copy->options, addr->options,
				       file, line);

	return copy;
}

/*
 * Form a DHCPv6 lease structure based upon packet contents.  Creates and
 * populates IA's and any IAADDR/IAPREFIX's they contain.
 * Parsed options are deleted in order to not save them in the lease file.
 */
static struct dhc6_lease *
dhc6_leaseify(struct packet *packet)
{
	struct data_string ds;
	struct dhc6_lease *lease;
	struct option_cache *oc;

	lease = dmalloc(sizeof(*lease), MDL);
	if (lease == NULL) {
		log_error("Out of memory for v6 lease structure.");
		return NULL;
	}

	memcpy(lease->dhcpv6_transaction_id, packet->dhcpv6_transaction_id, 3);
	option_state_reference(&lease->options, packet->options, MDL);

	memset(&ds, 0, sizeof(ds));

	/* Determine preference (default zero). */
	oc = lookup_option(&dhcpv6_universe, lease->options, D6O_PREFERENCE);
	if (oc &&
	    evaluate_option_cache(&ds, packet, NULL, NULL, lease->options,
				  NULL, &global_scope, oc, MDL)) {
		if (ds.len != 1) {
			log_error("Invalid length of DHCPv6 Preference option "
				  "(%d != 1)", ds.len);
			data_string_forget(&ds, MDL);
			dhc6_lease_destroy(&lease, MDL);
			return NULL;
		} else {
			lease->pref = ds.data[0];
			log_debug("RCV:  X-- Preference %u.",
				  (unsigned)lease->pref);
		}

		data_string_forget(&ds, MDL);
	}
	delete_option(&dhcpv6_universe, lease->options, D6O_PREFERENCE);

	/*
	 * Dig into recursive DHCPv6 pockets for IA_NA and contained IAADDR
	 * options.
	 */
	if (dhc6_parse_ia_na(&lease->bindings, packet,
			     lease->options) != ISC_R_SUCCESS) {
		/* Error conditions are logged by the caller. */
		dhc6_lease_destroy(&lease, MDL);
		return NULL;
	}
	/*
	 * Dig into recursive DHCPv6 pockets for IA_TA and contained IAADDR
	 * options.
	 */
	if (dhc6_parse_ia_ta(&lease->bindings, packet,
			     lease->options) != ISC_R_SUCCESS) {
		/* Error conditions are logged by the caller. */
		dhc6_lease_destroy(&lease, MDL);
		return NULL;
	}
	/*
	 * Dig into recursive DHCPv6 pockets for IA_PD and contained IAPREFIX
	 * options.
	 */
	if (dhc6_parse_ia_pd(&lease->bindings, packet,
			     lease->options) != ISC_R_SUCCESS) {
		/* Error conditions are logged by the caller. */
		dhc6_lease_destroy(&lease, MDL);
		return NULL;
	}

	/*
	 * This is last because in the future we may want to make a different
	 * key based upon additional information from the packet (we may need
	 * to allow multiple leases in one client state per server, but we're
	 * not sure based on what additional keys now).
	 */
	oc = lookup_option(&dhcpv6_universe, packet->options, D6O_SERVERID);
	if ((oc == NULL) ||
	    !evaluate_option_cache(&lease->server_id, packet, NULL, NULL,
				   lease->options, NULL, &global_scope,
				   oc, MDL) ||
	    lease->server_id.len == 0) {
		/* This should be impossible due to validation checks earlier.
		 */
		log_error("Invalid SERVERID option cache.");
		dhc6_lease_destroy(&lease, MDL);
		return NULL;
	} else {
		log_debug("RCV:  X-- Server ID: %s",
			  print_hex_1(lease->server_id.len,
				      lease->server_id.data, 52));
	}

	return lease;
}

static isc_result_t
dhc6_parse_ia_na(struct dhc6_ia **pia, struct packet *packet,
		 struct option_state *options)
{
	struct data_string ds;
	struct dhc6_ia *ia;
	struct option_cache *oc;
	isc_result_t result;

	memset(&ds, 0, sizeof(ds));

	oc = lookup_option(&dhcpv6_universe, options, D6O_IA_NA);
	for ( ; oc != NULL ; oc = oc->next) {
		ia = dmalloc(sizeof(*ia), MDL);
		if (ia == NULL) {
			log_error("Out of memory allocating IA_NA structure.");
			return ISC_R_NOMEMORY;
		} else if (evaluate_option_cache(&ds, packet, NULL, NULL,
						 options, NULL,
						 &global_scope, oc, MDL) &&
			   ds.len >= 12) {
			memcpy(ia->iaid, ds.data, 4);
			ia->ia_type = D6O_IA_NA;
			ia->starts = cur_time;
			ia->renew = getULong(ds.data + 4);
			ia->rebind = getULong(ds.data + 8);

			log_debug("RCV:  X-- IA_NA %s",
				  print_hex_1(4, ia->iaid, 59));
			/* XXX: This should be the printed time I think. */
			log_debug("RCV:  | X-- starts %u",
				  (unsigned)ia->starts);
			log_debug("RCV:  | X-- t1 - renew  +%u", ia->renew);
			log_debug("RCV:  | X-- t2 - rebind +%u", ia->rebind);

			/*
			 * RFC3315 section 22.4, discard IA_NA's that
			 * have t1 greater than t2, and both not zero.
			 * Since RFC3315 defines this behaviour, it is not
			 * an error - just normal operation.
			 *
			 * Note that RFC3315 says we MUST honor these values
			 * if they are not zero.  So insane values are
			 * totally OK.
			 */
			if ((ia->renew > 0) && (ia->rebind > 0) &&
			    (ia->renew > ia->rebind)) {
				log_debug("RCV:  | !-- INVALID renew/rebind "
					  "times, IA_NA discarded.");
				dfree(ia, MDL);
				data_string_forget(&ds, MDL);
				continue;
			}

			if (ds.len > 12) {
				log_debug("RCV:  | X-- [Options]");

				if (!option_state_allocate(&ia->options,
							   MDL)) {
					log_error("Out of memory allocating "
						  "IA_NA option state.");
					dfree(ia, MDL);
					data_string_forget(&ds, MDL);
					return ISC_R_NOMEMORY;
				}

				if (!parse_option_buffer(ia->options,
							 ds.data + 12,
							 ds.len - 12,
							 &dhcpv6_universe)) {
					log_error("Corrupt IA_NA options.");
					option_state_dereference(&ia->options,
								 MDL);
					dfree(ia, MDL);
					data_string_forget(&ds, MDL);
					return DHCP_R_BADPARSE;
				}
			}
			data_string_forget(&ds, MDL);

			if (ia->options != NULL) {
				result = dhc6_parse_addrs(&ia->addrs, packet,
							  ia->options);
				if (result != ISC_R_SUCCESS) {
					option_state_dereference(&ia->options,
								 MDL);
					dfree(ia, MDL);
					return result;
				}
			}

			while (*pia != NULL)
				pia = &(*pia)->next;
			*pia = ia;
			pia = &ia->next;
		} else {
			log_error("Invalid IA_NA option cache.");
			dfree(ia, MDL);
			if (ds.len != 0)
				data_string_forget(&ds, MDL);
			return ISC_R_UNEXPECTED;
		}
	}
	delete_option(&dhcpv6_universe, options, D6O_IA_NA);

	return ISC_R_SUCCESS;
}

static isc_result_t
dhc6_parse_ia_ta(struct dhc6_ia **pia, struct packet *packet,
		 struct option_state *options)
{
	struct data_string ds;
	struct dhc6_ia *ia;
	struct option_cache *oc;
	isc_result_t result;

	memset(&ds, 0, sizeof(ds));

	oc = lookup_option(&dhcpv6_universe, options, D6O_IA_TA);
	for ( ; oc != NULL ; oc = oc->next) {
		ia = dmalloc(sizeof(*ia), MDL);
		if (ia == NULL) {
			log_error("Out of memory allocating IA_TA structure.");
			return ISC_R_NOMEMORY;
		} else if (evaluate_option_cache(&ds, packet, NULL, NULL,
						 options, NULL,
						 &global_scope, oc, MDL) &&
			   ds.len >= 4) {
			memcpy(ia->iaid, ds.data, 4);
			ia->ia_type = D6O_IA_TA;
			ia->starts = cur_time;

			log_debug("RCV:  X-- IA_TA %s",
				  print_hex_1(4, ia->iaid, 59));
			/* XXX: This should be the printed time I think. */
			log_debug("RCV:  | X-- starts %u",
				  (unsigned)ia->starts);

			if (ds.len > 4) {
				log_debug("RCV:  | X-- [Options]");

				if (!option_state_allocate(&ia->options,
							   MDL)) {
					log_error("Out of memory allocating "
						  "IA_TA option state.");
					dfree(ia, MDL);
					data_string_forget(&ds, MDL);
					return ISC_R_NOMEMORY;
				}

				if (!parse_option_buffer(ia->options,
							 ds.data + 4,
							 ds.len - 4,
							 &dhcpv6_universe)) {
					log_error("Corrupt IA_TA options.");
					option_state_dereference(&ia->options,
								 MDL);
					dfree(ia, MDL);
					data_string_forget(&ds, MDL);
					return DHCP_R_BADPARSE;
				}
			}
			data_string_forget(&ds, MDL);

			if (ia->options != NULL) {
				result = dhc6_parse_addrs(&ia->addrs, packet,
							  ia->options);
				if (result != ISC_R_SUCCESS) {
					option_state_dereference(&ia->options,
								 MDL);
					dfree(ia, MDL);
					return result;
				}
			}

			while (*pia != NULL)
				pia = &(*pia)->next;
			*pia = ia;
			pia = &ia->next;
		} else {
			log_error("Invalid IA_TA option cache.");
			dfree(ia, MDL);
			if (ds.len != 0)
				data_string_forget(&ds, MDL);
			return ISC_R_UNEXPECTED;
		}
	}
	delete_option(&dhcpv6_universe, options, D6O_IA_TA);

	return ISC_R_SUCCESS;
}

static isc_result_t
dhc6_parse_ia_pd(struct dhc6_ia **pia, struct packet *packet,
		 struct option_state *options)
{
	struct data_string ds;
	struct dhc6_ia *ia;
	struct option_cache *oc;
	isc_result_t result;

	memset(&ds, 0, sizeof(ds));

	oc = lookup_option(&dhcpv6_universe, options, D6O_IA_PD);
	for ( ; oc != NULL ; oc = oc->next) {
		ia = dmalloc(sizeof(*ia), MDL);
		if (ia == NULL) {
			log_error("Out of memory allocating IA_PD structure.");
			return ISC_R_NOMEMORY;
		} else if (evaluate_option_cache(&ds, packet, NULL, NULL,
						 options, NULL,
						 &global_scope, oc, MDL) &&
			   ds.len >= 12) {
			memcpy(ia->iaid, ds.data, 4);
			ia->ia_type = D6O_IA_PD;
			ia->starts = cur_time;
			ia->renew = getULong(ds.data + 4);
			ia->rebind = getULong(ds.data + 8);

			log_debug("RCV:  X-- IA_PD %s",
				  print_hex_1(4, ia->iaid, 59));
			/* XXX: This should be the printed time I think. */
			log_debug("RCV:  | X-- starts %u",
				  (unsigned)ia->starts);
			log_debug("RCV:  | X-- t1 - renew  +%u", ia->renew);
			log_debug("RCV:  | X-- t2 - rebind +%u", ia->rebind);

			/*
			 * RFC3633 section 9, discard IA_PD's that
			 * have t1 greater than t2, and both not zero.
			 * Since RFC3633 defines this behaviour, it is not
			 * an error - just normal operation.
			 */
			if ((ia->renew > 0) && (ia->rebind > 0) &&
			    (ia->renew > ia->rebind)) {
				log_debug("RCV:  | !-- INVALID renew/rebind "
					  "times, IA_PD discarded.");
				dfree(ia, MDL);
				data_string_forget(&ds, MDL);
				continue;
			}

			if (ds.len > 12) {
				log_debug("RCV:  | X-- [Options]");

				if (!option_state_allocate(&ia->options,
							   MDL)) {
					log_error("Out of memory allocating "
						  "IA_PD option state.");
					dfree(ia, MDL);
					data_string_forget(&ds, MDL);
					return ISC_R_NOMEMORY;
				}

				if (!parse_option_buffer(ia->options,
							 ds.data + 12,
							 ds.len - 12,
							 &dhcpv6_universe)) {
					log_error("Corrupt IA_PD options.");
					option_state_dereference(&ia->options,
								 MDL);
					dfree(ia, MDL);
					data_string_forget(&ds, MDL);
					return DHCP_R_BADPARSE;
				}
			}
			data_string_forget(&ds, MDL);

			if (ia->options != NULL) {
				result = dhc6_parse_prefixes(&ia->addrs,
							     packet,
							     ia->options);
				if (result != ISC_R_SUCCESS) {
					option_state_dereference(&ia->options,
								 MDL);
					dfree(ia, MDL);
					return result;
				}
			}

			while (*pia != NULL)
				pia = &(*pia)->next;
			*pia = ia;
			pia = &ia->next;
		} else {
			log_error("Invalid IA_PD option cache.");
			dfree(ia, MDL);
			if (ds.len != 0)
				data_string_forget(&ds, MDL);
			return ISC_R_UNEXPECTED;
		}
	}
	delete_option(&dhcpv6_universe, options, D6O_IA_PD);

	return ISC_R_SUCCESS;
}


static isc_result_t
dhc6_parse_addrs(struct dhc6_addr **paddr, struct packet *packet,
		 struct option_state *options)
{
	struct data_string ds;
	struct option_cache *oc;
	struct dhc6_addr *addr;

	memset(&ds, 0, sizeof(ds));

	oc = lookup_option(&dhcpv6_universe, options, D6O_IAADDR);
	for ( ; oc != NULL ; oc = oc->next) {
		addr = dmalloc(sizeof(*addr), MDL);
		if (addr == NULL) {
			log_error("Out of memory allocating "
				  "address structure.");
			return ISC_R_NOMEMORY;
		} else if (evaluate_option_cache(&ds, packet, NULL, NULL,
						 options, NULL, &global_scope,
						 oc, MDL) &&
			   (ds.len >= 24)) {

			addr->address.len = 16;
			memcpy(addr->address.iabuf, ds.data, 16);
			addr->starts = cur_time;
			addr->preferred_life = getULong(ds.data + 16);
			addr->max_life = getULong(ds.data + 20);

			log_debug("RCV:  | | X-- IAADDR %s",
				  piaddr(addr->address));
			log_debug("RCV:  | | | X-- Preferred lifetime %u.",
				  addr->preferred_life);
			log_debug("RCV:  | | | X-- Max lifetime %u.",
				  addr->max_life);

			/*
			 * RFC 3315 section 22.6 says we must discard
			 * addresses whose pref is later than valid.
			 */
			if ((addr->preferred_life > addr->max_life)) {
				log_debug("RCV:  | | | !-- INVALID lifetimes, "
					  "IAADDR discarded.  Check your "
					  "server configuration.");
				dfree(addr, MDL);
				data_string_forget(&ds, MDL);
				continue;
			}

			/*
			 * Fortunately this is the last recursion in the
			 * protocol.
			 */
			if (ds.len > 24) {
				if (!option_state_allocate(&addr->options,
							   MDL)) {
					log_error("Out of memory allocating "
						  "IAADDR option state.");
					dfree(addr, MDL);
					data_string_forget(&ds, MDL);
					return ISC_R_NOMEMORY;
				}

				if (!parse_option_buffer(addr->options,
							 ds.data + 24,
							 ds.len - 24,
							 &dhcpv6_universe)) {
					log_error("Corrupt IAADDR options.");
					option_state_dereference(&addr->options,
								 MDL);
					dfree(addr, MDL);
					data_string_forget(&ds, MDL);
					return DHCP_R_BADPARSE;
				}
			}

			if (addr->options != NULL)
				log_debug("RCV:  | | | X-- "
					  "[Options]");

			data_string_forget(&ds, MDL);

			*paddr = addr;
			paddr = &addr->next;
		} else {
			log_error("Invalid IAADDR option cache.");
			dfree(addr, MDL);
			if (ds.len != 0)
				data_string_forget(&ds, MDL);
			return ISC_R_UNEXPECTED;
		}
	}
	delete_option(&dhcpv6_universe, options, D6O_IAADDR);

	return ISC_R_SUCCESS;
}

static isc_result_t
dhc6_parse_prefixes(struct dhc6_addr **ppfx, struct packet *packet,
		    struct option_state *options)
{
	struct data_string ds;
	struct option_cache *oc;
	struct dhc6_addr *pfx;

	memset(&ds, 0, sizeof(ds));

	oc = lookup_option(&dhcpv6_universe, options, D6O_IAPREFIX);
	for ( ; oc != NULL ; oc = oc->next) {
		pfx = dmalloc(sizeof(*pfx), MDL);
		if (pfx == NULL) {
			log_error("Out of memory allocating "
				  "prefix structure.");
			return ISC_R_NOMEMORY;
		} else if (evaluate_option_cache(&ds, packet, NULL, NULL,
						 options, NULL, &global_scope,
						 oc, MDL) &&
			   (ds.len >= 25)) {

			pfx->preferred_life = getULong(ds.data);
			pfx->max_life = getULong(ds.data + 4);
			pfx->plen = getUChar(ds.data + 8);
			pfx->address.len = 16;
			memcpy(pfx->address.iabuf, ds.data + 9, 16);
			pfx->starts = cur_time;

			log_debug("RCV:  | | X-- IAPREFIX %s/%d",
				  piaddr(pfx->address), (int)pfx->plen);
			log_debug("RCV:  | | | X-- Preferred lifetime %u.",
				  pfx->preferred_life);
			log_debug("RCV:  | | | X-- Max lifetime %u.",
				  pfx->max_life);

			/* Sanity check over the prefix length */
			if ((pfx->plen < 4) || (pfx->plen > 128)) {
				log_debug("RCV:  | | | !-- INVALID prefix "
					  "length, IAPREFIX discarded.  "
					  "Check your server configuration.");
				dfree(pfx, MDL);
				data_string_forget(&ds, MDL);
				continue;
			}
			/*
			 * RFC 3633 section 10 says we must discard
			 * prefixes whose pref is later than valid.
			 */
			if ((pfx->preferred_life > pfx->max_life)) {
				log_debug("RCV:  | | | !-- INVALID lifetimes, "
					  "IAPREFIX discarded.  Check your "
					  "server configuration.");
				dfree(pfx, MDL);
				data_string_forget(&ds, MDL);
				continue;
			}

			/*
			 * Fortunately this is the last recursion in the
			 * protocol.
			 */
			if (ds.len > 25) {
				if (!option_state_allocate(&pfx->options,
							   MDL)) {
					log_error("Out of memory allocating "
						  "IAPREFIX option state.");
					dfree(pfx, MDL);
					data_string_forget(&ds, MDL);
					return ISC_R_NOMEMORY;
				}

				if (!parse_option_buffer(pfx->options,
							 ds.data + 25,
							 ds.len - 25,
							 &dhcpv6_universe)) {
					log_error("Corrupt IAPREFIX options.");
					option_state_dereference(&pfx->options,
								 MDL);
					dfree(pfx, MDL);
					data_string_forget(&ds, MDL);
					return DHCP_R_BADPARSE;
				}
			}

			if (pfx->options != NULL)
				log_debug("RCV:  | | | X-- "
					  "[Options]");

			data_string_forget(&ds, MDL);

			*ppfx = pfx;
			ppfx = &pfx->next;
		} else {
			log_error("Invalid IAPREFIX option cache.");
			dfree(pfx, MDL);
			if (ds.len != 0)
				data_string_forget(&ds, MDL);
			return ISC_R_UNEXPECTED;
		}
	}
	delete_option(&dhcpv6_universe, options, D6O_IAPREFIX);

	return ISC_R_SUCCESS;
}

/* Clean up a lease object, deallocate all its parts, and set it to NULL. */
void
dhc6_lease_destroy(struct dhc6_lease **src, const char *file, int line)
{
	struct dhc6_ia *ia, *nia;
	struct dhc6_lease *lease;

	if (src == NULL || *src == NULL) {
		log_error("Attempt to destroy null lease.");
		return;
	}
	lease = *src;

	if (lease->server_id.len != 0)
		data_string_forget(&lease->server_id, file, line);

	for (ia = lease->bindings ; ia != NULL ; ia = nia) {
		nia = ia->next;

		dhc6_ia_destroy(&ia, file, line);
	}

	if (lease->options != NULL)
		option_state_dereference(&lease->options, file, line);

	dfree(lease, file, line);
	*src = NULL;
}

/*
 * Traverse the addresses list, and destroy their contents, and NULL the
 * list pointer.
 */
static void
dhc6_ia_destroy(struct dhc6_ia **src, const char *file, int line)
{
	struct dhc6_addr *addr, *naddr;
	struct dhc6_ia *ia;

	if (src == NULL || *src == NULL) {
		log_error("Attempt to destroy null IA.");
		return;
	}
	ia = *src;

	for (addr = ia->addrs ; addr != NULL ; addr = naddr) {
		naddr = addr->next;

		if (addr->options != NULL)
			option_state_dereference(&addr->options, file, line);

		dfree(addr, file, line);
	}

	if (ia->options != NULL)
		option_state_dereference(&ia->options, file, line);

	dfree(ia, file, line);
	*src = NULL;
}

/*
 * For a given lease, insert it into the tail of the lease list.  Upon
 * finding a duplicate by server id, remove it and take over its position.
 */
static void
insert_lease(struct dhc6_lease **head, struct dhc6_lease *new)
{
	while (*head != NULL) {
		if ((*head)->server_id.len == new->server_id.len &&
		    memcmp((*head)->server_id.data, new->server_id.data,
			   new->server_id.len) == 0) {
			new->next = (*head)->next;
			dhc6_lease_destroy(head, MDL);
			break;
		}

		head= &(*head)->next;
	}

	*head = new;
	return;
}

/*
 * Not really clear what to do here yet.
 */
static int
dhc6_score_lease(struct client_state *client, struct dhc6_lease *lease)
{
	struct dhc6_ia *ia;
	struct dhc6_addr *addr;
	struct option **req;
	int i;

	if (lease->score)
		return lease->score;

	lease->score = 1;

	/* If this lease lacks a required option, dump it. */
	/* XXX: we should be able to cache the failure... */
	req = client->config->required_options;
	if (req != NULL) {
		for (i = 0 ; req[i] != NULL ; i++) {
			if (lookup_option(&dhcpv6_universe, lease->options,
					  req[i]->code) == NULL) {
				lease->score = 0;
				return lease->score;
			}
		}
	}

	/* If this lease contains a requested option, improve its score. */
	req = client->config->requested_options;
	if (req != NULL) {
		for (i = 0 ; req[i] != NULL ; i++) {
			if (lookup_option(&dhcpv6_universe, lease->options,
					  req[i]->code) != NULL)
				lease->score++;
		}
	}

	for (ia = lease->bindings ; ia != NULL ; ia = ia->next) {
		lease->score += 50;

		for (addr = ia->addrs ; addr != NULL ; addr = addr->next) {
			lease->score += 100;
		}
	}

	return lease->score;
}

/*
 * start_init6() kicks off the process, transmitting a packet and
 * scheduling a retransmission event.
 */
void
start_init6(struct client_state *client)
{
	struct timeval tv;

	log_debug("PRC: Soliciting for leases (INIT).");
	client->state = S_INIT;

	/* Initialize timers, RFC3315 section 17.1.2. */
	client->IRT = SOL_TIMEOUT * 100;
	client->MRT = SOL_MAX_RT * 100;
	client->MRC = 0;
	/* Default is 0 (no max) but -1 changes this. */
	if (!onetry)
		client->MRD = 0;
	else
		client->MRD = client->config->timeout;

	dhc6_retrans_init(client);

	/*
	 * RFC3315 section 17.1.2 goes out of its way:
	 * Also, the first RT MUST be selected to be strictly greater than IRT
	 * by choosing RAND to be strictly greater than 0.
	 */
	/* if RAND < 0 then RAND = -RAND */
	if (client->RT <= client->IRT)
		client->RT = client->IRT + (client->IRT - client->RT);
	/* if RAND == 0 then RAND = 1 */
	if (client->RT <= client->IRT)
		client->RT = client->IRT + 1;

	client->v6_handler = init_handler;

	/*
	 * RFC3315 section 17.1.2 says we MUST start the first packet
	 * between 0 and SOL_MAX_DELAY seconds.  The good news is
	 * SOL_MAX_DELAY is 1.
	 */
	tv.tv_sec = cur_tv.tv_sec;
	tv.tv_usec = cur_tv.tv_usec;
	tv.tv_usec += (random() % (SOL_MAX_DELAY * 100)) * 10000;
	if (tv.tv_usec >= 1000000) {
		tv.tv_sec += 1;
		tv.tv_usec -= 1000000;
	}
	add_timeout(&tv, do_init6, client, NULL, NULL);

	if (nowait)
		finish_daemon();
}

/*
 * start_info_request6() kicks off the process, transmitting an info
 * request packet and scheduling a retransmission event.
 */
void
start_info_request6(struct client_state *client)
{
	struct timeval tv;

	log_debug("PRC: Requesting information (INIT).");
	client->state = S_INIT;

	/* Initialize timers, RFC3315 section 18.1.5. */
	client->IRT = INF_TIMEOUT * 100;
	client->MRT = INF_MAX_RT * 100;
	client->MRC = 0;
	/* Default is 0 (no max) but -1 changes this. */
	if (!onetry)
		client->MRD = 0;
	else
		client->MRD = client->config->timeout;

	dhc6_retrans_init(client);

	client->v6_handler = info_request_handler;

	/*
	 * RFC3315 section 18.1.5 says we MUST start the first packet
	 * between 0 and INF_MAX_DELAY seconds.  The good news is
	 * INF_MAX_DELAY is 1.
	 */
	tv.tv_sec = cur_tv.tv_sec;
	tv.tv_usec = cur_tv.tv_usec;
	tv.tv_usec += (random() % (INF_MAX_DELAY * 100)) * 10000;
	if (tv.tv_usec >= 1000000) {
		tv.tv_sec += 1;
		tv.tv_usec -= 1000000;
	}
	add_timeout(&tv, do_info_request6, client, NULL, NULL);

	if (nowait)
		go_daemon();
}

/*
 * start_confirm6() kicks off an "init-reboot" version of the process, at
 * startup to find out if old bindings are 'fair' and at runtime whenever
 * a link cycles state we'll eventually want to do this.
 */
void
start_confirm6(struct client_state *client)
{
	struct timeval tv;

	/* If there is no active lease, there is nothing to check. */
	if ((client->active_lease == NULL) ||
	    !active_prefix(client) ||
	    client->active_lease->released) {
		start_init6(client);
		return;
	}

	log_debug("PRC: Confirming active lease (INIT-REBOOT).");
	client->state = S_REBOOTING;

	/* Initialize timers, RFC3315 section 17.1.3. */
	client->IRT = CNF_TIMEOUT * 100;
	client->MRT = CNF_MAX_RT * 100;
	client->MRC = 0;
	client->MRD = CNF_MAX_RD;

	dhc6_retrans_init(client);

	client->v6_handler = reply_handler;

	/*
	 * RFC3315 section 18.1.2 says we MUST start the first packet
	 * between 0 and CNF_MAX_DELAY seconds.  The good news is
	 * CNF_MAX_DELAY is 1.
	 */
	tv.tv_sec = cur_tv.tv_sec;
	tv.tv_usec = cur_tv.tv_usec;
	tv.tv_usec += (random() % (CNF_MAX_DELAY * 100)) * 10000;
	if (tv.tv_usec >= 1000000) {
		tv.tv_sec += 1;
		tv.tv_usec -= 1000000;
	}
	if (wanted_ia_pd != 0) {
		client->state = S_REBINDING;
		client->refresh_type = DHCPV6_REBIND;
		add_timeout(&tv, do_refresh6, client, NULL, NULL);
	} else
		add_timeout(&tv, do_confirm6, client, NULL, NULL);
}

/*
 * check_timing6() check on the timing for sending a v6 message
 * and then do the basic initialization for a v6 message.
 */
#define CHK_TIM_SUCCESS		0
#define CHK_TIM_MRC_EXCEEDED	1
#define CHK_TIM_MRD_EXCEEDED	2
#define CHK_TIM_ALLOC_FAILURE	3

int
check_timing6 (struct client_state *client, u_int8_t msg_type, 
	       char *msg_str, struct dhc6_lease *lease,
	       struct data_string *ds)
{
	struct timeval elapsed;

	/*
	 * Start_time starts at the first transmission.
	 */
	if (client->txcount == 0) {
		client->start_time.tv_sec = cur_tv.tv_sec;
		client->start_time.tv_usec = cur_tv.tv_usec;
	} else if ((client->MRC != 0) && (client->txcount > client->MRC)) {
		log_info("Max retransmission count exceeded.");
		return(CHK_TIM_MRC_EXCEEDED);
	}

	/* elapsed = cur - start */
	elapsed.tv_sec = cur_tv.tv_sec - client->start_time.tv_sec;
	elapsed.tv_usec = cur_tv.tv_usec - client->start_time.tv_usec;
	if (elapsed.tv_usec < 0) {
		elapsed.tv_sec -= 1;
		elapsed.tv_usec += 1000000;
	}

	/* Check if finished (-1 argument). */
	if ((client->MRD != 0) && (elapsed.tv_sec > client->MRD)) {
		log_info("Max retransmission duration exceeded.");
		return(CHK_TIM_MRD_EXCEEDED);
	}

	memset(ds, 0, sizeof(*ds));
	if (!buffer_allocate(&(ds->buffer), 4, MDL)) {
		log_error("Unable to allocate memory for %s.", msg_str);
		return(CHK_TIM_ALLOC_FAILURE);
	}
	ds->data = ds->buffer->data;
	ds->len = 4;

	ds->buffer->data[0] = msg_type;
	memcpy(ds->buffer->data + 1, client->dhcpv6_transaction_id, 3);

	/* Form an elapsed option. */
	/* Maximum value is 65535 1/100s coded as 0xffff. */
	if ((elapsed.tv_sec < 0) || (elapsed.tv_sec > 655) ||
	    ((elapsed.tv_sec == 655) && (elapsed.tv_usec > 350000))) {
		client->elapsed = 0xffff;
	} else {
		client->elapsed = elapsed.tv_sec * 100;
		client->elapsed += elapsed.tv_usec / 10000;
	}

	if (client->elapsed == 0)
		log_debug("XMT: Forming %s, 0 ms elapsed.", msg_str);
	else
		log_debug("XMT: Forming %s, %u0 ms elapsed.", msg_str,
			  (unsigned)client->elapsed);

	client->elapsed = htons(client->elapsed);

	make_client6_options(client, &client->sent_options, lease, msg_type);

	return(CHK_TIM_SUCCESS);
}

/*
 * do_init6() marshals and transmits a solicit.
 */
void
do_init6(void *input)
{
	struct client_state *client;
	struct dhc6_ia *old_ia;
	struct dhc6_addr *old_addr;
	struct data_string ds;
	struct data_string ia;
	struct data_string addr;
	struct timeval tv;
	u_int32_t t1, t2;
	int i, idx, len, send_ret;

	client = input;

	/*
	 * In RFC3315 section 17.1.2, the retransmission timer is
	 * used as the selecting timer.
	 */
	if (client->advertised_leases != NULL) {
		start_selecting6(client);
		return;
	}

	switch(check_timing6(client, DHCPV6_SOLICIT, "Solicit", NULL, &ds)) {
	      case CHK_TIM_MRC_EXCEEDED:
	      case CHK_TIM_ALLOC_FAILURE:
		return;
	      case CHK_TIM_MRD_EXCEEDED:
		client->state = S_STOPPED;
		if (client->active_lease != NULL) {
			dhc6_lease_destroy(&client->active_lease, MDL);
			client->active_lease = NULL;
		}
		/* Stop if and only if this is the last client. */
		if (stopping_finished())
			exit(2);
		return;
	}

	/*
	 * Fetch any configured 'sent' options (includes DUID) in wire format.
	 */
	dhcpv6_universe.encapsulate(&ds, NULL, NULL, client,
				    NULL, client->sent_options, &global_scope,
				    &dhcpv6_universe);

	/* Use a specific handler with rapid-commit. */
	if (lookup_option(&dhcpv6_universe, client->sent_options,
			  D6O_RAPID_COMMIT) != NULL) {
		client->v6_handler = rapid_commit_handler;
	}

	/* Append IA_NA. */
	for (i = 0; i < wanted_ia_na; i++) {
		/*
		 * XXX: maybe the IA_NA('s) should be put into the sent_options
		 * cache.  They'd have to be pulled down as they also contain
		 * different option caches in the same universe...
		 */
		memset(&ia, 0, sizeof(ia));
		if (!buffer_allocate(&ia.buffer, 12, MDL)) {
			log_error("Unable to allocate memory for IA_NA.");
			data_string_forget(&ds, MDL);
			return;
		}
		ia.data = ia.buffer->data;
		ia.len = 12;

		/*
		 * A simple IAID is the last 4 bytes
		 * of the hardware address.
		 */
		if (client->interface->hw_address.hlen > 4) {
			idx = client->interface->hw_address.hlen - 4;
			len = 4;
		} else {
			idx = 0;
			len = client->interface->hw_address.hlen;
		}
		memcpy(ia.buffer->data,
		       client->interface->hw_address.hbuf + idx,
		       len);
		if (i)
			ia.buffer->data[3] += i;

		t1 = client->config->requested_lease / 2;
		t2 = t1 + (t1 / 2);
		putULong(ia.buffer->data + 4, t1);
		putULong(ia.buffer->data + 8, t2);

		log_debug("XMT:  X-- IA_NA %s",
			  print_hex_1(4, ia.buffer->data, 55));
		log_debug("XMT:  | X-- Request renew in  +%u", (unsigned)t1);
		log_debug("XMT:  | X-- Request rebind in +%u", (unsigned)t2);

		if ((client->active_lease != NULL) &&
		    ((old_ia = find_ia(client->active_lease->bindings,
				       D6O_IA_NA,
				       (char *)ia.buffer->data)) != NULL)) {
			/*
			 * For each address in the old IA_NA,
			 * request a binding.
			 */
			memset(&addr, 0, sizeof(addr));
			for (old_addr = old_ia->addrs ; old_addr != NULL ;
			     old_addr = old_addr->next) {
				if (old_addr->address.len != 16) {
					log_error("Invalid IPv6 address "
						  "length %d.  "
						  "Ignoring.  (%s:%d)",
						  old_addr->address.len,
						  MDL);
					continue;
				}

				if (!buffer_allocate(&addr.buffer, 24, MDL)) {
					log_error("Unable to allocate memory "
						  "for IAADDR.");
					data_string_forget(&ia, MDL);
					data_string_forget(&ds, MDL);
					return;
				}
				addr.data = addr.buffer->data;
				addr.len = 24;

				memcpy(addr.buffer->data,
				       old_addr->address.iabuf,
				       16);

				t1 = client->config->requested_lease;
				t2 = t1 + (t1 / 2);
				putULong(addr.buffer->data + 16, t1);
				putULong(addr.buffer->data + 20, t2);

				log_debug("XMT:  | X-- Request address %s.",
					  piaddr(old_addr->address));
				log_debug("XMT:  | | X-- Request "
					  "preferred in +%u",
					  (unsigned)t1);
				log_debug("XMT:  | | X-- Request valid "
					  "in     +%u",
					  (unsigned)t2);

				append_option(&ia, &dhcpv6_universe,
					      iaaddr_option,
					      &addr);

				data_string_forget(&addr, MDL);
			}
		}

		append_option(&ds, &dhcpv6_universe, ia_na_option, &ia);
		data_string_forget(&ia, MDL);
	}

	/* Append IA_TA. */
	for (i = 0; i < wanted_ia_ta; i++) {
		/*
		 * XXX: maybe the IA_TA('s) should be put into the sent_options
		 * cache.  They'd have to be pulled down as they also contain
		 * different option caches in the same universe...
		 */
		memset(&ia, 0, sizeof(ia));
		if (!buffer_allocate(&ia.buffer, 4, MDL)) {
			log_error("Unable to allocate memory for IA_TA.");
			data_string_forget(&ds, MDL);
			return;
		}
		ia.data = ia.buffer->data;
		ia.len = 4;

		/*
		 * A simple IAID is the last 4 bytes
		 * of the hardware address.
		 */
		if (client->interface->hw_address.hlen > 4) {
			idx = client->interface->hw_address.hlen - 4;
			len = 4;
		} else {
			idx = 0;
			len = client->interface->hw_address.hlen;
		}
		memcpy(ia.buffer->data,
		       client->interface->hw_address.hbuf + idx,
		       len);
		if (i)
			ia.buffer->data[3] += i;

		log_debug("XMT:  X-- IA_TA %s",
			  print_hex_1(4, ia.buffer->data, 55));

		if ((client->active_lease != NULL) &&
		    ((old_ia = find_ia(client->active_lease->bindings,
				       D6O_IA_TA,
				       (char *)ia.buffer->data)) != NULL)) {
			/*
			 * For each address in the old IA_TA,
			 * request a binding.
			 */
			memset(&addr, 0, sizeof(addr));
			for (old_addr = old_ia->addrs ; old_addr != NULL ;
			     old_addr = old_addr->next) {
				if (old_addr->address.len != 16) {
					log_error("Invalid IPv6 address "
						  "length %d.  "
						  "Ignoring.  (%s:%d)",
						  old_addr->address.len,
						  MDL);
					continue;
				}

				if (!buffer_allocate(&addr.buffer, 24, MDL)) {
					log_error("Unable to allocate memory "
						  "for IAADDR.");
					data_string_forget(&ia, MDL);
					data_string_forget(&ds, MDL);
					return;
				}
				addr.data = addr.buffer->data;
				addr.len = 24;

				memcpy(addr.buffer->data,
				       old_addr->address.iabuf,
				       16);

				t1 = client->config->requested_lease;
				t2 = t1 + (t1 / 2);
				putULong(addr.buffer->data + 16, t1);
				putULong(addr.buffer->data + 20, t2);

				log_debug("XMT:  | X-- Request address %s.",
					  piaddr(old_addr->address));
				log_debug("XMT:  | | X-- Request "
					  "preferred in +%u",
					  (unsigned)t1);
				log_debug("XMT:  | | X-- Request valid "
					  "in     +%u",
					  (unsigned)t2);

				append_option(&ia, &dhcpv6_universe,
					      iaaddr_option,
					      &addr);

				data_string_forget(&addr, MDL);
			}
		}

		append_option(&ds, &dhcpv6_universe, ia_ta_option, &ia);
		data_string_forget(&ia, MDL);
	}

	/* Append IA_PD. */
	for (i = 0; i < wanted_ia_pd; i++) {
		/*
		 * XXX: maybe the IA_PD('s) should be put into the sent_options
		 * cache.  They'd have to be pulled down as they also contain
		 * different option caches in the same universe...
		 */
		memset(&ia, 0, sizeof(ia));
		if (!buffer_allocate(&ia.buffer, 12, MDL)) {
			log_error("Unable to allocate memory for IA_PD.");
			data_string_forget(&ds, MDL);
			return;
		}
		ia.data = ia.buffer->data;
		ia.len = 12;

		/*
		 * A simple IAID is the last 4 bytes
		 * of the hardware address.
		 */
		if (client->interface->hw_address.hlen > 4) {
			idx = client->interface->hw_address.hlen - 4;
			len = 4;
		} else {
			idx = 0;
			len = client->interface->hw_address.hlen;
		}
		memcpy(ia.buffer->data,
		       client->interface->hw_address.hbuf + idx,
		       len);
		if (i)
			ia.buffer->data[3] += i;

		t1 = client->config->requested_lease / 2;
		t2 = t1 + (t1 / 2);
		putULong(ia.buffer->data + 4, t1);
		putULong(ia.buffer->data + 8, t2);

		log_debug("XMT:  X-- IA_PD %s",
			  print_hex_1(4, ia.buffer->data, 55));
		log_debug("XMT:  | X-- Request renew in  +%u", (unsigned)t1);
		log_debug("XMT:  | X-- Request rebind in +%u", (unsigned)t2);

		if ((client->active_lease != NULL) &&
		    ((old_ia = find_ia(client->active_lease->bindings,
				       D6O_IA_PD,
				       (char *)ia.buffer->data)) != NULL)) {
			/*
			 * For each prefix in the old IA_PD,
			 * request a binding.
			 */
			memset(&addr, 0, sizeof(addr));
			for (old_addr = old_ia->addrs ; old_addr != NULL ;
			     old_addr = old_addr->next) {
				if (old_addr->address.len != 16) {
					log_error("Invalid IPv6 prefix, "
						  "Ignoring.  (%s:%d)",
						  MDL);
					continue;
				}

				if (!buffer_allocate(&addr.buffer, 25, MDL)) {
					log_error("Unable to allocate memory "
						  "for IAPREFIX.");
					data_string_forget(&ia, MDL);
					data_string_forget(&ds, MDL);
					return;
				}
				addr.data = addr.buffer->data;
				addr.len = 25;

				t1 = client->config->requested_lease;
				t2 = t1 + (t1 / 2);
				putULong(addr.buffer->data, t1);
				putULong(addr.buffer->data + 4, t2);

				putUChar(addr.buffer->data + 8,
					 old_addr->plen);
				memcpy(addr.buffer->data + 9,
				       old_addr->address.iabuf,
				       16);

				log_debug("XMT:  | X-- Request prefix %s/%u.",
					  piaddr(old_addr->address),
					  (unsigned) old_addr->plen);
				log_debug("XMT:  | | X-- Request "
					  "preferred in +%u",
					  (unsigned)t1);
				log_debug("XMT:  | | X-- Request valid "
					  "in     +%u",
					  (unsigned)t2);

				append_option(&ia, &dhcpv6_universe,
					      iaprefix_option,
					      &addr);

				data_string_forget(&addr, MDL);
			}
		}

		append_option(&ds, &dhcpv6_universe, ia_pd_option, &ia);
		data_string_forget(&ia, MDL);
	}

	/* Transmit and wait. */

	log_info("XMT: Solicit on %s, interval %ld0ms.",
		 client->name ? client->name : client->interface->name,
		 (long int)client->RT);

	send_ret = send_packet6(client->interface,
				ds.data, ds.len, &DHCPv6DestAddr);
	if (send_ret != ds.len) {
		log_error("dhc6: send_packet6() sent %d of %d bytes",
			  send_ret, ds.len);
	}

	data_string_forget(&ds, MDL);

	/* Wait RT */
	tv.tv_sec = cur_tv.tv_sec + client->RT / 100;
	tv.tv_usec = cur_tv.tv_usec + (client->RT % 100) * 10000;
	if (tv.tv_usec >= 1000000) {
		tv.tv_sec += 1;
		tv.tv_usec -= 1000000;
	}
	add_timeout(&tv, do_init6, client, NULL, NULL);

	dhc6_retrans_advance(client);
}

/* do_info_request6() marshals and transmits an information-request. */
void
do_info_request6(void *input)
{
	struct client_state *client;
	struct data_string ds;
	struct timeval tv;
	int send_ret;

	client = input;

	switch(check_timing6(client, DHCPV6_INFORMATION_REQUEST,
			     "Info-Request", NULL, &ds)) {
	      case CHK_TIM_MRC_EXCEEDED:
	      case CHK_TIM_ALLOC_FAILURE:
		return;
	      case CHK_TIM_MRD_EXCEEDED:
		exit(2);
	      case CHK_TIM_SUCCESS:
		break;
	}

	/* Fetch any configured 'sent' options (includes DUID) in wire format.
	 */
	dhcpv6_universe.encapsulate(&ds, NULL, NULL, client,
				    NULL, client->sent_options, &global_scope,
				    &dhcpv6_universe);

	/* Transmit and wait. */

	log_info("XMT: Info-Request on %s, interval %ld0ms.",
		 client->name ? client->name : client->interface->name,
		 (long int)client->RT);

	send_ret = send_packet6(client->interface,
				ds.data, ds.len, &DHCPv6DestAddr);
	if (send_ret != ds.len) {
		log_error("dhc6: send_packet6() sent %d of %d bytes",
			  send_ret, ds.len);
	}

	data_string_forget(&ds, MDL);

	/* Wait RT */
	tv.tv_sec = cur_tv.tv_sec + client->RT / 100;
	tv.tv_usec = cur_tv.tv_usec + (client->RT % 100) * 10000;
	if (tv.tv_usec >= 1000000) {
		tv.tv_sec += 1;
		tv.tv_usec -= 1000000;
	}
	add_timeout(&tv, do_info_request6, client, NULL, NULL);

	dhc6_retrans_advance(client);
}

/* do_confirm6() creates a Confirm packet and transmits it.  This function
 * is called on every timeout to (re)transmit.
 */
void
do_confirm6(void *input)
{
	struct client_state *client;
	struct data_string ds;
	int send_ret;
	struct timeval tv;

	client = input;

	if (client->active_lease == NULL)
		log_fatal("Impossible condition at %s:%d.", MDL);

	/* In section 17.1.3, it is said:
	 *
	 *   If the client receives no responses before the message
	 *   transmission process terminates, as described in section 14,
	 *   the client SHOULD continue to use any IP addresses, using the
	 *   last known lifetimes for those addresses, and SHOULD continue
	 *   to use any other previously obtained configuration parameters.
	 *
	 * So if confirm times out, we go active.
	 *
	 * XXX: Should we reduce all IA's t1 to 0, so that we renew and
	 * stick there until we get a reply?
	 */

	switch(check_timing6(client, DHCPV6_CONFIRM, "Confirm",
			     client->active_lease, &ds)) {
	      case CHK_TIM_MRC_EXCEEDED:
	      case CHK_TIM_MRD_EXCEEDED:
		start_bound(client);
		return;
	      case CHK_TIM_ALLOC_FAILURE:
		return;
	      case CHK_TIM_SUCCESS:
		break;
	}

	/* Fetch any configured 'sent' options (includes DUID') in wire format.
	 */
	dhcpv6_universe.encapsulate(&ds, NULL, NULL, client, NULL,
				    client->sent_options, &global_scope,
				    &dhcpv6_universe);

	/* Append IA's. */
	if (wanted_ia_na &&
	    dhc6_add_ia_na(client, &ds, client->active_lease,
			   DHCPV6_CONFIRM) != ISC_R_SUCCESS) {
		data_string_forget(&ds, MDL);
		return;
	}
	if (wanted_ia_ta &&
	    dhc6_add_ia_ta(client, &ds, client->active_lease,
			   DHCPV6_CONFIRM) != ISC_R_SUCCESS) {
		data_string_forget(&ds, MDL);
		return;
	}

	/* Transmit and wait. */

	log_info("XMT: Confirm on %s, interval %ld0ms.",
		 client->name ? client->name : client->interface->name,
		 (long int)client->RT);

	send_ret = send_packet6(client->interface, ds.data, ds.len,
				&DHCPv6DestAddr);
	if (send_ret != ds.len) {
		log_error("dhc6: sendpacket6() sent %d of %d bytes",
			  send_ret, ds.len);
	}

	data_string_forget(&ds, MDL);

	/* Wait RT */
	tv.tv_sec = cur_tv.tv_sec + client->RT / 100;
	tv.tv_usec = cur_tv.tv_usec + (client->RT % 100) * 10000;
	if (tv.tv_usec >= 1000000) {
		tv.tv_sec += 1;
		tv.tv_usec -= 1000000;
	}
	add_timeout(&tv, do_confirm6, client, NULL, NULL);

	dhc6_retrans_advance(client);
}

/*
 * Release addresses.
 */
void
start_release6(struct client_state *client)
{
	/* Cancel any pending transmissions */
	cancel_timeout(do_confirm6, client);
	cancel_timeout(do_select6, client);
	cancel_timeout(do_refresh6, client);
	cancel_timeout(do_release6, client);
	client->state = S_STOPPED;

	/*
	 * It is written:  "The client MUST NOT use any of the addresses it
	 * is releasing as the source address in the Release message or in
	 * any subsequently transmitted message."  So unconfigure now.
	 */
	unconfigure6(client, "RELEASE6");

	/* Note this in the lease file. */
	if (client->active_lease == NULL)
		return;
	client->active_lease->released = ISC_TRUE;
	write_client6_lease(client, client->active_lease, 0, 1);

	/* Set timers per RFC3315 section 18.1.6. */
	client->IRT = REL_TIMEOUT * 100;
	client->MRT = 0;
	client->MRC = REL_MAX_RC;
	client->MRD = 0;

	dhc6_retrans_init(client);
	client->v6_handler = reply_handler;

	do_release6(client);
}
/*
 * do_release6() creates a Release packet and transmits it.
 */
static void
do_release6(void *input)
{
	struct client_state *client;
	struct data_string ds;
	int send_ret;
	struct timeval tv;

	client = input;

	if ((client->active_lease == NULL) || !active_prefix(client))
		return;

	switch(check_timing6(client, DHCPV6_RELEASE, "Release", 
			     client->active_lease, &ds)) {
	      case CHK_TIM_MRC_EXCEEDED:
	      case CHK_TIM_ALLOC_FAILURE:
	      case CHK_TIM_MRD_EXCEEDED:
		goto release_done;
	      case CHK_TIM_SUCCESS:
		break;
	}

	/*
	 * Don't use unicast as we don't know if we still have an
	 * available address with enough scope.
	 */

	dhcpv6_universe.encapsulate(&ds, NULL, NULL, client, NULL,
				    client->sent_options, &global_scope,
				    &dhcpv6_universe);

	/* Append IA's (but don't release temporary addresses). */
	if (wanted_ia_na &&
	    dhc6_add_ia_na(client, &ds, client->active_lease,
			   DHCPV6_RELEASE) != ISC_R_SUCCESS) {
		data_string_forget(&ds, MDL);
		goto release_done;
	}
	if (wanted_ia_pd &&
	    dhc6_add_ia_pd(client, &ds, client->active_lease,
			   DHCPV6_RELEASE) != ISC_R_SUCCESS) {
		data_string_forget(&ds, MDL);
		goto release_done;
	}

	/* Transmit and wait. */
	log_info("XMT: Release on %s, interval %ld0ms.",
		 client->name ? client->name : client->interface->name,
		 (long int)client->RT);

	send_ret = send_packet6(client->interface, ds.data, ds.len,
				&DHCPv6DestAddr);
	if (send_ret != ds.len) {
		log_error("dhc6: sendpacket6() sent %d of %d bytes",
			  send_ret, ds.len);
	}

	data_string_forget(&ds, MDL);

	/* Wait RT */
	tv.tv_sec = cur_tv.tv_sec + client->RT / 100;
	tv.tv_usec = cur_tv.tv_usec + (client->RT % 100) * 10000;
	if (tv.tv_usec >= 1000000) {
		tv.tv_sec += 1;
		tv.tv_usec -= 1000000;
	}
	add_timeout(&tv, do_release6, client, NULL, NULL);
	dhc6_retrans_advance(client);
	return;

      release_done:
	dhc6_lease_destroy(&client->active_lease, MDL);
	client->active_lease = NULL;
	if (stopping_finished())
		exit(0);
}

/* status_log() just puts a status code into displayable form and logs it
 * to info level.
 */
static void
status_log(int code, const char *scope, const char *additional, int len)
{
	const char *msg = NULL;

	switch(code) {
	      case STATUS_Success:
		msg = "Success";
		break;

	      case STATUS_UnspecFail:
		msg = "UnspecFail";
		break;

	      case STATUS_NoAddrsAvail:
		msg = "NoAddrsAvail";
		break;

	      case STATUS_NoBinding:
		msg = "NoBinding";
		break;

	      case STATUS_NotOnLink:
		msg = "NotOnLink";
		break;

	      case STATUS_UseMulticast:
		msg = "UseMulticast";
		break;

	      case STATUS_NoPrefixAvail:
		msg = "NoPrefixAvail";
		break;

	      default:
		msg = "UNKNOWN";
		break;
	}

	if (len > 0)
		log_info("%s status code %s: %s", scope, msg,
			 print_hex_1(len,
				     (const unsigned char *)additional, 50));
	else
		log_info("%s status code %s.", scope, msg);
}

/* Acquire a status code.
 */
static isc_result_t
dhc6_get_status_code(struct option_state *options, unsigned *code,
		     struct data_string *msg)
{
	struct option_cache *oc;
	struct data_string ds;
	isc_result_t rval = ISC_R_SUCCESS;

	if ((options == NULL) || (code == NULL))
		return DHCP_R_INVALIDARG;

	if ((msg != NULL) && (msg->len != 0))
		return DHCP_R_INVALIDARG;

	memset(&ds, 0, sizeof(ds));

	/* Assume success if there is no option. */
	*code = STATUS_Success;

	oc = lookup_option(&dhcpv6_universe, options, D6O_STATUS_CODE);
	if ((oc != NULL) &&
	    evaluate_option_cache(&ds, NULL, NULL, NULL, options,
				  NULL, &global_scope, oc, MDL)) {
		if (ds.len < 2) {
			log_error("Invalid status code length %d.", ds.len);
			rval = DHCP_R_FORMERR;
		} else
			*code = getUShort(ds.data);

		if ((msg != NULL) && (ds.len > 2)) {
			data_string_copy(msg, &ds, MDL);
			msg->data += 2;
			msg->len -= 2;
		}

		data_string_forget(&ds, MDL);
		return rval;
	}

	return ISC_R_NOTFOUND;
}

/* Look at status codes in an advertise, and reform the return value.
 */
static isc_result_t
dhc6_check_status(isc_result_t rval, struct option_state *options,
		  const char *scope, unsigned *code)
{
	struct data_string msg;
	isc_result_t status;

	if ((scope == NULL) || (code == NULL))
		return DHCP_R_INVALIDARG;

	/* If we don't find a code, we assume success. */
	*code = STATUS_Success;

	/* If there is no options cache, then there is no code. */
	if (options != NULL) {
		memset(&msg, 0, sizeof(msg));
		status = dhc6_get_status_code(options, code, &msg);

		if (status == ISC_R_SUCCESS) {
			status_log(*code, scope, (char *)msg.data, msg.len);
			data_string_forget(&msg, MDL);

			if (*code != STATUS_Success)
				rval = ISC_R_FAILURE;

		} else if (status != ISC_R_NOTFOUND)
			rval = status;
	}

	return rval;
}

/* Look in the packet, any IA's, and any IAADDR's within those IA's to find
 * status code options that are not SUCCESS.
 */
static isc_result_t
dhc6_check_advertise(struct dhc6_lease *lease)
{
	struct dhc6_ia *ia;
	struct dhc6_addr *addr;
	isc_result_t rval = ISC_R_SUCCESS;
	int have_addrs = ISC_FALSE;
	unsigned code;
	const char *scope;

	rval = dhc6_check_status(rval, lease->options, "message", &code);

	for (ia = lease->bindings ; ia != NULL ; ia = ia->next) {
		switch (ia->ia_type) {
			case D6O_IA_NA:
				scope = "IA_NA";
				break;
			case D6O_IA_TA:
				scope = "IA_TA";
				break;
			case D6O_IA_PD:
				scope = "IA_PD";
				break;
			default:
				log_error("dhc6_check_advertise: no type.");
				return ISC_R_FAILURE;
		}
		rval = dhc6_check_status(rval, ia->options, scope, &code);

		for (addr = ia->addrs ; addr != NULL ; addr = addr->next) {
			if (ia->ia_type != D6O_IA_PD)
				scope = "IAADDR";
			else
				scope = "IAPREFIX";
			rval = dhc6_check_status(rval, addr->options,
						 scope, &code);
			have_addrs = ISC_TRUE;
		}
	}

	if (have_addrs != ISC_TRUE)
		rval = ISC_R_ADDRNOTAVAIL;

	return rval;
}

/* status code <-> action matrix for the client in INIT state
 * (rapid/commit).  Returns always false as no action is defined.
 */
static isc_boolean_t
dhc6_init_action(struct client_state *client, isc_result_t *rvalp,
		 unsigned code)
{
	if (rvalp == NULL)
		log_fatal("Impossible condition at %s:%d.", MDL);

	if (client == NULL) {
		*rvalp = DHCP_R_INVALIDARG;
		return ISC_FALSE;
	}

	if (*rvalp == ISC_R_SUCCESS)
		return ISC_FALSE;

	/* No possible action in any case... */
	return ISC_FALSE;
}

/* status code <-> action matrix for the client in SELECT state
 * (request/reply).  Returns true if action was taken (and the
 * packet should be ignored), or false if no action was taken.
 */
static isc_boolean_t
dhc6_select_action(struct client_state *client, isc_result_t *rvalp,
		   unsigned code)
{
	struct dhc6_lease *lease;
	isc_result_t rval;

	if (rvalp == NULL)
		log_fatal("Impossible condition at %s:%d.", MDL);

	if (client == NULL) {
		*rvalp = DHCP_R_INVALIDARG;
		return ISC_FALSE;
	}
	rval = *rvalp;

	if (rval == ISC_R_SUCCESS)
		return ISC_FALSE;

	switch (code) {
		/* We may have an earlier failure status code (so no
		 * success rval), and a success code now.  This
		 * doesn't upgrade the rval to success, but it does
		 * mean we take no action here.
		 */
	      case STATUS_Success:
		/* Gimpy server, or possibly an attacker. */
	      case STATUS_NoBinding:
	      case STATUS_UseMulticast:
		/* Take no action. */
		return ISC_FALSE;

		/* If the server can't deal with us, either try the
		 * next advertised server, or continue retrying if there
		 * weren't any.
		 */
	      default:
	      case STATUS_UnspecFail:
		if (client->advertised_leases != NULL) {
			dhc6_lease_destroy(&client->selected_lease, MDL);
			client->selected_lease = NULL;

			start_selecting6(client);

			break;
		} else /* Take no action - continue to retry. */
			return ISC_FALSE;

		/* If the server has no addresses, try other servers if
		 * we got some, otherwise go to INIT to hope for more
		 * servers.
		 */
	      case STATUS_NoAddrsAvail:
	      case STATUS_NoPrefixAvail:
		if (client->state == S_REBOOTING)
			return ISC_FALSE;

		if (client->selected_lease == NULL)
			log_fatal("Impossible case at %s:%d.", MDL);

		dhc6_lease_destroy(&client->selected_lease, MDL);
		client->selected_lease = NULL;

		if (client->advertised_leases != NULL)
			start_selecting6(client);
		else
			start_init6(client);

		break;

		/* If we got a NotOnLink from a Confirm, then we're not
		 * on link.  Kill the old-active binding and start over.
		 *
		 * If we got a NotOnLink from our Request, something weird
		 * happened.  Start over from scratch anyway.
		 */
	      case STATUS_NotOnLink:
		if (client->state == S_REBOOTING) {
			if (client->active_lease == NULL)
				log_fatal("Impossible case at %s:%d.", MDL);

			dhc6_lease_destroy(&client->active_lease, MDL);
		} else {
			if (client->selected_lease == NULL)
				log_fatal("Impossible case at %s:%d.", MDL);

			dhc6_lease_destroy(&client->selected_lease, MDL);
			client->selected_lease = NULL;

			while (client->advertised_leases != NULL) {
				lease = client->advertised_leases;
				client->advertised_leases = lease->next;

				dhc6_lease_destroy(&lease, MDL);
			}
		}

		start_init6(client);
		break;
	}

	return ISC_TRUE;
}

static void
dhc6_withdraw_lease(struct client_state *client)
{
	struct dhc6_ia *ia;
	struct dhc6_addr *addr;

	if ((client == NULL) || (client->active_lease == NULL))
		return;

	for (ia = client->active_lease->bindings ; ia != NULL ;
	     ia = ia->next) {
		for (addr = ia->addrs ; addr != NULL ; addr = addr->next) {
			addr->max_life = addr->preferred_life = 0;
		}
	}

	/* Perform expiry. */
	do_expire(client);
}

/* status code <-> action matrix for the client in BOUND state
 * (request/reply).  Returns true if action was taken (and the
 * packet should be ignored), or false if no action was taken.
 */
static isc_boolean_t
dhc6_reply_action(struct client_state *client, isc_result_t *rvalp,
		  unsigned code)
{
	isc_result_t rval;

	if (rvalp == NULL)
		log_fatal("Impossible condition at %s:%d.", MDL);

	if (client == NULL) {
		*rvalp = DHCP_R_INVALIDARG;
		return ISC_FALSE;
	}
	rval = *rvalp;

	if (rval == ISC_R_SUCCESS)
		return ISC_FALSE;

	switch (code) {
		/* It's possible an earlier status code set rval to a failure
		 * code, and we've encountered a later success.
		 */
	      case STATUS_Success:
		/* In "refreshes" (where we get replies), we probably
		 * still have a valid lease.  So "take no action" and
		 * the upper levels will keep retrying until the lease
		 * expires (or we rebind).
		 */
	      case STATUS_UnspecFail:
		/* For unknown codes...it's a soft (retryable) error. */
	      default:
		return ISC_FALSE;

		/* The server is telling us to use a multicast address, so
		 * we have to delete the unicast option from the active
		 * lease, then allow retransmission to occur normally.
		 * (XXX: It might be preferable in this case to retransmit
		 * sooner than the current interval, but for now we don't.)
		 */
	      case STATUS_UseMulticast:
		if (client->active_lease != NULL)
			delete_option(&dhcp_universe,
				      client->active_lease->options,
				      D6O_UNICAST);
		return ISC_FALSE;

		/* "When the client receives a NotOnLink status from the
		 *  server in response to a Request, the client can either
		 *  re-issue the Request without specifying any addresses
		 *  or restart the DHCP server discovery process."
		 *
		 * This is strange.  If competing server evaluation is
		 * useful (and therefore in the protocol), then why would
		 * a client's first reaction be to request from the same
		 * server on a different link?  Surely you'd want to
		 * re-evaluate your server selection.
		 *
		 * Well, I guess that's the answer.
		 */
	      case STATUS_NotOnLink:
		/* In this case, we need to rescind all current active
		 * bindings (just 'expire' them all normally, if early).
		 * They're no use to us on the wrong link.  Then head back
		 * to init, redo server selection and get new addresses.
		 */
		dhc6_withdraw_lease(client);
		break;

		/* "If the status code is NoAddrsAvail, the client has
		 *  received no usable addresses in the IA and may choose
		 *  to try obtaining addresses for the IA from another
		 *  server."
		 */
	      case STATUS_NoAddrsAvail:
	      case STATUS_NoPrefixAvail:
		/* Head back to init, keeping any active bindings (!). */
		start_init6(client);
		break;

		/* -  sends a Request message if the IA contained a Status
		 *    Code option with the NoBinding status (and does not
		 *    send any additional Renew/Rebind messages)
		 */
	      case STATUS_NoBinding:
		if (client->advertised_leases != NULL)
			log_fatal("Impossible condition at %s:%d.", MDL);

		client->advertised_leases =
				dhc6_dup_lease(client->active_lease, MDL);
		start_selecting6(client);
		break;
	}

	return ISC_TRUE;
}

/* status code <-> action matrix for the client in STOPPED state
 * (release/decline).  Returns true if action was taken (and the
 * packet should be ignored), or false if no action was taken.
 * NoBinding is translated into Success.
 */
static isc_boolean_t
dhc6_stop_action(struct client_state *client, isc_result_t *rvalp,
		  unsigned code)
{
	isc_result_t rval;

	if (rvalp == NULL)
		log_fatal("Impossible condition at %s:%d.", MDL);

	if (client == NULL) {
		*rvalp = DHCP_R_INVALIDARG;
		return ISC_FALSE;
	}
	rval = *rvalp;

	if (rval == ISC_R_SUCCESS)
		return ISC_FALSE;

	switch (code) {
		/* It's possible an earlier status code set rval to a failure
		 * code, and we've encountered a later success.
		 */
	      case STATUS_Success:
		/* For unknown codes...it's a soft (retryable) error. */
	      case STATUS_UnspecFail:
	      default:
		return ISC_FALSE;

		/* NoBinding is not an error */
	      case STATUS_NoBinding:
		if (rval == ISC_R_FAILURE)
			*rvalp = ISC_R_SUCCESS;
		return ISC_FALSE;

		/* Should not happen */
	      case STATUS_NoAddrsAvail:
	      case STATUS_NoPrefixAvail:
		break;

		/* Give up on it */
	      case STATUS_NotOnLink:
		break;

		/* The server is telling us to use a multicast address, so
		 * we have to delete the unicast option from the active
		 * lease, then allow retransmission to occur normally.
		 * (XXX: It might be preferable in this case to retransmit
		 * sooner than the current interval, but for now we don't.)
		 */
	      case STATUS_UseMulticast:
		if (client->active_lease != NULL)
			delete_option(&dhcp_universe,
				      client->active_lease->options,
				      D6O_UNICAST);
		return ISC_FALSE;
	}

	return ISC_TRUE;
}

/* Look at a new and old lease, and make sure the new information is not
 * losing us any state.
 */
static isc_result_t
dhc6_check_reply(struct client_state *client, struct dhc6_lease *new)
{
	isc_boolean_t (*action)(struct client_state *,
				isc_result_t *, unsigned);
	struct dhc6_ia *ia;
	struct dhc6_addr *addr;
	isc_result_t rval = ISC_R_SUCCESS;
	unsigned code;
	const char *scope;
	int nscore, sscore;

	if ((client == NULL) || (new == NULL))
		return DHCP_R_INVALIDARG;

	switch (client->state) {
	      case S_INIT:
		action = dhc6_init_action;
		break;

	      case S_SELECTING:
	      case S_REBOOTING:
		action = dhc6_select_action;
		break;

	      case S_RENEWING:
	      case S_REBINDING:
		action = dhc6_reply_action;
		break;

	      case S_STOPPED:
		action = dhc6_stop_action;
		break;

	      default:
		log_fatal("Impossible condition at %s:%d.", MDL);
		return ISC_R_CANCELED;
	}

	/* If there is a code to extract, and if there is some
	 * action to take based on that code, then take the action
	 * and do not continue.
	 */
	rval = dhc6_check_status(rval, new->options, "message", &code);
	if (action(client, &rval, code))
		return ISC_R_CANCELED;

	for (ia = new->bindings ; ia != NULL ; ia = ia->next) {
		switch (ia->ia_type) {
			case D6O_IA_NA:
				scope = "IA_NA";
				break;
			case D6O_IA_TA:
				scope = "IA_TA";
				break;
			case D6O_IA_PD:
				scope = "IA_PD";
				break;
			default:
				log_error("dhc6_check_reply: no type.");
				return DHCP_R_INVALIDARG;
		}
		rval = dhc6_check_status(rval, ia->options,
					 scope, &code);
		if (action(client, &rval, code))
			return ISC_R_CANCELED;

		for (addr = ia->addrs ; addr != NULL ;
		     addr = addr->next) {
			if (ia->ia_type != D6O_IA_PD)
				scope = "IAADDR";
			else
				scope = "IAPREFIX";
			rval = dhc6_check_status(rval, addr->options,
						 scope, &code);
			if (action(client, &rval, code))
				return ISC_R_CANCELED;
		}
	}

	/* A Confirm->Reply is unsuitable for comparison to the old lease. */
	if (client->state == S_REBOOTING)
		return rval;

	/* No old lease in rapid-commit. */
	if (client->state == S_INIT)
		return rval;

	switch (client->state) {
	      case S_SELECTING:
		/* Compare the new lease with the selected lease to make
		 * sure there is no risky business.
		 */
		nscore = dhc6_score_lease(client, new);
		sscore = dhc6_score_lease(client, client->selected_lease);
		if ((client->advertised_leases != NULL) &&
		    (nscore < (sscore / 2))) {
			/* XXX: An attacker might reply this way to make
			 * XXX: sure we latch onto their configuration.
			 * XXX: We might want to ignore the packet and
			 * XXX: schedule re-selection at the next timeout?
			 */
			log_error("PRC: BAIT AND SWITCH detected.  Score of "
				  "supplied lease (%d) is substantially "
				  "smaller than the advertised score (%d).  "
				  "Trying other servers.",
				  nscore, sscore);

			dhc6_lease_destroy(&client->selected_lease, MDL);
			client->selected_lease = NULL;

			start_selecting6(client);

			return ISC_R_CANCELED;
		}
		break;

	      case S_RENEWING:
	      case S_REBINDING:
		/* This leaves one RFC3315 status check unimplemented:
		 *
		 * -  sends a Renew/Rebind if the IA is not in the Reply
		 *    message
		 *
		 * We rely on the scheduling system to note that the IA has
		 * not left Renewal/Rebinding/whatever since it still carries
		 * old times from the last successful binding.  So this is
		 * implemented actually, just not explicitly.
		 */
		break;

	      case S_STOPPED:
		/* Nothing critical to do at this stage. */
		break;

	      default:
		log_fatal("REALLY impossible condition at %s:%d.", MDL);
		return ISC_R_CANCELED;
	}

	return rval;
}

/* While in init state, we only collect advertisements.  If there happens
 * to be an advertisement with a preference option of 255, that's an
 * automatic exit.  Otherwise, we collect advertisements until our timeout
 * expires (client->RT).
 */
void
init_handler(struct packet *packet, struct client_state *client)
{
	struct dhc6_lease *lease;

	/* In INIT state, we send solicits, we only expect to get
	 * advertises (rapid commit has its own handler).
	 */
	if (packet->dhcpv6_msg_type != DHCPV6_ADVERTISE)
		return;

	/* RFC3315 section 15.3 validation (same as 15.10 since we
	 * always include a client id).
	 */
	if (!valid_reply(packet, client)) {
		log_error("Invalid Advertise - rejecting.");
		return;
	}

	lease = dhc6_leaseify(packet);

	if (dhc6_check_advertise(lease) != ISC_R_SUCCESS) {
		log_debug("PRC: Lease failed to satisfy.");
		dhc6_lease_destroy(&lease, MDL);
		return;
	}

	insert_lease(&client->advertised_leases, lease);

	/* According to RFC3315 section 17.1.2, the client MUST wait for
	 * the first RT before selecting a lease.  But on the 400th RT,
	 * we dont' want to wait the full timeout if we finally get an
	 * advertise.  We could probably wait a second, but ohwell,
	 * RFC3315 doesn't say so.
	 *
	 * If the lease is highest possible preference, 255, RFC3315 claims
	 * we should continue immediately even on the first RT.  We probably
	 * should not if the advertise contains less than one IA and address.
	 */
	if ((client->txcount > 1) ||
	    ((lease->pref == 255) &&
	    (dhc6_score_lease(client, lease) > 150))) {
		log_debug("RCV:  Advertisement immediately selected.");
		cancel_timeout(do_init6, client);
		start_selecting6(client);
	} else
		log_debug("RCV:  Advertisement recorded.");
}

/* info_request_handler() accepts a Reply to an Info-request.
 */
void
info_request_handler(struct packet *packet, struct client_state *client)
{
	isc_result_t check_status;
	unsigned code;

	if (packet->dhcpv6_msg_type != DHCPV6_REPLY)
		return;

	/* RFC3315 section 15.10 validation (same as 15.3 since we
	 * always include a client id).
	 */
	if (!valid_reply(packet, client)) {
		log_error("Invalid Reply - rejecting.");
		return;
	}

	check_status = dhc6_check_status(ISC_R_SUCCESS, packet->options,
					 "message", &code);
	if (check_status != ISC_R_SUCCESS) {
		/* If no action was taken, but there is an error, then
		 * we wait for a retransmission.
		 */
		if (check_status != ISC_R_CANCELED)
			return;
	}

	/* We're done retransmitting at this point. */
	cancel_timeout(do_info_request6, client);

	/* Action was taken, so now that we've torn down our scheduled
	 * retransmissions, return.
	 */
	if (check_status == ISC_R_CANCELED)
		return;

	/* Cleanup if a previous attempt to go bound failed. */
	if (client->old_lease != NULL) {
		dhc6_lease_destroy(&client->old_lease, MDL);
		client->old_lease = NULL;
	}

	/* Cache options in the active_lease. */
	if (client->active_lease != NULL)
		client->old_lease = client->active_lease;
	client->active_lease = dmalloc(sizeof(struct dhc6_lease), MDL);
	if (client->active_lease == NULL)
		log_fatal("Out of memory for v6 lease structure.");
	option_state_reference(&client->active_lease->options,
			       packet->options, MDL);

	start_informed(client);
}

/* Specific version of init_handler() for rapid-commit.
 */
void
rapid_commit_handler(struct packet *packet, struct client_state *client)
{
	struct dhc6_lease *lease;
	isc_result_t check_status;

	/* On ADVERTISE just fall back to the init_handler().
	 */
	if (packet->dhcpv6_msg_type == DHCPV6_ADVERTISE) {
		init_handler(packet, client);
		return;
	} else if (packet->dhcpv6_msg_type != DHCPV6_REPLY)
		return;

	/* RFC3315 section 15.10 validation (same as 15.3 since we
	 * always include a client id).
	 */
	if (!valid_reply(packet, client)) {
		log_error("Invalid Reply - rejecting.");
		return;
	}

	/* A rapid-commit option MUST be here. */
	if (lookup_option(&dhcpv6_universe, packet->options,
			  D6O_RAPID_COMMIT) == 0) {
		log_error("Reply without Rapid-Commit - rejecting.");
		return;
	}

	lease = dhc6_leaseify(packet);

	/* This is an out of memory condition...hopefully a temporary
	 * problem.  Returning now makes us try to retransmit later.
	 */
	if (lease == NULL)
		return;

	check_status = dhc6_check_reply(client, lease);
	if (check_status != ISC_R_SUCCESS) {
		dhc6_lease_destroy(&lease, MDL);
		return;
	}

	/* Jump to the selecting state. */
	cancel_timeout(do_init6, client);
	client->state = S_SELECTING;

	/* Merge any bindings in the active lease (if there is one) into
	 * the new active lease.
	 */
	dhc6_merge_lease(client->active_lease, lease);

	/* Cleanup if a previous attempt to go bound failed. */
	if (client->old_lease != NULL) {
		dhc6_lease_destroy(&client->old_lease, MDL);
		client->old_lease = NULL;
	}

	/* Make this lease active and BIND to it. */
	if (client->active_lease != NULL)
		client->old_lease = client->active_lease;
	client->active_lease = lease;

	/* We're done with the ADVERTISEd leases, if any. */
	while(client->advertised_leases != NULL) {
		lease = client->advertised_leases;
		client->advertised_leases = lease->next;

		dhc6_lease_destroy(&lease, MDL);
	}

	start_bound(client);
}

/* Find the 'best' lease in the cache of advertised leases (usually).  From
 * RFC3315 Section 17.1.3:
 *
 *   Upon receipt of one or more valid Advertise messages, the client
 *   selects one or more Advertise messages based upon the following
 *   criteria.
 *
 *   -  Those Advertise messages with the highest server preference value
 *      are preferred over all other Advertise messages.
 *
 *   -  Within a group of Advertise messages with the same server
 *      preference value, a client MAY select those servers whose
 *      Advertise messages advertise information of interest to the
 *      client.  For example, the client may choose a server that returned
 *      an advertisement with configuration options of interest to the
 *      client.
 *
 *   -  The client MAY choose a less-preferred server if that server has a
 *      better set of advertised parameters, such as the available
 *      addresses advertised in IAs.
 *
 * Note that the first and third contradict each other.  The third should
 * probably be taken to mean that the client should prefer answers that
 * offer bindings, even if that violates the preference rule.
 *
 * The above also isn't deterministic where there are ties.  So the final
 * tiebreaker we add, if all other values are equal, is to compare the
 * server identifiers and to select the numerically lower one.
 */
static struct dhc6_lease *
dhc6_best_lease(struct client_state *client, struct dhc6_lease **head)
{
	struct dhc6_lease **rpos, *rval, **candp, *cand;
	int cscore, rscore;

	if (head == NULL || *head == NULL)
		return NULL;

	rpos = head;
	rval = *rpos;
	rscore = dhc6_score_lease(client, rval);
	candp = &rval->next;
	cand = *candp;

	log_debug("PRC: Considering best lease.");
	log_debug("PRC:  X-- Initial candidate %s (s: %d, p: %u).",
		  print_hex_1(rval->server_id.len,
			      rval->server_id.data, 48),
		  rscore, (unsigned)rval->pref);

	for (; cand != NULL ; candp = &cand->next, cand = *candp) {
		cscore = dhc6_score_lease(client, cand);

		log_debug("PRC:  X-- Candidate %s (s: %d, p: %u).",
			  print_hex_1(cand->server_id.len,
				      cand->server_id.data, 48),
			  cscore, (unsigned)cand->pref);

		/* Above you'll find quoted RFC3315 Section 17.1.3.
		 *
		 * The third clause tells us to give up on leases that
		 * have no bindings even if their preference is better.
		 * So where our 'selected' lease's score is less than 150
		 * (1 ia + 1 addr), choose any candidate >= 150.
		 *
		 * The first clause tells us to make preference the primary
		 * deciding factor.  So if it's lower, reject, if it's
		 * higher, select.
		 *
		 * The second clause tells us where the preference is
		 * equal, we should use 'our judgement' of what we like
		 * to see in an advertisement primarily.
		 *
		 * But there can still be a tie.  To make this deterministic,
		 * we compare the server identifiers and select the binary
		 * lowest.
		 *
		 * Since server id's are unique in this list, there is
		 * no further tie to break.
		 */
		if ((rscore < 150) && (cscore >= 150)) {
			log_debug("PRC:  | X-- Selected, has bindings.");
		} else if (cand->pref < rval->pref) {
			log_debug("PRC:  | X-- Rejected, lower preference.");
			continue;
		} else if (cand->pref > rval->pref) {
			log_debug("PRC:  | X-- Selected, higher preference.");
		} else if (cscore > rscore) {
			log_debug("PRC:  | X-- Selected, equal preference, "
				  "higher score.");
		} else if (cscore < rscore) {
			log_debug("PRC:  | X-- Rejected, equal preference, "
				  "lower score.");
			continue;
		} else if ((cand->server_id.len < rval->server_id.len) ||
			   ((cand->server_id.len == rval->server_id.len) &&
			    (memcmp(cand->server_id.data,
				    rval->server_id.data,
				    cand->server_id.len) < 0))) {
			log_debug("PRC:  | X-- Selected, equal preference, "
				  "equal score, binary lesser server ID.");
		} else {
			log_debug("PRC:  | X-- Rejected, equal preference, "
				  "equal score, binary greater server ID.");
			continue;
		}

		rpos = candp;
		rval = cand;
		rscore = cscore;
	}

	/* Remove the selected lease from the chain. */
	*rpos = rval->next;

	return rval;
}

/* Select a lease out of the advertised leases and setup state to try and
 * acquire that lease.
 */
void
start_selecting6(struct client_state *client)
{
	struct dhc6_lease *lease;

	if (client->advertised_leases == NULL) {
		log_error("Can not enter DHCPv6 SELECTING state with no "
			  "leases to select from!");
		return;
	}

	log_debug("PRC: Selecting best advertised lease.");
	client->state = S_SELECTING;

	lease = dhc6_best_lease(client, &client->advertised_leases);

	if (lease == NULL)
		log_fatal("Impossible error at %s:%d.", MDL);

	client->selected_lease = lease;

	/* Set timers per RFC3315 section 18.1.1. */
	client->IRT = REQ_TIMEOUT * 100;
	client->MRT = REQ_MAX_RT * 100;
	client->MRC = REQ_MAX_RC;
	client->MRD = 0;

	dhc6_retrans_init(client);

	client->v6_handler = reply_handler;

	/* ("re")transmit the first packet. */
	do_select6(client);
}

/* Transmit a Request to select a lease offered in Advertisements.  In
 * the event of failure, either move on to the next-best advertised lease,
 * or head back to INIT state if there are none.
 */
void
do_select6(void *input)
{
	struct client_state *client;
	struct dhc6_lease *lease;
	struct data_string ds;
	struct timeval tv;
	int send_ret;

	client = input;

	/* 'lease' is fewer characters to type. */
	lease = client->selected_lease;
	if (lease == NULL || lease->bindings == NULL) {
		log_error("Illegal to attempt selection without selecting "
			  "a lease.");
		return;
	}

	switch(check_timing6(client, DHCPV6_REQUEST, "Request", lease, &ds)) {
	      case CHK_TIM_MRC_EXCEEDED:
	      case CHK_TIM_MRD_EXCEEDED:
		log_debug("PRC: Lease %s failed.",
			  print_hex_1(lease->server_id.len,
				      lease->server_id.data, 56));

		/* Get rid of the lease that timed/counted out. */
		dhc6_lease_destroy(&lease, MDL);
		client->selected_lease = NULL;

		/* If there are more leases great.  If not, get more. */
		if (client->advertised_leases != NULL)
			start_selecting6(client);
		else
			start_init6(client);
		return;
	      case CHK_TIM_ALLOC_FAILURE:
		return;
	      case CHK_TIM_SUCCESS:
		break;
	}

	/* Now make a packet that looks suspiciously like the one we
	 * got from the server.  But different.
	 *
	 * XXX: I guess IAID is supposed to be something the client
	 * indicates and uses as a key to its internal state.  It is
	 * kind of odd to ask the server for IA's whose IAID the client
	 * did not manufacture.  We first need a formal dhclient.conf
	 * construct for the iaid, then we can delve into this matter
	 * more properly.  In the time being, this will work.
	 */

	/* Fetch any configured 'sent' options (includes DUID) in wire format.
	 */
	dhcpv6_universe.encapsulate(&ds, NULL, NULL, client,
				    NULL, client->sent_options, &global_scope,
				    &dhcpv6_universe);

	/* Now append any IA's, and within them any IAADDR/IAPREFIXs. */
	if (wanted_ia_na &&
	    dhc6_add_ia_na(client, &ds, lease,
			   DHCPV6_REQUEST) != ISC_R_SUCCESS) {
		data_string_forget(&ds, MDL);
		return;
	}
	if (wanted_ia_ta &&
	    dhc6_add_ia_ta(client, &ds, lease,
			   DHCPV6_REQUEST) != ISC_R_SUCCESS) {
		data_string_forget(&ds, MDL);
		return;
	}
	if (wanted_ia_pd &&
	    dhc6_add_ia_pd(client, &ds, lease,
			   DHCPV6_REQUEST) != ISC_R_SUCCESS) {
		data_string_forget(&ds, MDL);
		return;
	}

	log_info("XMT: Request on %s, interval %ld0ms.",
		 client->name ? client->name : client->interface->name,
		 (long int)client->RT);

	send_ret = send_packet6(client->interface,
				ds.data, ds.len, &DHCPv6DestAddr);
	if (send_ret != ds.len) {
		log_error("dhc6: send_packet6() sent %d of %d bytes",
			  send_ret, ds.len);
	}

	data_string_forget(&ds, MDL);

	/* Wait RT */
	tv.tv_sec = cur_tv.tv_sec + client->RT / 100;
	tv.tv_usec = cur_tv.tv_usec + (client->RT % 100) * 10000;
	if (tv.tv_usec >= 1000000) {
		tv.tv_sec += 1;
		tv.tv_usec -= 1000000;
	}
	add_timeout(&tv, do_select6, client, NULL, NULL);

	dhc6_retrans_advance(client);
}

/* For each IA_NA in the lease, for each address in the IA_NA,
 * append that information onto the packet-so-far.
 */
static isc_result_t
dhc6_add_ia_na(struct client_state *client, struct data_string *packet,
	       struct dhc6_lease *lease, u_int8_t message)
{
	struct data_string iads;
	struct data_string addrds;
	struct dhc6_addr *addr;
	struct dhc6_ia *ia;
	isc_result_t rval = ISC_R_SUCCESS;
	TIME t1, t2;

	memset(&iads, 0, sizeof(iads));
	memset(&addrds, 0, sizeof(addrds));
	for (ia = lease->bindings;
	     ia != NULL && rval == ISC_R_SUCCESS;
	     ia = ia->next) {
		if (ia->ia_type != D6O_IA_NA)
			continue;

		if (!buffer_allocate(&iads.buffer, 12, MDL)) {
			log_error("Unable to allocate memory for IA_NA.");
			rval = ISC_R_NOMEMORY;
			break;
		}

		/* Copy the IAID into the packet buffer. */
		memcpy(iads.buffer->data, ia->iaid, 4);
		iads.data = iads.buffer->data;
		iads.len = 12;

		switch (message) {
		      case DHCPV6_REQUEST:
		      case DHCPV6_RENEW:
		      case DHCPV6_REBIND:

			t1 = client->config->requested_lease / 2;
			t2 = t1 + (t1 / 2);
#if MAX_TIME > 0xffffffff
			if (t1 > 0xffffffff)
				t1 = 0xffffffff;
			if (t2 > 0xffffffff)
				t2 = 0xffffffff;
#endif
			putULong(iads.buffer->data + 4, t1);
			putULong(iads.buffer->data + 8, t2);

			log_debug("XMT:  X-- IA_NA %s",
				  print_hex_1(4, iads.data, 59));
			log_debug("XMT:  | X-- Requested renew  +%u",
				  (unsigned) t1);
			log_debug("XMT:  | X-- Requested rebind +%u",
				  (unsigned) t2);
			break;

		      case DHCPV6_CONFIRM:
		      case DHCPV6_RELEASE:
		      case DHCPV6_DECLINE:
			/* Set t1 and t2 to zero; server will ignore them */
			memset(iads.buffer->data + 4, 0, 8);
			log_debug("XMT:  X-- IA_NA %s",
				  print_hex_1(4, iads.buffer->data, 55));

			break;

		      default:
			log_fatal("Impossible condition at %s:%d.", MDL);
		}

		for (addr = ia->addrs ; addr != NULL ; addr = addr->next) {
			/*
			 * Do not confirm expired addresses, do not request
			 * expired addresses (but we keep them around for
			 * solicit).
			 */
			if (addr->flags & DHC6_ADDR_EXPIRED)
				continue;

			if (addr->address.len != 16) {
				log_error("Illegal IPv6 address length (%d), "
					  "ignoring.  (%s:%d)",
					  addr->address.len, MDL);
				continue;
			}

			if (!buffer_allocate(&addrds.buffer, 24, MDL)) {
				log_error("Unable to allocate memory for "
					  "IAADDR.");
				rval = ISC_R_NOMEMORY;
				break;
			}

			addrds.data = addrds.buffer->data;
			addrds.len = 24;

			/* Copy the address into the packet buffer. */
			memcpy(addrds.buffer->data, addr->address.iabuf, 16);

			/* Copy in additional information as appropriate */
			switch (message) {
			      case DHCPV6_REQUEST:
			      case DHCPV6_RENEW:
			      case DHCPV6_REBIND:
				t1 = client->config->requested_lease;
				t2 = t1 + 300;
				putULong(addrds.buffer->data + 16, t1);
				putULong(addrds.buffer->data + 20, t2);

				log_debug("XMT:  | | X-- IAADDR %s",
					  piaddr(addr->address));
				log_debug("XMT:  | | | X-- Preferred "
					  "lifetime +%u", (unsigned)t1);
				log_debug("XMT:  | | | X-- Max lifetime +%u",
					  (unsigned)t2);

				break;

			      case DHCPV6_CONFIRM:
				/*
				 * Set preferred and max life to zero,
				 * per 17.1.3.
				 */
				memset(addrds.buffer->data + 16, 0, 8);
				log_debug("XMT:  | X-- Confirm Address %s",
					  piaddr(addr->address));
				break;

			      case DHCPV6_RELEASE:
				/* Preferred and max life are irrelevant */
				memset(addrds.buffer->data + 16, 0, 8);
				log_debug("XMT:  | X-- Release Address %s",
					  piaddr(addr->address));
				break;

			      case DHCPV6_DECLINE:
				/* Preferred and max life are irrelevant */
				memset(addrds.buffer->data + 16, 0, 8);
				log_debug("XMT:  | X-- Decline Address %s",
					  piaddr(addr->address));
				break;

			      default:
				log_fatal("Impossible condition at %s:%d.",
					  MDL);
			}

			append_option(&iads, &dhcpv6_universe, iaaddr_option,
				      &addrds);
			data_string_forget(&addrds, MDL);
		}

		/*
		 * It doesn't make sense to make a request without an
		 * address.
		 */
		if (ia->addrs == NULL) {
			log_debug("!!!:  V IA_NA has no IAADDRs - removed.");
			rval = ISC_R_FAILURE;
		} else if (rval == ISC_R_SUCCESS) {
			log_debug("XMT:  V IA_NA appended.");
			append_option(packet, &dhcpv6_universe, ia_na_option,
				      &iads);
		}

		data_string_forget(&iads, MDL);
	}

	return rval;
}

/* For each IA_TA in the lease, for each address in the IA_TA,
 * append that information onto the packet-so-far.
 */
static isc_result_t
dhc6_add_ia_ta(struct client_state *client, struct data_string *packet,
	       struct dhc6_lease *lease, u_int8_t message)
{
	struct data_string iads;
	struct data_string addrds;
	struct dhc6_addr *addr;
	struct dhc6_ia *ia;
	isc_result_t rval = ISC_R_SUCCESS;
	TIME t1, t2;

	memset(&iads, 0, sizeof(iads));
	memset(&addrds, 0, sizeof(addrds));
	for (ia = lease->bindings;
	     ia != NULL && rval == ISC_R_SUCCESS;
	     ia = ia->next) {
		if (ia->ia_type != D6O_IA_TA)
			continue;

		if (!buffer_allocate(&iads.buffer, 4, MDL)) {
			log_error("Unable to allocate memory for IA_TA.");
			rval = ISC_R_NOMEMORY;
			break;
		}

		/* Copy the IAID into the packet buffer. */
		memcpy(iads.buffer->data, ia->iaid, 4);
		iads.data = iads.buffer->data;
		iads.len = 4;

		log_debug("XMT:  X-- IA_TA %s",
			  print_hex_1(4, iads.buffer->data, 55));

		for (addr = ia->addrs ; addr != NULL ; addr = addr->next) {
			/*
			 * Do not confirm expired addresses, do not request
			 * expired addresses (but we keep them around for
			 * solicit).
			 */
			if (addr->flags & DHC6_ADDR_EXPIRED)
				continue;

			if (addr->address.len != 16) {
				log_error("Illegal IPv6 address length (%d), "
					  "ignoring.  (%s:%d)",
					  addr->address.len, MDL);
				continue;
			}

			if (!buffer_allocate(&addrds.buffer, 24, MDL)) {
				log_error("Unable to allocate memory for "
					  "IAADDR.");
				rval = ISC_R_NOMEMORY;
				break;
			}

			addrds.data = addrds.buffer->data;
			addrds.len = 24;

			/* Copy the address into the packet buffer. */
			memcpy(addrds.buffer->data, addr->address.iabuf, 16);

			/* Copy in additional information as appropriate */
			switch (message) {
			      case DHCPV6_REQUEST:
			      case DHCPV6_RENEW:
			      case DHCPV6_REBIND:
				t1 = client->config->requested_lease;
				t2 = t1 + 300;
				putULong(addrds.buffer->data + 16, t1);
				putULong(addrds.buffer->data + 20, t2);

				log_debug("XMT:  | | X-- IAADDR %s",
					  piaddr(addr->address));
				log_debug("XMT:  | | | X-- Preferred "
					  "lifetime +%u", (unsigned)t1);
				log_debug("XMT:  | | | X-- Max lifetime +%u",
					  (unsigned)t2);

				break;

			      case DHCPV6_CONFIRM:
				/*
				 * Set preferred and max life to zero,
				 * per 17.1.3.
				 */
				memset(addrds.buffer->data + 16, 0, 8);
				log_debug("XMT:  | X-- Confirm Address %s",
					  piaddr(addr->address));
				break;

			      case DHCPV6_RELEASE:
				/* Preferred and max life are irrelevant */
				memset(addrds.buffer->data + 16, 0, 8);
				log_debug("XMT:  | X-- Release Address %s",
					  piaddr(addr->address));
				break;

			      default:
				log_fatal("Impossible condition at %s:%d.",
					  MDL);
			}

			append_option(&iads, &dhcpv6_universe, iaaddr_option,
				      &addrds);
			data_string_forget(&addrds, MDL);
		}

		/*
		 * It doesn't make sense to make a request without an
		 * address.
		 */
		if (ia->addrs == NULL) {
			log_debug("!!!:  V IA_TA has no IAADDRs - removed.");
			rval = ISC_R_FAILURE;
		} else if (rval == ISC_R_SUCCESS) {
			log_debug("XMT:  V IA_TA appended.");
			append_option(packet, &dhcpv6_universe, ia_ta_option,
				      &iads);
		}

		data_string_forget(&iads, MDL);
	}

	return rval;
}

/* For each IA_PD in the lease, for each prefix in the IA_PD,
 * append that information onto the packet-so-far.
 */
static isc_result_t
dhc6_add_ia_pd(struct client_state *client, struct data_string *packet,
	       struct dhc6_lease *lease, u_int8_t message)
{
	struct data_string iads;
	struct data_string prefds;
	struct dhc6_addr *pref;
	struct dhc6_ia *ia;
	isc_result_t rval = ISC_R_SUCCESS;
	TIME t1, t2;

	memset(&iads, 0, sizeof(iads));
	memset(&prefds, 0, sizeof(prefds));
	for (ia = lease->bindings;
	     ia != NULL && rval == ISC_R_SUCCESS;
	     ia = ia->next) {
		if (ia->ia_type != D6O_IA_PD)
			continue;

		if (!buffer_allocate(&iads.buffer, 12, MDL)) {
			log_error("Unable to allocate memory for IA_PD.");
			rval = ISC_R_NOMEMORY;
			break;
		}

		/* Copy the IAID into the packet buffer. */
		memcpy(iads.buffer->data, ia->iaid, 4);
		iads.data = iads.buffer->data;
		iads.len = 12;

		switch (message) {
		      case DHCPV6_REQUEST:
		      case DHCPV6_RENEW:
		      case DHCPV6_REBIND:

			t1 = client->config->requested_lease / 2;
			t2 = t1 + (t1 / 2);
#if MAX_TIME > 0xffffffff
			if (t1 > 0xffffffff)
				t1 = 0xffffffff;
			if (t2 > 0xffffffff)
				t2 = 0xffffffff;
#endif
			putULong(iads.buffer->data + 4, t1);
			putULong(iads.buffer->data + 8, t2);

			log_debug("XMT:  X-- IA_PD %s",
				  print_hex_1(4, iads.data, 59));
			log_debug("XMT:  | X-- Requested renew  +%u",
				  (unsigned) t1);
			log_debug("XMT:  | X-- Requested rebind +%u",
				  (unsigned) t2);
			break;

		      case DHCPV6_RELEASE:
			/* Set t1 and t2 to zero; server will ignore them */
			memset(iads.buffer->data + 4, 0, 8);
			log_debug("XMT:  X-- IA_PD %s",
				  print_hex_1(4, iads.buffer->data, 55));

			break;

		      default:
			log_fatal("Impossible condition at %s:%d.", MDL);
		}

		for (pref = ia->addrs ; pref != NULL ; pref = pref->next) {
			/*
			 * Do not confirm expired prefixes, do not request
			 * expired prefixes (but we keep them around for
			 * solicit).
			 */
			if (pref->flags & DHC6_ADDR_EXPIRED)
				continue;

			if (pref->address.len != 16) {
				log_error("Illegal IPv6 prefix "
					  "ignoring.  (%s:%d)",
					  MDL);
				continue;
			}

			if (pref->plen == 0) {
				log_info("Null IPv6 prefix, "
					 "ignoring. (%s:%d)",
					 MDL);
			}

			if (!buffer_allocate(&prefds.buffer, 25, MDL)) {
				log_error("Unable to allocate memory for "
					  "IAPREFIX.");
				rval = ISC_R_NOMEMORY;
				break;
			}

			prefds.data = prefds.buffer->data;
			prefds.len = 25;

			/* Copy the prefix into the packet buffer. */
			putUChar(prefds.buffer->data + 8, pref->plen);
			memcpy(prefds.buffer->data + 9,
			       pref->address.iabuf,
			       16);

			/* Copy in additional information as appropriate */
			switch (message) {
			      case DHCPV6_REQUEST:
			      case DHCPV6_RENEW:
			      case DHCPV6_REBIND:
				t1 = client->config->requested_lease;
				t2 = t1 + 300;
				putULong(prefds.buffer->data, t1);
				putULong(prefds.buffer->data + 4, t2);

				log_debug("XMT:  | | X-- IAPREFIX %s/%u",
					  piaddr(pref->address),
					  (unsigned) pref->plen);
				log_debug("XMT:  | | | X-- Preferred "
					  "lifetime +%u", (unsigned)t1);
				log_debug("XMT:  | | | X-- Max lifetime +%u",
					  (unsigned)t2);

				break;

			      case DHCPV6_RELEASE:
				/* Preferred and max life are irrelevant */
				memset(prefds.buffer->data, 0, 8);
				log_debug("XMT:  | X-- Release Prefix %s/%u",
					  piaddr(pref->address),
					  (unsigned) pref->plen);
				break;

			      default:
				log_fatal("Impossible condition at %s:%d.",
					  MDL);
			}

			append_option(&iads, &dhcpv6_universe,
				      iaprefix_option, &prefds);
			data_string_forget(&prefds, MDL);
		}

		/*
		 * It doesn't make sense to make a request without an
		 * address.
		 */
		if (ia->addrs == NULL) {
			log_debug("!!!:  V IA_PD has no IAPREFIXs - removed.");
			rval = ISC_R_FAILURE;
		} else if (rval == ISC_R_SUCCESS) {
			log_debug("XMT:  V IA_PD appended.");
			append_option(packet, &dhcpv6_universe,
				      ia_pd_option, &iads);
		}

		data_string_forget(&iads, MDL);
	}

	return rval;
}

/* stopping_finished() checks if there is a remaining work to do.
 */
static isc_boolean_t
stopping_finished(void)
{
	struct interface_info *ip;
	struct client_state *client;

	for (ip = interfaces; ip; ip = ip -> next) {
		for (client = ip -> client; client; client = client -> next) {
			if (client->state != S_STOPPED)
				return ISC_FALSE;
			if (client->active_lease != NULL)
				return ISC_FALSE;
		}
	}
	return ISC_TRUE;
}

/* reply_handler() accepts a Reply while we're attempting Select or Renew or
 * Rebind.  Basically any Reply packet.
 */
void
reply_handler(struct packet *packet, struct client_state *client)
{
	struct dhc6_lease *lease;
	isc_result_t check_status;

	if (packet->dhcpv6_msg_type != DHCPV6_REPLY)
		return;

	/* RFC3315 section 15.10 validation (same as 15.3 since we
	 * always include a client id).
	 */
	if (!valid_reply(packet, client)) {
		log_error("Invalid Reply - rejecting.");
		return;
	}

	lease = dhc6_leaseify(packet);

	/* This is an out of memory condition...hopefully a temporary
	 * problem.  Returning now makes us try to retransmit later.
	 */
	if (lease == NULL)
		return;

	check_status = dhc6_check_reply(client, lease);
	if (check_status != ISC_R_SUCCESS) {
		dhc6_lease_destroy(&lease, MDL);

		/* If no action was taken, but there is an error, then
		 * we wait for a retransmission.
		 */
		if (check_status != ISC_R_CANCELED)
			return;
	}

	/* We're done retransmitting at this point. */
	cancel_timeout(do_confirm6, client);
	cancel_timeout(do_select6, client);
	cancel_timeout(do_refresh6, client);
	cancel_timeout(do_release6, client);

	/* If this is in response to a Release/Decline, clean up and return. */
	if (client->state == S_STOPPED) {
		if (client->active_lease == NULL)
			return;

		dhc6_lease_destroy(&client->active_lease, MDL);
		client->active_lease = NULL;
		/* We should never wait for nothing!? */
		if (stopping_finished())
			exit(0);
		return;
	}

	/* Action was taken, so now that we've torn down our scheduled
	 * retransmissions, return.
	 */
	if (check_status == ISC_R_CANCELED)
		return;

	if (client->selected_lease != NULL) {
		dhc6_lease_destroy(&client->selected_lease, MDL);
		client->selected_lease = NULL;
	}

	/* If this is in response to a confirm, we use the lease we've
	 * already got, not the reply we were sent.
	 */
	if (client->state == S_REBOOTING) {
		if (client->active_lease == NULL)
			log_fatal("Impossible condition at %s:%d.", MDL);

		dhc6_lease_destroy(&lease, MDL);
		start_bound(client);
		return;
	}

	/* Merge any bindings in the active lease (if there is one) into
	 * the new active lease.
	 */
	dhc6_merge_lease(client->active_lease, lease);

	/* Cleanup if a previous attempt to go bound failed. */
	if (client->old_lease != NULL) {
		dhc6_lease_destroy(&client->old_lease, MDL);
		client->old_lease = NULL;
	}

	/* Make this lease active and BIND to it. */
	if (client->active_lease != NULL)
		client->old_lease = client->active_lease;
	client->active_lease = lease;

	/* We're done with the ADVERTISEd leases, if any. */
	while(client->advertised_leases != NULL) {
		lease = client->advertised_leases;
		client->advertised_leases = lease->next;

		dhc6_lease_destroy(&lease, MDL);
	}

	start_bound(client);
}

/* DHCPv6 packets are a little sillier than they needed to be - the root
 * packet contains options, then IA's which contain options, then within
 * that IAADDR's which contain options.
 *
 * To sort this out at dhclient-script time (which fetches config parameters
 * in environment variables), start_bound() iterates over each IAADDR, and
 * calls this function to marshall an environment variable set that includes
 * the most-specific option values related to that IAADDR in particular.
 *
 * To achieve this, we load environment variables for the root options space,
 * then the IA, then the IAADDR.  Any duplicate option names will be
 * over-written by the later versions.
 */
static void
dhc6_marshall_values(const char *prefix, struct client_state *client,
		     struct dhc6_lease *lease, struct dhc6_ia *ia,
		     struct dhc6_addr *addr)
{
	/* Option cache contents, in descending order of
	 * scope.
	 */
	if ((lease != NULL) && (lease->options != NULL))
		script_write_params6(client, prefix, lease->options);
	if ((ia != NULL) && (ia->options != NULL))
		script_write_params6(client, prefix, ia->options);
	if ((addr != NULL) && (addr->options != NULL))
		script_write_params6(client, prefix, addr->options);

	/* addr fields. */
	if (addr != NULL) {
		if ((ia != NULL) && (ia->ia_type == D6O_IA_PD)) {
			client_envadd(client, prefix,
				      "ip6_prefix", "%s/%u",
				      piaddr(addr->address),
				      (unsigned) addr->plen);
		} else {
			/* Current practice is that all subnets are /64's, but
			 * some suspect this may not be permanent.
			 */
			client_envadd(client, prefix, "ip6_prefixlen",
				      "%d", 64);
			client_envadd(client, prefix, "ip6_address",
				      "%s", piaddr(addr->address));
		}
		if ((ia != NULL) && (ia->ia_type == D6O_IA_TA)) {
			client_envadd(client, prefix,
				      "ip6_type", "temporary");
		}
		client_envadd(client, prefix, "life_starts", "%d",
			      (int)(addr->starts));
		client_envadd(client, prefix, "preferred_life", "%d",
			      (int)(addr->preferred_life));
		client_envadd(client, prefix, "max_life", "%d",
			      (int)(addr->max_life));
	}

	/* ia fields. */
	if (ia != NULL) {
		client_envadd(client, prefix, "iaid", "%s",
			      print_hex_1(4, ia->iaid, 12));
		client_envadd(client, prefix, "starts", "%d",
			      (int)(ia->starts));
		client_envadd(client, prefix, "renew", "%u", ia->renew);
		client_envadd(client, prefix, "rebind", "%u", ia->rebind);
	}
}

/* Look at where the client's active lease is sitting.  If it's looking to
 * time out on renew, rebind, depref, or expiration, do those things.
 */
static void
dhc6_check_times(struct client_state *client)
{
	struct dhc6_lease *lease;
	struct dhc6_ia *ia;
	struct dhc6_addr *addr;
	TIME renew=MAX_TIME, rebind=MAX_TIME, depref=MAX_TIME,
	     lo_expire=MAX_TIME, hi_expire=0, tmp;
	int has_addrs = ISC_FALSE;
	struct timeval tv;

	lease = client->active_lease;

	/* Bit spammy.  We should probably keep record of scheduled
	 * events instead.
	 */
	cancel_timeout(start_renew6, client);
	cancel_timeout(start_rebind6, client);
	cancel_timeout(do_depref, client);
	cancel_timeout(do_expire, client);

	for(ia = lease->bindings ; ia != NULL ; ia = ia->next) {
		TIME this_ia_lo_expire, this_ia_hi_expire, use_expire;

		this_ia_lo_expire = MAX_TIME;
		this_ia_hi_expire = 0;

		for (addr = ia->addrs ; addr != NULL ; addr = addr->next) {
			if(!(addr->flags & DHC6_ADDR_DEPREFFED)) {
				if (addr->preferred_life == 0xffffffff)
					tmp = MAX_TIME;
				else
					tmp = addr->starts +
					      addr->preferred_life;

				if (tmp < depref)
					depref = tmp;
			}

			if (!(addr->flags & DHC6_ADDR_EXPIRED)) {
				/* Find EPOCH-relative expiration. */
				if (addr->max_life == 0xffffffff)
					tmp = MAX_TIME;
				else
					tmp = addr->starts + addr->max_life;

				/* Make the times ia->starts relative. */
				tmp -= ia->starts;

				if (tmp > this_ia_hi_expire)
					this_ia_hi_expire = tmp;
				if (tmp < this_ia_lo_expire)
					this_ia_lo_expire = tmp;

				has_addrs = ISC_TRUE;
			}
		}

		/* These times are ia->starts relative. */
		if (this_ia_lo_expire <= (this_ia_hi_expire / 2))
			use_expire = this_ia_hi_expire;
		else
			use_expire = this_ia_lo_expire;

		/*
		 * If the auto-selected expiration time is "infinite", or
		 * zero, assert a reasonable default.
		 */
		if ((use_expire == MAX_TIME) || (use_expire <= 1))
			use_expire = client->config->requested_lease / 2;
		else
			use_expire /= 2;

		/* Don't renew/rebind temporary addresses. */
		if (ia->ia_type != D6O_IA_TA) {

			if (ia->renew == 0) {
				tmp = ia->starts + use_expire;
			} else if (ia->renew == 0xffffffff)
				tmp = MAX_TIME;
			else
				tmp = ia->starts + ia->renew;

			if (tmp < renew)
				renew = tmp;

			if (ia->rebind == 0) {
				/* Set rebind to 3/4 expiration interval. */
				tmp = ia->starts;
				tmp += use_expire + (use_expire / 2);
			} else if (ia->rebind == 0xffffffff)
				tmp = MAX_TIME;
			else
				tmp = ia->starts + ia->rebind;

			if (tmp < rebind)
				rebind = tmp;
		}

		/*
		 * Return expiration ranges to EPOCH relative for event
		 * scheduling (add_timeout()).
		 */
		this_ia_hi_expire += ia->starts;
		this_ia_lo_expire += ia->starts;

		if (this_ia_hi_expire > hi_expire)
			hi_expire = this_ia_hi_expire;
		if (this_ia_lo_expire < lo_expire)
			lo_expire = this_ia_lo_expire;
	}

	/* If there are no addresses, give up, go to INIT.
	 * Note that if an address is unexpired with a date in the past,
	 * we're scheduling an expiration event to ocurr in the past.  We
	 * could probably optimize this to expire now (but then there's
	 * recursion).
	 *
	 * In the future, we may decide that we're done here, or to
	 * schedule a future request (using 4-pkt info-request model).
	 */
	if (has_addrs == ISC_FALSE) {
		dhc6_lease_destroy(&client->active_lease, MDL);
		client->active_lease = NULL;

		/* Go back to the beginning. */
		start_init6(client);
		return;
	}

	switch(client->state) {
	      case S_BOUND:
		/* We'd like to hit renewing, but if rebinding has already
		 * passed (time warp), head straight there.
		 */
		if ((rebind > cur_time) && (renew < rebind)) {
			log_debug("PRC: Renewal event scheduled in %d seconds, "
				  "to run for %u seconds.",
				  (int)(renew - cur_time),
				  (unsigned)(rebind - renew));
			client->next_MRD = rebind;
			tv.tv_sec = renew;
			tv.tv_usec = 0;
			add_timeout(&tv, start_renew6, client, NULL, NULL);

			break;
		}
		/* FALL THROUGH */
	      case S_RENEWING:
		/* While actively renewing, MRD is bounded by the time
		 * we stop renewing and start rebinding.  This helps us
		 * process the state change on time.
		 */
		client->MRD = rebind - cur_time;
		if (rebind != MAX_TIME) {
			log_debug("PRC: Rebind event scheduled in %d seconds, "
				  "to run for %d seconds.",
				  (int)(rebind - cur_time),
				  (int)(hi_expire - rebind));
			client->next_MRD = hi_expire;
			tv.tv_sec = rebind;
			tv.tv_usec = 0;
			add_timeout(&tv, start_rebind6, client, NULL, NULL);
		}
		break;

	      case S_REBINDING:
		/* For now, we rebind up until the last lease expires.  In
		 * the future, we might want to start SOLICITing when we've
		 * depreffed an address.
		 */
		client->MRD = hi_expire - cur_time;
		break;

	      default:
		log_fatal("Impossible condition at %s:%d.", MDL);
	}

	/* Separately, set a time at which we will depref and expire
	 * leases.  This might happen with multiple addresses while we
	 * keep trying to refresh.
	 */
	if (depref != MAX_TIME) {
		log_debug("PRC: Depreference scheduled in %d seconds.",
			  (int)(depref - cur_time));
		tv.tv_sec = depref;
		tv.tv_usec = 0;
		add_timeout(&tv, do_depref, client, NULL, NULL);
	}
	if (lo_expire != MAX_TIME) {
		log_debug("PRC: Expiration scheduled in %d seconds.",
			  (int)(lo_expire - cur_time));
		tv.tv_sec = lo_expire;
		tv.tv_usec = 0;
		add_timeout(&tv, do_expire, client, NULL, NULL);
	}
}

/* In a given IA chain, find the IA with the same type and 'iaid'. */
static struct dhc6_ia *
find_ia(struct dhc6_ia *head, u_int16_t type, const char *id)
{
	struct dhc6_ia *ia;

	for (ia = head ; ia != NULL ; ia = ia->next) {
		if (ia->ia_type != type)
			continue;
		if (memcmp(ia->iaid, id, 4) == 0)
			return ia;
	}

	return NULL;
}

/* In a given address chain, find a matching address. */
static struct dhc6_addr *
find_addr(struct dhc6_addr *head, struct iaddr *address)
{
	struct dhc6_addr *addr;

	for (addr = head ; addr != NULL ; addr = addr->next) {
		if ((addr->address.len == address->len) &&
		    (memcmp(addr->address.iabuf, address->iabuf,
			    address->len) == 0))
			return addr;
	}

	return NULL;
}

/* In a given prefix chain, find a matching prefix. */
static struct dhc6_addr *
find_pref(struct dhc6_addr *head, struct iaddr *prefix, u_int8_t plen)
{
	struct dhc6_addr *pref;

	for (pref = head ; pref != NULL ; pref = pref->next) {
		if ((pref->address.len == prefix->len) &&
		    (pref->plen == plen) &&
		    (memcmp(pref->address.iabuf, prefix->iabuf,
			    prefix->len) == 0))
			return pref;
	}

	return NULL;
}

/* Merge the bindings from the source lease into the destination lease
 * structure, where they are missing.  We have to copy the stateful
 * objects rather than move them over, because later code needs to be
 * able to compare new versus old if they contain any bindings.
 */
static void
dhc6_merge_lease(struct dhc6_lease *src, struct dhc6_lease *dst)
{
	struct dhc6_ia *sia, *dia, *tia;
	struct dhc6_addr *saddr, *daddr, *taddr;
	int changes = 0;

	if ((dst == NULL) || (src == NULL))
		return;

	for (sia = src->bindings ; sia != NULL ; sia = sia->next) {
		dia = find_ia(dst->bindings, sia->ia_type, (char *)sia->iaid);

		if (dia == NULL) {
			tia = dhc6_dup_ia(sia, MDL);

			if (tia == NULL)
				log_fatal("Out of memory merging lease - "
					  "Unable to continue without losing "
					  "state! (%s:%d)", MDL);

			/* XXX: consider sorting? */
			tia->next = dst->bindings;
			dst->bindings = tia;
			changes = 1;
		} else {
			for (saddr = sia->addrs ; saddr != NULL ;
			     saddr = saddr->next) {
				if (sia->ia_type != D6O_IA_PD)
					daddr = find_addr(dia->addrs,
							  &saddr->address);
				else
					daddr = find_pref(dia->addrs,
							  &saddr->address,
							  saddr->plen);

				if (daddr == NULL) {
					taddr = dhc6_dup_addr(saddr, MDL);

					if (taddr == NULL)
						log_fatal("Out of memory "
							  "merging lease - "
							  "Unable to continue "
							  "without losing "
							  "state! (%s:%d)",
							  MDL);

					/* XXX: consider sorting? */
					taddr->next = dia->addrs;
					dia->addrs = taddr;
					changes = 1;
				}
			}
		}
	}

	/* If we made changes, reset the score to 0 so it is recalculated. */
	if (changes)
		dst->score = 0;
}

/* We've either finished selecting or succeeded in Renew or Rebinding our
 * lease.  In all cases we got a Reply.  Give dhclient-script a tickle
 * to inform it about the new values, and then lay in wait for the next
 * event.
 */
static void
start_bound(struct client_state *client)
{
	struct dhc6_ia *ia, *oldia;
	struct dhc6_addr *addr, *oldaddr;
	struct dhc6_lease *lease, *old;
	const char *reason;
#if defined (NSUPDATE)
	TIME dns_update_offset = 1;
#endif

	lease = client->active_lease;
	if (lease == NULL) {
		log_error("Cannot enter bound state unless an active lease "
			  "is selected.");
		return;
	}
	lease->released = ISC_FALSE;
	old = client->old_lease;

	client->v6_handler = bound_handler;

	switch (client->state) {
	      case S_SELECTING:
	      case S_REBOOTING: /* Pretend we got bound. */
		reason = "BOUND6";
		break;

	      case S_RENEWING:
		reason = "RENEW6";
		break;

	      case S_REBINDING:
		reason = "REBIND6";
		break;

	      default:
		log_fatal("Impossible condition at %s:%d.", MDL);
		/* Silence compiler warnings. */
		return;
	}

	log_debug("PRC: Bound to lease %s.",
		  print_hex_1(client->active_lease->server_id.len,
			      client->active_lease->server_id.data, 55));
	client->state = S_BOUND;

	write_client6_lease(client, lease, 0, 1);

	oldia = NULL;
	for (ia = lease->bindings ; ia != NULL ; ia = ia->next) {
		if (old != NULL)
			oldia = find_ia(old->bindings,
					ia->ia_type,
					(char *)ia->iaid);
		else
			oldia = NULL;

		for (addr = ia->addrs ; addr != NULL ; addr = addr->next) {
			if (oldia != NULL) {
				if (ia->ia_type != D6O_IA_PD)
					oldaddr = find_addr(oldia->addrs,
							    &addr->address);
				else
					oldaddr = find_pref(oldia->addrs,
							    &addr->address,
							    addr->plen);
			} else
				oldaddr = NULL;

#if defined (NSUPDATE)
			if ((oldaddr == NULL) && (ia->ia_type == D6O_IA_NA))
				dhclient_schedule_updates(client,
							  &addr->address,
							  dns_update_offset++);
#endif

			/* Shell out to setup the new binding. */
			script_init(client, reason, NULL);

			if (old != NULL)
				dhc6_marshall_values("old_", client, old,
						     oldia, oldaddr);
			dhc6_marshall_values("new_", client, lease, ia, addr);
			script_write_requested6(client);

			script_go(client);
		}

		/* XXX: maybe we should loop on the old values instead? */
		if (ia->addrs == NULL) {
			script_init(client, reason, NULL);

			if (old != NULL)
				dhc6_marshall_values("old_", client, old,
						     oldia,
						     oldia != NULL ?
							 oldia->addrs : NULL);

			dhc6_marshall_values("new_", client, lease, ia,
					     NULL);
			script_write_requested6(client);

			script_go(client);
		}
	}

	/* XXX: maybe we should loop on the old values instead? */
	if (lease->bindings == NULL) {
		script_init(client, reason, NULL);

		if (old != NULL)
			dhc6_marshall_values("old_", client, old,
					     old->bindings,
					     (old->bindings != NULL) ?
						old->bindings->addrs : NULL);

		dhc6_marshall_values("new_", client, lease, NULL, NULL);
		script_write_requested6(client);

		script_go(client);
	}

	go_daemon();

	if (client->old_lease != NULL) {
		dhc6_lease_destroy(&client->old_lease, MDL);
		client->old_lease = NULL;
	}

	/* Schedule events. */
	dhc6_check_times(client);
}

/* While bound, ignore packets.  In the future we'll want to answer
 * Reconfigure-Request messages and the like.
 */
void
bound_handler(struct packet *packet, struct client_state *client)
{
	log_debug("RCV: Input packets are ignored once bound.");
}

/* start_renew6() gets us all ready to go to start transmitting Renew packets.
 * Note that client->next_MRD must be set before entering this function -
 * it must be set to the time at which the client should start Rebinding.
 */
void
start_renew6(void *input)
{
	struct client_state *client;

	client = (struct client_state *)input;

	log_info("PRC: Renewing lease on %s.",
		 client->name ? client->name : client->interface->name);
	client->state = S_RENEWING;

	client->v6_handler = reply_handler;

	/* Times per RFC3315 section 18.1.3. */
	client->IRT = REN_TIMEOUT * 100;
	client->MRT = REN_MAX_RT * 100;
	client->MRC = 0;
	/* MRD is special in renew - we need to set it by checking timer
	 * state.
	 */
	client->MRD = client->next_MRD - cur_time;

	dhc6_retrans_init(client);

	client->refresh_type = DHCPV6_RENEW;
	do_refresh6(client);
}

/* do_refresh6() transmits one DHCPv6 packet, be it a Renew or Rebind, and
 * gives the retransmission state a bump for the next time.  Note that
 * client->refresh_type must be set before entering this function.
 */
void
do_refresh6(void *input)
{
	struct option_cache *oc;
	struct sockaddr_in6 unicast, *dest_addr = &DHCPv6DestAddr;
	struct data_string ds;
	struct client_state *client;
	struct dhc6_lease *lease;
	struct timeval elapsed, tv;
	int send_ret;

	client = (struct client_state *)input;
	memset(&ds, 0, sizeof(ds));

	lease = client->active_lease;
	if (lease == NULL) {
		log_error("Cannot renew without an active binding.");
		return;
	}

	/* Ensure we're emitting a valid message type. */
	switch (client->refresh_type) {
	      case DHCPV6_RENEW:
	      case DHCPV6_REBIND:
		break;

	      default:
		log_fatal("Internal inconsistency (%d) at %s:%d.",
			  client->refresh_type, MDL);
	}

	/*
	 * Start_time starts at the first transmission.
	 */
	if (client->txcount == 0) {
		client->start_time.tv_sec = cur_tv.tv_sec;
		client->start_time.tv_usec = cur_tv.tv_usec;
	}

	/* elapsed = cur - start */
	elapsed.tv_sec = cur_tv.tv_sec - client->start_time.tv_sec;
	elapsed.tv_usec = cur_tv.tv_usec - client->start_time.tv_usec;
	if (elapsed.tv_usec < 0) {
		elapsed.tv_sec -= 1;
		elapsed.tv_usec += 1000000;
	}
	if (((client->MRC != 0) && (client->txcount > client->MRC)) ||
	    ((client->MRD != 0) && (elapsed.tv_sec >= client->MRD))) {
		/* We're done.  Move on to the next phase, if any. */
		dhc6_check_times(client);
		return;
	}

	/*
	 * Check whether the server has sent a unicast option; if so, we can
	 * use the address it specified for RENEWs.
	 */
	oc = lookup_option(&dhcpv6_universe, lease->options, D6O_UNICAST);
	if (oc && evaluate_option_cache(&ds, NULL, NULL, NULL,
					lease->options, NULL, &global_scope,
					oc, MDL)) {
		if (ds.len < 16) {
			log_error("Invalid unicast option length %d.", ds.len);
		} else {
			memset(&unicast, 0, sizeof(DHCPv6DestAddr));
			unicast.sin6_family = AF_INET6;
			unicast.sin6_port = remote_port;
			memcpy(&unicast.sin6_addr, ds.data, 16);
			if (client->refresh_type == DHCPV6_RENEW) {
				dest_addr = &unicast;
			}
		}

		data_string_forget(&ds, MDL);
	}

	/* Commence forming a renew packet. */
	memset(&ds, 0, sizeof(ds));
	if (!buffer_allocate(&ds.buffer, 4, MDL)) {
		log_error("Unable to allocate memory for packet.");
		return;
	}
	ds.data = ds.buffer->data;
	ds.len = 4;

	ds.buffer->data[0] = client->refresh_type;
	memcpy(ds.buffer->data + 1, client->dhcpv6_transaction_id, 3);

	/* Form an elapsed option. */
	/* Maximum value is 65535 1/100s coded as 0xffff. */
	if ((elapsed.tv_sec < 0) || (elapsed.tv_sec > 655) ||
	    ((elapsed.tv_sec == 655) && (elapsed.tv_usec > 350000))) {
		client->elapsed = 0xffff;
	} else {
		client->elapsed = elapsed.tv_sec * 100;
		client->elapsed += elapsed.tv_usec / 10000;
	}

	if (client->elapsed == 0)
		log_debug("XMT: Forming %s, 0 ms elapsed.",
			  dhcpv6_type_names[client->refresh_type]);
	else
		log_debug("XMT: Forming %s, %u0 ms elapsed.",
			  dhcpv6_type_names[client->refresh_type],
			  (unsigned)client->elapsed);

	client->elapsed = htons(client->elapsed);

	make_client6_options(client, &client->sent_options, lease,
			     client->refresh_type);

	/* Put in any options from the sent cache. */
	dhcpv6_universe.encapsulate(&ds, NULL, NULL, client, NULL,
				    client->sent_options, &global_scope,
				    &dhcpv6_universe);

	/* Append IA's */
	if (wanted_ia_na &&
	    dhc6_add_ia_na(client, &ds, lease,
			   client->refresh_type) != ISC_R_SUCCESS) {
		data_string_forget(&ds, MDL);
		return;
	}
	if (wanted_ia_pd &&
	    dhc6_add_ia_pd(client, &ds, lease,
			   client->refresh_type) != ISC_R_SUCCESS) {
		data_string_forget(&ds, MDL);
		return;
	}

	log_info("XMT: %s on %s, interval %ld0ms.",
		 dhcpv6_type_names[client->refresh_type],
		 client->name ? client->name : client->interface->name,
		 (long int)client->RT);

	send_ret = send_packet6(client->interface, ds.data, ds.len, dest_addr);

	if (send_ret != ds.len) {
		log_error("dhc6: send_packet6() sent %d of %d bytes",
			  send_ret, ds.len);
	}

	data_string_forget(&ds, MDL);

	/* Wait RT */
	tv.tv_sec = cur_tv.tv_sec + client->RT / 100;
	tv.tv_usec = cur_tv.tv_usec + (client->RT % 100) * 10000;
	if (tv.tv_usec >= 1000000) {
		tv.tv_sec += 1;
		tv.tv_usec -= 1000000;
	}
	add_timeout(&tv, do_refresh6, client, NULL, NULL);

	dhc6_retrans_advance(client);
}

/* start_rebind6() gets us all set up to go and rebind a lease.  Note that
 * client->next_MRD must be set before entering this function.  In this case,
 * MRD must be set to the maximum time any address in the packet will
 * expire.
 */
void
start_rebind6(void *input)
{
	struct client_state *client;

	client = (struct client_state *)input;

	log_info("PRC: Rebinding lease on %s.",
		 client->name ? client->name : client->interface->name);
	client->state = S_REBINDING;

	client->v6_handler = reply_handler;

	/* Times per RFC3315 section 18.1.4. */
	client->IRT = REB_TIMEOUT * 100;
	client->MRT = REB_MAX_RT * 100;
	client->MRC = 0;
	/* MRD is special in rebind - it's determined by the timer
	 * state.
	 */
	client->MRD = client->next_MRD - cur_time;

	dhc6_retrans_init(client);

	client->refresh_type = DHCPV6_REBIND;
	do_refresh6(client);
}

/* do_depref() runs through a given lease's addresses, for each that has
 * not yet been depreffed, shells out to the dhclient-script to inform it
 * of the status change.  The dhclient-script should then do...something...
 * to encourage applications to move off the address and onto one of the
 * remaining 'preferred' addresses.
 */
void
do_depref(void *input)
{
	struct client_state *client;
	struct dhc6_lease *lease;
	struct dhc6_ia *ia;
	struct dhc6_addr *addr;

	client = (struct client_state *)input;

	lease = client->active_lease;
	if (lease == NULL)
		return;

	for (ia = lease->bindings ; ia != NULL ; ia = ia->next) {
		for (addr = ia->addrs ; addr != NULL ; addr = addr->next) {
			if (addr->flags & DHC6_ADDR_DEPREFFED)
				continue;

			if (addr->starts + addr->preferred_life <= cur_time) {
				script_init(client, "DEPREF6", NULL);
				dhc6_marshall_values("cur_", client, lease,
						     ia, addr);
				script_write_requested6(client);
				script_go(client);

				addr->flags |= DHC6_ADDR_DEPREFFED;

				if (ia->ia_type != D6O_IA_PD)
				    log_info("PRC: Address %s depreferred.",
					     piaddr(addr->address));
				else
				    log_info("PRC: Prefix %s/%u depreferred.",
					     piaddr(addr->address),
					     (unsigned) addr->plen);

#if defined (NSUPDATE)
				/* Remove DDNS bindings at depref time. */
				if ((ia->ia_type == D6O_IA_NA) &&
				    client->config->do_forward_update)
					client_dns_remove(client, 
							  &addr->address);
#endif
			}
		}
	}

	dhc6_check_times(client);
}

/* do_expire() searches through all the addresses on a given lease, and
 * expires/removes any addresses that are no longer valid.
 */
void
do_expire(void *input)
{
	struct client_state *client;
	struct dhc6_lease *lease;
	struct dhc6_ia *ia;
	struct dhc6_addr *addr;
	int has_addrs = ISC_FALSE;

	client = (struct client_state *)input;

	lease = client->active_lease;
	if (lease == NULL)
		return;

	for (ia = lease->bindings ; ia != NULL ; ia = ia->next) {
		for (addr = ia->addrs ; addr != NULL ; addr = addr->next) {
			if (addr->flags & DHC6_ADDR_EXPIRED)
				continue;

			if (addr->starts + addr->max_life <= cur_time) {
				script_init(client, "EXPIRE6", NULL);
				dhc6_marshall_values("old_", client, lease,
						     ia, addr);
				script_write_requested6(client);
				script_go(client);

				addr->flags |= DHC6_ADDR_EXPIRED;

				if (ia->ia_type != D6O_IA_PD)
				    log_info("PRC: Address %s expired.",
					     piaddr(addr->address));
				else
				    log_info("PRC: Prefix %s/%u expired.",
					     piaddr(addr->address),
					     (unsigned) addr->plen);

#if defined (NSUPDATE)
				/* We remove DNS records at depref time, but
				 * it is possible that we might get here
				 * without depreffing.
				 */
				if ((ia->ia_type == D6O_IA_NA) &&
				    client->config->do_forward_update &&
				    !(addr->flags & DHC6_ADDR_DEPREFFED))
					client_dns_remove(client,
							  &addr->address);
#endif

				continue;
			}

			has_addrs = ISC_TRUE;
		}
	}

	/* Clean up empty leases. */
	if (has_addrs == ISC_FALSE) {
		log_info("PRC: Bound lease is devoid of active addresses."
			 "  Re-initializing.");

		dhc6_lease_destroy(&lease, MDL);
		client->active_lease = NULL;

		start_init6(client);
		return;
	}

	/* Schedule the next run through. */
	dhc6_check_times(client);
}

/*
 * Run client script to unconfigure interface.
 * Called with reason STOP6 when dhclient -x is run, or with reason
 * RELEASE6 when server has replied to a Release message.
 * Stateless is a special case.
 */
void
unconfigure6(struct client_state *client, const char *reason)
{
	struct dhc6_ia *ia;
	struct dhc6_addr *addr;

	if (stateless) {
		script_init(client, reason, NULL);
		if (client->active_lease != NULL)
			script_write_params6(client, "old_",
					     client->active_lease->options);
		script_write_requested6(client);
		script_go(client);
		return;
	}

	if (client->active_lease == NULL)
		return;

	for (ia = client->active_lease->bindings ; ia != NULL ; ia = ia->next) {
		if (ia->ia_type == D6O_IA_TA)
			continue;

		for (addr = ia->addrs ; addr != NULL ; addr = addr->next) {
			script_init(client, reason, NULL);
			dhc6_marshall_values("old_", client,
					     client->active_lease, ia, addr);
			script_write_requested6(client);
			script_go(client);

#if defined (NSUPDATE)
			if ((ia->ia_type == D6O_IA_NA) &&
			    client->config->do_forward_update)
				client_dns_remove(client, &addr->address);
#endif
		}
	}
}

static void
refresh_info_request6(void *input)
{
	struct client_state *client;

	client = (struct client_state *)input;
	start_info_request6(client);
}

/* Timeout for Information-Request (using the IRT option).
 */
static void
dhc6_check_irt(struct client_state *client)
{
	struct option **req;
	struct option_cache *oc;
	TIME expire = MAX_TIME;
	struct timeval tv;
	int i;
	isc_boolean_t found = ISC_FALSE;

	cancel_timeout(refresh_info_request6, client);

	req = client->config->requested_options;
	for (i = 0; req[i] != NULL; i++) {
		if (req[i] == irt_option) {
			found = ISC_TRUE;
			break;
		}
	}
	/* Simply return gives a endless loop waiting for nothing. */
	if (!found)
		exit(0);

	oc = lookup_option(&dhcpv6_universe, client->active_lease->options,
			   D6O_INFORMATION_REFRESH_TIME);
	if (oc != NULL) {
		struct data_string irt;

		memset(&irt, 0, sizeof(irt));
		if (!evaluate_option_cache(&irt, NULL, NULL, client,
					   client->active_lease->options,
					   NULL, &global_scope, oc, MDL) ||
		    (irt.len < 4)) {
			log_error("Can't evaluate IRT.");
		} else {
			expire = getULong(irt.data);
			if (expire < IRT_MINIMUM)
				expire = IRT_MINIMUM;
			if (expire == 0xffffffff)
				expire = MAX_TIME;
		}
		data_string_forget(&irt, MDL);
	} else
		expire = IRT_DEFAULT;

	if (expire != MAX_TIME) {
		log_debug("PRC: Refresh event scheduled in %u seconds.",
			  (unsigned) expire);
		tv.tv_sec = cur_time + expire;
		tv.tv_usec = 0;
		add_timeout(&tv, refresh_info_request6, client, NULL, NULL);
	}
}

/* We got a Reply. Give dhclient-script a tickle to inform it about
 * the new values, and then lay in wait for the next event.
 */
static void
start_informed(struct client_state *client)
{
	client->v6_handler = informed_handler;

	log_debug("PRC: Done.");

	client->state = S_BOUND;

	script_init(client, "RENEW6", NULL);
	if (client->old_lease != NULL)
		script_write_params6(client, "old_",
				     client->old_lease->options);
	script_write_params6(client, "new_", client->active_lease->options);
	script_write_requested6(client);
	script_go(client);

	go_daemon();

	if (client->old_lease != NULL) {
		dhc6_lease_destroy(&client->old_lease, MDL);
		client->old_lease = NULL;
	}

	/* Schedule events. */
	dhc6_check_irt(client);
}

/* While informed, ignore packets.
 */
void
informed_handler(struct packet *packet, struct client_state *client)
{
	log_debug("RCV: Input packets are ignored once bound.");
}

/* make_client6_options() fetches option caches relevant to the client's
 * scope and places them into the sent_options cache.  This cache is later
 * used to populate DHCPv6 output packets with options.
 */
static void
make_client6_options(struct client_state *client, struct option_state **op,
		     struct dhc6_lease *lease, u_int8_t message)
{
	struct option_cache *oc;
	struct option **req;
	struct buffer *buffer;
	int buflen, i, oro_len;

	if ((op == NULL) || (client == NULL))
		return;

	if (*op)
		option_state_dereference(op, MDL);

	/* Create a cache to carry options to transmission. */
	option_state_allocate(op, MDL);

	/* Create and store an 'elapsed time' option in the cache. */
	oc = NULL;
	if (option_cache_allocate(&oc, MDL)) {
		const unsigned char *cdata;

		cdata = (unsigned char *)&client->elapsed;

		if (make_const_data(&oc->expression, cdata, 2, 0, 0, MDL)) {
			option_reference(&oc->option, elapsed_option, MDL);
			save_option(&dhcpv6_universe, *op, oc);
		}

		option_cache_dereference(&oc, MDL);
	}

	/* Bring in any configured options to send. */
	if (client->config->on_transmission)
		execute_statements_in_scope(NULL, NULL, NULL, client,
					    lease ? lease->options : NULL,
					    *op, &global_scope,
					    client->config->on_transmission,
					    NULL, NULL);

	/* Rapid-commit is only for SOLICITs. */
	if (message != DHCPV6_SOLICIT)
		delete_option(&dhcpv6_universe, *op, D6O_RAPID_COMMIT);

	/* See if the user configured a DUID in a relevant scope.  If not,
	 * introduce our default manufactured id.
	 */
	if ((oc = lookup_option(&dhcpv6_universe, *op,
				D6O_CLIENTID)) == NULL) {
		if (!option_cache(&oc, &default_duid, NULL, clientid_option,
				  MDL))
			log_fatal("Failure assembling a DUID.");

		save_option(&dhcpv6_universe, *op, oc);
		option_cache_dereference(&oc, MDL);
	}

	/* In cases where we're responding to a single server, put the
	 * server's id in the response.
	 *
	 * Note that lease is NULL for SOLICIT or INFO request messages,
	 * and otherwise MUST be present.
	 */
	if (lease == NULL) {
		if ((message != DHCPV6_SOLICIT) &&
		    (message != DHCPV6_INFORMATION_REQUEST))
			log_fatal("Impossible condition at %s:%d.", MDL);
	} else if ((message != DHCPV6_REBIND) &&
		   (message != DHCPV6_CONFIRM)) {
		oc = lookup_option(&dhcpv6_universe, lease->options,
				   D6O_SERVERID);
		if (oc != NULL)
			save_option(&dhcpv6_universe, *op, oc);
	}

	/* 'send dhcp6.oro foo;' syntax we used in 4.0.0a1/a2 has been
	 * deprecated by adjustments to the 'request' syntax also used for
	 * DHCPv4.
	 */
	if (lookup_option(&dhcpv6_universe, *op, D6O_ORO) != NULL)
		log_error("'send dhcp6.oro' syntax is deprecated, please "
			  "use the 'request' syntax (\"man dhclient.conf\").");

	/* Construct and store an ORO (Option Request Option).  It is a
	 * fatal error to fail to send an ORO (of at least zero length).
	 *
	 * Discussion:  RFC3315 appears to be inconsistent in its statements
	 * of whether or not the ORO is mandatory.  In section 18.1.1
	 * ("Creation and Transmission of Request Messages"):
	 *
	 *    The client MUST include an Option Request option (see section
	 *    22.7) to indicate the options the client is interested in
	 *    receiving.  The client MAY include options with data values as
	 *    hints to the server about parameter values the client would like
	 *    to have returned.
	 *
	 * This MUST is missing from the creation/transmission of other
	 * messages (such as Renew and Rebind), and the section 22.7 ("Option
	 * Request Option" format and definition):
	 *
	 *    A client MAY include an Option Request option in a Solicit,
	 *    Request, Renew, Rebind, Confirm or Information-request message to
	 *    inform the server about options the client wants the server to
	 *    send to the client.  A server MAY include an Option Request
	 *    option in a Reconfigure option to indicate which options the
	 *    client should request from the server.
	 *
	 * seems to relax the requirement from MUST to MAY (and still other
	 * language in RFC3315 supports this).
	 *
	 * In lieu of a clarification of RFC3315, we will conform with the
	 * MUST.  Instead of an absent ORO, we will if there are no options
	 * to request supply an empty ORO.  Theoretically, an absent ORO is
	 * difficult to interpret (does the client want all options or no
	 * options?).  A zero-length ORO is intuitively clear: requesting
	 * nothing.
	 */
	buffer = NULL;
	oro_len = 0;
	buflen = 32;
	if (!buffer_allocate(&buffer, buflen, MDL))
		log_fatal("Out of memory constructing DHCPv6 ORO.");
	req = client->config->requested_options;
	if (req != NULL) {
		for (i = 0 ; req[i] != NULL ; i++) {
			if (buflen == oro_len) {
				struct buffer *tmpbuf = NULL;

				buflen += 32;

				/* Shell game. */
				buffer_reference(&tmpbuf, buffer, MDL);
				buffer_dereference(&buffer, MDL);

				if (!buffer_allocate(&buffer, buflen, MDL))
					log_fatal("Out of memory resizing "
						  "DHCPv6 ORO buffer.");

				memcpy(buffer->data, tmpbuf->data, oro_len);

				buffer_dereference(&tmpbuf, MDL);
			}

			if (req[i]->universe == &dhcpv6_universe) {
				/* Append the code to the ORO. */
				putUShort(buffer->data + oro_len,
					  req[i]->code);
				oro_len += 2;
			}
		}
	}

	oc = NULL;
	if (make_const_option_cache(&oc, &buffer, NULL, oro_len,
				    oro_option, MDL)) {
		save_option(&dhcpv6_universe, *op, oc);
	} else {
		log_fatal("Unable to create ORO option cache.");
	}

	/*
	 * Note: make_const_option_cache() consumes the buffer, we do not
	 * need to dereference it (XXX).
	 */
	option_cache_dereference(&oc, MDL);
}

/* A clone of the DHCPv4 script_write_params() minus the DHCPv4-specific
 * filename, server-name, etc specifics.
 *
 * Simply, store all values present in all universes of the option state
 * (probably derived from a DHCPv6 packet) into environment variables
 * named after the option names (and universe names) but with the 'prefix'
 * prepended.
 *
 * Later, dhclient-script may compare for example "new_time_servers" and
 * "old_time_servers" for differences, and only upon detecting a change
 * bother to rewrite ntp.conf and restart it.  Or something along those
 * generic lines.
 */
static void
script_write_params6(struct client_state *client, const char *prefix,
		     struct option_state *options)
{
	struct envadd_state es;
	int i;

	if (options == NULL)
		return;

	es.client = client;
	es.prefix = prefix;

	for (i = 0 ; i < options->universe_count ; i++) {
		option_space_foreach(NULL, NULL, client, NULL, options,
				     &global_scope, universes[i], &es,
				     client_option_envadd);
	}
}

/*
 * A clone of the DHCPv4 routine.
 * Write out the environment variables for the objects that the
 * client requested.  If the object was requested the variable will be:
 * requested_<option_name>=1
 * If it wasn't requested there won't be a variable.
 */
static void script_write_requested6(client)
	struct client_state *client;
{
	int i;
	struct option **req;
	char name[256];
	req = client->config->requested_options;

	if (req == NULL)
		return;

	for (i = 0 ; req[i] != NULL ; i++) {
		if ((req[i]->universe == &dhcpv6_universe) &&
		    dhcp_option_ev_name (name, sizeof(name), req[i])) {
			client_envadd(client, "requested_", name, "%d", 1);
		}
	}
}

/*
 * Check if there is something not fully defined in the active lease.
 */
static isc_boolean_t
active_prefix(struct client_state *client)
{
	struct dhc6_lease *lease;
	struct dhc6_ia *ia;
	struct dhc6_addr *pref;
	char zeros[16];

	lease = client->active_lease;
	if (lease == NULL)
		return ISC_FALSE;
	memset(zeros, 0, 16);
	for (ia = lease->bindings; ia != NULL; ia = ia->next) {
		if (ia->ia_type != D6O_IA_PD)
			continue;
		for (pref = ia->addrs; pref != NULL; pref = pref->next) {
			if (pref->plen == 0)
				return ISC_FALSE;
			if (pref->address.len != 16)
				return ISC_FALSE;
			if (memcmp(pref->address.iabuf, zeros, 16) == 0)
				return ISC_FALSE;
		}
	}
	return ISC_TRUE;
}
#endif /* DHCPv6 */
