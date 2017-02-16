/*	$NetBSD: class.c,v 1.1.1.3 2014/07/12 11:58:04 spz Exp $	*/
/* class.c

   Handling for client classes. */

/*
 * Copyright (c) 2009,2012-2014 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 2004,2007 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1998-2003 by Internet Software Consortium
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
__RCSID("$NetBSD: class.c,v 1.1.1.3 2014/07/12 11:58:04 spz Exp $");

#include "dhcpd.h"

struct collection default_collection = {
	(struct collection *)0,
	"default",
	(struct class *)0,
};

struct collection *collections = &default_collection;
struct executable_statement *default_classification_rules;

int have_billing_classes;

/* Build the default classification rule tree. */

void classification_setup ()
{
	/* eval ... */
	default_classification_rules = (struct executable_statement *)0;
	if (!executable_statement_allocate (&default_classification_rules,
					    MDL))
		log_fatal ("Can't allocate check of default collection");
	default_classification_rules -> op = eval_statement;

	/* check-collection "default" */
	if (!expression_allocate (&default_classification_rules -> data.eval,
				  MDL))
		log_fatal ("Can't allocate default check expression");
	default_classification_rules -> data.eval -> op = expr_check;
	default_classification_rules -> data.eval -> data.check =
		&default_collection;
}

void classify_client (packet)
	struct packet *packet;
{
	execute_statements (NULL, packet, NULL, NULL, packet->options, NULL,
			    &global_scope, default_classification_rules, NULL);
}

int check_collection (packet, lease, collection)
	struct packet *packet;
	struct lease *lease;
	struct collection *collection;
{
	struct class *class, *nc;
	struct data_string data;
	int matched = 0;
	int status;
	int ignorep;
	int classfound;

	for (class = collection -> classes; class; class = class -> nic) {
#if defined (DEBUG_CLASS_MATCHING)
		log_info ("checking against class %s...", class -> name);
#endif
		memset (&data, 0, sizeof data);

		/* If there is a "match if" expression, check it.   If
		   we get a match, and there's no subclass expression,
		   it's a match.   If we get a match and there is a subclass
		   expression, then we check the submatch.   If it's not a
		   match, that's final - we don't check the submatch. */

		if (class -> expr) {
			status = (evaluate_boolean_expression_result
				  (&ignorep, packet, lease,
				   (struct client_state *)0,
				   packet -> options, (struct option_state *)0,
				   lease ? &lease -> scope : &global_scope,
				   class -> expr));
			if (status) {
				if (!class -> submatch) {
					matched = 1;
#if defined (DEBUG_CLASS_MATCHING)
					log_info ("matches class.");
#endif
					classify (packet, class);
					continue;
				}
			} else
				continue;
		}

		/* Check to see if the client matches an existing subclass.
		   If it doesn't, and this is a spawning class, spawn a new
		   subclass and put the client in it. */
		if (class -> submatch) {
			status = (evaluate_data_expression
				  (&data, packet, lease,
				   (struct client_state *)0,
				   packet -> options, (struct option_state *)0,
				   lease ? &lease -> scope : &global_scope,
				   class -> submatch, MDL));
			if (status && data.len) {
				nc = (struct class *)0;
				classfound = class_hash_lookup (&nc, class -> hash,
					(const char *)data.data, data.len, MDL);

#ifdef LDAP_CONFIGURATION
				if (!classfound && find_subclass_in_ldap (class, &nc, &data))
					classfound = 1;
#endif

				if (classfound) {
#if defined (DEBUG_CLASS_MATCHING)
					log_info ("matches subclass %s.",
					      print_hex_1 (data.len,
							   data.data, 60));
#endif
					data_string_forget (&data, MDL);
					classify (packet, nc);
					matched = 1;
					class_dereference (&nc, MDL);
					continue;
				}
				if (!class -> spawning) {
					data_string_forget (&data, MDL);
					continue;
				}
				/* XXX Write out the spawned class? */
#if defined (DEBUG_CLASS_MATCHING)
				log_info ("spawning subclass %s.",
				      print_hex_1 (data.len, data.data, 60));
#endif
				status = class_allocate (&nc, MDL);
				group_reference (&nc -> group,
						 class -> group, MDL);
				class_reference (&nc -> superclass,
						 class, MDL);
				nc -> lease_limit = class -> lease_limit;
				nc -> dirty = 1;
				if (nc -> lease_limit) {
					nc -> billed_leases =
						(dmalloc
						 (nc -> lease_limit *
						  sizeof (struct lease *),
						  MDL));
					if (!nc -> billed_leases) {
						log_error ("no memory for%s",
							   " billing");
						data_string_forget
							(&nc -> hash_string,
							 MDL);
						class_dereference (&nc, MDL);
						data_string_forget (&data,
								    MDL);
						continue;
					}
					memset (nc -> billed_leases, 0,
						(nc -> lease_limit *
						 sizeof (struct lease *)));
				}
				data_string_copy (&nc -> hash_string, &data,
						  MDL);
				data_string_forget (&data, MDL);
				if (!class -> hash)
				    class_new_hash(&class->hash,
						   SCLASS_HASH_SIZE, MDL);
				class_hash_add (class -> hash,
						(const char *)
						nc -> hash_string.data,
						nc -> hash_string.len,
						nc, MDL);
				classify (packet, nc);
				class_dereference (&nc, MDL);
			}
		}
	}
	return matched;
}

void classify (packet, class)
	struct packet *packet;
	struct class *class;
{
	if (packet -> class_count < PACKET_MAX_CLASSES)
		class_reference (&packet -> classes [packet -> class_count++],
				 class, MDL);
	else
		log_error ("too many classes match %s",
		      print_hw_addr (packet -> raw -> htype,
				     packet -> raw -> hlen,
				     packet -> raw -> chaddr));
}


isc_result_t unlink_class(struct class **class) {
	struct collection *lp;
	struct class *cp, *pp;

	for (lp = collections; lp; lp = lp -> next) {
		for (pp = 0, cp = lp -> classes; cp; pp = cp, cp = cp -> nic)
			if (cp == *class) {
				if (pp == 0) {
					lp->classes = cp->nic;
				} else {
					pp->nic = cp->nic;
				}
				cp->nic = 0;
				class_dereference(class, MDL);

				return ISC_R_SUCCESS;
			}
	}
	return ISC_R_NOTFOUND;
}

	
isc_result_t find_class (struct class **class, const char *name,
			 const char *file, int line)
{
	struct collection *lp;
	struct class *cp;

	for (lp = collections; lp; lp = lp -> next) {
		for (cp = lp -> classes; cp; cp = cp -> nic)
			if (cp -> name && !strcmp (name, cp -> name)) {
				return class_reference (class, cp, file, line);
			}
	}
	return ISC_R_NOTFOUND;
}

int unbill_class (lease, class)
	struct lease *lease;
	struct class *class;
{
	int i;

	for (i = 0; i < class -> lease_limit; i++)
		if (class -> billed_leases [i] == lease)
			break;
	if (i == class -> lease_limit) {
		log_error ("lease %s unbilled with no billing arrangement.",
		      piaddr (lease -> ip_addr));
		return 0;
	}
	class_dereference (&lease -> billing_class, MDL);
	lease_dereference (&class -> billed_leases [i], MDL);
	class -> leases_consumed--;
	return 1;
}

int bill_class (lease, class)
	struct lease *lease;
	struct class *class;
{
	int i;

	if (lease -> billing_class) {
		log_error ("lease billed with existing billing arrangement.");
		unbill_class (lease, lease -> billing_class);
	}

	if (class -> leases_consumed == class -> lease_limit)
		return 0;

	for (i = 0; i < class -> lease_limit; i++)
		if (!class -> billed_leases [i])
			break;

	if (i == class -> lease_limit) {
		log_error ("class billing consumption disagrees with leases.");
		return 0;
	}

	lease_reference (&class -> billed_leases [i], lease, MDL);
	class_reference (&lease -> billing_class, class, MDL);
	class -> leases_consumed++;
	return 1;
}
