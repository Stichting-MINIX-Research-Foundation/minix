/*	$NetBSD: dlz_mysql_dynamic.c,v 1.1.1.3 2014/12/10 03:34:31 christos Exp $	*/

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
 * The development of Dynamically Loadable Zones (DLZ) for BIND 9 was
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
 * Copyright (C) 2013  Internet Systems Consortium.
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

/*
 * This provides the externally loadable MySQL DLZ module, without
 * update support
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

#include <dlz_minimal.h>
#include <dlz_list.h>
#include <dlz_dbi.h>
#include <dlz_pthread.h>

#include <mysql/mysql.h>

#define dbc_search_limit 30
#define ALLNODES 1
#define ALLOWXFR 2
#define AUTHORITY 3
#define FINDZONE 4
#define COUNTZONE 5
#define LOOKUP 6

#define safeGet(in) in == NULL ? "" : in

/*%
 * Structure to hold everthing needed by this "instance" of the MySQL
 * module remember, the module code is only loaded once, but may have
 * many separate instances.
 */
typedef struct {
#if PTHREADS
	db_list_t    *db; /*%< handle to a list of DB */
	int dbcount;
#else
	dbinstance_t *db; /*%< handle to DB */
#endif

	unsigned int flags;
	char *dbname;
	char *host;
	char *user;
	char *pass;
	char *socket;
	int port;

	/* Helper functions from the dlz_dlopen driver */
	log_t *log;
	dns_sdlz_putrr_t *putrr;
	dns_sdlz_putnamedrr_t *putnamedrr;
	dns_dlz_writeablezone_t *writeable_zone;
} mysql_instance_t;

/* forward references */
isc_result_t
dlz_findzonedb(void *dbdata, const char *name,
	       dns_clientinfomethods_t *methods,
	       dns_clientinfo_t *clientinfo);

void
dlz_destroy(void *dbdata);

static void
b9_add_helper(mysql_instance_t *db, const char *helper_name, void *ptr);

/*
 * Private methods
 */

void
mysql_destroy(dbinstance_t *db) {
	/* release DB connection */
	if (db->dbconn != NULL)
		mysql_close((MYSQL *) db->dbconn);

	/* destroy DB instance */
	destroy_dbinstance(db);
}

#if PTHREADS
/*%
 * Properly cleans up a list of database instances.
 * This function is only used when the module is compiled for
 * multithreaded operation.
 */
static void
mysql_destroy_dblist(db_list_t *dblist) {
	dbinstance_t *ndbi = NULL;
	dbinstance_t *dbi = NULL;

	ndbi = DLZ_LIST_HEAD(*dblist);
	while (ndbi != NULL) {
		dbi = ndbi;
		ndbi = DLZ_LIST_NEXT(dbi, link);

		mysql_destroy(dbi);
	}

	/* release memory for the list structure */
	free(dblist);
}

/*%
 * Loops through the list of DB instances, attempting to lock
 * on the mutex.  If successful, the DBI is reserved for use
 * and the thread can perform queries against the database.
 * If the lock fails, the next one in the list is tried.
 * looping continues until a lock is obtained, or until
 * the list has been searched dbc_search_limit times.
 * This function is only used when the module is compiled for
 * multithreaded operation.
 */
static dbinstance_t *
mysql_find_avail_conn(mysql_instance_t *mysql) {
	dbinstance_t *dbi = NULL, *head;
	int count = 0;

	/* get top of list */
	head = dbi = DLZ_LIST_HEAD(*(mysql->db));

	/* loop through list */
	while (count < dbc_search_limit) {
		/* try to lock on the mutex */
		if (dlz_mutex_trylock(&dbi->lock) == 0)
			return (dbi); /* success, return the DBI for use. */

		/* not successful, keep trying */
		dbi = DLZ_LIST_NEXT(dbi, link);

		/* check to see if we have gone to the top of the list. */
		if (dbi == NULL) {
			count++;
			dbi = head;
		}
	}

	mysql->log(ISC_LOG_INFO,
		   "MySQL module unable to find available connection "
		   "after searching %d times", count);
	return (NULL);
}
#endif /* PTHREADS */

/*%
 * Allocates memory for a new string, and then constructs the new
 * string by "escaping" the input string.  The new string is
 * safe to be used in queries.  This is necessary because we cannot
 * be sure of what types of strings are passed to us, and we don't
 * want special characters in the string causing problems.
 */
static char *
mysqldrv_escape_string(MYSQL *mysql, const char *instr) {

	char *outstr;
	unsigned int len;

	if (instr == NULL)
		return (NULL);

	len = strlen(instr);
	outstr = malloc((2 * len * sizeof(char)) + 1);
	if (outstr == NULL)
		return (NULL);

	mysql_real_escape_string(mysql, outstr, instr, len);

	return (outstr);
}

/*%
 * This function is the real core of the module.   Zone, record
 * and client strings are passed in (or NULL is passed if the
 * string is not available).  The type of query we want to run
 * is indicated by the query flag, and the dbdata object is passed
 * passed in to.  dbdata really holds a single database instance.
 * The function will construct and run the query, hopefully getting
 * a result set.
 */
static isc_result_t
mysql_get_resultset(const char *zone, const char *record,
		    const char *client, unsigned int query,
		    void *dbdata, MYSQL_RES **rs)
{
	isc_result_t result;
	dbinstance_t *dbi = NULL;
	mysql_instance_t *db = (mysql_instance_t *)dbdata;
	char *querystring = NULL;
	unsigned int i = 0;
	unsigned int j = 0;
	int qres = 0;

#if PTHREADS
	/* find an available DBI from the list */
	dbi = mysql_find_avail_conn(db);
#else /* PTHREADS */
	/*
	 * only 1 DBI - no need to lock instance lock either
	 * only 1 thread in the whole process, no possible contention.
	 */
	dbi = (dbinstance_t *)(db->db);
#endif /* PTHREADS */

	if (dbi == NULL) {
		result = ISC_R_FAILURE;
		goto cleanup;
	}

	/* what type of query are we going to run? */
	switch(query) {
	case ALLNODES:
		if (dbi->allnodes_q == NULL) {
			result = ISC_R_NOTIMPLEMENTED;
			goto cleanup;
		}
		break;
	case ALLOWXFR:
		if (dbi->allowxfr_q == NULL) {
			result = ISC_R_NOTIMPLEMENTED;
			goto cleanup;
		}
		break;
	case AUTHORITY:
		if (dbi->authority_q == NULL) {
			result = ISC_R_NOTIMPLEMENTED;
			goto cleanup;
		}
		break;
	case FINDZONE:
		if (dbi->findzone_q == NULL) {
			db->log(ISC_LOG_DEBUG(2),
				"No query specified for findzone.  "
				"Findzone requires a query");
			result = ISC_R_FAILURE;
			goto cleanup;
		}
		break;
	case COUNTZONE:
		if (dbi->countzone_q == NULL) {
			result = ISC_R_NOTIMPLEMENTED;
			goto cleanup;
		}
		break;
	case LOOKUP:
		if (dbi->lookup_q == NULL) {
			db->log(ISC_LOG_DEBUG(2),
				"No query specified for lookup.  "
				"Lookup requires a query");
			result = ISC_R_FAILURE;
			goto cleanup;
		}
		break;
	default:
		db->log(ISC_LOG_ERROR,
			"Incorrect query flag passed to "
			"mysql_get_resultset");
		result = ISC_R_UNEXPECTED;
		goto cleanup;
	}


	if (zone != NULL) {
		if (dbi->zone != NULL)
			free(dbi->zone);

		dbi->zone = mysqldrv_escape_string((MYSQL *) dbi->dbconn,
						   zone);
		if (dbi->zone == NULL) {
			result = ISC_R_NOMEMORY;
			goto cleanup;
		}
	} else
		dbi->zone = NULL;

	if (record != NULL) {
		if (dbi->record != NULL)
			free(dbi->record);

		dbi->record = mysqldrv_escape_string((MYSQL *) dbi->dbconn,
						     record);
		if (dbi->record == NULL) {
			result = ISC_R_NOMEMORY;
			goto cleanup;
		}
	} else
		dbi->record = NULL;

	if (client != NULL) {
		if (dbi->client != NULL)
			free(dbi->client);

		dbi->client = mysqldrv_escape_string((MYSQL *) dbi->dbconn,
						     client);
		if (dbi->client == NULL) {
			result = ISC_R_NOMEMORY;
			goto cleanup;
		}
	} else
		dbi->client = NULL;

	/*
	 * what type of query are we going to run?  this time we build
	 * the actual query to run.
	 */
	switch(query) {
	case ALLNODES:
		querystring = build_querystring(dbi->allnodes_q);
		break;
	case ALLOWXFR:
		querystring = build_querystring(dbi->allowxfr_q);
		break;
	case AUTHORITY:
		querystring = build_querystring(dbi->authority_q);
		break;
	case FINDZONE:
		querystring = build_querystring(dbi->findzone_q);
		break;
	case COUNTZONE:
		querystring = build_querystring(dbi->countzone_q);
		break;
	case LOOKUP:
		querystring = build_querystring(dbi->lookup_q);
		break;
	default:
		db->log(ISC_LOG_ERROR,
			"Incorrect query flag passed to "
			"mysql_get_resultset");
		result = ISC_R_UNEXPECTED; goto cleanup;
	}

	if (querystring == NULL) {
		result = ISC_R_NOMEMORY;
		goto cleanup;
	}

	/* output the full query string when debugging */
	db->log(ISC_LOG_DEBUG(1), "\nQuery String: %s\n", querystring);

	/* attempt query up to 3 times. */
	for (i = 0; i < 3; i++) {
		qres = mysql_query((MYSQL *) dbi->dbconn, querystring);
		if (qres == 0)
			break;
		for (j = 0; j < 4; j++)
		     if (mysql_ping((MYSQL *) dbi->dbconn) == 0)
			     break;
	}

	if (qres == 0) {
		result = ISC_R_SUCCESS;
		if (query != COUNTZONE) {
			*rs = mysql_store_result((MYSQL *) dbi->dbconn);
			if (*rs == NULL)
				result = ISC_R_FAILURE;
		}
	} else
		result = ISC_R_FAILURE;

 cleanup:
	if (dbi == NULL)
		return (ISC_R_FAILURE);

	if (dbi->zone != NULL) {
		free(dbi->zone);
		dbi->zone = NULL;
	}
	if (dbi->record != NULL) {
		free(dbi->record);
		dbi->record = NULL;
	}
	if (dbi->client != NULL) {
		free(dbi->client);
		dbi->client = NULL;
	}

	/* release the lock so another thread can use this dbi */
	(void) dlz_mutex_unlock(&dbi->lock);

	if (querystring != NULL)
		free(querystring);

	return (result);
}

/*%
 * The processing of result sets for lookup and authority are
 * exactly the same.  So that functionality has been moved
 * into this function to minimize code.
 */
static isc_result_t
mysql_process_rs(mysql_instance_t *db, dns_sdlzlookup_t *lookup,
		 MYSQL_RES *rs)
{
	isc_result_t result = ISC_R_NOTFOUND;
	MYSQL_ROW row;
	unsigned int fields;
	unsigned int j;
	char *tmpString;
	char *endp;
	int ttl;

	fields = mysql_num_fields(rs);	/* how many columns in result set */
	row = mysql_fetch_row(rs);	/* get a row from the result set */
	while (row != NULL) {
		unsigned int len = 0;

		switch(fields) {
		case 1:
			/*
			 * one column in rs, it's the data field.  use
			 * default type of A record, and default TTL
			 * of 86400
			 */
			result = db->putrr(lookup, "a", 86400, safeGet(row[0]));
			break;
		case 2:
			/*
			 * two columns, data field, and data type.
			 * use default TTL of 86400.
			 */
			result = db->putrr(lookup, safeGet(row[0]), 86400,
					   safeGet(row[1]));
			break;
		case 3:
			/*
			 * three columns, all data no defaults.
			 * convert text to int, make sure it worked
			 * right.
			 */
			ttl = strtol(safeGet(row[0]), &endp, 10);
			if (*endp != '\0' || ttl < 0) {
				db->log(ISC_LOG_ERROR,
					"MySQL module ttl must be "
					"a postive number");
				return (ISC_R_FAILURE);
			}

			result = db->putrr(lookup, safeGet(row[1]), ttl,
						safeGet(row[2]));
			break;
		default:
			/*
			 * more than 3 fields, concatenate the last
			 * ones together.  figure out how long to make
			 * string.
			 */
			for (j = 2; j < fields; j++)
				len += strlen(safeGet(row[j])) + 1;

			/*
			 * allocate string memory, allow for NULL to
			 * term string
			 */
			tmpString = malloc(len + 1);
			if (tmpString == NULL) {
				db->log(ISC_LOG_ERROR,
					"MySQL module unable to allocate "
					"memory for temporary string");
				mysql_free_result(rs);
				return (ISC_R_FAILURE);
			}

			strcpy(tmpString, safeGet(row[2]));
			for (j = 3; j < fields; j++) {
				strcat(tmpString, " ");
				strcat(tmpString, safeGet(row[j]));
			}

			ttl = strtol(safeGet(row[0]), &endp, 10);
			if (*endp != '\0' || ttl < 0) {
				db->log(ISC_LOG_ERROR,
					"MySQL module ttl must be "
					"a postive number");
				return (ISC_R_FAILURE);
			}

			result = db->putrr(lookup, safeGet(row[1]),
					   ttl, tmpString);
			free(tmpString);
		}

		if (result != ISC_R_SUCCESS) {
			mysql_free_result(rs);
			db->log(ISC_LOG_ERROR,
				"putrr returned error: %d", result);
			return (ISC_R_FAILURE);
		}

		row = mysql_fetch_row(rs);
	}

	mysql_free_result(rs);
	return (result);
}

/*
 * DLZ methods
 */

/*% determine if the zone is supported by (in) the database */
isc_result_t
dlz_findzonedb(void *dbdata, const char *name,
	       dns_clientinfomethods_t *methods,
	       dns_clientinfo_t *clientinfo)
{
	isc_result_t result;
	MYSQL_RES *rs = NULL;
	my_ulonglong rows;
	mysql_instance_t *db = (mysql_instance_t *)dbdata;

	UNUSED(methods);
	UNUSED(clientinfo);

	result = mysql_get_resultset(name, NULL, NULL, FINDZONE, dbdata, &rs);
	if (result != ISC_R_SUCCESS || rs == NULL) {
		if (rs != NULL)
			mysql_free_result(rs);

		db->log(ISC_LOG_ERROR,
			"MySQL module unable to return "
			"result set for findzone query");

		return (ISC_R_FAILURE);
	}

	/*
	 * if we returned any rows, the zone is supported.
	 */
	rows = mysql_num_rows(rs);
	mysql_free_result(rs);
	if (rows > 0) {
		mysql_get_resultset(name, NULL, NULL, COUNTZONE, dbdata, NULL);
		return (ISC_R_SUCCESS);
	}

	return (ISC_R_NOTFOUND);
}

/*% Determine if the client is allowed to perform a zone transfer */
isc_result_t
dlz_allowzonexfr(void *dbdata, const char *name, const char *client) {
	isc_result_t result;
	mysql_instance_t *db = (mysql_instance_t *)dbdata;
	MYSQL_RES *rs = NULL;
	my_ulonglong rows;

	/* first check if the zone is supported by the database. */
	result = dlz_findzonedb(dbdata, name, NULL, NULL);
	if (result != ISC_R_SUCCESS)
		return (ISC_R_NOTFOUND);

	/*
	 * if we get to this point we know the zone is supported by
	 * the database the only questions now are is the zone
	 * transfer is allowed for this client and did the config file
	 * have an allow zone xfr query.
	 */
	result = mysql_get_resultset(name, NULL, client, ALLOWXFR,
				     dbdata, &rs);
	if (result == ISC_R_NOTIMPLEMENTED)
		return (result);

	if (result != ISC_R_SUCCESS || rs == NULL) {
		if (rs != NULL)
			mysql_free_result(rs);
		db->log(ISC_LOG_ERROR,
			"MySQL module unable to return "
			"result set for allow xfr query");
		return (ISC_R_FAILURE);
	}

	/*
	 * count how many rows in result set; if we returned any,
	 * zone xfr is allowed.
	 */
	rows = mysql_num_rows(rs);
	mysql_free_result(rs);
	if (rows > 0)
		return (ISC_R_SUCCESS);

	return (ISC_R_NOPERM);
}

/*%
 * If the client is allowed to perform a zone transfer, the next order of
 * business is to get all the nodes in the zone, so bind can respond to the
 * query.
 */
isc_result_t
dlz_allnodes(const char *zone, void *dbdata, dns_sdlzallnodes_t *allnodes) {
	isc_result_t result;
	mysql_instance_t *db = (mysql_instance_t *)dbdata;
	MYSQL_RES *rs = NULL;
	MYSQL_ROW row;
	unsigned int fields;
	unsigned int j;
	char *tmpString;
	char *endp;
	int ttl;

	result = mysql_get_resultset(zone, NULL, NULL, ALLNODES, dbdata, &rs);
	if (result == ISC_R_NOTIMPLEMENTED)
		return (result);

	/* if we didn't get a result set, log an err msg. */
	if (result != ISC_R_SUCCESS) {
		db->log(ISC_LOG_ERROR,
			"MySQL module unable to return "
			"result set for all nodes query");
		goto cleanup;
	}

	result = ISC_R_NOTFOUND;

	fields = mysql_num_fields(rs);	/* how many columns in result set */
	row = mysql_fetch_row(rs);	/* get a row from the result set */
	while (row != NULL) {
		if (fields < 4) {
			db->log(ISC_LOG_ERROR,
				"MySQL module too few fields returned "
				"by all nodes query");
			result = ISC_R_FAILURE;
			goto cleanup;
		}

		ttl = strtol(safeGet(row[0]), &endp, 10);
		if (*endp != '\0' || ttl < 0) {
			db->log(ISC_LOG_ERROR,
				"MySQL module ttl must be "
				"a postive number");
			result = ISC_R_FAILURE;
			goto cleanup;
		}

		if (fields == 4) {
			result = db->putnamedrr(allnodes, safeGet(row[2]),
						safeGet(row[1]), ttl,
						safeGet(row[3]));
		} else {
			unsigned int len = 0;

			/*
			 * more than 4 fields, concatenate the last
			 * ones together.
			 */
			for (j = 3; j < fields; j++)
				len += strlen(safeGet(row[j])) + 1;

			tmpString = malloc(len + 1);
			if (tmpString == NULL) {
				db->log(ISC_LOG_ERROR,
					"MySQL module unable to allocate "
					"memory for temporary string");
				result = ISC_R_FAILURE;
				goto cleanup;
			}

			strcpy(tmpString, safeGet(row[3]));
			for (j = 4; j < fields; j++) {
				strcat(tmpString, " ");
				strcat(tmpString, safeGet(row[j]));
			}

			result = db->putnamedrr(allnodes, safeGet(row[2]),
						safeGet(row[1]),
						ttl, tmpString);
			free(tmpString);
		}

		if (result != ISC_R_SUCCESS) {
			db->log(ISC_LOG_ERROR,
				"putnamedrr returned error: %s", result);
			result = ISC_R_FAILURE;
			break;
		}

		row = mysql_fetch_row(rs);
	}

 cleanup:
	if (rs != NULL)
		mysql_free_result(rs);

	return (result);
}

/*%
 * If the lookup function does not return SOA or NS records for the zone,
 * use this function to get that information for named.
 */
isc_result_t
dlz_authority(const char *zone, void *dbdata, dns_sdlzlookup_t *lookup) {
	isc_result_t result;
	MYSQL_RES *rs = NULL;
	mysql_instance_t *db = (mysql_instance_t *)dbdata;

	result = mysql_get_resultset(zone, NULL, NULL, AUTHORITY, dbdata, &rs);
	if (result == ISC_R_NOTIMPLEMENTED)
		return (result);

	if (result != ISC_R_SUCCESS) {
		if (rs != NULL)
			mysql_free_result(rs);
		db->log(ISC_LOG_ERROR,
			"MySQL module unable to return "
			"result set for authority query");
		return (ISC_R_FAILURE);
	}

	/*
	 * lookup and authority result sets are processed in the same
	 * manner: mysql_process_rs does the job for both functions.
	 */
	return (mysql_process_rs(db, lookup, rs));
}

/*% If zone is supported, lookup up a (or multiple) record(s) in it */
isc_result_t
dlz_lookup(const char *zone, const char *name,
	   void *dbdata, dns_sdlzlookup_t *lookup,
	   dns_clientinfomethods_t *methods,
	   dns_clientinfo_t *clientinfo)
{
	isc_result_t result;
	MYSQL_RES *rs = NULL;
	mysql_instance_t *db = (mysql_instance_t *)dbdata;

	UNUSED(methods);
	UNUSED(clientinfo);

	result = mysql_get_resultset(zone, name, NULL, LOOKUP, dbdata, &rs);

	/* if we didn't get a result set, log an err msg. */
	if (result != ISC_R_SUCCESS) {
		if (rs != NULL)
			mysql_free_result(rs);
		db->log(ISC_LOG_ERROR,
			"MySQL module unable to return "
			"result set for lookup query");
		return (ISC_R_FAILURE);
	}

	/*
	 * lookup and authority result sets are processed in the same
	 * manner: mysql_process_rs does the job for both functions.
	 */
	return (mysql_process_rs(db, lookup, rs));
}

/*%
 * Create an instance of the module.
 */
isc_result_t
dlz_create(const char *dlzname, unsigned int argc, char *argv[],
	   void **dbdata, ...)
{
	isc_result_t result = ISC_R_FAILURE;
	mysql_instance_t *mysql = NULL;
	dbinstance_t *dbi = NULL;
	MYSQL *dbc;
	char *tmp = NULL;
	char *endp;
	int j;
	const char *helper_name;
#if MYSQL_VERSION_ID >= 50000
        my_bool auto_reconnect = 1;
#endif
#if PTHREADS
	int dbcount;
	int i;
#endif /* PTHREADS */
	va_list ap;

	UNUSED(dlzname);

	/* allocate memory for MySQL instance */
	mysql = calloc(1, sizeof(mysql_instance_t));
	if (mysql == NULL)
		return (ISC_R_NOMEMORY);
	memset(mysql, 0, sizeof(mysql_instance_t));

	/* Fill in the helper functions */
	va_start(ap, dbdata);
	while ((helper_name = va_arg(ap, const char*)) != NULL)
		b9_add_helper(mysql, helper_name, va_arg(ap, void*));
	va_end(ap);

#if PTHREADS
	/* if debugging, let user know we are multithreaded. */
	mysql->log(ISC_LOG_DEBUG(1), "MySQL module running multithreaded");
#else /* PTHREADS */
	/* if debugging, let user know we are single threaded. */
	mysql->log(ISC_LOG_DEBUG(1), "MySQL module running single threaded");
#endif /* PTHREADS */

	/* verify we have at least 4 arg's passed to the module */
	if (argc < 4) {
		mysql->log(ISC_LOG_ERROR,
			   "MySQL module requires "
			   "at least 4 command line args.");
		return (ISC_R_FAILURE);
	}

	/* no more than 8 arg's should be passed to the module */
	if (argc > 8) {
		mysql->log(ISC_LOG_ERROR,
			   "MySQL module cannot accept "
			   "more than 7 command line args.");
		return (ISC_R_FAILURE);
	}

	/* get db name - required */
	mysql->dbname = get_parameter_value(argv[1], "dbname=");
	if (mysql->dbname == NULL) {
		mysql->log(ISC_LOG_ERROR,
			   "MySQL module requires a dbname parameter.");
		result = ISC_R_FAILURE;
		goto cleanup;
	}

	/* get db port.  Not required, but must be > 0 if specified */
	tmp = get_parameter_value(argv[1], "port=");
	if (tmp == NULL)
		mysql->port = 0;
	else {
		mysql->port = strtol(tmp, &endp, 10);
		if (*endp != '\0' || mysql->port < 0) {
			mysql->log(ISC_LOG_ERROR,
				   "Mysql module: port "
				   "must be a positive number.");
			free(tmp);
			result = ISC_R_FAILURE;
			goto cleanup;
		}
		free(tmp);
	}

	mysql->host = get_parameter_value(argv[1], "host=");
	mysql->user = get_parameter_value(argv[1], "user=");
	mysql->pass = get_parameter_value(argv[1], "pass=");
	mysql->socket = get_parameter_value(argv[1], "socket=");

	mysql->flags = CLIENT_REMEMBER_OPTIONS;

	tmp = get_parameter_value(argv[1], "compress=");
	if (tmp != NULL) {
		if (strcasecmp(tmp, "true") == 0)
			mysql->flags |= CLIENT_COMPRESS;
		free(tmp);
	}

	tmp = get_parameter_value(argv[1], "ssl=");
	if (tmp != NULL) {
		if (strcasecmp(tmp, "true") == 0)
			mysql->flags |= CLIENT_SSL;
		free(tmp);
	}

	tmp = get_parameter_value(argv[1], "space=");
	if (tmp != NULL) {
		if (strcasecmp(tmp, "ignore") == 0)
			mysql->flags |= CLIENT_IGNORE_SPACE;
		free(tmp);
	}

#if PTHREADS
	/* multithreaded build can have multiple DB connections */
	tmp = get_parameter_value(argv[1], "threads=");
	if (tmp == NULL)
		dbcount = 1;
	else {
		dbcount = strtol(tmp, &endp, 10);
		if (*endp != '\0' || dbcount < 1) {
			mysql->log(ISC_LOG_ERROR,
				   "MySQL database connection count "
				   "must be positive.");
			free(tmp);
			result = ISC_R_FAILURE;
			goto cleanup;
		}
		free(tmp);
	}

	/* allocate memory for database connection list */
	mysql->db = calloc(1, sizeof(db_list_t));
	if (mysql->db == NULL) {
		result = ISC_R_NOMEMORY;
		goto cleanup;
	}

	/* initialize DB connection list */
	DLZ_LIST_INIT(*(mysql->db));

	/*
	 * create the appropriate number of database instances (DBI)
	 * append each new DBI to the end of the list
	 */
	for (i = 0; i < dbcount; i++) {
#endif /* PTHREADS */
		switch(argc) {
		case 4:
			result = build_dbinstance(NULL, NULL, NULL,
						  argv[2], argv[3], NULL,
						  &dbi, mysql->log);
			break;
		case 5:
			result = build_dbinstance(NULL, NULL, argv[4],
						  argv[2], argv[3], NULL,
						  &dbi, mysql->log);
			break;
		case 6:
			result = build_dbinstance(argv[5], NULL, argv[4],
						  argv[2], argv[3], NULL,
						  &dbi, mysql->log);
			break;
		case 7:
			result = build_dbinstance(argv[5], argv[6], argv[4],
						  argv[2], argv[3], NULL,
						  &dbi, mysql->log);
			break;
		case 8:
			result = build_dbinstance(argv[5], argv[6], argv[4],
						  argv[2], argv[3], argv[7],
						  &dbi, mysql->log);
			break;
		default:
			result = ISC_R_FAILURE;
		}


		if (result != ISC_R_SUCCESS) {
			mysql->log(ISC_LOG_ERROR,
				   "MySQL module could not create "
				   "database instance object.");
			result = ISC_R_FAILURE;
			goto cleanup;
		}

#if PTHREADS
		/* when multithreaded, build a list of DBI's */
		DLZ_LINK_INIT(dbi, link);
		DLZ_LIST_APPEND(*(mysql->db), dbi, link);
#else
		/*
		 * when single threaded, hold onto the one connection
		 * instance.
		 */
		mysql->db = dbi;
#endif

		/* create and set db connection */
		dbi->dbconn = mysql_init(NULL);
		if (dbi->dbconn == NULL) {
			mysql->log(ISC_LOG_ERROR,
				   "MySQL module could not allocate "
				   "memory for database connection");
			result = ISC_R_FAILURE;
			goto cleanup;
		}

		dbc = NULL;

#if MYSQL_VERSION_ID >= 50000
		/* enable automatic reconnection. */
		if (mysql_options((MYSQL *) dbi->dbconn, MYSQL_OPT_RECONNECT,
				  &auto_reconnect) != 0) {
			mysql->log(ISC_LOG_WARNING,
				   "MySQL module failed to set "
				   "MYSQL_OPT_RECONNECT option, continuing");
		}
#endif

		for (j = 0; dbc == NULL && j < 4; j++) {
			dbc = mysql_real_connect((MYSQL *) dbi->dbconn,
						 mysql->host, mysql->user,
						 mysql->pass, mysql->dbname,
						 mysql->port, mysql->socket,
						 mysql->flags);
			if (dbc == NULL)
				mysql->log(ISC_LOG_ERROR,
					   "MySQL connection failed: %s",
					   mysql_error((MYSQL *) dbi->dbconn));
		}

		if (dbc == NULL) {
			mysql->log(ISC_LOG_ERROR,
				   "MySQL module failed to create "
				   "database connection after 4 attempts");
			result = ISC_R_FAILURE;
			goto cleanup;
		}

#if PTHREADS
		/* set DBI = null for next loop through. */
		dbi = NULL;
	}
#endif /* PTHREADS */

	*dbdata = mysql;

	return (ISC_R_SUCCESS);

 cleanup:
	dlz_destroy(mysql);

	return (result);
}

/*%
 * Destroy the module.
 */
void
dlz_destroy(void *dbdata) {
	mysql_instance_t *db = (mysql_instance_t *)dbdata;
#if PTHREADS
	/* cleanup the list of DBI's */
	if (db->db != NULL)
		mysql_destroy_dblist((db_list_t *)(db->db));
#else /* PTHREADS */
	mysql_destroy(db);
#endif /* PTHREADS */

	if (db->dbname != NULL)
		free(db->dbname);
	if (db->host != NULL)
		free(db->host);
	if (db->user != NULL)
		free(db->user);
	if (db->pass != NULL)
		free(db->pass);
	if (db->socket != NULL)
		free(db->socket);
}

/*
 * Return the version of the API
 */
int
dlz_version(unsigned int *flags) {
	*flags |= (DNS_SDLZFLAG_RELATIVEOWNER |
		   DNS_SDLZFLAG_RELATIVERDATA |
		   DNS_SDLZFLAG_THREADSAFE);
	return (DLZ_DLOPEN_VERSION);
}

/*
 * Register a helper function from the bind9 dlz_dlopen driver
 */
static void
b9_add_helper(mysql_instance_t *db, const char *helper_name, void *ptr) {
	if (strcmp(helper_name, "log") == 0)
		db->log = (log_t *)ptr;
	if (strcmp(helper_name, "putrr") == 0)
		db->putrr = (dns_sdlz_putrr_t *)ptr;
	if (strcmp(helper_name, "putnamedrr") == 0)
		db->putnamedrr = (dns_sdlz_putnamedrr_t *)ptr;
	if (strcmp(helper_name, "writeable_zone") == 0)
		db->writeable_zone = (dns_dlz_writeablezone_t *)ptr;
}
