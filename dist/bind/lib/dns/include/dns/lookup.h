/*
 * Copyright (C) 2004-2007, 2009  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000, 2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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

/* $Id: lookup.h,v 1.14 2009-01-17 23:47:43 tbox Exp $ */

#ifndef DNS_LOOKUP_H
#define DNS_LOOKUP_H 1

/*****
 ***** Module Info
 *****/

/*! \file dns/lookup.h
 * \brief
 * The lookup module performs simple DNS lookups.  It implements
 * the full resolver algorithm, both looking for local data and
 * resolving external names as necessary.
 *
 * MP:
 *\li	The module ensures appropriate synchronization of data structures it
 *	creates and manipulates.
 *
 * Reliability:
 *\li	No anticipated impact.
 *
 * Resources:
 *\li	TBS
 *
 * Security:
 *\li	No anticipated impact.
 *
 * Standards:
 *\li	RFCs:	1034, 1035, 2181, TBS
 *\li	Drafts:	TBS
 */

#include <isc/lang.h>
#include <isc/event.h>

#include <dns/types.h>

ISC_LANG_BEGINDECLS

/*%
 * A 'dns_lookupevent_t' is returned when a lookup completes.
 * The sender field will be set to the lookup that completed.  If 'result'
 * is ISC_R_SUCCESS, then 'names' will contain a list of names associated
 * with the address.  The recipient of the event must not change the list
 * and must not refer to any of the name data after the event is freed.
 */
typedef struct dns_lookupevent {
	ISC_EVENT_COMMON(struct dns_lookupevent);
	isc_result_t			result;
	dns_name_t			*name;
	dns_rdataset_t			*rdataset;
	dns_rdataset_t			*sigrdataset;
	dns_db_t			*db;
	dns_dbnode_t			*node;
} dns_lookupevent_t;

isc_result_t
dns_lookup_create(isc_mem_t *mctx, dns_name_t *name, dns_rdatatype_t type,
		  dns_view_t *view, unsigned int options, isc_task_t *task,
		  isc_taskaction_t action, void *arg, dns_lookup_t **lookupp);
/*%<
 * Finds the rrsets matching 'name' and 'type'.
 *
 * Requires:
 *
 *\li	'mctx' is a valid mctx.
 *
 *\li	'name' is a valid name.
 *
 *\li	'view' is a valid view which has a resolver.
 *
 *\li	'task' is a valid task.
 *
 *\li	lookupp != NULL && *lookupp == NULL
 *
 * Returns:
 *
 *\li	ISC_R_SUCCESS
 *\li	ISC_R_NOMEMORY
 *
 *\li	Any resolver-related error (e.g. ISC_R_SHUTTINGDOWN) may also be
 *	returned.
 */

void
dns_lookup_cancel(dns_lookup_t *lookup);
/*%<
 * Cancel 'lookup'.
 *
 * Notes:
 *
 *\li	If 'lookup' has not completed, post its LOOKUPDONE event with a
 *	result code of ISC_R_CANCELED.
 *
 * Requires:
 *
 *\li	'lookup' is a valid lookup.
 */

void
dns_lookup_destroy(dns_lookup_t **lookupp);
/*%<
 * Destroy 'lookup'.
 *
 * Requires:
 *
 *\li	'*lookupp' is a valid lookup.
 *
 *\li	The caller has received the LOOKUPDONE event (either because the
 *	lookup completed or because dns_lookup_cancel() was called).
 *
 * Ensures:
 *
 *\li	*lookupp == NULL.
 */

ISC_LANG_ENDDECLS

#endif /* DNS_LOOKUP_H */
