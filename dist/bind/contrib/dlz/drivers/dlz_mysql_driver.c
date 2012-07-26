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

#ifdef DLZ_MYSQL

#include <config.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <dns/log.h>
#include <dns/sdlz.h>
#include <dns/result.h>

#include <isc/mem.h>
#include <isc/platform.h>
#include <isc/print.h>
#include <isc/result.h>
#include <isc/string.h>
#include <isc/util.h>

#include <named/globals.h>

#include <dlz/sdlz_helper.h>
#include <dlz/dlz_mysql_driver.h>

#include <mysql.h>

static dns_sdlzimplementation_t *dlz_mysql = NULL;

#define dbc_search_limit 30
#define ALLNODES 1
#define ALLOWXFR 2
#define AUTHORITY 3
#define FINDZONE 4
#define COUNTZONE 5
#define LOOKUP 6

#define safeGet(in) in == NULL ? "" : in

/*
 * Private methods
 */

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
		return NULL;

	len = strlen(instr);

	outstr = isc_mem_allocate(ns_g_mctx ,(2 * len * sizeof(char)) + 1);
	if (outstr == NULL)
		return NULL;

	mysql_real_escape_string(mysql, outstr, instr, len);

	return outstr;
}

/*%
 * This function is the real core of the driver.   Zone, record
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
	char *querystring = NULL;
	unsigned int i = 0;
	unsigned int j = 0;
	int qres = 0;

	if (query != COUNTZONE)
		REQUIRE(*rs == NULL);
	else
		REQUIRE(rs == NULL);

	/* get db instance / connection */
	dbi =  (dbinstance_t *) dbdata;

	/* if DBI is null, can't do anything else */
	if (dbi == NULL) {
		result = ISC_R_FAILURE;
		goto cleanup;
	}

	/* what type of query are we going to run? */
	switch(query) {
	case ALLNODES:
		/*
		 * if the query was not passed in from the config file
		 * then we can't run it.  return not_implemented, so
		 * it's like the code for that operation was never
		 * built into the driver.... AHHH flexibility!!!
		 */
		if (dbi->allnodes_q == NULL) {
			result = ISC_R_NOTIMPLEMENTED;
			goto cleanup;
		}
		break;
	case ALLOWXFR:
		/* same as comments as ALLNODES */
		if (dbi->allowxfr_q == NULL) {
			result = ISC_R_NOTIMPLEMENTED;
			goto cleanup;
		}
		break;
	case AUTHORITY:
		/* same as comments as ALLNODES */
		if (dbi->authority_q == NULL) {
			result = ISC_R_NOTIMPLEMENTED;
			goto cleanup;
		}
		break;
	case FINDZONE:
		/* this is required.  It's the whole point of DLZ! */
		if (dbi->findzone_q == NULL) {
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
				      DNS_LOGMODULE_DLZ, ISC_LOG_DEBUG(2),
				      "No query specified for findzone.  "
				      "Findzone requires a query");
			result = ISC_R_FAILURE;
			goto cleanup;
		}
		break;
	case COUNTZONE:
		/* same as comments as ALLNODES */
		if (dbi->countzone_q == NULL) {
			result = ISC_R_NOTIMPLEMENTED;
			goto cleanup;
		}
		break;
	case LOOKUP:
		/* this is required.  It's also a major point of DLZ! */
		if (dbi->lookup_q == NULL) {
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
				      DNS_LOGMODULE_DLZ, ISC_LOG_DEBUG(2),
				      "No query specified for lookup.  "
				      "Lookup requires a query");
			result = ISC_R_FAILURE;
			goto cleanup;
		}
		break;
	default:
		/*
		 * this should never happen.  If it does, the code is
		 * screwed up!
		 */
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "Incorrect query flag passed to "
				 "mysql_get_resultset");
		result = ISC_R_UNEXPECTED;
		goto cleanup;
	}


	/*
	 * was a zone string passed?  If so, make it safe for use in
	 * queries.
	 */
	if (zone != NULL) {
		dbi->zone = mysqldrv_escape_string((MYSQL *) dbi->dbconn,
						   zone);
		if (dbi->zone == NULL) {
			result = ISC_R_NOMEMORY;
			goto cleanup;
		}
	} else {	/* no string passed, set the string pointer to NULL */
		dbi->zone = NULL;
	}

	/*
	 * was a record string passed?  If so, make it safe for use in
	 * queries.
	 */
	if (record != NULL) {
		dbi->record = mysqldrv_escape_string((MYSQL *) dbi->dbconn,
						     record);
		if (dbi->record == NULL) {
			result = ISC_R_NOMEMORY;
			goto cleanup;
		}
	} else {	/* no string passed, set the string pointer to NULL */
		dbi->record = NULL;
	}

	/*
	 * was a client string passed?  If so, make it safe for use in
	 * queries.
	 */
	if (client != NULL) {
		dbi->client = mysqldrv_escape_string((MYSQL *) dbi->dbconn,
						     client);
		if (dbi->client == NULL) {
			result = ISC_R_NOMEMORY;
			goto cleanup;
		}
	} else {	/* no string passed, set the string pointer to NULL */
		dbi->client = NULL;
	}

	/*
	 * what type of query are we going to run?  this time we build
	 * the actual query to run.
	 */
	switch(query) {
	case ALLNODES:
		querystring = build_querystring(ns_g_mctx, dbi->allnodes_q);
		break;
	case ALLOWXFR:
		querystring = build_querystring(ns_g_mctx, dbi->allowxfr_q);
		break;
	case AUTHORITY:
		querystring = build_querystring(ns_g_mctx, dbi->authority_q);
		break;
	case FINDZONE:
		querystring = build_querystring(ns_g_mctx, dbi->findzone_q);
		break;
	case COUNTZONE:
		querystring = build_querystring(ns_g_mctx, dbi->countzone_q);
		break;
	case LOOKUP:
		querystring = build_querystring(ns_g_mctx, dbi->lookup_q);
		break;
	default:
		/*
		 * this should never happen.  If it does, the code is
		 * screwed up!
		 */
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "Incorrect query flag passed to "
				 "mysql_get_resultset");
		result = ISC_R_UNEXPECTED;
		goto cleanup;
	}

	/* if the querystring is null, Bummer, outta RAM.  UPGRADE TIME!!!   */
	if (querystring == NULL) {
		result = ISC_R_NOMEMORY;
		goto cleanup;
	}

	/*
	 * output the full query string during debug so we can see
	 * what lame error the query has.
	 */
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_DLZ, ISC_LOG_DEBUG(1),
		      "\nQuery String: %s\n", querystring);

	/* attempt query up to 3 times. */
	for (i=0; i < 3; i++) {
		qres = mysql_query((MYSQL *) dbi->dbconn, querystring);
		if (qres == 0)
			break;
		for (j=0; mysql_ping((MYSQL *) dbi->dbconn) != 0 && j < 4; j++)
			;
	}

	if (qres == 0) {
		result = ISC_R_SUCCESS;
		if (query != COUNTZONE) {
			*rs = mysql_store_result((MYSQL *) dbi->dbconn);
			if (*rs == NULL)
				result = ISC_R_FAILURE;
		}
	} else {
		result = ISC_R_FAILURE;
	}


 cleanup:
	/* it's always good to cleanup after yourself */

	/* if we couldn't even get DBI, just return NULL */
	if (dbi == NULL)
		return ISC_R_FAILURE;

	/* free dbi->zone string */
	if (dbi->zone != NULL)
		isc_mem_free(ns_g_mctx, dbi->zone);

	/* free dbi->record string */
	if (dbi->record != NULL)
		isc_mem_free(ns_g_mctx, dbi->record);

	/* free dbi->client string */
	if (dbi->client != NULL)
		isc_mem_free(ns_g_mctx, dbi->client);

	/* release query string */
	if (querystring  != NULL)
		isc_mem_free(ns_g_mctx, querystring);

	/* return result */
	return result;
}

/*%
 * The processing of result sets for lookup and authority are
 * exactly the same.  So that functionality has been moved
 * into this function to minimize code.
 */

static isc_result_t
mysql_process_rs(dns_sdlzlookup_t *lookup, MYSQL_RES *rs)
{
	isc_result_t result = ISC_R_NOTFOUND;
	MYSQL_ROW row;
	unsigned int fields;
	unsigned int j;
	unsigned int len;
	char *tmpString;
	char *endp;
	int ttl;

	row = mysql_fetch_row(rs);	/* get a row from the result set */
	fields = mysql_num_fields(rs);	/* how many columns in result set */
	while (row != NULL) {
		switch(fields) {
		case 1:
			/*
			 * one column in rs, it's the data field.  use
			 * default type of A record, and default TTL
			 * of 86400
			 */
			result = dns_sdlz_putrr(lookup, "a", 86400,
						safeGet(row[0]));
			break;
		case 2:
			/*
			 * two columns, data field, and data type.
			 * use default TTL of 86400.
			 */
			result = dns_sdlz_putrr(lookup, safeGet(row[0]), 86400,
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
				isc_log_write(dns_lctx,
					      DNS_LOGCATEGORY_DATABASE,
					      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
					      "mysql driver ttl must be "
					      "a postive number");
			}
			result = dns_sdlz_putrr(lookup, safeGet(row[1]), ttl,
						safeGet(row[2]));
			break;
		default:
			/*
			 * more than 3 fields, concatenate the last
			 * ones together.  figure out how long to make
			 * string.
			 */
			for (j=2, len=0; j < fields; j++) {
				len += strlen(safeGet(row[j])) + 1;
			}
			/*
			 * allocate string memory, allow for NULL to
			 * term string
			 */
			tmpString = isc_mem_allocate(ns_g_mctx, len + 1);
			if (tmpString == NULL) {
				/* major bummer, need more ram */
				isc_log_write(dns_lctx,
					      DNS_LOGCATEGORY_DATABASE,
					      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
					      "mysql driver unable "
					      "to allocate memory for "
					      "temporary string");
				mysql_free_result(rs);
				return (ISC_R_FAILURE);	/* Yeah, I'd say! */
			}
			/* copy field to tmpString */
			strcpy(tmpString, safeGet(row[2]));


			/*
			 * concat the rest of fields together, space
			 * between each one.
			 */
			for (j=3; j < fields; j++) {
				strcat(tmpString, " ");
				strcat(tmpString, safeGet(row[j]));
			}
			/* convert text to int, make sure it worked right */
			ttl = strtol(safeGet(row[0]), &endp, 10);
			if (*endp != '\0' || ttl < 0) {
				isc_log_write(dns_lctx,
					      DNS_LOGCATEGORY_DATABASE,
					      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
					      "mysql driver ttl must be "
					      "a postive number");
			}
			/* ok, now tell Bind about it. */
			result = dns_sdlz_putrr(lookup, safeGet(row[1]),
						ttl, tmpString);
			/* done, get rid of this thing. */
			isc_mem_free(ns_g_mctx, tmpString);
		}
		/* I sure hope we were successful */
		if (result != ISC_R_SUCCESS) {
			/* nope, get rid of the Result set, and log a msg */
			mysql_free_result(rs);
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
				      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
				      "dns_sdlz_putrr returned error. "
				      "Error code was: %s",
				      isc_result_totext(result));
			return (ISC_R_FAILURE);
		}
		row = mysql_fetch_row(rs);	/* get next row */
	}

	/* free result set memory */
	mysql_free_result(rs);

	/* return result code */
	return result;
}

/*
 * SDLZ interface methods
 */

/*% determine if the zone is supported by (in) the database */

static isc_result_t
mysql_findzone(void *driverarg, void *dbdata, const char *name)
{
	isc_result_t result;
	MYSQL_RES *rs = NULL;
	my_ulonglong rows;

	UNUSED(driverarg);

	/* run the query and get the result set from the database. */
	result = mysql_get_resultset(name, NULL, NULL, FINDZONE, dbdata, &rs);
	/* if we didn't get a result set, log an err msg. */
	if (result != ISC_R_SUCCESS || rs == NULL) {
		if (rs != NULL)
			mysql_free_result(rs);
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "mysql driver unable to return "
			      "result set for findzone query");
		return (ISC_R_FAILURE);
	}
	/* count how many rows in result set */
	rows = mysql_num_rows(rs);
	/* get rid of result set, we are done with it. */
	mysql_free_result(rs);

	/* if we returned any rows, zone is supported. */
	if (rows > 0) {
		mysql_get_resultset(name, NULL, NULL, COUNTZONE, dbdata, NULL);
		return (ISC_R_SUCCESS);
	}

	/* no rows returned, zone is not supported. */
	return (ISC_R_NOTFOUND);
}

/*% Determine if the client is allowed to perform a zone transfer */
static isc_result_t
mysql_allowzonexfr(void *driverarg, void *dbdata, const char *name,
		   const char *client)
{
	isc_result_t result;
	MYSQL_RES *rs = NULL;
	my_ulonglong rows;

	UNUSED(driverarg);

	/* first check if the zone is supported by the database. */
	result = mysql_findzone(driverarg, dbdata, name);
	if (result != ISC_R_SUCCESS)
		return (ISC_R_NOTFOUND);

	/*
	 * if we get to this point we know the zone is supported by
	 * the database the only questions now are is the zone
	 * transfer is allowed for this client and did the config file
	 * have an allow zone xfr query.
	 *
	 * Run our query, and get a result set from the database.
	 */
	result = mysql_get_resultset(name, NULL, client, ALLOWXFR,
				     dbdata, &rs);
	/* if we get "not implemented", send it along. */
	if (result == ISC_R_NOTIMPLEMENTED)
		return result;
	/* if we didn't get a result set, log an err msg. */
	if (result != ISC_R_SUCCESS || rs == NULL) {
		if (rs != NULL)
			mysql_free_result(rs);
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "mysql driver unable to return "
			      "result set for allow xfr query");
		return (ISC_R_FAILURE);
	}
	/* count how many rows in result set */
	rows = mysql_num_rows(rs);
	/* get rid of result set, we are done with it. */
	mysql_free_result(rs);

	/* if we returned any rows, zone xfr is allowed. */
	if (rows > 0)
		return (ISC_R_SUCCESS);

	/* no rows returned, zone xfr not allowed */
	return (ISC_R_NOPERM);
}

/*%
 * If the client is allowed to perform a zone transfer, the next order of
 * business is to get all the nodes in the zone, so bind can respond to the
 * query.
 */
static isc_result_t
mysql_allnodes(const char *zone, void *driverarg, void *dbdata,
	       dns_sdlzallnodes_t *allnodes)
{
	isc_result_t result;
	MYSQL_RES *rs = NULL;
	MYSQL_ROW row;
	unsigned int fields;
	unsigned int j;
	unsigned int len;
	char *tmpString;
	char *endp;
	int ttl;

	UNUSED(driverarg);

	/* run the query and get the result set from the database. */
	result = mysql_get_resultset(zone, NULL, NULL, ALLNODES, dbdata, &rs);
	/* if we get "not implemented", send it along */
	if (result == ISC_R_NOTIMPLEMENTED)
		return result;
	/* if we didn't get a result set, log an err msg. */
	if (result != ISC_R_SUCCESS) {
		if (rs != NULL)
			mysql_free_result(rs);
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "mysql driver unable to return "
			      "result set for all nodes query");
		return (ISC_R_FAILURE);
	}

	result = ISC_R_NOTFOUND;

	row = mysql_fetch_row(rs);	/* get a row from the result set */
	fields = mysql_num_fields(rs);	/* how many columns in result set */
	while (row != NULL) {
		if (fields < 4) {	/* gotta have at least 4 columns */
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
				      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
				      "mysql driver too few fields returned "
				      "by all nodes query");
		}
		/* convert text to int, make sure it worked right  */
		ttl = strtol(safeGet(row[0]), &endp, 10);
		if (*endp != '\0' || ttl < 0) {
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
				      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
				      "mysql driver ttl must be "
				      "a postive number");
		}
		if (fields == 4) {
			/* tell Bind about it. */
			result = dns_sdlz_putnamedrr(allnodes, safeGet(row[2]),
						     safeGet(row[1]), ttl,
						     safeGet(row[3]));
		} else {
			/*
			 * more than 4 fields, concatenate the last
			 * ones together.  figure out how long to make
			 * string.
			 */
			for (j=3, len=0; j < fields; j++) {
				len += strlen(safeGet(row[j])) + 1;
			}
			/* allocate memory, allow for NULL to term string */
			tmpString = isc_mem_allocate(ns_g_mctx, len + 1);
			if (tmpString == NULL) {	/* we need more ram. */
				isc_log_write(dns_lctx,
					      DNS_LOGCATEGORY_DATABASE,
					      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
					      "mysql driver unable "
					      "to allocate memory for "
					      "temporary string");
				mysql_free_result(rs);
				return (ISC_R_FAILURE);
			}
			/* copy this field to tmpString */
			strcpy(tmpString, safeGet(row[3]));
			/* concatonate the rest, with spaces between */
			for (j=4; j < fields; j++) {
				strcat(tmpString, " ");
				strcat(tmpString, safeGet(row[j]));
			}
			/* tell Bind about it. */
			result = dns_sdlz_putnamedrr(allnodes, safeGet(row[2]),
						     safeGet(row[1]),
						     ttl, tmpString);
			isc_mem_free(ns_g_mctx, tmpString);
		}
		/* if we weren't successful, log err msg */
		if (result != ISC_R_SUCCESS) {
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
				      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
				      "dns_sdlz_putnamedrr returned error. "
				      "Error code was: %s",
				      isc_result_totext(result));
			result = ISC_R_FAILURE;
			break;
		}
		/* get next row from the result set */
		row = mysql_fetch_row(rs);
	}

	/* free result set memory */
	mysql_free_result(rs);

	return result;
}

/*% if the lookup function does not return SOA or NS records for the zone,
 * use this function to get that information for Bind.
 */

static isc_result_t
mysql_authority(const char *zone, void *driverarg, void *dbdata,
		dns_sdlzlookup_t *lookup)
{
	isc_result_t result;
	MYSQL_RES *rs = NULL;

	UNUSED(driverarg);

	/* run the query and get the result set from the database. */
	result = mysql_get_resultset(zone, NULL, NULL, AUTHORITY, dbdata, &rs);
	/* if we get "not implemented", send it along */
	if (result == ISC_R_NOTIMPLEMENTED)
		return result;
	/* if we didn't get a result set, log an err msg. */
	if (result != ISC_R_SUCCESS) {
		if (rs != NULL)
			mysql_free_result(rs);
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "mysql driver unable to return "
			      "result set for authority query");
		return (ISC_R_FAILURE);
	}
	/*
	 * lookup and authority result sets are processed in the same
	 * manner mysql_process_rs does the job for both functions.
	 */
	return mysql_process_rs(lookup, rs);
}

/*% if zone is supported, lookup up a (or multiple) record(s) in it */
static isc_result_t
mysql_lookup(const char *zone, const char *name, void *driverarg,
	     void *dbdata, dns_sdlzlookup_t *lookup)
{
	isc_result_t result;
	MYSQL_RES *rs = NULL;

	UNUSED(driverarg);

	/* run the query and get the result set from the database. */
	result = mysql_get_resultset(zone, name, NULL, LOOKUP, dbdata, &rs);
	/* if we didn't get a result set, log an err msg. */
	if (result != ISC_R_SUCCESS) {
		if (rs != NULL)
			mysql_free_result(rs);
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "mysql driver unable to return "
			      "result set for lookup query");
		return (ISC_R_FAILURE);
	}
	/*
	 * lookup and authority result sets are processed in the same manner
	 * mysql_process_rs does the job for both functions.
	 */
	return mysql_process_rs(lookup, rs);
}

/*%
 * create an instance of the driver.  Remember, only 1 copy of the driver's
 * code is ever loaded, the driver has to remember which context it's
 * operating in.  This is done via use of the dbdata argument which is
 * passed into all query functions.
 */
static isc_result_t
mysql_create(const char *dlzname, unsigned int argc, char *argv[],
	     void *driverarg, void **dbdata)
{
	isc_result_t result;
	dbinstance_t *dbi = NULL;
	char *tmp = NULL;
	char *dbname = NULL;
	char *host = NULL;
	char *user = NULL;
	char *pass = NULL;
	char *socket = NULL;
	int port;
	MYSQL *dbc;
	char *endp;
	int j;
	unsigned int flags = 0;
#if MYSQL_VERSION_ID >= 50000
        my_bool auto_reconnect = 1;
#endif

	UNUSED(driverarg);
	UNUSED(dlzname);

	/* verify we have at least 4 arg's passed to the driver */
	if (argc < 4) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "mysql driver requires "
			      "at least 4 command line args.");
		return (ISC_R_FAILURE);
	}

	/* no more than 8 arg's should be passed to the driver */
	if (argc > 8) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "mysql driver cannot accept "
			      "more than 7 command line args.");
		return (ISC_R_FAILURE);
	}

	/* parse connection string and get paramters. */

	/* get db name - required */
	dbname = getParameterValue(argv[1], "dbname=");
	if (dbname == NULL) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "mysql driver requires a dbname parameter.");
		result = ISC_R_FAILURE;
		goto full_cleanup;
	}

	/* get db port.  Not required, but must be > 0 if specified */
	tmp = getParameterValue(argv[1], "port=");
	if (tmp == NULL) {
		port = 0;
	} else {
		port = strtol(tmp, &endp, 10);
		if (*endp != '\0' || port < 0) {
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
				      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
				      "Mysql driver port "
				      "must be a positive number.");
			isc_mem_free(ns_g_mctx, tmp);
			result = ISC_R_FAILURE;
			goto full_cleanup;
		}
		isc_mem_free(ns_g_mctx, tmp);
	}

	/* how many queries were passed in from config file? */
	switch(argc) {
	case 4:
		result = build_sqldbinstance(ns_g_mctx, NULL, NULL, NULL,
					     argv[2], argv[3], NULL, &dbi);
		break;
	case 5:
		result = build_sqldbinstance(ns_g_mctx, NULL, NULL, argv[4],
					     argv[2], argv[3], NULL, &dbi);
		break;
	case 6:
		result = build_sqldbinstance(ns_g_mctx, argv[5], NULL, argv[4],
					     argv[2], argv[3], NULL, &dbi);
		break;
	case 7:
		result = build_sqldbinstance(ns_g_mctx, argv[5],
					     argv[6], argv[4],
					     argv[2], argv[3], NULL, &dbi);
		break;
	case 8:
		result = build_sqldbinstance(ns_g_mctx, argv[5],
					     argv[6], argv[4],
					     argv[2], argv[3], argv[7], &dbi);
		break;
	default:
		/* not really needed, should shut up compiler. */
		result = ISC_R_FAILURE;
	}

	/* unsuccessful?, log err msg and cleanup. */
	if (result != ISC_R_SUCCESS) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "mysql driver could not create "
			      "database instance object.");
		result = ISC_R_FAILURE;
		goto cleanup;
	}

	/* create and set db connection */
	dbi->dbconn = mysql_init(NULL);

	/* if db connection cannot be created, log err msg and cleanup. */
	if (dbi->dbconn == NULL) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "mysql driver could not allocate "
			      "memory for database connection");
		result = ISC_R_FAILURE;
		goto full_cleanup;
	}

	tmp = getParameterValue(argv[1], "compress=");
	if (tmp != NULL) {
		if (strcasecmp(tmp, "true") == 0)
			flags = CLIENT_COMPRESS;
		isc_mem_free(ns_g_mctx, tmp);
	}

	tmp = getParameterValue(argv[1], "ssl=");
	if (tmp != NULL) {
		if (strcasecmp(tmp, "true") == 0)
			flags = flags | CLIENT_SSL;
		isc_mem_free(ns_g_mctx, tmp);
	}

	tmp = getParameterValue(argv[1], "space=");
	if (tmp != NULL) {
		if (strcasecmp(tmp, "ignore") == 0)
			flags = flags | CLIENT_IGNORE_SPACE;
		isc_mem_free(ns_g_mctx, tmp);
	}

	dbc = NULL;
	host = getParameterValue(argv[1], "host=");
	user = getParameterValue(argv[1], "user=");
	pass = getParameterValue(argv[1], "pass=");
	socket = getParameterValue(argv[1], "socket=");

#if MYSQL_VERSION_ID >= 50000
	/* enable automatic reconnection. */
        if (mysql_options((MYSQL *) dbi->dbconn, MYSQL_OPT_RECONNECT,
			  &auto_reconnect) != 0) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_WARNING,
			      "mysql driver failed to set "
			      "MYSQL_OPT_RECONNECT option, continuing");
	}
#endif

	for (j=0; dbc == NULL && j < 4; j++)
		dbc = mysql_real_connect((MYSQL *) dbi->dbconn, host,
					 user, pass, dbname, port, socket,
					 flags);

	/* let user know if we couldn't connect. */
	if (dbc == NULL) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "mysql driver failed to create "
			      "database connection after 4 attempts");
		result = ISC_R_FAILURE;
		goto full_cleanup;
	}

	/* return db connection via dbdata */
	*dbdata = dbi;

	result = ISC_R_SUCCESS;
	goto cleanup;

 full_cleanup:

	destroy_sqldbinstance(dbi);

 cleanup:

	if (dbname != NULL)
		isc_mem_free(ns_g_mctx, dbname);
	if (host != NULL)
		isc_mem_free(ns_g_mctx, host);
	if (user != NULL)
		isc_mem_free(ns_g_mctx, user);
	if (pass != NULL)
		isc_mem_free(ns_g_mctx, pass);
	if (socket != NULL)
		isc_mem_free(ns_g_mctx, socket);


	return result;
}

/*%
 * destroy the driver.  Remember, only 1 copy of the driver's
 * code is ever loaded, the driver has to remember which context it's
 * operating in.  This is done via use of the dbdata argument.
 * so we really only need to clean it up since we are not using driverarg.
 */

static void
mysql_destroy(void *driverarg, void *dbdata)
{
	dbinstance_t *dbi;

	UNUSED(driverarg);

	dbi = (dbinstance_t *) dbdata;

	/* release DB connection */
	if (dbi->dbconn != NULL)
		mysql_close((MYSQL *) dbi->dbconn);

	/* destroy DB instance */
	destroy_sqldbinstance(dbi);
}

/* pointers to all our runtime methods. */
/* this is used during driver registration */
/* i.e. in dlz_mysql_init below. */
static dns_sdlzmethods_t dlz_mysql_methods = {
	mysql_create,
	mysql_destroy,
	mysql_findzone,
	mysql_lookup,
	mysql_authority,
	mysql_allnodes,
	mysql_allowzonexfr,
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
dlz_mysql_init(void) {
	isc_result_t result;

	/*
	 * Write debugging message to log
	 */
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_DLZ, ISC_LOG_DEBUG(2),
		      "Registering DLZ mysql driver.");

	/* Driver is always threadsafe.  Because of the way MySQL handles
         * threads the MySQL driver can only be used when bind is run single
         * threaded.  Using MySQL with Bind running multi-threaded is not
         * allowed.  When using the MySQL driver "-n1" should always be
         * passed to Bind to guarantee single threaded operation.
	 */
	result = dns_sdlzregister("mysql", &dlz_mysql_methods, NULL,
				  DNS_SDLZFLAG_RELATIVEOWNER |
				  DNS_SDLZFLAG_RELATIVERDATA |
				  DNS_SDLZFLAG_THREADSAFE,
				  ns_g_mctx, &dlz_mysql);
	/* if we can't register the driver, there are big problems. */
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
dlz_mysql_clear(void) {

	/*
	 * Write debugging message to log
	 */
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_DLZ, ISC_LOG_DEBUG(2),
		      "Unregistering DLZ mysql driver.");

	/* unregister the driver. */
	if (dlz_mysql != NULL)
		dns_sdlzunregister(&dlz_mysql);
}

#endif
