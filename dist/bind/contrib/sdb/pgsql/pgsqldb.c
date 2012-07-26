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

/* $Id: pgsqldb.c,v 1.15 2007-06-19 23:47:07 tbox Exp $ */

#include <config.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <pgsql/libpq-fe.h>

#include <isc/mem.h>
#include <isc/print.h>
#include <isc/result.h>
#include <isc/util.h>

#include <dns/sdb.h>
#include <dns/result.h>

#include <named/globals.h>

#include "pgsqldb.h"

/*
 * A simple database driver that interfaces to a PostgreSQL database.  This
 * is not complete, and not designed for general use.  It opens one
 * connection to the database per zone, which is inefficient.  It also may
 * not handle quoting correctly.
 *
 * The table must contain the fields "name", "rdtype", and "rdata", and 
 * is expected to contain a properly constructed zone.  The program "zonetodb"
 * creates such a table.
 */

static dns_sdbimplementation_t *pgsqldb = NULL;

struct dbinfo {
	PGconn *conn;
	char *database;
	char *table;
	char *host;
	char *user;
	char *passwd;
};

static void
pgsqldb_destroy(const char *zone, void *driverdata, void **dbdata);

/*
 * Canonicalize a string before writing it to the database.
 * "dest" must be an array of at least size 2*strlen(source) + 1.
 */
static void
quotestring(const char *source, char *dest) {
	while (*source != 0) {
		if (*source == '\'')
			*dest++ = '\'';
		/* SQL doesn't treat \ as special, but PostgreSQL does */
		else if (*source == '\\')
			*dest++ = '\\';
		*dest++ = *source++;
	}
	*dest++ = 0;
}

/*
 * Connect to the database.
 */
static isc_result_t
db_connect(struct dbinfo *dbi) {
	dbi->conn = PQsetdbLogin(dbi->host, NULL, NULL, NULL, dbi->database,
				 dbi->user, dbi->passwd);

	if (PQstatus(dbi->conn) == CONNECTION_OK)
		return (ISC_R_SUCCESS);
	else
		return (ISC_R_FAILURE);
}

/*
 * Check to see if the connection is still valid.  If not, attempt to
 * reconnect.
 */
static isc_result_t
maybe_reconnect(struct dbinfo *dbi) {
	if (PQstatus(dbi->conn) == CONNECTION_OK)
		return (ISC_R_SUCCESS);

	return (db_connect(dbi));
}

/*
 * This database operates on absolute names.
 *
 * Queries are converted into SQL queries and issued synchronously.  Errors
 * are handled really badly.
 */
static isc_result_t
pgsqldb_lookup(const char *zone, const char *name, void *dbdata,
	       dns_sdblookup_t *lookup)
{
	isc_result_t result;
	struct dbinfo *dbi = dbdata;
	PGresult *res;
	char str[1500];
	char *canonname;
	int i;

	UNUSED(zone);

	canonname = isc_mem_get(ns_g_mctx, strlen(name) * 2 + 1);
	if (canonname == NULL)
		return (ISC_R_NOMEMORY);
	quotestring(name, canonname);
	snprintf(str, sizeof(str),
		 "SELECT TTL,RDTYPE,RDATA FROM \"%s\" WHERE "
		 "lower(NAME) = lower('%s')", dbi->table, canonname);
	isc_mem_put(ns_g_mctx, canonname, strlen(name) * 2 + 1);

	result = maybe_reconnect(dbi);
	if (result != ISC_R_SUCCESS)
		return (result);

	res = PQexec(dbi->conn, str);
	if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
		PQclear(res);
		return (ISC_R_FAILURE);
	}
	if (PQntuples(res) == 0) {
		PQclear(res);
		return (ISC_R_NOTFOUND);
	}

	for (i = 0; i < PQntuples(res); i++) {
		char *ttlstr = PQgetvalue(res, i, 0);
		char *type = PQgetvalue(res, i, 1);
		char *data = PQgetvalue(res, i, 2);
		dns_ttl_t ttl;
		char *endp;
		ttl = strtol(ttlstr, &endp, 10);
		if (*endp != '\0') {
			PQclear(res);
			return (DNS_R_BADTTL);
		}
		result = dns_sdb_putrr(lookup, type, ttl, data);
		if (result != ISC_R_SUCCESS) {
			PQclear(res);
			return (ISC_R_FAILURE);
		}
	}

	PQclear(res);
	return (ISC_R_SUCCESS);
}

/*
 * Issue an SQL query to return all nodes in the database and fill the
 * allnodes structure.
 */
static isc_result_t
pgsqldb_allnodes(const char *zone, void *dbdata, dns_sdballnodes_t *allnodes) {
	struct dbinfo *dbi = dbdata;
	PGresult *res;
	isc_result_t result;
	char str[1500];
	int i;

	UNUSED(zone);

	snprintf(str, sizeof(str),
		 "SELECT TTL,NAME,RDTYPE,RDATA FROM \"%s\" ORDER BY NAME",
		 dbi->table);

	result = maybe_reconnect(dbi);
	if (result != ISC_R_SUCCESS)
		return (result);

	res = PQexec(dbi->conn, str);
	if (!res || PQresultStatus(res) != PGRES_TUPLES_OK ) {
		PQclear(res);
		return (ISC_R_FAILURE);
	}
	if (PQntuples(res) == 0) {
		PQclear(res);
		return (ISC_R_NOTFOUND);
	}

	for (i = 0; i < PQntuples(res); i++) {
		char *ttlstr = PQgetvalue(res, i, 0);
		char *name = PQgetvalue(res, i, 1);
		char *type = PQgetvalue(res, i, 2);
		char *data = PQgetvalue(res, i, 3);
		dns_ttl_t ttl;
		char *endp;
		ttl = strtol(ttlstr, &endp, 10);
		if (*endp != '\0') {
			PQclear(res);
			return (DNS_R_BADTTL);
		}
		result = dns_sdb_putnamedrr(allnodes, name, type, ttl, data);
		if (result != ISC_R_SUCCESS) {
			PQclear(res);
			return (ISC_R_FAILURE);
		}
	}

	PQclear(res);
	return (ISC_R_SUCCESS);
}

/*
 * Create a connection to the database and save any necessary information
 * in dbdata.
 *
 * argv[0] is the name of the database
 * argv[1] is the name of the table
 * argv[2] (if present) is the name of the host to connect to
 * argv[3] (if present) is the name of the user to connect as
 * argv[4] (if present) is the name of the password to connect with
 */
static isc_result_t
pgsqldb_create(const char *zone, int argc, char **argv,
	       void *driverdata, void **dbdata)
{
	struct dbinfo *dbi;
	isc_result_t result;

	UNUSED(zone);
	UNUSED(driverdata);

	if (argc < 2)
		return (ISC_R_FAILURE);

	dbi = isc_mem_get(ns_g_mctx, sizeof(struct dbinfo));
	if (dbi == NULL)
		return (ISC_R_NOMEMORY);
	dbi->conn = NULL;
	dbi->database = NULL;
	dbi->table = NULL;
	dbi->host = NULL;
	dbi->user = NULL;
	dbi->passwd = NULL;

#define STRDUP_OR_FAIL(target, source)				\
	do {							\
		target = isc_mem_strdup(ns_g_mctx, source);	\
		if (target == NULL) {				\
			result = ISC_R_NOMEMORY;		\
			goto cleanup;				\
		}						\
	} while (0);

	STRDUP_OR_FAIL(dbi->database, argv[0]);
	STRDUP_OR_FAIL(dbi->table, argv[1]);
	if (argc > 2)
		STRDUP_OR_FAIL(dbi->host, argv[2]);
	if (argc > 3)
		STRDUP_OR_FAIL(dbi->user, argv[3]);
	if (argc > 4)
		STRDUP_OR_FAIL(dbi->passwd, argv[4]);

	result = db_connect(dbi);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	*dbdata = dbi;
	return (ISC_R_SUCCESS);

 cleanup:
	pgsqldb_destroy(zone, driverdata, (void **)&dbi);
	return (result);
}

/*
 * Close the connection to the database.
 */
static void
pgsqldb_destroy(const char *zone, void *driverdata, void **dbdata) {
	struct dbinfo *dbi = *dbdata;

	UNUSED(zone);
	UNUSED(driverdata);

	if (dbi->conn != NULL)
		PQfinish(dbi->conn);
	if (dbi->database != NULL)
		isc_mem_free(ns_g_mctx, dbi->database);
	if (dbi->table != NULL)
		isc_mem_free(ns_g_mctx, dbi->table);
	if (dbi->host != NULL)
		isc_mem_free(ns_g_mctx, dbi->host);
	if (dbi->user != NULL)
		isc_mem_free(ns_g_mctx, dbi->user);
	if (dbi->passwd != NULL)
		isc_mem_free(ns_g_mctx, dbi->passwd);
	if (dbi->database != NULL)
		isc_mem_free(ns_g_mctx, dbi->database);
	isc_mem_put(ns_g_mctx, dbi, sizeof(struct dbinfo));
}

/*
 * Since the SQL database corresponds to a zone, the authority data should
 * be returned by the lookup() function.  Therefore the authority() function
 * is NULL.
 */
static dns_sdbmethods_t pgsqldb_methods = {
	pgsqldb_lookup,
	NULL, /* authority */
	pgsqldb_allnodes,
	pgsqldb_create,
	pgsqldb_destroy
};

/*
 * Wrapper around dns_sdb_register().
 */
isc_result_t
pgsqldb_init(void) {
	unsigned int flags;
	flags = 0;
	return (dns_sdb_register("pgsql", &pgsqldb_methods, NULL, flags,
				 ns_g_mctx, &pgsqldb));
}

/*
 * Wrapper around dns_sdb_unregister().
 */
void
pgsqldb_clear(void) {
	if (pgsqldb != NULL)
		dns_sdb_unregister(&pgsqldb);
}
