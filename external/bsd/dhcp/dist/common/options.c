/*	$NetBSD: options.c,v 1.1.1.3 2014/07/12 11:57:46 spz Exp $	*/
/* options.c

   DHCP options parsing and reassembly. */

/*
 * Copyright (c) 2004-2012,2014 by Internet Systems Consortium, Inc. ("ISC")
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
__RCSID("$NetBSD: options.c,v 1.1.1.3 2014/07/12 11:57:46 spz Exp $");

#define DHCP_OPTION_DATA
#include "dhcpd.h"
#include <omapip/omapip_p.h>
#include <limits.h>

struct option *vendor_cfg_option;

static int pretty_text(char **, char *, const unsigned char **,
			 const unsigned char *, int);
static int pretty_domain(char **, char *, const unsigned char **,
			 const unsigned char *);
static int prepare_option_buffer(struct universe *universe, struct buffer *bp,
				 unsigned char *buffer, unsigned length,
				 unsigned code, int terminatep,
				 struct option_cache **opp);

/* Parse all available options out of the specified packet. */

int parse_options (packet)
	struct packet *packet;
{
	struct option_cache *op = (struct option_cache *)0;

	/* Allocate a new option state. */
	if (!option_state_allocate (&packet -> options, MDL)) {
		packet -> options_valid = 0;
		return 0;
	}

	/* If we don't see the magic cookie, there's nothing to parse. */
	if (memcmp (packet -> raw -> options, DHCP_OPTIONS_COOKIE, 4)) {
		packet -> options_valid = 0;
		return 1;
	}

	/* Go through the options field, up to the end of the packet
	   or the End field. */
	if (!parse_option_buffer (packet -> options,
				  &packet -> raw -> options [4],
				  (packet -> packet_length -
				   DHCP_FIXED_NON_UDP - 4),
				  &dhcp_universe)) {

		/* STSN servers have a bug where they send a mangled
		   domain-name option, and whatever is beyond that in
		   the packet is junk.   Microsoft clients accept this,
		   which is probably why whoever implemented the STSN
		   server isn't aware of the problem yet.   To work around
		   this, we will accept corrupt packets from the server if
		   they contain a valid DHCP_MESSAGE_TYPE option, but
		   will not accept any corrupt client packets (the ISC DHCP
		   server is sufficiently widely used that it is probably
		   beneficial for it to be picky) and will not accept
		   packets whose type can't be determined. */

		if ((op = lookup_option (&dhcp_universe, packet -> options,
					 DHO_DHCP_MESSAGE_TYPE))) {
			if (!op -> data.data ||
			    (op -> data.data [0] != DHCPOFFER &&
			     op -> data.data [0] != DHCPACK &&
			     op -> data.data [0] != DHCPNAK))
				return 0;
		} else
			return 0;
	}

	/* If we parsed a DHCP Option Overload option, parse more
	   options out of the buffer(s) containing them. */
	if ((op = lookup_option (&dhcp_universe, packet -> options,
				 DHO_DHCP_OPTION_OVERLOAD))) {
		if (op -> data.data [0] & 1) {
			if (!parse_option_buffer
			    (packet -> options,
			     (unsigned char *)packet -> raw -> file,
			     sizeof packet -> raw -> file,
			     &dhcp_universe))
				return 0;
		}
		if (op -> data.data [0] & 2) {
			if (!parse_option_buffer
			    (packet -> options,
			     (unsigned char *)packet -> raw -> sname,
			     sizeof packet -> raw -> sname,
			     &dhcp_universe))
				return 0;
		}
	}
	packet -> options_valid = 1;
	return 1;
}

/* Parse options out of the specified buffer, storing addresses of option
 * values in packet->options.
 */
int parse_option_buffer (options, buffer, length, universe)
	struct option_state *options;
	const unsigned char *buffer;
	unsigned length;
	struct universe *universe;
{
	unsigned len, offset;
	unsigned code;
	struct option_cache *op = NULL, *nop = NULL;
	struct buffer *bp = (struct buffer *)0;
	struct option *option = NULL;
	char *reason = "general failure";

	if (!buffer_allocate (&bp, length, MDL)) {
		log_error ("no memory for option buffer.");
		return 0;
	}
	memcpy (bp -> data, buffer, length);

	for (offset = 0;
	     (offset + universe->tag_size) <= length &&
	     (code = universe->get_tag(buffer + offset)) != universe->end; ) {
		offset += universe->tag_size;

		/* Pad options don't have a length - just skip them. */
		if (code == DHO_PAD)
			continue;

		/* Don't look for length if the buffer isn't that big. */
		if ((offset + universe->length_size) > length) {
			reason = "code tag at end of buffer - missing "
				 "length field";
			goto bogus;
		}

		/* All other fields (except PAD and END handled above)
		 * have a length field, unless it's a DHCPv6 zero-length
		 * options space (eg any of the enterprise-id'd options).
		 *
		 * Zero-length-size option spaces basically consume the
		 * entire options buffer, so have at it.
		 */
		if (universe->get_length != NULL)
			len = universe->get_length(buffer + offset);
		else if (universe->length_size == 0)
			len = length - universe->tag_size;
		else {
			log_fatal("Improperly configured option space(%s): "
				  "may not have a nonzero length size "
				  "AND a NULL get_length function.",
				  universe->name);

			/* Silence compiler warnings. */
			return 0;
		}

		offset += universe->length_size;

		option_code_hash_lookup(&option, universe->code_hash, &code,
					0, MDL);

		/* If the length is outrageous, the options are bad. */
		if (offset + len > length) {
			reason = "option length exceeds option buffer length";
		      bogus:
			log_error("parse_option_buffer: malformed option "
				  "%s.%s (code %u): %s.", universe->name,
				  option ? option->name : "<unknown>",
				  code, reason);
			buffer_dereference (&bp, MDL);
			return 0;
		}

		/* If the option contains an encapsulation, parse it.   If
		   the parse fails, or the option isn't an encapsulation (by
		   far the most common case), or the option isn't entirely
		   an encapsulation, keep the raw data as well. */
		if (!(option &&
		      (option->format[0] == 'e' ||
		       option->format[0] == 'E') &&
		      (parse_encapsulated_suboptions(options, option,
						     bp->data + offset, len,
						     universe, NULL)))) {
			op = lookup_option(universe, options, code);

			if (op != NULL && universe->concat_duplicates) {
				struct data_string new;
				memset(&new, 0, sizeof new);
				if (!buffer_allocate(&new.buffer,
						     op->data.len + len,
						     MDL)) {
					log_error("parse_option_buffer: "
						  "No memory.");
					buffer_dereference(&bp, MDL);
					return 0;
				}
				/* Copy old option to new data object. */
				memcpy(new.buffer->data, op->data.data,
					op->data.len);
				/* Concat new option behind old. */
				memcpy(new.buffer->data + op->data.len,
					bp->data + offset, len);
				new.len = op->data.len + len;
				new.data = new.buffer->data;
				/* Save new concat'd object. */
				data_string_forget(&op->data, MDL);
				data_string_copy(&op->data, &new, MDL);
				data_string_forget(&new, MDL);
			} else if (op != NULL) {
				/* We must append this statement onto the
				 * end of the list.
				 */
				while (op->next != NULL)
					op = op->next;

				if (!option_cache_allocate(&nop, MDL)) {
					log_error("parse_option_buffer: "
						  "No memory.");
					buffer_dereference(&bp, MDL);
					return 0;
				}

				option_reference(&nop->option, op->option, MDL);

				nop->data.buffer = NULL;
				buffer_reference(&nop->data.buffer, bp, MDL);
				nop->data.data = bp->data + offset;
				nop->data.len = len;

				option_cache_reference(&op->next, nop, MDL);
				option_cache_dereference(&nop, MDL);
			} else {
				if (save_option_buffer(universe, options, bp,
						       bp->data + offset, len,
						       code, 1) == 0) {
					log_error("parse_option_buffer: "
						  "save_option_buffer failed");
					buffer_dereference(&bp, MDL);
					return 0;
				}
			}
		}
		option_dereference(&option, MDL);
		offset += len;
	}
	buffer_dereference (&bp, MDL);
	return 1;
}

/* If an option in an option buffer turns out to be an encapsulation,
   figure out what to do.   If we don't know how to de-encapsulate it,
   or it's not well-formed, return zero; otherwise, return 1, indicating
   that we succeeded in de-encapsulating it. */

struct universe *find_option_universe (struct option *eopt, const char *uname)
{
	int i;
	char *s, *t;
	struct universe *universe = (struct universe *)0;

	/* Look for the E option in the option format. */
	s = strchr (eopt -> format, 'E');
	if (!s) {
		log_error ("internal encapsulation format error 1.");
		return 0;
	}
	/* Look for the universe name in the option format. */
	t = strchr (++s, '.');
	/* If there was no trailing '.', or there's something after the
	   trailing '.', the option is bogus and we can't use it. */
	if (!t || t [1]) {
		log_error ("internal encapsulation format error 2.");
		return 0;
	}
	if (t == s && uname) {
		for (i = 0; i < universe_count; i++) {
			if (!strcmp (universes [i] -> name, uname)) {
				universe = universes [i];
				break;
			}
		}
	} else if (t != s) {
		for (i = 0; i < universe_count; i++) {
			if (strlen (universes [i] -> name) == t - s &&
			    !memcmp (universes [i] -> name,
				     s, (unsigned)(t - s))) {
				universe = universes [i];
				break;
			}
		}
	}
	return universe;
}

/* If an option in an option buffer turns out to be an encapsulation,
   figure out what to do.   If we don't know how to de-encapsulate it,
   or it's not well-formed, return zero; otherwise, return 1, indicating
   that we succeeded in de-encapsulating it. */

int parse_encapsulated_suboptions (struct option_state *options,
				   struct option *eopt,
				   const unsigned char *buffer,
				   unsigned len, struct universe *eu,
				   const char *uname)
{
	int i;
	struct universe *universe = find_option_universe (eopt, uname);

	/* If we didn't find the universe, we can't do anything with it
	   right now (e.g., we can't decode vendor options until we've
	   decoded the packet and executed the scopes that it matches). */
	if (!universe)
		return 0;
		
	/* If we don't have a decoding function for it, we can't decode
	   it. */
	if (!universe -> decode)
		return 0;

	i = (*universe -> decode) (options, buffer, len, universe);

	/* If there is stuff before the suboptions, we have to keep it. */
	if (eopt -> format [0] != 'E')
		return 0;
	/* Otherwise, return the status of the decode function. */
	return i;
}

int fqdn_universe_decode (struct option_state *options,
			  const unsigned char *buffer,
			  unsigned length, struct universe *u)
{
	struct buffer *bp = (struct buffer *)0;

	/* FQDN options have to be at least four bytes long. */
	if (length < 3)
		return 0;

	/* Save the contents of the option in a buffer. */
	if (!buffer_allocate (&bp, length + 4, MDL)) {
		log_error ("no memory for option buffer.");
		return 0;
	}
	memcpy (&bp -> data [3], buffer + 1, length - 1);

	if (buffer [0] & 4)	/* encoded */
		bp -> data [0] = 1;
	else
		bp -> data [0] = 0;
	if (!save_option_buffer(&fqdn_universe, options, bp,
				bp->data, 1, FQDN_ENCODED, 0)) {
	      bad:
		buffer_dereference (&bp, MDL);
		return 0;
	}

	if (buffer [0] & 1)	/* server-update */
		bp -> data [2] = 1;
	else
		bp -> data [2] = 0;
	if (buffer [0] & 2)	/* no-client-update */
		bp -> data [1] = 1;
	else
		bp -> data [1] = 0;

	/* XXX Ideally we should store the name in DNS format, so if the
	   XXX label isn't in DNS format, we convert it to DNS format,
	   XXX rather than converting labels specified in DNS format to
	   XXX the plain ASCII representation.   But that's hard, so
	   XXX not now. */

	/* Not encoded using DNS format? */
	if (!bp -> data [0]) {
		unsigned i;

		/* Some broken clients NUL-terminate this option. */
		if (buffer [length - 1] == 0) {
			--length;
			bp -> data [1] = 1;
		}

		/* Determine the length of the hostname component of the
		   name.  If the name contains no '.' character, it
		   represents a non-qualified label. */
		for (i = 3; i < length && buffer [i] != '.'; i++);
		i -= 3;

		/* Note: If the client sends a FQDN, the first '.' will
		   be used as a NUL terminator for the hostname. */
		if (i && (!save_option_buffer(&fqdn_universe, options, bp,
					      &bp->data[5], i,
					      FQDN_HOSTNAME, 0)))
			goto bad;
		/* Note: If the client sends a single label, the
		   FQDN_DOMAINNAME option won't be set. */
		if (length > 4 + i &&
		    (!save_option_buffer(&fqdn_universe, options, bp,
					 &bp -> data[6 + i], length - 4 - i,
					 FQDN_DOMAINNAME, 1)))
			goto bad;
		/* Also save the whole name. */
		if (length > 3) {
			if (!save_option_buffer(&fqdn_universe, options, bp,
						&bp -> data [5], length - 3,
						FQDN_FQDN, 1))
				goto bad;
		}
	} else {
		unsigned len;
		unsigned total_len = 0;
		unsigned first_len = 0;
		int terminated = 0;
		unsigned char *s;

		s = &bp -> data[5];

		while (s < &bp -> data[0] + length + 2) {
			len = *s;
			if (len > 63) {
				log_info ("fancy bits in fqdn option");
				return 0;
			}	
			if (len == 0) {
				terminated = 1;
				break;
			}
			if (s + len > &bp -> data [0] + length + 3) {
				log_info ("fqdn tag longer than buffer");
				return 0;
			}

			if (first_len == 0) {
				first_len = len;
			}

			*s = '.';
			s += len + 1;
			total_len += len + 1;
		}

		/* We wind up with a length that's one too many because
		   we shouldn't increment for the last label, but there's
		   no way to tell we're at the last label until we exit
		   the loop.   :'*/
		if (total_len > 0)
			total_len--;

		if (!terminated) {
			first_len = total_len;
		}

		if (first_len > 0 &&
		    !save_option_buffer(&fqdn_universe, options, bp,
					&bp -> data[6], first_len,
					FQDN_HOSTNAME, 0))
			goto bad;
		if (total_len > 0 && first_len != total_len) {
			if (!save_option_buffer(&fqdn_universe, options, bp,
						&bp->data[6 + first_len],
						total_len - first_len,
						FQDN_DOMAINNAME, 1))
				goto bad;
		}
		if (total_len > 0)
			if (!save_option_buffer (&fqdn_universe, options, bp,
						 &bp -> data [6], total_len,
						 FQDN_FQDN, 1))
				goto bad;
	}

	if (!save_option_buffer (&fqdn_universe, options, bp,
				 &bp -> data [1], 1,
				 FQDN_NO_CLIENT_UPDATE, 0))
	    goto bad;
	if (!save_option_buffer (&fqdn_universe, options, bp,
				 &bp -> data [2], 1,
				 FQDN_SERVER_UPDATE, 0))
		goto bad;

	if (!save_option_buffer (&fqdn_universe, options, bp,
				 &bp -> data [3], 1,
				 FQDN_RCODE1, 0))
		goto bad;
	if (!save_option_buffer (&fqdn_universe, options, bp,
				 &bp -> data [4], 1,
				 FQDN_RCODE2, 0))
		goto bad;

	buffer_dereference (&bp, MDL);
	return 1;
}

/*
 * Load all options into a buffer, and then split them out into the three
 * separate fields in the dhcp packet (options, file, and sname) where
 * options can be stored.
 *
 * returns 0 on error, length of packet on success
 */
int
cons_options(struct packet *inpacket, struct dhcp_packet *outpacket,
	     struct lease *lease, struct client_state *client_state,
	     int mms, struct option_state *in_options,
	     struct option_state *cfg_options,
	     struct binding_scope **scope,
	     int overload_avail, int terminate, int bootpp,
	     struct data_string *prl, const char *vuname)
{
#define PRIORITY_COUNT 300
	unsigned priority_list[PRIORITY_COUNT];
	int priority_len;
	unsigned char buffer[4096], agentopts[1024];
	unsigned index = 0;
	unsigned mb_size = 0, mb_max = 0;
	unsigned option_size = 0, agent_size = 0;
	unsigned length;
	int i;
	struct option_cache *op;
	struct data_string ds;
	pair pp, *hash;
	int overload_used = 0;
	int of1 = 0, of2 = 0;

	memset(&ds, 0, sizeof ds);

	/*
	 * If there's a Maximum Message Size option in the incoming packet
	 * and no alternate maximum message size has been specified, or
	 * if the one specified in the packet is shorter than the
	 * alternative, take the one in the packet.
	 */

	if (inpacket &&
	    (op = lookup_option(&dhcp_universe, inpacket->options,
				DHO_DHCP_MAX_MESSAGE_SIZE)) &&
	    (evaluate_option_cache(&ds, inpacket, lease,
				   client_state, in_options,
				   cfg_options, scope, op, MDL) != 0)) {
		if (ds.len >= sizeof (u_int16_t)) {
			i = getUShort(ds.data);
			if(!mms || (i < mms))
				mms = i;
		}
		data_string_forget(&ds, MDL);
	}

	/*
	 * If the client has provided a maximum DHCP message size,
	 * use that, up to the MTU limit.  Otherwise, if it's BOOTP,
	 * only 64 bytes; otherwise use up to the minimum IP MTU size
	 * (576 bytes).
	 *
	 * XXX if a BOOTP client specifies a max message size, we will
	 * honor it.
	 */
	if (mms) {
		if (mms < DHCP_MTU_MIN)
		        /* Enforce minimum packet size, per RFC 2132 */
			mb_size = DHCP_MIN_OPTION_LEN;
		else if (mms > DHCP_MTU_MAX)
			/*
			 * TODO: Packets longer than 1500 bytes really
			 * should be allowed, but it requires upstream
			 * changes to the way the packet is allocated.  For
			 * now, we forbid them.  They won't be needed very
			 * often anyway.
			 */
			mb_size = DHCP_MAX_OPTION_LEN;
		else
			mb_size = mms - DHCP_FIXED_LEN;
	} else if (bootpp) {
		mb_size = 64;
		if (inpacket != NULL &&
		    (inpacket->packet_length >= 64 + DHCP_FIXED_NON_UDP))
			mb_size = inpacket->packet_length - DHCP_FIXED_NON_UDP;
	} else
		mb_size = DHCP_MIN_OPTION_LEN;

	/*
	 * If answering a client message, see whether any relay agent
	 * options were included with the message.  If so, save them
	 * to copy back in later, and make space in the main buffer
	 * to accommodate them
	 */
	if (client_state == NULL) {
		priority_list[0] = DHO_DHCP_AGENT_OPTIONS;
		priority_len = 1;
		agent_size = store_options(NULL, agentopts, 0,
					   sizeof(agentopts),
					   inpacket, lease, client_state,
					   in_options, cfg_options, scope,
					   priority_list, priority_len,
					   0, 0, 0, NULL);

		mb_size += agent_size;
		if (mb_size > DHCP_MAX_OPTION_LEN)
			mb_size = DHCP_MAX_OPTION_LEN;
	}

	/*
	 * Set offsets for buffer data to be copied into filename
	 * and servername fields 
	 */
	mb_max = mb_size;

	if (overload_avail & 1) {
		of1 = mb_max;
		mb_max += DHCP_FILE_LEN;
	}

	if (overload_avail & 2) {
		of2 = mb_max;
		mb_max += DHCP_SNAME_LEN;
	}
		
	/*
	 * Preload the option priority list with protocol-mandatory options.
	 * This effectively gives these options the highest priority.
	 * This provides the order for any available options, the option
	 * must be in the option cache in order to actually be included.
	 */
	priority_len = 0;
	priority_list[priority_len++] = DHO_DHCP_MESSAGE_TYPE;
	priority_list[priority_len++] = DHO_DHCP_SERVER_IDENTIFIER;
	priority_list[priority_len++] = DHO_DHCP_LEASE_TIME;
	priority_list[priority_len++] = DHO_DHCP_RENEWAL_TIME;
	priority_list[priority_len++] = DHO_DHCP_REBINDING_TIME;
	priority_list[priority_len++] = DHO_DHCP_MESSAGE;
	priority_list[priority_len++] = DHO_DHCP_REQUESTED_ADDRESS;
	priority_list[priority_len++] = DHO_ASSOCIATED_IP;

	if (prl != NULL && prl->len > 0) {
		if ((op = lookup_option(&dhcp_universe, cfg_options,
					 DHO_SUBNET_SELECTION))) {
			if (priority_len < PRIORITY_COUNT)
				priority_list[priority_len++] =
					DHO_SUBNET_SELECTION;
		}

		data_string_truncate(prl, (PRIORITY_COUNT - priority_len));

		/*
		 * Copy the client's PRL onto the priority_list after our high
		 * priority header.
		 */
		for (i = 0; i < prl->len; i++) {
			/*
			 * Prevent client from changing order of delivery
			 * of relay agent information option.
			 */
			if (prl->data[i] != DHO_DHCP_AGENT_OPTIONS)
				priority_list[priority_len++] = prl->data[i];
		}

		/*
		 * If the client doesn't request the FQDN option explicitly,
		 * to indicate priority, consider it lowest priority.  Fit
		 * in the packet if there is space.  Note that the option
		 * may only be included if the client supplied one.
		 */
		if ((inpacket != NULL) && (priority_len < PRIORITY_COUNT) &&
		    (lookup_option(&fqdn_universe, inpacket->options,
				   FQDN_ENCODED) != NULL))
			priority_list[priority_len++] = DHO_FQDN;

		/*
		 * Some DHCP Servers will give the subnet-mask option if
		 * it is not on the parameter request list - so some client
		 * implementations have come to rely on this - so we will
		 * also make sure we supply this, at lowest priority.
		 *
		 * This is only done in response to DHCPDISCOVER or
		 * DHCPREQUEST messages, to avoid providing the option on
		 * DHCPINFORM or DHCPLEASEQUERY responses (if the client
		 * didn't request it).
		 */
		if ((inpacket != NULL) && (priority_len < PRIORITY_COUNT) &&
		    ((inpacket->packet_type == DHCPDISCOVER) ||
		     (inpacket->packet_type == DHCPREQUEST)))
			priority_list[priority_len++] = DHO_SUBNET_MASK;
	} else {
		/*
		 * First, hardcode some more options that ought to be
		 * sent first...these are high priority to have in the
		 * packet.
		 */
		priority_list[priority_len++] = DHO_SUBNET_MASK;
		priority_list[priority_len++] = DHO_ROUTERS;
		priority_list[priority_len++] = DHO_DOMAIN_NAME_SERVERS;
		priority_list[priority_len++] = DHO_HOST_NAME;
		priority_list[priority_len++] = DHO_FQDN;

		/*
		 * Append a list of the standard DHCP options from the
		 * standard DHCP option space.  Actually, if a site
		 * option space hasn't been specified, we wind up
		 * treating the dhcp option space as the site option
		 * space, and the first for loop is skipped, because
		 * it's slightly more general to do it this way,
		 * taking the 1Q99 DHCP futures work into account.
		 */
		if (cfg_options->site_code_min) {
		    for (i = 0; i < OPTION_HASH_SIZE; i++) {
			hash = cfg_options->universes[dhcp_universe.index];
			if (hash) {
			    for (pp = hash[i]; pp; pp = pp->cdr) {
				op = (struct option_cache *)(pp->car);
				if (op->option->code <
				     cfg_options->site_code_min &&
				    priority_len < PRIORITY_COUNT &&
				    op->option->code != DHO_DHCP_AGENT_OPTIONS)
					priority_list[priority_len++] =
						op->option->code;
			    }
			}
		    }
		}

		/*
		 * Now cycle through the site option space, or if there
		 * is no site option space, we'll be cycling through the
		 * dhcp option space.
		 */
		for (i = 0; i < OPTION_HASH_SIZE; i++) {
		    hash = cfg_options->universes[cfg_options->site_universe];
		    if (hash != NULL)
			for (pp = hash[i]; pp; pp = pp->cdr) {
				op = (struct option_cache *)(pp->car);
				if (op->option->code >=
				     cfg_options->site_code_min &&
				    priority_len < PRIORITY_COUNT &&
				    op->option->code != DHO_DHCP_AGENT_OPTIONS)
					priority_list[priority_len++] =
						op->option->code;
			}
		}

		/*
		 * Put any spaces that are encapsulated on the list,
		 * sort out whether they contain values later.
		 */
		for (i = 0; i < cfg_options->universe_count; i++) {
		    if (universes[i]->enc_opt &&
			priority_len < PRIORITY_COUNT &&
			universes[i]->enc_opt->universe == &dhcp_universe) {
			    if (universes[i]->enc_opt->code !=
				DHO_DHCP_AGENT_OPTIONS)
				    priority_list[priority_len++] =
					    universes[i]->enc_opt->code;
		    }
		}

		/*
		 * The vendor option space can't stand on its own, so always
		 * add it to the list.
		 */
		if (priority_len < PRIORITY_COUNT)
			priority_list[priority_len++] =
				DHO_VENDOR_ENCAPSULATED_OPTIONS;
	}

	/* Put the cookie up front... */
	memcpy(buffer, DHCP_OPTIONS_COOKIE, 4);
	index += 4;

	/* Copy the options into the big buffer... */
	option_size = store_options(&overload_used, buffer, index, mb_max,
				    inpacket, lease, client_state,
				    in_options, cfg_options, scope,
				    priority_list, priority_len,
				    of1, of2, terminate, vuname);

	/* If store_options() failed */
	if (option_size == 0)
		return 0;

	/* How much was stored in the main buffer? */
	index += option_size;

	/*
	 * If we're going to have to overload, store the overload
	 * option first.
	 */
	if (overload_used) {
		if (mb_size - agent_size - index < 3)
			return 0;

		buffer[index++] = DHO_DHCP_OPTION_OVERLOAD;
		buffer[index++] = 1;
		buffer[index++] = overload_used;

		if (overload_used & 1)
			memcpy(outpacket->file, &buffer[of1], DHCP_FILE_LEN);

		if (overload_used & 2)
			memcpy(outpacket->sname, &buffer[of2], DHCP_SNAME_LEN);
	}

	/* Now copy in preserved agent options, if any */
	if (agent_size) {
		if (mb_size - index >= agent_size) {
			memcpy(&buffer[index], agentopts, agent_size);
			index += agent_size;
		} else
			log_error("Unable to store relay agent information "
				  "in reply packet.");
	}

	/* Tack a DHO_END option onto the packet if we need to. */
	if (index < mb_size)
		buffer[index++] = DHO_END;

	/* Copy main buffer into the options buffer of the packet */
	memcpy(outpacket->options, buffer, index);

	/* Figure out the length. */
	length = DHCP_FIXED_NON_UDP + index;
	return length;
}

/*
 * XXX: We currently special case collecting VSIO options.
 *      We should be able to handle this in a more generic fashion, by
 *      including any encapsulated options that are present and desired.
 *      This will look something like the VSIO handling VSIO code.
 *      We may also consider handling the ORO-like options within
 *      encapsulated spaces.
 */

struct vsio_state {
	char *buf;
	int buflen;
	int bufpos;
};

static void
vsio_options(struct option_cache *oc,
	     struct packet *packet,
	     struct lease *dummy_lease, 
	     struct client_state *dummy_client_state,
	     struct option_state *dummy_opt_state,
	     struct option_state *opt_state,
	     struct binding_scope **dummy_binding_scope,
	     struct universe *universe, 
	     void *void_vsio_state) {
	struct vsio_state *vs = (struct vsio_state *)void_vsio_state;
	struct data_string ds;
	int total_len;

	memset(&ds, 0, sizeof(ds));
	if (evaluate_option_cache(&ds, packet, NULL,
				  NULL, opt_state, NULL, 
				  &global_scope, oc, MDL)) {
		total_len = ds.len + universe->tag_size + universe->length_size;
		if (total_len <= (vs->buflen - vs->bufpos)) {
			if (universe->tag_size == 1) {
				vs->buf[vs->bufpos++] = oc->option->code;
			} else if (universe->tag_size == 2) {
				putUShort((unsigned char *)vs->buf+vs->bufpos,
					  oc->option->code);
				vs->bufpos += 2;
			} else if (universe->tag_size == 4) {
				putULong((unsigned char *)vs->buf+vs->bufpos,
					 oc->option->code);
				vs->bufpos += 4;
			}
			if (universe->length_size == 1) {
				vs->buf[vs->bufpos++] = ds.len;
			} else if (universe->length_size == 2) {
				putUShort((unsigned char *)vs->buf+vs->bufpos, 
					  ds.len);
				vs->bufpos += 2;
			} else if (universe->length_size == 4) {
				putULong((unsigned char *)vs->buf+vs->bufpos, 
					 ds.len);
				vs->bufpos += 4;
			}
			memcpy(vs->buf + vs->bufpos, ds.data, ds.len);
			vs->bufpos += ds.len;
		} else {
			log_debug("No space for option %d in VSIO space %s.",
		  		oc->option->code, universe->name);
		}
		data_string_forget(&ds, MDL);
	} else {
		log_error("Error evaluating option %d in VSIO space %s.",
		  	oc->option->code, universe->name);
	}
}

/*
 * Stores the options from the DHCPv6 universe into the buffer given.
 *
 * Required options are given as a 0-terminated list of option codes.
 * Once those are added, the ORO is consulted.
 */

int
store_options6(char *buf, int buflen, 
	       struct option_state *opt_state, 
	       struct packet *packet,
	       const int *required_opts,
	       struct data_string *oro) {
	int i, j;
	struct option_cache *oc;
	struct option *o;
	struct data_string ds;
	int bufpos;
	int oro_size;
	u_int16_t code;
	int in_required_opts;
	int vsio_option_code;
	int vsio_wanted;
	struct vsio_state vs;
	unsigned char *tmp;

	bufpos = 0;
	vsio_wanted = 0;

	/*
	 * Find the option code for the VSIO universe.
	 */
	vsio_option_code = 0;
	o = vsio_universe.enc_opt;
	while (o != NULL) { 
		if (o->universe == &dhcpv6_universe) {
			vsio_option_code = o->code;
			break;
		} 
		o = o->universe->enc_opt;
	}
	if (vsio_option_code == 0) {
		log_fatal("No VSIO option code found.");
	}

	if (required_opts != NULL) {
		for (i=0; required_opts[i] != 0; i++) {
			if (required_opts[i] == vsio_option_code) {
				vsio_wanted = 1;
			}

			oc = lookup_option(&dhcpv6_universe, 
					   opt_state, required_opts[i]);
			if (oc == NULL) {
				continue;
			}
			memset(&ds, 0, sizeof(ds));
			for (; oc != NULL ; oc = oc->next) {
				if (evaluate_option_cache(&ds, packet, NULL,
							  NULL, opt_state,
							  NULL, &global_scope,
							  oc, MDL)) {
					if ((ds.len + 4) <=
					    (buflen - bufpos)) {
						tmp = (unsigned char *)buf;
						tmp += bufpos;
						/* option tag */
						putUShort(tmp,
							  required_opts[i]);
						/* option length */
						putUShort(tmp+2, ds.len);
						/* option data */
						memcpy(tmp+4, ds.data, ds.len);
						/* update position */
						bufpos += (4 + ds.len);
					} else {
						log_debug("No space for "
							  "option %d",
							  required_opts[i]);
					}
					data_string_forget(&ds, MDL);
				} else {
					log_error("Error evaluating option %d",
					  	required_opts[i]);
				}
			}
		}
	}

	if (oro == NULL) {
		oro_size = 0;
	} else {
		oro_size = oro->len / 2;
	}
	for (i=0; i<oro_size; i++) {
		memcpy(&code, oro->data+(i*2), 2);
		code = ntohs(code);

		/* 
		 * See if we've already included this option because
		 * it is required.
		 */
		in_required_opts = 0;
		if (required_opts != NULL) {
			for (j=0; required_opts[j] != 0; j++) {
				if (required_opts[j] == code) {
					in_required_opts = 1;
					break;
				}
			}
		}
		if (in_required_opts) {
			continue;
		}

		/*
		 * See if this is the VSIO option.
		 */
		if (code == vsio_option_code) {
			vsio_wanted = 1;
		}

		/* 
		 * Not already added, find this option.
		 */
		oc = lookup_option(&dhcpv6_universe, opt_state, code);
		memset(&ds, 0, sizeof(ds));
		for (; oc != NULL ; oc = oc->next) {
			if (evaluate_option_cache(&ds, packet, NULL, NULL,
						  opt_state, NULL,
						  &global_scope, oc, MDL)) {
				if ((ds.len + 4) <= (buflen - bufpos)) {
					tmp = (unsigned char *)buf + bufpos;
					/* option tag */
					putUShort(tmp, code);
					/* option length */
					putUShort(tmp+2, ds.len);
					/* option data */
					memcpy(tmp+4, ds.data, ds.len);
					/* update position */
					bufpos += (4 + ds.len);
				} else {
					log_debug("No space for option %d",
						  code);
				}
				data_string_forget(&ds, MDL);
			} else {
				log_error("Error evaluating option %d", code);
			}
		}
	}

	if (vsio_wanted) {
		for (i=0; i < opt_state->universe_count; i++) {
			if (opt_state->universes[i] != NULL) {
		    		o = universes[i]->enc_opt;
				if ((o != NULL) && 
				    (o->universe == &vsio_universe)) {
					/*
					 * Add the data from this VSIO option.
					 */
					vs.buf = buf;
					vs.buflen = buflen;
					vs.bufpos = bufpos+8;
					option_space_foreach(packet, NULL,
							     NULL, 
							     NULL, opt_state,
			     				     NULL, 
							     universes[i], 
							     (void *)&vs,
			     				     vsio_options);

					/* 
					 * If there was actually data here,
					 * add the "header".
					 */
					if (vs.bufpos > bufpos+8) {
						tmp = (unsigned char *)buf +
						      bufpos;
						putUShort(tmp,
							  vsio_option_code);
						putUShort(tmp+2,
							  vs.bufpos-bufpos-4);
						putULong(tmp+4, o->code);

						bufpos = vs.bufpos;
					}
				}
			}
		}
	}

	return bufpos;
}

/*
 * Store all the requested options into the requested buffer.
 * XXX: ought to be static
 */
int
store_options(int *ocount,
	      unsigned char *buffer, unsigned index, unsigned buflen,
	      struct packet *packet, struct lease *lease,
	      struct client_state *client_state,
	      struct option_state *in_options,
	      struct option_state *cfg_options,
	      struct binding_scope **scope,
	      unsigned *priority_list, int priority_len,
	      unsigned first_cutoff, int second_cutoff, int terminate,
	      const char *vuname)
{
	int bufix = 0, six = 0, tix = 0;
	int i;
	int ix;
	int tto;
	int bufend, sbufend;
	struct data_string od;
	struct option_cache *oc;
	struct option *option = NULL;
	unsigned code;

	/*
	 * These arguments are relative to the start of the buffer, so 
	 * reduce them by the current buffer index, and advance the
	 * buffer pointer to where we're going to start writing.
	 */
	buffer = &buffer[index];
	buflen -= index;
	if (first_cutoff)
		first_cutoff -= index;
	if (second_cutoff)
		second_cutoff -= index;

	/* Calculate the start and end of each section of the buffer */
	bufend = sbufend = buflen;
	if (first_cutoff) {
	    if (first_cutoff >= buflen)
		log_fatal("%s:%d:store_options: Invalid first cutoff.", MDL);
	    bufend = first_cutoff;

	    if (second_cutoff) {
	        if (second_cutoff >= buflen)
		    log_fatal("%s:%d:store_options: Invalid second cutoff.",
			      MDL);
	        sbufend = second_cutoff;
	    }
	} else if (second_cutoff) {
	    if (second_cutoff >= buflen)
		log_fatal("%s:%d:store_options: Invalid second cutoff.", MDL);
	    bufend = second_cutoff;
	}

	memset (&od, 0, sizeof od);

	/* Eliminate duplicate options from the parameter request list.
	 * Enforce RFC-mandated ordering of options that are present.
	 */
	for (i = 0; i < priority_len - 1; i++) {
		/* Eliminate duplicates. */
		tto = 0;
		for (ix = i + 1; ix < priority_len + tto; ix++) {
			if (tto)
				priority_list [ix - tto] =
					priority_list [ix];
			if (priority_list [i] == priority_list [ix]) {
				tto++;
				priority_len--;
			}
		}

		/* Enforce ordering of SUBNET_MASK options, according to
		 * RFC2132 Section 3.3:
		 *
		 *   If both the subnet mask and the router option are
		 *   specified in a DHCP reply, the subnet mask option MUST
		 *   be first.
		 *
		 * This guidance does not specify what to do if the client
		 * PRL explicitly requests the options out of order, it is
		 * a general statement.
		 */
		if (priority_list[i] == DHO_SUBNET_MASK) {
			for (ix = i - 1 ; ix >= 0 ; ix--) {
				if (priority_list[ix] == DHO_ROUTERS) {
                                        /* swap */
					priority_list[ix] = DHO_SUBNET_MASK;
					priority_list[i] = DHO_ROUTERS;
					break;
				}
			}
		}
	}

	/* Copy out the options in the order that they appear in the
	   priority list... */
	for (i = 0; i < priority_len; i++) {
	    /* Number of bytes left to store (some may already
	       have been stored by a previous pass). */
	    unsigned length;
	    int optstart, soptstart, toptstart;
	    struct universe *u;
	    int have_encapsulation = 0;
	    struct data_string encapsulation;
	    int splitup;

	    memset (&encapsulation, 0, sizeof encapsulation);
	    have_encapsulation = 0;

	    if (option != NULL)
		option_dereference(&option, MDL);

	    /* Code for next option to try to store. */
	    code = priority_list [i];
	    
	    /* Look up the option in the site option space if the code
	       is above the cutoff, otherwise in the DHCP option space. */
	    if (code >= cfg_options -> site_code_min)
		    u = universes [cfg_options -> site_universe];
	    else
		    u = &dhcp_universe;

	    oc = lookup_option (u, cfg_options, code);

	    if (oc && oc->option)
		option_reference(&option, oc->option, MDL);
	    else
		option_code_hash_lookup(&option, u->code_hash, &code, 0, MDL);

	    /* If it's a straight encapsulation, and the user supplied a
	     * value for the entire option, use that.  Otherwise, search
	     * the encapsulated space.
	     *
	     * If it's a limited encapsulation with preceding data, and the
	     * user supplied values for the preceding bytes, search the
	     * encapsulated space.
	     */
	    if ((option != NULL) &&
		(((oc == NULL) && (option->format[0] == 'E')) ||
		 ((oc != NULL) && (option->format[0] == 'e')))) {
		static char *s, *t;
		struct option_cache *tmp;
		struct data_string name;

		s = strchr (option->format, 'E');
		if (s)
		    t = strchr (++s, '.');
		if (s && t) {
		    memset (&name, 0, sizeof name);

		    /* A zero-length universe name means the vendor
		       option space, if one is defined. */
		    if (t == s) {
			if (vendor_cfg_option) {
			    tmp = lookup_option (vendor_cfg_option -> universe,
						 cfg_options,
						 vendor_cfg_option -> code);
			    if (tmp)
				/* No need to check the return as we check name.len below */
				(void) evaluate_option_cache (&name, packet, lease,
							      client_state,
							      in_options,
							      cfg_options,
							      scope, tmp, MDL);
			} else if (vuname) {
			    name.data = (unsigned char *)s;
			    name.len = strlen (s);
			}
		    } else {
			name.data = (unsigned char *)s;
			name.len = t - s;
		    }
			
		    /* If we found a universe, and there are options configured
		       for that universe, try to encapsulate it. */
		    if (name.len) {
			have_encapsulation =
				(option_space_encapsulate
				 (&encapsulation, packet, lease, client_state,
				  in_options, cfg_options, scope, &name));
			data_string_forget (&name, MDL);
		    }
		}
	    }

	    /* In order to avoid memory leaks, we have to get to here
	       with any option cache that we allocated in tmp not being
	       referenced by tmp, and whatever option cache is referenced
	       by oc being an actual reference.   lookup_option doesn't
	       generate a reference (this needs to be fixed), so the
	       preceding goop ensures that if we *didn't* generate a new
	       option cache, oc still winds up holding an actual reference. */

	    /* If no data is available for this option, skip it. */
	    if (!oc && !have_encapsulation) {
		    continue;
	    }
	    
	    /* Find the value of the option... */
	    od.len = 0;
	    if (oc) {
		/* No need to check the return as we check od.len below */
		(void) evaluate_option_cache (&od, packet,
					      lease, client_state, in_options,
					      cfg_options, scope, oc, MDL);

		/* If we have encapsulation for this option, and an oc
		 * lookup succeeded, but the evaluation failed, it is
		 * either because this is a complex atom (atoms before
		 * E on format list) and the top half of the option is
		 * not configured, or this is a simple encapsulated
		 * space and the evaluator is giving us a NULL.  Prefer
		 * the evaluator's opinion over the subspace.
		 */
		if (!od.len) {
		    data_string_forget (&encapsulation, MDL);
		    data_string_forget (&od, MDL);
		    continue;
		}
	    }

	    /* We should now have a constant length for the option. */
	    length = od.len;
	    if (have_encapsulation) {
		    length += encapsulation.len;

		    /* od.len can be nonzero if we got here without an
		     * oc (cache lookup failed), but did have an encapsulated
		     * simple encapsulation space.
		     */
		    if (!od.len) {
			    data_string_copy (&od, &encapsulation, MDL);
			    data_string_forget (&encapsulation, MDL);
		    } else {
			    struct buffer *bp = (struct buffer *)0;
			    if (!buffer_allocate (&bp, length, MDL)) {
				    option_cache_dereference (&oc, MDL);
				    data_string_forget (&od, MDL);
				    data_string_forget (&encapsulation, MDL);
				    continue;
			    }
			    memcpy (&bp -> data [0], od.data, od.len);
			    memcpy (&bp -> data [od.len], encapsulation.data,
				    encapsulation.len);
			    data_string_forget (&od, MDL);
			    data_string_forget (&encapsulation, MDL);
			    od.data = &bp -> data [0];
			    buffer_reference (&od.buffer, bp, MDL);
			    buffer_dereference (&bp, MDL);
			    od.len = length;
			    od.terminated = 0;
		    }
	    }

	    /* Do we add a NUL? */
	    if (terminate && option && format_has_text(option->format)) {
		    length++;
		    tto = 1;
	    } else {
		    tto = 0;
	    }

	    /* Try to store the option. */
	    
	    /* If the option's length is more than 255, we must store it
	       in multiple hunks.   Store 255-byte hunks first.  However,
	       in any case, if the option data will cross a buffer
	       boundary, split it across that boundary. */

	    if (length > 255)
		splitup = 1;
	    else
		splitup = 0;

	    ix = 0;
	    optstart = bufix;
	    soptstart = six;
	    toptstart = tix;
	    while (length) {
		    unsigned incr = length;
		    int *pix;
		    unsigned char *base;

		    /* Try to fit it in the options buffer. */
		    if (!splitup &&
			((!six && !tix && (i == priority_len - 1) &&
			  (bufix + 2 + length < bufend)) ||
			 (bufix + 5 + length < bufend))) {
			base = buffer;
			pix = &bufix;
		    /* Try to fit it in the second buffer. */
		    } else if (!splitup && first_cutoff &&
			       (first_cutoff + six + 3 + length < sbufend)) {
			base = &buffer[first_cutoff];
			pix = &six;
		    /* Try to fit it in the third buffer. */
		    } else if (!splitup && second_cutoff &&
			       (second_cutoff + tix + 3 + length < buflen)) {
			base = &buffer[second_cutoff];
			pix = &tix;
		    /* Split the option up into the remaining space. */
		    } else {
			splitup = 1;

			/* Use any remaining options space. */
			if (bufix + 6 < bufend) {
			    incr = bufend - bufix - 5;
			    base = buffer;
			    pix = &bufix;
			/* Use any remaining first_cutoff space. */
			} else if (first_cutoff &&
				   (first_cutoff + six + 4 < sbufend)) {
			    incr = sbufend - (first_cutoff + six) - 3;
			    base = &buffer[first_cutoff];
			    pix = &six;
			/* Use any remaining second_cutoff space. */
			} else if (second_cutoff &&
				   (second_cutoff + tix + 4 < buflen)) {
			    incr = buflen - (second_cutoff + tix) - 3;
			    base = &buffer[second_cutoff];
			    pix = &tix;
			/* Give up, roll back this option. */
			} else {
			    bufix = optstart;
			    six = soptstart;
			    tix = toptstart;
			    break;
			}
		    }

		    if (incr > length)
			incr = length;
		    if (incr > 255)
			incr = 255;

		    /* Everything looks good - copy it in! */
		    base [*pix] = code;
		    base [*pix + 1] = (unsigned char)incr;
		    if (tto && incr == length) {
			    if (incr > 1)
				memcpy (base + *pix + 2,
					od.data + ix, (unsigned)(incr - 1));
			    base [*pix + 2 + incr - 1] = 0;
		    } else {
			    memcpy (base + *pix + 2,
				    od.data + ix, (unsigned)incr);
		    }
		    length -= incr;
		    ix += incr;
		    *pix += 2 + incr;
	    }
	    data_string_forget (&od, MDL);
	}

	if (option != NULL)
	    option_dereference(&option, MDL);

	/* If we can overload, and we have, then PAD and END those spaces. */
	if (first_cutoff && six) {
	    if ((first_cutoff + six + 1) < sbufend)
		memset (&buffer[first_cutoff + six + 1], DHO_PAD,
			sbufend - (first_cutoff + six + 1));
	    else if (first_cutoff + six >= sbufend)
		log_fatal("Second buffer overflow in overloaded options.");

	    buffer[first_cutoff + six] = DHO_END;
	    if (ocount != NULL)
	    	*ocount |= 1; /* So that caller knows there's data there. */
	}

	if (second_cutoff && tix) {
	    if (second_cutoff + tix + 1 < buflen) {
		memset (&buffer[second_cutoff + tix + 1], DHO_PAD,
			buflen - (second_cutoff + tix + 1));
	    } else if (second_cutoff + tix >= buflen)
		log_fatal("Third buffer overflow in overloaded options.");

	    buffer[second_cutoff + tix] = DHO_END;
	    if (ocount != NULL)
	    	*ocount |= 2; /* So that caller knows there's data there. */
	}

	if ((six || tix) && (bufix + 3 > bufend))
	    log_fatal("Not enough space for option overload option.");

	return bufix;
}

/* Return true if the format string has a variable length text option
 * ("t"), return false otherwise.
 */

int
format_has_text(format)
	const char *format;
{
	const char *p;

	p = format;
	while (*p != '\0') {
		switch (*p++) {
		    case 'd':
		    case 't':
			return 1;

			/* These symbols are arbitrary, not fixed or
			 * determinable length...text options with them is
			 * invalid (whatever the case, they are never NULL
			 * terminated).
			 */
		    case 'A':
		    case 'a':
		    case 'X':
		    case 'x':
		    case 'D':
			return 0;

		    case 'c':
			/* 'c' only follows 'D' atoms, and indicates that
			 * compression may be used.  If there was a 'D'
			 * atom already, we would have returned.  So this
			 * is an error, but continue looking for 't' anyway.
			 */
			log_error("format_has_text(%s): 'c' atoms are illegal "
				  "except after 'D' atoms.", format);
			break;

			/* 'E' is variable length, but not arbitrary...you
			 * can find its length if you can find an END option.
			 * N is (n)-byte in length but trails a name of a
			 * space defining the enumeration values.  So treat
			 * both the same - valid, fixed-length fields.
			 */
		    case 'E':
		    case 'N':
			/* Consume the space name. */
			while ((*p != '\0') && (*p++ != '.'))
				;
			break;

		    default:
			break;
		}
	}

	return 0;
}

/* Determine the minimum length of a DHCP option prior to any variable
 * or inconsistent length formats, according to its configured format
 * variable (and possibly from supplied option cache contents for variable
 * length format symbols).
 */

int
format_min_length(format, oc)
	const char *format;
	struct option_cache *oc;
{
	const char *p, *name;
	int min_len = 0;
	int last_size = 0;
	struct enumeration *espace;

	p = format;
	while (*p != '\0') {
		switch (*p++) {
		    case '6': /* IPv6 Address */
			min_len += 16;
			last_size = 16;
			break;

		    case 'I': /* IPv4 Address */
		    case 'l': /* int32_t */
		    case 'L': /* uint32_t */
		    case 'T': /* Lease Time, uint32_t equivalent */
			min_len += 4;
			last_size = 4;
			break;

		    case 's': /* int16_t */
		    case 'S': /* uint16_t */
			min_len += 2;
			last_size = 2;
			break;

		    case 'N': /* Enumeration value. */
			/* Consume space name. */
			name = p;
			p = strchr(p, '.');
			if (p == NULL)
				log_fatal("Corrupt format: %s", format);

			espace = find_enumeration(name, p - name);
			if (espace == NULL) {
				log_error("Unknown enumeration: %s", format);
				/* Max is safest value to return. */
				return INT_MAX;
			}

			min_len += espace->width;
			last_size = espace->width;
			p++;

			break;

		    case 'b': /* int8_t */
		    case 'B': /* uint8_t */
		    case 'F': /* Flag that is always true. */
		    case 'f': /* Flag */
			min_len++;
			last_size = 1;
			break;

		    case 'o': /* Last argument is optional. */
			min_len -= last_size;

		    /* XXX: It MAY be possible to sense the end of an
		     * encapsulated space, but right now this is too
		     * hard to support.  Return a safe value.
		     */
		    case 'e': /* Encapsulation hint (there is an 'E' later). */
		    case 'E': /* Encapsulated options. */
			return min_len;

		    case 'd': /* "Domain name" */
		    case 'D': /* "rfc1035 formatted names" */
		    case 't': /* "ASCII Text" */
		    case 'X': /* "ASCII or Hex Conditional */
		    case 'x': /* "Hex" */
		    case 'A': /* Array of all that precedes. */
		    case 'a': /* Array of preceding symbol. */
		    case 'Z': /* nothing. */
			return min_len;

		    case 'c': /* Compress flag for D atom. */
			log_error("format_min_length(%s): 'c' atom is illegal "
				  "except after 'D' atom.", format);
			return INT_MAX;

		    default:
			/* No safe value is known. */
			log_error("format_min_length(%s): No safe value "
				  "for unknown format symbols.", format);
			return INT_MAX;
		}
	}

	return min_len;
}


/* Format the specified option so that a human can easily read it. */

const char *pretty_print_option (option, data, len, emit_commas, emit_quotes)
	struct option *option;
	const unsigned char *data;
	unsigned len;
	int emit_commas;
	int emit_quotes;
{
	static char optbuf [32768]; /* XXX */
	static char *endbuf = &optbuf[sizeof(optbuf)];
	int hunksize = 0;
	int opthunk = 0;
	int hunkinc = 0;
	int numhunk = -1;
	int numelem = 0;
	int count;
	int i, j, k, l;
	char fmtbuf[32] = "";
	struct iaddr iaddr;
	struct enumeration *enumbuf[32]; /* MUST be same as fmtbuf */
	char *op = optbuf;
	const unsigned char *dp = data;
	char comma;
	unsigned long tval;
	isc_boolean_t a_array = ISC_FALSE;
	int len_used;

	if (emit_commas)
		comma = ',';
	else
		comma = ' ';

	memset (enumbuf, 0, sizeof enumbuf);

	/* Figure out the size of the data. */
	for (l = i = 0; option -> format [i]; i++, l++) {
		if (l >= sizeof(fmtbuf) - 1)
			log_fatal("Bounds failure on internal buffer at "
				  "%s:%d", MDL);

		if (!numhunk) {
			log_error ("%s: Extra codes in format string: %s",
				   option -> name,
				   &(option -> format [i]));
			break;
		}
		numelem++;
		fmtbuf [l] = option -> format [i];
		switch (option -> format [i]) {
		      case 'a':
			a_array = ISC_TRUE;
			/* Fall through */
		      case 'A':
			--numelem;
			fmtbuf [l] = 0;
			numhunk = 0;
			break;
		      case 'E':
			/* Skip the universe name. */
			while (option -> format [i] &&
			       option -> format [i] != '.')
				i++;
			/* Fall Through! */
		      case 'X':
			for (k = 0; k < len; k++) {
				if (!isascii (data [k]) ||
				    !isprint (data [k]))
					break;
			}
			/* If we found no bogus characters, or the bogus
			   character we found is a trailing NUL, it's
			   okay to print this option as text. */
			if (k == len || (k + 1 == len && data [k] == 0)) {
				fmtbuf [l] = 't';
				numhunk = -2;
			} else {
				fmtbuf [l] = 'x';
				hunksize++;
				comma = ':';
				numhunk = 0;
				a_array = ISC_TRUE;
				hunkinc = 1;
			}
			fmtbuf [l + 1] = 0;
			break;
		      case 'c':
			/* The 'c' atom is a 'D' modifier only. */
			log_error("'c' atom not following D atom in format "
				  "string: %s", option->format);
			break;
		      case 'D':
			/*
			 * Skip the 'c' atom, if present.  It does not affect
			 * how we convert wire->text format (if compression is
			 * present either way, we still process it).
			 */
			if (option->format[i+1] == 'c')
				i++;
			fmtbuf[l + 1] = 0;
			numhunk = -2;
			break;
		      case 'd':
			fmtbuf[l] = 't';
			/* Fall Through ! */
		      case 't':
			fmtbuf[l + 1] = 0;
			numhunk = -2;
			break;
		      case 'N':
			k = i;
			while (option -> format [i] &&
			       option -> format [i] != '.')
				i++;
			enumbuf [l] =
				find_enumeration (&option -> format [k] + 1,
						  i - k - 1);
			if (enumbuf[l] == NULL) {
				hunksize += 1;
				hunkinc = 1;
			} else {
				hunksize += enumbuf[l]->width;
				hunkinc = enumbuf[l]->width;
			}
			break;
		      case '6':
			hunksize += 16;
			hunkinc = 16;
			break;
		      case 'I':
		      case 'l':
		      case 'L':
		      case 'T':
			hunksize += 4;
			hunkinc = 4;
			break;
		      case 's':
		      case 'S':
			hunksize += 2;
			hunkinc = 2;
			break;
		      case 'b':
		      case 'B':
		      case 'f':
		      case 'F':
			hunksize++;
			hunkinc = 1;
			break;
		      case 'e':
		      case 'Z':
			break;
		      case 'o':
			opthunk += hunkinc;
			break;
		      default:
			log_error ("%s: garbage in format string: %s",
			      option -> name,
			      &(option -> format [i]));
			break;
		} 
	}

	/* Check for too few bytes... */
	if (hunksize - opthunk > len) {
		log_error ("%s: expecting at least %d bytes; got %d",
		      option -> name,
		      hunksize, len);
		return "<error>";
	}
	/* Check for too many bytes... */
	if (numhunk == -1 && hunksize < len)
		log_error ("%s: %d extra bytes",
		      option -> name,
		      len - hunksize);

	/* If this is an array, compute its size. */
	if (numhunk == 0) {
		if (a_array == ISC_TRUE) {
			/*
			 * It is an 'a' type array - we repeat the
			 * last format type.  A binary string for 'X'
			 * is also like this.  hunkinc is the size
			 * of the last format type and we add 1 to
			 * cover the entire first record.
			 */
			numhunk = ((len - hunksize) / hunkinc) + 1;
			len_used = hunksize + ((numhunk - 1) * hunkinc);
		} else {
			/*
			 * It is an 'A' type array - we repeat the
			 * entire record
			 */
			numhunk = len / hunksize;
			len_used = numhunk * hunksize;
		}

		/* See if we got an exact number of hunks. */
		if (len_used < len) {
			log_error ("%s: %d extra bytes at end of array\n",
				   option -> name,
				   len - len_used);
		}
	}


	/* A one-hunk array prints the same as a single hunk. */
	if (numhunk < 0)
		numhunk = 1;

	/* Cycle through the array (or hunk) printing the data. */
	for (i = 0; i < numhunk; i++) {
		if ((a_array == ISC_TRUE) && (i != 0) && (numelem > 0)) {
			/*
			 * For 'a' type of arrays we repeat
			 * only the last format character
			 * We should never hit the case of numelem == 0
			 * but let's include the check to be safe.
			 */
			j = numelem - 1;
		} else {
			/*
			 * for other types of arrays or the first
			 * time through for 'a' types, we go through
			 * the entire set of format characters.
			 */
			j = 0;
		}

		for (; j < numelem; j++) {
			switch (fmtbuf [j]) {
			      case 't':
				/* endbuf-1 leaves room for NULL. */
				k = pretty_text(&op, endbuf - 1, &dp,
						data + len, emit_quotes);
				if (k == -1) {
					log_error("Error printing text.");
					break;
				}
				*op = 0;
				break;
			      case 'D': /* RFC1035 format name list */
				for( ; dp < (data + len) ; dp += k) {
					unsigned char nbuff[NS_MAXCDNAME];
					const unsigned char *nbp, *nend;

					nend = &nbuff[sizeof(nbuff)];

					/* If this is for ISC DHCP consumption
					 * (emit_quotes), lay it out as a list
					 * of STRING tokens.  Otherwise, it is
					 * a space-separated list of DNS-
					 * escaped names as /etc/resolv.conf
					 * might digest.
					 */
					if (dp != data) {
						if (op + 2 > endbuf)
							break;

						if (emit_quotes)
							*op++ = ',';
						*op++ = ' ';
					}

					/* XXX: if fmtbuf[j+1] != 'c', we
					 * should warn if the data was
					 * compressed anyway.
					 */
					k = MRns_name_unpack(data,
							     data + len,
							     dp, nbuff,
							     sizeof(nbuff));

					if (k == -1) {
						log_error("Invalid domain "
							  "list.");
						break;
					}

					/* If emit_quotes, then use ISC DHCP
					 * escapes.  Otherwise, rely only on
					 * ns_name_ntop().
					 */
					if (emit_quotes) {
						nbp = nbuff;
						pretty_domain(&op, endbuf-1,
							      &nbp, nend);
					} else {
						/* ns_name_ntop() includes
						 * a trailing NUL in its
						 * count.
						 */
						count = MRns_name_ntop(
								nbuff, op, 
								(endbuf-op)-1);

						if (count <= 0) {
							log_error("Invalid "
								"domain name.");
							break;
						}

						/* Consume all but the trailing
						 * NUL.
						 */
						op += count - 1;

						/* Replace the trailing NUL
						 * with the implicit root
						 * (in the unlikely event the
						 * domain name /is/ the root).
						 */
						*op++ = '.';
					}
				}
				*op = '\0';
				break;
				/* pretty-printing an array of enums is
				   going to get ugly. */
			      case 'N':
				if (!enumbuf [j]) {
					tval = *dp++;
					goto enum_as_num;
				}

				switch (enumbuf[j]->width) {
				      case 1:
					tval = getUChar(dp);
					break;

				     case 2:
					tval = getUShort(dp);
					break;

				    case 4:
					tval = getULong(dp);
					break;

				    default:
					log_fatal("Impossible case at %s:%d.",
						  MDL);
					return "<double impossible condition>";
				}

				for (i = 0; ;i++) {
					if (!enumbuf [j] -> values [i].name)
						goto enum_as_num;
					if (enumbuf [j] -> values [i].value ==
					    tval)
						break;
				}
				strcpy (op, enumbuf [j] -> values [i].name);
				dp += enumbuf[j]->width;
				break;

			      enum_as_num:
				sprintf(op, "%lu", tval);
				break;

			      case 'I':
				iaddr.len = 4;
				memcpy(iaddr.iabuf, dp, 4);
				strcpy(op, piaddr(iaddr));
				dp += 4;
				break;
			      case '6':
				iaddr.len = 16;
				memcpy(iaddr.iabuf, dp, 16);
				strcpy(op, piaddr(iaddr));
				dp += 16;
				break;
			      case 'l':
				sprintf (op, "%ld", (long)getLong (dp));
				dp += 4;
				break;
			      case 'T':
				tval = getULong (dp);
				if (tval == -1)
					sprintf (op, "%s", "infinite");
				else
					sprintf(op, "%lu", tval);
				break;
			      case 'L':
				sprintf(op, "%lu",
					(unsigned long)getULong(dp));
				dp += 4;
				break;
			      case 's':
				sprintf (op, "%d", (int)getShort (dp));
				dp += 2;
				break;
			      case 'S':
				sprintf(op, "%u", (unsigned)getUShort(dp));
				dp += 2;
				break;
			      case 'b':
				sprintf (op, "%d", *(const char *)dp++);
				break;
			      case 'B':
				sprintf (op, "%d", *dp++);
				break;
			      case 'X':
			      case 'x':
				sprintf (op, "%x", *dp++);
				break;
			      case 'f':
				strcpy (op, *dp++ ? "true" : "false");
				break;
			      case 'F':
				strcpy (op, "true");
				break;
			      case 'e':
			      case 'Z':
				*op = '\0';
				break;
			      default:
				log_error ("Unexpected format code %c",
					   fmtbuf [j]);
			}
			op += strlen (op);
			if (dp == data + len)
				break;
			if (j + 1 < numelem && comma != ':')
				*op++ = ' ';
		}
		if (i + 1 < numhunk) {
			*op++ = comma;
		}
		if (dp == data + len)
			break;
	}
	return optbuf;
}

int get_option (result, universe, packet, lease, client_state,
		in_options, cfg_options, options, scope, code, file, line)
	struct data_string *result;
	struct universe *universe;
	struct packet *packet;
	struct lease *lease;
	struct client_state *client_state;
	struct option_state *in_options;
	struct option_state *cfg_options;
	struct option_state *options;
	struct binding_scope **scope;
	unsigned code;
	const char *file;
	int line;
{
	struct option_cache *oc;

	if (!universe -> lookup_func)
		return 0;
	oc = ((*universe -> lookup_func) (universe, options, code));
	if (!oc)
		return 0;
	if (!evaluate_option_cache (result, packet, lease, client_state,
				    in_options, cfg_options, scope, oc,
				    file, line))
		return 0;
	return 1;
}

void set_option (universe, options, option, op)
	struct universe *universe;
	struct option_state *options;
	struct option_cache *option;
	enum statement_op op;
{
	struct option_cache *oc, *noc;

	switch (op) {
	      case if_statement:
	      case add_statement:
	      case eval_statement:
	      case break_statement:
	      default:
		log_error ("bogus statement type in set_option.");
		break;

	      case default_option_statement:
		oc = lookup_option (universe, options,
				    option -> option -> code);
		if (oc)
			break;
		save_option (universe, options, option);
		break;

	      case supersede_option_statement:
	      case send_option_statement:
		/* Install the option, replacing any existing version. */
		save_option (universe, options, option);
		break;

	      case append_option_statement:
	      case prepend_option_statement:
		oc = lookup_option (universe, options,
				    option -> option -> code);
		if (!oc) {
			save_option (universe, options, option);
			break;
		}
		/* If it's not an expression, make it into one. */
		if (!oc -> expression && oc -> data.len) {
			if (!expression_allocate (&oc -> expression, MDL)) {
				log_error ("Can't allocate const expression.");
				break;
			}
			oc -> expression -> op = expr_const_data;
			data_string_copy
				(&oc -> expression -> data.const_data,
				 &oc -> data, MDL);
			data_string_forget (&oc -> data, MDL);
		}
		noc = (struct option_cache *)0;
		if (!option_cache_allocate (&noc, MDL))
			break;
		if (op == append_option_statement) {
			if (!make_concat (&noc -> expression,
					  oc -> expression,
					  option -> expression)) {
				option_cache_dereference (&noc, MDL);
				break;
			}
		} else {
			if (!make_concat (&noc -> expression,
					  option -> expression,
					  oc -> expression)) {
				option_cache_dereference (&noc, MDL);
				break;
			}
		}
		option_reference(&(noc->option), oc->option, MDL);
		save_option (universe, options, noc);
		option_cache_dereference (&noc, MDL);
		break;
	}
}

struct option_cache *lookup_option (universe, options, code)
	struct universe *universe;
	struct option_state *options;
	unsigned code;
{
	if (!options)
		return (struct option_cache *)0;
	if (universe -> lookup_func)
		return (*universe -> lookup_func) (universe, options, code);
	else
		log_error ("can't look up options in %s space.",
			   universe -> name);
	return (struct option_cache *)0;
}

struct option_cache *lookup_hashed_option (universe, options, code)
	struct universe *universe;
	struct option_state *options;
	unsigned code;
{
	int hashix;
	pair bptr;
	pair *hash;

	/* Make sure there's a hash table. */
	if (universe -> index >= options -> universe_count ||
	    !(options -> universes [universe -> index]))
		return (struct option_cache *)0;

	hash = options -> universes [universe -> index];

	hashix = compute_option_hash (code);
	for (bptr = hash [hashix]; bptr; bptr = bptr -> cdr) {
		if (((struct option_cache *)(bptr -> car)) -> option -> code ==
		    code)
			return (struct option_cache *)(bptr -> car);
	}
	return (struct option_cache *)0;
}

/* Save a specified buffer into an option cache. */
int
save_option_buffer(struct universe *universe, struct option_state *options,
		   struct buffer *bp, unsigned char *buffer, unsigned length,
		   unsigned code, int terminatep)
{
	struct option_cache *op = NULL;
	int status = 1;

	status = prepare_option_buffer(universe, bp, buffer, length, code,
				       terminatep, &op);

	if (status == 0)
		goto cleanup;

	save_option(universe, options, op);

    cleanup:
	if (op != NULL)
		option_cache_dereference(&op, MDL);

	return status;
}

/* Append a specified buffer onto the tail of an option cache. */
int
append_option_buffer(struct universe *universe, struct option_state *options,
		     struct buffer *bp, unsigned char *buffer, unsigned length,
		     unsigned code, int terminatep)
{
	struct option_cache *op = NULL;
	int status = 1;

	status = prepare_option_buffer(universe, bp, buffer, length, code,
				       terminatep, &op);

	if (status == 0)
		goto cleanup;

	also_save_option(universe, options, op);

      cleanup:
	if (op != NULL)
		option_cache_dereference(&op, MDL);

	return status;
}

/* Create/copy a buffer into a new option cache. */
static int
prepare_option_buffer(struct universe *universe, struct buffer *bp,
		      unsigned char *buffer, unsigned length, unsigned code,
		      int terminatep, struct option_cache **opp)
{
	struct buffer *lbp = NULL;
	struct option *option = NULL;
	struct option_cache *op;
	int status = 1;

	/* Code sizes of 8, 16, and 32 bits are allowed. */
	switch(universe->tag_size) {
	      case 1:
		if (code > 0xff)
			return 0;
		break;
	      case 2:
		if (code > 0xffff)
			return 0;
		break;
	      case 4:
		if (code > 0xffffffff)
			return 0;
		break;

	      default:
		log_fatal("Inconsistent universe tag size at %s:%d.", MDL);
	}

	option_code_hash_lookup(&option, universe->code_hash, &code, 0, MDL);

	/* If we created an option structure for each option a client
	 * supplied, it's possible we may create > 2^32 option structures.
	 * That's not feasible.  So by failing to enter these option
	 * structures into the code and name hash tables, references will
	 * never be more than 1 - when the option cache is destroyed, this
	 * will be cleaned up.
	 */
	if (!option) {
		char nbuf[sizeof("unknown-4294967295")];

		sprintf(nbuf, "unknown-%u", code);

		option = new_option(nbuf, MDL);

		if (!option)
			return 0;

		option->format = default_option_format;
		option->universe = universe;
		option->code = code;

		/* new_option() doesn't set references, pretend. */
		option->refcnt = 1;
	}

	if (!option_cache_allocate (opp, MDL)) {
		log_error("No memory for option code %s.%s.",
			  universe->name, option->name);
		status = 0;
		goto cleanup;
	}

	/* Pointer rather than double pointer makes for less parens. */
	op = *opp;

	option_reference(&op->option, option, MDL);

	/* If we weren't passed a buffer in which the data are saved and
	   refcounted, allocate one now. */
	if (!bp) {
		if (!buffer_allocate (&lbp, length + terminatep, MDL)) {
			log_error ("no memory for option buffer.");

			status = 0;
			goto cleanup;
		}
		memcpy (lbp -> data, buffer, length + terminatep);
		bp = lbp;
		buffer = &bp -> data [0]; /* Refer to saved buffer. */
	}

	/* Reference buffer copy to option cache. */
	op -> data.buffer = (struct buffer *)0;
	buffer_reference (&op -> data.buffer, bp, MDL);

	/* Point option cache into buffer. */
	op -> data.data = buffer;
	op -> data.len = length;

	if (terminatep) {
		/* NUL terminate (we can get away with this because we (or
		   the caller!) allocated one more than the buffer size, and
		   because the byte following the end of an option is always
		   the code of the next option, which the caller is getting
		   out of the *original* buffer. */
		buffer [length] = 0;
		op -> data.terminated = 1;
	} else
		op -> data.terminated = 0;

	/* If this option is ultimately a text option, null determinate to
	 * comply with RFC2132 section 2.  Mark a flag so this can be sensed
	 * later to echo NULLs back to clients that supplied them (they
	 * probably expect them).
	 */
	if (format_has_text(option->format)) {
		int min_len = format_min_length(option->format, op);

		while ((op->data.len > min_len) &&
		       (op->data.data[op->data.len-1] == '\0')) {
			op->data.len--;
			op->flags |= OPTION_HAD_NULLS;
		}
	}

	/* And let go of our references. */
      cleanup:
	if (lbp != NULL)
		buffer_dereference(&lbp, MDL);
	option_dereference(&option, MDL);

	return status;
}

static void
count_options(struct option_cache *dummy_oc,
	      struct packet *dummy_packet,
	      struct lease *dummy_lease, 
	      struct client_state *dummy_client_state,
	      struct option_state *dummy_opt_state,
	      struct option_state *opt_state,
	      struct binding_scope **dummy_binding_scope,
	      struct universe *dummy_universe, 
	      void *void_accumulator) {
	int *accumulator = (int *)void_accumulator;

	*accumulator += 1;
}

static void
collect_oro(struct option_cache *oc,
	    struct packet *dummy_packet,
	    struct lease *dummy_lease, 
	    struct client_state *dummy_client_state,
	    struct option_state *dummy_opt_state,
	    struct option_state *opt_state,
	    struct binding_scope **dummy_binding_scope,
	    struct universe *dummy_universe, 
	    void *void_oro) {
	struct data_string *oro = (struct data_string *)void_oro;

	putUShort(oro->buffer->data + oro->len, oc->option->code);
	oro->len += 2;
}

/* build_server_oro() is presently unusued, but may be used at a future date
 * with support for Reconfigure messages (as a hint to the client about new
 * option value contents).
 */
void
build_server_oro(struct data_string *server_oro, 
		 struct option_state *options,
		 const char *file, int line) {
	int num_opts;
	int i;
	struct option *o;

	/*
	 * Count the number of options, so we can allocate enough memory.
	 * We want to mention sub-options too, so check all universes.
	 */
	num_opts = 0;
	option_space_foreach(NULL, NULL, NULL, NULL, options,
			     NULL, &dhcpv6_universe, (void *)&num_opts,
			     count_options);
	for (i=0; i < options->universe_count; i++) {
		if (options->universes[i] != NULL) {
		    	o = universes[i]->enc_opt;
			while (o != NULL) {
				if (o->universe == &dhcpv6_universe) {
					num_opts++;
					break;
				}
				o = o->universe->enc_opt;
			}
		}
	}

	/*
	 * Allocate space.
	 */
	memset(server_oro, 0, sizeof(*server_oro));
	if (!buffer_allocate(&server_oro->buffer, num_opts * 2, MDL)) {
		log_fatal("no memory to build server ORO");
	}
	server_oro->data = server_oro->buffer->data;

	/*
	 * Copy the data in.
	 * We want to mention sub-options too, so check all universes.
	 */
	server_oro->len = 0; 	/* gets set in collect_oro */
	option_space_foreach(NULL, NULL, NULL, NULL, options,
			     NULL, &dhcpv6_universe, (void *)server_oro,
			     collect_oro);
	for (i=0; i < options->universe_count; i++) {
		if (options->universes[i] != NULL) {
		    	o = universes[i]->enc_opt;
			while (o != NULL) {
				if (o->universe == &dhcpv6_universe) {
					unsigned char *tmp;
					tmp = server_oro->buffer->data;
					putUShort(tmp + server_oro->len,
						  o->code);
					server_oro->len += 2;
					break;
				}
				o = o->universe->enc_opt;
			}
		}
	}
}

/* Wrapper function to put an option cache into an option state. */
void
save_option(struct universe *universe, struct option_state *options,
	    struct option_cache *oc)
{
	if (universe->save_func)
		(*universe->save_func)(universe, options, oc, ISC_FALSE);
	else
		log_error("can't store options in %s space.", universe->name);
}

/* Wrapper function to append an option cache into an option state's list. */
void
also_save_option(struct universe *universe, struct option_state *options,
		 struct option_cache *oc)
{
	if (universe->save_func)
		(*universe->save_func)(universe, options, oc, ISC_TRUE);
	else
		log_error("can't store options in %s space.", universe->name);
}

void
save_hashed_option(struct universe *universe, struct option_state *options,
		   struct option_cache *oc, isc_boolean_t appendp)
{
	int hashix;
	pair bptr;
	pair *hash = options -> universes [universe -> index];
	struct option_cache **ocloc;

	if (oc -> refcnt == 0)
		abort ();

	/* Compute the hash. */
	hashix = compute_option_hash (oc -> option -> code);

	/* If there's no hash table, make one. */
	if (!hash) {
		hash = (pair *)dmalloc (OPTION_HASH_SIZE * sizeof *hash, MDL);
		if (!hash) {
			log_error ("no memory to store %s.%s",
				   universe -> name, oc -> option -> name);
			return;
		}
		memset (hash, 0, OPTION_HASH_SIZE * sizeof *hash);
		options -> universes [universe -> index] = (void *)hash;
	} else {
		/* Try to find an existing option matching the new one. */
		for (bptr = hash [hashix]; bptr; bptr = bptr -> cdr) {
			if (((struct option_cache *)
			     (bptr -> car)) -> option -> code ==
			    oc -> option -> code)
				break;
		}

		/* Deal with collisions on the hash list. */
		if (bptr) {
			ocloc = (struct option_cache **)&bptr->car;

			/*
			 * If appendp is set, append it onto the tail of the
			 * ->next list.  If it is not set, rotate it into
			 * position at the head of the list.
			 */
			if (appendp) {
				do {
					ocloc = &(*ocloc)->next;
				} while (*ocloc != NULL);
			} else {
				option_cache_dereference(ocloc, MDL);
			}

			option_cache_reference(ocloc, oc, MDL);
			return;
		}
	}

	/* Otherwise, just put the new one at the head of the list. */
	bptr = new_pair (MDL);
	if (!bptr) {
		log_error ("No memory for option_cache reference.");
		return;
	}
	bptr -> cdr = hash [hashix];
	bptr -> car = 0;
	option_cache_reference ((struct option_cache **)&bptr -> car, oc, MDL);
	hash [hashix] = bptr;
}

void delete_option (universe, options, code)
	struct universe *universe;
	struct option_state *options;
	int code;
{
	if (universe -> delete_func)
		(*universe -> delete_func) (universe, options, code);
	else
		log_error ("can't delete options from %s space.",
			   universe -> name);
}

void delete_hashed_option (universe, options, code)
	struct universe *universe;
	struct option_state *options;
	int code;
{
	int hashix;
	pair bptr, prev = (pair)0;
	pair *hash = options -> universes [universe -> index];

	/* There may not be any options in this space. */
	if (!hash)
		return;

	/* Try to find an existing option matching the new one. */
	hashix = compute_option_hash (code);
	for (bptr = hash [hashix]; bptr; bptr = bptr -> cdr) {
		if (((struct option_cache *)(bptr -> car)) -> option -> code
		    == code)
			break;
		prev = bptr;
	}
	/* If we found one, wipe it out... */
	if (bptr) {
		if (prev)
			prev -> cdr = bptr -> cdr;
		else
			hash [hashix] = bptr -> cdr;
		option_cache_dereference
			((struct option_cache **)(&bptr -> car), MDL);
		free_pair (bptr, MDL);
	}
}

extern struct option_cache *free_option_caches; /* XXX */

int option_cache_dereference (ptr, file, line)
	struct option_cache **ptr;
	const char *file;
	int line;
{
	if (!ptr || !*ptr) {
		log_error ("Null pointer in option_cache_dereference: %s(%d)",
			   file, line);
#if defined (POINTER_DEBUG)
		abort ();
#else
		return 0;
#endif
	}

	(*ptr) -> refcnt--;
	rc_register (file, line, ptr, *ptr, (*ptr) -> refcnt, 1, RC_MISC);
	if (!(*ptr) -> refcnt) {
		if ((*ptr) -> data.buffer)
			data_string_forget (&(*ptr) -> data, file, line);
		if ((*ptr)->option)
			option_dereference(&(*ptr)->option, MDL);
		if ((*ptr) -> expression)
			expression_dereference (&(*ptr) -> expression,
						file, line);
		if ((*ptr) -> next)
			option_cache_dereference (&((*ptr) -> next),
						  file, line);
		/* Put it back on the free list... */
		(*ptr) -> expression = (struct expression *)free_option_caches;
		free_option_caches = *ptr;
		dmalloc_reuse (free_option_caches, (char *)0, 0, 0);
	}
	if ((*ptr) -> refcnt < 0) {
		log_error ("%s(%d): negative refcnt!", file, line);
#if defined (DEBUG_RC_HISTORY)
		dump_rc_history (*ptr);
#endif
#if defined (POINTER_DEBUG)
		abort ();
#else
		*ptr = (struct option_cache *)0;
		return 0;
#endif
	}
	*ptr = (struct option_cache *)0;
	return 1;

}

int hashed_option_state_dereference (universe, state, file, line)
	struct universe *universe;
	struct option_state *state;
	const char *file;
	int line;
{
	pair *heads;
	pair cp, next;
	int i;

	/* Get the pointer to the array of hash table bucket heads. */
	heads = (pair *)(state -> universes [universe -> index]);
	if (!heads)
		return 0;

	/* For each non-null head, loop through all the buckets dereferencing
	   the attached option cache structures and freeing the buckets. */
	for (i = 0; i < OPTION_HASH_SIZE; i++) {
		for (cp = heads [i]; cp; cp = next) {
			next = cp -> cdr;
			option_cache_dereference
				((struct option_cache **)&cp -> car,
				 file, line);
			free_pair (cp, file, line);
		}
	}

	dfree (heads, file, line);
	state -> universes [universe -> index] = (void *)0;
	return 1;
}

/* The 'data_string' primitive doesn't have an appension mechanism.
 * This function must then append a new option onto an existing buffer
 * by first duplicating the original buffer and appending the desired
 * values, followed by coping the new value into place.
 */
int
append_option(struct data_string *dst, struct universe *universe,
	      struct option *option, struct data_string *src)
{
	struct data_string tmp;

	if (src->len == 0 && option->format[0] != 'Z')
		return 0;

	memset(&tmp, 0, sizeof(tmp));

	/* Allocate a buffer to hold existing data, the current option's
	 * tag and length, and the option's content.
	 */
	if (!buffer_allocate(&tmp.buffer,
			     (dst->len + universe->length_size +
			      universe->tag_size + src->len), MDL)) {
		/* XXX: This kills all options presently stored in the
		 * destination buffer.  This is the way the original code
		 * worked, and assumes an 'all or nothing' approach to
		 * eg encapsulated option spaces.  It may or may not be
		 * desirable.
		 */
		data_string_forget(dst, MDL);
		return 0;
	}
	tmp.data = tmp.buffer->data;

	/* Copy the existing data off the destination. */
	if (dst->len != 0)
		memcpy(tmp.buffer->data, dst->data, dst->len);
	tmp.len = dst->len;

	/* Place the new option tag and length. */
	(*universe->store_tag)(tmp.buffer->data + tmp.len, option->code);
	tmp.len += universe->tag_size;
	(*universe->store_length)(tmp.buffer->data + tmp.len, src->len);
	tmp.len += universe->length_size;

	/* Copy the option contents onto the end. */
	memcpy(tmp.buffer->data + tmp.len, src->data, src->len);
	tmp.len += src->len;

	/* Play the shell game. */
	data_string_forget(dst, MDL);
	data_string_copy(dst, &tmp, MDL);
	data_string_forget(&tmp, MDL);
	return 1;
}

int
store_option(struct data_string *result, struct universe *universe,
	     struct packet *packet, struct lease *lease,
	     struct client_state *client_state,
	     struct option_state *in_options, struct option_state *cfg_options,
	     struct binding_scope **scope, struct option_cache *oc)
{
	struct data_string tmp;
	struct universe *subu=NULL;
	int status;
	char *start, *end;

	memset(&tmp, 0, sizeof(tmp));

	if (evaluate_option_cache(&tmp, packet, lease, client_state,
				  in_options, cfg_options, scope, oc, MDL)) {
		/* If the option is an extended 'e'ncapsulation (not a
		 * direct 'E'ncapsulation), append the encapsulated space
		 * onto the currently prepared value.
		 */
		do {
			if (oc->option->format &&
			    oc->option->format[0] == 'e') {
				/* Skip forward to the universe name. */
				start = strchr(oc->option->format, 'E');
				if (start == NULL)
					break;

				/* Locate the name-terminating '.'. */
				end = strchr(++start, '.');

				/* A zero-length name is not allowed in
				 * these kinds of encapsulations.
				 */
				if (end == NULL || start == end)
					break;

				universe_hash_lookup(&subu, universe_hash,
						     start, end - start, MDL);

				if (subu == NULL) {
					log_error("store_option: option %d "
						  "refers to unknown "
						  "option space '%.*s'.",
						  oc->option->code,
						  (int)(end - start), start);
					break;
				}

				/* Append encapsulations, if any.  We
				 * already have the prepended values, so
				 * we send those even if there are no
				 * encapsulated options (and ->encapsulate()
				 * returns zero).
				 */
				subu->encapsulate(&tmp, packet, lease,
						  client_state, in_options,
						  cfg_options, scope, subu);
				subu = NULL;
			}
		} while (ISC_FALSE);

		status = append_option(result, universe, oc->option, &tmp);
		data_string_forget(&tmp, MDL);

		return status;
	}

	return 0;
}

int option_space_encapsulate (result, packet, lease, client_state,
			      in_options, cfg_options, scope, name)
	struct data_string *result;
	struct packet *packet;
	struct lease *lease;
	struct client_state *client_state;
	struct option_state *in_options;
	struct option_state *cfg_options;
	struct binding_scope **scope;
	struct data_string *name;
{
	struct universe *u = NULL;
	int status = 0;

	universe_hash_lookup(&u, universe_hash, 
			     (const char *)name->data, name->len, MDL);
	if (u == NULL) {
		log_error("option_space_encapsulate: option space '%.*s' does "
			  "not exist, but is configured.",
			  (int)name->len, name->data);
		return status;
	}

	if (u->encapsulate != NULL) {
		if (u->encapsulate(result, packet, lease, client_state,
				   in_options, cfg_options, scope, u))
			status = 1;
	} else
		log_error("encapsulation requested for '%s' with no support.",
			  name->data);

	return status;
}

/* Attempt to store any 'E'ncapsulated options that have not yet been
 * placed on the option buffer by the above (configuring a value in
 * the space over-rides any values in the child universe).
 *
 * Note that there are far fewer universes than there will ever be
 * options in any universe.  So it is faster to traverse the
 * configured universes, checking if each is encapsulated in the
 * current universe, and if so attempting to do so.
 *
 * For each configured universe for this configuration option space,
 * which is encapsulated within the current universe, can not be found
 * by the lookup function (the universe-specific encapsulation
 * functions would already have stored such a value), and encapsulates
 * at least one option, append it.
 */
static int
search_subencapsulation(struct data_string *result, struct packet *packet,
			struct lease *lease, struct client_state *client_state,
			struct option_state *in_options,
			struct option_state *cfg_options,
			struct binding_scope **scope,
			struct universe *universe)
{
	struct data_string sub;
	struct universe *subu;
	int i, status = 0;

	memset(&sub, 0, sizeof(sub));
	for (i = 0 ; i < cfg_options->universe_count ; i++) {
		subu = universes[i];

		if (subu == NULL)
			log_fatal("Impossible condition at %s:%d.", MDL);

		if (subu->enc_opt != NULL &&
		    subu->enc_opt->universe == universe &&
		    subu->enc_opt->format != NULL &&
		    subu->enc_opt->format[0] == 'E' &&
		    lookup_option(universe, cfg_options,
				  subu->enc_opt->code) == NULL &&
		    subu->encapsulate(&sub, packet, lease, client_state,
				      in_options, cfg_options,
				      scope, subu)) {
			if (append_option(result, universe,
					  subu->enc_opt, &sub))
				status = 1;

			data_string_forget(&sub, MDL);
		}
	}

	return status;
}

int hashed_option_space_encapsulate (result, packet, lease, client_state,
				     in_options, cfg_options, scope, universe)
	struct data_string *result;
	struct packet *packet;
	struct lease *lease;
	struct client_state *client_state;
	struct option_state *in_options;
	struct option_state *cfg_options;
	struct binding_scope **scope;
	struct universe *universe;
{
	pair p, *hash;
	int status;
	int i;

	if (universe -> index >= cfg_options -> universe_count)
		return 0;

	hash = cfg_options -> universes [universe -> index];
	if (!hash)
		return 0;

	/* For each hash bucket, and each configured option cache within
	 * that bucket, append the option onto the buffer in encapsulated
	 * format appropriate to the universe.
	 */
	status = 0;
	for (i = 0; i < OPTION_HASH_SIZE; i++) {
		for (p = hash [i]; p; p = p -> cdr) {
			if (store_option(result, universe, packet, lease,
					 client_state, in_options, cfg_options,
					 scope, (struct option_cache *)p->car))
				status = 1;
		}
	}

	if (search_subencapsulation(result, packet, lease, client_state,
				    in_options, cfg_options, scope, universe))
		status = 1;

	return status;
}

int nwip_option_space_encapsulate (result, packet, lease, client_state,
				   in_options, cfg_options, scope, universe)
	struct data_string *result;
	struct packet *packet;
	struct lease *lease;
	struct client_state *client_state;
	struct option_state *in_options;
	struct option_state *cfg_options;
	struct binding_scope **scope;
	struct universe *universe;
{
	pair ocp;
	int status;
	static struct option_cache *no_nwip;
	struct data_string ds;
	struct option_chain_head *head;

	if (universe -> index >= cfg_options -> universe_count)
		return 0;
	head = ((struct option_chain_head *)
		cfg_options -> universes [nwip_universe.index]);
	if (!head)
		return 0;

	status = 0;
	for (ocp = head -> first; ocp; ocp = ocp -> cdr) {
		if (store_option (result, universe, packet,
				  lease, client_state, in_options,
				  cfg_options, scope,
				  (struct option_cache *)ocp -> car))
			status = 1;
	}

	/* If there's no data, the nwip suboption is supposed to contain
	   a suboption saying there's no data. */
	if (!status) {
		if (!no_nwip) {
			unsigned one = 1;
			static unsigned char nni [] = { 1, 0 };

			memset (&ds, 0, sizeof ds);
			ds.data = nni;
			ds.len = 2;
			if (option_cache_allocate (&no_nwip, MDL))
				data_string_copy (&no_nwip -> data, &ds, MDL);
			if (!option_code_hash_lookup(&no_nwip->option,
						     nwip_universe.code_hash,
						     &one, 0, MDL))
				log_fatal("Nwip option hash does not contain "
					  "1 (%s:%d).", MDL);
		}
		if (no_nwip) {
			if (store_option (result, universe, packet, lease,
					  client_state, in_options,
					  cfg_options, scope, no_nwip))
				status = 1;
		}
	} else {
		memset (&ds, 0, sizeof ds);

		/* If we have nwip options, the first one has to be the
		   nwip-exists-in-option-area option. */
		if (!buffer_allocate (&ds.buffer, result -> len + 2, MDL)) {
			data_string_forget (result, MDL);
			return 0;
		}
		ds.data = &ds.buffer -> data [0];
		ds.buffer -> data [0] = 2;
		ds.buffer -> data [1] = 0;
		memcpy (&ds.buffer -> data [2], result -> data, result -> len);
		data_string_forget (result, MDL);
		data_string_copy (result, &ds, MDL);
		data_string_forget (&ds, MDL);
	}

	return status;
}

/* We don't want to use ns_name_pton()...it doesn't tell us how many bytes
 * it has consumed, and it plays havoc with our escapes.
 *
 * So this function does DNS encoding, and returns either the number of
 * octects consumed (on success), or -1 on failure.
 */
static int
fqdn_encode(unsigned char *dst, int dstlen, const unsigned char *src,
	    int srclen)
{
	unsigned char *out;
	int i, j, len, outlen=0;

	out = dst;
	for (i = 0, j = 0 ; i < srclen ; i = j) {
		while ((j < srclen) && (src[j] != '.') && (src[j] != '\0'))
			j++;

		len = j - i;
		if ((outlen + 1 + len) > dstlen)
			return -1;

		*out++ = len;
		outlen++;

		/* We only do one FQDN, ending in one root label. */
		if (len == 0)
			return outlen;

		memcpy(out, src + i, len);
		out += len;
		outlen += len;

		/* Advance past the root label. */
		j++;
	}

	if ((outlen + 1) > dstlen)
		return -1;

	/* Place the root label. */
	*out++ = 0;
	outlen++;

	return outlen;
}

int fqdn_option_space_encapsulate (result, packet, lease, client_state,
				   in_options, cfg_options, scope, universe)
	struct data_string *result;
	struct packet *packet;
	struct lease *lease;
	struct client_state *client_state;
	struct option_state *in_options;
	struct option_state *cfg_options;
	struct binding_scope **scope;
	struct universe *universe;
{
	pair ocp;
	struct data_string results [FQDN_SUBOPTION_COUNT + 1];
	int status = 1;
	int i;
	unsigned len;
	struct buffer *bp = (struct buffer *)0;
	struct option_chain_head *head;

	/* If there's no FQDN universe, don't encapsulate. */
	if (fqdn_universe.index >= cfg_options -> universe_count)
		return 0;
	head = ((struct option_chain_head *)
		cfg_options -> universes [fqdn_universe.index]);
	if (!head)
		return 0;

	/* Figure out the values of all the suboptions. */
	memset (results, 0, sizeof results);
	for (ocp = head -> first; ocp; ocp = ocp -> cdr) {
		struct option_cache *oc = (struct option_cache *)(ocp -> car);
		if (oc -> option -> code > FQDN_SUBOPTION_COUNT)
			continue;
		/* No need to check the return code, we check the length later */
		(void) evaluate_option_cache (&results[oc->option->code],
					      packet, lease, client_state,
					      in_options, cfg_options, scope,
					      oc, MDL);
	}
	/* We add a byte for the flags field.
	 * We add two bytes for the two RCODE fields.
	 * We add a byte because we will prepend a label count.
	 * We add a byte because the input len doesn't count null termination,
	 * and we will add a root label.
	 */
	len = 5 + results [FQDN_FQDN].len;
	/* Save the contents of the option in a buffer. */
	if (!buffer_allocate (&bp, len, MDL)) {
		log_error ("no memory for option buffer.");
		status = 0;
		goto exit;
	}
	buffer_reference (&result -> buffer, bp, MDL);
	result -> len = 3;
	result -> data = &bp -> data [0];

	memset (&bp -> data [0], 0, len);
	/* XXX: The server should set bit 4 (yes, 4, not 3) to 1 if it is
	 * not going to perform any ddns updates.  The client should set the
	 * bit if it doesn't want the server to perform any updates.
	 * The problem is at this layer of abstraction we have no idea if
	 * the caller is a client or server.
	 *
	 * See RFC4702, Section 3.1, 'The "N" bit'.
	 *
	 * if (?)
	 *	bp->data[0] |= 8;
	 */
	if (results [FQDN_NO_CLIENT_UPDATE].len &&
	    results [FQDN_NO_CLIENT_UPDATE].data [0])
		bp -> data [0] |= 2;
	if (results [FQDN_SERVER_UPDATE].len &&
	    results [FQDN_SERVER_UPDATE].data [0])
		bp -> data [0] |= 1;
	if (results [FQDN_RCODE1].len)
		bp -> data [1] = results [FQDN_RCODE1].data [0];
	if (results [FQDN_RCODE2].len)
		bp -> data [2] = results [FQDN_RCODE2].data [0];

	if (results [FQDN_ENCODED].len &&
	    results [FQDN_ENCODED].data [0]) {
		bp->data[0] |= 4;
		if (results [FQDN_FQDN].len) {
			i = fqdn_encode(&bp->data[3], len - 3,
					results[FQDN_FQDN].data,
					results[FQDN_FQDN].len);

			if (i < 0) {
				status = 0;
				goto exit;
			}

			result->len += i;
			result->terminated = 0;
		}
	} else {
		if (results [FQDN_FQDN].len) {
			memcpy (&bp -> data [3], results [FQDN_FQDN].data,
				results [FQDN_FQDN].len);
			result -> len += results [FQDN_FQDN].len;
			result -> terminated = 0;
		}
	}
      exit:
	for (i = 1; i <= FQDN_SUBOPTION_COUNT; i++) {
		if (results [i].len)
			data_string_forget (&results [i], MDL);
	}
	buffer_dereference (&bp, MDL);
	if (!status)
		data_string_forget(result, MDL);
	return status;
}

/*
 * Trap invalid attempts to inspect FQND6 contents.
 */
struct option_cache *
lookup_fqdn6_option(struct universe *universe, struct option_state *options,
		    unsigned code)
{
	log_fatal("Impossible condition at %s:%d.", MDL);
	return NULL;
}

/*
 * Trap invalid attempts to save options directly to FQDN6 rather than FQDN.
 */
void
save_fqdn6_option(struct universe *universe, struct option_state *options,
		  struct option_cache *oc, isc_boolean_t appendp)
{
	log_fatal("Impossible condition at %s:%d.", MDL);
}

/*
 * Trap invalid attempts to delete an option out of the FQDN6 universe.
 */
void
delete_fqdn6_option(struct universe *universe, struct option_state *options,
		    int code)
{
	log_fatal("Impossible condition at %s:%d.", MDL);
}

/* Shill to the DHCPv4 fqdn option cache any attempts to traverse the
 * V6's option cache entry.
 *
 * This function is called speculatively by dhclient to setup
 * environment variables.  But it would have already called the
 * foreach on the normal fqdn universe, so this is superfluous.
 */
void
fqdn6_option_space_foreach(struct packet *packet, struct lease *lease,
			   struct client_state *client_state,
			   struct option_state *in_options,
			   struct option_state *cfg_options,
			   struct binding_scope **scope,
			   struct universe *u, void *stuff,
			   void (*func)(struct option_cache *,
					struct packet *,
					struct lease *,
					struct client_state *,
					struct option_state *,
					struct option_state *,
					struct binding_scope **,
					struct universe *, void *))
{
	/* Pretend it is empty. */
	return;
}

/* Turn the FQDN option space into a DHCPv6 FQDN option buffer.
 */
int
fqdn6_option_space_encapsulate(struct data_string *result,
			       struct packet *packet, struct lease *lease,
			       struct client_state *client_state,
			       struct option_state *in_options,
			       struct option_state *cfg_options,
			       struct binding_scope **scope,
			       struct universe *universe)
{
	pair ocp;
	struct option_chain_head *head;
	struct option_cache *oc;
	unsigned char *data;
	int i, len, rval = 0, count;
	struct data_string results[FQDN_SUBOPTION_COUNT + 1];

	if (fqdn_universe.index >= cfg_options->universe_count)
		return 0;
	head = ((struct option_chain_head *)
		cfg_options->universes[fqdn_universe.index]);
	if (head == NULL)
		return 0;

	memset(results, 0, sizeof(results));
	for (ocp = head->first ; ocp != NULL ; ocp = ocp->cdr) {
		oc = (struct option_cache *)(ocp->car);
		if (oc->option->code > FQDN_SUBOPTION_COUNT)
			log_fatal("Impossible condition at %s:%d.", MDL);
		/* No need to check the return code, we check the length later */
		(void) evaluate_option_cache(&results[oc->option->code], packet,
					     lease, client_state, in_options,
					     cfg_options, scope, oc, MDL);
	}

	/* We add a byte for the flags field at the start of the option.
	 * We add a byte because we will prepend a label count.
	 * We add a byte because the input length doesn't include a trailing
	 * NULL, and we will add a root label.
	 */
	len = results[FQDN_FQDN].len + 3;
	if (!buffer_allocate(&result->buffer, len, MDL)) {
		log_error("No memory for virtual option buffer.");
		goto exit;
	}
	data = result->buffer->data;
	result->data = data;

	/* The first byte is the flags field. */
	result->len = 1;
	data[0] = 0;
	/* XXX: The server should set bit 3 (yes, 3, not 4) to 1 if we
	 * are not going to perform any DNS updates.  The problem is
	 * that at this layer of abstraction, we do not know if the caller
	 * is the client or the server.
	 *
	 * See RFC4704 Section 4.1, 'The "N" bit'.
	 *
	 * if (?)
	 *	data[0] |= 4;
	 */
	if (results[FQDN_NO_CLIENT_UPDATE].len &&
	    results[FQDN_NO_CLIENT_UPDATE].data[0])
		data[0] |= 2;
	if (results[FQDN_SERVER_UPDATE].len &&
	    results[FQDN_SERVER_UPDATE].data[0])
		data[0] |= 1;

	/* If there is no name, we're done. */
	if (results[FQDN_FQDN].len == 0) {
		rval = 1;
		goto exit;
	}

	/* Convert textual representation to DNS format. */
	count = fqdn_encode(data + 1, len - 1,
			    results[FQDN_FQDN].data, results[FQDN_FQDN].len);

	if (count < 0) {
		rval = 0;
		data_string_forget(result, MDL);
		goto exit;
	}

	result->len += count;
	result->terminated = 0;

	/* Success! */
	rval = 1;

      exit:
	for (i = 1 ; i <= FQDN_SUBOPTION_COUNT ; i++) {
		if (result[i].len)
			data_string_forget(&results[i], MDL);
	}

	return rval;
}

/* Read the DHCPv6 FQDN option's contents into the FQDN virtual space.
 */
int
fqdn6_universe_decode(struct option_state *options,
		      const unsigned char *buffer, unsigned length,
		      struct universe *u)
{
	struct buffer *bp = NULL;
	unsigned char *first_dot;
	int len, hlen, dlen;

	/* The FQDN option has to be at least 1 byte long. */
	if (length < 1)
		return 0;

	/* Save the contents of the option in a buffer.  There are 3
	 * one-byte values we record from the packet, so we go ahead
	 * and allocate a bigger buffer to accommodate them.  But the
	 * 'length' we got (because it is a DNS encoded string) is
	 * one longer than we need...so we only add two extra octets.
	 */
	if (!buffer_allocate(&bp, length + 2, MDL)) {
		log_error("No memory for dhcp6.fqdn option buffer.");
		return 0;
	}

	/* The v6 FQDN is always 'encoded' per DNS. */
	bp->data[0] = 1;
	if (!save_option_buffer(&fqdn_universe, options, bp,
				bp->data, 1, FQDN_ENCODED, 0))
		goto error;

	/* XXX: We need to process 'The "N" bit'. */

	if (buffer[0] & 1) /* server-update. */
		bp->data[2] = 1;
	else
		bp->data[2] = 0;

	if (!save_option_buffer(&fqdn_universe, options, bp, bp->data + 2, 1,
				FQDN_SERVER_UPDATE, 0))
		goto error;

	if (buffer[0] & 2) /* no-client-update. */
		bp->data[1] = 1;
	else
		bp->data[1] = 0;

	if (!save_option_buffer(&fqdn_universe, options, bp, bp->data + 1, 1,
				FQDN_NO_CLIENT_UPDATE, 0))
		goto error;

	/* Convert the domain name to textual representation for config. */
	len = MRns_name_ntop(buffer + 1, (char *)bp->data + 3, length - 1);
	if (len == -1) {
		log_error("Unable to convert dhcp6.fqdn domain name to "
			  "printable form.");
		goto error;
	}

	/* Save the domain name. */
	if (len > 0) {
		unsigned char *fqdn_start = bp->data + 3;

		if (!save_option_buffer(&fqdn_universe, options, bp,
					fqdn_start, len, FQDN_FQDN, 1))
			goto error;

		first_dot = (unsigned char *)strchr((char *)fqdn_start, '.');

		if (first_dot != NULL) {
			hlen = first_dot - fqdn_start;
			dlen = len - hlen;
		} else {
			hlen = len;
			dlen = 0;
		}

		if (!save_option_buffer(&fqdn_universe, options, bp,
					fqdn_start, len, FQDN_FQDN, 1) ||
		    ((hlen > 0) &&
		     !save_option_buffer(&fqdn_universe, options, bp,
					 fqdn_start, hlen,
					 FQDN_HOSTNAME, 0)) ||
		    ((dlen > 0) &&
		     !save_option_buffer(&fqdn_universe, options, bp,
					 first_dot, dlen, FQDN_DOMAINNAME, 0)))
				goto error;
	}

	buffer_dereference(&bp, MDL);
	return 1;

      error:
	buffer_dereference(&bp, MDL);
	return 0;
}

void option_space_foreach (struct packet *packet, struct lease *lease,
			   struct client_state *client_state,
			   struct option_state *in_options,
			   struct option_state *cfg_options,
			   struct binding_scope **scope,
			   struct universe *u, void *stuff,
			   void (*func) (struct option_cache *,
					 struct packet *,
					 struct lease *, struct client_state *,
					 struct option_state *,
					 struct option_state *,
					 struct binding_scope **,
					 struct universe *, void *))
{
	if (u -> foreach)
		(*u -> foreach) (packet, lease, client_state, in_options,
				 cfg_options, scope, u, stuff, func);
}

void suboption_foreach (struct packet *packet, struct lease *lease,
			struct client_state *client_state,
			struct option_state *in_options,
			struct option_state *cfg_options,
			struct binding_scope **scope,
			struct universe *u, void *stuff,
			void (*func) (struct option_cache *,
				      struct packet *,
				      struct lease *, struct client_state *,
				      struct option_state *,
				      struct option_state *,
				      struct binding_scope **,
				      struct universe *, void *),
			struct option_cache *oc,
			const char *vsname)
{
	struct universe *universe = find_option_universe (oc -> option,
							  vsname);
	if (universe -> foreach)
		(*universe -> foreach) (packet, lease, client_state,
					in_options, cfg_options,
					scope, universe, stuff, func);
}

void hashed_option_space_foreach (struct packet *packet, struct lease *lease,
				  struct client_state *client_state,
				  struct option_state *in_options,
				  struct option_state *cfg_options,
				  struct binding_scope **scope,
				  struct universe *u, void *stuff,
				  void (*func) (struct option_cache *,
						struct packet *,
						struct lease *,
						struct client_state *,
						struct option_state *,
						struct option_state *,
						struct binding_scope **,
						struct universe *, void *))
{
	pair *hash;
	int i;
	struct option_cache *oc;

	if (cfg_options -> universe_count <= u -> index)
		return;

	hash = cfg_options -> universes [u -> index];
	if (!hash)
		return;
	for (i = 0; i < OPTION_HASH_SIZE; i++) {
		pair p;
		/* XXX save _all_ options! XXX */
		for (p = hash [i]; p; p = p -> cdr) {
			oc = (struct option_cache *)p -> car;
			(*func) (oc, packet, lease, client_state,
				 in_options, cfg_options, scope, u, stuff);
		}
	}
}

void
save_linked_option(struct universe *universe, struct option_state *options,
		   struct option_cache *oc, isc_boolean_t appendp)
{
	pair *tail;
	struct option_chain_head *head;
	struct option_cache **ocloc;

	if (universe -> index >= options -> universe_count)
		return;
	head = ((struct option_chain_head *)
		options -> universes [universe -> index]);
	if (!head) {
		if (!option_chain_head_allocate (((struct option_chain_head **)
						  &options -> universes
						  [universe -> index]), MDL))
			return;
		head = ((struct option_chain_head *)
			options -> universes [universe -> index]);
	}

	/* Find the tail of the list. */
	for (tail = &head -> first; *tail; tail = &((*tail) -> cdr)) {
		ocloc = (struct option_cache **)&(*tail)->car;

		if (oc->option->code == (*ocloc)->option->code) {
			if (appendp) {
				do {
					ocloc = &(*ocloc)->next;
				} while (*ocloc != NULL);
			} else {
				option_cache_dereference(ocloc, MDL);
			}
			option_cache_reference(ocloc, oc, MDL);
			return;
		}
	}

	*tail = cons (0, 0);
	if (*tail) {
		option_cache_reference ((struct option_cache **)
					(&(*tail) -> car), oc, MDL);
	}
}

int linked_option_space_encapsulate (result, packet, lease, client_state,
				    in_options, cfg_options, scope, universe)
	struct data_string *result;
	struct packet *packet;
	struct lease *lease;
	struct client_state *client_state;
	struct option_state *in_options;
	struct option_state *cfg_options;
	struct binding_scope **scope;
	struct universe *universe;
{
	int status = 0;
	pair oc;
	struct option_chain_head *head;

	if (universe -> index >= cfg_options -> universe_count)
		return status;
	head = ((struct option_chain_head *)
		cfg_options -> universes [universe -> index]);
	if (!head)
		return status;

	for (oc = head -> first; oc; oc = oc -> cdr) {
		if (store_option (result, universe, packet,
				  lease, client_state, in_options, cfg_options,
				  scope, (struct option_cache *)(oc -> car)))
			status = 1;
	}

	if (search_subencapsulation(result, packet, lease, client_state,
				    in_options, cfg_options, scope, universe))
		status = 1;

	return status;
}

void delete_linked_option (universe, options, code)
	struct universe *universe;
	struct option_state *options;
	int code;
{
	pair *tail, tmp = (pair)0;
	struct option_chain_head *head;

	if (universe -> index >= options -> universe_count)
		return;
	head = ((struct option_chain_head *)
		options -> universes [universe -> index]);
	if (!head)
		return;

	for (tail = &head -> first; *tail; tail = &((*tail) -> cdr)) {
		if (code ==
		    ((struct option_cache *)(*tail) -> car) -> option -> code)
		{
			tmp = (*tail) -> cdr;
			option_cache_dereference ((struct option_cache **)
						  (&(*tail) -> car), MDL);
			dfree (*tail, MDL);
			(*tail) = tmp;
			break;
		}
	}
}

struct option_cache *lookup_linked_option (universe, options, code)
	struct universe *universe;
	struct option_state *options;
	unsigned code;
{
	pair oc;
	struct option_chain_head *head;

	if (universe -> index >= options -> universe_count)
		return 0;
	head = ((struct option_chain_head *)
		options -> universes [universe -> index]);
	if (!head)
		return 0;

	for (oc = head -> first; oc; oc = oc -> cdr) {
		if (code ==
		    ((struct option_cache *)(oc -> car)) -> option -> code) {
			return (struct option_cache *)(oc -> car);
		}
	}

	return (struct option_cache *)0;
}

int linked_option_state_dereference (universe, state, file, line)
	struct universe *universe;
	struct option_state *state;
	const char *file;
	int line;
{
	return (option_chain_head_dereference
		((struct option_chain_head **)
		 (&state -> universes [universe -> index]), MDL));
}

void linked_option_space_foreach (struct packet *packet, struct lease *lease,
				  struct client_state *client_state,
				  struct option_state *in_options,
				  struct option_state *cfg_options,
				  struct binding_scope **scope,
				  struct universe *u, void *stuff,
				  void (*func) (struct option_cache *,
						struct packet *,
						struct lease *,
						struct client_state *,
						struct option_state *,
						struct option_state *,
						struct binding_scope **,
						struct universe *, void *))
{
	pair car;
	struct option_chain_head *head;

	if (u -> index >= cfg_options -> universe_count)
		return;
	head = ((struct option_chain_head *)
		cfg_options -> universes [u -> index]);
	if (!head)
		return;
	for (car = head -> first; car; car = car -> cdr) {
		(*func) ((struct option_cache *)(car -> car),
			 packet, lease, client_state,
			 in_options, cfg_options, scope, u, stuff);
	}
}

void do_packet (interface, packet, len, from_port, from, hfrom)
	struct interface_info *interface;
	struct dhcp_packet *packet;
	unsigned len;
	unsigned int from_port;
	struct iaddr from;
	struct hardware *hfrom;
{
	struct option_cache *op;
	struct packet *decoded_packet;
#if defined (DEBUG_MEMORY_LEAKAGE)
	unsigned long previous_outstanding = dmalloc_outstanding;
#endif

#if defined (TRACING)
	trace_inpacket_stash(interface, packet, len, from_port, from, hfrom);
#endif

	decoded_packet = NULL;
	if (!packet_allocate(&decoded_packet, MDL)) {
		log_error("do_packet: no memory for incoming packet!");
		return;
	}
	decoded_packet->raw = packet;
	decoded_packet->packet_length = len;
	decoded_packet->client_port = from_port;
	decoded_packet->client_addr = from;
	interface_reference(&decoded_packet->interface, interface, MDL);
	decoded_packet->haddr = hfrom;

	if (packet->hlen > sizeof packet->chaddr) {
		packet_dereference(&decoded_packet, MDL);
		log_info("Discarding packet with bogus hlen.");
		return;
	}

	/* If there's an option buffer, try to parse it. */
	if (decoded_packet->packet_length >= DHCP_FIXED_NON_UDP + 4) {
		if (!parse_options(decoded_packet)) {
			if (decoded_packet->options)
				option_state_dereference
					(&decoded_packet->options, MDL);
			packet_dereference (&decoded_packet, MDL);
			return;
		}

		if (decoded_packet->options_valid &&
		    (op = lookup_option(&dhcp_universe,
					decoded_packet->options, 
					DHO_DHCP_MESSAGE_TYPE))) {
			struct data_string dp;
			memset(&dp, 0, sizeof dp);
			evaluate_option_cache(&dp, decoded_packet, NULL, NULL,
					      decoded_packet->options, NULL,
					      NULL, op, MDL);
			if (dp.len > 0)
				decoded_packet->packet_type = dp.data[0];
			else
				decoded_packet->packet_type = 0;
			data_string_forget(&dp, MDL);
		}
	}

	if (validate_packet(decoded_packet) != 0) {
		if (decoded_packet->packet_type)
			dhcp(decoded_packet);
		else
			bootp(decoded_packet);
	}

	/* If the caller kept the packet, they'll have upped the refcnt. */
	packet_dereference(&decoded_packet, MDL);

#if defined (DEBUG_MEMORY_LEAKAGE)
	log_info("generation %ld: %ld new, %ld outstanding, %ld long-term",
		 dmalloc_generation,
		 dmalloc_outstanding - previous_outstanding,
		 dmalloc_outstanding, dmalloc_longterm);
	dmalloc_dump_outstanding();
#endif
#if defined (DEBUG_RC_HISTORY_EXHAUSTIVELY)
	dump_rc_history(0);
#endif
}

int
packet6_len_okay(const char *packet, int len) {
	if (len < 1) {
		return 0;
	}
	if ((packet[0] == DHCPV6_RELAY_FORW) || 
	    (packet[0] == DHCPV6_RELAY_REPL)) {
		if (len >= offsetof(struct dhcpv6_relay_packet, options)) {
			return 1;
		} else {
			return 0;
		}
	} else {
		if (len >= offsetof(struct dhcpv6_packet, options)) {
			return 1;
		} else {
			return 0;
		}
	}
}

#ifdef DHCPv6
void 
do_packet6(struct interface_info *interface, const char *packet, 
	   int len, int from_port, const struct iaddr *from, 
	   isc_boolean_t was_unicast) {
	unsigned char msg_type;
	const struct dhcpv6_packet *msg;
	const struct dhcpv6_relay_packet *relay; 
	struct packet *decoded_packet;
#if defined (DEBUG_MEMORY_LEAKAGE)
	unsigned long previous_outstanding = dmalloc_outstanding;
#endif

	if (!packet6_len_okay(packet, len)) {
		log_info("do_packet6: "
			 "short packet from %s port %d, len %d, dropped",
			 piaddr(*from), from_port, len);
		return;
	}

	decoded_packet = NULL;
	if (!packet_allocate(&decoded_packet, MDL)) {
		log_error("do_packet6: no memory for incoming packet.");
		return;
	}

	if (!option_state_allocate(&decoded_packet->options, MDL)) {
		log_error("do_packet6: no memory for options.");
		packet_dereference(&decoded_packet, MDL);
		return;
	}

	/* IPv4 information, already set to 0 */
	/* decoded_packet->packet_type = 0; */
	/* memset(&decoded_packet->haddr, 0, sizeof(decoded_packet->haddr)); */
	/* decoded_packet->circuit_id = NULL; */
	/* decoded_packet->circuit_id_len = 0; */
	/* decoded_packet->remote_id = NULL; */
	/* decoded_packet->remote_id_len = 0; */
	decoded_packet->raw = (struct dhcp_packet *)packet;
	decoded_packet->packet_length = (unsigned)len;
	decoded_packet->client_port = from_port;
	decoded_packet->client_addr = *from;
	interface_reference(&decoded_packet->interface, interface, MDL);

	decoded_packet->unicast = was_unicast;

	msg_type = packet[0];
	if ((msg_type == DHCPV6_RELAY_FORW) || 
	    (msg_type == DHCPV6_RELAY_REPL)) {
		int relaylen = (int)(offsetof(struct dhcpv6_relay_packet, options));
		relay = (const struct dhcpv6_relay_packet *)packet;
		decoded_packet->dhcpv6_msg_type = relay->msg_type;

		/* relay-specific data */
		decoded_packet->dhcpv6_hop_count = relay->hop_count;
		memcpy(&decoded_packet->dhcpv6_link_address,
		       relay->link_address, sizeof(relay->link_address));
		memcpy(&decoded_packet->dhcpv6_peer_address,
		       relay->peer_address, sizeof(relay->peer_address));

		if (!parse_option_buffer(decoded_packet->options, 
					 relay->options, len - relaylen, 
					 &dhcpv6_universe)) {
			/* no logging here, as parse_option_buffer() logs all
			   cases where it fails */
			packet_dereference(&decoded_packet, MDL);
			return;
		}
	} else {
		int msglen = (int)(offsetof(struct dhcpv6_packet, options));
		msg = (const struct dhcpv6_packet *)packet;
		decoded_packet->dhcpv6_msg_type = msg->msg_type;

		/* message-specific data */
		memcpy(decoded_packet->dhcpv6_transaction_id, 
		       msg->transaction_id, 
		       sizeof(decoded_packet->dhcpv6_transaction_id));

		if (!parse_option_buffer(decoded_packet->options, 
					 msg->options, len - msglen, 
					 &dhcpv6_universe)) {
			/* no logging here, as parse_option_buffer() logs all
			   cases where it fails */
			packet_dereference(&decoded_packet, MDL);
			return;
		}
	}

	dhcpv6(decoded_packet);

	packet_dereference(&decoded_packet, MDL);

#if defined (DEBUG_MEMORY_LEAKAGE)
	log_info("generation %ld: %ld new, %ld outstanding, %ld long-term",
		 dmalloc_generation,
		 dmalloc_outstanding - previous_outstanding,
		 dmalloc_outstanding, dmalloc_longterm);
	dmalloc_dump_outstanding();
#endif
#if defined (DEBUG_RC_HISTORY_EXHAUSTIVELY)
	dump_rc_history(0);
#endif
}
#endif /* DHCPv6 */

int
pretty_escape(char **dst, char *dend, const unsigned char **src,
	      const unsigned char *send)
{
	int count = 0;

	/* If there aren't as many bytes left as there are in the source
	 * buffer, don't even bother entering the loop.
	 */
	if (dst == NULL || dend == NULL || src == NULL || send == NULL ||
	    *dst == NULL || *src == NULL || (*dst >= dend) || (*src > send) ||
	    ((send - *src) > (dend - *dst)))
		return -1;

	for ( ; *src < send ; (*src)++) {
		if (!isascii (**src) || !isprint (**src)) {
			/* Skip trailing NUL. */
			if ((*src + 1) != send || **src != '\0') {
				if (*dst + 4 > dend)
					return -1;

				sprintf(*dst, "\\%03o",
					**src);
				(*dst) += 4;
				count += 4;
			}
		} else if (**src == '"' || **src == '\'' || **src == '$' ||
			   **src == '`' || **src == '\\' || **src == '|' ||
			   **src == '&') {
			if (*dst + 2 > dend)
				return -1;

			**dst = '\\';
			(*dst)++;
			**dst = **src;
			(*dst)++;
			count += 2;
		} else {
			if (*dst + 1 > dend)
				return -1;

			**dst = **src;
			(*dst)++;
			count++;
		}
	}

	return count;
}

static int
pretty_text(char **dst, char *dend, const unsigned char **src,
	    const unsigned char *send, int emit_quotes)
{
	int count;

	if (dst == NULL || dend == NULL || src == NULL || send == NULL ||
	    *dst == NULL || *src == NULL ||
	    ((*dst + (emit_quotes ? 2 : 0)) > dend) || (*src > send))
		return -1;

	if (emit_quotes) {
		**dst = '"';
		(*dst)++;
	}

	/* dend-1 leaves 1 byte for the closing quote. */
	count = pretty_escape(dst, dend - (emit_quotes ? 1 : 0), src, send);

	if (count == -1)
		return -1;

	if (emit_quotes && (*dst < dend)) {
		**dst = '"';
		(*dst)++;

		/* Includes quote prior to pretty_escape(); */
		count += 2;
	}

	return count;
}

static int
pretty_domain(char **dst, char *dend, const unsigned char **src,
	      const unsigned char *send)
{
	const unsigned char *tend;
	int count = 2;
	int tsiz, status;

	if (dst == NULL || dend == NULL || src == NULL || send == NULL ||
	    *dst == NULL || *src == NULL ||
	    ((*dst + 2) > dend) || (*src >= send))
		return -1;

	**dst = '"';
	(*dst)++;

	do {
		/* Continue loop until end of src buffer. */
		if (*src >= send)
			break;

		/* Consume tag size. */
		tsiz = **src;
		(*src)++;

		/* At root, finis. */
		if (tsiz == 0)
			break;

		tend = (*src) + tsiz;

		/* If the tag exceeds the source buffer, it's illegal.
		 * This should also trap compression pointers (which should
		 * not be in these buffers).
		 */
		if (tend > send)
			return -1;

		/* dend-2 leaves room for a trailing dot and quote. */
		status = pretty_escape(dst, dend-2, src, tend);

		if ((status == -1) || ((*dst + 2) > dend))
			return -1;

		**dst = '.';
		(*dst)++;
		count += status + 1;
	}
	while(1);

	**dst = '"';
	(*dst)++;

	return count;
}

/*
 * Add the option identified with the option number and data to the
 * options state.
 */
int
add_option(struct option_state *options,
	   unsigned int option_num,
	   void *data,
	   unsigned int data_len)
{
	struct option_cache *oc;
	struct option *option;

	/* INSIST(options != NULL); */
	/* INSIST(data != NULL); */

	option = NULL;
	if (!option_code_hash_lookup(&option, dhcp_universe.code_hash, 
				     &option_num, 0, MDL)) {
		log_error("Attempting to add unknown option %d.", option_num);
		return 0;
	}

	oc = NULL;
	if (!option_cache_allocate(&oc, MDL)) {
		log_error("No memory for option cache adding %s (option %d).",
			  option->name, option_num);
		return 0;
	}

	if (!make_const_data(&oc->expression, 
			     data, 
			     data_len,
			     0, 
			     0, 
			     MDL)) {
		log_error("No memory for constant data adding %s (option %d).",
			  option->name, option_num);
		option_cache_dereference(&oc, MDL);
		return 0;
	}

	option_reference(&(oc->option), option, MDL);
	save_option(&dhcp_universe, options, oc);
	option_cache_dereference(&oc, MDL);

	return 1;
}

/**
 *  Checks if received BOOTP/DHCPv4 packet is sane
 *
 * @param packet received, decoded packet
 *
 * @return 1 if packet is sane, 0 if it is not
 */
int validate_packet(struct packet *packet)
{
	struct option_cache *oc = NULL;

	oc = lookup_option (&dhcp_universe, packet->options,
			    DHO_DHCP_CLIENT_IDENTIFIER);
	if (oc) {
		/* Let's check if client-identifier is sane */
		if (oc->data.len == 0) {
			log_debug("Dropped DHCPv4 packet with zero-length client-id");
			return (0);

		} else if (oc->data.len == 1) {
			/*
			 * RFC2132, section 9.14 states that minimum length of client-id
			 * is 2.  We will allow single-character client-ids for now (for
			 * backwards compatibility), but warn the user that support for
			 * this is against the standard.
			 */
			log_debug("Accepted DHCPv4 packet with one-character client-id - "
				"a future version of ISC DHCP will reject this");
		}
	} else {
		/* 
		 * If hlen is 0 we don't have any identifier, we warn the user
		 * but continue processing the packet as we can.
		 */
		if (packet->raw->hlen == 0) {
			log_debug("Received DHCPv4 packet without client-id"
				  " option and empty hlen field.");
		}
	}

	/* @todo: Add checks for other received options */

	return (1);
}
