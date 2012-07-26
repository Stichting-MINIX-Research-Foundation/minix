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

#ifdef DLZ_POSTGRES

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
#include <dlz/dlz_postgres_driver.h>

/* temporarily include time. */
#include <time.h>

#include <libpq-fe.h>

static dns_sdlzimplementation_t *dlz_postgres = NULL;

#define dbc_search_limit 30
#define ALLNODES 1
#define ALLOWXFR 2
#define AUTHORITY 3
#define FINDZONE 4
#define LOOKUP 5

/*
 * Private methods
 */

/* ---------------
 * Escaping arbitrary strings to get valid SQL strings/identifiers.
 *
 * Replaces "\\" with "\\\\" and "'" with "''".
 * length is the length of the buffer pointed to by
 * from.  The buffer at to must be at least 2*length + 1 characters
 * long.  A terminating NUL character is written.
 *
 * NOTICE!!!
 * This function was borrowed directly from PostgreSQL's libpq.
 * The function was originally called PQescapeString and renamed
 * to postgres_makesafe to avoid a naming collision.
 * PQescapeString is a new function made available in Postgres 7.2.
 * For some reason the function is not properly exported on Win32
 * builds making the function unavailable on Windows.  Also, since
 * this function is new it would require building this driver with
 * the libpq 7.2.  By borrowing this function the Windows problem
 * is solved, and the dependence on libpq 7.2 is removed.  Libpq is
 * still required of course, but an older version should work now too.
 *
 * The copyright statements from the original file containing this
 * function are included below:
 * Portions Copyright (c) 1996-2001, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 * ---------------
 */

static size_t
postgres_makesafe(char *to, const char *from, size_t length)
{
	const char *source = from;
	char	   *target = to;
	unsigned int remaining = length;

	while (remaining > 0)
	{
		switch (*source)
		{
		case '\\':
			*target = '\\';
			target++;
			*target = '\\';
			/* target and remaining are updated below. */
			break;

		case '\'':
			*target = '\'';
			target++;
			*target = '\'';
			/* target and remaining are updated below. */
			break;

		default:
			*target = *source;
			/* target and remaining are updated below. */
		}
		source++;
		target++;
		remaining--;
	}

	/* Write the terminating NUL character. */
	*target = '\0';

	return target - to;
}

#ifdef ISC_PLATFORM_USETHREADS

/*%
 * Properly cleans up a list of database instances.
 * This function is only used when the driver is compiled for
 * multithreaded operation.
 */
static void
postgres_destroy_dblist(db_list_t *dblist)
{

	dbinstance_t *ndbi = NULL;
	dbinstance_t *dbi = NULL;

	/* get the first DBI in the list */
	ndbi = ISC_LIST_HEAD(*dblist);

	/* loop through the list */
	while (ndbi != NULL) {
		dbi = ndbi;
		/* get the next DBI in the list */
		ndbi = ISC_LIST_NEXT(dbi, link);
		/* release DB connection */
		if (dbi->dbconn != NULL)
			PQfinish((PGconn *) dbi->dbconn);
		/* release all memory that comprised a DBI */
		destroy_sqldbinstance(dbi);
	}
	/* release memory for the list structure */
	isc_mem_put(ns_g_mctx, dblist, sizeof(db_list_t));
}

/*%
 * Loops through the list of DB instances, attempting to lock
 * on the mutex.  If successful, the DBI is reserved for use
 * and the thread can perform queries against the database.
 * If the lock fails, the next one in the list is tried.
 * looping continues until a lock is obtained, or until
 * the list has been searched dbc_search_limit times.
 * This function is only used when the driver is compiled for
 * multithreaded operation.
 */

static dbinstance_t *
postgres_find_avail_conn(db_list_t *dblist)
{
	dbinstance_t *dbi = NULL;
	dbinstance_t *head;
	int count = 0;

	/* get top of list */
	head = dbi = ISC_LIST_HEAD(*dblist);

	/* loop through list */
	while (count < dbc_search_limit) {
		/* try to lock on the mutex */
		if (isc_mutex_trylock(&dbi->instance_lock) == ISC_R_SUCCESS)
			return dbi; /* success, return the DBI for use. */

		/* not successful, keep trying */
		dbi = ISC_LIST_NEXT(dbi, link);

		/* check to see if we have gone to the top of the list. */
		if (dbi == NULL) {
			count++;
			dbi = head;
		}
	}
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_DLZ, ISC_LOG_INFO,
		      "Postgres driver unable to find available connection "
		      "after searching %d times",
		      count);
	return NULL;
}

#endif /* ISC_PLATFORM_USETHREADS */

/*%
 * Allocates memory for a new string, and then constructs the new
 * string by "escaping" the input string.  The new string is
 * safe to be used in queries.  This is necessary because we cannot
 * be sure of what types of strings are passed to us, and we don't
 * want special characters in the string causing problems.
 */

static char *
postgres_escape_string(const char *instr) {

	char *outstr;
	unsigned int len;

	if (instr == NULL)
		return NULL;

	len = strlen(instr);

	outstr = isc_mem_allocate(ns_g_mctx ,(2 * len * sizeof(char)) + 1);
	if (outstr == NULL)
		return NULL;

	postgres_makesafe(outstr, instr, len);
	/* PQescapeString(outstr, instr, len); */

	return outstr;
}

/*%
 * This function is the real core of the driver.   Zone, record
 * and client strings are passed in (or NULL is passed if the
 * string is not available).  The type of query we want to run
 * is indicated by the query flag, and the dbdata object is passed
 * passed in to.  dbdata really holds either:
 *		1) a list of database instances (in multithreaded mode) OR
 *		2) a single database instance (in single threaded mode)
 * The function will construct the query and obtain an available
 * database instance (DBI).  It will then run the query and hopefully
 * obtain a result set.  Postgres is nice, in that once the result
 * set is returned, we can make the db connection available for another
 * thread to use, while this thread continues on.  So, the DBI is made
 * available ASAP by unlocking the instance_lock after we have cleaned
 * it up properly.
 */
static isc_result_t
postgres_get_resultset(const char *zone, const char *record,
		       const char *client, unsigned int query,
		       void *dbdata, PGresult **rs)
{
	isc_result_t result;
	dbinstance_t *dbi = NULL;
	char *querystring = NULL;
	unsigned int i = 0;
	unsigned int j = 0;

	/* temporarily get a unique thread # */
	unsigned int dlz_thread_num = 1+(int) (1000.0*rand()/(RAND_MAX+1.0));

	REQUIRE(*rs == NULL);

#if 0
	/* temporary logging message */
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
		      "%d Getting DBI", dlz_thread_num);
#endif

	/* get db instance / connection */
#ifdef ISC_PLATFORM_USETHREADS

	/* find an available DBI from the list */
	dbi = postgres_find_avail_conn((db_list_t *) dbdata);

#else /* ISC_PLATFORM_USETHREADS */

	/*
	 * only 1 DBI - no need to lock instance lock either
	 * only 1 thread in the whole process, no possible contention.
	 */
	dbi =  (dbinstance_t *) dbdata;

#endif /* ISC_PLATFORM_USETHREADS */

#if 0
	/* temporary logging message */
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
		      "%d Got DBI - checking query", dlz_thread_num);
#endif

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
				 "postgres_get_resultset");
		result = ISC_R_UNEXPECTED;
		goto cleanup;
	}

#if 0
	/* temporary logging message */
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
		      "%d checked query", dlz_thread_num);
#endif

	/*
	 * was a zone string passed?  If so, make it safe for use in
	 * queries.
	 */
	if (zone != NULL) {
		dbi->zone = postgres_escape_string(zone);
		if (dbi->zone == NULL) {
			result = ISC_R_NOMEMORY;
			goto cleanup;
		}
	} else {	/* no string passed, set the string pointer to NULL */
		dbi->zone = NULL;
	}

#if 0
	/* temporary logging message */
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
		      "%d did zone", dlz_thread_num);
#endif

	/*
	 * was a record string passed?  If so, make it safe for use in
	 * queries.
	 */
	if (record != NULL) {
		dbi->record = postgres_escape_string(record);
		if (dbi->record == NULL) {
			result = ISC_R_NOMEMORY;
			goto cleanup;
		}
	} else {	/* no string passed, set the string pointer to NULL */
		dbi->record = NULL;
	}


#if 0
	/* temporary logging message */
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
		      "%d did record", dlz_thread_num);
#endif

	/*
	 * was a client string passed?  If so, make it safe for use in
	 * queries.
	 */
	if (client != NULL) {
		dbi->client = postgres_escape_string(client);
		if (dbi->client == NULL) {
			result = ISC_R_NOMEMORY;
			goto cleanup;
		}
	} else {	/* no string passed, set the string pointer to NULL */
		dbi->client = NULL;
	}

#if 0
	/* temporary logging message */
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
	DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
		      "%d did client", dlz_thread_num);
#endif

	/*
	 * what type of query are we going to run?
	 * this time we build the actual query to run.
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
				 "postgres_get_resultset");
		result = ISC_R_UNEXPECTED;
		goto cleanup;
	}

#if 0
	/* temporary logging message */
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
		      "%d built query", dlz_thread_num);
#endif

	/* if the querystring is null, Bummer, outta RAM.  UPGRADE TIME!!!   */
	if (querystring  == NULL) {
		result = ISC_R_NOMEMORY;
		goto cleanup;
	}

#if 0
	/* temporary logging message */
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
		      "%d query is '%s'", dlz_thread_num, querystring);
#endif

	/*
	 * output the full query string during debug so we can see
	 * what lame error the query has.
	 */
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_DLZ, ISC_LOG_DEBUG(1),
		      "\nQuery String: %s\n", querystring);

	/* attempt query up to 3 times. */
	for (j=0; j < 3; j++) {
#if 0
		/* temporary logging message */
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "%d executing query for %d time",
			      dlz_thread_num, j);
#endif
		/* try to get result set */
		*rs = PQexec((PGconn *)dbi->dbconn, querystring );
		result = ISC_R_SUCCESS;
		/*
		 * if result set is null, reset DB connection, max 3
		 * attempts.
		 */
		for (i=0; *rs == NULL && i < 3; i++) {
#if 0
			/* temporary logging message */
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
				      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
				      "%d resetting connection",
				      dlz_thread_num);
#endif
			result = ISC_R_FAILURE;
			PQreset((PGconn *) dbi->dbconn);
			/* connection ok, break inner loop */
			if (PQstatus((PGconn *) dbi->dbconn) == CONNECTION_OK)
				break;
		}
		/* result set ok, break outter loop */
		if (PQresultStatus(*rs) == PGRES_TUPLES_OK) {
#if 0
			/* temporary logging message */
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
				      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
				      "%d rs ok", dlz_thread_num);
#endif
			break;
		} else {
			/* we got a result set object, but it's not right. */
#if 0
			/* temporary logging message */
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
				      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
				      "%d clearing rs", dlz_thread_num);
#endif
			PQclear(*rs);	/* get rid of it */
			/* in case this was the last attempt */
			result = ISC_R_FAILURE;
		}
	}

 cleanup:
	/* it's always good to cleanup after yourself */

#if 0
	/* temporary logging message */
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
		      "%d cleaning up", dlz_thread_num);
#endif

	/* if we couldn't even allocate DBI, just return NULL */
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

#ifdef ISC_PLATFORM_USETHREADS

#if 0
	/* temporary logging message */
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
		      "%d unlocking mutex", dlz_thread_num);
#endif

	/* release the lock so another thread can use this dbi */
	isc_mutex_unlock(&dbi->instance_lock);

#endif /* ISC_PLATFORM_USETHREADS */

	/* release query string */
	if (querystring  != NULL)
		isc_mem_free(ns_g_mctx, querystring );

#if 0
	/* temporary logging message */
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
		      "%d returning", dlz_thread_num);
#endif

	/* return result */
	return result;
}

/*%
 * The processing of result sets for lookup and authority are
 * exactly the same.  So that functionality has been moved
 * into this function to minimize code.
 */

static isc_result_t
postgres_process_rs(dns_sdlzlookup_t *lookup, PGresult *rs)
{
	isc_result_t result;
	unsigned int i;
	unsigned int rows;
	unsigned int fields;
	unsigned int j;
	unsigned int len;
	char *tmpString;
	char *endp;
	int ttl;

	rows = PQntuples(rs);	/* how many rows in result set */
	fields = PQnfields(rs);	/* how many columns in result set */
	for (i=0; i < rows; i++) {
		switch(fields) {
		case 1:
			/*
			 * one column in rs, it's the data field.  use
			 * default type of A record, and default TTL
			 * of 86400
			 */
			result = dns_sdlz_putrr(lookup, "a", 86400,
						PQgetvalue(rs, i, 0));
			break;
		case 2:
			/* two columns, data field, and data type.
			 * use default TTL of 86400.
			 */
			result = dns_sdlz_putrr(lookup, PQgetvalue(rs, i, 0),
						86400, PQgetvalue(rs, i, 1));
			break;
		case 3:
			/* three columns, all data no defaults.
			 * convert text to int, make sure it worked
			 * right.
			 */
			ttl = strtol(PQgetvalue(rs, i, 0), &endp, 10);
			if (*endp != '\0' || ttl < 0) {
				isc_log_write(dns_lctx,
					      DNS_LOGCATEGORY_DATABASE,
					      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
					      "Postgres driver ttl must be "
					      "a positive number");
			}
			result = dns_sdlz_putrr(lookup, PQgetvalue(rs, i, 1),
						ttl, PQgetvalue(rs, i, 2));
			break;
		default:
		  	/*
			 * more than 3 fields, concatenate the last
			 * ones together.  figure out how long to make
			 * string
			 */
			for (j=2, len=0; j < fields; j++) {
				len += strlen(PQgetvalue(rs, i, j)) + 1;
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
					      "Postgres driver unable to "
					      "allocate memory for "
					      "temporary string");
				PQclear(rs);
				return (ISC_R_FAILURE);	/* Yeah, I'd say! */
			}
			/* copy field to tmpString */
			strcpy(tmpString, PQgetvalue(rs, i, 2));
			/*
			 * concat the rest of fields together, space
			 * between each one.
			 */
			for (j=3; j < fields; j++) {
				strcat(tmpString, " ");
				strcat(tmpString, PQgetvalue(rs, i, j));
			}
			/* convert text to int, make sure it worked right */
			ttl = strtol(PQgetvalue(rs, i, 0), &endp, 10);
			if (*endp != '\0' || ttl < 0) {
				isc_log_write(dns_lctx,
					      DNS_LOGCATEGORY_DATABASE,
					      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
					      "Postgres driver ttl must be "
					      "a postive number");
			}
			/* ok, now tell Bind about it. */
			result = dns_sdlz_putrr(lookup, PQgetvalue(rs, i, 1),
						ttl, tmpString);
			/* done, get rid of this thing. */
			isc_mem_free(ns_g_mctx, tmpString);
		}
		/* I sure hope we were successful */
		if (result != ISC_R_SUCCESS) {
			/* nope, get rid of the Result set, and log a msg */
			PQclear(rs);
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
				      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
				      "dns_sdlz_putrr returned error. "
				      "Error code was: %s",
				      isc_result_totext(result));
			return (ISC_R_FAILURE);
		}
	}

	/* free result set memory */
	PQclear(rs);

	/* if we did return results, we are successful */
	if (rows > 0)
		return (ISC_R_SUCCESS);

	/* empty result set, no data found */
	return (ISC_R_NOTFOUND);
}

/*
 * SDLZ interface methods
 */

/*% determine if the zone is supported by (in) the database */

static isc_result_t
postgres_findzone(void *driverarg, void *dbdata, const char *name)
{
	isc_result_t result;
	PGresult *rs = NULL;
	unsigned int rows;
	UNUSED(driverarg);

	/* run the query and get the result set from the database. */
	result = postgres_get_resultset(name, NULL, NULL,
					FINDZONE, dbdata, &rs);
	/* if we didn't get a result set, log an err msg. */
	if (result != ISC_R_SUCCESS) {
		if (rs != NULL)
			PQclear(rs);
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "Postgres driver unable to return "
			      "result set for findzone query");
		return (ISC_R_FAILURE);
	}
	/* count how many rows in result set */
	rows = PQntuples(rs);
	/* get rid of result set, we are done with it. */
	PQclear(rs);

	/* if we returned any rows, zone is supported. */
	if (rows > 0)
		return (ISC_R_SUCCESS);

	/* no rows returned, zone is not supported. */
	return (ISC_R_NOTFOUND);
}

/*% Determine if the client is allowed to perform a zone transfer */
static isc_result_t
postgres_allowzonexfr(void *driverarg, void *dbdata, const char *name,
		      const char *client)
{
	isc_result_t result;
	PGresult *rs = NULL;
	unsigned int rows;
	UNUSED(driverarg);

	/* first check if the zone is supported by the database. */
	result = postgres_findzone(driverarg, dbdata, name);
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
	result = postgres_get_resultset(name, NULL, client,
					ALLOWXFR, dbdata, &rs);
	/* if we get "not implemented", send it along. */
	if (result == ISC_R_NOTIMPLEMENTED)
		return result;
	/* if we didn't get a result set, log an err msg. */
	if (result != ISC_R_SUCCESS) {
		if (rs != NULL)
			PQclear(rs);
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "Postgres driver unable to return "
			      "result set for allow xfr query");
		return (ISC_R_FAILURE);
	}
	/* count how many rows in result set */
	rows = PQntuples(rs);
	/* get rid of result set, we are done with it. */
	PQclear(rs);

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
postgres_allnodes(const char *zone, void *driverarg, void *dbdata,
		  dns_sdlzallnodes_t *allnodes)
{
	isc_result_t result;
	PGresult *rs = NULL;
	unsigned int i;
	unsigned int rows;
	unsigned int fields;
	unsigned int j;
	unsigned int len;
	char *tmpString;
	char *endp;
	int ttl;

	UNUSED(driverarg);

	/* run the query and get the result set from the database. */
	result = postgres_get_resultset(zone, NULL, NULL,
					ALLNODES, dbdata, &rs);
	/* if we get "not implemented", send it along */
	if (result == ISC_R_NOTIMPLEMENTED)
		return result;
	/* if we didn't get a result set, log an err msg. */
	if (result != ISC_R_SUCCESS) {
		if (rs != NULL)
			PQclear(rs);
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "Postgres driver unable to return "
			      "result set for all nodes query");
		return (ISC_R_FAILURE);
	}

	rows = PQntuples(rs);	/* how many rows in result set */
	fields = PQnfields(rs);	/* how many columns in result set */
	for (i=0; i < rows; i++) {
		if (fields < 4) {	/* gotta have at least 4 columns */
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
				      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
				      "Postgres driver too few fields "
				      "returned by all nodes query");
		}
		/* convert text to int, make sure it worked right  */
		ttl = strtol(PQgetvalue(rs, i, 0), &endp, 10);
		if (*endp != '\0' || ttl < 0) {
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
				      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
				      "Postgres driver ttl must be "
				      "a postive number");
		}
		if (fields == 4) {
			/* tell Bind about it. */
			result = dns_sdlz_putnamedrr(allnodes,
						     PQgetvalue(rs, i, 2),
						     PQgetvalue(rs, i, 1),
						     ttl,
						     PQgetvalue(rs, i, 3));
		} else {
			/*
			 * more than 4 fields, concatonat the last
			 * ones together.  figure out how long to make
			 * string
			 */
			for (j=3, len=0; j < fields; j++) {
				len += strlen(PQgetvalue(rs, i, j)) + 1;
			}
			/* allocate memory, allow for NULL to term string */
			tmpString = isc_mem_allocate(ns_g_mctx, len + 1);
			if (tmpString == NULL) {	/* we need more ram. */
				isc_log_write(dns_lctx,
					      DNS_LOGCATEGORY_DATABASE,
					      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
					      "Postgres driver unable to "
					      "allocate memory for "
					      "temporary string");
				PQclear(rs);
				return (ISC_R_FAILURE);
			}
			/* copy this field to tmpString */
			strcpy(tmpString, PQgetvalue(rs, i, 3));
			/* concatonate the rest, with spaces between */
			for (j=4; j < fields; j++) {
				strcat(tmpString, " ");
				strcat(tmpString, PQgetvalue(rs, i, j));
			}
			/* tell Bind about it. */
			result = dns_sdlz_putnamedrr(allnodes,
						     PQgetvalue(rs, i, 2),
						     PQgetvalue(rs, i, 1),
						     ttl, tmpString);
			isc_mem_free(ns_g_mctx, tmpString);
		}
		/* if we weren't successful, log err msg */
		if (result != ISC_R_SUCCESS) {
			PQclear(rs);
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
				      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
				      "dns_sdlz_putnamedrr returned error. "
				      "Error code was: %s",
				      isc_result_totext(result));
			return (ISC_R_FAILURE);
		}
	}

	/* free result set memory */
	PQclear(rs);

	/* if we did return results, we are successful */
	if (rows > 0)
		return (ISC_R_SUCCESS);

	/* empty result set, no data found */
	return (ISC_R_NOTFOUND);
}

/*%
 * if the lookup function does not return SOA or NS records for the zone,
 * use this function to get that information for Bind.
 */

static isc_result_t
postgres_authority(const char *zone, void *driverarg, void *dbdata,
		   dns_sdlzlookup_t *lookup)
{
	isc_result_t result;
	PGresult *rs = NULL;

	UNUSED(driverarg);

	/* run the query and get the result set from the database. */
	result = postgres_get_resultset(zone, NULL, NULL,
					AUTHORITY, dbdata, &rs);
	/* if we get "not implemented", send it along */
	if (result == ISC_R_NOTIMPLEMENTED)
		return result;
	/* if we didn't get a result set, log an err msg. */
	if (result != ISC_R_SUCCESS) {
		if (rs != NULL)
			PQclear(rs);
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "Postgres driver unable to return "
			      "result set for authority query");
		return (ISC_R_FAILURE);
	}
	/*
	 * lookup and authority result sets are processed in the same
	 * manner postgres_process_rs does the job for both
	 * functions.
	 */
	return postgres_process_rs(lookup, rs);
}

/*% if zone is supported, lookup up a (or multiple) record(s) in it */
static isc_result_t
postgres_lookup(const char *zone, const char *name, void *driverarg,
		void *dbdata, dns_sdlzlookup_t *lookup)
{
	isc_result_t result;
	PGresult *rs = NULL;

	UNUSED(driverarg);

	/* run the query and get the result set from the database. */
	result = postgres_get_resultset(zone, name, NULL, LOOKUP, dbdata, &rs);
	/* if we didn't get a result set, log an err msg. */
	if (result != ISC_R_SUCCESS) {
		if (rs != NULL)
			PQclear(rs);
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "Postgres driver unable to "
			      "return result set for lookup query");
		return (ISC_R_FAILURE);
	}
	/*
	 * lookup and authority result sets are processed in the same
	 * manner postgres_process_rs does the job for both functions.
	 */
	return postgres_process_rs(lookup, rs);
}

/*%
 * create an instance of the driver.  Remember, only 1 copy of the driver's
 * code is ever loaded, the driver has to remember which context it's
 * operating in.  This is done via use of the dbdata argument which is
 * passed into all query functions.
 */
static isc_result_t
postgres_create(const char *dlzname, unsigned int argc, char *argv[],
		void *driverarg, void **dbdata)
{
	isc_result_t result;
	dbinstance_t *dbi = NULL;
	unsigned int j;

#ifdef ISC_PLATFORM_USETHREADS
	/* if multi-threaded, we need a few extra variables. */
	int dbcount;
	db_list_t *dblist = NULL;
	int i;
	char *endp;

#endif /* ISC_PLATFORM_USETHREADS */

	UNUSED(driverarg);
	UNUSED(dlzname);

/* seed random # generator */
	srand( (unsigned)time( NULL ) );


#ifdef ISC_PLATFORM_USETHREADS
	/* if debugging, let user know we are multithreaded. */
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_DLZ, ISC_LOG_DEBUG(1),
		      "Postgres driver running multithreaded");
#else /* ISC_PLATFORM_USETHREADS */
	/* if debugging, let user know we are single threaded. */
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_DLZ, ISC_LOG_DEBUG(1),
		      "Postgres driver running single threaded");
#endif /* ISC_PLATFORM_USETHREADS */

	/* verify we have at least 5 arg's passed to the driver */
	if (argc < 5) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "Postgres driver requires at least "
			      "4 command line args.");
		return (ISC_R_FAILURE);
	}

	/* no more than 8 arg's should be passed to the driver */
	if (argc > 8) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "Postgres driver cannot accept more than "
			      "7 command line args.");
		return (ISC_R_FAILURE);
	}

	/* multithreaded build can have multiple DB connections */
#ifdef ISC_PLATFORM_USETHREADS

	/* check how many db connections we should create */
	dbcount = strtol(argv[1], &endp, 10);
	if (*endp != '\0' || dbcount < 0) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "Postgres driver database connection count "
			      "must be positive.");
		return (ISC_R_FAILURE);
	}

	/* allocate memory for database connection list */
	dblist = isc_mem_get(ns_g_mctx, sizeof(db_list_t));
	if (dblist == NULL)
		return (ISC_R_NOMEMORY);

	/* initialize DB connection list */
	ISC_LIST_INIT(*dblist);

	/*
	 * create the appropriate number of database instances (DBI)
	 * append each new DBI to the end of the list
	 */
	for (i=0; i < dbcount; i++) {

#endif /* ISC_PLATFORM_USETHREADS */

		/* how many queries were passed in from config file? */
		switch(argc) {
		case 5:
			result = build_sqldbinstance(ns_g_mctx, NULL, NULL,
						     NULL, argv[3], argv[4],
						     NULL, &dbi);
			break;
		case 6:
			result = build_sqldbinstance(ns_g_mctx, NULL, NULL,
						     argv[5], argv[3], argv[4],
						     NULL, &dbi);
			break;
		case 7:
			result = build_sqldbinstance(ns_g_mctx, argv[6], NULL,
						     argv[5], argv[3], argv[4],
						     NULL, &dbi);
			break;
		case 8:
			result = build_sqldbinstance(ns_g_mctx, argv[6],
						     argv[7], argv[5], argv[3],
						     argv[4], NULL, &dbi);
			break;
		default:
			/* not really needed, should shut up compiler. */
			result = ISC_R_FAILURE;
		}


		if (result == ISC_R_SUCCESS) {
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
				      DNS_LOGMODULE_DLZ, ISC_LOG_DEBUG(2),
				      "Postgres driver created database "
				      "instance object.");
		} else { /* unsuccessful?, log err msg and cleanup. */
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
				      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
				      "Postgres driver could not create "
				      "database instance object.");
			goto cleanup;
		}

#ifdef ISC_PLATFORM_USETHREADS

		/* when multithreaded, build a list of DBI's */
		ISC_LINK_INIT(dbi, link);
		ISC_LIST_APPEND(*dblist, dbi, link);

#endif

		/* create and set db connection */
		dbi->dbconn = PQconnectdb(argv[2]);
		/*
		 * if db connection cannot be created, log err msg and
		 * cleanup.
		 */
		if (dbi->dbconn == NULL) {
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
				      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
				      "Postgres driver could not allocate "
				      "memory for database connection");
			goto cleanup;
		}

		/* if we cannot connect the first time, try 3 more times. */
		for (j = 0;
		     PQstatus((PGconn *) dbi->dbconn) != CONNECTION_OK &&
			     j < 3;
		     j++)
			PQreset((PGconn *) dbi->dbconn);


#ifdef ISC_PLATFORM_USETHREADS

		/*
		 * if multi threaded, let user know which connection
		 * failed.  user could be attempting to create 10 db
		 * connections and for some reason the db backend only
		 * allows 9
		 */
		if (PQstatus((PGconn *) dbi->dbconn) != CONNECTION_OK) {
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
				      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
				      "Postgres driver failed to create "
				      "database connection number %u "
				      "after 4 attempts",
				      i + 1);
			goto cleanup;
		}

		/* set DBI = null for next loop through. */
		dbi = NULL;
	}	/* end for loop */

		/* set dbdata to the list we created. */
	*dbdata = dblist;

#else /* ISC_PLATFORM_USETHREADS */
	/* if single threaded, just let user know we couldn't connect. */
	if (PQstatus((PGconn *) dbi->dbconn) != CONNECTION_OK) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_DLZ, ISC_LOG_ERROR,
			      "Postgres driver failed to create database "
			      "connection after 4 attempts");
		goto cleanup;
	}

	/*
	 * single threaded build can only use 1 db connection, return
	 * it via dbdata
	 */
	*dbdata = dbi;

#endif /* ISC_PLATFORM_USETHREADS */

	/* hey, we got through all of that ok, return success. */
	return(ISC_R_SUCCESS);

 cleanup:

#ifdef ISC_PLATFORM_USETHREADS
	/*
	 * if multithreaded, we could fail because only 1 connection
	 * couldn't be made.  We should cleanup the other successful
	 * connections properly.
	 */
	postgres_destroy_dblist(dblist);

#else /* ISC_PLATFORM_USETHREADS */
	if (dbi != NULL)
		destroy_sqldbinstance(dbi);

#endif /* ISC_PLATFORM_USETHREADS */
	return(ISC_R_FAILURE);
}

/*%
 * destroy an instance of the driver.  Remember, only 1 copy of the driver's
 * code is ever loaded, the driver has to remember which context it's
 * operating in.  This is done via use of the dbdata argument.
 * so we really only need to clean it up since we are not using driverarg.
 */
static void
postgres_destroy(void *driverarg, void *dbdata)
{

#ifdef ISC_PLATFORM_USETHREADS

	UNUSED(driverarg);
	/* cleanup the list of DBI's */
	postgres_destroy_dblist((db_list_t *) dbdata);

#else /* ISC_PLATFORM_USETHREADS */

	dbinstance_t *dbi;

	UNUSED(driverarg);

	dbi = (dbinstance_t *) dbdata;

	/* release DB connection */
	if (dbi->dbconn != NULL)
		PQfinish((PGconn *) dbi->dbconn);

	/* destroy single DB instance */
	destroy_sqldbinstance(dbi);

#endif /* ISC_PLATFORM_USETHREADS */
}

/* pointers to all our runtime methods. */
/* this is used during driver registration */
/* i.e. in dlz_postgres_init below. */
static dns_sdlzmethods_t dlz_postgres_methods = {
	postgres_create,
	postgres_destroy,
	postgres_findzone,
	postgres_lookup,
	postgres_authority,
	postgres_allnodes,
	postgres_allowzonexfr,
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
dlz_postgres_init(void) {
	isc_result_t result;

	/*
	 * Write debugging message to log
	 */
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_DLZ, ISC_LOG_DEBUG(2),
		      "Registering DLZ postgres driver.");

	/*
	 * Driver is always threadsafe.  When multithreaded all
	 * functions use multithreaded code.  When not multithreaded,
	 * all functions can only be entered once, but only 1 thread
	 * of operation is available in Bind.  So everything is still
	 * threadsafe.
	 */
	result = dns_sdlzregister("postgres", &dlz_postgres_methods, NULL,
				  DNS_SDLZFLAG_RELATIVEOWNER |
				  DNS_SDLZFLAG_RELATIVERDATA |
				  DNS_SDLZFLAG_THREADSAFE,
				  ns_g_mctx, &dlz_postgres);
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
dlz_postgres_clear(void) {

	/*
	 * Write debugging message to log
	 */
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_DLZ, ISC_LOG_DEBUG(2),
		      "Unregistering DLZ postgres driver.");

	/* unregister the driver. */
	if (dlz_postgres != NULL)
		dns_sdlzunregister(&dlz_postgres);
}

#endif
