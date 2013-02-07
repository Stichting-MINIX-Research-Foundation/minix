/* dhcp.c

   ICMP Protocol engine - for sending out pings and receiving
   responses. */

/*
 * Copyright (c) 2011 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 2004,2007,2009 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996-2003 by Internet Software Consortium
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
 * This software has been written for Internet Systems Consortium
 * by Ted Lemon in cooperation with Vixie Enterprises and Nominum, Inc.
 * To learn more about Internet Systems Consortium, see
 * ``https://www.isc.org/''.  To learn more about Vixie Enterprises,
 * see ``http://www.vix.com''.   To learn more about Nominum, Inc., see
 * ``http://www.nominum.com''.
 */

#include "dhcpd.h"
#include "netinet/ip.h"
#include "netinet/ip_icmp.h"

struct icmp_state *icmp_state;
static omapi_object_type_t *dhcp_type_icmp;
static int no_icmp;

OMAPI_OBJECT_ALLOC (icmp_state, struct icmp_state, dhcp_type_icmp)

#if defined (TRACING)
trace_type_t *trace_icmp_input;
trace_type_t *trace_icmp_output;
#endif

/* Initialize the ICMP protocol. */

void icmp_startup (routep, handler)
	int routep;
	void (*handler) (struct iaddr, u_int8_t *, int);
{
	struct protoent *proto;
	int protocol = 1;
	int state;
	isc_result_t result;

	/* Only initialize icmp once. */
	if (dhcp_type_icmp)
		log_fatal ("attempted to reinitialize icmp protocol");

	result = omapi_object_type_register (&dhcp_type_icmp, "icmp",
					     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
					     sizeof (struct icmp_state),
					     0, RC_MISC);

	if (result != ISC_R_SUCCESS)
		log_fatal ("Can't register icmp object type: %s",
			   isc_result_totext (result));

	icmp_state_allocate (&icmp_state, MDL);
	icmp_state -> icmp_handler = handler;

#if defined (TRACING)
	trace_icmp_input = trace_type_register ("icmp-input", (void *)0,
						trace_icmp_input_input,
						trace_icmp_input_stop, MDL);
	trace_icmp_output = trace_type_register ("icmp-output", (void *)0,
						 trace_icmp_output_input,
						 trace_icmp_output_stop, MDL);

	/* If we're playing back a trace file, don't create the socket
	   or set up the callback. */
	if (!trace_playback ()) {
#endif
		/* Get the protocol number (should be 1). */
		proto = getprotobyname ("icmp");
		if (proto)
			protocol = proto -> p_proto;
		
		/* Get a raw socket for the ICMP protocol. */
		icmp_state -> socket = socket (AF_INET, SOCK_RAW, protocol);
		if (icmp_state -> socket < 0) {
			no_icmp = 1;
			log_error ("unable to create icmp socket: %m");
			return;
		}

#if defined (HAVE_SETFD)
		if (fcntl (icmp_state -> socket, F_SETFD, 1) < 0)
			log_error ("Can't set close-on-exec on icmp: %m");
#endif

		/* Make sure it does routing... */
		state = 0;
		if (setsockopt (icmp_state -> socket, SOL_SOCKET, SO_DONTROUTE,
				(char *)&state, sizeof state) < 0)
			log_fatal ("Can't disable SO_DONTROUTE on ICMP: %m");

		result = (omapi_register_io_object
			  ((omapi_object_t *)icmp_state,
			   icmp_readsocket, 0, icmp_echoreply, 0, 0));
		if (result != ISC_R_SUCCESS)
			log_fatal ("Can't register icmp handle: %s",
				   isc_result_totext (result));
#if defined (TRACING)
	}
#endif
}

int icmp_readsocket (h)
	omapi_object_t *h;
{
	struct icmp_state *state;

	state = (struct icmp_state *)h;
	return state -> socket;
}

int icmp_echorequest (addr)
	struct iaddr *addr;
{
	struct sockaddr_in to;
	struct icmp icmp;
	int status;
#if defined (TRACING)
	trace_iov_t iov [2];
#endif

	if (no_icmp)
		return 1;
	if (!icmp_state)
		log_fatal ("ICMP protocol used before initialization.");

	memset (&to, 0, sizeof(to));
#ifdef HAVE_SA_LEN
	to.sin_len = sizeof to;
#endif
	to.sin_family = AF_INET;
	to.sin_port = 0; /* unused. */
	memcpy (&to.sin_addr, addr -> iabuf, sizeof to.sin_addr); /* XXX */

	icmp.icmp_type = ICMP_ECHO;
	icmp.icmp_code = 0;
	icmp.icmp_cksum = 0;
	icmp.icmp_seq = 0;
#if SIZEOF_STRUCT_IADDR_P == 8
	icmp.icmp_id = (((u_int32_t)(u_int64_t)addr) ^
  			(u_int32_t)(((u_int64_t)addr) >> 32));
#else
	icmp.icmp_id = (u_int32_t)addr;
#endif
	memset (&icmp.icmp_dun, 0, sizeof icmp.icmp_dun);

	icmp.icmp_cksum = wrapsum (checksum ((unsigned char *)&icmp,
					     sizeof icmp, 0));

#if defined (TRACING)
	if (trace_playback ()) {
		char *buf = (char *)0;
		unsigned buflen = 0;

		/* Consume the ICMP event. */
		status = trace_get_packet (&trace_icmp_output, &buflen, &buf);
		if (status != ISC_R_SUCCESS)
			log_error ("icmp_echorequest: %s",
				   isc_result_totext (status));
		if (buf)
			dfree (buf, MDL);
	} else {
		if (trace_record ()) {
			iov [0].buf = (char *)addr;
			iov [0].len = sizeof *addr;
			iov [1].buf = (char *)&icmp;
			iov [1].len = sizeof icmp;
			trace_write_packet_iov (trace_icmp_output,
						2, iov, MDL);
		}
#endif
		/* Send the ICMP packet... */
		status = sendto (icmp_state -> socket,
				 (char *)&icmp, sizeof icmp, 0,
				 (struct sockaddr *)&to, sizeof to);
		if (status < 0)
			log_error ("icmp_echorequest %s: %m",
				   inet_ntoa(to.sin_addr));

		if (status != sizeof icmp)
			return 0;
#if defined (TRACING)
	}
#endif
	return 1;
}

isc_result_t icmp_echoreply (h)
	omapi_object_t *h;
{
	struct icmp *icfrom;
	struct ip *ip;
	struct sockaddr_in from;
	u_int8_t icbuf [1500];
	int status;
	SOCKLEN_T sl;
	int hlen, len;
	struct iaddr ia;
	struct icmp_state *state;
#if defined (TRACING)
	trace_iov_t iov [2];
#endif

	state = (struct icmp_state *)h;

	sl = sizeof from;
	status = recvfrom (state -> socket, (char *)icbuf, sizeof icbuf, 0,
			  (struct sockaddr *)&from, &sl);
	if (status < 0) {
		log_error ("icmp_echoreply: %m");
		return ISC_R_UNEXPECTED;
	}

	/* Find the IP header length... */
	ip = (struct ip *)icbuf;
	hlen = IP_HL (ip);

	/* Short packet? */
	if (status < hlen + (sizeof *icfrom)) {
		return ISC_R_SUCCESS;
	}

	len = status - hlen;
	icfrom = (struct icmp *)(icbuf + hlen);

	/* Silently discard ICMP packets that aren't echoreplies. */
	if (icfrom -> icmp_type != ICMP_ECHOREPLY) {
		return ISC_R_SUCCESS;
	}

	/* If we were given a second-stage handler, call it. */
	if (state -> icmp_handler) {
		memcpy (ia.iabuf, &from.sin_addr, sizeof from.sin_addr);
		ia.len = sizeof from.sin_addr;

#if defined (TRACING)
		if (trace_record ()) {
			ia.len = htonl(ia.len);
			iov [0].buf = (char *)&ia;
			iov [0].len = sizeof ia;
			iov [1].buf = (char *)icbuf;
			iov [1].len = len;
			trace_write_packet_iov (trace_icmp_input, 2, iov, MDL);
			ia.len = ntohl(ia.len);
		}
#endif
		(*state -> icmp_handler) (ia, icbuf, len);
	}
	return ISC_R_SUCCESS;
}

#if defined (TRACING)
void trace_icmp_input_input (trace_type_t *ttype, unsigned length, char *buf)
{
	struct iaddr *ia;
	u_int8_t *icbuf;
	ia = (struct iaddr *)buf;
	ia->len = ntohl(ia->len);
	icbuf = (u_int8_t *)(ia + 1);
	if (icmp_state -> icmp_handler)
		(*icmp_state -> icmp_handler) (*ia, icbuf,
					       (int)(length - sizeof ia));
}

void trace_icmp_input_stop (trace_type_t *ttype) { }

void trace_icmp_output_input (trace_type_t *ttype, unsigned length, char *buf)
{
	struct icmp *icmp;
	struct iaddr ia;

	if (length != (sizeof (*icmp) + (sizeof ia))) {
		log_error ("trace_icmp_output_input: data size mismatch %d:%d",
			   length, (int)((sizeof (*icmp)) + (sizeof ia)));
		return;
	}
	ia.len = 4;
	memcpy (ia.iabuf, buf, 4);
	icmp = (struct icmp *)(buf + 1);

	log_error ("trace_icmp_output_input: unsent ping to %s", piaddr (ia));
}

void trace_icmp_output_stop (trace_type_t *ttype) { }
#endif /* TRACING */
