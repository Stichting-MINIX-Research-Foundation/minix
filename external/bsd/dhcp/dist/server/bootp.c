/*	$NetBSD: bootp.c,v 1.1.1.3 2014/07/12 11:58:04 spz Exp $	*/
/* bootp.c

   BOOTP Protocol support. */

/*
 * Copyright (c) 2009,2012-2014 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 2004,2005,2007 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1995-2003 by Internet Software Consortium
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
__RCSID("$NetBSD: bootp.c,v 1.1.1.3 2014/07/12 11:58:04 spz Exp $");

#include "dhcpd.h"
#include <errno.h>

#if defined (TRACING)
# define send_packet trace_packet_send
#endif

void bootp (packet)
	struct packet *packet;
{
	int result;
	struct host_decl *hp = (struct host_decl *)0;
	struct host_decl *host = (struct host_decl *)0;
	struct packet outgoing;
	struct dhcp_packet raw;
	struct sockaddr_in to;
	struct in_addr from;
	struct hardware hto;
	struct option_state *options = (struct option_state *)0;
	struct lease *lease = (struct lease *)0;
	unsigned i;
	struct data_string d1;
	struct option_cache *oc;
	char msgbuf [1024];
	int ignorep;
	int peer_has_leases = 0;

	if (packet -> raw -> op != BOOTREQUEST)
		return;

	/* %Audit% This is log output. %2004.06.17,Safe%
	 * If we truncate we hope the user can get a hint from the log.
	 */
	snprintf (msgbuf, sizeof msgbuf, "BOOTREQUEST from %s via %s",
		 print_hw_addr (packet -> raw -> htype,
				packet -> raw -> hlen,
				packet -> raw -> chaddr),
		 packet -> raw -> giaddr.s_addr
		 ? inet_ntoa (packet -> raw -> giaddr)
		 : packet -> interface -> name);

	if (!locate_network (packet)) {
		log_info ("%s: network unknown", msgbuf);
		return;
	}

	find_lease (&lease, packet, packet -> shared_network,
		    0, 0, (struct lease *)0, MDL);

	if (lease && lease->host)
		host_reference(&hp, lease->host, MDL);

	if (!lease || ((lease->flags & STATIC_LEASE) == 0)) {
		struct host_decl *h;

		/* We didn't find an applicable fixed-address host
		   declaration.  Just in case we may be able to dynamically
		   assign an address, see if there's a host declaration
		   that doesn't have an ip address associated with it. */

		if (!hp)
			find_hosts_by_haddr(&hp, packet->raw->htype,
					    packet->raw->chaddr,
					    packet->raw->hlen, MDL);

		for (h = hp; h; h = h -> n_ipaddr) {
			if (!h -> fixed_addr) {
				host_reference(&host, h, MDL);
				break;
			}
		}

		if (hp)
			host_dereference(&hp, MDL);

		if (host) {
			host_reference(&hp, host, MDL);
			host_dereference(&host, MDL);
		}

		/* Allocate a lease if we have not yet found one. */
		if (!lease)
			allocate_lease (&lease, packet,
					packet -> shared_network -> pools,
					&peer_has_leases);

		if (lease == NULL) {
			log_info("%s: BOOTP from dynamic client and no "
				 "dynamic leases", msgbuf);
			goto out;
		}

#if defined(FAILOVER_PROTOCOL)
		if ((lease->pool != NULL) &&
		    (lease->pool->failover_peer != NULL)) {
			dhcp_failover_state_t *peer;

			peer = lease->pool->failover_peer;

			/* If we are in a failover state that bars us from
			 * answering, do not do so.
			 * If we are in a cooperative state, load balance
			 * (all) responses.
			 */
			if ((peer->service_state == not_responding) ||
			    (peer->service_state == service_startup)) {
				log_info("%s: not responding%s",
					 msgbuf, peer->nrr);
				goto out;
			} else if((peer->service_state == cooperating) &&
				  !load_balance_mine(packet, peer)) {
				log_info("%s: load balance to peer %s",
					 msgbuf, peer->name);
				goto out;
			}
		}
#endif

		ack_lease (packet, lease, 0, 0, msgbuf, 0, hp);
		goto out;
	}

	/* Run the executable statements to compute the client and server
	   options. */
	option_state_allocate (&options, MDL);

	/* Execute the subnet statements. */
	execute_statements_in_scope (NULL, packet, lease, NULL,
				     packet->options, options,
				     &lease->scope, lease->subnet->group,
				     NULL, NULL);

	/* Execute statements from class scopes. */
	for (i = packet -> class_count; i > 0; i--) {
		execute_statements_in_scope(NULL, packet, lease, NULL,
					    packet->options, options,
					    &lease->scope,
					    packet->classes[i - 1]->group,
					    lease->subnet->group, NULL);
	}

	/* Execute the host statements. */
	if (hp != NULL) {
		execute_statements_in_scope (NULL, packet, lease, NULL,
					     packet->options, options,
					     &lease->scope, hp->group,
					     lease->subnet->group, NULL);
	}
	
	/* Drop the request if it's not allowed for this client. */
	if ((oc = lookup_option (&server_universe, options, SV_ALLOW_BOOTP)) &&
	    !evaluate_boolean_option_cache (&ignorep, packet, lease,
					    (struct client_state *)0,
					    packet -> options, options,
					    &lease -> scope, oc, MDL)) {
		if (!ignorep)
			log_info ("%s: bootp disallowed", msgbuf);
		goto out;
	} 

	if ((oc = lookup_option (&server_universe,
				 options, SV_ALLOW_BOOTING)) &&
	    !evaluate_boolean_option_cache (&ignorep, packet, lease,
					    (struct client_state *)0,
					    packet -> options, options,
					    &lease -> scope, oc, MDL)) {
		if (!ignorep)
			log_info ("%s: booting disallowed", msgbuf);
		goto out;
	}

	/* Set up the outgoing packet... */
	memset (&outgoing, 0, sizeof outgoing);
	memset (&raw, 0, sizeof raw);
	outgoing.raw = &raw;

	/* If we didn't get a known vendor magic number on the way in,
	   just copy the input options to the output. */
	if (!packet -> options_valid &&
	    !(evaluate_boolean_option_cache
	      (&ignorep, packet, lease, (struct client_state *)0,
	       packet -> options, options, &lease -> scope,
	       lookup_option (&server_universe, options,
			      SV_ALWAYS_REPLY_RFC1048), MDL))) {
		memcpy (outgoing.raw -> options,
			packet -> raw -> options, DHCP_MAX_OPTION_LEN);
		outgoing.packet_length = BOOTP_MIN_LEN;
	} else {

		/* Use the subnet mask from the subnet declaration if no other
		   mask has been provided. */

		oc = (struct option_cache *)0;
		i = DHO_SUBNET_MASK;
		if (!lookup_option (&dhcp_universe, options, i)) {
			if (option_cache_allocate (&oc, MDL)) {
				if (make_const_data
				    (&oc -> expression,
				     lease -> subnet -> netmask.iabuf,
				     lease -> subnet -> netmask.len,
				     0, 0, MDL)) {
					option_code_hash_lookup(&oc->option,
							dhcp_universe.code_hash,
								&i, 0, MDL);
					save_option (&dhcp_universe,
						     options, oc);
				}
				option_cache_dereference (&oc, MDL);
			}
		}

		/* Pack the options into the buffer.  Unlike DHCP, we
		   can't pack options into the filename and server
		   name buffers. */

		outgoing.packet_length =
			cons_options (packet, outgoing.raw, lease,
				      (struct client_state *)0, 0,
				      packet -> options, options,
				      &lease -> scope,
				      0, 0, 1, (struct data_string *)0,
				      (const char *)0);
		if (outgoing.packet_length < BOOTP_MIN_LEN)
			outgoing.packet_length = BOOTP_MIN_LEN;
	}

	/* Take the fields that we care about... */
	raw.op = BOOTREPLY;
	raw.htype = packet -> raw -> htype;
	raw.hlen = packet -> raw -> hlen;
	memcpy (raw.chaddr, packet -> raw -> chaddr, sizeof raw.chaddr);
	raw.hops = packet -> raw -> hops;
	raw.xid = packet -> raw -> xid;
	raw.secs = packet -> raw -> secs;
	raw.flags = packet -> raw -> flags;
	raw.ciaddr = packet -> raw -> ciaddr;

	/* yiaddr is an ipv4 address, it must be 4 octets. */
	memcpy (&raw.yiaddr, lease->ip_addr.iabuf, 4);

	/* If we're always supposed to broadcast to this client, set
	   the broadcast bit in the bootp flags field. */
	if ((oc = lookup_option (&server_universe,
				options, SV_ALWAYS_BROADCAST)) &&
	    evaluate_boolean_option_cache (&ignorep, packet, lease,
					   (struct client_state *)0,
					   packet -> options, options,
					   &lease -> scope, oc, MDL))
		raw.flags |= htons (BOOTP_BROADCAST);

	/* Figure out the address of the next server. */
	memset (&d1, 0, sizeof d1);
	oc = lookup_option (&server_universe, options, SV_NEXT_SERVER);
	if (oc &&
	    evaluate_option_cache (&d1, packet, lease,
				   (struct client_state *)0,
				   packet -> options, options,
				   &lease -> scope, oc, MDL)) {
		/* If there was more than one answer, take the first. */
		if (d1.len >= 4 && d1.data)
			memcpy (&raw.siaddr, d1.data, 4);
		data_string_forget (&d1, MDL);
	} else {
		if ((lease->subnet->shared_network->interface != NULL) &&
		    lease->subnet->shared_network->interface->address_count)
		    raw.siaddr =
			lease->subnet->shared_network->interface->addresses[0];
		else if (packet->interface->address_count)
			raw.siaddr = packet->interface->addresses[0];
	}

	raw.giaddr = packet -> raw -> giaddr;

	/* Figure out the filename. */
	oc = lookup_option (&server_universe, options, SV_FILENAME);
	if (oc &&
	    evaluate_option_cache (&d1, packet, lease,
				   (struct client_state *)0,
				   packet -> options, options,
				   &lease -> scope, oc, MDL)) {
		memcpy (raw.file, d1.data,
			d1.len > sizeof raw.file ? sizeof raw.file : d1.len);
		if (sizeof raw.file > d1.len)
			memset (&raw.file [d1.len],
				0, (sizeof raw.file) - d1.len);
		data_string_forget (&d1, MDL);
	} else
		memcpy (raw.file, packet -> raw -> file, sizeof raw.file);

	/* Choose a server name as above. */
	oc = lookup_option (&server_universe, options, SV_SERVER_NAME);
	if (oc &&
	    evaluate_option_cache (&d1, packet, lease,
				   (struct client_state *)0,
				   packet -> options, options,
				   &lease -> scope, oc, MDL)) {
		memcpy (raw.sname, d1.data,
			d1.len > sizeof raw.sname ? sizeof raw.sname : d1.len);
		if (sizeof raw.sname > d1.len)
			memset (&raw.sname [d1.len],
				0, (sizeof raw.sname) - d1.len);
		data_string_forget (&d1, MDL);
	}

	/* Execute the commit statements, if there are any. */
	execute_statements (NULL, packet, lease, NULL, packet->options,
			    options, &lease->scope, lease->on_star.on_commit,
			    NULL);

	/* We're done with the option state. */
	option_state_dereference (&options, MDL);

	/* Set up the hardware destination address... */
	hto.hbuf [0] = packet -> raw -> htype;
	hto.hlen = packet -> raw -> hlen + 1;
	memcpy (&hto.hbuf [1], packet -> raw -> chaddr, packet -> raw -> hlen);

	if (packet->interface->address_count) {
		from = packet->interface->addresses[0];
	} else {
		log_error("%s: Interface %s appears to have no IPv4 "
			  "addresses, and so dhcpd cannot select a source "
			  "address.", msgbuf, packet->interface->name);
		goto out;
	}

	/* Report what we're doing... */
	log_info("%s", msgbuf);
	log_info("BOOTREPLY for %s to %s (%s) via %s",
		 piaddr(lease->ip_addr),
		 ((hp != NULL) && (hp->name != NULL)) ? hp -> name : "unknown",
		 print_hw_addr (packet->raw->htype,
				packet->raw->hlen,
				packet->raw->chaddr),
		 packet->raw->giaddr.s_addr
		 ? inet_ntoa (packet->raw->giaddr)
		 : packet->interface->name);

	/* Set up the parts of the address that are in common. */
	to.sin_family = AF_INET;
#ifdef HAVE_SA_LEN
	to.sin_len = sizeof to;
#endif
	memset (to.sin_zero, 0, sizeof to.sin_zero);

	/* If this was gatewayed, send it back to the gateway... */
	if (raw.giaddr.s_addr) {
		to.sin_addr = raw.giaddr;
		to.sin_port = local_port;

		if (fallback_interface) {
			result = send_packet (fallback_interface, NULL, &raw,
					      outgoing.packet_length, from,
					      &to, &hto);
			if (result < 0) {
				log_error ("%s:%d: Failed to send %d byte long "
					   "packet over %s interface.", MDL,
					   outgoing.packet_length,
					   fallback_interface->name);
			}

			goto out;
		}

	/* If it comes from a client that already knows its address
	   and is not requesting a broadcast response, and we can
	   unicast to a client without using the ARP protocol, sent it
	   directly to that client. */
	} else if (!(raw.flags & htons (BOOTP_BROADCAST)) &&
		   can_unicast_without_arp (packet -> interface)) {
		to.sin_addr = raw.yiaddr;
		to.sin_port = remote_port;

	/* Otherwise, broadcast it on the local network. */
	} else {
		to.sin_addr = limited_broadcast;
		to.sin_port = remote_port; /* XXX */
	}

	errno = 0;
	result = send_packet(packet->interface, packet, &raw,
			     outgoing.packet_length, from, &to, &hto);
	if (result < 0) {
		log_error ("%s:%d: Failed to send %d byte long packet over %s"
			   " interface.", MDL, outgoing.packet_length,
			   packet->interface->name);
	}

      out:

	if (options)
		option_state_dereference (&options, MDL);
	if (lease)
		lease_dereference (&lease, MDL);
	if (hp)
		host_dereference (&hp, MDL);
	if (host)
		host_dereference (&host, MDL);
}
