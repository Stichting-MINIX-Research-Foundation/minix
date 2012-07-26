/*
 * Copyright (C) 2007  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: sqlitedb.c,v 1.1 2007-03-05 05:30:22 marka Exp $ */

#include <config.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sqlite3.h>

#include <isc/mem.h>
#include <isc/print.h>
#include <isc/result.h>
#include <isc/util.h>

#include <dns/sdb.h>
#include <dns/result.h>

#include <named/globals.h>

#include "sqlitedb.h"

/*
 * A simple database driver that interfaces to a SQLite database.
 *
 * The table must contain the fields "name", "rdtype", and "rdata", and 
 * is expected to contain a properly constructed zone.  The program "zonetodb"
 * creates such a table.
 */

static dns_sdbimplementation_t *sqlitedb = NULL;

typedef struct _dbinfo {
    sqlite3 *db;
    char *filename;
    char *table;
} dbinfo_t;


static isc_result_t
db_connect(dbinfo_t *dbi)
{
    if (sqlite3_open(dbi->filename, &dbi->db) == SQLITE_OK) {
	return (ISC_R_SUCCESS);
    } else {
	/* a connection is returned even if the open fails */
	sqlite3_close(dbi->db);
	dbi->db = NULL;
	return (ISC_R_FAILURE);
    }
}


typedef struct _lookup_parm_t {
    int              i;
    dns_sdblookup_t *lookup;
    isc_result_t     result;
} lookup_parm_t;


static int
sqlitedb_lookup_cb(void *p, int cc, char **cv, char **cn)
{
    lookup_parm_t *parm = p;
    dns_ttl_t ttl;
    char *endp;

    /* FIXME - check these(num/names); I'm assuming a mapping for now */
    char *ttlstr = cv[0];
    char *type   = cv[1];
    char *data   = cv[2];

    UNUSED(cc);
    UNUSED(cn);

    ttl = strtol(ttlstr, &endp, 10);
    if (*endp) {
	parm->result = DNS_R_BADTTL;
	return 1;
    }

    parm->result = dns_sdb_putrr(parm->lookup, type, ttl, data);

    if (parm->result != ISC_R_SUCCESS)
	return 1;

    (parm->i)++;

    return 0;
}


static isc_result_t
sqlitedb_lookup(const char *zone,
		const char *name, void *dbdata,
		dns_sdblookup_t *lookup)
/*
 * synchronous absolute name lookup
 */
{
    dbinfo_t *dbi = (dbinfo_t *) dbdata;
    char *sql;
    lookup_parm_t parm = { 0, lookup, ISC_R_SUCCESS };
    char *errmsg = NULL;
    int result;

    UNUSED(zone);

    sql = sqlite3_mprintf(
	"SELECT TTL,RDTYPE,RDATA FROM \"%q\" WHERE "
	"lower(NAME) = lower('%q')",
	dbi->table, name);

    result = sqlite3_exec(dbi->db, sql,
			  &sqlitedb_lookup_cb, &parm,
			  &errmsg);
    sqlite3_free(sql);

    if (result != SQLITE_OK)
	return (ISC_R_FAILURE);
    if (parm.i == 0)
	return (ISC_R_NOTFOUND);

    return (ISC_R_SUCCESS);
}


typedef struct _allnodes_parm_t {
    int                i;
    dns_sdballnodes_t *allnodes;
    isc_result_t       result;
} allnodes_parm_t;


static int
sqlitedb_allnodes_cb(void *p, int cc, char **cv, char **cn)
{
    allnodes_parm_t *parm = p;
    dns_ttl_t ttl;
    char *endp;

    /* FIXME - check these(num/names); I'm assuming a mapping for now */
    char *ttlstr = cv[0];
    char *name   = cv[1];
    char *type   = cv[2];
    char *data   = cv[3];

    UNUSED(cc);
    UNUSED(cn);

    ttl = strtol(ttlstr, &endp, 10);
    if (*endp) {
	parm->result = DNS_R_BADTTL;
	return 1;
    }

    parm->result = dns_sdb_putnamedrr(parm->allnodes, name, type, ttl, data);

    if (parm->result != ISC_R_SUCCESS)
	return 1;

    (parm->i)++;

    return 0;
}


static isc_result_t
sqlitedb_allnodes(const char *zone,
		  void *dbdata,
		  dns_sdballnodes_t *allnodes)
{
    dbinfo_t *dbi = (dbinfo_t *) dbdata;
    char *sql;
    allnodes_parm_t parm = { 0, allnodes, ISC_R_SUCCESS };
    char *errmsg = NULL;
    int result;

    UNUSED(zone);

    sql = sqlite3_mprintf(
	"SELECT TTL,NAME,RDTYPE,RDATA FROM \"%q\" ORDER BY NAME",
	dbi->table);

    result = sqlite3_exec(dbi->db, sql,
			  &sqlitedb_allnodes_cb, &parm,
			  &errmsg);
    sqlite3_free(sql);

    if (result != SQLITE_OK)
	return (ISC_R_FAILURE);
    if (parm.i == 0)
	return (ISC_R_NOTFOUND);

    return (ISC_R_SUCCESS);
}


static void
sqlitedb_destroy(const char *zone, void *driverdata, void **dbdata)
{
    dbinfo_t *dbi = *dbdata;

    UNUSED(zone);
    UNUSED(driverdata);

    if (dbi->db != NULL)
	sqlite3_close(dbi->db);
    if (dbi->table != NULL)
	isc_mem_free(ns_g_mctx, dbi->table);
    if (dbi->filename != NULL)
	isc_mem_free(ns_g_mctx, dbi->filename);

    isc_mem_put(ns_g_mctx, dbi, sizeof(dbinfo_t));
}


#define STRDUP_OR_FAIL(target, source)				\
	do {							\
		target = isc_mem_strdup(ns_g_mctx, source);	\
		if (target == NULL) {				\
			result = ISC_R_NOMEMORY;		\
			goto cleanup;				\
		}						\
	} while (0);

/*
 * Create a connection to the database and save any necessary information
 * in dbdata.
 *
 * argv[0] is the name of the database file
 * argv[1] is the name of the table
 */
static isc_result_t
sqlitedb_create(const char *zone,
		int argc, char **argv,
		void *driverdata, void **dbdata)
{
    dbinfo_t *dbi;
    isc_result_t result;

    UNUSED(zone);
    UNUSED(driverdata);

    if (argc < 2)
	return (ISC_R_FAILURE);

    dbi = isc_mem_get(ns_g_mctx, sizeof(dbinfo_t));
    if (dbi == NULL)
	return (ISC_R_NOMEMORY);
    dbi->db       = NULL;
    dbi->filename = NULL;
    dbi->table    = NULL;

    STRDUP_OR_FAIL(dbi->filename, argv[0]);
    STRDUP_OR_FAIL(dbi->table, argv[1]);

    result = db_connect(dbi);
    if (result != ISC_R_SUCCESS)
	goto cleanup;

    *dbdata = dbi;
    return (ISC_R_SUCCESS);

cleanup:
    sqlitedb_destroy(zone, driverdata, (void **)&dbi);
    return (result);
}


/*
 * Since the SQL database corresponds to a zone, the authority data should
 * be returned by the lookup() function.  Therefore the authority() function
 * is NULL.
 */
static dns_sdbmethods_t sqlitedb_methods = {
    sqlitedb_lookup,
    NULL, /* authority */
    sqlitedb_allnodes,
    sqlitedb_create,
    sqlitedb_destroy
};


/*
 * Wrapper around dns_sdb_register().
 */
isc_result_t
sqlitedb_init(void)
{
    unsigned int flags;
    flags = 0;
    return (dns_sdb_register("sqlite", &sqlitedb_methods, NULL, flags,
			     ns_g_mctx, &sqlitedb));
}


/*
 * Wrapper around dns_sdb_unregister().
 */
void
sqlitedb_clear(void)
{
    if (sqlitedb != NULL)
	dns_sdb_unregister(&sqlitedb);
}
