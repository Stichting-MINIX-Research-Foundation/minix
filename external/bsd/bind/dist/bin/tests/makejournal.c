/*	$NetBSD: makejournal.c,v 1.3 2014/12/10 04:37:53 christos Exp $	*/

/*
 * Copyright (C) 2013  Internet Systems Consortium, Inc. ("ISC")
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

/*! \file */
#include <config.h>

#include <isc/entropy.h>
#include <isc/hash.h>
#include <isc/log.h>
#include <isc/mem.h>
#include <isc/util.h>

#include <dns/db.h>
#include <dns/fixedname.h>
#include <dns/journal.h>
#include <dns/log.h>
#include <dns/name.h>
#include <dns/result.h>
#include <dns/types.h>

#include <stdlib.h>

#define CHECK(r) \
	do { \
		result = (r); \
		if (result != ISC_R_SUCCESS) \
		goto cleanup; \
	} while (/*CONSTCOND*/0)

isc_mem_t *mctx = NULL;
isc_log_t *lctx = NULL;
isc_entropy_t *ectx = NULL;

static isc_boolean_t hash_active = ISC_FALSE, dst_active = ISC_FALSE;

/*
 * Logging categories: this needs to match the list in bin/named/log.c.
 */
static isc_logcategory_t categories[] = {
		{ "",                0 },
		{ "client",          0 },
		{ "network",         0 },
		{ "update",          0 },
		{ "queries",         0 },
		{ "unmatched",       0 },
		{ "update-security", 0 },
		{ "query-errors",    0 },
		{ NULL,              0 }
};

static isc_result_t
loadzone(dns_db_t **db, const char *origin, const char *filename) {
	isc_result_t result;
	dns_fixedname_t fixed;
	dns_name_t *name;

	dns_fixedname_init(&fixed);
	name = dns_fixedname_name(&fixed);

	result = dns_name_fromstring(name, origin, 0, NULL);
	if (result != ISC_R_SUCCESS)
		return(result);

	result = dns_db_create(mctx, "rbt", name, dns_dbtype_zone,
			       dns_rdataclass_in, 0, NULL, db);
	if (result != ISC_R_SUCCESS)
		return (result);

	result = dns_db_load(*db, filename);
	return (result);
}

int
main(int argc, char **argv) {
	isc_result_t result;
	char *origin, *file1, *file2, *journal;
	dns_db_t *old = NULL, *new = NULL;
	isc_logdestination_t destination;
	isc_logconfig_t *logconfig = NULL;

	if (argc != 5) {
		printf("usage: %s origin file1 file2 journal\n", argv[0]);
		return (1);
	}

	origin = argv[1];
	file1 = argv[2];
	file2 = argv[3];
	journal = argv[4];

	isc_mem_debugging |= ISC_MEM_DEBUGRECORD;
	CHECK(isc_mem_create(0, 0, &mctx));
	CHECK(isc_entropy_create(mctx, &ectx));

	CHECK(isc_hash_create(mctx, ectx, DNS_NAME_MAXWIRE));
	hash_active = ISC_TRUE;

	CHECK(dst_lib_init(mctx, ectx, ISC_ENTROPY_BLOCKING));
	dst_active = ISC_TRUE;

	CHECK(isc_log_create(mctx, &lctx, &logconfig));
	isc_log_registercategories(lctx, categories);
	isc_log_setcontext(lctx);
	dns_log_init(lctx);
	dns_log_setcontext(lctx);

	destination.file.stream = stderr;
	destination.file.name = NULL;
	destination.file.versions = ISC_LOG_ROLLNEVER;
	destination.file.maximum_size = 0;
	CHECK(isc_log_createchannel(logconfig, "stderr",
				    ISC_LOG_TOFILEDESC, ISC_LOG_DYNAMIC,
				    &destination, 0));
	CHECK(isc_log_usechannel(logconfig, "stderr", NULL, NULL));

	dns_result_register();

	result = loadzone(&old, origin, file1);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "Couldn't load %s: ", file1);
		goto cleanup;
	}

	result = loadzone(&new, origin, file2);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "Couldn't load %s: ", file2);
		goto cleanup;
	}

	result = dns_db_diff(mctx, new, NULL, old, NULL, journal);

  cleanup:
	if (result != ISC_R_SUCCESS)
		fprintf(stderr, "%s\n", isc_result_totext(result));

	if (new != NULL)
		dns_db_detach(&new);
	if (old != NULL)
		dns_db_detach(&old);

	if (lctx != NULL)
		isc_log_destroy(&lctx);
	if (dst_active) {
		dst_lib_destroy();
		dst_active = ISC_FALSE;
	}
	if (hash_active) {
		isc_hash_destroy();
		hash_active = ISC_FALSE;
	}
	if (ectx != NULL)
		isc_entropy_detach(&ectx);
	if (mctx != NULL)
		isc_mem_destroy(&mctx);

	return(result != ISC_R_SUCCESS ? 1 : 0);
}

