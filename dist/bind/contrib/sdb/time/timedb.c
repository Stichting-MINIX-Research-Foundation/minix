/*
 * Copyright (C) 2004, 2007  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: timedb.c,v 1.10 2007-06-19 23:47:10 tbox Exp $ */

/*
 * A simple database driver that enables the server to return the
 * current time in a DNS record.
 */

#include <config.h>

#include <string.h>
#include <stdio.h>
#include <time.h>

#include <isc/print.h>
#include <isc/result.h>
#include <isc/util.h>

#include <dns/sdb.h>

#include <named/globals.h>

#include "timedb.h"

static dns_sdbimplementation_t *timedb = NULL;

/*
 * This database operates on relative names.
 *
 * "time" and "@" return the time in a TXT record.  
 * "clock" is a CNAME to "time"
 * "current" is a DNAME to "@" (try time.current.time)
 */ 
static isc_result_t
timedb_lookup(const char *zone, const char *name, void *dbdata,
	      dns_sdblookup_t *lookup)
{
	isc_result_t result;

	UNUSED(zone);
	UNUSED(dbdata);

	if (strcmp(name, "@") == 0 || strcmp(name, "time") == 0) {
		time_t now = time(NULL);
		char buf[100];
		int n;

		/*
		 * Call ctime to create the string, put it in quotes, and
		 * remove the trailing newline.
		 */
		n = snprintf(buf, sizeof(buf), "\"%s", ctime(&now));
		if (n < 0)
			return (ISC_R_FAILURE);
		buf[n - 1] = '\"';
		result = dns_sdb_putrr(lookup, "txt", 1, buf);
		if (result != ISC_R_SUCCESS)
			return (ISC_R_FAILURE);
	} else if (strcmp(name, "clock") == 0) {
		result = dns_sdb_putrr(lookup, "cname", 1, "time");
		if (result != ISC_R_SUCCESS)
			return (ISC_R_FAILURE);
	} else if (strcmp(name, "current") == 0) {
		result = dns_sdb_putrr(lookup, "dname", 1, "@");
		if (result != ISC_R_SUCCESS)
			return (ISC_R_FAILURE);
	} else
		return (ISC_R_NOTFOUND);

	return (ISC_R_SUCCESS);
}

/*
 * lookup() does not return SOA or NS records, so authority() must be defined.
 */
static isc_result_t
timedb_authority(const char *zone, void *dbdata, dns_sdblookup_t *lookup) {
	isc_result_t result;

	UNUSED(zone);
	UNUSED(dbdata);

	result = dns_sdb_putsoa(lookup, "localhost.", "root.localhost.", 0);
	if (result != ISC_R_SUCCESS)
		return (ISC_R_FAILURE);

	result = dns_sdb_putrr(lookup, "ns", 86400, "ns1.localdomain.");
	if (result != ISC_R_SUCCESS)
		return (ISC_R_FAILURE);
	result = dns_sdb_putrr(lookup, "ns", 86400, "ns2.localdomain.");
	if (result != ISC_R_SUCCESS)
		return (ISC_R_FAILURE);

	return (ISC_R_SUCCESS);
}

/*
 * This zone does not support zone transfer, so allnodes() is NULL.  There
 * is no database specific data, so create() and destroy() are NULL.
 */
static dns_sdbmethods_t timedb_methods = {
	timedb_lookup,
	timedb_authority,
	NULL,	/* allnodes */
	NULL,	/* create */
	NULL	/* destroy */
};

/*
 * Wrapper around dns_sdb_register().
 */
isc_result_t
timedb_init(void) {
	unsigned int flags;
	flags = DNS_SDBFLAG_RELATIVEOWNER | DNS_SDBFLAG_RELATIVERDATA;
	return (dns_sdb_register("time", &timedb_methods, NULL, flags,
				 ns_g_mctx, &timedb));
}

/*
 * Wrapper around dns_sdb_unregister().
 */
void
timedb_clear(void) {
	if (timedb != NULL)
		dns_sdb_unregister(&timedb);
}
