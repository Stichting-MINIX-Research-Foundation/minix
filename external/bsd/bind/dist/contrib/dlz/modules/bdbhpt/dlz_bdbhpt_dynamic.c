/*	$NetBSD: dlz_bdbhpt_dynamic.c,v 1.1.1.5 2015/07/08 15:37:45 christos Exp $	*/

/*
 * Copyright (C) 2002 Stichting NLnet, Netherlands, stichting@nlnet.nl.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND STICHTING NLNET
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * STICHTING NLNET BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
 * USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * The development of Dynamically Loadable Zones (DLZ) for Bind 9 was
 * conceived and contributed by Rob Butler.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ROB BUTLER
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * ROB BUTLER BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
 * USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Copyright (C) 2011  Internet Systems Consortium, Inc. ("ISC")
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

/*
 * This is simply a merge of Andrew Tridgell's dlz_example.c and the
 * original bdb_bdbhpt_driver.c
 *
 * This provides the externally loadable bdbhpt DLZ driver, without
 * update support
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

#include <db.h>

#include "dlz_minimal.h"

/* should the bdb driver use threads. */
#ifdef ISC_PLATFORM_USETHREADS
#define bdbhpt_threads DB_THREAD
#else
#define bdbhpt_threads 0
#endif

/* bdbhpt database names */
#define dlz_data   "dns_data"
#define dlz_zone   "dns_zone"
#define dlz_xfr    "dns_xfr"
#define dlz_client "dns_client"

#define dlz_bdbhpt_dynamic_version "0.1"

/*
 * This structure contains all our DB handles and helper functions we
 * inherit from the dlz_dlopen driver
 *
 */
typedef struct bdbhpt_instance {
	DB_ENV    *dbenv;       /* bdbhpt environment */
	DB        *data;        /* dns_data database handle */
	DB        *zone;        /* zone database handle */
	DB        *xfr;         /* zone xfr database handle */
	DB        *client;      /* client database handle */

	/* Helper functions from the dlz_dlopen driver */
	log_t *log;
	dns_sdlz_putrr_t *putrr;
	dns_sdlz_putnamedrr_t *putnamedrr;
	dns_dlz_writeablezone_t *writeable_zone;
} bdbhpt_instance_t;

typedef struct bdbhpt_parsed_data {
	char *host;
	char *type;
	int ttl;
	char *data;
} bdbhpt_parsed_data_t;

static void
b9_add_helper(struct bdbhpt_instance *db, const char *helper_name, void *ptr);

/*%
 * Reverses a string in place.
 */
static char
*bdbhpt_strrev(char *str) {
	char *p1, *p2;

	if (! str || ! *str)
		return str;
	for (p1 = str, p2 = str + strlen(str) - 1; p2 > p1; ++p1, --p2) {
		*p1 ^= *p2;
		*p2 ^= *p1;
		*p1 ^= *p2;
	}
	return str;
}

/*%
 * Parses the DBT from the Berkeley DB into a parsed_data record
 * The parsed_data record should be allocated before and passed into the
 * bdbhpt_parse_data function.  The char (type & data) fields should not
 * be "free"d as that memory is part of the DBT data field.  It will be
 * "free"d when the DBT is freed.
 */

static isc_result_t
bdbhpt_parse_data(log_t *log, char *in, bdbhpt_parsed_data_t *pd) {

	char *endp, *ttlStr;
	char *tmp = in;
	char *lastchar = (char *) &tmp[strlen(tmp)];
  
	/*%
	 * String should be formatted as:
	 *   replication_id
	 *   (a space)
	 *   host_name
	 *   (a space)
	 *   ttl
	 *   (a space)
	 *   type
	 *   (a space)
	 *   remaining data
	 *
	 * examples:
	 *
	 * 9191 host 10 A 127.0.0.1
	 * server1_212 host 10 A 127.0.0.2
	 * {xxxx-xxxx-xxxx-xxxx-xxxx} host 10 MX 20 mail.example.com
	 */
  
	/*
	 * we don't need the replication id, so don't
	 * bother saving a pointer to it.
	 */
  
	/* find space after replication id */
	tmp = strchr(tmp, ' ');
	/* verify we found a space */
	if (tmp == NULL)
		return ISC_R_FAILURE;
	/* make sure it is safe to increment pointer */
	if (++tmp > lastchar)
		return ISC_R_FAILURE;

	/* save pointer to host */
	pd->host = tmp;

	/* find space after host and change it to a '\0' */
	tmp = strchr(tmp, ' ');
	/* verify we found a space */
	if (tmp == NULL)
		return ISC_R_FAILURE;
	/* change the space to a null (string terminator) */
	tmp[0] = '\0';
	/* make sure it is safe to increment pointer */
	if (++tmp > lastchar)
		return ISC_R_FAILURE;
  
	/* save pointer to ttl string */
	ttlStr = tmp;
  
	/* find space after ttl and change it to a '\0' */
	tmp = strchr(tmp, ' ');
	/* verify we found a space */
	if (tmp == NULL)
		return ISC_R_FAILURE;
	/* change the space to a null (string terminator) */
	tmp[0] = '\0';
	/* make sure it is safe to increment pointer */
	if (++tmp > lastchar)
		return ISC_R_FAILURE;
	
	/* save pointer to dns type */
	pd->type = tmp;
	
	/* find space after type and change it to a '\0' */
	tmp = strchr(tmp, ' ');
	/* verify we found a space */
	if (tmp == NULL)
		return ISC_R_FAILURE;
	/* change the space to a null (string terminator) */
	tmp[0] = '\0';
	/* make sure it is safe to increment pointer */
	if (++tmp > lastchar)
		return ISC_R_FAILURE;
	
	/* save pointer to remainder of DNS data */
	pd->data = tmp;
	
	/* convert ttl string to integer */
	pd->ttl = strtol(ttlStr, &endp, 10);
	if (*endp != '\0' || pd->ttl < 0) {
		log(ISC_LOG_ERROR,
				"bdbhpt_dynamic: "
				"ttl must be a positive number");
		return ISC_R_FAILURE;
	}
	
	/* if we get this far everything should have worked. */
	return ISC_R_SUCCESS;
}

/*
 * See if a zone transfer is allowed
 */
isc_result_t
dlz_allowzonexfr(void *dbdata, const char *name, const char *client) {
	isc_result_t result;
	bdbhpt_instance_t *db = (bdbhpt_instance_t *) dbdata;
	DBT key, data;
	
	/* check to see if we are authoritative for the zone first. */
#if DLZ_DLOPEN_VERSION >= 3
	result = dlz_findzonedb(dbdata, name, NULL, NULL);
#else
	result = dlz_findzonedb(dbdata, name);
#endif
	if (result != ISC_R_SUCCESS)
		return (ISC_R_NOTFOUND);
	
	memset(&key, 0, sizeof(DBT));
	key.flags = DB_DBT_MALLOC;
	key.data = strdup(name);
	if (key.data == NULL) {
		result = ISC_R_NOMEMORY;
		goto xfr_cleanup;
	}
	key.size = strlen(key.data);
	
	memset(&data, 0, sizeof(DBT));
	data.flags = DB_DBT_MALLOC;
	data.data = strdup(client);
	if (data.data == NULL) {
		result = ISC_R_NOMEMORY;
		goto xfr_cleanup;
	}
	data.size = strlen(data.data);
	
	switch(db->client->get(db->client, NULL, &key, &data, DB_GET_BOTH)) {
	case DB_NOTFOUND:
		result = ISC_R_NOTFOUND;
		break;
	case 0:
		result = ISC_R_SUCCESS;
		break;
	default:
		result = ISC_R_FAILURE;
	}

 xfr_cleanup:
	/* free any memory duplicate string in the key field */
	if (key.data != NULL)
		free(key.data);
	
	/* free any memory allocated to the data field. */
	if (data.data != NULL)
		free(data.data);
	
	return result;
}

/*%
 * Perform a zone transfer
 *
 * BDB does not allow a secondary index on a database that allows
 * duplicates.	We have a few options:
 *
 * 1) kill speed by having lookup method use a secondary db which
 * is associated to the primary DB with the DNS data.	 Then have
 * another secondary db for zone transfer which also points to
 * the dns_data primary.	NO - The	point of this driver is
 * lookup performance.
 *
 * 2) Blow up database size by storing DNS data twice.	Once for
 * the lookup (dns_data) database, and a second time for the zone
 * transfer (dns_xfr) database. NO - That would probably require
 * a larger cache to provide good performance.	Also, that would
 * make the DB larger on disk potentially slowing it as well.
 *
 * 3) Loop through the dns_xfr database with a cursor to get
 * all the different hosts in a zone.	 Then use the zone & host
 * together to lookup the data in the dns_data database. YES -
 * This may slow down zone xfr's a little, but that's ok they
 * don't happen as often and don't need to be as fast. We can
 * also use this table when deleting a zone (The BDB driver
 * is read only - the delete would be used during replication
 * updates by a separate process).
 */
isc_result_t
dlz_allnodes(const char *zone, void *dbdata, dns_sdlzallnodes_t *allnodes) {
	isc_result_t result = ISC_R_NOTFOUND;
	bdbhpt_instance_t *db = (bdbhpt_instance_t *) dbdata;
	DBC *xfr_cursor = NULL;
	DBC *dns_cursor = NULL;
	DBT xfr_key, xfr_data, dns_key, dns_data;
	int xfr_flags;
	int dns_flags;
	int bdbhptres;
	bdbhpt_parsed_data_t pd;
	char *tmp = NULL, *tmp_zone, *tmp_zone_host = NULL;
	
	memset(&xfr_key, 0, sizeof(DBT));
	memset(&xfr_data, 0, sizeof(DBT));
	memset(&dns_key, 0, sizeof(DBT));
	memset(&dns_data, 0, sizeof(DBT));
	
	xfr_key.data = tmp_zone = strdup(zone);
	if (xfr_key.data == NULL)
		return (ISC_R_NOMEMORY);
	
	xfr_key.size = strlen(xfr_key.data);
	
	/* get a cursor to loop through dns_xfr table */
	if (db->xfr->cursor(db->xfr, NULL, &xfr_cursor, 0) != 0) {
		result = ISC_R_FAILURE;
		goto allnodes_cleanup;
	}
	
	/* get a cursor to loop through dns_data table */
	if (db->data->cursor(db->data, NULL, &dns_cursor, 0) != 0) {
		result = ISC_R_FAILURE;
		goto allnodes_cleanup;
	}
	
	xfr_flags = DB_SET;
	
	/* loop through xfr table for specified zone. */
	while ((bdbhptres = xfr_cursor->c_get(xfr_cursor, &xfr_key,
					      &xfr_data, xfr_flags)) == 0)
	{
		xfr_flags = DB_NEXT_DUP;
		
		/* +1 to allow for space between zone and host names */
		dns_key.size = xfr_data.size + xfr_key.size + 1;
		
		/* +1 to allow for null term at end of string. */
		dns_key.data = tmp_zone_host = malloc(dns_key.size + 1);
		if (dns_key.data == NULL)
			goto allnodes_cleanup;
		
		/*
		 * construct search key for dns_data.
		 * zone_name(a space)host_name
		 */
		strcpy(dns_key.data, zone);
		strcat(dns_key.data, " ");
		strncat(dns_key.data, xfr_data.data, xfr_data.size);
		
		dns_flags = DB_SET;
		
		while ((bdbhptres = dns_cursor->c_get(dns_cursor,
						      &dns_key,
						      &dns_data,
						      dns_flags)) == 0)
		{
			dns_flags = DB_NEXT_DUP;
			
			/* +1 to allow for null term at end of string. */
			tmp = realloc(tmp, dns_data.size + 1);
			if (tmp == NULL)
				goto allnodes_cleanup;
			
			/* copy data to tmp string, and append null term. */
			strncpy(tmp, dns_data.data, dns_data.size);
			tmp[dns_data.size] = '\0';
			
			/* split string into dns data parts. */
			if (bdbhpt_parse_data(db->log,
					      tmp, &pd) != ISC_R_SUCCESS)
				goto allnodes_cleanup;
			result = db->putnamedrr(allnodes, pd.host,
						pd.type, pd.ttl, pd.data);
			if (result != ISC_R_SUCCESS)
				goto allnodes_cleanup;
			
		}	 /* end inner while loop */
		
		/* clean up memory */
		if (tmp_zone_host != NULL) {
			free(tmp_zone_host);
			tmp_zone_host = NULL;
		}
	} /* end outer while loop */
	
 allnodes_cleanup:
	/* free any memory */
	if (tmp != NULL)
		free(tmp);
	
	if (tmp_zone_host != NULL)
		free(tmp_zone_host);
	
	if (tmp_zone != NULL)
		free(tmp_zone);
	
	/* get rid of cursors */
	if (xfr_cursor != NULL)
		xfr_cursor->c_close(xfr_cursor);
	
	if (dns_cursor != NULL)
		dns_cursor->c_close(dns_cursor);
	
	return result;
}

/*%
 * Performs bdbhpt cleanup.
 * Used by bdbhpt_create if there is an error starting up.
 * Used by bdbhpt_destroy when the driver is shutting down.
 */
static void
bdbhpt_cleanup(bdbhpt_instance_t *db) {
	/* close databases */
	if (db->data != NULL)
		db->data->close(db->data, 0);
	if (db->xfr != NULL)
		db->xfr->close(db->xfr, 0);
	if (db->zone != NULL)
		db->zone->close(db->zone, 0);
	if (db->client != NULL)
		db->client->close(db->client, 0);
	
	/* close environment */
	if (db->dbenv != NULL)
		db->dbenv->close(db->dbenv, 0);
}

/*
 * See if we handle a given zone
 */
#if DLZ_DLOPEN_VERSION < 3
isc_result_t
dlz_findzonedb(void *dbdata, const char *name)
#else
isc_result_t
dlz_findzonedb(void *dbdata, const char *name,
		 dns_clientinfomethods_t *methods,
		 dns_clientinfo_t *clientinfo)
#endif
{
	isc_result_t result;
	bdbhpt_instance_t *db = (bdbhpt_instance_t *) dbdata;
	DBT key, data;
	
	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));
	data.flags = DB_DBT_MALLOC;

#if DLZ_DLOPEN_VERSION >= 3
	UNUSED(methods);
	UNUSED(clientinfo);
#endif

	key.data = strdup(name);

	if (key.data == NULL)
		return (ISC_R_NOMEMORY);

	/*
	 * reverse string to take advantage of BDB locality of reference
	 * if we need futher lookups because the zone doesn't match the
	 * first time.
	 */
	key.data = bdbhpt_strrev(key.data);
	key.size = strlen(key.data);

	switch(db->zone->get(db->zone, NULL, &key, &data, 0)) {
	case DB_NOTFOUND:
		result = ISC_R_NOTFOUND;
		break;
	case 0:
		result = ISC_R_SUCCESS;
		break;
	default:
		result = ISC_R_FAILURE;
	}
	
	/* free any memory duplicate string in the key field */
	if (key.data != NULL)
		free(key.data);
	
	/* free any memory allocated to the data field. */
	if (data.data != NULL)
		free(data.data);
	
	return result;
}

/*
 * Look up one record in the database.
 *
 */
#if DLZ_DLOPEN_VERSION == 1
isc_result_t dlz_lookup(const char *zone, const char *name, 
			void *dbdata, dns_sdlzlookup_t *lookup)
#else
isc_result_t dlz_lookup(const char *zone, const char *name, void *dbdata,
			dns_sdlzlookup_t *lookup,
			dns_clientinfomethods_t *methods,
			dns_clientinfo_t *clientinfo)
#endif
{
	isc_result_t result = ISC_R_NOTFOUND;
	bdbhpt_instance_t *db = (bdbhpt_instance_t *) dbdata;
	DBC *data_cursor = NULL;
	DBT key, data;
	int bdbhptres;
	int flags;

	bdbhpt_parsed_data_t pd;
	char *tmp = NULL;
	char *keyStr = NULL;
	
#if DLZ_DLOPEN_VERSION >= 2
	UNUSED(methods);
	UNUSED(clientinfo);
#endif

	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));
	
	key.size = strlen(zone) + strlen(name) + 1;

	/* allocate mem for key */
	key.data = keyStr = malloc((key.size + 1) * sizeof(char));
	
	if (keyStr == NULL)
		return ISC_R_NOMEMORY;
	
	strcpy(keyStr, zone);
	strcat(keyStr, " ");
	strcat(keyStr, name);
	
	/* get a cursor to loop through data */
	if (db->data->cursor(db->data, NULL, &data_cursor, 0) != 0) {
		result = ISC_R_FAILURE;
		goto lookup_cleanup;
	}

	result = ISC_R_NOTFOUND;

	flags = DB_SET;
	while ((bdbhptres = data_cursor->c_get(data_cursor, &key, &data,
					       flags)) == 0)
	{ 
		flags = DB_NEXT_DUP;
		tmp = realloc(tmp, data.size + 1);
		if (tmp == NULL)
			goto lookup_cleanup;
		
		strncpy(tmp, data.data, data.size);
		tmp[data.size] = '\0';
		
		if (bdbhpt_parse_data(db->log, tmp, &pd) != ISC_R_SUCCESS)
			goto lookup_cleanup;
		
		result = db->putrr(lookup, pd.type, pd.ttl, pd.data);
		if (result != ISC_R_SUCCESS)
			goto lookup_cleanup;
	} /* end while loop */
	
 lookup_cleanup:
	/* get rid of cursor */
	if (data_cursor != NULL)
		data_cursor->c_close(data_cursor);
	
	if (keyStr != NULL)
		free(keyStr);
	if (tmp != NULL)
		free(tmp);
	
	return result;
}

/*%
 * Initialises, sets flags and then opens Berkeley databases.
 */
static isc_result_t
bdbhpt_opendb(log_t *log, DB_ENV *db_env, DBTYPE db_type, DB **db,
	      const char *db_name, char *db_file, int flags)
{
	int result;

	/* Initialise the database. */
	if ((result = db_create(db, db_env, 0)) != 0) {
		log(ISC_LOG_ERROR,
		    "bdbhpt_dynamic: could not initialize %s database. "
		    "BerkeleyDB error: %s",
		    db_name, db_strerror(result));
		return ISC_R_FAILURE;
	}

	/* set database flags. */
	if ((result = (*db)->set_flags(*db, flags)) != 0) {
		log(ISC_LOG_ERROR,
		    "bdbhpt_dynamic: could not set flags for %s database. "
		    "BerkeleyDB error: %s",
		    db_name, db_strerror(result));
		return ISC_R_FAILURE;
	}

	/* open the database. */
	if ((result = (*db)->open(*db, NULL, db_file, db_name, db_type,
				  DB_RDONLY | bdbhpt_threads, 0)) != 0) {
		log(ISC_LOG_ERROR,
		    "bdbhpt_dynamic: could not open %s database in %s. "
		    "BerkeleyDB error: %s",
		    db_name, db_file, db_strerror(result));
		return ISC_R_FAILURE;
	}
	
	return ISC_R_SUCCESS;
}


/*
 * Called to initialize the driver
 */
isc_result_t
dlz_create(const char *dlzname, unsigned int argc, char *argv[],
		 void **dbdata, ...)
{
	isc_result_t result;
	int bdbhptres;
	int bdbFlags = 0;
	bdbhpt_instance_t *db = NULL;
	
	const char *helper_name;
	va_list ap;

	UNUSED(dlzname);

	/* Allocate memory for our db structures and helper functions */
	db = calloc(1, sizeof(struct bdbhpt_instance));
	if (db == NULL)
		return (ISC_R_NOMEMORY);

	/* Fill in the helper functions */
	va_start(ap, dbdata);
	while ((helper_name = va_arg(ap, const char *)) != NULL)
		b9_add_helper(db, helper_name, va_arg(ap, void*));
	va_end(ap);

	/* verify we have 4 arg's passed to the driver */
	if (argc != 4) {
		db->log(ISC_LOG_ERROR,
			"bdbhpt_dynamic: please supply 3 command line args. "
			"You supplied: %s", argc);
		return (ISC_R_FAILURE);
	}

	switch((char) *argv[1]) {
		/*
		 * Transactional mode.	Highest safety - lowest speed.
		 */
	case 'T':
	case 't':
		bdbFlags = DB_INIT_MPOOL | DB_INIT_LOCK |
			DB_INIT_LOG | DB_INIT_TXN;
		db->log(ISC_LOG_INFO,
			"bdbhpt_dynamic: using transactional mode.");
		break;

		/*
		 * Concurrent mode.	 Lower safety (no rollback) -
		 * higher speed.
		 */
	case 'C':
	case 'c':
		bdbFlags = DB_INIT_CDB | DB_INIT_MPOOL;
		db->log(ISC_LOG_INFO,
			"bdbhpt_dynamic: using concurrent mode.");
		break;

		/*
		 * Private mode. No inter-process communication & no locking.
		 * Lowest saftey - highest speed.
		 */
	case 'P':
	case 'p':
		bdbFlags = DB_PRIVATE | DB_INIT_MPOOL;
		db->log(ISC_LOG_INFO,
			"bdbhpt_dynamic: using private mode.");
		break;
	default:
		db->log(ISC_LOG_ERROR,
			"bdbhpt_dynamic: "
			"operating mode must be set to P or C or T. "
			"You specified '%s'", argv[1]);
		return (ISC_R_FAILURE);
	}
	
	/*
	 * create bdbhpt environment
	 * Basically bdbhpt allocates and assigns memory to db->dbenv
	 */
	bdbhptres = db_env_create(&db->dbenv, 0);
	if (bdbhptres != 0) {
		db->log(ISC_LOG_ERROR,
			"bdbhpt_dynamic: db environment could not be created. "
			"BerkeleyDB error: %s", db_strerror(bdbhptres));
		result = ISC_R_FAILURE;
		goto init_cleanup;
	}
	
	/* open bdbhpt environment */
	bdbhptres = db->dbenv->open(db->dbenv, argv[2],
				    bdbFlags | bdbhpt_threads | DB_CREATE, 0);
	if (bdbhptres != 0) {
		db->log(ISC_LOG_ERROR,
			"bdbhpt_dynamic: "
			"db environment at '%s' could not be opened. "
			"BerkeleyDB error: %s",
			argv[2], db_strerror(bdbhptres));
		result = ISC_R_FAILURE;
		goto init_cleanup;
	}

	/* open dlz_data database. */
	result = bdbhpt_opendb(db->log, db->dbenv, DB_UNKNOWN, &db->data,
			       dlz_data, argv[3], DB_DUP | DB_DUPSORT);
	if (result != ISC_R_SUCCESS)
		goto init_cleanup;

	/* open dlz_xfr database. */
	result = bdbhpt_opendb(db->log, db->dbenv, DB_UNKNOWN, &db->xfr,
			       dlz_xfr, argv[3], DB_DUP | DB_DUPSORT);
	if (result != ISC_R_SUCCESS)
		goto init_cleanup;
	
	/* open dlz_zone database. */
	result = bdbhpt_opendb(db->log, db->dbenv, DB_UNKNOWN, &db->zone,
			       dlz_zone, argv[3], 0);
	if (result != ISC_R_SUCCESS)
		goto init_cleanup;
	
	/* open dlz_client database. */
	result = bdbhpt_opendb(db->log, db->dbenv, DB_UNKNOWN, &db->client,
			       dlz_client, argv[3], DB_DUP | DB_DUPSORT);
	if (result != ISC_R_SUCCESS)
		goto init_cleanup;
	
	*dbdata = db;

	db->log(ISC_LOG_INFO,
		"bdbhpt_dynamic: version %s, started",
		dlz_bdbhpt_dynamic_version);
	return(ISC_R_SUCCESS);
	
 init_cleanup:
	bdbhpt_cleanup(db);
	return result;
}

/*
 * Shut down the backend
 */
void
dlz_destroy(void *dbdata) {
	struct bdbhpt_instance *db = (struct bdbhpt_instance *)dbdata;
	
	db->log(ISC_LOG_INFO,
		"dlz_bdbhpt_dynamic (%s): shutting down",
		dlz_bdbhpt_dynamic_version);
	bdbhpt_cleanup((bdbhpt_instance_t *) dbdata);
	free(db);
}

/*
 * Return the version of the API
 */
int
dlz_version(unsigned int *flags) {
	UNUSED(flags);
	return (DLZ_DLOPEN_VERSION);
}

/*
 * Register a helper function from the bind9 dlz_dlopen driver
 */
static void
b9_add_helper(struct bdbhpt_instance *db, const char *helper_name, void *ptr) {
	if (strcmp(helper_name, "log") == 0)
		db->log = (log_t *)ptr;
	if (strcmp(helper_name, "putrr") == 0)
		db->putrr = (dns_sdlz_putrr_t *)ptr;
	if (strcmp(helper_name, "putnamedrr") == 0)
		db->putnamedrr = (dns_sdlz_putnamedrr_t *)ptr;
	if (strcmp(helper_name, "writeable_zone") == 0)
		db->writeable_zone = (dns_dlz_writeablezone_t *)ptr;
}

