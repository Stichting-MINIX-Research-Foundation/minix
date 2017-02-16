/*	$NetBSD: ddns.c,v 1.4 2014/07/12 12:09:38 spz Exp $	*/
/* ddns.c

   Dynamic DNS updates. */

/*
 * 
 * Copyright (c) 2009-2014 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 2004-2007 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 2000-2003 by Internet Software Consortium
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
 * This software has been donated to Internet Systems Consortium
 * by Damien Neil of Nominum, Inc.
 *
 * To learn more about Internet Systems Consortium, see
 * ``https://www.isc.org/''.   To learn more about Nominum, Inc., see
 * ``http://www.nominum.com''.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: ddns.c,v 1.4 2014/07/12 12:09:38 spz Exp $");

#include "dhcpd.h"
#include "dst/md5.h"
#include <dns/result.h>

char *ddns_standard_tag = "ddns-dhcid";
char *ddns_interim_tag  = "ddns-txt";

#ifdef NSUPDATE

static void ddns_fwd_srv_connector(struct lease          *lease,
				   struct iasubopt       *lease6,
				   struct binding_scope **inscope,
				   dhcp_ddns_cb_t        *ddns_cb,
				   isc_result_t           eresult);

/* DN: No way of checking that there is enough space in a data_string's
   buffer.  Be certain to allocate enough!
   TL: This is why the expression evaluation code allocates a *new*
   data_string.   :') */
static void data_string_append (struct data_string *ds1,
				struct data_string *ds2)
{
	memcpy (ds1 -> buffer -> data + ds1 -> len,
		ds2 -> data,
		ds2 -> len);
	ds1 -> len += ds2 -> len;
}


/* Determine what, if any, forward and reverse updates need to be
 * performed, and carry them through.
 */
int
ddns_updates(struct packet *packet, struct lease *lease, struct lease *old,
	     struct iasubopt *lease6, struct iasubopt *old6,
	     struct option_state *options)
{
	unsigned long ddns_ttl = DEFAULT_DDNS_TTL;
	struct data_string ddns_hostname;
	struct data_string ddns_domainname;
	struct data_string old_ddns_fwd_name;
	struct data_string ddns_fwd_name;
	struct data_string ddns_dhcid;
	struct binding_scope **scope = NULL;
	struct data_string d1;
	struct option_cache *oc;
	int s1, s2;
	int result = 0;
	int server_updates_a = 1;
	struct buffer *bp = (struct buffer *)0;
	int ignorep = 0, client_ignorep = 0;
	int rev_name_len;
	int i;

	dhcp_ddns_cb_t *ddns_cb;
	int do_remove = 0;

	if ((ddns_update_style != DDNS_UPDATE_STYLE_STANDARD) &&
	    (ddns_update_style != DDNS_UPDATE_STYLE_INTERIM))
		return (0);

	/*
	 * sigh, I want to cancel any previous udpates before we do anything
	 * else but this means we need to deal with the lease vs lease6
	 * question twice.
	 * If there is a ddns request already outstanding cancel it.
	 */

	if (lease != NULL) {
		if ((old != NULL) && (old->ddns_cb != NULL)) {
			ddns_cancel(old->ddns_cb, MDL);
			old->ddns_cb = NULL;
		}
	} else if (lease6 != NULL) {
		if ((old6 != NULL) && (old6->ddns_cb != NULL)) {
			ddns_cancel(old6->ddns_cb, MDL);
			old6->ddns_cb = NULL;
		}
	} else {
		log_fatal("Impossible condition at %s:%d.", MDL);
		/* Silence compiler warnings. */
		result = 0;
		return(0);
	}

	/* allocate our control block */
	ddns_cb = ddns_cb_alloc(MDL);
	if (ddns_cb == NULL) {
		return(0);
	}
	/*
	 * Assume that we shall update both the A and ptr records and,
	 * as this is an update, set the active flag 
	 */
	ddns_cb->flags = DDNS_UPDATE_ADDR | DDNS_UPDATE_PTR |
		DDNS_ACTIVE_LEASE;

	/*
	 * For v4 we flag static leases so we don't try
	 * and manipulate the lease later.  For v6 we don't
	 * get static leases and don't need to flag them.
	 */
	if (lease != NULL) {
		scope = &(lease->scope);
		ddns_cb->address = lease->ip_addr;
		if (lease->flags & STATIC_LEASE)
			ddns_cb->flags |= DDNS_STATIC_LEASE;
	} else if (lease6 != NULL) {
		scope = &(lease6->scope);
		memcpy(ddns_cb->address.iabuf, lease6->addr.s6_addr, 16);
		ddns_cb->address.len = 16;
	}

	memset (&d1, 0, sizeof(d1));
	memset (&ddns_hostname, 0, sizeof (ddns_hostname));
	memset (&ddns_domainname, 0, sizeof (ddns_domainname));
	memset (&old_ddns_fwd_name, 0, sizeof (ddns_fwd_name));
	memset (&ddns_fwd_name, 0, sizeof (ddns_fwd_name));
	memset (&ddns_dhcid, 0, sizeof (ddns_dhcid));

	/* If we are allowed to accept the client's update of its own A
	   record, see if the client wants to update its own A record. */
	if (!(oc = lookup_option(&server_universe, options,
				 SV_CLIENT_UPDATES)) ||
	    evaluate_boolean_option_cache(&client_ignorep, packet, lease, NULL,
					  packet->options, options, scope,
					  oc, MDL)) {
		/* If there's no fqdn.no-client-update or if it's
		   nonzero, don't try to use the client-supplied
		   XXX */
		if (!(oc = lookup_option (&fqdn_universe, packet -> options,
					  FQDN_SERVER_UPDATE)) ||
		    evaluate_boolean_option_cache(&ignorep, packet, lease,
						  NULL, packet->options,
						  options, scope, oc, MDL))
			goto noclient;
		/* Win98 and Win2k will happily claim to be willing to
		   update an unqualified domain name. */
		if (!(oc = lookup_option (&fqdn_universe, packet -> options,
					  FQDN_DOMAINNAME)))
			goto noclient;
		if (!(oc = lookup_option (&fqdn_universe, packet -> options,
					  FQDN_FQDN)) ||
		    !evaluate_option_cache(&ddns_fwd_name, packet, lease,
					   NULL, packet->options,
					   options, scope, oc, MDL))
			goto noclient;
		ddns_cb->flags &= ~DDNS_UPDATE_ADDR;
		server_updates_a = 0;
		goto client_updates;
	}
      noclient:
	/* If do-forward-updates is disabled, this basically means don't
	   do an update unless the client is participating, so if we get
	   here and do-forward-updates is disabled, we can stop. */
	if ((oc = lookup_option (&server_universe, options,
				 SV_DO_FORWARD_UPDATES)) &&
	    !evaluate_boolean_option_cache(&ignorep, packet, lease,
					   NULL, packet->options,
					   options, scope, oc, MDL)) {
		goto out;
	}

	/* If it's a static lease, then don't do the DNS update unless we're
	   specifically configured to do so.   If the client asked to do its
	   own update and we allowed that, we don't do this test. */
	/* XXX: note that we cannot detect static DHCPv6 leases. */
	if ((lease != NULL) && (lease->flags & STATIC_LEASE)) {
		if (!(oc = lookup_option(&server_universe, options,
					 SV_UPDATE_STATIC_LEASES)) ||
		    !evaluate_boolean_option_cache(&ignorep, packet, lease,
						   NULL, packet->options,
						   options, scope, oc, MDL))
			goto out;
	}

	/*
	 * Compute the name for the A record.
	 */
	oc = lookup_option(&server_universe, options, SV_DDNS_HOST_NAME);
	if (oc)
		s1 = evaluate_option_cache(&ddns_hostname, packet, lease,
					   NULL, packet->options,
					   options, scope, oc, MDL);
	else
		s1 = 0;

	oc = lookup_option(&server_universe, options, SV_DDNS_DOMAIN_NAME);
	if (oc)
		s2 = evaluate_option_cache(&ddns_domainname, packet, lease,
					   NULL, packet->options,
					   options, scope, oc, MDL);
	else
		s2 = 0;

	if (s1 && s2) {
		if (ddns_hostname.len + ddns_domainname.len > 253) {
			log_error ("ddns_update: host.domain name too long");

			goto out;
		}

		buffer_allocate (&ddns_fwd_name.buffer,
				 ddns_hostname.len + ddns_domainname.len + 2,
				 MDL);
		if (ddns_fwd_name.buffer) {
			ddns_fwd_name.data = ddns_fwd_name.buffer->data;
			data_string_append (&ddns_fwd_name, &ddns_hostname);
			ddns_fwd_name.buffer->data[ddns_fwd_name.len] = '.';
			ddns_fwd_name.len++;
			data_string_append (&ddns_fwd_name, &ddns_domainname);
			ddns_fwd_name.buffer->data[ddns_fwd_name.len] ='\0';
			ddns_fwd_name.terminated = 1;
		}
	}
      client_updates:

	/* See if there's a name already stored on the lease. */
	if (find_bound_string(&old_ddns_fwd_name, *scope, "ddns-fwd-name")) {
		/* If there is, see if it's different. */
		if (old_ddns_fwd_name.len != ddns_fwd_name.len ||
		    memcmp (old_ddns_fwd_name.data, ddns_fwd_name.data,
			    old_ddns_fwd_name.len)) {
			/*
			 * If the name is different, mark the old record
			 * for deletion and continue getting the new info.
			 */
			do_remove = 1;
			goto in;
		}

#if defined  (DDNS_UPDATE_SLOW_TRANSITION)
		/*
		 * If the slow transition code is enabled check to see
		 * if the stored type (standard or interim doesn't
		 * match the type currently in use.  If it doesn't
		 * try to remove and replace the DNS record
		 */
		if (((ddns_update_style == DDNS_UPDATE_STYLE_STANDARD) &&
		     find_bound_string(&ddns_dhcid, *scope, ddns_interim_tag)) ||
		    ((ddns_update_style == DDNS_UPDATE_STYLE_INTERIM) &&
		     find_bound_string(&ddns_dhcid, *scope, ddns_standard_tag))) {
			data_string_forget(&ddns_dhcid, MDL);
			do_remove = 1;
			goto in;
		}
#endif
		   
		/* See if the administrator wants to do updates even
		   in cases where the update already appears to have been
		   done. */
		if (!(oc = lookup_option(&server_universe, options,
					 SV_UPDATE_OPTIMIZATION)) ||
		    evaluate_boolean_option_cache(&ignorep, packet, lease,
						  NULL, packet->options,
						  options, scope, oc, MDL)) {
			result = 1;
			goto noerror;
		}
	/* If there's no "ddns-fwd-name" on the lease record, see if
	 * there's a ddns-client-fqdn indicating a previous client
	 * update (if it changes, we need to adjust the PTR).
	 */
	} else if (find_bound_string(&old_ddns_fwd_name, *scope,
				     "ddns-client-fqdn")) {
		/* If the name is not different, no need to update
		   the PTR record. */
		if (old_ddns_fwd_name.len == ddns_fwd_name.len &&
		    !memcmp (old_ddns_fwd_name.data, ddns_fwd_name.data,
			     old_ddns_fwd_name.len) &&
		    (!(oc = lookup_option(&server_universe, options,
					  SV_UPDATE_OPTIMIZATION)) ||
		     evaluate_boolean_option_cache(&ignorep, packet, lease,
						   NULL, packet->options,
						   options, scope, oc, MDL))) {
			goto noerror;
		}
	}
      in:
		
	/* If we don't have a name that the client has been assigned, we
	   can just skip all this. */

	if ((!ddns_fwd_name.len) || (ddns_fwd_name.len > 255)) {
		if (ddns_fwd_name.len > 255) {
			log_error ("client provided fqdn: too long");
		}

		/* If desired do the removals */
		if (do_remove != 0) {
			(void) ddns_removals(lease, lease6, NULL, ISC_TRUE);
		}
		goto out;
	}

	/*
	 * Compute the RR TTL.
	 *
	 * We have two ways of computing the TTL.
	 * The old behavior was to allow for the customer to set up
	 * the option or to default things.  For v4 this was 1/2
	 * of the lease time, for v6 this was DEFAULT_DDNS_TTL.
	 * The new behavior continues to allow the customer to set
	 * up an option but the defaults are a little different.
	 * We now use 1/2 of the (preferred) lease time for both
	 * v4 and v6 and cap them at a maximum value. 
	 * If the customer chooses to use an experession that references
	 * part of the lease the v6 value will be the default as there
	 * isn't a lease available for v6.
	 */

	ddns_ttl = DEFAULT_DDNS_TTL;
	if (lease != NULL) {
		if (lease->ends <= cur_time) {
			ddns_ttl = 0;
		} else {
			ddns_ttl = (lease->ends - cur_time)/2;
		}
	}
#ifndef USE_OLD_DDNS_TTL
	else if (lease6 != NULL) {
		ddns_ttl = lease6->prefer/2;
	}

	if (ddns_ttl > MAX_DEFAULT_DDNS_TTL) {
		ddns_ttl = MAX_DEFAULT_DDNS_TTL;
	}
#endif 		

	if ((oc = lookup_option(&server_universe, options, SV_DDNS_TTL))) {
		if (evaluate_option_cache(&d1, packet, lease, NULL,
					  packet->options, options,
					  scope, oc, MDL)) {
			if (d1.len == sizeof (u_int32_t))
				ddns_ttl = getULong (d1.data);
			data_string_forget (&d1, MDL);
		}
	}

	ddns_cb->ttl = ddns_ttl;

	/*
	 * Compute the reverse IP name, starting with the domain name.
	 */
	oc = lookup_option(&server_universe, options, SV_DDNS_REV_DOMAIN_NAME);
	if (oc)
		s1 = evaluate_option_cache(&d1, packet, lease, NULL,
					   packet->options, options,
					   scope, oc, MDL);
	else
		s1 = 0;

	/* 
	 * Figure out the length of the part of the name that depends 
	 * on the address.
	 */
	if (ddns_cb->address.len == 4) {
		char buf[17];
		/* XXX: WOW this is gross. */
		rev_name_len = snprintf(buf, sizeof(buf), "%u.%u.%u.%u.",
					ddns_cb->address.iabuf[3] & 0xff,
					ddns_cb->address.iabuf[2] & 0xff,
					ddns_cb->address.iabuf[1] & 0xff,
					ddns_cb->address.iabuf[0] & 0xff) + 1;

		if (s1) {
			rev_name_len += d1.len;

			if (rev_name_len > 255) {
				log_error("ddns_update: Calculated rev domain "
					  "name too long.");
				s1 = 0;
				data_string_forget(&d1, MDL);
			}
		}
	} else if (ddns_cb->address.len == 16) {
		/* 
		 * IPv6 reverse names are always the same length, with 
		 * 32 hex characters separated by dots.
		 */
		rev_name_len = sizeof("0.1.2.3.4.5.6.7."
				      "8.9.a.b.c.d.e.f."
				      "0.1.2.3.4.5.6.7."
				      "8.9.a.b.c.d.e.f."
				      "ip6.arpa.");

		/* Set s1 to make sure we gate into updates. */
		s1 = 1;
	} else {
		log_fatal("invalid address length %d", ddns_cb->address.len);
		/* Silence compiler warnings. */
		return 0;
	}

	/* See if we are configured NOT to do reverse ptr updates */
	if ((oc = lookup_option(&server_universe, options,
				SV_DO_REVERSE_UPDATES)) &&
	    !evaluate_boolean_option_cache(&ignorep, packet, lease, NULL,
					   packet->options, options,
					   scope, oc, MDL)) {
		ddns_cb->flags &= ~DDNS_UPDATE_PTR;
	}

	if (s1) {
		buffer_allocate(&ddns_cb->rev_name.buffer, rev_name_len, MDL);
		if (ddns_cb->rev_name.buffer != NULL) {
			struct data_string *rname = &ddns_cb->rev_name;
			rname->data = rname->buffer->data;

			if (ddns_cb->address.len == 4) {
				rname->len =
				    sprintf((char *)rname->buffer->data,
					    "%u.%u.%u.%u.", 
					    ddns_cb->address.iabuf[3] & 0xff,
					    ddns_cb->address.iabuf[2] & 0xff,
					    ddns_cb->address.iabuf[1] & 0xff,
					    ddns_cb->address.iabuf[0] & 0xff);

				/*
				 * d1.data may be opaque, garbage bytes, from
				 * user (mis)configuration.
				 */
				data_string_append(rname, &d1);
				rname->buffer->data[rname->len] = '\0';
			} else if (ddns_cb->address.len == 16) {
				char *p = (char *)&rname->buffer->data;
				unsigned char *a = ddns_cb->address.iabuf + 15;
				for (i=0; i<16; i++) {
					sprintf(p, "%x.%x.", 
						(*a & 0xF), ((*a >> 4) & 0xF));
					p += 4;
					a -= 1;
				}
				strcat(p, "ip6.arpa.");
				rname->len = strlen((const char *)rname->data);
			}

			rname->terminated = 1;
		}

		if (d1.data != NULL)
			data_string_forget(&d1, MDL);
	}

	/*
	 * copy the string now so we can pass it to the dhcid routines
	 * via the ddns_cb pointer
	 */
	data_string_copy(&ddns_cb->fwd_name, &ddns_fwd_name, MDL);

	/*
	 * If we are updating the A record, compute the DHCID value.
	 * We have two options for computing the DHCID value, the older
	 * interim version and the newer standard version.  The interim
	 * has some issues but is left as is to avoid compatibility issues.
	 *
	 * We select the type of DHCID to construct and the information to
	 * use for the digest based on 4701 section 3.3
	 */
	if ((ddns_cb->flags & DDNS_UPDATE_ADDR) != 0) {
		int ddns_type;
		int ddns_len;
		if (ddns_update_style == DDNS_UPDATE_STYLE_STANDARD) {
			/* The standard style */
			ddns_cb->lease_tag = ddns_standard_tag;
			ddns_cb->dhcid_class = dns_rdatatype_dhcid;
			ddns_type = 1;
			ddns_len = 4;
		} else {
			/* The older interim style */
			ddns_cb->lease_tag = ddns_interim_tag;
			ddns_cb->dhcid_class = dns_rdatatype_txt;
			/* for backwards compatibility */
			ddns_type = DHO_DHCP_CLIENT_IDENTIFIER;
			/* IAID incorrectly included */
			ddns_len = 0;
		}


		if (lease6 != NULL) {
			if (lease6->ia->iaid_duid.len < ddns_len)
				goto badfqdn;
			result = get_dhcid(ddns_cb, 2,
					   lease6->ia->iaid_duid.data + ddns_len,
					   lease6->ia->iaid_duid.len - ddns_len);
		} else if ((lease != NULL) &&
			   (lease->uid != NULL) &&
			   (lease->uid_len != 0)) {
			/* If this is standard check for an RFC 4361
			 * compliant client identifier
			 */
			if ((ddns_update_style == DDNS_UPDATE_STYLE_STANDARD) &&
			    (lease->uid[0] == 255)) {
				if (lease->uid_len < 5)
					goto badfqdn;
				result = get_dhcid(ddns_cb, 2,
						   lease->uid + 5,
						   lease->uid_len - 5);
			} else {
				result = get_dhcid(ddns_cb, ddns_type,
						   lease->uid,
						   lease->uid_len);
			}
		} else if (lease != NULL)
			result = get_dhcid(ddns_cb, 0,
					   lease->hardware_addr.hbuf,
					   lease->hardware_addr.hlen);
		else
			log_fatal("Impossible condition at %s:%d.", MDL);

		if (!result)
			goto badfqdn;
	}

	/*
	 * Perform updates.
	 */

	if (ddns_cb->flags & DDNS_UPDATE_ADDR) {
		oc = lookup_option(&server_universe, options,
				   SV_DDNS_CONFLICT_DETECT);
		if (oc &&
		    !evaluate_boolean_option_cache(&ignorep, packet, lease,
						   NULL, packet->options,
						   options, scope, oc, MDL))
			ddns_cb->flags |= DDNS_CONFLICT_OVERRIDE;

	}

	/*
	 * Previously if we failed during the removal operations
	 * we skipped the fqdn option processing.  I'm not sure
	 * if we want to continue with that if we fail before sending
	 * the ddns messages.  Currently we don't.
	 */
	if (do_remove) {
		/*
		 * We should log a more specific error closer to the actual
		 * error if we want one. ddns_removal failure not logged here.
		 */
		 (void) ddns_removals(lease, lease6, ddns_cb, ISC_TRUE);
	}
	else {
		ddns_fwd_srv_connector(lease, lease6, scope, ddns_cb,
				       ISC_R_SUCCESS);
	}
	ddns_cb = NULL;

      noerror:
	/*
	 * If fqdn-reply option is disabled in dhcpd.conf, then don't
	 * send the client an FQDN option at all, even if one was requested.
	 * (WinXP clients allegedly misbehave if the option is present,
	 * refusing to handle PTR updates themselves).
	 */
	if ((oc = lookup_option (&server_universe, options, SV_FQDN_REPLY)) &&
  	    !evaluate_boolean_option_cache(&ignorep, packet, lease, NULL,
  					   packet->options, options,
  					   scope, oc, MDL)) {
  	    	goto badfqdn;

	/* If we're ignoring client updates, then we tell a sort of 'white
	 * lie'.  We've already updated the name the server wants (per the
	 * config written by the server admin).  Now let the client do as
	 * it pleases with the name they supplied (if any).
	 *
	 * We only form an FQDN option this way if the client supplied an
	 * FQDN option that had FQDN_SERVER_UPDATE set false.
	 */
	} else if (client_ignorep &&
	    (oc = lookup_option(&fqdn_universe, packet->options,
				FQDN_SERVER_UPDATE)) &&
	    !evaluate_boolean_option_cache(&ignorep, packet, lease, NULL,
					   packet->options, options,
					   scope, oc, MDL)) {
		oc = lookup_option(&fqdn_universe, packet->options, FQDN_FQDN);
		if (oc && evaluate_option_cache(&d1, packet, lease, NULL,
						packet->options, options,
						scope, oc, MDL)) {
			if (d1.len == 0 ||
			    !buffer_allocate(&bp, d1.len + 5, MDL))
				goto badfqdn;

			/* Server pretends it is not updating. */
			bp->data[0] = 0;
			if (!save_option_buffer(&fqdn_universe, options,
						bp, &bp->data[0], 1,
						FQDN_SERVER_UPDATE, 0))
				goto badfqdn;

			/* Client is encouraged to update. */
			bp->data[1] = 0;
			if (!save_option_buffer(&fqdn_universe, options,
						bp, &bp->data[1], 1,
						FQDN_NO_CLIENT_UPDATE, 0))
				goto badfqdn;

			/* Use the encoding of client's FQDN option. */
			oc = lookup_option(&fqdn_universe, packet->options,
					   FQDN_ENCODED);
			if (oc &&
			    evaluate_boolean_option_cache(&ignorep, packet,
							  lease, NULL,
							  packet->options,
							  options, scope,
							  oc, MDL))
				bp->data[2] = 1; /* FQDN is encoded. */
			else
				bp->data[2] = 0; /* FQDN is not encoded. */

			if (!save_option_buffer(&fqdn_universe, options,
						bp, &bp->data[2], 1,
						FQDN_ENCODED, 0))
				goto badfqdn;

			/* Current FQDN drafts indicate 255 is mandatory. */
			bp->data[3] = 255;
			if (!save_option_buffer(&fqdn_universe, options,
						bp, &bp->data[3], 1,
						FQDN_RCODE1, 0))
				goto badfqdn;

			bp->data[4] = 255;
			if (!save_option_buffer(&fqdn_universe, options,
						bp, &bp->data[4], 1,
						FQDN_RCODE2, 0))
				goto badfqdn;

			/* Copy in the FQDN supplied by the client.  Note well
			 * that the format of this option in the cache is going
			 * to be in text format.  If the fqdn supplied by the
			 * client is encoded, it is decoded into the option
			 * cache when parsed out of the packet.  It will be
			 * re-encoded when the option is assembled to be
			 * transmitted if the client elects that encoding.
			 */
			memcpy(&bp->data[5], d1.data, d1.len);
			if (!save_option_buffer(&fqdn_universe, options,
						bp, &bp->data[5], d1.len,
						FQDN_FQDN, 0))
				goto badfqdn;

			data_string_forget(&d1, MDL);
		}
	/* Set up the outgoing FQDN option if there was an incoming
	 * FQDN option.  If there's a valid FQDN option, there MUST
	 * be an FQDN_SERVER_UPDATES suboption, it's part of the fixed
	 * length head of the option contents, so we test the latter
	 * to detect the presence of the former.
	 */
	} else if ((oc = lookup_option(&fqdn_universe, packet->options,
				       FQDN_ENCODED)) &&
		   buffer_allocate(&bp, ddns_fwd_name.len + 5, MDL)) {
		bp -> data [0] = server_updates_a;
		if (!save_option_buffer(&fqdn_universe, options,
					bp, &bp->data [0], 1,
					FQDN_SERVER_UPDATE, 0))
			goto badfqdn;
		bp -> data [1] = server_updates_a;
		if (!save_option_buffer(&fqdn_universe, options,
					 bp, &bp->data [1], 1,
					 FQDN_NO_CLIENT_UPDATE, 0))
			goto badfqdn;

		/* Do the same encoding the client did. */
		if (evaluate_boolean_option_cache(&ignorep, packet, lease,
						  NULL, packet->options,
						  options, scope, oc, MDL))
			bp -> data [2] = 1;
		else
			bp -> data [2] = 0;
		if (!save_option_buffer(&fqdn_universe, options,
					bp, &bp->data [2], 1,
					FQDN_ENCODED, 0))
			goto badfqdn;
		bp -> data [3] = 255;//isc_rcode_to_ns (rcode1);
		if (!save_option_buffer(&fqdn_universe, options,
					bp, &bp->data [3], 1,
					FQDN_RCODE1, 0))
			goto badfqdn;
		bp -> data [4] = 255;//isc_rcode_to_ns (rcode2);
		if (!save_option_buffer(&fqdn_universe, options,
					bp, &bp->data [4], 1,
					FQDN_RCODE2, 0))
			goto badfqdn;
		if (ddns_fwd_name.len) {
		    memcpy (&bp -> data [5],
			    ddns_fwd_name.data, ddns_fwd_name.len);
		    if (!save_option_buffer(&fqdn_universe, options,
					     bp, &bp->data [5],
					     ddns_fwd_name.len,
					     FQDN_FQDN, 0))
			goto badfqdn;
		}
	}

      badfqdn:
      out:
	/*
	 * Final cleanup.
	 */
	if (ddns_cb != NULL) {
		ddns_cb_free(ddns_cb, MDL);
	}

	data_string_forget(&d1, MDL);
	data_string_forget(&ddns_hostname, MDL);
	data_string_forget(&ddns_domainname, MDL);
	data_string_forget(&old_ddns_fwd_name, MDL);
	data_string_forget(&ddns_fwd_name, MDL);
	if (bp)
		buffer_dereference(&bp, MDL);

	return result;
}

/*%<
 * Utility function to update text strings within a lease.
 *
 * The first issue is to find the proper scope.  Sometimes we shall be
 * called with a pointer to the scope in other cases we need to find
 * the proper lease and then get the scope.  Once we have the scope we update
 * the proper strings, as indicated by the state value in the control block.
 * Lastly, if we needed to find the scope we write it out, if we used a
 * scope that was passed as an argument we don't write it, assuming that
 * our caller (or his ...) will do the write.
 * 
 *\li ddns_cb - the control block for the DDNS request
 *
 *\li inscope - a pointer to the scope to update.  This may be NULL
 *    in which case we use the control block to find the lease and
 *    then the scope.
 *
 * Returns
 *\li ISC_R_SUCCESS
 *
 *\li ISC_R_FAILURE - The routine was unable to find an expected scope.
 *    In some cases (static and inactive leases) we don't expect a scope
 *    and return success.
 */

static isc_result_t
ddns_update_lease_text(dhcp_ddns_cb_t        *ddns_cb,
		       struct binding_scope **inscope)
{
	struct binding_scope **scope  = NULL;
	struct lease          *lease  = NULL;
	struct iasubopt       *lease6 = NULL;
	struct ipv6_pool      *pool   = NULL;
	struct in6_addr        addr;
	struct data_string     lease_dhcid;
	
	/*
	 * If the lease was static (for a fixed address)
	 * we don't need to do any work.
	 */
	if (ddns_cb->flags & DDNS_STATIC_LEASE)
		return (ISC_R_SUCCESS);

	/* 
	 * If we are processing an expired or released v6 lease
	 * or some types of v4 leases we don't actually have a
	 * scope to update
	 */
	if ((ddns_cb->flags & DDNS_ACTIVE_LEASE) == 0)
		return (ISC_R_SUCCESS);

	if (inscope != NULL) {
		scope = inscope;
	} else if (ddns_cb->address.len == 4) {
		if (find_lease_by_ip_addr(&lease, ddns_cb->address, MDL) != 0){
			scope = &(lease->scope);
		}
	} else if (ddns_cb->address.len == 16) {
		memcpy(&addr, &ddns_cb->address.iabuf, 16);
		if ((find_ipv6_pool(&pool, D6O_IA_TA, &addr) ==
		     ISC_R_SUCCESS) ||
		    (find_ipv6_pool(&pool, D6O_IA_NA, &addr) == 
		     ISC_R_SUCCESS)) {
			if (iasubopt_hash_lookup(&lease6,  pool->leases,
						 &addr, 16, MDL)) {
				scope = &(lease6->scope);
			}
			ipv6_pool_dereference(&pool, MDL);
		}
	} else {
		log_fatal("Impossible condition at %s:%d.", MDL);
	}

	if (scope == NULL) {
		/* If necessary get rid of the lease */
		if (lease) {
			lease_dereference(&lease, MDL);
		}
		else if (lease6) {
			iasubopt_dereference(&lease6, MDL);
		}

		return(ISC_R_FAILURE);
	}

	/* We now have a scope and can proceed to update it */
	switch(ddns_cb->state) {
	case DDNS_STATE_REM_PTR:
		unset(*scope, "ddns-rev-name");
		if ((ddns_cb->flags & DDNS_CLIENT_DID_UPDATE) != 0) {
			unset(*scope, "ddns-client-fqdn");
		}
		break;

	case DDNS_STATE_ADD_PTR:
	case DDNS_STATE_CLEANUP:
		bind_ds_value(scope, "ddns-rev-name", &ddns_cb->rev_name);
		if ((ddns_cb->flags & DDNS_UPDATE_ADDR) == 0) {
			bind_ds_value(scope, "ddns-client-fqdn",
				      &ddns_cb->fwd_name);
		}
		break;

	case DDNS_STATE_ADD_FW_YXDHCID:
	case DDNS_STATE_ADD_FW_NXDOMAIN:
		bind_ds_value(scope, "ddns-fwd-name", &ddns_cb->fwd_name);

		if (ddns_cb->lease_tag == ddns_standard_tag) {
			bind_ds_value(scope, ddns_standard_tag, &ddns_cb->dhcid);
		} else {
			/* convert from dns version to lease version of dhcid */
			memset(&lease_dhcid, 0, sizeof(lease_dhcid));
			dhcid_tolease(&ddns_cb->dhcid, &lease_dhcid);
			bind_ds_value(scope, ddns_interim_tag, &lease_dhcid);
			data_string_forget(&lease_dhcid, MDL);
		}
		break;

	case DDNS_STATE_REM_FW_NXRR:
	case DDNS_STATE_REM_FW_YXDHCID:
		unset(*scope, "ddns-fwd-name");
		unset(*scope, ddns_cb->lease_tag);
		break;
	}
		
	/* If necessary write it out and get rid of the lease */
	if (lease) {
		write_lease(lease);
		lease_dereference(&lease, MDL);
	} else if (lease6) {
		write_ia(lease6->ia);
		iasubopt_dereference(&lease6, MDL);
	}

	return(ISC_R_SUCCESS);
}

#ifdef notdef
/*
 * This function should be called when update_lease_ptr function fails.
 * It does inform user about the condition, provides some hints how to
 * resolve this and dies gracefully. This can happend in at least three
 * cases (all are configuration mistakes):
 * a) IPv4: user have duplicate fixed-address entries (the same
 *    address is defined twice). We may have found wrong lease.
 * b) IPv6: user have overlapping pools (we tried to find 
 *    a lease in a wrong pool)
 * c) IPv6: user have duplicate fixed-address6 entires (the same
 *    address is defined twice). We may have found wrong lease.
 *
 * Comment: while it would be possible to recover from both cases
 * by forcibly searching for leases in *all* following pools, that would
 * only hide the real problem - a misconfiguration. Proper solution
 * is to log the problem, die and let the user fix his config file.
 */
void
update_lease_failed(struct lease *lease,
		    struct iasubopt *lease6,
		    dhcp_ddns_cb_t  *ddns_cb,
		    dhcp_ddns_cb_t  *ddns_cb_set,
		    const char * file, int line)
{
	char lease_address[MAX_ADDRESS_STRING_LEN + 64];
	char reason[128]; /* likely reason */

	sprintf(reason, "unknown");
	sprintf(lease_address, "unknown");

	/*
	 * let's pretend that everything is ok, so we can continue for 
	 * information gathering purposes
	 */
	
	if (ddns_cb != NULL) {
		strncpy(lease_address, piaddr(ddns_cb->address), 
			MAX_ADDRESS_STRING_LEN);
		
		if (ddns_cb->address.len == 4) {
			sprintf(reason, "duplicate IPv4 fixed-address entry");
		} else if (ddns_cb->address.len == 16) {
			sprintf(reason, "duplicate IPv6 fixed-address6 entry "
				"or overlapping pools");
		} else {
			/* 
			 * Should not happen. We have non-IPv4, non-IPv6 
			 * address. Something is very wrong here.
			 */
			sprintf(reason, "corrupted ddns_cb structure (address "
				"length is %d)", ddns_cb->address.len);
		}
	}
	
	log_error("Failed to properly update internal lease structure with "
		  "DDNS");
	log_error("control block structures. Tried to update lease for"
		  "%s address, ddns_cb=%p.", lease_address, ddns_cb);

	log_error("%s", "");
	log_error("This condition can occur, if DHCP server configuration is "
		  "inconsistent.");
	log_error("In particular, please do check that your configuration:");
	log_error("a) does not have overlapping pools (especially containing");
	log_error("   %s address).", lease_address);
	log_error("b) there are no duplicate fixed-address or fixed-address6");
	log_error("entries for the %s address.", lease_address);
	log_error("%s", "");
	log_error("Possible reason for this failure: %s", reason);

	log_fatal("%s(%d): Failed to update lease database with DDNS info for "
		  "address %s. Lease database inconsistent. Unable to recover."
		  " Terminating.", file, line, lease_address);
}
#endif

/*
 * utility function to update found lease. It does extra checks
 * that we are indeed updating the right lease. It may happen
 * that user have duplicate fixed-address entries, so we attempt
 * to update wrong lease. See also safe_lease6_update.
 */

static void
safe_lease_update(struct lease *lease,
		  dhcp_ddns_cb_t *oldcb,
		  dhcp_ddns_cb_t *newcb,
		  const char *file, int line)
{
	if (lease == NULL) {
		/* should never get here */
		log_fatal("Impossible condition at %s:%d (called from %s:%d).", 
			  MDL, file, line);
	}

	if ( (lease->ddns_cb == NULL) && (newcb == NULL) ) {
		/*
		 * Trying to clean up pointer that is already null. We
		 * are most likely trying to update wrong lease here.
		 */

		/* 
		 * Previously this error message popped out during
		 * DNS update for fixed leases.  As we no longer
		 * try to update the lease for a fixed (static) lease
		 * this should not be a problem.
		 */
		log_error("%s(%d): Invalid lease update. Tried to "
			  "clear already NULL DDNS control block "
			  "pointer for lease %s.",
			  file, line, piaddr(lease->ip_addr) );

#if defined (DNS_UPDATES_MEMORY_CHECKS)
		update_lease_failed(lease, NULL, oldcb, newcb, file, line);
#endif
		/* 
		 * May not reach this: update_lease_failed calls 
		 * log_fatal.
		 */ 
		return;
	}

	if ( (lease->ddns_cb != NULL) && (lease->ddns_cb != oldcb) ) {
		/* 
		 * There is existing cb structure, but it differs from
		 * what we expected to see there. Most likely we are 
		 * trying to update wrong lease. 
		 */
		log_error("%s(%d): Failed to update internal lease "
			  "structure with DDNS control block. Existing"
			  " ddns_cb structure does not match "
			  "expectations.IPv4=%s, old ddns_cb=%p, tried"
			  "to update to new ddns_cb=%p", file, line,
			  piaddr(lease->ip_addr), oldcb,  newcb);

#if defined (DNS_UPDATES_MEMORY_CHECKS)
		update_lease_failed(lease, NULL, oldcb, newcb, file, line);
#endif
		/* 
		 * May not reach this: update_lease_failed calls 
		 * log_fatal.
		 */ 
		return; 
	}

	/* additional IPv4 specific checks may be added here */

	/* update the lease */
	lease->ddns_cb = newcb;
}

static void
safe_lease6_update(struct iasubopt *lease6,
		   dhcp_ddns_cb_t *oldcb,
		   dhcp_ddns_cb_t *newcb,
		   const char *file, int line)
{
	char addrbuf[MAX_ADDRESS_STRING_LEN];

	if (lease6 == NULL) {
		/* should never get here */
		log_fatal("Impossible condition at %s:%d (called from %s:%d).", 
			  MDL, file, line);
	}

	if ( (lease6->ddns_cb == NULL) && (newcb == NULL) ) {
		inet_ntop(AF_INET6, &lease6->addr, addrbuf, 
			  MAX_ADDRESS_STRING_LEN);
		/*
		 * Trying to clean up pointer that is already null. We
		 * are most likely trying to update wrong lease here.
		 */
		log_error("%s(%d): Failed to update internal lease "
			  "structure. Tried to clear already NULL "
			  "DDNS control block pointer for lease %s.",
			  file, line, addrbuf);

#if defined (DNS_UPDATES_MEMORY_CHECKS)
		update_lease_failed(NULL, lease6, oldcb, newcb, file, line);
#endif

		/* 
		 * May not reach this: update_lease_failed calls 
		 * log_fatal.
		 */ 
		return; 
	}

	if ( (lease6->ddns_cb != NULL) && (lease6->ddns_cb != oldcb) ) {
		/* 
		 * there is existing cb structure, but it differs from
		 * what we expected to see there. Most likely we are 
		 * trying to update wrong lease. 
		 */
		inet_ntop(AF_INET6, &lease6->addr, addrbuf, 
			  MAX_ADDRESS_STRING_LEN);

		log_error("%s(%d): Failed to update internal lease "
			  "structure with DDNS control block. Existing"
			  " ddns_cb structure does not match "
			  "expectations.IPv6=%s, old ddns_cb=%p, tried"
			  "to update to new ddns_cb=%p", file, line,
			  addrbuf, oldcb,  newcb);

#if defined (DNS_UPDATES_MEMORY_CHECKS)
		update_lease_failed(NULL, lease6, oldcb, newcb, file, line);
#endif
		/* 
		 * May not reach this: update_lease_failed calls 
		 * log_fatal.
		 */ 
		return;
	}
	/* additional IPv6 specific checks may be added here */
	
	/* update the lease */
	lease6->ddns_cb = newcb;
}

/*
 * Utility function to update the pointer to the DDNS control block
 * in a lease.
 * SUCCESS - able to update the pointer
 * FAILURE - lease didn't exist or sanity checks failed
 * lease and lease6 may be empty in which case we attempt to find
 * the lease from the ddns_cb information.
 * ddns_cb is the control block to use if a lookup is necessary
 * ddns_cb_set is the pointer to insert into the lease and may be NULL
 * The last two arguments may look odd as they will be the same much of the
 * time, but I need an argument to tell me if I'm setting or clearing in
 * addition to the address information from the cb to look up the lease.
 * using the same value twice allows me more flexibility.
 */

static isc_result_t 
ddns_update_lease_ptr(struct lease    *lease,
		      struct iasubopt *lease6,
		      dhcp_ddns_cb_t  *ddns_cb,
		      dhcp_ddns_cb_t  *ddns_cb_set,
		      const char * file, int line)
{
	char ddns_address[MAX_ADDRESS_STRING_LEN];
	sprintf(ddns_address, "unknown");
	if (ddns_cb == NULL) {
		log_info("%s(%d): No control block for lease update",
			 file, line);
		return (ISC_R_FAILURE);
	}
	else {
		strncpy(ddns_address, piaddr(ddns_cb->address), 
			MAX_ADDRESS_STRING_LEN);
	}
#if defined (DEBUG_DNS_UPDATES)
	log_info("%s(%d): Updating lease_ptr for ddns_cp=%p (addr=%s)",
		 file, line, ddns_cb, ddns_address );
#endif

	/*
	 * If the lease was static (for a fixed address)
	 * we don't need to do any work.
	 */
	if (ddns_cb->flags & DDNS_STATIC_LEASE) {
#if defined (DEBUG_DNS_UPDATES)
		log_info("lease is static, returning");
#endif
		return (ISC_R_SUCCESS);
	}

	/* 
	 * If we are processing an expired or released v6 lease
	 * we don't actually have a lease to update
	 */
	if ((ddns_cb->address.len == 16) &&
	    ((ddns_cb->flags & DDNS_ACTIVE_LEASE) == 0)) {
		return (ISC_R_SUCCESS);
	}

	if (lease != NULL) {
		safe_lease_update(lease, ddns_cb, ddns_cb_set, 
				  file, line);
	} else if (lease6 != NULL) {
		safe_lease6_update(lease6, ddns_cb, ddns_cb_set, 
				  file, line);
	} else if (ddns_cb->address.len == 4) {
		struct lease *find_lease = NULL;
		if (find_lease_by_ip_addr(&find_lease,
					  ddns_cb->address, MDL) != 0) {
#if defined (DEBUG_DNS_UPDATES)
			log_info("%s(%d): find_lease_by_ip_addr(%s) successful:"
				 "lease=%p", file, line, ddns_address, 
				 find_lease);
#endif
		  
			safe_lease_update(find_lease, ddns_cb,
					  ddns_cb_set, file, line);
			lease_dereference(&find_lease, MDL);
		}
		else {
			log_error("%s(%d): ddns_update_lease_ptr failed. "
				  "Lease for %s not found.",
				  file, line, piaddr(ddns_cb->address));

#if defined (DNS_UPDATES_MEMORY_CHECKS)
			update_lease_failed(NULL, NULL, ddns_cb, ddns_cb_set,
					    file, line);
#endif
			/*
			 * may not reach this. update_lease_failed 
			 * calls log_fatal. 
			 */
			return(ISC_R_FAILURE); 
						  
		}
	} else if (ddns_cb->address.len == 16) {
		struct iasubopt *find_lease6 = NULL;
		struct ipv6_pool *pool = NULL;
		struct in6_addr addr;
		char addrbuf[MAX_ADDRESS_STRING_LEN];

		memcpy(&addr, &ddns_cb->address.iabuf, 16);
		if ((find_ipv6_pool(&pool, D6O_IA_TA, &addr) != 
		     ISC_R_SUCCESS) &&
		    (find_ipv6_pool(&pool, D6O_IA_NA, &addr) != 
		     ISC_R_SUCCESS)) {
			inet_ntop(AF_INET6, &addr, addrbuf,
				  MAX_ADDRESS_STRING_LEN);
			log_error("%s(%d): Pool for lease %s not found.",
				  file, line, addrbuf);
#if defined (DNS_UPDATES_MEMORY_CHECKS)
			update_lease_failed(NULL, NULL, ddns_cb, ddns_cb_set,
					    file, line);
#endif
			/*
			 * never reached. update_lease_failed 
			 * calls log_fatal. 
			 */
			return(ISC_R_FAILURE);
		}

		if (iasubopt_hash_lookup(&find_lease6, pool->leases,
					 &addr, 16, MDL)) {
			find_lease6->ddns_cb = ddns_cb_set;
			iasubopt_dereference(&find_lease6, MDL);
		} else {
			inet_ntop(AF_INET6, &addr, addrbuf,
				  MAX_ADDRESS_STRING_LEN);
			log_error("%s(%d): Lease %s not found within pool.",
				  file, line, addrbuf);
#if defined (DNS_UPDATES_MEMORY_CHECKS)
			update_lease_failed(NULL, NULL, ddns_cb, ddns_cb_set,
					    file, line);
#endif
			/*
			 * never reached. update_lease_failed 
			 * calls log_fatal. 
			 */
			return(ISC_R_FAILURE);
		}
		ipv6_pool_dereference(&pool, MDL);
	} else {
		/* shouldn't get here */
		log_fatal("Impossible condition at %s:%d, called from %s:%d.", 
			  MDL, file, line);
	}

	return(ISC_R_SUCCESS);
}		

static void
ddns_ptr_add(dhcp_ddns_cb_t *ddns_cb,
	     isc_result_t    eresult)
{
	if (eresult == ISC_R_SUCCESS) {
		log_info("Added reverse map from %.*s to %.*s",
			 (int)ddns_cb->rev_name.len,
			 (const char *)ddns_cb->rev_name.data,
			 (int)ddns_cb->fwd_name.len,
			 (const char *)ddns_cb->fwd_name.data);

		ddns_update_lease_text(ddns_cb, NULL);
	} else {
		log_error("Unable to add reverse map from %.*s to %.*s: %s",
			  (int)ddns_cb->rev_name.len,
			  (const char *)ddns_cb->rev_name.data,
			  (int)ddns_cb->fwd_name.len,
			  (const char *)ddns_cb->fwd_name.data,
			  isc_result_totext (eresult));
	}

	ddns_update_lease_ptr(NULL, NULL, ddns_cb, NULL, MDL);
	ddns_cb_free(ddns_cb, MDL);
	/*
	 * A single DDNS operation may require several calls depending on
	 * the current state as the prerequisites for the first message
	 * may not succeed requiring a second operation and potentially
	 * a ptr operation after that.  The commit_leases operation is
	 * invoked at the end of this set of operations in order to require
	 * a single write for all of the changes.  We call commit_leases
	 * here rather than immediately after the call to update the lease
	 * text in order to save any previously written data.
	 */
	commit_leases();
	return;
}

/*
 * action routine when trying to remove a pointer
 * this will be called after the ddns queries have completed
 * if we succeeded in removing the pointer we go to the next step (if any)
 * if not we cleanup and leave.
 */

static void
ddns_ptr_remove(dhcp_ddns_cb_t *ddns_cb,
		isc_result_t    eresult)
{
	isc_result_t result = eresult;

	switch(eresult) {
	case ISC_R_SUCCESS:
		log_info("Removed reverse map on %.*s",
			 (int)ddns_cb->rev_name.len,
			 (const char *)ddns_cb->rev_name.data);
		/* fall through */
	case DNS_R_NXRRSET:
	case DNS_R_NXDOMAIN:
		/* No entry is the same as success.
		 * Remove the information from the lease and
		 * continue with any next step */
		ddns_update_lease_text(ddns_cb, NULL);

		/* trigger any add operation */
		result = ISC_R_SUCCESS;
#if defined (DEBUG_DNS_UPDATES)
		log_info("DDNS: removed map or no reverse map to remove %.*s",
			 (int)ddns_cb->rev_name.len,
			 (const char *)ddns_cb->rev_name.data);
#endif
		break;

	default:
		log_error("Can't remove reverse map on %.*s: %s",
			  (int)ddns_cb->rev_name.len,
			  (const char *)ddns_cb->rev_name.data,
			  isc_result_totext (eresult));
		break;
	}

	ddns_update_lease_ptr(NULL, NULL, ddns_cb, NULL, MDL);
	ddns_fwd_srv_connector(NULL, NULL, NULL, ddns_cb->next_op, result);
	ddns_cb_free(ddns_cb, MDL);
	return;
}


/*
 * If the first query succeeds, the updater can conclude that it
 * has added a new name whose only RRs are the A and DHCID RR records.
 * The A RR update is now complete (and a client updater is finished,
 * while a server might proceed to perform a PTR RR update).
 *   -- "Interaction between DHCP and DNS"
 *
 * If the second query succeeds, the updater can conclude that the current
 * client was the last client associated with the domain name, and that
 * the name now contains the updated A RR. The A RR update is now
 * complete (and a client updater is finished, while a server would
 * then proceed to perform a PTR RR update).
 *   -- "Interaction between DHCP and DNS"
 *
 * If the second query fails with NXRRSET, the updater must conclude
 * that the client's desired name is in use by another host.  At this
 * juncture, the updater can decide (based on some administrative
 * configuration outside of the scope of this document) whether to let
 * the existing owner of the name keep that name, and to (possibly)
 * perform some name disambiguation operation on behalf of the current
 * client, or to replace the RRs on the name with RRs that represent
 * the current client. If the configured policy allows replacement of
 * existing records, the updater submits a query that deletes the
 * existing A RR and the existing DHCID RR, adding A and DHCID RRs that
 * represent the IP address and client-identity of the new client.
 *   -- "Interaction between DHCP and DNS"
 */

static void
ddns_fwd_srv_add2(dhcp_ddns_cb_t *ddns_cb,
		  isc_result_t    eresult)
{
	isc_result_t result;
	const char *logstr = NULL;
	char ddns_address[MAX_ADDRESS_STRING_LEN];

	/* Construct a printable form of the address for logging */
	strcpy(ddns_address, piaddr(ddns_cb->address));

	switch(eresult) {
	case ISC_R_SUCCESS:
		log_info("Added new forward map from %.*s to %s",
			 (int)ddns_cb->fwd_name.len,
			 (const char *)ddns_cb->fwd_name.data,
			 ddns_address);

		ddns_update_lease_text(ddns_cb, NULL);

		if ((ddns_cb->flags & DDNS_UPDATE_PTR) != 0) {
			/* if we have zone information get rid of it */
			if (ddns_cb->zone != NULL) {
				ddns_cb_forget_zone(ddns_cb);
			}

			ddns_cb->state = DDNS_STATE_ADD_PTR;
			ddns_cb->cur_func = ddns_ptr_add;
			
			result = ddns_modify_ptr(ddns_cb, MDL);
			if (result == ISC_R_SUCCESS) {
				return;
			}
		}
		break;

	case DNS_R_YXRRSET:
	case DNS_R_YXDOMAIN:
		logstr = "DHCID mismatch, belongs to another client.";
		break;

	case DNS_R_NXRRSET:
	case DNS_R_NXDOMAIN:
		logstr = "Has an address record but no DHCID, not mine.";
		break;

	default:
		logstr = isc_result_totext(eresult);
		break;
	}

	if (logstr != NULL) {
		log_error("Forward map from %.*s to %s FAILED: %s",
			  (int)ddns_cb->fwd_name.len,
			  (const char *)ddns_cb->fwd_name.data,
			  ddns_address, logstr);
	}

	ddns_update_lease_ptr(NULL, NULL, ddns_cb, NULL, MDL);
	ddns_cb_free(ddns_cb, MDL);
	/*
	 * A single DDNS operation may require several calls depending on
	 * the current state as the prerequisites for the first message
	 * may not succeed requiring a second operation and potentially
	 * a ptr operation after that.  The commit_leases operation is
	 * invoked at the end of this set of operations in order to require
	 * a single write for all of the changes.  We call commit_leases
	 * here rather than immediately after the call to update the lease
	 * text in order to save any previously written data.
	 */
	commit_leases();
	return;
}

static void
ddns_fwd_srv_add1(dhcp_ddns_cb_t *ddns_cb,
		  isc_result_t    eresult)
{
	isc_result_t result;
	char ddns_address[MAX_ADDRESS_STRING_LEN];

	/* Construct a printable form of the address for logging */
	strcpy(ddns_address, piaddr(ddns_cb->address));

	switch(eresult) {
	case ISC_R_SUCCESS:
		log_info ("Added new forward map from %.*s to %s",
			  (int)ddns_cb->fwd_name.len,
			  (const char *)ddns_cb->fwd_name.data,
			  ddns_address);

		ddns_update_lease_text(ddns_cb, NULL);

		if ((ddns_cb->flags & DDNS_UPDATE_PTR) != 0) {
			/* if we have zone information get rid of it */
			if (ddns_cb->zone != NULL) {
				ddns_cb_forget_zone(ddns_cb);
			}

			ddns_cb->state = DDNS_STATE_ADD_PTR;
			ddns_cb->cur_func = ddns_ptr_add;
			
			result = ddns_modify_ptr(ddns_cb, MDL);
			if (result == ISC_R_SUCCESS) {
				return;
			}
		}
		break;

	case DNS_R_YXDOMAIN:
		/* we can reuse the zone information */
		ddns_cb->state = DDNS_STATE_ADD_FW_YXDHCID;
		ddns_cb->cur_func = ddns_fwd_srv_add2;
			
		result = ddns_modify_fwd(ddns_cb, MDL);
		if (result == ISC_R_SUCCESS) {
			return;
		}
		break;

	default:
		log_error ("Unable to add forward map from %.*s to %s: %s",
			   (int)ddns_cb->fwd_name.len,
			   (const char *)ddns_cb->fwd_name.data,
			   ddns_address,
			   isc_result_totext (eresult));
		break;
	}

	ddns_update_lease_ptr(NULL, NULL, ddns_cb, NULL, MDL);
	ddns_cb_free(ddns_cb, MDL);
	/*
	 * A single DDNS operation may require several calls depending on
	 * the current state as the prerequisites for the first message
	 * may not succeed requiring a second operation and potentially
	 * a ptr operation after that.  The commit_leases operation is
	 * invoked at the end of this set of operations in order to require
	 * a single write for all of the changes.  We call commit_leases
	 * here rather than immediately after the call to update the lease
	 * text in order to save any previously written data.
	 */
	commit_leases();
	return;
}

static void
ddns_fwd_srv_connector(struct lease          *lease,
		       struct iasubopt       *lease6,
		       struct binding_scope **inscope,
		       dhcp_ddns_cb_t        *ddns_cb,
		       isc_result_t           eresult)
{
	isc_result_t result = ISC_R_FAILURE;

	if (ddns_cb == NULL) {
		/* nothing to do */
		return;
	}

	if (eresult == ISC_R_SUCCESS) {
		/*
		 * If we have updates dispatch as appropriate,
		 * if not do FQDN binding if desired.
		 */

		if (ddns_cb->flags & DDNS_UPDATE_ADDR) {
			ddns_cb->state    = DDNS_STATE_ADD_FW_NXDOMAIN;
			ddns_cb->cur_func = ddns_fwd_srv_add1;
			result = ddns_modify_fwd(ddns_cb, MDL);
		} else if ((ddns_cb->flags & DDNS_UPDATE_PTR) &&
			 (ddns_cb->rev_name.len != 0)) {
			ddns_cb->state    = DDNS_STATE_ADD_PTR;
			ddns_cb->cur_func = ddns_ptr_add;
			result = ddns_modify_ptr(ddns_cb, MDL);
		} else {
			ddns_update_lease_text(ddns_cb, inscope);
		}
	}

	if (result == ISC_R_SUCCESS) {
		ddns_update_lease_ptr(lease, lease6, ddns_cb, ddns_cb, MDL);
	} else {
		ddns_cb_free(ddns_cb, MDL);
	}

	return;
}

/*
 * If the first query fails, the updater MUST NOT delete the DNS name.  It
 * may be that the host whose lease on the server has expired has moved
 * to another network and obtained a lease from a different server,
 * which has caused the client's A RR to be replaced. It may also be
 * that some other client has been configured with a name that matches
 * the name of the DHCP client, and the policy was that the last client
 * to specify the name would get the name.  In this case, the DHCID RR
 * will no longer match the updater's notion of the client-identity of
 * the host pointed to by the DNS name.
 *   -- "Interaction between DHCP and DNS"
 */

static void
ddns_fwd_srv_rem2(dhcp_ddns_cb_t *ddns_cb,
		  isc_result_t    eresult)
{
	if (eresult == ISC_R_SUCCESS) {
		ddns_update_lease_text(ddns_cb, NULL);

		/* Do the next operation */
		if ((ddns_cb->flags & DDNS_UPDATE_PTR) != 0) {
			/* if we have zone information get rid of it */
			if (ddns_cb->zone != NULL) {
				ddns_cb_forget_zone(ddns_cb);
			}

			ddns_cb->state = DDNS_STATE_REM_PTR;
			ddns_cb->cur_func = ddns_ptr_remove;
			
			eresult = ddns_modify_ptr(ddns_cb, MDL);
			if (eresult == ISC_R_SUCCESS) {
				return;
			}
		}
	}

	ddns_update_lease_ptr(NULL, NULL, ddns_cb, NULL, MDL);
	ddns_fwd_srv_connector(NULL, NULL, NULL, ddns_cb->next_op, eresult);
	ddns_cb_free(ddns_cb, MDL);
	return;
}


/*
 * First action routine when trying to remove a fwd
 * this will be called after the ddns queries have completed
 * if we succeeded in removing the fwd we go to the next step (if any)
 * if not we cleanup and leave.
 */

static void
ddns_fwd_srv_rem1(dhcp_ddns_cb_t *ddns_cb,
		  isc_result_t    eresult)
{
	isc_result_t result = eresult;
	char ddns_address[MAX_ADDRESS_STRING_LEN];

	switch(eresult) {
	case ISC_R_SUCCESS:
		/* Construct a printable form of the address for logging */
		strcpy(ddns_address, piaddr(ddns_cb->address));
		log_info("Removed forward map from %.*s to %s",
			 (int)ddns_cb->fwd_name.len, 
			 (const char*)ddns_cb->fwd_name.data,
			 ddns_address);

		/* Do the second step of the FWD removal */
		ddns_cb->state    = DDNS_STATE_REM_FW_NXRR;
		ddns_cb->cur_func = ddns_fwd_srv_rem2;
		result = ddns_modify_fwd(ddns_cb, MDL);
		if (result == ISC_R_SUCCESS) {
			return;
		}
		break;

	case DNS_R_NXRRSET:
	case DNS_R_NXDOMAIN:
		ddns_update_lease_text(ddns_cb, NULL);

#if defined (DEBUG_DNS_UPDATES)
		log_info("DDNS: no forward map to remove. %p", ddns_cb);
#endif

		/* Do the next operation */
		if ((ddns_cb->flags & DDNS_UPDATE_PTR) != 0) {
			/* if we have zone information get rid of it */
			if (ddns_cb->zone != NULL) {
				ddns_cb_forget_zone(ddns_cb);
			}

			ddns_cb->state    = DDNS_STATE_REM_PTR;
			ddns_cb->cur_func = ddns_ptr_remove;
			
			result = ddns_modify_ptr(ddns_cb, MDL);
			if (result == ISC_R_SUCCESS) {
				return;
			}
		}
		else {
			/* Trigger the add operation */
			eresult = ISC_R_SUCCESS;
		}
		break;
			
	default:
		break;
	}

	ddns_update_lease_ptr(NULL, NULL, ddns_cb, NULL, MDL);
	ddns_fwd_srv_connector(NULL, NULL, NULL, ddns_cb->next_op, eresult);
	ddns_cb_free(ddns_cb, MDL);
}

/*%<
 * Remove relevant entries from DNS.
 *
 * \li lease  - lease to start with if this is for v4
 * 
 * \li lease6 - lease to start with if this is for v6
 * 
 * \li add_ddns_cb - control block for additional DDNS work.  This
 *     is used when the code is going to add a DDNS entry after removing
 *     the current entry.
 * 
 * \li active - indication about the status of the lease. It is
 *     ISC_TRUE if the lease is still active, and FALSE if the lease
 *     is inactive.  This is used to indicate if the lease is inactive or going
 *     to inactive so we can avoid trying to update the lease with cb pointers
 *     and text information if it isn't useful.
 * 
 * Returns
 * \li #ISC_R_FAILURE - badness occurred and we weren't able to do what was wanted
 * \li #ISC_R_SUCCESS - we were able to do stuff but it's in progress
 *
 * in both cases any additional block has been passed on to it's handler
 */

isc_result_t
ddns_removals(struct lease    *lease,
	      struct iasubopt *lease6,
	      dhcp_ddns_cb_t  *add_ddns_cb,
	      isc_boolean_t    active)
{
	isc_result_t rcode, execute_add = ISC_R_FAILURE;
	struct binding_scope **scope = NULL;
	isc_result_t result = ISC_R_FAILURE;
	dhcp_ddns_cb_t        *ddns_cb = NULL;
	struct data_string     leaseid;

	/*
	 * See if we need to cancel an outstanding request.  Mostly this is
	 * used to handle the case where this routine is called twice for
	 * the same release or abandon event.
	 * 
	 * When called from the dns code as part of an update request
	 * (add_ddns_cb != NULL) any outstanding requests will have already
	 * been cancelled.
	 * 
	 * If the new request is just a removal and we have an outstanding
	 * request we have several options:
	 *
	 * - we are doing an update or we are doing a removal and the active
	 * flag has changed from TRUE to FALSE.  In these cases we  need to
	 * cancel the old request and start the new one.
	 *
	 * - other wise we are doing a removal with the active flag unchanged.
	 * In this case we can let the current removal continue and do not need
	 * to start a new one.  If the old request included an update to be
	 * done after the removal we need to kill the update part of the
	 * request.
	 */
	
	if (add_ddns_cb == NULL) {
		if ((lease != NULL) && (lease->ddns_cb != NULL)) {
			ddns_cb = lease->ddns_cb;

			/*
			 * Is the old request an update or did the 
			 * the active flag change?
			 */
			if (((ddns_cb->state == DDNS_STATE_ADD_PTR) ||
			     (ddns_cb->state == DDNS_STATE_ADD_FW_NXDOMAIN) ||
			     (ddns_cb->state == DDNS_STATE_ADD_FW_YXDHCID)) ||
			    ((active == ISC_FALSE) &&
			     ((ddns_cb->flags & DDNS_ACTIVE_LEASE) != 0))) {
				/* Cancel the current request */
				ddns_cancel(lease->ddns_cb, MDL);
				lease->ddns_cb = NULL;
			} else {
				/* Remvoval, check and remove updates */
				if (ddns_cb->next_op != NULL) {
					ddns_cb_free(ddns_cb->next_op, MDL);
					ddns_cb->next_op = NULL;
				}
#if defined (DEBUG_DNS_UPDATES)
				log_info("DDNS %s(%d): removal already in "
					 "progress new ddns_cb=%p",
					 MDL, ddns_cb);
#endif
				return (ISC_R_SUCCESS);
			}
		} else if ((lease6 != NULL) && (lease6->ddns_cb != NULL)) {
			ddns_cb = lease6->ddns_cb;

			/*
			 * Is the old request an update or did the 
			 * the active flag change?
			 */
			if (((ddns_cb->state == DDNS_STATE_ADD_PTR) ||
			     (ddns_cb->state == DDNS_STATE_ADD_FW_NXDOMAIN) ||
			     (ddns_cb->state == DDNS_STATE_ADD_FW_YXDHCID)) ||
			    ((active == ISC_FALSE) &&
			     ((ddns_cb->flags & DDNS_ACTIVE_LEASE) != 0))) {
				/* Cancel the current request */
				ddns_cancel(lease6->ddns_cb, MDL);
				lease6->ddns_cb = NULL;
			} else {
				/* Remvoval, check and remove updates */
				if (ddns_cb->next_op != NULL) {
					ddns_cb_free(ddns_cb->next_op, MDL);
					ddns_cb->next_op = NULL;
				}
#if defined (DEBUG_DNS_UPDATES)
				log_info("DDNS %s(%d): removal already in "
					 "progress new ddns_cb=%p",
					 MDL, ddns_cb);
#endif
				return (ISC_R_SUCCESS);
			}
		}
		ddns_cb = NULL;
	}

	/* allocate our control block */
	ddns_cb = ddns_cb_alloc(MDL);
	if (ddns_cb == NULL) {
		goto cleanup;
	}

	/*
	 * For v4 we flag static leases so we don't try
	 * and manipulate the lease later.  For v6 we don't
	 * get static leases and don't need to flag them.
	 */
	if (lease != NULL) {
		scope = &(lease->scope);
		ddns_cb->address = lease->ip_addr;
		if (lease->flags & STATIC_LEASE)
			ddns_cb->flags |= DDNS_STATIC_LEASE;
	} else if (lease6 != NULL) {
		scope = &(lease6->scope);
		memcpy(&ddns_cb->address.iabuf, lease6->addr.s6_addr, 16);
		ddns_cb->address.len = 16;
	} else
		goto cleanup;

	/*
	 * Set the flag bit if the lease is active, that is it isn't
	 * expired or released.  This is used to determine if we need
	 * to update the scope information for both v4 and v6 and
	 * the lease information for v6 when the response
	 * from the DNS code is processed.
	 */
	if (active == ISC_TRUE) {
		ddns_cb->flags |= DDNS_ACTIVE_LEASE;
	}

	/* No scope implies that DDNS has not been performed for this lease. */
	if (*scope == NULL)
		goto cleanup;

	if ((ddns_update_style != DDNS_UPDATE_STYLE_STANDARD) &&
	    (ddns_update_style != DDNS_UPDATE_STYLE_INTERIM))
		goto cleanup;

	/* Assume that we are removing both records */
	ddns_cb->flags |= DDNS_UPDATE_ADDR | DDNS_UPDATE_PTR;

	/* and that we want to do the add call */
	execute_add = ISC_R_SUCCESS;

	/*
	 * Look up stored names.
	 */

	/*
	 * Find the fwd name and copy it to the control block.  If we don't
	 * have it we can't delete the fwd record but we can still try to
	 * remove the ptr record and cleanup the lease information if the
	 * client did the fwd update.
	 */
	if (!find_bound_string(&ddns_cb->fwd_name, *scope, "ddns-fwd-name")) {
		/* don't try and delete the A, or do the add */
		ddns_cb->flags &= ~DDNS_UPDATE_ADDR;
		execute_add = ISC_R_FAILURE;

		/* Check if client did update */
		if (find_bound_string(&ddns_cb->fwd_name, *scope,
				      "ddns-client-fqdn")) {
			ddns_cb->flags |= DDNS_CLIENT_DID_UPDATE;
		}
	}

	/*
	 * Find the txt or dhcid tag and copy it to the control block.  If we don't
	 * have one this isn't an interim or standard record so we can't delete
	 * the A record using this mechanism but we can delete the ptr record.
	 * In this case we will attempt to do any requested next step.
	 */
	memset(&leaseid, 0, sizeof(leaseid));
	if (find_bound_string (&leaseid, *scope, ddns_standard_tag)) {
		/* We have a standard tag */
		ddns_cb->lease_tag = ddns_standard_tag;
		ddns_cb->dhcid_class = dns_rdatatype_dhcid;
		data_string_copy(&ddns_cb->dhcid, &leaseid, MDL);
		data_string_forget(&leaseid, MDL);
	} else 	if (find_bound_string (&leaseid, *scope, ddns_interim_tag)) {
		/* we have an interim tag */
		ddns_cb->lease_tag = ddns_interim_tag;
		ddns_cb->dhcid_class = dns_rdatatype_txt;
		if (dhcid_fromlease(&ddns_cb->dhcid, &leaseid) != 
		    ISC_R_SUCCESS) {
			/* We couldn't convert the dhcid from the lease
			 * version to the dns version.  We can't delete
			 * the A record but can continue to the ptr
			 */
			ddns_cb->flags &= ~DDNS_UPDATE_ADDR;
		}
		data_string_forget(&leaseid, MDL);
	} else {
		ddns_cb->flags &= ~DDNS_UPDATE_ADDR;
	}		

	/*
	 * Find the rev name and copy it to the control block.  If we don't
	 * have it we can't get rid of it but we can try to remove the fwd
	 * pointer if desired.
	 */
	if (!find_bound_string(&ddns_cb->rev_name, *scope, "ddns-rev-name")) {
		ddns_cb->flags &= ~DDNS_UPDATE_PTR;
	}
	
	/*
	 * If we have a second control block for doing an add
	 * after the remove finished attach it to our control block.
	 */
	ddns_cb->next_op = add_ddns_cb;

	/*
	 * Now that we've collected the information we can try to process it.
	 * If necessary we call an appropriate routine to send a message and
	 * provide it with an action routine to run on the control block given
	 * the results of the message.  We have three entry points from here,
	 * one for removing the A record, the next for removing the PTR and
	 * the third for doing any requested add.
	 */
	if ((ddns_cb->flags & DDNS_UPDATE_ADDR) != 0) {
		if (ddns_cb->fwd_name.len != 0) {
			ddns_cb->state    = DDNS_STATE_REM_FW_YXDHCID;
			ddns_cb->cur_func = ddns_fwd_srv_rem1;

			rcode = ddns_modify_fwd(ddns_cb, MDL);
			if (rcode == ISC_R_SUCCESS) {
				ddns_update_lease_ptr(lease, lease6, ddns_cb,
						      ddns_cb, MDL);
				return (ISC_R_SUCCESS);
			}

			/*
			 * We weren't able to process the request tag the
			 * add so we won't execute it.
			 */
			execute_add = ISC_R_FAILURE;
			goto cleanup;
		}
		else {
			/*remove info from scope */
			unset(*scope, "ddns-fwd-name");
			unset(*scope, ddns_cb->lease_tag);
		}
	}

	if ((ddns_cb->flags & DDNS_UPDATE_PTR) != 0) {
		ddns_cb->state      = DDNS_STATE_REM_PTR;
		ddns_cb->cur_func   = ddns_ptr_remove;

		/*
		 * if execute add isn't success remove the control block so
		 * it won't be processed when the remove completes.  We
		 * also arrange to clean it up and get rid of it.
		 */
		if (execute_add != ISC_R_SUCCESS) {
		   	ddns_cb->next_op = NULL;
			ddns_fwd_srv_connector(lease, lease6, scope, 
					       add_ddns_cb, execute_add);
			add_ddns_cb = NULL;
		}
		else {
			result = ISC_R_SUCCESS;
		}

		rcode = ddns_modify_ptr(ddns_cb, MDL);
		if (rcode == ISC_R_SUCCESS) {
			ddns_update_lease_ptr(lease, lease6, ddns_cb, ddns_cb,
					      MDL);
			return (result);
		}

		/* We weren't able to process the request tag the
		 * add so we won't execute it */
		execute_add = ISC_R_FAILURE;
		goto cleanup;
	}

 cleanup:
	/*
	 * We've gotten here because we didn't need to send a message or
	 * we failed when trying to do so.  We send the additional cb
	 * off to handle sending and/or cleanup and cleanup anything
	 * we allocated here.
	 */
	ddns_fwd_srv_connector(lease, lease6, scope, add_ddns_cb, execute_add);
	if (ddns_cb != NULL) 
		ddns_cb_free(ddns_cb, MDL);

	return (result);
}

#endif /* NSUPDATE */
