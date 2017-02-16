/*	$NetBSD: salloc.c,v 1.1.1.3 2014/07/12 11:58:16 spz Exp $	*/
/* salloc.c

   Memory allocation for the DHCP server... */

/*
 * Copyright (c) 2009,2012,2014 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 2004-2007 by Internet Systems Consortium, Inc. ("ISC")
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
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: salloc.c,v 1.1.1.3 2014/07/12 11:58:16 spz Exp $");

#include "dhcpd.h"
#include <omapip/omapip_p.h>

#if defined (COMPACT_LEASES)
struct lease *free_leases;

#if defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
struct lease *lease_hunks;

void relinquish_lease_hunks ()
{
	struct lease *c, *n, **p;
	int i;

	/* Account for all the leases on the free list. */
	for (n = lease_hunks; n; n = n->next) {
	    for (i = 1; i < n->starts + 1; i++) {
		p = &free_leases;
		for (c = free_leases; c; c = c->next) {
		    if (c == &n[i]) {
			*p = c->next;
			n->ends++;
			break;
		    }
		    p = &c->next;
		}
		if (!c) {
		    log_info("lease %s refcnt %d",
			     piaddr (n[i].ip_addr), n[i].refcnt);
#if defined (DEBUG_RC_HISTORY)
		    dump_rc_history(&n[i]);
#endif
		}
	    }
	}

	for (c = lease_hunks; c; c = n) {
		n = c->next;
		if (c->ends != c->starts) {
			log_info("lease hunk %lx leases %ld free %ld",
				 (unsigned long)c, (unsigned long)(c->starts),
				 (unsigned long)(c->ends));
		}
		dfree(c, MDL);
	}

	/* Free all the rogue leases. */
	for (c = free_leases; c; c = n) {
		n = c->next;
		dfree(c, MDL);
	}
}
#endif

struct lease *new_leases (n, file, line)
	unsigned n;
	const char *file;
	int line;
{
	struct lease *rval;
#if defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
	rval = dmalloc ((n + 1) * sizeof (struct lease), file, line);
	memset (rval, 0, sizeof (struct lease));
	rval -> starts = n;
	rval -> next = lease_hunks;
	lease_hunks = rval;
	rval++;
#else
	rval = dmalloc (n * sizeof (struct lease), file, line);
#endif
	return rval;
}

/* If we are allocating leases in aggregations, there's really no way
   to free one, although perhaps we can maintain a free list. */

isc_result_t dhcp_lease_free (omapi_object_t *lo,
			      const char *file, int line)
{
	struct lease *lease;
	if (lo -> type != dhcp_type_lease)
		return DHCP_R_INVALIDARG;
	lease = (struct lease *)lo;
	memset (lease, 0, sizeof (struct lease));
	lease -> next = free_leases;
	free_leases = lease;
	return ISC_R_SUCCESS;
}

isc_result_t dhcp_lease_get (omapi_object_t **lp,
			     const char *file, int line)
{
	struct lease **lease = (struct lease **)lp;
	struct lease *lt;

	if (free_leases) {
		lt = free_leases;
		free_leases = lt -> next;
		*lease = lt;
		return ISC_R_SUCCESS;
	}
	return ISC_R_NOMEMORY;
}
#endif /* COMPACT_LEASES */

OMAPI_OBJECT_ALLOC (lease, struct lease, dhcp_type_lease)
OMAPI_OBJECT_ALLOC (class, struct class, dhcp_type_class)
OMAPI_OBJECT_ALLOC (subclass, struct class, dhcp_type_subclass)
OMAPI_OBJECT_ALLOC (pool, struct pool, dhcp_type_pool)

#if !defined (NO_HOST_FREES)	/* Scary debugging mode - don't enable! */
OMAPI_OBJECT_ALLOC (host, struct host_decl, dhcp_type_host)
#else
isc_result_t host_allocate (struct host_decl **p, const char *file, int line)
{
	return omapi_object_allocate ((omapi_object_t **)p,
				      dhcp_type_host, 0, file, line);
}

isc_result_t host_reference (struct host_decl **pptr, struct host_decl *ptr,
			       const char *file, int line)
{
	return omapi_object_reference ((omapi_object_t **)pptr,
				       (omapi_object_t *)ptr, file, line);
}

isc_result_t host_dereference (struct host_decl **ptr,
			       const char *file, int line)
{
	if ((*ptr) -> refcnt == 1) {
		log_error ("host dereferenced with refcnt == 1.");
#if defined (DEBUG_RC_HISTORY)
		dump_rc_history ();
#endif
		abort ();
	}
	return omapi_object_dereference ((omapi_object_t **)ptr, file, line);
}
#endif

struct lease_state *free_lease_states;

struct lease_state *new_lease_state (file, line)
	const char *file;
	int line;
{
	struct lease_state *rval;

	if (free_lease_states) {
		rval = free_lease_states;
		free_lease_states =
			(struct lease_state *)(free_lease_states -> next);
 		dmalloc_reuse (rval, file, line, 0);
	} else {
		rval = dmalloc (sizeof (struct lease_state), file, line);
		if (!rval)
			return rval;
	}
	memset (rval, 0, sizeof *rval);
	if (!option_state_allocate (&rval -> options, file, line)) {
		free_lease_state (rval, file, line);
		return (struct lease_state *)0;
	}
	return rval;
}

void free_lease_state (ptr, file, line)
	struct lease_state *ptr;
	const char *file;
	int line;
{
	if (ptr -> options)
		option_state_dereference (&ptr -> options, file, line);
	if (ptr -> packet)
		packet_dereference (&ptr -> packet, file, line);
	if (ptr -> shared_network)
		shared_network_dereference (&ptr -> shared_network,
					    file, line);

	data_string_forget (&ptr -> parameter_request_list, file, line);
	data_string_forget (&ptr -> filename, file, line);
	data_string_forget (&ptr -> server_name, file, line);
	ptr -> next = free_lease_states;
	free_lease_states = ptr;
	dmalloc_reuse (free_lease_states, (char *)0, 0, 0);
}

#if defined (DEBUG_MEMORY_LEAKAGE) || \
		defined (DEBUG_MEMORY_LEAKAGE_ON_EXIT)
void relinquish_free_lease_states ()
{
	struct lease_state *cs, *ns;

	for (cs = free_lease_states; cs; cs = ns) {
		ns = cs -> next;
		dfree (cs, MDL);
	}
	free_lease_states = (struct lease_state *)0;
}
#endif

struct permit *new_permit (file, line)
	const char *file;
	int line;
{
	struct permit *permit = ((struct permit *)
				 dmalloc (sizeof (struct permit), file, line));
	if (!permit)
		return permit;
	memset (permit, 0, sizeof *permit);
	return permit;
}

void free_permit (permit, file, line)
	struct permit *permit;
	const char *file;
	int line;
{
	if (permit -> type == permit_class)
		class_dereference (&permit -> class, MDL);
	dfree (permit, file, line);
}
