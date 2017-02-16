/*	$NetBSD: dhcp.c,v 1.1.1.3 2014/07/12 11:58:08 spz Exp $	*/
/* dhcp.c

   DHCP Protocol engine. */

/*
 * Copyright (c) 2004-2014 by Internet Systems Consortium, Inc. ("ISC")
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
__RCSID("$NetBSD: dhcp.c,v 1.1.1.3 2014/07/12 11:58:08 spz Exp $");

#include "dhcpd.h"
#include <errno.h>
#include <limits.h>
#include <sys/time.h>

static void commit_leases_ackout(void *foo);
static void maybe_return_agent_options(struct packet *packet,
				       struct option_state *options);

int outstanding_pings;

struct leasequeue *ackqueue_head, *ackqueue_tail;
static struct leasequeue *free_ackqueue;
static struct timeval max_fsync;

int outstanding_acks;
int max_outstanding_acks = DEFAULT_DELAYED_ACK;
int max_ack_delay_secs = DEFAULT_ACK_DELAY_SECS;
int max_ack_delay_usecs = DEFAULT_ACK_DELAY_USECS;
int min_ack_delay_usecs = DEFAULT_MIN_ACK_DELAY_USECS;

static char dhcp_message [256];
static int site_code_min;

static int find_min_site_code(struct universe *);
static isc_result_t lowest_site_code(const void *, unsigned, void *);

static const char *dhcp_type_names [] = { 
	"DHCPDISCOVER",
	"DHCPOFFER",
	"DHCPREQUEST",
	"DHCPDECLINE",
	"DHCPACK",
	"DHCPNAK",
	"DHCPRELEASE",
	"DHCPINFORM",
	"type 9",
	"DHCPLEASEQUERY",
	"DHCPLEASEUNASSIGNED",
	"DHCPLEASEUNKNOWN",
	"DHCPLEASEACTIVE"
};
const int dhcp_type_name_max = ((sizeof dhcp_type_names) / sizeof (char *));

#if defined (TRACING)
# define send_packet trace_packet_send
#endif

void
dhcp (struct packet *packet) {
	int ms_nulltp = 0;
	struct option_cache *oc;
	struct lease *lease = NULL;
	const char *errmsg;
	struct data_string data;

	if (!locate_network(packet) &&
	    packet->packet_type != DHCPREQUEST &&
	    packet->packet_type != DHCPINFORM && 
	    packet->packet_type != DHCPLEASEQUERY) {
		const char *s;
		char typebuf[32];
		errmsg = "unknown network segment";
	      bad_packet:
		
		if (packet->packet_type > 0 &&
		    packet->packet_type <= dhcp_type_name_max) {
			s = dhcp_type_names[packet->packet_type - 1];
		} else {
			/* %Audit% Cannot exceed 28 bytes. %2004.06.17,Safe% */
			sprintf(typebuf, "type %d", packet->packet_type);
			s = typebuf;
		}
		
		log_info("%s from %s via %s: %s", s,
			 (packet->raw->htype
			  ? print_hw_addr(packet->raw->htype,
					  packet->raw->hlen,
					  packet->raw->chaddr)
			  : "<no identifier>"),
			 packet->raw->giaddr.s_addr
			 ? inet_ntoa(packet->raw->giaddr)
			 : packet->interface->name, errmsg);
		goto out;
	}

	/* There is a problem with the relay agent information option,
	 * which is that in order for a normal relay agent to append
	 * this option, the relay agent has to have been involved in
	 * getting the packet from the client to the server.  Note
	 * that this is the software entity known as the relay agent,
	 * _not_ the hardware entity known as a router in which the
	 * relay agent may be running, so the fact that a router has
	 * forwarded a packet does not mean that the relay agent in
	 * the router was involved.
	 *
	 * So when the client broadcasts (DHCPDISCOVER, or giaddr is set),
	 * we can be sure that there are either agent options in the
	 * packet, or there aren't supposed to be.  When the giaddr is not
	 * set, it's still possible that the client is on a directly
	 * attached subnet, and agent options are being appended by an l2
	 * device that has no address, and so sets no giaddr.
	 *
	 * But in either case it's possible that the packets we receive
	 * from the client in RENEW state may not include the agent options,
	 * so if they are not in the packet we must "pretend" the last values
	 * we observed were provided.
	 */
	if (packet->packet_type == DHCPREQUEST &&
	    packet->raw->ciaddr.s_addr && !packet->raw->giaddr.s_addr &&
	    (packet->options->universe_count <= agent_universe.index ||
	     packet->options->universes[agent_universe.index] == NULL))
	{
		struct iaddr cip;

		cip.len = sizeof packet -> raw -> ciaddr;
		memcpy (cip.iabuf, &packet -> raw -> ciaddr,
			sizeof packet -> raw -> ciaddr);
		if (!find_lease_by_ip_addr (&lease, cip, MDL))
			goto nolease;

		/* If there are no agent options on the lease, it's not
		   interesting. */
		if (!lease -> agent_options)
			goto nolease;

		/* The client should not be unicasting a renewal if its lease
		   has expired, so make it go through the process of getting
		   its agent options legally. */
		if (lease -> ends < cur_time)
			goto nolease;

		if (lease -> uid_len) {
			oc = lookup_option (&dhcp_universe, packet -> options,
					    DHO_DHCP_CLIENT_IDENTIFIER);
			if (!oc)
				goto nolease;

			memset (&data, 0, sizeof data);
			if (!evaluate_option_cache (&data,
						    packet, (struct lease *)0,
						    (struct client_state *)0,
						    packet -> options,
						    (struct option_state *)0,
						    &global_scope, oc, MDL))
				goto nolease;
			if (lease -> uid_len != data.len ||
			    memcmp (lease -> uid, data.data, data.len)) {
				data_string_forget (&data, MDL);
				goto nolease;
			}
			data_string_forget (&data, MDL);
		} else
			if ((lease -> hardware_addr.hbuf [0] !=
			     packet -> raw -> htype) ||
			    (lease -> hardware_addr.hlen - 1 !=
			     packet -> raw -> hlen) ||
			    memcmp (&lease -> hardware_addr.hbuf [1],
				    packet -> raw -> chaddr,
				    packet -> raw -> hlen))
				goto nolease;

		/* Okay, so we found a lease that matches the client. */
		option_chain_head_reference ((struct option_chain_head **)
					     &(packet -> options -> universes
					       [agent_universe.index]),
					     lease -> agent_options, MDL);

		if (packet->options->universe_count <= agent_universe.index)
			packet->options->universe_count =
						agent_universe.index + 1;

		packet->agent_options_stashed = ISC_TRUE;
	}
      nolease:

	/* If a client null terminates options it sends, it probably
	 * expects the server to reciprocate.
	 */
	if ((oc = lookup_option (&dhcp_universe, packet -> options,
				 DHO_HOST_NAME))) {
		if (!oc -> expression)
			ms_nulltp = oc->flags & OPTION_HAD_NULLS;
	}

	/* Classify the client. */
	classify_client (packet);

	switch (packet -> packet_type) {
	      case DHCPDISCOVER:
		dhcpdiscover (packet, ms_nulltp);
		break;

	      case DHCPREQUEST:
		dhcprequest (packet, ms_nulltp, lease);
		break;

	      case DHCPRELEASE:
		dhcprelease (packet, ms_nulltp);
		break;

	      case DHCPDECLINE:
		dhcpdecline (packet, ms_nulltp);
		break;

	      case DHCPINFORM:
		dhcpinform (packet, ms_nulltp);
		break;

	      case DHCPLEASEQUERY:
		dhcpleasequery(packet, ms_nulltp);
		break;

	      case DHCPACK:
	      case DHCPOFFER:
	      case DHCPNAK:
	      case DHCPLEASEUNASSIGNED:
	      case DHCPLEASEUNKNOWN:
	      case DHCPLEASEACTIVE:
		break;

	      default:
		errmsg = "unknown packet type";
		goto bad_packet;
	}
      out:
	if (lease)
		lease_dereference (&lease, MDL);
}

void dhcpdiscover (packet, ms_nulltp)
	struct packet *packet;
	int ms_nulltp;
{
	struct lease *lease = (struct lease *)0;
	char msgbuf [1024]; /* XXX */
	TIME when;
	const char *s;
	int peer_has_leases = 0;
#if defined (FAILOVER_PROTOCOL)
	dhcp_failover_state_t *peer;
#endif

	find_lease (&lease, packet, packet -> shared_network,
		    0, &peer_has_leases, (struct lease *)0, MDL);

	if (lease && lease -> client_hostname) {
		if ((strlen (lease -> client_hostname) <= 64) &&
		    db_printable((unsigned char *)lease->client_hostname))
			s = lease -> client_hostname;
		else
			s = "Hostname Unsuitable for Printing";
	} else
		s = (char *)0;

	/* %Audit% This is log output. %2004.06.17,Safe%
	 * If we truncate we hope the user can get a hint from the log.
	 */
	snprintf (msgbuf, sizeof msgbuf, "DHCPDISCOVER from %s %s%s%svia %s",
		 (packet -> raw -> htype
		  ? print_hw_addr (packet -> raw -> htype,
				   packet -> raw -> hlen,
				   packet -> raw -> chaddr)
		  : (lease
		     ? print_hex_1(lease->uid_len, lease->uid, 60)
		     : "<no identifier>")),
		  s ? "(" : "", s ? s : "", s ? ") " : "",
		  packet -> raw -> giaddr.s_addr
		  ? inet_ntoa (packet -> raw -> giaddr)
		  : packet -> interface -> name);

	/* Sourceless packets don't make sense here. */
	if (!packet -> shared_network) {
		log_info ("Packet from unknown subnet: %s",
		      inet_ntoa (packet -> raw -> giaddr));
		goto out;
	}

#if defined (FAILOVER_PROTOCOL)
	if (lease && lease -> pool && lease -> pool -> failover_peer) {
		peer = lease -> pool -> failover_peer;

		/*
		 * If the lease is ours to (re)allocate, then allocate it.
		 *
		 * If the lease is active, it belongs to the client.  This
		 * is the right lease, if we are to offer one.  We decide
		 * whether or not to offer later on.
		 *
		 * If the lease was last active, and we've reached this
		 * point, then it was last active with the same client.  We
		 * can safely re-activate the lease with this client.
		 */
		if (lease->binding_state == FTS_ACTIVE ||
		    lease->rewind_binding_state == FTS_ACTIVE ||
		    lease_mine_to_reallocate(lease)) {
			; /* This space intentionally left blank. */

		/* Otherwise, we can't let the client have this lease. */
		} else {
#if defined (DEBUG_FIND_LEASE)
		    log_debug ("discarding %s - %s",
			       piaddr (lease -> ip_addr),
			       binding_state_print (lease -> binding_state));
#endif
		    lease_dereference (&lease, MDL);
		}
	}
#endif

	/* If we didn't find a lease, try to allocate one... */
	if (!lease) {
		if (!allocate_lease (&lease, packet,
				     packet -> shared_network -> pools, 
				     &peer_has_leases)) {
			if (peer_has_leases)
				log_error ("%s: peer holds all free leases",
					   msgbuf);
			else
				log_error ("%s: network %s: no free leases",
					   msgbuf,
					   packet -> shared_network -> name);
			return;
		}
	}

#if defined (FAILOVER_PROTOCOL)
	if (lease && lease -> pool && lease -> pool -> failover_peer) {
		peer = lease -> pool -> failover_peer;
		if (peer -> service_state == not_responding ||
		    peer -> service_state == service_startup) {
			log_info ("%s: not responding%s",
				  msgbuf, peer -> nrr);
			goto out;
		}
	} else
		peer = (dhcp_failover_state_t *)0;

	/* Do load balancing if configured. */
	if (peer && (peer -> service_state == cooperating) &&
	    !load_balance_mine (packet, peer)) {
		if (peer_has_leases) {
			log_debug ("%s: load balance to peer %s",
				   msgbuf, peer -> name);
			goto out;
		} else {
			log_debug ("%s: cancel load balance to peer %s - %s",
				   msgbuf, peer -> name, "no free leases");
		}
	}
#endif

	/* If it's an expired lease, get rid of any bindings. */
	if (lease -> ends < cur_time && lease -> scope)
		binding_scope_dereference (&lease -> scope, MDL);

	/* Set the lease to really expire in 2 minutes, unless it has
	   not yet expired, in which case leave its expiry time alone. */
	when = cur_time + 120;
	if (when < lease -> ends)
		when = lease -> ends;

	ack_lease (packet, lease, DHCPOFFER, when, msgbuf, ms_nulltp,
		   (struct host_decl *)0);
      out:
	if (lease)
		lease_dereference (&lease, MDL);
}

void dhcprequest (packet, ms_nulltp, ip_lease)
	struct packet *packet;
	int ms_nulltp;
	struct lease *ip_lease;
{
	struct lease *lease;
	struct iaddr cip;
	struct iaddr sip;
	struct subnet *subnet;
	int ours = 0;
	struct option_cache *oc;
	struct data_string data;
	char msgbuf [1024]; /* XXX */
	const char *s;
	char smbuf [19];
#if defined (FAILOVER_PROTOCOL)
	dhcp_failover_state_t *peer;
#endif
	int have_requested_addr = 0;

	oc = lookup_option (&dhcp_universe, packet -> options,
			    DHO_DHCP_REQUESTED_ADDRESS);
	memset (&data, 0, sizeof data);
	if (oc &&
	    evaluate_option_cache (&data, packet, (struct lease *)0,
				   (struct client_state *)0,
				   packet -> options, (struct option_state *)0,
				   &global_scope, oc, MDL)) {
		cip.len = 4;
		memcpy (cip.iabuf, data.data, 4);
		data_string_forget (&data, MDL);
		have_requested_addr = 1;
	} else {
		oc = (struct option_cache *)0;
		cip.len = 4;
		memcpy (cip.iabuf, &packet -> raw -> ciaddr.s_addr, 4);
	}

	/* Find the lease that matches the address requested by the
	   client. */

	subnet = (struct subnet *)0;
	lease = (struct lease *)0;
	if (find_subnet (&subnet, cip, MDL))
		find_lease (&lease, packet,
			    subnet -> shared_network, &ours, 0, ip_lease, MDL);

	if (lease && lease -> client_hostname) {
		if ((strlen (lease -> client_hostname) <= 64) &&
		    db_printable((unsigned char *)lease->client_hostname))
			s = lease -> client_hostname;
		else
			s = "Hostname Unsuitable for Printing";
	} else
		s = (char *)0;

	oc = lookup_option (&dhcp_universe, packet -> options,
			    DHO_DHCP_SERVER_IDENTIFIER);
	memset (&data, 0, sizeof data);
	if (oc &&
	    evaluate_option_cache (&data, packet, (struct lease *)0,
				   (struct client_state *)0,
				   packet -> options, (struct option_state *)0,
				   &global_scope, oc, MDL)) {
		sip.len = 4;
		memcpy (sip.iabuf, data.data, 4);
		data_string_forget (&data, MDL);
		/* piaddr() should not return more than a 15 byte string.
		 * safe.
		 */
		sprintf (smbuf, " (%s)", piaddr (sip));
	} else {
		smbuf [0] = 0;
		sip.len = 0;
	}

	/* %Audit% This is log output. %2004.06.17,Safe%
	 * If we truncate we hope the user can get a hint from the log.
	 */
	snprintf (msgbuf, sizeof msgbuf,
		 "DHCPREQUEST for %s%s from %s %s%s%svia %s",
		 piaddr (cip), smbuf,
		 (packet -> raw -> htype
		  ? print_hw_addr (packet -> raw -> htype,
				   packet -> raw -> hlen,
				   packet -> raw -> chaddr)
		  : (lease
		     ? print_hex_1(lease->uid_len, lease->uid, 60)
		     : "<no identifier>")),
		 s ? "(" : "", s ? s : "", s ? ") " : "",
		  packet -> raw -> giaddr.s_addr
		  ? inet_ntoa (packet -> raw -> giaddr)
		  : packet -> interface -> name);

#if defined (FAILOVER_PROTOCOL)
	if (lease && lease -> pool && lease -> pool -> failover_peer) {
		peer = lease -> pool -> failover_peer;
		if (peer -> service_state == not_responding ||
		    peer -> service_state == service_startup) {
			log_info ("%s: not responding%s",
				  msgbuf, peer -> nrr);
			goto out;
		}

		/* "load balance to peer" - is not done at all for request.
		 *
		 * If it's RENEWING, we are the only server to hear it, so
		 * we have to serve it.   If it's REBINDING, it's out of
		 * communication with the other server, so there's no point
		 * in waiting to serve it.    However, if the lease we're
		 * offering is not a free lease, then we may be the only
		 * server that can offer it, so we can't load balance if
		 * the lease isn't in the free or backup state.  If it is
		 * in the free or backup state, then that state is what
		 * mandates one server or the other should perform the
		 * allocation, not the LBA...we know the peer cannot
		 * allocate a request for an address in our free state.
		 *
		 * So our only compass is lease_mine_to_reallocate().  This
		 * effects both load balancing, and a sanity-check that we
		 * are not going to try to allocate a lease that isn't ours.
		 */
		if ((lease -> binding_state == FTS_FREE ||
		     lease -> binding_state == FTS_BACKUP) &&
		    !lease_mine_to_reallocate (lease)) {
			log_debug ("%s: lease owned by peer", msgbuf);
			goto out;
		}

		/*
		 * If the lease is in a transitional state, we can't
		 * renew it unless we can rewind it to a non-transitional
		 * state (active, free, or backup).  lease_mine_to_reallocate()
		 * checks for free/backup, so we only need to check for active.
		 */
		if ((lease->binding_state == FTS_RELEASED ||
		     lease->binding_state == FTS_EXPIRED) &&
		    lease->rewind_binding_state != FTS_ACTIVE &&
		    !lease_mine_to_reallocate(lease)) {
			log_debug("%s: lease in transition state %s", msgbuf,
				  (lease->binding_state == FTS_RELEASED)
				   ? "released" : "expired");
			goto out;
		}

		/* It's actually very unlikely that we'll ever get here,
		   but if we do, tell the client to stop using the lease,
		   because the administrator reset it. */
		if (lease -> binding_state == FTS_RESET &&
		    !lease_mine_to_reallocate (lease)) {
			log_debug ("%s: lease reset by administrator", msgbuf);
			nak_lease (packet, &cip);
			goto out;
		}

#if defined(SERVER_ID_CHECK)
		/* Do a quick check on the server source address to see if
		   it is ours.  sip is the incoming servrer id.  To avoid
		   problems with confused clients we do some sanity checks
		   to verify sip's length and that it isn't all zeros.
		   We then get the server id we would likely use for this
		   packet and compare them.  If they don't match it we assume
		   we didn't send the offer and so we don't process the request.
		*/

		if ((sip.len == 4) &&
		    (memcmp(sip.iabuf, "\0\0\0\0", sip.len) != 0)) {
			struct in_addr from;
			setup_server_source_address(&from, NULL, packet);
			if (memcmp(sip.iabuf, &from, sip.len) != 0) {
				log_debug("%s: not our server id", msgbuf);
				goto out;
			}
		}
#endif /* if defined(SERVER_ID_CHECK) */

		/* At this point it's possible that we will get a broadcast
		   DHCPREQUEST for a lease that we didn't offer, because
		   both we and the peer are in a position to offer it.
		   In that case, we probably shouldn't answer.   In order
		   to not answer, we would have to compare the server
		   identifier sent by the client with the list of possible
		   server identifiers we can send, and if the client's
		   identifier isn't on the list, drop the DHCPREQUEST.
		   We aren't currently doing that for two reasons - first,
		   it's not clear that all clients do the right thing
		   with respect to sending the client identifier, which
		   could mean that we might simply not respond to a client
		   that is depending on us to respond.   Secondly, we allow
		   the user to specify the server identifier to send, and
		   we don't enforce that the server identifier should be
		   one of our IP addresses.   This is probably not a big
		   deal, but it's theoretically an issue.

		   The reason we care about this is that if both servers
		   send a DHCPACK to the DHCPREQUEST, they are then going
		   to send dueling BNDUPD messages, which could cause
		   trouble.   I think it causes no harm, but it seems
		   wrong. */
	} else
		peer = (dhcp_failover_state_t *)0;
#endif

	/* If a client on a given network REQUESTs a lease on an
	   address on a different network, NAK it.  If the Requested
	   Address option was used, the protocol says that it must
	   have been broadcast, so we can trust the source network
	   information.

	   If ciaddr was specified and Requested Address was not, then
	   we really only know for sure what network a packet came from
	   if it came through a BOOTP gateway - if it came through an
	   IP router, we'll just have to assume that it's cool.

	   If we don't think we know where the packet came from, it
	   came through a gateway from an unknown network, so it's not
	   from a RENEWING client.  If we recognize the network it
	   *thinks* it's on, we can NAK it even though we don't
	   recognize the network it's *actually* on; otherwise we just
	   have to ignore it.

	   We don't currently try to take advantage of access to the
	   raw packet, because it's not available on all platforms.
	   So a packet that was unicast to us through a router from a
	   RENEWING client is going to look exactly like a packet that
	   was broadcast to us from an INIT-REBOOT client.

	   Since we can't tell the difference between these two kinds
	   of packets, if the packet appears to have come in off the
	   local wire, we have to treat it as if it's a RENEWING
	   client.  This means that we can't NAK a RENEWING client on
	   the local wire that has a bogus address.  The good news is
	   that we won't ACK it either, so it should revert to INIT
	   state and send us a DHCPDISCOVER, which we *can* work with.

	   Because we can't detect that a RENEWING client is on the
	   wrong wire, it's going to sit there trying to renew until
	   it gets to the REBIND state, when we *can* NAK it because
	   the packet will get to us through a BOOTP gateway.  We
	   shouldn't actually see DHCPREQUEST packets from RENEWING
	   clients on the wrong wire anyway, since their idea of their
	   local router will be wrong.  In any case, the protocol
	   doesn't really allow us to NAK a DHCPREQUEST from a
	   RENEWING client, so we can punt on this issue. */

	if (!packet -> shared_network ||
	    (packet -> raw -> ciaddr.s_addr &&
	     packet -> raw -> giaddr.s_addr) ||
	    (have_requested_addr && !packet -> raw -> ciaddr.s_addr)) {
		
		/* If we don't know where it came from but we do know
		   where it claims to have come from, it didn't come
		   from there. */
		if (!packet -> shared_network) {
			if (subnet && subnet -> group -> authoritative) {
				log_info ("%s: wrong network.", msgbuf);
				nak_lease (packet, &cip);
				goto out;
			}
			/* Otherwise, ignore it. */
			log_info ("%s: ignored (%s).", msgbuf,
				  (subnet
				   ? "not authoritative" : "unknown subnet"));
			goto out;
		}

		/* If we do know where it came from and it asked for an
		   address that is not on that shared network, nak it. */
		if (subnet)
			subnet_dereference (&subnet, MDL);
		if (!find_grouped_subnet (&subnet, packet -> shared_network,
					  cip, MDL)) {
			if (packet -> shared_network -> group -> authoritative)
			{
				log_info ("%s: wrong network.", msgbuf);
				nak_lease (packet, &cip);
				goto out;
			}
			log_info ("%s: ignored (not authoritative).", msgbuf);
			return;
		}
	}

	/* If the address the client asked for is ours, but it wasn't
	   available for the client, NAK it. */
	if (!lease && ours) {
		log_info ("%s: lease %s unavailable.", msgbuf, piaddr (cip));
		nak_lease (packet, &cip);
		goto out;
	}

	/* Otherwise, send the lease to the client if we found one. */
	if (lease) {
		ack_lease (packet, lease, DHCPACK, 0, msgbuf, ms_nulltp,
			   (struct host_decl *)0);
	} else
		log_info ("%s: unknown lease %s.", msgbuf, piaddr (cip));

      out:
	if (subnet)
		subnet_dereference (&subnet, MDL);
	if (lease)
		lease_dereference (&lease, MDL);
	return;
}

void dhcprelease (packet, ms_nulltp)
	struct packet *packet;
	int ms_nulltp;
{
	struct lease *lease = (struct lease *)0, *next = (struct lease *)0;
	struct iaddr cip;
	struct option_cache *oc;
	struct data_string data;
	const char *s;
	char msgbuf [1024], cstr[16]; /* XXX */


	/* DHCPRELEASE must not specify address in requested-address
	   option, but old protocol specs weren't explicit about this,
	   so let it go. */
	if ((oc = lookup_option (&dhcp_universe, packet -> options,
				 DHO_DHCP_REQUESTED_ADDRESS))) {
		log_info ("DHCPRELEASE from %s specified requested-address.",
		      print_hw_addr (packet -> raw -> htype,
				     packet -> raw -> hlen,
				     packet -> raw -> chaddr));
	}

	oc = lookup_option (&dhcp_universe, packet -> options,
			    DHO_DHCP_CLIENT_IDENTIFIER);
	memset (&data, 0, sizeof data);
	if (oc &&
	    evaluate_option_cache (&data, packet, (struct lease *)0,
				   (struct client_state *)0,
				   packet -> options, (struct option_state *)0,
				   &global_scope, oc, MDL)) {
		find_lease_by_uid (&lease, data.data, data.len, MDL);
		data_string_forget (&data, MDL);

		/* See if we can find a lease that matches the IP address
		   the client is claiming. */
		while (lease) {
			if (lease -> n_uid)
				lease_reference (&next, lease -> n_uid, MDL);
			if (!memcmp (&packet -> raw -> ciaddr,
				     lease -> ip_addr.iabuf, 4)) {
				break;
			}
			lease_dereference (&lease, MDL);
			if (next) {
				lease_reference (&lease, next, MDL);
				lease_dereference (&next, MDL);
			}
		}
		if (next)
			lease_dereference (&next, MDL);
	}

	/* The client is supposed to pass a valid client-identifier,
	   but the spec on this has changed historically, so try the
	   IP address in ciaddr if the client-identifier fails. */
	if (!lease) {
		cip.len = 4;
		memcpy (cip.iabuf, &packet -> raw -> ciaddr, 4);
		find_lease_by_ip_addr (&lease, cip, MDL);
	}


	/* If the hardware address doesn't match, don't do the release. */
	if (lease &&
	    (lease -> hardware_addr.hlen != packet -> raw -> hlen + 1 ||
	     lease -> hardware_addr.hbuf [0] != packet -> raw -> htype ||
	     memcmp (&lease -> hardware_addr.hbuf [1],
		     packet -> raw -> chaddr, packet -> raw -> hlen)))
		lease_dereference (&lease, MDL);

	if (lease && lease -> client_hostname) {
		if ((strlen (lease -> client_hostname) <= 64) &&
		    db_printable((unsigned char *)lease->client_hostname))
			s = lease -> client_hostname;
		else
			s = "Hostname Unsuitable for Printing";
	} else
		s = (char *)0;

	/* %Audit% Cannot exceed 16 bytes. %2004.06.17,Safe%
	 * We copy this out to stack because we actually want to log two
	 * inet_ntoa()'s in this message.
	 */
	strncpy(cstr, inet_ntoa (packet -> raw -> ciaddr), 15);
	cstr[15] = '\0';

	/* %Audit% This is log output. %2004.06.17,Safe%
	 * If we truncate we hope the user can get a hint from the log.
	 */
	snprintf (msgbuf, sizeof msgbuf,
		 "DHCPRELEASE of %s from %s %s%s%svia %s (%sfound)",
		 cstr,
		 (packet -> raw -> htype
		  ? print_hw_addr (packet -> raw -> htype,
				   packet -> raw -> hlen,
				   packet -> raw -> chaddr)
		  : (lease
		     ? print_hex_1(lease->uid_len, lease->uid, 60)
		     : "<no identifier>")),
		 s ? "(" : "", s ? s : "", s ? ") " : "",
		 packet -> raw -> giaddr.s_addr
		 ? inet_ntoa (packet -> raw -> giaddr)
		 : packet -> interface -> name,
		 lease ? "" : "not ");

#if defined (FAILOVER_PROTOCOL)
	if (lease && lease -> pool && lease -> pool -> failover_peer) {
		dhcp_failover_state_t *peer = lease -> pool -> failover_peer;
		if (peer -> service_state == not_responding ||
		    peer -> service_state == service_startup) {
			log_info ("%s: ignored%s",
				  peer -> name, peer -> nrr);
			goto out;
		}

		/* DHCPRELEASE messages are unicast, so if the client
		   sent the DHCPRELEASE to us, it's not going to send it
		   to the peer.   Not sure why this would happen, and
		   if it does happen I think we still have to change the
		   lease state, so that's what we're doing.
		   XXX See what it says in the draft about this. */
	}
#endif

	/* If we found a lease, release it. */
	if (lease && lease -> ends > cur_time) {
		release_lease (lease, packet);
	} 
	log_info ("%s", msgbuf);
#if defined(FAILOVER_PROTOCOL)
      out:
#endif
	if (lease)
		lease_dereference (&lease, MDL);
}

void dhcpdecline (packet, ms_nulltp)
	struct packet *packet;
	int ms_nulltp;
{
	struct lease *lease = (struct lease *)0;
	struct option_state *options = (struct option_state *)0;
	int ignorep = 0;
	int i;
	const char *status;
	const char *s;
	char msgbuf [1024]; /* XXX */
	struct iaddr cip;
	struct option_cache *oc;
	struct data_string data;

	/* DHCPDECLINE must specify address. */
	if (!(oc = lookup_option (&dhcp_universe, packet -> options,
				  DHO_DHCP_REQUESTED_ADDRESS)))
		return;
	memset (&data, 0, sizeof data);
	if (!evaluate_option_cache (&data, packet, (struct lease *)0,
				    (struct client_state *)0,
				    packet -> options,
				    (struct option_state *)0,
				    &global_scope, oc, MDL))
		return;

	cip.len = 4;
	memcpy (cip.iabuf, data.data, 4);
	data_string_forget (&data, MDL);
	find_lease_by_ip_addr (&lease, cip, MDL);

	if (lease && lease -> client_hostname) {
		if ((strlen (lease -> client_hostname) <= 64) &&
		    db_printable((unsigned char *)lease->client_hostname))
			s = lease -> client_hostname;
		else
			s = "Hostname Unsuitable for Printing";
	} else
		s = (char *)0;

	/* %Audit% This is log output. %2004.06.17,Safe%
	 * If we truncate we hope the user can get a hint from the log.
	 */
	snprintf (msgbuf, sizeof msgbuf,
		 "DHCPDECLINE of %s from %s %s%s%svia %s",
		 piaddr (cip),
		 (packet -> raw -> htype
		  ? print_hw_addr (packet -> raw -> htype,
				   packet -> raw -> hlen,
				   packet -> raw -> chaddr)
		  : (lease
		     ? print_hex_1(lease->uid_len, lease->uid, 60)
		     : "<no identifier>")),
		 s ? "(" : "", s ? s : "", s ? ") " : "",
		 packet -> raw -> giaddr.s_addr
		 ? inet_ntoa (packet -> raw -> giaddr)
		 : packet -> interface -> name);

	option_state_allocate (&options, MDL);

	/* Execute statements in scope starting with the subnet scope. */
	if (lease)
		execute_statements_in_scope(NULL, packet, NULL, NULL,
					    packet->options, options,
					    &global_scope,
					    lease->subnet->group,
					    NULL, NULL);

	/* Execute statements in the class scopes. */
	for (i = packet -> class_count; i > 0; i--) {
		execute_statements_in_scope
			(NULL, packet, NULL, NULL, packet->options, options,
			 &global_scope, packet->classes[i - 1]->group,
			 lease ? lease->subnet->group : NULL, NULL);
	}

	/* Drop the request if dhcpdeclines are being ignored. */
	oc = lookup_option (&server_universe, options, SV_DECLINES);
	if (!oc ||
	    evaluate_boolean_option_cache (&ignorep, packet, lease,
					   (struct client_state *)0,
					   packet -> options, options,
					   &lease -> scope, oc, MDL)) {
	    /* If we found a lease, mark it as unusable and complain. */
	    if (lease) {
#if defined (FAILOVER_PROTOCOL)
		if (lease -> pool && lease -> pool -> failover_peer) {
		    dhcp_failover_state_t *peer =
			    lease -> pool -> failover_peer;
		    if (peer -> service_state == not_responding ||
			peer -> service_state == service_startup) {
			if (!ignorep)
			    log_info ("%s: ignored%s",
				      peer -> name, peer -> nrr);
			goto out;
		    }

		    /* DHCPDECLINE messages are broadcast, so we can safely
		       ignore the DHCPDECLINE if the peer has the lease.
		       XXX Of course, at this point that information has been
		       lost. */
		}
#endif

		abandon_lease (lease, "declined.");
		status = "abandoned";
	    } else {
		status = "not found";
	    }
	} else
	    status = "ignored";

	if (!ignorep)
		log_info ("%s: %s", msgbuf, status);

#if defined(FAILOVER_PROTOCOL)
      out:
#endif
	if (options)
		option_state_dereference (&options, MDL);
	if (lease)
		lease_dereference (&lease, MDL);
}

void dhcpinform (packet, ms_nulltp)
	struct packet *packet;
	int ms_nulltp;
{
	char msgbuf[1024], *addr_type;
	struct data_string d1, prl, fixed_addr;
	struct option_cache *oc;
	struct option_state *options = NULL;
	struct dhcp_packet raw;
	struct packet outgoing;
	unsigned char dhcpack = DHCPACK;
	struct subnet *subnet = NULL;
	struct iaddr cip, gip, sip;
	unsigned i;
	int nulltp;
	struct sockaddr_in to;
	struct in_addr from;
	isc_boolean_t zeroed_ciaddr;
	struct interface_info *interface;
	int result, h_m_client_ip = 0;
	struct host_decl  *host = NULL, *hp = NULL, *h;
#if defined (DEBUG_INFORM_HOST)
	int h_w_fixed_addr = 0;
#endif

	/* The client should set ciaddr to its IP address, but apparently
	   it's common for clients not to do this, so we'll use their IP
	   source address if they didn't set ciaddr. */
	if (!packet->raw->ciaddr.s_addr) {
		zeroed_ciaddr = ISC_TRUE;
		cip.len = 4;
		memcpy(cip.iabuf, &packet->client_addr.iabuf, 4);
		addr_type = "source";
	} else {
		zeroed_ciaddr = ISC_FALSE;
		cip.len = 4;
		memcpy(cip.iabuf, &packet->raw->ciaddr, 4);
		addr_type = "client";
	}
	sip.len = 4;
	memcpy(sip.iabuf, cip.iabuf, 4);

	if (packet->raw->giaddr.s_addr) {
		gip.len = 4;
		memcpy(gip.iabuf, &packet->raw->giaddr, 4);
		if (zeroed_ciaddr == ISC_TRUE) {
			addr_type = "relay";
			memcpy(sip.iabuf, gip.iabuf, 4);
		}
	} else
		gip.len = 0;

	/* %Audit% This is log output. %2004.06.17,Safe%
	 * If we truncate we hope the user can get a hint from the log.
	 */
	snprintf(msgbuf, sizeof(msgbuf), "DHCPINFORM from %s via %s",
		 piaddr(cip),
		 packet->raw->giaddr.s_addr ?
		 inet_ntoa(packet->raw->giaddr) :
		 packet->interface->name);

	/* If the IP source address is zero, don't respond. */
	if (!memcmp(cip.iabuf, "\0\0\0", 4)) {
		log_info("%s: ignored (null source address).", msgbuf);
		return;
	}

	/* Find the subnet that the client is on. 
	 * CC: Do the link selection / subnet selection
	 */

	option_state_allocate(&options, MDL);

	if ((oc = lookup_option(&agent_universe, packet->options,
				RAI_LINK_SELECT)) == NULL)
		oc = lookup_option(&dhcp_universe, packet->options,
				   DHO_SUBNET_SELECTION);

	memset(&d1, 0, sizeof d1);
	if (oc && evaluate_option_cache(&d1, packet, NULL, NULL,
					packet->options, NULL,
					&global_scope, oc, MDL)) {
		struct option_cache *noc = NULL;

		if (d1.len != 4) {
			log_info("%s: ignored (invalid subnet selection option).", msgbuf);
			option_state_dereference(&options, MDL);
			return;
		}

		memcpy(sip.iabuf, d1.data, 4);
		data_string_forget(&d1, MDL);

		/* Make a copy of the data. */
		if (option_cache_allocate(&noc, MDL)) {
			if (oc->data.len)
				data_string_copy(&noc->data, &oc->data, MDL);
			if (oc->expression)
				expression_reference(&noc->expression,
						     oc->expression, MDL);
			if (oc->option)
				option_reference(&(noc->option), oc->option,
						 MDL);
		}
		save_option(&dhcp_universe, options, noc);
		option_cache_dereference(&noc, MDL);

		if ((zeroed_ciaddr == ISC_TRUE) && (gip.len != 0))
			addr_type = "relay link select";
		else
			addr_type = "selected";
	}

	find_subnet(&subnet, sip, MDL);

	if (subnet == NULL) {
		log_info("%s: unknown subnet for %s address %s",
			 msgbuf, addr_type, piaddr(sip));
		option_state_dereference (&options, MDL);
		return;
	}

	/* We don't respond to DHCPINFORM packets if we're not authoritative.
	   It would be nice if a per-host value could override this, but
	   there's overhead involved in checking this, so let's see how people
	   react first. */
	if (subnet && !subnet -> group -> authoritative) {
		static int eso = 0;
		log_info ("%s: not authoritative for subnet %s",
			  msgbuf, piaddr (subnet -> net));
		if (!eso) {
			log_info ("If this DHCP server is authoritative for%s",
				  " that subnet,");
			log_info ("please write an `authoritative;' directi%s",
				  "ve either in the");
			log_info ("subnet declaration or in some scope that%s",
				  " encloses the");
			log_info ("subnet declaration - for example, write %s",
				  "it at the top");
			log_info ("of the dhcpd.conf file.");
		}
		if (eso++ == 100)
			eso = 0;
		subnet_dereference (&subnet, MDL);
		option_state_dereference (&options, MDL);
		return;
	}
	
	memset (&outgoing, 0, sizeof outgoing);
	memset (&raw, 0, sizeof raw);
	outgoing.raw = &raw;

	maybe_return_agent_options(packet, options);

	/* Execute statements in scope starting with the subnet scope. */
	if (subnet)
		execute_statements_in_scope (NULL, packet, NULL, NULL,
					     packet->options, options,
					     &global_scope, subnet->group,
					     NULL, NULL);
 		
	/* Execute statements in the class scopes. */
	for (i = packet -> class_count; i > 0; i--) {
		execute_statements_in_scope(NULL, packet, NULL, NULL,
					    packet->options, options,
					    &global_scope,
					    packet->classes[i - 1]->group,
					    subnet ? subnet->group : NULL,
					    NULL);
	}

	/*
	 * Process host declarations during DHCPINFORM, 
	 * Try to find a matching host declaration by cli ID or HW addr.
	 *
	 * Look through the host decls for one that matches the
	 * client identifer or the hardware address.  The preference
	 * order is:
	 * client id with matching ip address
	 * hardware address with matching ip address
	 * client id without a ip fixed address
	 * hardware address without a fixed ip address
	 * If found, set host to use its option definitions.
         */
	oc = lookup_option(&dhcp_universe, packet->options,
			   DHO_DHCP_CLIENT_IDENTIFIER);
	memset(&d1, 0, sizeof(d1));
	if (oc &&
	    evaluate_option_cache(&d1, packet, NULL, NULL,
				  packet->options, NULL,
				  &global_scope, oc, MDL)) {
		find_hosts_by_uid(&hp, d1.data, d1.len, MDL);
		data_string_forget(&d1, MDL);

#if defined (DEBUG_INFORM_HOST)
		if (hp)
			log_debug ("dhcpinform: found host by ID "
				   "-- checking fixed-address match");
#endif
		/* check if we have one with fixed-address
		 * matching the client ip first */
		for (h = hp; !h_m_client_ip && h; h = h->n_ipaddr) {
			if (!h->fixed_addr)
				continue;

			memset(&fixed_addr, 0, sizeof(fixed_addr));
			if (!evaluate_option_cache (&fixed_addr, NULL,
						    NULL, NULL, NULL, NULL,
						    &global_scope,
						    h->fixed_addr, MDL))
				continue;

#if defined (DEBUG_INFORM_HOST)
			h_w_fixed_addr++;
#endif
			for (i = 0;
			     (i + cip.len) <= fixed_addr.len;
			     i += cip.len) {
				if (memcmp(fixed_addr.data + i,
					   cip.iabuf, cip.len) == 0) {
#if defined (DEBUG_INFORM_HOST)
					log_debug ("dhcpinform: found "
						   "host with matching "
						   "fixed-address by ID");
#endif
					host_reference(&host, h, MDL);
					h_m_client_ip = 1;
					break;
				}
			}
			data_string_forget(&fixed_addr, MDL);
		}

		/* fallback to a host without fixed-address */
		for (h = hp; !host && h; h = h->n_ipaddr) {
			if (h->fixed_addr)
				continue;

#if defined (DEBUG_INFORM_HOST)
			log_debug ("dhcpinform: found host "
				   "without fixed-address by ID");
#endif
			host_reference(&host, h, MDL);
			break;
		}
		if (hp)
			host_dereference (&hp, MDL);
	}
	if (!host || !h_m_client_ip) {
		find_hosts_by_haddr(&hp, packet->raw->htype,
				    packet->raw->chaddr,
				    packet->raw->hlen, MDL);

#if defined (DEBUG_INFORM_HOST)
		if (hp)
			log_debug ("dhcpinform: found host by HW "
				   "-- checking fixed-address match");
#endif

		/* check if we have one with fixed-address
		 * matching the client ip first */
		for (h = hp; !h_m_client_ip && h; h = h->n_ipaddr) {
			if (!h->fixed_addr)
				continue;

			memset (&fixed_addr, 0, sizeof(fixed_addr));
			if (!evaluate_option_cache (&fixed_addr, NULL,
						    NULL, NULL, NULL, NULL,
						    &global_scope,
						    h->fixed_addr, MDL))
				continue;

#if defined (DEBUG_INFORM_HOST)
			h_w_fixed_addr++;
#endif
			for (i = 0;
			     (i + cip.len) <= fixed_addr.len;
			     i += cip.len) {
				if (memcmp(fixed_addr.data + i,
					   cip.iabuf, cip.len) == 0) {
#if defined (DEBUG_INFORM_HOST)
					log_debug ("dhcpinform: found "
						   "host with matching "
						   "fixed-address by HW");
#endif
					/*
					 * Hmm.. we've found one
					 * without IP by ID and now
					 * (better) one with IP by HW.
					 */
					if(host)
						host_dereference(&host, MDL);
					host_reference(&host, h, MDL);
					h_m_client_ip = 1;
					break;
				}
			}
			data_string_forget(&fixed_addr, MDL);
		}
		/* fallback to a host without fixed-address */
		for (h = hp; !host && h; h = h->n_ipaddr) {
			if (h->fixed_addr)
				continue;

#if defined (DEBUG_INFORM_HOST)
			log_debug ("dhcpinform: found host without "
				   "fixed-address by HW");
#endif
			host_reference (&host, h, MDL);
			break;
		}

		if (hp)
			host_dereference (&hp, MDL);
	}
 
#if defined (DEBUG_INFORM_HOST)
	/* Hmm..: what when there is a host with a fixed-address,
	 * that matches by hw or id, but the fixed-addresses
	 * didn't match client ip?
	 */
	if (h_w_fixed_addr && !h_m_client_ip) {
		log_info ("dhcpinform: matching host with "
			  "fixed-address different than "
			  "client IP detected?!");
	}
#endif

	/* If we have a host_decl structure, run the options
	 * associated with its group. Whether the host decl
	 * struct is old or not. */
	if (host) {
#if defined (DEBUG_INFORM_HOST)
		log_info ("dhcpinform: applying host (group) options");
#endif
		execute_statements_in_scope(NULL, packet, NULL, NULL,
					    packet->options, options,
					    &global_scope, host->group,
					    host->group ?
					      host->group->next : NULL,
					    NULL);
		host_dereference (&host, MDL);
	}

 	/* CC: end of host entry processing.... */
	
	/* Figure out the filename. */
	memset (&d1, 0, sizeof d1);
	oc = lookup_option (&server_universe, options, SV_FILENAME);
	if (oc &&
	    evaluate_option_cache (&d1, packet, (struct lease *)0,
				   (struct client_state *)0,
				   packet -> options, (struct option_state *)0,
				   &global_scope, oc, MDL)) {
		i = d1.len;
		if (i >= sizeof(raw.file)) {
			log_info("file name longer than packet field "
				 "truncated - field: %lu name: %d %.*s", 
				 (unsigned long)sizeof(raw.file), i,
				 (int)i, d1.data);
			i = sizeof(raw.file);
		} else
			raw.file[i] = 0;
		memcpy (raw.file, d1.data, i);
		data_string_forget (&d1, MDL);
	}

	/* Choose a server name as above. */
	oc = lookup_option (&server_universe, options, SV_SERVER_NAME);
	if (oc &&
	    evaluate_option_cache (&d1, packet, (struct lease *)0,
				   (struct client_state *)0,
				   packet -> options, (struct option_state *)0,
				   &global_scope, oc, MDL)) {
		i = d1.len;
		if (i >= sizeof(raw.sname)) {
			log_info("server name longer than packet field "
				 "truncated - field: %lu name: %d %.*s", 
				 (unsigned long)sizeof(raw.sname), i,
				 (int)i, d1.data);
			i = sizeof(raw.sname);
		} else
			raw.sname[i] = 0;
		memcpy (raw.sname, d1.data, i);
		data_string_forget (&d1, MDL);
	}

	/* Set a flag if this client is a lame Microsoft client that NUL
	   terminates string options and expects us to do likewise. */
	nulltp = 0;
	if ((oc = lookup_option (&dhcp_universe, packet -> options,
				 DHO_HOST_NAME))) {
		if (!oc->expression)
			nulltp = oc->flags & OPTION_HAD_NULLS;
	}

	/* Put in DHCP-specific options. */
	i = DHO_DHCP_MESSAGE_TYPE;
	oc = (struct option_cache *)0;
	if (option_cache_allocate (&oc, MDL)) {
		if (make_const_data (&oc -> expression,
				     &dhcpack, 1, 0, 0, MDL)) {
			option_code_hash_lookup(&oc->option,
						dhcp_universe.code_hash,
						&i, 0, MDL);
			save_option (&dhcp_universe, options, oc);
		}
		option_cache_dereference (&oc, MDL);
	}

	get_server_source_address(&from, options, options, packet);

	/* Use the subnet mask from the subnet declaration if no other
	   mask has been provided. */
	i = DHO_SUBNET_MASK;
	if (subnet && !lookup_option (&dhcp_universe, options, i)) {
		oc = (struct option_cache *)0;
		if (option_cache_allocate (&oc, MDL)) {
			if (make_const_data (&oc -> expression,
					     subnet -> netmask.iabuf,
					     subnet -> netmask.len,
					     0, 0, MDL)) {
				option_code_hash_lookup(&oc->option,
							dhcp_universe.code_hash,
							&i, 0, MDL);
				save_option (&dhcp_universe, options, oc);
			}
			option_cache_dereference (&oc, MDL);
		}
	}

	/* If a site option space has been specified, use that for
	   site option codes. */
	i = SV_SITE_OPTION_SPACE;
	if ((oc = lookup_option (&server_universe, options, i)) &&
	    evaluate_option_cache (&d1, packet, (struct lease *)0,
				   (struct client_state *)0,
				   packet -> options, options,
				   &global_scope, oc, MDL)) {
		struct universe *u = (struct universe *)0;

		if (!universe_hash_lookup (&u, universe_hash,
					   (const char *)d1.data, d1.len,
					   MDL)) {
			log_error ("unknown option space %s.", d1.data);
			option_state_dereference (&options, MDL);
			if (subnet)
				subnet_dereference (&subnet, MDL);
			return;
		}

		options -> site_universe = u -> index;
		options->site_code_min = find_min_site_code(u);
		data_string_forget (&d1, MDL);
	} else {
		options -> site_universe = dhcp_universe.index;
		options -> site_code_min = 0; /* Trust me, it works. */
	}

	memset (&prl, 0, sizeof prl);

	/* Use the parameter list from the scope if there is one. */
	oc = lookup_option (&dhcp_universe, options,
			    DHO_DHCP_PARAMETER_REQUEST_LIST);

	/* Otherwise, if the client has provided a list of options
	   that it wishes returned, use it to prioritize.  Otherwise,
	   prioritize based on the default priority list. */

	if (!oc)
		oc = lookup_option (&dhcp_universe, packet -> options,
				    DHO_DHCP_PARAMETER_REQUEST_LIST);

	if (oc)
		evaluate_option_cache (&prl, packet, (struct lease *)0,
				       (struct client_state *)0,
				       packet -> options, options,
				       &global_scope, oc, MDL);

#ifdef DEBUG_PACKET
	dump_packet (packet);
	dump_raw ((unsigned char *)packet -> raw, packet -> packet_length);
#endif

	log_info ("%s", msgbuf);

	/* Figure out the address of the boot file server. */
	if ((oc =
	     lookup_option (&server_universe, options, SV_NEXT_SERVER))) {
		if (evaluate_option_cache (&d1, packet, (struct lease *)0,
					   (struct client_state *)0,
					   packet -> options, options,
					   &global_scope, oc, MDL)) {
			/* If there was more than one answer,
			   take the first. */
			if (d1.len >= 4 && d1.data)
				memcpy (&raw.siaddr, d1.data, 4);
			data_string_forget (&d1, MDL);
		}
	}

	/*
	 * Remove any time options, per section 3.4 RFC 2131
	 */
	delete_option(&dhcp_universe, options, DHO_DHCP_LEASE_TIME);
	delete_option(&dhcp_universe, options, DHO_DHCP_RENEWAL_TIME);
	delete_option(&dhcp_universe, options, DHO_DHCP_REBINDING_TIME);

	/* Set up the option buffer... */
	outgoing.packet_length =
		cons_options (packet, outgoing.raw, (struct lease *)0,
			      (struct client_state *)0,
			      0, packet -> options, options, &global_scope,
			      0, nulltp, 0,
			      prl.len ? &prl : (struct data_string *)0,
			      (char *)0);
	option_state_dereference (&options, MDL);
	data_string_forget (&prl, MDL);

	/* Make sure that the packet is at least as big as a BOOTP packet. */
	if (outgoing.packet_length < BOOTP_MIN_LEN)
		outgoing.packet_length = BOOTP_MIN_LEN;

	raw.giaddr = packet -> raw -> giaddr;
	raw.ciaddr = packet -> raw -> ciaddr;
	memcpy (raw.chaddr, packet -> raw -> chaddr, sizeof raw.chaddr);
	raw.hlen = packet -> raw -> hlen;
	raw.htype = packet -> raw -> htype;

	raw.xid = packet -> raw -> xid;
	raw.secs = packet -> raw -> secs;
	raw.flags = packet -> raw -> flags;
	raw.hops = packet -> raw -> hops;
	raw.op = BOOTREPLY;

#ifdef DEBUG_PACKET
	dump_packet (&outgoing);
	dump_raw ((unsigned char *)&raw, outgoing.packet_length);
#endif

	/* Set up the common stuff... */
	to.sin_family = AF_INET;
#ifdef HAVE_SA_LEN
	to.sin_len = sizeof to;
#endif
	memset (to.sin_zero, 0, sizeof to.sin_zero);

	/* RFC2131 states the server SHOULD unciast to ciaddr.
	 * There are two wrinkles - relays, and when ciaddr is zero.
	 * There's actually no mention of relays at all in rfc2131 in
	 * regard to DHCPINFORM, except to say we might get packets from
	 * clients via them.  Note: relays unicast to clients to the
	 * "yiaddr" address, which servers are forbidden to set when
	 * answering an inform.
	 *
	 * The solution: If ciaddr is zero, and giaddr is set, go via the
	 * relay with the broadcast flag set to help the relay (with no
	 * yiaddr and very likely no chaddr, it will have no idea where to
	 * send the packet).
	 *
	 * If the ciaddr is zero and giaddr is not set, go via the source
	 * IP address (but you are permitted to barf on their shoes).
	 *
	 * If ciaddr is not zero, send the packet there always.
	 */
	if (!raw.ciaddr.s_addr && gip.len) {
		memcpy(&to.sin_addr, gip.iabuf, 4);
		to.sin_port = local_port;
		raw.flags |= htons(BOOTP_BROADCAST);
	} else {
		gip.len = 0;
		memcpy(&to.sin_addr, cip.iabuf, 4);
		to.sin_port = remote_port;
	}

	/* Report what we're sending. */
	snprintf(msgbuf, sizeof msgbuf, "DHCPACK to %s (%s) via", piaddr(cip),
		 (packet->raw->htype && packet->raw->hlen) ?
			print_hw_addr(packet->raw->htype, packet->raw->hlen,
				      packet->raw->chaddr) :
			"<no client hardware address>");
	log_info("%s %s", msgbuf, gip.len ? piaddr(gip) :
					    packet->interface->name);

	errno = 0;
	interface = (fallback_interface ? fallback_interface
		     : packet -> interface);
	result = send_packet(interface, &outgoing, &raw,
			     outgoing.packet_length, from, &to, NULL);
	if (result < 0) {
		log_error ("%s:%d: Failed to send %d byte long packet over %s "
			   "interface.", MDL, outgoing.packet_length,
			   interface->name);
	}


	if (subnet)
		subnet_dereference (&subnet, MDL);
}

void nak_lease (packet, cip)
	struct packet *packet;
	struct iaddr *cip;
{
	struct sockaddr_in to;
	struct in_addr from;
	int result;
	struct dhcp_packet raw;
	unsigned char nak = DHCPNAK;
	struct packet outgoing;
	unsigned i;
	struct option_state *options = (struct option_state *)0;
	struct option_cache *oc = (struct option_cache *)0;

	option_state_allocate (&options, MDL);
	memset (&outgoing, 0, sizeof outgoing);
	memset (&raw, 0, sizeof raw);
	outgoing.raw = &raw;

	/* Set DHCP_MESSAGE_TYPE to DHCPNAK */
	if (!option_cache_allocate (&oc, MDL)) {
		log_error ("No memory for DHCPNAK message type.");
		option_state_dereference (&options, MDL);
		return;
	}
	if (!make_const_data (&oc -> expression, &nak, sizeof nak,
			      0, 0, MDL)) {
		log_error ("No memory for expr_const expression.");
		option_cache_dereference (&oc, MDL);
		option_state_dereference (&options, MDL);
		return;
	}
	i = DHO_DHCP_MESSAGE_TYPE;
	option_code_hash_lookup(&oc->option, dhcp_universe.code_hash,
				&i, 0, MDL);
	save_option (&dhcp_universe, options, oc);
	option_cache_dereference (&oc, MDL);
		     
	/* Set DHCP_MESSAGE to whatever the message is */
	if (!option_cache_allocate (&oc, MDL)) {
		log_error ("No memory for DHCPNAK message type.");
		option_state_dereference (&options, MDL);
		return;
	}
	if (!make_const_data (&oc -> expression,
			      (unsigned char *)dhcp_message,
			      strlen (dhcp_message), 1, 0, MDL)) {
		log_error ("No memory for expr_const expression.");
		option_cache_dereference (&oc, MDL);
		option_state_dereference (&options, MDL);
		return;
	}
	i = DHO_DHCP_MESSAGE;
	option_code_hash_lookup(&oc->option, dhcp_universe.code_hash,
				&i, 0, MDL);
	save_option (&dhcp_universe, options, oc);
	option_cache_dereference (&oc, MDL);

	/*
	 * If we are configured to do so we try to find a server id
	 * option even for NAKS by calling setup_server_source_address().
	 * This function will set up an options list from the global
	 * and subnet scopes before trying to get the source address.
	 * 
	 * Otherwise we simply call get_server_source_address()
	 * directly, without a server options list, this means
	 * we'll get the source address from the interface address.
	 */
#if defined(SERVER_ID_FOR_NAK)
	setup_server_source_address(&from, options, packet);
#else
	get_server_source_address(&from, NULL, options, packet);
#endif /* if defined(SERVER_ID_FOR_NAK) */


	/* If there were agent options in the incoming packet, return
	 * them.  We do not check giaddr to detect the presence of a
	 * relay, as this excludes "l2" relay agents which have no
	 * giaddr to set.
	 */
	if (packet->options->universe_count > agent_universe.index &&
	    packet->options->universes [agent_universe.index]) {
		option_chain_head_reference
		    ((struct option_chain_head **)
		     &(options -> universes [agent_universe.index]),
		     (struct option_chain_head *)
		     packet -> options -> universes [agent_universe.index],
		     MDL);
	}

	/* Do not use the client's requested parameter list. */
	delete_option (&dhcp_universe, packet -> options,
		       DHO_DHCP_PARAMETER_REQUEST_LIST);

	/* Set up the option buffer... */
	outgoing.packet_length =
		cons_options (packet, outgoing.raw, (struct lease *)0,
			      (struct client_state *)0,
			      0, packet -> options, options, &global_scope,
			      0, 0, 0, (struct data_string *)0, (char *)0);
	option_state_dereference (&options, MDL);

/*	memset (&raw.ciaddr, 0, sizeof raw.ciaddr);*/
	if (packet->interface->address_count)
		raw.siaddr = packet->interface->addresses[0];
	raw.giaddr = packet -> raw -> giaddr;
	memcpy (raw.chaddr, packet -> raw -> chaddr, sizeof raw.chaddr);
	raw.hlen = packet -> raw -> hlen;
	raw.htype = packet -> raw -> htype;

	raw.xid = packet -> raw -> xid;
	raw.secs = packet -> raw -> secs;
	raw.flags = packet -> raw -> flags | htons (BOOTP_BROADCAST);
	raw.hops = packet -> raw -> hops;
	raw.op = BOOTREPLY;

	/* Report what we're sending... */
	log_info ("DHCPNAK on %s to %s via %s",
	      piaddr (*cip),
	      print_hw_addr (packet -> raw -> htype,
			     packet -> raw -> hlen,
			     packet -> raw -> chaddr),
	      packet -> raw -> giaddr.s_addr
	      ? inet_ntoa (packet -> raw -> giaddr)
	      : packet -> interface -> name);

#ifdef DEBUG_PACKET
	dump_packet (packet);
	dump_raw ((unsigned char *)packet -> raw, packet -> packet_length);
	dump_packet (&outgoing);
	dump_raw ((unsigned char *)&raw, outgoing.packet_length);
#endif

	/* Set up the common stuff... */
	to.sin_family = AF_INET;
#ifdef HAVE_SA_LEN
	to.sin_len = sizeof to;
#endif
	memset (to.sin_zero, 0, sizeof to.sin_zero);

	/* Make sure that the packet is at least as big as a BOOTP packet. */
	if (outgoing.packet_length < BOOTP_MIN_LEN)
		outgoing.packet_length = BOOTP_MIN_LEN;

	/* If this was gatewayed, send it back to the gateway.
	   Otherwise, broadcast it on the local network. */
	if (raw.giaddr.s_addr) {
		to.sin_addr = raw.giaddr;
		if (raw.giaddr.s_addr != htonl (INADDR_LOOPBACK))
			to.sin_port = local_port;
		else
			to.sin_port = remote_port; /* for testing. */

		if (fallback_interface) {
			result = send_packet(fallback_interface, packet, &raw,
					     outgoing.packet_length, from, &to,
					     NULL);
			if (result < 0) {
				log_error ("%s:%d: Failed to send %d byte long "
					   "packet over %s interface.", MDL,
					   outgoing.packet_length,
					   fallback_interface->name);
			}

			return;
		}
	} else {
		to.sin_addr = limited_broadcast;
		to.sin_port = remote_port;
	}

	errno = 0;
	result = send_packet(packet->interface, packet, &raw,
			     outgoing.packet_length, from, &to, NULL);
        if (result < 0) {
                log_error ("%s:%d: Failed to send %d byte long packet over %s "
                           "interface.", MDL, outgoing.packet_length,
                           packet->interface->name);
        }

}

void ack_lease (packet, lease, offer, when, msg, ms_nulltp, hp)
	struct packet *packet;
	struct lease *lease;
	unsigned int offer;
	TIME when;
	char *msg;
	int ms_nulltp;
	struct host_decl *hp;
{
	struct lease *lt;
	struct lease_state *state;
	struct lease *next;
	struct host_decl *host = (struct host_decl *)0;
	TIME lease_time;
	TIME offered_lease_time;
	struct data_string d1;
	TIME min_lease_time;
	TIME max_lease_time;
	TIME default_lease_time;
	struct option_cache *oc;
	isc_result_t result;
	TIME ping_timeout;
	TIME lease_cltt;
	struct in_addr from;
	TIME remaining_time;
	struct iaddr cip;
#if defined(DELAYED_ACK)
	/* By default we don't do the enqueue */
	isc_boolean_t enqueue = ISC_FALSE;
#endif
	int use_old_lease = 0;

	unsigned i, j;
	int s1;
	int ignorep;
	struct timeval tv;

	/* If we're already acking this lease, don't do it again. */
	if (lease -> state)
		return;

	/* Save original cltt for comparison later. */
	lease_cltt = lease->cltt;

	/* If the lease carries a host record, remember it. */
	if (hp)
		host_reference (&host, hp, MDL);
	else if (lease -> host)
		host_reference (&host, lease -> host, MDL);

	/* Allocate a lease state structure... */
	state = new_lease_state (MDL);
	if (!state)
		log_fatal ("unable to allocate lease state!");
	state -> got_requested_address = packet -> got_requested_address;
	shared_network_reference (&state -> shared_network,
				  packet -> interface -> shared_network, MDL);

	/* See if we got a server identifier option. */
	if (lookup_option (&dhcp_universe,
			   packet -> options, DHO_DHCP_SERVER_IDENTIFIER))
		state -> got_server_identifier = 1;

	maybe_return_agent_options(packet, state->options);

	/* If we are offering a lease that is still currently valid, preserve
	   the events.  We need to do this because if the client does not
	   REQUEST our offer, it will expire in 2 minutes, overriding the
	   expire time in the currently in force lease.  We want the expire
	   events to be executed at that point. */
	if (lease->ends <= cur_time && offer != DHCPOFFER) {
		/* Get rid of any old expiry or release statements - by
		   executing the statements below, we will be inserting new
		   ones if there are any to insert. */
		if (lease->on_star.on_expiry)
			executable_statement_dereference
				(&lease->on_star.on_expiry, MDL);
		if (lease->on_star.on_commit)
			executable_statement_dereference
				(&lease->on_star.on_commit, MDL);
		if (lease->on_star.on_release)
			executable_statement_dereference
				(&lease->on_star.on_release, MDL);
	}

	/* Execute statements in scope starting with the subnet scope. */
	execute_statements_in_scope (NULL, packet, lease,
				     NULL, packet->options,
				     state->options, &lease->scope,
				     lease->subnet->group, NULL, NULL);

	/* If the lease is from a pool, run the pool scope. */
	if (lease->pool)
		(execute_statements_in_scope(NULL, packet, lease, NULL,
					     packet->options, state->options,
					     &lease->scope, lease->pool->group,
					     lease->pool->
						shared_network->group,
					     NULL));

	/* Execute statements from class scopes. */
	for (i = packet -> class_count; i > 0; i--) {
		execute_statements_in_scope(NULL, packet, lease, NULL,
					    packet->options, state->options,
					    &lease->scope,
					    packet->classes[i - 1]->group,
					    (lease->pool ? lease->pool->group
					     : lease->subnet->group),
					    NULL);
	}

	/* See if the client is only supposed to have one lease at a time,
	   and if so, find its other leases and release them.    We can only
	   do this on DHCPREQUEST.    It's a little weird to do this before
	   looking at permissions, because the client might not actually
	   _get_ a lease after we've done the permission check, but the
	   assumption for this option is that the client has exactly one
	   network interface, and will only ever remember one lease.   So
	   if it sends a DHCPREQUEST, and doesn't get the lease, it's already
	   forgotten about its old lease, so we can too. */
	if (packet -> packet_type == DHCPREQUEST &&
	    (oc = lookup_option (&server_universe, state -> options,
				 SV_ONE_LEASE_PER_CLIENT)) &&
	    evaluate_boolean_option_cache (&ignorep,
					   packet, lease,
					   (struct client_state *)0,
					   packet -> options,
					   state -> options, &lease -> scope,
					   oc, MDL)) {
	    struct lease *seek;
	    if (lease -> uid_len) {
		do {
		    seek = (struct lease *)0;
		    find_lease_by_uid (&seek, lease -> uid,
				       lease -> uid_len, MDL);
		    if (!seek)
			break;
		    if (seek == lease && !seek -> n_uid) {
			lease_dereference (&seek, MDL);
			break;
		    }
		    next = (struct lease *)0;

		    /* Don't release expired leases, and don't
		       release the lease we're going to assign. */
		    next = (struct lease *)0;
		    while (seek) {
			if (seek -> n_uid)
			    lease_reference (&next, seek -> n_uid, MDL);
			if (seek != lease &&
			    seek -> binding_state != FTS_RELEASED &&
			    seek -> binding_state != FTS_EXPIRED &&
			    seek -> binding_state != FTS_RESET &&
			    seek -> binding_state != FTS_FREE &&
			    seek -> binding_state != FTS_BACKUP)
				break;
			lease_dereference (&seek, MDL);
			if (next) {
			    lease_reference (&seek, next, MDL);
			    lease_dereference (&next, MDL);
			}
		    }
		    if (next)
			lease_dereference (&next, MDL);
		    if (seek) {
			release_lease (seek, packet);
			lease_dereference (&seek, MDL);
		    } else
			break;
		} while (1);
	    }
	    if (!lease -> uid_len ||
		(host &&
		 !host -> client_identifier.len &&
		 (oc = lookup_option (&server_universe, state -> options,
				      SV_DUPLICATES)) &&
		 !evaluate_boolean_option_cache (&ignorep, packet, lease,
						 (struct client_state *)0,
						 packet -> options,
						 state -> options,
						 &lease -> scope,
						 oc, MDL))) {
		do {
		    seek = (struct lease *)0;
		    find_lease_by_hw_addr
			    (&seek, lease -> hardware_addr.hbuf,
			     lease -> hardware_addr.hlen, MDL);
		    if (!seek)
			    break;
		    if (seek == lease && !seek -> n_hw) {
			    lease_dereference (&seek, MDL);
			    break;
		    }
		    next = (struct lease *)0;
		    while (seek) {
			if (seek -> n_hw)
			    lease_reference (&next, seek -> n_hw, MDL);
			if (seek != lease &&
			    seek -> binding_state != FTS_RELEASED &&
			    seek -> binding_state != FTS_EXPIRED &&
			    seek -> binding_state != FTS_RESET &&
			    seek -> binding_state != FTS_FREE &&
			    seek -> binding_state != FTS_BACKUP)
				break;
			lease_dereference (&seek, MDL);
			if (next) {
			    lease_reference (&seek, next, MDL);
			    lease_dereference (&next, MDL);
			}
		    }
		    if (next)
			lease_dereference (&next, MDL);
		    if (seek) {
			release_lease (seek, packet);
			lease_dereference (&seek, MDL);
		    } else
			break;
		} while (1);
	    }
	}
	

	/* Make sure this packet satisfies the configured minimum
	   number of seconds. */
	memset (&d1, 0, sizeof d1);
	if (offer == DHCPOFFER &&
	    (oc = lookup_option (&server_universe, state -> options,
				 SV_MIN_SECS))) {
		if (evaluate_option_cache (&d1, packet, lease,
					   (struct client_state *)0,
					   packet -> options, state -> options,
					   &lease -> scope, oc, MDL)) {
			if (d1.len &&
			    ntohs (packet -> raw -> secs) < d1.data [0]) {
				log_info("%s: configured min-secs value (%d) "
					 "is greater than secs field (%d).  "
					 "message dropped.", msg, d1.data[0],
					 ntohs(packet->raw->secs));
				data_string_forget (&d1, MDL);
				free_lease_state (state, MDL);
				if (host)
					host_dereference (&host, MDL);
				return;
			}
			data_string_forget (&d1, MDL);
		}
	}

	/* Try to find a matching host declaration for this lease.
	 */
	if (!host) {
		struct host_decl *hp = (struct host_decl *)0;
		struct host_decl *h;

		/* Try to find a host_decl that matches the client
		   identifier or hardware address on the packet, and
		   has no fixed IP address.   If there is one, hang
		   it off the lease so that its option definitions
		   can be used. */
		oc = lookup_option (&dhcp_universe, packet -> options,
				    DHO_DHCP_CLIENT_IDENTIFIER);
		if (oc &&
		    evaluate_option_cache (&d1, packet, lease,
					   (struct client_state *)0,
					   packet -> options, state -> options,
					   &lease -> scope, oc, MDL)) {
			find_hosts_by_uid (&hp, d1.data, d1.len, MDL);
			data_string_forget (&d1, MDL);
			for (h = hp; h; h = h -> n_ipaddr) {
				if (!h -> fixed_addr)
					break;
			}
			if (h)
				host_reference (&host, h, MDL);
			if (hp != NULL)
				host_dereference(&hp, MDL);
		}
		if (!host) {
			find_hosts_by_haddr (&hp,
					     packet -> raw -> htype,
					     packet -> raw -> chaddr,
					     packet -> raw -> hlen,
					     MDL);
			for (h = hp; h; h = h -> n_ipaddr) {
				if (!h -> fixed_addr)
					break;
			}
			if (h)
				host_reference (&host, h, MDL);
			if (hp != NULL)
				host_dereference(&hp, MDL);
		}
		if (!host) {
			find_hosts_by_option(&hp, packet,
					     packet->options, MDL);
			for (h = hp; h; h = h -> n_ipaddr) {
				if (!h -> fixed_addr)
					break;
			}
			if (h)
				host_reference (&host, h, MDL);
			if (hp != NULL)
				host_dereference(&hp, MDL);
		}
	}

	/* If we have a host_decl structure, run the options associated
	   with its group.  Whether the host decl struct is old or not. */
	if (host)
		execute_statements_in_scope (NULL, packet, lease, NULL,
					     packet->options, state->options,
					     &lease->scope, host->group,
					     (lease->pool
					      ? lease->pool->group
					      : lease->subnet->group),
					     NULL);

	/* Drop the request if it's not allowed for this client.   By
	   default, unknown clients are allowed. */
	if (!host &&
	    (oc = lookup_option (&server_universe, state -> options,
				 SV_BOOT_UNKNOWN_CLIENTS)) &&
	    !evaluate_boolean_option_cache (&ignorep,
					    packet, lease,
					    (struct client_state *)0,
					    packet -> options,
					    state -> options,
					    &lease -> scope, oc, MDL)) {
		if (!ignorep)
			log_info ("%s: unknown client", msg);
		free_lease_state (state, MDL);
		if (host)
			host_dereference (&host, MDL);
		return;
	} 

	/* Drop the request if it's not allowed for this client. */
	if (!offer &&
	    (oc = lookup_option (&server_universe, state -> options,
				   SV_ALLOW_BOOTP)) &&
	    !evaluate_boolean_option_cache (&ignorep,
					    packet, lease,
					    (struct client_state *)0,
					    packet -> options,
					    state -> options,
					    &lease -> scope, oc, MDL)) {
		if (!ignorep)
			log_info ("%s: bootp disallowed", msg);
		free_lease_state (state, MDL);
		if (host)
			host_dereference (&host, MDL);
		return;
	} 

	/* Drop the request if booting is specifically denied. */
	oc = lookup_option (&server_universe, state -> options,
			    SV_ALLOW_BOOTING);
	if (oc &&
	    !evaluate_boolean_option_cache (&ignorep,
					    packet, lease,
					    (struct client_state *)0,
					    packet -> options,
					    state -> options,
					    &lease -> scope, oc, MDL)) {
		if (!ignorep)
			log_info ("%s: booting disallowed", msg);
		free_lease_state (state, MDL);
		if (host)
			host_dereference (&host, MDL);
		return;
	}

	/* If we are configured to do per-class billing, do it. */
	if (have_billing_classes && !(lease -> flags & STATIC_LEASE)) {
		/* See if the lease is currently being billed to a
		   class, and if so, whether or not it can continue to
		   be billed to that class. */
		if (lease -> billing_class) {
			for (i = 0; i < packet -> class_count; i++)
				if (packet -> classes [i] ==
				    lease -> billing_class)
					break;
			if (i == packet -> class_count)
				unbill_class (lease, lease -> billing_class);
		}

		/* If we don't have an active billing, see if we need
		   one, and if we do, try to do so. */
		if (lease->billing_class == NULL) {
			char *cname = "";
			int bill = 0;

			for (i = 0; i < packet->class_count; i++) {
				struct class *billclass, *subclass;

				billclass = packet->classes[i];
				if (billclass->lease_limit) {
					bill++;
					if (bill_class(lease, billclass))
						break;

					subclass = billclass->superclass;
					if (subclass == NULL)
						cname = subclass->name;
					else
						cname = billclass->name;
				}
			}
			if (bill != 0 && i == packet->class_count) {
				log_info("%s: no available billing: lease "
					 "limit reached in all matching "
					 "classes (last: '%s')", msg, cname);
				free_lease_state(state, MDL);
				if (host)
					host_dereference(&host, MDL);
				return;
			}

			/*
			 * If this is an offer, undo the billing.  We go
			 * through all the steps above to bill a class so
			 * we can hit the 'no available billing' mark and
			 * abort without offering.  But it just doesn't make
			 * sense to permanently bill a class for a non-active
			 * lease.  This means on REQUEST, we will bill this
			 * lease again (if there is a REQUEST).
			 */
			if (offer == DHCPOFFER &&
			    lease->billing_class != NULL &&
			    lease->binding_state != FTS_ACTIVE)
				unbill_class(lease, lease->billing_class);
		}
	}

	/* Figure out the filename. */
	oc = lookup_option (&server_universe, state -> options, SV_FILENAME);
	if (oc)
		evaluate_option_cache (&state -> filename, packet, lease,
				       (struct client_state *)0,
				       packet -> options, state -> options,
				       &lease -> scope, oc, MDL);

	/* Choose a server name as above. */
	oc = lookup_option (&server_universe, state -> options,
			    SV_SERVER_NAME);
	if (oc)
		evaluate_option_cache (&state -> server_name, packet, lease,
				       (struct client_state *)0,
				       packet -> options, state -> options,
				       &lease -> scope, oc, MDL);

	/* At this point, we have a lease that we can offer the client.
	   Now we construct a lease structure that contains what we want,
	   and call supersede_lease to do the right thing with it. */
	lt = (struct lease *)0;
	result = lease_allocate (&lt, MDL);
	if (result != ISC_R_SUCCESS) {
		log_info ("%s: can't allocate temporary lease structure: %s",
			  msg, isc_result_totext (result));
		free_lease_state (state, MDL);
		if (host)
			host_dereference (&host, MDL);
		return;
	}
		
	/* Use the ip address of the lease that we finally found in
	   the database. */
	lt -> ip_addr = lease -> ip_addr;

	/* Start now. */
	lt -> starts = cur_time;

	/* Figure out how long a lease to assign.    If this is a
	   dynamic BOOTP lease, its duration must be infinite. */
	if (offer) {
		lt->flags &= ~BOOTP_LEASE;

		default_lease_time = DEFAULT_DEFAULT_LEASE_TIME;
		if ((oc = lookup_option (&server_universe, state -> options,
					 SV_DEFAULT_LEASE_TIME))) {
			if (evaluate_option_cache (&d1, packet, lease,
						   (struct client_state *)0,
						   packet -> options,
						   state -> options,
						   &lease -> scope, oc, MDL)) {
				if (d1.len == sizeof (u_int32_t))
					default_lease_time =
						getULong (d1.data);
				data_string_forget (&d1, MDL);
			}
		}

		if ((oc = lookup_option (&dhcp_universe, packet -> options,
					 DHO_DHCP_LEASE_TIME)))
			s1 = evaluate_option_cache (&d1, packet, lease,
						    (struct client_state *)0,
						    packet -> options,
						    state -> options,
						    &lease -> scope, oc, MDL);
		else
			s1 = 0;

		if (s1 && (d1.len == 4)) {
			u_int32_t ones = 0xffffffff;

			/* One potential use of reserved leases is to allow
			 * clients to signal reservation of their lease.  They
			 * can kinda sorta do this, if you squint hard enough,
			 * by supplying an 'infinite' requested-lease-time
			 * option.  This is generally bad practice...you want
			 * clients to return to the server on at least some
			 * period (days, months, years) to get up-to-date
			 * config state.  So;
			 *
			 * 1) A client requests 0xffffffff lease-time.
			 * 2) The server reserves the lease, and assigns a
			 *    <= max_lease_time lease-time to the client, which
			 *    we presume is much smaller than 0xffffffff.
			 * 3) The client ultimately fails to renew its lease
			 *    (all clients go offline at some point).
			 * 4) The server retains the reservation, although
			 *    the lease expires and passes through those states
			 *    as normal, it's placed in the 'reserved' queue,
			 *    and is under no circumstances allocated to any
			 *    clients.
			 *
			 * Whether the client knows its reserving its lease or
			 * not, this can be a handy tool for a sysadmin.
			 */
			if ((memcmp(d1.data, &ones, 4) == 0) &&
			    (oc = lookup_option(&server_universe,
						state->options,
						SV_RESERVE_INFINITE)) &&
			    evaluate_boolean_option_cache(&ignorep, packet,
						lease, NULL, packet->options,
						state->options, &lease->scope,
						oc, MDL)) {
				lt->flags |= RESERVED_LEASE;
				if (!ignorep)
					log_info("Infinite-leasetime "
						 "reservation made on %s.",
						 piaddr(lt->ip_addr));
			}

			lease_time = getULong (d1.data);
		} else
			lease_time = default_lease_time;

		if (s1)
			data_string_forget(&d1, MDL);

		/* See if there's a maximum lease time. */
		max_lease_time = DEFAULT_MAX_LEASE_TIME;
		if ((oc = lookup_option (&server_universe, state -> options,
					 SV_MAX_LEASE_TIME))) {
			if (evaluate_option_cache (&d1, packet, lease,
						   (struct client_state *)0,
						   packet -> options,
						   state -> options,
						   &lease -> scope, oc, MDL)) {
				if (d1.len == sizeof (u_int32_t))
					max_lease_time =
						getULong (d1.data);
				data_string_forget (&d1, MDL);
			}
		}

		/* Enforce the maximum lease length. */
		if (lease_time < 0 /* XXX */
		    || lease_time > max_lease_time)
			lease_time = max_lease_time;
			
		min_lease_time = DEFAULT_MIN_LEASE_TIME;
		if (min_lease_time > max_lease_time)
			min_lease_time = max_lease_time;

		if ((oc = lookup_option (&server_universe, state -> options,
					 SV_MIN_LEASE_TIME))) {
			if (evaluate_option_cache (&d1, packet, lease,
						   (struct client_state *)0,
						   packet -> options,
						   state -> options,
						   &lease -> scope, oc, MDL)) {
				if (d1.len == sizeof (u_int32_t))
					min_lease_time = getULong (d1.data);
				data_string_forget (&d1, MDL);
			}
		}

		/* CC: If there are less than
		   adaptive-lease-time-threshold % free leases,
		     hand out only short term leases */

		memset(&d1, 0, sizeof(d1));
		if (lease->pool &&
		    (oc = lookup_option(&server_universe, state->options,
					SV_ADAPTIVE_LEASE_TIME_THRESHOLD)) &&
		    evaluate_option_cache(&d1, packet, lease, NULL,
					  packet->options, state->options,
					  &lease->scope, oc, MDL)) {
			if (d1.len == 1 && d1.data[0] > 0 &&
			    d1.data[0] < 100) {
				TIME adaptive_time;
				int poolfilled, total, count;

				if (min_lease_time)
					adaptive_time = min_lease_time;
				else
					adaptive_time = DEFAULT_MIN_LEASE_TIME;

				/* Allow the client to keep its lease. */
				if (lease->ends - cur_time > adaptive_time)
					adaptive_time = lease->ends - cur_time;

				count = lease->pool->lease_count;
				total = count - (lease->pool->free_leases +
						 lease->pool->backup_leases);

				poolfilled = (total > (INT_MAX / 100)) ?
					     total / (count / 100) :
					     (total * 100) / count;

				log_debug("Adap-lease: Total: %d, Free: %d, "
					  "Ends: %d, Adaptive: %d, Fill: %d, "
					  "Threshold: %d",
					  lease->pool->lease_count,
					  lease->pool->free_leases,
					  (int)(lease->ends - cur_time),
					  (int)adaptive_time, poolfilled,
					  d1.data[0]);

				if (poolfilled >= d1.data[0] &&
				    lease_time > adaptive_time) {
					log_info("Pool over threshold, time "
						 "for %s reduced from %d to "
						 "%d.", piaddr(lease->ip_addr),
						 (int)lease_time,
						 (int)adaptive_time);

					lease_time = adaptive_time;
				}
			}
			data_string_forget(&d1, MDL);
		}

		/* a client requests an address which is not yet active*/
		if (lease->pool && lease->pool->valid_from && 
                    cur_time < lease->pool->valid_from) {
			/* NAK leases before pool activation date */
			cip.len = 4;
			memcpy (cip.iabuf, &lt->ip_addr.iabuf, 4);
			nak_lease(packet, &cip);
			free_lease_state (state, MDL);
			lease_dereference (&lt, MDL);
			if (host)
				host_dereference (&host, MDL);
			return;
			
		}

		/* CC:
		a) NAK current lease if past the expiration date
		b) extend lease only up to the expiration date, but not
		below min-lease-time
		Setting min-lease-time is essential for this to work!
		The value of min-lease-time determines the lenght
		of the transition window:
		A client renewing a second before the deadline will
		get a min-lease-time lease. Since the current ip might not
		be routable after the deadline, the client will
		be offline until it DISCOVERS again. Otherwise it will
		receive a NAK at T/2.
		A min-lease-time of 6 seconds effectively switches over
		all clients in this pool very quickly.
			*/
 
		if (lease->pool && lease->pool->valid_until) {
			if (cur_time >= lease->pool->valid_until) {
				/* NAK leases after pool expiration date */
				cip.len = 4;
				memcpy (cip.iabuf, &lt->ip_addr.iabuf, 4);
				nak_lease(packet, &cip);
				free_lease_state (state, MDL);
				lease_dereference (&lt, MDL);
				if (host)
					host_dereference (&host, MDL);
				return;
			}
			remaining_time = lease->pool->valid_until - cur_time;
			if (lease_time > remaining_time)
				lease_time = remaining_time;
		}
 
		if (lease_time < min_lease_time) {
			if (min_lease_time)
				lease_time = min_lease_time;
			else
				lease_time = default_lease_time;
		}


#if defined (FAILOVER_PROTOCOL)
		/* Okay, we know the lease duration.   Now check the
		   failover state, if any. */
		if (lease -> pool && lease -> pool -> failover_peer) {
			TIME new_lease_time = lease_time;
			dhcp_failover_state_t *peer =
			    lease -> pool -> failover_peer;

			/* Copy previous lease failover ack-state. */
			lt->tsfp = lease->tsfp;
			lt->atsfp = lease->atsfp;

			/* cltt set below */

			/* Lease times less than MCLT are not a concern. */
			if (lease_time > peer->mclt) {
				/* Each server can only offer a lease time
				 * that is either equal to MCLT (at least),
				 * or up to TSFP+MCLT.  Only if the desired
				 * lease time falls within TSFP+MCLT, can
				 * the server allow it.
				 */
				if (lt->tsfp <= cur_time)
					new_lease_time = peer->mclt;
				else if ((cur_time + lease_time) >
					 (lt->tsfp + peer->mclt))
					new_lease_time = (lt->tsfp - cur_time)
								+ peer->mclt;
			}

			/* Update potential expiry.  Allow for the desired
			 * lease time plus one half the actual (whether
			 * modified downward or not) lease time, which is
			 * actually an estimate of when the client will
			 * renew.  This way, the client will be able to get
			 * the desired lease time upon renewal.
			 */
			if (offer == DHCPACK) {
				lt->tstp = cur_time + lease_time +
						(new_lease_time / 2);

				/* If we reduced the potential expiry time,
				 * make sure we don't offer an old-expiry-time
				 * lease for this lease before the change is
				 * ack'd.
				 */
				if (lt->tstp < lt->tsfp)
					lt->tsfp = lt->tstp;
			} else
				lt->tstp = lease->tstp;

			/* Use failover-modified lease time.  */
			lease_time = new_lease_time;
		}
#endif /* FAILOVER_PROTOCOL */

		/* If the lease duration causes the time value to wrap,
		   use the maximum expiry time. */
		if (cur_time + lease_time < cur_time)
			state -> offered_expiry = MAX_TIME - 1;
		else
			state -> offered_expiry = cur_time + lease_time;
		if (when)
			lt -> ends = when;
		else
			lt -> ends = state -> offered_expiry;

		/* Don't make lease active until we actually get a
		   DHCPREQUEST. */
		if (offer == DHCPACK)
			lt -> next_binding_state = FTS_ACTIVE;
		else
			lt -> next_binding_state = lease -> binding_state;
	} else {
		lt->flags |= BOOTP_LEASE;

		lease_time = MAX_TIME - cur_time;

		if ((oc = lookup_option (&server_universe, state -> options,
					 SV_BOOTP_LEASE_LENGTH))) {
			if (evaluate_option_cache (&d1, packet, lease,
						   (struct client_state *)0,
						   packet -> options,
						   state -> options,
						   &lease -> scope, oc, MDL)) {
				if (d1.len == sizeof (u_int32_t))
					lease_time = getULong (d1.data);
				data_string_forget (&d1, MDL);
			}
		}

		if ((oc = lookup_option (&server_universe, state -> options,
					 SV_BOOTP_LEASE_CUTOFF))) {
			if (evaluate_option_cache (&d1, packet, lease,
						   (struct client_state *)0,
						   packet -> options,
						   state -> options,
						   &lease -> scope, oc, MDL)) {
				if (d1.len == sizeof (u_int32_t))
					lease_time = (getULong (d1.data) -
						      cur_time);
				data_string_forget (&d1, MDL);
			}
		}

		lt -> ends = state -> offered_expiry = cur_time + lease_time;
		lt -> next_binding_state = FTS_ACTIVE;
	}

	/* Update Client Last Transaction Time. */
	lt->cltt = cur_time;

	/* See if we want to record the uid for this client */
	oc = lookup_option(&server_universe, state->options,
			   SV_IGNORE_CLIENT_UIDS);
	if ((oc == NULL) ||
	    !evaluate_boolean_option_cache(&ignorep, packet, lease, NULL,
					   packet->options, state->options,
					   &lease->scope, oc, MDL)) {
	
		/* Record the uid, if given... */
		oc = lookup_option (&dhcp_universe, packet -> options,
				    DHO_DHCP_CLIENT_IDENTIFIER);
		if (oc &&
		    evaluate_option_cache(&d1, packet, lease, NULL,
					  packet->options, state->options,
					  &lease->scope, oc, MDL)) {
			if (d1.len <= sizeof(lt->uid_buf)) {
				memcpy(lt->uid_buf, d1.data, d1.len);
				lt->uid = lt->uid_buf;
				lt->uid_max = sizeof(lt->uid_buf);
				lt->uid_len = d1.len;
			} else {
				unsigned char *tuid;
				lt->uid_max = d1.len;
				lt->uid_len = d1.len;
				tuid = (unsigned char *)dmalloc(lt->uid_max,
								MDL);
				/* XXX inelegant */
				if (!tuid)
					log_fatal ("no memory for large uid.");
				memcpy(tuid, d1.data, lt->uid_len);
				lt->uid = tuid;
			}
			data_string_forget (&d1, MDL);
		}
	}

	if (host) {
		host_reference (&lt -> host, host, MDL);
		host_dereference (&host, MDL);
	}
	if (lease -> subnet)
		subnet_reference (&lt -> subnet, lease -> subnet, MDL);
	if (lease -> billing_class)
		class_reference (&lt -> billing_class,
				 lease -> billing_class, MDL);

	/* Set a flag if this client is a broken client that NUL
	   terminates string options and expects us to do likewise. */
	if (ms_nulltp)
		lease -> flags |= MS_NULL_TERMINATION;
	else
		lease -> flags &= ~MS_NULL_TERMINATION;

	/* Save any bindings. */
	if (lease -> scope) {
		binding_scope_reference (&lt -> scope, lease -> scope, MDL);
		binding_scope_dereference (&lease -> scope, MDL);
	}
	if (lease -> agent_options)
		option_chain_head_reference (&lt -> agent_options,
					     lease -> agent_options, MDL);

	/* Save the vendor-class-identifier for DHCPLEASEQUERY. */
	oc = lookup_option(&dhcp_universe, packet->options,
			   DHO_VENDOR_CLASS_IDENTIFIER);
	if (oc != NULL &&
	    evaluate_option_cache(&d1, packet, NULL, NULL, packet->options,
				  NULL, &lease->scope, oc, MDL)) {
		if (d1.len != 0) {
			bind_ds_value(&lease->scope, "vendor-class-identifier",
				      &d1);
		}

		data_string_forget(&d1, MDL);
	}

	/* If we got relay agent information options from the packet, then
	 * cache them for renewal in case the relay agent can't supply them
	 * when the client unicasts.  The options may be from an addressed
	 * "l3" relay, or from an unaddressed "l2" relay which does not set
	 * giaddr.
	 */
	if (!packet->agent_options_stashed &&
	    (packet->options != NULL) &&
	    packet->options->universe_count > agent_universe.index &&
	    packet->options->universes[agent_universe.index] != NULL) {
	    oc = lookup_option (&server_universe, state -> options,
				SV_STASH_AGENT_OPTIONS);
	    if (!oc ||
		evaluate_boolean_option_cache (&ignorep, packet, lease,
					       (struct client_state *)0,
					       packet -> options,
					       state -> options,
					       &lease -> scope, oc, MDL)) {
		if (lt -> agent_options)
		    option_chain_head_dereference (&lt -> agent_options, MDL);
		option_chain_head_reference
			(&lt -> agent_options,
			 (struct option_chain_head *)
			 packet -> options -> universes [agent_universe.index],
			 MDL);
	    }
	}

	/* Replace the old lease hostname with the new one, if it's changed. */
	oc = lookup_option (&dhcp_universe, packet -> options, DHO_HOST_NAME);
	if (oc)
		s1 = evaluate_option_cache (&d1, packet, (struct lease *)0,
					    (struct client_state *)0,
					    packet -> options,
					    (struct option_state *)0,
					    &global_scope, oc, MDL);
	else
		s1 = 0;

	if (oc && s1 &&
	    lease -> client_hostname &&
	    strlen (lease -> client_hostname) == d1.len &&
	    !memcmp (lease -> client_hostname, d1.data, d1.len)) {
		/* Hasn't changed. */
		data_string_forget (&d1, MDL);
		lt -> client_hostname = lease -> client_hostname;
		lease -> client_hostname = (char *)0;
	} else if (oc && s1) {
		lt -> client_hostname = dmalloc (d1.len + 1, MDL);
		if (!lt -> client_hostname)
			log_error ("no memory for client hostname.");
		else {
			memcpy (lt -> client_hostname, d1.data, d1.len);
			lt -> client_hostname [d1.len] = 0;
		}
		data_string_forget (&d1, MDL);
	}

	/* Record the hardware address, if given... */
	lt -> hardware_addr.hlen = packet -> raw -> hlen + 1;
	lt -> hardware_addr.hbuf [0] = packet -> raw -> htype;
	memcpy (&lt -> hardware_addr.hbuf [1], packet -> raw -> chaddr,
		sizeof packet -> raw -> chaddr);

	lt -> flags = lease -> flags & ~PERSISTENT_FLAGS;

	/* If there are statements to execute when the lease is
	   committed, execute them. */
	if (lease->on_star.on_commit && (!offer || offer == DHCPACK)) {
		execute_statements (NULL, packet, lt, NULL, packet->options,
				    state->options, &lt->scope,
				    lease->on_star.on_commit, NULL);
		if (lease->on_star.on_commit)
			executable_statement_dereference
				(&lease->on_star.on_commit, MDL);
	}

#ifdef NSUPDATE
	/* Perform DDNS updates, if configured to. */
	if ((!offer || offer == DHCPACK) &&
	    (!(oc = lookup_option (&server_universe, state -> options,
				   SV_DDNS_UPDATES)) ||
	     evaluate_boolean_option_cache (&ignorep, packet, lt,
					    (struct client_state *)0,
					    packet -> options,
					    state -> options,
					    &lt -> scope, oc, MDL))) {
		ddns_updates(packet, lt, lease, NULL, NULL, state->options);
	}
#endif /* NSUPDATE */

	/* Don't call supersede_lease on a mocked-up lease. */
	if (lease -> flags & STATIC_LEASE) {
		/* Copy the hardware address into the static lease
		   structure. */
		lease -> hardware_addr.hlen = packet -> raw -> hlen + 1;
		lease -> hardware_addr.hbuf [0] = packet -> raw -> htype;
		memcpy (&lease -> hardware_addr.hbuf [1],
			packet -> raw -> chaddr,
			sizeof packet -> raw -> chaddr); /* XXX */
	} else {
		int commit = (!offer || (offer == DHCPACK));
		int thresh = DEFAULT_CACHE_THRESHOLD;

		/*
		 * Check if the lease was issued recently, if so replay the 
		 * current lease and do not require a database sync event.  
		 * Recently is defined as being issued less than a given 
		 * percentage of the lease previously. The percentage can be 
		 * chosen either from a default value or via configuration.
		 *
		 */
		if ((oc = lookup_option(&server_universe, state->options,
					SV_CACHE_THRESHOLD)) &&
		    evaluate_option_cache(&d1, packet, lt, NULL,
					  packet->options, state->options,
					  &lt->scope, oc, MDL)) {
			if (d1.len == 1 && (d1.data[0] < 100))
				thresh = d1.data[0];

			data_string_forget(&d1, MDL);
		}

		/*
		 * We check on ddns_cb to see if the ddns code has
		 * updated the lt structure.  We could probably simply
		 * copy the ddns_cb pointer in that case but lets be
		 * simple and safe and update the entire lease.
		 */
		if ((lt->ddns_cb == NULL) &&
		    (thresh > 0) && (offer == DHCPACK) &&
		    (lease->binding_state == FTS_ACTIVE)) {
			int limit;
			int prev_lease = lease->ends - lease->starts;

			/* it is better to avoid division by 0 */
			if (prev_lease <= (INT_MAX / thresh))
				limit = prev_lease * thresh / 100;
			else
				limit = prev_lease / 100 * thresh;

			if ((lt->starts - lease->starts) <= limit) {
				lt->starts = lease->starts;
				state->offered_expiry = lt->ends = lease->ends;
				commit = 0;
				use_old_lease = 1;
			}
		}

#if !defined(DELAYED_ACK)
		/* Install the new information on 'lt' onto the lease at
		 * 'lease'.  If this is a DHCPOFFER, it is a 'soft' promise,
		 * if it is a DHCPACK, it is a 'hard' binding, so it needs
		 * to be recorded and propogated immediately.  If the update
		 * fails, don't ACK it (or BOOTREPLY) either; we may give
		 * the same lease to another client later, and that would be
		 * a conflict.
		 */
		if ((use_old_lease == 0) &&
		    !supersede_lease(lease, lt, commit,
				     offer == DHCPACK, offer == DHCPACK)) {
#else /* defined(DELAYED_ACK) */
		/*
		 * If there already isn't a need for a lease commit, and we
		 * can just answer right away, set a flag to indicate this.
		 */
		if (commit)
			enqueue = ISC_TRUE;

		/* Install the new information on 'lt' onto the lease at
		 * 'lease'.  We will not 'commit' this information to disk
		 * yet (fsync()), we will 'propogate' the information if
		 * this is BOOTP or a DHCPACK, but we will not 'pimmediate'ly
		 * transmit failover binding updates (this is delayed until
		 * after the fsync()).  If the update fails, don't ACK it (or
		 * BOOTREPLY either); we may give the same lease out to a
		 * different client, and that would be a conflict.
		 */
		if ((use_old_lease == 0) &&
		    !supersede_lease(lease, lt, 0,
				     !offer || offer == DHCPACK, 0)) {
#endif
			log_info ("%s: database update failed", msg);
			free_lease_state (state, MDL);
			lease_dereference (&lt, MDL);
			return;
		}
	}
	lease_dereference (&lt, MDL);

	/* Remember the interface on which the packet arrived. */
	state -> ip = packet -> interface;

	/* Remember the giaddr, xid, secs, flags and hops. */
	state -> giaddr = packet -> raw -> giaddr;
	state -> ciaddr = packet -> raw -> ciaddr;
	state -> xid = packet -> raw -> xid;
	state -> secs = packet -> raw -> secs;
	state -> bootp_flags = packet -> raw -> flags;
	state -> hops = packet -> raw -> hops;
	state -> offer = offer;

	/* If we're always supposed to broadcast to this client, set
	   the broadcast bit in the bootp flags field. */
	if ((oc = lookup_option (&server_universe, state -> options,
				SV_ALWAYS_BROADCAST)) &&
	    evaluate_boolean_option_cache (&ignorep, packet, lease,
					   (struct client_state *)0,
					   packet -> options, state -> options,
					   &lease -> scope, oc, MDL))
		state -> bootp_flags |= htons (BOOTP_BROADCAST);

	/* Get the Maximum Message Size option from the packet, if one
	   was sent. */
	oc = lookup_option (&dhcp_universe, packet -> options,
			    DHO_DHCP_MAX_MESSAGE_SIZE);
	if (oc &&
	    evaluate_option_cache (&d1, packet, lease,
				   (struct client_state *)0,
				   packet -> options, state -> options,
				   &lease -> scope, oc, MDL)) {
		if (d1.len == sizeof (u_int16_t))
			state -> max_message_size = getUShort (d1.data);
		data_string_forget (&d1, MDL);
	} else {
		oc = lookup_option (&dhcp_universe, state -> options,
				    DHO_DHCP_MAX_MESSAGE_SIZE);
		if (oc &&
		    evaluate_option_cache (&d1, packet, lease,
					   (struct client_state *)0,
					   packet -> options, state -> options,
					   &lease -> scope, oc, MDL)) {
			if (d1.len == sizeof (u_int16_t))
				state -> max_message_size =
					getUShort (d1.data);
			data_string_forget (&d1, MDL);
		}
	}

	/* Get the Subnet Selection option from the packet, if one
	   was sent. */
	if ((oc = lookup_option (&dhcp_universe, packet -> options,
				 DHO_SUBNET_SELECTION))) {

		/* Make a copy of the data. */
		struct option_cache *noc = (struct option_cache *)0;
		if (option_cache_allocate (&noc, MDL)) {
			if (oc -> data.len)
				data_string_copy (&noc -> data,
						  &oc -> data, MDL);
			if (oc -> expression)
				expression_reference (&noc -> expression,
						      oc -> expression, MDL);
			if (oc -> option)
				option_reference(&(noc->option), oc->option,
						 MDL);

			save_option (&dhcp_universe, state -> options, noc);
			option_cache_dereference (&noc, MDL);
		}
	}

	/* Now, if appropriate, put in DHCP-specific options that
	   override those. */
	if (state -> offer) {
		i = DHO_DHCP_MESSAGE_TYPE;
		oc = (struct option_cache *)0;
		if (option_cache_allocate (&oc, MDL)) {
			if (make_const_data (&oc -> expression,
					     &state -> offer, 1, 0, 0, MDL)) {
				option_code_hash_lookup(&oc->option,
							dhcp_universe.code_hash,
							&i, 0, MDL);
				save_option (&dhcp_universe,
					     state -> options, oc);
			}
			option_cache_dereference (&oc, MDL);
		}

		get_server_source_address(&from, state->options,
					  state->options, packet);
		memcpy(state->from.iabuf, &from, sizeof(from));
		state->from.len = sizeof(from);

		offered_lease_time =
			state -> offered_expiry - cur_time;

		putULong(state->expiry, (u_int32_t)offered_lease_time);
		i = DHO_DHCP_LEASE_TIME;
		oc = (struct option_cache *)0;
		if (option_cache_allocate (&oc, MDL)) {
			if (make_const_data(&oc->expression, state->expiry,
					    4, 0, 0, MDL)) {
				option_code_hash_lookup(&oc->option,
							dhcp_universe.code_hash,
							&i, 0, MDL);
				save_option (&dhcp_universe,
					     state -> options, oc);
			}
			option_cache_dereference (&oc, MDL);
		}

		/*
		 * Validate any configured renew or rebinding times against
		 * the determined lease time.  Do rebinding first so that
		 * the renew time can be validated against the rebind time.
		 */
		if ((oc = lookup_option(&dhcp_universe, state->options,
					DHO_DHCP_REBINDING_TIME)) != NULL &&
		    evaluate_option_cache(&d1, packet, lease, NULL,
					  packet->options, state->options,
					  &lease->scope, oc, MDL)) {
			TIME rebind_time = getULong(d1.data);

			/* Drop the configured (invalid) rebinding time. */
			if (rebind_time >= offered_lease_time)
				delete_option(&dhcp_universe, state->options,
					      DHO_DHCP_REBINDING_TIME);
			else /* XXX: variable is reused. */
				offered_lease_time = rebind_time;

			data_string_forget(&d1, MDL);
		}

		if ((oc = lookup_option(&dhcp_universe, state->options,
					DHO_DHCP_RENEWAL_TIME)) != NULL &&
		    evaluate_option_cache(&d1, packet, lease, NULL,
					  packet->options, state->options,
					  &lease->scope, oc, MDL)) {
			if (getULong(d1.data) >= offered_lease_time)
				delete_option(&dhcp_universe, state->options,
					      DHO_DHCP_RENEWAL_TIME);

			data_string_forget(&d1, MDL);
		}
	} else {
		/* XXXSK: should we use get_server_source_address() here? */
		if (state -> ip -> address_count) {
			state -> from.len =
				sizeof state -> ip -> addresses [0];
			memcpy (state -> from.iabuf,
				&state -> ip -> addresses [0],
				state -> from.len);
		}
	}

	/* Figure out the address of the boot file server. */
	memset (&state -> siaddr, 0, sizeof state -> siaddr);
	if ((oc =
	     lookup_option (&server_universe,
			    state -> options, SV_NEXT_SERVER))) {
		if (evaluate_option_cache (&d1, packet, lease,
					   (struct client_state *)0,
					   packet -> options, state -> options,
					   &lease -> scope, oc, MDL)) {
			/* If there was more than one answer,
			   take the first. */
			if (d1.len >= 4 && d1.data)
				memcpy (&state -> siaddr, d1.data, 4);
			data_string_forget (&d1, MDL);
		}
	}

	/* Use the subnet mask from the subnet declaration if no other
	   mask has been provided. */
	i = DHO_SUBNET_MASK;
	if (!lookup_option (&dhcp_universe, state -> options, i)) {
		oc = (struct option_cache *)0;
		if (option_cache_allocate (&oc, MDL)) {
			if (make_const_data (&oc -> expression,
					     lease -> subnet -> netmask.iabuf,
					     lease -> subnet -> netmask.len,
					     0, 0, MDL)) {
				option_code_hash_lookup(&oc->option,
							dhcp_universe.code_hash,
							&i, 0, MDL);
				save_option (&dhcp_universe,
					     state -> options, oc);
			}
			option_cache_dereference (&oc, MDL);
		}
	}

	/* Use the hostname from the host declaration if there is one
	   and no hostname has otherwise been provided, and if the 
	   use-host-decl-name flag is set. */
	i = DHO_HOST_NAME;
	j = SV_USE_HOST_DECL_NAMES;
	if (!lookup_option (&dhcp_universe, state -> options, i) &&
	    lease -> host && lease -> host -> name &&
	    (evaluate_boolean_option_cache
	     (&ignorep, packet, lease, (struct client_state *)0,
	      packet -> options, state -> options, &lease -> scope,
	      lookup_option (&server_universe, state -> options, j), MDL))) {
		oc = (struct option_cache *)0;
		if (option_cache_allocate (&oc, MDL)) {
			if (make_const_data (&oc -> expression,
					     ((unsigned char *)
					      lease -> host -> name),
					     strlen (lease -> host -> name),
					     1, 0, MDL)) {
				option_code_hash_lookup(&oc->option,
							dhcp_universe.code_hash,
							&i, 0, MDL);
				save_option (&dhcp_universe,
					     state -> options, oc);
			}
			option_cache_dereference (&oc, MDL);
		}
	}

	/* If we don't have a hostname yet, and we've been asked to do
	   a reverse lookup to find the hostname, do it. */
	i = DHO_HOST_NAME;
	j = SV_GET_LEASE_HOSTNAMES;
	if (!lookup_option(&dhcp_universe, state->options, i) &&
	    evaluate_boolean_option_cache
	     (&ignorep, packet, lease, NULL,
	      packet->options, state->options, &lease->scope,
	      lookup_option (&server_universe, state->options, j), MDL)) {
		struct in_addr ia;
		struct hostent *h;
		
		memcpy (&ia, lease -> ip_addr.iabuf, 4);
		
		h = gethostbyaddr ((char *)&ia, sizeof ia, AF_INET);
		if (!h)
			log_error ("No hostname for %s", inet_ntoa (ia));
		else {
			oc = (struct option_cache *)0;
			if (option_cache_allocate (&oc, MDL)) {
				if (make_const_data (&oc -> expression,
						     ((unsigned char *)
						      h -> h_name),
						     strlen (h -> h_name) + 1,
						     1, 1, MDL)) {
					option_code_hash_lookup(&oc->option,
							dhcp_universe.code_hash,
								&i, 0, MDL);
					save_option (&dhcp_universe,
						     state -> options, oc);
				}
				option_cache_dereference (&oc, MDL);
			}
		}
	}

	/* If so directed, use the leased IP address as the router address.
	   This supposedly makes Win95 machines ARP for all IP addresses,
	   so if the local router does proxy arp, you win. */

	if (evaluate_boolean_option_cache
	    (&ignorep, packet, lease, (struct client_state *)0,
	     packet -> options, state -> options, &lease -> scope,
	     lookup_option (&server_universe, state -> options,
			    SV_USE_LEASE_ADDR_FOR_DEFAULT_ROUTE), MDL)) {
		i = DHO_ROUTERS;
		oc = lookup_option (&dhcp_universe, state -> options, i);
		if (!oc) {
			oc = (struct option_cache *)0;
			if (option_cache_allocate (&oc, MDL)) {
				if (make_const_data (&oc -> expression,
						     lease -> ip_addr.iabuf,
						     lease -> ip_addr.len,
						     0, 0, MDL)) {
					option_code_hash_lookup(&oc->option,
							dhcp_universe.code_hash,
								&i, 0, MDL);
					save_option (&dhcp_universe,
						     state -> options, oc);
				}
				option_cache_dereference (&oc, MDL);	
			}
		}
	}

	/* If a site option space has been specified, use that for
	   site option codes. */
	i = SV_SITE_OPTION_SPACE;
	if ((oc = lookup_option (&server_universe, state -> options, i)) &&
	    evaluate_option_cache (&d1, packet, lease,
				   (struct client_state *)0,
				   packet -> options, state -> options,
				   &lease -> scope, oc, MDL)) {
		struct universe *u = (struct universe *)0;

		if (!universe_hash_lookup (&u, universe_hash,
					   (const char *)d1.data, d1.len,
					   MDL)) {
			log_error ("unknown option space %s.", d1.data);
			return;
		}

		state -> options -> site_universe = u -> index;
		state->options->site_code_min = find_min_site_code(u);
		data_string_forget (&d1, MDL);
	} else {
		state -> options -> site_code_min = 0;
		state -> options -> site_universe = dhcp_universe.index;
	}

	/* If the client has provided a list of options that it wishes
	   returned, use it to prioritize.  If there's a parameter
	   request list in scope, use that in preference.  Otherwise
	   use the default priority list. */

	oc = lookup_option (&dhcp_universe, state -> options,
			    DHO_DHCP_PARAMETER_REQUEST_LIST);

	if (!oc)
		oc = lookup_option (&dhcp_universe, packet -> options,
				    DHO_DHCP_PARAMETER_REQUEST_LIST);
	if (oc)
		evaluate_option_cache (&state -> parameter_request_list,
				       packet, lease, (struct client_state *)0,
				       packet -> options, state -> options,
				       &lease -> scope, oc, MDL);

#ifdef DEBUG_PACKET
	dump_packet (packet);
	dump_raw ((unsigned char *)packet -> raw, packet -> packet_length);
#endif

	lease -> state = state;

	log_info ("%s", msg);

	/* Hang the packet off the lease state. */
	packet_reference (&lease -> state -> packet, packet, MDL);

	/* If this is a DHCPOFFER, ping the lease address before actually
	   sending the offer. */
	if (offer == DHCPOFFER && !(lease -> flags & STATIC_LEASE) &&
	    ((cur_time - lease_cltt) > 60) &&
	    (!(oc = lookup_option (&server_universe, state -> options,
				   SV_PING_CHECKS)) ||
	     evaluate_boolean_option_cache (&ignorep, packet, lease,
					    (struct client_state *)0,
					    packet -> options,
					    state -> options,
					    &lease -> scope, oc, MDL))) {
		icmp_echorequest (&lease -> ip_addr);

		/* Determine whether to use configured or default ping timeout.
		 */
		if ((oc = lookup_option (&server_universe, state -> options,
						SV_PING_TIMEOUT)) &&
		    evaluate_option_cache (&d1, packet, lease, NULL,
						packet -> options,
						state -> options,
						&lease -> scope, oc, MDL)) {
			if (d1.len == sizeof (u_int32_t))
				ping_timeout = getULong (d1.data);
			else
				ping_timeout = DEFAULT_PING_TIMEOUT;

			data_string_forget (&d1, MDL);
		} else
			ping_timeout = DEFAULT_PING_TIMEOUT;

#ifdef DEBUG
		log_debug ("Ping timeout: %ld", (long)ping_timeout);
#endif

		/*
		 * Set a timeout for 'ping-timeout' seconds from NOW, including
		 * current microseconds.  As ping-timeout defaults to 1, the
		 * exclusion of current microseconds causes a value somewhere
		 * /between/ zero and one.
		 */
		tv.tv_sec = cur_tv.tv_sec + ping_timeout;
		tv.tv_usec = cur_tv.tv_usec;
		add_timeout (&tv, lease_ping_timeout, lease,
			     (tvref_t)lease_reference,
			     (tvunref_t)lease_dereference);
		++outstanding_pings;
	} else {
  		lease->cltt = cur_time;
#if defined(DELAYED_ACK)
		if (enqueue)
			delayed_ack_enqueue(lease);
		else 
#endif
			dhcp_reply(lease);
	}
}

/*
 * CC: queue single ACK:
 * - write the lease (but do not fsync it yet)
 * - add to double linked list
 * - commit if more than xx ACKs pending
 * - if necessary set the max timer and bump the next timer
 *   but only up to the max timer value.
 */

void
delayed_ack_enqueue(struct lease *lease)
{
	struct leasequeue *q;

	if (!write_lease(lease)) 
		return;
	if (free_ackqueue) {
	   	q = free_ackqueue;
		free_ackqueue = q->next;
	} else {
		q = ((struct leasequeue *)
			     dmalloc(sizeof(struct leasequeue), MDL));
		if (!q)
			log_fatal("delayed_ack_enqueue: no memory!");
	}
	memset(q, 0, sizeof *q);
	/* prepend to ackqueue*/
	lease_reference(&q->lease, lease, MDL);
	q->next = ackqueue_head;
	ackqueue_head = q;
	if (!ackqueue_tail) 
		ackqueue_tail = q;
	else
		q->next->prev = q;

	outstanding_acks++;
	if (outstanding_acks > max_outstanding_acks) {
		commit_leases();

		/* Reset max_fsync and cancel any pending timeout. */
		memset(&max_fsync, 0, sizeof(max_fsync));
		cancel_timeout(commit_leases_ackout, NULL);
	} else {
		struct timeval next_fsync;

		if (max_fsync.tv_sec == 0 && max_fsync.tv_usec == 0) {
			/* set the maximum time we'll wait */
			max_fsync.tv_sec = cur_tv.tv_sec + max_ack_delay_secs;
			max_fsync.tv_usec = cur_tv.tv_usec +
				max_ack_delay_usecs;

			if (max_fsync.tv_usec >= 1000000) {
				max_fsync.tv_sec++;
				max_fsync.tv_usec -= 1000000;
			}
		}

		/* Set the timeout */
		next_fsync.tv_sec = cur_tv.tv_sec;
		next_fsync.tv_usec = cur_tv.tv_usec + min_ack_delay_usecs;
		if (next_fsync.tv_usec >= 1000000) {
			next_fsync.tv_sec++;
			next_fsync.tv_usec -= 1000000;
		}
		/* but not more than the max */
		if ((next_fsync.tv_sec > max_fsync.tv_sec) ||
		    ((next_fsync.tv_sec == max_fsync.tv_sec) &&
		     (next_fsync.tv_usec > max_fsync.tv_usec))) {
			next_fsync.tv_sec = max_fsync.tv_sec;
			next_fsync.tv_usec = max_fsync.tv_usec;
		}

		add_timeout(&next_fsync, commit_leases_ackout, NULL,
			    (tvref_t) NULL, (tvunref_t) NULL);
	}
}

static void
commit_leases_ackout(void *foo)
{
	if (outstanding_acks) {
		commit_leases();

		memset(&max_fsync, 0, sizeof(max_fsync));
	}
}

/* CC: process the delayed ACK responses:
   - send out the ACK packets
   - move the queue slots to the free list
 */
void
flush_ackqueue(void *foo) 
{
	struct leasequeue *ack, *p;
	/*  process from bottom to retain packet order */
	for (ack = ackqueue_tail ; ack ; ack = p) { 
		p = ack->prev;

		/* dhcp_reply() requires that the reply state still be valid */
		if (ack->lease->state == NULL)
			log_error("delayed ack for %s has gone stale",
				  piaddr(ack->lease->ip_addr));
		else
			dhcp_reply(ack->lease);

		lease_dereference(&ack->lease, MDL);
		ack->next = free_ackqueue;
		free_ackqueue = ack;
	}
	ackqueue_head = NULL;
	ackqueue_tail = NULL;
	outstanding_acks = 0;
}

#if defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
void
relinquish_ackqueue(void)
{
	struct leasequeue *q, *n;
	
	for (q = ackqueue_head ; q ; q = n) {
		n = q->next;
		dfree(q, MDL);
	}
	for (q = free_ackqueue ; q ; q = n) {
		n = q->next;
		dfree(q, MDL);
	}
}
#endif

void dhcp_reply (lease)
	struct lease *lease;
{
	int bufs = 0;
	unsigned packet_length;
	struct dhcp_packet raw;
	struct sockaddr_in to;
	struct in_addr from;
	struct hardware hto;
	int result;
	struct lease_state *state = lease -> state;
	int nulltp, bootpp, unicastp = 1;
	struct data_string d1;
	const char *s;

	if (!state)
		log_fatal ("dhcp_reply was supplied lease with no state!");

	/* Compose a response for the client... */
	memset (&raw, 0, sizeof raw);
	memset (&d1, 0, sizeof d1);

	/* Copy in the filename if given; otherwise, flag the filename
	   buffer as available for options. */
	if (state -> filename.len && state -> filename.data) {
		memcpy (raw.file,
			state -> filename.data,
			state -> filename.len > sizeof raw.file
			? sizeof raw.file : state -> filename.len);
		if (sizeof raw.file > state -> filename.len)
			memset (&raw.file [state -> filename.len], 0,
				(sizeof raw.file) - state -> filename.len);
		else 
			log_info("file name longer than packet field "
				 "truncated - field: %lu name: %d %.*s", 
				 (unsigned long)sizeof(raw.file),
				 state->filename.len, (int)state->filename.len,
				 state->filename.data);
	} else
		bufs |= 1;

	/* Copy in the server name if given; otherwise, flag the
	   server_name buffer as available for options. */
	if (state -> server_name.len && state -> server_name.data) {
		memcpy (raw.sname,
			state -> server_name.data,
			state -> server_name.len > sizeof raw.sname
			? sizeof raw.sname : state -> server_name.len);
		if (sizeof raw.sname > state -> server_name.len)
			memset (&raw.sname [state -> server_name.len], 0,
				(sizeof raw.sname) - state -> server_name.len);
		else 
			log_info("server name longer than packet field "
				 "truncated - field: %lu name: %d %.*s", 
				 (unsigned long)sizeof(raw.sname),
				 state->server_name.len,
				 (int)state->server_name.len,
				 state->server_name.data);
	} else
		bufs |= 2; /* XXX */

	memcpy (raw.chaddr,
		&lease -> hardware_addr.hbuf [1], sizeof raw.chaddr);
	raw.hlen = lease -> hardware_addr.hlen - 1;
	raw.htype = lease -> hardware_addr.hbuf [0];

	/* See if this is a Microsoft client that NUL-terminates its
	   strings and expects us to do likewise... */
	if (lease -> flags & MS_NULL_TERMINATION)
		nulltp = 1;
	else
		nulltp = 0;

	/* See if this is a bootp client... */
	if (state -> offer)
		bootpp = 0;
	else
		bootpp = 1;

	/* Insert such options as will fit into the buffer. */
	packet_length = cons_options (state -> packet, &raw, lease,
				      (struct client_state *)0,
				      state -> max_message_size,
				      state -> packet -> options,
				      state -> options, &global_scope,
				      bufs, nulltp, bootpp,
				      &state -> parameter_request_list,
				      (char *)0);

	memcpy (&raw.ciaddr, &state -> ciaddr, sizeof raw.ciaddr);
	memcpy (&raw.yiaddr, lease -> ip_addr.iabuf, 4);
	raw.siaddr = state -> siaddr;
	raw.giaddr = state -> giaddr;

	raw.xid = state -> xid;
	raw.secs = state -> secs;
	raw.flags = state -> bootp_flags;
	raw.hops = state -> hops;
	raw.op = BOOTREPLY;

	if (lease -> client_hostname) {
		if ((strlen (lease -> client_hostname) <= 64) &&
		    db_printable((unsigned char *)lease->client_hostname))
			s = lease -> client_hostname;
		else
			s = "Hostname Unsuitable for Printing";
	} else
		s = (char *)0;

	/* Say what we're doing... */
	log_info ("%s on %s to %s %s%s%svia %s",
		  (state -> offer
		   ? (state -> offer == DHCPACK ? "DHCPACK" : "DHCPOFFER")
		   : "BOOTREPLY"),
		  piaddr (lease -> ip_addr),
		  (lease -> hardware_addr.hlen
		   ? print_hw_addr (lease -> hardware_addr.hbuf [0],
				    lease -> hardware_addr.hlen - 1,
				    &lease -> hardware_addr.hbuf [1])
		   : print_hex_1(lease->uid_len, lease->uid, 60)),
		  s ? "(" : "", s ? s : "", s ? ") " : "",
		  (state -> giaddr.s_addr
		   ? inet_ntoa (state -> giaddr)
		   : state -> ip -> name));

	/* Set up the hardware address... */
	hto.hlen = lease -> hardware_addr.hlen;
	memcpy (hto.hbuf, lease -> hardware_addr.hbuf, hto.hlen);

	to.sin_family = AF_INET;
#ifdef HAVE_SA_LEN
	to.sin_len = sizeof to;
#endif
	memset (to.sin_zero, 0, sizeof to.sin_zero);

#ifdef DEBUG_PACKET
	dump_raw ((unsigned char *)&raw, packet_length);
#endif

	/* Make sure outgoing packets are at least as big
	   as a BOOTP packet. */
	if (packet_length < BOOTP_MIN_LEN)
		packet_length = BOOTP_MIN_LEN;

	/* If this was gatewayed, send it back to the gateway... */
	if (raw.giaddr.s_addr) {
		to.sin_addr = raw.giaddr;
		if (raw.giaddr.s_addr != htonl (INADDR_LOOPBACK))
			to.sin_port = local_port;
		else
			to.sin_port = remote_port; /* For debugging. */

		if (fallback_interface) {
			result = send_packet(fallback_interface, NULL, &raw,
					     packet_length, raw.siaddr, &to,
					     NULL);
			if (result < 0) {
				log_error ("%s:%d: Failed to send %d byte long "
					   "packet over %s interface.", MDL,
					   packet_length,
					   fallback_interface->name);
			}


			free_lease_state (state, MDL);
			lease -> state = (struct lease_state *)0;
			return;
		}

	/* If the client is RENEWING, unicast to the client using the
	   regular IP stack.  Some clients, particularly those that
	   follow RFC1541, are buggy, and send both ciaddr and server
	   identifier.  We deal with this situation by assuming that
	   if we got both dhcp-server-identifier and ciaddr, and
	   giaddr was not set, then the client is on the local
	   network, and we can therefore unicast or broadcast to it
	   successfully.  A client in REQUESTING state on another
	   network that's making this mistake will have set giaddr,
	   and will therefore get a relayed response from the above
	   code. */
	} else if (raw.ciaddr.s_addr &&
		   !((state -> got_server_identifier ||
		      (raw.flags & htons (BOOTP_BROADCAST))) &&
		     /* XXX This won't work if giaddr isn't zero, but it is: */
		     (state -> shared_network ==
		      lease -> subnet -> shared_network)) &&
		   state -> offer == DHCPACK) {
		to.sin_addr = raw.ciaddr;
		to.sin_port = remote_port;

		if (fallback_interface) {
			result = send_packet(fallback_interface, NULL, &raw,
					     packet_length, raw.siaddr, &to,
					     NULL);
			if (result < 0) {
				log_error("%s:%d: Failed to send %d byte long"
					  " packet over %s interface.", MDL,
					   packet_length,
					   fallback_interface->name);
			}

			free_lease_state (state, MDL);
			lease -> state = (struct lease_state *)0;
			return;
		}

	/* If it comes from a client that already knows its address
	   and is not requesting a broadcast response, and we can
	   unicast to a client without using the ARP protocol, sent it
	   directly to that client. */
	} else if (!(raw.flags & htons (BOOTP_BROADCAST)) &&
		   can_unicast_without_arp (state -> ip)) {
		to.sin_addr = raw.yiaddr;
		to.sin_port = remote_port;

	/* Otherwise, broadcast it on the local network. */
	} else {
		to.sin_addr = limited_broadcast;
		to.sin_port = remote_port;
		if (!(lease -> flags & UNICAST_BROADCAST_HACK))
			unicastp = 0;
	}

	memcpy (&from, state -> from.iabuf, sizeof from);

	result = send_packet(state->ip, NULL, &raw, packet_length,
			      from, &to, unicastp ? &hto : NULL);
	if (result < 0) {
	    log_error ("%s:%d: Failed to send %d byte long "
		       "packet over %s interface.", MDL,
		       packet_length, state->ip->name);
	}


	/* Free all of the entries in the option_state structure
	   now that we're done with them. */

	free_lease_state (state, MDL);
	lease -> state = (struct lease_state *)0;
}

int find_lease (struct lease **lp,
		struct packet *packet, struct shared_network *share, int *ours,
		int *peer_has_leases, struct lease *ip_lease_in,
		const char *file, int line)
{
	struct lease *uid_lease = (struct lease *)0;
	struct lease *ip_lease = (struct lease *)0;
	struct lease *hw_lease = (struct lease *)0;
	struct lease *lease = (struct lease *)0;
	struct iaddr cip;
	struct host_decl *hp = (struct host_decl *)0;
	struct host_decl *host = (struct host_decl *)0;
	struct lease *fixed_lease = (struct lease *)0;
	struct lease *next = (struct lease *)0;
	struct option_cache *oc;
	struct data_string d1;
	int have_client_identifier = 0;
	struct data_string client_identifier;
	struct hardware h;

#if defined(FAILOVER_PROTOCOL)
	/* Quick check to see if the peer has leases. */
	if (peer_has_leases) {
		struct pool *pool;

		for (pool = share->pools ; pool ; pool = pool->next) {
			dhcp_failover_state_t *peer = pool->failover_peer;

			if (peer &&
			    ((peer->i_am == primary && pool->backup_leases) ||
			     (peer->i_am == secondary && pool->free_leases))) {
				*peer_has_leases = 1;
				break;
			}
		}
	}
#endif /* FAILOVER_PROTOCOL */

	if (packet -> raw -> ciaddr.s_addr) {
		cip.len = 4;
		memcpy (cip.iabuf, &packet -> raw -> ciaddr, 4);
	} else {
		/* Look up the requested address. */
		oc = lookup_option (&dhcp_universe, packet -> options,
				    DHO_DHCP_REQUESTED_ADDRESS);
		memset (&d1, 0, sizeof d1);
		if (oc &&
		    evaluate_option_cache (&d1, packet, (struct lease *)0,
					   (struct client_state *)0,
					   packet -> options,
					   (struct option_state *)0,
					   &global_scope, oc, MDL)) {
			packet -> got_requested_address = 1;
			cip.len = 4;
			memcpy (cip.iabuf, d1.data, cip.len);
			data_string_forget (&d1, MDL);
		} else 
			cip.len = 0;
	}

	/* Try to find a host or lease that's been assigned to the
	   specified unique client identifier. */
	oc = lookup_option (&dhcp_universe, packet -> options,
			    DHO_DHCP_CLIENT_IDENTIFIER);
	memset (&client_identifier, 0, sizeof client_identifier);
	if (oc &&
	    evaluate_option_cache (&client_identifier,
				   packet, (struct lease *)0,
				   (struct client_state *)0,
				   packet -> options, (struct option_state *)0,
				   &global_scope, oc, MDL)) {
		/* Remember this for later. */
		have_client_identifier = 1;

		/* First, try to find a fixed host entry for the specified
		   client identifier... */
		if (find_hosts_by_uid (&hp, client_identifier.data,
				       client_identifier.len, MDL)) {
			/* Remember if we know of this client. */
			packet -> known = 1;
			mockup_lease (&fixed_lease, packet, share, hp);
		}

#if defined (DEBUG_FIND_LEASE)
		if (fixed_lease) {
			log_info ("Found host for client identifier: %s.",
			      piaddr (fixed_lease -> ip_addr));
		}
#endif
		if (hp) {
			if (!fixed_lease) /* Save the host if we found one. */
				host_reference (&host, hp, MDL);
			host_dereference (&hp, MDL);
		}

		find_lease_by_uid (&uid_lease, client_identifier.data,
				   client_identifier.len, MDL);
	}

	/* If we didn't find a fixed lease using the uid, try doing
	   it with the hardware address... */
	if (!fixed_lease && !host) {
		if (find_hosts_by_haddr (&hp, packet -> raw -> htype,
					 packet -> raw -> chaddr,
					 packet -> raw -> hlen, MDL)) {
			/* Remember if we know of this client. */
			packet -> known = 1;
			if (host)
				host_dereference (&host, MDL);
			host_reference (&host, hp, MDL);
			host_dereference (&hp, MDL);
			mockup_lease (&fixed_lease, packet, share, host);
#if defined (DEBUG_FIND_LEASE)
			if (fixed_lease) {
				log_info ("Found host for link address: %s.",
				      piaddr (fixed_lease -> ip_addr));
			}
#endif
		}
	}

	/* Finally, if we haven't found anything yet try again with the
	 * host-identifier option ... */
	if (!fixed_lease && !host) {
		if (find_hosts_by_option(&hp, packet,
					 packet->options, MDL) == 1) {
			packet->known = 1;
			if (host)
				host_dereference(&host, MDL);
			host_reference(&host, hp, MDL);
			host_dereference(&hp, MDL);
			mockup_lease (&fixed_lease, packet, share, host);
#if defined (DEBUG_FIND_LEASE)
			if (fixed_lease) {
				log_info ("Found host via host-identifier");
			}
#endif
		}
	}

	/* If fixed_lease is present but does not match the requested
	   IP address, and this is a DHCPREQUEST, then we can't return
	   any other lease, so we might as well return now. */
	if (packet -> packet_type == DHCPREQUEST && fixed_lease &&
	    (fixed_lease -> ip_addr.len != cip.len ||
	     memcmp (fixed_lease -> ip_addr.iabuf,
		     cip.iabuf, cip.len))) {
		if (ours)
			*ours = 1;
		strcpy (dhcp_message, "requested address is incorrect");
#if defined (DEBUG_FIND_LEASE)
		log_info ("Client's fixed-address %s doesn't match %s%s",
			  piaddr (fixed_lease -> ip_addr), "request ",
			  print_dotted_quads (cip.len, cip.iabuf));
#endif
		goto out;
	}

	/*
	 * If we found leases matching the client identifier, loop through
	 * the n_uid pointer looking for one that's actually valid.   We
	 * can't do this until we get here because we depend on
	 * packet -> known, which may be set by either the uid host
	 * lookup or the haddr host lookup.
	 *
	 * Note that the n_uid lease chain is sorted in order of
	 * preference, so the first one is the best one.
	 */
	while (uid_lease) {
#if defined (DEBUG_FIND_LEASE)
		log_info ("trying next lease matching client id: %s",
			  piaddr (uid_lease -> ip_addr));
#endif

#if defined (FAILOVER_PROTOCOL)
		/*
		 * When we lookup a lease by uid, we know the client identifier
		 * matches the lease's record.  If it is active, or was last
		 * active with the same client, we can trivially extend it.
		 * If is not or was not active, we can allocate it to this
		 * client if it matches the usual free/backup criteria (which
		 * is contained in lease_mine_to_reallocate()).
		 */
		if (uid_lease->binding_state != FTS_ACTIVE &&
		    uid_lease->rewind_binding_state != FTS_ACTIVE &&
		    !lease_mine_to_reallocate(uid_lease)) {
#if defined (DEBUG_FIND_LEASE)
			log_info("not active or not mine to allocate: %s",
				 piaddr(uid_lease->ip_addr));
#endif
			goto n_uid;
		}
#endif

		if (uid_lease -> subnet -> shared_network != share) {
#if defined (DEBUG_FIND_LEASE)
			log_info ("wrong network segment: %s",
				  piaddr (uid_lease -> ip_addr));
#endif
			goto n_uid;
		}

		if ((uid_lease -> pool -> prohibit_list &&
		     permitted (packet, uid_lease -> pool -> prohibit_list)) ||
		    (uid_lease -> pool -> permit_list &&
		     !permitted (packet, uid_lease -> pool -> permit_list))) {
#if defined (DEBUG_FIND_LEASE)
			log_info ("not permitted: %s",
				  piaddr (uid_lease -> ip_addr));
#endif
		       n_uid:
			if (uid_lease -> n_uid)
				lease_reference (&next,
						 uid_lease -> n_uid, MDL);
			if (!packet -> raw -> ciaddr.s_addr)
				release_lease (uid_lease, packet);
			lease_dereference (&uid_lease, MDL);
			if (next) {
				lease_reference (&uid_lease, next, MDL);
				lease_dereference (&next, MDL);
			}
			continue;
		}
		break;
	}
#if defined (DEBUG_FIND_LEASE)
	if (uid_lease)
		log_info ("Found lease for client id: %s.",
		      piaddr (uid_lease -> ip_addr));
#endif

	/* Find a lease whose hardware address matches, whose client
	 * identifier matches (or equally doesn't have one), that's
	 * permitted, and that's on the correct subnet.
	 *
	 * Note that the n_hw chain is sorted in order of preference, so
	 * the first one found is the best one.
	 */
	h.hlen = packet -> raw -> hlen + 1;
	h.hbuf [0] = packet -> raw -> htype;
	memcpy (&h.hbuf [1], packet -> raw -> chaddr, packet -> raw -> hlen);
	find_lease_by_hw_addr (&hw_lease, h.hbuf, h.hlen, MDL);
	while (hw_lease) {
#if defined (DEBUG_FIND_LEASE)
		log_info ("trying next lease matching hw addr: %s",
			  piaddr (hw_lease -> ip_addr));
#endif
#if defined (FAILOVER_PROTOCOL)
		/*
		 * When we lookup a lease by chaddr, we know the MAC address
		 * matches the lease record (we will check if the lease has a
		 * client-id the client does not next).  If the lease is
		 * currently active or was last active with this client, we can
		 * trivially extend it.  Otherwise, there are a set of rules
		 * that govern if we can reallocate this lease to any client
		 * ("lease_mine_to_reallocate()") including this one.
		 */
		if (hw_lease->binding_state != FTS_ACTIVE &&
		    hw_lease->rewind_binding_state != FTS_ACTIVE &&
		    !lease_mine_to_reallocate(hw_lease)) {
#if defined (DEBUG_FIND_LEASE)
			log_info("not active or not mine to allocate: %s",
				 piaddr(hw_lease->ip_addr));
#endif
			goto n_hw;
		}
#endif

		/*
		 * This conditional skips "potentially active" leases (leases
		 * we think are expired may be extended by the peer, etc) that
		 * may be assigned to a differently /client-identified/ client
		 * with the same MAC address.
		 */
		if (hw_lease -> binding_state != FTS_FREE &&
		    hw_lease -> binding_state != FTS_BACKUP &&
		    hw_lease -> uid &&
		    (!have_client_identifier ||
		     hw_lease -> uid_len != client_identifier.len ||
		     memcmp (hw_lease -> uid, client_identifier.data,
			     hw_lease -> uid_len))) {
#if defined (DEBUG_FIND_LEASE)
			log_info ("wrong client identifier: %s",
				  piaddr (hw_lease -> ip_addr));
#endif
			goto n_hw;
		}
		if (hw_lease -> subnet -> shared_network != share) {
#if defined (DEBUG_FIND_LEASE)
			log_info ("wrong network segment: %s",
				  piaddr (hw_lease -> ip_addr));
#endif
			goto n_hw;
		}
		if ((hw_lease -> pool -> prohibit_list &&
		      permitted (packet, hw_lease -> pool -> prohibit_list)) ||
		    (hw_lease -> pool -> permit_list &&
		     !permitted (packet, hw_lease -> pool -> permit_list))) {
#if defined (DEBUG_FIND_LEASE)
			log_info ("not permitted: %s",
				  piaddr (hw_lease -> ip_addr));
#endif
			if (!packet -> raw -> ciaddr.s_addr)
				release_lease (hw_lease, packet);
		       n_hw:
			if (hw_lease -> n_hw)
				lease_reference (&next, hw_lease -> n_hw, MDL);
			lease_dereference (&hw_lease, MDL);
			if (next) {
				lease_reference (&hw_lease, next, MDL);
				lease_dereference (&next, MDL);
			}
			continue;
		}
		break;
	}
#if defined (DEBUG_FIND_LEASE)
	if (hw_lease)
		log_info ("Found lease for hardware address: %s.",
		      piaddr (hw_lease -> ip_addr));
#endif

	/* Try to find a lease that's been allocated to the client's
	   IP address. */
	if (ip_lease_in)
		lease_reference (&ip_lease, ip_lease_in, MDL);
	else if (cip.len)
		find_lease_by_ip_addr (&ip_lease, cip, MDL);

#if defined (DEBUG_FIND_LEASE)
	if (ip_lease)
		log_info ("Found lease for requested address: %s.",
		      piaddr (ip_lease -> ip_addr));
#endif

	/* If ip_lease is valid at this point, set ours to one, so that
	   even if we choose a different lease, we know that the address
	   the client was requesting was ours, and thus we can NAK it. */
	if (ip_lease && ours)
		*ours = 1;

	/* If the requested IP address isn't on the network the packet
	   came from, don't use it.  Allow abandoned leases to be matched
	   here - if the client is requesting it, there's a decent chance
	   that it's because the lease database got trashed and a client
	   that thought it had this lease answered an ARP or PING, causing the
	   lease to be abandoned.   If so, this request probably came from
	   that client. */
	if (ip_lease && (ip_lease -> subnet -> shared_network != share)) {
		if (ours)
			*ours = 1;
#if defined (DEBUG_FIND_LEASE)
		log_info ("...but it was on the wrong shared network.");
#endif
		strcpy (dhcp_message, "requested address on bad subnet");
		lease_dereference (&ip_lease, MDL);
	}

	/*
	 * If the requested address is in use (or potentially in use) by
	 * a different client, it can't be granted.
	 *
	 * This first conditional only detects if the lease is currently
	 * identified to a different client (client-id and/or chaddr
	 * mismatch).  In this case we may not want to give the client the
	 * lease, if doing so may potentially be an addressing conflict.
	 */
	if (ip_lease &&
	    (ip_lease -> uid ?
	     (!have_client_identifier ||
	      ip_lease -> uid_len != client_identifier.len ||
	      memcmp (ip_lease -> uid, client_identifier.data,
		      ip_lease -> uid_len)) :
	     (ip_lease -> hardware_addr.hbuf [0] != packet -> raw -> htype ||
	      ip_lease -> hardware_addr.hlen != packet -> raw -> hlen + 1 ||
	      memcmp (&ip_lease -> hardware_addr.hbuf [1],
		      packet -> raw -> chaddr,
		      (unsigned)(ip_lease -> hardware_addr.hlen - 1))))) {
		/*
		 * A lease is unavailable for allocation to a new client if
		 * it is not in the FREE or BACKUP state.  There may be
		 * leases that are in the expired state with a rewinding
		 * state that is free or backup, but these will be processed
		 * into the free or backup states by expiration processes, so
		 * checking for them here is superfluous.
		 */
		if (ip_lease -> binding_state != FTS_FREE &&
		    ip_lease -> binding_state != FTS_BACKUP) {
#if defined (DEBUG_FIND_LEASE)
			log_info ("rejecting lease for requested address.");
#endif
			/* If we're rejecting it because the peer has
			   it, don't set "ours", because we shouldn't NAK. */
			if (ours && ip_lease -> binding_state != FTS_ACTIVE)
				*ours = 0;
			lease_dereference (&ip_lease, MDL);
		}
	}

	/*
	 * If we got an ip_lease and a uid_lease or hw_lease, and ip_lease
	 * is/was not active, and is not ours to reallocate, forget about it.
	 */
	if (ip_lease && (uid_lease || hw_lease) &&
	    ip_lease->binding_state != FTS_ACTIVE &&
	    ip_lease->rewind_binding_state != FTS_ACTIVE &&
#if defined(FAILOVER_PROTOCOL)
	    !lease_mine_to_reallocate(ip_lease) &&
#endif
	    packet->packet_type == DHCPDISCOVER) {
#if defined (DEBUG_FIND_LEASE)
		log_info("ip lease not active or not ours to offer.");
#endif
		lease_dereference(&ip_lease, MDL);
	}

	/* If for some reason the client has more than one lease
	   on the subnet that matches its uid, pick the one that
	   it asked for and (if we can) free the other. */
	if (ip_lease && ip_lease->binding_state == FTS_ACTIVE &&
	    ip_lease->uid && ip_lease != uid_lease) {
		if (have_client_identifier &&
		    (ip_lease -> uid_len == client_identifier.len) &&
		    !memcmp (client_identifier.data,
			     ip_lease -> uid, ip_lease -> uid_len)) {
			if (uid_lease) {
			    if (uid_lease->binding_state == FTS_ACTIVE) {
				log_error ("client %s has duplicate%s on %s",
					   (print_hw_addr
					    (packet -> raw -> htype,
					     packet -> raw -> hlen,
					     packet -> raw -> chaddr)),
					   " leases",
					   (ip_lease -> subnet ->
					    shared_network -> name));

				/* If the client is REQUESTing the lease,
				   it shouldn't still be using the old
				   one, so we can free it for allocation. */
				if (uid_lease &&
				    uid_lease->binding_state == FTS_ACTIVE &&
				    !packet -> raw -> ciaddr.s_addr &&
				    (share ==
				     uid_lease -> subnet -> shared_network) &&
				    packet -> packet_type == DHCPREQUEST)
					release_lease (uid_lease, packet);
			    }
			    lease_dereference (&uid_lease, MDL);
			    lease_reference (&uid_lease, ip_lease, MDL);
			}
		}

		/* If we get to here and fixed_lease is not null, that means
		   that there are both a dynamic lease and a fixed-address
		   declaration for the same IP address. */
		if (packet -> packet_type == DHCPREQUEST && fixed_lease) {
			lease_dereference (&fixed_lease, MDL);
		      db_conflict:
			log_error ("Dynamic and static leases present for %s.",
				   piaddr (cip));
			log_error ("Remove host declaration %s or remove %s",
				   (fixed_lease && fixed_lease -> host
				    ? (fixed_lease -> host -> name
				       ? fixed_lease -> host -> name
				       : piaddr (cip))
				    : piaddr (cip)),
				    piaddr (cip));
			log_error ("from the dynamic address pool for %s",
				   ip_lease -> subnet -> shared_network -> name
				  );
			if (fixed_lease)
				lease_dereference (&ip_lease, MDL);
			strcpy (dhcp_message,
				"database conflict - call for help!");
		}

		if (ip_lease && ip_lease != uid_lease) {
#if defined (DEBUG_FIND_LEASE)
			log_info ("requested address not available.");
#endif
			lease_dereference (&ip_lease, MDL);
		}
	}

	/* If we get to here with both fixed_lease and ip_lease not
	   null, then we have a configuration file bug. */
	if (packet -> packet_type == DHCPREQUEST && fixed_lease && ip_lease)
		goto db_conflict;

	/* Toss extra pointers to the same lease... */
	if (hw_lease && hw_lease == uid_lease) {
#if defined (DEBUG_FIND_LEASE)
		log_info ("hardware lease and uid lease are identical.");
#endif
		lease_dereference (&hw_lease, MDL);
	}
	if (ip_lease && ip_lease == hw_lease) {
		lease_dereference (&hw_lease, MDL);
#if defined (DEBUG_FIND_LEASE)
		log_info ("hardware lease and ip lease are identical.");
#endif
	}
	if (ip_lease && ip_lease == uid_lease) {
		lease_dereference (&uid_lease, MDL);
#if defined (DEBUG_FIND_LEASE)
		log_info ("uid lease and ip lease are identical.");
#endif
	}

	/* Make sure the client is permitted to use the requested lease. */
	if (ip_lease &&
	    ((ip_lease -> pool -> prohibit_list &&
	      permitted (packet, ip_lease -> pool -> prohibit_list)) ||
	     (ip_lease -> pool -> permit_list &&
	      !permitted (packet, ip_lease -> pool -> permit_list)))) {
		if (!packet->raw->ciaddr.s_addr &&
		    (ip_lease->binding_state == FTS_ACTIVE))
			release_lease (ip_lease, packet);

		lease_dereference (&ip_lease, MDL);
	}

	if (uid_lease &&
	    ((uid_lease -> pool -> prohibit_list &&
	      permitted (packet, uid_lease -> pool -> prohibit_list)) ||
	     (uid_lease -> pool -> permit_list &&
	      !permitted (packet, uid_lease -> pool -> permit_list)))) {
		if (!packet -> raw -> ciaddr.s_addr)
			release_lease (uid_lease, packet);
		lease_dereference (&uid_lease, MDL);
	}

	if (hw_lease &&
	    ((hw_lease -> pool -> prohibit_list &&
	      permitted (packet, hw_lease -> pool -> prohibit_list)) ||
	     (hw_lease -> pool -> permit_list &&
	      !permitted (packet, hw_lease -> pool -> permit_list)))) {
		if (!packet -> raw -> ciaddr.s_addr)
			release_lease (hw_lease, packet);
		lease_dereference (&hw_lease, MDL);
	}

	/* If we've already eliminated the lease, it wasn't there to
	   begin with.   If we have come up with a matching lease,
	   set the message to bad network in case we have to throw it out. */
	if (!ip_lease) {
		strcpy (dhcp_message, "requested address not available");
	}

	/* If this is a DHCPREQUEST, make sure the lease we're going to return
	   matches the requested IP address.   If it doesn't, don't return a
	   lease at all. */
	if (packet -> packet_type == DHCPREQUEST &&
	    !ip_lease && !fixed_lease) {
#if defined (DEBUG_FIND_LEASE)
		log_info ("no applicable lease found for DHCPREQUEST.");
#endif
		goto out;
	}

	/* At this point, if fixed_lease is nonzero, we can assign it to
	   this client. */
	if (fixed_lease) {
		lease_reference (&lease, fixed_lease, MDL);
		lease_dereference (&fixed_lease, MDL);
#if defined (DEBUG_FIND_LEASE)
		log_info ("choosing fixed address.");
#endif
	}

	/* If we got a lease that matched the ip address and don't have
	   a better offer, use that; otherwise, release it. */
	if (ip_lease) {
		if (lease) {
			if (!packet -> raw -> ciaddr.s_addr)
				release_lease (ip_lease, packet);
#if defined (DEBUG_FIND_LEASE)
			log_info ("not choosing requested address (!).");
#endif
		} else {
#if defined (DEBUG_FIND_LEASE)
			log_info ("choosing lease on requested address.");
#endif
			lease_reference (&lease, ip_lease, MDL);
			if (lease -> host)
				host_dereference (&lease -> host, MDL);
		}
		lease_dereference (&ip_lease, MDL);
	}

	/* If we got a lease that matched the client identifier, we may want
	   to use it, but if we already have a lease we like, we must free
	   the lease that matched the client identifier. */
	if (uid_lease) {
		if (lease) {
			log_error("uid lease %s for client %s is duplicate "
				  "on %s",
				  piaddr(uid_lease->ip_addr),
				  print_hw_addr(packet->raw->htype,
						packet->raw->hlen,
						packet->raw->chaddr),
				  uid_lease->subnet->shared_network->name);

			if (!packet -> raw -> ciaddr.s_addr &&
			    packet -> packet_type == DHCPREQUEST &&
			    uid_lease -> binding_state == FTS_ACTIVE)
				release_lease(uid_lease, packet);
#if defined (DEBUG_FIND_LEASE)
			log_info ("not choosing uid lease.");
#endif
		} else {
			lease_reference (&lease, uid_lease, MDL);
			if (lease -> host)
				host_dereference (&lease -> host, MDL);
#if defined (DEBUG_FIND_LEASE)
			log_info ("choosing uid lease.");
#endif
		}
		lease_dereference (&uid_lease, MDL);
	}

	/* The lease that matched the hardware address is treated likewise. */
	if (hw_lease) {
		if (lease) {
#if defined (DEBUG_FIND_LEASE)
			log_info ("not choosing hardware lease.");
#endif
		} else {
			/* We're a little lax here - if the client didn't
			   send a client identifier and it's a bootp client,
			   but the lease has a client identifier, we still
			   let the client have a lease. */
			if (!hw_lease -> uid_len ||
			    (have_client_identifier
			     ? (hw_lease -> uid_len ==
				client_identifier.len &&
				!memcmp (hw_lease -> uid,
					 client_identifier.data,
					 client_identifier.len))
			     : packet -> packet_type == 0)) {
				lease_reference (&lease, hw_lease, MDL);
				if (lease -> host)
					host_dereference (&lease -> host, MDL);
#if defined (DEBUG_FIND_LEASE)
				log_info ("choosing hardware lease.");
#endif
			} else {
#if defined (DEBUG_FIND_LEASE)
				log_info ("not choosing hardware lease: %s.",
					  "uid mismatch");
#endif
			}
		}
		lease_dereference (&hw_lease, MDL);
	}

	/*
	 * If we found a host_decl but no matching address, try to
	 * find a host_decl that has no address, and if there is one,
	 * hang it off the lease so that we can use the supplied
	 * options.
	 */
	if (lease && host && !lease->host) {
		struct host_decl *p = NULL;
		struct host_decl *n = NULL;

		host_reference(&p, host, MDL);
		while (p != NULL) {
			if (!p->fixed_addr) {
				/*
				 * If the lease is currently active, then it
				 * must be allocated to the present client.
				 * We store a reference to the host record on
				 * the lease to save a lookup later (in
				 * ack_lease()).  We mustn't refer to the host
				 * record on non-active leases because the
				 * client may be denied later.
				 *
				 * XXX: Not having this reference (such as in
				 * DHCPDISCOVER/INIT) means ack_lease will have
				 * to perform this lookup a second time.  This
				 * hopefully isn't a problem as DHCPREQUEST is
				 * more common than DHCPDISCOVER.
				 */
				if (lease->binding_state == FTS_ACTIVE)
					host_reference(&lease->host, p, MDL);

				host_dereference(&p, MDL);
				break;
			}
			if (p->n_ipaddr != NULL)
				host_reference(&n, p->n_ipaddr, MDL);
			host_dereference(&p, MDL);
			if (n != NULL) {
				host_reference(&p, n, MDL);
				host_dereference(&n, MDL);
			}
		}
	}

	/* If we find an abandoned lease, but it's the one the client
	   requested, we assume that previous bugginess on the part
	   of the client, or a server database loss, caused the lease to
	   be abandoned, so we reclaim it and let the client have it. */
	if (lease &&
	    (lease -> binding_state == FTS_ABANDONED) &&
	    lease == ip_lease &&
	    packet -> packet_type == DHCPREQUEST) {
		log_error ("Reclaiming REQUESTed abandoned IP address %s.",
		      piaddr (lease -> ip_addr));
	} else if (lease && (lease -> binding_state == FTS_ABANDONED)) {
	/* Otherwise, if it's not the one the client requested, we do not
	   return it - instead, we claim it's ours, causing a DHCPNAK to be
	   sent if this lookup is for a DHCPREQUEST, and force the client
	   to go back through the allocation process. */
		if (ours)
			*ours = 1;
		lease_dereference (&lease, MDL);
	}

      out:
	if (have_client_identifier)
		data_string_forget (&client_identifier, MDL);

	if (fixed_lease)
		lease_dereference (&fixed_lease, MDL);
	if (hw_lease)
		lease_dereference (&hw_lease, MDL);
	if (uid_lease)
		lease_dereference (&uid_lease, MDL);
	if (ip_lease)
		lease_dereference (&ip_lease, MDL);
	if (host)
		host_dereference (&host, MDL);

	if (lease) {
#if defined (DEBUG_FIND_LEASE)
		log_info ("Returning lease: %s.",
		      piaddr (lease -> ip_addr));
#endif
		lease_reference (lp, lease, file, line);
		lease_dereference (&lease, MDL);
		return 1;
	}
#if defined (DEBUG_FIND_LEASE)
	log_info ("Not returning a lease.");
#endif
	return 0;
}

/* Search the provided host_decl structure list for an address that's on
   the specified shared network.  If one is found, mock up and return a
   lease structure for it; otherwise return the null pointer. */

int mockup_lease (struct lease **lp, struct packet *packet,
		  struct shared_network *share, struct host_decl *hp)
{
	struct lease *lease = (struct lease *)0;
	struct host_decl *rhp = (struct host_decl *)0;
	
	if (lease_allocate (&lease, MDL) != ISC_R_SUCCESS)
		return 0;
	if (host_reference (&rhp, hp, MDL) != ISC_R_SUCCESS) {
		lease_dereference (&lease, MDL);
		return 0;
	}
	if (!find_host_for_network (&lease -> subnet,
				    &rhp, &lease -> ip_addr, share)) {
		lease_dereference (&lease, MDL);
		host_dereference (&rhp, MDL);
		return 0;
	}
	host_reference (&lease -> host, rhp, MDL);
	if (rhp -> client_identifier.len > sizeof lease -> uid_buf)
		lease -> uid = dmalloc (rhp -> client_identifier.len, MDL);
	else
		lease -> uid = lease -> uid_buf;
	if (!lease -> uid) {
		lease_dereference (&lease, MDL);
		host_dereference (&rhp, MDL);
		return 0;
	}
	memcpy (lease -> uid, rhp -> client_identifier.data,
		rhp -> client_identifier.len);
	lease -> uid_len = rhp -> client_identifier.len;
	lease -> hardware_addr = rhp -> interface;
	lease -> starts = lease -> cltt = lease -> ends = MIN_TIME;
	lease -> flags = STATIC_LEASE;
	lease -> binding_state = FTS_FREE;

	lease_reference (lp, lease, MDL);

	lease_dereference (&lease, MDL);
	host_dereference (&rhp, MDL);
	return 1;
}

/* Look through all the pools in a list starting with the specified pool
   for a free lease.   We try to find a virgin lease if we can.   If we
   don't find a virgin lease, we try to find a non-virgin lease that's
   free.   If we can't find one of those, we try to reclaim an abandoned
   lease.   If all of these possibilities fail to pan out, we don't return
   a lease at all. */

int allocate_lease (struct lease **lp, struct packet *packet,
		    struct pool *pool, int *peer_has_leases)
{
	struct lease *lease = (struct lease *)0;
	struct lease *candl = (struct lease *)0;

	for (; pool ; pool = pool -> next) {
		if ((pool -> prohibit_list &&
		     permitted (packet, pool -> prohibit_list)) ||
		    (pool -> permit_list &&
		     !permitted (packet, pool -> permit_list)))
			continue;

#if defined (FAILOVER_PROTOCOL)
		/* Peer_has_leases just says that we found at least one
		   free lease.  If no free lease is returned, the caller
		   can deduce that this means the peer is hogging all the
		   free leases, so we can print a better error message. */
		/* XXX Do we need code here to ignore PEER_IS_OWNER and
		 * XXX just check tstp if we're in, e.g., PARTNER_DOWN?
		 * XXX Where do we deal with CONFLICT_DETECTED, et al? */
		/* XXX This should be handled by the lease binding "state
		 * XXX machine" - that is, when we get here, if a lease
		 * XXX could be allocated, it will have the correct
		 * XXX binding state so that the following code will
		 * XXX result in its being allocated. */
		/* Skip to the most expired lease in the pool that is not
		 * owned by a failover peer. */
		if (pool->failover_peer != NULL) {
			if (pool->failover_peer->i_am == primary) {
				candl = pool->free;

				/*
				 * In normal operation, we never want to touch
				 * the peer's leases.  In partner-down 
				 * operation, we need to be able to pick up
				 * the peer's leases after STOS+MCLT.
				 */
				if (pool->backup != NULL) {
					if (((candl == NULL) ||
					     (candl->ends >
					      pool->backup->ends)) &&
					    lease_mine_to_reallocate(
							    pool->backup)) {
						candl = pool->backup;
					} else {
						*peer_has_leases = 1;
					}
				}
			} else {
				candl = pool->backup;

				if (pool->free != NULL) {
					if (((candl == NULL) ||
					     (candl->ends >
					      pool->free->ends)) &&
					    lease_mine_to_reallocate(
							    pool->free)) {
						candl = pool->free;
					} else {
						*peer_has_leases = 1;
					}
				}
			}

			/* Try abandoned leases as a last resort. */
			if ((candl == NULL) &&
			    (pool->abandoned != NULL) &&
			    lease_mine_to_reallocate(pool->abandoned))
				candl = pool->abandoned;
		} else
#endif
		{
			if (pool -> free)
				candl = pool -> free;
			else
				candl = pool -> abandoned;
		}

		/*
		 * XXX: This may not match with documented expectation.
		 * It's expected that when we OFFER a lease, we set its
		 * ends time forward 2 minutes so that it gets sorted to
		 * the end of its free list (avoiding a similar allocation
		 * to another client).  It is not expected that we issue a
		 * "no free leases" error when the last lease has been
		 * offered, but it's not exactly broken either.
		 */
		if (!candl || (candl -> ends > cur_time))
			continue;

		if (!lease) {
			lease = candl;
			continue;
		}

		/*
		 * There are tiers of lease state preference, listed here in
		 * reverse order (least to most preferential):
		 *
		 *    ABANDONED
		 *    FREE/BACKUP
		 *
		 * If the selected lease and candidate are both of the same
		 * state, select the oldest (longest ago) expiration time
		 * between the two.  If the candidate lease is of a higher
		 * preferred grade over the selected lease, use it.
		 */
		if ((lease -> binding_state == FTS_ABANDONED) &&
		    ((candl -> binding_state != FTS_ABANDONED) ||
		     (candl -> ends < lease -> ends))) {
			lease = candl;
			continue;
		} else if (candl -> binding_state == FTS_ABANDONED)
			continue;

		if ((lease -> uid_len || lease -> hardware_addr.hlen) &&
		    ((!candl -> uid_len && !candl -> hardware_addr.hlen) ||
		     (candl -> ends < lease -> ends))) {
			lease = candl;
			continue;
		} else if (candl -> uid_len || candl -> hardware_addr.hlen)
			continue;

		if (candl -> ends < lease -> ends)
			lease = candl;
	}

	if (lease != NULL) {
		if (lease->binding_state == FTS_ABANDONED)
			log_error("Reclaiming abandoned lease %s.",
				  piaddr(lease->ip_addr));

		/*
		 * XXX: For reliability, we go ahead and remove the host
		 * record and try to move on.  For correctness, if there
		 * are any other stale host vectors, we want to find them.
		 */
		if (lease->host != NULL) {
			log_debug("soft impossible condition (%s:%d): stale "
				  "host \"%s\" found on lease %s", MDL,
				  lease->host->name,
				  piaddr(lease->ip_addr));
			host_dereference(&lease->host, MDL);
		}

		lease_reference (lp, lease, MDL);
		return 1;
	}

	return 0;
}

/* Determine whether or not a permit exists on a particular permit list
   that matches the specified packet, returning nonzero if so, zero if
   not. */

int permitted (packet, permit_list)
	struct packet *packet;
	struct permit *permit_list;
{
	struct permit *p;
	int i;

	for (p = permit_list; p; p = p -> next) {
		switch (p -> type) {
		      case permit_unknown_clients:
			if (!packet -> known)
				return 1;
			break;

		      case permit_known_clients:
			if (packet -> known)
				return 1;
			break;

		      case permit_authenticated_clients:
			if (packet -> authenticated)
				return 1;
			break;

		      case permit_unauthenticated_clients:
			if (!packet -> authenticated)
				return 1;
			break;

		      case permit_all_clients:
			return 1;

		      case permit_dynamic_bootp_clients:
			if (!packet -> options_valid ||
			    !packet -> packet_type)
				return 1;
			break;
			
		      case permit_class:
			for (i = 0; i < packet -> class_count; i++) {
				if (p -> class == packet -> classes [i])
					return 1;
				if (packet -> classes [i] &&
				    packet -> classes [i] -> superclass &&
				    (packet -> classes [i] -> superclass ==
				     p -> class))
					return 1;
			}
			break;

		      case permit_after:
			if (cur_time > p->after)
				return 1;
			break;
		}
	}
	return 0;
}

int locate_network (packet)
	struct packet *packet;
{
	struct iaddr ia;
	struct data_string data;
	struct subnet *subnet = (struct subnet *)0;
	struct option_cache *oc;

	/* See if there's a Relay Agent Link Selection Option, or a
	 * Subnet Selection Option.  The Link-Select and Subnet-Select
	 * are formatted and used precisely the same, but we must prefer
	 * the link-select over the subnet-select.
	 */
	if ((oc = lookup_option(&agent_universe, packet->options,
				RAI_LINK_SELECT)) == NULL)
		oc = lookup_option(&dhcp_universe, packet->options,
				   DHO_SUBNET_SELECTION);

	/* If there's no SSO and no giaddr, then use the shared_network
	   from the interface, if there is one.   If not, fail. */
	if (!oc && !packet -> raw -> giaddr.s_addr) {
		if (packet -> interface -> shared_network) {
			shared_network_reference
				(&packet -> shared_network,
				 packet -> interface -> shared_network, MDL);
			return 1;
		}
		return 0;
	}

	/* If there's an option indicating link connection, and it's valid,
	 * use it to figure out the subnet.  If it's not valid, fail.
	 */
	if (oc) {
		memset (&data, 0, sizeof data);
		if (!evaluate_option_cache (&data, packet, (struct lease *)0,
					    (struct client_state *)0,
					    packet -> options,
					    (struct option_state *)0,
					    &global_scope, oc, MDL)) {
			return 0;
		}
		if (data.len != 4) {
			return 0;
		}
		ia.len = 4;
		memcpy (ia.iabuf, data.data, 4);
		data_string_forget (&data, MDL);
	} else {
		ia.len = 4;
		memcpy (ia.iabuf, &packet -> raw -> giaddr, 4);
	}

	/* If we know the subnet on which the IP address lives, use it. */
	if (find_subnet (&subnet, ia, MDL)) {
		shared_network_reference (&packet -> shared_network,
					  subnet -> shared_network, MDL);
		subnet_dereference (&subnet, MDL);
		return 1;
	}

	/* Otherwise, fail. */
	return 0;
}

/*
 * Try to figure out the source address to send packets from.
 *
 * from is the address structure we use to return any address
 * we find.
 *
 * options is the option cache to search.  This may include
 * options from the incoming packet and configuration information.
 *
 * out_options is the outgoing option cache.  This cache
 * may be the same as options.  If send_options isn't NULL
 * we may save the server address option into it.  We do so
 * if send_options is different than options or if the option
 * wasn't in options and we needed to find the address elsewhere.
 *
 * packet is the state structure for the incoming packet
 *
 * When finding the address we first check to see if it is
 * in the options list.  If it isn't we use the first address
 * from the interface.
 *
 * While this is slightly more complicated than I'd like it allows
 * us to use the same code in several different places.  ack,
 * inform and lease query use it to find the address and fill
 * in the options if we get the address from the interface.
 * nack uses it to find the address and copy it to the outgoing
 * cache.  dhcprequest uses it to find the address for comparison
 * and doesn't need to add it to an outgoing list.
 */

void
get_server_source_address(struct in_addr *from,
			  struct option_state *options,
			  struct option_state *out_options,
			  struct packet *packet) {
	unsigned option_num;
	struct option_cache *oc = NULL;
	struct data_string d;
	struct in_addr *a = NULL;
	isc_boolean_t found = ISC_FALSE;
	int allocate = 0;

	memset(&d, 0, sizeof(d));
	memset(from, 0, sizeof(*from));

       	option_num = DHO_DHCP_SERVER_IDENTIFIER;
       	oc = lookup_option(&dhcp_universe, options, option_num);
       	if (oc != NULL)  {
		if (evaluate_option_cache(&d, packet, NULL, NULL, 
					  packet->options, options, 
					  &global_scope, oc, MDL)) {
			if (d.len == sizeof(*from)) {
				found = ISC_TRUE;
				memcpy(from, d.data, sizeof(*from));

				/*
				 * Arrange to save a copy of the data
				 * to the outgoing list.
				 */
				if ((out_options != NULL) &&
				    (options != out_options)) {
					a = from;
					allocate = 1;
				}
			}
			data_string_forget(&d, MDL);
		}
		oc = NULL;
	}

	if ((found == ISC_FALSE) &&
	    (packet->interface->address_count > 0)) {
		*from = packet->interface->addresses[0];

		if (out_options != NULL) {
			a = &packet->interface->addresses[0];
		}
	}

	if ((a != NULL) &&
	    (option_cache_allocate(&oc, MDL))) {
		if (make_const_data(&oc->expression,
				    (unsigned char *)a, sizeof(*a),
				    0, allocate, MDL)) {
			option_code_hash_lookup(&oc->option, 
						dhcp_universe.code_hash,
						&option_num, 0, MDL);
			save_option(&dhcp_universe, out_options, oc);
		}
		option_cache_dereference(&oc, MDL);
	}

	return;
}

/*
 * Set up an option state list to try and find a server option.
 * We don't go through all possible options - in particualr we
 * skip the hosts and we don't include the lease to avoid 
 * making changes to it.  This means that we won't get the
 * correct server id if the admin puts them on hosts or
 * builds the server id with information from the lease.
 *
 * As this is a fallback function (used to handle NAKs or
 * sort out server id mismatch in failover) and requires
 * configuration by the admin, it should be okay.
 */
 
void
setup_server_source_address(struct in_addr *from,
			    struct option_state *options,
			    struct packet *packet) {

	struct option_state *sid_options = NULL;

	if (packet->shared_network != NULL) {
		option_state_allocate (&sid_options, MDL);

		/*
		 * If we have a subnet and group start with that else start
		 * with the shared network group.  The first will recurse and
		 * include the second.
		 */
		if ((packet->shared_network->subnets != NULL) &&
		    (packet->shared_network->subnets->group != NULL)) {
			execute_statements_in_scope(NULL, packet, NULL, NULL,
					packet->options, sid_options,
					&global_scope,
					packet->shared_network->subnets->group,
					NULL, NULL);
		} else {
			execute_statements_in_scope(NULL, packet, NULL, NULL,
					packet->options, sid_options,
					&global_scope,
					packet->shared_network->group,
					NULL, NULL);
		}

		/* do the pool if there is one */
		if (packet->shared_network->pools != NULL) {
			execute_statements_in_scope(NULL, packet, NULL, NULL,
					packet->options, sid_options,
					&global_scope,
					packet->shared_network->pools->group,
					packet->shared_network->group,
					NULL);
		}

		/* currently we don't bother with classes or hosts as
		 * neither seems to be useful in this case */
	}

	/* Make the call to get the server address */
	get_server_source_address(from, sid_options, options, packet);

	/* get rid of the option cache */
	if (sid_options != NULL)
		option_state_dereference(&sid_options, MDL);
}

/*
 * Look for the lowest numbered site code number and
 * apply a log warning if it is less than 224.  Do not
 * permit site codes less than 128 (old code never did).
 *
 * Note that we could search option codes 224 down to 128
 * on the hash table, but the table is (probably) smaller
 * than that if it was declared as a standalone table with
 * defaults.  So we traverse the option code hash.
 */
static int
find_min_site_code(struct universe *u)
{
	if (u->site_code_min)
		return u->site_code_min;

	/*
	 * Note that site_code_min has to be global as we can't pass an
	 * argument through hash_foreach().  The value 224 is taken from
	 * RFC 3942.
	 */
	site_code_min = 224;
	option_code_hash_foreach(u->code_hash, lowest_site_code);

	if (site_code_min < 224) {
		log_error("WARNING: site-local option codes less than 224 have "
			  "been deprecated by RFC3942.  You have options "
			  "listed in site local space %s that number as low as "
			  "%d.  Please investigate if these should be declared "
			  "as regular options rather than site-local options, "
			  "or migrated up past 224.",
			  u->name, site_code_min);
	}

	/*
	 * don't even bother logging, this is just silly, and never worked
	 * on any old version of software.
	 */
	if (site_code_min < 128)
		site_code_min = 128;

	/*
	 * Cache the determined minimum site code on the universe structure.
	 * Note that due to the < 128 check above, a value of zero is
	 * impossible.
	 */
	u->site_code_min = site_code_min;

	return site_code_min;
}

static isc_result_t
lowest_site_code(const void *key, unsigned len, void *object)
{
	struct option *option = object;

	if (option->code < site_code_min)
		site_code_min = option->code;

	return ISC_R_SUCCESS;
}

static void
maybe_return_agent_options(struct packet *packet, struct option_state *options)
{
	/* If there were agent options in the incoming packet, return
	 * them.  Do not return the agent options if they were stashed
	 * on the lease.  We do not check giaddr to detect the presence of
	 * a relay, as this excludes "l2" relay agents which have no giaddr
	 * to set.
	 *
	 * XXX: If the user configures options for the relay agent information
	 * (state->options->universes[agent_universe.index] is not NULL),
	 * we're still required to duplicate other values provided by the
	 * relay agent.  So we need to merge the old values not configured
	 * by the user into the new state, not just give up.
	 */
	if (!packet->agent_options_stashed &&
	    (packet->options != NULL) &&
	    packet->options->universe_count > agent_universe.index &&
	    packet->options->universes[agent_universe.index] != NULL &&
	    (options->universe_count <= agent_universe.index ||
	     options->universes[agent_universe.index] == NULL)) {
		option_chain_head_reference
		    ((struct option_chain_head **)
		     &(options->universes[agent_universe.index]),
		     (struct option_chain_head *)
		     packet->options->universes[agent_universe.index], MDL);

		if (options->universe_count <= agent_universe.index)
			options->universe_count = agent_universe.index + 1;
	}
}
