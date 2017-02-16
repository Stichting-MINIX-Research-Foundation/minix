/*	$NetBSD: failover.c,v 1.1.1.3 2014/07/12 11:58:12 spz Exp $	*/
/* failover.c

   Failover protocol support code... */

/*
 * Copyright (c) 2004-2014 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1999-2003 by Internet Software Consortium
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
__RCSID("$NetBSD: failover.c,v 1.1.1.3 2014/07/12 11:58:12 spz Exp $");

#include "cdefs.h"
#include "dhcpd.h"
#include <omapip/omapip_p.h>

#if defined (FAILOVER_PROTOCOL)
dhcp_failover_state_t *failover_states;
static isc_result_t do_a_failover_option (omapi_object_t *,
					  dhcp_failover_link_t *);
dhcp_failover_listener_t *failover_listeners;

static isc_result_t failover_message_reference (failover_message_t **,
						failover_message_t *,
						const char *file, int line);
static isc_result_t failover_message_dereference (failover_message_t **,
						  const char *file, int line);

static void dhcp_failover_pool_balance(dhcp_failover_state_t *state);
static void dhcp_failover_pool_reqbalance(dhcp_failover_state_t *state);
static int dhcp_failover_pool_dobalance(dhcp_failover_state_t *state,
					isc_boolean_t *sendreq);
static inline int secondary_not_hoarding(dhcp_failover_state_t *state,
					 struct pool *p);


void dhcp_failover_startup ()
{
	dhcp_failover_state_t *state;
	isc_result_t status;
	struct timeval tv;

	for (state = failover_states; state; state = state -> next) {
		dhcp_failover_state_transition (state, "startup");

		if (state -> pool_count == 0) {
			log_error ("failover peer declaration with no %s",
				   "referring pools.");
			log_error ("In order to use failover, you MUST %s",
				   "refer to your main failover declaration");
			log_error ("in each pool declaration.   You MUST %s",
				   "NOT use range declarations outside");
			log_fatal ("of pool declarations.");
		}
		/* In case the peer is already running, immediately try
		   to establish a connection with it. */
		status = dhcp_failover_link_initiate ((omapi_object_t *)state);
		if (status != ISC_R_SUCCESS && status != DHCP_R_INCOMPLETE) {
#if defined (DEBUG_FAILOVER_TIMING)
			log_info ("add_timeout +90 dhcp_failover_reconnect");
#endif
			tv . tv_sec = cur_time + 90;
			tv . tv_usec = 0;
			add_timeout (&tv,
				     dhcp_failover_reconnect, state,
				     (tvref_t)
				     dhcp_failover_state_reference,
				     (tvunref_t)
				     dhcp_failover_state_dereference);
			log_error ("failover peer %s: %s", state -> name,
				   isc_result_totext (status));
		}

		status = (dhcp_failover_listen
			  ((omapi_object_t *)state));
		if (status != ISC_R_SUCCESS) {
#if defined (DEBUG_FAILOVER_TIMING)
			log_info ("add_timeout +90 %s",
				  "dhcp_failover_listener_restart");
#endif
			tv . tv_sec = cur_time + 90;
			tv . tv_usec = 0;
			add_timeout (&tv,
				     dhcp_failover_listener_restart,
				     state,
				     (tvref_t)omapi_object_reference,
				     (tvunref_t)omapi_object_dereference);
		}
	}
}

int dhcp_failover_write_all_states ()
{
	dhcp_failover_state_t *state;

	for (state = failover_states; state; state = state -> next) {
		if (!write_failover_state (state))
			return 0;
	}
	return 1;
}

isc_result_t enter_failover_peer (peer)
	dhcp_failover_state_t *peer;
{
	dhcp_failover_state_t *dup = (dhcp_failover_state_t *)0;
	isc_result_t status;

	status = find_failover_peer (&dup, peer -> name, MDL);
	if (status == ISC_R_NOTFOUND) {
		if (failover_states) {
			dhcp_failover_state_reference (&peer -> next,
						      failover_states, MDL);
			dhcp_failover_state_dereference (&failover_states,
							 MDL);
		}
		dhcp_failover_state_reference (&failover_states, peer, MDL);
		return ISC_R_SUCCESS;
	}
	dhcp_failover_state_dereference (&dup, MDL);
	if (status == ISC_R_SUCCESS)
		return ISC_R_EXISTS;
	return status;
}

isc_result_t find_failover_peer (peer, name, file, line)
	dhcp_failover_state_t **peer;
	const char *name;
	const char *file;
	int line;
{
	dhcp_failover_state_t *p;

	for (p = failover_states; p; p = p -> next)
		if (!strcmp (name, p -> name))
			break;
	if (p)
		return dhcp_failover_state_reference (peer, p, file, line);
	return ISC_R_NOTFOUND;
}

/* The failover protocol has three objects associated with it.  For
   each failover partner declaration in the dhcpd.conf file, primary
   or secondary, there is a failover_state object.  For any primary or
   secondary state object that has a connection to its peer, there is
   also a failover_link object, which has its own input state separate
   from the failover protocol state for managing the actual bytes
   coming in off the wire.  Finally, there will be one listener object
   for every distinct port number associated with a secondary
   failover_state object.  Normally all secondary failover_state
   objects are expected to listen on the same port number, so there
   need be only one listener object, but if different port numbers are
   specified for each failover object, there could be as many as one
   listener object for each secondary failover_state object. */

/* This, then, is the implementation of the failover link object. */

isc_result_t dhcp_failover_link_initiate (omapi_object_t *h)
{
	isc_result_t status;
	dhcp_failover_link_t *obj;
	dhcp_failover_state_t *state;
	omapi_object_t *o;
	int i;
	struct data_string ds;
	omapi_addr_list_t *addrs = (omapi_addr_list_t *)0;
	omapi_addr_t local_addr;

	/* Find the failover state in the object chain. */
	for (o = h; o -> outer; o = o -> outer)
		;
	for (; o; o = o -> inner) {
		if (o -> type == dhcp_type_failover_state)
			break;
	}
	if (!o)
		return DHCP_R_INVALIDARG;
	state = (dhcp_failover_state_t *)o;

	obj = (dhcp_failover_link_t *)0;
	status = dhcp_failover_link_allocate (&obj, MDL);
	if (status != ISC_R_SUCCESS)
		return status;
	option_cache_reference (&obj -> peer_address,
				state -> partner.address, MDL);
	obj -> peer_port = state -> partner.port;
	dhcp_failover_state_reference (&obj -> state_object, state, MDL);

	memset (&ds, 0, sizeof ds);
	if (!evaluate_option_cache (&ds, (struct packet *)0, (struct lease *)0,
				    (struct client_state *)0,
				    (struct option_state *)0,
				    (struct option_state *)0,
				    &global_scope, obj -> peer_address, MDL)) {
		dhcp_failover_link_dereference (&obj, MDL);
		return ISC_R_UNEXPECTED;
	}

	/* Make an omapi address list out of a buffer containing zero or more
	   IPv4 addresses. */
	status = omapi_addr_list_new (&addrs, ds.len / 4, MDL);
	if (status != ISC_R_SUCCESS) {
		dhcp_failover_link_dereference (&obj, MDL);
		return status;
	}

	for (i = 0; i  < addrs -> count; i++) {
		addrs -> addresses [i].addrtype = AF_INET;
		addrs -> addresses [i].addrlen = sizeof (struct in_addr);
		memcpy (addrs -> addresses [i].address,
			&ds.data [i * 4], sizeof (struct in_addr));
		addrs -> addresses [i].port = obj -> peer_port;
	}
	data_string_forget (&ds, MDL);

	/* Now figure out the local address that we're supposed to use. */
	if (!state -> me.address ||
	    !evaluate_option_cache (&ds, (struct packet *)0,
				    (struct lease *)0,
				    (struct client_state *)0,
				    (struct option_state *)0,
				    (struct option_state *)0,
				    &global_scope, state -> me.address,
				    MDL)) {
		memset (&local_addr, 0, sizeof local_addr);
		local_addr.addrtype = AF_INET;
		local_addr.addrlen = sizeof (struct in_addr);
		if (!state -> server_identifier.len) {
			log_fatal ("failover peer %s: no local address.",
				   state -> name);
		}
	} else {
		if (ds.len != sizeof (struct in_addr)) {
			log_error("failover peer %s: 'address' parameter "
				  "fails to resolve to an IPv4 address",
				  state->name);
			data_string_forget (&ds, MDL);
			dhcp_failover_link_dereference (&obj, MDL);
			omapi_addr_list_dereference (&addrs, MDL);
			return DHCP_R_INVALIDARG;
		}
		local_addr.addrtype = AF_INET;
		local_addr.addrlen = ds.len;
		memcpy (local_addr.address, ds.data, ds.len);
		if (!state -> server_identifier.len)
			data_string_copy (&state -> server_identifier,
					  &ds, MDL);
		data_string_forget (&ds, MDL);
		local_addr.port = 0;  /* Let the O.S. choose. */
	}

	status = omapi_connect_list ((omapi_object_t *)obj,
				     addrs, &local_addr);
	omapi_addr_list_dereference (&addrs, MDL);

	dhcp_failover_link_dereference (&obj, MDL);
	return status;
}

isc_result_t dhcp_failover_link_signal (omapi_object_t *h,
					const char *name, va_list ap)
{
	isc_result_t status;
	dhcp_failover_link_t *link;
	omapi_object_t *c;
	dhcp_failover_state_t *s, *state = (dhcp_failover_state_t *)0;
	char *sname;
	int slen;
	struct timeval tv;

	if (h -> type != dhcp_type_failover_link) {
		/* XXX shouldn't happen.   Put an assert here? */
		return ISC_R_UNEXPECTED;
	}
	link = (dhcp_failover_link_t *)h;

	if (!strcmp (name, "connect")) {
	    if (link -> state_object -> i_am == primary) {
		status = dhcp_failover_send_connect (h);
		if (status != ISC_R_SUCCESS) {
		    log_info ("dhcp_failover_send_connect: %s",
			      isc_result_totext (status));
		    omapi_disconnect (h -> outer, 1);
		}
	    } else
		status = ISC_R_SUCCESS;
	    /* Allow the peer fifteen seconds to send us a
	       startup message. */
#if defined (DEBUG_FAILOVER_TIMING)
	    log_info ("add_timeout +15 %s",
		      "dhcp_failover_link_startup_timeout");
#endif
	    tv . tv_sec = cur_time + 15;
	    tv . tv_usec = 0;
	    add_timeout (&tv,
			 dhcp_failover_link_startup_timeout,
			 link,
			 (tvref_t)dhcp_failover_link_reference,
			 (tvunref_t)dhcp_failover_link_dereference);
	    return status;
	}

	if (!strcmp (name, "disconnect")) {
	    if (link -> state_object) {
		dhcp_failover_state_reference (&state,
					       link -> state_object, MDL);
		link -> state = dhcp_flink_disconnected;

		/* Make the transition. */
		if (state->link_to_peer == link)
		    dhcp_failover_state_transition(link->state_object, name);

		/* Schedule an attempt to reconnect. */
#if defined (DEBUG_FAILOVER_TIMING)
		log_info("add_timeout +5 dhcp_failover_reconnect");
#endif
		tv.tv_sec = cur_time + 5;
		tv.tv_usec = cur_tv.tv_usec;
		add_timeout(&tv, dhcp_failover_reconnect, state,
			    (tvref_t)dhcp_failover_state_reference,
			    (tvunref_t)dhcp_failover_state_dereference);

		dhcp_failover_state_dereference (&state, MDL);
	    }
	    return ISC_R_SUCCESS;
	}

	if (!strcmp (name, "status")) {
	  if (link -> state_object) {
	    isc_result_t	status;

	    status = va_arg(ap, isc_result_t);

	    if ((status == ISC_R_HOSTUNREACH) || (status == ISC_R_TIMEDOUT)) {
	      dhcp_failover_state_reference (&state,
					     link -> state_object, MDL);
	      link -> state = dhcp_flink_disconnected;

	      /* Make the transition. */
	      dhcp_failover_state_transition (link -> state_object,
					      "disconnect");

	      /* Start trying to reconnect. */
#if defined (DEBUG_FAILOVER_TIMING)
	      log_info ("add_timeout +5 %s",
			"dhcp_failover_reconnect");
#endif
	      tv . tv_sec = cur_time + 5;
	      tv . tv_usec = 0;
	      add_timeout (&tv, dhcp_failover_reconnect,
			   state,
			   (tvref_t)dhcp_failover_state_reference,
			   (tvunref_t)dhcp_failover_state_dereference);
	    }
	    dhcp_failover_state_dereference (&state, MDL);
	  }
	  return ISC_R_SUCCESS;
	}

	/* Not a signal we recognize? */
	if (strcmp (name, "ready")) {
		if (h -> inner && h -> inner -> type -> signal_handler)
			return (*(h -> inner -> type -> signal_handler))
				(h -> inner, name, ap);
		return ISC_R_NOTFOUND;
	}

	if (!h -> outer || h -> outer -> type != omapi_type_connection)
		return DHCP_R_INVALIDARG;
	c = h -> outer;

	/* We get here because we requested that we be woken up after
           some number of bytes were read, and that number of bytes
           has in fact been read. */
	switch (link -> state) {
	      case dhcp_flink_start:
		link -> state = dhcp_flink_message_length_wait;
		if ((omapi_connection_require (c, 2)) != ISC_R_SUCCESS)
			break;
	      case dhcp_flink_message_length_wait:
	      next_message:
		link -> state = dhcp_flink_message_wait;
		link -> imsg = dmalloc (sizeof (failover_message_t), MDL);
		if (!link -> imsg) {
			status = ISC_R_NOMEMORY;
		      dhcp_flink_fail:
			if (link -> imsg) {
				failover_message_dereference (&link->imsg,
							      MDL);
			}
			link -> state = dhcp_flink_disconnected;
			log_info ("message length wait: %s",
				  isc_result_totext (status));
			omapi_disconnect (c, 1);
			/* XXX just blow away the protocol state now?
			   XXX or will disconnect blow it away? */
			return ISC_R_UNEXPECTED;
		}
		memset (link -> imsg, 0, sizeof (failover_message_t));
		link -> imsg -> refcnt = 1;
		/* Get the length: */
		omapi_connection_get_uint16 (c, &link -> imsg_len);
		link -> imsg_count = 0;	/* Bytes read. */

		/* Ensure the message is of valid length. */
		if (link->imsg_len < DHCP_FAILOVER_MIN_MESSAGE_SIZE ||
		    link->imsg_len > DHCP_FAILOVER_MAX_MESSAGE_SIZE) {
			status = ISC_R_UNEXPECTED;
			goto dhcp_flink_fail;
		}

		if ((omapi_connection_require (c, link -> imsg_len - 2U)) !=
		    ISC_R_SUCCESS)
			break;
	      case dhcp_flink_message_wait:
		/* Read in the message.  At this point we have the
		   entire message in the input buffer.  For each
		   incoming value ID, set a bit in the bitmask
		   indicating that we've gotten it.  Maybe flag an
		   error message if the bit is already set.  Once
		   we're done reading, we can check the bitmask to
		   make sure that the required fields for each message
		   have been included. */

		link -> imsg_count += 2;	/* Count the length as read. */

		/* Get message type. */
		omapi_connection_copyout (&link -> imsg -> type, c, 1);
		link -> imsg_count++;

		/* Get message payload offset. */
		omapi_connection_copyout (&link -> imsg_payoff, c, 1);
		link -> imsg_count++;

		/* Get message time. */
		omapi_connection_get_uint32 (c, &link -> imsg -> time);
		link -> imsg_count += 4;

		/* Get transaction ID. */
		omapi_connection_get_uint32 (c, &link -> imsg -> xid);
		link -> imsg_count += 4;

#if defined (DEBUG_FAILOVER_MESSAGES)
# if !defined(DEBUG_FAILOVER_CONTACT_MESSAGES)
		if (link->imsg->type == FTM_CONTACT)
			goto skip_contact;
# endif
		log_info ("link: message %s  payoff %d  time %ld  xid %ld",
			  dhcp_failover_message_name (link -> imsg -> type),
			  link -> imsg_payoff,
			  (unsigned long)link -> imsg -> time,
			  (unsigned long)link -> imsg -> xid);
# if !defined(DEBUG_FAILOVER_CONTACT_MESSAGES)
	      skip_contact:
# endif
#endif
		/* Skip over any portions of the message header that we
		   don't understand. */
		if (link -> imsg_payoff - link -> imsg_count) {
			omapi_connection_copyout ((unsigned char *)0, c,
						  (link -> imsg_payoff -
						   link -> imsg_count));
			link -> imsg_count = link -> imsg_payoff;
		}
				
		/* Now start sucking options off the wire. */
		while (link -> imsg_count < link -> imsg_len) {
			status = do_a_failover_option (c, link);
			if (status != ISC_R_SUCCESS)
				goto dhcp_flink_fail;
		}

		/* If it's a connect message, try to associate it with
		   a state object. */
		/* XXX this should be authenticated! */
		if (link -> imsg -> type == FTM_CONNECT) {
		    const char *errmsg;
		    int reason;

		    if (!(link->imsg->options_present &
				FTB_RELATIONSHIP_NAME)) {
			errmsg = "missing relationship-name";
			reason = FTR_INVALID_PARTNER;
			goto badconnect;
		    }

		    /* See if we can find a failover_state object that
		       matches this connection.  This message should only
		       be received by a secondary from a primary. */
		    for (s = failover_states; s; s = s -> next) {
			if (dhcp_failover_state_match_by_name(s,
			    &link->imsg->relationship_name))
				state = s;
		    }

		    /* If we can't find a failover protocol state
		       for this remote host, drop the connection */
		    if (!state) {
			    errmsg = "unknown failover relationship name";
			    reason = FTR_INVALID_PARTNER;

			  badconnect:
				/* XXX Send a refusal message first?
				   XXX Look in protocol spec for guidance. */

			    if (state != NULL) {
				sname = state->name;
				slen = strlen(sname);
			    } else if (link->imsg->options_present &
				       FTB_RELATIONSHIP_NAME) {
				sname = (char *)link->imsg->
						relationship_name.data;
				slen = link->imsg->relationship_name.count;
			    } else {
				sname = "unknown";
				slen = strlen(sname);
			    }

			    log_error("Failover CONNECT from %.*s: %s",
				      slen, sname, errmsg);
			    dhcp_failover_send_connectack
				    ((omapi_object_t *)link, state,
				     reason, errmsg);
			    log_info ("failover: disconnect: %s", errmsg);
			    omapi_disconnect (c, 0);
			    link -> state = dhcp_flink_disconnected;
			    return ISC_R_SUCCESS;
		    }

		    if ((cur_time > link -> imsg -> time &&
			 cur_time - link -> imsg -> time > 60) ||
			(cur_time < link -> imsg -> time &&
			 link -> imsg -> time - cur_time > 60)) {
			    errmsg = "time offset too large";
			    reason = FTR_TIMEMISMATCH;
			    goto badconnect;
		    }

		    if (!(link -> imsg -> options_present & FTB_HBA) ||
			link -> imsg -> hba.count != 32) {
			    errmsg = "invalid HBA";
			    reason = FTR_HBA_CONFLICT; /* XXX */
			    goto badconnect;
		    }
		    if (state -> hba)
			    dfree (state -> hba, MDL);
		    state -> hba = dmalloc (32, MDL);
		    if (!state -> hba) {
			    errmsg = "no memory";
			    reason = FTR_MISC_REJECT;
			    goto badconnect;
		    }
		    memcpy (state -> hba, link -> imsg -> hba.data, 32);

		    if (!link -> state_object)
			    dhcp_failover_state_reference
				    (&link -> state_object, state, MDL);
		    if (!link -> peer_address)
			    option_cache_reference
				    (&link -> peer_address,
				     state -> partner.address, MDL);
		}

		/* If we don't have a state object at this point, it's
		   some kind of bogus situation, so just drop the
		   connection. */
		if (!link -> state_object) {
			log_info ("failover: connect: no matching state.");
			omapi_disconnect (c, 1);
			link -> state = dhcp_flink_disconnected;
			return DHCP_R_INVALIDARG;
		}

		/* Once we have the entire message, and we've validated
		   it as best we can here, pass it to the parent. */
		omapi_signal ((omapi_object_t *)link -> state_object,
			      "message", link);
		link -> state = dhcp_flink_message_length_wait;
		if (link -> imsg)
			failover_message_dereference (&link -> imsg, MDL);
		/* XXX This is dangerous because we could get into a tight
		   XXX loop reading input without servicing any other stuff.
		   XXX There needs to be a way to relinquish control but
		   XXX get it back immediately if there's no other work to
		   XXX do. */
		if ((omapi_connection_require (c, 2)) == ISC_R_SUCCESS)
			goto next_message;
		break;

	      default:
		log_fatal("Impossible case at %s:%d.", MDL);
		break;
	}
	return ISC_R_SUCCESS;
}

static isc_result_t do_a_failover_option (c, link)
	omapi_object_t *c;
	dhcp_failover_link_t *link;
{
	u_int16_t option_code;
	u_int16_t option_len;
	unsigned char *op;
	unsigned op_size;
	unsigned op_count;
	int i;
	
	if (link -> imsg_count + 2 > link -> imsg_len) {
		log_error ("FAILOVER: message overflow at option code.");
		return DHCP_R_PROTOCOLERROR;
	}

	/* Get option code. */
	omapi_connection_get_uint16 (c, &option_code);
	link -> imsg_count += 2;
	
	if (link -> imsg_count + 2 > link -> imsg_len) {
		log_error ("FAILOVER: message overflow at length.");
		return DHCP_R_PROTOCOLERROR;
	}

	/* Get option length. */
	omapi_connection_get_uint16 (c, &option_len);
	link -> imsg_count += 2;

	if (link -> imsg_count + option_len > link -> imsg_len) {
		log_error ("FAILOVER: message overflow at data.");
		return DHCP_R_PROTOCOLERROR;
	}

	/* If it's an unknown code, skip over it. */
	if ((option_code > FTO_MAX) ||
	    (ft_options[option_code].type == FT_UNDEF)) {
#if defined (DEBUG_FAILOVER_MESSAGES)
		log_debug ("  option code %d (%s) len %d (not recognized)",
			   option_code,
			   dhcp_failover_option_name (option_code),
			   option_len);
#endif
		omapi_connection_copyout ((unsigned char *)0, c, option_len);
		link -> imsg_count += option_len;
		return ISC_R_SUCCESS;
	}

	/* If it's the digest, do it now. */
	if (ft_options [option_code].type == FT_DIGEST) {
		link -> imsg_count += option_len;
		if (link -> imsg_count != link -> imsg_len) {
			log_error ("FAILOVER: digest not at end of message");
			return DHCP_R_PROTOCOLERROR;
		}
#if defined (DEBUG_FAILOVER_MESSAGES)
		log_debug ("  option %s len %d",
			   ft_options [option_code].name, option_len);
#endif
		/* For now, just dump it. */
		omapi_connection_copyout ((unsigned char *)0, c, option_len);
		return ISC_R_SUCCESS;
	}
	
	/* Only accept an option once. */
	if (link -> imsg -> options_present & ft_options [option_code].bit) {
		log_error ("FAILOVER: duplicate option %s",
			   ft_options [option_code].name);
		return DHCP_R_PROTOCOLERROR;
	}

	/* Make sure the option is appropriate for this type of message.
	   Really, any option is generally allowed for any message, and the
	   cases where this is not true are too complicated to represent in
	   this way - what this code is doing is to just avoid saving the
	   value of an option we don't have any way to use, which allows
	   us to make the failover_message structure smaller. */
	if (ft_options [option_code].bit &&
	    !(fto_allowed [link -> imsg -> type] &
	      ft_options [option_code].bit)) {
		omapi_connection_copyout ((unsigned char *)0, c, option_len);
		link -> imsg_count += option_len;
		return ISC_R_SUCCESS;
	}		

	/* Figure out how many elements, how big they are, and where
	   to store them. */
	if (ft_options [option_code].num_present) {
		/* If this option takes a fixed number of elements,
		   we expect the space for them to be preallocated,
		   and we can just read the data in. */

		op = ((unsigned char *)link -> imsg) +
			ft_options [option_code].offset;
		op_size = ft_sizes [ft_options [option_code].type];
		op_count = ft_options [option_code].num_present;

		if (option_len != op_size * op_count) {
			log_error ("FAILOVER: option size (%d:%d), option %s",
				   option_len,
				   (ft_sizes [ft_options [option_code].type] *
				    ft_options [option_code].num_present),
				   ft_options [option_code].name);
			return DHCP_R_PROTOCOLERROR;
		}
	} else {
		failover_option_t *fo;

		/* FT_DDNS* are special - one or two bytes of status
		   followed by the client FQDN. */
		if (ft_options [option_code].type == FT_DDNS1 ||
		    ft_options [option_code].type == FT_DDNS1) {
			ddns_fqdn_t *ddns =
				((ddns_fqdn_t *)
				 (((char *)link -> imsg) +
				  ft_options [option_code].offset));

			op_count = (ft_options [option_code].type == FT_DDNS1
				    ? 1 : 2);

			omapi_connection_copyout (&ddns -> codes [0],
						  c, op_count);
			link -> imsg_count += op_count;
			if (op_count == 1)
				ddns -> codes [1] = 0;
			op_size = 1;
			op_count = option_len - op_count;

			ddns -> length = op_count;
			ddns -> data = dmalloc (op_count, MDL);
			if (!ddns -> data) {
				log_error ("FAILOVER: no memory getting%s(%d)",
					   " DNS data ", op_count);

				/* Actually, NO_MEMORY, but if we lose here
				   we have to drop the connection. */
				return DHCP_R_PROTOCOLERROR;
			}
			omapi_connection_copyout (ddns -> data, c, op_count);
			goto out;
		}

		/* A zero for num_present means that any number of
		   elements can appear, so we have to figure out how
		   many we got from the length of the option, and then
		   fill out a failover_option structure describing the
		   data. */
		op_size = ft_sizes [ft_options [option_code].type];

		/* Make sure that option data length is a multiple of the
		   size of the data type being sent. */
		if (op_size > 1 && option_len % op_size) {
			log_error ("FAILOVER: option_len %d not %s%d",
				   option_len, "multiple of ", op_size);
			return DHCP_R_PROTOCOLERROR;
		}

		op_count = option_len / op_size;
		
		fo = ((failover_option_t *)
		      (((char *)link -> imsg) +
		       ft_options [option_code].offset));

		fo -> count = op_count;
		fo -> data = dmalloc (option_len, MDL);
		if (!fo -> data) {
			log_error ("FAILOVER: no memory getting %s (%d)",
				   "option data", op_count);

			return DHCP_R_PROTOCOLERROR;
		}			
		op = fo -> data;
	}

	/* For single-byte message values and multi-byte values that
           don't need swapping, just read them in all at once. */
	if (op_size == 1 || ft_options [option_code].type == FT_IPADDR) {
		omapi_connection_copyout ((unsigned char *)op, c, option_len);
		link -> imsg_count += option_len;

		/*
		 * As of 3.1.0, many option codes were changed to conform to
		 * draft revision 12 (which alphabetized, then renumbered all
		 * the option codes without preserving the version option code
		 * nor bumping its value).  As it turns out, the message codes
		 * for CONNECT and CONNECTACK turn out the same, so it tries
		 * its darndest to connect, and falls short (when TLS_REQUEST
		 * comes up size 2 rather than size 1 as draft revision 12 also
		 * mandates).
		 *
		 * The VENDOR_CLASS code in 3.0.x was 11, which is now the HBA
		 * code.  Both work out to be arbitrarily long text-or-byte
		 * strings, so they pass parsing.
		 *
		 * Note that it is possible (or intentional), if highly
		 * improbable, for the HBA bit array to exactly match
		 * isc-V3.0.x.  Warning here is not an issue; if it really is
		 * 3.0.x, there will be a protocol error later on.  If it isn't
		 * actually 3.0.x, then I guess the lucky user will have to
		 * live with a weird warning.
		 */
		if ((option_code == 11) && (option_len > 9) &&
		    (strncmp((const char *)op, "isc-V3.0.", 9) == 0)) {
		        log_error("WARNING: failover as of versions 3.1.0 and "
				  "on are not reverse compatible with "
				  "versions 3.0.x.");
		}

		goto out;
	}

	/* For values that require swapping, read them in one at a time
	   using routines that swap bytes. */
	for (i = 0; i < op_count; i++) {
		switch (ft_options [option_code].type) {
		      case FT_UINT32:
			omapi_connection_get_uint32 (c, (u_int32_t *)op);
			op += 4;
			link -> imsg_count += 4;
			break;
			
		      case FT_UINT16:
			omapi_connection_get_uint16 (c, (u_int16_t *)op);
			op += 2;
			link -> imsg_count += 2;
			break;
			
		      default:
			/* Everything else should have been handled
			   already. */
			log_error ("FAILOVER: option %s: bad type %d",
				   ft_options [option_code].name,
				   ft_options [option_code].type);
			return DHCP_R_PROTOCOLERROR;
		}
	}
      out:
	/* Remember that we got this option. */
	link -> imsg -> options_present |= ft_options [option_code].bit;
	return ISC_R_SUCCESS;
}

isc_result_t dhcp_failover_link_set_value (omapi_object_t *h,
					   omapi_object_t *id,
					   omapi_data_string_t *name,
					   omapi_typed_data_t *value)
{
	if (h -> type != omapi_type_protocol)
		return DHCP_R_INVALIDARG;

	/* Never valid to set these. */
	if (!omapi_ds_strcmp (name, "link-port") ||
	    !omapi_ds_strcmp (name, "link-name") ||
	    !omapi_ds_strcmp (name, "link-state"))
		return ISC_R_NOPERM;

	if (h -> inner && h -> inner -> type -> set_value)
		return (*(h -> inner -> type -> set_value))
			(h -> inner, id, name, value);
	return ISC_R_NOTFOUND;
}

isc_result_t dhcp_failover_link_get_value (omapi_object_t *h,
					   omapi_object_t *id,
					   omapi_data_string_t *name,
					   omapi_value_t **value)
{
	dhcp_failover_link_t *link;

	if (h -> type != omapi_type_protocol)
		return DHCP_R_INVALIDARG;
	link = (dhcp_failover_link_t *)h;
	
	if (!omapi_ds_strcmp (name, "link-port")) {
		return omapi_make_int_value (value, name,
					     (int)link -> peer_port, MDL);
	} else if (!omapi_ds_strcmp (name, "link-state")) {
		if (link -> state >= dhcp_flink_state_max)
			return omapi_make_string_value (value, name,
							"invalid link state",
							MDL);
		return omapi_make_string_value
			(value, name,
			 dhcp_flink_state_names [link -> state], MDL);
	}

	if (h -> inner && h -> inner -> type -> get_value)
		return (*(h -> inner -> type -> get_value))
			(h -> inner, id, name, value);
	return ISC_R_NOTFOUND;
}

isc_result_t dhcp_failover_link_destroy (omapi_object_t *h,
					 const char *file, int line)
{
	dhcp_failover_link_t *link;
	if (h -> type != dhcp_type_failover_link)
		return DHCP_R_INVALIDARG;
	link = (dhcp_failover_link_t *)h;

	if (link -> peer_address)
		option_cache_dereference (&link -> peer_address, file, line);
	if (link -> imsg)
		failover_message_dereference (&link -> imsg, file, line);
	if (link -> state_object)
		dhcp_failover_state_dereference (&link -> state_object,
						 file, line);
	return ISC_R_SUCCESS;
}

/* Write all the published values associated with the object through the
   specified connection. */

isc_result_t dhcp_failover_link_stuff_values (omapi_object_t *c,
					      omapi_object_t *id,
					      omapi_object_t *l)
{
	dhcp_failover_link_t *link;
	isc_result_t status;

	if (l -> type != dhcp_type_failover_link)
		return DHCP_R_INVALIDARG;
	link = (dhcp_failover_link_t *)l;
	
	status = omapi_connection_put_name (c, "link-port");
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_uint32 (c, sizeof (int));
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_uint32 (c, link -> peer_port);
	if (status != ISC_R_SUCCESS)
		return status;
	
	status = omapi_connection_put_name (c, "link-state");
	if (status != ISC_R_SUCCESS)
		return status;
	if (link -> state >= dhcp_flink_state_max)
		status = omapi_connection_put_string (c, "invalid link state");
	else
		status = (omapi_connection_put_string
			  (c, dhcp_flink_state_names [link -> state]));
	if (status != ISC_R_SUCCESS)
		return status;

	if (link -> inner && link -> inner -> type -> stuff_values)
		return (*(link -> inner -> type -> stuff_values)) (c, id,
								link -> inner);
	return ISC_R_SUCCESS;
}

/* Set up a listener for the omapi protocol.    The handle stored points to
   a listener object, not a protocol object. */

isc_result_t dhcp_failover_listen (omapi_object_t *h)
{
	isc_result_t status;
	dhcp_failover_listener_t *obj, *l;
	omapi_value_t *value = (omapi_value_t *)0;
	omapi_addr_t local_addr;
	unsigned long port;

	status = omapi_get_value_str (h, (omapi_object_t *)0,
				      "local-port", &value);
	if (status != ISC_R_SUCCESS)
		return status;
	if (!value -> value) {
		omapi_value_dereference (&value, MDL);
		return DHCP_R_INVALIDARG;
	}
	
	status = omapi_get_int_value (&port, value -> value);
	omapi_value_dereference (&value, MDL);
	if (status != ISC_R_SUCCESS)
		return status;
	local_addr.port = port;

	status = omapi_get_value_str (h, (omapi_object_t *)0,
				      "local-address", &value);
	if (status != ISC_R_SUCCESS)
		return status;
	if (!value -> value) {
	      nogood:
		omapi_value_dereference (&value, MDL);
		return DHCP_R_INVALIDARG;
	}
	
	if (value -> value -> type != omapi_datatype_data ||
	    value -> value -> u.buffer.len != sizeof (struct in_addr))
		goto nogood;

	memcpy (local_addr.address, value -> value -> u.buffer.value,
		value -> value -> u.buffer.len);
	local_addr.addrlen = value -> value -> u.buffer.len;
	local_addr.addrtype = AF_INET;

	omapi_value_dereference (&value, MDL);

	/* Are we already listening on this port and address? */
	for (l = failover_listeners; l; l = l -> next) {
		if (l -> address.port == local_addr.port &&
		    l -> address.addrtype == local_addr.addrtype &&
		    l -> address.addrlen == local_addr.addrlen &&
		    !memcmp (l -> address.address, local_addr.address,
			     local_addr.addrlen))
			break;
	}
	/* Already listening. */
	if (l)
		return ISC_R_SUCCESS;

	obj = (dhcp_failover_listener_t *)0;
	status = dhcp_failover_listener_allocate (&obj, MDL);
	if (status != ISC_R_SUCCESS)
		return status;
	obj -> address = local_addr;
	
	status = omapi_listen_addr ((omapi_object_t *)obj, &obj -> address, 1);
	if (status != ISC_R_SUCCESS)
		return status;

	status = omapi_object_reference (&h -> outer,
					 (omapi_object_t *)obj, MDL);
	if (status != ISC_R_SUCCESS) {
		dhcp_failover_listener_dereference (&obj, MDL);
		return status;
	}
	status = omapi_object_reference (&obj -> inner, h, MDL);
	if (status != ISC_R_SUCCESS) {
		dhcp_failover_listener_dereference (&obj, MDL);
		return status;
	}

	/* Put this listener on the list. */
	if (failover_listeners) {
		dhcp_failover_listener_reference (&obj -> next,
						  failover_listeners, MDL);
		dhcp_failover_listener_dereference (&failover_listeners, MDL);
	}
	dhcp_failover_listener_reference (&failover_listeners, obj, MDL);

	return dhcp_failover_listener_dereference (&obj, MDL);
}

/* Signal handler for protocol listener - if we get a connect signal,
   create a new protocol connection, otherwise pass the signal down. */

isc_result_t dhcp_failover_listener_signal (omapi_object_t *o,
					    const char *name, va_list ap)
{
	isc_result_t status;
	omapi_connection_object_t *c;
	dhcp_failover_link_t *obj;
	dhcp_failover_listener_t *p;
	dhcp_failover_state_t *s, *state = (dhcp_failover_state_t *)0;

	if (!o || o -> type != dhcp_type_failover_listener)
		return DHCP_R_INVALIDARG;
	p = (dhcp_failover_listener_t *)o;

	/* Not a signal we recognize? */
	if (strcmp (name, "connect")) {
		if (p -> inner && p -> inner -> type -> signal_handler)
			return (*(p -> inner -> type -> signal_handler))
				(p -> inner, name, ap);
		return ISC_R_NOTFOUND;
	}

	c = va_arg (ap, omapi_connection_object_t *);
	if (!c || c -> type != omapi_type_connection)
		return DHCP_R_INVALIDARG;

	/* See if we can find a failover_state object that
	   matches this connection. */
	for (s = failover_states; s; s = s -> next) {
		if (dhcp_failover_state_match
		    (s, (u_int8_t *)&c -> remote_addr.sin_addr,
		    sizeof c -> remote_addr.sin_addr)) {
			state = s;
			break;
		}
	}		
	if (!state) {
		log_info ("failover: listener: no matching state");
		omapi_disconnect ((omapi_object_t *)c, 1);
		return(ISC_R_NOTFOUND);
	}

	obj = (dhcp_failover_link_t *)0;
	status = dhcp_failover_link_allocate (&obj, MDL);
	if (status != ISC_R_SUCCESS)
		return status;
	obj -> peer_port = ntohs (c -> remote_addr.sin_port);

	status = omapi_object_reference (&obj -> outer,
					 (omapi_object_t *)c, MDL);
	if (status != ISC_R_SUCCESS) {
	      lose:
		dhcp_failover_link_dereference (&obj, MDL);
		log_info ("failover: listener: picayune failure.");
		omapi_disconnect ((omapi_object_t *)c, 1);
		return status;
	}

	status = omapi_object_reference (&c -> inner,
					 (omapi_object_t *)obj, MDL);
	if (status != ISC_R_SUCCESS)
		goto lose;

	status = dhcp_failover_state_reference (&obj -> state_object,
						state, MDL);
	if (status != ISC_R_SUCCESS)
		goto lose;

	omapi_signal_in ((omapi_object_t *)obj, "connect");

	return dhcp_failover_link_dereference (&obj, MDL);
}

isc_result_t dhcp_failover_listener_set_value (omapi_object_t *h,
						omapi_object_t *id,
						omapi_data_string_t *name,
						omapi_typed_data_t *value)
{
	if (h -> type != dhcp_type_failover_listener)
		return DHCP_R_INVALIDARG;
	
	if (h -> inner && h -> inner -> type -> set_value)
		return (*(h -> inner -> type -> set_value))
			(h -> inner, id, name, value);
	return ISC_R_NOTFOUND;
}

isc_result_t dhcp_failover_listener_get_value (omapi_object_t *h,
						omapi_object_t *id,
						omapi_data_string_t *name,
						omapi_value_t **value)
{
	if (h -> type != dhcp_type_failover_listener)
		return DHCP_R_INVALIDARG;
	
	if (h -> inner && h -> inner -> type -> get_value)
		return (*(h -> inner -> type -> get_value))
			(h -> inner, id, name, value);
	return ISC_R_NOTFOUND;
}

isc_result_t dhcp_failover_listener_destroy (omapi_object_t *h,
					      const char *file, int line)
{
	dhcp_failover_listener_t *l;

	if (h -> type != dhcp_type_failover_listener)
		return DHCP_R_INVALIDARG;
	l = (dhcp_failover_listener_t *)h;
	if (l -> next)
		dhcp_failover_listener_dereference (&l -> next, file, line);

	return ISC_R_SUCCESS;
}

/* Write all the published values associated with the object through the
   specified connection. */

isc_result_t dhcp_failover_listener_stuff (omapi_object_t *c,
					   omapi_object_t *id,
					   omapi_object_t *p)
{
	if (p -> type != dhcp_type_failover_listener)
		return DHCP_R_INVALIDARG;

	if (p -> inner && p -> inner -> type -> stuff_values)
		return (*(p -> inner -> type -> stuff_values)) (c, id,
								p -> inner);
	return ISC_R_SUCCESS;
}

/* Set up master state machine for the failover protocol. */

isc_result_t dhcp_failover_register (omapi_object_t *h)
{
	isc_result_t status;
	dhcp_failover_state_t *obj;
	unsigned long port;
	omapi_value_t *value = (omapi_value_t *)0;

	status = omapi_get_value_str (h, (omapi_object_t *)0,
				      "local-port", &value);
	if (status != ISC_R_SUCCESS)
		return status;
	if (!value -> value) {
		omapi_value_dereference (&value, MDL);
		return DHCP_R_INVALIDARG;
	}
	
	status = omapi_get_int_value (&port, value -> value);
	omapi_value_dereference (&value, MDL);
	if (status != ISC_R_SUCCESS)
		return status;

	obj = (dhcp_failover_state_t *)0;
	dhcp_failover_state_allocate (&obj, MDL);
	obj -> me.port = port;
	
	status = omapi_listen ((omapi_object_t *)obj, port, 1);
	if (status != ISC_R_SUCCESS) {
		dhcp_failover_state_dereference (&obj, MDL);
		return status;
	}

	status = omapi_object_reference (&h -> outer, (omapi_object_t *)obj,
					 MDL);
	if (status != ISC_R_SUCCESS) {
		dhcp_failover_state_dereference (&obj, MDL);
		return status;
	}
	status = omapi_object_reference (&obj -> inner, h, MDL);
	dhcp_failover_state_dereference (&obj, MDL);
	return status;
}

/* Signal handler for protocol state machine. */

isc_result_t dhcp_failover_state_signal (omapi_object_t *o,
					 const char *name, va_list ap)
{
	isc_result_t status;
	dhcp_failover_state_t *state;
	dhcp_failover_link_t *link;
	struct timeval tv;

	if (!o || o -> type != dhcp_type_failover_state)
		return DHCP_R_INVALIDARG;
	state = (dhcp_failover_state_t *)o;

	/* Not a signal we recognize? */
	if (strcmp (name, "disconnect") &&
	    strcmp (name, "message")) {
		if (state -> inner && state -> inner -> type -> signal_handler)
			return (*(state -> inner -> type -> signal_handler))
				(state -> inner, name, ap);
		return ISC_R_NOTFOUND;
	}

	/* Handle connect signals by seeing what state we're in
	   and potentially doing a state transition. */
	if (!strcmp (name, "disconnect")) {
		link = va_arg (ap, dhcp_failover_link_t *);

		dhcp_failover_link_dereference (&state -> link_to_peer, MDL);
		dhcp_failover_state_transition (state, "disconnect");
		if (state -> i_am == primary) {
#if defined (DEBUG_FAILOVER_TIMING)
			log_info ("add_timeout +90 %s",
				  "dhcp_failover_reconnect");
#endif
			tv . tv_sec = cur_time + 90;
			tv . tv_usec = 0;
			add_timeout (&tv, dhcp_failover_reconnect,
				     state,
				     (tvref_t)dhcp_failover_state_reference,
				     (tvunref_t)
				     dhcp_failover_state_dereference);
		}
	} else if (!strcmp (name, "message")) {
		link = va_arg (ap, dhcp_failover_link_t *);

		if (link -> imsg -> type == FTM_CONNECT) {
			/* If we already have a link to the peer, it must be
			   dead, so drop it.
			   XXX Is this the right thing to do?
			   XXX Probably not - what if both peers start at
			   XXX the same time? */
			if (state -> link_to_peer) {
				dhcp_failover_send_connectack
					((omapi_object_t *)link, state,
					 FTR_DUP_CONNECTION,
					 "already connected");
				omapi_disconnect (link -> outer, 1);
				return ISC_R_SUCCESS;
			}
			if (!(link -> imsg -> options_present & FTB_MCLT)) {
				dhcp_failover_send_connectack
					((omapi_object_t *)link, state,
					 FTR_INVALID_MCLT,
					 "no MCLT provided");
				omapi_disconnect (link -> outer, 1);
				return ISC_R_SUCCESS;
			}				

			dhcp_failover_link_reference (&state -> link_to_peer,
						      link, MDL);
			status = (dhcp_failover_send_connectack
				  ((omapi_object_t *)link, state, 0, 0));
			if (status != ISC_R_SUCCESS) {
				dhcp_failover_link_dereference
					(&state -> link_to_peer, MDL);
				log_info ("dhcp_failover_send_connectack: %s",
					  isc_result_totext (status));
				omapi_disconnect (link -> outer, 1);
				return ISC_R_SUCCESS;
			}
			if (link -> imsg -> options_present & FTB_MAX_UNACKED)
				state -> partner.max_flying_updates =
					link -> imsg -> max_unacked;
			if (link -> imsg -> options_present & FTB_RECEIVE_TIMER)
				state -> partner.max_response_delay =
					link -> imsg -> receive_timer;
			state -> mclt = link -> imsg -> mclt;
			dhcp_failover_send_state (state);
			cancel_timeout (dhcp_failover_link_startup_timeout,
					link);
		} else if (link -> imsg -> type == FTM_CONNECTACK) {
		    const char *errmsg;
		    char errbuf[1024];
		    int reason;

		    cancel_timeout (dhcp_failover_link_startup_timeout,
				    link);

		    if (!(link->imsg->options_present &
			  FTB_RELATIONSHIP_NAME)) {
			errmsg = "missing relationship-name";
			reason = FTR_INVALID_PARTNER;
			goto badconnectack;
		    }

		    if (link->imsg->options_present & FTB_REJECT_REASON) {
			/* XXX: add message option to text output. */
			log_error ("Failover CONNECT to %s rejected: %s",
				   state ? state->name : "unknown",
				   (dhcp_failover_reject_reason_print
				    (link -> imsg -> reject_reason)));
			/* XXX print message from peer if peer sent message. */
			omapi_disconnect (link -> outer, 1);
			return ISC_R_SUCCESS;
		    }

		    if (!dhcp_failover_state_match_by_name(state,
			&link->imsg->relationship_name)) {
			/* XXX: Overflow results in log truncation, safe. */
			snprintf(errbuf, sizeof(errbuf), "remote failover "
				 "relationship name %.*s does not match",
				 (int)link->imsg->relationship_name.count,
				 link->imsg->relationship_name.data);
			errmsg = errbuf;
			reason = FTR_INVALID_PARTNER;
		      badconnectack:
			log_error("Failover CONNECTACK from %s: %s",
				  state->name, errmsg);
			dhcp_failover_send_disconnect ((omapi_object_t *)link,
						       reason, errmsg);
			omapi_disconnect (link -> outer, 0);
			return ISC_R_SUCCESS;
		    }

		    if (state -> link_to_peer) {
			errmsg = "already connected";
			reason = FTR_DUP_CONNECTION;
			goto badconnectack;
		    }

		    if ((cur_time > link -> imsg -> time &&
			 cur_time - link -> imsg -> time > 60) ||
			(cur_time < link -> imsg -> time &&
			 link -> imsg -> time - cur_time > 60)) {
			    errmsg = "time offset too large";
			    reason = FTR_TIMEMISMATCH;
			    goto badconnectack;
		    }

		    dhcp_failover_link_reference (&state -> link_to_peer,
						  link, MDL);
#if 0
		    /* XXX This is probably the right thing to do, but
		       XXX for release three, to make the smallest possible
		       XXX change, we are doing this when the peer state
		       XXX changes instead. */
		    if (state -> me.state == startup)
			    dhcp_failover_set_state (state,
						     state -> saved_state);
		    else
#endif
			    dhcp_failover_send_state (state);

		    if (link -> imsg -> options_present & FTB_MAX_UNACKED)
			    state -> partner.max_flying_updates =
				    link -> imsg -> max_unacked;
		    if (link -> imsg -> options_present & FTB_RECEIVE_TIMER)
			    state -> partner.max_response_delay =
				    link -> imsg -> receive_timer;
#if defined (DEBUG_FAILOVER_CONTACT_TIMING)
		    log_info ("add_timeout +%d %s",
			      (int)state -> partner.max_response_delay / 3,
			      "dhcp_failover_send_contact");
#endif
		    tv . tv_sec = cur_time +
			    (int)state -> partner.max_response_delay / 3;
		    tv . tv_usec = 0;
		    add_timeout (&tv,
				 dhcp_failover_send_contact, state,
				 (tvref_t)dhcp_failover_state_reference,
				 (tvunref_t)dhcp_failover_state_dereference);
#if defined (DEBUG_FAILOVER_CONTACT_TIMING)
		    log_info ("add_timeout +%d %s",
			      (int)state -> me.max_response_delay,
			      "dhcp_failover_timeout");
#endif
		    tv . tv_sec = cur_time +
			    (int)state -> me.max_response_delay;
		    tv . tv_usec = 0;
		    add_timeout (&tv,
				 dhcp_failover_timeout, state,
				 (tvref_t)dhcp_failover_state_reference,
				 (tvunref_t)dhcp_failover_state_dereference);
		} else if (link -> imsg -> type == FTM_DISCONNECT) {
		    if (link -> imsg -> reject_reason) {
			log_error ("Failover DISCONNECT from %s: %s",
				   state ? state->name : "unknown",
				   (dhcp_failover_reject_reason_print
				    (link -> imsg -> reject_reason)));
		    }
		    omapi_disconnect (link -> outer, 1);
		} else if (link -> imsg -> type == FTM_BNDUPD) {
			dhcp_failover_process_bind_update (state,
							   link -> imsg);
		} else if (link -> imsg -> type == FTM_BNDACK) {
			dhcp_failover_process_bind_ack (state, link -> imsg);
		} else if (link -> imsg -> type == FTM_UPDREQ) {
			dhcp_failover_process_update_request (state,
							      link -> imsg);
		} else if (link -> imsg -> type == FTM_UPDREQALL) {
			dhcp_failover_process_update_request_all
				(state, link -> imsg);
		} else if (link -> imsg -> type == FTM_UPDDONE) {
			dhcp_failover_process_update_done (state,
							   link -> imsg);
		} else if (link -> imsg -> type == FTM_POOLREQ) {
			dhcp_failover_pool_reqbalance(state);
		} else if (link -> imsg -> type == FTM_POOLRESP) {
			log_info ("pool response: %ld leases",
				  (unsigned long)
				  link -> imsg -> addresses_transferred);
		} else if (link -> imsg -> type == FTM_STATE) {
			dhcp_failover_peer_state_changed (state,
							  link -> imsg);
		}

		/* Add a timeout so that if the partner doesn't send
		   another message for the maximum transmit idle time
		   plus a grace of one second, we close the
		   connection. */
		if (state -> link_to_peer &&
		    state -> link_to_peer == link &&
		    state -> link_to_peer -> state != dhcp_flink_disconnected)
		{
#if defined (DEBUG_FAILOVER_CONTACT_TIMING)
		    log_info ("add_timeout +%d %s",
			      (int)state -> me.max_response_delay,
			      "dhcp_failover_timeout");
#endif
		    tv . tv_sec = cur_time +
			    (int)state -> me.max_response_delay;
		    tv . tv_usec = 0;
		    add_timeout (&tv,
				 dhcp_failover_timeout, state,
				 (tvref_t)dhcp_failover_state_reference,
				 (tvunref_t)dhcp_failover_state_dereference);

		}
	}

	/* Handle all the events we care about... */
	return ISC_R_SUCCESS;
}

isc_result_t dhcp_failover_state_transition (dhcp_failover_state_t *state,
					     const char *name)
{
	isc_result_t status;

	/* XXX Check these state transitions against the spec! */
	if (!strcmp (name, "disconnect")) {
		if (state -> link_to_peer) {
		    log_info ("peer %s: disconnected", state -> name);
		    if (state -> link_to_peer -> state_object)
			dhcp_failover_state_dereference
				(&state -> link_to_peer -> state_object, MDL);
		    dhcp_failover_link_dereference (&state -> link_to_peer,
						    MDL);
		}
		cancel_timeout (dhcp_failover_send_contact, state);
		cancel_timeout (dhcp_failover_timeout, state);
		cancel_timeout (dhcp_failover_startup_timeout, state);

		switch (state -> me.state == startup ?
			state -> saved_state : state -> me.state) {
		      /* In these situations, we remain in the current
		       * state, or if in startup enter those states.
		       */
		      case communications_interrupted:
		      case conflict_done:
		      case partner_down:
		      case paused:
		      case recover:
		      case recover_done:
		      case recover_wait:
		      case resolution_interrupted:
		      case shut_down:
			/* Already in the right state? */
			if (state -> me.state == startup)
				return (dhcp_failover_set_state
					(state, state -> saved_state));
			return ISC_R_SUCCESS;
		
		      case potential_conflict:
			return dhcp_failover_set_state
				(state, resolution_interrupted);
				
		      case normal:
			return dhcp_failover_set_state
				(state, communications_interrupted);

		      case unknown_state:
			return dhcp_failover_set_state
				(state, resolution_interrupted);

		      default:
			log_fatal("Impossible case at %s:%d.", MDL);
			break;	/* can't happen. */
		}
	} else if (!strcmp (name, "connect")) {
		switch (state -> me.state) {
		      case communications_interrupted:
			status = dhcp_failover_set_state (state, normal);
			dhcp_failover_send_updates (state);
			return status;

		      case resolution_interrupted:
			return dhcp_failover_set_state (state,
							potential_conflict);

		      case conflict_done:
		      case partner_down:
		      case potential_conflict:
		      case normal:
		      case recover:
		      case shut_down:
		      case paused:
		      case unknown_state:
		      case recover_done:
		      case startup:
		      case recover_wait:
			return dhcp_failover_send_state (state);

		      default:
			log_fatal("Impossible case at %s:%d.", MDL);
			break;
		}
	} else if (!strcmp (name, "startup")) {
		dhcp_failover_set_state (state, startup);
		return ISC_R_SUCCESS;
	} else if (!strcmp (name, "connect-timeout")) {
		switch (state -> me.state) {
		      case communications_interrupted:
		      case partner_down:
		      case resolution_interrupted:
		      case paused:
		      case startup:
		      case shut_down:
		      case conflict_done:
			return ISC_R_SUCCESS;

		      case normal:
		      case recover:
		      case recover_wait:
		      case recover_done:
		      case unknown_state:
			return dhcp_failover_set_state
				(state, communications_interrupted);

		      case potential_conflict:
			return dhcp_failover_set_state
				(state, resolution_interrupted);

		      default:
			log_fatal("Impossible case at %s:%d.", MDL);
			break;
		}
	}
	return DHCP_R_INVALIDARG;
}

isc_result_t dhcp_failover_set_service_state (dhcp_failover_state_t *state)
{
	switch (state -> me.state) {
	      case unknown_state:
		state -> service_state = not_responding;
		state -> nrr = " (my state unknown)";
		break;

	      case partner_down:
		state -> service_state = service_partner_down;
		state -> nrr = "";
		break;

	      case normal:
		state -> service_state = cooperating;
		state -> nrr = "";
		break;

	      case communications_interrupted:
		state -> service_state = not_cooperating;
		state -> nrr = "";
		break;

	      case resolution_interrupted:
	      case potential_conflict:
	      case conflict_done:
		state -> service_state = not_responding;
		state -> nrr = " (resolving conflicts)";
		break;

	      case recover:
		state -> service_state = not_responding;
		state -> nrr = " (recovering)";
		break;

	      case shut_down:
		state -> service_state = not_responding;
		state -> nrr = " (shut down)";
		break;

	      case paused:
		state -> service_state = not_responding;
		state -> nrr = " (paused)";
		break;

	      case recover_wait:
		state -> service_state = not_responding;
		state -> nrr = " (recover wait)";
		break;

	      case recover_done:
		state -> service_state = not_responding;
		state -> nrr = " (recover done)";
		break;

	      case startup:
		state -> service_state = service_startup;
		state -> nrr = " (startup)";
		break;

	      default:
		log_fatal("Impossible case at %s:%d.\n", MDL);
		break;
	}

	/* Some peer states can require us not to respond, even if our
	   state doesn't. */
	/* XXX hm.   I suspect this isn't true anymore. */
	if (state -> service_state != not_responding) {
		switch (state -> partner.state) {
		      case partner_down:
			state -> service_state = not_responding;
			state -> nrr = " (peer demands: recovering)";
			break;

		      case potential_conflict:
		      case conflict_done:
		      case resolution_interrupted:
			state -> service_state = not_responding;
			state -> nrr = " (peer demands: resolving conflicts)";
			break;

			/* Other peer states don't affect our behaviour. */
		      default:
			break;
		}
	}

	return ISC_R_SUCCESS;
}

isc_result_t dhcp_failover_set_state (dhcp_failover_state_t *state,
				      enum failover_state new_state)
{
    enum failover_state saved_state;
    TIME saved_stos;
    struct pool *p;
    struct shared_network *s;
    struct lease *l;
    struct timeval tv;

    /* If we're in certain states where we're sending updates, and the peer
     * state changes, we need to re-schedule any pending updates just to
     * be on the safe side.  This results in retransmission.
     */
    switch (state -> me.state) {
      case normal:
      case potential_conflict:
      case partner_down:
	if (state -> ack_queue_tail) {
	    struct lease *lp;
		
	    /* Zap the flags. */
	    for (lp = state -> ack_queue_head; lp; lp = lp -> next_pending)
		    lp -> flags = ((lp -> flags & ~ON_ACK_QUEUE) |
				   ON_UPDATE_QUEUE);

	    /* Now hook the ack queue to the beginning of the update
	       queue. */
	    if (state -> update_queue_head) {
		lease_reference (&state -> ack_queue_tail -> next_pending,
				 state -> update_queue_head, MDL);
		lease_dereference (&state -> update_queue_head, MDL);
	    }
	    lease_reference (&state -> update_queue_head,
			     state -> ack_queue_head, MDL);
	    if (!state -> update_queue_tail) {
#if defined (POINTER_DEBUG)
		if (state -> ack_queue_tail -> next_pending) {
		    log_error ("next pending on ack queue tail.");
		    abort ();
		}
#endif
		lease_reference (&state -> update_queue_tail,
				 state -> ack_queue_tail, MDL);
	    }
	    lease_dereference (&state -> ack_queue_tail, MDL);
	    lease_dereference (&state -> ack_queue_head, MDL);
	    state -> cur_unacked_updates = 0;
	}
	/* We will re-queue a timeout later, if applicable. */
	cancel_timeout (dhcp_failover_keepalive, state);
	break;
	
      default:
	break;
    }

    /* Tentatively make the transition. */
    saved_state = state -> me.state;
    saved_stos = state -> me.stos;

    /* Keep the old stos if we're going into recover_wait or if we're
       coming into or out of startup. */
    if (new_state != recover_wait && new_state != startup &&
	saved_state != startup)
	    state -> me.stos = cur_time;

    /* If we're in shutdown, peer is in partner_down, and we're moving
       to recover, we can skip waiting for MCLT to expire.    This happens
       when a server is moved administratively into shutdown prior to
       actually shutting down.   Of course, if there are any updates
       pending we can't actually do this. */
    if (new_state == recover && saved_state == shut_down &&
	state -> partner.state == partner_down &&
	!state -> update_queue_head && !state -> ack_queue_head)
	    state -> me.stos = cur_time - state -> mclt;

    state -> me.state = new_state;
    if (new_state == startup && saved_state != startup)
	state -> saved_state = saved_state;

    /* If we can't record the new state, we can't make a state transition. */
    if (!write_failover_state (state) || !commit_leases ()) {
	    log_error ("Unable to record current failover state for %s",
		       state -> name);
	    state -> me.state = saved_state;
	    state -> me.stos = saved_stos;
	    return ISC_R_IOERROR;
    }

    log_info ("failover peer %s: I move from %s to %s",
	      state -> name, dhcp_failover_state_name_print (saved_state),
	      dhcp_failover_state_name_print (state -> me.state));

    /* If both servers are now normal log it */
    if ((state->me.state == normal) && (state->partner.state == normal))
	    log_info("failover peer %s: Both servers normal", state->name);
    
    /* If we were in startup and we just left it, cancel the timeout. */
    if (new_state != startup && saved_state == startup)
	cancel_timeout (dhcp_failover_startup_timeout, state);

    /*
     * If the state changes for any reason, cancel 'delayed auto state
     * changes' (currently there is just the one).
     */
    cancel_timeout(dhcp_failover_auto_partner_down, state);

    /* Set our service state. */
    dhcp_failover_set_service_state (state);

    /* Tell the peer about it. */
    if (state -> link_to_peer)
	    dhcp_failover_send_state (state);

    switch (new_state) {
	  case communications_interrupted:
	    /*
	     * There is an optional feature to automatically enter partner
	     * down after a timer expires, upon entering comms-interrupted.
	     * This feature is generally not safe except in specific
	     * circumstances.
	     *
	     * A zero value (also the default) disables it.
	     */
	    if (state->auto_partner_down == 0)
		break;

#if defined (DEBUG_FAILOVER_TIMING)
	    log_info("add_timeout +%lu dhcp_failover_auto_partner_down",
		      (unsigned long)state->auto_partner_down);
#endif
	    tv.tv_sec = cur_time + state->auto_partner_down;
	    tv.tv_usec = 0;
	    add_timeout(&tv, dhcp_failover_auto_partner_down, state,
			(tvref_t)omapi_object_reference,
			(tvunref_t)omapi_object_dereference);
	    break;

	  case normal:
	    /* Upon entering normal state, the server is expected to retransmit
	     * all pending binding updates.  This is a good opportunity to
	     * rebalance the pool (potentially making new pending updates),
	     * which also schedules the next pool rebalance.
	     */
	    dhcp_failover_pool_balance(state);
	    dhcp_failover_generate_update_queue(state, 0);

	    if (state->update_queue_tail != NULL) {
		dhcp_failover_send_updates(state);
		log_info("Sending updates to %s.", state->name);
	    }

	    break;

	  case potential_conflict:
	    if (state -> i_am == primary)
		    dhcp_failover_send_update_request (state);
	    break;

	  case startup:
#if defined (DEBUG_FAILOVER_TIMING)
	    log_info ("add_timeout +15 %s",
		      "dhcp_failover_startup_timeout");
#endif
	    tv . tv_sec = cur_time + 15;
	    tv . tv_usec = 0;
	    add_timeout (&tv,
			 dhcp_failover_startup_timeout,
			 state,
			 (tvref_t)omapi_object_reference,
			 (tvunref_t)
			 omapi_object_dereference);
	    break;
	    
	    /* If we come back in recover_wait and there's still waiting
	       to do, set a timeout. */
	  case recover_wait:
	    if (state -> me.stos + state -> mclt > cur_time) {
#if defined (DEBUG_FAILOVER_TIMING)
		    log_info ("add_timeout +%d %s",
			      (int)(cur_time -
				    state -> me.stos + state -> mclt),
			      "dhcp_failover_startup_timeout");
#endif
		    tv . tv_sec = (int)(state -> me.stos + state -> mclt);
		    tv . tv_usec = 0;
		    add_timeout (&tv,
				 dhcp_failover_recover_done,
				 state,
				 (tvref_t)omapi_object_reference,
				 (tvunref_t)
				 omapi_object_dereference);
	    } else
		    dhcp_failover_recover_done (state);
	    break;
	    
	  case recover:
	    /* XXX: We're supposed to calculate if updreq or updreqall is
	     * needed.  In theory, we should only have to updreqall if we
	     * are positive we lost our stable storage.
	     */
	    if (state -> link_to_peer)
		    dhcp_failover_send_update_request_all (state);
	    break;

	  case partner_down:
	    /* For every expired lease, set a timeout for it to become free. */
	    for (s = shared_networks; s; s = s -> next) {
		for (p = s -> pools; p; p = p -> next) {
		    if (p -> failover_peer == state) {
			for (l = p->expired ; l ; l = l->next) {
			    l->tsfp = state->me.stos + state->mclt;
			    l->sort_time = (l->tsfp > l->ends) ?
					   l->tsfp : l->ends;
			}
			if (p->expired &&
			    (p->expired->sort_time < p->next_event_time)) {

			    p->next_event_time = p->expired->sort_time;
#if defined (DEBUG_FAILOVER_TIMING)
			    log_info ("add_timeout +%d %s",
				      (int)(cur_time - p->next_event_time),
				      "pool_timer");
#endif
			    tv.tv_sec = p->next_event_time;
			    tv.tv_usec = 0;
			    add_timeout(&tv, pool_timer, p,
					(tvref_t)pool_reference,
					(tvunref_t)pool_dereference);
			}
		    }
		}
	    }
	    break;


	  default:
	    break;
    }

    return ISC_R_SUCCESS;
}

isc_result_t dhcp_failover_peer_state_changed (dhcp_failover_state_t *state,
					       failover_message_t *msg)
{
	enum failover_state previous_state = state -> partner.state;
	enum failover_state new_state;
	int startupp;

	new_state = msg -> server_state;
	startupp = (msg -> server_flags & FTF_SERVER_STARTUP) ? 1 : 0;

	if (state -> partner.state == new_state && state -> me.state) {
		switch (state -> me.state) {
		      case startup:
			dhcp_failover_set_state (state, state -> saved_state);
			return ISC_R_SUCCESS;

		      case unknown_state:
		      case normal:
		      case potential_conflict:
		      case recover_done:
		      case shut_down:
		      case paused:
		      case recover_wait:
			return ISC_R_SUCCESS;

			/* If we get a peer state change when we're
			   disconnected, we always process it. */
		      case partner_down:
		      case communications_interrupted:
		      case resolution_interrupted:
		      case recover:
		      case conflict_done:
			break;

		      default:
			log_fatal("Impossible case at %s:%d.", MDL);
			break;
		}
	}

	state -> partner.state = new_state;

	log_info ("failover peer %s: peer moves from %s to %s",
		  state -> name,
		  dhcp_failover_state_name_print (previous_state),
		  dhcp_failover_state_name_print (state -> partner.state));

	/* If both servers are now normal log it */
	if ((state->me.state == normal) && (state->partner.state == normal))
		log_info("failover peer %s: Both servers normal", state->name);
    
	if (!write_failover_state (state) || !commit_leases ()) {
		/* This is bad, but it's not fatal.  Of course, if we
		   can't write to the lease database, we're not going to
		   get much done anyway. */
		log_error ("Unable to record current failover state for %s",
			   state -> name);
	}

	/* Quickly validate the new state as being one of the 13 known
	 * states.
	 */
	switch (new_state) {
	      case unknown_state:
	      case startup:
	      case normal:
	      case communications_interrupted:
	      case partner_down:
	      case potential_conflict:
	      case recover:
	      case paused:
	      case shut_down:
	      case recover_done:
	      case resolution_interrupted:
	      case conflict_done:
	      case recover_wait:
		break;

	      default:
		log_error("failover peer %s: Invalid state: %d", state->name,
			  new_state);
		dhcp_failover_set_state(state, shut_down);
		return ISC_R_SUCCESS;
	}

	/* Do any state transitions that are required as a result of the
	   peer's state transition. */

	switch (state -> me.state == startup ?
		state -> saved_state : state -> me.state) {
	      case normal:
		switch (new_state) {
		      case normal:
			dhcp_failover_state_pool_check (state);
			break;

		      case partner_down:
			if (state -> me.state == startup)
				dhcp_failover_set_state (state, recover);
			else
				dhcp_failover_set_state (state,
							 potential_conflict);
			break;

		      case potential_conflict:
		      case resolution_interrupted:
		      case conflict_done:
			/* None of these transitions should ever occur. */
			log_error("Peer %s: Invalid state transition %s "
				"to %s.", state->name,
				dhcp_failover_state_name_print(previous_state),
				dhcp_failover_state_name_print(new_state));
			dhcp_failover_set_state (state, shut_down);
			break;

		      case recover:
		      case shut_down:
			dhcp_failover_set_state (state, partner_down);
			break;

		      case paused:
			dhcp_failover_set_state (state,
						 communications_interrupted);
			break;

		      default:
			/* recover_wait, recover_done, unknown_state, startup,
			 * communications_interrupted
			 */
			break;
		}
		break;

	      case recover:
		switch (new_state) {
		      case recover:
			log_info ("failover peer %s: requesting %s",
				  state -> name, "full update from peer");
			/* Don't send updreqall if we're really in the
			   startup state, because that will result in two
			   being sent. */
			if (state -> me.state == recover)
				dhcp_failover_send_update_request_all (state);
			break;

		      case potential_conflict:
		      case resolution_interrupted:
		      case conflict_done:
		      case normal:
			dhcp_failover_set_state (state, potential_conflict);
			break;

		      case partner_down:
		      case communications_interrupted:
			/* We're supposed to send an update request at this
			   point. */
			/* XXX we don't currently have code here to do any
			   XXX clever detection of when we should send an
			   XXX UPDREQALL message rather than an UPDREQ
			   XXX message.   What to do, what to do? */
			/* Currently when we enter recover state, no matter
			 * the reason, we send an UPDREQALL.  So, it makes
			 * the most sense to stick to that until something
			 * better is done.
			 * Furthermore, we only want to send the update
			 * request if we are not in startup state.
			 */
			if (state -> me.state == recover)
				dhcp_failover_send_update_request_all (state);
			break;

		      case shut_down:
			/* XXX We're not explicitly told what to do in this
			   XXX case, but this transition is consistent with
			   XXX what is elsewhere in the draft. */
			dhcp_failover_set_state (state, partner_down);
			break;

			/* We can't really do anything in this case. */
		      default:
			/* paused, recover_done, recover_wait, unknown_state,
			 * startup.
			 */
			break;
		}
		break;

	      case potential_conflict:
		switch (new_state) {
		      case normal:
			/* This is an illegal transition. */
			log_error("Peer %s moves to normal during conflict "
				  "resolution - panic, shutting down.",
				  state->name);
			dhcp_failover_set_state(state, shut_down);
			break;

		      case conflict_done:
			if (previous_state == potential_conflict)
				dhcp_failover_send_update_request (state);
			else
				log_error("Peer %s: Unexpected move to "
					  "conflict-done.", state->name);
			break;

		      case recover_done:
		      case recover_wait:
		      case potential_conflict:
		      case partner_down:
		      case communications_interrupted:
		      case resolution_interrupted:
		      case paused:
			break;

		      case recover:
			dhcp_failover_set_state (state, recover);
			break;

		      case shut_down:
			dhcp_failover_set_state (state, partner_down);
			break;

		      default:
			/* unknown_state, startup */
			break;
		}
		break;

	      case conflict_done:
		switch (new_state) {
		      case normal:
		      case shut_down:
			dhcp_failover_set_state(state, new_state);
			break;

		      default:
			log_fatal("Peer %s: Invalid attempt to move from %s "
				"to %s while local state is conflict-done.",
				state->name,
				dhcp_failover_state_name_print(previous_state),
				dhcp_failover_state_name_print(new_state));
		}
		break;

	      case partner_down:
		/* Take no action if other server is starting up. */
		if (startupp)
			break;

		switch (new_state) {
			/* This is where we should be. */
		      case recover:
		      case recover_wait:
			break;

		      case recover_done:
			dhcp_failover_set_state (state, normal);
			break;

		      case normal:
		      case potential_conflict:
		      case partner_down:
		      case communications_interrupted:
		      case resolution_interrupted:
		      case conflict_done:
			dhcp_failover_set_state (state, potential_conflict);
			break;

		      default:
			/* shut_down, paused, unknown_state, startup */
			break;
		}
		break;

	      case communications_interrupted:
		switch (new_state) {
		      case paused:
			/* Stick with the status quo. */
			break;

			/* If we're in communications-interrupted and an
			   amnesic peer connects, go to the partner_down
			   state immediately. */
		      case recover:
			dhcp_failover_set_state (state, partner_down);
			break;

		      case normal:
		      case communications_interrupted:
		      case recover_done:
		      case recover_wait:
			/* XXX so we don't need to do this specially in
			   XXX the CONNECT and CONNECTACK handlers. */
			dhcp_failover_send_updates (state);
			dhcp_failover_set_state (state, normal);
			break;

		      case potential_conflict:
		      case partner_down:
		      case resolution_interrupted:
		      case conflict_done:
			dhcp_failover_set_state (state, potential_conflict);
			break;

		      case shut_down:
			dhcp_failover_set_state (state, partner_down);
			break;

		      default:
			/* unknown_state, startup */
			break;
		}
		break;

	      case resolution_interrupted:
		switch (new_state) {
		      case normal:
		      case recover:
		      case potential_conflict:
		      case partner_down:
		      case communications_interrupted:
		      case resolution_interrupted:
		      case conflict_done:
		      case recover_done:
		      case recover_wait:
			dhcp_failover_set_state (state, potential_conflict);
			break;

		      case shut_down:
			dhcp_failover_set_state (state, partner_down);
			break;

		      default:
			/* paused, unknown_state, startup */
			break;
		}
		break;

	      /* Make no transitions while in recover_wait...just wait. */
	      case recover_wait:
		break;

	      case recover_done:
		switch (new_state) {
		      case recover_done:
			log_error("Both servers have entered recover-done!");
		      case normal:
			dhcp_failover_set_state (state, normal);
			break;

		      case shut_down:
			dhcp_failover_set_state (state, partner_down);
			break;

		      default:
			/* potential_conflict, partner_down,
			 * communications_interrupted, resolution_interrupted,
			 * paused, recover, recover_wait, unknown_state,
			 * startup.
			 */
			break;
		}
		break;

		/* We are essentially dead in the water when we're in
		   either shut_down or paused states, and do not do any
		   automatic state transitions. */
	      case shut_down:
	      case paused:
		break;

	      /* XXX: Shouldn't this be a fatal condition? */
	      case unknown_state:
		break;

	      default:
		log_fatal("Impossible condition at %s:%d.", MDL);
		break;

	}

	/* If we didn't make a transition out of startup as a result of
	   the peer's state change, do it now as a result of the fact that
	   we got a state change from the peer. */
	if (state -> me.state == startup && state -> saved_state != startup)
		dhcp_failover_set_state (state, state -> saved_state);
	
	/* For now, just set the service state based on the peer's state
	   if necessary. */
	dhcp_failover_set_service_state (state);

	return ISC_R_SUCCESS;
}

/*
 * Balance operation manual entry; startup, entrance to normal state.  No
 * sense sending a POOLREQ at this stage; the peer is likely about to schedule
 * their own rebalance event upon entering normal themselves.
 */
static void
dhcp_failover_pool_balance(dhcp_failover_state_t *state)
{
	/* Cancel pending event. */
	cancel_timeout(dhcp_failover_pool_rebalance, state);
	state->sched_balance = 0;

	dhcp_failover_pool_dobalance(state, NULL);
}

/*
 * Balance operation entry from timer event.  Once per timer interval is
 * the only time we want to emit POOLREQs (asserting an interrupt in our
 * peer).
 */
void
dhcp_failover_pool_rebalance(void *failover_state)
{
	dhcp_failover_state_t *state;
	isc_boolean_t sendreq = ISC_FALSE;

	state = (dhcp_failover_state_t *)failover_state;

	/* Clear scheduled event indicator. */
	state->sched_balance = 0;

	if (dhcp_failover_pool_dobalance(state, &sendreq))
		dhcp_failover_send_updates(state);

	if (sendreq)
		dhcp_failover_send_poolreq(state);
}

/*
 * Balance operation entry from POOLREQ protocol message.  Do not permit a
 * POOLREQ to send back a POOLREQ.  Ping pong.
 */
static void
dhcp_failover_pool_reqbalance(dhcp_failover_state_t *state)
{
	int queued;

	/* Cancel pending event. */
	cancel_timeout(dhcp_failover_pool_rebalance, state);
	state->sched_balance = 0;

	queued = dhcp_failover_pool_dobalance(state, NULL);

	dhcp_failover_send_poolresp(state, queued);

	if (queued)
		dhcp_failover_send_updates(state);
	else
		log_info("peer %s: Got POOLREQ, answering negatively!  "
			 "Peer may be out of leases or database inconsistent.",
			 state->name);
}

/*
 * Do the meat of the work common to all forms of pool rebalance.  If the
 * caller deems it appropriate to transmit POOLREQ messages, it can use the
 * sendreq pointer to pass in the address of a FALSE value which this function
 * will conditionally turn TRUE if a POOLREQ is determined to be necessary.
 * A NULL value may be passed, in which case no action is taken.
 */
static int
dhcp_failover_pool_dobalance(dhcp_failover_state_t *state,
			    isc_boolean_t *sendreq)
{
	int lts, total, thresh, hold, panic, pass;
	int leases_queued = 0;
	struct lease *lp = (struct lease *)0;
	struct lease *next = (struct lease *)0;
	struct shared_network *s;
	struct pool *p;
	binding_state_t peer_lease_state;
	/* binding_state_t my_lease_state; */
        /* XXX Why is this my_lease_state never used? */
	struct lease **lq;
	int (*log_func)(const char *, ...);
	const char *result, *reqlog;

	if (state -> me.state != normal)
		return 0;

	state->last_balance = cur_time;

	for (s = shared_networks ; s ; s = s->next) {
	    for (p = s->pools ; p ; p = p->next) {
		if (p->failover_peer != state)
		    continue;

		/* Right now we're giving the peer half of the free leases.
		   If we have more leases than the peer (i.e., more than
		   half), then the number of leases we have, less the number
		   of leases the peer has, will be how many more leases we
		   have than the peer has.   So if we send half that number
		   to the peer, we should be even. */
		if (p->failover_peer->i_am == primary) {
			lts = (p->free_leases - p->backup_leases) / 2;
			peer_lease_state = FTS_BACKUP;
			/* my_lease_state = FTS_FREE; */
			lq = &p->free;
		} else {
			lts = (p->backup_leases - p->free_leases) / 2;
			peer_lease_state = FTS_FREE;
			/* my_lease_state = FTS_BACKUP; */
			lq = &p->backup;
		}

		total = p->backup_leases + p->free_leases;

		thresh = ((total * state->max_lease_misbalance) + 50) / 100;
		hold = ((total * state->max_lease_ownership) + 50) / 100;

		/*
		 * If we need leases (so lts is negative) more than negative
		 * double the thresh%, panic and send poolreq to hopefully wake
		 * up the peer (but more likely the db is inconsistent).  But,
		 * if this comes out zero, switch to -1 so that the POOLREQ is
		 * sent on lts == -2 rather than right away at -1.
		 *
		 * Note that we do not subtract -1 from panic all the time
		 * because thresh% and hold% may come out to the same number,
		 * and that is correct operation...where thresh% and hold% are
		 * both -1, we want to send poolreq when lts reaches -3.  So,
		 * "-3 < -2", lts < panic.
		 */
		panic = thresh * -2;

		if (panic == 0)
			panic = -1;

		if ((sendreq != NULL) && (lts < panic)) {
			reqlog = "  (requesting peer rebalance!)";
			*sendreq = ISC_TRUE;
		} else
			reqlog = "";

		log_info("balancing pool %lx %s  total %d  free %d  "
			 "backup %d  lts %d  max-own (+/-)%d%s",
			 (unsigned long)p,
			 (p->shared_network ?
			  p->shared_network->name : ""), p->lease_count,
			 p->free_leases, p->backup_leases, lts, hold,
			 reqlog);

		/* In the first pass, try to allocate leases to the
		 * peer which it would normally be responsible for (if
		 * the lease has a hardware address or client-identifier,
		 * and the load-balance-algorithm chooses the peer to
		 * answer that address), up to a hold% excess in the peer's
		 * favor.  In the second pass, just send the oldest (first
		 * on the list) leases up to a hold% excess in our favor.
		 *
		 * This could make for additional pool rebalance
		 * events, but preserving MAC possession should be
		 * worth it.
		 */
		pass = 0;
		lease_reference(&lp, *lq, MDL);

		while (lp) {
			if (next)
			    lease_dereference(&next, MDL);
			if (lp->next)
			    lease_reference(&next, lp->next, MDL);

			/*
			 * Stop if the pool is 'balanced enough.'
			 *
			 * The pool is balanced enough if:
			 *
			 * 1) We're on the first run through and the peer has
			 *    its fair share of leases already (lts reaches
			 *    -hold).
			 * 2) We're on the second run through, we are shifting
			 *    never-used leases, and there is a perfectly even
			 *    balance (lts reaches zero).
			 * 3) Second run through, we are shifting previously
			 *    used leases, and the local system has its fair
			 *    share but no more (lts reaches hold).
			 *
			 * Note that this is implemented below in 3,2,1 order.
			 */
			if (pass) {
				if (lp->ends) {
					if (lts <= hold)
						break;
				} else {
					if (lts <= 0)
						break;
				}
			} else if (lts <= -hold)
				break;

			if (pass || peer_wants_lease(lp)) {
			    --lts;
			    ++leases_queued;
			    lp->next_binding_state = peer_lease_state;
			    lp->tstp = cur_time;
			    lp->starts = cur_time;

			    if (!supersede_lease(lp, NULL, 0, 1, 0) ||
			        !write_lease(lp))
			    	    log_error("can't commit lease %s on "
					      "giveaway", piaddr(lp->ip_addr));
			}

			lease_dereference(&lp, MDL);
			if (next)
				lease_reference(&lp, next, MDL);
			else if (!pass) {
				pass = 1;
				lease_reference(&lp, *lq, MDL);
			}
		}

		if (next)
			lease_dereference(&next, MDL);
		if (lp)
			lease_dereference(&lp, MDL);

		if (lts > thresh) {
			result = "IMBALANCED";
			log_func = log_error;
		} else {
			result = "balanced";
			log_func = log_info;
		}

		log_func("%s pool %lx %s  total %d  free %d  backup %d  "
			 "lts %d  max-misbal %d", result, (unsigned long)p,
			 (p->shared_network ?
			  p->shared_network->name : ""), p->lease_count,
			 p->free_leases, p->backup_leases, lts, thresh);
 
		/* Recalculate next rebalance event timer. */
		dhcp_failover_pool_check(p);
	    }
	}

	if (leases_queued)
		commit_leases();

	return leases_queued;
}

/* dhcp_failover_pool_check: Called whenever FREE or BACKUP leases change
 * states, on both servers.  Check the scheduled time to rebalance the pool
 * and lower it if applicable.
 */
void
dhcp_failover_pool_check(struct pool *pool)
{
	dhcp_failover_state_t *peer;
	TIME est1, est2;
	struct timeval tv;

	peer = pool->failover_peer;

	if(!peer || peer->me.state != normal)
		return;

	/* Estimate the time left until lease exhaustion.
	 * The first lease on the backup or free lists is also the oldest
	 * lease.  It is reasonable to guess that it will take at least
	 * as much time for a pool to run out of leases, as the present
	 * age of the oldest lease (seconds since it expired).
	 *
	 * Note that this isn't so sane of an assumption if the oldest
	 * lease is a virgin (ends = 0), we wind up sending this against
	 * the max_balance bounds check.
	 */
	if(pool->free && pool->free->ends < cur_time)
		est1 = cur_time - pool->free->ends;
	else
		est1 = 0;

	if(pool->backup && pool->backup->ends < cur_time)
		est2 = cur_time - pool->backup->ends;
	else
		est2 = 0;

	/* We don't want to schedule rebalance for when we think we'll run
	 * out of leases, we want to schedule the rebalance for when we think
	 * the disparity will be 'large enough' to warrant action.
	 */
	est1 = ((est1 * peer->max_lease_misbalance) + 50) / 100;
	est2 = ((est2 * peer->max_lease_misbalance) + 50) / 100;

	/* Guess when the local system will begin issuing POOLREQ panic
	 * attacks because "max_lease_misbalance*2" has been exceeded.
	 */
	if(peer->i_am == primary)
		est1 *= 2;
	else
		est2 *= 2;

	/* Select the smallest time. */
	if(est1 > est2)
		est1 = est2;

	/* Bounded by the maximum configured value. */
	if(est1 > peer->max_balance)
		est1 = peer->max_balance;

	/* Project this time into the future. */
	est1 += cur_time;

	/* Do not move the time down under the minimum. */
	est2 = peer->last_balance + peer->min_balance;
	if(peer->last_balance && (est1 < est2))
		est1 = est2;

	/* Introduce a random delay. */
	est1 += random() % 5;

	/* Do not move the time forward, or reset to the same time. */
	if(peer->sched_balance) {
		if (est1 >= peer->sched_balance)
			return;

		/* We are about to schedule the time down, cancel the
		 * current timeout.
		 */
		cancel_timeout(dhcp_failover_pool_rebalance, peer);
	}

	/* The time is different, and lower, use it. */
	peer->sched_balance = est1;

#if defined(DEBUG_FAILOVER_TIMING)
	log_info("add_timeout +%d dhcp_failover_pool_rebalance",
		 (int)(est1 - cur_time));
#endif
	tv.tv_sec = est1;
	tv.tv_usec = 0;
	add_timeout(&tv, dhcp_failover_pool_rebalance, peer,
			(tvref_t)dhcp_failover_state_reference,
			(tvunref_t)dhcp_failover_state_dereference);
}

int dhcp_failover_state_pool_check (dhcp_failover_state_t *state)
{
	struct shared_network *s;
	struct pool *p;

	for (s = shared_networks; s; s = s -> next) {
		for (p = s -> pools; p; p = p -> next) {
			if (p -> failover_peer != state)
				continue;
			dhcp_failover_pool_check (p);
		}
	}
	return 0;
}

isc_result_t dhcp_failover_send_updates (dhcp_failover_state_t *state)
{
	struct lease *lp = (struct lease *)0;
	isc_result_t status;

	/* Can't update peer if we're not talking to it! */
	if (!state -> link_to_peer)
		return ISC_R_SUCCESS;

	/* If there are acks pending, transmit them prior to potentially
	 * sending new updates for the same lease.
	 */
	if (state->toack_queue_head != NULL)
		dhcp_failover_send_acks(state);

	while ((state -> partner.max_flying_updates >
		state -> cur_unacked_updates) && state -> update_queue_head) {
		/* Grab the head of the update queue. */
		lease_reference (&lp, state -> update_queue_head, MDL);

		/* Send the update to the peer. */
		status = dhcp_failover_send_bind_update (state, lp);
		if (status != ISC_R_SUCCESS) {
			lease_dereference (&lp, MDL);
			return status;
		}
		lp -> flags &= ~ON_UPDATE_QUEUE;

		/* Take it off the head of the update queue and put the next
		   item in the update queue at the head. */
		lease_dereference (&state -> update_queue_head, MDL);
		if (lp -> next_pending) {
			lease_reference (&state -> update_queue_head,
					 lp -> next_pending, MDL);
			lease_dereference (&lp -> next_pending, MDL);
		} else {
			lease_dereference (&state -> update_queue_tail, MDL);
		}

		if (state -> ack_queue_head) {
			lease_reference
				(&state -> ack_queue_tail -> next_pending,
				 lp, MDL);
			lease_dereference (&state -> ack_queue_tail, MDL);
		} else {
			lease_reference (&state -> ack_queue_head, lp, MDL);
		}
#if defined (POINTER_DEBUG)
		if (lp -> next_pending) {
			log_error ("ack_queue_tail: lp -> next_pending");
			abort ();
		}
#endif
		lease_reference (&state -> ack_queue_tail, lp, MDL);
		lp -> flags |= ON_ACK_QUEUE;
		lease_dereference (&lp, MDL);

		/* Count the object as an unacked update. */
		state -> cur_unacked_updates++;
	}
	return ISC_R_SUCCESS;
}

/* Queue an update for a lease.   Always returns 1 at this point - it's
   not an error for this to be called on a lease for which there's no
   failover peer. */

int dhcp_failover_queue_update (struct lease *lease, int immediate)
{
	dhcp_failover_state_t *state;

	if (!lease -> pool ||
	    !lease -> pool -> failover_peer)
		return 1;

	/* If it's already on the update queue, leave it there. */
	if (lease -> flags & ON_UPDATE_QUEUE)
		return 1;

	/* Get the failover state structure for this lease. */
	state = lease -> pool -> failover_peer;

	/* If it's on the ack queue, take it off. */
	if (lease -> flags & ON_ACK_QUEUE)
		dhcp_failover_ack_queue_remove (state, lease);

	if (state -> update_queue_head) {
		lease_reference (&state -> update_queue_tail -> next_pending,
				 lease, MDL);
		lease_dereference (&state -> update_queue_tail, MDL);
	} else {
		lease_reference (&state -> update_queue_head, lease, MDL);
	}
#if defined (POINTER_DEBUG)
	if (lease -> next_pending) {
		log_error ("next pending on update queue lease.");
#if defined (DEBUG_RC_HISTORY)
		dump_rc_history (lease);
#endif
		abort ();
	}
#endif
	lease_reference (&state -> update_queue_tail, lease, MDL);
	lease -> flags |= ON_UPDATE_QUEUE;
	if (immediate)
		dhcp_failover_send_updates (state);
	return 1;
}

int dhcp_failover_send_acks (dhcp_failover_state_t *state)
{
	failover_message_t *msg = (failover_message_t *)0;

	/* Must commit all leases prior to acking them. */
	if (!commit_leases ())
		return 0;

	while (state -> toack_queue_head) {
		failover_message_reference
			(&msg, state -> toack_queue_head, MDL);
		failover_message_dereference
			(&state -> toack_queue_head, MDL);
		if (msg -> next) {
			failover_message_reference
				(&state -> toack_queue_head, msg -> next, MDL);
		}

		dhcp_failover_send_bind_ack (state, msg, 0, (const char *)0);

		failover_message_dereference (&msg, MDL);
	}

	if (state -> toack_queue_tail)
		failover_message_dereference (&state -> toack_queue_tail, MDL);
	state -> pending_acks = 0;

	return 1;
}

void dhcp_failover_toack_queue_timeout (void *vs)
{
	dhcp_failover_state_t *state = vs;

#if defined (DEBUG_FAILOVER_TIMING)
	log_info ("dhcp_failover_toack_queue_timeout");
#endif

	dhcp_failover_send_acks (state);
}

/* Queue an ack for a message.  There is currently no way to queue a
   negative ack -- these need to be sent directly. */

int dhcp_failover_queue_ack (dhcp_failover_state_t *state,
			     failover_message_t *msg)
{
	struct timeval tv;

	if (state -> toack_queue_head) {
		failover_message_reference
			(&state -> toack_queue_tail -> next, msg, MDL);
		failover_message_dereference (&state -> toack_queue_tail, MDL);
	} else {
		failover_message_reference (&state -> toack_queue_head,
					    msg, MDL);
	}
	failover_message_reference (&state -> toack_queue_tail, msg, MDL);

	state -> pending_acks++;

	/* Flush the toack queue whenever we exceed half the number of
	   allowed unacked updates. */
	if (state -> pending_acks >= state -> partner.max_flying_updates / 2) {
		dhcp_failover_send_acks (state);
	}

	/* Schedule a timeout to flush the ack queue. */
	if (state -> pending_acks > 0) {
#if defined (DEBUG_FAILOVER_TIMING)
		log_info ("add_timeout +2 %s",
			  "dhcp_failover_toack_queue_timeout");
#endif
		tv . tv_sec = cur_time + 2;
		tv . tv_usec = 0;
		add_timeout (&tv,
			     dhcp_failover_toack_queue_timeout, state,
			     (tvref_t)dhcp_failover_state_reference,
			     (tvunref_t)dhcp_failover_state_dereference);
	}

	return 1;
}

void dhcp_failover_ack_queue_remove (dhcp_failover_state_t *state,
				     struct lease *lease)
{
	struct lease *lp;

	if (!(lease -> flags & ON_ACK_QUEUE))
		return;

	if (state -> ack_queue_head == lease) {
		lease_dereference (&state -> ack_queue_head, MDL);
		if (lease -> next_pending) {
			lease_reference (&state -> ack_queue_head,
					 lease -> next_pending, MDL);
			lease_dereference (&lease -> next_pending, MDL);
		} else {
			lease_dereference (&state -> ack_queue_tail, MDL);
		}
	} else {
		for (lp = state -> ack_queue_head;
		     lp && lp -> next_pending != lease;
		     lp = lp -> next_pending)
			;

		if (!lp)
			return;

		lease_dereference (&lp -> next_pending, MDL);
		if (lease -> next_pending) {
			lease_reference (&lp -> next_pending,
					 lease -> next_pending, MDL);
			lease_dereference (&lease -> next_pending, MDL);
		} else {
			lease_dereference (&state -> ack_queue_tail, MDL);
			if (lp -> next_pending) {
				log_error ("state -> ack_queue_tail");
				abort ();
			}
			lease_reference (&state -> ack_queue_tail, lp, MDL);
		}
	}

	lease -> flags &= ~ON_ACK_QUEUE;
	/* Multiple acks on one XID is an error and may cause badness. */
	lease->last_xid = 0;
	/* XXX: this violates draft-failover.  We can't send another
	 * update just because we forgot about an old one that hasn't
	 * been acked yet.
	 */
	state -> cur_unacked_updates--;

	/*
	 * When updating leases as a result of an ack, we defer the commit
	 * for performance reasons.  When there are no more acks pending,
	 * do a commit.
	 */
	if (state -> cur_unacked_updates == 0) {
		commit_leases();
	}
}

isc_result_t dhcp_failover_state_set_value (omapi_object_t *h,
					    omapi_object_t *id,
					    omapi_data_string_t *name,
					    omapi_typed_data_t *value)
{
	isc_result_t status;

	if (h -> type != dhcp_type_failover_state)
		return DHCP_R_INVALIDARG;

	/* This list of successful returns is completely wrong, but the
	   fastest way to make dhcpctl do something vaguely sane when
	   you try to change the local state. */

	if (!omapi_ds_strcmp (name, "name")) {
		return ISC_R_SUCCESS;
	} else if (!omapi_ds_strcmp (name, "partner-address")) {
		return ISC_R_SUCCESS;
	} else if (!omapi_ds_strcmp (name, "local-address")) {
		return ISC_R_SUCCESS;
	} else if (!omapi_ds_strcmp (name, "partner-port")) {
		return ISC_R_SUCCESS;
	} else if (!omapi_ds_strcmp (name, "local-port")) {
		return ISC_R_SUCCESS;
	} else if (!omapi_ds_strcmp (name, "max-outstanding-updates")) {
		return ISC_R_SUCCESS;
	} else if (!omapi_ds_strcmp (name, "mclt")) {
		return ISC_R_SUCCESS;
	} else if (!omapi_ds_strcmp (name, "load-balance-max-secs")) {
		return ISC_R_SUCCESS;
	} else if (!omapi_ds_strcmp (name, "load-balance-hba")) {
		return ISC_R_SUCCESS;
	} else if (!omapi_ds_strcmp (name, "partner-state")) {
		return ISC_R_SUCCESS;
	} else if (!omapi_ds_strcmp (name, "local-state")) {
		unsigned long l;
		status = omapi_get_int_value (&l, value);
		if (status != ISC_R_SUCCESS)
			return status;
		return dhcp_failover_set_state ((dhcp_failover_state_t *)h, l);
	} else if (!omapi_ds_strcmp (name, "partner-stos")) {
		return ISC_R_SUCCESS;
	} else if (!omapi_ds_strcmp (name, "local-stos")) {
		return ISC_R_SUCCESS;
	} else if (!omapi_ds_strcmp (name, "hierarchy")) {
		return ISC_R_SUCCESS;
	} else if (!omapi_ds_strcmp (name, "last-packet-sent")) {
		return ISC_R_SUCCESS;
	} else if (!omapi_ds_strcmp (name, "last-timestamp-received")) {
		return ISC_R_SUCCESS;
	} else if (!omapi_ds_strcmp (name, "skew")) {
		return ISC_R_SUCCESS;
	} else if (!omapi_ds_strcmp (name, "max-response-delay")) {
		return ISC_R_SUCCESS;
	} else if (!omapi_ds_strcmp (name, "cur-unacked-updates")) {
		return ISC_R_SUCCESS;
	}
		
	if (h -> inner && h -> inner -> type -> set_value)
		return (*(h -> inner -> type -> set_value))
			(h -> inner, id, name, value);
	return ISC_R_NOTFOUND;
}

void dhcp_failover_keepalive (void *vs)
{
}

void dhcp_failover_reconnect (void *vs)
{
	dhcp_failover_state_t *state = vs;
	isc_result_t status;
	struct timeval tv;

#if defined (DEBUG_FAILOVER_TIMING)
	log_info ("dhcp_failover_reconnect");
#endif
	/* If we already connected the other way, let the connection
           recovery code initiate any retry that may be required. */
	if (state -> link_to_peer)
		return;

	status = dhcp_failover_link_initiate ((omapi_object_t *)state);
	if (status != ISC_R_SUCCESS && status != DHCP_R_INCOMPLETE) {
		log_info ("failover peer %s: %s", state -> name,
			  isc_result_totext (status));
#if defined (DEBUG_FAILOVER_TIMING)
		log_info("add_timeout +90 dhcp_failover_reconnect");
#endif
		tv . tv_sec = cur_time + 90;
		tv . tv_usec = 0;
		add_timeout(&tv, dhcp_failover_reconnect, state,
			    (tvref_t)dhcp_failover_state_reference,
			    (tvunref_t)dhcp_failover_state_dereference);
	}
}

void dhcp_failover_startup_timeout (void *vs)
{
	dhcp_failover_state_t *state = vs;

#if defined (DEBUG_FAILOVER_TIMING)
	log_info ("dhcp_failover_startup_timeout");
#endif

	dhcp_failover_state_transition (state, "disconnect");
}

void dhcp_failover_link_startup_timeout (void *vl)
{
	dhcp_failover_link_t *link = vl;
	omapi_object_t *p;

	for (p = (omapi_object_t *)link; p -> inner; p = p -> inner)
		;
	for (; p; p = p -> outer)
		if (p -> type == omapi_type_connection)
			break;
	if (p) {
		log_info ("failover: link startup timeout");
		omapi_disconnect (p, 1);
	}
}

void dhcp_failover_listener_restart (void *vs)
{
	dhcp_failover_state_t *state = vs;
	isc_result_t status;
	struct timeval tv;

#if defined (DEBUG_FAILOVER_TIMING)
	log_info ("dhcp_failover_listener_restart");
#endif

	status = dhcp_failover_listen ((omapi_object_t *)state);
	if (status != ISC_R_SUCCESS) {
		log_info ("failover peer %s: %s", state -> name,
			  isc_result_totext (status));
#if defined (DEBUG_FAILOVER_TIMING)
		log_info ("add_timeout +90 %s",
			  "dhcp_failover_listener_restart");
#endif
		tv . tv_sec = cur_time + 90;
		tv . tv_usec = 0;
		add_timeout (&tv,
			     dhcp_failover_listener_restart, state,
			     (tvref_t)dhcp_failover_state_reference,
			     (tvunref_t)dhcp_failover_state_dereference);
	}
}

void
dhcp_failover_auto_partner_down(void *vs)
{
	dhcp_failover_state_t *state = vs;

#if defined (DEBUG_FAILOVER_TIMING)
	log_info("dhcp_failover_auto_partner_down");
#endif

	dhcp_failover_set_state(state, partner_down);
}

isc_result_t dhcp_failover_state_get_value (omapi_object_t *h,
					    omapi_object_t *id,
					    omapi_data_string_t *name,
					    omapi_value_t **value)
{
	dhcp_failover_state_t *s;
	struct option_cache *oc;
	struct data_string ds;
	isc_result_t status;

	if (h -> type != dhcp_type_failover_state)
		return DHCP_R_INVALIDARG;
	s = (dhcp_failover_state_t *)h;
	
	if (!omapi_ds_strcmp (name, "name")) {
		if (s -> name)
			return omapi_make_string_value (value,
							name, s -> name, MDL);
		return ISC_R_NOTFOUND;
	} else if (!omapi_ds_strcmp (name, "partner-address")) {
		oc = s -> partner.address;
	      getaddr:
		memset (&ds, 0, sizeof ds);
		if (!evaluate_option_cache (&ds, (struct packet *)0,
					    (struct lease *)0,
					    (struct client_state *)0,
					    (struct option_state *)0,
					    (struct option_state *)0,
					    &global_scope, oc, MDL)) {
			return ISC_R_NOTFOUND;
		}
		status = omapi_make_const_value (value,
						 name, ds.data, ds.len, MDL);
		/* Disgusting kludge: */
		if (oc == s -> me.address && !s -> server_identifier.len)
			data_string_copy (&s -> server_identifier, &ds, MDL);
		data_string_forget (&ds, MDL);
		return status;
	} else if (!omapi_ds_strcmp (name, "local-address")) {
		oc = s -> me.address;
		goto getaddr;
	} else if (!omapi_ds_strcmp (name, "partner-port")) {
		return omapi_make_int_value (value, name,
					     s -> partner.port, MDL);
	} else if (!omapi_ds_strcmp (name, "local-port")) {
		return omapi_make_int_value (value,
					     name, s -> me.port, MDL);
	} else if (!omapi_ds_strcmp (name, "max-outstanding-updates")) {
		return omapi_make_uint_value (value, name,
					      s -> me.max_flying_updates,
					      MDL);
	} else if (!omapi_ds_strcmp (name, "mclt")) {
		return omapi_make_uint_value (value, name, s -> mclt, MDL);
	} else if (!omapi_ds_strcmp (name, "load-balance-max-secs")) {
		return omapi_make_int_value (value, name,
					     s -> load_balance_max_secs, MDL);
	} else if (!omapi_ds_strcmp (name, "load-balance-hba")) {
		if (s -> hba)
			return omapi_make_const_value (value, name,
						       s -> hba, 32, MDL);
		return ISC_R_NOTFOUND;
	} else if (!omapi_ds_strcmp (name, "partner-state")) {
		return omapi_make_uint_value (value, name,
					     s -> partner.state, MDL);
	} else if (!omapi_ds_strcmp (name, "local-state")) {
		return omapi_make_uint_value (value, name,
					      s -> me.state, MDL);
	} else if (!omapi_ds_strcmp (name, "partner-stos")) {
		return omapi_make_int_value (value, name,
					     s -> partner.stos, MDL);
	} else if (!omapi_ds_strcmp (name, "local-stos")) {
		return omapi_make_int_value (value, name,
					     s -> me.stos, MDL);
	} else if (!omapi_ds_strcmp (name, "hierarchy")) {
		return omapi_make_uint_value (value, name, s -> i_am, MDL);
	} else if (!omapi_ds_strcmp (name, "last-packet-sent")) {
		return omapi_make_int_value (value, name,
					     s -> last_packet_sent, MDL);
	} else if (!omapi_ds_strcmp (name, "last-timestamp-received")) {
		return omapi_make_int_value (value, name,
					     s -> last_timestamp_received,
					     MDL);
	} else if (!omapi_ds_strcmp (name, "skew")) {
		return omapi_make_int_value (value, name, s -> skew, MDL);
	} else if (!omapi_ds_strcmp (name, "max-response-delay")) {
		return omapi_make_uint_value (value, name,
					     s -> me.max_response_delay,
					      MDL);
	} else if (!omapi_ds_strcmp (name, "cur-unacked-updates")) {
		return omapi_make_int_value (value, name,
					     s -> cur_unacked_updates, MDL);
	}
		
	if (h -> inner && h -> inner -> type -> get_value)
		return (*(h -> inner -> type -> get_value))
			(h -> inner, id, name, value);
	return ISC_R_NOTFOUND;
}

isc_result_t dhcp_failover_state_destroy (omapi_object_t *h,
					      const char *file, int line)
{
	dhcp_failover_state_t *s;

	if (h -> type != dhcp_type_failover_state)
		return DHCP_R_INVALIDARG;
	s = (dhcp_failover_state_t *)h;

	if (s -> link_to_peer)
	    dhcp_failover_link_dereference (&s -> link_to_peer, file, line);
	if (s -> name) {
		dfree (s -> name, MDL);
		s -> name = (char *)0;
	}
	if (s -> partner.address)
		option_cache_dereference (&s -> partner.address, file, line);
	if (s -> me.address)
		option_cache_dereference (&s -> me.address, file, line);
	if (s -> hba) {
		dfree (s -> hba, file, line);
		s -> hba = (u_int8_t *)0;
	}
	if (s -> update_queue_head)
		lease_dereference (&s -> update_queue_head, file, line);
	if (s -> update_queue_tail)
		lease_dereference (&s -> update_queue_tail, file, line);
	if (s -> ack_queue_head)
		lease_dereference (&s -> ack_queue_head, file, line);
	if (s -> ack_queue_tail)
		lease_dereference (&s -> ack_queue_tail, file, line);
	if (s -> send_update_done)
		lease_dereference (&s -> send_update_done, file, line);
	if (s -> toack_queue_head)
		failover_message_dereference (&s -> toack_queue_head,
					      file, line);
	if (s -> toack_queue_tail)
		failover_message_dereference (&s -> toack_queue_tail,
					      file, line);
	return ISC_R_SUCCESS;
}

/* Write all the published values associated with the object through the
   specified connection. */

isc_result_t dhcp_failover_state_stuff (omapi_object_t *c,
					omapi_object_t *id,
					omapi_object_t *h)
{
	/* In this function c should be a (omapi_connection_object_t *) */

	dhcp_failover_state_t *s;
	isc_result_t status;

	if (c -> type != omapi_type_connection)
		return DHCP_R_INVALIDARG;

	if (h -> type != dhcp_type_failover_state)
		return DHCP_R_INVALIDARG;
	s = (dhcp_failover_state_t *)h;
	
	status = omapi_connection_put_name (c, "name");
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_string (c, s -> name);
	if (status != ISC_R_SUCCESS)
		return status;

	status = omapi_connection_put_name (c, "partner-address");
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_uint32 (c, sizeof s -> partner.address);
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_copyin (c, (u_int8_t *)&s -> partner.address,
					  sizeof s -> partner.address);
	if (status != ISC_R_SUCCESS)
		return status;
	
	status = omapi_connection_put_name (c, "partner-port");
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_uint32 (c, sizeof (u_int32_t));
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_uint32 (c, (u_int32_t)s -> partner.port);
	if (status != ISC_R_SUCCESS)
		return status;
	
	status = omapi_connection_put_name (c, "local-address");
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_uint32 (c, sizeof s -> me.address);
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_copyin (c, (u_int8_t *)&s -> me.address,
					  sizeof s -> me.address);
	if (status != ISC_R_SUCCESS)
		return status;
	
	status = omapi_connection_put_name (c, "local-port");
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_uint32 (c, sizeof (u_int32_t));
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_uint32 (c, (u_int32_t)s -> me.port);
	if (status != ISC_R_SUCCESS)
		return status;
	
	status = omapi_connection_put_name (c, "max-outstanding-updates");
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_uint32 (c, sizeof (u_int32_t));
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_uint32 (c,
					      s -> me.max_flying_updates);
	if (status != ISC_R_SUCCESS)
		return status;

	status = omapi_connection_put_name (c, "mclt");
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_uint32 (c, sizeof (u_int32_t));
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_uint32 (c, s -> mclt);
	if (status != ISC_R_SUCCESS)
		return status;

	status = omapi_connection_put_name (c, "load-balance-max-secs");
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_uint32 (c, sizeof (u_int32_t));
	if (status != ISC_R_SUCCESS)
		return status;
	status = (omapi_connection_put_uint32
		  (c, (u_int32_t)s -> load_balance_max_secs));
	if (status != ISC_R_SUCCESS)
		return status;

	
	if (s -> hba) {
		status = omapi_connection_put_name (c, "load-balance-hba");
		if (status != ISC_R_SUCCESS)
			return status;
		status = omapi_connection_put_uint32 (c, 32);
		if (status != ISC_R_SUCCESS)
			return status;
		status = omapi_connection_copyin (c, s -> hba, 32);
		if (status != ISC_R_SUCCESS)
			return status;
	}

	status = omapi_connection_put_name (c, "partner-state");
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_uint32 (c, sizeof (u_int32_t));
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_uint32 (c, s -> partner.state);
	if (status != ISC_R_SUCCESS)
		return status;
	
	status = omapi_connection_put_name (c, "local-state");
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_uint32 (c, sizeof (u_int32_t));
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_uint32 (c, s -> me.state);
	if (status != ISC_R_SUCCESS)
		return status;
	
	status = omapi_connection_put_name (c, "partner-stos");
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_uint32 (c, sizeof (u_int32_t));
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_uint32 (c,
					      (u_int32_t)s -> partner.stos);
	if (status != ISC_R_SUCCESS)
		return status;

	status = omapi_connection_put_name (c, "local-stos");
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_uint32 (c, sizeof (u_int32_t));
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_uint32 (c, (u_int32_t)s -> me.stos);
	if (status != ISC_R_SUCCESS)
		return status;

	status = omapi_connection_put_name (c, "hierarchy");
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_uint32 (c, sizeof (u_int32_t));
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_uint32 (c, s -> i_am);
	if (status != ISC_R_SUCCESS)
		return status;

	status = omapi_connection_put_name (c, "last-packet-sent");
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_uint32 (c, sizeof (u_int32_t));
	if (status != ISC_R_SUCCESS)
		return status;
	status = (omapi_connection_put_uint32
		  (c, (u_int32_t)s -> last_packet_sent));
	if (status != ISC_R_SUCCESS)
		return status;

	status = omapi_connection_put_name (c, "last-timestamp-received");
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_uint32 (c, sizeof (u_int32_t));
	if (status != ISC_R_SUCCESS)
		return status;
	status = (omapi_connection_put_uint32
		  (c, (u_int32_t)s -> last_timestamp_received));
	if (status != ISC_R_SUCCESS)
		return status;

	status = omapi_connection_put_name (c, "skew");
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_uint32 (c, sizeof (u_int32_t));
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_uint32 (c, (u_int32_t)s -> skew);
	if (status != ISC_R_SUCCESS)
		return status;

	status = omapi_connection_put_name (c, "max-response-delay");
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_uint32 (c, sizeof (u_int32_t));
	if (status != ISC_R_SUCCESS)
		return status;
	status = (omapi_connection_put_uint32
		  (c, (u_int32_t)s -> me.max_response_delay));
	if (status != ISC_R_SUCCESS)
		return status;
	
	status = omapi_connection_put_name (c, "cur-unacked-updates");
	if (status != ISC_R_SUCCESS)
		return status;
	status = omapi_connection_put_uint32 (c, sizeof (u_int32_t));
	if (status != ISC_R_SUCCESS)
		return status;
	status = (omapi_connection_put_uint32
		  (c, (u_int32_t)s -> cur_unacked_updates));
	if (status != ISC_R_SUCCESS)
		return status;

	if (h -> inner && h -> inner -> type -> stuff_values)
		return (*(h -> inner -> type -> stuff_values)) (c, id,
								h -> inner);
	return ISC_R_SUCCESS;
}

isc_result_t dhcp_failover_state_lookup (omapi_object_t **sp,
					 omapi_object_t *id,
					 omapi_object_t *ref)
{
	omapi_value_t *tv = (omapi_value_t *)0;
	isc_result_t status;
	dhcp_failover_state_t *s;

	if (!ref)
		return DHCP_R_NOKEYS;

	/* First see if we were sent a handle. */
	status = omapi_get_value_str (ref, id, "handle", &tv);
	if (status == ISC_R_SUCCESS) {
		status = omapi_handle_td_lookup (sp, tv -> value);

		omapi_value_dereference (&tv, MDL);
		if (status != ISC_R_SUCCESS)
			return status;

		/* Don't return the object if the type is wrong. */
		if ((*sp) -> type != dhcp_type_failover_state) {
			omapi_object_dereference (sp, MDL);
			return DHCP_R_INVALIDARG;
		}
	}

	/* Look the failover state up by peer name. */
	status = omapi_get_value_str (ref, id, "name", &tv);
	if (status == ISC_R_SUCCESS) {
		for (s = failover_states; s; s = s -> next) {
			unsigned l = strlen (s -> name);
			if (l == tv -> value -> u.buffer.len &&
			    !memcmp (s -> name,
				     tv -> value -> u.buffer.value, l))
				break;
		}
		omapi_value_dereference (&tv, MDL);

		/* If we already have a lease, and it's not the same one,
		   then the query was invalid. */
		if (*sp && *sp != (omapi_object_t *)s) {
			omapi_object_dereference (sp, MDL);
			return DHCP_R_KEYCONFLICT;
		} else if (!s) {
			if (*sp)
				omapi_object_dereference (sp, MDL);
			return ISC_R_NOTFOUND;
		} else if (!*sp)
			/* XXX fix so that hash lookup itself creates
			   XXX the reference. */
			omapi_object_reference (sp, (omapi_object_t *)s, MDL);
	}

	/* If we get to here without finding a lease, no valid key was
	   specified. */
	if (!*sp)
		return DHCP_R_NOKEYS;
	return ISC_R_SUCCESS;
}

isc_result_t dhcp_failover_state_create (omapi_object_t **sp,
					 omapi_object_t *id)
{
	return ISC_R_NOTIMPLEMENTED;
}

isc_result_t dhcp_failover_state_remove (omapi_object_t *sp,
					 omapi_object_t *id)
{
	return ISC_R_NOTIMPLEMENTED;
}

int dhcp_failover_state_match (dhcp_failover_state_t *state,
			       u_int8_t *addr, unsigned addrlen)
{
	struct data_string ds;
	int i;
	
	memset (&ds, 0, sizeof ds);
	if (evaluate_option_cache (&ds, (struct packet *)0,
				   (struct lease *)0,
				   (struct client_state *)0,
				   (struct option_state *)0,
				   (struct option_state *)0,
				   &global_scope,
				   state -> partner.address, MDL)) {
		for (i = 0; i + addrlen - 1 < ds.len; i += addrlen) {
			if (!memcmp (&ds.data [i],
				     addr, addrlen)) {
				data_string_forget (&ds, MDL);
				return 1;
			}
		}
		data_string_forget (&ds, MDL);
	}
	return 0;
}

int
dhcp_failover_state_match_by_name(state, name)
	dhcp_failover_state_t *state;
	failover_option_t *name;
{
	if ((strlen(state->name) == name->count) &&
	    (memcmp(state->name, name->data, name->count) == 0))
		return 1;

	return 0;
}

const char *dhcp_failover_reject_reason_print (int reason)
{
    static char resbuf[sizeof("Undefined-255: This reason code is not defined "
			      "in the protocol standard.")];

    if ((reason > 0xff) || (reason < 0))
	return "Reason code out of range.";

    switch (reason) {
      case FTR_ILLEGAL_IP_ADDR:
	return "Illegal IP address (not part of any address pool).";

      case FTR_FATAL_CONFLICT:
	return "Fatal conflict exists: address in use by other client.";

      case FTR_MISSING_BINDINFO:
	return "Missing binding information.";

      case FTR_TIMEMISMATCH:
	return "Connection rejected, time mismatch too great.";

      case FTR_INVALID_MCLT:
	return "Connection rejected, invalid MCLT.";

      case FTR_MISC_REJECT:
	return "Connection rejected, unknown reason.";

      case FTR_DUP_CONNECTION:
	return "Connection rejected, duplicate connection.";

      case FTR_INVALID_PARTNER:
	return "Connection rejected, invalid failover partner.";

      case FTR_TLS_UNSUPPORTED:
	return "TLS not supported.";

      case FTR_TLS_UNCONFIGURED:
	return "TLS supported but not configured.";

      case FTR_TLS_REQUIRED:
	return "TLS required but not supported by partner.";

      case FTR_DIGEST_UNSUPPORTED:
	return "Message digest not supported.";

      case FTR_DIGEST_UNCONFIGURED:
	return "Message digest not configured.";

      case FTR_VERSION_MISMATCH:
	return "Protocol version mismatch.";

      case FTR_OUTDATED_BIND_INFO:
	return "Outdated binding information.";

      case FTR_LESS_CRIT_BIND_INFO:
	return "Less critical binding information.";

      case FTR_NO_TRAFFIC:
	return "No traffic within sufficient time.";

      case FTR_HBA_CONFLICT:
	return "Hash bucket assignment conflict.";

      case FTR_IP_NOT_RESERVED:
	return "IP not reserved on this server.";

      case FTR_IP_DIGEST_FAILURE:
	return "Message digest failed to compare.";

      case FTR_IP_MISSING_DIGEST:
	return "Missing message digest.";

      case FTR_UNKNOWN:
	return "Unknown Error.";

      default:
	sprintf(resbuf, "Undefined-%d: This reason code is not defined in the "
			"protocol standard.", reason);
	return resbuf;
    }
}

const char *dhcp_failover_state_name_print (enum failover_state state)
{
	switch (state) {
	      default:
	      case unknown_state:
		return "unknown-state";

	      case partner_down:
		return "partner-down";

	      case normal:
		return "normal";

	      case conflict_done:
		return "conflict-done";

	      case communications_interrupted:
		return "communications-interrupted";

	      case resolution_interrupted:
		return "resolution-interrupted";

	      case potential_conflict:
		return "potential-conflict";

	      case recover:
		return "recover";

	      case recover_done:
		return "recover-done";

	      case recover_wait:
		return "recover-wait";

	      case shut_down:
		return "shutdown";

	      case paused:
		return "paused";

	      case startup:
		return "startup";
	}
}

const char *dhcp_failover_message_name (unsigned type)
{
	static char messbuf[sizeof("unknown-message-255")];

	if (type > 0xff)
		return "invalid-message";

	switch (type) {
	      case FTM_POOLREQ:
		return "pool-request";
		
	      case FTM_POOLRESP:
		return "pool-response";

	      case FTM_BNDUPD:
		return "bind-update";

	      case FTM_BNDACK:
		return "bind-ack";

	      case FTM_CONNECT:
		return "connect";

	      case FTM_CONNECTACK:
		return "connect-ack";

	      case FTM_UPDREQ:
		return "update-request";

	      case FTM_UPDDONE:
		return "update-done";

	      case FTM_UPDREQALL:
		return "update-request-all";

	      case FTM_STATE:
		return "state";

	      case FTM_CONTACT:
		return "contact";

	      case FTM_DISCONNECT:
		return "disconnect";

	      default:
		sprintf(messbuf, "unknown-message-%u", type);
		return messbuf;
	}
}

const char *dhcp_failover_option_name (unsigned type)
{
	static char optbuf[sizeof("unknown-option-65535")];

	if (type > 0xffff)
		return "invalid-option";

	switch (type) {
	    case FTO_ADDRESSES_TRANSFERRED:
		return "addresses-transferred";

	    case FTO_ASSIGNED_IP_ADDRESS:
		return "assigned-ip-address";

	    case FTO_BINDING_STATUS:
		return "binding-status";

	    case FTO_CLIENT_IDENTIFIER:
		return "client-identifier";

	    case FTO_CHADDR:
		return "chaddr";

	    case FTO_CLTT:
		return "cltt";

	    case FTO_DDNS:
		return "ddns";

	    case FTO_DELAYED_SERVICE:
		return "delayed-service";

	    case FTO_HBA:
		return "hba";

	    case FTO_IP_FLAGS:
		return "ip-flags";

	    case FTO_LEASE_EXPIRY:
		return "lease-expiry";

	    case FTO_MAX_UNACKED:
		return "max-unacked";

	    case FTO_MCLT:
		return "mclt";

	    case FTO_MESSAGE:
		return "message";

	    case FTO_MESSAGE_DIGEST:
		return "message-digest";

	    case FTO_POTENTIAL_EXPIRY:
		return "potential-expiry";

	    case FTO_PROTOCOL_VERSION:
		return "protocol-version";

	    case FTO_RECEIVE_TIMER:
		return "receive-timer";

	    case FTO_REJECT_REASON:
		return "reject-reason";

	    case FTO_RELATIONSHIP_NAME:
		return "relationship-name";

	    case FTO_REPLY_OPTIONS:
		return "reply-options";

	    case FTO_REQUEST_OPTIONS:
		return "request-options";

	    case FTO_SERVER_FLAGS:
		return "server-flags";

	    case FTO_SERVER_STATE:
		return "server-state";

	    case FTO_STOS:
		return "stos";

	    case FTO_TLS_REPLY:
		return "tls-reply";

	    case FTO_TLS_REQUEST:
		return "tls-request";

	    case FTO_VENDOR_CLASS:
		return "vendor-class";

	    case FTO_VENDOR_OPTIONS:
		return "vendor-options";

	    default:
		sprintf(optbuf, "unknown-option-%u", type);
		return optbuf;
	}
}

failover_option_t *dhcp_failover_option_printf (unsigned code,
						char *obuf,
						unsigned *obufix,
						unsigned obufmax,
						const char *fmt, ...)
{
	va_list va;
	char tbuf [256];

	/* %Audit% Truncation causes panic. %2004.06.17,Revisit%
	 * It is unclear what the effects of truncation here are, or
	 * how that condition should be handled.  It seems that this
	 * function is used for formatting messages in the failover
	 * command channel.  For now the safest thing is for
	 * overflow-truncation to cause a fatal log.
	 */
	va_start (va, fmt);
	if (vsnprintf (tbuf, sizeof tbuf, fmt, va) >= sizeof tbuf)
		log_fatal ("%s: vsnprintf would truncate",
				"dhcp_failover_make_option");
	va_end (va);

	return dhcp_failover_make_option (code, obuf, obufix, obufmax,
					  strlen (tbuf), tbuf);
}

failover_option_t *dhcp_failover_make_option (unsigned code,
					      char *obuf, unsigned *obufix,
					      unsigned obufmax, ...)
{
	va_list va;
	struct failover_option_info *info;
	int i;
	unsigned size, count;
	unsigned val;
	u_int8_t *iaddr;
	unsigned ilen = 0;
	u_int8_t *bval;
	char *txt = NULL;
#if defined (DEBUG_FAILOVER_MESSAGES)
	char tbuf [256];
#endif

	/* Note that the failover_option structure is used differently on
	   input than on output - on input, count is an element count, and
	   on output it's the number of bytes total in the option, including
	   the option code and option length. */
	failover_option_t option, *op;


	/* Bogus option code? */
	if (code < 1 || code > FTO_MAX || ft_options [code].type == FT_UNDEF) {
		return &null_failover_option;
	}
	info = &ft_options [code];
		
	va_start (va, obufmax);

	/* Get the number of elements and the size of the buffer we need
	   to allocate. */
	if (info -> type == FT_DDNS || info -> type == FT_DDNS1) {
		count = info -> type == FT_DDNS ? 1 : 2;
		size = va_arg (va, int) + count;
	} else {
		/* Find out how many items in this list. */
		if (info -> num_present)
			count = info -> num_present;
		else
			count = va_arg (va, int);

		/* Figure out size. */
		switch (info -> type) {
		      case FT_UINT8:
		      case FT_BYTES:
		      case FT_DIGEST:
			size = count;
			break;

		      case FT_TEXT_OR_BYTES:
		      case FT_TEXT:
			txt = va_arg (va, char *);
			size = count;
			break;

		      case FT_IPADDR:
			ilen = va_arg (va, unsigned);
			size = count * ilen;
			break;

		      case FT_UINT32:
			size = count * 4;
			break;

		      case FT_UINT16:
			size = count * 2;
			break;

		      default:
			/* shouldn't get here. */
			log_fatal ("bogus type in failover_make_option: %d",
				   info -> type);
			return &null_failover_option;
		}
	}
	
	size += 4;

	/* Allocate a buffer for the option. */
	option.count = size;
	option.data = dmalloc (option.count, MDL);
	if (!option.data) {
		va_end (va);
		return &null_failover_option;
	}

	/* Put in the option code and option length. */
	putUShort (option.data, code);
	putUShort (&option.data [2], size - 4);

#if defined (DEBUG_FAILOVER_MESSAGES)	
	/* %Audit% Truncation causes panic. %2004.06.17,Revisit%
	 * It is unclear what the effects of truncation here are, or
	 * how that condition should be handled.  It seems that this
	 * message may be sent over the failover command channel.
	 * For now the safest thing is for overflow-truncation to cause
	 * a fatal log.
	 */
	if (snprintf (tbuf, sizeof tbuf, " (%s<%d>", info -> name,
			option.count) >= sizeof tbuf)
		log_fatal ("dhcp_failover_make_option: tbuf overflow");
	failover_print (obuf, obufix, obufmax, tbuf);
#endif

	/* Now put in the data. */
	switch (info -> type) {
	      case FT_UINT8:
		for (i = 0; i < count; i++) {
			val = va_arg (va, unsigned);
#if defined (DEBUG_FAILOVER_MESSAGES)
			/* %Audit% Cannot exceed 24 bytes. %2004.06.17,Safe% */
			sprintf (tbuf, " %d", val);
			failover_print (obuf, obufix, obufmax, tbuf);
#endif
			option.data [i + 4] = val;
		}
		break;

	      case FT_IPADDR:
		for (i = 0; i < count; i++) {
			iaddr = va_arg (va, u_int8_t *);
			if (ilen != 4) {
				dfree (option.data, MDL);
				log_error ("IP addrlen=%d, should be 4.",
					   ilen);
				va_end (va);
				return &null_failover_option;
			}
				
#if defined (DEBUG_FAILOVER_MESSAGES)
			/*%Audit% Cannot exceed 17 bytes.  %2004.06.17,Safe%*/
			sprintf (tbuf, " %u.%u.%u.%u",
				  iaddr [0], iaddr [1], iaddr [2], iaddr [3]);
			failover_print (obuf, obufix, obufmax, tbuf);
#endif
			memcpy (&option.data [4 + i * ilen], iaddr, ilen);
		}
		break;

	      case FT_UINT32:
		for (i = 0; i < count; i++) {
			val = va_arg (va, unsigned);
#if defined (DEBUG_FAILOVER_MESSAGES)
			/*%Audit% Cannot exceed 24 bytes.  %2004.06.17,Safe%*/
			sprintf (tbuf, " %d", val);
			failover_print (obuf, obufix, obufmax, tbuf);
#endif
			putULong (&option.data [4 + i * 4], val);
		}
		break;

	      case FT_BYTES:
	      case FT_DIGEST:
		bval = va_arg (va, u_int8_t *);
#if defined (DEBUG_FAILOVER_MESSAGES)
		for (i = 0; i < count; i++) {
			/* 23 bytes plus nul, safe. */
			sprintf (tbuf, " %d", bval [i]);
			failover_print (obuf, obufix, obufmax, tbuf);
		}
#endif
		memcpy (&option.data [4], bval, count);
		break;

		/* On output, TEXT_OR_BYTES is _always_ text, and always NUL
		   terminated.  Note that the caller should be careful not
		   to provide a format and data that amount to more than 256
		   bytes of data, since it will cause a fatal error. */
	      case FT_TEXT_OR_BYTES:
	      case FT_TEXT:
#if defined (DEBUG_FAILOVER_MESSAGES)
		/* %Audit% Truncation causes panic. %2004.06.17,Revisit%
		 * It is unclear what the effects of truncation here are, or
		 * how that condition should be handled.  It seems that this
		 * function is used for formatting messages in the failover
		 * command channel.  For now the safest thing is for
		 * overflow-truncation to cause a fatal log.
		 */
		if (snprintf (tbuf, sizeof tbuf, "\"%s\"", txt) >= sizeof tbuf)
			log_fatal ("dhcp_failover_make_option: tbuf overflow");
		failover_print (obuf, obufix, obufmax, tbuf);
#endif
		memcpy (&option.data [4], txt, count);
		break;

	      case FT_DDNS:
	      case FT_DDNS1:
		option.data [4] = va_arg (va, unsigned);
		if (count == 2)
			option.data [5] = va_arg (va, unsigned);
		bval = va_arg (va, u_int8_t *);
		memcpy (&option.data [4 + count], bval, size - count - 4);
#if defined (DEBUG_FAILOVER_MESSAGES)
		for (i = 4; i < size; i++) {
			/*%Audit% Cannot exceed 24 bytes. %2004.06.17,Safe%*/
			sprintf (tbuf, " %d", option.data [i]);
			failover_print (obuf, obufix, obufmax, tbuf);
		}
#endif
		break;

	      case FT_UINT16:
		for (i = 0; i < count; i++) {
			val = va_arg (va, u_int32_t);
#if defined (DEBUG_FAILOVER_MESSAGES)
			/*%Audit% Cannot exceed 24 bytes. %2004.06.17,Safe%*/
			sprintf (tbuf, " %d", val);
			failover_print (obuf, obufix, obufmax, tbuf);
#endif
			putUShort (&option.data [4 + i * 2], val);
		}
		break;

	      case FT_UNDEF:
	      default:
		break;
	}

#if defined DEBUG_FAILOVER_MESSAGES
	failover_print (obuf, obufix, obufmax, ")");
#endif
	va_end (va);

	/* Now allocate a place to store what we just set up. */
	op = dmalloc (sizeof (failover_option_t), MDL);
	if (!op) {
		dfree (option.data, MDL);
		return &null_failover_option;
	}

	*op = option;
	return op;
}

/* Send a failover message header. */

isc_result_t dhcp_failover_put_message (dhcp_failover_link_t *link,
					omapi_object_t *connection,
					int msg_type, u_int32_t xid, ...)
{
	unsigned size = 0;
	int bad_option = 0;
	int opix = 0;
	va_list list;
	failover_option_t *option;
	unsigned char *opbuf;
	isc_result_t status = ISC_R_SUCCESS;
	unsigned char cbuf;
	struct timeval tv;

	/* Run through the argument list once to compute the length of
	   the option portion of the message. */
	va_start (list, xid);
	while ((option = va_arg (list, failover_option_t *))) {
		if (option != &skip_failover_option)
			size += option -> count;
		if (option == &null_failover_option)
			bad_option = 1;
	}
	va_end (list);

	/* Allocate an option buffer, unless we got an error. */
	if (!bad_option && size) {
		opbuf = dmalloc (size, MDL);
		if (!opbuf)
			status = ISC_R_NOMEMORY;
	} else
		opbuf = (unsigned char *)0;

	va_start (list, xid);
	while ((option = va_arg (list, failover_option_t *))) {
		if (option == &skip_failover_option)
		    continue;
		if (!bad_option && opbuf)
			memcpy (&opbuf [opix],
				option -> data, option -> count);
		if (option != &null_failover_option &&
		    option != &skip_failover_option) {
			opix += option -> count;
			dfree (option -> data, MDL);
			dfree (option, MDL);
		}
	}
	va_end(list);

	if (bad_option)
		return DHCP_R_INVALIDARG;

	/* Now send the message header. */

	/* Message length. */
	status = omapi_connection_put_uint16 (connection, size + 12);
	if (status != ISC_R_SUCCESS)
		goto err;

	/* Message type. */
	cbuf = msg_type;
	status = omapi_connection_copyin (connection, &cbuf, 1);
	if (status != ISC_R_SUCCESS)
		goto err;

	/* Payload offset. */
	cbuf = 12;
	status = omapi_connection_copyin (connection, &cbuf, 1);
	if (status != ISC_R_SUCCESS)
		goto err;

	/* Current time. */
	status = omapi_connection_put_uint32 (connection, (u_int32_t)cur_time);
	if (status != ISC_R_SUCCESS)
		goto err;

	/* Transaction ID. */
	status = omapi_connection_put_uint32(connection, xid);
	if (status != ISC_R_SUCCESS)
		goto err;

	/* Payload. */
	if (opbuf) {
		status = omapi_connection_copyin (connection, opbuf, size);
		if (status != ISC_R_SUCCESS)
			goto err;
		dfree (opbuf, MDL);
	}
	if (link -> state_object &&
	    link -> state_object -> link_to_peer == link) {
#if defined (DEBUG_FAILOVER_CONTACT_TIMING)
		log_info ("add_timeout +%d %s",
			  (int)(link -> state_object ->
				partner.max_response_delay) / 3,
			  "dhcp_failover_send_contact");
#endif
		tv . tv_sec = cur_time +
			(int)(link -> state_object ->
			      partner.max_response_delay) / 3;
		tv . tv_usec = 0;
		add_timeout (&tv,
			     dhcp_failover_send_contact, link -> state_object,
			     (tvref_t)dhcp_failover_state_reference,
			     (tvunref_t)dhcp_failover_state_dereference);
	}
	return status;

      err:
	if (opbuf)
		dfree (opbuf, MDL);
	log_info ("dhcp_failover_put_message: something went wrong.");
	omapi_disconnect (connection, 1);
	return status;
}

void dhcp_failover_timeout (void *vstate)
{
	dhcp_failover_state_t *state = vstate;
	dhcp_failover_link_t *link;

#if defined (DEBUG_FAILOVER_TIMING)
	log_info ("dhcp_failover_timeout");
#endif

	if (!state || state -> type != dhcp_type_failover_state)
		return;
	link = state -> link_to_peer;
	if (!link ||
	    !link -> outer ||
	    link -> outer -> type != omapi_type_connection)
		return;

	log_error ("timeout waiting for failover peer %s", state -> name);

	/* If we haven't gotten a timely response, blow away the connection.
	   This will cause the state to change automatically. */
	omapi_disconnect (link -> outer, 1);
}

void dhcp_failover_send_contact (void *vstate)
{
	dhcp_failover_state_t *state = vstate;
	dhcp_failover_link_t *link;
	isc_result_t status;

#if defined(DEBUG_FAILOVER_MESSAGES) && \
    defined(DEBUG_FAILOVER_CONTACT_MESSAGES)
	char obuf [64];
	unsigned obufix = 0;

	failover_print(obuf, &obufix, sizeof(obuf), "(contact");
#endif

#if defined (DEBUG_FAILOVER_CONTACT_TIMING)
	log_info ("dhcp_failover_send_contact");
#endif

	if (!state || state -> type != dhcp_type_failover_state)
		return;
	link = state -> link_to_peer;
	if (!link ||
	    !link -> outer ||
	    link -> outer -> type != omapi_type_connection)
		return;

	status = (dhcp_failover_put_message
		  (link, link -> outer,
		   FTM_CONTACT, link->xid++,
		   (failover_option_t *)0));

#if defined(DEBUG_FAILOVER_MESSAGES) && \
    defined(DEBUG_FAILOVER_CONTACT_MESSAGES)
	if (status != ISC_R_SUCCESS)
		failover_print(obuf, &obufix, sizeof(obuf), " (failed)");
	failover_print(obuf, &obufix, sizeof(obuf), ")");
	if (obufix) {
		log_debug ("%s", obuf);
	}
#else
        IGNORE_UNUSED(status);
#endif
	return;
}

isc_result_t dhcp_failover_send_state (dhcp_failover_state_t *state)
{
	dhcp_failover_link_t *link;
	isc_result_t status;

#if defined (DEBUG_FAILOVER_MESSAGES)	
	char obuf [64];
	unsigned obufix = 0;
	
# define FMA obuf, &obufix, sizeof obuf
	failover_print (FMA, "(state");
#else
# define FMA (char *)0, (unsigned *)0, 0
#endif

	if (!state || state -> type != dhcp_type_failover_state)
		return DHCP_R_INVALIDARG;
	link = state -> link_to_peer;
	if (!link ||
	    !link -> outer ||
	    link -> outer -> type != omapi_type_connection)
		return DHCP_R_INVALIDARG;

	status = (dhcp_failover_put_message
		  (link, link -> outer,
		   FTM_STATE, link->xid++,
		   dhcp_failover_make_option (FTO_SERVER_STATE, FMA,
					      (state -> me.state == startup
					       ? state -> saved_state
					       : state -> me.state)),
		   dhcp_failover_make_option
		   (FTO_SERVER_FLAGS, FMA,
		    (state -> service_state == service_startup
		     ? FTF_SERVER_STARTUP : 0)),
		   dhcp_failover_make_option (FTO_STOS, FMA, state -> me.stos),
		   (failover_option_t *)0));

#if defined (DEBUG_FAILOVER_MESSAGES)
	if (status != ISC_R_SUCCESS)
		failover_print (FMA, " (failed)");
	failover_print (FMA, ")");
	if (obufix) {
		log_debug ("%s", obuf);
	}
#else
        IGNORE_UNUSED(status);
#endif
	return ISC_R_SUCCESS;
}

/* Send a connect message. */

isc_result_t dhcp_failover_send_connect (omapi_object_t *l)
{
	dhcp_failover_link_t *link;
	dhcp_failover_state_t *state;
	isc_result_t status;
#if defined (DEBUG_FAILOVER_MESSAGES)	
	char obuf [64];
	unsigned obufix = 0;
	
# define FMA obuf, &obufix, sizeof obuf
	failover_print (FMA, "(connect");
#else
# define FMA (char *)0, (unsigned *)0, 0
#endif

	if (!l || l -> type != dhcp_type_failover_link)
		return DHCP_R_INVALIDARG;
	link = (dhcp_failover_link_t *)l;
	state = link -> state_object;
	if (!l -> outer || l -> outer -> type != omapi_type_connection)
		return DHCP_R_INVALIDARG;

	status =
	    (dhcp_failover_put_message
	     (link, l -> outer,
	      FTM_CONNECT, link->xid++,
	      dhcp_failover_make_option(FTO_RELATIONSHIP_NAME, FMA,
					strlen(state->name), state->name),
	      dhcp_failover_make_option (FTO_MAX_UNACKED, FMA,
					 state -> me.max_flying_updates),
	      dhcp_failover_make_option (FTO_RECEIVE_TIMER, FMA,
					 state -> me.max_response_delay),
	      dhcp_failover_option_printf(FTO_VENDOR_CLASS, FMA,
					  "isc-%s", PACKAGE_VERSION),
	      dhcp_failover_make_option (FTO_PROTOCOL_VERSION, FMA,
					 DHCP_FAILOVER_VERSION),
	      dhcp_failover_make_option (FTO_TLS_REQUEST, FMA,
					 0, 0),
	      dhcp_failover_make_option (FTO_MCLT, FMA,
					 state -> mclt),
	      (state -> hba
	       ? dhcp_failover_make_option (FTO_HBA, FMA, 32, state -> hba)
	       : &skip_failover_option),
	      (failover_option_t *)0));
	
#if defined (DEBUG_FAILOVER_MESSAGES)
	if (status != ISC_R_SUCCESS)
		failover_print (FMA, " (failed)");
	failover_print (FMA, ")");
	if (obufix) {
		log_debug ("%s", obuf);
	}
#endif
	return status;
}

isc_result_t dhcp_failover_send_connectack (omapi_object_t *l,
					    dhcp_failover_state_t *state,
					    int reason, const char *errmsg)
{
	dhcp_failover_link_t *link;
	isc_result_t status;
#if defined (DEBUG_FAILOVER_MESSAGES)	
	char obuf [64];
	unsigned obufix = 0;
	
# define FMA obuf, &obufix, sizeof obuf
	failover_print (FMA, "(connectack");
#else
# define FMA (char *)0, (unsigned *)0, 0
#endif

	if (!l || l -> type != dhcp_type_failover_link)
		return DHCP_R_INVALIDARG;
	link = (dhcp_failover_link_t *)l;
	if (!l -> outer || l -> outer -> type != omapi_type_connection)
		return DHCP_R_INVALIDARG;

	status =
	    (dhcp_failover_put_message
	     (link, l -> outer,
	      FTM_CONNECTACK, link->imsg->xid,
	      state
	       ? dhcp_failover_make_option(FTO_RELATIONSHIP_NAME, FMA,
					   strlen(state->name), state->name)
	       : (link->imsg->options_present & FTB_RELATIONSHIP_NAME)
		  ? dhcp_failover_make_option(FTO_RELATIONSHIP_NAME, FMA,
					      link->imsg->relationship_name.count,
					      link->imsg->relationship_name.data)
		  : &skip_failover_option,
	      state
	       ? dhcp_failover_make_option (FTO_MAX_UNACKED, FMA,
					    state -> me.max_flying_updates)
	       : &skip_failover_option,
	      state
	       ? dhcp_failover_make_option (FTO_RECEIVE_TIMER, FMA,
					    state -> me.max_response_delay)
	       : &skip_failover_option,
	      dhcp_failover_option_printf(FTO_VENDOR_CLASS, FMA,
					  "isc-%s", PACKAGE_VERSION),
	      dhcp_failover_make_option (FTO_PROTOCOL_VERSION, FMA,
					 DHCP_FAILOVER_VERSION),
	      (link->imsg->options_present & FTB_TLS_REQUEST)
	       ? dhcp_failover_make_option(FTO_TLS_REPLY, FMA,
					   0, 0)
	       : &skip_failover_option,
	      reason
	       ? dhcp_failover_make_option (FTO_REJECT_REASON,
					    FMA, reason)
	       : &skip_failover_option,
	      (reason && errmsg)
	       ? dhcp_failover_make_option (FTO_MESSAGE, FMA,
					    strlen (errmsg), errmsg)
	       : &skip_failover_option,
	      (failover_option_t *)0));

#if defined (DEBUG_FAILOVER_MESSAGES)
	if (status != ISC_R_SUCCESS)
		failover_print (FMA, " (failed)");
	failover_print (FMA, ")");
	if (obufix) {
		log_debug ("%s", obuf);
	}
#endif
	return status;
}

isc_result_t dhcp_failover_send_disconnect (omapi_object_t *l,
					    int reason,
					    const char *message)
{
	dhcp_failover_link_t *link;
	isc_result_t status;
#if defined (DEBUG_FAILOVER_MESSAGES)	
	char obuf [64];
	unsigned obufix = 0;
	
# define FMA obuf, &obufix, sizeof obuf
	failover_print (FMA, "(disconnect");
#else
# define FMA (char *)0, (unsigned *)0, 0
#endif

	if (!l || l -> type != dhcp_type_failover_link)
		return DHCP_R_INVALIDARG;
	link = (dhcp_failover_link_t *)l;
	if (!l -> outer || l -> outer -> type != omapi_type_connection)
		return DHCP_R_INVALIDARG;

	if (!message && reason)
		message = dhcp_failover_reject_reason_print (reason);

	status = (dhcp_failover_put_message
		  (link, l -> outer,
		   FTM_DISCONNECT, link->xid++,
		   dhcp_failover_make_option (FTO_REJECT_REASON,
					      FMA, reason),
		   (message
		    ? dhcp_failover_make_option (FTO_MESSAGE, FMA,
						 strlen (message), message)
		    : &skip_failover_option),
		   (failover_option_t *)0));

#if defined (DEBUG_FAILOVER_MESSAGES)
	if (status != ISC_R_SUCCESS)
		failover_print (FMA, " (failed)");
	failover_print (FMA, ")");
	if (obufix) {
		log_debug ("%s", obuf);
	}
#endif
	return status;
}

/* Send a Bind Update message. */

isc_result_t dhcp_failover_send_bind_update (dhcp_failover_state_t *state,
					     struct lease *lease)
{
	dhcp_failover_link_t *link;
	isc_result_t status;
	int flags = 0;
	binding_state_t transmit_state;
#if defined (DEBUG_FAILOVER_MESSAGES)	
	char obuf [64];
	unsigned obufix = 0;
	
# define FMA obuf, &obufix, sizeof obuf
	failover_print (FMA, "(bndupd");
#else
# define FMA (char *)0, (unsigned *)0, 0
#endif

	if (!state -> link_to_peer ||
	    state -> link_to_peer -> type != dhcp_type_failover_link)
		return DHCP_R_INVALIDARG;
	link = (dhcp_failover_link_t *)state -> link_to_peer;

	if (!link -> outer || link -> outer -> type != omapi_type_connection)
		return DHCP_R_INVALIDARG;

	transmit_state = lease->desired_binding_state;
	if (lease->flags & RESERVED_LEASE) {
		/* If we are listing an allocable (not yet ACTIVE etc) lease
		 * as reserved, toggle to the peer's 'free state', per the
		 * draft.  This gives the peer permission to alloc it to the
		 * chaddr/uid-named client.
		 */
		if ((state->i_am == primary) && (transmit_state == FTS_FREE))
			transmit_state = FTS_BACKUP;
		else if ((state->i_am == secondary) &&
			 (transmit_state == FTS_BACKUP))
			transmit_state = FTS_FREE;

		flags |= FTF_IP_FLAG_RESERVE;
	}
	if (lease->flags & BOOTP_LEASE)
		flags |= FTF_IP_FLAG_BOOTP;

	/* last_xid == 0 is illegal, seek past zero if we hit it. */
	if (link->xid == 0)
		link->xid = 1;

	lease->last_xid = link->xid++;

	/*
	 * Our very next action is to transmit a binding update relating to
	 * this lease over the wire, and although there is a BNDACK, there is
	 * no BNDACKACK or BNDACKACKACK...the basic issue as we send a BNDUPD,
	 * we may not receive a BNDACK.  This non-reception does not imply the
	 * peer did not receive and process the BNDUPD.  So at this point, we
	 * must divest any state that would be dangerous to retain under the
	 * impression the peer has been updated.  Normally state changes like
	 * this are processed in supersede_lease(), but in this case we need a
	 * very late binding.
	 *
	 * In failover rules, a server is permitted to work forward in certain
	 * directions from a given lease's state; active leases may be
	 * extended, so forth.  There is an 'optimization' in the failover
	 * draft that permits a server to 'rewind' any work they have not
	 * informed the peer.  Since we can't know if the peer received our
	 * update but was unable to acknowledge it, we make this change on
	 * transmit rather than upon receiving the acknowledgement.
	 *
	 * XXX: Frequent lease commits are undesirable.  This should hopefully
	 * only trigger when a server is sending a lease /state change/, and
	 * not merely an update such as with a renewal.
	 */
	if (lease->rewind_binding_state != lease->binding_state) {
		lease->rewind_binding_state = lease->binding_state;

		write_lease(lease);
		commit_leases();
	}

	/* Send the update. */
	status = (dhcp_failover_put_message
		  (link, link -> outer,
		   FTM_BNDUPD, lease->last_xid,
		   dhcp_failover_make_option (FTO_ASSIGNED_IP_ADDRESS, FMA,
					      lease -> ip_addr.len,
					      lease -> ip_addr.iabuf),
		   dhcp_failover_make_option (FTO_BINDING_STATUS, FMA,
					      lease -> desired_binding_state),
		   lease -> uid_len
		   ? dhcp_failover_make_option (FTO_CLIENT_IDENTIFIER, FMA,
						lease -> uid_len,
						lease -> uid)
		   : &skip_failover_option,
		   lease -> hardware_addr.hlen
		   ? dhcp_failover_make_option (FTO_CHADDR, FMA,
						lease -> hardware_addr.hlen,
						lease -> hardware_addr.hbuf)
		   : &skip_failover_option,
		   dhcp_failover_make_option (FTO_LEASE_EXPIRY, FMA,
					      lease -> ends),
		   dhcp_failover_make_option (FTO_POTENTIAL_EXPIRY, FMA,
					      lease -> tstp),
		   dhcp_failover_make_option (FTO_STOS, FMA,
					      lease -> starts),
		   (lease->cltt != 0) ? 
			dhcp_failover_make_option(FTO_CLTT, FMA, lease->cltt) :
			&skip_failover_option, /* No CLTT */
		   flags ? dhcp_failover_make_option(FTO_IP_FLAGS, FMA,
						     flags) :
			   &skip_failover_option, /* No IP_FLAGS */
		   &skip_failover_option,	/* XXX DDNS */
		   &skip_failover_option,	/* XXX request options */
		   &skip_failover_option,	/* XXX reply options */
		   (failover_option_t *)0));

#if defined (DEBUG_FAILOVER_MESSAGES)
	if (status != ISC_R_SUCCESS)
		failover_print (FMA, " (failed)");
	failover_print (FMA, ")");
	if (obufix) {
		log_debug ("%s", obuf);
	}
#endif
	return status;
}

/* Send a Bind ACK message. */

isc_result_t dhcp_failover_send_bind_ack (dhcp_failover_state_t *state,
					  failover_message_t *msg,
					  int reason, const char *message)
{
	dhcp_failover_link_t *link;
	isc_result_t status;
#if defined (DEBUG_FAILOVER_MESSAGES)	
	char obuf [64];
	unsigned obufix = 0;
	
# define FMA obuf, &obufix, sizeof obuf
	failover_print (FMA, "(bndack");
#else
# define FMA (char *)0, (unsigned *)0, 0
#endif

	if (!state -> link_to_peer ||
	    state -> link_to_peer -> type != dhcp_type_failover_link)
		return DHCP_R_INVALIDARG;
	link = (dhcp_failover_link_t *)state -> link_to_peer;

	if (!link -> outer || link -> outer -> type != omapi_type_connection)
		return DHCP_R_INVALIDARG;

	if (!message && reason)
		message = dhcp_failover_reject_reason_print (reason);

	/* Send the update. */
	status = (dhcp_failover_put_message
		  (link, link -> outer,
		   FTM_BNDACK, msg->xid,
		   dhcp_failover_make_option (FTO_ASSIGNED_IP_ADDRESS, FMA,
					      sizeof msg -> assigned_addr,
					      &msg -> assigned_addr),
#ifdef DO_BNDACK_SHOULD_NOT
		   dhcp_failover_make_option (FTO_BINDING_STATUS, FMA,
					      msg -> binding_status),
		   (msg -> options_present & FTB_CLIENT_IDENTIFIER)
		   ? dhcp_failover_make_option (FTO_CLIENT_IDENTIFIER, FMA,
						msg -> client_identifier.count,
						msg -> client_identifier.data)
		   : &skip_failover_option,
		   (msg -> options_present & FTB_CHADDR)
		   ? dhcp_failover_make_option (FTO_CHADDR, FMA,
						msg -> chaddr.count,
						msg -> chaddr.data)
		   : &skip_failover_option,
		   dhcp_failover_make_option (FTO_LEASE_EXPIRY, FMA,
					      msg -> expiry),
		   dhcp_failover_make_option (FTO_POTENTIAL_EXPIRY, FMA,
					      msg -> potential_expiry),
		   dhcp_failover_make_option (FTO_STOS, FMA,
					      msg -> stos),
		   (msg->options_present & FTB_CLTT) ?
			dhcp_failover_make_option(FTO_CLTT, FMA, msg->cltt) :
			&skip_failover_option, /* No CLTT in the msg to ack. */
		   ((msg->options_present & FTB_IP_FLAGS) && msg->ip_flags) ? 
			dhcp_failover_make_option(FTO_IP_FLAGS, FMA,
						  msg->ip_flags)
			: &skip_failover_option,
#endif /* DO_BNDACK_SHOULD_NOT */
		   reason
		    ? dhcp_failover_make_option(FTO_REJECT_REASON, FMA, reason)
		    : &skip_failover_option,
		   (reason && message)
		    ? dhcp_failover_make_option (FTO_MESSAGE, FMA,
						 strlen (message), message)
		    : &skip_failover_option,
#ifdef DO_BNDACK_SHOULD_NOT
		   &skip_failover_option,	/* XXX DDNS */
		   &skip_failover_option,	/* XXX request options */
		   &skip_failover_option,	/* XXX reply options */
#endif /* DO_BNDACK_SHOULD_NOT */
		   (failover_option_t *)0));

#if defined (DEBUG_FAILOVER_MESSAGES)
	if (status != ISC_R_SUCCESS)
		failover_print (FMA, " (failed)");
	failover_print (FMA, ")");
	if (obufix) {
		log_debug ("%s", obuf);
	}
#endif
	return status;
}

isc_result_t dhcp_failover_send_poolreq (dhcp_failover_state_t *state)
{
	dhcp_failover_link_t *link;
	isc_result_t status;
#if defined (DEBUG_FAILOVER_MESSAGES)	
	char obuf [64];
	unsigned obufix = 0;
	
# define FMA obuf, &obufix, sizeof obuf
	failover_print (FMA, "(poolreq");
#else
# define FMA (char *)0, (unsigned *)0, 0
#endif

	if (!state -> link_to_peer ||
	    state -> link_to_peer -> type != dhcp_type_failover_link)
		return DHCP_R_INVALIDARG;
	link = (dhcp_failover_link_t *)state -> link_to_peer;

	if (!link -> outer || link -> outer -> type != omapi_type_connection)
		return DHCP_R_INVALIDARG;

	status = (dhcp_failover_put_message
		  (link, link -> outer,
		   FTM_POOLREQ, link->xid++,
		   (failover_option_t *)0));

#if defined (DEBUG_FAILOVER_MESSAGES)
	if (status != ISC_R_SUCCESS)
		failover_print (FMA, " (failed)");
	failover_print (FMA, ")");
	if (obufix) {
		log_debug ("%s", obuf);
	}
#endif
	return status;
}

isc_result_t dhcp_failover_send_poolresp (dhcp_failover_state_t *state,
					  int leases)
{
	dhcp_failover_link_t *link;
	isc_result_t status;
#if defined (DEBUG_FAILOVER_MESSAGES)	
	char obuf [64];
	unsigned obufix = 0;
	
# define FMA obuf, &obufix, sizeof obuf
	failover_print (FMA, "(poolresp");
#else
# define FMA (char *)0, (unsigned *)0, 0
#endif

	if (!state -> link_to_peer ||
	    state -> link_to_peer -> type != dhcp_type_failover_link)
		return DHCP_R_INVALIDARG;
	link = (dhcp_failover_link_t *)state -> link_to_peer;

	if (!link -> outer || link -> outer -> type != omapi_type_connection)
		return DHCP_R_INVALIDARG;

	status = (dhcp_failover_put_message
		  (link, link -> outer,
		   FTM_POOLRESP, link->imsg->xid,
		   dhcp_failover_make_option (FTO_ADDRESSES_TRANSFERRED, FMA,
					      leases),
		   (failover_option_t *)0));

#if defined (DEBUG_FAILOVER_MESSAGES)
	if (status != ISC_R_SUCCESS)
		failover_print (FMA, " (failed)");
	failover_print (FMA, ")");
	if (obufix) {
		log_debug ("%s", obuf);
	}
#endif
	return status;
}

isc_result_t dhcp_failover_send_update_request (dhcp_failover_state_t *state)
{
	dhcp_failover_link_t *link;
	isc_result_t status;
#if defined (DEBUG_FAILOVER_MESSAGES)	
	char obuf [64];
	unsigned obufix = 0;

# define FMA obuf, &obufix, sizeof obuf
	failover_print (FMA, "(updreq");
#else
# define FMA (char *)0, (unsigned *)0, 0
#endif

	if (!state -> link_to_peer ||
	    state -> link_to_peer -> type != dhcp_type_failover_link)
		return DHCP_R_INVALIDARG;
	link = (dhcp_failover_link_t *)state -> link_to_peer;

	if (!link -> outer || link -> outer -> type != omapi_type_connection)
		return DHCP_R_INVALIDARG;

	if (state -> curUPD)
		return ISC_R_ALREADYRUNNING;

	status = (dhcp_failover_put_message
		  (link, link -> outer,
		   FTM_UPDREQ, link->xid++,
		   (failover_option_t *)0));

	if (status == ISC_R_SUCCESS)
		state -> curUPD = FTM_UPDREQ;

#if defined (DEBUG_FAILOVER_MESSAGES)
	if (status != ISC_R_SUCCESS)
		failover_print (FMA, " (failed)");
	failover_print (FMA, ")");
	if (obufix) {
		log_debug ("%s", obuf);
	}
#endif
	log_info ("Sent update request message to %s", state -> name);
	return status;
}

isc_result_t dhcp_failover_send_update_request_all (dhcp_failover_state_t
						    *state)
{
	dhcp_failover_link_t *link;
	isc_result_t status;
#if defined (DEBUG_FAILOVER_MESSAGES)	
	char obuf [64];
	unsigned obufix = 0;
	
# define FMA obuf, &obufix, sizeof obuf
	failover_print (FMA, "(updreqall");
#else
# define FMA (char *)0, (unsigned *)0, 0
#endif

	if (!state -> link_to_peer ||
	    state -> link_to_peer -> type != dhcp_type_failover_link)
		return DHCP_R_INVALIDARG;
	link = (dhcp_failover_link_t *)state -> link_to_peer;

	if (!link -> outer || link -> outer -> type != omapi_type_connection)
		return DHCP_R_INVALIDARG;

	/* If there is an UPDREQ in progress, then upgrade to UPDREQALL. */
	if (state -> curUPD && (state -> curUPD != FTM_UPDREQ))
		return ISC_R_ALREADYRUNNING;

	status = (dhcp_failover_put_message
		  (link, link -> outer,
		   FTM_UPDREQALL, link->xid++,
		   (failover_option_t *)0));

	if (status == ISC_R_SUCCESS)
		state -> curUPD = FTM_UPDREQALL;

#if defined (DEBUG_FAILOVER_MESSAGES)
	if (status != ISC_R_SUCCESS)
		failover_print (FMA, " (failed)");
	failover_print (FMA, ")");
	if (obufix) {
		log_debug ("%s", obuf);
	}
#endif
	log_info ("Sent update request all message to %s", state -> name);
	return status;
}

isc_result_t dhcp_failover_send_update_done (dhcp_failover_state_t *state)
{
	dhcp_failover_link_t *link;
	isc_result_t status;
#if defined (DEBUG_FAILOVER_MESSAGES)	
	char obuf [64];
	unsigned obufix = 0;
	
# define FMA obuf, &obufix, sizeof obuf
	failover_print (FMA, "(upddone");
#else
# define FMA (char *)0, (unsigned *)0, 0
#endif

	if (!state -> link_to_peer ||
	    state -> link_to_peer -> type != dhcp_type_failover_link)
		return DHCP_R_INVALIDARG;
	link = (dhcp_failover_link_t *)state -> link_to_peer;

	if (!link -> outer || link -> outer -> type != omapi_type_connection)
		return DHCP_R_INVALIDARG;

	status = (dhcp_failover_put_message
		  (link, link -> outer,
		   FTM_UPDDONE, state->updxid,
		   (failover_option_t *)0));

#if defined (DEBUG_FAILOVER_MESSAGES)
	if (status != ISC_R_SUCCESS)
		failover_print (FMA, " (failed)");
	failover_print (FMA, ")");
	if (obufix) {
		log_debug ("%s", obuf);
	}
#endif

	log_info ("Sent update done message to %s", state -> name);

	state->updxid--; /* Paranoia, just so it mismatches. */

	/* There may be uncommitted leases at this point (since
	   dhcp_failover_process_bind_ack() doesn't commit leases);
	   commit the lease file. */
	commit_leases();

	return status;
}

/*
 * failover_lease_is_better() compares the binding update in 'msg' with
 * the current lease in 'lease'.  If the determination is that the binding
 * update shouldn't be allowed to update/crush more critical binding info
 * on the lease, the lease is preferred.  A value of true is returned if the
 * local lease is preferred, or false if the remote binding update is
 * preferred.
 *
 * For now this function is hopefully simplistic and trivial.  It may be that
 * a more detailed system of preferences is required, so this is something we
 * should monitor as we gain experience with these dueling events.
 */
static isc_boolean_t
failover_lease_is_better(dhcp_failover_state_t *state, struct lease *lease,
			 failover_message_t *msg)
{
	binding_state_t local_state;
	TIME msg_cltt;

	if (lease->binding_state != lease->desired_binding_state)
		local_state = lease->desired_binding_state;
	else
		local_state = lease->binding_state;

	if ((msg->options_present & FTB_CLTT) != 0)
		msg_cltt = msg->cltt;
	else
		msg_cltt = 0;

	switch(local_state) {
	      case FTS_ACTIVE:
		if (msg->binding_status == FTS_ACTIVE) {
			if (msg_cltt < lease->cltt)
				return ISC_TRUE;
			else if (msg_cltt > lease->cltt)
				return ISC_FALSE;
			else if (state->i_am == primary)
				return ISC_TRUE;
			else
				return ISC_FALSE;
		} else if (msg->binding_status == FTS_EXPIRED) {
			return ISC_FALSE;
		}
		/* FALL THROUGH */

	      case FTS_FREE:
	      case FTS_BACKUP:
	      case FTS_EXPIRED:
	      case FTS_RELEASED:
	      case FTS_ABANDONED:
	      case FTS_RESET:
		if (msg->binding_status == FTS_ACTIVE)
			return ISC_FALSE;
		else if (state->i_am == primary)
			return ISC_TRUE;
		else
			return ISC_FALSE;
		/* FALL THROUGH to impossible condition */

	      default:
		log_fatal("Impossible condition at %s:%d.", MDL);
	}

	log_fatal("Impossible condition at %s:%d.", MDL);
	/* Silence compiler warning. */
	return ISC_FALSE;
}

isc_result_t dhcp_failover_process_bind_update (dhcp_failover_state_t *state,
					       failover_message_t *msg)
{
	struct lease *lt = NULL, *lease = NULL;
	struct iaddr ia;
	int reason = FTR_MISC_REJECT;
	const char *message;
	int new_binding_state;
	int send_to_backup = 0;
	int required_options;
	isc_boolean_t chaddr_changed = ISC_FALSE;
	isc_boolean_t ident_changed = ISC_FALSE;

	/* Validate the binding update. */
	required_options = FTB_ASSIGNED_IP_ADDRESS | FTB_BINDING_STATUS;
	if ((msg->options_present & required_options) != required_options) {
		message = "binding update lacks required options";
		reason = FTR_MISSING_BINDINFO;
		goto bad;
	}

	ia.len = sizeof msg -> assigned_addr;
	memcpy (ia.iabuf, &msg -> assigned_addr, ia.len);

	if (!find_lease_by_ip_addr (&lease, ia, MDL)) {
		message = "unknown IP address";
		reason = FTR_ILLEGAL_IP_ADDR;
		goto bad;
	}

	/*
	 * If this lease is covered by a different failover peering
	 * relationship, assert an error.
	 */
	if ((lease->pool == NULL) || (lease->pool->failover_peer == NULL) ||
	    (lease->pool->failover_peer != state)) {
		message = "IP address is covered by a different failover "
			  "relationship state";
		reason = FTR_ILLEGAL_IP_ADDR;
		goto bad;
	}

	/*
	 * Dueling updates:  This happens when both servers send a BNDUPD
	 * at the same time.  We want the best update to win, which means
	 * we reject if we think ours is better, or cancel if we think the
	 * peer's is better.  We only assert a problem if the lease is on
	 * the ACK queue, not on the UPDATE queue.  This means that after
	 * accepting this server's BNDUPD, we will send our own BNDUPD
	 * /after/ sending the BNDACK (this order was recently enforced in
	 * queue processing).
	 */
	if ((lease->flags & ON_ACK_QUEUE) != 0) {
		if (failover_lease_is_better(state, lease, msg)) {
			message = "incoming update is less critical than "
				  "outgoing update";
			reason = FTR_LESS_CRIT_BIND_INFO;
			goto bad;
		} else {
			/* This makes it so we ignore any spurious ACKs. */
			dhcp_failover_ack_queue_remove(state, lease);
		}
	}

	/* Install the new info.  Start by taking a copy to markup. */
	if (!lease_copy (&lt, lease, MDL)) {
		message = "no memory";
		goto bad;
	}

	if (msg -> options_present & FTB_CHADDR) {
		if (msg->binding_status == FTS_ABANDONED) {
			message = "BNDUPD to ABANDONED with a CHADDR";
			goto bad;
		}
		if (msg -> chaddr.count > sizeof lt -> hardware_addr.hbuf) {
			message = "chaddr too long";
			goto bad;
		}

		if ((lt->hardware_addr.hlen != msg->chaddr.count) ||
		    (memcmp(lt->hardware_addr.hbuf, msg->chaddr.data,
			    msg->chaddr.count) != 0))
			chaddr_changed = ISC_TRUE;

		lt -> hardware_addr.hlen = msg -> chaddr.count;
		memcpy (lt -> hardware_addr.hbuf, msg -> chaddr.data,
			msg -> chaddr.count);
	} else if (msg->binding_status == FTS_ACTIVE ||
		   msg->binding_status == FTS_EXPIRED ||
		   msg->binding_status == FTS_RELEASED) {
		message = "BNDUPD without CHADDR";
		reason = FTR_MISSING_BINDINFO;
		goto bad;
	} else if (msg->binding_status == FTS_ABANDONED) {
		chaddr_changed = ISC_TRUE;
		lt->hardware_addr.hlen = 0;
		if (lt->scope)
			binding_scope_dereference(&lt->scope, MDL);
	}

	/* There is no explicit message content to indicate that the client
	 * supplied no client-identifier.  So if we don't hear of a value,
	 * we discard the last one.
	 */
	if (msg->options_present & FTB_CLIENT_IDENTIFIER) {
		if (msg->binding_status == FTS_ABANDONED) {
			message = "BNDUPD to ABANDONED with client-id";
			goto bad;
		}

		if ((lt->uid_len != msg->client_identifier.count) ||
		    (lt->uid == NULL) || /* Sanity; should never happen. */
		    (memcmp(lt->uid, msg->client_identifier.data,
			    lt->uid_len) != 0))
			ident_changed = ISC_TRUE;

		lt->uid_len = msg->client_identifier.count;

		/* Allocate the lt->uid buffer if we haven't already, or
		 * re-allocate the lt-uid buffer if we have one that is not
		 * large enough.  Otherwise, just use the extant buffer.
		 */
		if (!lt->uid || lt->uid == lt->uid_buf ||
		    lt->uid_len > lt->uid_max) {
			if (lt->uid && lt->uid != lt->uid_buf)
				dfree(lt->uid, MDL);

			if (lt->uid_len > sizeof(lt->uid_buf)) {
				lt->uid_max = lt->uid_len;
				lt->uid = dmalloc(lt->uid_len, MDL);
				if (!lt->uid) {
					message = "no memory";
					goto bad;
				}
			} else {
				lt->uid_max = sizeof(lt->uid_buf);
				lt->uid = lt->uid_buf;
			}
		}
		memcpy (lt -> uid,
			msg -> client_identifier.data, lt -> uid_len);
	} else if (lt->uid && msg->binding_status != FTS_RESET &&
		   msg->binding_status != FTS_FREE &&
		   msg->binding_status != FTS_BACKUP) {
		ident_changed = ISC_TRUE;
		if (lt->uid != lt->uid_buf)
			dfree (lt->uid, MDL);
		lt->uid = NULL;
		lt->uid_max = lt->uid_len = 0;
	}

	/*
	 * A server's configuration can assign a 'binding scope';
	 *
	 *	set var = "value";
	 *
	 * The problem with these binding scopes is that they are refreshed
	 * when the server processes a client's DHCP packet.  A local binding
	 * scope is trash, then, when the lease has been assigned by the
	 * partner server.  There is no real way to detect this, a peer may
	 * be updating us (as through potential conflict) with a binding we
	 * sent them, but we can trivially detect the /problematic/ case;
	 *
	 *	lease is free.
	 *	primary allocates lease to client A, assigns ddns name A.
	 *	primary fails.
	 *	secondary enters partner down.
	 *	lease expires, and is set free.
	 *	lease is allocated to client B and given ddns name B.
	 *	primary recovers.
	 *
	 * The binding update in this case will be active->active, but the
	 * client identification on the lease will have changed.  The ddns
	 * update on client A will have leaked if we just remove the binding
	 * scope blindly.
	 */
	if (msg->binding_status == FTS_ACTIVE &&
	    (chaddr_changed || ident_changed)) {
		(void) ddns_removals(lease, NULL, NULL, ISC_FALSE);

		if (lease->scope != NULL)
			binding_scope_dereference(&lease->scope, MDL);
	}

	/* XXX Times may need to be adjusted based on clock skew! */
	if (msg -> options_present & FTB_STOS) {
		lt -> starts = msg -> stos;
	}
	if (msg -> options_present & FTB_LEASE_EXPIRY) {
		lt -> ends = msg -> expiry;
	}
	if (msg->options_present & FTB_POTENTIAL_EXPIRY) {
		lt->atsfp = lt->tsfp = msg->potential_expiry;
	}
	if (msg->options_present & FTB_IP_FLAGS) {
		if (msg->ip_flags & FTF_IP_FLAG_RESERVE) {
			if ((((state->i_am == primary) &&
			      (lease->binding_state == FTS_FREE)) ||
			     ((state->i_am == secondary) &&
			      (lease->binding_state == FTS_BACKUP))) &&
			    !(lease->flags & RESERVED_LEASE)) {
				message = "Address is not reserved.";
				reason = FTR_IP_NOT_RESERVED;
				goto bad;
			}

			lt->flags |= RESERVED_LEASE;
		} else
			lt->flags &= ~RESERVED_LEASE;

		if (msg->ip_flags & FTF_IP_FLAG_BOOTP) {
			if ((((state->i_am == primary) &&
			      (lease->binding_state == FTS_FREE)) ||
			     ((state->i_am == secondary) &&
			      (lease->binding_state == FTS_BACKUP))) &&
			    !(lease->flags & BOOTP_LEASE)) {
				message = "Address is not allocated to BOOTP.";
				goto bad;
			}
			lt->flags |= BOOTP_LEASE;
		} else
			lt->flags &= ~BOOTP_LEASE;

		if (msg->ip_flags & ~(FTF_IP_FLAG_RESERVE | FTF_IP_FLAG_BOOTP))
			log_info("Unknown IP-flags set in BNDUPD (0x%x).",
				 msg->ip_flags);
	} else /* Flags may only not appear if the values are zero. */
		lt->flags &= ~(RESERVED_LEASE | BOOTP_LEASE);

#if defined (DEBUG_LEASE_STATE_TRANSITIONS)
	log_info ("processing state transition for %s: %s to %s",
		  piaddr (lease -> ip_addr),
		  binding_state_print (lease -> binding_state),
		  binding_state_print (msg -> binding_status));
#endif

	/* If we're in normal state, make sure the state transition
	   we got is valid. */
	if (state -> me.state == normal) {
		new_binding_state =
			(normal_binding_state_transition_check
			 (lease, state, msg -> binding_status,
			  msg -> potential_expiry));
		/* XXX if the transition the peer asked for isn't
		   XXX allowed, maybe we should make the transition
		   XXX into potential-conflict at this point. */
	} else {
		new_binding_state =
			(conflict_binding_state_transition_check
			 (lease, state, msg -> binding_status,
			  msg -> potential_expiry));
	}
	if (new_binding_state != msg -> binding_status) {
		char outbuf [100];

		if (snprintf (outbuf, sizeof outbuf,
			  "%s: invalid state transition: %s to %s",
			  piaddr (lease -> ip_addr),
			  binding_state_print (lease -> binding_state),
			  binding_state_print (msg -> binding_status))
					>= sizeof outbuf)
			log_fatal ("%s: impossible outbuf overflow",
				"dhcp_failover_process_bind_update");

		dhcp_failover_send_bind_ack (state, msg,
					     FTR_FATAL_CONFLICT,
					     outbuf);
		goto out;
	}
	if (new_binding_state == FTS_EXPIRED ||
	    new_binding_state == FTS_RELEASED ||
	    new_binding_state == FTS_RESET) {
		lt -> next_binding_state = FTS_FREE;

		/* Mac address affinity.  Assign the lease to
		 * BACKUP state if we are the primary and the
		 * peer is more likely to reallocate this lease
		 * to a returning client.
		 */
		if ((state->i_am == primary) &&
		    !(lt->flags & (RESERVED_LEASE | BOOTP_LEASE)))
			send_to_backup = peer_wants_lease(lt);
	} else {
		lt -> next_binding_state = new_binding_state;
	}
	msg -> binding_status = lt -> next_binding_state;

	/*
	 * If we accept a peer's binding update, then we can't rewind a
	 * lease behind the peer's state.
	 */
	lease->rewind_binding_state = lt->next_binding_state;

	/* Try to install the new information. */
	if (!supersede_lease (lease, lt, 0, 0, 0) ||
	    !write_lease (lease)) {
		message = "database update failed";
	      bad:
		dhcp_failover_send_bind_ack (state, msg, reason, message);
		goto out;
	} else {
		dhcp_failover_queue_ack (state, msg);
	}

	/* If it is probably wise, assign lease to backup state if the peer
	 * is not already hoarding leases.
	 */
	if (send_to_backup && secondary_not_hoarding(state, lease->pool)) {
		lease->next_binding_state = FTS_BACKUP;
		lease->tstp = cur_time;
		lease->starts = cur_time;

		if (!supersede_lease(lease, NULL, 0, 1, 0) ||
		    !write_lease(lease))
			log_error("can't commit lease %s for mac addr "
				  "affinity", piaddr(lease->ip_addr));

		dhcp_failover_send_updates(state);
	}

      out:
	if (lt)
		lease_dereference (&lt, MDL);
	if (lease)
		lease_dereference (&lease, MDL);

	return ISC_R_SUCCESS;
}

/* This was hairy enough I didn't want to do it all in an if statement.
 *
 * Returns: Truth is the secondary is allowed to get more leases based upon
 * MAC address affinity.  False otherwise.
 */
static inline int
secondary_not_hoarding(dhcp_failover_state_t *state, struct pool *p) {
	int total;
	int hold;
	int lts;

	total = p->free_leases + p->backup_leases;

	/* How many leases is one side or the other allowed to "hold"? */
	hold = ((total * state->max_lease_ownership) + 50) / 100;

	/* If we were to send leases (or if the secondary were to send us
	 * leases in the negative direction), how many would that be?
	 */
	lts = (p->free_leases - p->backup_leases) / 2;

	/* The peer is not hoarding leases if we would send them more leases
	 * (or they would take fewer leases) than the maximum they are allowed
	 * to hold (the negative hold).
	 */
	return(lts > -hold);
}

isc_result_t dhcp_failover_process_bind_ack (dhcp_failover_state_t *state,
					     failover_message_t *msg)
{
	struct lease *lt = (struct lease *)0;
	struct lease *lease = (struct lease *)0;
	struct iaddr ia;
	const char *message = "no memory";
	u_int32_t pot_expire;
	int send_to_backup = ISC_FALSE;
	struct timeval tv;

	ia.len = sizeof msg -> assigned_addr;
	memcpy (ia.iabuf, &msg -> assigned_addr, ia.len);

	if (!find_lease_by_ip_addr (&lease, ia, MDL)) {
		message = "no such lease";
		goto bad;
	}

	/* XXX check for conflicts. */
	if (msg -> options_present & FTB_REJECT_REASON) {
		log_error ("bind update on %s from %s rejected: %.*s",
			   piaddr (ia), state -> name,
			   (int)((msg -> options_present & FTB_MESSAGE)
				 ? msg -> message.count
				 : strlen (dhcp_failover_reject_reason_print
					   (msg -> reject_reason))),
			   (msg -> options_present & FTB_MESSAGE)
			   ? (const char *)(msg -> message.data)
			   : (dhcp_failover_reject_reason_print
			      (msg -> reject_reason)));
		goto unqueue;
	}

	/* Silently discard acks for leases we did not update (or multiple
	 * acks).
	 */
	if (!lease->last_xid)
		goto unqueue;

	if (lease->last_xid != msg->xid) {
		message = "xid mismatch";
		goto bad;
	}

	/* XXX Times may need to be adjusted based on clock skew! */
	if (msg->options_present & FTO_POTENTIAL_EXPIRY)
		pot_expire = msg->potential_expiry;
	else
		pot_expire = lease->tstp;

	/* If the lease was desired to enter a binding state, we set
	 * such a value upon transmitting a bndupd.  We do not clear it
	 * if we receive a bndupd in the meantime (or change the state
	 * of the lease again ourselves), but we do set binding_state
	 * if we get a bndupd.
	 *
	 * So desired_binding_state tells us what we sent a bndupd for,
	 * and binding_state tells us what we have since determined in
	 * the meantime.
	 */
	if (lease->desired_binding_state == FTS_EXPIRED ||
	    lease->desired_binding_state == FTS_RESET ||
	    lease->desired_binding_state == FTS_RELEASED)
	{
		/* It is not a problem to do this directly as we call
		 * supersede_lease immediately after: the lease is requeued
		 * even if its sort order (tsfp) has changed.
		 */
		lease->atsfp = lease->tsfp = pot_expire;
		if ((state->i_am == secondary) &&
		    (lease->flags & RESERVED_LEASE))
			lease->next_binding_state = FTS_BACKUP;
		else
			lease->next_binding_state = FTS_FREE;

		/* Clear this condition for the next go-round. */
		lease->desired_binding_state = lease->next_binding_state;

		/* The peer will have made this state change, so set rewind. */
		lease->rewind_binding_state = lease->next_binding_state;

		supersede_lease(lease, (struct lease *)0, 0, 0, 0);
		write_lease(lease);

		/* Lease has returned to FREE state from the
		 * transitional states.  If the lease 'belongs'
		 * to a client that would be served by the
		 * peer, process a binding update now to send
		 * the lease to backup state.  But not if we
		 * think we already have.
		 */
		if (state->i_am == primary &&
		    !(lease->flags & (RESERVED_LEASE | BOOTP_LEASE)) &&
		    peer_wants_lease(lease))
			send_to_backup = ISC_TRUE;

		if (!send_to_backup && state->me.state == normal)
			commit_leases();
	} else {
		/* XXX It could be a problem to do this directly if the lease
		 * XXX is sorted by tsfp.
		 */
		lease->atsfp = lease->tsfp = pot_expire;
		if (lease->desired_binding_state != lease->binding_state) {
			lease->next_binding_state =
				lease->desired_binding_state;
			supersede_lease(lease,
					(struct lease *)0, 0, 0, 0);
		}
		write_lease(lease);
		/* Commit the lease only after a two-second timeout,
		   so that if we get a bunch of acks in quick
		   succession (e.g., when stealing leases from the
		   secondary), we do not do an immediate commit for
		   each one. */
		tv.tv_sec = cur_time + 2;
		tv.tv_usec = 0;
		add_timeout(&tv, commit_leases_timeout, (void *)0, 0, 0);
	}

      unqueue:
	dhcp_failover_ack_queue_remove (state, lease);

	/* If we are supposed to send an update done after we send
	   this lease, go ahead and send it. */
	if (state -> send_update_done == lease) {
		lease_dereference (&state -> send_update_done, MDL);
		dhcp_failover_send_update_done (state);
	}

	/* Now that the lease is off the ack queue, consider putting it
	 * back on the update queue for mac address affinity.
	 */
	if (send_to_backup && secondary_not_hoarding(state, lease->pool)) {
		lease->next_binding_state = FTS_BACKUP;
		lease->tstp = lease->starts = cur_time;

		if (!supersede_lease(lease, NULL, 0, 1, 0) ||
		    !write_lease(lease))
			log_error("can't commit lease %s for "
				  "client affinity", piaddr(lease->ip_addr));

		if (state->me.state == normal)
			commit_leases();
	}

	/* If there are updates pending, we've created space to send at
	   least one. */
	dhcp_failover_send_updates (state);

      out:
	lease_dereference (&lease, MDL);
	if (lt)
		lease_dereference (&lt, MDL);

	return ISC_R_SUCCESS;

      bad:
	log_info ("bind update on %s got ack from %s: %s.",
		  piaddr (ia), state -> name, message);
	goto out;
}

isc_result_t dhcp_failover_generate_update_queue (dhcp_failover_state_t *state,
						  int everythingp)
{
	struct shared_network *s;
	struct pool *p;
	struct lease *l;
	int i;
#define FREE_LEASES 0
#define ACTIVE_LEASES 1
#define EXPIRED_LEASES 2
#define ABANDONED_LEASES 3
#define BACKUP_LEASES 4
#define RESERVED_LEASES 5
	struct lease **lptr[RESERVED_LEASES+1];

	/* Loop through each pool in each shared network and call the
	   expiry routine on the pool. */
	for (s = shared_networks; s; s = s -> next) {
	    for (p = s -> pools; p; p = p -> next) {
		if (p->failover_peer != state)
			continue;

		lptr[FREE_LEASES] = &p->free;
		lptr[ACTIVE_LEASES] = &p->active;
		lptr[EXPIRED_LEASES] = &p->expired;
		lptr[ABANDONED_LEASES] = &p->abandoned;
		lptr[BACKUP_LEASES] = &p->backup;
		lptr[RESERVED_LEASES] = &p->reserved;

		for (i = FREE_LEASES; i <= RESERVED_LEASES; i++) {
		    for (l = *(lptr [i]); l; l = l -> next) {
			if ((l->flags & ON_QUEUE) == 0 &&
			    (everythingp ||
			     (l->tstp > l->atsfp) ||
			     (i == EXPIRED_LEASES))) {
				l -> desired_binding_state = l -> binding_state;
				dhcp_failover_queue_update (l, 0);
			}
		    }
		}
	    }
	}
	return ISC_R_SUCCESS;
}

isc_result_t
dhcp_failover_process_update_request (dhcp_failover_state_t *state,
				      failover_message_t *msg)
{
	if (state->send_update_done) {
		log_info("Received update request while old update still "
			 "flying!  Silently discarding old request.");
		lease_dereference(&state->send_update_done, MDL);
	}

	/* Generate a fresh update queue. */
	dhcp_failover_generate_update_queue (state, 0);

	state->updxid = msg->xid;

	/* If there's anything on the update queue (there shouldn't be
	   anything on the ack queue), trigger an update done message
	   when we get an ack for that lease. */
	if (state -> update_queue_tail) {
		lease_reference (&state -> send_update_done,
				 state -> update_queue_tail, MDL);
		dhcp_failover_send_updates (state);
		log_info ("Update request from %s: sending update",
			   state -> name);
	} else {
		/* Otherwise, there are no updates to send, so we can
		   just send an UPDDONE message immediately. */
		dhcp_failover_send_update_done (state);
		log_info ("Update request from %s: nothing pending",
			   state -> name);
	}

	return ISC_R_SUCCESS;
}

isc_result_t
dhcp_failover_process_update_request_all (dhcp_failover_state_t *state,
					  failover_message_t *msg)
{
	if (state->send_update_done) {
		log_info("Received update request while old update still "
			 "flying!  Silently discarding old request.");
		lease_dereference(&state->send_update_done, MDL);
	}

	/* Generate a fresh update queue that includes every lease. */
	dhcp_failover_generate_update_queue (state, 1);

	state->updxid = msg->xid;

	if (state -> update_queue_tail) {
		lease_reference (&state -> send_update_done,
				 state -> update_queue_tail, MDL);
		dhcp_failover_send_updates (state);
		log_info ("Update request all from %s: sending update",
			   state -> name);
	} else {
		/* This should really never happen, but it could happen
		   on a server that currently has no leases configured. */
		dhcp_failover_send_update_done (state);
		log_info ("Update request all from %s: nothing pending",
			   state -> name);
	}

	return ISC_R_SUCCESS;
}

isc_result_t
dhcp_failover_process_update_done (dhcp_failover_state_t *state,
				   failover_message_t *msg)
{
	struct timeval tv;

	log_info ("failover peer %s: peer update completed.",
		  state -> name);

	state -> curUPD = 0;

	switch (state -> me.state) {
	      case unknown_state:
	      case partner_down:
	      case normal:
	      case communications_interrupted:
	      case resolution_interrupted:
	      case shut_down:
	      case paused:
	      case recover_done:
	      case startup:
	      case recover_wait:
		break;	/* shouldn't happen. */

		/* We got the UPDDONE, so we can go into normal state! */
	      case potential_conflict:
		if (state->partner.state == conflict_done) {
			if (state->i_am == secondary) {
				dhcp_failover_set_state (state, normal);
			} else {
				log_error("Secondary is in conflict_done "
					  "state after conflict resolution, "
					  "this is illegal.");
				dhcp_failover_set_state (state, shut_down);
			}
		} else {
			if (state->i_am == primary)
				dhcp_failover_set_state (state, conflict_done);
			else
				log_error("Spurious update-done message.");
		}

		break;

	      case conflict_done:
		log_error("Spurious update-done message.");
		break;

	      case recover:
		/* Wait for MCLT to expire before moving to recover_done,
		   except that if both peers come up in recover, there is
		   no point in waiting for MCLT to expire - this probably
		   indicates the initial startup of a newly-configured
		   failover pair. */
		if (state -> me.stos + state -> mclt > cur_time &&
		    state -> partner.state != recover &&
		    state -> partner.state != recover_done) {
			dhcp_failover_set_state (state, recover_wait);
#if defined (DEBUG_FAILOVER_TIMING)
			log_info ("add_timeout +%d %s",
				  (int)(cur_time -
					state -> me.stos + state -> mclt),
				  "dhcp_failover_recover_done");
#endif
			tv . tv_sec = (int)(state -> me.stos + state -> mclt);
			tv . tv_usec = 0;
			add_timeout (&tv,
				     dhcp_failover_recover_done,
				     state,
				     (tvref_t)omapi_object_reference,
				     (tvunref_t)
				     omapi_object_dereference);
		} else
			dhcp_failover_recover_done (state);
	}

	return ISC_R_SUCCESS;
}

void dhcp_failover_recover_done (void *sp)
{
	dhcp_failover_state_t *state = sp;

#if defined (DEBUG_FAILOVER_TIMING)
	log_info ("dhcp_failover_recover_done");
#endif

	dhcp_failover_set_state (state, recover_done);
}

#if defined (DEBUG_FAILOVER_MESSAGES)
/* Print hunks of failover messages, doing line breaks as appropriate.
   Note that this assumes syslog is being used, rather than, e.g., the
   Windows NT logging facility, where just dumping the whole message in
   one hunk would be more appropriate. */

void failover_print (char *obuf,
		     unsigned *obufix, unsigned obufmax, const char *s)
{
	int len = strlen (s);

	while (len + *obufix + 1 >= obufmax) {
		log_debug ("%s", obuf);
		if (!*obufix) {
			log_debug ("%s", s);
			*obufix = 0;
			return;
		}
		*obufix = 0;
	}
	strcpy (&obuf [*obufix], s);
	*obufix += len;
}	
#endif /* defined (DEBUG_FAILOVER_MESSAGES) */

/* Taken from draft-ietf-dhc-loadb-01.txt: */
/* A "mixing table" of 256 distinct values, in pseudo-random order. */
unsigned char loadb_mx_tbl[256] = {
    251, 175, 119, 215,  81,  14,  79, 191, 103,  49,
    181, 143, 186, 157,   0, 232,  31,  32,  55,  60,
    152,  58,  17, 237, 174,  70, 160, 144, 220,  90,
    57,  223,  59,   3,  18, 140, 111, 166, 203, 196,
    134, 243, 124,  95, 222, 179, 197,  65, 180,  48,
     36,  15, 107,  46, 233, 130, 165,  30, 123, 161,
    209,  23,  97,  16,  40,  91, 219,  61, 100,  10,
    210, 109, 250, 127,  22, 138,  29, 108, 244,  67,
    207,   9, 178, 204,  74,  98, 126, 249, 167, 116,
    34,   77, 193, 200, 121,   5,  20, 113,  71,  35,
    128,  13, 182,  94,  25, 226, 227, 199,  75,  27,
     41, 245, 230, 224,  43, 225, 177,  26, 155, 150,
    212, 142, 218, 115, 241,  73,  88, 105,  39, 114,
     62, 255, 192, 201, 145, 214, 168, 158, 221, 148,
    154, 122,  12,  84,  82, 163,  44, 139, 228, 236,
    205, 242, 217,  11, 187, 146, 159,  64,  86, 239,
    195,  42, 106, 198, 118, 112, 184, 172,  87,   2,
    173, 117, 176, 229, 247, 253, 137, 185,  99, 164,
    102, 147,  45,  66, 231,  52, 141, 211, 194, 206,
    246, 238,  56, 110,  78, 248,  63, 240, 189,  93,
     92,  51,  53, 183,  19, 171,  72,  50,  33, 104,
    101,  69,   8, 252,  83, 120,  76, 135,  85,  54,
    202, 125, 188, 213,  96, 235, 136, 208, 162, 129,
    190, 132, 156,  38,  47,   1,   7, 254,  24,   4,
    216, 131,  89,  21,  28, 133,  37, 153, 149,  80,
    170,  68,   6, 169, 234, 151 };

static unsigned char loadb_p_hash (const unsigned char *, unsigned);
static unsigned char loadb_p_hash (const unsigned char *key, unsigned len)
{
        unsigned char hash = len;
        int i;
        for(i = len; i > 0;  )
		hash = loadb_mx_tbl [hash ^ (key [--i])];
        return hash;
}

int load_balance_mine (struct packet *packet, dhcp_failover_state_t *state)
{
	struct option_cache *oc;
	struct data_string ds;
	unsigned char hbaix;
	int hm;
	u_int16_t ec; 

	ec = ntohs(packet->raw->secs);

#if defined(SECS_BYTEORDER)
	/*
	 * If desired check to see if the secs field may have been byte
	 * swapped.  We assume it has if the high order byte isn't cleared
	 * while the low order byte is cleared.  In this case we swap the
	 * bytes and continue processing.
	 */
	if ((ec > 255) && ((ec & 0xff) == 0)) {
		ec = (ec >> 8) | (ec << 8);
	}
#endif

	if (state->load_balance_max_secs < ec) {
		return (1);
	}

	/* If we don't have a hash bucket array, we can't tell if this
	   one's ours, so we assume it's not. */
	if (!state->hba)
		return (0);

	oc = lookup_option(&dhcp_universe, packet->options,
			   DHO_DHCP_CLIENT_IDENTIFIER);
	memset(&ds, 0, sizeof ds);
	if (oc &&
	    evaluate_option_cache(&ds, packet, NULL, NULL,
				  packet->options, NULL,
				  &global_scope, oc, MDL)) {
		hbaix = loadb_p_hash(ds.data, ds.len);

		data_string_forget(&ds, MDL);
	} else {
		hbaix = loadb_p_hash(packet->raw->chaddr,
				     packet->raw->hlen);
	}

	hm = state->hba[(hbaix >> 3) & 0x1F] & (1 << (hbaix & 0x07));

	if (state->i_am == primary)
		return (hm);
	else
		return (!hm);
}

/* The inverse of load_balance_mine ("load balance theirs").  We can't
 * use the regular load_balance_mine() and invert it because of the case
 * where there might not be an HBA, and we want to indicate false here
 * in this case only.
 */
int
peer_wants_lease(struct lease *lp)
{
	dhcp_failover_state_t *state;
	unsigned char hbaix;
	int hm;

	if (!lp->pool)
		return 0;

	state = lp->pool->failover_peer;

	if (!state || !state->hba)
		return 0;

	if (lp->uid_len)
		hbaix = loadb_p_hash(lp->uid, lp->uid_len);
	else if (lp->hardware_addr.hlen > 1)
		/* Skip the first byte, which is the hardware type, and is
		 * not included during actual load balancing checks above
		 * since it is separate from the packet header chaddr field.
		 * The remainder of the hardware address should be identical
		 * to the chaddr contents.
		 */
		hbaix = loadb_p_hash(lp->hardware_addr.hbuf + 1,
				     lp->hardware_addr.hlen - 1);
	else /* impossible to categorize into LBA */
		return 0;

	hm = state->hba[(hbaix >> 3) & 0x1F] & (1 << (hbaix & 0x07));

	if (state->i_am == primary)
		return !hm;
	else
		return hm;
}

/* This deals with what to do with bind updates when
   we're in the normal state 

   Note that tsfp had better be set from the latest bind update
   _before_ this function is called! */

binding_state_t
normal_binding_state_transition_check (struct lease *lease,
				       dhcp_failover_state_t *state,
				       binding_state_t binding_state,
				       u_int32_t tsfp)
{
	binding_state_t new_state;

	/* If there is no transition, it's no problem. */
	if (binding_state == lease -> binding_state)
		return binding_state;

	switch (lease -> binding_state) {
	      case FTS_FREE:
	      case FTS_ABANDONED:
		switch (binding_state) {
		      case FTS_ACTIVE:
		      case FTS_ABANDONED:
		      case FTS_BACKUP:
		      case FTS_EXPIRED:
		      case FTS_RELEASED:
		      case FTS_RESET:
			/* If the lease was free, and our peer is primary,
			   then it can make it active, or abandoned, or
			   backup.    Abandoned is treated like free in
			   this case. */
			if (state -> i_am == secondary)
				return binding_state;

			/* Otherwise, it can't legitimately do any sort of
			   state transition.   Because the lease was free,
			   and the error has already been made, we allow the
			   peer to change its state anyway, but log a warning
			   message in hopes that the error will be fixed. */
		      case FTS_FREE: /* for compiler */
			new_state = binding_state;
			goto out;

		      default:
			log_fatal ("Impossible case at %s:%d.", MDL);
			return FTS_RESET;
		}
	      case FTS_ACTIVE:
		/* The secondary can't change the state of an active
		   lease. */
		if (state -> i_am == primary) {
			/* Except that the client may send the DHCPRELEASE
			   to the secondary, and we have to accept that. */
			if (binding_state == FTS_RELEASED)
				return binding_state;
			new_state = lease -> binding_state;
			goto out;
		}

		/* So this is only for transitions made by the primary: */
		switch (binding_state) {
		      case FTS_FREE:
		      case FTS_BACKUP:
			/* Can't set a lease to free or backup until the
			   peer agrees that it's expired. */
			if (tsfp > cur_time) {
				new_state = lease -> binding_state;
				goto out;
			}
			return binding_state;

		      case FTS_EXPIRED:
			/* XXX 65 should be the clock skew between the peers
			   XXX plus a fudge factor.   This code will result
			   XXX in problems if MCLT is really short or the
			   XXX max-lease-time is really short (less than the
			   XXX fudge factor. */
			if (lease -> ends - 65 > cur_time) {
				new_state = lease -> binding_state;
				goto out;
			}

		      case FTS_RELEASED:
		      case FTS_ABANDONED:
		      case FTS_RESET:
		      case FTS_ACTIVE:
			return binding_state;

		      default:
			log_fatal ("Impossible case at %s:%d.", MDL);
			return FTS_RESET;
		}
		break;
	      case FTS_EXPIRED:
		switch (binding_state) {
		      case FTS_BACKUP:
		      case FTS_FREE:
			/* Can't set a lease to free or backup until the
			   peer agrees that it's expired. */
			if (tsfp > cur_time) {
				new_state = lease -> binding_state;
				goto out;
			}
			return binding_state;

		      case FTS_ACTIVE:
		      case FTS_RELEASED:
		      case FTS_ABANDONED:
		      case FTS_RESET:
		      case FTS_EXPIRED:
			return binding_state;

		      default:
			log_fatal ("Impossible case at %s:%d.", MDL);
			return FTS_RESET;
		}
	      case FTS_RELEASED:
		switch (binding_state) {
		      case FTS_FREE:
		      case FTS_BACKUP:

			/* These are invalid state transitions - should we
			   prevent them? */
		      case FTS_EXPIRED:
		      case FTS_ABANDONED:
		      case FTS_RESET:
		      case FTS_ACTIVE:
		      case FTS_RELEASED:
			return binding_state;

		      default:
			log_fatal ("Impossible case at %s:%d.", MDL);
			return FTS_RESET;
		}
	      case FTS_RESET:
		switch (binding_state) {
		      case FTS_FREE:
		      case FTS_BACKUP:
			/* Can't set a lease to free or backup until the
			   peer agrees that it's expired. */
			if (tsfp > cur_time) {
				new_state = lease -> binding_state;
				goto out;
			}
			return binding_state;

		      case FTS_ACTIVE:
		      case FTS_EXPIRED:
		      case FTS_RELEASED:
		      case FTS_ABANDONED:
		      case FTS_RESET:
			return binding_state;

		      default:
			log_fatal ("Impossible case at %s:%d.", MDL);
			return FTS_RESET;
		}
	      case FTS_BACKUP:
		switch (binding_state) {
		      case FTS_ACTIVE:
		      case FTS_ABANDONED:
		      case FTS_EXPIRED:
		      case FTS_RELEASED:
		      case FTS_RESET:
			/* If the lease was in backup, and our peer
			   is secondary, then it can make it active
			   or abandoned. */
			if (state -> i_am == primary)
				return binding_state;

			/* Either the primary or the secondary can
			   reasonably move a lease from the backup
			   state to the free state. */
		      case FTS_FREE:
			return binding_state;

		      case FTS_BACKUP:
			new_state = lease -> binding_state;
			goto out;

		      default:
			log_fatal ("Impossible case at %s:%d.", MDL);
			return FTS_RESET;
		}

	      default:
		log_fatal ("Impossible case at %s:%d.", MDL);
		return FTS_RESET;
	}
      out:
	return new_state;
}

/* Determine whether the state transition is okay when we're potentially
   in conflict with the peer. */
binding_state_t
conflict_binding_state_transition_check (struct lease *lease,
					 dhcp_failover_state_t *state,
					 binding_state_t binding_state,
					 u_int32_t tsfp)
{
	binding_state_t new_state;

	/* If there is no transition, it's no problem. */
	if (binding_state == lease -> binding_state)
		new_state = binding_state;
	else {
		switch (lease -> binding_state) {
			/* If we think the lease is not in use, then the
			   state into which the partner put it is just fine,
			   whatever it is. */
		      case FTS_FREE:
		      case FTS_ABANDONED:
		      case FTS_EXPIRED:
		      case FTS_RELEASED:
		      case FTS_RESET:
		      case FTS_BACKUP:
			new_state = binding_state;
			break;

			/* If we think the lease *is* in use, then we're not
			   going to take the partner's change if the partner
			   thinks it's free. */
		      case FTS_ACTIVE:
			switch (binding_state) {
			      case FTS_FREE:
			      case FTS_BACKUP:
				new_state = lease -> binding_state;
				break;

			      case FTS_EXPIRED:
				/* If we don't agree about expiry, it's
				 * invalid.  65 should allow for max
				 * clock skew (60) plus some fudge.
				 * XXX: should we refetch cur_time?
				 */
				if ((lease->ends - 65) > cur_time)
					new_state = lease->binding_state;
				else
					new_state = binding_state;
				break;

				/* RELEASED, RESET, and ABANDONED indicate
				 * that our partner has information about
				 * this lease that we did not witness.  Our
				 * partner wins.
				 */
			      case FTS_RELEASED:
			      case FTS_RESET:
			      case FTS_ABANDONED:
				new_state = binding_state;
				break;

			      default:
				log_fatal ("Impossible case at %s:%d.", MDL);
				return FTS_RESET;
			}
			break;

		      default:
			log_fatal ("Impossible case at %s:%d.", MDL);
			return FTS_RESET;
		}
	}
	return new_state;
}

/* We can reallocate a lease under the following circumstances:

   (1) It belongs to us - it's FTS_FREE, and we're primary, or it's
       FTS_BACKUP, and we're secondary.
   (2) We're in partner_down, and the lease is not active, and we
       can be sure that the other server didn't make it active.
       We can only be sure that the server didn't make it active
       when we are in the partner_down state and one of the following
       two conditions holds:
       (a) in the case that the time sent from the peer is earlier than
           the time we entered the partner_down state, at least MCLT has
	   gone by since we entered partner_down, or
       (b) in the case that the time sent from the peer is later than
           the time when we entered partner_down, the current time is
	   later than the time sent from the peer by at least MCLT. */

int lease_mine_to_reallocate (struct lease *lease)
{
	dhcp_failover_state_t *peer;

	if (lease && lease->pool &&
	    (peer = lease->pool->failover_peer)) {
		/*
		 * In addition to the normal rules governing wether a server
		 * is allowed to operate changes on a lease, the server is
		 * allowed to operate on a lease from the standpoint of the
		 * most conservative guess of the peer's state for this lease.
		 */
		switch (lease->binding_state) {
		      case FTS_ACTIVE:
			/* ACTIVE leases may not be reallocated. */
			return 0;

		      case FTS_FREE:
		      case FTS_ABANDONED:
			/* FREE leases may only be allocated by the primary,
			 * unless the secondary is acting in partner_down
			 * state and stos+mclt or tsfp+mclt has expired,
			 * whichever is greater.
			 *
			 * ABANDONED are treated the same as FREE for all
			 * purposes here.  Note that servers will only try
			 * for ABANDONED leases as a last resort anyway.
			 */
			if (peer -> i_am == primary)
				return 1;

			return(peer->service_state == service_partner_down &&
			       ((lease->tsfp < peer->me.stos) ?
				(peer->me.stos + peer->mclt < cur_time) :
				(lease->tsfp + peer->mclt < cur_time)));

		      case FTS_RELEASED:
		      case FTS_EXPIRED:
			/*
			 * These leases are generally untouchable until the
			 * peer acknowledges their state change.  However, as
			 * this is impossible if the peer is offline, the
			 * failover protocol permits an 'optimization' to
			 * rewind the lease to a previous state that the server
			 * is allowed to operate on, if that was the state that
			 * was last acknowledged by the peer.
			 *
			 * So if a lease was free, was allocated by this
			 * server, and expired without ever being transmitted
			 * to the peer, it can be returned to free and given
			 * to any new client legally.
			 */
			if ((peer->i_am == primary) &&
			    (lease->rewind_binding_state == FTS_FREE))
				return 1;
			if ((peer->i_am == secondary) &&
			    (lease->rewind_binding_state == FTS_BACKUP))
				return 1;

			/* FALL THROUGH (released, expired, reset) */
		      case FTS_RESET:
			/*
			 * Released, expired, and reset leases go onto the
			 * 'expired' queue all together.  Upon entry into
			 * partner-down state, this queue of leases has their
			 * tsfp values modified to equal stos+mclt, the point
			 * at which the server is allowed to remove them from
			 * these transitional states.
			 *
			 * Note that although tsfp has been possibly extended
			 * past the actual tsfp we received from the peer, we
			 * don't have to take any special action.  Since tsfp
			 * will be equal to the current time when the lease
			 * transitions to free, tsfp will not be used to grant
			 * lease-times longer than the MCLT to clients, which
			 * is the only danger for this sort of modification.
			 */
			return((peer->service_state == service_partner_down) &&
			       (lease->tsfp < cur_time));

		      case FTS_BACKUP:
			/* Only the secondary may allocate BACKUP leases,
			 * unless in partner_down state in which case at
			 * least TSFP+MCLT or STOS+MCLT must have expired,
			 * whichever is greater.
			 */
			if (peer->i_am == secondary)
				return 1;

			return((peer->service_state == service_partner_down) &&
			       ((lease->tsfp < peer->me.stos) ?
				(peer->me.stos + peer->mclt < cur_time) :
				(lease->tsfp + peer->mclt < cur_time)));

		      default:
			/* All lease states appear above. */
			log_fatal("Impossible case at %s:%d.", MDL);
			break;
		}
		return 0;
	}
	if (lease)
		return(lease->binding_state == FTS_FREE ||
		       lease->binding_state == FTS_BACKUP);
	else
		return 0;
}

static isc_result_t failover_message_reference (failover_message_t **mp,
						failover_message_t *m,
						const char *file, int line)
{
	*mp = m;
	m -> refcnt++;
	return ISC_R_SUCCESS;
}

static isc_result_t failover_message_dereference (failover_message_t **mp,
						  const char *file, int line)
{
	failover_message_t *m;
	m = (*mp);
	m -> refcnt--;
	if (m -> refcnt == 0) {
		if (m -> next)
			failover_message_dereference (&m -> next,
						      file, line);
		if (m -> chaddr.data)
			dfree (m -> chaddr.data, file, line);
		if (m -> client_identifier.data)
			dfree (m -> client_identifier.data, file, line);
		if (m -> hba.data)
			dfree (m -> hba.data, file, line);
		if (m -> message.data)
			dfree (m -> message.data, file, line);
		if (m -> relationship_name.data)
			dfree (m -> relationship_name.data, file, line);
		if (m -> reply_options.data)
			dfree (m -> reply_options.data, file, line);
		if (m -> request_options.data)
			dfree (m -> request_options.data, file, line);
		if (m -> vendor_class.data)
			dfree (m -> vendor_class.data, file, line);
		if (m -> vendor_options.data)
			dfree (m -> vendor_options.data, file, line);
		if (m -> ddns.data)
			dfree (m -> ddns.data, file, line);
		dfree (*mp, file, line);
	}
	*mp = 0;
	return ISC_R_SUCCESS;
}

OMAPI_OBJECT_ALLOC (dhcp_failover_state, dhcp_failover_state_t,
		    dhcp_type_failover_state)
OMAPI_OBJECT_ALLOC (dhcp_failover_listener, dhcp_failover_listener_t,
		    dhcp_type_failover_listener)
OMAPI_OBJECT_ALLOC (dhcp_failover_link, dhcp_failover_link_t,
		    dhcp_type_failover_link)
#endif /* defined (FAILOVER_PROTOCOL) */

const char *binding_state_print (enum failover_state state)
{
	switch (state) {
	      case FTS_FREE:
		return "free";
		break;

	      case FTS_ACTIVE:
		return "active";
		break;

	      case FTS_EXPIRED:
		return "expired";
		break;

	      case FTS_RELEASED:
		return "released";
		break;

	      case FTS_ABANDONED:
		return "abandoned";
		break;

	      case FTS_RESET:
		return "reset";
		break;

	      case FTS_BACKUP:
		return "backup";
		break;

	      default:
		return "unknown";
		break;
	}
}
