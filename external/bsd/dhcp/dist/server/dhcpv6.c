/*	$NetBSD: dhcpv6.c,v 1.5 2014/07/12 12:09:38 spz Exp $	*/
/*
 * Copyright (C) 2006-2013 by Internet Systems Consortium, Inc. ("ISC")
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

#include <sys/cdefs.h>
__RCSID("$NetBSD: dhcpv6.c,v 1.5 2014/07/12 12:09:38 spz Exp $");

/*! \file server/dhcpv6.c */

#include "dhcpd.h"

#ifdef DHCPv6

/*
 * We use print_hex_1() to output DUID values. We could actually output 
 * the DUID with more information... MAC address if using type 1 or 3, 
 * and so on. However, RFC 3315 contains Grave Warnings against actually 
 * attempting to understand a DUID.
 */

/* 
 * TODO: gettext() or other method of localization for the messages
 *       for status codes (and probably for log formats eventually)
 * TODO: refactoring (simplify, simplify, simplify)
 * TODO: support multiple shared_networks on each interface (this 
 *       will allow the server to issue multiple IPv6 addresses to 
 *       a single interface)
 */

/*
 * DHCPv6 Reply workflow assist.  A Reply packet is built by various
 * different functions; this gives us one location where we keep state
 * regarding a reply.
 */
struct reply_state {
	/* root level persistent state */
	struct shared_network *shared;
	struct host_decl *host;
	struct subnet *subnet; /* Used to match fixed-addrs to subnet scopes. */
	struct option_state *opt_state;
	struct packet *packet;
	struct data_string client_id;

	/* IA level persistent state */
	unsigned ia_count;
	unsigned pd_count;
	unsigned client_resources;
	isc_boolean_t resources_included;
	isc_boolean_t static_lease;
	unsigned static_prefixes;
	struct ia_xx *ia;
	struct ia_xx *old_ia;
	struct option_state *reply_ia;
	struct data_string fixed;
	struct iaddrcidrnet fixed_pref; /* static prefix for logging */

	/* IAADDR/PREFIX level persistent state */
	struct iasubopt *lease;

	/*
	 * "t1", "t2", preferred, and valid lifetimes records for calculating
	 * t1 and t2 (min/max).
	 */
	u_int32_t renew, rebind, prefer, valid;

	/* Client-requested valid and preferred lifetimes. */
	u_int32_t client_valid, client_prefer;

	/* Chosen values to transmit for valid and preferred lifetimes. */
	u_int32_t send_valid, send_prefer;

	/* Preferred prefix length (-1 is any). */
	int preflen;

	/* Index into the data field that has been consumed. */
	unsigned cursor;

	/* Space for the on commit statements for a fixed host */
	struct on_star on_star;

	union reply_buffer {
		unsigned char data[65536];
		struct dhcpv6_packet reply;
	} buf;
};

/* 
 * Prototypes local to this file.
 */
static int get_encapsulated_IA_state(struct option_state **enc_opt_state,
				     struct data_string *enc_opt_data,
				     struct packet *packet,
				     struct option_cache *oc,
				     int offset);
static void build_dhcpv6_reply(struct data_string *, struct packet *);
static isc_result_t shared_network_from_packet6(struct shared_network **shared,
						struct packet *packet);
static void seek_shared_host(struct host_decl **hp,
			     struct shared_network *shared);
static isc_boolean_t fixed_matches_shared(struct host_decl *host,
					  struct shared_network *shared);
static isc_result_t reply_process_ia_na(struct reply_state *reply,
					struct option_cache *ia);
static isc_result_t reply_process_ia_ta(struct reply_state *reply,
					struct option_cache *ia);
static isc_result_t reply_process_addr(struct reply_state *reply,
				       struct option_cache *addr);
static isc_boolean_t address_is_owned(struct reply_state *reply,
				      struct iaddr *addr);
static isc_boolean_t temporary_is_available(struct reply_state *reply,
					    struct iaddr *addr);
static isc_result_t find_client_temporaries(struct reply_state *reply);
static isc_result_t reply_process_try_addr(struct reply_state *reply,
					   struct iaddr *addr);
static isc_result_t find_client_address(struct reply_state *reply);
static isc_result_t reply_process_is_addressed(struct reply_state *reply,
					       struct binding_scope **scope,
					       struct group *group);
static isc_result_t reply_process_send_addr(struct reply_state *reply,
					    struct iaddr *addr);
static struct iasubopt *lease_compare(struct iasubopt *alpha,
				      struct iasubopt *beta);
static isc_result_t reply_process_ia_pd(struct reply_state *reply,
					struct option_cache *ia_pd);
static isc_result_t reply_process_prefix(struct reply_state *reply,
					 struct option_cache *pref);
static isc_boolean_t prefix_is_owned(struct reply_state *reply,
				     struct iaddrcidrnet *pref);
static isc_result_t find_client_prefix(struct reply_state *reply);
static isc_result_t reply_process_try_prefix(struct reply_state *reply,
					     struct iaddrcidrnet *pref);
static isc_result_t reply_process_is_prefixed(struct reply_state *reply,
					      struct binding_scope **scope,
					      struct group *group);
static isc_result_t reply_process_send_prefix(struct reply_state *reply,
					      struct iaddrcidrnet *pref);
static struct iasubopt *prefix_compare(struct reply_state *reply,
				       struct iasubopt *alpha,
				       struct iasubopt *beta);
static int find_hosts_by_duid_chaddr(struct host_decl **host,
				     const struct data_string *client_id);
/*
 * This function returns the time since DUID time start for the
 * given time_t value.
 */
static u_int32_t
duid_time(time_t when) {
	/*
	 * This time is modulo 2^32.
	 */
	while ((when - DUID_TIME_EPOCH) > 4294967295u) {
		/* use 2^31 to avoid spurious compiler warnings */
		when -= 2147483648u;
		when -= 2147483648u;
	}

	return when - DUID_TIME_EPOCH;
}


/* 
 * Server DUID.
 *
 * This must remain the same for the lifetime of this server, because
 * clients return the server DUID that we sent them in Request packets.
 *
 * We pick the server DUID like this:
 *
 * 1. Check dhcpd.conf - any value the administrator has configured 
 *    overrides any possible values.
 * 2. Check the leases.txt - we want to use the previous value if 
 *    possible.
 * 3. Check if dhcpd.conf specifies a type of server DUID to use,
 *    and generate that type.
 * 4. Generate a type 1 (time + hardware address) DUID.
 */
static struct data_string server_duid;

/*
 * Check if the server_duid has been set.
 */
isc_boolean_t
server_duid_isset(void) {
	return (server_duid.data != NULL);
}

/*
 * Return the server_duid.
 */
void
copy_server_duid(struct data_string *ds, const char *file, int line) {
	data_string_copy(ds, &server_duid, file, line);
}

/*
 * Set the server DUID to a specified value. This is used when
 * the server DUID is stored in persistent memory (basically the
 * leases.txt file).
 */
void
set_server_duid(struct data_string *new_duid) {
	/* INSIST(new_duid != NULL); */
	/* INSIST(new_duid->data != NULL); */

	if (server_duid_isset()) {
		data_string_forget(&server_duid, MDL);
	}
	data_string_copy(&server_duid, new_duid, MDL);
}


/*
 * Set the server DUID based on the D6O_SERVERID option. This handles
 * the case where the administrator explicitly put it in the dhcpd.conf 
 * file.
 */
isc_result_t
set_server_duid_from_option(void) {
	struct option_state *opt_state;
	struct option_cache *oc;
	struct data_string option_duid;
	isc_result_t ret_val;

	opt_state = NULL;
	if (!option_state_allocate(&opt_state, MDL)) {
		log_fatal("No memory for server DUID.");
	}

	execute_statements_in_scope(NULL, NULL, NULL, NULL, NULL,
				    opt_state, &global_scope, root_group,
				    NULL, NULL);

	oc = lookup_option(&dhcpv6_universe, opt_state, D6O_SERVERID);
	if (oc == NULL) {
		ret_val = ISC_R_NOTFOUND;
	} else {
		memset(&option_duid, 0, sizeof(option_duid));
		if (!evaluate_option_cache(&option_duid, NULL, NULL, NULL,
					   opt_state, NULL, &global_scope,
					   oc, MDL)) {
			ret_val = ISC_R_UNEXPECTED;
		} else {
			set_server_duid(&option_duid);
			data_string_forget(&option_duid, MDL);
			ret_val = ISC_R_SUCCESS;
		}
	}

	option_state_dereference(&opt_state, MDL);

	return ret_val;
}

/*
 * DUID layout, as defined in RFC 3315, section 9.
 * 
 * We support type 1 (hardware address plus time) and type 3 (hardware
 * address).
 *
 * We can support type 2 for specific vendors in the future, if they 
 * publish the specification. And of course there may be additional
 * types later.
 */
static int server_duid_type = DUID_LLT;

/* 
 * Set the DUID type.
 */
void
set_server_duid_type(int type) {
	server_duid_type = type;
}

/*
 * Generate a new server DUID. This is done if there was no DUID in 
 * the leases.txt or in the dhcpd.conf file.
 */
isc_result_t
generate_new_server_duid(void) {
	struct interface_info *p;
	u_int32_t time_val;
	struct data_string generated_duid;

	/*
	 * Verify we have a type that we support.
	 */
	if ((server_duid_type != DUID_LL) && (server_duid_type != DUID_LLT)) {
		log_error("Invalid DUID type %d specified, "
			  "only LL and LLT types supported", server_duid_type);
		return DHCP_R_INVALIDARG;
	}

	/*
	 * Find an interface with a hardware address.
	 * Any will do. :)
	 */
	for (p = interfaces; p != NULL; p = p->next) {
		if (p->hw_address.hlen > 0) {
			break;
		}
	}
	if (p == NULL) {
		return ISC_R_UNEXPECTED;
	}

	/*
	 * Build our DUID.
	 */
	memset(&generated_duid, 0, sizeof(generated_duid));
	if (server_duid_type == DUID_LLT) {
		time_val = duid_time(time(NULL));
		generated_duid.len = 8 + p->hw_address.hlen - 1;
		if (!buffer_allocate(&generated_duid.buffer,
				     generated_duid.len, MDL)) {
			log_fatal("No memory for server DUID.");
		}
		generated_duid.data = generated_duid.buffer->data;
		putUShort(generated_duid.buffer->data, DUID_LLT);
		putUShort(generated_duid.buffer->data + 2,
			  p->hw_address.hbuf[0]);
		putULong(generated_duid.buffer->data + 4, time_val);
		memcpy(generated_duid.buffer->data + 8,
		       p->hw_address.hbuf+1, p->hw_address.hlen-1);
	} else if (server_duid_type == DUID_LL) {
		generated_duid.len = 4 + p->hw_address.hlen - 1;
		if (!buffer_allocate(&generated_duid.buffer,
				     generated_duid.len, MDL)) {
			log_fatal("No memory for server DUID.");
		}
		generated_duid.data = generated_duid.buffer->data;
		putUShort(generated_duid.buffer->data, DUID_LL);
		putUShort(generated_duid.buffer->data + 2,
			  p->hw_address.hbuf[0]);
		memcpy(generated_duid.buffer->data + 4,
		       p->hw_address.hbuf+1, p->hw_address.hlen-1);
	} else {
		log_fatal("Unsupported server DUID type %d.", server_duid_type);
	}

	set_server_duid(&generated_duid);
	data_string_forget(&generated_duid, MDL);

	return ISC_R_SUCCESS;
}

/*
 * Get the client identifier from the packet.
 */
isc_result_t
get_client_id(struct packet *packet, struct data_string *client_id) {
	struct option_cache *oc;

	/*
	 * Verify our client_id structure is empty.
	 */
	if ((client_id->data != NULL) || (client_id->len != 0)) {
		return DHCP_R_INVALIDARG;
	}

	oc = lookup_option(&dhcpv6_universe, packet->options, D6O_CLIENTID);
	if (oc == NULL) {
		return ISC_R_NOTFOUND;
	}

	if (!evaluate_option_cache(client_id, packet, NULL, NULL,
				   packet->options, NULL,
				   &global_scope, oc, MDL)) {
		return ISC_R_FAILURE;
	}

	return ISC_R_SUCCESS;
}

/*
 * Message validation, defined in RFC 3315, sections 15.2, 15.5, 15.7:
 *
 *    Servers MUST discard any Solicit messages that do not include a
 *    Client Identifier option or that do include a Server Identifier
 *    option.
 */
static int
valid_client_msg(struct packet *packet, struct data_string *client_id) {
	int ret_val;
	struct option_cache *oc;
	struct data_string data;

	ret_val = 0;
	memset(client_id, 0, sizeof(*client_id));
	memset(&data, 0, sizeof(data));

	switch (get_client_id(packet, client_id)) {
		case ISC_R_SUCCESS:
			break;
		case ISC_R_NOTFOUND:
			log_debug("Discarding %s from %s; "
				  "client identifier missing",
				  dhcpv6_type_names[packet->dhcpv6_msg_type],
				  piaddr(packet->client_addr));
			goto exit;
		default:
			log_error("Error processing %s from %s; "
				  "unable to evaluate Client Identifier",
				  dhcpv6_type_names[packet->dhcpv6_msg_type],
				  piaddr(packet->client_addr));
			goto exit;
	}

	/*
	 * Required by RFC 3315, section 15.
	 */
	if (packet->unicast) {
		log_debug("Discarding %s from %s; packet sent unicast "
			  "(CLIENTID %s)",
			  dhcpv6_type_names[packet->dhcpv6_msg_type],
			  piaddr(packet->client_addr),
			  print_hex_1(client_id->len, client_id->data, 60));
		goto exit;
	}


	oc = lookup_option(&dhcpv6_universe, packet->options, D6O_SERVERID);
	if (oc != NULL) {
		if (evaluate_option_cache(&data, packet, NULL, NULL,
					  packet->options, NULL,
					  &global_scope, oc, MDL)) {
			log_debug("Discarding %s from %s; "
				  "server identifier found "
				  "(CLIENTID %s, SERVERID %s)",
				  dhcpv6_type_names[packet->dhcpv6_msg_type],
				  piaddr(packet->client_addr),
				  print_hex_1(client_id->len,
				  	      client_id->data, 60),
				  print_hex_2(data.len,
				  	      data.data, 60));
		} else {
			log_debug("Discarding %s from %s; "
				  "server identifier found "
				  "(CLIENTID %s)",
				  dhcpv6_type_names[packet->dhcpv6_msg_type],
				  print_hex_1(client_id->len,
				  	      client_id->data, 60),
				  piaddr(packet->client_addr));
		}
		goto exit;
	}

	/* looks good */
	ret_val = 1;

exit:
	if (data.len > 0) {
		data_string_forget(&data, MDL);
	}
	if (!ret_val) {
		if (client_id->len > 0) {
			data_string_forget(client_id, MDL);
		}
	}
	return ret_val;
}

/*
 * Response validation, defined in RFC 3315, sections 15.4, 15.6, 15.8, 
 * 15.9 (slightly different wording, but same meaning):
 *
 *   Servers MUST discard any received Request message that meet any of
 *   the following conditions:
 *
 *   -  the message does not include a Server Identifier option.
 *   -  the contents of the Server Identifier option do not match the
 *      server's DUID.
 *   -  the message does not include a Client Identifier option.
 */
static int
valid_client_resp(struct packet *packet,
		  struct data_string *client_id,
		  struct data_string *server_id)
{
	int ret_val;
	struct option_cache *oc;

	/* INSIST((duid.data != NULL) && (duid.len > 0)); */

	ret_val = 0;
	memset(client_id, 0, sizeof(*client_id));
	memset(server_id, 0, sizeof(*server_id));

	switch (get_client_id(packet, client_id)) {
		case ISC_R_SUCCESS:
			break;
		case ISC_R_NOTFOUND:
			log_debug("Discarding %s from %s; "
				  "client identifier missing",
				  dhcpv6_type_names[packet->dhcpv6_msg_type],
				  piaddr(packet->client_addr));
			goto exit;
		default:
			log_error("Error processing %s from %s; "
				  "unable to evaluate Client Identifier",
				  dhcpv6_type_names[packet->dhcpv6_msg_type],
				  piaddr(packet->client_addr));
			goto exit;
	}

	oc = lookup_option(&dhcpv6_universe, packet->options, D6O_SERVERID);
	if (oc == NULL) {
		log_debug("Discarding %s from %s: "
			  "server identifier missing (CLIENTID %s)",
			  dhcpv6_type_names[packet->dhcpv6_msg_type],
			  piaddr(packet->client_addr),
			  print_hex_1(client_id->len, client_id->data, 60));
		goto exit;
	}
	if (!evaluate_option_cache(server_id, packet, NULL, NULL,
				   packet->options, NULL,
				   &global_scope, oc, MDL)) {
		log_error("Error processing %s from %s; "
			  "unable to evaluate Server Identifier (CLIENTID %s)",
			  dhcpv6_type_names[packet->dhcpv6_msg_type],
			  piaddr(packet->client_addr),
			  print_hex_1(client_id->len, client_id->data, 60));
		goto exit;
	}
	if ((server_duid.len != server_id->len) ||
	    (memcmp(server_duid.data, server_id->data, server_duid.len) != 0)) {
		log_debug("Discarding %s from %s; "
			  "not our server identifier "
			  "(CLIENTID %s, SERVERID %s, server DUID %s)",
			  dhcpv6_type_names[packet->dhcpv6_msg_type],
			  piaddr(packet->client_addr),
			  print_hex_1(client_id->len, client_id->data, 60),
			  print_hex_2(server_id->len, server_id->data, 60),
			  print_hex_3(server_duid.len, server_duid.data, 60));
		goto exit;
	}

	/* looks good */
	ret_val = 1;

exit:
	if (!ret_val) {
		if (server_id->len > 0) {
			data_string_forget(server_id, MDL);
		}
		if (client_id->len > 0) {
			data_string_forget(client_id, MDL);
		}
	}
	return ret_val;
}

/*
 * Information request validation, defined in RFC 3315, section 15.12:
 *
 *   Servers MUST discard any received Information-request message that
 *   meets any of the following conditions:
 *
 *   -  The message includes a Server Identifier option and the DUID in
 *      the option does not match the server's DUID.
 *
 *   -  The message includes an IA option.
 */
static int
valid_client_info_req(struct packet *packet, struct data_string *server_id) {
	int ret_val;
	struct option_cache *oc;
	struct data_string client_id;
	char client_id_str[80];	/* print_hex_1() uses maximum 60 characters,
				   plus a few more for extra information */

	ret_val = 0;
	memset(server_id, 0, sizeof(*server_id));
	memset(&client_id, 0, sizeof(client_id));

	/*
	 * Make a string that we can print out to give more 
	 * information about the client if we need to.
	 *
	 * By RFC 3315, Section 18.1.5 clients SHOULD have a 
	 * client-id on an Information-request packet, but it 
	 * is not strictly necessary.
	 */
	if (get_client_id(packet, &client_id) == ISC_R_SUCCESS) {
		snprintf(client_id_str, sizeof(client_id_str), " (CLIENTID %s)",
			 print_hex_1(client_id.len, client_id.data, 60));
		data_string_forget(&client_id, MDL);
	} else {
		client_id_str[0] = '\0';
	}

	/*
	 * Required by RFC 3315, section 15.
	 */
	if (packet->unicast) {
		log_debug("Discarding %s from %s; packet sent unicast%s",
			  dhcpv6_type_names[packet->dhcpv6_msg_type],
			  piaddr(packet->client_addr), client_id_str);
		goto exit;
	}

	oc = lookup_option(&dhcpv6_universe, packet->options, D6O_IA_NA);
	if (oc != NULL) {
		log_debug("Discarding %s from %s; "
			  "IA_NA option present%s",
			  dhcpv6_type_names[packet->dhcpv6_msg_type],
			  piaddr(packet->client_addr), client_id_str);
		goto exit;
	}
	oc = lookup_option(&dhcpv6_universe, packet->options, D6O_IA_TA);
	if (oc != NULL) {
		log_debug("Discarding %s from %s; "
			  "IA_TA option present%s",
			  dhcpv6_type_names[packet->dhcpv6_msg_type],
			  piaddr(packet->client_addr), client_id_str);
		goto exit;
	}
	oc = lookup_option(&dhcpv6_universe, packet->options, D6O_IA_PD);
	if (oc != NULL) {
		log_debug("Discarding %s from %s; "
			  "IA_PD option present%s",
			  dhcpv6_type_names[packet->dhcpv6_msg_type],
			  piaddr(packet->client_addr), client_id_str);
		goto exit;
	}

	oc = lookup_option(&dhcpv6_universe, packet->options, D6O_SERVERID);
	if (oc != NULL) {
		if (!evaluate_option_cache(server_id, packet, NULL, NULL,
					   packet->options, NULL,
					   &global_scope, oc, MDL)) {
			log_error("Error processing %s from %s; "
				  "unable to evaluate Server Identifier%s",
				  dhcpv6_type_names[packet->dhcpv6_msg_type],
				  piaddr(packet->client_addr), client_id_str);
			goto exit;
		}
		if ((server_duid.len != server_id->len) ||
		    (memcmp(server_duid.data, server_id->data,
		    	    server_duid.len) != 0)) {
			log_debug("Discarding %s from %s; "
				  "not our server identifier "
				  "(SERVERID %s, server DUID %s)%s",
				  dhcpv6_type_names[packet->dhcpv6_msg_type],
				  piaddr(packet->client_addr),
				  print_hex_1(server_id->len,
				  	      server_id->data, 60),
				  print_hex_2(server_duid.len,
				  	      server_duid.data, 60),
				  client_id_str);
			goto exit;
		}
	}

	/* looks good */
	ret_val = 1;

exit:
	if (!ret_val) {
		if (server_id->len > 0) {
			data_string_forget(server_id, MDL);
		}
	}
	return ret_val;
}

/* 
 * Options that we want to send, in addition to what was requested
 * via the ORO.
 */
static const int required_opts[] = {
	D6O_CLIENTID,
	D6O_SERVERID,
	D6O_STATUS_CODE,
	D6O_PREFERENCE,
	0
};
static const int required_opts_NAA[] = {
	D6O_CLIENTID,
	D6O_SERVERID,
	D6O_STATUS_CODE,
	0
};
static const int required_opts_solicit[] = {
	D6O_CLIENTID,
	D6O_SERVERID,
	D6O_IA_NA,
	D6O_IA_TA,
	D6O_IA_PD,
	D6O_RAPID_COMMIT,
	D6O_STATUS_CODE,
	D6O_RECONF_ACCEPT,
	D6O_PREFERENCE,
	0
};
static const int required_opts_agent[] = {
	D6O_INTERFACE_ID,
	D6O_RELAY_MSG,
	0
};
static const int required_opts_IA[] = {
	D6O_IAADDR,
	D6O_STATUS_CODE,
	0
};
static const int required_opts_IA_PD[] = {
	D6O_IAPREFIX,
	D6O_STATUS_CODE,
	0
};
static const int required_opts_STATUS_CODE[] = {
	D6O_STATUS_CODE,
	0
};

/*
 * Extracts from packet contents an IA_* option, storing the IA structure
 * in its entirety in enc_opt_data, and storing any decoded DHCPv6 options
 * in enc_opt_state for later lookup and evaluation.  The 'offset' indicates
 * where in the IA_* the DHCPv6 options commence.
 */
static int
get_encapsulated_IA_state(struct option_state **enc_opt_state,
			  struct data_string *enc_opt_data,
			  struct packet *packet,
			  struct option_cache *oc,
			  int offset)
{
	/* 
	 * Get the raw data for the encapsulated options.
	 */
	memset(enc_opt_data, 0, sizeof(*enc_opt_data));
	if (!evaluate_option_cache(enc_opt_data, packet,
				   NULL, NULL, packet->options, NULL,
				   &global_scope, oc, MDL)) {
		log_error("get_encapsulated_IA_state: "
			  "error evaluating raw option.");
		return 0;
	}
	if (enc_opt_data->len < offset) {
		log_error("get_encapsulated_IA_state: raw option too small.");
		data_string_forget(enc_opt_data, MDL);
		return 0;
	}

	/*
	 * Now create the option state structure, and pass it to the 
	 * function that parses options.
	 */
	*enc_opt_state = NULL;
	if (!option_state_allocate(enc_opt_state, MDL)) {
		log_error("get_encapsulated_IA_state: no memory for options.");
		data_string_forget(enc_opt_data, MDL);
		return 0;
	}
	if (!parse_option_buffer(*enc_opt_state,
				 enc_opt_data->data + offset, 
				 enc_opt_data->len - offset,
				 &dhcpv6_universe)) {
		log_error("get_encapsulated_IA_state: error parsing options.");
		option_state_dereference(enc_opt_state, MDL);
		data_string_forget(enc_opt_data, MDL);
		return 0;
	}

	return 1;
}

static int
set_status_code(u_int16_t status_code, const char *status_message,
		struct option_state *opt_state)
{
	struct data_string d;
	int ret_val;

	memset(&d, 0, sizeof(d));
	d.len = sizeof(status_code) + strlen(status_message);
	if (!buffer_allocate(&d.buffer, d.len, MDL)) {
		log_fatal("set_status_code: no memory for status code.");
	}
	d.data = d.buffer->data;
	putUShort(d.buffer->data, status_code);
	memcpy(d.buffer->data + sizeof(status_code), 
	       status_message, d.len - sizeof(status_code));
	if (!save_option_buffer(&dhcpv6_universe, opt_state, 
				d.buffer, (unsigned char *)d.data, d.len, 
				D6O_STATUS_CODE, 0)) {
		log_error("set_status_code: error saving status code.");
		ret_val = 0;
	} else {
		ret_val = 1;
	}
	data_string_forget(&d, MDL);
	return ret_val;
}

/*
 * We have a set of operations we do to set up the reply packet, which
 * is the same for many message types.
 */
static int
start_reply(struct packet *packet,
	    const struct data_string *client_id, 
	    const struct data_string *server_id,
	    struct option_state **opt_state,
	    struct dhcpv6_packet *reply)
{
	struct option_cache *oc;
	const unsigned char *server_id_data;
	int server_id_len;

	/*
	 * Build our option state for reply.
	 */
	*opt_state = NULL;
	if (!option_state_allocate(opt_state, MDL)) {
		log_error("start_reply: no memory for option_state.");
		return 0;
	}
	execute_statements_in_scope(NULL, packet, NULL, NULL,
				    packet->options, *opt_state,
				    &global_scope, root_group, NULL, NULL);

	/*
	 * A small bit of special handling for Solicit messages.
	 *
	 * We could move the logic into a flag, but for now just check
	 * explicitly.
	 */
	if (packet->dhcpv6_msg_type == DHCPV6_SOLICIT) {
		reply->msg_type = DHCPV6_ADVERTISE;

		/*
		 * If:
		 * - this message type supports rapid commit (Solicit), and
		 * - the server is configured to supply a rapid commit, and
		 * - the client requests a rapid commit,
		 * Then we add a rapid commit option, and send Reply (instead
		 * of an Advertise).
		 */
		oc = lookup_option(&dhcpv6_universe,
				   *opt_state, D6O_RAPID_COMMIT);
		if (oc != NULL) {
			oc = lookup_option(&dhcpv6_universe,
					   packet->options, D6O_RAPID_COMMIT);
			if (oc != NULL) {
				/* Rapid-commit in action. */
				reply->msg_type = DHCPV6_REPLY;
			} else {
				/* Don't want a rapid-commit in advertise. */
				delete_option(&dhcpv6_universe,
					      *opt_state, D6O_RAPID_COMMIT);
			}
		}
	} else {
		reply->msg_type = DHCPV6_REPLY;
		/* Delete the rapid-commit from the sent options. */
		oc = lookup_option(&dhcpv6_universe,
				   *opt_state, D6O_RAPID_COMMIT);
		if (oc != NULL) {
			delete_option(&dhcpv6_universe,
				      *opt_state, D6O_RAPID_COMMIT);
		}
	}

	/* 
	 * Use the client's transaction identifier for the reply.
	 */
	memcpy(reply->transaction_id, packet->dhcpv6_transaction_id, 
	       sizeof(reply->transaction_id));

	/* 
	 * RFC 3315, section 18.2 says we need server identifier and
	 * client identifier.
	 *
	 * If the server ID is defined via the configuration file, then
	 * it will already be present in the option state at this point, 
	 * so we don't need to set it.
	 *
	 * If we have a server ID passed in from the caller, 
	 * use that, otherwise use the global DUID.
	 */
	oc = lookup_option(&dhcpv6_universe, *opt_state, D6O_SERVERID);
	if (oc == NULL) {
		if (server_id == NULL) {
			server_id_data = server_duid.data;
			server_id_len = server_duid.len;
		} else {
			server_id_data = server_id->data;
			server_id_len = server_id->len;
		}
		if (!save_option_buffer(&dhcpv6_universe, *opt_state, 
					NULL, (unsigned char *)server_id_data,
					server_id_len, D6O_SERVERID, 0)) {
				log_error("start_reply: "
					  "error saving server identifier.");
				return 0;
		}
	}

	if (client_id->buffer != NULL) {
		if (!save_option_buffer(&dhcpv6_universe, *opt_state, 
					client_id->buffer, 
					(unsigned char *)client_id->data, 
					client_id->len, 
					D6O_CLIENTID, 0)) {
			log_error("start_reply: error saving "
				  "client identifier.");
			return 0;
		}
	}

	/*
	 * If the client accepts reconfiguration, let it know that we
	 * will send them.
	 *
	 * Note: we don't actually do this yet, but DOCSIS requires we
	 *       claim to.
	 */
	oc = lookup_option(&dhcpv6_universe, packet->options,
			   D6O_RECONF_ACCEPT);
	if (oc != NULL) {
		if (!save_option_buffer(&dhcpv6_universe, *opt_state,
					NULL, (unsigned char *)"", 0, 
					D6O_RECONF_ACCEPT, 0)) {
			log_error("start_reply: "
				  "error saving RECONF_ACCEPT option.");
			option_state_dereference(opt_state, MDL);
			return 0;
		}
	}

	return 1;
}

/*
 * Try to get the IPv6 address the client asked for from the
 * pool.
 *
 * addr is the result (should be a pointer to NULL on entry)
 * pool is the pool to search in
 * requested_addr is the address the client wants
 */
static isc_result_t
try_client_v6_address(struct iasubopt **addr,
		      struct ipv6_pool *pool,
		      const struct data_string *requested_addr)
{
	struct in6_addr tmp_addr;
	isc_result_t result;

	if (requested_addr->len < sizeof(tmp_addr)) {
		return DHCP_R_INVALIDARG;
	}
	memcpy(&tmp_addr, requested_addr->data, sizeof(tmp_addr));
	if (IN6_IS_ADDR_UNSPECIFIED(&tmp_addr)) {
		return ISC_R_FAILURE;
	}

	/*
	 * The address is not covered by this (or possibly any) dynamic
	 * range.
	 */
	if (!ipv6_in_pool(&tmp_addr, pool)) {
		return ISC_R_ADDRNOTAVAIL;
	}

	if (lease6_exists(pool, &tmp_addr)) {
		return ISC_R_ADDRINUSE;
	}

	result = iasubopt_allocate(addr, MDL);
	if (result != ISC_R_SUCCESS) {
		return result;
	}
	(*addr)->addr = tmp_addr;
	(*addr)->plen = 0;

	/* Default is soft binding for 2 minutes. */
	result = add_lease6(pool, *addr, cur_time + 120);
	if (result != ISC_R_SUCCESS) {
		iasubopt_dereference(addr, MDL);
	}
	return result;
}


/*!
 *
 * \brief  Get an IPv6 address for the client.
 *
 * Attempt to find a usable address for the client.  We walk through
 * the ponds checking for permit and deny then through the pools
 * seeing if they have an available address.
 *
 * \param reply = the state structure for the current work on this request
 *                if we create a lease we return it using reply->lease
 *
 * \return
 * ISC_R_SUCCESS = we were able to find an address and are returning a
 *                 pointer to the lease
 * ISC_R_NORESOURCES = there don't appear to be any free addresses.  This
 *                     is probabalistic.  We don't exhaustively try the
 *                     address range, instead we hash the duid and if
 *                     the address derived from the hash is in use we
 *                     hash the address.  After a number of failures we
 *                     conclude the pool is basically full.
 */
static isc_result_t 
pick_v6_address(struct reply_state *reply)
{
	struct ipv6_pool *p = NULL;
	struct ipv6_pond *pond;
	int i;
	int start_pool;
	unsigned int attempts;
	char tmp_buf[INET6_ADDRSTRLEN];
	struct iasubopt **addr = &reply->lease;

	/*
	 * Do a quick walk through of the ponds and pools
	 * to see if we have any NA address pools
	 */
	for (pond = reply->shared->ipv6_pond; pond != NULL; pond = pond->next) {
		if (pond->ipv6_pools == NULL)
			continue;

		for (i = 0; (p = pond->ipv6_pools[i]) != NULL; i++) {
			if (p->pool_type == D6O_IA_NA)
				break;
		}
		if (p != NULL)
			break;
	}

	/* If we get here and p is NULL we have no useful pools */
	if (p == NULL) {
		log_debug("Unable to pick client address: "
			  "no IPv6 pools on this shared network");
		return ISC_R_NORESOURCES;
	}
		
	/*
	 * We have at least one pool that could provide an address
	 * Now we walk through the ponds and pools again and check
	 * to see if the client is permitted and if an address is
	 * available
	 * 
	 * Within a given pond we start looking at the last pool we
	 * allocated from, unless it had a collision trying to allocate
	 * an address. This will tend to move us into less-filled pools.
	 */

	for (pond = reply->shared->ipv6_pond; pond != NULL; pond = pond->next) {
		if (((pond->prohibit_list != NULL) &&
		     (permitted(reply->packet, pond->prohibit_list))) ||
		    ((pond->permit_list != NULL) &&
		     (!permitted(reply->packet, pond->permit_list))))
			continue;

		start_pool = pond->last_ipv6_pool;
		i = start_pool;
		do {
			p = pond->ipv6_pools[i];
			if ((p->pool_type == D6O_IA_NA) &&
			    (create_lease6(p, addr, &attempts,
					   &reply->ia->iaid_duid,
					   cur_time + 120) == ISC_R_SUCCESS)) {
				/*
				 * Record the pool used (or next one if there 
				 * was a collision).
				 */
				if (attempts > 1) {
					i++;
					if (pond->ipv6_pools[i] == NULL) {
						i = 0;
					}
				}
				pond->last_ipv6_pool = i;

				log_debug("Picking pool address %s",
					  inet_ntop(AF_INET6, &((*addr)->addr),
						    tmp_buf, sizeof(tmp_buf)));
				return (ISC_R_SUCCESS);
			}

			i++;
			if (pond->ipv6_pools[i] == NULL) {
				i = 0;
			}
		} while (i != start_pool);
	}

	/*
	 * If we failed to pick an IPv6 address from any of the subnets.
	 * Presumably that means we have no addresses for the client.
	 */
	log_debug("Unable to pick client address: no addresses available");
	return ISC_R_NORESOURCES;
}

/*
 * Try to get the IPv6 prefix the client asked for from the
 * prefix pool.
 *
 * pref is the result (should be a pointer to NULL on entry)
 * pool is the prefix pool to search in
 * requested_pref is the address the client wants
 */
static isc_result_t
try_client_v6_prefix(struct iasubopt **pref,
		     struct ipv6_pool *pool,
		     const struct data_string *requested_pref)
{
	u_int8_t tmp_plen;
	struct in6_addr tmp_pref;
	struct iaddr ia;
	isc_result_t result;

	if (requested_pref->len < sizeof(tmp_plen) + sizeof(tmp_pref)) {
		return DHCP_R_INVALIDARG;
	}
	tmp_plen = (int) requested_pref->data[0];
	if ((tmp_plen < 3) || (tmp_plen > 128) ||
	    ((int)tmp_plen != pool->units)) {
		return ISC_R_FAILURE;
	}
	memcpy(&tmp_pref, requested_pref->data + 1, sizeof(tmp_pref));
	if (IN6_IS_ADDR_UNSPECIFIED(&tmp_pref)) {
		return ISC_R_FAILURE;
	}
	ia.len = 16;
	memcpy(&ia.iabuf, &tmp_pref, 16);
	if (!is_cidr_mask_valid(&ia, (int) tmp_plen)) {
		return ISC_R_FAILURE;
	}

	if (!ipv6_in_pool(&tmp_pref, pool)) {
		return ISC_R_ADDRNOTAVAIL;
	}

	if (prefix6_exists(pool, &tmp_pref, tmp_plen)) {
		return ISC_R_ADDRINUSE;
	}

	result = iasubopt_allocate(pref, MDL);
	if (result != ISC_R_SUCCESS) {
		return result;
	}
	(*pref)->addr = tmp_pref;
	(*pref)->plen = tmp_plen;

	/* Default is soft binding for 2 minutes. */
	result = add_lease6(pool, *pref, cur_time + 120);
	if (result != ISC_R_SUCCESS) {
		iasubopt_dereference(pref, MDL);
	}
	return result;
}

/*!
 *
 * \brief  Get an IPv6 prefix for the client.
 *
 * Attempt to find a usable prefix for the client.  We walk through
 * the ponds checking for permit and deny then through the pools
 * seeing if they have an available prefix.
 *
 * \param reply = the state structure for the current work on this request
 *                if we create a lease we return it using reply->lease
 *
 * \return
 * ISC_R_SUCCESS = we were able to find an prefix and are returning a
 *                 pointer to the lease
 * ISC_R_NORESOURCES = there don't appear to be any free addresses.  This
 *                     is probabalistic.  We don't exhaustively try the
 *                     address range, instead we hash the duid and if
 *                     the address derived from the hash is in use we
 *                     hash the address.  After a number of failures we
 *                     conclude the pool is basically full.
 */

static isc_result_t 
pick_v6_prefix(struct reply_state *reply)
{
	struct ipv6_pool *p = NULL;
	struct ipv6_pond *pond;
	int i;
	unsigned int attempts;
	char tmp_buf[INET6_ADDRSTRLEN];
	struct iasubopt **pref = &reply->lease;

	/*
	 * Do a quick walk through of the ponds and pools
	 * to see if we have any prefix pools
	 */
	for (pond = reply->shared->ipv6_pond; pond != NULL; pond = pond->next) {
		if (pond->ipv6_pools == NULL)
			continue;

		for (i = 0; (p = pond->ipv6_pools[i]) != NULL; i++) {
			if (p->pool_type == D6O_IA_PD)
				break;
		}
		if (p != NULL)
			break;
	}

	/* If we get here and p is NULL we have no useful pools */
	if (p == NULL) {
		log_debug("Unable to pick client prefix: "
			  "no IPv6 pools on this shared network");
		return ISC_R_NORESOURCES;
	}

	/*
	 * We have at least one pool that could provide a prefix
	 * Now we walk through the ponds and pools again and check
	 * to see if the client is permitted and if an prefix is
	 * available
	 * 
	 */

	for (pond = reply->shared->ipv6_pond; pond != NULL; pond = pond->next) {
		if (((pond->prohibit_list != NULL) &&
		     (permitted(reply->packet, pond->prohibit_list))) ||
		    ((pond->permit_list != NULL) &&
		     (!permitted(reply->packet, pond->permit_list))))
			continue;

		for (i = 0; (p = pond->ipv6_pools[i]) != NULL; i++) {
			if (p->pool_type != D6O_IA_PD) {
				continue;
			}

			/*
			 * Try only pools with the requested prefix length if any.
			 */
			if ((reply->preflen >= 0) && (p->units != reply->preflen)) {
				continue;
			}

			if (create_prefix6(p, pref, &attempts, &reply->ia->iaid_duid,
					   cur_time + 120) == ISC_R_SUCCESS) {
				log_debug("Picking pool prefix %s/%u",
					  inet_ntop(AF_INET6, &((*pref)->addr),
						    tmp_buf, sizeof(tmp_buf)),
					  (unsigned) (*pref)->plen);

				return (ISC_R_SUCCESS);
			}
		}
	}

	/*
	 * If we failed to pick an IPv6 prefix
	 * Presumably that means we have no prefixes for the client.
	 */
	log_debug("Unable to pick client prefix: no prefixes available");
	return ISC_R_NORESOURCES;
}

/*
 *! \file server/dhcpv6.c
 *
 * \brief construct a reply containing information about a client's lease
 *
 * lease_to_client() is called from several messages to construct a
 * reply that contains all that we know about the client's correct lease
 * (or projected lease).
 *
 * Solicit - "Soft" binding, ignore unknown addresses or bindings, just
 *	     send what we "may" give them on a request.
 *
 * Request - "Hard" binding, but ignore supplied addresses (just provide what
 *	     the client should really use).
 *
 * Renew   - "Hard" binding, but client-supplied addresses are 'real'.  Error
 * Rebind    out any "wrong" addresses the client sends.  This means we send
 *	     an empty IA_NA with a status code of NoBinding or NotOnLink or
 *	     possibly send the address with zeroed lifetimes.
 *
 * Information-Request - No binding.
 *
 * The basic structure is to traverse the client-supplied data first, and
 * validate and echo back any contents that can be.  If the client-supplied
 * data does not error out (on renew/rebind as above), but we did not send
 * any addresses, attempt to allocate one.
 *
 * At the end of the this function we call commit_leases_timed() to
 * fsync and rotate the file as necessary.  commit_leases_timed() will
 * check that we have written at least one lease to the file and that
 * some time has passed before doing any fsync or file rewrite so we
 * don't bother tracking if we did a write_ia during this function.
 */
/* TODO: look at client hints for lease times */

static void
lease_to_client(struct data_string *reply_ret,
		struct packet *packet, 
		const struct data_string *client_id,
		const struct data_string *server_id)
{
	static struct reply_state reply;
	struct option_cache *oc;
	struct data_string packet_oro;
#if defined (RFC3315_PRE_ERRATA_2010_08)
	isc_boolean_t no_resources_avail = ISC_FALSE;
#endif
	int i;

	memset(&packet_oro, 0, sizeof(packet_oro));

	/* Locate the client.  */
	if (shared_network_from_packet6(&reply.shared,
					packet) != ISC_R_SUCCESS)
		goto exit;

	/* 
	 * Initialize the reply.
	 */
	packet_reference(&reply.packet, packet, MDL);
	data_string_copy(&reply.client_id, client_id, MDL);

	if (!start_reply(packet, client_id, server_id, &reply.opt_state,
			 &reply.buf.reply))
		goto exit;

	/* Set the write cursor to just past the reply header. */
	reply.cursor = REPLY_OPTIONS_INDEX;

	/*
	 * Get the ORO from the packet, if any.
	 */
	oc = lookup_option(&dhcpv6_universe, packet->options, D6O_ORO);
	if (oc != NULL) {
		if (!evaluate_option_cache(&packet_oro, packet, 
					   NULL, NULL, 
					   packet->options, NULL,
					   &global_scope, oc, MDL)) {
			log_error("lease_to_client: error evaluating ORO.");
			goto exit;
		}
	}

	/* 
	 * Find a host record that matches from the packet, if any, and is
	 * valid for the shared network the client is on.
	 */
	if (find_hosts_by_uid(&reply.host, client_id->data, client_id->len,
			      MDL)) {
		packet->known = 1;
		seek_shared_host(&reply.host, reply.shared);
	}

	if ((reply.host == NULL) &&
	    find_hosts_by_option(&reply.host, packet, packet->options, MDL)) {
		packet->known = 1;
		seek_shared_host(&reply.host, reply.shared);
	}

	/*
	 * Check for 'hardware' matches last, as some of the synthesis methods
	 * are not considered to be as reliable.
	 */
	if ((reply.host == NULL) &&
	    find_hosts_by_duid_chaddr(&reply.host, client_id)) {
		packet->known = 1;
		seek_shared_host(&reply.host, reply.shared);
	}

	/* Process the client supplied IA's onto the reply buffer. */
	reply.ia_count = 0;
	oc = lookup_option(&dhcpv6_universe, packet->options, D6O_IA_NA);

	for (; oc != NULL ; oc = oc->next) {
		isc_result_t status;

		/* Start counting resources (addresses) offered. */
		reply.client_resources = 0;
		reply.resources_included = ISC_FALSE;

		status = reply_process_ia_na(&reply, oc);

		/*
		 * We continue to try other IA's whether we can address
		 * this one or not.  Any other result is an immediate fail.
		 */
		if ((status != ISC_R_SUCCESS) &&
		    (status != ISC_R_NORESOURCES))
			goto exit;

#if defined (RFC3315_PRE_ERRATA_2010_08)
		/*
		 * If any address cannot be given to any IA, then set the
		 * NoAddrsAvail status code.
		 */
		if (reply.client_resources == 0)
			no_resources_avail = ISC_TRUE;
#endif
	}
	oc = lookup_option(&dhcpv6_universe, packet->options, D6O_IA_TA);
	for (; oc != NULL ; oc = oc->next) {
		isc_result_t status;

		/* Start counting resources (addresses) offered. */
		reply.client_resources = 0;
		reply.resources_included = ISC_FALSE;

		status = reply_process_ia_ta(&reply, oc);

		/*
		 * We continue to try other IA's whether we can address
		 * this one or not.  Any other result is an immediate fail.
		 */
		if ((status != ISC_R_SUCCESS) &&
		    (status != ISC_R_NORESOURCES))
			goto exit;

#if defined (RFC3315_PRE_ERRATA_2010_08)
		/*
		 * If any address cannot be given to any IA, then set the
		 * NoAddrsAvail status code.
		 */
		if (reply.client_resources == 0)
			no_resources_avail = ISC_TRUE;
#endif
	}

	/* Same for IA_PD's. */
	reply.pd_count = 0;
	oc = lookup_option(&dhcpv6_universe, packet->options, D6O_IA_PD);
	for (; oc != NULL ; oc = oc->next) {
		isc_result_t status;

		/* Start counting resources (prefixes) offered. */
		reply.client_resources = 0;
		reply.resources_included = ISC_FALSE;

		status = reply_process_ia_pd(&reply, oc);

		/*
		 * We continue to try other IA_PD's whether we can address
		 * this one or not.  Any other result is an immediate fail.
		 */
		if ((status != ISC_R_SUCCESS) &&
		    (status != ISC_R_NORESOURCES))
			goto exit;
	}

	/*
	 * Make no reply if we gave no resources and is not
	 * for Information-Request.
	 */
	if ((reply.ia_count == 0) && (reply.pd_count == 0)) {
		if (reply.packet->dhcpv6_msg_type !=
					    DHCPV6_INFORMATION_REQUEST)
			goto exit;

		/*
		 * Because we only execute statements on a per-IA basis,
		 * we need to execute statements in any non-IA reply to
		 * source configuration.
		 */
		execute_statements_in_scope(NULL, reply.packet, NULL, NULL,
					    reply.packet->options,
					    reply.opt_state, &global_scope,
					    reply.shared->group, root_group,
					    NULL);

		/* Execute statements from class scopes. */
		for (i = reply.packet->class_count; i > 0; i--) {
			execute_statements_in_scope(NULL, reply.packet,
						    NULL, NULL,
						    reply.packet->options,
						    reply.opt_state,
						    &global_scope,
						    reply.packet->classes[i - 1]->group,
						    reply.shared->group, NULL);
		}

		/* Bring in any configuration from a host record. */
		if (reply.host != NULL)
			execute_statements_in_scope(NULL, reply.packet,
						    NULL, NULL,
						    reply.packet->options,
						    reply.opt_state,
						    &global_scope,
						    reply.host->group,
						    reply.shared->group, NULL);
	}

	/*
	 * RFC3315 section 17.2.2 (Solicit):
	 *
	 * If the server will not assign any addresses to any IAs in a
	 * subsequent Request from the client, the server MUST send an
	 * Advertise message to the client that includes only a Status
	 * Code option with code NoAddrsAvail and a status message for
	 * the user, a Server Identifier option with the server's DUID,
	 * and a Client Identifier option with the client's DUID.
	 *
	 * Section 18.2.1 (Request):
	 *
	 * If the server cannot assign any addresses to an IA in the
	 * message from the client, the server MUST include the IA in
	 * the Reply message with no addresses in the IA and a Status
	 * Code option in the IA containing status code NoAddrsAvail.
	 *
	 * Section 18.1.8 (Client Behavior):
	 *
	 * Leave unchanged any information about addresses the client has
	 * recorded in the IA but that were not included in the IA from
	 * the server.
	 * Sends a Renew/Rebind if the IA is not in the Reply message.
	 */
#if defined (RFC3315_PRE_ERRATA_2010_08)
	if (no_resources_avail && (reply.ia_count != 0) &&
	    (reply.packet->dhcpv6_msg_type == DHCPV6_SOLICIT))
	{
		/* Set the NoAddrsAvail status code. */
		if (!set_status_code(STATUS_NoAddrsAvail,
				     "No addresses available for this "
				     "interface.", reply.opt_state)) {
			log_error("lease_to_client: Unable to set "
				  "NoAddrsAvail status code.");
			goto exit;
		}

		/* Rewind the cursor to the start. */
		reply.cursor = REPLY_OPTIONS_INDEX;

		/*
		 * Produce an advertise that includes only:
		 *
		 * Status code.
		 * Server DUID.
		 * Client DUID.
		 */
		reply.buf.reply.msg_type = DHCPV6_ADVERTISE;
		reply.cursor += store_options6((char *)reply.buf.data +
							reply.cursor,
					       sizeof(reply.buf) -
					       		reply.cursor,
					       reply.opt_state, reply.packet,
					       required_opts_NAA,
					       NULL);
	} else {
		/*
		 * Having stored the client's IA's, store any options that
		 * will fit in the remaining space.
		 */
		reply.cursor += store_options6((char *)reply.buf.data +
							reply.cursor,
					       sizeof(reply.buf) -
							reply.cursor,
					       reply.opt_state, reply.packet,
					       required_opts_solicit,
					       &packet_oro);
	}
#else /* defined (RFC3315_PRE_ERRATA_2010_08) */
	/*
	 * Having stored the client's IA's, store any options that
	 * will fit in the remaining space.
	 */
	reply.cursor += store_options6((char *)reply.buf.data + reply.cursor,
				       sizeof(reply.buf) - reply.cursor,
				       reply.opt_state, reply.packet,
				       required_opts_solicit,
				       &packet_oro);
#endif /* defined (RFC3315_PRE_ERRATA_2010_08) */

	/* Return our reply to the caller. */
	reply_ret->len = reply.cursor;
	reply_ret->buffer = NULL;
	if (!buffer_allocate(&reply_ret->buffer, reply.cursor, MDL)) {
		log_fatal("No memory to store Reply.");
	}
	memcpy(reply_ret->buffer->data, reply.buf.data, reply.cursor);
	reply_ret->data = reply_ret->buffer->data;

	/* If appropriate commit and rotate the lease file */
	(void) commit_leases_timed();

      exit:
	/* Cleanup. */
	if (reply.shared != NULL)
		shared_network_dereference(&reply.shared, MDL);
	if (reply.host != NULL)
		host_dereference(&reply.host, MDL);
	if (reply.opt_state != NULL)
		option_state_dereference(&reply.opt_state, MDL);
	if (reply.packet != NULL)
		packet_dereference(&reply.packet, MDL);
	if (reply.client_id.data != NULL)
		data_string_forget(&reply.client_id, MDL);
	if (packet_oro.buffer != NULL)
		data_string_forget(&packet_oro, MDL);
	reply.renew = reply.rebind = reply.prefer = reply.valid = 0;
	reply.cursor = 0;
}

/* Process a client-supplied IA_NA.  This may append options to the tail of
 * the reply packet being built in the reply_state structure.
 */
static isc_result_t
reply_process_ia_na(struct reply_state *reply, struct option_cache *ia) {
	isc_result_t status = ISC_R_SUCCESS;
	u_int32_t iaid;
	unsigned ia_cursor;
	struct option_state *packet_ia;
	struct option_cache *oc;
	struct data_string ia_data, data;

	/* Initialize values that will get cleaned up on return. */
	packet_ia = NULL;
	memset(&ia_data, 0, sizeof(ia_data));
	memset(&data, 0, sizeof(data));
	/* 
	 * Note that find_client_address() may set reply->lease. 
	 */

	/* Make sure there is at least room for the header. */
	if ((reply->cursor + IA_NA_OFFSET + 4) > sizeof(reply->buf)) {
		log_error("reply_process_ia_na: Reply too long for IA.");
		return ISC_R_NOSPACE;
	}


	/* Fetch the IA_NA contents. */
	if (!get_encapsulated_IA_state(&packet_ia, &ia_data, reply->packet,
				       ia, IA_NA_OFFSET)) {
		log_error("reply_process_ia_na: error evaluating ia");
		status = ISC_R_FAILURE;
		goto cleanup;
	}

	/* Extract IA_NA header contents. */
	iaid = getULong(ia_data.data);
	reply->renew = getULong(ia_data.data + 4);
	reply->rebind = getULong(ia_data.data + 8);

	/* Create an IA_NA structure. */
	if (ia_allocate(&reply->ia, iaid, (char *)reply->client_id.data, 
			reply->client_id.len, MDL) != ISC_R_SUCCESS) {
		log_error("reply_process_ia_na: no memory for ia.");
		status = ISC_R_NOMEMORY;
		goto cleanup;
	}
	reply->ia->ia_type = D6O_IA_NA;

	/* Cache pre-existing IA, if any. */
	ia_hash_lookup(&reply->old_ia, ia_na_active,
		       (unsigned char *)reply->ia->iaid_duid.data,
		       reply->ia->iaid_duid.len, MDL);

	/*
	 * Create an option cache to carry the IA_NA option contents, and
	 * execute any user-supplied values into it.
	 */
	if (!option_state_allocate(&reply->reply_ia, MDL)) {
		status = ISC_R_NOMEMORY;
		goto cleanup;
	}

	/* Check & cache the fixed host record. */
	if ((reply->host != NULL) && (reply->host->fixed_addr != NULL)) {
		struct iaddr tmp_addr;

		if (!evaluate_option_cache(&reply->fixed, NULL, NULL, NULL,
					   NULL, NULL, &global_scope,
					   reply->host->fixed_addr, MDL)) {
			log_error("reply_process_ia_na: unable to evaluate "
				  "fixed address.");
			status = ISC_R_FAILURE;
			goto cleanup;
		}

		if (reply->fixed.len < 16) {
			log_error("reply_process_ia_na: invalid fixed address.");
			status = DHCP_R_INVALIDARG;
			goto cleanup;
		}

		/* Find the static lease's subnet. */
		tmp_addr.len = 16;
		memcpy(tmp_addr.iabuf, reply->fixed.data, 16);

		if (find_grouped_subnet(&reply->subnet, reply->shared,
					tmp_addr, MDL) == 0)
			log_fatal("Impossible condition at %s:%d.", MDL);

		reply->static_lease = ISC_TRUE;
	} else
		reply->static_lease = ISC_FALSE;

	/*
	 * Save the cursor position at the start of the IA, so we can
	 * set length and adjust t1/t2 values later.  We write a temporary
	 * header out now just in case we decide to adjust the packet
	 * within sub-process functions.
	 */
	ia_cursor = reply->cursor;

	/* Initialize the IA_NA header.  First the code. */
	putUShort(reply->buf.data + reply->cursor, (unsigned)D6O_IA_NA);
	reply->cursor += 2;

	/* Then option length. */
	putUShort(reply->buf.data + reply->cursor, 0x0Cu);
	reply->cursor += 2;

	/* Then IA_NA header contents; IAID. */
	putULong(reply->buf.data + reply->cursor, iaid);
	reply->cursor += 4;

	/* We store the client's t1 for now, and may over-ride it later. */
	putULong(reply->buf.data + reply->cursor, reply->renew);
	reply->cursor += 4;

	/* We store the client's t2 for now, and may over-ride it later. */
	putULong(reply->buf.data + reply->cursor, reply->rebind);
	reply->cursor += 4;

	/* 
	 * For each address in this IA_NA, decide what to do about it.
	 *
	 * Guidelines:
	 *
	 * The client leaves unchanged any information about addresses
	 * it has recorded but are not included ("cancel/break" below).
	 * A not included IA ("cleanup" below) could give a Renew/Rebind.
	 */
	oc = lookup_option(&dhcpv6_universe, packet_ia, D6O_IAADDR);
	reply->valid = reply->prefer = 0xffffffff;
	reply->client_valid = reply->client_prefer = 0;
	for (; oc != NULL ; oc = oc->next) {
		status = reply_process_addr(reply, oc);

		/*
		 * Canceled means we did not allocate addresses to the
		 * client, but we're "done" with this IA - we set a status
		 * code.  So transmit this reply, e.g., move on to the next
		 * IA.
		 */
		if (status == ISC_R_CANCELED)
			break;

		if ((status != ISC_R_SUCCESS) &&
		    (status != ISC_R_ADDRINUSE) &&
		    (status != ISC_R_ADDRNOTAVAIL))
			goto cleanup;
	}

	reply->ia_count++;

	/*
	 * If we fell through the above and never gave the client
	 * an address, give it one now.
	 */
	if ((status != ISC_R_CANCELED) && (reply->client_resources == 0)) {
		status = find_client_address(reply);

		if (status == ISC_R_NORESOURCES) {
			switch (reply->packet->dhcpv6_msg_type) {
			      case DHCPV6_SOLICIT:
				/*
				 * No address for any IA is handled
				 * by the caller.
				 */
				/* FALL THROUGH */

			      case DHCPV6_REQUEST:
				/* Section 18.2.1 (Request):
				 *
				 * If the server cannot assign any addresses to
				 * an IA in the message from the client, the
				 * server MUST include the IA in the Reply
				 * message with no addresses in the IA and a
				 * Status Code option in the IA containing
				 * status code NoAddrsAvail.
				 */
				option_state_dereference(&reply->reply_ia, MDL);
				if (!option_state_allocate(&reply->reply_ia,
							   MDL))
				{
					log_error("reply_process_ia_na: No "
						  "memory for option state "
						  "wipe.");
					status = ISC_R_NOMEMORY;
					goto cleanup;
				}

				if (!set_status_code(STATUS_NoAddrsAvail,
						     "No addresses available "
						     "for this interface.",
						      reply->reply_ia)) {
					log_error("reply_process_ia_na: Unable "
						  "to set NoAddrsAvail status "
						  "code.");
					status = ISC_R_FAILURE;
					goto cleanup;
				}

				status = ISC_R_SUCCESS;
				break;

			      default:
				/*
				 * RFC 3315 does not tell us to emit a status
				 * code in this condition, or anything else.
				 *
				 * If we included non-allocated addresses
				 * (zeroed lifetimes) in an IA, then the client
				 * will deconfigure them.
				 *
				 * So we want to include the IA even if we
				 * can't give it a new address if it includes
				 * zeroed lifetime addresses.
				 *
				 * We don't want to include the IA if we
				 * provide zero addresses including zeroed
				 * lifetimes.
				 */
				if (reply->resources_included)
					status = ISC_R_SUCCESS;
				else
					goto cleanup;
				break;
			}
		}

		if (status != ISC_R_SUCCESS)
			goto cleanup;
	}

	reply->cursor += store_options6((char *)reply->buf.data + reply->cursor,
					sizeof(reply->buf) - reply->cursor,
					reply->reply_ia, reply->packet,
					required_opts_IA, NULL);

	/* Reset the length of this IA to match what was just written. */
	putUShort(reply->buf.data + ia_cursor + 2,
		  reply->cursor - (ia_cursor + 4));

	/*
	 * T1/T2 time selection is kind of weird.  We actually use DHCP
	 * (v4) scoped options as handy existing places where these might
	 * be configured by an administrator.  A value of zero tells the
	 * client it may choose its own renewal time.
	 */
	reply->renew = 0;
	oc = lookup_option(&dhcp_universe, reply->opt_state,
			   DHO_DHCP_RENEWAL_TIME);
	if (oc != NULL) {
		if (!evaluate_option_cache(&data, reply->packet, NULL, NULL,
					   reply->packet->options,
					   reply->opt_state, &global_scope,
					   oc, MDL) ||
		    (data.len != 4)) {
			log_error("Invalid renewal time.");
		} else {
			reply->renew = getULong(data.data);
		}

		if (data.data != NULL)
			data_string_forget(&data, MDL);
	}
	putULong(reply->buf.data + ia_cursor + 8, reply->renew);

	/* Now T2. */
	reply->rebind = 0;
	oc = lookup_option(&dhcp_universe, reply->opt_state,
			   DHO_DHCP_REBINDING_TIME);
	if (oc != NULL) {
		if (!evaluate_option_cache(&data, reply->packet, NULL, NULL,
					   reply->packet->options,
					   reply->opt_state, &global_scope,
					   oc, MDL) ||
		    (data.len != 4)) {
			log_error("Invalid rebinding time.");
		} else {
			reply->rebind = getULong(data.data);
		}

		if (data.data != NULL)
			data_string_forget(&data, MDL);
	}
	putULong(reply->buf.data + ia_cursor + 12, reply->rebind);

	/*
	 * yes, goto's aren't the best but we also want to avoid extra
	 * indents
	 */
	if (status == ISC_R_CANCELED)
		goto cleanup;

	/*
	 * Handle static leases, we always log stuff and if it's
	 * a hard binding we run any commit statements that we have
	 */
	if (reply->static_lease) {
		char tmp_addr[INET6_ADDRSTRLEN];
		log_info("%s NA: address %s to client with duid %s iaid = %d "
			 "static",
			 dhcpv6_type_names[reply->buf.reply.msg_type],
			 inet_ntop(AF_INET6, reply->fixed.data, tmp_addr,
				   sizeof(tmp_addr)),
			 print_hex_1(reply->client_id.len,
				     reply->client_id.data, 60),
			 iaid);

		if ((reply->buf.reply.msg_type == DHCPV6_REPLY) &&
		    (reply->on_star.on_commit != NULL)) {
			execute_statements(NULL, reply->packet, NULL, NULL, 
					   reply->packet->options,
					   reply->opt_state, NULL,
					   reply->on_star.on_commit, NULL);
			executable_statement_dereference
				(&reply->on_star.on_commit, MDL);
		}
		goto cleanup;
	}

	/*
	 * If we have any addresses log what we are doing.
	 */
	if (reply->ia->num_iasubopt != 0) {
		struct iasubopt *tmp;
		int i;
		char tmp_addr[INET6_ADDRSTRLEN];

		for (i = 0 ; i < reply->ia->num_iasubopt ; i++) {
			tmp = reply->ia->iasubopt[i];

			log_info("%s NA: address %s to client with duid %s "
				 "iaid = %d valid for %d seconds",
				 dhcpv6_type_names[reply->buf.reply.msg_type],
				 inet_ntop(AF_INET6, &tmp->addr,
					   tmp_addr, sizeof(tmp_addr)),
				 print_hex_1(reply->client_id.len,
					     reply->client_id.data, 60),
				 iaid, tmp->valid);
		}
	}

	/*
	 * If this is not a 'soft' binding, consume the new changes into
	 * the database (if any have been attached to the ia_na).
	 *
	 * Loop through the assigned dynamic addresses, referencing the
	 * leases onto this IA_NA rather than any old ones, and updating
	 * pool timers for each (if any).
	 */

	if ((reply->ia->num_iasubopt != 0) &&
	    (reply->buf.reply.msg_type == DHCPV6_REPLY)) {
		struct iasubopt *tmp;
		struct data_string *ia_id;
		int i;

		for (i = 0 ; i < reply->ia->num_iasubopt ; i++) {
			tmp = reply->ia->iasubopt[i];

			if (tmp->ia != NULL)
				ia_dereference(&tmp->ia, MDL);
			ia_reference(&tmp->ia, reply->ia, MDL);

			/* Commit 'hard' bindings. */
			renew_lease6(tmp->ipv6_pool, tmp);
			schedule_lease_timeout(tmp->ipv6_pool);

			/* If we have anything to do on commit do it now */
			if (tmp->on_star.on_commit != NULL) {
				execute_statements(NULL, reply->packet,
						   NULL, NULL, 
						   reply->packet->options,
						   reply->opt_state,
						   &tmp->scope,
						   tmp->on_star.on_commit,
						   &tmp->on_star);
				executable_statement_dereference
					(&tmp->on_star.on_commit, MDL);
			}

#if defined (NSUPDATE)
			/*
			 * Perform ddns updates.
			 */
			oc = lookup_option(&server_universe, reply->opt_state,
					   SV_DDNS_UPDATES);
			if ((oc == NULL) ||
			    evaluate_boolean_option_cache(NULL, reply->packet,
							  NULL, NULL,
							reply->packet->options,
							  reply->opt_state,
							  &tmp->scope,
							  oc, MDL)) {
				ddns_updates(reply->packet, NULL, NULL,
					     tmp, NULL, reply->opt_state);
			}
#endif
		}

		/* Remove any old ia from the hash. */
		if (reply->old_ia != NULL) {
			ia_id = &reply->old_ia->iaid_duid;
			ia_hash_delete(ia_na_active,
				       (unsigned char *)ia_id->data,
				       ia_id->len, MDL);
			ia_dereference(&reply->old_ia, MDL);
		}

		/* Put new ia into the hash. */
		reply->ia->cltt = cur_time;
		ia_id = &reply->ia->iaid_duid;
		ia_hash_add(ia_na_active, (unsigned char *)ia_id->data,
			    ia_id->len, reply->ia, MDL);

		write_ia(reply->ia);
	}

      cleanup:
	if (packet_ia != NULL)
		option_state_dereference(&packet_ia, MDL);
	if (reply->reply_ia != NULL)
		option_state_dereference(&reply->reply_ia, MDL);
	if (ia_data.data != NULL)
		data_string_forget(&ia_data, MDL);
	if (data.data != NULL)
		data_string_forget(&data, MDL);
	if (reply->ia != NULL)
		ia_dereference(&reply->ia, MDL);
	if (reply->old_ia != NULL)
		ia_dereference(&reply->old_ia, MDL);
	if (reply->lease != NULL)
		iasubopt_dereference(&reply->lease, MDL);
	if (reply->fixed.data != NULL)
		data_string_forget(&reply->fixed, MDL);
	if (reply->subnet != NULL)
		subnet_dereference(&reply->subnet, MDL);
	if (reply->on_star.on_expiry != NULL)
		executable_statement_dereference
			(&reply->on_star.on_expiry, MDL);
	if (reply->on_star.on_release != NULL)
		executable_statement_dereference
			(&reply->on_star.on_release, MDL);

	/*
	 * ISC_R_CANCELED is a status code used by the addr processing to
	 * indicate we're replying with a status code.  This is still a
	 * success at higher layers.
	 */
	return((status == ISC_R_CANCELED) ? ISC_R_SUCCESS : status);
}

/*
 * Process an IAADDR within a given IA_xA, storing any IAADDR reply contents
 * into the reply's current ia-scoped option cache.  Returns ISC_R_CANCELED
 * in the event we are replying with a status code and do not wish to process
 * more IAADDRs within this IA.
 */
static isc_result_t
reply_process_addr(struct reply_state *reply, struct option_cache *addr) {
	u_int32_t pref_life, valid_life;
	struct binding_scope **scope;
	struct group *group;
	struct subnet *subnet;
	struct iaddr tmp_addr;
	struct option_cache *oc;
	struct data_string iaaddr, data;
	isc_result_t status = ISC_R_SUCCESS;

	/* Initializes values that will be cleaned up. */
	memset(&iaaddr, 0, sizeof(iaaddr));
	memset(&data, 0, sizeof(data));
	/* Note that reply->lease may be set by address_is_owned() */

	/*
	 * There is no point trying to process an incoming address if there
	 * is no room for an outgoing address.
	 */
	if ((reply->cursor + 28) > sizeof(reply->buf)) {
		log_error("reply_process_addr: Out of room for address.");
		return ISC_R_NOSPACE;
	}

	/* Extract this IAADDR option. */
	if (!evaluate_option_cache(&iaaddr, reply->packet, NULL, NULL, 
				   reply->packet->options, NULL, &global_scope,
				   addr, MDL) ||
	    (iaaddr.len < IAADDR_OFFSET)) {
		log_error("reply_process_addr: error evaluating IAADDR.");
		status = ISC_R_FAILURE;
		goto cleanup;
	}

	/* The first 16 bytes are the IPv6 address. */
	pref_life = getULong(iaaddr.data + 16);
	valid_life = getULong(iaaddr.data + 20);

	if ((reply->client_valid == 0) ||
	    (reply->client_valid > valid_life))
		reply->client_valid = valid_life;

	if ((reply->client_prefer == 0) ||
	    (reply->client_prefer > pref_life))
		reply->client_prefer = pref_life;

	/* 
	 * Clients may choose to send :: as an address, with the idea to give
	 * hints about preferred-lifetime or valid-lifetime.
	 */
	tmp_addr.len = 16;
	memset(tmp_addr.iabuf, 0, 16);
	if (!memcmp(iaaddr.data, tmp_addr.iabuf, 16)) {
		/* Status remains success; we just ignore this one. */
		goto cleanup;
	}

	/* tmp_addr len remains 16 */
	memcpy(tmp_addr.iabuf, iaaddr.data, 16);

	/*
	 * Verify that this address is on the client's network.
	 */
	for (subnet = reply->shared->subnets ; subnet != NULL ;
	     subnet = subnet->next_sibling) {
		if (addr_eq(subnet_number(tmp_addr, subnet->netmask),
			    subnet->net))
			break;
	}

	/* Address not found on shared network. */
	if (subnet == NULL) {
		/* Ignore this address on 'soft' bindings. */
		if (reply->packet->dhcpv6_msg_type == DHCPV6_SOLICIT) {
			/* disable rapid commit */
			reply->buf.reply.msg_type = DHCPV6_ADVERTISE;
			delete_option(&dhcpv6_universe,
				      reply->opt_state,
				      D6O_RAPID_COMMIT);
			/* status remains success */
			goto cleanup;
		}

		/*
		 * RFC3315 section 18.2.1:
		 *
		 * If the server finds that the prefix on one or more IP
		 * addresses in any IA in the message from the client is not
		 * appropriate for the link to which the client is connected,
		 * the server MUST return the IA to the client with a Status
		 * Code option with the value NotOnLink.
		 */
		if (reply->packet->dhcpv6_msg_type == DHCPV6_REQUEST) {
			/* Rewind the IA_NA to empty. */
			option_state_dereference(&reply->reply_ia, MDL);
			if (!option_state_allocate(&reply->reply_ia, MDL)) {
				log_error("reply_process_addr: No memory for "
					  "option state wipe.");
				status = ISC_R_NOMEMORY;
				goto cleanup;
			}

			/* Append a NotOnLink status code. */
			if (!set_status_code(STATUS_NotOnLink,
					     "Address not for use on this "
					     "link.", reply->reply_ia)) {
				log_error("reply_process_addr: Failure "
					  "setting status code.");
				status = ISC_R_FAILURE;
				goto cleanup;
			}

			/* Fin (no more IAADDRs). */
			status = ISC_R_CANCELED;
			goto cleanup;
		}

		/*
		 * RFC3315 sections 18.2.3 and 18.2.4 have identical language:
		 *
		 * If the server finds that any of the addresses are not
		 * appropriate for the link to which the client is attached,
		 * the server returns the address to the client with lifetimes
		 * of 0.
		 */
		if ((reply->packet->dhcpv6_msg_type != DHCPV6_RENEW) &&
		    (reply->packet->dhcpv6_msg_type != DHCPV6_REBIND)) {
			log_error("It is impossible to lease a client that is "
				  "not sending a solicit, request, renew, or "
				  "rebind.");
			status = ISC_R_FAILURE;
			goto cleanup;
		}

		reply->send_prefer = reply->send_valid = 0;
		goto send_addr;
	}

	/* Verify the address belongs to the client. */
	if (!address_is_owned(reply, &tmp_addr)) {
		/*
		 * For solicit and request, any addresses included are
		 * 'requested' addresses.  For rebind, we actually have
		 * no direction on what to do from 3315 section 18.2.4!
		 * So I think the best bet is to try and give it out, and if
		 * we can't, zero lifetimes.
		 */
		if ((reply->packet->dhcpv6_msg_type == DHCPV6_SOLICIT) ||
		    (reply->packet->dhcpv6_msg_type == DHCPV6_REQUEST) ||
		    (reply->packet->dhcpv6_msg_type == DHCPV6_REBIND)) {
			status = reply_process_try_addr(reply, &tmp_addr);

			/*
			 * If the address is in use, or isn't in any dynamic
			 * range, continue as normal.  If any other error was
			 * found, error out.
			 */
			if ((status != ISC_R_SUCCESS) && 
			    (status != ISC_R_ADDRINUSE) &&
			    (status != ISC_R_ADDRNOTAVAIL))
				goto cleanup;

			/*
			 * If we didn't honor this lease, for solicit and
			 * request we simply omit it from our answer.  For
			 * rebind, we send it with zeroed lifetimes.
			 */
			if (reply->lease == NULL) {
				if (reply->packet->dhcpv6_msg_type ==
							DHCPV6_REBIND) {
					reply->send_prefer = 0;
					reply->send_valid = 0;
					goto send_addr;
				}

				/* status remains success - ignore */
				goto cleanup;
			}
		/*
		 * RFC3315 section 18.2.3:
		 *
		 * If the server cannot find a client entry for the IA the
		 * server returns the IA containing no addresses with a Status
		 * Code option set to NoBinding in the Reply message.
		 *
		 * On mismatch we (ab)use this pretending we have not the IA
		 * as soon as we have not an address.
		 */
		} else if (reply->packet->dhcpv6_msg_type == DHCPV6_RENEW) {
			/* Rewind the IA_NA to empty. */
			option_state_dereference(&reply->reply_ia, MDL);
			if (!option_state_allocate(&reply->reply_ia, MDL)) {
				log_error("reply_process_addr: No memory for "
					  "option state wipe.");
				status = ISC_R_NOMEMORY;
				goto cleanup;
			}

			/* Append a NoBinding status code.  */
			if (!set_status_code(STATUS_NoBinding,
					     "Address not bound to this "
					     "interface.", reply->reply_ia)) {
				log_error("reply_process_addr: Unable to "
					  "attach status code.");
				status = ISC_R_FAILURE;
				goto cleanup;
			}

			/* Fin (no more IAADDRs). */
			status = ISC_R_CANCELED;
			goto cleanup;
		} else {
			log_error("It is impossible to lease a client that is "
				  "not sending a solicit, request, renew, or "
				  "rebind message.");
			status = ISC_R_FAILURE;
			goto cleanup;
		}
	}

	if (reply->static_lease) {
		if (reply->host == NULL)
			log_fatal("Impossible condition at %s:%d.", MDL);

		scope = &global_scope;
		group = reply->subnet->group;
	} else {
		if (reply->lease == NULL)
			log_fatal("Impossible condition at %s:%d.", MDL);

		scope = &reply->lease->scope;
		group = reply->lease->ipv6_pool->ipv6_pond->group;
	}

	/*
	 * If client_resources is nonzero, then the reply_process_is_addressed
	 * function has executed configuration state into the reply option
	 * cache.  We will use that valid cache to derive configuration for
	 * whether or not to engage in additional addresses, and similar.
	 */
	if (reply->client_resources != 0) {
		unsigned limit = 1;

		/*
		 * Does this client have "enough" addresses already?  Default
		 * to one.  Everybody gets one, and one should be enough for
		 * anybody.
		 */
		oc = lookup_option(&server_universe, reply->opt_state,
				   SV_LIMIT_ADDRS_PER_IA);
		if (oc != NULL) {
			if (!evaluate_option_cache(&data, reply->packet,
						   NULL, NULL,
						   reply->packet->options,
						   reply->opt_state,
						   scope, oc, MDL) ||
			    (data.len != 4)) {
				log_error("reply_process_addr: unable to "
					  "evaluate addrs-per-ia value.");
				status = ISC_R_FAILURE;
				goto cleanup;
			}

			limit = getULong(data.data);
			data_string_forget(&data, MDL);
		}

		/*
		 * If we wish to limit the client to a certain number of
		 * addresses, then omit the address from the reply.
		 */
		if (reply->client_resources >= limit)
			goto cleanup;
	}

	status = reply_process_is_addressed(reply, scope, group);
	if (status != ISC_R_SUCCESS)
		goto cleanup;

      send_addr:
	status = reply_process_send_addr(reply, &tmp_addr);

      cleanup:
	if (iaaddr.data != NULL)
		data_string_forget(&iaaddr, MDL);
	if (data.data != NULL)
		data_string_forget(&data, MDL);
	if (reply->lease != NULL)
		iasubopt_dereference(&reply->lease, MDL);

	return status;
}

/*
 * Verify the address belongs to the client.  If we've got a host
 * record with a fixed address, it has to be the assigned address
 * (fault out all else).  Otherwise it's a dynamic address, so lookup
 * that address and make sure it belongs to this DUID:IAID pair.
 */
static isc_boolean_t
address_is_owned(struct reply_state *reply, struct iaddr *addr) {
	int i;
	struct ipv6_pond *pond;

	/*
	 * This faults out addresses that don't match fixed addresses.
	 */
	if (reply->static_lease) {
		if (reply->fixed.data == NULL)
			log_fatal("Impossible condition at %s:%d.", MDL);

		if (memcmp(addr->iabuf, reply->fixed.data, 16) == 0)
			return (ISC_TRUE);

		return (ISC_FALSE);
	}

	if ((reply->old_ia == NULL) || (reply->old_ia->num_iasubopt == 0))
		return (ISC_FALSE);

	for (i = 0 ; i < reply->old_ia->num_iasubopt ; i++) {
		struct iasubopt *tmp;

		tmp = reply->old_ia->iasubopt[i];

		if (memcmp(addr->iabuf, &tmp->addr, 16) == 0) {
			if (lease6_usable(tmp) == ISC_FALSE) {
				return (ISC_FALSE);
			}

			pond = tmp->ipv6_pool->ipv6_pond;
			if (((pond->prohibit_list != NULL) &&
			     (permitted(reply->packet, pond->prohibit_list))) ||
			    ((pond->permit_list != NULL) &&
			     (!permitted(reply->packet, pond->permit_list))))
				return (ISC_FALSE);

			iasubopt_reference(&reply->lease, tmp, MDL);

			return (ISC_TRUE);
		}
	}

	return (ISC_FALSE);
}

/* Process a client-supplied IA_TA.  This may append options to the tail of
 * the reply packet being built in the reply_state structure.
 */
static isc_result_t
reply_process_ia_ta(struct reply_state *reply, struct option_cache *ia) {
	isc_result_t status = ISC_R_SUCCESS;
	u_int32_t iaid;
	unsigned ia_cursor;
	struct option_state *packet_ia;
	struct option_cache *oc;
	struct data_string ia_data, data;
	struct data_string iaaddr;
	u_int32_t pref_life, valid_life;
	struct iaddr tmp_addr;

	/* Initialize values that will get cleaned up on return. */
	packet_ia = NULL;
	memset(&ia_data, 0, sizeof(ia_data));
	memset(&data, 0, sizeof(data));
	memset(&iaaddr, 0, sizeof(iaaddr));

	/* Make sure there is at least room for the header. */
	if ((reply->cursor + IA_TA_OFFSET + 4) > sizeof(reply->buf)) {
		log_error("reply_process_ia_ta: Reply too long for IA.");
		return ISC_R_NOSPACE;
	}


	/* Fetch the IA_TA contents. */
	if (!get_encapsulated_IA_state(&packet_ia, &ia_data, reply->packet,
				       ia, IA_TA_OFFSET)) {
		log_error("reply_process_ia_ta: error evaluating ia");
		status = ISC_R_FAILURE;
		goto cleanup;
	}

	/* Extract IA_TA header contents. */
	iaid = getULong(ia_data.data);

	/* Create an IA_TA structure. */
	if (ia_allocate(&reply->ia, iaid, (char *)reply->client_id.data,
			reply->client_id.len, MDL) != ISC_R_SUCCESS) {
		log_error("reply_process_ia_ta: no memory for ia.");
		status = ISC_R_NOMEMORY;
		goto cleanup;
	}
	reply->ia->ia_type = D6O_IA_TA;

	/* Cache pre-existing IA, if any. */
	ia_hash_lookup(&reply->old_ia, ia_ta_active,
		       (unsigned char *)reply->ia->iaid_duid.data,
		       reply->ia->iaid_duid.len, MDL);

	/*
	 * Create an option cache to carry the IA_TA option contents, and
	 * execute any user-supplied values into it.
	 */
	if (!option_state_allocate(&reply->reply_ia, MDL)) {
		status = ISC_R_NOMEMORY;
		goto cleanup;
	}

	/*
	 * Temporary leases are dynamic by definition.
	 */
	reply->static_lease = ISC_FALSE;

	/*
	 * Save the cursor position at the start of the IA, so we can
	 * set length later.  We write a temporary
	 * header out now just in case we decide to adjust the packet
	 * within sub-process functions.
	 */
	ia_cursor = reply->cursor;

	/* Initialize the IA_TA header.  First the code. */
	putUShort(reply->buf.data + reply->cursor, (unsigned)D6O_IA_TA);
	reply->cursor += 2;

	/* Then option length. */
	putUShort(reply->buf.data + reply->cursor, 0x04u);
	reply->cursor += 2;

	/* Then IA_TA header contents; IAID. */
	putULong(reply->buf.data + reply->cursor, iaid);
	reply->cursor += 4;

	/* 
	 * Deal with an IAADDR for lifetimes.
	 * For all or none, process IAADDRs as hints.
	 */
	reply->valid = reply->prefer = 0xffffffff;
	reply->client_valid = reply->client_prefer = 0;
	oc = lookup_option(&dhcpv6_universe, packet_ia, D6O_IAADDR);
	for (; oc != NULL; oc = oc->next) {
		memset(&iaaddr, 0, sizeof(iaaddr));
		if (!evaluate_option_cache(&iaaddr, reply->packet,
					   NULL, NULL,
					   reply->packet->options, NULL,
					   &global_scope, oc, MDL) ||
		    (iaaddr.len < IAADDR_OFFSET)) {
			log_error("reply_process_ia_ta: error "
				  "evaluating IAADDR.");
			status = ISC_R_FAILURE;
			goto cleanup;
		}
		/* The first 16 bytes are the IPv6 address. */
		pref_life = getULong(iaaddr.data + 16);
		valid_life = getULong(iaaddr.data + 20);

		if ((reply->client_valid == 0) ||
		    (reply->client_valid > valid_life))
			reply->client_valid = valid_life;

		if ((reply->client_prefer == 0) ||
		    (reply->client_prefer > pref_life))
			reply->client_prefer = pref_life;

		/* Nothing more if something has failed. */
		if (status == ISC_R_CANCELED)
			continue;

		tmp_addr.len = 16;
		memcpy(tmp_addr.iabuf, iaaddr.data, 16);
		if (!temporary_is_available(reply, &tmp_addr))
			goto bad_temp;
		status = reply_process_is_addressed(reply,
						    &reply->lease->scope,
						    reply->lease->ipv6_pool->ipv6_pond->group);
		if (status != ISC_R_SUCCESS)
			goto bad_temp;
		status = reply_process_send_addr(reply, &tmp_addr);
		if (status != ISC_R_SUCCESS)
			goto bad_temp;
		if (reply->lease != NULL)
			iasubopt_dereference(&reply->lease, MDL);
		continue;

	bad_temp:
		/* Rewind the IA_TA to empty. */
		option_state_dereference(&reply->reply_ia, MDL);
		if (!option_state_allocate(&reply->reply_ia, MDL)) {
			status = ISC_R_NOMEMORY;
			goto cleanup;
		}
		status = ISC_R_CANCELED;
		reply->client_resources = 0;
		reply->resources_included = ISC_FALSE;
		if (reply->lease != NULL)
			iasubopt_dereference(&reply->lease, MDL);
	}
	reply->ia_count++;

	/*
	 * Give the client temporary addresses.
	 */
	if (reply->client_resources != 0)
		goto store;
	status = find_client_temporaries(reply);
	if (status == ISC_R_NORESOURCES) {
		switch (reply->packet->dhcpv6_msg_type) {
		      case DHCPV6_SOLICIT:
			/*
			 * No address for any IA is handled
			 * by the caller.
			 */
			/* FALL THROUGH */

		      case DHCPV6_REQUEST:
			/* Section 18.2.1 (Request):
			 *
			 * If the server cannot assign any addresses to
			 * an IA in the message from the client, the
			 * server MUST include the IA in the Reply
			 * message with no addresses in the IA and a
			 * Status Code option in the IA containing
			 * status code NoAddrsAvail.
			 */
			option_state_dereference(&reply->reply_ia, MDL);
			if (!option_state_allocate(&reply->reply_ia,  MDL)) {
				log_error("reply_process_ia_ta: No "
					  "memory for option state wipe.");
				status = ISC_R_NOMEMORY;
				goto cleanup;
			}

			if (!set_status_code(STATUS_NoAddrsAvail,
					     "No addresses available "
					     "for this interface.",
					      reply->reply_ia)) {
				log_error("reply_process_ia_ta: Unable "
					  "to set NoAddrsAvail status code.");
				status = ISC_R_FAILURE;
				goto cleanup;
			}

			status = ISC_R_SUCCESS;
			break;

		      default:
			/*
			 * We don't want to include the IA if we
			 * provide zero addresses including zeroed
			 * lifetimes.
			 */
			if (reply->resources_included)
				status = ISC_R_SUCCESS;
			else
				goto cleanup;
			break;
		}
	} else if (status != ISC_R_SUCCESS)
		goto cleanup;

      store:
	reply->cursor += store_options6((char *)reply->buf.data + reply->cursor,
					sizeof(reply->buf) - reply->cursor,
					reply->reply_ia, reply->packet,
					required_opts_IA, NULL);

	/* Reset the length of this IA to match what was just written. */
	putUShort(reply->buf.data + ia_cursor + 2,
		  reply->cursor - (ia_cursor + 4));

	/*
	 * yes, goto's aren't the best but we also want to avoid extra
	 * indents
	 */
	if (status == ISC_R_CANCELED)
		goto cleanup;

	/*
	 * If we have any addresses log what we are doing.
	 */
	if (reply->ia->num_iasubopt != 0) {
		struct iasubopt *tmp;
		int i;
		char tmp_addr[INET6_ADDRSTRLEN];

		for (i = 0 ; i < reply->ia->num_iasubopt ; i++) {
			tmp = reply->ia->iasubopt[i];

			log_info("%s TA: address %s to client with duid %s "
				 "iaid = %d valid for %d seconds",
				 dhcpv6_type_names[reply->buf.reply.msg_type],
				 inet_ntop(AF_INET6, &tmp->addr,
					   tmp_addr, sizeof(tmp_addr)),
				 print_hex_1(reply->client_id.len,
					     reply->client_id.data, 60),
				 iaid,
				 tmp->valid);
		}
	}

	/*
	 * For hard bindings we consume the new changes into
	 * the database (if any have been attached to the ia_ta).
	 *
	 * Loop through the assigned dynamic addresses, referencing the
	 * leases onto this IA_TA rather than any old ones, and updating
	 * pool timers for each (if any).
	 */
	if ((reply->ia->num_iasubopt != 0) &&
	    (reply->buf.reply.msg_type == DHCPV6_REPLY)) {
		struct iasubopt *tmp;
		struct data_string *ia_id;
		int i;

		for (i = 0 ; i < reply->ia->num_iasubopt ; i++) {
			tmp = reply->ia->iasubopt[i];

			if (tmp->ia != NULL)
				ia_dereference(&tmp->ia, MDL);
			ia_reference(&tmp->ia, reply->ia, MDL);

			/* Commit 'hard' bindings. */
			renew_lease6(tmp->ipv6_pool, tmp);
			schedule_lease_timeout(tmp->ipv6_pool);

			/* If we have anything to do on commit do it now */
			if (tmp->on_star.on_commit != NULL) {
				execute_statements(NULL, reply->packet,
						   NULL, NULL, 
						   reply->packet->options,
						   reply->opt_state,
						   &tmp->scope,
						   tmp->on_star.on_commit,
						   &tmp->on_star);
				executable_statement_dereference
					(&tmp->on_star.on_commit, MDL);
			}

#if defined (NSUPDATE)
			/*
			 * Perform ddns updates.
			 */
			oc = lookup_option(&server_universe, reply->opt_state,
					   SV_DDNS_UPDATES);
			if ((oc == NULL) ||
			    evaluate_boolean_option_cache(NULL, reply->packet,
							  NULL, NULL,
							reply->packet->options,
							  reply->opt_state,
							  &tmp->scope,
							  oc, MDL)) {
				ddns_updates(reply->packet, NULL, NULL,
					     tmp, NULL, reply->opt_state);
			}
#endif
		}

		/* Remove any old ia from the hash. */
		if (reply->old_ia != NULL) {
			ia_id = &reply->old_ia->iaid_duid;
			ia_hash_delete(ia_ta_active,
				       (unsigned char *)ia_id->data,
				       ia_id->len, MDL);
			ia_dereference(&reply->old_ia, MDL);
		}

		/* Put new ia into the hash. */
		reply->ia->cltt = cur_time;
		ia_id = &reply->ia->iaid_duid;
		ia_hash_add(ia_ta_active, (unsigned char *)ia_id->data,
			    ia_id->len, reply->ia, MDL);

		write_ia(reply->ia);
	}

      cleanup:
	if (packet_ia != NULL)
		option_state_dereference(&packet_ia, MDL);
	if (iaaddr.data != NULL)
		data_string_forget(&iaaddr, MDL);
	if (reply->reply_ia != NULL)
		option_state_dereference(&reply->reply_ia, MDL);
	if (ia_data.data != NULL)
		data_string_forget(&ia_data, MDL);
	if (data.data != NULL)
		data_string_forget(&data, MDL);
	if (reply->ia != NULL)
		ia_dereference(&reply->ia, MDL);
	if (reply->old_ia != NULL)
		ia_dereference(&reply->old_ia, MDL);
	if (reply->lease != NULL)
		iasubopt_dereference(&reply->lease, MDL);

	/*
	 * ISC_R_CANCELED is a status code used by the addr processing to
	 * indicate we're replying with other addresses.  This is still a
	 * success at higher layers.
	 */
	return((status == ISC_R_CANCELED) ? ISC_R_SUCCESS : status);
}

/*
 * Verify the temporary address is available.
 */
static isc_boolean_t
temporary_is_available(struct reply_state *reply, struct iaddr *addr) {
	struct in6_addr tmp_addr;
	struct subnet *subnet;
	struct ipv6_pool *pool = NULL;
	struct ipv6_pond *pond = NULL;
	int i;

	memcpy(&tmp_addr, addr->iabuf, sizeof(tmp_addr));
	/*
	 * Clients may choose to send :: as an address, with the idea to give
	 * hints about preferred-lifetime or valid-lifetime.
	 * So this is not a request for this address.
	 */
	if (IN6_IS_ADDR_UNSPECIFIED(&tmp_addr))
		return ISC_FALSE;

	/*
	 * Verify that this address is on the client's network.
	 */
	for (subnet = reply->shared->subnets ; subnet != NULL ;
	     subnet = subnet->next_sibling) {
		if (addr_eq(subnet_number(*addr, subnet->netmask),
			    subnet->net))
			break;
	}

	/* Address not found on shared network. */
	if (subnet == NULL)
		return ISC_FALSE;

	/*
	 * Check if this address is owned (must be before next step).
	 */
	if (address_is_owned(reply, addr))
		return ISC_TRUE;

	/*
	 * Verify that this address is in a temporary pool and try to get it.
	 */
	for (pond = reply->shared->ipv6_pond; pond != NULL; pond = pond->next) {
		if (((pond->prohibit_list != NULL) &&
		     (permitted(reply->packet, pond->prohibit_list))) ||
		    ((pond->permit_list != NULL) &&
		     (!permitted(reply->packet, pond->permit_list))))
			continue;

		for (i = 0 ; (pool = pond->ipv6_pools[i]) != NULL ; i++) {
			if (pool->pool_type != D6O_IA_TA)
				continue;

			if (ipv6_in_pool(&tmp_addr, pool))
				break;
		}

		if (pool != NULL)
			break;
	}

	if (pool == NULL)
		return ISC_FALSE;
	if (lease6_exists(pool, &tmp_addr))
		return ISC_FALSE;
	if (iasubopt_allocate(&reply->lease, MDL) != ISC_R_SUCCESS)
		return ISC_FALSE;
	reply->lease->addr = tmp_addr;
	reply->lease->plen = 0;
	/* Default is soft binding for 2 minutes. */
	if (add_lease6(pool, reply->lease, cur_time + 120) != ISC_R_SUCCESS)
		return ISC_FALSE;

	return ISC_TRUE;
}

/*
 * Get a temporary address per prefix.
 */
static isc_result_t
find_client_temporaries(struct reply_state *reply) {
	int i;
	struct ipv6_pool *p = NULL;
	struct ipv6_pond *pond;
	isc_result_t status = ISC_R_NORESOURCES;;
	unsigned int attempts;
	struct iaddr send_addr;

	/*
	 * Do a quick walk through of the ponds and pools
	 * to see if we have any prefix pools
	 */
	for (pond = reply->shared->ipv6_pond; pond != NULL; pond = pond->next) {
		if (pond->ipv6_pools == NULL)
			continue;

		for (i = 0; (p = pond->ipv6_pools[i]) != NULL; i++) {
			if (p->pool_type == D6O_IA_TA)
				break;
		}
		if (p != NULL)
			break;
	}

	/* If we get here and p is NULL we have no useful pools */
	if (p == NULL) {
		log_debug("Unable to get client addresses: "
			  "no IPv6 pools on this shared network");
		return ISC_R_NORESOURCES;
	}

	/*
	 * We have at least one pool that could provide an address
	 * Now we walk through the ponds and pools again and check
	 * to see if the client is permitted and if an address is
	 * available
	 */

	for (pond = reply->shared->ipv6_pond; pond != NULL; pond = pond->next) {
		if (((pond->prohibit_list != NULL) &&
		     (permitted(reply->packet, pond->prohibit_list))) ||
		    ((pond->permit_list != NULL) &&
		     (!permitted(reply->packet, pond->permit_list))))
			continue;

		for (i = 0; (p = pond->ipv6_pools[i]) != NULL; i++) {
			if (p->pool_type != D6O_IA_TA) {
				continue;
			}

			/*
			 * Get an address in this temporary pool.
			 */
			status = create_lease6(p, &reply->lease, &attempts,
					       &reply->client_id, cur_time + 120);
			if (status != ISC_R_SUCCESS) {
				log_debug("Unable to get a temporary address.");
				goto cleanup;
			}

			status = reply_process_is_addressed(reply,
							    &reply->lease->scope,
							    pond->group);
			if (status != ISC_R_SUCCESS) {
				goto cleanup;
			}
			send_addr.len = 16;
			memcpy(send_addr.iabuf, &reply->lease->addr, 16);
			status = reply_process_send_addr(reply, &send_addr);
			if (status != ISC_R_SUCCESS) {
				goto cleanup;
			}
			/*
			 * reply->lease can't be null as we use it above
			 * add check if that changes
			 */
			iasubopt_dereference(&reply->lease, MDL);
		}
	}

      cleanup:
	if (reply->lease != NULL) {
		iasubopt_dereference(&reply->lease, MDL);
	}
	return status;
}

/*
 * This function only returns failure on 'hard' failures.  If it succeeds,
 * it will leave a lease structure behind.
 */
static isc_result_t
reply_process_try_addr(struct reply_state *reply, struct iaddr *addr) {
	isc_result_t status = ISC_R_ADDRNOTAVAIL;
	struct ipv6_pool *pool = NULL;
	struct ipv6_pond *pond = NULL;
	int i;
	struct data_string data_addr;

	if ((reply == NULL) || (reply->shared == NULL) ||
	    (addr == NULL) || (reply->lease != NULL))
		return (DHCP_R_INVALIDARG);

	/*
	 * Do a quick walk through of the ponds and pools
	 * to see if we have any NA address pools
	 */
	for (pond = reply->shared->ipv6_pond; pond != NULL; pond = pond->next) {
		if (pond->ipv6_pools == NULL)
			continue;

		for (i = 0; ; i++) {
			pool = pond->ipv6_pools[i];
			if ((pool == NULL) ||
			    (pool->pool_type == D6O_IA_NA))
				break;
		}
		if (pool != NULL)
			break;
	}

	/* If we get here and p is NULL we have no useful pools */
	if (pool == NULL) {
		return (ISC_R_ADDRNOTAVAIL);
	}

	memset(&data_addr, 0, sizeof(data_addr));
	data_addr.len = addr->len;
	data_addr.data = addr->iabuf;

	/*
	 * We have at least one pool that could provide an address
	 * Now we walk through the ponds and pools again and check
	 * to see if the client is permitted and if an address is
	 * available
	 * 
	 * Within a given pond we start looking at the last pool we
	 * allocated from, unless it had a collision trying to allocate
	 * an address. This will tend to move us into less-filled pools.
	 */

	for (pond = reply->shared->ipv6_pond; pond != NULL; pond = pond->next) {
		if (((pond->prohibit_list != NULL) &&
		     (permitted(reply->packet, pond->prohibit_list))) ||
		    ((pond->permit_list != NULL) &&
		     (!permitted(reply->packet, pond->permit_list))))
			continue;

		for (i = 0 ; (pool = pond->ipv6_pools[i]) != NULL ; i++) {
			if (pool->pool_type != D6O_IA_NA)
				continue;

			status = try_client_v6_address(&reply->lease, pool,
						       &data_addr);
			if (status == ISC_R_SUCCESS)
				break;
		}

		if (status == ISC_R_SUCCESS)
			break;
	}

	/* Note that this is just pedantry.  There is no allocation to free. */
	data_string_forget(&data_addr, MDL);
	/* Return just the most recent status... */
	return (status);
}

/* Look around for an address to give the client.  First, look through the
 * old IA for addresses we can extend.  Second, try to allocate a new address.
 * Finally, actually add that address into the current reply IA.
 */
static isc_result_t
find_client_address(struct reply_state *reply) {
	struct iaddr send_addr;
	isc_result_t status = ISC_R_NORESOURCES;
	struct iasubopt *lease, *best_lease = NULL;
	struct binding_scope **scope;
	struct group *group;
	int i;

	if (reply->static_lease) {
		if (reply->host == NULL)
			return DHCP_R_INVALIDARG;

		send_addr.len = 16;
		memcpy(send_addr.iabuf, reply->fixed.data, 16);

		scope = &global_scope;
		group = reply->subnet->group;
		goto send_addr;
	}

	if (reply->old_ia != NULL)  {
		for (i = 0 ; i < reply->old_ia->num_iasubopt ; i++) {
			struct shared_network *candidate_shared;
			struct ipv6_pond *pond;

			lease = reply->old_ia->iasubopt[i];
			candidate_shared = lease->ipv6_pool->shared_network;
			pond = lease->ipv6_pool->ipv6_pond;

			/*
			 * Look for the best lease on the client's shared
			 * network, that is still permitted
			 */

			if ((candidate_shared != reply->shared) ||
			    (lease6_usable(lease) != ISC_TRUE))
				continue;

			if (((pond->prohibit_list != NULL) &&
			     (permitted(reply->packet, pond->prohibit_list))) ||
			    ((pond->permit_list != NULL) &&
			     (!permitted(reply->packet, pond->permit_list))))
				continue;

			best_lease = lease_compare(lease, best_lease);
		}
	}

	/* Try to pick a new address if we didn't find one, or if we found an
	 * abandoned lease.
	 */
	if ((best_lease == NULL) || (best_lease->state == FTS_ABANDONED)) {
		status = pick_v6_address(reply);
	} else if (best_lease != NULL) {
		iasubopt_reference(&reply->lease, best_lease, MDL);
		status = ISC_R_SUCCESS;
	}

	/* Pick the abandoned lease as a last resort. */
	if ((status == ISC_R_NORESOURCES) && (best_lease != NULL)) {
		/* I don't see how this is supposed to be done right now. */
		log_error("Reclaiming abandoned addresses is not yet "
			  "supported.  Treating this as an out of space "
			  "condition.");
		/* iasubopt_reference(&reply->lease, best_lease, MDL); */
	}

	/* Give up now if we didn't find a lease. */
	if (status != ISC_R_SUCCESS)
		return status;

	if (reply->lease == NULL)
		log_fatal("Impossible condition at %s:%d.", MDL);

	/* Draw binding scopes from the lease's binding scope, and config
	 * from the lease's containing subnet and higher.  Note that it may
	 * be desirable to place the group attachment directly in the pool.
	 */
	scope = &reply->lease->scope;
	group = reply->lease->ipv6_pool->ipv6_pond->group;

	send_addr.len = 16;
	memcpy(send_addr.iabuf, &reply->lease->addr, 16);

      send_addr:
	status = reply_process_is_addressed(reply, scope, group);
	if (status != ISC_R_SUCCESS)
		return status;

	status = reply_process_send_addr(reply, &send_addr);
	return status;
}

/* Once an address is found for a client, perform several common functions;
 * Calculate and store valid and preferred lease times, draw client options
 * into the option state.
 */
static isc_result_t
reply_process_is_addressed(struct reply_state *reply,
			   struct binding_scope **scope, struct group *group)
{
	isc_result_t status = ISC_R_SUCCESS;
	struct data_string data;
	struct option_cache *oc;
	struct option_state *tmp_options = NULL;
	struct on_star *on_star;
	int i;

	/* Initialize values we will cleanup. */
	memset(&data, 0, sizeof(data));

	/*
	 * Find the proper on_star block to use.  We use the
	 * one in the lease if we have a lease or the one in
	 * the reply if we don't have a lease because this is
	 * a static instance
	 */
	if (reply->lease) {
		on_star = &reply->lease->on_star;
	} else {
		on_star = &reply->on_star;
	}

	/*
	 * Bring in the root configuration.  We only do this to bring
	 * in the on * statements, as we didn't have the lease available
	 * we did it the first time.
	 */
	option_state_allocate(&tmp_options, MDL);
	execute_statements_in_scope(NULL, reply->packet, NULL, NULL,
				    reply->packet->options, tmp_options,
				    &global_scope, root_group, NULL,
				    on_star);
	if (tmp_options != NULL) {
		option_state_dereference(&tmp_options, MDL);
	}

	/*
	 * Bring configured options into the root packet level cache - start
	 * with the lease's closest enclosing group (passed in by the caller
	 * as 'group').
	 */
	execute_statements_in_scope(NULL, reply->packet, NULL, NULL,
				    reply->packet->options, reply->opt_state,
				    scope, group, root_group, on_star);

	/* Execute statements from class scopes. */
	for (i = reply->packet->class_count; i > 0; i--) {
		execute_statements_in_scope(NULL, reply->packet, NULL, NULL,
					    reply->packet->options,
					    reply->opt_state, scope,
					    reply->packet->classes[i - 1]->group,
					    group, on_star);
	}

	/*
	 * If there is a host record, over-ride with values configured there,
	 * without re-evaluating configuration from the previously executed
	 * group or its common enclosers.
	 */
	if (reply->host != NULL)
		execute_statements_in_scope(NULL, reply->packet, NULL, NULL,
					    reply->packet->options,
					    reply->opt_state, scope,
					    reply->host->group, group,
					    on_star);

	/* Determine valid lifetime. */
	if (reply->client_valid == 0)
		reply->send_valid = DEFAULT_DEFAULT_LEASE_TIME;
	else
		reply->send_valid = reply->client_valid;

	oc = lookup_option(&server_universe, reply->opt_state,
			   SV_DEFAULT_LEASE_TIME);
	if (oc != NULL) {
		if (!evaluate_option_cache(&data, reply->packet, NULL, NULL,
					   reply->packet->options,
					   reply->opt_state,
					   scope, oc, MDL) ||
		    (data.len != 4)) {
			log_error("reply_process_is_addressed: unable to "
				  "evaluate default lease time");
			status = ISC_R_FAILURE;
			goto cleanup;
		}

		reply->send_valid = getULong(data.data);
		data_string_forget(&data, MDL);
	}

	if (reply->client_prefer == 0)
		reply->send_prefer = reply->send_valid;
	else
		reply->send_prefer = reply->client_prefer;

	if (reply->send_prefer >= reply->send_valid)
		reply->send_prefer = (reply->send_valid / 2) +
				     (reply->send_valid / 8);

	oc = lookup_option(&server_universe, reply->opt_state,
			   SV_PREFER_LIFETIME);
	if (oc != NULL) {
		if (!evaluate_option_cache(&data, reply->packet, NULL, NULL,
					   reply->packet->options,
					   reply->opt_state,
					   scope, oc, MDL) ||
		    (data.len != 4)) {
			log_error("reply_process_is_addressed: unable to "
				  "evaluate preferred lease time");
			status = ISC_R_FAILURE;
			goto cleanup;
		}

		reply->send_prefer = getULong(data.data);
		data_string_forget(&data, MDL);
	}

	/* Note lowest values for later calculation of renew/rebind times. */
	if (reply->prefer > reply->send_prefer)
		reply->prefer = reply->send_prefer;

	if (reply->valid > reply->send_valid)
		reply->valid = reply->send_valid;

#if 0
	/*
	 * XXX: Old 4.0.0 alpha code would change the host {} record
	 * XXX: uid upon lease assignment.  This was intended to cover the
	 * XXX: case where a client first identifies itself using vendor
	 * XXX: options in a solicit, or request, but later neglects to include
	 * XXX: these options in a Renew or Rebind.  It is not clear that this
	 * XXX: is required, and has some startling ramifications (such as
	 * XXX: how to recover this dynamic host {} state across restarts).
	 */
	if (reply->host != NULL)
		change_host_uid(host, reply->client_id->data,
				reply->client_id->len);
#endif /* 0 */

	/* Perform dynamic lease related update work. */
	if (reply->lease != NULL) {
		/* Cached lifetimes */
		reply->lease->prefer = reply->send_prefer;
		reply->lease->valid = reply->send_valid;

		/* Advance (or rewind) the valid lifetime. */
		if (reply->buf.reply.msg_type == DHCPV6_REPLY) {
			reply->lease->soft_lifetime_end_time =
				cur_time + reply->send_valid;
			/* Wait before renew! */
		}

		status = ia_add_iasubopt(reply->ia, reply->lease, MDL);
		if (status != ISC_R_SUCCESS) {
			log_fatal("reply_process_is_addressed: Unable to "
				  "attach lease to new IA: %s",
				  isc_result_totext(status));
		}

		/*
		 * If this is a new lease, make sure it is attached somewhere.
		 */
		if (reply->lease->ia == NULL) {
			ia_reference(&reply->lease->ia, reply->ia, MDL);
		}
	}

	/* Bring a copy of the relevant options into the IA scope. */
	execute_statements_in_scope(NULL, reply->packet, NULL, NULL,
				    reply->packet->options, reply->reply_ia,
				    scope, group, root_group, NULL);

	/* Execute statements from class scopes. */
	for (i = reply->packet->class_count; i > 0; i--) {
		execute_statements_in_scope(NULL, reply->packet, NULL, NULL,
					    reply->packet->options,
					    reply->reply_ia, scope,
					    reply->packet->classes[i - 1]->group,
					    group, NULL);
	}
	  
	/*
	 * And bring in host record configuration, if any, but not to overlap
	 * the previous group or its common enclosers.
	 */
	if (reply->host != NULL)
		execute_statements_in_scope(NULL, reply->packet, NULL, NULL,
					    reply->packet->options,
					    reply->reply_ia, scope,
					    reply->host->group, group, NULL);

      cleanup:
	if (data.data != NULL)
		data_string_forget(&data, MDL);

	if (status == ISC_R_SUCCESS)
		reply->client_resources++;

	return status;
}

/* Simply send an IAADDR within the IA scope as described. */
static isc_result_t
reply_process_send_addr(struct reply_state *reply, struct iaddr *addr) {
	isc_result_t status = ISC_R_SUCCESS;
	struct data_string data;

	memset(&data, 0, sizeof(data));

	/* Now append the lease. */
	data.len = IAADDR_OFFSET;
	if (!buffer_allocate(&data.buffer, data.len, MDL)) {
		log_error("reply_process_send_addr: out of memory"
			  "allocating new IAADDR buffer.");
		status = ISC_R_NOMEMORY;
		goto cleanup;
	}
	data.data = data.buffer->data;

	memcpy(data.buffer->data, addr->iabuf, 16);
	putULong(data.buffer->data + 16, reply->send_prefer);
	putULong(data.buffer->data + 20, reply->send_valid);

	if (!append_option_buffer(&dhcpv6_universe, reply->reply_ia,
				  data.buffer, data.buffer->data,
				  data.len, D6O_IAADDR, 0)) {
		log_error("reply_process_send_addr: unable "
			  "to save IAADDR option");
		status = ISC_R_FAILURE;
		goto cleanup;
	}

	reply->resources_included = ISC_TRUE;

      cleanup:
	if (data.data != NULL)
		data_string_forget(&data, MDL);

	return status;
}

/* Choose the better of two leases. */
static struct iasubopt *
lease_compare(struct iasubopt *alpha, struct iasubopt *beta) {
	if (alpha == NULL)
		return beta;
	if (beta == NULL)
		return alpha;

	switch(alpha->state) {
	      case FTS_ACTIVE:
		switch(beta->state) {
		      case FTS_ACTIVE:
			/* Choose the lease with the longest lifetime (most
			 * likely the most recently allocated).
			 */
			if (alpha->hard_lifetime_end_time < 
			    beta->hard_lifetime_end_time)
				return beta;
			else
				return alpha;

		      case FTS_EXPIRED:
		      case FTS_ABANDONED:
			return alpha;

		      default:
			log_fatal("Impossible condition at %s:%d.", MDL);
		}
		break;

	      case FTS_EXPIRED:
		switch (beta->state) {
		      case FTS_ACTIVE:
			return beta;

		      case FTS_EXPIRED:
			/* Choose the most recently expired lease. */
			if (alpha->hard_lifetime_end_time <
			    beta->hard_lifetime_end_time)
				return beta;
			else if ((alpha->hard_lifetime_end_time ==
				  beta->hard_lifetime_end_time) &&
				 (alpha->soft_lifetime_end_time <
				  beta->soft_lifetime_end_time))
				return beta;
			else
				return alpha;

		      case FTS_ABANDONED:
			return alpha;

		      default:
			log_fatal("Impossible condition at %s:%d.", MDL);
		}
		break;

	      case FTS_ABANDONED:
		switch (beta->state) {
		      case FTS_ACTIVE:
		      case FTS_EXPIRED:
			return alpha;

		      case FTS_ABANDONED:
			/* Choose the lease that was abandoned longest ago. */
			if (alpha->hard_lifetime_end_time <
			    beta->hard_lifetime_end_time)
				return alpha;

		      default:
			log_fatal("Impossible condition at %s:%d.", MDL);
		}
		break;

	      default:
		log_fatal("Impossible condition at %s:%d.", MDL);
	}

	log_fatal("Triple impossible condition at %s:%d.", MDL);
	return NULL;
}

/* Process a client-supplied IA_PD.  This may append options to the tail of
 * the reply packet being built in the reply_state structure.
 */
static isc_result_t
reply_process_ia_pd(struct reply_state *reply, struct option_cache *ia) {
	isc_result_t status = ISC_R_SUCCESS;
	u_int32_t iaid;
	unsigned ia_cursor;
	struct option_state *packet_ia;
	struct option_cache *oc;
	struct data_string ia_data, data;

	/* Initialize values that will get cleaned up on return. */
	packet_ia = NULL;
	memset(&ia_data, 0, sizeof(ia_data));
	memset(&data, 0, sizeof(data));
	/* 
	 * Note that find_client_prefix() may set reply->lease.
	 */

	/* Make sure there is at least room for the header. */
	if ((reply->cursor + IA_PD_OFFSET + 4) > sizeof(reply->buf)) {
		log_error("reply_process_ia_pd: Reply too long for IA.");
		return ISC_R_NOSPACE;
	}


	/* Fetch the IA_PD contents. */
	if (!get_encapsulated_IA_state(&packet_ia, &ia_data, reply->packet,
				       ia, IA_PD_OFFSET)) {
		log_error("reply_process_ia_pd: error evaluating ia");
		status = ISC_R_FAILURE;
		goto cleanup;
	}

	/* Extract IA_PD header contents. */
	iaid = getULong(ia_data.data);
	reply->renew = getULong(ia_data.data + 4);
	reply->rebind = getULong(ia_data.data + 8);

	/* Create an IA_PD structure. */
	if (ia_allocate(&reply->ia, iaid, (char *)reply->client_id.data, 
			reply->client_id.len, MDL) != ISC_R_SUCCESS) {
		log_error("reply_process_ia_pd: no memory for ia.");
		status = ISC_R_NOMEMORY;
		goto cleanup;
	}
	reply->ia->ia_type = D6O_IA_PD;

	/* Cache pre-existing IA_PD, if any. */
	ia_hash_lookup(&reply->old_ia, ia_pd_active,
		       (unsigned char *)reply->ia->iaid_duid.data,
		       reply->ia->iaid_duid.len, MDL);

	/*
	 * Create an option cache to carry the IA_PD option contents, and
	 * execute any user-supplied values into it.
	 */
	if (!option_state_allocate(&reply->reply_ia, MDL)) {
		status = ISC_R_NOMEMORY;
		goto cleanup;
	}

	/* Check & count the fixed prefix host records. */
	reply->static_prefixes = 0;
	if ((reply->host != NULL) && (reply->host->fixed_prefix != NULL)) {
		struct iaddrcidrnetlist *fp;

		for (fp = reply->host->fixed_prefix; fp != NULL;
		     fp = fp->next) {
			reply->static_prefixes += 1;
		}
	}

	/*
	 * Save the cursor position at the start of the IA_PD, so we can
	 * set length and adjust t1/t2 values later.  We write a temporary
	 * header out now just in case we decide to adjust the packet
	 * within sub-process functions.
	 */
	ia_cursor = reply->cursor;

	/* Initialize the IA_PD header.  First the code. */
	putUShort(reply->buf.data + reply->cursor, (unsigned)D6O_IA_PD);
	reply->cursor += 2;

	/* Then option length. */
	putUShort(reply->buf.data + reply->cursor, 0x0Cu);
	reply->cursor += 2;

	/* Then IA_PD header contents; IAID. */
	putULong(reply->buf.data + reply->cursor, iaid);
	reply->cursor += 4;

	/* We store the client's t1 for now, and may over-ride it later. */
	putULong(reply->buf.data + reply->cursor, reply->renew);
	reply->cursor += 4;

	/* We store the client's t2 for now, and may over-ride it later. */
	putULong(reply->buf.data + reply->cursor, reply->rebind);
	reply->cursor += 4;

	/* 
	 * For each prefix in this IA_PD, decide what to do about it.
	 */
	oc = lookup_option(&dhcpv6_universe, packet_ia, D6O_IAPREFIX);
	reply->valid = reply->prefer = 0xffffffff;
	reply->client_valid = reply->client_prefer = 0;
	reply->preflen = -1;
	for (; oc != NULL ; oc = oc->next) {
		status = reply_process_prefix(reply, oc);

		/*
		 * Canceled means we did not allocate prefixes to the
		 * client, but we're "done" with this IA - we set a status
		 * code.  So transmit this reply, e.g., move on to the next
		 * IA.
		 */
		if (status == ISC_R_CANCELED)
			break;

		if ((status != ISC_R_SUCCESS) &&
		    (status != ISC_R_ADDRINUSE) &&
		    (status != ISC_R_ADDRNOTAVAIL))
			goto cleanup;
	}

	reply->pd_count++;

	/*
	 * If we fell through the above and never gave the client
	 * a prefix, give it one now.
	 */
	if ((status != ISC_R_CANCELED) && (reply->client_resources == 0)) {
		status = find_client_prefix(reply);

		if (status == ISC_R_NORESOURCES) {
			switch (reply->packet->dhcpv6_msg_type) {
			      case DHCPV6_SOLICIT:
				/*
				 * No prefix for any IA is handled
				 * by the caller.
				 */
				/* FALL THROUGH */

			      case DHCPV6_REQUEST:
				/* Same than for addresses. */
				option_state_dereference(&reply->reply_ia, MDL);
				if (!option_state_allocate(&reply->reply_ia,
							   MDL))
				{
					log_error("reply_process_ia_pd: No "
						  "memory for option state "
						  "wipe.");
					status = ISC_R_NOMEMORY;
					goto cleanup;
				}

				if (!set_status_code(STATUS_NoPrefixAvail,
						     "No prefixes available "
						     "for this interface.",
						      reply->reply_ia)) {
					log_error("reply_process_ia_pd: "
						  "Unable to set "
						  "NoPrefixAvail status "
						  "code.");
					status = ISC_R_FAILURE;
					goto cleanup;
				}

				status = ISC_R_SUCCESS;
				break;

			      default:
				if (reply->resources_included)
					status = ISC_R_SUCCESS;
				else
					goto cleanup;
				break;
			}
		}

		if (status != ISC_R_SUCCESS)
			goto cleanup;
	}

	reply->cursor += store_options6((char *)reply->buf.data + reply->cursor,
					sizeof(reply->buf) - reply->cursor,
					reply->reply_ia, reply->packet,
					required_opts_IA_PD, NULL);

	/* Reset the length of this IA_PD to match what was just written. */
	putUShort(reply->buf.data + ia_cursor + 2,
		  reply->cursor - (ia_cursor + 4));

	/*
	 * T1/T2 time selection is kind of weird.  We actually use DHCP
	 * (v4) scoped options as handy existing places where these might
	 * be configured by an administrator.  A value of zero tells the
	 * client it may choose its own renewal time.
	 */
	reply->renew = 0;
	oc = lookup_option(&dhcp_universe, reply->opt_state,
			   DHO_DHCP_RENEWAL_TIME);
	if (oc != NULL) {
		if (!evaluate_option_cache(&data, reply->packet, NULL, NULL,
					   reply->packet->options,
					   reply->opt_state, &global_scope,
					   oc, MDL) ||
		    (data.len != 4)) {
			log_error("Invalid renewal time.");
		} else {
			reply->renew = getULong(data.data);
		}

		if (data.data != NULL)
			data_string_forget(&data, MDL);
	}
	putULong(reply->buf.data + ia_cursor + 8, reply->renew);

	/* Now T2. */
	reply->rebind = 0;
	oc = lookup_option(&dhcp_universe, reply->opt_state,
			   DHO_DHCP_REBINDING_TIME);
	if (oc != NULL) {
		if (!evaluate_option_cache(&data, reply->packet, NULL, NULL,
					   reply->packet->options,
					   reply->opt_state, &global_scope,
					   oc, MDL) ||
		    (data.len != 4)) {
			log_error("Invalid rebinding time.");
		} else {
			reply->rebind = getULong(data.data);
		}

		if (data.data != NULL)
			data_string_forget(&data, MDL);
	}
	putULong(reply->buf.data + ia_cursor + 12, reply->rebind);

	/*
	 * yes, goto's aren't the best but we also want to avoid extra
	 * indents
	 */
	if (status == ISC_R_CANCELED)
		goto cleanup;

	/*
	 * Handle static prefixes, we always log stuff and if it's
	 * a hard binding we run any commit statements that we have
	 */
	if (reply->static_prefixes != 0) {
		char tmp_addr[INET6_ADDRSTRLEN];
		log_info("%s PD: address %s/%d to client with duid %s "
			 "iaid = %d static",
			 dhcpv6_type_names[reply->buf.reply.msg_type],
			 inet_ntop(AF_INET6, reply->fixed_pref.lo_addr.iabuf,
				   tmp_addr, sizeof(tmp_addr)),
			 reply->fixed_pref.bits,
			 print_hex_1(reply->client_id.len,
				     reply->client_id.data, 60),
			 iaid);
		if ((reply->buf.reply.msg_type == DHCPV6_REPLY) &&
		    (reply->on_star.on_commit != NULL)) {
			execute_statements(NULL, reply->packet, NULL, NULL,
					   reply->packet->options,
					   reply->opt_state,
					   NULL, reply->on_star.on_commit,
					   NULL);
			executable_statement_dereference
				(&reply->on_star.on_commit, MDL);
		}
		goto cleanup;
	}

	/*
	 * If we have any addresses log what we are doing.
	 */
	if (reply->ia->num_iasubopt != 0) {
		struct iasubopt *tmp;
		int i;
		char tmp_addr[INET6_ADDRSTRLEN];

		for (i = 0 ; i < reply->ia->num_iasubopt ; i++) {
			tmp = reply->ia->iasubopt[i];

			log_info("%s PD: address %s/%d to client with duid %s"
				 " iaid = %d valid for %d seconds",
				 dhcpv6_type_names[reply->buf.reply.msg_type],
				 inet_ntop(AF_INET6, &tmp->addr,
					   tmp_addr, sizeof(tmp_addr)),
				 (int)tmp->plen,
				 print_hex_1(reply->client_id.len,
					     reply->client_id.data, 60),
				 iaid, tmp->valid);
		}
	}

	/*
	 * If this is not a 'soft' binding, consume the new changes into
	 * the database (if any have been attached to the ia_pd).
	 *
	 * Loop through the assigned dynamic prefixes, referencing the
	 * prefixes onto this IA_PD rather than any old ones, and updating
	 * prefix pool timers for each (if any).
	 */
	if ((reply->buf.reply.msg_type == DHCPV6_REPLY) &&
	    (reply->ia->num_iasubopt != 0)) {
		struct iasubopt *tmp;
		struct data_string *ia_id;
		int i;

		for (i = 0 ; i < reply->ia->num_iasubopt ; i++) {
			tmp = reply->ia->iasubopt[i];

			if (tmp->ia != NULL)
				ia_dereference(&tmp->ia, MDL);
			ia_reference(&tmp->ia, reply->ia, MDL);

			/* Commit 'hard' bindings. */
			renew_lease6(tmp->ipv6_pool, tmp);
			schedule_lease_timeout(tmp->ipv6_pool);

			/* If we have anything to do on commit do it now */
			if (tmp->on_star.on_commit != NULL) {
				execute_statements(NULL, reply->packet,
						   NULL, NULL, 
						   reply->packet->options,
						   reply->opt_state,
						   &tmp->scope,
						   tmp->on_star.on_commit,
						   &tmp->on_star);
				executable_statement_dereference
					(&tmp->on_star.on_commit, MDL);
			}
		}

		/* Remove any old ia from the hash. */
		if (reply->old_ia != NULL) {
			ia_id = &reply->old_ia->iaid_duid;
			ia_hash_delete(ia_pd_active,
				       (unsigned char *)ia_id->data,
				       ia_id->len, MDL);
			ia_dereference(&reply->old_ia, MDL);
		}

		/* Put new ia into the hash. */
		reply->ia->cltt = cur_time;
		ia_id = &reply->ia->iaid_duid;
		ia_hash_add(ia_pd_active, (unsigned char *)ia_id->data,
			    ia_id->len, reply->ia, MDL);

		write_ia(reply->ia);
	}

      cleanup:
	if (packet_ia != NULL)
		option_state_dereference(&packet_ia, MDL);
	if (reply->reply_ia != NULL)
		option_state_dereference(&reply->reply_ia, MDL);
	if (ia_data.data != NULL)
		data_string_forget(&ia_data, MDL);
	if (data.data != NULL)
		data_string_forget(&data, MDL);
	if (reply->ia != NULL)
		ia_dereference(&reply->ia, MDL);
	if (reply->old_ia != NULL)
		ia_dereference(&reply->old_ia, MDL);
	if (reply->lease != NULL)
		iasubopt_dereference(&reply->lease, MDL);
	if (reply->on_star.on_expiry != NULL)
		executable_statement_dereference
			(&reply->on_star.on_expiry, MDL);
	if (reply->on_star.on_release != NULL)
		executable_statement_dereference
			(&reply->on_star.on_release, MDL);

	/*
	 * ISC_R_CANCELED is a status code used by the prefix processing to
	 * indicate we're replying with a status code.  This is still a
	 * success at higher layers.
	 */
	return((status == ISC_R_CANCELED) ? ISC_R_SUCCESS : status);
}

/*
 * Process an IAPREFIX within a given IA_PD, storing any IAPREFIX reply
 * contents into the reply's current ia_pd-scoped option cache.  Returns
 * ISC_R_CANCELED in the event we are replying with a status code and do
 * not wish to process more IAPREFIXes within this IA_PD.
 */
static isc_result_t
reply_process_prefix(struct reply_state *reply, struct option_cache *pref) {
	u_int32_t pref_life, valid_life;
	struct binding_scope **scope;
	struct iaddrcidrnet tmp_pref;
	struct option_cache *oc;
	struct data_string iapref, data;
	isc_result_t status = ISC_R_SUCCESS;
	struct group *group;

	/* Initializes values that will be cleaned up. */
	memset(&iapref, 0, sizeof(iapref));
	memset(&data, 0, sizeof(data));
	/* Note that reply->lease may be set by prefix_is_owned() */

	/*
	 * There is no point trying to process an incoming prefix if there
	 * is no room for an outgoing prefix.
	 */
	if ((reply->cursor + 29) > sizeof(reply->buf)) {
		log_error("reply_process_prefix: Out of room for prefix.");
		return ISC_R_NOSPACE;
	}

	/* Extract this IAPREFIX option. */
	if (!evaluate_option_cache(&iapref, reply->packet, NULL, NULL, 
				   reply->packet->options, NULL, &global_scope,
				   pref, MDL) ||
	    (iapref.len < IAPREFIX_OFFSET)) {
		log_error("reply_process_prefix: error evaluating IAPREFIX.");
		status = ISC_R_FAILURE;
		goto cleanup;
	}

	/*
	 * Layout: preferred and valid lifetimes followed by the prefix
	 * length and the IPv6 address.
	 */
	pref_life = getULong(iapref.data);
	valid_life = getULong(iapref.data + 4);

	if ((reply->client_valid == 0) ||
	    (reply->client_valid > valid_life))
		reply->client_valid = valid_life;

	if ((reply->client_prefer == 0) ||
	    (reply->client_prefer > pref_life))
		reply->client_prefer = pref_life;

	/* 
	 * Clients may choose to send ::/0 as a prefix, with the idea to give
	 * hints about preferred-lifetime or valid-lifetime.
	 */
	tmp_pref.lo_addr.len = 16;
	memset(tmp_pref.lo_addr.iabuf, 0, 16);
	if ((iapref.data[8] == 0) &&
	    (memcmp(iapref.data + 9, tmp_pref.lo_addr.iabuf, 16) == 0)) {
		/* Status remains success; we just ignore this one. */
		goto cleanup;
	}

	/*
	 * Clients may choose to send ::/X as a prefix to specify a
	 * preferred/requested prefix length. Note X is never zero here.
	 */
	tmp_pref.bits = (int) iapref.data[8];
	if (reply->preflen < 0) {
		/* Cache the first preferred prefix length. */
		reply->preflen = tmp_pref.bits;
	}
	if (memcmp(iapref.data + 9, tmp_pref.lo_addr.iabuf, 16) == 0) {
		goto cleanup;
	}

	memcpy(tmp_pref.lo_addr.iabuf, iapref.data + 9, 16);

	/* Verify the prefix belongs to the client. */
	if (!prefix_is_owned(reply, &tmp_pref)) {
		/* Same than for addresses. */
		if ((reply->packet->dhcpv6_msg_type == DHCPV6_SOLICIT) ||
		    (reply->packet->dhcpv6_msg_type == DHCPV6_REQUEST) ||
		    (reply->packet->dhcpv6_msg_type == DHCPV6_REBIND)) {
			status = reply_process_try_prefix(reply, &tmp_pref);

			/* Either error out or skip this prefix. */
			if ((status != ISC_R_SUCCESS) &&
			    (status != ISC_R_ADDRINUSE) &&
			    (status != ISC_R_ADDRNOTAVAIL))
				goto cleanup;

			if (reply->lease == NULL) {
				if (reply->packet->dhcpv6_msg_type ==
							DHCPV6_REBIND) {
					reply->send_prefer = 0;
					reply->send_valid = 0;
					goto send_pref;
				}

				/* status remains success - ignore */
				goto cleanup;
			}
		/*
		 * RFC3633 section 18.2.3:
		 *
		 * If the delegating router cannot find a binding
		 * for the requesting router's IA_PD the delegating
		 * router returns the IA_PD containing no prefixes
		 * with a Status Code option set to NoBinding in the
		 * Reply message.
		 *
		 * On mismatch we (ab)use this pretending we have not the IA
		 * as soon as we have not a prefix.
		 */
		} else if (reply->packet->dhcpv6_msg_type == DHCPV6_RENEW) {
			/* Rewind the IA_PD to empty. */
			option_state_dereference(&reply->reply_ia, MDL);
			if (!option_state_allocate(&reply->reply_ia, MDL)) {
				log_error("reply_process_prefix: No memory "
					  "for option state wipe.");
				status = ISC_R_NOMEMORY;
				goto cleanup;
			}

			/* Append a NoBinding status code.  */
			if (!set_status_code(STATUS_NoBinding,
					     "Prefix not bound to this "
					     "interface.", reply->reply_ia)) {
				log_error("reply_process_prefix: Unable to "
					  "attach status code.");
				status = ISC_R_FAILURE;
				goto cleanup;
			}

			/* Fin (no more IAPREFIXes). */
			status = ISC_R_CANCELED;
			goto cleanup;
		} else {
			log_error("It is impossible to lease a client that is "
				  "not sending a solicit, request, renew, or "
				  "rebind message.");
			status = ISC_R_FAILURE;
			goto cleanup;
		}
	}

	if (reply->static_prefixes > 0) {
		if (reply->host == NULL)
			log_fatal("Impossible condition at %s:%d.", MDL);

		scope = &global_scope;

		/* Find the static prefixe's subnet. */
		if (find_grouped_subnet(&reply->subnet, reply->shared,
					tmp_pref.lo_addr, MDL) == 0)
			log_fatal("Impossible condition at %s:%d.", MDL);
		group = reply->subnet->group;
		subnet_dereference(&reply->subnet, MDL);

		/* Copy the static prefix for logging purposes */
		memcpy(&reply->fixed_pref, &tmp_pref, sizeof(tmp_pref));
	} else {
		if (reply->lease == NULL)
			log_fatal("Impossible condition at %s:%d.", MDL);

		scope = &reply->lease->scope;
		group = reply->lease->ipv6_pool->ipv6_pond->group;
	}

	/*
	 * If client_resources is nonzero, then the reply_process_is_prefixed
	 * function has executed configuration state into the reply option
	 * cache.  We will use that valid cache to derive configuration for
	 * whether or not to engage in additional prefixes, and similar.
	 */
	if (reply->client_resources != 0) {
		unsigned limit = 1;

		/*
		 * Does this client have "enough" prefixes already?  Default
		 * to one.  Everybody gets one, and one should be enough for
		 * anybody.
		 */
		oc = lookup_option(&server_universe, reply->opt_state,
				   SV_LIMIT_PREFS_PER_IA);
		if (oc != NULL) {
			if (!evaluate_option_cache(&data, reply->packet,
						   NULL, NULL,
						   reply->packet->options,
						   reply->opt_state,
						   scope, oc, MDL) ||
			    (data.len != 4)) {
				log_error("reply_process_prefix: unable to "
					  "evaluate prefs-per-ia value.");
				status = ISC_R_FAILURE;
				goto cleanup;
			}

			limit = getULong(data.data);
			data_string_forget(&data, MDL);
		}

		/*
		 * If we wish to limit the client to a certain number of
		 * prefixes, then omit the prefix from the reply.
		 */
		if (reply->client_resources >= limit)
			goto cleanup;
	}

	status = reply_process_is_prefixed(reply, scope, group);
	if (status != ISC_R_SUCCESS)
		goto cleanup;

      send_pref:
	status = reply_process_send_prefix(reply, &tmp_pref);

      cleanup:
	if (iapref.data != NULL)
		data_string_forget(&iapref, MDL);
	if (data.data != NULL)
		data_string_forget(&data, MDL);
	if (reply->lease != NULL)
		iasubopt_dereference(&reply->lease, MDL);

	return status;
}

/*
 * Verify the prefix belongs to the client.  If we've got a host
 * record with fixed prefixes, it has to be an assigned prefix
 * (fault out all else).  Otherwise it's a dynamic prefix, so lookup
 * that prefix and make sure it belongs to this DUID:IAID pair.
 */
static isc_boolean_t
prefix_is_owned(struct reply_state *reply, struct iaddrcidrnet *pref) {
	struct iaddrcidrnetlist *l;
	int i;
	struct ipv6_pond *pond;

	/*
	 * This faults out prefixes that don't match fixed prefixes.
	 */
	if (reply->static_prefixes > 0) {
		for (l = reply->host->fixed_prefix; l != NULL; l = l->next) {
			if ((pref->bits == l->cidrnet.bits) &&
			    (memcmp(pref->lo_addr.iabuf,
				    l->cidrnet.lo_addr.iabuf, 16) == 0))
				return (ISC_TRUE);
		}
		return (ISC_FALSE);
	}

	if ((reply->old_ia == NULL) ||
	    (reply->old_ia->num_iasubopt == 0))
		return (ISC_FALSE);

	for (i = 0 ; i < reply->old_ia->num_iasubopt ; i++) {
		struct iasubopt *tmp;

		tmp = reply->old_ia->iasubopt[i];

		if ((pref->bits == (int) tmp->plen) &&
		    (memcmp(pref->lo_addr.iabuf, &tmp->addr, 16) == 0)) {
			if (lease6_usable(tmp) == ISC_FALSE) {
				return (ISC_FALSE);
			}

			pond = tmp->ipv6_pool->ipv6_pond;
			if (((pond->prohibit_list != NULL) &&
			     (permitted(reply->packet, pond->prohibit_list))) ||
			    ((pond->permit_list != NULL) &&
			     (!permitted(reply->packet, pond->permit_list))))
				return (ISC_FALSE);

			iasubopt_reference(&reply->lease, tmp, MDL);
			return (ISC_TRUE);
		}
	}

	return (ISC_FALSE);
}

/*
 * This function only returns failure on 'hard' failures.  If it succeeds,
 * it will leave a prefix structure behind.
 */
static isc_result_t
reply_process_try_prefix(struct reply_state *reply,
			 struct iaddrcidrnet *pref) {
	isc_result_t status = ISC_R_ADDRNOTAVAIL;
	struct ipv6_pool *pool = NULL;
	struct ipv6_pond *pond = NULL;
	int i;
	struct data_string data_pref;

	if ((reply == NULL) || (reply->shared == NULL) ||
	    (pref == NULL) || (reply->lease != NULL))
		return (DHCP_R_INVALIDARG);

	/*
	 * Do a quick walk through of the ponds and pools
	 * to see if we have any prefix pools
	 */
	for (pond = reply->shared->ipv6_pond; pond != NULL; pond = pond->next) {
		if (pond->ipv6_pools == NULL)
			continue;

		for (i = 0; (pool = pond->ipv6_pools[i]) != NULL; i++) {
			if (pool->pool_type == D6O_IA_PD)
				break;
		}
		if (pool != NULL)
			break;
	}

	/* If we get here and p is NULL we have no useful pools */
	if (pool == NULL) {
		return (ISC_R_ADDRNOTAVAIL);
	}

	memset(&data_pref, 0, sizeof(data_pref));
	data_pref.len = 17;
	if (!buffer_allocate(&data_pref.buffer, data_pref.len, MDL)) {
		log_error("reply_process_try_prefix: out of memory.");
		return (ISC_R_NOMEMORY);
	}
	data_pref.data = data_pref.buffer->data;
	data_pref.buffer->data[0] = (u_int8_t) pref->bits;
	memcpy(data_pref.buffer->data + 1, pref->lo_addr.iabuf, 16);

	/*
	 * We have at least one pool that could provide a prefix
	 * Now we walk through the ponds and pools again and check
	 * to see if the client is permitted and if an prefix is
	 * available
	 * 
	 */

	for (pond = reply->shared->ipv6_pond; pond != NULL; pond = pond->next) {
		if (((pond->prohibit_list != NULL) &&
		     (permitted(reply->packet, pond->prohibit_list))) ||
		    ((pond->permit_list != NULL) &&
		     (!permitted(reply->packet, pond->permit_list))))
			continue;

		for (i = 0; (pool = pond->ipv6_pools[i]) != NULL; i++) {
			if (pool->pool_type != D6O_IA_PD) {
				continue;
			}

			status = try_client_v6_prefix(&reply->lease, pool,
						      &data_pref);
			/* If we found it in this pool (either in use or available), 
			   there is no need to look further. */
			if ( (status == ISC_R_SUCCESS) || (status == ISC_R_ADDRINUSE) )
				break;
			}
		if ( (status == ISC_R_SUCCESS) || (status == ISC_R_ADDRINUSE) )
			break;
	}

	data_string_forget(&data_pref, MDL);
	/* Return just the most recent status... */
	return (status);
}

/* Look around for a prefix to give the client.  First, look through the old
 * IA_PD for prefixes we can extend.  Second, try to allocate a new prefix.
 * Finally, actually add that prefix into the current reply IA_PD.
 */
static isc_result_t
find_client_prefix(struct reply_state *reply) {
	struct iaddrcidrnet send_pref;
	isc_result_t status = ISC_R_NORESOURCES;
	struct iasubopt *prefix, *best_prefix = NULL;
	struct binding_scope **scope;
	int i;
	struct group *group;

	if (reply->static_prefixes > 0) {
		struct iaddrcidrnetlist *l;

		if (reply->host == NULL)
			return DHCP_R_INVALIDARG;

		for (l = reply->host->fixed_prefix; l != NULL; l = l->next) {
			if (l->cidrnet.bits == reply->preflen)
				break;
		}
		if (l == NULL) {
			/*
			 * If no fixed prefix has the preferred length,
			 * get the first one.
			 */
			l = reply->host->fixed_prefix;
		}
		memcpy(&send_pref, &l->cidrnet, sizeof(send_pref));

		scope = &global_scope;

		/* Find the static prefixe's subnet. */
		if (find_grouped_subnet(&reply->subnet, reply->shared,
					send_pref.lo_addr, MDL) == 0)
			log_fatal("Impossible condition at %s:%d.", MDL);
		group = reply->subnet->group;
		subnet_dereference(&reply->subnet, MDL);

		/* Copy the prefix for logging purposes */
		memcpy(&reply->fixed_pref, &l->cidrnet, sizeof(send_pref));

		goto send_pref;
	}

	if (reply->old_ia != NULL)  {
		for (i = 0 ; i < reply->old_ia->num_iasubopt ; i++) {
			struct shared_network *candidate_shared;
			struct ipv6_pond *pond;

			prefix = reply->old_ia->iasubopt[i];
			candidate_shared = prefix->ipv6_pool->shared_network;
			pond = prefix->ipv6_pool->ipv6_pond;

			/*
			 * Consider this prefix if it is in a global pool or
			 * if it is scoped in a pool under the client's shared
			 * network.
			 */
			if (((candidate_shared != NULL) &&
			     (candidate_shared != reply->shared)) ||
			    (lease6_usable(prefix) != ISC_TRUE))
				continue;

			/*
			 * And check if the prefix is still permitted
			 */

			if (((pond->prohibit_list != NULL) &&
			     (permitted(reply->packet, pond->prohibit_list))) ||
			    ((pond->permit_list != NULL) &&
			     (!permitted(reply->packet, pond->permit_list))))
				continue;

			best_prefix = prefix_compare(reply, prefix,
						     best_prefix);
		}
	}

	/* Try to pick a new prefix if we didn't find one, or if we found an
	 * abandoned prefix.
	 */
	if ((best_prefix == NULL) || (best_prefix->state == FTS_ABANDONED)) {
		status = pick_v6_prefix(reply);
	} else if (best_prefix != NULL) {
		iasubopt_reference(&reply->lease, best_prefix, MDL);
		status = ISC_R_SUCCESS;
	}

	/* Pick the abandoned prefix as a last resort. */
	if ((status == ISC_R_NORESOURCES) && (best_prefix != NULL)) {
		/* I don't see how this is supposed to be done right now. */
		log_error("Reclaiming abandoned prefixes is not yet "
			  "supported.  Treating this as an out of space "
			  "condition.");
		/* iasubopt_reference(&reply->lease, best_prefix, MDL); */
	}

	/* Give up now if we didn't find a prefix. */
	if (status != ISC_R_SUCCESS)
		return status;

	if (reply->lease == NULL)
		log_fatal("Impossible condition at %s:%d.", MDL);

	scope = &reply->lease->scope;
	group = reply->lease->ipv6_pool->ipv6_pond->group;

	send_pref.lo_addr.len = 16;
	memcpy(send_pref.lo_addr.iabuf, &reply->lease->addr, 16);
	send_pref.bits = (int) reply->lease->plen;

      send_pref:
	status = reply_process_is_prefixed(reply, scope, group);
	if (status != ISC_R_SUCCESS)
		return status;

	status = reply_process_send_prefix(reply, &send_pref);
	return status;
}

/* Once a prefix is found for a client, perform several common functions;
 * Calculate and store valid and preferred prefix times, draw client options
 * into the option state.
 */
static isc_result_t
reply_process_is_prefixed(struct reply_state *reply,
			  struct binding_scope **scope, struct group *group)
{
	isc_result_t status = ISC_R_SUCCESS;
	struct data_string data;
	struct option_cache *oc;
	struct option_state *tmp_options = NULL;
	struct on_star *on_star;
	int i;

	/* Initialize values we will cleanup. */
	memset(&data, 0, sizeof(data));

	/*
	 * Find the proper on_star block to use.  We use the
	 * one in the lease if we have a lease or the one in
	 * the reply if we don't have a lease because this is
	 * a static instance
	 */
	if (reply->lease) {
		on_star = &reply->lease->on_star;
	} else {
		on_star = &reply->on_star;
	}

	/*
	 * Bring in the root configuration.  We only do this to bring
	 * in the on * statements, as we didn't have the lease available
	 * we we did it the first time.
	 */
	option_state_allocate(&tmp_options, MDL);
	execute_statements_in_scope(NULL, reply->packet, NULL, NULL,
				    reply->packet->options, tmp_options,
				    &global_scope, root_group, NULL,
				    on_star);
	if (tmp_options != NULL) {
		option_state_dereference(&tmp_options, MDL);
	}

	/*
	 * Bring configured options into the root packet level cache - start
	 * with the lease's closest enclosing group (passed in by the caller
	 * as 'group').
	 */
	execute_statements_in_scope(NULL, reply->packet, NULL, NULL,
				    reply->packet->options, reply->opt_state,
				    scope, group, root_group, on_star);

	/* Execute statements from class scopes. */
	for (i = reply->packet->class_count; i > 0; i--) {
		execute_statements_in_scope(NULL, reply->packet, NULL, NULL,
					    reply->packet->options,
					    reply->opt_state, scope,
					    reply->packet->classes[i - 1]->group,
					    group, on_star);
	}
	  
	/*
	 * If there is a host record, over-ride with values configured there,
	 * without re-evaluating configuration from the previously executed
	 * group or its common enclosers.
	 */
	if (reply->host != NULL)
		execute_statements_in_scope(NULL, reply->packet, NULL, NULL,
					    reply->packet->options,
					    reply->opt_state, scope,
					    reply->host->group, group,
					    on_star);

	/* Determine valid lifetime. */
	if (reply->client_valid == 0)
		reply->send_valid = DEFAULT_DEFAULT_LEASE_TIME;
	else
		reply->send_valid = reply->client_valid;

	oc = lookup_option(&server_universe, reply->opt_state,
			   SV_DEFAULT_LEASE_TIME);
	if (oc != NULL) {
		if (!evaluate_option_cache(&data, reply->packet, NULL, NULL,
					   reply->packet->options,
					   reply->opt_state,
					   scope, oc, MDL) ||
		    (data.len != 4)) {
			log_error("reply_process_is_prefixed: unable to "
				  "evaluate default prefix time");
			status = ISC_R_FAILURE;
			goto cleanup;
		}

		reply->send_valid = getULong(data.data);
		data_string_forget(&data, MDL);
	}

	if (reply->client_prefer == 0)
		reply->send_prefer = reply->send_valid;
	else
		reply->send_prefer = reply->client_prefer;

	if (reply->send_prefer >= reply->send_valid)
		reply->send_prefer = (reply->send_valid / 2) +
				     (reply->send_valid / 8);

	oc = lookup_option(&server_universe, reply->opt_state,
			   SV_PREFER_LIFETIME);
	if (oc != NULL) {
		if (!evaluate_option_cache(&data, reply->packet, NULL, NULL,
					   reply->packet->options,
					   reply->opt_state,
					   scope, oc, MDL) ||
		    (data.len != 4)) {
			log_error("reply_process_is_prefixed: unable to "
				  "evaluate preferred prefix time");
			status = ISC_R_FAILURE;
			goto cleanup;
		}

		reply->send_prefer = getULong(data.data);
		data_string_forget(&data, MDL);
	}

	/* Note lowest values for later calculation of renew/rebind times. */
	if (reply->prefer > reply->send_prefer)
		reply->prefer = reply->send_prefer;

	if (reply->valid > reply->send_valid)
		reply->valid = reply->send_valid;

	/* Perform dynamic prefix related update work. */
	if (reply->lease != NULL) {
		/* Cached lifetimes */
		reply->lease->prefer = reply->send_prefer;
		reply->lease->valid = reply->send_valid;

		/* Advance (or rewind) the valid lifetime. */
		if (reply->buf.reply.msg_type == DHCPV6_REPLY) {
			reply->lease->soft_lifetime_end_time =
				cur_time + reply->send_valid;
			/* Wait before renew! */
		}

		status = ia_add_iasubopt(reply->ia, reply->lease, MDL);
		if (status != ISC_R_SUCCESS) {
			log_fatal("reply_process_is_prefixed: Unable to "
				  "attach prefix to new IA_PD: %s",
				  isc_result_totext(status));
		}

		/*
		 * If this is a new prefix, make sure it is attached somewhere.
		 */
		if (reply->lease->ia == NULL) {
			ia_reference(&reply->lease->ia, reply->ia, MDL);
		}
	}

	/* Bring a copy of the relevant options into the IA_PD scope. */
	execute_statements_in_scope(NULL, reply->packet, NULL, NULL,
				    reply->packet->options, reply->reply_ia,
				    scope, group, root_group, NULL);

	/* Execute statements from class scopes. */
	for (i = reply->packet->class_count; i > 0; i--) {
		execute_statements_in_scope(NULL, reply->packet, NULL, NULL,
					    reply->packet->options,
					    reply->reply_ia, scope,
					    reply->packet->classes[i - 1]->group,
					    group, NULL);
	}
	  
	/*
	 * And bring in host record configuration, if any, but not to overlap
	 * the previous group or its common enclosers.
	 */
	if (reply->host != NULL)
		execute_statements_in_scope(NULL, reply->packet, NULL, NULL,
					    reply->packet->options,
					    reply->reply_ia, scope,
					    reply->host->group, group, NULL);

      cleanup:
	if (data.data != NULL)
		data_string_forget(&data, MDL);

	if (status == ISC_R_SUCCESS)
		reply->client_resources++;

	return status;
}

/* Simply send an IAPREFIX within the IA_PD scope as described. */
static isc_result_t
reply_process_send_prefix(struct reply_state *reply,
			  struct iaddrcidrnet *pref) {
	isc_result_t status = ISC_R_SUCCESS;
	struct data_string data;

	memset(&data, 0, sizeof(data));

	/* Now append the prefix. */
	data.len = IAPREFIX_OFFSET;
	if (!buffer_allocate(&data.buffer, data.len, MDL)) {
		log_error("reply_process_send_prefix: out of memory"
			  "allocating new IAPREFIX buffer.");
		status = ISC_R_NOMEMORY;
		goto cleanup;
	}
	data.data = data.buffer->data;

	putULong(data.buffer->data, reply->send_prefer);
	putULong(data.buffer->data + 4, reply->send_valid);
	data.buffer->data[8] = pref->bits;
	memcpy(data.buffer->data + 9, pref->lo_addr.iabuf, 16);

	if (!append_option_buffer(&dhcpv6_universe, reply->reply_ia,
				  data.buffer, data.buffer->data,
				  data.len, D6O_IAPREFIX, 0)) {
		log_error("reply_process_send_prefix: unable "
			  "to save IAPREFIX option");
		status = ISC_R_FAILURE;
		goto cleanup;
	}

	reply->resources_included = ISC_TRUE;

      cleanup:
	if (data.data != NULL)
		data_string_forget(&data, MDL);

	return status;
}

/* Choose the better of two prefixes. */
static struct iasubopt *
prefix_compare(struct reply_state *reply,
	       struct iasubopt *alpha, struct iasubopt *beta) {
	if (alpha == NULL)
		return beta;
	if (beta == NULL)
		return alpha;

	if (reply->preflen >= 0) {
		if ((alpha->plen == reply->preflen) &&
		    (beta->plen != reply->preflen))
			return alpha;
		if ((beta->plen == reply->preflen) &&
		    (alpha->plen != reply->preflen))
			return beta;
	}

	switch(alpha->state) {
	      case FTS_ACTIVE:
		switch(beta->state) {
		      case FTS_ACTIVE:
			/* Choose the prefix with the longest lifetime (most
			 * likely the most recently allocated).
			 */
			if (alpha->hard_lifetime_end_time < 
			    beta->hard_lifetime_end_time)
				return beta;
			else
				return alpha;

		      case FTS_EXPIRED:
		      case FTS_ABANDONED:
			return alpha;

		      default:
			log_fatal("Impossible condition at %s:%d.", MDL);
		}
		break;

	      case FTS_EXPIRED:
		switch (beta->state) {
		      case FTS_ACTIVE:
			return beta;

		      case FTS_EXPIRED:
			/* Choose the most recently expired prefix. */
			if (alpha->hard_lifetime_end_time <
			    beta->hard_lifetime_end_time)
				return beta;
			else if ((alpha->hard_lifetime_end_time ==
				  beta->hard_lifetime_end_time) &&
				 (alpha->soft_lifetime_end_time <
				  beta->soft_lifetime_end_time))
				return beta;
			else
				return alpha;

		      case FTS_ABANDONED:
			return alpha;

		      default:
			log_fatal("Impossible condition at %s:%d.", MDL);
		}
		break;

	      case FTS_ABANDONED:
		switch (beta->state) {
		      case FTS_ACTIVE:
		      case FTS_EXPIRED:
			return alpha;

		      case FTS_ABANDONED:
			/* Choose the prefix that was abandoned longest ago. */
			if (alpha->hard_lifetime_end_time <
			    beta->hard_lifetime_end_time)
				return alpha;

		      default:
			log_fatal("Impossible condition at %s:%d.", MDL);
		}
		break;

	      default:
		log_fatal("Impossible condition at %s:%d.", MDL);
	}

	log_fatal("Triple impossible condition at %s:%d.", MDL);
	return NULL;
}

/*
 * Solicit is how a client starts requesting addresses.
 *
 * If the client asks for rapid commit, and we support it, we will 
 * allocate the addresses and reply.
 *
 * Otherwise we will send an advertise message.
 */

static void
dhcpv6_solicit(struct data_string *reply_ret, struct packet *packet) {
	struct data_string client_id;

	/* 
	 * Validate our input.
	 */
	if (!valid_client_msg(packet, &client_id)) {
		return;
	}

	lease_to_client(reply_ret, packet, &client_id, NULL);

	/*
	 * Clean up.
	 */
	data_string_forget(&client_id, MDL);
}

/*
 * Request is how a client actually requests addresses.
 *
 * Very similar to Solicit handling, except the server DUID is required.
 */

/* TODO: reject unicast messages, unless we set unicast option */
static void
dhcpv6_request(struct data_string *reply_ret, struct packet *packet) {
	struct data_string client_id;
	struct data_string server_id;

	/*
	 * Validate our input.
	 */
	if (!valid_client_resp(packet, &client_id, &server_id)) {
		return;
	}

	/*
	 * Issue our lease.
	 */
	lease_to_client(reply_ret, packet, &client_id, &server_id);

	/*
	 * Cleanup.
	 */
	data_string_forget(&client_id, MDL);
	data_string_forget(&server_id, MDL);
}

/* Find a DHCPv6 packet's shared network from hints in the packet.
 */
static isc_result_t
shared_network_from_packet6(struct shared_network **shared,
			    struct packet *packet)
{
	const struct packet *chk_packet;
	const struct in6_addr *link_addr, *first_link_addr;
	struct iaddr tmp_addr;
	struct subnet *subnet;
	isc_result_t status;

	if ((shared == NULL) || (*shared != NULL) || (packet == NULL))
		return DHCP_R_INVALIDARG;

	/*
	 * First, find the link address where the packet from the client
	 * first appeared (if this packet was relayed).
	 */
	first_link_addr = NULL;
	chk_packet = packet->dhcpv6_container_packet;
	while (chk_packet != NULL) {
		link_addr = &chk_packet->dhcpv6_link_address;
		if (!IN6_IS_ADDR_UNSPECIFIED(link_addr) &&
		    !IN6_IS_ADDR_LINKLOCAL(link_addr)) {
			first_link_addr = link_addr;
			break;
		}
		chk_packet = chk_packet->dhcpv6_container_packet;
	}

	/*
	 * If there is a relayed link address, find the subnet associated
	 * with that, and use that to get the appropriate
	 * shared_network.
	 */
	if (first_link_addr != NULL) {
		tmp_addr.len = sizeof(*first_link_addr);
		memcpy(tmp_addr.iabuf,
		       first_link_addr, sizeof(*first_link_addr));
		subnet = NULL;
		if (!find_subnet(&subnet, tmp_addr, MDL)) {
			log_debug("No subnet found for link-address %s.",
				  piaddr(tmp_addr));
			return ISC_R_NOTFOUND;
		}
		status = shared_network_reference(shared,
						  subnet->shared_network, MDL);
		subnet_dereference(&subnet, MDL);

	/*
	 * If there is no link address, we will use the interface
	 * that this packet came in on to pick the shared_network.
	 */
	} else if (packet->interface != NULL) {
		status = shared_network_reference(shared,
					 packet->interface->shared_network,
					 MDL);
                if (packet->dhcpv6_container_packet != NULL) {
			log_info("[L2 Relay] No link address in relay packet "
				 "assuming L2 relay and using receiving "
				 "interface");
                }

	} else {
		/*
		 * We shouldn't be able to get here but if there is no link
		 * address and no interface we don't know where to get the
		 * pool from log an error and return an error.
		 */
		log_error("No interface and no link address " 
			  "can't determine pool");
		status = DHCP_R_INVALIDARG;
	}

	return status;
}

/*
 * When a client thinks it might be on a new link, it sends a 
 * Confirm message.
 *
 * From RFC3315 section 18.2.2:
 *
 *   When the server receives a Confirm message, the server determines
 *   whether the addresses in the Confirm message are appropriate for the
 *   link to which the client is attached.  If all of the addresses in the
 *   Confirm message pass this test, the server returns a status of
 *   Success.  If any of the addresses do not pass this test, the server
 *   returns a status of NotOnLink.  If the server is unable to perform
 *   this test (for example, the server does not have information about
 *   prefixes on the link to which the client is connected), or there were
 *   no addresses in any of the IAs sent by the client, the server MUST
 *   NOT send a reply to the client.
 */

static void
dhcpv6_confirm(struct data_string *reply_ret, struct packet *packet) {
	struct shared_network *shared;
	struct subnet *subnet;
	struct option_cache *ia, *ta, *oc;
	struct data_string cli_enc_opt_data, iaaddr, client_id, packet_oro;
	struct option_state *cli_enc_opt_state, *opt_state;
	struct iaddr cli_addr;
	int pass;
	isc_boolean_t inappropriate, has_addrs;
	char reply_data[65536];
	struct dhcpv6_packet *reply = (struct dhcpv6_packet *)reply_data;
	int reply_ofs = (int)(offsetof(struct dhcpv6_packet, options));

	/* 
	 * Basic client message validation.
	 */
	memset(&client_id, 0, sizeof(client_id));
	if (!valid_client_msg(packet, &client_id)) {
		return;
	}

	/*
	 * Do not process Confirms that do not have IA's we do not recognize.
	 */
	ia = lookup_option(&dhcpv6_universe, packet->options, D6O_IA_NA);
	ta = lookup_option(&dhcpv6_universe, packet->options, D6O_IA_TA);
	if ((ia == NULL) && (ta == NULL))
		return;

	/*
	 * IA_PD's are simply ignored.
	 */
	delete_option(&dhcpv6_universe, packet->options, D6O_IA_PD);

	/* 
	 * Bit of variable initialization.
	 */
	opt_state = cli_enc_opt_state = NULL;
	memset(&cli_enc_opt_data, 0, sizeof(cli_enc_opt_data));
	memset(&iaaddr, 0, sizeof(iaaddr));
	memset(&packet_oro, 0, sizeof(packet_oro));

	/* Determine what shared network the client is connected to.  We
	 * must not respond if we don't have any information about the
	 * network the client is on.
	 */
	shared = NULL;
	if ((shared_network_from_packet6(&shared, packet) != ISC_R_SUCCESS) ||
	    (shared == NULL))
		goto exit;

	/* If there are no recorded subnets, then we have no
	 * information about this subnet - ignore Confirms.
	 */
	subnet = shared->subnets;
	if (subnet == NULL)
		goto exit;

	/* Are the addresses in all the IA's appropriate for that link? */
	has_addrs = inappropriate = ISC_FALSE;
	pass = D6O_IA_NA;
	while(!inappropriate) {
		/* If we've reached the end of the IA_NA pass, move to the
		 * IA_TA pass.
		 */
		if ((pass == D6O_IA_NA) && (ia == NULL)) {
			pass = D6O_IA_TA;
			ia = ta;
		}

		/* If we've reached the end of all passes, we're done. */
		if (ia == NULL)
			break;

		if (((pass == D6O_IA_NA) &&
		     !get_encapsulated_IA_state(&cli_enc_opt_state,
						&cli_enc_opt_data,
						packet, ia, IA_NA_OFFSET)) ||
		    ((pass == D6O_IA_TA) &&
		     !get_encapsulated_IA_state(&cli_enc_opt_state,
						&cli_enc_opt_data,
						packet, ia, IA_TA_OFFSET))) {
			goto exit;
		}

		oc = lookup_option(&dhcpv6_universe, cli_enc_opt_state,
				   D6O_IAADDR);

		for ( ; oc != NULL ; oc = oc->next) {
			if (!evaluate_option_cache(&iaaddr, packet, NULL, NULL,
						   packet->options, NULL,
						   &global_scope, oc, MDL) ||
			    (iaaddr.len < IAADDR_OFFSET)) {
				log_error("dhcpv6_confirm: "
					  "error evaluating IAADDR.");
				goto exit;
			}

			/* Copy out the IPv6 address for processing. */
			cli_addr.len = 16;
			memcpy(cli_addr.iabuf, iaaddr.data, 16);

			data_string_forget(&iaaddr, MDL);

			/* Record that we've processed at least one address. */
			has_addrs = ISC_TRUE;

			/* Find out if any subnets cover this address. */
			for (subnet = shared->subnets ; subnet != NULL ;
			     subnet = subnet->next_sibling) {
				if (addr_eq(subnet_number(cli_addr,
							  subnet->netmask),
					    subnet->net))
					break;
			}

			/* If we reach the end of the subnet list, and no
			 * subnet matches the client address, then it must
			 * be inappropriate to the link (so far as our
			 * configuration says).  Once we've found one
			 * inappropriate address, there is no reason to
			 * continue searching.
			 */
			if (subnet == NULL) {
				inappropriate = ISC_TRUE;
				break;
			}
		}

		option_state_dereference(&cli_enc_opt_state, MDL);
		data_string_forget(&cli_enc_opt_data, MDL);

		/* Advance to the next IA_*. */
		ia = ia->next;
	}

	/* If the client supplied no addresses, do not reply. */
	if (!has_addrs)
		goto exit;

	/* 
	 * Set up reply.
	 */
	if (!start_reply(packet, &client_id, NULL, &opt_state, reply)) {
		goto exit;
	}

	/* 
	 * Set our status.
	 */
	if (inappropriate) {
		if (!set_status_code(STATUS_NotOnLink, 
				     "Some of the addresses are not on link.",
				     opt_state)) {
			goto exit;
		}
	} else {
		if (!set_status_code(STATUS_Success, 
				     "All addresses still on link.",
				     opt_state)) {
			goto exit;
		}
	}

	/* 
	 * Only one option: add it.
	 */
	reply_ofs += store_options6(reply_data+reply_ofs,
				    sizeof(reply_data)-reply_ofs, 
				    opt_state, packet,
				    required_opts, &packet_oro);

	/* 
	 * Return our reply to the caller.
	 */
	reply_ret->len = reply_ofs;
	reply_ret->buffer = NULL;
	if (!buffer_allocate(&reply_ret->buffer, reply_ofs, MDL)) {
		log_fatal("No memory to store reply.");
	}
	reply_ret->data = reply_ret->buffer->data;
	memcpy(reply_ret->buffer->data, reply, reply_ofs);

exit:
	/* Cleanup any stale data strings. */
	if (cli_enc_opt_data.buffer != NULL)
		data_string_forget(&cli_enc_opt_data, MDL);
	if (iaaddr.buffer != NULL)
		data_string_forget(&iaaddr, MDL);
	if (client_id.buffer != NULL)
		data_string_forget(&client_id, MDL);
	if (packet_oro.buffer != NULL)
		data_string_forget(&packet_oro, MDL);

	/* Release any stale option states. */
	if (cli_enc_opt_state != NULL)
		option_state_dereference(&cli_enc_opt_state, MDL);
	if (opt_state != NULL)
		option_state_dereference(&opt_state, MDL);
}

/*
 * Renew is when a client wants to extend its lease/prefix, at time T1.
 *
 * We handle this the same as if the client wants a new lease/prefix,
 * except for the error code of when addresses don't match.
 */

/* TODO: reject unicast messages, unless we set unicast option */
static void
dhcpv6_renew(struct data_string *reply, struct packet *packet) {
	struct data_string client_id;
	struct data_string server_id;

	/* 
	 * Validate the request.
	 */
	if (!valid_client_resp(packet, &client_id, &server_id)) {
		return;
	}

	/*
	 * Renew our lease.
	 */
	lease_to_client(reply, packet, &client_id, &server_id);

	/*
	 * Cleanup.
	 */
	data_string_forget(&server_id, MDL);
	data_string_forget(&client_id, MDL);
}

/*
 * Rebind is when a client wants to extend its lease, at time T2.
 *
 * We handle this the same as if the client wants a new lease, except
 * for the error code of when addresses don't match.
 */

static void
dhcpv6_rebind(struct data_string *reply, struct packet *packet) {
	struct data_string client_id;

	if (!valid_client_msg(packet, &client_id)) {
		return;
	}

	lease_to_client(reply, packet, &client_id, NULL);

	data_string_forget(&client_id, MDL);
}

static void
ia_na_match_decline(const struct data_string *client_id,
		    const struct data_string *iaaddr,
		    struct iasubopt *lease)
{
	char tmp_addr[INET6_ADDRSTRLEN];

	log_error("Client %s reports address %s is "
		  "already in use by another host!",
		  print_hex_1(client_id->len, client_id->data, 60),
		  inet_ntop(AF_INET6, iaaddr->data, 
		  	    tmp_addr, sizeof(tmp_addr)));
	if (lease != NULL) {
		decline_lease6(lease->ipv6_pool, lease);
		lease->ia->cltt = cur_time;
		write_ia(lease->ia);
	}
}

static void
ia_na_nomatch_decline(const struct data_string *client_id,
		      const struct data_string *iaaddr,
		      u_int32_t *ia_na_id,
		      struct packet *packet,
		      char *reply_data,
		      int *reply_ofs,
		      int reply_len)
{
	char tmp_addr[INET6_ADDRSTRLEN];
	struct option_state *host_opt_state;
	int len;

	log_info("Client %s declines address %s, which is not offered to it.",
		 print_hex_1(client_id->len, client_id->data, 60),
		 inet_ntop(AF_INET6, iaaddr->data, tmp_addr, sizeof(tmp_addr)));

	/*
	 * Create state for this IA_NA.
	 */
	host_opt_state = NULL;
	if (!option_state_allocate(&host_opt_state, MDL)) {
		log_error("ia_na_nomatch_decline: out of memory "
			  "allocating option_state.");
		goto exit;
	}

	if (!set_status_code(STATUS_NoBinding, "Decline for unknown address.",
			     host_opt_state)) {
		goto exit;
	}

	/*
	 * Insure we have enough space
	 */
	if (reply_len < (*reply_ofs + 16)) {
		log_error("ia_na_nomatch_decline: "
			  "out of space for reply packet.");
		goto exit;
	}

	/*
	 * Put our status code into the reply packet.
	 */
	len = store_options6(reply_data+(*reply_ofs)+16,
			     reply_len-(*reply_ofs)-16,
			     host_opt_state, packet,
			     required_opts_STATUS_CODE, NULL);

	/*
	 * Store the non-encapsulated option data for this 
	 * IA_NA into our reply packet. Defined in RFC 3315, 
	 * section 22.4.  
	 */
	/* option number */
	putUShort((unsigned char *)reply_data+(*reply_ofs), D6O_IA_NA);
	/* option length */
	putUShort((unsigned char *)reply_data+(*reply_ofs)+2, len + 12);
	/* IA_NA, copied from the client */
	memcpy(reply_data+(*reply_ofs)+4, ia_na_id, 4);
	/* t1 and t2, odd that we need them, but here it is */
	putULong((unsigned char *)reply_data+(*reply_ofs)+8, 0);
	putULong((unsigned char *)reply_data+(*reply_ofs)+12, 0);

	/*
	 * Get ready for next IA_NA.
	 */
	*reply_ofs += (len + 16);

exit:
	option_state_dereference(&host_opt_state, MDL);
}

static void
iterate_over_ia_na(struct data_string *reply_ret, 
		   struct packet *packet,
		   const struct data_string *client_id,
		   const struct data_string *server_id,
		   const char *packet_type,
		   void (*ia_na_match)(const struct data_string *,
                                       const struct data_string *,
                                       struct iasubopt *),
		   void (*ia_na_nomatch)(const struct data_string *,
                                         const struct data_string *,
                                         u_int32_t *, struct packet *, char *,
                                         int *, int))
{
	struct option_state *opt_state;
	struct host_decl *packet_host;
	struct option_cache *ia;
	struct option_cache *oc;
	/* cli_enc_... variables come from the IA_NA/IA_TA options */
	struct data_string cli_enc_opt_data;
	struct option_state *cli_enc_opt_state;
	struct host_decl *host;
	struct option_state *host_opt_state;
	struct data_string iaaddr;
	struct data_string fixed_addr;
	char reply_data[65536];
	struct dhcpv6_packet *reply = (struct dhcpv6_packet *)reply_data;
	int reply_ofs = (int)(offsetof(struct dhcpv6_packet, options));
	char status_msg[32];
	struct iasubopt *lease;
	struct ia_xx *existing_ia_na;
	int i;
	struct data_string key;
	u_int32_t iaid;

	/*
	 * Initialize to empty values, in case we have to exit early.
	 */
	opt_state = NULL;
	memset(&cli_enc_opt_data, 0, sizeof(cli_enc_opt_data));
	cli_enc_opt_state = NULL;
	memset(&iaaddr, 0, sizeof(iaaddr));
	memset(&fixed_addr, 0, sizeof(fixed_addr));
	host_opt_state = NULL;
	lease = NULL;

	/* 
	 * Find the host record that matches from the packet, if any.
	 */
	packet_host = NULL;
	if (!find_hosts_by_uid(&packet_host, 
			       client_id->data, client_id->len, MDL)) {
		packet_host = NULL;
		/* 
		 * Note: In general, we don't expect a client to provide
		 *       enough information to match by option for these
		 *       types of messages, but if we don't have a UID
		 *       match we can check anyway.
		 */
		if (!find_hosts_by_option(&packet_host, 
					  packet, packet->options, MDL)) {
			packet_host = NULL;

			if (!find_hosts_by_duid_chaddr(&packet_host,
						       client_id))
				packet_host = NULL;
		}
	}

	/* 
	 * Set our reply information.
	 */
	reply->msg_type = DHCPV6_REPLY;
	memcpy(reply->transaction_id, packet->dhcpv6_transaction_id, 
	       sizeof(reply->transaction_id));

	/*
	 * Build our option state for reply.
	 */
	opt_state = NULL;
	if (!option_state_allocate(&opt_state, MDL)) {
		log_error("iterate_over_ia_na: no memory for option_state.");
		goto exit;
	}
	execute_statements_in_scope(NULL, packet, NULL, NULL, 
				    packet->options, opt_state, 
				    &global_scope, root_group, NULL, NULL);

	/* 
	 * RFC 3315, section 18.2.7 tells us which options to include.
	 */
	oc = lookup_option(&dhcpv6_universe, opt_state, D6O_SERVERID);
	if (oc == NULL) {
		if (!save_option_buffer(&dhcpv6_universe, opt_state, NULL, 
					(unsigned char *)server_duid.data, 
					server_duid.len, D6O_SERVERID, 0)) {
			log_error("iterate_over_ia_na: "
				  "error saving server identifier.");
			goto exit;
		}
	}

	if (!save_option_buffer(&dhcpv6_universe, opt_state, 
				client_id->buffer, 
				(unsigned char *)client_id->data,
				client_id->len, 
				D6O_CLIENTID, 0)) {
		log_error("iterate_over_ia_na: "
			  "error saving client identifier.");
		goto exit;
	}

	snprintf(status_msg, sizeof(status_msg), "%s received.", packet_type);
	if (!set_status_code(STATUS_Success, status_msg, opt_state)) {
		goto exit;
	}

	/* 
	 * Add our options that are not associated with any IA_NA or IA_TA. 
	 */
	reply_ofs += store_options6(reply_data+reply_ofs,
				    sizeof(reply_data)-reply_ofs, 
				    opt_state, packet,
				    required_opts, NULL);

	/*
	 * Loop through the IA_NA reported by the client, and deal with
	 * addresses reported as already in use.
	 */
	for (ia = lookup_option(&dhcpv6_universe, packet->options, D6O_IA_NA);
	     ia != NULL; ia = ia->next) {

		if (!get_encapsulated_IA_state(&cli_enc_opt_state,
					       &cli_enc_opt_data,
					       packet, ia, IA_NA_OFFSET)) {
			goto exit;
		}

		iaid = getULong(cli_enc_opt_data.data);

		/* 
		 * XXX: It is possible that we can get multiple addresses
		 *      sent by the client. We don't send multiple 
		 *      addresses, so this indicates a client error. 
		 *      We should check for multiple IAADDR options, log
		 *      if found, and set as an error.
		 */
		oc = lookup_option(&dhcpv6_universe, cli_enc_opt_state, 
				   D6O_IAADDR);
		if (oc == NULL) {
			/* no address given for this IA, ignore */
			option_state_dereference(&cli_enc_opt_state, MDL);
			data_string_forget(&cli_enc_opt_data, MDL);
			continue;
		}

		memset(&iaaddr, 0, sizeof(iaaddr));
		if (!evaluate_option_cache(&iaaddr, packet, NULL, NULL, 
					   packet->options, NULL,
					   &global_scope, oc, MDL)) {
			log_error("iterate_over_ia_na: "
				  "error evaluating IAADDR.");
			goto exit;
		}

		/* 
		 * Now we need to figure out which host record matches
		 * this IA_NA and IAADDR (encapsulated option contents
		 * matching a host record by option).
		 *
		 * XXX: We don't currently track IA_NA separately, but
		 *      we will need to do this!
		 */
		host = NULL;
		if (!find_hosts_by_option(&host, packet, 
					  cli_enc_opt_state, MDL)) { 
			if (packet_host != NULL) {
				host = packet_host;
			} else {
				host = NULL;
			}
		}
		while (host != NULL) {
			if (host->fixed_addr != NULL) {
				if (!evaluate_option_cache(&fixed_addr, NULL, 
							   NULL, NULL, NULL, 
							   NULL, &global_scope,
							   host->fixed_addr, 
							   MDL)) {
					log_error("iterate_over_ia_na: error "
						  "evaluating host address.");
					goto exit;
				}
				if ((iaaddr.len >= 16) &&
				    !memcmp(fixed_addr.data, iaaddr.data, 16)) {
					data_string_forget(&fixed_addr, MDL);
					break;
				}
				data_string_forget(&fixed_addr, MDL);
			}
			host = host->n_ipaddr;
		}

		if ((host == NULL) && (iaaddr.len >= IAADDR_OFFSET)) {
			/*
			 * Find existing IA_NA.
			 */
			if (ia_make_key(&key, iaid, 
					(char *)client_id->data,
					client_id->len, 
					MDL) != ISC_R_SUCCESS) {
				log_fatal("iterate_over_ia_na: no memory for "
					  "key.");
			}

			existing_ia_na = NULL;
			if (ia_hash_lookup(&existing_ia_na, ia_na_active, 
					   (unsigned char *)key.data, 
					   key.len, MDL)) {
				/* 
				 * Make sure this address is in the IA_NA.
				 */
				for (i=0; i<existing_ia_na->num_iasubopt; i++) {
					struct iasubopt *tmp;
					struct in6_addr *in6_addr;

					tmp = existing_ia_na->iasubopt[i];
					in6_addr = &tmp->addr;
					if (memcmp(in6_addr, 
						   iaaddr.data, 16) == 0) {
						iasubopt_reference(&lease,
								   tmp, MDL);
						break;
					}
				}
			}

			data_string_forget(&key, MDL);
		}

		if ((host != NULL) || (lease != NULL)) {
			ia_na_match(client_id, &iaaddr, lease);
		} else {
			ia_na_nomatch(client_id, &iaaddr, 
				      (u_int32_t *)cli_enc_opt_data.data, 
				      packet, reply_data, &reply_ofs, 
				      sizeof(reply_data));
		}

		if (lease != NULL) {
			iasubopt_dereference(&lease, MDL);
		}

		data_string_forget(&iaaddr, MDL);
		option_state_dereference(&cli_enc_opt_state, MDL);
		data_string_forget(&cli_enc_opt_data, MDL);
	}

	/* 
	 * Return our reply to the caller.
	 */
	reply_ret->len = reply_ofs;
	reply_ret->buffer = NULL;
	if (!buffer_allocate(&reply_ret->buffer, reply_ofs, MDL)) {
		log_fatal("No memory to store reply.");
	}
	reply_ret->data = reply_ret->buffer->data;
	memcpy(reply_ret->buffer->data, reply, reply_ofs);

exit:
	if (lease != NULL) {
		iasubopt_dereference(&lease, MDL);
	}
	if (host_opt_state != NULL) {
		option_state_dereference(&host_opt_state, MDL);
	}
	if (fixed_addr.buffer != NULL) {
		data_string_forget(&fixed_addr, MDL);
	}
	if (iaaddr.buffer != NULL) {
		data_string_forget(&iaaddr, MDL);
	}
	if (cli_enc_opt_state != NULL) {
		option_state_dereference(&cli_enc_opt_state, MDL);
	}
	if (cli_enc_opt_data.buffer != NULL) {
		data_string_forget(&cli_enc_opt_data, MDL);
	}
	if (opt_state != NULL) {
		option_state_dereference(&opt_state, MDL);
	}
}

/*
 * Decline means a client has detected that something else is using an
 * address we gave it.
 *
 * Since we're only dealing with fixed leases for now, there's not
 * much we can do, other that log the occurrence.
 * 
 * When we start issuing addresses from pools, then we will have to
 * record our declined addresses and issue another. In general with
 * IPv6 there is no worry about DoS by clients exhausting space, but
 * we still need to be aware of this possibility.
 */

/* TODO: reject unicast messages, unless we set unicast option */
/* TODO: IA_TA */
static void
dhcpv6_decline(struct data_string *reply, struct packet *packet) {
	struct data_string client_id;
	struct data_string server_id;

	/* 
	 * Validate our input.
	 */
	if (!valid_client_resp(packet, &client_id, &server_id)) {
		return;
	}

	/*
	 * Undefined for IA_PD.
	 */
	delete_option(&dhcpv6_universe, packet->options, D6O_IA_PD);

	/*
	 * And operate on each IA_NA in this packet.
	 */
	iterate_over_ia_na(reply, packet, &client_id, &server_id, "Decline", 
			   ia_na_match_decline, ia_na_nomatch_decline);

	data_string_forget(&server_id, MDL);
	data_string_forget(&client_id, MDL);
}

static void
ia_na_match_release(const struct data_string *client_id,
		    const struct data_string *iaaddr,
		    struct iasubopt *lease)
{
	char tmp_addr[INET6_ADDRSTRLEN];

	log_info("Client %s releases address %s",
		 print_hex_1(client_id->len, client_id->data, 60),
		 inet_ntop(AF_INET6, iaaddr->data, tmp_addr, sizeof(tmp_addr)));
	if (lease != NULL) {
		release_lease6(lease->ipv6_pool, lease);
		lease->ia->cltt = cur_time;
		write_ia(lease->ia);
	}
}

static void
ia_na_nomatch_release(const struct data_string *client_id,
		      const struct data_string *iaaddr,
		      u_int32_t *ia_na_id,
		      struct packet *packet,
		      char *reply_data,
		      int *reply_ofs,
		      int reply_len)
{
	char tmp_addr[INET6_ADDRSTRLEN];
	struct option_state *host_opt_state;
	int len;

	log_info("Client %s releases address %s, which is not leased to it.",
		 print_hex_1(client_id->len, client_id->data, 60),
		 inet_ntop(AF_INET6, iaaddr->data, tmp_addr, sizeof(tmp_addr)));

	/*
	 * Create state for this IA_NA.
	 */
	host_opt_state = NULL;
	if (!option_state_allocate(&host_opt_state, MDL)) {
		log_error("ia_na_nomatch_release: out of memory "
			  "allocating option_state.");
		goto exit;
	}

	if (!set_status_code(STATUS_NoBinding, 
			     "Release for non-leased address.",
			     host_opt_state)) {
		goto exit;
	}

	/*
	 * Insure we have enough space
	 */
	if (reply_len < (*reply_ofs + 16)) {
		log_error("ia_na_nomatch_release: "
			  "out of space for reply packet.");
		goto exit;
	}

	/*
	 * Put our status code into the reply packet.
	 */
	len = store_options6(reply_data+(*reply_ofs)+16,
			     reply_len-(*reply_ofs)-16,
			     host_opt_state, packet,
			     required_opts_STATUS_CODE, NULL);

	/*
	 * Store the non-encapsulated option data for this 
	 * IA_NA into our reply packet. Defined in RFC 3315, 
	 * section 22.4.  
	 */
	/* option number */
	putUShort((unsigned char *)reply_data+(*reply_ofs), D6O_IA_NA);
	/* option length */
	putUShort((unsigned char *)reply_data+(*reply_ofs)+2, len + 12);
	/* IA_NA, copied from the client */
	memcpy(reply_data+(*reply_ofs)+4, ia_na_id, 4);
	/* t1 and t2, odd that we need them, but here it is */
	putULong((unsigned char *)reply_data+(*reply_ofs)+8, 0);
	putULong((unsigned char *)reply_data+(*reply_ofs)+12, 0);

	/*
	 * Get ready for next IA_NA.
	 */
	*reply_ofs += (len + 16);

exit:
	option_state_dereference(&host_opt_state, MDL);
}

static void
ia_pd_match_release(const struct data_string *client_id,
		    const struct data_string *iapref,
		    struct iasubopt *prefix)
{
	char tmp_addr[INET6_ADDRSTRLEN];

	log_info("Client %s releases prefix %s/%u",
		 print_hex_1(client_id->len, client_id->data, 60),
		 inet_ntop(AF_INET6, iapref->data + 9,
			   tmp_addr, sizeof(tmp_addr)),
		 (unsigned) getUChar(iapref->data + 8));
	if (prefix != NULL) {
		release_lease6(prefix->ipv6_pool, prefix);
		prefix->ia->cltt = cur_time;
		write_ia(prefix->ia);
	}
}

static void
ia_pd_nomatch_release(const struct data_string *client_id,
		      const struct data_string *iapref,
		      u_int32_t *ia_pd_id,
		      struct packet *packet,
		      char *reply_data,
		      int *reply_ofs,
		      int reply_len)
{
	char tmp_addr[INET6_ADDRSTRLEN];
	struct option_state *host_opt_state;
	int len;

	log_info("Client %s releases prefix %s/%u, which is not leased to it.",
		 print_hex_1(client_id->len, client_id->data, 60),
		 inet_ntop(AF_INET6, iapref->data + 9,
			   tmp_addr, sizeof(tmp_addr)),
		 (unsigned) getUChar(iapref->data + 8));

	/*
	 * Create state for this IA_PD.
	 */
	host_opt_state = NULL;
	if (!option_state_allocate(&host_opt_state, MDL)) {
		log_error("ia_pd_nomatch_release: out of memory "
			  "allocating option_state.");
		goto exit;
	}

	if (!set_status_code(STATUS_NoBinding, 
			     "Release for non-leased prefix.",
			     host_opt_state)) {
		goto exit;
	}

	/*
	 * Insure we have enough space
	 */
	if (reply_len < (*reply_ofs + 16)) {
		log_error("ia_pd_nomatch_release: "
			  "out of space for reply packet.");
		goto exit;
	}

	/*
	 * Put our status code into the reply packet.
	 */
	len = store_options6(reply_data+(*reply_ofs)+16,
			     reply_len-(*reply_ofs)-16,
			     host_opt_state, packet,
			     required_opts_STATUS_CODE, NULL);

	/*
	 * Store the non-encapsulated option data for this 
	 * IA_PD into our reply packet. Defined in RFC 3315, 
	 * section 22.4.  
	 */
	/* option number */
	putUShort((unsigned char *)reply_data+(*reply_ofs), D6O_IA_PD);
	/* option length */
	putUShort((unsigned char *)reply_data+(*reply_ofs)+2, len + 12);
	/* IA_PD, copied from the client */
	memcpy(reply_data+(*reply_ofs)+4, ia_pd_id, 4);
	/* t1 and t2, odd that we need them, but here it is */
	putULong((unsigned char *)reply_data+(*reply_ofs)+8, 0);
	putULong((unsigned char *)reply_data+(*reply_ofs)+12, 0);

	/*
	 * Get ready for next IA_PD.
	 */
	*reply_ofs += (len + 16);

exit:
	option_state_dereference(&host_opt_state, MDL);
}

static void
iterate_over_ia_pd(struct data_string *reply_ret, 
		   struct packet *packet,
		   const struct data_string *client_id,
		   const struct data_string *server_id,
		   const char *packet_type,
                   void (*ia_pd_match)(const struct data_string *,
                                       const struct data_string *,
                                       struct iasubopt *),
                   void (*ia_pd_nomatch)(const struct data_string *,
                                         const struct data_string *,
                                         u_int32_t *, struct packet *, char *,
                                         int *, int))
{
	struct data_string reply_new;
	int reply_len;
	struct option_state *opt_state;
	struct host_decl *packet_host;
	struct option_cache *ia;
	struct option_cache *oc;
	/* cli_enc_... variables come from the IA_PD options */
	struct data_string cli_enc_opt_data;
	struct option_state *cli_enc_opt_state;
	struct host_decl *host;
	struct option_state *host_opt_state;
	struct data_string iaprefix;
	char reply_data[65536];
	int reply_ofs;
	struct iasubopt *prefix;
	struct ia_xx *existing_ia_pd;
	int i;
	struct data_string key;
	u_int32_t iaid;

	/*
	 * Initialize to empty values, in case we have to exit early.
	 */
	memset(&reply_new, 0, sizeof(reply_new));
	opt_state = NULL;
	memset(&cli_enc_opt_data, 0, sizeof(cli_enc_opt_data));
	cli_enc_opt_state = NULL;
	memset(&iaprefix, 0, sizeof(iaprefix));
	host_opt_state = NULL;
	prefix = NULL;

	/*
	 * Compute the available length for the reply.
	 */
	reply_len = sizeof(reply_data) - reply_ret->len;
	reply_ofs = 0;

	/* 
	 * Find the host record that matches from the packet, if any.
	 */
	packet_host = NULL;
	if (!find_hosts_by_uid(&packet_host, 
			       client_id->data, client_id->len, MDL)) {
		packet_host = NULL;
		/* 
		 * Note: In general, we don't expect a client to provide
		 *       enough information to match by option for these
		 *       types of messages, but if we don't have a UID
		 *       match we can check anyway.
		 */
		if (!find_hosts_by_option(&packet_host, 
					  packet, packet->options, MDL)) {
			packet_host = NULL;

			if (!find_hosts_by_duid_chaddr(&packet_host,
						       client_id))
				packet_host = NULL;
		}
	}

	/*
	 * Build our option state for reply.
	 */
	opt_state = NULL;
	if (!option_state_allocate(&opt_state, MDL)) {
		log_error("iterate_over_ia_pd: no memory for option_state.");
		goto exit;
	}
	execute_statements_in_scope(NULL, packet, NULL, NULL, 
				    packet->options, opt_state, 
				    &global_scope, root_group, NULL, NULL);

	/*
	 * Loop through the IA_PD reported by the client, and deal with
	 * prefixes reported as already in use.
	 */
	for (ia = lookup_option(&dhcpv6_universe, packet->options, D6O_IA_PD);
	     ia != NULL; ia = ia->next) {

	    if (!get_encapsulated_IA_state(&cli_enc_opt_state,
					   &cli_enc_opt_data,
					   packet, ia, IA_PD_OFFSET)) {
		goto exit;
	    }

	    iaid = getULong(cli_enc_opt_data.data);

	    oc = lookup_option(&dhcpv6_universe, cli_enc_opt_state, 
			       D6O_IAPREFIX);
	    if (oc == NULL) {
		/* no prefix given for this IA_PD, ignore */
		option_state_dereference(&cli_enc_opt_state, MDL);
		data_string_forget(&cli_enc_opt_data, MDL);
		continue;
	    }

	    for (; oc != NULL; oc = oc->next) {
		memset(&iaprefix, 0, sizeof(iaprefix));
		if (!evaluate_option_cache(&iaprefix, packet, NULL, NULL, 
					   packet->options, NULL,
					   &global_scope, oc, MDL)) {
			log_error("iterate_over_ia_pd: "
				  "error evaluating IAPREFIX.");
			goto exit;
		}

		/* 
		 * Now we need to figure out which host record matches
		 * this IA_PD and IAPREFIX (encapsulated option contents
		 * matching a host record by option).
		 *
		 * XXX: We don't currently track IA_PD separately, but
		 *      we will need to do this!
		 */
		host = NULL;
		if (!find_hosts_by_option(&host, packet, 
					  cli_enc_opt_state, MDL)) { 
			if (packet_host != NULL) {
				host = packet_host;
			} else {
				host = NULL;
			}
		}
		while (host != NULL) {
			if (host->fixed_prefix != NULL) {
				struct iaddrcidrnetlist *l;
				int plen = (int) getUChar(iaprefix.data + 8);

				for (l = host->fixed_prefix; l != NULL;
				     l = l->next) {
					if (plen != l->cidrnet.bits)
						continue;
					if (memcmp(iaprefix.data + 9,
						   l->cidrnet.lo_addr.iabuf,
						   16) == 0)
						break;
				}
				if ((l != NULL) && (iaprefix.len >= 17))
					break;
			}
			host = host->n_ipaddr;
		}

		if ((host == NULL) && (iaprefix.len >= IAPREFIX_OFFSET)) {
			/*
			 * Find existing IA_PD.
			 */
			if (ia_make_key(&key, iaid, 
					(char *)client_id->data,
					client_id->len, 
					MDL) != ISC_R_SUCCESS) {
				log_fatal("iterate_over_ia_pd: no memory for "
					  "key.");
			}

			existing_ia_pd = NULL;
			if (ia_hash_lookup(&existing_ia_pd, ia_pd_active, 
					   (unsigned char *)key.data, 
					   key.len, MDL)) {
				/* 
				 * Make sure this prefix is in the IA_PD.
				 */
				for (i = 0;
				     i < existing_ia_pd->num_iasubopt;
				     i++) {
					struct iasubopt *tmp;
					u_int8_t plen;

					plen = getUChar(iaprefix.data + 8);
					tmp = existing_ia_pd->iasubopt[i];
					if ((tmp->plen == plen) &&
					    (memcmp(&tmp->addr,
						    iaprefix.data + 9,
						    16) == 0)) {
						iasubopt_reference(&prefix,
								   tmp, MDL);
						break;
					}
				}
			}

			data_string_forget(&key, MDL);
		}

		if ((host != NULL) || (prefix != NULL)) {
			ia_pd_match(client_id, &iaprefix, prefix);
		} else {
			ia_pd_nomatch(client_id, &iaprefix, 
				      (u_int32_t *)cli_enc_opt_data.data, 
				      packet, reply_data, &reply_ofs, 
				      reply_len - reply_ofs);
		}

		if (prefix != NULL) {
			iasubopt_dereference(&prefix, MDL);
		}

		data_string_forget(&iaprefix, MDL);
	    }

	    option_state_dereference(&cli_enc_opt_state, MDL);
	    data_string_forget(&cli_enc_opt_data, MDL);
	}

	/* 
	 * Return our reply to the caller.
	 * The IA_NA routine has already filled at least the header.
	 */
	reply_new.len = reply_ret->len + reply_ofs;
	if (!buffer_allocate(&reply_new.buffer, reply_new.len, MDL)) {
		log_fatal("No memory to store reply.");
	}
	reply_new.data = reply_new.buffer->data;
	memcpy(reply_new.buffer->data,
	       reply_ret->buffer->data, reply_ret->len);
	memcpy(reply_new.buffer->data + reply_ret->len,
	       reply_data, reply_ofs);
	data_string_forget(reply_ret, MDL);
	data_string_copy(reply_ret, &reply_new, MDL);
	data_string_forget(&reply_new, MDL);

exit:
	if (prefix != NULL) {
		iasubopt_dereference(&prefix, MDL);
	}
	if (host_opt_state != NULL) {
		option_state_dereference(&host_opt_state, MDL);
	}
	if (iaprefix.buffer != NULL) {
		data_string_forget(&iaprefix, MDL);
	}
	if (cli_enc_opt_state != NULL) {
		option_state_dereference(&cli_enc_opt_state, MDL);
	}
	if (cli_enc_opt_data.buffer != NULL) {
		data_string_forget(&cli_enc_opt_data, MDL);
	}
	if (opt_state != NULL) {
		option_state_dereference(&opt_state, MDL);
	}
}

/*
 * Release means a client is done with the leases.
 */

/* TODO: reject unicast messages, unless we set unicast option */
static void
dhcpv6_release(struct data_string *reply, struct packet *packet) {
	struct data_string client_id;
	struct data_string server_id;

	/* 
	 * Validate our input.
	 */
	if (!valid_client_resp(packet, &client_id, &server_id)) {
		return;
	}

	/*
	 * And operate on each IA_NA in this packet.
	 */
	iterate_over_ia_na(reply, packet, &client_id, &server_id, "Release", 
			   ia_na_match_release, ia_na_nomatch_release);

	/*
	 * And operate on each IA_PD in this packet.
	 */
	iterate_over_ia_pd(reply, packet, &client_id, &server_id, "Release",
			   ia_pd_match_release, ia_pd_nomatch_release);

	data_string_forget(&server_id, MDL);
	data_string_forget(&client_id, MDL);
}

/*
 * Information-Request is used by clients who have obtained an address
 * from other means, but want configuration information from the server.
 */

static void
dhcpv6_information_request(struct data_string *reply, struct packet *packet) {
	struct data_string client_id;
	struct data_string server_id;

	/*
	 * Validate our input.
	 */
	if (!valid_client_info_req(packet, &server_id)) {
		return;
	}

	/*
	 * Get our client ID, if there is one.
	 */
	memset(&client_id, 0, sizeof(client_id));
	if (get_client_id(packet, &client_id) != ISC_R_SUCCESS) {
		data_string_forget(&client_id, MDL);
	}

	/*
	 * Use the lease_to_client() function. This will work fine, 
	 * because the valid_client_info_req() insures that we 
	 * don't have any IA that would cause us to allocate
	 * resources to the client.
	 */
	lease_to_client(reply, packet, &client_id,
			server_id.data != NULL ? &server_id : NULL);

	/*
	 * Cleanup.
	 */
	if (client_id.data != NULL) {
		data_string_forget(&client_id, MDL);
	}
	data_string_forget(&server_id, MDL);
}

/* 
 * The Relay-forw message is sent by relays. It typically contains a
 * single option, which encapsulates an entire packet.
 *
 * We need to build an encapsulated reply.
 */

/* XXX: this is very, very similar to do_packet6(), and should probably
	be combined in a clever way */
static void
dhcpv6_relay_forw(struct data_string *reply_ret, struct packet *packet) {
	struct option_cache *oc;
	struct data_string enc_opt_data;
	struct packet *enc_packet;
	unsigned char msg_type;
	const struct dhcpv6_packet *msg;
	const struct dhcpv6_relay_packet *relay;
	struct data_string enc_reply;
	char link_addr[sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255")];
	char peer_addr[sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255")];
	struct data_string a_opt, packet_ero;
	struct option_state *opt_state;
	static char reply_data[65536];
	struct dhcpv6_relay_packet *reply;
	int reply_ofs;

	/* 
	 * Initialize variables for early exit.
	 */
	opt_state = NULL;
	memset(&a_opt, 0, sizeof(a_opt));
	memset(&packet_ero, 0, sizeof(packet_ero));
	memset(&enc_reply, 0, sizeof(enc_reply));
	memset(&enc_opt_data, 0, sizeof(enc_opt_data));
	enc_packet = NULL;

	/*
	 * Get our encapsulated relay message.
	 */
	oc = lookup_option(&dhcpv6_universe, packet->options, D6O_RELAY_MSG);
	if (oc == NULL) {
		inet_ntop(AF_INET6, &packet->dhcpv6_link_address,
			  link_addr, sizeof(link_addr));
		inet_ntop(AF_INET6, &packet->dhcpv6_peer_address,
			  peer_addr, sizeof(peer_addr));
		log_info("Relay-forward from %s with link address=%s and "
			 "peer address=%s missing Relay Message option.",
			  piaddr(packet->client_addr), link_addr, peer_addr);
		goto exit;
	}

	if (!evaluate_option_cache(&enc_opt_data, NULL, NULL, NULL, 
				   NULL, NULL, &global_scope, oc, MDL)) {
		log_error("dhcpv6_forw_relay: error evaluating "
			  "relayed message.");
		goto exit;
	}

	if (!packet6_len_okay((char *)enc_opt_data.data, enc_opt_data.len)) {
		log_error("dhcpv6_forw_relay: encapsulated packet too short.");
		goto exit;
	}

	/*
	 * Build a packet structure from this encapsulated packet.
	 */
	enc_packet = NULL;
	if (!packet_allocate(&enc_packet, MDL)) {
		log_error("dhcpv6_forw_relay: "
			  "no memory for encapsulated packet.");
		goto exit;
	}

	if (!option_state_allocate(&enc_packet->options, MDL)) {
		log_error("dhcpv6_forw_relay: "
			  "no memory for encapsulated packet's options.");
		goto exit;
	}

	enc_packet->client_port = packet->client_port;
	enc_packet->client_addr = packet->client_addr;
	interface_reference(&enc_packet->interface, packet->interface, MDL);
	enc_packet->dhcpv6_container_packet = packet;

	msg_type = enc_opt_data.data[0];
	if ((msg_type == DHCPV6_RELAY_FORW) ||
	    (msg_type == DHCPV6_RELAY_REPL)) {
		int relaylen = (int)(offsetof(struct dhcpv6_relay_packet, options));
		relay = (struct dhcpv6_relay_packet *)enc_opt_data.data;
		enc_packet->dhcpv6_msg_type = relay->msg_type;

		/* relay-specific data */
		enc_packet->dhcpv6_hop_count = relay->hop_count;
		memcpy(&enc_packet->dhcpv6_link_address,
		       relay->link_address, sizeof(relay->link_address));
		memcpy(&enc_packet->dhcpv6_peer_address,
		       relay->peer_address, sizeof(relay->peer_address));

		if (!parse_option_buffer(enc_packet->options,
					 relay->options, 
					 enc_opt_data.len - relaylen,
					 &dhcpv6_universe)) {
			/* no logging here, as parse_option_buffer() logs all
			   cases where it fails */
			goto exit;
		}
	} else {
		int msglen = (int)(offsetof(struct dhcpv6_packet, options));
		msg = (struct dhcpv6_packet *)enc_opt_data.data;
		enc_packet->dhcpv6_msg_type = msg->msg_type;

		/* message-specific data */
		memcpy(enc_packet->dhcpv6_transaction_id,
		       msg->transaction_id,
		       sizeof(enc_packet->dhcpv6_transaction_id));

		if (!parse_option_buffer(enc_packet->options,
					 msg->options, 
					 enc_opt_data.len - msglen,
					 &dhcpv6_universe)) {
			/* no logging here, as parse_option_buffer() logs all
			   cases where it fails */
			goto exit;
		}
	}

	/*
	 * This is recursive. It is possible to exceed maximum packet size.
	 * XXX: This will cause the packet send to fail.
	 */
	build_dhcpv6_reply(&enc_reply, enc_packet);

	/*
	 * If we got no encapsulated data, then it is discarded, and
	 * our reply-forw is also discarded.
	 */
	if (enc_reply.data == NULL) {
		goto exit;
	}

	/*
	 * Now we can use the reply_data buffer.
	 * Packet header stuff all comes from the forward message.
	 */
	reply = (struct dhcpv6_relay_packet *)reply_data;
	reply->msg_type = DHCPV6_RELAY_REPL;
	reply->hop_count = packet->dhcpv6_hop_count;
	memcpy(reply->link_address, &packet->dhcpv6_link_address,
	       sizeof(reply->link_address));
	memcpy(reply->peer_address, &packet->dhcpv6_peer_address,
	       sizeof(reply->peer_address));
	reply_ofs = (int)(offsetof(struct dhcpv6_relay_packet, options));

	/*
	 * Get the reply option state.
	 */
	opt_state = NULL;
	if (!option_state_allocate(&opt_state, MDL)) {
		log_error("dhcpv6_relay_forw: no memory for option state.");
		goto exit;
	}

	/*
	 * Append the interface-id if present.
	 */
	oc = lookup_option(&dhcpv6_universe, packet->options,
			   D6O_INTERFACE_ID);
	if (oc != NULL) {
		if (!evaluate_option_cache(&a_opt, packet,
					   NULL, NULL, 
					   packet->options, NULL,
					   &global_scope, oc, MDL)) {
			log_error("dhcpv6_relay_forw: error evaluating "
				  "Interface ID.");
			goto exit;
		}
		if (!save_option_buffer(&dhcpv6_universe, opt_state, NULL,
					(unsigned char *)a_opt.data,
					a_opt.len,
					D6O_INTERFACE_ID, 0)) {
			log_error("dhcpv6_relay_forw: error saving "
				  "Interface ID.");
			goto exit;
		}
		data_string_forget(&a_opt, MDL);
	}

	/* 
	 * Append our encapsulated stuff for caller.
	 */
	if (!save_option_buffer(&dhcpv6_universe, opt_state, NULL,
				(unsigned char *)enc_reply.data,
				enc_reply.len,
				D6O_RELAY_MSG, 0)) {
		log_error("dhcpv6_relay_forw: error saving Relay MSG.");
		goto exit;
	}

	/*
	 * Get the ERO if any.
	 */
	oc = lookup_option(&dhcpv6_universe, packet->options, D6O_ERO);
	if (oc != NULL) {
		unsigned req;
		int i;

		if (!evaluate_option_cache(&packet_ero, packet,
					   NULL, NULL,
					   packet->options, NULL,
					   &global_scope, oc, MDL) ||
			(packet_ero.len & 1)) {
			log_error("dhcpv6_relay_forw: error evaluating ERO.");
			goto exit;
		}

		/* Decode and apply the ERO. */
		for (i = 0; i < packet_ero.len; i += 2) {
			req = getUShort(packet_ero.data + i);
			/* Already in the reply? */
			oc = lookup_option(&dhcpv6_universe, opt_state, req);
			if (oc != NULL)
				continue;
			/* Get it from the packet if present. */
			oc = lookup_option(&dhcpv6_universe,
					   packet->options,
					   req);
			if (oc == NULL)
				continue;
			if (!evaluate_option_cache(&a_opt, packet,
						   NULL, NULL,
						   packet->options, NULL,
						   &global_scope, oc, MDL)) {
				log_error("dhcpv6_relay_forw: error "
					  "evaluating option %u.", req);
				goto exit;
			}
			if (!save_option_buffer(&dhcpv6_universe,
						opt_state,
						NULL,
						(unsigned char *)a_opt.data,
						a_opt.len,
						req,
						0)) {
				log_error("dhcpv6_relay_forw: error saving "
					  "option %u.", req);
				goto exit;
			}
			data_string_forget(&a_opt, MDL);
		}
	}

	reply_ofs += store_options6(reply_data + reply_ofs,
				    sizeof(reply_data) - reply_ofs,
				    opt_state, packet,
				    required_opts_agent, &packet_ero);

	/*
	 * Return our reply to the caller.
	 */
	reply_ret->len = reply_ofs;
	reply_ret->buffer = NULL;
	if (!buffer_allocate(&reply_ret->buffer, reply_ret->len, MDL)) {
		log_fatal("No memory to store reply.");
	}
	reply_ret->data = reply_ret->buffer->data;
	memcpy(reply_ret->buffer->data, reply_data, reply_ofs);

exit:
	if (opt_state != NULL)
		option_state_dereference(&opt_state, MDL);
	if (a_opt.data != NULL) {
		data_string_forget(&a_opt, MDL);
	}
	if (packet_ero.data != NULL) {
		data_string_forget(&packet_ero, MDL);
	}
	if (enc_reply.data != NULL) {
		data_string_forget(&enc_reply, MDL);
	}
	if (enc_opt_data.data != NULL) {
		data_string_forget(&enc_opt_data, MDL);
	}
	if (enc_packet != NULL) {
		packet_dereference(&enc_packet, MDL);
	}
}

static void
dhcpv6_discard(struct packet *packet) {
	/* INSIST(packet->msg_type > 0); */
	/* INSIST(packet->msg_type < dhcpv6_type_name_max); */

	log_debug("Discarding %s from %s; message type not handled by server", 
		  dhcpv6_type_names[packet->dhcpv6_msg_type],
		  piaddr(packet->client_addr));
}

static void 
build_dhcpv6_reply(struct data_string *reply, struct packet *packet) {
	memset(reply, 0, sizeof(*reply));

	/* I would like to classify the client once here, but
	 * as I don't want to classify all of the incoming packets
	 * I need to do it before handling specific types.
	 * We don't need to classify if we are tossing the packet
	 * or if it is a relay - the classification step will get
	 * done when we process the inner client packet.
	 */

	switch (packet->dhcpv6_msg_type) {
		case DHCPV6_SOLICIT:
			classify_client(packet);
			dhcpv6_solicit(reply, packet);
			break;
		case DHCPV6_ADVERTISE:
			dhcpv6_discard(packet);
			break;
		case DHCPV6_REQUEST:
			classify_client(packet);
			dhcpv6_request(reply, packet);
			break;
		case DHCPV6_CONFIRM:
			classify_client(packet);
			dhcpv6_confirm(reply, packet);
			break;
		case DHCPV6_RENEW:
			classify_client(packet);
			dhcpv6_renew(reply, packet);
			break;
		case DHCPV6_REBIND:
			classify_client(packet);
			dhcpv6_rebind(reply, packet);
			break;
		case DHCPV6_REPLY:
			dhcpv6_discard(packet);
			break;
		case DHCPV6_RELEASE:
			classify_client(packet);
			dhcpv6_release(reply, packet);
			break;
		case DHCPV6_DECLINE:
			classify_client(packet);
			dhcpv6_decline(reply, packet);
			break;
		case DHCPV6_RECONFIGURE:
			dhcpv6_discard(packet);
			break;
		case DHCPV6_INFORMATION_REQUEST:
			classify_client(packet);
			dhcpv6_information_request(reply, packet);
			break;
		case DHCPV6_RELAY_FORW:
			dhcpv6_relay_forw(reply, packet);
			break;
		case DHCPV6_RELAY_REPL:
			dhcpv6_discard(packet);
			break;
		case DHCPV6_LEASEQUERY:
			classify_client(packet);
			dhcpv6_leasequery(reply, packet);
			break;
		case DHCPV6_LEASEQUERY_REPLY:
			dhcpv6_discard(packet);
			break;
		default:
			/* XXX: would be nice if we had "notice" level, 
				as syslog, for this */
			log_info("Discarding unknown DHCPv6 message type %d "
				 "from %s", packet->dhcpv6_msg_type, 
				 piaddr(packet->client_addr));
	}
}

static void
log_packet_in(const struct packet *packet) {
	struct data_string s;
	u_int32_t tid;
	char tmp_addr[INET6_ADDRSTRLEN];
	const void *addr;

	memset(&s, 0, sizeof(s));

	if (packet->dhcpv6_msg_type < dhcpv6_type_name_max) {
		data_string_sprintfa(&s, "%s message from %s port %d",
				     dhcpv6_type_names[packet->dhcpv6_msg_type],
				     piaddr(packet->client_addr),
				     ntohs(packet->client_port));
	} else {
		data_string_sprintfa(&s, 
				     "Unknown message type %d from %s port %d",
				     packet->dhcpv6_msg_type,
				     piaddr(packet->client_addr),
				     ntohs(packet->client_port));
	}
	if ((packet->dhcpv6_msg_type == DHCPV6_RELAY_FORW) || 
	    (packet->dhcpv6_msg_type == DHCPV6_RELAY_REPL)) {
	    	addr = &packet->dhcpv6_link_address;
	    	data_string_sprintfa(&s, ", link address %s", 
				     inet_ntop(AF_INET6, addr, 
					       tmp_addr, sizeof(tmp_addr)));
	    	addr = &packet->dhcpv6_peer_address;
	    	data_string_sprintfa(&s, ", peer address %s", 
				     inet_ntop(AF_INET6, addr, 
					       tmp_addr, sizeof(tmp_addr)));
	} else {
		tid = 0;
		memcpy(((char *)&tid)+1, packet->dhcpv6_transaction_id, 3);
		data_string_sprintfa(&s, ", transaction ID 0x%06X", tid);

/*
		oc = lookup_option(&dhcpv6_universe, packet->options, 
				   D6O_CLIENTID);
		if (oc != NULL) {
			memset(&tmp_ds, 0, sizeof(tmp_ds_));
			if (!evaluate_option_cache(&tmp_ds, packet, NULL, NULL, 
						   packet->options, NULL,
						   &global_scope, oc, MDL)) {
				log_error("Error evaluating Client Identifier");
			} else {
				data_strint_sprintf(&s, ", client ID %s",

				data_string_forget(&tmp_ds, MDL);
			}
		}
*/

	}
	log_info("%s", s.data);

	data_string_forget(&s, MDL);
}

void 
dhcpv6(struct packet *packet) {
	struct data_string reply;
	struct sockaddr_in6 to_addr;
	int send_ret;

	/* 
	 * Log a message that we received this packet.
	 */
	log_packet_in(packet); 

	/*
	 * Build our reply packet.
	 */
	build_dhcpv6_reply(&reply, packet);

	if (reply.data != NULL) {
		/* 
		 * Send our reply, if we have one.
		 */
		memset(&to_addr, 0, sizeof(to_addr));
		to_addr.sin6_family = AF_INET6;
		if ((packet->dhcpv6_msg_type == DHCPV6_RELAY_FORW) || 
		    (packet->dhcpv6_msg_type == DHCPV6_RELAY_REPL)) {
			to_addr.sin6_port = local_port;
		} else {
			to_addr.sin6_port = remote_port;
		}

#if defined (REPLY_TO_SOURCE_PORT)
		/*
		 * This appears to have been included for testing so we would
		 * not need a root client, but was accidently left in the
		 * final code.  We continue to include it in case
		 * some users have come to rely upon it, but leave
		 * it off by default as it's a bad idea.
		 */
		to_addr.sin6_port = packet->client_port;
#endif

		memcpy(&to_addr.sin6_addr, packet->client_addr.iabuf, 
		       sizeof(to_addr.sin6_addr));

		log_info("Sending %s to %s port %d", 
			 dhcpv6_type_names[reply.data[0]],
			 piaddr(packet->client_addr),
			 ntohs(to_addr.sin6_port));

		send_ret = send_packet6(packet->interface, 
					reply.data, reply.len, &to_addr);
		if (send_ret != reply.len) {
			log_error("dhcpv6: send_packet6() sent %d of %d bytes",
				  send_ret, reply.len);
		}
		data_string_forget(&reply, MDL);
	}
}

static void
seek_shared_host(struct host_decl **hp, struct shared_network *shared) {
	struct host_decl *nofixed = NULL;
	struct host_decl *seek, *hold = NULL;

	/*
	 * Seek forward through fixed addresses for the right link.
	 *
	 * Note: how to do this for fixed prefixes???
	 */
	host_reference(&hold, *hp, MDL);
	host_dereference(hp, MDL);
	seek = hold;
	while (seek != NULL) {
		if (seek->fixed_addr == NULL)
			nofixed = seek;
		else if (fixed_matches_shared(seek, shared))
			break;

		seek = seek->n_ipaddr;
	}

	if ((seek == NULL) && (nofixed != NULL))
		seek = nofixed;

	if (seek != NULL)
		host_reference(hp, seek, MDL);
}

static isc_boolean_t
fixed_matches_shared(struct host_decl *host, struct shared_network *shared) {
	struct subnet *subnet;
	struct data_string addr;
	isc_boolean_t matched;
	struct iaddr fixed;

	if (host->fixed_addr == NULL)
		return ISC_FALSE;

	memset(&addr, 0, sizeof(addr));
	if (!evaluate_option_cache(&addr, NULL, NULL, NULL, NULL, NULL,
				   &global_scope, host->fixed_addr, MDL))
		return ISC_FALSE;

	if (addr.len < 16) {
		data_string_forget(&addr, MDL);
		return ISC_FALSE;
	}

	fixed.len = 16;
	memcpy(fixed.iabuf, addr.data, 16);

	matched = ISC_FALSE;
	for (subnet = shared->subnets ; subnet != NULL ;
	     subnet = subnet->next_sibling) {
		if (addr_eq(subnet_number(fixed, subnet->netmask),
			    subnet->net)) {
			matched = ISC_TRUE;
			break;
		}
	}

	data_string_forget(&addr, MDL);
	return matched;
}

/*
 * find_host_by_duid_chaddr() synthesizes a DHCPv4-like 'hardware'
 * parameter from a DHCPv6 supplied DUID (client-identifier option),
 * and may seek to use client or relay supplied hardware addresses.
 */
static int
find_hosts_by_duid_chaddr(struct host_decl **host,
			  const struct data_string *client_id) {
	static int once_htype;
	int htype, hlen;
	const unsigned char *chaddr;

	/*
	 * The DUID-LL and DUID-LLT must have a 2-byte DUID type and 2-byte
	 * htype.
	 */
	if (client_id->len < 4)
		return 0;

	/*
	 * The third and fourth octets of the DUID-LL and DUID-LLT
	 * is the hardware type, but in 16 bits.
	 */
	htype = getUShort(client_id->data + 2);
	hlen = 0;
	chaddr = NULL;

	/* The first two octets of the DUID identify the type. */
	switch(getUShort(client_id->data)) {
	      case DUID_LLT:
		if (client_id->len > 8) {
			hlen = client_id->len - 8;
			chaddr = client_id->data + 8;
		}
		break;

	      case DUID_LL:
		/*
		 * Note that client_id->len must be greater than or equal
		 * to four to get to this point in the function.
		 */
		hlen = client_id->len - 4;
		chaddr = client_id->data + 4;
		break;

	      default:
		break;
	}

	if ((hlen == 0) || (hlen > HARDWARE_ADDR_LEN)) 
		return 0;

	/*
	 * XXX: DHCPv6 gives a 16-bit field for the htype.  DHCPv4 gives an
	 * 8-bit field.  To change the semantics of the generic 'hardware'
	 * structure, we would have to adjust many DHCPv4 sources (from
	 * interface to DHCPv4 lease code), and we would have to update the
	 * 'hardware' config directive (probably being reverse compatible and
	 * providing a new upgrade/replacement primitive).  This is a little
	 * too much to change for now.  Hopefully we will revisit this before
	 * hardware types exceeding 8 bits are assigned.
	 */
	if ((htype & 0xFF00) && !once_htype) {
		once_htype = 1;
		log_error("Attention: At least one client advertises a "
			  "hardware type of %d, which exceeds the software "
			  "limitation of 255.", htype);
	}

	return find_hosts_by_haddr(host, htype, chaddr, hlen, MDL);
}

#endif /* DHCPv6 */

