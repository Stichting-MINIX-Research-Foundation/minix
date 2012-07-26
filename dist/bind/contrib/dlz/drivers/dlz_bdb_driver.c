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
 * Copyright (C) 1999-2001  Internet Software Consortium.
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

#ifdef DLZ_BDB

#include <config.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <dns/log.h>
#include <dns/sdlz.h>
#include <dns/result.h>

#include <isc/mem.h>
#include <isc/print.h>
#include <isc/result.h>
#include <isc/util.h>

#include <named/globals.h>

#include <dlz/dlz_bdb_driver.h>

#include <db.h>

static dns_sdlzimplementation_t *dlz_bdb = NULL;

/* should the bdb driver use threads. */
#ifdef ISC_PLATFORM_USETHREADS
#define bdb_threads DB_THREAD
#else
#define bdb_threads 0
#endif

/* BDB database names */
#define dlz_data "dns_data"
#define dlz_zone "dns_zone"
#define dlz_host "dns_host"
#define dlz_client "dns_client"

/*%
 * This structure contains all the Berkeley DB handles
 * for this instance of the BDB driver.
 */

typedef struct bdb_instance {
	DB_ENV	*dbenv;		/*%< BDB environment */
	DB	*data;		/*%< dns_data database handle */
	DB	*zone;		/*%< zone database handle */
	DB	*host;		/*%< host database handle */
	DB	*client;	/*%< client database handle */
	isc_mem_t *mctx;	/*%< memory context */

} bdb_instance_t;

typedef struct parsed_data {
	char *zone;
	char *host;
	char *type;
	int ttl;
	char *data;
} parsed_data_t;


/* forward reference */

static isc_result_t
bdb_findzone(void *driverarg, void *dbdata, const char *name);

/*%
 * Parses the DBT from the Berkeley DB into a parsed_data record
 * The parsed_data record should be allocated before and passed into the
 * bdb_parse_data function.  The char (type & data) fields should not
 * be "free"d as that memory is part of the DBT data field.  It will be
 * "free"d when the DBT is freed.
 */

static isc_result_t
bdb_parse_data(char *in, parsed_data_t *pd) {

	char *endp, *ttlStr;
	char *tmp = in;
	char *lastchar = (char *) &tmp[strlen(tmp) + 1];

	/*%
	 * String should be formated as:
	 * zone(a space)host(a space)ttl(a space)type(a space)remaining data
	 * examples:
	 * example.com www 10 A 127.0.0.1
	 * example.com mail 10 A 127.0.0.2
	 * example.com @ 10 MX 20 mail.example.com
	 */

	/* save pointer to zone */
	pd->zone = tmp;

	/* find space after zone and change it to a '\0' */
	tmp = strchr(tmp, ' ');
	/* verify we found a space */
	if (tmp == NULL)
		return ISC_R_FAILURE;
	/* change the space to a null (string terminator) */
	tmp[0] = '\0';
	/* make sure it is safe to increment pointer */
	if (++tmp > lastchar)
		return ISC_R_FAILURE;

	/* save pointer to host */
	pd->host = tmp;

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

	/* save pointer to dns ttl */
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

	/* save pointer to remainder of DNS data */
	pd->data = tmp;

	/* convert ttl string to integer */
	pd->ttl = strtol(ttlStr, &endp, 10);
	if (*endp != '\0' || pd->ttl < 0) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "BDB driver ttl must be a postive number");
		return ISC_R_FAILURE;
	}

	/* if we get this far everything should have worked. */
	return ISC_R_SUCCESS;
}

/*
 * DLZ methods
 */

static isc_result_t
bdb_allowzonexfr(void *driverarg, void *dbdata, const char *name,
		 const char *client)
{
	isc_result_t result;
	bdb_instance_t *db = (bdb_instance_t *) dbdata;
	DBC *client_cursor = NULL;
	DBT key, data;

	/* check to see if we are authoritative for the zone first. */
	result = bdb_findzone(driverarg, dbdata, name);
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

	/* get a cursor to loop through zone data */
	if (db->client->cursor(db->client, NULL, &client_cursor, 0) != 0) {
		result = ISC_R_FAILURE;
		goto xfr_cleanup;
	}

	switch(client_cursor->c_get(client_cursor, &key, &data, DB_GET_BOTH)) {
	case DB_NOTFOUND:
	case DB_SECONDARY_BAD:
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

	/* get rid of zone_cursor */
	if (client_cursor != NULL)
		client_cursor->c_close(client_cursor);

	return result;

}

static isc_result_t
bdb_allnodes(const char *zone, void *driverarg, void *dbdata,
	     dns_sdlzallnodes_t *allnodes)
{

	isc_result_t result = ISC_R_NOTFOUND;
	bdb_instance_t *db = (bdb_instance_t *) dbdata;
	DBC *zone_cursor = NULL;
	DBT key, data;
	int flags;
	int bdbres;
	parsed_data_t pd;
	char *tmp = NULL, *tmp_zone;

	UNUSED(driverarg);

	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));

	key.data = tmp_zone = strdup(zone);

	if (key.data == NULL)
		return (ISC_R_NOMEMORY);

	key.size = strlen(key.data);

	/* get a cursor to loop through zone data */
	if (db->zone->cursor(db->zone, NULL, &zone_cursor, 0) != 0) {
		result = ISC_R_FAILURE;
		goto allnodes_cleanup;
	}

	flags = DB_SET;

	while ((bdbres = zone_cursor->c_get(zone_cursor, &key, &data,
					    flags)) == 0) {

		flags = DB_NEXT_DUP;

		tmp = realloc(tmp, data.size + 1);
		if (tmp == NULL)
			goto allnodes_cleanup;

		strncpy(tmp, data.data, data.size);
		tmp[data.size] = '\0';

		if (bdb_parse_data(tmp, &pd) != ISC_R_SUCCESS)
			goto allnodes_cleanup;

		result = dns_sdlz_putnamedrr(allnodes, pd.host, pd.type,
					     pd.ttl, pd.data);
		if (result != ISC_R_SUCCESS)
			goto allnodes_cleanup;

	} /* end while loop */

 allnodes_cleanup:

	if (tmp != NULL)
		free(tmp);

	/* free any memory duplicate string in the key field */
	if (tmp_zone != NULL)
		free(tmp_zone);

	/* get rid of zone_cursor */
	if (zone_cursor != NULL)
		zone_cursor->c_close(zone_cursor);

	return result;

}

/*%
 * Performs BDB cleanup.
 * Used by bdb_create if there is an error starting up.
 * Used by bdb_destroy when the driver is shutting down.
 */

static void
bdb_cleanup(bdb_instance_t *db) {

	isc_mem_t *mctx;

	/* close databases */
	if (db->data != NULL)
		db->data->close(db->data, 0);
	if (db->host != NULL)
		db->host->close(db->host, 0);
	if (db->zone != NULL)
		db->zone->close(db->zone, 0);
	if (db->client != NULL)
		db->client->close(db->client, 0);

	/* close environment */
	if (db->dbenv != NULL)
		db->dbenv->close(db->dbenv, 0);

	/* cleanup memory */
	if (db->mctx != NULL) {
		/* save mctx for later */
		mctx = db->mctx;
		/* return, and detach the memory */
		isc_mem_put(mctx, db, sizeof(bdb_instance_t));
		isc_mem_detach(&mctx);
	}
}

static isc_result_t
bdb_findzone(void *driverarg, void *dbdata, const char *name)
{

	isc_result_t result;
	bdb_instance_t *db = (bdb_instance_t *) dbdata;
	DBC *zone_cursor = NULL;
	DBT key, data;

	UNUSED(driverarg);

	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));
	data.flags = DB_DBT_MALLOC;

	key.data = strdup(name);

	if (key.data == NULL)
		return (ISC_R_NOMEMORY);

	key.size = strlen(key.data);

	/* get a cursor to loop through zone data */
	if (db->zone->cursor(db->zone, NULL, &zone_cursor, 0) != 0) {
		result = ISC_R_NOTFOUND;
		goto findzone_cleanup;
	}

	switch(zone_cursor->c_get(zone_cursor, &key, &data, DB_SET)) {
	case DB_NOTFOUND:
	case DB_SECONDARY_BAD:
		result = ISC_R_NOTFOUND;
		break;
	case 0:
		result = ISC_R_SUCCESS;
		break;
	default:
		result = ISC_R_FAILURE;
	}

 findzone_cleanup:

	/* free any memory duplicate string in the key field */
	if (key.data != NULL)
		free(key.data);

	/* free any memory allocated to the data field. */
	if (data.data != NULL)
		free(data.data);

	/* get rid of zone_cursor */
	if (zone_cursor != NULL)
		zone_cursor->c_close(zone_cursor);

	return result;
}

static isc_result_t
bdb_lookup(const char *zone, const char *name, void *driverarg,
	   void *dbdata, dns_sdlzlookup_t *lookup)
{

	isc_result_t result = ISC_R_NOTFOUND;
	bdb_instance_t *db = (bdb_instance_t *) dbdata;
	DBC *zone_cursor = NULL;
	DBC *host_cursor = NULL;
	DBC *join_cursor = NULL;
	DBT key, data;
	DBC *cur_arr[3];
	int bdbres;
	parsed_data_t pd;
	char *tmp_zone, *tmp_host = NULL;
	char *tmp = NULL;

	UNUSED(driverarg);

	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));

	/* set zone key */
	key.data = tmp_zone = strdup(zone);
	if (key.data == NULL) {
		result = ISC_R_NOMEMORY;
		goto lookup_cleanup;
	}
	key.size = strlen(key.data);

	/* get a cursor to loop through zone data */
	if (db->zone->cursor(db->zone, NULL, &zone_cursor, 0) != 0) {
		result = ISC_R_FAILURE;
		goto lookup_cleanup;
	}

	/* initialize zone_cursor with zone_key */
	if (zone_cursor->c_get(zone_cursor, &key, &data, DB_SET) != 0) {
		result = ISC_R_NOTFOUND;
		goto lookup_cleanup;
	}

	/* set host key */
	key.data = tmp_host = strdup(name);
	if (key.data == NULL) {
		result = ISC_R_NOMEMORY;
		goto lookup_cleanup;
	}
	key.size = strlen(key.data);

	/* get a cursor to loop through host data */
	if (db->host->cursor(db->host, NULL, &host_cursor, 0) != 0) {
		result = ISC_R_FAILURE;
		goto lookup_cleanup;
	}

	/* initialize host_cursor with host_key */
	if (host_cursor->c_get(host_cursor, &key, &data, DB_SET) != 0) {
		result = ISC_R_NOTFOUND;
		goto lookup_cleanup;
	}

	cur_arr[0] = zone_cursor;
	cur_arr[1] = host_cursor;
	cur_arr[2] = NULL;

	db->data->join(db->data, cur_arr, &join_cursor, 0);

	while ((bdbres = join_cursor->c_get(join_cursor, &key,
					    &data, 0)) == 0) {

		tmp = realloc(tmp, data.size + 1);
		if (tmp == NULL)
			goto lookup_cleanup;

		strncpy(tmp, data.data, data.size);
		tmp[data.size] = '\0';

		if (bdb_parse_data(tmp, &pd) != ISC_R_SUCCESS)
			goto lookup_cleanup;

		result = dns_sdlz_putrr(lookup, pd.type, pd.ttl, pd.data);

		if (result != ISC_R_SUCCESS)
			goto lookup_cleanup;
	} /* end while loop */

 lookup_cleanup:

	if (tmp != NULL)
		free(tmp);
	if (tmp_zone != NULL)
		free(tmp_zone);
	if (tmp_host != NULL)
		free(tmp_host);

	/* get rid of the joined cusor */
	if (join_cursor != NULL)
		join_cursor->c_close(join_cursor);

	/* get rid of zone_cursor */
	if (zone_cursor != NULL)
		zone_cursor->c_close(zone_cursor);

	/* get rid of host_cursor */
	if (host_cursor != NULL)
		host_cursor->c_close(host_cursor);

	return result;
}


/*% Initializes, sets flags and then opens Berkeley databases. */

static isc_result_t
bdb_opendb(DB_ENV *db_env, DBTYPE db_type, DB **db, const char *db_name,
	   char *db_file, int flags) {

	int result;

	/* Initialize the database. */
	if ((result = db_create(db, db_env, 0)) != 0) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "BDB could not initialize %s database. "
			      "BDB error: %s",
			      db_name, db_strerror(result));
		return ISC_R_FAILURE;
	}

	/* set database flags. */
	if ((result = (*db)->set_flags(*db, flags)) != 0) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "BDB could not set flags for %s database. "
			      "BDB error: %s",
			      db_name, db_strerror(result));
		return ISC_R_FAILURE;
	}

	/* open the database. */
	if ((result = (*db)->open(*db, NULL, db_file, db_name, db_type,
				  DB_RDONLY | bdb_threads, 0)) != 0) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "BDB could not open %s database in %s. "
			      "BDB error: %s",
			      db_name, db_file, db_strerror(result));
		return ISC_R_FAILURE;
	}

	return ISC_R_SUCCESS;
}

static isc_result_t
bdb_create(const char *dlzname, unsigned int argc, char *argv[],
	   void *driverarg, void **dbdata)
{
	isc_result_t result;
	int bdbres;
	bdb_instance_t *db = NULL;

	UNUSED(dlzname);
	UNUSED(driverarg);

	/* verify we have 3 arg's passed to the driver */
	if (argc != 3) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "Berkeley DB driver requires at least "
			      "2 command line args.");
		return (ISC_R_FAILURE);
	}

	/* allocate and zero memory for driver structure */
	db = isc_mem_get(ns_g_mctx, sizeof(bdb_instance_t));
	if (db == NULL) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "Could not allocate memory for "
			      "database instance object.");
		return (ISC_R_NOMEMORY);
	}
	memset(db, 0, sizeof(bdb_instance_t));

	/* attach to the memory context */
	isc_mem_attach(ns_g_mctx, &db->mctx);

	/* create BDB environment
	 * Basically BDB allocates and assigns memory to db->dbenv
	 */
	bdbres = db_env_create(&db->dbenv, 0);
	if (bdbres != 0) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "BDB environment could not be created. "
			      "BDB error: %s",
			      db_strerror(bdbres));
		result = ISC_R_FAILURE;
		goto init_cleanup;
	}

	/* open BDB environment */
	bdbres = db->dbenv->open(db->dbenv, argv[1],
				 DB_INIT_CDB | DB_INIT_MPOOL |
				 bdb_threads | DB_CREATE,
				 0);
	if (bdbres != 0) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "BDB environment at '%s' could not be opened. "
			      "BDB error: %s",
			      argv[1], db_strerror(bdbres));
		result = ISC_R_FAILURE;
		goto init_cleanup;
	}

	/* open dlz_data database. */
	result = bdb_opendb(db->dbenv, DB_UNKNOWN, &db->data,
			    dlz_data, argv[2], 0);
	if (result != ISC_R_SUCCESS)
		goto init_cleanup;

	/* open dlz_host database. */
	result = bdb_opendb(db->dbenv, DB_UNKNOWN, &db->host,
			    dlz_host, argv[2],
			    DB_DUP | DB_DUPSORT);
	if (result != ISC_R_SUCCESS)
		goto init_cleanup;

	/* open dlz_zone database. */
	result = bdb_opendb(db->dbenv, DB_UNKNOWN, &db->zone,
			    dlz_zone, argv[2],
			    DB_DUP | DB_DUPSORT);
	if (result != ISC_R_SUCCESS)
		goto init_cleanup;

	/* open dlz_client database. */
	result = bdb_opendb(db->dbenv, DB_UNKNOWN, &db->client,
			    dlz_client, argv[2], DB_DUP | DB_DUPSORT);
	if (result != ISC_R_SUCCESS)
		goto init_cleanup;

	/* associate the host secondary database with the primary database */
	bdbres = db->data->associate(db->data, NULL, db->host, NULL, 0);
	if (bdbres != 0) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "BDB could not associate %s database with %s. "
			      "BDB error: %s",
			      dlz_host, dlz_data, db_strerror(bdbres));
		result = ISC_R_FAILURE;
		goto init_cleanup;
	}

	/* associate the zone secondary database with the primary database */
	bdbres = db->data->associate(db->data, NULL, db->zone, NULL, 0);
	if (bdbres != 0) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "BDB could not associate %s database with %s. "
			      "BDB error: %s",
			      dlz_zone, dlz_data, db_strerror(bdbres));
		result = ISC_R_FAILURE;
		goto init_cleanup;
	}

	*dbdata = db;

	return(ISC_R_SUCCESS);

 init_cleanup:

	bdb_cleanup(db);
	return result;
}

static void
bdb_destroy(void *driverarg, void *dbdata)
{
	UNUSED(driverarg);

	bdb_cleanup((bdb_instance_t *) dbdata);
}

/* bdb_authority not needed as authority data is returned by lookup */
static dns_sdlzmethods_t dlz_bdb_methods = {
	bdb_create,
	bdb_destroy,
	bdb_findzone,
	bdb_lookup,
	NULL,
	bdb_allnodes,
	bdb_allowzonexfr,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
};

/*%
 * Wrapper around dns_sdlzregister().
 */
isc_result_t
dlz_bdb_init(void) {
	isc_result_t result;

	/*
	 * Write debugging message to log
	 */
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_DLZ, ISC_LOG_DEBUG(2),
		      "Registering DLZ bdb driver.");

	result = dns_sdlzregister("bdb", &dlz_bdb_methods, NULL,
				  DNS_SDLZFLAG_RELATIVEOWNER |
				  DNS_SDLZFLAG_RELATIVERDATA |
				  DNS_SDLZFLAG_THREADSAFE,
				  ns_g_mctx, &dlz_bdb);
	if (result != ISC_R_SUCCESS) {
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "dns_sdlzregister() failed: %s",
				 isc_result_totext(result));
		result = ISC_R_UNEXPECTED;
	}


	return result;
}

/*%
 * Wrapper around dns_sdlzunregister().
 */
void
dlz_bdb_clear(void) {

	/*
	 * Write debugging message to log
	 */
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_DLZ, ISC_LOG_DEBUG(2),
		      "Unregistering DLZ bdb driver.");

	if (dlz_bdb != NULL)
		dns_sdlzunregister(&dlz_bdb);
}

#endif
