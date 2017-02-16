/*	$NetBSD: ctrace.c,v 1.1.1.2 2014/07/12 11:57:40 spz Exp $	*/
/* trace.c

   Subroutines that support dhcp tracing... */

/*
 * Copyright (c) 2004,2007,2009,2014 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 2001-2003 by Internet Software Consortium
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
__RCSID("$NetBSD: ctrace.c,v 1.1.1.2 2014/07/12 11:57:40 spz Exp $");

#include "dhcpd.h"

#if defined (TRACING)
void trace_interface_register (trace_type_t *ttype, struct interface_info *ip)
{
	trace_interface_packet_t tipkt;

	if (trace_record ()) {
		memset (&tipkt, 0, sizeof tipkt);
		memcpy (&tipkt.hw_address,
			&ip -> hw_address, sizeof ip -> hw_address);
		if (ip->address_count)
			memcpy(&tipkt.primary_address,
			       ip->addresses, sizeof(*ip->addresses));
		memcpy (tipkt.name, ip -> name, sizeof ip -> name);
		tipkt.index = htonl (ip -> index);

		trace_write_packet (ttype, sizeof tipkt, (char *)&tipkt, MDL);
	}	
}

void trace_interface_input (trace_type_t *ttype, unsigned len, char *buf)
{
	trace_interface_packet_t *tipkt;
	struct interface_info *ip;
	struct sockaddr_in *sin;
	struct iaddr addr;
	isc_result_t status;

	if (len != sizeof *tipkt) {
		log_error ("trace interface packet size mismatch: %ld != %d",
			   (long)(sizeof *tipkt), len);
		return;
	}
	tipkt = (trace_interface_packet_t *)buf;
	
	ip = (struct interface_info *)0;
	status = interface_allocate (&ip, MDL);
	if (status != ISC_R_SUCCESS) {
	      foo:
		log_error ("trace_interface_input: %s.",
			   isc_result_totext (status));
		return;
	}
	ip -> ifp = dmalloc (sizeof *(ip -> ifp), MDL);
	if (!ip -> ifp) {
		interface_dereference (&ip, MDL);
		status = ISC_R_NOMEMORY;
		goto foo;
	}

	memcpy (&ip -> hw_address, &tipkt -> hw_address,
		sizeof ip -> hw_address);
	/* XXX: Without the full addresses state it's not quite a full
	 * trace.
	 */
	ip->address_count = ip->address_max = 1;
	ip->addresses = dmalloc(sizeof(*ip->addresses), MDL);
	memcpy(ip->addresses, &tipkt->primary_address, sizeof(*ip->addresses));
	memcpy (ip -> name, tipkt -> name, sizeof ip -> name);
	ip -> index = ntohl (tipkt -> index);

	interface_snorf (ip, 0);
	if (dhcp_interface_discovery_hook)
		(*dhcp_interface_discovery_hook) (ip);

	/* Fake up an ifp. */
	memcpy (ip -> ifp -> ifr_name, ip -> name, sizeof ip -> name);
#ifdef HAVE_SA_LEN
	ip -> ifp -> ifr_addr.sa_len = sizeof (struct sockaddr_in);
#endif
	sin = (struct sockaddr_in *)&ip -> ifp -> ifr_addr;
	sin->sin_addr = ip->addresses[0];

	addr.len = 4;
	memcpy (addr.iabuf, &sin -> sin_addr.s_addr, addr.len);
	if (dhcp_interface_setup_hook)
		(*dhcp_interface_setup_hook) (ip, &addr);
	interface_stash (ip);

	if (!quiet_interface_discovery) {
		log_info ("Listening on Trace/%s/%s%s%s",
			  ip -> name,
			  print_hw_addr (ip -> hw_address.hbuf [0],
					 ip -> hw_address.hlen - 1,
					 &ip -> hw_address.hbuf [1]),
			  (ip -> shared_network ? "/" : ""),
			  (ip -> shared_network ?
			   ip -> shared_network -> name : ""));
		if (strcmp (ip -> name, "fallback")) {
			log_info ("Sending   on Trace/%s/%s%s%s",
				  ip -> name,
				  print_hw_addr (ip -> hw_address.hbuf [0],
						 ip -> hw_address.hlen - 1,
						 &ip -> hw_address.hbuf [1]),
				  (ip -> shared_network ? "/" : ""),
				  (ip -> shared_network ?
				   ip -> shared_network -> name : ""));
		}
	}
	interface_dereference (&ip, MDL);
}

void trace_interface_stop (trace_type_t *ttype) {
	/* XXX */
}

void trace_inpacket_stash (struct interface_info *interface,
			   struct dhcp_packet *packet,
			   unsigned len,
			   unsigned int from_port,
			   struct iaddr from,
			   struct hardware *hfrom)
{
	trace_inpacket_t tip;
	trace_iov_t iov [2];

	if (!trace_record ())
		return;
	tip.from_port = from_port;
	tip.from = from;
	tip.from.len = htonl (tip.from.len);
	if (hfrom) {
		tip.hfrom = *hfrom;
		tip.havehfrom = 1;
	} else {
		memset (&tip.hfrom, 0, sizeof tip.hfrom);
		tip.havehfrom = 0;
	}
	tip.index = htonl (interface -> index);

	iov [0].buf = (char *)&tip;
	iov [0].len = sizeof tip;
	iov [1].buf = (char *)packet;
	iov [1].len = len;
	trace_write_packet_iov (inpacket_trace, 2, iov, MDL);
}

void trace_inpacket_input (trace_type_t *ttype, unsigned len, char *buf)
{
	trace_inpacket_t *tip;
	int index;

	if (len < sizeof *tip) {
		log_error ("trace_input_packet: too short - %d", len);
		return;
	}
	tip = (trace_inpacket_t *)buf;
	index = ntohl (tip -> index);
	tip -> from.len = ntohl (tip -> from.len);
	
	if (index > interface_count ||
	    index < 0 ||
	    !interface_vector [index]) {
		log_error ("trace_input_packet: unknown interface index %d",
			   index);
		return;
	}

	if (!bootp_packet_handler) {
		log_error ("trace_input_packet: no bootp packet handler.");
		return;
	}

	(*bootp_packet_handler) (interface_vector [index],
				 (struct dhcp_packet *)(tip + 1),
				 len - sizeof *tip,
				 tip -> from_port,
				 tip -> from,
				 (tip -> havehfrom ?
				  &tip -> hfrom
				  : (struct hardware *)0));
}

void trace_inpacket_stop (trace_type_t *ttype) { }

ssize_t trace_packet_send (struct interface_info *interface,
			   struct packet *packet,
			   struct dhcp_packet *raw,
			   size_t len,
			   struct in_addr from,
			   struct sockaddr_in *to,
			   struct hardware *hto)
{
	trace_outpacket_t tip;
	trace_iov_t iov [2];

	if (trace_record ()) {
		if (hto) {
			tip.hto = *hto;
			tip.havehto = 1;
		} else {
			memset (&tip.hto, 0, sizeof tip.hto);
			tip.havehto = 0;
		}
		tip.from.len = 4;
		memcpy (tip.from.iabuf, &from, 4);
		tip.to.len = 4;
		memcpy (tip.to.iabuf, &to -> sin_addr, 4);
		tip.to_port = to -> sin_port;
		tip.index = htonl (interface -> index);

		iov [0].buf = (char *)&tip;
		iov [0].len = sizeof tip;
		iov [1].buf = (char *)raw;
		iov [1].len = len;
		trace_write_packet_iov (outpacket_trace, 2, iov, MDL);
	}
	if (!trace_playback ()) {
		return send_packet (interface, packet, raw, len,
				    from, to, hto);
	}
	return len;
}

void trace_outpacket_input (trace_type_t *ttype, unsigned len, char *buf)
{
	trace_outpacket_t *tip;
	int index;

	if (len < sizeof *tip) {
		log_error ("trace_input_packet: too short - %d", len);
		return;
	}
	tip = (trace_outpacket_t *)buf;
	index = ntohl (tip -> index);
	
	if (index > interface_count ||
	    index < 0 ||
	    !interface_vector [index]) {
		log_error ("trace_input_packet: unknown interface index %d",
			   index);
		return;
	}

	/* XXX would be nice to somehow take notice of these. */
}

void trace_outpacket_stop (trace_type_t *ttype) { }

void trace_seed_stash (trace_type_t *ttype, unsigned seed)
{
	u_int32_t outseed;
	if (!trace_record ())
		return;
	outseed = htonl (seed);
	trace_write_packet (ttype, sizeof outseed, (char *)&outseed, MDL);
	return;
}

void trace_seed_input (trace_type_t *ttype, unsigned length, char *buf)
{
	u_int32_t *seed;

	if (length != sizeof seed) {
		log_error ("trace_seed_input: wrong size (%d)", length);
	}
	seed = (u_int32_t *)buf;
	srandom (ntohl (*seed));
}

void trace_seed_stop (trace_type_t *ttype) { }
#endif /* TRACING */
