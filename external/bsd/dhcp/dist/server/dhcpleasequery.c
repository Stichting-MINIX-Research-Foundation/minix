/*	$NetBSD: dhcpleasequery.c,v 1.5 2014/07/12 12:09:38 spz Exp $	*/
/*
 * Copyright (C) 2011-2013 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2006-2007,2009 by Internet Systems Consortium, Inc. ("ISC")
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
__RCSID("$NetBSD: dhcpleasequery.c,v 1.5 2014/07/12 12:09:38 spz Exp $");

#include "dhcpd.h"

/*
 * TODO: RFC4388 specifies that the server SHOULD return the same
 *       options it would for a DHCREQUEST message, if no Parameter
 *       Request List option (option 55) is passed. We do not do that.
 *
 * TODO: RFC4388 specifies the creation of a "non-sensitive options"
 *       configuration list, and that these SHOULD be returned. We
 *       have no such list.
 *
 * TODO: RFC4388 says the server SHOULD use RFC3118, "Authentication
 *       for DHCP Messages".
 *
 * TODO: RFC4388 specifies that you SHOULD insure that you cannot be
 *       DoS'ed by DHCPLEASEQUERY message.
 */

/* 
 * If you query by hardware address or by client ID, then you may have
 * more than one IP address for your query argument. We need to do two
 * things:
 *
 *   1. Find the most recent lease.
 *   2. Find all additional IP addresses for the query argument.
 *
 * We do this by looking through all of the leases associated with a
 * given hardware address or client ID. We use the cltt (client last
 * transaction time) of the lease, which only has a resolution of one
 * second, so we might not actually give the very latest IP.
 */

static struct lease*
next_hw(const struct lease *lease) {
	/* INSIST(lease != NULL); */
	return lease->n_hw;
}

static struct lease*
next_uid(const struct lease *lease) {
	/* INSIST(lease != NULL); */
	return lease->n_uid;
}

static void
get_newest_lease(struct lease **retval,
		 struct lease *lease,
		 struct lease *(*next)(const struct lease *)) {

	struct lease *p;
	struct lease *newest;

	/* INSIST(newest != NULL); */
	/* INSIST(next != NULL); */

	*retval = NULL;

	if (lease == NULL) {
		return;
	}

	newest = lease;
	for (p=next(lease); p != NULL; p=next(p)) {
		if (newest->binding_state == FTS_ACTIVE) {
			if ((p->binding_state == FTS_ACTIVE) && 
		    	(p->cltt > newest->cltt)) {
				newest = p;
			}
		} else {
			if (p->ends > newest->ends) {
				newest = p;
			}
		}
	}

	lease_reference(retval, newest, MDL);
}

static int
get_associated_ips(const struct lease *lease,
		   struct lease *(*next)(const struct lease *), 
		   const struct lease *newest,
		   u_int32_t *associated_ips,
		   unsigned int associated_ips_size) {

	const struct lease *p;
	int cnt;

	/* INSIST(next != NULL); */
	/* INSIST(associated_ips != NULL); */

	if (lease == NULL) {
		return 0;
	}

	cnt = 0;
	for (p=lease; p != NULL; p=next(p)) {
		if ((p->binding_state == FTS_ACTIVE) && (p != newest)) {
			if (cnt < associated_ips_size) {
				memcpy(&associated_ips[cnt],
				       p->ip_addr.iabuf,
				       sizeof(associated_ips[cnt]));
			}
			cnt++;
		}
	}
	return cnt;
}


void 
dhcpleasequery(struct packet *packet, int ms_nulltp) {
	char msgbuf[256];
	char dbg_info[128];
	struct iaddr cip;
	struct iaddr gip;
	struct data_string uid;
	struct hardware h;
	struct lease *tmp_lease;
	struct lease *lease;
	int want_associated_ip;
	int assoc_ip_cnt;
	u_int32_t assoc_ips[40];  /* XXXSK: arbitrary maximum number of IPs */
	const int nassoc_ips = sizeof(assoc_ips) / sizeof(assoc_ips[0]);

	unsigned char dhcpMsgType;
	const char *dhcp_msg_type_name;
	struct subnet *subnet;
	struct group *relay_group;
	struct option_state *options;
	struct option_cache *oc;
	int allow_leasequery;
	int ignorep;
	u_int32_t lease_duration;
	u_int32_t time_renewal;
	u_int32_t time_rebinding;
	u_int32_t time_expiry;
	u_int32_t client_last_transaction_time;
	struct sockaddr_in to;
	struct in_addr siaddr;
	struct data_string prl;
	struct data_string *prl_ptr;

	int i;
	struct interface_info *interface;

	/* INSIST(packet != NULL); */

	/*
	 * Prepare log information.
	 */
	snprintf(msgbuf, sizeof(msgbuf), 
		"DHCPLEASEQUERY from %s", inet_ntoa(packet->raw->giaddr));

	/* 
	 * We can't reply if there is no giaddr field.
	 */
	if (!packet->raw->giaddr.s_addr) {
		log_info("%s: missing giaddr, ciaddr is %s, no reply sent", 
			 msgbuf, inet_ntoa(packet->raw->ciaddr));
		return;
	}

	/* 
	 * Initially we use the 'giaddr' subnet options scope to determine if
	 * the giaddr-identified relay agent is permitted to perform a
	 * leasequery.  The subnet is not required, and may be omitted, in
	 * which case we are essentially interrogating the root options class
	 * to find a globally permit.
	 */
	gip.len = sizeof(packet->raw->giaddr);
	memcpy(gip.iabuf, &packet->raw->giaddr, sizeof(packet->raw->giaddr));

	subnet = NULL;
	find_subnet(&subnet, gip, MDL);
	if (subnet != NULL)
		relay_group = subnet->group;
	else
		relay_group = root_group;

	subnet_dereference(&subnet, MDL);

	options = NULL;
	if (!option_state_allocate(&options, MDL)) {
		log_error("No memory for option state.");
		log_info("%s: out of memory, no reply sent", msgbuf);
		return;
	}

	execute_statements_in_scope(NULL, packet, NULL, NULL, packet->options,
				    options, &global_scope, relay_group,
				    NULL, NULL);

	for (i=packet->class_count-1; i>=0; i--) {
		execute_statements_in_scope(NULL, packet, NULL, NULL,
					    packet->options, options,
					    &global_scope,
					    packet->classes[i]->group,
					    relay_group, NULL);
	}

	/* 
	 * Because LEASEQUERY has some privacy concerns, default to deny.
	 */
	allow_leasequery = 0;

	/*
	 * See if we are authorized to do LEASEQUERY.
	 */
	oc = lookup_option(&server_universe, options, SV_LEASEQUERY);
	if (oc != NULL) {
		allow_leasequery = evaluate_boolean_option_cache(&ignorep,
					 packet, NULL, NULL, packet->options,
					 options, &global_scope, oc, MDL);
	}

	if (!allow_leasequery) {
		log_info("%s: LEASEQUERY not allowed, query ignored", msgbuf);
		option_state_dereference(&options, MDL);
		return;
	}


	/* 
	 * Copy out the client IP address.
	 */
	cip.len = sizeof(packet->raw->ciaddr);
	memcpy(cip.iabuf, &packet->raw->ciaddr, sizeof(packet->raw->ciaddr));

	/* 
	 * If the client IP address is valid (not all zero), then we 
	 * are looking for information about that IP address.
	 */
	assoc_ip_cnt = 0;
	lease = tmp_lease = NULL;
	if (memcmp(cip.iabuf, "\0\0\0", 4)) {

		want_associated_ip = 0;

		snprintf(dbg_info, sizeof(dbg_info), "IP %s", piaddr(cip));
		find_lease_by_ip_addr(&lease, cip, MDL);


	} else {

		want_associated_ip = 1;

		/*
		 * If the client IP address is all zero, then we will
		 * either look up by the client identifier (if we have
		 * one), or by the MAC address.
		 */

		memset(&uid, 0, sizeof(uid));
		if (get_option(&uid, 
			       &dhcp_universe,
			       packet,
			       NULL,
			       NULL,
			       packet->options,
			       NULL,
			       packet->options, 
			       &global_scope,
			       DHO_DHCP_CLIENT_IDENTIFIER,
			       MDL)) {

			snprintf(dbg_info, 
				 sizeof(dbg_info), 
				 "client-id %s",
				 print_hex_1(uid.len, uid.data, 60));

			find_lease_by_uid(&tmp_lease, uid.data, uid.len, MDL);
			data_string_forget(&uid, MDL);
			get_newest_lease(&lease, tmp_lease, next_uid);
			assoc_ip_cnt = get_associated_ips(tmp_lease,
							  next_uid, 
							  lease,
							  assoc_ips, 
							  nassoc_ips);

		} else {

			if (packet->raw->hlen+1 > sizeof(h.hbuf)) {
				log_info("%s: hardware length too long, "
					 "no reply sent", msgbuf);
				option_state_dereference(&options, MDL);
				return;
			}

			h.hlen = packet->raw->hlen + 1;
			h.hbuf[0] = packet->raw->htype;
			memcpy(&h.hbuf[1], 
			       packet->raw->chaddr, 
			       packet->raw->hlen);

			snprintf(dbg_info, 
				 sizeof(dbg_info), 
				 "MAC address %s",
				 print_hw_addr(h.hbuf[0], 
					       h.hlen - 1, 
					       &h.hbuf[1]));

			find_lease_by_hw_addr(&tmp_lease, h.hbuf, h.hlen, MDL);
			get_newest_lease(&lease, tmp_lease, next_hw);
			assoc_ip_cnt = get_associated_ips(tmp_lease,
							  next_hw, 
							  lease,
							  assoc_ips, 
							  nassoc_ips);

		}

		lease_dereference(&tmp_lease, MDL);

		if (lease != NULL) {
			memcpy(&packet->raw->ciaddr, 
			       lease->ip_addr.iabuf,
			       sizeof(packet->raw->ciaddr));
		}

		/*
		 * Log if we have too many IP addresses associated
		 * with this client.
		 */
		if (want_associated_ip && (assoc_ip_cnt > nassoc_ips)) {
			log_info("%d IP addresses associated with %s, "
				 "only %d sent in reply.",
				 assoc_ip_cnt, dbg_info, nassoc_ips);
		}
	}

	/*
	 * We now know the query target too, so can report this in 
	 * our log message.
	 */
	snprintf(msgbuf, sizeof(msgbuf), 
		"DHCPLEASEQUERY from %s for %s",
		inet_ntoa(packet->raw->giaddr), dbg_info);

	/*
	 * Figure our our return type.
	 */
	if (lease == NULL) {
		dhcpMsgType = DHCPLEASEUNKNOWN;
		dhcp_msg_type_name = "DHCPLEASEUNKNOWN";
	} else {
		if (lease->binding_state == FTS_ACTIVE) {
			dhcpMsgType = DHCPLEASEACTIVE;
			dhcp_msg_type_name = "DHCPLEASEACTIVE";
		} else {
			dhcpMsgType = DHCPLEASEUNASSIGNED;
			dhcp_msg_type_name = "DHCPLEASEUNASSIGNED";
		}
	}

	/* 
	 * Set options that only make sense if we have an active lease.
	 */

	if (dhcpMsgType == DHCPLEASEACTIVE)
	{
		/*
		 * RFC 4388 uses the PRL to request options for the agent to
		 * receive that are "about" the client.  It is confusing
		 * because in some cases it wants to know what was sent to
		 * the client (lease times, adjusted), and in others it wants
		 * to know information the client sent.  You're supposed to
		 * know this on a case-by-case basis.
		 *
		 * "Name servers", "domain name", and the like from the relay
		 * agent's scope seems less than useful.  Our options are to
		 * restart the option cache from the lease's best point of view
		 * (execute statements from the lease pool's group), or to
		 * simply restart the option cache from empty.
		 *
		 * I think restarting the option cache from empty best
		 * approaches RFC 4388's intent; specific options are included.
		 */
		option_state_dereference(&options, MDL);

		if (!option_state_allocate(&options, MDL)) {
			log_error("%s: out of memory, no reply sent", msgbuf);
			lease_dereference(&lease, MDL);
			return;
		}

		/* 
		 * Set the hardware address fields.
		 */

		packet->raw->hlen = lease->hardware_addr.hlen - 1;
		packet->raw->htype = lease->hardware_addr.hbuf[0];
		memcpy(packet->raw->chaddr, 
		       &lease->hardware_addr.hbuf[1], 
		       sizeof(packet->raw->chaddr));

		/*
		 * Set client identifier option.
		 */
		if (lease->uid_len > 0) {
			if (!add_option(options,
					DHO_DHCP_CLIENT_IDENTIFIER,
					lease->uid,
					lease->uid_len)) {
				option_state_dereference(&options, MDL);
				lease_dereference(&lease, MDL);
				log_info("%s: out of memory, no reply sent",
					 msgbuf);
				return;
			}
		}


		/*
		 * Calculate T1 and T2, the times when the client
		 * tries to extend its lease on its networking
		 * address.
		 * These seem to be hard-coded in ISC DHCP, to 0.5 and
		 * 0.875 of the lease time.
		 */

		lease_duration = lease->ends - lease->starts;
		time_renewal = lease->starts + 
			(lease_duration / 2);
		time_rebinding = lease->starts + 
			(lease_duration / 2) +
			(lease_duration / 4) +
			(lease_duration / 8);

		if (time_renewal > cur_time) {
			time_renewal = htonl(time_renewal - cur_time);

			if (!add_option(options, 
					DHO_DHCP_RENEWAL_TIME,
					&time_renewal, 
					sizeof(time_renewal))) {
				option_state_dereference(&options, MDL);
				lease_dereference(&lease, MDL);
				log_info("%s: out of memory, no reply sent",
					 msgbuf);
				return;
			}
		}

		if (time_rebinding > cur_time) {
			time_rebinding = htonl(time_rebinding - cur_time);

			if (!add_option(options, 
					DHO_DHCP_REBINDING_TIME,
					&time_rebinding, 
					sizeof(time_rebinding))) {
				option_state_dereference(&options, MDL);
				lease_dereference(&lease, MDL);
				log_info("%s: out of memory, no reply sent",
					 msgbuf);
				return;
			}
		}

		if (lease->ends > cur_time) {
			time_expiry = htonl(lease->ends - cur_time);

			if (!add_option(options, 
					DHO_DHCP_LEASE_TIME,
					&time_expiry, 
					sizeof(time_expiry))) {
				option_state_dereference(&options, MDL);
				lease_dereference(&lease, MDL);
				log_info("%s: out of memory, no reply sent",
					 msgbuf);
				return;
			}
		}

		/* Supply the Vendor-Class-Identifier. */
		if (lease->scope != NULL) {
			struct data_string vendor_class;

			memset(&vendor_class, 0, sizeof(vendor_class));

			if (find_bound_string(&vendor_class, lease->scope,
					      "vendor-class-identifier")) {
				if (!add_option(options,
						DHO_VENDOR_CLASS_IDENTIFIER,
						(void *)vendor_class.data,
						vendor_class.len)) {
					option_state_dereference(&options,
								 MDL);
					lease_dereference(&lease, MDL);
					log_error("%s: error adding vendor "
						  "class identifier, no reply "
						  "sent", msgbuf);
					data_string_forget(&vendor_class, MDL);
					return;
				}
				data_string_forget(&vendor_class, MDL);
			}
		}

		/*
		 * Set the relay agent info.
		 *
		 * Note that because agent info is appended without regard
		 * to the PRL in cons_options(), this will be sent as the
		 * last option in the packet whether it is listed on PRL or
		 * not.
		 */

		if (lease->agent_options != NULL) {
			int idx = agent_universe.index;
			struct option_chain_head **tmp1 = 
				(struct option_chain_head **)
				&(options->universes[idx]);
				struct option_chain_head *tmp2 = 
				(struct option_chain_head *)
				lease->agent_options;

			option_chain_head_reference(tmp1, tmp2, MDL);
		}

		/* 
	 	 * Set the client last transaction time.
		 * We check to make sure we have a timestamp. For
		 * lease files that were saved before running a 
		 * timestamp-aware version of the server, this may
		 * not be set.
	 	 */

		if (lease->cltt != MIN_TIME) {
			if (cur_time > lease->cltt) {
				client_last_transaction_time = 
					htonl(cur_time - lease->cltt);
			} else {
				client_last_transaction_time = htonl(0);
			}
			if (!add_option(options, 
					DHO_CLIENT_LAST_TRANSACTION_TIME,
					&client_last_transaction_time,
		     			sizeof(client_last_transaction_time))) {
				option_state_dereference(&options, MDL);
				lease_dereference(&lease, MDL);
				log_info("%s: out of memory, no reply sent",
					 msgbuf);
				return;
			}
		}

		/*
	 	 * Set associated IPs, if requested and there are some.
	 	 */
		if (want_associated_ip && (assoc_ip_cnt > 0)) {
			if (!add_option(options, 
					DHO_ASSOCIATED_IP,
					assoc_ips,
					assoc_ip_cnt * sizeof(assoc_ips[0]))) {
				option_state_dereference(&options, MDL);
				lease_dereference(&lease, MDL);
				log_info("%s: out of memory, no reply sent",
					 msgbuf);
				return;
			}
		}
	}

	/* 
	 * Set the message type.
	 */

	packet->raw->op = BOOTREPLY;

	/*
	 * Set DHCP message type.
	 */
	if (!add_option(options, 
		        DHO_DHCP_MESSAGE_TYPE,
		        &dhcpMsgType, 
			sizeof(dhcpMsgType))) {
		option_state_dereference(&options, MDL);
		lease_dereference(&lease, MDL);
		log_info("%s: error adding option, no reply sent", msgbuf);
		return;
	}

	/*
	 * Log the message we've received.
	 */
	log_info("%s", msgbuf);

	/*
	 * Figure out which address to use to send from.
	 */
	get_server_source_address(&siaddr, options, options, packet);

	/* 
	 * Set up the option buffer.
	 */

	memset(&prl, 0, sizeof(prl));
	oc = lookup_option(&dhcp_universe, options, 
			   DHO_DHCP_PARAMETER_REQUEST_LIST);
	if (oc != NULL) {
		evaluate_option_cache(&prl, 
				      packet, 
				      NULL,
				      NULL,
				      packet->options,
				      options,
				      &global_scope,
				      oc,
				      MDL);
	}
	if (prl.len > 0) {
		prl_ptr = &prl;
	} else {
		prl_ptr = NULL;
	}

	packet->packet_length = cons_options(packet, 
					     packet->raw, 
					     lease,
					     NULL,
					     0,
					     packet->options,
					     options,
					     &global_scope,
					     0,
					     0,
					     0, 
					     prl_ptr,
					     NULL);

	data_string_forget(&prl, MDL);	/* SK: safe, even if empty */
	option_state_dereference(&options, MDL);
	lease_dereference(&lease, MDL);

	to.sin_family = AF_INET;
#ifdef HAVE_SA_LEN
	to.sin_len = sizeof(to);
#endif
	memset(to.sin_zero, 0, sizeof(to.sin_zero));

	/* 
	 * Leasequery packets are be sent to the gateway address.
	 */
	to.sin_addr = packet->raw->giaddr;
	if (packet->raw->giaddr.s_addr != htonl(INADDR_LOOPBACK)) {
		to.sin_port = local_port;
	} else {
		to.sin_port = remote_port; /* XXXSK: For debugging. */
	}

	/* 
	 * The fallback_interface lets us send with a real IP
	 * address. The packet interface sends from all-zeros.
	 */
	if (fallback_interface != NULL) {
		interface = fallback_interface;
	} else {
		interface = packet->interface;
	}

	/*
	 * Report what we're sending.
	 */
	log_info("%s to %s for %s (%d associated IPs)",
		dhcp_msg_type_name, 
		inet_ntoa(to.sin_addr), dbg_info, assoc_ip_cnt);

	send_packet(interface,
		    NULL,
		    packet->raw, 
		    packet->packet_length,
		    siaddr,
		    &to,
		    NULL);
}

#ifdef DHCPv6

/*
 * TODO: RFC5007 query-by-clientid.
 *
 * TODO: RFC5007 look at the pools according to the link-address.
 *
 * TODO: get fixed leases too.
 *
 * TODO: RFC5007 ORO in query-options.
 *
 * TODO: RFC5007 lq-relay-data.
 *
 * TODO: RFC5007 lq-client-link.
 *
 * Note: the code is still nearly compliant and usable for the target
 * case with these missing features!
 */

/*
 * The structure to handle a leasequery.
 */
struct lq6_state {
	struct packet *packet;
	struct data_string client_id;
	struct data_string server_id;
	struct data_string lq_query;
	uint8_t query_type;
	struct in6_addr link_addr;
	struct option_state *query_opts;

	struct option_state *reply_opts;
	unsigned cursor;
	union reply_buffer {
		unsigned char data[65536];
		struct dhcpv6_packet reply;
	} buf;
};

/*
 * Options that we want to send.
 */
static const int required_opts_lq[] = {
	D6O_CLIENTID,
	D6O_SERVERID,
	D6O_STATUS_CODE,
	D6O_CLIENT_DATA,
	D6O_LQ_RELAY_DATA,
	D6O_LQ_CLIENT_LINK,
	0
};
static const int required_opt_CLIENT_DATA[] = {
	D6O_CLIENTID,
	D6O_IAADDR,
	D6O_IAPREFIX,
	D6O_CLT_TIME,
	0
};

/*
 * Get the lq-query option from the packet.
 */
static isc_result_t
get_lq_query(struct lq6_state *lq)
{
	struct data_string *lq_query = &lq->lq_query;
	struct packet *packet = lq->packet;
	struct option_cache *oc;

	/*
	 * Verify our lq_query structure is empty.
	 */
	if ((lq_query->data != NULL) || (lq_query->len != 0)) {
		return DHCP_R_INVALIDARG;
	}

	oc = lookup_option(&dhcpv6_universe, packet->options, D6O_LQ_QUERY);
	if (oc == NULL) {
		return ISC_R_NOTFOUND;
	}

	if (!evaluate_option_cache(lq_query, packet, NULL, NULL,
				   packet->options, NULL,
				   &global_scope, oc, MDL)) {
		return ISC_R_FAILURE;
	}

	return ISC_R_SUCCESS;
}

/*
 * Message validation, RFC 5007 section 4.2.1:
 *  dhcpv6.c:valid_client_msg() - unicast + lq-query option.
 */
static int
valid_query_msg(struct lq6_state *lq) {
	struct packet *packet = lq->packet;
	int ret_val = 0;
	struct option_cache *oc;

	/* INSIST((lq != NULL) || (packet != NULL)); */

	switch (get_client_id(packet, &lq->client_id)) {
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
	if (oc != NULL) {
		if (evaluate_option_cache(&lq->server_id, packet, NULL, NULL,
					  packet->options, NULL, 
					  &global_scope, oc, MDL)) {
			log_debug("Discarding %s from %s; " 
				  "server identifier found "
				  "(CLIENTID %s, SERVERID %s)", 
				  dhcpv6_type_names[packet->dhcpv6_msg_type],
				  piaddr(packet->client_addr),
				  print_hex_1(lq->client_id.len, 
				  	      lq->client_id.data, 60),
				  print_hex_2(lq->server_id.len,
				  	      lq->server_id.data, 60));
		} else {
			log_debug("Discarding %s from %s; " 
				  "server identifier found "
				  "(CLIENTID %s)", 
				  dhcpv6_type_names[packet->dhcpv6_msg_type],
				  print_hex_1(lq->client_id.len, 
				  	      lq->client_id.data, 60),
				  piaddr(packet->client_addr));
		}
		goto exit;
	}

	switch (get_lq_query(lq)) {
		case ISC_R_SUCCESS:
			break;
		case ISC_R_NOTFOUND:
			log_debug("Discarding %s from %s; lq-query missing",
				  dhcpv6_type_names[packet->dhcpv6_msg_type],
				  piaddr(packet->client_addr));
			goto exit;
		default:
			log_error("Error processing %s from %s; "
				  "unable to evaluate LQ-Query",
				  dhcpv6_type_names[packet->dhcpv6_msg_type],
				  piaddr(packet->client_addr));
			goto exit;
	}

	/* looks good */
	ret_val = 1;

exit:
	if (!ret_val) {
		if (lq->client_id.len > 0) {
			data_string_forget(&lq->client_id, MDL);
		}
		if (lq->server_id.len > 0) {
			data_string_forget(&lq->server_id, MDL);
		}
		if (lq->lq_query.len > 0) {
			data_string_forget(&lq->lq_query, MDL);
		}
	}
	return ret_val;
}

/*
 * Set an error in a status-code option (from set_status_code).
 */
static int
set_error(struct lq6_state *lq, u_int16_t code, const char *message) {
	struct data_string d;
	int ret_val;

	memset(&d, 0, sizeof(d));
	d.len = sizeof(code) + strlen(message);
	if (!buffer_allocate(&d.buffer, d.len, MDL)) {
		log_fatal("set_error: no memory for status code.");
	}
	d.data = d.buffer->data;
	putUShort(d.buffer->data, code);
	memcpy(d.buffer->data + sizeof(code), message, d.len - sizeof(code));
	if (!save_option_buffer(&dhcpv6_universe, lq->reply_opts,
				d.buffer, (unsigned char *)d.data, d.len, 
				D6O_STATUS_CODE, 0)) {
		log_error("set_error: error saving status code.");
		ret_val = 0;
	} else {
		ret_val = 1;
	}
	data_string_forget(&d, MDL);
	return ret_val;
}

/*
 * Process a by-address lease query.
 */
static int
process_lq_by_address(struct lq6_state *lq) {
	struct packet *packet = lq->packet;
	struct option_cache *oc;
	struct ipv6_pool *pool = NULL;
	struct data_string data;
	struct in6_addr addr;
	struct iasubopt *iaaddr = NULL;
	struct option_state *opt_state = NULL;
	u_int32_t lifetime;
	unsigned opt_cursor;
	int ret_val = 0;

	/*
	 * Get the IAADDR.
	 */
	oc = lookup_option(&dhcpv6_universe, lq->query_opts, D6O_IAADDR);
	if (oc == NULL) {
		if (!set_error(lq, STATUS_MalformedQuery,
			       "No OPTION_IAADDR.")) {
			log_error("process_lq_by_address: unable "
				  "to set MalformedQuery status code.");
			return 0;
		}
		return 1;
	}
	memset(&data, 0, sizeof(data));
	if (!evaluate_option_cache(&data, packet,
				   NULL, NULL,
				   lq->query_opts, NULL,
				   &global_scope, oc, MDL) ||
	    (data.len < IAADDR_OFFSET)) {
		log_error("process_lq_by_address: error evaluating IAADDR.");
		goto exit;
	}
	memcpy(&addr, data.data, sizeof(addr));
	data_string_forget(&data, MDL);

	/*
	 * Find the lease.
	 * Note the RFC 5007 says to use the link-address to find the link
	 * or the ia-aadr when it is :: but in any case the ia-addr has
	 * to be on the link, so we ignore the link-address here.
	 */
	if (find_ipv6_pool(&pool, D6O_IA_NA, &addr) != ISC_R_SUCCESS) {
		if (!set_error(lq, STATUS_NotConfigured,
			       "Address not in a pool.")) {
			log_error("process_lq_by_address: unable "
				  "to set NotConfigured status code.");
			goto exit;
		}
		ret_val = 1;
		goto exit;
	}
	if (iasubopt_hash_lookup(&iaaddr, pool->leases, &addr,
				 sizeof(addr), MDL) == 0) {
		ret_val = 1;
		goto exit;
	}
	if ((iaaddr == NULL) || (iaaddr->state != FTS_ACTIVE) ||
	    (iaaddr->ia == NULL) || (iaaddr->ia->iaid_duid.len <= 4)) {
		ret_val = 1;
		goto exit;
	}

	/*
	 * Build the client-data option (with client-id, ia-addr and clt-time).
	 */
	if (!option_state_allocate(&opt_state, MDL)) {
		log_error("process_lq_by_address: "
			  "no memory for option state.");
		goto exit;
	}

	data_string_copy(&data, &iaaddr->ia->iaid_duid, MDL);
	data.data += 4;
	data.len -= 4;
	if (!save_option_buffer(&dhcpv6_universe, opt_state,
				NULL, (unsigned char *)data.data, data.len,
				D6O_CLIENTID, 0)) {
		log_error("process_lq_by_address: error saving client ID.");
		goto exit;
	}
	data_string_forget(&data, MDL);

	data.len = IAADDR_OFFSET;
	if (!buffer_allocate(&data.buffer, data.len, MDL)) {
		log_error("process_lq_by_address: no memory for ia-addr.");
		goto exit;
	}
	data.data = data.buffer->data;
	memcpy(data.buffer->data, &iaaddr->addr, 16);
	lifetime = iaaddr->prefer;
	putULong(data.buffer->data + 16, lifetime);
	lifetime = iaaddr->valid;
	putULong(data.buffer->data + 20, lifetime);
	if (!save_option_buffer(&dhcpv6_universe, opt_state,
				NULL, (unsigned char *)data.data, data.len,
				D6O_IAADDR, 0)) {
		log_error("process_lq_by_address: error saving ia-addr.");
		goto exit;
	}
	data_string_forget(&data, MDL);

	lifetime = htonl(iaaddr->ia->cltt);
	if (!save_option_buffer(&dhcpv6_universe, opt_state,
				NULL, (unsigned char *)&lifetime, 4,
				D6O_CLT_TIME, 0)) {
		log_error("process_lq_by_address: error saving clt time.");
		goto exit;
	}

	/*
	 * Store the client-data option.
	 */
	opt_cursor = lq->cursor;
	putUShort(lq->buf.data + lq->cursor, (unsigned)D6O_CLIENT_DATA);
	lq->cursor += 2;
	/* Skip option length. */
	lq->cursor += 2;

	lq->cursor += store_options6((char *)lq->buf.data + lq->cursor,
				     sizeof(lq->buf) - lq->cursor,
				     opt_state, lq->packet,
				     required_opt_CLIENT_DATA, NULL);
	/* Reset the length. */
	putUShort(lq->buf.data + opt_cursor + 2,
		  lq->cursor - (opt_cursor + 4));

	/* Done. */
	ret_val = 1;

     exit:
	if (data.data != NULL)
		data_string_forget(&data, MDL);
	if (pool != NULL)
		ipv6_pool_dereference(&pool, MDL);
	if (iaaddr != NULL)
		iasubopt_dereference(&iaaddr, MDL);
	if (opt_state != NULL)
		option_state_dereference(&opt_state, MDL);
	return ret_val;
}


/*
 * Process a lease query.
 */
void
dhcpv6_leasequery(struct data_string *reply_ret, struct packet *packet) {
	static struct lq6_state lq;
	struct option_cache *oc;
	int allow_lq;

	/*
	 * Initialize the lease query state.
	 */
	lq.packet = NULL;
	memset(&lq.client_id, 0, sizeof(lq.client_id));
	memset(&lq.server_id, 0, sizeof(lq.server_id));
	memset(&lq.lq_query, 0, sizeof(lq.lq_query));
	lq.query_opts = NULL;
	lq.reply_opts = NULL;
	packet_reference(&lq.packet, packet, MDL);

	/*
	 * Validate our input.
	 */
	if (!valid_query_msg(&lq)) {
		goto exit;
	}

	/*
	 * Prepare our reply.
	 */
	if (!option_state_allocate(&lq.reply_opts, MDL)) {
		log_error("dhcpv6_leasequery: no memory for option state.");
		goto exit;
	}
	execute_statements_in_scope(NULL, lq.packet, NULL, NULL,
				    lq.packet->options, lq.reply_opts,
				    &global_scope, root_group, NULL, NULL);

	lq.buf.reply.msg_type = DHCPV6_LEASEQUERY_REPLY;

	memcpy(lq.buf.reply.transaction_id,
	       lq.packet->dhcpv6_transaction_id,
	       sizeof(lq.buf.reply.transaction_id));

	/* 
	 * Because LEASEQUERY has some privacy concerns, default to deny.
	 */
	allow_lq = 0;

	/*
	 * See if we are authorized to do LEASEQUERY.
	 */
	oc = lookup_option(&server_universe, lq.reply_opts, SV_LEASEQUERY);
	if (oc != NULL) {
		allow_lq = evaluate_boolean_option_cache(NULL,
							 lq.packet,
							 NULL, NULL,
							 lq.packet->options,
							 lq.reply_opts,
							 &global_scope,
							 oc, MDL);
	}

	if (!allow_lq) {
		log_info("dhcpv6_leasequery: not allowed, query ignored.");
		goto exit;
	}
	    
	/*
	 * Same than transmission of REPLY message in RFC 3315:
	 *  server-id
	 *  client-id
	 */

	oc = lookup_option(&dhcpv6_universe, lq.reply_opts, D6O_SERVERID);
	if (oc == NULL) {
		/* If not already in options, get from query then global. */
		if (lq.server_id.data == NULL)
			copy_server_duid(&lq.server_id, MDL);
		if (!save_option_buffer(&dhcpv6_universe,
					lq.reply_opts,
					NULL,
					(unsigned char *)lq.server_id.data,
					lq.server_id.len, 
					D6O_SERVERID,
					0)) {
			log_error("dhcpv6_leasequery: "
				  "error saving server identifier.");
			goto exit;
		}
	}

	if (!save_option_buffer(&dhcpv6_universe,
				lq.reply_opts,
				lq.client_id.buffer,
				(unsigned char *)lq.client_id.data,
				lq.client_id.len,
				D6O_CLIENTID,
				0)) {
		log_error("dhcpv6_leasequery: "
			  "error saving client identifier.");
		goto exit;
	}

	lq.cursor = 4;

	/*
	 * Decode the lq-query option.
	 */

	if (lq.lq_query.len <= LQ_QUERY_OFFSET) {
		if (!set_error(&lq, STATUS_MalformedQuery,
			       "OPTION_LQ_QUERY too short.")) {
			log_error("dhcpv6_leasequery: unable "
				  "to set MalformedQuery status code.");
			goto exit;
		}
		goto done;
	}

	lq.query_type = lq.lq_query.data [0];
	memcpy(&lq.link_addr, lq.lq_query.data + 1, sizeof(lq.link_addr));
	switch (lq.query_type) {
		case LQ6QT_BY_ADDRESS:
			break;
		case LQ6QT_BY_CLIENTID:
			if (!set_error(&lq, STATUS_UnknownQueryType,
				       "QUERY_BY_CLIENTID not supported.")) {
				log_error("dhcpv6_leasequery: unable to "
					  "set UnknownQueryType status code.");
				goto exit;
			}
			goto done;
		default:
			if (!set_error(&lq, STATUS_UnknownQueryType,
				       "Unknown query-type.")) {
				log_error("dhcpv6_leasequery: unable to "
					  "set UnknownQueryType status code.");
				goto exit;
			}
			goto done;
	}

	if (!option_state_allocate(&lq.query_opts, MDL)) {
		log_error("dhcpv6_leasequery: no memory for option state.");
		goto exit;
	}
	if (!parse_option_buffer(lq.query_opts,
				 lq.lq_query.data + LQ_QUERY_OFFSET,
				 lq.lq_query.len - LQ_QUERY_OFFSET,
				 &dhcpv6_universe)) {
		log_error("dhcpv6_leasequery: error parsing query-options.");
		if (!set_error(&lq, STATUS_MalformedQuery,
			       "Bad query-options.")) {
			log_error("dhcpv6_leasequery: unable "
				  "to set MalformedQuery status code.");
			goto exit;
		}
		goto done;
	}

	/* Do it. */
	if (!process_lq_by_address(&lq))
		goto exit;

      done:
	/* Store the options. */
	lq.cursor += store_options6((char *)lq.buf.data + lq.cursor,
				    sizeof(lq.buf) - lq.cursor,
				    lq.reply_opts,
				    lq.packet,
				    required_opts_lq,
				    NULL);

	/* Return our reply to the caller. */
	reply_ret->len = lq.cursor;
	reply_ret->buffer = NULL;
	if (!buffer_allocate(&reply_ret->buffer, lq.cursor, MDL)) {
		log_fatal("dhcpv6_leasequery: no memory to store Reply.");
	}
	memcpy(reply_ret->buffer->data, lq.buf.data, lq.cursor);
	reply_ret->data = reply_ret->buffer->data;

      exit:
	/* Cleanup. */
	if (lq.packet != NULL)
		packet_dereference(&lq.packet, MDL);
	if (lq.client_id.data != NULL)
		data_string_forget(&lq.client_id, MDL);
	if (lq.server_id.data != NULL)
		data_string_forget(&lq.server_id, MDL);
	if (lq.lq_query.data != NULL)
		data_string_forget(&lq.lq_query, MDL);
	if (lq.query_opts != NULL)
		option_state_dereference(&lq.query_opts, MDL);
	if (lq.reply_opts != NULL)
		option_state_dereference(&lq.reply_opts, MDL);
}

#endif /* DHCPv6 */
